# 05 · Key Management

Data structures decide how a single key behaves. **Key management decides how thousands or millions of keys behave together**: how they are named, how you find them safely, and how you remove them without hurting production.

## Lessons

| # | Lesson | You will learn |
|---|--------|----------------|
| 01 | [Key Design](./01_key-design.md) | Naming, namespaces, versioning, cluster-safe keys, TTL policy, key registries |
| 02 | [Scan and Iteration](./02_scan-and-iteration.md) | `SCAN`, `HSCAN`, `SSCAN`, `ZSCAN`, `scanStream`, audits, Cluster scanning |
| 03 | [Delete and Unlink](./03_delete-and-unlink.md) | `DEL` vs `UNLINK`, bulk deletion, big keys, invalidation patterns |

## Learning outcomes

After this module you can:

- Design a key scheme that stays consistent as the codebase and team grow
- Find keys by pattern without blocking Redis
- Delete one key, millions of keys, or one enormous key safely
- Audit a live keyspace for missing TTLs and oversized keys

## Prerequisites

- Completed [04_data-structures](../04_data-structures/README.md)
- Comfort with the naming basics in [Keys and Values](../02_redis-fundamentals/01_keys-and-values.md)

## The three rules of this module

1. **Never use `KEYS` in production.** Use `SCAN`.
2. **Never delete large things with `DEL`.** Use `UNLINK`.
3. **Never let a key class exist without a TTL or a trim policy** unless you can explain why.

## Next

Continue to [06_advanced-commands](../06_advanced-commands/README.md).
