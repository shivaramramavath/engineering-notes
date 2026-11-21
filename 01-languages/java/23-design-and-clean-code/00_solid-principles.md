# SOLID Principles in Modern Java

> Applying the five foundational object-oriented design principles (SRP, OCP, LSP, ISP, DIP) using modern Java 17/21/25 idioms: Records, Sealed Interfaces, Pattern Matching, and functional composition.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core Engineering / Software Architecture |
| **Prerequisites** | [04_oop](../04-oop/README.md), [12_records](../12-modern-java/00_records.md), [12_sealed-classes](../12-modern-java/01_sealed-classes.md) |
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

### What are the SOLID Principles?
Introduced by Robert C. Martin ("Uncle Bob"), the SOLID principles represent five core tenets of maintainable, flexible, and robust object-oriented software engineering:
1. **S - Single Responsibility Principle (SRP)**: A class should have one, and only one, reason to change.
2. **O - Open/Closed Principle (OCP)**: Software entities should be open for extension, but closed for modification.
3. **L - Liskov Substitution Principle (LSP)**: Subtypes must be substitutable for their base types without altering program correctness.
4. **I - Interface Segregation Principle (ISP)**: Clients should not be forced to depend upon interfaces they do not use.
5. **D - Dependency Inversion Principle (DIP)**: High-level modules should not depend on low-level modules; both should depend on abstractions.

### Modern Evolution with Java 17–25
In early Java (Java 1.0–8), SOLID often led to deep class hierarchies, excessive factory classes, and verbose boilerplate. Modern Java transforms SOLID:
- **SRP**: Enforced via compact immutable **Records** acting as clean data carriers.
- **OCP**: Expressed via **Sealed Interfaces** and exhaustive **Pattern Matching for Switch**.
- **LSP**: Strictly compiler-enforced via algebraic data types and record components.
- **ISP**: Replaced fat interfaces with **Single Abstract Method (SAM) Functional Interfaces** (`Function`, `Consumer`, `Predicate`).
- **DIP**: Decoupled using constructor dependency injection and pluggable functional lambdas.

---

## 2. Core Concepts

### Single Responsibility Principle (SRP)
A class should encapsulate only one cohesive aspect of functionality.
- *Violation*: A `UserService` that validates user passwords, executes SQL queries against PostgreSQL, hashes credentials, and sends welcome emails via SMTP.
- *Solution*: Decompose into `UserValidator`, `UserRepository`, `PasswordHasher`, and `NotificationService`.

### Open/Closed Principle (OCP)
System behavior can be extended without altering existing compiled source code.
- *Modern Approach*: Instead of complex visitor patterns or reflective class loaders, modern Java utilizes **Sealed Interfaces** that define permitted variants, paired with **Pattern Matching Switch Expressions** that guarantee compile-time exhaustiveness.

### Liskov Substitution Principle (LSP)
If $S$ is a subtype of $T$, then objects of type $T$ may be replaced with objects of type $S$ without altering the desirable properties of the program (correctness, invariant maintenance).
- *Classic Violation*: `Square` extending `Rectangle`. Setting the width on a square modifies its height, violating rectangle post-conditions.
- *Modern Approach*: Favor composition over inheritance; make value objects immutable records so invariants cannot be violated after construction.

### Interface Segregation Principle (ISP)
Prefer small, focused, role-based interfaces over monolithic "kitchen sink" interfaces.
- *Example*: Rather than `public interface Worker { void work(); void eat(); void sleep(); }`, separate into `Workable` and `Restable`. Clients implementing automated background bots only implement `Workable`.

### Dependency Inversion Principle (DIP)
- High-level business policies should not depend on low-level infrastructure details (SQL, HTTP, AWS SDK).
- Both should depend on abstractions (Java interfaces).
- Concrete infrastructure classes implement those abstractions.

---

## 3. How It Works Internally

### The Inversion of Control Architectural Shift
```
+--------------------------------------------------------------------------+
|                  TRADITIONAL TIGHTLY-COUPLED FLOW                        |
|  [ OrderService ] -----> [ PostgresOrderDao ] -----> [ PostgreSQL DB ]    |
|  (OrderService directly instantiates and depends on PostgresOrderDao)    |
+--------------------------------------------------------------------------+
|                  DEPENDENCY INVERSION PRINCIPLE (DIP)                    |
|  [ OrderService ] -----> [ OrderRepository (Interface) ]                |
|                                    ^                                     |
|                                    | (implements)                        |
|                          [ PostgresOrderRepository ]                     |
|  (High-level business service depends ONLY on domain abstraction)        |
+--------------------------------------------------------------------------+
```

