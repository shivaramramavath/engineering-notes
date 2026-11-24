---
title: "Health Checks and Readiness Probes in FastAPI"
description: "Design production-grade health check architectures in FastAPI: Liveness, Readiness, and Startup probes for Kubernetes, deep dependency validation, and avoiding cascading probe outages."
fastapi_version: "0.115.x+"
python_version: "3.12+"
pydantic_version: "v2.10+"
tested_with: "fastapi 0.115+, sqlalchemy 2.0+, redis 5.2+, pytest 8.3+"
free_threading_status: "Fully Compatible"
---

# Health Checks and Readiness Probes in FastAPI

## 1. Conceptual Foundation & Mental Model

### The Three Kubernetes Health Probes
In containerized orchestrators like Kubernetes, container health is monitored using three distinct probes, each serving a unique operational purpose:
1. **Startup Probe (`/health/startup`)**: Determines whether the application process has finished bootstrapping (loading ML model weights, applying Alembic migrations, pre-warming caches). If this probe fails, the orchestrator terminates and restarts the container after `failureThreshold` attempts.
2. **Liveness Probe (`/health/liveness`)**: Evaluates whether the application process is alive and responsive, or whether it has entered an unrecoverable deadlock or corrupted state. **If the liveness probe fails, Kubernetes kills and restarts the container.**
3. **Readiness Probe (`/health/readiness`)**: Evaluates whether the application is currently capable of handling user traffic (verifying database connectivity, Redis pool responsiveness, and queue connections). **If the readiness probe fails, Kubernetes removes the pod from load balancer endpoints WITHOUT restarting it.**

```
+-------------------------------------------------------------------------------+
|                       THE THREE PROBE PARADIGM                                |
|                                                                               |
|  1. Startup Probe:    "Have I finished initializing?"                         |
|     - Check: Configuration parsed, warmups complete                           |
|     - Failure Action: Wait or restart container                               |
|                                                                               |
|  2. Liveness Probe:   "Is the Python event loop deadlocked or frozen?"        |
|     - Check: Ultra-lightweight in-memory check (Shallow Check)                |
|     - Failure Action: RESTART POD (Destructive)                               |
|                                                                               |
|  3. Readiness Probe:  "Can I talk to PostgreSQL and Redis right now?"         |
|     - Check: Ping database connection pool and Redis (Deep Check)             |
|     - Failure Action: DEREGISTER FROM LOAD BALANCER (Non-destructive)         |
+-------------------------------------------------------------------------------+
```

### The Cascading Outage Anti-Pattern (Liveness vs. Readiness)
The most catastrophic mistake in health probe architecture is placing a database connectivity check inside the **Liveness** probe:
1. Suppose PostgreSQL encounters a temporary connection spike or runs out of connections.
2. All FastAPI liveness probes fail because `SELECT 1` times out.
3. Kubernetes assumes all 50 FastAPI pods are deadlocked and restarts them simultaneously.
4. During startup, all 50 pods hammer PostgreSQL with new connection attempts at the exact same millisecond.
5. PostgreSQL crashes completely under the connection stampede. The entire platform enters an infinite reboot loop.

**Rule**: Liveness probes must **never** check external dependencies! Liveness checks purely internal process health.

---

## 2. Architecture & Internal Mechanics

Understanding how health probes interact with Kubernetes service endpoints and the FastAPI application state.

```
       [Kubernetes Ingress / Load Balancer]
                        |
            Routes traffic only to Healthy Pods
                        |
        +---------------+---------------+
        |                               |
        v                               v
 [Pod 1 (Ready: True)]        [Pod 2 (Ready: False)]
 - Handles User Traffic       - Removed from Service Endpoints!
                              - Remains alive; waiting for DB recovery
        |                               |
        v                               v
[Readiness Check: OK]         [Readiness Check: 503 Service Unavailable]
- DB Ping: 2ms                - DB Ping: Connection Timeout (DB saturated)
        |                               |
[Liveness Check: 200 OK]      [Liveness Check: 200 OK]
- In-memory event loop alive  - In-memory event loop alive (DO NOT RESTART!)
```

