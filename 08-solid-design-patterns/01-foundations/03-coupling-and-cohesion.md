# Coupling and Cohesion

> **One-line summary:** Coupling measures the degree of interdependence between modules, while cohesion measures how focused a module's internal responsibilities are: the golden architectural directive is to achieve **high cohesion** and **low coupling**.

| Attribute | Specification |
|---|---|
| **Level** | Foundations / Core |
| **Prerequisites** | [01-oop-pillars.md](01-oop-pillars.md), [02-composition-vs-inheritance.md](02-composition-vs-inheritance.md) |
| **Estimated Reading Time** | 30 minutes |
| **Status** | Complete |
| **Category** | Foundations |
| **Also Known As** | Structural Quality Metrics, Afferent/Efferent Coupling |
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

A fast-growing fintech team created a single class called `OrderService`. Over two years, `OrderService` grew to 3,800 lines. Inside its single `processOrder()` method, it:
1. Validates incoming credit card numbers and formats.
2. Directly calls the Stripe REST API via raw HTTP sockets.
3. Opens a direct PostgreSQL transaction to insert into the `orders`, `line_items`, and `audit_log` tables.
4. Generates an HTML invoice string and connects to an external SMTP server to deliver customer receipts.
5. Emits real-time metrics over UDP to a Datadog agent.

When the payments provider updates its API token authentication headers, the developer editing `OrderService` accidentally introduces a syntax error into the SQL insert statement. A simple payment configuration change crashes order persistence in production.

Moreover, writing an automated unit test for `processOrder()` requires:
- A live PostgreSQL database running in docker.
- A mocked HTTP server responding to Stripe endpoints.
- A fake SMTP mail server listening on port 25.

Because testing is agonizingly slow, developers stop writing unit tests entirely. The blast radius of every change encompasses the entire checkout flow.

---

## 2 Why It Exists

In 1974, Larry Constantine and Edward Yourdon formulated the concepts of **Coupling** and **Cohesion** in their seminal work on Structured Design:
- **Coupling**: The strength of the connection between two modules. When module A changes, how likely is module B to break?
- **Cohesion**: The degree to which the elements of a module belong together. Does this module represent a single, focused conceptual boundary?

These two forces dictate the long-term maintainability of every software architecture. High coupling coupled with low cohesion leads directly to the "Big Ball of Mud" anti-pattern.

---

## 3 What It Is

- **Cohesion (Internal Focus)**: An individual module should do one job, do it completely, and contain only the data and methods necessary for that job.
- **Coupling (External Dependency)**: Modules should minimize their knowledge of and dependence on the concrete details of other modules.

```mermaid
graph TD
    subgraph LowCohesion["Low Cohesion & High Coupling (Monolithic God Class)"]
        Monolith[Monolithic Order Service] --> DB[(PostgreSQL)]
        Monolith --> Stripe[Stripe API]
        Monolith --> SMTP[SMTP Server]
        Monolith --> DD[Datadog UDP]
    end
    subgraph HighCohesion["High Cohesion & Low Coupling (Collaborator Network)"]
        Coord[Order Coordinator] --> IPay[PaymentGateway Interface]
        Coord --> IRepo[OrderRepository Interface]
        Coord --> INotif[NotificationService Interface]
        IPay <|.. SGate[Stripe Implementation]
        IRepo <|.. SRepo[SQL Implementation]
        INotif <|.. ENotif[Email Implementation]
    end
```

---

## 4 The Problem Without It

Here is the naive monolithic implementation with low cohesion and tight coupling:

```java
// File: CouplingWithoutApp.java
class MonolithicOrderService {
    public void processOrder(String orderId, String customerEmail, double amount, String rawCardNumber) {
        // Concern 1: Validation
        if (amount <= 0 || !rawCardNumber.startsWith("4")) {
            System.out.println("Validation failed");
            return;
        }

        // Concern 2: External Payment Gateway Network Call
        System.out.println("Connecting to Stripe API for " + rawCardNumber);
        boolean paid = true;

        // Concern 3: Relational Database Persistence
        if (paid) {
            System.out.println("INSERT INTO orders (id, amount) VALUES ('" + orderId + "', " + amount + ")");
        }

        // Concern 4: Network Email Delivery
        System.out.println("Sending SMTP email to " + customerEmail);
        
        System.out.println("Monolith: order processed with tight coupling to 4 concerns.");
    }
}

public class CouplingWithoutApp {
    public static void main(String[] args) {
        MonolithicOrderService svc = new MonolithicOrderService();
        svc.processOrder("ORD-99", "user@test.com", 150.0, "4111111111");
    }
}
```

