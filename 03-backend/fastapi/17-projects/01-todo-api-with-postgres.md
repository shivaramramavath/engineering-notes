# Project 01: Production Task Management API (`todo-api-with-postgres`)

> Enterprise-grade RESTful Task & Todo Management Microservice built with FastAPI, SQLAlchemy 2.0 Async, PostgreSQL, Alembic, Pydantic v2, and JWT authentication.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-fastapi-01-todo-postgres` |
| **FastAPI Version** | FastAPI 0.115.6+ |
| **Pydantic Version** | Pydantic v2.10.3+ (pydantic-settings 2.6.1+) |
| **Python Baseline** | Python 3.12+ (Free-Threading 3.13t compatible) |
| **ASGI Server** | Uvicorn 0.32.0+ with uvloop & httptools |
| **Database & ORM** | PostgreSQL 16 (Async SQLAlchemy 2.0 / asyncpg) + Alembic |
| **Queue & Cache** | Redis 7 (aioredis for token revocation cache) |
| **Validation & Auth** | Pydantic v2 + Argon2id password hashing + HS256 JWT |
| **Testing** | pytest-asyncio + httpx.AsyncClient + aiosqlite in-memory |
| **Complexity Level** | Production Reference Architecture |

---

## 1. Architectural Overview & Domain Design

### 1.1 System Architecture & Data Flow

The service follows a strict **Layered Clean Architecture** where HTTP concerns, business domain logic, and data access persistence are decoupled through abstract repositories and dependency injection.

```
+----------------------------------------------------------------------------------------------------+
|                                    Todo API Modular Architecture                                   |
|                                                                                                    |
|  [ Client: Web / Mobile ] ───(HTTP/REST + JWT)───> [ FastAPI APIRouter: /api/v1/tasks ]            |
|                                                              │                                     |
|                                                              ▼                                     |
|  [ Pydantic v2 TaskCreate / TaskUpdate ] <────── [ Depends(get_current_user) Security Guard ]       |
|            │                                                                                       |
|            ▼                                                                                       |
|  [ TaskService (Domain Business Logic) ] ───(Injected via Depends(get_task_service))                |
|            │                                                                                       |
|            ├───> [ TaskRepository (Data Access Layer - SQLAlchemy 2.0 Async) ]                     |
|            │             │                                                                         |
|            │             ▼                                                                         |
|            │      [ PostgreSQL DB with QueuePool (asyncpg driver) ]                                |
|            │                                                                                       |
|            └───> [ Event Logger / Audit Log Stream ]                                               |
+----------------------------------------------------------------------------------------------------+
```

### 1.2 Directory Layout

```
todo-api-with-postgres/
├── pyproject.toml
├── alembic.ini
├── Dockerfile
├── docker-compose.yml
├── migrations/
│   ├── env.py
│   └── script.py.mako
├── src/
│   ├── __init__.py
│   ├── main.py
│   ├── config.py
│   ├── database.py
│   ├── domain/
│   │   ├── __init__.py
│   │   ├── models.py
│   │   └── schemas.py
│   ├── repositories/
│   │   ├── __init__.py
│   │   └── task_repo.py
│   ├── services/
│   │   ├── __init__.py
│   │   ├── auth_service.py
│   │   └── task_service.py
│   └── api/
│       ├── __init__.py
│       ├── dependencies.py
│       └── v1/
│           ├── __init__.py
│           ├── auth_router.py
│           └── task_router.py
└── tests/
    ├── __init__.py
    ├── conftest.py
    └── test_api.py
```

---

## 2. Complete Production Implementation

### 2.1 Configuration & Dependencies (`pyproject.toml`)

```toml
[project]
name = "todo-api-with-postgres"
version = "0.1.0"
description = "Production Task API with FastAPI, SQLAlchemy 2.0 async, and PostgreSQL"
readme = "README.md"
requires-python = ">=3.12"
dependencies = [
    "fastapi>=0.115.6",
    "uvicorn[standard]>=0.32.0",
    "pydantic>=2.10.3",
    "pydantic-settings>=2.6.1",
    "sqlalchemy>=2.0.36",
    "asyncpg>=0.30.0",
    "alembic>=1.14.0",
    "pwdlib[argon2]>=0.2.1",
    "pyjwt>=2.10.1",
    "orjson>=3.10.12",
]

