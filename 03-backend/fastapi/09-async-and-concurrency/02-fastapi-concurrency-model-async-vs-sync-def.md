---
title: "FastAPI Concurrency Model: async def vs sync def"
category: "09-async-and-concurrency"
order: 2
tags: ["fastapi", "concurrency", "async-def", "sync-def", "threadpool", "anyio"]
py_version: "3.12+"
fastapi_version: "0.115+"
starlette_version: "0.38+"
pydantic_version: "2.8+"
tested_with: "Python 3.12, 3.13"
---

# FastAPI Concurrency Model: async def vs sync def

## 1. Conceptual Foundation & Mental Model

One of the most defining and misunderstood features of FastAPI is how it routes and executes endpoint functions declared as `async def` versus standard synchronous `def`.

Developers coming from Flask, Django, or traditional WSGI frameworks often assume that converting every route to `async def` automatically makes their application faster. Conversely, developers familiar with raw asyncio frameworks (like aiohttp or Sanic) might assume standard `def` is unsupported or blocks the server.

Both assumptions are incorrect:
- **`async def` Endpoints**: FastAPI runs these functions directly on the **Main Event Loop Thread**. If your code performs non-blocking operations (`await db.execute()`, `await httpx_client.get()`), this provides optimal throughput with zero thread context-switch overhead. However, if your code calls a blocking function (`time.sleep()`, synchronous `requests.get()`, or legacy ORM queries) inside an `async def`, you freeze the entire event loop, choking throughput for all clients.
- **Synchronous `def` Endpoints**: FastAPI does **NOT** run standard `def` routes on the main event loop. Instead, FastAPI detects that the function is a synchronous callable and automatically dispatches it to an external worker **Threadpool** managed by `anyio` (`anyio.to_thread.run_sync`). This protects the event loop from stalling during synchronous file I/O, CPU work, or legacy database calls.

### The Mental Model: Express Checkout vs Personal Shopper Desk
- An **`async def` route** is like an **Express Scanner checkout counter**. The cashier (event loop) processes 50 shoppers simultaneously because shoppers scan their own items via barcodes (cooperative non-blocking I/O). If a shopper stops to fill out a 10-page paper survey at the register (blocking I/O), the entire line stops dead.
- A **`def` route** is like a **Personal Shopper Desk**. When a customer arrives with complex, manual paperwork (blocking I/O), the cashier immediately delegates the customer to one of 40 available assistant shoppers in the back room (the AnyIO Threadpool). The main checkout line stays completely clear.

---

## 2. Architecture & Internal Mechanics

```
                       Incoming ASGI Request
                                 |
                                 v
                     FastAPI Routing Dispatcher
                                 |
                +----------------+----------------+
                |                                 |
        Is `async def`?                     Is sync `def`?
                |                                 |
               YES                               YES
                |                                 |
                v                                 v
    +-----------------------+         +-----------------------+
    |   Main Event Loop     |         |   AnyIO Threadpool    |
    |   (Single OS Thread)  |         |   (Worker Threads)    |
    |                       |         |                       |
    | Directly awaited:     |         | Dispatched via:       |
    | `await endpoint()`    |         | `run_in_threadpool()` |
    +-----------------------+         +-----------------------+
                |                                 |
    +-----------+-----------+                     |
    | Non-blocking: Instant |                     |
    | Blocking: Freezes All |                     |
    +-----------------------+                     |
                |                                 |
                +----------------+----------------+
                                 |
                                 v
                        HTTP Response Sent
```

### Execution Pathways Inside Starlette / FastAPI
FastAPI relies on Starlette's `run_in_threadpool` utility:
```python
# Simplified Starlette implementation:
import anyio
import inspect

async def run_endpoint(endpoint, *args, **kwargs):
    if inspect.iscoroutinefunction(endpoint):
        # async def: executed directly on the running loop
        return await endpoint(*args, **kwargs)
    else:
        # sync def: dispatched to the anyio worker threadpool
        return await anyio.to_thread.run_sync(endpoint, *args, **kwargs)
```

---

## 3. Comprehensive Syntax & Core Mechanics

### When to Choose Which Signature

