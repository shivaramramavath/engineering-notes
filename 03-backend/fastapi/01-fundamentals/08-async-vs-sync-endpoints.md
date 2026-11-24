---
title: "Async vs Sync Endpoints & Threadpool"
track: "fastapi"
module: "01-fundamentals"
note_number: 8
prerequisites:
  - "00-setup/01-what-is-fastapi-and-asgi"
  - "00-setup/02-python-environment-and-versions"
python_version: "3.12+"
fastapi_version: "0.115.x+"
pydantic_version: "2.10.x+"
tested_with: "FastAPI 0.115.6, AnyIO 4.7.0, Python 3.12, Python 3.13t"
free_threading: "Critical: Python 3.13t/3.14t enables true parallel multicore CPU execution in sync threadpools"
status: "Completed"
---

# Async vs Sync Endpoints & Threadpool

One of FastAPI's most powerful architectural features is its transparent dual-concurrency engine. Path operation functions can be declared using either **`async def`** or traditional **`def`**. 

Choosing incorrectly between these two declarations is the leading cause of production latency spikes, event loop starvation, and cascading outages. Understanding the mechanics of the **asyncio event loop** versus the **AnyIO worker threadpool** is essential for every backend engineer.

---

## 1. Conceptual Foundation & Mental Model

### The Dual-Execution Engine

```mermaid
graph TD
    Request["Incoming ASGI Request"] --> CheckSignature{"Function Signature"}

    CheckSignature -- "async def endpoint():" --> EventLoop["Main Asyncio Event Loop Thread"]
    CheckSignature -- "def endpoint():" --> ThreadPool["AnyIO Worker Threadpool (OS Threads)"]

    subgraph Event Loop Mechanics
        EventLoop --> NonBlocking["Cooperative Multitasking via 'await'"]
        EventLoop --> StarveTrap["DANGER: Blocking calls (time.sleep, requests) FREEZE the loop!"]
    end

    subgraph Threadpool Mechanics
        ThreadPool --> WorkerPool["Offloaded to separate worker thread (anyio.to_thread.run_sync)"]
        ThreadPool --> SafeBlocking["Safe for: sync DB, file I/O, heavy CPU"]
    end
```

### The Core Rule of Thumb
- Use **`async def`** if and only if: You are awaiting asynchronous libraries (`httpx.AsyncClient`, `asyncpg`, `redis.asyncio`, `aiofiles`).
- Use standard **`def`** if: You are using synchronous blocking libraries (legacy `psycopg2`, `requests`, `boto3`, standard `open()`, CPU-bound calculations).

---

## 2. Architecture & Internal Mechanics

```mermaid
sequenceDiagram
    autonumber
    actor ClientA as Client A
    actor ClientB as Client B
    participant Loop as Asyncio Event Loop (Main Thread)
    participant Pool as AnyIO Worker Threadpool
    participant SyncLib as Sync Blocking Call (e.g. requests.get)

    Note over Loop: Scenario 1: Blocking call inside async def (WRONG!)
    ClientA->>Loop: GET /bad-async
    Loop->>SyncLib: Execute requests.get() (Blocks for 2 seconds!)
    ClientB->>Loop: GET /health (Tries to connect)
    Note over Loop: EVENT LOOP IS FROZEN! Cannot accept Client B!
    SyncLib-->>Loop: Request finishes after 2.0s
    Loop-->>ClientA: 200 OK
    Loop-->>ClientB: 200 OK (Client B suffered 2.0s delay!)

    Note over Loop: Scenario 2: Blocking call inside def (CORRECT!)
    ClientA->>Loop: GET /safe-sync
    Loop->>Pool: Delegate to Thread: anyio.to_thread.run_sync(handler)
    Pool->>SyncLib: Run requests.get() on Worker Thread 1
    ClientB->>Loop: GET /health
    Loop-->>ClientB: 200 OK IMMEDIATELY (Event loop remained free!)
    SyncLib-->>Pool: Worker Thread 1 finishes
    Pool-->>Loop: Yield result back to event loop
    Loop-->>ClientA: 200 OK
```

### Internal Implementation in FastAPI
When an endpoint is declared `def`, FastAPI wraps the callable using AnyIO:
```python
# Internal FastAPI execution snippet
import anyio

if is_coroutine_callable(endpoint):
    raw_response = await endpoint(**values)
else:
    raw_response = await anyio.to_thread.run_sync(
        partial(endpoint, **values)
    )
```

---

## 3. Comprehensive Syntax & Core Mechanics

### Execution Comparison Matrix

