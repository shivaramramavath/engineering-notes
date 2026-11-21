# Streams Fundamentals

> Declarative sequence processing: stream pipeline architecture, lazy evaluation mechanics, consumption lifecycle, and creation sources.

| Metadata | Value |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | [00_lambda-expressions.md](00_lambda-expressions.md), [01_functional-interfaces.md](01_functional-interfaces.md), [08-collections/00_collection-hierarchy.md](../08-collections/00_collection-hierarchy.md) |
| **Time to Read** | 24 minutes |
| **Status** | Active |
| **Java Version** | Java 8+ |

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

Before Java 8, manipulating collections required writing imperative loops: manually setting up iterators, managing intermediate accumulator lists, and nesting control flow branches (`if`, `for`). This imperative style suffered from several shortcomings:
- **High Cognitive Complexity**: The business logic was obscured by boilerplate iteration mechanics.
- **Accidental Mutation**: Mutable accumulators were exposed to thread-safety bugs.
- **Lack of Parallelization**: Parallelizing an imperative loop required rewriting the entire loop with threads, mutex locks, and error handling.

Introduced in Java 8, the **Java Streams API** (`java.util.stream`) represents a sequence of elements supporting sequential and parallel aggregate operations. A stream is **not** a data structure; it does not store elements. Instead, it conveys elements from a **source** (such as a collection, an array, a generator function, or I/O channel) through a **pipeline of computational operations** declaratively.

---

## 2. Core Concepts

### The 3 Stages of a Stream Pipeline
Every stream pipeline consists of three distinct phases:
1. **Source**: Produces the stream elements (e.g., `list.stream()`, `IntStream.range(1, 100)`).
2. **Intermediate Operations**: Transform the stream into another stream (e.g., `filter()`, `map()`, `sorted()`). Intermediate operations are **lazy**; they execute no computation until a terminal operation is invoked.
3. **Terminal Operation**: Produces a non-stream result (a primitive, an object, a collection, or void side-effect, e.g., `collect()`, `reduce()`, `count()`, `forEach()`). The terminal operation **initiates** data processing and closes the stream.

```
+------------+       +-------------------+       +--------------------+
|   Source   | ----> | Intermediate Ops  | ----> | Terminal Operation |
| Collection |       | filter() -> map() |       |     collect()      |
+------------+       +-------------------+       +--------------------+
                              ^
                              | (Evaluated lazily in a single pass!)
```

### Core Characteristics of Streams
- **No Storage**: A stream does not store data; it operates on the backing source on demand.
- **Functional in Nature**: A stream operation produces a result without mutating its source.
- **Laziness-Seeking**: Operations are evaluated lazily to enable optimizations like loop fusion and short-circuiting.
- **Possibly Unbounded**: Streams can be infinite (`Stream.generate()`, `Stream.iterate()`); short-circuiting operations can truncate infinite streams into finite computations.
- **Consumable Once**: A stream cannot be reused. Once a terminal operation is called, the stream is considered consumed and closed.

---

## 3. How It Works Internally

### Pipeline Linking and Pipeline Stages
Inside OpenJDK, stream operations are implemented via linked pipeline stages inheriting from `java.util.stream.AbstractPipeline`:
- `Head`: Represents the stream source.
- `StatelessOp`: Represents intermediate operations that do not require state across elements (e.g., `filter`, `map`).
- `StatefulOp`: Represents intermediate operations that require knowledge of previously processed elements (e.g., `sorted`, `distinct`).

Each intermediate operation creates a new `PipelineHelper` stage linked backward to the previous stage.

```
list.stream()                     .filter(x -> x > 10)            .map(x -> x * 2)
[ Head: ReferencePipeline$Head ] <- [ StatelessOp: filter ] <- [ StatelessOp: map ]
                                                                       ^
                                                                       |
Terminal op: .toList() ------------------------------------------------+
Triggers single-pass loop traversal!
```

### Loop Fusion and Lazy Pull
When a terminal operation is invoked:
1. The stream stages are compiled into a downstream chain of `Sink` objects.
2. The source spliterator pushes elements into the first sink.
3. Each sink processes an element and immediately pushes it to the downstream sink.
4. **Loop Fusion**: Instead of executing `filter` on all elements into a temporary list and then executing `map` on all elements, elements flow through `filter -> map -> collector` **one element at a time**.

