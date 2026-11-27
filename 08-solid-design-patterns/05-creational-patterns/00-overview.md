# Creational Design Patterns Overview

> **One-line summary:** Creational patterns abstract the instantiation process—hiding *which* concrete classes are created, *how* they are assembled, and *how* their lifetimes and pools are managed.

| Attribute | Specification |
|---|---|
| **Level** | Core / Foundational |
| **Prerequisites** | [Module 04: Patterns Intro](file:///e:/Learning/engineering-notes/08-solid-design-patterns/04-patterns-intro/README.md) |
| **Estimated Reading Time** | 15 minutes |
| **Status** | Complete |
| **Category** | Creational Patterns |

---

## 1. Why `new` Is Architectural Glue

Every time business logic writes `new StripePaymentClient(...)`, it creates **compile-time and runtime coupling** to a concrete class:
- You cannot substitute a fake in unit tests without hacks.
- You cannot swap implementations based on environment/config without editing the caller (violating OCP).
- Complex objects with 10 optional fields turn into unreadable telescoping constructors.

---

## 2. Creational Pattern Selection Map

```mermaid
flowchart TD
    Q1{What creation problem<br/>are you solving?}
    Q1 -->|Need 1 shared instance| Sing[01 Singleton\nPrefer DI Singleton!]
    Q1 -->|Need 1 product selected by subclass/type| FM[02 Factory Method /\n06 Static Factory]
    Q1 -->|Need a compatible FAMILY of products| AF[03 Abstract Factory]
    Q1 -->|Too many constructor params / immutable build| Build[04 Builder]
    Q1 -->|Expensive to build from scratch; copy template| Proto[05 Prototype]
    Q1 -->|Expensive sockets/DB connections to reuse| Pool[07 Object Pool]
    Q1 -->|Defer costly creation until first access| Lazy[08 Lazy Init & Holder]
    Q1 -->|One instance per key/tenant| Mult[09 Multiton & Registry]
```

---

[← Module 05 Index](file:///e:/Learning/engineering-notes/08-solid-design-patterns/05-creational-patterns/README.md) | [Next: 01 - Singleton Pattern →](file:///e:/Learning/engineering-notes/08-solid-design-patterns/05-creational-patterns/01-singleton.md)
