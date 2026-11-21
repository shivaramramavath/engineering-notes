# Java Streams & Functional Pipelines Reference Cheat Sheet

> Exhaustive quick-reference for Java Streams: intermediate vs terminal operations, Collectors reference, primitive streams, reduction patterns, parallel stream concurrency, and performance tuning.

| Metadata | Details |
| :--- | :--- |
| **Level** | Comprehensive Reference |
| **Prerequisites** | [02_collections.md](./02_collections.md), [09-functional-java](../09-functional-java/README.md) |
| **Time to Read** | 35 minutes |
| **Status** | Complete |
| **Java Version** | Java 8 to 25 LTS |

---

## Table of Contents
1. [Stream Lifecycle & Architecture](#1-stream-lifecycle--architecture)
2. [Intermediate Operations Reference](#2-intermediate-operations-reference)
3. [Terminal Operations Reference](#3-terminal-operations-reference)
4. [java.util.stream.Collectors Master Index](#4-javautilstreamcollectors-master-index)
5. [Primitive Streams (IntStream, LongStream, DoubleStream)](#5-primitive-streams-intstream-longstream-doublestream)
6. [Short-Circuiting Operations](#6-short-circuiting-operations)
7. [Reduction & Folding Patterns](#7-reduction--folding-patterns)
8. [Parallel Streams & Spliterators](#8-parallel-streams--spliterators)
9. [Stream Performance Guidelines](#9-stream-performance-guidelines)
10. [Anti-Patterns & Pitfalls](#10-anti-patterns--pitfalls)

---

## 1. Stream Lifecycle & Architecture

A Stream pipeline consists of three sequential phases:
```
[Stream Source]  ->  [0 or More Intermediate Operations]  ->  [Terminal Operation]
(Collection, Array,    (filter, map, flatMap, sorted)         (collect, count,
 Generator, File)      *LAZY: No execution occurs yet*        reduce, forEach)
                                                              *EAGER: Pulls data*
```
- **Pipelines are Lazy:** Intermediate operations construct a linked pipeline of `Sink` objects. Data is only processed element-by-element when the terminal operation is invoked.
- **Single-Use Only:** Once a terminal operation completes, the stream is closed and cannot be reused.

---

## 2. Intermediate Operations Reference

| Operation | Input Parameter | Output | Statefulness | Description |
| :--- | :--- | :--- | :---: | :--- |
| `filter(Predicate<T>)` | `T -> boolean` | `Stream<T>` | Stateless | Retains elements matching predicate |
| `map(Function<T, R>)` | `T -> R` | `Stream<R>` | Stateless | Transforms each element |
| `flatMap(Function<T, Stream<R>>)`| `T -> Stream<R>` | `Stream<R>` | Stateless | Flattens nested streams into single stream |
| `mapMulti(BiConsumer<T, Consumer<R>>)` | `(T, Consumer) -> void` | `Stream<R>` | Stateless | Imperative 1-to-many mapper (Java 16+) |
| `distinct()` | None | `Stream<T>` | Stateful | Filters duplicates using `equals()` |
| `sorted()` | None | `Stream<T>` | Stateful | Sorts elements by natural order |
| `sorted(Comparator<T>)` | `Comparator<T>` | `Stream<T>` | Stateful | Sorts elements by comparator |
| `peek(Consumer<T>)` | `T -> void` | `Stream<T>` | Stateless | Performs action for debugging |
| `limit(long maxSize)` | `long` | `Stream<T>` | Short-circuit | Truncates stream to max size |
| `skip(long n)` | `long` | `Stream<T>` | Stateful | Discards the first $n$ elements |
| `takeWhile(Predicate<T>)` | `T -> boolean` | `Stream<T>` | Short-circuit | Takes elements while condition holds (Java 9+) |
| `dropWhile(Predicate<T>)` | `T -> boolean` | `Stream<T>` | Stateful | Drops elements until condition fails (Java 9+) |

---

## 3. Terminal Operations Reference

| Operation | Return Type | Category | Description |
| :--- | :--- | :---: | :--- |
| `collect(Collector<T, A, R>)` | `R` | Mutable Reduction | Accumulates into collections or containers |
| `toList()` | `List<T>` | Collector | Collects into unmodifiable List (Java 16+) |
| `count()` | `long` | Reduction | Returns total element count |
| `reduce(BinaryOperator<T>)` | `Optional<T>` | Immutable Reduction | Combines elements associatively |
| `min(Comparator<T>)` | `Optional<T>` | Reduction | Minimum element according to comparator |
| `max(Comparator<T>)` | `Optional<T>` | Reduction | Maximum element according to comparator |
| `forEach(Consumer<T>)` | `void` | Side-Effect | Iterates over each element |
| `forEachOrdered(Consumer<T>)` | `void` | Side-Effect | Iterates preserving encounter order |
| `findFirst()` | `Optional<T>` | Short-circuit | Returns first element |
| `findAny()` | `Optional<T>` | Short-circuit | Returns any element (optimal for parallel) |
| `anyMatch(Predicate<T>)` | `boolean` | Short-circuit | True if any element matches |
| `allMatch(Predicate<T>)` | `boolean` | Short-circuit | True if all elements match |
| `noneMatch(Predicate<T>)` | `boolean` | Short-circuit | True if no elements match |

---

## 4. `java.util.stream.Collectors` Master Index

```java
import static java.util.stream.Collectors.*;

// 1. Basic Collections
List<String> list = stream.collect(toList());
Set<String> set = stream.collect(toSet());
LinkedList<String> customList = stream.collect(toCollection(LinkedList::new));

// 2. Joining Strings
String csv = stream.collect(joining(", ", "[", "]")); // "[A, B, C]"

// 3. Grouping and Partitioning
Map<Department, List<Employee>> byDept = employees.stream()
        .collect(groupingBy(Employee::getDepartment));

Map<Boolean, List<Employee>> partitioned = employees.stream()
        .collect(partitioningBy(e -> e.getSalary() > 100_000));

// 4. Downstream Reductions
Map<Department, Long> countByDept = employees.stream()
        .collect(groupingBy(Employee::getDepartment, counting()));

Map<Department, Double> avgSalary = employees.stream()
        .collect(groupingBy(Employee::getDepartment, averagingDouble(Employee::getSalary)));

// 5. To Map
Map<Long, Employee> employeeMap = employees.stream()
        .collect(toMap(
                Employee::getId,
                Function.identity(),
                (existing, replacement) -> existing // Merge function for duplicate keys
        ));
```

---

## 5. Primitive Streams

Avoids autoboxing overhead when processing numerical sequences:
- `IntStream`, `LongStream`, `DoubleStream`

```java
// Numerical ranges
IntStream.range(0, 10);       // 0 to 9
IntStream.rangeClosed(1, 10); // 1 to 10

// Primitive aggregations
int sum = IntStream.of(1, 2, 3, 4, 5).sum();
OptionalDouble avg = IntStream.of(1, 2, 3).average();
IntSummaryStatistics stats = IntStream.of(1, 2, 3, 4, 5).summaryStatistics();
System.out.printf("Min: %d, Max: %d, Avg: %.2f%n", stats.getMin(), stats.getMax(), stats.getAverage());

// Conversion to object stream
Stream<Integer> boxedStream = IntStream.range(1, 5).boxed();
```

---

## 6. Short-Circuiting Operations

Short-circuiting operations do not need to process the entire infinite or finite stream to produce a result:
- Intermediate: `limit(n)`, `takeWhile(predicate)`
- Terminal: `anyMatch(predicate)`, `allMatch(predicate)`, `noneMatch(predicate)`, `findFirst()`, `findAny()`

```java
// Safely consuming an infinite generator stream
Stream.iterate(1, n -> n + 1)
      .filter(n -> n % 2 == 0)
      .limit(5) // Halts pipeline after 5 even integers
      .forEach(System.out::println);
```

---

## 7. Reduction & Folding Patterns

```java
// Form 1: Identity + BinaryOperator
int product = numbers.stream().reduce(1, (a, b) -> a * b);

// Form 2: No Identity -> Returns Optional (handles empty stream)
Optional<Integer> maxVal = numbers.stream().reduce(Integer::max);

// Form 3: Combiner for Parallel Reduction (Identity + Accumulator + Combiner)
int totalLength = strings.parallelStream().reduce(
        0,
        (acc, str) -> acc + str.length(), // Accumulator: (int, String) -> int
        Integer::sum                      // Combiner: (int, int) -> int
);
```

---

## 8. Parallel Streams & Spliterators

```java
// Executed across the common ForkJoinPool (ForkJoinPool.commonPool())
List<String> processed = items.parallelStream()
        .map(this::heavyComputation)
        .toList();
```

### When Parallel Streams Help:
1. **$N 	imes Q > 10,000$ Rule:** $N$ is number of elements, $Q$ is computational cost per element.
2. **Splitting Efficiency:** Source data structure splits efficiently:
   - **Excellent:** `ArrayList`, primitive arrays (`int[]`), `IntStream.range` (constant-time array slicing).
   - **Poor:** `LinkedList`, `Stream.iterate` (linear scan required to split).
3. **Stateless Operations:** Operations are completely side-effect free and associative.

---

## 9. Stream Performance Guidelines

1. **Avoid Boxing Overhead:** Use `mapToInt()`, `mapToLong()`, and primitive streams instead of `map(i -> (Integer) i)`.
2. **Prefer Traditional For-Loops for Micro-Benchmarking:** In ultra-low-latency hot paths (<100 nanoseconds), simple for-loops avoid pipeline instantiation and iterator allocations.
3. **Use `mapMulti` over `flatMap` for Small Fan-Outs:** `mapMulti` avoids creating intermediate `Stream` instances per parent element, drastically reducing GC pressure.

---

## 10. Anti-Patterns & Pitfalls

| Anti-Pattern | Why It Is Dangerous | Correct Alternative |
| :--- | :--- | :--- |
| **Modifying Shared State in `forEach`** | Causes data races and corrupted state when run in parallel streams. | Use reduction (`collect`, `reduce`) to accumulate results safely. |
| **Reusing a Closed Stream** | Throws `IllegalStateException: stream has already been operated upon or closed`. | Create a supplier: `Supplier<Stream<T>> streamSupplier = () -> list.stream();` |
| **Unbounded Parallel I/O** | `parallelStream()` exhausts `ForkJoinPool.commonPool()`, blocking other services across the JVM. | Use custom `ExecutorService` with Virtual Threads or dedicated pools. |
| **Using `peek()` for Business Logic** | HotSpot compiler optimizes away `peek()` when downstream terminal operations count elements. | Use `map()` or explicit `forEach()`. |


---

## 11. Custom Collector Implementation (`Collector.of`)

When standard collectors do not satisfy custom aggregation requirements, define a custom `Collector<T, A, R>` using `Collector.of(...)`:

```java
import java.util.StringJoiner;
import java.util.stream.Collector;

public class CustomCollectorRecipes {
    // Custom collector accumulating strings into a JSON array string
    public static Collector<String, StringBuilder, String> toJsonArrayCollector() {
        return Collector.of(
                StringBuilder::new,                                    // Supplier: mutable accumulator
                (sb, s) -> {                                           // Accumulator: adds element
                    if (sb.isEmpty()) sb.append("[");
                    else sb.append(", ");
                    sb.append('"').append(s).append('"');
                },
                (sb1, sb2) -> {                                        // Combiner: merges two accumulators
                    if (sb1.isEmpty()) return sb2;
                    if (sb2.isEmpty()) return sb1;
                    return sb1.append(", ").append(sb2.substring(1));
                },
                sb -> sb.isEmpty() ? "[]" : sb.append("]").toString(), // Finisher: final transformation
                Collector.Characteristics.UNORDERED                    // Characteristics
        );
    }
}
```

---

## 12. Stream Gatherers (Modern Java Enhancement - JEP 461/473/485)

Stream Gatherers extend the Intermediate Operations capabilities of streams, allowing custom windowing, stateful transformation, and folding before terminal collection:

```java
// Fixed-size windowing: transforms Stream<T> into Stream<List<T>>
List<Integer> numbers = List.of(1, 2, 3, 4, 5, 6, 7);

// Using built-in Gatherers:
// Stream<List<Integer>> windows = numbers.stream()
//         .gather(Gatherers.windowFixed(3))
//         .toList();
// Result: [[1, 2, 3], [4, 5, 6], [7]]
```

---

## 13. Spliterator Architecture & Characteristics

Every Stream source implements `java.util.Spliterator<T>` to partition and traverse elements:
```
           +---------------------------------------------+
           |               Root Spliterator              |
           |             [Elements 0 to 10,000]          |
           +---------------------------------------------+
                                  |
                           trySplit()
                   +--------------+--------------+
                   |                             |
                   v                             v
+-----------------------------+   +-----------------------------+
|     Child Spliterator 1     |   |     Child Spliterator 2     |
|     [Elements 0 to 5,000]   |   |   [Elements 5,001 to 10,000]|
+-----------------------------+   +-----------------------------+
```

### Spliterator Characteristics Flags
| Characteristic | Integer Mask | Meaning |
| :--- | :---: | :--- |
| `ORDERED` | `0x00000010` | Encounter order is defined (e.g., `List`) |
| `DISTINCT` | `0x00000001` | No two elements are equal (e.g., `Set`) |
| `SORTED` | `0x00000004` | Elements follow defined `Comparator` order |
| `SIZED` | `0x00000040` | Exact element count known before traversal |
| `NONNULL` | `0x00000100` | Source guarantees no `null` elements exist |
| `IMMUTABLE`| `0x00000400` | Element source cannot be structurally modified |
| `CONCURRENT`| `0x00001000`| Thread-safe source; can be concurrently mutated |
| `SUBSIZED` | `0x00004000` | All splits produced by `trySplit()` will also be `SIZED` |

---

## 14. Practical Stream Recipes & Production Idioms

### Recipe 1: Partitioning Numbers into Evens and Odds
```java
Map<Boolean, List<Integer>> evensAndOdds = numbers.stream()
        .collect(Collectors.partitioningBy(n -> n % 2 == 0));
List<Integer> evens = evensAndOdds.get(true);
List<Integer> odds = evensAndOdds.get(false);
```

### Recipe 2: Finding Top-N Elements with Streams
```java
List<Employee> top3Earners = employees.stream()
        .sorted(Comparator.comparingDouble(Employee::getSalary).reversed())
        .limit(3)
        .toList();
```

### Recipe 3: Flattening Nested Structures (One-to-Many Mapping)
```java
List<String> allSkills = developers.stream()
        .flatMap(dev -> dev.skills().stream())
        .distinct()
        .sorted()
        .toList();
```

### Recipe 4: Reading File Lines into Stream with Try-With-Resources
```java
try (Stream<String> lines = Files.lines(Path.of("/var/log/application.log"))) {
    long errorCount = lines.filter(line -> line.contains("ERROR"))
                           .count();
    System.out.println("Total errors: " + errorCount);
}
```

---

## 15. Quick Summary / Cheat Sheet Card

- **Intermediate vs Terminal:** Intermediate operations (`filter`, `map`, `sorted`) are lazy and return a new `Stream`. Terminal operations (`collect`, `count`, `reduce`) trigger execution and consume the stream.
- **Primitive Streams:** Use `IntStream`, `LongStream`, `DoubleStream` to eliminate boxing overhead on numbers.
- **Parallel Rule:** Only use `.parallelStream()` for CPU-heavy tasks on $N > 10,000$ elements with easily split sources (`ArrayList`, arrays).
- **Collector Mastery:** Grouping (`groupingBy`), partitioning (`partitioningBy`), joining (`joining`), and mapping (`toMap`) cover 90% of aggregation pipelines.

---

## 16. Stream vs Loop vs Parallel Benchmark Matrix

| Workload Type | Traditional For-Loop | Sequential Stream | Parallel Stream | Recommended Choice |
| :--- | :---: | :---: | :---: | :--- |
| **Small Array Sum ($N=1,000$)** | ~0.8 µs | ~1.5 µs | ~35.0 µs (thread fork overhead) | **Traditional For-Loop** |
| **Large Array Sum ($N=10,000,000$)** | ~4.2 ms | ~5.1 ms | ~1.1 ms (multi-core scaling) | **Parallel Stream** |
| **Regex Parsing ($N=100,000$)** | ~120 ms | ~122 ms | ~28 ms (CPU-bound) | **Parallel Stream** |
| **Blocking Network I/O Calls** | $O(N)$ serial latency | $O(N)$ serial latency | Starves ForkJoin common pool | **Virtual Threads Executor** |

---

## 17. Primitive Summary Statistics Deep Dive

```java
import java.util.IntSummaryStatistics;
import java.util.stream.IntStream;

public class SummaryStatisticsMastery {
    public static void main(String[] args) {
        IntSummaryStatistics stats = IntStream.of(15, 22, 8, 94, 37, 51, 63)
                .summaryStatistics();

        System.out.println("Count:   " + stats.getCount());
        System.out.println("Sum:     " + stats.getSum());
        System.out.println("Min:     " + stats.getMin());
        System.out.println("Max:     " + stats.getMax());
        System.out.println("Average: " + stats.getAverage());

        // Combining multiple statistics objects (useful for distributed map-reduce)
        IntSummaryStatistics otherStats = IntStream.of(100, 200).summaryStatistics();
        stats.combine(otherStats);
        System.out.println("Combined Count: " + stats.getCount());
        System.out.println("Combined Sum:   " + stats.getSum());
    }
}
```

---

## 18. Stream Debugging with Peek and Custom Loggers

```java
List<String> results = names.stream()
        .filter(n -> n.length() > 3)
        .peek(n -> System.out.println("[DEBUG filter] Passed: " + n))
        .map(String::toUpperCase)
        .peek(n -> System.out.println("[DEBUG map] Upper: " + n))
        .limit(2)
        .toList();
```

---

## 19. Infinite Streams: Stream.iterate vs Stream.generate

```java
import java.util.Random;
import java.util.stream.Stream;

public class InfiniteStreamsReference {
    public static void main(String[] args) {
        // 1. Unbounded generator: Supplier<T>
        Stream<Double> randomStream = Stream.generate(Math::random).limit(5);

        // 2. Unbounded iterator: seed + UnaryOperator<T>
        Stream<Integer> powersOfTwo = Stream.iterate(1, n -> n * 2).limit(10);

        // 3. Finite predicate iterator (Java 9+ for-loop replacement):
        // Stream.iterate(initial, hasNextPredicate, nextFunction)
        Stream<Integer> forLoopStream = Stream.iterate(0, i -> i < 100, i -> i + 10);
        forLoopStream.forEach(n -> System.out.print(n + " ")); // 0 10 20 ... 90
    }
}
```
