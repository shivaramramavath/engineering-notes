# 11 · Distributed Locks

A distributed lock makes sure **only one process at a time** does something, across many servers. It sounds simple (`SET key NX`) and is full of traps: expiry while still working, failover, clock assumptions, and the difference between "usually works" and "provably safe".

This module teaches you to build locks that are **good enough for the job**, and to know when "good enough" is not enough.

## Lessons

| # | Lesson | Core idea |
|---|--------|-----------|
| 01 | [Locking Fundamentals](./01_locking-fundamentals.md) | `SET NX PX`, token ownership, safe release, retries, "run once" jobs |
| 02 | [Lock Expiration and Renewal](./02_lock-expiration-and-renewal.md) | Leases, watchdog renewal, abort signals, fencing tokens, leader election |
| 03 | [Redlock](./03_redlock.md) | Multi-node locking, the safety debate, and what to use instead |

## Learning outcomes

After this module you can:

- Acquire and release a Redis lock safely (token plus compare-and-delete)
- Choose TTLs, retry strategies and failure policies deliberately
- Keep long-running work protected with renewal, and stop work when the lock is lost
- Use **fencing tokens** so a stale lock holder can't corrupt data
- Explain what Redlock does, what it doesn't, and when to pick something else

## First question: do you really need a lock?

Locks are the **last** tool to reach for. Check these first:

| Instead of a lock | When |
|-------------------|------|
| A single atomic command (`INCR`, `SET NX`, `ZADD GT`) | One-step check-and-write ([Atomic Ops](../06_advanced-commands/04_atomic-and-conditional-ops.md)) |
| A Lua script | Read, decide, write on Redis data ([Lua](../06_advanced-commands/03_lua-scripts.md)) |
| A unique constraint or conditional update in your database | Protecting database rows |
| An idempotency key | Making retries safe ([Stream Patterns](../10_streams/04_stream-patterns.md#idempotent-consumers)) |
| A queue with one consumer (or a partition per key) | Serializing work per entity ([Streams](../10_streams/04_stream-patterns.md#partitioned-streams)) |

Use a lock when work must not overlap and none of the above fits: a nightly job that must run on one instance, rebuilding an expensive cache entry once, an external API that can't take concurrent calls for the same resource.

## Two very different purposes

| | **Efficiency** lock | **Correctness** lock |
|---|---------------------|----------------------|
| Goal | Avoid doing the same work twice | Never let two actors modify the same thing at once |
| If it fails | Wasted work, a duplicate email | **Corrupted data**, double charges |
| Typical tools | A single Redis lock | Fencing tokens or database-level checks, possibly a consensus system |
| This module | Lessons 01 and 02 | Lessons 02 (fencing) and 03 |

Be honest about which one you are building. Most locks are the efficiency kind, and that is fine.

## The mental model

```
client A ── SET lock tokenA NX PX 10000 ──► OK       (A holds the lock for at most 10 s)
client B ── SET lock tokenB NX PX 10000 ──► nil      (B must wait or give up)
client A ── work ── release (only if token matches) ─► lock free
client A crashed? ──► the TTL expires ──► lock free   (no deadlock)
```

## Prerequisites

- Completed [10_streams](../10_streams/README.md)
- Comfortable with [Lua scripts](../06_advanced-commands/03_lua-scripts.md) and [atomic operations](../06_advanced-commands/04_atomic-and-conditional-ops.md), since safe release needs Lua

## Next

Continue to [12_rate-limiting](../12_rate-limiting/README.md).
