# Production Reference Project: High-Performance URL Shortener with Redis & PostgreSQL

## Metadata & Production Baseline

| Attribute | Specification |
|:---|:---|
| **Project Code** | `FASTAPI-PROJ-004` |
| **FastAPI Version** | `0.115.x+` / `0.165.x+` |
| **Pydantic Version** | `2.10.x+` (V2 Core) |
| **Python Baseline** | `3.12+` / `3.13` (Free-Threading Safe) |
| **ASGI Server** | `Uvicorn 0.32+` / `Granian` |
| **Database & ORM** | `PostgreSQL 16+`, `SQLAlchemy 2.0.36+ (asyncpg)`, `Alembic 1.13+` |
| **Cache & Buffering** | `Redis 7.2+ (redis-py async)` for redirect cache & click analytics buffering |
| **Validation & Security** | `Pydantic-Settings 2.6+`, Base62 encoder/decoder, Hashids / Bijective mapping |
| **Testing** | `pytest 8.3+`, `pytest-asyncio 0.24+`, `fakeredis 2.25+`, `httpx 0.27+` |
| **Complexity Level** | Intermediate / Production High-Throughput |

---

## 1. Architectural Overview & Domain Design

### 1.1 System Architecture & Data Flow

```
                      +---------------------------------------+
                      |       Client / Web / Mobile App       |
                      +-------------------+-------------------+
                                          |
                        GET /{code}       |       POST /api/v1/links
                        (High Throughput) |       (Low Throughput)
                                          v
                      +-------------------+-------------------+
                      |      FastAPI Gateway (Uvicorn)        |
                      +-------------------+-------------------+
                                          |
                +-------------------------+-------------------------+
                |                                                   |
      [Redirect Fast-Path]                                [Link Creation Path]
                |                                                   |
                v                                                   v
      +-------------------+                               +-------------------+
      | Redis Link Cache  |                               | PostgreSQL DB     |
      | GET url:{code}    |                               | Insert Link Row   |
      +---------+---------+                               | Base62 / ID Map   |
                |                                         +---------+---------+
      +---------+---------+                                         |
      | Cache Hit?        |                                         v
      | YES: 307 Redirect |                               +-------------------+
      | NO: Query DB &    |                               | Populate Redis    |
      |     Populate Cache|                               | SET url:{code}    |
      +---------+---------+                               +-------------------+
                |
                v (Fire-and-Forget / Async Buffer)
      +-------------------+
      | Redis Stream /    |
      | HINCRBY / LPUSH   |-----> [Background Flush Task / Worker]
      | click_events      |       Persists Aggregate Clicks to PostgreSQL
      +-------------------+
```

### 1.2 Directory Layout

```
04-url-shortener/
|-- pyproject.toml
|-- Dockerfile
|-- docker-compose.yml
|-- src/
|   |-- __init__.py
|   |-- main.py
|   |-- config.py
|   |-- database.py
|   |-- redis_client.py
|   |-- base62.py
|   |-- domain/
|   |   |-- __init__.py
|   |   |-- models.py
|   |   `-- schemas.py
|   |-- repositories/
|   |   |-- __init__.py
|   |   |-- link_repo.py
|   |   `-- analytics_repo.py
|   |-- services/
|   |   |-- __init__.py
|   |   |-- link_service.py
|   |   `-- analytics_worker.py
|   `-- api/
|       |-- __init__.py
|       |-- dependencies.py
|       `-- v1/
|           |-- __init__.py
|           |-- links.py
|           `-- redirects.py
`-- tests/
    |-- __init__.py
    |-- conftest.py
    |-- test_base62.py
    `-- test_links_api.py
```

---

## 2. Complete Production Implementation

### 2.1 `pyproject.toml`

