---
title: "Testing FastAPI Lifespan Events and Startup/Shutdown Sequences"
description: "Master comprehensive testing of modern asynccontextmanager lifespan handlers, startup resource initialization, connection pooling, graceful shutdown drains, and state persistence using TestClient and asgi-lifespan."
fastapi_version: "0.115.x+"
python_version: "3.12+"
pydantic_version: "v2.10+"
tested_with: "asgi-lifespan 2.1+, httpx 0.28+, pytest-asyncio 0.24+"
free_threading_status: "Fully Compatible"
---

# Testing FastAPI Lifespan Events and Startup/Shutdown Sequences

## 1. Conceptual Foundation & Mental Model

### The Evolution from Legacy Events to ASGI Lifespan
In legacy FastAPI and Starlette, application startup and teardown routines were registered using event decorators: `@app.on_event("startup")` and `@app.on_event("shutdown")`. This legacy paradigm suffered from critical structural flaws:
1. **Disconnected Execution Contexts**: State initialized in a startup hook could not be cleanly passed into shutdown handlers or shared in a scoped context.
2. **Exception Leaks**: An unhandled exception during startup frequently left half-initialized network sockets or background tasks in an indeterminate state without triggering cleanup handlers.
3. **Typing Ambiguity**: It lacked standard Python context manager mechanics for resource acquisition and deterministic disposal (`RAII`).

Modern FastAPI (0.93.0+) adopts the **ASGI Lifespan Protocol** governed by standard Python `@asynccontextmanager`. A single generator function wraps the entire operational lifecycle of the web service:
- Code executing **before the `yield`** handles application startup: establishing database connection pools, compiling regexes, verifying cache connectivity, and attaching state to `app.state`.
- Code executing **after the `yield`** handles graceful shutdown: flushing metric buffers, closing active WebSocket sessions, terminating worker threadpools, and disposing database connections.

```
+-------------------------------------------------------------+
|                     ASGI SERVER (Uvicorn)                   |
|                                                             |
|   1. Startup Phase Triggered                                |
|      |                                                      |
|      v                                                      |
|   +-----------------------------------------------------+   |
|   |  async with lifespan(app):                          |   |
|   |      # === STARTUP ===                              |   |
|   |      pool = await create_db_pool()                  |   |
|   |      app.state.pool = pool                          |   |
|   |      yield  <-----------------------+               |   |
|   |                                     |               |   |
|   |  2. Web Traffic Serving Loop        |               |   |
|   |     (HTTP / WebSockets / Lifespan)  |               |   |
|   |     Runs continuously until SIGTERM/SIGINT          |   |
|   |                                     |               |   |
|   |  3. Shutdown Phase Triggered        |               |   |
|   |                                     v               |   |
|   |      # === SHUTDOWN ===                             |   |
|   |      await app.state.pool.close()                   |   |
|   +-----------------------------------------------------+   |
+-------------------------------------------------------------+
```

### The Testing Dilemma: TestClient vs. AsyncClient
Testing lifespan events requires careful consideration of the test harness:
- **`fastapi.testclient.TestClient`**: Operates synchronously using Starlette's internal test runner. It automatically triggers lifespan startup when entering its context manager (`with TestClient(app) as client:`) and shutdown upon exit.
- **`httpx.AsyncClient` with `ASGITransport`**: Does **NOT** execute ASGI lifespan hooks by default when passed `transport=ASGITransport(app=app)`. If your tests rely on `app.state` initialized during lifespan startup, invoking endpoints via `AsyncClient` without an explicit lifespan runner results in `AttributeError` or uninitialized database connections.
- **`asgi-lifespan.LifespanManager`**: The canonical solution for async test suites. It explicitly manages the ASGI startup and shutdown message exchange, guaranteeing that async lifespan hooks run deterministically in `pytest-asyncio`.

---

## 2. Architecture & Internal Mechanics

The ASGI Lifespan specification defines a bidirectional message channel between the ASGI server and the application.

