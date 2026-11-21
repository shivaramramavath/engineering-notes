# Interview Mastery: Functional Programming, Streams & Modern Java

> Comprehensive interview guide for Functional Java: Lambda bytecode translation via `LambdaMetafactory`, Stream pipeline internals, custom Collectors, `Optional` patterns, and modern Java 17–25 idioms.

| Metadata | Value |
|---|---|
| **Level** | Advanced |
| **Prerequisites** | [00_core-java.md](00_core-java.md), [09-functional-java](../09-functional-java/README.md) |
| **Time to Read** | 35 minutes |
| **Status** | Complete |
| **Java Version** | Java 8 to Java 25 LTS |

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

The introduction of Lambda Expressions and the Stream API in Java 8 marked the most profound paradigm shift in Java's history, moving the language from purely imperative mutations to **declarative, functional data transformations**.

In senior technical interviews, functional questions go far beyond writing simple `.filter().map()` expressions:
- Why Lambdas are **not syntactic sugar for anonymous inner classes**.
- How the JVM utilizes **`invokedynamic` and `LambdaMetafactory`** to construct lambda call sites at runtime.
- The architectural distinction between **Stateless** (`filter`, `map`) and **Stateful** (`sorted`, `distinct`) stream operations.
- The mechanics, thread-safety hazards, and pool starvation risks of **Parallel Streams**.
- Advanced reduction pipelines using custom and multi-tier **`Collector`** composition.

---

## 2. Core Concepts

### 1. Functional Interfaces & Method References
A Functional Interface is an interface with **exactly one abstract method (SAM)**, annotated with `@FunctionalInterface`.
- **`Function<T, R>`**: `R apply(T t)` — Transforms input $T$ to output $R$.
- **`Predicate<T>`**: `boolean test(T t)` — Evaluates boolean condition.
- **`Consumer<T>`**: `void accept(T t)` — Performs side-effect on $T$.
- **`Supplier<T>`**: `T get()` — Produces an instance of $T$ lazily.
- **Method Reference Types**:
  1. Static: `Math::max`
  2. Bound Instance: `System.out::println`
  3. Unbound Instance: `String::toLowerCase`
  4. Constructor: `ArrayList::new`

### 2. Stream Architecture: Lazy Evaluation & Pipelines
A Stream is a sequence of elements supporting sequential and parallel aggregate operations. Streams do **not store data**.
A stream pipeline consists of:
1. **Source**: Collection, Array, I/O channel, or Generator.
2. **Intermediate Operations (Lazy)**: Transforms a stream into another stream. Execution is deferred until a terminal operation is called.
   - *Stateless*: `filter()`, `map()`, `flatMap()`.
   - *Stateful*: `sorted()`, `distinct()`, `limit()`.
3. **Terminal Operation (Eager)**: Produces a non-stream result (e.g. `collect()`, `reduce()`, `count()`) or side-effect (`forEach()`). Calling a terminal operation **consumes the stream permanently**.

---

## 3. How It Works Internally

### Lambda Compilation: `invokedynamic` vs Anonymous Inner Classes
Why didn't Java implement Lambdas as anonymous inner classes (`new Runnable() { ... }`)?
1. **Class-file Bloat**: Anonymous inner classes generate an independent `.class` file on disk (`OuterClass$1.class`) for every lambda, bloating JAR sizes and slowing class loading.
2. **Memory Footprint**: Each inner class instance creates a full heap object with object header and instance fields.

Instead, Java uses **`invokedynamic` (Indy)**:
```
Bytecode:
0: invokedynamic #2, 0 // InvokeDynamic #0:apply:()Ljava/util/function/Function;
```
- At first execution, the JVM invokes the bootstrap method `java.lang.invoke.LambdaMetafactory`.
- `LambdaMetafactory` generates an optimized, lightweight synthetic call site and returns a `CallSite` handle.
- If the lambda does not capture enclosing variables (non-capturing lambda), the JVM caches a single instance, making subsequent invocations **zero-allocation**!

