# Database Architecture

Most Nest apps are, at their core, an HTTP layer in front of a database. How you structure the data-access side decides how testable, changeable, and fast the app stays as it grows. This note covers the decisions that come **before** picking an ORM: layers, data ownership, and the SQL-vs-document and ORM-vs-query-builder trade-offs.

## Where database code lives

```text
Controller  ──►  Service  ──►  Repository  ──►  ORM / driver  ──►  DB
 HTTP only       rules         data access
```

| Layer | Owns | Should not |
|-------|------|-----------|
| Controller | Request/response mapping, status codes | Touch the database |
| Service | Business rules, orchestration, **transaction boundaries** | Build SQL/queries by hand, know HTTP |
| Repository | Queries, persistence mapping, translating DB errors | Contain business decisions |
| ORM/driver | Talking to the database | Leak its types upward |

For simple CRUD, a service calling the ORM directly is fine. Introduce a repository layer when queries get complex, when you want to swap or fake persistence in tests, or when several services need the same queries ([repository pattern](./03-repository-pattern.md)). Don't add layers for ceremony.

## Three kinds of "model"

Confusing these is the most common architecture smell:

| Model | Purpose | Example |
|-------|---------|---------|
| **DTO** | Shape of data over HTTP | `CreateUserDto`, `UserResponseDto` |
| **Entity / persistence model** | Shape of data in storage | TypeORM `@Entity`, Prisma model, Mongoose schema |
| **Domain model** | Business concepts and invariants | `Order` with `markPaid()` |

Small apps often collapse entity and domain model into one class, which is fine. What you should avoid is using entities as **request DTOs** (mass assignment) or returning them as **responses** (leaking columns). See [DTO](../../03-core-concepts/02-validation-and-serialization/01-dto.md) and [serialization](../../03-core-concepts/02-validation-and-serialization/07-serialization.md).

## Modules own their data

In a [modular monolith](../../08-architecture-and-patterns/01-architecture/02-modular-monolith.md), each feature module owns its tables and exposes behavior through its **service**, not its repository:

```text
OrdersModule  ──uses──►  UsersService   (exported)
              ✗ does not query the users table directly
```

Direct cross-module table access couples modules at the schema level, so a column rename in `users` silently breaks `orders`. Cross-module foreign keys are fine; cross-module queries should go through the owner's public API. This also keeps a future split into services possible.

## SQL or document store?

| | Relational (PostgreSQL, MySQL) | Document (MongoDB) |
|-|-------------------------------|--------------------|
| Data shape | Normalized tables, relations, joins | Nested documents, flexible schema |
| Integrity | Constraints, foreign keys, strong transactions | Schema validation optional; app enforces more |
| Queries | Powerful joins, aggregations, ad-hoc reporting | Fast single-document reads, aggregation pipeline |
| Fits | Most business apps (users, orders, payments), reporting | Hierarchical/self-contained aggregates, varied shapes, high write scale patterns |
| Cost of change | Migrations required | Easier shape changes, harder consistency |

A reasonable default for typical business backends is a **relational database (often PostgreSQL)**. Choose a document store when your data is naturally a self-contained document and you rarely need cross-document joins or multi-document transactions. "We might need flexibility" is a weak reason by itself: relational databases also store JSON columns.

Don't pick a database by fashion; pick by data shape, consistency needs, and what your team can operate.

## ORM, query builder, or raw driver

| Approach | Control | Productivity | Type safety | Typical use |
|----------|---------|--------------|-------------|-------------|
| **Raw driver** (`pg`, `mysql2`) | Total | Low | Manual | Performance-critical queries, scripts |
| **Query builder** | High (composes SQL) | Medium | Varies | Complex dynamic queries |
| **ORM** (TypeORM, Prisma, Mongoose) | Medium | High | Varies by tool | Most application code |

These aren't exclusive. Most ORMs let you drop to raw SQL for the hard 5% (reports, bulk updates), and that's healthy. Compare tools in [ORM comparison](./08-orm-comparison.md).

Costs of ORMs to be aware of: hidden queries (the **N+1** problem, see [indexing and queries](./06-indexing-and-query-basics.md)), leaky abstractions around transactions and locking, and generated SQL you don't read. Use query logging in development to see what actually runs.

## Connection as shared infrastructure

The database connection (pool) is app-wide infrastructure: one pool per database per process, created at startup, closed at shutdown. In Nest, that's a root-level (often global) module configured asynchronously from [config](../../03-core-concepts/03-configuration/README.md). Feature modules then register their entities/models and get repositories injected. Details in [database connection](./02-database-connection.md).

## Consistency lives in the database

Enforce what must always be true **in the database**, not only in code:

- `NOT NULL`, `UNIQUE`, `FOREIGN KEY`, `CHECK` constraints.
- Transactions for multi-step writes ([transactions](./04-transactions.md)).

Application-level checks (`if (!exists) insert`) race under concurrency; constraints don't. Treat code-level validation as the friendly first line and constraints as the guarantee ([database errors](./07-database-errors.md)).

## Scaling considerations (just enough)

- **Vertical first**: better indexes and queries beat bigger servers.
- **Read replicas** for read-heavy load: route reads to replicas, accept replication lag, never read-your-own-write from a replica without care.
- **Caching** reduces reads but adds invalidation problems ([caching](../../05-advanced/01-caching/01-caching-fundamentals.md)).
- **One database per service** only when you've genuinely split into services ([monolith vs microservices](../../08-architecture-and-patterns/01-architecture/07-monolith-vs-microservices.md)).
- Connection counts multiply with instances ([connection sizing](./02-database-connection.md)).

## Common mistakes

- **Returning entities from controllers** and leaking internal columns.
- **Using entities as input DTOs**, enabling mass assignment.
- **Business logic in repositories** or SQL in controllers.
- **Cross-module table access** that bypasses the owning module.
- **`synchronize`-style schema management** instead of migrations ([migrations](./05-migrations.md)).
- **Enforcing uniqueness only in code.**
- **Choosing NoSQL "for flexibility"** and then reimplementing joins and integrity in the app.
- **Never reading the generated SQL**, so N+1 and full scans go unnoticed.

## Quick Summary

- Layers: controller (HTTP) → service (rules, transactions) → repository (data access) → ORM/driver.
- Keep DTOs, persistence models, and domain models distinct in purpose even if classes merge.
- Modules own their tables; others go through the module's service.
- Default to a relational database for business data; choose document stores for document-shaped data.
- ORMs boost productivity but hide queries; log them and use raw SQL where needed.
- Enforce invariants with database constraints and transactions.

## Next

[Database connection →](./02-database-connection.md)
