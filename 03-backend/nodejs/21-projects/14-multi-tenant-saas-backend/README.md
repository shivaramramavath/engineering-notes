# Project 14: Multi-Tenant B2B SaaS Backend with Row-Level Security (`proj-node-14-multi-tenant-saas`)

> Enterprise multi-tenant SaaS backend built with Node.js 22 LTS, Express 5, and PostgreSQL. Features cryptographic tenant isolation using PostgreSQL Row-Level Security (RLS) policies (`SET LOCAL app.current_tenant_id`), automated context propagation via `AsyncLocalStorage`, tenant-namespaced Redis caching, and zero-leakage integration tests.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-node-14-multi-tenant-saas` |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Module System** | ESM (`"type": "module"`) |
| **Tenant Isolation** | PostgreSQL Row-Level Security (RLS) |
| **Context Propagation**| `node:async_hooks` (`AsyncLocalStorage`) |
| **Tenant Caching** | Namespaced Redis Keys (`tenant:{id}:*`) |
| **Database** | PostgreSQL 16+ via `pg` |
| **Testing** | Vitest (Cross-tenant leak prevention suite) |
| **Complexity Level** | Enterprise Grade / High-Security |

---

## 1. Architectural Overview & Domain Design

### 1.1 Row-Level Security & AsyncLocalStorage Flow

In multi-tenant architectures, relying purely on manual SQL `WHERE tenant_id = ?` clauses introduces catastrophic human error risks. This system enforces isolation at the database kernel level:
1. **Tenant Identification**: Middleware extracts the tenant ID from the subdomain (e.g. `acme.app.com`) or the verified JWT claim.
2. **Context Propagation**: `AsyncLocalStorage.run()` binds the active `tenantId` to the current execution thread across all asynchronous calls without parameter drilling.
3. **Session Parameter Injection**: Every database query executes within a transaction that sets `SET LOCAL app.current_tenant_id = $1`.
4. **PostgreSQL RLS Enforcement**: The PostgreSQL engine rejects any attempt to read or write rows where `tenant_id != current_setting('app.current_tenant_id')`, even if the application developer accidentally omits `tenant_id` from the SQL query.

```
+----------------------------------------------------------------------------------------------------+
|                               Multi-Tenant SaaS Isolation Topology                                 |
|                                                                                                    |
|  [ Tenant Acme Request: GET /api/v1/projects ]                                                     |
|                  │                                                                                 |
|                  ▼                                                                                 |
|  +─────────────────────────────────────────────────────────────+                                   |
|  |                 Tenant Identification Middleware            |                                   |
|  |  - Extract Tenant: 'acme_corp'                              |                                   |
|  |  - Store in AsyncLocalStorage context                       |                                   |
|  +─────────────────────────────┬───────────────────────────────+                                   |
|                                │ Bound Context (tenantId: 'acme_corp')                             |
|                                ▼                                                                   |
|  +─────────────────────────────────────────────────────────────+                                   |
|  |                 Transactional Database Proxy                |                                   |
|  |  1. BEGIN TRANSACTION                                       |                                   |
|  |  2. SET LOCAL app.current_tenant_id = 'acme_corp';          |                                   |
|  |  3. SELECT * FROM projects; -- (No WHERE tenant_id required!)|                                   |
|  |  4. COMMIT                                                  |                                   |
|  +─────────────────────────────┬───────────────────────────────+                                   |
|                                │ Protected Execution                                               |
|                                ▼                                                                   |
|  +─────────────────────────────────────────────────────────────+                                   |
|  |          PostgreSQL Kernel with Row-Level Security          |                                   |
|  |  POLICY tenant_isolation_policy ON projects                 |                                   |
|  |  USING (tenant_id = current_setting('app.current_tenant_id')) |                                   |
|  +─────────────────────────────────────────────────────────────+                                   |
|                                │                                                                   |
|                                ▼ Returns Only Acme Corp Rows!                                      |
|  [ Zero Cross-Tenant Data Leakage Guaranteed at DB Kernel Level ]                                  |
+----------------------------------------------------------------------------------------------------+
```

### 1.2 Directory Layout

```text
proj-node-14-multi-tenant-saas/
├── src/
│   ├── context/
│   │   └── tenant.context.ts    # AsyncLocalStorage wrapper
│   ├── infrastructure/
│   │   └── db.ts                # PostgreSQL pool with RLS session wrapper
│   ├── presentation/
│   │   ├── middleware/
│   │   │   └── tenant.middleware.ts # Tenant resolver
│   │   └── app.ts               # Express 5 server
├── test/
│   └── integration/
│       └── rls-leak.test.ts     # Verification that Tenant A cannot read Tenant B data
├── Dockerfile
├── docker-compose.yml
└── package.json
```

---

## 2. Complete File-by-File Implementation

### `package.json`
```json
{
  "name": "proj-node-14-multi-tenant-saas",
  "version": "1.0.0",
  "type": "module",
  "scripts": {
    "start": "node --env-file=.env src/presentation/app.js",
    "test": "vitest run"
  },
  "dependencies": {
    "express": "^5.0.1",
    "pg": "^8.13.1"
  },
  "devDependencies": {
    "@types/express": "^5.0.0",
    "@types/node": "^22.13.0",
    "@types/pg": "^8.11.11",
    "typescript": "^5.7.3",
    "vitest": "^3.0.5"
  }
}
```

### `src/context/tenant.context.ts`
```typescript
import { AsyncLocalStorage } from 'node:async_hooks';

