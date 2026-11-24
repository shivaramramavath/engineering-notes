# Production Reference Project: Multi-Channel Notification Service with Redis Streams

## Metadata & Production Baseline

| Attribute | Specification |
|:---|:---|
| **Project Code** | `FASTAPI-PROJ-006` |
| **FastAPI Version** | `0.115.x+` / `0.165.x+` |
| **Pydantic Version** | `2.10.x+` (V2 Core) |
| **Python Baseline** | `3.12+` / `3.13` (Free-Threading Safe) |
| **ASGI Server** | `Uvicorn 0.32+` |
| **Database & ORM** | `PostgreSQL 16+`, `SQLAlchemy 2.0.36+ (asyncpg)` |
| **Queue & Messaging** | `Redis 7.2+ Streams (XADD, XREADGROUP, XACK)`, Consumer Groups |
| **Templating & Dispatch** | `Jinja2 3.1+`, Mock Multi-Channel (Email/SMTP, SMS/Twilio, Webhook/HTTP) |
| **Testing** | `pytest 8.3+`, `pytest-asyncio 0.24+`, `fakeredis 2.25+`, `httpx 0.27+` |
| **Complexity Level** | Intermediate / Event-Driven Queueing |

---

## 1. Architectural Overview & Domain Design

### 1.1 System Architecture & Data Flow

```
                      +-----------------------------------------+
                      | Client / Order Service / Billing Service|
                      +--------------------+--------------------+
                                           |
                                           | POST /api/v1/notifications/send
                                           v
                      +--------------------+--------------------+
                      |     FastAPI Notification Gateway        |
                      +--------------------+--------------------+
                                           |
                                           | 1. Validate payload with Pydantic
                                           | 2. Persist Notification (STATUS: QUEUED)
                                           | 3. XADD to Redis Stream: "stream:notifications"
                                           v
                              +------------+------------+
                              | Redis Stream Broker     |
                              | Group: "notif-workers"  |
                              +------------+------------+
                                           |
                       +-------------------+-------------------+
                       | XREADGROUP                            | XREADGROUP
                       v                                       v
        +--------------+--------------+         +--------------+--------------+
        | Background Dispatch Worker 1|         | Background Dispatch Worker 2|
        +--------------+--------------+         +--------------+--------------+
                       |                                       |
           +-----------+-----------+               +-----------+-----------+
           | Channel Dispatcher:   |               | Channel Dispatcher:   |
           | - EMAIL (SMTP/SES)    |               | - SMS (Twilio/SNS)    |
           | - WEBHOOK (HTTP POST) |               | - PUSH (FCM/APNS)     |
           +-----------+-----------+               +-----------+-----------+
                       |                                       |
                       +-------------------+-------------------+
                                           |
                                           | 4. XACK processed message
                                           | 5. Update PostgreSQL status to "DELIVERED"
                                           v
                               +-----------+-----------+
                               | PostgreSQL Database   |
                               | Notification Log &    |
                               | Delivery Audit Trail  |
                               +-----------------------+
```

### 1.2 Directory Layout

```
06-notification-service/
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
|   |-- channels/
|   |   |-- __init__.py
|   |   |-- base.py
|   |   |-- email_channel.py
|   |   |-- sms_channel.py
|   |   `-- webhook_channel.py
|   |-- workers/
|   |   |-- __init__.py
|   |   `-- stream_consumer.py
|   `-- api/
|       |-- __init__.py
|       `-- v1/
|           `-- notifications.py
`-- tests/
    |-- __init__.py
    |-- conftest.py
    `-- test_notifications_api.py
```

---

## 2. Complete Production Implementation

### 2.1 `pyproject.toml`

```toml
[project]
name = "fastapi-notification-service"
version = "0.1.0"
description = "Event-driven multi-channel notification dispatcher using Redis Streams"
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
    "jinja2>=3.1.4",
    "httpx>=0.27.0",
    "orjson>=3.10.0",
]

