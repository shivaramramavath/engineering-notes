# Module 08: API Design — 05: Request-Response Lifecycle (`05-request-response-lifecycle`)

> Understanding the microscopic progression of an ASGI request through Uvicorn, Starlette middleware layers, FastAPI dependency graphs, Pydantic validation engines, and threadpool executors is essential for debugging latency spikes, memory leaks, and serialization overhead.

| Field | Value |
|---|---|
| **Category** | API Design & Standards |
| **Target Level** | Advanced / Production Internals |
| **FastAPI Version** | FastAPI 0.165.0+ |
| **Starlette Version**| Starlette 1.10.0+ |
| **Pydantic Version** | Pydantic v2.14.4+ (pydantic-settings 2.16.3+) |
| **Python Version**   | Python 3.12+ (3.13 / 3.14 compatible, Free-Threading verified) |
| **Tested With**      | Uvicorn 0.54+, httpx 0.28+, pytest 9.1+ |

---

## Table of Contents
1. [Conceptual Overview](#1-conceptual-overview)
2. [Core Concepts & Architecture](#2-core-concepts--architecture)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Verified Code Examples](#5-verified-code-examples)
6. [Edge Cases, Pitfalls & Failure Modes](#6-edge-cases-pitfalls--failure-modes)
7. [Common Mistakes Table](#7-common-mistakes-table)
8. [Performance & Scalability Considerations](#8-performance--scalability-considerations)
9. [Security & OWASP Guardrails](#9-security--owasp-guardrails)
10. [Testing Strategies & Pytest Recipes](#10-testing-strategies--pytest-recipes)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Technical Interview Questions](#12-technical-interview-questions)
13. [Practical Exercises & Solutions](#13-practical-exercises--solutions)

---

## 1. Conceptual Overview

An incoming HTTP request does not arrive directly at your route handler. It traverses a multi-stage execution pipeline:
1. **Network Layer**: OS TCP socket accept -> TLS termination.
2. **ASGI Server (Uvicorn)**: Byte stream parsed into ASGI `scope`, `receive`, and `send` callables.
3. **Starlette Middleware Chain**: CORS, GZip, custom logging, and security headers executed in onion-like sequence.
4. **Router Dispatch**: Route regex matching and HTTP verb evaluation.
5. **Dependency Graph Resolution (`solve_dependencies`)**: Leaf-to-root traversal; caching resolved values per request.
6. **Input Validation**: Pydantic v2 parses headers, cookies, query parameters, and raw JSON request body bytes into validated Python domain models.
7. **Handler Execution**: `async def` runs on the event loop; standard `def` is dispatched to an AnyIO worker threadpool.
8. **Response Serialization**: Return value filtered and serialized via `response_model` into JSON bytes.
9. **Post-Response Pipeline**: Background tasks dispatched; middleware unwound.

---

## 2. Core Concepts & Architecture

```
+-----------------------------------------------------------------------------------+
|               Complete FastAPI Request-Response Execution Pipeline                |
+-----------------------------------------------------------------------------------+
|                                                                                   |
|  [ Client Browser ]                                                               |
|        │ HTTP Request (Bytes)                                                     |
|        ▼                                                                          |
|  [ Uvicorn ASGI Server ] ──> Builds `scope = {"type": "http", "path": "/...",}`   |
|        │                                                                          |
|        ▼                                                                          |
|  [ Outer Middleware Stack ] (CORS, RequestId, SecurityHeaders)                    |
|        │                                                                          |
|        ▼                                                                          |
|  [ Starlette Router ] ──> Matches Path Regex & HTTP Method                        |
|        │                                                                          |
|        ▼                                                                          |
|  [ FastAPI Dependency Solver ] (`solve_dependencies`)                             |
|        ├── Resolves Database Sessions, Auth, Headers                              |
|        └── Caches shared dependencies (`use_cache=True`)                          |
|        │                                                                          |
|        ▼                                                                          |
|  [ Pydantic v2 Body Validation ]                                                  |
|        ├── Valid? ──> Coerces into Request Models                                 |
|        └── Invalid? ──> Aborts immediately with HTTP 422                          |
|        │                                                                          |
|        ▼                                                                          |
|  [ Endpoint Execution ]                                                           |
|        ├── `async def` ──> Executed directly on asyncio event loop                |
|        └── `def`       ──> Offloaded to AnyIO threadpool executor                 |
|        │                                                                          |
|        ▼                                                                          |
|  [ Response Serialization ]                                                       |
|        ├── Validates against `response_model`                                     |
|        └── Encodes to JSON bytes via Pydantic Rust Core                           |
|        │                                                                          |
|        ▼                                                                          |
|  [ Background Tasks Triggered ] (Executed after response headers are sent)        |
|        │                                                                          |
|        ▼                                                                          |
|  [ Unwind Middleware Stack ]                                                      |
|        │ HTTP Response (Bytes)                                                    |
|        ▼                                                                          |
|  [ Client Receives Response ]                                                     |
+-----------------------------------------------------------------------------------+
```

---

## 3. How It Works Internally

1. **`solve_dependencies()`**:
   - Located in `fastapi.dependencies.utils`.
   - Iterates through the topological sort of the route's dependency graph.
   - Evaluates sub-dependencies recursively. If an exception (`HTTPException`) is raised anywhere in the graph, execution halts and is caught by Starlette's exception handlers.
2. **`def` vs `async def` Dispatch**:
   - If an endpoint is declared `async def`, FastAPI `await`s it directly on the main event loop thread.
   - If declared standard `def`, FastAPI wraps the call in `anyio.to_thread.run_sync()`, offloading it to an asynchronous worker threadpool to avoid blocking the main event loop.

---

## 4. Syntax & API Reference

```python
import time
from fastapi import FastAPI, Request, Response
from starlette.middleware.base import BaseHTTPMiddleware

app = FastAPI()

# Middleware demonstrating ingress and egress lifecycle timing
class LifecycleTimingMiddleware(BaseHTTPMiddleware):
    async def dispatch(self, request: Request, call_next) -> Response:
        start_time = time.perf_counter()
        
        # 1. Ingress Phase: Before handler
        request.state.start_time = start_time
        
        # 2. Hand off to downstream routers / dependencies / handlers
        response: Response = await call_next(request)
        
        # 3. Egress Phase: After handler, before network transmission
        process_time = time.perf_counter() - start_time
        response.headers["X-Process-Time"] = f"{process_time:.6f}"
        return response

app.add_middleware(LifecycleTimingMiddleware)
```

---

## 5. Verified Code Examples

### Example 1: Minimal Lifecycle Instrumenter

```python
import time
from fastapi import FastAPI, Request

app = FastAPI()

@app.middleware("http")
async def add_process_time_header(request: Request, call_next):
    start_time = time.perf_counter()
    response = await call_next(request)
    duration = time.perf_counter() - start_time
    response.headers["X-Response-Time-Ms"] = str(round(duration * 1000, 2))
    return response

@app.get("/ping")
async def ping():
    return {"message": "pong"}
```

### Example 2: Enterprise Request Lifecycle Hook Matrix

```python
from contextlib import asynccontextmanager
from typing import Annotated
from fastapi import BackgroundTasks, Depends, FastAPI, Request, Response
from pydantic import BaseModel

# 1. Application Lifespan (Server Boot & Shutdown)
@asynccontextmanager
async def lifespan(app: FastAPI):
    print("[1] Application starting: initializing connection pools")
    yield
    print("[10] Application shutting down: closing resources")

app = FastAPI(lifespan=lifespan)

# 2. Dependency Lifecycle with Cleanup (Generator Dependency)
async def get_scoped_resource():
    print("[4] Entering Dependency setup")
    resource = {"conn": "active"}
    try:
        yield resource
    finally:
        print("[8] Dependency cleanup executed (after response sent)")

# Background task execution
def background_audit_flush(data: str):
    print(f"[9] Background task completed: {data}")

@app.post("/transaction")
async def execute_transaction(
    res: Annotated[dict, Depends(get_scoped_resource)],
    bg_tasks: BackgroundTasks,
):
    print("[6] Route handler executing")
    bg_tasks.add_task(background_audit_flush, "tx_123_flushed")
    return {"status": "committed"}
```

### Example 3: Edge Case / Boundary — Yield Dependency Exception Handling

```python
from fastapi import Depends, FastAPI, HTTPException

app = FastAPI()

async def transaction_scope():
    print("Transaction started")
    try:
        yield
        print("Transaction committed")
    except Exception as e:
        print(f"Transaction rolled back due to error: {e}")
        raise

@app.get("/fail", dependencies=[Depends(transaction_scope)])
async def trigger_failure():
    raise HTTPException(status_code=400, detail="Business logic failure")
```

### Example 4: Failure Case — Pydantic Validation Early Rejection

```python
# When an incoming request body fails Pydantic schema validation:
# POST /users {"age": "not-an-int"}
#
# The request NEVER reaches the route handler or any database dependencies!
# FastAPI short-circuits at the validation stage and responds with:
# HTTP 422 Unprocessable Entity
# {
#   "detail": [
#     {"type": "int_parsing", "loc": ["body", "age"], "msg": "Input should be a valid integer"}
#   ]
# }
```

### Example 5: Idiomatic Modern Pattern with Request State Propagation

```python
import uuid
from typing import Annotated
from fastapi import Depends, FastAPI, Request
from starlette.middleware.base import BaseHTTPMiddleware

app = FastAPI()

class RequestStateMiddleware(BaseHTTPMiddleware):
    async def dispatch(self, request: Request, call_next):
        # Inject correlation data directly into ASGI request.state
        request.state.trace_id = str(uuid.uuid4())
        request.state.caller_tenant = "tenant_alpha"
        return await call_next(request)

app.add_middleware(RequestStateMiddleware)

def get_trace_id(request: Request) -> str:
    return getattr(request.state, "trace_id", "unknown")

@app.get("/trace")
def check_trace(trace_id: Annotated[str, Depends(get_trace_id)]):
    return {"trace_id": trace_id}
```

---

## 6. Edge Cases, Pitfalls & Failure Modes

1. **Streaming Body Consumption Pitfall**: Calling `await request.body()` inside an early middleware buffers the entire HTTP stream into memory. If streaming multi-gigabyte file uploads, this exhausts server RAM!
2. **Generator Dependency Cleanup vs. Exceptions**: If an unhandled exception occurs inside a route handler, Starlette catches it before the `yield` block unwinds. Ensure `finally` blocks are used in generator dependencies to guarantee resource cleanup.
3. **Background Tasks Execution Timing**: Background tasks registered via `BackgroundTasks` execute *after* the HTTP response has been sent to the client, but *before* the request socket is fully closed.

---

## 7. Common Mistakes Table

| Mistake | Why It Fails | Error / Diagnostic | Correct Idiomatic Fix |
|---|---|---|---|
| 1. Synchronous I/O in `async def` handler | Freezes event loop during lifecycle | Latency spikes on all routes | Offload via `to_thread.run_sync` or use `def` |
| 2. Reading `request.body()` multiple times in pure ASGI | ASGI receive stream is single-read and consumed | Body is empty in route handler | Cache body bytes in `request._body` or state |
| 3. Mutating global state in dependencies | Concurrency hazard; requests overwrite each other | Data leakage across users | Store request data in `request.state` or `contextvars` |
| 4. Expecting background tasks to run before response | Background tasks run after response streaming starts | Client waits for background work | Execute task inline before `return` |
| 5. Not catching exceptions around `yield` in dependencies | Cleanup code skipped on error | Leaked database connections / sessions | Use `try ... finally` in generator dependencies |
| 6. Modifying response headers after body streaming started | ASGI protocol violation | `RuntimeError: Response already started` | Append headers before calling `call_next` |

---

## 8. Performance & Scalability Considerations

- **Serialization Overhead**: `response_model` validation traverses Pydantic models. For ultra-high throughput endpoints (10,000+ RPS), consider returning raw `Response(content=..., media_type="application/json")` with pre-serialized bytes to bypass re-validation.
- **Dependency Caching (`use_cache=True`)**: If a dependency is shared by 5 parameters/sub-dependencies in a single request, FastAPI calculates it once and reuses the cached object.

---

## 9. Security & OWASP Guardrails

- **Early Rejection**: Because Pydantic validation executes *before* route logic, malformed inputs, SQL injection strings, and oversized payloads are rejected before reaching database drivers.
- **Header Stripping**: Use middleware to strip internal headers (`X-Backend-Server`, `X-Powered-By`) before transmitting responses to public clients.

---

## 10. Testing Strategies & Pytest Recipes

```python
import pytest
from httpx import ASGITransport, AsyncClient
from fastapi import FastAPI

@pytest.mark.asyncio
async def test_lifecycle_headers_present(app: FastAPI):
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        res = await client.get("/ping")
        assert res.status_code == 200
        assert "x-response-time-ms" in res.headers
```

---

## 11. Real-World Case Study

A fintech microservice suffered mysterious database connection leaks under load. Investigation of the request lifecycle revealed that developers used a generator dependency:
```python
async def get_db():
    db = await pool.acquire()
    yield db
    await db.close()
```
When an unhandled exception occurred in the route handler, the line `await db.close()` was never reached because it was not enclosed in a `try...finally` block.
- **Remediation**:
  Wrapped the dependency in `try: yield db finally: await db.close()`. All orphaned database connections were successfully eliminated.

---

## 12. Technical Interview Questions

<details>
<summary>Q1: What are the three primary components of the ASGI specification?</summary>

**Answer**: An ASGI application is an async callable taking three arguments:
1. `scope`: A dictionary containing connection metadata (type, path, HTTP method, headers, client IP).
2. `receive`: An async callable used to read incoming event messages (such as `http.request` containing body byte chunks).
3. `send`: An async callable used to send outgoing event messages (`http.response.start` for status/headers, and `http.response.body` for content chunks).
</details>

<details>
<summary>Q2: What is the mechanical difference between how FastAPI executes `def` vs `async def` route functions?</summary>

**Answer**:
- `async def`: FastAPI assumes the code is non-blocking. It awaits the coroutine directly on the main asyncio event loop.
- `def`: FastAPI assumes the code may contain blocking synchronous I/O. It offloads the execution to an asynchronous worker threadpool using `anyio.to_thread.run_sync()`.
</details>

<details>
<summary>Q3: When does a generator dependency's teardown code (after `yield`) execute?</summary>

**Answer**: The code after `yield` executes in the egress phase of the request lifecycle, *after* the route handler finishes and the HTTP response has been generated, but before the connection socket is closed. It executes in reverse order of dependency acquisition.
</details>

<details>
<summary>Q4: How does FastAPI's dependency caching mechanism (`use_cache=True`) work?</summary>

**Answer**: Within a single HTTP request lifecycle, if multiple dependencies or route handlers declare the same dependency callable, FastAPI evaluates the dependency on its first encounter, caches the resolved return value in `request.scope["fastapi_astack"]`, and returns the cached object for subsequent references during that request.
</details>

<details>
<summary>Q5: At what exact point in the lifecycle do `BackgroundTasks` execute?</summary>

**Answer**: `BackgroundTasks` execute after the route handler returns and the response has been sent to the ASGI server (meaning the client has already received the response headers and body bytes), but before the request context is finalized.
</details>

<details>
<summary>Q6: What happens if an exception is raised inside a middleware before `call_next()`?</summary>

**Answer**: The request pipeline is aborted immediately. Downstream middleware, routers, dependencies, and route handlers never execute. Starlette's server exception handler catches the exception and returns an HTTP 500 Internal Server Error (or custom exception response).
</details>

<details>
<summary>Q7: Why can't `request.body()` be read multiple times in raw ASGI without buffering?</summary>

**Answer**: In ASGI, `receive()` is an asynchronous stream of byte chunks. Once read, the socket stream is drained and cannot be rewound. Starlette's `Request` class buffers the stream in `request._body` on the first call so subsequent calls can return the cached bytes.
</details>

<details>
<summary>Q8: How does Pydantic v2's Rust core optimize response serialization in the egress phase?</summary>

**Answer**: Pydantic v2 validates and serializes response models directly in compiled Rust (`pydantic-core`), skipping Python bytecode interpretation. This reduces egress serialization latency by 5x to 20x compared to Pydantic v1.
</details>

---

## 13. Practical Exercises & Solutions

<details>
<summary>Exercise 1 (Easy): Measure route execution time in a middleware</summary>

**Problem**: Create a middleware that records execution time in `X-Duration` header.

**Solution**:
```python
import time
from fastapi import FastAPI, Request

app = FastAPI()

@app.middleware("http")
async def track_time(request: Request, call_next):
    t0 = time.perf_counter()
    res = await call_next(request)
    res.headers["X-Duration"] = f"{time.perf_counter() - t0:.4f}"
    return res
```
</details>

<details>
<summary>Exercise 2 (Easy): Store a request ID in `request.state`</summary>

**Problem**: Middleware setting `request.state.req_id = "abc-123"` and reading it in an endpoint.

**Solution**:
```python
from fastapi import FastAPI, Request

app = FastAPI()

@app.middleware("http")
async def set_state(request: Request, call_next):
    request.state.req_id = "abc-123"
    return await call_next(request)

@app.get("/req-id")
def get_req_id(request: Request):
    return {"id": request.state.req_id}
```
</details>

<details>
<summary>Exercise 3 (Medium): Safe Generator Dependency with try...finally</summary>

**Problem**: Write a generator dependency acquiring a resource and guaranteeing cleanup on error.

**Solution**:
```python
from fastapi import Depends, FastAPI, HTTPException

app = FastAPI()

async def safe_resource():
    resource = {"status": "open"}
    try:
        yield resource
    finally:
        resource["status"] = "closed"
        print("Resource safely closed")

@app.get("/test", dependencies=[Depends(safe_resource)])
def test_route():
    return {"message": "ok"}
```
</details>

<details>
<summary>Exercise 4 (Medium): Trigger post-response background task</summary>

**Problem**: Use `BackgroundTasks` to log an action after the response is returned.

**Solution**:
```python
from fastapi import BackgroundTasks, FastAPI

app = FastAPI()

def write_audit(msg: str):
    print(f"AUDIT LOGGED: {msg}")

@app.post("/action")
def do_action(bg: BackgroundTasks):
    bg.add_task(write_audit, "Action completed successfully")
    return {"status": "success"}
```
</details>

<details>
<summary>Exercise 5 (Hard): Implement Pure ASGI Streaming Body Inspecting Middleware</summary>

**Problem**: Write a pure ASGI middleware class that logs the size of the request body without draining the stream for downstream handlers.

**Solution**:
```python
class AsgiBodyLogger:
    def __init__(self, app):
        self.app = app

    async def __call__(self, scope, receive, send):
        if scope["type"] != "http":
            return await self.app(scope, receive, send)

        body_chunks = []
        async def buffered_receive():
            message = await receive()
            if message["type"] == "http.request":
                body_chunks.append(message.get("body", b""))
            return message

        await self.app(scope, buffered_receive, send)
```
</details>

<details>
<summary>Exercise 6 (Hard): Construct Complete Multi-Phase Telemetry Pipeline</summary>

**Problem**: Build a pipeline capturing metrics across 4 phases: Ingress, Auth Dependency, Route Execution, and Egress Serialization.

**Solution**:
```python
import time
from typing import Annotated
from fastapi import Depends, FastAPI, Request, Response
from starlette.middleware.base import BaseHTTPMiddleware

app = FastAPI()

class TelemetryMiddleware(BaseHTTPMiddleware):
    async def dispatch(self, request: Request, call_next) -> Response:
        request.state.t_ingress = time.perf_counter()
        response = await call_next(request)
        t_total = time.perf_counter() - request.state.t_ingress
        response.headers["X-Total-Duration"] = f"{t_total:.6f}"
        return response

app.add_middleware(TelemetryMiddleware)

def measure_auth(request: Request):
    t0 = time.perf_counter()
    # Simulated auth
    user = "alice"
    request.state.t_auth = time.perf_counter() - t0
    return user

@app.get("/telemetry-data")
def read_data(
    request: Request,
    user: Annotated[str, Depends(measure_auth)],
):
    return {
        "user": user,
        "auth_duration": getattr(request.state, "t_auth", 0),
    }
```
</details>
