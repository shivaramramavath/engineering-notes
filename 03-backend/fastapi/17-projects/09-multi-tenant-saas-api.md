# Production Reference Project: Multi-Tenant SaaS Architecture with Schema Isolation

## Metadata & Production Baseline

| Attribute | Specification |
|:---|:---|
| **Project Code** | `FASTAPI-PROJ-009` |
| **FastAPI Version** | `0.115.x+` / `0.165.x+` |
| **Pydantic Version** | `2.10.x+` (V2 Core) |
| **Python Baseline** | `3.12+` / `3.13` (Free-Threading Safe) |
| **ASGI Server** | `Uvicorn 0.32+` |
| **Database & ORM** | `PostgreSQL 16+`, `SQLAlchemy 2.0.36+ (asyncpg)` with Dynamic PostgreSQL Schema Switching (`SET search_path`) |
| **Isolation Model** | Schema-per-Tenant (Isolated Postgres Schemas, Shared Database Instance) |
| **Tenant Resolution** | Subdomain (`tenant1.app.com`) or `X-Tenant-ID` Header with Middleware ContextVar propagation |
| **Testing** | `pytest 8.3+`, `pytest-asyncio 0.24+`, `httpx 0.27+` |
| **Complexity Level** | Advanced / Enterprise Multi-Tenancy |

---

## 1. Architectural Overview & Domain Design

### 1.1 System Architecture & Data Flow

```
                      +---------------------------------------+
                      | Client Request (tenant-a.saas.io)     |
                      | Header: X-Tenant-ID: tenant_a         |
                      +-------------------+-------------------+
                                          |
                                          v
                      +-------------------+-------------------+
                      | MultiTenantMiddleware                 |
                      | 1. Extract tenant identifier          |
                      | 2. Verify tenant exists in Shared DB  |
                      | 3. Set ContextVar(tenant_schema)      |
                      +-------------------+-------------------+
                                          |
                                          v
                      +-------------------+-------------------+
                      | Database Session Dependency           |
                      | - Acquire connection from async pool  |
                      | - Execute: SET search_path TO         |
                      |            tenant_a, public           |
                      +-------------------+-------------------+
                                          |
                                          v
                      +-------------------+-------------------+
                      | Domain Repositories & Services        |
                      | Run standard queries:                 |
                      | "SELECT * FROM users"                 |
                      | (Automatically queries tenant_a.users)|
                      +-------------------+-------------------+
                                          |
                                          v
                      +-------------------+-------------------+
                      | PostgreSQL Server                     |
                      | |-- public (tenants, billing, auth)   |
                      | |-- tenant_a (users, projects, items) |
                      | `-- tenant_b (users, projects, items) |
                      +---------------------------------------+
```

### 1.2 Directory Layout

```
09-multi-tenant-saas/
|-- pyproject.toml
|-- Dockerfile
|-- docker-compose.yml
|-- src/
|   |-- __init__.py
|   |-- main.py
|   |-- config.py
|   |-- database.py
|   |-- tenant_context.py
|   |-- middleware/
|   |   |-- __init__.py
|   |   `-- tenant_middleware.py
|   |-- domain/
|   |   |-- __init__.py
|   |   |-- shared_models.py
|   |   |-- tenant_models.py
|   |   `-- schemas.py
|   |-- services/
|   |   |-- __init__.py
|   |   |-- provisioner.py
|   |   `-- project_service.py
|   `-- api/
|       |-- __init__.py
|       `-- v1/
|           |-- tenants.py
|           `-- projects.py
`-- tests/
    |-- __init__.py
    |-- conftest.py
    `-- test_multi_tenancy.py
```

---

## 2. Complete Production Implementation

### 2.1 `pyproject.toml`

```toml
[project]
name = "fastapi-multi-tenant-saas"
version = "0.1.0"
description = "Multi-tenant SaaS API with PostgreSQL schema isolation and ContextVar routing"
readme = "README.md"
requires-python = ">=3.12"
dependencies = [
    "fastapi>=0.115.0",
    "uvicorn[standard]>=0.32.0",
    "pydantic>=2.10.0",
    "pydantic-settings>=2.6.0",
    "sqlalchemy>=2.0.36",
    "asyncpg>=0.30.0",
]

