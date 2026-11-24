# 01: Spring Core & Dependency Injection Technical Interview Q&A

| Metadata Field | Value |
|---|---|
| **Topic** | Spring IoC Container, Bean Lifecycles, Proxies & Advanced DI |
| **Spring Boot Version** | 3.4.x |
| **Java Version** | 21 / 25 LTS |
| **Target Level** | Senior / Lead / Staff Backend Engineer |
| **Key Focus Areas** | BeanPostProcessor, BeanFactoryPostProcessor, JDK vs CGLIB Proxies, Circular Dependencies |

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

The Spring Inversion of Control (IoC) container and Dependency Injection (DI) engine constitute the foundation of the Spring ecosystem. In senior and staff-level technical interviews, interviewers assess whether a candidate understands the container beyond basic `@Autowired` usage.

Engineers are evaluated on the exact lifecycle stages of a Spring Bean, the role of meta-programming extension points (`BeanFactoryPostProcessor` vs `BeanPostProcessor`), how dynamic byte-code proxies (CGLIB vs JDK Dynamic Proxies) power declarative annotations like `@Transactional` and `@Async`, and why circular dependencies were permanently banned by default in modern Spring Boot versions.

---

## 2. Core Concepts

### 2.1 The Complete Bean Lifecycle
1. **BeanDefinition Loading**: The container scans `@Configuration`, `@Component`, and XML definitions into `BeanDefinition` metadata registries.
2. **BeanFactoryPostProcessors (BFPP)**: Modify `BeanDefinition` metadata before beans are instantiated (e.g., resolving property placeholders `${db.url}`).
3. **Instantiation**: JVM creates bean instance via reflection/constructor (`DefaultListableBeanFactory.createBeanInstance()`).
4. **Populate Properties**: Dependencies are injected via constructor, setter, or field.
5. **Aware Interfaces**: `BeanNameAware`, `BeanFactoryAware`, `ApplicationContextAware` invoked.
6. **BeanPostProcessor (BPP) Before-Initialization**: `postProcessBeforeInitialization()`.
7. **Initialization**:
   - `@PostConstruct` method.
   - `InitializingBean.afterPropertiesSet()`.
   - Custom `initMethod`.
8. **BeanPostProcessor (BPP) After-Initialization**: `postProcessAfterInitialization()` wraps beans in dynamic proxies (AOP, Transactions).
9. **Ready for Use**.
10. **Destruction**: `@PreDestroy`, `DisposableBean.destroy()`, custom `destroyMethod`.

---

## 3. Internals & Architecture

```
 ┌────────────────────────────────────────────────────────┐
 │ 1. Scan Configuration & Build BeanDefinition Registry  │
 └───────────────────────────┬────────────────────────────┘
                             │
                             ▼
 ┌────────────────────────────────────────────────────────┐
 │ 2. Execute BeanFactoryPostProcessors (BFPP)            │
 │    (e.g., PropertySourcesPlaceholderConfigurer)        │
 └───────────────────────────┬────────────────────────────┘
                             │
                             ▼
 ┌────────────────────────────────────────────────────────┐
 │ 3. Instantiate Bean via Constructor Reflection         │
 └───────────────────────────┬────────────────────────────┘
                             │
                             ▼
 ┌────────────────────────────────────────────────────────┐
 │ 4. Populate Properties (Dependency Injection)          │
 └───────────────────────────┬────────────────────────────┘
                             │
                             ▼
 ┌────────────────────────────────────────────────────────┐
 │ 5. Invoke Aware Callbacks (BeanNameAware, etc.)        │
 └───────────────────────────┬────────────────────────────┘
                             │
                             ▼
 ┌────────────────────────────────────────────────────────┐
 │ 6. BeanPostProcessor.postProcessBeforeInitialization() │
 └───────────────────────────┬────────────────────────────┘
                             │
                             ▼
 ┌────────────────────────────────────────────────────────┐
 │ 7. Initialize Bean (@PostConstruct / InitializingBean) │
 └───────────────────────────┬────────────────────────────┘
                             │
                             ▼
 ┌────────────────────────────────────────────────────────┐
 │ 8. BeanPostProcessor.postProcessAfterInitialization()  │
 │    --> Dynamic AOP Proxy Creation (CGLIB / JDK)        │
 └───────────────────────────┬────────────────────────────┘
                             │
                             ▼
                   [ Bean In Service ]
```

