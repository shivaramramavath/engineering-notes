# Records

> Deep dive into Java Records (JEP 395): transparent carriers for immutable data, canonical and compact constructors, record patterns (JEP 440), JVM bytecode representation, and serialization guarantees.

| Metadata | Value |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | [04-oop/00_classes-and-objects.md](../04-oop/00_classes-and-objects.md), [04-oop/01_constructors.md](../04-oop/01_constructors.md), [04-oop/14_equals-and-hashcode.md](../04-oop/14_equals-and-hashcode.md) |
| **Time to Read** | 26 minutes |
| **Status** | Active |
| **Java Version** | Java 14/15 Preview, Java 16 Final (JEP 395), Java 21 Record Patterns (JEP 440) |

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

### The Boilerplate Tax of Data Carriers
In traditional Java, writing a simple data-transfer object (DTO) or value carrier required dozens of lines of repetitive ceremony: private final fields, a verbose constructor assigning each field, accessor getters (`getX()`), an `equals()` implementation checking every component, a matching `hashCode()`, and a `toString()` method.

Lombok (`@Value`, `@Data`) emerged as a popular workaround, but relied on non-standard annotation processing and internal compiler hacking.

### The Solution: Records (JEP 395)
Introduced as final in Java 16, a **Record** is a nominal, transparent carrier of immutable data. By declaring `record Point(int x, int y) {}`, the language guarantees:
- All fields are `private final`.
- A canonical constructor initializing all components.
- Accessor methods with names matching the components (`x()`, not `getX()`).
- Correct, reflexive, symmetric `equals()`, matching `hashCode()`, and readable `toString()`.
- Built-in reflection metadata and special, secure serialization protocols.

---

## 2. Core Concepts

### Transparent Carriers
A record's state description is part of its public API contract. There is no encapsulation hiding what fields a record contains. The record header `record User(String id, String name)` explicitly states that the entire identity and state of a `User` consists of `id` and `name`.

### Record Class Restrictions
1. **Implicitly Final**: Records cannot be extended (`final class`).
2. **Superclass is `java.lang.Record`**: Records cannot extend any class (they implicitly extend `Record`), but they can implement arbitrary interfaces.
3. **No Additional Instance Fields**: A record cannot declare instance fields other than the components in its header. Static fields are permitted.
4. **Shallow Immutability**: The record's component references are `final`. However, if a component is a mutable object (e.g. `java.util.List`), the referenced object itself can still be modified unless defensively copied.

### Constructors: Canonical, Compact, and Custom
- **Canonical Constructor**: Matches the record header signature (`Point(int x, int y)`). Automatically provided if omitted.
- **Compact Constructor**: Has no parameter list in parentheses (`Point { ... }`). Variables `x` and `y` are implicitly in scope. Used for validation and normalization before field assignment.
- **Custom / Overloaded Constructor**: Secondary constructors must delegate to another constructor using `this(...)`.

---

## 3. How It Works Internally

### Bytecode Decompilation of `record Point(int x, int y)`
When compiled, `Point` produces the following bytecode equivalent:

```java
public final class Point extends java.lang.Record {
    private final int x;
    private final int y;

    public Point(int x, int y) {
        this.x = x;
        this.y = y;
    }

    public int x() { return this.x; }
    public int y() { return this.y; }

    @Override
    public final boolean equals(Object o) { /* invokedynamic bootstrap to ObjectMethods */ }

    @Override
    public final int hashCode() { /* invokedynamic bootstrap to ObjectMethods */ }

    @Override
    public final String toString() { /* invokedynamic bootstrap to ObjectMethods */ }
}
```

### Dynamic Method Bootstrapping
Rather than emitting static bytecodes for `equals`, `hashCode`, and `toString`, the compiler emits an `invokedynamic` call site targeting `java.lang.runtime.ObjectMethods.bootstrap`. This defers implementation to the JVM runtime, optimizing performance and reducing class file size.