[project.optional-dependencies]
dev = [
    "pytest>=8.3.0",
    "pytest-asyncio>=0.24.0",
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
    PROJECT_NAME: str = "Notification Dispatcher"
    ENVIRONMENT: str = "production"
    
    # Database
    DATABASE_URL: str = "postgresql+asyncpg://postgres:postgres@localhost:5432/notif_db"
    
    # Redis
    REDIS_URL: str = "redis://localhost:6379/0"
    STREAM_NAME: str = "stream:notifications"
    CONSUMER_GROUP: str = "notif_workers_group"
    CONSUMER_NAME: str = "worker-1"
    
    # SMTP / Mock Settings
    SMTP_HOST: str = "smtp.mailtrap.io"
    SMTP_PORT: int = 2525
    SMTP_USER: str = "mock"
    SMTP_PASSWORD: str = "mock"

    model_config = SettingsConfigDict(
        env_file=".env",
        extra="ignore"
    )


@lru_cache
def get_settings() -> Settings:
    return Settings()
```

### 2.3 `src/domain/models.py` & `src/domain/schemas.py`

```python
# src/domain/models.py
import enum
import uuid
from datetime import datetime, timezone
from sqlalchemy import DateTime, Enum, Index, String, Text
from sqlalchemy.dialects.postgresql import JSONB, UUID
from sqlalchemy.orm import DeclarativeBase, Mapped, mapped_column


class Base(DeclarativeBase):
    pass


class ChannelType(str, enum.Enum):
    EMAIL = "EMAIL"
    SMS = "SMS"
    WEBHOOK = "WEBHOOK"


class NotificationStatus(str, enum.Enum):
    QUEUED = "QUEUED"
    PROCESSING = "PROCESSING"
    DELIVERED = "DELIVERED"
    FAILED = "FAILED"


class Notification(Base):
    __tablename__ = "notifications"

    id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4)
    channel: Mapped[ChannelType] = mapped_column(Enum(ChannelType, native_enum=False), nullable=False)
    recipient: Mapped[str] = mapped_column(String(255), nullable=False)
    subject: Mapped[str | None] = mapped_column(String(255), nullable=True)
    body: Mapped[str] = mapped_column(Text, nullable=False)
    template_data: Mapped[dict | None] = mapped_column(JSONB, nullable=True)
    status: Mapped[NotificationStatus] = mapped_column(
        Enum(NotificationStatus, native_enum=False),
        default=NotificationStatus.QUEUED,
        nullable=False,
        index=True,
    )
    error_reason: Mapped[str | None] = mapped_column(Text, nullable=True)
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True),
        default=lambda: datetime.now(timezone.utc),
        nullable=False,
    )
    updated_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True),
        default=lambda: datetime.now(timezone.utc),
        onupdate=lambda: datetime.now(timezone.utc),
        nullable=False,
    )
```

```python
# src/domain/schemas.py
import uuid
from datetime import datetime
from pydantic import BaseModel, ConfigDict, EmailStr, Field
from src.domain.models import ChannelType, NotificationStatus


class SendNotificationRequest(BaseModel):
    channel: ChannelType
    recipient: str = Field(..., description="Email address, E.164 phone number, or Webhook URL")
    subject: str | None = Field(default=None, max_length=255)
    body_template: str = Field(..., description="Jinja2 template string")
    context: dict[str, str | int | float] = Field(default_factory=dict)


class NotificationResponse(BaseModel):
    id: uuid.UUID
    channel: ChannelType
    recipient: str
    status: NotificationStatus
    created_at: datetime

    model_config = ConfigDict(from_attributes=True)
```

### 2.4 `src/channels/base.py` & Multi-Channel Providers

```python
# src/channels/base.py
from abc import ABC, abstractmethod
import jinja2


class BaseNotificationChannel(ABC):
    @abstractmethod
    async def deliver(self, recipient: str, subject: str | None, body: str) -> None:
        pass

    def render(self, template_str: str, context: dict) -> str:
        template = jinja2.Template(template_str)
        return template.render(**context)
```

```python
# src/channels/email_channel.py
import logging
from src.channels.base import BaseNotificationChannel

logger = logging.getLogger("email_channel")


class EmailChannel(BaseNotificationChannel):
    async def deliver(self, recipient: str, subject: str | None, body: str) -> None:
        # Mock SMTP / AWS SES delivery
        logger.info("[EMAIL SENT] To: %s | Subject: %s | Body: %s", recipient, subject, body[:40])
