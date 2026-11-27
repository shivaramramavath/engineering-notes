# Module 09: Enterprise Application Patterns

Enterprise applications live for years, evolve across dozens of developers, and sit between complex business rules, relational/document databases, and external APIs. Inspired by Martin Fowler's *Patterns of Enterprise Application Architecture (PoEAA)* and Eric Evans' *Domain-Driven Design*, this module teaches the core patterns that keep persistence, domain logic, concurrency, and UI boundaries decoupled.

---

## Notes in This Module

| # | Topic | Focus |
| :--- | :--- | :--- |
| 00 | [Module Overview](file:///E:/Learning/engineering-notes/08-solid-design-patterns/09-enterprise-application-patterns/00-overview.md) | PoEAA map, persistence vs domain vs presentation layers |
| 01 | [Repository](file:///E:/Learning/engineering-notes/08-solid-design-patterns/09-enterprise-application-patterns/01-repository.md) | Collection-like abstraction over persistence |
| 02 | [Unit of Work](file:///E:/Learning/engineering-notes/08-solid-design-patterns/09-enterprise-application-patterns/02-unit-of-work.md) | Atomic transaction & change-tracking coordinator |
| 03 | [Data Mapper vs Active Record](file:///E:/Learning/engineering-notes/08-solid-design-patterns/09-enterprise-application-patterns/03-data-mapper-vs-active-record.md) | Separating domain objects from database tables |
| 04 | [Specification](file:///E:/Learning/engineering-notes/08-solid-design-patterns/09-enterprise-application-patterns/04-specification.md) | Composable, reusable business query & validation rules |
| 05 | [Service Layer](file:///E:/Learning/engineering-notes/08-solid-design-patterns/09-enterprise-application-patterns/05-service-layer.md) | Application boundary coordinating use cases & transactions |
| 06 | [Data Transfer Object (DTO)](file:///E:/Learning/engineering-notes/08-solid-design-patterns/09-enterprise-application-patterns/06-data-transfer-object.md) | Decoupling wire/API contracts from internal domain entities |
| 07 | [Identity Map](file:///E:/Learning/engineering-notes/08-solid-design-patterns/09-enterprise-application-patterns/07-identity-map.md) | Per-request/per-session object cache preventing duplicate loads |
| 08 | [Lazy Loading](file:///E:/Learning/engineering-notes/08-solid-design-patterns/09-enterprise-application-patterns/08-lazy-loading.md) | Deferring expensive relation loads—and avoiding the N+1 trap |
| 09 | [Gateway & Adapter for External Systems](file:///E:/Learning/engineering-notes/08-solid-design-patterns/09-enterprise-application-patterns/09-gateway-and-adapter-for-external-systems.md) | Isolating third-party HTTP/SDK boundaries (Anti-Corruption Layer) |
| 10 | [Domain Model vs Transaction Script](file:///E:/Learning/engineering-notes/08-solid-design-patterns/09-enterprise-application-patterns/10-domain-model-vs-transaction-script.md) | Choosing procedural scripts vs object-oriented domain modeling |
| 11 | [Anemic vs Rich Domain Model](file:///E:/Learning/engineering-notes/08-solid-design-patterns/09-enterprise-application-patterns/11-anemic-vs-rich-domain-model.md) | Keeping invariants & behavior inside domain entities |
| 12 | [MVC, MVP & MVVM](file:///E:/Learning/engineering-notes/08-solid-design-patterns/09-enterprise-application-patterns/12-mvc-mvp-mvvm.md) | Presentation-layer separation patterns |
| 13 | [Plugin & Registry](file:///E:/Learning/engineering-notes/08-solid-design-patterns/09-enterprise-application-patterns/13-plugin-and-registry.md) | Runtime extension points without core code changes |
| 14 | [Optimistic & Pessimistic Locking](file:///E:/Learning/engineering-notes/08-solid-design-patterns/09-enterprise-application-patterns/14-optimistic-and-pessimistic-locking-patterns.md) | Preventing lost updates under concurrent writes |
| 15 | [Value Object, Entity & Aggregate](file:///E:/Learning/engineering-notes/08-solid-design-patterns/09-enterprise-application-patterns/15-value-object-entity-aggregate.md) | DDD tactical building blocks & consistency boundaries |
| 16 | [Domain Events](file:///E:/Learning/engineering-notes/08-solid-design-patterns/09-enterprise-application-patterns/16-domain-events.md) | Decoupling side effects within and across bounded contexts |

---

## Navigation

| Previous | Up | Next |
| :--- | :--- | :--- |
| [Module 08: Concurrency Patterns](file:///E:/Learning/engineering-notes/08-solid-design-patterns/08-concurrency-patterns/README.md) | [Track Index](file:///E:/Learning/engineering-notes/08-solid-design-patterns/INDEX.md) | [Module 10: Architecture](file:///E:/Learning/engineering-notes/08-solid-design-patterns/10-architecture/README.md) |
