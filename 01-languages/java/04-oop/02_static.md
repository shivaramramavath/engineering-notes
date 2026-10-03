# Static

A `static` member belongs to the **class itself**, not to any object. There is exactly one copy, shared by all instances, and it exists even if no instance has been created.

```java
public class Counter {
    private static int total = 0;      // shared by all Counter objects
    private int mine = 0;              // one per object

    void hit() { mine++; total++; }
    static int getTotal() { return total; }
}

Counter a = new Counter(), b = new Counter();
a.hit(); a.hit(); b.hit();
Counter.getTotal();                    // 3 (call through the class name)
```

```
  Class Counter (one copy)            Objects (one copy each)
  ┌───────────────────┐               ┌─────────┐   ┌─────────┐
  │ static total = 3  │ ◄── shared ── │ mine = 2│   │ mine = 1│
  └───────────────────┘               │  (a)    │   │  (b)    │
                                      └─────────┘   └─────────┘
```

## Static vs instance

| | Instance member | Static member |
|---|-----------------|---------------|
| Belongs to | An object | The class |
| Copies | One per object | One total |
| Access | `obj.member` | `ClassName.member` |
| Needs an object? | Yes | No |
| Can use `this`? | Yes | **No** |
| Can access instance members directly? | Yes | **No** (needs an object reference) |
| Can access static members? | Yes | Yes |

```java
class Demo {
    int x = 1;
    static int y = 2;

    static void s() {
        System.out.println(y);        // OK
        // System.out.println(x);     // ERROR: non-static variable x cannot be referenced from a static context
        System.out.println(new Demo().x);   // OK: through an object
    }
    void i() { System.out.println(x + y); }   // instance code sees both
}
```

`main` is `static`, which is why calling instance methods from it needs an object ([methods](../02-methods/00_methods.md)).

## Static fields and constants

```java
public class Config {
    public static final int MAX_USERS = 100;               // constant: static + final, UPPER_SNAKE_CASE
    public static final List<String> LANGS = List.of("en", "de");   // immutable collection
    private static int instances;                           // shared mutable state: use with care
}
```

- `static final` primitives and strings initialized with constants are **compile-time constants** and are inlined into callers. If you change one in a library, dependent code must be recompiled to see the new value ([03_final.md](./03_final.md))
- A `static final` reference to a **mutable** object is still mutable (`static final List<String> L = new ArrayList<>()` can be modified)

## Static methods

Use for behavior that does not depend on instance state: utilities, factories, pure computations.

```java
Math.max(3, 5);
Integer.parseInt("42");
List.of(1, 2, 3);
LocalDate.now();
```

| Properties | |
|------------|--|
| No access to `this` or `super` | They are not tied to an object |
| **Not overridden**, only hidden | Resolved at compile time by the declared type ([06](./06_inheritance.md), [07](./07_polymorphism.md)) |
| Cannot be `abstract` | No override, no abstract |
| Hard to mock in tests | Prefer instance methods behind an interface when behavior needs substituting ([19-testing](../19-testing/README.md)) |

### Calling a static method through an instance

```java
Counter c = null;
c.getTotal();        // legal and works: resolved by the declared type, no NullPointerException
```

It compiles but is misleading. Always use `ClassName.method()`.

## Static initializer blocks

Run **once**, when the class is initialized, in textual order with static field initializers.

```java
public class Registry {
    static final Map<String, Integer> CODES = new HashMap<>();

    static {                                   // complex setup that does not fit one expression
        CODES.put("A", 1);
        CODES.put("B", 2);
    }
}
```

When initialization happens, and the interplay with instance initialization: [05_initialization-order.md](./05_initialization-order.md). An exception in a static block surfaces as `ExceptionInInitializerError`, and later uses of the class fail with `NoClassDefFoundError`.

## Utility classes

```java
public final class StringUtil {
    private StringUtil() {}                      // prevent instances
    public static boolean isBlank(String s) { return s == null || s.isBlank(); }
}
```

Keep them small and stateless. A growing `Utils` class is a smell: move behavior to the type it belongs to.

## Static imports

```java
import static java.lang.Math.max;
import static java.lang.Math.PI;

double r = max(1, 2) * PI;
```

Use sparingly (constants, test assertions); overuse hides where names come from ([05-packages-and-modules/00_packages-and-imports.md](../05-packages-and-modules/00_packages-and-imports.md)).

## Static nested classes and interface members

- A `static` nested class does not need an outer instance ([12](./12_nested-and-inner-classes.md))
- Interfaces may declare `static` methods (called as `InterfaceName.method()`), and their fields are implicitly `static final` ([09](./09_interfaces.md))

## Static and inheritance: hiding

```java
class Parent { static String who() { return "parent"; } }
class Child extends Parent { static String who() { return "child"; } }   // hides, not overrides

Parent p = new Child();
p.who();               // "parent": chosen by the declared type Parent
Child.who();           // "child"
```

Static methods and fields are bound at compile time: **no polymorphism**.

## When static is the wrong choice

| Problem | Why |
|---------|-----|
| **Global mutable state** (`static` counters, caches, config) | Hidden coupling; tests interfere with each other |
| **Thread safety** | Shared mutable statics need synchronization ([14-concurrency](../14-concurrency/README.md)) |
| **Memory leaks** | A static collection holds references forever |
| **Testability** | Static calls cannot be replaced by test doubles easily |
| **Singletons everywhere** | Hard dependencies: use dependency injection ([24-design-patterns/04-architecture/00_dependency-injection.md](../24-design-patterns/04-architecture/00_dependency-injection.md)) |

Good static: `Math`, `Collections`, `Objects`, pure functions, factory methods, constants. Questionable static: anything that changes over time.

## Lifecycle: when does static state exist?

- Static fields live as long as the **class** is loaded (normally the lifetime of the application, or its class loader)
- The class is initialized lazily, on first active use (first `new`, static method call, or non-constant static field access) ([15-jvm-internals/01_class-loading.md](../15-jvm-internals/01_class-loading.md))

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Using an instance field in a static method | `non-static variable cannot be referenced` | Pass an object, or make the method an instance method |
| Making everything `static` to avoid creating objects | Procedural code, no polymorphism, hard to test | Design with objects |
| Mutable static collections | Leaks, race conditions | Immutable constants, or inject dependencies |
| Calling statics through an instance | Misleading code | `ClassName.method()` |
| Expecting static methods to be overridden | Parent version runs | Use instance methods |
| Changing a `static final` constant in a library without recompiling clients | Clients still use the old value | Recompile, or avoid constants that may change |
| Static initializer that can throw | `ExceptionInInitializerError`, class unusable | Keep it simple; handle failures |
| Utility class without a private constructor | Pointless instances | `private` constructor |
| Static counters used for IDs in multi-threaded code | Duplicate IDs | `AtomicLong` ([14-concurrency/06_atomic-classes.md](../14-concurrency/06_atomic-classes.md)) |

## Key takeaways

- `static` = belongs to the class; one copy, no `this`, access via `ClassName`
- Static code cannot touch instance members directly; instance code can use statics
- Static methods are hidden, not overridden: no runtime polymorphism
- Good for constants, utilities and factories; bad for shared mutable state
- Static initializers run once, when the class is first used

**Next:** [Final](./03_final.md)
