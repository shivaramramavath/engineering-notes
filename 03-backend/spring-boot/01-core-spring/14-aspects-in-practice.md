# Practical Enterprise Aspects: Audit, Rate Limiting & Tracing

> Build production-grade cross-cutting infrastructure: custom enterprise aspects for structured audit trails, in-memory token-bucket rate limiting, distributed trace ID propagation, multi-tenant routing, and parameter-binding pointcuts.

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [AOP Fundamentals & Dynamic Proxies](file:///03-backend/spring-boot/01-core-spring/13-aop-fundamentals-and-proxies.md) |
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
In enterprise applications, operations like security auditing, rate limiting, and distributed tracing are non-functional cross-cutting requirements. If every developer writes custom audit logging or rate-limiting logic manually within business methods, systems suffer from inconsistent log formatting, brittle token accounting, missed security events, and massive code duplication.

### Historical Evolution
- **Manual Interception (J2EE)**: Handcrafted decorators or Servlet filters. Unable to intercept service-layer or persistence-layer calls.
- **Spring AOP Patterns**: Allowed aspects to intercept any Spring bean method anywhere in the architecture stack, standardizing cross-cutting policies.
- **Modern Observability (Boot 3.x)**: Standardized on Micrometer Tracing and W3C `traceparent` propagation, which can be cleanly tied into aspects for sub-millisecond operational tracing.

### Real-World Relevance
Aspects allow platform and security teams to enforce compliance policies (e.g., PCI-DSS audit trails, API throttling) centrally without altering a single line of business domain logic.

---

## 2. Core Concepts

### 1. Pointcut Designators (PCD)
- **`execution(modifiers? return-type declaring-type? method-name(params) throws?)`**: Primary method-matching designator.
- **`within(type-pattern)`**: Limits matching to join points within certain types or packages.
- **`@annotation(annotation-type)`**: Matches methods annotated with a specific custom annotation.
- **`@within(annotation-type)`**: Matches all methods inside classes annotated with a specific annotation.
- **`args(type-pattern, ..)`**: Matches by runtime argument types and binds them to advice parameters.

### 2. Parameter Binding in Pointcuts
Advice methods can capture method arguments and annotations directly as method parameters instead of querying `JoinPoint.getArgs()` reflectively:
```java
@Around("@annotation(audit) && args(customerId, ..)")
public Object logAudit(ProceedingJoinPoint pjp, Auditable audit, String customerId) throws Throwable { ... }
```

### 3. Aspect Ordering with `@Order`
When multiple aspects intercept the same method (e.g., `@Transactional`, `@RateLimited`, `@Audited`), order determines nesting:
- Lower order number runs **first** on the way in.
- Lower order number runs **last** on the way out (nested wrapping).

---

## 3. How It Works Internally

### Enterprise Aspect Interception Pipeline

```
HTTP Request / Service Invocation
             |
             v
+-------------------------------------------------------------------------+
|                  CGLIB Dynamic Proxy Entry Point                        |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|   Aspect 1: RateLimitingAspect (@Order(1))                              |
|   - Checks Token Bucket: Sufficient tokens available?                   |
|     * NO:  Throws RateLimitExceededException (Halts chain)              |
|     * YES: Decrements token, calls pjp.proceed()                        |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|   Aspect 2: DistributedTracingAspect (@Order(2))                        |
|   - Reads/Generates W3C TraceId & SpanId                                |
|   - Pushes into MDC (Mapped Diagnostic Context)                         |
|   - Calls pjp.proceed()                                                 |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|   Aspect 3: AuditLoggingAspect (@Order(3))                              |
|   - Captures method parameters & caller identity                        |
|   - Calls pjp.proceed()                                                 |
|   - Emits structured JSON audit event with duration                     |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|   Target Business Method (e.g., OrderService.placeOrder())              |
|   - Pure domain business logic; completely unaware of AOP!              |
+-------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### Parameter-Binding Pointcut Syntax
```java
@Around("@annotation(audited) && args(accountId, ..)")
public Object process(ProceedingJoinPoint pjp, Audited audited, String accountId) throws Throwable {
    System.out.printf("Action: %s on Account: %s%n", audited.action(), accountId);
    return pjp.proceed();
}
```

---

## 5. Practical Examples

### Example 1: Production-Grade Enterprise Audit Logging Aspect
```java
package com.example.demo.aspects;

import org.aspectj.lang.ProceedingJoinPoint;
import org.aspectj.lang.annotation.Around;
import org.aspectj.lang.annotation.Aspect;
import org.aspectj.lang.reflect.MethodSignature;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.core.annotation.Order;
import org.springframework.stereotype.Component;

import java.lang.annotation.*;
import java.time.Instant;
import java.util.UUID;

@Target(ElementType.METHOD)
@Retention(RetentionPolicy.RUNTIME)
public @interface AuditAction {
    String value();
    boolean includeArgs() default true;
}

@Aspect
@Component
@Order(10)
public class EnterpriseAuditAspect {

    private static final Logger log = LoggerFactory.getLogger(EnterpriseAuditAspect.class);

    @Around("@annotation(auditAction)")
    public Object auditExecution(ProceedingJoinPoint pjp, AuditAction auditAction) throws Throwable {
        String actionName = auditAction.value();
        String traceId = UUID.randomUUID().toString().substring(0, 8);
        Instant startTime = Instant.now();
        MethodSignature sig = (MethodSignature) pjp.getSignature();

        log.info("[AUDIT-START] Trace: {} | Action: {} | Method: {}.{}", 
            traceId, actionName, sig.getDeclaringType().getSimpleName(), sig.getName());

        if (auditAction.includeArgs()) {
            log.info("[AUDIT-ARGS] Trace: {} | Arguments: {}", traceId, pjp.getArgs());
        }

        Object result;
        try {
            result = pjp.proceed();
            log.info("[AUDIT-SUCCESS] Trace: {} | Action: {} | Duration: {} ms", 
                traceId, actionName, java.time.Duration.between(startTime, Instant.now()).toMillis());
            return result;
        } catch (Throwable ex) {
            log.error("[AUDIT-FAILURE] Trace: {} | Action: {} | Error: {}", 
                traceId, actionName, ex.getMessage());
            throw ex; // Re-throw to avoid swallowing business exceptions!
        }
    }
}
```

### Example 2: In-Memory Token-Bucket Rate Limiting Aspect
```java
package com.example.demo.aspects;

import org.aspectj.lang.ProceedingJoinPoint;
import org.aspectj.lang.annotation.Around;
import org.aspectj.lang.annotation.Aspect;
import org.springframework.core.annotation.Order;
import org.springframework.stereotype.Component;

import java.lang.annotation.*;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.atomic.AtomicInteger;

@Target(ElementType.METHOD)
@Retention(RetentionPolicy.RUNTIME)
public @interface RateLimited {
    int maxRequestsPerMinute() default 10;
}

public class RateLimitExceededException extends RuntimeException {
    public RateLimitExceededException(String message) { super(message); }
}

@Aspect
@Component
@Order(1) // Run FIRST before transactions or business logic
public class RateLimitingAspect {

    private final ConcurrentHashMap<String, AtomicInteger> requestCounts = new ConcurrentHashMap<>();
    private final ConcurrentHashMap<String, Long> windowStartTimes = new ConcurrentHashMap<>();

    @Around("@annotation(rateLimited)")
    public Object enforceRateLimit(ProceedingJoinPoint pjp, RateLimited rateLimited) throws Throwable {
        String key = pjp.getSignature().toShortString();
        long now = System.currentTimeMillis();

        windowStartTimes.putIfAbsent(key, now);
        requestCounts.putIfAbsent(key, new AtomicInteger(0));

        // Reset window every 60 seconds
        if (now - windowStartTimes.get(key) > 60_000) {
            windowStartTimes.put(key, now);
            requestCounts.get(key).set(0);
        }

        int currentRequests = requestCounts.get(key).incrementAndGet();
        if (currentRequests > rateLimited.maxRequestsPerMinute()) {
            throw new RateLimitExceededException(
                "Rate limit exceeded for " + key + ": " + currentRequests + " requests in current minute");
        }

        return pjp.proceed();
    }
}
```

### Example 3: Multi-Tenant Schema Routing Aspect
```java
package com.example.demo.aspects;

import org.aspectj.lang.ProceedingJoinPoint;
import org.aspectj.lang.annotation.Around;
import org.aspectj.lang.annotation.Aspect;
import org.springframework.stereotype.Component;

import java.lang.annotation.*;

@Target({ElementType.METHOD, ElementType.TYPE})
@Retention(RetentionPolicy.RUNTIME)
public @interface TenantAware {}

// ThreadLocal holder for current tenant ID
public class TenantContext {
    private static final ThreadLocal<String> CURRENT_TENANT = new ThreadLocal<>();
    public static void set(String tenantId) { CURRENT_TENANT.set(tenantId); }
    public static String get() { return CURRENT_TENANT.get(); }
    public static void clear() { CURRENT_TENANT.remove(); }
}

@Aspect
@Component
public class MultiTenantRoutingAspect {

    // Matches any method in a @TenantAware class where the first arg is tenantId
    @Around("@within(TenantAware) && args(tenantId, ..)")
    public Object routeTenant(ProceedingJoinPoint pjp, String tenantId) throws Throwable {
        try {
            TenantContext.set(tenantId);
            System.out.println("[TENANT ROUTING] Active tenant context set to: " + tenantId);
            return pjp.proceed();
        } finally {
            TenantContext.clear(); // Always clean up ThreadLocal
            System.out.println("[TENANT ROUTING] Cleared tenant context.");
        }
    }
}
```

### Example 4: Swallowing Exceptions Anti-Pattern & Remediation
```java
// ANTI-PATTERN: Catching and swallowing exceptions in @Around advice
@Aspect
@Component
public class BadMetricsAspect {

    @Around("execution(* com.example.service..*(..))")
    public Object measure(ProceedingJoinPoint pjp) {
        try {
            return pjp.proceed();
        } catch (Throwable t) {
            // FATAL FLAW: Swallowing the exception causes @Transactional to COMMIT 
            // corrupt database data, and callers receive null instead of handling errors!
            System.err.println("Error occurred: " + t.getMessage());
            return null; // DANGEROUS!
        }
    }
}

// REMEDIATION: Always log/measure in finally, and re-throw the original Throwable!
@Aspect
@Component
public class CleanMetricsAspect {

    @Around("execution(* com.example.service..*(..))")
    public Object measure(ProceedingJoinPoint pjp) throws Throwable {
        try {
            return pjp.proceed();
        } catch (Throwable t) {
            System.err.println("Metric record: method failed with " + t.getClass().getSimpleName());
            throw t; // MANDATORY: Re-throw to allow transaction rollback and caller handling
        }
    }
}
```

### Example 5: In-Memory Method Caching Aspect
```java
package com.example.demo.aspects;

import org.aspectj.lang.ProceedingJoinPoint;
import org.aspectj.lang.annotation.Around;
import org.aspectj.lang.annotation.Aspect;
import org.springframework.stereotype.Component;

import java.lang.annotation.*;
import java.util.Arrays;
import java.util.concurrent.ConcurrentHashMap;

@Target(ElementType.METHOD)
@Retention(RetentionPolicy.RUNTIME)
public @interface SimpleCache {}

@Aspect
@Component
public class SimpleCacheAspect {

    private final ConcurrentHashMap<String, Object> cacheStore = new ConcurrentHashMap<>();

    @Around("@annotation(SimpleCache)")
    public Object cacheResult(ProceedingJoinPoint pjp) throws Throwable {
        String cacheKey = pjp.getSignature().toShortString() + "_" + Arrays.deepToString(pjp.getArgs());

        if (cacheStore.containsKey(cacheKey)) {
            System.out.println("[CACHE HIT] Returning cached result for: " + cacheKey);
            return cacheStore.get(cacheKey);
        }

        System.out.println("[CACHE MISS] Executing target method for: " + cacheKey);
        Object result = pjp.proceed();

        if (result != null) {
            cacheStore.put(cacheKey, result);
        }
        return result;
    }
}
```

---

## 6. Best Practices

1. **Always Re-throw Exceptions in `@Around` Advice**: Swallowing exceptions in an around aspect prevents `@Transactional` from rolling back and returns `null` to the caller. Always re-throw `throw ex;`.
2. **Explicitly Define `@Order` on Every Aspect**: Prevent non-deterministic behavior by explicitly ordering aspects (e.g., `@Order(1)` for Security/RateLimiting, `@Order(2)` for Transactions, `@Order(10)` for Logging).
3. **Use Annotation-Driven Pointcuts (`@annotation`)**: Pointcuts matching custom annotations are refactoring-safe, whereas string-based package regex pointcuts break when packages are restructured.
4. **Clean Up `ThreadLocal` in a `finally` Block**: Any aspect that stores data in a `ThreadLocal` (such as tenant IDs or trace contexts) must invoke `.remove()` in a `finally` block to prevent thread pool leaks.
5. **Bind Parameters Declaratively**: Use `args(myArg, ..)` in the pointcut rather than reflectively extracting arguments from `pjp.getArgs()`.
6. **Keep Aspects Stateless**: Aspect beans are standard singletons. Do not store request-specific state in aspect instance variables; pass state through method local variables or ThreadLocal.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Swallowing exceptions in `@Around`** | Calling `return null;` inside a `catch` block prevents transaction rollbacks and breaks error flows. | Always re-throw the caught `Throwable`. |
| **Memory leak from `ThreadLocal`** | Storing tenant/security state in a `ThreadLocal` without `.remove()` corrupts subsequent requests in worker threads. | Always call `threadLocal.remove()` in a `finally` block. |
| **Omitting `@Order`** | Aspects run in arbitrary order, causing logging to execute before rate limiting or after transactions commit. | Add explicit `@Order(int)` to all aspect classes. |
| **Heavy I/O inside `@Before`** | Making slow network or disk calls in an advice delays every single service invocation. | Offload logging or metrics asynchronously using queues or background threads. |
| **Self-invocation bypassing custom aspect** | Calling `this.myAuditedMethod()` does not trigger the aspect because it bypasses the CGLIB proxy. | Move audited method to a separate collaborator bean. |
| **Missing `args()` parameter types** | Misspelling bound parameter names between pointcut string and advice signature throws `IllegalArgumentException`. | Ensure parameter names in `args(param)` match advice method argument names. |

---

## 8. Performance & Security Notes

### Pointcut Matching Cost
Pointcuts using expensive reflective designators (like broad regex `execution(* *..*.*(..))`) are evaluated by Spring at bean instantiation time, not per-request. However, parameter-matching pointcuts (`args()`) perform runtime type checks:
- Keep pointcut matching narrow using `within()` or `@annotation()`.

### Security Hardening
Audit aspects must never log sensitive personal information (PII) such as plaintext passwords, credit card numbers, or auth tokens. Implement masking filters inside audit aspects:
```java
String maskedArg = arg.replaceAll("(?i)(password|secret)=\\w+", "$1=******");
```

---

## 9. Comparison / When to Use What

| Interception Mechanism | Scope | Protocol | Access to Spring Beans | Best For |
| :--- | :--- | :--- | :---: | :--- |
| **Spring AOP Aspect** | Any Spring Bean method | Agnostic (Web, Kafka, Batch) | **Full Access** | Business auditing, method metrics, security |
| **Servlet Filter** | HTTP Request Ingress | HTTP only | Limited (via context) | IP filtering, CORS, gzip compression |
| **Spring MVC HandlerInterceptor**| HTTP Controller level | HTTP only | Full Access | Pre-controller auth checks, locale parsing |

---

## 10. Interview Questions

<details>
<summary><strong>Q1: How do you bind method parameters in a Spring AOP pointcut?</strong></summary>

**Answer:**  
By declaring `args(paramName, ..)` in the pointcut and matching the name with a parameter in the advice method signature:
```java
@Before("@annotation(Audited) && args(accountId, ..)")
public void audit(JoinPoint jp, String accountId) {
    System.out.println("Auditing account: " + accountId);
}
```
Spring automatically casts and passes the argument into the advice method without requiring reflection.
</details>

<details>
<summary><strong>Q2: Why is swallowing an exception inside an `@Around` advice considered a critical bug?</strong></summary>

**Answer:**  
In an `@Around` advice, if you catch a `Throwable` and return `null` instead of re-throwing it:
1. Spring's `@Transactional` interceptor never sees the exception, so it will **commit the transaction** instead of rolling back, corrupting the database.
2. The caller receives `null` instead of the expected exception, causing unpredictable `NullPointerException`s downstream.
</details>

<details>
<summary><strong>Q3: How does aspect precedence work when multiple aspects intercept the same method?</strong></summary>

**Answer:**  
Aspects are ordered using the `@Order(int)` annotation or by implementing `Ordered`. The aspect with the **lowest integer value** has the highest precedence:
- On method entry (`@Before` / first part of `@Around`): Lowest order runs first.
- On method exit (`@After` / second part of `@Around`): Highest order returns first, wrapping outwards like nested function calls.
</details>

<details>
<summary><strong>Q4: What is the difference between `@annotation` and `@within` in Pointcut expressions?</strong></summary>

**Answer:**  
- `@annotation(MyAnnotation)`: Matches join points where the **specific method** is annotated with `@MyAnnotation`.
- `@within(MyAnnotation)`: Matches join points where the **enclosing class** is annotated with `@MyAnnotation` (all methods inside that class are matched).
</details>

<details>
<summary><strong>Q5: Can Spring AOP aspects intercept calls originating from asynchronous threads?</strong></summary>

**Answer:**  
Yes, provided the method invocation is executed through the Spring bean proxy. However, if the aspect relies on `ThreadLocal` storage (like `SecurityContext` or `RequestContextHolder`), those values will not be available in the async thread unless explicitly propagated.
</details>

<details>
<summary><strong>Q6: How can an aspect alter the return value of a method?</strong></summary>

**Answer:**  
Using an `@Around` advice. The advice captures the result of `Object original = pjp.proceed()`, performs transformations, and returns the modified object:
```java
@Around("execution(* getSecret())")
public Object maskSecret(ProceedingJoinPoint pjp) throws Throwable {
    String secret = (String) pjp.proceed();
    return "******" + secret.substring(secret.length() - 4);
}
```
</details>

<details>
<summary><strong>Q7: What is the difference between Spring AOP and a Spring MVC `HandlerInterceptor`?</strong></summary>

**Answer:**  
- `HandlerInterceptor`: Operates strictly within Spring WebMvc at the HTTP layer, intercepting requests before they reach the `@Controller`.
- Spring AOP: Operates at the Java method level across any Spring bean in any layer (Controllers, Services, Repositories, Batch Jobs, Kafka Listeners).
</details>

<details>
<summary><strong>Q8: How do you prevent sensitive data (like passwords) from being logged by an auditing aspect?</strong></summary>

**Answer:**  
1. Create a custom annotation `@Sensitive` to mark parameters that should not be logged.
2. Inspect method parameter annotations in the aspect and mask values corresponding to `@Sensitive`.
3. Alternatively, implement string pattern masking replacing password fields in JSON payloads before writing to log streams.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Create a Parameter Logging Aspect (Easy)</strong></summary>

**Task:**  
Write an aspect with `@Before("execution(* com.example.service..*(..))")` that prints the method name and every argument value passed to the method.
</details>

<details>
<summary><strong>Exercise 2: Type-Safe Parameter Binding (Easy)</strong></summary>

**Task:**  
Create a pointcut that intercepts a method `updateUser(UUID userId, UserData data)` and binds `userId` directly as an advice parameter using `args(userId, ..)`.
</details>

<details>
<summary><strong>Exercise 3: Test Aspect Ordering Precedence (Medium)</strong></summary>

**Task:**  
Create two aspects: `AspectA` (`@Order(1)`) and `AspectB` (`@Order(2)`). Add print statements in `@Before` and `@After`. Verify that on entry AspectA runs before AspectB, and on exit AspectB runs before AspectA.
</details>

<details>
<summary><strong>Exercise 4: Implement In-Memory Rate Limiter (Medium)</strong></summary>

**Task:**  
Implement the token-bucket `@RateLimited(maxRequestsPerMinute = 5)` aspect. Write a multithreaded test sending 10 rapid requests and assert that 5 succeed and 5 throw `RateLimitExceededException`.
</details>

<details>
<summary><strong>Exercise 5: Dynamic Cache with Aspect (Hard)</strong></summary>

**Task:**  
Implement a custom `@CustomCacheable` aspect that caches method return values in a `ConcurrentHashMap` based on method name and arguments. Test that calling the method twice with identical arguments executes the underlying logic only once.
</details>

<details>
<summary><strong>Exercise 6: Secure Masking Audit Aspect (Hard)</strong></summary>

**Task:**  
Create an audit aspect that intercepts `LoginRequest(String username, String password)`. Ensure that `password` is masked as `********` before being recorded in the audit log.
</details>

---

## 12. Summary / Cheat Sheet

| Pointcut Designator | Syntax Example | Matches Join Point |
| :--- | :--- | :--- |
| **Method Execution** | `execution(* com.example..*.*(..))` | Method signature pattern |
| **Package / Class** | `within(com.example.service..*)` | All methods in matched types |
| **Method Annotation**| `@annotation(Audited)` | Methods marked with `@Audited` |
| **Class Annotation** | `@within(TenantAware)` | All methods in `@TenantAware` class |
| **Parameter Binding**| `args(userId, ..)` | Binds runtime argument to parameter |
| **Precedence** | `@Order(1)` | Lowest integer executes first |
| **Exception Rule** | `throw ex;` in `@Around` | **Never swallow exceptions in advice!** |

---

## 13. Further Reading
- [Official Spring Framework Reference: Declaring an Aspect](https://docs.spring.io/spring-framework/reference/core/aop/ataspectj/at-aspectj.html)
- [Micrometer Tracing Documentation](https://micrometer.io/docs/tracing)
- [OWASP Logging Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Logging_Cheat_Sheet.html)