---

## 3. Comprehensive Syntax & Core Mechanics

### Status Codes and Content Types
- **HTTP 200 OK**: Component is healthy and ready to serve traffic.
- **HTTP 503 Service Unavailable**: Component is degraded, warming up, or external dependencies are unresponsive.
- **Content-Type**: Standard `application/json` with detailed component statuses.

```python
from pydantic import BaseModel
from typing import Dict, Literal

class ComponentStatus(BaseModel):
    status: Literal["healthy", "degraded", "unhealthy"]
    latency_ms: float
    detail: str | None = None

class HealthResponse(BaseModel):
    status: Literal["healthy", "unhealthy"]
    components: Dict[str, ComponentStatus]
```

---

## 4. Practical Implementation: 5 Verified Examples

### Example 1: Shallow Liveness Probe (Zero External Dependencies)
A non-blocking probe verifying that the ASGI event loop is active and responsive.

```python
# app/api/v1/health/liveness.py
from fastapi import APIRouter, Response, status

router = APIRouter(prefix="/health", tags=["Health"])

@router.get("/liveness", status_code=status.HTTP_200_OK)
async def liveness_probe():
    """Shallow probe for Kubernetes: checks only that process is running."""
    return {"status": "alive"}
```

---

### Example 2: Deep Readiness Probe with Timeout Guardrails
Checking PostgreSQL and Redis connectivity with strict 1.0-second timeouts to prevent probe hangs.

```python
# app/api/v1/health/readiness.py
import asyncio
import time
from fastapi import APIRouter, Depends, Response, status
from sqlalchemy import text
from sqlalchemy.ext.asyncio import AsyncSession
import redis.asyncio as aioredis
from app.core.database import get_db_session

router = APIRouter(prefix="/health", tags=["Health"])

async def check_database(session: AsyncSession) -> tuple[bool, float, str]:
    start = time.perf_counter()
    try:
        # Enforce strict 1-second timeout on DB ping
        await asyncio.wait_for(session.execute(text("SELECT 1")), timeout=1.0)
        elapsed = round((time.perf_counter() - start) * 1000, 2)
        return True, elapsed, "Connected"
    except Exception as exc:
        elapsed = round((time.perf_counter() - start) * 1000, 2)
        return False, elapsed, str(exc)

async def check_redis(redis_client: aioredis.Redis) -> tuple[bool, float, str]:
    start = time.perf_counter()
    try:
        await asyncio.wait_for(redis_client.ping(), timeout=1.0)
        elapsed = round((time.perf_counter() - start) * 1000, 2)
        return True, elapsed, "Connected"
    except Exception as exc:
        elapsed = round((time.perf_counter() - start) * 1000, 2)
        return False, elapsed, str(exc)

@router.get("/readiness")
async def readiness_probe(
    response: Response,
    session: AsyncSession = Depends(get_db_session),
):
    # Dummy redis client for demo
    mock_redis = aioredis.from_url("redis://localhost:6379/0")
    
    db_ok, db_ms, db_msg = await check_database(session)
    redis_ok, redis_ms, redis_msg = await check_redis(mock_redis)

    is_ready = db_ok and redis_ok

    if not is_ready:
        # Crucial: return HTTP 503 so Kubernetes deregisters the pod
        response.status_code = status.HTTP_503_SERVICE_UNAVAILABLE

    return {
        "status": "ready" if is_ready else "unhealthy",
        "components": {
            "database": {"ready": db_ok, "latency_ms": db_ms, "detail": db_msg},
            "redis": {"ready": redis_ok, "latency_ms": redis_ms, "detail": redis_msg},
        }
    }
```

---

### Example 3: Startup Probe with Lifespan State Integration
Validating that application initialization (pre-warming ML weights or downloading schemas) is complete before allowing traffic.