---

## 4. Syntax & Usage

### Modern Stream Collectors & Reductions

```java
import java.util.*;
import java.util.stream.Collectors;

public class FunctionalSyntax {
    public record Transaction(String id, String category, double amount) {}

    public static void main(String[] args) {
        List<Transaction> txns = List.of(
            new Transaction("T1", "GROCERY", 45.50),
            new Transaction("T2", "TECH", 1200.00),
            new Transaction("T3", "GROCERY", 22.00)
        );

        // Grouping with downstream summing
        Map<String, Double> totalByCategory = txns.stream()
            .collect(Collectors.groupingBy(
                Transaction::category,
                Collectors.summingDouble(Transaction::amount)
            ));

        // Java 12+ Teeing Collector (two independent collectors combined in one pass)
        var stats = txns.stream()
            .collect(Collectors.teeing(
                Collectors.summingDouble(Transaction::amount),
                Collectors.counting(),
                (sum, count) -> "Total: $" + sum + ", Avg: $" + (sum / count)
            ));

        System.out.println(totalByCategory);
        System.out.println(stats);
    }
}
```

---

## 5. Practical Examples

### Example 1: Multi-Level Grouping & Partitioning

```java
import java.util.*;
import java.util.stream.Collectors;

public class AdvancedGrouping {
    public record Employee(String name, String department, int age, double salary) {}

    public static void main(String[] args) {
        List<Employee> employees = List.of(
            new Employee("Alice", "Engineering", 32, 120_000),
            new Employee("Bob", "Engineering", 28, 95_000),
            new Employee("Charlie", "Sales", 45, 110_000),
            new Employee("Diana", "Sales", 31, 105_000)
        );

        // Multi-level Map: Department -> (Above 30 -> List<Names>)
        Map<String, Map<Boolean, List<String>>> structured = employees.stream()
            .collect(Collectors.groupingBy(
                Employee::department,
                Collectors.partitioningBy(
                    e -> e.age() > 30,
                    Collectors.mapping(Employee::name, Collectors.toList())
                )
            ));

        System.out.println(structured);
    }
}
```

### Example 2: Primitive Streams vs Boxed Overhead

```java
import java.util.stream.IntStream;
import java.util.stream.Stream;

public class PrimitiveStreamBenchmark {
    public static void benchmark() {
        int n = 10_000_000;

        // SLOW: Stream<Integer> boxes every primitive into an Integer object
        long start1 = System.nanoTime();
        long sum1 = Stream.iterate(1, i -> i + 1)
            .limit(n)
            .mapToLong(i -> i)
            .sum();
        long time1 = System.nanoTime() - start1;

        // FAST: IntStream operates entirely on primitive ints inside CPU registers
        long start2 = System.nanoTime();
        long sum2 = IntStream.rangeClosed(1, n)
            .asLongStream()
            .sum();
        long time2 = System.nanoTime() - start2;

        System.out.printf("Boxed Stream:   %.2f ms%n", time1 / 1e6);
        System.out.printf("Primitive Stream: %.2f ms (4x-10x faster)%n", time2 / 1e6);
    }
}
```

### Example 3: Thread-Safety Bug in Parallel Streams

```java
import java.util.*;
import java.util.stream.IntStream;

public class ParallelStreamHazard {
    public static void main(String[] args) {
        // DANGEROUS: ArrayList is NOT thread-safe!
        List<Integer> unsafeList = new ArrayList<>();

        // Parallel streams execute concurrently across ForkJoinPool.commonPool()
        IntStream.range(0, 10_000).parallel().forEach(unsafeList::add);

        // Result is unpredictably LESS than 10,000 due to race conditions and lost writes!
        System.out.println("Unsafe size: " + unsafeList.size() + " (Expected: 10000)");

        // CORRECT: Use thread-safe reduction or collect()
        List<Integer> safeList = IntStream.range(0, 10_000).parallel()
            .boxed()
            .toList(); // Thread-safe collector

        System.out.println("Safe size: " + safeList.size());
    }
}
```

