# Smell-to-Refactoring Map

Quick diagnostic reference mapping code smells and anti-patterns to their root cause and safest refactoring move.

| Symptom / Smell | Root Cause | Recommended Refactoring / Pattern |
| :--- | :--- | :--- |
| **God Object / Large Class** | Multiple axes of change in 1 class | Extract Class, Vertical-Slice Use Cases |
| **Long Method** | Mixed abstraction levels | Extract Method, Compose Method |
| **Deeply Nested `if/else`** | Missing early exits or polymorphism | Guard Clauses, Strategy / State / ADT |
| **Switch on Type Code** | Procedural branching over variants | Replace Conditional with Polymorphism / `sealed` ADT |
| **Primitive Obsession** | Missing domain types | Introduce Value Object (`Money`, `Email`) |
| **Feature Envy** | Method lives on wrong class | Move Method to data owner |
| **Divergent Change** | 1 class does too many things | Extract Class (SRP) |
| **Shotgun Surgery** | 1 concept scattered everywhere | Move Method / Inline Class |
| **Boolean Flag Arguments** | 1 function doing 2 different jobs | Separate Explicit Methods / Options Object |
| **Magic Numbers / Strings** | Unexplained literals | Extract Constant / Enum |
| **Singleton / Service Locator Abuse** | Hidden global coupling | Constructor / Functional DI |
| **Deep Inheritance** | Fragile base class / Yo-Yo | Replace Inheritance with Composition / Decorator |
| **Big Ball of Mud** | No module boundaries | Modular Monolith + CI Fitness Functions + Strangler Fig |

---

## Navigation

| Previous | Up | Next |
| :--- | :--- | :--- |
| [Leaky Abstractions](file:///E:/Learning/engineering-notes/08-solid-design-patterns/13-anti-patterns-and-code-smells/19-leaky-abstractions.md) | [Module 13](file:///E:/Learning/engineering-notes/08-solid-design-patterns/13-anti-patterns-and-code-smells/README.md) | [Module 14: Refactoring](file:///E:/Learning/engineering-notes/08-solid-design-patterns/14-refactoring/README.md) |
