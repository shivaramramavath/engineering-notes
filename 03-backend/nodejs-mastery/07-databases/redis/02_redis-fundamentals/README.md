# 02 · Redis Fundamentals

The core concepts every later module depends on: how keys and values work, which data types exist, how expiry behaves, how data survives restarts, and what happens when memory runs out.

## Lessons

| #   | Lesson                                             | You will learn                                                   |
| --- | -------------------------------------------------- | ---------------------------------------------------------------- |
| 01  | [Keys and Values](./01_keys-and-values.md)         | Key rules, naming conventions, key-builder basics, databases     |
| 02  | [Data Types Overview](./02_data-types-overview.md) | Every core type, when to use each, basic ioredis calls           |
| 03  | [Expiration and TTL](./03_expiration-and-ttl.md)   | Setting, reading and removing expiry, and how Redis expires keys |
| 04  | [Persistence](./04_persistence.md)                 | RDB, AOF, hybrid setups and choosing durability                  |
| 05  | [Memory and Eviction](./05_memory-and-eviction.md) | `maxmemory`, eviction policies, measuring and controlling memory |

## Learning outcomes

After this module you can:

- Design consistent, scannable key names
- Choose the right data type for a job
- Use TTLs correctly and avoid common expiry mistakes
- Pick a persistence setup that matches your durability needs
- Configure memory limits so Redis degrades predictably

## Prerequisites

- Completed [01_setup](../01_setup/README.md) with a running Redis and a working ioredis client

## Next

Continue to [03_ioredis-basics](../03_ioredis-basics/README.md).
