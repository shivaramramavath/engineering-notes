# Module 07: PostgreSQL for Node.js Engineering

> Production-grade relational database architecture with PostgreSQL: drivers, connection pooling, transactions, migrations, type-safe query builders, and real database testing.

---

## Module Overview

PostgreSQL is the industry-standard relational database for modern Node.js backend systems. This sub-track covers everything from low-level `pg` socket pooling and parameterized queries to advanced schema migrations, Drizzle ORM, Kysely, full-text search, and real database verification using Testcontainers.

---

## Contents

| Index | Note | Core Focus |
| :---: | :--- | :--- |
| **01** | [01-connection-and-queries.md](./01-connection-and-queries.md) | `pg.Pool` configuration, socket borrowing, parameterized query security. |
| **02** | [02-transactions-and-indexing.md](./02-transactions-and-indexing.md) | ACID transactions, isolation levels, `FOR UPDATE` row locks, composite indexes. |
| **03** | [03-migrations-with-node-pg-migrate-and-drizzle.md](./03-migrations-with-node-pg-migrate-and-drizzle.md) | Schema migrations with `node-pg-migrate` and `drizzle-kit`. |
| **04** | [04-query-builders-kysely-and-knex.md](./04-query-builders-kysely-and-knex.md) | Zero-overhead type-safe SQL query building with Kysely. |
| **05** | [05-orms-prisma-drizzle-typeorm-compared.md](./05-orms-prisma-drizzle-typeorm-compared.md) | Architectural comparison: Drizzle vs Prisma v6 vs TypeORM. |
| **06** | [06-pagination-and-n-plus-one.md](./06-pagination-and-n-plus-one.md) | Keyset/cursor pagination vs offset pagination, solving N+1 with DataLoader. |
| **07** | [07-json-and-full-text-search.md](./07-json-and-full-text-search.md) | `JSONB` with GIN indexes, PostgreSQL native Full-Text Search (`tsvector`). |
| **08** | [08-listen-notify-and-replication.md](./08-listen-notify-and-replication.md) | Real-time database event push via `LISTEN`/`NOTIFY`. |
| **09** | [09-testing-with-real-databases-and-testcontainers.md](./09-testing-with-real-databases-and-testcontainers.md) | Ephemeral Docker testing with `@testcontainers/postgresql`. |

---

## Next Steps
Proceed to other database guides in [07-databases](../README.md) or explore [mongodb](../mongodb/README.md) and [redis](../redis/README.md).
