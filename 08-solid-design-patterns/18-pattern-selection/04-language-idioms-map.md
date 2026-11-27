# Language Idioms Map (GoF → Java 25 / TypeScript 5.8 / Python 3.14)

| Pattern | Java 25 LTS Idiom | TypeScript 5.8 Idiom | Python 3.14 Idiom |
| :--- | :--- | :--- | :--- |
| **Singleton** | `enum` / DI `@Singleton` | ES Module export / DI | Module-level instance / DI |
| **Builder** | `Builder` / `record` | Options object `{ a?: 1 }` + defaults | Dataclass with keyword defaults |
| **Strategy** | `Function<T, R>` / lambda | `(x: T) => R` function type | `Callable[[T], R]` / function |
| **Decorator** | Wrapper / `Function.andThen` | Higher-order function `withX(fn)` | `@decorator` with `@wraps` |
| **Visitor** | `sealed interface` + `switch` | Discriminated union + `switch` + `never` | Union + `match/case` + `assert_never` |
| **Iterator** | `Iterable` / `Stream` | `function*` / `[Symbol.iterator]` | Generators `(x for x in ...)` |
| **Null Object / Option** | `Optional<T>` | `T \| undefined` / `Option<T>` | `T \| None` |
| **Observer / Events** | `Flow` / Spring Events | `EventEmitter` / RxJS / Signals | Callbacks / `asyncio.Queue` |
