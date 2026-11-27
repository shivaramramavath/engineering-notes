# Module 05: Creational Design Patterns

Welcome to **Module 05: Creational Design Patterns**. Calling `new ConcreteClass()` directly inside business logic hardwires your domain policy to concrete implementations, making unit testing, swapping implementations, and assembling complex immutable objects brittle. Creational patterns decouple **how objects are constructed, pooled, and initialized** from the code that uses them.

---

## Module Roadmap & Contents

| # | Note | Core Focus & Purpose | Status | Est. Time |
|---|---|---|:---:|:---:|
| 00 | [Creational Patterns Overview](file:///e:/Learning/engineering-notes/08-solid-design-patterns/05-creational-patterns/00-overview.md) | Why `new` is dangerous in domain code, taxonomy of Creational Patterns | Complete | 15 min |
| 01 | [Singleton Pattern](file:///e:/Learning/engineering-notes/08-solid-design-patterns/05-creational-patterns/01-singleton.md) | Single instance guarantees, thread-safe Bill Pugh / Enum, why it is often an anti-pattern | Complete | 30 min |
| 02 | [Factory Method Pattern](file:///e:/Learning/engineering-notes/08-solid-design-patterns/05-creational-patterns/02-factory-method.md) | Delegating instantiation of a single product to subclasses/creators (OCP) | Complete | 30 min |
| 03 | [Abstract Factory Pattern](file:///e:/Learning/engineering-notes/08-solid-design-patterns/05-creational-patterns/03-abstract-factory.md) | Creating compatible families of related objects without knowing concrete classes | Complete | 30 min |
| 04 | [Builder Pattern](file:///e:/Learning/engineering-notes/08-solid-design-patterns/05-creational-patterns/04-builder.md) | Step-by-step construction of complex immutable objects without telescoping constructors | Complete | 30 min |
| 05 | [Prototype Pattern](file:///e:/Learning/engineering-notes/08-solid-design-patterns/05-creational-patterns/05-prototype.md) | Cloning pre-configured instances, shallow vs deep copy traps | Complete | 25 min |
| 06 | [Simple Factory & Static Factory Methods](file:///e:/Learning/engineering-notes/08-solid-design-patterns/05-creational-patterns/06-simple-factory-and-static-factory-methods.md) | Effective Java Item 1: named constructors, caching, subtype returns | Complete | 25 min |
| 07 | [Object Pool Pattern](file:///e:/Learning/engineering-notes/08-solid-design-patterns/05-creational-patterns/07-object-pool.md) | Reusing expensive connections/sockets (HikariCP), lifecycle & leak prevention | Complete | 30 min |
| 08 | [Lazy Initialisation & Holder Idiom](file:///e:/Learning/engineering-notes/08-solid-design-patterns/05-creational-patterns/08-lazy-initialisation-and-holder.md) | Deferring expensive instantiation safely across threads (DCL, `Lazy<T>`, `functools`) | Complete | 25 min |
| 09 | [Multiton & Registry Pattern](file:///e:/Learning/engineering-notes/08-solid-design-patterns/05-creational-patterns/09-multiton-and-registry.md) | Keyed singleton maps and dynamic plugin registries | Complete | 25 min |
| 10 | [Dependency Injection vs Factories](file:///e:/Learning/engineering-notes/08-solid-design-patterns/05-creational-patterns/10-dependency-injection-vs-factories.md) | When to inject a service directly vs inject a factory for runtime-parameterized creation | Complete | 25 min |
| 11 | [Creational Patterns Comparison](file:///e:/Learning/engineering-notes/08-solid-design-patterns/05-creational-patterns/11-creational-patterns-comparison.md) | Decision tree, trade-off matrix, and language idiom shortcuts | Complete | 15 min |

---

## Core Learning Outcomes

By completing this module, you will:
1. Eliminate hardwired `new` coupling in domain logic using **Factory Method**, **Abstract Factory**, and **Dependency Injection**.
2. Construct complex immutable objects cleanly with **Builder** (and know when TypeScript options / Python keyword args replace it).
3. Avoid **Singleton** global-state test traps and understand thread-safe initialization (**Enum**, **Initialization-on-Demand Holder**, **DCL**).
4. Manage scarce, expensive resources (DB connections, worker buffers) safely with **Object Pool**.

---

[← Back to Module 04: Patterns Intro](file:///e:/Learning/engineering-notes/08-solid-design-patterns/04-patterns-intro/README.md) | [Next: 00 - Creational Patterns Overview →](file:///e:/Learning/engineering-notes/08-solid-design-patterns/05-creational-patterns/00-overview.md)
