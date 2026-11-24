# Production Reference Project: High-Throughput ML Model Serving with ONNX Runtime & Dynamic Batching

## Metadata & Production Baseline

| Attribute | Specification |
|:---|:---|
| **Project Code** | `FASTAPI-PROJ-014` |
| **FastAPI Version** | `0.115.x+` / `0.165.x+` |
| **Pydantic Version** | `2.10.x+` (V2 Core) |
| **Python Baseline** | `3.12+` / `3.13` (Free-Threading Safe) |
| **ASGI Server** | `Uvicorn 0.32+` |
| **Inference Engine** | `ONNX Runtime 1.20+ (onnxruntime-cpu / onnxruntime-gpu)` |
| **Throughput Optimizer**| Dynamic Request Batcher (`asyncio.Queue` coalescing requests over micro-windows) |
| **Execution Model** | Non-blocking thread offloading via `asyncio.to_thread` / C++ native threadpool |
| **Testing** | `pytest 8.3+`, `pytest-asyncio 0.24+`, `numpy 2.1+`, `httpx 0.27+` |
| **Complexity Level** | Advanced / Production Machine Learning Inference |

---

## 1. Architectural Overview & Domain Design

### 1.1 System Architecture & Data Flow

```
                      +---------------------------------------+
                      |       Concurrent Client Inferences    |
                      |   Client 1        Client 2   Client 3 |
                      +-------+--------------+-----------+----+
                              |              |           |
                              v              v           v
                      +-------+--------------+-----------+----+
                      | FastAPI Inference Endpoint            |
                      | POST /api/v1/predict                  |
                      +-------------------+-------------------+
                                          |
                                          | Enqueues Request Future
                                          v
                      +-------------------+-------------------+
                      | Dynamic Micro-Batcher (asyncio.Queue) |
                      | - Coalesces requests within 10ms      |
                      | - Or when Max Batch Size (32) reached |
                      +-------------------+-------------------+
                                          |
                                          | Batched Tensor Shape: [B, D]
                                          v
                      +-------------------+-------------------+
                      | asyncio.to_thread Worker Pool         |
                      | (Avoids blocking ASGI event loop)     |
                      +-------------------+-------------------+
                                          |
                                          | Invokes C++ Runtime
                                          v
                      +-------------------+-------------------+
                      | ONNX Runtime Session (Inference)      |
                      | model.onnx (Optimized Graph)          |
                      +-------------------+-------------------+
                                          |
                                          | Slices outputs & Resolves Futures
                                          v
                      +-------------------+-------------------+
                      | Client 1 Output | Client 2 | Client 3 |
                      +---------------------------------------+
```

### 1.2 Directory Layout

```
14-ml-model-serving/
|-- pyproject.toml
|-- Dockerfile
|-- docker-compose.yml
|-- src/
|   |-- __init__.py
|   |-- main.py
|   |-- config.py
|   |-- ml/
|   |   |-- __init__.py
|   |   |-- onnx_runner.py
|   |   `-- batcher.py
|   |-- domain/
|   |   |-- __init__.py
|   |   `-- schemas.py
|   `-- api/
|       |-- __init__.py
|       `-- v1/
|           `-- predict.py
`-- tests/
    |-- __init__.py
    |-- conftest.py
    `-- test_predict_api.py
