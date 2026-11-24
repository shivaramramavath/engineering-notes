# Module 15: Architecture & Design Patterns

Welcome to **Module 15: Architecture & Design Patterns**. This module presents high-level software architecture within the Spring Boot ecosystem: Layered vs Hexagonal (Ports & Adapters) vs Clean Architecture, Domain-Driven Design (DDD) with aggregates and domain events, CQRS, Modular Monoliths vs Microservices, classic GoF design patterns in Spring, and real-world resilience patterns.

---

## Module Contents

| # | Note | Description | Level | Status |
|---|---|---|---|---|
| 01 | [01-layered-architecture.md](file:///03-backend/spring-boot/15-architecture-and-patterns/01-layered-architecture.md) | Traditional Controller-Service-Repository 3-tier architecture, benefits, failure modes | Beginner | Complete |
| 02 | [02-hexagonal-clean-onion-with-spring.md](file:///03-backend/spring-boot/15-architecture-and-patterns/02-hexagonal-clean-onion-with-spring.md) | Ports & Adapters architecture, isolating domain core from framework dependencies | Advanced | Complete |
| 03 | [03-domain-driven-design-with-spring.md](file:///03-backend/spring-boot/15-architecture-and-patterns/03-domain-driven-design-with-spring.md) | Aggregates, Entities, Value Objects, Domain Events, Repositories with Spring Data | Advanced | Complete |
| 04 | [04-cqrs-and-event-sourcing.md](file:///03-backend/spring-boot/15-architecture-and-patterns/04-cqrs-and-event-sourcing.md) | Command Query Responsibility Segregation (CQRS) and Event Sourcing overview | Advanced | Complete |
| 05 | [05-modular-monolith-vs-microservices.md](file:///03-backend/spring-boot/15-architecture-and-patterns/05-modular-monolith-vs-microservices.md) | Trade-off analysis: Spring Modulith modular monoliths vs distributed microservices | Advanced | Complete |
| 06 | [06-design-patterns-in-spring.md](file:///03-backend/spring-boot/15-architecture-and-patterns/06-design-patterns-in-spring.md) | GoF patterns applied: Factory, Strategy, Template Method, Proxy, Observer, Decorator | Intermediate | Complete |
| 07 | [07-repository-and-unit-of-work.md](file:///03-backend/spring-boot/15-architecture-and-patterns/07-repository-and-unit-of-work.md) | Repository pattern vs DAO pattern, Spring `@Transactional` as Unit of Work | Intermediate | Complete |
| 08 | [08-dto-entity-domain-separation.md](file:///03-backend/spring-boot/15-architecture-and-patterns/08-dto-entity-domain-separation.md) | Clean boundary modeling: DTO Records $\to$ Domain Models $\to$ Persistence Entities | Intermediate | Complete |
| 09 | [09-error-handling-architecture.md](file:///03-backend/spring-boot/15-architecture-and-patterns/09-error-handling-architecture.md) | Architectural taxonomy of exceptions: Domain, Infrastructure, Validation errors | Intermediate | Complete |
| 10 | [10-feature-flags.md](file:///03-backend/spring-boot/15-architecture-and-patterns/10-feature-flags.md) | Implementing dynamic feature toggling with Togglz, Unleash, and Spring `@Conditional` | Intermediate | Complete |
| 11 | [11-multi-tenancy-architecture.md](file:///03-backend/spring-boot/15-architecture-and-patterns/11-multi-tenancy-architecture.md) | Multi-tenancy models: Shared DB Discriminator vs Schema-per-tenant vs Database-per-tenant | Advanced | Complete |
| 12 | [12-api-versioning-strategies.md](file:///03-backend/spring-boot/15-architecture-and-patterns/12-api-versioning-strategies.md) | Comparing URI versioning, Header versioning, and Media-Type content negotiation | Intermediate | Complete |
| 13 | [13-clean-code-and-code-review-checklist.md](file:///03-backend/spring-boot/15-architecture-and-patterns/13-clean-code-and-code-review-checklist.md) | Production code review checklist for Spring Boot: immutability, transactions, security | Intermediate | Complete |
| 14 | [14-real-world-patterns.md](file:///03-backend/spring-boot/15-architecture-and-patterns/14-real-world-patterns.md) | Outbox pattern, distributed lock (Redlock/ShedLock), idempotency keys, soft delete | Advanced | Complete |

---

## Next Steps
Proceed to **[Module 16: Spring AI](file:///03-backend/spring-boot/16-ai/)** to explore Large Language Model integration, embeddings, and RAG.