```toml
[project]
name = "fastapi-url-shortener"
version = "0.1.0"
description = "High-performance URL shortener with Redis caching and analytics"
readme = "README.md"
requires-python = ">=3.12"
dependencies = [
    "fastapi>=0.115.0",
    "uvicorn[standard]>=0.32.0",
    "pydantic>=2.10.0",
    "pydantic-settings>=2.6.0",
    "sqlalchemy>=2.0.36",
    "asyncpg>=0.30.0",
    "redis[hiredis]>=5.2.0",
    "orjson>=3.10.0",
]

[project.optional-dependencies]
dev = [
    "pytest>=8.3.0",
    "pytest-asyncio>=0.24.0",
    "httpx>=0.27.0",
    "fakeredis>=2.25.0",
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
    PROJECT_NAME: str = "URL Shortener Service"
    ENVIRONMENT: str = "production"
    DEBUG: bool = False
    
    # Database
    DATABASE_URL: str = "postgresql+asyncpg://postgres:postgres@localhost:5432/shortener_db"
    DB_POOL_SIZE: int = 20
    DB_MAX_OVERFLOW: int = 10
    
    # Redis
    REDIS_URL: str = "redis://localhost:6379/0"
    REDIS_CACHE_TTL_SECONDS: int = 86400  # 24 Hours
    
    # Shortener Config
    SHORT_CODE_LENGTH: int = 7
    BASE_URL: str = "https://sho.rt"
    ANALYTICS_FLUSH_INTERVAL_SECONDS: int = 10
    ANALYTICS_BATCH_SIZE: int = 500

    model_config = SettingsConfigDict(
        env_file=".env",
        env_file_encoding="utf-8",
        case_sensitive=True,
        extra="ignore"
    )


@lru_cache
def get_settings() -> Settings:
    return Settings()
```

### 2.3 `src/base62.py`

```python
import string

ALPHABET = string.digits + string.ascii_letters  # 0-9, a-z, A-Z (62 chars)
BASE = len(ALPHABET)


def encode(num: int) -> str:
    """Encode an integer ID into a Base62 alphanumeric string."""
    if num == 0:
        return ALPHABET[0]
    digits = []
    while num > 0:
        digits.append(ALPHABET[num % BASE])
        num //= BASE
    return "".join(reversed(digits))


def decode(code: str) -> int:
    """Decode a Base62 string back into its primary key integer ID."""
    num = 0
    for char in code:
        idx = ALPHABET.find(char)
        if idx == -1:
            raise ValueError(f"Invalid Base62 character: {char}")
        num = num * BASE + idx
    return num
```

### 2.4 `src/database.py` & `src/redis_client.py`

```python
# src/database.py
from collections.abc import AsyncGenerator
from sqlalchemy.ext.asyncio import (
    AsyncEngine,
    AsyncSession,
    async_sessionmaker,
    create_async_engine,
)
from src.config import get_settings

settings = get_settings()

engine: AsyncEngine = create_async_engine(
    settings.DATABASE_URL,
    pool_size=settings.DB_POOL_SIZE,
    max_overflow=settings.DB_MAX_OVERFLOW,
    pool_pre_ping=True,
    echo=settings.DEBUG,
)

async_session_factory = async_sessionmaker(
    bind=engine,
    class_=AsyncSession,
    expire_on_commit=False,
)


async def get_db_session() -> AsyncGenerator[AsyncSession, None]:
    async with async_session_factory() as session:
        try:
            yield session
        except Exception:
            await session.rollback()
            raise
```

```python
# src/redis_client.py
from collections.abc import AsyncGenerator
import redis.asyncio as aioredis
from src.config import get_settings

settings = get_settings()

redis_pool = aioredis.ConnectionPool.from_url(
    settings.REDIS_URL,
    max_connections=50,
    decode_responses=True,
)


def get_redis_client() -> aioredis.Redis:
    return aioredis.Redis(connection_pool=redis_pool)


async def get_redis() -> AsyncGenerator[aioredis.Redis, None]:
    client = get_redis_client()
    try:
        yield client
    finally:
        await client.aclose()
```

### 2.5 `src/domain/models.py` & `src/domain/schemas.py`

