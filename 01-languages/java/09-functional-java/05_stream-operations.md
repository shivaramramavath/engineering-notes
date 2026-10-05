# Stream Operations

A reference for the operations you chain on a stream: **intermediate** operations (return a new stream, lazy) and **terminal** operations (produce a result and start the pipeline). Pipeline mechanics are in [04_streams-fundamentals.md](./04_streams-fundamentals.md).

```java
record Person(String name, int age, String city) { }

List<Person> people = List.of(
    new Person("Ada", 36, "London"), new Person("Linus", 54, "Helsinki"),
    new Person("Grace", 85, "NYC"), new Person("Alan", 41, "London"));
```

## Intermediate operations

| Operation | Purpose | Stateful? | Short-circuit? |
|-----------|---------|:---------:|:--------------:|
| `filter(Predicate)` | Keep matching elements | No | No |
| `map(Function)` | Transform each element | No | No |
| `mapToInt/Long/Double` | Transform to a primitive stream | No | No |
| `mapToObj`, `boxed` | Primitive stream to object stream | No | No |
| `flatMap(Function→Stream)` | Transform each element into a stream and flatten | No | No |
| `mapMulti` (16+) | Emit zero or more outputs per element | No | No |
| `distinct()` | Remove duplicates (by `equals`) | **Yes** | No |
| `sorted()` / `sorted(Comparator)` | Sort | **Yes** | No |
| `peek(Consumer)` | Look at each element (debugging) | No | No |
| `limit(n)` | First `n` elements | Yes* | **Yes** |
| `skip(n)` | Discard the first `n` | Yes* | No |
| `takeWhile(Predicate)` (9+) | Take while the condition holds | Yes* | **Yes** |
| `dropWhile(Predicate)` (9+) | Drop while the condition holds | Yes* | No |
| `parallel()`, `sequential()`, `unordered()`, `onClose()` | Pipeline characteristics | | |

\* order-dependent/"quasi-stateful"; in parallel pipelines these can be expensive ([07](./07_parallel-streams.md)).

### `filter`: keep what matches

```java
people.stream().filter(p -> p.age() >= 40).toList();                          // Linus, Grace, Alan
people.stream().filter(p -> p.city().equals("London")).filter(p -> p.age() > 40);   // chain = AND
Stream.of("a", "", "b").filter(Predicate.not(String::isEmpty)).toList();      // [a, b]
stream.filter(Objects::nonNull);
```

### `map`: transform one-to-one

```java
people.stream().map(Person::name).toList();                   // [Ada, Linus, Grace, Alan]
people.stream().map(p -> p.name().toUpperCase()).toList();
names.stream().map(String::length).toList();                  // Stream<Integer> (boxed)
people.stream().mapToInt(Person::age).sum();                  // IntStream: no boxing
```

### `flatMap`: transform one-to-many and flatten

`map` with a function that returns a stream gives `Stream<Stream<T>>`; `flatMap` merges the inner streams into one.

```java
List<List<Integer>> nested = List.of(List.of(1, 2), List.of(3), List.of(4, 5));
nested.stream().flatMap(List::stream).toList();               // [1, 2, 3, 4, 5]

List<String> sentences = List.of("hello world", "java streams");
sentences.stream()
         .flatMap(s -> Arrays.stream(s.split(" ")))
         .toList();                                           // [hello, world, java, streams]

orders.stream().flatMap(o -> o.items().stream());             // all items of all orders
optionals.stream().flatMap(Optional::stream);                 // drop empties, unwrap present (Java 9)
```

Use it whenever each element maps to **0, 1 or many** results. Never return `null` from the mapper (return `Stream.empty()`).

### `distinct`, `sorted`, `limit`, `skip`

```java
Stream.of(1, 2, 2, 3, 1).distinct().toList();                              // [1, 2, 3]  (uses equals/hashCode)
people.stream().sorted(Comparator.comparingInt(Person::age)).toList();     // by age
people.stream().sorted(Comparator.comparing(Person::name).reversed());
Stream.of("b", "a").sorted().toList();                                     // natural order: elements must be Comparable
people.stream().skip(1).limit(2);                                          // pagination: elements 2 and 3
```

`sorted()` on non-`Comparable` elements throws `ClassCastException`. Sorting is **stable** for ordered streams ([08-collections/05](../08-collections/05_comparable-and-comparator.md)).

### `takeWhile` / `dropWhile` (on **ordered** streams)

```java
Stream.of(1, 2, 3, 10, 4, 5).takeWhile(n -> n < 5).toList();    // [1, 2, 3]       stops at the first failure
Stream.of(1, 2, 3, 10, 4, 5).dropWhile(n -> n < 5).toList();    // [10, 4, 5]      drops the leading run
Stream.of(1, 2, 3, 10, 4, 5).filter(n -> n < 5).toList();       // [1, 2, 3, 4]    filter checks ALL elements
```

