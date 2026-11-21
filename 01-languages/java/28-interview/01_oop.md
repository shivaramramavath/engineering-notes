# Interview Mastery: Object-Oriented Programming (OOP) & Polymorphism

> Architectural interview guide for Object-Oriented Programming: dynamic dispatch mechanics, vtable resolution, sealed hierarchies, interface evolution, and composition over inheritance.

| Metadata | Value |
|---|---|
| **Level** | Intermediate to Advanced |
| **Prerequisites** | [00_core-java.md](00_core-java.md), [04-oop](../04-oop/README.md) |
| **Time to Read** | 35 minutes |
| **Status** | Complete |
| **Java Version** | Java 8 to Java 25 LTS |

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
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

Object-Oriented Programming (OOP) in Java is not merely a set of syntactic keywords; it is a discipline of **managing complexity, bounding dependencies, and isolating mutations**.

In high-stakes technical interviews, questions around OOP evaluate whether an engineer:
- Knows how the JVM resolves virtual methods at runtime via **vtables (virtual method tables)**.
- Can design extensible systems adhering to **Liskov Substitution (LSP)** and **Open-Closed (OCP)** principles.
- Understands why modern Java favors **Composition over Inheritance** and **Sealed Type Hierarchies**.
- Can resolve multiple inheritance conflicts introduced by interface default methods (the Diamond Problem).

---

## 2. Core Concepts

### 1. The 4 Pillars of OOP
1. **Encapsulation**: Bundling internal state with mutating methods and hiding private representation details from external interference.
2. **Abstraction**: Exposing clean, contract-driven interfaces while concealing implementation complexities.
3. **Inheritance**: Deriving specialized subclasses from general parent classes to share structure and behavior.
4. **Polymorphism**: The ability of an object reference to manifest multiple behaviors at runtime.
   - **Static Polymorphism (Compile-time)**: Method Overloading resolved by the compiler based on argument types.
   - **Dynamic Polymorphism (Runtime)**: Method Overriding resolved dynamically by the JVM based on the concrete heap object.

### 2. Modern Interfaces vs Abstract Classes
Since Java 8, 9, and 17, the boundaries between interfaces and abstract classes have shifted dramatically:
- **Interfaces**: Can contain `default` methods (behavioral traits), `static` helper methods, and `private` utility methods. They cannot hold instance state (fields are strictly `public static final`).
- **Abstract Classes**: Can hold mutable instance state, constructors, and package-private methods. A class can extend only one abstract class.
- **Sealed Interfaces / Classes (Java 17+)**: Restrict which subclasses may implement or extend them using the `permits` clause, creating closed algebraic data types (ADTs).

---

## 3. How It Works Internally

### Virtual Method Table (vtable) and Interface Table (itable)
In HotSpot JVM, dynamic dispatch is not implemented through slow textual method lookups:
```
Subclass Heap Object (0x7F1000):
+-------------------------------+
| Object Header: Mark Word      | (8 bytes)
| Object Header: Klass Word     | (4 bytes) -> Points to Klass metadata in Metaspace
| Instance Field: data          | (4 bytes)
+-------------------------------+

Klass Metadata in Metaspace:
+-------------------------------+
| vtable (Virtual Method Table) |
| [0] Object.toString()         | -> 0x55A0 (Parent implementation)
| [1] Object.hashCode()         | -> 0x55B0
| [2] Shape.draw()              | -> 0x7700 (Overridden Circle.draw() pointer)
| [3] Shape.calculateArea()     | -> 0x7740 (Overridden Circle.calculateArea())
+-------------------------------+
```
When `shape.draw()` is executed:
1. Bytecode `invokevirtual` reads the object's `Klass` pointer.
2. Jumps to fixed offset `vtable[2]`.
3. Calls the native method pointer at `0x7700` in $O(1)$ constant time.

