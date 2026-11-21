# ThreadLocal and Scoped Values

> Deep dive into Java thread-local storage: `ThreadLocal`, internal `ThreadLocalMap` structure, memory leaks in thread pools, `InheritableThreadLocal`, and modern lightweight `ScopedValue` (JEP 506 / JEP 446 in Java 21-25).

| Metadata | Value |
|---|---|
| **Level** | Advanced |
| **Prerequisites** | [00_threads-and-lifecycle.md](00_threads-and-lifecycle.md), [02_thread-safety.md](02_thread-safety.md) |
| **Time to Read** | 25 minutes |
| **Status** | Active |
| **Java Version** | Java 1.2+ (ThreadLocal), Java 21+ Preview / Java 25 Final (Scoped Values JEP 506) |

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

### The Implicit Context Passing Problem
In complex enterprise applications, data such as **User Security Principals, Transaction IDs, and Trace Contexts** must be accessible across multiple layers of software (controllers, service layers, repositories).
Passing these parameters explicitly through every method signature creates bloated, unmaintainable APIs:
```java
// Anti-pattern: Parameter drilling
public void processOrder(Order o, UserContext ctx, TraceContext trace, TxContext tx) ...
```

### The Classic Solution: `ThreadLocal`
Java 1.2 introduced `ThreadLocal<T>`. A `ThreadLocal` variable provides independent, thread-isolated copies of a value for each thread that accesses it. Each thread reads and writes only its own local copy, eliminating synchronization overhead.

### The Modern Dilemma and Solution: `ScopedValue`
While `ThreadLocal` was effective for platform threads, it suffers from critical architectural flaws:
- **Unbounded Mutability**: Any method can modify the thread-local state.
- **Memory Leaks in Thread Pools**: Values remain attached to pooled threads indefinitely unless manually removed.
- **Virtual Thread Incompatibility**: With millions of virtual threads, copying thread locals consumes massive memory.
**Scoped Values (JEP 506 / JEP 446)** introduce immutable, bounded-lifetime context variables designed specifically for Virtual Threads.

---

## 2. Core Concepts

### `ThreadLocal` Mechanics
- `ThreadLocal.withInitial(Supplier<T>)`: Factory to define default value.
- `get()`: Returns the calling thread's copy.
- `set(T val)`: Updates the calling thread's copy.
- `remove()`: Removes the calling thread's copy. **Mandatory** in pooled environments!

### Memory Leak Mechanics in Thread Pools
Every `Thread` object holds an internal field: `ThreadLocal.ThreadLocalMap threadLocals;`.
- The keys in `ThreadLocalMap` are `WeakReference<ThreadLocal<?>>`.
- The values are **strong references**.
If a thread pool worker thread survives for days, and a thread-local value is not explicitly cleaned up with `remove()`, the value object is **never garbage collected**, leaking class loaders and enterprise heap memory.

### `ScopedValue` (Java 21 to 25)
A `ScopedValue` allows safely and efficiently sharing immutable data across method calls and child threads:
- **Immutable**: Once bound, the value cannot be mutated.
- **Bounded Lifetime**: Bound only for the duration of a specific `run()` or `call()` invocation.
- **Automatic Cleanup**: Memory is released immediately when the scope block exits, with zero risk of thread pool memory leaks.

---

## 3. How It Works Internally

### `ThreadLocalMap` Internal Memory Layout
```
+-------------------------------------------------------------+
|                        Thread Instance                      |
|  threadLocals: ThreadLocalMap                               |
|  +-------------------------------------------------------+  |
|  | Entry[] table                                         |  |
|  |  +-----------------------------+-------------------+  |  |
|  |  | Key (WeakReference)         | Value (StrongRef) |  |  |
|  |  | ThreadLocal instance        | LargeSessionData  |  |  |
|  |  +-----------------------------+-------------------+  |  |
|  +-------------------------------------------------------+  |
+-------------------------------------------------------------+
```
When the `ThreadLocal` instance is dereferenced by user code, the weak reference key is cleared during GC. However, the `Entry.value` remains strongly reachable from the `Thread` instance, leaking memory until `remove()` or table rehash occurs.

