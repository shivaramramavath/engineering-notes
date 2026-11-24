---
title: "Graceful Shutdown in FastAPI"
description: "Mastering graceful application lifecycle management, POSIX signal handling (SIGTERM/SIGINT), connection draining, background task completion, and resource cleanup."
fastapi_version: "0.115+"
python_version: "3.12+"
free_threading_compatible: true
tested_with: "Python 3.12.8, FastAPI 0.115.6, Uvicorn 0.32.0"
---

# Graceful Shutdown in FastAPI

## 1. Conceptual Foundation & Mental Model

When a container orchestrator (Kubernetes, AWS ECS) or process manager updates, reschedules, or terminates a FastAPI instance, it initiates a shutdown sequence by transmitting a POSIX **`SIGTERM`** signal. If an application terminates abruptly, active client requests are severed, database transactions are left dangling in uncommitted states, message queue acknowledgments are lost, and users experience abrupt network failures.

**Graceful Shutdown** is the systematic process of safely terminating an application:
1. **Stop Accepting New Ingress**: The server immediately stops accepting new incoming TCP connections. In Kubernetes, the pod IP is removed from Service endpoints.
2. **In-Flight Request Draining**: The ASGI server permits currently executing HTTP requests and WebSocket connections a designated grace period (e.g. 30 seconds) to complete processing and return responses to clients.
3. **Background Worker Draining**: Long-running asynchronous tasks (e.g., Celery consumers, background queue drainers) finish processing current jobs and refrain from polling new work.
4. **Subsystem Teardown**: Database connection pools, Redis clients, message broker connections, and telemetry exporters flush remaining data and close sockets cleanly.

```
[ Orchestrator Sends SIGTERM to Container (PID 1) ]
                        │
                        ▼
            [ Uvicorn Signals Shutdown ]
                        │
       ├── 1. Stops accepting new TCP connections
       ├── 2. Ingress traffic begins draining
       └── 3. In-flight requests execute to completion (Within Grace Period)
                        │
                        ▼
       [ FastAPI Application Lifespan Teardown ]
       (Code following 'yield' in lifespan executes)
                        │
       ├── Flush remaining OpenTelemetry spans to collector
       ├── Complete in-flight background worker queue items
       ├── Gracefully dispose SQLAlchemy connection pools (await engine.dispose())
       └── Close Redis socket pools (await redis.aclose())
                        │
                        ▼
            [ Process Exits with Code 0 ]
```

---

## 2. Architecture & Internal Mechanics

### The Lifespan Context Manager Protocol (ASGI 3.0)

In modern FastAPI, the deprecated `@app.on_event("startup")` and `@app.on_event("shutdown")` decorators are replaced by the **Lifespan Protocol** (`contextlib.asynccontextmanager`). The lifespan protocol guarantees that startup logic executes *before* any request is received, and shutdown logic executes *only after* all in-flight requests have completed or timed out.

```
+---------------------------------------------------------------+
|                    ASGI Lifespan Protocol                     |
|                                                               |
|  @asynccontextmanager                                         |
|  async def lifespan(app: FastAPI):                            |
|      # ----------------------------------------------------   |
|      # STARTUP PHASE: Runs before server accepts requests     |
|      db_engine = create_async_engine(...)                     |
|      redis_client = aioredis.from_url(...)                    |
|      # ----------------------------------------------------   |
|      yield                                                    |
|      # ----------------------------------------------------   |
|      # SHUTDOWN PHASE: Runs after server stops accepting reqs |
|      await redis_client.aclose()                              |
|      await db_engine.dispose()                                |
|      # ----------------------------------------------------   |
+---------------------------------------------------------------+
```

### The Kubernetes SIGTERM to SIGKILL Grace Period

Kubernetes grants pods a `terminationGracePeriodSeconds` (default: 30s).
- At $T = 0s$: Kubernetes sends `SIGTERM` and begins updating endpoints.
- From $T = 0s$ to $T = 30s$: Uvicorn drains in-flight requests and executes lifespan teardown.
- At $T = 30s$: If the container has not exited, the Linux kernel issues a non-catchable `SIGKILL`, terminating the process immediately.

---

## 3. Comprehensive Syntax & Core Mechanics

