---
title: "Python Async Internals: Event Loop, Coroutines, and Tasks"
category: "09-async-and-concurrency"
order: 1
tags: ["fastapi", "asyncio", "event-loop", "coroutines", "tasks", "python-internals"]
py_version: "3.12+"
fastapi_version: "0.115+"
starlette_version: "0.38+"
pydantic_version: "2.8+"
tested_with: "Python 3.12, 3.13 (free-threading safe)"
---

# Python Async Internals: Event Loop, Coroutines, and Tasks

## 1. Conceptual Foundation & Mental Model

Asynchronous programming in Python via `asyncio` is cooperative multitasking single-threaded execution (or per-thread execution in free-threaded Python 3.13t). Unlike preemptive multitasking operating system threads where the kernel interrupts execution arbitrarily via time-slicing interrupts, cooperative multitasking relies on the executing code to explicitly yield control back to an orchestrator known as the **Event Loop**.

To understand `asyncio` in FastAPI, one must delineate three foundational primitives:
1. **Generators & Coroutines**: A generator pauses execution via `yield`. Python 3.5 formalized native coroutines via `async def`, which instantiate coroutine objects adhering to the `awaitable` protocol (`__await__()`). A coroutine does not run when called; calling `coro_fn()` merely constructs a suspended generator-like state machine frame.
2. **The Event Loop**: An infinite poll loop (typically implemented using OS multiplexing primitives such as Linux `epoll`, BSD/macOS `kqueue`, or Windows `IOCP` via `selectors` or C-based `uvloop`). It tracks file descriptors (sockets) registered for read/write readiness, maintains a priority queue of scheduled timer callbacks, and drives ready tasks to their next suspension point.
3. **Tasks and Futures**: A `Future` represents an eventual result of an asynchronous operation (a low-level callback sink). A `Task` is a concrete subclass of `asyncio.Future` that wraps a coroutine, registers itself with the event loop, and schedules its step-by-step execution across loop iterations.

### The Mental Model: The Chef and Order Tickets
Imagine a single chef in a kitchen (the single thread running the event loop). 
- In **synchronous/blocking execution**, the chef puts bread in the toaster and stares at the heating coil for 3 minutes without moving. No other orders can be taken or cooked.
- In **cooperative asynchronous execution**, the chef drops bread in the toaster, sets an alarm callback with the kitchen timer (OS socket/timer), and immediately turns around to chop vegetables or plate another meal. When the timer rings (the file descriptor signals `EPOLLIN`), the chef resumes the toasted bread task (`await`).
- If the chef starts doing an intensive 2-minute mathematical calculation on paper at the prep counter without yielding (CPU-bound work or synchronous `time.sleep()`), the toaster dings unheeded, water boils over, and tickets accumulate at the door.

---

## 2. Architecture & Internal Mechanics

```
+-----------------------------------------------------------------------------------+
|                              Python Event Loop                                    |
|                                                                                   |
|  +---------------------------+       +-----------------------------------------+  |
|  |     Ready Queue (FIFO)    |       |        Scheduled Timers (Min-Heap)      |  |
|  | [Task 1] [Task 3] [Task 5]|       |  t=100.2ms -> Callback A               |  |
|  +-------------+-------------+       |  t=105.0ms -> Callback B               |  |
|                |                     +-----------------------------------------+  |
|                v                                                                  |
|       Execute Current Task                                                        |
|      (task._step() -> coro.send(None))                                            |
|                |                                                                  |
|         Yields at `await`                                                         |
|         Registers I/O Descriptor with Selector                                    |
|                v                                                                  |
|  +-----------------------------------------------------------------------------+  |
|  |                      OS Multiplexer (epoll / kqueue / IOCP)                 |  |
|  |                                                                             |  |
|  |   FD 4 (Postgres Socket)  ---> Ready to Read   ---> Wake Task 2             |  |
|  |   FD 7 (Redis Socket)     ---> Waiting...                                   |  |
|  |   FD 9 (HTTP Client)      ---> Ready to Write  ---> Wake Task 4             |  |
|  +-----------------------------------------------------------------------------+  |
+-----------------------------------------------------------------------------------+
```