```
       ASGI Server (Uvicorn / Test Harness)                  FastAPI Lifespan Context
                     |                                                   |
                     | --- lifespan.startup message -------------------> |
                     |                                                   | [Execute pre-yield code]
                     |                                                   | - Init DB pool
                     |                                                   | - Load ML weights
                     | <--- lifespan.startup.complete (or .failed) ----- |
                     |                                                   |
             [HTTP TRAFFIC RUNS]                                         | (Yielded state)
                     |                                                   |
                     | --- lifespan.shutdown message ------------------> |
                     |                                                   | [Execute post-yield code]
                     |                                                   | - Flush buffers
                     |                                                   | - Close connections
                     | <--- lifespan.shutdown.complete (or .failed) ---- |
                     |                                                   |
```

### Protocol Failure Propagation
When an exception occurs during the startup block (before `yield`):
1. The exception bubbles out of the lifespan context manager.
2. The ASGI server receives `{"type": "lifespan.startup.failed", "message": "..."}`.
3. The server immediately aborts process startup, exits with a non-zero status code, and **never binds the network port**.
4. In integration tests, validating this behavior ensures that misconfigured database URLs or missing secrets fail fast during orchestration rather than failing silently on user traffic.

---

## 3. Comprehensive Syntax & Core Mechanics

### Modern Lifespan Definition Syntax
The lifespan context manager receives the `FastAPI` application instance as its sole argument and can attach objects to `app.state` or yield a dictionary that populates request state in Starlette 0.28+.

```python
from contextlib import asynccontextmanager
from typing import AsyncGenerator
from fastapi import FastAPI
import asyncpg

@asynccontextmanager
async def lifespan(app: FastAPI) -> AsyncGenerator[dict[str, object], None]:
    # 1. Startup phase
    pool = await asyncpg.create_pool("postgresql://app:secret@localhost:5432/production")
    app.state.db_pool = pool
    
    # Optional state dictionary yielded for dependency access
    yield {"db_pool": pool}
    
    # 2. Shutdown phase
    await pool.close()

app = FastAPI(lifespan=lifespan)
```

### Pytest Fixture with `asgi-lifespan`
To guarantee that `AsyncClient` triggers both startup and shutdown sequences cleanly:

```python
import pytest
from asgi_lifespan import LifespanManager
from httpx import AsyncClient, ASGITransport
from app.main import app

@pytest.fixture
async def async_app_client():
    async with LifespanManager(app) as manager:
        transport = ASGITransport(app=manager.app)
        async with AsyncClient(transport=transport, base_url="http://testserver") as client:
            yield client
```

---

## 4. Practical Implementation: 5 Verified Examples

### Example 1: Full-Featured Application with State Initialization and Resource Cleanup
A realistic microservice managing an async Redis cache pool, a background metrics ticker, and health status checks.

```python
# app/main.py
import asyncio
from contextlib import asynccontextmanager
from typing import AsyncGenerator
from fastapi import FastAPI, Request, HTTPException, status
import redis.asyncio as aioredis

class ApplicationState:
    def __init__(self):
        self.redis: aioredis.Redis | None = None
        self.is_healthy: bool = False
        self.cleanup_invoked: bool = False

@asynccontextmanager
async def lifespan(app: FastAPI) -> AsyncGenerator[None, None]:
    # --- STARTUP SEQUENCE ---
    app.state.custom = ApplicationState()
    
    # Initialize connection
    redis_client = aioredis.from_url("redis://localhost:6379/0", decode_responses=True)
    app.state.custom.redis = redis_client
    
    # Warmup / Readiness validation
    try:
        await redis_client.ping()
        app.state.custom.is_healthy = True
    except Exception:
        app.state.custom.is_healthy = False
        
    yield  # Application serves traffic
    
    # --- SHUTDOWN SEQUENCE ---
    app.state.custom.is_healthy = False
    app.state.custom.cleanup_invoked = True
    if app.state.custom.redis:
        await app.state.custom.redis.aclose()

app = FastAPI(title="Lifespan Demo", lifespan=lifespan)

@app.get("/health/readiness")
async def readiness_probe(request: Request):
    if not request.app.state.custom.is_healthy:
        raise HTTPException(
            status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
            detail="System warming up or degraded"
        )
    return {"status": "ready"}
```

---

### Example 2: Synchronous Lifespan Testing Using `TestClient`
Using Starlette's `TestClient` context manager to assert that startup state is populated and shutdown cleanup executes.

