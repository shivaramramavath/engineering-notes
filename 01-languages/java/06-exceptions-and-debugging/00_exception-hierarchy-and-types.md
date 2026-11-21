# Java Exception Hierarchy: Throwable, Errors, and Exceptions

> An authoritative guide to `java.lang.Throwable`, the fundamental distinction between Checked and Unchecked exceptions, JVM fatal errors, and stack frame generation mechanics.

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [00_classes-and-objects.md](../04-oop/00_classes-and-objects.md), [06_inheritance.md](../04-oop/06_inheritance.md) |
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
9. [Comparison: Checked vs Unchecked vs Error](#9-comparison-checked-vs-unchecked-vs-error)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

In software development, exceptional conditions—file not found, network timeouts, invalid user inputs, memory exhaustion—are inevitable. In C, error handling relied on integer error codes (`return -1;`), which developers frequently neglected to check, causing silent data corruption and cascading failures.

Java revolutionized error handling by introducing **Structured Exception Handling** anchored by the `java.lang.Throwable` class hierarchy:
- Separates normal business logic from error recovery code.
- Propagates errors automatically up the call stack until an appropriate handler is found.
- Captures runtime execution snapshots (stack traces) pinpointing the exact source file and line number of the failure.

---

## 2. Core Concepts

### 2.1 The Throwable Hierarchy
Every exception and error in Java is a direct or indirect subclass of `java.lang.Throwable`:

```
                             ┌──────────────────────────┐
                             │   java.lang.Throwable    │
                             └────────────┬─────────────┘
                                          │
            ┌─────────────────────────────┴─────────────────────────────┐
            ▼                                                           ▼
┌──────────────────────────┐                               ┌──────────────────────────┐
│     java.lang.Error      │                               │   java.lang.Exception    │
├──────────────────────────┤                               ├──────────────────────────┤
│ Fatal JVM conditions     │                               │ Recoverable conditions   │
│ - OutOfMemoryError       │                               │ - IOException (Checked)  │
│ - StackOverflowError     │                               │ - SQLException (Checked) │
│ - InternalError          │                               └────────────┬─────────────┘
└──────────────────────────┘                                            │
                                                                        ▼
                                                           ┌──────────────────────────┐
                                                           │  java.lang.RuntimeException
                                                           ├──────────────────────────┤
                                                           │ Unchecked bugs & flaws   │
                                                           │ - NullPointerException   │
                                                           │ - IllegalArgumentException
                                                           │ - IndexOutOfBoundsException
                                                           └──────────────────────────┘
```

1. **`java.lang.Error` (Unchecked):** Represents catastrophic conditions originating from JVM hardware or resource exhaustion (e.g. `OutOfMemoryError`, `StackOverflowError`). Applications should **never** attempt to catch or recover from `Error`.
2. **Checked Exceptions (`Exception` excluding `RuntimeException`):** Conditions outside the direct control of the program (I/O, database networks, missing files) that a well-written application should anticipate and recover from. The compiler mandates that callers either catch them (`try-catch`) or declare them (`throws`).
3. **Unchecked Exceptions (`RuntimeException` and its subclasses):** Programming errors, API misuse, or contract violations (e.g. passing null, indexing past array bounds). The compiler does not require catching or declaring them.

---

## 3. How It Works Internally

### 3.1 Stack Frame Capture: `fillInStackTrace()`
When an exception object is constructed (`new Exception()`):
- The `Throwable` constructor immediately invokes the native method `fillInStackTrace()`.
- The JVM pauses the current thread and walks the execution call stack in native C++, recording the method metadata, class name, source filename, and bytecode index (BCI) for every active stack frame.
- **Performance Cost:** Over **90% of the CPU cost** of throwing an exception is consumed by `fillInStackTrace()`. In microsecond-sensitive systems, throwing exceptions in hot loops degrades throughput significantly.

### 3.2 Suppressed Exceptions (Java 7+)
When an exception occurs inside a `try` block, and a secondary exception occurs during resource cleanup in `close()`, Java does not discard either exception. The primary exception is thrown, and the secondary exception is attached as a **Suppressed Exception** via `addSuppressed(Throwable)`.

---

## 4. Syntax & Usage

### 4.1 Inspecting Exception Metadata
```java
try {
    Files.readString(Path.of("/non/existent/file.txt"));
} catch (IOException ex) {
    // 1. Descriptive message
    String msg = ex.getMessage();

    // 2. Underlying root cause
    Throwable cause = ex.getCause();

    // 3. Suppressed exceptions (e.g. from try-with-resources)
    Throwable[] suppressed = ex.getSuppressed();

    // 4. Print stack trace to standard error stream
    ex.printStackTrace();
}
```

---

## 5. Practical Examples

### Example 1: Differentiating Checked vs Unchecked Failures
```java
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;

public class ExceptionTypesDemo {
    // Checked Exception: Method MUST declare throws IOException
    public static String readConfig(String pathStr) throws IOException {
        // Unchecked Exception: Programming error if path is null
        if (pathStr == null) {
            throw new IllegalArgumentException("Path cannot be null!");
        }
        return Files.readString(Path.of(pathStr));
    }

    public static void main(String[] args) {
        // Must handle or propagate checked exception
        try {
            readConfig("missing.json");
        } catch (IOException e) {
            System.out.println("Handled anticipated I/O failure: " + e.getMessage());
        }

        // Unchecked exception compiles without try-catch, but fails at runtime:
        try {
            readConfig(null);
        } catch (IllegalArgumentException e) {
            System.out.println("Caught contract bug: " + e.getMessage());
        }
    }
}
```
**Output:**
```text
Handled anticipated I/O failure: missing.json
Caught contract bug: Path cannot be null!
```

### Example 2: Real-World Suppressed Exceptions in Custom Resources
```java
public class SuppressedExceptionDemo {
    static class BrokenResource implements AutoCloseable {
        public void work() {
            throw new IllegalStateException("Primary business failure during processing!");
        }

        @Override
        public void close() {
            throw new RuntimeException("Secondary cleanup failure during close()!");
        }
    }

    public static void main(String[] args) {
        try (BrokenResource res = new BrokenResource()) {
            res.work();
        } catch (Exception ex) {
            System.out.println("Primary Exception:    " + ex.getMessage());
            for (Throwable s : ex.getSuppressed()) {
                System.out.println("Suppressed Exception: " + s.getMessage());
            }
        }
    }
}
```
**Output:**
```text
Primary Exception:    Primary business failure during processing!
Suppressed Exception: Secondary cleanup failure during close()!
```

### Example 3: Edge Case – High-Throughput Lightweight Exception (Disabling Stack Trace)
```java
// For ultra-low-latency control flow where stack traces are unnecessary:
public class FastValidationException extends RuntimeException {
    public FastValidationException(String msg) {
        super(msg, null, false, false); // writableStackTrace = false!
    }

    @Override
    public synchronized Throwable fillInStackTrace() {
        return this; // No-op: Zero native stack walk overhead!
    }

    public static void main(String[] args) {
        long start = System.nanoTime();
        for (int i = 0; i < 1_000_000; i++) {
            new FastValidationException("Invalid input");
        }
        long duration = (System.nanoTime() - start) / 1_000_000;
        System.out.println("1,000,000 lightweight exceptions allocated in: " + duration + " ms");
    }
}
```
**Output:**
```text
1,000,000 lightweight exceptions allocated in: ... (under 15 ms!)
```

### Example 4: Failure Case – Catching `Throwable` or `Error`
```java
public class CatchThrowableTrap {
    public static void main(String[] args) {
        try {
            // Unrecoverable JVM Fatal Error
            throw new OutOfMemoryError("Direct buffer memory exhausted");
        } catch (Throwable t) {
            // ANTI-PATTERN: Catching Throwable or Error prevents JVM shutdown
            // and leaves application in corrupted zombie state!
            System.out.println("Caught: " + t.getClass().getSimpleName() + " (DON'T DO THIS!)");
        }
    }
}
```
**Output:**
```text
Caught: OutOfMemoryError (DON'T DO THIS!)
```

### Example 5: Optimized / Idiomatic – Chained Exceptions Preserving Root Cause
```java
public class ChainedExceptionDemo {
    public static void processPayment(String accountId) {
        try {
            simulateNetworkTimeout();
        } catch (IOException networkError) {
            // IDIOMATIC: Wrap low-level technical exception into domain exception,
            // passing original error as the CAUSE!
            throw new IllegalStateException("Payment processing failed for account: " + accountId, networkError);
        }
    }

    private static void simulateNetworkTimeout() throws IOException {
        throw new IOException("Connection reset by peer: 10.0.4.12:443");
    }

    public static void main(String[] args) {
        try {
            processPayment("ACC-8831");
        } catch (IllegalStateException e) {
            System.out.println("Domain Message: " + e.getMessage());
            System.out.println("Root Cause:     " + e.getCause());
        }
    }
}
```
**Output:**
```text
Domain Message: Payment processing failed for account: ACC-8831
Root Cause:     java.io.IOException: Connection reset by peer: 10.0.4.12:443
```

---

## 6. Best Practices

1. **Use Checked Exceptions for Recoverable Conditions:** If the caller can reasonably be expected to recover (retry alternate server, prompt user), use a checked exception.
2. **Use Unchecked Exceptions for Programming Bugs:** If the failure is due to precondition violation or contract bug (null argument, invalid index), throw a `RuntimeException`.
3. **Never Catch `Throwable` or `Error`:** Catching `Error` (e.g. `OutOfMemoryError`) prevents proper JVM fail-fast shutdown and leaves threads in corrupted states.
4. **Always Preserve Root Causes:** When wrapping exceptions, pass the caught exception to the new exception's constructor (`new ServiceException("msg", caughtEx)`).
5. **Never Use Exceptions for Normal Flow Control:** Exceptions are expensive ($100\times$ slower than branches) and obscure control flow.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Is Wrong | Correct Idiom |
| :--- | :--- | :--- |
| Catching `Throwable` | Catches fatal JVM errors like `OutOfMemoryError`, keeping corrupted servers alive. | Catch `Exception` or specific subtypes. |
| Losing root cause | Writing `throw new CustomException(e.getMessage())` drops original stack trace. | Write `throw new CustomException("msg", e)`. |
| Declaring `throws Exception` | Weakens API contract; callers cannot tell what specific errors can occur. | Declare specific exceptions (`throws IOException, SQLException`). |
| Empty catch block | Silently swallows errors, creating impossible-to-debug production issues. | Log error or rethrow. |
| Using exceptions for loop termination | Massive performance penalty and anti-pattern. | Use conditional loop checks. |

---

## 8. Performance & Security Notes

### 8.1 Stack Trace Information Leakage
Printing full stack traces directly to HTTP responses or public UI screens exposes internal package names, database server IP addresses, library versions, and filesystem paths to adversaries. In production, log stack traces securely to server logs and return generic correlation IDs (`"Error ID: ERR-9921"`) to clients.

---

## 9. Comparison: Checked vs Unchecked vs Error

| Dimension | Checked Exceptions | Unchecked (`RuntimeException`) | `Error` |
| :--- | :--- | :--- | :--- |
| **Parent Class** | `java.lang.Exception` | `java.lang.RuntimeException` | `java.lang.Error` |
| **Compiler Enforcement**| Must catch or declare | Optional | Optional |
| **Cause** | External environment / I/O | Program bug / contract breach | JVM / hardware catastrophe |
| **Recovery Expected?** | ✅ Yes | ❌ Fix code bug | ❌ Terminate process |
| **Examples** | `IOException`, `SQLException` | `NullPointerException`, `IAE` | `OutOfMemoryError`, `StackOverflow` |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the fundamental difference between Checked and Unchecked exceptions?</strong></summary>

- **Checked Exceptions** (subclasses of `Exception` that do not inherit from `RuntimeException`) represent anticipated, recoverable conditions. The compiler enforces that callers must either handle them via `try-catch` or declare them in their method signature via `throws`.
- **Unchecked Exceptions** (subclasses of `RuntimeException` and `Error`) represent programming flaws, contract bugs, or fatal JVM conditions. The compiler does not require catching or declaring them.
</details>

<details>
<summary><strong>2. Why is throwing an exception computationally expensive in Java?</strong></summary>

Over 90% of the cost is consumed by the native method `fillInStackTrace()`, which halts the thread and walks every stack frame currently on the call stack to record class names, method names, line numbers, and bytecode indices.
</details>

<details>
<summary><strong>3. What are Suppressed Exceptions and how are they retrieved?</strong></summary>

Introduced in Java 7 alongside try-with-resources, suppressed exceptions occur when an exception is thrown in the `try` block, and subsequent exceptions are thrown while closing resources. Rather than discarding subsequent exceptions, the JVM attaches them to the primary exception. They can be inspected using `ex.getSuppressed()`.
</details>

<details>
<summary><strong>4. Why is it an anti-pattern to catch java.lang.Throwable?</strong></summary>

`Throwable` is the superclass of both `Exception` and `Error`. Catching `Throwable` intercepts fatal JVM errors such as `OutOfMemoryError`, `StackOverflowError`, and `VirtualMachineError`. Attempting to continue execution after an `Error` leaves the JVM in an undefined, corrupted state.
</details>

<details>
<summary><strong>5. Can an Error be caught in a try-catch block?</strong></summary>

Syntactically yes (`catch (Error e)` or `catch (Throwable t)`), but architecturally it should **never** be done. Errors signal conditions from which normal application recovery is impossible.
</details>

<details>
<summary><strong>6. What is the purpose of Exception Chaining?</strong></summary>

Exception chaining allows a high-level abstraction (e.g. `OrderServiceException`) to wrap a lower-level technical exception (e.g. `SQLException`) while preserving the original exception as the "cause". This presents a clean API to clients while preserving the complete diagnostic history for developers via `ex.getCause()`.
</details>

<details>
<summary><strong>7. How can you create a lightweight exception that does not incur stack walk overhead?</strong></summary>

By overriding `fillInStackTrace()` to return `this` without calling `super.fillInStackTrace()`, or by calling the `protected Throwable(message, cause, enableSuppression, writableStackTrace)` constructor with `writableStackTrace = false`.
</details>

<details>
<summary><strong>8. Does a finally block run if System.exit(0) is called in the try block?</strong></summary>

No. Calling `System.exit()` immediately terminates the JVM process; remaining threads are killed instantly, and no `finally` blocks are executed.
</details>

---

## 11. Practice Exercises

### Easy
1. **Checked Exception File Reader:** Write a method reading a file that declares `throws IOException` and catch it in `main`.
2. **Precondition Validator:** Validate an age parameter and throw `IllegalArgumentException` if negative.
3. **Print Cause:** Catch a chained exception and print its root cause message.

### Medium
4. **Suppressed Exception Inspector:** Construct a custom `AutoCloseable` that throws in both `try` and `close()`, printing all suppressed exceptions.
5. **Exception Wrapping Pipeline:** Build a service that catches database errors and re-throws a domain `BankingException` with full cause chaining.
6. **Stackless Exception Benchmark:** Measure the latency difference between throwing a standard `RuntimeException` vs a stackless exception across 100,000 iterations.

### Hard
7. **Thread Death Detection:** Write a thread monitor that detects when worker threads die due to uncaught runtime exceptions using `Thread.UncaughtExceptionHandler`.
8. **Bytecode Exception Table Inspection:** Compile a try-catch-finally block and inspect the bytecode exception table using `javap -v`.
9. **Resilient Circuit Breaker State Machine:** Build a state machine that tracks consecutive exception rates and opens the circuit to fail fast.

<details>
<summary><strong>Exercise Solutions</strong></summary>

```java
// Solution to Exercise 1: Checked Exception File Reader
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;

public class FileReaderDemo {
    public static String readFile(String path) throws IOException {
        return Files.readString(Path.of(path));
    }

    public static void main(String[] args) {
        try {
            readFile("config.txt");
        } catch (IOException e) {
            System.out.println("Handled missing file: " + e.getMessage());
        }
    }
}
```
</details>

---

## 12. Summary & Cheat Sheet

```
┌────────────────────────────────────────────────────────────────────────┐
│                   EXCEPTION HIERARCHY CHEAT SHEET                      │
├────────────────────────────────────────────────────────────────────────┤
│ Throwable:          Root of all errors and exceptions                  │
│ Error:              Fatal JVM conditions; NEVER catch                  │
│ Exception:          Recoverable conditions (Checked unless RuntimeEx)  │
│ RuntimeException:   Unchecked; programming bugs / contract breaches    │
│ Cost of Exception:  fillInStackTrace() consumes 90%+ CPU; avoid in hot │
│                     loops                                              │
│ Cause Chaining:     new CustomException("msg", rootCause)              │
│ Suppressed:         ex.getSuppressed() captures secondary close() errors│
│ Cardinal Rule:      Never use exceptions for normal control flow       │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 13. Further Reading
- Java Language Specification (JLS) Section 11: Exceptions
- *Effective Java (3rd Edition)*, Joshua Bloch:
  - Item 69: "Use exceptions only for exceptional conditions".
  - Item 70: "Use checked exceptions for recoverable conditions and runtime exceptions for programming errors".
  - Item 71: "Avoid unnecessary use of checked exceptions".
  - Item 73: "Throw exceptions appropriate to the abstraction".