```text
Connecting to Stripe API for 4111111111
INSERT INTO orders (id, amount) VALUES ('ORD-99', 150.0)
Sending SMTP email to user@test.com
Monolith: order processed with tight coupling to 4 concerns.
```

### The Change Request: Switch from Stripe to Adyen
When management switches providers to save 0.5% in transaction fees:
1. `MonolithicOrderService` must be modified directly.
2. The developer touches the same file containing sensitive database transactions and email logic.
3. Testing the payment change requires verifying database and email functionality from scratch.

---

## 5 How It Solves It

We refactor by establishing **high cohesion** (each class handles one specific technical or business capability) and **low coupling** (the coordinator depends only on abstract interfaces).

### Java Implementation
```java
// File: CouplingWithApp.java
interface PaymentGateway { boolean charge(String card, double amount); }
interface OrderRepository { void save(String id, double amount); }
interface NotificationService { void sendConfirmation(String email, String id); }

class StripeGateway implements PaymentGateway {
    public boolean charge(String card, double amount) {
        System.out.println("StripeGateway: charged $" + amount);
        return true;
    }
}

class SqlOrderRepository implements OrderRepository {
    public void save(String id, double amount) {
        System.out.println("SqlOrderRepository: saved " + id);
    }
}

class EmailNotificationService implements NotificationService {
    public void sendConfirmation(String email, String id) {
        System.out.println("EmailNotificationService: notified " + email);
    }
}

class OrderCoordinator {
    private final PaymentGateway payment;
    private final OrderRepository repository;
    private final NotificationService notifier;

    public OrderCoordinator(PaymentGateway payment, OrderRepository repository, NotificationService notifier) {
        this.payment = payment;
        this.repository = repository;
        this.notifier = notifier;
    }

    public void process(String orderId, String email, double amount, String card) {
        if (!payment.charge(card, amount)) throw new IllegalStateException("Payment failed");
        repository.save(orderId, amount);
        notifier.sendConfirmation(email, orderId);
        System.out.println("Coordinator: order " + orderId + " completed via cohesive collaborators.");
    }
}

public class CouplingWithApp {
    public static void main(String[] args) {
        OrderCoordinator coordinator = new OrderCoordinator(
            new StripeGateway(),
            new SqlOrderRepository(),
            new EmailNotificationService()
        );
        coordinator.process("ORD-99", "user@test.com", 150.0, "4111111111");
    }
}
```

### TypeScript Implementation
```typescript
// File: CouplingWithApp.ts
interface PaymentGateway { charge(card: string, amount: number): boolean; }
interface OrderRepository { save(id: string, amount: number): void; }
interface NotificationService { sendConfirmation(email: string, id: string): void; }

class StripeGateway implements PaymentGateway {
    charge(card: string, amount: number): boolean {
        console.log(`TS StripeGateway: charged $${amount}`);
        return true;
    }
}

class SqlOrderRepository implements OrderRepository {
    save(id: string, amount: number): void {
        console.log(`TS SqlOrderRepository: saved ${id}`);
    }
}

class EmailNotificationService implements NotificationService {
    sendConfirmation(email: string, id: string): void {
        console.log(`TS EmailNotificationService: notified ${email}`);
    }
}

class OrderCoordinator {
    private readonly payment: PaymentGateway;
    private readonly repository: OrderRepository;
    private readonly notifier: NotificationService;

    constructor(
        payment: PaymentGateway,
        repository: OrderRepository,
        notifier: NotificationService
    ) {
        this.payment = payment;
        this.repository = repository;
        this.notifier = notifier;
    }

    process(orderId: string, email: string, amount: number, card: string): void {
        if (!this.payment.charge(card, amount)) throw new Error("Payment failed");
        this.repository.save(orderId, amount);
        this.notifier.sendConfirmation(email, orderId);
        console.log(`TS Coordinator: order ${orderId} completed via cohesive collaborators.`);
    }
}

const coordinator = new OrderCoordinator(
    new StripeGateway(),
    new SqlOrderRepository(),
    new EmailNotificationService()
);
coordinator.process("ORD-99", "user@test.com", 150.0, "4111111111");
```

