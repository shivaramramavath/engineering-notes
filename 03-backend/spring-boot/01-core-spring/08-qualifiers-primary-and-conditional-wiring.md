# Qualifiers, Primary Beans & Conditional Wiring

> The definitive guide to bean disambiguation and conditional registration: mastering `@Qualifier`, `@Primary`, Spring Framework 6.2's `@Fallback`, custom type-safe qualifiers, the `@Conditional` engine, and the exact 5-step autowiring resolution algorithm.

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [IoC Container & ApplicationContext Internals](file:///03-backend/spring-boot/01-core-spring/01-ioc-container-and-application-context.md), [Spring Beans & BeanDefinitions](file:///03-backend/spring-boot/01-core-spring/02-beans-and-bean-definitions.md) |
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
In real-world applications, multiple classes often implement the same interface. For example, a system might have both a `StripePaymentGateway` and a `PayPalPaymentGateway` implementing `PaymentGateway`. If a service declares a dependency on `PaymentGateway`, Spring's autowiring mechanism encounters multiple candidates of the same type and throws a fatal `NoUniqueBeanDefinitionException`. Furthermore, modular platforms require conditional bean activation based on environment variables, classpath dependencies, or licensing tiers.

### Historical Evolution
- **Spring 2.5**: Introduced `@Qualifier` and `@Primary` to resolve bean ambiguity.
- **Spring 4.0**: Introduced the generalized `@Conditional` infrastructure and `Condition` interface, enabling Spring Boot's revolutionary auto-configuration engine.
- **Spring 6.2 & Boot 3.4 (2024)**: Introduced the `@Fallback` annotation, allowing libraries to declare fallback beans that yield whenever any primary or regular bean is configured by user code.

### Real-World Relevance
Mastering disambiguation and conditional wiring allows developers to build pluggable architectures, write reusable shared libraries and starters, and safely evolve complex enterprise microservices without breaking existing autowired injection points.

---

## 2. Core Concepts

### 1. Disambiguation Primitives
- **`@Primary`**: Designates a bean as the default candidate when multiple beans match a target type. If no qualifier is specified at the injection point, the `@Primary` bean is chosen.
- **`@Qualifier`**: Fine-grained disambiguation. Placed on bean definitions and injection points to bind specific implementations by name or qualifier value.
- **`@Fallback` (Spring 6.2+)**: Marks a bean as a fallback candidate. If multiple candidates exist, regular and `@Primary` beans always take precedence over `@Fallback` beans. A fallback bean is only chosen if no other matching bean is available.

### 2. Custom Type-Safe Qualifiers
String-based qualifiers (`@Qualifier("postgresDataSource")`) are prone to typo bugs. Spring allows developers to define custom Java annotations meta-annotated with `@Qualifier` containing enums or attributes for compile-time safety.

### 3. Conditional Registration (`@Conditional`)
The `@Conditional` annotation conditionally registers a bean or configuration class based on the result of a custom `Condition` implementation:
```java
public interface Condition {
    boolean matches(ConditionContext context, AnnotatedTypeMetadata metadata);
}
```

---

## 3. How It Works Internally

### Spring's 5-Step Autowiring Candidate Resolution Algorithm

```
+-------------------------------------------------------------------------+
|                  1. Discover All Beans Matching Target Type             |
|  - E.g., `PaymentGateway` -> finds [StripeGateway, PayPalGateway]       |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                  2. Evaluate Explicit @Qualifier                        |
|  - Is @Qualifier present on injection point?                            |
|    * YES: Filter candidates matching qualifier value or annotation.     |
|           If exactly 1 matches -> SELECT BEAN & RETURN!                 |
|    * NO:  Proceed to Step 3.                                            |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                  3. Evaluate @Primary Flag                              |
|  - Is exactly ONE candidate annotated with @Primary?                    |
|    * YES: SELECT PRIMARY BEAN & RETURN!                                 |
|    * NO:  If multiple @Primary -> THROW NoUniqueBeanDefinitionException!|
|    * NONE: Proceed to Step 4.                                           |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                  4. Evaluate @Fallback Flag (Spring 6.2+)               |
|  - Filter out any candidate beans annotated with @Fallback              |
|    * If exactly ONE non-fallback candidate remains -> SELECT & RETURN!  |
|    * If NO non-fallback candidates remain -> Pick single @Fallback bean |
|    * If multiple candidates remain -> Proceed to Step 5.                |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                  5. Fallback to Target Field / Parameter Name           |
|  - Does any candidate bean name match the injection parameter name?     |
|    (e.g., `PaymentGateway stripeGateway`)                               |
|    * YES: SELECT MATCHING BEAN & RETURN!                                |
|    * NO:  THROW NoUniqueBeanDefinitionException!                        |
+-------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### Basic `@Primary` and `@Qualifier`
```java
// Definition
@Component
@Primary
public class StripeGateway implements PaymentGateway {}

@Component
@Qualifier("paypal")
public class PayPalGateway implements PaymentGateway {}

// Injection
@Service
public class CheckoutService {
    // Injects PayPalGateway via explicit qualifier
    public CheckoutService(@Qualifier("paypal") PaymentGateway gateway) { ... }
}
```

### Spring 6.2 `@Fallback` Definition
```java
@Component
@Fallback // Used ONLY if no other NotificationClient bean is defined
public class DefaultLoggingNotificationClient implements NotificationClient {}
```

---

## 5. Practical Examples

### Example 1: Disambiguation with `@Primary` and `@Qualifier`
```java
package com.example.demo.disambiguation;

import org.springframework.beans.factory.annotation.Qualifier;
import org.springframework.context.annotation.Primary;
import org.springframework.stereotype.Component;
import org.springframework.stereotype.Service;

interface Formatter {
    String format(String text);
}

@Component
@Primary // Default choice
class JsonFormatter implements Formatter {
    @Override public String format(String text) { return "{\"text\":\"" + text + "\"}"; }
}

@Component
@Qualifier("xml")
class XmlFormatter implements Formatter {
    @Override public String format(String text) { return "<text>" + text + "</text>"; }
}

@Service
public class DocumentService {

    private final Formatter defaultFormatter;
    private final Formatter xmlFormatter;

    // defaultFormatter resolves to JsonFormatter via @Primary
    // xmlFormatter resolves to XmlFormatter via @Qualifier("xml")
    public DocumentService(Formatter defaultFormatter, @Qualifier("xml") Formatter xmlFormatter) {
        this.defaultFormatter = defaultFormatter;
        this.xmlFormatter = xmlFormatter;
    }

    public void render() {
        System.out.println("Default: " + defaultFormatter.format("Hello"));
        System.out.println("XML: " + xmlFormatter.format("Hello"));
    }
}
```

### Example 2: Custom Type-Safe Qualifier Annotation
```java
package com.example.demo.qualifiers;

import org.springframework.beans.factory.annotation.Qualifier;

import java.lang.annotation.*;

// Enum representing datasource tiers
public enum StorageTier {
    HOT_CACHE,
    COLD_ARCHIVE
}

// Custom Type-Safe Qualifier Annotation
@Target({ElementType.FIELD, ElementType.PARAMETER, ElementType.TYPE, ElementType.METHOD})
@Retention(RetentionPolicy.RUNTIME)
@Qualifier // Meta-annotated with @Qualifier
public @interface Storage {
    StorageTier value();
}
```
```java
package com.example.demo.qualifiers;

import org.springframework.stereotype.Component;
import org.springframework.stereotype.Service;

interface StorageRepository {
    void store(String key, String data);
}

@Component
@Storage(StorageTier.HOT_CACHE)
class RedisStorageRepository implements StorageRepository {
    @Override public void store(String key, String data) { System.out.println("Stored in Redis: " + key); }
}

@Component
@Storage(StorageTier.COLD_ARCHIVE)
class S3StorageRepository implements StorageRepository {
    @Override public void store(String key, String data) { System.out.println("Stored in S3: " + key); }
}

@Service
public class BackupService {
    private final StorageRepository coldStorage;

    // Type-safe injection: No magic strings!
    public BackupService(@Storage(StorageTier.COLD_ARCHIVE) StorageRepository coldStorage) {
        this.coldStorage = coldStorage;
    }

    public void backup(String id, String payload) {
        coldStorage.store(id, payload);
    }
}
```

### Example 3: Spring 6.2 `@Fallback` for Default Library Beans
```java
package com.example.demo.fallback;

import org.springframework.context.annotation.Fallback;
import org.springframework.stereotype.Component;
import org.springframework.stereotype.Service;

interface MetricPublisher {
    void publish(String metric);
}

// Fallback provided by starter library: Active only if user defines no custom bean
@Component
@Fallback
class NoOpMetricPublisher implements MetricPublisher {
    @Override public void publish(String metric) {
        // No-op fallback
    }
}

// User-defined custom bean (takes priority automatically without @Primary!)
@Component
class CloudWatchMetricPublisher implements MetricPublisher {
    @Override public void publish(String metric) {
        System.out.println("Publishing to CloudWatch: " + metric);
    }
}

@Service
public class MetricService {
    private final MetricPublisher publisher;

    // Automatically receives CloudWatchMetricPublisher because NoOp is marked @Fallback
    public MetricService(MetricPublisher publisher) {
        this.publisher = publisher;
    }
}
```

### Example 4: `NoUniqueBeanDefinitionException` Anti-Pattern & Remediation
```java
// ANTI-PATTERN: Two beans match interface without @Primary or @Qualifier
interface SmsGateway {}

@Component class TwilioSmsGateway implements SmsGateway {}
@Component class InfobipSmsGateway implements SmsGateway {}

@Service
public class NotificationManager {
    // CRASH ON STARTUP:
    // NoUniqueBeanDefinitionException: No qualifying bean of type 'SmsGateway' available:
    // expected single matching bean but found 2: twilioSmsGateway, infobipSmsGateway
    public NotificationManager(SmsGateway gateway) { ... }
}

// REMEDIATION 1: Designate one as @Primary
@Component
@Primary
class TwilioSmsGateway implements SmsGateway {}

// REMEDIATION 2: Inject specific bean via @Qualifier
@Service
public class NotificationManager {
    public NotificationManager(@Qualifier("twilioSmsGateway") SmsGateway gateway) {
        // Explicit and unambiguous
    }
}
```

### Example 5: Custom `@Conditional` Evaluation
```java
package com.example.demo.conditional;

import org.springframework.context.annotation.*;
import org.springframework.core.type.AnnotatedTypeMetadata;

// Custom condition checking if running on Linux
class OnLinuxCondition implements Condition {
    @Override
    public boolean matches(ConditionContext context, AnnotatedTypeMetadata metadata) {
        String os = context.getEnvironment().getProperty("os.name", "");
        return os.toLowerCase().contains("linux");
    }
}

// Annotation wrapper
@Target({ElementType.TYPE, ElementType.METHOD})
@Retention(RetentionPolicy.RUNTIME)
@Conditional(OnLinuxCondition.class)
public @interface ConditionalOnLinux {}

@Configuration(proxyBeanMethods = false)
public class NativeTerminalConfig {

    @Bean
    @ConditionalOnLinux
    public String terminalDriver() {
        return "Linux-epoll-native-driver";
    }
}
```

---

## 6. Best Practices

1. **Prefer Custom Qualifier Annotations Over String Literals**: Creating `@Database(DatabaseType.REPLICA)` prevents hard-to-detect runtime typos common with `@Qualifier("replicaDataSource")`.
2. **Use `@Primary` Sparingly**: Mark only the most natural, default implementation as `@Primary`. Multiple `@Primary` annotations on the same type cause fatal startup collisions.
3. **Use Spring 6.2 `@Fallback` for Starters**: When writing platform libraries, mark default dummy/in-memory implementations with `@Fallback` so consumers can override them simply by declaring a regular `@Component`.
4. **Never Rely on Parameter Name Matching (Step 5)**: While Spring will match beans by parameter name if no qualifier is present, compiler flags (`-parameters`) or code minification can strip parameter names, causing sudden failures in production.
5. **Combine Conditionals with Clear Failure Diagnostics**: When using custom `@Conditional` classes, implement `SpringBootCondition` to output clear diagnostics in Actuator `/actuator/conditions`.
6. **Use `@ConditionalOnProperty` with `matchIfMissing`**: In Spring Boot starters, always specify `matchIfMissing = false` (or `true`) explicitly to make default behavior crystal clear.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Multiple `@Primary` annotations on same type** | Spring cannot choose between two beans both marked `@Primary`, throwing `NoUniqueBeanDefinitionException`. | Ensure at most ONE bean of a given type is marked `@Primary`. |
| **Typo in String `@Qualifier`** | `@Qualifier("striepGateway")` finds no matching bean and throws `NoSuchBeanDefinitionException`. | Use type-safe custom qualifier annotations or verify bean name. |
| **Relying on Parameter Name Matching** | Compiling without `-parameters` flag erases method argument names (`arg0`), breaking Step 5 name-matching. | Always use an explicit `@Qualifier` or `@Primary`. |
| **Heavy I/O inside `Condition.matches()`** | Custom condition making network calls or database checks freezes container bootstrap. | Only inspect `Environment` properties, classpath resources, or BeanFactory metadata. |
| **Misunderstanding `@Fallback` precedence** | Assuming `@Fallback` overrides regular beans. | Remember: Regular beans ALWAYS override `@Fallback` beans. |
| **Using `@Qualifier` without `@Autowired` on setters** | Placing `@Qualifier` on a setter without `@Autowired` is ignored by Spring. | Place `@Autowired` on the setter or switch to constructor injection. |

---

## 8. Performance & Security Notes

### Condition Evaluation Caching
During bootstrap, Spring evaluates hundreds of conditional checks. Custom `Condition` classes are evaluated repeatedly:
- Keep `Condition.matches()` purely algorithmic (checking environment strings and class presence).
- Never open socket connections, query databases, or perform disk I/O inside `matches()`.

### Security Hardening
Do not use user-controlled runtime input to dynamically construct qualifier strings or evaluate condition expressions via SpEL (`@ConditionalOnExpression`), as this exposes the application to expression injection vulnerabilities.

---

## 9. Comparison / When to Use What

| Disambiguation Tool | Priority Order | Injected If | Best For |
| :--- | :---: | :--- | :--- |
| **`@Qualifier("name")`** | 1 (Highest) | Exact qualifier match requested | Explicit targeted injection |
| **`@Primary`** | 2 | No qualifier specified; picks primary | Standard default production choice |
| **`@Fallback`** | 4 (Lowest) | No regular or primary beans exist | Library fallback / default dummy beans |
| **Parameter Name** | 3 | No qualifier; name matches bean ID | Implicit fallback (not recommended) |
| **`@Conditional`** | Pre-creation | Controls whether bean definition exists | Environment/feature toggle gating |

---

## 10. Interview Questions

<details>
<summary><strong>Q1: What happens if two beans of the same type exist and neither is marked `@Primary` or `@Qualifier`?</strong></summary>

**Answer:**  
Spring attempts Step 5 of its candidate resolution algorithm: it checks if the injection point's parameter or field name matches the bean name of either candidate. If one matches, that bean is injected. If neither matches (or if compiler parameter metadata was stripped), Spring throws a `NoUniqueBeanDefinitionException` and halts application startup.
</details>

<details>
<summary><strong>Q2: What is the purpose of the `@Fallback` annotation introduced in Spring Framework 6.2?</strong></summary>

**Answer:**  
`@Fallback` marks a bean as a secondary fallback candidate. Unlike `@Primary` (which takes precedence over other beans), a `@Fallback` bean yields to ALL other non-fallback candidates. It is only selected if no other bean matching the target type is found in the container. This eliminates the need for `@ConditionalOnMissingBean` in many starter library scenarios.
</details>

<details>
<summary><strong>Q3: How do you create a custom type-safe qualifier?</strong></summary>

**Answer:**  
Create a custom Java annotation and meta-annotate it with `@Qualifier`:
```java
@Target({ElementType.FIELD, ElementType.PARAMETER, ElementType.TYPE, ElementType.METHOD})
@Retention(RetentionPolicy.RUNTIME)
@Qualifier
public @interface DatabaseType {
    String value();
}
```
You can then annotate both the `@Component` and the injection point with `@DatabaseType("primary")`, eliminating fragile magic strings.
</details>

<details>
<summary><strong>Q4: What is the difference between `@Qualifier` on a `@Bean` definition vs. on an injection point?</strong></summary>

**Answer:**  
- On a **`@Bean` method or `@Component`**: It assigns a qualifier tag/alias to the bean, distinguishing it from other beans of the same type.
- On an **injection point (constructor param or field)**: It instructs Spring to filter candidate beans and inject ONLY the bean whose qualifier tag matches.
</details>

<details>
<summary><strong>Q5: Can `@Qualifier` and `@Primary` be used together on different beans of the same type?</strong></summary>

**Answer:**  
Yes. A common enterprise pattern is to mark the standard implementation as `@Primary` (so most services get it by default without annotations) while marking specialized implementations with `@Qualifier` (so specific services can explicitly opt in to the non-default bean).
</details>

<details>
<summary><strong>Q6: How does Spring Boot's `@ConditionalOnProperty` work?</strong></summary>

**Answer:**  
`@ConditionalOnProperty` checks the Spring `Environment` for a specified property name:
- `havingValue`: The bean is created only if the property equals this value.
- `matchIfMissing = true`: The bean is created if the property is absent from configuration.
- If the property value doesn't match and `matchIfMissing` is false, the bean is discarded before instantiation.
</details>

<details>
<summary><strong>Q7: Can a class have multiple `@Qualifier` annotations?</strong></summary>

**Answer:**  
Yes, Spring supports multiple qualifiers on a single bean. An injection point can declare multiple qualifiers, and Spring will select the bean that matches ALL declared qualifiers.
</details>

<details>
<summary><strong>Q8: What is the difference between `@ConditionalOnBean` and `@ConditionalOnMissingBean`?</strong></summary>

**Answer:**  
- `@ConditionalOnBean`: The bean is created ONLY if another specified bean already exists in the `ApplicationContext`.
- `@ConditionalOnMissingBean`: The bean is created ONLY if no bean of the specified type exists in the context (commonly used in auto-configurations to provide default beans that users can override).
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Resolve Ambiguity with `@Primary` (Easy)</strong></summary>

**Task:**  
Create two implementations of `EmailSender`: `SmtpEmailSender` and `MockEmailSender`. Without adding any annotations at the injection point in `UserService`, configure `MockEmailSender` as `@Primary` and verify it is injected.
</details>

<details>
<summary><strong>Exercise 2: Create a Custom Qualifier Annotation (Easy)</strong></summary>

**Task:**  
Create an annotation `@PaymentProvider(ProviderType.STRIPE)` using an enum. Apply it to `StripePaymentService` and inject it into `OrderService` using the custom annotation.
</details>

<details>
<summary><strong>Exercise 3: Test Spring 6.2 `@Fallback` Precedence (Medium)</strong></summary>

**Task:**  
Declare a `DefaultPricingEngine` annotated with `@Fallback`. In a test, verify it is injected when alone. Then register a `CustomPricingEngine` (unannotated) and verify that `CustomPricingEngine` is automatically injected without modifying the injection site.
</details>

<details>
<summary><strong>Exercise 4: Custom Environment Condition (Medium)</strong></summary>

**Task:**  
Write a custom `Condition` that checks if the system has at least 4 available CPU cores (`Runtime.getRuntime().availableProcessors() >= 4`). Wire a `ParallelDataProcessor` bean conditionally using `@Conditional`.
</details>

<details>
<summary><strong>Exercise 5: Build a Feature-Toggled Starter Bean (Hard)</strong></summary>

**Task:**  
Create a `@ConditionalOnProperty(prefix = "app.feature", name = "ai-assistant", havingValue = "enabled", matchIfMissing = false)` bean. Write unit tests with `@TestPropertySource` testing both enabled and disabled states.
</details>

<details>
<summary><strong>Exercise 6: ArchUnit Enforcing Type-Safe Qualifiers (Hard)</strong></summary>

**Task:**  
Write an ArchUnit test asserting that no field or parameter in the codebase uses standard string-based `@Qualifier("...")`; mandate that only custom meta-annotated qualifiers (e.g., `@Database`, `@Channel`) may be used.
</details>

---

## 12. Summary / Cheat Sheet

| Mechanism | Purpose | Precedence Rule |
| :--- | :--- | :--- |
| **`@Qualifier("name")`** | Specific disambiguation by key | Explicitly overrides `@Primary` and `@Fallback` |
| **`@Primary`** | Sets default bean candidate | Overrides regular candidates when no qualifier used |
| **`@Fallback`** | Sets backup bean candidate | Yields to ALL non-fallback beans (Spring 6.2+) |
| **Parameter Name** | Implicit name-based fallback | Used only if no qualifier or primary bean matches |
| **`@Conditional`** | Boolean evaluation gating bean creation | Evaluated during bean definition registration |
| **`@ConditionalOnProperty`** | Config property toggle | Creates bean based on `application.yaml` property |

---

## 13. Further Reading
- [Official Spring Framework Reference: Fine-tuning Annotation-based Autowiring with Qualifiers](https://docs.spring.io/spring-framework/reference/core/beans/annotation-config/autowired-qualifiers.html)
- [Spring Framework 6.2 Release Notes: Fallback Beans](https://github.com/spring-projects/spring-framework/wiki/Spring-Framework-6.2-Release-Notes)
- [Spring Boot Auto-configuration Conditions](https://docs.spring.io/spring-boot/docs/current/reference/html/features.html#features.developing-auto-configuration.condition-annotations)
