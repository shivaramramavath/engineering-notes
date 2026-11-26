# Design Patterns: Introduction Overview

The previous sections gave you the principles: low coupling, high cohesion, depend on abstractions. This section introduces **design patterns**, which are named, reusable solutions to problems that show up again and again when applying those principles.

Two files in this folder:

| File | Question it answers |
|---|---|
| [01-what-are-patterns.md](./01-what-are-patterns.md) | What is a design pattern, how are patterns organized, and how do I read and choose one? |
| [02-dependency-injection.md](./02-dependency-injection.md) | How do objects get their collaborators? The technique almost every pattern relies on. |

## Prerequisites

- [Programming to interfaces](../01-foundations/04-programming-to-interfaces.md)
- [Composition vs inheritance](../01-foundations/02-composition-vs-inheritance.md)
- [SOLID principles](../02-solid-principles/00-overview.md), especially OCP and DIP

## Why dependency injection lives here

Dependency injection is not a Gang of Four pattern, but nearly every pattern that follows (Strategy, Decorator, Observer, Factory, Repository) assumes you can *hand an object its collaborators* from outside. Learning it first removes a lot of friction later.

## What comes next

```text
03-patterns-intro        (you are here: vocabulary + DI)
   ↓
04-creational-patterns   how objects get created
05-structural-patterns   how objects are assembled into larger structures
06-behavioral-patterns   how objects communicate and share work
   ↓
07-application-patterns  patterns for data access and business logic layers
```

## How to read the pattern files

Every pattern file in the next sections follows the same spine, adapted to the pattern:

1. **Problem**: the situation that motivates it.
2. **Structure**: participants and relationships.
3. **Example**: minimal, realistic Java code.
4. **Trade-offs**: what it costs.
5. **Related patterns**: what it is often confused with, and what it combines with.

You don't need to memorize all patterns. Know the problem each one solves, and look up the details when you meet that problem.

## Quick Summary

- Patterns are shared vocabulary for common design problems.
- Dependency injection is the supporting technique; learn it before the patterns.
- Learn a pattern by the *problem* it solves, not by its class diagram.

## Next

[01-what-are-patterns.md](./01-what-are-patterns.md)