```

```python
# src/channels/webhook_channel.py
import httpx
import logging
from src.channels.base import BaseNotificationChannel

logger = logging.getLogger("webhook_channel")


class WebhookChannel(BaseNotificationChannel):
    async def deliver(self, recipient: str, subject: str | None, body: str) -> None:
        async with httpx.AsyncClient(timeout=10.0) as client:
            resp = await client.post(recipient, json={"subject": subject, "payload": body})
            resp.raise_for_status()
            logger.info("[WEBHOOK DELIVERED] URL: %s | Status: %s", recipient, resp.status_code)
```

### 2.5 `src/workers/stream_consumer.py`

```python
import asyncio
import logging
import orjson
import redis.asyncio as aioredis
from sqlalchemy import select
from src.channels.email_channel import EmailChannel
from src.channels.webhook_channel import WebhookChannel
from src.config import Settings
from src.database import async_session_factory
from src.domain.models import ChannelType, Notification, NotificationStatus

logger = logging.getLogger("stream_consumer")

CHANNELS = {
    ChannelType.EMAIL.value: EmailChannel(),
    ChannelType.WEBHOOK.value: WebhookChannel(),
}


async def init_redis_group(redis_client: aioredis.Redis, settings: Settings) -> None:
    try:
        await redis_client.xgroup_create(
            settings.STREAM_NAME, settings.CONSUMER_GROUP, id="0", mkstream=True
        )
    except aioredis.ResponseError as exc:
        if "BUSYGROUP" not in str(exc):
            raise


async def process_notification_worker(redis_client: aioredis.Redis, settings: Settings) -> None:
    await init_redis_group(redis_client, settings)
    logger.info("Notification stream consumer started on group '%s'", settings.CONSUMER_GROUP)

    while True:
        try:
            # Read 10 messages from stream
            entries = await redis_client.xreadgroup(
                settings.CONSUMER_GROUP,
                settings.CONSUMER_NAME,
                {settings.STREAM_NAME: ">"},
                count=10,
                block=2000,
            )

            if not entries:
                continue

            for stream_name, messages in entries:
                for msg_id, raw_data in messages:
                    payload = orjson.loads(raw_data["data"])
                    notif_id = payload["id"]
                    channel_name = payload["channel"]
                    recipient = payload["recipient"]
                    subject = payload.get("subject")
                    body = payload["body"]

                    async with async_session_factory() as session:
                        stmt = select(Notification).where(Notification.id == notif_id)
                        res = await session.execute(stmt)
                        notif = res.scalars().first()

                        try:
                            dispatcher = CHANNELS.get(channel_name, EmailChannel())
                            await dispatcher.deliver(recipient, subject, body)
                            if notif:
                                notif.status = NotificationStatus.DELIVERED
                            await session.commit()
                            await redis_client.xack(settings.STREAM_NAME, settings.CONSUMER_GROUP, msg_id)
                        except Exception as err:
                            logger.error("Failed to deliver notification %s: %s", notif_id, err)
                            if notif:
                                notif.status = NotificationStatus.FAILED
                                notif.error_reason = str(err)
                            await session.commit()
                            # Acknowledge to prevent infinite reprocessing loop or send to DLQ
                            await redis_client.xack(settings.STREAM_NAME, settings.CONSUMER_GROUP, msg_id)
        except asyncio.CancelledError:
            break
        except Exception as exc:
            logger.error("Stream reader exception: %s", exc)
            await asyncio.sleep(1)
```

### 2.6 `src/main.py` & API Routers

```python
# src/api/v1/notifications.py
import jinja2
import orjson
from fastapi import APIRouter, Depends, HTTPException, status
import redis.asyncio as aioredis
from sqlalchemy.ext.asyncio import AsyncSession

from src.config import Settings, get_settings
from src.database import get_db_session
from src.domain.models import Notification, NotificationStatus
from src.domain.schemas import NotificationResponse, SendNotificationRequest
from src.redis_client import get_redis

router = APIRouter(prefix="/notifications", tags=["Notifications"])


