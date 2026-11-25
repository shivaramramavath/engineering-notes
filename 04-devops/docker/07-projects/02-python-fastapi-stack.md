<!--
================================================================================
Metadata:
  Title: Reference Project 02 - Production Python FastAPI Stack with Celery and Redis
  Track: Docker / DevOps
  Difficulty: Intermediate
  Estimated Time: 60 minutes
  Prerequisites: 03-dockerfile/04-multi-stage-builds.md, 05-compose/02-compose-multi-service.md
  Key Terminology: FastAPI, Uvicorn, Celery, Asynchronous Workers, Virtualenv Mount, Non-Root User, Redis Broker
================================================================================
-->

# Reference Project 02: Production Python FastAPI Stack with Celery and Redis

[<- 01-node-api-with-postgres-and-redis.md](./01-node-api-with-postgres-and-redis.md) | [Back to Projects Overview](./README.md) | [Next Project: 03-spring-boot-with-database.md ->](./03-spring-boot-with-database.md)

---

## 1. Overview & Architecture

Modern high-performance web applications require asynchronous offloading for CPU-bound computations, PDF generation, machine learning inference, and third-party webhook integrations. This reference project implements an enterprise-grade Python stack consisting of:
1. **FastAPI Web Service**: High-speed asynchronous REST API powered by Uvicorn.
2. **Celery Worker**: Distributed task execution worker sharing the application codebase.
3. **Redis Broker & Result Store**: Low-latency message broker queuing jobs and persisting task outcomes.

### Architectural Diagram

```
+-----------------------------------------------------------------------------------+
|                                  HOST MACHINE                                     |
|                                                                                   |
|    HTTP Client / Swagger UI (:8000)                                               |
|           |                                                                       |
|           v                                                                       |
|  +-----------------------------------------------------------------------------+  |
|  |                          DOCKER NETWORK: app-net                            |  |
|  |                                                                             |  |
|  |   +---------------------------------------+                                 |  |
|  |   |           SERVICE: fastapi-api        |                                 |  |
|  |   |  FastAPI + Uvicorn (Port 8000)        |                                 |  |
|  |   |  User: appuser (UID 10001)            |                                 |  |
|  |   |  Healthcheck: /healthz                |                                 |  |
|  |   +-------------------+-------------------+                                 |  |
|  |                       | Enqueues Task: process_heavy_job                    |  |
|  |                       v                                                     |  |
|  |   +---------------------------------------------------------------------+   |  |
|  |   |                       SERVICE: redis-broker                         |   |  |
|  |   |  Image: redis:7.4-alpine (Port 6379, Internal Only)                 |   |  |
|  |   |  Acts as Celery Message Broker & Result Backend                     |   |  |
|  |   +-------------------+-------------------------------------------------+   |  |
|  |                       ^                                                     |  |
|  |                       | Consumes Task Messages & Writes Return Value        |  |
|  |   +-------------------+-------------------+                                 |  |
|  |   |        SERVICE: celery-worker         |                                 |  |
|  |   |  Celery Worker Process (Concurrency 2)|                                 |  |
|  |   |  User: appuser (UID 10001)            |                                 |  |
|  |   |  Healthcheck: celery inspect ping     |                                 |  |
|  |   +---------------------------------------+                                 |  |
|  +-----------------------------------------------------------------------------+  |
+-----------------------------------------------------------------------------------+
```

---

## 2. Directory Structure & File Listing

```
fastapi-celery-stack/
├── .dockerignore
├── .env.example
├── compose.yaml
├── Dockerfile
├── requirements.txt
└── app/
    ├── __init__.py
    ├── celery_app.py
    ├── main.py
    └── tasks.py
```

---

## 3. Complete Implementation

### File: `requirements.txt`

```
fastapi==0.111.0
uvicorn[standard]==0.29.0
celery==5.4.0
redis==5.0.4
pydantic==2.7.1
httpx==0.27.0
```

### File: `app/celery_app.py`

```python
import os
from celery import Celery

redis_url = os.getenv("CELERY_BROKER_URL", "redis://redis-broker:6379/0")

celery_app = Celery(
    "worker",
    broker=redis_url,
    backend=redis_url,
    include=["app.tasks"]
)

celery_app.conf.update(
    task_serializer="json",
    accept_content=["json"],
    result_serializer="json",
    timezone="UTC",
    enable_utc=True,
    task_track_started=True,
    task_time_limit=300,
    worker_prefetch_multiplier=1,
)
```

### File: `app/tasks.py`

