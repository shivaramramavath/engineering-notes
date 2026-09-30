# Commands and Options

Every Redis command is a **lowercase method** on the client. Arguments are passed positionally, and command flags (like `EX` or `NX`) are passed as extra string arguments.

## Basic pattern

```ts
await redis.set("name", "Ada");          // SET name Ada
await redis.get("name");                 // GET name
await redis.del("a", "b", "c");          // DEL a b c
await redis.incrby("views", 5);          // INCRBY views 5
```

Redis command `SET key value EX 60 NX` becomes:

```ts
await redis.set("key", "value", "EX", 60, "NX");
```

Just append the tokens in the same order as the Redis docs.

## Command options you'll use often

### `SET`

```ts
await redis.set("k", "v", "EX", 60);           // expire in 60s
await redis.set("k", "v", "PX", 1500);         // expire in 1500ms
await redis.set("k", "v", "NX");               // only if the key does not exist
await redis.set("k", "v", "XX");               // only if the key exists
await redis.set("k", "v", "KEEPTTL");          // keep the existing TTL
await redis.set("k", "v", "EX", 30, "NX");     // combined
await redis.set("k", "new", "GET");            // returns the old value (Redis 6.2+)
```

Return values:

| Call | Returns |
|------|---------|
| `set` success | `"OK"` |
| `set ... NX` and key exists | `null` |
| `set ... XX` and key missing | `null` |
| `set ... GET` | previous value or `null` |

### `ZADD`

```ts
await redis.zadd("lb", 100, "alice");                 // score, member
await redis.zadd("lb", "NX", 100, "alice");           // add only
await redis.zadd("lb", "GT", "CH", 300, "alice");     // update only if higher, count changes
await redis.zadd("lb", "INCR", 10, "alice");          // increment, returns new score
```

### `SCAN`

```ts
await redis.scan("0", "MATCH", "user:*", "COUNT", 100); // [nextCursor, keys[]]
```

### Ranges

```ts
await redis.zrange("lb", 0, 9, "WITHSCORES");            // ascending, flat array
await redis.zrange("lb", 0, 9, "REV", "WITHSCORES");     // descending (Redis 6.2+)
await redis.zrevrange("lb", 0, 9, "WITHSCORES");         // older equivalent
```

## Passing arguments

ioredis flattens arrays, so both styles work:

```ts
await redis.sadd("tags", "a", "b", "c");
await redis.sadd("tags", ["a", "b", "c"]);
await redis.del(["k1", "k2"]);
await redis.mget(["k1", "k2"]);
```

Objects and Maps work for commands that take field/value pairs:

```ts
await redis.hset("user:1", { name: "Ada", age: 36 });
await redis.hset("user:1", new Map([["city", "Hyderabad"]]));
await redis.mset({ a: 1, b: 2 });
```

Rules:

- Arguments are sent as strings. Numbers are converted for you
- **Serialize objects yourself** with `JSON.stringify` for plain string values. Passing an object to `SET` does not do what you want
- Don't pass `undefined` or `null` as values, because the outcome isn't what you expect. Validate first

```ts
await redis.set("cfg", JSON.stringify({ theme: "dark" }));
```

## Return shapes

Redis replies map to JavaScript like this:

| Redis reply | JavaScript |
|-------------|-----------|
| Simple string (`OK`) | `"OK"` |
| Bulk string | `string` (or `null` if missing) |
| Integer | `number` |
| Array | `Array` (nested arrays allowed) |
| Nil | `null` |
| Error | Promise rejects with an error |

Examples worth memorizing:

```ts
await redis.get("missing");                  // null
await redis.exists("a", "b");                // 1 or 2 (count of existing keys)
await redis.mget("a", "missing", "c");       // ["1", null, "3"]
await redis.hgetall("user:1");               // { name: "Ada", age: "36" }  (object, values are strings)
await redis.hgetall("missing");              // {}
await redis.hmget("user:1", "name", "zzz");  // ["Ada", null]
await redis.zrange("lb", 0, -1, "WITHSCORES"); // ["bob","90","alice","100"]  (flat, scores are strings)
await redis.lrange("list", 0, -1);           // ["a", "b"]
await redis.smembers("set");                 // ["x", "y"]
```

Convert types yourself:

```ts
const views = Number(await redis.get("views"));
const user = await redis.hgetall("user:1");
const age = Number(user.age);
```

### Turning a flat ZSET reply into objects

```ts
function pairs(flat: string[]) {
  const out: { member: string; score: number }[] = [];
  for (let i = 0; i < flat.length; i += 2) {
    out.push({ member: flat[i], score: Number(flat[i + 1]) });
  }
  return out;
}

const top = pairs(await redis.zrevrange("lb", 0, 9, "WITHSCORES"));
```

## Commands ioredis doesn't wrap

Use `call()` to send any command by name:

```ts
await redis.call("SET", "k", "v", "EX", "60");
await redis.call("MEMORY", "USAGE", "k");
await redis.call("JSON.SET", "doc", "$", '{"a":1}'); // module commands
```

`call` returns the raw reply. All arguments should be strings or numbers.

## Iterating keys with `scanStream`

```ts
const stream = redis.scanStream({ match: "user:*", count: 100 });

stream.on("data", (keys: string[]) => {
  // process each batch
});
stream.on("end", () => console.log("done"));
```

Or with async iteration:

```ts
for await (const keys of redis.scanStream({ match: "user:*", count: 100 })) {
  for (const key of keys) console.log(key);
}
```

Similar helpers exist: `hscanStream`, `sscanStream`, `zscanStream`. Covered in `05_key-management`.

## Pipelines and transactions (preview)

```ts
const results = await redis.pipeline().set("a", 1).incr("a").get("a").exec();
// [[null, "OK"], [null, 2], [null, "2"]]

await redis.multi().set("x", 1).incr("x").exec();
```

Full details are in `06_advanced-commands`.

## Blocking commands

```ts
const item = await blocker.blpop("queue", 5); // wait up to 5s, returns [key, value] or null
```

Use a **dedicated connection** for blocking commands so they don't stall other traffic.

## TypeScript notes

ioredis includes typings for most commands, with overloads for common option combinations:

```ts
const v: string | null = await redis.get("k");
const n: number = await redis.incr("k");
const h: Record<string, string> = await redis.hgetall("user:1");
```

For commands not typed, `call()` returns `unknown`. Narrow it yourself.

## Common mistakes

| Mistake | Result | Fix |
|---------|--------|-----|
| `redis.set("k", 60, "EX")` (wrong order) | Error or wrong behavior | `redis.set("k", "v", "EX", 60)` |
| Treating numbers as numbers | `"5" + 1 = "51"` | `Number(...)` |
| Storing objects without `JSON.stringify` | Garbage value | Serialize explicitly |
| Ignoring `null` from `get` | `TypeError` on parse | Handle missing keys |
| `set ... NX` result ignored | Thinking the lock was acquired | Check for `"OK"` |
| Using `KEYS` in production | Blocks the server | `scanStream` |

## Key takeaways

- Method name = command name, and options are trailing string tokens
- Arrays, objects and Maps are flattened for you where Redis expects multiple arguments
- Replies come back as strings, numbers, arrays or `null`, and you convert types yourself
- Use `call()` for unwrapped commands and `scanStream` for iteration

**Next:** [Promises, Callbacks and Buffers](./04_promises-callbacks-buffers.md)
