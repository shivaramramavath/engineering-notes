---
title: "Microservices and Messaging Best Practices in NestJS"
description: "Designing reliable distributed microservices: idempotent consumers, dead-letter queues, and correlation tracking"
tags: [nestjs, best-practices, microservices, messaging, idempotency, dead-letter-queues]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Microservices and Messaging Best Practices in NestJS

## Core Principles
1. **Idempotent Consumers**: Design every message and event handler so receiving duplicate messages produces no unintended side effects.
2. **Configure Dead-Letter Queues (DLQ)**: Poison pill messages that fail repeatedly must be quarantined to a DLQ rather than blocking the broker.
3. **Propagate Correlation IDs**: Ensure all microservice calls carry `X-Correlation-ID`.

---

## Cross-References
- [01-microservices-architecture.md](../../05-advanced/06-microservices/01-microservices-architecture.md) - Microservices architecture