### Secure Deserialization Semantics
Standard Java serialization allocates uninitialized heap memory and bypasses constructors. **Records are completely different**:
- Deserialization of a record **always** invokes the canonical constructor!
- The stream unpacks component values and passes them into the constructor, ensuring that validations in compact constructors cannot be bypassed by malicious binary payloads.

---

## 4. Syntax & Usage

### Compact Constructor for Validation and Normalization
```java
public record Range(int start, int end) {
    // Compact constructor: no parameter list!
    public Range {
        if (start > end) {
            throw new IllegalArgumentException("start (" + start + ") > end (" + end + ")");
        }
        // Fields are automatically assigned: this.start = start; this.end = end;
    }
}
```

### Defensive Copying in Compact Constructor
```java
import java.util.List;

public record TaskGroup(String name, List<String> tasks) {
    public TaskGroup {
        // Defensive copy guarantees true immutability!
        tasks = List.copyOf(tasks);
    }
}
```

---

## 5. Practical Examples

### Example 1: Basic Record with Accessors
```java
// Goal: Model 2D coordinate with distance calculation
public record Coordinate(double x, double y) {
    public double distanceTo(Coordinate other) {
        double dx = this.x - other.x;
        double dy = this.y - other.y;
        return Math.hypot(dx, dy);
    }
}

public class CoordinateDemo {
    public static void main(String[] args) {
        Coordinate c1 = new Coordinate(0, 0);
        Coordinate c2 = new Coordinate(3, 4);

        System.out.println("Distance: " + c1.distanceTo(c2));
        System.out.println("ToString: " + c1);
        System.out.println("Equals: " + c1.equals(new Coordinate(0, 0)));
    }
}
```
*Output:*
```
Distance: 5.0
ToString: Coordinate[x=0.0, y=0.0]
Equals: true
```

### Example 2: Real-World Record Pattern Matching (Java 21 JEP 440)
```java
// Goal: Deconstruct records directly in switch patterns
sealed interface Shape permits Circle, Rectangle {}
record Circle(double radius) implements Shape {}
record Rectangle(double width, double height) implements Shape {}

public class ShapeCalculator {
    public static double calculateArea(Shape shape) {
        return switch (shape) {
            case Circle(double r) -> Math.PI * r * r;
            case Rectangle(double w, double h) -> w * h;
        };
    }

    public static void main(String[] args) {
        Shape s = new Rectangle(5, 10);
        System.out.println("Calculated Area: " + calculateArea(s));
    }
}
```
*Output:*
```
Calculated Area: 50.0
```
*Explanation:* Java 21 Record Patterns deconstruct the record components directly in the case clause, eliminating manual casting and accessor calls.

### Example 3: Edge Case: Local Records Inside Methods
```java
// Goal: Use records as intermediate, zero-cost data carriers within stream pipelines
import java.util.*;

public class LocalRecordDemo {
    public static void main(String[] args) {
        List<String> words = List.of("apple", "banana", "cherry", "date");

        // Local record scoped only inside main!
        record WordStats(String word, int length) {}

        WordStats longest = words.stream()
            .map(w -> new WordStats(w, w.length()))
            .max(Comparator.comparingInt(WordStats::length))
            .orElseThrow();

        System.out.println("Longest word: " + longest.word() + " (" + longest.length() + ")");
    }
}
```
*Output:*
```
Longest word: banana (6)
```
*Explanation:* Local records provide clean, type-safe tuple replacements without polluting package namespaces.

### Example 4: Failure Case: Assuming Mutable Collections inside Records are Immutable
```java
// Goal: Illustrate shallow immutability hazard
import java.util.*;

record FlawedTeam(String name, List<String> members) {}

public class ShallowImmutabilityHazard {
    public static void demonstrate() {
        List<String> list = new ArrayList<>(List.of("Alice", "Bob"));
        FlawedTeam team = new FlawedTeam("Alpha", list);

        // MUTATION HAZARD:
        list.add("Mallory"); // Modifies team's internal state!
        System.out.println("Team members: " + team.members()); // Alice, Bob, Mallory
    }
}
```
*Explanation:* Records are shallowly immutable. Always use `List.copyOf()` or `Collections.unmodifiableList()` in compact constructors.

