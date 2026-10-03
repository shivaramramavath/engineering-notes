# Constructors

A **constructor** initializes a new object. It runs once, as part of `new`, and should leave the object in a **valid state**.

```java
public class Person {
    private final String name;
    private final int age;

    public Person(String name, int age) {      // constructor: name = class name, no return type
        if (age < 0) throw new IllegalArgumentException("age must be >= 0");
        this.name = name;
        this.age = age;
    }
}

Person p = new Person("Ada", 36);
```

## Rules

| Rule | Detail |
|------|--------|
| Name | Exactly the class name |
| Return type | **None** (not even `void`: `void Person()` is an ordinary method) |
| Called by | `new`, `this(...)`, `super(...)` only; never as a normal method |
| Inherited? | **No**, but a subclass constructor must call one of the parent's |
| Modifiers | Access modifiers allowed (`private`, `protected`, ...); not `static`, `final`, `abstract` |
| May throw | Exceptions, including checked ones (declared with `throws`) |

## The default constructor

If you declare **no** constructor, the compiler adds a public no-argument one.

```java
class Point { int x, y; }
Point p = new Point();          // works: default constructor

class Point2 {
    int x, y;
    Point2(int x, int y) { this.x = x; this.y = y; }
}
new Point2();                   // ERROR: no no-arg constructor any more
```

Once you write **any** constructor, the default one disappears. Add a no-arg constructor explicitly if you still need it (many frameworks do, for example JPA and serialization libraries).

## Overloaded constructors and `this(...)` chaining

Several constructors let callers supply different amounts of information. Funnel them into one **primary** constructor to avoid duplication.

```java
public class Rectangle {
    private final double width, height;

    public Rectangle(double width, double height) {     // primary
        if (width <= 0 || height <= 0) throw new IllegalArgumentException("sides must be positive");
        this.width = width;
        this.height = height;
    }

    public Rectangle(double side) {                      // square
        this(side, side);                                // delegate
    }

    public Rectangle() {                                 // unit rectangle
        this(1);
    }
}
```

- `this(...)` calls another constructor of the **same** class
- Traditionally it must be the **first statement**. Java 25 relaxes this (flexible constructor bodies): you may run statements, such as argument validation, **before** `this(...)`/`super(...)` as long as they do not use `this`
- A constructor cannot call itself, directly or in a cycle

## `super(...)`: the parent constructor

Every constructor starts by calling a parent constructor. If you do not write one, the compiler inserts `super();`.

```java
class Animal {
    protected final String name;
    Animal(String name) { this.name = name; }
}

class Dog extends Animal {
    Dog(String name) {
        super(name);               // must pass what Animal needs
    }
}

class Cat extends Animal {
    Cat() { }                      // ERROR: Animal has no no-arg constructor to call implicitly
}
```

The object is built **top-down**: `Object` → ... → parent → child. See [05_initialization-order.md](./05_initialization-order.md) and [06_inheritance.md](./06_inheritance.md).

## Initialization before the body

Field initializers and instance initializer blocks run **before** the constructor body (after `super(...)`):

```java
class Counter {
    private int count = 10;                // 1. field initializer
    { System.out.println("instance block"); }   // 2. instance initializer block
    Counter() { count++; }                 // 3. constructor body
}
```

Details in [05_initialization-order.md](./05_initialization-order.md).

## Validation: never create invalid objects

```java
public Email(String value) {
    Objects.requireNonNull(value, "value");
    if (!value.contains("@")) throw new IllegalArgumentException("not an email: " + value);
    this.value = value.strip().toLowerCase(Locale.ROOT);
}
```

If the constructor throws, no object is created, and callers never see a half-initialized one. Prefer unchecked exceptions for argument errors ([06-exceptions-and-debugging](../06-exceptions-and-debugging/README.md)).

## `final` fields

A `final` field must be assigned exactly once, in a field initializer, an initializer block, or **every** constructor path.

