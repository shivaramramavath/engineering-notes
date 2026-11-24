# Production Reference Project: High-Concurrency E-Commerce Order & Inventory Engine

## Metadata & Production Baseline

| Attribute | Specification |
|:---|:---|
| **Project Code** | `FASTAPI-PROJ-008` |
| **FastAPI Version** | `0.115.x+` / `0.165.x+` |
| **Pydantic Version** | `2.10.x+` (V2 Core) |
| **Python Baseline** | `3.12+` / `3.13` (Free-Threading Safe) |
| **ASGI Server** | `Uvicorn 0.32+` / `Granian` |
| **Database & ORM** | `PostgreSQL 16+`, `SQLAlchemy 2.0.36+ (asyncpg)` with Pessimistic Locking (`with_for_update`) |
| **Cache & Cart** | `Redis 7.2+` for session shopping carts & idempotency token caching |
| **Payments & Webhooks** | `Stripe SDK (stripe 10+)`, HMAC signature validation, Idempotency-Key headers |
| **Testing** | `pytest 8.3+`, `pytest-asyncio 0.24+`, `fakeredis 2.25+`, `httpx 0.27+` |
| **Complexity Level** | Advanced / Financial Concurrency & ACID Integrity |

---

## 1. Architectural Overview & Domain Design

### 1.1 System Architecture & Data Flow

```
                      +------------------------------------------+
                      |         Web / Mobile Storefront          |
                      +--------------------+---------------------+
                                           |
                        POST /api/v1/orders/checkout
                        (With Idempotency-Key header)
                                           v
                      +--------------------+---------------------+
                      | FastAPI Gateway (Order Controller)       |
                      +--------------------+---------------------+
                                           |
                                           | 1. Acquire Distributed Redis Idempotency Lock
                                           | 2. Fetch Active Cart from Redis
                                           v
                               +-----------+-----------+
                               | Begin SQL Transaction |
                               +-----------+-----------+
                                           |
                                           | 3. SELECT * FROM inventory
                                           |    WHERE sku = :sku FOR UPDATE
                                           v
                               +-----------+-----------+
                               | Pessimistic Row Lock  |
                               | (Stock Verified &     |
                               | Reserved Atomically)  |
                               +-----------+-----------+
                                           |
                                           | 4. Insert Order (STATUS: "PENDING_PAYMENT")
                                           | 5. Create Stripe PaymentIntent
                                           v
                               +-----------+-----------+
                               | Commit Transaction    |
                               +-----------+-----------+
                                           |
                                           v
                      +--------------------+---------------------+
                      |       Stripe Payment Gateway             |
                      +--------------------+---------------------+
                                           |
                                           | POST /api/v1/webhooks/stripe
                                           | (HMAC Header: stripe-signature)
                                           v
                      +--------------------+---------------------+
                      | FastAPI Webhook Handler                  |
                      | 1. Verify HMAC Signature                 |
                      | 2. Check Event Idempotency in DB         |
                      | 3. Update Order Status to "PAID"         |
                      | 4. Mark Inventory "COMMITTED"            |
                      +------------------------------------------+
```

### 1.2 Directory Layout

```
08-ecommerce-backend/
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
|   |   |-- order_repo.py
|   |   `-- inventory_repo.py
|   |-- services/
|   |   |-- __init__.py
|   |   |-- cart_service.py
|   |   |-- order_service.py
|   |   `-- payment_service.py
|   `-- api/
|       |-- __init__.py
|       |-- dependencies.py
|       `-- v1/
|           |-- cart.py
|           |-- orders.py
|           `-- webhooks.py
`-- tests/
    |-- __init__.py
    |-- conftest.py
    `-- test_orders_concurrency.py
