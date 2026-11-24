# System Design Blueprint: Multi-Tenant Enterprise SaaS Platform

## Metadata

- **Topic**: Enterprise Multi-Tenancy Architecture, Isolation Models, Dynamic Database Routing & Resource Quotas
- **Scale Requirements**: 5,000 corporate tenants (100 Enterprise tier, 4,900 SMB tier), 500,000 daily active business users
- **Target SLA**: 100% strict cross-tenant data isolation (zero data leakage), noisy neighbor protection via rate limit quotas
- **Tech Stack**: FastAPI, PostgreSQL (Schema-per-tenant & Silo Databases), ContextVar, Redis (Tenant Quotas), Nginx / Envoy

---

## 1. Requirements & System Estimation

### 1.1 Tenant Isolation Strategies

| Isolation Model | Pros | Cons | Target Tier |
|:---|:---|:---|:---|
| **Shared DB, Shared Schema (Discriminator Column `tenant_id`)** | Lowest operational cost, simplest schema migrations | Risk of SQL bug leaking data across tenants; single noisy neighbor can impact all | Free / Starter Tier |
| **Shared DB, Separate Schemas (`SET search_path`)** | High data isolation, shared hardware costs, easy individual backup | Schema migration tooling overhead at 5,000 schemas | Professional / SMB Tier |
| **Database-per-Tenant (Isolated DB / Silo Instance)** | Absolute cryptographic & infrastructure isolation, tenant-specific encryption keys | Highest infrastructure & connection pool costs | Enterprise / Regulated Healthcare & Finance |

---

## 2. High-Level Architecture Diagram

```
                 +-----------------------------------------------+
                 |   Client Request (e.g. acme.enterprise.io)    |
                 +-----------------------+-----------------------+
                                         |
                                         v
                 +-----------------------+-----------------------+
                 | Envoy / Nginx Reverse Proxy                   |
                 | - Resolves Host Header to Tenant ID           |
                 +-----------------------+-----------------------+
                                         |
                                         v
                 +-----------------------+-----------------------+
                 | FastAPI SaaS Gateway (Stateless Pods)         |
                 | 1. MultiTenantMiddleware extracts tenant_id   |
                 | 2. Checks Redis Quota & Subscription Status   |
                 | 3. Binds tenant connection context (ContextVar)
                 +-----------------------+-----------------------+
                                         |
                       +-----------------+-----------------+
                       |                                   |
             [SMB Tier: Schema Routing]           [Enterprise: Silo Routing]
                       |                                   |
                       v                                   v
         +-------------+-------------+       +-------------+-------------+
         | Shared PostgreSQL Cluster |       | Dedicated Isolated DB     |
         | |-- schema: tenant_acme   |       | (Encrypted with Tenant    |
         | `-- schema: tenant_globex |       |  AWS KMS Key)             |
         +---------------------------+       +---------------------------+
```

---

## 3. Dynamic Database Routing via ContextVar

```python
from collections.abc import AsyncGenerator
from contextvars import ContextVar
from fastapi import Depends, FastAPI, HTTPException, Request
from starlette.middleware.base import BaseHTTPMiddleware
from sqlalchemy import text
from sqlalchemy.ext.asyncio import AsyncSession, async_sessionmaker, create_async_engine

current_tenant_cv: ContextVar[str | None] = ContextVar("current_tenant", default=None)

engine = create_async_engine("postgresql+asyncpg://postgres:pass@localhost:5432/saas_shared")
session_factory = async_sessionmaker(engine, expire_on_commit=False)


class TenantResolutionMiddleware(BaseHTTPMiddleware):
    async def dispatch(self, request: Request, call_next):
        # 1. Resolve tenant from subdomain or header
        host = request.headers.get("host", "")
        subdomain = host.split(".")[0] if "." in host else None
        tenant_id = request.headers.get("X-Tenant-ID") or subdomain

        if not tenant_id or tenant_id in ["www", "api"]:
            return await call_next(request)

        # 2. Bind to ContextVar
        token = current_tenant_cv.set(f"tenant_{tenant_id.lower()}")
        try:
            return await call_next(request)
        finally:
            current_tenant_cv.reset(token)


async def get_tenant_db() -> AsyncGenerator[AsyncSession, None]:
    tenant_schema = current_tenant_cv.get()
    if not tenant_schema:
        raise HTTPException(status_code=400, detail="Tenant context missing")

    async with session_factory() as session:
        # Enforce search_path isolation before yielding session
        await session.execute(text(f'SET search_path TO "{tenant_schema}", public'))
        try:
            yield session
        finally:
            await session.rollback()
```

---

## 4. Failure Modes, Scalability & Bottlenecks

| Failure Scenario | Impact | Mitigation Strategy |
|:---|:---|:---|
| **Noisy Neighbor Problem** | Heavy reporting query from 1 tenant starves DB pool for all 4,900 other tenants. | Implement **Tenant-Level Concurrency Semaphores in Redis**: Cap max concurrent active DB queries per tenant (e.g. max 10 concurrent queries for SMB). |
| **Schema Migration Drift** | Alembic migration fails halfway on schema #2,410 out of 5,000, leaving database in inconsistent state. | Implement **Parallel Multi-Tenant Migration Runner** with transactional rollbacks per schema and detailed audit checkpoints. |
| **Accidental Cross-Tenant Data Leak via Connection Reuse** | Asyncpg connection from pool retains previous tenant's `SET search_path`. | Always reset connection state (`DISCARD ALL` or `SET search_path TO public`) when releasing connection back to SQLAlchemy pool. |