### Python Implementation
```python
# File: coupling_with_app.py
from typing import Protocol

class PaymentGateway(Protocol):
    def charge(self, card: str, amount: float) -> bool: ...

class OrderRepository(Protocol):
    def save(self, order_id: str, amount: float) -> None: ...

class NotificationService(Protocol):
    def send_confirmation(self, email: str, order_id: str) -> None: ...

class StripeGateway:
    def charge(self, card: str, amount: float) -> bool:
        print(f"Python StripeGateway: charged ${amount}")
        return True

class SqlOrderRepository:
    def save(self, order_id: str, amount: float) -> None:
        print(f"Python SqlOrderRepository: saved {order_id}")

class EmailNotificationService:
    def send_confirmation(self, email: str, order_id: str) -> None:
        print(f"Python EmailNotificationService: notified {email}")

class OrderCoordinator:
    def __init__(self, payment: PaymentGateway, repository: OrderRepository, notifier: NotificationService):
        self._payment = payment
        self._repo = repository
        self._notifier = notifier

    def process(self, order_id: str, email: str, amount: float, card: str) -> None:
        if not self._payment.charge(card, amount):
            raise RuntimeError("Payment failed")
        self._repo.save(order_id, amount)
        self._notifier.send_confirmation(email, order_id)
        print(f"Python Coordinator: order {order_id} completed via cohesive collaborators.")

coordinator = OrderCoordinator(StripeGateway(), SqlOrderRepository(), EmailNotificationService())
coordinator.process("ORD-99", "user@test.com", 150.0, "4111111111")
```

```text
StripeGateway: charged $150.0
SqlOrderRepository: saved ORD-99
EmailNotificationService: notified user@test.com
Coordinator: order ORD-99 completed via cohesive collaborators.
TS StripeGateway: charged $150
TS SqlOrderRepository: saved ORD-99
TS EmailNotificationService: notified user@test.com
TS Coordinator: order ORD-99 completed via cohesive collaborators.
Python StripeGateway: charged $150.0
Python SqlOrderRepository: saved ORD-99
Python EmailNotificationService: notified user@test.com
Python Coordinator: order ORD-99 completed via cohesive collaborators.
```

---

## 6 How It Works

### The Mathematical Metrics of Coupling

Robert C. Martin formalized package coupling metrics that can be applied to modules and classes:
- **Afferent Coupling ($C_a$)**: The number of external classes that depend on this class. High $C_a$ means the class is **responsible**; changing it has a large blast radius.
- **Efferent Coupling ($C_e$)**: The number of external classes that this class depends on. High $C_e$ means the class is **vulnerable** to changes in its dependencies.
- **Instability Metric ($I$)**:
  $$I = \frac{C_e}{C_a + C_e}$$
  - $I = 0$: **Maximally Stable**. Many classes depend on it; it depends on nothing (e.g. `java.lang.String`). Hard to change, but safe to depend on.
  - $I = 1$: **Maximally Unstable**. Depends on many classes; nothing depends on it. Easy to change, but risky to depend on.

### The Spectrum of Cohesion (Best to Worst)

```
[BEST]
  Functional Cohesion:       Every element contributes to a single well-defined task (e.g. calculateTax())
  Sequential Cohesion:       Output of one step is the input to the next (e.g. parseJson -> validate -> transform)
  Communicational Cohesion:  Functions operate on the same data set (e.g. updateRecord, deleteRecord)
  Procedural Cohesion:       Components grouped because they execute in an arbitrary sequence
  Temporal Cohesion:         Operations grouped because they happen at the same time (e.g. startupInitialization())
  Logical Cohesion:          Methods grouped by category (e.g. MathUtils or StringUtils)
  Coincidental Cohesion:     Elements grouped randomly with no meaningful relationship (CommonHelper)
[WORST]
```

---

## 7 Real-World Examples

### 1. Business Domain: Microservices Checkout Architecture
Instead of a single monolithic checkout service, high cohesion dictates:
- `InventoryService`: Tracks stock availability, warehouse reservations.
- `PricingService`: Applies promotion codes, tiered discounts, currency conversion.
- `PaymentService`: Coordinates PSP transactions and tokenized cards.  
Each service can scale independently and uses separate databases.

