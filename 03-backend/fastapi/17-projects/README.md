# Module 17: Production Reference Projects Catalog

Welcome to the **FastAPI Production Reference Projects** module. This suite contains 16 comprehensive, production-grade reference architectures adhering to modern enterprise design principles, high concurrency, strict type safety with Pydantic v2, and asynchronous PostgreSQL/Redis integrations.

Each project is fully specified with end-to-end architectures, domain designs, schemas, repositories, services, automated testing suites, multi-stage Dockerfiles, docker-compose orchestration, and operational runbooks.

---

## Reference Architecture Matrix

| # | Project Code | Project Title | Key Architectural Patterns & Tech Stack | Complexity |
|---|---|---|---|---|
| **01** | `FASTAPI-PROJ-001` | [Todo REST API with PostgreSQL](file:///03-backend/fastapi/17-projects/01-todo-api-with-postgres.md) | SQLAlchemy 2.0 Async, Alembic migrations, Dependency Injection, CRUD Repository pattern | Beginner |
| **02** | `FASTAPI-PROJ-002` | [Production Blog & Content Management API](file:///03-backend/fastapi/17-projects/02-blog-api.md) | JWT auth, Tagging many-to-many, Cursor pagination, Markdown sanitization | Intermediate |
| **03** | `FASTAPI-PROJ-003` | [Enterprise Auth & Session Service](file:///03-backend/fastapi/17-projects/03-auth-service.md) | Dual-token (Access/Refresh), Redis token revocation blocklist, Argon2 password hashing | Intermediate |
| **04** | `FASTAPI-PROJ-004` | [High-Performance URL Shortener](file:///03-backend/fastapi/17-projects/04-url-shortener-with-redis.md) | Base62 encoding, Redis 307 redirect cache, Async click analytics buffering & batch flush | Intermediate |
| **05** | `FASTAPI-PROJ-005` | [Secure File Storage & Processing Service](file:///03-backend/fastapi/17-projects/05-file-upload-service.md) | Direct S3 presigned PUT/GET, Spooled file streaming, SHA256 checksums, Virus scan hook | Intermediate |
| **06** | `FASTAPI-PROJ-006` | [Multi-Channel Notification Dispatcher](file:///03-backend/fastapi/17-projects/06-notification-service.md) | Redis Streams (`XADD`, `XREADGROUP`), Consumer Groups, Jinja2 templating, Email/SMS/Webhook | Intermediate |
| **07** | `FASTAPI-PROJ-007` | [Real-Time Chat Engine](file:///03-backend/fastapi/17-projects/07-realtime-chat-with-websockets.md) | WebSockets, Multi-Pod Redis Pub/Sub broadcast, Persistent message history, Connection Manager | Advanced |
| **08** | `FASTAPI-PROJ-008` | [High-Concurrency E-Commerce Engine](file:///03-backend/fastapi/17-projects/08-ecommerce-backend.md) | Pessimistic Row Locks (`with_for_update`), Redis cart sessions, Stripe webhooks, Idempotency-Key | Advanced |
| **09** | `FASTAPI-PROJ-009` | [Multi-Tenant SaaS with Schema Isolation](file:///03-backend/fastapi/17-projects/09-multi-tenant-saas-api.md) | Schema-per-tenant, ContextVar propagation, Dynamic `SET search_path`, Tenant middleware | Advanced |
| **10** | `FASTAPI-PROJ-010` | [Task Manager with Dynamic RBAC & PBAC](file:///03-backend/fastapi/17-projects/10-task-manager-with-rbac.md) | Role & permission matrix, Policy evaluation, Scoped Security dependencies | Intermediate |
| **11** | `FASTAPI-PROJ-011` | [GraphQL API with Strawberry](file:///03-backend/fastapi/17-projects/11-graphql-api-with-strawberry.md) | Strawberry GraphQL, Async DataLoader batching (N+1 resolution), Schema stitching | Intermediate |
| **12** | `FASTAPI-PROJ-012` | [Microservices Gateway & Distributed Saga](file:///03-backend/fastapi/17-projects/12-microservices-system.md) | gRPC asynchronous client channels, Saga Orchestrator with compensating transactions | Advanced |
| **13** | `FASTAPI-PROJ-013` | [Production RAG API with pgvector](file:///03-backend/fastapi/17-projects/13-rag-api-with-pgvector.md) | PostgreSQL `pgvector`, HNSW cosine index, OpenAI embeddings, Grounded prompt synthesis | Advanced |
| **14** | `FASTAPI-PROJ-014` | [ML Model Serving with ONNX Runtime](file:///03-backend/fastapi/17-projects/14-ml-model-serving-api.md) | Dynamic micro-batching (`asyncio.Queue`), Thread pool offloading, Sub-10ms inference | Advanced |
| **15** | `FASTAPI-PROJ-015` | [Seat Reservation with Optimistic Locking](file:///03-backend/fastapi/17-projects/15-booking-system-with-concurrency-control.md) | OCC (`version_id`), Redis distributed hold mutex (10-minute timer), HTTP 409 conflict reconciliation | Advanced |
| **16** | `FASTAPI-PROJ-016` | [Webhook Dispatcher & Receiver](file:///03-backend/fastapi/17-projects/16-webhook-dispatcher-and-receiver.md) | HMAC-SHA256 signing & constant-time verification, Exponential backoff + jitter retries, Dead Letter Queue | Advanced |

---

## Architectural Patterns Guide

### 1. Concurrency Control (OCC vs. Pessimistic)
- **Pessimistic Locking (`SELECT FOR UPDATE`)**: Used in [Project 08 (E-Commerce)](file:///03-backend/fastapi/17-projects/08-ecommerce-backend.md) where stock contention is extremely high and inventory cannot be oversold.
- **Optimistic Locking (`version_id`) + Redis Holds**: Used in [Project 15 (Seat Booking)](file:///03-backend/fastapi/17-projects/15-booking-system-with-concurrency-control.md) to provide 10-minute cart holds without holding open database locks.

### 2. High-Throughput Buffering & Event Streams
- **Redis Buffering**: [Project 04 (URL Shortener)](file:///03-backend/fastapi/17-projects/04-url-shortener-with-redis.md) increments click counts in Redis and flushes in batches to avoid write-heavy DB bottlenecks.
- **Redis Streams & Consumer Groups**: [Project 06 (Notification Service)](file:///03-backend/fastapi/17-projects/06-notification-service.md) decouples ingress from multi-channel delivery workers with guaranteed delivery and ack semantics.

### 3. Distributed Coordination & State Machines
- **Saga Orchestrator**: [Project 12 (Microservices Gateway)](file:///03-backend/fastapi/17-projects/12-microservices-system.md) coordinates cross-service transactions across inventory and payments with automated compensation rollbacks.