---

## 4. Syntax & Practical Usage

### 4.1 Custom BeanPostProcessor Syntax
```java
@Component
public class CustomAuditPostProcessor implements BeanPostProcessor {
    @Override
    public Object postProcessAfterInitialization(Object bean, String beanName) {
        if (bean.getClass().isAnnotationPresent(Audited.class)) {
            // Return CGLIB / dynamic proxy
        }
        return bean;
    }
}
```

---

## 5. 5 Practical Examples

### Example 1: Constructor Injection with `final` Fields (Immutability)

```java
package com.example.core.service;

import org.springframework.stereotype.Service;

@Service
public class OrderProcessingService {

    private final PaymentClient paymentClient;
    private final InventoryRepository inventoryRepository;

    // Idiomatic Spring Boot 3: No @Autowired needed for single constructor
    public OrderProcessingService(PaymentClient paymentClient, InventoryRepository inventoryRepository) {
        this.paymentClient = paymentClient;
        this.inventoryRepository = inventoryRepository;
    }

    public void processOrder(String orderId) {
        paymentClient.charge(orderId);
    }
}
```

### Example 2: Custom BeanPostProcessor for Method Execution Timing

```java
package com.example.core.bpp;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.BeansException;
import org.springframework.beans.factory.config.BeanPostProcessor;
import org.springframework.cglib.proxy.Enhancer;
import org.springframework.cglib.proxy.MethodInterceptor;
import org.springframework.stereotype.Component;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@Component
public class BenchmarkBeanPostProcessor implements BeanPostProcessor {

    private static final Logger log = LoggerFactory.getLogger(BenchmarkBeanPostProcessor.class);

    @Retention(RetentionPolicy.RUNTIME)
    public @interface Benchmark {}

    @Override
    public Object postProcessAfterInitialization(Object bean, String beanName) throws BeansException {
        Class<?> clazz = bean.getClass();
        if (clazz.isAnnotationPresent(Benchmark.class)) {
            log.info("Creating dynamic performance proxy for bean: {}", beanName);
            Enhancer enhancer = new Enhancer();
            enhancer.setSuperclass(clazz);
            enhancer.setCallback((MethodInterceptor) (obj, method, args, proxy) -> {
                long start = System.nanoTime();
                try {
                    return proxy.invoke(bean, args);
                } finally {
                    long duration = System.nanoTime() - start;
                    log.info("Method {}.{} executed in {} ms", clazz.getSimpleName(), method.getName(), duration / 1_000_000.0);
                }
            });
            return enhancer.create();
        }
        return bean;
    }
}
```

### Example 3: Resolving Prototype in Singleton via `ObjectProvider`

```java
package com.example.core.scope;

import org.springframework.beans.factory.ObjectProvider;
import org.springframework.beans.factory.config.ConfigurableBeanFactory;
import org.springframework.context.annotation.Scope;
import org.springframework.stereotype.Component;

@Component
@Scope(ConfigurableBeanFactory.SCOPE_PROTOTYPE)
class PrototypeTaskWorker {
    public void execute(int taskId) {
        // Stateful task execution
    }
}

@Component
public class SingletonTaskManager {

    private final ObjectProvider<PrototypeTaskWorker> workerProvider;

    public SingletonTaskManager(ObjectProvider<PrototypeTaskWorker> workerProvider) {
        this.workerProvider = workerProvider;
    }

    public void runTask(int taskId) {
        // Fresh prototype instance retrieved on every call
        PrototypeTaskWorker worker = workerProvider.getObject();
        worker.execute(taskId);
    }
}
```

### Example 4: Custom `BeanFactoryPostProcessor` Dynamic Definition

