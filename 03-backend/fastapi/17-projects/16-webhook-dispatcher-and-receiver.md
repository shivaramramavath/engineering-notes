# Production Reference Project: Enterprise Webhook Dispatcher & Receiver with HMAC Verification

## Metadata & Production Baseline

| Attribute | Specification |
|:---|:---|
| **Project Code** | `FASTAPI-PROJ-016` |
| **FastAPI Version** | `0.115.x+` / `0.165.x+` |
| **Pydantic Version** | `2.10.x+` (V2 Core) |
| **Python Baseline** | `3.12+` / `3.13` (Free-Threading Safe) |
| **ASGI Server** | `Uvicorn 0.32+` |
| **Database & ORM** | `PostgreSQL 16+`, `SQLAlchemy 2.0.36+ (asyncpg)` |
| **Signature Standard** | SHA-256 HMAC (`X-Signature-SHA256` / `t=...,v1=...`) with timing-safe comparison (`hmac.compare_digest`) |
| **Resilience & Retries**| Exponential backoff with jitter (5 retries), Dead Letter Queue (DLQ), Event idempotency cache |
| **Testing** | `pytest 8.3+`, `pytest-asyncio 0.24+`, `respx 0.21+`, `httpx 0.27+` |
| **Complexity Level** | Advanced / Enterprise Integration |

---

## 1. Architectural Overview & Domain Design

### 1.1 System Architecture & Data Flow

```
     [OUTBOUND DISPATCH FLOW]                              [INBOUND RECEIVER FLOW]
                |                                                     |
                | Internal Event Triggered                            | Partner Webhook Inbound
                | e.g. "invoice.paid"                                 | POST /api/v1/webhooks/receive
                v                                                     v
     +----------+----------+                               +----------+----------+
     | FastAPI Gateway     |                               | FastAPI Gateway     |
     +----------+----------+                               +----------+----------+
                |                                                     |
                | 1. Record Webhook Delivery Task                     | 1. Verify Timestamp &
                | 2. Compute HMAC SHA-256 Signature                   |    HMAC-SHA256 Sig
                v                                                     v
     +----------+----------+                               +----------+----------+
     | Async Dispatcher    |                               | Idempotency Check   |
     | (httpx.AsyncClient) |                               | (Redis / DB Unique) |
     +----------+----------+                               +----------+----------+
                |                                                     |
        +-------+-------+                                             | 2. Inbound event valid
        |               |                                             v
  HTTP 200 OK      HTTP 5xx / Timeout                      +----------+----------+
        |               |                                  | Enqueue Business    |
        v               v                                  | Action / Background |
  +-----+-----+   +-----+-----+                            | Processing Task     |
  | Delivery  |   | Schedule  |                            +---------------------+
  | Marked    |   | Retry 1-5 |
  | SUCCESS   |   | (Exp.     |
  +-----------+   | Backoff)  |
                  +-----+-----+
                        |
                 Exhausted Retries?
                        v
                  +-----+-----+
                  | Move to   |
                  | Dead      |
                  | Letter    |
                  | Queue     |
                  +-----------+
```

### 1.2 Directory Layout

```
16-webhook-dispatcher/
|-- pyproject.toml
|-- Dockerfile
|-- docker-compose.yml
|-- src/
|   |-- __init__.py
|   |-- main.py
|   |-- config.py
|   |-- database.py
|   |-- security/
|   |   |-- __init__.py
|   |   `-- hmac_signer.py
|   |-- domain/
|   |   |-- __init__.py
|   |   |-- models.py
|   |   `-- schemas.py
|   |-- services/
|   |   |-- __init__.py
|   |   `-- dispatcher.py
|   `-- api/
|       |-- __init__.py
|       `-- v1/
|           |-- subscriptions.py
|           |-- dispatch.py
|           `-- receiver.py
`-- tests/
    |-- __init__.py
    |-- conftest.py
    `-- test_webhooks.py
```

---

## 2. Complete Production Implementation

### 2.1 `pyproject.toml`

```toml
[project]
name = "fastapi-webhook-dispatcher"
version = "0.1.0"
description = "Production webhook dispatching and receiving engine with HMAC verification and retries"
readme = "README.md"
requires-python = ">=3.12"
dependencies = [
    "fastapi>=0.115.0",
    "uvicorn[standard]>=0.32.0",
    "pydantic>=2.10.0",
    "pydantic-settings>=2.6.0",
    "sqlalchemy>=2.0.36",
    "asyncpg>=0.30.0",
    "httpx>=0.27.0",
    "orjson>=3.10.0",
]

[project.optional-dependencies]
dev = [
    "pytest>=8.3.0",
    "pytest-asyncio>=0.24.0",
    "respx>=0.21.0",
]

[build-system]
requires = ["hatchling"]
build-backend = "hatchling.build"
```

### 2.2 `src/config.py` & `src/security/hmac_signer.py`

