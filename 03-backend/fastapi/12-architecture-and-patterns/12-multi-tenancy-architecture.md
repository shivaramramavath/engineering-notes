---
title: "Multi-Tenancy Architecture in FastAPI Applications"
description: "Architect secure, scalable multi-tenant SaaS applications in FastAPI: compare shared schema with row-level security (RLS), schema-per-tenant, and database-per-tenant models, and implement tenant context propagation."
fastapi_version: "0.115.x+"
python_version: "3.12+"
pydantic_version: "v2.10+"
tested_with: "sqlalchemy 2.0+, pydantic 2.10+, fastapi 0.115+, pytest 8.3+"
free_threading_status: "Fully Compatible"
---

# Multi-Tenancy Architecture in FastAPI Applications

## 1. Conceptual Foundation & Mental Model

### The Multi-Tenant SaaS Imperative
Multi-tenancy is an architectural model in which a single software instance serves multiple distinct customer organizations (known as **Tenants**). The fundamental engineering challenge of multi-tenancy is **strict data isolation**: ensuring that Tenant A can never read, modify, or corrupt Tenant B's data under any circumstance, while maximizing resource utilization and keeping operational infrastructure costs low.

### The Three Multi-Tenancy Architectural Strategies

```
+---------------------------------------------------------------------------------+
| Strategy 1: Shared Database, Shared Schema (Discriminator Column)               |
| - Every table contains a `tenant_id` column.                                    |
| - Isolation enforced by application queries or PostgreSQL Row-Level Security.   |
| - Lowest infrastructure cost; highest operational scalability (100k+ tenants). |
+---------------------------------------------------------------------------------+
                                       |
                                       v
+---------------------------------------------------------------------------------+
| Strategy 2: Shared Database, Separate Schema (Schema-per-Tenant)                |
| - One database containing distinct PostgreSQL schemas (`tenant_a`, `tenant_b`). |
| - Queries switch namespaces via `SET search_path TO tenant_x`.                  |
| - Moderate isolation; higher Alembic migration complexity (caps at ~5k schemas).|
+---------------------------------------------------------------------------------+
                                       |
                                       v
+---------------------------------------------------------------------------------+
| Strategy 3: Separate Database (Database-per-Tenant)                             |
| - Each tenant possesses a completely isolated database instance or RDS cluster. |
| - Applications dynamically route connection pools based on tenant context.      |
| - Maximum security & compliance (HIPAA, PCI); highest infrastructure cost.      |
+---------------------------------------------------------------------------------+
```

---

## 2. Architecture & Internal Mechanics

Understanding how tenant identity is resolved from inbound HTTP requests and propagated through the FastAPI execution context to the database session.

```
[Inbound Request: https://acme.saas.com/api/v1/projects]
                       |
                       v
[Tenant Identification Middleware]
  - Resolves Tenant from:
    1. Subdomain: "acme"
    2. JWT Token Claim: {"tenant_id": "t_acme123"}
    3. Custom Header: X-Tenant-ID
                       |
                       v (Stores in Python contextvars)
[Request Execution Context: tenant_context.set("t_acme123")]
                       |
                       v
[FastAPI Dependency: get_db_session]
  - Injects Tenant Filter:
    Option A: Attaches `tenant_id` to query predicates automatically
    Option B: Executes `SET LOCAL search_path = tenant_schema;`
    Option C: Sets Postgres RLS session variable: `SET LOCAL app.current_tenant = 't_acme123';`
                       |
                       v
[PostgreSQL Database Engine]
  - Row-Level Security (RLS) policy blocks cross-tenant reads at the engine level!
```

---

## 3. Comprehensive Syntax & Core Mechanics

### Context Variable Propagation (`contextvars`)
Using Python's standard library `contextvars` to ensure asynchronous task-safe tenant propagation across coroutines without passing `tenant_id` manually through every function signature.

```python
import contextvars

tenant_context: contextvars.ContextVar[str] = contextvars.ContextVar(
    "tenant_context", default="public"
)

def get_current_tenant() -> str:
    return tenant_context.get()

def set_current_tenant(tenant_id: str) -> None:
    tenant_context.set(tenant_id)
```