### `peek`: debugging only

```java
.peek(x -> System.out.println("processing " + x))       // do not use it to change state or for real logic
```

## Terminal operations

| Operation | Returns | Short-circuit? |
|-----------|---------|:--------------:|
| `forEach(Consumer)` / `forEachOrdered` | `void` | No |
| `collect(Collector)` | Any result ([06](./06_collectors.md)) | No |
| `toList()` (16+) | Unmodifiable `List` | No |
| `toArray()` / `toArray(String[]::new)` | Array | No |
| `reduce(...)` | Combined value / `Optional` | No |
| `count()` | `long` | No |
| `min(cmp)` / `max(cmp)` | `Optional<T>` | No |
| `findFirst()` / `findAny()` | `Optional<T>` | **Yes** |
| `anyMatch` / `allMatch` / `noneMatch` | `boolean` | **Yes** |
| `iterator()` | `Iterator<T>` | |
| `sum()`, `average()`, `summaryStatistics()` (primitive streams) | Numbers / stats | No |

### Matching and finding

```java
people.stream().anyMatch(p -> p.age() > 80);                    // true: stops at the first match
people.stream().allMatch(p -> p.age() > 18);                    // true: stops at the first non-match
people.stream().noneMatch(p -> p.city().isEmpty());

Optional<Person> first = people.stream().filter(p -> p.age() > 40).findFirst();     // Linus
Optional<Person> any = people.stream().filter(p -> p.age() > 40).findAny();         // any match: faster in parallel
```

On an empty stream: `anyMatch` → `false`, `allMatch` → `true`, `noneMatch` → `true`. `findFirst` throws `NullPointerException` if the selected element is `null`.

### Counting and extremes

```java
people.stream().filter(p -> p.city().equals("London")).count();                      // 2
Optional<Person> oldest = people.stream().max(Comparator.comparingInt(Person::age)); // Grace
Optional<Person> youngest = people.stream().min(Comparator.comparingInt(Person::age));
```

### Numeric terminal operations (primitive streams)

```java
IntStream ages = people.stream().mapToInt(Person::age);
ages.sum();                           // 216
OptionalDouble avg = people.stream().mapToInt(Person::age).average();      // OptionalDouble
IntSummaryStatistics s = people.stream().mapToInt(Person::age).summaryStatistics();
s.getMin(); s.getMax(); s.getAverage(); s.getSum(); s.getCount();
OptionalInt max = people.stream().mapToInt(Person::age).max();
```

### `forEach` vs `forEachOrdered`

```java
list.stream().forEach(System.out::println);                 // order not guaranteed in parallel streams
list.parallelStream().forEachOrdered(System.out::println);  // preserves encounter order (slower in parallel)
```

`forEach` is for **side effects at the end** (printing, saving). To produce data, use `collect`/`toList`/`reduce`. `list.forEach(...)` directly on a collection is shorter when you need no pipeline.

### `toList()` vs `collect(Collectors.toList())`

```java
List<String> a = stream.toList();                           // Java 16+: unmodifiable, allows nulls
List<String> b = stream.collect(Collectors.toList());       // a mutable list in practice (not guaranteed)
List<String> c = stream.collect(Collectors.toCollection(ArrayList::new));    // guaranteed ArrayList
```

([08-collections/13](../08-collections/13_immutable-and-unmodifiable-collections.md).)

## `reduce`: combine into one value

Three forms:

```java
// 1. Identity + accumulator: always returns a value
int sum = Stream.of(1, 2, 3).reduce(0, (a, b) -> a + b);           // 6   (or Integer::sum)
String joined = Stream.of("a", "b").reduce("", String::concat);

// 2. Accumulator only: returns Optional (empty stream has no result)
Optional<Integer> max = Stream.of(3, 9, 4).reduce(Integer::max);   // Optional[9]

// 3. Identity + accumulator + combiner: result type differs from element type
int totalLength = Stream.of("a", "bb", "ccc")
        .reduce(0, (acc, s) -> acc + s.length(), Integer::sum);     // 6
```

Rules for correctness (especially in parallel):

| Requirement | Meaning |
|-------------|---------|
| **Identity** | `identity op x == x` for all `x` (`0` for `+`, `1` for `*`, `""` for concat) |
| **Associative** | `(a op b) op c == a op (b op c)` (`+`, `max` yes; `-`, `/` no) |
| **Stateless, non-interfering** | No mutation of shared data |