### Example 4: Custom `Collector` Implementation

```java
import java.util.*;
import java.util.stream.Collector;

public class MedianCollector {
    // Custom collector to compute the median of a stream of doubles
    public static Collector<Double, List<Double>, Double> toMedian() {
        return Collector.of(
            ArrayList::new,                  // Supplier
            List::add,                       // Accumulator
            (left, right) -> { left.addAll(right); return left; }, // Combiner
            list -> {                        // Finisher
                if (list.isEmpty()) return 0.0;
                Collections.sort(list);
                int mid = list.size() / 2;
                return (list.size() % 2 == 1) ? list.get(mid) : (list.get(mid - 1) + list.get(mid)) / 2.0;
            }
        );
    }

    public static void main(String[] args) {
        List<Double> scores = List.of(90.0, 85.0, 100.0, 70.0, 95.0);
        double median = scores.stream().collect(toMedian());
        System.out.println("Median Score: " + median); // 90.0
    }
}
```

### Example 5: Idiomatic `Optional` Usage

```java
import java.util.Optional;

public class OptionalBestPractices {
    record Profile(String bio) {}
    record User(String id, Profile profile) {}

    // Clean functional transformation with flatMap
    public static String extractBio(User user) {
        return Optional.ofNullable(user)
            .map(User::profile)
            .map(Profile::bio)
            .filter(bio -> !bio.isBlank())
            .orElse("No bio provided");
    }

    public static void main(String[] args) {
        User u1 = new User("U1", new Profile("Java Specialist"));
        User u2 = new User("U2", null);

        System.out.println(extractBio(u1)); // "Java Specialist"
        System.out.println(extractBio(u2)); // "No bio provided"
    }
}
```

---

## 6. Best Practices

1. **Keep Stream Operations Pure**: Lambda expressions inside stream pipelines must be **side-effect-free**. Never mutate external collections or shared state inside `.map()` or `.filter()`.
2. **Prefer Primitive Streams for Arithmetic**: Always use `IntStream`, `LongStream`, or `DoubleStream` to eliminate boxing and unboxing allocation costs.
3. **Never Call `Optional.get()` Directly**: Calling `.get()` without `.isPresent()` defeats the purpose of `Optional`. Always use `.orElse()`, `.orElseGet()`, `.orElseThrow()`, or functional mapping `.map()`.
4. **Never Use `Optional` for Class Fields or Method Parameters**: `Optional` does not implement `Serializable` and adds unnecessary pointer indirection. Use `Optional` exclusively as a **method return type** to signify possible absence.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Fix |
|---|---|---|
| Re-operating on a consumed stream | Throws `IllegalStateException: stream has already been operated upon or closed` | Create a new stream pipeline for each terminal operation. |
| Using `parallel()` on small collections ($N < 10,000$) | Thread coordination and context switching overhead exceeds execution gains | Keep streams sequential unless processing massive computational workloads. |
| Modifying external state inside `forEach()` | Violates functional principles; race conditions in parallel streams | Use standard `.collect()` or `.reduce()`. |
| Infinite stream without `limit()` | Pipeline hangs in an infinite loop allocating memory | Place `.limit()` before stateful or terminal operations. |
| Using `orElse(computeDefault())` instead of `orElseGet()` | Eagerly executes the default computation even when value is present! | Use lazy evaluation: `.orElseGet(() -> computeDefault())`. |

---

## 8. Performance & Security Notes

- **Parallel Stream ForkJoinPool Starvation**: By default, `stream.parallel()` executes on the JVM-wide `ForkJoinPool.commonPool()`. If a parallel stream performs blocking I/O (database queries, HTTP calls), it can saturate the entire common pool and starve all other asynchronous tasks across the JVM.
- **Short-Circuiting Optimization**: Operations like `anyMatch()`, `allMatch()`, `noneMatch()`, and `findFirst()` stop evaluation as soon as the result is determined, bypassing unnecessary computation on the remaining elements.
- **Inlining of Lambdas**: HotSpot's C2 compiler easily inlines non-capturing lambdas into the calling loop, generating assembly code that is identical to hand-written `for` loops.