### 2. Standard Library: Java Collection Framework
`java.util.Collections` maintains loose coupling through interfaces:
- Algorithms (like `Collections.sort()` or `Collections.binarySearch()`) depend only on `List<T>` and `Comparable<T>` / `Comparator<T>`.
- The sorting algorithm has zero coupling to whether the list is an `ArrayList`, `LinkedList`, or custom unmodifiable view.

### 3. Modern Cloud / Infrastructure: Linux Shell Pipelines
The quintessential Unix philosophy:
```bash
cat access.log | grep "500" | cut -d' ' -f1 | sort | uniq -c | sort -nr
```
Each command has maximal functional cohesion (does exactly one job) and minimal coupling (communicates purely over a standard POSIX text stream).

---

## 8 When to Use and When NOT to Use

### Increase Cohesion / Decrease Coupling When:
- Classes exceed 300–500 lines or handle multiple infrastructural concerns.
- Modifying one feature triggers regressions in unrelated features.
- Unit testing requires spinning up databases, networks, or file systems.

### Avoid / Over-Engineering Signals:
- **Micro-Class Explosion**: Splitting a 20-line utility function across 5 separate classes and interfaces just to achieve "pure" single responsibility.
- **Premature Generic Indirection**: Introducing an interface when there is only one implementation and no automated testing requirement for it.

---

## 9 Variations and Language Idioms

- **Go**: Achieves minimal coupling via implicit interface satisfaction. A consumer defines the interface it requires; the producer does not even know the interface exists.
- **Python**: Structural typing with `typing.Protocol` allows duck-typed decoupling with compile-time static type checker support (Mypy).
- **TypeScript**: Structural interfaces allow composing lightweight shapes without runtime overhead.

---

## 10 Relationship to Other Principles and Patterns

- **Single Responsibility Principle (SRP)**: High cohesion applied at the class/module level.
- **Dependency Inversion Principle (DIP)**: The primary technique for achieving low coupling.
- **Facade Pattern**: Wraps a tightly-coupled subsystem to present a loosely-coupled interface to callers.
- **Mediator Pattern**: Reduces coupling between $M \times N$ peer components by introducing a central coordinator.

---

## 11 Common Mistakes and Anti-Patterns

| Mistake | Why It Breaks | Production Fix |
|---|---|---|
| **The God Object** | A single class knows too much and does too much (low cohesion). | Split along domain boundaries into cohesive collaborators. |
| **Feature Envy** | A method in Class A spends more time calling getters on Class B than using its own data. | Move the method into Class B where the data lives. |
| **Shotgun Surgery** | A single logical change forces edits across 10 different files. | Consolidate the scattered logic into one cohesive module. |
| **Inappropriate Intimacy** | Two classes know each other's private fields or internal implementation details. | Introduce an interface or mediator to decouple them. |
| **Utility Dumpster (`CommonUtils`)** | A class holding 50 unrelated static methods (coincidental cohesion). | Group methods into domain-specific, cohesive modules (e.g. `TaxCalculator`, `DateFormatter`). |
| **Tramp Data Coupling** | Passing an entire `User` object through 5 layers just so the deepest layer can read `user.id`. | Pass only the specific primitive or narrow data structure required. |

---

## 12 Testing and Design Verification

Low coupling directly enables sub-millisecond, deterministic unit testing without external infrastructure:

```java
class MockPaymentGateway implements PaymentGateway {
    boolean charged = false;
    public boolean charge(String card, double amount) {
        this.charged = true;
        return true;
    }
}

class OrderCoordinatorTest {
    void testSuccessfulOrderFlow() {
        MockPaymentGateway mockPayment = new MockPaymentGateway();
        OrderRepository mockRepo = (id, amt) -> {};
        NotificationService mockNotif = (email, id) -> {};

        OrderCoordinator coordinator = new OrderCoordinator(mockPayment, mockRepo, mockNotif);
        coordinator.process("ORD-1", "test@test.com", 100.0, "4111");

        assert mockPayment.charged;
    }
}
```

---

## 13 Performance, Concurrency and Security Notes

- **Performance**: Decoupled systems with many small objects have negligible memory allocation overhead on modern generational garbage collectors. High cohesion improves CPU instruction cache locality by keeping related logic grouped.
- **Concurrency**: Highly cohesive, decoupled components with immutable state eliminate shared mutable state, preventing concurrency race conditions and deadlocks.
- **Security**: Tight coupling across boundaries allows an exploit in an email parsing library to gain access to database credentials. Decoupled modules with narrow interfaces enforce defense-in-depth.