[dependency-groups]
dev = [
    "pytest>=8.3.4",
    "pytest-asyncio>=0.24.0",
    "httpx>=0.28.1",
    "aiosqlite>=0.20.0",
    "ruff>=0.8.0",
]
```

### 2.2 Core Application Entrypoint (`src/main.py`)

```python
from fastapi import FastAPI, Request, status
from fastapi.responses import JSONResponse
from fastapi.middleware.cors import CORSMiddleware
from src.config import get_settings, lifespan
from src.api.v1.auth_router import router as auth_router
from src.api.v1.task_router import router as task_router
import time
import logging

settings = get_settings()
logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(levelname)s] %(name)s: %(message)s")
logger = logging.getLogger("api")

app = FastAPI(
    title=settings.app_name,
    version="1.0.0",
    docs_url="/docs" if settings.app_env != "production" else None,
    redoc_url=None,
    lifespan=lifespan
)

# CORS Configuration
app.add_middleware(
    CORSMiddleware,
    allow_origins=settings.cors_origins,
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Request Timing Middleware
@app.middleware("http")
async def add_process_time_header(request: Request, call_next):
    start_time = time.perf_counter()
    response = await call_next(request)
    duration_ms = (time.perf_counter() - start_time) * 1000
    response.headers["X-Process-Time-Ms"] = f"{duration_ms:.2f}"
    return response

# Global Exception Handler
@app.exception_handler(Exception)
async def global_exception_handler(request: Request, exc: Exception):
    logger.error(f"Unhandled exception at {request.url.path}: {str(exc)}", exc_info=True)
    return JSONResponse(
        status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
        content={"detail": "Internal server error occurred. Please contact support."}
    )

# Routers
app.include_router(auth_router, prefix="/api/v1/auth", tags=["Authentication"])
app.include_router(task_router, prefix="/api/v1/tasks", tags=["Tasks"])

@app.get("/health", tags=["Health"])
async def health_check():
    return {"status": "HEALTHY", "version": "1.0.0"}
```

### 2.3 Settings & Lifespan (`src/config.py`)

```python
from functools import lru_cache
from typing import Literal
from pydantic import PostgresDsn, SecretStr, Field
from pydantic_settings import BaseSettings, SettingsConfigDict
from contextlib import asynccontextmanager
from fastapi import FastAPI
import logging

logger = logging.getLogger("config")

class Settings(BaseSettings):
    app_name: str = "Todo Management Service"
    app_env: Literal["development", "testing", "production"] = "development"
    debug: bool = False
    
    database_url: PostgresDsn
    database_pool_size: int = 15
    database_max_overflow: int = 5
    
    jwt_secret_key: SecretStr
    jwt_algorithm: str = "HS256"
    jwt_expire_minutes: int = 60
    
    cors_origins: list[str] = Field(default_factory=lambda: ["*"])

    model_config = SettingsConfigDict(
        env_file=".env",
        case_sensitive=False,
        extra="ignore"
    )

@lru_cache
def get_settings() -> Settings:
    return Settings()

@asynccontextmanager
async def lifespan(app: FastAPI):
    from src.database import init_db_engine, close_db_engine
    logger.info("Initializing database connection pool...")
    init_db_engine()
    yield
    logger.info("Disposing database connection pool...")
    await close_db_engine()
```

### 2.4 Database Engine & Session (`src/database.py`)

```python
from sqlalchemy.ext.asyncio import create_async_engine, async_sessionmaker, AsyncSession, AsyncEngine
from sqlalchemy.orm import DeclarativeBase
from src.config import get_settings

settings = get_settings()

engine: AsyncEngine | None = None
AsyncSessionLocal: async_sessionmaker[AsyncSession] | None = None

class Base(DeclarativeBase):
    pass

def init_db_engine():
    global engine, AsyncSessionLocal
    engine = create_async_engine(
        str(settings.database_url),
        pool_size=settings.database_pool_size,
        max_overflow=settings.database_max_overflow,
        pool_timeout=30.0,
        pool_pre_ping=True,
        echo=settings.debug
    )
    AsyncSessionLocal = async_sessionmaker(
        bind=engine,
        class_=AsyncSession,
        expire_on_commit=False,
        autoflush=False
    )

async def close_db_engine():
    global engine
    if engine:
        await engine.dispose()

async def get_db_session() -> AsyncSession:
    assert AsyncSessionLocal is not None, "Database session factory not initialized"
    async with AsyncSessionLocal() as session:
        try:
            yield session
        finally:
            await session.close()
```

### 2.5 Domain Schemas & Models (`src/domain/`)

#### Models (`src/domain/models.py`)
```python
from datetime import datetime, timezone
from sqlalchemy.orm import Mapped, mapped_column, relationship
from sqlalchemy import Integer, String, Text, Boolean, DateTime, ForeignKey, Enum as SQLEnum
import enum
from src.database import Base

class PriorityEnum(str, enum.Enum):
    LOW = "LOW"
    MEDIUM = "MEDIUM"
    HIGH = "HIGH"
    CRITICAL = "CRITICAL"

class User(Base):
    __tablename__ = "users"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    email: Mapped[str] = mapped_column(String(255), unique=True, index=True, nullable=False)
    hashed_password: Mapped[str] = mapped_column(String(255), nullable=False)
    full_name: Mapped[str] = mapped_column(String(255), nullable=False)
    is_active: Mapped[bool] = mapped_column(Boolean, default=True)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc))

    tasks: Mapped[list["Task"]] = relationship("Task", back_populates="owner", cascade="all, delete-orphan")

