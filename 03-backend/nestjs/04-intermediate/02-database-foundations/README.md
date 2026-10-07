# Database Foundations

Before choosing an ORM, you need the ideas that apply to **every** database layer: where data access code lives, how connections work, how to keep multi-step writes consistent, how schema changes ship safely, and how to read database errors. The ORM-specific sections ([TypeORM](../03-typeorm/README.md), [Prisma](../04-prisma/README.md), [Mongoose](../05-mongoose/README.md)) build on this one.

```text
 Controller
     │  DTOs
     ▼
  Service            business rules, orchestrates, owns transaction boundaries
     │
     ▼
  Repository         data access: queries, mapping, error translation
     │
     ▼
  ORM / driver       TypeORM · Prisma · Mongoose · raw driver
     │
     ▼
  Connection pool    reused connections, limits, timeouts
     │
     ▼
  Database           schema (migrations), indexes, constraints
```

> Examples lean on PostgreSQL for SQL specifics. Concepts transfer; error codes and DDL details differ per engine, and each note says so where it matters.

## Reading order

| # | Note | Answers |
|---|------|---------|
| 01 | [Database architecture](./01-database-architecture.md) | Where DB code lives, SQL vs document, ORM vs query builder, module ownership |
| 02 | [Database connection](./02-database-connection.md) | Pools, async config, sizing, shutdown, connection failures |
| 03 | [Repository pattern](./03-repository-pattern.md) | When to wrap the ORM, ports and adapters, testability |
| 04 | [Transactions](./04-transactions.md) | ACID, isolation, passing transaction context, locking, side effects |
| 05 | [Migrations](./05-migrations.md) | Versioned schema changes, safe deployment, expand/contract |
| 06 | [Indexing and query basics](./06-indexing-and-query-basics.md) | Indexes, `EXPLAIN`, N+1, pagination cost |
| 07 | [Database errors](./07-database-errors.md) | Constraint/connection/deadlock errors and mapping them to HTTP |
| 08 | [ORM comparison](./08-orm-comparison.md) | TypeORM vs Prisma vs Mongoose, how to choose |

## Prerequisites

- [Providers and services](../../02-fundamentals/04-providers-and-services.md) and [dynamic modules](../../03-core-concepts/04-modules-and-di/03-dynamic-modules.md)
- [Configuration](../../03-core-concepts/03-configuration/README.md)
- Basic SQL (`SELECT`, `JOIN`, `INSERT`, `UPDATE`)

## Related

- [Exception filters](../../03-core-concepts/01-request-pipeline/08-exception-filters.md) for mapping errors to responses
- [Integration testing](../01-testing/05-integration-testing.md) for testing against a real database
- [Database performance](../../07-production/02-performance/03-database-performance.md)
- [Transactional outbox](../../08-architecture-and-patterns/04-real-world-patterns/03-transactional-outbox.md)
