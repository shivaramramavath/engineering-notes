# Programming to Interfaces

> **One-line summary:** Program to an interface, not an implementation: clients should depend on abstract contracts rather than concrete classes so that dependencies can be replaced, mocked, and extended without modifying consuming code.

| Attribute | Specification |
|---|---|
| **Level** | Foundations / Core |
| **Prerequisites** | [01-oop-pillars.md](01-oop-pillars.md), [03-coupling-and-cohesion.md](03-coupling-and-cohesion.md) |
| **Estimated Reading Time** | 30 minutes |
| **Status** | Complete |
| **Category** | Foundations |
| **Also Known As** | "Depend on Abstractions", Contract-Based Design |
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

An accounting platform processes end-of-month client tax invoices. Inside `InvoiceDocumentService`, the author directly instantiates `ConcreteS3Uploader`:

```java
public class InvoiceDocumentService {
    private ConcreteS3Uploader uploader = new ConcreteS3Uploader();
    public void processInvoice(String id, byte[] pdf) {
        uploader.uploadFile("prod-invoices", id + ".pdf", pdf);
    }
}
```

This works until three critical requirements land:
1. **Local Development Failure**: Developers cannot run the system offline on laptops because `ConcreteS3Uploader` requires valid AWS IAM credentials, a live VPC endpoint, and real network bandwidth.
2. **Enterprise Cloud Compliance**: An enterprise bank client demands on-premises deployment where files must be saved to an internal NAS (Network Attached Storage) or Azure Blob Storage, not AWS S3. Supporting Azure requires rewriting `InvoiceDocumentService`.
3. **Flaky CI/CD Tests**: Unit tests in GitHub Actions fail intermittently due to AWS network timeouts and cost \$400/month in S3 API request fees.

The system is hardwired to a volatile concretion. The business logic cannot be separated from the cloud provider's network SDK.

---

## 2 Why It Exists

In 1994, the Gang of Four (Gamma, Helm, Johnson, Vlissides) articulated the first principle of object-oriented design:
> *"Program to an interface, not an implementation."*

In software engineering, an **interface** is any contractual agreement between caller and callee. When code is written to depend on a concrete class, it is chained to that class's exact construction mechanics, internal dependencies, and external network requirements. When written to an interface, the concrete implementation becomes a swappable plug-in.

---

## 3 What It Is

Programming to interfaces means:
1. Client code declares variables, parameters, and fields using the **interface type** rather than the concrete class.
2. Object creation is decoupled from usage (via dependency injection, factory methods, or builders).
3. The interface expresses **domain intent** (`store(key, bytes)`), not vendor mechanics (`uploadToS3BucketUsingV4Signature(...)`).

```mermaid
graph TD
    subgraph WithoutInterface["Hardwired Concretion (Brittle)"]
        ClientService[InvoiceDocumentService] -->|instantiates| S3[ConcreteS3Uploader]
    end
    subgraph WithInterface["Programming to Interface (Swappable)"]
        DecoupledService[DecoupledInvoiceService] -->|depends on| IBlob[BlobStorage Interface]
        IBlob <|.. S3Impl[S3BlobStorage (Production)]
        IBlob <|.. AzureImpl[AzureBlobStorage (Enterprise)]
        IBlob <|.. MemImpl[InMemoryBlobStorage (Unit Tests)]
    end
```

---

## 4 The Problem Without It

Here is the tight-coupling implementation that directly instantiates a concrete cloud client:

```java
// File: InterfacesWithoutApp.java
class ConcreteS3Uploader {
    public void uploadFile(String bucket, String key, byte[] content) {
        System.out.println("S3: Uploading " + key + " to bucket " + bucket);
    }
}

class InvoiceDocumentService {
    // Tightly coupled to concrete AWS S3 implementation
    private ConcreteS3Uploader uploader = new ConcreteS3Uploader();

    public void processInvoice(String id, byte[] pdf) {
        // Hardcoded dependency prevents local testing
        uploader.uploadFile("prod-invoices", id + ".pdf", pdf);
        System.out.println("Invoice " + id + " uploaded to S3.");
    }
}

public class InterfacesWithoutApp {
    public static void main(String[] args) {
        InvoiceDocumentService service = new InvoiceDocumentService();
        service.processInvoice("INV-2026-001", new byte[]{1, 2, 3});
    }
}
```