```python
# src/domain/models.py
from datetime import datetime, timezone
from sqlalchemy import BigInteger, Boolean, DateTime, Index, Integer, String, Text
from sqlalchemy.orm import DeclarativeBase, Mapped, mapped_column


class Base(DeclarativeBase):
    pass


class ShortLink(Base):
    __tablename__ = "short_links"

    id: Mapped[int] = mapped_column(BigInteger, primary_key=True, autoincrement=True)
    custom_alias: Mapped[str | None] = mapped_column(String(64), unique=True, nullable=True, index=True)
    short_code: Mapped[str] = mapped_column(String(16), unique=True, nullable=False, index=True)
    original_url: Mapped[str] = mapped_column(Text, nullable=False)
    is_active: Mapped[bool] = mapped_column(Boolean, default=True, nullable=False)
    click_count: Mapped[int] = mapped_column(Integer, default=0, nullable=False)
    expires_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True),
        default=lambda: datetime.now(timezone.utc),
        nullable=False,
    )

    __table_args__ = (
        Index("ix_short_links_code_active", "short_code", "is_active"),
    )
```

```python
# src/domain/schemas.py
from datetime import datetime
from pydantic import BaseModel, ConfigDict, Field, HttpUrl


class LinkCreateRequest(BaseModel):
    url: HttpUrl = Field(..., description="Destination target URL")
    custom_alias: str | None = Field(
        default=None,
        min_length=3,
        max_length=64,
        pattern=r"^[a-zA-Z0-9_-]+$",
        description="Optional custom alphanumeric alias",
    )
    expires_at: datetime | None = Field(
        default=None,
        description="Optional expiration timestamp in UTC",
    )


class LinkResponse(BaseModel):
    short_code: str
    short_url: str
    original_url: str
    created_at: datetime
    expires_at: datetime | None
    click_count: int

    model_config = ConfigDict(from_attributes=True)


class LinkStatsResponse(BaseModel):
    short_code: str
    original_url: str
    total_clicks: int
    is_active: bool
    created_at: datetime
    expires_at: datetime | None
```

### 2.6 `src/repositories/link_repo.py` & `src/services/link_service.py`

```python
# src/repositories/link_repo.py
from datetime import datetime, timezone
from sqlalchemy import select, update
from sqlalchemy.ext.asyncio import AsyncSession
from src.domain.models import ShortLink


class LinkRepository:
    def __init__(self, session: AsyncSession):
        self.session = session

    async def get_by_code(self, code: str) -> ShortLink | None:
        stmt = select(ShortLink).where(
            (ShortLink.short_code == code) | (ShortLink.custom_alias == code)
        )
        result = await self.session.execute(stmt)
        return result.scalars().first()

    async def create_link(
        self,
        original_url: str,
        custom_alias: str | None = None,
        expires_at: datetime | None = None,
    ) -> ShortLink:
        link = ShortLink(
            original_url=original_url,
            custom_alias=custom_alias,
            short_code="tmp",  # Populated after ID allocation
            expires_at=expires_at,
        )
        self.session.add(link)
        await self.session.flush()  # Allocates link.id
        return link

    async def update_short_code(self, link_id: int, code: str) -> None:
        stmt = update(ShortLink).where(ShortLink.id == link_id).values(short_code=code)
        await self.session.execute(stmt)

    async def increment_click_count_bulk(self, click_counts: dict[str, int]) -> None:
        for code, increment in click_counts.items():
            stmt = (
                update(ShortLink)
                .where((ShortLink.short_code == code) | (ShortLink.custom_alias == code))
                .values(click_count=ShortLink.click_count + increment)
            )
            await self.session.execute(stmt)
```

