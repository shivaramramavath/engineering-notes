# Module 10: Backend System Architecture in Node.js

> Clean Architecture, Hexagonal boundaries, Dependency Injection, CQRS, and Domain-Driven Design for enterprise backends.

---

## Module Overview

Enterprise Node.js applications succeed or fail based on architectural separation of concerns. This module covers structuring applications to survive framework churn, decoupling business rules from databases, and managing complexity with DDD and CQRS patterns.

---

## Contents

| Index | Note | Core Focus |
| :---: | :--- | :--- |
| **01** | [01-mvc-and-layered-architecture.md](./01-mvc-and-layered-architecture.md) | Traditional MVC, 3-tier layering (Presentation $\rightarrow$ Service $\rightarrow$ Data). |
| **02** | [02-clean-architecture.md](./02-clean-architecture.md) | Uncle Bob's Clean Architecture, dependency inversion principle, use case boundaries. |
| **03** | [03-repository-and-service-pattern.md](./03-repository-and-service-pattern.md) | Repository interfaces, service orchestrations, transaction boundaries. |
| **04** | [04-dependency-injection.md](./04-dependency-injection.md) | Constructor injection vs containers (InversifyJS, tsyringe), test doubles. |
| **05** | [05-modular-monolith-vs-microservices.md](./05-modular-monolith-vs-microservices.md) | Boundary design, domain packaging, migration pathways from monolith to microservices. |
| **06** | [06-hexagonal-architecture-and-ports-adapters.md](./06-hexagonal-architecture-and-ports-adapters.md) | Ports & Adapters, decoupling external infrastructure from pure business rules. |
| **07** | [07-cqrs-and-event-sourcing.md](./07-cqrs-and-event-sourcing.md) | Command/Query segregation, append-only event stores, state reconstruction. |
| **08** | [08-domain-driven-design-with-node.md](./08-domain-driven-design-with-node.md) | Entities, Value Objects, Aggregates, Domain Events in TypeScript. |

---

## Next Steps
Proceed to [11-async-processing](../11-async-processing/README.md) to explore background job queues, BullMQ, RabbitMQ, Kafka, and the Outbox pattern.
