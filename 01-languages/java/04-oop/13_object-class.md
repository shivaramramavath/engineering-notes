# The Object Class

`java.lang.Object` is the **root of the class hierarchy**. Every class extends it, directly or indirectly, so every object has the methods defined here. If a class declares no `extends`, the compiler makes it extend `Object`.

```java
class Point { }                    // really: class Point extends Object { }

Object o = new Point();            // any object fits an Object variable
Object s = "text";
Object n = 42;                     // autoboxed to Integer
```

```
                 Object
        ┌──────────┼───────────┐
      String    Number      Point (your class)
               ┌───┴───┐
           Integer   Double
```

Arrays are objects too: `Object o = new int[3];` is legal.

## The methods

| Method | Purpose | Override it? |
|--------|---------|--------------|
| `toString()` | Text representation | **Yes**, almost always |
| `equals(Object)` | Logical equality | Yes, for value-like classes ([14](./14_equals-and-hashcode.md)) |
| `hashCode()` | Hash for hash-based collections | Yes, **together with** `equals` |
| `getClass()` | The runtime `Class` object | No (`final`) |
| `clone()` | Create a copy | Rarely: see [15](./15_clone-and-copying.md) |
| `wait()`, `notify()`, `notifyAll()` | Thread coordination | No (`final`); see [14-concurrency/03_synchronization.md](../14-concurrency/03_synchronization.md) |
| `finalize()` | Cleanup before GC | **Never**: deprecated for removal |

## `toString()`

The default implementation returns the class name, `@`, and the identity hash code in hex: not useful.

```java
class Point { int x, y; Point(int x, int y) { this.x = x; this.y = y; } }
System.out.println(new Point(1, 2));          // Point@1b6d3586
```

Override it:

```java
@Override
public String toString() {
    return "Point[x=" + x + ", y=" + y + "]";
}
System.out.println(new Point(1, 2));          // Point[x=1, y=2]
```

`toString()` is called implicitly by string concatenation, `println`, `String.valueOf`, `%s` in `format`, collection printing, and debuggers/logs.

### Guidelines

| Guideline | Why |
|-----------|-----|
| Include the **meaningful state**, in a consistent format | Debugging and logging |
| Never include secrets (passwords, tokens, card numbers, personal data) | `toString` ends up in logs ([21-security](../21-security/README.md)) |
| Do not call `toString()` of objects that point back (cycles) | `StackOverflowError` |
| Keep it cheap and side-effect free | It is called implicitly and often |
| Do not make program logic depend on its format | It is for humans; use explicit methods for machine formats |
| Consider `Objects.toString(x, "default")` for nullable fields | `"null"` vs a default |

Records generate `toString`, `equals` and `hashCode` for you ([12-modern-java/01_records.md](../12-modern-java/01_records.md)). IDEs generate them too.

For arrays use `Arrays.toString` / `Arrays.deepToString` ([arrays](../01-fundamentals/07_arrays.md)).

## `equals` and `hashCode` (overview)

- Default `equals` is **identity**: `a.equals(b)` is the same as `a == b`
- Default `hashCode` is derived from the object's identity
- Override both when two objects with the same state should be considered equal (value objects, map keys, set elements)

Full contracts, a correct implementation and common bugs: [14_equals-and-hashcode.md](./14_equals-and-hashcode.md).

## `getClass()`

Returns the **runtime** class of the object (never the declared type).

```java
Object o = "hello";
o.getClass();                       // class java.lang.String
o.getClass().getName();             // "java.lang.String"
o.getClass().getSimpleName();       // "String"
o.getClass() == String.class;       // true

Animal a = new Dog();
a.getClass() == Dog.class;          // true
a instanceof Animal;                // true
a.getClass() == Animal.class;       // false
```

| | `instanceof` | `getClass() ==` |
|---|--------------|-----------------|
| True for subclasses? | **Yes** | **No** (exact class only) |
| `null` | `false` | `NullPointerException` |
| Typical use | "Can I treat this as T?" | "Is this exactly T?" (strict `equals` in non-final classes) |