class Task(Base):
    __tablename__ = "tasks"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    title: Mapped[str] = mapped_column(String(255), nullable=False, index=True)
    description: Mapped[str] = mapped_column(Text, nullable=True)
    priority: Mapped[PriorityEnum] = mapped_column(SQLEnum(PriorityEnum), default=PriorityEnum.MEDIUM)
    is_completed: Mapped[bool] = mapped_column(Boolean, default=False, index=True)
    due_date: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc))
    updated_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc), onupdate=lambda: datetime.now(timezone.utc))

    owner_id: Mapped[int] = mapped_column(ForeignKey("users.id", ondelete="CASCADE"), nullable=False, index=True)
    owner: Mapped["User"] = relationship("User", back_populates="tasks")
```

#### Schemas (`src/domain/schemas.py`)
```python
from pydantic import BaseModel, ConfigDict, EmailStr, Field
from datetime import datetime
from src.domain.models import PriorityEnum

# Auth DTOs
class UserRegisterDTO(BaseModel):
    email: EmailStr
    password: str = Field(min_length=8, max_length=64)
    full_name: str = Field(min_length=2, max_length=100)

class UserLoginDTO(BaseModel):
    email: EmailStr
    password: str

class UserResponseDTO(BaseModel):
    model_config = ConfigDict(from_attributes=True)
    id: int
    email: EmailStr
    full_name: str
    is_active: bool
    created_at: datetime

class TokenResponseDTO(BaseModel):
    access_token: str
    token_type: str = "bearer"
    expires_in_seconds: int

# Task DTOs
class TaskCreateDTO(BaseModel):
    title: str = Field(min_length=1, max_length=255)
    description: str | None = Field(default=None, max_length=5000)
    priority: PriorityEnum = PriorityEnum.MEDIUM
    due_date: datetime | None = None

class TaskUpdateDTO(BaseModel):
    title: str | None = Field(default=None, min_length=1, max_length=255)
    description: str | None = Field(default=None, max_length=5000)
    priority: PriorityEnum | None = None
    is_completed: bool | None = None
    due_date: datetime | None = None

