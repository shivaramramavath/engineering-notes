# FastAPI Enterprise Backend Curriculum

> Comprehensive, production-grade learning track and engineering reference for **FastAPI**, standardized on **FastAPI 0.165.x**, **Starlette 1.10.x**, **Pydantic v2.14.x**, and **Python 3.12+ (3.13 / 3.14t compatible)**.

---

## Curriculum Structure

| Module | Title | Scope & Topics |
| :--- | :--- | :--- |
| [00-setup](./00-setup/README.md) | **Setup & Environment** | ASGI, Starlette, Uvicorn, `uv`, FastAPI CLI (`fastapi dev`), project structure. |
| [01-fundamentals](./01-fundamentals/README.md) | **Core Fundamentals** | Path/Query parameters, Pydantic request bodies, 422 errors, async vs sync threadpool. |
| [02-pydantic-v2](./02-pydantic-v2/README.md) | **Pydantic v2 Mastery** | Rust core validation, `ConfigDict`, field/model validators, TypeAdapter, serialization. |
| [03-dependency-injection](./03-dependency-injection/README.md) | **Dependency Injection** | `Depends()`, `typing.Annotated`, `use_cache`, sub-dependencies, yield lifecycle. |
| [04-routing-and-structure](./04-routing-and-structure/README.md) | **Routing & Architecture** | `APIRouter`, modular apps, Lifespan context managers, app state, custom routes. |
| [05-middleware-and-requests](./05-middleware-and-requests/README.md) | **Middleware & Realtime** | Middleware order, BaseHTTPMiddleware pitfalls, SSE, WebSockets, streaming. |
| [06-databases](./06-databases/README.md) | **Databases & ORMs** | SQLAlchemy 2.0 Async, Alembic, asyncpg, psycopg3, SQLModel, Testcontainers. |
| [07-authentication-and-security](./07-authentication-and-security/README.md) | **Auth & Enterprise Security**| Argon2id, RS256 JWT, refresh tokens, Passkeys, RBAC/ABAC, OWASP Top 10. |
| [08-api-design](./08-api-design/README.md) | **API Design & Contracts** | REST pagination, RFC 7807 Problem Details, Idempotency, OpenAPI, GraphQL. |
| [09-async-and-concurrency](./09-async-and-concurrency/README.md) | **Async & Concurrency** | Asyncio, AnyIO, threadpool offload, blocking detection, timeouts. |
| [10-background-and-messaging](./10-background-and-messaging/README.md) | **Background Queues** | Celery 5.7, Taskiq, Arq, Redis Streams, RabbitMQ, Kafka. |
| [11-testing](./11-testing/README.md) | **Testing & Quality** | `httpx.AsyncClient`, pytest-asyncio, Testcontainers, Schemathesis, Locust. |
| [12-architecture-and-patterns](./12-architecture-and-patterns/README.md) | **Architecture & Patterns** | Hexagonal, Clean, DDD, Modular Monolith, CQRS, Design Patterns. |
| [13-observability-and-operations](./13-observability-and-operations/README.md)| **Observability & Operations** | Structlog, OpenTelemetry distributed tracing, Prometheus metrics, Terminus. |
| [14-performance](./14-performance/README.md) | **Performance & Tuning** | Uvicorn/Gunicorn tuning, uvloop, orjson, Free-threaded Python 3.14t. |
| [15-production-and-deployment](./15-production-and-deployment/README.md) | **Production & Deployment** | Multi-stage Docker with `uv`, Nginx proxy, Kubernetes manifests, CI/CD. |
| [16-ai-and-ml-serving](./16-ai-and-ml-serving/README.md) | **AI & ML Model Serving** | Model serving with Lifespan, batching, LLM streaming (SSE), pgvector RAG. |
| [17-projects](./17-projects/README.md) | **Reference Applications** | 16 complete production reference projects with code, tests, and Docker. |
| [18-system-design](./18-system-design/README.md) | **System Design Blueprints** | 14 high-throughput system design blueprints. |
| [19-interview](./19-interview/README.md) | **Interview Question Bank** | 12 technical interview question sets with verified solutions. |
| [20-cheatsheets](./20-cheatsheets/README.md) | **Quick Reference Cards** | 14 high-density syntax, CLI, and configuration cheat sheets. |

---

## Baseline Standards & Tooling

```bash
# Recommended Project Setup with uv
uv init my-fastapi-app
cd my-fastapi-app
uv add "fastapi[standard]" sqlalchemy[asyncio] asyncpg pydantic-settings
uv run fastapi dev main.py
```

- **Python Baseline**: Python 3.12+ (Python 3.14t No-GIL compatible).
- **FastAPI**: `>=0.165.0`.
- **Pydantic**: `>=2.14.0`.
- **SQLAlchemy**: `>=2.0.30` (Async mode).
- **Package Manager**: `uv`.
- **Linter & Type Checker**: `ruff`, `mypy --strict`.

---

## Architectural Conventions & Rules

All notes and applications adhere to:
1. **[FASTAPI_CONVENTIONS.md](./FASTAPI_CONVENTIONS.md)**: Metadata schemas, 13-section concept format, project format.
2. **[FASTAPI_FACTS.md](./FASTAPI_FACTS.md)**: Verified framework versions, release notes, and deprecations.
3. **[FASTAPI_DECISIONS.md](./FASTAPI_DECISIONS.md)**: Architecture Decision Records (ADRs 001 - 006).
4. **Markdown-Only Discipline**: All source code, manifests, and tests reside in fenced code blocks inside markdown files. Zero non-markdown files inside the repository.
