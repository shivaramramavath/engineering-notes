---
title: "Modular Monolith Architecture in NestJS"
description: "Architecting scalable modular monoliths: bounded contexts, module boundaries, encapsulation, and circular dependency prevention"
tags: [nestjs, architecture, modular-monolith, bounded-context, encapsulation]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Modular Monolith Architecture in NestJS

## Conceptual Overview
Before prematurely decomposing a system into distributed microservices, a **Modular Monolith** provides the optimal balance of developer velocity, operational simplicity, and architectural discipline.

A Modular Monolith structures a single deployable artifact into strictly encapsulated domain modules mirroring Domain-Driven Design (DDD) Bounded Contexts. Modules expose a formal public API through `exports: [...]` while keeping internal domain services, entities, and repositories private.

```
+-------------------------------------------------------------------------------+
|                       Modular Monolith Architecture                           |
|                                                                               |
|  [ OrdersModule ]             [ BillingModule ]             [ ShippingModule ] |
|  - OrdersController           - InvoicesController          - TrackingController|
|  - OrdersService (Public API) - BillingService (Public API) - ShippingService   |
|  - OrdersRepo (Private)       - InvoicesRepo (Private)      - ParcelsRepo (Priv)|
|        |                             |                             |          |
|        +-----------------------------+-----------------------------+          |
|                                      v                                        |
|                          [ Event Bus / In-Memory ]                            |
+-------------------------------------------------------------------------------+
```

---

## Core Mechanisms & Architecture

### 1. Public API vs Internal Implementation
A well-architected NestJS module should expose only high-level facades or interfaces:
```typescript
@Module({
  imports: [TypeOrmModule.forFeature([InternalOrderEntity])],
  providers: [OrdersFacade, InternalOrdersRepository, InternalCalculatorService],
  exports: [OrdersFacade], // Only OrdersFacade is accessible to other modules
})
export class OrdersModule {}
```

Any attempt by `BillingModule` to inject `InternalOrdersRepository` directly will result in a NestJS DI compile-time resolution error.

---

## Key Takeaways & Cheat Sheet

- Treat NestJS `@Module` boundaries as strict architectural fences.
- Export only public facade services; keep database entities and internal helpers private.
- Communicate between modules using asynchronous events (`EventEmitter2`) to decouple workflows.
- Modular monoliths eliminate network latency and distributed transaction failures of microservices.

---

## Cross-References
- [01-layered-architecture.md](./01-layered-architecture.md) - Layered architecture
- [07-monolith-vs-microservices.md](./07-monolith-vs-microservices.md) - Monolith vs microservices tradeoffs