```java
package com.example.core.bfpp;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.BeansException;
import org.springframework.beans.factory.config.BeanFactoryPostProcessor;
import org.springframework.beans.factory.config.ConfigurableListableBeanFactory;
import org.springframework.stereotype.Component;

@Component
public class CustomDefinitionValidatorBfpp implements BeanFactoryPostProcessor {

    private static final Logger log = LoggerFactory.getLogger(CustomDefinitionValidatorBfpp.class);

    @Override
    public void postProcessBeanFactory(ConfigurableListableBeanFactory beanFactory) throws BeansException {
        int beanDefCount = beanFactory.getBeanDefinitionCount();
        log.info("BFPP Inspecting {} registered BeanDefinitions before instantiation...", beanDefCount);
        for (String name : beanFactory.getBeanDefinitionNames()) {
            var def = beanFactory.getBeanDefinition(name);
            if (def.isLazyInit()) {
                log.debug("Discovered lazy bean definition: {}", name);
            }
        }
    }
}
```

### Example 5: Clean Resolution of Self-Invocation Proxy Traps

```java
package com.example.core.proxy;

import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.context.annotation.Lazy;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Propagation;
import org.springframework.transaction.annotation.Transactional;

@Service
public class SelfInvocationOrderService {

    // Self-injection with @Lazy bypasses self-invocation proxy bypass
    @Autowired
    @Lazy
    private SelfInvocationOrderService self;

    public void processBatch() {
        // Calling this.requiresNewTx() BYPASSES the proxy!
        // Calling self.requiresNewTx() ENTERS the Spring transaction proxy!
        self.requiresNewTx();
    }

    @Transactional(propagation = Propagation.REQUIRES_NEW)
    public void requiresNewTx() {
        // Runs in separate isolated transaction
    }
}
```

---

## 6. Best Practices

- **Strictly Use Constructor Injection**: Eliminates field injection reflection overhead, allows `final` immutability, and makes unit tests simple without Spring test contexts.
- **Avoid Circular Dependencies**: Do not rely on `spring.main.allow-circular-references: true`. Refactor using domain events (`ApplicationEventPublisher`) or interface segregation.
- **Use `ObjectProvider<T>` for Dynamic Scopes**: When a Singleton needs a Prototype or Request-scoped bean, inject `ObjectProvider<T>` rather than `ApplicationContext`.
- **Prefer CGLIB (Class-based) Proxies**: Standard in modern Spring Boot (`spring.aop.proxy-target-class=true`).
- **Keep `@PostConstruct` Methods Fast**: Any blocking I/O (e.g. database schema migrations or remote network calls) inside `@PostConstruct` delays overall application startup.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Why It Fails | Correct Approach |
|---|---|---|
| **Field Injection (`@Autowired private Svc svc`)** | Cannot be instantiated cleanly in unit tests without reflection; hides dependencies. | Use constructor injection with `final` fields. |
| **Self-Invocation Proxy Bypass** | Calling `@Transactional` method $B()$ from method $A()$ on `this` bypasses the dynamic proxy. | Extract $B()$ to a separate service or inject self with `@Lazy`. |
| **Heavy Logic in `@PostConstruct`** | Long-running operations block container initialization, causing Kubernetes readiness probe timeouts. | Run heavy background tasks asynchronously using `ApplicationRunner`. |
| **Injecting Prototype into Singleton Directly** | The prototype is injected once at startup and effectively behaves as a singleton forever. | Inject `ObjectProvider<PrototypeBean>` or use scoped proxy. |
| **Enabling Circular References** | Masking circular dependencies hides architectural coupling and causes subtle init order bugs. | Decouple via events or refactor into a shared coordinator service. |
| **Modifying Beans in BFPP** | Calling `beanFactory.getBean()` inside a `BeanFactoryPostProcessor` forces premature bean instantiation. | Only inspect and modify `BeanDefinition` metadata inside BFPP. |

---

## 8. Performance & Security Considerations

### 8.1 Startup Time & Reflection Optimization
Avoid excessive `@ComponentScan` over root packages. Keep component scans narrowly targeted to optimize classpath scanning.

