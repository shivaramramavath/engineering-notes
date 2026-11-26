# Dependency Inversion Principle (DIP)

> **One-line summary:** High-level policy modules should not depend on low-level detail modules; both must depend on abstractions. Abstractions must not depend on details; details must depend on abstractions.

| Attribute | Specification |
|---|---|
| **Level** | Core / Architectural |
| **Prerequisites** | [01-oop-pillars.md](../01-foundations/01-oop-pillars.md), [04-programming-to-interfaces.md](../01-foundations/04-programming-to-interfaces.md), [05-abstraction-and-information-hiding.md](../01-foundations/05-abstraction-and-information-hiding.md) |
| **Estimated Reading Time** | 35 minutes |
| **Status** | Complete |
| **Category** | SOLID Principles |
| **Also Known As** | Inversion of Control Principle, Ports and Adapters Foundation |
| **Languages Tested** | Java 25 (LTS) / TypeScript 5.8 (Node 24 native) / Python 3.14 |

---

## Table of Contents
- [1 The Problem](#1-the-problem)
- [2 Why It Exists](#2-why-it-exists)
- [3 What It Is](#3-what-it-is)
- [4 The Problem Without It](#4-the-problem-without-it)
- [5 How It Solves It](#5-how-it-solves-it)
- [6 How It Works](#6-how-it-works)
- [7 Real-World Examples](#7-real-world-examples)
- [8 When to Use and When NOT to Use](#8-when-to-use-and-when-not-to-use)
- [9 Variations and Language Idioms](#9-variations-and-language-idioms)
- [10 Relationship to Other Principles and Patterns](#10-relationship-to-other-principles-and-patterns)
- [11 Common Mistakes and Anti-Patterns](#11-common-mistakes-and-anti-patterns)
- [12 Testing and Design Verification](#12-testing-and-design-verification)
- [13 Performance, Concurrency and Security Notes](#13-performance-concurrency-and-security-notes)
- [14 Interview Questions](#14-interview-questions)
- [15 Practice Exercises](#15-practice-exercises)
- [16 Summary / Cheat Sheet](#16-summary--cheat-sheet)
- [17 Further Reading](#17-further-reading)

---

## 1 The Problem

Imagine an engineering team building an enterprise e-commerce order fulfillment platform. The lead developer creates an `OrderService` to implement core checkout logic. Because the application uses MySQL for data storage and SendGrid for email dispatch, the developer writes:

```java
public class OrderService {
    private MySqlDatabase database = new MySqlDatabase();
    private SmtpMailer mailer = new SmtpMailer();

    public void checkout(Order order) {
        // Business logic...
        database.insertRawOrder(order);
        mailer.sendSmtpEmail(order.getCustomerEmail(), "Order Confirmed");
    }
}
```

Within six months, the engineering organization encounters four crippling roadblocks:
1. **The Testability Wall:** It is physically impossible to unit-test `OrderService.checkout()` without spinning up a live MySQL database daemon and an SMTP mail server. Automated CI builds take 25 minutes, fail unpredictably due to network timeouts, and spam developer inboxes with test emails.
2. **The Cloud Migration Trap:** The executive team mandates migrating from an on-premise MySQL cluster to AWS Aurora PostgreSQL and replacing raw SMTP with Amazon Simple Email Service (SES). Because `OrderService` directly instantiates `MySqlDatabase` and `SmtpMailer`, the core business policy must be ripped open and rewritten, risking regressions in financial transaction accounting.
3. **The Layer Inversion Inversion:** The high-level module (the core business rules of how an order is validated and priced) is subordinate to low-level implementation details (database socket connections, SQL dialects, TCP port 25 protocol packets).
4. **Circular Package Dependencies:** Packaging the domain model into a clean, reusable JAR or npm package is impossible because it drags in MySQL JDBC drivers, TCP socket networking stacks, and email client libraries.

When high-level policy is directly glued to low-level mechanical plumbing, the business logic loses all agility, portability, and testability.

---

## 2 Why It Exists

In traditional procedural software architecture (and naive early OOP), software was structured in a top-down dependency hierarchy:
- High-level modules (UI / Workflow) called Medium-level modules (Business Rules).
- Medium-level modules called Low-level modules (Database, Network, File I/O).

Under this model, **dependencies flowed in the exact same direction as control flow**. As a consequence, whenever a low-level detail changed (e.g. switching database drivers or schema), changes rippled *upward* through the entire hierarchy, forcing high-level business rules to recompile.

In 1996, **Robert C. Martin ("Uncle Bob")** published *The Dependency Inversion Principle*, identifying that object-oriented polymorphism provides a superpower: **the ability to invert the source code dependency graph relative to the runtime control flow**.

By placing an interface boundary between high-level policy and low-level details, the high-level policy defines the interface it needs, and the low-level infrastructure implements that interface. At compile time, the source code dependencies point *inward* toward the high-level domain, freeing business rules from infrastructure volatility.

---

## 3 What It Is

The Dependency Inversion Principle is formally defined by two rules:

1. **High-level modules should not depend on low-level modules. Both should depend on abstractions.**
   - *High-level modules:* Encapsulate core business policy and domain decisions (e.g. `OrderService`, `PricingEngine`).
   - *Low-level modules:* Implement plumbing, mechanical I/O, database access, and network protocols (e.g. `PostgresRepository`, `KafkaProducer`, `AwsS3Client`).
2. **Abstractions should not depend on details. Details should depend on abstractions.**
   - The interface contract must be shaped by what the *caller* needs, not by the idiosyncrasies of the database table or third-party SDK.

```mermaid
graph TD
    subgraph "DIP Violation: Top-Down Coupling"
        High1[OrderService\n(High-Level Domain)] --> LowDB1[MySqlDatabase\n(Low-Level Plumbing)]
        High1 --> LowMail1[SmtpMailer\n(Low-Level Plumbing)]
        Note1[Control flow and source dependencies<br/>point in the same direction]
    end

    subgraph "DIP Compliant: Inverted Dependencies"
        High2[OrderService\n(High-Level Domain)] --> AbsRepo["<<interface>>\nOrderRepository"]
        High2 --> AbsMail["<<interface>>\nNotificationService"]
        
        LowDB2[PostgresOrderRepository] -.->|implements| AbsRepo
        LowMail2[SendGridNotificationService] -.->|implements| AbsMail
        
        Note2[Source code dependencies inverted!<br/>Plumbing points toward Domain abstractions]
    end
```

### The Crucial Triad: DIP vs. DI vs. IoC
Engineers frequently confuse these three concepts:
- **DIP (Dependency Inversion Principle):** The **architectural design principle** stating that high-level modules should own the abstractions they depend on, decoupling them from low-level details.
- **DI (Dependency Injection):** The **design pattern and structural technique** of passing (injecting) concrete dependencies into an object via its constructor, method, or property, rather than letting the object instantiate them internally.
- **IoC (Inversion of Control):** The **architectural runtime mechanism** where the execution framework controls the program's lifecycle and calls your code (the "Hollywood Principle": *Don't call us, we'll call you*). Examples: Spring Framework IoC container, NestJS runtime injector, web framework event loops.

---

## 4 The Problem Without It

Here is the naive implementation violating DIP. Notice how `OrderServiceViolation` hardcodes the creation of `MySqlDatabase` and `SmtpMailer`:

```java
// Naive Implementation violating DIP
class MySqlDatabase {
    public void insert(String sql) {
        System.out.println("Executing raw SQL on MySQL: " + sql);
    }
}

class SmtpMailer {
    public void sendEmail(String to, String msg) {
        System.out.println("Connecting to SMTP port 25 to send email to " + to);
    }
}

public class OrderServiceViolation {
    // Hardcoded low-level dependencies! High-level policy depends directly on plumbing!
    private MySqlDatabase database = new MySqlDatabase();
    private SmtpMailer mailer = new SmtpMailer();

    public void checkout(String orderId, String customerEmail, double total) {
        if (total <= 0) throw new IllegalArgumentException("Total must be positive");
        
        // Direct coupling to SQL syntax
        database.insert("INSERT INTO orders VALUES ('" + orderId + "', " + total + ")");
        
        // Direct coupling to SMTP protocol
        mailer.sendEmail(customerEmail, "Order " + orderId + " confirmed!");
    }
}
```

### The Change Request & Catastrophic Blast Radius
- The infrastructure team migrates to PostgreSQL. Every single line of raw MySQL queries in `OrderServiceViolation.java` must be rewritten and retested.
- The security team blocks outbound port 25 and mandates using SendGrid's HTTPS REST API. The domain checkout class must be edited to import HTTP client libraries.
- Unit testing `checkout()` requires a live MySQL instance and an SMTP relay. When the network glitches, tests fail with false alarms.

### The Failure Output
```text
Executing raw SQL on MySQL: INSERT INTO orders VALUES ('ORD-101', 150.0)
Connecting to SMTP port 25 to send email to alice@example.com
[TEST RUNNER TIMEOUT] Connection refused on localhost:25!
[REGRESSION REPORT] Database schema change in migration v42 broke OrderService checkout!
```

---

## 5 How It Solves It

We invert the dependencies by defining domain abstractions:
1. `OrderRepository`: Defines the capability to persist orders.
2. `NotificationService`: Defines the capability to send notifications.

`OrderService` depends exclusively on these two interfaces. The concrete implementations (`PostgresOrderRepository`, `SendGridNotificationService`) are injected from the outside via constructor injection.

### Java Implementation (Java 25 LTS)

```java
package com.design.solid.dip;

// Abstraction 1: Owned by the Domain Layer
public interface OrderRepository {
    void save(String orderId, double total);
}

// Abstraction 2: Owned by the Domain Layer
public interface NotificationService {
    void notify(String recipient, String message);
}

// Low-Level Detail 1: Implements Domain Contract
public class PostgresOrderRepository implements OrderRepository {
    @Override
    public void save(String orderId, double total) {
        System.out.println("Java: Postgres saved order " + orderId + " ($" + total + ")");
    }
}

// Low-Level Detail 2: Implements Domain Contract
public class SendGridNotificationService implements NotificationService {
    @Override
    public void notify(String recipient, String message) {
        System.out.println("Java: SendGrid API delivered: '" + message + "' to " + recipient);
    }
}

// High-Level Domain Policy: Inverted Dependencies via Constructor Injection
public class OrderService {
    private final OrderRepository repository;
    private final NotificationService notifier;

    public OrderService(OrderRepository repository, NotificationService notifier) {
        this.repository = repository;
        this.notifier = notifier;
    }

    public void checkout(String orderId, String customerEmail, double total) {
        if (total <= 0) {
            throw new IllegalArgumentException("Total must be positive");
        }
        repository.save(orderId, total);
        notifier.notify(customerEmail, "Order " + orderId + " confirmed!");
    }
}

public class DipSolutionApp {
    public static void main(String[] args) {
        // Wiring / Composition Root
        OrderRepository repo = new PostgresOrderRepository();
        NotificationService notifier = new SendGridNotificationService();
        
        OrderService service = new OrderService(repo, notifier);
        service.checkout("ORD-202", "alice@example.com", 299.99);
    }
}
```

### TypeScript Implementation (Node 24 Native)

```typescript
// Domain Abstractions
interface OrderRepository {
    save(orderId: string, total: number): void;
}

interface NotificationService {
    notify(recipient: string, message: string): void;
}

// Infrastructure Details
class PostgresOrderRepository implements OrderRepository {
    save(orderId: string, total: number): void {
        console.log(`TS: Postgres saved order ${orderId} ($${total})`);
    }
}

class SendGridNotificationService implements NotificationService {
    notify(recipient: string, message: string): void {
        console.log(`TS: SendGrid API delivered: '${message}' to ${recipient}`);
    }
}

// High-Level Domain Module
class OrderService {
    private repository: OrderRepository;
    private notifier: NotificationService;

    constructor(repository: OrderRepository, notifier: NotificationService) {
        this.repository = repository;
        this.notifier = notifier;
    }

    checkout(orderId: string, customerEmail: string, total: number): void {
        if (total <= 0) {
            throw new Error("Total must be positive");
        }
        this.repository.save(orderId, total);
        this.notifier.notify(customerEmail, `Order ${orderId} confirmed!`);
    }
}

// Execution / Composition Root
const repo = new PostgresOrderRepository();
const notifier = new SendGridNotificationService();
const service = new OrderService(repo, notifier);
service.checkout("ORD-202", "alice@example.com", 299.99);
```

### Python Implementation (Python 3.14)

```python
from typing import Protocol

# Domain Protocols (Abstractions)
class OrderRepository(Protocol):
    def save(self, order_id: str, total: float) -> None: ...

class NotificationService(Protocol):
    def notify(self, recipient: str, message: str) -> None: ...

# Infrastructure Details
class PostgresOrderRepository:
    def save(self, order_id: str, total: float) -> None:
        print(f"Python: Postgres saved order {order_id} (${total})")

class SendGridNotificationService:
    def notify(self, recipient: str, message: str) -> None:
        print(f"Python: SendGrid API delivered: '{message}' to {recipient}")

# High-Level Policy
class OrderService:
    def __init__(self, repository: OrderRepository, notifier: NotificationService) -> None:
        self._repository = repository
        self._notifier = notifier

    def checkout(self, order_id: str, customer_email: str, total: float) -> None:
        if total <= 0:
            raise ValueError("Total must be positive")
        self._repository.save(order_id, total)
        self._notifier.notify(customer_email, f"Order {order_id} confirmed!")

if __name__ == "__main__":
    repo = PostgresOrderRepository()
    notifier = SendGridNotificationService()
    service = OrderService(repo, notifier)
    service.checkout("ORD-202", "alice@example.com", 299.99)
```

### Real Execution Output
```text
Java: Postgres saved order ORD-202 ($299.99)
Java: SendGrid API delivered: 'Order ORD-202 confirmed!' to alice@example.com
TS: Postgres saved order ORD-202 ($299.99)
TS: SendGrid API delivered: 'Order ORD-202 confirmed!' to alice@example.com
Python: Postgres saved order ORD-202 ($299.99)
Python: SendGrid API delivered: 'Order ORD-202 confirmed!' to alice@example.com
```

---

## 6 How It Works

### The Composition Root Pattern
When dependencies are inverted, an application's classes never instantiate their own collaborators with `new`. Where, then, does object instantiation happen?

It happens in a single, dedicated location at the application entry point called the **Composition Root**:
- In Java/Spring: The Spring ApplicationContext / `@Configuration` classes.
- In Node/NestJS: The `@Module` injector tree.
- In pure vanilla code: The `main()` method.

```mermaid
graph TD
    subgraph "Composition Root (Application Entry Point)"
        Main[main / IoC Container] --> CreateRepo[Instantiate PostgresOrderRepository]
        Main --> CreateNotifier[Instantiate SendGridNotificationService]
        Main --> CreateService[Instantiate OrderService(repo, notifier)]
    end

    subgraph "Domain Layer"
        CreateService --> Svc[OrderService]
        Svc --> AbsR[<<interface>> OrderRepository]
        Svc --> AbsN[<<interface>> NotificationService]
    end

    subgraph "Infrastructure Layer"
        CreateRepo -.->|implements| AbsR
        CreateNotifier -.->|implements| AbsN
    end
```

### Clean / Hexagonal Architecture (Ports and Adapters)
DIP is the mathematical engine of **Hexagonal Architecture** (Alistair Cockburn) and **Clean Architecture** (Robert C. Martin):
- The **Domain** defines **Ports** (Interfaces like `OrderRepository`, `PaymentGateway`).
- The **Infrastructure** provides **Adapters** (Classes like `PostgresAdapter`, `StripeAdapter`).
Because dependencies point inward, you can replace the entire database, web framework, or messaging bus without changing a single line of business logic.

---

## 7 Real-World Examples

### 1. Enterprise Framework: Spring Boot Dependency Injection
In Spring Boot, controllers and service layers never call `new JdbcTemplate()`. Instead, Spring automatically injects data repositories and message brokers:
```java
@Service
public class OrderService {
    private final OrderRepository repository;

    @Autowired // Constructor injection
    public OrderService(OrderRepository repository) {
        this.repository = repository;
    }
}
```
This enables swapping from an H2 in-memory test database in `@DataJpaTest` to an enterprise Oracle or PostgreSQL database in production with zero code changes.

### 2. Standard Library: JDBC `java.sql.Connection` & Python DB-API
The Java Database Connectivity (JDBC) API is pure DIP:
- High-level business code depends on `java.sql.Connection`, `PreparedStatement`, and `ResultSet`.
- Database vendors (Oracle, PostgreSQL, MySQL) implement these interfaces in their respective JAR files.
Your application code does not import Oracle internal drivers; it imports standard `java.sql` abstractions.

### 3. Cloud / Distributed: Multi-Cloud Object Storage
A cloud storage abstraction (`BlobStorageGateway`) defines `upload(String key, byte[] content)`. The application domain interacts only with this gateway. The production deployment binds this to `AwsS3BlobStorage`, while local developer testing binds to `LocalFileBlobStorage`, and government deployments bind to `AzureBlobStorage`.

---

## 8 When to Use and When NOT to Use

| Context | Action | Rationale |
|---|---|---|
| **Core Domain Business Rules** | **Mandatory DIP** | Protect high-value intellectual property from infrastructure churn. |
| **Volatile I/O (Database, Network, Third-Party APIs)** | **Mandatory DIP** | Enables isolated unit testing, caching, and vendor swapping. |
| **Stable Value Objects & Data Structures** | **Do NOT invert** | Do not create an interface for `String`, `UUID`, `BigDecimal`, or `Money`. They are immutable and stable. |
| **Deterministic Standard Utilities** (e.g. `Math.max`) | **Call directly** | Inverting `Math.max()` or `Arrays.sort()` is absurd over-engineering. |

### Diagnostic Signs of DIP Violations
- Domain classes containing imports of database drivers (`java.sql`, `pg`, `psycopg2`), HTTP clients, or file system libraries.
- The keyword `new` used inside business logic to instantiate services, repositories, or external clients.
- Inability to test a business method without booting a database or network daemon.
- Tight coupling to vendor-specific SDK classes (e.g., `AmazonS3Client` directly referenced in domain services).

---

## 9 Variations and Language Idioms

### Java: Constructor Injection vs Field Injection
Field injection (`@Autowired private OrderRepository repo;`) is widely considered an anti-pattern because it hides dependencies and makes testing without reflection difficult. **Constructor injection** is the modern gold standard:
- Dependencies are explicitly declared.
- Fields can be marked `private final`, ensuring immutability.
- Unit tests can instantiate the service using plain `new` without any framework magic.

### TypeScript: InversifyJS vs Functional Composition
In TypeScript, DIP can be achieved with enterprise IoC containers like **InversifyJS** or **NestJS**, or functionally via Currying and Higher-Order Functions:
```typescript
type SaveOrder = (id: string, total: number) => Promise<void>;

// High-level policy receives repository function as parameter
const checkout = (save: SaveOrder) => async (id: string, total: number) => {
    await save(id, total);
};
```

### Python: Structural Protocols & Dependency Injector
Python 3.8+ `typing.Protocol` allows structural DIP without subclassing. For runtime DI wiring, libraries like `dependency-injector` or framework built-ins (FastAPI's `Depends`) provide clean dependency resolution.

---

## 10 Relationship to Other Principles and Patterns

- **DIP Enables OCP:** You cannot make a system open for extension and closed for modification if high-level modules are hardwired to concrete classes. DIP provides the interfaces that make OCP possible.
- **Factory Pattern:** When high-level code needs to create instances at runtime, it calls an abstract Factory interface (DIP), preventing direct `new` coupling.
- **Strategy Pattern:** The Strategy pattern is DIP applied to algorithmic choices.
- **Hexagonal Architecture:** The direct architectural embodiment of DIP.

---

## 11 Common Mistakes and Anti-Patterns

| Anti-Pattern | Description | Production Consequence | Correct Remedy |
|---|---|---|---|
| **Interface Ownership Inversion** | Low-level module defines the interface, and high-level module imports it (`import com.db.MySqlRepoInterface`). | Dependencies still point toward the low-level database! Defeats DIP. | Move interface into the Domain package; low-level package imports Domain. |
| **Service Locator Pattern** | Replacing direct `new` with a global registry query (`ServiceLocator.get(OrderRepo.class)`). | Hides dependencies, creates hidden global state, causes runtime lookup crashes. | Use explicit Constructor Injection. |
| **Field Injection Addiction** | Using `@Autowired` on private fields without constructors. | Impossible to instantiate in plain unit tests without reflection or Spring test runner. | Use Constructor Injection exclusively. |
| **Inverting Value Objects** | Creating interfaces for simple data structures (`interface IOrderData`). | Speculative bloat with zero polymorphism benefit. | Keep Value Objects and DTOs as direct concrete records or dataclasses. |
| **Leaky Infrastructure Abstraction** | Interface methods include database-specific exceptions (`void save() throws SQLException`). | Couples caller to JDBC even though an interface was used! | Wrap low-level exceptions in domain-specific exceptions (`RepositoryException`). |
| **God Container Configuration** | Registering 500 beans in a single untyped configuration file. | Slow startup, silent missing bean runtime crashes. | Modularize configuration by bounded context or feature module. |

---

## 12 Testing and Design Verification

With DIP, unit testing becomes effortless and lightning-fast. We replace heavy database drivers with an in-memory **Fake** or mock that runs in microseconds:

```java
class OrderServiceTest {
    // In-memory Test Double
    static class FakeOrderRepository implements OrderRepository {
        final Map<String, Double> database = new HashMap<>();

        @Override
        public void save(String orderId, double total) {
            database.put(orderId, total);
        }
    }

    static class FakeNotificationService implements NotificationService {
        String lastRecipient;
        String lastMessage;

        @Override
        public void notify(String recipient, String message) {
            this.lastRecipient = recipient;
            this.lastMessage = message;
        }
    }

    @Test
    void shouldSuccessfullyCheckoutOrder() {
        FakeOrderRepository fakeRepo = new FakeOrderRepository();
        FakeNotificationService fakeNotifier = new FakeNotificationService();

        OrderService service = new OrderService(fakeRepo, fakeNotifier);
        service.checkout("ORD-999", "tester@domain.com", 75.0);

        // Verification
        assertEquals(75.0, fakeRepo.database.get("ORD-999"));
        assertEquals("tester@domain.com", fakeNotifier.lastRecipient);
    }
}
```
**Zero testcontainers, zero network calls, zero flakiness.** The entire test suite completes in under 2 milliseconds.

---

## 13 Performance, Concurrency and Security Notes

### Performance
- **Interface Indirection:** Calling methods via interfaces introduces an indirect pointer lookup. Modern JVMs and V8 optimize monomorphic interface call sites into direct CPU jumps or inlined code. The overhead is imperceptible (< 1ns).
- **Startup Time vs Runtime Speed:** IoC containers (like Spring or NestJS) perform reflection and graph resolution during application startup, which can increase boot time (e.g. in serverless functions). Modern solutions like GraalVM native images and compile-time DI (e.g. Dagger, Micronaut) resolve the dependency graph at build time with zero startup reflection penalty.

### Concurrency
- Injected service dependencies should almost always be **stateless singletons**. If `PostgresOrderRepository` holds no mutable instance state, a single instance can be safely shared across thousands of concurrent worker threads.

### Security: Isolation of Secrets
- By separating configuration and infrastructure from domain logic, sensitive database credentials and API tokens remain confined to the Composition Root / Infrastructure layer. The domain code has zero access to connection strings or secret vaults, reducing the attack surface.

---

## 14 Interview Questions

<details>
<summary><strong>1. What is the fundamental difference between Dependency Inversion (DIP), Dependency Injection (DI), and Inversion of Control (IoC)?</strong></summary>

- **DIP** is an **architectural principle**: High-level modules should not depend on low-level modules; both should depend on abstractions owned by the high-level policy.
- **DI** is a **creational design pattern**: A concrete technique for passing dependencies into a class (e.g. via constructor injection) instead of letting the class instantiate them.
- **IoC** is a **runtime framework mechanism**: Inverting the flow of control where an external framework manages the application lifecycle and invokes your code (the Hollywood Principle).
</details>

<details>
<summary><strong>2. Who should own the interface in DIP: the high-level module or the low-level module?</strong></summary>

The **high-level module** must own the interface. If the low-level module defines the interface and the high-level module imports it, the dependency still points toward the low-level detail, completely defeating DIP. The interface represents the *needs of the consumer*, not the capabilities of the provider.
</details>

<details>
<summary><strong>3. Why is constructor injection preferred over field injection in modern frameworks?</strong></summary>

1. **Testability:** Constructor injection allows classes to be tested in plain unit tests using `new` without needing a Spring container or reflection hacks.
2. **Immutability:** Dependencies can be marked `final` / `readonly`, ensuring they are never mutated after construction.
3. **Fail-Fast:** Omitting a required dependency causes a compile error or immediate instantiation failure, rather than a hidden `NullPointerException` at runtime.
</details>

<details>
<summary><strong>4. What is a "Composition Root" and where should it reside?</strong></summary>

A Composition Root is a single, centralized location in the application entry point (e.g. `main()`, `ApplicationContext`, or root module) where the dependency graph is composed and instantiated. It resides in the outermost infrastructure layer, ensuring that no domain classes are polluted with wiring code.
</details>

<details>
<summary><strong>5. How does DIP enable Hexagonal Architecture (Ports and Adapters)?</strong></summary>

Hexagonal Architecture places the domain logic at the center, isolated from the outside world. The domain defines "Ports" (interfaces). External infrastructure (database, message broker, web controllers) provides "Adapters" that implement or call those ports. DIP ensures that all source code dependencies point inward toward the core domain.
</details>

<details>
<summary><strong>6. Why is the Service Locator pattern often criticized as an anti-pattern compared to DI?</strong></summary>

Service Locator hides class dependencies behind a global registry. Looking at a class constructor does not reveal what it needs to run, turning compile-time errors into runtime exceptions if a service is missing. It also introduces global state and couples classes to the locator framework.
</details>

<details>
<summary><strong>7. When should you NOT apply DIP?</strong></summary>

Do not apply DIP to stable, deterministic standard library classes or immutable value objects (e.g. `String`, `Math`, `UUID`, `BigDecimal`, `LocalDate`). Inverting dependencies on stable data structures creates unnecessary interfaces with zero variation or testing benefit.
</details>

<details>
<summary><strong>8. What is a "Leaky Abstraction" in the context of DIP?</strong></summary>

A leaky abstraction occurs when low-level implementation details bleed into the interface contract. For example, declaring `void save(Order o) throws SQLException` leaks the fact that a relational database is used, forcing high-level callers to catch JDBC exceptions even though an interface was used.
</details>

---

## 15 Practice Exercises

### Exercise 1: Spot the DIP Violation (Easy)
Identify the DIP violations in this authentication service:
```java
public class AuthService {
    private BCryptPasswordEncoder encoder = new BCryptPasswordEncoder();
    private RedisSessionStore sessionStore = new RedisSessionStore("localhost", 6379);

    public boolean login(String username, String password) {
        // ...
    }
}
```

<details>
<summary><strong>Solution</strong></summary>

`AuthService` directly instantiates concrete infrastructure classes (`BCryptPasswordEncoder` and `RedisSessionStore`) with `new` and hardcodes connection details (`localhost`, `6379`). It cannot be tested without running Redis, and cannot switch hashing algorithms.
**Fix:** Define `PasswordEncoder` and `SessionStore` interfaces. Inject them into `AuthService` via constructor injection.
</details>

---

### Exercise 2: Refactor Notification Dispatcher (Medium)
Refactor this TypeScript notification service to obey DIP:

```typescript
import { TwilioSmsClient } from "twilio";

class AlertDispatcher {
    private twilio = new TwilioSmsClient(process.env.TWILIO_KEY!);

    sendUrgentAlert(phone: string, alertText: string): void {
        this.twilio.send({ to: phone, body: alertText });
    }
}
```

<details>
<summary><strong>Solution</strong></summary>

```typescript
// 1. Domain Abstraction
interface SmsGateway {
    sendSms(to: string, message: string): Promise<void>;
}

// 2. High-Level Domain Policy
class AlertDispatcher {
    private gateway: SmsGateway;

    constructor(gateway: SmsGateway) {
        this.gateway = gateway;
    }

    async sendUrgentAlert(phone: string, alertText: string): Promise<void> {
        await this.gateway.sendSms(phone, alertText);
    }
}

// 3. Infrastructure Adapter
class TwilioSmsAdapter implements SmsGateway {
    private client: TwilioSmsClient;
    constructor(apiKey: string) {
        this.client = new TwilioSmsClient(apiKey);
    }
    async sendSms(to: string, message: string): Promise<void> {
        await this.client.send({ to, body: message });
    }
}
```
</details>

---

### Exercise 3: Hexagonal Architecture in Python (Hard)
Design a complete Ports and Adapters structure in Python for a book lending library:
1. Define a domain entity `Book` and a port `BookRepository` (protocol).
2. Implement an in-memory test adapter `InMemoryBookRepository`.
3. Implement a domain service `LendingService` that checks out books.
4. Demonstrate wiring in the composition root and verifying via test.

<details>
<summary><strong>Solution</strong></summary>

```python
from typing import Protocol, Optional
from dataclasses import dataclass

@dataclass
class Book:
    isbn: str
    title: str
    is_lent: bool = False

# Port (Owned by Domain)
class BookRepository(Protocol):
    def get_by_isbn(self, isbn: str) -> Optional[Book]: ...
    def save(self, book: Book) -> None: ...

# High-Level Domain Policy
class LendingService:
    def __init__(self, repo: BookRepository) -> None:
        self._repo = repo

    def checkout_book(self, isbn: str) -> bool:
        book = self._repo.get_by_isbn(isbn)
        if not book or book.is_lent:
            return False
        book.is_lent = True
        self._repo.save(book)
        return True

# Adapter (Infrastructure / Test Double)
class InMemoryBookRepository:
    def __init__(self) -> None:
        self.storage: dict[str, Book] = {}

    def get_by_isbn(self, isbn: str) -> Optional[Book]:
        return self.storage.get(isbn)

    def save(self, book: Book) -> None:
        self.storage[book.isbn] = book

# Test
repo = InMemoryBookRepository()
repo.save(Book(isbn="978-0134494166", title="Clean Architecture"))
service = LendingService(repo)

assert service.checkout_book("978-0134494166") is True
assert service.checkout_book("978-0134494166") is False # Already lent!
```
</details>

---

## 16 Summary / Cheat Sheet

```
+--------------------------------------------------------------------------+
|                       DEPENDENCY INVERSION PRINCIPLE                     |
+--------------------------------------------------------------------------+
| RULE 1         : High-level policy must not depend on low-level details; |
|                  both must depend on abstractions.                       |
| RULE 2         : Abstractions must not depend on details; details must   |
|                  depend on abstractions.                                 |
| CORE MECHANISM : Invert source-code dependencies using interfaces so     |
|                  infrastructure points inward toward the domain.         |
| IMPLEMENTATION : Constructor injection via a Composition Root.          |
| DANGER SIGNS   : Hardcoded 'new' on services/repos, database imports in  |
|                  domain, impossible unit tests without Docker/network.   |
| GOLDEN ADVICE  : The domain OWNS the interfaces it depends upon.         |
+--------------------------------------------------------------------------+
```

---

## 17 Further Reading

- Robert C. Martin, *The Dependency Inversion Principle*, C++ Report, May 1996.
- Martin Fowler, *Inversion of Control Containers and the Dependency Injection Pattern*, martinfowler.com/articles/injection.html, 2004.
- Alistair Cockburn, *Hexagonal Architecture (Ports and Adapters)*, alistair.cockburn.us/hexagonal-architecture/, 2005.

### In Your Language
| Language Track | Deep Dive Reference |
|---|---|
| Java | [Java OOP & Design Patterns](file:///e:/Learning/engineering-notes/01-languages/java/24-design-patterns/README.md) |
| TypeScript | [TypeScript Design Patterns](file:///e:/Learning/engineering-notes/01-languages/typescript/17-design-patterns/README.md) |
| Python | [Python Design Patterns](file:///e:/Learning/engineering-notes/01-languages/python/23-design-patterns/README.md) |

---

[← Previous: 04 - Interface Segregation Principle](file:///e:/Learning/engineering-notes/08-solid-design-patterns/02-solid-principles/04-interface-segregation.md) | [Module 02 Index](file:///e:/Learning/engineering-notes/08-solid-design-patterns/02-solid-principles/README.md) | [Next: 06 - SOLID in Practice →](file:///e:/Learning/engineering-notes/08-solid-design-patterns/02-solid-principles/06-solid-in-practice.md)