```text
S3: Uploading INV-2026-001.pdf to bucket prod-invoices
Invoice INV-2026-001 uploaded to S3.
```

### The Change Request: Run in Offline Unit Tests
When asked to write a unit test that verifies invoice PDF generation:
- You cannot substitute `ConcreteS3Uploader` because it is instantiated with `new` inside private field initialization.
- You must either mock private fields with bytecode manipulation (PowerMock) or allow your test to make real AWS network calls.

---

## 5 How It Solves It

By introducing `BlobStorage` and injecting it via the constructor, `DecoupledInvoiceService` works seamlessly in production with AWS S3 and in tests with an ultra-fast in-memory map.

### Java Implementation
```java
// File: InterfacesWithApp.java
import java.util.*;

interface BlobStorage {
    void store(String key, byte[] data);
    byte[] retrieve(String key);
}

class S3BlobStorage implements BlobStorage {
    private final String bucket;
    public S3BlobStorage(String bucket) { this.bucket = bucket; }
    public void store(String key, byte[] data) {
        System.out.println("S3Storage: Stored " + key + " in " + bucket);
    }
    public byte[] retrieve(String key) { return new byte[0]; }
}

class InMemoryBlobStorage implements BlobStorage {
    private final Map<String, byte[]> memory = new HashMap<>();
    public void store(String key, byte[] data) {
        memory.put(key, data);
        System.out.println("InMemoryStorage: Stored " + key + " (size: " + data.length + " bytes)");
    }
    public byte[] retrieve(String key) { return memory.get(key); }
}

class DecoupledInvoiceService {
    private final BlobStorage storage;

    public DecoupledInvoiceService(BlobStorage storage) {
        this.storage = storage;
    }

    public void processInvoice(String id, byte[] pdf) {
        storage.store("invoices/" + id + ".pdf", pdf);
        System.out.println("DecoupledService: Invoice " + id + " stored successfully.");
    }
}

public class InterfacesWithApp {
    public static void main(String[] args) {
        // Production wiring
        BlobStorage prodStorage = new S3BlobStorage("prod-vault");
        DecoupledInvoiceService prodService = new DecoupledInvoiceService(prodStorage);
        prodService.processInvoice("INV-001", new byte[]{10, 20});

        // Test wiring (sub-millisecond in-memory test)
        BlobStorage testStorage = new InMemoryBlobStorage();
        DecoupledInvoiceService testService = new DecoupledInvoiceService(testStorage);
        testService.processInvoice("INV-TEST", new byte[]{99, 88, 77});
    }
}
```

### TypeScript Implementation
```typescript
// File: InterfacesWithApp.ts
interface BlobStorage {
    store(key: string, data: Uint8Array): void;
    retrieve(key: string): Uint8Array | undefined;
}

class S3BlobStorage implements BlobStorage {
    private readonly bucket: string;
    constructor(bucket: string) { this.bucket = bucket; }
    store(key: string, data: Uint8Array): void {
        console.log(`TS S3: Stored ${key} in ${this.bucket}`);
    }
    retrieve(key: string): Uint8Array | undefined { return undefined; }
}

class InMemoryBlobStorage implements BlobStorage {
    private readonly memory = new Map<string, Uint8Array>();
    store(key: string, data: Uint8Array): void {
        this.memory.set(key, data);
        console.log(`TS InMemory: Stored ${key} (size: ${data.length} bytes)`);
    }
    retrieve(key: string): Uint8Array | undefined { return this.memory.get(key); }
}

class DecoupledInvoiceService {
    private readonly storage: BlobStorage;
    constructor(storage: BlobStorage) { this.storage = storage; }
    processInvoice(id: string, pdf: Uint8Array): void {
        this.storage.store(`invoices/${id}.pdf`, pdf);
        console.log(`TS DecoupledService: Invoice ${id} stored.`);
    }
}

const svc = new DecoupledInvoiceService(new InMemoryBlobStorage());
svc.processInvoice("INV-TS", new Uint8Array([1, 2, 3]));
```

