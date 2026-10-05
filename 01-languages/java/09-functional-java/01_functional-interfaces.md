# Functional Interfaces

A **functional interface** has **exactly one abstract method** (a *single abstract method*, SAM). Lambdas and method references can implement it. The package `java.util.function` supplies about 40 ready-made ones, so you almost never need to define your own.

```java
@FunctionalInterface
public interface Greeter {
    String greet(String name);                          // the one abstract method
}

Greeter g = name -> "Hello, " + name;
g.greet("Ada");                                         // "Hello, Ada"
```

## What counts as functional

| Allowed in a functional interface | Does **not** count toward "one abstract method" |
|-----------------------------------|--------------------------------------------------|
| Exactly **one** abstract method | `default` methods |
| | `static` methods |
| | `private` methods |
| | Abstract methods that match a public `Object` method (`equals`, `hashCode`, `toString`), e.g. `Comparator.equals` |

```java
public interface Comparator<T> {
    int compare(T a, T b);                              // the single abstract method
    boolean equals(Object obj);                         // matches Object.equals: not counted
    default Comparator<T> reversed() { ... }            // default: not counted
    static <T extends Comparable<? super T>> Comparator<T> naturalOrder() { ... }
}
```

### `@FunctionalInterface`

An **optional** annotation that makes the compiler verify the rule. Add it to your own functional interfaces: if someone later adds a second abstract method, compilation fails instead of silently breaking every lambda.

Familiar functional interfaces from before Java 8: `Runnable`, `Callable<V>`, `Comparator<T>`, `ActionListener`, `FileFilter`, `Iterable<T>` (one abstract method: `iterator()`).

## The four core interfaces

| Interface | Method | Takes | Returns | Think |
|-----------|--------|-------|---------|-------|
| `Function<T, R>` | `R apply(T t)` | one `T` | `R` | **transform** |
| `Predicate<T>` | `boolean test(T t)` | one `T` | `boolean` | **test** / filter |
| `Supplier<T>` | `T get()` | nothing | `T` | **produce** / lazy value |
| `Consumer<T>` | `void accept(T t)` | one `T` | nothing | **consume** / side effect |

```java
Function<String, Integer> length = String::length;
length.apply("hello");                                  // 5

Predicate<String> isEmpty = String::isEmpty;
isEmpty.test("");                                       // true

Supplier<List<String>> newList = ArrayList::new;
List<String> list = newList.get();

Consumer<String> printer = System.out::println;
printer.accept("hi");
```

## The full family

### Two arguments

| Interface | Method | Example |
|-----------|--------|---------|
| `BiFunction<T, U, R>` | `R apply(T, U)` | `(a, b) -> a + b` |
| `BiPredicate<T, U>` | `boolean test(T, U)` | `(s, n) -> s.length() > n` |
| `BiConsumer<T, U>` | `void accept(T, U)` | `(k, v) -> print(k, v)` (`Map.forEach`) |

### Operators: same type in and out

| Interface | Equivalent to | Method | Example |
|-----------|---------------|--------|---------|
| `UnaryOperator<T>` | `Function<T, T>` | `T apply(T)` | `String::trim`, `list.replaceAll(...)` |
| `BinaryOperator<T>` | `BiFunction<T, T, T>` | `T apply(T, T)` | `Integer::sum`, `stream.reduce(...)` |

### Primitive specializations (avoid boxing)

Generic interfaces box `int` to `Integer` on every call. The primitive variants avoid that, and you will see them throughout the Stream API:

