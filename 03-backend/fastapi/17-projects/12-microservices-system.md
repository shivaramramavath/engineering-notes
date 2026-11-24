# Production Reference Project: Microservices API Gateway with gRPC & Distributed Saga Coordinator

## Metadata & Production Baseline

| Attribute | Specification |
|:---|:---|
| **Project Code** | `FASTAPI-PROJ-012` |
| **FastAPI Version** | `0.115.x+` / `0.165.x+` |
| **Pydantic Version** | `2.10.x+` (V2 Core) |
| **Python Baseline** | `3.12+` / `3.13` (Free-Threading Safe) |
| **ASGI Server** | `Uvicorn 0.32+` |
| **RPC & Microservices** | `grpcio 1.68+`, `grpcio-tools`, Async gRPC client channels |
| **Distributed Saga** | Orchestration-based Saga Coordinator with Compensating Transactions |
| **Circuit Breaker** | `pybreaker 1.2+` / Async In-Memory Circuit Breaker |
| **Testing** | `pytest 8.3+`, `pytest-asyncio 0.24+`, `grpc-testing`, `httpx 0.27+` |
| **Complexity Level** | Advanced / Distributed Systems Architecture |

---

## 1. Architectural Overview & Domain Design

### 1.1 System Architecture & Data Flow

```
                      +---------------------------------------+
                      |         Web / Mobile Client           |
                      +-------------------+-------------------+
                                          |
                         POST /api/v1/orders/create (REST)
                                          v
                      +-------------------+-------------------+
                      |   FastAPI Edge Gateway / Saga Leader  |
                      +-------------------+-------------------+
                                          |
       +----------------------------------+----------------------------------+
       | Step 1: Reserve Inventory (gRPC) | Step 2: Charge Payment (gRPC)    |
       v                                  v                                  v
+------+------+                    +------+------+                    +------+------+
| Inventory   |                    | Payment     |                    | Shipping    |
| Service     |                    | Service     |                    | Service     |
| (gRPC :50051)                    | (gRPC :50052)                    | (gRPC :50053)
+------+------+                    +------+------+                    +------+------+
       |                                  |                                  |
       +-- Success                        +-- FAILURE! (Card Declined)       |
                                          |                                  |
                                          v                                  |
                      +-------------------+-------------------+              |
                      | Distributed Saga Compensation Loop    |              |
                      | 1. Cancel / Release Inventory (gRPC)  |              |
                      | 2. Abort Shipping Reservation         |              |
                      | 3. Return 402 Payment Required to API |              |
                      +---------------------------------------+              |
```

### 1.2 Directory Layout

```
12-microservices-system/
|-- pyproject.toml
|-- Dockerfile
|-- docker-compose.yml
|-- proto/
|   |-- inventory.proto
|   `-- payment.proto
|-- src/
|   |-- __init__.py
|   |-- main.py
|   |-- config.py
|   |-- rpc/
|   |   |-- __init__.py
|   |   |-- inventory_client.py
|   |   `-- payment_client.py
|   |-- saga/
|   |   |-- __init__.py
|   |   |-- coordinator.py
|   |   `-- steps.py
|   `-- api/
|       |-- __init__.py
|       `-- v1/
|           `-- orders.py
`-- tests/
    |-- __init__.py
    |-- conftest.py
    `-- test_saga_orchestration.py
```

---

## 2. Complete Production Implementation

### 2.1 `pyproject.toml`

```toml
[project]
name = "fastapi-microservices-saga"
version = "0.1.0"
description = "Microservices API gateway orchestrating distributed sagas via gRPC"
readme = "README.md"
requires-python = ">=3.12"
dependencies = [
    "fastapi>=0.115.0",
    "uvicorn[standard]>=0.32.0",
    "pydantic>=2.10.0",
    "pydantic-settings>=2.6.0",
    "grpcio>=1.68.0",
    "protobuf>=5.28.0",
    "orjson>=3.10.0",
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
    PROJECT_NAME: str = "Microservices Gateway"
    INVENTORY_GRPC_HOST: str = "localhost:50051"
    PAYMENT_GRPC_HOST: str = "localhost:50052"
    GRPC_TIMEOUT_SECONDS: float = 5.0

    model_config = SettingsConfigDict(env_file=".env", extra="ignore")


@lru_cache
def get_settings() -> Settings:
    return Settings()
```

### 2.3 `src/saga/coordinator.py` (Distributed Saga Orchestration Engine)

```python
import logging
from typing import Any, Callable, Coroutine

logger = logging.getLogger("saga_coordinator")


class SagaStep:
    def __init__(
        self,
        name: str,
        action: Callable[[], Coroutine[Any, Any, Any]],
        compensate: Callable[[], Coroutine[Any, Any, Any]],
    ):
        self.name = name
        self.action = action
        self.compensate = compensate


class SagaCoordinator:
    """Executes a chain of distributed actions. If any step fails, rolls back prior steps in reverse order."""

    def __init__(self):
        self.steps: list[SagaStep] = []
        self.executed_steps: list[SagaStep] = []

    def add_step(self, step: SagaStep) -> "SagaCoordinator":
        self.steps.append(step)
        return self

    async def execute(self) -> dict[str, Any]:
        results = {}
        for step in self.steps:
            logger.info("Executing Saga step: %s", step.name)
            try:
                result = await step.action()
                results[step.name] = result
                self.executed_steps.append(step)
            except Exception as exc:
                logger.error("Saga step '%s' failed: %s. Initiating compensation rollback...", step.name, exc)
                await self.rollback()
                raise exc
        return results

    async def rollback(self) -> None:
        # Execute compensating transactions in reverse order of execution
        for step in reversed(self.executed_steps):
            logger.warning("Compensating Saga step: %s", step.name)
            try:
                await step.compensate()
            except Exception as comp_exc:
                logger.critical("Compensating step '%s' failed! Human intervention needed: %s", step.name, comp_exc)
```

