# Foundations: Overview

SOLID and design patterns are not new ideas on top of OOP. They are answers to a few problems that OOP creates when code grows: classes that know too much about each other, hierarchies that are hard to change, and behavior that is hard to swap or test. This folder covers the ideas those answers are built on.

If you skip it, SOLID reads like a list of rules to memorize. If you read it first, each principle looks like a consequence of something you already understand.

## The core problem

Software changes. The goal of everything in this repository is to make a change in one place *not* force changes in ten others.

```text
Requirement changes
        ↓
How many places must I edit?
        ↓
   Few → low coupling, high cohesion
   Many → tight coupling, scattered responsibility
```

## What's in this folder

| File | Question it answers |
|---|---|
| [01-oop-pillars.md](./01-oop-pillars.md) | What are encapsulation, abstraction, inheritance and polymorphism, and what is each one for? |
| [02-composition-vs-inheritance.md](./02-composition-vs-inheritance.md) | When should one class *be* another, and when should it just *use* another? |
| [03-coupling-and-cohesion.md](./03-coupling-and-cohesion.md) | How do I judge a design in concrete terms? |
| [04-programming-to-interfaces.md](./04-programming-to-interfaces.md) | How do I make parts of a system replaceable? |

## How the pieces connect

```text
OOP pillars ──────────► tools you have
   │
   ├─ inheritance ──┐
   │                ├──► composition vs inheritance: choosing between them
   └─ polymorphism ─┘
                         │
coupling & cohesion ◄────┘    the criteria for judging the choice
   │
   ▼
programming to interfaces     the main technique for lowering coupling
   │
   ▼
SOLID (next folder)           the principles that formalize all of this
```

## Common misconceptions

- **"OOP means using inheritance."** Inheritance is one tool, and often the weakest. Much of good OOP design is avoiding it.
- **"Encapsulation means private fields with getters and setters."** Exposing every field through accessors hides nothing. Encapsulation hides *decisions*, not just data.
- **"An interface is a keyword."** An interface is any contract callers depend on. The language keyword is one way to express it.
- **"Low coupling is always the goal."** Some coupling is unavoidable. Aim to couple to stable things (abstractions), not volatile ones (concrete implementations).

## How to use this folder

- **Learning:** read 01 to 04 in order.
- **Reference:** jump to 03 when reviewing a design, and to 02 when someone proposes a new base class.
- **Short on time:** read 03 first. Its vocabulary is used in every later folder.

> Code examples in this repository are written in Java.

## Quick Summary

- Foundations explain *why* SOLID and patterns exist.
- The recurring goal is limiting how many places a change touches.
- Prefer composition and interfaces over inheritance in most designs.
- Coupling and cohesion are how you judge a design.

## Next

Start with [01-oop-pillars.md](./01-oop-pillars.md). After this folder, continue to [../02-solid-principles/00-overview.md](../02-solid-principles/00-overview.md). Unfamiliar terms are in [../GLOSSARY.md](../GLOSSARY.md).