```python
from fastapi import FastAPI
import asyncio
import time

app = FastAPI()

# 1. USE ASYNC DEF: When using pure async libraries (httpx, asyncpg, redis.asyncio)
@app.get("/async-correct")
async def async_correct():
    await asyncio.sleep(0.1)  # Non-blocking, cooperates with loop
    return {"mode": "async", "status": "optimal"}

# 2. USE SYNC DEF: When using synchronous libraries (requests, psycopg2, PIL, boto3)
@app.get("/sync-correct")
def sync_correct():
    time.sleep(0.1)  # Blocking, but safely executed in a worker thread!
    return {"mode": "sync", "status": "safe_in_threadpool"}

# 3. DANGEROUS DISASTER: Blocking code inside async def
@app.get("/async-catastrophe")
async def async_catastrophe():
    time.sleep(1.0)  # FREEZES THE ENTIRE SERVER FOR 1 FULL SECOND
    return {"status": "all other clients experienced severe latency"}
```

### Threadpool Sizing with AnyIO
By default, `anyio` allocates a threadpool with a default cap (typically 40 threads or `min(32, os.cpu_count() + 4)`). If 50 simultaneous requests hit a slow `def` endpoint, 40 execute concurrently and 10 wait in the threadpool queue, while the event loop continues serving other requests effortlessly.

```python
import anyio

# Adjusting threadpool capacity programmatically
def configure_threadpool(max_threads: int = 100) -> None:
    limiter = anyio.to_thread.current_default_thread_limiter()
    limiter.total_tokens = max_threads
```

---

## 4. Practical Implementation

### Example 1: Basic Comparison — Async vs Sync Endpoints
```python
import asyncio
import time
from fastapi import FastAPI

app = FastAPI()

@app.get("/benchmark/async")
async def benchmark_async() -> dict[str, str]:
    await asyncio.sleep(0.02)
    return {"type": "async", "worker": "event-loop"}

@app.get("/benchmark/sync")
def benchmark_sync() -> dict[str, str]:
    time.sleep(0.02)
    return {"type": "sync", "worker": "threadpool"}
```

### Example 2: Production Integration — Sync ORM vs Async HTTP Client
```python
import httpx
from fastapi import FastAPI, Depends
from sqlalchemy import create_engine, text
from sqlalchemy.orm import Session, sessionmaker

# Legacy synchronous database setup
sync_engine = create_engine("sqlite:///./sync_test.db")
SyncSessionLocal = sessionmaker(bind=sync_engine)

def get_sync_db():
    db = SyncSessionLocal()
    try:
        yield db
    finally:
        db.close()

app = FastAPI()

# 1. Sync endpoint handles synchronous DB sessions safely in threadpool
@app.get("/users/legacy")
def list_users_legacy(db: Session = Depends(get_sync_db)) -> list[dict[str, str]]:
    # Safe: executed in anyio threadpool, does not freeze the loop
    result = db.execute(text("SELECT 'user1' as username UNION SELECT 'user2'"))
    return [{"username": row[0]} for row in result]

# 2. Async endpoint uses async HTTP client on main loop
@app.get("/weather/current")
async def get_weather(city: str) -> dict[str, str]:
    async with httpx.AsyncClient() as client:
        # Safe: async I/O yields execution
        resp = await client.get(f"https://wttr.in/{city}?format=j1", timeout=3.0)
        return {"city": city, "data": resp.json()}
```

### Example 3: Edge Case — Calling Sync Functions inside Async Endpoints
```python
import anyio
from fastapi import FastAPI
import hashlib

app = FastAPI()

def heavy_password_hash(secret: str) -> str:
    # Synchronous CPU-bound operation
    current = secret.encode()
    for _ in range(100_000):
        current = hashlib.sha256(current).digest()
    return current.hex()

@app.post("/hash-token")
async def hash_token_endpoint(token: str) -> dict[str, str]:
    # CORRECT: If you are already inside an `async def`, you CANNOT just call heavy_password_hash(token)
    # You must explicitly wrap it with anyio.to_thread.run_sync!
    hashed = await anyio.to_thread.run_sync(heavy_password_hash, token)
    return {"token_hash": hashed}
```

