# Dependency Injection Styles: Constructor, Setter & Field Injection

> A comprehensive architectural comparison of Spring Dependency Injection styles: why modern Spring mandates Constructor Injection, the precise operational role of Setter Injection, the severe pitfalls of Field Injection, and advanced collection and optional wiring patterns.

| Metadata | Value |
| :--- | :--- |
| **Level** | Beginner |
| **Prerequisites** | [IoC Container & ApplicationContext Internals](file:///03-backend/spring-boot/01-core-spring/01-ioc-container-and-application-context.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.0.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | OpenJDK 25.0.1, Spring Boot 3.4.0 |

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
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### Problem Statement
In any object-oriented application, components require collaborators to execute business workflows. How these dependencies are supplied directly dictates class immutability, thread safety, testability, lifecycle predictability, and coupling. Choosing the wrong dependency injection mechanism introduces hidden null pointer exceptions, prevents fast unit testing, masks Single Responsibility Principle (SRP) violations, and causes memory leaks.

### Historical Evolution
- **Spring 1.x - 2.x (2003-2007)**: Setter injection via XML was heavily promoted because constructors with 10+ arguments were unreadable in XML.
- **Spring 2.5 - 4.x (2007-2015)**: Annotation-driven field injection (`@Autowired private MyService myService;`) became widespread due to its low keystroke overhead, leading to widespread bad testing habits and tight coupling.
- **Spring 4.3+ & Spring Boot 3.x (2016-Present)**: Constructor injection without explicit `@Autowired` became the official enterprise standard. Paired with Java records and immutable architecture, field injection is now classified as an anti-pattern.

### Real-World Relevance
Modern engineering organizations enforce strict linters and ArchUnit rules banning field injection in favor of constructor injection. Understanding how the container resolves parameters at construction time is essential for writing robust, clean enterprise software.

---

## 2. Core Concepts

### 1. Constructor Injection
Dependencies are provided as parameters to the class constructor.
- **Immutability**: Fields can and should be declared `final`.
- **Null Safety**: An object cannot be instantiated in a partially initialized or illegal state.
- **Single Constructor Rule**: Since Spring 4.3, if a class has exactly one constructor, the `@Autowired` annotation is completely optional; Spring automatically detects and injects all constructor parameters.

### 2. Setter Injection
Dependencies are supplied via `public` or package-private setter methods annotated with `@Autowired`.
- **Mutability**: Fields cannot be `final`.
- **Use Case**: Appropriate exclusively for *optional* dependencies or configuration properties that may change dynamically at runtime.

### 3. Field Injection
Dependencies are injected directly into `private` fields using Java Reflection (`Field.setAccessible(true)`), driven by `AutowiredAnnotationBeanPostProcessor`.
- **Flaw**: Violates encapsulation, prevents standard `new MyClass()` unit testing without Spring or Mockito reflection hacks, and obscures code bloat.

### 4. Dynamic Collection Injection (Strategy Pattern)
Spring natively supports injecting collections of beans implementing a shared interface:
- `List<PaymentStrategy>`: Injects all beans implementing `PaymentStrategy`, ordered by `@Order`.
- `Map<String, PaymentStrategy>`: Injects a map where keys are bean names and values are the corresponding bean instances.

---

## 3. How It Works Internally

### Constructor vs. Field Injection Internal Lifecycle

```
A. CONSTRUCTOR INJECTION FLOW
+-------------------------------------------------------------------------+
|                      1. Target Bean Instantiation                       |
|  - Spring inspects candidate constructors via reflection                |
|  - Identifies single constructor or constructor with @Autowired        |
|  - Resolves required collaborator beans from the container              |
|  - Calls constructor: `new OrderService(orderRepository, auditService)` |
|  - RESULT: Object is immediately fully initialized and immutable!      |
+-------------------------------------------------------------------------+

B. FIELD INJECTION FLOW
+-------------------------------------------------------------------------+
|                  1. Default No-Arg Constructor Called                   |
|  - Calls `new OrderService()`                                           |
|  - Fields are initially NULL                                            |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|             2. AutowiredAnnotationBeanPostProcessor Invoked             |
|  - Scans for `@Autowired` annotations on fields                         |
|  - Uses reflection: `field.setAccessible(true)`                         |
|  - Injects collaborator references                                      |
|  - WARNING: If tested outside Spring via `new`, fields remain NULL!     |
+-------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### Modern Constructor Injection (Idiomatic Java 21)
```java
@Service
public class OrderService {

    private final OrderRepository orderRepository;
    private final NotificationService notificationService;

    // Single constructor: @Autowired is completely optional
    public OrderService(OrderRepository orderRepository, NotificationService notificationService) {
        this.orderRepository = Objects.requireNonNull(orderRepository);
        this.notificationService = Objects.requireNonNull(notificationService);
    }
}
```

### Setter Injection for Optional Configuration
```java
@Service
public class ReportService {

    private Formatter formatter = new DefaultTextFormatter(); // Default fallback

    @Autowired(required = false)
    public void setFormatter(Formatter formatter) {
        this.formatter = formatter;
    }
}
```

---

## 5. Practical Examples

### Example 1: Constructor Injection with Java 21 Record DTOs and Services
```java
package com.example.demo.di;

import org.springframework.stereotype.Service;
import java.time.Instant;
import java.util.Objects;
import java.util.UUID;

public record Invoice(UUID id, String customerId, double amount, Instant issuedAt) {}

interface BillingRepository {
    void save(Invoice invoice);
}

@Service
public class BillingService {

    private final BillingRepository repository;

    // Constructor injection guarantees repository is never null
    public BillingService(BillingRepository repository) {
        this.repository = Objects.requireNonNull(repository, "repository must not be null");
    }

    public Invoice issueInvoice(String customerId, double amount) {
        Invoice invoice = new Invoice(UUID.randomUUID(), customerId, amount, Instant.now());
        repository.save(invoice);
        return invoice;
    }
}
```

### Example 2: Strategy Pattern with Dynamic Collection & Map Injection
```java
package com.example.demo.strategy;

import org.springframework.stereotype.Component;
import org.springframework.stereotype.Service;

import java.util.List;
import java.util.Map;

// Strategy Interface
public interface NotificationChannel {
    String channelName();
    void send(String recipient, String message);
}

@Component("emailChannel")
class EmailChannel implements NotificationChannel {
    @Override public String channelName() { return "email"; }
    @Override public void send(String recipient, String message) {
        System.out.println("Email sent to " + recipient + ": " + message);
    }
}

@Component("smsChannel")
class SmsChannel implements NotificationChannel {
    @Override public String channelName() { return "sms"; }
    @Override public void send(String recipient, String message) {
        System.out.println("SMS sent to " + recipient + ": " + message);
    }
}

@Service
public class NotificationRouter {

    // Spring automatically populates Map<beanName, BeanInstance>
    private final Map<String, NotificationChannel> channels;

    public NotificationRouter(Map<String, NotificationChannel> channels) {
        this.channels = channels;
    }

    public void dispatch(String channelBeanName, String recipient, String message) {
        NotificationChannel channel = channels.get(channelBeanName);
        if (channel == null) {
            throw new IllegalArgumentException("Unsupported channel: " + channelBeanName);
        }
        channel.send(recipient, message);
    }
}
```

### Example 3: Handling Optional Collaborators via `Optional<T>`
```java
package com.example.demo.optional;

import org.springframework.stereotype.Service;
import java.util.Optional;

interface MetricsPublisher {
    void publishMetric(String metricName, double value);
}

@Service
public class InventoryService {

    private final Optional<MetricsPublisher> metricsPublisher;

    // Optional<T> explicitly documents that MetricsPublisher may or may not be defined in context
    public InventoryService(Optional<MetricsPublisher> metricsPublisher) {
        this.metricsPublisher = metricsPublisher;
    }

    public void deductStock(String productId, int quantity) {
        System.out.println("Deducted " + quantity + " units of " + productId);
        
        // Clean conditional invocation without null checks
        metricsPublisher.ifPresent(p -> p.publishMetric("stock.deductions", quantity));
    }
}
```

### Example 4: Field Injection Anti-Pattern & Clean Unit Test Remediation
```java
// ANTI-PATTERN: Field Injection hides dependencies and breaks plain unit tests
@Service
public class BadUserService {
    @Autowired
    private UserRepository userRepository; // Cannot be final, hidden coupling

    public boolean userExists(String email) {
        return userRepository.findByEmail(email).isPresent();
    }
}

// WHY IT FAILS IN TESTS:
// BadUserService service = new BadUserService();
// service.userExists("test@example.com"); // THROWS NullPointerException! userRepository is null!

// REMEDIATION: Clean Constructor Injection with Pure Unit Test
@Service
public class CleanUserService {
    private final UserRepository userRepository;

    public CleanUserService(UserRepository userRepository) {
        this.userRepository = userRepository;
    }

    public boolean userExists(String email) {
        return userRepository.findByEmail(email).isPresent();
    }
}

// CLEAN UNIT TEST (Zero Spring overhead, executes in 2 milliseconds):
class CleanUserServiceTest {
    @Test
    void shouldReturnTrueWhenUserExists() {
        UserRepository mockRepo = org.mockito.Mockito.mock(UserRepository.class);
        org.mockito.Mockito.when(mockRepo.findByEmail("alice@corp.com"))
            .thenReturn(Optional.of(new User("alice@corp.com")));

        CleanUserService service = new CleanUserService(mockRepo);
        org.junit.jupiter.api.Assertions.assertTrue(service.userExists("alice@corp.com"));
    }
}
```

### Example 5: Multiple Constructors with Explicit `@Autowired`
```java
package com.example.demo.constructors;

import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.stereotype.Service;

@Service
public class CacheService {

    private final String cacheCluster;
    private final int ttlSeconds;

    // When multiple constructors exist, mark the one Spring must use with @Autowired
    @Autowired
    public CacheService(String cacheCluster) {
        this(cacheCluster, 3600); // Default TTL 1 hour
    }

    // Secondary constructor for manual programmatic instantiation or testing
    public CacheService(String cacheCluster, int ttlSeconds) {
        this.cacheCluster = cacheCluster;
        this.ttlSeconds = ttlSeconds;
    }

    public String getCluster() { return cacheCluster; }
    public int getTtl() { return ttlSeconds; }
}
```

---

## 6. Best Practices

1. **Mandate Constructor Injection**: Standardize on constructor injection across all production code. Mark fields `final` to ensure thread-safe immutability.
2. **Omit `@Autowired` on Single Constructors**: Do not clutter classes with `@Autowired` if there is only one constructor. Spring handles it implicitly.
3. **Ban Field Injection via Linters / ArchUnit**: Add an ArchUnit test in CI to reject any commit containing field `@Autowired`.
4. **Use `Optional<T>` for Optional Dependencies**: Avoid `@Autowired(required = false)` on fields; wrap constructor arguments in `java.util.Optional<T>` to make optionality explicit in the type signature.
5. **Leverage Map / List Injection for Strategy Pattern**: Inject `Map<String, StrategyInterface>` to eliminate verbose `switch` statements or manual factory maps.
6. **Keep Constructor Parameter Count $\le 5$**: A constructor requiring 6 or more arguments is a major design smell indicating that the class violates the Single Responsibility Principle. Refactor into smaller, focused delegates.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Field injection causing NPE in unit tests** | Instantiating the class with `new` leaves `@Autowired` fields null, resulting in `NullPointerException`. | Refactor to constructor injection so mocks can be passed directly via constructor. |
| **Circular dependency deadlocks with constructor DI** | Bean A requires Bean B in constructor; Bean B requires Bean A in constructor. Startup crashes with `BeanCurrentlyInCreationException`. | Decouple design using domain events (`ApplicationEventPublisher`) or refactor shared logic into Bean C. |
| **Multiple constructors without `@Autowired`** | Spring cannot guess which constructor to invoke and throws `NoUniqueBeanDefinitionException`. | Place `@Autowired` explicitly on the primary constructor intended for Spring. |
| **Modifying setter-injected fields concurrently** | Non-final fields exposed to setters can be mutated by multiple threads, leading to race conditions. | Make fields `final` via constructor injection. |
| **Injecting concrete implementations instead of interfaces** | Limits testing and prevents Spring from applying JDK Dynamic Proxies. | Inject the interface type (e.g., `PaymentService`), not the implementation. |
| **Ambiguity in collection injection** | Injecting `List<CommonInterface>` when no beans implement the interface results in an empty list, not a startup error. | Add verification in constructor if at least one bean is strictly mandatory. |

---

## 8. Performance & Security Notes

### Reflection & Memory Overhead
Field injection requires the JVM to violate encapsulation via reflection (`field.setAccessible(true)`), which incurs reflection caching overhead in Java's Module System (JPMS). Constructor injection uses standard JVM bytecode invocation (`invokespecial`), maximizing JIT compiler optimization and escape analysis.

### Security Guarantees
Immutable classes with `final` fields prevent unauthorized mutation via malicious reflection or concurrency bugs after initialization.

---

## 9. Comparison / When to Use What

| Criteria | Constructor Injection | Setter Injection | Field Injection |
| :--- | :--- | :--- | :--- |
| **Immutability (`final` fields)** | Yes (Enforced) | No | No |
| **Unit Testability (without Spring)** | Exceptional (`new MyClass(mock)`) | Good (calls `setter`) | Very Poor (requires reflection) |
| **Null Safety** | Compile-time guaranteed | Runtime only | Runtime only |
| **Circular Dependency Detection** | Fails fast on startup | Allows cycles (risky) | Allows cycles (risky) |
| **Best Practice Recommendation** | **Mandatory Standard** | Rare (optional settings) | **Banned / Anti-Pattern** |

---

## 10. Interview Questions

<details>
<summary><strong>Q1: Why is Constructor Injection strongly favored over Field Injection in modern Spring?</strong></summary>

**Answer:**  
1. **Immutability**: Fields can be declared `final`, making beans thread-safe and immutable.
2. **Null Safety**: Guarantees that objects are fully initialized upon construction; impossible to have partially constructed states.
3. **Pure Unit Testing**: Classes can be tested with standard Java `new MyClass(mockCollaborator)` without needing Spring test contexts, reflection, or Mockito runner.
4. **SRP Adherence**: A class with too many constructor parameters immediately signals excessive responsibilities.
</details>

<details>
<summary><strong>Q2: When is the `@Autowired` annotation optional on a constructor?</strong></summary>

**Answer:**  
Since Spring 4.3, if a class defines **exactly one constructor**, the `@Autowired` annotation is completely optional. Spring automatically treats that sole constructor as the autowired injection point. If a class has two or more constructors, one must be explicitly annotated with `@Autowired` to inform the container which constructor to invoke.
</details>

<details>
<summary><strong>Q3: How does Spring resolve dependencies when injecting a `Map<String, T>`?</strong></summary>

**Answer:**  
Spring inspects the generic type `T`. It discovers all beans in the `ApplicationContext` that implement or extend type `T`, and constructs a `Map` where each entry's key is the bean's name (ID) in the container and the value is the bean instance. This is ideal for the Strategy Pattern.
</details>

<details>
<summary><strong>Q4: How do you declare an optional dependency using Constructor Injection?</strong></summary>

**Answer:**  
Wrap the constructor parameter in `java.util.Optional<T>`:
```java
public OrderService(Optional<AuditClient> auditClient) {
    this.auditClient = auditClient;
}
```
If an `AuditClient` bean exists in the context, it is wrapped in `Optional.of(bean)`. If none exists, Spring passes `Optional.empty()` instead of failing bootstrap.
</details>

<details>
<summary><strong>Q5: Why can Setter Injection sometimes resolve circular dependencies where Constructor Injection fails?</strong></summary>

**Answer:**  
Constructor injection requires collaborator instances *before* the dependent object can be allocated in memory. If Bean A needs Bean B and Bean B needs Bean A, neither constructor can complete. Setter injection, however, instantiates both objects first via default constructors, registers their raw references into Spring's three-level singleton cache, and invokes setters afterward.
</details>

<details>
<summary><strong>Q6: What is the primary operational use case for Setter Injection in modern applications?</strong></summary>

**Answer:**  
Setter injection is appropriate for optional dependencies or mutable configuration parameters that have sensible default values and may need to be reconfigured dynamically at runtime (e.g., via JMX or actuator endpoints) without rebuilding the bean instance.
</details>

<details>
<summary><strong>Q7: What post-processor is responsible for processing `@Autowired` annotations?</strong></summary>

**Answer:**  
`AutowiredAnnotationBeanPostProcessor` (`org.springframework.beans.factory.annotation.AutowiredAnnotationBeanPostProcessor`). It scans classes for `@Autowired` and `@Value` annotations on fields, setters, and arbitrary config methods.
</details>

<details>
<summary><strong>Q8: How does Spring determine the order of beans when injecting a `List<MyInterface>`?</strong></summary>

**Answer:**  
Spring orders elements in the injected `List` based on:
1. The `@Order(int)` annotation on the bean classes (lower integer values have higher priority).
2. Implementing the `org.springframework.core.Ordered` interface.
3. `@Priority` annotation from `jakarta.annotation`.
Beans without ordering metadata are placed at the end of the collection in non-deterministic order.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Refactor Field Injection to Constructor Injection (Easy)</strong></summary>

**Task:**  
Take a legacy service class with three `@Autowired private ...` fields. Refactor it to use idiomatic constructor injection with `final` fields and no `@Autowired` annotation.
</details>

<details>
<summary><strong>Exercise 2: Write a Unit Test Without Spring Runner (Easy)</strong></summary>

**Task:**  
Write a JUnit 5 test for the refactored constructor-injected service from Exercise 1 using plain `new MyService(mockA, mockB, mockC)`. Verify that no `@ExtendWith(SpringExtension.class)` is needed.
</details>

<details>
<summary><strong>Exercise 3: Implement Strategy Pattern with Map Injection (Medium)</strong></summary>

**Task:**  
Create an interface `PaymentGateway` with methods `String id()` and `void charge(double amount)`. Implement `StripeGateway` ("stripe") and `PayPalGateway` ("paypal"). Inject `Map<String, PaymentGateway>` into a `CheckoutService` and execute charges dynamically by key.
</details>

<details>
<summary><strong>Exercise 4: Ordered List Injection (Medium)</strong></summary>

**Task:**  
Create a `DataFilter` interface. Implement `SanitizationFilter` (`@Order(1)`), `AuthenticationFilter` (`@Order(2)`), and `LoggingFilter` (`@Order(3)`). Inject `List<DataFilter>` into a pipeline runner and verify they execute in exact numerical order.
</details>

<details>
<summary><strong>Exercise 5: ArchUnit Rule Banning Field Injection (Hard)</strong></summary>

**Task:**  
Write an ArchUnit test asserting that no field across any package under `com.example` has `@Autowired` or `@Inject`. Add a violation to verify that the build fails with a descriptive explanation.
</details>

<details>
<summary><strong>Exercise 6: Dynamic Optional Configuration with Fallback (Hard)</strong></summary>

**Task:**  
Create a service that accepts an `Optional<CustomCacheProvider>`. If present, the service uses the custom cache; if absent, it gracefully falls back to an in-memory `ConcurrentHashMap` without throwing errors.
</details>

---

## 12. Summary / Cheat Sheet

| Injection Style | Syntax Example | Fields Final? | Preferred Use Case |
| :--- | :--- | :---: | :--- |
| **Constructor** | `public Service(Repo r) { this.r = r; }` | **Yes** | **Default for 100% of mandatory dependencies** |
| **Setter** | `@Autowired(required=false) public void setX(X x)` | No | Optional dependencies with defaults |
| **Field** | `@Autowired private Repo r;` | No | **Prohibited / Anti-pattern** |
| **Map Injection** | `public Service(Map<String, Strategy> m)` | **Yes** | Dynamic Strategy Pattern by bean name |
| **List Injection** | `public Service(List<Filter> filters)` | **Yes** | Ordered processing chains (`@Order`) |
| **Optional DI** | `public Service(Optional<Metrics> m)` | **Yes** | Non-mandatory telemetry or external plugins |

---

## 13. Further Reading
- [Official Spring Framework Reference: Dependencies and DI](https://docs.spring.io/spring-framework/reference/core/beans/dependencies.html)
- [Why Field Injection is Evil by Oliver Drotbohm](https://odrotbohm.de/2013/11/why-field-injection-is-evil/)
- [ArchUnit Testing Guide for Architecture Rules](https://www.archunit.org/)