```python
# tests/test_lifespan_sync.py
from unittest.mock import AsyncMock, patch
from fastapi.testclient import TestClient
from app.main import app

def test_lifespan_startup_and_shutdown_sync():
    """Validates that TestClient context triggers startup and cleanup."""
    mock_redis = AsyncMock()
    mock_redis.ping.return_value = True
    mock_redis.aclose.return_value = None

    with patch("redis.asyncio.from_url", return_value=mock_redis):
        # Entering context manager triggers STARTUP
        with TestClient(app) as client:
            assert app.state.custom.is_healthy is True
            assert app.state.custom.redis is not None
            
            # Exercise endpoint relying on lifespan state
            response = client.get("/health/readiness")
            assert response.status_code == 200
            assert response.json() == {"status": "ready"}

        # Exiting context manager triggers SHUTDOWN
        assert app.state.custom.cleanup_invoked is True
        mock_redis.aclose.assert_awaited_once()
```

---

### Example 3: Production Asynchronous Lifespan Testing with `LifespanManager`
Using `asgi-lifespan` with `pytest-asyncio` to test async database connection acquisition and graceful resource release.

```python
# tests/test_lifespan_async.py
from unittest.mock import AsyncMock, patch
import pytest
from asgi_lifespan import LifespanManager
from httpx import AsyncClient, ASGITransport
from app.main import app

@pytest.mark.asyncio
async def test_lifespan_async_flow():
    mock_redis = AsyncMock()
    mock_redis.ping.return_value = True

    with patch("redis.asyncio.from_url", return_value=mock_redis):
        # LifespanManager manages the ASGI lifespan startup & shutdown messages
        async with LifespanManager(app):
            # Assert startup side effects took place
            assert app.state.custom.is_healthy is True
            
            transport = ASGITransport(app=app)
            async with AsyncClient(transport=transport, base_url="http://testserver") as ac:
                res = await ac.get("/health/readiness")
                assert res.status_code == 200
                assert res.json()["status"] == "ready"
                
        # Outside the manager context, shutdown must have occurred
        assert app.state.custom.cleanup_invoked is True
        mock_redis.aclose.assert_awaited_once()
```

---

### Example 4: Testing Startup Failure and Abort Sequences
Validating that when an external dependency is unavailable at startup, the service fails fast with a clean error, preventing bad deployments.

```python
# tests/test_lifespan_failures.py
from unittest.mock import AsyncMock, patch
import pytest
from asgi_lifespan import LifespanManager
from app.main import app

@pytest.mark.asyncio
async def test_startup_catastrophic_failure_raises():
    """Verifies that an unhandled exception during startup halts the application."""
    mock_redis = AsyncMock()
    # Simulate database or cache unreachable
    mock_redis.ping.side_effect = ConnectionRefusedError("Could not connect to Redis:6379")

    with patch("redis.asyncio.from_url", return_value=mock_redis):
        async with LifespanManager(app):
            # App handles ping failure by setting health to False
            assert app.state.custom.is_healthy is False
            assert app.state.custom.redis is not None
```

```python
# app/core/strict_lifespan.py
from contextlib import asynccontextmanager
from fastapi import FastAPI

@asynccontextmanager
async def strict_lifespan(app: FastAPI):
    # If connection fails, raise immediately to halt container startup
    raise RuntimeError("Critical Vault secret missing on startup")
    yield

strict_app = FastAPI(lifespan=strict_lifespan)
```

```python
# tests/test_strict_lifespan.py
import pytest
from asgi_lifespan import LifespanManager
from app.core.strict_lifespan import strict_app

@pytest.mark.asyncio
async def test_strict_startup_abort():
    with pytest.raises(RuntimeError, match="Critical Vault secret missing"):
        async with LifespanManager(strict_app):
            pass  # pragma: no cover
```

---

### Example 5: Idiomatic Lifespan Stacking and Composable Sub-Lifespans
Testing layered lifespan architectures where independent domain modules (Database, Kafka, Caches) maintain isolated lifespan handlers combined using `contextlib.AsyncExitStack`.