[project.optional-dependencies]
dev = [
    "pytest>=8.3.0",
    "pytest-asyncio>=0.24.0",
    "httpx>=0.27.0",
]

[build-system]
requires = ["hatchling"]
build-backend = "hatchling.build"
```

### 2.2 `src/tenant_context.py` & `src/database.py`

```python
# src/tenant_context.py
from contextvars import ContextVar

tenant_context: ContextVar[str | None] = ContextVar("tenant_context", default=None)


def get_current_tenant() -> str:
    tenant = tenant_context.get()
    if not tenant:
        raise RuntimeError("No tenant context active for current request.")
    return tenant


def set_current_tenant(tenant: str) -> None:
    tenant_context.set(tenant)
```

```python
# src/database.py
from collections.abc import AsyncGenerator
import re
from sqlalchemy import text
from sqlalchemy.ext.asyncio import AsyncSession, async_sessionmaker, create_async_engine
from src.config import get_settings
from src.tenant_context import tenant_context

settings = get_settings()

engine = create_async_engine(
    settings.DATABASE_URL,
    pool_size=20,
    max_overflow=10,
    pool_pre_ping=True,
)

async_session_factory = async_sessionmaker(
    bind=engine,
    class_=AsyncSession,
    expire_on_commit=False,
)

SCHEMA_NAME_PATTERN = re.compile(r"^[a-zA-Z0-9_]{3,32}$")


async def get_db() -> AsyncGenerator[AsyncSession, None]:
    async with async_session_factory() as session:
        # Retrieve active tenant schema from ContextVar
        current_tenant = tenant_context.get()
        if current_tenant:
            # Prevent SQL injection on search_path
            if not SCHEMA_NAME_PATTERN.match(current_tenant):
                raise ValueError("Invalid schema identifier format.")
            # Set search_path to isolated schema
            await session.execute(text(f'SET search_path TO "{current_tenant}", public'))
        try:
            yield session
        finally:
            await session.rollback()
```

### 2.3 `src/domain/shared_models.py` & `src/domain/tenant_models.py`

```python
# src/domain/shared_models.py (Lives in public schema)
import uuid
from datetime import datetime, timezone
from sqlalchemy import Boolean, DateTime, String
from sqlalchemy.dialects.postgresql import UUID
from sqlalchemy.orm import DeclarativeBase, Mapped, mapped_column


class SharedBase(DeclarativeBase):
    pass


class Tenant(SharedBase):
    __tablename__ = "tenants"
    __table_args__ = {"schema": "public"}

    id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4)
    name: Mapped[str] = mapped_column(String(100), nullable=False)
    slug: Mapped[str] = mapped_column(String(32), unique=True, nullable=False, index=True)
    schema_name: Mapped[str] = mapped_column(String(32), unique=True, nullable=False)
    is_active: Mapped[bool] = mapped_column(Boolean, default=True, nullable=False)
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True),
        default=lambda: datetime.now(timezone.utc),
        nullable=False,
    )
```

```python
# src/domain/tenant_models.py (Lives inside isolated tenant schema)
import uuid
from datetime import datetime, timezone
from sqlalchemy import DateTime, String, Text
from sqlalchemy.dialects.postgresql import UUID
from sqlalchemy.orm import DeclarativeBase, Mapped, mapped_column


class TenantBase(DeclarativeBase):
    pass


class Project(TenantBase):
    __tablename__ = "projects"

    id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4)
    name: Mapped[str] = mapped_column(String(128), nullable=False)
    description: Mapped[str | None] = mapped_column(Text, nullable=True)
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True),
        default=lambda: datetime.now(timezone.utc),
        nullable=False,
    )
```

### 2.4 `src/services/provisioner.py` (Dynamic Schema Provisioning)

```python
import re
from sqlalchemy import text
from sqlalchemy.ext.asyncio import AsyncSession
from src.database import engine
from src.domain.shared_models import Tenant
from src.domain.tenant_models import TenantBase


