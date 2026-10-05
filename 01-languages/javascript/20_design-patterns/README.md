# 20 · Design Patterns

Design patterns are named, reusable solutions to problems that keep showing up when structuring code. They give developers a shared vocabulary ("wrap it in a decorator", "inject that") and a set of tradeoffs to weigh, not templates to copy.

JavaScript changes how these patterns look. Functions are values, objects are created without classes, and closures give private state. Several patterns that need class hierarchies in other languages collapse into a function or an object literal here. These notes show the **JavaScript-shaped** version of each pattern, and say when the pattern isn't needed.

---

## Prerequisites

- [Closures](../06_closures/01_closures.md): private state and many pattern implementations rely on them
- [Higher-Order Functions](../02_functions/06_higher-order-functions.md): strategy and decorator in function form
- [Classes](../05_this-and-oop/05_classes.md) and [Private Fields](../05_this-and-oop/07_private-fields.md): the class-based versions
- [ES Modules](../13_modules/01_es-modules.md): modules cache instances, which affects singleton and module patterns

---

## Notes in This Section

| # | Note | The problem it solves |
| --- | --- | --- |
| 01 | [Module Pattern](./01_module-pattern.md) | Hide state behind a small public API |
| 02 | [Factory Pattern](./02_factory-pattern.md) | Centralize creation and choose types at runtime |
| 03 | [Singleton Pattern](./03_singleton-pattern.md) | Exactly one shared instance, and when that's a trap |
| 04 | [Builder Pattern](./04_builder-pattern.md) | Construct complex objects step by step, with validation |
| 05 | [Strategy Pattern](./05_strategy-pattern.md) | Swap algorithms without `if/else` chains |
| 06 | [Observer Pattern](./06_observer-pattern.md) | Notify many listeners without coupling to them |
| 07 | [Adapter Pattern](./07_adapter-pattern.md) | Make an incompatible interface fit the one you want |
| 08 | [Decorator Pattern](./08_decorator-pattern.md) | Add behavior to a function or object, same interface |
| 09 | [Dependency Injection](./09_dependency-injection.md) | Pass dependencies in so code is testable and configurable |

Read them in order the first time. 01 → 03 are about creating and owning things, 04 → 05 about shaping behavior, 06 → 08 about connecting and wrapping, and 09 ties several of them together.

---

## Picking a Pattern

Start from the problem, not the pattern name.

| If you notice... | Consider |
| --- | --- |
| Object creation logic is duplicated or branches on a type | [Factory](./02_factory-pattern.md) |
| A long `if/else` or `switch` chooses between algorithms | [Strategy](./05_strategy-pattern.md) |
| A constructor or options object has many optional, interdependent parts | [Builder](./04_builder-pattern.md) |
| Several parts of the app must react to a change | [Observer](./06_observer-pattern.md) |
| A library's API doesn't match what your code expects | [Adapter](./07_adapter-pattern.md) |
| You want logging, caching, or retries without touching core logic | [Decorator](./08_decorator-pattern.md) |
| Tests need real databases or clocks, or config is hard-wired | [Dependency Injection](./09_dependency-injection.md) |
| You need private state | [Module Pattern](./01_module-pattern.md) or [Private Fields](../05_this-and-oop/07_private-fields.md) |
| You think you need "only one of these" | Check [Singleton](./03_singleton-pattern.md) first. A module export, or passing one instance in, is usually enough |

---

## Things Worth Remembering

- **A pattern is a response to a problem, not a goal.** If the plain version is simple and clear, leave it alone.
- **Patterns often combine.** A factory that returns decorated, injected objects is common.
- **Native APIs already use them.** `sort` comparators (strategy), `addEventListener` (observer), `Array.from` (factory), `util.promisify` (adapter). Recognizing them in the platform is half the value.
- **Names help communication, not correctness.** Teams don't need to agree on exact textbook definitions, only on what the code is doing.

---

## Next

- [Testing](../21_testing/README.md): where dependency injection and mocking pay off
- [Real-World Patterns](../23_real-world-patterns/README.md): practical utilities built from these ideas (retry, caching, event emitter)