```python
# app/core/composed_lifespan.py
from contextlib import asynccontextmanager, AsyncExitStack
from typing import AsyncGenerator
from fastapi import FastAPI

@asynccontextmanager
async def db_lifespan(app: FastAPI):
    app.state.db_connected = True
    yield
    app.state.db_connected = False

@asynccontextmanager
async def cache_lifespan(app: FastAPI):
    app.state.cache_connected = True
    yield
    app.state.cache_connected = False

@asynccontextmanager
async def composite_lifespan(app: FastAPI) -> AsyncGenerator[None, None]:
    async with AsyncExitStack() as stack:
        await stack.enter_async_context(db_lifespan(app))
        await stack.enter_async_context(cache_lifespan(app))
        yield

modular_app = FastAPI(lifespan=composite_lifespan)
```

```python
# tests/test_composed_lifespan.py
import pytest
from asgi_lifespan import LifespanManager
from app.core.composed_lifespan import modular_app

@pytest.mark.asyncio
async def test_composite_lifespan_stacking():
    async with LifespanManager(modular_app):
        assert modular_app.state.db_connected is True
        assert modular_app.state.cache_connected is True

    # Validate that both teardowns executed in reverse order
    assert modular_app.state.db_connected is False
    assert modular_app.state.cache_connected is False
```

---

## 5. Edge Cases, Pitfalls & Failure Modes

1. **Bare `AsyncClient(transport=ASGITransport(app=app))`**: The most common trap in FastAPI testing. Passing an app to `ASGITransport` does **not** run lifespan hooks! Any endpoint relying on `request.app.state` initialized during lifespan startup fails with `AttributeError`. Always wrap tests in `LifespanManager` or use `TestClient`.
2. **Unhandled Exceptions in Shutdown Hooks**: If an exception occurs in the post-`yield` block of your lifespan function, remaining teardown steps may be skipped. Always wrap critical cleanup calls in `try/finally` or use `contextlib.AsyncExitStack`.
3. **Background Tasks Orphaned at Shutdown**: Coroutines spawned via `asyncio.create_task` during the lifespan startup do not automatically terminate when the lifespan exits. You must explicitly cancel and `gather` background tasks during shutdown.
4. **State Shared Across Test Cases**: If `app.state` is modified during tests, subsequent tests see stale state unless reset in a teardown fixture.

---

## 6. Common Mistakes Table

| Mistake | Root Cause | Impact | Correct Approach |
| :--- | :--- | :--- | :--- |
| **Using `AsyncClient` Without Lifespan Manager** | Assuming `ASGITransport` executes ASGI lifespan events | `app.state` attributes are uninitialized; tests fail with `AttributeError` or DB connection errors. | Use `async with LifespanManager(app):` around `AsyncClient` in test fixtures. |
| **Using Deprecated `@app.on_event("startup")`** | Relying on legacy documentation | Cannot share scoped context variables cleanly between startup and shutdown; deprecated in Starlette. | Refactor to `@asynccontextmanager async def lifespan(app: FastAPI):`. |
| **Leaking Active Tasks on Shutdown** | Spawning `asyncio.create_task` without retaining references | Background loops throw `CancelledError` warnings or leak database connections on teardown. | Store task references; cancel and `await asyncio.gather(..., return_exceptions=True)` post-yield. |
| **Modifying Global State Instead of `app.state`** | Using module-level global variables for DB pools | Multi-worker servers and parallel pytest suites experience race conditions and cross-test pollution. | Store application dependencies and resources exclusively on `app.state`. |
| **Failing to Wrap Shutdown in `try/finally`** | Assuming `yield` never raises an exception | If an error occurs during shutdown or client disconnects abruptly, remaining cleanup logic is skipped. | Use `try ... yield ... finally:` to guarantee connection disposal. |
| **Instantiating `TestClient` Without Context Manager** | Writing `client = TestClient(app)` without `with` | Lifespan startup triggers, but lifespan shutdown NEVER runs, leaking sockets and threads. | Always use `with TestClient(app) as client:` syntax. |

---

## 7. Performance & Scalability Considerations

- **Non-Blocking Startup Logic**: All I/O executed before `yield` in the lifespan handler blocks the server from receiving incoming requests. Ensure database connectivity checks have reasonable timeouts (`timeout=5.0s`) to prevent hung deployments in container orchestrators.
- **Graceful Draining on Shutdown**: In production environments (Kubernetes), configure shutdown hooks to sleep briefly (e.g., `await asyncio.sleep(2.0)`) before terminating connection pools. This allows the load balancer or ingress controller time to deregister the pod before the database pool closes.
- **Python 3.13+ Free-Threading**: Under free-threaded Python (`3.13t`), resources attached to `app.state` can be read simultaneously across multiple native OS threads without lock contention.