### Python Implementation
```python
# File: interfaces_with_app.py
from typing import Protocol, Optional

class BlobStorage(Protocol):
    def store(self, key: str, data: bytes) -> None: ...
    def retrieve(self, key: str) -> Optional[bytes]: ...

class InMemoryBlobStorage:
    def __init__(self):
        self._memory: dict[str, bytes] = {}

    def store(self, key: str, data: bytes) -> None:
        self._memory[key] = data
        print(f"Python InMemory: Stored {key} (size: {len(data)} bytes)")

    def retrieve(self, key: str) -> Optional[bytes]:
        return self._memory.get(key)

class DecoupledInvoiceService:
    def __init__(self, storage: BlobStorage):
        self._storage = storage

    def process_invoice(self, invoice_id: str, pdf: bytes) -> None:
        self._storage.store(f"invoices/{invoice_id}.pdf", pdf)
        print(f"Python DecoupledService: Invoice {invoice_id} stored.")

svc = DecoupledInvoiceService(InMemoryBlobStorage())
svc.process_invoice("INV-PY", b"test content")
```

```text
S3Storage: Stored invoices/INV-001.pdf in prod-vault
DecoupledService: Invoice INV-001 stored successfully.
InMemoryStorage: Stored invoices/INV-TEST.pdf (size: 3 bytes)
DecoupledService: Invoice INV-TEST stored successfully.
TS InMemory: Stored invoices/INV-TS.pdf (size: 3 bytes)
TS DecoupledService: Invoice INV-TS stored.
Python InMemory: Stored invoices/INV-PY.pdf (size: 12 bytes)
Python DecoupledService: Invoice INV-PY stored.
```

---

## 6 How It Works

### The Seam Concept (Michael Feathers)
A **Seam** is a place where you can alter behavior in a program without editing in that place.
- In `InterfacesWithoutApp`, there is no seam: the instantiation of `new ConcreteS3Uploader()` is sealed inside the class.
- In `InterfacesWithApp`, the constructor parameter `BlobStorage` creates an **Object Seam**: callers can inject any conforming implementation without modifying a single line of `DecoupledInvoiceService`.

---

## 7 Real-World Examples

### 1. Business Domain: Payment Gateway Processing
An e-commerce checkout depends on `PaymentProcessor`:
```java
public interface PaymentProcessor {
    PaymentAuthorization authorize(Money amount, PaymentToken token);
}
```
In production, `StripePaymentProcessor` executes over HTTPS. In staging, `SandboxPaymentProcessor` returns simulated 200 approvals. In automated load tests, `NoOpPaymentProcessor` returns instant authorizations, allowing the team to benchmark database ingestion rates without hitting third-party rate limits.

### 2. Standard Library: `java.util.List` vs `ArrayList`
Every experienced Java engineer writes:
```java
List<String> names = new ArrayList<>(); // Good
ArrayList<String> names = new ArrayList<>(); // Brittle
```
Declaring `List<String>` guarantees that if you later change the backing store to `Collections.synchronizedList()` or `CopyOnWriteArrayList` for thread safety, zero downstream method signatures break.

### 3. Modern Cloud / Infrastructure: OpenTelemetry Tracing
The OpenTelemetry SDK provides an abstract `Tracer` interface. Microservices instrument their code by calling `tracer.spanBuilder("checkout").startSpan()`. The service has zero compile-time coupling to whether traces are exported to Jaeger, Datadog, Honeycomb, or a local console.

---

## 8 When to Use and When NOT to Use

### Program to Interfaces When:
- A dependency communicates with external I/O (databases, network APIs, message brokers, file systems).
- The class needs to be isolated for automated unit testing using mocks or fakes.
- Multiple production implementations exist or are planned (e.g. multi-cloud storage).

### Avoid / Over-Engineering Signals:
- **1-to-1 Interface Explosion**: Automatically generating `IUserService` for every `UserService` when there is only one implementation and no polymorphic need.
- **Data Transfer Objects & Value Objects**: Creating an interface for `Address` or `Point2D` adds indirection with zero benefit. Value objects are concrete by design.

---

## 9 Variations and Language Idioms

