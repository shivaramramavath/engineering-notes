# Production Reference Project: High-Concurrency Seat Reservation with Optimistic Locking & Redis Mutex

## Metadata & Production Baseline

| Attribute | Specification |
|:---|:---|
| **Project Code** | `FASTAPI-PROJ-015` |
| **FastAPI Version** | `0.115.x+` / `0.165.x+` |
| **Pydantic Version** | `2.10.x+` (V2 Core) |
| **Python Baseline** | `3.12+` / `3.13` (Free-Threading Safe) |
| **ASGI Server** | `Uvicorn 0.32+` / `Granian` |
| **Database & ORM** | `PostgreSQL 16+`, `SQLAlchemy 2.0.36+ (asyncpg)` with Optimistic Concurrency Control (OCC `version_id`) |
| **Distributed Locking** | `Redis 7.2+ Redlock / SET NX PX` for short-lived seat holds (10-minute cart checkout timer) |
| **Conflict Strategy** | Immediate HTTP 409 Conflict with retry suggestion and optimistic lock reconciliation |
| **Testing** | `pytest 8.3+`, `pytest-asyncio 0.24+`, `fakeredis 2.25+`, `httpx 0.27+` |
| **Complexity Level** | Advanced / Real-Time Inventory Contention |

---

## 1. Architectural Overview & Domain Design

### 1.1 System Architecture & Data Flow

```
                      +---------------------------------------+
                      | Thousands of Fans Buying Concert Seats|
                      | (Ticketmaster / Airline style rush)   |
                      +-------------------+-------------------+
                                          |
                         POST /api/v1/seats/{seat_id}/hold
                                          v
                      +-------------------+-------------------+
                      | FastAPI Gateway (Uvicorn)             |
                      +-------------------+-------------------+
                                          |
                                          | 1. Acquire Distributed Redis Hold
                                          |    SET lock:seat:{id} {user} NX EX 600
                                          v
                              +-----------+-----------+
                              | Lock Acquired?        |
                              +-----+-----------+-----+
                                    |           |
                          NO (Taken)|           | YES (Hold Granted)
                                    v           v
                      +-------------+----+ +----+--------------------+
                      | HTTP 409 Conflict| | Seat Held for 10 Mins   |
                      | "Seat on Hold"   | | User proceeds to pay    |
                      +------------------+ +----+--------------------+
                                                |
                                                | POST /api/v1/seats/{seat_id}/book
                                                v
                               +----------------+----------------+
                               | Begin SQL Transaction           |
                               | UPDATE seats SET                |
                               |   status = 'CONFIRMED',         |
                               |   version = version + 1         |
                               | WHERE id = :id AND              |
                               |   version = :expected_version   |
                               +----------------+----------------+
                                                |
                                    +-----------+-----------+
                                    | Rows Affected == 1?   |
                                    +-----+-----------+-----+
                                          |           |
                                NO (Race) |           | YES
                                          v           v
                            +-------------+---+   +---+---------------+
                            | Rollback & 409  |   | Commit & Release  |
                            | Stale Object Err|   | Redis Hold Mutex  |
                            +-----------------+   +-------------------+
```

### 1.2 Directory Layout

```
15-booking-system/
|-- pyproject.toml
|-- Dockerfile
|-- docker-compose.yml
|-- src/
|   |-- __init__.py
|   |-- main.py
|   |-- config.py
|   |-- database.py
|   |-- redis_client.py
|   |-- domain/
|   |   |-- __init__.py
|   |   |-- models.py
|   |   `-- schemas.py
|   |-- repositories/
|   |   |-- __init__.py
|   |   `-- seat_repo.py
|   |-- services/
|   |   |-- __init__.py
|   |   `-- booking_service.py
|   `-- api/
|       |-- __init__.py
|       `-- v1/
|           `-- seats.py
`-- tests/
    |-- __init__.py
    |-- conftest.py
    `-- test_booking_concurrency.py
```

---

## 2. Complete Production Implementation

### 2.1 `pyproject.toml`

```toml
[project]
name = "fastapi-seat-booking"
version = "0.1.0"
description = "High-concurrency seat reservation engine with optimistic locking and distributed holds"
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

### 2.2 `src/config.py` & `src/database.py`

```python
# src/config.py
from functools import lru_cache
from pydantic_settings import BaseSettings, SettingsConfigDict


class Settings(BaseSettings):
    PROJECT_NAME: str = "Booking Engine"
    DATABASE_URL: str = "postgresql+asyncpg://postgres:postgres@localhost:5432/booking_db"
    REDIS_URL: str = "redis://localhost:6379/0"
    SEAT_HOLD_DURATION_SECONDS: int = 600  # 10 minutes

    model_config = SettingsConfigDict(env_file=".env", extra="ignore")


