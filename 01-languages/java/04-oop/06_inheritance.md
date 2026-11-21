# Inheritance in Java: Extends, Overriding, and Hierarchy Architecture

> An authoritative guide to class inheritance in Java, method overriding (`@Override`), covariant return types, the Liskov Substitution Principle, and the Fragile Base Class Problem.

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [00_classes-and-objects.md](./00_classes-and-objects.md), [01_constructors.md](./01_constructors.md), [04_encapsulation-and-access-modifiers.md](./04_encapsulation-and-access-modifiers.md) |
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
9. [Comparison: Inheritance vs Composition](#9-comparison-inheritance-vs-composition)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

**Inheritance** is an object-oriented mechanism that allows a new class (**subclass** or derived class) to inherit state (fields) and behavior (methods) from an existing class (**superclass** or base class) using the `extends` keyword.

Key motivations:
- **Code Reuse:** Shared logic lives in a single common superclass rather than being duplicated across subclasses.
- **Subtyping / Polymorphism:** A subclass is an instance of the superclass (`is-a` relationship), allowing instances of the subclass to be substituted wherever the superclass type is expected.

### Why Single Inheritance of Classes?
Java strictly enforces **single inheritance for classes** (a class can extend at most one superclass). This deliberate architectural decision eliminated the complex ambiguities of C++ multiple inheritance (the "Diamond Problem" of multiple state inheritance and object memory slicing). Multiple inheritance of behavior is supported safely through **Interfaces**.

---

## 2. Core Concepts

### 2.1 The `extends` and `super` Keywords
- `extends`: Establishes the inheritance relationship (`public class Dog extends Animal`).
- `super`: Refers to the immediate parent class, used to invoke superclass constructors (`super(name)`) or parent method implementations (`super.makeSound()`).

### 2.2 Method Overriding vs Method Hiding
- **Method Overriding:** A subclass provides a specialized implementation of an instance method defined in its superclass. Invocation is resolved at runtime via dynamic dispatch.
- **Method Hiding:** If a subclass declares a field or a `static` method with the same name as in the superclass, the subclass member **hides** the parent member. Invocation is resolved at compile time based on the declared reference type.

### 2.3 Covariant Return Types
Since Java 5, an overriding method may declare a return type that is a **subtype** of the return type declared in the superclass method:
```java
class AnimalProducer {
    public Animal produce() { return new Animal(); }
}
class DogProducer extends AnimalProducer {
    @Override
    public Dog produce() { return new Dog(); } // Legal: Dog is a subtype of Animal!
}
```

---

## 3. How It Works Internally

### 3.1 Object Memory Layout Under Inheritance
When a subclass is instantiated, the heap object contains a single contiguous block containing:
1. Object Header (Mark Word + Klass Pointer)
2. All fields inherited from `java.lang.Object` (none)
3. All fields inherited from the direct superclass (including `private` fields!)
4. All fields declared in the subclass
5. Alignment padding

```
Memory Layout of Dog (Subclass of Animal):
┌────────────────────────────────────────────────────────┐
│                      Dog Object                        │
├──────────────────────────┬─────────────────────────────┤
│ Mark Word                │ 8 bytes                     │
│ Klass Pointer            │ 4 bytes                     │
├──────────────────────────┼─────────────────────────────┤
│ Superclass Animal Fields │ private String species (4B) │
│                          │ int age (4B)                │
├──────────────────────────┼─────────────────────────────┤
│ Subclass Dog Fields      │ private String breed (4B)   │
├──────────────────────────┼─────────────────────────────┤
│ Padding                  │ 0 bytes                     │
└──────────────────────────┴─────────────────────────────┘
```
Even though `species` is `private` in `Animal` and inaccessible to `Dog`'s code directly, it is physically present in `Dog`'s heap memory allocation!

### 3.2 Virtual Method Table (`vtable`)
In HotSpot, every class has a `vtable` containing function pointers to the bytecode implementations of all virtual methods.
- When `Dog` overrides `makeSound()`, `Dog`'s `vtable` slot for `makeSound` points to `Dog::makeSound`.
- Unoverridden methods point to the superclass method address.

---

## 4. Syntax & Usage

### 4.1 Basic Inheritance Hierarchy
```java
public class Employee {
    private final String id;
    private final String name;
    protected double baseSalary;

    public Employee(String id, String name, double baseSalary) {
        this.id = id;
        this.name = name;
        this.baseSalary = baseSalary;
    }

    public double calculatePay() {
        return baseSalary;
    }

    public String getId() { return id; }
    public String getName() { return name; }
}

public class Manager extends Employee {
    private double quarterlyBonus;

    public Manager(String id, String name, double baseSalary, double quarterlyBonus) {
        super(id, name, baseSalary); // Mandatory call to super constructor
        this.quarterlyBonus = quarterlyBonus;
    }

    @Override
    public double calculatePay() {
        // Calling parent method logic
        return super.calculatePay() + quarterlyBonus;
    }

    public void setBonus(double bonus) {
        this.quarterlyBonus = bonus;
    }
}
```

---

## 5. Practical Examples

### Example 1: Demonstrating Covariant Return Types
```java
class Vehicle {
    public Vehicle copy() {
        return new Vehicle();
    }
}

class SportsCar extends Vehicle {
    @Override
    public SportsCar copy() { // Covariant: returns SportsCar instead of Vehicle
        return new SportsCar();
    }
}

public class CovariantDemo {
    public static void main(String[] args) {
        SportsCar car = new SportsCar();
        // Caller does NOT need an explicit cast!
        SportsCar cloned = car.copy();
        System.out.println("Cloned car type: " + cloned.getClass().getSimpleName());
    }
}
```
**Output:**
```text
Cloned car type: SportsCar
```

### Example 2: Real-World Template Method Pattern
```java
public abstract class ReportGenerator {
    // Template Method (final prevents subclasses from changing algorithm skeleton)
    public final void generateReport() {
        fetchData();
        formatContent();
        exportOutput();
    }

    protected abstract void fetchData();
    protected abstract void formatContent();

    // Hook method with default implementation
    protected void exportOutput() {
        System.out.println("Default: Printing report to standard console.");
    }
}

public class PdfReportGenerator extends ReportGenerator {
    @Override
    protected void fetchData() {
        System.out.println("PDF: Fetching data from SQL database.");
    }

    @Override
    protected void formatContent() {
        System.out.println("PDF: Formatting data into 2-column tabular layout.");
    }

    @Override
    protected void exportOutput() {
        System.out.println("PDF: Exporting binary PDF stream to /reports/output.pdf.");
    }

    public static void main(String[] args) {
        ReportGenerator report = new PdfReportGenerator();
        report.generateReport();
    }
}
```
**Output:**
```text
PDF: Fetching data from SQL database.
PDF: Formatting data into 2-column tabular layout.
PDF: Exporting binary PDF stream to /reports/output.pdf.
```

### Example 3: Edge Case – Field Hiding vs Method Overriding
```java
class ParentNode {
    public String label = "PARENT_LABEL";
    public void print() {
        System.out.println("Parent method: " + label);
    }
}

class ChildNode extends ParentNode {
    // HIDES ParentNode.label (Does NOT override it!)
    public String label = "CHILD_LABEL";

    @Override
    public void print() {
        System.out.println("Child method:  " + label);
    }
}

public class FieldHidingDemo {
    public static void main(String[] args) {
        ChildNode child = new ChildNode();
        ParentNode parentRef = child; // Upcasting

        // Fields are resolved based on compile-time REFERENCE TYPE!
        System.out.println("parentRef.label: " + parentRef.label); // "PARENT_LABEL"
        System.out.println("child.label:     " + child.label);     // "CHILD_LABEL"

        // Methods are resolved based on runtime OBJECT TYPE!
        parentRef.print(); // "Child method:  CHILD_LABEL"
    }
}
```
**Output:**
```text
parentRef.label: PARENT_LABEL
child.label:     CHILD_LABEL
Child method:  CHILD_LABEL
```

### Example 4: Failure Case – The Fragile Base Class Problem
```java
import java.util.Collection;
import java.util.HashSet;
import java.util.List;

// Broken Subclass attempting to count added items via inheritance
class InstrumentedHashSet<E> extends HashSet<E> {
    private int addCount = 0;

    @Override
    public boolean add(E e) {
        addCount++;
        return super.add(e);
    }

    @Override
    public boolean addAll(Collection<? extends E> c) {
        // FATAL FLAW: HashSet.addAll() internally calls add()!
        // Incrementing here and then calling super.addAll() doubles the count!
        addCount += c.size();
        return super.addAll(c);
    }

    public int getAddCount() { return addCount; }
}

public class FragileBaseClassDemo {
    public static void main(String[] args) {
        InstrumentedHashSet<String> set = new InstrumentedHashSet<>();
        set.addAll(List.of("A", "B", "C"));

        // Expected 3, but prints 6 because HashSet.addAll calls add() internally!
        System.out.println("Added items count: " + set.getAddCount());
    }
}
```
**Output:**
```text
Added items count: 6
```

### Example 5: Optimized / Idiomatic – Composition Over Inheritance Solution
```java
import java.util.Collection;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

// Robust Decorator Pattern using composition instead of inheritance
public class SafeInstrumentedSet<E> {
    private final Set<E> delegate;
    private int addCount = 0;

    public SafeInstrumentedSet(Set<E> delegate) {
        this.delegate = delegate;
    }

    public boolean add(E e) {
        addCount++;
        return delegate.add(e);
    }

    public boolean addAll(Collection<? extends E> c) {
        addCount += c.size();
        return delegate.addAll(c); // Independent of internal delegate implementation!
    }

    public int getAddCount() { return addCount; }

    public static void main(String[] args) {
        SafeInstrumentedSet<String> set = new SafeInstrumentedSet<>(new HashSet<>());
        set.addAll(List.of("A", "B", "C"));
        System.out.println("Correct count: " + set.getAddCount());
    }
}
```
**Output:**
```text
Correct count: 3
```

---

## 6. Best Practices

1. **Favor Composition Over Inheritance:** Unless there is a genuine `is-a` relationship and the subclass will never violate superclass invariants, use composition and forwarding (Effective Java Item 18).
2. **Always Use `@Override`:** Always annotate overriding methods with `@Override`. It prevents silent bugs when method parameters or casings change.
3. **Obey the Liskov Substitution Principle (LSP):** Subtypes must be substitutable for their base types without altering program correctness. A subclass must not weaken preconditions or strengthen postconditions.
4. **Design for Inheritance or Prohibit It:** If a class is not specifically engineered, documented, and tested for extension, mark it `final` (Effective Java Item 19).
5. **Never Hide Fields:** Never declare an instance field in a subclass with the same name as a superclass field. It creates confusion and reference-dependent access.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Is Wrong | Correct Idiom |
| :--- | :--- | :--- |
| Overriding without `@Override` | Typos in method signature create an accidental overload rather than an override. | Always annotate with `@Override`. |
| Weakening method access | An overriding method cannot have weaker visibility than the parent (e.g. `public` to `protected`). | Keep same or broader access (`protected` to `public`). |
| Throwing broader checked exceptions | Overriding methods can only throw same, narrower, or fewer checked exceptions than parent. | Throw matching or subclass exceptions. |
| The Fragile Base Class Problem | Subclass breaks when parent class implementation details change. | Use composition and delegation. |
| Forgetting `super()` call when parent has no default ctor | Subclass fails compilation. | Call `super(args)` explicitly. |
| Hiding fields | Subclass and parent have fields with same name; behavior varies depending on reference type. | Use unique field names. |

---

## 8. Performance & Security Notes

### 8.1 Monomorphic Call Inlining
If the HotSpot C2 compiler detects that a class hierarchy has only one loaded implementation of an overridden method (monomorphic call site), it can eliminate the `vtable` lookup entirely and inline the method body directly. If a second subclass is loaded later, the JVM dynamically deoptimizes the code using a safepoint deoptimization trap.

---

## 9. Comparison: Inheritance vs Composition

| Dimension | Inheritance (`is-a`) | Composition (`has-a`) |
| :--- | :--- | :--- |
| **Coupling** | Tight (white-box reuse) | Loose (black-box reuse) |
| **Encapsulation** | May breach parent encapsulation | Fully preserves encapsulation |
| **Flexibility** | Fixed at compile time | Swappable dynamically at runtime |
| **Fragile Base Class**| Highly vulnerable | Immune |
| **Polymorphism** | Built-in via subtyping | Via interfaces |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the Liskov Substitution Principle (LSP)?</strong></summary>

The Liskov Substitution Principle states that if class `S` is a subtype of class `T`, objects of type `T` ought to be replaceable with objects of type `S` without altering any of the desirable properties of the program (correctness, performance, invariant guarantees). A subclass must not strengthen preconditions (e.g. demanding stricter arguments) or weaken postconditions (e.g. returning broader unverified outputs).
</details>

<details>
<summary><strong>2. Why does Java not support multiple inheritance of classes?</strong></summary>

To prevent the "Diamond Problem" of multiple inheritance. If class `D` extends both `B` and `C`, and both inherit from `A`, ambiguities arise if both `B` and `C` override a method or maintain duplicate copies of `A`'s fields. Resolving memory layouts, constructor execution orders, and dispatch tables in multiple state inheritance creates significant complexity and bugs.
</details>

<details>
<summary><strong>3. What are covariant return types in Java?</strong></summary>

Introduced in Java 5, covariant return types allow an overriding subclass method to declare a return type that is a subclass/subtype of the return type declared in the superclass method. This eliminates the need for callers to perform explicit type casting when invoking methods on subclass references.
</details>

<details>
<summary><strong>4. What rules apply to checked exceptions in method overriding?</strong></summary>

An overriding method can throw:
1. No exceptions at all.
2. The exact same checked exceptions as the superclass method.
3. Subclasses of the checked exceptions declared by the superclass method.
4. Any unchecked (`RuntimeException` or `Error`) exceptions.
It **cannot** throw new or broader checked exceptions that the superclass method did not declare.
</details>

<details>
<summary><strong>5. What is the difference between method overriding and method hiding?</strong></summary>

Method overriding applies to instance methods; the method executed is determined dynamically at runtime based on the actual object type in the heap (`invokevirtual`). Method hiding occurs when a subclass declares a `static` method with the same signature as a superclass `static` method; the method executed is determined at compile time based on the declared reference type (`invokestatic`).
</details>

<details>
<summary><strong>6. What is the Fragile Base Class problem?</strong></summary>

The Fragile Base Class problem occurs when changes to a superclass's implementation inadvertently break the functionality of its subclasses. Because subclasses often depend on the internal invocation patterns of their parent (such as `addAll()` delegating to `add()`), any change to the parent class's internal behavior can corrupt subclass invariants.
</details>

<details>
<summary><strong>7. Are private fields inherited by a subclass?</strong></summary>

Private fields are physically present in the heap memory allocated for the subclass object, but they are not directly accessible by name in the subclass code. They can only be read or modified through inherited non-private methods (getters/setters).
</details>

<details>
<summary><strong>8. Can an overriding method have narrower access privileges than the overridden method?</strong></summary>

No. An overriding method cannot reduce the visibility of the parent method. If a superclass method is `public`, the subclass method must be `public`. If the parent method is `protected`, the subclass method may be `protected` or `public`, but cannot be package-private or `private`.
</details>

---

## 11. Practice Exercises

### Easy
1. **Animal Hierarchy:** Implement an `Animal` base class and a `Dog` subclass with method overriding and `super.makeSound()`.
2. **Covariant Cloner:** Implement a class hierarchy where each subclass overrides a `cloneObject()` method with a covariant return type.
3. **Salary Calculator:** Build an `Employee` and `SalesExecutive` hierarchy that calculates commission-based pay.

### Medium
4. **Fragile Base Class Demonstration:** Write a small test demonstrating duplicate counting in a subclass and refactor it using the Decorator Pattern.
5. **Template Method Cooking:** Create an abstract `CookingRecipe` with a final template method and customizable preparation steps.
6. **LSP Violation Detector:** Identify and correct an LSP violation in a `Rectangle` vs `Square` hierarchy.

### Hard
7. **Bytecode Inspection of Bridge Methods:** Compile a class with covariant return types and use `javap -c -v` to inspect the synthetic bridge method generated by `javac`.
8. **Multi-Level Dispatch Benchmark:** Measure execution latency of 4-level deep virtual method dispatch vs direct interface dispatch.
9. **Dynamic Method Handle Dispatch:** Invoke an overridden superclass method from a grandchild class using Java `MethodHandles.lookup()`.

<details>
<summary><strong>Exercise Solutions</strong></summary>

```java
// Solution to Exercise 1: Animal Hierarchy
public class Animal {
    private final String name;
    public Animal(String name) { this.name = name; }
    public void makeSound() { System.out.println(name + " makes a generic sound."); }
    public String getName() { return name; }
}

public class Dog extends Animal {
    public Dog(String name) { super(name); }
    @Override
    public void makeSound() {
        super.makeSound();
        System.out.println(getName() + " barks!");
    }
}
```
</details>

---

## 12. Summary & Cheat Sheet

```
┌────────────────────────────────────────────────────────────────────────┐
│                        INHERITANCE CHEAT SHEET                         │
├────────────────────────────────────────────────────────────────────────┤
│ Syntax:             class Sub extends Super {}                         │
│ Multiple Inherit:   DISALLOWED for classes (Single inheritance only)   │
│ Constructor:        super(...) must be called (explicitly or default)  │
│ Overriding:         @Override; same signature; runtime virtual dispatch│
│ Covariant Return:   Overriding method can return a subtype             │
│ Exceptions:         Overriding method cannot throw broader checked ex  │
│ Visibility:         Overriding method cannot reduce access visibility  │
│ Field Hiding:       Fields resolved by reference type, NOT object type │
│ Core Rule:          Favor composition over inheritance (Item 18)       │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 13. Further Reading
- Java Language Specification (JLS) Section 8.4.8: Inheritance, Overriding, and Hiding
- *Effective Java (3rd Edition)*, Joshua Bloch, Item 18: "Favor composition over inheritance".
- *Effective Java (3rd Edition)*, Joshua Bloch, Item 19: "Design and document for inheritance or else prohibit it".