### Frame Execution Mechanics
When Python evaluates `await awaitable`:
1. The coroutine object implements `__await__()`, which returns an iterator.
2. The current coroutine yields execution down the call stack until reaching the root `asyncio.Task`.
3. The Task's internal method `task._step()` captures the yielded control message (typically an unregistered or registered Future).
4. The task attaches a callback `future.add_done_callback(task._wakeup)`.
5. The loop moves to the next ready task in its FIFO queue or polls the OS selector (`select()`, `epoll_wait()`).
6. When the underlying OS socket receives bytes, the selector notifies the loop. The loop fires the done callback, appending the waiting Task back to the Ready Queue.
7. On the next loop tick, `task._step()` invokes `coro.send(result)` or `coro.throw(exception)`, resuming execution directly after the `await` statement.

---

## 3. Comprehensive Syntax & Core Mechanics

### Coroutine Definition vs Task Creation
```python
import asyncio
from typing import Any

async def fetch_data(source_id: int) -> dict[str, Any]:
    # Coroutine definition: calling fetch_data(1) DOES NOT execute it!
    await asyncio.sleep(0.05)  # Yields control back to the loop
    return {"source": source_id, "status": "ok"}

async def orchestrate() -> list[dict[str, Any]]:
    # Sequential execution: total runtime = 0.05 + 0.05 = 0.10s
    res1 = await fetch_data(1)
    res2 = await fetch_data(2)

    # Concurrent execution via Tasks:
    # asyncio.create_task schedules the coroutine onto the loop immediately
    task1 = asyncio.create_task(fetch_data(3))
    task2 = asyncio.create_task(fetch_data(4))
    
    # Both tasks run concurrently while awaiting
    results = await asyncio.gather(task1, task2)
    return [res1, res2, *results]
```

### Inspecting Running Loop State
```python
import asyncio

def inspect_event_loop_state() -> None:
    loop = asyncio.get_running_loop()
    print(f"Loop class: {loop.__class__.__name__}")
    print(f"Is running: {loop.is_running()}")
    print(f"Is closed: {loop.is_closed()}")
    print(f"Debug mode: {loop.get_debug()}")
    
    # Introspect all currently active tasks
    all_tasks = asyncio.all_tasks(loop)
    current_task = asyncio.current_task(loop)
    print(f"Active tasks count: {len(all_tasks)}")
    print(f"Currently executing task: {current_task.get_name() if current_task else None}")
```

---

## 4. Practical Implementation

### Example 1: Basic Event Loop Introspection in FastAPI
```python
import asyncio
from fastapi import FastAPI

app = FastAPI(title="Loop Inspector API")

@app.get("/loop-info")
async def get_loop_info() -> dict[str, object]:
    loop = asyncio.get_running_loop()
    current = asyncio.current_task(loop)
    all_tasks = asyncio.all_tasks(loop)
    
    return {
        "loop_implementation": loop.__class__.__module__ + "." + loop.__class__.__qualname__,
        "current_task_name": current.get_name() if current else "unknown",
        "total_active_tasks": len(all_tasks),
        "task_names": [t.get_name() for t in list(all_tasks)[:10]]
    }
```

### Example 2: Production Multi-Worker Concurrent Data Aggregator
```python
import asyncio
from typing import Any
import httpx
from fastapi import FastAPI, HTTPException, status

app = FastAPI()

async def fetch_upstream(client: httpx.AsyncClient, service_name: str, url: str) -> dict[str, Any]:
    try:
        response = await client.get(url, timeout=2.0)
        response.raise_for_status()
        return {"service": service_name, "data": response.json(), "error": None}
    except Exception as exc:
        return {"service": service_name, "data": None, "error": str(exc)}

@app.get("/aggregate")
async def aggregate_dashboard() -> dict[str, Any]:
    endpoints = {
        "billing": "https://api.internal.local/billing/summary",
        "inventory": "https://api.internal.local/inventory/status",
        "notifications": "https://api.internal.local/notifications/unread",
    }
    
    async with httpx.AsyncClient() as client:
        # Schedule tasks concurrently using modern TaskGroup (Python 3.11+)
        tasks: dict[str, asyncio.Task[dict[str, Any]]] = {}
        async with asyncio.TaskGroup() as tg:
            for name, url in endpoints.items():
                tasks[name] = tg.create_task(
                    fetch_upstream(client, name, url),
                    name=f"task-upstream-{name}"
                )
        
        # Results are guaranteed ready once TaskGroup exits cleanly
        results = {name: task.result() for name, task in tasks.items()}
        return {"aggregated_at": asyncio.get_running_loop().time(), "services": results}
```