---

## 4. Syntax & Usage

### 1. Common Stream Creation Sources
```java
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Arrays;
import java.util.List;
import java.util.Random;
import java.util.stream.IntStream;
import java.util.stream.Stream;

public class StreamSourcesDemo {
    public static void main(String[] args) throws IOException {
        // 1. From Collection
        Stream<String> s1 = List.of("A", "B", "C").stream();

        // 2. From Array
        Stream<String> s2 = Arrays.stream(new String[]{"X", "Y"});

        // 3. Static Factory of(...)
        Stream<Integer> s3 = Stream.of(1, 2, 3);

        // 4. Primitive Numeric Ranges
        IntStream s4 = IntStream.rangeClosed(1, 5); // 1, 2, 3, 4, 5

        // 5. Infinite Streams (Truncated via limit)
        Stream<Double> s5 = Stream.generate(Math::random).limit(3);
        Stream<Integer> s6 = Stream.iterate(0, n -> n + 2).limit(5); // 0, 2, 4, 6, 8

        // 6. I/O Channel lines
        // Stream<String> lines = Files.lines(Path.of("data.txt"));
    }
}
```

### 2. Demonstrating Laziness
```java
import java.util.List;

public class StreamLazinessDemo {
    public static void main(String[] args) {
        List<String> names = List.of("Alice", "Bob", "Charlie", "David");

        // The filter and map statements DO NOT RUN HERE!
        var stream = names.stream()
            .filter(name -> {
                System.out.println("Filtering: " + name);
                return name.length() > 3;
            })
            .map(name -> {
                System.out.println("Mapping: " + name);
                return name.toUpperCase();
            });

        System.out.println("--> Terminal operation starting...");
        // Execution begins ONLY when terminal operation (findFirst) is invoked:
        String result = stream.findFirst().orElse("NONE");
        System.out.println("Result: " + result);
    }
}
```
Output:
```
--> Terminal operation starting...
Filtering: Alice
Mapping: Alice
Result: ALICE
```
Notice that "Bob", "Charlie", and "David" are **never filtered or mapped** because `findFirst()` short-circuits on the first match!

---

## 5. Practical Examples

### Example 1: Declarative CSV String Processing (Basic)
```java
import java.util.Arrays;
import java.util.List;

public class CsvProcessor {
    public static void main(String[] args) {
        String rawData = "  apple, ORANGE, , banana , Cherry, APPLE ";

        List<String> cleaned = Arrays.stream(rawData.split(","))
            .map(String::trim)
            .filter(s -> !s.isEmpty())
            .map(String::toLowerCase)
            .distinct()
            .sorted()
            .toList();

        System.out.println("Cleaned tokens: " + cleaned);
        // [apple, banana, cherry, orange]
    }
}
```

### Example 2: Fibonacci Generator via `Stream.iterate` (Real-World)
```java
import java.util.List;
import java.util.stream.Stream;

public class FibonacciStream {
    public static List<Long> generateFibonacci(int n) {
        return Stream.iterate(new long[]{0, 1}, f -> new long[]{f[1], f[0] + f[1]})
            .limit(n)
            .map(f -> f[0])
            .toList();
    }

    public static void main(String[] args) {
        System.out.println("First 10 Fib numbers: " + generateFibonacci(10));
        // [0, 1, 1, 2, 3, 5, 8, 13, 21, 34]
    }
}
```

### Example 3: Edge Case - Consuming Stream Twice Throws IllegalStateException
```java
import java.util.stream.Stream;

public class StreamReuseFailure {
    public static void main(String[] args) {
        Stream<String> stream = Stream.of("A", "B", "C");

        stream.forEach(System.out::println); // First terminal op: CONSUMES STREAM!

        try {
            // Attempting to reuse the same stream instance
            long count = stream.count();
        } catch (IllegalStateException e) {
            System.out.println("Caught expected: " + e.getMessage());
            // "stream has already been operated upon or closed"
        }
    }
}
```

### Example 4: Failure Case - Infinite Stream without Short-Circuiting
```java
import java.util.stream.Stream;

public class InfiniteStreamFreezeFailure {
    public static void main(String[] args) {
        // FATAL MISTAKE: Calling sorted() or toList() on an infinite stream without limit()
        // Stream.iterate(0, n -> n + 1).sorted().forEach(System.out::println); // FREEZES JVM!
    }
}
```

