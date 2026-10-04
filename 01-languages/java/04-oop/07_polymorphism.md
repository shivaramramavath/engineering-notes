# Polymorphism

**Polymorphism** ("many forms") means code written against a general type works with many specific types, and each one behaves in its own way. In Java the central mechanism is **dynamic dispatch**: which overridden method runs is decided at **runtime** by the object's actual class.

```java
abstract class Shape { abstract double area(); }
class Circle extends Shape {
    final double r; Circle(double r) { this.r = r; }
    double area() { return Math.PI * r * r; }
}
class Rect extends Shape {
    final double w, h; Rect(double w, double h) { this.w = w; this.h = h; }
    double area() { return w * h; }
}

List<Shape> shapes = List.of(new Circle(1), new Rect(2, 3));
double total = 0;
for (Shape s : shapes) total += s.area();      // each call runs the right version
```

The loop does not know or care which shapes exist. Adding `Triangle` later requires **no change** to it.

## Two kinds of polymorphism

| | Compile-time (static) | Runtime (dynamic) |
|---|----------------------|-------------------|
| Mechanism | **Overloading** | **Overriding** |
| Decided by | Static (declared) types of the arguments | The actual object type |
| When | Compile time | Runtime |
| Where | [02-methods/02_overloading-and-varargs.md](../02-methods/02_overloading-and-varargs.md) | This file |

## Reference type vs object type

```java
Animal a = new Dog("Rex");
//  ^^^^^^        ^^^^^^^
//  reference     actual object
//  type          type
```

| Question | Answered by | When |
|----------|-------------|------|
| Which **members can I call**? | The **reference** type (`Animal`) | Compile time |
| Which **implementation runs** for an overridden method? | The **object** type (`Dog`) | Runtime |

```java
Animal a = new Dog("Rex");
a.sound();      // Dog's version: "Woof" (runtime type)
a.fetch();      // ERROR: Animal has no fetch()  (compile-time type)
((Dog) a).fetch();   // OK after a cast
```

## Upcasting and downcasting

```java
Dog dog = new Dog("Rex");
Animal animal = dog;           // UPCAST: implicit, always safe
Dog again = (Dog) animal;      // DOWNCAST: explicit, checked at runtime

Animal cat = new Cat("Tom");
Dog wrong = (Dog) cat;         // compiles; ClassCastException at runtime
```

Check first:

```java
if (animal instanceof Dog) {
    Dog d = (Dog) animal;
    d.fetch();
}

if (animal instanceof Dog d) {      // pattern matching (Java 16+): test + cast + variable
    d.fetch();
}
```

Frequent downcasting is a design smell: it usually means the supertype lacks a method it should have. Prefer adding the method (polymorphism) over `instanceof` chains ([12-modern-java/04_pattern-matching.md](../12-modern-java/04_pattern-matching.md) shows the legitimate modern uses with sealed types).

## How dynamic dispatch works

```
Animal a = new Dog();
a.sound();
         │
         ▼   1. follow the reference to the object
         ▼   2. find its real class (Dog)
         ▼   3. look up sound() in Dog's method table (virtual method table), falling back to parent classes
         ▼   4. invoke that implementation
```

The JIT compiler often **inlines** these calls when it observes only one or two receiver types, so polymorphism is cheap in practice ([15-jvm-internals/06_jit-compiler.md](../15-jvm-internals/06_jit-compiler.md)).

## What is NOT polymorphic

| Member | Resolved by | Result |
|--------|-------------|--------|
| **Instance methods** (non-private, non-final) | Runtime type | Polymorphic |
| **Fields** | Declared (compile-time) type | Not polymorphic |
| **Static methods** | Declared type | Hidden, not overridden |
| **Private methods** | Declaring class | Not visible to subclasses, no dispatch |
| **Overloaded** methods | Declared types of arguments | Compile-time choice |
| **Constructors** | n/a | Not inherited |

```java
class P { String name = "P"; static String s() { return "P.s"; } String m() { return "P.m"; } }
class C extends P { String name = "C"; static String s() { return "C.s"; } String m() { return "C.m"; } }

P x = new C();
x.name;     // "P"     field: declared type
x.s();      // "P.s"   static: declared type
x.m();      // "C.m"   instance method: runtime type
```

## Polymorphism through interfaces