### 8.2 Ahead-Of-Time (AOT) Processing in Spring Boot 3
Spring Boot 3 AOT replaces dynamic runtime reflection and CGLIB proxying with pre-computed Java source code, enabling instant native startup via GraalVM.

---

## 9. Comparison / Decision Matrix

| Dimension | JDK Dynamic Proxy | CGLIB Proxy |
|---|---|---|
| **Requirement** | Target class must implement an interface | Can proxy concrete classes (subclassing) |
| **Mechanism** | `java.lang.reflect.Proxy` | Bytecode generation via ByteBuddy/CGLIB |
| **Final Classes/Methods** | Supported (interfaces cannot be final) | Cannot subclass `final` class or override `final` method |
| **Spring Boot 3 Default** | No | Yes (`proxyTargetClass = true`) |

---

## 10. Real-World Interview Q&A

<details>
<summary><strong>1. Explain the difference between `BeanFactoryPostProcessor` and `BeanPostProcessor`.</strong></summary>

A `BeanFactoryPostProcessor` (BFPP) operates on bean metadata (`BeanDefinition` objects) before any bean instances are created (e.g. resolving `${...}` property placeholders). A `BeanPostProcessor` (BPP) operates on actual bean instances during initialization, executing before and after `@PostConstruct` to wrap beans in dynamic AOP/Transactional proxies.
</details>

<details>
<summary><strong>2. What is the "Self-Invocation" problem in Spring AOP, and how do you resolve it?</strong></summary>

Spring AOP works by wrapping beans in dynamic proxies. When method $A()$ invokes method $B()$ on the same class using `this.B()`, the call is executed directly within the JVM without passing through the outer proxy, causing annotations like `@Transactional` or `@Async` on $B()$ to be silently ignored. Fix: inject self via `@Lazy`, extract $B()$ to a distinct bean, or use AspectJ compile-time weaving.
</details>

<details>
<summary><strong>3. Why did Spring Boot 2.6+ disable circular dependencies by default?</strong></summary>

Circular dependencies ($A \to B \to A$) indicate flawed architectural design and tight coupling. They require complex, brittle three-level cache reflection tricks during bean initialization and frequently break when proxies or `@Async` beans are involved. Spring enforces clean architectural separation by rejecting circular references at startup.
</details>

<details>
<summary><strong>4. How do you inject a Prototype-scoped bean into a Singleton-scoped bean properly?</strong></summary>

Direct injection results in the prototype being instantiated once at startup and shared forever. To obtain a fresh instance every time, inject `ObjectProvider<PrototypeBean>`, use `@Lookup` method injection, or declare the prototype with `@Scope(value = "prototype", proxyMode = ScopedProxyMode.TARGET_CLASS)`.
</details>

<details>
<summary><strong>5. What are the three caches used by `DefaultSingletonBeanRegistry` to resolve singletons?</strong></summary>

1. `singletonObjects` (ConcurrentHashMap): Fully initialized, ready-to-use singleton beans.
2. `earlySingletonObjects` (HashMap): Partially created beans (instantiated, but properties not yet injected).
3. `singletonFactories` (HashMap): Object factories providing early access to the bean to handle circular proxy wrapping.
</details>

<details>
<summary><strong>6. When does Spring choose CGLIB over JDK Dynamic Proxies?</strong></summary>

In Spring Boot 3, CGLIB (via ByteBuddy) is the default (`spring.aop.proxy-target-class=true`). It proxies concrete classes by generating dynamic subclasses. JDK Dynamic Proxies are used when explicit interface proxying is requested and the target class implements interfaces.
</details>

<details>
<summary><strong>7. What happens if an exception is thrown in a `@PostConstruct` method?</strong></summary>

The `BeanCreationException` aborts the `ApplicationContext` initialization, stops the application from starting, and immediately triggers destruction callbacks on already initialized singletons before terminating the JVM.
</details>

<details>
<summary><strong>8. Why is constructor injection preferred over field injection?</strong></summary>

