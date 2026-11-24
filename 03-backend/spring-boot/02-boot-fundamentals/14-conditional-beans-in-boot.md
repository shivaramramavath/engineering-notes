# Conditional Beans in Spring Boot

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | Spring IoC Container, BeanPostProcessor, Spring Boot Auto-Configuration |
| **Estimated Reading Time** | 24 minutes |
| **Status** | Production-Ready |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Baseline** | OpenJDK 21 LTS / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | OpenJDK 21.0.2 |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Configuration](#4-syntax--configuration)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance & Security Considerations](#8-performance--security-considerations)
9. [Comparison: Core Conditionals vs Custom Conditions](#9-comparison-core-conditionals-vs-custom-conditions)
10. [Interview Questions & Deep Dives](#10-interview-questions--deep-dives)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference](#12-summary--quick-reference)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

Spring Boot's "opinionated defaults with effortless escape hatches" philosophy is powered almost entirely by the `@Conditional` family of annotations. Without conditional registration, developers would be forced to configure every DataSource, ObjectMapper, security filter, and transaction manager explicitly, or suffer classpath bloat where unused beans are instantiated indiscriminately.

### The Problem
In complex enterprise architectures:
1. Applications run across varied environments (cloud, on-premise, test containers, serverless).
2. Different backing stores are used (PostgreSQL in production, H2 in dev, mock clients in unit tests).
3. Auto-configurations must yield automatically when a developer provides their own custom bean definition.

### The Solution: Conditional Beans
Conditional beans allow configuration classes and `@Bean` methods to evaluate runtime criteria (classes present on classpath, active environment properties, existing beans in context, web application types) before registering a bean definition. If conditions evaluate to false, the bean is completely skipped at zero runtime execution cost.

---

## 2. Core Concepts

### Mental Model & Evaluation Flow
```
                   Configuration Class / Bean Method Encountered
                                        │
                                        ▼
                           Inspect @Conditional Annotations
                     (@ConditionalOnClass, @ConditionalOnProperty, etc.)
                                        │
                                        ▼
                            Evaluate Condition Phase
                    ConfigurationPhase.PARSE_CONFIGURATION vs REGISTER_BEAN
                                        │
                                        ▼
                         Execute Condition.matches(...)
                       Query ConditionContext & Metadata
                                        │
                         ┌──────────────┴──────────────┐
                         ▼                             ▼
                    Condition Met               Condition Failed
                         │                             │
                         ▼                             ▼
             Register BeanDefinition into       Discard BeanDefinition
              DefaultListableBeanFactory        (Record reason in ConditionEvaluationReport)
```

### Invariants
1. **Ordering Sensitivity for `@ConditionalOnBean` / `@ConditionalOnMissingBean`**: Condition evaluations that inspect existing beans *depend strictly on configuration processing order*. Using `@ConditionalOnBean` in user configuration classes without explicit ordering can lead to nondeterministic bean creation.
2. **Safe Class Checking**: `@ConditionalOnClass` allows checking class presence using string class names (`name = "com.foo.Bar"`) to prevent runtime `NoClassDefFoundError` if the dependency is absent from the JVM classpath.
3. **Auditability**: Every evaluated condition is recorded in the `ConditionEvaluationReport`, accessible via Actuator's `/actuator/conditions` endpoint.

---

## 3. How It Works Internally

### Condition Evaluation Phases
Spring Framework evaluates conditions in two distinct phases (`ConfigurationCondition.ConfigurationPhase`):

1. **`PARSE_CONFIGURATION`**: Evaluated while Spring is parsing `@Configuration` classes into the configuration model. Used by `@ConditionalOnProperty`, `@ConditionalOnClass`, and `@ConditionalOnResource`. If this condition fails, the entire configuration class and all its `@Bean` methods are discarded immediately.
2. **`REGISTER_BEAN`**: Evaluated when beans are actually registered with the `BeanFactory`. Used by `@ConditionalOnBean` and `@ConditionalOnMissingBean`. This guarantees that other configuration classes have had a chance to register their beans first.

### The `SpringBootCondition` Base Class
All Spring Boot built-in conditions extend `org.springframework.boot.autoconfigure.condition.SpringBootCondition`:

```java
public abstract class SpringBootCondition implements Condition {
    @Override
    public final boolean matches(ConditionContext context, AnnotatedTypeMetadata metadata) {
        // Obtains class or method name for diagnostic reporting
        String classOrMethodName = getClassOrMethodName(metadata);
        
        // Template method implemented by concrete condition (e.g., OnClassCondition)
        ConditionOutcome outcome = getMatchOutcome(context, metadata);
        
        // Logs condition evaluation to ConditionEvaluationReport
        recordEvaluation(context, classOrMethodName, outcome);
        return outcome.isMatch();
    }

    public abstract ConditionOutcome getMatchOutcome(ConditionContext context, AnnotatedTypeMetadata metadata);
}
```

---

## 4. Syntax & Configuration

### Comprehensive Conditional Annotations Overview

| Annotation | Triggers When... |
| :--- | :--- |
| `@ConditionalOnClass` | Target classes exist on the JVM classpath |
| `@ConditionalOnMissingClass` | Target classes do NOT exist on the classpath |
| `@ConditionalOnBean` | Specific bean type or name already exists in the `BeanFactory` |
| `@ConditionalOnMissingBean` | No bean of the specified type or name exists in the `BeanFactory` |
| `@ConditionalOnProperty` | Specific environment property equals an expected value (or exists) |
| `@ConditionalOnResource` | Specific file exists on the classpath (e.g., `schema.sql`) |
| `@ConditionalOnWebApplication` | Context is a web app (SERVLET, REACTIVE, or ANY) |
| `@ConditionalOnNotWebApplication` | Context is NOT a web application (e.g., CLI, batch) |
| `@ConditionalOnExpression` | SpEL expression evaluates to `true` |
| `@ConditionalOnJava` | JVM runtime matches specified Java version range |

---

## 5. Practical Examples

### Example 1: Basic `@ConditionalOnProperty` with Defaults

```java
package com.example.conditional.payment;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.boot.autoconfigure.condition.ConditionalOnProperty;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;

public interface PaymentGateway {
    String processPayment(double amount);
}

class StripePaymentGateway implements PaymentGateway {
    @Override
    public String processPayment(double amount) {
        return "Processed $" + amount + " via Stripe Production Gateway";
    }
}

class MockPaymentGateway implements PaymentGateway {
    @Override
    public String processPayment(double amount) {
        return "Processed $" + amount + " via Mock Sandbox Gateway";
    }
}

@Configuration(proxyBeanMethods = false)
public class PaymentGatewayConfiguration {

    private static final Logger log = LoggerFactory.getLogger(PaymentGatewayConfiguration.class);

    @Bean
    @ConditionalOnProperty(name = "features.payment.provider", havingValue = "stripe")
    public PaymentGateway stripePaymentGateway() {
        log.info("Wiring StripePaymentGateway...");
        return new StripePaymentGateway();
    }

    @Bean
    @ConditionalOnProperty(name = "features.payment.provider", havingValue = "mock", matchIfMissing = true)
    public PaymentGateway mockPaymentGateway() {
        log.info("Wiring MockPaymentGateway (fallback/default)...");
        return new MockPaymentGateway();
    }
}
```

### Example 2: Enterprise Escape-Hatch Pattern with `@ConditionalOnMissingBean`

```java
package com.example.conditional.cache;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.SerializationFeature;
import com.fasterxml.jackson.datatype.jsr310.JavaTimeModule;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.boot.autoconfigure.condition.ConditionalOnMissingBean;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.context.annotation.Primary;

@Configuration(proxyBeanMethods = false)
public class JacksonAutoConfigurationDefaults {

    private static final Logger log = LoggerFactory.getLogger(JacksonAutoConfigurationDefaults.class);

    /**
     * Default ObjectMapper bean provided by auto-configuration.
     * If the user defines their own ObjectMapper bean anywhere in their code,
     * this method is skipped entirely.
     */
    @Bean
    @Primary
    @ConditionalOnMissingBean(ObjectMapper.class)
    public ObjectMapper defaultObjectMapper() {
        log.info("Registering default production-configured ObjectMapper bean");
        return new ObjectMapper()
                .registerModule(new JavaTimeModule())
                .disable(SerializationFeature.WRITE_DATES_AS_TIMESTAMPS);
    }
}
```

### Example 3: Edge Case — Safe Classpath Checking Using Class Names as Strings

When developing an optional integration with a third-party library (e.g., Redis or Kafka), referencing the class directly (`RedisTemplate.class`) in annotation attributes will cause a `ClassNotFoundException` if the user does not have the dependency in their classpath. Using String class names makes the condition completely safe.

```java
package com.example.conditional.integration;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.boot.autoconfigure.condition.ConditionalOnClass;
import org.springframework.boot.autoconfigure.condition.ConditionalOnMissingClass;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;

@Configuration(proxyBeanMethods = false)
public class SafeRedisIntegrationConfig {

    private static final Logger log = LoggerFactory.getLogger(SafeRedisIntegrationConfig.class);

    // Completely safe against ClassNotFoundException even if spring-data-redis is absent
    @Bean
    @ConditionalOnClass(name = "org.springframework.data.redis.core.RedisTemplate")
    public String redisIntegrationActive() {
        log.info("Redis libraries detected on classpath! Activating Redis integration adapter.");
        return "REDIS_SUPPORT_ACTIVE";
    }

    @Bean
    @ConditionalOnMissingClass("org.springframework.data.redis.core.RedisTemplate")
    public String inMemoryFallbackActive() {
        log.warn("Redis libraries absent! Falling back to local in-memory storage.");
        return "IN_MEMORY_FALLBACK_ACTIVE";
    }
}
```

### Example 4: Anti-Pattern vs Production Fix

#### Anti-Pattern: Misusing `@ConditionalOnBean` in User Configuration Classes
```java
// ANTI-PATTERN: Depending on bean registration order in standard user configuration!
@Configuration
public class MisguidedUserConfig {

    // If UserSecurityConfig registers customSecurityManager AFTER this class is parsed,
    // this condition unexpectedly evaluates to FALSE and registers the fallback!
    @Bean
    @ConditionalOnMissingBean(SecurityManager.class)
    public SecurityManager fallbackSecurityManager() {
        return new DefaultSecurityManager();
    }
}
```

#### Production Fix: Use Auto-Configuration Ordering or Explicit `@AutoConfigureAfter`
```java
// PRODUCTION FIX: Place in an AutoConfiguration class with strict order constraints
@AutoConfiguration
@AutoConfigureAfter(name = "com.example.security.UserSecurityAutoConfiguration")
public class SafeFallbackSecurityAutoConfiguration {

    @Bean
    @ConditionalOnMissingBean(SecurityManager.class)
    public SecurityManager fallbackSecurityManager() {
        return new DefaultSecurityManager();
    }
}
```

### Example 5: High-Performance Custom `SpringBootCondition`

```java
package com.example.conditional.custom;

import org.springframework.boot.autoconfigure.condition.ConditionOutcome;
import org.springframework.boot.autoconfigure.condition.SpringBootCondition;
import org.springframework.context.annotation.ConditionContext;
import org.springframework.context.annotation.Conditional;
import org.springframework.core.type.AnnotatedTypeMetadata;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.nio.file.Files;
import java.nio.file.Path;

@Target({ElementType.TYPE, ElementType.METHOD})
@Retention(RetentionPolicy.RUNTIME)
@Conditional(OnKubernetesEnvironmentCondition.class)
public @interface ConditionalOnKubernetes {
}

class OnKubernetesEnvironmentCondition extends SpringBootCondition {

    private static final Path K8S_SERVICE_ACCOUNT_PATH = 
            Path.of("/var/run/secrets/kubernetes.io/serviceaccount/token");

    @Override
    public ConditionOutcome getMatchOutcome(ConditionContext context, AnnotatedTypeMetadata metadata) {
        // Fast environmental probe: check K8s service account secret file existence
        boolean runningInK8s = Files.exists(K8S_SERVICE_ACCOUNT_PATH) || 
                               System.getenv("KUBERNETES_SERVICE_HOST") != null;

        if (runningInK8s) {
            return ConditionOutcome.match("Kubernetes runtime environment detected");
        }

        return ConditionOutcome.noMatch("Not running within a Kubernetes cluster");
    }
}
```

---

## 6. Best Practices

1. **Keep Conditions Fast and Side-Effect Free**: Conditions are evaluated repeatedly during startup. Never perform blocking network calls or slow disk I/O inside `Condition.matches()`.
2. **Use String Class Names for Optional Dependencies**: Avoid referencing classes directly (`MyClass.class`) when writing libraries whose dependencies might not be present at runtime.
3. **Always Provide Clear Failure Messages in `ConditionOutcome`**: Use `ConditionOutcome.noMatch(...)` with clear reasons so administrators can inspect failure causes in `/actuator/conditions`.
4. **Reserve `@ConditionalOnBean` for Auto-Configurations**: Do not use `@ConditionalOnBean` or `@ConditionalOnMissingBean` across standard `@Configuration` classes without controlling configuration evaluation order.
5. **Combine with `@AutoConfiguration`**: When building starter libraries, put conditional beans inside auto-configuration classes referenced in `META-INF/spring/org.springframework.boot.autoconfigure.AutoConfiguration.imports`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Using `@ConditionalOnBean` in User Config** | User `@Configuration` classes are parsed before auto-configurations, leading to erratic evaluation results. | Only use `@ConditionalOnBean` in `@AutoConfiguration` classes with explicit `@AutoConfigureAfter`. |
| **Referencing Class Directly in `@ConditionalOnClass`** | Triggers `NoClassDefFoundError` during class loading if the dependency is absent. | Use string variant: `@ConditionalOnClass(name = "com.foo.Bar")`. |
| **Forgetting `matchIfMissing = true` on Defaults** | Fallback bean is never created if the property is absent from `application.yaml`. | Add `matchIfMissing = true` to `@ConditionalOnProperty` for default fallbacks. |
| **Using Heavy Logic in Custom Conditions** | Slows down application startup across all instances. | Cache static checks or keep condition checks limited to environment properties and class checks. |
| **Expecting Conditional Beans to React at Runtime** | Conditions are evaluated once during startup. Changing a property later does not trigger bean re-evaluation. | For dynamic behavior, use `@RefreshScope` or design dynamic proxies instead of conditionals. |
| **SpEL Expression Typos in `@ConditionalOnExpression`** | Evaluates to false or throws runtime syntax exceptions silently. | Prefer `@ConditionalOnProperty` or strongly typed custom conditions over SpEL expressions. |

---

## 8. Performance & Security Considerations

### Startup Latency Impact
Each condition evaluation incurs introspection and classloader checks. Spring Boot optimizes this by caching classloader lookups and evaluating cheap conditions (`@ConditionalOnProperty`) before expensive ones (`@ConditionalOnBean`). When writing custom conditions, check simple conditions first.

### Security Invariants
Conditional beans must not be used to enforce authorization rules or dynamic tenant switching. Conditions are container-level structural gates evaluated once during boot, not runtime per-request security guards.

---

## 9. Comparison: Core Conditionals vs Custom Conditions

| Aspect | Built-in Annotations (`@ConditionalOn*`) | Custom `SpringBootCondition` |
| :--- | :--- | :--- |
| **Implementation Effort** | Zero (just add annotation) | Moderate (custom Java class) |
| **Maintainability** | High (standardized, familiar) | Lower (custom logic to test and document) |
| **Actuator Visibility** | Automatically reported in `/actuator/conditions` | Requires `ConditionOutcome` messages |
| **Flexibility** | Covers 95% of typical use cases | Handles complex OS, filesystem, or hardware checks |
| **Best Used For** | Classpath, Properties, Bean Presence | Platform runtime, hypervisor, or hardware detection |

---

## 10. Interview Questions & Deep Dives

<details>
<summary><strong>Q1: Why does Spring Boot advise against using `@ConditionalOnBean` in user `@Configuration` classes?</strong></summary>

**Answer:**
User configuration classes are parsed in arbitrary order during the initial context scanning phase. If Bean A is conditional on Bean B, but Bean B is declared in a configuration class parsed *after* Bean A's class, Bean A's condition will evaluate to false even though Bean B would have existed later. In `@AutoConfiguration` classes, Spring Boot strictly manages ordering via `@AutoConfigureBefore`, `@AutoConfigureAfter`, and `@AutoConfigureOrder`, making `@ConditionalOnBean` deterministic.
</details>

<details>
<summary><strong>Q2: What is the difference between `ConfigurationPhase.PARSE_CONFIGURATION` and `ConfigurationPhase.REGISTER_BEAN`?</strong></summary>

**Answer:**
- `PARSE_CONFIGURATION`: Evaluated while Spring parses the `@Configuration` class. If false, the class and its `@Bean` methods are not even read into the model.
- `REGISTER_BEAN`: Evaluated later when individual `@Bean` definitions are being registered into the `BeanFactory`. This is necessary for bean-existence conditions like `@ConditionalOnBean`, because all configuration classes must be parsed before knowing all potential bean registrations.
</details>

<details>
<summary><strong>Q3: How does `@ConditionalOnMissingBean` enable the "starter" pattern in Spring Boot?</strong></summary>

**Answer:**
Starters provide opinionated, pre-configured beans (e.g., `DataSource`, `ObjectMapper`, `RestTemplate`) annotated with `@ConditionalOnMissingBean`. If a developer declares their own custom bean of that type in their application code, the starter's conditional bean definition is discarded, allowing the user's bean to take precedence without bean definition collision errors.
</details>

<details>
<summary><strong>Q4: How do you inspect which conditional beans matched and which failed in a running application?</strong></summary>

**Answer:**
1. Enable the Actuator conditions endpoint: `management.endpoints.web.exposure.include=conditions`. Then inspect `GET /actuator/conditions`.
2. Start the application with `--debug` or `-Ddebug=true`. Spring Boot will print the `ConditionEvaluationReport` to standard output, displaying detailed positive and negative match logs.
</details>

<details>
<summary><strong>Q5: Can you apply multiple conditional annotations to a single `@Bean` method? What is the evaluation logic?</strong></summary>

**Answer:**
Yes. When multiple conditional annotations are placed on a class or method, Spring treats them as a logical **AND**. All conditions must evaluate to `true` for the bean to be registered. If any single condition fails, the bean is skipped.
</details>

<details>
<summary><strong>Q6: How does `@ConditionalOnProperty` behave when `havingValue` is omitted?</strong></summary>

**Answer:**
If `havingValue` is not specified, `@ConditionalOnProperty` evaluates to `true` as long as the property exists in the environment and its value is not equal to `"false"`.
</details>

<details>
<summary><strong>Q7: Why should library authors use `name = "com.example.Class"` instead of `value = com.example.Class.class` in `@ConditionalOnClass`?</strong></summary>

**Answer:**
Using `.class` literals forces the JVM classloader to resolve the class when loading the bytecode of the configuration class. If the library is absent from the runtime classpath, the JVM throws `NoClassDefFoundError`. Using the String variant allows Spring to safely check whether the class exists using reflection (`Class.forName`) without crashing if it is missing.
</details>

<details>
<summary><strong>Q8: What is the purpose of `ConditionOutcome` in `SpringBootCondition`?</strong></summary>

**Answer:**
`ConditionOutcome` encapsulates the boolean match result (`isMatch()`) along with an explanatory diagnostic message (`ConditionMessage`). This message is stored in the `ConditionEvaluationReport`, providing visibility into why a bean was or was not instantiated.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Create a Property-Guarded Feature Flag Bean</strong></summary>

**Task**: Write a `@Bean` definition that creates a `NotificationService` only when `features.email-notifications.enabled=true`.

```java
// Solution:
@Configuration(proxyBeanMethods = false)
public class NotificationConfig {
    @Bean
    @ConditionalOnProperty(name = "features.email-notifications.enabled", havingValue = "true")
    public NotificationService emailNotificationService() {
        return new EmailNotificationService();
    }
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Fallback InMemory Cache</strong></summary>

**Task**: Write an auto-configuration method that creates an `InMemoryCache` if no bean of type `CacheManager` exists in the context.

```java
// Solution:
@Bean
@ConditionalOnMissingBean(CacheManager.class)
public CacheManager inMemoryCacheManager() {
    return new ConcurrentMapCacheManager();
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Safe Classpath-Guarded Metric Reporter</strong></summary>

**Task**: Create a configuration class that registers a `PrometheusReporter` bean only if `io.prometheus.client.CollectorRegistry` is present on the classpath, using safe String class names.

```java
// Solution:
@Configuration(proxyBeanMethods = false)
public class PrometheusConfig {
    @Bean
    @ConditionalOnClass(name = "io.prometheus.client.CollectorRegistry")
    public Object prometheusReporter() {
        return new PrometheusReporter();
    }
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Custom Operating System Condition</strong></summary>

**Task**: Implement a custom `SpringBootCondition` that matches only when the host OS is Linux.

```java
// Solution:
public class OnLinuxCondition extends SpringBootCondition {
    @Override
    public ConditionOutcome getMatchOutcome(ConditionContext context, AnnotatedTypeMetadata metadata) {
        String os = System.getProperty("os.name").toLowerCase();
        if (os.contains("linux")) {
            return ConditionOutcome.match("Running on Linux OS: " + os);
        }
        return ConditionOutcome.noMatch("Non-Linux OS detected: " + os);
    }
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Multi-Tenant Conditional Data Source Factory</strong></summary>

**Task**: Build a configuration that checks for properties `tenants[0].id` and dynamically registers distinct DataSource beans only if multi-tenancy mode is set to `DATABASE` in `application.yaml`.
</details>

<details>
<summary><strong>Exercise 6 (Hard): Unit Testing Conditional Configurations with `ApplicationContextRunner`</strong></summary>

**Task**: Write a JUnit 5 test using Spring Boot's `ApplicationContextRunner` verifying that a custom configuration class registers Bean A when property `foo=bar` is set, and does NOT register Bean A when `foo=baz`.
</details>

---

## 12. Summary & Quick Reference

- **Escape Hatch Pattern**: Combine `@Bean` with `@ConditionalOnMissingBean` for sensible defaults.
- **Safe Classpath Check**: Use `@ConditionalOnClass(name = "...")` to avoid class loading crashes.
- **Diagnostic Endpoint**: `/actuator/conditions` reveals why beans were created or skipped.
- **Golden Rule**: Never use `@ConditionalOnBean` in user configuration classes without explicit ordering.

---

## 13. Further Reading

- [Spring Boot Official Documentation: Creating Your Own Auto-Configuration](https://docs.spring.io/spring-boot/reference/using/spring-boot-starter-custom.html)
- [Spring Framework Core: The @Conditional Annotation](https://docs.spring.io/spring-framework/reference/core/beans/java/conditional.html)
- [ConditionEvaluationReport and Actuator Diagnostics](https://docs.spring.io/spring-boot/reference/actuator/endpoints.html#actuator.conditions)
- [Effective Spring Boot Starter Development](https://spring.io/guides)