```java
class Order {
    private final long id;
    private final List<String> items = new ArrayList<>();

    Order(long id) { this.id = id; }               // required: otherwise "variable id might not have been initialized"
}
```

See [03_final.md](./03_final.md).

## Copy constructors

A constructor that takes another instance of the same class.

```java
public Person(Person other) {
    this(other.name, other.age);
}

public Team(Team other) {
    this.name = other.name;
    this.members = new ArrayList<>(other.members);     // copy the list too: deep enough for immutable elements
}
```

Preferred over `clone()`: [15_clone-and-copying.md](./15_clone-and-copying.md).

## `private` constructors

| Use | Example |
|-----|---------|
| Utility class (only static members) | `private MathUtil() {}` so nobody can `new` it |
| Singleton | `private Config() {}` plus a static accessor ([24-design-patterns/01-creational/00_singleton.md](../24-design-patterns/01-creational/00_singleton.md)) |
| Force use of factories or builders | `private Money(...)` + `Money.of(...)` |

```java
public final class Strings {
    private Strings() { throw new AssertionError("no instances"); }
    public static boolean isNullOrBlank(String s) { return s == null || s.isBlank(); }
}
```

## Static factory methods

A named alternative to constructors.

```java
public final class Money {
    private final long cents;
    private Money(long cents) { this.cents = cents; }

    public static Money ofCents(long cents) { return new Money(cents); }
    public static Money ofDollars(double d) { return new Money(Math.round(d * 100)); }
    public static final Money ZERO = new Money(0);
}
```

| Advantage over constructors | Example |
|-----------------------------|---------|
| Descriptive names | `LocalDate.of(...)`, `List.of(...)`, `Integer.valueOf(...)` |
| May return a cached or shared instance | `Boolean.valueOf` |
| May return a subtype | `List.of(...)` returns an internal class |
| Can fail or return `Optional` more naturally | `Optional.ofNullable` |

For classes with many optional parameters use a **builder** ([24-design-patterns/01-creational/03_builder.md](../24-design-patterns/01-creational/03_builder.md)). For simple data carriers use a **record**, which generates the constructor for you ([12-modern-java/01_records.md](../12-modern-java/01_records.md)).

## Do not call overridable methods in a constructor

```java
class Base {
    Base() { describe(); }                // calls an overridable method
    void describe() { }
}
class Derived extends Base {
    private String label = "ready";
    @Override void describe() { System.out.println(label); }    // prints null!
}
new Derived();
```

The parent constructor runs before `Derived`'s field initializers, so the override sees default values. Call only `private`, `final` or `static` methods from constructors ([05](./05_initialization-order.md)).

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Adding a constructor and losing the default | `constructor X in class X cannot be applied` | Add a no-arg one if needed |
| `void` before the constructor name | Never runs on `new` | Remove `void` |
| `name = name;` | Field not set | `this.name = name;` |
| Subclass without a matching `super(...)` | `no suitable constructor found` | Call `super(args)` |
| Duplicated initialization logic across constructors | Inconsistencies | Chain with `this(...)` |
| Skipping validation | Invalid objects in the system | Validate and throw |
| Storing a caller's mutable argument directly | Outside code can change your state | Defensive copy ([pass-by-value](../02-methods/01_pass-by-value.md)) |
| Overridable method call in a constructor | Sees default values | Do not do it |
| Doing heavy work or I/O in constructors | Hard to test, slow, may fail halfway | Use factories or separate `init` steps |
| Instantiable utility classes | Pointless instances | `private` constructor |

## Key takeaways

- A constructor has the class name and no return type; it runs once per `new`
- The compiler supplies a no-arg constructor only if you declare none
- Chain with `this(...)` to centralize logic; subclasses must call a parent constructor with `super(...)`
- Validate arguments and assign `final` fields so objects are always valid
- Use static factories or builders when plain constructors become unclear

**Next:** [Static](./02_static.md)
