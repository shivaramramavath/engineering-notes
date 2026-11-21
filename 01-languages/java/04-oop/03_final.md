# The Final Keyword: Immutability, Inlining, and Concurrency

> A deep dive into the `final` keyword in Java, covering final variables, blank finals, method inlining, immutable classes, and the Java Memory Model freeze action.

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [00_classes-and-objects.md](./00_classes-and-objects.md), [01_constructors.md](./01_constructors.md) |
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
9. [Comparison: final vs const vs Immutable](#9-comparison-final-vs-const-vs-immutable)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

In Java, the `final` keyword represents **non-reassignability** and **immutability constraints**. It can be applied in three distinct contexts:
1. **Final Variables:** The reference or primitive value cannot be changed once initialized (definite assignment).
2. **Final Methods:** The method cannot be overridden by any subclass.
3. **Final Classes:** The class cannot be extended/subclassed by any other class.

Beyond syntax enforcement, `final` plays a vital role in compiler optimizations (constant folding, method inlining) and concurrent thread safety under the **Java Memory Model (JMM)**.

---

## 2. Core Concepts

### 2.1 Final Variables vs Object Immutability
A critical distinction in Java:
- Declaring an object reference `final` means the **reference itself cannot be reassigned** to another object.
- It does **NOT** make the referenced object immutable!

```java
final List<String> names = new ArrayList<>();
// ILLEGAL: names = new ArrayList<>(); (Reassignment error)
// FULLY LEGAL:
names.add("Alice"); // Mutates internal state of the ArrayList!
```

### 2.2 Blank Finals
A `final` variable that is not initialized at its point of declaration is called a **blank final**.
- A blank instance final field must be assigned exactly once in every constructor.
- A blank static final field must be assigned exactly once in a `static { ... }` block.
- The Java compiler enforces **definite assignment**: failure to initialize or initializing more than once produces a compilation error.

---

## 3. How It Works Internally

### 3.1 Java Memory Model (JMM) Final Field Semantics
Under JSR-133 (Java Memory Model):
When an object is created, the values written to its `final` fields are protected by a **Freeze Action** at the end of the constructor:
- The JVM ensures a memory barrier (StoreStore barrier on architectures like ARM/PowerPC) before the object reference is published.
- Any thread that obtains a reference to the initialized object is guaranteed to observe the properly initialized values of its `final` fields **without needing locks or volatile reads**!
- If a field is non-final, a reader thread can observe the object reference before the field write is flushed to main memory, seeing default values (`0` or `null`).

```
Thread 1 (Constructor):
  1. Allocate object memory
  2. Write final field x = 42
  3. [Freeze Action / Memory Barrier]
  4. Publish reference: sharedRef = obj;

Thread 2 (Reader):
  1. Read sharedRef
  2. Read final field x -> GUARANTEED to see 42!
```

### 3.2 Method Inlining
Declaring a method `final` informs the compiler and JIT that the method implementation is terminal. The HotSpot C2 compiler can inline `final` methods directly into call sites without speculative deoptimization traps, reducing method dispatch call-stack overhead.

---

## 4. Syntax & Usage

### 4.1 Applying `final` Across Constructs
```java
// 1. Final Class: Cannot be subclassed
public final class ImmutableToken {
    // 2. Final Static Constant: Folded into bytecode constants
    public static final int MAX_TTL_SECONDS = 3600;

    // 3. Final Instance Fields (Blank Finals)
    private final String tokenId;
    private final long issuedAt;

    public ImmutableToken(String tokenId, long issuedAt) {
        this.tokenId = tokenId;
        this.issuedAt = issuedAt;
    }

    // 4. Final Method: Subclasses cannot override (redundant if class is final)
    public final String getTokenId() {
        return tokenId;
    }

    // 5. Final Method Parameters: Prevents reassignment inside method body
    public boolean isValid(final long currentTimestamp) {
        // currentTimestamp = 0; // COMPILE ERROR
        return (currentTimestamp - issuedAt) < MAX_TTL_SECONDS;
    }
}
```

---

## 5. Practical Examples

### Example 1: Compile-Time Constant Folding with Primitive Finals
```java
public class ConstantFoldingDemo {
    public static final int DAYS_IN_WEEK = 7;
    public static final int HOURS_PER_DAY = 24;
    // Evaluated at compile time by javac into 168!
    public static final int HOURS_PER_WEEK = DAYS_IN_WEEK * HOURS_PER_DAY;

    public static void main(String[] args) {
        System.out.println("Hours in week: " + HOURS_PER_WEEK);
    }
}
```
**Output:**
```text
Hours in week: 168
```

### Example 2: Real-World Truly Immutable Value Object
```java
import java.util.Collections;
import java.util.List;
import java.util.ArrayList;

public final class UserPermissions {
    private final String username;
    private final List<String> roles; // Defensively copied

    public UserPermissions(String username, List<String> roles) {
        if (username == null) throw new IllegalArgumentException("Username required");
        this.username = username;
        // Defensive copy ensures caller cannot mutate list externally
        this.roles = (roles != null) ? List.copyOf(roles) : List.of();
    }

    public String getUsername() { return username; }

    // Returning unmodifiable list preserves immutability
    public List<String> getRoles() { return roles; }

    public static void main(String[] args) {
        List<String> rawRoles = new ArrayList<>();
        rawRoles.add("USER");

        UserPermissions up = new UserPermissions("Alice", rawRoles);
        rawRoles.add("ADMIN"); // External modification has zero effect on up!

        System.out.println("User roles: " + up.getRoles());
    }
}
```
**Output:**
```text
User roles: [USER]
```

### Example 3: Edge Case – Effectively Final in Lambdas and Anonymous Classes
```java
public class EffectivelyFinalDemo {
    public static void main(String[] args) {
        int port = 8080; // Not explicitly declared final, but never reassigned -> Effectively Final

        Runnable r = () -> {
            // Can access 'port' because it is effectively final
            System.out.println("Server running on port: " + port);
        };

        r.run();

        // If we uncomment the line below, 'port' is no longer effectively final and lambda fails to compile!
        // port = 9000;
    }
}
```
**Output:**
```text
Server running on port: 8080
```

### Example 4: Failure Case – Reflection Bypassing Final Fields
```java
import java.lang.reflect.Field;

public class FinalReflectionTrap {
    static class SecretHolder {
        private final String secret = "INITIAL_SECRET";
        public String getSecret() { return secret; }
    }

    public static void main(String[] args) throws Exception {
        SecretHolder holder = new SecretHolder();
        System.out.println("Before: " + holder.getSecret());

        // In modern Java, setAccessible(true) on fields of strong modules is restricted!
        Field f = SecretHolder.class.getDeclaredField("secret");
        f.setAccessible(true);
        f.set(holder, "HACKED_SECRET");

        System.out.println("After:  " + holder.getSecret());
    }
}
```
**Output:**
```text
Before: INITIAL_SECRET
After:  HACKED_SECRET
```

### Example 5: Optimized / Idiomatic – Safe Publication Pattern
```java
public class SafePublicationManager {
    // Immutable wrapper holding multiple configuration attributes
    public static class ConfigSnapshot {
        public final String dbUrl;
        public final int maxPool;

        public ConfigSnapshot(String dbUrl, int maxPool) {
            this.dbUrl = dbUrl;
            this.maxPool = maxPool;
        }
    }

    // Volatile reference to immutable snapshot enables lock-free atomic updates!
    private volatile ConfigSnapshot currentConfig;

    public void updateConfig(String url, int pool) {
        this.currentConfig = new ConfigSnapshot(url, pool);
    }

    public ConfigSnapshot getConfig() {
        return currentConfig;
    }

    public static void main(String[] args) {
        SafePublicationManager mgr = new SafePublicationManager();
        mgr.updateConfig("jdbc:postgresql://db:5432/app", 50);
        System.out.println("Active DB: " + mgr.getConfig().dbUrl);
    }
}
```
**Output:**
```text
Active DB: jdbc:postgresql://db:5432/app
```

---

## 6. Best Practices

1. **Follow "Design for Inheritance or Prohibit It":** If a class is not specifically engineered and documented for extension, mark it `final` (Effective Java Item 19).
2. **Defensive Copies for Final References:** Remember that `final List<T>` only prevents pointer reassignment. Always wrap mutable collections with `List.copyOf()` or `Collections.unmodifiableList()`.
3. **Use Final Method Parameters:** Declaring method arguments `final` prevents accidental local parameter reassignment inside long methods.
4. **Make Value Objects Truly Immutable:** Combine `final class` + `final` private fields + defensive copying to achieve thread-safe value objects.
5. **Prefer Records Over Manual Final Classes:** In modern Java, use `record Point(int x, int y) {}` to automatically declare final immutable data carriers without boilerplate.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Is Wrong | Correct Idiom |
| :--- | :--- | :--- |
| Assuming `final` means immutable | A `final` reference to a `List` or array can still have its elements mutated. | Use unmodifiable collections (`List.copyOf`). |
| Uninitialized blank final | Compile error: `variable might not have been initialized`. | Ensure all constructor branches initialize every blank final. |
| Redundant `final` in `final` class | Marking methods `final` inside a class that is already `final` is redundant. | Omit `final` on methods of final classes. |
| Reassigning effectively final vars in lambdas | Lambdas capture variables by value; reassignment breaks effectively final status. | Use atomic holders (`AtomicInteger`) or avoid reassignment. |
| Modifying final fields via reflection | Violates JMM safe publication; in modern Java (Java 17+), strongly encapsulated modules block this. | Treat final fields as strictly immutable. |

---

## 8. Performance & Security Notes

### 8.1 Security Guarantees of Final Classes
Classes like `java.lang.String`, `java.lang.Integer`, and `java.net.Socket` are explicitly declared `final` for critical security reasons. If `String` were non-final, an attacker could create a malicious subclass `MaliciousString extends String` that overrides `equals()` or `hashCode()` to bypass access controls or authentication checks.

---

## 9. Comparison: final vs const vs Immutable

| Concept | Language Mechanism | Scope | Can State Mutate? |
| :--- | :--- | :--- | :--- |
| **`final` Variable** | Java keyword | Reassignment prevention | ✅ Yes (if referenced object is mutable) |
| **`const`** | C++ keyword / Java reserved keyword | Value or pointer immutability | Depends on pointer vs value const |
| **Immutable Object**| Design pattern / `record` | Entire object payload frozen | ❌ No |
| **`final` Method** | Java keyword | Prohibits overriding | N/A |
| **`final` Class** | Java keyword | Prohibits subclassing | N/A |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the difference between final, finally, and finalize?</strong></summary>

- `final`: Keyword applied to variables (prevents reassignment), methods (prevents overriding), and classes (prevents inheritance).
- `finally`: Block in try-catch-finally used to guarantee resource cleanup regardless of whether an exception occurs.
- `finalize`: Deprecated method on `java.lang.Object` historically called by GC before reclaiming memory; permanently deprecated in Java 9 and deprecated for removal in Java 18 (JEP 421).
</details>

<details>
<summary><strong>2. Does declaring a reference final make the object immutable?</strong></summary>

No. Declaring a reference `final` means that variable cannot be reassigned to point to another object on the heap. However, the internal state of the referenced object can still be modified through its methods (e.g. `final List<String> list; list.add("item");`).
</details>

<details>
<summary><strong>3. What is a "blank final" variable?</strong></summary>

A blank final is a `final` variable that is declared without an immediate initial value. A blank instance final field must be assigned exactly once in every constructor of the class. A blank static final field must be assigned exactly once in a `static { ... }` block.
</details>

<details>
<summary><strong>4. How does the Java Memory Model treat final fields during object construction?</strong></summary>

The JMM guarantees **safe publication** for `final` fields via a freeze action at constructor completion. Any thread that observes a reference to an object is guaranteed to see the properly initialized values of its `final` fields without requiring explicit synchronization, provided the `this` reference did not escape during construction.
</details>

<details>
<summary><strong>5. What is an "effectively final" variable?</strong></summary>

Introduced in Java 8, an effectively final variable is a local variable whose value is never changed after initialization, even though it lacks the explicit `final` keyword. Local variables accessed inside lambda expressions or anonymous inner classes must be `final` or effectively final.
</details>

<details>
<summary><strong>6. Why is String declared final in Java?</strong></summary>

1. **Security:** Strings are used for file paths, network sockets, class loading, and authentication tokens. If `String` could be subclassed, malicious subclasses could override methods to bypass security checks.
2. **String Pool Caching:** Immutability guarantees that pooled string literals can be safely shared across classes and threads without synchronization.
3. **Hash Code Caching:** `String` caches its hashcode; if it could be mutated, hash-based collections (`HashMap`) would be corrupted.
</details>

<details>
<summary><strong>7. Can you have an abstract final class or an abstract final method?</strong></summary>

No. `abstract` mandates that a class must be subclassed or a method must be overridden to be instantiated or executed. `final` explicitly prohibits subclassing or overriding. The two keywords are mutually contradictory and cause a compilation error.
</details>

<details>
<summary><strong>8. Can constructor be declared final?</strong></summary>

No. Constructors are never inherited by subclasses and therefore cannot be overridden. Declaring a constructor `final` is a syntax error.
</details>

---

## 11. Practice Exercises

### Easy
1. **Definite Assignment Check:** Write a class with 2 blank final fields and ensure they are assigned across multiple overloaded constructors.
2. **Immutable Point:** Create a `final class Point` with `final int x` and `final int y`.
3. **Effectively Final Lambda:** Demonstrate accessing an effectively final variable in a Stream lambda.

### Medium
4. **Truly Immutable Team Entity:** Build an immutable `Team` class holding a `final String name` and a `final List<Member>` with deep defensive copying.
5. **JMM Safe Publication Test:** Write a multi-threaded test illustrating thread visibility with and without `final` fields.
6. **Final Method Inlining Benchmark:** Author a microbenchmark comparing virtual method dispatch vs final method dispatch.

### Hard
7. **Thread-Safe Copy-on-Write Registry:** Build a thread-safe registry utilizing a `volatile` reference to a `final` immutable map.
8. **Bytecode Verification of Freeze Action:** Inspect class bytecode and JIT assembly to observe where memory barrier instructions are inserted for final fields.
9. **Reflection Barrier:** Verify that attempting to modify a final field in a modern modular application throws an exception under standard JVM security flags.

<details>
<summary><strong>Exercise Solutions</strong></summary>

```java
// Solution to Exercise 2: Immutable Point
public final class Point {
    private final int x;
    private final int y;

    public Point(int x, int y) {
        this.x = x;
        this.y = y;
    }

    public int getX() { return x; }
    public int getY() { return y; }
}
```
</details>

---

## 12. Summary & Cheat Sheet

```
┌────────────────────────────────────────────────────────────────────────┐
│                          FINAL CHEAT SHEET                             │
├────────────────────────────────────────────────────────────────────────┤
│ Final Variable:     Cannot be reassigned (Definite assignment checked) │
│ Final Field JMM:    Freeze action ensures safe multi-threaded visibility│
│ Final Method:       Cannot be overridden (Enables JIT call inlining)   │
│ Final Class:        Cannot be extended (Subclassed)                    │
│ Blank Final:        Must be assigned in every constructor              │
│ Immutability Rule:  final reference != immutable object (use defensive │
│                     copies for collections!)                           │
│ Lambdas:            Captured variables must be final or effectively fin│
└────────────────────────────────────────────────────────────────────────┘
```

---

## 13. Further Reading
- Java Language Specification (JLS) Section 4.12.4: `final` Variables
- JSR-133: Java Memory Model and Thread Specification (Section on Final Field Semantics)
- *Effective Java (3rd Edition)*, Joshua Bloch, Item 17: "Minimize mutability".
