# Singleton Pattern in Modern Java

> Thread-safe single-instance lifecycles: Double-Checked Locking with `volatile`, Enum singletons (Effective Java Item 3), Bill Pugh Lazy Initialization, reflection/serialization defense, and testing implications.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core Design Pattern / Creational |
| **Prerequisites** | [04_oop](../../04-oop/README.md), [14_concurrency](../../14-concurrency/README.md), [15_jvm-internals](../../15-jvm-internals/README.md) |
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

### What is the Singleton Pattern?
The Singleton pattern is a creational design pattern that guarantees a class has **only one instance** throughout the lifetime of an application, providing a global point of access to that instance.

In enterprise software engineering, Singletons represent shared infrastructure resources:
- Database Connection Pools (HikariDataSource)
- Thread Pool Managers (Application ExecutorServices)
- Hardware Device Drivers or File System abstractions
- Application Configuration Registries

### Why Singleton Design Requires Extreme Care
Naive implementations of Singleton fail catastrophically in multi-threaded environments:
- **Race Conditions**: Two concurrent threads invoke `getInstance()` simultaneously, instantiating two separate instances and violating the invariant.
- **Instruction Reordering Hazards**: Without memory barriers (`volatile`), Thread 2 can observe an uninitialized object instance whose constructor has not completed.
- **Reflection & Serialization Attacks**: Reflection (`setAccessible(true)`) and Java native serialization bypass private constructors.
- **Tight Coupling**: Hardcoded `Singleton.getInstance()` calls introduce hidden global dependencies, defeating unit testing and dependency injection.

---

## 2. Core Concepts

### Five Ways to Implement Singleton in Java
1. **Eager Initialization**: Created at class loading time. Thread-safe by JVM classloader contract, but wastes memory if never used.
2. **Synchronized Lazy Initialization**: Thread-safe, but `synchronized` on every `getInstance()` call creates an unnecessary 10x bottleneck.
3. **Double-Checked Locking (DCL)**: Synchronizes only on initial creation; requires `volatile` to prevent instruction reordering.
4. **Bill Pugh Lazy Initialization (Initialization-on-demand Holder)**: Leverages JVM classloader mechanics; static inner class is loaded only when referenced. 100% thread-safe with zero synchronization overhead!
5. **Enum Singleton (Joshua Bloch Item 3)**: Gold standard. Intrinsically thread-safe, immune to reflection, and immune to serialization attacks.

---

## 3. How It Works Internally

### Instruction Reordering and the Double-Checked Locking Trap
Why does Double-Checked Locking require `volatile`?
When compiling `instance = new Singleton()`, the JVM bytecode performs three discrete operations:

```
1. Allocate memory block for Singleton on Heap
2. Execute Singleton constructor (<init>) to initialize fields
3. Assign memory address to reference variable `instance`
```

Without the `volatile` modifier, the CPU and JIT compiler are legally permitted to reorder steps 2 and 3:
```
1. Allocate memory block on Heap
2. Assign memory address to reference variable `instance` (INSTANCE IS NOT YET INITIALIZED!)
3. Execute Singleton constructor
```

```
+--------------------------------------------------------------------------+
|                  INSTRUCTION REORDERING HAZARD (NO VOLATILE)             |
|                                                                          |
| Thread 1: Steps 1 & 2 done (instance != null), Step 3 pending...        |
|                                                                          |
| Thread 2: if (instance == null) -> FALSE! (Memory address is non-null)   |
| Thread 2: returns instance;                                              |
| Thread 2: attempts to read instance.config -> CRASH / CORRUPT DATA!      |
+--------------------------------------------------------------------------+
```
Declaring `private static volatile Singleton instance;` introduces a JMM acquire/release barrier, preventing step 3 from reordering before step 2.

---

## 4. Syntax & Usage

### 1. Bill Pugh Holder Singleton (Recommended for Classes)
```java
public final class ConfigurationManager {
    // Private constructor prevents external instantiation
    private ConfigurationManager() {
        // Defense against reflection instantiation
        if (Holder.INSTANCE != null) {
            throw new IllegalStateException("Singleton already initialized!");
        }
    }

    // Static nested class loaded ONLY when getInstance() is called
    private static class Holder {
        private static final ConfigurationManager INSTANCE = new ConfigurationManager();
    }

    public static ConfigurationManager getInstance() {
        return Holder.INSTANCE;
    }
}
```