### Inline Caching & JIT Devirtualization
- **Monomorphic Call Site**: If a call site always invokes the exact same concrete class, the C2 JIT compiler eliminates the vtable lookup entirely and inlines the method body directly!
- **Bimorphic Call Site**: Checks two target classes via conditional branch.
- **Megamorphic Call Site** ($\ge 3$ concrete classes): Degrades to full vtable lookup on every call.

---

## 4. Syntax & Usage

### Modern Sealed Hierarchies with Pattern Matching (Java 17–25)

```java
// Sealed interface declares exact permits list
public sealed interface PaymentMethod
    permits CreditCard, BankTransfer, CryptoPayment {}

public record CreditCard(String cardNumber, String cvv) implements PaymentMethod {}
public record BankTransfer(String iban, String swift) implements PaymentMethod {}
public record CryptoPayment(String walletAddress, String network) implements PaymentMethod {}

public class PaymentProcessor {
    // Exhaustive pattern matching switch (no default branch needed!)
    public static String process(PaymentMethod method) {
        return switch (method) {
            case CreditCard cc   -> "Processing Card: " + cc.cardNumber();
            case BankTransfer bt -> "Transferring to IBAN: " + bt.iban();
            case CryptoPayment cp -> "Broadcasting to: " + cp.network();
        };
    }
}
```

---

## 5. Practical Examples

### Example 1: Dynamic Dispatch vs Static Binding

```java
public class PolymorphismProof {
    static class Animal {
        static void identify() { System.out.print("ANIMAL_STATIC "); }
        void sound() { System.out.print("ANIMAL_SOUND "); }
    }

    static class Dog extends Animal {
        static void identify() { System.out.print("DOG_STATIC "); } // Hides Animal.identify
        @Override
        void sound() { System.out.print("BARK "); }                 // Overrides Animal.sound
    }

    public static void main(String[] args) {
        Animal a = new Dog();
        a.identify(); // Static: resolved at compile-time by reference type (Animal) -> "ANIMAL_STATIC "
        a.sound();    // Dynamic: resolved at runtime by heap instance (Dog) -> "BARK"
    }
}
```

### Example 2: Resolving the Interface Diamond Problem

```java
public class DiamondProblemResolution {
    interface Engine {
        default void start() {
            System.out.println("Engine: Standard ignition");
        }
    }

    interface ElectricSystem {
        default void start() {
            System.out.println("ElectricSystem: Power inverter on");
        }
    }

    // HybridCar implements both interfaces with colliding default start() methods
    static class HybridCar implements Engine, ElectricSystem {
        @Override
        public void start() {
            // Must explicitly disambiguate using InterfaceName.super.method()
            System.out.println("HybridCar: Coordinating start sequence...");
            ElectricSystem.super.start();
            Engine.super.start();
        }
    }

    public static void main(String[] args) {
        new HybridCar().start();
    }
}
```

### Example 3: Composition Over Inheritance (Delegation Pattern)

```java
import java.util.*;

// BAD: Extending HashSet breaks encapsulation (fragile base class)
// GOOD: Forwarding wrapper with composition
public class InstrumentedSet<E> implements Set<E> {
    private final Set<E> delegate;
    private int addCount = 0;

    public InstrumentedSet(Set<E> delegate) {
        this.delegate = Objects.requireNonNull(delegate);
    }

    @Override
    public boolean add(E e) {
        addCount++;
        return delegate.add(e);
    }

    @Override
    public boolean addAll(Collection<? extends E> c) {
        addCount += c.size();
        return delegate.addAll(c);
    }

    public int getAddCount() { return addCount; }

    // Forwarding methods
    @Override public int size() { return delegate.size(); }
    @Override public boolean isEmpty() { return delegate.isEmpty(); }
    @Override public boolean contains(Object o) { return delegate.contains(o); }
    @Override public Iterator<E> iterator() { return delegate.iterator(); }
    @Override public Object[] toArray() { return delegate.toArray(); }
    @Override public <T> T[] toArray(T[] a) { return delegate.toArray(a); }
    @Override public boolean remove(Object o) { return delegate.remove(o); }
    @Override public boolean containsAll(Collection<?> c) { return delegate.containsAll(c); }
    @Override public boolean retainAll(Collection<?> c) { return delegate.retainAll(c); }
    @Override public boolean removeAll(Collection<?> c) { return delegate.removeAll(c); }
    @Override public void clear() { delegate.clear(); }
}
```

