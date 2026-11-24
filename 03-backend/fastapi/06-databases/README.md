# Module 06: Databases & Asynchronous ORMs

Welcome to **Module 06: Databases & Asynchronous ORMs**. This module explores data persistence in FastAPI, centered on modern SQLAlchemy 2.0 Async, Alembic migrations, connection pooling, and multi-database patterns.

---

## Module Overview

| File | Title | Description |
| :--- | :--- | :--- |
| [01-sqlalchemy-2-core-and-orm-fundamentals.md](./01-sqlalchemy-2-core-and-orm-fundamentals.md) | **SQLAlchemy 2.0 Fundamentals** | 2.0 select syntax, Mapped types, and engine initialization. |
| [02-async-sqlalchemy-with-asyncpg-psycopg3.md](./02-async-sqlalchemy-with-asyncpg-psycopg3.md) | **Async Drivers (asyncpg/psycopg3)** | Choosing and tuning asynchronous PostgreSQL drivers. |
| [03-session-management-with-dependencies.md](./03-session-management-with-dependencies.md) | **Session Management** | Scoped `AsyncSession` dependency injection and commit lifecycle. |
| [04-models-vs-schemas-separation.md](./04-models-vs-schemas-separation.md) | **Models vs Schemas** | Clean decoupling of SQLAlchemy ORM models from Pydantic DTOs. |
| [05-repository-pattern-and-unit-of-work.md](./05-repository-pattern-and-unit-of-work.md) | **Repository & Unit of Work** | Abstracting data access and multi-table atomic transactions. |
| [06-alembic-migrations-and-ci.md](./06-alembic-migrations-and-ci.md) | **Alembic Migrations** | Async migration runner, auto-generate, and CI pipeline checks. |
| [07-transactions-and-isolation.md](./07-transactions-and-isolation.md) | **Transactions & Isolation** | Read Committed, Repeatable Read, Serializable, and savepoints. |
| [08-relationships-and-loading-strategies.md](./08-relationships-and-loading-strategies.md) | **Relationships & Loading** | `selectinload` vs `joinedload` across async boundaries. |
| [09-n-plus-one-and-query-performance.md](./09-n-plus-one-and-query-performance.md) | **N+1 Problem & Optimization** | Detecting and eliminating N+1 query performance degradations. |
| [10-connection-pooling-and-limits.md](./10-connection-pooling-and-limits.md) | **Connection Pooling** | Sizing `pool_size` and `max_overflow` for container clusters. |
| [11-sqlmodel-overview-and-tradeoffs.md](./11-sqlmodel-overview-and-tradeoffs.md) | **SQLModel Overview** | Combining Pydantic and SQLAlchemy: ergonomics vs enterprise limits. |
| [12-tortoise-and-other-orms-compared.md](./12-tortoise-and-other-orms-compared.md) | **Alternative ORMs** | Tortoise ORM, Peewee, and Piccolo compared. |
| [13-pagination-filtering-and-sorting.md](./13-pagination-filtering-and-sorting.md) | **Pagination & Filtering** | Offset vs Keyset/Cursor pagination for high-volume tables. |
| [14-soft-delete-and-multi-tenancy.md](./14-soft-delete-and-multi-tenancy.md) | **Soft Delete & Tenancy** | `deleted_at` handling, partial indexes, and tenant isolation. |
| [15-mongodb-with-beanie-or-motor.md](./15-mongodb-with-beanie-or-motor.md) | **MongoDB with Beanie** | Asynchronous document ODM with Beanie and Motor. |
| [16-redis-with-redis-py-async.md](./16-redis-with-redis-py-async.md) | **Redis with redis-py** | Async Redis client, connection pools, and caching patterns. |
| [17-testing-with-real-databases.md](./17-testing-with-real-databases.md) | **Testcontainers Integration** | Testing migrations and queries with real Dockerized PostgreSQL. |
| [18-raw-sql-and-sqlalchemy-text-safely.md](./18-raw-sql-and-sqlalchemy-text-safely.md) | **Safe Raw SQL** | Using `text()` with bound parameters; preventing SQL injection. |
| [19-full-text-search-and-pgvector.md](./19-full-text-search-and-pgvector.md) | **Full-Text & pgvector** | PostgreSQL tsvector GIN indexing and vector similarity search. |
