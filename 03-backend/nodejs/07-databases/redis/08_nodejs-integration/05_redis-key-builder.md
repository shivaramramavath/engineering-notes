# Redis Key Builder

[Key Design](../05_key-management/01_key-design.md) explained **what** good keys look like. This lesson builds the tool that produces them: a small, tested module that every other file uses, so keys can't drift, collide or leak.

## Requirements

A key builder should:

1. Produce **consistent, hierarchical** keys from one place
2. **Reject or encode** unsafe input (colons, whitespace, glob characters, braces)
3. Support **hash tags** for Cluster-safe grouping
4. Produce **SCAN patterns** that match exactly what it builds
5. Carry **policy** (TTL, eviction safety, owner) for each key class
6. Be **easy to test**

## Segment encoding

Segments come from user input, database IDs and emails. They must not be able to change the **structure** of a key:

```ts
// "1:cart" must not become two segments. "*" must not become a wildcard.
const UNSAFE = /[%:\s*?\[\]\\{}]/g;

export function encodeSegment(input: string | number): string {
  const s = String(input);
  if (s.length === 0) throw new Error("empty key segment");
  return s.replace(UNSAFE, (c) => "%" + c.charCodeAt(0).toString(16).toUpperCase().padStart(2, "0"));
}

export function decodeSegment(s: string): string {
  return decodeURIComponent(s);
}
```

| Input | Encoded |
|-------|---------|
| `1042` | `1042` |
| `ada@example.com` | `ada@example.com` (`@` and `.` are fine) |
| `1:admin` | `1%3Aadmin` |
| `a*b` | `a%2Ab` |
| `50% off` | `50%25%20off` |

Because `%` is always encoded, decoding is exact, and encoded glob characters can never act as wildcards in a `SCAN` pattern.

## The builder

```ts
// src/redis/key-builder.ts
export const WILDCARD = Symbol("wildcard");
export type Part = string | number;
export type PatternPart = Part | typeof WILDCARD;

export class KeyBuilder {
  private constructor(private readonly prefix: readonly string[]) {}

  /** KeyBuilder.create("shop") → every key starts with "shop:" */
  static create(...prefix: Part[]): KeyBuilder {
    return new KeyBuilder(prefix.map(encodeSegment));
  }

  /** A narrower builder: K.child("cache") → "shop:cache:..." */
  child(...segments: Part[]): KeyBuilder {
    return new KeyBuilder([...this.prefix, ...segments.map(encodeSegment)]);
  }

  /** shop:user:1042 */
  key(...segments: Part[]): string {
    return [...this.prefix, ...segments.map(encodeSegment)].join(":");
  }

  /**
   * Cluster-safe key. Only the text inside the FIRST {...} is hashed,
   * so all keys sharing a tag land in the same slot.
   *   K.tagged(["user", 1042], "profile") → shop:{user:1042}:profile
   */
  tagged(tag: Part[], ...segments: Part[]): string {
    const t = tag.map(encodeSegment).join(":");
    return [...this.prefix, `{${t}}`, ...segments.map(encodeSegment)].join(":");
  }

  /** A SCAN pattern. Literal parts are encoded like real keys, WILDCARD becomes "*". */
  pattern(...segments: PatternPart[]): string {
    return [...this.prefix, ...segments.map((s) => (s === WILDCARD ? "*" : encodeSegment(s)))].join(":");
  }

  /** Split a key back into decoded segments (prefix removed, braces stripped). */
  parse(key: string): string[] {
    const parts = key.split(":");
    const rest = parts.slice(this.prefix.length);
    return rest.map((p) => decodeSegment(p.replace(/^\{|\}$/g, "")));
  }

  /** Does this key belong to this builder's namespace? */
  owns(key: string): boolean {
    return key.startsWith(this.prefix.join(":") + ":");
  }
}
```

Note on tags: `tagged(["user", 1042], ...)` puts a **colon inside** the braces (`{user:1042}`). That is deliberate and valid, since the whole text between the braces is hashed. `parse` strips only the outer braces of each part, so a tag with a colon parses as two parts (`user`, `1042`) when split. If you need exact round-tripping for tagged keys, keep them in a separate `parseTagged`.

## The key registry

Put every key in one file, built from the builder:

```ts
// src/redis/keys.ts
import { KeyBuilder, WILDCARD } from "./key-builder.js";
import { env } from "../config/env.js";

const K = KeyBuilder.create(env.redis.keyPrefix);      // e.g. "shop"
const cache = K.child("cache");
const idx = K.child("idx");

type Id = string | number;

export const keys = {
  // entities
  user:             (id: Id)    => K.key("user", id),
  userCart:         (id: Id)    => K.key("user", id, "cart"),
  userEmailIndex:   (email: string) => idx.key("user", "email", email.toLowerCase()),
  userCreatedIndex: ()          => idx.key("user", "created"),

  // sessions and security
  session:          (sid: string) => K.key("session", sid),
  lock:             (name: string) => K.key("lock", name),
  rateLimit:        (scope: string, id: string) => K.key("rate", scope, id),
  otp:              (phone: string) => K.key("otp", phone),

  // caches (versioned)
  productCache:     (id: Id, v = 3) => cache.key("product", id, `v${v}`),
  httpCache:        (digest: string, v = 1) => cache.key("http", `v${v}`, digest),
  pageViews:        (postId: Id)   => K.key("views", postId),

  // time-bucketed
  dau:              (day: string)  => K.key("dau", day),

  // Cluster-safe group: profile, cart and orders share one slot
  userGroup: {
    profile: (id: Id) => K.tagged(["user", id], "profile"),
    cart:    (id: Id) => K.tagged(["user", id], "cart"),
    orders:  (id: Id) => K.tagged(["user", id], "orders"),
  },

  // SCAN patterns, never for KEYS
  patterns: {
    productCache: () => cache.pattern("product", WILDCARD),
    allCache:     () => cache.pattern(WILDCARD),
    userAll:      (id: Id) => K.pattern("user", id, WILDCARD),
  },
};
```

Rules for this file:

- **No key construction anywhere else.** Code review rule: `redis.get(\`...\`)` with a template literal is a bug
- Functions take **typed arguments** (numbers, strings), never pre-built fragments
- Cache keys **carry a version**, with a default argument so call sites stay short
- Patterns live next to the keys they match

The [repository](./03_redis-repository.md), the [typed registry](./04_typed-redis-client.md) and the [middleware](./06_express-integration.md) in this module all import from here.

## Key classes with policy

Give every family of keys a declared policy. This turns the table from the key design lesson into code:

```ts
// src/redis/key-classes.ts
export interface KeyClass {
  name: string;
  regex: RegExp;                 // recognizes keys of this class
  type: "string" | "hash" | "list" | "set" | "zset" | "stream";
  ttl: { min?: number; max?: number } | "none";   // seconds
  evictable: boolean;            // safe to lose under memory pressure?
  owner: string;
}

const p = env.redis.keyPrefix;

export const KEY_CLASSES: KeyClass[] = [
  { name: "cache",   regex: new RegExp(`^${p}:cache:`),   type: "string", ttl: { min: 10, max: 86_400 }, evictable: true,  owner: "platform" },
  { name: "session", regex: new RegExp(`^${p}:session:`), type: "hash",   ttl: { min: 60, max: 604_800 }, evictable: false, owner: "auth" },
  { name: "lock",    regex: new RegExp(`^${p}:lock:`),    type: "string", ttl: { max: 120 },              evictable: false, owner: "orders" },
  { name: "rate",    regex: new RegExp(`^${p}:rate:`),    type: "string", ttl: { max: 3_600 },            evictable: true,  owner: "platform" },
  { name: "user",    regex: new RegExp(`^${p}:user:`),    type: "hash",   ttl: "none",                    evictable: false, owner: "accounts" },
  { name: "idx",     regex: new RegExp(`^${p}:idx:`),     type: "zset",   ttl: "none",                    evictable: false, owner: "accounts" },
];

export function classify(key: string): KeyClass | undefined {
  return KEY_CLASSES.find((c) => c.regex.test(key));
}
```

Use it to audit a live keyspace with `scanStream` (see [Scan and Iteration](../05_key-management/02_scan-and-iteration.md)):

```ts
async function auditTtl() {
  const problems: string[] = [];
  for await (const batch of redis.scanStream({ match: `${env.redis.keyPrefix}:*`, count: 500 })) {
    const p = redis.pipeline();
    batch.forEach((k: string) => p.ttl(k));
    const res = (await p.exec()) ?? [];

    res.forEach(([, ttl], i) => {
      const key = batch[i] as string;
      const cls = classify(key);
      if (!cls) return problems.push(`unclassified: ${key}`);
      if (cls.ttl !== "none" && ttl === -1) problems.push(`missing TTL (${cls.name}): ${key}`);
    });
  }
  return problems;
}
```

Also generate `KEYS.md` from `KEY_CLASSES` in a build step, so the docs can't go stale.

## Namespace versions for bulk invalidation

```ts
export const nsVersionKey = (ns: string) => K.key("ver", ns);

export async function versioned(redis: Redis, ns: string, ...segments: Part[]) {
  const v = (await redis.get(nsVersionKey(ns))) ?? "1";
  return cache.key(ns, `n${v}`, ...segments);
}

export const invalidateNamespace = (redis: Redis, ns: string) => redis.incr(nsVersionKey(ns));
```