### 2. Enum Singleton (Recommended for Absolute Robustness)
```java
public enum GlobalRegistry {
    INSTANCE;

    private final java.util.Map<String, String> properties = new java.util.concurrent.ConcurrentHashMap<>();

    public void register(String key, String value) {
        properties.put(key, value);
    }

    public String get(String key) {
        return properties.get(key);
    }
}
```

---

## 5. Practical Examples

### Example 1: Production Double-Checked Locking Singleton
**Goal:** Implement a thread-safe lazy-loaded singleton with reflection defense and serialization defense.

```java
import java.io.Serial;
import java.io.Serializable;

public final class DatabaseConnectionPool implements Serializable {
    @Serial
    private static final long serialVersionUID = 1L;

    // volatile is MANDATORY to prevent instruction reordering
    private static volatile DatabaseConnectionPool instance;

    private DatabaseConnectionPool() {
        // Reflection attack defense
        if (instance != null) {
            throw new IllegalStateException("Singleton instance already exists; reflection forbidden.");
        }
        // Expensive resource initialization here...
    }

    public static DatabaseConnectionPool getInstance() {
        DatabaseConnectionPool localRef = instance; // Local variable optimization
        if (localRef == null) {
            synchronized (DatabaseConnectionPool.class) {
                localRef = instance;
                if (localRef == null) {
                    instance = localRef = new DatabaseConnectionPool();
                }
            }
        }
        return localRef;
    }

    // Serialization defense: returns the canonical singleton on readObject
    @Serial
    private Object readResolve() {
        return getInstance();
    }
}
```

### Example 2: Reflection Attack Demonstration & Defense
```java
import java.lang.reflect.Constructor;

public class ReflectionBreaker {
    public static void testReflection() throws Exception {
        ConfigurationManager s1 = ConfigurationManager.getInstance();

        Constructor<ConfigurationManager> constructor = 
            ConfigurationManager.class.getDeclaredConstructor();
        constructor.setAccessible(true);

        try {
            ConfigurationManager s2 = constructor.newInstance();
            System.out.println("VULNERABLE: Created second instance!");
        } catch (Exception ex) {
            System.out.println("SECURE: Reflection attack blocked by constructor guard!");
        }
    }
}
```

### Example 3: Edge Case — Multi-ClassLoader Duplication
In modular OSGi or multi-WAR servlet containers, if the Singleton class is loaded by two distinct `ClassLoader` instances, each classloader maintains its own static fields, resulting in **two separate Singleton instances** in the same JVM!
- *Fix*: Load the singleton class strictly via the parent bootstrap/system classloader or manage singletons via Dependency Injection frameworks (Spring Singleton Scope).

---

## 6. Best Practices

1. **Prefer Dependency Injection over Static `getInstance()`**: Inject singleton instances via constructor parameters. This makes client classes 100% unit-testable with mock objects.
2. **Use Enum for Absolute Safety**: If the class does not require inheritance, use an `enum` singleton.
3. **Always Use `volatile` in Double-Checked Locking**: Never omit `volatile` in DCL; it is required by the Java Memory Model.
4. **Implement `readResolve()` if Serializable**: Prevent native deserialization from spawning duplicate instances.
5. **Guard Against Reflection**: Throw `IllegalStateException` in the private constructor if the instance already exists.
6. **Local Variable Optimization in DCL**: Read `volatile` into a local variable (`DatabaseConnectionPool localRef = instance`) to avoid repeated volatile reads from main memory.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Secure Fix |
| :--- | :--- | :--- |
| **Omitting `volatile` in Double-Checked Locking** | JIT/CPU reorders constructor and reference assignment; exposes partially constructed objects. | Always mark `static volatile`. |
| **`public synchronized static getInstance()`** | Synchronizes every single read; causes massive thread contention under load. | Use Bill Pugh Holder or Double-Checked Locking. |
| **Allowing Deserialization without `readResolve()`** | Deserializing a singleton creates a brand new heap object, breaking uniqueness. | Implement `private Object readResolve() { return INSTANCE; }`. |
| **Public No-Arg Constructor** | Anyone can call `new MySingleton()`. | Declare constructor `private`. |
| **Mutable Shared State without Thread Safety** | Multiple threads calling singleton mutators cause race conditions. | Use concurrent collections or immutable state. |
| **Multiple Classloaders in Application Servers** | Each ClassLoader loads its own class copy with independent static instance. | Ensure singleton is loaded by common parent ClassLoader. |

