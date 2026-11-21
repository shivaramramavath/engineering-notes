# Immutability in Modern Java

> Engineering thread-safe, robust, and leak-proof architectures through deep immutability: Java Records, unmodifiable collections, defensive copying, and persistent data structures.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core Engineering / Software Architecture |
| **Prerequisites** | [04_oop](../04-oop/README.md), [12_records](../12-modern-java/00_records.md), [14_concurrency](../14-concurrency/README.md) |
| **Time to Read** | 35 minutes |
| **Status** | Verified Production Guide |
| **Java Baseline** | Java 17 LTS / Java 21 LTS / Java 25 LTS |

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
10. [Interview Questions & Answers](#10-interview-questions--answers)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### What is Immutability?
An immutable object is an object whose observable state cannot be modified after it is constructed. Once an instance is created, its fields and referenced internal graphs remain completely fixed throughout its entire lifecycle.

Joshua Bloch (Effective Java, Item 17) states:
> *"Classes should be immutable unless there\'s a very good reason to make them mutable. Immutability makes programs simpler, less error-prone, and much more secure."*

### Why Immutability is the Cornerstone of Modern Java
1. **Thread Safety by Default**: Immutable objects require no synchronization, locks, or volatile barriers. They can be shared freely across threads, virtual threads, and async pipelines without race conditions.
2. **Defensive Stability**: Eliminates spooky action-at-a-distance bugs where passing an object to an external method results in unexpected mutation of its internal state.
3. **Safe Hash Keys**: Mutable objects used as keys in `HashMap` or `HashSet` corrupt hash buckets if their state mutates after insertion. Immutable objects guarantee deterministic hash codes.
4. **Cacheability & Safe Sharing**: Immutable instances can be cached and reused indefinitely without defensive cloning.

---

## 2. Core Concepts

### Shallow vs. Deep Immutability
- **Shallow Immutability**: All fields of a class are marked `final`. However, if one of those final fields points to a mutable object (e.g. `final List<String> list` or `final Date date`), callers can still mutate the contents of that list (`list.add("item")`).
- **Deep Immutability**: The object\'s fields are final AND all transitively referenced objects are also completely immutable or protected via defensive copies. Deep immutability guarantees zero mutation anywhere in the object graph.

### The Five Rules of Immutable Classes
1. Don\'t provide mutator methods (no setters).
2. Ensure the class cannot be extended (`final class` or `record`).
3. Make all fields `final`.
4. Make all fields `private`.
5. Ensure exclusive access to any mutable components (perform defensive copies on both construction and access).

### Modern Java Records and Immutability
Java 14+ Records (`record User(String name, int age)`) provide shallow immutability automatically:
- All fields are implicitly `private final`.
- The record class is implicitly `final`.
- No setter methods are generated.
- *Caveat*: If a record holds a mutable `List`, deep immutability still requires a compact constructor with `List.copyOf()`.

---

## 3. How It Works Internally

### Java Memory Model (JMM) Final Field Freeze Semantics
In Java, multi-threaded memory visibility is governed by JSR-133:
- For normal (non-final) fields, when Thread 1 creates an object and Thread 2 reads it without synchronization, Thread 2 might observe partially initialized fields (observing `0` or `null` before the actual value).
- For **`final` fields**, the JMM guarantees **Final Field Freeze**:
  - The JVM compiler inserts a memory store barrier (e.g., `MemBarStoreStore` on x86/ARM) at the end of the constructor.
  - All writes to `final` fields are guaranteed to be frozen and visible to all other threads before the reference to the new object is published!
  - Therefore, fully immutable objects are **safely published across threads with zero synchronization**.

```
+--------------------------------------------------------------------------+
|                  FINAL FIELD FREEZE SEMANTICS (JMM)                      |
|                                                                          |
|  Constructor:                                                            |
|    this.name = name;                                                     |
|    this.id = id;                                                         |
|    [ CPU StoreStore Barrier: All final writes flushed to cache/RAM ]     |
|  End of Constructor                                                      |
|         |                                                                |
|         v                                                                |
|  Reference Published to other threads -> GUARANTEED FULLY VISIBLE        |
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### 1. Achieving Deep Immutability in Records
```java
import java.util.List;

public record UserProfile(
    String username,
    List<String> roles // Need to ensure roles list is truly unmodifiable
) {
    // Compact constructor for deep immutability
    public UserProfile {
        // List.copyOf() creates an unmodifiable copy and validates non-null
        roles = List.copyOf(roles);
    }
}
```

---

## 5. Practical Examples

### Example 1: Deeply Immutable Value Object with Defensive Copying
**Goal:** Create a robust `AuditRecord` that cannot be mutated even if callers modify passed collections.

```java
import java.time.Instant;
import java.util.*;

public final class AuditRecord {
    private final String actor;
    private final Instant timestamp;
    private final Map<String, String> metadata;

    public AuditRecord(String actor, Instant timestamp, Map<String, String> metadata) {
        this.actor = Objects.requireNonNull(actor, "actor required");
        this.timestamp = Objects.requireNonNull(timestamp, "timestamp required");
        // Defensive copy: creates a truly unmodifiable snapshot
        this.metadata = Map.copyOf(metadata);
    }

    public String actor() { return actor; }
    public Instant timestamp() { return timestamp; }
    
    // Returns unmodifiable view; callers cannot call .put() or .clear()
    public Map<String, String> metadata() {
        return metadata;
    }
}
```

### Example 2: Immutable Copy-on-Write Evolution Pattern (Wither Methods)
**Goal:** In functional and immutable systems, updating state means returning a new instance with the modified value.

```java
import java.util.Objects;

public record ServerConfig(String host, int port, int maxConnections) {
    public ServerConfig {
        Objects.requireNonNull(host, "host cannot be null");
        if (port < 1 || port > 65535) throw new IllegalArgumentException("Invalid port");
    }

    // "Wither" methods returning new immutable instances
    public ServerConfig withPort(int newPort) {
        return new ServerConfig(this.host, newPort, this.maxConnections);
    }

    public ServerConfig withMaxConnections(int newMax) {
        return new ServerConfig(this.host, this.port, newMax);
    }
}
```

### Example 3: Thread-Safe In-Memory Snapshot Cache
**Goal:** Share complex application state across 100 concurrent threads without any locking mechanisms.

```java
import java.util.List;
import java.util.concurrent.atomic.AtomicReference;

public class ConfigurationRegistry {
    public record RouteTable(List<String> activeRoutes) {
        public RouteTable {
            activeRoutes = List.copyOf(activeRoutes);
        }
    }

    // Atomic reference holding an immutable state snapshot
    private final AtomicReference<RouteTable> currentRoutes = 
        new AtomicReference<>(new RouteTable(List.of("/api/v1", "/api/v2")));

    public RouteTable getRoutes() {
        return currentRoutes.get(); // Zero locks, zero synchronization!
    }

    public void addRoute(String newRoute) {
        currentRoutes.updateAndGet(old -> {
            var updated = new java.util.ArrayList<>(old.activeRoutes());
            updated.add(newRoute);
            return new RouteTable(updated);
        });
    }
}
```

### Example 4: Failure Case — Leaking Mutable References in Records
```java
// VULNERABLE: Shallow Immutability Trap
public record BadEmployee(String name, Date hireDate, List<String> skills) {}

// Exploit:
Date date = new Date();
List<String> skills = new ArrayList<>();
BadEmployee emp = new BadEmployee("Alice", date, skills);

// Caller mutates internal state externally!
date.setTime(0L); // Modifies emp's hireDate!
skills.add("Hacking"); // Mutates emp's internal skills list!
```

---

## 6. Best Practices

1. **Default to Records for Data Carriers**: Use Java records for DTOs, value objects, domain events, and configuration.
2. **Always Use `List.copyOf()` or `Map.copyOf()`**: Wrap collection fields inside record compact constructors to achieve deep immutability.
3. **Use "Withers" for Evolution**: Add `withFieldName(...)` methods that return new record instances for state transitions.
4. **Never Use `java.util.Date`**: Always use immutable `java.time` classes (`Instant`, `LocalDate`, `ZonedDateTime`).
5. **Safely Publish via AtomicReference**: For state that must evolve in concurrent environments, hold immutable records in an `AtomicReference`.
6. **Eliminate No-Arg Constructors**: Immutable classes should validate and initialize all fields at construction time.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Secure Fix |
| :--- | :--- | :--- |
| **Using `Collections.unmodifiableList(list)` without copying** | It is only an unmodifiable *view*; if original caller mutates `list`, the view changes! | Use `List.copyOf(list)` which takes an independent snapshot. |
| **Using `java.util.Date` as a field** | `Date` is mutable via `setTime()`; leaks state outside class. | Use immutable `java.time.Instant`. |
| **Assuming Records are deeply immutable by default** | Records holding mutable collections or arrays can still be mutated internally. | Add defensive copies in compact constructor. |
| **Using mutable arrays (`int[]`, `byte[]`)** | Arrays in Java are always mutable even if marked `final`. | Clone arrays on read/write or use immutable lists. |
| **Returning internal collection directly** | Callers can mutate or clear the collection if not protected. | Return `Collections.unmodifiableList()` or `List.copyOf()`. |
| **Mutable objects as `HashMap` keys** | Mutating an object after insertion corrupts the hash bucket, making lookup impossible. | Ensure all Map keys are strictly immutable. |

---

## 8. Performance & Security Notes

### Garbage Collection and Copy Overhead
A historical myth claims creating new immutable objects causes excessive GC overhead compared to in-place mutation.
- Modern JVM generational garbage collectors (G1, ZGC) are optimized specifically for short-lived immutable objects in the Young generation (Eden allocation takes nanoseconds).
- Object creation is often faster than the locking overhead and synchronization barriers required to manage shared mutable state safely across CPU cores.

---

## 9. Comparison / When to Use What

| Technique | Level of Immutability | Null Allowed | Best For |
| :--- | :--- | :--- | :--- |
| **Java Record (Default)** | Shallow Immutability | Yes | Simple primitive data carriers |
| **Record + `List.copyOf()`** | Deep Immutability | No (Rejects nulls) | Domain value objects, events |
| **`Collections.unmodifiableList()`** | Unmodifiable View | Yes | Wrapping existing collections without memory copy |
| **`List.copyOf()`** | Deep Snapshot | No | True defensive copying of untrusted input |

---

## 10. Interview Questions & Answers

### Q1: What is the difference between Shallow Immutability and Deep Immutability?
<details>
<summary>View Answer</summary>
Shallow immutability means that the references held by an object cannot be changed (all fields are `final`), but the objects pointed to by those references may themselves be mutable. Deep immutability means that not only are all fields final, but every object in the transitive reference graph is also immutable or strictly encapsulated with defensive copies. A Java record holding a `java.util.ArrayList` has shallow immutability, but lacks deep immutability because elements can still be added or removed from the list.
</details>

### Q2: How does the Java Memory Model (JMM) guarantee safe publication of immutable objects?
<details>
<summary>View Answer</summary>
Under JSR-133, final fields have special freeze semantics. At the end of a constructor, the compiler and processor enforce a memory store barrier ensuring that all writes to `final` fields are committed to memory before a reference to the newly constructed object is exposed. Any thread that receives the reference is guaranteed to observe the properly initialized values of all final fields without requiring locks or `volatile` reads.
</details>

### Q3: Why is `List.copyOf(list)` superior to `Collections.unmodifiableList(list)` for defensive copying?
<details>
<summary>View Answer</summary>
`Collections.unmodifiableList()` creates a wrapper view around the original list. While callers cannot modify the list through the wrapper, if the original list is modified by the caller who created it, those mutations are immediately reflected in the wrapper. In contrast, `List.copyOf()` creates an independent, unmodifiable snapshot (and optimizes by returning the instance itself if it is already an unmodifiable list).
</details>

### Q4: Why are mutable objects dangerous as keys in a `HashMap`?
<details>
<summary>View Answer</summary>
When an entry is added to a `HashMap`, its storage bucket is determined by calculating `key.hashCode()`. If the key object is mutable and its fields are subsequently modified, its hash code changes. When `map.get(key)` is called later, the map searches the bucket corresponding to the *new* hash code, failing to find the entry even though the identical object instance is still stored inside the map.
</details>

### Q5: How do "wither" methods work in immutable architectures?
<details>
<summary>View Answer</summary>
"Wither" methods (e.g. `withAge(int newAge)`) are functional alternatives to mutating setter methods. Instead of modifying the current instance, a wither method constructs and returns a brand-new immutable instance with the specified field updated while preserving all other existing fields unchanged.
</details>

### Q6: Can arrays in Java ever be made truly immutable?
<details>
<summary>View Answer</summary>
No. In Java, array instances (`int[]`, `Object[]`) are always mutable; their elements can be reassigned at any time regardless of `final` modifiers. To achieve immutability with array-like data, you must either clone the array defensively on both construction and read access, or convert the array to an immutable collection like `List.of()` or `List.copyOf()`.
</details>

### Q7: Why does `List.copyOf()` throw `NullPointerException` on null elements?
<details>
<summary>View Answer</summary>
Modern Java immutable collection factories (`List.of()`, `Set.of()`, `Map.of()`, and `List.copyOf()`) were deliberately designed to reject `null` elements fail-fast. In modern software engineering, `null` inside collections is considered a major code smell and bug magnet. Disallowing nulls simplifies internal collection implementations, eliminates null checks during iteration, and prevents silent errors.
</details>

### Q8: How does immutability simplify concurrency with Java 21 Virtual Threads?
<details>
<summary>View Answer</summary>
Virtual threads make spawning millions of concurrent tasks lightweight. However, high concurrency amplifies race conditions and lock contention if tasks share mutable state. When state is modeled using immutable records, virtual threads can read, pass, and share data across millions of tasks with zero synchronization, zero locks, and zero risk of carrier thread pinning.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a Deeply Immutable Employee Record
**Task:** Create a record `Employee` with fields `id` (long), `name` (String), `joinedDate` (Instant), and `tags` (Set<String>) that enforces deep immutability and rejects nulls.
<details>
<summary>View Solution</summary>

```java
import java.time.Instant;
import java.util.Objects;
import java.util.Set;

public record Employee(long id, String name, Instant joinedDate, Set<String> tags) {
    public Employee {
        Objects.requireNonNull(name, "name required");
        Objects.requireNonNull(joinedDate, "joinedDate required");
        if (id <= 0) throw new IllegalArgumentException("id must be positive");
        tags = Set.copyOf(tags); // Deep immutability; rejects nulls
    }
}
```
</details>

### Exercise 2: Implement a Safe Array Wrapper
**Task:** Create an immutable wrapper for a `byte[]` cryptographic key that prevents callers from mutating the internal bytes.
<details>
<summary>View Solution</summary>

```java
import java.util.Arrays;
import java.util.Objects;

public final class ImmutableByteArray {
    private final byte[] data;

    public ImmutableByteArray(byte[] source) {
        Objects.requireNonNull(source, "source cannot be null");
        this.data = source.clone(); // Defensive copy on write
    }

    public byte[] getBytes() {
        return data.clone(); // Defensive copy on read
    }

    public int length() {
        return data.length;
    }
}
```
</details>

### Exercise 3: Functional Wither Evolution
**Task:** Add a `withBalance` method to an immutable `BankAccount` record.
<details>
<summary>View Solution</summary>

```java
import java.math.BigDecimal;
import java.util.Objects;

public record BankAccount(String accountNumber, BigDecimal balance) {
    public BankAccount {
        Objects.requireNonNull(accountNumber, "accountNumber required");
        Objects.requireNonNull(balance, "balance required");
    }

    public BankAccount withBalance(BigDecimal newBalance) {
        return new BankAccount(this.accountNumber, newBalance);
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Core Rule**: Once created, state never changes.
- **Thread Safety**: Immutable objects are thread-safe by design; no synchronization needed.
- **Deep vs. Shallow**: Final fields pointing to mutable collections are NOT deeply immutable.
- **Defensive Copying**: Always use `List.copyOf()` or `Map.copyOf()` in record constructors.
- **Safe Evolution**: Use "wither" methods (`withX(...)`) to return updated copies.
- **Safe Keys**: Only use immutable objects as `Map` keys or `Set` elements.

---

## 13. Further Reading
- Effective Java: [Item 17: Minimize Mutability](https://www.oreilly.com/library/view/effective-java-3rd/9780134686097/)
- JSR-133: [Java Memory Model and Thread Specification](https://www.cs.umd.edu/~pugh/java/memoryModel/)
- OpenJDK JEP 395: [Records](https://openjdk.org/jeps/395)
- Brian Goetz: [Towards Language Support for Immutability](https://cr.openjdk.org/~briangoetz/amber/datum.html)