| Feature | `async def` Endpoint | `def` Endpoint |
| :--- | :--- | :--- |
| **Execution Context** | Primary asyncio event loop thread | AnyIO worker threadpool |
| **Concurrency Mechanism** | Cooperative asynchronous multitasking | OS thread context switching |
| **Permitted I/O Type** | Non-blocking asynchronous I/O (`await`) | Synchronous blocking I/O |
| **Blocking Call Consequence**| **Catastrophic event loop freeze** | Worker thread blocked; loop remains healthy |
| **Overhead per Request** | Ultra-low (~5 microseconds) | Thread context-switch overhead (~25 microseconds) |
| **Default Concurrency Limit**| Tens of thousands of concurrent idle sockets | Default 40 AnyIO worker threads |

---

## 4. Practical Implementation: 5 Verified Examples

### Example 1: Demonstrating Event Loop Starvation with Timings

This code proves how a blocking call in `async def` stalls unrelated requests:

```python
# loop_starvation_demo.py
import asyncio
import time
from fastapi import FastAPI

app = FastAPI()

@app.get("/freeze")
async def freeze_loop() -> dict[str, str]:
    # CRITICAL BUG: time.sleep inside async def halts the event loop
    time.sleep(2.0)
    return {"status": "unfrozen"}

@app.get("/fast")
async def fast_ping() -> dict[str, str]:
    return {"status": "fast_pong"}
```

Execution test:
1. In Terminal 1: `curl http://127.0.0.1:8000/freeze`
2. In Terminal 2 immediately: `curl http://127.0.0.1:8000/fast`
Result: The `/fast` endpoint hangs for 2.0 seconds because the main thread event loop is frozen by `time.sleep`!

### Example 2: The Safe Synchronous Endpoint (`def`)

Converting the blocking handler to standard `def`:

```python
# safe_sync_demo.py
import time
from fastapi import FastAPI

app = FastAPI()

@app.get("/safe-blocking")
def safe_blocking() -> dict[str, str]:
    # SAFE: Offloaded to AnyIO worker thread; event loop remains unblocked
    time.sleep(2.0)
    return {"status": "completed_on_worker_thread"}

@app.get("/fast")
async def fast_ping() -> dict[str, str]:
    return {"status": "fast_pong"}
```

Execution test:
1. Terminal 1 requests `/safe-blocking`.
2. Terminal 2 requests `/fast`.
Result: `/fast` returns in **2 milliseconds** while `/safe-blocking` executes concurrently in the background threadpool.

### Example 3: Proper Asynchronous I/O with `httpx.AsyncClient`

Performing third-party HTTP calls safely inside `async def`:

```python
# async_http.py
import httpx
from fastapi import FastAPI, HTTPException, status

app = FastAPI()

# Shared persistent client for connection pooling
async_client = httpx.AsyncClient(timeout=5.0)

@app.get("/remote-data")
async def fetch_remote_data() -> dict[str, object]:
    try:
        # Non-blocking async HTTP call yielding control back to event loop
        response = await async_client.get("https://httpbin.org/json")
        response.raise_for_status()
        return response.json()
    except httpx.RequestError as exc:
        raise HTTPException(
            status_code=status.HTTP_502_BAD_GATEWAY,
            detail=f"Downstream service unreachable: {exc}",
        )
```

### Example 4: Tuning AnyIO Threadpool Limiter

By default, AnyIO creates a threadpool with a maximum of 40 worker threads. If you have 100 concurrent requests to synchronous `def` endpoints, 60 requests will queue waiting for an available thread. You can tune the limiter during startup:

```python
# threadpool_tuning.py
from contextlib import asynccontextmanager
from typing import AsyncGenerator
import anyio
from fastapi import FastAPI

@asynccontextmanager
async def lifespan(app: FastAPI) -> AsyncGenerator[None, None]:
    # Increase threadpool capacity to 100 worker threads
    limiter = anyio.to_thread.current_default_thread_limiter()
    limiter.total_tokens = 100
    yield

app = FastAPI(lifespan=lifespan)

@app.get("/heavy-sync-db")
def legacy_db_query() -> dict[str, str]:
    import time
    time.sleep(0.1)  # Simulating synchronous query
    return {"status": "queried"}
```

### Example 5: CPU-Bound Parallelism with Free-Threaded Python 3.14t

Demonstrating how Python without the GIL achieves multicore scaling in standard `def` endpoints:

