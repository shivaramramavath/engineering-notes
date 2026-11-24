---
title: "Database Migration Operations in Zero-Downtime Deployments"
description: "Executing safe, zero-downtime database schema migrations using Expand/Contract pattern, TypeORM, and Prisma in CI/CD"
tags: [nestjs, operations, database-migrations, zero-downtime, expand-contract, typeorm, prisma]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Database Migration Operations in Zero-Downtime Deployments

## Conceptual Overview
Running `npm run migration:run` inside application container `onModuleInit()` during rolling deployments is dangerous: multiple pods run migrations concurrently, causing table lock deadlocks or breaking older pods still servicing traffic.

Zero-downtime schema evolution requires the **Expand and Contract (Parallel Run) Pattern**:
1. **Expand**: Add new columns/tables without removing or altering existing ones. (Old code and new code both function).
2. **Deploy**: Roll out the new NestJS application version writing to both columns.
3. **Contract**: Execute a post-deployment migration dropping the obsolete columns/tables.

---

## Key Takeaways & Cheat Sheet

- Never run database migrations inside application startup hooks (`main.ts`).
- Execute migrations as a dedicated pre-deployment step (e.g., Kubernetes Job or CI pipeline).
- Use the Expand/Contract pattern for breaking schema alterations (renaming columns, changing types).

---

## Cross-References
- [02-database-foundations](../../04-intermediate/02-database-foundations/README.md) - Database configuration
- [07-kubernetes.md](../04-deployment/07-kubernetes.md) - Kubernetes jobs