- **Java**: Explicit `interface` keyword. Since Java 8, interfaces can contain `default` methods for backward-compatible evolution.
- **TypeScript**: `interface` and `type` declarations are purely compile-time constructs with zero runtime overhead (they are stripped during compilation). Structural typing means any object with the required shape matches the interface.
- **Python**: Uses `typing.Protocol` (PEP 544) to provide static duck-typing. Classes don't need to inherit from `Protocol` to satisfy it.

---

## 10 Relationship to Other Principles and Patterns

- **Dependency Inversion Principle (DIP)**: The architectural formalization of programming to interfaces.
- **Strategy Pattern**: Implements programming to interfaces by encapsulating interchangeable algorithms.
- **Abstract Factory Pattern**: Produces families of objects where every returned object is typed as an interface.

---

## 11 Common Mistakes and Anti-Patterns

| Mistake | Why It Breaks | Production Fix |
|---|---|---|
| **Header Interfaces (`IFoo` / `FooImpl`)** | Blindly creating an interface for every single class leads to 2x file clutter with zero polymorphic value. | Create interfaces only where volatility, testing seams, or multiple implementations exist. |
| **Leaking Implementation Details in Interface** | Naming a method `saveToPostgresTable()` or accepting `java.sql.Connection` couples the interface to database details. | Express domain intent: `void save(User user);`. |
| **Fat Interfaces** | Forcing clients to depend on 25 methods when they only need 2 (violates ISP). | Split into small, focused, role-based interfaces (`Readable`, `Writable`). |
| **Direct `new` Inside Business Logic** | Bypasses the interface by hardcoding concrete instantiations deep inside methods. | Inject dependencies via constructor injection or factories. |
| **Using Abstract Classes for Contracts** | Abstract classes consume the single inheritance slot and couple subclasses to default behavior. | Prefer interfaces for public contracts; use abstract classes only for skeletal implementation reuse. |
| **Downcasting (`(S3Storage) storage`)** | Casting an interface back to its concretion destroys the abstraction and causes runtime `ClassCastException`. | If client needs specific behavior, model it as a polymorphic method on the interface or split the interface. |

---

## 12 Testing and Design Verification

Programming to interfaces transforms slow, brittle integration tests into deterministic, in-memory unit tests:

```java
// Testing DecoupledInvoiceService without AWS or local network
class FakeStorage implements BlobStorage {
    boolean stored = false;
    public void store(String key, byte[] data) { stored = true; }
    public byte[] retrieve(String key) { return new byte[0]; }
}

class InvoiceServiceTest {
    void testInvoiceStorage() {
        FakeStorage fake = new FakeStorage();
        DecoupledInvoiceService service = new DecoupledInvoiceService(fake);
        service.processInvoice("INV-TEST", new byte[]{1, 2});
        assert fake.stored; // Verified deterministically in 0.2ms
    }
}
```

---

## 13 Performance, Concurrency and Security Notes

- **Interface Call Performance**: Interface invocations in Java (`invokeinterface`) are slightly more complex than class method calls (`invokevirtual`). However, modern JVMs utilize inline caches (monomorphic and bimorphic call sites). For 99% of enterprise applications, the cost difference is unmeasurable (sub-nanosecond).
- **Security**: Interfaces enforce strict capability boundaries. A plugin can be passed a narrow `LogWriter` interface rather than a full `FileSystem` reference, preventing malicious directory traversals.

---

## 14 Interview Questions

<details>
<summary><strong>1. What does "Program to an interface, not an implementation" actually mean?</strong></summary>

**Answer:**  
It means client code should declare variables, parameters, and return types using abstract contracts (interfaces or abstract base classes) rather than concrete classes. The client knows *what* operations are available, but remains completely decoupled from *how* those operations are executed.
</details>

<details>
<summary><strong>2. What is the danger of prefixing every interface with "I" (e.g. <code>IUserService</code>)?</strong></summary>

**Answer:**  
Prefixing with "I" is a historical C# / COM convention often misused in Java and TypeScript to create mindless 1-to-1 pairings (`IUserService` and `UserServiceImpl`). If an interface has only one implementation and represents the core domain capability, the interface should simply be named `UserService`. If different implementations exist, name them by their variation (e.g. `SqlUserRepository`, `InMemoryUserRepository`).
</details>

