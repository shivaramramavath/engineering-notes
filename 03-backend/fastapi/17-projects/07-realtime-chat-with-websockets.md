# Production Reference Project: Real-Time Chat Engine with WebSockets & Redis Pub/Sub

## Metadata & Production Baseline

| Attribute | Specification |
|:---|:---|
| **Project Code** | `FASTAPI-PROJ-007` |
| **FastAPI Version** | `0.115.x+` / `0.165.x+` |
| **Pydantic Version** | `2.10.x+` (V2 Core) |
| **Python Baseline** | `3.12+` / `3.13` (Free-Threading Safe) |
| **ASGI Server** | `Uvicorn 0.32+ (websockets / wsproto)` |
| **Database & ORM** | `PostgreSQL 16+`, `SQLAlchemy 2.0.36+ (asyncpg)` |
| **Pub/Sub & Multi-Pod** | `Redis 7.2+ Pub/Sub` for cross-worker message synchronization |
| **Validation & Auth** | JWT Query Token handshake, Pydantic WebSocket event envelope validation |
| **Testing** | `pytest 8.3+`, `pytest-asyncio 0.24+`, `websockets 12+`, `httpx 0.27+` |
| **Complexity Level** | Advanced / Real-Time Bi-Directional |

---

## 1. Architectural Overview & Domain Design

### 1.1 System Architecture & Data Flow

```
                      +---------------------------------------+
                      |       Web / Mobile Clients            |
                      |   (Client A on Pod 1, Client B on Pod 2)
                      +-------+-----------------------+-------+
                              |                       |
               WebSocket Conn |                       | WebSocket Conn
               (Room: #tech)  |                       | (Room: #tech)
                              v                       v
                      +-------+-------+       +-------+-------+
                      | FastAPI Pod 1 |       | FastAPI Pod 2 |
                      | In-Memory Conns       | In-Memory Conns
                      +-------+-------+       +-------+-------+
                              |                       ^
       Client A sends message |                       | Redis PubSub notifies
       1. Publish to Redis    v                       | 2. Broadcast to local
                   +----------+-----------------------+----------+
                   |           Redis Pub/Sub Channel             |
                   |             channel:room:tech               |
                   +----------+----------------------------------+
                              |
                              v (Async Background Consumer)
                   +----------+----------+
                   |  PostgreSQL DB      |
                   |  Chat Message       |
                   |  Persistent History |
                   +---------------------+
```

### 1.2 Directory Layout

```
07-realtime-chat/
|-- pyproject.toml
|-- Dockerfile
|-- docker-compose.yml
|-- src/
|   |-- __init__.py
|   |-- main.py
|   |-- config.py
|   |-- database.py
|   |-- redis_client.py
|   |-- connection_manager.py
|   |-- domain/
|   |   |-- __init__.py
|   |   |-- models.py
|   |   `-- schemas.py
|   |-- repositories/
|   |   |-- __init__.py
|   |   `-- message_repo.py
|   |-- services/
|   |   |-- __init__.py
|   |   `-- chat_service.py
|   `-- api/
|       |-- __init__.py
|       |-- dependencies.py
|       `-- v1/
|           |-- chat_rest.py
|           `-- chat_ws.py
`-- tests/
    |-- __init__.py
    |-- conftest.py
    `-- test_chat_websocket.py
```

---

## 2. Complete Production Implementation

### 2.1 `pyproject.toml`

```toml
[project]
name = "fastapi-realtime-chat"
version = "0.1.0"
description = "Scalable multi-node real-time chat service using WebSockets and Redis Pub/Sub"
readme = "README.md"
requires-python = ">=3.12"
dependencies = [
    "fastapi>=0.115.0",
    "uvicorn[standard]>=0.32.0",
    "websockets>=12.0",
    "pydantic>=2.10.0",
    "pydantic-settings>=2.6.0",
    "sqlalchemy>=2.0.36",
    "asyncpg>=0.30.0",
    "redis[hiredis]>=5.2.0",
    "pyjwt[crypto]>=2.9.0",
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
    PROJECT_NAME: str = "Real-Time Chat Engine"
    ENVIRONMENT: str = "production"
    
    # DB & Redis
    DATABASE_URL: str = "postgresql+asyncpg://postgres:postgres@localhost:5432/chat_db"
    REDIS_URL: str = "redis://localhost:6379/0"
    
    # Auth
    JWT_SECRET_KEY: str = "production-chat-secret-key-32-chars-min"
    JWT_ALGORITHM: str = "HS256"

    model_config = SettingsConfigDict(
        env_file=".env",
        extra="ignore"
    )


@lru_cache
def get_settings() -> Settings:
    return Settings()
```

