# Multi-Tenant SaaS Backend Architecture in Node.js

> Multi-tenant isolation models (database-per-tenant, schema-per-tenant, row-level security), subdomain resolution, and tenant-scoped connection pooling.

| Metadata | Details |
| :--- | :--- |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Patterns** | Multi-Tenancy (RLS, Dynamic Connection Routing) |
| **Compliance** | SOC 2 Type II, Tenant Data Isolation |

---

## 1. Comparing Multi-Tenancy Isolation Models

| Model | Description | Pros | Cons |
| :--- | :--- | :--- | :--- |
| **1. Database-Per-Tenant** | Each customer gets their own isolated PostgreSQL DB instance. | Maximum isolation, easy backup/restore per tenant. | Costly, complex connection pool management in Node. |
| **2. Schema-Per-Tenant** | Single DB instance; each tenant has their own schema (`tenant_acme`). | Strong isolation, lower cost than separate DBs. | Migration overhead across thousands of schemas. |
| **3. Shared Database with RLS** | Shared tables with `tenant_id` column protected by PostgreSQL Row-Level Security. | Lowest cost, simplified migrations, highly scalable. | Requires rigorous query enforcement to prevent leaks. |

---

## 2. Implementing Tenant Scoping via AsyncLocalStorage & RLS

```javascript
import { AsyncLocalStorage } from 'node:async_hooks';

export const tenantContext = new AsyncLocalStorage();

// Express / Fastify Middleware resolving tenant from subdomain or JWT
export function tenantMiddleware(req, res, next) {
  const host = req.headers.host || '';
  const subdomain = host.split('.')[0]; // e.g. 'acme' from 'acme.platform.com'

  const tenant = resolveTenant(subdomain);
  if (!tenant) return res.status(404).json({ error: 'Tenant not found' });

  tenantContext.run(tenant, () => {
    next();
  });
}

// Database query wrapper applying PostgreSQL Row-Level Security session variable
export async function withTenantDb(callback) {
  const tenant = tenantContext.getStore();
  if (!tenant) throw new Error('No active tenant context!');

  const client = await pool.connect();
  try {
    // Set PostgreSQL session variable for RLS
    await client.query(`SET LOCAL app.current_tenant_id = '${tenant.id}'`);
    return await callback(client);
  } finally {
    client.release();
  }
}
```
All SQL queries executed within `withTenantDb` are mathematically restricted by Postgres to rows where `tenant_id = current_setting('app.current_tenant_id')`.
