# Wildcards and PECS

Generic types are **invariant**: `List<Integer>` is **not** a `List<Number>`, even though `Integer` is a `Number`. **Wildcards** (`?`) let a method accept a *family* of parameterized types, and **PECS** tells you which wildcard to use.

```java
double sum(List<Number> nums) { ... }
sum(List.of(1, 2, 3));              // ERROR: List<Integer> is not a List<Number>

double sum(List<? extends Number> nums) { ... }
sum(List.of(1, 2, 3));              // OK
sum(List.of(1.5, 2.5));             // OK
```

## Why generics are invariant

```java
List<Integer> ints = new ArrayList<>();
List<Number> nums = ints;           // suppose this were allowed...
nums.add(3.14);                     // ...legal for List<Number>
Integer i = ints.get(0);            // ClassCastException: a Double is in a "list of Integer"
```

Invariance protects you. The price is that a method taking `List<Number>` rejects `List<Integer>`. Wildcards restore flexibility **safely**, by restricting what you can do with the list.

## The three wildcards

| Wildcard | Read as | Means |
|----------|---------|-------|
| `List<?>` | "list of unknown" | A list of **some** type; you do not know or care which |
| `List<? extends Number>` | "list of some subtype of Number" | Elements are `Number` or a subtype (**upper bound**) |
| `List<? super Integer>` | "list of some supertype of Integer" | Elements' type is `Integer` or a supertype (**lower bound**) |

```
                      List<?>                         ◄── everything below is a List<?>
                      /      \
       List<? extends Number>   List<? super Integer>
          /       |       \          /        |        \
 List<Integer> List<Double> ...  List<Integer> List<Number> List<Object>
```

## `? extends T`: a **producer** (read-only)

```java
List<? extends Number> list = new ArrayList<Integer>();    // could be Integer, Double, ...

Number n = list.get(0);          // OK: whatever it is, it is at least a Number
list.add(1);                     // ERROR: it might be a List<Double>
list.add(null);                  // only null is allowed
```

You can **get** elements as `T` (the bound), but you cannot **add**, because the compiler does not know the exact element type. Use `? extends` when the collection **produces** values for you.

```java
static double sum(Collection<? extends Number> numbers) {
    double total = 0;
    for (Number n : numbers) total += n.doubleValue();         // reading as Number
    return total;
}
sum(List.of(1, 2, 3));
sum(Set.of(1.5, 2.5));
```

## `? super T`: a **consumer** (write-only)

```java
List<? super Integer> list = new ArrayList<Number>();      // could be Integer, Number or Object

list.add(42);                    // OK: an Integer fits in any of those lists
list.add(Integer.valueOf(7));    // OK
list.add(3.14);                  // ERROR: a Double does not fit a List<Integer>
Object o = list.get(0);          // only Object is guaranteed when reading
```

You can **add** `T` (and its subtypes), but reading gives you only `Object`. Use `? super` when the collection **consumes** values you give it.

```java
static void fillWithNumbers(List<? super Integer> sink, int count) {
    for (int i = 0; i < count; i++) sink.add(i);
}
List<Number> numbers = new ArrayList<>();
List<Object> objects = new ArrayList<>();
fillWithNumbers(numbers, 3);     // OK
fillWithNumbers(objects, 3);     // OK
```

## PECS: **P**roducer **E**xtends, **C**onsumer **S**uper

A mnemonic from *Effective Java*:

> If a parameterized type **produces** `T` values for you (you read from it), use `? extends T`.
> If it **consumes** `T` values from you (you write to it), use `? super T`.
> If it does both, do not use a wildcard: use exactly `T`.

| You... | Use | Example |
|--------|-----|---------|
| Only **read** `T`s from it | `? extends T` | `Collection<? extends E> src` |
| Only **write** `T`s into it | `? super T` | `Collection<? super E> dst` |
| Read **and** write | `T` (no wildcard) | `List<T> list` |
| Neither (only `size()`, `isEmpty()`, `Object` methods) | `?` | `List<?> list` |

### The canonical example: copying

```java
public static <T> void copy(List<? super T> dest, List<? extends T> src) {
    for (T item : src) dest.add(item);       // read T from src (producer), write T into dest (consumer)
}

List<Integer> ints = List.of(1, 2, 3);
List<Number> nums = new ArrayList<>();
List<Object> objs = new ArrayList<>();
copy(nums, ints);                            // Integers into a List<Number>
copy(objs, ints);                            // Integers into a List<Object>
```

The JDK follows PECS everywhere:

| API | Signature |
|-----|-----------|
| `Collections.copy` | `<T> void copy(List<? super T> dest, List<? extends T> src)` |
| `Collection.addAll` | `boolean addAll(Collection<? extends E> c)` |
| `Collections.sort` | `<T> void sort(List<T> list, Comparator<? super T> c)` |
| `Stream.map` | `<R> Stream<R> map(Function<? super T, ? extends R> mapper)` |
| `Stream.forEach` | `void forEach(Consumer<? super T> action)` |
| `Optional.orElseGet` | `T orElseGet(Supplier<? extends T> supplier)` |
| `Comparator.thenComparing` | `Comparator<T> thenComparing(Comparator<? super T> other)` |

Read `Function<? super T, ? extends R>` as: the function **consumes** `T`s (so it may accept any supertype of `T`) and **produces** `R`s (so it may return any subtype of `R`).

### A comparator that accepts a supertype

```java
Comparator<Object> byToString = Comparator.comparing(Object::toString);
List<String> names = new ArrayList<>(List.of("b", "a"));
names.sort(byToString);                      // OK because sort takes Comparator<? super String>
```