---

## 6. Best Practices
1. **Defensively Copy Collections**: Always wrap mutable collection parameters with `List.copyOf()`, `Set.copyOf()`, or `Map.copyOf()` in compact constructors.
2. **Use Compact Constructors for Invariant Checks**: Prefer compact constructors (`public RecordName { ... }`) over full canonical constructors to eliminate redundant assignments.
3. **Keep Records As Pure Data Carriers**: Avoid adding complex business workflow logic into records. Use them for DTOs, API payloads, query results, and composite keys.
4. **Leverage Local Records**: Use local records inside method bodies and stream transformations instead of `Map.Entry` or ad-hoc arrays.
5. **Combine with Sealed Interfaces**: Pair records with sealed interfaces to construct robust Algebraic Data Types (ADTs).

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why Wrong | Fix |
|---|---|---|
| Attempting to extend a record | Records are implicitly `final` and extend `java.lang.Record`. | Use composition or implement interfaces instead. |
| Declaring additional instance fields | Records only permit instance fields defined in their header. | Put extra state into header or use `static` fields. |
| Calling accessors as `getX()` | Record accessors match the component name directly (`x()`, not `getX()`). | Call `record.x()` instead of JavaBean getters. |
| Forgetting defensive copying on collections | External callers can mutate the collection, breaking immutability. | Use `tasks = List.copyOf(tasks);` in compact constructor. |
| Overriding `equals`/`hashCode` improperly | Defeats the compiler's optimized dynamic bootstrapping and can break record patterns. | Rely on the compiler-generated `equals` and `hashCode`. |
| Using records for JPA entities | JPA/Hibernate requires mutable no-arg constructors, proxying, and dirty tracking incompatible with final records. | Keep JPA entities as classes; use records for read-only projection DTOs. |

---

## 8. Performance & Security Notes

### Memory Footprint & Escape Analysis
Because records have known, immutable component fields, the HotSpot C2 compiler aggressively applies **Scalar Replacement**. If a local record instance does not escape the calling method, the JVM allocates its components directly into CPU registers or stack memory, entirely bypassing heap allocation and GC pressure.

### Serialization Security
Unlike traditional classes, records cannot be deserialized into corrupted states via stream tampering. The Java deserialization mechanism reads the stream fields and passes them directly to the record's canonical constructor, ensuring all validation rules execute unconditionally.

---

## 9. Comparison / When to Use What

| Feature | Java Record | Lombok `@Value` | Traditional Class |
|---|---|---|---|
| **Language Standard** | Official Java 16+ SE | Third-party plugin | Standard Java 1.0+ |
| **Immutability** | Enforced by JVM | Compile-time bytecode rewrite | Manual discipline |
| **Extensible** | No (Final) | No (Final) | Yes (unless final) |
| **Constructor Validation** | Compact Constructor | Custom constructor | Manual boilerplate |
| **Pattern Matching** | Native Record Patterns | Not supported natively | Not supported natively |
| **Reflection Metadata** | `Class.isRecord()` | Regular class reflection | Regular reflection |

---

## 10. Interview Questions

<details>
<summary>1. What is a Java Record and what does the compiler generate for it?</summary>
A record is a concise declaration of a class whose purpose is to carry immutable data. The compiler generates private final fields, canonical constructor, component-named accessors, `equals()`, `hashCode()`, `toString()`, and extends `java.lang.Record`.
</details>

<details>
<summary>2. What is a Compact Constructor?</summary>
A special constructor syntax without parameter parentheses (`public Point { ... }`). It runs before instance fields are assigned, allowing validation and normalization of arguments without boilerplate `this.x = x;` statements.
</details>

<details>
<summary>3. Can a record extend another class or be extended?</summary>
No. Records are implicitly `final` and implicitly extend `java.lang.Record`. They cannot extend any class, nor can any class extend them. They can, however, implement interfaces.
</details>

