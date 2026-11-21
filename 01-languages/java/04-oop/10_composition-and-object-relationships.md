# Composition and Object Relationships in Java

> An authoritative guide to object relationships (Association, Aggregation, Composition, Dependency), the Decorator pattern, and the foundational principle "Favor composition over inheritance".

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [00_classes-and-objects.md](./00_classes-and-objects.md), [06_inheritance.md](./06_inheritance.md) |
| **Time to Read** | 25 minutes |
| **Status** | Active |
| **Java Version** | Java 8+ (Java 25 LTS verified) |

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
9. [Comparison: Object Relationship Taxonomy](#9-comparison-object-relationship-taxonomy)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

In object-oriented software engineering, systems are rarely composed of isolated classes. Classes collaborate by forming structural relationships to exchange messages, manage lifecycles, and model domain structures.

The most famous design guideline in object-oriented architecture is:
> *"Favor object composition over class inheritance."* — Gang of Four (*Design Patterns*, 1994) / Joshua Bloch (*Effective Java*, Item 18)

While inheritance couples classes tightly across compile-time boundaries, **composition** achieves flexible reuse by embedding references to other objects, enabling dynamic behavior swapping, black-box encapsulation, and complete immunity to the Fragile Base Class problem.

---

## 2. Core Concepts

### 2.1 The UML Relationship Hierarchy
Object relationships range from temporary transient interactions to permanent lifecycle coupling:

```
┌────────────────────────────────────────────────────────┐
│ Dependency (Weakest)    "uses-a"   Transient parameter │
└───────────┬────────────────────────────────────────────┘
            │
┌───────────▼────────────────────────────────────────────┐
│ Association             "knows-a"  Field reference     │
└───────────┬────────────────────────────────────────────┘
            │
┌───────────▼────────────────────────────────────────────┐
│ Aggregation             "has-a"    Independent life    │
└───────────┬────────────────────────────────────────────┘
            │
┌───────────▼────────────────────────────────────────────┐
│ Composition (Strongest) "owns-a"   Dependent life      │
└────────────────────────────────────────────────────────┘
```

1. **Dependency (`uses-a`):** Class A uses Class B temporarily (e.g. as a method argument or local variable). No persistent reference is stored.
2. **Association (`knows-a`):** Class A holds a reference to Class B as an instance field. Both objects can exist independently.
3. **Aggregation (`has-a`, Weak Ownership):** A whole-part relationship where parts can exist independently of the whole. If the container is destroyed, the child objects survive (e.g. `Department` and `Professor`).
4. **Composition (`owns-a`, Strong Ownership):** A strict whole-part relationship where the child object's lifecycle is bound to the container. If the container is destroyed, the child objects are destroyed (e.g. `House` and `Room`).

---

## 3. How It Works Internally

### 3.1 Memory Layout: Inheritance vs Composition
- **Inheritance (`Car extends Engine`):** Engine fields are flattened directly into the single contiguous `Car` object on the heap. Tight physical coupling; cannot swap the engine at runtime.
- **Composition (`Car` holds reference to `Engine`):** `Car` holds a 4-byte compressed reference pointing to a distinct `Engine` object on the heap. Loosely coupled; the engine reference can be reassigned or mocked dynamically.

```
Memory Layout of Composition:
┌──────────────────────────┐
│        Car Object        │
├──────────────────────────┤
│ Mark Word (8B)           │
│ Klass Pointer (4B)       │
│ String vin (4B)          │
│ Engine engineRef (4B) ───┼──────► ┌──────────────────────────┐
└──────────────────────────┘        │      Engine Object       │
                                    ├──────────────────────────┤
                                    │ Mark Word (8B)           │
                                    │ Klass Pointer (4B)       │
                                    │ int horsepower (4B)      │
                                    └──────────────────────────┘
```

---

## 4. Syntax & Usage

### 4.1 Forwarding Class Pattern (Decorator)
```java
public class ForwardingSet<E> implements Set<E> {
    private final Set<E> s;
    public ForwardingSet(Set<E> s) { this.s = s; }

    public void clear() { s.clear(); }
    public boolean contains(Object o) { return s.contains(o); }
    public boolean isEmpty() { return s.isEmpty(); }
    public int size() { return s.size(); }
    public boolean add(E e) { return s.add(e); }
    public boolean addAll(Collection<? extends E> c) { return s.addAll(c); }
    // remaining Set methods forwarded...
}
```

---

## 5. Practical Examples

### Example 1: The Decorator Pattern (Safe Instrumented Set)
```java
import java.util.Collection;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

public class InstrumentedSet<E> extends ForwardingSet<E> {
    private int addCount = 0;

    public InstrumentedSet(Set<E> s) {
        super(s);
    }

    @Override
    public boolean add(E e) {
        addCount++;
        return super.add(e);
    }

    @Override
    public boolean addAll(Collection<? extends E> c) {
        addCount += c.size();
        return super.addAll(c);
    }

    public int getAddCount() { return addCount; }

    public static void main(String[] args) {
        InstrumentedSet<String> set = new InstrumentedSet<>(new HashSet<>());
        set.addAll(List.of("Snap", "Crackle", "Pop"));

        // Composition guarantees accurate count (3), unlike broken inheritance!
        System.out.println("Items added: " + set.getAddCount());
    }
}
```
**Output:**
```text
Items added: 3
```

### Example 2: Real-World Dependency Injection via Composition
```java
interface PaymentGateway {
    void process(double amount);
}

class StripeGateway implements PaymentGateway {
    public void process(double amount) {
        System.out.printf("Stripe processed $%.2f%n", amount);
    }
}

class PayPalGateway implements PaymentGateway {
    public void process(double amount) {
        System.out.printf("PayPal processed $%.2f%n", amount);
    }
}

public class OrderCheckoutService {
    // Composed collaborator injected via constructor
    private final PaymentGateway paymentGateway;

    public OrderCheckoutService(PaymentGateway paymentGateway) {
        this.paymentGateway = paymentGateway;
    }

    public void completeOrder(double total) {
        System.out.println("Finalizing order...");
        paymentGateway.process(total);
    }

    public static void main(String[] args) {
        OrderCheckoutService checkout = new OrderCheckoutService(new StripeGateway());
        checkout.completeOrder(129.50);
    }
}
```
**Output:**
```text
Finalizing order...
Stripe processed $129.50
```

### Example 3: Edge Case – Aggregation vs Composition Lifecycle
```java
import java.util.List;
import java.util.ArrayList;

class Department {
    private final String name;
    // Aggregation: Professors exist independently of Department
    private final List<Professor> faculty = new ArrayList<>();

    public Department(String name) { this.name = name; }
    public void addProfessor(Professor p) { faculty.add(p); }
}

class Professor {
    private final String name;
    public Professor(String name) { this.name = name; }
}

class Building {
    // Composition: Rooms are created and destroyed WITH the Building
    private final List<Room> rooms = new ArrayList<>();

    public Building() {
        rooms.add(new Room("101")); // Lifecycle owned exclusively by Building
    }

    private static class Room {
        private final String roomNumber;
        Room(String roomNumber) { this.roomNumber = roomNumber; }
    }
}
```

### Example 4: Failure Case – Inheritance Misuse (The Java `Properties` Flaw)
```java
import java.util.Properties;

public class PropertiesFlawDemo {
    public static void main(String[] args) {
        // Historical Java design mistake: Properties extends Hashtable<Object, Object>
        // instead of composing it!
        Properties props = new Properties();
        props.setProperty("timeout", "5000");

        // Inherited put() bypasses String-only contract of Properties!
        props.put("port", 8080); // Corrupts properties file serialization!

        try {
            // Throws ClassCastException when reading via getProperty()!
            String port = props.getProperty("port");
        } catch (ClassCastException e) {
            System.out.println("Caught expected ClassCastException from broken inheritance!");
        }
    }
}
```
**Output:**
```text
Caught expected ClassCastException from broken inheritance!
```

### Example 5: Optimized / Idiomatic – Dynamic Strategy Swapping
```java
interface CompressionStrategy {
    byte[] compress(byte[] data);
}

class GzipStrategy implements CompressionStrategy {
    public byte[] compress(byte[] data) {
        System.out.println("Compressed with GZIP");
        return data;
    }
}

class SnappyStrategy implements CompressionStrategy {
    public byte[] compress(byte[] data) {
        System.out.println("Compressed with Snappy (High Speed)");
        return data;
    }
}

public class ArchiveManager {
    private CompressionStrategy strategy;

    public ArchiveManager(CompressionStrategy strategy) {
        this.strategy = strategy;
    }

    public void setStrategy(CompressionStrategy strategy) {
        this.strategy = strategy; // Swappable at runtime!
    }

    public void save(byte[] data) {
        strategy.compress(data);
    }

    public static void main(String[] args) {
        ArchiveManager mgr = new ArchiveManager(new GzipStrategy());
        mgr.save(new byte[100]);
        mgr.setStrategy(new SnappyStrategy());
        mgr.save(new byte[100]);
    }
}
```
**Output:**
```text
Compressed with GZIP
Compressed with Snappy (High Speed)
```

---

## 6. Best Practices

1. **Ask "Is-a" vs "Has-a":** Only use inheritance if class B is genuinely an instance of class A. If B merely *uses* or *has* A, use composition.
2. **Use Dependency Injection:** Pass collaborators via constructor arguments rather than instantiating them internally (`new Service()`).
3. **Hide Composed Collaborators:** Do not provide public getters for internal composed components; expose only domain operations.
4. **Use Forwarding Classes for Decorators:** Implement interface contracts and forward calls to the composed delegate.
5. **Beware Identity Confusion (Self-Problem):** In the Decorator pattern, wrapped objects do not know about their wrapper; passing `this` to external listeners passes the inner object, not the decorator.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Is Wrong | Correct Idiom |
| :--- | :--- | :--- |
| Inheritance for code reuse alone | Exposes all superclass methods, violating encapsulation (like `Properties` extending `Hashtable`). | Use composition and forward only needed methods. |
| Hardcoding collaborators with `new` | Prevents mocking during unit testing and locks behavior at compile time. | Inject collaborators via constructor. |
| Excessive delegation boilerplate | Writing 30 forwarding methods manually can be tedious. | Use IDE generation or Lombok/Lombok-like delegation. |
| Circular composition references | Object A holds B and B holds A; complicates GC graph and equals/hashCode recursions. | Break cycles using interfaces or event buses. |
| Ignoring aggregation null safety | Aggregated optional collaborators can be null; calling methods triggers NPE. | Use `Optional` or Null Object pattern. |

---

## 8. Performance & Security Notes

### 8.1 Indirection Overhead vs JIT Inlining
Composition introduces an extra pointer indirection (heap reference). However, HotSpot C2 JIT performs **escape analysis** and **scalar replacement**. If the composed object does not escape the calling method, the JVM unwraps its fields directly into CPU registers, eliminating pointer overhead entirely!

---

## 9. Comparison: Object Relationship Taxonomy

| Relationship | Type | Lifecycle Coupling | Multiplicity | Java Implementation |
| :--- | :--- | :--- | :--- | :--- |
| **Dependency** | `uses-a` | None (Method scope) | $1 \to 1$ | Method argument or local variable |
| **Association** | `knows-a` | Independent | $1 \to 1$ or $1 \to N$ | Field reference |
| **Aggregation** | `has-a` | Independent | $1 \to N$ | Collection of external objects |
| **Composition** | `owns-a` | Bound (Parent owns life) | $1 \to N$ | Instantiated and managed internally |
| **Inheritance** | `is-a` | Same object | $1 \to 1$ | `class Sub extends Super` |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why is composition preferred over inheritance?</strong></summary>

Composition is preferred because:
1. It preserves encapsulation (black-box reuse).
2. It is immune to the Fragile Base Class problem where superclass implementation changes break subclasses.
3. Behavior can be swapped dynamically at runtime by reassigning the composed reference.
4. It avoids polluting the subclass's public API with unwanted superclass methods.
</details>

<details>
<summary><strong>2. What is the difference between Aggregation and Composition?</strong></summary>

Both represent whole-part relationships, but differ in lifecycle ownership:
- In **Aggregation**, the parts can exist independently of the whole. For example, a `Department` has `Professor`s; deleting the department does not destroy the professors.
- In **Composition**, the parts cannot exist without the whole. For example, a `House` owns `Room`s; destroying the house destroys the rooms.
</details>

<details>
<summary><strong>3. What historical mistakes in the Java standard library violate this principle?</strong></summary>

- `java.util.Properties extends Hashtable<Object, Object>`: Allows non-String keys/values to be inserted, violating the String-only contract.
- `java.util.Stack extends Vector`: Exposes arbitrary index insertions (`add(index, elem)`), violating LIFO stack semantics.
</details>

<details>
<summary><strong>4. What is the Decorator (Wrapper) Pattern?</strong></summary>

The Decorator pattern uses composition to attach additional responsibilities or behaviors to an object dynamically. The decorator implements the same interface as the wrapped object and forwards calls to it while adding pre- or post-processing logic (e.g. `BufferedReader` wrapping `FileReader`).
</details>

<details>
<summary><strong>5. What is the "Self-Problem" in the Decorator pattern?</strong></summary>

In the Decorator pattern, the wrapped object has no reference to the wrapping decorator. If the wrapped object invokes a method on `this`, the call bypasses any overrides provided by the decorator.
</details>

<details>
<summary><strong>6. How does Dependency Injection relate to composition?</strong></summary>

Dependency Injection (DI) is a structural pattern that implements composition by supplying external collaborators (dependencies) to an object through its constructor or setter methods rather than having the object construct them directly via `new`.
</details>

<details>
<summary><strong>7. When is inheritance genuinely appropriate?</strong></summary>

Inheritance is appropriate only when:
1. A genuine `is-a` relationship exists (every instance of the subclass is truly an instance of the superclass).
2. The subclass will never violate any invariants of the superclass.
3. The superclass was explicitly designed and documented for extension.
</details>

<details>
<summary><strong>8. Does composition consume more memory than inheritance?</strong></summary>

Technically yes, because each composed object has its own 12-to-16-byte object header and reference pointer. However, modern JVM JIT compilers eliminate this overhead in local hot paths using escape analysis and scalar replacement.
</details>

---

## 11. Practice Exercises

### Easy
1. **Computer System:** Model a `Computer` class composed of `Processor`, `Memory`, and `HardDrive`.
2. **Car and Engine:** Implement `Car` and `Engine` with start/stop operations delegated to the engine.
3. **Dependency Example:** Write a `Printer` class that accepts a `Document` as a method parameter (dependency).

### Medium
4. **Custom Synchronized Decorator:** Create a thread-safe `SynchronizedListDecorator<T>` that wraps any `List<T>` and synchronizes all method calls.
5. **Dynamic Logger Strategy:** Implement a `Logger` service that dynamically switches between `ConsoleOutput` and `FileOutput` using composition.
6. **Refactor Stack:** Refactor `java.util.Stack` into a clean `ArrayStack` using composition over `ArrayList`.

### Hard
7. **Multi-Layered Decorator Pipeline:** Build a streaming pipeline where an `InputStream` is wrapped with `GzipInputStream`, `BufferedInputStream`, and `CipherInputStream`.
8. **JMH Benchmark of Composition vs Inheritance:** Measure microsecond execution differences between deep virtual dispatch vs forwarding delegation.
9. **Zero-Allocation Forwarding Proxy:** Write a bytecode generator using ByteBuddy that generates forwarding wrapper classes at runtime.

<details>
<summary><strong>Exercise Solutions</strong></summary>

```java
// Solution to Exercise 2: Car and Engine
public class Engine {
    public void start() { System.out.println("Engine started."); }
    public void stop() { System.out.println("Engine stopped."); }
}

public class Car {
    private final Engine engine;

    public Car(Engine engine) {
        this.engine = engine;
    }

    public void drive() {
        engine.start();
        System.out.println("Car is driving.");
    }

    public void park() {
        System.out.println("Car is parked.");
        engine.stop();
    }
}
```
</details>

---

## 12. Summary & Cheat Sheet

```
┌────────────────────────────────────────────────────────────────────────┐
│                      COMPOSITION CHEAT SHEET                           │
├────────────────────────────────────────────────────────────────────────┤
│ Dependency:        "uses-a" (Method parameter, temporary scope)        │
│ Association:       "knows-a" (Instance field, independent life)        │
│ Aggregation:       "has-a" (Container with independent child objects)  │
│ Composition:       "owns-a" (Container owns child lifecycle)          │
│ Golden Rule:       Favor composition over inheritance (Item 18)        │
│ Decorator Pattern: Implements interface + forwards calls to delegate  │
│ Flexibility:       Swappable dynamically at runtime via setters/DI     │
│ Safety:            Immune to Fragile Base Class problems               │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 13. Further Reading
- *Effective Java (3rd Edition)*, Joshua Bloch, Item 18: "Favor composition over inheritance".
- *Design Patterns: Elements of Reusable Object-Oriented Software*, Erich Gamma et al.
- UML Distilled (3rd Edition), Martin Fowler.
