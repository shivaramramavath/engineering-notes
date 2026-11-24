# Technical Interview Mastery: FastAPI Core Internals & Architecture

## Overview & Evaluation Criteria

Senior and Staff-level interviews focus heavily on what happens *under the hood* of FastAPI: how it wraps Starlette, parses ASGI specifications, introspects Python type annotations, constructs OpenAPI schemas, and dispatches requests between asynchronous event loops and thread pools.

---

## Technical Q&A Deep Dive

<details>
<summary><strong>Q1: How does FastAPI internally bridge Starlette and Pydantic? Walk through the complete lifecycle of an incoming HTTP request.</strong></summary>

### Answer:
FastAPI inherits directly from Starlette's `Starlette` class (`class FastAPI(Starlette)`). Its core contribution lies in request routing and dependency resolution:
1. **ASGI Entrypoint**: The ASGI server (e.g. Uvicorn) receives an HTTP connection, translates it into an ASGI `scope`, `receive`, and `send` tuple, and calls `app(scope, receive, send)`.
2. **Routing & Dispatch**: Starlette's `Router` matches the path and HTTP method against registered `APIRoute` instances.
3. **Endpoint Introspection**: During startup, FastAPI wraps each endpoint function with an internal `solve_dependencies` runner. It introspects function parameter signatures using Python's `inspect.signature` and typing annotations.
4. **Data Extraction & Pydantic Validation**:
   - Headers, query params, cookies, and path parameters are extracted from Starlette's `Request` object.
   - The body stream is read and passed to the generated Pydantic `TypeAdapter` / model validator (`model_validate_json` in Pydantic v2).
   - If validation fails, a `RequestValidationError` (HTTP 422) is raised with detailed field-level error trees.
5. **Dependency Execution**: Injected dependencies (`Depends`) are solved as a directed acyclic graph (DAG), honoring dependency caching (`use_cache=True`).
6. **Execution Target Selection**:
   - If defined as `async def`: Executed directly on the ASGI asyncio event loop.
   - If defined as `def`: Offloaded to `starlette.concurrency.run_in_threadpool` (AnyIO worker thread pool) to avoid blocking the event loop.
7. **Response Serialization**: The returned object is validated against `response_model` using Pydantic, dumped to JSON/bytes via `fastapi.encoders.jsonable_encoder` (or `orjson`), and sent via the ASGI `send` callable.

</details>

<details>
<summary><strong>Q2: Why does declaring a route as <code>def</code> instead of <code>async def</code> prevent the event loop from blocking? What are the performance trade-offs?</strong></summary>

### Answer:
- When a route is defined as `def` (synchronous), FastAPI detects `not asyncio.iscoroutinefunction(endpoint)`.
- Starlette delegates execution to `anyio.to_thread.run_sync` (backed by a thread pool, default max 40 threads in Python standard thread pools).
- **Benefit**: If a developer performs a blocking synchronous operation (e.g. legacy `time.sleep()`, synchronous `requests.get()`, or standard `psycopg2` queries), it blocks *only that individual thread*, leaving the main asyncio event loop free to handle thousands of concurrent asynchronous requests.
- **Trade-off / Penalty**: Thread context switching has measurable memory overhead ($\approx 8\text{MB}$ stack per thread depending on OS) and CPU latency compared to pure async coroutines. If the thread pool is exhausted (all 40 worker threads blocked), subsequent synchronous requests queue up, leading to high latency.

</details>

<details>
<summary><strong>Q3: How does FastAPI construct the OpenAPI 3.1 specification dynamically at runtime? Can it be cached in production?</strong></summary>

### Answer:
- FastAPI dynamically builds the OpenAPI schema on the first call to `/openapi.json` by inspecting all registered `APIRoute` objects.
- It iterates over route parameters, converting Pydantic models into OpenAPI components using Pydantic v2's `.model_json_schema()`.
- **Production Optimization**: In massive APIs with hundreds of endpoints and complex nested Pydantic models, computing the OpenAPI schema dynamically can add 500ms to 2s of cold-start latency.
- **Best Practice Recipe**:
```python
from fastapi import FastAPI
from fastapi.openapi.utils import get_openapi

app = FastAPI()

def custom_openapi():
    if app.openapi_schema:
        return app.openapi_schema
    app.openapi_schema = get_openapi(
        title="Production API",
        version="1.0.0",
        routes=app.routes,
    )
    return app.openapi_schema

app.openapi = custom_openapi
```
</details>

<details>
<summary><strong>Q4: Explain the difference between <code>BackgroundTasks</code> in FastAPI and a distributed task queue like Celery or ARQ. When is it dangerous to use <code>BackgroundTasks</code>?</strong></summary>

### Answer:
- **`BackgroundTasks`**:
  - Runs in-process on the same server after sending the HTTP response.
  - Implemented using Starlette's `BackgroundTask` wrapper around `anyio` or coroutines.
  - **Dangers**:
    1. **Zero Durability**: If the server crashes, power cycles, or is terminated by an auto-scaler/deploy (SIGTERM), all queued in-memory background tasks are permanently lost.
    2. **Resource Starvation**: CPU-heavy tasks (video encoding, data processing) compete directly with the web server event loop and memory.
    3. **No Retries or DLQ**: Lacks native retry logic, exponential backoff, or dead-letter queues.
- **Rule of Thumb**: Use `BackgroundTasks` only for trivial, fire-and-forget side effects (e.g. sending a lightweight telemetry beacon or logging a non-critical audit entry). For business-critical operations (emailing receipts, billing, transacting), always use an external durable broker (Redis Streams, Celery, SQS).
</details>

<details>
<summary><strong>Q5: What is the role of <code>jsonable_encoder</code> and why can it become a performance bottleneck in high-throughput endpoints?</strong></summary>

### Answer:
- `jsonable_encoder` recursively converts complex Python objects (datetimes, UUIDs, Decimals, Sets, Pydantic models) into JSON-compatible primitives (`str`, `int`, `float`, `dict`, `list`).
- **Bottleneck**: Because it performs deep recursive traversal in pure Python, calling `jsonable_encoder` on large lists (e.g., 5,000 DB records) incurs substantial CPU overhead and garbage collection pressure.
- **Solution**: Bypass `jsonable_encoder` by using `ORJSONResponse` combined with Pydantic v2's native Rust-compiled serialization (`model.model_dump_json()`), achieving up to 5x-10x throughput gains.
</details>
