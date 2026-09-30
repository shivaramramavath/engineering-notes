# 08 · Node.js Integration

Earlier modules taught Redis commands. This module teaches how to **structure a real Node.js codebase around Redis** so it stays testable, consistent, and safe as it grows.

## Lessons

| # | Lesson | Core idea |
|---|--------|-----------|
| 01 | [Connection Management](./01_connection-management.md) | How many connections, created once, closed once |
| 02 | [Redis Service](./02_redis-service.md) | A thin service with policies, injected where needed |
| 03 | [Redis Repository](./03_redis-repository.md) | Persist domain objects without leaking Redis details |
| 04 | [Typed Redis Client](./04_typed-redis-client.md) | Compile-time and runtime type safety for keys and values |
| 05 | [Redis Key Builder](./05_redis-key-builder.md) | One source of truth for every key |
| 06 | [Express Integration](./06_express-integration.md) | Wiring it all into an HTTP app, health checks, shutdown |

## Learning outcomes

After this module you can:

- Decide how many Redis connections your process needs, and manage their lifecycle
- Keep Redis behind small interfaces so business code is easy to test
- Store and load domain objects with explicit mapping and index maintenance
- Get end-to-end types for keys, values and custom commands
- Build an Express app with health checks, error mapping and clean shutdown

## Target project layout

```
src/
├── config/
│   └── env.ts                 # validated environment
├── redis/
│   ├── connection.ts          # client factories and lifecycle (lesson 01)
│   ├── keys.ts                # key builder and key registry (lesson 05)
│   ├── codecs.ts              # serialization and validation (lesson 04)
│   ├── store.ts               # typed store (lesson 04)
│   ├── service.ts             # RedisService (lesson 02)
│   ├── commands.ts            # defineCommand + typings (module 06)
│   └── scripts/               # .lua files
├── repositories/
│   └── user.repository.ts     # lesson 03
├── services/                  # business logic, depends on interfaces
├── http/
│   ├── app.ts                 # Express app factory (lesson 06)
│   ├── middleware/
│   └── routes/
├── container.ts               # composition root: builds and wires everything
└── main.ts                    # starts the server, handles shutdown
```

## Design principles

1. **One composition root.** Objects are created in one place (`container.ts`) and passed in, so nothing imports a global client by accident
2. **Depend on interfaces, not on ioredis.** Business code takes a `CachePort` or a repository interface, which makes tests trivial
3. **Keys come from one place.** No inline template strings for keys anywhere else
4. **Decide the failure policy per use case.** A cache fails open, a lock fails closed. The policy lives in the service, not in every call site
5. **Validate at the boundary.** Data read from Redis is untrusted input (old versions, manual edits, bugs)

## Prerequisites

- Completed [07_caching](../07_caching/README.md)
- Comfortable with [error handling](../03_ioredis-basics/05_error-handling.md) and [graceful shutdown](../03_ioredis-basics/06_graceful-shutdown.md)

## Next

Continue to [09_pub-sub](../09_pub-sub/README.md).
