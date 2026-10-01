# Typed Redis Client

ioredis gives you `string | null` for almost everything. That is honest, but it pushes every cast and parse to the call site. This lesson builds a small layer so that **the key decides the value type**, TypeScript checks it at compile time, and a schema checks it at runtime.

## Two kinds of safety

| Kind | Protects against | Mechanism |
|------|------------------|-----------|
| **Compile time** | Writing a `Product` where a `User` belongs, wrong key arguments | Generics on key definitions |
| **Runtime** | Old data shapes after a deploy, manual edits, bugs, partial writes | A schema or decoder on every read |

Compile-time types alone are a promise. Redis data outlives your deployments, so **validate what you read**.

## What ioredis already types

```ts
const v: string | null = await redis.get("k");
const n: number = await redis.incr("k");
const h: Record<string, string> = await redis.hgetall("k");
const r = await redis.call("JSON.GET", "doc");          // unknown: narrow it yourself
```

Turn on strict checks in `tsconfig.json`:

```json
{ "compilerOptions": { "strict": true, "noUncheckedIndexedAccess": true } }
```

`noUncheckedIndexedAccess` makes `results[0]` typed as possibly `undefined`, which is exactly how pipeline and `mget` results should be treated.

## Step 1: codecs

A codec turns a value into a string and back. Keep it tiny:

```ts
// src/redis/codecs.ts
import { z } from "zod";

export interface Codec<T> {
  encode(value: T): string;
  decode(raw: string): T;          // throw if the data is invalid
}

export const jsonCodec = <T>(): Codec<T> => ({
  encode: (v) => JSON.stringify(v),
  decode: (raw) => JSON.parse(raw) as T,         // unchecked: prefer a schema codec
});

export function schemaCodec<S extends z.ZodTypeAny>(schema: S): Codec<z.infer<S>> {
  return {
    encode: (v) => JSON.stringify(v),
    decode: (raw) => schema.parse(JSON.parse(raw)),   // throws ZodError on a bad shape
  };
}

export const numberCodec: Codec<number> = {
  encode: String,
  decode: (raw) => {
    const n = Number(raw);
    if (!Number.isFinite(n)) throw new Error(`not a number: ${raw}`);
    return n;
  },
};
```

Zod is one option. Valibot, ArkType or a hand-written function all fit the same `Codec<T>` interface.

Dates survive JSON as strings. With Zod, `z.coerce.date()` revives them:

```ts
export const ProductSchema = z.object({
  id: z.number().int(),
  name: z.string(),
  priceCents: z.number().int().nonnegative(),
  updatedAt: z.coerce.date(),
});
export type Product = z.infer<typeof ProductSchema>;
```

## Step 2: key definitions that carry their value type

```ts
// src/redis/typed-keys.ts
import type { Codec } from "./codecs.js";

export interface StringKey<T, A extends unknown[]> {
  readonly kind: "string";
  key: (...args: A) => string;
  codec: Codec<T>;
  ttlSec?: number;
}

export function stringKey<T, A extends unknown[]>(def: {
  key: (...args: A) => string;
  codec: Codec<T>;
  ttlSec?: number;
}): StringKey<T, A> {
  return { kind: "string", ...def };
}

export interface HashKey<T, A extends unknown[]> {
  readonly kind: "hash";
  key: (...args: A) => string;
  toHash: (value: T) => Record<string, string>;
  fromHash: (h: Record<string, string>) => T | null;   // null = treat as missing or invalid
  ttlSec?: number;
}

export function hashKey<T, A extends unknown[]>(def: Omit<HashKey<T, A>, "kind">): HashKey<T, A> {
  return { kind: "hash", ...def };
}
```

Declare your keys once:

```ts
// src/redis/registry.ts
import { keys } from "./keys.js";
import { schemaCodec, numberCodec } from "./codecs.js";
import { stringKey, hashKey } from "./typed-keys.js";
import { ProductSchema } from "../domain/product.js";

export const defs = {
  productCache: stringKey({
    key: (id: number) => keys.productCache(id),
    codec: schemaCodec(ProductSchema),
    ttlSec: 300,
  }),

  pageViews: stringKey({
    key: (postId: number) => keys.pageViews(postId),
    codec: numberCodec,
  }),

  sessionData: hashKey({
    key: (sid: string) => keys.session(sid),
    toHash: (s: { userId: string; role: string }) => ({ userId: s.userId, role: s.role }),
    fromHash: (h) => (h.userId ? { userId: h.userId, role: h.role ?? "user" } : null),
    ttlSec: 1800,
  }),
};
```