The most flexible form: depend on **what an object can do**, not what class it is.

```java
interface PaymentMethod { void pay(long cents); }
class CreditCard implements PaymentMethod { public void pay(long c) { /* ... */ } }
class PayPal     implements PaymentMethod { public void pay(long c) { /* ... */ } }

void checkout(PaymentMethod method, long total) {
    method.pay(total);              // works for any implementation, including future ones
}
```

Unrelated classes can be treated uniformly. See [09_interfaces.md](./09_interfaces.md).

## Replacing conditionals with polymorphism

```java
// Before: adding a type means editing this method (and every similar one)
double area(Shape s) {
    if (s instanceof Circle c) return Math.PI * c.r * c.r;
    else if (s instanceof Rect r) return r.w * r.h;
    else throw new IllegalArgumentException();
}

// After: each type owns its behavior
double total = shapes.stream().mapToDouble(Shape::area).sum();
```

This is the **Open/Closed Principle**: open for extension (new subclasses), closed for modification ([23-design-and-clean-code/00_solid-principles.md](../23-design-and-clean-code/00_solid-principles.md)). Patterns that rely on it: Strategy, Template Method, Command, State ([24-design-patterns](../24-design-patterns/README.md)).

## Polymorphic collections, arrays and parameters

```java
List<Animal> zoo = new ArrayList<>();
zoo.add(new Dog("Rex"));
zoo.add(new Cat("Tom"));
zoo.forEach(a -> System.out.println(a.sound()));

Animal[] arr = { new Dog("Rex"), new Cat("Tom") };
void feed(Animal a) { a.eat(); }                // accepts any Animal
```

`List<Dog>` is **not** a `List<Animal>` (generics are invariant); see [07-generics/02_wildcards-and-pecs.md](../07-generics/02_wildcards-and-pecs.md). Arrays are covariant (`Animal[] a = new Dog[1]`), which can fail at runtime with `ArrayStoreException`.

## Covariant return types

An override may return a more specific type:

```java
class Animal { Animal reproduce() { return new Animal(); } }
class Dog extends Animal {
    @Override Dog reproduce() { return new Dog(); }
}
Dog puppy = new Dog().reproduce();              // no cast needed
```

## `super` and the whole chain

```java
class A { String who() { return "A"; } }
class B extends A { String who() { return "B>" + super.who(); } }
class C extends B { String who() { return "C>" + super.who(); } }
new C().who();      // "C>B>A"
```

## Predict the output

```java
class A { void show() { System.out.println("A"); } void call() { show(); } }
class B extends A { void show() { System.out.println("B"); } }

A a = new B();
a.show();    // ?
a.call();    // ?
```

Both print `B`. Even though `call()` is declared in `A`, the call `show()` inside it is dispatched on the actual object (`B`).

## Benefits and costs

| Benefits | Costs |
|----------|-------|
| Extend behavior without touching existing code | Harder to trace which code runs |
| Eliminates long `if`/`switch` ladders | Requires a well-designed supertype |
| Enables testing with substitutes | Misused inheritance hierarchies |
| Foundation of frameworks, plugins, DI | Slight runtime overhead (usually optimized away) |

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Expecting fields to be polymorphic | Parent's value appears | Access through methods |
| Expecting static or private methods to be overridden | Parent version runs | Use instance, non-private methods |
| Calling a subclass-only method on a parent reference | Compile error | Add it to the parent/interface, or cast safely |
| Unchecked downcasts | `ClassCastException` | `instanceof` first, or redesign |
| Long `instanceof` chains | Rigid, error-prone code | Move behavior into the types |
| Overloading when overriding was intended (wrong parameter type) | Wrong method runs | `@Override` |
| `List<Dog>` passed where `List<Animal>` is required | Compile error | Wildcards (`List<? extends Animal>`) |
| Subclass changes the meaning of a method | Violates substitutability | Respect the parent's contract |

## Key takeaways

- The **reference type** decides what you may call; the **object type** decides which overridden method runs
- Only instance methods dispatch dynamically; fields, static, private methods and overload choice do not
- Upcasting is implicit; downcasting needs a check (`instanceof`) and often signals a design problem
- Program to interfaces or abstract types; replace type checks with polymorphic methods

**Next:** [Abstract Classes](./08_abstract-classes.md)
