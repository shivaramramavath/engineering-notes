# FastAPI & ASGI Technical Glossary (GLOSSARY.md)

> Standardized terminology, protocol definitions, and architectural concepts across the FastAPI curriculum.

---

### ASGI (Asynchronous Server Gateway Interface)
The standard Python specification between async web servers (Uvicorn, Hypercorn) and async web frameworks (FastAPI, Starlette). Operates on a 3-argument callable: `async def app(scope, receive, send)`.

### Annotated Dependencies (PEP 593)
Using Python's `typing.Annotated` to bind dependency callables to types (e.g. `DbSession = Annotated[AsyncSession, Depends(get_db)]`), preserving static type safety without default parameter hacks.

### AnyIO
Asynchronous compatibility library bridging `asyncio` and `trio`. FastAPI uses AnyIO internally for structured concurrency and threadpool offloading.

### BaseHTTPMiddleware
Starlette middleware base class wrapping ASGI requests in an HTTP abstraction. Has known performance overhead and streaming cancellation caveats compared to pure ASGI middleware.

### ConfigDict
Pydantic v2's replacement for Pydantic v1's inner `class Config`. Configures schema generation, mutation restrictions (`frozen=True`), and attribute extraction (`from_attributes=True`).

### Depends()
FastAPI's dependency injection primitive. Declares a sub-routine or provider to be evaluated before calling the route handler. Results can be cached across a single request via `use_cache=True`.

### Lifespan
Starlette/FastAPI protocol replacing legacy `@app.on_event("startup")` and `@app.on_event("shutdown")`. Managed as an asynchronous context manager wrapping the application lifecycle.

### Pydantic v2
Complete rewrite of Pydantic with a Rust core (`pydantic-core`). Delivers 5-20x faster serialization and validation, strict mode, and unified validator decorators.

### Starlette
The lightweight ASGI toolkit providing routing, HTTP request/response abstractions, WebSocket support, and session/cookie utilities that FastAPI builds upon.

### Uvicorn
Lightning-fast ASGI server implementation using `uvloop` (libuv wrapper) and `httptools` (C HTTP parser). Default server runner for FastAPI.

### uv
High-performance Python package and project manager written in Rust, replacing pip, virtualenv, and pip-tools with sub-second resolution.
