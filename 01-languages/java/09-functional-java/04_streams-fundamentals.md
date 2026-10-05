# Streams Fundamentals

A **stream** is a sequence of elements supporting **aggregate operations** that you describe declaratively, as a **pipeline**. A stream is **not a data structure**: it stores nothing, never modifies its source, and can be consumed **once**.

```java
List<String> result = names.stream()              // 1. source
        .filter(n -> n.length() > 3)              // 2. intermediate operations (lazy)
        .map(String::toUpperCase)
        .sorted()
        .toList();                                // 3. terminal operation (triggers everything)
```

```
 source ──► filter ──► map ──► sorted ──► terminal (toList / forEach / count / ...)
 collection  intermediate operations (return a new Stream)     produces a result or side effect
             nothing runs here ...                              ... everything runs here
```

## The three parts of a pipeline

| Part | Role | Examples |
|------|------|----------|
| **Source** | Where elements come from | `list.stream()`, `Stream.of(...)`, `Files.lines(path)`, `IntStream.range(0, 10)` |
| **Intermediate operations** (zero or more) | Transform the stream; return a **new `Stream`**; **lazy** | `filter`, `map`, `flatMap`, `sorted`, `distinct`, `limit`, `peek` |
| **Terminal operation** (exactly one) | Starts processing; produces a result or side effect; **ends** the stream | `collect`, `toList`, `forEach`, `reduce`, `count`, `findFirst`, `anyMatch` |

Operations are covered in [05_stream-operations.md](./05_stream-operations.md) and collectors in [06_collectors.md](./06_collectors.md).

## Creating streams

```java
// From collections and arrays
List.of(1, 2, 3).stream();
set.stream();   map.keySet().stream();   map.entrySet().stream();   map.values().stream();
Arrays.stream(array);                         // whole array
Arrays.stream(array, 1, 4);                   // a range

// From values
Stream.of("a", "b", "c");
Stream.empty();
Stream.ofNullable(maybeNull);                 // 0 or 1 element (Java 9)

// Infinite / generated
Stream.iterate(1, x -> x * 2);                // 1, 2, 4, 8, ... (infinite)
Stream.iterate(1, x -> x < 100, x -> x * 2);  // with a stop condition (Java 9)
Stream.generate(() -> "x");                   // infinite from a Supplier
Stream.generate(Math::random).limit(5);

// Primitive streams
IntStream.range(0, 5);                        // 0, 1, 2, 3, 4
IntStream.rangeClosed(1, 5);                  // 1, 2, 3, 4, 5
IntStream.of(3, 1, 2);
"hello".chars();                              // IntStream of UTF-16 code units
new Random().ints(5, 0, 100);                 // 5 random ints in [0, 100)

// I/O and text
try (Stream<String> lines = Files.lines(path)) { ... }      // must be closed
Pattern.compile(",").splitAsStream("a,b,c");
text.lines();                                  // Stream<String> (Java 11)

// Builder
Stream.<String>builder().add("a").add("b").build();
Stream.concat(streamA, streamB);
```

## Key properties

### 1. Streams do not store or modify the source

```java
List<Integer> source = List.of(3, 1, 2);
List<Integer> sorted = source.stream().sorted().toList();     // source is unchanged: [3, 1, 2]
```

Operations produce **new** values. Avoid modifying the source while the stream runs ([12-iterators](../08-collections/12_iterators-and-fail-fast-behavior.md)).

### 2. Streams are lazy

Intermediate operations only **describe** work. Nothing happens until a terminal operation runs:

```java
Stream<String> s = names.stream()
        .filter(n -> { System.out.println("filter " + n); return n.length() > 3; });
// Nothing printed yet: no terminal operation.
s.toList();                                                    // now the filter runs
```

### 3. Elements flow one at a time ("vertical" processing)

Each element travels through the **whole pipeline** before the next one starts, rather than each stage processing the entire collection in turn:

```java
Stream.of("a", "b", "c")
      .filter(s -> { System.out.println("filter " + s); return true; })
      .map(s -> { System.out.println("map " + s); return s.toUpperCase(); })
      .forEach(s -> System.out.println("forEach " + s));
```

```
filter a → map a → forEach A
filter b → map b → forEach B
filter c → map c → forEach C
```

This is why streams can **short-circuit** and why `limit` works on infinite streams:

```java
Stream.iterate(1, x -> x + 1)
      .filter(x -> x % 7 == 0)
      .limit(3)
      .toList();                         // [7, 14, 21]: stops after finding three; never builds an infinite list
```

Exceptions: **stateful** operations such as `sorted()` and `distinct()` must see (some or all) elements first, so they buffer.

### 4. Streams are single-use

```java
Stream<String> s = names.stream();
s.forEach(System.out::println);
s.forEach(System.out::println);          // IllegalStateException: stream has already been operated upon or closed
```

Create a new stream from the source each time, or keep a `Supplier<Stream<T>>`:

```java
Supplier<Stream<String>> supplier = () -> names.stream();
supplier.get().count();
supplier.get().findFirst();
```

### 5. Operations should be non-interfering and stateless

| Rule | Reason |
|------|--------|
| **Non-interfering**: do not modify the source during the pipeline | Undefined behavior, `ConcurrentModificationException` |
| **Stateless** lambdas: do not depend on mutable external state | Reproducible, safe for parallel ([07](./07_parallel-streams.md)) |
| **No side effects** in `map`/`filter` | The runtime may skip, reorder or repeat evaluation (laziness, optimizations, parallelism) |

```java
List<Integer> out = new ArrayList<>();
stream.forEach(x -> out.add(x * 2));                          // works sequentially, but wrong style and unsafe in parallel

List<Integer> out2 = stream.map(x -> x * 2).toList();         // correct: use the pipeline's result
```

