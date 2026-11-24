# Quick Reference: Architectural Patterns & Critical Anti-Patterns

## Production Patterns & Anti-Patterns Comparison

| Category | Recommended Production Pattern | Critical Anti-Pattern to Avoid |
|:---|:---|:---|
| **Route Concurrency** | Use `async def` with purely async I/O (`httpx`, `asyncpg`, `aioredis`). | Calling blocking synchronous functions (`time.sleep`, `requests.get`) inside `async def` (halts the entire event loop). |
| **Blocking Code** | Use `def` (synchronous) or `asyncio.to_thread` for CPU/blocking work. | Running heavy regex or CPU-bound tasks in the event loop thread without offloading. |
| **Database Sessions** | Request-scoped session via dependency with `yield` and rollback guards. | Storing a single global `Session()` instance shared across all concurrent requests. |
| **ORM Serialization** | Use `ConfigDict(from_attributes=True)` with explicit response models. | Returning raw SQLAlchemy ORM entities without response models (leaks private columns/hashes). |
| **N+1 SQL Queries** | Eager-load relationships with `selectinload()` or `joinedload()`. | Accessing related collections inside loops during Pydantic serialization. |
| **Large File Uploads**| Direct S3 presigned PUT uploads; stream large payloads with chunks. | Reading entire 5GB files into Python memory via `file.read()` (causes OOM crashes). |
| **Background Work** | External durable queues (ARQ, Celery, Redis Streams, SQS) for critical jobs.| Using in-process `BackgroundTasks` for billing, payments, or critical email deliveries. |
| **Secrets Management**| `pydantic-settings` reading from `.env` or AWS Secrets Manager. | Hardcoding API keys and database credentials in source code or committing `.env`. |
| **Kubernetes Scaling**| 1 Uvicorn worker per container; scale pods horizontally via HPA. | Running `--workers 8` inside a 1-core container (breaks HPA metrics and CPU scheduling). |
| **Liveness Probes** | Lightweight `/health/live` endpoint returning 200 without DB checks. | Checking PostgreSQL in Liveness Probe (DB latency causes mass cascade pod kills). |
| **Exception Handling**| Centralized `@app.exception_handler` returning structured JSON error schemas.| Bare `except: pass` or returning raw Python stack traces to the public internet. |
