# Inheritance

**Inheritance** lets a class (the **subclass**) reuse and specialize another class (the **superclass**) with `extends`. It models an **is-a** relationship: a `Dog` *is an* `Animal`.

```java
class Animal {
    protected final String name;
    Animal(String name) { this.name = name; }
    void eat() { System.out.println(name + " eats"); }
    String sound() { return "..."; }
}

class Dog extends Animal {
    Dog(String name) { super(name); }
    @Override String sound() { return "Woof"; }       // specialize
    void fetch() { System.out.println(name + " fetches"); }   // add
}
```

```
        Animal           ◄── superclass (parent, base class)
       /      \
     Dog      Cat       ◄── subclasses (children, derived classes)
```

```java
Dog d = new Dog("Rex");
d.eat();                 // inherited from Animal
d.sound();               // overridden: "Woof"
d.fetch();               // Dog's own
Animal a = d;            // a Dog can be used wherever an Animal is expected
```

## What is inherited

| Inherited | Not inherited |
|-----------|---------------|
| `public` and `protected` members | `private` members (they exist in the object but are not accessible by name) |
| Package-private members (same package only) | **Constructors** |
| Default methods from interfaces | Static members are accessible but **hidden**, not overridden |

Every class extends exactly one superclass (**single inheritance**); if you write none, it is `Object` ([13_object-class.md](./13_object-class.md)). A class can implement **many interfaces** ([09](./09_interfaces.md)).

## Constructors and `super(...)`

A subclass constructor must call a superclass constructor first; if you do not write one, the compiler inserts `super();`.

```java
class Dog extends Animal {
    Dog(String name) { super(name); }        // required: Animal has no no-arg constructor
}
```

Objects are built from the top down, and initialization order is covered in [05_initialization-order.md](./05_initialization-order.md).

## Overriding

A subclass provides its own implementation of an inherited **instance** method.

```java
class Shape { double area() { return 0; } }
class Circle extends Shape {
    private final double r;
    Circle(double r) { this.r = r; }
    @Override
    double area() { return Math.PI * r * r; }
}
```

### Overriding rules

| Rule | Detail |
|------|--------|
| Same name and parameter types | Different parameters is **overloading**, not overriding ([02-methods](../02-methods/02_overloading-and-varargs.md)) |
| Return type | Same, or a **subtype** (covariant return) |
| Access | Same or **wider** (`protected` → `public` is fine; narrowing is an error) |
| Checked exceptions | Cannot throw **new or broader** checked exceptions |
| `final`, `static`, `private` methods | `final` cannot be overridden; `static` is hidden; `private` is not visible |
| `@Override` | Optional but **always use it**: the compiler checks you really override |

```java
class Parent { Number value() { return 1; } }
class Child extends Parent {
    @Override Integer value() { return 2; }      // covariant return: Integer is a Number
}

class A { void run() { } }
class B extends A {
    @Override
    void runn() { }       // ERROR with @Override: method does not override (typo caught)
}
```

### Calling the parent version: `super.method()`

```java
class Dog extends Animal {
    @Override
    void eat() {
        super.eat();                       // reuse the parent behavior
        System.out.println("and wags tail");
    }
}
```

## Field hiding (do not do it)

Fields are **not** overridden; a field with the same name **hides** the parent's. Access depends on the **declared** type.

```java
class P { String name = "parent"; }
class C extends P { String name = "child"; }

P x = new C();
x.name;            // "parent"  (static type)
((C) x).name;      // "child"
```

Do not redeclare inherited fields.

## Static methods are hidden, not overridden

```java
class P { static String who() { return "P"; } }
class C extends P { static String who() { return "C"; } }
P x = new C();
x.who();           // "P": no runtime dispatch for static methods
```

See [02_static.md](./02_static.md) and [07_polymorphism.md](./07_polymorphism.md).

## `protected`

Visible to subclasses (even in other packages) and the same package. Treat it as part of your public API: subclass authors depend on it ([encapsulation](./04_encapsulation-and-access-modifiers.md)).

## Preventing inheritance and controlling it

| Tool | Effect |
|------|--------|
| `final class` | No subclasses ([03](./03_final.md)) |
| `final` method | Cannot be overridden |
| `sealed class ... permits A, B` | Only listed subclasses ([12-modern-java/02_sealed-classes.md](../12-modern-java/02_sealed-classes.md)) |
| Package-private constructor | Only classes in the package can extend |

## Type relationships

```java
Dog d = new Dog("Rex");
d instanceof Animal;         // true: a Dog is an Animal
Animal a = d;                // upcast: implicit and always safe
Dog back = (Dog) a;          // downcast: explicit, checked at runtime
```

