# Functional Patterns

Lambdas, functional interfaces, `Optional`, and streams are tools. This note covers the **patterns that make them pay off in real Java code**: how to compose small functions, replace boilerplate design patterns, handle errors without breaking pipelines, and know when *not* to be functional.

Java is not Haskell. The goal is not purity, but code that is easier to test, compose, and reason about.

**Prerequisites:** [Lambda Expressions](00_lambda-expressions.md), [Functional Interfaces](01_functional-interfaces.md), [Method References](02_method-references.md), [Optional](03_optional.md), [Stream Operations](05_stream-operations.md).

---

## 1. Core ideas

### Pure functions

A function is **pure** if its result depends only on its arguments and it has no side effects.

```java
// Pure: same input → same output, nothing else touched
int discounted(int price, int percent) { return price - price * percent / 100; }

// Impure: reads hidden state, writes external state
int discountedImpure(int price) {
    auditLog.add("discount");           // side effect
    return price - price * currentPromo.percent() / 100;   // hidden input
}
```

Pure functions are trivially testable, safe to cache, and safe to run in parallel ([Parallel Streams](07_parallel-streams.md)). Keep the core logic pure and push I/O and mutation to the edges of the application.

### Immutability

Data flowing through functions should not be modified. Records, `List.of`, `Stream.toList()`, and `final` fields do most of the work. See [Immutability](../23-design-and-clean-code/04_immutability.md).

### Functions as values (higher-order functions)

A function can be passed in, returned, and stored:

```java
static <T> List<T> filterBy(List<T> list, Predicate<T> rule) {   // takes a function
    return list.stream().filter(rule).toList();
}

static Predicate<String> longerThan(int n) {                      // returns a function
    return s -> s.length() > n;
}

filterBy(words, longerThan(5));
```

---

## 2. Composition

Small functions combined into larger ones are the main payoff.

### `Function`

```java
Function<String, String> trim  = String::trim;
Function<String, String> upper = String::toUpperCase;

Function<String, String> normalize = trim.andThen(upper);   // trim first, then upper
Function<String, String> same      = upper.compose(trim);   // compose = run argument first
// normalize and same behave identically here

normalize.apply("  hi ");   // "HI"
```

`f.andThen(g)` means `g(f(x))`. `f.compose(g)` means `f(g(x))`.

### `Predicate`

```java
Predicate<Employee> senior = e -> e.age() > 30;
Predicate<Employee> eng    = e -> e.dept().equals("ENG");

staff.stream().filter(senior.and(eng.negate())).toList();
staff.stream().filter(Predicate.not(String::isBlank));   // Java 11+: negating a method ref
```

### `Comparator`

```java
staff.sort(Comparator.comparing(Employee::dept)
                     .thenComparing(Employee::salary, Comparator.reverseOrder())
                     .thenComparing(Employee::name));
```

See [Comparable and Comparator](../08-collections/05_comparable-and-comparator.md).

### Building a pipeline from a list of steps

```java
List<UnaryOperator<String>> steps = List.of(String::trim, String::toLowerCase, s -> s.replace(' ', '-'));

Function<String, String> slugify = steps.stream()
        .map(s -> (Function<String, String>) s)
        .reduce(Function.identity(), Function::andThen);

slugify.apply("  Hello World ");   // "hello-world"
```

This is useful when steps are configured or assembled at runtime.

---

## 3. Replacing classic design patterns

Many object-oriented patterns collapse to a function when the "class" has a single method.

| Pattern | Functional form |
|---|---|
| Strategy | Pass a `Function` / `Predicate` / `Comparator` |
| Command | `Runnable` / `Supplier` stored in a list or map |
| Template Method | Pass the variable step as a lambda |
| Decorator | Function that wraps and returns a function |
| Factory | `Supplier<T>` / `Map<String, Supplier<T>>` |
| Observer | `List<Consumer<Event>>` |

See [Strategy](../24-design-patterns/03-behavioral/00_strategy.md), [Command](../24-design-patterns/03-behavioral/02_command.md), and [Template Method](../24-design-patterns/03-behavioral/03_template-method.md).

### Strategy via a lookup table

```java
Map<String, BinaryOperator<Double>> ops = Map.of(
    "+", Double::sum,
    "-", (a, b) -> a - b,
    "*", (a, b) -> a * b
);

double result = ops.getOrDefault(op, (a, b) -> { throw new IllegalArgumentException(op); })
                   .apply(x, y);
```

No `if`/`switch` chain and no class per operation. Adding an operation is one line.

### Decorator: wrap behavior around a function