```python
import time
from app.celery_app import celery_app

@celery_app.task(bind=True, name="process_heavy_job")
def process_heavy_job(self, task_name: str, duration_sec: int):
    """Simulates a heavy computational job or report generation."""
    self.update_state(state="PROGRESS", meta={"percent": 10})
    time.sleep(duration_sec // 2)
    self.update_state(state="PROGRESS", meta={"percent": 50})
    time.sleep(duration_sec // 2)

    return {
        "status": "COMPLETED",
        "task_name": task_name,
        "processed_duration": duration_sec,
        "result": f"Report generated successfully for {task_name}"
    }
```

### File: `app/main.py`

```python
import os
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel
from celery.result import AsyncResult
from app.tasks import process_heavy_job
from app.celery_app import celery_app

app = FastAPI(title="Production FastAPI + Celery Stack", version="1.0.0")

class JobRequest(BaseModel):
    task_name: str
    duration_sec: int = 5

@app.get("/healthz")
def health_check():
    try:
        # Ping Celery broker
        insp = celery_app.control.ping(timeout=1.0)
        return {"status": "UP", "broker": "connected", "workers_online": len(insp)}
    except Exception as e:
        raise HTTPException(status_code=503, detail=str(e))

@app.post("/api/jobs", status_code=202)
def submit_job(req: JobRequest):
    task = process_heavy_job.delay(req.task_name, req.duration_sec)
    return {"task_id": task.id, "status": "QUEUED"}

@app.get("/api/jobs/{task_id}")
def get_job_status(task_id: str):
    task_result = AsyncResult(task_id, app=celery_app)
    response = {
        "task_id": task_id,
        "status": task_result.status,
    }
    if task_result.state == "PROGRESS":
        response["meta"] = task_result.info
    elif task_result.successful():
        response["result"] = task_result.result
    elif task_result.failed():
        response["error"] = str(task_result.info)
    return response
```

### File: `Dockerfile`

```dockerfile
# syntax=docker/dockerfile:1.7

# -------------------------------------------------------------
# Stage 1: Build Dependencies in Virtual Environment
# -------------------------------------------------------------
FROM python:3.12-slim-bookworm AS builder

ENV PYTHONUNBUFFERED=1 \
    PYTHONDONTWRITEBYTECODE=1 \
    PIP_NO_CACHE_DIR=off \
    PIP_DISABLE_PIP_VERSION_CHECK=on

WORKDIR /build

# Install compilation tools if C-extensions are required
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Create virtualenv to encapsulate site-packages cleanly
RUN python -m venv /opt/venv
ENV PATH="/opt/venv/bin:$PATH"

COPY requirements.txt .

# Install dependencies using BuildKit pip cache mount
RUN --mount=type=cache,target=/root/.cache/pip \
    pip install -r requirements.txt

# -------------------------------------------------------------
# Stage 2: Hardened Production Runtime
# -------------------------------------------------------------
FROM python:3.12-slim-bookworm AS runtime

ENV PYTHONUNBUFFERED=1 \
    PYTHONDONTWRITEBYTECODE=1 \
    PATH="/opt/venv/bin:$PATH"

WORKDIR /app

# Create unprivileged system user
RUN groupadd -g 10001 appgroup && \
    useradd -u 10001 -g appgroup -s /sbin/nologin -M appuser

# Install curl for reliable HTTP healthcheck
RUN apt-get update && apt-get install -y --no-install-recommends \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Copy virtualenv and application code
COPY --from=builder /opt/venv /opt/venv
COPY --chown=appuser:appgroup app/ /app/app/

USER 10001:10001

EXPOSE 8000

# Default command runs FastAPI via Uvicorn
CMD ["uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "8000", "--workers", "2"]
```

### File: `.dockerignore`

```
__pycache__
*.pyc
*.pyo
*.pyd
.Python
env/
venv/
.venv/
.git
.gitignore
.env*
Dockerfile
compose*.yaml
README.md
```

### File: `.env.example`

```bash
CELERY_BROKER_URL=redis://redis-broker:6379/0
CELERY_RESULT_BACKEND=redis://redis-broker:6379/0
API_PORT=8000
```

### File: `compose.yaml`