See [Cache Invalidation](../07_caching/03_cache-invalidation.md#4-versioned-keys-and-namespaces). Cache the version in-process for a second or two to avoid an extra read per lookup.

## Environments and tenants

```ts
// environment in the prefix when sharing an instance (separate instances are better)
const K = KeyBuilder.create(env.redis.keyPrefix);          // "shop" or "shop-staging"

// tenant-aware builder, created per request or per job
export const tenantKeys = (tenantId: Id) => {
  const T = K.child("t", tenantId);
  return {
    user:  (id: Id) => T.key("user", id),
    cache: (name: string, id: Id) => T.key("cache", name, id),
    pattern: () => T.pattern(WILDCARD),                      // find or delete one tenant's data
  };
};
```

For real isolation, use separate instances or ACL key patterns (`~shop:t:acme:*`). Naming alone doesn't enforce anything.

## `keyPrefix` vs a builder

| | ioredis `keyPrefix` | Key builder |
|---|---------------------|-------------|
| Applies to key arguments of commands | Yes | Yes (explicit) |
| Applies to `SCAN` patterns and results | **No** | Yes (`pattern()`) |
| Applies to Pub/Sub channels | **No** | Yes (`K.key("events", ...)`) |
| Applies to keys inside Lua | Only declared `KEYS` | Yes, if you pass them |
| Visible in code | Hidden | Explicit |
| Risk of double prefixing | **Yes** (with scanned names) | No |

Pick the builder and **don't also set `keyPrefix`**, or you'll get `shop:shop:...` keys.

## Tests

Use Node's built-in runner (no extra dependency):

```ts
import test from "node:test";
import assert from "node:assert/strict";
import { KeyBuilder, WILDCARD, encodeSegment, decodeSegment } from "./key-builder.js";

const K = KeyBuilder.create("shop");

test("builds hierarchical keys", () => {
  assert.equal(K.key("user", 1042, "cart"), "shop:user:1042:cart");
});

test("encodes structural characters so they can't alter the key", () => {
  assert.equal(K.key("user", "1:admin"), "shop:user:1%3Aadmin");
  assert.equal(K.key("user", "a*b"), "shop:user:a%2Ab");
});

test("decode is the exact inverse of encode", () => {
  for (const s of ["a:b", "50% off", "x{y}", "a*b?c[d]", "plain", "ünïcode"]) {
    assert.equal(decodeSegment(encodeSegment(s)), s);
  }
});

test("rejects empty segments", () => {
  assert.throws(() => K.key("user", ""));
});

test("tagged keys share the same hash tag", () => {
  const a = K.tagged(["user", 1], "profile");
  const b = K.tagged(["user", 1], "cart");
  assert.equal(a.match(/\{[^}]*\}/)![0], b.match(/\{[^}]*\}/)![0]);
});

test("patterns only treat WILDCARD as a wildcard", () => {
  assert.equal(K.pattern("cache", WILDCARD), "shop:cache:*");
  assert.equal(K.pattern("cache", "a*b"), "shop:cache:a%2Ab");   // literal star is escaped
});

test("parse returns decoded segments", () => {
  assert.deepEqual(K.parse("shop:user:1%3Aadmin"), ["user", "1:admin"]);
});
```

For extra confidence, add a **property-based test** (fast-check): for random strings, `decode(encode(s)) === s` and the encoded form contains no `:`, whitespace or glob characters.

A Cluster check worth having: compute the slot of each key in a tagged group with a CRC16 helper (or call `CLUSTER KEYSLOT` in an integration test) and assert they are equal.

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| Template-literal keys scattered around | Everything through `keys.ts` |
| Raw IDs, emails and user text in keys | `encodeSegment` in the builder |
| Also setting `keyPrefix` | Choose one, preferably the builder |
| Unversioned cache keys | A version segment with a default |
| Patterns built by string concatenation | `pattern()` with `WILDCARD` |
| Hash-tagging everything the same (`{app}`) | Tag by entity |
| Policies living only in a wiki | `KEY_CLASSES` in code, plus an audit |
| Changing a key's shape without a migration plan | New version segment, let old keys expire |

## Key takeaways

- A key builder encodes segments, builds patterns and supports hash tags, so input can't change key structure
- One `keys.ts` registry is the only place keys are defined
- Key classes turn TTL and eviction policy into code you can audit against the live keyspace
- Choose the builder **or** `keyPrefix`, not both, and test the builder thoroughly

**Next:** [Express Integration](./06_express-integration.md)