```java
static <T, R> Function<T, R> timed(String label, Function<T, R> f) {
    return t -> {
        long start = System.nanoTime();
        try { return f.apply(t); }
        finally { System.out.println(label + ": " + (System.nanoTime() - start) / 1_000 + " µs"); }
    };
}

Function<String, User> load = timed("loadUser", repo::findByEmail);
```

### Execute-around (resource handling and cross-cutting concerns)

Hand the caller a resource, and own setup and cleanup yourself:

```java
static <R> R inTransaction(DataSource ds, Function<Connection, R> work) throws SQLException {
    try (Connection c = ds.getConnection()) {
        c.setAutoCommit(false);
        try {
            R result = work.apply(c);
            c.commit();
            return result;
        } catch (RuntimeException e) {
            c.rollback();
            throw e;
        }
    }
}
```

Callers can no longer forget the commit, rollback, or close. This is the idea behind `JdbcTemplate`-style APIs.

---

## 4. Lazy evaluation with `Supplier`

Pass a `Supplier` when computing the value is expensive and might not be needed.

```java
// Eager: expensive() always runs
value.orElse(expensive());

// Lazy: runs only if empty
value.orElseGet(() -> expensive());

// Same idea in logging APIs: the message is built only if the level is enabled
logger.log(Level.FINE, () -> "state=" + dumpState());
```

Streams themselves are lazy: nothing in the intermediate operations runs until a terminal operation is called.

---

## 5. Memoization

Cache the result of a pure function keyed by its argument:

```java
static <T, R> Function<T, R> memoize(Function<T, R> f) {
    Map<T, R> cache = new ConcurrentHashMap<>();
    return t -> cache.computeIfAbsent(t, f);
}

Function<Integer, BigInteger> slowSquare = n -> BigInteger.valueOf(n).pow(2);
Function<Integer, BigInteger> fast = memoize(slowSquare);
```

Caveats:

- Only valid for **pure** functions. If the result can change, the cache serves stale data.
- `ConcurrentHashMap` does not allow `null` values, so `f` must not return `null`.
- **Don't call `computeIfAbsent` recursively on the same map** (a memoized Fibonacci that calls itself inside the mapping function). `HashMap` throws `ConcurrentModificationException` for this, and `ConcurrentHashMap` may throw `IllegalStateException` ("Recursive update") or block. Use an explicit `get` / `put` instead.
- The cache grows without bound. For production caching, see [Caching](../25-real-world-patterns/00_caching.md).

---

## 6. Currying and partial application

Java has no built-in currying, but nested `Function`s model it:

```java
Function<Integer, Function<Integer, Integer>> add = a -> b -> a + b;

Function<Integer, Integer> plus10 = add.apply(10);   // partial application
plus10.apply(5);                                     // 15
```

It's handy for configuring a function once and reusing it, but beyond that, the type signatures get noisy. In Java, a small named method or a lambda that captures a variable is usually clearer:

```java
static Function<Integer, Integer> plus(int n) { return x -> x + n; }
```

---

## 7. Errors in functional code

### Checked exceptions in lambdas

`Function`, `Predicate`, `Consumer`, and friends can't throw checked exceptions, so this doesn't compile:

```java
paths.stream().map(p -> Files.readString(p));   // IOException is not allowed here
```

Option 1: wrap at the lambda site.

```java
paths.stream().map(p -> {
    try { return Files.readString(p); }
    catch (IOException e) { throw new UncheckedIOException(e); }
});
```

Option 2: define a throwing functional interface once and adapt it.

```java
@FunctionalInterface
interface ThrowingFunction<T, R> { R apply(T t) throws Exception; }

static <T, R> Function<T, R> unchecked(ThrowingFunction<T, R> f) {
    return t -> {
        try { return f.apply(t); }
        catch (RuntimeException e) { throw e; }
        catch (Exception e) { throw new RuntimeException(e); }
    };
}

paths.stream().map(unchecked(Files::readString));
```

If the failing step needs real handling (retry, skip, report), don't hide it behind a generic wrapper.

### Errors as values: a `Result` type (Java 21+)

When one bad element shouldn't kill a whole stream, represent success/failure as data using sealed types, records, and pattern matching ([Sealed Classes](../12-modern-java/02_sealed-classes.md), [Records](../12-modern-java/01_records.md), [Pattern Matching](../12-modern-java/04_pattern-matching.md)):