<details>
<summary>4. Are records deeply immutable?</summary>
No. Records are shallowly immutable. Their references cannot be reassigned, but if a component refers to a mutable object (like `ArrayList`), that object's contents can be mutated unless defensively copied.
</details>

<details>
<summary>5. How does record deserialization differ from standard class deserialization?</summary>
Standard class deserialization allocates memory and populates fields directly, bypassing constructors. Record deserialization mandates invoking the canonical constructor with stream values, guaranteeing invariant enforcement.
</details>

<details>
<summary>6. How do Record Patterns work in Java 21 (JEP 440)?</summary>
Record patterns allow matching and deconstructing a record's components directly inside `switch` expressions or `instanceof` checks (e.g., `case Point(int x, int y)`).
</details>

<details>
<summary>7. Can records declare instance fields or static fields?</summary>
Records cannot declare additional instance fields beyond those in the record header. However, they are permitted to declare `static` fields, methods, initializers, and constants.
</details>

<details>
<summary>8. Why are records unsuitable as JPA/Hibernate entities?</summary>
JPA requires a non-private no-arg constructor, mutable getters/setters for dirty-checking and proxy subclassing (lazy loading), none of which are compatible with records.
</details>

---

## 11. Practice Exercises

### Exercise 1 (Easy): Basic Record with Invariant Check
- **Task**: Declare a record `Email(String address)` with a compact constructor that validates the address contains an `'@'` character and is not null.
<details>
<summary>Solution</summary>

```java
public record Email(String address) {
    public Email {
        if (address == null || !address.contains("@")) {
            throw new IllegalArgumentException("Invalid email: " + address);
        }
    }
}
```
</details>

### Exercise 2 (Medium): Deeply Immutable Container
- **Task**: Implement a record `ShoppingCart(String customerId, List<String> items)` that guarantees deep immutability against outside modification.
<details>
<summary>Solution</summary>

```java
import java.util.List;

public record ShoppingCart(String customerId, List<String> items) {
    public ShoppingCart {
        if (customerId == null || customerId.isBlank()) {
            throw new IllegalArgumentException("Customer ID required");
        }
        items = (items == null) ? List.of() : List.copyOf(items);
    }
}
```
</details>

### Exercise 3 (Hard): Record Patterns in Nested Domain Hierarchies
- **Task**: Model `Address(String city, String country)` and `Person(String name, Address address)`. Write a method using Java 21 Record Patterns to extract and print the city of any person living in "Japan".
<details>
<summary>Solution</summary>

```java
public class NestedRecordPatternDemo {
    public record Address(String city, String country) {}
    public record Person(String name, Address address) {}

    public static void printJapaneseCities(Object obj) {
        if (obj instanceof Person(String name, Address(String city, String country)) && "Japan".equalsIgnoreCase(country)) {
            System.out.println(name + " lives in " + city + ", Japan");
        }
    }

    public static void main(String[] args) {
        Person p = new Person("Yuki", new Address("Kyoto", "Japan"));
        printJapaneseCities(p);
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+-----------------------------------------------------------------------------------------+
|                                    JAVA RECORD CHEAT SHEET                              |
+-----------------------------------------------------------------------------------------+
| Declaration              | public record Point(int x, int y) {}                         |
| Inheritance              | Extends java.lang.Record, implicitly final, can implement IF |
| Accessors                | point.x(), point.y() (no 'get' prefix)                       |
| Compact Constructor      | public Point { if (x < 0) throw ...; }                       |
| Custom Constructor       | public Point(int v) { this(v, v); } (Must delegate)          |
| Java 21 Record Pattern   | case Point(int x, int y) -> x + y;                           |
| Defensive Copy           | items = List.copyOf(items);                                  |
| Serialization Security   | Always invokes canonical constructor during deserialization  |
+-----------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [JEP 395: Records (Standardized in Java 16)](https://openjdk.org/jeps/395)
- [JEP 440: Record Patterns (Java 21)](https://openjdk.org/jeps/440)
- Java Language Specification — Section 8.10 (Record Classes)
- Inside Java Podcast: Episode on Records and Sealed Classes
