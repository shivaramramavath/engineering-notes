# Structural Patterns: Overview

Structural patterns deal with **how classes and objects are composed into larger structures**. They help you combine existing pieces, add behavior around them, or hide their complexity, without rewriting the pieces themselves. Most are built on [composition](../01-foundations/02-composition-vs-inheritance.md) rather than inheritance.

## The seven patterns

| Pattern | Problem it solves | One-line idea |
|---|---|---|
| [Adapter](./01-adapter.md) | An existing class has the wrong interface | Convert one interface into the one clients expect |
| [Bridge](./02-bridge.md) | Two independent dimensions of variation cause a class explosion | Split abstraction from implementation and compose them |
| [Composite](./03-composite.md) | Individual objects and groups should be treated the same | Tree structure with a shared interface |
| [Decorator](./04-decorator.md) | Add behavior dynamically without subclassing | Wrap an object with the same interface |
| [Facade](./05-facade.md) | A subsystem is complicated to use | One simple entry point |
| [Flyweight](./06-flyweight.md) | Huge numbers of similar objects use too much memory | Share common state |
| [Proxy](./07-proxy.md) | Control access to an object | A stand-in with the same interface |

## Prerequisites

- [Composition vs inheritance](../01-foundations/02-composition-vs-inheritance.md)
- [Programming to interfaces](../01-foundations/04-programming-to-interfaces.md)

## The "wrapper" family

Adapter, Decorator, Proxy and Facade all **wrap** something, and their code looks similar. They differ by **intent**.

| Pattern | Interface relative to the wrapped object | Intent |
|---|---|---|
| Adapter | **Different** (converts it) | Make incompatible things work together |
| Decorator | **Same** | Add responsibilities |
| Proxy | **Same** | Control access (lazy load, security, caching, remote) |
| Facade | **New, simpler**, over *many* objects | Hide complexity |

```text
Adapter:    Client ──► [Adapter: wanted interface] ──► Adaptee (different interface)
Decorator:  Client ──► [Decorator] ──► Component   (same interface, adds behavior)
Proxy:      Client ──► [Proxy]     ──► RealSubject (same interface, controls access)
Facade:     Client ──► [Facade] ──► subsystem A, B, C
```

When code looks like a wrapper, ask: **what is it for?** The answer names the pattern.

## Choosing between them

```text
Interfaces don't match, and you can't change one side?       → Adapter
Want to add behavior around an object at runtime?            → Decorator
Want to control or defer access to an object?                → Proxy
Want a simpler API over a group of classes?                  → Facade
Part-whole hierarchy treated uniformly?                      → Composite
Two dimensions (e.g. shape × renderer) multiplying classes?  → Bridge
Millions of similar objects eating memory?                   → Flyweight
```

For comparisons with patterns in other categories, see [pattern selection](../13-quick-reference/03-pattern-selection.md).

## Common misconceptions

- **"Decorator and Proxy are the same."** Same shape, different purpose: decorators add behavior the client chose; proxies manage access to an object.
- **"Adapter and Facade both simplify."** An adapter changes *one* interface to match another; a facade provides a new simpler interface over *many* classes.
- **"Bridge is just an interface."** It's a deliberate split of two hierarchies so each can vary independently.

## Quick Summary

- Structural patterns assemble objects into bigger structures through composition.
- Adapter, Decorator, Proxy and Facade are wrappers distinguished by intent.
- Composite builds trees, Bridge separates two hierarchies, Flyweight saves memory.

## Next

[01-adapter.md](./01-adapter.md)
