# AOP Fundamentals & Dynamic Proxies: JDK vs. CGLIB

> The definitive architectural breakdown of Aspect-Oriented Programming (AOP) in Spring: JDK Dynamic Proxies vs CGLIB, the self-invocation (`this.`) trap, `AnnotationAwareAspectJAutoProxyCreator`, and mastering advice types.

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [IoC Container & ApplicationContext Internals](file:///03-backend/spring-boot/01-core-spring/01-ioc-container-and-application-context.md), [BeanPostProcessors](file:///03-backend/spring-boot/01-core-spring/07-bean-post-processors-and-factory-post-processors.md) |
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
Cross-cutting concerns—such as database transaction management (`@Transactional`), security authorization (`@PreAuthorize`), performance metrics, distributed tracing, and caching (`@Cacheable`)—are required across hundreds of business services. Implementing these concerns manually inside every service method creates duplicated boilerplate, pollutes domain logic, and causes maintenance nightmares.

### Historical Evolution
- **Pre-AOP (1990s)**: Cross-cutting code was copy-pasted into every method (`try { tx.begin(); ... tx.commit(); } catch { tx.rollback(); }`).
- **AspectJ (2001)**: Introduced complete AOP language with compile-time (CTW) and load-time weaving (LTW), but required specialized compilers.
- **Spring AOP (2003-Present)**: Leverages pure Java runtime dynamic proxies. Eliminates special compilers while handling 99% of enterprise interception requirements.
- **Spring Boot 2.x & 3.x**: Standardized on **CGLIB proxies by default** (`spring.aop.proxy-target-class=true`), avoiding JDK interface-casting bugs.

### Real-World Relevance
Spring AOP powers almost every high-level Spring feature. Understanding how dynamic proxies intercept calls—and why self-invocation (`this.`) silently bypasses all aspects—is essential for any senior backend engineer.

---

## 2. Core Concepts

### 1. AOP Terminology
- **Aspect**: A modular unit encapsulating a cross-cutting concern (e.g., `LoggingAspect`).
- **Join Point**: A point during program execution. In Spring AOP, **only method executions** are join points.
- **Pointcut**: A predicate expression matching join points (e.g., `execution(* com.example.service.*.*(..))`).
- **Advice**: Action taken by an aspect at a join point:
  * `@Before`: Executes before method execution.
  * `@After`: Executes after method returns or throws (finally).
  * `@AfterReturning`: Executes only after successful return.
  * `@AfterThrowing`: Executes only if an exception is thrown.
  * `@Around`: Wraps method execution, controlling whether and when to proceed.

### 2. JDK Dynamic Proxy vs. CGLIB
- **JDK Dynamic Proxy (`java.lang.reflect.Proxy`)**:
  * Built into the Java standard library.
  * Can **only** proxy interfaces (target class must implement an interface).
  * Generates a proxy class implementing the same interfaces.
- **CGLIB Proxy (Bytecode Generation)**:
  * Generates a dynamic subclass of the target class at runtime.
  * Can proxy concrete classes without requiring interfaces.
  * Cannot proxy `final` classes or override `final` methods.
  * **Default in Spring Boot** (`spring.aop.proxy-target-class=true`).

---

## 3. How It Works Internally

### Proxy Interception vs. The Self-Invocation Trap

```
A. EXTERNAL CALL VIA PROXY (ASPECTS EXECUTE)
Caller ---> [ Spring CGLIB Dynamic Proxy ] ---> [ Target OrderService ]
                     |
         1. Intercepts method: execute()
         2. Opens @Transactional boundary
         3. Invokes target.execute()
         4. Commits transaction
                     |
Caller <-------------+

B. INTERNAL SELF-INVOCATION (ASPECTS SILENTLY BYPASSED!)
Caller ---> [ Spring CGLIB Dynamic Proxy ] ---> [ Target OrderService ]
                     |                                 |
         1. Calls proxy.placeOrder()                   |
         2. Delegates to target.placeOrder() --------->|
                                                       | Calls this.sendEmail()
                                                       | (BYPASSES PROXY COMPLETELY!)
                                                       | No @Transactional!
                                                       | No @Async!
                                                       v
```

---

## 4. Syntax & Usage

### Basic Aspect Configuration
```java
@Aspect
@Component
public class LoggingAspect {

    // Pointcut matching all methods in service package
    @Pointcut("execution(* com.example.service..*.*(..))")
    public void serviceMethods() {}

    // Around Advice measuring duration
    @Around("serviceMethods()")
    public Object profile(ProceedingJoinPoint pjp) throws Throwable {
        long start = System.currentTimeMillis();
        try {
            return pjp.proceed(); // Delegate to target method
        } finally {
            long duration = System.currentTimeMillis() - start;
            System.out.printf("[AOP] %s took %d ms%n", pjp.getSignature().toShortString(), duration);
        }
    }
}
```

---

## 5. Practical Examples

### Example 1: High-Performance Execution Timing Aspect
```java
package com.example.demo.aop;

import org.aspectj.lang.ProceedingJoinPoint;
import org.aspectj.lang.annotation.Around;
import org.aspectj.lang.annotation.Aspect;
import org.aspectj.lang.reflect.MethodSignature;
import org.springframework.stereotype.Component;

import java.lang.annotation.*;

@Target(ElementType.METHOD)
@Retention(RetentionPolicy.RUNTIME)
public @interface TrackLatency {
    String metricName() default "";
}

@Aspect
@Component
public class LatencyTrackingAspect {

    @Around("@annotation(trackLatency)")
    public Object measure(ProceedingJoinPoint pjp, TrackLatency trackLatency) throws Throwable {
        long startTime = System.nanoTime();
        String metric = trackLatency.metricName().isEmpty() 
            ? pjp.getSignature().toShortString() 
            : trackLatency.metricName();

        try {
            return pjp.proceed();
        } finally {
            long elapsedMicros = (System.nanoTime() - startTime) / 1000;
            System.out.printf("[LATENCY] %s: %d µs%n", metric, elapsedMicros);
        }
    }
}
```

### Example 2: Comparing JDK Dynamic Proxy vs CGLIB Programmatically
```java
package com.example.demo.aop.comparison;

import org.springframework.aop.framework.ProxyFactory;
import java.lang.reflect.Proxy;

interface GreetingService {
    String greet(String name);
}

class GreetingServiceImpl implements GreetingService {
    @Override public String greet(String name) { return "Hello, " + name; }
}

public class ProxyComparisonDemo {

    public static void main(String[] args) {
        GreetingServiceImpl target = new GreetingServiceImpl();

        // 1. Force JDK Dynamic Proxy (setInterfaces, proxyTargetClass = false)
        ProxyFactory jdkFactory = new ProxyFactory(target);
        jdkFactory.setInterfaces(GreetingService.class);
        jdkFactory.setProxyTargetClass(false);
        GreetingService jdkProxy = (GreetingService) jdkFactory.getProxy();

        System.out.println("JDK Proxy Class: " + jdkProxy.getClass().getName());
        System.out.println("Is JDK Dynamic Proxy? " + Proxy.isProxyClass(jdkProxy.getClass()));

        // 2. Force CGLIB Proxy (proxyTargetClass = true)
        ProxyFactory cglibFactory = new ProxyFactory(target);
        cglibFactory.setProxyTargetClass(true);
        GreetingService cglibProxy = (GreetingService) cglibFactory.getProxy();

        System.out.println("CGLIB Proxy Class: " + cglibProxy.getClass().getName());
        System.out.println("Is CGLIB Proxy? " + cglibProxy.getClass().getName().contains("$$SpringCGLIB$$"));
    }
}
```

### Example 3: Auditing Method Arguments with `@AfterReturning`
```java
package com.example.demo.aop.audit;

import org.aspectj.lang.JoinPoint;
import org.aspectj.lang.annotation.AfterReturning;
import org.aspectj.lang.annotation.Aspect;
import org.springframework.stereotype.Component;

import java.util.Arrays;

@Aspect
@Component
public class SecurityAuditAspect {

    @AfterReturning(
        pointcut = "execution(* com.example.demo.service.OrderService.createOrder(..))",
        returning = "orderId"
    )
    public void logOrderCreation(JoinPoint jp, Object orderId) {
        Object[] args = jp.getArgs();
        System.out.printf("[SECURITY AUDIT] Method %s completed. Args: %s -> Created ID: %s%n",
            jp.getSignature().getName(), Arrays.toString(args), orderId);
    }
}
```

### Example 4: The Self-Invocation Trap & 3 Proven Remediations
```java
package com.example.demo.aop.selfinvocation;

import org.springframework.aop.framework.AopContext;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.context.annotation.Lazy;
import org.springframework.scheduling.annotation.Async;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

@Service
public class OrderProcessingService {

    // SOLUTION 2: Self-injection (lazy to prevent circular DI deadlock)
    @Autowired
    @Lazy
    private OrderProcessingService self;

    public void processOrder(String orderId) {
        System.out.println("1. Processing order: " + orderId);

        // FLAW: Direct this.sendNotification() bypasses proxy! @Async will NOT work!
        // this.sendNotification(orderId);

        // REMEDIATION A: Use self-injected proxy
        self.sendNotification(orderId);

        // REMEDIATION B: Use AopContext.currentProxy() (Requires @EnableAspectJAutoProxy(exposeProxy = true))
        // ((OrderProcessingService) AopContext.currentProxy()).sendNotification(orderId);
    }

    @Async
    public void sendNotification(String orderId) {
        System.out.printf("2. Notification sent asynchronously on thread: %s%n", 
            Thread.currentThread().getName());
    }
}

// REMEDIATION C (BEST ARCHITECTURAL PRACTICE): Refactor into a separate collaborator bean!
@Service
class NotificationDispatchService {
    @Async
    public void sendNotification(String orderId) {
        System.out.println("Notification sent via decoupled collaborator.");
    }
}
```

### Example 5: Exception Alerting with `@AfterThrowing`
```java
package com.example.demo.aop.alerts;

import org.aspectj.lang.JoinPoint;
import org.aspectj.lang.annotation.AfterThrowing;
import org.aspectj.lang.annotation.Aspect;
import org.springframework.stereotype.Component;

@Aspect
@Component
public class GlobalExceptionAlertAspect {

    @AfterThrowing(
        pointcut = "within(com.example.demo.service..*)",
        throwing = "ex"
    )
    public void alertOnServiceFailure(JoinPoint jp, Throwable ex) {
        System.err.printf("[CRITICAL ALERT] Exception in %s: %s (%s)%n",
            jp.getSignature().toShortString(), ex.getClass().getSimpleName(), ex.getMessage());
    }
}
```

---

## 6. Best Practices

1. **Beware of Self-Invocation**: Internal method calls (`this.method()`) bypass proxies. When a method needs `@Transactional`, `@Async`, or `@Cacheable`, move it to a separate collaborator bean.
2. **Prefer Custom Annotations for Pointcuts**: Instead of fragile regex pointcuts (`execution(* com.example..*Service.*(..))`), create custom marker annotations (`@Audited`, `@TrackLatency`) and match via `@annotation(...)`.
3. **Always Call `pjp.proceed()` in `@Around`**: Failing to call `proceed()` in an `@Around` advice prevents the target method from executing, silently swallowing the business call.
4. **Never Make Proxied Classes or Methods `final`**: CGLIB subclasses the target class and overrides methods. Declaring a class or method `final` prevents proxying.
5. **Keep Advice Logic Lightweight**: Advice executes synchronously on every intercepted method call. Avoid blocking operations inside `@Before` or `@After` advice.
6. **Use `@Order` to Control Multi-Aspect Precedence**: When multiple aspects intercept the same join point, annotate aspect classes with `@Order(int)` to guarantee predictable execution sequence.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Self-invocation (`this.call()`)** | Calls bypass the CGLIB proxy; `@Transactional` or `@Async` are silently ignored. | Refactor target method into a separate Spring bean. |
| **`final` class or method with CGLIB** | CGLIB cannot subclass `final` classes or override `final` methods. | Remove `final` keyword from Spring-managed beans and methods. |
| **Swallowing return value in `@Around`** | Forgetting `return pjp.proceed()` causes the method caller to receive `null`. | Always return the result of `pjp.proceed()`. |
| **Aspect on `private` methods** | Spring AOP proxies only intercept `public` method executions; `private` methods are ignored. | Apply aspects to `public` methods only. |
| **Type-casting JDK proxy to concrete class** | When JDK proxies are used, casting to `OrderServiceImpl` throws `ClassCastException` (proxy implements interface only). | Inject interface `OrderService` or enable CGLIB (`proxy-target-class=true`). |
| **Wrong ProceedingJoinPoint usage** | Using `ProceedingJoinPoint` on `@Before` or `@After` throws runtime configuration errors (valid only on `@Around`). | Use `JoinPoint` for `@Before`/`@After`, and `ProceedingJoinPoint` only for `@Around`. |

---

## 8. Performance & Security Notes

### CGLIB Proxy Execution Overhead
CGLIB proxies invoke generated bytecode method dispatchers, adding less than 10 nanoseconds of latency per call. This overhead is negligible compared to network or database latency.

### Security Vulnerabilities with Self-Invocation
If an internal method annotated with `@Secured("ROLE_ADMIN")` is called via `this.adminTask()` from a non-admin method, the security interceptor is bypassed, creating a severe privilege escalation bug. Always enforce security at public component boundaries.

---

## 9. Comparison / When to Use What

| Feature | Spring AOP (JDK Dynamic) | Spring AOP (CGLIB) | Full AspectJ (Compile-Time / LTW) |
| :--- | :--- | :--- | :--- |
| **Target Requirement** | Must implement interface | Non-final concrete class | Any class |
| **Join Points Supported** | Public methods only | Public methods only | Methods, fields, constructors |
| **Performance** | Reflection invocation | Direct bytecode dispatch | Direct compiled inlined bytecodes |
| **Complexity** | Zero setup | Zero setup (Boot default) | Requires `ajc` compiler or JVM agent |
| **Best For** | Legacy interface-first code | **Default for 99% of Spring apps** | Field interception, non-managed objects |

---

## 10. Interview Questions

<details>
<summary><strong>Q1: What is the difference between JDK Dynamic Proxies and CGLIB proxies in Spring?</strong></summary>

**Answer:**  
- **JDK Dynamic Proxy**: Standard Java library feature (`java.lang.reflect.Proxy`). Requires target classes to implement an interface. Generates a proxy implementing the same interfaces.
- **CGLIB Proxy**: Third-party bytecode generation library (repackaged in Spring). Subclasses the target class at runtime and overrides methods. Can proxy concrete classes without interfaces. Spring Boot enables CGLIB by default (`spring.aop.proxy-target-class=true`).
</details>

<details>
<summary><strong>Q2: What is the self-invocation problem in Spring AOP and why does it happen?</strong></summary>

**Answer:**  
Spring AOP is proxy-based. When an external caller invokes a method on a bean, it calls the proxy, which executes aspects before delegating to the real target object. However, if a method inside the target object calls another method on the same object using `this.method()`, the call is executed directly within the target instance without passing through the proxy. Consequently, any `@Transactional`, `@Async`, or custom aspect on the called method is silently bypassed.
</details>

<details>
<summary><strong>Q3: How do you fix the self-invocation problem?</strong></summary>

**Answer:**  
1. **Refactoring (Best Practice)**: Move the called method to a separate collaborator bean and inject it.
2. **Self-Injection**: Inject the bean into itself using `@Autowired @Lazy private MyService self;` and invoke `self.method()`.
3. **`AopContext.currentProxy()`**: Enable `exposeProxy = true` on `@EnableAspectJAutoProxy` and call `((MyService) AopContext.currentProxy()).method()`.
</details>

<details>
<summary><strong>Q4: Can Spring AOP intercept `private` or `final` methods? Why?</strong></summary>

**Answer:**  
No.
- **Private methods**: Cannot be overridden by CGLIB subclasses and are invisible to interface-based JDK proxies.
- **Final methods**: The Java language strictly prohibits overriding `final` methods, so CGLIB cannot generate a subclass method interceptor.
</details>

<details>
<summary><strong>Q5: What is the difference between `JoinPoint` and `ProceedingJoinPoint`?</strong></summary>

**Answer:**  
- `JoinPoint`: Provides reflective access to the current method being executed (arguments, method signature, target object). Used with `@Before`, `@After`, `@AfterReturning`, and `@AfterThrowing`.
- `ProceedingJoinPoint`: Sub-interface of `JoinPoint` used exclusively with `@Around` advice. It adds the `proceed()` method, which allows the aspect to control if, when, and with what arguments the underlying target method executes.
</details>

<details>
<summary><strong>Q6: Which Spring post-processor is responsible for creating AOP proxies?</strong></summary>

**Answer:**  
`AnnotationAwareAspectJAutoProxyCreator` (a `BeanPostProcessor`). In its `postProcessAfterInitialization()` method, it queries the `BeanFactory` for all registered aspects, checks if any pointcut matches the current bean, and wraps the bean in a dynamic proxy if matches are found.
</details>

<details>
<summary><strong>Q7: What does `@Around` advice do, and why must you return its result?</strong></summary>

**Answer:**  
`@Around` advice completely surrounds the target method invocation. It can inspect/modify arguments before the call, alter the return value, catch exceptions, or measure execution time. The advice must return the result of `pjp.proceed()`; if it returns `void` or `null`, the caller will receive `null` instead of the actual method result.
</details>

<details>
<summary><strong>Q8: What is the execution order when multiple aspects intercept the same method?</strong></summary>

**Answer:**  
Aspects execute according to their `@Order(int)` value (lower integers have higher precedence):
- On method entry (`@Before` / first half of `@Around`): Lower numbers run first.
- On method exit (`@After` / second half of `@Around`): Higher numbers run first (LIFO order, like nested Russian nesting dolls).
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Create an Execution Time Logging Aspect (Easy)</strong></summary>

**Task:**  
Create a custom annotation `@LogExecutionTime`. Write an aspect with `@Around("@annotation(LogExecutionTime)")` that logs the execution duration in milliseconds.
</details>

<details>
<summary><strong>Exercise 2: Demonstrate Self-Invocation Failure (Easy)</strong></summary>

**Task:**  
Create a service where method A calls method B (`this.b()`). Put `@LogExecutionTime` on method B only. Call method A from a test and verify that method B's log is NEVER printed.
</details>

<details>
<summary><strong>Exercise 3: Fix Self-Invocation with Self-Injection (Medium)</strong></summary>

**Task:**  
Fix Exercise 2 by injecting `@Autowired @Lazy private MyService self;` and calling `self.b()`. Verify that the timing aspect now executes successfully.
</details>

<details>
<summary><strong>Exercise 4: Verify CGLIB Subclass in Debugger (Medium)</strong></summary>

**Task:**  
Inject an `@Aspect`-intercepted service into a test. Print `service.getClass().getName()`. Verify it contains `$$SpringCGLIB$$` and verify that `service.getClass().getSuperclass()` is the original domain service class.
</details>

<details>
<summary><strong>Exercise 5: Dynamic Argument Validation Aspect (Hard)</strong></summary>

**Task:**  
Create an aspect with `@Before` advice that inspects all arguments passed to any service method. If any `String` argument contains SQL injection characters (e.g., `DROP TABLE`), throw a `SecurityException`.
</details>

<details>
<summary><strong>Exercise 6: Programmatic ProxyFactory with Advisors (Hard)</strong></summary>

**Task:**  
Using Spring's `ProxyFactory`, create a proxy programmatically without annotations: add a `DefaultPointcutAdvisor` matching methods named `find*` and attach a custom `MethodInterceptor`. Test against a plain POJO instance.
</details>

---

## 12. Summary / Cheat Sheet

| Advice Type | Annotation | ProceedingJoinPoint? | Execution Timing |
| :--- | :--- | :---: | :--- |
| **Around** | `@Around("pointcut()")` | **Yes (`pjp.proceed()`)**| Surrounds method execution |
| **Before** | `@Before("pointcut()")` | No (`JoinPoint`) | Before target method |
| **After** | `@After("pointcut()")` | No (`JoinPoint`) | After return or throw (finally) |
| **After Returning** | `@AfterReturning(returning="val")`| No (`JoinPoint`) | After successful return only |
| **After Throwing** | `@AfterThrowing(throwing="ex")` | No (`JoinPoint`) | After exception thrown only |
| **Self-Invocation Trap**| `this.method()` | N/A | **Bypasses proxy completely!** |
| **Default Proxy** | CGLIB (`proxy-target-class=true`)| N/A | Subclasses concrete class |

---

## 13. Further Reading
- [Official Spring Framework Reference: Aspect Oriented Programming with Spring](https://docs.spring.io/spring-framework/reference/core/aop.html)
- [Spring AOP vs Full AspectJ Feature Comparison](https://docs.spring.io/spring-framework/reference/core/aop/ataspectj/ataspectj-aspectj.html)
- [CGLIB Library and Proxy Mechanics](https://github.com/cglib/cglib)
