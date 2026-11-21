# Enterprise Java Anti-Patterns

> Recognizing, diagnosing, and refactoring notorious architectural and object-oriented anti-patterns: God Class, Anemic Domain Model, Golden Hammer, Lava Flow, and Exception Swallowing.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core Architecture / Technical Review |
| **Prerequisites** | [00_pattern-selection-guide](00_pattern-selection-guide.md), [03_refactoring](../23-design-and-clean-code/03_refactoring-and-code-smells.md) |
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

### What is an Anti-Pattern?
An anti-pattern is a common response to a recurring problem that is usually ineffective, counterproductive, and carries significant compound interest in technical debt and production risk.

While design patterns document best practices, anti-patterns document **attractive traps**—solutions that initially appear intuitive or fast to implement, but inevitably degrade the architecture over time.

---

## 2. Core Concepts

### Top 5 Enterprise Java Anti-Patterns
1. **The God Class (Monster Class)**: A single massive class (2,000+ lines) that knows too much and does too much, aggregating unrelated business concerns.
2. **Anemic Domain Model**: Domain entities that are mere bags of getters and setters containing zero business logic, with all behavior scattered across procedural "Manager" services.
3. **The Golden Hammer**: Believing that one specific pattern or technology (e.g. Microservices, Reactive Streams, or Redis) solves every software problem.
4. **Lava Flow (Dead / Zombie Code)**: Preserving obsolete, dead code because "we are afraid to delete it in case something breaks".
5. **The Burying Ground (Exception Swallowing)**: Empty `catch (Exception e) {}` blocks that silence production crashes.

---

## 3. How It Works Internally

### Anemic Domain Model vs. Rich Domain Model
```
+--------------------------------------------------------------------------+
|                  ANEMIC DOMAIN MODEL (PROCEDURAL ANTI-PATTERN)           |
|  [ AccountEntity ] -> purely holds data (private double balance, setters)|
|         ^                                                                |
|  [ AccountManager ] -> evaluates if balance < 0, mutates entity, saves   |
|  (Data and Behavior are completely separated; violates Encapsulation!)   |
+--------------------------------------------------------------------------+
|                  RICH DOMAIN MODEL (TRUE OBJECT-ORIENTED)                |
|  [ Account ] -> encapsulates both data and behavior:                     |
|                 account.withdraw(Money amount);                          |
|                 (Protects its own invariants internally!)                |
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### 1. Refactoring an Anemic Model to Rich Domain Model
```java
// ANTI-PATTERN: Anemic Model
public class AnemicOrder {
    private double total;
    public double getTotal() { return total; }
    public void setTotal(double t) { this.total = t; }
}

// RICH DOMAIN MODEL:
public class RichOrder {
    private BigDecimal total = BigDecimal.ZERO;

    public void addLineItem(BigDecimal price, int quantity) {
        if (quantity <= 0) throw new IllegalArgumentException("Invalid quantity");
        this.total = this.total.add(price.multiply(BigDecimal.valueOf(quantity)));
    }

    public BigDecimal getTotal() { return total; }
}
```

---

## 5. Practical Examples

### Example 1: Eliminating the Exception Swallowing Anti-Pattern
```java
// DANGEROUS ANTI-PATTERN:
try {
    processPayment();
} catch (Exception e) {
    // SILENT CATASTROPHE: Bug swallowed! Customer charged twice!
}

