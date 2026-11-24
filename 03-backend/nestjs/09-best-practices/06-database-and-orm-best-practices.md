---
title: "Database and ORM Best Practices in NestJS"
description: "Best practices for TypeORM, Prisma, and Kysely: migrations, indexing, transactions, and connection management"
tags: [nestjs, best-practices, database, typeorm, prisma, kysely, migrations]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Database and ORM Best Practices in NestJS

## Core Principles
1. **Never Run Auto-Sync in Production**: Disable `synchronize: true` in TypeORM. Always use version-controlled SQL migrations.
2. **Index All Foreign Keys**: Unindexed foreign keys trigger full table scans during joins and delete cascades.
3. **Use Transaction Runners**: Encapsulate multi-table mutations in explicit ACID transactions.

---

## Cross-References
- [03-database-performance.md](../../07-production/02-performance/03-database-performance.md) - Query optimization
