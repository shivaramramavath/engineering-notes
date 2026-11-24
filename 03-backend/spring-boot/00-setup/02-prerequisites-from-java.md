# Java Prerequisites for Spring Boot Engineering

> Master the foundational Java mechanisms underpinning Spring Boot: Annotations and Meta-Annotations, Reflection, Bytecode Generation, JDK Dynamic vs. CGLIB Proxies, Generics Type Erasure, and the Jakarta Namespace Migration.

| Metadata | Value |
| :--- | :--- |
| **Level** | Beginner |
| **Prerequisites** | [Java Fundamentals](file:///01-languages/java/01-syntax-and-structure/) |
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
Spring Boot is not an abstract black box; it is an orchestrator built directly on core Java language features and JVM primitives. Developers who treat Spring as magic frequently struggle when debugging proxy interception failures, `LazyInitializationException` in transactional contexts, annotation inheritance surprises, reflection access restrictions under Java 9+ modules, or `NoSuchMethodError` caused by generic type erasure.

### Historical Evolution
- **Java 5 (2004)**: Introduced Annotations (`@interface`), Generics, and Enums, enabling Spring 2.5 to move away from pure XML into annotation-driven development (`@Autowired`, `@Component`).
- **Java 8 (2014)**: Introduced Default Interface Methods, Repeatable Annotations, Lambdas, and Streams, leading to Spring Security's modern functional configuration.
- **Java 9-17 (2017-2021)**: Introduced the Java Platform Module System (JPMS), restricting illegal reflective access (`--add-opens`), Records (`record`), Pattern Matching, and Sealed Classes.
- **Java 21-25 (2023-Present)**: Virtual Threads (JEP 444), Sequenced Collections (JEP 431), and String Templates revolutionize Spring Boot's concurrency and data handling.

---

## 2. Core Concepts

### 1. Annotations as Metadata & Composition
Spring annotations are runtime metadata (`@Retention(RetentionPolicy.RUNTIME)`). Spring Framework extends Java's native annotation model with **Meta-Annotations** (annotations placed on other annotations) and **Composed Annotations** (e.g. `@RestController` = `@Controller` + `@ResponseBody`).

### 2. Reflection & Introspection
Spring uses Java Reflection (`java.lang.reflect`) to inspect classes at runtime, scan for constructors, instantiate objects without hardcoded `new` invocations, and read private field types.

### 3. Dynamic Proxies (The Engine of AOP)
Spring implements Aspect-Oriented Programming (AOP), `@Transactional`, `@Async`, and `@Cacheable` using dynamic proxies:
- **JDK Dynamic Proxies**: Built-in Java (`java.lang.reflect.Proxy`), requires the target class to implement one or more interfaces.
- **CGLIB / ByteBuddy Proxies**: Subclassing proxies generated at runtime via bytecode generation; required when proxying concrete classes.

### 4. Generics & ResolvableType
Due to Java type erasure, generic type arguments (`List<User>`) are erased at runtime. Spring provides the `ResolvableType` API to navigate generic superclasses, interface signatures, and factory method return types.

---

## 3. How It Works Internally

### How Spring Intercepts Methods via Dynamic Proxies

```
+-------------------------------------------------------------+
|                Caller (e.g. OrderController)                |
+------------------------------+------------------------------+
                               |
                               | invokes orderService.placeOrder()
                               v
+-------------------------------------------------------------+
|          Spring Generated Proxy (CGLIB Subclass)            |
|  +-------------------------------------------------------+  |
|  | Interceptor / MethodInterceptor (e.g. TransactionAspect)|  |
|  | 1. Begin Database Transaction                         |  |
|  | 2. Delegate: target.placeOrder()                      |  |
|  +---------------------------+---------------------------+  |
+------------------------------|------------------------------+
                               |
                               v
+-------------------------------------------------------------+
|         Actual Target Bean (OrderServiceImpl)               |
|  - Executes raw business logic & database writes            |
+------------------------------+------------------------------+
                               |
                               v
+-------------------------------------------------------------+
|          Return to Proxy & Commit/Rollback                  |
|  3. TransactionManager.commit()                             |
|  4. Return result to Caller                                 |
+-------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### Creating Custom Composed Annotations in Java

```java
package com.learning.springboot.annotations;

import org.springframework.core.annotation.AliasFor;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RequestMethod;
import org.springframework.web.bind.annotation.RestController;
import java.lang.annotation.*;

@Target(ElementType.TYPE)
@Retention(RetentionPolicy.RUNTIME)
@Documented
@RestController
@RequestMapping
public @interface ApiEndpoint {

    @AliasFor(annotation = RequestMapping.class, attribute = "path")
    String[] path() default {};

    @AliasFor(annotation = RequestMapping.class, attribute = "value")
    String[] value() default {};
}
```

---

## 5. Practical Examples

### Example 1: Pure Java Dynamic Proxy Demonstration
- **Goal:** Show how JDK Dynamic Proxy intercepts method calls without Spring, illustrating how Spring's AOP works at the lowest JVM level.

```java
package com.learning.springboot.prereq;

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;

public class JdkProxyDemo {

    public interface PaymentService {
        void pay(double amount);
    }

    public static class PaymentServiceImpl implements PaymentService {
        @Override
        public void pay(double amount) {
            System.out.println("Processing actual payment of: $" + amount);
        }
    }

    public static class LoggingInvocationHandler implements InvocationHandler {
        private final Object target;

        public LoggingInvocationHandler(Object target) {
            this.target = target;
        }

        @Override
        public Object invoke(Object proxy, Method method, Object[] args) throws Throwable {
            System.out.println("[BEFORE] Invoking method: " + method.getName());
            Object result = method.invoke(target, args);
            System.out.println("[AFTER] Method completed: " + method.getName());
            return result;
        }
    }

    public static void main(String[] args) {
        PaymentService rawService = new PaymentServiceImpl();
        PaymentService proxyService = (PaymentService) Proxy.newProxyInstance(
            PaymentService.class.getClassLoader(),
            new Class<?>[]{PaymentService.class},
            new LoggingInvocationHandler(rawService)
        );

        proxyService.pay(250.00);
    }
}
```

- **Console Output:**
```text
[BEFORE] Invoking method: pay
Processing actual payment of: $250.0
[AFTER] Method completed: pay
```

- **Explanation:** Demonstrates that the caller talks directly to a synthetic proxy generated by the JVM. The proxy wraps the target call with cross-cutting logic (logging, transactions, metrics), which is the exact foundation of Spring AOP.

---

### Example 2: Resolving Generic Types Across Erasure via `ResolvableType`
- **Goal:** Demonstrate how Spring inspects and navigates generic parameters that would otherwise be erased by the JVM compiler.

```java
package com.learning.springboot.prereq;

import org.springframework.core.ResolvableType;
import java.util.List;

public class GenericsInspectionDemo {

    public static class EventRepository<T> {
        public List<T> findRecentEvents() {
            return List.of();
        }
    }

    public static class OrderEventRepository extends EventRepository<String> {}

    public static void main(String[] args) throws NoSuchMethodException {
        ResolvableType returnType = ResolvableType.forMethodReturnType(
            OrderEventRepository.class.getMethod("findRecentEvents")
        );

        System.out.println("Raw return type: " + returnType.resolve());
        System.out.println("Generic argument 0: " + returnType.getGeneric(0).resolve());
    }
}
```

- **Console Output:**
```text
Raw return type: interface java.util.List
Generic argument 0: class java.lang.String
```

- **Explanation:** Despite Java type erasure, generic type metadata of class hierarchies remains stored in the bytecode class signatures. Spring's `ResolvableType` navigates this metadata to enable dependency injection into generic collections (e.g. injecting all `List<Validator<Order>>`).

---

### Example 3: Edge Case – Reflection under Java 9+ Modules (JPMS)
- **Goal:** Understand reflection access barriers when accessing non-public constructors across module boundaries.

```java
package com.learning.springboot.prereq;

import java.lang.reflect.Constructor;

public class ReflectionAccessDemo {

    static class PrivateConfig {
        private final String secretKey;

        private PrivateConfig() {
            this.secretKey = "vault-token-xyz";
        }
    }

    public static void main(String[] args) throws Exception {
        Constructor<PrivateConfig> constructor = PrivateConfig.class.getDeclaredConstructor();
        // Required for Spring to instantiate package-private or private beans:
        constructor.setAccessible(true);
        PrivateConfig instance = constructor.newInstance();
        System.out.println("Instantiated private bean via reflection successfully!");
    }
}
```

- **Console Output:**
```text
Instantiated private bean via reflection successfully!
```

- **Explanation:** Demonstrates `setAccessible(true)`. In modular Java (`module-info.java`), packages containing Spring beans must be declared `opens com.mycompany.service to spring.core, spring.beans;` to allow the container to access non-public members.

---

### Example 4: Failure & Anti-Pattern – The Self-Invocation Proxy Trap
- **Goal:** Demonstrate why calling an annotated method from within the same class silently bypasses proxy interception.

```java
package com.learning.springboot.prereq;

public class SelfInvocationFailureDemo {

    public interface OrderService {
        void processOrder();
        void saveAuditLog();
    }

    public static class OrderServiceImpl implements OrderService {

        @Override
        public void processOrder() {
            System.out.println("Executing processOrder...");
            // ANTI-PATTERN: Direct self-invocation!
            // 'this' refers to the raw target object, NOT the proxy!
            saveAuditLog();
        }

        @Override
        public void saveAuditLog() {
            System.out.println("Executing saveAuditLog (Should be audited/transactional)...");
        }
    }

    public static void main(String[] args) {
        OrderService raw = new OrderServiceImpl();
        OrderService proxy = (OrderService) java.lang.reflect.Proxy.newProxyInstance(
            OrderService.class.getClassLoader(),
            new Class<?>[]{OrderService.class},
            (p, method, mArgs) -> {
                System.out.println(">> Intercepting: " + method.getName());
                return method.invoke(raw, mArgs);
            }
        );

        proxy.processOrder();
    }
}
```

- **Console Output:**
```text
>> Intercepting: processOrder
Executing processOrder...
Executing saveAuditLog (Should be audited/transactional)...
```

- **Explanation & Fix:** Notice that `>> Intercepting: saveAuditLog` was **never logged**! When `processOrder()` calls `saveAuditLog()`, it invokes the method on `this` (the un-proxied instance), silently bypassing `@Transactional`, `@Async`, and `@Cacheable`. The fix is to extract `saveAuditLog()` to a separate Spring bean, or use `TransactionTemplate`.

---

### Example 5: Modern Java Records as Immutable Spring DTOs
- **Goal:** Demonstrate how modern Java records replace boilerplate Lombok / JavaBeans in Spring Boot 3+.

```java
package com.learning.springboot.prereq;

import java.math.BigDecimal;
import java.time.Instant;
import java.util.UUID;

public class ModernRecordDtoDemo {

    public record CreateOrderRequest(
        UUID customerId,
        String sku,
        int quantity,
        BigDecimal unitPrice,
        Instant requestedAt
    ) {
        // Compact constructor for validation invariants
        public CreateOrderRequest {
            if (quantity <= 0) {
                throw new IllegalArgumentException("Quantity must be positive");
            }
            if (unitPrice.compareTo(BigDecimal.ZERO) <= 0) {
                throw new IllegalArgumentException("Price must be greater than zero");
            }
        }
    }

    public static void main(String[] args) {
        CreateOrderRequest req = new CreateOrderRequest(
            UUID.randomUUID(),
            "LAPTOP-M3",
            2,
            new BigDecimal("1299.99"),
            Instant.now()
        );
        System.out.println("Created immutable record: " + req.sku() + " x " + req.quantity());
    }
}
```

- **Console Output:**
```text
Created immutable record: LAPTOP-M3 x 2
```

- **Explanation:** Modern Java records provide transparent, immutable data containers with automatic `equals()`, `hashCode()`, and `toString()`. Jackson and Spring MVC serialize and deserialize records natively without reflection hacks or setters.

---

## 6. Best Practices

1. **Avoid Self-Invocation**: Never invoke a method annotated with `@Transactional`, `@Async`, or `@Cacheable` from another method in the same class.
2. **Use Records for DTOs**: Prefer immutable Java `record` declarations for HTTP request bodies, responses, and `@ConfigurationProperties`.
3. **Declare Injected Fields as Final**: Enforce constructor injection by making all dependencies `private final`.
4. **Be Mindful of Dynamic Proxies**: Final classes and final methods cannot be subclassed by CGLIB; keep Spring beans and `@Transactional` methods non-final.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Declaring Spring Beans as `final`** | CGLIB cannot generate a subclass proxy for a `final` class or `final` method | Remove the `final` modifier from `@Service`, `@Repository`, or `@Configuration` classes |
| **Calling Proxied Methods Internally via `this`** | Bypasses the dynamic proxy wrapper, ignoring `@Transactional` or `@Async` | Extract the method to a separate collaborator bean, or inject self via `@Lazy` |
| **Private `@Transactional` Methods** | Spring's standard CGLIB/JDK proxies cannot intercept private methods | Make all transactional and cached methods `public` |
| **Assuming Annotation Inheritance on Classes** | Standard Java does not inherit annotations from interfaces to implementing classes | Place Spring stereotype annotations (`@Service`, `@RestController`) directly on concrete classes |
| **Using `new` on a Managed Bean** | Instantiating a bean with `new OrderService()` bypasses Spring DI, leaving all `@Autowired` dependencies null | Always obtain beans from the Spring context via injection |
| **Relying on Generic Type Information at Runtime** | Type erasure strips generic type arguments, causing `ClassCastException` on untyped collections | Use Spring's `ResolvableType` or `ParameterizedTypeReference<T>` |

---

## 8. Performance & Security Notes

- **Reflection Cache**: Java reflection lookups (`Class.getMethod()`, `Class.getDeclaredFields()`) are expensive. Spring caches reflective lookups during startup in `ReflectionUtils` and `AnnotationUtils`.
- **CGLIB FastClass Optimization**: CGLIB generates index-based method invokers rather than standard Java reflective invocations, significantly reducing method call overhead.
- **Proxy Memory Overhead**: Each CGLIB proxy creates an additional runtime-generated class in the JVM Metaspace. Ensure sufficient `-XX:MaxMetaspaceSize=256m` in containerized environments.

---

## 9. Comparison / When to Use What

| Feature | JDK Dynamic Proxy | CGLIB Proxy (ByteBuddy) | Compile-Time AspectJ |
|:---|:---|:---|:---|
| **Underlying Mechanism** | `java.lang.reflect.Proxy` | Bytecode generation (subclassing) | Bytecode weaving at compile/load time |
| **Interface Requirement** | Mandatory | Not required (proxies concrete classes) | Not required |
| **Self-Invocation Interception**| No | No | Yes (Weaved directly into bytecode) |
| **Final Class / Method Support**| Yes (proxies interface) | No (Cannot subclass final) | Yes |
| **Default in Spring Boot** | Optional | **Default since Boot 2.0 (`proxyTargetClass=true`)** | Optional via `spring-aspects` |

---

## 10. Interview Questions

<details>
<summary><strong>Q1: Why does Spring Boot 2.0+ default to CGLIB proxies instead of JDK dynamic proxies?</strong></summary>

### Answer:
In legacy Spring, JDK dynamic proxies were default. If a developer had a class `UserServiceImpl implements UserService` and injected `UserServiceImpl` into a controller, Spring failed with `BeanNotOfRequiredTypeException` because the JDK proxy implemented `UserService` but was *not* an instance of `UserServiceImpl`. To eliminate this recurring developer trap, Spring Boot enabled `spring.aop.proxy-target-class=true` by default, generating CGLIB subclass proxies that can be injected as either the interface or the concrete class.
</details>

<details>
<summary><strong>Q2: Explain the "self-invocation" issue in Spring AOP. How does it occur and how do you resolve it?</strong></summary>

### Answer:
Spring AOP is proxy-based. When a client calls a bean method, it invokes the method on the proxy, which executes interceptor logic before delegating to the target object. When method A inside the target object calls method B on `this`, the call bypasses the proxy entirely, executing raw bytecode without transaction, security, or caching advice. Remedies include:
1. Refactoring: Moving method B to a separate collaborator bean.
2. Programmatic advice: Using `TransactionTemplate` instead of `@Transactional`.
3. Self-injection: Injecting `self` via constructor or `@Lazy`.
</details>

<details>
<summary><strong>Q3: How does Spring preserve generic types across JVM type erasure?</strong></summary>

### Answer:
While the JVM erases generic types of local variables and method invocations, the compiler preserves generic signatures in class headers and method metadata (attributes in the `.class` file format). Spring uses the `ResolvableType` API to inspect `Class.getGenericSuperclass()` and `Method.getGenericReturnType()`, allowing it to resolve the exact generic type parameters of dependencies and collection members.
</details>

<details>
<summary><strong>Q4: Why does `@Transactional` not work on `private` methods in Spring Boot?</strong></summary>

### Answer:
Spring AOP proxies wrap public methods. For JDK dynamic proxies, private methods cannot be part of an interface. For CGLIB subclass proxies, private methods cannot be overridden in a subclass. Consequently, the proxy generator ignores private methods. If transactional semantics are needed, the method must be `public` (or package-private if using full AspectJ weaving).
</details>

<details>
<summary><strong>Q5: What are composed annotations in Spring and how are `@AliasFor` attributes used?</strong></summary>

### Answer:
Composed annotations are custom annotations annotated with one or more Spring meta-annotations (e.g. `@GetMapping` is composed of `@RequestMapping(method = RequestMethod.GET)`). `@AliasFor` is used to create bidirectional attribute aliases between the custom annotation and the underlying meta-annotation attributes, allowing developers to customize values without explicitly referencing the target annotation.
</details>

<details>
<summary><strong>Q6: How does the Java Platform Module System (JPMS) affect Spring Boot reflection?</strong></summary>

### Answer:
JPMS (Java 9+) enforces strong encapsulation. By default, a module cannot use reflection (`setAccessible(true)`) to access non-public members of another module. To run on the module path, application modules must open their packages to Spring: `opens com.mycompany.app to spring.core, spring.beans, spring.context;`.
</details>

<details>
<summary><strong>Q7: What is the difference between `@Inherited` in standard Java and Spring's `@Inherited` handling?</strong></summary>

### Answer:
In standard Java, `@Inherited` only applies to class-level annotations inherited from superclasses, not from interfaces or to methods. Spring's `AnnotatedElementUtils` and `AnnotationUtils` provide deep searching algorithms that scan superclasses, implemented interfaces, and meta-annotations recursively, creating the illusion of universal annotation inheritance.
</details>

<details>
<summary><strong>Q8: When should an enterprise project switch from Spring AOP to full AspectJ?</strong></summary>

### Answer:
Switch to full AspectJ (compile-time or load-time weaving) when:
1. Self-invocation method interception is mandatory.
2. Methods must be intercepted on non-Spring-managed objects (e.g. entities instantiated via `new`).
3. Interception is required on `private` or `final` methods.
4. Maximum execution performance is required (AspectJ eliminates proxy method call overhead).
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Create a Composed `@ServiceEndpoint` Annotation</strong></summary>

**Problem:** Create a composed annotation `@ServiceEndpoint` that bundles `@RestController` and `@RequestMapping("/api/v1")`.

**Solution:**
```java
package com.learning.springboot.prereq;

import org.springframework.core.annotation.AliasFor;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;
import java.lang.annotation.*;

@Target(ElementType.TYPE)
@Retention(RetentionPolicy.RUNTIME)
@Documented
@RestController
@RequestMapping("/api/v1")
public @interface ServiceEndpoint {
    @AliasFor(annotation = RequestMapping.class, attribute = "path")
    String[] path() default {};
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Generic Repository Parameter Extraction</strong></summary>

**Problem:** Use `ResolvableType` to extract and print the generic entity type of a custom `Repository<T>` implementation.

**Solution:**
```java
package com.learning.springboot.prereq;

import org.springframework.core.ResolvableType;

public class GenericExtractor {
    interface Repository<T> {}
    static class AccountRepository implements Repository<Long> {}

    public static void main(String[] args) {
        ResolvableType type = ResolvableType.forClass(AccountRepository.class).as(Repository.class);
        System.out.println("Entity Type: " + type.getGeneric(0).resolve());
    }
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Custom Dynamic Method Execution Timer</strong></summary>

**Problem:** Write a pure Java `InvocationHandler` that intercepts any method call on an interface and measures its execution time in nanoseconds.

**Solution:**
```java
package com.learning.springboot.prereq;

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;

public class TimingInvocationHandler implements InvocationHandler {
    private final Object target;

    public TimingInvocationHandler(Object target) {
        this.target = target;
    }

    @Override
    public Object invoke(Object proxy, Method method, Object[] args) throws Throwable {
        long start = System.nanoTime();
        try {
            return method.invoke(target, args);
        } finally {
            long duration = System.nanoTime() - start;
            System.out.printf("Method %s executed in %d ns%n", method.getName(), duration);
        }
    }
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Enforce Immutable Record Validation</strong></summary>

**Problem:** Create a Java record representing a User Registration DTO that uses a compact constructor to enforce non-blank username and non-negative age.

**Solution:**
```java
package com.learning.springboot.prereq;

public record UserRegistration(String username, String email, int age) {
    public UserRegistration {
        if (username == null || username.isBlank()) {
            throw new IllegalArgumentException("Username cannot be blank");
        }
        if (age < 0) {
            throw new IllegalArgumentException("Age cannot be negative");
        }
    }
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Self-Injection Fix Implementation</strong></summary>

**Problem:** Refactor the self-invocation trap in a service bean by injecting `self` using `@Lazy` so that internal method calls traverse the Spring proxy.

**Solution:**
```java
package com.learning.springboot.prereq;

import org.springframework.context.annotation.Lazy;
import org.springframework.stereotype.Service;

@Service
public class OrderServiceFixed {

    private final OrderServiceFixed self;

    // Self-injection via constructor with @Lazy breaks circular dependency
    public OrderServiceFixed(@Lazy OrderServiceFixed self) {
        this.self = self;
    }

    public void processOrder() {
        System.out.println("Processing order...");
        // Traverses proxy correctly!
        self.saveAuditLog();
    }

    public void saveAuditLog() {
        System.out.println("Saving audit log with active transaction...");
    }
}
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Custom Runtime Annotation Processor</strong></summary>

**Problem:** Write a utility method that inspects an object at runtime and returns a `Map<String, Object>` of all fields marked with a custom `@AuditField` annotation.

**Solution:**
```java
package com.learning.springboot.prereq;

import java.lang.annotation.*;
import java.lang.reflect.Field;
import java.util.HashMap;
import java.util.Map;

@Target(ElementType.FIELD)
@Retention(RetentionPolicy.RUNTIME)
@interface AuditField {}

public class AnnotationAuditor {

    public static Map<String, Object> extractAuditFields(Object target) throws IllegalAccessException {
        Map<String, Object> results = new HashMap<>();
        Class<?> clazz = target.getClass();

        for (Field field : clazz.getDeclaredFields()) {
            if (field.isAnnotationPresent(AuditField.class)) {
                field.setAccessible(true);
                results.put(field.getName(), field.get(target));
            }
        }
        return results;
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **AOP Proxies**: Spring uses CGLIB subclassing by default; never invoke internal methods on `this` if proxy advice is expected.
- **Java Records**: Use immutable `record` types for all DTOs and configuration objects.
- **Generics**: Use Spring's `ResolvableType` to navigate type metadata preserved past type erasure.
- **Jakarta EE**: Always use `jakarta.persistence.*`, `jakarta.servlet.*`, `jakarta.validation.*`.

---

## 13. Further Reading
- [Java Language Specification: Annotations](https://docs.oracle.com/javase/specs/jls/se21/html/jls-9.html#jls-9.6)
- [Spring Framework Core Technologies: Customizing Beans](https://docs.spring.io/spring-framework/reference/core/beans/factory-extension.html)
- [Spring ResolvableType JavaDoc](https://docs.spring.io/spring-framework/docs/current/javadoc-api/org/springframework/core/ResolvableType.html)
