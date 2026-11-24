---
title: "Async Context Managers and Generators in FastAPI"
category: "09-async-and-concurrency"
order: 4
tags: ["fastapi", "async-context-manager", "async-generator", "lifespan", "streaming", "resource-management"]
py_version: "3.12+"
fastapi_version: "0.115+"
starlette_version: "0.38+"
pydantic_version: "2.8+"
tested_with: "Python 3.12, 3.13"
---

# Async Context Managers and Generators in FastAPI

## 1. Conceptual Foundation & Mental Model

Modern web applications must safely acquire, manage, and release finite external resources: database connections, HTTP client sessions, Redis pipelines, file handles, and distributed locks. In synchronous Python, the `with` statement and generators (`yield`) handle deterministic resource acquisition and teardown.

In asynchronous FastAPI applications, standard `with` statements and generators are insufficient because opening or closing a network resource requires non-blocking network I/O (`await`). Python provides:
1. **Async Context Managers (`async with`)**: Defined via `__aenter__()` and `__aexit__()` methods (or `@contextlib.asynccontextmanager`). They guarantee deterministic acquisition and teardown across cooperative asynchronous boundaries, even when tasks are cancelled or fail.
2. **Async Generators (`async for`, `yield`)**: Defined by declaring `yield` inside an `async def` function. They implement `__aiter__()` and `__anext__()`, allowing memory-efficient streaming of items over time without buffering entire datasets into RAM.

### The Mental Model: The Laboratory Air Handler and Conveyor Belt
- An **Async Context Manager** is like entering a sterile biosafety cleanroom. You swipe your card and wait for the airlock doors to seal (`await __aenter__()`). You perform your experiment. No matter what occurs—even if a biohazard alarm triggers an emergency evacuation (`asyncio.CancelledError`)—the airlock's decontamination shower and safety seals run deterministically before the doors unlock (`await __aexit__()`).
- An **Async Generator** is a conveyor belt carrying raw sensor samples. Instead of waiting for 1,000,000 samples to be loaded into a massive warehouse before inspecting them, each sample is picked off the belt the instant it arrives (`async for item in stream:`), keeping memory footprint constant at $O(1)$.

---

## 2. Architecture & Internal Mechanics

```
               `async with manager:` Lifecycle
                             |
                             v
                 Calls `await __aenter__()`
                 (Connect to Postgres / Redis)
                             |
                             +-------------------+
                             |                   |
                       Normal Flow        Exception Raised / Cancelled
                             |                   |
                             v                   v
                    Execute with Block      Capture Traceback
                             |                   |
                             +---------+---------+
                                       |
                                       v
                           Calls `await __aexit__(*exc_info)`
                           (Rollback/Commit, Release Socket)
                                       |
                                       v
                              Execution Continues
```

### Dependency Injection with `yield` in FastAPI
FastAPI maps dependency functions containing `yield` into asynchronous context managers:
1. When a request begins, FastAPI executes code up to `yield`.
2. The yielded value is injected into the route endpoint.
3. When the route completes (or raises an HTTPException), FastAPI executes code after `yield`.
4. If an unhandled exception occurs inside the endpoint, FastAPI re-raises it inside the generator at the `yield` statement, enabling clean `try / except / finally` rollbacks.

---

## 3. Comprehensive Syntax & Core Mechanics

### Custom Async Context Manager Class
```python
from typing import Any

class AsyncDatabasePool:
    def __init__(self, dsn: str):
        self.dsn = dsn
        self.connection = None

    async def __aenter__(self) -> "AsyncDatabasePool":
        # Non-blocking async connection setup
        print("Acquiring connection asynchronously...")
        self.connection = f"conn_to_{self.dsn}"
        return self

    async def __aexit__(self, exc_type: Any, exc_val: Any, exc_tb: Any) -> bool | None:
        # Non-blocking cleanup and teardown
        print("Closing connection asynchronously...")
        self.connection = None
        # Return True to suppress exception, False/None to propagate
        return None
```

### Using `@contextlib.asynccontextmanager`
```python
from contextlib import asynccontextmanager
from typing import AsyncIterator

@asynccontextmanager
async def temporary_cache_lock(lock_key: str) -> AsyncIterator[str]:
    print(f"Acquired distributed lock: {lock_key}")
    try:
        yield f"token_for_{lock_key}"
    finally:
        print(f"Released distributed lock: {lock_key}")
```

