# Composition and Object Relationships

Objects rarely work alone. This file covers how classes relate (**dependency, association, aggregation, composition**), and why **composition over inheritance** is one of the most useful design guidelines in OOP.

```
is-a   (inheritance)    Dog  ──────▷  Animal         "a Dog is an Animal"
has-a  (composition)    Car  ◆────── Engine          "a Car has an Engine"
uses   (dependency)     Printer - - ▷ Document       "a Printer uses a Document"
```

## The relationships

| Relationship | Meaning | Lifetime | Example |
|--------------|---------|----------|---------|
| **Dependency** (uses) | One class uses another temporarily (parameter, local variable, return type) | Only during a method call | `OrderService.process(Order o)` |
| **Association** | A long-lived reference between objects; neither owns the other | Independent | `Student` ↔ `Course` |
| **Aggregation** | "has-a", a whole refers to parts that can **exist without** it | Parts outlive the whole | `Department` has `Professor`s |
| **Composition** | "has-a", the whole **owns** its parts; parts die with it | Part's lifetime = whole's | `Order` has `OrderLine`s |

The distinction between aggregation and composition is about **ownership and lifecycle**, not syntax: in Java both are a field holding a reference.

### Dependency

```java
class ReportPrinter {
    void print(Report report, PrintStream out) {      // uses Report and PrintStream, stores neither
        out.println(report.title());
    }
}
```

### Association

```java
class Student {
    private final List<Course> courses = new ArrayList<>();     // many-to-many with Course
    void enroll(Course c) { courses.add(c); }
}
class Course { private final List<Student> students = new ArrayList<>(); }
```

Associations can be **one-to-one**, **one-to-many**, **many-to-many** (multiplicity), and **unidirectional** or **bidirectional**. Bidirectional links must be kept consistent on both sides, which is why you prefer one direction unless you need both.

### Aggregation

```java
class Professor { /* exists on its own */ }

class Department {
    private final List<Professor> professors = new ArrayList<>();
    void add(Professor p) { professors.add(p); }       // parts are created elsewhere and passed in
}
// Deleting the Department does not delete the Professors
```

### Composition

```java
class Order {
    private final List<OrderLine> lines = new ArrayList<>();

    void addLine(String product, int qty, long unitCents) {
        lines.add(new OrderLine(product, qty, unitCents));        // the Order creates and owns its parts
    }
    List<OrderLine> lines() { return List.copyOf(lines); }        // do not leak the owned parts
}

class OrderLine { /* meaningless without an Order */ }
```

Signs of composition: the whole **creates** the parts, parts are not shared, and parts are not exposed in a way that lets outsiders retain or modify them.

```
Order ◆───────── OrderLine        filled diamond = composition (owns)
Department ◇──── Professor        hollow diamond = aggregation (refers to)
Student ───────── Course          plain line     = association
```

These are UML notations; you do not need UML to apply the ideas.

## Composition over inheritance

Inheritance couples a subclass tightly to its parent's implementation ([06](./06_inheritance.md)). Composition builds new behavior by **holding other objects and delegating to them**.

### Example: a stack that has a list

```java
// Inheritance: exposes ALL ArrayList methods and breaks stack discipline
class BadStack<E> extends ArrayList<E> { /* push, pop */ }

// Composition: exposes only what a stack should
class Stack<E> {
    private final Deque<E> items = new ArrayDeque<>();     // has-a
    public void push(E e) { items.addFirst(e); }
    public E pop() { return items.removeFirst(); }
    public boolean isEmpty() { return items.isEmpty(); }
}
```

### Delegation / forwarding

```java
class CountingSet<E> implements Set<E> {                    // wraps any Set
    private final Set<E> delegate;
    private int added = 0;
    CountingSet(Set<E> delegate) { this.delegate = delegate; }

    @Override public boolean add(E e) { added++; return delegate.add(e); }
    @Override public boolean addAll(Collection<? extends E> c) {
        added += c.size();
        return delegate.addAll(c);              // no double counting: the delegate's internals do not matter
    }
    // ... remaining Set methods forward to delegate
    int addedCount() { return added; }
}
```

