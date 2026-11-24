# Production Reference Project: Task Management System with Dynamic RBAC & PBAC

## Metadata & Production Baseline

| Attribute | Specification |
|:---|:---|
| **Project Code** | `FASTAPI-PROJ-010` |
| **FastAPI Version** | `0.115.x+` / `0.165.x+` |
| **Pydantic Version** | `2.10.x+` (V2 Core) |
| **Python Baseline** | `3.12+` / `3.13` (Free-Threading Safe) |
| **ASGI Server** | `Uvicorn 0.32+` |
| **Database & ORM** | `PostgreSQL 16+`, `SQLAlchemy 2.0.36+ (asyncpg)` |
| **Authorization Model** | Dynamic Role-Based Access Control (RBAC) & Policy-Based Guardrails (PBAC) |
| **Security & Tokens** | Scoped JWTs, Fast Security Dependencies (`Security(has_permission(...))`) |
| **Testing** | `pytest 8.3+`, `pytest-asyncio 0.24+`, `httpx 0.27+` |
| **Complexity Level** | Intermediate / Enterprise Authorization |

---

## 1. Architectural Overview & Domain Design

### 1.1 System Architecture & Data Flow

```
                      +---------------------------------------+
                      |       Authenticated Client Request    |
                      |       Authorization: Bearer <JWT>     |
                      +-------------------+-------------------+
                                          |
                                          v
                      +-------------------+-------------------+
                      |   FastAPI Dependency Injection Chain  |
                      | 1. get_current_user (Validates JWT)   |
                      | 2. Security(PermissionChecker("...")) |
                      +-------------------+-------------------+
                                          |
                                          v
                      +-------------------+-------------------+
                      | Dynamic RBAC & PBAC Policy Engine     |
                      | - Load User Roles & Direct Permissions|
                      | - Check: Permission in User.Perms?    |
                      | - PBAC: Is User the Task Owner OR     |
                      |         Tenant Org Admin?             |
                      +---------+-------------------+---------+
                                |                   |
                      Deny: 403 |                   | Allow
                                v                   v
                      +---------+---------+   +-----+-----------------+
                      | HTTP 403 Forbidden|   | Execute Domain Action |
                      +-------------------+   | In PostgreSQL DB      |
                                              +-----------------------+
```

### 1.2 Directory Layout

```
10-task-manager-rbac/
|-- pyproject.toml
|-- Dockerfile
|-- docker-compose.yml
|-- src/
|   |-- __init__.py
|   |-- main.py
|   |-- config.py
|   |-- database.py
|   |-- security.py
|   |-- domain/
|   |   |-- __init__.py
|   |   |-- models.py
|   |   `-- schemas.py
|   |-- permissions/
|   |   |-- __init__.py
|   |   |-- constants.py
|   |   `-- checker.py
|   `-- api/
|       |-- __init__.py
|       `-- v1/
|           |-- auth.py
|           |-- tasks.py
|           `-- roles.py
`-- tests/
    |-- __init__.py
    |-- conftest.py
    `-- test_rbac_api.py