---

## 4. Practical Implementation: 5 Verified Examples

### Example 1: Tenant Resolution Middleware (Subdomain & Header)
Extracting tenant identifiers from subdomains or headers and setting context variables.

```python
# app/core/tenancy/middleware.py
from starlette.middleware.base import BaseHTTPMiddleware
from starlette.requests import Request
from starlette.responses import JSONResponse
from app.core.tenancy.context import tenant_context

class TenantResolutionMiddleware(BaseHTTPMiddleware):
    async def dispatch(self, request: Request, call_next):
        # 1. Primary: Subdomain resolution (e.g., acme.myplatform.com)
        host = request.headers.get("host", "").split(":")[0]
        parts = host.split(".")
        tenant_id = None
        
        if len(parts) >= 3 and parts[0] != "www":
            tenant_id = parts[0]

        # 2. Secondary: Fallback to custom header (useful for mobile apps/dev)
        if not tenant_id:
            tenant_id = request.headers.get("X-Tenant-ID")

        # 3. Default to public/system if unspecified
        tenant_id = tenant_id or "public"

        # Validate tenant identifier format (prevent SQL injection in schema names)
        if not tenant_id.replace("_", "").replace("-", "").isalnum():
            return JSONResponse(
                status_code=400,
                content={"error": "INVALID_TENANT_ID", "detail": "Tenant identifier is malformed."}
            )

        # Set task-safe asynchronous context
        token = tenant_context.set(tenant_id)
        try:
            response = await call_next(request)
            response.headers["X-Resolved-Tenant"] = tenant_id
            return response
        finally:
            tenant_context.reset(token)
```

---

### Example 2: Strategy 1: Shared Database with PostgreSQL Row-Level Security (RLS)
PostgreSQL native Row-Level Security guarantees that even if a developer forgets a `WHERE tenant_id = ...` clause, the database itself denies access to rows belonging to other tenants.

```sql
-- DDL Migration for Row-Level Security
CREATE TABLE documents (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    tenant_id VARCHAR(64) NOT NULL,
    title VARCHAR(255) NOT NULL,
    content TEXT NOT NULL
);

-- Enable RLS on the table
ALTER TABLE documents ENABLE ROW LEVEL SECURITY;

-- Create policy enforcing isolation based on PostgreSQL session variable
CREATE POLICY tenant_isolation_policy ON documents
    USING (tenant_id = CURRENT_SETTING('app.current_tenant', true))
    WITH CHECK (tenant_id = CURRENT_SETTING('app.current_tenant', true));
```

```python
# app/infrastructure/db/rls_session.py
from sqlalchemy import text
from sqlalchemy.ext.asyncio import AsyncSession
from app.core.tenancy.context import get_current_tenant

async def configure_rls_session(session: AsyncSession) -> None:
    """Sets PostgreSQL session variable before query execution."""
    tenant_id = get_current_tenant()
    # SET LOCAL scopes the variable strictly to the current database transaction
    await session.execute(
        text("SET LOCAL app.current_tenant = :tenant"),
        {"tenant": tenant_id}
    )
```

---

### Example 3: Strategy 2: Schema-per-Tenant with Dynamic `search_path`
Each tenant has its own isolated schema. The database session sets `search_path` on connection checkout.

```python
# app/infrastructure/db/schema_session.py
from sqlalchemy import text
from sqlalchemy.ext.asyncio import AsyncSession, async_sessionmaker
from app.core.tenancy.context import get_current_tenant

class TenantSchemaManager:
    def __init__(self, session_factory: async_sessionmaker[AsyncSession]):
        self._session_factory = session_factory

    async def get_session(self) -> AsyncSession:
        tenant_id = get_current_tenant()
        session = self._session_factory()
        
        # Confine queries to the tenant's schema
        schema_name = f"tenant_{tenant_id}"
        await session.execute(text(f"SET LOCAL search_path TO {schema_name}, public"))
        return session
```

---