### Async Generator Function
```python
from typing import AsyncGenerator
import asyncio

async def number_stream(limit: int) -> AsyncGenerator[int, None]:
    for i in range(limit):
        await asyncio.sleep(0.01)  # Simulate I/O latency
        yield i
```

---

## 4. Practical Implementation

### Example 1: Basic Lifespan Resource Manager
```python
from contextlib import asynccontextmanager
from typing import AsyncIterator
import httpx
from fastapi import FastAPI

@asynccontextmanager
async def lifespan(app: FastAPI) -> AsyncIterator[None]:
    # Startup: Initialize shared HTTP client
    app.state.http_client = httpx.AsyncClient(timeout=5.0)
    print("Application Startup: HTTP Client Pool Created")
    
    yield  # Application runs and handles requests here
    
    # Shutdown: Deterministic resource teardown
    await app.state.http_client.aclose()
    print("Application Shutdown: HTTP Client Pool Closed")

app = FastAPI(lifespan=lifespan)

@app.get("/external-ip")
async def get_external_ip():
    client: httpx.AsyncClient = app.state.http_client
    resp = await client.get("https://httpbin.org/ip")
    return resp.json()
```

### Example 2: Production Async Database Dependency with Transaction Rollback
```python
from typing import AsyncIterator
import asyncio
from fastapi import FastAPI, Depends, HTTPException, status

class MockAsyncTransaction:
    async def commit(self):
        print("DB: Transaction Committed")
        
    async def rollback(self):
        print("DB: Transaction Rolled Back")
        
    async def close(self):
        print("DB: Connection Returned to Pool")

async def get_db_session() -> AsyncIterator[MockAsyncTransaction]:
    session = MockAsyncTransaction()
    try:
        yield session
        await session.commit()
    except Exception as exc:
        await session.rollback()
        raise exc
    finally:
        await session.close()

app = FastAPI()

@app.post("/users/create")
async def create_user(username: str, db: MockAsyncTransaction = Depends(get_db_session)):
    if username == "forbidden":
        # Raising HTTPException triggers rollback in generator exception block!
        raise HTTPException(status_code=400, detail="Disallowed username")
    return {"status": "created", "user": username}
```

### Example 3: Edge Case — Streaming Large Datasets via SSE
```python
import asyncio
import json
from typing import AsyncGenerator
from fastapi import FastAPI
from fastapi.responses import StreamingResponse

app = FastAPI()

async def server_sent_event_generator(total_ticks: int) -> AsyncGenerator[str, None]:
    try:
        for tick in range(1, total_ticks + 1):
            await asyncio.sleep(0.5)
            payload = {"event": "heartbeat", "tick": tick, "active": True}
            yield f"data: {json.dumps(payload)}\n\n"
    except asyncio.CancelledError:
        # Triggered when client abruptly closes browser or disconnects
        print("Client disconnected from SSE stream. Cleaning up resources...")
        raise

@app.get("/events/stream")
async def stream_events():
    return StreamingResponse(
        server_sent_event_generator(10),
        media_type="text/event-stream"
    )
```

### Example 4: Failure Case — Leaking Generator Cleanup on Unhandled Route Crashes
```python
from fastapi import FastAPI, Depends

app = FastAPI()

# ANTI-PATTERN: Missing try/finally block around yield
async def leaky_dependency():
    resource = {"status": "allocated"}
    yield resource
    # IF THE ROUTE CRASHES OR CLIENT DISCONNECTS, THIS CODE IS SKIPPED!
    resource["status"] = "freed"

@app.get("/leaky")
async def leaky_endpoint(res = Depends(leaky_dependency)):
    raise RuntimeError("Unexpected internal crash")
```

### Example 5: Idiomatic Modern — Safe Async Generator Context Manager
```python
import asyncio
from contextlib import asynccontextmanager
from typing import AsyncIterator
from fastapi import FastAPI, Depends

class AsyncRedisLock:
    def __init__(self, key: str):
        self.key = key

    async def acquire(self) -> bool:
        await asyncio.sleep(0.01)
        return True

    async def release(self) -> None:
        await asyncio.sleep(0.01)

@asynccontextmanager
async def distributed_lock(key: str) -> AsyncIterator[None]:
    lock = AsyncRedisLock(key)
    await lock.acquire()
    try:
        yield
    finally:
        await lock.release()

app = FastAPI()

async def get_payment_lock() -> AsyncIterator[None]:
    async with distributed_lock("payment_mutex"):
        yield

@app.post("/checkout", dependencies=[Depends(get_payment_lock)])
async def checkout():
    return {"status": "paid"}
```

