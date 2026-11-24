# Module 05: NestJS Advanced Systems Engineering

> Advanced distributed systems engineering with NestJS 11: caching, distributed job queues, event-driven messaging, real-time WebSockets, GraphQL, microservices, and AI/LLM integration.

---

## Sub-Modules & Topics

### 01. Caching ([01-caching](01-caching/README.md))
- **Keyv Architecture**: `cache-manager` v6 and `@keyv/redis` integration in NestJS 11.
- **Cache-Aside & Interceptors**: Response caching with `@UseInterceptors(CacheInterceptor)` and manual cache stores.
- **Distributed Invalidation**: Tag-based cache invalidation, TTL strategies, and cache stampede protection.

### 02. Background Processing & Scheduling ([02-background-processing](02-background-processing/README.md))
- **BullMQ**: Distributed Redis job queues, rate limiting, parent-child flows, and sandboxed worker processors.
- **Scheduling**: Cron expressions, intervals, dynamic timezone scheduling, and distributed cron locks with Redis.

### 03. Events & Messaging ([03-events-and-messaging](03-events-and-messaging/README.md))
- **EventEmitter2**: In-process asynchronous event dispatching with `@OnEvent()`.
- **Message Brokers**: High-throughput distributed streaming with Apache Kafka and RabbitMQ exchanges/queues.
- **CQRS Module**: `@nestjs/cqrs` command buses, query buses, domain event publishing, and long-running Sagas.

### 04. Real-Time & WebSockets ([04-realtime](04-realtime/README.md))
- **Gateways**: `@WebSocketGateway()`, Socket.IO, and raw `ws` platform adapters.
- **Scaling**: Redis adapter clustering for multi-node WebSocket fan-out, room multiplexing, and Server-Sent Events (SSE).

### 05. GraphQL ([05-graphql](05-graphql/README.md))
- **Code-First & Schema-First**: Apollo Server, Yoga adapter, TypeScript decorators, and SDL schemas.
- **DataLoader**: Eliminating the N+1 problem in GraphQL resolvers with microtask batching.
- **Apollo Federation**: Multi-subgraph federated architecture and schema complexity limiting.

### 06. Microservices ([06-microservices](06-microservices/README.md))
- **Transports**: TCP, Redis, NATS, MQTT, RabbitMQ, Kafka, and gRPC.
- **Client & Server**: `@MessagePattern()`, `@EventPattern()`, `ClientProxy`, and transporter `.unwrap()`.
- **Hybrid Applications**: Combining HTTP REST and microservice listeners in a single process.

### 07. Integrations ([07-integrations](07-integrations/README.md))
- **External Services**: Stripe payments with raw webhook signatures, Twilio SMS, and Firebase Admin.
- **Search Engines**: Elasticsearch and Meilisearch integration for full-text search.

### 08. AI & Large Language Models ([08-ai-and-llm](08-ai-and-llm/README.md))
- **LLM Integrations**: OpenAI, Anthropic, and LangChain integration in NestJS services.
- **RAG & Vector Embeddings**: Retrieval-Augmented Generation using PostgreSQL `pgvector`, streaming responses, and cost/rate control.