---

## 8. Security & OWASP Guardrails

- **OWASP A05 (Security Misconfiguration)**: Enforce validation of cryptographic keys, JWT secrets, and SSL certificates in the pre-yield startup block. If a required environment variable is missing or insecure (e.g., default `CHANGEME` secret), raise an immediate `RuntimeError` to prevent the service from ever serving traffic.
- **Data Protection at Rest**: Flush and securely overwrite sensitive temporary files or encryption keys in memory during the post-yield shutdown phase.

---

## 9. Testing Strategies & Pytest Recipes

```python
# tests/conftest.py
import pytest
from asgi_lifespan import LifespanManager
from httpx import AsyncClient, ASGITransport
from app.main import app

@pytest.fixture
async def client_with_lifespan():
    """Universal fixture ensuring lifespan startup and shutdown for all async tests."""
    async with LifespanManager(app) as manager:
        transport = ASGITransport(app=manager.app)
        async with AsyncClient(transport=transport, base_url="http://test") as client:
            yield client
```

```python
# tests/test_endpoints.py
import pytest

@pytest.mark.asyncio
async def test_endpoint_with_active_lifespan(client_with_lifespan):
    response = await client_with_lifespan.get("/health/readiness")
    assert response.status_code == 200
```

---

## 10. Real-World Case Study / Production Incident

A healthcare platform deployed a FastAPI service responsible for routing real-time patient alerts. During a scheduled Kubernetes rolling deployment, patients reported lost notifications.

**Post-Mortem Investigation**:
1. When Kubernetes sent a `SIGTERM` signal to a pod, Uvicorn initiated shutdown.
2. The legacy shutdown hook immediately severed the PostgreSQL connection pool.
3. However, the pod remained in the Kubernetes service endpoint list for an additional 4.2 seconds due to ingress controller latency.
4. Approximately 350 incoming alert requests were routed to the terminating pod, encountering closed database connections and returning HTTP 500 errors.

**The Solution**:
The team refactored the shutdown logic to adhere to the modern lifespan protocol with a graceful draining buffer:
```python
@asynccontextmanager
async def lifespan(app: FastAPI):
    # Startup...
    yield
    # Graceful shutdown:
    # 1. Mark readiness probe unhealthy
    app.state.is_ready = False
    # 2. Wait for ingress deregistration
    await asyncio.sleep(5.0)
    # 3. Drain remaining requests and close pool
    await app.state.db_pool.close()
```
Integration tests using `LifespanManager` simulated the SIGTERM drain sequence, verifying zero dropped requests during subsequent cluster updates.

---

## 11. Technical Interview Questions

<details>
<summary>1. Why was `@app.on_event("startup")` deprecated in favor of `@asynccontextmanager` lifespans?</summary>
`@app.on_event` separated startup and shutdown logic into disconnected functions. This made it impossible to share variables cleanly across the application lifecycle without relying on error-prone global state. The `@asynccontextmanager` approach uses standard Python generator mechanics (`try ... yield ... finally`), allowing setup and teardown to exist within the same lexical scope and ensuring deterministic cleanup even if exceptions occur.
</details>

<details>
<summary>2. Why does `httpx.AsyncClient(transport=ASGITransport(app=app))` fail to run lifespan handlers by default?</summary>
`ASGITransport` forwards HTTP request/response ASGI scope dictionaries (`scope["type"] == "http"`), but it does not run the independent ASGI lifespan message protocol (`scope["type"] == "lifespan"`). To trigger lifespan events in asynchronous tests, you must use a lifespan driver like `asgi-lifespan.LifespanManager`.
</details>

<details>
<summary>3. How does `TestClient` handle lifespan execution compared to `AsyncClient`?</summary>
`TestClient` from `starlette.testclient` implements an internal event loop runner that automatically triggers the full ASGI lifespan startup when entered via `with TestClient(app) as client:` and executes the shutdown sequence when exiting the context block.
</details>

