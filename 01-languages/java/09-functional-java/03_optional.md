# Optional

`Optional<T>` is a container that holds **either one value or nothing**. It exists to make "this method might not return a result" **visible in the type**, instead of returning `null` and hoping every caller remembers to check. Tony Hoare called the null reference his "billion-dollar mistake"; `Optional` is Java's partial answer for return values.

```java
Optional<User> findUser(long id) { ... }                // the signature says: there may be no user

String name = findUser(42)
        .map(User::name)
        .filter(n -> !n.isBlank())
        .orElse("anonymous");                           // no null checks anywhere
```

## The problem

```java
User user = repository.findById(id);
String city = user.getAddress().getCity();              // NullPointerException if user or address is null

// Defensive code: correct but noisy
String city = "unknown";
if (user != null && user.getAddress() != null && user.getAddress().getCity() != null) {
    city = user.getAddress().getCity();
}
```

With `Optional` return types the compiler **forces** you to acknowledge absence, and you can chain transformations safely.

## Creating

```java
Optional<String> a = Optional.of("x");                  // value must be non-null: Optional.of(null) → NullPointerException
Optional<String> b = Optional.ofNullable(maybeNull);    // empty if the argument is null
Optional<String> c = Optional.empty();                  // no value
```

| Factory | Use when |
|---------|----------|
| `of(value)` | The value is guaranteed non-null (a null would be a bug) |
| `ofNullable(value)` | Wrapping something that might be null (map lookup, legacy API) |
| `empty()` | Returning "nothing found" |

## Getting the value out

| Method | Behavior | When to use |
|--------|----------|-------------|
| `orElse(default)` | Value, or the default | A **cheap, constant** default |
| `orElseGet(supplier)` | Value, or the supplier's result | A default that is **expensive** or has side effects |
| `orElseThrow()` | Value, or `NoSuchElementException` (Java 10+) | Absence is a bug |
| `orElseThrow(supplier)` | Value, or the supplied exception | Absence is a **domain error** (`OrderNotFoundException`) |
| `get()` | Value, or `NoSuchElementException` | **Avoid**: it hides the check; prefer `orElseThrow()` for the same behavior with a clear name |
| `isPresent()` / `isEmpty()` (Java 11) | `boolean` | Rarely needed; usually a smell (see below) |
| `ifPresent(consumer)` | Runs the action if present | Side effect on a present value |
| `ifPresentOrElse(consumer, runnable)` (Java 9) | Action or fallback | Both branches are side effects |

```java
User user = findUser(id).orElseThrow(() -> new UserNotFoundException(id));
findUser(id).ifPresent(u -> log.info("found {}", u));
findUser(id).ifPresentOrElse(u -> greet(u), () -> log.warn("no user {}", id));
```

### `orElse` vs `orElseGet`: eager vs lazy

```java
String v1 = opt.orElse(expensiveDefault());             // expensiveDefault() runs EVERY time, even if opt has a value
String v2 = opt.orElseGet(() -> expensiveDefault());    // runs only when opt is empty
String v3 = opt.orElse("n/a");                          // fine: a constant costs nothing
```

`orElse` evaluates its argument **before** the call, so it is wrong for expensive or side-effecting defaults (database calls, object creation, logging).

## Transforming: `map`, `flatMap`, `filter`, `or`

```java
Optional<String> name = findUser(id).map(User::name);               // Optional<String>: empty if user is empty
Optional<Integer> len = name.map(String::length);

Optional<String> adult = findUser(id).filter(u -> u.age() >= 18).map(User::name);

// flatMap: when the mapper itself returns an Optional (avoids Optional<Optional<T>>)
Optional<Address> address = findUser(id).flatMap(User::address);    // User.address() returns Optional<Address>

// or (Java 9): fall back to another Optional
Optional<User> u = cache.find(id).or(() -> database.find(id));

// stream (Java 9): 0 or 1 element, perfect for flatMap in streams
List<String> names = ids.stream().map(this::findUser).flatMap(Optional::stream).map(User::name).toList();
```

| Method | Signature (simplified) | Behavior |
|--------|------------------------|----------|
| `map` | `Function<T, R>` | If present, apply; result wrapped (a `null` result becomes empty) |
| `flatMap` | `Function<T, Optional<R>>` | If present, apply and **flatten** |
| `filter` | `Predicate<T>` | Keep the value only if it matches, else empty |
| `or` | `Supplier<Optional<T>>` | This, or the alternative when empty |

### A chain replacing nested null checks

```java
// Before
String zip = null;
if (order != null) {
    Customer c = order.getCustomer();
    if (c != null) {
        Address a = c.getAddress();
        if (a != null) zip = a.getZip();
    }
}

// After (getters return Optional<...>)
String zip = order.customer()
                  .flatMap(Customer::address)
                  .map(Address::zip)
                  .orElse("unknown");
```

## Primitive optionals

`OptionalInt`, `OptionalLong`, `OptionalDouble` avoid boxing and are returned by stream terminal operations such as `IntStream.max()`, `average()`:

```java
OptionalDouble avg = IntStream.of(1, 2, 3).average();
double a = avg.orElse(0.0);
OptionalInt max = IntStream.empty().max();               // empty
max.getAsInt();                                          // NoSuchElementException
```

