# Module 18: System Design Blueprints

Welcome to **Module 18: System Design Blueprints**. This module provides deep architectural breakdowns for designing high-throughput, fault-tolerant distributed systems using FastAPI, ASGI, and modern data infrastructure.

---

## Architectural Blueprints

| File | Title | Description |
| :--- | :--- | :--- |
| [01-system-design-approach.md](./01-system-design-approach.md) | **System Design Methodology** | Requirements gathering, capacity estimations, back-of-envelope math, and high-level design. |
| [02-scalable-rest-api.md](./02-scalable-rest-api.md) | **Scalable REST API Architecture** | Horizontally scaling FastAPI to 100k+ RPS with load balancers, caching, and read replicas. |
| [03-authentication-system.md](./03-authentication-system.md) | **Distributed Authentication System**| Designing an enterprise IAM with OAuth2, OIDC, JWT blacklists, and SSO. |
| [04-notification-system.md](./04-notification-system.md) | **High-Throughput Notification Engine**| Fan-out patterns, user preference engines, priority queues, and rate-limited dispatchers. |
| [05-file-upload-system.md](./05-file-upload-system.md) | **Large-Scale File Upload System**| Multipart chunked uploads, direct-to-S3 presigned transfers, virus scanning, and CDN delivery. |
| [06-job-queue-system.md](./06-job-queue-system.md) | **Distributed Job Queue Architecture**| At-least-once vs exactly-once delivery, delayed queues, dead-letter exchanges, and worker pools. |
| [07-realtime-chat-system.md](./07-realtime-chat-system.md) | **Global Realtime Chat System** | WebSockets connection scaling, Redis/Kafka pub-sub message buses, presence, and offline sync. |
| [08-ecommerce-backend.md](./08-ecommerce-backend.md) | **Distributed E-Commerce System** | Inventory reservation, Saga pattern for distributed transactions, and order state machines. |
| [09-url-shortener.md](./09-url-shortener.md) | **High-Throughput URL Shortener**| Base62 generation, distributed ID generators (Snowflake), Redis caching, and 301 redirects. |
| [10-rate-limiter.md](./10-rate-limiter.md) | **Distributed Rate Limiter** | Token bucket, sliding window log, sliding window counter algorithms using Redis and Lua. |
| [11-payment-system-and-idempotency.md](./11-payment-system-and-idempotency.md)| **Payment Gateway & Idempotency**| Idempotency keys, two-phase commits, reconciliation jobs, and ledger consistency. |
| [12-search-service.md](./12-search-service.md) | **Full-Text & Autocomplete Search**| Synchronizing PostgreSQL with Elasticsearch/OpenSearch via CDC (Debezium) and FastAPI query layers. |
| [13-ml-inference-platform.md](./13-ml-inference-platform.md) | **High-Volume ML Inference Platform**| GPU autoscaling, Triton/ONNX serving gateways, queue-based buffering, and model caching. |
| [14-analytics-ingestion.md](./14-analytics-ingestion.md) | **Real-Time Analytics Ingestion Engine**| Ingesting billions of event pings, Kafka ingestion buffers, and ClickHouse OLAP sinks. |