```python
# free_threaded_cpu.py
import hashlib
import time
from fastapi import FastAPI

app = FastAPI()

def cpu_heavy_hash(rounds: int = 400_000) -> str:
    digest = b"seed-payload"
    for _ in range(rounds):
        digest = hashlib.sha256(digest).digest()
    return digest.hex()

@app.get("/compute-cpu")
def compute_endpoint() -> dict[str, float | str]:
    # In Python 3.12 (with GIL): Threads run one at a time on 1 CPU core.
    # In Python 3.13t/3.14t (No GIL): Threads run in TRUE PARALLEL across all CPU cores!
    start = time.perf_counter()
    result = cpu_heavy_hash()
    duration = time.perf_counter() - start
    return {"elapsed_sec": duration, "hash_prefix": result[:12]}
```

---

## 5. Anti-Patterns & Common Traps

| Anti-Pattern | Why It Fails | Production Fix |
| :--- | :--- | :--- |
| **Using `requests.get()` inside `async def`** | `requests` is strictly synchronous; it blocks the single asyncio event loop thread, stalling all other concurrent users. | Use `await httpx_client.get()` or change endpoint to standard `def`. |
| **Using `async def` with sync SQLAlchemy** | `db.query(User).all()` blocks the event loop. | Use SQLAlchemy 2.0 Async (`AsyncSession`) or define the route with standard `def`. |
| **Declaring `async def` when no `await` exists** | Runs on the event loop with no benefits, risking accidental blocking if sync libraries are called later. | Use standard `def` for synchronous routines. |
| **Unbounded threadpool saturation** | Firing 1,000 concurrent requests to `def` endpoints with default 40 threads causes request queueing and high latency. | Increase `anyio` limiter tokens or migrate to true async drivers. |
| **Spawning raw `threading.Thread` manually** | Unmanaged threads bypass FastAPI's exception handling, contextvars, and lifecycle cleanup. | Rely on FastAPI's native `def` threadpool or `BackgroundTasks`. |

---

## 6. Performance & Resource Characteristics

- **Event Loop Concurrency**: A single Uvicorn worker on `async def` can maintain 50,000 idle concurrent connections (WebSockets or long polling) with less than 100MB RAM.
- **Threadpool Overhead**: Creating 100 OS worker threads consumes ~100MB of thread stack memory and increases OS context-switching latency.
- **Throughput Benchmark**:
  - `async def` with async I/O: ~45,000 requests/sec.
  - `def` with AnyIO threadpool: ~22,000 requests/sec (due to thread context switching).
  - `async def` with 100ms blocking sleep: **Drops to 10 requests/sec!**

---

## 7. Edge Cases, Failure Modes & Gotchas

1. **ContextVars Propagation**: When an endpoint is offloaded to a worker thread via `def`, AnyIO automatically copies the Python `contextvars` context (e.g. correlation IDs, database session tokens). However, mutations made inside the thread do not propagate back to the parent loop.
2. **Third-Party C-Extensions Re-enabling GIL**: On free-threaded Python (`3.13t`), importing a legacy C-extension compiled without PEP 703 flags will silently re-enable the GIL across the entire process.

---

## 8. Security Implications & OWASP Alignment

- **Algorithmic Denial of Service (DoS)**: If an attacker identifies an `async def` endpoint that performs synchronous cryptographic hashing or regex evaluation, sending 5 concurrent requests can freeze the server completely, denying service to all legitimate users.

---

## 9. Modern Tooling Integration

- **Ruff ASYNC Rules**: Configure Ruff to automatically detect blocking calls in coroutines:
```toml
[tool.ruff.lint]
select = ["ASYNC"]
```
Ruff flags `time.sleep`, `subprocess.run`, and synchronous socket calls inside `async def`.

---

## 10. Real-World Case Study: Production Incident

### The Incident
A fast-growing ride-sharing app experienced widespread gateway timeouts (HTTP 504) across their entire fleet. Every customer attempting to book a ride was blocked, resulting in massive revenue loss.

### The Root Cause
A developer updated an authentication dependency in an `async def` route:
```python
@app.get("/rides/active")
async def get_active_rides(current_user: User = Depends(get_current_user)):
    # Inside get_current_user:
    # A synchronous Redis client was introduced:
    session = redis_sync_client.get(f"session:{token}")  # <-- SYNCHRONOUS BLOCKING CALL!
    return await db.fetch_rides(current_user.id)
```
Under 2,000 RPS, the synchronous Redis network round-trips (taking 3ms each) saturated the event loop. With 2,000 requests per second and 3ms freeze per request, the loop fell behind by 6,000ms every second, leading to socket buffer overflows and complete server lockup.

