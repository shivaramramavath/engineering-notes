# Primitive Obsession

| Field | Value |
| :--- | :--- |
| **Category** | Code Smell (Data Modeling) |
| **Fix** | [Value Objects](file:///E:/Learning/engineering-notes/08-solid-design-patterns/09-enterprise-application-patterns/15-value-object-entity-aggregate.md) (`Money`, `Email`, `UserId`) |

> **TL;DR**: Using raw `string`, `int`, `float` everywhere for domain concepts (`charge(String userId, String orderId, double amount, String currency)`) causes swapped-argument bugs, floating-point money rounding errors, and duplicated regex checks.

## Before vs After
```java
// ❌ Easy to swap userId & orderId; float rounding bugs!
void refund(String userId, String orderId, double amount) {}

// ✅ Compiler catches swapped IDs; Money enforces non-negative cents!
record UserId(String value) {}
record OrderId(String value) {}
record Money(long cents, String currency) {}
void refund(UserId userId, OrderId orderId, Money amount) {}
```
