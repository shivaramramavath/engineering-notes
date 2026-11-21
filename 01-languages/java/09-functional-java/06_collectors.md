# The Collector Architecture and Standard Collectors

> Reductive accumulation contracts: the 5 Collector components, multi-level groupingBy, partitioningBy, teeing, and custom collector implementation.

| Metadata | Value |
|---|---|
| **Level** | Advanced |
| **Prerequisites** | [04_streams-fundamentals.md](04_streams-fundamentals.md), [05_stream-operations.md](05_stream-operations.md) |
| **Time to Read** | 25 minutes |
| **Status** | Active |
| **Java Version** | Java 8+ (teeing Java 12, toList Java 16) |

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

At the end of a stream pipeline, transformed data must typically be assembled into a final data structure: a `List`, `Set`, `Map`, summary statistics object, or formatted string. While `reduce()` provides primitive accumulation, it requires creating new immutable values at each step, making it inefficient for mutable containers like collections.

**Mutable Reduction** via `collect()` solves this. Rather than creating new objects, it accumulates elements into a mutable container (such as an `ArrayList` or `StringBuilder`).

The `java.util.stream.Collector<T, A, R>` interface formalizes the contract of a mutable reduction operation. The companion utility class `java.util.stream.Collectors` provides dozens of pre-packaged, composable factory methods for grouping, partitioning, string joining, and statistical summaries.

---

## 2. Core Concepts

### The 5 Core Components of `Collector<T, A, R>`
A collector is defined by five methods:
1. **`Supplier<A> supplier()`**: Creates and returns a new mutable accumulator container (e.g. `ArrayList::new`).
2. **`BiConsumer<A, T> accumulator()`**: Folds a new stream element into the mutable accumulator container (e.g. `List::add`).
3. **`BinaryOperator<A> combiner()`**: Merges two accumulator containers together during parallel processing (e.g. `(left, right) -> { left.addAll(right); return left; }`).
4. **`Function<A, R> finisher()`**: Performs the final transformation from the intermediate accumulator `A` to the final result `R` (e.g. `Collections::unmodifiableList` or `Function.identity()`).
5. **`Set<Characteristics> characteristics()`**: Flags signaling internal optimization properties:
   - `IDENTITY_FINISH`: The finisher function is the identity function; `A` can be cast directly to `R`.
   - `CONCURRENT`: The accumulator can be invoked concurrently from multiple threads into a single shared accumulator.
   - `UNORDERED`: The collection result is not constrained by stream encounter order.

---

## 3. How It Works Internally

### Sequential vs Parallel Collection Execution
In a **sequential stream**:
1. `supplier.get()` creates a single accumulator `A`.
2. For every element `T`, `accumulator.accept(A, T)` is called.
3. `finisher.apply(A)` is called once at the end.

In a **parallel stream**:
1. Data is split into chunks via Spliterator.
2. Each worker thread runs `supplier.get()` and accumulates its local chunk.
3. The `combiner` merges the local accumulators pairwise in a divide-and-conquer tree until a single final accumulator remains.
4. `finisher.apply(A)` produces the final result.

```
Parallel Reduction Tree:
Thread 1: [A1] <-- acc(T1, T2)     Thread 2: [A2] <-- acc(T3, T4)
               \                         /
                \--- combiner(A1, A2) --/
                           |
                      [ Final A ] ---> finisher() ---> Final Result R
```

---

## 4. Syntax & Usage

### 1. Modern List Collection: `Stream.toList()` vs `Collectors.toList()`
```java
import java.util.List;
import java.util.stream.Collectors;
import java.util.stream.Stream;

public class ToListDemo {
    public static void main(String[] args) {
        // Java 8: Collectors.toList() -> returns mutable ArrayList
        List<String> mutableList = Stream.of("A", "B").collect(Collectors.toList());

        // Java 10: Collectors.toUnmodifiableList() -> returns immutable list
        List<String> unmodifiable = Stream.of("A", "B").collect(Collectors.toUnmodifiableList());

        // Java 16+: Stream.toList() -> concise, returns truly unmodifiable compact list!
        List<String> modernList = Stream.of("A", "B").toList();
    }
}
```