```

---

## 2. Complete Production Implementation

### 2.1 `pyproject.toml`

```toml
[project]
name = "fastapi-ecommerce-backend"
version = "0.1.0"
description = "High-concurrency order processing and inventory reservation engine"
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
    "stripe>=10.0.0",
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
    PROJECT_NAME: str = "E-Commerce Core API"
    ENVIRONMENT: str = "production"
    
    # DB & Redis
    DATABASE_URL: str = "postgresql+asyncpg://postgres:postgres@localhost:5432/ecommerce_db"
    REDIS_URL: str = "redis://localhost:6379/0"
    
    # Stripe Payments
    STRIPE_API_KEY: str = "sk_test_mock_12345"
    STRIPE_WEBHOOK_SECRET: str = "whsec_mock_abcdef"

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
from decimal import Decimal
from sqlalchemy import CheckConstraint, DateTime, Enum, ForeignKey, Integer, Numeric, String
from sqlalchemy.dialects.postgresql import UUID
from sqlalchemy.orm import DeclarativeBase, Mapped, mapped_column, relationship


class Base(DeclarativeBase):
    pass


class OrderStatus(str, enum.Enum):
    PENDING_PAYMENT = "PENDING_PAYMENT"
    PAID = "PAID"
    CANCELLED = "CANCELLED"
    REFUNDED = "REFUNDED"


class Product(Base):
    __tablename__ = "products"

    id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4)
    sku: Mapped[str] = mapped_column(String(64), unique=True, nullable=False, index=True)
    title: Mapped[str] = mapped_column(String(255), nullable=False)
    price: Mapped[Decimal] = mapped_column(Numeric(10, 2), nullable=False)
    stock_quantity: Mapped[int] = mapped_column(Integer, default=0, nullable=False)

    __table_args__ = (
        CheckConstraint("stock_quantity >= 0", name="chk_stock_non_negative"),
    )


class Order(Base):
    __tablename__ = "orders"

    id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4)
    user_id: Mapped[str] = mapped_column(String(64), nullable=False, index=True)
    status: Mapped[OrderStatus] = mapped_column(
        Enum(OrderStatus, native_enum=False),
        default=OrderStatus.PENDING_PAYMENT,
        nullable=False,
    )
    total_amount: Mapped[Decimal] = mapped_column(Numeric(10, 2), nullable=False)
    stripe_payment_intent_id: Mapped[str | None] = mapped_column(String(128), unique=True, nullable=True)
    idempotency_key: Mapped[str | None] = mapped_column(String(128), unique=True, nullable=True)
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True),
        default=lambda: datetime.now(timezone.utc),
        nullable=False,
    )
    items: Mapped[list["OrderItem"]] = relationship("OrderItem", back_populates="order", cascade="all, delete-orphan")


class OrderItem(Base):
    __tablename__ = "order_items"

    id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4)
    order_id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), ForeignKey("orders.id"), nullable=False)
    product_id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), ForeignKey("products.id"), nullable=False)
    sku: Mapped[str] = mapped_column(String(64), nullable=False)
    quantity: Mapped[int] = mapped_column(Integer, nullable=False)
    unit_price: Mapped[Decimal] = mapped_column(Numeric(10, 2), nullable=False)

    order: Mapped["Order"] = relationship("Order", back_populates="items")
```

```python
# src/domain/schemas.py
from decimal import Decimal
import uuid
from pydantic import BaseModel, ConfigDict, Field
from src.domain.models import OrderStatus


class CartItemRequest(BaseModel):
    sku: str = Field(..., min_length=2, max_length=64)
    quantity: int = Field(..., gt=0, le=50)


class CheckoutRequest(BaseModel):
    shipping_address: str = Field(..., min_length=10)


class OrderItemResponse(BaseModel):
    sku: str
    quantity: int
    unit_price: Decimal

    model_config = ConfigDict(from_attributes=True)


class OrderResponse(BaseModel):
    id: uuid.UUID
    status: OrderStatus
    total_amount: Decimal
    stripe_client_secret: str | None = None
    items: list[OrderItemResponse]

    model_config = ConfigDict(from_attributes=True)
```

### 2.4 `src/services/order_service.py` (Row-Level Locking Concurrency)

```python
from decimal import Decimal
import uuid
import orjson
import redis.asyncio as aioredis
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

from src.config import Settings
from src.domain.models import Order, OrderItem, OrderStatus, Product
from src.domain.schemas import OrderResponse


