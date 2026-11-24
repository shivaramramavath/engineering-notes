---
title: "Common Async Mistakes and Anti-Patterns in FastAPI"
category: "09-async-and-concurrency"
order: 10
tags: ["fastapi", "async-mistakes", "anti-patterns", "concurrency", "troubleshooting"]
py_version: "3.12+"
fastapi_version: "0.115+"
starlette_version: "0.38+"
pydantic_version: "2.8+"
tested_with: "Python 3.12, 3.13"
---

# Common Async Mistakes and Anti-Patterns in FastAPI

## 1. Conceptual Foundation & Mental Model

Asynchronous architecture in Python is deceptively simple: prefixing a function with `async def` and using `await` appears as straightforward as writing synchronous code. However, the underlying mechanics—cooperative scheduling, shared single-thread execution, and task cancellation semantics—introduce failure modes that do not exist in traditional multi-threaded or multi-process systems.

A single subtle mistake—such as calling a synchronous cryptographic function, catching `BaseException` without re-raising, or spawning unreferenced tasks—can silently cascade into global application outages, memory exhaustion, or corrupted database states.

### The Mental Model: The High-Speed Monorail System
- An asynchronous FastAPI server is like a single-track high-speed monorail system. 
- In this system, dozens of trains (coroutines) operate simultaneously on a single track, coordinating their movements through automated signaling switches (`await`).
- If every train moves when scheduled and yields at junctions, thousands of passengers are transported per second with near-zero friction.
- However, if a single train driver stops the train on the main track to hand-write a postal letter (a blocking synchronous call), **every train on the entire railway network stops behind it**. 
- Furthermore, if a switch operator improperly ignores emergency warning signals (swallowing cancellation), trains collide or derail (state corruption and resource leaks).

---

## 2. Architecture & The Anatomy of Async Anti-Patterns

```
                             Common Anti-Pattern Hazards
                                          |
        +------------------+--------------+------------------+------------------+
        |                  |                                 |                  |
        v                  v                                 v                  v
  Loop Starvation   Swallowed Cancel    Memory Leaks / Zombie Tasks   Multi-Worker Lock Failures
  (Sync I/O in      (Missing re-raise   (Unreferenced Task reaped     (`asyncio.Lock` across
  `async def`)      on disconnects)     by Python Garbage Collector)  multiple OS processes)
```

### The Suspension Invariant
Every time `await` is written, the programmer must assume:
1. **Time Passes**: State can and will be mutated by other coroutines before the next line of code executes.
2. **Cancellation May Occur**: An `asyncio.CancelledError` may be injected at that exact line if the client disconnects or an upstream timeout expires.
3. **Execution May Never Resume**: If an unhandled exception occurs in a sibling task within a `TaskGroup`, execution terminates at this line.

---

## 3. Comprehensive Syntax & Core Mechanics

### Comparison: Fatal Anti-Patterns vs Idiomatic Corrections

```python
# 1. ANTI-PATTERN: Blocking I/O inside async def
async def bad_file_read():
    with open("data.json", "r") as f:  # Blocks the entire event loop!
        return f.read()

# CORRECTION: Offload to threadpool or use non-blocking async file driver
import anyio
async def good_file_read():
    return await anyio.to_thread.run_sync(lambda: open("data.json").read())


# 2. ANTI-PATTERN: Swallowing CancelledError
async def bad_cancellation_handler():
    try:
        await asyncio.sleep(10.0)
    except Exception:
        pass  # In Python 3.8+, CancelledError is BaseException, but catching BaseException will swallow it!
    except BaseException:
        pass  # CATASTROPHIC: Prevents task cancellation!

# CORRECTION: Always re-raise CancelledError
async def good_cancellation_handler():
    try:
        await asyncio.sleep(10.0)
    except asyncio.CancelledError:
        # Perform cleanup, then re-raise!
        raise


# 3. ANTI-PATTERN: Fire-and-forget task without strong reference
def bad_spawn():
    # Garbage collector can reap this task mid-execution!
    asyncio.create_task(background_work())

# CORRECTION: Retain strong reference in a persistent set
active_tasks = set()
def good_spawn():
    t = asyncio.create_task(background_work())
    active_tasks.add(t)
    t.add_done_callback(active_tasks.discard)
```

---

## 4. Practical Implementation