---

## 5. Edge Cases, Pitfalls & Failure Modes

1. **Forgetting `finally` in Dependency Generators**: In FastAPI dependencies using `yield`, any code after `yield` executes after the response is sent. If the endpoint raises an exception, the generator receives that exception. If the generator does not wrap `yield` in `try / finally`, the cleanup logic is aborted, leaking memory or connection pool handles.
2. **Client Abort During Streaming**: When a client terminates a connection during a `StreamingResponse`, the generator receives an `asyncio.CancelledError`. If the generator catches `Exception` instead of `BaseException`, or fails to re-raise `CancelledError`, internal task states become desynchronized.
3. **Consuming an Async Generator Twice**: Async generators cannot be rewound or re-iterated. Calling `[x async for x in gen]` consumes the stream. A second iteration yields zero elements.
4. **Nested Exception Swallowing**: Returning `True` from `__aexit__` swallows exceptions. In FastAPI dependencies, if an `__aexit__` suppresses an `HTTPException`, FastAPI will treat the endpoint response as successful (HTTP 200) with a `None` body.

---

## 6. Common Mistakes & Anti-Patterns

| Anti-Pattern | Root Cause | Impact | Correct Practice |
|:---|:---|:---|:---|
| Dependency `yield` without `try...finally` | Assuming endpoints always succeed | Resource leak on any endpoint exception | Always wrap `yield` in `try...finally` |
| Calling sync `with open()` in async routes | Forgetting disk I/O blocks the loop | Loop starvation on heavy disk access | Use `anyio.to_thread.run_sync` or `aiofiles` |
| Swallowing `CancelledError` in async generators | Writing `except BaseException: pass` | Leaks background tasks on client disconnect | Always re-raise `asyncio.CancelledError` |
| Using `@contextmanager` instead of `@asynccontextmanager` | Using sync context decorator for async code | `TypeError: object is not an async context manager` | Use `@asynccontextmanager` from `contextlib` |
| Initializing shared pools in module scope | No event loop exists during import | Loop attachment errors across worker processes | Initialize in `lifespan` context manager |
| Loading whole generator into list in memory | Converting stream with `list()` | $O(N)$ RAM exhaustion defeats streaming purpose | Stream chunk by chunk directly to client |

---

## 7. Performance & Scalability Considerations

- **Memory Efficiency**: Streaming 10 GB files using async generators requires less than 64 KB of buffer memory, compared to 10 GB of RAM if read into memory at once.
- **Connection Pool Lifespan**: Initializing database pools once in `lifespan` eliminates the 20ms to 50ms TLS handshake and authentication overhead per request.
- **Generator Suspension Overhead**: Async generator suspension (`yield`) has minimal overhead (~100 nanoseconds per yield), enabling micro-batching without performance degradation.

---

## 8. Security & OWASP Guardrails

- **Unreleased Distributed Locks**: If an async lock context manager crashes before `__aexit__` without a TTL, resources become permanently deadlocked (Denial of Service). Always configure TTLs on distributed Redis locks.
- **Slowloris Stream Starvation**: Attackers opening SSE streams and reading at 1 byte per minute can exhaust server connection limits. Enforce client read timeouts and idle disconnect policies.

---

## 9. Testing Strategies & Pytest Recipes

```python
import pytest
from httpx import AsyncClient, ASGITransport
from fastapi import FastAPI, Depends

app = FastAPI()
cleanup_tracker = {"cleaned": False}

async def monitored_dependency():
    try:
        yield "active_resource"
    finally:
        cleanup_tracker["cleaned"] = True

@app.get("/test-cleanup")
async def test_cleanup(res: str = Depends(monitored_dependency)):
    return {"resource": res}

@pytest.mark.anyio
async def test_dependency_generator_cleanup_executes():
    cleanup_tracker["cleaned"] = False
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        resp = await client.get("/test-cleanup")
        assert resp.status_code == 200
        assert resp.json() == {"resource": "active_resource"}
        
    # Verify cleanup ran after response
    assert cleanup_tracker["cleaned"] is True
```

---

## 10. Real-World Case Study / Production Incident

**The Incident**: A high-traffic social network backend ran out of PostgreSQL connections within 15 minutes of deployment. Postgres reported 500 active idle-in-transaction connections, rejecting all new queries.

**Root Cause**: The team refactored their database dependency to use `yield`. However, they caught generic `Exception` inside the route, logged it, and did not re-raise. Because no exception propagated out of the route, the dependency's `session.commit()` ran, but when a validation error caused an early return before the route finished, `session.close()` was never invoked because it was written after a conditional return instead of inside a `finally` block.