`T` is inferred from the codec (or the mapper), and `A` (the argument list) from the key function.

## Step 3: the store

```ts
// src/redis/store.ts
import type { Redis } from "ioredis";
import type { StringKey, HashKey } from "./typed-keys.js";

export class TypedStore {
  constructor(
    private redis: Redis,
    private onCorrupt: (key: string, err: unknown) => void = () => {}
  ) {}

  // ---- strings ----
  async get<T, A extends unknown[]>(def: StringKey<T, A>, ...args: A): Promise<T | null> {
    const key = def.key(...args);
    const raw = await this.redis.get(key);
    if (raw === null) return null;
    try {
      return def.codec.decode(raw);
    } catch (err) {
      this.onCorrupt(key, err);                       // metric + log
      await this.redis.unlink(key).catch(() => {});   // drop the bad entry, treat as a miss
      return null;
    }
  }

  async set<T, A extends unknown[]>(def: StringKey<T, A>, value: T, ...args: A): Promise<void> {
    const key = def.key(...args);
    const raw = def.codec.encode(value);
    if (def.ttlSec) await this.redis.set(key, raw, "EX", def.ttlSec);
    else await this.redis.set(key, raw);
  }

  async del<T, A extends unknown[]>(def: StringKey<T, A> | HashKey<T, A>, ...args: A): Promise<void> {
    await this.redis.unlink(def.key(...args));
  }

  async getMany<T>(def: StringKey<T, [number]>, ids: number[]): Promise<(T | null)[]> {
    if (ids.length === 0) return [];
    const raws = await this.redis.mget(ids.map((id) => def.key(id)));
    return raws.map((raw, i) => {
      if (raw === null) return null;
      try { return def.codec.decode(raw); }
      catch (err) { this.onCorrupt(def.key(ids[i]!), err); return null; }
    });
  }

  // ---- hashes ----
  async getHash<T, A extends unknown[]>(def: HashKey<T, A>, ...args: A): Promise<T | null> {
    const h = await this.redis.hgetall(def.key(...args));
    if (Object.keys(h).length === 0) return null;     // hgetall returns {} for a missing key
    return def.fromHash(h);
  }

  async setHash<T, A extends unknown[]>(def: HashKey<T, A>, value: T, ...args: A): Promise<void> {
    const key = def.key(...args);
    const m = this.redis.multi().hset(key, def.toHash(value));
    if (def.ttlSec) m.expire(key, def.ttlSec);
    await m.exec();
  }
}
```

### Using it

```ts
const store = new TypedStore(redis, (key, err) => logger.warn({ key, err }, "corrupt cache entry"));

await store.set(defs.productCache, product, product.id);   // checked: value must be a Product, args must be [number]
const p = await store.get(defs.productCache, 88);          // Product | null, fully typed

await store.set(defs.productCache, { wrong: true }, 88);   // ✗ compile error: not a Product
await store.get(defs.productCache, "88");                  // ✗ compile error: id must be a number
await store.get(defs.pageViews, 7);                        // number | null
```

The key definition is now the **single source of truth** for three things: how the key is named, what type lives there, and how it is encoded. You can't mix them up at a call site.

## Why validate on read

A realistic failure:

```
v1 deployed:  { id, name, price }                      → cached for 1 hour
v2 deployed:  { id, name, priceCents, updatedAt }      → reads v1 entries
```

Without validation, v2 code reads `priceCents` as `undefined` and charges nothing. With the schema codec, the read throws inside `decode`, the store logs it, drops the entry, and treats it as a miss. The cache heals itself.