### Example 1: The Accidental Blocking Network Call
```python
import requests  # SYNCHRONOUS HTTP CLIENT
import httpx     # ASYNCHRONOUS HTTP CLIENT
from fastapi import FastAPI

app = FastAPI()

# MISTAKE
@app.get("/bad-weather")
async def bad_weather():
    # requests.get halts the single-threaded event loop!
    # Under load, p99 latency skyrockets to seconds.
    resp = requests.get("https://wttr.in/London?format=j1")
    return resp.json()

# CORRECTION
@app.get("/good-weather")
async def good_weather():
    # httpx.AsyncClient cooperates with the loop via non-blocking sockets
    async with httpx.AsyncClient() as client:
        resp = await client.get("https://wttr.in/London?format=j1")
        return resp.json()
```

### Example 2: The Silent Dependency Cleanup Leak
```python
from fastapi import FastAPI, Depends

app = FastAPI()

# MISTAKE: Code after yield executes outside try/finally
async def bad_db_session():
    db = {"connection": "open"}
    yield db
    # If endpoint raises an exception, THIS LINE IS SKIPPED! Connection leaked!
    db["connection"] = "closed"

# CORRECTION: Always wrap yield in try/finally
async def good_db_session():
    db = {"connection": "open"}
    try:
        yield db
    finally:
        # Guaranteed to execute even if endpoint crashes or client disconnects
        db["connection"] = "closed"
```

### Example 3: Edge Case — Coroutine Called Without `await`
```python
from fastapi import FastAPI

app = FastAPI()

async def send_slack_alert(msg: str):
    print(f"Alert sent: {msg}")

# MISTAKE
@app.post("/bad-alert")
async def bad_alert():
    # Calling coroutine without await only returns a coroutine object!
    # It does NOT execute!
    send_slack_alert("Server is running out of disk space!")
    # RuntimeWarning: coroutine 'send_slack_alert' was never awaited
    return {"status": "alert dispatched (FALSE!)"}

# CORRECTION
@app.post("/good-alert")
async def good_alert():
    await send_slack_alert("Server is running out of disk space!")
    return {"status": "alert dispatched"}
```

### Example 4: Failure Case — Non-Atomic State Read-Modify-Write
```python
import asyncio
from fastapi import FastAPI

app = FastAPI()
inventory = {"widgets": 10}

# MISTAKE: Concurrency race condition across await boundary
@app.post("/bad-purchase")
async def bad_purchase():
    global inventory
    if inventory["widgets"] > 0:
        # Suspension point yields execution!
        await asyncio.sleep(0.01)
        # Another request could have purchased the last widget during the sleep!
        inventory["widgets"] -= 1
        return {"status": "purchased"}
    return {"status": "out of stock"}

# CORRECTION: Synchronize critical section via Lock or DB Row Lock
inv_lock = asyncio.Lock()

@app.post("/good-purchase")
async def good_purchase():
    global inventory
    async with inv_lock:
        if inventory["widgets"] > 0:
            await asyncio.sleep(0.01)
            inventory["widgets"] -= 1
            return {"status": "purchased"}
        return {"status": "out of stock"}
```

### Example 5: Idiomatic Modern — Safe Concurrency with Structured Error Boundaries
```python
import asyncio
from typing import Any
from fastapi import FastAPI, HTTPException

app = FastAPI()

async def resilient_fetch(coro, fallback: Any) -> Any:
    try:
        return await coro
    except Exception as exc:
        print(f"Subtask failed gracefully: {exc}")
        return fallback

@app.get("/dashboard/resilient")
async def resilient_dashboard():
    # Use modern TaskGroup with isolated error boundaries
    async with asyncio.TaskGroup() as tg:
        t_user = tg.create_task(resilient_fetch(asyncio.sleep(0.01), fallback="Default User"))
        t_feed = tg.create_task(resilient_fetch(asyncio.sleep(0.02), fallback=[]))

    return {
        "user": t_user.result(),
        "feed": t_feed.result()
    }
```

---

## 5. Edge Cases, Pitfalls & Failure Modes

1. **`asyncio.gather(*tasks)` Orphan Task Leaks**: When `gather()` is invoked without `return_exceptions=True`, the first task to raise an exception causes `gather` to immediately return the error. However, the other tasks continue running unmonitored in the background.
2. **Missing `lifespan` Cleanup**: Initializing async database connection pools or HTTP clients in module scope or startup events without a corresponding shutdown cleanup leaks open TCP sockets on process reload.
3. **Using Thread-Local Storage in Async Code**: `threading.local()` is bound to OS threads. Because multiple coroutines execute on the same thread, coroutines will overwrite each other's data! Always use `contextvars.ContextVar` instead of `threading.local`.
4. **Nested Event Loop Creation (`asyncio.run()`)**: Calling `asyncio.run()` inside an `async def` route raises `RuntimeError: This event loop is already running`.

