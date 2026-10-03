# Serialization

Redis stores **bytes**. Anything richer than a string, a number or a flat hash has to be converted on the way in and back on the way out. That conversion is serialization, and it affects **memory, network, CPU, safety and compatibility**.

```
JS object ──encode──► bytes ──SET──► Redis ──GET──► bytes ──decode──► JS object
```

## Options at a glance

| Format         | Size     | Speed         | Human-readable | Cross-language | Schema   |
| -------------- | -------- | ------------- | -------------- | -------------- | -------- |
| JSON           | Medium   | Fast (native) | Yes            | Yes            | No       |
| MessagePack    | Smaller  | Fast          | No             | Yes            | No       |
| CBOR           | Smaller  | Fast          | No             | Yes            | No       |
| Protobuf       | Smallest | Fast          | No             | Yes            | Required |
| Avro           | Small    | Fast          | No             | Yes            | Required |
| `v8.serialize` | Medium   | Fast          | No             | **Node only**  | No       |
| Hash fields    | Compact  | Fast          | Yes            | Yes            | Implicit |

Guideline: **start with JSON, move to MessagePack or Protobuf when size or CPU shows up in measurements, and add compression only for large values.**

## JSON (the default)

```ts
const user = { id: 1042, name: "Ada", plan: "pro" };

await redis.set("user:1042", JSON.stringify(user), "EX", 3600);

const raw = await redis.get("user:1042");
const loaded = raw ? (JSON.parse(raw) as typeof user) : null;
```

Pros: debuggable in `redis-cli`, universal, no dependency. Cons: verbose, no binary type, lossy for several JavaScript types.

### What JSON silently breaks

| Value             | After `JSON.parse(JSON.stringify(x))`            |
| ----------------- | ------------------------------------------------ |
| `Date`            | ISO string, not a `Date`                         |
| `undefined`       | Property dropped (becomes `null` in arrays)      |
| `NaN`, `Infinity` | `null`                                           |
| `Map`, `Set`      | `{}`                                             |
| `BigInt`          | **Throws** `TypeError`                           |
| `Buffer`          | `{ type: "Buffer", data: [...] }`, which is huge |
| Class instance    | Plain object without methods                     |

Handle these explicitly:

```ts
const replacer = (_k: string, v: unknown) =>
  typeof v === "bigint" ? { $big: v.toString() } : v;

const reviver = (_k: string, v: any) =>
  v && typeof v === "object" && "$big" in v ? BigInt(v.$big) : v;

const text = JSON.stringify({ n: 10n }, replacer);
const back = JSON.parse(text, reviver);

// Dates: store epoch millis and rebuild on read
const record = { at: Date.now() };
const at = new Date(record.at);
```

## Validate on read

Data in Redis outlives your code. A deploy, a bug or another service can leave a shape you do not expect. Validate at the boundary instead of trusting a cast:

```ts
import { z } from "zod";

const UserSchema = z.object({
  id: z.number(),
  name: z.string(),
  plan: z.enum(["free", "pro"]),
});
type User = z.infer<typeof UserSchema>;

async function getUser(id: number): Promise<User | null> {
  const raw = await redis.get(`user:${id}`);
  if (!raw) return null;

  const parsed = UserSchema.safeParse(JSON.parse(raw));
  if (!parsed.success) {
    await redis.unlink(`user:${id}`); // bad or stale entry: treat as a cache miss
    return null;
  }
  return parsed.data;
}
```

For a cache, a failed validation is just a miss. For a primary store, log it and alert.

## A pluggable serializer

Hide the format behind one interface so you can change it later without touching call sites. This slots into the repository layer from [Redis Repository](../08_nodejs-integration/03_redis-repository.md).

```ts
export interface Serializer<T> {
  encode(value: T): Buffer;
  decode(data: Buffer): T;
}

export class JsonSerializer<T> implements Serializer<T> {
  encode(value: T) {
    return Buffer.from(JSON.stringify(value), "utf8");
  }
  decode(data: Buffer) {
    return JSON.parse(data.toString("utf8")) as T;
  }
}
```

Use the `Buffer` variants of commands so binary data is not mangled by string decoding:

```ts
const ser = new JsonSerializer<User>();

await redis.set("user:1042", ser.encode(user), "EX", 3600);

const buf = await redis.getBuffer("user:1042"); // Buffer | null
const value = buf ? ser.decode(buf) : null;
```

Related buffer commands: `getBuffer`, `hgetBuffer`, `hgetallBuffer`, `lrangeBuffer`, `mgetBuffer`. See [Promises, Callbacks and Buffers](../03_ioredis-basics/04_promises-callbacks-buffers.md).