### Example 3: Edge Case — Handling Timeouts Without Leaking Background Execution
```python
import asyncio
from fastapi import FastAPI, HTTPException, status

app = FastAPI()

async def long_running_query() -> str:
    try:
        # Simulate cancellation-aware async call
        await asyncio.sleep(5.0)
        return "Query Complete"
    except asyncio.CancelledError:
        # Clean up database resources, release locks
        # MUST re-raise CancelledError to fulfill async cancellation contract
        raise

@app.get("/query-with-timeout")
async def query_with_timeout() -> dict[str, str]:
    try:
        # asyncio.timeout context manager cancels the task tree upon deadline expiry
        async with asyncio.timeout(1.0):
            result = await long_running_query()
            return {"status": "success", "result": result}
    except TimeoutError:
        raise HTTPException(
            status_code=status.HTTP_504_GATEWAY_TIMEOUT,
            detail="Upstream processing exceeded 1.0 second threshold"
        )
```

### Example 4: Failure Case — Loop Blocking via Synchronous Sleep / Crypto Hashing
```python
import time
import hashlib
from fastapi import FastAPI

app = FastAPI()

# ANTI-PATTERN: This blocks the OS thread running the single event loop.
# During this execution, NO OTHER HTTP REQUESTS CAN BE PROCESSED.
@app.get("/bad-blocking")
async def bad_blocking() -> dict[str, str]:
    # Blocks for 2 seconds synchronously
    time.sleep(2.0)
    
    # Heavy CPU hashing in async def blocks loop ticks
    data = b"secret_payload" * 100000
    for _ in range(500):
        data = hashlib.sha256(data).digest()
        
    return {"status": "starved other requests"}
```

### Example 5: Idiomatic Modern — Safe Offloading of Blocking Work
```python
import hashlib
import anyio
from fastapi import FastAPI

app = FastAPI()

def cpu_heavy_hashing(payload: bytes, rounds: int) -> str:
    # Pure synchronous CPU-bound function
    curr = payload
    for _ in range(rounds):
        curr = hashlib.sha256(curr).digest()
    return curr.hex()

@app.post("/safe-compute")
async def safe_compute() -> dict[str, str]:
    # anyio.to_thread.run_sync dispatches the blocking callable to the threadpool worker
    # leaving the event loop free to handle hundreds of concurrent I/O requests
    digest = await anyio.to_thread.run_sync(
        cpu_heavy_hashing,
        b"secure_input_vector" * 1000,
        50000
    )
    return {"digest": digest}
```

---

## 5. Edge Cases, Pitfalls & Failure Modes

1. **Unawaited Coroutines**: Calling `coro()` without `await` or `asyncio.create_task()` creates a `coroutine object` that is immediately garbage collected, generating `RuntimeWarning: coroutine '...' was never awaited`. The scheduled code never executes.
2. **Loop Starvation via Synchronous Calls**: Calling `requests.get()`, `psycopg2.connect()`, or `time.sleep()` inside an `async def` route stops the entire ASGI worker process. Latency for all concurrent clients spikes to the duration of the synchronous call.
3. **Catching `BaseException` or Swallowing `CancelledError`**: In Python 3.8+, `asyncio.CancelledError` inherits from `BaseException`. Catching `Exception` will not intercept cancellation, but catching `BaseException` without re-raising prevents task cancellation and causes timeouts to hang forever.
4. **Fire-and-Forget Task Garbage Collection**: Creating a task via `asyncio.create_task(work())` without keeping a strong reference in a persistent `set` allows Python's cyclic garbage collector to reap the task midway through execution if a GC pass occurs while suspended.
5. **Cross-Thread Event Loop Access**: Calling `loop.create_task()` or interacting with coroutines from a different OS thread without `loop.call_soon_threadsafe()` triggers race conditions and crashes the loop state.

---

## 6. Common Mistakes & Anti-Patterns