---

## 8. Performance & Security Notes

### The Cost of Volatile Reads in Double-Checked Locking
Volatile reads require memory barriers on architectures with relaxed memory models (ARMv8, PowerPC).
By caching the volatile reference in a local stack variable:
```java
DatabaseConnectionPool localRef = instance;
if (localRef == null) { ... }
return localRef;
```
The volatile field is read only once instead of twice on the happy path, shaving ~2–5 nanoseconds per invocation on high-frequency paths.

---

## 9. Comparison / When to Use What

| Technique | Lazy Loading | Thread-Safe | Reflection Safe | Serialization Safe | Complexity |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Enum Singleton** | No (Loaded on access) | Yes (JVM contract)| Yes (JVM enforced) | Yes (JVM enforced) | Minimal |
| **Bill Pugh Holder** | Yes (True Lazy) | Yes (JVM ClassLoader)| Requires manual guard | Requires `readResolve()` | Very Low |
| **Double-Checked Locking** | Yes (True Lazy) | Yes (with `volatile`)| Requires manual guard | Requires `readResolve()` | Moderate |
| **Eager Static Constant** | No | Yes | Requires manual guard | Requires `readResolve()` | Minimal |

---

## 10. Interview Questions & Answers

### Q1: Why is Double-Checked Locking broken without the `volatile` keyword?
<details>
<summary>View Answer</summary>
Without `volatile`, the Java compiler and CPU can reorder instructions during object instantiation. The allocation of memory and the assignment of the reference pointer to the variable can happen *before* the constructor finishes executing. If Thread 1 is preempted immediately after reference assignment, Thread 2 checks `if (instance == null)`, observes a non-null address, and returns the instance. Thread 2 then accesses the object\'s uninitialized fields, resulting in data corruption or runtime crashes.
</details>

### Q2: How does the Bill Pugh Singleton pattern achieve thread safety without synchronization?
<details>
<summary>View Answer</summary>
The Bill Pugh pattern uses a private static nested class (Holder). In the Java Language Specification (JLS), a nested class is not loaded into memory when the outer class is loaded; it is loaded and initialized only when one of its members is explicitly referenced. When `getInstance()` is called, the JVM loads `Holder` and initializes its static field `INSTANCE`. Class initialization in the JVM is guaranteed by the classloader contract to be strictly sequential and thread-safe without explicit developer locks.
</details>

### Q3: Why does Joshua Bloch recommend Enum as the best way to implement a Singleton?
<details>
<summary>View Answer</summary>
Enums provide JVM-level guarantees that classes cannot match easily:
1. Thread Safety: The JVM guarantees enum constants are instantiated only once during class initialization.
2. Reflection Immune: `Constructor.newInstance()` explicitly checks for `Modifier.ENUM` and throws `IllegalArgumentException: Cannot reflectively create enum objects`.
3. Serialization Safe: Java native serialization guarantees that enum constants are never re-created; the JVM serializes only the constant name and resolves it via `Enum.valueOf()` on deserialization.
</details>

### Q4: How does a Singleton break unit testability, and how do you fix it?
<details>
<summary>View Answer</summary>
Singletons accessed via static methods (`MyService.getInstance()`) introduce tight coupling and global mutable state across tests. Tests cannot mock or stub the singleton, and state mutated by Test A leaks into Test B, causing flaky failures. The fix is to use Dependency Injection: define an interface (`MyService`), create a singleton implementation, and inject it into consuming classes via constructors. In tests, a mock implementation can be passed directly.
</details>

### Q5: What is the purpose of `readResolve()` in a serializable Singleton?
<details>
<summary>View Answer</summary>
When a serialized object is deserialized via `ObjectInputStream.readObject()`, Java allocates a new object instance on the heap bypassing normal constructors. If a singleton implements `Serializable`, deserializing it produces a second instance. Implementing `private Object readResolve()` tells the deserialization engine to discard the newly unpacked object and return the existing canonical singleton instance instead.
</details>

### Q6: What is the "Local Variable Optimization" in Double-Checked Locking?
<details>
<summary>View Answer</summary>
In DCL, copying `instance` into a local variable (`Singleton local = instance`) improves performance. Reading a `volatile` field requires a memory fence instruction. In cases where the instance is already initialized, using the local variable ensures that the volatile field is read from main memory only once rather than twice, speeding up execution by ~25% in microbenchmarks.
</details>