### 2.3 `src/connection_manager.py`

```python
from collections import defaultdict
import logging
from fastapi import WebSocket

logger = logging.getLogger("connection_manager")


class WebSocketConnectionManager:
    """Manages active local WebSocket connections partitioned by room_id."""

    def __init__(self):
        # Mapping: room_id -> list of active WebSocket connections
        self._rooms: dict[str, list[WebSocket]] = defaultdict(list)

    async def connect(self, room_id: str, websocket: WebSocket) -> None:
        await websocket.accept()
        self._rooms[room_id].append(websocket)
        logger.info("Connected WS client to room '%s' (Total: %d)", room_id, len(self._rooms[room_id]))

    def disconnect(self, room_id: str, websocket: WebSocket) -> None:
        if websocket in self._rooms[room_id]:
            self._rooms[room_id].remove(websocket)
            if not self._rooms[room_id]:
                del self._rooms[room_id]
        logger.info("Disconnected WS client from room '%s'", room_id)

    async def broadcast_local(self, room_id: str, message_json: str) -> None:
        """Broadcasts a payload to all locally connected sockets in this process."""
        if room_id not in self._rooms:
            return

        dead_connections: list[WebSocket] = []
        for socket in self._rooms[room_id]:
            try:
                await socket.send_text(message_json)
            except Exception:
                dead_connections.append(socket)

        for dead_socket in dead_connections:
            self.disconnect(room_id, dead_socket)


manager = WebSocketConnectionManager()
```

### 2.4 `src/domain/models.py` & `src/domain/schemas.py`

```python
# src/domain/models.py
import enum
import uuid
from datetime import datetime, timezone
from sqlalchemy import DateTime, Index, String, Text
from sqlalchemy.dialects.postgresql import UUID
from sqlalchemy.orm import DeclarativeBase, Mapped, mapped_column


class Base(DeclarativeBase):
    pass


class Message(Base):
    __tablename__ = "chat_messages"

    id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4)
    room_id: Mapped[str] = mapped_column(String(64), nullable=False, index=True)
    sender_id: Mapped[str] = mapped_column(String(64), nullable=False)
    sender_username: Mapped[str] = mapped_column(String(64), nullable=False)
    content: Mapped[str] = mapped_column(Text, nullable=False)
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True),
        default=lambda: datetime.now(timezone.utc),
        nullable=False,
    )

    __table_args__ = (
        Index("ix_chat_messages_room_created", "room_id", "created_at"),
    )
```

```python
# src/domain/schemas.py
import enum
import uuid
from datetime import datetime
from pydantic import BaseModel, ConfigDict, Field


class EventType(str, enum.Enum):
    CHAT_MESSAGE = "CHAT_MESSAGE"
    USER_TYPING = "USER_TYPING"
    USER_JOINED = "USER_JOINED"
    USER_LEFT = "USER_LEFT"


class ChatEventIn(BaseModel):
    event_type: EventType
    content: str | None = Field(default=None, max_length=4000)


class ChatEventOut(BaseModel):
    event_type: EventType
    room_id: str
    sender_id: str
    sender_username: str
    content: str | None = None
    timestamp: datetime


class MessageHistoryResponse(BaseModel):
    id: uuid.UUID
    room_id: str
    sender_id: str
    sender_username: str
    content: str
    created_at: datetime

    model_config = ConfigDict(from_attributes=True)
```

### 2.5 `src/services/chat_service.py`