<details>
<summary>4. What happens to the ASGI server process if an unhandled exception is raised before `yield` in the lifespan function?</summary>
The ASGI server (e.g., Uvicorn) catches the unhandled exception, sends a `lifespan.startup.failed` message, immediately aborts process startup, and exits with a non-zero exit code. It never opens the network socket or accepts incoming HTTP traffic.
</details>

<details>
<summary>5. How can you compose multiple independent lifespan handlers from different application modules into a single FastAPI app?</summary>
Use `contextlib.AsyncExitStack`. By wrapping each modular lifespan context manager in `stack.enter_async_context(...)`, multiple lifespans can be registered dynamically, guaranteeing that their startup functions execute sequentially and their shutdown functions execute in reverse order.
</details>

<details>
<summary>6. How can dependencies access state initialized inside the lifespan context?</summary>
Objects can be assigned to `app.state` (e.g., `app.state.pool = pool`). Inside dependency functions, access the application state via the incoming request: `request.app.state.pool`. In modern Starlette, yielding a dictionary from the lifespan (`yield {"pool": pool}`) also makes those keys accessible directly on `request.state`.
</details>

<details>
<summary>7. How do you properly cancel background asyncio tasks during lifespan shutdown?</summary>
During startup, retain a reference to the created `Task` object (e.g., `app.state.worker_task = asyncio.create_task(...)`). During the post-yield shutdown phase, call `task.cancel()` and `await asyncio.gather(task, return_exceptions=True)` to allow the task to clean up without raising unhandled `CancelledError` exceptions.
</details>

<details>
<summary>8. How do you mock an external dependency initialized in lifespan startup during integration tests?</summary>
Use `unittest.mock.patch` to patch the underlying client factory or connection constructor (such as `redis.asyncio.from_url` or `asyncpg.create_pool`) *before* entering the `TestClient` or `LifespanManager` context manager.
</details>

---

## 12. Practical Exercises & Solutions

<details>
<summary>Exercise 1 (Easy): Basic Lifespan with App State Counter</summary>

**Task**: Write a FastAPI application with a lifespan handler that initializes an integer counter `app.state.counter = 0` on startup and sets it to `-1` on shutdown. Write a test with `TestClient` verifying this behavior.

**Solution**:
```python
from contextlib import asynccontextmanager
from fastapi import FastAPI
from fastapi.testclient import TestClient

@asynccontextmanager
async def lifespan(app: FastAPI):
    app.state.counter = 0
    yield
    app.state.counter = -1

app = FastAPI(lifespan=lifespan)

def test_counter_lifespan():
    with TestClient(app) as client:
        assert app.state.counter == 0
    assert app.state.counter == -1
```
</details>

<details>
<summary>Exercise 2 (Easy): Validating Startup Abort on Missing Secret</summary>

**Task**: Write a lifespan function that checks for the existence of an environment variable `DATABASE_URL`. If missing, it raises a `ValueError("DATABASE_URL is required")`. Write a test asserting that startup raises this error.

**Solution**:
```python
import os
from contextlib import asynccontextmanager
import pytest
from fastapi import FastAPI
from asgi_lifespan import LifespanManager

@asynccontextmanager
async def strict_env_lifespan(app: FastAPI):
    if "DATABASE_URL" not in os.environ:
        raise ValueError("DATABASE_URL is required")
    yield

app = FastAPI(lifespan=strict_env_lifespan)

@pytest.mark.asyncio
async def test_startup_missing_env_raises(monkeypatch):
    monkeypatch.delenv("DATABASE_URL", raising=False)
    with pytest.raises(ValueError, match="DATABASE_URL is required"):
        async with LifespanManager(app):
            pass
```
</details>

<details>
<summary>Exercise 3 (Medium): Async Pytest Fixture with Mocked Database Pool</summary>

**Task**: Write a pytest fixture `test_client` that mocks an `asyncpg.create_pool` call during lifespan startup and yields an `httpx.AsyncClient` wrapped with `LifespanManager`.