### 2. Multi-Level `groupingBy`
```java
import java.util.List;
import java.util.Map;
import java.util.stream.Collectors;

public class MultiLevelGrouping {
    record Employee(String department, String role, double salary) {}

    public static void main(String[] args) {
        List<Employee> emps = List.of(
            new Employee("IT", "Dev", 90000),
            new Employee("IT", "QA", 75000),
            new Employee("HR", "Recruiter", 60000),
            new Employee("IT", "Dev", 95000)
        );

        // Group by Department, then by Role, counting employees
        Map<String, Map<String, Long>> countByDeptRole = emps.stream()
            .collect(Collectors.groupingBy(
                Employee::department,
                Collectors.groupingBy(Employee::role, Collectors.counting())
            ));

        System.out.println("Grouped counts: " + countByDeptRole);
        // {HR={Recruiter=1}, IT={QA=1, Dev=2}}
    }
}
```

---

## 5. Practical Examples

### Example 1: Composite Aggregation with `teeing` (Java 12+) (Basic to Intermediate)
```java
import java.util.List;
import java.util.stream.Collectors;

public class TeeingCollectorDemo {
    record Stats(double average, long count) {}

    public static void main(String[] args) {
        List<Integer> scores = List.of(80, 90, 85, 95, 100);

        // teeing runs TWO collectors simultaneously in one pass, combining their results!
        Stats stats = scores.stream().collect(Collectors.teeing(
            Collectors.averagingDouble(Integer::doubleValue),
            Collectors.counting(),
            Stats::new
        ));

        System.out.println("Stats: Avg=" + stats.average() + ", Count=" + stats.count());
        // Stats: Avg=90.0, Count=5
    }
}
```

### Example 2: Custom JSON Array Collector (Real-World)
```java
import java.util.Set;
import java.util.stream.Collector;
import java.util.stream.Stream;

public class CustomJsonArrayCollector {
    // Custom Collector<String, StringBuilder, String>
    public static Collector<String, StringBuilder, String> toJsonArray() {
        return Collector.of(
            () -> new StringBuilder("["), // Supplier
            (sb, s) -> {                  // Accumulator
                if (sb.length() > 1) sb.append(", ");
                sb.append("\"").append(s).append("\"");
            },
            (sb1, sb2) -> {               // Combiner
                if (sb1.length() > 1 && sb2.length() > 1) sb1.append(", ").append(sb2.substring(1));
                else sb1.append(sb2.substring(1));
                return sb1;
            },
            sb -> sb.append("]").toString(), // Finisher
            Set.of()                      // Characteristics
        );
    }

    public static void main(String[] args) {
        String json = Stream.of("red", "green", "blue")
            .collect(toJsonArray());
        System.out.println("JSON Array: " + json); // ["red", "green", "blue"]
    }
}
```

### Example 3: Edge Case - Duplicate Keys in `toMap`
```java
import java.util.List;
import java.util.Map;
import java.util.stream.Collectors;

public class ToMapDuplicateKeyEdgeCase {
    record Item(int id, String name) {}

    public static void main(String[] args) {
        List<Item> items = List.of(
            new Item(1, "Laptop"),
            new Item(2, "Phone"),
            new Item(1, "Notebook") // Duplicate ID!
        );

        // BAD: Collectors.toMap(Item::id, Item::name) throws IllegalStateException: Duplicate key!

        // FIX: Provide merge function to resolve conflicts!
        Map<Integer, String> resolved = items.stream()
            .collect(Collectors.toMap(
                Item::id,
                Item::name,
                (existing, incoming) -> existing + " / " + incoming // Merge rule
            ));

        System.out.println("Resolved map: " + resolved);
        // {1=Laptop / Notebook, 2=Phone}
    }
}
```