// CLEAN PRODUCTION REMEDIATION:
try {
    processPayment();
} catch (PaymentException e) {
    logger.error("Payment processing failed for order {}: {}", orderId, e.getMessage(), e);
    throw new DomainTransactionException("Payment failure", e);
}
```

---

## 6. Best Practices

1. **Keep Domain Models Rich**: Place business logic directly on the classes that own the data.
2. **Delete Dead Code Aggressively**: Version control (Git) remembers old code; delete unused classes rather than commenting them out.
3. **Break Down God Classes**: Decompose monolithic services into cohesive domain services.
4. **Never Catch `Throwable` or `Error`**: Catch specific, recoverable exceptions.
5. **Question Dogma (Avoid Golden Hammer)**: Choose technology based on measurable requirements, not industry hype.

---

## 7. Common Mistakes & Pitfalls

| Anti-Pattern | Primary Danger | Refactoring Strategy |
| :--- | :--- | :--- |
| **God Class** | Impossible to test; high merge conflicts | Extract Class / Single Responsibility Principle |
| **Anemic Domain** | Procedural sprawl; duplicate business rules | Move business methods into domain entities |
| **Exception Swallowing** | Phantom data loss; impossible to debug | Proper logging and exception translation |
| **Lava Flow** | Cognitive bloat from dead code | Delete with Git safety net |

---

## 8. Performance & Security Notes

### Invariant Protection in Domain Models
Anemic domain models allow external services to put entities into corrupt states (e.g. setting `account.setBalance(-100)` directly). Rich domain models enforce invariants inside methods, eliminating security vulnerabilities caused by invalid state configurations.

---

## 9. Comparison / When to Use What

| Architecture Style | Domain Model Style | Best Fit |
| :--- | :--- | :--- |
| **Simple CRUD** | Anemic / Records | Simple data-entry applications |
| **Complex DDD** | Rich Domain Model | Core business systems with complex rules |

---

## 10. Interview Questions & Answers

### Q1: What is an Anemic Domain Model and why did Martin Fowler call it an anti-pattern?
<details>
<summary>View Answer</summary>
An Anemic Domain Model consists of entities that look like objects (they have names and properties), but contain zero business behavior—only getters and setters. All business logic resides in procedural Service or Manager classes. Fowler criticized it because it defeats the fundamental purpose of object-oriented design: combining data and behavior together. It leads to duplicate logic, lack of encapsulation, and procedural code disguised as OOP.
</details>

### Q2: What is the "Lava Flow" anti-pattern?
<details>
<summary>View Answer</summary>
Lava Flow refers to old, dead, or obsolete code that solidifies in a codebase because the original authors have left and current developers are afraid to remove it, fearing hidden dependencies. It clutters the architecture and confuses maintainers. The fix is to rely on Git history and delete dead code boldly.
</details>

---

## 11. Practice Exercises

### Exercise 1: Refactor Anemic Bank Account
**Task:** Move deposit and withdraw validation logic from `AccountService` into `Account`.
<details>
<summary>View Solution</summary>

```java
public class Account {
    private int balance;
    public void deposit(int amt) {
        if (amt <= 0) throw new IllegalArgumentException("Positive amount required");
        this.balance += amt;
    }
}
```
</details>

---


## 1. Top 10 Enterprise Java Anti-Patterns

Understanding anti-patterns is just as critical as mastering design patterns. Here are the 10 most common enterprise Java architectural anti-patterns:

### 1. The God Object (Blob)
- **Symptom**: A single class (e.g., `OrderManager` or `SystemController`) spans 3,000+ lines, holds 50 fields, and implements dozens of unrelated responsibilities.
- **Why It Fails**: Violates Single Responsibility Principle. Impossible to unit test in isolation, high merge conflicts, extreme blast radius for regressions.
- **Remedy**: Decompose using Single Responsibility Principle. Extract value objects, domain services, and dedicated sub-repositories.

### 2. Anemic Domain Model
- **Symptom**: Entities consist solely of private getters and setters with zero business logic. All business rules reside inside massive procedural Service classes (`OrderService.calculateTax()`, `OrderService.verifyCredit()`).
- **Why It Fails**: Transforms object-oriented design into procedural programming with classes as glorified C structs. Entity validation rules get duplicated across multiple services.
- **Remedy**: Push business invariants into the domain entities (Rich Domain Model). Methods like `order.cancel(reason)` ensure the entity guarantees its own consistency.

### 3. Golden Hammer
- **Symptom**: Developers force their favorite pattern or tool (e.g., Reactive streams, Event Sourcing, Microservices, or Reflection) onto every single problem, regardless of fit.
- **Why It Fails**: Adds immense cognitive overhead, debugging difficulty, and latency where simple synchronous, procedural, or standard MVC patterns would suffice.
- **Remedy**: Always evaluate the architectural trade-offs matrix. Match complexity to business scale.

### 4. Premature Optimization
- **Symptom**: Writing intricate, unreadable bitwise logic, custom thread pools, or manual object pooling for standard POJOs before measuring performance.
- **Why It Fails**: Java's modern JVM (C2 compiler, G1/ZGC) optimizes idiomatic, clean code far better than convoluted manual hacks. Object pooling actually degrades GC throughput in modern JVMs.
- **Remedy**: Profile first with JFR (Java Flight Recorder) and JMH. Optimize only verified bottlenecks.

### 5. Lava Flow (Dead Code & Obsolete Workarounds)
- **Symptom**: Abandoned classes, deprecated methods, unused interfaces, and legacy hack workarounds left in the codebase because "we don't know who calls it."
- **Why It Fails**: Clutters code search, confuses new team members, bloats bytecode, and triggers false positives in static analyzers.
- **Remedy**: Use IDE structural search, compiler warnings (`-Xlint:deprecation`), and code coverage to ruthlessly prune dead code. Rely on Git history for recovery.

### 6. Poltergeists (Gypsy Wagons)
- **Symptom**: Transient classes whose only role is to initialize another class or pass a single message and disappear (e.g., `OrderProcessorInitializerHelperFactory`).
- **Why It Fails**: Clutters the class hierarchy with zero added value or encapsulation.
- **Remedy**: Collapse the intermediary class directly into the consumer or producer.

### 7. Error Hiding (Swallowed Exceptions)
- **Symptom**: Catching `Exception` or `Throwable` and printing a line or returning `null` without rethrowing or logging stack traces:
  ```java
  try {
      database.save(order);
  } catch (Exception e) {
      System.out.println("Error saving"); // Swallowed!
  }
  ```
- **Why It Fails**: The system enters an inconsistent state, data corruption occurs silently, and debugging root causes in production becomes virtually impossible.
- **Remedy**: Handle specific checked exceptions, log structured diagnostic context (MDC), and rethrow wrapped unchecked domain exceptions (`new PersistenceException(e)`).

### 8. Distributed Monolith
- **Symptom**: Microservices that share a single common database, perform synchronous cascading HTTP calls across 8 services to complete one user request, and require coordinated simultaneous deployments.
- **Why It Fails**: Inherits all the operational headaches of microservices (network latency, distributed tracing, network partitions) with none of the benefits of a monolith.
- **Remedy**: Re-establish bounded contexts. Use asynchronous event-driven choreography via Kafka/RabbitMQ or merge tightly coupled services back into a modular monolith.

### 9. Hardcoded Secrets & Configuration Coupling
- **Symptom**: Storing database passwords, API tokens, or environment-specific URLs inside `.java` source code or static constant classes.
- **Why It Fails**: Massive security vulnerability; credentials leak in version control; requires code recompilation to change an environment endpoint.
- **Remedy**: Externalize configuration using Twelve-Factor App methodology (`application.properties`, environment variables, AWS Secrets Manager, HashiCorp Vault).

### 10. Thread-Per-Connection Starvation
- **Symptom**: Launching traditional unconstrained OS platform threads (`new Thread()`) for high-concurrency I/O workloads.
- **Why It Fails**: Exhausts native operating system memory and thread limits, causing `OutOfMemoryError: unable to create native thread`.
- **Remedy**: Adopt Java 21+ Virtual Threads (`Executors.newVirtualThreadPerTaskExecutor()`) or non-blocking asynchronous architectures.

## 12. Summary / Cheat Sheet

- **God Class**: Decompose into small, focused classes.
- **Anemic Model**: Co-locate data with behavior in rich domain models.
- **Exception Swallowing**: Never use empty catch blocks; log or rethrow.
- **Lava Flow**: Delete dead code fearlessly; Git preserves history.
- **Golden Hammer**: Choose tools matching real requirements, not hype.

---

## 13. Further Reading
- Martin Fowler: [Anemic Domain Model](https://martinfowler.com/bliki/AnemicDomainModel.html)
- AntiPatterns: Refactoring Software, Architectures, and Projects in Crisis (Brown et al.)


### 11. Primitive Obsession
- **Symptom**: Relying exclusively on standard Java primitives (`String`, `Long`, `BigDecimal`) to represent rich domain concepts like `EmailAddress`, `Currency`, `OrderId`, or `TaxIdentificationNumber`.
- **Why It Fails**: Leads to scattered duplicate validation checks, high risk of argument transposing bugs (e.g. `sendTransfer(senderId, recipientId, amount)` where sender and recipient are accidentally reversed), and inability to enforce domain invariants.
- **Remedy**: Encapsulate primitives into lightweight Value Objects or Java Records:
  ```java
  public record EmailAddress(String value) {
      private static final Pattern VALID_EMAIL = Pattern.compile("^[A-Z0-9._%+-]+@[A-Z0-9.-]+\.[A-Z]{2,6}$", Pattern.CASE_INSENSITIVE);
      public EmailAddress {
          Objects.requireNonNull(value, "Email cannot be null");
          if (!VALID_EMAIL.matcher(value).matches()) {
              throw new IllegalArgumentException("Malformed email address: " + value);
          }
      }
  }
  ```

### 12. Feature Envy
- **Symptom**: A method in Class A spends most of its lines querying getters from Class B to perform calculations on Class B's data:
  ```java
  public class OrderReceiptPrinter {
      public double computeTotalWithTax(Order order) {
          double subtotal = 0.0;
          for (OrderItem item : order.getItems()) {
              subtotal += item.getPrice() * item.getQuantity();
          }
          return subtotal * (1.0 + order.getCustomer().getTaxRate());
      }
  }
  ```
- **Why It Fails**: Breaks data encapsulation and increases coupling between classes.
- **Remedy**: Move the method directly to the class that owns the data (Information Expert principle in GRASP). Let `Order.calculateTotalWithTax()` handle its own calculations.

### 13. Shotgun Surgery
- **Symptom**: Making a single logical change (e.g., adding a new customer status code or currency type) requires editing 15 different source files across controllers, converters, models, and repositories.
- **Why It Fails**: High likelihood of missing one of the 15 places, creating silent edge-case regressions.
- **Remedy**: Consolidate the related responsibilities into a single cohesive module or enum with polymorphic behavior.

### 14. Exceptions as Flow Control
- **Symptom**: Using `try-catch` blocks to control ordinary application logic (e.g. catching `NullPointerException` instead of checking for `null`, or catching `NumberFormatException` to validate user input in loops).
  ```java
  // Anti-Pattern: Exception for control flow
  public boolean isInteger(String text) {
      try {
          Integer.parseInt(text);
          return true;
      } catch (NumberFormatException e) {
          return false;
      }
  }
  ```
- **Why It Fails**: Creating exceptions requires taking a snapshot of the thread execution stack (`fillInStackTrace()`), which is an expensive JVM operation. It also obscures real bugs.
- **Remedy**: Validate with state-testing methods or regex/character checks prior to invocation.

### 15. Inappropriate Intimacy
- **Symptom**: Two classes know far too much about each other's internal private mechanics, accessing package-private fields or calling internal helper routines in tight lockstep.
- **Why It Fails**: Prevents independent testing or refactoring of either class.
- **Remedy**: Introduce a formal interface boundary or merge the tightly coupled classes if they truly represent a single cohesive concept.


---

## 2. Automated Anti-Pattern Detection Tooling & Rules

Modern enterprise Java teams enforce architecture quality gates in CI/CD pipelines using static analysis and automated rule testing:

### 1. ArchUnit Anti-Pattern Tests
Catch structural anti-patterns before code merges into `main`:

```java
import com.tngtech.archunit.core.domain.JavaClasses;
import com.tngtech.archunit.core.importer.ClassFileImporter;
import com.tngtech.archunit.lang.ArchRule;
import org.junit.jupiter.api.Test;
import static com.tngtech.archunit.lang.syntax.ArchRuleDefinition.*;

