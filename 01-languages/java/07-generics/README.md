# Java Generics (`07-generics`)

> Complete guide to Java generics: compile-time type parameterization, bounded types, PECS wildcard variance, bytecode type erasure, synthetic bridge methods, and advanced architectural generic design patterns.

---

## Module Overview

Java generics provide compile-time type safety and eliminate runtime `ClassCastException` while allowing algorithms to operate polymorphically across diverse types. This module provides a rigorous, in-depth reference for generic programming in modern Java (Java 5 through Java 25 LTS).

---

## Notes Directory

| # | Note | Description | Level | Estimated Reading Time |
|---|---|---|---|---|
| 00 | [Generic Classes and Methods](00_generic-classes-and-methods.md) | Parameterized classes, interfaces, generic methods, diamond operator, heap pollution, and `@SafeVarargs`. | Intermediate | 22 min |
| 01 | [Bounded Type Parameters](01_bounded-types.md) | Single upper bounds, multiple bounds, recursive type bounds, and erasure to the first bound. | Intermediate | 24 min |
| 02 | [Wildcards and PECS](02_wildcards-and-pecs.md) | Invariance, covariance (`? extends T`), contravariance (`? super T`), the PECS rule, and wildcard capture. | Advanced | 25 min |
| 03 | [Type Erasure and Bytecode Mechanics](03_type-erasure.md) | Erasure mechanism, synthetic bridge methods (`ACC_BRIDGE`), reification boundaries, and Super Type Tokens. | Advanced | 24 min |
| 04 | [Advanced Generic Design Patterns](04_generic-patterns.md) | Typesafe Heterogeneous Containers, CRTP, Phantom Types, compile-time state machines, and dynamic casting. | Advanced | 25 min |

---

## Core Principles Quick Reference

### The PECS Rule
- **Producer Extends**: If the container produces data to be read by your method, use `<? extends T>`.
- **Consumer Super**: If the container consumes data written by your method, use `<? super T>`.
- **Both**: Use exact invariant type parameter `<T>`.

### Invariance vs Arrays
- **Arrays are Covariant and Reified**: `String[]` is a `Object[]`, checked at runtime (`ArrayStoreException`).
- **Generics are Invariant and Erased**: `List<String>` is NOT a `List<Object>`, checked strictly at compile time.

### Bytecode Translation
- Generic type variables are erased to `Object` or their first upper bound.
- The compiler inserts synthetic `checkcast` instructions at invocation sites.
- The compiler generates synthetic `ACC_BRIDGE` methods when overriding generic methods to preserve polymorphic dispatch.

---

## Prerequisites
- [04-oop](../04-oop/README.md) - Object-Oriented Programming (Classes, Inheritance, Polymorphism, Interfaces)