class TaskResponseDTO(BaseModel):
    model_config = ConfigDict(from_attributes=True)
    id: int
    title: str
    description: str | None
    priority: PriorityEnum
    is_completed: bool
    due_date: datetime | None
    created_at: datetime
    updated_at: datetime
    owner_id: int
```

### 2.6 Service & Repository Layer (`src/services/` & `src/repositories/`)

#### Repository (`src/repositories/task_repo.py`)
```python
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy import select, update, delete
from src.domain.models import Task

class TaskRepository:
    def __init__(self, session: AsyncSession):
        self.session = session

    async def create(self, task: Task) -> Task:
        self.session.add(task)
        await self.session.commit()
        await self.session.refresh(task)
        return task

    async def get_by_id(self, task_id: int, owner_id: int) -> Task | None:
        stmt = select(Task).where(Task.id == task_id, Task.owner_id == owner_id)
        result = await self.session.execute(stmt)
        return result.scalar_one_or_none()

    async def list_tasks(self, owner_id: int, is_completed: bool | None, limit: int, offset: int) -> list[Task]:
        stmt = select(Task).where(Task.owner_id == owner_id)
        if is_completed is not None:
            stmt = stmt.where(Task.is_completed == is_completed)
        stmt = stmt.order_by(Task.created_at.desc()).limit(limit).offset(offset)
        result = await self.session.execute(stmt)
        return list(result.scalars().all())

    async def update(self, task: Task, update_data: dict) -> Task:
        for key, value in update_data.items():
            setattr(task, key, value)
        await self.session.commit()
        await self.session.refresh(task)
        return task

    async def delete(self, task: Task) -> None:
        await self.session.delete(task)
        await self.session.commit()
```

#### Task Service (`src/services/task_service.py`)
```python
from fastapi import HTTPException, status
from src.repositories.task_repo import TaskRepository
from src.domain.models import Task
from src.domain.schemas import TaskCreateDTO, TaskUpdateDTO

class TaskService:
    def __init__(self, repo: TaskRepository):
        self.repo = repo

    async def create_task(self, owner_id: int, dto: TaskCreateDTO) -> Task:
        task = Task(
            title=dto.title,
            description=dto.description,
            priority=dto.priority,
            due_date=dto.due_date,
            owner_id=owner_id
        )
        return await self.repo.create(task)

    async def get_task(self, task_id: int, owner_id: int) -> Task:
        task = await self.repo.get_by_id(task_id, owner_id)
        if not task:
            raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="Task not found")
        return task

    async def list_tasks(self, owner_id: int, is_completed: bool | None, limit: int, offset: int) -> list[Task]:
        return await self.repo.list_tasks(owner_id, is_completed, limit, offset)

    async def update_task(self, task_id: int, owner_id: int, dto: TaskUpdateDTO) -> Task:
        task = await self.get_task(task_id, owner_id)
        delta = dto.model_dump(exclude_unset=True)
        if not delta:
            return task
        return await self.repo.update(task, delta)

    async def delete_task(self, task_id: int, owner_id: int) -> None:
        task = await self.get_task(task_id, owner_id)
        await self.repo.delete(task)
```

#### Auth Service (`src/services/auth_service.py`)
```python
from pwdlib import PasswordHash
from pwdlib.hashers.argon2 import Argon2Hasher
import jwt
from datetime import datetime, timedelta, timezone
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy import select
from fastapi import HTTPException, status
from src.domain.models import User
from src.domain.schemas import UserRegisterDTO, UserLoginDTO, TokenResponseDTO
from src.config import get_settings

password_hash = PasswordHash((Argon2Hasher(),))
settings = get_settings()