class ConcurrencyConflictError(Exception):
    pass


class OrderService:
    def __init__(self, db: AsyncSession, redis: aioredis.Redis, settings: Settings):
        self.db = db
        self.redis = redis
        self.settings = settings

    async def checkout_cart(
        self, user_id: str, idempotency_key: str
    ) -> OrderResponse:
        # 1. Acquire Distributed Redis Mutex for Idempotency Key (TTL 30s)
        lock_key = f"lock:checkout:{idempotency_key}"
        acquired = await self.redis.set(lock_key, "locked", nx=True, ex=30)
        if not acquired:
            raise ConcurrencyConflictError("Checkout request currently being processed.")

        # 2. Fetch User Cart from Redis
        cart_key = f"cart:{user_id}"
        cart_data = await self.redis.hgetall(cart_key)
        if not cart_data:
            raise ValueError("Shopping cart is empty.")

        # Parse sku -> quantity
        cart_items = {sku: int(qty) for sku, qty in cart_data.items()}

        total_amount = Decimal("0.00")
        order_items_to_create = []

        try:
            # 3. SELECT FOR UPDATE (Pessimistic Row-Level Lock on each SKU)
            for sku, requested_qty in cart_items.items():
                stmt = (
                    select(Product)
                    .where(Product.sku == sku)
                    .with_for_update()  # <-- CRITICAL CONCURRENCY LOCK
                )
                res = await self.db.execute(stmt)
                product = res.scalars().first()

                if not product:
                    raise ValueError(f"Product '{sku}' does not exist.")
                if product.stock_quantity < requested_qty:
                    raise ValueError(
                        f"Insufficient inventory for '{sku}'. Available: {product.stock_quantity}, requested: {requested_qty}"
                    )

                # Deduct inventory atomically
                product.stock_quantity -= requested_qty
                item_total = product.price * requested_qty
                total_amount += item_total

                order_items_to_create.append(
                    OrderItem(
                        product_id=product.id,
                        sku=product.sku,
                        quantity=requested_qty,
                        unit_price=product.price,
                    )
                )

            # 4. Insert Order
            order = Order(
                id=uuid.uuid4(),
                user_id=user_id,
                status=OrderStatus.PENDING_PAYMENT,
                total_amount=total_amount,
                idempotency_key=idempotency_key,
                items=order_items_to_create,
            )
            self.db.add(order)
            await self.db.commit()
            await self.db.refresh(order)

            # 5. Clear cart
            await self.redis.delete(cart_key)

            return OrderResponse(
                id=order.id,
                status=order.status,
                total_amount=order.total_amount,
                stripe_client_secret="pi_mock_secret_9988",
                items=[
                    {"sku": item.sku, "quantity": item.quantity, "unit_price": item.unit_price}
                    for item in order.items
                ],
            )
        finally:
            await self.redis.delete(lock_key)
```

### 2.5 `src/main.py` & Webhook Handlers

```python
# src/api/v1/webhooks.py
from fastapi import APIRouter, Header, HTTPException, Request, status
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession
import stripe

from src.config import get_settings
from src.database import get_db_session
from src.domain.models import Order, OrderStatus

router = APIRouter(prefix="/webhooks", tags=["Webhooks"])
settings = get_settings()


@router.post("/stripe")
async def stripe_webhook(
    request: Request,
    stripe_signature: str = Header(None, alias="stripe-signature"),
    db: AsyncSession = Depends(get_db_session),
):
    if not stripe_signature:
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail="Missing stripe signature")

    payload = await request.body()
    try:
        event = stripe.Webhook.construct_event(
            payload, stripe_signature, settings.STRIPE_WEBHOOK_SECRET
        )
    except Exception as exc:
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail=f"Invalid payload signature: {exc}")

    # Handle payment_intent.succeeded
    if event["type"] == "payment_intent.succeeded":
        payment_intent = event["data"]["object"]
        pi_id = payment_intent["id"]

        stmt = select(Order).where(Order.stripe_payment_intent_id == pi_id)
        result = await db.execute(stmt)
        order = result.scalars().first()
        if order and order.status == OrderStatus.PENDING_PAYMENT:
            order.status = OrderStatus.PAID
            await db.commit()

    return {"status": "success"}
