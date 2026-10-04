# Abstract Classes

**Abstraction** is the idea of exposing *what* something does while hiding *how*. Java provides two mechanisms: **abstract classes** (this file) and **interfaces** ([next file](./09_interfaces.md)). An abstract class is a class that cannot be instantiated and may leave some methods **unimplemented** for subclasses to complete.

```java
abstract class Shape {
    private final String name;

    Shape(String name) { this.name = name; }          // constructors are allowed

    abstract double area();                           // no body: subclasses must implement

    String describe() {                               // concrete method, shared by all shapes
        return name + " with area " + String.format("%.2f", area());
    }
}

class Circle extends Shape {
    private final double r;
    Circle(double r) { super("Circle"); this.r = r; }
    @Override double area() { return Math.PI * r * r; }
}

Shape s = new Circle(2);
s.describe();                // Circle with area 12.57
// new Shape("x");           // ERROR: Shape is abstract; cannot be instantiated
```

## Rules

| Rule | Detail |
|------|--------|
| `abstract class` | Cannot be instantiated with `new` (but may have constructors, called via `super(...)`) |
| `abstract` method | Declaration only: `abstract double area();` (no body, ends with `;`) |
| A class with an abstract method | Must itself be `abstract` |
| Subclass | Must implement **all** inherited abstract methods, or be `abstract` too |
| An abstract class may have | Fields, concrete methods, constructors, static members, nested types, **no** abstract methods at all |
| Invalid combinations | `abstract` + `final` (nobody could implement it), `abstract` + `private` (cannot be overridden), `abstract` + `static` |
| Access | Abstract methods can be `public` or `protected` or package-private, never `private` |

You can still declare variables and parameters of an abstract type (`Shape s`): they hold instances of concrete subclasses. That is how polymorphism works ([07](./07_polymorphism.md)).

## Abstract vs concrete: a spectrum

```
interface            abstract class                concrete class
(pure contract)      (partial implementation)      (complete)
 no state            state + some behavior         everything implemented
```

An abstract class can be a **skeleton** that implements the common 80% and leaves the varying 20% to subclasses.

## Template Method: the main use

The abstract class defines the **algorithm's skeleton** in a `final` method and delegates the variable steps to abstract methods.

```java
abstract class ReportGenerator {
    public final String generate() {          // fixed algorithm: final so subclasses cannot reorder it
        StringBuilder sb = new StringBuilder();
        sb.append(header()).append('\n');
        for (String row : rows()) sb.append(row).append('\n');
        sb.append(footer());
        return sb.toString();
    }

    protected abstract List<String> rows();                 // must vary
    protected String header() { return "=== Report ==="; }  // optional hook with a default
    protected String footer() { return "=== End ==="; }
}

class SalesReport extends ReportGenerator {
    @Override protected List<String> rows() { return List.of("Q1: 100", "Q2: 150"); }
}
```

This is the **Template Method pattern**: [24-design-patterns/03-behavioral/03_template-method.md](../24-design-patterns/03-behavioral/03_template-method.md). Frameworks use it everywhere (servlets, JDBC templates, test base classes). Using `final` on the template and `protected` on the hooks is good design ([03_final.md](./03_final.md)).

## Abstract class vs interface

| | Abstract class | Interface |
|---|----------------|-----------|
| Instantiate | No | No |
| Inheritance | A class extends **one** | A class implements **many** |
| Instance state (fields) | Yes | No (only `public static final` constants) |
| Constructors | Yes | No |
| Methods | Abstract and concrete (any access level) | Abstract, `default`, `static`, `private` (methods are `public` by default) |
| Relationship expressed | **is-a** with shared implementation | **can-do** capability or contract |
| Best for | Closely related classes sharing code and state | Unrelated types sharing behavior, API contracts, multiple "types" |
| Evolution | Add concrete methods freely | Add `default` methods |

**Guideline:** start with an **interface** for the contract. Add an **abstract class** that implements the interface to supply shared code (skeletal implementation), as the JDK does with `List` and `AbstractList`:

```java
interface Notifier { void send(String to, String message); }

abstract class BaseNotifier implements Notifier {
    @Override public final void send(String to, String message) {
        validate(to);
        deliver(to, message);
        log(to);
    }
    protected abstract void deliver(String to, String message);
    private void validate(String to) { if (to == null || to.isBlank()) throw new IllegalArgumentException("to"); }
    private void log(String to) { System.out.println("sent to " + to); }
}
```

Details in [09_interfaces.md](./09_interfaces.md).

## Constructors in abstract classes

They exist and run (via `super(...)`) to initialize the abstract class's own fields. They should not call abstract methods ([05](./05_initialization-order.md)).

```java
abstract class Employee {
    private final String name;
    private final double baseSalary;
    protected Employee(String name, double baseSalary) { this.name = name; this.baseSalary = baseSalary; }
    abstract double pay();
}
```

## Anonymous subclasses and lambdas

You can instantiate an abstract class by creating an **anonymous subclass** right at the use site:

```java
Shape unit = new Shape("Unit square") {
    @Override double area() { return 1; }
};
```

For interfaces with one method, a lambda is simpler. Abstract classes cannot be lambda targets ([12](./12_nested-and-inner-classes.md), [09-functional-java/00_lambda-expressions.md](../09-functional-java/00_lambda-expressions.md)).

## Abstraction in practice

| Level | Example |
|-------|---------|
| Method | `sort(list)` hides the algorithm |
| Class | `ArrayList` hides the array and resizing |
| Type | `List` hides which implementation is used |
| Module / API | A public interface hides the packages behind it |

Good abstractions are **small, stable and honest**: a caller should not need to know implementation details, and the abstraction should not leak them (for example, by throwing implementation-specific exceptions).

## When to use an abstract class

| Use it when | Do not use it when |
|-------------|--------------------|
| Subclasses share fields **and** behavior | Only a contract is needed (use an interface) |
| You want a template method with enforced steps | The classes are unrelated |
| You need constructors or non-public members in the base | You need multiple inheritance of type |
| You want a skeletal implementation of an interface | A simple `final` class or record suffices |

Sealed hierarchies ([12-modern-java/02_sealed-classes.md](../12-modern-java/02_sealed-classes.md)) are the modern alternative when the set of subclasses is **closed** and known.

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| `new` on an abstract class | `is abstract; cannot be instantiated` | Instantiate a concrete subclass |
| Forgetting to implement an abstract method | `is not abstract and does not override abstract method` | Implement it, or declare the class `abstract` |
| `abstract` + `private`, `final` or `static` | Compile error | Remove the conflicting modifier |
| Calling an abstract method in the abstract class's constructor | Subclass fields still default | Avoid; use a template method or init step |
| Using an abstract class where an interface fits | Locks users into single inheritance | Interface + optional skeletal abstract class |
| Making everything abstract "for flexibility" | Over-engineering | Start concrete; abstract when real variation appears |
| Deep chains of abstract classes | Hard to follow | Prefer composition ([10](./10_composition-and-object-relationships.md)) |
| Non-`final` template method | Subclasses break the algorithm | `final` on the skeleton |
| Adding state to the abstract class that subclasses must keep consistent | Fragile coupling | Keep it `private`, expose protected accessors |

## Key takeaways

- Abstraction hides details behind a contract; Java offers abstract classes and interfaces
- An abstract class cannot be instantiated, may mix abstract and concrete members, and has constructors and state
- Subclasses must implement all abstract methods or stay abstract
- Template Method is the classic use: a `final` skeleton plus abstract steps
- Prefer interfaces for contracts; add an abstract class when you also need shared code or state

**Next:** [Interfaces](./09_interfaces.md)