### HotSpot Monomorphic Inlining and Interface Call Optimization
A common concern with DIP and interfaces is dynamic dispatch overhead:
- When an interface has only a single implementing class loaded in the JVM, HotSpot\'s C2 JIT compiler applies **Monomorphic Call Devirtualization**.
- The interface call is converted into a direct native jump and inlined into the caller!
- SOLID abstractions with single implementations incur **zero runtime performance penalty**.

---

## 4. Syntax & Usage

### Modern Open/Closed Principle with Sealed Interfaces (Java 21+)
```java
// Sealed domain boundary: Closed for arbitrary subclassing, open for controlled extension
public sealed interface PaymentMethod 
    permits CreditCardPayment, BankTransferPayment, CryptoPayment {}

public record CreditCardPayment(String cardNumber, String cvv) implements PaymentMethod {}
public record BankTransferPayment(String iban, String swift) implements PaymentMethod {}
public record CryptoPayment(String walletAddress, String network) implements PaymentMethod {}

public class PaymentProcessor {
    // Compiler verifies exhaustive pattern matching without default case!
    public String process(PaymentMethod method) {
        return switch (method) {
            case CreditCardPayment cc -> "Charging credit card: " + cc.cardNumber();
            case BankTransferPayment bt -> "Initiating wire to: " + bt.iban();
            case CryptoPayment crypto -> "Submitting TX to: " + crypto.network();
        };
    }
}
```

---

## 5. Practical Examples

### Example 1: SRP & DIP Refactoring in E-Commerce Checkout
**Goal:** Refactor an all-in-one God Class into clean, single-responsibility services wired via Dependency Inversion.

**Before Refactoring (Vulnerable to changes, hard to test):**
```java
// ANTI-PATTERN: God class violating SRP and DIP
public class OrderManager {
    public void checkout(String orderId, String email, double amount) {
        // 1. Validation logic
        if (amount <= 0) throw new IllegalArgumentException("Invalid amount");

        // 2. Direct database persistence
        try (Connection conn = DriverManager.getConnection("jdbc:postgresql://localhost/db")) {
            conn.createStatement().execute("INSERT INTO orders VALUES ('" + orderId + "', " + amount + ")");
        } catch (SQLException e) { throw new RuntimeException(e); }

        // 3. Payment gateway call
        StripeClient client = new StripeClient("api-key");
        client.charge(amount);

        // 4. SMTP email sending
        SmtpTransport.send(email, "Order confirmed: " + orderId);
    }
}
```

**After Refactoring (Adhering to SOLID):**
```java
import java.util.Objects;

// Domain Record (SRP: Represents immutable order data)
public record Order(String id, String customerEmail, double amount) {
    public Order {
        Objects.requireNonNull(id, "id required");
        Objects.requireNonNull(customerEmail, "email required");
        if (amount <= 0) throw new IllegalArgumentException("Amount must be positive");
    }
}

// Abstractions (DIP: High-level interfaces)
public interface OrderRepository {
    void save(Order order);
}

public interface PaymentGateway {
    void charge(Order order);
}

public interface NotificationService {
    void sendConfirmation(Order order);
}

// High-level Business Service (SRP: Orchestrates checkout only)
public final class CheckoutService {
    private final OrderRepository repository;
    private final PaymentGateway paymentGateway;
    private final NotificationService notifier;

    // DIP: Injected via constructor
    public CheckoutService(OrderRepository repository, PaymentGateway paymentGateway, NotificationService notifier) {
        this.repository = Objects.requireNonNull(repository);
        this.paymentGateway = Objects.requireNonNull(paymentGateway);
        this.notifier = Objects.requireNonNull(notifier);
    }

    public void processCheckout(Order order) {
        paymentGateway.charge(order);
        repository.save(order);
        notifier.sendConfirmation(order);
    }
}
```

### Example 2: Interface Segregation Principle (ISP) with Functional Interfaces
**Goal:** Prevent read-only consumers from depending on mutative methods.