```python
# src/services/link_service.py
from datetime import datetime, timezone
import orjson
import redis.asyncio as aioredis
from sqlalchemy.ext.asyncio import AsyncSession

from src.base62 import encode
from src.config import Settings
from src.domain.models import ShortLink
from src.domain.schemas import LinkCreateRequest, LinkResponse
from src.repositories.link_repo import LinkRepository


class LinkService:
    def __init__(self, db_session: AsyncSession, redis: aioredis.Redis, settings: Settings):
        self.repo = LinkRepository(db_session)
        self.db = db_session
        self.redis = redis
        self.settings = settings

    async def create_short_link(self, req: LinkCreateRequest) -> LinkResponse:
        url_str = str(req.url)
        
        # Check custom alias uniqueness if supplied
        if req.custom_alias:
            existing = await self.repo.get_by_code(req.custom_alias)
            if existing:
                raise ValueError(f"Custom alias '{req.custom_alias}' already in use.")

        link = await self.repo.create_link(
            original_url=url_str,
            custom_alias=req.custom_alias,
            expires_at=req.expires_at,
        )

        code = req.custom_alias if req.custom_alias else encode(link.id)
        await self.repo.update_short_code(link.id, code)
        await self.db.commit()
        await self.db.refresh(link)

        # Warm Redis Cache
        cache_key = f"url:{code}"
        payload = {
            "original_url": link.original_url,
            "expires_at": link.expires_at.isoformat() if link.expires_at else None,
            "is_active": link.is_active,
        }
        await self.redis.set(cache_key, orjson.dumps(payload), ex=self.settings.REDIS_CACHE_TTL_SECONDS)

        return LinkResponse(
            short_code=code,
            short_url=f"{self.settings.BASE_URL}/{code}",
            original_url=link.original_url,
            created_at=link.created_at,
            expires_at=link.expires_at,
            click_count=link.click_count,
        )

    async def resolve_redirect(self, code: str) -> str | None:
        cache_key = f"url:{code}"
        
        # 1. Fast Cache Read
        cached = await self.redis.get(cache_key)
        if cached:
            data = orjson.loads(cached)
            if not data.get("is_active"):
                return None
            if data.get("expires_at"):
                exp = datetime.fromisoformat(data["expires_at"])
                if exp < datetime.now(timezone.utc):
                    return None
            # Async Click Tracking in Redis Hash Buffer
            await self.redis.hincrby("analytics:clicks", code, 1)
            return data["original_url"]

        # 2. Database Fallback Read
        link = await self.repo.get_by_code(code)
        if not link or not link.is_active:
            return None
        if link.expires_at and link.expires_at < datetime.now(timezone.utc):
            return None

        # 3. Populate Redis Cache
        payload = {
            "original_url": link.original_url,
            "expires_at": link.expires_at.isoformat() if link.expires_at else None,
            "is_active": link.is_active,
        }
        await self.redis.set(cache_key, orjson.dumps(payload), ex=self.settings.REDIS_CACHE_TTL_SECONDS)
        await self.redis.hincrby("analytics:clicks", code, 1)
        return link.original_url
```

### 2.7 `src/services/analytics_worker.py`

```python
import asyncio
import logging
import redis.asyncio as aioredis
from sqlalchemy.ext.asyncio import AsyncSession
from src.database import async_session_factory
from src.repositories.link_repo import LinkRepository

logger = logging.getLogger("analytics_worker")


async def flush_analytics_batch(redis_client: aioredis.Redis) -> None:
    """Flush batched click increments from Redis to PostgreSQL."""
    clicks = await redis_client.hgetall("analytics:clicks")
    if not clicks:
        return

    # Delete processed keys atomically using rename
    temp_key = "analytics:clicks:processing"
    try:
        await redis_client.rename("analytics:clicks", temp_key)
    except aioredis.ResponseError:
        return  # Key did not exist or was empty

    parsed_clicks = {code: int(count) for code, count in clicks.items()}

    async with async_session_factory() as session:
        try:
            repo = LinkRepository(session)
            await repo.increment_click_count_bulk(parsed_clicks)
            await session.commit()
            await redis_client.delete(temp_key)
            logger.info("Successfully flushed %d click records to DB", len(parsed_clicks))
        except Exception as e:
            await session.rollback()
            logger.error("Failed to flush analytics to database: %s", e)
            # Re-increment lost clicks back to buffer
            pipe = redis_client.pipeline()
            for code, count in parsed_clicks.items():
                pipe.hincrby("analytics:clicks", code, count)
            await pipe.execute()
            await redis_client.delete(temp_key)


async def analytics_flush_loop(redis_client: aioredis.Redis, interval: int = 10) -> None:
    """Background recurring task runner."""
    while True:
        try:
            await flush_analytics_batch(redis_client)
        except asyncio.CancelledError:
            break
        except Exception as exc:
            logger.error("Error in analytics loop: %s", exc)
        await asyncio.sleep(interval)
```

