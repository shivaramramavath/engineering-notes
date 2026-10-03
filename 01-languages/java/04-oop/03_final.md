# Final

`final` means "cannot change after this point". Its effect depends on where it is used: on a **variable** (assign once), a **method** (cannot be overridden) or a **class** (cannot be extended).

```java
final int max = 10;                  // variable: assigned once
final class Money { }                // class: no subclasses
class A { final void run() { } }     // method: no overriding
```

## `final` variables

| Where | Meaning |
|-------|---------|
| Local variable | Assigned exactly once |
| Parameter | Cannot be reassigned inside the method |
| Instance field | Assigned once per object (initializer, initializer block, or every constructor) |
| Static field | Assigned once per class (`static final`: a constant) |

```java
void demo(final int n) {
    final int doubled = n * 2;
    // doubled = 5;          // ERROR: cannot assign a value to final variable
    // n = 1;                // ERROR
}

final int x;                 // "blank final": declared now, assigned later (exactly once on every path)
if (flag) x = 1; else x = 2;
```

### Blank final fields

```java
class Order {
    private final long id;                         // must be set in every constructor
    private final List<String> items = new ArrayList<>();   // set at declaration
    Order(long id) { this.id = id; }
}
```

The compiler guarantees that a `final` field is assigned before the constructor finishes ([constructors](./01_constructors.md)). `final` fields also have safe-publication guarantees across threads ([14-concurrency/04_memory-model-and-volatile.md](../14-concurrency/04_memory-model-and-volatile.md)).

## `final` does NOT mean immutable

`final` fixes the **reference** (or primitive value), not the **object** it points to.

```java
final List<String> names = new ArrayList<>();
names.add("Ada");                   // allowed: the list object is modified
// names = new ArrayList<>();       // ERROR: the reference cannot change

final int[] arr = {1, 2, 3};
arr[0] = 99;                        // allowed
// arr = new int[5];                // ERROR

final Person p = new Person("Ada");
p.setName("Grace");                 // allowed
```

```
final List<String> names ──► [ "Ada" ]       the arrow is fixed; the box it points to is not
```

For real immutability: make all fields `final`, avoid setters, use immutable types (`String`, `List.of`, records) and make defensive copies ([23-design-and-clean-code/04_immutability.md](../23-design-and-clean-code/04_immutability.md)).

## Constants: `static final`

```java
public static final int MAX_RETRIES = 3;
public static final String DEFAULT_CHARSET = "UTF-8";
public static final Duration TIMEOUT = Duration.ofSeconds(5);     // immutable object
```

- Naming: `UPPER_SNAKE_CASE`
- A `static final` primitive or `String` assigned a constant expression is a **compile-time constant**; the compiler copies its value into every class that uses it
- Prefer enums for related constants ([11_enums.md](./11_enums.md))
- Avoid "constant interfaces" (interfaces that only hold constants)

## `final` methods

A `final` method cannot be overridden by subclasses.

```java
class Account {
    public final void audit() { ... }          // subclasses cannot change the audit rule
}
class Savings extends Account {
    // public void audit() { }                 // ERROR: cannot override final method
}
```

Use it to protect behavior that must not change, particularly steps of a **template method** ([08_abstract-classes.md](./08_abstract-classes.md)), and for methods called from constructors ([05](./05_initialization-order.md)). `private` and `static` methods are effectively non-overridable already.

## `final` classes

A `final` class cannot be extended.

```java
public final class Money { ... }
// class Dollars extends Money { }             // ERROR
```

| Examples in the JDK | `String`, `Integer` and other wrappers, `Math`, `LocalDate`, records (implicitly `final`) |
|---|---|

| Use `final` classes when | Because |
|--------------------------|---------|
| The class is immutable or security-sensitive | Subclasses could break guarantees |
| The class is not designed for extension | Inheritance without a design is fragile ([06](./06_inheritance.md)) |
| Value types | `equals` stays symmetric ([14](./14_equals-and-hashcode.md)) |

Trade-off: `final` classes cannot be mocked by default in some test frameworks, and users cannot extend them. For controlled hierarchies use **sealed classes** ([12-modern-java/02_sealed-classes.md](../12-modern-java/02_sealed-classes.md)): a class that allows only specific subclasses.

Common guidance (from *Effective Java*): *design for inheritance or prohibit it.* Making classes `final` by default is a good habit.

## Effectively final and lambdas

A local variable that is never reassigned is **effectively final**, even without the keyword. Lambdas and anonymous/local classes can only capture such variables.

```java
int base = 10;                                   // effectively final: assigned once
Runnable r = () -> System.out.println(base);     // OK

int count = 0;
Runnable bad = () -> System.out.println(count);  // ERROR if count is changed anywhere
count++;
```

Workaround for counters: `AtomicInteger`, or a one-element array, or better, restructure with streams ([09-functional-java/00_lambda-expressions.md](../09-functional-java/00_lambda-expressions.md)).

## `final` in other places

```java
for (final String s : names) { ... }       // allowed; the variable is final per iteration
try (final var in = open()) { ... }        // resources are implicitly final
catch (final IOException e) { ... }        // allowed
```

## `final` and performance

`final` is **not** a performance tool. The JIT compiler inlines methods based on observed behavior, with or without `final` ([15-jvm-internals/06_jit-compiler.md](../15-jvm-internals/06_jit-compiler.md)). Use `final` for **design** reasons.

## The keyword soup: `final`, `finally`, `finalize`

| Word | What it is |
|------|------------|
| `final` | Modifier (this file) |
| `finally` | A block that runs after `try`/`catch` ([06-exceptions-and-debugging/01_try-catch-finally.md](../06-exceptions-and-debugging/01_try-catch-finally.md)) |
| `finalize()` | An obsolete `Object` method, deprecated for removal: do not use ([13](./13_object-class.md)) |

## When to use `final`

| Use | Recommendation |
|-----|----------------|
| Fields | **Default to `final`** unless the field must change |
| Constants | Always `static final` |
| Local variables and parameters | Optional; many teams skip the keyword for brevity |
| Methods | When overriding would break correctness |
| Classes | For value types, utilities and anything not designed for extension |

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Believing `final` makes an object immutable | The list or array still changes | Immutable types and defensive copies |
| Forgetting to assign a `final` field in some constructor | `might not have been initialized` | Assign in all paths, or chain with `this(...)` |
| Mutable `static final` collection used as a constant | Callers modify "constants" | `List.of`, `Map.of`, `Collections.unmodifiableX` |
| Reassigning a captured variable used in a lambda | `local variables referenced from a lambda expression must be final or effectively final` | `AtomicInteger`, or restructure |
| Using `final` hoping for speed | No real effect | Remove the expectation |
| Changing a `static final` constant in a shared library | Clients keep the old inlined value | Rebuild clients, or use a method or non-constant |
| Making a class `final` and then needing to mock it | Mock framework errors | Depend on an interface, or use an inline mock maker |
| Declaring non-final public fields | Anyone can change state | `private final` plus accessors |

## Key takeaways

- `final` variable = assigned once; `final` method = no override; `final` class = no subclass
- `final` protects the **reference**, not the object: immutability needs more
- Make fields `final` by default; constants are `static final`
- Lambdas capture only (effectively) final locals
- `final` is a design tool, not an optimization

**Next:** [Encapsulation and Access Modifiers](./04_encapsulation-and-access-modifiers.md)
