# Enums in Java: Typesafe Enumerations, Bitsets, and Singleton Architecture

> An authoritative guide to `java.lang.Enum`, bytecode internals, constant-specific behavior, high-performance `EnumSet` and `EnumMap`, and serialization-proof singletons.

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [00_classes-and-objects.md](./00_classes-and-objects.md), [07_polymorphism.md](./07_polymorphism.md) |
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
9. [Comparison: Enum vs String Constants vs Integer Constants](#9-comparison-enum-vs-string-constants-vs-integer-constants)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

Before Java 5, developers represented fixed sets of choices (days of the week, order statuses, currency codes) using the **Int Enum Pattern** (`public static final int STATUS_PENDING = 0;`). This pattern was notoriously fragile:
- Zero type safety (any arbitrary integer could be passed).
- No namespace isolation (collision of constants).
- Meaningless debug output (prints `0` instead of `"PENDING"`).
- Fragile compilation (reordering integer constants required recompiling all clients).

**Java Enums** (introduced in Java 5) are full-featured **classes** that export one instance for each enumeration constant. Java enums provide compile-time type safety, rich methods and state, custom behavior per constant, and native integration with high-speed collections (`EnumSet` and `EnumMap`).

---

## 2. Core Concepts

### 2.1 The Enum Class Contract
Every enum in Java automatically extends `java.lang.Enum<E>`:
```java
public abstract class Enum<E extends Enum<E>>
    implements Constable, Comparable<E>, Serializable {
    private final String name;
    private final int ordinal;
    // methods
}
```

Key guarantees:
- **`final`:** Enums cannot be subclassed by other classes.
- **Fixed Instances:** The set of enum instances is fixed at compile time; you cannot invoke `new EnumType()`.
- **`Comparable`:** Enums implement natural ordering according to declaration order (`ordinal`).
- **`Serializable`:** Enums enjoy special JVM serialization handling preventing duplicate instance creation.

---

## 3. How It Works Internally

### 3.1 Bytecode Decompilation of an Enum
When you write:
```java
public enum Priority { LOW, MEDIUM, HIGH }
```
The compiler generates bytecode equivalent to:
```java
public final class Priority extends java.lang.Enum<Priority> {
    public static final Priority LOW = new Priority("LOW", 0);
    public static final Priority MEDIUM = new Priority("MEDIUM", 1);
    public static final Priority HIGH = new Priority("HIGH", 2);

    private static final Priority[] $VALUES = new Priority[] { LOW, MEDIUM, HIGH };

    public static Priority[] values() {
        return (Priority[]) $VALUES.clone(); // Defensive copy!
    }

    public static Priority valueOf(String name) {
        return (Priority) java.lang.Enum.valueOf(Priority.class, name);
    }

    private Priority(String name, int ordinal) {
        super(name, ordinal);
    }
}
```

### 3.2 High-Performance Collections: `EnumSet` and `EnumMap`
- **`EnumSet`:** Internally represented as a single 64-bit integer bit vector (`RegularEnumSet` using `long elements` for up to 64 constants). Bitwise operations (`OR`, `AND`) execute in a single CPU clock cycle, outperforming `HashSet` by orders of magnitude with zero object allocation overhead.
- **`EnumMap`:** Internally represented as a flat, contiguous array indexed directly by `ordinal()`. Key lookups execute in $O(1)$ without computing hash codes or resolving bucket collisions.

---

## 4. Syntax & Usage

### 4.1 Custom Fields, Constructors, and Methods
```java
public enum HttpStatus {
    OK(200, "Success"),
    BAD_REQUEST(400, "Client Error"),
    NOT_FOUND(404, "Not Found"),
    INTERNAL_SERVER_ERROR(500, "Server Error");

    private final int code;
    private final String reason;

    // Constructors are always private (explicit or implicit)
    HttpStatus(int code, String reason) {
        this.code = code;
        this.reason = reason;
    }

    public int getCode() { return code; }
    public String getReason() { return reason; }
    public boolean isError() { return code >= 400; }
}
```

---

## 5. Practical Examples

### Example 1: Constant-Specific Class Bodies (Strategy Pattern)
```java
public enum Operation {
    PLUS("+") {
        @Override
        public double apply(double x, double y) { return x + y; }
    },
    MINUS("-") {
        @Override
        public double apply(double x, double y) { return x - y; }
    },
    TIMES("*") {
        @Override
        public double apply(double x, double y) { return x * y; }
    },
    DIVIDE("/") {
        @Override
        public double apply(double x, double y) {
            if (y == 0.0) throw new ArithmeticException("Division by zero");
            return x / y;
        }
    };

    private final String symbol;
    Operation(String symbol) { this.symbol = symbol; }

    public abstract double apply(double x, double y);
    public String getSymbol() { return symbol; }

    public static void main(String[] args) {
        double a = 10, b = 2;
        for (Operation op : Operation.values()) {
            System.out.printf("%.1f %s %.1f = %.1f%n", a, op.getSymbol(), b, op.apply(a, b));
        }
    }
}
```
**Output:**
```text
10.0 + 2.0 = 12.0
10.0 - 2.0 = 8.0
10.0 * 2.0 = 20.0
10.0 / 2.0 = 5.0
```

### Example 2: Ultra-Fast Bitset with `EnumSet`
```java
import java.util.EnumSet;

public class PermissionManager {
    public enum Permission { READ, WRITE, EXECUTE, DELETE, AUDIT }

    public static void main(String[] args) {
        // Highly optimized 64-bit bitmask operations
        EnumSet<Permission> userRoles = EnumSet.of(Permission.READ, Permission.WRITE);
        EnumSet<Permission> adminRoles = EnumSet.allOf(Permission.class);

        System.out.println("User has EXECUTE: " + userRoles.contains(Permission.EXECUTE)); // false
        System.out.println("Admin has EXECUTE: " + adminRoles.contains(Permission.EXECUTE)); // true

        // Bitwise union
        userRoles.add(Permission.EXECUTE);
        System.out.println("User updated permissions: " + userRoles);
    }
}
```
**Output:**
```text
User has EXECUTE: false
Admin has EXECUTE: true
User updated permissions: [READ, WRITE, EXECUTE]
```

### Example 3: Edge Case – Safe Lookup Map Instead of Repeated `values()`
```java
import java.util.HashMap;
import java.util.Map;
import java.util.Optional;

public enum CurrencyCode {
    USD(840), EUR(978), JPY(392), GBP(826);

    private final int numericCode;
    private static final Map<Integer, CurrencyCode> BY_CODE = new HashMap<>();

    static {
        // Cache reverse lookup map during class initialization
        for (CurrencyCode c : values()) {
            BY_CODE.put(c.numericCode, c);
        }
    }

    CurrencyCode(int numericCode) {
        this.numericCode = numericCode;
    }

    public static Optional<CurrencyCode> fromCode(int code) {
        return Optional.ofNullable(BY_CODE.get(code));
    }

    public static void main(String[] args) {
        System.out.println("Code 978: " + fromCode(978).get());
        System.out.println("Code 999: " + fromCode(999).isPresent());
    }
}
```
**Output:**
```text
Code 978: EUR
Code 999: false
```

### Example 4: Failure Case – The `ordinal()` Indexing Anti-Pattern
```java
public enum Day {
    MONDAY, TUESDAY, WEDNESDAY, THURSDAY, FRIDAY, SATURDAY, SUNDAY;

    // ANTI-PATTERN: Never use ordinal() to store database values or array indices!
    // If someone reorders or adds an enum constant, all historical data is corrupted!
    public int getDbId() {
        return this.ordinal(); // DANGEROUS!
    }

    public static void main(String[] args) {
        System.out.println("Monday ordinal: " + MONDAY.ordinal()); // 0
    }
}
```
**Output:**
```text
Monday ordinal: 0
```
*Fix: Always define an explicit, immutable instance field (`private final int id;`).*

### Example 5: Optimized / Idiomatic – The Single-Element Enum Singleton (Effective Java Item 3)
```java
public enum DatabaseConnectionPool {
    INSTANCE; // The one and only instance

    private final String connectionString;

    DatabaseConnectionPool() {
        // Initialized exactly once by JVM runtime classloader
        this.connectionString = "jdbc:mysql://cluster:3306/production";
        System.out.println("DatabaseConnectionPool initialized securely!");
    }

    public void execute(String sql) {
        System.out.println("Executing on " + connectionString + ": " + sql);
    }

    public static void main(String[] args) {
        // Completely proof against:
        // 1. Reflection attacks (Constructor.newInstance blocks enums!)
        // 2. Serialization attacks (JVM guarantees unique identity!)
        // 3. Multi-threading race conditions (Class initialization lock!)
        DatabaseConnectionPool.INSTANCE.execute("SELECT 1");
    }
}
```
**Output:**
```text
DatabaseConnectionPool initialized securely!
Executing on jdbc:mysql://cluster:3306/production: SELECT 1
```

---

## 6. Best Practices

1. **Use Single-Element Enum for Singletons:** It is universally recognized as the safest, cleanest way to implement singletons in Java (Effective Java Item 3).
2. **Never Derive State from `ordinal()`:** Never store `ordinal()` in databases or config files. Add an explicit `id` field.
3. **Use `EnumSet` Instead of Bit Fields:** Never write `public static final int FLAG_READ = 1 << 0;`. Use `EnumSet.of(Permission.READ)` for type safety and equal speed.
4. **Use `EnumMap` Instead of `HashMap`:** Whenever keys are known enums, use `new EnumMap<>(MyEnum.class)`. It uses a compact array without hashing overhead.
5. **Cache Reverse Lookups:** Pre-compute reverse maps (`Map<String, MyEnum>`) inside a static initializer rather than scanning `values()` in linear time on every query.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Is Wrong | Correct Idiom |
| :--- | :--- | :--- |
| `enum.ordinal()` for business logic | Reordering constants silently corrupts database IDs or array indices. | Store explicit `private final int code;`. |
| Repeatedly calling `values()` in hot loops | `values()` returns a **new array clone** on every invocation to prevent mutation of the internal array! | Cache `values()` once in a static final array. |
| Using `==` vs `.equals()` | Both work, but `==` provides compile-time type safety and prevents `NullPointerException`. | Prefer `a == MyEnum.VAL`. |
| Public enum constructor | Compile error; enum constructors can only be private or package-private. | Keep constructor private. |
| Calling `.name()` instead of `toString()` | `name()` is final and strict; override `toString()` if human-readable representation is needed. | Use `name()` for programmatic IDs, `toString()` for UI. |

---

## 8. Performance & Security Notes

### 8.1 Reflection Protection
Java's reflection API (`Constructor.newInstance()`) explicitly checks `(clazz.getModifiers() & Modifier.ENUM) != 0` and throws `IllegalArgumentException("Cannot reflectively create enum objects")`. This makes enum singletons immune to reflection-based attacks that break ordinary private constructors.

---

## 9. Comparison: Enum vs String Constants vs Integer Constants

| Feature | Java `enum` | `String` Constants | `int` Constants |
| :--- | :--- | :--- | :--- |
| **Type Safety** | ✅ Absolute (Compile-time) | ⚠️ Partial | ❌ None |
| **Performance** | $O(1)$ pointer comparison (`==`)| Slower string equality | $O(1)$ primitive comparison |
| **Namespace Isolation**| ✅ Scoped to Enum class | ⚠️ Risk of collision | ⚠️ High collision risk |
| **Behavior / Methods** | ✅ Full classes with methods | ❌ Data only | ❌ Data only |
| **Debug Readability** | ✅ Self-documenting name | ✅ Human readable | ❌ Prints raw numbers |
| **Memory / Bitsets** | `EnumSet` (Single `long`) | Bulky heap strings | Bitwise masks (`1 << n`) |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why is a single-element enum considered the best way to implement a singleton in Java?</strong></summary>

According to Joshua Bloch (Effective Java Item 3), a single-element enum is the best approach because:
1. It is concise and free of boilerplate.
2. It provides guaranteed thread-safe initialization backed by JVM class loading.
3. It provides built-in protection against reflection attacks (`Constructor.newInstance` rejects enum classes).
4. The JVM handles serialization natively, guaranteeing that deserialization never creates a duplicate instance.
</details>

<details>
<summary><strong>2. Why does Enum.values() return a clone of the internal array on every invocation?</strong></summary>

Because arrays in Java are mutable. If `values()` returned a direct reference to the internal `$VALUES` array, a caller could modify its elements (`values()[0] = null`), corrupting the JVM enum state for all other threads and classes. Returning a clone (`$VALUES.clone()`) guarantees immutability, but creates garbage if invoked repeatedly in tight loops.
</details>

<details>
<summary><strong>3. How does EnumSet achieve extreme performance compared to HashSet?</strong></summary>

`EnumSet` is implemented as a bit vector (`RegularEnumSet` using a single 64-bit `long elements` field for enums with up to 64 constants). Adding, removing, or checking for an element maps directly to bitwise operations (`1L << ordinal`), executing in a single CPU instruction without allocating heap nodes or computing hash codes.
</details>

<details>
<summary><strong>4. Can an enum extend another class? Can it implement interfaces?</strong></summary>

An enum **cannot extend any class** because it already implicitly extends `java.lang.Enum<E>` (and Java disallows multiple class inheritance). However, an enum **can implement any number of interfaces**, allowing enums to represent polymorphic strategies or commands.
</details>

<details>
<summary><strong>5. What is a constant-specific method in an enum?</strong></summary>

An enum can declare an `abstract` method, and each individual enum constant can define an anonymous class body implementing that method uniquely. This replaces fragile `switch` statements with clean polymorphism directly on the enum constants.
</details>

<details>
<summary><strong>6. What is the difference between enum.name() and enum.toString()?</strong></summary>

`name()` is a `final` method that returns the exact declared identifier of the enum constant as a string (e.g. `"NEW_YORK"`). `toString()` by default returns `name()`, but unlike `name()`, it can be overridden by the developer to return user-friendly, localized, or formatted representations.
</details>

<details>
<summary><strong>7. Why should you use == instead of .equals() when comparing enums?</strong></summary>

While both yield identical results, `==` is preferred because:
1. It provides compile-time type safety (comparing two different enum types with `==` causes a compile error, whereas `.equals()` compiles and fails silently at runtime).
2. It is completely immune to `NullPointerException` if the left-hand variable is null (`val == MyEnum.A` is safe; `val.equals(MyEnum.A)` throws NPE if `val` is null).
3. It performs a fast $O(1)$ pointer comparison.
</details>

<details>
<summary><strong>8. Can you clone an enum?</strong></summary>

No. The `clone()` method in `java.lang.Enum` is declared `final` and unconditionally throws `CloneNotSupportedException` to guarantee that duplicate enum instances can never be created.
</details>

---

## 11. Practice Exercises

### Easy
1. **Day of Week Classifier:** Create an enum `Day` with a method `boolean isWeekend()`.
2. **Traffic Light State:** Implement a `TrafficLight` enum (`RED`, `YELLOW`, `GREEN`) with a method `TrafficLight next()`.
3. **Safe Enum Equality:** Demonstrate comparing enums with `==` and handling `null` safely.

### Medium
4. **Calculated Tax by State:** Create an enum `StateTax` with tax calculation methods and reverse lookup by state abbreviation.
5. **EnumSet Role Engine:** Build a role-based authorization filter using `EnumSet` and bitwise permissions.
6. **EnumMap Dispatcher:** Implement an event router using `EnumMap<EventType, Consumer<Event>>`.

### Hard
7. **Extensible Enum Pattern via Interfaces:** Implement an extensible operation system using an interface and multiple enums (Effective Java Item 38).
8. **Bytecode Inspection of Enum Class:** Use `javap -c -p` to verify the generated `$VALUES` array, private constructor, and static initializer.
9. **Zero-GC High-Throughput Enum Router:** Build a router processing 10,000,000 events/sec caching `values()` to achieve zero allocations.

<details>
<summary><strong>Exercise Solutions</strong></summary>

```java
// Solution to Exercise 1: Day of Week Classifier
public enum Day {
    MONDAY, TUESDAY, WEDNESDAY, THURSDAY, FRIDAY, SATURDAY, SUNDAY;

    public boolean isWeekend() {
        return this == SATURDAY || this == SUNDAY;
    }
}
```
</details>

---

## 12. Summary & Cheat Sheet

```
┌────────────────────────────────────────────────────────────────────────┐
│                          ENUMS CHEAT SHEET                             │
├────────────────────────────────────────────────────────────────────────┤
│ Definition:         public enum Name { CONST_A, CONST_B }              │
│ Hierarchy:          Implicitly extends java.lang.Enum (Cannot extend)  │
│ Interfaces:         Can implement any number of interfaces             │
│ Comparison:         Always use `==` (Type-safe & null-safe)            │
│ Singleton:          Single-element enum is the gold standard singleton │
│ Collections:        Use EnumSet (Bitmask speed) and EnumMap (Array)    │
│ values() Caution:   Clones array on every call; cache in static array  │
│ Anti-pattern:       Never use ordinal() for persistence (Add ID field) │
│ Polymorphism:       Constant-specific class bodies via abstract methods│
└────────────────────────────────────────────────────────────────────────┘
```

---

## 13. Further Reading
- Java Language Specification (JLS) Section 8.9: Enums
- *Effective Java (3rd Edition)*, Joshua Bloch:
  - Item 3: "Enforce the singleton property with a private constructor or an enum type".
  - Item 34: "Use enums instead of int constants".
  - Item 35: "Use instance fields instead of ordinals".
  - Item 36: "Use EnumSet instead of bit fields".
  - Item 37: "Use EnumMap instead of ordinal indexing".