```java
sealed interface Result<T> {
    record Ok<T>(T value) implements Result<T> {}
    record Err<T>(Exception error) implements Result<T> {}

    static <T> Result<T> attempt(Callable<T> c) {
        try { return new Ok<>(c.call()); }
        catch (Exception e) { return new Err<>(e); }
    }
}

List<Result<Integer>> parsed = inputs.stream()
        .map(s -> Result.attempt(() -> Integer.parseInt(s)))
        .toList();

for (Result<Integer> r : parsed) {
    switch (r) {
        case Result.Ok<Integer> ok   -> use(ok.value());
        case Result.Err<Integer> err -> log.warn("bad input", err.error());
    }
}
```

The `switch` is exhaustive because `Result` is sealed: the compiler tells you if a case is missing. This is a pattern, not a standard-library type. See [Exception Handling Patterns](../06-exceptions-and-debugging/05_exception-handling-patterns.md) for when exceptions remain the right tool.

### `Optional` as a pipeline, not a null check

```java
String city = findUser(id)
        .map(User::address)          // Optional<Address>
        .map(Address::city)
        .filter(c -> !c.isBlank())
        .orElse("Unknown");

findUser(id).ifPresentOrElse(this::greet, this::askToRegister);   // Java 9+

Optional<User> u = findInCache(id).or(() -> findInDb(id));        // Java 9+: lazy fallback
```

Avoid `isPresent()` followed by `get()`. That is just a null check with extra steps. Prefer `map`/`orElse*`/`ifPresent*`/`orElseThrow`.

---

## 8. Stream usage patterns

```java
// Index a list for lookups
Map<Long, User> byId = users.stream().collect(Collectors.toMap(User::id, Function.identity()));

// Flatten nested data
List<String> allTags = posts.stream().flatMap(p -> p.tags().stream()).distinct().toList();

// Stream an Optional (Java 9+): drop empties from a stream of Optionals
List<User> found = ids.stream().map(this::findUser).flatMap(Optional::stream).toList();

// Pre-validated, simple loop → no stream needed
for (Order o : orders) {
    if (o.isPaid()) ship(o);        // side effect per element: a loop is clearer than forEach
}
```

Guidelines:

- **Streams are for transforming and aggregating data.** Use loops for side-effect-heavy logic and when you need early `break`/`continue`, index access, or checked exceptions.
- Keep each lambda short. If it's more than ~3 lines, extract a named method and use a method reference.
- Don't mutate external state from inside `map`/`filter`. `peek` is for debugging, not logic.

---

## 9. When *not* to be functional

| Situation | Prefer |
|---|---|
| Lambda body is long, branching, or nested | A named method |
| Deeply chained `Optional`/stream expressions that need a debugger | Intermediate variables or a loop |
| Hot inner loops on primitives | Plain loops, or primitive streams after measuring |
| Heavy mutation of local state | A loop |
| Team or codebase isn't comfortable reading the style | Clarity over cleverness |

Stack traces from lambdas are longer and less readable (synthetic names like `lambda$process$0`), and complex pipelines are harder to step through. Readability is the actual goal, so stop abstracting when it stops helping.

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Lambdas that mutate captured state | Use `collect`/`reduce`, or a plain loop |
| Capturing a non-effectively-final local variable | Use a one-element array/atomic only as a last resort, and prefer restructuring |
| `Optional` as a field or method parameter | Use it for return values; see [Optional](03_optional.md) |
| `isPresent()` + `get()` | `map`/`orElseThrow`/`ifPresentOrElse` |
| `orElse(expensiveCall())` | `orElseGet(() -> expensiveCall())` |
| Memoizing impure functions | Only memoize pure ones |
| Swallowing exceptions in a wrapper to keep the stream going | Log or model failures explicitly, as with `Result` |
| Re-creating `Comparator`/`Predicate` objects in tight loops unnecessarily | Hold them in constants |

---

## Quick Summary

- Prefer **pure functions and immutable data**; push side effects to the edges.
- **Compose** with `andThen`, `compose`, `Predicate.and/or/negate/not`, and `Comparator.comparing().thenComparing()`.
- Many OO patterns (Strategy, Command, Template Method, Decorator, Factory, Observer) become a lambda plus a `Map`/`List` of functions.
- Use `Supplier` for **laziness** (`orElseGet`, lazy logging) and execute-around for resource and transaction handling.
- Memoize only **pure** functions, and never recursively inside `computeIfAbsent`.
- Handle checked exceptions by wrapping, or model failures as data (`Result` with sealed types, Java 21+).
- Use `Optional` as a pipeline. Don't fall back to `isPresent()` + `get()`.
- Stop being functional when it hurts readability or debugging.

**Next:** [Date and Time](../10-date-and-time/00_java-time-overview.md)
