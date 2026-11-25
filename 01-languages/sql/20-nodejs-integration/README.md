# Node.js Integration

Using SQL from Node.js: drivers, pooling, transactions, error handling, ORMs and query builders.

| Note | What it covers |
|---|---|
| [Node.js Database Drivers](./00-node-drivers.md) | node-postgres and mysql2: connecting, querying and parameterizing. |
| [Connection Pooling](./01-connection-pooling.md) | Pool configuration, sizing, leaks and PgBouncer. |
| [Transactions in Node.js](./02-node-transactions.md) | Checking out clients, commit/rollback flow and retry on serialization failures. |
| [Database Error Handling](./03-error-handling.md) | SQLSTATE codes, constraint violations and mapping errors to application errors. |
| [Repository Pattern](./04-repository-pattern.md) | Structuring data access code and keeping SQL out of business logic. |
| [ORM vs Raw SQL](./05-orm-vs-raw-sql.md) | Raw SQL, query builders and ORMs: trade-offs and how to choose. |
| [Prisma](./06-prisma.md) | Schema, client, migrations and when Prisma fits. |
| [Drizzle](./07-drizzle.md) | Schema-in-TypeScript, queries and migrations. |
| [TypeORM](./08-typeorm.md) | Entities, repositories and query building. |
| [Knex](./09-knex.md) | Query builder basics, migrations and seeds. |
| [N+1 Queries and ORM Performance](./10-n-plus-one.md) | Detecting and fixing N+1 queries and other ORM performance pitfalls. |


Next folder: [Production](../21-production/README.md)