```python
# app/core/lifecycle.py
from contextlib import asynccontextmanager
from fastapi import FastAPI

@asynccontextmanager
async def lifespan(app: FastAPI):
    # Initial state: not initialized
    app.state.is_bootstrapped = False

    # Simulate heavy startup work (compiling schemas, loading models)
    await asyncio.sleep(0.5)
    app.state.is_bootstrapped = True

    yield

    app.state.is_bootstrapped = False
```

```python
# app/api/v1/health/startup.py
from fastapi import APIRouter, Request, Response, status

router = APIRouter(prefix="/health", tags=["Health"])

@router.get("/startup")
async def startup_probe(request: Request, response: Response):
    is_ready = getattr(request.app.state, "is_bootstrapped", False)
    if not is_ready:
        response.status_code = status.HTTP_503_SERVICE_UNAVAILABLE
        return {"status": "bootstrapping"}
    return {"status": "started"}
```

---

### Example 4: Graceful Shutdown Draining State Probe
When receiving `SIGTERM`, marking the readiness probe unhealthy immediately while allowing 5 seconds to drain in-flight requests.

```python
# app/core/draining.py
import asyncio
import signal
from fastapi import FastAPI, Response, status

class GracefulDrainState:
    def __init__(self):
        self.is_draining = False

drain_state = GracefulDrainState()

def setup_graceful_drain(app: FastAPI):
    loop = asyncio.get_event_loop()

    def handle_sigterm():
        # Immediately fail readiness checks so load balancers stop sending new traffic
        drain_state.is_draining = True

    # Register OS signal handler
    for sig in (signal.SIGTERM, signal.SIGINT):
        loop.add_signal_handler(sig, handle_sigterm)
```

```python
# In readiness endpoint:
@router.get("/readiness")
async def readiness_with_drain(response: Response):
    if drain_state.is_draining:
        response.status_code = status.HTTP_503_SERVICE_UNAVAILABLE
        return {"status": "draining", "detail": "Pod terminating, traffic diverted"}
    return {"status": "ready"}
```

---

### Example 5: Kubernetes Manifest Configuration
Production Kubernetes YAML specifying exact timing, timeouts, and thresholds for all three probes.

```yaml
# deployment.yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: fastapi-backend
spec:
  replicas: 4
  template:
    spec:
      containers:
        - name: fastapi-app
          image: myregistry/fastapi-app:v1.0.0
          ports:
            - containerPort: 8000

          # 1. Startup Probe: Allows up to 30s for heavy bootstrap
          startupProbe:
            httpGet:
              path: /health/startup
              port: 8000
            failureThreshold: 30
            periodSeconds: 1

          # 2. Liveness Probe: Checks only internal event loop
          livenessProbe:
            httpGet:
              path: /health/liveness
              port: 8000
            initialDelaySeconds: 5
            periodSeconds: 10
            timeoutSeconds: 2
            failureThreshold: 3

          # 3. Readiness Probe: Deep check of DB and Redis
          readinessProbe:
            httpGet:
              path: /health/readiness
              port: 8000
            periodSeconds: 5
            timeoutSeconds: 2
            failureThreshold: 2
```

---

## 5. Edge Cases, Pitfalls & Failure Modes

1. **Liveness Timeout Squeezing**: Setting `timeoutSeconds: 1` on liveness probes when Uvicorn workers are temporarily busy processing a heavy request. If the liveness check queues behind user requests, the probe times out, and Kubernetes kills healthy containers. Expose health checks on a separate, dedicated internal port or prioritize probe handling.
2. **Cascading Failure from Deep Liveness Checks**: Including external APIs (Stripe, Twilio, Sendgrid) in readiness probes. If Stripe is down, your application cannot process payments, but it should still serve user logins, account settings, and catalog reads. Only include **hard dependencies** (primary database) in readiness checks; degrade optional third-party integrations gracefully.
3. **Connection Leaks in Readiness Probes**: Executing `SELECT 1` every 5 seconds without properly closing or returning the session to the SQLAlchemy connection pool leaks connections over time.
4. **Probe Self-Denial of Service**: If 100 pods probe the same PostgreSQL database every 1 second, the probes themselves generate 100 queries/second against the database. Set reasonable `periodSeconds` (5s to 15s).

