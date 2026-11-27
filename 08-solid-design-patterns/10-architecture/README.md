# Module 10: Software Architecture Patterns

Design patterns shape individual classes and modules; **architecture patterns** shape entire systems, team boundaries, deployment units, and long-term evolvability. This module covers the spectrum from Layered Monoliths and Hexagonal/Clean Architecture to Modular Monoliths, DDD, Event-Driven, CQRS, Microservices (and when *not* to use them), Cell-Based resilience, ADRs, and Evolutionary Architecture.

---

## Notes in This Module

| # | Topic | Focus |
| :--- | :--- | :--- |
| 00 | [Module Overview](file:///E:/Learning/engineering-notes/08-solid-design-patterns/10-architecture/00-overview.md) | Architectural trade-offs, coupling vs autonomy, and reading map |
| 01 | [Layered Architecture](file:///E:/Learning/engineering-notes/08-solid-design-patterns/10-architecture/01-layered.md) | Presentation → Application → Domain → Infrastructure (and sinkhole risk) |
| 02 | [Clean, Hexagonal & Onion Architecture](file:///E:/Learning/engineering-notes/08-solid-design-patterns/10-architecture/02-clean-hexagonal-onion.md) | Dependency Rule, Ports & Adapters, framework independence |
| 03 | [Modular Monolith](file:///E:/Learning/engineering-notes/08-solid-design-patterns/10-architecture/03-modular-monolith.md) | Strong module boundaries in a single deployable binary |
| 04 | [Domain-Driven Design (Strategic)](file:///E:/Learning/engineering-notes/08-solid-design-patterns/10-architecture/04-domain-driven-design.md) | Bounded Contexts, Ubiquitous Language, Context Mapping |
| 05 | [Event-Driven Architecture](file:///E:/Learning/engineering-notes/08-solid-design-patterns/10-architecture/05-event-driven.md) | Brokers, Event Notification vs ECST, Choreography vs Orchestration |
| 06 | [Microservices (and When NOT to Use Them)](file:///E:/Learning/engineering-notes/08-solid-design-patterns/10-architecture/06-microservices-and-when-not-to.md) | Independent deployment, distributed tax, Monolith-First rule |
| 07 | [CQRS & Event Sourcing](file:///E:/Learning/engineering-notes/08-solid-design-patterns/10-architecture/07-cqrs-and-event-sourcing.md) | Separating read/write models and append-only event logs |
| 08 | [Serverless & Function Patterns](file:///E:/Learning/engineering-notes/08-solid-design-patterns/10-architecture/08-serverless-and-function-patterns.md) | FaaS, cold starts, statelessness, event triggers |
| 09 | [Pipes & Filters](file:///E:/Learning/engineering-notes/08-solid-design-patterns/10-architecture/09-pipes-and-filters.md) | Streaming ETL, compilers, and composable filter stages |
| 10 | [Microkernel & Plugin Architecture](file:///E:/Learning/engineering-notes/08-solid-design-patterns/10-architecture/10-micro-kernel-and-plugin.md) | Minimal core + hot-swappable plugin ecosystem |
| 11 | [Space-Based & Cell-Based Architecture](file:///E:/Learning/engineering-notes/08-solid-design-patterns/10-architecture/11-space-based-and-cell-based.md) | Eliminating DB bottlenecks and isolating blast radius |
| 12 | [Architecture Decision Records (ADRs)](file:///E:/Learning/engineering-notes/08-solid-design-patterns/10-architecture/12-architecture-decision-records.md) | Lightweight immutable log of "why" behind key decisions |
| 13 | [Architecture Fitness Functions](file:///E:/Learning/engineering-notes/08-solid-design-patterns/10-architecture/13-architecture-fitness-functions.md) | Automated ArchUnit/dependency tests enforcing boundaries |
| 14 | [Evolutionary Architecture](file:///E:/Learning/engineering-notes/08-solid-design-patterns/10-architecture/14-evolutionary-architecture.md) | Incremental change, Strangler Fig, and Last Responsible Moment |

---

## Navigation

| Previous | Up | Next |
| :--- | :--- | :--- |
| [Module 09: Enterprise Application Patterns](file:///E:/Learning/engineering-notes/08-solid-design-patterns/09-enterprise-application-patterns/README.md) | [Track Index](file:///E:/Learning/engineering-notes/08-solid-design-patterns/INDEX.md) | [Module 11: Distributed & Cloud Patterns](file:///E:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/README.md) |