### 2.8 `src/main.py` & API Routers

```python
# src/api/v1/links.py
from fastapi import APIRouter, Depends, HTTPException, status
import redis.asyncio as aioredis
from sqlalchemy.ext.asyncio import AsyncSession

from src.config import Settings, get_settings
from src.database import get_db_session
from src.domain.schemas import LinkCreateRequest, LinkResponse, LinkStatsResponse
from src.redis_client import get_redis
from src.repositories.link_repo import LinkRepository
from src.services.link_service import LinkService

router = APIRouter(prefix="/links", tags=["Links"])


@router.post("/", response_model=LinkResponse, status_code=status.HTTP_201_CREATED)
async def create_link(
    payload: LinkCreateRequest,
    db: AsyncSession = Depends(get_db_session),
    redis: aioredis.Redis = Depends(get_redis),
    settings: Settings = Depends(get_settings),
):
    service = LinkService(db, redis, settings)
    try:
        return await service.create_short_link(payload)
    except ValueError as err:
        raise HTTPException(status_code=status.HTTP_409_CONFLICT, detail=str(err))


@router.get("/{code}/stats", response_model=LinkStatsResponse)
async def get_link_stats(
    code: str,
    db: AsyncSession = Depends(get_db_session),
    redis: aioredis.Redis = Depends(get_redis),
):
    repo = LinkRepository(db)
    link = await repo.get_by_code(code)
    if not link:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="Link not found")

    # Add un-flushed buffer clicks
    buffered_clicks = await redis.hget("analytics:clicks", code)
    extra_clicks = int(buffered_clicks) if buffered_clicks else 0

    return LinkStatsResponse(
        short_code=code,
        original_url=link.original_url,
        total_clicks=link.click_count + extra_clicks,
        is_active=link.is_active,
        created_at=link.created_at,
        expires_at=link.expires_at,
    )
```

```python
# src/main.py
import asyncio
from contextlib import asynccontextmanager
from fastapi import FastAPI, HTTPException, status
from fastapi.responses import RedirectResponse

from src.api.v1.links import router as links_router
from src.config import get_settings
from src.database import engine
from src.domain.models import Base
from src.redis_client import get_redis_client, redis_pool
from src.services.analytics_worker import analytics_flush_loop, flush_analytics_batch
from src.services.link_service import LinkService

settings = get_settings()


@asynccontextmanager
async def lifespan(app: FastAPI):
    # Startup: Create tables if dev, start background analytics flush task
    async with engine.begin() as conn:
        await conn.run_sync(Base.metadata.create_all)
    
    redis = get_redis_client()
    task = asyncio.create_task(
        analytics_flush_loop(redis, settings.ANALYTICS_FLUSH_INTERVAL_SECONDS)
    )
    yield
    # Shutdown: Cancel flush task and trigger one final emergency flush
    task.cancel()
    try:
        await task
    except asyncio.CancelledError:
        pass
    await flush_analytics_batch(redis)
    await redis.aclose()
    await redis_pool.disconnect()
    await engine.dispose()


app = FastAPI(
    title=settings.PROJECT_NAME,
    lifespan=lifespan,
    docs_url="/docs",
    redoc_url="/redoc",
)

app.include_router(links_router, prefix="/api/v1")


@app.get("/{code}", response_class=RedirectResponse, status_code=status.HTTP_307_TEMPORARY_REDIRECT)
async def redirect_to_url(code: str):
    redis = get_redis_client()
    from src.database import async_session_factory
    async with async_session_factory() as session:
        service = LinkService(session, redis, settings)
        target = await service.resolve_redirect(code)
    
    if not target:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="Short URL not found or expired")
    
    return RedirectResponse(url=target, status_code=status.HTTP_307_TEMPORARY_REDIRECT)
```

---

## 3. Automated Testing Suite

### 3.1 `tests/conftest.py`