---

## 6. Common Mistakes Table

| Mistake | Root Cause | Impact | Correct Approach |
| :--- | :--- | :--- | :--- |
| **Checking Database in Liveness Probe** | Confusing Liveness with Readiness | Database blip triggers mass restart of all pods, crashing the cluster. | Liveness must ONLY check internal process health; move DB checks to Readiness. |
| **Omitting Timeouts on Health Checks** | Assuming `SELECT 1` is always fast | Hung database lock freezes probe coroutine indefinitely; probe times out. | Wrap checks in `asyncio.wait_for(..., timeout=1.0)`. |
| **Checking Non-Critical Third-Party APIs** | Over-zealous readiness checks | If Twilio or SendGrid goes down, the entire API goes offline. | Only include essential hard dependencies (DB) in readiness checks. |
| **Logging Every Health Check Probe** | No exclusion filter in access logs | Millions of useless health check logs clutter Elasticsearch and disk. | Exclude `/health/*` paths from access logging. |
| **Returning 200 with `{"status": "error"}`** | Misunderstanding HTTP semantics | Kubernetes checks HTTP status code, not JSON body; treats pod as healthy! | Return HTTP `503 Service Unavailable` on component failure. |
| **No Startup Probe for Heavy Apps** | Relying solely on Liveness initialDelay | Heavy app gets killed by liveness probe before finish bootstrapping. | Use a dedicated `startupProbe` with high `failureThreshold`. |

---

## 7. Performance & Scalability Considerations

- **Probe Latency Target**: Liveness probes must respond in under 1 millisecond. Readiness probes must complete in under 15 milliseconds.
- **Probe Caching**: If readiness checks require checking 4 separate subsystems, cache probe results for 2–3 seconds in memory to avoid redundant queries when multiple probe agents scrape the pod simultaneously.
- **Python 3.13+ Free-Threading**: Health check endpoints executing in free-threaded Python bypass the GIL entirely, responding instantly even if other worker threads are performing CPU-bound calculations.

---

## 8. Security & OWASP Guardrails

- **Information Disclosure in Health Payloads**: Do not expose sensitive internal hostnames, connection strings, or database credentials in the health check failure JSON (e.g., `detail: "password authentication failed for user postgres at 10.0.4.15"`). Return sanitized diagnostic status strings (`"database": "unreachable"`).
- **Network Segmentation**: In high-security environments, bind health probes to an internal loopback or management interface that cannot be accessed through the public ingress.

---

## 9. Testing Strategies & Pytest Recipes

```python
# tests/test_health_probes.py
from fastapi.testclient import TestClient
from app.main import app

client = TestClient(app)

def test_liveness_probe_returns_200():
    response = client.get("/health/liveness")
    assert response.status_code == 200
    assert response.json()["status"] == "alive"

def test_readiness_probe_returns_200_when_healthy():
    response = client.get("/health/readiness")
    assert response.status_code == 200
    data = response.json()
    assert data["status"] == "ready"
    assert "database" in data["components"]
```

---

## 10. Real-World Case Study / Production Incident

A popular e-commerce platform experienced a catastrophic complete cluster outage during their Cyber Monday launch.

**The Failure Chain**:
1. At 9:00 AM, traffic spiked by 10x.
2. PostgreSQL connection pool reached 100% capacity; query queue latency rose to 3 seconds.
3. The platform's Kubernetes deployment had configured:
   ```yaml
   livenessProbe:
     httpGet:
       path: /health
     timeoutSeconds: 1
   ```
