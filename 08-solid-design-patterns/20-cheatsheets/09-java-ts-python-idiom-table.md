# Java 25 / TypeScript 5.8 / Python 3.14 Idiom Table

| Concept | Java 25 LTS | TypeScript 5.8 | Python 3.14 |
| :--- | :--- | :--- | :--- |
| **Immutable Value** | `record Money(long cents)` | `readonly` + `as const` | `@dataclass(frozen=True)` |
| **Closed Sum Type (ADT)** | `sealed interface` + `switch` | Discriminated union + `never` | `A \| B` + `match` + `assert_never` |
| **Strategy** | `Function<T, R>` / lambda | `(x: T) => R` | `Callable[[T], R]` |
| **Decorator** | Wrapper / `andThen` | `withX(fn)` HOF | `@decorator` + `@wraps` |
| **Explicit Failure** | `sealed Result<T, E>` / `Optional` | `{ ok: true } \| { ok: false }` | `Ok \| Err` / `T \| None` |
| **Structured Concurrency** | `StructuredTaskScope` + Virtual Threads | `Promise.all` + `AbortController` | `asyncio.TaskGroup()` |
| **Bounded Backpressure** | `ArrayBlockingQueue` / `Flow` | Bounded async queue / `pipeline()` | `asyncio.Queue(maxsize=N)` |

---

## Navigation

| Previous | Up | Next |
| :--- | :--- | :--- |
| [Pattern Selection Flowchart](file:///E:/Learning/engineering-notes/08-solid-design-patterns/20-cheatsheets/08-pattern-selection-flowchart.md) | [Module 20](file:///E:/Learning/engineering-notes/08-solid-design-patterns/20-cheatsheets/README.md) | [Track Root](file:///E:/Learning/engineering-notes/08-solid-design-patterns/README.md) |