| Mistake / Anti-Pattern | Root Cause | Impact | Correct Approach |
|:---|:---|:---|:---|
| Invoking `time.sleep()` in `async def` | Treating `async def` as multi-threaded | Freezes the event loop; drops throughput to 1 req/sec | Use `await asyncio.sleep()` or offload via `anyio.to_thread.run_sync` |
| Calling coroutines without `await` | Forgetting that `coro()` only returns a generator frame | Function never executes; data loss; silent failures | Always `await coro()` or schedule via `tg.create_task(coro())` |
| Swallowing `asyncio.CancelledError` | Writing generic `except BaseException:` without re-raise | Prevents task cancellation during client disconnects and timeouts | Always re-raise `asyncio.CancelledError` |
| Using `asyncio.gather(*tasks)` without error boundaries | One failed task can leave remaining tasks running unmonitored | Resource leakage, unhandled exceptions | Use `asyncio.TaskGroup` for structured concurrency |
| Creating unreferenced tasks (`asyncio.create_task(...)`) | Python GC reaps unreferenced tasks suspended on I/O | Task disappears mid-execution without error or log | Store active tasks in a module-level `set` and discard on done |
| Initializing async resources at module import level | No event loop exists during initial module load | `RuntimeError: There is no current event loop` | Initialize resources inside the `lifespan` context manager |

---

## 7. Performance & Scalability Considerations

- **`uvloop` Replacement**: Under Linux/macOS, replacing standard `asyncio` with `uvloop` (via `uvicorn --loop uvloop` or `uvloop.install()`) replaces Python's pure-Python selectors with `libuv` (the C engine powering Node.js), yielding a 2x-4x throughput improvement on network I/O.
- **Task Allocation Overhead**: Instantiating a `Task` incurs memory allocations (approx 1-2 KB per task). For micro-operations executing in sub-microseconds, direct coroutine awaiting (`await inner_coro()`) is 10x faster than wrapping in `asyncio.create_task()`.
- **Free-Threading (Python 3.13t)**: In Python 3.13 free-threaded builds without GIL, each OS thread runs its own distinct event loop. FastAPI workers can scale across all CPU cores within a single process space using thread pools without inter-process IPC overhead.

---

## 8. Security & OWASP Guardrails

- **Event Loop Denial of Service (DoS)**: An attacker transmitting payload sizes that force heavy synchronous JSON parsing or regex backtracking inside an `async def` route can freeze the event loop, starving all legitimate users. Enforce strict `Content-Length` limits and stream large payloads.
- **Resource Exhaustion via Unbounded Task Spawning**: Allowing clients to trigger background tasks without bounded semaphores (`asyncio.Semaphore(max_concurrent)`) allows an attacker to exhaust memory by issuing 50,000 rapid requests.

---

## 9. Testing Strategies & Pytest Recipes

```python
import pytest
import asyncio
from httpx import AsyncClient, ASGITransport
from fastapi import FastAPI

test_app = FastAPI()

@test_app.get("/ping")
async def ping() -> dict[str, str]:
    await asyncio.sleep(0.01)
    return {"message": "pong"}

@pytest.mark.anyio
async def test_async_event_loop_execution():
    transport = ASGITransport(app=test_app)
    async with AsyncClient(transport=transport, base_url="http://testserver") as client:
        response = await client.get("/ping")
        assert response.status_code == 200
        assert response.json() == {"message": "pong"}

@pytest.mark.anyio
async def test_concurrent_requests_do_not_block():
    transport = ASGITransport(app=test_app)
    async with AsyncClient(transport=transport, base_url="http://testserver") as client:
        start_time = asyncio.get_running_loop().time()
        # Fire 10 requests concurrently
        tasks = [client.get("/ping") for _ in range(10)]
        responses = await asyncio.gather(*tasks)
        elapsed = asyncio.get_running_loop().time() - start_time
        
        assert all(r.status_code == 200 for r in responses)
        # 10 requests with 0.01s sleep should take ~0.02s total concurrently, not 0.10s
        assert elapsed < 0.08
```

---

## 10. Real-World Case Study / Production Incident

**The Incident**: A high-volume FinTech payment gateway built on FastAPI experienced severe latency spikes where p99 latency jumped from 15ms to 8,500ms during peak trading hours. CPU utilization was under 18%, and database connection pools were healthy.

**Root Cause**: A newly deployed audit-logging module included an IP geolocation lookup function that used Python's built-in `socket.gethostbyaddr(client_ip)` inside an `async def` middleware. When upstream DNS servers slowed down under load, each reverse DNS lookup blocked the single OS thread running the event loop for 2 to 5 seconds. All other concurrent HTTP connections were completely stalled in the kernel TCP backlog.

