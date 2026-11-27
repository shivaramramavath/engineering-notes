# Module 12: Functional & Modern Idioms

Modern languages (Java 25 LTS, TypeScript 5.8, Python 3.14) are multi-paradigm. Many classic GoF object-oriented patterns—Strategy, Command, Decorator, Visitor, Template Method—collapse into first-class functions, higher-order composition, algebraic data types (ADTs), and immutable data pipelines.

This module bridges **classic OOP design patterns** and **modern functional idioms**, showing when a 3-line function replaces a 4-class hierarchy—and when OOP classes are still the clearer choice.

---

## Notes in This Module

| # | Topic | Focus |
| :--- | :--- | :--- |
| 00 | [Module Overview](file:///E:/Learning/engineering-notes/08-solid-design-patterns/12-functional-and-modern-idioms/00-overview.md) | Why functional idioms simplify modern architecture & learning map |
| 01 | [Patterns as Functions](file:///E:/Learning/engineering-notes/08-solid-design-patterns/12-functional-and-modern-idioms/01-patterns-as-functions.md) | Strategy, Command, Decorator, Template Method as higher-order functions |
| 02 | [Pipelines & Middleware](file:///E:/Learning/engineering-notes/08-solid-design-patterns/12-functional-and-modern-idioms/02-pipelines-and-middleware.md) | Composable processing chains (`pipe`, Express/FastAPI/HTTP middleware) |
| 03 | [Option, Result, Either & Railway-Oriented Error Handling](file:///E:/Learning/engineering-notes/08-solid-design-patterns/12-functional-and-modern-idioms/03-option-result-either-and-railway-oriented-error-handling.md) | Making failure states explicit in types instead of hidden exceptions or `null` |
| 04 | [Currying, Partial Application & Composition](file:///E:/Learning/engineering-notes/08-solid-design-patterns/12-functional-and-modern-idioms/04-currying-partial-application-and-composition.md) | Building reusable specialized functions from general ones |
| 05 | [Memoisation](file:///E:/Learning/engineering-notes/08-solid-design-patterns/12-functional-and-modern-idioms/05-memoisation.md) | Caching pure function outputs (`@lru_cache`, `computeIfAbsent`, closures) |
| 06 | [Immutability & Persistent Data](file:///E:/Learning/engineering-notes/08-solid-design-patterns/12-functional-and-modern-idioms/06-immutability-and-persistent-data.md) | Structural sharing, `record` / `frozen=True` / `Readonly<T>`, safe state updates |
| 07 | [Algebraic Data Types & Sealed Hierarchies](file:///E:/Learning/engineering-notes/08-solid-design-patterns/12-functional-and-modern-idioms/07-algebraic-data-types-and-sealed-hierarchies.md) | Sum/product types & exhaustive pattern matching replacing Visitor |
| 08 | [Dependency Injection in Functional Style](file:///E:/Learning/engineering-notes/08-solid-design-patterns/12-functional-and-modern-idioms/08-dependency-injection-in-functional-style.md) | Partial application, Reader/context passing, and ports as function types |
| 09 | [Reactive & Streams Patterns](file:///E:/Learning/engineering-notes/08-solid-design-patterns/12-functional-and-modern-idioms/09-reactive-and-streams-patterns.md) | Lazy sequences, `map`/`filter`/`reduce`, pull vs push streams |
| 10 | [When FP Replaces OOP Patterns](file:///E:/Learning/engineering-notes/08-solid-design-patterns/12-functional-and-modern-idioms/10-when-fp-replaces-oop-patterns.md) | Decision matrix: functions + ADTs vs interfaces + classes |

---

## Navigation

| Previous | Up | Next |
| :--- | :--- | :--- |
| [Module 11: Distributed & Cloud Patterns](file:///E:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/README.md) | [Track Index](file:///E:/Learning/engineering-notes/08-solid-design-patterns/INDEX.md) | [Module 13: Anti-Patterns & Code Smells](file:///E:/Learning/engineering-notes/08-solid-design-patterns/13-anti-patterns-and-code-smells/README.md) |
