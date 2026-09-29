# Data Types Overview

Redis is a **data structure server**. Choosing the right structure is the biggest lever for performance and simplicity. This page is a map. Each type gets its own deep-dive in `04_data-structures`.

## At a glance

| Type                  | What it is                                 | Typical use                                                |
| --------------------- | ------------------------------------------ | ---------------------------------------------------------- |
| **String**            | Binary-safe value up to 512 MB             | Cache, counters, locks, flags                              |
| **Hash**              | Field → value map                          | Objects (user profile), small records                      |
| **List**              | Ordered sequence (linked list)             | Queues, recent activity feeds                              |
| **Set**               | Unordered unique members                   | Tags, unique visitors, relationships                       |
| **Sorted Set (ZSet)** | Unique members ordered by score            | Leaderboards, rankings, time-ordered indexes, rate windows |
| **Stream**            | Append-only log with consumer groups       | Event pipelines, job processing                            |
| **Bitmap**            | Bit operations on a string                 | Daily active flags, feature bits                           |
| **Bitfield**          | Multiple integer fields packed in a string | Compact counters                                           |
| **HyperLogLog**       | Probabilistic unique counter (~12 KB)      | Unique visitors at huge scale                              |
| **Geospatial**        | Longitude/latitude indexed in a sorted set | "Nearby" searches                                          |

(Pub/Sub channels are a messaging feature, not a stored type.)

## Quick ioredis tour

### String

```ts
await redis.set("greeting", "hello");
await redis.get("greeting");
await redis.incr("views"); // atomic counter
await redis.incrby("views", 10);
await redis.set("lock", "1", "EX", 30, "NX"); // only if not exists
```

### Hash

```ts
await redis.hset("user:1", { name: "Ada", age: 36 });
await redis.hget("user:1", "name"); // "Ada"
await redis.hgetall("user:1"); // { name: "Ada", age: "36" }
await redis.hincrby("user:1", "age", 1);
```

### List

```ts
await redis.lpush("tasks", "a", "b"); // push left
await redis.rpush("tasks", "c"); // push right
await redis.lrange("tasks", 0, -1); // all items
await redis.rpop("tasks"); // pop right
await redis.ltrim("recent", 0, 99); // keep newest 100
```

### Set

```ts
await redis.sadd("tags", "redis", "nodejs");
await redis.sismember("tags", "redis"); // 1
await redis.smembers("tags");
await redis.sinter("tags:a", "tags:b"); // intersection
```

### Sorted set

```ts
await redis.zadd("leaderboard", 1500, "alice", 2100, "bob");
await redis.zincrby("leaderboard", 50, "alice");
await redis.zrevrange("leaderboard", 0, 9, "WITHSCORES"); // top 10
await redis.zrevrank("leaderboard", "alice"); // 0-based rank
```

### Stream

```ts
await redis.xadd("events", "*", "type", "signup", "user", "1");
await redis.xrange("events", "-", "+");
```

### Bitmap, HyperLogLog, Geo

```ts
await redis.setbit("active:2026-09-29", 1042, 1); // user 1042 was active
await redis.bitcount("active:2026-09-29");

await redis.pfadd("visitors", "u1", "u2", "u1");
await redis.pfcount("visitors"); // ~2

await redis.geoadd("stores", 78.4867, 17.385, "hyderabad");
await redis.geosearch("stores", "FROMLONLAT", 78.5, 17.4, "BYRADIUS", 50, "km");
```

## Complexity cheat sheet

| Operation                                         | Typical cost                      |
| ------------------------------------------------- | --------------------------------- |
| `GET`, `SET`, `HGET`, `HSET`, `SADD`, `SISMEMBER` | O(1)                              |
| `LPUSH`, `RPUSH`, `LPOP`, `RPOP`                  | O(1)                              |
| `LINDEX`, `LRANGE`                                | O(N) (N = elements touched)       |
| `ZADD`, `ZINCRBY`, `ZRANK`                        | O(log N)                          |
| `ZRANGE`                                          | O(log N + M)                      |
| `SMEMBERS`, `HGETALL`, `LRANGE 0 -1`              | O(N), **dangerous on large keys** |
| `SINTER`, `SUNION`                                | Can be expensive on large sets    |

## Choosing a type: quick guide

| I need to...                                | Use                              |
| ------------------------------------------- | -------------------------------- |
| Cache a blob or JSON                        | String                           |
| Store an object and update single fields    | Hash                             |
| Build a simple queue or a recent-items list | List (or Stream for reliability) |
| Track unique items or membership            | Set                              |
| Rank or order by a number or timestamp      | Sorted set                       |
| Process events with acknowledgements        | Stream                           |
| Count uniques approximately, cheaply        | HyperLogLog                      |
| Track true/false per numeric id             | Bitmap                           |
| Find things near a location                 | Geo                              |

## Encodings (why small data is cheap)

Redis stores small collections in compact encodings (for example `listpack` or `intset`) and converts them to full structures as they grow. You can see the current one:

```ts
await redis.object("ENCODING", "user:1"); // e.g. "listpack" or "hashtable"
```

This is one reason **many small hashes** can use far less memory than **many separate string keys**.

## Type rules

- A command works only on its matching type. Otherwise you get `WRONGTYPE Operation against a key holding the wrong kind of value`
- Empty aggregates (a list with no items, an empty set) are **removed automatically**
- Reading a missing key returns `null` (or an empty collection for aggregate read commands)

```ts
await redis.set("k", "v");
await redis.lpush("k", "x"); // throws WRONGTYPE
```

## Key takeaways

- Pick the type based on the **access pattern**, not just the data shape
- Hashes beat many tiny string keys for objects
- Sorted sets are the most versatile tool for ranking and time windows
- Watch O(N) commands (`HGETALL`, `SMEMBERS`) on large keys

**Next:** [Expiration and TTL](./03_expiration-and-ttl.md)
