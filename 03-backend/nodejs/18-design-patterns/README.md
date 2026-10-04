# 20 · Design Patterns

A **design pattern** is a named, reusable solution to a problem that shows up again and again in software design. Patterns are not libraries or copy-paste code. They are shared vocabulary: saying "use a strategy here" or "wrap it in an adapter" communicates a whole design in a few words.

JavaScript changes how classic patterns look. Functions are first-class values, objects can be created without classes, and modules give you encapsulation for free. Many patterns from class-based languages shrink to a few lines of JavaScript, and some (like the singleton) are nearly built in.

## What you will learn

- Nine patterns that appear constantly in real JavaScript and TypeScript code
- How each pattern is written idiomatically in modern JavaScript (functions, closures, modules, classes)
- When each pattern helps and when it adds needless complexity
- How patterns relate to each other and to the language features you already know

## Contents

| # | File | Category | Problem it solves |
|---|------|----------|-------------------|
| 01 | [Module Pattern](./01_module-pattern.md) | Structural | Encapsulate private state and expose a small public API |
| 02 | [Factory Pattern](./02_factory-pattern.md) | Creational | Create objects without exposing construction details |
| 03 | [Singleton Pattern](./03_singleton-pattern.md) | Creational | Guarantee exactly one shared instance |
| 04 | [Builder Pattern](./04_builder-pattern.md) | Creational | Construct complex objects step by step |
| 05 | [Strategy Pattern](./05_strategy-pattern.md) | Behavioral | Swap algorithms at runtime |
| 06 | [Observer Pattern](./06_observer-pattern.md) | Behavioral | Notify many listeners when something changes |
| 07 | [Adapter Pattern](./07_adapter-pattern.md) | Structural | Make incompatible interfaces work together |
| 08 | [Decorator Pattern](./08_decorator-pattern.md) | Structural | Add behavior without modifying the original |
| 09 | [Dependency Injection](./09_dependency-injection.md) | Architectural | Pass dependencies in instead of creating them inside |

## The three classic categories

| Category | Concern | Patterns here |
|----------|---------|---------------|
| **Creational** | How objects are created | Factory, Singleton, Builder |
| **Structural** | How objects and modules are composed | Module, Adapter, Decorator |
| **Behavioral** | How objects communicate and share responsibility | Strategy, Observer |

Dependency injection is less a pattern than a **design technique** that makes many other patterns easier to apply and test.

## Prerequisites

- [Functions](../02_functions/00_README.md) and [Higher-Order Functions](../02_functions/06_higher-order-functions.md)
- [Closures](../06_closures/00_README.md)
- [`this` and OOP](../05_this-and-oop/00_README.md): classes, prototypes, composition
- [Modules](../13_modules/00_README.md)

## How to choose

| If you need to... | Reach for |
|-------------------|-----------|
| Hide internals and expose a clean API | Module |
| Decide which object or function to create at runtime | Factory |
| Share exactly one instance across the app | Singleton (often just a module-level export) |
| Build an object with many optional settings | Builder |
| Swap an algorithm or behavior | Strategy |
| React to changes without tight coupling | Observer |
| Use a library or API whose shape does not match yours | Adapter |
| Add logging, caching, retry, auth around existing behavior | Decorator |
| Make code easy to test and reconfigure | Dependency injection |

## A note on over-engineering

Patterns solve specific problems. Applying one without that problem makes code harder to read, not easier. A good rule: **write the simple version first, and reach for a pattern when you feel the pain it addresses** (duplicated construction logic, a growing `if/else` chain, tight coupling that blocks testing).

```js
// You do not need a StrategyFactoryManager for this
const greet = (name) => `Hello, ${name}`;
```

In JavaScript, a plain function, an object literal, or a module is often the entire "pattern".

## Key takeaways

- Patterns are named solutions to recurring problems, and a shared vocabulary for design discussions
- JavaScript's first-class functions, closures, and modules make many patterns much lighter than in classic OOP languages
- Group them as creational (making objects), structural (composing objects), and behavioral (object interaction)
- Prefer the simplest thing that works; introduce a pattern when its problem actually appears
- Dependency injection underpins testable code and pairs naturally with strategy, factory, and adapter

**Next:** [Module Pattern](./01_module-pattern.md)
