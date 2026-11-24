---
title: "The Observer Pattern in NestJS"
description: "Publish-subscribe communication, RxJS Observables, Subject mechanics, and EventEmitter2 in NestJS"
tags: [nestjs, design-patterns, observer, rxjs, pubsub, eventemitter]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# The Observer Pattern in NestJS

## Conceptual Overview
The Observer pattern establishes a one-to-many dependency where state changes in a Subject automatically notify and update all registered Observers.

In NestJS:
- **RxJS Observables & Subjects**: Fundamental to interceptors, SSE streaming, and microservices.
- **EventEmitter2 (`@nestjs/event-emitter`)**: Decoupled domain event broadcasting.

---

## Key Takeaways & Cheat Sheet

- Use RxJS Subjects for in-process reactive streams.
- Use `EventEmitter2` for decoupled application domain event notification.

---

## Cross-References
- [01-event-emitter.md](../../05-advanced/03-events-and-messaging/01-event-emitter.md) - EventEmitter in depth
- [06-event-driven-architecture.md](../01-architecture/06-event-driven-architecture.md) - Event-driven architecture