### Example 4: Tenant-Aware Base Model and Automatic Query Filtering
When using shared schemas without database RLS, an ORM event listener automatically attaches `tenant_id` to all insert and select queries.

```python
# app/infrastructure/orm/tenant_base.py
from sqlalchemy import Column, String, event
from sqlalchemy.orm import declarative_base, Session
from app.core.tenancy.context import get_current_tenant

Base = declarative_base()

class TenantAwareMixin:
    """Mixin adding tenant_id column and auto-injection."""
    tenant_id = Column(String(64), nullable=False, index=True)

class ProjectORM(Base, TenantAwareMixin):
    __tablename__ = "projects"
    id = Column(String(36), primary_key=True)
    name = Column(String(100), nullable=False)

# Auto-populate tenant_id on INSERT
@event.listens_for(Session, "before_flush")
def auto_populate_tenant_id(session, flush_context, instances):
    current_tenant = get_current_tenant()
    for instance in session.new:
        if isinstance(instance, TenantAwareMixin) and not instance.tenant_id:
            instance.tenant_id = current_tenant
```

---

### Example 5: FastAPI Router and Dependency Injection
Exposing clean endpoints where tenant context is completely transparent to the route handler.

```python
# app/api/v1/projects.py
from fastapi import APIRouter, Depends, HTTPException
from pydantic import BaseModel
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession
from app.core.tenancy.context import get_current_tenant
from app.infrastructure.orm.tenant_base import ProjectORM

router = APIRouter(prefix="/projects", tags=["Projects"])

class ProjectCreate(BaseModel):
    name: str

class ProjectResponse(BaseModel):
    id: str
    name: str
    tenant_id: str

# Database session dependency setting RLS / tenant context
async def get_tenant_db_session() -> AsyncSession:
    pass  # pragma: no cover

@router.post("/", response_model=ProjectResponse)
async def create_project(
    payload: ProjectCreate,
    session: AsyncSession = Depends(get_tenant_db_session),
):
    import uuid
    tenant_id = get_current_tenant()
    
    project = ProjectORM(
        id=str(uuid.uuid4()),
        name=payload.name,
        tenant_id=tenant_id
    )
    session.add(project)
    await session.commit()
    
    return ProjectResponse(id=project.id, name=project.name, tenant_id=project.tenant_id)

@router.get("/")
async def list_projects(
    session: AsyncSession = Depends(get_tenant_db_session),
):
    # If using RLS or query filters, this returns ONLY the current tenant's projects!
    tenant_id = get_current_tenant()
    stmt = select(ProjectORM).where(ProjectORM.tenant_id == tenant_id)
    result = await session.execute(stmt)
    records = result.scalars().all()
    return [{"id": p.id, "name": p.name} for p in records]
```

---

## 5. Edge Cases, Pitfalls & Failure Modes

1. **Context Leakage Across Async Coroutines**: Failing to reset context variables or using mutable objects (like lists or dictionaries) inside `ContextVar` causes tenant credentials to bleed across unrelated requests on the same event loop.
2. **Background Worker Tenant Disconnect**: Background tasks spawned via `BackgroundTasks` or Celery run outside the HTTP request lifecycle. If a background worker executes without explicitly restoring the originating `tenant_id`, queries execute against the wrong tenant or fail completely.
3. **Connection Pool Contamination (`SET search_path`)**: If a session executes `SET search_path TO tenant_a` without using `SET LOCAL` or without resetting the connection upon checkout/checkin, the physical connection returned to the pool remains bound to `tenant_a`. Subsequent requests for `tenant_b` that acquire that connection will query `tenant_a`'s data. Always use `SET LOCAL` within transactions or configure pool `reset_on_return`.
4. **Schema Migration Bottlenecks**: In the Schema-per-Tenant model, running database migrations across 5,000 schemas requires running Alembic 5,000 times, causing multi-hour deployment windows.

---

## 6. Common Mistakes Table

