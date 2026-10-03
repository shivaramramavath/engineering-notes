# 18 Testing and Debugging

Redis code fails in ways ordinary code does not: keys expire in the middle of a test, two requests race on the same counter, a reconnect swallows a command, a script silently truncates a number. This chapter covers how to **prove** your Redis code works, how to **find out why** it does not, and which mistakes show up again and again.

```
write code ──► test it (real Redis) ──► ship ──► observe ──► debug ──► add a test
     ▲                                                                    │
     └────────────────────────────────────────────────────────────────────┘
```

## What you will learn

- Choose between mocks, fakes and a real Redis, and know what each can and cannot prove
- Isolate tests so they run in parallel without sharing keys
- Test TTLs, concurrency, Pub/Sub, streams, Lua scripts and failure paths
- Trace commands from Node and from the server (`MONITOR`, `SLOWLOG`, `CLIENT LIST`, `INFO`)
- Diagnose the most common errors and symptoms quickly
- Avoid the classic pitfalls before they reach production

## Contents

| # | File | Topic |
|---|------|-------|
| 01 | [Testing with Redis](./01_testing-with-redis.md) | Test strategy, isolation, containers, CI, concurrency, failure injection |
| 02 | [Debugging](./02_debugging.md) | ioredis logging, `redis-cli`, server diagnostics, error table, scenario playbooks |
| 03 | [Common Pitfalls](./03_common-pitfalls.md) | A catalog of mistakes by area, each with a fix |

## Which tool for which question

| Question | Tool |
|----------|------|
| Does my logic work? | Unit tests against a fake store |
| Does it work **with Redis semantics** (TTL, atomicity, Lua)? | Integration tests against a real Redis |
| Which commands is my app actually sending? | `DEBUG=ioredis:*`, a tracing client, `MONITOR` (briefly, never in production) |
| What is slow? | `SLOWLOG GET`, `LATENCY DOCTOR`, `redis-cli --latency` |
| Who is connected? | `CLIENT LIST` and named connections |
| Why did a key disappear? | `TTL`, `INFO stats` (`expired_keys`, `evicted_keys`), key prefix, DB number |
| Why is memory growing? | `MEMORY USAGE`, `--memkeys` (see [Memory Optimization](../16_performance/03_memory-optimization.md)) |

## Rules of thumb

1. **Test against real Redis.** Mocks cannot prove expiry, atomicity or script behavior
2. **Isolate every test** with its own key prefix. Never use `FLUSHALL` on a shared instance
3. **Never sleep when you can poll.** Wait for conditions, not for time
4. **Test the failure path.** A cache that takes your API down when Redis is unreachable is a bug
5. **Observe before you guess.** Read `INFO`, `SLOWLOG` and `CLIENT LIST` first
6. **Keep production debugging read-only and cheap.** `SCAN` over `KEYS`, replicas over primaries
7. **Turn every incident into a regression test**

## Prerequisites

- [Connection](../03_ioredis-basics/01_connection.md) and [Error Handling](../03_ioredis-basics/05_error-handling.md)
- [Redis Service](../08_nodejs-integration/02_redis-service.md) and [Redis Repository](../08_nodejs-integration/03_redis-repository.md), since testable code starts with a clean boundary
- [Security Checklist](../17_security/03_security-checklist.md) for what must stay off-limits when you debug a shared instance

**Start:** [Testing with Redis](./01_testing-with-redis.md)
