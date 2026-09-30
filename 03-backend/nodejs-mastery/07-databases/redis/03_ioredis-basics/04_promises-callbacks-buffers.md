# Promises, Callbacks and Buffers

ioredis supports several ways to receive results and two data representations: strings and Buffers.

## Promises (recommended)

Every command returns a Promise:

```ts
const value = await redis.get("k");

redis.get("k").then((v) => console.log(v)).catch((e) => console.error(e));
```

### Concurrency with `Promise.all`

```ts
const [a, b, c] = await Promise.all([
  redis.get("a"),
  redis.get("b"),
  redis.get("c"),
]);
```

All three are sent immediately over the same connection. This is safe and fast (see [Connection](./01_connection.md)). For hundreds or thousands of commands, prefer a pipeline or auto pipelining to reduce overhead.

### Sequential vs parallel

```ts
// Sequential: 3 round trips one after another
const x = await redis.get("a");
const y = await redis.get("b");
const z = await redis.get("c");

// Parallel: sent together
const [x2, y2, z2] = await Promise.all([redis.get("a"), redis.get("b"), redis.get("c")]);
```

Only use sequential calls when a later command depends on an earlier result.

### Unhandled rejections

An un-awaited failing command becomes an unhandled rejection, which can crash Node 15+ processes:

```ts
redis.set("k", "v");            // If this rejects and nobody handles it: trouble
redis.set("k", "v").catch(log); // fire-and-forget, safely
```

## Callbacks

The Node-style callback is still supported as the **last argument**:

```ts
redis.get("k", (err, result) => {
  if (err) return console.error(err);
  console.log(result);
});
```

When you pass a callback, the command still returns a Promise, and the callback is invoked too. Use callbacks only for legacy code. Mixing both styles in one call site is confusing.

## Strings vs Buffers

By default ioredis decodes replies as **UTF-8 strings**. That is fine for text and JSON, but corrupts arbitrary binary data.

For binary values, use the **`Buffer` variant** of a command by appending `Buffer` to the method name:

```ts
await redis.set("img", fs.readFileSync("logo.png"));       // Buffers are accepted as arguments
const data = await redis.getBuffer("img");                 // Buffer
fs.writeFileSync("copy.png", data!);
```

More variants:

```ts
await redis.hgetBuffer("h", "field");
await redis.lrangeBuffer("list", 0, -1);   // Buffer[]
await redis.smembersBuffer("set");         // Buffer[]
await redis.callBuffer("GET", "img");      // any command, raw Buffer reply
```

Use Buffer variants when:

- Storing images, files, protobuf, MessagePack, compressed data, encrypted blobs
- Keys are binary
- You need exact bytes back

### Example: compressed cache values

```ts
import { gzipSync, gunzipSync } from "node:zlib";

async function setCompressed(key: string, obj: unknown, ttl = 300) {
  const buf = gzipSync(Buffer.from(JSON.stringify(obj)));
  await redis.set(key, buf, "EX", ttl);
}

async function getCompressed<T>(key: string): Promise<T | null> {
  const buf = await redis.getBuffer(key);          // must be Buffer, not string
  return buf ? (JSON.parse(gunzipSync(buf).toString()) as T) : null;
}
```

Reading compressed data with plain `get()` would decode it as UTF-8 and corrupt it.

## Numbers and large integers

Integer replies (`INCR`, `LLEN`, `ZADD`, etc.) arrive as JavaScript numbers. JavaScript numbers are safe only up to 2^53 - 1. For counters that may exceed that, either keep them as strings or enable:

```ts
new Redis({ stringNumbers: true }); // integer replies returned as strings
```

Values stored with `SET` and read with `GET` are always strings, so convert with `Number()` (or `BigInt()` for huge values).

## Working with JSON safely

```ts
async function getJson<T>(key: string): Promise<T | null> {
  const raw = await redis.get(key);
  if (raw === null) return null;
  try {
    return JSON.parse(raw) as T;
  } catch {
    await redis.del(key);         // corrupted entry, drop it
    return null;
  }
}

async function setJson(key: string, value: unknown, ttlSec?: number) {
  const s = JSON.stringify(value);
  if (ttlSec) await redis.set(key, s, "EX", ttlSec);
  else await redis.set(key, s);
}
```

JSON drops `undefined`, functions and converts `Date` to strings. Revive dates yourself when reading.

## Choosing a style

| Need | Use |
|------|-----|
| Normal application code | Promises with `async/await` |
| Independent commands | `Promise.all` or a pipeline |
| Legacy callback code | Callbacks |
| Text, JSON | Default string methods |
| Binary data | `...Buffer` methods |
| Very large integers | `stringNumbers: true` or `BigInt` |

## Common mistakes

| Mistake | Effect | Fix |
|---------|--------|-----|
| Using `get` on binary data | Corrupted bytes | `getBuffer` |
| Awaiting in a loop for independent reads | Slow | `Promise.all` or pipeline |
| Fire-and-forget without `.catch` | Unhandled rejection | Attach a catch handler |
| Forgetting numbers come back as strings | String concatenation bugs | `Number()` |
| Assuming `hgetall` values are typed | Everything is a string | Convert per field |

## Key takeaways

- Promises with `async/await` are the default, and callbacks are legacy
- Concurrent commands share one connection safely
- Use `...Buffer` methods for binary data
- Convert strings to numbers and JSON yourself, and handle `null`

**Next:** [Error Handling](./05_error-handling.md)
