# Strings

The simplest and most used type. A Redis string is a **binary-safe value up to 512 MB**. It can hold text, JSON, a number, serialized bytes or an image.

## Command tour

```ts
await redis.set("name", "Ada");                    // SET
await redis.get("name");                           // "Ada" (or null)
await redis.getdel("name");                        // read and delete in one step
await redis.getex("name", "EX", 60);               // read and set a new TTL
await redis.strlen("name");                        // length in bytes
await redis.append("log", "line\n");               // returns new length
await redis.getrange("name", 0, 2);                // substring
await redis.setrange("name", 1, "xx");             // overwrite from offset
```

Multi-key operations run in a single round trip:

```ts
await redis.mset({ a: 1, b: 2, c: 3 });            // set many
await redis.mget("a", "b", "missing");             // ["1", "2", null]
await redis.msetnx({ x: 1, y: 2 });                // all-or-nothing, only if none exist
```

`SET` options (`EX`, `PX`, `NX`, `XX`, `KEEPTTL`, `GET`) are covered in [Commands and Options](../03_ioredis-basics/03_commands-and-options.md).

> `SETNX`, `SETEX` and `GETSET` are legacy. Prefer `SET` with options.

## Counters

Redis interprets a string that looks like an integer as a number, and `INCR` is **atomic**, so concurrent clients never lose updates.

```ts
await redis.incr("views:post:42");                 // 1, 2, 3 ...
await redis.incrby("stock:sku1", -5);              // subtract 5
await redis.decr("tickets");
await redis.incrbyfloat("balance", 10.5);          // returns the new value as a string
```

Rules:

- Values must fit a **signed 64-bit integer**, otherwise `ERR increment or decrement would overflow`
- Incrementing a non-numeric string fails with `ERR value is not an integer or out of range`
- Missing keys start at `0`
- `INCR` keeps the existing TTL

### Sequence generator

```ts
const orderId = await redis.incr("seq:order");     // unique, monotonically increasing
```

## Patterns

### 1. Cache a JSON value

```ts
await redis.set(`cache:product:${id}`, JSON.stringify(product), "EX", 300);
const raw = await redis.get(`cache:product:${id}`);
const product = raw ? JSON.parse(raw) : null;
```

Best for values you read and write **as a whole**. If you update single fields often, use a [hash](./02_hashes.md).

### 2. Counter with expiry (fixed window)

```ts
const key = `hits:${ip}:${Math.floor(Date.now() / 60_000)}`;
const n = await redis.incr(key);
if (n === 1) await redis.expire(key, 60);
```

### 3. One-time flag or dedupe marker

```ts
const first = await redis.set(`seen:${eventId}`, "1", "EX", 86400, "NX");
if (first === "OK") {
  // first time we see this event, process it
}
```

### 4. Simple lock

```ts
const token = crypto.randomUUID();
const ok = await redis.set(`lock:${resource}`, token, "PX", 10_000, "NX");
// release safely with a Lua script that checks the token (module 11)
```

### 5. Feature flag or config value

```ts
await redis.set("flag:new-checkout", "on");
const enabled = (await redis.get("flag:new-checkout")) === "on";
```

### 6. OTP and short-lived tokens

```ts
await redis.set(`otp:${phone}`, code, "EX", 300);
```

## Encodings and memory

Small strings and integers use compact internal encodings:

```ts
await redis.set("n", 12345);
await redis.object("ENCODING", "n");   // "int"
await redis.set("s", "hello");
await redis.object("ENCODING", "s");   // "embstr" (short) or "raw" (long)
```

Every key has overhead, so **millions of tiny string keys cost more memory than the same data in hashes** (see [Hashes](./02_hashes.md)).

## Complexity

| Command | Cost |
|---------|------|
| `GET`, `SET`, `INCR`, `APPEND`, `STRLEN` | O(1) |
| `MGET`, `MSET` | O(N) keys |
| `GETRANGE`, `SETRANGE` | O(N) bytes touched |

## Pitfalls

| Pitfall | Why it matters | Fix |
|---------|----------------|-----|
| Storing an object as JSON and rewriting it for each field change | Race conditions and wasted bandwidth | Use a hash |
| Reading a counter and writing back (`get` then `set`) | Lost updates | Use `INCR` |
| `SET` without options after `SET ... EX` | Silently removes the TTL | `KEEPTTL` |
| Huge values (MBs) | Blocks the thread, bloats replication | Store elsewhere, keep a reference |
| Assuming numbers come back as numbers | `"5" + 1 === "51"` | `Number()` |
| `GET` on binary data | UTF-8 corruption | `getBuffer` |

## Key takeaways

- Strings hold text, numbers or bytes, up to 512 MB
- `INCR` and `SET NX` are the building blocks for counters, locks and dedupe
- Use `MGET`/`MSET` to batch, and `SET ... EX` for atomic expiry
- If you update parts of a value, a hash is usually better

**Next:** [Hashes](./02_hashes.md)
