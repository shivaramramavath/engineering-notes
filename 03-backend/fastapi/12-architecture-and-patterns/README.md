# Module 12: Architecture & Software Patterns

Welcome to **Module 12: Architecture & Software Patterns**. This module explores enterprise application architecture, structural boundaries, domain-driven design, and maintainable software patterns specifically tailored for FastAPI backends.

---

## Module Overview

| File | Title | Description |
| :--- | :--- | :--- |
| [01-layered-architecture-for-fastapi.md](./01-layered-architecture-for-fastapi.md) | **Layered Architecture** | Presentation, service, repository, and domain layer separation. |
| [02-service-layer-and-repository.md](./02-service-layer-and-repository.md) | **Service Layer & Repository Pattern** | Decoupling ORM queries from business rules and HTTP contracts. |
| [03-hexagonal-and-clean-architecture.md](./03-hexagonal-and-clean-architecture.md) | **Hexagonal & Clean Architecture** | Ports and adapters, core domain independence, and dependency inversion. |
| [04-domain-driven-design-with-python.md](./04-domain-driven-design-with-python.md) | **Domain-Driven Design (DDD)** | Aggregates, value objects, domain entities, and domain events. |
| [05-cqrs-and-event-sourcing.md](./05-cqrs-and-event-sourcing.md) | **CQRS & Event Sourcing** | Command-query responsibility segregation and append-only event stores. |
| [06-modular-monolith-vs-microservices.md](./06-modular-monolith-vs-microservices.md)| **Modular Monolith vs Microservices** | Bounded contexts within a single codebase vs distributed network boundaries. |
| [07-dependency-inversion-and-protocols.md](./07-dependency-inversion-and-protocols.md)| **Dependency Inversion & Protocols** | Structural typing with `typing.Protocol` and dependency injection swapping. |
| [08-dto-vs-entity-vs-domain-models.md](./08-dto-vs-entity-vs-domain-models.md) | **DTO vs Entity vs Domain Models** | Boundaries between Pydantic schemas, SQLAlchemy models, and pure domain dataclasses. |
| [09-design-patterns-in-fastapi.md](./09-design-patterns-in-fastapi.md) | **GoF Patterns in FastAPI** | Strategy, Factory, Adapter, Decorator, Facade, Unit of Work, and Chain of Responsibility. |
| [10-error-handling-architecture.md](./10-error-handling-architecture.md) | **Error Handling Architecture** | Domain exceptions, mapped HTTP handlers, and uniform RFC 7807 responses. |
| [11-feature-flags.md](./11-feature-flags.md) | **Feature Flags & Toggles** | Dynamic configuration, percentage rollouts, and runtime dependency switching. |
| [12-multi-tenancy-architecture.md](./12-multi-tenancy-architecture.md) | **Multi-Tenancy Architecture** | Shared DB shared schema, schema-per-tenant, and DB-per-tenant strategies. |
| [13-real-world-patterns.md](./13-real-world-patterns.md) | **Real-World Resilience Patterns** | Circuit breakers, distributed locks, idempotency keys, and audit logging. |
| [14-clean-code-and-code-review-checklist.md](./14-clean-code-and-code-review-checklist.md)| **Clean Code & Review Checklist** | Production readiness, Ruff rules, typing strictness, and architectural smell checklists. |