## MessagePack

A binary format with the same data model as JSON. Usually smaller, and it handles binary and a few extra types natively.

```ts
import { encode, decode } from "@msgpack/msgpack";

class MsgpackSerializer<T> implements Serializer<T> {
  encode(value: T) {
    return Buffer.from(encode(value));
  }
  decode(data: Buffer) {
    return decode(data) as T;
  }
}
```

Good fit when you cache many medium-sized objects and want a quick win without schemas. A bonus: Redis Lua scripts have `cmsgpack` and `cjson` built in, so a script can read these values directly:

```lua
local obj = cmsgpack.unpack(redis.call("GET", KEYS[1]))
return obj.plan
```

## Protobuf (schema-based)

Smallest and fastest to parse, with explicit schemas and safe evolution. Best for high-volume data or when several services in different languages share the same keys.

```proto
// user.proto
syntax = "proto3";
message User {
  int64  id    = 1;
  string name  = 2;
  string plan  = 3;
}
```

```ts
// with a generator such as ts-proto
const bytes = Buffer.from(
  User.encode({ id: 1042, name: "Ada", plan: "pro" }).finish(),
);
await redis.set("user:1042", bytes);

const buf = await redis.getBuffer("user:1042");
const u = buf ? User.decode(buf) : null;
```

Cost: a build step and tooling, and data you can no longer eyeball in `redis-cli`.

## `v8.serialize` (Node only)

```ts
import v8 from "node:v8";

const bytes = v8.serialize(new Map([["a", new Date()]])); // keeps Map, Set, Date, BigInt
const value = v8.deserialize(bytes);
```

It round-trips JavaScript types faithfully, but:

- Not readable by other languages
- Format can change between Node versions, so old entries may not decode after an upgrade
- Never deserialize data that could come from an untrusted source

Reasonable for a short-lived, Node-only cache. Avoid it for anything long-lived or shared.

## Hash vs serialized blob

Sometimes the best serialization is none. For flat objects, store fields directly:

| Need                     | Hash fields      | Serialized blob                     |
| ------------------------ | ---------------- | ----------------------------------- |
| Read or update one field | O(1), no rewrite | Read, decode, change, encode, write |
| Atomic counters          | `HINCRBY`        | Not possible                        |
| Nested data              | Awkward          | Natural                             |
| Read the whole object    | `HGETALL`        | `GET` and decode                    |

Details in [Hashes](../04_data-structures/02_hashes.md). A common compromise is a hash with scalar fields plus one JSON-string field for nested parts.

## Compression

Compress only when values are large and compressible (JSON text usually is). For small values the CPU and header cost exceeds the savings.

```ts
import { promisify } from "node:util";
import {
  gzip,
  gunzip,
  brotliCompress,
  brotliDecompress,
  constants,
} from "node:zlib";

const gz = promisify(gzip);
const gunz = promisify(gunzip);
```

Wrap any serializer with a threshold and a one-byte header so readers know whether a value is compressed:

```ts
const RAW = 0x00;
const GZIP = 0x01;

export class CompressedSerializer<T> {
  constructor(
    private inner: Serializer<T>,
    private threshold = 1024, // bytes; tune with real data
  ) {}

  async encode(value: T): Promise<Buffer> {
    const body = this.inner.encode(value);
    if (body.length < this.threshold) {
      return Buffer.concat([Buffer.from([RAW]), body]);
    }
    const packed = await gz(body, { level: 6 });
    return Buffer.concat([Buffer.from([GZIP]), packed]);
  }

  async decode(data: Buffer): Promise<T> {
    const flag = data[0];
    const body = data.subarray(1);
    const bytes = flag === GZIP ? await gunz(body) : body;
    return this.inner.decode(bytes);
  }
}
```

Notes:

- Use the **async** zlib functions. The sync versions block the event loop on large payloads
- Gzip is a safe default. Brotli compresses better but is slower at high quality, so use a low quality level (for example 4 to 5) for hot paths
- The flag byte lets you change the threshold or algorithm later without breaking old entries
- Compression helps most when it also cuts network transfer, not just memory

## Versioning and schema evolution

A cached shape changes the day you add a field. Plan for old entries still in Redis.

### Option 1: version in the key

```ts
const key = (id: number) => `v2:user:${id}`; // bump on breaking change; old keys expire
```

Simple and safe for caches. Old entries die by TTL.

### Option 2: version in the payload

