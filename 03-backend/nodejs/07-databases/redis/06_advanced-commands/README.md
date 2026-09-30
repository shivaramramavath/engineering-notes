# 06 · Advanced Commands

Single commands are atomic, but real features need **many commands** that are fast, consistent, or both. This module covers the four tools for that job, and how to choose between them.

## Lessons

| # | Lesson | Core idea |
|---|--------|-----------|
| 01 | [Pipelines and Auto Pipelining](./01_pipelines-and-auto-pipelining.md) | Fewer round trips. **Fast, not atomic** |
| 02 | [Transactions](./02_transactions.md) | `MULTI`/`EXEC` and `WATCH`. **Atomic batch, no logic** |
| 03 | [Lua Scripts](./03_lua-scripts.md) | Server-side logic. **Atomic, with conditionals** |
| 04 | [Atomic and Conditional Operations](./04_atomic-and-conditional-ops.md) | `SET NX`, `INCR`, compare-and-set, idempotency |

## Learning outcomes

After this module you can:

- Cut latency by batching commands with pipelines and auto pipelining
- Apply transactions correctly, and explain why they don't roll back
- Write, load and test Lua scripts with `defineCommand`
- Remove race conditions from check-then-act code
- Pick the lightest tool that gives the guarantee you need

## Which tool do I need?

| Need | Tool |
|------|------|
| Many independent commands, just faster | Pipeline / auto pipelining |
| Same operation on many keys | `MGET`, `MSET`, or a pipeline |
| A fixed batch that must run without interleaving | `MULTI`/`EXEC` |
| Read a value, decide, then write, atomically | **Lua script** |
| One conditional write | A single command with options (`SET NX`, `ZADD GT`, `EXPIRE NX`) |
| Optimistic concurrency, low contention | `WATCH` + `MULTI` (on a dedicated connection) |

```
Faster?         ─► Pipeline
Atomic batch?   ─► MULTI/EXEC
Atomic + logic? ─► Lua
One command can do it? ─► Use that command
```

## Prerequisites

- Completed [05_key-management](../05_key-management/README.md)
- Comfortable with [error handling](../03_ioredis-basics/05_error-handling.md), since pipeline errors are per command

## Next

Continue to [07_caching](../07_caching/README.md).