**Resolution**:
1. Replaced the synchronous DNS lookup with an async non-blocking resolver (`aiodns`).
2. Wrapped legacy synchronous metrics dispatchers with `anyio.to_thread.run_sync`.
3. Added `asyncio-event-loop-lag` metrics in Prometheus to alert whenever event loop tick lag exceeds 50 milliseconds.

---

## 11. Technical Interview Questions & Answers

<details>
<summary>1. What is the fundamental difference between a Coroutine, a Future, and a Task in Python?</summary>
A <b>Coroutine</b> is a suspended generator-like state machine created by evaluating an <code>async def</code> function; it does nothing until driven by an await or loop tick. A <b>Future</b> is a low-level object representing an eventual asynchronous result, containing callbacks and state (PENDING, FINISHED, CANCELLED). A <b>Task</b> is a concrete subclass of Future that wraps a coroutine and actively schedules its execution onto the event loop's ready queue via <code>task._step()</code>.
</details>

<details>
<summary>2. Why does calling <code>requests.get()</code> inside an <code>async def</code> route degrade performance for all users?</summary>
In an <code>async def</code> route, FastAPI executes the function directly on the main event loop thread. <code>requests.get()</code> is a synchronous, blocking network call that does not yield control via <code>await</code>. The OS thread freezes waiting for the HTTP response, halting the event loop. No other tasks or incoming requests can make progress until the blocking call returns.
</details>

<details>
<summary>3. How does Python 3.11's <code>asyncio.TaskGroup</code> improve over <code>asyncio.gather()</code>?</summary>
<code>TaskGroup</code> implements structured concurrency. If any child task in a <code>TaskGroup</code> raises an exception, the remaining active tasks in the group are automatically cancelled, and exceptions are collected into an <code>ExceptionGroup</code>. In contrast, <code>asyncio.gather(return_exceptions=False)</code> raises on the first error but leaves orphan child tasks continuing to run unmonitored in the background.
</details>

<details>
<summary>4. What happens if a client disconnects while an async FastAPI route is awaiting a database query?</summary>
Starlette / Uvicorn detects the TCP disconnect and cancels the root ASGI task handling that request by injecting an <code>asyncio.CancelledError</code> into the coroutine frame at its next suspension point. If properly written, the coroutine unwinds, releases its database connection, and cleans up context managers.
</details>

<details>
<summary>5. Explain the purpose of <code>uvloop</code> and how it integrates with FastAPI.</summary>
<code>uvloop</code> is an ultra-fast, C-based drop-in replacement for the standard library's <code>asyncio</code> event loop, built on top of <code>libuv</code>. It interfaces directly with OS system calls (<code>epoll</code> on Linux, <code>kqueue</code> on macOS) with minimal Python interpreter overhead, doubling or tripling FastAPI request throughput under heavy I/O workloads.
</details>

<details>
<summary>6. How can you safely run a CPU-intensive task without blocking the event loop in FastAPI?</summary>
CPU-bound work must be offloaded from the event loop thread. For moderate CPU tasks, use <code>anyio.to_thread.run_sync(sync_func, *args)</code> to run it in a worker thread. For heavy multi-core CPU tasks that would saturate Python's GIL, use <code>asyncio.get_running_loop().run_in_executor(process_pool_executor, sync_func, *args)</code>.
</details>

<details>
<summary>7. What is a "fire-and-forget" task bug in Python 3.12+, and how do you prevent it?</summary>
If you execute <code>asyncio.create_task(some_coro())</code> without assigning the returned Task to a persistent variable or data structure, Python's garbage collector can collect and destroy the Task while it is suspended waiting on an I/O event. The task will silently terminate without completing. To prevent this, retain strong references in a set: <code>background_tasks.add(task); task.add_done_callback(background_tasks.discard)</code>.
</details>

<details>
<summary>8. How does Python 3.13's free-threaded mode (PEP 703) impact <code>asyncio</code> and FastAPI?</summary>
In free-threaded Python (no GIL), multiple OS threads can execute Python bytecode truly in parallel. Each worker thread can run its own independent event loop handling separate sockets, or a shared threadpool can process CPU workloads concurrently without process IPC overhead, drastically simplifying concurrent microservice architectures.
</details>