**The Fix**:
Refactored the database session dependency to strictly enforce `finally`:
```python
async def get_db():
    session = AsyncSessionLocal()
    try:
        yield session
    finally:
        await session.close()
```
Connection count stabilized at 35 connections permanently.

---

## 11. Technical Interview Questions & Answers

<details>
<summary>1. How does FastAPI implement dependencies that contain a <code>yield</code> statement?</summary>
FastAPI transforms functions with <code>yield</code> into context managers. Code before <code>yield</code> is executed during dependency resolution. The yielded value is injected into the route. Code after <code>yield</code> is guaranteed to execute after the response has been sent to the client (or when an exception occurs).
</details>

<details>
<summary>2. What happens if an exception is raised inside a route that uses a <code>yield</code> dependency?</summary>
FastAPI catches the exception and passes it into the dependency generator at the point of the <code>yield</code> statement using <code>generator.athrow(exc)</code>. If the generator wraps <code>yield</code> in a <code>try / except</code> or <code>try / finally</code> block, it can catch the error, perform rollbacks, and clean up resources.
</details>

<details>
<summary>3. Why is the <code>lifespan</code> parameter preferred over legacy <code>@app.on_event("startup")</code>?</summary>
The modern <code>lifespan</code> context manager provides unified, deterministic setup and teardown within a single scope. Shared resources can be initialized before <code>yield</code> and cleanly closed in <code>finally</code>, avoiding global state leaks and facilitating simpler unit testing.
</details>

<details>
<summary>4. How does an async generator differ from a standard synchronous generator?</summary>
An async generator is defined with <code>async def</code> and contains <code>yield</code>. It implements the <code>__aiter__()</code> and <code>__anext__()</code> protocols, allowing <code>await</code> expressions inside its body. It is consumed using <code>async for item in gen:</code>.
</details>

<details>
<summary>5. What special exception is raised in an async generator when a client closes a StreamingResponse?</summary>
An <code>asyncio.CancelledError</code> is injected into the generator. The generator must re-raise this exception to maintain cooperative task cancellation invariants.
</details>

<details>
<summary>6. How can you suppress an exception inside an async context manager?</summary>
By returning <code>True</code> from the <code>__aexit__(self, exc_type, exc_val, exc_tb)</code> method. If <code>True</code> is returned, the exception is silenced and execution continues outside the context block.
</details>

<details>
<summary>7. What happens if you forget the <code>finally</code> block in a <code>yield</code> dependency?</summary>
If an exception occurs in the route or any downstream dependency, execution inside the generator terminates abruptly at the <code>yield</code> statement, skipping all subsequent lines and causing connection or memory leaks.
</details>

<details>
<summary>8. Can you use <code>async with</code> inside a standard synchronous <code>def</code> function?</summary>
No. <code>async with</code> requires an active event loop and can only be evaluated inside an <code>async def</code> coroutine function.
</details>

---

## 12. Practical Exercises & Solutions

### Exercise 1 (Easy): Monitored Execution Context Manager
**Task**: Write an async context manager `execution_timer(name)` that logs the elapsed time in milliseconds when the context exits.
<details>
<summary>Solution</summary>

```python
import asyncio
from contextlib import asynccontextmanager
from typing import AsyncIterator
import logging

logger = logging.getLogger("timer")

@asynccontextmanager
async def execution_timer(operation_name: str) -> AsyncIterator[None]:
    loop = asyncio.get_running_loop()
    start_time = loop.time()
    try:
        yield
    finally:
        elapsed_ms = (loop.time() - start_time) * 1000.0
        logger.info("Operation '%s' took %.2f ms", operation_name, elapsed_ms)
```
</details>

### Exercise 2 (Easy): Async Number Chunking Generator
**Task**: Create an async generator `chunked_stream(data_list, chunk_size)` that yields sub-lists of size `chunk_size` with a 10ms async sleep between chunks.
<details>
<summary>Solution</summary>

```python
import asyncio
from typing import AsyncGenerator, TypeVar

T = TypeVar("T")

async def chunked_stream(data: list[T], chunk_size: int) -> AsyncGenerator[list[T], None]:
    for i in range(0, len(data), chunk_size):
        await asyncio.sleep(0.01)
        yield data[i:i + chunk_size]
```
</details>

