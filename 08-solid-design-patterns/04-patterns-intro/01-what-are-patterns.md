# What Are Design Patterns?

A design pattern is a **named, proven solution to a recurring design problem**, described at a level that is independent of any one codebase. It is not a library or a piece of code you copy; it's a template for how to arrange classes and objects so a particular kind of problem becomes easy to solve and easy to change.

The main value is **shared vocabulary**. Saying "we'll put a Decorator around that" or "this is basically an Observer" communicates a whole design in a few words.

## Where patterns come from

The idea was borrowed from architecture (Christopher Alexander). It reached software through the 1994 book *Design Patterns: Elements of Reusable Object-Oriented Software* by Gamma, Helm, Johnson and Vlissides, known as the **Gang of Four (GoF)**. It documented 23 patterns, and those are what this repository's folders 04 to 06 cover. Patterns observed since then (Repository, Unit of Work, Specification and others) are covered in [application patterns](../07-application-patterns/00-overview.md).

## Anatomy of a pattern

Each pattern is described by roughly the same parts:

| Part | Purpose |
|---|---|
| Name | Shared vocabulary |
| Problem | When it applies, and what forces are in tension |
| Solution | The participants (classes/objects) and how they collaborate |
| Consequences | Benefits *and* costs; no pattern is free |

If a description lacks a problem and consequences, it's just a code recipe.

## Classification

The GoF patterns are grouped by purpose.

| Category | Concern | Patterns |
|---|---|---|
| **Creational** | How objects are created | Singleton, Factory Method, Abstract Factory, Builder, Prototype |
| **Structural** | How objects are composed into larger structures | Adapter, Bridge, Composite, Decorator, Facade, Flyweight, Proxy |
| **Behavioral** | How objects communicate and share responsibility | Chain of Responsibility, Command, Iterator, Mediator, Memento, Observer, State, Strategy, Template Method, Visitor |

A second axis is scope:

- **Class patterns** rely on inheritance and are fixed at compile time (Factory Method, Template Method, class-form Adapter).
- **Object patterns** rely on composition and can change at runtime (most of the rest).

Object patterns dominate, which reflects the idea in [composition vs inheritance](../01-foundations/02-composition-vs-inheritance.md).

## Patterns vs related concepts

| Concept | Level | Example |
|---|---|---|
| Idiom | Language-specific trick | Try-with-resources in Java |
| Principle | General guideline | Single Responsibility |
| **Design pattern** | Class/object collaboration | Strategy, Observer |
| Architectural pattern | System structure | Layered, hexagonal, event-driven |

Principles say *what to aim for*. Patterns show *one way to get there*. Architectural patterns apply similar thinking at a larger scale (see [architecture](../08-architecture/00-overview.md)).

## How patterns relate to SOLID

Most patterns are applications of the principles you already know.

| Pattern | Principle it embodies |
|---|---|
| Strategy, Decorator | Open/Closed |
| Factory Method, Abstract Factory | Dependency Inversion |
| Adapter, Facade | Interface Segregation, decoupling |
| Observer | Loose coupling, OCP |

## How to use a pattern

```text
1. Identify the problem (what is hard to change, test or understand?)
        ↓
2. Check whether a pattern addresses *that* problem
        ↓
3. Read its Consequences: is the cost acceptable here?
        ↓
4. Apply the smallest version that solves the problem
        ↓
5. Name it in code or docs so others recognize it
```

Start from the problem, never from the pattern. "Where can I use Strategy?" produces forced designs. "This `switch` keeps growing" leads naturally to Strategy.

For choosing between similar patterns, see [pattern selection](../13-quick-reference/03-pattern-selection.md).

## Patterns and modern languages

Some patterns exist partly to work around language limits, and some languages make them nearly invisible.

| Pattern | In modern Java |
|---|---|
| Strategy / Command | Often just a lambda or functional interface |
| Iterator | Built in (`Iterable`, `for-each`, streams) |
| Observer | Listeners, reactive streams |
| Singleton | Usually replaced by a DI container managing one instance |
| Builder | Frequently a record with factory methods, or a library-generated builder |

Knowing the pattern still matters: it explains the idea behind the language feature.

## When not to use patterns

- When a plain method or `if` is clearer.
- When you are guessing about future requirements (YAGNI).
- When the team doesn't recognize the pattern and the benefit is small.
- When the pattern adds layers but doesn't remove a real pain.

Overuse is common; see [avoiding overengineering](../10-production/05-avoiding-overengineering.md).

## Common misconceptions

- **"Patterns are code you copy."** They are *designs* you adapt.
- **"More patterns means better design."** The best design uses the fewest that solve the problem.
- **"Patterns are only for Java/C++."** They describe object relationships, so they apply in any language with objects or closures.
- **"A pattern is correct only in its textbook form."** Real implementations vary; the intent is what matters.
- **"Singleton is a good default."** It's the most overused and often hides global state.

## Quick Summary

- A pattern is a named solution to a recurring problem, with explicit trade-offs.
- GoF patterns are grouped as creational, structural and behavioral.
- Patterns implement principles like OCP and DIP, mostly through composition.
- Choose by problem, apply the simplest form, and don't use a pattern without a pain to solve.

## Next

[02-dependency-injection.md](./02-dependency-injection.md), then [creational patterns](../04-creational-patterns/00-overview.md).