---

## 12. Practical Exercises & Solutions

### Exercise 1 (Easy): Event Loop Timer Measurement
**Task**: Write an async route `/measure-latency` that measures the exact execution overhead (loop tick scheduling jitter) of `asyncio.sleep(0.05)`.
<details>
<summary>Solution</summary>

```python
import asyncio
from fastapi import FastAPI

app = FastAPI()

@app.get("/measure-latency")
async def measure_latency() -> dict[str, float]:
    loop = asyncio.get_running_loop()
    start = loop.time()
    expected_duration = 0.05
    await asyncio.sleep(expected_duration)
    actual_duration = loop.time() - start
    jitter_ms = (actual_duration - expected_duration) * 1000.0
    return {
        "expected_seconds": expected_duration,
        "actual_seconds": round(actual_duration, 6),
        "jitter_ms": round(jitter_ms, 3)
    }
```
</details>

### Exercise 2 (Easy): Safe Coroutine Execution Wrapper
**Task**: Implement an async helper `safe_await(coro, default_value)` that awaits a coroutine and returns `default_value` if any `Exception` occurs, without crashing.
<details>
<summary>Solution</summary>

```python
from typing import TypeVar, Coroutine, Any
import logging

T = TypeVar("T")

async def safe_await(coro: Coroutine[Any, Any, T], default_value: T) -> T:
    try:
        return await coro
    except Exception as exc:
        logging.warning("safe_await intercepted error: %s", exc)
        return default_value
```
</details>

### Exercise 3 (Medium): Resilient Multi-Source Data Aggregator with TaskGroup
**Task**: Build an endpoint `/multi-stats` that queries three simulated services (`stats_a`, `stats_b`, `stats_c`) concurrently. If any single service fails, return data from the remaining services with an error notation for the failed one.
<details>
<summary>Solution</summary>

```python
import asyncio
from typing import Any
from fastapi import FastAPI

app = FastAPI()

async def fetch_service_a() -> dict[str, Any]:
    await asyncio.sleep(0.02)
    return {"status": "ok", "users": 1500}

async def fetch_service_b() -> dict[str, Any]:
    await asyncio.sleep(0.03)
    raise RuntimeError("Service B connection refused")

async def fetch_service_c() -> dict[str, Any]:
    await asyncio.sleep(0.01)
    return {"status": "ok", "orders": 420}

async def isolate_service(coro: Any) -> Any:
    try:
        return await coro
    except Exception as exc:
        return {"status": "failed", "error": str(exc)}

@app.get("/multi-stats")
async def multi_stats() -> dict[str, Any]:
    async with asyncio.TaskGroup() as tg:
        t_a = tg.create_task(isolate_service(fetch_service_a()))
        t_b = tg.create_task(isolate_service(fetch_service_b()))
        t_c = tg.create_task(isolate_service(fetch_service_c()))
        
    return {
        "service_a": t_a.result(),
        "service_b": t_b.result(),
        "service_c": t_c.result()
    }
```
</details>

### Exercise 4 (Medium): Bounded Concurrency Semaphore Gate
**Task**: Create an async endpoint `/download-batch` that processes an array of 20 URLs concurrently, but limits concurrent network operations to at most 4 simultaneous requests using `asyncio.Semaphore`.
<details>
<summary>Solution</summary>

```python
import asyncio
from fastapi import FastAPI, Query

app = FastAPI()

async def simulate_download(sem: asyncio.Semaphore, item_id: int) -> dict[str, int]:
    async with sem:
        # Maximum 4 tasks can execute this section concurrently
        await asyncio.sleep(0.05)
        return {"id": item_id, "size_bytes": 1024 * (item_id + 1)}

@app.get("/download-batch")
async def download_batch(count: int = Query(default=20, ge=1, le=50)) -> dict[str, Any]:
    semaphore = asyncio.Semaphore(4)
    async with asyncio.TaskGroup() as tg:
        tasks = [
            tg.create_task(simulate_download(semaphore, i))
            for i in range(count)
        ]
    return {"results": [t.result() for t in tasks]}
```
</details>

### Exercise 5 (Hard): Non-Garbage-Collected Background Task Registry
**Task**: Build a robust, production-grade background task registry in FastAPI that prevents premature garbage collection of unawaited tasks, logs task completion or failure, and gracefully awaits remaining tasks during application shutdown.
<details>
<summary>Solution</summary>

