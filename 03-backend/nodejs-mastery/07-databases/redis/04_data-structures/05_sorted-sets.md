# Sorted Sets

A sorted set (ZSET) is a **set of unique members, each with a numeric score**. Members are kept ordered by score, and ties are ordered lexicographically. Lookups, inserts and rank queries are O(log N).

```
leaderboard  ─►  bob:90 ─ alice:100 ─ cy:250 ─ dee:300       (ascending by score)
```

It is the most versatile Redis structure: leaderboards, time-ordered indexes, priority and delayed queues, sliding windows and more.

## Command tour

```ts
await redis.zadd("lb", 100, "alice", 90, "bob");     // score first, then member
await redis.zscore("lb", "alice");                   // "100" (a string)
await redis.zmscore("lb", "alice", "zzz");           // ["100", null]  (Redis 6.2+)
await redis.zincrby("lb", 25, "alice");              // atomic, returns new score
await redis.zcard("lb");                             // member count
await redis.zcount("lb", 50, 150);                   // members with score in [50,150]
await redis.zrank("lb", "alice");                    // 0-based rank, ascending
await redis.zrevrank("lb", "alice");                 // 0-based rank, descending
await redis.zrem("lb", "bob");                       // remove
```

### `ZADD` flags

```ts
await redis.zadd("lb", "NX", 10, "new");             // only add, never update
await redis.zadd("lb", "XX", 10, "existing");        // only update existing
await redis.zadd("lb", "GT", 500, "alice");          // update only if the new score is greater
await redis.zadd("lb", "LT", 5, "alice");            // update only if the new score is lower
await redis.zadd("lb", "CH", 500, "alice");          // return changed count instead of added count
await redis.zadd("lb", "INCR", 10, "alice");         // like ZINCRBY, returns the new score
```

`GT` and `LT` (Redis 6.2+) are handy for "keep the best score".

### Ranges

`ZRANGE` is the unified command since Redis 6.2:

```ts
await redis.zrange("lb", 0, 9);                                  // by rank, ascending
await redis.zrange("lb", 0, 9, "REV");                           // by rank, descending
await redis.zrange("lb", 0, 9, "WITHSCORES");                    // flat [member, score, ...]
await redis.zrange("lb", 50, 150, "BYSCORE");                    // by score range
await redis.zrange("lb", "-inf", "+inf", "BYSCORE", "LIMIT", 0, 10);
await redis.zrange("lb", "(50", "150", "BYSCORE");               // "(" makes the bound exclusive
await redis.zrange("names", "[a", "[c", "BYLEX");                // lexicographic (equal scores)
```

Older commands (`ZREVRANGE`, `ZRANGEBYSCORE`, `ZREVRANGEBYSCORE`) still work.

### Removing and popping

```ts
await redis.zremrangebyrank("lb", 0, -101);          // keep only the top 100
await redis.zremrangebyscore("lb", 0, cutoff);       // remove by score range
await redis.zpopmin("lb");                           // remove and return lowest [member, score]
await redis.zpopmax("lb", 3);                        // remove and return highest 3
await redis.bzpopmin("queue", 5);                    // blocking pop (dedicated connection)
```

### Combining

```ts
await redis.zunionstore("dest", 2, "lb:week1", "lb:week2");                  // sum scores
await redis.zunionstore("dest", 2, "a", "b", "WEIGHTS", 1, 2, "AGGREGATE", "MAX");
await redis.zinterstore("dest", 2, "a", "b");
```

## Scores

- Scores are **64-bit doubles**. Integers are exact up to 2^53
- `+inf` and `-inf` are valid
- Timestamps in milliseconds fit comfortably
- Returned scores are **strings**, so convert with `Number()`

## Helper: flat reply to objects

```ts
function toEntries(flat: string[]) {
  const out: { member: string; score: number }[] = [];
  for (let i = 0; i < flat.length; i += 2) {
    out.push({ member: flat[i], score: Number(flat[i + 1]) });
  }
  return out;
}
```

## Patterns

### 1. Leaderboard

```ts
const LB = "lb:global";

// keep each player's best score
await redis.zadd(LB, "GT", score, playerId);

// top 10
const top = toEntries(await redis.zrange(LB, 0, 9, "REV", "WITHSCORES"));

// my rank (1-based)
const r = await redis.zrevrank(LB, playerId);
const myRank = r === null ? null : r + 1;

// players around me
if (r !== null) {
  const around = toEntries(
    await redis.zrange(LB, Math.max(0, r - 2), r + 2, "REV", "WITHSCORES")
  );
}
```

