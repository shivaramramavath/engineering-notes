# Dependency Injection Pattern in Modern Java

> Inversion of Control (IoC) architectures: Constructor vs. Field vs. Setter injection, pure Java DI without frameworks, JSR-330 (`jakarta.inject`), and integration with Spring Boot & Quarkus.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core Architecture / Structural |
| **Prerequisites** | [00_solid-principles](../23-design-and-clean-code/00_solid-principles.md), [04_immutability](../23-design-and-clean-code/04_immutability.md) |
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

### What is Dependency Injection?
Dependency Injection (DI) is a software design pattern that implements **Inversion of Control (IoC)** for resolving dependencies. An injection occurs when an external entity (an assembler or DI container) supplies the required dependencies (collaborating services, data sources, clients) to an object, rather than the object creating them internally via `new`.

In enterprise software engineering:
- Decouples high-level business policies from low-level infrastructure implementations (DIP).
- Makes classes 100% unit testable by allowing mocks or stubs to be passed cleanly.
- Enables flexible runtime configuration changes without re-compilation.

### Why Hardcoded `new` Operators Poison Architecture
When a class writes:
```java
public class UserService {
    private final OrderDao dao = new PostgresOrderDao(); // TIGHT COUPLING!
}
```
1. `UserService` cannot be tested without a running PostgreSQL database.
2. It cannot switch to an in-memory cache, an Oracle DB, or an external microservice without modifying source code.
3. It violates the Single Responsibility Principle by managing both business logic and dependency lifecycles.

---

## 2. Core Concepts

### The Three Styles of Injection
1. **Constructor Injection (Gold Standard)**: Dependencies are passed as constructor arguments and stored in `private final` fields.
   - *Benefits*: Enforces immutability, guarantees non-null dependencies, detects circular dependencies at compile time, requires zero framework reflection.
2. **Setter Injection**: Dependencies are passed via public setter methods.
   - *Use Case*: Optional or reconfigurable dependencies with sane defaults.
3. **Field Injection (Anti-Pattern)**: Annotating private fields directly (`@Autowired private OrderDao dao;`).
   - *Problems*: Bypasses constructors, prevents immutability, hides dependencies, breaks plain unit testing.

### JSR-330 (`jakarta.inject`) Standard Annotations
Java standardized dependency injection annotations across containers:
- `@Inject`: Identifies injectable constructors, methods, or fields.
- `@Named` / `@Qualifier`: Differentiates multiple implementations of the same interface.
- `@Singleton`: Marks a bean as a single shared application instance.

---

## 3. How It Works Internally

