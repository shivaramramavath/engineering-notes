# Polymorphism and Dynamic Method Dispatch in Java

> A comprehensive deep-dive into compile-time vs runtime polymorphism, HotSpot virtual method tables (`vtable`), inline caching, and modern Pattern Matching.

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate to Advanced |
| **Prerequisites** | [00_classes-and-objects.md](./00_classes-and-objects.md), [06_inheritance.md](./06_inheritance.md) |
| **Time to Read** | 25 minutes |
| **Status** | Active |
| **Java Version** | Java 8+ (Pattern Matching for instanceof in Java 16+) |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance & Security Notes](#8-performance--security-notes)
9. [Comparison: Compile-Time vs Runtime Polymorphism](#9-comparison-compile-time-vs-runtime-polymorphism)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

**Polymorphism** (from Greek *poly* "many" and *morph* "form") is the ability of an entity (such as a variable, method parameter, or collection) to take on different forms or behaviors at runtime depending on the actual concrete type of the object it holds.

In Java, polymorphism enables:
- **Extensibility:** New classes can be introduced without modifying existing client code.
- **Loose Coupling:** Clients program to abstract interfaces or superclasses rather than concrete implementations.
- **Dynamic Dispatch:** The JVM determines which method implementation to execute at runtime based on the actual object located on the heap.

---

## 2. Core Concepts

### 2.1 The Two Types of Polymorphism
1. **Compile-Time Polymorphism (Static Binding / Ad-hoc):**
   - Achieved through **Method Overloading**.
   - Resolved at compile time by `javac` based on the static types and count of arguments.
2. **Runtime Polymorphism (Dynamic Binding / Subtyping):**
   - Achieved through **Method Overriding** via inheritance and interface implementation.
   - Resolved at runtime by the JVM based on the concrete heap object type.

### 2.2 Upcasting and Downcasting
- **Upcasting:** Treating a specialized subclass instance as its generalized superclass type (`Animal a = new Dog()`). Always safe and implicit.
- **Downcasting:** Casting a generalized reference back to its specialized subtype (`Dog d = (Dog) a`). Requires an explicit cast and can throw `ClassCastException` if the underlying object is not an instance of the target type.

---

## 3. How It Works Internally

### 3.1 HotSpot Virtual Method Tables (`vtable` & `itable`)
When `invokevirtual` executes:
1. The JVM inspects the object's header to find its `Klass` metadata pointer in Metaspace.
2. Every `Klass` maintains a **`vtable`** (array of method pointers indexed by integer offsets):
   - Superclass methods occupy fixed lower indices.
   - Overriding subclass methods overwrite the method pointer at that exact same index.
3. The JVM indexes into the table in $O(1)$ time:
   $$\text{Method Address} = \text{vtable}[\text{method\_offset}]$$

```
HotSpot vtable Dispatch:
Animal ref = new Cat();
ref.makeSound(); // bytecode: invokevirtual #5 (makeSound offset: 2)

Cat Object on Heap
┌──────────────────────────┐
│ Mark Word                │
│ Klass Pointer ───────────┼──────► Cat InstanceKlass in Metaspace
└──────────────────────────┘        ┌───────────────────────────────┐
                                    │ vtable:                       │
                                    │ [0] Object::toString          │
                                    │ [1] Object::equals            │
                                    │ [2] Cat::makeSound  ──────────┼──► Execute Cat's Bytecode!
                                    └───────────────────────────────┘
```

For interface methods (`invokeinterface`), the JVM uses an **`itable`** (Interface Table), which searches through an array of interface offsets.

### 3.2 HotSpot C2 Inline Caching
To avoid table lookups on every call, the JIT compiler uses **Inline Caching**:
- **Monomorphic Call Site (1 receiver type observed):** The JIT devirtualizes the call into a direct jump or inlines the target bytecode directly with a speculative type check.
- **Bimorphic Call Site (2 receiver types observed):** The JIT compiles a fast conditional branch: `if (type == Cat) Cat::sound(); else Dog::sound();`.
- **Megamorphic Call Site (3+ receiver types observed):** The JIT falls back to standard `vtable`/`itable` lookup.

---

## 4. Syntax & Usage

### 4.1 Pattern Matching for `instanceof` (Java 16+ JEP 394)
Historically in Java, casting required verbose, error-prone boilerplate:
```java
// Pre-Java 16 Clunky Casting:
if (obj instanceof String) {
    String s = (String) obj;
    System.out.println(s.length());
}

// Modern Java 16+ Pattern Matching:
if (obj instanceof String s) {
    // Variable 's' is automatically cast and scoped strictly to this block!
    System.out.println(s.length());
}
```

---

## 5. Practical Examples

### Example 1: Classic Polymorphic Dispatch Pipeline
```java
interface NotificationChannel {
    void send(String message);
}

class EmailChannel implements NotificationChannel {
    @Override
    public void send(String message) {
        System.out.println("[EMAIL] Dispatching SMTP payload: " + message);
    }
}

class SmsChannel implements NotificationChannel {
    @Override
    public void send(String message) {
        System.out.println("[SMS] Sending SMS via Twilio: " + message);
    }
}

class SlackChannel implements NotificationChannel {
    @Override
    public void send(String message) {
        System.out.println("[SLACK] Posting webhook message: " + message);
    }
}

public class NotificationService {
    public static void broadcast(List<NotificationChannel> channels, String alert) {
        for (NotificationChannel channel : channels) {
            // Polymorphic dispatch: invokes correct implementation at runtime!
            channel.send(alert);
        }
    }

    public static void main(String[] args) {
        var channels = List.of(new EmailChannel(), new SmsChannel(), new SlackChannel());
        broadcast(channels, "Production database failover initiated!");
    }
}
```
**Output:**
```text
[EMAIL] Dispatching SMTP payload: Production database failover initiated!
[SMS] Sending SMS via Twilio: Production database failover initiated!
[SLACK] Posting webhook message: Production database failover initiated!
```

### Example 2: Real-World Safe Polymorphic Shape Renderer with Pattern Matching
```java
sealed interface Shape permits Circle, Rectangle {}

record Circle(double radius) implements Shape {}
record Rectangle(double width, double height) implements Shape {}

public class ShapeCalculator {
    public static double calculateArea(Shape shape) {
        // Java 21+ Pattern matching for switch expressions!
        return switch (shape) {
            case Circle c -> Math.PI * c.radius() * c.radius();
            case Rectangle r -> r.width() * r.height();
        };
    }

    public static void main(String[] args) {
        Shape c = new Circle(10.0);
        Shape r = new Rectangle(4.0, 5.0);

        System.out.printf("Circle area:    %.2f%n", calculateArea(c));
        System.out.printf("Rectangle area: %.2f%n", calculateArea(r));
    }
}
```
**Output:**
```text
Circle area:    314.16
Rectangle area: 20.00
```

### Example 3: Edge Case – Overloading Resolution vs Overriding Resolution
```java
class SuperPrinter {
    public void print(Object obj) {
        System.out.println("SuperPrinter: Object");
    }
}

class SubPrinter extends SuperPrinter {
    // OVERLOAD (Different parameter type!)
    public void print(String str) {
        System.out.println("SubPrinter: String");
    }
}

public class OverloadOverrideAmbiguity {
    public static void main(String[] args) {
        SuperPrinter printer = new SubPrinter();
        Object text = "Hello World";

        // Reference type is SuperPrinter; compiler resolves to print(Object)!
        printer.print(text); // Prints "SuperPrinter: Object"!
    }
}
```
**Output:**
```text
SuperPrinter: Object
```

### Example 4: Failure Case – Unsafe Downcasting (`ClassCastException`)
```java
public class ClassCastExceptionDemo {
    static class Animal {}
    static class Dog extends Animal {}
    static class Cat extends Animal {}

    public static void main(String[] args) {
        Animal a = new Cat();

        try {
            // DANGEROUS: Cat cannot be cast to Dog!
            Dog d = (Dog) a;
        } catch (ClassCastException e) {
            System.out.println("Caught: " + e.getMessage());
        }

        // SAFE: Pattern matching instanceof
        if (a instanceof Dog d) {
            System.out.println("Safely cast to Dog!");
        } else {
            System.out.println("Verified object is not a Dog safely.");
        }
    }
}
```
**Output:**
```text
Caught: class ClassCastExceptionDemo$Cat cannot be cast to class ClassCastExceptionDemo$Dog
Verified object is not a Dog safely.
```

### Example 5: Optimized / Idiomatic – Eliminating Conditional Type Checks with Polymorphism
```java
// ANTI-PATTERN: Procedural type inspection
class BrokenTaxCalculator {
    public double calculateTax(String country, double income) {
        if ("US".equals(country)) return income * 0.25;
        else if ("DE".equals(country)) return income * 0.42;
        else if ("SG".equals(country)) return income * 0.15;
        throw new IllegalArgumentException("Unknown country");
    }
}

// IDIOMATIC: Polymorphic Strategy Pattern
interface TaxStrategy {
    double calculate(double income);
}

class UsTaxStrategy implements TaxStrategy {
    public double calculate(double income) { return income * 0.25; }
}

class GermanTaxStrategy implements TaxStrategy {
    public double calculate(double income) { return income * 0.42; }
}

public class CleanTaxSystem {
    public static void main(String[] args) {
        TaxStrategy strategy = new GermanTaxStrategy();
        System.out.println("Tax on 100k: " + strategy.calculate(100_000));
    }
}
```
**Output:**
```text
Tax on 100k: 42000.0
```

---

## 6. Best Practices

1. **Program to Interfaces, Not Implementations:** Declare variable types, parameters, and return types as interfaces (`List<String> list = new ArrayList<>()`).
2. **Replace `if-else` Type Checks with Polymorphism:** If you see large `switch` or `if (type == ...)` chains, refactor into polymorphic class hierarchies or strategy patterns.
3. **Use Modern Pattern Matching for `instanceof`:** Eliminate manual casting using `if (obj instanceof TargetType t)`.
4. **Beware Megamorphic Call Sites in Hot Loops:** Passing 10+ different interface implementations through a single hot call site degrades JIT inline caching. Keep hot paths monomorphic where possible.
5. **Keep Inheritance Hierarchies Shallow:** Deep hierarchies (4+ levels) make virtual dispatch harder to reason about and increase memory indirection.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Is Wrong | Correct Idiom |
| :--- | :--- | :--- |
| Downcasting without `instanceof` | Throws `ClassCastException` if object type changes. | Use pattern matching: `if (obj instanceof Dog d)`. |
| Confusing overloading with overriding | Overloading is resolved at compile time based on reference types; overriding is resolved at runtime. | Annotate overrides with `@Override`. |
| Excessive type inspection (`instanceof` cascades) | Violates Open-Closed Principle (OCP); modifying code requires updating all cascades. | Delegate behavior polymorphically. |
| Casting to concrete classes | Couples client code to concrete implementations. | Cast to interface if cast is required. |
| Forgetting covariant return types | Unnecessarily casts returned objects when subclasses return more specific types. | Declare specific return types in overriding methods. |

---

## 8. Performance & Security Notes

### 8.1 The Cost of Megamorphic Calls
In micro-benchmarks, monomorphic virtual calls have virtually identical performance to static calls (nanosecond scale) due to C2 JIT inlining. However, if a call site becomes **megamorphic** (invoking methods on 3 or more distinct concrete classes at the exact same bytecode location), the JIT compiler cannot inline the target and must emit a full `itable` or `vtable` stub lookup on every single iteration.

---

## 9. Comparison: Compile-Time vs Runtime Polymorphism

| Dimension | Compile-Time (Overloading) | Runtime (Overriding) |
| :--- | :--- | :--- |
| **Mechanism** | Method Overloading | Method Overriding (`extends` / `implements`) |
| **Resolution Time** | Compile Time (`javac`) | Runtime (JVM `invokevirtual` / `invokeinterface`) |
| **Binding** | Static binding | Dynamic binding (`vtable` / `itable`) |
| **Signatures** | Different parameters, same name | Exact same parameters and name |
| **Performance** | Zero runtime lookup overhead | Small lookup overhead unless inlined by JIT |

---

## 10. Interview Questions

<details>
<summary><strong>1. How does dynamic method dispatch work internally in the JVM?</strong></summary>

When an instance method is called via `invokevirtual`, the JVM reads the object's `Klass` pointer in its header to locate its `vtable` in Metaspace. The `vtable` contains pointers to executable method bytecodes. Subclass overrides replace the pointer at the inherited method index. The JVM indexes into the `vtable` in $O(1)$ time to dispatch the call to the concrete method implementation.
</details>

<details>
<summary><strong>2. What is the difference between invokevirtual, invokeinterface, and invokestatic?</strong></summary>

- `invokevirtual`: Invokes a public or protected instance method using a class `vtable`.
- `invokeinterface`: Invokes a method declared on an interface using an `itable` search.
- `invokestatic`: Invokes a static method resolved directly at compile time without dynamic dispatch.
</details>

<details>
<summary><strong>3. What is an Inline Cache in HotSpot JIT compilation?</strong></summary>

An Inline Cache is an optimization where the JIT caches the target method address of the last observed receiver type at a virtual call site. If future calls come from the same type (monomorphic), the call bypasses the `vtable` lookup entirely and jumps directly to the compiled machine code.
</details>

<details>
<summary><strong>4. What is Pattern Matching for instanceof and in which Java release was it standardized?</strong></summary>

Standardized in **Java 16 (JEP 394)**, Pattern Matching for `instanceof` combines type testing and type casting into a single atomic statement: `if (obj instanceof String s)`. If `obj` is an instance of `String`, it is safely cast and assigned to variable `s`, which is scoped exclusively to the truth branch.
</details>

<details>
<summary><strong>5. Can you override a private or static method?</strong></summary>

No. Private methods are not inherited and cannot be seen by subclasses (`invokespecial`). Static methods belong to the class and are bound at compile time (`invokestatic`). Subclasses declaring identical signatures merely hide the superclass static method.
</details>

<details>
<summary><strong>6. What happens if a call site is megamorphic?</strong></summary>

A call site is megamorphic if it observes three or more different concrete receiver classes at runtime. The JIT compiler abandons direct inlining and bimorphic branches, falling back to a full `vtable` or `itable` stub lookup on every method invocation.
</details>

<details>
<summary><strong>7. Why should you program to an interface rather than a concrete class?</strong></summary>

Programming to an interface decouples client code from specific implementation classes. It allows swapping implementations (e.g. replacing `ArrayList` with `LinkedList` or a mock test implementation) without changing any client business logic.
</details>

<details>
<summary><strong>8. What is the difference between upcasting and downcasting?</strong></summary>

Upcasting converts a subclass reference to a superclass reference; it is always safe, implicit, and guaranteed by the type system. Downcasting converts a superclass reference back to a subclass reference; it requires an explicit cast and may throw `ClassCastException` if the runtime object is not of the specified type.
</details>

---

## 11. Practice Exercises

### Easy
1. **Polymorphic Speaker:** Implement an interface `Speaker` with `sayHello()` implemented by `EnglishSpeaker` and `SpanishSpeaker`.
2. **Safe Downcaster:** Write a method using modern pattern matching `instanceof` to extract and print integer lengths or string lengths.
3. **Vehicle Fleet:** Store different subclasses of `Vehicle` in a `List<Vehicle>` and call `drive()` polymorphically.

### Medium
4. **Payment Gateway Router:** Implement a polymorphic payment processing engine that dispatches to `PayPal`, `CreditCard`, or `Crypto` handlers.
5. **Switch Pattern Matcher:** Implement a calculation engine using Java 21+ pattern matching for switch over sealed shapes.
6. **Bimorphic vs Megamorphic Benchmark:** Write a test demonstrating the throughput difference between a monomorphic call loop and a megamorphic call loop.

### Hard
7. **Bytecode Instruction Comparison:** Inspect with `javap -c` the differences between `invokevirtual`, `invokeinterface`, and `invokestatic` across three identical call targets.
8. **Custom Dynamic Dispatch Table:** Implement your own software `vtable` in Java using arrays of `MethodHandle` instances.
9. **JIT Inlining Verification:** Run an application with `-XX:+PrintInlining` to observe how HotSpot devirtualizes monomorphic virtual calls.

<details>
<summary><strong>Exercise Solutions</strong></summary>

```java
// Solution to Exercise 1: Polymorphic Speaker
public interface Speaker {
    void sayHello();
}

public class EnglishSpeaker implements Speaker {
    public void sayHello() { System.out.println("Hello!"); }
}

public class SpanishSpeaker implements Speaker {
    public void sayHello() { System.out.println("¡Hola!"); }
}
```
</details>

---

## 12. Summary & Cheat Sheet

```
┌────────────────────────────────────────────────────────────────────────┐
│                        POLYMORPHISM CHEAT SHEET                        │
├────────────────────────────────────────────────────────────────────────┤
│ Compile-Time:       Method Overloading (Static binding, compile-time)  │
│ Runtime:            Method Overriding (Dynamic binding, vtable/itable) │
│ Bytecode:           invokevirtual (Classes), invokeinterface (Ifaces)   │
│ Dispatch Tables:    vtable contains method pointers indexed by offset  │
│ Inline Cache:       Monomorphic (1 type: inlined), Bimorphic (2 types),│
│                     Megamorphic (3+ types: table lookup)               │
│ Pattern Matching:   if (obj instanceof String s) (Java 16 JEP 394)     │
│ Core Rule:          Program to interfaces, not implementations         │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 13. Further Reading
- Java Language Specification (JLS) Section 8.4.8: Overriding and Dynamic Method Lookup
- [JEP 394: Pattern Matching for instanceof (Standard in Java 16)](https://openjdk.org/jeps/394)
- OpenJDK HotSpot Source: `vtableStubs.cpp` and Dynamic Virtual Dispatch Architecture