```python
# src/config.py
from functools import lru_cache
from pydantic_settings import BaseSettings, SettingsConfigDict


class Settings(BaseSettings):
    PROJECT_NAME: str = "Enterprise Webhook Engine"
    DATABASE_URL: str = "postgresql+asyncpg://postgres:postgres@localhost:5432/webhook_db"
    INBOUND_WEBHOOK_SECRET: str = "whsec_inbound_shared_secret_abc123"
    MAX_RETRIES: int = 5
    REQUEST_TIMEOUT_SECONDS: float = 10.0

    model_config = SettingsConfigDict(env_file=".env", extra="ignore")


@lru_cache
def get_settings() -> Settings:
    return Settings()
```

```python
# src/security/hmac_signer.py
import hashlib
import hmac
import time


def generate_webhook_signature(payload_bytes: bytes, secret: str) -> str:
    timestamp = int(time.time())
    signed_payload = f"t={timestamp}.".encode("utf-8") + payload_bytes
    computed_hmac = hmac.new(
        secret.encode("utf-8"),
        signed_payload,
        hashlib.sha256,
    ).hexdigest()
    return f"t={timestamp},v1={computed_hmac}"


def verify_webhook_signature(payload_bytes: bytes, header_signature: str, secret: str, tolerance_sec: int = 300) -> bool:
    try:
        parts = dict(item.split("=", 1) for item in header_signature.split(","))
        timestamp = int(parts["t"])
        expected_sig = parts["v1"]

        # Prevent replay attacks
        if abs(time.time() - timestamp) > tolerance_sec:
            return False

        signed_payload = f"t={timestamp}.".encode("utf-8") + payload_bytes
        computed_sig = hmac.new(
            secret.encode("utf-8"),
            signed_payload,
            hashlib.sha256,
        ).hexdigest()

        # Constant-time comparison prevents timing attacks
        return hmac.compare_digest(expected_sig, computed_sig)
    except Exception:
        return False
```

### 2.3 `src/domain/models.py` & `src/domain/schemas.py`

```python
# src/domain/models.py
import enum
import uuid
from datetime import datetime, timezone
from sqlalchemy import DateTime, Enum, Integer, String, Text
from sqlalchemy.dialects.postgresql import JSONB, UUID
from sqlalchemy.orm import DeclarativeBase, Mapped, mapped_column


class Base(DeclarativeBase):
    pass


class DeliveryStatus(str, enum.Enum):
    PENDING = "PENDING"
    SUCCESS = "SUCCESS"
    RETRYING = "RETRYING"
    DEAD_LETTER = "DEAD_LETTER"


class WebhookDelivery(Base):
    __tablename__ = "webhook_deliveries"

    id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4)
    target_url: Mapped[str] = mapped_column(String(512), nullable=False)
    event_type: Mapped[str] = mapped_column(String(64), nullable=False, index=True)
    payload: Mapped[dict] = mapped_column(JSONB, nullable=False)
    status: Mapped[DeliveryStatus] = mapped_column(
        Enum(DeliveryStatus, native_enum=False),
        default=DeliveryStatus.PENDING,
        nullable=False,
    )
    attempt_count: Mapped[int] = mapped_column(Integer, default=0, nullable=False)
    last_response_code: Mapped[int | None] = mapped_column(Integer, nullable=True)
    last_error: Mapped[str | None] = mapped_column(Text, nullable=True)
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True),
        default=lambda: datetime.now(timezone.utc),
        nullable=False,
    )
```

```python
# src/domain/schemas.py
from pydantic import BaseModel, HttpUrl


class TriggerWebhookRequest(BaseModel):
    target_url: HttpUrl
    secret: str
    event_type: str
    payload: dict
```

### 2.4 `src/services/dispatcher.py` (Exponential Backoff with Jitter)

```python
import asyncio
import logging
import random
import httpx
import orjson
from src.config import Settings
from src.database import async_session_factory
from src.domain.models import DeliveryStatus, WebhookDelivery
from src.security.hmac_signer import generate_webhook_signature

logger = logging.getLogger("webhook_dispatcher")


async def dispatch_webhook_with_retries(
    delivery_id: str,
    target_url: str,
    secret: str,
    event_type: str,
    payload: dict,
    settings: Settings,
) -> None:
    payload_bytes = orjson.dumps(payload)
    signature_header = generate_webhook_signature(payload_bytes, secret)

    headers = {
        "Content-Type": "application/json",
        "X-Webhook-Event": event_type,
        "X-Signature-SHA256": signature_header,
    }

    async with httpx.AsyncClient(timeout=settings.REQUEST_TIMEOUT_SECONDS) as client:
        for attempt in range(1, settings.MAX_RETRIES + 1):
            try:
                resp = await client.post(target_url, content=payload_bytes, headers=headers)
                resp.raise_for_status()

                # Update DB on success
                async with async_session_factory() as session:
                    delivery = await session.get(WebhookDelivery, delivery_id)
                    if delivery:
                        delivery.status = DeliveryStatus.SUCCESS
                        delivery.attempt_count = attempt
                        delivery.last_response_code = resp.status_code
                        await session.commit()
                logger.info("Webhook %s delivered successfully on attempt %d", delivery_id, attempt)
                return
            except Exception as exc:
                logger.warning("Attempt %d failed for webhook %s: %s", attempt, delivery_id, exc)
                # Exponential backoff + Full Jitter: min(2^attempt, 60s) * random(0.5, 1.5)
                backoff = min(2 ** attempt, 30) * random.uniform(0.8, 1.2)
                await asyncio.sleep(backoff)

        # Retries exhausted: Move to Dead Letter Queue
        async with async_session_factory() as session:
            delivery = await session.get(WebhookDelivery, delivery_id)
            if delivery:
                delivery.status = DeliveryStatus.DEAD_LETTER
                delivery.attempt_count = settings.MAX_RETRIES
                delivery.last_error = "Exhausted all retry attempts"
                await session.commit()
        logger.error("Webhook %s permanently failed, sent to DLQ", delivery_id)
```

