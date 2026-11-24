---
title: "Multi-Tenancy Architectures in NestJS"
description: "Architecting SaaS multi-tenancy: Shared Database with Discriminator, Schema-per-Tenant, and Database-per-Tenant with PostgreSQL RLS"
tags: [nestjs, patterns, multi-tenancy, saas, rls, schema-per-tenant, row-level-security]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Multi-Tenancy Architectures in NestJS

## Conceptual Overview
Multi-tenant SaaS architectures isolate tenant data while maximizing resource utilization across three primary strategies:

| Strategy | Isolation Level | Infrastructure Cost | Operational Complexity |
| :--- | :--- | :--- | :--- |
| **Shared DB, Shared Schema (Discriminator)** | Logical (where tenant_id) | Lowest | Low (enforced via application or RLS) |
| **Shared DB, Schema-per-Tenant** | Moderate (Postgres schema) | Medium | Moderate (migration automation across schemas) |
| **Database-per-Tenant** | Complete (physical isolation) | Highest | High (connection pooling management) |

---

## PostgreSQL Row-Level Security (RLS) Implementation

PostgreSQL Row-Level Security enforces tenant boundaries at the database engine level, preventing data leaks even if application code forgets a `WHERE tenant_id = ...` filter:

```sql
-- Enable RLS on orders table
ALTER TABLE orders ENABLE ROW LEVEL SECURITY;

-- Enforce tenant isolation via session setting
CREATE POLICY tenant_isolation_policy ON orders
  USING (tenant_id = current_setting('app.current_tenant_id')::uuid);
```

```typescript
// NestJS Interceptor setting session variable:
await queryRunner.query(`SET LOCAL app.current_tenant_id = '${tenantId}'`);
```

---

## Key Takeaways & Cheat Sheet

- For high-scale SaaS, prefer **Shared Database with PostgreSQL Row-Level Security (RLS)**.
- Use `AsyncLocalStorage` to propagate the active `tenantId` across service layers.
- Always include `tenant_id` in compound unique indexes.

---

## Cross-References
- [06-provider-scopes-and-performance-impact.md](../../03-core-concepts/04-modules-and-di/06-provider-scopes-and-performance-impact.md) - Scopes and tenancy
- [01-audit-logging.md](./01-audit-logging.md) - Audit logging
