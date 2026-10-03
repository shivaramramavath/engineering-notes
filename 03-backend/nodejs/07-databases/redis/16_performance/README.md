# 16 · Performance

Redis is fast by default, so most performance problems are **self-inflicted**: a slow command on a huge key, a loop of one-at-a-time round trips, a value that is ten times bigger than it needs to be, or a blocked Node.js event loop that makes Redis look slow. This module teaches you to **measure first, then fix the thing that matters**.

## Lessons

| #   | Lesson                                                       | Core idea                                                                      |
| --- | ------------------------------------------------------------ | ------------------------------------------------------------------------------ |
| 01  | [Latency and Benchmarking](./01_latency-and-benchmarking.md) | Where time goes, how to measure it honestly, how to diagnose a slow Redis      |
| 02  | [Command Optimization](./02_command-optimization.md)         | Big keys, O(N) commands, round trips, `SLOWLOG`, server settings worth knowing |
| 03  | [Memory Optimization](./03_memory-optimization.md)           | Encodings, bucketing, caps, fragmentation, measuring bytes per entity          |
| 04  | [Serialization](./04_serialization.md)                       | JSON, MessagePack, Protobuf, compression, schema evolution, event-loop cost    |

## Learning outcomes

After this module you can:

- Explain where a Redis request's time goes, from your Node.js event loop to the Redis main thread
- Measure latency as **percentiles**, run a fair benchmark, and read `SLOWLOG`, `LATENCY` and `INFO commandstats`
- Find and fix big keys, O(N) commands and N+1 round-trip patterns
- Cut memory with the right structure, compact encodings, bucketing, compression and caps
- Choose a serialization format with eyes open about size, CPU, debuggability and evolution
- Build a repeatable "is it Redis, the network, or my code?" investigation

## Where a request's time goes

```
Node.js process                                   network                 Redis server
┌──────────────────────────────┐                 ┌──────┐      ┌──────────────────────────────┐
│ build args, serialize (CPU)  │─── write ──────►│ RTT  │─────►│ queue behind other commands  │
│ event loop turn (may be late)│                 │      │      │ execute (single main thread) │
│ parse reply, deserialize     │◄── read ────────│ RTT  │◄─────│ persistence / fork / swap?   │
└──────────────────────────────┘                 └──────┘      └──────────────────────────────┘
   client-side latency                          round trips         server-side latency
```

| Layer            | Typical culprits                                                  | Where you look                                           |
| ---------------- | ----------------------------------------------------------------- | -------------------------------------------------------- |
| **Client**       | Blocked event loop, big `JSON.parse`, too many awaits in sequence | Event-loop delay, app metrics                            |
| **Network**      | Cross-AZ or cross-region hops, NAT, TLS handshakes, DNS           | `redis-cli --latency`, ping from the app host            |
| **Server queue** | One slow command makes everyone wait                              | `SLOWLOG`, `INFO commandstats`, CPU of the Redis process |
| **Execution**    | O(N) commands on big keys, long Lua                               | `SLOWLOG`, big-key scans                                 |
| **System**       | Fork for snapshots, `fsync`, swapping, THP, noisy neighbors       | `LATENCY DOCTOR`, `INFO persistence`, host metrics       |

## The rules of the road

1. **Measure before changing.** Guessing is how people tune the wrong thing
2. **Look at percentiles** (p95, p99), not averages. Users feel the tail
3. **Round trips beat everything.** Ten sequential commands cost ten round trips, however fast Redis is
4. **One slow command hurts all clients.** Redis runs commands one at a time
5. **Smaller is faster.** Smaller values mean less memory, less network and less parsing
6. **Redis rarely needs tuning. Your usage usually does.** Fix access patterns before touching server settings
7. **Re-measure after every change**, in a setup that resembles production

## Prerequisites

- Completed [15_redis-architecture](../15_redis-architecture/README.md)
- Comfortable with [Pipelines](../06_advanced-commands/01_pipelines-and-auto-pipelining.md), [Scan and Iteration](../05_key-management/02_scan-and-iteration.md), [Memory and Eviction](../02_redis-fundamentals/05_memory-and-eviction.md), and `INFO` parsing from [Replication](../15_redis-architecture/01_replication.md#monitoring-replication)

## Next

Continue to [17_security](../17_security/README.md).