### Example 4: Template Method Pattern with Abstract Classes

```java
public abstract class DataMiner {
    // Template method defines the invariant execution skeleton
    public final void mineData(String path) {
        openFile(path);
        extractData();
        parseData();
        closeFile();
    }

    protected abstract void openFile(String path);
    protected abstract void extractData();
    protected abstract void parseData();

    // Concrete hook method
    protected void closeFile() {
        System.out.println("Closing generic file handle.");
    }
}

class PdfDataMiner extends DataMiner {
    @Override protected void openFile(String path) { System.out.println("Opening PDF: " + path); }
    @Override protected void extractData() { System.out.println("Extracting PDF font streams."); }
    @Override protected void parseData() { System.out.println("Parsing PDF text tokens."); }
}
```

### Example 5: Constructor Escape (Leaking `this` Trap)

```java
public class ConstructorEscapeTrap {
    static class SuperClass {
        SuperClass() {
            // DANGEROUS: Invoking virtual method in constructor before SubClass fields are initialized!
            init();
        }
        void init() {
            System.out.println("SuperClass init");
        }
    }

    static class SubClass extends SuperClass {
        private final String message = "SUB_INITIALIZED";

        @Override
        void init() {
            // At this point, message has NOT been initialized yet!
            System.out.println("SubClass init: message=" + message);
        }
    }

    public static void main(String[] args) {
        // Output: "SubClass init: message=null"
        new SubClass();
    }
}
```

---

## 6. Best Practices

1. **Favor Composition Over Inheritance**: Inheritance tightly couples subclasses to superclass implementation details (the Fragile Base Class problem). Use composition and interface forwarding instead.
2. **Never Call Overridable Methods in Constructors**: At the time the superclass constructor executes, the subclass's instance fields have not been initialized yet. Calling virtual methods from constructors leaks `null` states.
3. **Use Sealed Interfaces for Finite Domain Taxonomies**: When an interface's implementers are completely known (e.g., AST Nodes, Domain Events), declare it `sealed`. This enables compiler-checked exhaustive pattern matching switch without `default` fallbacks.
4. **Follow Liskov Substitution Principle (LSP)**: Subclasses must fulfill the contracts of their base types without strengthening preconditions or weakening postconditions.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Fix |
|---|---|---|
| Calling overridable method inside constructor | Subclass fields are uninitialized (`null`/`0`) when method executes | Keep constructor methods `private` or `final`. |
| Extending concrete collection classes (`extends ArrayList`) | Subclass depends on internal method implementation details | Wrap the collection via composition (`delegate`). |
| Ambiguous default method collisions in interfaces | Compiler refuses to compile diamond inheritance | Override colliding method and use `Interface.super.method()`. |
| Marking abstract class methods `private` | Subclasses cannot see or override private methods | Use `protected abstract` or `public abstract`. |
| Using inheritance solely for code reuse | Violates the "is-a" relationship; breaks LSP | Extract reusable functionality into a helper component. |

---

## 8. Performance & Security Notes

- **Devirtualization**: When the HotSpot C2 compiler detects that an interface or abstract class has only one loaded implementation across the entire JVM (Monomorphic), it replaces the dynamic vtable lookup with a direct machine code jump or inlines the method body entirely.
- **Megamorphic Call Overhead**: If a loop invokes a virtual method across $\ge 3$ distinct subclasses, inline caching fails, forcing full vtable indirection on every call (~10-15 CPU cycles per call).
- **Security Implications of Non-Final Classes**: Malicious subclasses can override methods to bypass security checks or leak sensitive state. Mark classes `final` unless explicitly designed for extension.

