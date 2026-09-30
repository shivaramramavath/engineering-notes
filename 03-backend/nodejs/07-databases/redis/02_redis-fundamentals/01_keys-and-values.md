# Keys and Values

Everything in Redis is a **key** pointing to a **value**. The key is always a string. The value can be a string or one of several data structures.

```
key  ─────►  value (string | hash | list | set | sorted set | stream | ...)
"user:1"     { name: "Ada", age: "36" }
```

## Key rules

- Keys are **binary safe**: any byte sequence works, including an empty string or a JPEG
- Maximum key size is **512 MB** (never get anywhere near this)
- Each key holds **exactly one type**. Writing a different type to an existing key needs the old one deleted (or overwritten by `SET`)
- Key lookup is O(1) on average

## Key size guidance

Very long keys cost memory and slow comparisons. Very short cryptic keys hurt readability. Aim for **readable and reasonably short**.

| Key                                                 | Verdict     |
| --------------------------------------------------- | ----------- |
| `u1000flw`                                          | Too cryptic |
| `user:1000:followers`                               | Good        |
| `the-user-with-id-1000-and-their-list-of-followers` | Wasteful    |

## Naming conventions

Use a **colon-separated hierarchy**, most general to most specific:

```
<app>:<entity>:<id>:<attribute>
```

Examples:

```
shop:user:1042
shop:user:1042:cart
shop:session:9f8a7c
cache:product:88:v2
lock:order:5501
rate:ip:203.0.113.7:2026-09-29T15:04
```

Rules of thumb:

1. Use **lowercase** and `:` as the separator
2. Put the **entity type first** so keys group together and scan well
3. Include the **id** as its own segment
4. Add a **version** to cache keys you may reshape (`cache:product:88:v2`)
5. Prefix by **purpose** (`cache:`, `lock:`, `rate:`, `session:`) so eviction, monitoring and cleanup are easy
6. Never build keys from raw user input without validating it (avoid key injection such as `user:` + `"1:admin"`)

## Key builder basics

Centralize key construction so names never drift between files:

```ts
// src/redis/keys.ts
const PREFIX = "shop";

export const keys = {
  user: (id: string | number) => `${PREFIX}:user:${id}`,
  userCart: (id: string | number) => `${PREFIX}:user:${id}:cart`,
  session: (sid: string) => `${PREFIX}:session:${sid}`,
  productCache: (id: string | number, v = 2) =>
    `${PREFIX}:cache:product:${id}:v${v}`,
  lock: (resource: string) => `${PREFIX}:lock:${resource}`,
};
```

```ts
await redis.hset(keys.user(1042), { name: "Ada", plan: "pro" });
```

A fuller version is in `08_nodejs-integration/05_redis-key-builder.md`.

## ioredis `keyPrefix`

ioredis can prepend a prefix to every key automatically:

```ts
const redis = new Redis({ keyPrefix: "shop:" });

await redis.set("user:1", "Ada"); // stored as "shop:user:1"
await redis.get("user:1"); // reads "shop:user:1"
```

Caveats:

- The prefix is applied to key arguments of commands, but **not** to results. `redis.keys("*")` still returns full names
- `SCAN` patterns and Pub/Sub channels are **not** prefixed automatically
- Lua scripts need care: keys passed via `KEYS[]` are prefixed, keys built inside the script are not

## Values and strings

A Redis string is binary safe and up to **512 MB**. Numbers, JSON, serialized objects and raw bytes are all just strings.

```ts
await redis.set("count", 10);
await redis.incr("count"); // 11 (Redis interprets the string as an integer)

await redis.set("cfg", JSON.stringify({ theme: "dark" }));
const cfg = JSON.parse((await redis.get("cfg")) ?? "{}");
```

Note that ioredis returns **strings** (or `null` for missing keys). Convert numbers yourself: `Number(await redis.get("count"))`.

## Working with keys

```ts
await redis.exists("user:1"); // 1 or 0 (can take several keys and returns a count)
await redis.type("user:1"); // "string" | "hash" | "list" | "set" | "zset" | "stream"
await redis.rename("old", "new"); // overwrites "new" if it exists
await redis.renamenx("old", "new"); // only if "new" does not exist
await redis.del("a", "b"); // delete (blocking for big values)
await redis.unlink("a", "b"); // delete, frees memory in the background
await redis.copy("src", "dst"); // Redis 6.2+
```

Never use `KEYS pattern` in production. Use `SCAN` (covered in `05_key-management`).

## Logical databases

Redis has **16 logical databases** by default, numbered 0 to 15:

```ts
const cacheDb = new Redis({ db: 1 });
```

- Same server and memory, separate keyspaces
- `FLUSHDB` clears one, `FLUSHALL` clears all
- **Redis Cluster supports only database 0**

For isolation in real projects, prefer **key prefixes or separate instances** over multiple databases.

## Key design pitfalls

| Pitfall                                     | Why it hurts                          | Better                           |
| ------------------------------------------- | ------------------------------------- | -------------------------------- |
| No naming pattern                           | Impossible to scan, audit or clean up | Consistent hierarchy             |
| Unbounded key growth (`log:<uuid>` forever) | Memory leak                           | TTL or capped structures         |
| Huge values under one key                   | Blocks the thread, slow replication   | Split, or use a hash             |
| User input inside keys                      | Injection, collisions                 | Validate or hash it              |
| Different cases (`User:1` vs `user:1`)      | Two separate keys                     | Enforce lowercase in the builder |

## Key takeaways

- Keys are binary-safe strings, up to 512 MB, looked up in O(1)
- Use `entity:id:attribute` naming and a shared key-builder
- `keyPrefix` helps but has limits (SCAN, Pub/Sub, Lua)
- Prefer `SCAN` over `KEYS`, and `UNLINK` over `DEL` for large values

**Next:** [Data Types Overview](./02_data-types-overview.md)
