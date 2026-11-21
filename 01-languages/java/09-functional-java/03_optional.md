# Optional Deep Dive and Monadic Design

> Value-based container semantics, avoiding anti-patterns, monadic composition pipelines, and primitive Optional specializations.

| Metadata | Value |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | [00_lambda-expressions.md](00_lambda-expressions.md), [01_functional-interfaces.md](01_functional-interfaces.md) |
| **Time to Read** | 24 minutes |
| **Status** | Active |
| **Java Version** | Java 8+ (or, ifPresentOrElse, stream Java 9+) |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance & Security Notes](#8-performance--security-notes)
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

Tony Hoare, inventor of the null pointer in 1965, famously referred to it as his **"billion-dollar mistake"**. In Java, returning `null` to signify the absence of a value has historically been a rampant source of bugs:
- Methods returning `null` force callers to remember to write null-checks.
- Forgetting a null-check results in the notorious `NullPointerException` (NPE) at runtime.
- `null` is silent and untyped: a method signature `User findUser(String id)` provides no indication of whether the return value could be missing.

Introduced in Java 8, `java.util.Optional<T>` is a single-element container that either contains a non-null value (present) or contains nothing (empty).

`Optional` was **expressly designed as a method return type** to communicate possible absence clearly in method signatures and force callers to explicitly handle presence or absence through monadic operations (`map`, `flatMap`, `filter`, `orElseThrow`).

---

## 2. Core Concepts

### Value-Based Class Semantics
`Optional` is a **Value-Based Class**:
- It has no public constructors; instances are created via factory methods: `Optional.of(val)`, `Optional.ofNullable(val)`, `Optional.empty()`.
- It does not support identity-sensitive operations: synchronizing on an `Optional`, using `==` identity comparison, or relying on its identity hash code is strictly discouraged and may produce compiler warnings.

### Factory Methods
- `Optional.empty()`: Returns the shared singleton empty `Optional` instance.
- `Optional.of(value)`: Returns an `Optional` holding `value`. Throws `NullPointerException` immediately if `value == null`!
- `Optional.ofNullable(value)`: Returns an `Optional` holding `value` if non-null, or `Optional.empty()` if null.

### The Monadic Pipeline
Rather than extracting values using `get()` and writing procedural `if (opt.isPresent())` checks, `Optional` exposes fluent monadic functional transformations:
- `map(Function<? super T, ? extends U> mapper)`
- `flatMap(Function<? super T, ? extends Optional<? extends U>> mapper)`
- `filter(Predicate<? super T> predicate)`
- `or(Supplier<? extends Optional<? extends T>> supplier)` (Java 9+)
- `ifPresentOrElse(Consumer<? super T> action, Runnable emptyAction)` (Java 9+)
- `stream()`: Converts `Optional` to a 0- or 1-element `Stream` (Java 9+)

---

## 3. How It Works Internally

### Memory Layout
Inside OpenJDK, `Optional<T>` is a standard object holding a single reference field:
```java
public final class Optional<T> {
    private static final Optional<?> EMPTY = new Optional<>(null);
    private final T value;

    private Optional(T value) {
        this.value = value;
    }
}
```
Because `EMPTY` is a static singleton, `Optional.empty()` never allocates heap memory. However, `Optional.of(value)` allocates an `Optional` wrapper object (16 bytes on a 64-bit JVM with compressed OOPs).

### Primitive Specializations
To prevent auto-boxing primitives into wrapper objects (`Optional<Integer>`), Java provides:
- `OptionalInt`: holds `int` directly with `boolean isPresent`.
- `OptionalLong`: holds `long` directly.
- `OptionalDouble`: holds `double` directly.

```
Optional<Integer>:
Optional Object (16B) ---> Boxed Integer Object (16B) = 32 bytes overhead!

OptionalInt:
OptionalInt Object (16B with primitive int field)     = 16 bytes (50% savings!)
```

---

## 4. Syntax & Usage

### 1. Constructing Optionals
```java
import java.util.Optional;

public class CreationDemo {
    public static void main(String[] args) {
        // 1. Explicitly empty
        Optional<String> empty = Optional.empty();

        // 2. Non-null (throws NPE if null passed)
        Optional<String> nonNull = Optional.of("Hello");

        // 3. Nullable (safe conversion)
        String nullableVal = getNullableString();
        Optional<String> safe = Optional.ofNullable(nullableVal);
    }

    static String getNullableString() { return null; }
}
```

### 2. Monadic Data Transformation
```java
import java.util.Optional;

public class MonadicPipelineDemo {
    record Address(String city, String zipCode) {}
    record User(String name, Address address) {}

    public static void main(String[] args) {
        User user = new User("Alice", new Address("Seattle", "98101"));

        // Extract zip code safely without nested null checks!
        String zip = Optional.ofNullable(user)
            .map(User::address)
            .map(Address::zipCode)
            .filter(z -> z.startsWith("98"))
            .orElse("UNKNOWN");

        System.out.println("Zip: " + zip); // 98101
    }
}
```

---

## 5. Practical Examples

### Example 1: Database Repository Return (Basic to Intermediate)
```java
import java.util.HashMap;
import java.util.Map;
import java.util.Optional;

public class UserRepository {
    private final Map<Long, String> users = new HashMap<>();

    public UserRepository() {
        users.put(1L, "Alice");
        users.put(2L, "Bob");
    }

    // Communicates to callers that user may not exist!
    public Optional<String> findById(Long id) {
        return Optional.ofNullable(users.get(id));
    }

    public static void main(String[] args) {
        UserRepository repo = new UserRepository();

        // Safe consumption with orElseThrow
        String username = repo.findById(1L)
            .orElseThrow(() -> new IllegalArgumentException("User not found"));
        System.out.println("User: " + username);

        // Safe action with ifPresentOrElse
        repo.findById(99L).ifPresentOrElse(
            u -> System.out.println("Found: " + u),
            () -> System.out.println("User 99 does not exist")
        );
    }
}
```

### Example 2: FlatMapping Nested Optionals (Real-World)
```java
import java.util.Optional;

public class FlatMapDemo {
    record Order(String id, Optional<String> trackingNumber) {}

    public static void main(String[] args) {
        Order order = new Order("ORD-101", Optional.of("TRK-9999"));

        // If map() were used, result would be Optional<Optional<String>>!
        // flatMap flattens the nested Optional!
        Optional<String> tracking = Optional.of(order)
            .flatMap(Order::trackingNumber);

        System.out.println("Tracking: " + tracking.orElse("Pending"));
    }
}
```

### Example 3: Edge Case - `orElse` vs `orElseGet` Performance Trap
```java
import java.util.Optional;

public class OrElsePerformanceTrap {
    public static void main(String[] args) {
        Optional<String> cached = Optional.of("CachedData");

        // DANGEROUS: orElse() ALWAYS executes the fallback method, even if value is present!
        System.out.println("Testing orElse:");
        String res1 = cached.orElse(expensiveNetworkCall());

        // SAFE: orElseGet() executes lazily ONLY if the value is empty!
        System.out.println("Testing orElseGet:");
        String res2 = cached.orElseGet(() -> expensiveNetworkCall());
    }

    static String expensiveNetworkCall() {
        System.out.println("--> Executing EXPENSIVE fallback call!");
        return "FallbackData";
    }
}
```
Output:
```
Testing orElse:
--> Executing EXPENSIVE fallback call!
Testing orElseGet:
(Notice: expensiveNetworkCall is NOT executed!)
```

### Example 4: Failure Case - Using `Optional.get()` Without Checking
```java
import java.util.Optional;

public class GetWithoutCheckFailure {
    public static void main(String[] args) {
        Optional<String> empty = Optional.empty();

        try {
            // SEVERE ANTI-PATTERN: Calling get() directly reproduces NPE as NoSuchElementException!
            // In Java 10+, opt.orElseThrow() is preferred over opt.get() to make the exception explicit.
            String val = empty.get();
        } catch (Exception e) {
            System.out.println("Caught: " + e.getClass().getSimpleName()); // NoSuchElementException
        }
    }
}
```

---

## 6. Best Practices

1. **Use `Optional` ONLY as a Method Return Type**: Do not use `Optional` as class fields, method parameters, or collection elements.
2. **Never Call `Optional.get()` Directly**: Use `orElse()`, `orElseGet()`, or `orElseThrow()`. If calling without default in Java 10+, use `orElseThrow()`.
3. **Always Prefer `orElseGet()` Over `orElse()` When Fallback is Expensive**: `orElse(val)` evaluates eagerly; `orElseGet(() -> val)` evaluates lazily.
4. **Never Return `null` from a Method Returning `Optional`**: Always return `Optional.empty()`. Returning `null` defeats the entire purpose of `Optional`.
5. **Never Wrap Collections in `Optional`**: Return an empty collection (`List.of()`, `Set.of()`) rather than `Optional<List<T>>`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why Wrong | Fix |
|---|---|---|
| Returning `null` from method returning `Optional` | Causes `NullPointerException` on caller's monadic chain. | Always return `Optional.empty()`. |
| Calling `opt.get()` unconditionally | Throws `NoSuchElementException` if empty. | Use `orElseGet()` or `orElseThrow()`. |
| Writing `if (opt.isPresent()) opt.get()` | Replaces `if (x != null)` with verbose wrapper checks. | Use monadic `opt.ifPresent()`, `map()`, etc. |
| Using `orElse(expensiveCompute())` | Computes fallback eagerly every single time even when value is present. | Use `orElseGet(() -> expensiveCompute())`. |
| Using `Optional<T>` as a class field | Not serializable; incurs 16-24 bytes wrapper overhead per field. | Use null fields with `Optional` getters. |
| Returning `Optional<List<T>>` | Creates two layers of emptiness checks. | Return empty `List<T>`. |

---

## 8. Performance & Security Notes

### Performance
- **Object Allocation Overhead**: `Optional` creates a short-lived heap allocation. While HotSpot Escape Analysis frequently scalar-replaces `Optional` instances within local method scopes, returning them across non-inlined methods retains the allocation overhead.
- **Project Valhalla (Future Java)**: When Project Valhalla value classes arrive, `Optional` will become a primitive value class with zero heap pointer overhead.

### Security
- **Defensive API Contracts**: `Optional` forces callers to acknowledge that a resource (user, session, security token) may not exist, eliminating silent null propagation security flaws.

---

## 9. Comparison / When to Use What

| Scenario | Anti-Pattern | Recommended Solution |
|---|---|---|
| Method might return missing value | Return `null` | Return `Optional<T>` |
| Method might return empty collection | Return `Optional<List<T>>` | Return empty `List<T>` |
| Domain model entity field | `private Optional<String> bio;` | `private String bio;` (getter returns `Optional`) |
| Method parameter | `void process(Optional<String> opt)` | Overloaded methods or nullable parameter |
| Primitive return | `Optional<Integer>` | `OptionalInt` |

---

## 10. Interview Questions

<details>
<summary>1. What was the explicit design intent of `Optional` in Java?</summary>
`Optional` was designed exclusively as a method return type to represent the potential absence of a value, forcing API consumers to handle absence explicitly and eliminating silent `NullPointerException` bugs.
</details>

<details>
<summary>2. Why should `Optional` not be used as a field in a class?</summary>
`Optional` does not implement `java.io.Serializable`. Using it as a field breaks standard Java serialization. Furthermore, it adds 16-24 bytes of heap overhead per field. The standard idiom is to store a nullable field and return `Optional.ofNullable(field)` in the getter.
</details>

<details>
<summary>3. What is the difference between `orElse()` and `orElseGet()`?</summary>
`orElse(T other)` accepts an already-evaluated object; the expression is executed eagerly even if the `Optional` contains a value. `orElseGet(Supplier<T> supplier)` accepts a lazy supplier function executed only if the `Optional` is empty.
</details>

<details>
<summary>4. Why was `Optional.get()` deprecated in favor of `orElseThrow()` in Java 10?</summary>
Because calling `.get()` conveys the illusion that retrieving the value is always safe, leading to unexpected `NoSuchElementException` crashes. `orElseThrow()` explicitly communicates that the call throws an exception if empty.
</details>

<details>
<summary>5. How does `Optional.flatMap()` differ from `Optional.map()`?</summary>
`map()` wraps the return value of the mapper function in a new `Optional`. If the mapper function already returns an `Optional`, `map()` produces `Optional<Optional<T>>`. `flatMap()` flattens the result into a single `Optional<T>`.
</details>

<details>
<summary>6. Why should you never return `Optional<List<T>>`?</summary>
A collection already has a natural, standard representation of emptiness: an empty collection (`Collections.emptyList()`, `List.of()`). Returning `Optional<List<T>>` forces callers to check both whether the optional is present and whether the list is empty.
</details>

<details>
<summary>7. What does `Optional.stream()` do (Java 9+)?</summary>
It transforms the `Optional` into a `Stream` containing either zero elements (if empty) or one element (if present). This enables seamless stream pipeline filtering: `stream.map(this::find).flatMap(Optional::stream)`.
</details>

<details>
<summary>8. What happens if you pass `null` to `Optional.of()`?</summary>
It immediately throws `NullPointerException`. Use `Optional.ofNullable()` if the argument might be `null`.
</details>

---

## 11. Practice Exercises

### Easy
1. Create an `Optional` containing a string and print its value in uppercase using `ifPresent()`.
2. Wrap a potentially null string in an `Optional` and provide a default value "GUEST" using `orElse()`.
3. Check if an `Optional` contains a value greater than 50 using `filter()`.

### Medium
4. Given a nested object hierarchy `Company -> Department -> Manager -> Email`, write an `Optional` pipeline to extract the manager's email safely.
5. Convert a `List<Optional<String>>` into a `List<String>` containing only the present values using `Optional.stream()`.
6. Implement a configuration lookup method `getConfig(String key)` that checks memory cache, falls back to environment variable, then to a default value using `Optional.or()`.

### Hard
7. Implement a custom monadic `Result<T, E>` class that models either a Success(T) or a Failure(E) with complete monadic map/flatMap operations.
8. Design a JSON path query evaluator that traverses untyped JSON tree nodes using monadic `Optional` flatMap chains.
9. Measure and compare heap allocation differences between 100,000 method returns using `Optional<T>` vs nullable references under JMH.

<details>
<summary>Solutions</summary>

```java
// Easy 1: ifPresent
Optional.of("antigravity").ifPresent(s -> System.out.println(s.toUpperCase()));

// Medium 5: Flatten List of Optionals
public static List<String> extractPresent(List<Optional<String>> list) {
    return list.stream()
        .flatMap(Optional::stream)
        .toList();
}

// Medium 6: Chained fallbacks with Optional.or
public static Optional<String> resolveConfig(String key, Map<String, String> cache) {
    return Optional.ofNullable(cache.get(key))
        .or(() -> Optional.ofNullable(System.getenv(key)))
        .or(() -> Optional.of("DEFAULT_VALUE"));
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Creation**: `Optional.of(val)` (non-null), `Optional.ofNullable(val)`, `Optional.empty()`.
- **Transformation**: `map()`, `flatMap()`, `filter()`.
- **Extraction**: `orElse(val)` (eager), `orElseGet(supplier)` (lazy), `orElseThrow()`.
- **Conditionals**: `ifPresent(consumer)`, `ifPresentOrElse(consumer, runnable)`.
- **Stream**: `opt.stream()` yields 0 or 1 element.
- **Cardinal Rule**: Use as return type only; never as fields, parameters, or collections.

---

## 13. Further Reading
- Bloch, Joshua. *Effective Java (3rd Edition)*: Item 55 (Return optionals judiciously).
- Oracle Java Documentation: `java.util.Optional` API specification.
- Goetz, Brian. *Design Considerations for Java Optional*.
