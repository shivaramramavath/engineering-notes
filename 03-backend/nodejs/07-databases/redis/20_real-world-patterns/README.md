# 20 Real-World Patterns

The earlier chapters taught the building blocks: data structures, atomic commands, Lua, Pub/Sub, streams, locks and queues. This chapter shows how they combine into the features teams actually ship: safe retries, duplicate suppression, rankings, notifications, feature flags and event-driven workflows.

Each pattern follows the same shape, so you can skim to the part you need:

```
Problem ─► Design (keys, structures, commands) ─► Implementation (ioredis + TypeScript)
        ─► Edge cases and failure modes ─► Testing ─► Pitfalls
```

## What you will learn

- Make retried requests safe with idempotency keys
- Suppress duplicate requests, events and alerts without losing real ones
- Build leaderboards with sorted sets, including ties, time windows and friends views
- Design an inbox, unread counts, real-time delivery and channel fan-out
- Evaluate feature flags in memory with sticky percentage rollouts
- Build an event-driven system on Streams with consumer groups, retries and dead letters

## Contents

| # | File | Core Redis tools | Key idea |
|---|------|------------------|----------|
| 01 | [Idempotency](./01_idempotency.md) | `SET NX`, hashes, Lua, TTL | Same key and same request returns the same result, side effects run once |
| 02 | [Request Deduplication](./02_request-deduplication.md) | `SET NX EX`, sets, windows | Drop repeats inside a time window, claim before processing |
| 03 | [Leaderboard](./03_leaderboard.md) | Sorted sets, `ZINCRBY`, `ZUNIONSTORE` | Rank in O(log N), tie-breaks and windows by key design |
| 04 | [Notification System](./04_notification-system.md) | Sorted sets, sets, Pub/Sub, BullMQ | Store once, deliver many ways, never block the request |
| 05 | [Feature Flags](./05_feature-flags.md) | Hashes, Pub/Sub, streams, HyperLogLog | Evaluate from memory, change through Redis |
| 06 | [Event-Driven System](./06_event-driven-system.md) | Streams, consumer groups, `XAUTOCLAIM` | At-least-once delivery plus idempotent consumers |

## Which pattern for which problem?

| You need to... | Reach for |
|----------------|-----------|
| Stop a retry from charging a card twice | [Idempotency](./01_idempotency.md) |
| Ignore a webhook that was delivered three times | [Request Deduplication](./02_request-deduplication.md) |
| Collapse a flood of identical alerts into one | [Request Deduplication](./02_request-deduplication.md) |
| Show top players, my rank and the people around me | [Leaderboard](./03_leaderboard.md) |
| Give each user an inbox with an unread badge | [Notification System](./04_notification-system.md) |
| Roll a feature out to 5 percent, then 50, then everyone | [Feature Flags](./05_feature-flags.md) |
| Let several services react to the same business event | [Event-Driven System](./06_event-driven-system.md) |

### Idempotency vs deduplication

They look similar and solve different problems:

| | Idempotency | Deduplication |
|--|-------------|---------------|
| Goal | A repeat returns the **same result** as the first call | A repeat is **ignored** |
| Caller expects | The original response | Usually nothing (an acknowledgement) |
| Typical place | Client-facing APIs (payments, orders) | Consumers, webhooks, alerts, job intake |
| State stored | In-progress marker plus the response | A "seen" marker |

## Themes that repeat

Every pattern here leans on the same few ideas. Learn them once:

1. **Atomicity.** One command (`SET NX`, `ZINCRBY`) or one Lua script, never read then write
2. **Claim, then do the work.** The first caller wins a short-lived claim, the rest back off
3. **Bounded state.** Every key has a TTL or a size cap
4. **At-least-once is the norm.** Networks, queues and consumers repeat work, so make handlers safe to repeat
5. **Redis is a fast layer, not always the truth.** Keep a durable source of truth (a database unique constraint, an outbox) behind anything that matters
6. **Degrade on purpose.** Decide per feature what happens when Redis is slow or gone: fail open (flags, caches) or fail closed (payments)
7. **Test with concurrency.** Fire many callers at once and assert the outcome

## Prerequisites

- [Strings](../04_data-structures/01_strings.md), [Hashes](../04_data-structures/02_hashes.md), [Sorted Sets](../04_data-structures/05_sorted-sets.md) and [Streams Overview](../04_data-structures/08_streams-overview.md)
- [Lua Scripts](../06_advanced-commands/03_lua-scripts.md) and [Atomic and Conditional Ops](../06_advanced-commands/04_atomic-and-conditional-ops.md)
- [Distributed Locks](../11_distributed-locks/01_locking-fundamentals.md) and [Queues and Workers](../14_queues-and-workers/01_queue-fundamentals.md)
- [Testing with Redis](../18_testing-and-debugging/01_testing-with-redis.md) for the test helpers used in the examples

The examples use `ioredis` with TypeScript. Where a command needs a recent Redis version, the text says so.

**Start:** [Idempotency](./01_idempotency.md)
