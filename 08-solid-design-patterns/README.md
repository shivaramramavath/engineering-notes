# SOLID, Design Principles & Design Patterns Curriculum

Welcome to the **canonical, language-neutral conceptual home** for Object-Oriented Design, SOLID principles, GoF patterns, enterprise application patterns, cloud/distributed resilience patterns, and modern software architecture.

> **Our Core Philosophy:** Every principle and every pattern is taught **PROBLEM-FIRST**. We never start with an abstract definition. Instead, we examine a concrete production failure, analyze why naive code breaks when business requirements change, and then demonstrate how the design principle or pattern elegantly solves that friction with verified code in **Java 25 (LTS)**, **TypeScript 5.8 (Node 24 native)**, and **Python 3.14**.

---

## Curriculum Architecture & Module Overview

```
08-solid-design-patterns/
├── 01-foundations/                      # OOP pillars, composition vs inheritance, coupling & cohesion
├── 02-solid-principles/                 # Single Responsibility, Open/Closed, Liskov, Interface Segregation, DIP
├── 03-design-principles/                # DRY, KISS, YAGNI, Law of Demeter, Tell-Don't-Ask, GRASP, Package Principles
├── 04-patterns-intro/                   # What patterns are (and are not), DI, IoC containers, pattern history
├── 05-creational-patterns/              # Singleton, Factory Method, Abstract Factory, Builder, Prototype, Pool
├── 06-structural-patterns/              # Adapter, Bridge, Composite, Decorator, Facade, Flyweight, Proxy, Null Object
├── 07-behavioral-patterns/              # Strategy, Observer, Command, State, Template Method, Chain, Visitor, Interpreter
├── 08-concurrency-patterns/             # Thread Pool, Producer-Consumer, Monitor Object, Read-Write Lock, Actors
├── 09-enterprise-application-patterns/  # Repository, Unit of Work, Data Mapper, Active Record, Service Layer, DTO
├── 10-architecture/                     # Layered, Clean/Hexagonal/Onion, Modular Monolith, DDD, Event-Driven, ADRs
├── 11-distributed-and-cloud-patterns/   # Circuit Breaker, Retry/Jitter, Saga, Outbox, Strangler Fig, Bulkhead
├── 12-functional-and-modern-idioms/     # Patterns as functions, Railway error handling (Result/Option), ADTs
├── 13-anti-patterns-and-code-smells/    # God Object, Primitive Obsession, Feature Envy, Shotgun Surgery, Ball of Mud
├── 14-refactoring/                      # Refactoring workflows, safety nets, characterization tests, code metrics
├── 15-testing-and-debugging/            # Testable design, seams, test doubles (dummy, stub, spy, mock, fake), TDD
├── 16-production/                       # Maintainability, error handling, observability, API versioning, checklists
├── 17-case-studies/                     # 12 complete end-to-end multi-pattern systems with verified execution
├── 18-pattern-selection/                # Problem-based decision trees, comparison matrices, over-engineering guards
├── 19-interview/                        # 10 deep technical interview guides, low-level design challenges
└── 20-cheatsheets/                      # 9 operational reference sheets, GoF at a glance, UML recipes
```

---

## 17-Section Problem-First Note Blueprint

Every principle and pattern note in this track follows an uncompromising 17-section structure:

1. **The Problem**: A concrete production scenario (what the team built, why it started failing).
2. **Why It Exists**: Historical context and forces being balanced.
3. **What It Is**: Precise definition, one-sentence intent, key terms, structural diagram.
4. **The Problem Without It**: Naive code that works today; apply a realistic Change Request; demonstrate the exact breakage/blast radius.
5. **How It Solves It**: The principle/pattern applied to the **exact same scenario**; same Change Request now frictionless; runnable code in Java + TypeScript + Python.
6. **How It Works**: Class diagram, sequence diagram, participants & responsibilities, invariants.
7. **Real-World Examples**: Three verified implementations: (a) Business domain, (b) Standard Library / Framework, (c) Modern Cloud / Distributed.
8. **When to Use and When NOT to Use**: Clear boundaries, cost/benefit trade-offs, over-engineering signals.
9. **Variations and Language Idioms**: Functional expressions, closures, language-specific mechanics.
10. **Relationship to Other Principles and Patterns**: SOLID mapping, look-alike distinction tables.
11. **Common Mistakes and Anti-Patterns**: Table with 6+ rows (Mistake / Why Wrong / Fix).
12. **Testing and Design Verification**: Testability seams, test doubles, real unit tests.
13. **Performance, Concurrency and Security Notes**: Memory overhead, thread safety, access control.
14. **Interview Questions**: 8 deep questions with collapsible `<details>` answers.
15. **Practice Exercises**: 6 graded exercises (2 easy, 2 medium, 2 hard) with verified solutions.
16. **Summary / Cheat Sheet**: High-density one-screen reference.
17. **Further Reading & "In Your Language" Links**: Verified documentation and links to language track deep dives.

---

## Quick Navigation Index

| Roadmap & Navigation | Purpose |
|---|---|
| [INDEX.md](file:///e:/Learning/engineering-notes/08-solid-design-patterns/INDEX.md) | Searchable index of all principles and patterns by problem, category, and language |
| [ROADMAP.md](file:///e:/Learning/engineering-notes/08-solid-design-patterns/ROADMAP.md) | Targeted learning paths (Junior, Senior Backend, Systems Architect, Interview Prep) |
| [GLOSSARY.md](file:///e:/Learning/engineering-notes/08-solid-design-patterns/GLOSSARY.md) | 150+ authoritative software design definitions and terminology |
| [CONTRIBUTING.md](file:///e:/Learning/engineering-notes/08-solid-design-patterns/CONTRIBUTING.md) | Guidelines for verifying code and adding new patterns |
| [SDP_FACTS.md](file:///e:/Learning/engineering-notes/08-solid-design-patterns/SDP_FACTS.md) | Verified runtime versions and standard library references |

---

## Cross-Repository Language Deep Dives

This track teaches **language-neutral conceptual mechanics**. For language-specific idioms, refer to our dedicated language tracks:

- [Java Design Patterns Track](file:///e:/Learning/engineering-notes/01-languages/java/24-design-patterns/README.md)
- [TypeScript Design Patterns Track](file:///e:/Learning/engineering-notes/01-languages/typescript/17-design-patterns/README.md)
- [Python Design Patterns Track](file:///e:/Learning/engineering-notes/01-languages/python/23-design-patterns/README.md)
- [C Design & Patterns Track](file:///e:/Learning/engineering-notes/01-languages/c/10-design-and-patterns/README.md)
- [Node.js Design Patterns Track](file:///e:/Learning/engineering-notes/03-backend/nodejs/18-design-patterns/README.md)
- [NestJS Architecture & Patterns](file:///e:/Learning/engineering-notes/03-backend/nestjs/08-architecture-and-patterns/README.md)
- [Spring Boot Architecture & Patterns](file:///e:/Learning/engineering-notes/03-backend/spring-boot/15-architecture-and-patterns/README.md)
- [FastAPI Architecture & Patterns](file:///e:/Learning/engineering-notes/03-backend/fastapi/12-architecture-and-patterns/README.md)
- [React Component Design](file:///e:/Learning/engineering-notes/02-frontend/reactjs/05-component-design/README.md)

---
*Created and maintained as part of the Engineering Mastery Repository.*
