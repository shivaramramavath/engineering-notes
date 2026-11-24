# NestJS Learning Roadmaps & Specialization Tracks

> Structured learning roadmaps tailored for backend engineers at various career stages, complete with recommended modules, dependencies, and estimated completion times.

---

## 1. Track 1: Foundations to Production REST API (Beginner to Intermediate)
**Target**: Junior to Mid-level Backend Engineers  
**Time Commitment**: 3 to 4 Weeks (~35 Hours)  
**Goal**: Build, validate, test, and containerize production-ready RESTful APIs using standard NestJS patterns.

```
00-prerequisites ──> 01-getting-started ──> 02-fundamentals ──> 03-core-concepts ──> 04-intermediate (Auth & DB)
```

1. **Step 1: Foundational Prerequisites** (`00-prerequisites` - 4h)
   - Understand decorators, metadata reflection (`Reflect.getMetadata`), and the Node.js event loop.
2. **Step 2: CLI & Bootstrapping** (`01-getting-started` - 4h)
   - Master Nest CLI code generators, project structure, SWC compilation, and debugging.
3. **Step 3: Core Framework Building Blocks** (`02-fundamentals` - 6h)
   - Modules, Controllers, Providers, and Dependency Injection container mechanics.
4. **Step 4: The Request Pipeline** (`03-core-concepts` - 8h)
   - Middleware, Guards, Interceptors, Pipes (`ValidationPipe`, `ParseDatePipe`), and Exception Filters.
5. **Step 5: Database & Persistence** (`04-intermediate/02-database-foundations` & `03-typeorm` / `04-prisma` - 8h)
   - Schema modeling, migrations, connection pools, and repository injection.
6. **Step 6: Authentication & Authorization** (`04-intermediate/06-authentication` & `07-authorization` - 5h)
   - Passport JWT strategy, refresh token rotation, and RBAC guards.

---

## 2. Track 2: Enterprise Architecture & Domain-Driven Design (Senior Engineer)
**Target**: Senior Backend Engineers, Technical Leads  
**Time Commitment**: 4 Weeks (~40 Hours)  
**Goal**: Master Hexagonal Architecture, Modular Monoliths, CQRS, transactional outboxes, and advanced DI.

```
08-architecture ──> 03-core-concepts (Scopes & DI) ──> 04-intermediate (MikroORM) ──> 05-advanced (CQRS)
```

1. **Step 1: Architectural Paradigms** (`08-architecture-and-patterns/01-architecture` - 8h)
   - Modular Monolith, Clean/Hexagonal Architecture, and Ports & Adapters.
2. **Step 2: Domain-Driven Design in NestJS** (`08-architecture-and-patterns/01-architecture/04-ddd` - 8h)
   - Entities, Aggregates, Value Objects, and Domain Events.
3. **Step 3: Unit of Work & Identity Map with MikroORM** (`04-intermediate/02-database-foundations/03-mikro-orm.md` - 6h)
   - Fine-grained transaction management and entity state tracking.
4. **Step 4: CQRS & Event Sourcing** (`05-advanced/03-events-and-messaging/05-cqrs-module.md` - 10h)
   - Command buses, Query buses, Event handlers, and Sagas.
5. **Step 5: Distributed Reliability Patterns** (`08-architecture-and-patterns/04-real-world-patterns` - 8h)
   - Transactional Outbox pattern, Idempotency keys, and Distributed locks with Redis.

---

## 3. Track 3: Distributed Microservices & Event-Driven Systems (Staff / Cloud Architect)
**Target**: Staff Engineers, Distributed Systems Architects  
**Time Commitment**: 3 to 4 Weeks (~35 Hours)  
**Goal**: Design, deploy, and observe decoupled microservices communicating over gRPC, Kafka, and RabbitMQ.

```
05-advanced (06-microservices) ──> 05-advanced (03-events) ──> 07-production (03-observability) ──> 11-system-design
```

1. **Step 1: Transport Protocols** (`05-advanced/06-microservices` - 8h)
   - TCP, gRPC, Hybrid Applications, and Transporter `.unwrap()`.
2. **Step 2: Message Brokers (Kafka & RabbitMQ)** (`05-advanced/03-events-and-messaging` - 8h)
   - Consumer groups, partitions, dead-letter exchanges, and schema evolution.
3. **Step 3: Background Task Queues with BullMQ** (`05-advanced/02-background-processing` - 6h)
   - Rate-limited queues, parent-child flows, and sandboxed job processors.
4. **Step 4: Distributed Observability** (`07-production/03-observability` - 5h)
   - OpenTelemetry distributed tracing, correlation IDs (`X-Request-Id`), and Prometheus metrics.
5. **Step 5: High-Scale System Design** (`11-system-design` - 8h)
   - Architecting large-scale distributed payment and reservation systems.

---

## 4. Track 4: High Performance, Security & Production Operations
**Target**: DevOps Engineers, SREs, Production Engineers  
**Time Commitment**: 2 Weeks (~20 Hours)  
**Goal**: Hardening NestJS applications for maximum throughput, minimal latency, and zero-downtime operations.

```
02-fundamentals (Fastify) ──> 06-internals ──> 07-production (Security, Perf, Deploy) ──> 09-best-practices
```

1. **Step 1: High-Performance Engine Tuning** (`02-fundamentals/11-platform-express-vs-fastify.md` - 4h)
   - Benchmarking Fastify 5 vs Express 5, schema compilation, and zero-allocation context.
2. **Step 2: Security Hardening** (`07-production/01-security` - 5h)
   - OWASP API Security Top 10, Helmet, CORS, CSRF, ReDoS mitigation, and timing-safe crypto.
3. **Step 3: Framework Internals & Bootstrap** (`06-internals` - 4h)
   - V8 memory profiling, DI compiler walkthrough, and lifecycle hook ordering.
4. **Step 4: Production Deployment & Containerization** (`07-production/04-deployment` - 4h)
   - Multi-stage Docker, Distroless images, Kubernetes health probes, and graceful SIGTERM shutdown.
5. **Step 5: Runbooks & Incident Response** (`07-production/05-operations` - 3h)
   - Heap dump analysis, memory leak diagnosis, and config rotation runbooks.

---

## 5. Track 5: Technical Interview & Staff Coding Preparation
**Target**: Engineers preparing for Senior, Staff, and Lead Technical Interviews  
**Time Commitment**: 2 Weeks (~20 Hours)

1. **Module 12: Interview Preparation** (`12-interview-preparation` - 10h)
   - Master libuv execution order, DI scope implications, and 10 tricky output-prediction questions.
2. **Module 10: Reference Projects** (`10-projects` - 6h)
   - Review architectural decisions across all 15 reference projects.
3. **Module 13: Quick Reference & Cheatsheets** (`13-quick-reference` - 4h)
   - Review 25 high-density cards for rapid syntax recall during live-coding rounds.