### Example 4: Failure Case — Accidental Loop Starvation
```python
import requests  # SYNCHRONOUS HTTP CLIENT
from fastapi import FastAPI

app = FastAPI()

# CATASTROPHIC ANTI-PATTERN
@app.get("/fetch-github-bad")
async def fetch_github_bad():
    # requests.get is blocking! In async def, this freezes the loop!
    # Under 50 req/sec load, the application will completely stop responding.
    response = requests.get("https://api.github.com/events", timeout=5.0)
    return response.json()
```

### Example 5: Idiomatic Modern — Dependency Resolution Dynamics
FastAPI dependencies also adhere to the same execution model:
```python
import time
import asyncio
from fastapi import FastAPI, Depends

app = FastAPI()

# Sync dependency: executed in threadpool
def get_auth_token_sync() -> str:
    time.sleep(0.01)  # Safe in threadpool
    return "token-xyz"

# Async dependency: executed on event loop
async def get_user_profile_async(token: str = Depends(get_auth_token_sync)) -> dict[str, str]:
    # FastAPI resolves get_auth_token_sync in threadpool first,
    # then invokes this async dependency on the event loop!
    await asyncio.sleep(0.01)
    return {"user": "alice", "token": token}

@app.get("/me")
async def get_me(profile: dict[str, str] = Depends(get_user_profile_async)):
    return profile
```

---

## 5. Edge Cases, Pitfalls & Failure Modes

1. **Threadpool Saturation (Worker Starvation)**: If you have dozens of synchronous `def` routes that perform slow I/O (e.g., 5-second queries) and your threadpool limit is 40, the 41st request queues up. If health checks (`/healthz`) are declared as `def`, they also get queued behind the slow requests, causing Kubernetes liveness probes to fail and restart the pod. **Remedy**: Always write health check endpoints as `async def return {"status": "ok"}`.
2. **ContextVars Leakage Across Threads**: Synchronous `def` functions run on worker threads. `contextvars.ContextVar` values set in middleware are propagated by AnyIO to threadpool workers, but mutations made inside worker threads do not propagate back to the parent async context.
3. **Database Transaction Binding**: Thread-local database sessions (such as `scoped_session` from SQLAlchemy) can misbehave if an async route switches threads or dispatches tasks concurrently. Use modern `AsyncSession` with `async def`.
4. **Calling `asyncio.run()` Inside Any Route**: Calling `asyncio.run()` inside an `async def` route raises `RuntimeError: This event loop is already running`. Calling it inside a `def` route starts a brand new, isolated loop on the worker thread, causing connection pools and global event-loop state to desynchronize.

---

## 6. Common Mistakes & Anti-Patterns

| Anti-Pattern | Root Cause | Impact | Correct Practice |
|:---|:---|:---|:---|
| Declaring all routes `async def` by default | Believing `async` is a magic speed booster | Disasters occur when blocking code is called inside `async def` | Use `def` for sync code; use `async def` only with true `await` |
| Writing `/healthz` as `def` | Ignorance of threadpool mechanics | Health check hangs when threadpool is saturated, triggering pod restarts | Always declare health endpoints as `async def` |
| Calling `requests` or `urllib` in `async def` | Using sync network libraries in async context | Event loop freezes; complete server lockup | Use `httpx.AsyncClient` or `aiohttp` |
| Calling `time.sleep()` in `async def` | Confusing `time.sleep` with `asyncio.sleep` | Total loop freeze for the sleep duration | Use `await asyncio.sleep()` |
| Running CPU-heavy loops directly in `async def` | Forgetting CPU operations don't yield | Starves concurrent async requests | Offload to threadpool via `anyio.to_thread.run_sync` or process pool |
| Over-increasing threadpool to 5,000 threads | Trying to mask blocking I/O bottleneck | Memory exhaustion and massive OS context-switch overhead | Transition blocking I/O to true async drivers |

---

## 7. Performance & Scalability Considerations

- **Memory Overhead**: An async task consumes ~1-2 KB of memory. An OS thread consumes 4 MB to 8 MB of stack space. Handling 10,000 concurrent idle connections (e.g. WebSockets or SSE) with `async def` requires ~20 MB of RAM. Attempting the same with threads would require 40 GB+ of RAM.
- **Latency Predictability**: `async def` routes have negligible scheduling jitter (sub-millisecond). Threadpool dispatched `def` routes incur thread synchronization and context-switch latency (typically 0.1ms to 0.5ms per invocation).
- **Free-Threading in Python 3.13t**: In GIL-free Python, threadpools achieve true CPU parallelism across cores, making `def` endpoints significantly more performant for mixed CPU/IO tasks.

