# Collectors

A `Collector` describes **how to fold a stream's elements into a result container**: a `List`, a `Map`, a joined `String`, a summary statistic, or anything you design. You pass it to the terminal operation `Stream.collect(...)`.

`Collectors` (plural) is the utility class with ready-made collectors. Most real-world stream code ends in one of them, and `groupingBy` alone replaces a surprising amount of hand-written loop-and-map code.

**Prerequisites:** [Streams Fundamentals](04_streams-fundamentals.md), [Stream Operations](05_stream-operations.md), [Map](../08-collections/03_map.md).

---

## Running example

Every snippet below uses this data:

```java
record Employee(String name, String dept, double salary, int age) {}

List<Employee> staff = List.of(
    new Employee("Asha",  "ENG",   120_000, 31),
    new Employee("Ravi",  "ENG",   95_000,  26),
    new Employee("Meera", "HR",    70_000,  41),
    new Employee("John",  "SALES", 65_000,  35),
    new Employee("Kiran", "SALES", 80_000,  29)
);
```

---

## 1. Collecting into collections

```java
List<String> names = staff.stream().map(Employee::name).collect(Collectors.toList());
Set<String>  depts = staff.stream().map(Employee::dept).collect(Collectors.toSet());

// You choose the concrete type
TreeSet<String> sorted = staff.stream()
        .map(Employee::name)
        .collect(Collectors.toCollection(TreeSet::new));
```

### `Collectors.toList()` vs `Stream.toList()` vs `toUnmodifiableList()`