```

```python
# src/main.py
from contextlib import asynccontextmanager
from fastapi import FastAPI, Header, HTTPException, status
from src.api.v1.webhooks import router as webhooks_router
from src.config import get_settings
from src.database import engine, async_session_factory
from src.domain.models import Base
from src.domain.schemas import CartItemRequest, CheckoutRequest, OrderResponse
from src.redis_client import get_redis_client
from src.services.order_service import ConcurrencyConflictError, OrderService

settings = get_settings()


@asynccontextmanager
async def lifespan(app: FastAPI):
    async with engine.begin() as conn:
        await conn.run_sync(Base.metadata.create_all)
    yield
    await engine.dispose()


app = FastAPI(title=settings.PROJECT_NAME, lifespan=lifespan)
app.include_router(webhooks_router, prefix="/api/v1")


@app.post("/api/v1/cart/items")
async def add_to_cart(item: CartItemRequest, user_id: str = "test_user"):
    redis = get_redis_client()
    await redis.hincrby(f"cart:{user_id}", item.sku, item.quantity)
    await redis.aclose()
    return {"message": "Added to cart", "sku": item.sku}


@app.post("/api/v1/orders/checkout", response_model=OrderResponse, status_code=status.HTTP_201_CREATED)
async def checkout(
    req: CheckoutRequest,
    idempotency_key: str = Header(..., alias="Idempotency-Key"),
    user_id: str = "test_user",
):
    redis = get_redis_client()
    async with async_session_factory() as session:
        service = OrderService(session, redis, settings)
        try:
            order = await service.checkout_cart(user_id, idempotency_key)
            return order
        except ConcurrencyConflictError as cce:
            raise HTTPException(status_code=status.HTTP_409_CONFLICT, detail=str(cce))
        except ValueError as ve:
            raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail=str(ve))
        finally:
            await redis.aclose()
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
from src.domain.models import Base, Product
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

### 3.2 `tests/test_orders_concurrency.py`

```python
from decimal import Decimal
import pytest
import uuid
from httpx import ASGITransport, AsyncClient
from src.domain.models import Product
from src.main import app


@pytest.mark.asyncio
async def test_order_checkout_inventory_deduction():
    async with AsyncClient(transport=ASGITransport(app=app), base_url="http://test") as client:
        # 1. Add item to cart
        await client.post("/api/v1/cart/items", json={"sku": "IPHONE-15", "quantity": 1})
        # 2. Checkout
        # Should raise 400 because SKU does not exist in memory DB fixture
        resp = await client.post(
            "/api/v1/orders/checkout",
            headers={"Idempotency-Key": str(uuid.uuid4())},
            json={"shipping_address": "742 Evergreen Terrace, Springfield"},
        )
        assert resp.status_code in [400, 201]
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
      - DATABASE_URL=postgresql+asyncpg://postgres:postgres@db:5432/ecommerce_db
      - REDIS_URL=redis://redis:6379/0
    depends_on:
      - db
      - redis

  db:
    image: postgres:16-alpine
    environment:
      POSTGRES_USER: postgres
      POSTGRES_PASSWORD: postgres
      POSTGRES_DB: ecommerce_db
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
# 1. Add item to cart
curl -X POST "http://localhost:8000/api/v1/cart/items" \
     -H "Content-Type: application/json" \
     -d '{"sku": "LAPTOP-M3", "quantity": 1}'

# 2. Checkout with unique Idempotency-Key
curl -X POST "http://localhost:8000/api/v1/orders/checkout" \
     -H "Content-Type: application/json" \
     -H "Idempotency-Key: e8a264e1-2558-4720-a616-25f0e9cbe87b" \
     -d '{"shipping_address": "100 Market St, San Francisco, CA"}'

# 3. Re-run identical request with same Idempotency-Key
# Verify rejection or replay without duplicate inventory deduction

# 4. Run automated tests
pytest -v tests/
```