---

## 9. Comparison / When to Use What

| Feature | Interface | Abstract Class | Sealed Class |
|---|---|---|---|
| **Multiple Inheritance** | Supported (implement multiple) | Prohibited (single inheritance) | Prohibited |
| **Instance State** | None (`public static final` only) | Supported (fields allowed) | Supported |
| **Constructors** | Prohibited | Supported | Supported |
| **Default Implementations**| `default` methods | Concrete methods | Concrete methods |
| **Hierarchy Control** | Open (any class can implement) | Open | Strictly restricted via `permits` |

---

## 10. Interview Questions

<details>
<summary>1. How does the JVM implement virtual method invocation under the hood?</summary>

The JVM uses the `invokevirtual` bytecode instruction. Each loaded class metadata structure in Metaspace contains a **Virtual Method Table (vtable)**, which is an array of direct pointers to method implementations. When a method is called on a reference, the JVM retrieves the object's `Klass` pointer, looks up the fixed slot index in the vtable, and executes the target pointer in $O(1)$ constant time.
</details>

<details>
<summary>2. What is the Fragile Base Class problem and how does composition solve it?</summary>

The Fragile Base Class problem occurs when changes to a superclass's internal implementation inadvertently break the behavior of subclasses that rely on those internal details (e.g. `HashSet.addAll()` internally calling `add()`). Composition solves this by treating the other class as a black-box dependency (`delegate`), invoking only its public contract without inheriting internal implementation couplings.
</details>

<details>
<summary>3. What happens when a class implements two interfaces that declare identical default methods?</summary>

A compile-time error occurs due to the **Diamond Problem**. Java refuses to guess which default method should take precedence. The implementing class **must explicitly override** the colliding method and specify which interface to call using:
`InterfaceA.super.collidingMethod();`
or supply a completely new implementation.
</details>

<details>
<summary>4. Explain Liskov Substitution Principle (LSP) with a classic counterexample.</summary>

LSP states that objects of a superclass should be replaceable with objects of a subclass without altering the correctness of the program.
**Classic Counterexample**: `Square extends Rectangle`. A `Rectangle` allows setting width and height independently (`setWidth(5); setHeight(4); area == 20`). A `Square` forces width and height to be equal, breaking client invariants when substituted in code expecting rectangular behavior.
</details>

<details>
<summary>5. Why did Java introduce Sealed Classes in Java 17?</summary>

Prior to Java 17, inheritance was binary: either fully open (anyone could extend) or fully closed (`final`). Sealed classes allow an API author to restrict inheritance to a known set of subclasses using `permits`. This models **Algebraic Data Types (Sum Types)** and enables the compiler to perform **exhaustive pattern matching switches** without needing a dummy `default` branch.
</details>

<details>
<summary>6. Can you override a `static` method in Java? Why or why not?</summary>

No. Static methods belong to the class, not instance objects. When a subclass declares a static method with the same signature, it **hides** the superclass method; it does not override it. Dynamic method dispatch applies exclusively to instance methods.
</details>

<details>
<summary>7. What is the difference between Open-Closed Principle and Dependency Inversion Principle?</summary>

- **Open-Closed Principle (OCP)**: Software entities should be open for extension (via new implementations) but closed for modification (existing code remains untouched).
- **Dependency Inversion Principle (DIP)**: High-level modules should not depend on low-level modules; both should depend on abstractions (interfaces). Abstractions should not depend on details; details should depend on abstractions.
</details>

<details>
<summary>8. Why should you never invoke an overridable method inside a class constructor?</summary>

Because in Java's initialization sequence, the superclass constructor executes **before** the subclass's instance variables and field initializers. If the superclass constructor calls an overridable method that is overridden by the subclass, that method executes against an object whose subclass fields are still `null` or uninitialized, leading to subtle bugs and `NullPointerExceptions`.
</details>

