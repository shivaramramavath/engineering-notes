# Generic Classes and Methods

A **generic** type or method declares one or more **type parameters** (placeholders such as `T`) that callers replace with real types. The compiler then checks that the types are used consistently.

```java
List<String> names = new ArrayList<>();
names.add("Ada");
String first = names.get(0);          // no cast needed
names.add(42);                        // compile error: int cannot be converted to String
```

## The problem generics solve

Before Java 5, a container of "anything" had to use `Object`:

```java
List raw = new ArrayList();           // raw type: no type information
raw.add("Ada");
raw.add(42);                          // nothing stops this
String s = (String) raw.get(1);       // ClassCastException at RUNTIME
```

| Without generics | With generics |
|------------------|---------------|
| Casts everywhere | No casts |
| Wrong types found at **runtime** (`ClassCastException`) | Wrong types found at **compile time** |
| Types hidden in comments | The signature documents the type: `List<String>` |
| One class per element type, or `Object` and casts | One class for all types |

## Generic classes

```java
public class Box<T> {                         // T = type parameter
    private T value;

    public Box(T value) { this.value = value; }

    public T get() { return value; }
    public void set(T value) { this.value = value; }
}

Box<String> s = new Box<>("hello");           // T = String
String text = s.get();                        // typed as String
Box<Integer> n = new Box<>(42);               // T = Integer
// Box<int> bad;                              // ERROR: type arguments must be reference types
```

```
class Box<T>          the template: T is a placeholder
Box<String>           a parameterized type: T replaced by String
Box<Integer>          another one: Box<String> and Box<Integer> are unrelated types
```

### Naming conventions

| Letter | Typical meaning |
|--------|------------------|
| `T` | Type (the default) |
| `E` | Element (collections) |
| `K`, `V` | Key, value (maps) |
| `N` | Number |
| `R` | Result / return type |
| `S`, `U` | Second, third types |

Single capital letters distinguish type parameters from class names. Descriptive names (`<Key, Value>`) are legal but uncommon.

### Several type parameters

```java
public class Pair<K, V> {
    private final K key;
    private final V value;
    public Pair(K key, V value) { this.key = key; this.value = value; }
    public K key() { return key; }
    public V value() { return value; }
}

Pair<String, Integer> p = new Pair<>("age", 36);
int age = p.value();                          // unboxed from Integer
```

A **record** can be generic too, which is the concise way to write pairs and tuples:

```java
public record Pair<A, B>(A first, B second) { }
```

### Generic interfaces

```java
public interface Repository<T, ID> {
    Optional<T> findById(ID id);
    List<T> findAll();
}

public class UserRepository implements Repository<User, Long> {     // fixes T and ID
    public Optional<User> findById(Long id) { ... }
    public List<User> findAll() { ... }
}
```

Familiar examples: `Comparable<T>`, `Comparator<T>`, `Iterable<T>`, `Function<T, R>`, `Supplier<T>`.

### A generic stack

```java
public class Stack<E> {
    private final List<E> items = new ArrayList<>();

    public void push(E e) { items.add(e); }

    public E pop() {
        if (items.isEmpty()) throw new NoSuchElementException("stack is empty");
        return items.remove(items.size() - 1);
    }

    public boolean isEmpty() { return items.isEmpty(); }
}

Stack<String> stack = new Stack<>();
stack.push("a");
String top = stack.pop();
```

## The diamond operator `<>`

The compiler can infer the type arguments of a constructor call from the target type:

```java
Map<String, List<Integer>> map = new HashMap<String, List<Integer>>();   // verbose (Java 5)
Map<String, List<Integer>> map = new HashMap<>();                         // diamond (Java 7+)
var map = new HashMap<String, List<Integer>>();                           // var: you must then spell the arguments
```

The diamond also works with anonymous classes since Java 9.

## Generic methods

A method can have its **own** type parameters, declared **before the return type**, independent of the class:

```java
public static <T> T firstOrNull(List<T> list) {
    return list.isEmpty() ? null : list.get(0);
}

String s = firstOrNull(List.of("a", "b"));        // T inferred as String
Integer i = firstOrNull(List.of(1, 2));           // T inferred as Integer
```

```
public static  <T>   T      firstOrNull( List<T> list )
                 │   │                       │
   type parameter    return type uses T      parameter uses T
   declaration
```

More examples:

```java
public static <T> void swap(T[] array, int i, int j) {
    T tmp = array[i];
    array[i] = array[j];
    array[j] = tmp;
}

public static <T> boolean contains(T[] array, T target) {
    for (T item : array) if (Objects.equals(item, target)) return true;
    return false;
}

public static <K, V> Map<V, K> invert(Map<K, V> map) {
    Map<V, K> result = new HashMap<>();
    map.forEach((k, v) -> result.put(v, k));
    return result;
}
```

### Type inference

The compiler infers `T` from the arguments and, if needed, from the expected return type:

```java
List<String> empty = Collections.emptyList();       // T inferred from the assignment target
List<String> names = List.of("a", "b");             // T = String
process(Collections.emptyList());                   // inferred from process's parameter type (Java 8+)
```

Explicit type arguments are rarely needed, but the syntax is:

```java
List<String> list = Collections.<String>emptyList();
this.<String>helper();           // qualifier required: object, class or `this`
```

### Static methods and class type parameters

A **static** method cannot use the class's `T`, because `T` belongs to an instance (`Box<String>`). It must declare its own:

```java
public class Box<T> {
    // public static T create() { }              // ERROR: cannot reference non-static type variable T
    public static <U> Box<U> of(U value) { return new Box<>(value); }    // its own <U>
}
```