A comparator for `Object` can compare `String`s; `? super T` makes that legal.

## Unbounded wildcard `?`

Use `List<?>` when the **element type is irrelevant**.

```java
static void printAll(List<?> list) {
    for (Object o : list) System.out.println(o);       // reading as Object
}
static int count(Collection<?> c) { return c.size(); }

printAll(List.of(1, 2));
printAll(List.of("a", "b"));
```

| `List<?>` | `List<Object>` |
|-----------|----------------|
| Accepts a list of **any** type | Accepts only `List<Object>` (not `List<String>`) |
| Cannot add anything (except `null`) | Can add any object |

For type tests, `x instanceof List<?>` is the legal form ([03_type-erasure.md](./03_type-erasure.md)). `Class<?>` is an everyday example: "a class object of some type".

## Wildcard capture

The compiler can **capture** a wildcard as a hidden type variable. Sometimes you need that variable by name, so you add a generic helper method:

```java
static void reverse(List<?> list) {
    reverseHelper(list);                     // the wildcard is captured as T inside the helper
}
private static <T> void reverseHelper(List<T> list) {
    for (int i = 0, j = list.size() - 1; i < j; i++, j--) {
        T tmp = list.get(i);
        list.set(i, list.get(j));
        list.set(j, tmp);
    }
}
```

Directly, `list.set(i, list.get(j))` on a `List<?>` is a compile error ("capture of ? cannot be converted to capture of ?"), because the two `?` are not known to be the same type. The helper gives that type a name. `Collections.reverse` and `Collections.swap` use this idiom.

## Wildcard or type parameter?

Both can express "any list of numbers". Choose by whether the type must be **named**.

```java
static double sum(List<? extends Number> list)         // wildcard: type not needed again
static <T extends Number> double sum(List<T> list)     // equivalent here, more verbose
```

| Use a **wildcard** when | Use a **type parameter** when |
|-------------------------|-------------------------------|
| The type appears **once** in the signature | The type appears **more than once** (ties parameters, or parameter and return) |
| You only read (or only write) | You both read and write elements of the same type |
| You want the simplest signature | You need the type inside the body (`T item = ...`) |

```java
static <T> void swap(List<T> list, int i, int j)       // needs T for the temporary variable and both accesses
static <T> T pick(List<T> a, List<T> b)                // return type matches the element type
static <T extends Comparable<? super T>> T max(Collection<? extends T> items)   // combination
```

Rule of thumb: **prefer wildcards for parameters, type parameters when you need to refer to the type.**

## Do not use wildcards in return types

```java
List<? extends Number> numbers();       // forces callers to deal with wildcards: awkward
List<Number> numbers();                 // callers get a usable type
<T extends Number> List<T> numbers();   // or let the caller choose, when it makes sense
```

Wildcards are for **flexible parameters**, not for return values ("if the user of a class has to think about wildcard types, there is probably something wrong with its API", *Effective Java*).

## Nested and multiple wildcards

```java
Map<String, ? extends List<?>> m;             // values are lists of something, can be read as List<?>
List<? extends List<? extends Number>> lol;   // lists of lists of numbers
Map<Class<?>, Object> registry;               // keys: any class
```

Nested wildcards get unreadable quickly. Introduce a type parameter or a named type if you find yourself writing them.

## Arrays vs generics: covariance

```java
Number[] arr = new Integer[1];     // arrays are COVARIANT
arr[0] = 3.14;                     // compiles; ArrayStoreException at RUNTIME

List<? extends Number> list = new ArrayList<Integer>();    // generics: wildcards, safe by construction
// list.add(3.14);                 // compile error
```

Generics move the check to compile time, at the cost of wildcards.

## Quick reference

| Declaration | Can read as | Can write |
|-------------|-------------|-----------|
| `List<T>` | `T` | `T` |
| `List<? extends T>` | `T` | nothing (except `null`) |
| `List<? super T>` | `Object` | `T` (and subtypes) |
| `List<?>` | `Object` | nothing (except `null`) |

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Taking `List<Number>` and expecting `List<Integer>` to fit | `incompatible types` | `List<? extends Number>` |
| Adding to a `List<? extends T>` | `no suitable method found for add` | It is a producer: use `? super T` or `T` if you need to write |
| Reading `T` from a `List<? super T>` | Only `Object` available | It is a consumer: use `? extends` if you need to read |
| `List<?>` when you need to add | Compile error | A type parameter `<T> List<T>` |
| Wildcards in return types | Awkward callers | Concrete or type-parameter return types |
| `List<Object>` instead of `List<?>` | Rejects `List<String>` | `List<?>` |
| Overusing wildcards where one `T` is clearer | Unreadable signatures | Name the type |
| Capture errors on `List<?>` | `capture of ?` messages | A generic helper method |
| Forgetting `Comparator<? super T>` | Cannot pass a supertype comparator | Use `? super T` |
| Mixing up the mnemonic | Wrong bound | PECS: you **read** from producers, **write** to consumers |

## Key takeaways

- `List<Integer>` is not a `List<Number>`: generics are invariant, to keep writes safe
- `? extends T` = producer (read as `T`, no writes); `? super T` = consumer (write `T`, read `Object`); `?` = element type irrelevant
- **PECS**: Producer Extends, Consumer Super; if you both read and write, use plain `T`
- Prefer wildcards on parameters, type parameters when the type must be named, and never wildcards in return types
- Use a generic helper to "capture" a wildcard when you need to write through it

**Next:** [Type Erasure](./03_type-erasure.md)