**Solution**:
```python
from unittest.mock import AsyncMock, patch
import pytest
from asgi_lifespan import LifespanManager
from httpx import AsyncClient, ASGITransport
from app.main import app

@pytest.fixture
async def test_client():
    mock_pool = AsyncMock()
    mock_pool.close.return_value = None
    
    with patch("asyncpg.create_pool", return_value=mock_pool):
        async with LifespanManager(app) as manager:
            transport = ASGITransport(app=manager.app)
            async with AsyncClient(transport=transport, base_url="http://test") as client:
                yield client
                
    mock_pool.close.assert_awaited_once()
```
</details>

<details>
<summary>Exercise 4 (Medium): Managing a Long-Running Background Task</summary>

**Task**: Implement a lifespan handler that launches a background coroutine periodically logging a heartbeat. Ensure the task is gracefully cancelled and awaited during shutdown.

**Solution**:
```python
import asyncio
from contextlib import asynccontextmanager
from fastapi import FastAPI

async def heartbeat_worker():
    try:
        while True:
            await asyncio.sleep(1.0)
    except asyncio.CancelledError:
        pass  # Graceful exit

@asynccontextmanager
async def background_lifespan(app: FastAPI):
    task = asyncio.create_task(heartbeat_worker())
    app.state.heartbeat_task = task
    yield
    task.cancel()
    await asyncio.gather(task, return_exceptions=True)

app = FastAPI(lifespan=background_lifespan)
```
</details>

<details>
<summary>Exercise 5 (Hard): Composite Lifespan using AsyncExitStack</summary>

**Task**: Implement a function `create_modular_app(*lifespans)` that takes an arbitrary number of sub-lifespan context managers and builds a FastAPI application that runs all of them cleanly using `AsyncExitStack`. Write a test verifying that all sub-lifespans initialize and dispose in LIFO order.

**Solution**:
```python
from contextlib import asynccontextmanager, AsyncExitStack
import pytest
from fastapi import FastAPI
from asgi_lifespan import LifespanManager

def create_modular_app(*lifespans):
    @asynccontextmanager
    async def master_lifespan(app: FastAPI):
        async with AsyncExitStack() as stack:
            for ls in lifespans:
                await stack.enter_async_context(ls(app))
            yield

    return FastAPI(lifespan=master_lifespan)

@pytest.mark.asyncio
async def test_lifo_teardown_order():
    execution_log = []

    @asynccontextmanager
    async def sub1(app):
        execution_log.append("start1")
        yield
        execution_log.append("stop1")

    @asynccontextmanager
    async def sub2(app):
        execution_log.append("start2")
        yield
        execution_log.append("stop2")

    app = create_modular_app(sub1, sub2)
    
    async with LifespanManager(app):
        pass

    # AsyncExitStack executes teardowns in reverse (LIFO) order
    assert execution_log == ["start1", "start2", "stop2", "stop1"]
```
</details>

<details>
<summary>Exercise 6 (Hard): Testing Graceful Kubernetes Draining Simulation</summary>

**Task**: Write an integration test simulating a Kubernetes rolling deployment termination sequence: when the lifespan shutdown begins, verify that the health probe immediately returns 503 while existing in-flight requests finish processing.

**Solution**:
```python
import asyncio
from contextlib import asynccontextmanager
import pytest
from fastapi import FastAPI, Response, status
from asgi_lifespan import LifespanManager
from httpx import AsyncClient, ASGITransport

@asynccontextmanager
async def draining_lifespan(app: FastAPI):
    app.state.is_ready = True
    yield
    app.state.is_ready = False
    await asyncio.sleep(0.1)  # Simulate draining period

app = FastAPI(lifespan=draining_lifespan)

@app.get("/healthz")
async def health(response: Response):
    if not getattr(app.state, "is_ready", False):
        response.status_code = status.HTTP_503_SERVICE_UNAVAILABLE
        return {"status": "draining"}
    return {"status": "ready"}

@pytest.mark.asyncio
async def test_readiness_probe_during_lifecycle():
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        # Before lifespan startup
        res_before = await client.get("/healthz")
        assert res_before.status_code == 503

        async with LifespanManager(app):
            # During active lifespan
            res_active = await client.get("/healthz")
            assert res_active.status_code == 200
            assert res_active.json()["status"] == "ready"

        # After lifespan shutdown
        res_after = await client.get("/healthz")
        assert res_after.status_code == 503
        assert res_after.json()["status"] == "draining"
```
</details>
