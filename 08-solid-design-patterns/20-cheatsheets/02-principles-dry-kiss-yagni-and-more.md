# Design Principles — Quick Reference

| Principle | Core Rule |
| :--- | :--- |
| **DRY & Rule of Three** | Don't duplicate *knowledge*; wait for 3 occurrences before abstracting |
| **KISS** | Simplest working design wins; avoid cleverness |
| **YAGNI** | Don't build speculative features/hooks "just in case" |
| **Composition > Inheritance** | Compose behaviors via interfaces/delegation; avoid deep hierarchies |
| **Program to Interfaces** | Depend on contracts (`List`, `Repo`), not concrete implementations |
| **Encapsulate What Varies** | Isolate volatile rules (tax, pricing, vendor APIs) behind boundaries |
| **Loose Coupling, High Cohesion** | Group things that change together; minimize cross-module deps |
| **Law of Demeter** | Avoid `a.getB().getC().doX()`; talk only to direct friends |
| **Tell, Don't Ask** | Call `order.ship()`, not `if (order.getStatus() == PAID) order.setStatus(SHIPPED)` |
| **Fail Fast** | Validate inputs/config immediately at startup or boundary |
| **Idempotency** | Retrying the same request (`Idempotency-Key`) yields the same state |