---

## 6. Common Mistakes & Anti-Patterns Reference Table

| Anti-Pattern | Root Cause | Real-World Impact | Idiomatic Fix |
|:---|:---|:---|:---|
| Calling sync network client (`requests`, `boto3`) in `async def` | Misunderstanding `async def` single-thread execution | Complete event loop freeze; all clients stall | Use `httpx.AsyncClient` or offload via `anyio.to_thread.run_sync` |
| Calling coroutines without `await` | Treating coroutine call as immediate execution | Coroutine never runs; `RuntimeWarning: never awaited` | Always `await coro()` or schedule via `tg.create_task()` |
| Swallowing `CancelledError` | Writing generic `except BaseException: pass` | Prevents task cancellation on timeouts/disconnects | Always re-raise `asyncio.CancelledError` |
| Fire-and-forget `create_task()` without reference | Python cyclic garbage collector sweeps unreferenced task | Task vanishes mid-execution without error | Store active task in a persistent `set` |
| Using `threading.local` for request context | Async coroutines share the same OS thread | Cross-request data leakage and tenant contamination | Use `contextvars.ContextVar` |
| Using `asyncio.Lock` across multiple Uvicorn workers | Assuming in-memory lock crosses OS process boundaries | Silent race conditions and data corruption | Use Redis distributed locks or Postgres `FOR UPDATE` |
| Missing `try...finally` in `yield` dependency | Unhandled route exceptions skip post-yield code | Connection pool exhaustion and memory leaks | Wrap all post-yield cleanups in `try...finally` |
| Declaring pure CPU tasks in `async def` | Forgetting CPU bytecode holds the GIL | Server responsiveness collapses to 0 req/sec | Offload to `ProcessPoolExecutor` |

---

## 7. Performance & Scalability Considerations

- **Throughput Penalties**: A single blocking 100ms synchronous call executed in an `async def` route limits the worker's theoretical maximum throughput to 10 requests per second, regardless of CPU core count.
- **ContextVars Overhead**: `ContextVar` lookup and mutation has $O(1)$ overhead (~30ns), making it vastly superior to thread-local or dictionary lookups in async loops.

---

## 8. Security & OWASP Guardrails

- **Data Contamination via Shared State**: Using global mutable state (e.g. global dicts or module variables) across async requests can allow sensitive user data (e.g. tokens, PII) from Request A to be returned to Request B.
- **Denial of Service via Loop Freezing**: An unauthenticated attacker sending payloads that trigger synchronous regex or crypto operations inside `async def` routes can trivially achieve complete Denial of Service.

---

## 9. Testing Strategies & Pytest Recipes

```python
import pytest
import asyncio
from httpx import AsyncClient, ASGITransport
from fastapi import FastAPI, Depends

app = FastAPI()
cleaned_up = False

async def safe_dependency():
    global cleaned_up
    cleaned_up = False
    try:
        yield "resource"
    finally:
        cleaned_up = True

@app.get("/error-route")
async def error_route(res: str = Depends(safe_dependency)):
    raise ValueError("Simulated Route Failure")

@pytest.mark.anyio
async def test_dependency_cleans_up_even_on_crash():
    global cleaned_up
    transport = ASGITransport(app=app, raise_app_exceptions=False)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        resp = await client.get("/error-route")
        assert resp.status_code == 500
        
    # Crucial assertion: dependency cleanup MUST run despite crash
    assert cleaned_up is True
```

---

## 10. Real-World Case Study / Production Incident

**The Incident**: A healthcare API allowed doctors to update patient prescriptions. Shortly after migrating to FastAPI and async routes, the hospital experienced an audit failure: patient records were being updated with prescriptions belonging to completely different patients.

**Root Cause**: A developer used `threading.local()` to store the currently authenticated user's `patient_id` during request authentication middleware. Because FastAPI runs async coroutines on the same single operating system thread, when Doctor A's request yielded control during a database query, Doctor B's request began executing on the same thread, overwriting `threading.local().patient_id`. When Doctor A's query resumed, it read Doctor B's patient ID!

**The Fix**:
Replaced `threading.local` with Python's asynchronous-native `contextvars.ContextVar`:
```python
from contextvars import ContextVar

# Thread-safe and Coroutine-safe context propagation
current_patient_id: ContextVar[str] = ContextVar("current_patient_id")

# In middleware:
token = current_patient_id.set(extracted_id)
# In database repository:
patient_id = current_patient_id.get()
```
ContextVars are isolated per asynchronous execution context, eliminating cross-request data corruption permanently.