| Pattern | Interfaces |
|---------|------------|
| Predicates | `IntPredicate`, `LongPredicate`, `DoublePredicate` |
| Suppliers | `IntSupplier`, `LongSupplier`, `DoubleSupplier`, `BooleanSupplier` |
| Consumers | `IntConsumer`, `LongConsumer`, `DoubleConsumer` |
| Functions from a primitive | `IntFunction<R>`, `LongFunction<R>`, `DoubleFunction<R>` |
| Functions to a primitive | `ToIntFunction<T>`, `ToLongFunction<T>`, `ToDoubleFunction<T>` |
| Primitive to primitive | `IntToLongFunction`, `IntToDoubleFunction`, `LongToIntFunction`, ... |
| Operators | `IntUnaryOperator`, `IntBinaryOperator`, `LongUnaryOperator`, `DoubleBinaryOperator`, ... |
| Two args to a primitive | `ToIntBiFunction<T, U>`, `ToDoubleBiFunction<T, U>`, ... |
| Object + primitive consumer | `ObjIntConsumer<T>`, ... |

```java
IntPredicate even = n -> n % 2 == 0;
ToIntFunction<String> len = String::length;
IntBinaryOperator add = (a, b) -> a + b;
IntFunction<String> stars = "*"::repeat;
people.stream().mapToInt(Person::age).sum();               // uses ToIntFunction: no boxing
```

### Which one do I use?

```
Does the code take an argument?
├── no ───────────────► returns a value?  yes → Supplier<T>     no → Runnable
└── yes ─► returns...
          ├── boolean ► Predicate<T>             (two args: BiPredicate)
          ├── nothing ► Consumer<T>              (two args: BiConsumer)
          ├── same type as the argument ► UnaryOperator<T>    (two same-typed args: BinaryOperator<T>)
          └── some other type ► Function<T, R>               (two args: BiFunction<T, U, R>)
Primitive int/long/double involved? use the Int/Long/Double variant.
Needs to throw a checked exception? Callable<V>, or your own interface.
```

## Composition methods

Functional interfaces include `default` methods for combining them into new functions.

### `Function`

```java
Function<Integer, Integer> doubleIt = x -> x * 2;
Function<Integer, Integer> addTen = x -> x + 10;

doubleIt.andThen(addTen).apply(5);       // addTen(doubleIt(5))  = 20     "first this, then that"
doubleIt.compose(addTen).apply(5);       // doubleIt(addTen(5))  = 30     "first that, then this"
Function.<Integer>identity().apply(7);   // 7
```

### `Predicate`

```java
Predicate<String> notEmpty = s -> !s.isEmpty();
Predicate<String> shortWord = s -> s.length() < 5;

notEmpty.and(shortWord).test("abc");     // true   (short-circuits like &&)
notEmpty.or(shortWord).test("");         // true
notEmpty.negate().test("");              // true
Predicate.not(String::isEmpty);          // Java 11: negation for method references: filter(Predicate.not(String::isBlank))
Predicate.isEqual("x");                  // null-safe equality predicate
```

### `Consumer`, `BinaryOperator`, `Comparator`

```java
Consumer<String> log = s -> System.out.println("LOG " + s);
Consumer<String> save = s -> repository.save(s);
log.andThen(save).accept("record");      // runs both, in order

BinaryOperator.minBy(Comparator.<Integer>naturalOrder());
BinaryOperator.maxBy(Comparator.comparing(String::length));

Comparator.comparing(Person::name).thenComparing(Person::age).reversed();
```

Composition lets you build behavior from small named parts instead of one big lambda ([08_functional-patterns.md](./08_functional-patterns.md)).

## Defining your own

Define your own functional interface when:

- No standard one fits (three arguments, a checked exception, a domain name that documents intent)
- You want a **meaningful name** in an API (`TaxCalculator` instead of `Function<Order, BigDecimal>`)

```java
@FunctionalInterface
interface TriFunction<A, B, C, R> { R apply(A a, B b, C c); }

@FunctionalInterface
interface ThrowingSupplier<T> { T get() throws Exception; }          // lets lambdas throw checked exceptions

@FunctionalInterface
interface DiscountPolicy {
    long apply(long cents);
    default DiscountPolicy then(DiscountPolicy next) { return c -> next.apply(apply(c)); }
}

DiscountPolicy tenOff = c -> c * 90 / 100;
DiscountPolicy shipping = c -> c + 500;
tenOff.then(shipping).apply(10_000);                                   // 9500
```