@router.post("/send", response_model=NotificationResponse, status_code=status.HTTP_202_ACCEPTED)
async def dispatch_notification(
    payload: SendNotificationRequest,
    db: AsyncSession = Depends(get_db_session),
    redis: aioredis.Redis = Depends(get_redis),
    settings: Settings = Depends(get_settings),
):
    # 1. Render Template
    try:
        template = jinja2.Template(payload.body_template)
        rendered_body = template.render(**payload.context)
    except Exception as err:
        raise HTTPException(status_code=status.HTTP_422_UNPROCESSABLE_ENTITY, detail=f"Template error: {err}")

    # 2. Persist in Postgres
    notif = Notification(
        channel=payload.channel,
        recipient=payload.recipient,
        subject=payload.subject,
        body=rendered_body,
        template_data=payload.context,
        status=NotificationStatus.QUEUED,
    )
    db.add(notif)
    await db.commit()
    await db.refresh(notif)

    # 3. Enqueue to Redis Stream
    stream_payload = {
        "id": str(notif.id),
        "channel": payload.channel.value,
        "recipient": payload.recipient,
        "subject": payload.subject,
        "body": rendered_body,
    }
    await redis.xadd(settings.STREAM_NAME, {"data": orjson.dumps(stream_payload)})

    return notif
```

```python
# src/main.py
import asyncio
from contextlib import asynccontextmanager
from fastapi import FastAPI
from src.api.v1.notifications import router as notif_router
from src.config import get_settings
from src.database import engine
from src.domain.models import Base
from src.redis_client import get_redis_client, redis_pool
from src.workers.stream_consumer import process_notification_worker

settings = get_settings()


@asynccontextmanager
async def lifespan(app: FastAPI):
    async with engine.begin() as conn:
        await conn.run_sync(Base.metadata.create_all)
    
    redis = get_redis_client()
    worker_task = asyncio.create_task(process_notification_worker(redis, settings))
    yield
    worker_task.cancel()
    try:
        await worker_task
    except asyncio.CancelledError:
        pass
    await redis.aclose()
    await redis_pool.disconnect()
    await engine.dispose()


app = FastAPI(
    title=settings.PROJECT_NAME,
    lifespan=lifespan,
    docs_url="/docs",
    redoc_url="/redoc",
)

app.include_router(notif_router, prefix="/api/v1")
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

from src.database import get_db_session
from src.domain.models import Base
from src.main import app
from src.redis_client import get_redis

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

### 3.2 `tests/test_notifications_api.py`

```python
import pytest
from httpx import AsyncClient


@pytest.mark.asyncio
async def test_enqueue_notification_email(client: AsyncClient):
    payload = {
        "channel": "EMAIL",
        "recipient": "developer@example.com",
        "subject": "Order #{{ order_id }} Confirmed",
        "body_template": "Hello {{ name }}, your total is ${{ total }}.",
        "context": {"order_id": 9942, "name": "Alice", "total": 129.50},
    }
    resp = await client.post("/api/v1/notifications/send", json=payload)
    assert resp.status_code == 202
    data = resp.json()
    assert data["recipient"] == "developer@example.com"
    assert data["status"] == "QUEUED"
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
      - DATABASE_URL=postgresql+asyncpg://postgres:postgres@db:5432/notif_db
      - REDIS_URL=redis://redis:6379/0
    depends_on:
      - db
      - redis

  db:
    image: postgres:16-alpine
    environment:
      POSTGRES_USER: postgres
      POSTGRES_PASSWORD: postgres
      POSTGRES_DB: notif_db
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
# 1. Enqueue templated notification
curl -X POST "http://localhost:8000/api/v1/notifications/send" \
     -H "Content-Type: application/json" \
     -d '{
       "channel": "EMAIL",
       "recipient": "user@example.com",
       "subject": "Welcome {{ user_name }}!",
       "body_template": "Thanks for signing up for our service. Your code is {{ pin }}.",
       "context": {"user_name": "Sarah", "pin": 4829}
     }'

# 2. Inspect Redis stream entries via redis-cli
docker exec -it notification_redis redis-cli XREAD COUNT 2 STREAMS stream:notifications 0-0

# 3. Run automated tests
pytest -v tests/
```
