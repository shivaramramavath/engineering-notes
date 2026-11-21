# Defensive Programming in Java

> Engineering fault-tolerant, fail-fast systems: preconditions validation, postconditions assertion, invariant protection, `Objects.requireNonNull`, and designing robust software contracts.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core Engineering / Software Craftsmanship |
| **Prerequisites** | [00_solid-principles](00_solid-principles.md), [04_immutability](04_immutability.md) |
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

### What is Defensive Programming?
Defensive programming is the discipline of designing software that continues to function predictably and reliably despite unexpected inputs, environmental anomalies, or programming mistakes in client code.

The core principle:
> *"Assume Murphy\'s Law: If something can go wrong, it will. Validate contracts at boundaries, protect internal invariants, and fail fast and loudly when violations occur."*

### Why Defensive Programming is Vital
Without defensive boundaries:
- A null or malformed parameter passed into a service is accepted silently, stored in a database, and causes an obscure `NullPointerException` three days later in a completely unrelated background batch job.
- Unvalidated array indexes or paths can lead to memory corruption, out-of-bounds access, or path traversal security exploits.
- Failing fast at the exact point of entry reduces debugging time from days to seconds.

---

## 2. Core Concepts

### Design by Contract (DbC)
Formulated by Bertrand Meyer, Design by Contract models software interaction as a formal agreement between a caller and a supplier:
1. **Preconditions**: What must be true *before* a method is executed (caller\'s responsibility). E.g. argument not null, quantity $> 0$.
2. **Postconditions**: What the method guarantees to be true *after* execution completes (supplier\'s responsibility). E.g. returned list is sorted, balance equals previous balance minus withdrawal.
3. **Class Invariants**: Conditions that must always hold true for a class outside of method execution. E.g. `size >= 0`, `startDate <= endDate`.

### Fail-Fast vs. Fault Tolerance
- **Fail-Fast**: If preconditions are violated, throw an exception immediately (`IllegalArgumentException`, `NullPointerException`) at the exact boundary. Never attempt to "guess" user intent or patch invalid inputs silently.
- **Fault Tolerance**: At architectural boundaries (external network calls, message consumers), catch and isolate failures gracefully without crashing the entire application.

### Defensive Copying
When an object receives a mutable parameter in its constructor or returns a mutable field from a getter, it must create a defensive copy to prevent external code from mutating internal state.

---

## 3. How It Works Internally

### Fail-Fast Execution Pipeline
```
[ Untrusted Input ]
         |
         v
[ Boundary Method Entry ]
         |
         +--> [ Precondition Checks ]
         |       |
         |       +-- Invalid? --> Throw IllegalArgumentException IMMEDIATELY!
         |                        (Zero side-effects; stack trace points directly to caller)
         v
[ Core Business Logic Executes ]
         |
         v
[ Postcondition / Invariant Verification ]
         |
         v
[ Return Valid Result ]
```

By failing at method entry, state corruption is prevented before any database transaction, disk write, or external network call occurs.

---

## 4. Syntax & Usage

### 1. Modern Precondition Validation with `java.util.Objects`
Java standard library provides built-in validation utilities:
```java
import java.util.Objects;

public class ContractDemo {
    public void execute(String name, int index, int size) {
        // Null checks
        Objects.requireNonNull(name, "name cannot be null");

        // Index checks (Java 9+)
        Objects.checkIndex(index, size); // Throws IndexOutOfBoundsException if index < 0 or index >= size

        // Sub-range checks (Java 9+)
        Objects.checkFromToIndex(0, index, size);
    }
}
```

---

## 5. Practical Examples

### Example 1: Enforcing Invariants in Value Objects
**Goal:** Build a `DateRange` class that guarantees `start` is never null and `end` never precedes `start`.

```java
import java.time.LocalDate;
import java.util.Objects;

public record DateRange(LocalDate startDate, LocalDate endDate) {
    public DateRange {
        Objects.requireNonNull(startDate, "startDate cannot be null");
        Objects.requireNonNull(endDate, "endDate cannot be null");
        
        if (endDate.isBefore(startDate)) {
            throw new IllegalArgumentException(
                String.format("endDate (%s) cannot be before startDate (%s)", endDate, startDate)
            );
        }
    }

    public boolean contains(LocalDate date) {
        Objects.requireNonNull(date, "date to check cannot be null");
        return !date.isBefore(startDate) && !date.isAfter(endDate);
    }
}
```

### Example 2: Defensive Copying for Mutable Collections
**Goal:** Prevent external callers from modifying an internal queue.

```java
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public final class JobQueue {
    private final List<String> jobs;

    public JobQueue(List<String> incomingJobs) {
        // Defensive copy on construction:
        if (incomingJobs == null) {
            this.jobs = new ArrayList<>();
        } else {
            this.jobs = new ArrayList<>(incomingJobs);
        }
    }

    public List<String> getJobs() {
        // Defensive copy on read (or unmodifiable view):
        return Collections.unmodifiableList(jobs);
    }

    public void addJob(String job) {
        if (job == null || job.isBlank()) {
            throw new IllegalArgumentException("Job name cannot be blank");
        }
        jobs.add(job);
    }
}
```

### Example 3: Assertions vs. Exceptions
**Goal:** Use Java `assert` for internal invariant verification during development, and runtime exceptions for public API contract validation.

```java
public class BinarySearchTree {
    private int size = 0;

    public void insert(int value) {
        int previousSize = size;
        
        // Execute insertion logic...
        size++;

        // Internal postcondition check:
        // Assertions can be enabled/disabled via JVM flag (-ea / -da).
        // Use ONLY for internal developer sanity checks, NEVER for public argument validation!
        assert size == previousSize + 1 : "Postcondition failed: size did not increment correctly";
    }
}
```

### Example 4: Failure Case — Silent Failure Antipattern
```java
// ANTI-PATTERN: Silent Failure / Swallowing Errors
public void setDiscount(double discount) {
    if (discount < 0 || discount > 1.0) {
        return; // SILENTLY IGNORED! Caller has no idea discount was rejected!
    }
    this.discount = discount;
}
```

---

## 6. Best Practices

1. **Fail Fast and Loudly**: Throw `IllegalArgumentException`, `IllegalStateException`, or `NullPointerException` at the earliest point of failure.
2. **Never Return Null**: Return `Optional<T>`, `List.of()`, or default objects.
3. **Validate Public Method Arguments**: Enforce preconditions on every public API entry point.
4. **Defensively Copy In and Out**: Never store or return references to mutable objects passed by callers.
5. **Never Swallowing Exceptions**: Avoid empty `catch (Exception e) {}` blocks. Always log or wrap in a domain exception.
6. **Use Specific Exception Types**: Prefer `IllegalArgumentException`, `IllegalStateException`, or `NoSuchElementException` over generic `RuntimeException`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Secure Fix |
| :--- | :--- | :--- |
| **Using `assert` for public argument validation** | Assertions are disabled by default in production (`-da`); checks will not run! | Use `Objects.requireNonNull` and `IllegalArgumentException`. |
| **Silently ignoring invalid inputs** | Hides caller bugs; causes mysterious cascading failures downstream. | Throw an explicit exception immediately. |
| **Catching `Throwable` or `Error`** | Catches `OutOfMemoryError` or `StackOverflowError`, leaving the JVM in a corrupt state. | Catch specific, recoverable exceptions. |
| **Defensive copying after validation** | Vulnerable to Time-of-Check to Time-of-Use (TOCTOU) race condition in multithreading. | Copy the argument first, then validate the copy. |
| **Over-validating private methods** | Clutters internal algorithms with redundant checks that cannot fail. | Validate at public boundaries; use `assert` internally. |
| **Modifying input parameters** | Violates caller expectations; creates hidden side-effects. | Treat all input parameters as read-only. |

---

## 8. Performance & Security Notes

### The TOCTOU (Time-of-Check to Time-of-Use) Vulnerability
When receiving a mutable object (such as a `Date` or an array) across a security boundary:
```java
// VULNERABLE TO TOCTOU:
public void setPeriod(Date start, Date end) {
    if (start.after(end)) throw new IllegalArgumentException(); // Check
    // Attacker thread mutates start here!
    this.start = (Date) start.clone(); // Use
}

// SECURE: Copy FIRST, then validate!
public void setPeriod(Date start, Date end) {
    this.start = (Date) start.clone(); // Copy first
    this.end = (Date) end.clone();
    if (this.start.after(this.end)) throw new IllegalArgumentException(); // Validate copy
}
```

---

## 9. Comparison / When to Use What

| Validation Technique | When to Use | Behavior on Failure |
| :--- | :--- | :--- |
| **`Objects.requireNonNull()`** | Validating non-null references | Throws `NullPointerException` |
| **`IllegalArgumentException`** | Precondition bounds/format failures | Throws exception |
| **`IllegalStateException`** | Method called at inappropriate lifecycle state | Throws exception |
| **Java `assert`** | Internal class invariant checks | Throws `AssertionError` (if `-ea` enabled) |
| **Jakarta Bean Validation** | Declarative validation on REST DTOs | Collects validation errors for client response |

---

## 10. Interview Questions & Answers

### Q1: What is the "Fail-Fast" principle and why is it preferred over silent recovery?
<details>
<summary>View Answer</summary>
The Fail-Fast principle states that a system should immediately report any condition that indicates an error or contract violation, terminating execution at the earliest possible stage. Silent recovery (e.g. defaulting a negative salary to 0 or ignoring a null parameter) masks bugs, allowing corrupted or unintended state to propagate deeper into the system. When an error eventually surfaces far from the root cause, debugging becomes exponentially more difficult.
</details>

### Q2: Why should Java assertions (`assert`) never be used for validating public API arguments?
<details>
<summary>View Answer</summary>
Java assertions are designed exclusively for developer testing and internal invariant verification. In production JVMs, assertions are **disabled by default** unless explicitly enabled with the `-ea` command-line flag. If public API argument validation relies on `assert`, those checks will simply not execute in production, leaving the system completely unprotected against invalid inputs. Public arguments must be validated with explicit runtime checks throwing `IllegalArgumentException`.
</details>

### Q3: What is a Time-of-Check to Time-of-Use (TOCTOU) race condition in Java defensive copying?
<details>
<summary>View Answer</summary>
A TOCTOU race condition occurs when a method validates a mutable argument before creating a defensive copy. Between the moment the check completes and the moment the object is copied, another thread can modify the mutable object\'s state. By mutating the object after validation but before copying, the attacker bypasses the validation check entirely. The correct approach is to clone/copy the mutable object first, and then perform validation on the private copy.
</details>

### Q4: When should you throw `IllegalArgumentException` versus `IllegalStateException`?
<details>
<summary>View Answer</summary>
- `IllegalArgumentException`: Thrown when the caller passes an invalid argument value (e.g. passing a negative integer for a deposit amount, or an empty string for an email address).
- `IllegalStateException`: Thrown when the argument itself may be valid, but the receiving object is in an inappropriate state to perform the requested operation (e.g. calling `iterator.remove()` before `iterator.next()`, or attempting to execute a query on a closed database connection).
</details>

### Q5: How does `Objects.checkIndex(index, length)` simplify boundary checking in Java 9+?
<details>
<summary>View Answer</summary>
Prior to Java 9, checking index bounds required writing verbose conditional statements (`if (index < 0 || index >= length) throw new IndexOutOfBoundsException(...)`). `Objects.checkIndex(index, length)` performs this check in a single concise line, throwing an informative `IndexOutOfBoundsException` if the index is out of bounds, and is recognized as an intrinsic candidate by the HotSpot JIT compiler for bound-check elimination.
</details>

### Q6: Why is swallowing exceptions (`catch (Exception e) {}`) one of the worst anti-patterns?
<details>
<summary>View Answer</summary>
Swallowing an exception completely conceals the failure. The program continues executing with corrupt or uninitialized state, and operators have zero log traces or error indicators to identify that an outage occurred. Swallowing exceptions leads to phantom data loss and makes troubleshooting production incidents virtually impossible.
</details>

### Q7: What is an invariant in object-oriented programming?
<details>
<summary>View Answer</summary>
An invariant is a condition or rule that must always remain true for a class throughout its entire lifetime, outside of internal method transitions. For example, in a `BoundedQueue`, an invariant might be `0 <= currentSize <= capacity`. Defensive programming ensures that constructor checks establish the invariant, and all public mutator methods preserve the invariant before returning.
</details>

### Q8: What is the difference between Defensive Programming and Defensive Driving?
<details>
<summary>View Answer</summary>
The analogy originates from driving: a defensive driver assumes other drivers might make mistakes, brake suddenly, or fail to signal, and takes proactive precautions to avoid accidents. Similarly, a defensive programmer assumes client code might pass nulls, out-of-range numbers, or unexpected formats, and validates inputs at boundaries to keep the system resilient against external flaws.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build an Invariant-Protected Bank Account
**Task:** Create a class `Account` with an invariant that balance can never be negative. Implement `deposit` and `withdraw` with fail-fast preconditions.
<details>
<summary>View Solution</summary>

```java
import java.math.BigDecimal;
import java.util.Objects;

public class Account {
    private BigDecimal balance;

    public Account(BigDecimal initialBalance) {
        Objects.requireNonNull(initialBalance, "initialBalance cannot be null");
        if (initialBalance.compareTo(BigDecimal.ZERO) < 0) {
            throw new IllegalArgumentException("Initial balance cannot be negative");
        }
        this.balance = initialBalance;
    }

    public void deposit(BigDecimal amount) {
        Objects.requireNonNull(amount, "amount cannot be null");
        if (amount.compareTo(BigDecimal.ZERO) <= 0) {
            throw new IllegalArgumentException("Deposit amount must be positive");
        }
        balance = balance.add(amount);
    }

    public void withdraw(BigDecimal amount) {
        Objects.requireNonNull(amount, "amount cannot be null");
        if (amount.compareTo(BigDecimal.ZERO) <= 0) {
            throw new IllegalArgumentException("Withdrawal amount must be positive");
        }
        if (balance.compareTo(amount) < 0) {
            throw new IllegalStateException("Insufficient funds for withdrawal");
        }
        balance = balance.subtract(amount);
    }

    public BigDecimal getBalance() {
        return balance;
    }
}
```
</details>

### Exercise 2: Secure Defensive Array Copying
**Task:** Write a method that accepts an array of security permissions, copies it defensively to prevent TOCTOU race conditions, and validates that no permission is null or blank.
<details>
<summary>View Solution</summary>

```java
import java.util.Arrays;
import java.util.Objects;

public class SecurityContext {
    private final String[] permissions;

    public SecurityContext(String[] permissions) {
        Objects.requireNonNull(permissions, "permissions array cannot be null");
        // Step 1: Copy FIRST (prevents TOCTOU)
        this.permissions = permissions.clone();
        
        // Step 2: Validate the private copy
        for (String perm : this.permissions) {
            if (perm == null || perm.isBlank()) {
                throw new IllegalArgumentException("Permission entry cannot be null or blank");
            }
        }
    }

    public String[] getPermissions() {
        return permissions.clone(); // Defensive copy on read
    }
}
```
</details>

### Exercise 3: Standard Object Index Checker
**Task:** Implement a safe buffer reader method that uses `Objects.checkFromIndexSize` to validate slice boundaries.
<details>
<summary>View Solution</summary>

```java
import java.util.Objects;

public class SafeBuffer {
    public static byte[] slice(byte[] source, int offset, int length) {
        Objects.requireNonNull(source, "source buffer cannot be null");
        // Validates: offset >= 0, length >= 0, offset + length <= source.length
        Objects.checkFromIndexSize(offset, length, source.length);

        byte[] slice = new byte[length];
        System.arraycopy(source, offset, slice, 0, length);
        return slice;
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Fail Fast**: Abort immediately on invalid inputs; never guess user intent.
- **Preconditions**: Validate public method arguments with `Objects.requireNonNull` and `IllegalArgumentException`.
- **TOCTOU Defense**: When copying mutable parameters, copy first, then validate the copy.
- **Never Return Null**: Use `Optional`, empty collections (`List.of()`), or null-object patterns.
- **Assert vs. Exception**: Use `assert` for internal invariants; use exceptions for public API boundaries.
- **Never Swallow Errors**: Always log or re-throw caught exceptions.

---

## 13. Further Reading
- Effective Java: [Item 49: Check parameters for validity](https://www.oreilly.com/library/view/effective-java-3rd/9780134686097/)
- Effective Java: [Item 50: Make defensive copies when needed](https://www.oreilly.com/library/view/effective-java-3rd/9780134686097/)
- Bertrand Meyer: [Object-Oriented Software Construction (Design by Contract)](https://www.eiffel.com/values/design-by-contract/)
- Oracle Java SE Documentation: [Programming With Assertions](https://docs.oracle.com/javase/8/docs/technotes/guides/language/assert.html)