<details>
<summary><strong>3. When should you use an abstract class instead of an interface?</strong></summary>

**Answer:**  
Use an **interface** when defining an API contract, swappable capability, or role across unrelated classes. Use an **abstract class** when you want to share common state (instance fields) and non-trivial default implementation code among a closely related family of classes (e.g. Template Method pattern).
</details>

<details>
<summary><strong>4. How does structural typing in TypeScript change the way interfaces are used?</strong></summary>

**Answer:**  
In nominal languages like Java, a class must explicitly declare `implements Interface`. In TypeScript, interfaces are structural: any object that satisfies the required shape automatically satisfies the interface. This allows consumers to define their own minimal target interfaces without requiring third-party library classes to declare adherence.
</details>

<details>
<summary><strong>5. What is a "Seam" in software testing?</strong></summary>

**Answer:**  
Coined by Michael Feathers, a seam is a place in the codebase where program behavior can be altered without editing the source code in that place. Constructor-injected interfaces create ideal object seams, allowing production code to be injected with test doubles.
</details>

<details>
<summary><strong>6. What is the difference between an Interface and an Abstract Data Type (ADT)?</strong></summary>

**Answer:**  
An **interface** is a language-level contract specifying method signatures that an object must implement. An **ADT** is a mathematical model for data types defined by behavior from the point of view of a user of the data (e.g. a Stack, Queue, or Map) without specifying how the underlying memory is arranged.
</details>

<details>
<summary><strong>7. What happens if you downcast an interface to a concrete class in client code?</strong></summary>

**Answer:**  
Downcasting violates the abstraction boundary. It tightly couples the client to the specific concrete type, introduces runtime `ClassCastException` risk, and breaks the ability to substitute alternative implementations or test doubles.
</details>

<details>
<summary><strong>8. How does Python's <code>typing.Protocol</code> differ from an abstract base class (<code>abc.ABC</code>)?</strong></summary>

**Answer:**  
`abc.ABC` enforces nominal inheritance: classes must inherit from the ABC (or be explicitly registered) to be recognized as instances. `typing.Protocol` enforces structural subtyping (static duck-typing): any class that implements the methods specified in the protocol satisfies the type hint at static analysis time, without inheriting from it.
</details>

---

## 15 Practice Exercises

### Exercise 1: Spot the Tight Coupling (Easy)
Identify why this repository method prevents testing:
```java
public class NotificationManager {
    public void alertAdmins(String text) {
        TwilioSmsClient client = new TwilioSmsClient("AUTH_TOKEN");
        client.sendSms("+15551234567", text);
    }
}
```
<details>
<summary><strong>Solution</strong></summary>

`NotificationManager` directly instantiates `new TwilioSmsClient(...)` inside the method. There is no seam to inject a fake SMS sender for unit testing, and running this code in tests will attempt to make real Twilio network calls and spend money.  
**Fix:** Inject an `SmsClient` interface via the constructor.
</details>

### Exercise 2: Refactor to an Interface (Medium)
Extract an interface for caching and refactor `ReportGenerator`:
```java
public class RedisCache {
    public void set(String k, String v) { /* redis set */ }
    public String get(String k) { /* redis get */ return null; }
}

public class ReportGenerator {
    private RedisCache cache;
    public ReportGenerator(RedisCache cache) { this.cache = cache; }
}
```
<details>
<summary><strong>Solution</strong></summary>

```java
public interface CacheProvider {
    void put(String key, String value);
    String get(String key);
}

public class RedisCache implements CacheProvider {
    public void put(String key, String value) { /* redis set */ }
    public String get(String key) { /* redis get */ return null; }
}

public class ReportGenerator {
    private final CacheProvider cache;
    public ReportGenerator(CacheProvider cache) { this.cache = cache; }
}
```
</details>

### Exercise 3: Consumer-Driven Interface (Medium)
A class `FullUserRecord` has 25 fields and methods. A `WelcomeEmailService` only needs `user.getEmail()` and `user.getFirstName()`. Design a minimal interface.
<details>
<summary><strong>Solution</strong></summary>