4. The `/health` endpoint checked PostgreSQL. Because queries took 3 seconds, the liveness probe timed out after 1 second.
5. Kubernetes killed all 40 API pods simultaneously.
6. When the 40 replacement pods started, they all initiated database connections concurrently during their startup phase, crashing PostgreSQL completely.
7. Total downtime: 3.5 hours. Estimated revenue loss: \$2,400,000.

**The Solution**:
The team split the monolithic `/health` endpoint into two separate endpoints:
- `/health/liveness`: Checked only in-memory process responsiveness (never touched the database).
- `/health/readiness`: Checked PostgreSQL.
When the database became saturated during the subsequent holiday sale, the readiness probe marked pods as not ready, pausing new incoming traffic and allowing PostgreSQL to clear its query queue without killing a single container.

---

## 11. Technical Interview Questions

<details>
<summary>1. What is the crucial architectural difference between a Liveness Probe and a Readiness Probe?</summary>
A Liveness Probe evaluates whether the process is alive or deadlocked. If a liveness probe fails, Kubernetes terminates and restarts the container (a destructive action). A Readiness Probe evaluates whether the container is currently capable of handling incoming traffic. If a readiness probe fails, Kubernetes removes the container from the load balancer service endpoints without restarting it, preserving in-flight operations and giving downstream dependencies time to recover.
</details>

<details>
<summary>2. Why is checking a database connection in a Liveness Probe considered a dangerous anti-pattern?</summary>
If the database suffers a temporary network hiccup or connection pool saturation, checking the database in the liveness probe causes all application containers to fail their liveness checks simultaneously. Kubernetes will kill and restart every pod in the cluster. When the pods reboot, they flood the struggling database with hundreds of simultaneous connection setup requests, causing a catastrophic cascading failure.
</details>

<details>
<summary>3. What is the purpose of a Startup Probe?</summary>
A Startup Probe is used for applications with long initialization times (such as pre-warming machine learning weights, loading large caches, or running schema checks). It disables liveness and readiness checks until the application finishes bootstrapping, preventing Kubernetes from prematurely killing slow-starting containers.
</details>

<details>
<summary>4. What HTTP status code must an endpoint return to signal to Kubernetes that a probe has failed?</summary>
Any HTTP status code outside the range of 200–399 signals a probe failure. In practice, `HTTP 503 Service Unavailable` is the universal standard for failed readiness or startup probes.
</details>

<details>
<summary>5. How should an application handle health checks during a graceful SIGTERM shutdown?</summary>
Upon receiving `SIGTERM`, the application should immediately mark its readiness probe as unhealthy (returning HTTP 503). This prompts the ingress controller to remove the pod from its routing pool. The application then waits 2–5 seconds (to allow in-flight network requests to drain) before terminating its database connection pools and exiting.
</details>

<details>
<summary>6. Should external third-party APIs (like Stripe or SendGrid) be included in readiness probes?</summary>
No. Third-party APIs are external dependencies over which you have no operational control. If Stripe experiences a temporary outage, including it in your readiness probe will bring down your entire application (preventing users from logging in, viewing past orders, or updating profiles). Only include hard internal dependencies (e.g., your primary database) in readiness checks.
</details>

<details>
<summary>7. Why must database queries in readiness probes use strict timeouts?</summary>
If a database query inside a readiness probe (e.g., `SELECT 1`) hangs due to connection exhaustion or row locks, and does not have a timeout, the probe coroutine blocks indefinitely. The orchestrator's probe client times out, but the worker process remains tied up. Wrapping the check in `asyncio.wait_for(..., timeout=1.0)` guarantees that the check fails fast and releases resources.
</details>

<details>
<summary>8. How do you prevent health checks from polluting application access logs?</summary>
By filtering health check paths in logging middleware or Uvicorn access log filters. For example, configure the logging handler to drop any entry where `http_path` starts with `/health/`.
</details>