## Encounter order

Streams from ordered sources (`List`, arrays, `LinkedHashSet`, `TreeSet`, `IntStream.range`) have an **encounter order**, and operations respect it (`findFirst`, `limit`, `skip`, `forEachOrdered`, `toList`). Streams from unordered sources (`HashSet`, `HashMap`) have none, and `sorted()` imposes one. Ordering constraints can reduce parallel speed ([07](./07_parallel-streams.md)).

## Primitive streams

`Stream<Integer>` boxes every number. Use `IntStream`, `LongStream` and `DoubleStream` for numeric work: no boxing, plus numeric terminal operations.

```java
int sum = IntStream.rangeClosed(1, 100).sum();                  // 5050
double avg = people.stream().mapToInt(Person::age).average().orElse(0);
IntSummaryStatistics stats = IntStream.of(3, 1, 2).summaryStatistics();   // count, sum, min, max, average
stats.getMax();

List<Integer> boxed = IntStream.range(0, 5).boxed().toList();   // IntStream → Stream<Integer>
int[] array = list.stream().mapToInt(Integer::intValue).toArray();
```

| Conversion | Method |
|------------|--------|
| `Stream<T>` → `IntStream` | `mapToInt(ToIntFunction)` (also `mapToLong`, `mapToDouble`) |
| `IntStream` → `Stream<Integer>` | `boxed()` |
| `IntStream` → `Stream<R>` | `mapToObj(IntFunction)` |
| `IntStream` → `LongStream`/`DoubleStream` | `asLongStream()`, `asDoubleStream()` |

## Loops vs streams

```java
// Index loop
for (int i = 0; i < 5; i++) System.out.println(i);
// Stream
IntStream.range(0, 5).forEach(System.out::println);
```

| Streams shine | Loops are better |
|---------------|------------------|
| Filter / map / group / aggregate pipelines | Need an index, `break`, `continue`, or early `return` mid-logic |
| Declarative, composable transformations | Modifying local state step by step |
| Large data with easy parallelization | Checked exceptions in the body |
| Readability of "what, not how" | Simple traversals where a stream adds noise |
| Immutable, side-effect-free processing | Complex control flow, nested conditions |

Do not force streams everywhere: a plain loop is clearer for simple, stateful or I/O-heavy code. Streams are slightly slower than loops for tiny workloads (setup overhead) and fine for typical code ([20-performance](../20-performance/README.md)).

## Closing streams

Streams backed by I/O resources (`Files.lines`, `Files.list`, `Files.walk`, `BufferedReader.lines()`) must be closed; use try-with-resources ([06-exceptions-and-debugging/03_try-with-resources.md](../06-exceptions-and-debugging/03_try-with-resources.md)):

```java
try (Stream<String> lines = Files.lines(path)) {
    long errors = lines.filter(l -> l.contains("ERROR")).count();
}
```

Streams from collections and arrays need no closing.

## Debugging pipelines

```java
List<String> result = names.stream()
        .filter(n -> n.length() > 3)
        .peek(n -> System.out.println("after filter: " + n))      // temporary: inspect the flow
        .map(String::toUpperCase)
        .peek(n -> System.out.println("after map: " + n))
        .toList();
```

`peek` is for **debugging only**: it may not run at all if the terminal operation can answer without it (for example, `count()` on a sized source since Java 9). Alternatively, set a breakpoint inside a lambda, or split the pipeline into named variables/methods ([stack traces and debugging](../06-exceptions-and-debugging/06_stack-traces-and-debugging.md)).

Exceptions inside lambdas surface from the **terminal** operation, with stack traces full of stream-internal frames; keep lambdas small so the culprit is obvious.

## Stream vs `Collection` vs `Iterator`

| | Collection | Iterator | Stream |
|---|-----------|----------|--------|
| Holds data | Yes | No (cursor) | No |
| Reusable | Yes | Single traversal | **Single use** |
| Evaluation | Eager | n/a | **Lazy** |
| Can be infinite | No | Possible | **Yes** |
| Parallelizable | No | No | **Yes** (`parallel()`) |
| Style | Imperative | Imperative | **Declarative** |

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Forgetting the terminal operation | Nothing happens | End with `toList()`, `collect`, `forEach`, ... |
| Reusing a stream | `IllegalStateException: stream has already been operated upon or closed` | Create a new stream, or use a `Supplier` |
| Modifying the source collection in the pipeline | `ConcurrentModificationException`, wrong results | Collect into a new collection |
| Side effects in `map`/`filter` | Order- and parallel-dependent bugs | Pure functions; use the pipeline result |
| Using `peek` for real logic | Skipped or reordered | `map`/`forEach` |
| Not closing `Files.lines`/`walk` | File handle leaks | try-with-resources |
| `Stream<Integer>` for heavy numeric work | Boxing overhead | `IntStream` etc. |
| Infinite stream without `limit`/short-circuit | Hangs or `OutOfMemoryError` | `limit`, `takeWhile`, `findFirst` |
| Expecting a stream to be a collection | No `size()`, no `get(i)` | `collect`, `count()` |
| Streams for trivial loops with `break`/index logic | More complex than a loop | Use the loop |
| Debugging by assuming stages run in sequence | Interleaved output surprises | Remember vertical, one-element-at-a-time flow |

## Key takeaways

- A stream is a **lazy, single-use pipeline**: source → intermediate operations → one terminal operation
- Nothing executes until the terminal operation; elements flow one at a time, enabling short-circuiting and infinite streams
- Streams never modify the source; lambdas should be **stateless and side-effect free**
- Use `IntStream`/`LongStream`/`DoubleStream` for numbers
- Close I/O-backed streams; prefer loops when the logic is stateful or needs `break`/index

**Next:** [Stream Operations](./05_stream-operations.md)