### Q7: Can a Singleton be garbage collected during application execution?
<details>
<summary>View Answer</summary>
In modern JVMs, a class and its static variables remain reachable as long as its `ClassLoader` is reachable. For classes loaded by the System/Application ClassLoader, the classloader is never collected, so the singleton is never garbage collected until JVM shutdown. However, in custom classloader environments (like OSGi bundles or Tomcat webapps), if the entire bundle/webapp is undeployed and its ClassLoader becomes unreachable, the singleton will be garbage collected.
</details>

### Q8: What is the difference between a Singleton and a Monostate pattern?
<details>
<summary>View Answer</summary>
In a Singleton, object *identity* is constrained: only one instance can ever be created. In the Monostate pattern, any number of instances can be created, but all instances share the exact same underlying state because all fields are declared `static`. Callers treat Monostate like a normal class, but all instances act as transparent windows into the shared static data.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a Reflection-Proof Bill Pugh Singleton
**Task:** Implement a configuration loader using the Bill Pugh Holder idiom with a constructor check that throws `IllegalStateException` if reflection attempts second instantiation.
<details>
<summary>View Solution</summary>

```java
public final class SecureConfigLoader {
    private SecureConfigLoader() {
        if (Holder.INSTANCE != null) {
            throw new IllegalStateException("Instance already initialized!");
        }
    }

    private static class Holder {
        private static final SecureConfigLoader INSTANCE = new SecureConfigLoader();
    }

    public static SecureConfigLoader getInstance() {
        return Holder.INSTANCE;
    }
}
```
</details>

### Exercise 2: Thread-Safe In-Memory Cache Singleton (Enum)
**Task:** Implement an in-memory cache using an Enum Singleton backed by `ConcurrentHashMap`.
<details>
<summary>View Solution</summary>

```java
import java.util.concurrent.ConcurrentHashMap;

public enum AppCache {
    INSTANCE;

    private final ConcurrentHashMap<String, Object> map = new ConcurrentHashMap<>();

    public void put(String key, Object val) { map.put(key, val); }
    public Object get(String key) { return map.get(key); }
    public void clear() { map.clear(); }
}
```
</details>

### Exercise 3: Double-Checked Locking Benchmark Verification
**Task:** Write a simple main method that spawns 20 threads requesting a DCL singleton simultaneously and asserts all 20 threads receive identical references (`==`).
<details>
<summary>View Solution</summary>

```java
import java.util.concurrent.*;

public class DclTest {
    public static void main(String[] args) throws Exception {
        ExecutorService pool = Executors.newFixedThreadPool(20);
        DatabaseConnectionPool first = DatabaseConnectionPool.getInstance();

        Callable<Boolean> task = () -> DatabaseConnectionPool.getInstance() == first;
        var futures = pool.invokeAll(java.util.Collections.nCopies(50, task));

        for (var f : futures) {
            assert f.get() : "Thread received divergent instance!";
        }
        pool.shutdown();
        System.out.println("DCL concurrency test PASSED: All references identical.");
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Best Overall**: Use `enum Singleton { INSTANCE; }` for guaranteed thread, reflection, and serialization safety.
- **Best for Lazy Class**: Use Bill Pugh Holder (`private static class Holder`).
- **Double-Checked Locking**: Always mark the static variable `volatile`!
- **Serialization**: Implement `private Object readResolve() { return INSTANCE; }`.
- **Testing**: Inject singletons via constructors; avoid hardcoded static `getInstance()` in business logic.
- **Local Optimization**: Use local variable in DCL to minimize volatile read fences.

---

## 13. Further Reading
- Effective Java: [Item 3: Enforce the singleton property with a private constructor or an enum type](https://www.oreilly.com/library/view/effective-java-3rd/9780134686097/)
- Java Language Specification: [Chapter 17: Threads and Locks](https://docs.oracle.com/javase/specs/jls/se21/html/jls-17.html)
- Bill Pugh: [The Double-Check Locking is Broken Declaration](https://www.cs.umd.edu/~pugh/java/memoryModel/DoubleCheckedLocking.html)
- GoF Design Patterns: [Singleton Pattern](https://www.oreilly.com/library/view/design-patterns-elements/0201633612/)