---

## 8. Security & OWASP Guardrails

- **Threadpool Exhaustion Attack (Denial of Service)**: An unauthenticated attacker targeting a slow `def` endpoint (e.g., `/search` with full table scan) can saturate the AnyIO thread pool with 40 requests, rendering all other `def` endpoints completely unreachable.
- **Remedy**: Implement aggressive rate limiting (`slowapi` or Redis token buckets) before requests reach the threadpool.

---

## 9. Testing Strategies & Pytest Recipes

```python
import pytest
import time
import asyncio
from httpx import AsyncClient, ASGITransport
from fastapi import FastAPI

app = FastAPI()

@app.get("/fast-async")
async def fast_async():
    return {"mode": "async"}

@app.get("/fast-sync")
def fast_sync():
    return {"mode": "sync"}

@pytest.mark.anyio
async def test_both_route_types_respond_accurately():
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        res_async = await client.get("/fast-async")
        res_sync = await client.get("/fast-sync")
        
        assert res_async.status_code == 200
        assert res_async.json() == {"mode": "async"}
        assert res_sync.status_code == 200
        assert res_sync.json() == {"mode": "sync"}

@pytest.mark.anyio
async def test_sync_def_runs_concurrently_in_threadpool():
    # If 5 requests taking 0.05s each run sequentially, it takes 0.25s.
    # In threadpool, they execute concurrently in ~0.06s.
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        start = asyncio.get_running_loop().time()
        tasks = [client.get("/fast-sync") for _ in range(5)]
        responses = await asyncio.gather(*tasks)
        duration = asyncio.get_running_loop().time() - start
        
        assert all(r.status_code == 200 for r in responses)
        assert duration < 0.15
```

---

## 10. Real-World Case Study / Production Incident

**The Incident**: An e-commerce platform experienced total downtime during Black Friday. Kubernetes reported that all pods were being killed and restarted in an endless crash loop due to failing `/healthz` liveness probes.

**Root Cause**: The application had declared all routes, including `/healthz`, as synchronous `def` functions. A third-party inventory API slowed down, causing the `/inventory/check` endpoint to take 4 seconds per request. Because traffic was high, all 40 worker threads in AnyIO's thread pool became stuck waiting on the inventory API. When Kubernetes sent a `GET /healthz` probe, it was placed in the threadpool queue behind 300 inventory requests. The 5-second Kubernetes probe timeout expired, causing Kubernetes to terminate the pod.

**The Fix**:
1. Converted `/healthz` to `async def healthz(): return {"status": "ok"}`. Because it was now `async def`, it was served immediately on the main event loop thread without waiting for the saturated threadpool.
2. Replaced the synchronous HTTP client in `/inventory/check` with `httpx.AsyncClient` inside an `async def` route with a strict 1-second timeout.

---

## 11. Technical Interview Questions & Answers

<details>
<summary>1. How does FastAPI execute a route declared with <code>def</code> vs one declared with <code>async def</code>?</summary>
FastAPI runs <code>async def</code> routes directly on the main event loop thread via normal coroutine evaluation (<code>await endpoint()</code>). Standard <code>def</code> routes are automatically offloaded to an external worker threadpool via AnyIO (<code>anyio.to_thread.run_sync</code>), ensuring synchronous operations do not block the event loop.
</details>

<details>
<summary>2. Why is declaring a health check endpoint as standard <code>def</code> considered an anti-pattern in production?</summary>
Standard <code>def</code> endpoints run in the AnyIO worker threadpool. If slow database queries or external API calls exhaust the threadpool capacity, incoming health check requests will be queued behind the slow tasks. When the orchestrator's (Kubernetes) liveness probe times out, healthy pods are mistakenly killed. Health checks should always be <code>async def</code>.
</details>

