# Throw and Throws: Exception Propagation, Rethrowing, and Precise Rethrow

> An authoritative guide to the `throw` statement, `throws` method clauses, call stack propagation, exception translation, and Java 7+ Precise Rethrow.

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [00_exception-hierarchy-and-types.md](./00_exception-hierarchy-and-types.md) |
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
9. [Comparison: throw vs throws](#9-comparison-throw-vs-throws)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

In Java:
- **`throw`** is an executable statement that immediately raises an exception object (`throw new IllegalArgumentException(...)`).
- **`throws`** is a method signature clause that advertises to callers that this method may propagate one or more checked exception types (`public void readFile() throws IOException`).

Together, they govern how failure signals originate, traverse stack frames, and demand programmatic handling across system boundaries.

---

## 2. Core Concepts

### 2.1 Exception Propagation
When an exception is thrown:
1. The current method execution halts immediately.
2. The JVM searches the current method for a matching `catch` block.
3. If not found, the JVM pops the current stack frame and continues searching in the caller method.
4. This unwinding continues until a matching handler is found or the main thread stack is exhausted, triggering the JVM's `UncaughtExceptionHandler`.

### 2.2 Precise Rethrow (Java 7+)
In Java 6 and earlier, if you caught `Exception` and rethrew it (`throw e;`), the method had to declare `throws Exception`.
Starting in Java 7, the compiler performs static flow analysis: if a `catch (Exception e)` block rethrows `e` without reassigning it, the method signature only needs to declare the **specific checked exceptions** that could actually be thrown inside the `try` block:
```java
public void process() throws IOException, SQLException {
    try {
        doIO();
        doSql();
    } catch (Exception e) {
        logger.error("Failed", e);
        throw e; // Legal in Java 7+! Compiler knows it's only IOException or SQLException!
    }
}
```

---

## 3. How It Works Internally

### 3.1 The `athrow` Bytecode Opcode
When a `throw` statement executes, the compiler emits the bytecode instruction `athrow`:
- The object reference on top of the operand stack must be an instance of `java.lang.Throwable` (or a subclass).
- The JVM examines the current method's Exception Table.
- If unhandled, the JVM pops the frame, restores caller operand stacks, and re-executes `athrow` in the calling frame.

---

## 4. Syntax & Usage

### 4.1 Exception Translation (Chaining)
```java
public Customer getCustomer(String id) {
    try {
        return database.findCustomer(id);
    } catch (SQLException ex) {
        // Exception Translation: Low-level SQL error translated to high-level Domain exception
        throw new NoSuchCustomerException("Customer not found: " + id, ex);
    }
}
```

---

## 5. Practical Examples

### Example 1: Precise Rethrow in Action
```java
import java.io.IOException;
import java.sql.SQLException;

public class PreciseRethrowDemo {
    public static void executeStep(int code) throws IOException, SQLException {
        if (code == 1) throw new IOException("I/O error");
        if (code == 2) throw new SQLException("SQL error");
    }

    // Notice: Method declares specific exceptions, NOT generic Exception!
    public static void run(int code) throws IOException, SQLException {
        try {
            executeStep(code);
        } catch (Exception e) {
            System.out.println("Logging intercepted error: " + e.getMessage());
            // Precise rethrow: compiler analyzes try block and verifies only IOException / SQLException
            throw e;
        }
    }

    public static void main(String[] args) {
        try {
            run(1);
        } catch (IOException | SQLException e) {
            System.out.println("Caught cleanly in caller: " + e.getMessage());
        }
    }
}
```
**Output:**
```text
Logging intercepted error: I/O error
Caught cleanly in caller: I/O error
```

### Example 2: Real-World Parameter Validation Guard Clauses
```java
public class BankTransferService {
    public static void transfer(String fromAccount, String toAccount, double amount) {
        // Fail-Fast Guard Clauses
        if (fromAccount == null || fromAccount.isBlank()) {
            throw new IllegalArgumentException("Source account must be specified");
        }
        if (toAccount == null || toAccount.isBlank()) {
            throw new IllegalArgumentException("Destination account must be specified");
        }
        if (fromAccount.equals(toAccount)) {
            throw new IllegalArgumentException("Cannot transfer to the same account: " + fromAccount);
        }
        if (amount <= 0.0) {
            throw new IllegalArgumentException("Transfer amount must be positive: " + amount);
        }

        System.out.println("Transferring $" + amount + " from " + fromAccount + " to " + toAccount);
    }

    public static void main(String[] args) {
        transfer("ACC-1", "ACC-2", 250.0);
    }
}
```
**Output:**
```text
Transferring $250.0 from ACC-1 to ACC-2
```

### Example 3: Edge Case – Throwing `null`
```java
public class ThrowNullTrap {
    public static void main(String[] args) {
        try {
            Throwable t = null;
            // Throwing null does NOT fail silently; it throws a NullPointerException!
            throw t;
        } catch (NullPointerException e) {
            System.out.println("Caught expected NullPointerException when attempting to throw null!");
        }
    }
}
```
**Output:**
```text
Caught expected NullPointerException when attempting to throw null!
```

### Example 4: Failure Case – The Log-and-Rethrow Anti-Pattern
```java
public class LogAndRethrowTrap {
    public static void step1() throws Exception {
        throw new Exception("Original root failure");
    }

    public static void step2() throws Exception {
        try {
            step1();
        } catch (Exception e) {
            // ANTI-PATTERN: Logging and then re-throwing causes duplicate log entries!
            System.err.println("[LOG] Error in step2: " + e.getMessage());
            throw e; // Rethrowing again!
        }
    }

    public static void main(String[] args) {
        try {
            step2();
        } catch (Exception e) {
            // Logs identical error a SECOND time!
            System.err.println("[LOG] Error in main: " + e.getMessage());
        }
    }
}
```
**Output:**
```text
[LOG] Error in step2: Original root failure
[LOG] Error in main: Original root failure
```
*Rule: Either handle and log the exception, OR rethrow it to the caller. Never do both!*

### Example 5: Optimized / Idiomatic – Exception Translation (Effective Java Item 73)
```java
class LowLevelDatabaseException extends Exception {
    LowLevelDatabaseException(String msg) { super(msg); }
}

class UserNotFoundException extends RuntimeException {
    UserNotFoundException(String userId, Throwable cause) {
        super("User not found with identifier: " + userId, cause);
    }
}

public class ExceptionTranslationDemo {
    public static String findUsername(String id) {
        try {
            if ("invalid".equals(id)) throw new LowLevelDatabaseException("ORA-00942: table does not exist");
            return "Alice";
        } catch (LowLevelDatabaseException ex) {
            // Translate low-level SQL error into high-level domain exception:
            throw new UserNotFoundException(id, ex);
        }
    }

    public static void main(String[] args) {
        try {
            findUsername("invalid");
        } catch (UserNotFoundException e) {
            System.out.println("Domain Message: " + e.getMessage());
            System.out.println("Underlying SQL: " + e.getCause().getMessage());
        }
    }
}
```
**Output:**
```text
Domain Message: User not found with identifier: invalid
Underlying SQL: ORA-00942: table does not exist
```

---

## 6. Best Practices

1. **Document All Throws with `@throws` in Javadoc:** Document both checked exceptions in the method signature and unchecked exceptions in Javadoc.
2. **Translate Exceptions at Abstraction Boundaries:** High-level APIs should not leak low-level checked exceptions (e.g. `SQLException` from an authentication service).
3. **Fail-Fast with Precondition Checks:** Throw `IllegalArgumentException`, `NullPointerException`, or `IllegalStateException` at the very beginning of methods.
4. **Preserve Root Causes During Translation:** Always pass the caught exception as the `cause` argument to custom exceptions.
5. **Never Throw `Throwable` or `Error`:** Only throw subclasses of `Exception` or `RuntimeException`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Is Wrong | Correct Idiom |
| :--- | :--- | :--- |
| Log and Rethrow | Floods log files with duplicate stack traces at every layer of the call stack. | Log OR rethrow; never both. |
| Throwing `null` | Throws `NullPointerException` instead of expected exception. | Ensure thrown object is non-null. |
| Declaring `throws Exception` | Weakens contract; callers cannot anticipate specific checked exceptions. | Declare specific types (`throws IOException`). |
| Swallowing cause in translation | `throw new DomainEx(e.getMessage())` erases original stack trace. | Pass `e` as cause: `new DomainEx(msg, e)`. |
| Modifying parameter in precise rethrow | Reassigning `e` in catch block disables compiler precise rethrow analysis. | Treat caught exception as effectively final. |

---

## 8. Performance & Security Notes

### 8.1 Stack Unwinding Costs
When an exception propagates across 30+ stack frames, the JVM must unwind each frame sequentially, checking exception tables and executing inlined `finally` blocks. Keep exception propagation distances short.

---

## 9. Comparison: throw vs throws

| Dimension | `throw` | `throws` |
| :--- | :--- | :--- |
| **Type** | Executable statement | Method signature clause |
| **Location** | Inside method or block body | In method declaration header |
| **Followed By** | An exception **instance** (`new Ex()`) | One or more exception **classes** |
| **Multiplicity** | Can throw strictly **one** exception | Can declare **multiple** comma-separated classes |
| **Bytecode** | Generates `athrow` opcode | Encoded in `Exceptions` class attribute |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is Java 7's Precise Rethrow feature?</strong></summary>

Precise Rethrow allows a catch block catching a broad type (like `catch (Exception e)`) to rethrow `e` (`throw e;`) while having the method signature declare only the **specific checked exceptions** that can actually be thrown from the `try` block, provided `e` is not reassigned. The compiler verifies this via static type analysis.
</details>

<details>
<summary><strong>2. What is Exception Translation (Effective Java Item 73)?</strong></summary>

Exception translation is the practice where higher layers in an architecture catch lower-level technical exceptions (e.g. `SQLException`, `IOException`) and rethrow domain-appropriate abstractions (e.g. `OrderNotFoundException`, `BillingServiceException`), chaining the original exception as the cause.
</details>

<details>
<summary><strong>3. What happens when an exception is thrown and never caught by any method?</strong></summary>

The exception reaches the bottom of the thread call stack. The JVM invokes the thread's `UncaughtExceptionHandler` (or the default handler), prints the stack trace to `System.err`, and terminates that specific thread. If the thread was the last non-daemon thread, the entire JVM process terminates.
</details>

<details>
<summary><strong>4. Can a method declare throws for unchecked exceptions?</strong></summary>

Yes. A method can declare `throws IllegalArgumentException` or any other `RuntimeException`. While the compiler does not mandate this, it serves as valuable self-documenting code.
</details>

<details>
<summary><strong>5. Can an overriding subclass method declare broader checked exceptions than the superclass?</strong></summary>

No. An overriding subclass method can only declare the same checked exceptions, subclasses of those exceptions, or no exceptions at all. It cannot declare new or broader checked exceptions because that would violate the Liskov Substitution Principle.
</details>

<details>
<summary><strong>6. What bytecode instruction represents the throw statement?</strong></summary>

The `athrow` instruction. It takes the object reference on top of the operand stack and initiates JVM exception table traversal.
</details>

<details>
<summary><strong>7. What happens if you execute throw null;?</strong></summary>

The JVM encounters a null reference on top of the operand stack during `athrow` execution and immediately throws a `NullPointerException`.
</details>

<details>
<summary><strong>8. Why is the "Log and Rethrow" pattern considered harmful?</strong></summary>

Because if every method in an 8-level call stack catches, logs, and rethrows the same exception, the exact same error is written to the log files 8 distinct times, cluttering logs and inflating monitoring metrics artificially.
</details>

---

## 11. Practice Exercises

### Easy
1. **Validation Throw:** Write a method `validateAge(int age)` that throws `IllegalArgumentException` if age < 0.
2. **Method Throws Declaration:** Write a method reading a file that declares `throws IOException`.
3. **Rethrow Checked:** Catch an `IOException`, wrap it in an `UncheckedIOException`, and throw it.

### Medium
4. **Precise Rethrow Implementation:** Write a method with multiple checked exceptions, catch `Exception`, and rethrow using Precise Rethrow.
5. **Exception Translation Service:** Implement a `UserService` that catches `SQLException` and translates it into `UserAlreadyExistsException`.
6. **Thread Uncaught Exception Handler:** Register a custom `UncaughtExceptionHandler` on a thread that records error alerts to a list.

### Hard
7. **Bytecode athrow Verification:** Inspect the bytecode of a method using `javap -c` and identify where `athrow` instructions are generated.
8. **Call Stack Unwinding Benchmark:** Measure latency of returning error codes vs throwing and unwinding across 25 nested call frames.
9. **Circuit Breaker Exception Filter:** Implement an exception filter that ignores validation exceptions but triggers circuit breaker trips on system timeouts.

<details>
<summary><strong>Exercise Solutions</strong></summary>

```java
// Solution to Exercise 1: Validation Throw
public class Validator {
    public static void validateAge(int age) {
        if (age < 0) {
            throw new IllegalArgumentException("Age cannot be negative: " + age);
        }
    }
}
```
</details>

---

## 12. Summary & Cheat Sheet

```
┌────────────────────────────────────────────────────────────────────────┐
│                        THROW & THROWS CHEAT SHEET                      │
├────────────────────────────────────────────────────────────────────────┤
│ throw:              Executes immediate exception raise (athrow opcode) │
│ throws:             Signature clause declaring propagated checked types│
│ Precise Rethrow:    catch (Exception e) { throw e; } preserves types   │
│ Translation:        Translate low-level errors to domain exceptions    │
│ Chaining:           new DomainException("msg", caughtCause)            │
│ Anti-Pattern:       Log and rethrow (Choose one: handle OR propagate)  │
│ Overriding Rule:    Subclasses cannot declare broader checked exceptions│
│ Throwing null:      Triggers NullPointerException                      │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 13. Further Reading
- Java Language Specification (JLS) Section 14.18: The `throw` Statement
- Java Language Specification (JLS) Section 8.4.6: Method `throws` Clause
- *Effective Java (3rd Edition)*, Joshua Bloch, Item 73: "Throw exceptions appropriate to the abstraction".