### Pure Dependency Injection (No Frameworks Required)
Dependency Injection does **not** require Spring, Guice, or any third-party framework!
Pure Java DI (Poor Man\'s DI) wires dependencies explicitly in the application entrypoint (`main()`):

```
+--------------------------------------------------------------------------+
|                  PURE JAVA DEPENDENCY INJECTION AT STARTUP               |
|                                                                          |
|  public static void main(String[] args) {                                |
|      // 1. Instantiate Low-level Infrastructure                         |
|      DataSource ds = new HikariDataSource(config);                      |
|      OrderRepository repo = new PostgresOrderRepository(ds);            |
|      PaymentGateway gateway = new StripePaymentGateway(apiKey);         |
|      EmailService email = new SmtpEmailService(smtpHost);               |
|                                                                          |
|      // 2. Inject into High-level Domain Service                         |
|      OrderService service = new OrderService(repo, gateway, email);      |
|                                                                          |
|      // 3. Start Server with Fully Wired Service                         |
|      HttpServer.start(service);                                         |
|  }                                                                       |
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### 1. Mandatory Constructor Injection Pattern
```java
import java.util.Objects;

public final class PaymentProcessor {
    private final PaymentGateway gateway;
    private final AuditLogger logger;

    // The single canonical constructor
    public PaymentProcessor(PaymentGateway gateway, AuditLogger logger) {
        this.gateway = Objects.requireNonNull(gateway, "gateway cannot be null");
        this.logger = Objects.requireNonNull(logger, "logger cannot be null");
    }

    public void processPayment(String account, double amount) {
        gateway.charge(account, amount);
        logger.log("Payment processed for: " + account);
    }
}
```

---

## 5. Practical Examples

### Example 1: Unit Testing with Constructor Injected Mocks
**Goal:** Test `OrderFulfillmentService` in pure memory in 2 milliseconds without starting any database, Spring context, or network sockets.

```java
import java.math.BigDecimal;
import java.util.Objects;

public record Order(String id, BigDecimal amount) {}

public interface OrderRepository {
    void save(Order o);
}

public interface InventoryClient {
    boolean reserveStock(String id);
}

// Service under test
public class OrderFulfillmentService {
    private final OrderRepository repository;
    private final InventoryClient inventory;

    public OrderFulfillmentService(OrderRepository repo, InventoryClient inv) {
        this.repository = Objects.requireNonNull(repo);
        this.inventory = Objects.requireNonNull(inv);
    }

    public boolean fulfill(Order order) {
        if (!inventory.reserveStock(order.id())) {
            return false;
        }
        repository.save(order);
        return true;
    }
}

// Plain JUnit 5 Test (Runs in 1 ms with zero frameworks!)
public class OrderFulfillmentTest {
    public static void main(String[] args) {
        // Fast in-memory test stubs
        OrderRepository mockRepo = order -> System.out.println("TEST STUB: Saved " + order.id());
        InventoryClient mockInventorySuccess = id -> true;
        InventoryClient mockInventoryFail = id -> false;

        OrderFulfillmentService service = new OrderFulfillmentService(mockRepo, mockInventorySuccess);
        boolean success = service.fulfill(new Order("ORD-1", new BigDecimal("100.00")));
        assert success : "Fulfillment should succeed";

        OrderFulfillmentService failService = new OrderFulfillmentService(mockRepo, mockInventoryFail);
        boolean failed = failService.fulfill(new Order("ORD-2", new BigDecimal("50.00")));
        assert !failed : "Fulfillment should fail when inventory unavailable";

        System.out.println("PASS: Fast pure DI unit tests passed!");
    }
}
```

### Example 2: Circular Dependency Detection
**Goal:** Understand why Constructor Injection protects against circular dependency design flaws.

If `ClassA` requires `ClassB` in its constructor, and `ClassB` requires `ClassA`:
- With Constructor Injection, `new ClassA(new ClassB(???))` is impossible to compile or instantiate! The compiler or DI container immediately throws a `BeanCurrentlyInCreationException`.
- Circular dependencies indicate a severe architectural flaw: a third class should be extracted to hold the shared responsibility.

---

## 6. Best Practices

1. **Mandate Constructor Injection**: Always declare dependencies as `private final` fields initialized in constructors.
2. **Never Use Field Injection**: Forbid `@Autowired private MyService s;` in team code reviews.
3. **Use Fail-Fast Preconditions**: Validate non-null dependencies with `Objects.requireNonNull()` in constructors.
4. **Program to Interfaces**: Inject `OrderRepository` (interface), not `PostgresOrderRepository` (concrete class).
5. **Keep Constructors Free of Side-Effects**: Constructors should only assign fields. Do not make database calls, open network sockets, or start threads in constructors.
6. **Use `@Qualifier` for Ambiguous Implementations**: When multiple implementations exist, qualify them cleanly.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Secure Fix |
| :--- | :--- | :--- |
| **Field Injection (`@Autowired`)** | Cannot make fields final; breaks plain unit tests; hides dependencies. | Use Constructor Injection. |
| **I/O inside constructors** | Slows container startup; prevents easy testing; risks resource leaks on failure. | Move I/O to lifecycle methods (e.g. `@PostConstruct`). |
| **Using DI containers for simple CLI tools** | Adds 20 MB of framework bloat and 2-second startup lag for a 50-line tool. | Use Pure Java DI in `main()`. |
| **Circular Dependencies (`A -> B -> A`)** | Tight coupling; indicates flawed domain boundaries. | Extract shared logic into `ClassC`. |

---

## 8. Performance & Security Notes

### Framework Reflection vs. Pure DI Performance
- Frameworks like Spring historically used heavy runtime reflection and classpath scanning to wire beans.
- Modern frameworks (Quarkus, Micronaut) use **Ahead-of-Time (AOT) compile-time annotation processing** to generate direct constructor instantiation bytecode, eliminating reflection overhead.
- Pure Java DI in `main()` has absolute zero reflection overhead, optimal for micro-benchmarks and AWS Lambda cold starts.

---

## 9. Comparison / When to Use What

| Injection Style | Immutability | Testability | Circularity Safety | Modern Recommendation |
| :--- | :--- | :--- | :--- | :--- |
| **Constructor** | Yes (`final`) | Exceptional | Compile-time detected | Mandated Standard |
| **Setter** | No (Mutable) | Good | Allowed (Risky) | Optional dependencies only |
| **Field** | No | Extremely Poor | Allowed (Fragile) | Strongly Deprecated |

---

## 10. Interview Questions & Answers

### Q1: What is Inversion of Control (IoC) and how does Dependency Injection implement it?
<details>
<summary>View Answer</summary>
Inversion of Control is an architectural principle where the flow of control is inverted: instead of an application object instantiating and controlling its dependencies, an external framework or assembler controls dependency creation and injects them into the object. Dependency Injection is the primary design pattern that implements IoC by passing dependencies into constructors, setters, or fields.
</details>

### Q2: Why is Constructor Injection universally preferred over Field Injection?
<details>
<summary>View Answer</summary>
1. **Immutability**: Dependencies can be declared `private final`, ensuring thread safety and preventing reassignment.
2. **Testability**: Classes can be tested in pure JUnit without reflection or starting heavy Spring contexts.
3. **Fail-Fast Safety**: Prevents `NullPointerException` at runtime; an object cannot be instantiated without its required dependencies.
4. **Architectural Clarity**: The constructor signature explicitly documents all dependencies of the class.
</details>

### Q3: What is "Pure DI" (or "Poor Man\'s DI")?
<details>
<summary>View Answer</summary>
Pure DI is the practice of practicing Dependency Injection without using any DI framework or container (like Spring, Guice, or Dagger). All dependencies are instantiated and wired together manually in the application\'s composition root (typically the `main()` method) using plain Java `new` expressions.
</details>

### Q4: How does Dependency Injection solve the Service Locator anti-pattern?
<details>
<summary>View Answer</summary>
A Service Locator (`ServiceLocator.getService(MyService.class)`) acts as a global registry. Classes query the locator directly, which hides true dependencies, couples code to the locator API, and causes runtime crashes if a service is missing. Dependency Injection explicitly declares dependencies in the constructor, making them transparent and enforced at compile time.
</details>

### Q5: What is a Composition Root?
<details>
<summary>View Answer</summary>
The Composition Root is the single location in an application where the entire object graph is composed and wired together. In pure Java, this is the `main()` method. In framework-based applications, it is the startup configuration class that bootstraps the container.
</details>

### Q6: Can a Java Record use Dependency Injection?
<details>
<summary>View Answer</summary>
Yes! A Java record\'s canonical constructor is an ideal target for constructor injection: `public record OrderController(OrderService service) {}`.
</details>

---

## 11. Practice Exercises

### Exercise 1: Refactor Field Injection to Constructor Injection
**Task:** Convert a field-injected Spring service to clean immutable constructor injection.
<details>
<summary>View Solution</summary>

```java
import java.util.Objects;

// CLEAN REFACTORED CODE:
public final class CustomerService {
    private final CustomerDao customerDao;
    private final AuditLogger auditLogger;

    public CustomerService(CustomerDao customerDao, AuditLogger auditLogger) {
        this.customerDao = Objects.requireNonNull(customerDao);
        this.auditLogger = Objects.requireNonNull(auditLogger);
    }
}
```
</details>

### Exercise 2: Pure Java Composition Root
**Task:** Wire a 3-tier application in `main()` without frameworks.
<details>
<summary>View Solution</summary>

```java
public class ApplicationRoot {
    public static void main(String[] args) {
        // Tier 1: Infrastructure
        OrderDao dao = new InMemoryOrderDao();
        // Tier 2: Domain Service
        OrderService service = new OrderService(dao);
        // Tier 3: Presentation Controller
        OrderController controller = new OrderController(service);

        controller.handleRequest("ORD-101");
    }
}
```
</details>

### Exercise 3: Conditional Mock Injection
**Task:** Write a factory that injects real or mock SMS services based on an environment flag.
<details>
<summary>View Solution</summary>

```java
public interface SmsService { void send(String msg); }

public class SmsFactory {
    public static SmsService create(boolean isProduction) {
        return isProduction ? msg -> System.out.println("Twilio SMS: " + msg)
                            : msg -> System.out.println("Mock Console SMS: " + msg);
    }
}
```
</details>

---


### 11.4 Enterprise Dependency Injection Container Internals (Spring / CDI Model)

In enterprise Java frameworks such as Spring Framework, Micronaut, and Quarkus (CDI), the dependency injection container operates a multi-stage lifecycle and manages bean scopes:

#### 1. Three-Level Cache Resolution for Circular Dependencies
In singleton beans with setter/field injection, Spring resolves cycles using a three-level cache:
1. `singletonObjects` (Cache 1): Fully initialized singleton instances ready for consumption.
2. `earlySingletonObjects` (Cache 2): Exposed early instances (often wrapped in proxy factories for AOP).
3. `singletonFactories` (Cache 3): ObjectFactory callbacks creating early bean references.

```
+-------------------------------------------------------+
|                getBean("beanA") Request               |
+-------------------------------------------------------+
                           |
                           v
              Does beanA exist in Cache 1?
                /                     \
             Yes                      No
             /                          \
     Return beanA              Create beanA instance (Raw)
                               Add ObjectFactory to Cache 3
                                         |
                                         v
                               Populate dependencies:
                                    needs beanB
                                         |
                                         v
                              getBean("beanB") Request
                                         |
                                         v
                               Create beanB instance
                               Populate dependencies:
                                    needs beanA
                                         |
                                         v
                             Check beanA in Cache 1? (No)
                             Check beanA in Cache 2? (No)
                             Check beanA in Cache 3? (Yes!)
                                         |
                             Promote beanA to Cache 2
                             Inject early beanA into beanB
                                         |
                                         v
                              Initialize beanB fully
                             Promote beanB to Cache 1
                                         |
                                         v
                        Complete beanA dependency injection
                              Initialize beanA fully
                             Promote beanA to Cache 1
```

> [!WARNING]
> Circular dependencies with **constructor injection** CANNOT be resolved by the container because the constructor must finish executing before an object reference even exists. The container will throw `BeanCurrentlyInCreationException`. The architectural remedy is to refactor the design to eliminate the cycle or introduce an event publisher/mediator.

#### 2. Bean Lifecycle Phases
Every container-managed bean traverses these precise execution checkpoints:
1. **Instantiation**: Reflection invokes the constructor or factory method.
2. **Property Population**: Dependencies are injected into fields/setters.
3. **BeanNameAware / BeanFactoryAware**: Injects container infrastructure metadata.
4. **BeanPostProcessor (Pre-Initialization)**: Custom hooks mutate bean or inspect annotations (`@PostConstruct`).
5. **InitializingBean / custom init-method**: `afterPropertiesSet()` runs initialization logic.
6. **BeanPostProcessor (Post-Initialization)**: Dynamic proxies (CGLIB or JDK dynamic proxy) wrap the bean for `@Transactional` or `@Async`.
7. **Ready for Use**: Bean is cached in the singleton pool and served to callers.
8. **Pre-Destroy**: On context shutdown, `@PreDestroy` or `DisposableBean.destroy()` executes.

## 12. Summary / Cheat Sheet

- **Core Goal**: Supply dependencies from outside; never call `new` inside business logic.
- **Constructor Injection**: Always use `private final` fields initialized in constructors.
- **Fail Fast**: Check `Objects.requireNonNull()` on all injected dependencies.
- **No Field Injection**: Reject `@Autowired` on private fields in code reviews.
- **Testability**: Pure constructor injection allows testing in pure JUnit in milliseconds.
- **Pure DI**: You don\'t need Spring to do clean dependency injection; wire in `main()`.

---

## 13. Further Reading
- Martin Fowler: [Inversion of Control Containers and the Dependency Injection pattern](https://martinfowler.com/articles/injection.html)
- Mark Seemann: [Dependency Injection Principles, Practices, and Patterns](https://www.manning.com/books/dependency-injection-principles-practices-patterns)
- JSR-330: [Dependency Injection for Java Specification](https://jcp.org/en/jsr/detail?id=330)