---

## 9. Comparison / When to Use What

| Feature | Standard `for` Loop | Sequential `Stream` | `parallelStream()` |
|---|---|---|---|
| **Readability** | Verbose / Imperative | Declarative / High-level | Declarative |
| **Performance ($N < 10^4$)** | Fastest (zero overhead) | Negligible difference | Slower due to thread overhead |
| **Performance ($N > 10^6$, CPU heavy)**| Requires manual concurrency | Single core bound | Scales linearly across CPU cores |
| **Debugging** | Easy step-by-step | Harder (requires Peek / Stream debugger) | Very complex |
| **Control Flow** | Supports `break`, `continue`, `return`| Cannot break early (except short-circuit)| Cannot break early |

---

## 10. Interview Questions

<details>
<summary>1. How do Lambdas differ from Anonymous Inner Classes in bytecode and memory?</summary>

1. **Bytecode Instruction**: Anonymous inner classes use `new` and `invokespecial`. Lambdas use `invokedynamic` targeting `LambdaMetafactory`.
2. **Generated Classes**: Anonymous inner classes create physical `.class` files on disk (`OuterClass$1.class`). Lambdas generate lightweight classes in memory dynamically at runtime.
3. **Allocation**: An anonymous inner class allocates a new heap object every time it is evaluated. A non-capturing lambda is instantiated once by the JVM and cached as a singleton.
4. **Scope of `this`**: In an inner class, `this` refers to the inner class instance. In a lambda, `this` refers to the enclosing class instance (lexical scoping).
</details>

<details>
<summary>2. What is the difference between Intermediate and Terminal operations in Streams?</summary>

- **Intermediate Operations** (e.g. `filter`, `map`, `sorted`) are **lazy**. They return a new Stream and execute zero work until a terminal operation is invoked. They can be stateless or stateful.
- **Terminal Operations** (e.g. `collect`, `reduce`, `count`, `forEach`) are **eager**. They trigger traversal of the pipeline, consume the elements, and return a concrete value or side-effect. Once terminated, the stream cannot be reused.
</details>

<details>
<summary>3. Why is using `parallelStream()` dangerous in enterprise web applications?</summary>

Parallel streams execute on the shared, JVM-wide `ForkJoinPool.commonPool()`, which has a parallelism level equal to `Runtime.getRuntime().availableProcessors() - 1`. If parallel streams perform blocking operations (e.g., HTTP requests or database calls), worker threads become blocked, starving all other concurrent parallel streams and asynchronous tasks across the entire application server.
</details>

<details>
<summary>4. What is the difference between `Optional.orElse()` and `Optional.orElseGet()`?</summary>

- **`orElse(defaultVal)`** evaluates its argument **eagerly**, regardless of whether the `Optional` is present or empty. If the default involves an expensive database query or object instantiation, it runs redundantly!
- **`orElseGet(Supplier)`** evaluates lazily: the `Supplier` lambda is invoked **only if the Optional is empty**.
</details>

<details>
<summary>5. Explain the difference between `reduce()` and `collect()` in the Stream API.</summary>

- **`reduce()`** is intended for **immutable reductions**: it combines elements pairwise to produce a new value each time (`(a, b) -> a + b`). Using `reduce()` to accumulate into a mutable container (like an `ArrayList`) copies the entire list on every step, degrading to $O(N^2)$ time.
- **`collect()`** is designed for **mutable reductions**: it accumulates elements into a single mutable container (like an `ArrayList` or `StringBuilder`) in $O(N)$ time.
</details>

<details>
<summary>6. What are Short-Circuiting operations in Streams?</summary>

An operation is short-circuiting if it can produce a finite result or terminate processing without evaluating an infinite or remaining stream.
- Intermediate short-circuit: `limit(n)`.
- Terminal short-circuits: `findFirst()`, `findAny()`, `anyMatch()`, `allMatch()`, `noneMatch()`.
</details>

