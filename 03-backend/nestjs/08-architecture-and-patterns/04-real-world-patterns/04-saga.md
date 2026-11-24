---
title: "The Saga Pattern in NestJS Distributed Systems"
description: "Managing distributed transactions, eventual consistency, and compensating transactions across microservices using Sagas"
tags: [nestjs, patterns, saga, distributed-transactions, eventual-consistency, compensation]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# The Saga Pattern in NestJS Distributed Systems

## Conceptual Overview
In microservice architectures where each service maintains its own database, traditional two-phase commit (2PC) is impractical due to high latency and coordinator failure risks.

The **Saga Pattern** manages distributed transactions as a sequence of local transactions:
1. Each local transaction updates the database and publishes an event.
2. If any local transaction fails, the Saga coordinates **compensating transactions** in reverse order to undo changes and restore data consistency.

---

## Orchestrated Saga Workflow

```
CreateOrder (OrdersService)
    |
    v (Success)
AuthorizePayment (PaymentService)
    |
    v (FAIL: Insufficient Funds)
Compensate: CancelOrder (OrdersService)
```

---

## Key Takeaways & Cheat Sheet

- Use Sagas for multi-service transactions requiring eventual consistency.
- Every forward action must have an idempotent compensating action.
- Sagas can be Choreographed (event-driven) or Orchestrated (central coordinator).

---

## Cross-References
- [03-transactional-outbox.md](./03-transactional-outbox.md) - Transactional outbox
- [05-retry-and-circuit-breaker.md](./05-retry-and-circuit-breaker.md) - Resilience
