# Interfaces

An **interface** defines a **contract**: a set of methods a type promises to provide, without saying how. A class that `implements` an interface must provide those methods. Interfaces let unrelated classes be used interchangeably and are the main tool for decoupling code.

```java
public interface Payable {
    double amount();                         // implicitly public abstract
}

public class Invoice implements Payable {
    private final double total;
    public Invoice(double total) { this.total = total; }
    @Override public double amount() { return total; }
}

public class Employee implements Payable {
    @Override public double amount() { return 5000; }
}

static double sum(List<? extends Payable> items) {
    return items.stream().mapToDouble(Payable::amount).sum();    // works for any Payable
}
```

`Invoice` and `Employee` are unrelated by inheritance, yet both are `Payable`.

## What an interface can contain

| Member | Modifiers (implicit) | Since | Purpose |
|--------|----------------------|-------|---------|
| Abstract method | `public abstract` | 1.0 | The contract |
| Constant field | `public static final` | 1.0 | Shared constants |
| `default` method | `public` | Java 8 | Method with a body, inherited by implementors (can be overridden) |
| `static` method | `public` | Java 8 | Utility method on the interface itself |
| `private` method | | Java 9 | Share code between default methods |
| Nested type | `public static` | 1.0 | Related enums, records, interfaces |

```java
public interface Greeter {
    String PREFIX = "Hello, ";                                 // constant

    String name();                                              // abstract

    default String greet() {                                    // default method
        return PREFIX + decorate(name());
    }

    private String decorate(String s) { return s.strip(); }     // private helper (Java 9+)

    static Greeter of(String name) { return () -> name; }       // static factory (Java 8+)
}
```

**No** instance fields, constructors or instance initializers.

## Implementing interfaces

```java
class Report implements Printable, Exportable, Comparable<Report> { ... }   // many interfaces: OK
interface Archivable extends Printable, Exportable { }                      // interface extends interfaces
```

- A class `extends` **one** class and `implements` **any number** of interfaces: this is Java's form of multiple inheritance (of **type** and, via default methods, of behavior)
- Implementing methods must be `public` (interface methods are public, and you cannot reduce access)
- A non-abstract class must implement all abstract methods

## Programming to an interface

Declare variables, parameters and return types using the **interface**, not the implementation.

```java
List<String> names = new ArrayList<>();       // not ArrayList<String>
Map<String, Integer> counts = new HashMap<>();

void process(Collection<String> input) { }    // accepts List, Set, Queue ...
```

You can change `ArrayList` to `LinkedList` in one place, accept more argument types, and substitute test doubles. This is the **Dependency Inversion Principle** ([23-design-and-clean-code/00_solid-principles.md](../23-design-and-clean-code/00_solid-principles.md), [24-design-patterns/04-architecture/00_dependency-injection.md](../24-design-patterns/04-architecture/00_dependency-injection.md)).

## Default methods

Added so interfaces could **evolve** without breaking existing implementors (for example, `Collection.stream()`, `List.sort()`, `Map.getOrDefault()`).

```java
public interface Shape {
    double area();
    default String describe() { return "Shape with area " + area(); }
}
class Circle implements Shape {
    public double area() { return 3.14; }
    // describe() inherited; may be overridden
}
```

### The diamond problem

If two interfaces provide the same default method, the implementing class **must** resolve the conflict:

```java
interface A { default String hello() { return "A"; } }
interface B { default String hello() { return "B"; } }

class C implements A, B {
    @Override public String hello() {
        return A.super.hello() + B.super.hello();      // choose explicitly: "AB"
    }
}
```

Resolution rules:
1. A **class** method (declared or inherited) always wins over interface defaults
2. Otherwise the **most specific** interface wins (a sub-interface overrides its parent's default)
3. Otherwise the class must override and may call `X.super.method()`

Default methods cannot override `Object` methods (`equals`, `hashCode`, `toString`).

## Static interface methods

```java
interface Validator {
    boolean isValid(String s);

    static Validator notBlank() { return s -> s != null && !s.isBlank(); }
}
Validator.notBlank().isValid("x");     // call through the interface name; not inherited
```

## Functional interfaces

An interface with **exactly one abstract method** can be implemented with a **lambda** or method reference:

```java
@FunctionalInterface
interface Discount { double apply(double price); }

Discount tenPercent = price -> price * 0.9;
tenPercent.apply(200);                  // 180.0
```

JDK examples: `Runnable`, `Comparator`, `Function`, `Predicate`. See [09-functional-java/01_functional-interfaces.md](../09-functional-java/01_functional-interfaces.md).

## Marker interfaces

Interfaces with no methods that tag a type: `Serializable`, `Cloneable`, `RandomAccess`. Annotations are usually a better modern choice for new tags ([13-advanced-language-features/00_annotations.md](../13-advanced-language-features/00_annotations.md)).

## Sealed interfaces

Restrict which types may implement an interface ([12-modern-java/02_sealed-classes.md](../12-modern-java/02_sealed-classes.md)):

```java
public sealed interface Shape permits Circle, Square { }
```

## Interface vs abstract class

| | Interface | Abstract class |
|---|-----------|----------------|
| State | Constants only | Instance fields |
| Constructors | No | Yes |
| Inheritance | Implement many | Extend one |
| Method access | `public` (plus `private` helpers) | Any |
| Typical meaning | **can-do** / contract | **is-a** with shared code |

Rule of thumb: **default to an interface**. Add an abstract skeleton class if implementations share code ([08](./08_abstract-classes.md)).

## Design guidelines

| Guideline | Why |
|-----------|-----|
| Keep interfaces **small and focused** (Interface Segregation) | Implementors are not forced to stub unused methods |
| Name by role or capability: `Comparable`, `Closeable`, `PaymentGateway` | Clear contract. Do not prefix with `I` (Java convention) |
| Document the **contract**: preconditions, postconditions, exceptions, thread-safety | Different implementations must behave consistently |
| Add methods to a published interface with `default` bodies | Adding an abstract method breaks all implementors |
| Do not use interfaces only for constants | "Constant interface" anti-pattern: use a class or enum |
| One implementation only? Maybe skip the interface for now | Extract it when a second implementation or a test substitute appears |
| Avoid exposing implementation types in signatures | Keeps the API stable |

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Implementing a method without `public` | `attempting to assign weaker access privileges` | Add `public` |
| Forgetting an abstract method | `is not abstract and does not override...` | Implement it |
| Two interfaces with the same default method | `inherits unrelated defaults` | Override and choose with `X.super.m()` |
| Trying to add fields or constructors to an interface | Compile error (fields become constants) | Use an abstract class |
| Huge "god" interfaces | Painful implementations | Split by responsibility |
| Declaring variables as concrete classes | Tight coupling | Use the interface type |
| Adding an abstract method to a published interface | Breaks every implementor | `default` method |
| Expecting an interface default method to override `toString` | Compile error | Implement in the class |
| Overusing default methods for logic | State-less "mixin" confusion | Keep them thin, delegate to abstract methods |
| Interface for everything | Needless indirection | Add interfaces where variation or substitution is real |

## Key takeaways

- An interface is a contract; classes `implements` many, `extends` one
- Members: abstract methods, constants, `default`, `static` and `private` methods; no instance state
- Program to interfaces; use default methods for evolution and `X.super.m()` to resolve conflicts
- A single-abstract-method interface is a functional interface and works with lambdas
- Default to interfaces for contracts; combine with abstract skeleton classes for shared code

**Next:** [Composition and Object Relationships](./10_composition-and-object-relationships.md)