### Exercise 3 (Medium): Safe Multi-Step Lifespan Manager
**Task**: Create a FastAPI lifespan function that initializes both a fake Redis connection and an HTTP client. Ensure that if the Redis connection fails during startup, the HTTP client is cleanly closed.
<details>
<summary>Solution</summary>

```python
from contextlib import asynccontextmanager
from typing import AsyncIterator
import httpx
from fastapi import FastAPI

@asynccontextmanager
async def lifespan(app: FastAPI) -> AsyncIterator[None]:
    client = httpx.AsyncClient()
    app.state.client = client
    
    redis_connected = False
    try:
        # Simulate connection
        app.state.redis = "redis_connected"
        redis_connected = True
        yield
    finally:
        if redis_connected:
            print("Closing Redis...")
        await client.aclose()
        print("HTTP Client safely closed.")

app = FastAPI(lifespan=lifespan)
```
</details>

### Exercise 4 (Medium): Resilient CSV Streamer
**Task**: Create a FastAPI route `/export-csv` that streams 50,000 generated CSV rows using `StreamingResponse` and an async generator, maintaining memory usage below 5 MB.
<details>
<summary>Solution</summary>

```python
import asyncio
from typing import AsyncGenerator
from fastapi import FastAPI
from fastapi.responses import StreamingResponse

app = FastAPI()

async def generate_csv_rows(total: int) -> AsyncGenerator[str, None]:
    yield "id,username,email\n"
    for i in range(1, total + 1):
        if i % 1000 == 0:
            await asyncio.sleep(0)  # Yield control to event loop
        yield f"{i},user_{i},user_{i}@example.com\n"

@app.get("/export-csv")
async def export_csv():
    return StreamingResponse(
        generate_csv_rows(50_000),
        media_type="text/csv",
        headers={"Content-Disposition": "attachment; filename=export.csv"}
    )
```
</details>

### Exercise 5 (Hard): Distributed Lock Dependency with Auto-Renewal
**Task**: Implement a FastAPI dependency `get_distributed_lock` using an async context manager that spawns a background task to periodically renew the lock TTL while the route is executing, cancelling the renewal task on exit.
<details>
<summary>Solution</summary>

```python
import asyncio
from typing import AsyncIterator
from fastapi import FastAPI, Depends

class DistributedLock:
    def __init__(self, key: str):
        self.key = key
        self._renewal_task: asyncio.Task[None] | None = None

    async def _renew_loop(self):
        try:
            while True:
                await asyncio.sleep(1.0)
                print(f"Renewed lock for {self.key}")
        except asyncio.CancelledError:
            print(f"Renewal cancelled for {self.key}")
            raise

    async def __aenter__(self):
        print(f"Acquired lock: {self.key}")
        self._renewal_task = asyncio.create_task(self._renew_loop())
        return self

    async def __aexit__(self, exc_type, exc_val, exc_tb):
        if self._renewal_task:
            self._renewal_task.cancel()
            try:
                await self._renewal_task
            except asyncio.CancelledError:
                pass
        print(f"Released lock: {self.key}")

async def lock_dep() -> AsyncIterator[None]:
    async with DistributedLock("account_123"):
        yield

app = FastAPI()

@app.post("/transfer", dependencies=[Depends(lock_dep)])
async def transfer():
    await asyncio.sleep(2.5)  # Triggers 2 renewals
    return {"status": "transferred"}
```
</details>

### Exercise 6 (Hard): Async Context Manager Pipeline
**Task**: Build a generic async pipeline context manager `compose_async_contexts(*context_managers)` that enters an arbitrary list of async context managers in order, and exits them in reverse order, ensuring all exits run even if an enter fails.
<details>
<summary>Solution</summary>

```python
from contextlib import AsyncExitStack, asynccontextmanager
from typing import AsyncIterator, Any
from fastapi import FastAPI

@asynccontextmanager
async def ctx_a() -> AsyncIterator[str]:
    print("Enter A")
    yield "A"
    print("Exit A")

@asynccontextmanager
async def ctx_b() -> AsyncIterator[str]:
    print("Enter B")
    yield "B"
    print("Exit B")

@asynccontextmanager
async def composed_stack() -> AsyncIterator[list[Any]]:
    async with AsyncExitStack() as stack:
        res_a = await stack.enter_async_context(ctx_a())
        res_b = await stack.enter_async_context(ctx_b())
        yield [res_a, res_b]

app = FastAPI()

@app.get("/pipeline")
async def pipeline():
    async with composed_stack() as values:
        return {"values": values}
```
</details>
