# 17 - Design Patterns

Eight recurring solutions to recurring structural problems, written the way they actually look in TypeScript. Classic design-pattern books assume class hierarchies and mutable objects. TypeScript offers other tools: first-class functions, discriminated unions, structural typing, generics, and closures. Many patterns shrink to a few lines, and the type system can enforce parts that other languages leave to convention.

The goal is not to apply patterns for their own sake. Each note says what problem the pattern solves, shows the TypeScript-idiomatic version, and says when a simpler approach is better.

## Prerequisites

- [04 Objects and Interfaces](../04-objects-and-interfaces/README.md) and [05 Classes](../05-classes/README.md): interfaces, `implements`, class basics
- [06 Generics](../06-generics/README.md): generic factories, strategies, and emitters
- [03 Unions and Narrowing](../03-unions-and-narrowing/README.md): discriminated unions drive state machines
- [11 Error Handling](../11-error-handling/README.md) and [12 Async and Iteration](../12-async-and-iteration/README.md): used throughout the examples

## Notes in this section

| # | Note | Kind | Covers |
|---|---|---|---|
| 00 | [Factory](./00-factory.md) | creational | simple factories, `Record` lookup tables, argument-dependent return types, static factory methods, registries |
| 01 | [Builder](./01-builder.md) | creational | options objects first, fluent builders, type-state builders for required steps, test data builders |
| 02 | [Strategy](./02-strategy.md) | behavioral | interchangeable behavior as functions, strategy tables, generics, strategy vs union and `switch` |
| 03 | [Adapter](./03-adapter.md) | structural | isolating vendors and legacy code, mapper functions, callbacks to promises, anti-corruption layers |
| 04 | [Repository](./04-repository.md) | architectural | collection-like persistence interface, in-memory fakes, mapping, "not found", transactions, contract tests |
| 05 | [Dependency injection](./05-dependency-injection.md) | architectural | constructor and function injection, composition root, containers and tokens, lifetimes, service locator |
| 06 | [State machines](./06-state-machines.md) | behavioral | discriminated unions for states, pure transitions, tables, guards, effects as data |
| 07 | [Typed event emitter](./07-typed-event-emitter.md) | behavioral | event maps, `on`/`off`/`emit` typed with generics, cleanup, errors in listeners |

Notes can be read in any order. 03, 04, and 05 are closely related: a repository is an adapter for persistence, and dependency injection is how you supply either one.

## Which pattern fits my problem?

| Problem | Pattern |
|---|---|
| Callers should not know which concrete class they get | [Factory](./00-factory.md) |
| A function takes too many optional arguments | options object, then [Builder](./01-builder.md) |
| I need compile-time enforcement of required steps | type-state [Builder](./01-builder.md) |
| I need valid test objects with small per-test changes | test data builder ([01](./01-builder.md)) |
| Behavior varies and should be swappable (pricing, shipping, auth) | [Strategy](./02-strategy.md) |
| A third-party SDK or legacy API has the wrong shape | [Adapter](./03-adapter.md) |
| Business logic is tangled with SQL or ORM calls | [Repository](./04-repository.md) |
| I cannot test a class without a real database or network | [Dependency injection](./05-dependency-injection.md) |
| I have several boolean flags that can contradict each other | [State machine](./06-state-machines.md) |
| Many independent parts need to react to something | [Typed event emitter](./07-typed-event-emitter.md) |

## Ideas that recur across the section

- **Depend on an interface you own.** Factories, adapters, repositories, and DI all put a small interface between your code and something you do not control.
- **Functions are often enough.** A strategy, a factory, and an adapter can each be a function. Reach for classes when you need state or several operations.
- **Let the type system enforce the pattern.** `Record<Union, ...>` forces every case. Discriminated unions make illegal states unrepresentable. Generics tie a name to its payload.
- **Construct in one place.** Create real objects in a composition root or factory, not scattered through business code.
- **Make behavior testable.** Each pattern here makes it possible to substitute a fake: in-memory repository, stub strategy, fake gateway, injected clock.
- **Do not over-apply.** A pattern that wraps a single call, or serves two cases, adds indirection without paying for it.

## A note on trade-offs

Every pattern buys flexibility at a cost: more types, more files, more places to look. Introduce one when you feel the problem it solves: duplicated branching, painful tests, a vendor you may replace, flags that keep contradicting each other. If you cannot name the problem, wait.

## Related sections

- [07 Utility Types](../07-utility-types/README.md): `Extract`, `ReturnType`, `ConstructorParameters` used in factories and state types
- [10 Advanced Types](../10-advanced-types/README.md): mapped, template literal, and variadic types behind typed emitters and builders
- [16 Type-Safe APIs: DTO pattern](../16-type-safe-apis/02-dto-pattern.md): mapping at boundaries, like adapters and repositories
- [18 Testing and Debugging](../18-testing-and-debugging/README.md): fakes, mocks, and contract tests
- [19 React and Frontend](../19-react-and-frontend/README.md): reducers, context, and state
- [20 Node.js Backend](../20-nodejs-backend/README.md): service and repository layers, NestJS
- [24 Best Practices](../24-best-practices/README.md)
- [27 Interview: design and architecture questions](../27-interview/10-design-and-architecture-questions.md)

## Next

[18 Testing and Debugging](../18-testing-and-debugging/README.md)
