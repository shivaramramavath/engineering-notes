# Refactoring to Patterns (and Away From Over-Engineered Patterns)

| Field | Value |
| :--- | :--- |
| **Category** | Refactoring (Pattern Evolution) |
| **Origin** | Joshua Kerievsky (*Refactoring to Patterns*, 2004) |

> **TL;DR**: Good engineers don't force GoF patterns upfront—they **refactor into** patterns when duplication or rigid conditionals appear, and **refactor out of** patterns when an abstraction has only 1 implementation left.

## Refactoring *To* Patterns
- **Repeated `if/switch` on algorithm variants** → *Replace Conditional Logic with **Strategy*** (or pure function).
- **Complex state-dependent `if (status == ...)`** → *Replace State-Altering Conditionals with **State*** (or `sealed` ADT).
- **Telescoping constructors (`new User(a, b, null, null, true)`)** → *Encapsulate with **Builder*** (or named options).
- **Duplicated logging/retry wrappers** → ***Decorator*** / Higher-Order Function.

## Refactoring *Away From* Patterns
- **`Strategy` interface with 1 implementation for 2 years** → *Inline Class / Collapse Hierarchy*.
- **Verbose `Visitor` in Java 25 / TS / Python** → *Replace with `sealed` ADT + exhaustive `switch`/`match`*.
