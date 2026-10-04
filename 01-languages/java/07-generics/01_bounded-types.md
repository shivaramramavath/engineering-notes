# Bounded Types

A plain type parameter `T` could be **anything**, so inside the generic code you can only use `Object`'s methods. A **bound** restricts which types are allowed and, in return, lets you call the bound's methods.

```java
static <T> T max(T a, T b) {
    return a.compareTo(b) > 0 ? a : b;        // ERROR: Object has no compareTo
}

static <T extends Comparable<T>> T max(T a, T b) {
    return a.compareTo(b) > 0 ? a : b;        // OK: T is guaranteed to be Comparable
}

max(3, 7);              // 7
max("pear", "apple");   // "pear"
max(new Object(), new Object());   // ERROR: Object is not Comparable<Object>
```

## Upper bounds: `T extends X`

```java
public class NumberBox<T extends Number> {
    private final T value;
    public NumberBox(T value) { this.value = value; }
    public double doubled() { return value.doubleValue() * 2; }     // Number's methods are available
}

new NumberBox<>(5);            // Integer is a Number
new NumberBox<>(2.5);          // Double is a Number
new NumberBox<String>("x");    // ERROR: String is not within bounds
```

```
T extends Number
   allowed:  Integer, Long, Double, BigDecimal, ... any subclass of Number
   not:      String, Boolean, Object
```

### `extends` means "is a subtype of", for classes and interfaces

In a bound, the keyword is **always `extends`**, even when the bound is an interface:

```java
<T extends Comparable<T>>        // T implements Comparable<T>
<T extends Runnable>
<T extends Number>               // T is Number or a subclass
```

Never `implements` in a type-parameter bound.

### On methods

```java
public static <T extends Number> double sum(List<T> list) {
    double total = 0;
    for (T n : list) total += n.doubleValue();
    return total;
}

sum(List.of(1, 2, 3));            // 6.0
sum(List.of(1.5, 2.5));           // 4.0
```

## Multiple bounds

Combine bounds with `&`. At most **one class** (and it must be listed **first**), followed by any number of interfaces:

```java
<T extends Number & Comparable<T>>          // a Number that is also Comparable
<T extends Shape & Serializable & Cloneable>

static <T extends Number & Comparable<T>> T largest(List<T> list) {
    T best = list.get(0);
    for (T item : list) if (item.compareTo(best) > 0) best = item;
    return best;
}
```

```java
<T extends Comparable<T> & Number>    // ERROR: the class must come first
<T extends Number & Integer>          // ERROR: two classes
```

The compiler **erases** `T` to the **first** bound, so the order also decides which type appears in the compiled signature ([03_type-erasure.md](./03_type-erasure.md)).

## The recursive bound: `T extends Comparable<T>`

"T can be compared with other Ts."

```java
public static <T extends Comparable<T>> T maxOf(Collection<T> items) {
    Iterator<T> it = items.iterator();
    T best = it.next();
    while (it.hasNext()) {
        T next = it.next();
        if (next.compareTo(best) > 0) best = next;
    }
    return best;
}
```

It is called **recursive** or **self-referential** because `T` appears in its own bound. It guarantees that `a.compareTo(b)` takes another `T`.

### The more flexible form: `Comparable<? super T>`

Consider a subclass whose parent defines the ordering:

```java
class Animal implements Comparable<Animal> { ... }
class Dog extends Animal { }

<T extends Comparable<T>> T max(List<T> l);
max(List.of(new Dog()));     // ERROR: Dog is not Comparable<Dog> (it is Comparable<Animal>)

<T extends Comparable<? super T>> T max(List<T> l);
max(List.of(new Dog()));     // OK: Dog is Comparable<Animal>, and Animal is a supertype of Dog
```

`Comparable<? super T>` ("T can be compared to T or any supertype of T") is the signature the JDK uses, for example in `Collections.sort` and `Collections.max`. It follows the PECS rule: a `Comparable` **consumes** `T`s ([02_wildcards-and-pecs.md](./02_wildcards-and-pecs.md)).

For simple code, `T extends Comparable<T>` is fine. Use the wildcard form in **library** APIs that must accept subclasses.

### Self-bound in `Enum`

```java
public abstract class Enum<E extends Enum<E>> implements Comparable<E> { ... }
```

Each enum type `Color extends Enum<Color>`, so `compareTo` accepts only the same enum type. The same trick powers **self-typed builders** ([04_generic-patterns.md](./04_generic-patterns.md)). Related APIs: `EnumSet<E extends Enum<E>>` and `Enum.valueOf(Class<T>, String)` with `<T extends Enum<T>>`.

## Bounds and type safety in practice