---

## 11. Technical Interview Questions & Answers

<details>
<summary>1. Why must you never use <code>threading.local()</code> to store request-scoped data in FastAPI?</summary>
In an async FastAPI application, multiple concurrent requests (coroutines) execute on the <b>same OS thread</b>. <code>threading.local()</code> binds data to the thread. If Coroutine A yields execution, Coroutine B executing on the same thread can overwrite the data in <code>threading.local()</code>, causing severe cross-request data leaks. Always use <code>contextvars.ContextVar</code>.
</details>

<details>
<summary>2. What happens if you catch <code>BaseException</code> in an async task and do not re-raise it?</summary>
In Python 3.8+, <code>asyncio.CancelledError</code> inherits from <code>BaseException</code>. If you catch <code>BaseException</code> and swallow it, task cancellation signals (issued by client disconnects, timeouts, or TaskGroups) will be ignored. The task will continue running indefinitely as a zombie task.
</details>

<details>
<summary>3. Why does <code>asyncio.create_task()</code> sometimes cause tasks to disappear mid-execution in Python 3.12+?</summary>
If you do not retain a strong reference to the returned <code>Task</code> object, Python's garbage collector may identify the task object as unreachable during a garbage collection cycle while the task is suspended awaiting I/O. The GC reaps the task, aborting execution silently without error.
</details>

<details>
<summary>4. How does an unhandled exception inside a route affect a <code>yield</code> dependency?</summary>
FastAPI catches the exception and throws it into the dependency generator at the <code>yield</code> statement using <code>athrow()</code>. If the generator has a <code>try / finally</code> block, the <code>finally</code> block executes. If no <code>try / finally</code> is present, execution aborts immediately at <code>yield</code>, leaking resources.
</details>

<details>
<summary>5. What is the danger of calling a function returning a coroutine without using <code>await</code>?</summary>
Calling an <code>async def</code> function without <code>await</code> does not execute the function; it only constructs and returns an unawaited coroutine object. The intended logic is never run, and Python raises a <code>RuntimeWarning: coroutine was never awaited</code>.
</details>

<details>
<summary>6. How can you safely run a CPU-intensive calculation without blocking the FastAPI event loop?</summary>
Use <code>concurrent.futures.ProcessPoolExecutor</code> and dispatch via <code>await asyncio.get_running_loop().run_in_executor(pool, func, *args)</code>. This offloads the computation to a separate OS process with its own GIL.
</details>

<details>
<summary>7. What happens if you invoke <code>asyncio.run()</code> inside an existing async FastAPI route?</summary>
Python raises <code>RuntimeError: This event loop is already running</code> because <code>asyncio.run()</code> cannot be called when an event loop is already active on the current thread.
</details>

<details>
<summary>8. Why is using <code>asyncio.Lock</code> insufficient for multi-worker FastAPI deployments?</summary>
<code>asyncio.Lock</code> resides in the virtual memory of a single OS process. In production with multiple Uvicorn workers or multiple container replicas, each process has its own isolated memory. Synchronization across workers requires an external distributed lock (e.g. Redis Redlock or Postgres row locks).
</details>

---

## 12. Practical Exercises & Solutions

### Exercise 1 (Easy): Catching Missing Await Warnings
**Task**: Write a unit test that verifies that invoking a coroutine function directly without `await` generates a `RuntimeWarning`.
<details>
<summary>Solution</summary>

```python
import pytest
import warnings

async def dummy_coro():
    return "done"

def test_unawaited_coroutine_warning():
    with pytest.warns(RuntimeWarning, match="coroutine 'dummy_coro' was never awaited"):
        # Intentionally not awaiting
        coro_obj = dummy_coro()
        # Explicitly delete to trigger GC warning immediately
        del coro_obj
```
</details>

### Exercise 2 (Easy): ContextVar Isolation Verification
**Task**: Demonstrate that two concurrent async tasks writing to a `ContextVar` maintain isolated values without leaking across tasks.
<details>
<summary>Solution</summary>

```python
import asyncio
from contextvars import ContextVar
import pytest

request_id_var: ContextVar[str] = ContextVar("request_id")

async def worker(val: str) -> str:
    request_id_var.set(val)
    await asyncio.sleep(0.01)  # Context switch
    return request_id_var.get()

@pytest.mark.anyio
async def test_contextvar_concurrency_isolation():
    task1 = asyncio.create_task(worker("REQ_AAA"))
    task2 = asyncio.create_task(worker("REQ_BBB"))
    
    res1, res2 = await asyncio.gather(task1, task2)
    assert res1 == "REQ_AAA"
    assert res2 == "REQ_BBB"
```
</details>