This is the **Decorator** pattern ([24-design-patterns/02-structural/01_decorator.md](../24-design-patterns/02-structural/01_decorator.md)): it works with **any** `Set` implementation and cannot be broken by changes inside the wrapped class.

### Swap behavior at runtime: Strategy

```java
interface DiscountPolicy { long apply(long cents); }

class Checkout {
    private DiscountPolicy policy;                         // composed behavior
    Checkout(DiscountPolicy policy) { this.policy = policy; }
    long total(long cents) { return policy.apply(cents); }
}

new Checkout(c -> c * 90 / 100);       // 10% off
new Checkout(c -> c);                  // no discount
```

With inheritance you would need a subclass per policy, fixed at compile time ([24-design-patterns/03-behavioral/00_strategy.md](../24-design-patterns/03-behavioral/00_strategy.md)).

### Why prefer composition

| Inheritance | Composition |
|-------------|-------------|
| Fixed at compile time | Can change at runtime (swap a collaborator) |
| Exposes the parent's whole API | Exposes only what you choose |
| Depends on the parent's implementation (fragile base class) | Depends only on the collaborator's **interface** |
| One parent only | Combine any number of components |
| Hard to test in isolation | Collaborators can be replaced with test doubles |

**When inheritance is still right:** a real, stable is-a relationship with substitutability, or a class designed for extension (template method).

### Combine them: interface + composition

```java
interface Engine { void start(); }
class PetrolEngine implements Engine { public void start() { /* ... */ } }
class ElectricEngine implements Engine { public void start() { /* ... */ } }

class Car {
    private final Engine engine;                  // depends on the abstraction
    Car(Engine engine) { this.engine = engine; }  // injected: dependency injection
    void start() { engine.start(); }
}
```

## Designing relationships well

| Guideline | Why |
|-----------|-----|
| Prefer **unidirectional** associations | Avoid two-way consistency bugs |
| Make ownership explicit: the owner creates, stores and (if exposed) copies its parts | Clear lifecycles |
| Do not leak owned parts (`return List.copyOf(parts)`) | Keeps invariants ([04](./04_encapsulation-and-access-modifiers.md)) |
| Inject collaborators through constructors | Testable, explicit dependencies |
| Depend on interfaces, not classes | Loose coupling |
| **Law of Demeter**: talk to your direct collaborators, not their internals (`a.getB().getC().doIt()` is a smell) | Reduces ripple effects ([23-design-and-clean-code/01_design-principles.md](../23-design-and-clean-code/01_design-principles.md)) |
| Avoid circular dependencies between classes or packages | They make code hard to test and reuse |
| Keep classes **cohesive** (one responsibility) and **loosely coupled** | The goal behind all of the above |

## Relationships in persistence

The same ideas appear when mapping objects to databases: composition corresponds to cascade delete, associations to foreign keys, many-to-many to join tables ([16-jdbc-and-databases/07_from-jdbc-to-orm.md](../16-jdbc-and-databases/07_from-jdbc-to-orm.md)).

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Using inheritance for code reuse | Broken invariants, rigid hierarchies | Compose and delegate |
| Exposing owned collections directly | Outsiders mutate internal state | Copies or unmodifiable views |
| Bidirectional associations without maintaining both sides | Inconsistent state, memory leaks | One direction, or update both in one method |
| Creating collaborators with `new` inside the class | Cannot substitute in tests | Constructor injection |
| Train-wreck calls (`a.getB().getC().run()`) | Fragile code | Ask `a` to do the job |
| Confusing aggregation and composition in design | Wrong delete/lifecycle behavior | Decide who owns whom |
| Circular references between objects with ownership | Hard to build, copy or serialize | Break the cycle |
| Deep hierarchies where a few collaborators would do | Hard to change | Replace inheritance with composition |
| Delegating by copying parent code | Duplication | Forward to the delegate |

## Key takeaways

- Four relationships: dependency (uses), association (knows), aggregation (has, shared), composition (owns)
- Composition = hold collaborators and delegate; it is flexible, testable and avoids fragile base classes
- Use inheritance for true is-a and designed-for-extension cases; otherwise prefer interfaces + composition
- Own what you create, hide what you own, inject what you depend on

**Next:** [Enums](./11_enums.md)