---

### Spliterator Characteristics and Characteristics Flags
The efficiency of a stream pipeline is determined by its underlying Spliterator characteristics flags:
- ORDERED: Encounter order is preserved (e.g. List, linked collections).
- DISTINCT: Elements are guaranteed unique according to equals().
- SORTED: Elements follow a defined sorting order (Comparable or custom Comparator).
- SIZED: The exact number of elements is known prior to traversal.
- NONNULL: Elements are guaranteed non-null.
- IMMUTABLE: Source collection cannot be structurally modified.
- CONCURRENT: The data source can be safely modified concurrently across threads.
- SUBSIZED: Split child spliterators are also guaranteed SIZED.

## 6. Best Practices

1. **Keep Pipeline Operations Stateless and Non-Interfering**: Do not modify the backing collection while a stream is executing; avoid stateful lambda variables.
2. **Use Primitive Streams for Numbers**: Use `IntStream`, `LongStream`, and `DoubleStream` to avoid massive auto-boxing garbage.
3. **Close Streams Backed by I/O Resources**: When using `Files.lines()`, always wrap the stream in a `try-with-resources` block because `Files.lines` holds open an underlying OS file handle.
4. **Prefer Method References in Stream Chains**: `map(String::toUpperCase)` is cleaner and more readable than `map(s -> s.toUpperCase())`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why Wrong | Fix |
|---|---|---|
| Reusing a stream instance | Streams are single-use; throws `IllegalStateException`. | Re-create stream from source. |
| Forgetting terminal operation | Intermediate operations never execute without a terminal operation. | Call `.collect()`, `.toList()`, etc. |
| Modifying stream source during traversal | Throws `ConcurrentModificationException`. | Keep stream sources immutable or uncoupled. |
| Unclosed `Files.lines()` streams | Leaks OS file descriptors. | Use `try (Stream<String> lines = Files.lines(...))`. |
| Calling stateful operations on infinite streams | E.g. `iterate(0, ...).sorted()` loops infinitely trying to buffer infinity. | Call `.limit(N)` before stateful ops. |
| Using streams for simple 1-line loop tasks | Adds overhead when an ordinary `for` loop is simpler. | Use streams when data transformations/filtering are present. |

---

## 8. Performance & Security Notes

### Performance
- **Loop Fusion**: Intermediate operations execute in a single integrated pass over the data, avoiding intermediate array allocations.
- **Overhead for Tiny Collections**: For collections with fewer than 20 elements, an imperative index loop (`for (int i=0; i<len; i++)`) is often faster due to zero pipeline object overhead.

### Security
- **Denial of Service via Infinite Stream Generation**: Never expose un-bounded `Stream.generate()` endpoints to untrusted input without enforcing hard `.limit()` caps.

---

## 9. Comparison / When to Use What

| Feature | Collections | Streams |
|---|---|---|
| **Primary Purpose** | Storing and managing data | Computing and transforming data |
| **Data Storage** | Holds elements in heap memory | Holds no elements; pipeline wrapper |
| **Evaluation** | Eager (computed upfront) | Lazy (computed on terminal operation) |
| **Reusability** | Reusable indefinitely | Single-use only |
| **Iteration** | External iteration (`for`, `iterator`) | Internal iteration (`forEach`, `sink`) |

---

## 10. Interview Questions

<details>
<summary>1. What is the fundamental difference between an intermediate and a terminal stream operation?</summary>
Intermediate operations return a new `Stream`, are evaluated lazily, and perform no actual work until a terminal operation is called. Terminal operations initiate data processing, traverse elements through the pipeline, produce a final non-stream result (or side effect), and consume/close the stream.
</details>

<details>
<summary>2. Why does a stream throw `IllegalStateException` if operated upon twice?</summary>
A stream is a transient pipeline of operations that consumes data from a spliterator. Once a terminal operation is executed, the internal spliterator cursor has moved to the end and the pipeline is closed. Reusing it would require re-buffering or rewinding the source, which is impossible for one-way streams (like network sockets or I/O channels).
</details>

<details>
<summary>3. What is "loop fusion" in Java Streams?</summary>
Loop fusion is an optimization where multiple intermediate operations (such as `filter` and `map`) are combined into a single pass over the elements. Instead of executing `filter` on all elements to create an intermediate list and then running `map` on all elements, each element passes through both operations consecutively in one step.
</details>

