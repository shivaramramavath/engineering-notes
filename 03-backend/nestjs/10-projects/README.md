# Module 10: Production Reference Applications & Projects

Welcome to the **Production Reference Projects** catalog for the NestJS backend engineering curriculum. This directory contains 15 production-grade, architectural reference implementations demonstrating real-world enterprise engineering patterns.

---

## Project Catalog

| ID | Folder | Title | Architectural Highlights |
| :--- | :--- | :--- | :--- |
| **01** | [`01-blog-api`](./01-blog-api/README.md) | **Headless Blog & CMS API** | Layered DDD, TypeORM, PostgreSQL 16, Passport JWT, RBAC, Swagger. |
| **02** | [`02-auth-service`](./02-auth-service/README.md) | **Centralized Auth & IdP** | Argon2id, JWT Rotation, Redis Blacklist, WebAuthn / Passkeys. |
| **03** | [`03-task-manager-api`](./03-task-manager-api/README.md) | **Collaborative Task Manager** | Prisma ORM, Kanban columns, Server-Sent Events (SSE) streaming. |
| **04** | [`04-ecommerce-backend`](./04-ecommerce-backend/README.md) | **High-Scale E-Commerce API** | Pessimistic locking, Redis caching, Stripe Webhooks. |
| **05** | [`05-url-shortener`](./05-url-shortener/README.md) | **URL Shortener & Telemetry** | Fastify 5 adapter (>50k req/s), Redis caching, Base62. |
| **06** | [`06-file-upload-service`](./06-file-upload-service/README.md) | **Cloud Ingestion Gateway** | S3 Presigned URLs, Multer streaming, BullMQ Sharp thumbnails. |
| **07** | [`07-notification-service`](./07-notification-service/README.md) | **Multi-Channel Dispatcher** | BullMQ queues, Nodemailer SMTP, Twilio SMS, Webhook retries. |
| **08** | [`08-realtime-chat`](./08-realtime-chat/README.md) | **Realtime Collaborative Chat** | Socket.IO, `@socket.io/redis-adapter` multi-pod scaling, JWT WsGuard. |
| **09** | [`09-multi-tenant-saas-api`](./09-multi-tenant-saas-api/README.md) | **Multi-Tenant SaaS API** | PostgreSQL Row-Level Security (RLS), AsyncLocalStorage tenancy. |
| **10** | [`10-graphql-api`](./10-graphql-api/README.md) | **Code-First GraphQL API** | Apollo Server 4, DataLoader N+1 solving, Complexity limiting. |
| **11** | [`11-microservices-system`](./11-microservices-system/README.md) | **Distributed Microservices** | API Gateway, RabbitMQ events, gRPC synchronous Protobuf RPC. |
| **12** | [`12-cqrs-event-sourced-bank`](./12-cqrs-event-sourced-bank/README.md) | **CQRS & Event-Sourced Bank** | `@nestjs/cqrs`, Event stream replay, Snapshots, Financial ledger. |
| **13** | [`13-ai-rag-api`](./13-ai-rag-api/README.md) | **AI Knowledge Base & RAG** | OpenAI embeddings, `pgvector` HNSW search, Cohere ReRank, SSE stream. |
| **14** | [`14-booking-system`](./14-booking-system/README.md) | **Realtime Booking Engine** | Redis Redlock, Pessimistic DB locks, BullMQ delayed holds. |
| **15** | [`15-analytics-ingestion-pipeline`](./15-analytics-ingestion-pipeline/README.md) | **Analytics Ingestion Engine** | Kafka Transporter, 100k events/s, PostgreSQL Range Partitioning. |
