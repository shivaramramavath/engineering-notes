# Module 17: Design Patterns

> Comprehensive guide and curriculum for applying Gang of Four (GoF) and modern architectural design patterns in TypeScript.

## Overview
This module explores classic and modern software design patterns refactored for TypeScript's structural type system, first-class functions, discriminated unions, and generic type-state builders.

## Module Structure

| Note | Title | Description | Baseline |
| :--- | :--- | :--- | :--- |
| **[00-factory.md](./00-factory.md)** | Factory & Abstract Factory | Functional factories, static creation methods, and type-safe registry maps. | TS 4.5+ |
| **[01-builder.md](./01-builder.md)** | Builder & Fluent APIs | Fluent method chaining, compile-time mandatory property verification (Type-State). | TS 4.5+ |
| **[02-strategy.md](./02-strategy.md)** | Strategy Pattern | Decoupling algorithms from contexts using first-class functions and strategy maps. | TS 4.5+ |
| **[03-adapter.md](./03-adapter.md)** | Adapter & Facade | Anti-corruption layers (ACL), bridging incompatible interfaces, and subsystem facades. | TS 4.5+ |
| **[04-repository.md](./04-repository.md)** | Repository & Unit of Work | Decoupling persistence from domain models, transaction boundaries, and specifications. | TS 4.5+ |
| **[05-dependency-injection.md](./05-dependency-injection.md)** | Dependency Injection & IoC | Constructor injection, lightweight type-safe IoC containers, and lifecycle management. | TS 4.5+ |
| **[06-state-machines.md](./06-state-machines.md)** | State Machine Pattern (FSM) | Discriminated union states, impossible state elimination, and pure transition reducers. | TS 4.5+ |
| **[07-typed-event-emitter.md](./07-typed-event-emitter.md)** | Typed Event Emitter & Observer | Strict event-to-payload mappings, memory leak prevention, and reactive subjects. | TS 4.5+ |

## Learning Path & Prerequisites
- Complete **[05-classes](../05-classes/README.md)** and **[06-generics](../06-generics/README.md)** before starting this module.
- After completing this module, proceed to **[18-testing-and-debugging](../18-testing-and-debugging/README.md)**.