class TenantProvisioner:
    @staticmethod
    async def provision_tenant(session: AsyncSession, name: str, slug: str) -> Tenant:
        schema_name = f"tenant_{slug.lower().replace('-', '_')}"
        if not re.match(r"^[a-zA-Z0-9_]{3,32}$", schema_name):
            raise ValueError("Slug contains illegal characters.")

        tenant = Tenant(name=name, slug=slug, schema_name=schema_name)
        session.add(tenant)
        await session.commit()
        await session.refresh(tenant)

        # Create Schema and DDL tables
        async with engine.begin() as conn:
            await conn.execute(text(f'CREATE SCHEMA IF NOT EXISTS "{schema_name}"'))
            # Set search_path temporarily to execute tenant DDL
            await conn.execute(text(f'SET search_path TO "{schema_name}"'))
            await conn.run_sync(TenantBase.metadata.create_all)
            await conn.execute(text('SET search_path TO public'))

        return tenant
```

### 2.5 `src/middleware/tenant_middleware.py`

```python
from starlette.middleware.base import BaseHTTPMiddleware
from starlette.requests import Request
from starlette.responses import JSONResponse
from src.tenant_context import tenant_context


class MultiTenantMiddleware(BaseHTTPMiddleware):
    async def dispatch(self, request: Request, call_next):
        # Exclude docs and root tenant management paths
        if request.url.path.startswith(("/docs", "/openapi.json", "/redoc", "/api/v1/tenants")):
            return await call_next(request)

        # Resolve tenant from header or subdomain
        tenant_id = request.headers.get("X-Tenant-ID")
        if not tenant_id:
            # Subdomain fallback: acme.saas.com -> acme
            host = request.headers.get("host", "")
            parts = host.split(".")
            if len(parts) >= 3:
                tenant_id = parts[0]

        if not tenant_id:
            return JSONResponse(
                status_code=400,
                content={"detail": "Missing X-Tenant-ID header or tenant subdomain"},
            )

        # Bind tenant to async context
        token = tenant_context.set(f"tenant_{tenant_id.lower().replace('-', '_')}")
        try:
            response = await call_next(request)
            return response
        finally:
            tenant_context.reset(token)
```

### 2.6 `src/main.py` & API Routers

```python
# src/api/v1/tenants.py
from fastapi import APIRouter, Depends, HTTPException, status
from pydantic import BaseModel
from sqlalchemy.ext.asyncio import AsyncSession
from src.database import get_db
from src.services.provisioner import TenantProvisioner

router = APIRouter(prefix="/tenants", tags=["Tenants"])


class CreateTenantRequest(BaseModel):
    name: str
    slug: str


@router.post("/", status_code=status.HTTP_201_CREATED)
async def create_tenant(req: CreateTenantRequest, db: AsyncSession = Depends(get_db)):
    try:
        tenant = await TenantProvisioner.provision_tenant(db, req.name, req.slug)
        return {"id": tenant.id, "name": tenant.name, "schema": tenant.schema_name}
    except Exception as exc:
        raise HTTPException(status_code=400, detail=str(exc))
```

```python
# src/api/v1/projects.py
from fastapi import APIRouter, Depends, status
from pydantic import BaseModel
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession
from src.database import get_db
from src.domain.tenant_models import Project

router = APIRouter(prefix="/projects", tags=["Tenant Projects"])


class CreateProjectRequest(BaseModel):
    name: str
    description: str | None = None


@router.post("/", status_code=status.HTTP_201_CREATED)
async def create_project(req: CreateProjectRequest, db: AsyncSession = Depends(get_db)):
    project = Project(name=req.name, description=req.description)
    db.add(project)
    await db.commit()
    await db.refresh(project)
    return project


@router.get("/")
async def list_projects(db: AsyncSession = Depends(get_db)):
    stmt = select(Project)
    result = await db.execute(stmt)
    return result.scalars().all()
```

```python
# src/main.py
from contextlib import asynccontextmanager
from fastapi import FastAPI
from src.api.v1.projects import router as projects_router
from src.api.v1.tenants import router as tenants_router
from src.config import get_settings
from src.database import engine
from src.domain.shared_models import SharedBase
from src.middleware.tenant_middleware import MultiTenantMiddleware

settings = get_settings()


@asynccontextmanager
async def lifespan(app: FastAPI):
    async with engine.begin() as conn:
        await conn.run_sync(SharedBase.metadata.create_all)
    yield
    await engine.dispose()


app = FastAPI(title="Multi-Tenant SaaS API", lifespan=lifespan)
app.add_middleware(MultiTenantMiddleware)

