---
title: "Event-Driven Architecture (EDA) in NestJS"
description: "Asynchronous event distribution, choreography vs orchestration, Outbox pattern, and decoupling microservices with events"
tags: [nestjs, architecture, event-driven, eda, choreography, orchestration]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Event-Driven Architecture (EDA) in NestJS

## Conceptual Overview
Event-Driven Architecture decouples software systems by emitting and reacting to events asynchronously. Producers publish facts (`PaymentProcessed`) without knowing which downstream services consume them.

Two coordination models exist:
- **Choreography**: Each service reacts to events independently and publishes new events. Low coupling, but distributed flow tracing is complex.
- **Orchestration**: A central coordinator (Saga orchestrator) directs workflow steps explicitly.

---

## Key Takeaways & Cheat Sheet

- Use EDA to eliminate synchronous coupling between services.
- Guarantee at-least-once message delivery using the Transactional Outbox pattern.
- Ensure event handlers are idempotent.

---

## Cross-References
- [01-event-emitter.md](../../05-advanced/03-events-and-messaging/01-event-emitter.md) - EventEmitter2
- [03-transactional-outbox.md](../04-real-world-patterns/03-transactional-outbox.md) - Transactional outbox
