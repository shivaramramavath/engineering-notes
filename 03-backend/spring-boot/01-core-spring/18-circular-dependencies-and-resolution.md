# Circular Dependencies & Resolution Strategies

> Master the architecture of circular dependencies in Spring: the internal three-level singleton cache of `DefaultSingletonBeanRegistry`, why Spring Boot 2.6+ disables cycles by default, `@Lazy` resolution mechanics, and clean architectural refactoring with events and mediators.

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [IoC Container & ApplicationContext Internals](file:///03-backend/spring-boot/01-core-spring/01-ioc-container-and-application-context.md), [ApplicationContext Internals: refresh()](file:///03-backend/spring-boot/01-core-spring/17-application-context-internals.md) |
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
A circular dependency occurs when Bean A requires Bean B, and Bean B directly or indirectly requires Bean A (`A -> B -> A`, or `A -> B -> C -> A`). In real-world software, circular dependencies are symptoms of architectural rot: a violation of the Single Responsibility Principle (SRP) where two domain concepts are tightly coupled and cannot function independently.

### Historical Evolution
- **Spring 1.x - 5.x**: Spring automatically resolved circular dependencies for setter-injected and field-injected singletons using its three-level cache, masking bad architectural designs for nearly two decades.
- **Spring Boot 2.6 (2021)**: **Circular references were disabled by default** (`spring.main.allow-circular-references=false`). Boot now throws a fatal `BeanCurrentlyInCreationException` on startup if any cycle is detected.
- **Spring 6.x & Boot 3.x**: Ahead-of-Time (AOT) compilation and native GraalVM images strictly mandate zero circular references.

### Real-World Relevance
Engineers must know how Spring detects cycles, how the internal 3-level cache functions, and most importantly, how to systematically refactor circular dependencies using domain events or extraction patterns rather than relying on band-aids.

---

## 2. Core Concepts

### 1. Constructor vs. Setter Cycles
- **Constructor Injection Cycle**: Bean A needs Bean B in its constructor; Bean B needs Bean A in its constructor. **This cannot be resolved by Spring's cache** because neither Java object can be allocated in memory without the other already existing. Throws `BeanCurrentlyInCreationException`.
- **Setter / Field Injection Cycle**: Objects are instantiated via default constructors first. Spring exposes uninitialized "early references" via its cache before injecting dependencies, allowing cycles to resolve (if circular references are permitted).

### 2. Spring Boot's Stance: Zero Circular References
In modern Spring Boot 3.4+, circular references are treated as build-breaking architectural defects. While setting `spring.main.allow-circular-references=true` temporarily bypasses the startup error, it is considered technical debt that blocks GraalVM native image generation.

### 3. Resolution Strategies
1. **Extract Shared Logic (Best Practice)**: Move the shared coupling into an independent third component: `Bean C`.
2. **Domain Events**: Replace direct coupling with decoupled asynchronous or synchronous events (`ApplicationEventPublisher`).
3. **`@Lazy` Injection**: Instructs Spring to inject a CGLIB proxy instead of the real bean at construction time, breaking the initialization cycle.

---

## 3. How It Works Internally

### The Three-Level Cache in `DefaultSingletonBeanRegistry`

```
Level 1: `singletonObjects`       (Map<String, Object>)         -> Fully Initialized Singletons
Level 2: `earlySingletonObjects`  (Map<String, Object>)         -> Early Raw / Proxied References
Level 3: `singletonFactories`     (Map<String, ObjectFactory>)  -> Early Proxy Suppliers (Lambdas)

RESOLVING A SETTER CYCLE (Bean A <---> Bean B):
========================================================================================

1. Context initiates creation of Bean A:
   - Instantiates raw `new BeanA()` (constructor completes).
   - Adds lambda to Level 3 (`singletonFactories.put("beanA", () -> getEarlyBeanReference(beanA))`).

2. Populate Bean A properties:
   - Detects dependency on Bean B.
   - Triggers `getBean("beanB")`.

3. Context initiates creation of Bean B:
   - Instantiates raw `new BeanB()` (constructor completes).
   - Adds lambda to Level 3 (`singletonFactories.put("beanB", ...)`).

4. Populate Bean B properties:
   - Detects dependency on Bean A.
   - Triggers `getBean("beanA")`.

5. Resolving Bean A for Bean B:
   - Cache Check Level 1: Not in `singletonObjects`.
   - Cache Check Level 2: Not in `earlySingletonObjects`.
   - Cache Check Level 3: FOUND in `singletonFactories`!
   - Executes Level 3 ObjectFactory lambda -> creates early CGLIB proxy if AOP is active.
   - Promotes early reference to Level 2 (`earlySingletonObjects`) and removes from Level 3.
   - Injects Level 2 Bean A into Bean B!

6. Bean B completes:
   - Calls `@PostConstruct` on Bean B.
   - Bean B promoted to Level 1 (`singletonObjects`).

7. Bean A finishes:
   - Receives fully initialized Bean B.
   - Bean A promoted to Level 1 (`singletonObjects`).
   - BOTH BEANS READY!
```

---

## 4. Syntax & Usage

### Resolving Constructor Cycle with `@Lazy`
```java
@Service
public class OrderService {
    private final PaymentService paymentService;

    // @Lazy injects a dynamic proxy; PaymentService is not instantiated yet!
    public OrderService(@Lazy PaymentService paymentService) {
        this.paymentService = paymentService;
    }
}
```

---

## 5. Practical Examples

### Example 1: Constructor Circular Dependency Failure
```java
package com.example.demo.circular;

import org.springframework.stereotype.Service;

@Service
public class ServiceA {
    private final ServiceB serviceB;

    // Constructor cycle: A needs B
    public ServiceA(ServiceB serviceB) {
        this.serviceB = serviceB;
    }
}

@Service
class ServiceB {
    private final ServiceA serviceA;

    // Constructor cycle: B needs A
    public ServiceB(ServiceA serviceA) {
        this.serviceA = serviceA;
    }
}

// RESULT ON STARTUP:
// org.springframework.beans.factory.BeanCurrentlyInCreationException: 
// Error creating bean with name 'serviceA': Requested bean is currently in creation: 
// Is there an unresolvable circular reference?
```

### Example 2: Resolving via `@Lazy` on Constructor Argument
```java
package com.example.demo.circular.lazy;

import org.springframework.context.annotation.Lazy;
import org.springframework.stereotype.Service;

@Service
public class ReportService {
    private final AnalyticsService analyticsService;

    // @Lazy generates a CGLIB proxy for AnalyticsService!
    // The JVM can now construct ReportService immediately without needing real AnalyticsService!
    public ReportService(@Lazy AnalyticsService analyticsService) {
        this.analyticsService = analyticsService;
    }

    public void runReport() {
        // Real AnalyticsService is initialized upon first method call on the proxy
        analyticsService.track("Report generated");
    }
}

@Service
class AnalyticsService {
    private final ReportService reportService;

    public AnalyticsService(ReportService reportService) {
        this.reportService = reportService;
    }

    public void track(String event) {
        System.out.println("Tracked event: " + event);
    }
}
```

### Example 3: Resolving Circular Dependency via Domain Events
```java
package com.example.demo.circular.events;

import org.springframework.context.ApplicationEventPublisher;
import org.springframework.context.event.EventListener;
import org.springframework.stereotype.Service;

import java.util.UUID;

// Immutable event decouples both services
public record OrderPlacedEvent(UUID orderId, String customerId) {}

@Service
public class DecoupledOrderService {

    private final ApplicationEventPublisher eventPublisher;

    // Zero dependency on CustomerLoyaltyService!
    public DecoupledOrderService(ApplicationEventPublisher eventPublisher) {
        this.eventPublisher = eventPublisher;
    }

    public void createOrder(UUID orderId, String customerId) {
        System.out.println("Order created: " + orderId);
        // Publishes event instead of calling LoyaltyService directly
        eventPublisher.publishEvent(new OrderPlacedEvent(orderId, customerId));
    }
}

@Service
class CustomerLoyaltyService {

    // Listens for event without OrderService needing to know about LoyaltyService
    @EventListener
    public void onOrderPlaced(OrderPlacedEvent event) {
        System.out.printf("Added loyalty points for customer %s on order %s%n", 
            event.customerId(), event.orderId());
    }
}
```

### Example 4: The Dangerous `allow-circular-references=true` Anti-Pattern
```yaml
# application.yaml
spring:
  main:
    # ANTI-PATTERN: Band-aid setting that masks architectural design rot!
    # FLAW:
    # 1. Breaks GraalVM Ahead-of-Time compilation
    # 2. Hides tightly coupled classes violating Single Responsibility Principle
    # 3. Only works for setter/field injection; DOES NOT FIX constructor injection!
    allow-circular-references: true
```

### Example 5: Proper Architectural Refactoring (Extracting Bean C)
```java
package com.example.demo.circular.refactor;

import org.springframework.stereotype.Service;

// BEFORE REFACTOR:
// UserService calls NotificationService.notify()
// NotificationService calls UserService.getUserDetails() -> CIRCULAR!

// STEP 1: Extract shared state/logic into Bean C (UserRepository)
@Service
class UserProfileRepository {
    public String getUserEmail(String userId) {
        return userId + "@corp.internal";
    }
}

// STEP 2: NotificationService depends ONLY on UserProfileRepository
@Service
class CleanNotificationService {
    private final UserProfileRepository userProfileRepo;

    public CleanNotificationService(UserProfileRepository userProfileRepo) {
        this.userProfileRepo = userProfileRepo;
    }

    public void sendEmail(String userId, String msg) {
        String email = userProfileRepo.getUserEmail(userId);
        System.out.printf("Dispatched '%s' to %s%n", msg, email);
    }
}

// STEP 3: CleanUserService depends on both without any circular loops!
@Service
public class CleanUserService {
    private final UserProfileRepository userProfileRepo;
    private final CleanNotificationService notificationService;

    public CleanUserService(UserProfileRepository userProfileRepo, CleanNotificationService notificationService) {
        this.userProfileRepo = userProfileRepo;
        this.notificationService = notificationService;
    }

    public void register(String userId) {
        System.out.println("Registering: " + userId);
        notificationService.sendEmail(userId, "Welcome aboard!");
    }
}
```

---

## 6. Best Practices

1. **Never Enable `allow-circular-references=true`**: Treat circular references as architectural compilation errors. Refactor the design immediately.
2. **Extract Shared Responsibilities (Bean C)**: When two services depend on each other, identify the shared data or behavior and extract it into a dedicated repository, helper, or domain service.
3. **Use Domain Events for Side-Effects**: If Service A calls Service B merely to trigger a notification, cache eviction, or secondary workflow, replace the direct call with an `ApplicationEventPublisher.publishEvent()`.
4. **Use `@Lazy` Only as an Emergency Temporary Hotfix**: While `@Lazy` resolves the startup deadlock, it incurs CGLIB proxy overhead and preserves the underlying architectural coupling.
5. **Enforce Single Responsibility Principle (SRP)**: Circular dependencies almost always indicate that a class is doing too much. Splitting classes into focused domain components eliminates cycles naturally.
6. **Detect Cycles in CI via ArchUnit**: Write an ArchUnit test asserting that no package cycles exist (`slices().matching("com.example.(*)..").should().beFreeOfCycles()`).

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Using `allow-circular-references` as a permanent fix** | Masks technical debt, breaks GraalVM native image compilation, and prevents modularization. | Refactor code using Domain Events or extract Bean C. |
| **Expecting 3-level cache to fix constructor cycles** | Constructors require parameters before memory allocation; the cache cannot expose an object that has not started construction. | Use `@Lazy` on constructor parameter or refactor to events. |
| **Circular calls between `@Async` methods** | Calling an `@Async` method on a circular collaborator can trigger thread deadlocks or premature proxy exceptions. | Decouple using asynchronous event listeners or message queues. |
| **AOP Proxies in circular dependencies** | If a bean in a cycle is proxied (e.g., `@Transactional`), the early reference exposed must match the final proxy, otherwise `BeanCurrentlyInCreationException` is thrown. | The 3-level cache handles this via `SmartInstantiationAwareBeanPostProcessor.getEarlyBeanReference()`. |
| **Bidirectional JPA entity relationships causing JSON cycles** | Jackson serializing bidirectional JPA entities (`@OneToMany` <-> `@ManyToOne`) causes `StackOverflowError`. | Use `@JsonIgnore` or return immutable DTO records instead of JPA entities. |
| **Circular dependency between `@Configuration` classes** | Config class A imports B which imports A; breaks bean definition parsing. | Consolidate configurations or extract shared bean definitions. |

---

## 8. Performance & Security Notes

### Deadlock Risks in Concurrent Context Bootstrap
When multiple threads query the `DefaultSingletonBeanRegistry` while circular dependencies are being resolved, threads can lock Level 1 while waiting for Level 3 objects, triggering JVM-level monitor deadlocks. Eliminating circular references guarantees deterministic, single-pass bean initialization.

### Memory Overhead of Early Proxies
Level 3 `singletonFactories` invoke `SmartInstantiationAwareBeanPostProcessor` early, generating proxy classes before standard bean initialization. This bypasses normal lifecycle ordering and increases Metaspace usage.

---

## 9. Comparison / When to Use What

| Resolution Strategy | Architectural Purity | GraalVM Native Friendly? | Implementation Effort | Best For |
| :--- | :---: | :---: | :--- | :--- |
| **Extract Bean C** | **Highest (Clean SRP)**| **Yes (100%)** | Low-Medium | Shared state/business logic |
| **Domain Events** | **Highest (Decoupled)**| **Yes (100%)** | Low | Post-actions, notifications, audits |
| **`@Lazy` on Parameter** | Low (Hides coupling) | Yes (With proxy hints) | Instant (One annotation)| Emergency hotfix in legacy systems |
| **`allow-circular-references`**| **Zero (Anti-pattern)**| **NO (Fails in AOT)** | Zero (Config flag) | **Prohibited in modern Boot 3+** |

---

## 10. Interview Questions

<details>
<summary><strong>Q1: Why does Spring Boot 2.6+ disable circular references by default?</strong></summary>

**Answer:**  
Because circular references are an architectural anti-pattern violating the Single Responsibility Principle. They make code brittle, hard to test, and difficult to reason about. Disabling them by default forces developers to design clean, modular architectures and is mandatory for Ahead-of-Time (AOT) GraalVM native image compilation.
</details>

<details>
<summary><strong>Q2: Why can't Spring's three-level cache resolve circular dependencies with Constructor Injection?</strong></summary>

**Answer:**  
Spring's three-level cache relies on instantiating the raw Java object first via its constructor and then exposing that raw reference in Level 3 before populating its properties. With constructor injection, Java requires collaborator arguments *before* the constructor can even be called. If Bean A needs Bean B to call `new BeanA(b)`, and Bean B needs Bean A to call `new BeanB(a)`, neither object can be instantiated, and Java throws a circular dependency failure before Spring's cache can intervene.
</details>

<details>
<summary><strong>Q3: What are the three levels of the singleton cache in `DefaultSingletonBeanRegistry`?</strong></summary>

**Answer:**  
1. **Level 1 (`singletonObjects`)**: Map storing fully created, initialized, and post-processed singletons.
2. **Level 2 (`earlySingletonObjects`)**: Map storing partially created singletons (exposed before property population/init).
3. **Level 3 (`singletonFactories`)**: Map storing `ObjectFactory<?>` suppliers that dynamically produce the early bean reference (generating early AOP proxies if required).
</details>

<details>
<summary><strong>Q4: How does `@Lazy` resolve a constructor circular dependency?</strong></summary>

**Answer:**  
When `@Lazy` is placed on a constructor parameter (`public ServiceA(@Lazy ServiceB b)`), Spring does not instantiate `ServiceB` immediately. Instead, it generates a dynamic CGLIB proxy for `ServiceB` and injects that proxy into `ServiceA`. `ServiceA` finishes instantiation immediately. When `ServiceA` eventually calls a method on `serviceB`, the proxy intercepts the call and fetches the real `ServiceB` instance from the container on demand.
</details>

<details>
<summary><strong>Q5: What is the recommended architectural solution for a circular dependency between two services?</strong></summary>

**Answer:**  
1. **Extracting a Third Bean (Bean C)**: Identify the common methods or data that both services need, extract them into a new service/repository (`Bean C`), and have both original services depend on `Bean C`.
2. **Domain Events**: If the circular call is one-way notification/side-effect, publish an `ApplicationEvent` instead of holding a direct reference to the collaborator.
</details>

<details>
<summary><strong>Q6: How does the three-level cache ensure that AOP proxies are not created twice for circular beans?</strong></summary>

**Answer:**  
When an early reference is requested from Level 3, the `ObjectFactory` delegates to `SmartInstantiationAwareBeanPostProcessor.getEarlyBeanReference()`. If the bean requires an AOP proxy (e.g., `@Transactional`), the proxy is created immediately and stored in Level 2 (`earlySingletonObjects`). Later, when the bean reaches normal initialization, `postProcessAfterInitialization()` detects that the proxy was already created and returns the existing proxy rather than creating a duplicate.
</details>

<details>
<summary><strong>Q7: What exception does Spring throw when a circular dependency cannot be resolved?</strong></summary>

**Answer:**  
`org.springframework.beans.factory.BeanCurrentlyInCreationException`.
</details>

<details>
<summary><strong>Q8: Can ArchUnit be used to prevent circular dependencies at compile/test time?</strong></summary>

**Answer:**  
Yes. ArchUnit includes a built-in cycle detection rule:
```java
slices().matching("com.example.(*)..").should().beFreeOfCycles();
```
If an engineer introduces a circular package dependency, the JUnit build immediately fails in CI.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Reproduce `BeanCurrentlyInCreationException` (Easy)</strong></summary>

**Task:**  
Create two services with mutual constructor injection. Boot the application and observe the exact `BeanCurrentlyInCreationException` message and stack trace in the console.
</details>

<details>
<summary><strong>Exercise 2: Temporary Fix with `@Lazy` (Easy)</strong></summary>

**Task:**  
Add `@Lazy` to one constructor parameter in Exercise 1. Verify that the application boots successfully and that both beans can invoke methods on each other.
</details>

<details>
<summary><strong>Exercise 3: Trace Three-Level Cache with Debugger (Medium)</strong></summary>

**Task:**  
Enable setter injection cycles with `spring.main.allow-circular-references=true`. Place a breakpoint inside `DefaultSingletonBeanRegistry.getSingleton(String, boolean)`. Trace how Level 3 and Level 2 caches are populated during startup.
</details>

<details>
<summary><strong>Exercise 4: Refactor Cycle Using Domain Events (Medium)</strong></summary>

**Task:**  
Take two circular services (`OrderService` and `BillingService`). Refactor the dependency from `OrderService -> BillingService` into an `ApplicationEventPublisher.publishEvent(new OrderCompletedEvent(...))` pattern. Remove the circular dependency entirely.
</details>

<details>
<summary><strong>Exercise 5: Refactor Cycle by Extracting Bean C (Hard)</strong></summary>

**Task:**  
Given `UserService` (needs `NotificationService`) and `NotificationService` (needs `UserService` for user email), refactor the architecture by creating an extracted `UserContactInfoRepository` that both services depend on. Verify zero circular dependencies remain.
</details>

<details>
<summary><strong>Exercise 6: ArchUnit Cycle Prevention Test (Hard)</strong></summary>

**Task:**  
Write an ArchUnit test asserting that all packages under `com.example` are strictly free of cycles. Intentionally create a circular dependency between two packages and assert that the test fails.
</details>

---

## 12. Summary / Cheat Sheet

| Cycle Type | Resolvable by 3-Level Cache? | Spring Boot Default Behavior | Permanent Solution |
| :--- | :---: | :--- | :--- |
| **Constructor Cycle** | **NO** | Throws `BeanCurrentlyInCreationException` | Extract Bean C / Domain Events |
| **Setter / Field Cycle** | Yes (if enabled) | **Disabled by default** (Throws exception) | Extract Bean C / Domain Events |
| **`@Lazy` Workaround** | Yes | Creates dynamic CGLIB proxy | Temporary hotfix only |
| **`allow-circular-refs`**| Yes (Setter only) | Prohibited in modern microservices | **Banned anti-pattern** |

---

## 13. Further Reading
- [Official Spring Framework Reference: Circular Dependencies](https://docs.spring.io/spring-framework/reference/core/beans/dependencies/factory-collaborators.html#beans-dependency-resolution)
- [Spring Boot 2.6 Release Notes: Circular References Disabled by Default](https://github.com/spring-projects/spring-boot/wiki/Spring-Boot-2.6-Release-Notes#circular-references-prohibited-by-default)
- [ArchUnit Slices and Cycle Detection](https://www.archunit.org/user-guide/html/000_Index.html#_cycle_checks)