If an existing interface fits, use it: other developers already know `Function` and `Predicate`.

## Wildcards in functional parameters (PECS)

Accept the **most general** functional type so callers have flexibility ([07-generics/02_wildcards-and-pecs.md](../07-generics/02_wildcards-and-pecs.md)):

```java
<T, R> List<R> map(List<T> list, Function<? super T, ? extends R> fn)     // consumes T, produces R
void forEach(Iterable<T> it, Consumer<? super T> action)
boolean any(Collection<T> c, Predicate<? super T> p)
```

The JDK declares its APIs this way (`Stream.map`, `Collection.removeIf`, `Optional.map`).

## Functional interfaces in the JDK, by area

| Area | Uses |
|------|------|
| **Collections** | `removeIf(Predicate)`, `replaceAll(UnaryOperator)`, `forEach(Consumer)`, `Map.computeIfAbsent(Function)`, `Map.merge(BiFunction)`, `sort(Comparator)` |
| **Streams** | `filter(Predicate)`, `map(Function)`, `reduce(BinaryOperator)`, `collect(Collector)`, `mapToInt(ToIntFunction)` |
| **Optional** | `map(Function)`, `orElseGet(Supplier)`, `ifPresent(Consumer)`, `filter(Predicate)` |
| **Concurrency** | `Runnable`, `Callable`, `CompletableFuture.thenApply(Function)`, `supplyAsync(Supplier)` |
| **Other** | `Comparator`, `ThreadLocal.withInitial(Supplier)`, `Objects.requireNonNull(T, Supplier<String>)`, `Logger.log(Level, Supplier<String>)` (lazy message) |

### Supplier for laziness

```java
Objects.requireNonNull(x, () -> "expensive message " + compute());      // computed only if x is null
optional.orElseGet(() -> loadDefault());                                // computed only when empty
logger.debug(() -> "state=" + expensiveDump());                         // computed only if DEBUG is enabled
```

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Annotating an interface with `@FunctionalInterface` that has two abstract methods | `Multiple non-overriding abstract methods found` | Remove one, or make it `default` |
| Using `Function<Integer, Integer>` in hot numeric code | Boxing overhead | `IntUnaryOperator`, `IntBinaryOperator`, `ToIntFunction` |
| Choosing `Function<T, Boolean>` instead of `Predicate<T>` | Boxing, unclear intent, no `and`/`or` | `Predicate<T>` |
| Using `Consumer` and then needing a return value | Cannot return | `Function` |
| `f.andThen(g)` vs `f.compose(g)` mixed up | Operations in the wrong order | `andThen` = f first; `compose` = g first |
| Needing a checked exception from a standard interface | `unreported exception` | Custom interface (`ThrowingFunction`) or wrap |
| Writing `Predicate<String> p = s -> ...` then `Predicate.not(p::test)` | Verbose | `p.negate()` |
| Inventing a new interface when a standard one fits | Redundant types | Reuse `java.util.function` |
| Eager arguments where a `Supplier` was meant | Work done even when not needed | Pass a `Supplier` |
| Forgetting `? super`/`? extends` on functional parameters | Callers need awkward casts | PECS |

## Key takeaways

- A functional interface has one abstract method; `@FunctionalInterface` makes the compiler enforce it
- Core four: `Function` (transform), `Predicate` (test), `Supplier` (produce), `Consumer` (consume); plus `Bi*`, `UnaryOperator`, `BinaryOperator`
- Use primitive variants (`IntPredicate`, `ToIntFunction`, ...) in numeric code to avoid boxing
- Compose with `andThen`, `compose`, `and`, `or`, `negate`, `Predicate.not`
- Use `Supplier` for lazy values; accept `? super T` / `? extends R` in API parameters

**Next:** [Method References](./02_method-references.md)