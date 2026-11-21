# 04. Object-Oriented Programming (OOP) in Java

> Comprehensive reference and architecture guide to object-oriented programming, HotSpot memory layouts, inheritance mechanics, virtual method tables, encapsulation boundaries, and object contracts.

---

## Module Overview

Java is a class-based, object-oriented language. Underneath Java's object model lies a sophisticated JVM runtime architecture: 64-bit object headers, Compressed Ordinary Object Pointers (Compressed OOPs), dynamic virtual dispatch via `vtable` and `itable` structures, and JIT inline caching.

This module provides exhaustive, production-grade coverage of core OOP concepts, advanced language features, common traps, and industry best practices.

---

## Document Index

| # | Note | Level | Description | Key Topics |
| :- | :--- | :--- | :--- | :--- |
| 00 | [Classes & Objects](./00_classes-and-objects.md) | Beg-Int | Heap allocation & object memory layout | Mark Word, Klass Pointer, Compressed OOPs, 8-byte alignment |
| 01 | [Constructors & Initialization](./01_constructors.md) | Intermediate | Constructors & Flexible Constructor Bodies | `<init>`, chaining, JEP 513 statements before `super()` |
| 02 | [Static Keyword](./02_static.md) | Intermediate | Fields, methods, `<clinit>`, and Holder pattern | Class variables on Heap, class initialization lock, hiding |
| 03 | [Final Keyword](./03_final.md) | Intermediate | Immutability, inlining, and JMM freeze action | Blank finals, method inlining, JMM safe publication |
| 04 | [Encapsulation & Access Modifiers](./04_encapsulation-and-access-modifiers.md) | Intermediate | Four visibility levels & package boundaries | `private`, package-private, `protected`, `public`, JPMS boundaries |
| 05 | [Initialization Order](./05_initialization-order.md) | Int-Adv | Complete 11-step initialization lifecycle | `<clinit>` vs `<init>`, static blocks, instance blocks, hazards |
| 06 | [Inheritance](./06_inheritance.md) | Intermediate | Single inheritance, overriding, Fragile Base Class | `extends`, `super`, `@Override`, covariant return types, LSP |
| 07 | [Polymorphism](./07_polymorphism.md) | Int-Adv | Dynamic dispatch, vtables, and Pattern Matching | `vtable`/`itable`, monomorphic vs megamorphic JIT inline caching |
| 08 | [Abstract Classes](./08_abstract-classes.md) | Intermediate | Partial abstraction & Template Method pattern | `abstract`, constructors in abstract classes, skeletal implementation |
| 09 | [Interfaces](./09_interfaces.md) | Intermediate | Contracts, defaults, private methods, Diamond problem | `default`, `static`, `private` methods (Java 9), resolution rules |
| 10 | [Composition & Relationships](./10_composition-and-object-relationships.md) | Intermediate | Object relationships & Decorator pattern | "Favor composition over inheritance", Dependency, Aggregation |
| 11 | [Enums](./11_enums.md) | Intermediate | Typesafe enumerations, EnumSet, and singletons | `java.lang.Enum`, bitsets, `EnumMap`, serialization-proof singleton |
| 12 | [Nested & Inner Classes](./12_nested-and-inner-classes.md) | Intermediate | Static nested, member, local, anonymous | `this$0` memory leak, variable capture, Java 11 nestmates |
| 13 | [Object Class](./13_object-class.md) | Intermediate | Root hierarchy methods & demise of finalize | 11 methods, `wait`/`notify`, JEP 421 finalize removal, `Cleaner` |
| 14 | [Equals & HashCode](./14_equals-and-hashcode.md) | Intermediate | Equivalence relations and hash table mechanics | Symmetry/transitivity contract, 31 multiplier, hash collisions |
| 15 | [Clone & Copying](./15_clone-and-copying.md) | Intermediate | Why Cloneable is broken; copy constructors | Shallow vs deep copy, copy constructors, defensive copying |

---

## Conceptual Architecture Map

```
                                  ┌──────────────────────────┐
                                  │     java.lang.Object     │
                                  ├──────────────────────────┤
                                  │ equals(), hashCode()     │
                                  │ toString(), getClass()   │
                                  │ wait(), notify()         │
                                  └─────────────┬────────────┘
                                                │
                 ┌──────────────────────────────┴──────────────────────────────┐
                 ▼                                                             ▼
   ┌──────────────────────────┐                                  ┌──────────────────────────┐
   │     Abstract Classes     │                                  │        Interfaces        │
   ├──────────────────────────┤                                  ├──────────────────────────┤
   │ - Partial implementation │                                  │ - Pure contract          │
   │ - Non-static state/fields│                                  │ - default methods (Java8)│
   │ - Full constructors      │                                  │ - static methods (Java8) │
   │ - Single inheritance     │                                  │ - private methods (Java9)│
   └─────────────┬────────────┘                                  │ - Multiple implementation│
                 │                                               └─────────────┬────────────┘
                 ▼                                                             │
   ┌──────────────────────────┐                                                │
   │     Concrete Classes     │◄───────────────────────────────────────────────┘
   ├──────────────────────────┤
   │ - Final / Sealed         │
   │ - Dynamic vtable dispatch│
   │ - Composition components │
   └──────────────────────────┘
```

---

## Learning Objectives Checklist

- [x] Analyze 64-bit object memory layout (Mark Word, Klass Pointer, Compressed OOPs, padding).
- [x] Trace the complete 11-step class and instance initialization order without error.
- [x] Apply modern Flexible Constructor Bodies (JEP 513) for pre-construction validation.
- [x] Leverage package-private access to design clean library architectural boundaries.
- [x] Resolve multiple-interface Diamond Problem collisions using Java's 3 conflict rules.
- [x] Defend against the Fragile Base Class problem by applying composition and forwarding classes.
- [x] Implement robust, reflection-proof singletons using single-element enums.
- [x] Prevent inner class memory leaks caused by the synthetic `this$0` pointer.
- [x] Fulfill the strict equivalence relation and hashcode contracts for custom domain objects.
- [x] Implement deep copying cleanly using copy constructors rather than the broken `Cloneable` interface.
