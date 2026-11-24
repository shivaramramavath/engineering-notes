# Module 11: System Design with NestJS

Welcome to the **System Design** curriculum module. This track covers the architectural blueprints, scalability strategies, and trade-off analyses required to design high-throughput, fault-tolerant enterprise systems with NestJS.

---

## System Design Blueprints

| ID | Topic | Architectural Focus |
| :--- | :--- | :--- |
| **01** | [System Design Approach](./01-system-design-approach.md) | The 5-step framework for system design interviews. |
| **02** | [Scalable REST API](./02-scalable-rest-api.md) | 100k QPS REST API with Fastify and Redis. |
| **03** | [Authentication System](./03-authentication-system.md) | Centralized IdP, RS256 JWT, Passkeys, and Redis revocation. |
| **04** | [Notification Engine](./04-notification-system.md) | BullMQ multi-channel notification dispatcher (50M/day). |
| **05** | [File Ingestion Pipeline](./05-file-upload-system.md) | S3 presigned URLs, multipart uploads, and virus scanning. |
| **06** | [Job Queue System](./06-job-queue-system.md) | BullMQ distributed queues, delayed jobs, and DLQs. |
| **07** | [Realtime Chat Platform](./07-realtime-chat-system.md) | 1M concurrent WebSockets with Socket.IO & Redis adapter. |
| **08** | [E-Commerce Flash Sale](./08-ecommerce-backend.md) | Concurrency control, Redis pre-allocation, and Stripe webhooks. |
| **09** | [URL Shortener](./09-url-shortener.md) | Base62 encoding, Fastify 5 redirects, and click analytics. |
| **10** | [Distributed Rate Limiter](./10-rate-limiter.md) | Redis sliding-window throttler and multi-tier quotas. |
| **11** | [Microservices System](./11-microservices-system.md) | API Gateway, RabbitMQ events, and gRPC internal RPC. |
| **12** | [Social Media Feed](./12-social-media-feed.md) | Fan-out on write vs read, Redis activity timelines. |
| **13** | [Payment Gateway](./13-payment-processing-system.md) | Idempotency keys, transactional outbox, and Stripe. |
| **14** | [Distributed Search Engine](./14-distributed-search-engine.md) | Elasticsearch, PostgreSQL CDC, and autocomplete trie. |
| **15** | [Booking Engine](./15-hotel-seat-booking-system.md) | Redlock distributed locking and temporary seat holds. |
| **16** | [Collaborative Document Editor](./16-collaborative-document-editing.md) | CRDTs (Yjs) and realtime WebSocket synchronization. |
| **17** | [Video Transcoding Pipeline](./17-video-transcoding-pipeline.md) | S3 uploads, FFmpeg GPU workers, and HLS streaming. |
| **18** | [Realtime Analytics Pipeline](./18-realtime-analytics-pipeline.md) | Kafka ingestion, ClickHouse OLAP, 100k events/sec. |
| **19** | [API Gateway and BFF](./19-api-gateway-and-bff.md) | Backend-for-Frontend pattern, protocol translation. |
