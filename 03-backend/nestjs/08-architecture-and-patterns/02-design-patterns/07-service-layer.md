---
title: "The Service Layer Pattern in NestJS"
description: "Structuring enterprise service layers: Application Services vs Domain Services vs Infrastructure Services"
tags: [nestjs, design-patterns, service-layer, business-logic, transaction-script]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# The Service Layer Pattern in NestJS

## Conceptual Overview
The Service Layer pattern defines an application's boundary with a layer of services that coordinates operations and encapsulates responses.

In enterprise NestJS architectures, differentiate between three types of services:
1. **Application Services**: Manage use cases, transactions, security context, and orchestration.
2. **Domain Services**: Contain pure business logic that does not naturally belong to a single Entity.
3. **Infrastructure Services**: Wrap I/O operations (file storage, email, third-party APIs).

---

## Key Takeaways & Cheat Sheet

- Never put database queries or raw SQL directly in controllers.
- Separate Application Services (orchestration) from Domain Services (pure business logic).

---

## Cross-References
- [01-layered-architecture.md](../01-architecture/01-layered-architecture.md) - Layered architecture
- [05-facade.md](./05-facade.md) - Facade pattern