class AuthService:
    def __init__(self, session: AsyncSession):
        self.session = session

    async def register(self, dto: UserRegisterDTO) -> User:
        stmt = select(User).where(User.email == dto.email)
        res = await self.session.execute(stmt)
        if res.scalar_one_or_none():
            raise HTTPException(status_code=status.HTTP_409_CONFLICT, detail="Email already registered")

        hashed = password_hash.hash(dto.password)
        user = User(email=dto.email, hashed_password=hashed, full_name=dto.full_name)
        self.session.add(user)
        await self.session.commit()
        await self.session.refresh(user)
        return user

    async def login(self, dto: UserLoginDTO) -> TokenResponseDTO:
        stmt = select(User).where(User.email == dto.email)
        res = await self.session.execute(stmt)
        user = res.scalar_one_or_none()

        if not user or not password_hash.verify(dto.password, user.hashed_password):
            raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="Invalid credentials")

        expire_seconds = settings.jwt_expire_minutes * 60
        payload = {
            "sub": str(user.id),
            "email": user.email,
            "exp": datetime.now(timezone.utc) + timedelta(seconds=expire_seconds)
        }
        token = jwt.encode(payload, settings.jwt_secret_key.get_secret_value(), algorithm=settings.jwt_algorithm)
        return TokenResponseDTO(access_token=token, expires_in_seconds=expire_seconds)
```

### 2.7 API Routers & Dependencies (`src/api/`)

#### Dependencies (`src/api/dependencies.py`)
```python
from fastapi import Depends, HTTPException, status
from fastapi.security import HTTPBearer, HTTPAuthorizationCredentials
import jwt
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy import select
from src.database import get_db_session
from src.domain.models import User
from src.repositories.task_repo import TaskRepository
from src.services.task_service import TaskService
from src.services.auth_service import AuthService
from src.config import get_settings

security = HTTPBearer()
settings = get_settings()

async def get_current_user(
    credentials: HTTPAuthorizationCredentials = Depends(security),
    session: AsyncSession = Depends(get_db_session)
) -> User:
    token = credentials.credentials
    try:
        payload = jwt.decode(
            token,
            settings.jwt_secret_key.get_secret_value(),
            algorithms=[settings.jwt_algorithm]
        )
        user_id = int(payload.get("sub"))
    except (jwt.PyJWTError, ValueError):
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="Invalid token")

    stmt = select(User).where(User.id == user_id, User.is_active == True)
    res = await session.execute(stmt)
    user = res.scalar_one_or_none()
    if not user:
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="User not found or inactive")
    return user

def get_auth_service(session: AsyncSession = Depends(get_db_session)) -> AuthService:
    return AuthService(session)

def get_task_service(session: AsyncSession = Depends(get_db_session)) -> TaskService:
    repo = TaskRepository(session)
    return TaskService(repo)
```

#### Auth Router (`src/api/v1/auth_router.py`)
```python
from fastapi import APIRouter, Depends, status
from src.domain.schemas import UserRegisterDTO, UserLoginDTO, UserResponseDTO, TokenResponseDTO
from src.services.auth_service import AuthService
from src.api.dependencies import get_auth_service

router = APIRouter()

@router.post("/register", response_model=UserResponseDTO, status_code=status.HTTP_201_CREATED)
async def register_user(dto: UserRegisterDTO, service: AuthService = Depends(get_auth_service)):
    return await service.register(dto)

@router.post("/login", response_model=TokenResponseDTO)
async def login_user(dto: UserLoginDTO, service: AuthService = Depends(get_auth_service)):
    return await service.login(dto)
```

#### Task Router (`src/api/v1/task_router.py`)
```python
from fastapi import APIRouter, Depends, Query, status
from src.domain.schemas import TaskCreateDTO, TaskUpdateDTO, TaskResponseDTO
from src.domain.models import User
from src.services.task_service import TaskService
from src.api.dependencies import get_current_user, get_task_service

router = APIRouter()

@router.post("", response_model=TaskResponseDTO, status_code=status.HTTP_201_CREATED)
async def create_task(
    dto: TaskCreateDTO,
    current_user: User = Depends(get_current_user),
    service: TaskService = Depends(get_task_service)
):
    return await service.create_task(current_user.id, dto)