### Production Lifespan Definition
```python
from contextlib import asynccontextmanager
from fastapi import FastAPI
from sqlalchemy.ext.asyncio import create_async_engine, async_sessionmaker, AsyncEngine
import redis.asyncio as aioredis
import logging

logger = logging.getLogger("lifecycle")

class AppState:
    db_engine: AsyncEngine | None = None
    redis: aioredis.Redis | None = None

state = AppState()

@asynccontextmanager
async def lifespan(app: FastAPI):
    # STARTUP
    logger.info("Application starting up: Initializing database and cache pools...")
    state.db_engine = create_async_engine("postgresql+asyncpg://app:secret@db:5432/app", pool_size=10)
    state.redis = aioredis.from_url("redis://cache:6379/0")
    
    yield
    
    # SHUTDOWN
    logger.info("Application shutting down: Draining connections...")
    if state.redis:
        await state.redis.aclose()
        logger.info("Redis connection pool closed.")
        
    if state.db_engine:
        await state.db_engine.dispose()
        logger.info("Database connection pool disposed.")
        
    logger.info("Shutdown completed successfully.")

app = FastAPI(lifespan=lifespan)
```

---

## 4. Practical Implementation: 5 Production Scenarios

### Scenario 1: Draining In-Flight Background Tasks Safely
```python
import asyncio
from contextlib import asynccontextmanager
from fastapi import FastAPI, BackgroundTasks
import logging

logger = logging.getLogger("worker")
active_tasks: set[asyncio.Task] = set()

async def process_audit_log(event_name: str):
    logger.info(f"Processing audit: {event_name}")
    await asyncio.sleep(3.0) # Simulated work
    logger.info(f"Audit completed: {event_name}")

@asynccontextmanager
async def lifespan(app: FastAPI):
    yield
    # Wait for all background tasks to finish processing before exiting
    if active_tasks:
        logger.info(f"Waiting for {len(active_tasks)} background tasks to drain...")
        await asyncio.gather(*active_tasks, return_exceptions=True)
        logger.info("All background tasks completed.")

app = FastAPI(lifespan=lifespan)

@app.post("/events")
async def create_event(name: str):
    task = asyncio.create_task(process_audit_log(name))
    active_tasks.add(task)
    task.add_done_callback(active_tasks.discard)
    return {"status": "QUEUED"}
```

### Scenario 2: Graceful WebSocket Disconnection Broadcast
```python
from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from contextlib import asynccontextmanager

connected_clients: set[WebSocket] = set()

@asynccontextmanager
async def lifespan(app: FastAPI):
    yield
    # Broadcast shutdown notice and cleanly close all active WebSockets
    print("Notifying connected WebSocket clients of imminent shutdown...")
    for ws in list(connected_clients):
        try:
            await ws.send_json({"type": "SERVER_SHUTDOWN", "message": "Server restarting"})
            await ws.close(code=1001, reason="Server shutting down") # 1001: Going Away
        except Exception:
            pass
    connected_clients.clear()

app = FastAPI(lifespan=lifespan)

@app.websocket("/ws/notifications")
async def ws_notifications(websocket: WebSocket):
    await websocket.accept()
    connected_clients.add(websocket)
    try:
        while True:
            await websocket.receive_text()
    except WebSocketDisconnect:
        connected_clients.discard(websocket)
```

### Scenario 3: Flushing OpenTelemetry Spans on Shutdown
```python
from contextlib import asynccontextmanager
from fastapi import FastAPI
from opentelemetry import trace
from opentelemetry.sdk.trace import TracerProvider
from opentelemetry.sdk.trace.export import BatchSpanProcessor, ConsoleSpanExporter

tracer_provider = TracerProvider()
span_processor = BatchSpanProcessor(ConsoleSpanExporter())
tracer_provider.add_span_processor(span_processor)
trace.set_tracer_provider(tracer_provider)

@asynccontextmanager
async def lifespan(app: FastAPI):
    yield
    print("Flushing telemetry batches to collector...")
    # Force flush ensures in-flight traces are not dropped upon SIGTERM
    tracer_provider.force_flush(timeout_millis=5000)
    tracer_provider.shutdown()
    print("Telemetry flushed.")

app = FastAPI(lifespan=lifespan)
```

### Scenario 4: Custom POSIX Signal Interception in Standalone Uvicorn
```python
import signal
import sys
import uvicorn
from main import app

def handle_sigterm(signum, frame):
    print(f"Received signal {signum}. Initiating graceful teardown...")
    # Custom pre-shutdown metrics or hooks can be executed here
    sys.exit(0)

if __name__ == "__main__":
    signal.signal(signal.SIGTERM, handle_sigterm)
    uvicorn.run(app, host="0.0.0.0", port=8000, timeout_graceful_shutdown=30)
```