Constructor injection allows fields to be marked `final` for immutability, guarantees that beans are never constructed in an uninitialized/partial state, makes circular dependencies immediately obvious at compile/test time, and enables straightforward unit testing without reflection or Spring test runners.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Implement Dynamic Method Execution Timer via BPP</strong></summary>

**Prompt**: Write a custom `BeanPostProcessor` that wraps any bean annotated with `@TimedService` in a dynamic proxy that logs execution duration.

**Solution**:
Inspect Example 2: implement `postProcessAfterInitialization()`, check for custom annotation, and return CGLIB `Enhancer.create()`.
</details>

<details>
<summary><strong>Exercise 2: Fix Self-Invocation Proxy Bypass with `@Lazy`</strong></summary>

**Prompt**: Refactor a service where a non-transactional method calls a `@Transactional(REQUIRES_NEW)` method on the same class so that the new transaction boundary is respected.

**Solution**:
Inspect Example 5: inject `self` annotated with `@Lazy`, and replace `this.method()` with `self.method()`.
</details>

<details>
<summary><strong>Exercise 3: Retrieve Prototype Instances via `ObjectProvider`</strong></summary>

**Prompt**: Create a singleton `ReportGenerator` service that obtains a fresh `ReportBuilder` prototype bean on each request.

**Solution**:
Inspect Example 3: inject `ObjectProvider<ReportBuilder>` and call `provider.getObject()` inside the generation method.
</details>

<details>
<summary><strong>Exercise 4: Verify Circular Dependency Exception</strong></summary>

**Prompt**: Write two beans with constructor references to each other and assert that Spring throws `BeanCurrentlyInCreationException` on startup.

**Solution**:
Define `class A { A(B b) {} }` and `class B { B(A a) {} }`. Run `new AnnotationConfigApplicationContext(Config.class)` and assert `BeanCurrentlyInCreationException` is thrown.
</details>

<details>
<summary><strong>Exercise 5: Inspect Bean Definitions in BFPP</strong></summary>

**Prompt**: Write a `BeanFactoryPostProcessor` printing all bean names containing the word "Security".

**Solution**:
Inspect Example 4: iterate over `beanFactory.getBeanDefinitionNames()`, filter names with `contains("Security")`, and log details.
</details>

<details>
<summary><strong>Exercise 6: Test Bean Lifecycle Execution Order</strong></summary>

**Prompt**: Create a bean implementing `InitializingBean`, `BeanNameAware`, and having `@PostConstruct`, verifying the exact console output sequence.

**Solution**:
Execution order:
1. Constructor
2. `setBeanName` (Aware)
3. `@PostConstruct`
4. `afterPropertiesSet()` (InitializingBean)
</details>

---

## 12. Summary & Quick Reference Cheatsheet

```
┌────────────────────────────────────────────────────────────────────────┐
│                   SPRING CORE & DI INTERVIEW CHEATSHEET                │
├────────────────────────────────────────────────────────────────────────┤
│ 1. Lifecycle: BFPP -> Instantiate -> Populate -> Aware -> BPP -> Init  │
│ 2. Proxies: CGLIB default in Boot 3; BPP creates proxy during post-init│
│ 3. Self-Invocation: this.method() bypasses proxy; use self.@Lazy       │
│ 4. Injection: Constructor with final fields strictly preferred         │
│ 5. Prototypes: Inject via ObjectProvider<T> inside Singleton beans     │
│ 6. Circular Refs: Banned by default; decouple via domain events        │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 13. Further Reading & Official Resources

- [Spring Core Technologies Reference Documentation](https://docs.spring.io/spring-framework/reference/core.html)
- [Spring Framework IoC Container Internals](https://docs.spring.io/spring-framework/reference/core/beans.html)
- [CGLIB vs JDK Dynamic Proxies in Spring](https://docs.spring.io/spring-framework/reference/core/aop/proxying.html)
- [Spring Boot 3 Migration Guide (AOT & Native Compilation)](https://github.com/spring-projects/spring-boot/wiki/Spring-Boot-3.0-Migration-Guide)