```java
// Monolithic Fat Interface (VIOLATION)
interface HeavyRepository<T> {
    T findById(long id);
    void save(T entity);
    void delete(long id);
    void purgeAll();
}

// Segregated Role Interfaces (ISP COMPLIANT)
@FunctionalInterface
public interface EntityReader<T> {
    T findById(long id);
}

public interface EntityWriter<T> {
    void save(T entity);
    void delete(long id);
}

// A read-only reporting service depends ONLY on EntityReader:
public class ReportingService<T> {
    private final EntityReader<T> reader;

    public ReportingService(EntityReader<T> reader) {
        this.reader = reader;
    }

    public T generateReport(long id) {
        return reader.findById(id); // Cannot accidentally mutate or purge!
    }
}
```

### Example 3: Liskov Substitution Principle (LSP) Compliant Value Hierarchy
**Goal:** Model geometric figures where subtypes honor all parent contracts without unexpected side-effects.

```java
public sealed interface Shape permits Rectangle, Circle {
    double area();
    double perimeter();
}

public record Rectangle(double width, double height) implements Shape {
    public Rectangle {
        if (width <= 0 || height <= 0) throw new IllegalArgumentException("Dimensions must be positive");
    }
    @Override public double area() { return width * height; }
    @Override public double perimeter() { return 2 * (width + height); }
}

public record Circle(double radius) implements Shape {
    public Circle {
        if (radius <= 0) throw new IllegalArgumentException("Radius must be positive");
    }
    @Override public double area() { return Math.PI * radius * radius; }
    @Override public double perimeter() { return 2 * Math.PI * radius; }
}
```

---

## 6. Best Practices

1. **Use Constructor Dependency Injection**: Always declare dependencies as `private final` fields initialized through constructors. Avoid field-level `@Autowired` reflection.
2. **Favor Sealed Interfaces for Domain Polygons**: Use sealed types to enforce compiler-checked closed sets of polymorphism.
3. **Keep Interfaces Focused**: Use `@FunctionalInterface` for single-action abstractions. Never create multi-purpose god interfaces.
4. **Make Value Types Immutable Records**: Prevent LSP violations by eliminating mutating setters on data structures.
5. **Program to Interfaces, Not Implementations**: Accept `List` instead of `ArrayList`, `Map` instead of `HashMap`, `Reader` instead of `FileReader`.
6. **Apply Cohesion Over Splitting**: Do not split classes into absurdly small 1-line classes if they share the exact same reason to change.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Secure Fix |
| :--- | :--- | :--- |
| **Field Injection (`@Autowired private Service s;`)** | Bypasses constructor, prevents immutability, breaks unit testing. | Use explicit constructor injection with `final` fields. |
| **Fat Interfaces with 30 methods** | Forces mock implementations to stub 29 unused methods in tests. | Segregate into role interfaces with 1–3 cohesive methods. |
| **Throwing `UnsupportedOperationException` in sub-types** | Violates Liskov Substitution Principle; callers cannot treat type uniformly. | Split the interface hierarchy so unsupported methods do not exist. |
| **Extending concrete classes instead of composing** | Couples subclass to internal implementation details of parent. | Use composition: contain the object and delegate. |
| **Checking `instanceof` with deep `if-else` chains** | Violates Open/Closed Principle; every new subclass modifies the chain. | Use polymorphism or Java 21 exhaustive sealed switch. |
| **Creating an interface for every single class dogmatically** | Introduces 1-to-1 interface bloat without polymorphism benefits. | Introduce interfaces only when multiple implementations or testing boundaries exist. |

---

## 8. Performance & Security Notes

### JIT Inlining of Interface Calls
A historical myth suggests interfaces degrade Java execution performance.
- In HotSpot, if an interface has 1 active implementation (Monomorphic), C2 inlines the method call directly ($0\text{ overhead}$).
- If it has 2 active implementations (Bimorphic), C2 generates an inline conditional test.
- If it has 3+ implementations (Megamorphic), it performs a virtual table lookup via `itable` ($< 2\text{ nanoseconds}$).
- Clean SOLID abstractions have zero discernible performance penalty in real-world enterprise applications.

---

## 9. Comparison / When to Use What

