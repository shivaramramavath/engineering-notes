# 23 · Design and Clean Code

Writing functional Java is standard; writing expressive, resilient, maintainable, and thread-safe Java is craftsmanship. This module bridges software architecture with daily coding practices: SOLID principles modernized with Java 21 features, pragmatic KISS/YAGNI/DRY guidelines, clean code idioms, smell identification and refactoring, deep immutability, defensive programming, classic language pitfalls, and an authoritative production code review checklist.

## Contents

| # | Note | Focus |
|---|------|-------|
| 00 | [SOLID Principles in Modern Java](00_solid-principles.md) | SRP, OCP, LSP, ISP, DIP via Records, Sealed Interfaces, Pattern Matching |
| 01 | [Core Design Principles](01_design-principles.md) | KISS, YAGNI, DRY, Composition over Inheritance, Law of Demeter |
| 02 | [Clean Code in Modern Java](02_clean-code.md) | Naming conventions, method decomposition, parameter bounds, null-safety |
| 03 | [Refactoring and Code Smells](03_refactoring-and-code-smells.md) | Primitive Obsession, Feature Envy, Data Clumps, Extract Method, Polymorphism |
| 04 | [Immutability in Modern Java](04_immutability.md) | Deep vs. shallow immutability, JMM final field freeze, withers, thread safety |
| 05 | [Defensive Programming](05_defensive-programming.md) | Preconditions, postconditions, fail-fast, TOCTOU race conditions, assertions |
| 06 | [Common Java Mistakes](06_common-java-mistakes.md) | String ==, BigDecimal double trap, ConcurrentModificationException, Integer cache |
| 07 | [Production Code Review Checklist](07_code-review-checklist.md) | Concurrency review, resource leaks, N+1 queries, pull request audit matrix |

---
**Next module:** [Design Patterns](../24-design-patterns/README.md)
