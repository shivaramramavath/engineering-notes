# Initialization Order

When does each piece of initialization code run: static blocks, field initializers, instance blocks, constructors, in a class and its parents? Getting this wrong causes subtle bugs (fields that are `null` or `0` "impossibly"), and predicting the printed order is a classic interview question.

## The rules in one picture

```
Class is first used (new, static method call, non-constant static field access)
│
├── 1. Parent class initialization (recursively, top-down)
│       static field initializers and static blocks, in textual order
├── 2. Child class initialization
│       static field initializers and static blocks, in textual order
│
Each time `new Child()` runs:
│
├── 3. Memory allocated, all fields set to defaults (0, false, null)
├── 4. Child constructor is entered; first action is super(...) (explicit or implicit)
│       ├── 5. Parent: instance field initializers + instance blocks (textual order)
│       ├── 6. Parent: constructor body
├── 7. Child: instance field initializers + instance blocks (textual order)
└── 8. Child: constructor body
```

**Static** parts run **once per class**, parent before child. **Instance** parts run **once per object**, parent before child, and within a class the order is: field initializers and instance blocks (as written), then the constructor body.

## Example

```java
class Parent {
    static { System.out.println("1 Parent static block"); }
    private String p = log("3 Parent field");
    { System.out.println("4 Parent instance block"); }
    Parent() { System.out.println("5 Parent constructor"); }
    static String log(String s) { System.out.println(s); return s; }
}

class Child extends Parent {
    static { System.out.println("2 Child static block"); }
    private String c = log("6 Child field");
    { System.out.println("7 Child instance block"); }
    Child() { System.out.println("8 Child constructor"); }
}

public class Main {
    public static void main(String[] args) {
        new Child();
        System.out.println("---");
        new Child();
    }
}
```

Output:

```
1 Parent static block
2 Child static block
3 Parent field
4 Parent instance block
5 Parent constructor
6 Child field
7 Child instance block
8 Child constructor
---
3 Parent field
4 Parent instance block
5 Parent constructor
6 Child field
7 Child instance block
8 Child constructor
```

Static steps (1-2) appear only for the first `new`. The numbers in the labels show the execution order for the first object.

## Within one class: textual order

Initializers and blocks run **in the order they appear in the source**:

```java
class A {
    int a = init("a");
    { init("block1"); }
    int b = init("b");
    { init("block2"); }
    A() { init("constructor"); }
    static int init(String s) { System.out.println(s); return 0; }
}
new A();    // a, block1, b, block2, constructor
```

Same for static fields and `static { }` blocks.

### Forward references

```java
class B {
    int x = y + 1;       // ERROR: illegal forward reference (y declared later, used by simple name)
    int y = 5;

    int z = this.w + 1;  // compiles (qualified), but w is still 0 at this point -> z = 1
    int w = 5;
}
```

Reading a field before its initializer has run gives its **default value**.

## When is a class initialized?

A class is initialized **lazily**, the first time one of these happens:

| Trigger | Initializes |
|---------|-------------|
| `new C()` | `C` (and parents first) |
| Calling a static method of `C` | `C` |
| Reading or writing a **non-constant** static field of `C` | `C` |
| Initializing a subclass | Its superclasses first |
| `Class.forName("C")` | `C` |

These do **not** initialize the class:

| Does not trigger | Why |
|------------------|-----|
| Using a `static final` **compile-time constant** (`static final int MAX = 10;`) | The value is inlined by the compiler |
| Declaring a variable or array of the type (`C[] arr = new C[3]`) | No instance or static access |
| Referring to a parent's static field through a subclass (`Child.parentField`) | Only the declaring class (`Parent`) is initialized |
| `C.class` | Loads but does not initialize |

```java
class Config {
    static final int MAX = 10;                       // constant: inlined
    static final int RANDOM = new Random().nextInt();// not a constant: needs initialization
    static { System.out.println("Config initialized"); }
}
System.out.println(Config.MAX);       // prints 10; no "Config initialized"
System.out.println(Config.RANDOM);    // prints "Config initialized", then the value
```

Loading, linking and initialization in the JVM: [15-jvm-internals/01_class-loading.md](../15-jvm-internals/01_class-loading.md).

## The classic bug: overridable method in a constructor