<details>
<summary>3. Can an <code>async def</code> route call a synchronous function? What is the correct way to do it?</summary>
Yes, but calling a blocking synchronous function directly inside <code>async def</code> freezes the event loop. The correct approach is to offload it to the threadpool using <code>await anyio.to_thread.run_sync(sync_function, *args)</code>.
</details>

<details>
<summary>4. What determines the maximum number of concurrent synchronous <code>def</code> requests FastAPI can process?</summary>
It is determined by AnyIO's threadpool capacity limiter (accessible via <code>anyio.to_thread.current_default_thread_limiter()</code>), which defaults to 40 tokens (threads). Any synchronous requests beyond this limit wait in the threadpool queue.
</details>

<details>
<summary>5. Is an <code>async def</code> endpoint inherently faster than a <code>def</code> endpoint for CPU-heavy tasks?</summary>
No. In fact, running CPU-heavy tasks directly in <code>async def</code> is disastrous because it freezes the single-threaded event loop, preventing all other network I/O. In standard Python with GIL, CPU tasks should run in a process pool or worker thread.
</details>

<details>
<summary>6. How does FastAPI handle dependencies declared as <code>def</code> when injected into an <code>async def</code> route?</summary>
FastAPI inspects the dependency function. If the dependency is a standard <code>def</code>, FastAPI executes it in the AnyIO threadpool first, retrieves its result, and injects that result into the <code>async def</code> route on the event loop.
</details>

<details>
<summary>7. What happens if you run <code>time.sleep(5)</code> inside an <code>async def</code> route?</summary>
The OS thread running the event loop is put to sleep for 5 seconds. The event loop cannot tick, process incoming network packets, or resume any waiting tasks. Every client connected to that FastAPI worker process experiences an immediate 5-second freeze.
</details>

<details>
<summary>8. Under what circumstances should you deliberately choose <code>def</code> over <code>async def</code> in FastAPI?</summary>
Choose <code>def</code> when working with legacy synchronous libraries that lack async support, such as standard SQLAlchemy 1.4/2.0 without asyncpg, Django ORM, <code>boto3</code>, <code>PIL/Pillow</code>, or synchronous machine learning inference (e.g. Scikit-learn).
</details>

---

## 12. Practical Exercises & Solutions

### Exercise 1 (Easy): Immune Health Check Route
**Task**: Write a FastAPI application with two endpoints: a simulated slow synchronous route `/heavy-legacy` and a high-availability `/healthz` endpoint that is guaranteed never to be queued behind the legacy route.
<details>
<summary>Solution</summary>

```python
import time
from fastapi import FastAPI

app = FastAPI()

@app.get("/heavy-legacy")
def heavy_legacy():
    time.sleep(2.0)  # Runs safely in threadpool
    return {"status": "legacy-done"}

# Immune to threadpool starvation because it runs on the main event loop
@app.get("/healthz")
async def healthz():
    return {"status": "alive", "healthy": True}
```
</details>

### Exercise 2 (Easy): Safe Offloading Wrapper
**Task**: Create an async route `/compute-checksum` that receives a string and calculates an MD5 digest using Python's synchronous `hashlib` offloaded through `anyio.to_thread.run_sync`.
<details>
<summary>Solution</summary>

```python
import hashlib
import anyio
from fastapi import FastAPI

app = FastAPI()

def sync_hash(text: str) -> str:
    return hashlib.md5(text.encode()).hexdigest()

@app.get("/compute-checksum")
async def compute_checksum(data: str) -> dict[str, str]:
    checksum = await anyio.to_thread.run_sync(sync_hash, data)
    return {"input": data, "md5": checksum}
```
</details>

### Exercise 3 (Medium): Dynamic Threadpool Sizing Lifespan
**Task**: Implement a FastAPI `lifespan` handler that configures AnyIO's thread limiter to 100 worker threads on startup and logs the active thread limit.
<details>
<summary>Solution</summary>

```python
import logging
from contextlib import asynccontextmanager
from typing import AsyncIterator
import anyio
from fastapi import FastAPI

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("server")

@asynccontextmanager
async def lifespan(app: FastAPI) -> AsyncIterator[None]:
    limiter = anyio.to_thread.current_default_thread_limiter()
    limiter.total_tokens = 100
    logger.info("Configured AnyIO default threadpool tokens to: %d", limiter.total_tokens)
    yield
    logger.info("Application shutting down...")

app = FastAPI(lifespan=lifespan)

@app.get("/threadpool-status")
async def threadpool_status() -> dict[str, int]:
    limiter = anyio.to_thread.current_default_thread_limiter()
    return {
        "total_tokens": limiter.total_tokens,
        "borrowed_tokens": limiter.borrowed_tokens
    }
```
</details>