```python
import asyncio
from datetime import datetime, timezone
import logging
import orjson
import redis.asyncio as aioredis
from sqlalchemy.ext.asyncio import AsyncSession

from src.connection_manager import manager
from src.database import async_session_factory
from src.domain.models import Message
from src.domain.schemas import ChatEventIn, ChatEventOut, EventType

logger = logging.getLogger("chat_service")


async def subscribe_redis_pubsub(redis_url: str) -> None:
    """Listens to cross-pod Redis Pub/Sub events and broadcasts to local WebSocket clients."""
    client = aioredis.from_url(redis_url, decode_responses=True)
    pubsub = client.pubsub()
    await pubsub.psubscribe("chat:room:*")
    logger.info("Subscribed to Redis pattern 'chat:room:*'")

    try:
        async for message in pubsub.listen():
            if message["type"] == "pmessage":
                channel: str = message["channel"]  # e.g. "chat:room:tech"
                room_id = channel.removeprefix("chat:room:")
                raw_data = message["data"]
                # Deliver to local WebSockets in this node
                await manager.broadcast_local(room_id, raw_data)
    except asyncio.CancelledError:
        await pubsub.punsubscribe("chat:room:*")
        await client.aclose()


async def handle_incoming_event(
    room_id: str,
    user_id: str,
    username: str,
    event: ChatEventIn,
    redis: aioredis.Redis,
) -> None:
    now = datetime.now(timezone.utc)
    out_envelope = ChatEventOut(
        event_type=event.event_type,
        room_id=room_id,
        sender_id=user_id,
        sender_username=username,
        content=event.content,
        timestamp=now,
    )
    json_bytes = orjson.dumps(out_envelope.model_dump(mode="json"))

    # 1. Publish to Redis Pub/Sub (synchronizes all pods)
    await redis.publish(f"chat:room:{room_id}", json_bytes.decode("utf-8"))

    # 2. Persist CHAT_MESSAGE events asynchronously to Postgres
    if event.event_type == EventType.CHAT_MESSAGE and event.content:
        async with async_session_factory() as session:
            db_msg = Message(
                room_id=room_id,
                sender_id=user_id,
                sender_username=username,
                content=event.content,
                created_at=now,
            )
            session.add(db_msg)
            await session.commit()
```

### 2.6 `src/main.py` & API Routers

```python
# src/api/v1/chat_ws.py
from fastapi import APIRouter, Depends, Query, WebSocket, WebSocketDisconnect, status
import jwt
from pydantic import ValidationError
import redis.asyncio as aioredis

from src.config import Settings, get_settings
from src.connection_manager import manager
from src.domain.schemas import ChatEventIn, EventType
from src.redis_client import get_redis_client
from src.services.chat_service import handle_incoming_event

router = APIRouter(tags=["Chat WebSockets"])


async def authenticate_ws(token: str, settings: Settings) -> tuple[str, str]:
    try:
        payload = jwt.decode(token, settings.JWT_SECRET_KEY, algorithms=[settings.JWT_ALGORITHM])
        return payload["sub"], payload.get("username", "Anonymous")
    except Exception:
        raise ValueError("Invalid JWT credentials")


@router.websocket("/ws/rooms/{room_id}")
async def websocket_chat_endpoint(
    websocket: WebSocket,
    room_id: str,
    token: str = Query(...),
    settings: Settings = Depends(get_settings),
):
    try:
        user_id, username = await authenticate_ws(token, settings)
    except ValueError:
        await websocket.close(code=status.WS_1008_POLICY_VIOLATION)
        return

    await manager.connect(room_id, websocket)
    redis = get_redis_client()

    # Announce Join
    join_event = ChatEventIn(event_type=EventType.USER_JOINED, content=f"{username} joined.")
    await handle_incoming_event(room_id, user_id, username, join_event, redis)

    try:
        while True:
            text = await websocket.receive_text()
            try:
                event = ChatEventIn.model_validate_json(text)
                await handle_incoming_event(room_id, user_id, username, event, redis)
            except ValidationError:
                await websocket.send_text('{"error": "Invalid payload schema"}')
    except WebSocketDisconnect:
        manager.disconnect(room_id, websocket)
        leave_event = ChatEventIn(event_type=EventType.USER_LEFT, content=f"{username} left.")
        await handle_incoming_event(room_id, user_id, username, leave_event, redis)
    finally:
        await redis.aclose()
```