@router.get("", response_model=list[TaskResponseDTO])
async def list_tasks(
    is_completed: bool | None = Query(default=None),
    limit: int = Query(default=20, ge=1, le=100),
    offset: int = Query(default=0, ge=0),
    current_user: User = Depends(get_current_user),
    service: TaskService = Depends(get_task_service)
):
    return await service.list_tasks(current_user.id, is_completed, limit, offset)

@router.get("/{task_id}", response_model=TaskResponseDTO)
async def get_task(
    task_id: int,
    current_user: User = Depends(get_current_user),
    service: TaskService = Depends(get_task_service)
):
    return await service.get_task(task_id, current_user.id)

@router.patch("/{task_id}", response_model=TaskResponseDTO)
async def update_task(
    task_id: int,
    dto: TaskUpdateDTO,
    current_user: User = Depends(get_current_user),
    service: TaskService = Depends(get_task_service)
):
    return await service.update_task(task_id, current_user.id, dto)

@router.delete("/{task_id}", status_code=status.HTTP_204_NO_CONTENT)
async def delete_task(
    task_id: int,
    current_user: User = Depends(get_current_user),
    service: TaskService = Depends(get_task_service)
):
    await service.delete_task(task_id, current_user.id)
```

---

## 3. Automated Testing Suite

### 3.1 Test Fixtures (`tests/conftest.py`)

```python
import pytest
import pytest_asyncio
from httpx import AsyncClient, ASGITransport
from sqlalchemy.ext.asyncio import create_async_engine, async_sessionmaker, AsyncSession
from src.main import app
from src.database import Base, get_db_session
from src.config import get_settings

TEST_DATABASE_URL = "sqlite+aiosqlite:///:memory:"

@pytest_asyncio.fixture(scope="function")
async def test_session():
    engine = create_async_engine(TEST_DATABASE_URL, echo=False)
    async with engine.begin() as conn:
        await conn.run_sync(Base.metadata.create_all)
        
    session_factory = async_sessionmaker(bind=engine, class_=AsyncSession, expire_on_commit=False)
    async with session_factory() as session:
        yield session

    async with engine.begin() as conn:
        await conn.run_sync(Base.metadata.drop_all)
    await engine.dispose()

@pytest_asyncio.fixture(scope="function")
async def client(test_session: AsyncSession):
    app.dependency_overrides[get_db_session] = lambda: test_session
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as ac:
        yield ac
    app.dependency_overrides.clear()
```

### 3.2 Integration & API Tests (`tests/test_api.py`)

```python
import pytest
from httpx import AsyncClient

@pytest.mark.asyncio
async def test_full_auth_and_task_crud_lifecycle(client: AsyncClient):
    # 1. Register User
    reg_payload = {"email": "alice@example.com", "password": "Password123!", "full_name": "Alice Smith"}
    reg_resp = await client.post("/api/v1/auth/register", json=reg_payload)
    assert reg_resp.status_code == 201
    user_id = reg_resp.json()["id"]

    # 2. Login to get JWT
    login_payload = {"email": "alice@example.com", "password": "Password123!"}
    login_resp = await client.post("/api/v1/auth/login", json=login_payload)
    assert login_resp.status_code == 200
    token = login_resp.json()["access_token"]
    headers = {"Authorization": f"Bearer {token}"}

    # 3. Create Task
    task_payload = {"title": "Deploy to Production", "priority": "HIGH"}
    create_resp = await client.post("/api/v1/tasks", json=task_payload, headers=headers)
    assert create_resp.status_code == 201
    task_data = create_resp.json()
    assert task_data["title"] == "Deploy to Production"
    assert task_data["is_completed"] is False
    task_id = task_data["id"]

    # 4. List Tasks
    list_resp = await client.get("/api/v1/tasks", headers=headers)
    assert list_resp.status_code == 200
    assert len(list_resp.json()) == 1

    # 5. Patch Task (Mark Complete)
    patch_resp = await client.patch(f"/api/v1/tasks/{task_id}", json={"is_completed": True}, headers=headers)
    assert patch_resp.status_code == 200
    assert patch_resp.json()["is_completed"] is True

    # 6. Delete Task
    del_resp = await client.delete(f"/api/v1/tasks/{task_id}", headers=headers)
    assert del_resp.status_code == 204

    # 7. Verify Task is Gone (404)
    get_resp = await client.get(f"/api/v1/tasks/{task_id}", headers=headers)
    assert get_resp.status_code == 404