### Scoped Values Stack Frame Inheritance
Unlike `ThreadLocalMap`, `ScopedValue` bindings are stored directly in a compact linked list of scope bindings associated with the thread's execution frame. Sub-tasks in Structured Concurrency inherit scoped values with **zero copying overhead**, sharing identical immutable pointers.

---

## 4. Syntax & Usage

### Standard Idiom: Mandatory `try-finally` with `remove()`
```java
public class UserContextHolder {
    private static final ThreadLocal<String> currentUser = new ThreadLocal<>();

    public static void executeWithUser(String userId, Runnable action) {
        currentUser.set(userId);
        try {
            action.run();
        } finally {
            // MANDATORY: Always clean up to prevent thread pool memory leaks!
            currentUser.remove();
        }
    }

    public static String getCurrentUser() {
        return currentUser.get();
    }
}
```

### Modern `ScopedValue` Idiom (Java 21-25)
```java
import java.lang.ScopedValue;

public class ScopedContextDemo {
    public static final ScopedValue<String> USER_ID = ScopedValue.newInstance();

    public static void main(String[] args) {
        // Bind value for the execution of a runnable block
        ScopedValue.where(USER_ID, "ada_lovelace").run(() -> {
            System.out.println("Processing inside scope: " + USER_ID.get());
            helperMethod();
        });
        // Outside the block, USER_ID is automatically unbound!
    }

    private static void helperMethod() {
        System.out.println("Helper method reads user: " + USER_ID.get());
    }
}
```

---

## 5. Practical Examples

### Example 1: Transaction Context Propagation in Thread Pools
```java
// Goal: Propagate transaction ID across service layers safely
import java.util.UUID;
import java.util.concurrent.*;

public class TransactionContextManager {
    private static final ThreadLocal<String> txContext = ThreadLocal.withInitial(() -> UUID.randomUUID().toString());

    public static void processBatch() {
        try (ExecutorService pool = Executors.newFixedThreadPool(2)) {
            for (int i = 0; i < 4; i++) {
                pool.submit(() -> {
                    try {
                        txContext.set("TX-" + System.nanoTime());
                        System.out.println("Thread [" + Thread.currentThread().getName() + "] running with " + txContext.get());
                    } finally {
                        txContext.remove(); // Clean up!
                    }
                });
            }
        }
    }

    public static void main(String[] args) {
        processBatch();
    }
}
```

### Example 2: Thread-Safe Date Formatter Cache
```java
// Goal: Share expensive, non-thread-safe SimpleDateFormat instances safely per thread
import java.text.SimpleDateFormat;
import java.util.Date;

public class ThreadSafeFormatter {
    // Each thread gets its own dedicated instance; no synchronization required!
    private static final ThreadLocal<SimpleDateFormat> formatter = ThreadLocal.withInitial(
        () -> new SimpleDateFormat("yyyy-MM-dd HH:mm:ss")
    );

    public static String formatDate(Date date) {
        return formatter.get().format(date);
    }
}
```

### Example 3: Edge Case: Context Loss in Asynchronous Execution
```java
// Goal: Demonstrate why ThreadLocal is lost when submitting tasks to other threads
import java.util.concurrent.*;

public class ContextLossHazard {
    private static final ThreadLocal<String> user = ThreadLocal.withInitial(() -> "GUEST");

    public static void main(String[] args) throws Exception {
        user.set("ADMIN_USER");

        try (ExecutorService executor = Executors.newSingleThreadExecutor()) {
            executor.submit(() -> {
                // RUNS ON WORKER THREAD: Does NOT see main thread's ThreadLocal!
                System.out.println("Worker thread sees: " + user.get()); // Prints "GUEST", NOT "ADMIN_USER"!
            }).get();
        }
    }
}
```
*Explanation:* `ThreadLocal` is bound strictly to the thread that set it. Asynchronous handoffs require explicit context wrappers or Structured Concurrency with `ScopedValue`.

---