`getClass()` is also the entry point for reflection ([13-advanced-language-features/01_reflection.md](../13-advanced-language-features/01_reflection.md)). A common logging idiom is `getClass().getSimpleName()`.

## The `Objects` utility class

`java.util.Objects` (note the **s**) provides null-safe helpers:

| Method | Purpose |
|--------|---------|
| `Objects.equals(a, b)` | Null-safe `equals` |
| `Objects.hash(a, b, c)` | Hash code from several fields |
| `Objects.hashCode(o)` | `0` for `null`, else `o.hashCode()` |
| `Objects.toString(o)` / `toString(o, "default")` | Null-safe string |
| `Objects.requireNonNull(o, "message")` | Fail fast with a clear message |
| `Objects.requireNonNullElse(o, fallback)` | Default for `null` |
| `Objects.isNull(o)` / `nonNull(o)` | Handy in streams (`filter(Objects::nonNull)`) |
| `Objects.checkIndex(i, length)` | Index validation |

```java
public Person(String name) {
    this.name = Objects.requireNonNull(name, "name must not be null");
}
```

## `Object` as a parameter or type

```java
void log(Object value) { System.out.println(value); }     // accepts anything
Object[] anything = { 1, "two", 3.0 };                    // heterogeneous array
```

Using `Object` loses type information and forces casts. Before generics (Java 5), collections stored `Object`; today use generics: `List<String>` ([07-generics](../07-generics/README.md)). A downcast from `Object` needs `instanceof` ([07](./07_polymorphism.md)).

## `wait`, `notify`, `notifyAll`

Low-level monitor methods used with `synchronized` for thread coordination. Modern code uses `java.util.concurrent` (locks, `BlockingQueue`, `CompletableFuture`) instead ([14-concurrency](../14-concurrency/README.md)).

## `finalize()` is obsolete

`finalize()` was meant for cleanup before garbage collection, but it is unpredictable (may never run), slows GC, and can resurrect objects. It has been **deprecated for removal** since Java 18. Instead:

| Need | Use |
|------|-----|
| Release files, sockets, connections deterministically | `try-with-resources` and `AutoCloseable` ([06-exceptions-and-debugging/03_try-with-resources.md](../06-exceptions-and-debugging/03_try-with-resources.md)) |
| Safety net for native resources | `java.lang.ref.Cleaner` |

## Identity vs equality, revisited

```java
String a = new String("x");
String b = new String("x");
a == b;                 // false: identity
a.equals(b);            // true: String overrides equals
new Object().equals(new Object());   // false: default is identity
```

The `==` operator compares references and cannot be overridden. `equals` can.

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Not overriding `toString` | `Point@1b6d3586` in logs | Implement it |
| Putting passwords or tokens in `toString` | Secrets leak into logs | Exclude or mask them |
| Overriding `equals` without `hashCode` | `HashMap`/`HashSet` misbehave | Always override both ([14](./14_equals-and-hashcode.md)) |
| Using `getClass() ==` in `equals` when subclass equality is intended | Equal subclass instances compare unequal | Decide deliberately; see [14](./14_equals-and-hashcode.md) |
| `toString` with cyclic references | `StackOverflowError` | Print ids, not whole graphs |
| Relying on `toString` output for logic | Breaks when the format changes | Explicit methods |
| Using `Object` parameters to avoid generics | Casts everywhere, runtime errors | Generics |
| Overriding `finalize` | Unpredictable cleanup | `AutoCloseable` / `Cleaner` |
| `obj.equals(null)` thinking it may be true | Always `false` | `obj == null` |
| Confusing `Object` and `Objects` | Compile errors | `java.util.Objects` is the helper class |

## Key takeaways

- Every class inherits from `Object`; arrays and lambdas are objects too
- Override `toString` always, `equals` and `hashCode` together for value-like classes
- `getClass()` gives the runtime type; `instanceof` also accepts subtypes
- `Objects` provides null-safe `equals`, `hash`, `toString` and `requireNonNull`
- Forget `finalize()`; use `try-with-resources` and `Cleaner`

**Next:** [equals and hashCode](./14_equals-and-hashcode.md)
