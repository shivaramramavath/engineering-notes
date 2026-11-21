# Facade Pattern in Modern Java

> Simplifying complex subsystems: unified API entry points, domain service facades, subsystem encapsulation, and microservice gateway design.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core Design Pattern / Structural |
| **Prerequisites** | [00_solid-principles](../23-design-and-clean-code/00_solid-principles.md), [04_oop](../../04-oop/README.md) |
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
7. [Common Mistakes & Pitfalls](#7-Common-Mistakes--pitfalls)
8. [Performance & Security Notes](#8-performance--security-notes)
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions & Answers](#10-interview-questions--answers)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### What is the Facade Pattern?
The Facade pattern is a structural design pattern that provides a simplified, higher-level interface to a complex subsystem of classes, libraries, or frameworks.

In real-world enterprise architectures:
- Complex subsystems often involve dozens of interconnected classes (e.g. video transcoding requires audio demuxers, video decoders, color scalers, and container muxers).
- Exposing all those low-level classes to client controllers forces clients to understand complex initialization sequences and internal dependencies.
- A Facade encapsulates the chaos behind a single, clean method: `videoConverter.convert("movie.avi", "mp4")`.

### Why Facades are Vital
1. **Reduces Cognitive Load**: Clients interact with 1 intuitive interface instead of 15 technical classes.
2. **Decouples Subsystems**: If internal subsystem classes are refactored or upgraded, client code is unaffected as long as the facade signature remains stable.
3. **Improves Portability**: Acts as an API gateway for microservices or modular subsystems.

---

## 2. Core Concepts

### Facade vs. Subsystem Classes
- **Subsystem Classes**: The specialized workers that perform the actual heavy lifting (`AudioMixer`, `BitrateReader`, `CodecFactory`). They have no awareness of the Facade; they operate independently.
- **Facade**: A high-level coordinator that orchestrates the subsystem classes in the proper execution sequence.
- **Additional Facades**: A complex system can define multiple specialized facades for different client roles (e.g. `PublicCheckoutFacade` vs `AdminOrderFacade`).

---

## 3. How It Works Internally

### Subsystem Encapsulation Architecture
```
+--------------------------------------------------------------------------+
|                  FACADE PATTERN SUBSYSTEM DECOUPLING                     |
|                                                                          |
|  [ Client (Controller) ]                                                 |
|         |                                                                |
|         | calls orderFacade.placeOrder(orderId)                          |
|         v                                                                |
|  [ OrderProcessingFacade ]                                               |
|         |                                                                |
|         +--> [ InventoryService.checkStock() ]                           |
|         +--> [ PaymentGateway.chargeCard() ]                             |
|         +--> [ WarehouseService.reserveItems() ]                         |
|         +--> [ NotificationService.sendReceipt() ]                       |
|         +--> [ AnalyticsService.trackSale() ]                            |
|                                                                          |
|  (Client knows ONLY the Facade; all 5 internal classes are shielded!)   |
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### 1. Basic Domain Facade
```java
public class OrderFulfillmentFacade {
    private final InventorySubsystem inventory;
    private final PaymentSubsystem payment;
    private final ShippingSubsystem shipping;

    public OrderFulfillmentFacade(InventorySubsystem inv, PaymentSubsystem pay, ShippingSubsystem ship) {
        this.inventory = inv;
        this.payment = pay;
        this.shipping = ship;
    }

    public boolean completeOrder(String item, int qty, String card) {
        if (!inventory.check(item, qty)) return false;
        if (!payment.charge(card)) return false;
        shipping.dispatch(item, qty);
        return true;
    }
}
```

---

## 5. Practical Examples

### Example 1: Enterprise Microservice Checkout Facade
**Goal:** Coordinate inventory allocation, payment processing, fraud inspection, and shipping.

```java
import java.math.BigDecimal;
import java.util.Objects;

// Subsystem 1: Fraud Detection
public class FraudService {
    public boolean isSuspicious(String customerId, BigDecimal amount) {
        return amount.compareTo(new BigDecimal("10000")) > 0;
    }
}

// Subsystem 2: Payment Service
public class BillingService {
    public boolean processTransaction(String accountId, BigDecimal amount) {
        System.out.println("Billed $" + amount + " to account " + accountId);
        return true;
    }
}

// Subsystem 3: Warehouse Inventory
public class StockService {
    public boolean allocateStock(String sku, int quantity) {
        System.out.println("Allocated " + quantity + " units of " + sku);
        return true;
    }
}

// Subsystem 4: Notification
public class CustomerNotifier {
    public void notifySuccess(String customerId, String sku) {
        System.out.println("Email confirmation sent to " + customerId);
    }
}

// The Production Checkout Facade
public class CheckoutFacade {
    private final FraudService fraudService;
    private final BillingService billingService;
    private final StockService stockService;
    private final CustomerNotifier notifier;

    public CheckoutFacade(FraudService f, BillingService b, StockService s, CustomerNotifier n) {
        this.fraudService = Objects.requireNonNull(f);
        this.billingService = Objects.requireNonNull(b);
        this.stockService = Objects.requireNonNull(s);
        this.notifier = Objects.requireNonNull(n);
    }

    public boolean executeCheckout(String customerId, String sku, int qty, BigDecimal amount) {
        if (fraudService.isSuspicious(customerId, amount)) {
            System.err.println("Transaction blocked by fraud detector!");
            return false;
        }

        if (!stockService.allocateStock(sku, qty)) {
            return false;
        }

        if (!billingService.processTransaction(customerId, amount)) {
            return false;
        }

        notifier.notifySuccess(customerId, sku);
        return true;
    }
}
```

### Example 2: Simplifying Complex Java NIO File System Calls
**Goal:** Create a facade over verbose Java NIO Path/Files/Streams operations.

```java
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.util.List;

public class SimpleFileFacade {
    public static void appendLines(Path path, List<String> lines) throws IOException {
        if (!Files.exists(path)) {
            Files.createDirectories(path.getParent());
            Files.createFile(path);
        }
        Files.write(path, lines, StandardOpenOption.APPEND);
    }

    public static List<String> readAll(Path path) throws IOException {
        return Files.readAllLines(path);
    }
}
```

---

### Example 3: Database Transaction & Cache Invalidation Facade
**Goal:** Coordinate database persistence and Redis cache eviction cleanly behind a facade.

```java
public class UserRegistrationFacade {
    public interface UserDatabase { void insertUser(String user); }
    public interface CacheManager { void evict(String key); }
    public interface AuditTrail { void log(String event); }

    private final UserDatabase db;
    private final CacheManager cache;
    private final AuditTrail audit;

    public UserRegistrationFacade(UserDatabase db, CacheManager cache, AuditTrail audit) {
        this.db = db;
        this.cache = cache;
        this.audit = audit;
    }

    public void register(String username) {
        db.insertUser(username);
        cache.evict("users_list");
        audit.log("USER_REGISTERED: " + username);
    }
}
```

## 6. Best Practices

1. **Keep Facades Thin**: A facade should orchestrate subsystem calls; it should not contain heavy business calculations itself.
2. **Do Not Prevent Direct Access**: Advanced clients that need fine-grained control of the subsystem should still be allowed to access low-level subsystem classes directly.
3. **Use Dependency Injection**: Inject subsystem dependencies into the facade via constructor injection.
4. **Define Facade Interfaces for Testing**: Expose the facade as an interface so controller unit tests can mock it easily.
5. **Create Multiple Facades if Necessary**: If a single facade grows beyond 10 methods, split it by client use case.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Secure Fix |
| :--- | :--- | :--- |
| **God Facade Anti-Pattern** | Turning the facade into a monolithic 2,000-line monster class. | Split into smaller, role-specific facades. |
| **Blocking direct subsystem access** | Making subsystem classes private prevents advanced users from customizing behavior. | Keep subsystem classes public or package-private. |
| **Embedding complex business logic in facade** | Violates Single Responsibility Principle. | Move core logic into domain services; facade only orchestrates. |
| **Ignoring partial failures in multi-step workflows** | If step 3 fails, steps 1 and 2 are not compensated. | Integrate Saga pattern or distributed transactions in complex orchestrations. |

---

## 8. Performance & Security Notes

### Inlining Facade Method Delegations
Facades introduce an extra layer of indirection.
- Because facades typically delegate directly to subsystem components, HotSpot C2 compiler inlines the facade methods directly into the client code.
- Zero runtime dispatch penalty exists after JIT compilation.

---

## 9. Comparison / When to Use What

| Pattern | Primary Intent | Does it change interface? |
| :--- | :--- | :--- |
| **Facade** | Simplifies complex subsystems | Provides a brand new simpler interface |
| **Adapter** | Matches existing incompatible interfaces | Adapts to an existing Target interface |
| **Proxy** | Controls access / lifecycle | Implements identical interface |
| **Decorator** | Adds dynamic behavior | Implements identical interface |

---

## 10. Interview Questions & Answers

### Q1: What is the primary purpose of the Facade pattern?
<details>
<summary>View Answer</summary>
The Facade pattern provides a simplified, unified interface to a complex subsystem. It hides the complexity of multiple interacting classes, reduces coupling between clients and the subsystem, and provides a clear entry point for common operations.
</details>

### Q2: How does the Facade pattern differ from the Adapter pattern?
<details>
<summary>View Answer</summary>
- **Adapter** wraps an existing, incompatible interface to make it work with a specific target interface expected by a client. Its purpose is compatibility.
- **Facade** defines a *new*, higher-level interface that simplifies an entire subsystem of multiple classes. Its purpose is simplicity.
</details>

### Q3: What is the "God Facade" anti-pattern?
<details>
<summary>View Answer</summary>
A God Facade occurs when a single facade class attempts to handle every possible operation across all subsystems in an enterprise application, ballooning into thousands of lines of code. The solution is to create multiple cohesive facades targeted at specific client roles (e.g. `CustomerOrderFacade`, `AdminReportingFacade`, `InventoryAuditFacade`).
</details>

### Q4: Does the Facade pattern prevent clients from accessing the underlying subsystem classes directly?
<details>
<summary>View Answer</summary>
No. A facade provides a convenient shortcut for 80% of common use cases, but it should not restrict access to low-level subsystem classes for clients that genuinely require advanced, customized, or fine-grained control.
</details>

### Q5: Give an example of the Facade pattern in the Java Standard Library or Spring.
<details>
<summary>View Answer</summary>
1. `java.net.URL`: Provides a simple facade (`url.openStream()`) over complex DNS lookups, TCP socket connections, HTTP handshakes, and header parsing.
2. Spring\'s `RestTemplate` or `JdbcTemplate`: Simplifies low-level socket and JDBC boilerplate into single-method calls (`queryForList()`).
</details>

### Q6: How does Facade relate to the API Gateway pattern in microservices?
<details>
<summary>View Answer</summary>
An API Gateway is essentially the Facade pattern applied at the distributed system level. Rather than requiring mobile or web clients to make 10 HTTP calls to 10 independent microservices, the API Gateway acts as a facade, aggregating responses and exposing a single unified REST or GraphQL endpoint.
</details>

### Q7: Can a Facade be implemented as a static utility class?
<details>
<summary>View Answer</summary>
Yes, for stateless subsystems (like file operations or math toolkits), static methods are acceptable (e.g. `Files.readAllBytes()`). However, for enterprise business subsystems with stateful dependencies, instance-based facades injected via constructor DI are preferred for unit testability.
</details>

### Q8: What design principle does Facade directly support?
<details>
<summary>View Answer</summary>
The **Law of Demeter (Principle of Least Knowledge)**. By providing a facade, clients speak directly to the facade rather than traversing deep into the internal classes of the subsystem.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a Home Theater Facade
**Task:** Create a `HomeTheaterFacade` that simplifies turning on the TV, setting the soundbar volume, and dimming lights with a single `watchMovie()` method.
<details>
<summary>View Solution</summary>

```java
public class Tv { public void on() { System.out.println("TV On"); } }
public class Soundbar { public void setVolume(int v) { System.out.println("Volume: " + v); } }
public class Lights { public void dim(int pct) { System.out.println("Dimmed to " + pct + "%"); } }

public class HomeTheaterFacade {
    private final Tv tv = new Tv();
    private final Soundbar soundbar = new Soundbar();
    private final Lights lights = new Lights();

    public void watchMovie() {
        lights.dim(20);
        tv.on();
        soundbar.setVolume(25);
    }
}
```
</details>

### Exercise 2: User Onboarding Facade
**Task:** Coordinate user database creation, welcome email, and initial reward credits in an onboarding facade.
<details>
<summary>View Solution</summary>

```java
public class OnboardingFacade {
    public interface UserDb { void save(String user); }
    public interface Emailer { void sendWelcome(String user); }
    public interface Wallet { void creditBonus(String user, int amount); }

    private final UserDb db;
    private final Emailer emailer;
    private final Wallet wallet;

    public OnboardingFacade(UserDb db, Emailer emailer, Wallet wallet) {
        this.db = db; this.emailer = emailer; this.wallet = wallet;
    }

    public void onboard(String username) {
        db.save(username);
        wallet.creditBonus(username, 100);
        emailer.sendWelcome(username);
    }
}
```
</details>

### Exercise 3: Compression & Encryption Archive Facade
**Task:** Provide a single `archiveFile(Path source, Path destination)` facade.
<details>
<summary>View Solution</summary>

```java
import java.nio.file.Path;

public class ArchiveFacade {
    public static void archiveFile(Path source, Path destination) {
        System.out.println("Compressing " + source + " and encrypting into " + destination);
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Core Goal**: Provide a simple, unified interface to a complex subsystem.
- **Decoupling**: Shields clients from complex internal subsystem classes.
- **Law of Demeter**: Prevents clients from navigating deep object graphs.
- **Thin Layer**: Facades orchestrate; they should not contain heavy business logic.
- **Not Exclusive**: Advanced clients can still access low-level subsystem classes if needed.
- **Microservices**: API Gateways represent the Facade pattern at architectural scale.

---

## 13. Further Reading
- GoF Design Patterns: [Facade Pattern](https://www.oreilly.com/library/view/design-patterns-elements/0201633612/)
- Refactoring Guru: [Facade Design Pattern](https://refactoring.guru/design-patterns/facade)
- Microservice Architecture: [API Gateway Pattern](https://microservices.io/patterns/apigateway.html)