| Mistake | Root Cause | Impact | Correct Approach |
| :--- | :--- | :--- | :--- |
| **Trusting Client-Supplied `tenant_id`** | Accepting `?tenant_id=xxx` in body | IDOR vulnerability; any user can view any tenant's data. | Extract `tenant_id` cryptographically from validated JWT claims or subdomains. |
| **Using `SET search_path` Globally** | Omitting `LOCAL` or pool reset | Leaks tenant schema to subsequent requests sharing the pooled TCP connection. | Use `SET LOCAL search_path = ...` inside active transactions. |
| **Relying Only on App-Level Filters** | Forgetting `where(tenant_id=...)` | Developer error results in catastrophic cross-tenant data leakage. | Enforce database-level isolation via PostgreSQL Row-Level Security (RLS). |
| **Forgetting Context in Celery Tasks** | Background jobs run without HTTP headers | Background task mutates default or wrong tenant records. | Explicitly pass `tenant_id` as a task argument; initialize context in the worker. |
| **Creating 50,000 Schemas in One DB** | Over-scaling Schema-per-Tenant | PostgreSQL system catalog bloat; degraded query planner performance. | Use Shared Schema with RLS for systems exceeding 2,000 tenants. |
| **Using Global Variables for Tenant ID** | Concurrency misunderstanding | High-concurrency requests overwrite each other's active tenant IDs. | Use Python's standard `contextvars.ContextVar`. |

---

## 7. Performance & Scalability Considerations

- **Row-Level Security (RLS) Overhead**: PostgreSQL RLS introduces a negligible query planning overhead (~1–3%) while providing ironclad engine-level guarantees. Ensure composite indexes include `tenant_id` as the leading column: `CREATE INDEX idx_docs_tenant_created ON documents (tenant_id, created_at)`.
- **Connection Pool Sizing in Database-per-Tenant**: Sizing connection pools for 500 distinct databases causes connection pool explosion. Use dynamic on-demand connection caches with aggressive idle timeouts, or use PgBouncer.
- **Python 3.13+ Free-Threading**: Context variables in Python 3.13t are natively thread-safe and isolated per OS thread and async task group, allowing high-throughput multi-threaded tenant routing.

---

## 8. Security & OWASP Guardrails

- **OWASP A01 (Broken Object-Level Authorization - Multi-Tenant IDOR)**: Multi-tenancy isolation failure is the most catastrophic category of IDOR. Every database operation must be bounded by the resolved tenant context.
- **SQL Injection in Dynamic Schema Names**: When executing `SET search_path TO tenant_{name}`, never concatenate raw client strings. Validate schema names using strict alphanumeric regexes or `isidentifier()`.

---

## 9. Testing Strategies & Pytest Recipes

```python
# tests/test_multi_tenancy.py
import pytest
from app.core.tenancy.context import tenant_context, get_current_tenant

def test_tenant_context_isolation():
    token = tenant_context.set("tenant_alpha")
    assert get_current_tenant() == "tenant_alpha"

    # Nested context reset
    token_nested = tenant_context.set("tenant_beta")
    assert get_current_tenant() == "tenant_beta"
    tenant_context.reset(token_nested)

    assert get_current_tenant() == "tenant_alpha"
    tenant_context.reset(token)
```

---

## 10. Real-World Case Study / Production Incident

A project management SaaS platform utilized the Shared Schema strategy. An engineer implemented a new global search endpoint:
```python
@router.get("/search")
async def search(q: str, db: AsyncSession = Depends(get_db)):
    stmt = select(Task).where(Task.title.ilike(f"%{q}%"))
    return (await db.execute(stmt)).scalars().all()
```
The developer omitted the `where(Task.tenant_id == current_tenant.id)` predicate. When an enterprise customer searched for `"Q3 Budget"`, the endpoint returned confidential budget tasks from 40 other enterprise companies using the platform.

**The Post-Mortem & Fix**:
1. Within 24 hours, the engineering team enabled PostgreSQL Row-Level Security (RLS) across all tables:
   ```sql
   ALTER TABLE tasks ENABLE ROW LEVEL SECURITY;
   CREATE POLICY tenant_tasks_policy ON tasks USING (tenant_id = CURRENT_SETTING('app.current_tenant', true));
   ```