### Exercise 3 (Medium): Safe Cancellation-Aware Task
**Task**: Implement an async worker function that performs a loop of 10 steps. If cancelled midway, it catches `asyncio.CancelledError`, logs the cancellation step, and re-raises the error cleanly.
<details>
<summary>Solution</summary>

```python
import asyncio
import logging

logger = logging.getLogger("worker")

async def cancellable_batch_job():
    step = 0
    try:
        for step in range(1, 11):
            await asyncio.sleep(0.1)
            logger.info("Completed step %d", step)
    except asyncio.CancelledError:
        logger.warning("Batch job cancelled at step %d! Cleaning up...", step)
        # CRITICAL: Always re-raise CancelledError
        raise
```
</details>

### Exercise 4 (Medium): Robust Persistent Background Task Spawner
**Task**: Create a FastAPI utility class `BackgroundTaskRegistry` that holds strong references to spawned background tasks to prevent premature garbage collection, removing references upon task completion.
<details>
<summary>Solution</summary>

```python
import asyncio
from typing import Coroutine, Any

class SafeTaskRegistry:
    def __init__(self):
        self._tasks: set[asyncio.Task[Any]] = set()

    def spawn(self, coro: Coroutine[Any, Any, Any], name: str | None = None) -> asyncio.Task[Any]:
        task = asyncio.create_task(coro, name=name)
        self._tasks.add(task)
        task.add_done_callback(self._tasks.discard)
        return task

    @property
    def active_count(self) -> int:
        return len(self._tasks)

registry = SafeTaskRegistry()
```
</details>

### Exercise 5 (Hard): ThreadPool Leak Detector Middleware
**Task**: Build an ASGI middleware that records AnyIO threadpool borrowed tokens before and after request dispatch. If a request causes borrowed tokens to not return to zero after completion, log a critical warning alerting to a threadpool leak.
<details>
<summary>Solution</summary>

```python
import anyio
import logging
from fastapi import FastAPI, Request, Response
from starlette.middleware.base import BaseHTTPMiddleware, RequestResponseEndpoint

logger = logging.getLogger("leak_detector")
app = FastAPI()

class ThreadpoolLeakDetectorMiddleware(BaseHTTPMiddleware):
    async def dispatch(self, request: Request, call_next: RequestResponseEndpoint) -> Response:
        limiter = anyio.to_thread.current_default_thread_limiter()
        tokens_before = limiter.borrowed_tokens
        
        try:
            response = await call_next(request)
            return response
        finally:
            tokens_after = limiter.borrowed_tokens
            if tokens_after > tokens_before:
                logger.warning(
                    "Potential thread leak on %s! Tokens before: %d, after: %d",
                    request.url.path, tokens_before, tokens_after
                )

app.add_middleware(ThreadpoolLeakDetectorMiddleware)
```
</details>

### Exercise 6 (Hard): TOCTOU Race Condition Stress Harness
**Task**: Write a comprehensive `pytest` test suite with 100 concurrent requests attempting to purchase from an inventory of 1 item on an endpoint. Verify that a properly locked endpoint processes exactly 1 purchase and 99 out-of-stock failures.
<details>
<summary>Solution</summary>

```python
import asyncio
import pytest
from httpx import AsyncClient, ASGITransport
from fastapi import FastAPI, HTTPException

app = FastAPI()
inventory = 1
mutex = asyncio.Lock()

@app.post("/buy")
async def buy():
    global inventory
    async with mutex:
        if inventory > 0:
            await asyncio.sleep(0.001)  # Context switch
            inventory -= 1
            return {"status": "bought"}
        raise HTTPException(status_code=400, detail="Sold out")

@pytest.mark.anyio
async def test_toctou_concurrency_stress():
    global inventory
    inventory = 1
    transport = ASGITransport(app=app)
    
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        # Fire 100 concurrent purchase attempts
        tasks = [client.post("/buy") for _ in range(100)]
        responses = await asyncio.gather(*tasks)
        
        successes = [r for r in responses if r.status_code == 200]
        failures = [r for r in responses if r.status_code == 400]
        
        # Exactly 1 customer got the item, 99 received 400 Sold out
        assert len(successes) == 1
        assert len(failures) == 99
        assert inventory == 0
```
</details>
