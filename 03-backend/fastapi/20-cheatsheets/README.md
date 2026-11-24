# Module 20: Quick Reference Cheat Sheets

Welcome to **Module 20: Quick Reference Cheat Sheets**. This module contains condensed, high-density reference cards for daily production work, syntax lookups, CLI flags, common error solutions, and upgrade checklists.

---

## Cheat Sheet Catalog

| File | Title | Description |
| :--- | :--- | :--- |
| [01-fastapi-syntax.md](./01-fastapi-syntax.md) | **FastAPI Core Syntax** | Path operations, parameter annotations (`Annotated`), status codes, response models. |
| [02-pydantic-v2.md](./02-pydantic-v2.md) | **Pydantic v2 Cheat Sheet** | Field definitions, ConfigDict, `@field_validator`, `@model_validator`, TypeAdapter. |
| [03-dependencies.md](./03-dependencies.md) | **Dependency Injection** | Sub-dependencies, yield fixtures, class-based dependencies, and dependency overrides. |
| [04-sqlalchemy-async.md](./04-sqlalchemy-async.md) | **SQLAlchemy 2.0 Async** | Declarative Base, mapped_column, select/insert/update/delete queries, and relationship joins. |
| [05-alembic.md](./05-alembic.md) | **Alembic Commands & Recipes** | `init`, `revision --autogenerate`, `upgrade head`, `downgrade -1`, and async env.py. |
| [06-auth-and-jwt.md](./06-auth-and-jwt.md) | **Authentication & JWT** | Argon2id hashing, PyJWT encode/decode, OAuth2PasswordBearer, and token verification. |
| [07-testing-recipes.md](./07-testing-recipes.md) | **Testing Quick Recipes** | `httpx.AsyncClient` setup, pytest fixtures, dependency override idioms, and test DB rollback. |
| [08-background-and-queues.md](./08-background-and-queues.md) | **Background Tasks & Queues**| BackgroundTasks syntax, Celery task definitions, Taskiq brokers, and Arq workers. |
| [09-uvicorn-and-gunicorn-flags.md](./09-uvicorn-and-gunicorn-flags.md)| **Server CLI Flags** | All production CLI flags for `uvicorn`, `gunicorn -k uvicorn.workers.UvicornWorker`, and `hypercorn`. |
| [10-docker-and-compose.md](./10-docker-and-compose.md) | **Docker & Compose Recipes** | Multi-stage Dockerfile with `uv`, Docker Compose setups for Postgres, Redis, and app containers. |
| [11-openapi-tips.md](./11-openapi-tips.md) | **OpenAPI & Swagger Customization**| Tags, descriptions, summary, operation_id, security schemes, and custom Swagger UI URLs. |
| [12-http-status-and-errors.md](./12-http-status-and-errors.md) | **HTTP Status & Error Payloads**| RFC 7807 problem details schema, status code cheat sheet, and exception handlers. |
| [13-common-errors-and-fixes.md](./13-common-errors-and-fixes.md) | **Common Errors & Fixes** | Solutions to 422 Unprocessable Entity, GreenletSpawn errors, event loop closed, and cyclic imports. |
| [14-upgrade-cheatsheet.md](./14-upgrade-cheatsheet.md) | **FastAPI & Pydantic Upgrade Guide**| Step-by-step migration recipes from Pydantic v1 to v2 and on_event to lifespan. |