| Principle | Primary Problem Solved | Key Java Mechanism |
| :--- | :--- | :--- |
| **Single Responsibility (SRP)** | High coupling, monster classes | Small cohesive classes, Records |
| **Open/Closed (OCP)** | Breaking existing code during new features | Sealed Interfaces, Pattern Matching, Strategy Pattern |
| **Liskov Substitution (LSP)** | Runtime surprises, broken inheritance | Composition, Immutability, Formal Invariants |
| **Interface Segregation (ISP)**| Fat bloated contracts, unnecessary mocks | Functional Interfaces, Role Interfaces |
| **Dependency Inversion (DIP)** | Direct coupling to volatile infrastructure | Interfaces, Constructor Injection |

---

## 10. Interview Questions & Answers

### Q1: What is the Liskov Substitution Principle and how is it commonly violated in Java?
<details>
<summary>View Answer</summary>
The Liskov Substitution Principle states that objects of a superclass should be replaceable with objects of a subclass without breaking application correctness or violating contracts. A common violation occurs when a subclass overrides a superclass method and throws `UnsupportedOperationException` (e.g. `java.util.Collections.unmodifiableList().add()`). Another classic violation is the Rectangle-Square problem, where mutating square dimensions breaks the independent width/height invariant assumed by code written against Rectangle.
</details>

### Q2: How do modern Sealed Interfaces in Java 17+ support the Open/Closed Principle?
<details>
<summary>View Answer</summary>
The Open/Closed Principle requires code to be open for extension but closed for modification. Sealed interfaces (`sealed interface Event permits LoginEvent, LogoutEvent`) explicitly declare all permitted implementations. When consuming sealed types in a `switch` expression with pattern matching, the compiler enforces exhaustiveness. If a developer adds a new permitted record (e.g. `PasswordResetEvent`), the compiler immediately flags all switch expressions that must handle the new case, preventing silent unhandled runtime branch bugs.
</details>

### Q3: Why is Constructor Injection preferred over Field Injection in enterprise Java?
<details>
<summary>View Answer</summary>
(1) Immutability: Dependencies can be declared `private final`, ensuring they are non-null and cannot be reassigned after construction. (2) Testability: Classes can be instantiated directly in unit tests without starting heavy Spring or CDI reflection containers. (3) Fail-Fast: Instantiating a class without its mandatory dependencies fails immediately at compile time or initialization time with a `NullPointerException`. (4) Circular Dependency Detection: Circular constructor dependencies are detected immediately at startup.
</details>

### Q4: What is the difference between the Single Responsibility Principle and "Do only one thing"?
<details>
<summary>View Answer</summary>
"Do only one thing" is a guideline for methods (functions should execute a single clear algorithm). The Single Responsibility Principle applies to classes and modules, defining responsibility in terms of actors and change: "A module should be responsible to one, and only one, actor or business stakeholder." A class can contain multiple related methods as long as all those methods serve the exact same business reason to change.
</details>

### Q5: How does the Interface Segregation Principle improve unit testing?
<details>
<summary>View Answer</summary>
When a class depends on a narrow, segregated interface (e.g. containing only 1 or 2 methods), writing mock objects or stubs in unit tests is trivial. In contrast, if a class depends on a bloated interface with 25 methods, developers must either configure extensive mocking frameworks (Mockito) or implement dummy stubs for dozens of irrelevant methods.
</details>

### Q6: What is the Dependency Inversion Principle, and how does it relate to Inversion of Control (IoC)?
<details>
<summary>View Answer</summary>
Dependency Inversion Principle (DIP) is a high-level design principle stating that high-level modules should depend on abstractions rather than low-level concrete implementations. Inversion of Control (IoC) is a broader architectural pattern (and framework mechanism) where the control flow of a program is inverted: instead of the application code creating dependencies and controlling the lifecycle, an external container (like Spring or Guice) injects dependencies and manages lifecycles.
</details>

### Q7: Why does composition often triumph over inheritance in modern Java?
<details>
<summary>View Answer</summary>
Inheritance breaks encapsulation because a subclass is tightly coupled to the internal implementation details of its superclass (the fragile base class problem). If the superclass changes how internal methods call each other, subclasses can break unexpectedly. Composition treats the dependency as a black-box component, interacting solely through its public interface, providing loose coupling and runtime flexibility.
</details>