### Exercise 4 (Medium): Parallel Mixed Dependencies
**Task**: Build a route `/hybrid-profile` that depends on both a synchronous dependency `get_geo_ip` (simulating a sync maxmind lookup) and an async dependency `get_session_cache` (simulating an async Redis read). Ensure both resolve correctly.
<details>
<summary>Solution</summary>

```python
import time
import asyncio
from fastapi import FastAPI, Depends

app = FastAPI()

def get_geo_ip() -> dict[str, str]:
    # Synchronous lookup
    time.sleep(0.01)
    return {"country": "US", "city": "Seattle"}

async def get_session_cache() -> dict[str, str]:
    # Asynchronous cache
    await asyncio.sleep(0.01)
    return {"session_id": "sess_88912", "role": "admin"}

@app.get("/hybrid-profile")
async def hybrid_profile(
    geo: dict[str, str] = Depends(get_geo_ip),
    sess: dict[str, str] = Depends(get_session_cache)
) -> dict[str, object]:
    return {"geo": geo, "session": sess}
```
</details>

### Exercise 5 (Hard): Threadpool Concurrency Stress Tester
**Task**: Build a test suite using `pytest` and `httpx` that fires 50 concurrent requests to a sync endpoint `def sleep_worker()` with 0.1s sleep. Verify that when thread tokens are set to 50, all 50 finish in under 0.25 seconds.
<details>
<summary>Solution</summary>

```python
import time
import asyncio
import anyio
import pytest
from httpx import AsyncClient, ASGITransport
from fastapi import FastAPI

test_app = FastAPI()

@test_app.get("/sleep-sync")
def sleep_sync():
    time.sleep(0.1)
    return {"done": True}

@pytest.mark.anyio
async def test_threadpool_concurrency_capacity():
    # Expand threadpool tokens
    limiter = anyio.to_thread.current_default_thread_limiter()
    limiter.total_tokens = 60
    
    transport = ASGITransport(app=test_app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        start_time = asyncio.get_running_loop().time()
        # Fire 50 concurrent requests
        tasks = [client.get("/sleep-sync") for _ in range(50)]
        responses = await asyncio.gather(*tasks)
        total_time = asyncio.get_running_loop().time() - start_time
        
        assert all(r.status_code == 200 for r in responses)
        # If run sequentially, 50 * 0.1 = 5.0s. Concurrently in threadpool, < 0.35s
        assert total_time < 0.35
```
</details>

### Exercise 6 (Hard): Custom Bounded Thread Execution Decorator
**Task**: Implement a decorator `@run_in_custom_limiter(max_concurrent=5)` for FastAPI routes that isolates specific expensive sync functions to their own dedicated AnyIO `CapacityLimiter`, preventing them from exhausting the main threadpool tokens.
<details>
<summary>Solution</summary>

```python
import functools
import time
from typing import Callable, Any, TypeVar
import anyio
from fastapi import FastAPI

T = TypeVar("T")

def run_in_custom_limiter(max_concurrent: int) -> Callable[[Callable[..., T]], Callable[..., Any]]:
    limiter = anyio.CapacityLimiter(max_concurrent)
    
    def decorator(func: Callable[..., T]) -> Callable[..., Any]:
        @functools.wraps(func)
        async def wrapper(*args: Any, **kwargs: Any) -> T:
            # Executes sync func in threadpool bounded by this specific custom limiter
            return await anyio.to_thread.run_sync(
                functools.partial(func, *args, **kwargs),
                limiter=limiter
            )
        return wrapper
    return decorator

app = FastAPI()

def heavy_image_resizing(image_name: str) -> str:
    time.sleep(0.1)
    return f"Processed {image_name}"

@app.post("/resize")
@run_in_custom_limiter(max_concurrent=3)
def resize_endpoint(name: str) -> str:
    return heavy_image_resizing(name)
```
</details>
