# 04 · Data Structures

Redis is a data structure server. This module goes deep on each structure: the commands, the ioredis calls, the complexity, the real-world patterns and the traps.

## Lessons

| # | Lesson | Core idea |
|---|--------|-----------|
| 01 | [Strings](./01_strings.md) | Values, counters, locks, cache entries |
| 02 | [Hashes](./02_hashes.md) | Objects with per-field access |
| 03 | [Lists](./03_lists.md) | Ordered sequences, simple queues, capped feeds |
| 04 | [Sets](./04_sets.md) | Unique members and set algebra |
| 05 | [Sorted Sets](./05_sorted-sets.md) | Ranking, time indexes, delayed jobs, sliding windows |
| 06 | [Bitmaps and Bitfields](./06_bitmaps-and-bitfields.md) | Compact flags and packed counters |
| 07 | [HyperLogLog and Geo](./07_hyperloglog-and-geo.md) | Approximate unique counts, location queries |
| 08 | [Streams Overview](./08_streams-overview.md) | Append-only logs (deep dive in module 10) |
| 09 | [Choosing the Right Structure](./09_choosing-the-right-structure.md) | Decision guide and modeling examples |

## Learning outcomes

After this module you can:

- Read and write every core Redis type from ioredis
- Predict the cost (Big-O and memory) of the commands you use
- Model real features (profiles, feeds, leaderboards, presence) with the right structure
- Avoid the classic traps (`HGETALL` on huge hashes, `SMEMBERS` on big sets, lists as reliable queues)

## How each lesson is organized

1. What it is and when to use it
2. Command tour with ioredis code
3. Complexity notes
4. Patterns
5. Pitfalls

## Conventions in the examples

```ts
import { Redis } from "ioredis";
const redis = new Redis();
```

Every example assumes this client, the key naming from [02_redis-fundamentals](../02_redis-fundamentals/01_keys-and-values.md), and Redis 7 or newer. Where a command needs a newer version, the lesson says so.

## Prerequisites

- Completed [03_ioredis-basics](../03_ioredis-basics/README.md)

## Next

Continue to [05_key-management](../05_key-management/README.md).