app.include_router(tenants_router, prefix="/api/v1")
app.include_router(projects_router, prefix="/api/v1")
```

---

## 3. Automated Testing Suite

### 3.1 `tests/conftest.py`

```python
import pytest
import pytest_asyncio
from httpx import ASGITransport, AsyncClient
from sqlalchemy.ext.asyncio import AsyncSession, async_sessionmaker, create_async_engine
from src.database import get_db
from src.domain.shared_models import SharedBase
from src.main import app

TEST_DB_URL = "sqlite+aiosqlite:///:memory:"


@pytest_asyncio.fixture(scope="session")
async def test_engine():
    engine = create_async_engine(TEST_DB_URL)
    async with engine.begin() as conn:
        await conn.run_sync(SharedBase.metadata.create_all)
    yield engine
    await engine.dispose()


@pytest_asyncio.fixture
async def test_session(test_engine) -> AsyncSession:
    factory = async_sessionmaker(bind=test_engine, class_=AsyncSession, expire_on_commit=False)
    async with factory() as session:
        yield session
        await session.rollback()


@pytest_asyncio.fixture
async def client(test_session):
    app.dependency_overrides[get_db] = lambda: test_session
    async with AsyncClient(transport=ASGITransport(app=app), base_url="http://test") as ac:
        yield ac
    app.dependency_overrides.clear()
```

### 3.2 `tests/test_multi_tenancy.py`

```python
import pytest
from httpx import AsyncClient


@pytest.mark.asyncio
async def test_tenant_header_requirement(client: AsyncClient):
    # Requesting projects without X-Tenant-ID should fail with 400
    resp = await client.get("/api/v1/projects/")
    assert resp.status_code == 400
    assert "Missing X-Tenant-ID" in resp.json()["detail"]
```

---

## 4. Production Infrastructure & Deployment

### 4.1 Multi-Stage `Dockerfile`

```dockerfile
FROM python:3.12-slim-bookworm AS builder
WORKDIR /build
RUN apt-get update && apt-get install -y --no-install-recommends gcc libpq-dev && rm -rf /var/lib/apt/lists/*
RUN pip install --no-cache-dir hatchling
COPY pyproject.toml .
RUN pip wheel --no-cache-dir --wheel-dir /build/wheels .

FROM python:3.12-slim-bookworm AS runtime
WORKDIR /app
RUN apt-get update && apt-get install -y --no-install-recommends libpq5 curl && rm -rf /var/lib/apt/lists/*
COPY --from=builder /build/wheels /wheels
RUN pip install --no-cache-dir /wheels/*
COPY src/ /app/src/

USER 1001
EXPOSE 8000
CMD ["uvicorn", "src.main:app", "--host", "0.0.0.0", "--port", "8000", "--workers", "4"]
```

### 4.2 `docker-compose.yml`

```yaml
version: "3.9"

services:
  web:
    build: .
    ports:
      - "8000:8000"
    environment:
      - DATABASE_URL=postgresql+asyncpg://postgres:postgres@db:5432/saas_db
    depends_on:
      db:
        condition: service_healthy

  db:
    image: postgres:16-alpine
    environment:
      POSTGRES_USER: postgres
      POSTGRES_PASSWORD: postgres
      POSTGRES_DB: saas_db
    ports:
      - "5432:5432"
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U postgres"]
      interval: 5s
      timeout: 5s
      retries: 5
```

---

## 5. Operational Runbook & Verification

### 5.1 Verification Commands

```bash
# 1. Provision new tenant 'Acme Corp'
curl -X POST "http://localhost:8000/api/v1/tenants/" \
     -H "Content-Type: application/json" \
     -d '{"name": "Acme Corp", "slug": "acme"}'

# 2. Create project inside Acme's isolated schema
curl -X POST "http://localhost:8000/api/v1/projects/" \
     -H "Content-Type: application/json" \
     -H "X-Tenant-ID: acme" \
     -d '{"name": "Project Apollo", "description": "Classified rocket docs"}'

# 3. List projects for Acme
curl -X GET "http://localhost:8000/api/v1/projects/" \
     -H "X-Tenant-ID: acme"

# 4. List projects for another tenant (verify complete data isolation)
curl -X GET "http://localhost:8000/api/v1/projects/" \
     -H "X-Tenant-ID: globex"

# 5. Run automated tests
pytest -v tests/
```