They have fewer methods (no `map`/`filter`).

## When to use `Optional`

**Design intent** (from the JDK authors): `Optional` is for **method return types** where "no result" is a normal, expected outcome and `null` would be error-prone.

| Good use | Example |
|----------|---------|
| Return type of a lookup that may find nothing | `Optional<User> findByEmail(String e)` |
| Result of a stream terminal operation that may be empty | `stream.findFirst()`, `max()`, `reduce(op)` |
| Chaining safe transformations | `map`, `flatMap`, `filter` |

## When NOT to use `Optional`

| Don't | Why | Instead |
|-------|-----|---------|
| **Fields** (`private Optional<String> nickname;`) | Not `Serializable`; overhead; wasteful in every instance; breaks JavaBeans/ORM conventions | A nullable field plus a getter returning `Optional<String>` |
| **Method parameters** (`void send(Optional<String> cc)`) | Callers must wrap; three states (`null`, empty, present) | Overloads, `@Nullable`, or a default value |
| **Collection elements** (`List<Optional<T>>`) | Clutter | Filter out missing values; use an empty collection |
| **Return type collections** (`Optional<List<T>>`) | Two ways to say "nothing" | Return an **empty** collection |
| **Replace every null** | Not a general null replacement | Design to avoid `null` where possible |
| **Wrapping primitives** (`Optional<Integer>` in hot code) | Boxing | `OptionalInt` |
| **As the type of a stream element for performance-critical paths** | Allocation | Filter earlier |
| `Optional.of(...)` when the value may be null | `NullPointerException` | `ofNullable` |

```java
// Wrong ways to use it: they reintroduce null-style checks
if (opt.isPresent()) { use(opt.get()); }                  // same as: if (x != null) use(x)
opt.get();                                                // unguarded: could throw

// Right: express the intent
opt.ifPresent(this::use);
opt.map(this::transform).orElse(fallback);
```

An `Optional` variable itself must **never be `null`**. That defeats the whole idea.

## `Optional` is a value-based class

- Do not synchronize on it, or compare with `==` (use `equals`)
- `equals`/`hashCode`/`toString` are based on the contained value (`Optional[x]`, `Optional.empty`)
- It is **not `Serializable`**, so avoid it in fields of serializable classes and in DTOs/JSON models (Jackson supports it via a module, but a plain nullable field is simpler)
- Overhead: one small object per call; usually optimized away; irrelevant outside very hot paths

## Optional vs null vs exceptions vs empty collections

| Situation | Best choice |
|-----------|-------------|
| Lookup that may legitimately find nothing | `Optional<T>` |
| Missing value is a **bug** or violated precondition | Exception (`orElseThrow`, `requireNonNull`) |
| A "list of things" that may be empty | An **empty collection**, never `null`/`Optional` |
| Optional parameter | Overload or a default |
| Rarely-set field | Nullable field + `Optional` getter |
| Performance-critical numeric code | Primitive value + sentinel, or `OptionalInt` |

See [06-exceptions-and-debugging/02_throw-and-throws.md](../06-exceptions-and-debugging/02_throw-and-throws.md) and [05_exception-handling-patterns.md](../06-exceptions-and-debugging/05_exception-handling-patterns.md).

## Working with legacy `null`s

```java
Optional<String> v = Optional.ofNullable(map.get(key));                  // wrap a possibly-null return
String s = Optional.ofNullable(legacyApi()).map(String::trim).orElse("");
map.getOrDefault(key, fallback);                                         // simpler than Optional for map lookups
Objects.requireNonNullElse(x, defaultValue);                             // null-safe default without Optional
```

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| `opt.get()` without checking | `NoSuchElementException` | `orElseThrow()`, `orElse`, `ifPresent` |
| `if (opt.isPresent()) opt.get()...` | Verbose; defeats the purpose | `map`/`ifPresent`/`orElse` |
| `orElse(expensiveCall())` | Always executes the call | `orElseGet(() -> expensiveCall())` |
| `Optional.of(maybeNull)` | `NullPointerException` | `ofNullable` |
| Returning `null` from a method declared to return `Optional` | `NullPointerException` at the caller | Return `Optional.empty()` |
| `Optional` as a field or parameter | Serialization issues, awkward APIs | Nullable field, overloads |
| `Optional<List<T>>` | Two kinds of "empty" | An empty list |
| `map` with a function that returns `Optional` | `Optional<Optional<T>>` | `flatMap` |
| Using `Optional` for everything | Over-engineering | Use for return values with real absence |
| Comparing optionals with `==` | Wrong (identity) | `equals` |
| Wrapping and unwrapping in the same method | Pointless | Use plain `null` checks internally |

## Key takeaways

- `Optional<T>` makes "maybe no result" part of a method's signature; use it as a **return type**
- Extract values with `orElse`/`orElseGet`/`orElseThrow`; avoid bare `get()` and `isPresent()`+`get()`
- Transform with `map`, `flatMap`, `filter`, `or`, `stream`; chains replace nested null checks
- `orElse` is eager, `orElseGet` is lazy
- Not for fields, parameters, collection elements or `Optional<List<T>>`; never assign `null` to an `Optional`

**Next:** [Streams Fundamentals](./04_streams-fundamentals.md)