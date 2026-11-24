# Bean Scopes, Lifecycle Callbacks & Scoped Proxies

> A deep architectural investigation of Spring bean scopes (`singleton`, `prototype`, web scopes), the complete 12-step bean lifecycle, the prototype-in-singleton dependency mismatch problem, scoped proxies, and custom scope implementation.

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [IoC Container & ApplicationContext Internals](file:///03-backend/spring-boot/01-core-spring/01-ioc-container-and-application-context.md), [Spring Beans & BeanDefinitions](file:///03-backend/spring-boot/01-core-spring/02-beans-and-bean-definitions.md) |
| **Time to Read** | 30 minutes |
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
In enterprise applications, components have vastly different operational lifetimes. A database connection pool or REST client should exist as a single shared instance throughout the entire application lifecycle, whereas a user's shopping cart or security token must be isolated per HTTP session or HTTP request. Furthermore, components often require precise startup initialization (pre-allocating buffers, establishing sockets) and guaranteed shutdown teardown (flushing caches, draining queues).

### Historical Evolution
- **Spring 1.x**: Supported only `singleton` and `prototype` scopes via XML.
- **Spring 2.0**: Added web-aware scopes (`request`, `session`, `globalSession`) and the `Scope` SPI for custom scopes.
- **Spring 2.5 - 4.x**: Standardized on JSR-250 (`@PostConstruct`, `@PreDestroy`) and introduced `@Scope` annotations.
- **Spring 6.x & Boot 3.x**: Migrated lifecycle annotations to `jakarta.annotation.*` and optimized scoped proxies for high-throughput virtual threads.

### Real-World Relevance
Mishandling scopes—specifically injecting short-lived prototype or request beans into long-lived singletons without scoped proxies—is one of the most widespread causes of concurrency bugs, state leakage between users, and severe memory exhaustion in production.

---

## 2. Core Concepts

### 1. Spring Bean Scopes
- **`singleton` (Default)**: Exactly one shared instance is created per `ApplicationContext`. All requests for the bean return the cached singleton.
- **`prototype`**: A brand new instance is created every time the bean is requested from the container or injected into a collaborator.
- **`request`**: Exactly one instance per HTTP request lifecycle (web applications only).
- **`session`**: Exactly one instance per HTTP `HttpSession` lifecycle (web applications only).
- **`application`**: One instance per `ServletContext` (web applications only).
- **`websocket`**: One instance per WebSocket session.

### 2. The 12-Step Bean Lifecycle
When an `ApplicationContext` initializes a bean, it executes a strict sequence:
1. Instantiation (Constructor call)
2. Populate Properties (Dependency Injection)
3. BeanNameAware / BeanClassLoaderAware / BeanFactoryAware callbacks
4. `ApplicationContextAware` callbacks
5. `BeanPostProcessor.postProcessBeforeInitialization()`
6. `@PostConstruct` annotated methods
7. `InitializingBean.afterPropertiesSet()`
8. Custom `initMethod` (from `@Bean(initMethod="...")`)
9. `BeanPostProcessor.postProcessAfterInitialization()` (AOP proxies created here)
10. *Bean is active and serving traffic*
11. `@PreDestroy` annotated methods
12. `DisposableBean.destroy()` / Custom `destroyMethod`

### 3. The Prototype-in-Singleton Problem
When a `prototype` bean is injected into a `singleton` bean, the injection happens **only once** when the singleton is constructed. Consequently, the singleton holds the same prototype instance forever, effectively turning the prototype into a singleton!  
**Solutions**:
- **Scoped Proxies (`proxyMode = ScopedProxyMode.TARGET_CLASS`)**: Spring injects a dynamic CGLIB proxy that routes each method call to a newly resolved prototype or request instance.
- **`ObjectFactory<T>` / `Provider<T>`**: Injects a factory that calls `getObject()` whenever a fresh instance is needed.
- **`@Lookup` Method Injection**: Spring dynamically overrides an abstract method to fetch the prototype from the container.

---

## 3. How It Works Internally

### Scoped Proxy Dispatch: Short-Lived Bean inside Long-Lived Singleton

```
+-------------------------------------------------------------------------+
|                  Singleton Service (e.g. OrderService)                  |
|                                                                         |
|  Holds reference to: `UserSessionContext` (Scope: session / request)   |
+------------------------------------+------------------------------------+
                                     |
                                     | Method Call: `userSession.getUserId()`
                                     v
+-------------------------------------------------------------------------+
|             Scoped Proxy (CGLIB Subclass: UserSessionContext$$Spring)   |
|  - Intercepts method call                                               |
|  - Queries `RequestContextHolder.currentRequestAttributes()`            |
|  - Retrieves current thread's specific session / request attributes     |
+------------------------------------+------------------------------------+
                                     |
                                     | Fetches instance for Current Thread
                                     v
+-------------------------------------------------------------------------+
|             Actual Target Instance (Thread-Specific / Session-Specific)  |
|  - Thread A (User Alice)   --> `UserSessionContext (alice)`             |
|  - Thread B (User Bob)     --> `UserSessionContext (bob)`               |
|                                                                         |
|  Executes business method and returns result to OrderService            |
+-------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### Declaring Scopes
```java
// Prototype Scope
@Component
@Scope(ConfigurableBeanFactory.SCOPE_PROTOTYPE)
public class TaskWorker {}

// Web Request Scope with Scoped Proxy
@Component
@Scope(value = WebApplicationContext.SCOPE_REQUEST, proxyMode = ScopedProxyMode.TARGET_CLASS)
public class RequestContext {
    private String traceId;
    // Getters and setters
}
```

### Lifecycle Hooks (Jakarta EE Standard)
```java
@Component
public class CacheEngine {

    @jakarta.annotation.PostConstruct
    public void init() {
        // Runs immediately after dependencies are injected
        System.out.println("Cache initialized");
    }

    @jakarta.annotation.PreDestroy
    public void cleanup() {
        // Runs on context shutdown
        System.out.println("Cache flushed and destroyed");
    }
}
```

---

## 5. Practical Examples

### Example 1: Comprehensive Lifecycle Callbacks Demonstration
```java
package com.example.demo.lifecycle;

import jakarta.annotation.PostConstruct;
import jakarta.annotation.PreDestroy;
import org.springframework.beans.BeansException;
import org.springframework.beans.factory.BeanNameAware;
import org.springframework.beans.factory.DisposableBean;
import org.springframework.beans.factory.InitializingBean;
import org.springframework.context.ApplicationContext;
import org.springframework.context.ApplicationContextAware;
import org.springframework.stereotype.Component;

@Component
public class ManagedResourceBean implements 
    BeanNameAware, 
    ApplicationContextAware, 
    InitializingBean, 
    DisposableBean {

    private String beanName;
    private ApplicationContext context;

    public ManagedResourceBean() {
        System.out.println("1. Constructor: Bean allocated in memory");
    }

    @Override
    public void setBeanName(String name) {
        this.beanName = name;
        System.out.println("2. BeanNameAware: Injected bean name = " + name);
    }

    @Override
    public void setApplicationContext(ApplicationContext context) throws BeansException {
        this.context = context;
        System.out.println("3. ApplicationContextAware: Injected ApplicationContext");
    }

    @PostConstruct
    public void postConstruct() {
        System.out.println("4. @PostConstruct: Executing standard post-construct hook");
    }

    @Override
    public void afterPropertiesSet() {
        System.out.println("5. InitializingBean: afterPropertiesSet() invoked");
    }

    public void customInit() {
        System.out.println("6. Custom initMethod: Executed if configured on @Bean");
    }

    @PreDestroy
    public void preDestroy() {
        System.out.println("7. @PreDestroy: Starting cleanup before destruction");
    }

    @Override
    public void destroy() {
        System.out.println("8. DisposableBean: destroy() invoked on context close");
    }
}
```

### Example 2: Scoped Proxy Resolving Web Request Bean in Singleton
```java
package com.example.demo.scopedproxy;

import org.springframework.context.annotation.Scope;
import org.springframework.context.annotation.ScopedProxyMode;
import org.springframework.stereotype.Component;
import org.springframework.stereotype.Service;
import org.springframework.web.context.WebApplicationContext;

import java.util.UUID;

// Request-scoped bean with dynamic CGLIB proxy
@Component
@Scope(value = WebApplicationContext.SCOPE_REQUEST, proxyMode = ScopedProxyMode.TARGET_CLASS)
public class RequestUserContext {
    private final UUID requestId = UUID.randomUUID();
    private String userId;

    public UUID getRequestId() { return requestId; }
    public String getUserId() { return userId; }
    public void setUserId(String userId) { this.userId = userId; }
}

// Singleton Service safely injecting request-scoped bean
@Service
public class AuditLogger {

    // Safely injects the CGLIB proxy! Each HTTP thread resolves its own RequestUserContext instance.
    private final RequestUserContext userContext;

    public AuditLogger(RequestUserContext userContext) {
        this.userContext = userContext;
    }

    public void logAction(String action) {
        System.out.println("Audit [Request: " + userContext.getRequestId() + 
                           ", User: " + userContext.getUserId() + "]: " + action);
    }
}
```

### Example 3: Prototype Injection via `ObjectFactory<T>`
```java
package com.example.demo.factory;

import org.springframework.beans.factory.ObjectFactory;
import org.springframework.beans.factory.config.ConfigurableBeanFactory;
import org.springframework.context.annotation.Scope;
import org.springframework.stereotype.Component;
import org.springframework.stereotype.Service;

import java.time.Instant;
import java.util.UUID;

@Component
@Scope(ConfigurableBeanFactory.SCOPE_PROTOTYPE)
public class ReportJob {
    private final UUID jobId = UUID.randomUUID();
    private final Instant created = Instant.now();

    public void execute() {
        System.out.println("Executing Job " + jobId + " created at " + created);
    }
}

@Service
public class ReportScheduler {

    // ObjectFactory allows on-demand creation of prototype beans
    private final ObjectFactory<ReportJob> reportJobFactory;

    public ReportScheduler(ObjectFactory<ReportJob> reportJobFactory) {
        this.reportJobFactory = reportJobFactory;
    }

    public void scheduleRun() {
        // Calling getObject() returns a brand-new ReportJob instance every time
        ReportJob job1 = reportJobFactory.getObject();
        ReportJob job2 = reportJobFactory.getObject();

        job1.execute();
        job2.execute();
    }
}
```

### Example 4: Leaky Prototype Anti-Pattern & Remediation
```java
// ANTI-PATTERN: Injecting a Prototype directly into a Singleton without proxy or factory
@Component
@Scope("prototype")
public class TokenValidator {
    private Instant issuedAt = Instant.now();
    public Instant getIssuedAt() { return issuedAt; }
}

@Service // SINGLETON
public class BadAuthService {
    @Autowired
    private TokenValidator validator; // FLAW: Injected ONCE on application startup!

    public void check() {
        // ALWAYS prints the same timestamp from startup, never refreshes!
        System.out.println("Validator issued at: " + validator.getIssuedAt());
    }
}

// REMEDIATION: Method Injection using @Lookup
@Service
public abstract class CleanAuthService {

    // Spring overrides this abstract method at runtime to fetch a new prototype from context
    @org.springframework.beans.factory.annotation.Lookup
    public abstract TokenValidator getTokenValidator();

    public void check() {
        TokenValidator freshValidator = getTokenValidator(); // Fresh instance on every call
        System.out.println("Fresh Validator issued at: " + freshValidator.getIssuedAt());
    }
}
```

### Example 5: Implementing a Custom ThreadScope
```java
package com.example.demo.customscope;

import org.springframework.beans.factory.ObjectFactory;
import org.springframework.beans.factory.config.Scope;

import java.util.HashMap;
import java.util.Map;

// Custom Thread-Local Scope
public class SimpleThreadScope implements Scope {

    private final ThreadLocal<Map<String, Object>> threadScope = 
        ThreadLocal.withInitial(HashMap::new);

    @Override
    public Object get(String name, ObjectFactory<?> objectFactory) {
        Map<String, Object> scope = threadScope.get();
        return scope.computeIfAbsent(name, k -> objectFactory.getObject());
    }

    @Override
    public Object remove(String name) {
        return threadScope.get().remove(name);
    }

    @Override
    public void registerDestructionCallback(String name, Runnable callback) {
        // ThreadLocal scopes typically rely on thread exit or manual clear
    }

    @Override
    public Object resolveContextualObject(String key) {
        return null;
    }

    @Override
    public String getConversationId() {
        return Thread.currentThread().getName();
    }

    public void clear() {
        threadScope.remove(); // Prevent memory leaks
    }
}
```

---

## 6. Best Practices

1. **Default to Singleton**: 98% of enterprise Spring beans should be stateless singletons. They minimize garbage collection, maximize memory efficiency, and scale predictably.
2. **Never Inject Raw Prototype/Request Beans into Singletons**: Always use `proxyMode = ScopedProxyMode.TARGET_CLASS`, `ObjectFactory<T>`, or `@Lookup` to prevent state leakage and concurrency corruption.
3. **Use `@PostConstruct` and `@PreDestroy`**: Prefer standard `jakarta.annotation` lifecycle annotations over implementing Spring-specific `InitializingBean` or `DisposableBean` interfaces.
4. **Be Aware: Spring DOES NOT Manage Prototype Bean Destruction**: Spring creates, wires, and configures prototype beans, but completely drops references to them afterward. Prototype beans will NOT have their `@PreDestroy` or `destroy()` callbacks called by Spring!
5. **Always Clean Up ThreadLocal Scopes**: In custom thread scopes, register a servlet filter or interceptor to explicitly invoke `threadLocal.remove()` to prevent thread pool memory leaks.
6. **Keep Initialization Fast**: Do not perform heavy I/O or long-running computations inside `@PostConstruct`. It blocks application startup and slows down automated tests.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Injecting prototype directly into singleton** | The prototype is injected once at startup; it is never re-instantiated, defeating its purpose. | Use `proxyMode = ScopedProxyMode.TARGET_CLASS` or `ObjectFactory<T>`. |
| **Expecting `@PreDestroy` on prototype beans** | Spring does not hold references to prototypes after creation, so shutdown callbacks are never invoked. | Implement a custom `BeanPostProcessor` or manually manage prototype cleanup. |
| **Stateful singleton beans** | Adding mutable fields to a singleton bean causes catastrophic race conditions under concurrent traffic. | Make singletons completely stateless; store request state in method parameters or request-scoped beans. |
| **Blocking logic in `@PostConstruct`** | Long database migrations or external HTTP calls in `@PostConstruct` stall context initialization. | Move heavy tasks to an asynchronous `ApplicationRunner` or event listener. |
| **ThreadLocal memory leak in custom scopes** | Threads in web servers (Tomcat worker threads) are pooled and reused; failing to clear ThreadLocals corrupts subsequent requests. | Always call `threadLocal.remove()` in a `finally` block or filter. |
| **Using `new` on a Prototype bean** | Manually instantiating a prototype class via `new MyPrototype()` bypasses Spring DI, leaving fields null. | Fetch from container via `ObjectFactory.getObject()` or `@Lookup`. |

---

## 8. Performance & Security Notes

### Memory Leak Risk with Prototype Beans
Because Spring hands off prototype beans without retaining references, if a custom `BeanPostProcessor` stores prototype instances in an internal cache, those instances will never be garbage collected, creating an insidious memory leak.

### Thread Safety in Singletons
Singleton beans are accessed concurrently by hundreds of HTTP request threads (or virtual threads):
- Never store mutable user data in singleton class fields.
- If shared mutable state is unavoidable, use thread-safe data structures (`ConcurrentHashMap`, `AtomicLong`) or synchronization primitives.

---

## 9. Comparison / When to Use What

| Scope | Creation Timing | Destruction Callback? | Thread Safety Requirement | Best For |
| :--- | :--- | :---: | :--- | :--- |
| **`singleton`** | Context startup (eager) | **Yes** (`@PreDestroy`) | **Must be stateless/thread-safe** | Services, repositories, controllers, clients |
| **`prototype`** | On every request/injection | **NO** (Client must clean) | Isolated per caller | Stateful workers, calculation engines |
| **`request`** | On each HTTP request | **Yes** (at request end) | Thread-safe per request | Request trace headers, client tenant info |
| **`session`** | On HTTP session creation | **Yes** (at session expiry)| Thread-safe per user | User shopping cart, multi-step wizards |

---

## 10. Interview Questions

<details>
<summary><strong>Q1: What are all the standard bean scopes provided by Spring?</strong></summary>

**Answer:**  
1. `singleton`: Single shared instance per `ApplicationContext` (default).
2. `prototype`: A new instance is created every time the bean is requested.
3. `request`: One instance per HTTP request (web-aware context).
4. `session`: One instance per HTTP session (web-aware context).
5. `application`: One instance per `ServletContext` (web-aware context).
6. `websocket`: One instance per WebSocket session.
</details>

<details>
<summary><strong>Q2: What is the Prototype-in-Singleton injection problem, and how do you solve it?</strong></summary>

**Answer:**  
Because singletons are initialized once during startup, any prototype bean injected directly into a singleton's constructor or field is injected only once. The singleton retains that exact instance indefinitely, destroying the prototype behavior.  
**Solutions**:
1. Set `proxyMode = ScopedProxyMode.TARGET_CLASS` on the prototype/request bean.
2. Inject `ObjectFactory<T>` or `Provider<T>` and call `.getObject()`.
3. Use method injection with `@Lookup`.
</details>

<details>
<summary><strong>Q3: Does Spring invoke `@PreDestroy` or `destroy()` on prototype-scoped beans?</strong></summary>

**Answer:**  
**No.** Spring instantiates, configures, and injects prototype beans, but does not maintain a reference to them in the singleton cache. Therefore, when the `ApplicationContext` closes, Spring cannot invoke destruction callbacks on prototype beans. The client code that requested the prototype is responsible for managing its resource destruction.
</details>

<details>
<summary><strong>Q4: What is the difference between `@PostConstruct`, `InitializingBean.afterPropertiesSet()`, and `@Bean(initMethod)`?</strong></summary>

**Answer:**  
They all execute after properties have been injected, in the following order:
1. `@PostConstruct` (JSR-250 / Jakarta standard annotation).
2. `InitializingBean.afterPropertiesSet()` (Spring framework interface).
3. Custom `initMethod` (configured in `@Bean(initMethod = "...")`).  
Best practice is to use `@PostConstruct` because it avoids coupling code to Spring interfaces.
</details>

<details>
<summary><strong>Q5: How does a Scoped Proxy work under the hood?</strong></summary>

**Answer:**  
Spring creates a CGLIB subclass proxy of the target class and injects this proxy into the singleton. When a method on the proxy is invoked, the proxy intercepts the call, accesses the current request/session context via `RequestContextHolder`, resolves the target instance specific to the calling thread/request, and delegates the method invocation to that instance.
</details>

<details>
<summary><strong>Q6: How does `@Lookup` method injection work?</strong></summary>

**Answer:**  
`@Lookup` tells Spring to use CGLIB to generate a subclass of the bean and override the annotated method (e.g., `public abstract MyPrototype getPrototype();`). Inside the overridden method, Spring executes `context.getBean(MyPrototype.class)`, fetching a fresh instance from the container every time the method is called.
</details>

<details>
<summary><strong>Q7: Can a class implement both `InitializingBean` and have a `@PostConstruct` method?</strong></summary>

**Answer:**  
Yes. Spring will execute `@PostConstruct` first, followed immediately by `afterPropertiesSet()`. However, doing both is redundant and considered bad practice.
</details>

<details>
<summary><strong>Q8: What is the purpose of the `Aware` interfaces in the bean lifecycle?</strong></summary>

**Answer:**  
`Aware` marker interfaces (`BeanNameAware`, `BeanFactoryAware`, `ApplicationContextAware`) provide a way for beans to be notified of container infrastructure dependencies before initialization callbacks (`@PostConstruct`) run. They allow a bean to discover its own name, enclosing factory, or application context.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Verify Singleton vs Prototype Instantiation (Easy)</strong></summary>

**Task:**  
Create a `@Scope("prototype")` bean and a standard singleton bean. In a test runner, call `ctx.getBean()` three times for both. Assert that the prototype returns three distinct memory references, while the singleton returns identical references (`assertThat(b1).isSameAs(b2)`).
</details>

<details>
<summary><strong>Exercise 2: Trace the 12-Step Lifecycle (Easy)</strong></summary>

**Task:**  
Create a bean implementing `BeanNameAware`, `ApplicationContextAware`, and `InitializingBean` with `@PostConstruct` and `@PreDestroy`. Add console print statements in every method to verify the exact execution order.
</details>

<details>
<summary><strong>Exercise 3: Fix Prototype-in-Singleton with Scoped Proxy (Medium)</strong></summary>

**Task:**  
Create a `SessionCart` prototype bean injected into a `CartService` singleton. Demonstrate that calling `cartService.getCart()` returns the same instance. Add `@Scope(value="prototype", proxyMode=ScopedProxyMode.TARGET_CLASS)` and verify that every invocation now accesses a fresh instance.
</details>

<details>
<summary><strong>Exercise 4: On-Demand Injection via `ObjectFactory` (Medium)</strong></summary>

**Task:**  
Refactor a singleton to use `ObjectFactory<TaskWorker>`. Execute a loop generating 5 worker instances, proving each has a unique UUID generated in its constructor.
</details>

<details>
<summary><strong>Exercise 5: Implement Custom Thread Scope (Hard)</strong></summary>

**Task:**  
Implement the `org.springframework.beans.factory.config.Scope` interface using a `ThreadLocal<Map<String, Object>>`. Register it with `ConfigurableBeanFactory.registerScope("thread", new MyThreadScope())`. Verify that beans in the "thread" scope are singletons within the same thread but separate across threads.
</details>

<details>
<summary><strong>Exercise 6: Method Injection with `@Lookup` (Hard)</strong></summary>

**Task:**  
Define an abstract singleton class with an abstract method `@Lookup public abstract HeavyTask createHeavyTask();`. Verify that Spring dynamically generates a working subclass without compilation or bytecode errors.
</details>

---

## 12. Summary / Cheat Sheet

| Lifecycle Stage / Scope | Method / Annotation | Key Behavior |
| :--- | :--- | :--- |
| **`@PostConstruct`** | `jakarta.annotation.PostConstruct` | Runs after all dependencies injected |
| **`@PreDestroy`** | `jakarta.annotation.PreDestroy` | Runs on context shutdown (**Singletons only**) |
| **`InitializingBean`** | `afterPropertiesSet()` | Spring lifecycle interface (prefer `@PostConstruct`) |
| **`DisposableBean`** | `destroy()` | Spring destruction interface (prefer `@PreDestroy`) |
| **Scoped Proxy** | `proxyMode = ScopedProxyMode.TARGET_CLASS` | Allows short-lived beans inside singletons |
| **`ObjectFactory<T>`** | `objectFactory.getObject()` | On-demand creation of prototype instances |
| **`@Lookup`** | `@Lookup public abstract T getBean();` | CGLIB method override for prototype fetch |

---

## 13. Further Reading
- [Official Spring Framework Reference: Bean Scopes](https://docs.spring.io/spring-framework/reference/core/beans/factory-scopes.html)
- [Official Spring Framework Reference: Lifecycle Callbacks](https://docs.spring.io/spring-framework/reference/core/beans/factory-nature.html)
- [Jakarta Annotations Specification](https://jakarta.ee/specifications/annotations/)