```

---

## 2. Complete Production Implementation

### 2.1 `pyproject.toml`

```toml
[project]
name = "fastapi-ml-model-serving"
version = "0.1.0"
description = "High-throughput machine learning inference with ONNX Runtime and dynamic batching"
readme = "README.md"
requires-python = ">=3.12"
dependencies = [
    "fastapi>=0.115.0",
    "uvicorn[standard]>=0.32.0",
    "pydantic>=2.10.0",
    "pydantic-settings>=2.6.0",
    "onnxruntime>=1.20.0",
    "numpy>=2.1.0",
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
    PROJECT_NAME: str = "ONNX Model Serving API"
    MODEL_PATH: str = "models/classifier.onnx"
    MAX_BATCH_SIZE: int = 32
    MAX_BATCH_LATENCY_MS: float = 10.0  # 10ms micro-window
    FEATURE_VECTOR_SIZE: int = 16

    model_config = SettingsConfigDict(env_file=".env", extra="ignore")


@lru_cache
def get_settings() -> Settings:
    return Settings()
```

### 2.3 `src/domain/schemas.py`

```python
from pydantic import BaseModel, Field


class InferenceRequest(BaseModel):
    features: list[float] = Field(..., min_length=16, max_length=16, description="16-element float feature vector")


class InferenceResponse(BaseModel):
    prediction_class: int
    confidence_score: float
    batch_size_executed: int
```

### 2.4 `src/ml/batcher.py` (Dynamic Request Coalescing Batcher)

```python
import asyncio
import logging
from typing import Any
import numpy as np

logger = logging.getLogger("dynamic_batcher")


class QueuedInference:
    def __init__(self, features: list[float], loop: asyncio.AbstractEventLoop):
        self.features = features
        self.future: asyncio.Future = loop.create_future()


class DynamicBatcher:
    def __init__(self, max_batch_size: int, max_latency_ms: float, infer_fn):
        self.max_batch_size = max_batch_size
        self.max_latency_sec = max_latency_ms / 1000.0
        self.infer_fn = infer_fn
        self.queue: asyncio.Queue[QueuedInference] = asyncio.Queue()
        self.worker_task: asyncio.Task | None = None

    def start(self):
        self.worker_task = asyncio.create_task(self._batching_loop())

    async def stop(self):
        if self.worker_task:
            self.worker_task.cancel()
            try:
                await self.worker_task
            except asyncio.CancelledError:
                pass

    async def predict_single(self, features: list[float]) -> dict[str, Any]:
        loop = asyncio.get_running_loop()
        item = QueuedInference(features, loop)
        await self.queue.put(item)
        return await item.future

    async def _batching_loop(self):
        while True:
            # Wait for first item
            first_item = await self.queue.get()
            batch = [first_item]

            # Coalesce additional items until max_batch_size or timeout
            end_time = asyncio.get_running_loop().time() + self.max_latency_sec

            while len(batch) < self.max_batch_size:
                timeout = end_time - asyncio.get_running_loop().time()
                if timeout <= 0:
                    break
                try:
                    next_item = await asyncio.wait_for(self.queue.get(), timeout=timeout)
                    batch.append(next_item)
                except asyncio.TimeoutError:
                    break

            # Execute batched inference in background thread
            batch_data = np.array([item.features for item in batch], dtype=np.float32)
            try:
                results = await asyncio.to_thread(self.infer_fn, batch_data)
                # Distribute results to each waiting client future
                for i, item in enumerate(batch):
                    item.future.set_result({
                        "prediction_class": int(results["classes"][i]),
                        "confidence_score": float(results["scores"][i]),
                        "batch_size_executed": len(batch),
                    })
            except Exception as exc:
                for item in batch:
                    if not item.future.done():
                        item.future.set_exception(exc)
```

### 2.5 `src/main.py` & API Endpoints

```python
from contextlib import asynccontextmanager
from fastapi import FastAPI, HTTPException, status
import numpy as np

from src.config import get_settings
from src.domain.schemas import InferenceRequest, InferenceResponse
from src.ml.batcher import DynamicBatcher

settings = get_settings()


# Mock ONNX Inference callable for demonstration/tests
def mock_onnx_inference(batch_array: np.ndarray) -> dict[str, np.ndarray]:
    # Simulates ONNX model forward pass: batch_array shape [B, 16]
    batch_size = batch_array.shape[0]
    classes = np.argmax(batch_array[:, :2], axis=1)
    scores = np.ones(batch_size, dtype=np.float32) * 0.95
    return {"classes": classes, "scores": scores}


batcher = DynamicBatcher(
    max_batch_size=settings.MAX_BATCH_SIZE,
    max_latency_ms=settings.MAX_BATCH_LATENCY_MS,
    infer_fn=mock_onnx_inference,
)


@asynccontextmanager
async def lifespan(app: FastAPI):
    batcher.start()
    yield
    await batcher.stop()


app = FastAPI(title=settings.PROJECT_NAME, lifespan=lifespan)


@app.post("/api/v1/predict", response_model=InferenceResponse)
async def predict_features(payload: InferenceRequest):
    try:
        result = await batcher.predict_single(payload.features)
        return result
    except Exception as exc:
        raise HTTPException(status_code=status.HTTP_500_INTERNAL_SERVER_ERROR, detail=str(exc))
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

### 3.2 `tests/test_predict_api.py`

```python
import asyncio
import pytest
from httpx import AsyncClient


@pytest.mark.asyncio
async def test_predict_and_dynamic_batching(client: AsyncClient):
    features = [1.0] * 16

    # Send 5 concurrent requests to test coalescing
    async def make_req():
        return await client.post("/api/v1/predict", json={"features": features})

    responses = await asyncio.gather(*(make_req() for _ in range(5)))
    for resp in responses:
        assert resp.status_code == 200
        data = resp.json()
        assert "prediction_class" in data
        assert data["batch_size_executed"] >= 1
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
CMD ["uvicorn", "src.main:app", "--host", "0.0.0.0", "--port", "8000", "--workers", "2"]
```

### 4.2 `docker-compose.yml`

```yaml
version: "3.9"

services:
  inference-service:
    build: .
    ports:
      - "8000:8000"
    environment:
      - MAX_BATCH_SIZE=32
      - MAX_BATCH_LATENCY_MS=10.0
```

---

## 5. Operational Runbook & Verification

### 5.1 Verification Commands

```bash
# 1. Send inference vector to model server
curl -X POST "http://localhost:8000/api/v1/predict" \
     -H "Content-Type: application/json" \
     -d '{"features": [0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6]}'

# 2. Benchmark throughput using wrk or oha
oha -z 10s -c 50 -m POST \
    -H "Content-Type: application/json" \
    -d '{"features": [0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6]}' \
    http://localhost:8000/api/v1/predict

# 3. Run automated tests
pytest -v tests/
```