## 6. Best Practices
1. **Always Call `remove()` in a `finally` Block**: Protect against thread pool memory leaks by strictly enforcing `try { ... } finally { tl.remove(); }`.
2. **Declare `ThreadLocal` as `private static final`**: Prevent redundant instantiation of `ThreadLocal` coordinator handles.
3. **Migrate to `ScopedValue` for Virtual Threads**: In Java 21+, adopt `ScopedValue` for immutable context propagation to avoid memory blowup across millions of virtual threads.
4. **Avoid `InheritableThreadLocal` in Thread Pools**: Inheritable thread locals copy data on thread creation, not task assignment; pooled worker threads retain stale parent state.
5. **Keep ThreadLocal State Minimal**: Never store large data structures (like entity graphs) in thread locals; store simple identifiers or tokens.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why Wrong | Fix |
|---|---|---|
| Forgetting `remove()` in thread pool environments | Worker threads retain strong references to values, causing catastrophic class loader memory leaks. | Enclose usage in `try { ... } finally { tl.remove(); }`. |
| Expecting `ThreadLocal` to propagate to executor tasks | Worker threads in an executor have distinct thread instances and do not inherit thread locals. | Pass context explicitly or use `ScopedValue`. |
| Using non-static `ThreadLocal` fields | Creates duplicate entries in the thread's map for every enclosing class instance. | Declare `private static final ThreadLocal<T> ...`. |
| Reusing `InheritableThreadLocal` with pooled threads | Values are inherited only at thread creation time, causing subsequent tasks to read stale data. | Avoid `InheritableThreadLocal` in thread pools. |
| Storing mutable objects and mutating them across methods | Obscures data mutations and introduces hard-to-trace bugs. | Store immutable records in thread locals. |

---

## 8. Performance & Security Notes

### Memory Overhead in Virtual Threads
Platform thread pools rarely exceed a few thousand threads, making `ThreadLocalMap` memory footprint negligible. However, in applications spawning 1,000,000 virtual threads, if each thread initializes a thread-local map with several entries, gigabytes of heap memory are wasted on hash map arrays. `ScopedValue` shares immutable bindings with zero memory copying.

### Security and Privilege Leaks
In web application servers (like Tomcat), if request thread `A` authenticates as an administrator and fails to call `remove()`, subsequent untrusted request `B` assigned to the same worker thread can read the administrator context, resulting in privilege escalation vulnerabilities.

---

## 9. Comparison / When to Use What

| Feature | `ThreadLocal` | `ScopedValue` (Java 21-25) |
|---|---|---|
| **Mutability** | Mutable (`set()` anytime) | **Immutable** |
| **Lifetime** | Unbounded (until `remove()`) | Bounded to scope execution block |
| **Memory Leak Risk** | High (in thread pools) | **Zero (automatically reclaimed)** |
| **Virtual Thread Fit**| Poor (Heavy memory per thread) | **Optimized for millions of threads**|
| **Child Thread Share**| Deep copy (expensive) | Zero-copy reference sharing |

---

## 10. Interview Questions

<details>
<summary>1. How does `ThreadLocal` work internally?</summary>
Each `Thread` instance contains a package-private field `threadLocals` of type `ThreadLocalMap`. When `tl.get()` is called, it accesses the current thread's map using the `ThreadLocal` instance as a weak key to retrieve the stored value.
</details>

<details>
<summary>2. Why does `ThreadLocal` cause memory leaks in thread pools?</summary>
The keys in `ThreadLocalMap` are weak references, but the values are strong references. Because thread pool workers remain alive indefinitely, un-removed values remain strongly reachable from the thread's map even after the `ThreadLocal` key is garbage collected.
</details>

<details>
<summary>3. Why is `remove()` strictly necessary?</summary>
To remove both the key and the strong reference to the value from the current thread's `ThreadLocalMap`, allowing the value object to be garbage collected immediately.
</details>

<details>
<summary>4. What are Scoped Values (JEP 506 / JEP 446)?</summary>
A modern alternative to `ThreadLocal` that provides immutable, bounded-lifetime context sharing across method calls and child threads, designed specifically to eliminate memory leaks and scale efficiently with Virtual Threads.
</details>

<details>
<summary>5. How does `InheritableThreadLocal` work?</summary>
When a child thread is created, the JVM copies all values from the parent thread's `inheritableThreadLocals` map into the child thread's map. It does not synchronize subsequent updates.
</details>

