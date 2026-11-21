# Abstract Classes in Java: Partial Abstraction and Template Methods

> A comprehensive deep-dive into abstract classes, partial implementations, abstract methods, constructor initialization of shared state, and design trade-offs vs interfaces.

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
9. [Comparison: Abstract Class vs Interface](#9-comparison-abstract-class-vs-interface)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

An **Abstract Class** is a class declared with the `abstract` modifier that represents an incomplete blueprint. It cannot be directly instantiated with the `new` operator; it exists exclusively to be subclassed by concrete subclasses that provide implementations for its abstract methods.

Key motivations:
- **Shared State & Invariants:** Unlike interfaces, abstract classes can hold non-static instance fields (including `private` and `protected` fields) and enforce state validation via constructors.
- **Template Method Pattern:** Defines the invariant skeleton of an algorithm in a concrete method, deferring specific variant steps to abstract hook methods implemented by subclasses.
- **Controlled Visibility:** Abstract methods can be `protected` or package-private, restricting override hooks to trusted package members or subclasses, whereas interface methods are predominantly public.

---

## 2. Core Concepts

### 2.1 Abstract Class Anatomy
```java
public abstract class DataProcessor {
    // 1. Instance Fields (State)
    private final String processorName;
    protected long recordsProcessed = 0;

    // 2. Constructor (Invoked via super() by subclasses)
    public DataProcessor(String processorName) {
        if (processorName == null) throw new IllegalArgumentException("Name required");
        this.processorName = processorName;
    }

    // 3. Abstract Method (Must be implemented by concrete subclasses)
    public abstract void processRecord(byte[] data);

    // 4. Concrete Method (Shared logic)
    public final void runBatch(List<byte[]> batch) {
        for (byte[] item : batch) {
            processRecord(item);
            recordsProcessed++;
        }
    }

    public String getProcessorName() { return processorName; }
    public long getRecordsProcessed() { return recordsProcessed; }
}
```

### 2.2 Rules of Abstract Classes
- An abstract class **cannot be instantiated** directly (`new DataProcessor(...)` fails compilation).
- A class with **even one abstract method** MUST be declared `abstract`.
- An abstract class is **not required** to have abstract methods (declaring it abstract simply prevents instantiation).
- A concrete subclass extending an abstract class **must implement all inherited abstract methods**; otherwise, the subclass must also be declared `abstract`.

---

## 3. How It Works Internally

### 3.1 Class-File Metadata: `ACC_ABSTRACT`
In compiled bytecode:
- The class's `access_flags` has the `ACC_ABSTRACT` bit (`0x0400`) set.
- When JVM instruction `new` targets a class with `ACC_ABSTRACT`, the bytecode verifier rejects execution with `java.lang.InstantiationError`.
- Abstract methods have `ACC_ABSTRACT` set and have **no `Code` attribute** in bytecode.

### 3.2 Constructor Execution in Abstract Classes
Even though an abstract class cannot be instantiated directly, it has a fully functional `<init>` constructor bytecode method. When a subclass executes `super(...)`, it invokes the abstract class's `<init>` method to initialize inherited fields and establish invariants.

---

## 4. Syntax & Usage

### 4.1 Subclassing and Implementation
```java
public class JsonDataProcessor extends DataProcessor {
    public JsonDataProcessor() {
        super("JSON-Processor-V1");
    }

    @Override
    public void processRecord(byte[] data) {
        // Concrete implementation
        System.out.println("Parsing JSON payload of length: " + data.length);
    }
}
```

---

## 5. Practical Examples

### Example 1: Template Method Pattern for File Exporters
```java
import java.io.File;

public abstract class AbstractFileExporter {
    // Template Method (final prevents breaking algorithm sequence)
    public final void export(String destinationPath) {
        validateDestination(destinationPath);
        byte[] payload = serializeData();
        writeFile(destinationPath, payload);
        postExportCleanup();
    }

    private void validateDestination(String path) {
        if (path == null || path.isBlank()) throw new IllegalArgumentException("Path required");
        System.out.println("Validated destination path: " + path);
    }

    // Abstract Step (Deferred to subclass)
    protected abstract byte[] serializeData();

    // Hook Method (Optional override)
    protected void postExportCleanup() {
        System.out.println("Default cleanup completed.");
    }

    private void writeFile(String path, byte[] payload) {
        System.out.printf("Wrote %d bytes to %s%n", payload.length, path);
    }
}

class CsvExporter extends AbstractFileExporter {
    @Override
    protected byte[] serializeData() {
        return "id,name,role\n1,Alice,Admin\n".getBytes();
    }
}

public class ExporterDemo {
    public static void main(String[] args) {
        AbstractFileExporter exporter = new CsvExporter();
        exporter.export("/var/data/users.csv");
    }
}
```
**Output:**
```text
Validated destination path: /var/data/users.csv
Wrote 26 bytes to /var/data/users.csv
Default cleanup completed.
```

### Example 2: Real-World Database Connection Template
```java
public abstract class AbstractDatabaseGateway {
    private final String connectionString;
    private boolean connected = false;

    public AbstractDatabaseGateway(String connectionString) {
        this.connectionString = connectionString;
    }

    public final void executeQuery(String sql) {
        if (!connected) connect();
        try {
            doExecute(sql);
        } finally {
            disconnect();
        }
    }

    private void connect() {
        System.out.println("Connecting to: " + connectionString);
        connected = true;
    }

    private void disconnect() {
        System.out.println("Disconnected.");
        connected = false;
    }

    protected abstract void doExecute(String sql);
}

class PostgresGateway extends AbstractDatabaseGateway {
    public PostgresGateway() {
        super("jdbc:postgresql://localhost:5432/app");
    }

    @Override
    protected void doExecute(String sql) {
        System.out.println("PostgreSQL executing: " + sql);
    }

    public static void main(String[] args) {
        AbstractDatabaseGateway db = new PostgresGateway();
        db.executeQuery("SELECT * FROM orders;");
    }
}
```
**Output:**
```text
Connecting to: jdbc:postgresql://localhost:5432/app
PostgreSQL executing: SELECT * FROM orders;
Disconnected.
```

### Example 3: Edge Case – Abstract Class with No Abstract Methods
```java
// Legal: Declaring a class abstract without abstract methods
// Purpose: Explicitly prevents instantiation, forcing callers to subclass
public abstract class FrameworkConfigDefaults {
    public int getMaxRetries() { return 3; }
    public long getTimeoutMs() { return 5000L; }
}

class ProductionConfig extends FrameworkConfigDefaults {
    @Override
    public long getTimeoutMs() { return 10000L; }
}

public class AbstractNoMethodsDemo {
    public static void main(String[] args) {
        // FrameworkConfigDefaults cfg = new FrameworkConfigDefaults(); // COMPILE ERROR!
        ProductionConfig prod = new ProductionConfig();
        System.out.println("Timeout: " + prod.getTimeoutMs() + " ms");
    }
}
```
**Output:**
```text
Timeout: 10000 ms
```

### Example 4: Failure Case – Attempting `abstract final` or `abstract static`
```java
public class AbstractSyntaxContradictions {
    // COMPILATION ERROR: illegal combination of modifiers: abstract and final
    // public abstract final class BrokenClass {}

    // COMPILATION ERROR: illegal combination of modifiers: abstract and static
    // public abstract static void test();

    public static void main(String[] args) {
        System.out.println("Abstract cannot be combined with final, static, or private!");
    }
}
```
**Output:**
```text
Abstract cannot be combined with final, static, or private!
```

### Example 5: Optimized / Idiomatic – Skeletal Implementation (Effective Java Item 20)
```java
// 1. Interface defines public API contract
interface CustomList<E> {
    int size();
    E get(int index);
    boolean isEmpty();
}

// 2. Skeletal Abstract Implementation provides boilerplate
abstract class AbstractCustomList<E> implements CustomList<E> {
    @Override
    public boolean isEmpty() {
        return size() == 0;
    }
}

// 3. Concrete class only implements variant methods
class FixedArrayCustomList<E> extends AbstractCustomList<E> {
    private final E[] elements;

    @SafeVarargs
    public FixedArrayCustomList(E... elements) {
        this.elements = elements;
    }

    @Override
    public int size() { return elements.length; }

    @Override
    public E get(int index) { return elements[index]; }
}

public class SkeletalImplementationDemo {
    public static void main(String[] args) {
        CustomList<String> list = new FixedArrayCustomList<>("Alpha", "Beta", "Gamma");
        System.out.println("Size: " + list.size() + ", Empty: " + list.isEmpty() + ", First: " + list.get(0));
    }
}
```
**Output:**
```text
Size: 3, Empty: false, First: Alpha
```

---

## 6. Best Practices

1. **Prefer Skeletal Implementations:** Combine interfaces with abstract classes (e.g. `List` + `AbstractList`). The interface provides the multi-inheritance type contract, while the abstract class provides default boilerplate logic.
2. **Make Constructors `protected` or `public`:** Even though abstract classes cannot be instantiated with `new`, constructors are invoked by subclasses via `super(...)`.
3. **Use the Template Method Pattern for Invariant Flows:** Mark template methods `final` so subclasses cannot corrupt the execution ordering or error handling.
4. **Keep Visibility of Hook Methods `protected`:** Expose override hooks strictly to subclasses, not to external callers.
5. **Never Combine `abstract` with `final` or `private`:** `abstract` demands overriding; `final` and `private` prohibit it.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Is Wrong | Correct Idiom |
| :--- | :--- | :--- |
| Trying to instantiate abstract class | Compile error: `Class is abstract; cannot be instantiated`. | Instantiate a concrete subclass. |
| `abstract private` method | Private methods cannot be seen or overridden by subclasses. | Make method `protected abstract`. |
| `abstract final` method or class | Contradictory modifiers; `final` prohibits overriding while `abstract` mandates it. | Choose either `abstract` or `final`. |
| Missing super constructor call | If abstract class has no no-arg constructor, subclass fails to compile without explicit `super(...)`. | Call `super(args)` in subclass constructor. |
| Overusing abstract classes for type hierarchy | Single inheritance limit locks subclasses into rigid trees. | Use interfaces for types, abstract classes only for skeletal implementation. |

---

## 8. Performance & Security Notes

### 8.1 Constructor Security & State Protection
Abstract class constructors are the primary line of defense for class hierarchy invariants. If an argument is invalid or malicious, throwing an `IllegalArgumentException` in the abstract class constructor halts object instantiation before subclass memory or state can be initialized.

---

## 9. Comparison: Abstract Class vs Interface

| Dimension | Abstract Class | Interface (Java 8–25) |
| :--- | :--- | :--- |
| **Inheritance** | Single inheritance (`extends`) | Multiple inheritance (`implements`) |
| **Instance Fields**| ✅ Arbitrary mutable/final fields | ❌ None (Only `public static final` constants) |
| **Constructors** | ✅ Full constructors (`<init>`) | ❌ No constructors |
| **Access Visibility**| `public`, `protected`, package-private | Methods `public` (or `private` since Java 9) |
| **Best Used For** | Shared state, template methods, skeletal base | Defining API types, behavior contracts |

---

## 10. Interview Questions

<details>
<summary><strong>1. Can an abstract class have a constructor? If so, how is it invoked?</strong></summary>

Yes. Abstract classes can and often do have constructors. Although an abstract class cannot be instantiated directly with `new`, its constructor is invoked by subclass constructors via `super(...)` to initialize state and enforce invariants.
</details>

<details>
<summary><strong>2. Can an abstract class be declared without any abstract methods?</strong></summary>

Yes. An abstract class is not required to declare any abstract methods. Declaring a class `abstract` simply informs the compiler that the class cannot be instantiated directly with `new`, which is useful for utility bases or base configurations.
</details>

<details>
<summary><strong>3. Can an abstract method be declared static or final?</strong></summary>

No. `static` methods belong to the class and are resolved at compile time without virtual dispatch. `final` methods prohibit overriding. Because `abstract` methods have no body and exist solely to be overridden at runtime, combining `abstract` with `static` or `final` is a compilation error.
</details>

<details>
<summary><strong>4. What is a "Skeletal Implementation" (Effective Java Item 20)?</strong></summary>

A Skeletal Implementation (also known as an Abstract Interface) pairs an interface with an abstract class (e.g. `Collection` and `AbstractCollection`, `Map` and `AbstractMap`). The interface defines the full type contract and allows multiple inheritance. The abstract skeletal class provides default implementations for non-primitive methods, dramatically reducing the code required to build a custom concrete implementation.
</details>

<details>
<summary><strong>5. Can an abstract class implement an interface without implementing its methods?</strong></summary>

Yes. An abstract class that implements an interface is not required to provide implementations for the interface's abstract methods. The obligation to implement those methods is passed down to the first concrete subclass that extends the abstract class.
</details>

<details>
<summary><strong>6. What is the difference between an abstract class and an interface in modern Java?</strong></summary>

While modern interfaces support `default`, `static`, and `private` methods, they **cannot hold instance fields (state)** and **cannot have constructors**. An abstract class can have instance fields, constructors, and `protected` methods, but a class can extend only one abstract class, whereas it can implement multiple interfaces.
</details>

<details>
<summary><strong>7. What happens if a concrete subclass fails to implement all abstract methods of its parent?</strong></summary>

The Java compiler will report an error: `SubClass is not abstract and does not override abstract method in SuperClass`. To resolve this, the subclass must either implement all missing abstract methods or be declared `abstract` itself.
</details>

<details>
<summary><strong>8. What is the Template Method design pattern?</strong></summary>

The Template Method pattern defines the invariant skeleton of an algorithm in a concrete (usually `final`) method of an abstract class. The individual variant steps of the algorithm are defined as abstract or hook methods that concrete subclasses override to provide specific behavior without altering the overarching algorithm structure.
</details>

---

## 11. Practice Exercises

### Easy
1. **Abstract Graphic:** Implement an abstract `Graphic` class with an abstract method `draw()` and a concrete subclass `Circle`.
2. **Abstract Sensor:** Create an abstract `Sensor` with a protected `id` field and constructor.
3. **Prevent Direct Instantiation:** Create a class with only concrete methods declared `abstract` and verify it cannot be instantiated with `new`.

### Medium
4. **ETL Pipeline Template Method:** Build an abstract `EtlPipeline` with steps `extract()`, `transform()`, `load()` orchestrated by a final `run()` method.
5. **Skeletal Queue Implementation:** Create an interface `SimpleQueue` and an abstract skeletal class implementing common queue operations.
6. **Abstract Cache Template:** Design an abstract `AbstractCache` with concrete `put()` and `get()` methods delegating to abstract `evict()` hooks.

### Hard
7. **Bytecode Verification of Abstract Methods:** Use `javap -c -v` on an abstract class to verify that abstract methods lack the `Code` attribute and carry `ACC_ABSTRACT`.
8. **Dynamic Abstract Class Proxy:** Write a utility that uses ByteBuddy or JDK dynamic proxies to provide runtime implementations of an abstract class.
9. **Multi-Tier Skeletal Hierarchy:** Build a complete collections-like hierarchy (`CustomCollection -> AbstractCustomCollection -> AbstractCustomSet -> HashSetImpl`).

<details>
<summary><strong>Exercise Solutions</strong></summary>

```java
// Solution to Exercise 1: Abstract Graphic
public abstract class Graphic {
    private final String color;

    public Graphic(String color) {
        this.color = color;
    }

    public abstract void draw();
    public String getColor() { return color; }
}

public class Circle extends Graphic {
    private final double radius;

    public Circle(String color, double radius) {
        super(color);
        this.radius = radius;
    }

    @Override
    public void draw() {
        System.out.printf("Drawing %s circle with radius %.2f%n", getColor(), radius);
    }
}
```
</details>

---

## 12. Summary & Cheat Sheet

```
┌────────────────────────────────────────────────────────────────────────┐
│                        ABSTRACT CLASSES CHEAT SHEET                    │
├────────────────────────────────────────────────────────────────────────┤
│ Syntax:             abstract class ClassName {}                        │
│ Instantiation:      CANNOT be instantiated with `new`                  │
│ Methods:            Can have both abstract and concrete methods        │
│ State:              Supports arbitrary instance fields (mutable/final) │
│ Constructors:       Fully supported; invoked by subclasses via super() │
│ Access Modifiers:   Abstract methods can be public or protected        │
│ Template Method:    Skeleton in final method; steps in abstract methods│
│ Skeletal Pattern:   Interface + Abstract Base (Effective Java Item 20) │
│ Incompatible:       Cannot combine abstract with final, static, private│
└────────────────────────────────────────────────────────────────────────┘
```

---

## 13. Further Reading
- Java Language Specification (JLS) Section 8.1.1.1: `abstract` Classes
- *Effective Java (3rd Edition)*, Joshua Bloch, Item 20: "Prefer interfaces to abstract classes".
- Design Patterns: Elements of Reusable Object-Oriented Software (GoF) - Template Method Pattern.