Combine with versioned keys (`:v3`) so most shape changes never collide at all (see [Key Design](../05_key-management/01_key-design.md#versioned-keys)).

Cost: parsing adds CPU on every read. For very hot paths, validate only at the boundary that writes data, or sample (validate 1 in N reads).

## Typing custom commands (Lua)

Declare every custom command in **one file** so its signature lives next to its script ([Lua Scripts](../06_advanced-commands/03_lua-scripts.md)):

```ts
// src/redis/commands.ts
import type { Redis, Cluster } from "ioredis";
import { readFileSync } from "node:fs";
import { join } from "node:path";

declare module "ioredis" {
  interface RedisCommander<Context> {
    releaseLock(key: string, token: string): Promise<number>;
    rateLimit(key: string, limit: number, windowSec: number): Promise<[allowed: number, ttlSec: number]>;
    userUpdate(key: string, expectedVersion: number, ...fields: string[]): Promise<number>;
  }
}

const script = (name: string) =>
  readFileSync(join(import.meta.dirname, "scripts", `${name}.lua`), "utf8");

export function registerCommands(client: Redis | Cluster) {
  client.defineCommand("releaseLock", { numberOfKeys: 1, lua: script("release-lock") });
  client.defineCommand("rateLimit",   { numberOfKeys: 1, lua: script("rate-limit") });
  client.defineCommand("userUpdate",  { numberOfKeys: 1, lua: script("user-update") });
}
```

Call `registerCommands(client)` inside your connection factory so every client has them. Then `redis.rateLimit(key, 100, 60)` is typed, with no `as any`.

Notes:

- The augmentation's exact interface name and generics depend on your ioredis version, so check the typings you installed
- `import.meta.dirname` needs a recent Node.js (20.11+). Otherwise use `fileURLToPath(new URL(".", import.meta.url))`
- Keep the TypeScript signature and the Lua `KEYS`/`ARGV` order in the same file or directory, and cover them with an integration test

## Typed pipelines and transactions

Reuse the `unwrap` helper from [Pipelines](../06_advanced-commands/01_pipelines-and-auto-pipelining.md) and give it a tuple type so results are positional:

```ts
export function unwrap<T extends unknown[]>(results: [Error | null, unknown][] | null): T {
  if (!results) throw new Error("pipeline/transaction aborted");
  return results.map(([err, v]) => {
    if (err) throw err;
    return v;
  }) as T;
}

const [, views, title] = unwrap<[string, number, string | null]>(
  await redis.multi().set("a", 1).incr("views").get("title").exec()
);
```

The tuple is an assertion you maintain by hand, so keep pipelines short and close to their tuple type.

## The same idea for channels and streams

A channel definition ties a name to a payload type:

```ts
export interface Channel<T> { name: string; codec: Codec<T> }
export const channel = <T>(name: string, codec: Codec<T>): Channel<T> => ({ name, codec });

export const userEvents = channel("shop:events:user", schemaCodec(UserEventSchema));

class EventBus {
  constructor(private pub: Redis, private sub: Redis) {}

  publish<T>(ch: Channel<T>, msg: T) {
    return this.pub.publish(ch.name, ch.codec.encode(msg));
  }

  async subscribe<T>(ch: Channel<T>, handler: (msg: T) => void | Promise<void>) {
    await this.sub.subscribe(ch.name);
    this.sub.on("message", (channelName, raw) => {
      if (channelName !== ch.name) return;
      try { void handler(ch.codec.decode(raw)); }
      catch (err) { logger.warn({ err, channel: ch.name }, "bad message dropped"); }
    });
  }
}
```

Module `09_pub-sub` builds this out properly.

## Testing the typed layer

Test the pieces that hold the contract:

```ts
it("round-trips a product and revives dates", async () => {
  const p = { id: 1, name: "Pen", priceCents: 199, updatedAt: new Date("2026-09-30T00:00:00Z") };
  await store.set(defs.productCache, p, 1);
  expect(await store.get(defs.productCache, 1)).toEqual(p);
});

it("treats malformed cached data as a miss and removes it", async () => {
  await redis.set(defs.productCache.key(1), '{"id":1}');       // missing fields
  expect(await store.get(defs.productCache, 1)).toBeNull();
  expect(await redis.exists(defs.productCache.key(1))).toBe(0);
});
```

Compile-time behavior can be tested with `// @ts-expect-error` lines: the test fails if the line stops being an error.

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| `JSON.parse(raw) as T` everywhere | Schema codec in the key definition |
| Validation only at write time | Validate on read too, since data outlives deploys |
| Throwing on corrupt cache entries | Log, delete, treat as a miss |
| Dates and `BigInt` through JSON | `z.coerce.date()`, string-encode big integers |
| `as any` for custom commands | Module augmentation in one `commands.ts` |
| Key functions and value types defined in different files | One registry of key definitions |
| Hand-maintained tuple types drifting from the pipeline | Keep pipelines short, test them |
| Huge schemas validated on very hot reads | Validate at write time, or sample reads |

## Key takeaways

- Let the **key definition** carry the value type, argument types, TTL and encoding
- Compile-time types catch misuse, **runtime schemas** catch stale and corrupt data
- Declare Lua commands and their typings in one place
- Corrupt entries should heal (delete and miss), not crash requests

**Next:** [Redis Key Builder](./05_redis-key-builder.md)
