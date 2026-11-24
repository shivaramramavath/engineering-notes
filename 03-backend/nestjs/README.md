# NestJS Enterprise Curriculum & Engineering Guide

> Comprehensive, production-grade learning track and reference manual for building scalable, resilient, enterprise-grade backend systems with NestJS 11+ and Node.js 22 LTS.

---

## Curriculum Overview

NestJS is a progressive Node.js framework for building efficient, reliable, and scalable server-side applications. Built with TypeScript and combining elements of Object-Oriented Programming (OOP), Functional Programming (FP), and Functional Reactive Programming (FRP), Nest provides an out-of-the-box application architecture that enables teams to create highly testable, loosely coupled, easily maintainable, and scalable backends.

This track covers the entire engineering spectrum: from dependency injection mechanics, request pipeline execution order, and database integrations (TypeORM, Prisma, MikroORM, Drizzle) to event-driven architectures, Kafka/RabbitMQ microservices, CQRS, GraphQL, automated OpenAPI/Swagger contracts, production security hardening, and high-concurrency real-time systems.

---

## Master Module Roadmap

```
00-prerequisites ──> 01-getting-started ──> 02-fundamentals ──> 03-core-concepts
                                                                       │
┌──────────────────────────────────────────────────────────────────────┘
▼
04-intermediate ──> 05-advanced ──> 06-internals ──> 07-production
                                                            │
┌───────────────────────────────────────────────────────────┘
▼
08-architecture ──> 09-best-practices ──> 10-projects ──> 11-system-design
                                                                 │
┌────────────────────────────────────────────────────────────────┘
▼
12-interview-preparation ──> 13-quick-reference
```

| Module | Level | Focus & Key Concepts |
| :--- | :---: | :--- |
| **[00-prerequisites](00-prerequisites/README.md)** | Foundational | TypeScript decorators, metadata reflection (`Reflect.getMetadata`), Node.js event loop, ESM/CJS, HTTP basics, SOLID primer. |
| **[01-getting-started](01-getting-started/README.md)** | Beginner | Installation, Nest CLI, project layout, dev environment, debugging, Nest 10 $\rightarrow$ 11 upgrade guide, SWC/Vitest, REPL, standalone CLIs. |
| **[02-fundamentals](02-fundamentals/README.md)** | Beginner | Modules, controllers, providers, services, dependency injection, decorators, request/response, Express 5 vs Fastify 5, API versioning, SSE, file streaming. |
| **[03-core-concepts](03-core-concepts/README.md)** | Intermediate | Request pipeline execution order, custom decorators, validation (class-validator & nestjs-zod), serialization, configuration, dynamic modules, provider scopes, lifecycle hooks. |
| **[04-intermediate](04-intermediate/README.md)** | Intermediate | Testing with Testcontainers, database foundations, TypeORM, Prisma, Mongoose, MikroORM, Drizzle, Authentication (Passport, JWT, 2FA, Passkeys), Authorization (RBAC, ABAC, CASL), OpenAPI/Swagger. |
| **[05-advanced](05-advanced/README.md)** | Advanced | Caching with Keyv / cache-manager v6, background queues (BullMQ), event-emitter, CQRS, WebSockets/Gateways, GraphQL (Apollo/Federation), Microservices (gRPC/Kafka/RabbitMQ), AI & LLM integration. |
| **[06-internals](06-internals/README.md)** | Advanced | Bootstrap sequence, DI container internals, metadata reflection, request lifecycle underneath, module compiler, writing custom platform adapters. |
| **[07-production](07-production/README.md)** | Senior | Security hardening (OWASP Top 10, Helmet, CORS, CSRF), performance tuning, Pino structured logging, OpenTelemetry, Terminus health checks, multi-stage Docker, Kubernetes, zero-downtime deployment. |
| **[08-architecture-and-patterns](08-architecture-and-patterns/README.md)** | Senior / Staff | Layered architecture, Modular Monolith, Clean/Hexagonal Architecture, Domain-Driven Design (DDD), CQRS and Event Sourcing, Gang of Four (GoF) patterns, transactional outbox, distributed locks. |
| **[09-best-practices](09-best-practices/README.md)** | Senior / Staff | Project conventions, naming standards, DTO/Entity separation, testing strategies, code review checklists, anti-patterns, troubleshooting DI resolution errors. |
| **[10-projects](10-projects/README.md)** | Practice | 15 enterprise reference systems (Auth service, E-commerce, Realtime chat, BullMQ notification engine, Multi-tenant SaaS, GraphQL, Microservices, CQRS bank, AI-RAG). |
| **[11-system-design](11-system-design/README.md)** | Staff | High-scale backend architectures: Scalable REST API, Social feed, Payment system with idempotency, Booking reservation, Real-time collaboration, Log analytics. |
| **[12-interview-preparation](12-interview-preparation/README.md)** | Career | Comprehensive interview guides, output-prediction challenges, scenario questions, Nest 11 updates, DI scope traps, and timed live-coding challenges. |
| **[13-quick-reference](13-quick-reference/README.md)** | Reference | 25 high-density syntax cheat sheets covering decorators, DI, testing, fastify, MikroORM, Drizzle, CQRS, BullMQ, and migrations. |

---

## Cross-Module Knowledge Boundaries

To ensure maximum depth without redundant duplication across tracks:
- **Node.js & Express Fundamentals**: See [03-backend/nodejs](../nodejs/README.md).
- **TypeScript Language & Typing Mechanics**: See [01-languages/typescript](../../01-languages/typescript/README.md).
- **SQL & Relational Theory**: See [09-sql](../../09-sql/README.md).
- **Redis In-Memory Internals**: See [03-backend/nodejs/07-databases/redis](../nodejs/07-databases/redis/README.md).
- **Docker, Kubernetes & CI/CD Pipelines**: See [04-devops](../../04-devops/README.md).
- **Authentication & Security Theory**: See [06-project-patterns/auth](../../06-project-patterns/auth/README.md).
- **System Design Principles**: See [07-system-design](../../07-system-design/README.md).
- **SOLID & Design Patterns Theory**: See [08-solid-design-patterns](../../08-solid-design-patterns/README.md).
- **Next.js Backend-For-Frontend (BFF)**: See [02-frontend/nextjs](../../02-frontend/nextjs/README.md).

---

## Baseline Specification

- **Framework**: NestJS 11.x (`@nestjs/core: ^11.0.0`, `@nestjs/common: ^11.0.0`)
- **Runtime**: Node.js >= 20.0.0 (Node.js 22 LTS "Jod" reference)
- **HTTP Platform**: Express 5.0 (default) / Fastify 5.0
- **TypeScript**: TypeScript 5.7+ with strict mode enabled
- **Format Standard**: 13-section concept format conforming to `_templates/concept-template.md`
- **Output Rule**: 100% Markdown-only repository files; all code contained in fenced blocks.