For a running total instead of a best score, use `ZINCRBY`.

### 2. Time-ordered index (timeline)

```ts
await redis.zadd(`timeline:${userId}`, Date.now(), postId);

// newest 20
const latest = await redis.zrange(`timeline:${userId}`, 0, 19, "REV");

// posts in the last hour
await redis.zrange(`timeline:${userId}`, Date.now() - 3_600_000, "+inf", "BYSCORE");

// cap size
await redis.zremrangebyrank(`timeline:${userId}`, 0, -1001);   // keep newest 1000
```

### 3. Sliding-window rate limiter

```ts
import { randomUUID } from "node:crypto";

async function hit(userId: string, limit: number, windowMs: number) {
  const key = `rate:${userId}`;
  const now = Date.now();

  const res = await redis.multi()
    .zremrangebyscore(key, 0, now - windowMs)      // drop old hits
    .zadd(key, now, `${now}:${randomUUID()}`)      // record this hit
    .zcard(key)                                    // count in window
    .pexpire(key, windowMs)
    .exec();

  const count = res![2][1] as number;
  return count <= limit;
}
```

This counts rejected requests too. The Lua version in module 12 is more precise.

### 4. Delayed jobs

```ts
// schedule: score = when it should run
await redis.zadd("jobs:delayed", Date.now() + 60_000, jobId);

// poll for due jobs
const due = await redis.zrange("jobs:delayed", "-inf", Date.now(), "BYSCORE", "LIMIT", 0, 10);

for (const id of due) {
  if ((await redis.zrem("jobs:delayed", id)) === 1) {
    // we won the race, this worker owns the job
    await run(id);
  }
}
```

`ZREM` returns `1` only for the client that actually removed it, which makes it a safe claim. See module 14 for a robust version.

### 5. Priority queue

```ts
await redis.zadd("pq", 1, "urgent-job");             // lower score = higher priority
await redis.zadd("pq", 5, "normal-job");
const next = await redis.zpopmin("pq");              // ["urgent-job", "1"]
```

### 6. Presence with expiry

```ts
await redis.zadd("online", Date.now(), userId);                        // heartbeat
await redis.zremrangebyscore("online", 0, Date.now() - 60_000);        // drop stale users
const onlineCount = await redis.zcard("online");
```

### 7. Autocomplete (lexicographic)

Give every member the **same score** (for example `0`) and use `BYLEX`:

```ts
await redis.zadd("ac", 0, "redis", 0, "redisson", 0, "react");
await redis.zrange("ac", "[red", "[red\xff", "BYLEX");     // ["redis", "redisson"]
```

## Encoding and memory

Small sorted sets use a compact **listpack**, and larger ones use a **skiplist + hash table** (extra memory for O(log N) operations and O(1) score lookups).

## Complexity

| Command | Cost |
|---------|------|
| `ZADD`, `ZREM`, `ZINCRBY`, `ZRANK` | O(log N) |
| `ZSCORE`, `ZCARD` | O(1) |
| `ZRANGE` (any mode) | O(log N + M) (M = returned) |
| `ZCOUNT` | O(log N) |
| `ZREMRANGEBYRANK`, `ZREMRANGEBYSCORE` | O(log N + M) |
| `ZUNIONSTORE`, `ZINTERSTORE` | O(N·K) + O(M log M), can be heavy |

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| Reading scores as numbers | They are strings, so use `Number()` |
| Argument order (`zadd key score member`) | Score comes **before** member |
| Unbounded ZSETs (timelines, windows) | Trim by rank or score |
| Precision loss with huge integer scores | Keep scores below 2^53 |
| Race conditions when polling delayed jobs | Claim with `ZREM` result |
| `ZUNIONSTORE` on giant sets in request paths | Precompute and cache |

## Key takeaways

- Sorted sets = unique members ordered by a numeric score, all O(log N)
- Use `ZADD GT` to keep best scores and `ZINCRBY` for totals
- Timestamps as scores unlock timelines, delays and sliding windows
- Always trim, and remember scores come back as strings

**Next:** [Bitmaps and Bitfields](./06_bitmaps-and-bitfields.md)
