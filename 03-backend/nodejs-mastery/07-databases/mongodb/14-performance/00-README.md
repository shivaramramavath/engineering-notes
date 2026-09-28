# 14 — Performance

Everything so far has focused on correctness — getting the right data in and out. This section covers making it fast: indexes, query optimization, `.lean()`, pagination that scales, and connection pooling. Several earlier files pointed forward to this section whenever a performance trade-off came up.

## In this section

| File                                | Covers                                                                                                                       |
| ----------------------------------- | ---------------------------------------------------------------------------------------------------------------------------- |
| `01-indexes-in-mongoose.md`         | How indexes actually speed up queries, choosing what to index, and using `explain()` to verify an index is really being used |
| `02-query-optimization-and-lean.md` | `.lean()`, `.select()`, avoiding unnecessary work, and other query-level optimizations                                       |
| `03-pagination.md`                  | Why `skip`/`limit` degrades on large collections, and cursor-based (keyset) pagination as the scalable alternative           |
| `04-connection-pooling.md`          | How Mongoose's connection pool works, tuning `maxPoolSize`, and diagnosing pool exhaustion                                   |

## The single most important performance principle

**Measure before optimizing.** Every technique in this section has a real cost (index maintenance overhead, added code complexity, reduced convenience from `.lean()`) — applying them blindly everywhere is worse than applying them precisely where a measured problem actually exists. `explain()` (in `01-indexes-in-mongoose.md`) is the tool for finding out where time is actually going, rather than guessing.

## What you should be able to do after this section

- Choose which fields to index based on actual query patterns, and confirm an index is really being used
- Use `.lean()`, `.select()`, and related techniques appropriately, and know their trade-offs
- Implement pagination that stays fast on large collections
- Size and tune the connection pool sensibly, and recognize the symptoms of pool exhaustion

## Next

**`15-patterns-and-architecture`** covers higher-level structural patterns — the repository/service pattern, soft delete, and multi-tenancy.
