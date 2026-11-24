# Module 08: Software Architecture & Design Patterns in NestJS

> Architectural engineering guide covering Layered Architecture, Modular Monoliths, Clean/Hexagonal Architecture, Domain-Driven Design (DDD), CQRS, Event Sourcing, and enterprise design patterns.

---

## Sub-Modules & Topics

### 01. Architecture ([01-architecture](01-architecture/README.md))
- **Layered Architecture**: Controller, Service, and Repository boundaries.
- **Modular Monolith**: Enforcing strict module boundaries and preventing circular dependency spaghetti.
- **Clean & Hexagonal Architecture**: Ports & Adapters, decoupling the domain core from NestJS, Express, and databases.
- **Domain-Driven Design (DDD)**: Entities, Value Objects, Aggregates, Domain Services, and Repositories.

### 02. Design Patterns ([02-design-patterns](02-design-patterns/README.md))
- **Creational & Structural Patterns**: Factory, Strategy, Adapter, Decorator, Facade, and Service Layer implementations.
- **Behavioral Patterns**: Observer, Specification, Unit of Work, Chain of Responsibility, and Template Method.

### 03. Clean Code & SOLID ([03-clean-code](03-clean-code/README.md))
- **SOLID in NestJS**: Single Responsibility, Open/Closed, Liskov Substitution, Interface Segregation, and Dependency Inversion.
- **Refactoring Antipatterns**: Fat controllers, anemic domain models, god services, and leaky abstractions.

### 04. Real-World Patterns ([04-real-world-patterns](04-real-world-patterns/README.md))
- **Distributed Reliability**: Transactional Outbox pattern, Saga pattern, Idempotency keys, Circuit Breaker with retry backoff, and distributed locking with Redis.
- **Enterprise Features**: Audit logging, soft delete, multi-tenancy, and feature flagging.