```ts
interface Envelope<T> {
  v: number;
  data: T;
}

await redis.set(
  key,
  JSON.stringify({ v: 2, data: user } satisfies Envelope<User>),
);

function upgrade(env: Envelope<any>): User {
  if (env.v === 1) return { ...env.data, plan: "free" }; // migrate old shape
  return env.data;
}
```

Use this when entries are long-lived and you cannot afford to drop them. Protobuf and Avro give you evolution rules built in (add optional fields, never reuse field numbers).

## Measure before you switch

Sizes and speeds depend heavily on your data shape, so test with real samples:

```ts
import { encode as mpEncode, decode as mpDecode } from "@msgpack/msgpack";

const sample = await loadRealObjectFromDb(); // use a realistic payload, not toy data

const json = Buffer.from(JSON.stringify(sample));
const mp = Buffer.from(mpEncode(sample));
console.log({ json: json.length, msgpack: mp.length });

const N = 100_000;
let t = performance.now();
for (let i = 0; i < N; i++) JSON.parse(json.toString());
console.log("json decode ms", performance.now() - t);

t = performance.now();
for (let i = 0; i < N; i++) mpDecode(mp);
console.log("msgpack decode ms", performance.now() - t);
```

Compare **bytes stored**, **encode and decode time** and **end-to-end latency**. Native `JSON.parse` is heavily optimized, so MessagePack is not always faster in Node. The size savings are the usual reason to switch. For memory sizing, combine this with `MEMORY USAGE` from [Memory Optimization](./03_memory-optimization.md).

## Security

- Never use formats that can instantiate arbitrary objects or run code on untrusted data (`node-serialize`, `eval`, unsafe YAML loaders). `v8.deserialize` is also not for untrusted input
- Treat everything read from Redis as untrusted if other systems can write to it. Validate with a schema
- Guard against prototype pollution when merging parsed objects into others: avoid deep-merging raw `JSON.parse` output into live objects
- Do not store secrets in plain serialized blobs. Encrypt sensitive fields, or keep them out of Redis. See [Security](../17_security/03_security-checklist.md)

## Redis JSON module

If you need to read or update **inside** a nested document atomically, the Redis JSON data type (`JSON.SET`, `JSON.GET`, `JSON.NUMINCRBY`) can do it server-side without a read-modify-write cycle. It ships as a module in older setups and is bundled with newer Redis releases. Availability depends on your server version and provider, so check before relying on it.

```ts
await redis.call(
  "JSON.SET",
  "user:1042",
  "$",
  JSON.stringify({ plan: "pro", usage: { calls: 0 } }),
);
await redis.call("JSON.NUMINCRBY", "user:1042", "$.usage.calls", "1");
const doc = await redis.call("JSON.GET", "user:1042", "$.usage");
```

## Choosing a format

```
Flat object, independent fields?            → Hash
Nested, read as a whole, simple setup?      → JSON string
Many medium objects, need smaller size?     → MessagePack
High volume or shared across languages?     → Protobuf (or Avro)
Values over ~1 KB and compressible?         → add compression (gzip/brotli)
Node-only, short-lived, rich JS types?      → v8.serialize (carefully)
Need server-side edits inside a document?   → Redis JSON
```

## Pitfalls

| Pitfall                                                 | Fix                                             |
| ------------------------------------------------------- | ----------------------------------------------- |
| `Date`, `Map`, `Set`, `BigInt` lost or throwing in JSON | Custom replacer or reviver, or store primitives |
| Binary data read as a UTF-8 string                      | Use `getBuffer` and friends                     |
| Trusting `JSON.parse(...) as T`                         | Validate with a schema (for example zod)        |
| Changing shape without a plan                           | Version in the key or the payload               |
| Compressing tiny values                                 | Use a size threshold (for example 1 KB)         |
| Sync zlib on large payloads                             | Use async zlib                                  |
| `v8.deserialize` on untrusted data                      | Do not. Use JSON or Protobuf with validation    |
| Switching format without measuring                      | Benchmark size and latency on real data         |
| Mixed formats under one key prefix                      | One format per key family, or a header byte     |
| Serializing everything when a hash would do             | Use hash fields for flat objects                |

## Key takeaways

- Redis stores bytes, and your serializer decides size, speed and compatibility
- JSON is the right default: simple and debuggable
- Move to MessagePack or Protobuf when measurements justify it
- Use `Buffer` commands for binary payloads and validate everything you read
- Compress only large values, use async zlib and mark the format with a header byte
- Version your data so deploys do not break cached entries

**Previous:** [Memory Optimization](./03_memory-optimization.md) | **Next:** [Authentication and ACL](../17_security/01_authentication-and-acl.md)