---

## 14 Interview Questions

<details>
<summary><strong>1. What is the difference between Coupling and Cohesion in simple terms?</strong></summary>

**Answer:**  
- **Cohesion** is about *internal focus*: how well the contents of a single module stick together and serve one unified purpose.
- **Coupling** is about *external interdependence*: how tightly bound two distinct modules are to each other.
</details>

<details>
<summary><strong>2. Can a module have high cohesion and high coupling at the same time?</strong></summary>

**Answer:**  
Yes. A module might do exactly one job perfectly (e.g. generating an encrypted PDF invoice), but to accomplish that job it directly calls 15 concrete, proprietary external libraries without abstractions. It has high cohesion (one clear purpose) but high coupling (vulnerable to any library change).
</details>

<details>
<summary><strong>3. What is Afferent Coupling ($C_a$) versus Efferent Coupling ($C_e$)?</strong></summary>

**Answer:**  
- **Afferent Coupling ($C_a$)**: Incoming dependencies. The number of external classes that depend on this class. High $C_a$ indicates responsibility.
- **Efferent Coupling ($C_e$)**: Outgoing dependencies. The number of external classes this class depends on. High $C_e$ indicates instability.
</details>

<details>
<summary><strong>4. Why is Coincidental Cohesion the worst form of cohesion?</strong></summary>

**Answer:**  
Coincidental cohesion occurs when elements are grouped together arbitrarily with no meaningful relationship (e.g. a `MiscHelper` class containing string manipulation, credit card validation, and file deletion). It guarantees that any developer touching the class has no clear mental model, and changes are prone to unexpected regressions.
</details>

<details>
<summary><strong>5. What is the Law of Demeter and how does it reduce coupling?</strong></summary>

**Answer:**  
The Law of Demeter ("Principle of Least Knowledge") states that a method should only talk to its immediate collaborators and avoid navigating object graphs (e.g. `order.getCustomer().getAddress().getZipCode()`). Navigating chains couples the caller to the internal structural topology of three intermediate classes.
</details>

<details>
<summary><strong>6. How does Dependency Inversion reduce coupling?</strong></summary>

**Answer:**  
Instead of high-level business policy classes depending directly on low-level database or network classes, both depend on an abstract interface owned by the high-level policy. This eliminates compile-time coupling to concrete infrastructure.
</details>

<details>
<summary><strong>7. How do you detect low cohesion in a code review?</strong></summary>

**Answer:**  
Signals include: class file exceeding 500 lines, class name ending in `Manager` or `Processor`, methods using completely disjoint sets of instance fields, and importing packages from 4+ distinct architectural layers (e.g. HTTP, SQL, SMTP, Metrics).
</details>

<details>
<summary><strong>8. What is Tramp Data coupling?</strong></summary>

**Answer:**  
Tramp data coupling occurs when data is passed through intermediate components that do not need it, simply to deliver it to a downstream component. This needlessly couples intermediate components to the format and existence of that data.
</details>

---

## 15 Practice Exercises

### Exercise 1: Identify Cohesion Type (Easy)
Classify the cohesion type of `DateAndTimeUtils` with methods: `formatDate()`, `parseIsoString()`, `addDays()`, `isLeapYear()`.
<details>
<summary><strong>Solution</strong></summary>

**Communicational / Functional Cohesion**. All methods operate on the same conceptual domain (calendar and time calculations) and perform clear mathematical/formatting operations on dates.
</details>

### Exercise 2: Calculate Instability Metric (Medium)
Package `com.app.billing` has 8 incoming dependencies from other packages ($C_a = 8$) and imports 2 utility classes from `com.app.common` ($C_e = 2$). Calculate its Instability Metric $I$. Is it stable or unstable?
<details>
<summary><strong>Solution</strong></summary>

$$I = \frac{C_e}{C_a + C_e} = \frac{2}{8 + 2} = \frac{2}{10} = 0.2$$  
**Result:** With $I = 0.2$, the package is **highly stable**. It is heavily depended upon and should be difficult to change, which is ideal for core billing logic.
</details>

### Exercise 3: Refactor High Coupling (Medium)
Refactor this code to eliminate tight coupling to PostgreSQL:
```java
class UserService {
    public void registerUser(String email) {
        PgConnection conn = new PgConnection("jdbc:postgresql://localhost:5432/db");
        conn.execute("INSERT INTO users VALUES ('" + email + "')");
    }
}
```
<details>
<summary><strong>Solution</strong></summary>

