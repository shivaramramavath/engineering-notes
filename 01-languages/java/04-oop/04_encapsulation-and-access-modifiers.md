# Encapsulation and Access Modifiers in Java

> An authoritative guide to data encapsulation, information hiding, the four access levels, package-private architectural boundaries, and modular access controls.

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [00_classes-and-objects.md](./00_classes-and-objects.md) |
| **Time to Read** | 25 minutes |
| **Status** | Active |
| **Java Version** | Java 8+ (JPMS module boundaries in Java 9+) |

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
9. [Comparison: Access Levels Matrix](#9-comparison-access-levels-matrix)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

**Encapsulation** is the foundational pillar of object-oriented design that bundles state (data fields) and behavior (methods) into a cohesive unit while restricting direct external access to internal representations.

Closely coupled with encapsulation is **Information Hiding**:
- Exposes only a stable public API contract to external callers.
- Hides internal implementation details, data structures, and algorithms behind access boundaries.
- Allows developers to refactor, optimize, or replace internal implementation logic without breaking external client code.

Java enforces encapsulation at the language level through **Access Modifiers** (`public`, `protected`, package-private, and `private`) and at the architectural level through the **Java Platform Module System (JPMS)**.

---

## 2. Core Concepts

### 2.1 The Four Access Levels
Java defines four levels of visibility:
1. **`private`:** Accessible **only** within the declaring class body.
2. **Package-Private (Default / No Modifier):** Accessible by any class within the **same package**. Inaccessible outside the package.
3. **`protected`:** Accessible by classes in the **same package** AND by **subclasses** located in other packages.
4. **`public`:** Accessible everywhere across all packages (subject to module export rules).

### 2.2 Tell, Don't Ask Principle
True encapsulation means sending commands to objects rather than interrogating their internal data:
- **Anti-Pattern (Ask):** Querying fields to make decisions outside the object (`if (account.getBalance() >= amt) account.setBalance(...)`).
- **Idiomatic Encapsulation (Tell):** Delegating the decision to the object itself (`account.withdraw(amt)`).

---

## 3. How It Works Internally

### 3.1 Class-File Access Flags
In compiled `.class` bytecode, accessibility is encoded in the 16-bit `access_flags` bitmask for classes, fields, and methods:
- `ACC_PUBLIC` (`0x0001`)
- `ACC_PRIVATE` (`0x0002`)
- `ACC_PROTECTED` (`0x0004`)
- Package-private has no bit set in `access_flags` (flags & 7 == 0).

The JVM bytecode verifier checks these flags during class loading and bytecode verification to ensure unauthorized bytecode cannot invoke private or package-private members.

### 3.2 JPMS Module Boundaries (Java 9+)
Starting in Java 9, even a `public` class cannot be accessed from another module unless:
1. The defining module explicitly `exports` the containing package in its `module-info.java`.
2. The consuming module explicitly `requires` the defining module.

---

## 4. Syntax & Usage

### 4.1 Access Modifier Declarations
```java
package com.company.security;

public class Vault {
    // Accessible only within Vault
    private byte[] masterKey;

    // Accessible within com.company.security package only
    boolean isUnlocked;

    // Accessible within package and by subclasses in any package
    protected void auditAccess(String user) {
        System.out.println("Audit: " + user);
    }

    // Accessible everywhere
    public boolean verifyAccess(String credentials) {
        return internalCheck(credentials);
    }

    private boolean internalCheck(String creds) {
        return creds != null && !creds.isBlank();
    }
}
```

---

## 5. Practical Examples

### Example 1: Robust Encapsulation with Invariant Validation
```java
public class TemperatureSensor {
    // State is private; cannot be set to physically impossible temperatures
    private double kelvin;

    public TemperatureSensor(double kelvin) {
        setKelvin(kelvin);
    }

    public void setKelvin(double kelvin) {
        if (kelvin < 0.0) {
            throw new IllegalArgumentException("Temperature cannot be below absolute zero (0 K)");
        }
        this.kelvin = kelvin;
    }

    public double getKelvin() { return kelvin; }
    public double getCelsius() { return kelvin - 273.15; }
    public double getFahrenheit() { return (getCelsius() * 9.0 / 5.0) + 32.0; }

    public static void main(String[] args) {
        TemperatureSensor sensor = new TemperatureSensor(300.0);
        System.out.printf("Kelvin: %.2f K | Celsius: %.2f °C | Fahrenheit: %.2f °F%n",
            sensor.getKelvin(), sensor.getCelsius(), sensor.getFahrenheit());
    }
}
```
**Output:**
```text
Kelvin: 300.00 K | Celsius: 26.85 °C | Fahrenheit: 80.33 °F
```

### Example 2: Real-World Package-Private Framework Boundary
```java
// File: src/main/java/com/engine/api/WorkflowEngine.java
package com.engine.api;

public class WorkflowEngine {
    public static WorkflowJob createJob(String jobName) {
        // Implementation class DefaultWorkflowJob is package-private!
        // External callers cannot instantiate it or cast to it directly.
        return new DefaultWorkflowJob(jobName);
    }
}

// File: src/main/java/com/engine/api/WorkflowJob.java (Public Interface)
public interface WorkflowJob {
    void execute();
}

// File: src/main/java/com/engine/api/DefaultWorkflowJob.java (Package-Private Implementation)
class DefaultWorkflowJob implements WorkflowJob {
    private final String name;
    DefaultWorkflowJob(String name) { this.name = name; }
    @Override
    public void execute() {
        System.out.println("Executing job: " + name);
    }
}
```

### Example 3: Edge Case – Protected Access Across Packages
```java
// Package A
package com.pkg.a;
public class Parent {
    protected int protectedValue = 42;
}

// Package B
package com.pkg.b;
import com.pkg.a.Parent;

public class Child extends Parent {
    public void testAccess() {
        // Legal: Accessing inherited member through 'this' or Child reference
        System.out.println(this.protectedValue);

        // ILLEGAL: Cannot access protected member of another Parent instance from another package!
        // Parent p = new Parent();
        // System.out.println(p.protectedValue); // COMPILE ERROR
    }

    public static void main(String[] args) {
        new Child().testAccess();
    }
}
```
**Output:**
```text
42
```

### Example 4: Failure Case – Exposing Internal Mutable Collections
```java
import java.util.ArrayList;
import java.util.List;

public class LeakyEncapsulationDemo {
    private final List<String> permissions = new ArrayList<>();

    public LeakyEncapsulationDemo() {
        permissions.add("READ");
    }

    // BAD: Returning reference to internal mutable collection breaks encapsulation!
    public List<String> getPermissionsBad() {
        return permissions;
    }

    // GOOD: Returning unmodifiable defensive copy
    public List<String> getPermissionsGood() {
        return List.copyOf(permissions);
    }

    public static void main(String[] args) {
        LeakyEncapsulationDemo demo = new LeakyEncapsulationDemo();
        // Attacker mutates internal state directly:
        demo.getPermissionsBad().add("SUPER_ADMIN");
        System.out.println("Corrupted state: " + demo.getPermissionsBad());
    }
}
```
**Output:**
```text
Corrupted state: [READ, SUPER_ADMIN]
```

### Example 5: Optimized / Idiomatic – Tell, Don't Ask Wallet
```java
public class DigitalWallet {
    private double balance;

    public DigitalWallet(double initialBalance) {
        this.balance = Math.max(0, initialBalance);
    }

    // Business operation encapsulates validation and state change atomically
    public boolean debit(double amount) {
        if (amount <= 0 || amount > balance) {
            return false;
        }
        balance -= amount;
        return true;
    }

    public double getBalance() { return balance; }

    public static void main(String[] args) {
        DigitalWallet wallet = new DigitalWallet(100.0);
        boolean ok = wallet.debit(35.0);
        System.out.println("Debit success: " + ok + ", Remaining: $" + wallet.getBalance());
    }
}
```
**Output:**
```text
Debit success: true, Remaining: $65.0
```

---

## 6. Best Practices

1. **Make Everything as Inaccessible as Possible:** Start every field, method, and class as `private`. Elevate visibility only when strictly demanded by external clients (Effective Java Item 15).
2. **Leverage Package-Private for Architectural Boundaries:** Keep implementation classes package-private so they do not leak into the public API of your library. Expose only interfaces and factory methods.
3. **Never Provide Public Getters/Setters Without Reason:** Naked getters and setters convert objects into dumb data holders. Provide domain-specific operations (`user.changePassword()`).
4. **Return Defensive Copies for Mutable Fields:** If a class must return a mutable field (e.g. `Date`, array, or collection), wrap with `List.copyOf()` or `.clone()`.
5. **Beware `protected` Visibility:** `protected` forms an open contract with external subclasses that is difficult to change without breaking third-party libraries.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Is Wrong | Correct Idiom |
| :--- | :--- | :--- |
| Exposing mutable collections in getters | Callers can add, remove, or clear internal items, corrupting invariants. | Return unmodifiable views (`List.copyOf()`). |
| Defaulting to `public` classes | Clutters library namespace; clients couple to classes intended to be internal. | Make internal helper classes package-private. |
| Automatic getters & setters for all fields | Breaks encapsulation, creating an anemic domain model. | Design methods around business actions. |
| Misunderstanding `protected` across packages | Subclasses can only access protected members on instances of their own type, not on arbitrary parent instances. | Access through `this` or `super`. |
| Thinking `private` prevents reflection | Historically reflection could call `setAccessible(true)`; in Java 9+ JPMS, modular encapsulation blocks reflection across modules. | Use strong encapsulation in `module-info.java`. |

---

## 8. Performance & Security Notes

### 8.1 JIT Devirtualization via Private Methods
Because `private` methods cannot be overridden by subclasses, the JVM compiles calls to them using `invokespecial`. The JIT compiler can immediately inline `private` methods without inserting monomorphic guards or type checks, yielding optimal performance.

---

## 9. Comparison: Access Levels Matrix

| Modifier | Declaring Class | Same Package | Subclass (Diff Pkg) | Entire World |
| :--- | :---: | :---: | :---: | :---: |
| **`private`** | ✅ Yes | ❌ No | ❌ No | ❌ No |
| **Package-Private** *(default)* | ✅ Yes | ✅ Yes | ❌ No | ❌ No |
| **`protected`** | ✅ Yes | ✅ Yes | ✅ Yes | ❌ No |
| **`public`** | ✅ Yes | ✅ Yes | ✅ Yes | ✅ Yes |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the difference between Encapsulation and Information Hiding?</strong></summary>

Encapsulation is the mechanism of packaging data fields and methods together inside an object while restricting direct access to the fields. Information Hiding is the design principle of hiding internal implementation decisions and data structures behind a stable public interface so that changes to internal logic do not impact external callers.
</details>

<details>
<summary><strong>2. What is package-private access in Java, and why is it useful?</strong></summary>

Package-private (the default access when no modifier is specified) allows members to be accessed by any class in the same package, but forbids access from classes in other packages. It is extremely useful for designing clean libraries: you can expose public interfaces while keeping concrete implementation classes, helper utilities, and internal state package-private.
</details>

<details>
<summary><strong>3. Can a subclass in another package access a protected member of a parent class instance?</strong></summary>

A subclass in another package can access a `protected` member on its **own instance** (`this.field` or through a reference of the subclass type). However, it cannot access the protected member on a reference to the **parent class** (`parentRef.field`), as that would violate access boundaries for other potential subclasses.
</details>

<details>
<summary><strong>4. Why is exposing public fields generally considered bad practice?</strong></summary>

Public fields allow external code to mutate state without validation, violating class invariants (e.g. setting an account balance to a negative number). Furthermore, once a field is public, you can never change its internal data type or representation without breaking all external clients.
</details>

<details>
<summary><strong>5. What access modifiers can be applied to a top-level class?</strong></summary>

A top-level class can only be declared **`public`** or **package-private** (no modifier). It cannot be declared `private` or `protected` (those modifiers are only valid for nested member classes).
</details>

<details>
<summary><strong>6. How did Java 9 JPMS enhance encapsulation beyond access modifiers?</strong></summary>

Prior to Java 9, any `public` class on the classpath was accessible by any other class. JPMS introduced strong encapsulation at the module boundary: a `public` class in package `A` is hidden from other modules unless the declaring module explicitly `exports A` in its `module-info.java`. Furthermore, deep reflection via `setAccessible(true)` is blocked by default across modules.
</details>

<details>
<summary><strong>7. What is the "Tell, Don't Ask" principle in OOP?</strong></summary>

"Tell, Don't Ask" advises developers not to query an object for its internal data in order to make decisions and then update the object. Instead, tell the object what action to perform through a method, allowing the object to encapsulate the business logic and enforce its own invariants.
</details>

<details>
<summary><strong>8. What is the bytecode opcode used to invoke private methods?</strong></summary>

Private methods are invoked using the `invokespecial` opcode because they are statically bound and cannot be overridden dynamically.
</details>

---

## 11. Practice Exercises

### Easy
1. **Encapsulated User Profile:** Create a `UserProfile` class with private fields for `username` and `email`, providing getters and validation.
2. **Access Matrix Tester:** Write classes across two different packages verifying which access levels compile and which fail.
3. **Password Sanitizer:** Implement a `Credentials` class that stores a password hash privately and only exposes a `verify(char[] attempt)` method.

### Medium
4. **Package-Private Service Factory:** Create a package `com.service` with a public interface `PaymentGateway`, a package-private implementation `StripeGateway`, and a public factory `PaymentGatewayFactory`.
5. **Defensive Collection Protection:** Refactor a leaky class containing a `List<Order>` to guarantee 100% immutability of returned data.
6. **Protected Template Method:** Design a class hierarchy where a `protected abstract void onProcess()` hook is overridden by subclasses.

### Hard
7. **Modular Encapsulation Verification:** Build a multi-module Java project demonstrating that public classes in non-exported packages cannot be accessed at compile time or runtime.
8. **Reflection Access Blocker:** Write a test verifying that accessing private fields across modules throws `InaccessibleObjectException` under Java 17+.
9. **Zero-Overhead Encapsulation Benchmark:** Author a JMH benchmark measuring direct field access vs getter access under HotSpot C2 JIT inlining.

<details>
<summary><strong>Exercise Solutions</strong></summary>

```java
// Solution to Exercise 1: Encapsulated User Profile
public class UserProfile {
    private final String username;
    private String email;

    public UserProfile(String username, String email) {
        if (username == null || username.isBlank()) throw new IllegalArgumentException("Username required");
        setEmail(email);
        this.username = username;
    }

    public void setEmail(String email) {
        if (email == null || !email.contains("@")) throw new IllegalArgumentException("Invalid email");
        this.email = email;
    }

    public String getUsername() { return username; }
    public String getEmail() { return email; }
}
```
</details>

---

## 12. Summary & Cheat Sheet

```
┌────────────────────────────────────────────────────────────────────────┐
│                     ACCESS MODIFIERS CHEAT SHEET                       │
├────────────────────────────────────────────────────────────────────────┤
│ private:           Enclosing class body only (invokespecial)           │
│ Package-Private:   Same package only (Ideal for internal library code) │
│ protected:         Same package + Subclasses in any package            │
│ public:            Accessible everywhere (Subject to JPMS exports)     │
│ Core Rule:         Minimize visibility (Effective Java Item 15)        │
│ Invariant Safety:  Always return defensive copies of mutable fields    │
│ Principle:         Tell, Don't Ask (Delegate logic to the object)      │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 13. Further Reading
- Java Language Specification (JLS) Section 6.6: Access Control
- [JEP 261: Module System (Strong Encapsulation)](https://openjdk.org/jeps/261)
- *Effective Java (3rd Edition)*, Joshua Bloch, Item 15: "Minimize the accessibility of classes and members".
