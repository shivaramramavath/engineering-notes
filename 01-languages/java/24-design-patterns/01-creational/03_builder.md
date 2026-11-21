# Builder Pattern in Modern Java

> Constructing complex, multi-attribute, immutable objects cleanly: telescoping constructor anti-patterns, fluent interfaces, step builders, and comparisons with Java Records.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core Design Pattern / Creational |
| **Prerequisites** | [04_immutability](../23-design-and-clean-code/04_immutability.md), [04_oop](../../04-oop/README.md) |
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

### What is the Builder Pattern?
The Builder pattern is a creational design pattern that separates the construction of a complex object from its representation, allowing the same construction process to create various representations.

In modern Java, it is the primary architectural solution for **constructing immutable objects with multiple optional parameters** (Effective Java Item 2).

### Why Telescoping Constructors Fail
When an object has 4 or more optional parameters, legacy code often used:
1. **Telescoping Constructors**: Overloading constructors with 1, 2, 3, 4, 5, 6 parameters.
   - *Problem*: `new HttpClient(url, null, 0, true, null, false, 5000)` is completely unreadable. Swapping two boolean or integer parameters compiles silently but causes runtime corruption.
2. **JavaBeans Pattern (No-arg constructor + Setters)**:
   - *Problem*: Objects are mutable. State is in an inconsistent, half-initialized state during construction. Cannot be made immutable.
3. **The Builder Pattern**: Combines the safety of telescoping constructors with the readability of JavaBeans.

---

## 2. Core Concepts

### Fluent Builder
A builder class maintains mutable fields matching the target object. Each setter method returns `this` to allow fluent method chaining:
```java
HttpClient client = new HttpClient.Builder("https://api.bank.com")
    .timeout(Duration.ofSeconds(30))
    .retries(3)
    .followRedirects(true)
    .build();
```

### Step Builder Pattern (Wizard Pattern)
A compile-time state-machine builder that guides developers through a sequence of mandatory steps before unlocking optional parameters and the `build()` method.

---

## 3. How It Works Internally

### Validation in `build()` and Invariant Enforcement
```
[ Client Code ]
       |
       | 1. new User.Builder("john_doe") (Sets mandatory fields)
       | 2. .email("john@example.com")   (Sets optional fields)
       | 3. .age(25)                     (Sets optional fields)
       v
[ builder.build() ]
       |
       | Validates invariants across ALL fields simultaneously!
       | Checks: age >= 18, email matches regex, username not blank
       v
[ new User(this) ] (Private constructor copies fields to `private final`)
       |
       v
Returns 100% Immutable, Fully Validated User Instance
```

---

## 4. Syntax & Usage

### 1. Classic Joshua Bloch Builder
```java
public final class ServerConfig {
    private final String host;        // Mandatory
    private final int port;           // Mandatory
    private final int maxConnections; // Optional (default 100)
    private final boolean ssl;        // Optional (default true)

    private ServerConfig(Builder builder) {
        this.host = builder.host;
        this.port = builder.port;
        this.maxConnections = builder.maxConnections;
        this.ssl = builder.ssl;
    }

    public static class Builder {
        private final String host;
        private final int port;
        private int maxConnections = 100;
        private boolean ssl = true;

        public Builder(String host, int port) {
            this.host = java.util.Objects.requireNonNull(host);
            this.port = port;
        }

        public Builder maxConnections(int val) { this.maxConnections = val; return this; }
        public Builder ssl(boolean val) { this.ssl = val; return this; }

        public ServerConfig build() {
            if (port < 1 || port > 65535) throw new IllegalArgumentException("Invalid port: " + port);
            return new ServerConfig(this);
        }
    }
}
```

---

## 5. Practical Examples

### Example 1: The Step Builder Pattern (Compile-Time Enforced Steps)
**Goal:** Guarantee that `sender`, `recipient`, and `subject` are provided in strict order before `build()` is available.

```java
public final class EmailMessage {
    private final String from;
    private final String to;
    private final String subject;
    private final String body;

    private EmailMessage(String from, String to, String subject, String body) {
        this.from = from;
        this.to = to;
        this.subject = subject;
        this.body = body;
    }

    // Step Interfaces
    public interface FromStep { ToStep from(String from); }
    public interface ToStep { SubjectStep to(String to); }
    public interface SubjectStep { BuildStep subject(String subject); }
    public interface BuildStep {
        BuildStep body(String body);
        EmailMessage build();
    }

    public static FromStep builder() {
        return new StepBuilder();
    }

    private static class StepBuilder implements FromStep, ToStep, SubjectStep, BuildStep {
        private String from;
        private String to;
        private String subject;
        private String body = "";

        @Override public ToStep from(String from) { this.from = from; return this; }
        @Override public SubjectStep to(String to) { this.to = to; return this; }
        @Override public BuildStep subject(String subject) { this.subject = subject; return this; }
        @Override public BuildStep body(String body) { this.body = body; return this; }
        @Override public EmailMessage build() {
            return new EmailMessage(from, to, subject, body);
        }
    }
}

// Usage:
// EmailMessage.builder()
//     .from("admin@company.com")
//     .to("user@company.com")
//     .subject("Welcome")
//     .body("Hello!")
//     .build(); // Impossible to call build() without satisfying all previous steps!
```

