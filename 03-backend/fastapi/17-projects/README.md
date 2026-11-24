# Module 17: Production Reference Applications

Welcome to **Module 17: Production Reference Applications**. This directory contains 16 complete, end-to-end reference applications. Each application adheres to the authoritative project template, featuring complete source layouts, Pydantic schemas, SQLAlchemy async repositories, Alembic migrations, Docker Compose environments, and comprehensive pytest suites.

---

## Project Catalog

| Directory | Project Title | Key Technologies & Architecture |
| :--- | :--- | :--- |
| [01-todo-api-with-postgres](./01-todo-api-with-postgres/) | **Todo API with PostgreSQL** | Layered CRUD, SQLAlchemy 2.0 Async, Alembic, Docker Compose. |
| [02-blog-api](./02-blog-api/) | **Publishing & Blog API** | Slugs, tags, markdown rendering, author permissions, pagination. |
| [03-auth-service](./03-auth-service/) | **Enterprise Auth Service** | Argon2id, RS256 JWT, refresh tokens, OAuth2 Google, TOTP 2FA. |
| [04-url-shortener-with-redis](./04-url-shortener-with-redis/) | **URL Shortener with Redis** | Base62 encoding, Redis caching, write-behind analytics, TTL expiration. |
| [05-file-upload-service](./05-file-upload-service/) | **File Upload Service** | AWS S3 presigned URLs, chunked streaming, MIME validation, virus scanning hook. |
| [06-notification-service](./06-notification-service/) | **Notification Service** | Celery/Taskiq workers, email (SES), SMS (Twilio), in-app push, retry backoff. |
| [07-realtime-chat-with-websockets](./07-realtime-chat-with-websockets/) | **Realtime Chat Platform** | WebSockets, Redis Pub/Sub multi-node broadcast, room presence, chat history. |
| [08-ecommerce-backend](./08-ecommerce-backend/) | **E-Commerce Backend** | ACID order transactions, inventory reservation, Stripe checkout, transactional outbox. |
| [09-multi-tenant-saas-api](./09-multi-tenant-saas-api/) | **Multi-Tenant SaaS API** | Tenant isolation via schema/row, tenant contextvars, subdomain routing. |
| [10-task-manager-with-rbac](./10-task-manager-with-rbac/) | **Task Manager with RBAC** | Fine-grained roles and permissions, team workspaces, audit logging. |
| [11-graphql-api-with-strawberry](./11-graphql-api-with-strawberry/) | **GraphQL API with Strawberry**| Strawberry GraphQL, dataloaders for N+1 prevention, query complexity guards. |
| [12-microservices-system](./12-microservices-system/) | **Microservices with Event Broker**| API gateway + order/inventory services communicating over RabbitMQ/Kafka. |
| [13-rag-api-with-pgvector](./13-rag-api-with-pgvector/) | **RAG Pipeline API** | Document ingestion, text chunking, OpenAI embeddings, pgvector search. |
| [14-ml-model-serving-api](./14-ml-model-serving-api/) | **ML Model Serving API** | ONNX/PyTorch inference, dynamic request micro-batching, latency monitoring. |
| [15-booking-system-with-concurrency-control](./15-booking-system-with-concurrency-control/)| **Seat Booking Engine** | Pessimistic and optimistic locking, preventing double-booking under load. |
| [16-webhook-dispatcher-and-receiver](./16-webhook-dispatcher-and-receiver/)| **Webhook Platform** | HMAC-SHA256 signature verification, exponential retry dispatcher, dead letter queue. |