```yaml
version: "3.8"

services:
  fastapi-api:
    build:
      context: .
      dockerfile: Dockerfile
    image: company/fastapi-app:1.0.0
    restart: unless-stopped
    ports:
      - "8000:8000"
    environment:
      - CELERY_BROKER_URL=redis://redis-broker:6379/0
      - CELERY_RESULT_BACKEND=redis://redis-broker:6379/0
    depends_on:
      redis-broker:
        condition: service_healthy
    healthcheck:
      test: ["CMD", "curl", "-f", "http://127.0.0.1:8000/healthz"]
      interval: 10s
      timeout: 3s
      retries: 3
      start_period: 5s
    networks:
      - app-net
    deploy:
      resources:
        limits:
          cpus: "1.0"
          memory: 512M

  celery-worker:
    build:
      context: .
      dockerfile: Dockerfile
    image: company/fastapi-app:1.0.0
    restart: unless-stopped
    command: ["celery", "-A", "app.celery_app", "worker", "--loglevel=info", "--concurrency=2"]
    environment:
      - CELERY_BROKER_URL=redis://redis-broker:6379/0
      - CELERY_RESULT_BACKEND=redis://redis-broker:6379/0
    depends_on:
      redis-broker:
        condition: service_healthy
    networks:
      - app-net
    deploy:
      resources:
        limits:
          cpus: "1.0"
          memory: 512M

  redis-broker:
    image: redis:7.4-alpine
    restart: unless-stopped
    command: ["redis-server", "--save", "60", "1", "--loglevel", "warning"]
    volumes:
      - redis_broker_data:/data
    healthcheck:
      test: ["CMD", "redis-cli", "ping"]
      interval: 5s
      timeout: 3s
      retries: 5
    networks:
      - app-net
    deploy:
      resources:
        limits:
          cpus: "0.5"
          memory: 256M

networks:
  app-net:
    driver: bridge

volumes:
  redis_broker_data:
```

---

## 4. Step-by-Step Verification & Testing

### Step 1: Build and Launch Stack

```bash
mkdir -p fastapi-celery-stack && cd fastapi-celery-stack
cp .env.example .env
docker compose up -d --build
docker compose ps
```

### Step 2: Test Healthcheck

```bash
curl -s http://localhost:8000/healthz
# Output: {"status":"UP","broker":"connected","workers_online":1}
```

### Step 3: Enqueue Asynchronous Task and Poll for Completion

```bash
# 1. Dispatch asynchronous job (duration 4 seconds)
RESPONSE=$(curl -s -X POST http://localhost:8000/api/jobs \
  -H "Content-Type: application/json" \
  -d '{"task_name": "quarterly-report-2026", "duration_sec": 4}')
echo "$RESPONSE"
# Sample Output: {"task_id":"b3f88574-8d4e-4177-bc6d-74d39bc53e77","status":"QUEUED"}

TASK_ID=$(echo $RESPONSE | grep -o '"task_id":"[^"]*' | cut -d'"' -f4)

# 2. Check intermediate progress state
curl -s http://localhost:8000/api/jobs/$TASK_ID
# Output shows: {"task_id":"...","status":"PROGRESS","meta":{"percent":10}}

# 3. Wait 5 seconds and check completed result
sleep 5
curl -s http://localhost:8000/api/jobs/$TASK_ID
# Output shows:
# {
#   "task_id":"...",
#   "status":"SUCCESS",
#   "result":{
#     "status":"COMPLETED",
#     "task_name":"quarterly-report-2026",
#     "processed_duration":4,
#     "result":"Report generated successfully for quarterly-report-2026"
#   }
# }
```

### Step 4: Verify Celery Logs

```bash
docker compose logs celery-worker --tail=15
```
*Logs confirm task receipt, state updates, and successful task execution.*

### Step 5: Teardown

```bash
docker compose down -v
```

---

## 5. Production Considerations

1. **Virtualenv Encapsulation**: Storing dependencies inside `/opt/venv` cleanly separates application packages from operating system Python libraries, avoiding distribution packaging collisions.
2. **Worker Concurrency Tuning**: In I/O-bound workers, Celery concurrency can use `gevent` or eventlet pools (`--pool=gevent --concurrency=100`). For CPU-bound tasks, match concurrency to assigned container CPU limits (`--concurrency=2` on a 2-core container).
3. **Graceful Worker Shutdown**: When Docker stops `celery-worker`, Celery catches `SIGTERM` and waits for inflight tasks to complete. Set `stop_grace_period: 60s` in Compose to avoid terminating jobs prematurely.

---

[<- 01-node-api-with-postgres-and-redis.md](./01-node-api-with-postgres-and-redis.md) | [Back to Projects Overview](./README.md) | [Next Project: 03-spring-boot-with-database.md ->](./03-spring-boot-with-database.md)
