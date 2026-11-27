# Anemic Domain Model (Code Smell View)

| Field | Value |
| :--- | :--- |
| **Category** | Anti-Pattern (Procedural OOP) |
| **Full Deep-Dive** | [Anemic vs Rich Domain Model](file:///E:/Learning/engineering-notes/08-solid-design-patterns/09-enterprise-application-patterns/11-anemic-vs-rich-domain-model.md) |
| **Fix** | Move invariants & state transitions into Entities & Value Objects |

> **TL;DR**: Domain entities reduced to pure getter/setter structs while all business rules live in procedural `*Service` classes—allowing any caller to put entities into illegal states (`order.setStatus("SHIPPED")` before payment).

## Quick Diagnostic
- Are all fields exposed via public setters?
- Are `if (order.getStatus() == ...)` checks repeated across 4 different services?
- **Fix**: Replace `setStatus()` with `order.pay()` / `order.ship()`.