```java
public static <T extends Shape> double totalArea(List<T> shapes) {
    return shapes.stream().mapToDouble(Shape::area).sum();
}
```

Compare with the version without a type parameter:

```java
public static double totalArea(List<? extends Shape> shapes) { ... }
```

Both accept `List<Circle>` and `List<Rect>`. The wildcard form is simpler when you **do not need to name the type** again. Use a named bounded `T` when the type appears **more than once** in the signature (parameter and return, or two parameters that must match):

```java
static <T extends Comparable<T>> T max(T a, T b)               // T ties both parameters and the result together
static <T extends Shape> T biggest(List<T> shapes)             // returns the SAME element type
static <T extends Shape> void copy(List<T> src, List<T> dst)   // both lists must have the same T
```

More: [02_wildcards-and-pecs.md#wildcard-or-type-parameter](./02_wildcards-and-pecs.md#wildcard-or-type-parameter).

## Bounds on type parameters of classes

```java
public class Cache<K extends Comparable<K>, V> {                   // K must be sortable
    private final TreeMap<K, V> map = new TreeMap<>();
    ...
}

public class Pipeline<I, O extends Serializable> { ... }

public interface Visitor<R> { R visit(Node n); }                   // unbounded: R can be anything
```

## There is no lower bound on type parameters

You can say "`T` is a subtype of X" (`T extends X`), but **not** "`T` is a supertype of X" (`T super X` is illegal). Lower bounds exist only on **wildcards**:

```java
<T super Integer> void add(List<T> l);         // ERROR
void add(List<? super Integer> l) { l.add(1); }   // OK: wildcard with a lower bound
```

See [02_wildcards-and-pecs.md](./02_wildcards-and-pecs.md).

## What you can do with a bounded `T`

| You can | Because |
|---------|---------|
| Call methods of the bound (`value.doubleValue()`, `a.compareTo(b)`) | The compiler knows `T` is at least that type |
| Assign a `T` to a variable of the bound type (`Number n = t;`) | Upcast |
| Pass `T` where the bound is expected | Same |
| Use `instanceof` / casts to other types (with care) | Still ordinary objects |

| You cannot | Because |
|------------|---------|
| Assign a concrete value: `T t = Integer.valueOf(1)` | `T` might be `Double`, not `Integer` |
| Create `T` with `new T()` | Erasure ([03](./03_type-erasure.md)) |
| Use `<`, `>` on `T` | Operators work on primitives; use `compareTo` |
| Pass primitives as type arguments | `T` is always a reference type |

```java
static <T extends Number> T zero() { return 0; }     // ERROR: Integer is not necessarily T
static <T extends Number> T first(List<T> l) { return l.get(0); }   // OK
```

## Choosing a bound: guidelines

| Guideline | Why |
|-----------|-----|
| Use the **narrowest useful** bound | `Number` when you need `doubleValue()`; `Comparable` when you need ordering |
| Bound by an **interface** where possible | Gives callers more types to choose from |
| Prefer `Comparator<? super T>` parameters over bounding `T extends Comparable<T>` in flexible APIs | Callers can supply any ordering |
| Do not bound "just in case" | An unnecessary bound narrows the callers |
| Do not over-engineer | `T extends Comparable<? super T> & Serializable` is rarely justified |

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Writing `<T implements X>` | Syntax error | `<T extends X>` |
| `<T extends A & B>` where `A` is an interface listed first and `B` a class | Compile error | Class first |
| Calling `compareTo` on an unbounded `T` | `cannot find symbol` | `T extends Comparable<T>` |
| `T extends Comparable<T>` and then a subclass instance fails | `inferred type does not conform to upper bound` | `Comparable<? super T>` |
| Trying `T super X` | Syntax error | Wildcard `? super X` |
| Returning a literal as `T` (`return 0;`) | `incompatible types` | Take a `Supplier<T>`, or the value from an argument |
| Using a bound but then casting to a concrete subtype | Fragile, unchecked | Redesign with a different type parameter |
| Raw `Comparable` | Unchecked warnings | `Comparable<T>` |
| Over-restrictive bounds on public APIs | Users cannot call your method | Loosen with wildcards |

## Key takeaways

- A bound (`T extends X`) lets you call `X`'s methods on `T` and restricts the allowed type arguments
- Use `extends` for both classes and interfaces; combine with `&` (class first, at most one class)
- `T extends Comparable<T>` expresses "comparable to itself"; the flexible library form is `Comparable<? super T>`
- Type parameters have **only upper bounds**; lower bounds are for wildcards
- Name a bounded `T` when the type is used in more than one place; otherwise a wildcard is simpler

**Next:** [Wildcards and PECS](./02_wildcards-and-pecs.md)
