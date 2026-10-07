# TypeORM

TypeORM is a decorator-based ORM for TypeScript. You describe tables as classes (**entities**), and it maps rows to instances, builds queries, and can generate migrations from entity changes. `@nestjs/typeorm` is the official Nest integration.

```text
 @Entity classes ──► DataSource (pool, config) ──► EntityManager
                                                      │
                          Repository<User> ◄──────────┤   find / save / delete
                          QueryBuilder     ◄──────────┤   complex queries
                          QueryRunner      ◄──────────┘   transactions, raw control
                                   │
                                   ▼
                               database  ◄── migrations (CLI) ── schema
```

> Applies to TypeORM **0.3.x** with `@nestjs/typeorm` (Nest 10/11). API details change between releases (especially the CLI, `exist`/`exists`, and newer options), so confirm specifics against the TypeORM docs for your installed version.

## Reading order

| # | Note | Answers |
|---|------|---------|
| 01 | [Setup](./01-setup.md) | Installing, `forRoot`/`forRootAsync`, `forFeature`, a CLI `DataSource` |
| 02 | [Entities](./02-entities.md) | Columns, types, enums, timestamps, hooks, soft delete, naming |
| 03 | [Relations](./03-relations.md) | One-to-one/many, many-to-many, loading strategies, cascades |
| 04 | [Repositories](./04-repositories.md) | `find` options, `save` vs `update`, custom repositories, gotchas |
| 05 | [Query builder](./05-query-builder.md) | Joins, aggregates, subqueries, safe dynamic queries |
| 06 | [Migrations](./06-migrations.md) | Generate/run/revert, CLI data source, safe workflows |
| 07 | [Transactions](./07-transactions.md) | `DataSource.transaction`, `QueryRunner`, locking |

## Prerequisites

- [Database foundations](../02-database-foundations/README.md), especially [connection](../02-database-foundations/02-database-connection.md) and [repository pattern](../02-database-foundations/03-repository-pattern.md)
- [Dynamic modules](../../03-core-concepts/04-modules-and-di/03-dynamic-modules.md) and [configuration](../../03-core-concepts/03-configuration/README.md)

## Related

- [ORM comparison](../02-database-foundations/08-orm-comparison.md)
- [Database errors](../02-database-foundations/07-database-errors.md) (`QueryFailedError` handling)
- [Testing with a real database](../01-testing/05-integration-testing.md)
- [TypeORM quick reference](../../13-quick-reference/07-typeorm.md)