### Example 2: Builder with Java Records and Defensive Copying
```java
import java.util.List;
import java.util.ArrayList;

public record HttpRequestPayload(
    String url,
    String method,
    List<String> headers,
    byte[] body
) {
    public static class Builder {
        private final String url;
        private String method = "GET";
        private final List<String> headers = new ArrayList<>();
        private byte[] body = new byte[0];

        public Builder(String url) { this.url = url; }
        public Builder method(String m) { this.method = m; return this; }
        public Builder addHeader(String h) { this.headers.add(h); return this; }
        public Builder body(byte[] b) { this.body = b.clone(); return this; }

        public HttpRequestPayload build() {
            return new HttpRequestPayload(url, method, List.copyOf(headers), body.clone());
        }
    }
}
```

---

## 6. Best Practices

1. **Use Builder When Parameter Count $\ge 4$**: Especially when multiple parameters have identical primitive types.
2. **Make the Resulting Object Immutable**: Make fields `private final` and do not generate setters on the constructed class.
3. **Validate Invariants in `build()`**: Check cross-parameter constraints in the `build()` method before instantiation.
4. **Defensively Copy Collections**: Copy mutable collections inside `build()` using `List.copyOf()`.
5. **Consider Step Builders for Strict Sequences**: Use step builders when parameters must follow a strict business workflow.
6. **Keep Constructor Private**: Force callers to use the builder.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Secure Fix |
| :--- | :--- | :--- |
| **Using Builder for 2 simple fields** | Unnecessary boilerplate; violates KISS. | Use a standard constructor or record. |
| **Validating in individual setter methods** | Cannot validate cross-field invariants (e.g. `startDate <= endDate`). | Validate in `build()` where all fields are present. |
| **Mutable fields in built object** | Built object can be modified externally after `build()`. | Make built object strictly immutable. |
| **Reusing builder instances across threads** | Builders are stateful and not thread-safe. | Use builders as local stack variables. |

---

## 8. Performance & Security Notes

### Memory Overhead of Builder
A builder creates a temporary helper object (`new Builder()`) that is quickly discarded after `build()`.
- HotSpot C2 compiler easily optimizes builders through **Scalar Replacement**: the builder is not allocated on the heap; its fields are mapped directly to CPU registers!
- There is virtually zero GC penalty for using builders in modern Java.

---

## 9. Comparison / When to Use What

| Pattern | Immutability | Readability | Invariant Check | Best For |
| :--- | :--- | :--- | :--- | :--- |
| **Telescoping Constructor** | Yes | Terribly Poor | Constructor | $\le 3$ parameters |
| **JavaBeans (Getters/Setters)**| No | Good | Hard | Mutable legacy frameworks |
| **Fluent Builder** | Yes | Exceptional | In `build()` | $\ge 4$ parameters |
| **Step Builder** | Yes | Enforced at compile time | In each step | Strict sequential workflows |

---

## 10. Interview Questions & Answers

### Q1: Why is the Builder pattern preferred over Telescoping Constructors and JavaBeans?
<details>
<summary>View Answer</summary>
Telescoping constructors become unreadable and error-prone when parameter counts grow, especially with identical types. The JavaBeans pattern introduces mutability and leaves objects in an inconsistent, partially initialized state across multiple setter calls. The Builder pattern provides the safety and immutability of constructors alongside the clear, fluent readability of JavaBeans.
</details>

### Q2: What is the Step Builder pattern?
<details>
<summary>View Answer</summary>
The Step Builder pattern is a variation of the Builder pattern that enforces the creation of mandatory parameters in a strict compile-time sequence. It defines a chain of interfaces representing each step. Each method returns the interface for the next step, and the `build()` method is only available on the final interface, making it impossible for a developer to forget mandatory fields.
</details>

### Q3: When should you NOT use the Builder pattern?
<details>
<summary>View Answer</summary>
When a class has only 1 to 3 parameters, or when all parameters are mandatory and unlikely to change. For simple data structures, Java records or standard constructors are much simpler and avoid unnecessary boilerplate.
</details>