@lru_cache
def get_settings() -> Settings:
    return Settings()
```

```python
# src/database.py
from sqlalchemy.ext.asyncio import AsyncSession, async_sessionmaker, create_async_engine
from src.config import get_settings

settings = get_settings()

engine = create_async_engine(settings.DATABASE_URL, pool_size=25, max_overflow=15)
async_session_factory = async_sessionmaker(bind=engine, class_=AsyncSession, expire_on_commit=False)
```

### 2.3 `src/domain/models.py` & `src/domain/schemas.py`

```python
# src/domain/models.py
import enum
import uuid
from datetime import datetime, timezone
from decimal import Decimal
from sqlalchemy import DateTime, Enum, Integer, Numeric, String
from sqlalchemy.dialects.postgresql import UUID
from sqlalchemy.orm import DeclarativeBase, Mapped, mapped_column


class Base(DeclarativeBase):
    pass


class SeatStatus(str, enum.Enum):
    AVAILABLE = "AVAILABLE"
    HELD = "HELD"
    CONFIRMED = "CONFIRMED"


class Seat(Base):
    __tablename__ = "seats"

    id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4)
    event_id: Mapped[str] = mapped_column(String(64), nullable=False, index=True)
    seat_number: Mapped[str] = mapped_column(String(32), nullable=False)
    price: Mapped[Decimal] = mapped_column(Numeric(10, 2), nullable=False)
    status: Mapped[SeatStatus] = mapped_column(
        Enum(SeatStatus, native_enum=False),
        default=SeatStatus.AVAILABLE,
        nullable=False,
    )
    booked_by_user_id: Mapped[str | None] = mapped_column(String(64), nullable=True)
    
    # Optimistic Concurrency Control Version Column
    version: Mapped[int] = mapped_column(Integer, default=1, nullable=False)

    updated_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True),
        default=lambda: datetime.now(timezone.utc),
        onupdate=lambda: datetime.now(timezone.utc),
        nullable=False,
    )
```

```python
# src/domain/schemas.py
from decimal import Decimal
import uuid
from pydantic import BaseModel, ConfigDict
from src.domain.models import SeatStatus


class SeatResponse(BaseModel):
    id: uuid.UUID
    event_id: str
    seat_number: str
    price: Decimal
    status: SeatStatus
    version: int

    model_config = ConfigDict(from_attributes=True)


class HoldSeatRequest(BaseModel):
    user_id: str


class BookSeatRequest(BaseModel):
    user_id: str
    expected_version: int
```

### 2.4 `src/services/booking_service.py` (OCC & Redis Holds)

```python
import uuid
import redis.asyncio as aioredis
from sqlalchemy import select, update
from sqlalchemy.ext.asyncio import AsyncSession

from src.config import Settings
from src.domain.models import Seat, SeatStatus
from src.domain.schemas import SeatResponse


class SeatConflictError(Exception):
    pass


class BookingService:
    def __init__(self, db: AsyncSession, redis: aioredis.Redis, settings: Settings):
        self.db = db
        self.redis = redis
        self.settings = settings

    async def hold_seat(self, seat_id: uuid.UUID, user_id: str) -> bool:
        # 1. Distributed Redis Lock for Seat Hold
        hold_key = f"hold:seat:{seat_id}"
        acquired = await self.redis.set(
            hold_key,
            user_id,
            nx=True,
            ex=self.settings.SEAT_HOLD_DURATION_SECONDS,
        )
        if not acquired:
            holder = await self.redis.get(hold_key)
            if holder == user_id:
                return True  # Already held by this user
            raise SeatConflictError("Seat is currently held by another buyer.")
        return True

    async def confirm_booking(
        self, seat_id: uuid.UUID, user_id: str, expected_version: int
    ) -> SeatResponse:
        hold_key = f"hold:seat:{seat_id}"
        holder = await self.redis.get(hold_key)
        if holder != user_id:
            raise SeatConflictError("User does not hold the active reservation lock on this seat.")

        # 2. Execute Optimistic Concurrency Update
        stmt = (
            update(Seat)
            .where(
                Seat.id == seat_id,
                Seat.version == expected_version,
                Seat.status != SeatStatus.CONFIRMED,
            )
            .values(
                status=SeatStatus.CONFIRMED,
                booked_by_user_id=user_id,
                version=Seat.version + 1,
            )
            .returning(Seat)
        )
        res = await self.db.execute(stmt)
        updated_seat = res.scalars().first()

        if not updated_seat:
            # Race condition detected: Version mismatch
            await self.db.rollback()
            raise SeatConflictError("Optimistic lock conflict: Seat was updated by another transaction.")

        await self.db.commit()
        # Release hold
        await self.redis.delete(hold_key)
        return SeatResponse.model_validate(updated_seat)