<details>
<summary>6. Why should `ThreadLocal` always be declared `static final`?</summary>
Declaring it `static final` ensures there is only one instance of the `ThreadLocal` coordinator per JVM class loader, preventing redundant map key allocations across class instances.
</details>

<details>
<summary>7. Can `ThreadLocal` be used across asynchronous boundaries (e.g. `CompletableFuture`)?</summary>
No. Asynchronous stages execute on arbitrary worker threads in an executor pool, losing the calling thread's `ThreadLocal` context unless explicitly captured and transferred.
</details>

<details>
<summary>8. How do Scoped Values interact with Structured Concurrency?</summary>
Sub-tasks spawned inside a `StructuredTaskScope` automatically inherit all active `ScopedValue` bindings from the enclosing scope without memory copying, guaranteeing consistent context propagation.
</details>

---

## 11. Practice Exercises

### Exercise 1 (Easy): Thread-Isolated Random Generator
- **Task**: Implement a utility that provides a thread-isolated `java.util.Random` instance using `ThreadLocal.withInitial()` to avoid lock contention.
<details>
<summary>Solution</summary>

```java
import java.util.Random;

public class ThreadSafeRandomHolder {
    private static final ThreadLocal<Random> randomHolder = ThreadLocal.withInitial(Random::new);

    public static int nextInt(int bound) {
        return randomHolder.get().nextInt(bound);
    }
}
```
</details>

### Exercise 2 (Medium): Request Context Scope Guard
- **Task**: Implement an `AutoCloseable` guard class `RequestContext(String tenantId)` that sets a thread-local tenant ID and automatically removes it when used in a `try-with-resources` block.
<details>
<summary>Solution</summary>

```java
public class RequestContext implements AutoCloseable {
    private static final ThreadLocal<String> currentTenant = new ThreadLocal<>();

    public RequestContext(String tenantId) {
        currentTenant.set(tenantId);
    }

    public static String getTenantId() {
        return currentTenant.get();
    }

    @Override
    public void close() {
        currentTenant.remove();
    }

    public static void main(String[] args) {
        try (RequestContext ctx = new RequestContext("TENANT_CORP_100")) {
            System.out.println("Active tenant: " + RequestContext.getTenantId());
        }
        System.out.println("Outside scope tenant: " + RequestContext.getTenantId()); // null
    }
}
```
</details>

### Exercise 3 (Hard): ScopedValue Context Propagation
- **Task**: Using `ScopedValue`, write a method that binds a security token `ScopedValue<String> TOKEN`, invokes a sub-method that reads the token, and verifies it is unavailable outside the scope.
<details>
<summary>Solution</summary>

```java
public class ScopedSecurityDemo {
    public static final ScopedValue<String> AUTH_TOKEN = ScopedValue.newInstance();

    public static void executeSecureAction(String token, Runnable task) {
        ScopedValue.where(AUTH_TOKEN, token).run(task);
    }

    public static void main(String[] args) {
        executeSecureAction("BEARER_SECRET_KEY_999", () -> {
            System.out.println("Inside secure scope: Token = " + AUTH_TOKEN.get());
        });

        System.out.println("Is bound outside scope: " + AUTH_TOKEN.isBound()); // false
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+-----------------------------------------------------------------------------------------+
|                               THREADLOCAL & SCOPED VALUES                               |
+-----------------------------------------------------------------------------------------+
| ThreadLocal Declaration  | private static final ThreadLocal<T> tl = ThreadLocal...      |
| Mandatory Cleanup        | try { tl.set(v); ... } finally { tl.remove(); }              |
| Memory Leak Cause        | ThreadLocalMap has weak keys but STRONG values               |
| Modern Replacement       | ScopedValue<T> (JEP 506 / JEP 446 in Java 21-25)             |
| ScopedValue Usage        | ScopedValue.where(KEY, val).run(() -> ...);                  |
| ScopedValue Advantages   | Immutable, bounded lifetime, zero leak risk, VT optimized    |
+-----------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [JEP 506 / JEP 446: Scoped Values](https://openjdk.org/jeps/506)
- Java Concurrency in Practice by Brian Goetz — Chapter 3 (Thread Confinement)
- "Inside Java: Scoped Values vs ThreadLocal" by Ron Pressler