2. The FastAPI session dependency was configured to run `SET LOCAL app.current_tenant = :tenant` at transaction checkout.
3. Even with the omitted `where` clause in Python, PostgreSQL automatically filtered the results strictly to the requesting tenant, preventing any possibility of recurrence.

---

## 11. Technical Interview Questions

<details>
<summary>1. What are the three primary database multi-tenancy models, and what are their trade-offs?</summary>
1. **Shared Database, Shared Schema**: All tenants share tables, differentiated by `tenant_id`. Lowest cost and easiest migrations, but requires strict RLS or query filters to prevent leaks.
2. **Shared Database, Separate Schema**: Each tenant has their own schema namespace (`tenant_a`, `tenant_b`). Better isolation, but migrations are slow across thousands of schemas.
3. **Separate Database**: Each tenant has an independent database. Maximum compliance and security, but highest infrastructure cost and complex connection pool management.
</details>

<details>
<summary>2. How does PostgreSQL Row-Level Security (RLS) work in a multi-tenant FastAPI application?</summary>
PostgreSQL RLS enforces security policies at the database engine level. When a query is executed, PostgreSQL evaluates a declared policy (e.g., `USING (tenant_id = CURRENT_SETTING('app.current_tenant'))`). In FastAPI, the database session dependency executes `SET LOCAL app.current_tenant = :id` at the beginning of the transaction. The database automatically restricts all `SELECT`, `UPDATE`, and `DELETE` statements to rows matching that tenant, regardless of whether the application's SQL query included a tenant filter.
</details>

<details>
<summary>3. Why must `SET LOCAL` be used instead of `SET` when configuring session variables for multi-tenancy?</summary>
In modern web applications, database connections are managed via a connection pool (e.g., SQLAlchemy pool or PgBouncer). A single physical TCP connection is reused across many different HTTP requests. Using `SET search_path = ...` modifies the connection state permanently. If an error occurs and the connection is returned to the pool, the next request will inherit the previous tenant's schema. `SET LOCAL` confines the configuration strictly to the current `BEGIN ... COMMIT` transaction block.
</details>

<details>
<summary>4. Why is Python's `contextvars` module required for tenant context propagation instead of global variables?</summary>
Global variables are shared across the entire Python process. In an asynchronous ASGI application running on an event loop, multiple concurrent requests execute simultaneously. A global variable would be overwritten by whichever request executed last. `contextvars.ContextVar` provides task-local storage, ensuring that each concurrent async coroutine maintains its own isolated tenant context.
</details>

<details>
<summary>5. How should tenant context be propagated to asynchronous background workers (e.g., Celery or RQ)?</summary>
Because background workers execute outside the ASGI HTTP request pipeline, they do not have access to HTTP headers or subdomains. The originating HTTP endpoint must capture the active `tenant_id` from the context and pass it explicitly as an argument in the background task payload. The worker function's first action must be initializing the tenant context before performing database operations.
</details>

<details>
<summary>6. How can tenant subdomains be tested locally during development?</summary>
By mapping wildcard domains in the local `/etc/hosts` file (or using services like `lvh.me` or `nip.io`, which resolve all subdomains to `127.0.0.1`), or by allowing an override header (`X-Tenant-ID`) in development environments.
</details>

<details>
<summary>7. What is the impact of multi-tenancy on database index design?</summary>
In a shared-schema model, almost every database query begins with `tenant_id`. Therefore, composite indexes should almost always place `tenant_id` as the first column (e.g., `(tenant_id, created_at)` or `(tenant_id, status)`), allowing the database query planner to quickly narrow the index scan to the specific tenant's partition.
</details>

<details>
<summary>8. How do you handle tenant-specific database migrations in a Schema-per-Tenant model?</summary>
Using Alembic with dynamic multi-schema iteration. The migration script queries the database for all active tenant schema names and executes the migration upgrade loop against each schema sequentially, or applies migrations on-demand during tenant onboarding.
</details>

---

## 12. Practical Exercises & Solutions

<details>
<summary>Exercise 1 (Easy): Create a Task-Safe Tenant Context</summary>

