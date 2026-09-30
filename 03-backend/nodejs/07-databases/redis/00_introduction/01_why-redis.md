# Why Redis?

**Redis** (REmote DIctionary Server) is an in-memory data structure server. You store data under keys, and each value is a rich structure (string, hash, list, set, sorted set, stream, ...) with atomic operations built in.

## The problem it solves

Traditional databases keep data on disk and are optimized for durability and complex queries. That is a poor fit for workloads that need:

- **Sub-millisecond latency** (session lookups, rate-limit checks)
- **Very high throughput** (hundreds of thousands of ops/sec on one node)
- **Shared state across many app instances** (in-process memory is not shared)
- **Temporary data with automatic expiry** (OTPs, cache entries, locks)

Redis answers these by keeping the working set in RAM and exposing purpose-built data structures instead of a generic query language.

## Key strengths

| Strength                   | Why it matters                                                                              |
| -------------------------- | ------------------------------------------------------------------------------------------- |
| Speed                      | In-memory storage, simple protocol, no query planner                                        |
| Data structures            | Sorted sets for leaderboards, streams for events, HyperLogLog for counting, all server-side |
| Atomic operations          | `INCR`, `SET NX`, Lua scripts and transactions avoid race conditions                        |
| Built-in expiry            | `EXPIRE` / `SET ... EX` makes TTL a first-class feature                                     |
| Pub/Sub and Streams        | Lightweight messaging without another broker                                                |
| Persistence options        | RDB snapshots and AOF logs let data survive restarts                                        |
| Replication and clustering | Read scaling, high availability, horizontal sharding                                        |
| Simplicity                 | Small command set, easy to reason about                                                     |

## Common use cases

1. **Caching**: database query results, API responses, rendered fragments
2. **Session storage**: shared across stateless Node.js instances
3. **Rate limiting**: counters and sliding windows per user or IP
4. **Distributed locks**: coordinate work across processes
5. **Queues and background jobs**: BullMQ is built on Redis
6. **Real-time messaging**: Pub/Sub, Socket.IO adapter, Streams
7. **Leaderboards and rankings**: sorted sets
8. **Counters and analytics**: page views, unique visitors (HyperLogLog)
9. **Feature flags and config**: fast reads, instant updates
10. **Idempotency keys and deduplication**: `SET key value NX EX`

## A first taste with ioredis

```js
import Redis from "ioredis";

const redis = new Redis(); // localhost:6379

await redis.set("greeting", "hello", "EX", 60); // expires in 60s
console.log(await redis.get("greeting")); // "hello"

await redis.incr("page:views"); // atomic counter
await redis.zadd("leaderboard", 1500, "alice"); // sorted set
console.log(await redis.zrevrange("leaderboard", 0, 2, "WITHSCORES"));

await redis.quit();
```

## Why ioredis for Node.js?

- Full-featured, promise-based client with TypeScript typings
- Built-in support for Cluster, Sentinel, pipelining, Lua scripting and Streams
- Automatic reconnection with configurable strategy
- Used under the hood by BullMQ

## Key takeaways

- Redis is a **data structure server**, not just a cache.
- Its value comes from **speed + the right structure + atomicity**.
- It usually **complements** your primary database rather than replacing it.

**Next:** [Redis vs Alternatives](./02_redis-vs-alternatives.md)
