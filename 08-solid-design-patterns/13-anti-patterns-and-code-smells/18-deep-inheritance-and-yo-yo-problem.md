# Deep Inheritance & Yo-Yo Problem

| Field | Value |
| :--- | :--- |
| **Category** | Anti-Pattern (Hierarchy Fragility) |
| **Violates** | [Composition Over Inheritance](file:///E:/Learning/engineering-notes/08-solid-design-patterns/01-foundations/README.md) |
| **Fix** | Replace Inheritance with Composition / Strategy / Decorator |

> **TL;DR**: A 5-level inheritance tree (`BaseEntity → AuditableEntity → CachedEntity → OrderBase → RetailOrder`) forces you to bounce up and down 5 files (**Yo-Yo Problem**) just to understand what `save()` does—and changing a base method breaks unexpected subclasses (**Fragile Base Class**).

## Fix
Keep inheritance shallow (**1–2 levels max**), or prefer **Composition** (inject `AuditLogger`, `Cache` into `Order`).