```java
public interface EmailRecipient {
    String getEmail();
    String getFirstName();
}

public class WelcomeEmailService {
    public void sendWelcome(EmailRecipient recipient) {
        String msg = "Welcome " + recipient.getFirstName() + "!";
        // deliver to recipient.getEmail()
    }
}
```
</details>

### Exercise 4: Spot the Leaky Abstraction (Hard)
Explain why this interface leaks internal concretion details:
```java
public interface CustomerStore {
    void save(Customer customer, java.sql.Connection conn);
    java.sql.ResultSet executeRawQuery(String sql);
}
```
<details>
<summary><strong>Solution</strong></summary>

The interface explicitly leaks JDBC details (`java.sql.Connection`, `java.sql.ResultSet`, and raw SQL strings) in its method signatures. This makes it impossible to implement this interface using MongoDB, Redis, an in-memory test double, or a REST API backend.  
**Fix:** Remove JDBC dependencies; return domain entities (`Optional<Customer> findById(String id)`).
</details>

### Exercise 5: Verify Interface Polymorphism in TypeScript (Easy)
Write a TypeScript function that accepts any object with a `length: number` property and returns its double, without requiring explicit class inheritance.
<details>
<summary><strong>Solution</strong></summary>

```typescript
interface HasLength { length: number; }
function getDoubleLength(item: HasLength): number {
    return item.length * 2;
}
// Works on strings, arrays, or custom objects:
getDoubleLength("hello"); // 10
getDoubleLength([1, 2, 3]); // 6
getDoubleLength({ length: 42 }); // 84
```
</details>

### Exercise 6: Role Interface vs Class Interface (Hard)
Explain the difference between a "Role Interface" and a "Header Interface".
<details>
<summary><strong>Solution</strong></summary>

- A **Header Interface** simply mirrors all public methods of a concrete class (e.g. `OrderService` paired with `IOrderService`). It is organized around the supplier and provides little architectural value if there is only one implementation.
- A **Role Interface** is defined from the *perspective of the consumer* (e.g. `AutoCloseable`, `Comparable`, `Auditable`). It models a specific role or behavior that various unrelated classes can play, keeping interfaces small, cohesive, and decoupled.
</details>

---

## 16 Summary / Cheat Sheet

```
+-----------------------------------------------------------------------------------------+
| PROGRAMMING TO INTERFACES SUMMARY                                                       |
| Rule: Depend on abstractions, never on concrete implementations.                         |
|                                                                                         |
| Why:                                                                                    |
| - Creates test seams: swap network dependencies with in-memory test doubles.            |
| - Shields business logic from external SDK churn and cloud vendor lock-in.               |
| - Enables runtime polymorphism and dynamic strategy switching.                          |
|                                                                                         |
| How:                                                                                    |
| - Declare field and parameter types as interfaces (List, Map, BlobStorage).             |
| - Inject dependencies via constructor; avoid direct 'new' inside business methods.     |
| - Express domain operations (store, find), not vendor mechanics (uploadToS3Bucket).     |
+-----------------------------------------------------------------------------------------+
```

---

## 17 Further Reading

- Gamma, Helm, Johnson, Vlissides (1994). *"Design Patterns"*, Chapter 1: Programming to an Interface, not an Implementation.
- Feathers, Michael (2004). *"Working Effectively with Legacy Code"*, Chapter 3: Sensing and Separation with Seams.
- Martin, Robert C. (2002). *"Agile Software Development"*, Interface Segregation and Dependency Inversion.

### In Your Language
| Language Track | Deep Dive Reference |
|---|---|
| Java | [Java Interfaces & Contracts](file:///e:/Learning/engineering-notes/01-languages/java/24-design-patterns/README.md) |
| TypeScript | [TypeScript Interfaces & Types](file:///e:/Learning/engineering-notes/01-languages/typescript/17-design-patterns/README.md) |
| Python | [Python Protocols & Duck Typing](file:///e:/Learning/engineering-notes/01-languages/python/23-design-patterns/README.md) |

---
[← Previous: 03 - Coupling and Cohesion](03-coupling-and-cohesion.md) | [Section Index](README.md) | [Next: 05 - Abstraction and Information Hiding →](05-abstraction-and-information-hiding.md)
