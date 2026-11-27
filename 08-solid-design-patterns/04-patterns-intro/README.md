# Module 04: Introduction to Design Patterns

Welcome to **Module 04: Introduction to Design Patterns**. Before diving into the 23 Gang of Four (GoF) patterns and modern enterprise/cloud patterns, this module establishes *why* patterns exist, how to read and evaluate them without cargo-culting, how **Dependency Injection (DI)** and **Inversion of Control (IoC) Containers** actually wire pattern-based architectures together, and how pattern languages evolved from Christopher Alexander to GoF, PoEAA, and Cloud Architecture.

---

## Module Roadmap & Contents

| # | Note | Core Focus & Purpose | Status | Est. Time |
|---|---|---|:---:|:---:|
| 00 | [Patterns Introduction Overview](file:///e:/Learning/engineering-notes/08-solid-design-patterns/04-patterns-intro/00-overview.md) | Why patterns are shared vocabulary, Creational/Structural/Behavioral taxonomy | Complete | 15 min |
| 01 | [What Are Design Patterns?](file:///e:/Learning/engineering-notes/08-solid-design-patterns/04-patterns-intro/01-what-are-patterns.md) | Anatomy of a pattern (Context, Forces, Solution, Consequences), Pattern vs Idiom vs Algorithm | Complete | 25 min |
| 02 | [How to Read & Choose Patterns](file:///e:/Learning/engineering-notes/08-solid-design-patterns/04-patterns-intro/02-how-to-read-and-choose-patterns.md) | Refactoring *to* patterns (Kerievsky), avoiding Pattern Fever, symptom-to-pattern triage | Complete | 25 min |
| 03 | [Dependency Injection (DI)](file:///e:/Learning/engineering-notes/08-solid-design-patterns/04-patterns-intro/03-dependency-injection.md) | Constructor vs Setter vs Interface injection, Composition Root, pure manual DI | Complete | 35 min |
| 04 | [Inversion of Control (IoC) Containers](file:///e:/Learning/engineering-notes/08-solid-design-patterns/04-patterns-intro/04-inversion-of-control-containers.md) | Bean lifecycles, Singleton/Request/Transient scopes, reflection vs compile-time DI | Complete | 30 min |
| 05 | [Pattern Languages & Catalogues History](file:///e:/Learning/engineering-notes/08-solid-design-patterns/04-patterns-intro/05-pattern-languages-and-catalogues-history.md) | Christopher Alexander, GoF (1994), POSA, Martin Fowler's PoEAA, Cloud & EIP | Complete | 25 min |

---

## Core Learning Outcomes

By completing this module, you will:
1. Distinguish a **Design Pattern** (reusable architectural collaboration) from an **Algorithm**, a **Language Idiom**, and a **Framework**.
2. Diagnose codebases by **structural friction** and refactor *toward* patterns only when forces justify them.
3. Master **Dependency Injection** (Constructor Injection + Composition Root) without needing heavyweight IoC magic.
4. Understand how modern **IoC Containers** (Spring, NestJS, FastAPI `Depends`) resolve dependency graphs, scopes, and circular-dependency errors.

---

[← Back to Module 03: General Design Principles](file:///e:/Learning/engineering-notes/08-solid-design-patterns/03-design-principles/README.md) | [Next: 00 - Patterns Intro Overview →](file:///e:/Learning/engineering-notes/08-solid-design-patterns/04-patterns-intro/00-overview.md)
