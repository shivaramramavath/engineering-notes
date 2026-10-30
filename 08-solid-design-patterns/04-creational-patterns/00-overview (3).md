# Creational Patterns: Overview

Creational patterns deal with **how objects are created**. Using `new` directly ties code to a specific concrete class. Creational patterns move that decision elsewhere so the rest of the code depends on abstractions, in line with the [Dependency Inversion Principle](../02-solid-principles/05-dependency-inversion.md).

## The five patterns

| Pattern | Problem it solves | One-line idea |
|---|---|---|
| [Singleton](./01-singleton.md) | Exactly one instance is needed, with a global access point | Class controls its own instance count |
| [Factory Method](./02-factory-method.md) | Code shouldn't know which concrete class it creates | Subclass (or method) decides what to instantiate |
| [Abstract Factory](./03-abstract-factory.md) | Create *families* of related objects that must match | One factory produces a consistent set |
| [Builder](./04-builder.md) | Constructing a complex object step by step | Separate construction from representation |
| [Prototype](./05-prototype.md) | Creating objects by copying an existing one | Clone a configured instance |

## Prerequisites

- [Programming to interfaces](../01-foundations/04-programming-to-interfaces.md)
- [Dependency injection](../03-patterns-intro/02-dependency-injection.md): in practice, DI containers and composition roots handle much of what these patterns used to do.

## Choosing between them

```text
Need exactly one shared instance?
   └─ yes → let a DI container manage it (Singleton pattern only if necessary)

Is the object complex, with many optional parts?
   └─ yes → Builder

Does the caller need to avoid naming a concrete class?
   ├─ one product type  → Factory Method
   └─ several related products that must match → Abstract Factory

Is copying an existing configured object cheaper or simpler than building a new one?
   └─ yes → Prototype (copy constructor / copy method)
```

## How they relate

- **Factory Method** is often the building block inside **Abstract Factory** (each method of the abstract factory is a factory method).
- **Builder** focuses on *how* one complex object is assembled; **Abstract Factory** focuses on *which family* of objects is produced.
- A factory can return a **Singleton** or a **Prototype** copy.
- In practice, many of these are replaced or hidden by DI containers and language features (records, lambdas).

## Common misconceptions

- **"Factories are always better than `new`."** Plain `new` is fine for simple, stable classes. Add a factory when creation varies or is complicated.
- **"Singleton is the simplest pattern, so use it often."** It is the most misused; it introduces global state and hurts testing.
- **"Builder is only for many constructor arguments."** It is also valuable for validation and immutability.

## Quick Summary

- Creational patterns isolate *which class* gets created and *how*.
- Singleton controls instance count, Factory Method and Abstract Factory choose types, Builder assembles complex objects, Prototype copies.
- Prefer DI and simple factories first; use the heavier patterns when variation justifies them.

## Next

[01-singleton.md](./01-singleton.md)