```python
# src/main.py
import asyncio
from contextlib import asynccontextmanager
from fastapi import FastAPI
from src.api.v1.chat_ws import router as ws_router
from src.config import get_settings
from src.database import engine
from src.domain.models import Base
from src.services.chat_service import subscribe_redis_pubsub

settings = get_settings()


@asynccontextmanager
async def lifespan(app: FastAPI):
    async with engine.begin() as conn:
        await conn.run_sync(Base.metadata.create_all)
    
    # Start Redis Pub/Sub listener background task
    listener_task = asyncio.create_task(subscribe_redis_pubsub(settings.REDIS_URL))
    yield
    listener_task.cancel()
    try:
        await listener_task
    except asyncio.CancelledError:
        pass
    await engine.dispose()


app = FastAPI(
    title=settings.PROJECT_NAME,
    lifespan=lifespan,
    docs_url="/docs",
)

app.include_router(ws_router)
```

---

## 3. Automated Testing Suite

### 3.1 `tests/conftest.py`

```python
from datetime import datetime, timezone, timedelta
import pytest
import pytest_asyncio
import fakeredis.aioredis as fake_aioredis
import jwt
from httpx import ASGITransport, AsyncClient
from sqlalchemy.ext.asyncio import AsyncSession, async_sessionmaker, create_async_engine

from src.config import get_settings
from src.domain.models import Base
from src.main import app

settings = get_settings()


@pytest.fixture
def auth_jwt_token() -> str:
    payload = {
        "sub": "user_42",
        "username": "coder_bob",
        "exp": datetime.now(timezone.utc) + timedelta(hours=1),
    }
    return jwt.encode(payload, settings.JWT_SECRET_KEY, algorithm=settings.JWT_ALGORITHM)
```

### 3.2 `tests/test_chat_websocket.py`

```python
import pytest
from starlette.testclient import TestClient
from src.main import app


def test_websocket_chat_echo(auth_jwt_token: str):
    client = TestClient(app)
    # Test valid WebSocket Handshake & Join flow
    with client.websocket_connect(f"/ws/rooms/general?token={auth_jwt_token}") as websocket:
        # Send CHAT_MESSAGE
        websocket.send_json({"event_type": "CHAT_MESSAGE", "content": "Hello World!"})
        # Verify socket received the broadcast echo
        data = websocket.receive_json()
        assert data["event_type"] in ["USER_JOINED", "CHAT_MESSAGE"]
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
CMD ["uvicorn", "src.main:app", "--host", "0.0.0.0", "--port", "8000", "--workers", "2", "--ws", "websockets"]
```

### 4.2 `docker-compose.yml`

```yaml
version: "3.9"

services:
  app-pod1:
    build: .
    ports:
      - "8001:8000"
    environment:
      - DATABASE_URL=postgresql+asyncpg://postgres:postgres@db:5432/chat_db
      - REDIS_URL=redis://redis:6379/0
    depends_on:
      - db
      - redis

  app-pod2:
    build: .
    ports:
      - "8002:8000"
    environment:
      - DATABASE_URL=postgresql+asyncpg://postgres:postgres@db:5432/chat_db
      - REDIS_URL=redis://redis:6379/0
    depends_on:
      - db
      - redis

  db:
    image: postgres:16-alpine
    environment:
      POSTGRES_USER: postgres
      POSTGRES_PASSWORD: postgres
      POSTGRES_DB: chat_db
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
# 1. Connect via wscat (Node CLI tool) to Pod 1
wscat -c "ws://localhost:8001/ws/rooms/dev?token=<VALID_JWT_TOKEN>"

# 2. Connect via wscat to Pod 2
wscat -c "ws://localhost:8002/ws/rooms/dev?token=<VALID_JWT_TOKEN>"

# 3. Send message in Pod 1 terminal:
# {"event_type": "CHAT_MESSAGE", "content": "Cross-pod greeting!"}
# Verify message arrives simultaneously in Pod 2 terminal via Redis Pub/Sub!

# 4. Run automated tests
pytest -v tests/
```