```java
class Base {
    Base() { System.out.println("Base: " + describe()); }
    String describe() { return "base"; }
}

class Derived extends Base {
    private String name = "derived";                  // runs AFTER Base()
    private final int size;
    Derived() { super(); size = 5; }

    @Override String describe() { return name + ", size=" + size; }
}

new Derived();    // prints: Base: null, size=0
```

Why: step 4-6 (the parent constructor) happens before step 7 (the child's field initializers), so when `Base()` calls the overridden `describe()`, `Derived`'s fields still hold defaults. Even a `final` field can be observed as `0`/`null`.

| Rule | |
|------|--|
| Do not call overridable methods from constructors | `private`, `final` and `static` methods are safe |
| Do not let `this` escape from a constructor | Registering `this` as a listener, starting a thread: other code may see a half-built object |
| Prefer factory methods that finish construction first | Or a separate `init()` step called after `new` |

## Static initialization order pitfalls

### Order of static fields matters

```java
class Order {
    static final Order INSTANCE = new Order();     // runs the constructor now...
    static int counter = 10;                       // ...so counter is still 0 inside the constructor
    Order() { System.out.println("counter = " + counter); }   // prints 0
}
```

Put constants and dependencies **above** code that uses them. This is a well-known singleton gotcha.

### Circular static initialization

```java
class A { static int x = B.y + 1; }
class B { static int y = A.x + 1; }
// One of them sees the other's default value (0), depending on which is touched first
```

Avoid static dependencies between classes.

### Failures in static initializers

```java
class Broken {
    static int value = compute();                  // throws
    static int compute() { throw new RuntimeException("boom"); }
}
new Broken();   // ExceptionInInitializerError (first use)
new Broken();   // NoClassDefFoundError: Could not initialize class Broken (every later use)
```

The class is permanently unusable in that JVM. Keep static initializers simple and failure-proof; load risky resources lazily.

## Instance initializer blocks

```java
class Registry {
    private final List<String> names = new ArrayList<>();
    {
        names.add("default");        // runs before every constructor body
    }
    Registry() { }
    Registry(String extra) { names.add(extra); }
}
```

They are rarely needed. Prefer field initializers for simple values and constructors (with `this(...)` chaining) for logic ([constructors](./01_constructors.md)). A blank `final` field can be assigned in an instance block.

## Checklist: predicting output

1. Find the class that is used first. Initialize its **ancestors first**, then it (static parts, in textual order)
2. For each `new`: allocate with defaults, then walk **down** the chain: parent field initializers/blocks → parent constructor body → child field initializers/blocks → child constructor body
3. Remember a constructor's first action is `super(...)` (or `this(...)`, which delegates and skips its own initializers)
4. Trace overridden method calls using the **runtime** type (polymorphism: [07](./07_polymorphism.md))

With `this(...)`:

```java
class C {
    { System.out.println("block"); }          // runs once, in the constructor that calls super(...)
    C() { this(1); System.out.println("C()"); }
    C(int x) { System.out.println("C(int)"); }
}
new C();     // block, C(int), C()
```

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Calling an overridable method from a constructor | Subclass method sees `null`/`0` fields | Make it `private`/`final`, or use a factory/`init()` |
| Assuming the child's initializers run before the parent's constructor | Unexpected `null` | Parent constructor always runs first |
| Static field order in singletons | Counter or config is `0`/`null` during construction | Declare dependencies first |
| Expecting static blocks to run on every `new` | They run once per class | Use instance blocks or constructors |
| Expecting a class to initialize when you read a constant | Static block never runs | Constants are inlined |
| Heavy work or I/O in static blocks | `ExceptionInInitializerError`, slow startup | Lazy initialization |
| Letting `this` escape from a constructor | Other threads or code see a partly built object | Publish after construction completes |
| Reading a field in its own initializer chain by forward reference | `0` or compile error | Reorder |
| Circular dependencies between classes' statics | Order-dependent defaults | Remove the cycle |

## Key takeaways

- Static parts run **once**, when the class is first used, **parent first**
- For each object: parent initializers and constructor, then the child's, each in textual order (initializers/blocks before the constructor body)
- Fields hold default values until their initializer runs; this is what makes constructor-called overrides dangerous
- Compile-time constants do not trigger class initialization
- Keep initialization simple, order dependencies carefully, and never let `this` escape early

**Next:** [Inheritance](./06_inheritance.md)
