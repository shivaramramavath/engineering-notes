# Module 18: System Design Blueprints

Welcome to **Module 18: System Design Blueprints**. This module provides 15 comprehensive architectural blueprints for large-scale distributed systems engineered with Spring Boot, Java 21/25, and modern enterprise middleware.

---

## Blueprints Catalog

| # | Blueprint | Scale & Key Focus | Level | Status |
|---|---|---|---|---|
| 01 | [01-system-design-approach.md](file:///03-backend/spring-boot/18-system-design/01-system-design-approach.md) | Structured 4-step framework for backend system design interviews | Beginner | Complete |
| 02 | [02-scalable-rest-api.md](file:///03-backend/spring-boot/18-system-design/02-scalable-rest-api.md) | Scaling Spring Boot REST APIs to 100k QPS: L4/L7 load balancing, caching, read replicas | Intermediate | Complete |
| 03 | [03-authentication-system.md](file:///03-backend/spring-boot/18-system-design/03-authentication-system.md) | Distributed identity provider: OAuth2, OIDC, JWT key rotation, Redis revocation | Intermediate | Complete |
| 04 | [04-notification-system.md](file:///03-backend/spring-boot/18-system-design/04-notification-system.md) | Multi-channel notification engine (Push, SMS, Email, In-app), Kafka priority streams | Intermediate | Complete |
| 05 | [05-file-upload-system.md](file:///03-backend/spring-boot/18-system-design/05-file-upload-system.md) | Direct S3 presigned multipart uploads, async thumbnail workers, CloudFront CDN | Intermediate | Complete |
| 06 | [06-job-queue-system.md](file:///03-backend/spring-boot/18-system-design/06-job-queue-system.md) | Distributed asynchronous job processing with RabbitMQ, Dead Letter Queues, retries | Intermediate | Complete |
| 07 | [07-realtime-chat-system.md](file:///03-backend/spring-boot/18-system-design/07-realtime-chat-system.md) | 5M concurrent WebSockets, STOMP message routing, Redis pub/sub, ScyllaDB history | Advanced | Complete |
| 08 | [08-ecommerce-backend.md](file:///03-backend/spring-boot/18-system-design/08-ecommerce-backend.md) | High-contention flash sale checkout, Redis atomic inventory reservation, ledger DB | Advanced | Complete |
| 09 | [09-url-shortener.md](file:///03-backend/spring-boot/18-system-design/09-url-shortener.md) | 1B redirects daily: Base62 encoding, KGS service, Redis 307 redirect cache | Intermediate | Complete |
| 10 | [10-rate-limiter.md](file:///03-backend/spring-boot/18-system-design/10-rate-limiter.md) | Distributed Sliding Window Counter via Redis Lua scripts, fail-open resilience | Advanced | Complete |
| 11 | [11-payment-system-and-idempotency.md](file:///03-backend/spring-boot/18-system-design/11-payment-system-and-idempotency.md) | Double-entry bookkeeping ledger, strict Idempotency-Key headers, Stripe webhooks | Advanced | Complete |
| 12 | [12-search-service.md](file:///03-backend/spring-boot/18-system-design/12-search-service.md) | Full-text search with OpenSearch, real-time CDC sync via Debezium & Kafka | Advanced | Complete |
| 13 | [13-booking-and-inventory-reservation.md](file:///03-backend/spring-boot/18-system-design/13-booking-and-inventory-reservation.md) | Seat reservation with optimistic locking (`@Version`), distributed Redis holds | Advanced | Complete |
| 14 | [14-social-feed.md](file:///03-backend/spring-boot/18-system-design/14-social-feed.md) | Fan-out on write vs fan-out on read, hybrid timeline generation, Redis sorted sets | Advanced | Complete |
| 15 | [15-microservices-system.md](file:///03-backend/spring-boot/18-system-design/15-microservices-system.md) | Distributed microservices gateway, service mesh, saga orchestrator, distributed tracing | Advanced | Complete |

---

## Next Steps
Proceed to **[Module 19: Technical Interview Preparation](file:///03-backend/spring-boot/19-interview/)** to review senior and staff engineering Q&A sets.