---

## 11. Practice Exercises

### Exercise 1: Design a Rule-Engine Using Polymorphism
Eliminate this `if-else` chain using polymorphism and Strategy pattern:
```java
if (type.equals("STANDARD")) calculateStandard();
else if (type.equals("PREMIUM")) calculatePremium();
else if (type.equals("VIP")) calculateVip();
```

<details>
<summary>Solution</summary>

```java
public sealed interface DiscountStrategy permits StandardDiscount, PremiumDiscount, VipDiscount {
    double applyDiscount(double amount);
}

public final class StandardDiscount implements DiscountStrategy {
    @Override public double applyDiscount(double amount) { return amount * 0.95; }
}

public final class PremiumDiscount implements DiscountStrategy {
    @Override public double applyDiscount(double amount) { return amount * 0.85; }
}

public final class VipDiscount implements DiscountStrategy {
    @Override public double applyDiscount(double amount) { return amount * 0.70; }
}
```
</details>

### Exercise 2: Diagnose the Leaking `this` Bug
What does this program print, and why?
```java
class Super {
    Super() { print(); }
    void print() { System.out.println("Super"); }
}
class Sub extends Super {
    int x = 42;
    void print() { System.out.println("Sub x=" + x); }
}
public class Main {
    public static void main(String[] args) {
        new Sub();
    }
}
```

<details>
<summary>Solution</summary>

**Output**: `Sub x=0`

**Explanation**:
1. `new Sub()` calls `Super()` constructor first.
2. `Super()` calls `print()`. Because the actual heap object is an instance of `Sub`, virtual method dispatch invokes `Sub.print()`.
3. At this moment, `Sub`'s field `x = 42` has **not been executed yet**! Primitive `int` fields default to `0`.
4. Therefore, `Sub.print()` executes while `x` is still `0`, printing `Sub x=0`.
</details>

### Exercise 3: Refactor Inheritance to Delegation
Convert an inheritance-based `Stack extends Vector` design to a composition-based `Stack` wrapping a `Deque`.

<details>
<summary>Solution</summary>

```java
import java.util.ArrayDeque;
import java.util.Deque;
import java.util.NoSuchElementException;

public final class SafeStack<T> {
    private final Deque<T> storage = new ArrayDeque<>();

    public void push(T item) {
        storage.push(item);
    }

    public T pop() {
        if (storage.isEmpty()) throw new NoSuchElementException("Stack underflow");
        return storage.pop();
    }

    public T peek() {
        if (storage.isEmpty()) throw new NoSuchElementException("Stack empty");
        return storage.peek();
    }

    public int size() {
        return storage.size();
    }

    public boolean isEmpty() {
        return storage.isEmpty();
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Dynamic Dispatch**: Implemented via vtable arrays in Metaspace; C2 JIT devirtualizes monomorphic call sites.
- **Sealed Interfaces**: Restrict subtyping via `permits`; guarantees compiler-checked exhaustive pattern matching.
- **Diamond Problem**: Colliding interface default methods must be overridden and disambiguated via `Interface.super.method()`.
- **Constructor Invariant**: Never call virtual methods inside constructors; subclass fields are uninitialized.
- **Golden Rule**: Always favor composition and delegation over class inheritance.

---

## 13. Further Reading

- [Joshua Bloch: Effective Java (3rd Edition), Items 15-25: Classes and Interfaces](https://www.oreilly.com/library/view/effective-java-3rd/9780134686097/)
- [JEP 409: Sealed Classes (Java 17 LTS)](https://openjdk.org/jeps/409)
- [Baeldung: Guide to Java Virtual Method Tables](https://www.baeldung.com/java-virtual-method-table)
- [Robert C. Martin: Clean Architecture: A Craftsman's Guide to Software Structure and Design](https://www.oreilly.com/library/view/clean-architecture-a/9780134494272/)