<details>
<summary>7. What is the Teeing Collector introduced in Java 12 (JEP 332)?</summary>

`Collectors.teeing()` takes two downstream collectors and a merger function. It passes every stream element through **both collectors simultaneously in a single pass**, and merges their results into a composite value (e.g. computing sum and count together to calculate average without streaming twice).
</details>

<details>
<summary>8. Can you reuse a Stream after a terminal operation has been executed?</summary>

No. A Stream can be traversed **strictly once**. Attempting to invoke another operation on a consumed stream throws `IllegalStateException: stream has already been operated upon or closed`. To perform multiple operations, you must generate a new stream from the source.
</details>

---

## 11. Practice Exercises

### Exercise 1: Implement an In-Memory Top-K Search Using Streams
Given a list of strings, return the Top 3 most frequent words using Stream collectors.

<details>
<summary>Solution</summary>

```java
import java.util.*;
import java.util.stream.Collectors;

public class TopKFrequentStream {
    public static List<String> top3Words(List<String> words) {
        return words.stream()
            .collect(Collectors.groupingBy(w -> w, Collectors.counting()))
            .entrySet().stream()
            .sorted(Map.Entry.<String, Long>comparingByValue().reversed()
                .thenComparing(Map.Entry::getKey))
            .limit(3)
            .map(Map.Entry::getKey)
            .toList();
    }
}
```
</details>

### Exercise 2: Diagnose the Eager Evaluation Bug
Why does this code perform an unnecessary database query, and how do you fix it?
```java
public User findUser(String id) {
    return userCache.get(id)
        .orElse(fetchFromDatabase(id)); // BUGGY LINE!
}
```

<details>
<summary>Solution</summary>

**Cause**: `orElse()` evaluates its argument eagerly at method call time. Therefore, `fetchFromDatabase(id)` executes **on every single cache hit**, completely defeating the cache!

**Fix (Lazy Supplier)**:
```java
public User findUser(String id) {
    return userCache.get(id)
        .orElseGet(() -> fetchFromDatabase(id));
}
```
</details>

### Exercise 3: Flatten a Nested Data Structure with `flatMap`
Transform a list of `Department` objects (each holding a list of `Employee` objects) into a deduplicated list of all distinct job titles.

<details>
<summary>Solution</summary>

```java
import java.util.*;

public class DepartmentFlattening {
    record Employee(String name, String jobTitle) {}
    record Department(String name, List<Employee> employees) {}

    public static List<String> getUniqueJobTitles(List<Department> departments) {
        return departments.stream()
            .map(Department::employees)
            .filter(Objects::nonNull)
            .flatMap(List::stream)
            .map(Employee::jobTitle)
            .filter(Objects::nonNull)
            .distinct()
            .sorted()
            .toList();
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **`invokedynamic`**: Lambdas use `LambdaMetafactory`; non-capturing lambdas are zero-allocation singletons.
- **Lazy Pipelines**: Intermediate operations defer execution until an eager terminal operation executes.
- **Primitive Streams**: Always prefer `IntStream` over `Stream<Integer>` to eliminate boxing.
- **`Optional` Invariant**: Never call `.get()` bare; use `.orElseGet()` for lazy default evaluation.
- **Teeing Collector**: Evaluates two collectors simultaneously in a single linear pass.

---

## 13. Further Reading

- [Brian Goetz: Translation of Lambda Expressions](https://cr.openjdk.org/~briangoetz/lambda/lambda-translation.html)
- [JEP 332: Teeing Collector (Java 12)](https://openjdk.org/jeps/332)
- [Baeldung: Guide to Java 8 Streams](https://www.baeldung.com/java-8-streams)
- [Joshua Bloch: Effective Java (3rd Edition), Items 42-48: Lambdas and Streams](https://www.oreilly.com/library/view/effective-java-3rd/9780134686097/)