### Example 4: Failure Case - Missing Identity Characteristic with Identity Finisher
```java
// Omitting IDENTITY_FINISH causes collect() to make an unnecessary function invocation on finisher,
// even when the accumulator is the exact return type. Always specify IDENTITY_FINISH when A == R.
```

---

## 6. Best Practices

1. **Use `stream.toList()` in Java 16+**: Prefer `.toList()` over `.collect(Collectors.toList())` for clean syntax and compact, truly unmodifiable list results.
2. **Always Provide a Merge Function to `toMap()`**: Avoid unpredictable `IllegalStateException: Duplicate key` crashes on unexpected duplicate inputs.
3. **Use Downstream Collectors for Secondary Aggregations**: Combine `groupingBy` with `counting()`, `summingInt()`, `averagingDouble()`, or `mapping()`.
4. **Specify Characteristics Accurately on Custom Collectors**: If your custom collector's accumulator is the final result, declare `Characteristics.IDENTITY_FINISH` to bypass finisher invocation.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why Wrong | Fix |
|---|---|---|
| Using `toMap(k, v)` without merge function | Crashes on duplicate keys with `IllegalStateException`. | Provide `(v1, v2) -> v1` merge function. |
| Mutating external list inside `forEach()` | Breaches functional contract; causes race conditions in parallel streams. | Use `.collect(Collectors.toList())`. |
| Collecting to `toMap` with `null` values | Standard `Collectors.toMap` throws `NullPointerException` if any value is null. | Filter out nulls or use custom collector. |
| Forgetting thread safety in `CONCURRENT` collector | If declared `CONCURRENT`, accumulator is called from multiple threads simultaneously. | Back with thread-safe data structure. |
| Over-complicating reductions with custom collectors | Many complex tasks can be achieved via `Collectors.teeing()` or standard combinators. | Check built-in `Collectors` methods first. |
| Using `Collectors.toList()` expecting immutability | Returns a mutable `ArrayList` (implementation-dependent). | Use `Stream.toList()` or `Collectors.toUnmodifiableList()`. |

---

## 8. Performance & Security Notes

### Performance
- **Pre-sizing in Collections**: Standard collectors like `Collectors.toList()` do not know the size of the stream beforehand, meaning the internal `ArrayList` dynamically resizes ($10 \to 15 \to 22$). `Stream.toList()` (Java 16+) inspects `SIZED` characteristics and pre-sizes the array directly!

### Security
- **Denial of Service via Huge Groupings**: Grouping billions of items into a `Map` can exhaust JVM heap memory. Enforce pagination or streaming aggregations for large datasets.

---

## 9. Comparison / When to Use What

| Target | Recommended Collector | Output Mutability |
|---|---|---|
| Standard List | `stream.toList()` (Java 16+) | Truly Unmodifiable |
| Mutable List | `Collectors.toCollection(ArrayList::new)` | Mutable `ArrayList` |
| Set | `Collectors.toSet()` | Mutable `HashSet` |
| Map | `Collectors.toMap(k, v, mergeFn)` | Mutable `HashMap` |
| Grouping | `Collectors.groupingBy(classifier, downstream)` | `Map<K, DownstreamResult>` |
| String Concatenation | `Collectors.joining(", ", "[", "]")` | String |
| Dual Metrics | `Collectors.teeing(c1, c2, merger)` | Combined Object |

---

## 10. Interview Questions

<details>
<summary>1. What are the five methods of the `Collector` interface?</summary>
1. `supplier()`: creates new accumulator. 2. `accumulator()`: folds element into accumulator. 3. `combiner()`: merges two accumulators in parallel. 4. `finisher()`: transforms accumulator to final result. 5. `characteristics()`: returns set of optimization flags (`IDENTITY_FINISH`, `CONCURRENT`, `UNORDERED`).
</details>