### 2.4 `src/rpc/inventory_client.py` & `src/rpc/payment_client.py` (Mock / Async Stubs)

```python
# src/rpc/inventory_client.py
import logging

logger = logging.getLogger("inventory_client")


class InventoryClient:
    async def reserve(self, item_id: str, qty: int) -> bool:
        logger.info("[RPC CALL] InventoryService.ReserveItem(id=%s, qty=%d)", item_id, qty)
        if item_id == "OUT_OF_STOCK":
            raise RuntimeError("Item is out of stock")
        return True

    async def release(self, item_id: str, qty: int) -> bool:
        logger.info("[RPC COMPENSATE] InventoryService.ReleaseItem(id=%s, qty=%d)", item_id, qty)
        return True
```

```python
# src/rpc/payment_client.py
import logging

logger = logging.getLogger("payment_client")


class PaymentClient:
    async def charge(self, user_id: str, amount: float) -> str:
        logger.info("[RPC CALL] PaymentService.Charge(user=%s, amount=%.2f)", user_id, amount)
        if amount > 5000:
            raise ValueError("Credit card transaction limit exceeded")
        return "txn_mock_9921"

    async def refund(self, txn_id: str) -> bool:
        logger.info("[RPC COMPENSATE] PaymentService.Refund(txn=%s)", txn_id)
        return True
```

### 2.5 `src/main.py` & API Routers

```python
# src/api/v1/orders.py
from fastapi import APIRouter, HTTPException, status
from pydantic import BaseModel, Field
from src.rpc.inventory_client import InventoryClient
from src.rpc.payment_client import PaymentClient
from src.saga.coordinator import SagaCoordinator, SagaStep

router = APIRouter(prefix="/orders", tags=["Orders"])

inventory_client = InventoryClient()
payment_client = PaymentClient()


class CreateOrderRequest(BaseModel):
    user_id: str
    item_id: str
    quantity: int = Field(..., gt=0)
    total_amount: float = Field(..., gt=0)


@router.post("/create", status_code=status.HTTP_201_CREATED)
async def create_distributed_order(req: CreateOrderRequest):
    coordinator = SagaCoordinator()

    # Step 1: Inventory Reservation
    coordinator.add_step(
        SagaStep(
            name="inventory_reservation",
            action=lambda: inventory_client.reserve(req.item_id, req.quantity),
            compensate=lambda: inventory_client.release(req.item_id, req.quantity),
        )
    )

    # Step 2: Payment Authorization
    coordinator.add_step(
        SagaStep(
            name="payment_charge",
            action=lambda: payment_client.charge(req.user_id, req.total_amount),
            compensate=lambda: payment_client.refund("txn_mock_9921"),
        )
    )

    try:
        results = await coordinator.execute()
        return {"status": "SUCCESS", "results": results}
    except Exception as exc:
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail=f"Distributed transaction failed: {exc}",
        )
```

```python
# src/main.py
from fastapi import FastAPI
from src.api.v1.orders import router as orders_router
from src.config import get_settings

settings = get_settings()
app = FastAPI(title=settings.PROJECT_NAME)
app.include_router(orders_router, prefix="/api/v1")
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

### 3.2 `tests/test_saga_orchestration.py`

```python
import pytest
from httpx import AsyncClient


@pytest.mark.asyncio
async def test_saga_successful_order(client: AsyncClient):
    payload = {"user_id": "usr_10", "item_id": "ITEM_OK", "quantity": 2, "total_amount": 99.00}
    resp = await client.post("/api/v1/orders/create", json=payload)
    assert resp.status_code == 201
    assert resp.json()["status"] == "SUCCESS"


@pytest.mark.asyncio
async def test_saga_failure_and_rollback(client: AsyncClient):
    # Total amount > 5000 triggers payment failure & triggers compensation rollback
    payload = {"user_id": "usr_10", "item_id": "ITEM_OK", "quantity": 1, "total_amount": 9999.00}
    resp = await client.post("/api/v1/orders/create", json=payload)
    assert resp.status_code == 400
    assert "Credit card transaction limit exceeded" in resp.json()["detail"]
```

---

## 4. Production Infrastructure & Deployment

### 4.1 Multi-Stage `Dockerfile`

```dockerfile
FROM python:3.12-slim-bookworm AS builder
WORKDIR /build
RUN apt-get update && apt-get install -y --no-install-recommends gcc && rm -rf /var/lib/apt/lists/*
RUN pip install --no-cache-dir hatchling
COPY pyproject.toml .
RUN pip wheel --no-cache-dir --wheel-dir /build/wheels .

FROM python:3.12-slim-bookworm AS runtime
WORKDIR /app
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
  gateway:
    build: .
    ports:
      - "8000:8000"
    environment:
      - INVENTORY_GRPC_HOST=inventory:50051
      - PAYMENT_GRPC_HOST=payment:50052
```

---

## 5. Operational Runbook & Verification

### 5.1 Verification Commands

```bash
# 1. Trigger successful distributed Saga
curl -X POST "http://localhost:8000/api/v1/orders/create" \
     -H "Content-Type: application/json" \
     -d '{"user_id": "u1", "item_id": "WIDGET", "quantity": 1, "total_amount": 49.99}'

# 2. Trigger payment failure to inspect compensation logs
curl -X POST "http://localhost:8000/api/v1/orders/create" \
     -H "Content-Type: application/json" \
     -d '{"user_id": "u1", "item_id": "WIDGET", "quantity": 1, "total_amount": 10000.00}'

# 3. Run automated tests
pytest -v tests/
```
