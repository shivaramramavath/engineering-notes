# Choosing the Right Structure

The right structure makes a feature trivial. The wrong one makes it slow, expensive or buggy. Choose by **access pattern**, not by what the data looks like.

## Five questions to ask

1. **How will I read it?** Whole value, one field, a range, a rank, membership?
2. **How will I write it?** Replace, append, increment, update one part?
3. **Do I need order?** By insertion, by score, by time?
4. **Do I need uniqueness?** Members, events, IDs?
5. **How big can it get?** Is it bounded? What is the worst-case size?

## Decision tree

```
Single opaque value, read/written as a whole?
├─ yes ─► STRING  (cache blob, counter, flag, lock)
└─ no
   ├─ Object with named fields, update fields independently?
   │  └─ HASH
   ├─ Need unique items?
   │  ├─ Need ordering or ranking by a number/time? ─► SORTED SET
   │  ├─ Only membership / set algebra?             ─► SET
   │  ├─ Only a count of uniques, approximate is OK ─► HYPERLOGLOG
   │  └─ Dense integer IDs, true/false per ID       ─► BITMAP
   ├─ Ordered sequence?
   │  ├─ Simple queue / recent items                ─► LIST
   │  └─ Needs replay, acks, multiple consumers     ─► STREAM
   ├─ Location queries ("near me")                  ─► GEO
   └─ Many tiny fixed-width counters                ─► BITFIELD
```

## Requirement to structure

| I need to... | Use | Why |
|--------------|-----|-----|
| Cache an API response | String (JSON) | Read whole, single TTL |
| Store a user profile with partial updates | Hash | Per-field access, `HINCRBY` |
| Count page views | String (`INCR`) or Hash field | Atomic increment |
| Rate limit per user (simple) | String (`INCR` + `EXPIRE`) | Fixed window |
| Rate limit (sliding window) | Sorted set | Timestamps as scores |
| Leaderboard | Sorted set | Rank and range in O(log N) |
| Timeline / activity feed | Sorted set (or capped list) | Time-ordered, range by time |
| Tags, categories, followers | Set | Membership and intersection |
| Unique visitors (millions, approximate) | HyperLogLog | 12 KB |
| Unique visitors (exact) | Set or bitmap | Exactness |
| Daily active flags for numeric IDs | Bitmap | 1 bit per user |
| Simple job queue | List | Fast push/pop |
| Reliable job queue with retries | Stream or BullMQ | Acks, redelivery |
| Delayed / scheduled jobs | Sorted set | Score = run time |
| Priority queue | Sorted set | Lowest score first |
| Event log with replay | Stream | Persistent, IDs |
| Notification broadcast (ephemeral) | Pub/Sub | Fire and forget |
| Nearby stores or drivers | Geo | `GEOSEARCH` |
| Distributed lock | String (`SET NX PX`) | Atomic acquire with TTL |
| Session | Hash or string with TTL | Depends on field access |
| Feature flags | String or Hash | Small and simple |
| Presence with expiry | Sorted set (last-seen score) | Trim by time |
| Autocomplete | Sorted set (`BYLEX`) | Prefix ranges |
| Deduplicate events | Set (`SADD`) or `SET NX EX` | Atomic first-time check |

## Worked example: a social app

| Feature | Structure | Key example |
|---------|-----------|-------------|
| Profile | Hash | `user:{id}` |
| Session | Hash + TTL | `session:{sid}` |
| Followers / following | Sets | `followers:{id}`, `following:{id}` |
| Home timeline | Sorted set (score = timestamp) | `timeline:{id}` |
| Post like count | Hash field or string | `post:{id}:stats` |
| Who liked a post | Set | `post:{id}:likers` |
| Unique post viewers | HyperLogLog | `post:{id}:viewers` |
| Trending posts | Sorted set (score = weighted engagement) | `trending:24h` |
| Daily active users | Bitmap | `dau:{yyyy-mm-dd}` |
| Nearby users | Geo | `users:geo` |
| Notifications feed | Stream or capped list | `notifs:{id}` |
| Rate limit (API) | Sorted set or string | `rate:{ip}` |
| Job queue (emails, thumbnails) | BullMQ (lists, sorted sets, streams inside) | managed |

## Modeling techniques

### Object plus index

Hashes hold the data, sets or sorted sets hold **indexes** to find it:

```ts
await redis.multi()
  .hset("product:88", { name: "Keyboard", price: 4999, brand: "acme" })
  .sadd("idx:brand:acme", 88)
  .zadd("idx:price", 4999, 88)
  .exec();

// products by brand within a price range: intersect the two indexes
await redis.zrange("idx:price", 2000, 6000, "BYSCORE");     // ids by price
await redis.smembers("idx:brand:acme");                     // ids by brand
```

You maintain indexes yourself. Redis is a data structure server, not a query engine. Update the object and its indexes atomically with `multi()`.

### Bounded by design

Every growing structure needs a bound:

| Structure | Bounding tool |
|-----------|---------------|
| List | `LTRIM` |
| Sorted set | `ZREMRANGEBYRANK`, `ZREMRANGEBYSCORE` |
| Stream | `MAXLEN ~`, `MINID ~` |
| Any key | `EXPIRE` |

### Bucketing large collections

Instead of one giant set or hash, split by time or hash:

```
visitors:2026-09-30        instead of   visitors:all
stats:post:42:2026-09      instead of   stats:post:42:forever
```

### Composite scores

Encode two criteria into one score (for example rank by points, break ties by earlier time):

```ts
const score = points * 1e6 + (1e6 - (Date.now() % 1e6));   // keep below 2^53
```

Verify the range fits in 2^53 (about 9e15) to keep integer precision.

## Common anti-patterns

| Anti-pattern | Why it's bad | Better |
|--------------|--------------|--------|
| Everything as JSON strings | Full rewrites, no atomic field ops | Hashes, counters |
| A list used as a durable queue | Lost jobs on crash | Streams or BullMQ |
| `SMEMBERS` / `HGETALL` on huge keys | O(N), blocks the server | `SSCAN` / `HSCAN`, or shard |
| A set for presence | No per-member TTL | Sorted set by last-seen |
| One giant key for a whole dataset | Hot key, slow deletes | Bucketed keys |
| Pub/Sub for must-deliver messages | Messages lost if nobody listens | Streams |
| Bitmap on sparse or huge IDs | Huge allocations | Dense mapping or a set |
| No TTL or trim on growing data | Memory leak | Always bound |

## Complexity quick reference

| Structure | Fast (O(1) / O(log N)) | Watch out (O(N)) |
|-----------|------------------------|------------------|
| String | `GET`, `SET`, `INCR` | `GETRANGE` on huge values |
| Hash | `HGET`, `HSET`, `HINCRBY` | `HGETALL`, `HKEYS` |
| List | `LPUSH`, `RPOP`, `LLEN` | `LINDEX` mid-list, `LREM`, `LRANGE` big |
| Set | `SADD`, `SISMEMBER` | `SMEMBERS`, big `SINTER` |
| Sorted set | `ZADD`, `ZRANK`, `ZRANGE` (small M) | Huge `ZRANGE`, `ZUNIONSTORE` |
| Bitmap | `SETBIT`, `GETBIT` | `BITOP` on huge strings |
| HyperLogLog | `PFADD`, `PFCOUNT` | Many-key `PFCOUNT` |
| Geo | `GEOADD`, `GEOSEARCH` (bounded) | Huge radius without `COUNT` |
| Stream | `XADD`, `XACK`, `XREADGROUP` | `XRANGE` of everything |

## Beyond core types

Redis also offers optional modules (JSON, Search, Time Series, probabilistic structures such as Bloom filters). They are outside this course, but consider them when you need document queries, full-text search or time-series analytics. Check availability on your server or provider first.

## Summary checklist

- [ ] I named the read and write patterns before choosing the type
- [ ] The structure matches the pattern (one field → hash, rank → sorted set, ...)
- [ ] Every growing structure has a TTL or a trim
- [ ] No O(N) command runs on unbounded data in a request path
- [ ] Indexes are updated atomically with the data they point to
- [ ] Multi-key operations use hash tags if I might move to Cluster

## Key takeaways

- Pick by **access pattern**: whole value → string, fields → hash, unique → set, ranked → sorted set, queue → list/stream
- Sorted sets solve a surprising range of problems, so learn them well
- Bound everything, and avoid O(N) commands on big keys
- Build indexes deliberately and update them atomically

**Next module:** [05_key-management](../05_key-management/README.md)