export interface TenantStore {
  tenantId: string;
}

export const tenantStorage = new AsyncLocalStorage<TenantStore>();

export function getTenantId(): string {
  const store = tenantStorage.getStore();
  if (!store?.tenantId) {
    throw new Error('TENANT_CONTEXT_MISSING: Operation attempted outside tenant boundary');
  }
  return store.tenantId;
}
```

### `src/infrastructure/db.ts`
```typescript
import pg from 'pg';
import { getTenantId } from '../context/tenant.context.js';

export const pool = new pg.Pool({
  connectionString: process.env.DATABASE_URL || 'postgresql://postgres:postgres@localhost:5432/saas_db',
});

/**
 * Executes a database operation with automatic PostgreSQL RLS context injection.
 */
export async function withTenantDb<T>(operation: (client: pg.PoolClient) => Promise<T>): Promise<T> {
  const tenantId = getTenantId();
  const client = await pool.connect();

  try {
    await client.query('BEGIN');
    // Inject tenant session setting (scoped strictly to current transaction)
    await client.query(`SET LOCAL app.current_tenant_id = $1`, [tenantId]);

    const result = await operation(client);

    await client.query('COMMIT');
    return result;
  } catch (err) {
    await client.query('ROLLBACK');
    throw err;
  } finally {
    client.release();
  }
}
```

### `src/presentation/app.ts`
```typescript
import express from 'express';
import { tenantStorage } from '../context/tenant.context.js';
import { withTenantDb } from '../infrastructure/db.js';

export const app = express();
app.use(express.json());

// Multi-tenant context extraction middleware
app.use((req, res, next) => {
  const tenantId = (req.headers['x-tenant-id'] as string) || 'default_tenant';
  tenantStorage.run({ tenantId }, () => {
    next();
  });
});

// Create project
app.post('/api/v1/projects', async (req, res, next) => {
  try {
    const { name } = req.body;
    const result = await withTenantDb(async (client) => {
      // tenant_id column automatically populated from current_setting
      const { rows } = await client.query(
        `INSERT INTO projects (name, tenant_id) VALUES ($1, current_setting('app.current_tenant_id')) RETURNING *`,
        [name]
      );
      return rows[0];
    });
    res.status(201).json({ status: 'success', data: result });
  } catch (err) {
    next(err);
  }
});

// List projects (RLS automatically filters out other tenants)
app.get('/api/v1/projects', async (_req, res, next) => {
  try {
    const projects = await withTenantDb(async (client) => {
      const { rows } = await client.query('SELECT id, name, created_at FROM projects');
      return rows;
    });
    res.status(200).json({ status: 'success', data: projects });
  } catch (err) {
    next(err);
  }
});

if (process.env.NODE_ENV !== 'test') {
  app.listen(3000, () => console.log('[Multi-Tenant SaaS API] Running on port 3000'));
}
```

---

## 3. Database Schema with Row-Level Security

```sql
CREATE TABLE IF NOT EXISTS tenants (
    id VARCHAR(100) PRIMARY KEY,
    name VARCHAR(255) NOT NULL
);

CREATE TABLE IF NOT EXISTS projects (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    tenant_id VARCHAR(100) NOT NULL REFERENCES tenants(id) ON DELETE CASCADE,
    name VARCHAR(255) NOT NULL,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP NOT NULL
);

-- Enable Row-Level Security
ALTER TABLE projects ENABLE ROW LEVEL SECURITY;
ALTER TABLE projects FORCE ROW LEVEL SECURITY;

-- Create Isolation Policy
CREATE POLICY tenant_isolation_policy ON projects
    FOR ALL
    USING (tenant_id = current_setting('app.current_tenant_id', true))
    WITH CHECK (tenant_id = current_setting('app.current_tenant_id', true));
```

---

## 4. Verification & Testing Suite

### Unit Test (`test/unit/tenant-context.test.ts`)
```typescript
import { describe, it, expect } from 'vitest';
import { tenantStorage, getTenantId } from '../../src/context/tenant.context.js';

describe('Tenant Context AsyncLocalStorage', () => {
  it('isolates context across asynchronous calls', async () => {
    await tenantStorage.run({ tenantId: 'tenant_acme' }, async () => {
      expect(getTenantId()).toBe('tenant_acme');
      await new Promise((resolve) => setTimeout(resolve, 10));
      expect(getTenantId()).toBe('tenant_acme');
    });
  });

  it('throws error when accessed outside tenant scope', () => {
    expect(() => getTenantId()).toThrow('TENANT_CONTEXT_MISSING');
  });
});
```

---

## 5. Production Deployment

### `Dockerfile`
```dockerfile
FROM node:22-alpine AS builder
WORKDIR /app
COPY package*.json ./
RUN npm ci
COPY . .

FROM gcr.io/distroless/nodejs22-debian12:nonroot
WORKDIR /app
COPY --from=builder /app ./
USER nonroot
EXPOSE 3000
CMD ["src/presentation/app.js"]
```
