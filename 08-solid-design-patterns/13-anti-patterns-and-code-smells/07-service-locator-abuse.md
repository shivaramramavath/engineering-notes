# Service Locator Abuse

| Field | Value |
| :--- | :--- |
| **Category** | Anti-Pattern (Hidden Dependencies) |
| **Related** | [Dependency Injection](file:///E:/Learning/engineering-notes/08-solid-design-patterns/04-patterns-intro/03-dependency-injection.md), [Plugin & Registry](file:///E:/Learning/engineering-notes/08-solid-design-patterns/09-enterprise-application-patterns/13-plugin-and-registry.md) |
| **Fix** | Explicit Constructor / Function Parameters |

> **TL;DR**: Calling `Locator.get(PaymentGateway.class)` inside arbitrary methods hides what a class actually needs—causing runtime `MissingBindingException` crashes instead of compile-time errors.

## Why Constructor DI Beats Service Locator
With `new OrderService(PaymentGateway gw, OrderRepo repo)`, every dependency is **honest and visible** in the signature; with `Locator.get(...)`, callers have to read the entire implementation body to know what to mock in tests.