| | Mutable? | Allows `null` elements? |
|---|---|---|
| `collect(Collectors.toList())` | Unspecified (currently `ArrayList`, but don't rely on it) | Yes |
| `stream.toList()` (Java 16+) | No, unmodifiable | Yes |
| `collect(Collectors.toUnmodifiableList())` (Java 10+) | No | **No**, throws `NullPointerException` |

If you need a list you will modify later, don't depend on `Collectors.toList()` being mutable. Use `toCollection(ArrayList::new)` to make the intent explicit.

---

## 2. Joining and simple reductions

```java
String csv = staff.stream().map(Employee::name)
        .collect(Collectors.joining(", ", "[", "]"));   // [Asha, Ravi, Meera, John, Kiran]

long count          = staff.stream().collect(Collectors.counting());          // Long
double totalSalary  = staff.stream().collect(Collectors.summingDouble(Employee::salary));
double avgAge       = staff.stream().collect(Collectors.averagingInt(Employee::age)); // Double

Optional<Employee> top = staff.stream()
        .collect(Collectors.maxBy(Comparator.comparingDouble(Employee::salary)));

IntSummaryStatistics stats = staff.stream()
        .collect(Collectors.summarizingInt(Employee::age));  // count, sum, min, max, average
```

Notes:

- `joining` only works on `CharSequence` streams: `map` to strings first.
- For a plain count or sum, `stream.count()` or `mapToInt(...).sum()` is shorter. These collectors earn their keep as **downstream collectors** (section 4).
- `averagingX` returns `0.0` for an empty stream; `maxBy`/`minBy` return an empty `Optional`.
- `summingInt` overflows silently like normal `int` arithmetic. Use `summingLong` when totals can be large.

---

## 3. `toMap`

```java
Map<String, Double> salaryByName = staff.stream()
        .collect(Collectors.toMap(Employee::name, Employee::salary));
```

Three overloads, three levels of control:

```java
// 1. (keyFn, valueFn): duplicate keys throw IllegalStateException
// 2. (keyFn, valueFn, mergeFn): resolve duplicates
// 3. (keyFn, valueFn, mergeFn, mapFactory): choose the Map type

Map<String, Double> totalByDept = staff.stream()
        .collect(Collectors.toMap(
                Employee::dept,
                Employee::salary,
                Double::sum,                 // merge on key collision
                LinkedHashMap::new));        // keep encounter order
```

Gotchas:

- **Duplicate keys throw** `IllegalStateException: Duplicate key X (attempted merging values A and B)` unless you pass a merge function. This is the most common `toMap` bug in production, because test data rarely has duplicates.
- **`null` values throw `NullPointerException`**: `toMap` uses `Map.merge` internally, which rejects `null` values. Filter first or use `groupingBy`.
- The default map is a `HashMap` with no ordering guarantee.

---

## 4. `groupingBy`: the one to master

`groupingBy` is SQL's `GROUP BY`. It takes a **classifier** (stream element → key) and optionally a **downstream collector** that decides what to do with each group.

```text
elements ──classifier──► key ──► [ group ] ──downstream collector──► value
```

```java
// Default downstream = toList()
Map<String, List<Employee>> byDept = staff.stream()
        .collect(Collectors.groupingBy(Employee::dept));

// Count per group
Map<String, Long> headcount = staff.stream()
        .collect(Collectors.groupingBy(Employee::dept, Collectors.counting()));

// Average per group
Map<String, Double> avgSalary = staff.stream()
        .collect(Collectors.groupingBy(Employee::dept,
                 Collectors.averagingDouble(Employee::salary)));

// Different map type: sorted by key
Map<String, Long> sortedHeadcount = staff.stream()
        .collect(Collectors.groupingBy(Employee::dept, TreeMap::new, Collectors.counting()));
```

### Downstream collectors worth knowing

```java
// mapping: transform each element before collecting it
Map<String, List<String>> namesByDept = staff.stream()
        .collect(Collectors.groupingBy(Employee::dept,
                 Collectors.mapping(Employee::name, Collectors.toList())));

// filtering (Java 9+): filter inside each group
Map<String, List<Employee>> seniorByDept = staff.stream()
        .collect(Collectors.groupingBy(Employee::dept,
                 Collectors.filtering(e -> e.age() > 30, Collectors.toList())));

// flatMapping (Java 9+): flatten nested collections inside each group
// Collectors.flatMapping(e -> e.skills().stream(), Collectors.toSet())

// collectingAndThen: post-process the downstream result
Map<String, Employee> topEarnerByDept = staff.stream()
        .collect(Collectors.groupingBy(Employee::dept,
                 Collectors.collectingAndThen(
                     Collectors.maxBy(Comparator.comparingDouble(Employee::salary)),
                     Optional::get)));    // safe: a group always has at least one element

// Multi-level grouping
Map<String, Map<Boolean, List<Employee>>> nested = staff.stream()
        .collect(Collectors.groupingBy(Employee::dept,
                 Collectors.partitioningBy(e -> e.salary() > 90_000)));
```

### `filter()` before grouping vs `filtering()` inside it

```java
// Groups with no matches DISAPPEAR
staff.stream().filter(e -> e.age() > 40)
     .collect(Collectors.groupingBy(Employee::dept));       // {HR=[Meera]}

// Every dept stays as a key, with an empty list where nothing matched
staff.stream()
     .collect(Collectors.groupingBy(Employee::dept,
              Collectors.filtering(e -> e.age() > 40, Collectors.toList())));
// {ENG=[], HR=[Meera], SALES=[]}
```

Pick based on whether empty groups are meaningful for your output.

### Rules

- The classifier **must not return `null`**: `NullPointerException: element cannot be mapped to a null key`. Map nulls to a sentinel (`"UNKNOWN"`) first.
- Default result is a `HashMap`: use the `mapFactory` overload for sorted or insertion-ordered results.
- You don't get an `Optional` per group unless you ask for one (`maxBy`, `minBy`, `reducing` without identity).

---

## 5. `partitioningBy`

A `groupingBy` specialised for a **boolean predicate**. The result always contains **both** keys, even when one side is empty.

```java
Map<Boolean, List<Employee>> split = staff.stream()
        .collect(Collectors.partitioningBy(e -> e.salary() >= 80_000));

split.get(true);    // Asha, Ravi, Kiran
split.get(false);   // Meera, John

Map<Boolean, Long> counts = staff.stream()
        .collect(Collectors.partitioningBy(e -> e.age() < 30, Collectors.counting()));
```

Use it instead of `groupingBy(pred)` when you want guaranteed `true`/`false` entries and don't want to handle a missing key.

---

## 6. `teeing` (Java 12+): two collectors, one pass

When you need two results from a single traversal, `teeing` runs two collectors and merges their results.

```java
record Range(double min, double max) {}

Range salaryRange = staff.stream().collect(Collectors.teeing(
        Collectors.minBy(Comparator.comparingDouble(Employee::salary)),
        Collectors.maxBy(Comparator.comparingDouble(Employee::salary)),
        (min, max) -> new Range(min.get().salary(), max.get().salary())));
```

A stream can only be consumed once, so `teeing` is the clean alternative to collecting into a list and traversing it twice.

---

## 7. How a `Collector` works

A collector is four functions plus a set of characteristics:

```text
supplier      ()           -> A      create a fresh mutable container
accumulator   (A, T)       -> void   fold one element into the container
combiner      (A, A)       -> A      merge two containers (parallel streams)
finisher      (A)          -> R      convert container to final result
characteristics  CONCURRENT, UNORDERED, IDENTITY_FINISH
```

The `supplier` and `accumulator` do the sequential work. The `combiner` is used only when a parallel stream splits the work and has to merge partial results ([Parallel Streams](07_parallel-streams.md)).

### Writing your own

```java
// Average of doubles without boxing into a list first
Collector<Double, double[], Double> average = Collector.of(
        () -> new double[2],                         // [sum, count]
        (acc, x) -> { acc[0] += x; acc[1]++; },      // accumulator
        (a, b) -> { a[0] += b[0]; a[1] += b[1]; return a; },  // combiner
        acc -> acc[1] == 0 ? 0.0 : acc[0] / acc[1]); // finisher

double avg = Stream.of(1.0, 2.0, 6.0).collect(average);   // 3.0
```

Rules for a correct custom collector:

- The **supplier must return a new container every call**. Sharing one breaks parallel streams.
- The **combiner must be consistent with the accumulator**: merging two partials must equal accumulating everything into one.
- Don't declare `CONCURRENT` or `UNORDERED` unless you really satisfy them.

Most of the time you don't need a custom collector: `collectingAndThen`, `teeing`, and `reducing` cover the common cases.

> **Java 24+:** Stream Gatherers (`Stream.gather`) became final in Java 24 (JEP 485). They are *intermediate* custom operations, which fill the gap collectors can't: windowing, scanning, and so on. Collectors remain the tool for *terminal* aggregation.

---

## 8. Concurrent collectors

`groupingByConcurrent` and `toConcurrentMap` accumulate into a shared `ConcurrentMap` from multiple threads. This can reduce merge cost in parallel streams but gives up encounter order inside groups. Only consider them after measuring. See [Parallel Streams](07_parallel-streams.md).

---

## Common mistakes

| Mistake | Fix |
|---|---|
| `toMap` on non-unique keys | Add a merge function, or use `groupingBy` |
| `toMap` with a `null` value | Filter nulls, use `Optional`, or use `groupingBy` |
| Assuming `Collectors.toList()` returns a modifiable `ArrayList` | `toCollection(ArrayList::new)` |
| Assuming `HashMap` result is ordered | Supply `TreeMap::new` / `LinkedHashMap::new` |
| `groupingBy` on a key that can be `null` | Substitute a default key in the classifier |
| Mutating an outer collection inside `forEach` instead of using `collect` | Use `collect`: it is parallel-safe and expresses intent |
| `counting()` result treated as `int` | It returns `Long` |
| Chaining `filter` and `groupingBy` and wondering where empty groups went | Use `filtering` downstream |

### Debugging tips

- `IllegalStateException: Duplicate key`: the message names the key and both values. Check your data for duplicates.
- Unexpected `NullPointerException` inside `collect`: look for a `null` classifier result or `null` map value.
- Type inference fails on deeply nested collectors: assign intermediate collectors to typed local variables, or use `var` plus a helper method.

---

## Quick Summary

- A `Collector` = supplier + accumulator + combiner + finisher. `Collectors` provides the common ones.
- `toMap` fails on duplicate keys and null values unless you handle them.
- `groupingBy(classifier, [mapFactory], downstream)` is the workhorse. Learn `counting`, `mapping`, `filtering`, `collectingAndThen`, and `averagingX`.
- `partitioningBy` always yields both `true` and `false` entries.
- `teeing` (Java 12+) returns two aggregates in one pass.
- Prefer `Stream.toList()` for unmodifiable results, and `toCollection` when you need a specific mutable type.

**Next:** [Parallel Streams](07_parallel-streams.md)
