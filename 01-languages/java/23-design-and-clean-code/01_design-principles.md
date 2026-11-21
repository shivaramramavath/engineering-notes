# Core Design Principles: KISS, YAGNI, DRY, and Demeter

> Foundational pragmatic software engineering guidelines in modern Java: KISS (Keep It Simple, Stupid), YAGNI (You Aren't Gonna Need It), DRY (Don't Repeat Yourself), Composition over Inheritance, and the Law of Demeter.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core Engineering / Software Craftsmanship |
| **Prerequisites** | [00_solid-principles](00_solid-principles.md), [04_oop](../04-oop/README.md) |
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

### What are Pragmatic Design Principles?
While the SOLID principles provide architectural structure for object-oriented designs, pragmatic principles govern **daily coding decisions and complexity management**:
- **KISS (Keep It Simple, Stupid)**: Systems operate best when kept simple rather than made complex; simplicity should be a key goal in design.
- **YAGNI (You Aren\'t Gonna Need It)**: Do not implement functionality until it is actually needed by current requirements.
- **DRY (Don\'t Repeat Yourself)**: Every piece of knowledge must have a single, unambiguous, authoritative representation within a system.
- **Composition over Inheritance**: Assemble behavior dynamically through contained objects rather than static inheritance hierarchies.
- **Law of Demeter (LoD / Principle of Least Knowledge)**: A method should only speak to its immediate friends, never to strangers (avoid `a.getB().getC().getD().doSomething()`).

### The Curse of Over-Engineering in Enterprise Java
Historically, enterprise Java suffered from extreme over-engineering:
- Factories of Factories (`AbstractBeanFactoryDelegatingFactoryProvider`)
- Premature abstraction for features that were never built
- Massive inheritance trees 7 levels deep
- Fragile coupling spanning complex object graphs
Modern Java 17–25 emphasizes clarity, immutability, compact records, and direct solutions over premature architectural abstraction.

---

## 2. Core Concepts

### KISS: Keep It Simple, Stupid
- Avoid clever tricks, obscure bitwise hacks, or deeply nested generic type declarations when straightforward loops or standard library calls suffice.
- If junior engineers cannot understand the method in 60 seconds, it violates KISS.

### YAGNI: You Aren\'t Gonna Need It
- Resist building complex plugin architectures, multi-tenant databases, or dynamic rule engines for simple CRUD features on the off chance they "might be needed next year".
- Build for today\'s proven requirements with clean boundaries; refactor when new requirements actually emerge.

### DRY vs. Incidental Duplication
- **True Duplication**: The same core business logic, validation rule, or financial calculation is duplicated in multiple files. When the rule changes, developers miss one copy, causing subtle production bugs.
- **Incidental Duplication**: Two classes happen to share identical fields or structure (e.g. `UserEntity` vs. `UserRegistrationRequest`), but represent entirely different conceptual domains and change for different reasons. Forcing them to share a common base class creates artificial coupling.

### The Law of Demeter (Principle of Least Knowledge)
A method `m` of class `C` should only invoke methods on:
1. `C` itself.
2. An object passed as an argument to `m`.
3. An object created/instantiated inside `m`.
4. A direct instance variable of `C`.
It should **never** navigate across long chains of method calls (`order.getCustomer().getAddress().getCity().toLowerCase()`).

---

## 3. How It Works Internally

### Train Wrecks and High Coupling
Code that navigates deep object graphs is called a **Train Wreck**:
```java
// TRAIN WRECK: High coupling, violates Law of Demeter
String postalCode = order.getCustomer().getProfile().getBillingAddress().getPostalCode();
```
Internal Architecture Risk:
- `OrderService` is now tightly coupled to the internal structure of 4 distinct classes (`Order`, `Customer`, `Profile`, `BillingAddress`).
- If `Profile` is refactored or made optional, `OrderService` breaks.
- NullPointerExceptions can strike at any link in the navigation chain.

```
+--------------------------------------------------------------------------+
|                  TRAIN WRECK (VIOLATES LAW OF DEMETER)                   |
|  OrderService ----> Order ----> Customer ----> Profile ----> Address     |
|  (OrderService depends transitively on 4 internal classes!)              |
+--------------------------------------------------------------------------+
|                  LAW OF DEMETER (TELL, DON'T ASK)                        |
|  OrderService ----> Order.getBillingPostalCode()                         |
|  (Order encapsulates its own internal structure cleanly)                 |
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### 1. Refactoring Train Wrecks via "Tell, Don\'t Ask"
Instead of asking an object for its internal parts and manipulating them yourself:
```java
// Violates Demeter (Ask, Then Manipulate):
if (account.getBalance().getAmount().compareTo(BigDecimal.ZERO) < 0) {
    account.setStatus(AccountStatus.FROZEN);
}

// Complies with Demeter (Tell the object what to do):
account.freezeIfOverdrawn();
```

---

## 5. Practical Examples

### Example 1: Refactoring Premature Abstraction (Applying KISS & YAGNI)
**Goal:** Eliminate an unnecessary, complex Abstract Factory pattern built for a single implementation.

**Before (Over-engineered Factory Horror):**
```java
// ANTI-PATTERN: Premature abstraction for a feature that only ever exports CSV
public interface DataExporterFactory<T> {
    DataExporter<T> createExporter();
}
public interface DataExporter<T> {
    byte[] export(List<T> data);
}
public class CsvDataExporterFactory implements DataExporterFactory<User> {
    @Override public DataExporter<User> createExporter() {
        return new CsvDataExporter();
    }
}
public class CsvDataExporter implements DataExporter<User> {
    @Override public byte[] export(List<User> data) { /* export csv */ return new byte[0]; }
}
```

**After (KISS & Modern Java):**
```java
import java.util.List;

// Simple, direct, and readable
public class UserCsvExporter {
    public static String toCsv(List<User> users) {
        StringBuilder sb = new StringBuilder("id,name,email\n");
        for (User u : users) {
            sb.append(u.id()).append(",").append(u.name()).append(",").append(u.email()).append("\n");
        }
        return sb.toString();
    }
}
```

### Example 2: Composition over Inheritance with Modern Java Records
**Goal:** Reuse logging and metric capabilities across domain services without extending base classes.

```java
// Reusable metrics capability component (Composition)
public class OperationTimer {
    public <T> T time(String operationName, java.util.function.Supplier<T> action) {
        long start = System.nanoTime();
        try {
            return action.get();
        } finally {
            long durationMs = (System.nanoTime() - start) / 1_000_000;
            System.out.printf("Operation '%s' completed in %d ms%n", operationName, durationMs);
        }
    }
}

// Service composes the timer rather than inheriting from "AbstractTimedService"
public class InventoryService {
    private final OperationTimer timer;

    public InventoryService(OperationTimer timer) {
        this.timer = timer;
    }

    public boolean checkStock(String sku) {
        return timer.time("checkStock", () -> {
            // Actual inventory query...
            return true;
        });
    }
}
```

### Example 3: Law of Demeter in Payment Dispatching
**Goal:** Hide internal wallet structures from checkout processing.

```java
public record Money(java.math.BigDecimal amount) {}

public class Wallet {
    private Money balance;

    public boolean hasSufficientFunds(Money cost) {
        return balance.amount().compareTo(cost.amount()) >= 0;
    }
}

public class Customer {
    private final Wallet wallet;

    public Customer(Wallet wallet) {
        this.wallet = wallet;
    }

    // Demeter Compliant Delegate:
    public boolean canAfford(Money cost) {
        return wallet.hasSufficientFunds(cost); // Talks to immediate friend
    }
}
```

### Example 4: Failure Case — Wrong DRY (Coupling Unrelated Domains)
```java
// ANTI-PATTERN: Forcing identical structure to share an inheritance model
public class BaseEntity {
    protected String name;
    protected String description;
}
// Both inherit, but Customer.name is a human name, Product.name is a SKU title.
// When Product requires a 128-char limit and Customer requires first/last split,
// modifying BaseEntity breaks both systems!
```

---

## 6. Best Practices

1. **Start Simple, Refactor Later**: Implement the simplest thing that could possibly work. Do not create interfaces or factories until at least two distinct implementations exist.
2. **Tell, Don\'t Ask**: Direct objects to perform operations on their own data rather than querying internal components and manipulating them externally.
3. **Favor Pure Functions**: Write static methods for algorithmic transformations without hidden internal state.
4. **Resist Premature Generalization**: Build for the concrete problem in front of you today.
5. **Differentiate Knowledge Duplication vs. Structural Duplication**: Only extract shared abstractions when business rules are genuinely identical and must change together.
6. **Limit Method Chaining to Fluent Builders / Streams**: Method chaining is fine for builders and stream pipelines (`list.stream().filter().map()`), but forbidden for traversing stateful domain entity hierarchies.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Secure Fix |
| :--- | :--- | :--- |
| **Deep Train Wrecks (`a.getB().getC()`)** | Tightly couples caller to internal data structures; fragile to schema changes. | Add delegate methods to immediate classes. |
| **Premature Plugin Architecture (YAGNI)** | Adds hundreds of lines of unused interfaces and complex reflection. | Write simple direct classes; refactor when plugins are required. |
| **Unifying unrelated classes to satisfy DRY** | Binds independent business domains together into artificial base classes. | Allow incidental structural duplication across domain boundaries. |
| **Deep Inheritance Trees ($> 3$ levels)** | Fragile base class problem; changes to ancestor break unrelated descendants. | Replace inheritance with composition and delegation. |
| **Using Clever Bitwise Tricks for Simple Logic** | Sacrifices readability and maintainability for negligible nanosecond gains. | Write clean, obvious, readable Java code. |
| **Confusing Stream pipelines with Demeter violations** | Claiming `stream().filter().map()` violates Demeter. | Fluent pipelines transform streams, not entity graphs; they are Demeter-compliant. |

---

## 8. Performance & Security Notes

### Method Inlining and Delegation
A common fear is that adding delegate methods to satisfy the Law of Demeter (`customer.canAfford()` calling `wallet.hasSufficientFunds()`) increases call stack depth and degrades performance.
- HotSpot C2 compiler aggressively inlines short accessor and delegate methods ($< 35$ bytes of bytecode).
- At runtime, the JIT completely eliminates the delegate method call, executing the final condition directly without any subroutine call overhead.

---

## 9. Comparison / When to Use What

| Principle | Core Directive | Primary Danger if Ignored |
| :--- | :--- | :--- |
| **KISS** | Keep implementations simple and direct | Unmaintainable, unreadable code that team fears modifying |
| **YAGNI** | Do not write code until required | Wasted engineering time, dead code, architectural bloat |
| **DRY** | Single authoritative representation of knowledge | Bug fixes applied to one copy but missed in duplicates |
| **Composition** | Assemble behavior from discrete parts | Fragile base class bugs, rigid hierarchies |
| **Law of Demeter** | Talk only to immediate friends | Cascading null pointer bugs, tight structural coupling |

---

## 10. Interview Questions & Answers

### Q1: What is the Law of Demeter and how does it prevent brittle code?
<details>
<summary>View Answer</summary>
The Law of Demeter (Principle of Least Knowledge) states that a method should only call methods on its owning class, its parameters, objects it instantiates, or its direct instance variables. It prohibits navigating across object graphs (e.g. `order.getCustomer().getAddress().getZipCode()`). It prevents brittle code because the calling class does not depend on the internal structure of intermediate classes. If the customer or address data model changes, only the immediate parent class needs modification rather than every service across the application.
</details>

### Q2: Why is "Composition over Inheritance" an industry best practice?
<details>
<summary>View Answer</summary>
Inheritance creates a tight compile-time coupling between subclass and superclass. Subclasses depend on the internal implementation details of the superclass (the fragile base class problem). Modifying a base class can silently alter the behavior of subclasses. Composition assembles behavior at runtime using interfaces and contained objects. It respects encapsulation, allows components to be swapped or mocked easily in unit tests, and avoids rigid taxonomies.
</details>

### Q3: How do you identify when DRY is being misapplied?
<details>
<summary>View Answer</summary>
DRY is misapplied when developers eliminate "incidental duplication" between unrelated domains. For example, creating a shared base class for an internal database entity and an external REST request DTO simply because they share fields like `name` and `email`. Although structurally similar today, they change for entirely different reasons (database schema migration vs. external API versioning). Forcing them together creates artificial coupling and makes future independent evolution painful.
</details>

### Q4: What does "Tell, Don\'t Ask" mean in practical Java programming?
<details>
<summary>View Answer</summary>
"Tell, Don\'t Ask" means that rather than asking an object for its internal state and making decisions externally, you should tell the object what action to take. For example, instead of asking an `Account` for its balance and setting its status to frozen if negative, you tell the account: `account.freezeIfOverdrawn()`. This keeps business logic co-located with the data it operates on, improving encapsulation and reducing coupling.
</details>

### Q5: Does the Law of Demeter apply to Fluent APIs, Builders, and Java Streams?
<details>
<summary>View Answer</summary>
No. The Law of Demeter applies to navigating structural relationships between stateful domain entities. Fluent builders (`StringBuilder.append().append()`) and Java Streams (`list.stream().filter().map().toList()`) are transforming an existing stream or building a single object. Each method returns either the same object or a standardized wrapper/pipeline, which does not violate the principle.
</details>

### Q6: What is the main risk of ignoring YAGNI?
<details>
<summary>View Answer</summary>
Ignoring YAGNI results in writing, testing, reviewing, and maintaining code for hypothetical future requirements that often never materialize or materialize with completely different constraints than anticipated. This wastes valuable engineering time, increases cognitive load, complicates refactoring, and introduces bugs into code paths that are not actively exercised by production traffic.
</details>

### Q7: How does HotSpot optimize short delegation methods introduced by the Law of Demeter?
<details>
<summary>View Answer</summary>
HotSpot\'s C2 compiler inlines methods with small bytecode sizes (by default, methods with $\le 35$ bytes of bytecode). Simple delegate methods that forward a call to an internal field are trivial candidates for inlining. The JIT compiler replaces the delegate method call with the direct underlying operation at machine-code level, completely eliminating method invocation overhead.
</details>

### Q8: What is the difference between essential complexity and accidental complexity?
<details>
<summary>View Answer</summary>
Essential complexity is inherent in the business problem itself (e.g. tax calculations, cryptographic algorithms, distributed consensus). Accidental complexity is introduced by engineers\' choices of tooling, excessive abstraction, over-engineering, or poor design decisions (e.g. creating 10 layers of factories and proxies for a simple database query). Principles like KISS and YAGNI aim to minimize accidental complexity.
</details>

---

## 11. Practice Exercises

### Exercise 1: Refactor a Demeter Violation
**Task:** Refactor the following snippet so `InvoicePrinter` does not violate the Law of Demeter:
```java
// VIOLATION:
String city = invoice.getCustomer().getContactInfo().getAddress().getCity();
```
<details>
<summary>View Solution</summary>

```java
// Add delegate method to Customer:
public class Customer {
    private ContactInfo contactInfo;
    public String getCity() {
        return contactInfo.getAddress().getCity();
    }
}

// Add delegate method to Invoice:
public class Invoice {
    private Customer customer;
    public String getCustomerCity() {
        return customer.getCity();
    }
}

// InvoicePrinter now speaks only to its immediate friend:
String city = invoice.getCustomerCity();
```
</details>

### Exercise 2: Refactor Inheritance to Composition
**Task:** Given a class `AuditStack extends Stack<String>` that logs pushes and pops, refactor it to use composition with `Deque<String>`.
<details>
<summary>View Solution</summary>

```java
import java.util.ArrayDeque;
import java.util.Deque;

public class AuditStack {
    private final Deque<String> stack = new ArrayDeque<>();

    public void push(String item) {
        System.out.println("Pushing: " + item);
        stack.push(item);
    }

    public String pop() {
        String item = stack.pop();
        System.out.println("Popped: " + item);
        return item;
    }

    public boolean isEmpty() {
        return stack.isEmpty();
    }
}
```
</details>

### Exercise 3: Apply YAGNI to Multi-Database Service
**Task:** A developer built a `DatabaseRouter` supporting 5 database vendors (PostgreSQL, Oracle, MySQL, SQLServer, DB2) for a system that only uses PostgreSQL. Simplify it to a clean single-database connection provider.
<details>
<summary>View Solution</summary>

```java
import javax.sql.DataSource;

// Simple, direct, YAGNI-compliant
public class DatabaseConnectionProvider {
    private final DataSource dataSource;

    public DatabaseConnectionProvider(DataSource dataSource) {
        this.dataSource = dataSource;
    }

    public DataSource getDataSource() {
        return dataSource;
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **KISS**: Write clear, direct code. Avoid unnecessary generic abstractions.
- **YAGNI**: Implement only current requirements; do not anticipate unproven future needs.
- **DRY**: Eliminate duplication of authoritative business knowledge, not accidental structural similarity.
- **Composition**: Assemble behaviors from small components instead of deep inheritance trees.
- **Demeter**: Talk only to immediate friends. Avoid long method chains on domain entities.
- **Tell, Don\'t Ask**: Direct objects to act on their own state rather than manipulating their parts externally.

---

## 13. Further Reading
- The Pragmatic Programmer: [From Journeyman to Master](https://pragprog.com/titles/tpp20/the-pragmatic-programmer-20th-anniversary-edition/)
- Refactoring: [Improving the Design of Existing Code (Martin Fowler)](https://martinfowler.com/books/refactoring.html)
- Law of Demeter: [Ian Holland's Original Demeter Paper](https://www.ccs.neu.edu/research/demeter/)
- Effective Java: [Item 18: Favor Composition Over Inheritance](https://www.oreilly.com/library/view/effective-java-3rd/9780134686097/)