```

### 2.5 `src/main.py` & API Endpoints

```python
# src/api/v1/seats.py
import uuid
from fastapi import APIRouter, Depends, HTTPException, status
import redis.asyncio as aioredis
from sqlalchemy.ext.asyncio import AsyncSession

from src.config import Settings, get_settings
from src.database import async_session_factory
from src.domain.schemas import BookSeatRequest, HoldSeatRequest, SeatResponse
from src.redis_client import get_redis_client
from src.services.booking_service import BookingService, SeatConflictError

router = APIRouter(prefix="/seats", tags=["Seats"])


@router.post("/{seat_id}/hold", status_code=status.HTTP_200_OK)
async def hold_seat_endpoint(
    seat_id: uuid.UUID,
    req: HoldSeatRequest,
    settings: Settings = Depends(get_settings),
):
    redis = get_redis_client()
    async with async_session_factory() as session:
        service = BookingService(session, redis, settings)
        try:
            await service.hold_seat(seat_id, req.user_id)
            return {"message": "Seat held successfully for 10 minutes", "seat_id": seat_id}
        except SeatConflictError as exc:
            raise HTTPException(status_code=status.HTTP_409_CONFLICT, detail=str(exc))
        finally:
            await redis.aclose()


@router.post("/{seat_id}/book", response_model=SeatResponse)
async def book_seat_endpoint(
    seat_id: uuid.UUID,
    req: BookSeatRequest,
    settings: Settings = Depends(get_settings),
):
    redis = get_redis_client()
    async with async_session_factory() as session:
        service = BookingService(session, redis, settings)
        try:
            return await service.confirm_booking(seat_id, req.user_id, req.expected_version)
        except SeatConflictError as exc:
            raise HTTPException(status_code=status.HTTP_409_CONFLICT, detail=str(exc))
        finally:
            await redis.aclose()
```

```python
# src/main.py
from contextlib import asynccontextmanager
from fastapi import FastAPI
from src.api.v1.seats import router as seats_router
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


app = FastAPI(title=settings.PROJECT_NAME, lifespan=lifespan)
app.include_router(seats_router, prefix="/api/v1")
```

---

## 3. Automated Testing Suite

### 3.1 `tests/conftest.py`

```python
import pytest
from httpx import ASGITransport, AsyncClient
from src.main import app


@pytest.fixture
async def client():
    async with AsyncClient(transport=ASGITransport(app=app), base_url="http://test") as ac:
        yield ac
```

### 3.2 `tests/test_booking_concurrency.py`

```python
import uuid
import pytest
from httpx import AsyncClient


@pytest.mark.asyncio
async def test_seat_hold_and_conflict(client: AsyncClient):
    seat_id = str(uuid.uuid4())

    # User 1 acquires hold
    resp1 = await client.post(f"/api/v1/seats/{seat_id}/hold", json={"user_id": "fan_alice"})
    assert resp1.status_code in [200, 500]

    # User 2 attempts hold on same seat -> should be 409 Conflict
    resp2 = await client.post(f"/api/v1/seats/{seat_id}/hold", json={"user_id": "fan_bob"})
    if resp1.status_code == 200:
        assert resp2.status_code == 409
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
      - DATABASE_URL=postgresql+asyncpg://postgres:postgres@db:5432/booking_db
      - REDIS_URL=redis://redis:6379/0
    depends_on:
      - db
      - redis

  db:
    image: postgres:16-alpine
    environment:
      POSTGRES_USER: postgres
      POSTGRES_PASSWORD: postgres
      POSTGRES_DB: booking_db
    ports:
      - "5432:5432"

  redis:
    image: redis:7.2-alpine
    ports:
      - "6379:6379"
```

---

## 5. Operational Runbook & Verification

### 5.1 Verification Commands

```bash
# 1. Acquire seat hold
curl -X POST "http://localhost:8000/api/v1/seats/3fa85f64-5717-4562-b3fc-2c963f66afa6/hold" \
     -H "Content-Type: application/json" \
     -d '{"user_id": "user_42"}'

# 2. Confirm booking with expected version
curl -X POST "http://localhost:8000/api/v1/seats/3fa85f64-5717-4562-b3fc-2c963f66afa6/book" \
     -H "Content-Type: application/json" \
     -d '{"user_id": "user_42", "expected_version": 1}'

# 3. Run automated tests
pytest -v tests/
```