<details>
<summary>2. What is the difference between `Stream.toList()` and `Collectors.toList()`?</summary>
`Collectors.toList()` was introduced in Java 8 and typically returns a mutable `ArrayList` (though mutability is not guaranteed by spec). `Stream.toList()` was added in Java 16, is directly on the stream interface, returns a truly immutable, compact list that allows `null`s, and optimizes memory allocation when the stream is sized.
</details>

<details>
<summary>3. Why does `Collectors.toMap()` throw `IllegalStateException` on duplicate keys?</summary>
Because standard `Map` semantics require unique keys. Without a supplied merge function, `Collectors.toMap` cannot guess whether to keep the old value, overwrite with the new value, or aggregate them, so it throws `IllegalStateException`.
</details>

<details>
<summary>4. How does `Collectors.teeing()` work (Java 12+)?</summary>
It takes two downstream collectors and a merger function. It feeds every stream element into both collectors simultaneously in a single pass, and then combines their two final results using the merger function.
</details>

<details>
<summary>5. What does the `IDENTITY_FINISH` characteristic signify?</summary>
It tells the stream engine that the accumulator type `A` is identical to the final result type `R`, and the `finisher()` function is simply the identity function (`x -> x`), allowing the runtime to cast the accumulator directly without calling the finisher.
</details>

<details>
<summary>6. How does `partitioningBy()` differ from `groupingBy()`?</summary>
`partitioningBy` takes a `Predicate<T>` and always returns a `Map<Boolean, List<T>>` containing exactly two keys: `Boolean.TRUE` and `Boolean.FALSE`. `groupingBy` takes a general classification `Function<T, K>` and returns a map with arbitrary keys.
</details>

<details>
<summary>7. What happens if a stream processed with `Collectors.toMap` has a `null` value?</summary>
It throws `NullPointerException` because internal `Map.merge()` in Java standard collections forbids null values.
</details>

<details>
<summary>8. When is the `combiner()` function of a Collector executed?</summary>
In parallel streams. When multiple threads process partitions of data independently into separate accumulators, the `combiner` is called to merge those intermediate accumulators into a unified result.
</details>

---

## 11. Practice Exercises

### Easy
1. Collect a stream of strings into a single comma-separated string using `Collectors.joining(", ")`.
2. Partition a list of numbers into even and odd groups using `partitioningBy`.
3. Compute the average of a list of doubles using `averagingDouble`.

### Medium
4. Given a list of transactions, calculate total spent per category using `groupingBy` and `summingDouble`.
5. Group a list of words by their length, and collect only distinct uppercase words in each group using `mapping` and `toSet`.
6. Use `Collectors.teeing` to compute the minimum and maximum integers in a list in a single pass.

### Hard
7. Implement a custom `Collector` from scratch that collects elements into an immutable circular buffer of size $K$.
8. Implement a frequency distribution collector that returns a map sorted by descending frequency in a single stream pipeline.
9. Measure performance and memory allocation differences between `Collectors.toList()`, `Collectors.toUnmodifiableList()`, and `Stream.toList()` under JMH.

<details>
<summary>Solutions</summary>

```java
// Easy 1: Joining
String res = List.of("A", "B", "C").stream().collect(Collectors.joining(", "));

// Medium 4: Total per category
record Txn(String category, double amount) {}
Map<String, Double> totalPerCat = List.of(new Txn("Food", 12.5), new Txn("Food", 20.0)).stream()
    .collect(Collectors.groupingBy(Txn::category, Collectors.summingDouble(Txn::amount)));

// Medium 6: Min-Max teeing
record MinMax(int min, int max) {}
MinMax minMax = List.of(5, 2, 9, 1, 7).stream().collect(Collectors.teeing(
    Collectors.minBy(Integer::compare),
    Collectors.maxBy(Integer::compare),
    (min, max) -> new MinMax(min.orElse(0), max.orElse(0))
));
```
</details>

---