For accumulating into **mutable containers** (lists, maps, `StringBuilder`) use `collect`, not `reduce` ([06](./06_collectors.md)). Prefer the dedicated numeric terminals (`sum`, `max`) over `reduce` for readability.

## Short-circuiting and laziness together

```java
Optional<String> first = Stream.of("a", "bb", "ccc")
        .peek(s -> System.out.println("visit " + s))
        .filter(s -> s.length() > 1)
        .findFirst();
// visit a, visit bb: stops; "ccc" is never visited
```

`limit`, `findFirst`, `findAny`, `anyMatch`, `allMatch`, `noneMatch` and `takeWhile` can end processing early (and make infinite streams usable).

## Stateful operations and memory

`sorted()` and `distinct()` need to see (and buffer) all the elements they pass on; a pipeline with `sorted()` does **not** stream element-by-element past that point, and on huge or infinite streams it can run out of memory. Place `filter`/`limit` **before** them to shrink the data:

```java
stream.sorted().limit(10);                 // sorts everything, then takes 10
stream.filter(cheapTest).sorted().limit(10);   // better: filter first
```

For "top K" use `sorted().limit(k)` on small data, or a bounded heap for big data ([08-collections/10_priorityqueue.md](../08-collections/10_priorityqueue.md)).

## Order of operations matters for performance

```java
// Expensive map runs on every element, even those later dropped
list.stream().map(this::expensive).filter(x -> x.isValid()).limit(3);

// Cheaper: filter on the cheap field first
list.stream().filter(Item::isActive).map(this::expensive).limit(3);
```

## A realistic pipeline

```java
String report = orders.stream()
        .filter(o -> o.status() == Status.PAID)                         // 1. keep paid orders
        .flatMap(o -> o.lines().stream())                               // 2. all order lines
        .filter(l -> l.quantity() > 0)
        .map(l -> l.unitPrice().multiply(BigDecimal.valueOf(l.quantity())))   // 3. line totals
        .reduce(BigDecimal.ZERO, BigDecimal::add)                       // 4. grand total (BigDecimal: no double for money)
        .toPlainString();
```

Group-by style aggregation uses collectors: [06_collectors.md](./06_collectors.md).

## Exceptions and checked exceptions

Lambdas in a stream cannot throw checked exceptions. Wrap them ([00](./00_lambda-expressions.md#exceptions-in-lambdas)), or loop when each failure needs custom handling:

```java
List<String> contents = paths.stream()
        .map(p -> { try { return Files.readString(p); } catch (IOException e) { throw new UncheckedIOException(e); } })
        .toList();
```

## `mapMulti` (Java 16+)

A lower-overhead alternative to `flatMap` when each element yields few results:

```java
Stream.of(1, 2, 3).<Integer>mapMulti((n, sink) -> { if (n != 2) { sink.accept(n); sink.accept(n * 10); } }).toList();
// [1, 10, 3, 30]
```

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| `map` returning a stream, then expecting a flat result | `Stream<Stream<T>>` / `List<List<T>>` | `flatMap` |
| `flatMap` mapper returns `null` | `NullPointerException` | Return `Stream.empty()` |
| `reduce` with a non-associative operation (`-`, `/`) or wrong identity | Wrong results, especially in parallel | Associative op + true identity |
| `reduce` to build a list/map | O(n²) copying, wrong in parallel | `collect` |
| `sorted()` on non-comparable elements | `ClassCastException` | Provide a `Comparator` |
| `findFirst()` on a stream whose first element is `null` | `NullPointerException` | Avoid `null` elements |
| `peek` for logic | Not executed (e.g., `count()` shortcut) or reordered | `map`/`forEach` |
| Mapping to `int` with `map` and unboxing `null` | `NullPointerException` | `filter(Objects::nonNull)` first |
| Using `forEach` to build a result list | Not thread-safe in parallel, noisy | `toList`/`collect` |
| `Stream<Integer>` sums with `reduce(0, Integer::sum)` in hot code | Boxing | `mapToInt(...).sum()` |
| Sorting before filtering | Wasted work | Filter first |
| Terminal operation twice | `IllegalStateException` | New stream per terminal operation |

## Key takeaways

- Intermediate operations (`filter`, `map`, `flatMap`, `distinct`, `sorted`, `limit`, `skip`) are lazy; terminal operations (`collect`, `toList`, `reduce`, `count`, `forEach`, `findFirst`, `anyMatch`) run the pipeline
- `flatMap` for one-to-many; `reduce` needs an associative operation and a true identity; use `collect` for mutable results
- Short-circuiting operations (`limit`, `findFirst`, `anyMatch`, `takeWhile`) make early exit and infinite streams possible
- Filter early, sort late; use primitive streams for numbers; avoid side effects and `peek` for logic

**Next:** [Collectors](./06_collectors.md)