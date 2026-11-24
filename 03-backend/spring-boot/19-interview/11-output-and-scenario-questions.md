# 11: Code Output Tracing & Scenario-Based Technical Interview Questions

| Metadata Field | Value |
|---|---|
| **Topic** | Code Output Tracing, Tricky Edge Cases & Production Bug Scenarios |
| **Spring Boot Version** | 3.4.x |
| **Java Version** | 21 / 25 LTS |
| **Target Level** | Senior / Lead / Staff Backend Engineer |
| **Key Scenarios** | Proxy Bypass, Checked Exception Rollbacks, Async Thread Loss, Cache Misses |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [Internals & Architecture](#3-internals--architecture)
4. [Syntax & Practical Usage](#4-syntax--practical-usage)
5. [5 Practical Examples](#5-5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Pitfalls & Anti-Patterns](#7-common-pitfalls--anti-patterns)
8. [Performance & Security Considerations](#8-performance--security-considerations)
9. [Comparison / Decision Matrix](#9-comparison--decision-matrix)
10. [Real-World Interview Q&A](#10-real-world-interview-qa)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference Cheatsheet](#12-summary--quick-reference-cheatsheet)
13. [Further Reading & Official Resources](#13-further-reading--official-resources)

---

## 1. What & Why

In senior engineering interviews, theoretical knowledge alone is insufficient. Top tech companies and financial institutions evaluate candidates through **Scenario-Based Questions** and **Code Output Tracing**: presenting short snippets of Spring code that appear correct at first glance, but contain subtle runtime bugs, proxy bypasses, unexpected transaction commits, or thread-context leaks.

This module walks through real-world puzzle scenarios, explains the underlying framework mechanics causing unexpected behavior, and details the production fixes.

---

## 2. Core Concepts

### 2.1 The Silent Proxy Bypass
Annotations like `@Transactional`, `@Async`, `@Cacheable`, and `@Retryable` rely on Spring AOP proxies. When a method inside class $A$ calls another method inside class $A$ via `this.method()`, the call is executed locally within the JVM instance. It completely bypasses the dynamic proxy, causing the annotation to be silently ignored without any compiler or runtime error.

### 2.2 The Checked Exception Commit Trap
Spring's `@Transactional` rollback defaults to rolling back ONLY on `RuntimeException` and `Error`. If a method throws a checked exception (inheriting directly from `java.lang.Exception`), Spring logs the error but proceeds to **COMMIT the transaction**.

---

## 3. Internals & Architecture

```
 ┌────────────────────────────────────────────────────────┐
 │ Caller invokes: service.outerMethod()                  │
 └───────────────────────────┬────────────────────────────┘
                             │
                             ▼
 ┌────────────────────────────────────────────────────────┐
 │           Spring Dynamic AOP Proxy Layer               │
 │   - Opens Transaction / Checks Security                │
 └───────────────────────────┬────────────────────────────┘
                             │
                             ▼
 ┌────────────────────────────────────────────────────────┐
 │                 Actual Target Bean Instance            │
 │                                                        │
 │  public void outerMethod() {                           │
 │      // Direct internal call on 'this':                │
 │      this.innerTransactionalMethod();                  │
 │      // BYPASSES THE PROXY!                            │
 │      // No transaction, no cache, no async execution!  │
 │  }                                                     │
 └────────────────────────────────────────────────────────┘
```

---

## 4. Syntax & Practical Usage

### 4.1 What is the Output? (Scenario Preview)
```java
@Service
public class TrickyService {
    public void a() {
        b();
    }
    @Transactional(propagation = Propagation.REQUIRES_NEW)
    public void b() {
        // Will this run in a new transaction? NO!
    }
}
```

---

## 5. 5 Practical Examples

### Scenario 1: The Transaction Rollback Mystery

```java
package com.example.scenarios;

import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.io.IOException;

@Service
public class OrderRollbackScenario {

    private final OrderRepository repo;

    public OrderRollbackScenario(OrderRepository repo) {
        this.repo = repo;
    }

    // INTERVIEW QUESTION:
    // Does the database row get committed or rolled back when an IOException is thrown?
    @Transactional
    public void createOrderWithCheckedException(String orderId) throws IOException {
        repo.save(new Order(orderId));
        if (true) {
            throw new IOException("Disk error during receipt generation");
        }
    }
    
    // ANSWER:
    // IT COMMITS! Spring @Transactional by default only rolls back on RuntimeException/Error.
    // FIX: @Transactional(rollbackFor = Exception.class)
}
```

### Scenario 2: The Silent Cache Bypass

```java
package com.example.scenarios;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.cache.annotation.Cacheable;
import org.springframework.stereotype.Service;

@Service
public class ProductCatalogCacheScenario {

    private static final Logger log = LoggerFactory.getLogger(ProductCatalogCacheScenario.class);

    // INTERVIEW QUESTION:
    // When getProduct(1) is called 5 times, how many times does "Querying DB" log?
    public String getProduct(int id) {
        return fetchFromDatabase(id);
    }

    @Cacheable("products")
    public String fetchFromDatabase(int id) {
        log.info("Querying DB for product: {}", id);
        return "Product-" + id;
    }

    // ANSWER:
    // It logs "Querying DB" ALL 5 TIMES! 
    // Calling fetchFromDatabase() via this bypasses the Spring Cache proxy.
    // FIX: Move fetchFromDatabase to another service or inject self with @Lazy.
}
```

### Scenario 3: The Async ThreadLocal Security Context Loss

```java
package com.example.scenarios;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.scheduling.annotation.Async;
import org.springframework.security.core.context.SecurityContextHolder;
import org.springframework.stereotype.Service;

@Service
public class AsyncSecurityScenario {

    private static final Logger log = LoggerFactory.getLogger(AsyncSecurityScenario.class);

    // INTERVIEW QUESTION:
    // What does authentication.getName() print when called asynchronously?
    @Async
    public void executeBackgroundAudit() {
        var auth = SecurityContextHolder.getContext().getAuthentication();
        if (auth == null) {
            log.error("Authentication is NULL! Context was lost.");
        } else {
            log.info("Auditing user: {}", auth.getName());
        }
    }

    // ANSWER:
    // It logs "Authentication is NULL!" because SecurityContextHolder uses ThreadLocal by default,
    // and @Async runs on a distinct worker thread that has no security context.
    // FIX: Use DelegatingSecurityContextAsyncTaskExecutor.
}
```

### Scenario 4: The Spring Boot Bean Creation Order Deadlock

```java
package com.example.scenarios;

import jakarta.annotation.PostConstruct;
import org.springframework.stereotype.Component;

@Component
class ServiceA {
    private final ServiceB b;
    public ServiceA(ServiceB b) { this.b = b; }
    
    @PostConstruct
    public void init() {
        b.doSomething();
    }
}

@Component
class ServiceB {
    private final ServiceA a;
    public ServiceB(ServiceA a) { this.a = a; }

    public void doSomething() {}
}

// INTERVIEW QUESTION:
// What happens when Spring Boot starts up?
// ANSWER:
// Throws BeanCurrentlyInCreationException: There is a circular dependency between ServiceA and ServiceB!
// Spring Boot 2.6+ rejects circular references by default at startup.
```

### Scenario 5: `@Transactional` on Private Methods

```java
package com.example.scenarios;

import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

@Service
public class PrivateMethodScenario {

    public void publicEntrypoint() {
        executeTransactionalLogic();
    }

    // INTERVIEW QUESTION:
    // Does this private method run inside a database transaction?
    @Transactional
    private void executeTransactionalLogic() {
        // Database update...
    }

    // ANSWER:
    // NO! Spring AOP proxies (both CGLIB and JDK) only intercept public methods.
    // Annotations on private or protected methods are completely ignored by the proxy.
}
```

---

## 6. Best Practices

- **Never Put `@Transactional`, `@Async`, or `@Cacheable` on Private Methods**: Proxies only override public methods.
- **Always Declare `rollbackFor = Exception.class`**: Ensure all exceptions (checked and unchecked) trigger database rollback.
- **Be Vigilant for `this.method()` Calls**: Whenever calling a method on the same class, remember that AOP proxies are bypassed.
- **Use `@Lazy` Self-Injection for Same-Class Proxying**: If self-invocation is unavoidable, inject the bean into itself annotated with `@Lazy`.
- **Propagate Security Context to Background Tasks**: Never assume `SecurityContextHolder` is populated inside `@Async` or `CompletableFuture`.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Why It Fails | Correct Approach |
|---|---|---|
| **Self-Invoking `@Cacheable` Methods** | Bypasses proxy; cache is never checked or populated. | Extract to separate service or inject self with `@Lazy`. |
| **Throwing Checked Exception in `@Transactional`** | Default rollback rule only checks `RuntimeException`; transaction commits unexpectedly. | Add `rollbackFor = Exception.class`. |
| **`@Transactional` on `private` Method** | Spring AOP proxies cannot override private methods; annotation is ignored. | Make method `public` or manage transactions programmatically. |
| **Accessing `SecurityContext` in `@Async`** | ThreadLocal does not cross thread pool boundaries; principal is `null`. | Wrap executor in `DelegatingSecurityContextAsyncTaskExecutor`. |
| **Mixing Singletons and Unscoped Prototypes** | Prototype is injected once during startup; acts like a singleton forever. | Use `ObjectProvider<T>` or scoped proxy. |
| **Enabling Circular References via Config** | Hides bad architecture; causes subtle proxy wrapping initialization bugs. | Decouple using domain events or refactor into a coordinator service. |

---

## 8. Performance & Security Considerations

### 8.1 Transaction Leak Detection
Use HikariCP `leak-detection-threshold: 3000` to catch transactions held open during long operations.

### 8.2 SecurityContext Thread Contamination
Always call `SecurityContextHolder.clearContext()` in servlet filters after request execution to prevent thread pool reuse contamination.

---

## 9. Comparison / Decision Matrix

| Scenario Trap | Symptom | Root Cause |
|---|---|---|
| **Self-Invocation** | Annotations ignored silently | Call on `this` bypasses AOP proxy |
| **Checked Exception** | Data commits despite exception | Default rollback checks `RuntimeException` only |
| **Private Method AOP** | Annotations ignored silently | CGLIB/JDK proxies cannot intercept private methods |
| **Async Security Loss** | Principal is `null` in worker | `ThreadLocal` context does not cross thread boundaries |

---

## 10. Real-World Interview Q&A

<details>
<summary><strong>1. "I annotated my method with `@Transactional`, but when it threw an exception, data was still committed. Why?"</strong></summary>

The method likely threw a checked exception (inheriting from `java.lang.Exception`, such as `IOException` or `SQLException`). By default, Spring's transaction interceptor only rolls back on unchecked exceptions (`RuntimeException` and `Error`). To fix it, annotate with `@Transactional(rollbackFor = Exception.class)`.
</details>

<details>
<summary><strong>2. "Why does my `@Cacheable` method query the database every single time when called from another method in the same class?"</strong></summary>

Because of the Spring AOP proxy self-invocation bypass. The call `this.cachedMethod()` executes directly on the target instance in the JVM, bypassing the outer Spring CGLIB proxy that handles cache inspection and population.
</details>

<details>
<summary><strong>3. "Can I put `@Transactional` or `@Async` on a `private` method? What happens?"</strong></summary>

Spring will compile and run without error, but the annotation will be completely ignored. Spring AOP uses CGLIB subclassing or JDK interface proxies, which can only intercept `public` methods.
</details>

<details>
<summary><strong>4. "What happens when an `@Async` method calls `SecurityContextHolder.getContext().getAuthentication()`?"</strong></summary>

It returns `null`. `SecurityContextHolder` uses a `ThreadLocal` strategy by default. When `@Async` executes, it runs on a separate worker thread from the task executor where the security context has not been established.
</details>

<details>
<summary><strong>5. "What error does Spring Boot 3 throw if Bean A injects Bean B and Bean B injects Bean A via constructors?"</strong></summary>

Spring Boot throws `BeanCurrentlyInCreationException` stating that a circular dependency exists between Bean A and Bean B, terminating application startup.
</details>

<details>
<summary><strong>6. "How does `@Transactional(propagation = Propagation.REQUIRES_NEW)` behave when called from inside another `@Transactional` method in the same class?"</strong></summary>

It does NOT start a new transaction. Because of self-invocation, the call bypasses the proxy, executing directly inside the existing outer transaction.
</details>

<details>
<summary><strong>7. "Why does `@Transactional` not work if I call the method from a unit test without Spring runner?"</strong></summary>

Because Spring's declarative transaction management is powered entirely by dynamic proxies generated by the Spring container. Plain `new MyService()` instantiates raw Java bytecode with no proxy interceptor attached.
</details>

<details>
<summary><strong>8. "How can you make self-invocation work without extracting the method to a new class?"</strong></summary>

Inject the service into itself using `@Autowired @Lazy private MyService self;`, and call `self.targetMethod()`. This routes the call back through the outer dynamic proxy.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Trace Code Execution Output</strong></summary>

**Prompt**: Given class `X` with public method `m1()` calling private method `m2()` annotated with `@Transactional`, explain the exact transaction boundary.

**Solution**:
No transaction is ever created. `m2()` is private and self-invoked, so the proxy never intercepts the call.
</details>

<details>
<summary><strong>Exercise 2: Fix Checked Exception Rollback Bug</strong></summary>

**Prompt**: Refactor a service method throwing `CustomBusinessCheckedException` so that database changes are rolled back.

**Solution**:
Inspect Example 1: add `@Transactional(rollbackFor = CustomBusinessCheckedException.class)`.
</details>

<details>
<summary><strong>Exercise 3: Test Self-Invocation Cache Bug in JUnit</strong></summary>

**Prompt**: Write a unit test showing that calling `fetch()` through `this` invokes the database twice despite `@Cacheable`.

**Solution**:
Inspect Example 2: invoke `getProduct(1)` twice and assert that the underlying repository was queried twice.
</details>

<details>
<summary><strong>Exercise 4: Propagate SecurityContext in CompletableFuture</strong></summary>

**Prompt**: Wrap a `Supplier` in `SecurityContextHolder.createCopy()` before passing it to `CompletableFuture.supplyAsync()`.

**Solution**:
```java
SecurityContext context = SecurityContextHolder.getContext();
CompletableFuture.supplyAsync(() -> {
    SecurityContextHolder.setContext(context);
    try {
        return auditService.doWork();
    } finally {
        SecurityContextHolder.clearContext();
    }
});
```
</details>

<details>
<summary><strong>Exercise 5: Resolve Circular Dependency with Domain Events</strong></summary>

**Prompt**: Refactor two tightly coupled services (`OrderService` and `BillingService`) so neither injects the other.

**Solution**:
`OrderService` publishes `OrderPlacedEvent` via `ApplicationEventPublisher`. `BillingService` listens via `@EventListener`.
</details>

<details>
<summary><strong>Exercise 6: Test `@Async` Exception Handling</strong></summary>

**Prompt**: Write an `AsyncUncaughtExceptionHandler` implementation that catches unhandled exceptions thrown by void `@Async` methods.

**Solution**:
Implement `AsyncConfigurer` and override `getAsyncUncaughtExceptionHandler()`.
</details>

---

## 12. Summary & Quick Reference Cheatsheet

```
┌────────────────────────────────────────────────────────────────────────┐
│                   SCENARIO & OUTPUT TRACING CHEATSHEET                 │
├────────────────────────────────────────────────────────────────────────┤
│ 1. Self-Invocation: this.method() ALWAYS bypasses AOP proxies          │
│ 2. Method Visibility: Proxies ONLY intercept public methods            │
│ 3. Rollback Default: Rolls back on RuntimeException only               │
│ 4. Checked Rollback: Must specify rollbackFor = Exception.class        │
│ 5. Async Context: ThreadLocal is lost in worker threads without wrapper│
│ 6. Circular Refs: Banned by default; decouple via domain events        │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 13. Further Reading & Official Resources

- [Spring Framework AOP Proxying Mechanisms](https://docs.spring.io/spring-framework/reference/core/aop/proxying.html)
- [Spring Declarative Transaction Management Pitfalls](https://docs.spring.io/spring-framework/reference/data-access/transaction/declarative.html)
- [Baeldung: Common Spring Transactional Mistakes](https://www.baeldung.com/spring-transactional-propagation-isolation)
- [Spring Security Concurrency Documentation](https://docs.spring.io/spring-security/reference/features/integrations/concurrency.html)
