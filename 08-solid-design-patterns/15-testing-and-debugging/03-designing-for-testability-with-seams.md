# Designing for Testability with Seams

| Field | Value |
| :--- | :--- |
| **Category** | Testing & Legacy Design |
| **Origin** | Michael Feathers (*Working Effectively with Legacy Code*) |

> **TL;DR**: A **Seam** is a place where you can alter behavior in a program without editing the file itself—allowing tests to substitute a fast Fake in place of a live database or payment API.

## Three Seam Types
1. **Object Seam (Best)**: Polymorphic interface / function parameter (`OrderRepository`, `PaymentGateway`) swapped via constructor/argument.
2. **Link / Module Seam**: Swapping module imports or classpath fakes (`jest.mock`, Python `monkeypatch`, DI `@Primary` test beans).
3. **Subclass & Override Seam**: Making a legacy static call a protected virtual method (`protected Instant now()`) so a test subclass can override it.
