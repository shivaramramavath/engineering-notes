# Intermediate and Terminal Stream Operations

> Operational mechanics: stateless vs stateful intermediate operations, short-circuiting vs non-short-circuiting terminal operations, and Java 22/24 Stream Gatherers (JEP 485).

| Metadata | Value |
|---|---|
| **Level** | Advanced |
| **Prerequisites** | [04_streams-fundamentals.md](04_streams-fundamentals.md), [01_functional-interfaces.md](01_functional-interfaces.md) |
| **Time to Read** | 26 minutes |
| **Status** | Active |
| **Java Version** | Java 8+ (Stream Gatherers Java 22/24 Preview / Final JEP 485) |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Stream Gatherers (JEP 485)](#5-stream-gatherers-jep-485)
6. [Practical Examples](#6-practical-examples)
7. [Best Practices](#7-best-practices)
8. [Common Mistakes & Pitfalls](#8-common-mistakes--pitfalls)
9. [Performance & Security Notes](#9-performance--security-notes)
10. [Comparison / When to Use What](#10-comparison--when-to-use-what)
11. [Interview Questions](#11-interview-questions)
12. [Practice Exercises](#12-practice-exercises)
13. [Summary / Cheat Sheet](#13-summary--cheat-sheet)
14. [Further Reading](#14-further-reading)

---

## 1. What & Why

Understanding the behavioral classification of stream operations is critical for writing correct, high-performance applications.
Not all stream operations behave alike:
- Some operations can process elements one by one without retaining memory (**Stateless Intermediate Operations**).
- Other operations must buffer elements in memory before producing any output (**Stateful Intermediate Operations**), which can cause massive memory consumption or freeze infinite streams.
- Terminal operations determine whether a pipeline can terminate early (**Short-Circuiting Terminal Operations**) or must process the entire dataset.

Furthermore, for a decade, developers were limited to the fixed set of built-in intermediate operations (`filter`, `map`, `sorted`). In modern Java (Java 22/24 JEP 485), Java introduced **Stream Gatherers**, unlocking custom user-defined intermediate operations like sliding windows, batching, and stateful folds.

---

## 2. Core Concepts

### Taxonomy of Stream Operations
```
                              Stream Operations
                                      |
         +----------------------------+----------------------------+
         |                                                         |
Intermediate Operations (Return Stream<T>)             Terminal Operations (Return Non-Stream)
         |                                                         |
    +----+----+                                               +----+----+
    |         |                                               |         |
Stateless  Stateful                                      Short-Circuit  Non-Short-Circuit
(filter)   (sorted)                                      (findFirst)    (collect, reduce)
(map)      (distinct)                                    (anyMatch)     (count, forEach)
(flatMap)  (limit, skip)                                 (allMatch)
```

### Stateless vs Stateful Operations
- **Stateless Intermediate Operations**: Retain no state from previously seen elements. Each element is evaluated independently.
  Examples: `filter()`, `map()`, `flatMap()`, `peek()`, `unordered()`.
- **Stateful Intermediate Operations**: Must maintain state from previously seen elements to process the current element.
  Examples: `distinct()` (must track seen set), `sorted()` (must buffer all elements before emitting), `limit(n)`, `skip(n)`.

### Short-Circuiting Operations
- **Short-Circuiting Intermediate**: Can transform an infinite stream into a finite stream (`limit(n)`).
- **Short-Circuiting Terminal**: Can finish processing without inspecting the entire stream (`findFirst()`, `findAny()`, `anyMatch()`, `allMatch()`, `noneMatch()`).

---

## 3. How It Works Internally

### `Sink` Pipeline Chaining
When a terminal operation executes, intermediate operations are represented as a chain of `Sink` instances:
```java
interface Sink<T> extends Consumer<T> {
    default void begin(long size) {}
    default void end() {}
    default boolean cancellationRequested() { return false; }
}
```
1. `begin(size)`: Tells downstream sinks how many elements are expected (enables pre-sizing buffers).
2. `accept(item)`: Pushes an element downstream.
3. `cancellationRequested()`: Short-circuit flag. If a sink returns `true` (e.g. `findFirst` found its element), the upstream source immediately stops pushing data!
4. `end()`: Signals that no more elements are coming (triggers stateful operations like `sorted` to sort their buffer and flush).

---

## 4. Syntax & Usage

### 1. `flatMap` for One-to-Many Expansions
```java
import java.util.List;

public class FlatMapDemo {
    public static void main(String[] args) {
        List<List<String>> nested = List.of(
            List.of("A", "B"),
            List.of("C", "D", "E")
        );

        // Flattens Stream<List<String>> into Stream<String>
        List<String> flat = nested.stream()
            .flatMap(List::stream)
            .toList();

        System.out.println("Flattened: " + flat); // [A, B, C, D, E]
    }
}
```

### 2. Multi-Condition Reductions
```java
import java.util.List;

public class ReductionDemo {
    public static void main(String[] args) {
        List<Integer> numbers = List.of(1, 2, 3, 4, 5);

        // Sum via reduce(identity, accumulator)
        int sum = numbers.stream().reduce(0, Integer::sum);
        System.out.println("Sum: " + sum); // 15

        // Max via reduce without identity (returns Optional)
        int max = numbers.stream().reduce(Integer::max).orElse(0);
        System.out.println("Max: " + max); // 5
    }
}
```

---

## 5. Stream Gatherers (JEP 485)

### Custom Intermediate Operations
Introduced in Java 22 and finalized in Java 24 (JEP 485), **Stream Gatherers** (`java.util.stream.Gatherer<T, A, R>`) allow developers to create custom intermediate operations, extending the Streams API just as `Collector` extends terminal operations.

A Gatherer defines:
- **`Initializer`**: Supplies private state.
- **`Integrator`**: Processes elements and decides whether to emit downstream or cancel.
- **`Combiner`**: Combines state for parallel evaluation.
- **`Finisher`**: Emits trailing state when stream completes.

### Built-in Standard Gatherers (`java.util.stream.Gatherers`)
1. `Gatherers.windowFixed(windowSize)`: Groups elements into fixed-size lists:
   `[1, 2, 3, 4, 5]` with size 2 $\to$ `[[1, 2], [3, 4], [5]]`.
2. `Gatherers.windowSliding(windowSize)`: Groups elements into overlapping sliding windows:
   `[1, 2, 3, 4]` with size 2 $\to$ `[[1, 2], [2, 3], [3, 4]]`.
3. `Gatherers.fold(initialSupplier, folder)`: Performs an intermediate stateful cumulative reduction.
4. `Gatherers.scan(initialSupplier, scanner)`: Prefix-scan emitting running intermediate totals.
5. `Gatherers.mapConcurrent(maxConcurrency, mapper)`: Runs virtual thread concurrent mappings.

```java
// Example using Gatherers (Java 22+ preview / Java 24+ final)
// List<List<Integer>> windows = Stream.of(1, 2, 3, 4, 5)
//     .gather(Gatherers.windowFixed(2))
//     .toList();
// Result: [[1, 2], [3, 4], [5]]
```

---

## 6. Practical Examples

### Example 1: Finding Distinct Words Across Multiple Documents (Basic)
```java
import java.util.List;

public class DocumentWordExtractor {
    public static void main(String[] args) {
        List<String> docs = List.of(
            "Java streams are fast",
            "Functional programming in Java",
            "Streams make code clean"
        );

        List<String> uniqueWords = docs.stream()
            .map(String::toLowerCase)
            .flatMap(doc -> List.of(doc.split("\\s+")).stream())
            .distinct()
            .sorted()
            .toList();

        System.out.println("Unique words: " + uniqueWords);
    }
}
```

### Example 2: Sliding Window Moving Average (Real-World)
```java
import java.util.ArrayList;
import java.util.List;

public class MovingAverageDemo {
    public static List<Double> computeMovingAverage(List<Double> data, int windowSize) {
        List<Double> averages = new ArrayList<>();
        double currentSum = 0.0;

        for (int i = 0; i < data.size(); i++) {
            currentSum += data.get(i);
            if (i >= windowSize) {
                currentSum -= data.get(i - windowSize);
            }
            if (i >= windowSize - 1) {
                averages.add(currentSum / windowSize);
            }
        }
        return averages;
    }

    public static void main(String[] args) {
        List<Double> temps = List.of(20.0, 22.0, 24.0, 21.0, 19.0, 25.0);
        System.out.println("Moving Averages (window 3): " + computeMovingAverage(temps, 3));
    }
}
```

### Example 3: Edge Case - `peek()` is Not Guaranteed to Execute
```java
import java.util.List;

public class PeekMisuseEdgeCase {
    public static void main(String[] args) {
        List<String> items = List.of("A", "B", "C");

        // ANTI-PATTERN: Relying on peek for production side effects!
        // The Stream implementation can optimize count() by reading source size directly,
        // completely SKIPPING the peek operation!
        long count = items.stream()
            .peek(s -> System.out.println("Processing: " + s))
            .count();

        System.out.println("Count: " + count);
        // Notice: In modern Java, peek() may never execute for count()!
    }
}
```

### Example 4: Failure Case - Using Stateful Operations on Infinite Streams
```java
import java.util.stream.Stream;

public class StatefulInfiniteFailure {
    public static void main(String[] args) {
        // distinct() must track seen elements in memory forever!
        // Calling distinct() on infinite random stream causes OutOfMemoryError!
        // Stream.generate(Math::random).distinct().limit(10); // Dangerous if unbounded
    }
}
```

---

## 7. Best Practices

1. **Avoid `peek()` in Production Code**: `peek()` is intended strictly for debugging (`System.err.println`). Optimizers can bypass `peek()` without notice.
2. **Order Intermediate Operations Thoughtfully**: Place cheap stateless filters early in the pipeline (`filter`) before expensive operations (`sorted`, `map`) to reduce downstream workload.
3. **Use Short-Circuiting Operations Where Applicable**: Replace `.filter(x).toList().get(0)` with `.filter(x).findFirst().orElse(...)`.
4. **Leverage Stream Gatherers in Java 24+**: Use `gather(Gatherers.windowFixed(n))` instead of writing custom procedural chunking logic.

---

## 8. Common Mistakes & Pitfalls

| Mistake | Why Wrong | Fix |
|---|---|---|
| Using `peek()` for business logic | JIT/Stream engine may skip execution if count can be derived. | Use `forEach` or `map`. |
| Sorting an infinite stream | `sorted()` buffers forever, causing `OutOfMemoryError`. | Place `.limit()` before `.sorted()`. |
| Calling `.collect()` when a primitive reduction exists | E.g. `.map(x -> x).collect(toList()).size()` instead of `.count()`. | Use native terminal reduction. |
| Using `findAny()` expecting deterministic first match | In parallel streams, `findAny()` returns whatever thread finishes first. | Use `findFirst()` if order matters. |
| Neglecting `identity` rules in `reduce` | Identity must satisfy `reduce(identity, x) == x`; invalid identity produces wrong totals. | Obey mathematical monoid laws. |
| Mutating external state inside `forEach()` | Breaches functional contract; causes thread data races in parallel streams. | Use `.collect()` to gather results. |

---

## 9. Performance & Security Notes

### Performance
- **Stateful Buffer Allocations**: `sorted()` allocates an intermediate `Object[]` buffer sized to the incoming stream. On 10,000,000 elements, this requires a temporary 40MB array allocation before sorting.
- **Short-Circuit Sink Cancellation**: When `findFirst` hits a match, `cancellationRequested()` becomes true, stopping upstream spliterators within 1-2 elements.

### Security
- **Resource Exhaustion via Unbounded Buffering**: Do not allow untrusted user input to sort unbounded or excessively large streams without memory caps.

---

## 10. Comparison / When to Use What

| Operation | Category | Short-Circuiting? | Buffering Required? |
|---|---|---|---|
| `filter(Predicate)` | Intermediate (Stateless) | No | No ($O(1)$ space) |
| `map(Function)` | Intermediate (Stateless) | No | No ($O(1)$ space) |
| `distinct()` | Intermediate (Stateful) | No | Yes (Tracks seen Set) |
| `sorted()` | Intermediate (Stateful) | No | Yes (Buffers entire stream) |
| `limit(n)` | Intermediate (Stateful) | Yes | No ($O(1)$ space) |
| `findFirst()` | Terminal | Yes | No ($O(1)$ space) |
| `reduce()` | Terminal | No | No ($O(1)$ space) |
| `collect()` | Terminal | No | Yes (Accumulates into target) |

---

## 11. Interview Questions

<details>
<summary>1. What is the difference between stateless and stateful intermediate operations?</summary>
Stateless intermediate operations (`filter`, `map`) process each element independently without retaining memory of previous elements. Stateful intermediate operations (`sorted`, `distinct`, `limit`) must buffer elements or retain state across elements to determine whether and how to emit the current element.
</details>

<details>
<summary>2. Why does `sorted()` break stream laziness?</summary>
`sorted()` is an inherently stateful barrier operation: it is impossible to emit the smallest element until **every single element** in the stream has been examined. Therefore, `sorted()` must pull and buffer all upstream elements before downstream operations can proceed.
</details>

<details>
<summary>3. Why is `peek()` considered an anti-pattern for business logic?</summary>
The specification for `peek()` explicitly states that it exists to support debugging. In many cases, the JVM stream implementation is allowed to optimize operations (e.g. `stream.peek(...).count()`) by computing the count directly from the collection size, omitting `peek()` execution entirely.
</details>

<details>
<summary>4. What are Stream Gatherers introduced in Java 22/24 (JEP 485)?</summary>
Stream Gatherers are custom intermediate stream operations. Just as `Collector` standardizes custom terminal operations, `Gatherer` allows developers to define custom intermediate transformations such as sliding windows, fixed batching, cumulative scans, and concurrent mappings.
</details>

<details>
<summary>5. What is the difference between `findFirst()` and `findAny()`?</summary>
`findFirst()` strictly returns the first element in the stream's encounter order. `findAny()` returns any element from the stream; in parallel streams, `findAny()` returns the element from whichever thread completes first, maximizing parallel performance.
</details>

<details>
<summary>6. How does `flatMap()` work?</summary>
`flatMap` accepts a function that maps each element to a `Stream` of new elements, and then flattens all the generated streams into a single combined downstream stream.
</details>

<details>
<summary>7. What mathematical properties must the `identity` parameter in `reduce(identity, accumulator)` satisfy?</summary>
The identity element must be a neutral element for the accumulator function: `accumulator.apply(identity, x) == x` for all `x`. For example, `0` is the identity for addition, `1` for multiplication, and `""` for string concatenation.
</details>

<details>
<summary>8. How does `distinct()` determine element uniqueness?</summary>
It uses `equals()` and `hashCode()`. Internally, it maintains a hash set of previously encountered elements.
</details>

---

## 12. Practice Exercises

### Easy
1. Use `filter()` and `map()` to convert a list of strings to lengths for words starting with "A".
2. Find the maximum integer in a stream using `reduce()`.
3. Check if any number in a list is negative using `anyMatch()`.

### Medium
4. Given a list of sentences, count the total number of words across all sentences using `flatMap()`.
5. Group a list of numbers into distinct even and odd groups using `partitioningBy`.
6. Implement a top-3 largest numbers extractor using `sorted()` and `limit()`.

### Hard
7. Implement a custom `Gatherer` (or custom spliterator) that batches elements into chunks of size $N$.
8. Implement an in-memory stream pipeline that computes the standard deviation of numbers in a single pass.
9. Measure memory consumption and execution time between `stream().sorted()` vs `Collections.sort()` on 5,000,000 elements.

<details>
<summary>Solutions</summary>

```java
// Easy 1: filter and map
List<String> list = List.of("Apple", "Avocado", "Banana", "Apricot");
List<Integer> lengths = list.stream().filter(s -> s.startsWith("A")).map(String::length).toList();

// Easy 2: Max reduction
int max = List.of(10, 50, 20).stream().reduce(Integer.MIN_VALUE, Integer::max);

// Medium 4: Word count via flatMap
List<String> sentences = List.of("Hello world", "Java streams are awesome");
long wordCount = sentences.stream().flatMap(s -> List.of(s.split("\\s+")).stream()).count();
```
</details>

---

## 13. Summary / Cheat Sheet

- **Stateless Intermediate**: `filter`, `map`, `flatMap` (Process on the fly, $O(1)$ memory).
- **Stateful Intermediate**: `sorted`, `distinct`, `limit`, `skip` (Require state/buffering).
- **Short-Circuiting**: Stops early (`limit`, `findFirst`, `anyMatch`, `allMatch`).
- **Reduction**: `reduce(identity, accumulator)` requires valid mathematical identity.
- **Gatherers (JEP 485)**: Custom intermediate operations (`windowFixed`, `windowSliding`, `fold`).
- **Peek Caution**: Use `peek()` for debugging only, never production business logic.

---

## 14. Further Reading
- JEP 485: *Stream Gatherers* (OpenJDK).
- Bloch, Joshua. *Effective Java (3rd Edition)*: Item 46 (Prefer side-effect-free functions in streams).
- Oracle Java Documentation: *Stream Pipelines and Operations*.
