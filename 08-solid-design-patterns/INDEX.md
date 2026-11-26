# SOLID, Design Principles & Design Patterns Index (INDEX.md)

Welcome to the comprehensive topic lookup for software design principles, GoF patterns, architectural styles, and refactoring techniques.

---

## 1. Index by Problem ("I need to...")

| When you face this engineering problem... | Look at this Principle / Pattern | Location |
|---|---|---|
| A class changes every time a different department or stakeholder requests an update | **Single Responsibility Principle (SRP)** | [02-solid-principles/01-single-responsibility.md](file:///e:/Learning/engineering-notes/08-solid-design-patterns/02-solid-principles/01-single-responsibility.md) |
| Adding a new payment provider or file format requires modifying giant `switch` statements | **Open/Closed Principle (OCP)** & **Strategy** | [02-solid-principles/02-open-closed.md](file:///e:/Learning/engineering-notes/08-solid-design-patterns/02-solid-principles/02-open-closed.md) |
| Subclasses throw `NotSupportedException` or break client assumptions | **Liskov Substitution Principle (LSP)** | [02-solid-principles/03-liskov-substitution.md](file:///e:/Learning/engineering-notes/08-solid-design-patterns/02-solid-principles/03-liskov-substitution.md) |
| Clients are forced to implement methods they never use | **Interface Segregation Principle (ISP)** | [02-solid-principles/04-interface-segregation.md](file:///e:/Learning/engineering-notes/08-solid-design-patterns/02-solid-principles/04-interface-segregation.md) |
| High-level business logic is tightly bound to low-level database queries or HTTP clients | **Dependency Inversion Principle (DIP)** | [02-solid-principles/05-dependency-inversion.md](file:///e:/Learning/engineering-notes/08-solid-design-patterns/02-solid-principles/05-dependency-inversion.md) |
| Code is duplicated across 5 microservices or modules | **DRY & Rule of Three** | [03-design-principles/01-dry-and-rule-of-three.md](file:///e:/Learning/engineering-notes/08-solid-design-patterns/03-design-principles/01-dry-and-rule-of-three.md) |
| Object construction has 12 parameters and telescoping constructors | **Builder Pattern** | [05-creational-patterns/04-builder.md](file:///e:/Learning/engineering-notes/08-solid-design-patterns/05-creational-patterns/04-builder.md) |
| An incompatible third-party SDK interface needs to match your domain interface | **Adapter Pattern** | [06-structural-patterns/01-adapter.md](file:///e:/Learning/engineering-notes/08-solid-design-patterns/06-structural-patterns/01-adapter.md) |
| You need to add caching, logging, or metrics dynamically without modifying existing classes | **Decorator Pattern** & **Proxy Pattern** | [06-structural-patterns/04-decorator.md](file:///e:/Learning/engineering-notes/08-solid-design-patterns/06-structural-patterns/04-decorator.md) |
| A downstream service is failing intermittently and exhausting thread pools | **Circuit Breaker** & **Bulkhead** | [11-distributed-and-cloud-patterns/01-circuit-breaker.md](file:///e:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/01-circuit-breaker.md) |
| A distributed multi-service checkout flow needs atomic rollback without 2PC | **Saga Pattern** | [11-distributed-and-cloud-patterns/07-saga-pattern.md](file:///e:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/07-saga-pattern.md) |
| You need to publish domain events reliably when writing to a local SQL database | **Transactional Outbox Pattern** | [11-distributed-and-cloud-patterns/08-transactional-outbox.md](file:///e:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/08-transactional-outbox.md) |
| You need undo/redo capabilities or asynchronous queue execution | **Command Pattern** | [07-behavioral-patterns/03-command.md](file:///e:/Learning/engineering-notes/08-solid-design-patterns/07-behavioral-patterns/03-command.md) |
| A monolithic legacy system needs to be incrementally migrated without full downtime | **Strangler Fig Pattern** | [11-distributed-and-cloud-patterns/13-strangler-fig-migration.md](file:///e:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/13-strangler-fig-migration.md) |

---

## 2. Master Category Directory

### Module 01: Foundations
- [00 Overview](file:///e:/Learning/engineering-notes/08-solid-design-patterns/01-foundations/00-overview.md)
- [01 OOP Pillars](file:///e:/Learning/engineering-notes/08-solid-design-patterns/01-foundations/01-oop-pillars.md)
- [02 Composition vs Inheritance](file:///e:/Learning/engineering-notes/08-solid-design-patterns/01-foundations/02-composition-vs-inheritance.md)
- [03 Coupling and Cohesion](file:///e:/Learning/engineering-notes/08-solid-design-patterns/01-foundations/03-coupling-and-cohesion.md)
- [04 Programming to Interfaces](file:///e:/Learning/engineering-notes/08-solid-design-patterns/01-foundations/04-programming-to-interfaces.md)
- [05 Abstraction & Information Hiding](file:///e:/Learning/engineering-notes/08-solid-design-patterns/01-foundations/05-abstraction-and-information-hiding.md)
- [06 Encapsulate What Varies](file:///e:/Learning/engineering-notes/08-solid-design-patterns/01-foundations/06-encapsulate-what-varies.md)
- [07 Immutability & Value Objects](file:///e:/Learning/engineering-notes/08-solid-design-patterns/01-foundations/07-immutability-and-value-objects.md)
- [08 Command Query Separation (CQS)](file:///e:/Learning/engineering-notes/08-solid-design-patterns/01-foundations/08-command-query-separation.md)
- [09 Design by Contract & Defensive Programming](file:///e:/Learning/engineering-notes/08-solid-design-patterns/01-foundations/09-design-by-contract-and-defensive-programming.md)
- [10 What is Good Design?](file:///e:/Learning/engineering-notes/08-solid-design-patterns/01-foundations/10-what-is-good-design.md)

### Module 02: SOLID Principles
- [00 Overview](file:///e:/Learning/engineering-notes/08-solid-design-patterns/02-solid-principles/00-overview.md)
- [01 Single Responsibility Principle (SRP)](file:///e:/Learning/engineering-notes/08-solid-design-patterns/02-solid-principles/01-single-responsibility.md)
- [02 Open/Closed Principle (OCP)](file:///e:/Learning/engineering-notes/08-solid-design-patterns/02-solid-principles/02-open-closed.md)
- [03 Liskov Substitution Principle (LSP)](file:///e:/Learning/engineering-notes/08-solid-design-patterns/02-solid-principles/03-liskov-substitution.md)
- [04 Interface Segregation Principle (ISP)](file:///e:/Learning/engineering-notes/08-solid-design-patterns/02-solid-principles/04-interface-segregation.md)
- [05 Dependency Inversion Principle (DIP)](file:///e:/Learning/engineering-notes/08-solid-design-patterns/02-solid-principles/05-dependency-inversion.md)
- [06 SOLID in Practice](file:///e:/Learning/engineering-notes/08-solid-design-patterns/02-solid-principles/06-solid-in-practice.md)
- [07 Spotting Violations Guide](file:///e:/Learning/engineering-notes/08-solid-design-patterns/02-solid-principles/07-solid-violations-spotting-guide.md)
- [08 SOLID & Functional Programming](file:///e:/Learning/engineering-notes/08-solid-design-patterns/02-solid-principles/08-solid-and-functional-programming.md)
- [09 Criticisms & Limits of SOLID](file:///e:/Learning/engineering-notes/08-solid-design-patterns/02-solid-principles/09-criticisms-and-limits-of-solid.md)

### Module 03: General Design Principles
- [00 Overview](file:///e:/Learning/engineering-notes/08-solid-design-patterns/03-design-principles/00-overview.md)
- [01 DRY & Rule of Three](file:///e:/Learning/engineering-notes/08-solid-design-patterns/03-design-principles/01-dry-and-rule-of-three.md)
- [02 KISS Principle](file:///e:/Learning/engineering-notes/08-solid-design-patterns/03-design-principles/02-kiss-principle.md)
- [03 YAGNI](file:///e:/Learning/engineering-notes/08-solid-design-patterns/03-design-principles/03-yagni.md)
- [04 Law of Demeter](file:///e:/Learning/engineering-notes/08-solid-design-patterns/03-design-principles/04-law-of-demeter.md)
- [05 Separation of Concerns](file:///e:/Learning/engineering-notes/08-solid-design-patterns/03-design-principles/05-separation-of-concerns.md)
- [06 Tell, Don't Ask](file:///e:/Learning/engineering-notes/08-solid-design-patterns/03-design-principles/06-tell-dont-ask.md)
- [07 Principle of Least Astonishment](file:///e:/Learning/engineering-notes/08-solid-design-patterns/03-design-principles/07-principle-of-least-astonishment.md)
- [08 Fail-Fast Principle](file:///e:/Learning/engineering-notes/08-solid-design-patterns/03-design-principles/08-fail-fast-principle.md)
- [09 Boy Scout Rule](file:///e:/Learning/engineering-notes/08-solid-design-patterns/03-design-principles/09-boy-scout-rule-and-continuous-refactoring.md)
- [10 High Cohesion & Low Coupling in Practice](file:///e:/Learning/engineering-notes/08-solid-design-patterns/03-design-principles/10-high-cohesion-low-coupling-in-practice.md)
- [11 Orthogonality](file:///e:/Learning/engineering-notes/08-solid-design-patterns/03-design-principles/11-orthogonality.md)
- [12 Postel's Law (Robustness Principle)](file:///e:/Learning/engineering-notes/08-solid-design-patterns/03-design-principles/12-postels-law-robustness-principle.md)
- [13 Hollywood Principle & IoC](file:///e:/Learning/engineering-notes/08-solid-design-patterns/03-design-principles/13-hollywood-principle-and-inversion-of-control.md)
- [14 GRASP Principles](file:///e:/Learning/engineering-notes/08-solid-design-patterns/03-design-principles/14-grasp-principles.md)
- [15 Package & Component Principles (REP, CCP, CRP, ADP, SDP, SAP)](file:///e:/Learning/engineering-notes/08-solid-design-patterns/03-design-principles/15-package-and-component-principles.md)
- [16 Premature Optimization & Measuring](file:///e:/Learning/engineering-notes/08-solid-design-patterns/03-design-principles/16-premature-optimisation-and-measuring.md)
- [17 Idempotency Principle](file:///e:/Learning/engineering-notes/08-solid-design-patterns/03-design-principles/17-idempotency-principle.md)
- [18 Conway's Law](file:///e:/Learning/engineering-notes/08-solid-design-patterns/03-design-principles/18-conways-law-and-socio-technical-design.md)
- [19 Principles Conflicts & Trade-Offs](file:///e:/Learning/engineering-notes/08-solid-design-patterns/03-design-principles/19-principles-conflicts-and-trade-offs.md)
- [20 Principles Cheat Sheet](file:///e:/Learning/engineering-notes/08-solid-design-patterns/03-design-principles/20-principles-cheat-sheet.md)

---
*For a complete map across all modules, refer to [ROADMAP.md](file:///e:/Learning/engineering-notes/08-solid-design-patterns/ROADMAP.md).*