```python
import pytest
import pytest_asyncio
import fakeredis.aioredis as fake_aioredis
from httpx import ASGITransport, AsyncClient
from sqlalchemy.ext.asyncio import AsyncSession, async_sessionmaker, create_async_engine

from src.config import get_settings
from src.database import get_db_session
from src.domain.models import Base
from src.main import app
from src.redis_client import get_redis

TEST_DATABASE_URL = "sqlite+aiosqlite:///:memory:"


@pytest_asyncio.fixture(scope="session")
async def test_engine():
    engine = create_async_engine(TEST_DATABASE_URL)
    async with engine.begin() as conn:
        await conn.run_sync(Base.metadata.create_all)
    yield engine
    await engine.dispose()


@pytest_asyncio.fixture
async def test_session(test_engine) -> AsyncSession:
    session_factory = async_sessionmaker(bind=test_engine, class_=AsyncSession, expire_on_commit=False)
    async with session_factory() as session:
        yield session
        await session.rollback()


@pytest_asyncio.fixture
async def fake_redis():
    server = fake_aioredis.FakeServer()
    client = fake_aioredis.FakeRedis(server=server, decode_responses=True)
    yield client
    await client.aclose()


@pytest_asyncio.fixture
async def client(test_session, fake_redis):
    app.dependency_overrides[get_db_session] = lambda: test_session
    app.dependency_overrides[get_redis] = lambda: fake_redis
    
    async with AsyncClient(transport=ASGITransport(app=app), base_url="http://test") as ac:
        yield ac
    
    app.dependency_overrides.clear()
```

### 3.2 `tests/test_links_api.py`

```python
import pytest
from httpx import AsyncClient


@pytest.mark.asyncio
async def test_create_and_redirect_short_link(client: AsyncClient):
    # 1. Create Link
    create_resp = await client.post(
        "/api/v1/links/",
        json={"url": "https://fastapi.tiangolo.com/tutorial/", "custom_alias": "docs"},
    )
    assert create_resp.status_code == 201
    data = create_resp.json()
    assert data["short_code"] == "docs"
    assert data["original_url"] == "https://fastapi.tiangolo.com/tutorial/"

    # 2. Test Duplicate Custom Alias
    dup_resp = await client.post(
        "/api/v1/links/",
        json={"url": "https://google.com", "custom_alias": "docs"},
    )
    assert dup_resp.status_code == 409

    # 3. Test Redirect Endpoint (follow_redirects=False to verify 307)
    redir_resp = await client.get("/docs", follow_redirects=False)
    assert redir_resp.status_code == 307
    assert redir_resp.headers["location"] == "https://fastapi.tiangolo.com/tutorial/"

    # 4. Check Analytics
    stats_resp = await client.get("/api/v1/links/docs/stats")
    assert stats_resp.status_code == 200
    assert stats_resp.json()["total_clicks"] == 1
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
CMD ["uvicorn", "src.main:app", "--host", "0.0.0.0", "--port", "8000", "--workers", "4", "--log-level", "warning"]
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
      - DATABASE_URL=postgresql+asyncpg://postgres:postgres@db:5432/shortener_db
      - REDIS_URL=redis://redis:6379/0
    depends_on:
      db:
        condition: service_healthy
      redis:
        condition: service_started

  db:
    image: postgres:16-alpine
    environment:
      POSTGRES_USER: postgres
      POSTGRES_PASSWORD: postgres
      POSTGRES_DB: shortener_db
    ports:
      - "5432:5432"
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U postgres"]
      interval: 5s
      timeout: 5s
      retries: 5

  redis:
    image: redis:7.2-alpine
    ports:
      - "6379:6379"
```

---

## 5. Operational Runbook & Verification

### 5.1 Verification Commands

```bash
# 1. Create a short link
curl -X POST "http://localhost:8000/api/v1/links/" \
     -H "Content-Type: application/json" \
     -d '{"url": "https://news.ycombinator.com", "custom_alias": "hn"}'

# 2. Test 307 Redirect (Inspect response headers)
curl -I "http://localhost:8000/hn"

# 3. Retrieve link analytics
curl -s "http://localhost:8000/api/v1/links/hn/stats" | jq .

# 4. Run automated test suite
pytest -v tests/
```