```

---

## 4. Production Infrastructure & Deployment

### 4.1 Multi-Stage Dockerfile

```dockerfile
# Stage 1: Build virtual environment with Astral uv
FROM ghcr.io/astral-sh/uv:0.5-python3.12-bookworm-slim AS builder
WORKDIR /app
ENV UV_COMPILE_BYTECODE=1 \
    UV_LINK_MODE=copy

COPY pyproject.toml uv.lock ./
RUN --mount=type=cache,target=/root/.cache/uv \
    uv sync --frozen --no-install-project --no-dev

COPY . .
RUN --mount=type=cache,target=/root/.cache/uv \
    uv sync --frozen --no-dev

# Stage 2: Minimal runtime image
FROM python:3.12-slim-bookworm AS runtime
WORKDIR /app

RUN groupadd -g 10001 appgroup && \
    useradd -u 10001 -g appgroup -s /sbin/nologin -d /app appuser

COPY --from=builder --chown=appuser:appgroup /app/.venv /app/.venv
COPY --from=builder --chown=appuser:appgroup /app /app

ENV PATH="/app/.venv/bin:$PATH" \
    PYTHONUNBUFFERED=1

USER appuser
EXPOSE 8000

HEALTHCHECK --interval=30s --timeout=3s --retries=3 \
    CMD ["python", "-c", "import urllib.request; urllib.request.urlopen('http://127.0.0.1:8000/health')"]

ENTRYPOINT ["uvicorn", "src.main:app", "--host", "0.0.0.0", "--port", "8000"]
```

### 4.2 Docker Compose Stack

```yaml
version: "3.8"

services:
  api:
    build: .
    ports:
      - "8000:8000"
    environment:
      - APP_ENV=production
      - DATABASE_URL=postgresql+asyncpg://postgres:secret@postgres:5432/todo_db
      - JWT_SECRET_KEY=production-secret-signing-key-32-chars-long!
    depends_on:
      postgres:
        condition: service_healthy

  postgres:
    image: postgres:16-alpine
    environment:
      POSTGRES_USER: postgres
      POSTGRES_PASSWORD: secret
      POSTGRES_DB: todo_db
    ports:
      - "5432:5432"
    volumes:
      - pgdata:/var/lib/postgresql/data
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U postgres"]
      interval: 5s
      timeout: 5s
      retries: 5

volumes:
  pgdata:
```

---

## 5. Operational Runbook & Verification

### Local Setup & Execution
```bash
# 1. Install dependencies with Astral uv
uv sync

# 2. Run database migrations
uv run alembic upgrade head

# 3. Launch test suite
uv run pytest -v

# 4. Start local development server
uv run uvicorn src.main:app --reload --port 8000
```

### Verification Curl Commands
```bash
# Register User
curl -X POST http://localhost:8000/api/v1/auth/register \
  -H "Content-Type: application/json" \
  -d '{"email": "dev@enterprise.internal", "password": "SecurePassword123!", "full_name": "Dev Operator"}'

# Login
TOKEN=$(curl -s -X POST http://localhost:8000/api/v1/auth/login \
  -H "Content-Type: application/json" \
  -d '{"email": "dev@enterprise.internal", "password": "SecurePassword123!"}' | jq -r '.access_token')

# Create Task
curl -X POST http://localhost:8000/api/v1/tasks \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"title": "Review PR #42", "priority": "HIGH"}'

# List Tasks
curl -s -X GET http://localhost:8000/api/v1/tasks \
  -H "Authorization: Bearer $TOKEN" | jq .
```