### Scenario 5: External Consumer Queue Graceful Drain
```python
import asyncio
from contextlib import asynccontextmanager
from fastapi import FastAPI

is_running = True

async def queue_consumer():
    while is_running:
        # Poll message queue (e.g. SQS or RabbitMQ)
        await asyncio.sleep(1.0)
    print("Consumer loop terminated cleanly.")

@asynccontextmanager
async def lifespan(app: FastAPI):
    global is_running
    consumer_task = asyncio.create_task(queue_consumer())
    yield
    print("Stopping consumer loop...")
    is_running = False
    await consumer_task # Await current batch to finish
    print("Queue consumer successfully drained.")

app = FastAPI(lifespan=lifespan)
```

---

## 5. Edge Cases, Pitfalls & Failure Modes

1. **Deadlock Inside Lifespan Shutdown**:
   If an asynchronous operation in the shutdown phase (e.g., waiting for an external HTTP response or unacknowledged queue message) hangs indefinitely, Uvicorn will not exit. Eventually, Kubernetes will issue `SIGKILL` at $T=30s$, abruptly terminating the process. Always wrap shutdown tasks in strict timeouts (`asyncio.timeout(5.0)`).

2. **Database Engine Disposal While Requests Are Active**:
   If database connection pools are closed before in-flight requests finish processing, any active request attempting to commit a transaction will raise `RuntimeError: Engine is already disposed`. Lifespan guarantees that shutdown code runs *after* request handlers finish, provided handlers do not spawn untracked detached background tasks.

3. **Kubernetes Endpoint Propagation Delay**:
   When Kubernetes sends `SIGTERM`, it simultaneously begins removing the pod from Service endpoints. Because updating iptables across all cluster nodes takes 1-3 seconds, ingress proxies may still forward new requests to the terminating pod. A `preStop` hook with `sleep 5` prevents this race condition.

4. **Uvicorn `timeout_graceful_shutdown` Exceeded**:
   If a client upload or slow database query takes longer than Uvicorn's `timeout_graceful_shutdown` (default: 30s), Uvicorn forcefully severs the socket connection.

---

## 6. Anti-Patterns & Common Mistakes

| Anti-Pattern | Operational Impact | Correct Pattern |
| :--- | :--- | :--- |
| Using deprecated `@app.on_event("shutdown")` | Deprecated syntax; lacks unified lifecycle context | Use `lifespan` context manager protocol |
| Unbounded `asyncio.create_task()` without tracking | Tasks aborted mid-execution during shutdown | Track tasks in a set and `await asyncio.gather()` on exit |
| Disposing database engine without timeout | Unresponsive DB hangs shutdown until SIGKILL | Wrap `engine.dispose()` in `asyncio.timeout(5.0)` |
| Omitting `timeout_graceful_shutdown` in Uvicorn | Immediate socket drop on signals | Set `--timeout-graceful-shutdown 30` |
| Calling `sys.exit()` abruptly inside route handlers | Bypasses all shutdown hooks and severs other requests | Let Uvicorn handle process lifecycle |
| Forgetting to close WebSocket connections with code 1001 | Clients perceive abnormal disconnections and retry | Send 1001 Going Away close frame to all sockets |

---

## 7. Performance & Scalability Considerations

- **Grace Period Alignment**:
  $$\text{Uvicorn Graceful Timeout} < \text{Kubernetes Termination Grace Period}$$
  If Kubernetes `terminationGracePeriodSeconds = 30`, configure Uvicorn's `timeout_graceful_shutdown = 25`. This guarantees Uvicorn finishes clean teardown 5 seconds *before* the kernel issues `SIGKILL`.
- **Fast Shutdown Paths**: Keep teardown operations fast (sub-second). Long shutdown phases slow down CI/CD deployments and delay autoscaling scale-down operations.

---

## 8. Security & OWASP Guardrails

- **Flushing Audit Trails**: Ensure security audit logs and security telemetry events (e.g., failed authentication alerts) are flushed to persistent stores during shutdown to prevent audit log loss.
- **Revoking Ephemeral Tokens**: If the service generates temporary sessions or dynamic encryption keys stored in memory, revoke or invalidate them cleanly during shutdown.

---

## 9. Testing Strategies & Pytest Recipes

```python
import pytest
from httpx import AsyncClient, ASGITransport
from main import app, state

@pytest.mark.asyncio
async def test_lifespan_startup_and_teardown():
    transport = ASGITransport(app=app)
    
    # Using AsyncClient with lifespan context triggers startup and shutdown!
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        # Verify Startup executed
        assert state.db_engine is not None
        assert state.redis is not None
        
        response = await client.get("/health")
        assert response.status_code == 200

    # Verify Shutdown executed after client context exited
    # The database engine should now be disposed
```