**Task**: Create a module with `tenant_context` using `contextvars.ContextVar` with default `"public"`. Provide `get_tenant()` and `set_tenant(name: str)` helper functions.

**Solution**:
```python
import contextvars

tenant_context: contextvars.ContextVar[str] = contextvars.ContextVar(
    "tenant_context", default="public"
)

def get_tenant() -> str:
    return tenant_context.get()

def set_tenant(name: str) -> contextvars.Token:
    return tenant_context.set(name)
```
</details>

<details>
<summary>Exercise 2 (Easy): Extract Tenant from Subdomain</summary>

**Task**: Write a function `extract_subdomain(host: str) -> str | None` that returns `"acme"` from `"acme.saas.com"`, and returns `None` for `"www.saas.com"` or `"saas.com"`.

**Solution**:
```python
def extract_subdomain(host: str) -> str | None:
    clean_host = host.split(":")[0]  # Strip port
    parts = clean_host.split(".")
    if len(parts) >= 3 and parts[0] != "www":
        return parts[0]
    return None

assert extract_subdomain("acme.saas.com") == "acme"
assert extract_subdomain("www.saas.com") is None
assert extract_subdomain("saas.com") is None
```
</details>

<details>
<summary>Exercise 3 (Medium): Safe Schema Sanitizer</summary>

**Task**: Write a function `sanitize_schema_name(tenant_id: str) -> str` that validates the tenant ID contains only alphanumeric characters and underscores, raising `ValueError` if invalid, and returns `"tenant_{tenant_id}"`.

**Solution**:
```python
import re

def sanitize_schema_name(tenant_id: str) -> str:
    if not re.match(r"^[a-zA-Z0-9_]{1,32}$", tenant_id):
        raise ValueError(f"Invalid tenant identifier: '{tenant_id}'")
    return f"tenant_{tenant_id.lower()}"
```
</details>

<details>
<summary>Exercise 4 (Medium): Tenant Resolution ASGI Middleware</summary>

**Task**: Implement a simple Starlette middleware that extracts `X-Tenant-ID` header, sets the context variable, and resets it after the request completes.

**Solution**:
```python
from starlette.middleware.base import BaseHTTPMiddleware
from app.core.tenancy.context import tenant_context

class SimpleTenantMiddleware(BaseHTTPMiddleware):
    async def dispatch(self, request, call_next):
        tenant_id = request.headers.get("X-Tenant-ID", "public")
        token = tenant_context.set(tenant_id)
        try:
            return await call_next(request)
        finally:
            tenant_context.reset(token)
```
</details>

<details>
<summary>Exercise 5 (Hard): Row-Level Security Session Dependency</summary>

**Task**: Write an async SQLAlchemy session dependency that wraps the session in a transaction and executes `SET LOCAL app.current_tenant = :tenant` using the current context variable.

**Solution**:
```python
from sqlalchemy import text
from sqlalchemy.ext.asyncio import AsyncSession
from app.core.tenancy.context import get_tenant

async def get_rls_session(raw_session: AsyncSession):
    async with raw_session.begin():
        current_tenant = get_tenant()
        await raw_session.execute(
            text("SET LOCAL app.current_tenant = :tenant"),
            {"tenant": current_tenant}
        )
        yield raw_session
```
</details>

<details>
<summary>Exercise 6 (Hard): Multi-Tenant Query Interceptor in SQLAlchemy</summary>

**Task**: Implement a custom query compilation hook or select wrapper that automatically appends `.where(Entity.tenant_id == current_tenant)` to all select statements for entities inheriting from a `TenantMixin`.

**Solution**:
```python
from sqlalchemy import select
from app.core.tenancy.context import get_tenant

class TenantMixin:
    pass

def apply_tenant_filter(stmt, entity_cls):
    """Safely appends tenant filter to SQLAlchemy select statement."""
    if issubclass(entity_cls, TenantMixin):
        current_tenant = get_tenant()
        return stmt.where(entity_cls.tenant_id == current_tenant)
    return stmt

# Verification
# stmt = apply_tenant_filter(select(ProjectORM), ProjectORM)
```
</details>