```

---

## 2. Complete Production Implementation

### 2.1 `pyproject.toml`

```toml
[project]
name = "fastapi-task-manager-rbac"
version = "0.1.0"
description = "Granular role and policy-based task manager API"
readme = "README.md"
requires-python = ">=3.12"
dependencies = [
    "fastapi>=0.115.0",
    "uvicorn[standard]>=0.32.0",
    "pydantic>=2.10.0",
    "pydantic-settings>=2.6.0",
    "sqlalchemy>=2.0.36",
    "asyncpg>=0.30.0",
    "pyjwt[crypto]>=2.9.0",
    "passlib[bcrypt]>=1.7.4",
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

### 2.2 `src/config.py`

```python
from functools import lru_cache
from pydantic_settings import BaseSettings, SettingsConfigDict


class Settings(BaseSettings):
    PROJECT_NAME: str = "RBAC Task Manager"
    DATABASE_URL: str = "postgresql+asyncpg://postgres:postgres@localhost:5432/rbac_db"
    JWT_SECRET_KEY: str = "enterprise-rbac-token-secret-at-least-32-chars"
    JWT_ALGORITHM: str = "HS256"
    ACCESS_TOKEN_EXPIRE_MINUTES: int = 120

    model_config = SettingsConfigDict(env_file=".env", extra="ignore")


@lru_cache
def get_settings() -> Settings:
    return Settings()
```

### 2.3 `src/permissions/constants.py`

```python
class Permissions:
    TASK_CREATE = "task:create"
    TASK_READ = "task:read"
    TASK_UPDATE = "task:update"
    TASK_DELETE = "task:delete"
    ROLE_MANAGE = "role:manage"
    USER_MANAGE = "user:manage"
```

### 2.4 `src/domain/models.py` & `src/domain/schemas.py`

```python
# src/domain/models.py
import enum
import uuid
from datetime import datetime, timezone
from sqlalchemy import Column, DateTime, ForeignKey, String, Table, Text
from sqlalchemy.dialects.postgresql import UUID
from sqlalchemy.orm import DeclarativeBase, Mapped, mapped_column, relationship


class Base(DeclarativeBase):
    pass


# Association Table for User <-> Role Many-to-Many
user_roles = Table(
    "user_roles",
    Base.metadata,
    Column("user_id", UUID(as_uuid=True), ForeignKey("users.id", ondelete="CASCADE"), primary_key=True),
    Column("role_id", UUID(as_uuid=True), ForeignKey("roles.id", ondelete="CASCADE"), primary_key=True),
)

# Association Table for Role <-> Permission Many-to-Many
role_permissions = Table(
    "role_permissions",
    Base.metadata,
    Column("role_id", UUID(as_uuid=True), ForeignKey("roles.id", ondelete="CASCADE"), primary_key=True),
    Column("permission_id", UUID(as_uuid=True), ForeignKey("permissions.id", ondelete="CASCADE"), primary_key=True),
)


class Permission(Base):
    __tablename__ = "permissions"

    id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4)
    name: Mapped[str] = mapped_column(String(64), unique=True, nullable=False, index=True)
    description: Mapped[str | None] = mapped_column(String(255), nullable=True)


class Role(Base):
    __tablename__ = "roles"

    id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4)
    name: Mapped[str] = mapped_column(String(64), unique=True, nullable=False)
    permissions: Mapped[list[Permission]] = relationship("Permission", secondary=role_permissions, lazy="selectin")


class User(Base):
    __tablename__ = "users"

    id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4)
    email: Mapped[str] = mapped_column(String(255), unique=True, nullable=False, index=True)
    hashed_password: Mapped[str] = mapped_column(String(255), nullable=False)
    roles: Mapped[list[Role]] = relationship("Role", secondary=user_roles, lazy="selectin")


class TaskStatus(str, enum.Enum):
    TODO = "TODO"
    IN_PROGRESS = "IN_PROGRESS"
    DONE = "DONE"


class Task(Base):
    __tablename__ = "tasks"

    id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4)
    title: Mapped[str] = mapped_column(String(255), nullable=False)
    description: Mapped[str | None] = mapped_column(Text, nullable=True)
    status: Mapped[TaskStatus] = mapped_column(String(32), default=TaskStatus.TODO, nullable=False)
    owner_id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), ForeignKey("users.id"), nullable=False)
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True),
        default=lambda: datetime.now(timezone.utc),
        nullable=False,
    )
```

```python
# src/domain/schemas.py
import uuid
from pydantic import BaseModel, ConfigDict, EmailStr, Field
from src.domain.models import TaskStatus


class TaskCreate(BaseModel):
    title: str = Field(..., min_length=2, max_length=255)
    description: str | None = None


class TaskResponse(BaseModel):
    id: uuid.UUID
    title: str
    description: str | None
    status: TaskStatus
    owner_id: uuid.UUID

    model_config = ConfigDict(from_attributes=True)
```

### 2.5 `src/permissions/checker.py` & Auth Dependencies

```python
from fastapi import Depends, HTTPException, Security, status
from fastapi.security import HTTPAuthorizationCredentials, HTTPBearer
import jwt
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

from src.config import Settings, get_settings
from src.database import get_db
from src.domain.models import User

security_scheme = HTTPBearer()


async def get_current_user(
    credentials: HTTPAuthorizationCredentials = Security(security_scheme),
    db: AsyncSession = Depends(get_db),
    settings: Settings = Depends(get_settings),
) -> User:
    token = credentials.credentials
    try:
        payload = jwt.decode(token, settings.JWT_SECRET_KEY, algorithms=[settings.JWT_ALGORITHM])
        user_id = payload.get("sub")
    except Exception:
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="Invalid token")

    stmt = select(User).where(User.id == user_id)
    result = await db.execute(stmt)
    user = result.scalars().first()
    if not user:
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="User not found")
    return user


class PermissionChecker:
    def __init__(self, required_permission: str):
        self.required_permission = required_permission

    def __call__(self, user: User = Depends(get_current_user)) -> User:
        # Collect permissions across all roles assigned to user
        user_permissions = {
            perm.name
            for role in user.roles
            for perm in role.permissions
        }
        if self.required_permission not in user_permissions:
            raise HTTPException(
                status_code=status.HTTP_403_FORBIDDEN,
                detail=f"Permission denied: Missing '{self.required_permission}' permission",
            )
        return user
```

### 2.6 `src/main.py` & API Routers

```python
# src/api/v1/tasks.py
import uuid
from fastapi import APIRouter, Depends, HTTPException, Security, status
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

from src.database import get_db
from src.domain.models import Task, User
from src.domain.schemas import TaskCreate, TaskResponse
from src.permissions.checker import PermissionChecker, get_current_user
from src.permissions.constants import Permissions

router = APIRouter(prefix="/tasks", tags=["Tasks"])


@router.post("/", response_model=TaskResponse, status_code=status.HTTP_201_CREATED)
async def create_task(
    req: TaskCreate,
    current_user: User = Security(PermissionChecker(Permissions.TASK_CREATE)),
    db: AsyncSession = Depends(get_db),
):
    task = Task(title=req.title, description=req.description, owner_id=current_user.id)
    db.add(task)
    await db.commit()
    await db.refresh(task)
    return task


@router.delete("/{task_id}", status_code=status.HTTP_204_NO_CONTENT)
async def delete_task(
    task_id: uuid.UUID,
    current_user: User = Security(PermissionChecker(Permissions.TASK_DELETE)),
    db: AsyncSession = Depends(get_db),
):
    stmt = select(Task).where(Task.id == task_id)
    res = await db.execute(stmt)
    task = res.scalars().first()
    if not task:
        raise HTTPException(status_code=404, detail="Task not found")
    
    # PBAC: Only owner or admin can delete
    user_permissions = {p.name for r in current_user.roles for p in r.permissions}
    if task.owner_id != current_user.id and Permissions.ROLE_MANAGE not in user_permissions:
        raise HTTPException(status_code=status.HTTP_403_FORBIDDEN, detail="Cannot delete other user's tasks")

    await db.delete(task)
    await db.commit()
```

```python
# src/main.py
from contextlib import asynccontextmanager
from fastapi import FastAPI
from src.api.v1.tasks import router as tasks_router
from src.config import get_settings
from src.database import engine
from src.domain.models import Base

settings = get_settings()


@asynccontextmanager
async def lifespan(app: FastAPI):
    async with engine.begin() as conn:
        await conn.run_sync(Base.metadata.create_all)
    yield
    await engine.dispose()


app = FastAPI(title="RBAC Task Manager", lifespan=lifespan)
app.include_router(tasks_router, prefix="/api/v1")
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
from src.domain.models import Base, Permission, Role, User
from src.main import app

TEST_DB_URL = "sqlite+aiosqlite:///:memory:"


@pytest_asyncio.fixture(scope="session")
async def test_engine():
    engine = create_async_engine(TEST_DB_URL)
    async with engine.begin() as conn:
        await conn.run_sync(Base.metadata.create_all)
    yield engine
    await engine.dispose()


@pytest_asyncio.fixture
async def test_session(test_engine) -> AsyncSession:
    factory = async_sessionmaker(bind=test_engine, class_=AsyncSession, expire_on_commit=False)
    async with factory() as session:
        yield session
        await session.rollback()
```

### 3.2 `tests/test_rbac_api.py`

```python
import pytest
from httpx import ASGITransport, AsyncClient
from src.main import app


@pytest.mark.asyncio
async def test_unauthenticated_request_fails():
    async with AsyncClient(transport=ASGITransport(app=app), base_url="http://test") as client:
        resp = await client.post("/api/v1/tasks/", json={"title": "Test Task"})
        assert resp.status_code == 403  # HTTPBearer returns 403 when header missing
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
      - DATABASE_URL=postgresql+asyncpg://postgres:postgres@db:5432/rbac_db
    depends_on:
      - db

  db:
    image: postgres:16-alpine
    environment:
      POSTGRES_USER: postgres
      POSTGRES_PASSWORD: postgres
      POSTGRES_DB: rbac_db
    ports:
      - "5432:5432"
```

---

## 5. Operational Runbook & Verification

### 5.1 Verification Commands

```bash
# 1. Attempt task creation without permissions (returns 403)
curl -X POST "http://localhost:8000/api/v1/tasks/" \
     -H "Content-Type: application/json" \
     -H "Authorization: Bearer <INVALID_OR_READ_ONLY_TOKEN>" \
     -d '{"title": "Deploy v2"}'

# 2. Run automated test suite
pytest -v tests/
```