---

## 10. Real-World Production Incident

**Incident**: An online banking service experienced customer account balance discrepancies after every deployment. Several high-value fund transfers were debited from sender accounts but never credited to recipient accounts.

**Root Cause**: Fund transfers were executed across two microservices using an asynchronous background task:
```python
@app.post("/transfer")
async def initiate_transfer(tx: TransferRequest):
    # Untracked background task!
    asyncio.create_task(execute_two_phase_transfer(tx))
    return {"status": "ACCEPTED"}
```
During a rolling update, Kubernetes sent `SIGTERM` to the container. Because the background tasks were not tracked or awaited during shutdown, the container terminated while the fund transfer coroutine was halfway through execution, debiting Account A but getting killed before crediting Account B.

**Resolution**: Replaced the detached task with tracked tasks drained inside the `lifespan` handler, and wrapped the transfer in a persistent transactional outbox pattern using Celery. Account discrepancies dropped to zero.

---

## 11. Technical Interview Questions

<details>
<summary>1. What happens when a container receives a POSIX SIGTERM signal?</summary>

When a process receives `SIGTERM`, it is a polite request from the operating system or container orchestrator to terminate. The process has the opportunity to catch this signal, stop accepting new work, finish processing in-flight tasks, flush buffers, close open network sockets, and exit cleanly with code 0. If it fails to exit within a grace period, the kernel terminates it forcefully with `SIGKILL`.
</details>

<details>
<summary>2. Why was the <code>lifespan</code> protocol introduced to replace <code>@app.on_event("startup")</code> and <code>@app.on_event("shutdown")</code>?</summary>

The `@app.on_event` handlers were separate, uncoordinated functions that did not share scope and had limited error-handling capabilities. The `lifespan` context manager protocol (introduced in ASGI 3.0) unifies startup and shutdown into a standard Python context manager. Everything before `yield` runs at startup; everything after `yield` runs at shutdown. This allows resources (like database engines or client sessions) to be naturally managed using standard `try/finally` patterns.
</details>

<details>
<summary>3. Why must the Uvicorn graceful timeout be shorter than the Kubernetes termination grace period?</summary>

If Uvicorn's `timeout_graceful_shutdown` is set to 30 seconds and Kubernetes `terminationGracePeriodSeconds` is also 30 seconds, a slight delay in signal delivery or a slow database cleanup will cause Kubernetes to issue `SIGKILL` before Uvicorn finishes its shutdown logic. Setting Uvicorn's timeout to 25s ensures the application finishes clean teardown before Kubernetes forcefully terminates it.
</details>

<details>
<summary>4. How should an application notify connected WebSocket clients during a graceful shutdown?</summary>

The application should iterate through active WebSocket connections, send an application-level message alerting clients to the impending restart, and cleanly send a WebSocket close frame with status code `1001` (Going Away). This informs clients that the server is restarting, prompting them to reconnect to another healthy instance after a brief backoff.
</details>

<details>
<summary>5. What is the risk of using <code>asyncio.create_task()</code> for critical operations without tracking them in lifespan?</summary>

`asyncio.create_task()` creates a detached coroutine scheduled on the event loop. When Uvicorn shuts down, it stops processing new requests and runs lifespan teardown. If detached tasks are still running when the event loop is closed, they are abruptly canceled and destroyed midway through execution, risking data corruption and partial transactions.
</details>

<details>
<summary>6. How does a Kubernetes <code>preStop</code> hook interact with graceful shutdown?</summary>

The `preStop` hook executes *before* Kubernetes sends `SIGTERM` to the container. Running `preStop: exec: command: ["sleep", "5"]` delays `SIGTERM` by 5 seconds, giving Kubernetes time to remove the pod from Service endpoints and update iptables across all worker nodes. This prevents ingress proxies from routing new requests to a container that is already shutting down.
</details>

<details>
<summary>7. What is WebSocket close code 1001?</summary>

WebSocket close code `1001` indicates "Going Away". It is used by servers or clients when an endpoint is terminating due to a server restart or page navigation. Client libraries understand this code and typically initiate an automatic reconnection attempt to an alternate server.
</details>

<details>
<summary>8. How do you prevent a graceful shutdown hook from hanging indefinitely?</summary>

Wrap all asynchronous shutdown operations in strict timeouts using `asyncio.timeout(seconds)`. If a downstream database or Redis cluster fails to respond to a close signal within the timeout, catch the `TimeoutError`, log a warning, and proceed with process termination rather than blocking until `SIGKILL`.
</details>