```python
import asyncio
import logging
from contextlib import asynccontextmanager
from typing import AsyncIterator, Coroutine, Any
from fastapi import FastAPI

logger = logging.getLogger("task_runner")

class TaskRegistry:
    def __init__(self) -> None:
        self._active_tasks: set[asyncio.Task[Any]] = set()

    def spawn(self, coro: Coroutine[Any, Any, Any], name: str | None = None) -> asyncio.Task[Any]:
        task = asyncio.create_task(coro, name=name)
        self._active_tasks.add(task)

        def _cleanup(t: asyncio.Task[Any]) -> None:
            self._active_tasks.discard(t)
            try:
                if not t.cancelled() and t.exception():
                    logger.error("Background task '%s' failed: %s", t.get_name(), t.exception())
                else:
                    logger.info("Background task '%s' completed successfully", t.get_name())
            except Exception as ex:
                logger.error("Error inspecting task: %s", ex)

        task.add_done_callback(_cleanup)
        return task

    async def drain_all(self, timeout: float = 5.0) -> None:
        if not self._active_tasks:
            return
        logger.info("Draining %d background tasks...", len(self._active_tasks))
        try:
            async with asyncio.timeout(timeout):
                await asyncio.gather(*list(self._active_tasks), return_exceptions=True)
        except TimeoutError:
            logger.warning("Drain timeout reached. Cancelling remaining tasks.")
            for task in self._active_tasks:
                task.cancel()

registry = TaskRegistry()

@asynccontextmanager
async def lifespan(app: FastAPI) -> AsyncIterator[None]:
    yield
    await registry.drain_all(timeout=3.0)

app = FastAPI(lifespan=lifespan)

async def audit_worker(user_id: str, action: str) -> None:
    await asyncio.sleep(0.1)
    logger.info("Audited user=%s action=%s", user_id, action)

@app.post("/log-action")
async def log_action(user_id: str, action: str) -> dict[str, str]:
    registry.spawn(audit_worker(user_id, action), name=f"audit-{user_id}")
    return {"status": "enqueued"}
```
</details>

### Exercise 6 (Hard): Event Loop Health Monitor & Latency Lag Detector
**Task**: Implement a dedicated background coroutine started at application startup that ticks every 50ms. If the event loop is blocked and tick lag exceeds 100ms, increment an exposed anomaly metric.
<details>
<summary>Solution</summary>

```python
import asyncio
import logging
from contextlib import asynccontextmanager
from typing import AsyncIterator
from fastapi import FastAPI

logger = logging.getLogger("loop_monitor")

class LoopHealthTracker:
    def __init__(self) -> None:
        self.lag_spikes_detected: int = 0
        self.max_lag_recorded_ms: float = 0.0
        self._stop_event = asyncio.Event()

    async def run_monitor(self, target_interval: float = 0.05, threshold_ms: float = 50.0) -> None:
        loop = asyncio.get_running_loop()
        last_time = loop.time()
        
        while not self._stop_event.is_set():
            await asyncio.sleep(target_interval)
            current_time = loop.time()
            elapsed = current_time - last_time
            lag_ms = max(0.0, (elapsed - target_interval) * 1000.0)
            
            if lag_ms > self.max_lag_recorded_ms:
                self.max_lag_recorded_ms = lag_ms
                
            if lag_ms > threshold_ms:
                self.lag_spikes_detected += 1
                logger.warning("Event Loop Lag Detected! Delayed by %.2f ms", lag_ms)
                
            last_time = current_time

    def stop(self) -> None:
        self._stop_event.set()

health_tracker = LoopHealthTracker()

@asynccontextmanager
async def lifespan(app: FastAPI) -> AsyncIterator[None]:
    monitor_task = asyncio.create_task(health_tracker.run_monitor())
    yield
    health_tracker.stop()
    await monitor_task

app = FastAPI(lifespan=lifespan)

@app.get("/metrics/loop-health")
async def loop_health() -> dict[str, float | int]:
    return {
        "lag_spikes_detected": health_tracker.lag_spikes_detected,
        "max_lag_recorded_ms": round(health_tracker.max_lag_recorded_ms, 2)
    }
```
</details>
