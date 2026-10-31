# Behavioral Patterns: Overview

Behavioral patterns deal with **how objects communicate, share responsibility and vary their behavior**. Where creational patterns are about making objects and structural patterns about assembling them, behavioral patterns are about what happens *between* objects at runtime.

## The ten patterns

Listed in the order they're taught here: most commonly used first.

| Pattern | Problem it solves | One-line idea |
|---|---|---|
| [Strategy](./01-strategy.md) | Multiple interchangeable ways to do the same job | Encapsulate each algorithm behind an interface |
| [Observer](./02-observer.md) | Many objects must react when one changes | Subscribers are notified of events |
| [Command](./03-command.md) | Requests need to be queued, logged or undone | Turn a request into an object |
| [State](./04-state.md) | Behavior depends on state, and `if/switch` on state is spreading | Each state is a class; the object delegates to the current one |
| [Template Method](./05-template-method.md) | Same algorithm skeleton, different steps | Base class fixes the flow; subclasses fill in steps |
| [Chain of Responsibility](./06-chain-of-responsibility.md) | Several handlers might process a request | Pass it along a chain until handled |
| [Iterator](./07-iterator.md) | Traverse a collection without exposing its structure | A separate object walks the elements |
| [Mediator](./08-mediator.md) | Many objects talk to each other in a tangle | A central object coordinates them |
| [Memento](./09-memento.md) | Save and restore state without breaking encapsulation | Snapshot object holds the state |
| [Visitor](./10-visitor.md) | Add operations to a stable class hierarchy | Move the operation into a visitor object |

## Prerequisites

- [Polymorphism](../01-foundations/01-oop-pillars.md) and [programming to interfaces](../01-foundations/04-programming-to-interfaces.md)
- [Open/Closed Principle](../02-solid-principles/02-open-closed.md)

## Grouping by purpose

```text
Varying behavior / algorithms:   Strategy, State, Template Method, Visitor
Communication between objects:   Observer, Mediator, Chain of Responsibility, Command
Traversal and state capture:     Iterator, Memento
```

## Commonly confused pairs

| Pair | Difference |
|---|---|
| **Strategy vs State** | Same structure. Strategy: the *client* picks an interchangeable algorithm. State: the object changes its own behavior as its state changes, and states usually know about each other |
| **Strategy vs Template Method** | Strategy uses composition (swap an object); Template Method uses inheritance (override steps) |
| **Observer vs Mediator** | Observer: one-to-many broadcast, subject doesn't know subscribers. Mediator: many-to-many coordination through one central object that knows all |
| **Command vs Strategy** | Command represents *a request* (a thing to do, possibly later, with data); Strategy represents *how* to do a fixed task |
| **Chain of Responsibility vs Decorator** | A chain handler may *stop* the request; a decorator always delegates |
| **Memento vs Command (undo)** | Memento restores a saved snapshot; Command undoes by reversing an operation |

## Choosing between them

```text
Need to swap an algorithm?                          → Strategy
Behavior changes as the object's state changes?     → State
Same steps, only some vary?                         → Template Method
Notify unknown listeners of events?                 → Observer
Queue, log, retry or undo operations?               → Command
Handlers decide whether to process or pass on?      → Chain of Responsibility
Tangle of peer-to-peer communication?               → Mediator
Need undo through snapshots?                        → Memento
Many operations over a stable object structure?     → Visitor
Walk a collection uniformly?                        → Iterator
```

For comparisons across all categories, see [pattern selection](../13-quick-reference/03-pattern-selection.md).

## Modern Java note

Several behavioral patterns shrink in modern Java:

- Strategy and Command are often just **lambdas** and functional interfaces.
- Iterator is built into the language (`Iterable`, enhanced `for`, streams).
- Observer appears as listeners, event publishers and reactive streams.
- Visitor can often be replaced by **sealed types and pattern matching for `switch`** (Java 21+).

Knowing the pattern still helps you recognize the idea behind the feature.

## Quick Summary

- Behavioral patterns govern communication and behavior variation between objects.
- Strategy, Observer and Command are the most common in everyday code.
- Many patterns share a structure; distinguish them by **intent**.
- Prefer lambdas and language features when they express the idea more simply.

## Next

[01-strategy.md](./01-strategy.md)
