# Module 07: Database Architecture & Persistence in Node.js

> Multi-database engineering: PostgreSQL, MongoDB, Redis, SQLite, MySQL, DynamoDB, and polyglot persistence architectures.

---

## Module Overview

Node.js backends interact with diverse data storage engines. This module provides complete coverage across relational databases (PostgreSQL, MySQL, SQLite), document databases (MongoDB), in-memory key-value stores (Redis), and distributed cloud databases (DynamoDB, Elasticsearch).

---

## Sub-Tracks & Contents

### 1. PostgreSQL Relational Architecture
Complete sub-track in **[postgresql/](./postgresql/README.md)**:
- Connection pooling with `pg.Pool`, parameterized queries, and SQL injection defense.
- ACID transactions, isolation levels (`REPEATABLE READ`), and `FOR UPDATE` locks.
- Schema migrations with `node-pg-migrate` and `drizzle-kit`.
- Type-safe query builders: Kysely vs Knex.
- ORM comparison: Drizzle ORM vs Prisma v6 vs TypeORM.
- Keyset pagination and solving the N+1 problem.
- JSONB with GIN indexes and native Full-Text Search.
- Real-time pub/sub with `LISTEN`/`NOTIFY`.
- Ephemeral Docker testing with Testcontainers.

### 2. Document & In-Memory Sub-Tracks
- **[mongodb/](./mongodb/README.md)**: 18 deep modules covering Mongoose, aggregation pipelines, replica sets, and change streams.
- **[redis/](./redis/README.md)**: 21 comprehensive modules covering `ioredis`, caching strategies, pub/sub, Lua scripts, and Redis Streams.

### 3. Specialized Storage & Selection Guides
- [01-mysql-with-node.md](./01-mysql-with-node.md): MySQL 8+ connection pooling and transactions with `mysql2`.
- [02-sqlite-with-node.md](./02-sqlite-with-node.md): Native `node:sqlite` vs `better-sqlite3`, WAL mode.
- [03-dynamodb-from-node.md](./03-dynamodb-from-node.md): Single-digit ms serverless persistence with AWS SDK v3.
- [04-elasticsearch-and-opensearch.md](./04-elasticsearch-and-opensearch.md): Full-text search and aggregations.
- [05-database-selection-guide.md](./05-database-selection-guide.md): Architectural decision matrix and polyglot persistence.

---

## Next Steps
Proceed to [08-authentication-security](../08-authentication-security/README.md) to explore authentication, JWTs, Argon2 password hashing, and OWASP defenses.