<details>
<summary>4. How does short-circuiting work in streams?</summary>
A short-circuiting operation terminates stream traversal as soon as the final result can be determined, without processing remaining elements. Examples include intermediate operation `limit(n)` and terminal operations `anyMatch()`, `allMatch()`, `findFirst()`.
</details>

<details>
<summary>5. Why must `Files.lines()` be wrapped in a try-with-resources block?</summary>
Because `Files.lines()` opens an underlying OS file channel. `Stream` implements `AutoCloseable`, and wrapping it in try-with-resources ensures `stream.close()` is invoked, releasing the file descriptor.
</details>

<details>
<summary>6. What is the difference between internal and external iteration?</summary>
In external iteration (e.g., `for` loop, `Iterator`), the client code controls the iteration steps, cursor advances, and indexing. In internal iteration (Streams), the library manages the iteration behind the scenes, allowing optimizations like parallelization, out-of-order execution, and lazy evaluation.
</details>

<details>
<summary>7. What happens if you call `Stream.generate(supplier).sorted()`?</summary>
It will freeze the application in an infinite loop. `sorted()` is a stateful operation that must see all elements before it can emit any sorted output. Because `generate()` produces an infinite stream, `sorted()` will attempt to buffer infinite elements until an `OutOfMemoryError` occurs.
</details>

<details>
<summary>8. How do primitive streams (`IntStream`) prevent performance bottlenecks?</summary>
They operate directly on unboxed primitive values (`int`, `long`, `double`), eliminating auto-boxing allocations, pointer dereferencing, and GC overhead.
</details>

---

## 11. Practice Exercises

### Easy
1. Create a stream of numbers from 1 to 10 and print their squares using `map()`.
2. Filter a list of strings to only include those starting with the letter "J".
3. Generate an infinite stream of random integers, limit it to 5, and collect to a list.

### Medium
4. Write a stream pipeline that reads words from a list, eliminates duplicates, sorts them by length, and joins them with commas.
5. Implement a prime number generator using `IntStream.iterate()` and a custom `isPrime` predicate.
6. Flatten a `List<List<Integer>>` into a single sorted list using `flatMap()`.

### Hard
7. Implement a custom `Spliterator<T>` that reads fixed-width binary records from a `ByteBuffer` and exposes them as a `Stream<Record>`.
8. Design a sliding window aggregation stream that computes a moving average of streaming sensor data.
9. Measure instruction cache efficiency and CPU cycles between imperative for-loops vs Java streams on 1,000,000 objects.

<details>
<summary>Solutions</summary>

```java
// Easy 1: Squares
IntStream.rangeClosed(1, 10).map(x -> x * x).forEach(System.out::println);

// Medium 4: Sort by length and join
List<String> words = List.of("apple", "banana", "fig", "pear", "apple");
String joined = words.stream()
    .distinct()
    .sorted(Comparator.comparingInt(String::length))
    .collect(Collectors.joining(", "));

// Medium 5: Prime Stream
public static boolean isPrime(int n) {
    if (n < 2) return false;
    return IntStream.rangeClosed(2, (int) Math.sqrt(n)).noneMatch(i -> n % i == 0);
}
public static List<Integer> firstPrimes(int count) {
    return IntStream.iterate(2, n -> n + 1).filter(PrimeDemo::isPrime).limit(count).boxed().toList();
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Pipeline**: Source $\to$ Intermediate Operations (Lazy) $\to$ Terminal Operation (Eager).
- **Consumable**: Once terminal op runs, stream is consumed; cannot be reused.
- **Loop Fusion**: Elements pass through chained operations one-by-one.
- **Short-Circuiting**: Stops early (`findFirst`, `limit`, `anyMatch`).
- **Primitives**: Always use `IntStream`, `LongStream`, `DoubleStream`.
- **I/O Streams**: Always use `try-with-resources` for `Files.lines()`.

---

## 13. Further Reading
- Oracle Java Documentation: `java.util.stream` package specification.
- Bloch, Joshua. *Effective Java (3rd Edition)*: Item 45 (Use streams judiciously).
- Goetz, Brian. *Java 8 Stream Performance and Internals*.
