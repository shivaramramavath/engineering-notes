# Module 17: Production Reference Projects Catalog

Welcome to **Module 17: Production Reference Projects**. This module contains 16 complete, enterprise-grade reference applications demonstrating production patterns, strict type-safety, clean architecture, automated testing with Testcontainers, multi-stage Dockerfiles, and compose configurations.

---

## Reference Architecture Matrix

| # | Project Code | Project Title | Key Architectural Patterns & Tech Stack | Complexity |
|---|---|---|---|---|
| **01** | `SPRING-PROJ-001` | [REST API CRUD with PostgreSQL](file:///03-backend/spring-boot/17-projects/01-rest-api-crud-with-postgres.md) | Spring Data JPA, HikariCP, Flyway, DTO records, ControllerAdvice | Beginner |
| **02** | `SPRING-PROJ-002` | [Blog & Publishing Platform](file:///03-backend/spring-boot/17-projects/02-blog-platform.md) | Many-to-many relationships, Keyset pagination, Markdown sanitization, DTO projections | Intermediate |
| **03** | `SPRING-PROJ-003` | [Auth Server with JWT & OAuth2](file:///03-backend/spring-boot/17-projects/03-auth-server-with-jwt-and-oauth2.md) | Spring Authorization Server, JWT token rotation, Argon2, Redis blocklist | Intermediate |
| **04** | `SPRING-PROJ-004` | [High-Concurrency E-Commerce Backend](file:///03-backend/spring-boot/17-projects/04-ecommerce-backend.md) | Pessimistic locking (`PESSIMISTIC_WRITE`), Transactional Outbox, Stripe webhooks | Advanced |
| **05** | `SPRING-PROJ-005` | [Task Manager with Dynamic RBAC](file:///03-backend/spring-boot/17-projects/05-task-manager-with-rbac.md) | Granular permission evaluators, `@PreAuthorize`, method security, audit logging | Intermediate |
| **06** | `SPRING-PROJ-006` | [High-Performance URL Shortener](file:///03-backend/spring-boot/17-projects/06-url-shortener-with-redis.md) | Base62 encoding, Redis 307 redirect cache, async click analytics batch flush | Intermediate |
| **07** | `SPRING-PROJ-007` | [Secure File Storage Service](file:///03-backend/spring-boot/17-projects/07-file-storage-service.md) | AWS S3 Presigned URLs, streaming downloads, Magic Byte MIME validation | Intermediate |
| **08** | `SPRING-PROJ-008` | [Multi-Channel Notification Service](file:///03-backend/spring-boot/17-projects/08-notification-service.md) | Apache Kafka, RabbitMQ, priority queues, consumer groups, email/SMS dispatch | Advanced |
| **09** | `SPRING-PROJ-009` | [Real-Time Chat with WebSockets & STOMP](file:///03-backend/spring-boot/17-projects/09-realtime-chat-with-websockets.md) | WebSockets, STOMP message broker, Redis pub/sub multi-node synchronization | Advanced |
| **10** | `SPRING-PROJ-010` | [Multi-Tenant SaaS with Schema Isolation](file:///03-backend/spring-boot/17-projects/10-multi-tenant-saas-api.md) | Schema-per-tenant, `CurrentTenantIdentifierResolver`, dynamic Flyway migration | Advanced |
| **11** | `SPRING-PROJ-011` | [Microservices System](file:///03-backend/spring-boot/17-projects/11-microservices-system.md) | Spring Cloud Gateway, Resilience4j, Distributed Tracing, Eureka / K8s DNS | Advanced |
| **12** | `SPRING-PROJ-012` | [High-Volume Batch ETL Pipeline](file:///03-backend/spring-boot/17-projects/12-spring-batch-etl-pipeline.md) | Spring Batch 5+, chunk-oriented processing, skip/retry listeners, partitioned steps | Advanced |
| **13** | `SPRING-PROJ-013` | [GraphQL API with Spring GraphQL](file:///03-backend/spring-boot/17-projects/13-graphql-api.md) | Spring GraphQL, BatchMapping (DataLoader N+1 prevention), Schema validation | Intermediate |
| **14** | `SPRING-PROJ-014` | [Event-Driven Order Processing with Modulith](file:///03-backend/spring-boot/17-projects/14-event-driven-order-processing-with-modulith.md) | Modular monolith, architectural verification, persistent outbox event publication | Advanced |
| **15** | `SPRING-PROJ-015` | [RAG Knowledge Base API with Spring AI](file:///03-backend/spring-boot/17-projects/15-rag-api-with-spring-ai.md) | Document readers, token chunking, PostgreSQL pgvector VectorStore, ChatClient | Advanced |
| **16** | `SPRING-PROJ-016` | [Seat Reservation with Concurrency Control](file:///03-backend/spring-boot/17-projects/16-booking-system-with-concurrency-control.md) | Optimistic locking (`@Version`), Redis distributed mutex hold, HTTP 409 conflict | Advanced |

---

## Next Steps
Proceed to **[Module 18: System Design Blueprints](file:///03-backend/spring-boot/18-system-design/)** to study 15 distributed system architectures.