### Q4: How does HotSpot optimize Builders to eliminate heap allocation?
<details>
<summary>View Answer</summary>
Through Escape Analysis and Scalar Replacement. Because the builder instance is created, configured, and built entirely within a single method and its reference does not escape to other threads or fields, HotSpot decomposes the builder into CPU registers, eliminating heap allocation completely.
</details>

### Q5: Can a Builder construct Java Records?
<details>
<summary>View Answer</summary>
Yes. While records have a canonical constructor, complex records with 6+ components can define a static nested `Builder` class that validates defaults and provides a fluent API before calling the record\'s canonical constructor.
</details>

### Q6: Where should invariant validation occur in a Builder?
<details>
<summary>View Answer</summary>
Cross-field invariants must be validated inside the `build()` method. Basic null checks on mandatory parameters can be performed in the builder constructor, but constraints involving multiple fields (e.g. `minConnections <= maxConnections`) must be checked when all attributes have been supplied in `build()`.
</details>

### Q7: Are Builder instances thread-safe?
<details>
<summary>View Answer</summary>
No. Builders are stateful and mutable; they are not designed to be shared across threads. Builders should be instantiated, populated, and consumed locally within a single thread context.
</details>

### Q8: How does Lombok\'s `@Builder` relate to the GoF Builder pattern?
<details>
<summary>View Answer</summary>
Lombok\'s `@Builder` generates the Joshua Bloch fluent builder boilerplate automatically at compile time via annotation processing. While convenient, hand-crafted builders allow custom validation logic, defensive copying, and step builder sequencing that Lombok cannot achieve out-of-the-box.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a DatabaseConnectionConfig Builder
**Task:** Create an immutable `DbConfig` with mandatory `url` and optional `poolSize` (default 10) and `timeoutSeconds` (default 30).
<details>
<summary>View Solution</summary>

```java
import java.util.Objects;

public final class DbConfig {
    private final String url;
    private final int poolSize;
    private final int timeoutSeconds;

    private DbConfig(Builder b) {
        this.url = b.url;
        this.poolSize = b.poolSize;
        this.timeoutSeconds = b.timeoutSeconds;
    }

    public static class Builder {
        private final String url;
        private int poolSize = 10;
        private int timeoutSeconds = 30;

        public Builder(String url) { this.url = Objects.requireNonNull(url); }
        public Builder poolSize(int s) { this.poolSize = s; return this; }
        public Builder timeoutSeconds(int t) { this.timeoutSeconds = t; return this; }
        public DbConfig build() { return new DbConfig(this); }
    }
}
```
</details>

### Exercise 2: Step Builder for Financial Transfer
**Task:** Build a step builder requiring: `fromAccount` $\to$ `toAccount` $\to$ `amount` $\to$ `build()`.
<details>
<summary>View Solution</summary>

```java
import java.math.BigDecimal;

public class Transfer {
    public interface FromStep { ToStep from(String acc); }
    public interface ToStep { AmountStep to(String acc); }
    public interface AmountStep { Transfer build(BigDecimal amount); }

    public static FromStep builder() {
        return from -> to -> amount -> new Transfer(from, to, amount);
    }

    private final String from;
    private final String to;
    private final BigDecimal amount;

    private Transfer(String f, String t, BigDecimal a) {
        this.from = f; this.to = t; this.amount = a;
    }
}
```
</details>

### Exercise 3: Clone-Safe Array Builder
**Task:** Ensure array parameters are cloned inside builder setter and constructor.
<details>
<summary>View Solution</summary>

```java
public final class Packet {
    private final byte[] payload;

    public static class Builder {
        private byte[] payload = new byte[0];
        public Builder payload(byte[] p) {
            this.payload = p != null ? p.clone() : new byte[0];
            return this;
        }
        public Packet build() { return new Packet(payload.clone()); }
    }

    private Packet(byte[] p) { this.payload = p; }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Core Goal**: Construct complex, immutable objects cleanly without parameter confusion.
- **Fluent Chaining**: Return `this` from setter methods.
- **Invariant Validation**: Check multi-field constraints in `build()`.
- **Step Builder**: Enforce mandatory parameter sequences at compile time.
- **Zero Overhead**: HotSpot eliminates builder allocations via scalar replacement.
- **Use Case**: Best for classes with $\ge 4$ parameters or multiple optionals.

---

## 13. Further Reading
- Effective Java: [Item 2: Consider a builder when faced with many constructor parameters](https://www.oreilly.com/library/view/effective-java-3rd/9780134686097/)
- Refactoring Guru: [Builder Design Pattern](https://refactoring.guru/design-patterns/builder)
- Marco Castigliego: [Step Builder Pattern](https://rdafowler.org/step-builder-pattern/)
- Oracle Java Documentation: [StringBuilder Class](https://docs.oracle.com/en/java/javase/21/docs/api/java.base/java/lang/StringBuilder.html)
