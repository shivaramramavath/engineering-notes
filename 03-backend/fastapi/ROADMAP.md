# FastAPI Learning Roadmap (ROADMAP.md)

> Progressive career and mastery trajectories for FastAPI backend developers, cloud engineers, and technical architects.

---

## Track 1: Python Developer to API Engineer (3-4 Weeks)
**Objective**: Build and deploy typed, validated REST APIs with PostgreSQL.

```
00-setup ──> 01-fundamentals ──> 02-pydantic-v2 ──> 03-dependency-injection ──> 06-databases
```
- Master Pydantic v2 schemas, type coercion, and OpenAPI docs.
- Wire database sessions using `AsyncSession` and `Annotated[..., Depends()]`.
- Build [Project 01: Todo API with PostgreSQL](./17-projects/01-todo-api-with-postgres/README.md).

---

## Track 2: Enterprise Production Backend Engineer (6-8 Weeks)
**Objective**: Implement production authentication, database migrations, testing, and Docker deployment.

```
04-routing ──> 05-middleware ──> 07-authentication ──> 11-testing ──> 15-production
```
- Implement Argon2id password hashing, RS256 JWTs, refresh token rotation, and RBAC.
- Author zero-downtime Alembic migrations and CI/CD pipelines.
- Test endpoints with `httpx.AsyncClient`, `pytest-asyncio`, and Testcontainers.
- Build [Project 03: Auth Service](./17-projects/03-auth-service/README.md) and [Project 08: E-Commerce Backend](./17-projects/08-ecommerce-backend/README.md).

---

## Track 3: High-Performance & Distributed Async Specialist (5-6 Weeks)
**Objective**: Maximize throughput, optimize async event loops, and coordinate distributed message queues.

```
09-async-and-concurrency ──> 10-background-and-messaging ──> 13-observability ──> 14-performance
```
- Deep dive into event loop lag, uvloop, orjson, and Free-threaded Python 3.14t.
- Integrate Celery 5.7, Taskiq, and Redis Streams with dead-letter handling.
- Instrument OpenTelemetry distributed tracing and Prometheus metrics.
- Build [Project 04: URL Shortener](./17-projects/04-url-shortener-with-redis/README.md) and [Project 12: Microservices System](./17-projects/12-microservices-system/README.md).

---

## Track 4: AI & Machine Learning Serving Architect (4-5 Weeks)
**Objective**: Serve machine learning models and LLM agent backends with streaming and vector databases.

```
04-routing (Lifespan) ──> 05-middleware (SSE) ──> 16-ai-and-ml-serving ──> 18-system-design
```
- Preload ML models during application lifespan startup.
- Implement streaming LLM completions using Server-Sent Events (SSE).
- Connect `pgvector` for semantic search and Retrieval-Augmented Generation (RAG).
- Build [Project 13: RAG API with pgvector](./17-projects/13-rag-api-with-pgvector/README.md) and [Project 14: ML Model Serving API](./17-projects/14-ml-model-serving-api/README.md).