```java
interface UserRepository { void save(String email); }

class UserService {
    private final UserRepository repo;
    public UserService(UserRepository repo) { this.repo = repo; }
    public void registerUser(String email) {
        repo.save(email);
    }
}
```
</details>

### Exercise 4: Spot the Feature Envy Code Smell (Hard)
Identify the coupling problem:
```java
class CustomerSummaryView {
    public String render(Customer c) {
        String addr = c.getAddress().getStreet() + ", " + c.getAddress().getCity();
        double total = 0;
        for (Order o : c.getOrders()) {
            total += o.getAmount();
        }
        return c.getName() + " spent $" + total + " at " + addr;
    }
}
```
<details>
<summary><strong>Solution</strong></summary>

**Feature Envy & Law of Demeter Violation:** `CustomerSummaryView` navigates deep object graphs and performs business aggregations that belong to `Customer`.  
**Fix:** Move the total calculation into `customer.calculateLifetimeSpend()` and address formatting into `address.formatSingleLine()`.
</details>

### Exercise 5: Cohesion Audit Checklist (Easy)
List 3 questions you can ask during code review to test whether a class has high cohesion.
<details>
<summary><strong>Solution</strong></summary>

1. Can the class's responsibility be described in one simple sentence without using the word "and"?
2. Do all or most methods in the class utilize all or most instance fields?
3. If business requirements change for one feature, does this class change for unrelated features?
</details>

### Exercise 6: Decouple via Inversion of Control in TypeScript (Hard)
Refactor a `ReportExporter` that hardcodes `new PdfLibrary()` and `new S3Uploader()` so that both dependencies are loosely coupled.
<details>
<summary><strong>Solution</strong></summary>

```typescript
interface DocumentRenderer { render(data: unknown): Uint8Array; }
interface CloudUploader { upload(key: string, bytes: Uint8Array): Promise<string>; }

class ReportExporter {
    constructor(
        private readonly renderer: DocumentRenderer,
        private readonly uploader: CloudUploader
    ) {}

    async exportReport(reportId: string, data: unknown): Promise<string> {
        const bytes = this.renderer.render(data);
        return this.uploader.upload(`reports/${reportId}.pdf`, bytes);
    }
}
```
</details>

---

## 16 Summary / Cheat Sheet

```
+-----------------------------------------------------------------------------------------+
| COUPLING & COHESION CHEAT SHEET                                                         |
| Goal: High Cohesion + Low Coupling.                                                     |
|                                                                                         |
| High Cohesion:                                                                          |
| - Class does ONE well-defined job.                                                      |
| - All methods operate on the same instance fields.                                      |
| - High internal consistency; small, predictable blast radius.                           |
|                                                                                         |
| Low Coupling:                                                                           |
| - Classes depend on abstract contracts, not concrete classes.                           |
| - Zero deep property navigation (respect Law of Demeter).                               |
| - Swappable collaborators; sub-millisecond unit testability with mocks.                 |
+-----------------------------------------------------------------------------------------+
```

---

## 17 Further Reading

- Constantine, Larry L.; Yourdon, Edward (1979). *"Structured Design: Fundamentals of a Discipline of Computer Program and Systems Design"*, Prentice Hall.
- Martin, Robert C. (2002). *"Agile Software Development, Principles, Patterns, and Practices"*, Chapter 28: Granularity and Stability Metrics.
- Feathers, Michael (2004). *"Working Effectively with Legacy Code"*, Prentice Hall.

### In Your Language
| Language Track | Deep Dive Reference |
|---|---|
| Java | [Java Architecture & Coupling](file:///e:/Learning/engineering-notes/01-languages/java/24-design-patterns/README.md) |
| TypeScript | [TypeScript Modularity](file:///e:/Learning/engineering-notes/01-languages/typescript/17-design-patterns/README.md) |
| Python | [Python Cohesion & Modules](file:///e:/Learning/engineering-notes/01-languages/python/23-design-patterns/README.md) |

---
[← Previous: 02 - Composition vs Inheritance](02-composition-vs-inheritance.md) | [Section Index](README.md) | [Next: 04 - Programming to Interfaces →](04-programming-to-interfaces.md)