Static factory methods like `List.of`, `Optional.of`, `Map.entry` are generic methods.

### Generic constructors

Rare, but allowed:

```java
public class Converter {
    public <T> Converter(T seed) { ... }
}
```

## Inheritance with generics

```java
class IntBox extends Box<Integer> { }              // fixes T = Integer

class NamedBox<T> extends Box<T> {                 // keeps T open
    private final String name;
    NamedBox(String name, T value) { super(value); this.name = name; }
}

class Child<T> implements Comparable<Child<T>> { ... }
```

### Generic types are **invariant**

```java
Box<Integer> ints = new Box<>(1);
Box<Number> nums = ints;              // ERROR: Box<Integer> is NOT a Box<Number>
List<Object> objs = new ArrayList<String>();   // ERROR
```

Even though `Integer` is a `Number`, `Box<Integer>` and `Box<Number>` are unrelated types. If it were allowed, you could put a `Double` into a box that is secretly an `Integer` box. **Wildcards** express "a box of some kind of number": [02_wildcards-and-pecs.md](./02_wildcards-and-pecs.md).

Arrays, by contrast, are covariant (`Number[] n = new Integer[1]` compiles) and fail at runtime with `ArrayStoreException`: one more reason generics are safer.

## Restrictions you will run into

| You cannot... | Why | Do this |
|---------------|-----|---------|
| Use primitives as type arguments (`List<int>`) | Type arguments must be reference types | `List<Integer>` (boxing: [wrappers](../01-fundamentals/04_wrapper-classes-and-autoboxing.md)), or primitive streams |
| `new T()` | Erased at runtime | Pass a `Supplier<T>` or `Class<T>` |
| `new T[10]` | Erasure and array covariance | `List<T>`, or `(T[]) new Object[10]` with care, or `IntFunction<T[]>` |
| `x instanceof List<String>` | The argument is erased | `x instanceof List<?>` |
| Use `T` in a `static` field | One static field is shared by all instantiations | Do not |
| Throw or catch generic exceptions (`class MyEx<T> extends Exception`) | Erasure | Plain exceptions |
| Overload by type argument only (`m(List<String>)` and `m(List<Integer>)`) | Same erasure | Different names |

Most of these come from **type erasure**: [03_type-erasure.md](./03_type-erasure.md).

## Raw types: avoid them

A **raw type** is a generic type used without type arguments:

```java
List list = new ArrayList();          // raw
list.add("x");
list.add(1);                          // compiles, with an "unchecked" warning
```

Raw types exist for backward compatibility with pre-Java 5 code. They disable type checking and produce **unchecked warnings**. Always parameterize: `List<String>`; use `List<?>` if you genuinely do not care.

## Generics in everyday APIs

```java
Map<String, List<Order>> ordersByCustomer = new HashMap<>();
Optional<User> user = repository.findById(42L);
Comparator<String> byLength = Comparator.comparingInt(String::length);
Function<String, Integer> parse = Integer::parseInt;
List<String> sorted = names.stream().sorted().toList();
CompletableFuture<Response> future = client.sendAsync(request);
```

Reading generics is a core skill: `Map<String, List<Order>>` is "a map from String to a list of Orders". See [08-collections](../08-collections/README.md) and [09-functional-java](../09-functional-java/README.md).

## Generics and `var`

```java
var names = new ArrayList<String>();      // inferred ArrayList<String>
var list = new ArrayList<>();             // inferred ArrayList<Object>: probably not what you wanted
```

With `var` and the diamond, the compiler has no target type, so it falls back to `Object`. Spell the type argument or use an explicit type on the left ([var](../12-modern-java/05_var-and-type-inference.md)).

## When to write your own generic type

| Write one when | Otherwise |
|----------------|-----------|
| The class's logic is **independent of the element type** (containers, wrappers, caches, results) | Use the JDK's (`List`, `Map`, `Optional`) |
| You want a **type-safe contract** across several methods (`Repository<T, ID>`) | A concrete type is simpler |
| You are writing a **utility** that works on any type (`swap`, `max`) | Avoid generics for the sake of it |

Generics add cognitive load; use them where they remove casts and duplication.

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Using raw types (`List` instead of `List<String>`) | Unchecked warnings, runtime `ClassCastException` | Parameterize |
| Expecting `List<Integer>` to be a `List<Number>` | Incompatible types | Wildcards: `List<? extends Number>` |
| `new T()` or `new T[n]` | Compile error | `Supplier<T>`, `Class<T>`, `List<T>` |
| Using the class's `T` in a static method | `cannot be referenced from a static context` | Declare `<U>` on the method |
| Using `List<int>` | `unexpected type; required: reference` | `List<Integer>` |
| `var list = new ArrayList<>();` | `ArrayList<Object>` | Give the type argument |
| Forgetting `<T>` before the return type in a generic method | `cannot find symbol class T` | `public static <T> T foo(...)` |
| Suppressing unchecked warnings without thinking | Hidden heap pollution | Understand each warning ([03](./03_type-erasure.md)) |
| Making everything generic | Unreadable signatures | Use generics where they help |
| Comparing generic values with `==` | Reference comparison, wrong for boxed values | `equals` / `Objects.equals` |

## Key takeaways

- Type parameters turn runtime casts into compile-time checks and let one class serve many types
- Declare a type parameter on a class (`Box<T>`) or on a method (`<T> T first(...)`, before the return type)
- The diamond `<>` and method calls infer type arguments; spell them out only when needed
- Generic types are **invariant**: `List<Integer>` is not a `List<Number>`
- No primitives, no `new T()`, no `new T[]`, no static `T`; avoid raw types

**Next:** [Bounded Types](./01_bounded-types.md)
