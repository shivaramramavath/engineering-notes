# FastAPI Ecosystem Verified Facts (FASTAPI_FACTS.md)

> Authoritative reference facts, baseline versions, and architectural standards for the FastAPI curriculum (`03-backend/fastapi/`).
> Verified: 2026-11-24.

---

## 1. Core Framework & Runtime Baseline

| Component | Verified Version | Release / Status Notes |
| :--- | :--- | :--- |
| **FastAPI** | `0.165.0` | Requires Python `>=3.11`. Native `fastapi[standard]` bundle. Built-in FastAPI CLI (`fastapi dev`, `fastapi run`). Native Pydantic v2 core. |
| **Starlette** | `1.10.0` | Optimized form parsing, lean streaming, `GZipMiddleware` chunk flushing, RFC 9110 compliant `FileResponse`. |
| **Pydantic** | `v2.14.4` | Built on Rust `pydantic-core v2.50.5`. Deprecated v1 idioms (`class Config`, `.dict()`, `@validator`) replaced with `model_config = ConfigDict(...)`, `.model_dump()`, `@field_validator`, `@model_validator`. |
| **pydantic-settings** | `2.16.3` | Standalone package for environment configuration and `.env` parsing. |
| **Uvicorn** | `v0.54.0` | High-performance ASGI server based on `uvloop` and `httptools`. |
| **SQLModel** | `0.0.50` | Centralized under `fastapi/sqlmodel`. Strictly requires Pydantic v2 (`0.0.31+` dropped Pydantic v1). Compatible with SQLAlchemy 2.0+. |
| **SQLAlchemy** | `2.1.x` / `2.0.58` | Asynchronous ORM via `create_async_engine` (`sqlalchemy[asyncio]`). Default PostgreSQL driver shifted to `psycopg` (psycopg3), while `asyncpg` remains top choice for pure asyncio. |
| **Python Baseline** | Python 3.12 / 3.13 / 3.14 | Full support for Python 3.14t Free-Threading (No-GIL, PEP 703) in FastAPI `>=0.136.0` allowing sync `def` endpoints to execute parallel CPU workloads across cores. |

---

## 2. Key Ecosystem Tooling & Library Versions

| Domain | Library | Version | Architecture / Usage Rule |
| :--- | :--- | :--- | :--- |
| **Package Manager** | `uv` | `0.13+` | Next-generation Python package and virtualenv manager written in Rust. |
| **Background Queues** | `Celery` | `5.7.0` | Distributed task queue for Python 3.10-3.14. |
| **Async Queues** | `Taskiq` | `0.13.0` | Pure asyncio-native task queue with Pydantic v2 support. |
| **Async Queues** | `Arq` | `0.28.0` | Lightweight Redis-backed asyncio job queue. |
| **Authentication** | `passlib` / `argon2-cffi` | `23.x+` | Argon2id recommended for modern password hashing over bcrypt. |
| **JWT** | `PyJWT` / `joserfc` | `2.9+` | Cryptographically signed tokens (RS256 / ES256 / HS256). |
| **HTTP Client** | `httpx` | `0.28+` | `AsyncClient` and `ASGITransport` standard for FastAPI testing. |
| **Testing** | `pytest-asyncio` | `0.24+` | Async fixture support, `asyncio_mode = "auto"`. |
| **Containers** | `testcontainers-python` | `4.8+` | Ephemeral Docker containers for PostgreSQL, Redis, Kafka in tests. |
| **Observability** | `structlog` / `loguru` | Modern | Structured JSON logging with contextvars request ID propagation. |
| **Tracing** | `OpenTelemetry` | `1.30+` | `opentelemetry-instrumentation-fastapi` for W3C distributed tracing. |

---

## 3. Mandatory Coding Idioms & Anti-Patterns to Flag

1. **Pydantic v1 Deprecations**:
   - Flag: `class Config`, `@validator`, `@root_validator`, `.dict()`, `.json()`, `parse_obj()`.
   - Use: `model_config = ConfigDict(...)`, `@field_validator`, `@model_validator(mode='before'|'after')`, `.model_dump()`, `.model_dump_json()`, `TypeAdapter(T).validate_python()`.
2. **Lifespan Context Manager**:
   - Flag: `@app.on_event("startup")` and `@app.on_event("shutdown")` (deprecated).
   - Use: `@asynccontextmanager async def lifespan(app: FastAPI): ... yield ...`.
3. **Annotated Dependencies**:
   - Flag: `def endpoint(db: Session = Depends(get_db))`.
   - Use: `DbSession = Annotated[AsyncSession, Depends(get_db)]` $\to$ `def endpoint(db: DbSession)`.
4. **Async vs Sync Threadpool Gotchas**:
   - Running blocking I/O (e.g. `requests.get`, `time.sleep`, synchronous DB drivers) inside `async def` blocks the single-threaded event loop.
   - Synchronous `def` endpoints are automatically offloaded to an `anyio` worker threadpool.
5. **SQLAlchemy 2.0 Async Session Rules**:
   - Use `AsyncSession` with `async with async_session_maker() as session:`.
   - Execute queries using `await session.scalars(select(User).where(...))`.
   - Never use lazy loading across async boundaries without `selectinload` or `joinedload`.