More in [07_polymorphism.md](./07_polymorphism.md).

## Multilevel inheritance

```java
class Animal { }
class Mammal extends Animal { }
class Dog extends Mammal { }       // Dog inherits from Mammal and Animal
```

Deep hierarchies (more than 2-3 levels) are a smell: behavior is spread across many files and changes ripple.

## When inheritance is appropriate

Use it only when **all** of these hold:

1. The relationship is genuinely **is-a**, now and in the future
2. The subclass can be used everywhere the parent is expected (**Liskov Substitution Principle**: [23-design-and-clean-code/00_solid-principles.md](../23-design-and-clean-code/00_solid-principles.md))
3. You want to reuse **and** specialize behavior, and the parent was **designed** for extension (documented hooks)
4. The hierarchy is shallow and stable

Otherwise prefer **composition** ([10](./10_composition-and-object-relationships.md)) or **interfaces** ([09](./09_interfaces.md)).

### Classic failures

```java
// Inheritance for code reuse only: a Stack is NOT an ArrayList
class Stack<E> extends ArrayList<E> {
    void push(E e) { add(e); }
    E pop() { return remove(size() - 1); }
}
// Callers can still call add(0, x), remove(0), clear(): the stack's invariant is broken
```

```java
// Square is-a Rectangle? Behaviorally no (Liskov violation)
class Rectangle { void setWidth(int w) {...} void setHeight(int h) {...} }
class Square extends Rectangle {
    @Override void setWidth(int w) { super.setWidth(w); super.setHeight(w); }   // surprises code that expects independent sides
}
```

### The fragile base class problem

Subclasses depend on the parent's **implementation**, not just its contract:

```java
class CountingList<E> extends ArrayList<E> {
    private int added = 0;
    @Override public boolean add(E e) { added++; return super.add(e); }
    @Override public boolean addAll(Collection<? extends E> c) {
        added += c.size();
        return super.addAll(c);          // ArrayList.addAll may call add() internally in some implementations: double counting
    }
}
```

A change inside the parent can silently break the child. Document self-use if you design for inheritance, or avoid inheriting from classes you do not control. The alternative, wrapping a list and **delegating** to it, is immune ([10](./10_composition-and-object-relationships.md)).

## Design for inheritance, or prohibit it

If you allow subclassing:
- Document which overridable methods are called by which (**self-use**)
- Keep constructors free of overridable calls ([05](./05_initialization-order.md))
- Expose clear extension points (template-method hooks: [08](./08_abstract-classes.md))

Otherwise: make the class `final`.

## Inheritance in the JDK

| Hierarchy | Example |
|-----------|---------|
| Exceptions | `Exception` → `IOException` → `FileNotFoundException` ([06-exceptions-and-debugging](../06-exceptions-and-debugging/00_exception-hierarchy-and-types.md)) |
| Collections | `AbstractList` → `ArrayList` (inheritance for implementation; interfaces for types) |
| GUI toolkits | `Component` → `Container` → `JPanel` |
| `Object` | The root of everything |

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Inheriting just to reuse code | Wrong is-a, broken invariants (`Stack extends ArrayList`) | Composition and delegation |
| Forgetting `@Override` | Typo creates a new method; polymorphism silently fails | Always annotate |
| Narrowing access when overriding | `attempting to assign weaker access privileges` | Same or wider |
| No matching `super(...)` call | `constructor ... cannot be applied` | Call the right parent constructor |
| Redeclaring a field with the same name | Two fields; confusing results | Use a different name or reuse the parent's |
| Assuming static methods are overridden | Parent version runs | Use instance methods |
| Calling overridable methods in constructors | Child fields still default | [05](./05_initialization-order.md) |
| Deep hierarchies | Hard to understand and change | Flatten with interfaces and composition |
| `protected` fields | Subclasses can break invariants | `private` + protected accessors |
| Overriding `equals` in subclasses carelessly | Symmetry violations | [14](./14_equals-and-hashcode.md) |
| Subclasses that disable inherited behavior (throw `UnsupportedOperationException`) | Violates substitutability | Rethink the hierarchy |

## Key takeaways

- `extends` models is-a; a subclass inherits accessible members, not constructors
- Override instance methods with `@Override`; keep the same signature, covariant return, access no narrower
- Fields and static methods are hidden, not overridden
- A subclass constructor starts with `super(...)`
- Prefer composition and interfaces; inherit only for true, stable, documented is-a relationships, or make the class `final`

**Next:** [Polymorphism](./07_polymorphism.md)
