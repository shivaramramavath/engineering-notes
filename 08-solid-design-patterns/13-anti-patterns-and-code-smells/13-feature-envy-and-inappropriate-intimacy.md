# Feature Envy & Inappropriate Intimacy

| Field | Value |
| :--- | :--- |
| **Category** | Code Smell (Misplaced Responsibility) |
| **Violates** | [Tell, Don't Ask](file:///E:/Learning/engineering-notes/08-solid-design-patterns/03-design-principles/09-tell-dont-ask.md) |
| **Fix** | Move Method, Hide Delegate |

> **TL;DR**: **Feature Envy** is a method in Class A that spends 90% of its lines calling getters on Class B to compute something (`order.getCustomer().getAddress().getCountry()`). **Inappropriate Intimacy** is two classes reaching into each other's private internals.

## Fix
Move the calculation **onto the object that owns the data**: `customer.shippingZone()` instead of `orderService` unpacking 4 getters from `customer`.