### Q8: What is an example of an LSP violation in the Java Standard Library?
<details>
<summary>View Answer</summary>
`java.util.Collections.unmodifiableList(List)` implements `java.util.List`. The contract of `List.add()` specifies adding an element to the collection. However, calling `add()` on an unmodifiable list throws an `UnsupportedOperationException` at runtime. Client code expecting a standard `List` cannot substitute an unmodifiable list without risking unexpected runtime exceptions.
</details>

---

## 11. Practice Exercises

### Exercise 1: Refactor Monolithic Data Service
**Task:** Given a bloated `CustomerManager` that handles database SQL, email sending, and credit check logic, decompose it into 3 SRP-compliant classes using records and constructor injection.
<details>
<summary>View Solution</summary>

```java
public record Customer(long id, String email, int creditScore) {}

public interface CustomerDao {
    void persist(Customer c);
}

public interface CreditAgency {
    boolean verifyScore(Customer c);
}

public interface EmailClient {
    void sendWelcome(Customer c);
}

public class CustomerRegistrationService {
    private final CustomerDao dao;
    private final CreditAgency creditAgency;
    private final EmailClient emailClient;

    public CustomerRegistrationService(CustomerDao dao, CreditAgency creditAgency, EmailClient emailClient) {
        this.dao = dao;
        this.creditAgency = creditAgency;
        this.emailClient = emailClient;
    }

    public boolean register(Customer customer) {
        if (!creditAgency.verifyScore(customer)) {
            return false;
        }
        dao.persist(customer);
        emailClient.sendWelcome(customer);
        return true;
    }
}
```
</details>

### Exercise 2: Sealed Notification Strategy
**Task:** Implement an Open/Closed compliant notification router using a sealed interface and pattern-matching switch for Email, SMS, and Webhook.
<details>
<summary>View Solution</summary>

```java
public sealed interface Notification permits EmailAlert, SmsAlert, WebhookAlert {}

public record EmailAlert(String address, String subject) implements Notification {}
public record SmsAlert(String phoneNumber, String message) implements Notification {}
public record WebhookAlert(String url, byte[] payload) implements Notification {}

public class NotificationDispatcher {
    public static void dispatch(Notification n) {
        switch (n) {
            case EmailAlert e -> System.out.println("Emailing: " + e.address());
            case SmsAlert s -> System.out.println("SMS to: " + s.phoneNumber());
            case WebhookAlert w -> System.out.println("POST to webhook: " + w.url());
        }
    }
}
```
</details>

### Exercise 3: Segregated Audit Log Stream
**Task:** Create two segregated interfaces: `LogReader` and `LogAppender`. Ensure write-only logging proxies cannot inspect prior logs.
<details>
<summary>View Solution</summary>

```java
import java.util.List;

public interface LogReader {
    List<String> readRecent(int count);
}

@FunctionalInterface
public interface LogAppender {
    void append(String entry);
}

public class AuditService {
    private final LogAppender appender;

    public AuditService(LogAppender appender) {
        this.appender = appender;
    }

    public void record(String action) {
        appender.append("ACTION: " + action);
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **S (Single Responsibility)**: One reason to change. Separate domain, I/O, and business rules.
- **O (Open/Closed)**: Open for extension, closed for modification. Use Sealed Interfaces and Pattern Matching.
- **L (Liskov Substitution)**: Subtypes must fulfill base contracts. Avoid throwing `UnsupportedOperationException`.
- **I (Interface Segregation)**: Small, role-specific interfaces. Use functional interfaces.
- **D (Dependency Inversion)**: High-level modules depend on abstractions. Use Constructor Injection with `final` fields.
- **Composition**: Favor composition with records over deep class inheritance hierarchies.

---

## 13. Further Reading
- Clean Architecture: [A Craftsman's Guide to Software Structure and Design](https://www.oreilly.com/library/view/clean-architecture-a/9780134494272/)
- OpenJDK JEP 409: [Sealed Classes](https://openjdk.org/jeps/409)
- OpenJDK JEP 440: [Record Patterns](https://openjdk.org/jeps/440)
- Martin Fowler: [Inversion of Control Containers and the Dependency Injection Pattern](https://martinfowler.com/articles/injection.html)
