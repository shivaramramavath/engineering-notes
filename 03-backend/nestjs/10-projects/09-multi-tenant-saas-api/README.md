# Project 09: Multi-Tenant SaaS API (`09-multi-tenant-saas-api`)

> Enterprise multi-tenant SaaS backend with PostgreSQL Row-Level Security (RLS), AsyncLocalStorage tenant resolution, and subscription tier enforcement.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-nest-09-multi-tenant-saas` |
| **Isolation Model** | Shared Database, Row-Level Security (RLS) |
| **Database** | PostgreSQL 16 + TypeORM 0.3+ |
| **Context** | Node.js AsyncLocalStorage |
| **Complexity Level** | Enterprise Production Grade |

---

## 1. Architectural Overview & PostgreSQL RLS

```sql
-- Enable Row Level Security on core domain tables
ALTER TABLE organizations ENABLE ROW LEVEL SECURITY;
ALTER TABLE projects ENABLE ROW LEVEL SECURITY;

CREATE POLICY tenant_isolation_projects ON projects
  USING (tenant_id = current_setting('app.current_tenant_id')::uuid);
```

```typescript
// src/tenancy/tenant.interceptor.ts
import { Injectable, NestInterceptor, ExecutionContext, CallHandler } from '@nestjs/common';
import { DataSource } from 'typeorm';
import { tenantContext } from './tenant-context';

@Injectable()
export class TenantInterceptor implements NestInterceptor {
  constructor(private readonly dataSource: DataSource) {}

  async intercept(context: ExecutionContext, next: CallHandler) {
    const req = context.switchToHttp().getRequest();
    const tenantId = req.headers['x-tenant-id'] || req.user?.tenantId;

    return tenantContext.run({ tenantId }, async () => {
      // Set session variable in PostgreSQL transaction
      await this.dataSource.query(`SET LOCAL app.current_tenant_id = '${tenantId}'`);
      return next.handle();
    });
  }
}
```
