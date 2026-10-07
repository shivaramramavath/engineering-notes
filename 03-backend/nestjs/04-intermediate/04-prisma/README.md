# Prisma

Prisma is a schema-first ORM. You describe your data model in a `schema.prisma` file, Prisma **generates a type-safe client** from it, and `prisma migrate` turns schema changes into SQL migrations. In Nest you wrap the generated client in an injectable `PrismaService` (there's no official `@nestjs/prisma` module; the pattern is a documented recipe).

```text
 schema.prisma ──► prisma generate ──► generated client (types + query API)
      │                                        │
      │                                        ▼
      │                          PrismaService (client + driver adapter)
      │                                        │
      ▼                                        ▼
 prisma migrate ──► SQL migrations      services / repositories ──► database
```

> **Version note.** These notes target **Prisma ORM 7**, where the generator is `prisma-client` with a required `output` folder, SQL databases need a **driver adapter** (for PostgreSQL, `@prisma/adapter-pg`), CLI settings live in `prisma.config.ts`, and `.env` files are no longer loaded automatically. Prisma 5/6 projects differ (generator `prisma-client-js`, imports from `@prisma/client`, URL in the schema, no required adapter); differences are called out where they matter. Prisma moves fast, so confirm details against the current docs and the [upgrade guide](https://www.prisma.io/docs/guides/upgrade-guides) for your version.

## Reading order

| # | Note | Answers |
|---|------|---------|
| 01 | [Setup](./01-setup.md) | Install, generator, `prisma.config.ts`, adapter, `PrismaService`, build/CI |
| 02 | [Schema](./02-schema.md) | Models, field types, attributes, indexes, enums, type gotchas |
| 03 | [Prisma Client](./03-prisma-client.md) | CRUD, filters, `select`/`include`, pagination, raw SQL, the `undefined` trap |
| 04 | [Relations](./04-relations.md) | 1-1, 1-n, m-n, nested writes, referential actions, loading |
| 05 | [Repositories](./05-repositories.md) | Wrapping the client, types, transaction-aware repositories, testing |
| 06 | [Migrations](./06-migrations.md) | `migrate dev` vs `deploy`, editing SQL, baselining, seeding |
| 07 | [Transactions](./07-transactions.md) | Batch vs interactive, isolation, timeouts, locking, retries |

## Prerequisites

- [Database foundations](../02-database-foundations/README.md), especially [connection](../02-database-foundations/02-database-connection.md) and [transactions](../02-database-foundations/04-transactions.md)
- [Configuration](../../03-core-concepts/03-configuration/README.md) and [dynamic/global modules](../../03-core-concepts/04-modules-and-di/02-global-modules.md)

## Related

- [ORM comparison](../02-database-foundations/08-orm-comparison.md)
- [Database errors](../02-database-foundations/07-database-errors.md) (Prisma `P2xxx` codes)
- [Testing with a real database](../01-testing/05-integration-testing.md)
- [Prisma quick reference](../../13-quick-reference/08-prisma.md)