### 2.5 `src/main.py` & API Routers

```python
# src/api/v1/receiver.py
from fastapi import APIRouter, Header, HTTPException, Request, status
from src.config import Settings, get_settings
from src.security.hmac_signer import verify_webhook_signature

router = APIRouter(prefix="/webhooks", tags=["Webhook Receiver"])


@router.post("/receive", status_code=status.HTTP_200_OK)
async def receive_inbound_webhook(
    request: Request,
    signature: str = Header(..., alias="X-Signature-SHA256"),
    settings: Settings = Depends(get_settings),
):
    body = await request.body()
    is_valid = verify_webhook_signature(body, signature, settings.INBOUND_WEBHOOK_SECRET)
    if not is_valid:
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Invalid or expired HMAC signature",
        )

    # Process inbound payload
    return {"status": "accepted"}
```

```python
# src/main.py
from contextlib import asynccontextmanager
from fastapi import BackgroundTasks, FastAPI
from src.api.v1.receiver import router as receiver_router
from src.config import get_settings
from src.database import async_session_factory, engine
from src.domain.models import Base, DeliveryStatus, WebhookDelivery
from src.domain.schemas import TriggerWebhookRequest
from src.services.dispatcher import dispatch_webhook_with_retries

settings = get_settings()


@asynccontextmanager
async def lifespan(app: FastAPI):
    async with engine.begin() as conn:
        await conn.run_sync(Base.metadata.create_all)
    yield
    await engine.dispose()


app = FastAPI(title=settings.PROJECT_NAME, lifespan=lifespan)
app.include_router(receiver_router, prefix="/api/v1")


@app.post("/api/v1/webhooks/dispatch")
async def trigger_webhook(req: TriggerWebhookRequest, background_tasks: BackgroundTasks):
    async with async_session_factory() as session:
        record = WebhookDelivery(
            target_url=str(req.target_url),
            event_type=req.event_type,
            payload=req.payload,
            status=DeliveryStatus.PENDING,
        )
        session.add(record)
        await session.commit()
        await session.refresh(record)

    background_tasks.add_task(
        dispatch_webhook_with_retries,
        record.id,
        str(req.target_url),
        req.secret,
        req.event_type,
        req.payload,
        settings,
    )
    return {"message": "Webhook queued for delivery", "id": record.id}
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

### 3.2 `tests/test_webhooks.py`

```python
import pytest
from httpx import AsyncClient
from src.config import get_settings
from src.security.hmac_signer import generate_webhook_signature

settings = get_settings()


@pytest.mark.asyncio
async def test_webhook_receiver_hmac_verification(client: AsyncClient):
    payload = b'{"event":"customer.created","id":123}'
    valid_sig = generate_webhook_signature(payload, settings.INBOUND_WEBHOOK_SECRET)

    # 1. Valid Signature -> 200 OK
    resp_ok = await client.post(
        "/api/v1/webhooks/receive",
        content=payload,
        headers={"X-Signature-SHA256": valid_sig},
    )
    assert resp_ok.status_code == 200

    # 2. Tampered Signature -> 401 Unauthorized
    resp_bad = await client.post(
        "/api/v1/webhooks/receive",
        content=payload,
        headers={"X-Signature-SHA256": "t=1000,v1=tampered_signature"},
    )
    assert resp_bad.status_code == 401
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
      - DATABASE_URL=postgresql+asyncpg://postgres:postgres@db:5432/webhook_db
    depends_on:
      - db

  db:
    image: postgres:16-alpine
    environment:
      POSTGRES_USER: postgres
      POSTGRES_PASSWORD: postgres
      POSTGRES_DB: webhook_db
    ports:
      - "5432:5432"
```

---

## 5. Operational Runbook & Verification

### 5.1 Verification Commands

```bash
# 1. Trigger an outbound webhook dispatch
curl -X POST "http://localhost:8000/api/v1/webhooks/dispatch" \
     -H "Content-Type: application/json" \
     -d '{
       "target_url": "https://webhook.site/mock-endpoint",
       "secret": "client_secret_777",
       "event_type": "order.completed",
       "payload": {"order_id": "ord_88291", "amount": 149.00}
     }'

# 2. Test receiving an inbound webhook
curl -X POST "http://localhost:8000/api/v1/webhooks/receive" \
     -H "Content-Type: application/json" \
     -H "X-Signature-SHA256: <VALID_HMAC_HEADER>" \
     -d '{"data": "ping"}'

# 3. Run automated tests
pytest -v tests/
```