---

## 12. Practical Exercises & Solutions

<details>
<summary>Exercise 1 (Easy): Minimal Liveness Probe</summary>

**Task**: Write a FastAPI route `/healthz` returning status code 200 and `{"status": "ok"}`.

**Solution**:
```python
from fastapi import FastAPI

app = FastAPI()

@app.get("/healthz", status_code=200)
def healthz():
    return {"status": "ok"}
```
</details>

<details>
<summary>Exercise 2 (Easy): Return 503 on Flag Failure</summary>

**Task**: Write an endpoint `/ready` that inspects a global boolean variable `IS_READY`. If True, return 200; if False, return 503.

**Solution**:
```python
from fastapi import Response, status

IS_READY = True

@app.get("/ready")
def readiness_check(response: Response):
    if not IS_READY:
        response.status_code = status.HTTP_503_SERVICE_UNAVAILABLE
        return {"status": "not_ready"}
    return {"status": "ready"}
```
</details>

<details>
<summary>Exercise 3 (Medium): Async DB Ping with Timeout</summary>

**Task**: Write an async function `ping_db(session: AsyncSession) -> bool` that executes `SELECT 1` with a 1.0-second timeout, returning False if an exception or timeout occurs.

**Solution**:
```python
import asyncio
from sqlalchemy import text
from sqlalchemy.ext.asyncio import AsyncSession

async def ping_db(session: AsyncSession) -> bool:
    try:
        await asyncio.wait_for(session.execute(text("SELECT 1")), timeout=1.0)
        return True
    except Exception:
        return False
```
</details>

<details>
<summary>Exercise 4 (Medium): Comprehensive Component Health Dict</summary>

**Task**: Write an endpoint `/health/readiness` that checks two components (`db` and `cache`). If either is False, set status code to 503 and return the detailed component status dictionary.

**Solution**:
```python
from fastapi import APIRouter, Response, status

router = APIRouter()

@router.get("/health/readiness")
async def readiness(response: Response):
    db_ok = True      # Simulated check
    cache_ok = False  # Simulated check failure

    all_ok = db_ok and cache_ok
    if not all_ok:
        response.status_code = status.HTTP_503_SERVICE_UNAVAILABLE

    return {
        "status": "ready" if all_ok else "unhealthy",
        "components": {
            "database": "healthy" if db_ok else "unhealthy",
            "cache": "healthy" if cache_ok else "unhealthy",
        }
    }
```
</details>

<details>
<summary>Exercise 5 (Hard): Draining State Middleware</summary>

**Task**: Implement a singleton `DrainManager` class with `mark_draining()`. In a readiness route, if `drain_manager.is_draining()` is True, immediately return 503.

**Solution**:
```python
class DrainManager:
    def __init__(self):
        self._draining = False

    def mark_draining(self):
        self._draining = True

    def is_draining(self) -> bool:
        return self._draining

drain_mgr = DrainManager()

@app.get("/health/readiness")
def check_readiness(response: Response):
    if drain_mgr.is_draining():
        response.status_code = 503
        return {"status": "draining", "detail": "Container is shutting down"}
    return {"status": "ready"}
```
</details>

<details>
<summary>Exercise 6 (Hard): In-Memory Cached Readiness Probe</summary>

**Task**: Implement an async readiness probe that caches the result of expensive DB/cache checks for 3 seconds in memory to prevent hammering resources when probed rapidly.

**Solution**:
```python
import time

class CachedReadinessChecker:
    def __init__(self, ttl_seconds: float = 3.0):
        self.ttl = ttl_seconds
        self.last_checked = 0.0
        self.cached_status = False

    async def is_ready(self, check_func) -> bool:
        now = time.time()
        if now - self.last_checked > self.ttl:
            self.cached_status = await check_func()
            self.last_checked = now
        return self.cached_status

# Verification
checker = CachedReadinessChecker(ttl_seconds=3.0)
```
</details>