public class AntiPatternEnforcementTest {
    private final JavaClasses classes = new ClassFileImporter().importPackages("com.example.app");

    @Test
    void preventGodObjects() {
        // Enforce maximum method count per class to avoid Blob/God objects
        ArchRule rule = classes().that().resideInAPackage("..service..")
            .should().haveSimpleNameNotEndingWith("Manager")
            .andShould().haveModifier(java.lang.reflect.Modifier.PUBLIC);
        rule.check(classes);
    }

    @Test
    void preventThrowingGenericExceptions() {
        // Anti-pattern: Throwing Exception or Throwable directly
        ArchRule noGenericThrows = methods()
            .should().notDeclareThrowableOfType(Exception.class)
            .andShould().notDeclareThrowableOfType(Throwable.class);
        noGenericThrows.check(classes);
    }

    @Test
    void preventFieldInjection() {
        // Anti-pattern: @Autowired or @Inject directly on private fields
        ArchRule noFieldInjection = fields()
            .should().notBeAnnotatedWith("org.springframework.beans.factory.annotation.Autowired")
            .andShould().notBeAnnotatedWith("jakarta.inject.Inject");
        noFieldInjection.check(classes);
    }
}
```

### 2. SonarQube & SpotBugs Quality Profiles

Key rules to activate to prevent critical Java anti-patterns:

| Anti-Pattern | SonarQube Rule Key | SpotBugs Detector |
| :--- | :--- | :--- |
| **Swallowed Exception** | `java:S108` (Nested blocks of code should not be left empty) | `DE_MIGHT_IGNORE` |
| **Primitive Obsession** | `java:S107` (Methods should not have too many parameters) | `DLS_DEAD_LOCAL_STORE` |
| **Hardcoded Secrets** | `java:S2068` (Hard-coded credentials should not be used) | `HARD_CODE_PASSWORD` |
| **Double-Checked Locking Bug** | `java:S2168` (Double-checked locking should not be used) | `DC_DOUBLECHECK` |
| **God Object** | `java:S138` (Methods should not have too many lines of code) | `CYCLOMATIC_COMPLEXITY` |
| **Improper Equals/HashCode** | `java:S1206` (`equals(Object)` and `hashCode()` should be overridden in pairs) | `HE_EQUALS_USE_HASHCODE` |

---

## 3. Summary & Remediation Checklist

When conducting architectural code reviews, apply this triage checklist:
1. [ ] **Cohesion Check**: Does this class do exactly one thing, or does it manage lifecycle, calculation, formatting, and persistence simultaneously?
2. [ ] **Coupling Check**: Does modifying a field type ripple through 10+ unrelated classes?
3. [ ] **Domain Richness**: Are business rules encapsulated in entities or scattered across procedural services?
4. [ ] **Exception Hygiene**: Are exceptions thrown with domain context, or caught and swallowed silently?
5. [ ] **Resource Safety**: Are all streams, sockets, and database connections managed via `try-with-resources`?

6. [ ] **Concurrency Safety**: Are mutable shared states protected by locks or synchronized primitives?
7. [ ] **Immutability Default**: Are records or unmodifiable collections used by default?