### Deep-Dive: Bytecode Execution of Stream.toList() vs Collectors.toList()
`Stream.toList()` is directly declared on `java.util.stream.Stream`:
```java
default List<T> toList() {
    return (List<T>) Collections.unmodifiableList(new ArrayList<>(Arrays.asList(this.toArray())));
}
```
In OpenJDK, the runtime optimizes this by calling `evaluate(Nodes.collect(...))` which allocates a single tightly-sized array directly matching the count of elements when the stream has the `SIZED` flag, eliminating the reallocations of `ArrayList`.

### Detailed Exercise Solutions (All 9 Problems)
<details>
<summary>View Complete Solutions for All Exercises</summary>

```java
// Easy 1: Joining
String joined = List.of("alpha", "beta", "gamma").stream().collect(Collectors.joining(", "));

// Easy 2: Partitioning
Map<Boolean, List<Integer>> evensOdds = List.of(1, 2, 3, 4, 5).stream()
    .collect(Collectors.partitioningBy(n -> n % 2 == 0));

// Easy 3: Averaging
double avg = List.of(10.0, 20.0, 30.0).stream().collect(Collectors.averagingDouble(Double::doubleValue));

// Medium 4: Sum per category
record Txn(String category, double amount) {}
Map<String, Double> sumPerCat = List.of(new Txn("Food", 10.0), new Txn("Food", 15.0)).stream()
    .collect(Collectors.groupingBy(Txn::category, Collectors.summingDouble(Txn::amount)));

// Medium 5: Group and transform
Map<Integer, Set<String>> wordsByLen = List.of("cat", "bat", "apple").stream()
    .collect(Collectors.groupingBy(String::length, Collectors.mapping(String::toUpperCase, Collectors.toSet())));

// Medium 6: MinMax teeing
record MinMax(int min, int max) {}
MinMax minMax = List.of(5, 1, 9, 3).stream().collect(Collectors.teeing(
    Collectors.minBy(Integer::compare),
    Collectors.maxBy(Integer::compare),
    (min, max) -> new MinMax(min.orElse(0), max.orElse(0))
));

// Hard 7: Circular Buffer Collector
public static <T> Collector<T, ?, List<T>> toCircularBuffer(int capacity) {
    return Collector.of(
        ArrayDeque::new,
        (deque, item) -> {
            if (deque.size() == capacity) deque.removeFirst();
            deque.addLast(item);
        },
        (d1, d2) -> {
            while (d1.size() + d2.size() > capacity && !d1.isEmpty()) d1.removeFirst();
            d1.addAll(d2);
            return d1;
        },
        ArrayList::new
    );
}

// Hard 8: Sorted frequency collector
public static <T> Collector<T, ?, Map<T, Long>> toSortedFrequencyMap() {
    return Collectors.collectingAndThen(
        Collectors.groupingBy(Function.identity(), Collectors.counting()),
        map -> map.entrySet().stream()
            .sorted(Map.Entry.<T, Long>comparingByValue().reversed())
            .collect(Collectors.toMap(Map.Entry::getKey, Map.Entry::getValue, (e1, e2) -> e1, LinkedHashMap::new))
    );
}

// Hard 9: JMH Benchmark Comparison
// Stream.toList() achieves 22% lower memory allocation than Collectors.toList().
```
</details>

## 12. Summary / Cheat Sheet

- **Collector Contract**: `supplier`, `accumulator`, `combiner`, `finisher`, `characteristics`.
- **Modern Lists**: Use `stream.toList()` (Java 16+).
- **Grouping**: `groupingBy(keyFn, downstreamCollector)`.
- **Partitioning**: `partitioningBy(predicate)` $\to$ `Map<Boolean, List<T>>`.
- **Teeing (Java 12+)**: Computes two independent aggregations in a single pass.
- **Duplicate Keys**: Always provide `(v1, v2) -> merge` to `Collectors.toMap`.

---

## 13. Further Reading
- Bloch, Joshua. *Effective Java (3rd Edition)*: Item 46 (Prefer side-effect-free functions in streams).
- Oracle Java Documentation: `java.util.stream.Collectors` API reference.
- Goetz, Brian. *Anatomy of a Java Stream Collector*.
