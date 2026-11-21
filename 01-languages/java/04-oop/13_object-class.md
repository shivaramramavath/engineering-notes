# The java.lang.Object Class: Root Hierarchy and Methods

> A comprehensive deep-dive into `java.lang.Object`, its 11 foundational methods, the demise of `finalize()`, and modern resource cleanup with `Cleaner` and `AutoCloseable`.

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [00_classes-and-objects.md](./00_classes-and-objects.md) |
| **Time to Read** | 25 minutes |
| **Status** | Active |
| **Java Version** | Java 8+ (`finalize` deprecated for removal in Java 18 JEP 421) |

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
9. [Comparison: The 11 Methods of Object](#9-comparison-the-11-methods-of-object)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

In Java, `java.lang.Object` sits at the absolute root of the class hierarchy. Every class in the Java language (including user-defined classes, library classes, and array types) directly or indirectly inherits from `Object`.

`Object` defines the fundamental behavioral contract that all Java objects share:
- Identity and equivalence (`equals()`, `hashCode()`)
- String representation (`toString()`)
- Runtime reflection type identification (`getClass()`)
- Memory copying (`clone()`)
- Low-level concurrency monitor synchronization (`wait()`, `notify()`, `notifyAll()`)
- Garbage collection hooks (`finalize()` — now obsolete)

---

## 2. Core Concepts

### 2.1 The 11 Methods of `java.lang.Object`
```java
public class Object {
    // 1. Equivalence & Hashing
    public boolean equals(Object obj) { return (this == obj); }
    public native int hashCode();

    // 2. String Representation
    public String toString() {
        return getClass().getName() + "@" + Integer.toHexString(hashCode());
    }

    // 3. Runtime Type Identification
    public final native Class<?> getClass();

    // 4. Protected Memory Cloning
    protected native Object clone() throws CloneNotSupportedException;

    // 5. Monitor Concurrency (Thread coordination)
    public final void wait() throws InterruptedException { ... }
    public final native void wait(long timeoutMillis) throws InterruptedException;
    public final void wait(long timeoutMillis, int nanos) throws InterruptedException { ... }
    public final native void notify();
    public final native void notifyAll();

    // 6. Deprecated for Removal Lifecycle Hook
    @Deprecated(since="9", forRemoval=true)
    protected void finalize() throws Throwable { }
}
```

---

## 3. How It Works Internally

### 3.1 The Demise of `finalize()` (JEP 421)
For over two decades, `finalize()` was Java's mechanism for releasing native resources (file descriptors, C pointers, sockets) before GC reclaimed object memory.

#### Why `finalize()` Was Fundamentally Broken:
1. **Unpredictable Execution:** The JVM provides zero guarantee on *when* (or even *if*) finalizers will execute. File descriptors would exhaust long before GC ran finalizers.
2. **Severe GC Pauses:** Objects with finalizers cannot be collected immediately. They are moved to a special `Finalizer` queue, surviving at least one full GC cycle and causing generational memory leaks.
3. **Resurrection Attacks:** A malicious finalizer could assign `this` to a static variable, resurrecting a dead object in a zombie state!
4. **Performance Penalty:** Allocating objects with `finalize()` was 500% slower due to JVM registration overhead.

**Status:** Deprecated in Java 9, and marked for permanent removal in **Java 18 (JEP 421)**. Modern Java exclusively uses `AutoCloseable` with try-with-resources and `java.lang.ref.Cleaner`.

---

## 4. Syntax & Usage

### 4.1 Modern Resource Cleanup with `Cleaner` (Java 9+)
```java
import java.lang.ref.Cleaner;

public class NativeResourceHolder implements AutoCloseable {
    private static final Cleaner CLEANER = Cleaner.create();

    // Cleaning action MUST NOT refer to the outer instance (prevents memory leak!)
    private static class State implements Runnable {
        private long nativePointer;
        State(long ptr) { this.nativePointer = ptr; }

        @Override
        public void run() {
            if (nativePointer != 0) {
                System.out.println("Freeing native memory at: " + nativePointer);
                nativePointer = 0;
            }
        }
    }

    private final State state;
    private final Cleaner.Cleanable cleanable;

    public NativeResourceHolder(long ptr) {
        this.state = new State(ptr);
        this.cleanable = CLEANER.register(this, state);
    }

    @Override
    public void close() {
        cleanable.clean(); // Executes cleanup immediately and cancels GC hook
    }
}
```

---

## 5. Practical Examples

### Example 1: Overriding `toString()` for Informative Diagnostics
```java
public class ServerNode {
    private final String nodeId;
    private final String ipAddress;
    private final int port;

    public ServerNode(String nodeId, String ipAddress, int port) {
        this.nodeId = nodeId;
        this.ipAddress = ipAddress;
        this.port = port;
    }

    // Default toString() returns "ServerNode@4f023edb" (Useless in logs!)
    // Overridden toString() provides clean diagnostics:
    @Override
    public String toString() {
        return "ServerNode[id=" + nodeId + ", address=" + ipAddress + ":" + port + "]";
    }

    public static void main(String[] args) {
        ServerNode node = new ServerNode("node-us-east-1", "10.0.1.25", 8080);
        System.out.println("Active node: " + node);
    }
}
```
**Output:**
```text
Active node: ServerNode[id=node-us-east-1, address=10.0.1.25:8080]
```

### Example 2: Inspecting Runtime Types with `getClass()`
```java
public class ClassInspectionDemo {
    public static void printTypeInfo(Object obj) {
        if (obj == null) return;
        Class<?> clazz = obj.getClass();
        System.out.println("Class Name:        " + clazz.getName());
        System.out.println("Simple Name:       " + clazz.getSimpleName());
        System.out.println("Is Array:          " + clazz.isArray());
        System.out.println("Superclass:        " + clazz.getSuperclass().getSimpleName());
    }

    public static void main(String[] args) {
        printTypeInfo("Hello Java 25");
        printTypeInfo(new int[]{1, 2, 3});
    }
}
```
**Output:**
```text
Class Name:        java.lang.String
Simple Name:       String
Is Array:          false
Superclass:        Object
Class Name:        [I
Simple Name:       int[]
Is Array:          true
Superclass:        Object
```

### Example 3: Edge Case – System Identity Hash Code vs Overridden HashCode
```java
public class IdentityHashCodeDemo {
    static class CustomKey {
        @Override
        public int hashCode() { return 42; } // Fixed override
    }

    public static void main(String[] args) {
        CustomKey key = new CustomKey();
        System.out.println("Overridden hashCode: " + key.hashCode()); // 42
        // System.identityHashCode() returns the original memory-based hash from Mark Word
        System.out.println("Identity hashCode:   " + System.identityHashCode(key));
    }
}
```
**Output:**
```text
Overridden hashCode: 42
Identity hashCode:   ... (unique 31-bit integer, e.g. 1933863327)
```

### Example 4: Failure Case – Calling `wait()` Without Acquiring Monitor Lock
```java
public class IllegalMonitorStateTrap {
    public static void main(String[] args) {
        Object lock = new Object();
        try {
            // FATAL ERROR: Calling wait() without synchronizing on lock!
            lock.wait();
        } catch (IllegalMonitorStateException | InterruptedException e) {
            System.out.println("Caught: " + e.getClass().getSimpleName());
        }
    }
}
```
**Output:**
```text
Caught: IllegalMonitorStateException
```
*Fix: Always synchronize: `synchronized(lock) { lock.wait(); }`.*

### Example 5: Optimized / Idiomatic – Deterministic Cleanup with Try-With-Resources
```java
public class ResourceAutoCloseDemo {
    static class DatabaseSession implements AutoCloseable {
        public void execute(String sql) { System.out.println("Executing: " + sql); }
        @Override
        public void close() { System.out.println("DatabaseSession closed cleanly!"); }
    }

    public static void main(String[] args) {
        try (DatabaseSession session = new DatabaseSession()) {
            session.execute("SELECT * FROM metrics");
        } // session.close() is guaranteed to run automatically here!
    }
}
```
**Output:**
```text
Executing: SELECT * FROM metrics
DatabaseSession closed cleanly!
```

---

## 6. Best Practices

1. **Always Override `toString()`:** Every instantiable class should provide a concise, informative `toString()` representation (Effective Java Item 12).
2. **Never Use `finalize()`:** `finalize()` is deprecated for removal. Use `AutoCloseable` with try-with-resources for immediate deterministic cleanup.
3. **Use `java.lang.ref.Cleaner` for Safety Nets Only:** If native resources must be freed when callers forget `close()`, use `Cleaner` as a non-leaking fallback.
4. **Always Pair `equals()` with `hashCode()`:** If you override `equals()`, you MUST override `hashCode()`.
5. **Prefer High-Level Concurrency Utilities Over `wait()`/`notify()`:** Use `java.util.concurrent` locks, latches, queues, and semaphores instead of low-level `Object.wait()`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Is Wrong | Correct Idiom |
| :--- | :--- | :--- |
| Relying on `finalize()` for cleanup | Unpredictable timing, GC stalls, and deprecated for removal. | Implement `AutoCloseable` and use try-with-resources. |
| Calling `wait()` outside `synchronized` | Throws `IllegalMonitorStateException` at runtime. | Synchronize on the target monitor first. |
| Calling `wait()` inside `if` rather than `while` | Susceptible to spurious wakeups. | Always loop: `while (!condition) lock.wait();`. |
| Default `toString()` in logs | Prints cryptic `ClassName@hex` which provides zero diagnostic value. | Override `toString()`. |
| Overriding `getClass()` | `getClass()` is declared `final`; cannot be overridden. | Use `getClass()` directly for exact type checks. |

---

## 8. Performance & Security Notes

### 8.1 Monitor Inflation
Every Java object has a 64-bit Mark Word header. When a thread synchronizes on an object using `synchronized(obj)` or invokes `obj.wait()`, the JVM upgrades ("inflates") the lightweight lock into an OS-level heavyweight monitor (C++ `ObjectMonitor`), allocating native operating system mutex structures. Keep synchronization scopes tight.

---

## 9. Comparison: The 11 Methods of Object

| Method | Access | Is Final? | Purpose |
| :--- | :--- | :---: | :--- |
| `equals(Object)` | `public` | ❌ No | Content equivalence testing |
| `hashCode()` | `public` | ❌ No | Hash code for hash tables |
| `toString()` | `public` | ❌ No | Human-readable string representation |
| `getClass()` | `public` | ✅ Yes | Returns runtime `Class<?>` metadata |
| `clone()` | `protected` | ❌ No | Bitwise shallow copy (Cloneable) |
| `wait()` | `public` | ✅ Yes | Causes current thread to wait on monitor |
| `wait(long)` | `public` | ✅ Yes | Timed wait on monitor |
| `wait(long, int)` | `public` | ✅ Yes | Nanosecond timed wait on monitor |
| `notify()` | `public` | ✅ Yes | Wakes single thread waiting on monitor |
| `notifyAll()` | `public` | ✅ Yes | Wakes all threads waiting on monitor |
| `finalize()` | `protected` | ❌ No | Obsolete GC hook (Deprecated for removal) |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why is finalize() deprecated for removal in modern Java?</strong></summary>

`finalize()` was deprecated in Java 9 and marked for permanent removal in Java 18 (JEP 421) because:
1. It offers no timing guarantees (resources like file descriptors leak).
2. It causes significant GC pauses (objects survive into older generations).
3. It creates security vulnerabilities via object resurrection.
4. It imposes a massive performance penalty on object allocation.
The modern standard is `AutoCloseable` with try-with-resources, backed by `java.lang.ref.Cleaner` as a safety net.
</details>

<details>
<summary><strong>2. Why must wait() and notify() be called from within a synchronized block?</strong></summary>

`wait()` releases the monitor lock and suspends the thread, while `notify()` wakes up a waiting thread. If called without holding the monitor lock, race conditions between testing a condition and going to sleep (the "Lost Wakeup Problem") would cause threads to sleep indefinitely. The JVM enforces this by throwing `IllegalMonitorStateException`.
</details>

<details>
<summary><strong>3. Why should wait() always be called inside a while loop rather than an if statement?</strong></summary>

Because of **spurious wakeups** (threads can wake up from `wait()` without being explicitly notified by the OS) and race conditions (another thread may alter the state before this thread reacquires the lock). A `while` loop guarantees that the condition is re-tested before execution proceeds:
`while (!condition) { lock.wait(); }`
</details>

<details>
<summary><strong>4. What is the difference between getClass() and instanceof?</strong></summary>

`getClass()` checks for **exact type equality** (`obj.getClass() == Dog.class`), ignoring subclasses. `instanceof` checks whether the object is an instance of the class **or any of its subclasses/subinterfaces** (subtyping polymorphism).
</details>

<details>
<summary><strong>5. What does the default implementation of toString() return?</strong></summary>

`getClass().getName() + "@" + Integer.toHexString(hashCode())`. It returns the fully qualified class name followed by `@` and the hexadecimal representation of the object's identity hash code.
</details>

<details>
<summary><strong>6. Can you override getClass()?</strong></summary>

No. `getClass()` is declared `final native` in `java.lang.Object` to guarantee that runtime type identification cannot be spoofed or modified by subclasses.
</details>

<details>
<summary><strong>7. What does System.identityHashCode(obj) do?</strong></summary>

It returns the original hash code calculated by the JVM based on object memory layout in the Mark Word, exactly as if `hashCode()` had not been overridden by the object's class.
</details>

<details>
<summary><strong>8. What is the difference between notify() and notifyAll()?</strong></summary>

`notify()` wakes up a single arbitrary thread that is waiting on the object's monitor. `notifyAll()` wakes up all threads waiting on the object's monitor. `notifyAll()` is generally preferred to prevent thread starvation or deadlocks when threads wait on different conditions.
</details>

---

## 11. Practice Exercises

### Easy
1. **Clean toString:** Implement a `Book` class with `title`, `author`, and `isbn`, overriding `toString()`.
2. **Class Inspector:** Write a function that takes any `Object` and prints whether it is an interface, record, or class.
3. **Identity Hash Test:** Demonstrate that `System.identityHashCode(s)` differs from `s.hashCode()`.

### Medium
4. **AutoCloseable File Channel:** Implement a custom wrapper around an output stream that implements `AutoCloseable` with flush and close logging.
5. **Producer-Consumer with Wait/Notify:** Implement a bounded buffer queue using `wait()` and `notifyAll()` inside a `while` loop.
6. **Cleaner Safety Net:** Implement a native resource wrapper using `java.lang.ref.Cleaner`.

### Hard
7. **Resurrection Attack Demonstration:** Write a legacy Java class demonstrating how `finalize()` can resurrect a discarded object, and explain why JEP 421 eliminates this.
8. **Lock Inflation Profiler:** Use JVM flags `-XX:+PrintSafepointStatistics` and JFR to observe monitor inflation when calling `wait()`.
9. **Zero-Allocation ToString Formatter:** Write a zero-allocation serializer that dumps object state into a `ByteBuffer` without calling `toString()`.

<details>
<summary><strong>Exercise Solutions</strong></summary>

```java
// Solution to Exercise 1: Clean toString
public class Book {
    private final String title;
    private final String author;
    private final String isbn;

    public Book(String title, String author, String isbn) {
        this.title = title;
        this.author = author;
        this.isbn = isbn;
    }

    @Override
    public String toString() {
        return "Book[title='" + title + "', author='" + author + "', isbn='" + isbn + "']";
    }
}
```
</details>

---

## 12. Summary & Cheat Sheet

```
┌────────────────────────────────────────────────────────────────────────┐
│                        OBJECT CLASS CHEAT SHEET                        │
├────────────────────────────────────────────────────────────────────────┤
│ Hierarchy Root:     Every Java class extends java.lang.Object          │
│ Total Methods:      11 methods (6 final, 5 overridable)                │
│ toString():         Always override for informative logging            │
│ getClass():         final native; returns exact runtime Class metadata │
│ finalize():         DEPRECATED FOR REMOVAL (JEP 421); NEVER USE        │
│ Cleanup Standard:   AutoCloseable + try-with-resources                 │
│ Safety Net:         java.lang.ref.Cleaner (Java 9+)                    │
│ Concurrency:        wait()/notify() require synchronized monitor locks │
│ Spurious Wakeup:    Always loop: while (!condition) lock.wait();       │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 13. Further Reading
- Java Language Specification (JLS) Section 4.3.2: The Class `Object`
- [JEP 421: Deprecate Finalization for Removal (Java 18)](https://openjdk.org/jeps/421)
- *Effective Java (3rd Edition)*, Joshua Bloch, Item 8: "Avoid finalizers and cleaners".
- *Effective Java (3rd Edition)*, Joshua Bloch, Item 12: "Always override toString".