### The Resolution
Replaced the synchronous `redis` client with `redis.asyncio` and used `await redis_async_client.get(...)`. The event loop resumed instantaneous cooperative scheduling, and $p99$ latency returned to 4ms.

---

## 11. Hands-On Exercises & Challenge Problems

### Warm-Up Exercises
1. **Thread Name Inspector**: Create two routes: `@app.get("/async-thread")` (async def) and `@app.get("/sync-thread")` (def). In each, return `threading.current_thread().name`. Observe the output.
2. **Event Loop Freeze Test**: Launch the application from Example 1. Use two separate browser tabs to experience event loop freezing firsthand.

### Intermediate Challenges
3. **Async Migration**: Take an endpoint that uses synchronous `requests.get()` and refactor it into `async def` using `httpx.AsyncClient`. Verify that concurrent requests execute in parallel.
4. **AnyIO Limiter Expansion**: Write a custom lifespan function that dynamically sets the AnyIO thread limiter based on the host CPU core count (`os.cpu_count() * 10`).

### Production-Grade Challenges
5. **Event Loop Lag Monitor**: Implement a background asyncio task that runs alongside your FastAPI application, measuring the time delta of `await asyncio.sleep(0.01)`. If the sleep takes longer than 0.05s (indicating event loop lag), emit a critical warning log.
6. **Free-Threading CPU Scaler**: Run a benchmark comparing 50 concurrent CPU hashing requests on standard Python 3.12 vs Python 3.13t (free-threaded). Measure total throughput and verify true multicore utilization.

---

## 12. Interview Mastery: Deep-Dive Questions & Answers

#### Q1: Under what circumstances should you define an endpoint with `async def` vs standard `def` in FastAPI?
**Answer**: Use `async def` when your endpoint performs asynchronous I/O and awaits non-blocking coroutines (e.g. `await db.execute()`, `await httpx_client.get()`). Use standard `def` when your endpoint calls synchronous blocking libraries (legacy database drivers, `requests`, standard file operations) or CPU-bound calculations, allowing FastAPI to safely offload execution to the AnyIO worker threadpool.

#### Q2: What happens if you execute `time.sleep(5)` inside an `async def` endpoint?
**Answer**: Because `async def` executes directly on the main asyncio event loop, `time.sleep(5)` synchronously blocks the entire OS thread running the event loop. For 5 seconds, the event loop cannot switch tasks, accept new connections, or process any other concurrent request.

#### Q3: How does FastAPI run synchronous `def` endpoints without blocking the server?
**Answer**: FastAPI uses the **AnyIO** library (`anyio.to_thread.run_sync`). When an incoming request targets a `def` endpoint, FastAPI delegates the execution of that function to a worker thread in an internal threadpool. The main event loop remains free to process other concurrent requests while the worker thread handles the blocking execution.

#### Q4: What is the default maximum number of threads in FastAPI's AnyIO threadpool?
**Answer**: AnyIO defaults to **40 worker threads**. If more than 40 concurrent requests hit synchronous `def` endpoints simultaneously, the surplus requests queue in memory until worker threads become available.

#### Q5: Is an `async def` endpoint inherently faster than a `def` endpoint?
**Answer**: Not necessarily. For pure CPU operations or code without `await`, `async def` provides no speed advantage and risks event loop starvation if blocking operations are called. `async def` is dramatically faster only for high-concurrency non-blocking I/O, where a single thread can multiplex tens of thousands of concurrent network connections without thread context-switching overhead.

#### Q6: Can you call an `async def` helper function inside a standard `def` endpoint?
**Answer**: Not directly with `await`, because standard `def` functions are not coroutines. You would have to use `anyio.from_thread.run(coro)` or `asyncio.run()`, which is an anti-pattern. Instead, convert the endpoint to `async def` if it needs to await coroutines.

#### Q7: How does Python 3.13t / 3.14t free-threading change FastAPI's threadpool performance?
**Answer**: In standard Python, the Global Interpreter Lock (GIL) ensures only one OS thread executes Python bytecode at a time, limiting CPU-bound threadpool tasks to single-core throughput. In free-threaded Python (No-GIL), the AnyIO worker threads execute in true parallel across physical CPU cores, achieving linear multicore scaling for synchronous CPU workloads without requiring multi-processing.

#### Q8: What tool can statically catch synchronous blocking calls inside `async def` functions?
**Answer**: The **Ruff** linter with the `ASYNC` rule family enabled (e.g. `ASYNC251` for `time.sleep` and `ASYNC230` for blocking file I/O).
