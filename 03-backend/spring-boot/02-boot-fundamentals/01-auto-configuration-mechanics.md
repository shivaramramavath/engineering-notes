# Auto-Configuration Mechanics: How Spring Boot Configures Itself

> The definitive architectural deep-dive into Spring Boot's auto-configuration engine: `AutoConfigurationImportSelector`, `ImportCandidates`, the modern `AutoConfiguration.imports` registry, conditional evaluation cascades, auto-configuration ordering, and testing with `ApplicationContextRunner`.

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [IoC Container & ApplicationContext Internals](file:///03-backend/spring-boot/01-core-spring/01-ioc-container-and-application-context.md), [Qualifiers, Primary & Conditional Wiring](file:///03-backend/spring-boot/01-core-spring/08-qualifiers-primary-and-conditional-wiring.md) |
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
In traditional Spring Framework applications, configuring a basic stack—such as a database connection pool, Hibernate EntityManager, transaction manager, Jackson JSON mapper, and embedded web server—required writing hundreds of lines of boilerplate XML or `@Configuration` code. If any dependency was added to the classpath, developers had to manually wire matching beans. Auto-configuration solves this by automatically guessing and configuring the beans you need based on the JARs detected on your classpath.

### Historical Evolution
- **Spring Boot 1.0 - 2.6 (2014-2021)**: Auto-configurations were registered in `META-INF/spring.factories` under the key `org.springframework.boot.autoconfigure.EnableAutoConfiguration`.
- **Spring Boot 2.7 (2022)**: Introduced the new dedicated registry file `META-INF/spring/org.springframework.boot.autoconfigure.AutoConfiguration.imports`.
- **Spring Boot 3.0+ & 3.4+ (Present)**: Completely removed support for `spring.factories` auto-configuration. Introduced `@AutoConfiguration` with type-safe ordering (`after`, `before`) and deep Ahead-of-Time (AOT) pre-computation.

### Real-World Relevance
Understanding auto-configuration mechanics dispels the myth of "Spring Boot magic." It allows engineers to customize default behaviors, debug condition evaluation failures, write custom company starters, and selectively disable unwanted auto-configurations.

---

## 2. Core Concepts

### 1. The `@EnableAutoConfiguration` Annotation
Included automatically within `@SpringBootApplication`. It imports `AutoConfigurationImportSelector`, which scans the classpath for auto-configuration entry points.

### 2. The `AutoConfiguration.imports` Registry File
In Spring Boot 3+, auto-configuration classes are discovered strictly from:
`META-INF/spring/org.springframework.boot.autoconfigure.AutoConfiguration.imports`  
This file contains a simple newline-separated list of fully qualified configuration class names.

### 3. The `@AutoConfiguration` Annotation
Modern replacement for `@Configuration` on auto-configuration classes. It provides built-in attributes for deterministic sequencing:
- `after`: Run after specified configuration classes.
- `before`: Run before specified configuration classes.
- `proxyBeanMethods`: Defaults to `false` (Lite Mode) for optimal performance.

### 4. Condition Evaluation Report
Spring Boot evaluates conditions in three categories:
1. **Positive Matches**: Conditions that passed, leading to bean creation.
2. **Negative Matches**: Conditions that failed, causing configurations to be skipped.
3. **Exclusions**: Configurations explicitly excluded by the developer.

---

## 3. How It Works Internally

### The Auto-Configuration Discovery & Filtering Lifecycle

```
+-------------------------------------------------------------------------+
|                  1. Bootstrap Trigger: @SpringBootApplication           |
|  - Triggers `@EnableAutoConfiguration`                                  |
|  - Imports `AutoConfigurationImportSelector`                            |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|             2. Read `AutoConfiguration.imports` via ImportCandidates    |
|  - Scans all JARs on classpath for:                                     |
|    `META-INF/spring/org.springframework.boot.autoconfigure...imports`   |
|  - Aggregates ~150 standard Boot auto-configuration classes             |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|             3. Filter Exclusions                                        |
|  - Strips classes declared in `@SpringBootApplication(exclude = ...)`   |
|  - Strips classes declared in `spring.autoconfigure.exclude` YAML key   |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|             4. Evaluate Conditional Annotations                         |
|  - `@ConditionalOnClass`: Is the required library present on classpath? |
|  - `@ConditionalOnProperty`: Is the feature enabled in application.yaml?|
|  - `@ConditionalOnMissingBean`: Has the user already defined this bean? |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|             5. Topological Sort & Execution                             |
|  - Sorts matching classes via `@AutoConfigureBefore` / `After`          |
|  - Registers surviving bean definitions into `ApplicationContext`       |
+-------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### Declaring an Auto-Configuration Class
```java
package com.example.autoconfigure;

import org.springframework.boot.autoconfigure.AutoConfiguration;
import org.springframework.boot.autoconfigure.condition.ConditionalOnClass;
import org.springframework.boot.autoconfigure.condition.ConditionalOnMissingBean;
import org.springframework.context.annotation.Bean;

@AutoConfiguration // proxyBeanMethods = false by default
@ConditionalOnClass(RedisClient.class) // Only active if RedisClient is on classpath
public class RedisCacheAutoConfiguration {

    @Bean
    @ConditionalOnMissingBean // Backs off if user defines their own RedisClient
    public RedisClient redisClient() {
        return new RedisClient("localhost", 6379);
    }
}
```

### Excluding Unwanted Auto-Configurations
```java
// Method 1: Annotation-based exclusion
@SpringBootApplication(exclude = {
    DataSourceAutoConfiguration.class,
    SecurityAutoConfiguration.class
})
public class DemoApplication {}
```
```yaml
# Method 2: Property-based exclusion in application.yaml
spring:
  autoconfigure:
    exclude:
      - org.springframework.boot.autoconfigure.jdbc.DataSourceAutoConfiguration
```

---

## 5. Practical Examples

### Example 1: Complete Auto-Configuration with Ordering
```java
package com.example.autoconfigure.payment;

import org.springframework.boot.autoconfigure.AutoConfiguration;
import org.springframework.boot.autoconfigure.AutoConfigureAfter;
import org.springframework.boot.autoconfigure.condition.*;
import org.springframework.boot.autoconfigure.jdbc.DataSourceAutoConfiguration;
import org.springframework.boot.context.properties.EnableConfigurationProperties;
import org.springframework.context.annotation.Bean;

import javax.sql.DataSource;

// Auto-configuration executes strictly AFTER DataSourceAutoConfiguration
@AutoConfiguration
@AutoConfigureAfter(DataSourceAutoConfiguration.class)
@ConditionalOnClass(DataSource.class)
@EnableConfigurationProperties(PaymentProperties.class)
public class PaymentGatewayAutoConfiguration {

    @Bean
    @ConditionalOnMissingBean
    @ConditionalOnProperty(prefix = "payment", name = "enabled", havingValue = "true", matchIfMissing = true)
    public PaymentService paymentService(DataSource dataSource, PaymentProperties properties) {
        return new JdbcPaymentService(dataSource, properties.getApiKey());
    }
}
```

### Example 2: Inspecting Condition Evaluation Report via `--debug`
Run the application with the `--debug` flag:
```bash
./mvnw spring-boot:run -Dspring-boot.run.arguments="--debug"
```
Console output snippet:
```text
============================
CONDITIONS EVALUATION REPORT
============================

Positive matches:
-----------------
   DataSourceAutoConfiguration matched:
      - @ConditionalOnClass found required class 'javax.sql.DataSource' (OnClassCondition)

   JacksonAutoConfiguration matched:
      - @ConditionalOnClass found required class 'com.fasterxml.jackson.databind.ObjectMapper'

Negative matches:
-----------------
   MongoDataAutoConfiguration:
      Did not match:
         - @ConditionalOnClass did not find required class 'com.mongodb.client.MongoClient'

Exclusions:
-----------
   None
```

### Example 3: Disabling Batch Auto-Configuration in Web Microservice
```yaml
# application.yaml
spring:
  autoconfigure:
    exclude:
      - org.springframework.boot.autoconfigure.batch.BatchAutoConfiguration
      - org.springframework.boot.autoconfigure.jms.JmsAutoConfiguration
```

### Example 4: Auto-Configuration Ordering Anti-Pattern & Remediation
```java
// ANTI-PATTERN: Auto-configuration depends on another auto-configuration's bean without ordering
@AutoConfiguration
public class BadMetricsAutoConfiguration {

    // FLAW: If BadMetricsAutoConfiguration runs before InfluxDbAutoConfiguration,
    // InfluxDbClient does NOT exist yet, causing NoSuchBeanDefinitionException or condition failure!
    @Bean
    @ConditionalOnBean(InfluxDbClient.class)
    public MetricsPublisher metricsPublisher(InfluxDbClient client) {
        return new InfluxMetricsPublisher(client);
    }
}

// REMEDIATION: Declare explicit topological ordering using @AutoConfigureAfter
@AutoConfiguration(after = InfluxDbAutoConfiguration.class)
public class CleanMetricsAutoConfiguration {

    // SAFE: InfluxDbAutoConfiguration is guaranteed to have run first!
    @Bean
    @ConditionalOnBean(InfluxDbClient.class)
    public MetricsPublisher metricsPublisher(InfluxDbClient client) {
        return new InfluxMetricsPublisher(client);
    }
}
```

### Example 5: Testing Auto-Configuration in Isolation with `ApplicationContextRunner`
```java
package com.example.autoconfigure;

import org.junit.jupiter.api.Test;
import org.springframework.boot.autoconfigure.AutoConfigurations;
import org.springframework.boot.test.context.runner.ApplicationContextRunner;

import static org.assertj.core.api.Assertions.assertThat;

class PaymentGatewayAutoConfigurationTest {

    // Fast, lightweight test harness that boots in ~50ms without starting a web server
    private final ApplicationContextRunner contextRunner = new ApplicationContextRunner()
        .withConfiguration(AutoConfigurations.of(PaymentGatewayAutoConfiguration.class));

    @Test
    void shouldBackOffWhenUserDefinesCustomService() {
        contextRunner
            .withUserConfiguration(CustomUserConfig.class) // User provides own bean
            .run(context -> {
                assertThat(context).hasSingleBean(PaymentService.class);
                assertThat(context).getBean(PaymentService.class).isInstanceOf(CustomPaymentService.class);
            });
    }

    @Test
    void shouldNotLoadWhenDisabledByProperty() {
        contextRunner
            .withPropertyValues("payment.enabled=false")
            .run(context -> {
                assertThat(context).doesNotHaveBean(PaymentService.class);
            });
    }

    static class CustomUserConfig {
        @org.springframework.context.annotation.Bean
        public PaymentService paymentService() { return new CustomPaymentService(); }
    }
    static class CustomPaymentService implements PaymentService {}
}
```

---

## 6. Best Practices

1. **Always Use `@ConditionalOnMissingBean` on Factory Beans**: Guarantees that users can override your auto-configured beans simply by defining a standard `@Bean` in their own `@Configuration` classes.
2. **Always Use `AutoConfiguration.imports` in Spring Boot 3+**: Never place auto-configuration classes in `spring.factories`. Boot 3 silently ignores `EnableAutoConfiguration` in `spring.factories`.
3. **Keep Auto-Configuration Packages Outside Component Scan**: Place auto-configuration classes in a package separate from your application root (e.g., `com.company.starter.autoconfigure`) to prevent accidental premature scanning.
4. **Use `ApplicationContextRunner` for Fast Unit Testing**: Test auto-configurations in pure isolation using `ApplicationContextRunner`; avoids the overhead of `@SpringBootTest`.
5. **Always Specify Topological Ordering (`after`/`before`)**: If your auto-configuration relies on beans created by another auto-configuration (e.g., `DataSource`, `ObjectMapper`), specify `@AutoConfiguration(after = ...)`.
6. **Use `@AutoConfiguration` Over Standard `@Configuration`**: `@AutoConfiguration` sets `proxyBeanMethods = false` by default and integrates with Spring Boot's internal sorting engine.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Placing auto-config in scanned package** | If scanned by `@ComponentScan`, it runs as a regular configuration, ignoring auto-configuration ordering rules. | Move auto-configuration to a separate, non-scanned package. |
| **Using `spring.factories` in Boot 3** | Spring Boot 3 completely dropped `EnableAutoConfiguration` parsing in `spring.factories`. | Use `META-INF/spring/org.springframework.boot.autoconfigure.AutoConfiguration.imports`. |
| **Missing `@ConditionalOnMissingBean`** | If a user declares their own bean, the application crashes with `NoUniqueBeanDefinitionException` instead of backing off. | Add `@ConditionalOnMissingBean` to auto-configuration `@Bean` methods. |
| **Missing `matchIfMissing` in `@ConditionalOnProperty`** | If property is absent, the condition fails unexpectedly when the feature was intended to be enabled by default. | Explicitly define `matchIfMissing = true` (or `false`). |
| **Relying on `@ConditionalOnBean` without ordering** | If TargetAutoConfig hasn't executed yet, `@ConditionalOnBean` evaluates to false prematurely. | Add `@AutoConfigureAfter(TargetAutoConfig.class)`. |
| **Heavy I/O in condition evaluation** | Custom conditions opening network connections stall container bootstrap. | Restrict conditions to classpath checks, properties, and bean definitions. |

---

## 8. Performance & Security Notes

### Startup Overhead of Unused Auto-Configurations
Spring Boot evaluates ~150 conditions on every cold start. If an application never uses MongoDB, Cassandra, or RabbitMQ:
- Condition evaluation takes less than 20 milliseconds because bytecode checks (`SimpleMetadataReader`) avoid classloading.
- In GraalVM Ahead-of-Time (AOT) compilation, condition checks are evaluated once at build time, completely eliminating evaluation latency at runtime!

### Security Audit of Active Starters
Periodically run `./mvnw dependency:tree` or inspect Actuator `/actuator/conditions` to ensure unvetted third-party starters (which may auto-configure exposed endpoints) are not inadvertently active in production.

---

## 9. Comparison / When to Use What

| Configuration Mechanism | Discovery Method | Execution Timing | Best For |
| :--- | :--- | :--- | :--- |
| **`@Configuration` (Standard)** | `@ComponentScan` | Standard bean registration | Application-level business services |
| **`@AutoConfiguration`** | `AutoConfiguration.imports` | Sorted, conditional bootstrap | Reusable internal platform starters |
| **`@Import`** | Explicit class reference | Embedded with importing class | Modular sub-configurations |

---

## 10. Interview Questions

<details>
<summary><strong>Q1: How does Spring Boot discover auto-configuration classes in Spring Boot 3.x?</strong></summary>

**Answer:**  
In Spring Boot 3.x, `AutoConfigurationImportSelector` reads the file:
`META-INF/spring/org.springframework.boot.autoconfigure.AutoConfiguration.imports`  
using `ImportCandidates.load()`. It aggregates all listed class names from all JARs on the classpath, filters exclusions, evaluates conditional annotations, topologically sorts them, and registers the surviving bean definitions.
</details>

<details>
<summary><strong>Q2: Why was `META-INF/spring.factories` replaced in Spring Boot 2.7 / 3.0?</strong></summary>

**Answer:**  
`spring.factories` was an overloaded property file used for everything: initializers, listeners, auto-configurations, and environment post-processors. Parsing a monolithic key-value file with hundreds of multi-line values was slow and memory-intensive. Spring Boot split these into dedicated, clean `.imports` files for each SPI, improving parsing speed and GraalVM AOT compatibility.
</details>

<details>
<summary><strong>Q3: What is the purpose of `@ConditionalOnMissingBean`?</strong></summary>

**Answer:**  
It instructs Spring Boot to create the annotated bean ONLY if no other bean of that type (or name) has already been registered in the `ApplicationContext`. This allows Spring Boot starters to supply sensible defaults while allowing end-users to override them simply by declaring their own `@Bean`.
</details>

<details>
<summary><strong>Q4: Why must `@AutoConfigureAfter` be used when writing a condition with `@ConditionalOnBean`?</strong></summary>

**Answer:**  
Because auto-configurations execute in an ordered sequence. If AutoConfig A checks `@ConditionalOnBean(Database.class)` but AutoConfig B (which creates `Database`) has not executed yet, the condition evaluates to `false` prematurely. Adding `@AutoConfigureAfter(AutoConfigB.class)` guarantees that B executes and registers its beans before A evaluates its condition.
</details>

<details>
<summary><strong>Q5: How can a developer debug which auto-configurations ran and why certain beans were not created?</strong></summary>

**Answer:**  
1. Start the application with the `--debug` command-line flag or set `debug=true` in `application.properties`.
2. Inspect the **CONDITIONS EVALUATION REPORT** printed to stdout.
3. In production, query the Actuator `/actuator/conditions` HTTP endpoint to inspect positive matches, negative matches, and exclusions.
</details>

<details>
<summary><strong>Q6: How do you completely disable an auto-configuration like `SecurityAutoConfiguration`?</strong></summary>

**Answer:**  
Two ways:
1. In code: `@SpringBootApplication(exclude = SecurityAutoConfiguration.class)`
2. In configuration: `spring.autoconfigure.exclude=org.springframework.boot.autoconfigure.security.servlet.SecurityAutoConfiguration`
</details>

<details>
<summary><strong>Q7: What is `ApplicationContextRunner` and why is it preferred for testing auto-configurations?</strong></summary>

**Answer:**  
`ApplicationContextRunner` is a specialized, lightweight test utility from `org.springframework.boot.test.context.runner`. It allows testing auto-configuration classes in pure isolation with custom properties, mocked classes, or user configurations without the overhead of bootstrapping a real web server or complete `@SpringBootTest`.
</details>

<details>
<summary><strong>Q8: Why should auto-configuration classes never reside in the same package as `@SpringBootApplication`?</strong></summary>

**Answer:**  
If an auto-configuration class is in the application's base package, it will be discovered by `@ComponentScan` and registered as a standard eager `@Configuration` bean. This completely bypasses the auto-configuration lifecycle, ignoring `@AutoConfigureBefore`/`After` ordering and causing conditions to fail unpredictably.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Inspect Conditions Report via `--debug` (Easy)</strong></summary>

**Task:**  
Launch your Spring Boot application with `--debug`. Search the console output for `DataSourceAutoConfiguration` and identify whether it was a positive or negative match, and which specific class triggered the match.
</details>

<details>
<summary><strong>Exercise 2: Exclude DataSourceAutoConfiguration (Easy)</strong></summary>

**Task:**  
Exclude `DataSourceAutoConfiguration` via `application.yaml`. Boot the application and verify in the debug log that it appears under the `Exclusions:` section.
</details>

<details>
<summary><strong>Exercise 3: Test Auto-Configuration with `ApplicationContextRunner` (Medium)</strong></summary>

**Task:**  
Write a unit test using `ApplicationContextRunner` testing a custom auto-configuration. Assert that `context.hasSingleBean(MyService.class)` is true when property `app.service.enabled=true`, and false when `app.service.enabled=false`.
</details>

<details>
<summary><strong>Exercise 4: Verify User Override with `@ConditionalOnMissingBean` (Medium)</strong></summary>

**Task:**  
In the test from Exercise 3, supply a user `@Configuration` that provides a custom implementation of `MyService`. Assert that the user's bean is present and the auto-configured bean backed off.
</details>

<details>
<summary><strong>Exercise 5: Build a Complete Custom Auto-Configuration (Hard)</strong></summary>

**Task:**  
Create a standalone auto-configuration class `AuditingAutoConfiguration`. Create `src/main/resources/META-INF/spring/org.springframework.boot.autoconfigure.AutoConfiguration.imports`. Add the class name, package into a JAR, and verify an external application automatically loads it.
</details>

<details>
<summary><strong>Exercise 6: Topological Sort Verification (Hard)</strong></summary>

**Task:**  
Create `AutoConfigA` and `AutoConfigB` where B has `@AutoConfigureAfter(AutoConfigA.class)`. Add console print statements in their `@Bean` methods and verify that A's bean is always created strictly before B's bean.
</details>

---

## 12. Summary / Cheat Sheet

| Feature / Artifact | Path / Syntax | Purpose |
| :--- | :--- | :--- |
| **Registry File** | `META-INF/spring/...AutoConfiguration.imports` | Lists all auto-configuration classes |
| **Annotation** | `@AutoConfiguration(after = ..., before = ...)`| Modern auto-config class declaration |
| **Back-off Rule** | `@ConditionalOnMissingBean` | Yields when user declares custom bean |
| **Classpath Rule** | `@ConditionalOnClass(Target.class)` | Active only if library is on classpath |
| **Property Toggle** | `@ConditionalOnProperty(prefix="x", name="y")`| Active based on `application.yaml` |
| **Debug Flag** | `--debug` | Prints full condition evaluation report |
| **Exclusion** | `@SpringBootApplication(exclude = {...})` | Permanently disables target auto-config |
| **Testing Harness** | `ApplicationContextRunner` | Fast, isolated auto-configuration unit tests |

---

## 13. Further Reading
- [Official Spring Boot Reference: Creating Your Own Auto-configuration](https://docs.spring.io/spring-boot/docs/current/reference/html/features.html#features.developing-auto-configuration)
- [Spring Boot AutoConfiguration.imports Specification](https://github.com/spring-projects/spring-boot/wiki/Spring-Boot-2.7-Release-Notes#new-autoconfiguration-annotation)
- [Spring Boot Condition Evaluation Report](https://docs.spring.io/spring-boot/docs/current/reference/html/features.html#features.spring-application.condition-evaluation-report)