---

## 12. Practical Exercises & Solutions

### Exercise 1 (Easy): Basic Lifespan with DB Cleanup
**Task**: Write a minimal FastAPI application using the `lifespan` context manager that initializes a mock database on startup and closes it on shutdown.
<details>
<summary>Solution</summary>

```python
from contextlib import asynccontextmanager
from fastapi import FastAPI

mock_db = {}

@asynccontextmanager
async def lifespan(app: FastAPI):
    print("Connecting to DB...")
    mock_db["connected"] = True
    yield
    print("Closing DB connection...")
    mock_db["connected"] = False

app = FastAPI(lifespan=lifespan)
```
</details>

### Exercise 2 (Easy): Uvicorn Command with Graceful Timeout
**Task**: Write the bash command to launch Uvicorn with a 20-second graceful shutdown timeout.
<details>
<summary>Solution</summary>

```bash
uvicorn main:app --host 0.0.0.0 --port 8000 --timeout-graceful-shutdown 20
```
</details>

### Exercise 3 (Medium): Background Task Tracker
**Task**: Implement a helper function `safe_background_task(coro)` that tracks active tasks in a global set and cleans them up automatically upon task completion.
<details>
<summary>Solution</summary>

```python
import asyncio
from typing import Coroutine

RUNNING_TASKS: set[asyncio.Task] = set()

def track_background_task(coro: Coroutine) -> asyncio.Task:
    task = asyncio.create_task(coro)
    RUNNING_TASKS.add(task)
    task.add_done_callback(RUNNING_TASKS.discard)
    return task
```
</details>

### Exercise 4 (Medium): Timed Database Engine Disposal
**Task**: Write an async shutdown hook that disposes a SQLAlchemy engine with a strict 3-second timeout, logging a warning if it times out.
<details>
<summary>Solution</summary>

```python
import asyncio
import logging
from sqlalchemy.ext.asyncio import AsyncEngine

logger = logging.getLogger("shutdown")

async def safe_engine_dispose(engine: AsyncEngine):
    try:
        async with asyncio.timeout(3.0):
            await engine.dispose()
            logger.info("Engine disposed cleanly.")
    except TimeoutError:
        logger.warning("Engine disposal timed out after 3 seconds, forcing exit.")
```
</details>

### Exercise 5 (Hard): Complete WebSocket Drainer with Close Frames
**Task**: Build a connection manager that stores active WebSocket clients and broadcasts a `1001 Going Away` close frame to all connected sockets during lifespan shutdown.
<details>
<summary>Solution</summary>

```python
import asyncio
from contextlib import asynccontextmanager
from fastapi import FastAPI, WebSocket, WebSocketDisconnect

class WebSocketManager:
    def __init__(self):
        self.active_sockets: set[WebSocket] = set()

    def connect(self, ws: WebSocket):
        self.active_sockets.add(ws)

    def disconnect(self, ws: WebSocket):
        self.active_sockets.discard(ws)

    async def drain_all(self):
        close_tasks = []
        for ws in list(self.active_sockets):
            close_tasks.append(ws.close(code=1001, reason="Server shutting down"))
        if close_tasks:
            await asyncio.gather(*close_tasks, return_exceptions=True)
        self.active_sockets.clear()

ws_manager = WebSocketManager()

@asynccontextmanager
async def lifespan(app: FastAPI):
    yield
    print("Draining all active WebSockets...")
    await ws_manager.drain_all()
    print("All WebSockets closed.")

app = FastAPI(lifespan=lifespan)
```
</details>

### Exercise 6 (Hard): Integration Test Simulating SIGTERM
**Task**: Write an automated test using `asyncio` and `signal` that runs a FastAPI server in a subprocess, sends `SIGTERM`, and asserts it exits cleanly with exit code 0 within 5 seconds.
<details>
<summary>Solution</summary>

```python
import subprocess
import signal
import time
import pytest

def test_process_exits_cleanly_on_sigterm():
    # Start uvicorn server in subprocess
    proc = subprocess.Popen(
        ["uvicorn", "main:app", "--host", "127.0.0.1", "--port", "8999", "--timeout-graceful-shutdown", "5"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE
    )
    time.sleep(1.5) # Wait for server to bind

    # Send SIGTERM to process
    proc.send_signal(signal.SIGTERM)
    
    # Wait for graceful exit
    exit_code = proc.wait(timeout=6.0)
    assert exit_code == 0, f"Expected clean exit code 0, got {exit_code}"
```
</details>
