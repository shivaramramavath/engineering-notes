# Profiles in Spring Boot: Multi-Document YAML & Profile Groups

> Master production profile management in Spring Boot: multi-document YAML architectures (`---`), modern `spring.config.activate.on-profile`, composite profile groups, configuration precedence, and eliminating environment configuration drift.

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [Profiles & Environment Abstraction](file:///03-backend/spring-boot/01-core-spring/09-profiles-and-environment-abstraction.md), [Externalized Configuration & Property Hierarchy](file:///03-backend/spring-boot/02-boot-fundamentals/03-externalized-configuration-and-properties.md) |
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
Managing separate property files for every environment (`application-dev.yaml`, `application-test.yaml`, `application-staging.yaml`, `application-prod.yaml`) often leads to configuration sprawl and drift. When a new property key is added to `application.yaml`, developers frequently forget to update `application-prod.yaml`, leading to missing property exceptions when deploying to production.

### Historical Evolution
- **Spring Boot 1.x - 2.3**: Used legacy `spring.profiles` property and separate `application-{profile}.properties` files. Profile processing order was unpredictable when files referenced each other.
- **Spring Boot 2.4 (2020)**: Overhauled the configuration processing pipeline via **Config Data Processing**. Introduced multi-document YAML files separated by `---`, standardized on `spring.config.activate.on-profile`, and introduced Profile Groups.
- **Spring Boot 3.4+**: Streamlined Kubernetes ConfigTree volume mounting and profile activation.

### Real-World Relevance
Modern cloud-native backends consolidate environment defaults into a clean, single-file multi-document YAML architecture while leveraging profile groups to compose observability, database, and cloud infrastructure bundles.

---

## 2. Core Concepts

### 1. Multi-Document YAML (`---`)
A single `application.yaml` file can contain multiple configuration documents separated by triple hyphens (`---`).
- The **first document** contains global default properties active across all profiles.
- Subsequent documents declare `spring.config.activate.on-profile: {profile-name}` to override specific keys when that profile is active.

### 2. `spring.config.activate.on-profile`
Replaces the deprecated `spring.profiles` property. It binds the enclosing YAML document to a specific profile or boolean profile expression:
```yaml
---
spring:
  config:
    activate:
      on-profile: "prod"
```

### 3. Profile Groups
Allows developers to define aliases that bundle multiple related profiles under a single umbrella name:
```yaml
spring:
  profiles:
    group:
      production:
        - "prod"
        - "metrics"
        - "audit-logging"
        - "aws"
```
Activating `--spring.profiles.active=production` automatically activates all four constituent profiles.

### 4. `spring.profiles.active` vs. `spring.profiles.include`
- **`active`**: Sets the primary active profile(s) for the running process.
- **`include`**: Unconditionally forces additional secondary profiles to be activated whenever the declaring profile is active.

---

## 3. How It Works Internally

### Multi-Document YAML Processing Pipeline

```
+-------------------------------------------------------------------------+
|                  src/main/resources/application.yaml                    |
|                                                                         |
|  [ Document 1: Global Defaults ]                                        |
|  server.port: 8080                                                      |
|  app.cache.ttl: 60s                                                     |
|  ---------------------------------------------------------------------  |
|  [ Document 2: on-profile: dev ]                                        |
|  server.port: 8081                                                      |
|  spring.devtools.livereload.enabled: true                               |
|  ---------------------------------------------------------------------  |
|  [ Document 3: on-profile: prod ]                                       |
|  server.shutdown: graceful                                              |
|  app.cache.ttl: 300s                                                    |
+------------------------------------+------------------------------------+
                                     |
              Application launched with: `--spring.profiles.active=prod`
                                     |
                                     v
+-------------------------------------------------------------------------+
|             ConfigDataEnvironmentPostProcessor                          |
|  1. Parses Document 1 -> Loads base properties into Environment         |
|  2. Evaluates Document 2 -> `dev` is NOT active -> Document SKIPPED!    |
|  3. Evaluates Document 3 -> `prod` IS active -> Loads overrides!        |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|             Resolved Runtime Environment Configuration                  |
|  - `server.port`: 8080 (from Document 1)                                |
|  - `server.shutdown`: graceful (from Document 3)                        |
|  - `app.cache.ttl`: 300s (Document 3 cleanly overrides Document 1!)    |
+-------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### Consolidated Multi-Document `application.yaml`
```yaml
# =========================================================================
# Document 1: Global Base Configuration (Always Active)
# =========================================================================
server:
  port: 8080
spring:
  application:
    name: order-service
app:
  rate-limit: 100
  timeout: 5s

---
# =========================================================================
# Document 2: Development Environment
# =========================================================================
spring:
  config:
    activate:
      on-profile: "dev"
server:
  port: 8081 # Dev-specific port override
logging:
  level:
    com.example: DEBUG

---
# =========================================================================
# Document 3: Production Environment
# =========================================================================
spring:
  config:
    activate:
      on-profile: "prod"
server:
  shutdown: graceful
app:
  rate-limit: 5000
  timeout: 2s
logging:
  level:
    com.example: INFO
```

---

## 5. Practical Examples

### Example 1: Modern Profile Groups for Enterprise Microservices
```yaml
# application.yaml
spring:
  profiles:
    group:
      # Developer local setup bundle
      local-dev:
        - "dev"
        - "h2-database"
        - "mock-payment"
      # Certified production cluster bundle
      production-cloud:
        - "prod"
        - "postgres"
        - "stripe-real"
        - "datadog-telemetry"
```
```bash
# Launching developer bundle with one clean flag:
./mvnw spring-boot:run -Dspring-boot.run.arguments="--spring.profiles.active=local-dev"
```

### Example 2: Complex Boolean Activation Conditions
```yaml
---
# Activated ONLY when running on Kubernetes in Staging or Production
spring:
  config:
    activate:
      on-profile: "k8s & (staging | prod)"
management:
  endpoints:
    web:
      exposure:
        include: "health,metrics,prometheus"
  endpoint:
    health:
      probes:
        enabled: true # Enable liveness and readiness probes
```

### Example 3: Integration Testing with Dedicated Test Profile
```java
package com.example.demo.profile;

import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.test.context.ActiveProfiles;

import static org.assertj.core.api.Assertions.assertThat;

@SpringBootTest
@ActiveProfiles("test") // Activates Document 4 (on-profile: test)
class OrderServiceProfileTest {

    @Value("${app.rate-limit}")
    private int rateLimit;

    @Test
    void shouldLoadTestProfileConfiguration() {
        // Assert rate limit matches test configuration override
        assertThat(rateLimit).isEqualTo(9999);
    }
}
```

### Example 4: The Deprecated `spring.profiles` Anti-Pattern & Remediation
```yaml
# ANTI-PATTERN: Using deprecated spring.profiles key in Boot 3
# BROKEN: Throws InvalidConfigDataPropertyException in Spring Boot 3!
---
spring:
  profiles: dev # DEPRECATED / REMOVED in Boot 3!
server:
  port: 8081

# REMEDIATION: Use spring.config.activate.on-profile
---
spring:
  config:
    activate:
      on-profile: dev # MODERN: Fully supported in Boot 2.4 - 3.4+
server:
  port: 8081
```

### Example 5: Programmatic Inspection of Profile Group Resolution
```java
package com.example.demo.profile;

import org.springframework.boot.CommandLineRunner;
import org.springframework.core.env.Environment;
import org.springframework.stereotype.Component;

import java.util.Arrays;

@Component
public class ActiveProfileLogger implements CommandLineRunner {

    private final Environment environment;

    public ActiveProfileLogger(Environment environment) {
        this.environment = environment;
    }

    @Override
    public void run(String... args) {
        System.out.println("=== Resolved Profile State ===");
        System.out.println("Active Profiles: " + Arrays.toString(environment.getActiveProfiles()));
        System.out.println("Default Profiles: " + Arrays.toString(environment.getDefaultProfiles()));
    }
}
```

---

## 6. Best Practices

1. **Use Multi-Document YAML for Co-Located Settings**: Storing base and environment overrides in a single `application.yaml` eliminates drift and keeps all environment variants visible in a single file.
2. **Use Profile Groups for Cohesive Bundles**: Define composite groups like `local-dev` and `production-cloud` so engineers and deployment scripts activate one canonical name rather than a brittle string of 5 comma-separated profiles.
3. **Never Put Production Passwords in Profile YAML**: Never commit `application-prod.yaml` or a `prod` YAML block with real passwords. Inject passwords via Kubernetes Secrets or OS environment variables.
4. **Always Define a Global Fallback in Document 1**: The first document in `application.yaml` must define sensible default values for all properties so that missing profile keys fall back gracefully.
5. **Standardize on `spring.config.activate.on-profile`**: Avoid obsolete legacy keys (`spring.profiles`, `spring.profiles.include` in sub-documents).
6. **Use `@ActiveProfiles("test")` for Integration Tests**: Guarantee that automated tests never inadvertently connect to development or production databases.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Using `spring.profiles` in Boot 3** | Throws `InvalidConfigDataPropertyException: Property 'spring.profiles' is no longer supported`. | Replace with `spring.config.activate.on-profile`. |
| **Setting active profile in sub-document** | Declaring `spring.profiles.active: prod` inside an `on-profile: dev` block throws an error. | Define `spring.profiles.active` only in Document 1 or via CLI/env. |
| **Assuming lists append across profiles** | Defining a list in a profile block replaces the base list entirely. | Split into separate keys or use maps if appending is required. |
| **Typo in profile expression** | Writing `on-profile: "dev, prod"` uses legacy syntax instead of boolean `dev | prod`. | Use standard boolean operators (`&`, `|`, `!`). |
| **Forgetting `---` document separator** | Concatenating YAML blocks without `---` creates malformed YAML syntax errors. | Ensure three hyphens `---` separate distinct documents. |
| **Committing secrets in `application-prod.yaml`** | Leaks database credentials to version control. | Inject secrets through environment variables at runtime. |

---

## 8. Performance & Security Notes

### Memory Footprint of Multi-Document YAML
All documents in `application.yaml` are parsed during startup by `YamlPropertySourceLoader`. Non-active documents are immediately discarded from memory once profile conditions are evaluated, resulting in zero long-term memory overhead.

### Security Hardening: Profile Verification Gate
Add a startup assertion to prevent launching production with the `dev` profile active:
```java
@Component
public class ProductionSafetyGuard implements ApplicationRunner {
    @Override
    public void run(ApplicationArguments args) {
        if (Arrays.asList(env.getActiveProfiles()).contains("dev") &&
            Arrays.asList(env.getActiveProfiles()).contains("prod")) {
            throw new IllegalStateException("FATAL: Both 'dev' and 'prod' profiles are active!");
        }
    }
}
```

---

## 9. Comparison / When to Use What

| Pattern | Structure | Maintainability | Best For |
| :--- | :--- | :--- | :--- |
| **Multi-Document YAML** | Single file (`application.yaml` with `---`)| **Highest (Zero drift)** | Standard microservices & REST backends |
| **Separate Profile Files** | `application-dev.yaml`, `application-prod.yaml`| Moderate (Multiple files) | Massive monoliths with 500+ lines per profile |
| **Profile Groups** | Composite aliases in `application.yaml` | **High (Reusable bundles)**| Cloud-native teams with modular setups |

---

## 10. Interview Questions

<details>
<summary><strong>Q1: What changed in Spring Boot 2.4 / 3.0 regarding profile configuration?</strong></summary>

**Answer:**  
Spring Boot completely overhauled configuration processing:
1. Replaced the deprecated `spring.profiles` property with **`spring.config.activate.on-profile`**.
2. Standardized multi-document YAML files using triple hyphens (`---`).
3. Introduced **Profile Groups** (`spring.profiles.group.{name}`).
4. Enforced predictable, strict ordering: profile-specific properties now always override default properties regardless of processing order.
</details>

<details>
<summary><strong>Q2: How do you define a multi-document YAML file in Spring Boot?</strong></summary>

**Answer:**  
By separating documents with triple hyphens (`---`). The first document defines global base properties. Subsequent documents use `spring.config.activate.on-profile: {profile}` to activate overrides for specific profiles:
```yaml
server:
  port: 8080
---
spring:
  config:
    activate:
      on-profile: dev
server:
  port: 8081
```
</details>

<details>
<summary><strong>Q3: What are Profile Groups and why are they useful?</strong></summary>

**Answer:**  
Profile Groups allow developers to define composite aliases in `application.yaml` that group multiple related profiles into one logical name (e.g., grouping `database-postgres`, `metrics-datadog`, and `aws-s3` under `production`). Developers and deployment pipelines only need to specify a single profile (`--spring.profiles.active=production`), avoiding long, error-prone profile lists.
</details>

<details>
<summary><strong>Q4: Can you use boolean logic in `spring.config.activate.on-profile`?</strong></summary>

**Answer:**  
Yes. Spring Boot supports boolean expressions using `&` (AND), `|` (OR), `!` (NOT), and parentheses:
```yaml
spring:
  config:
    activate:
      on-profile: "cloud & (staging | prod)"
```
</details>

<details>
<summary><strong>Q5: Can you set `spring.profiles.active` inside a profile-specific YAML document?</strong></summary>

**Answer:**  
**No.** Setting `spring.profiles.active` inside a document that is already gated by `spring.config.activate.on-profile` throws an `InvalidConfigDataPropertyException`. Active profiles can only be set from the base un-profiled document, command-line flags, system properties, or environment variables.
</details>

<details>
<summary><strong>Q6: How does Spring Boot merge properties between the base document and a profile document?</strong></summary>

**Answer:**  
Spring Boot merges properties **hierarchically**:
- Scalar properties (strings, integers, booleans) defined in the profile document override values from the base document.
- Unmentioned properties from the base document are retained as-is.
- **Exception**: List/Array properties in YAML do NOT merge; the profile list completely replaces the base list.
</details>

<details>
<summary><strong>Q7: What happens if an application defines both `application.yaml` and `application-dev.yaml`?</strong></summary>

**Answer:**  
When the `dev` profile is active, both files are loaded into the `Environment`. `application-dev.yaml` is loaded with higher precedence than `application.yaml`, meaning properties in `application-dev.yaml` override matching keys from `application.yaml`.
</details>

<details>
<summary><strong>Q8: How do you activate a profile group in an integration test?</strong></summary>

**Answer:**  
Use the `@ActiveProfiles` annotation on the test class:
```java
@SpringBootTest
@ActiveProfiles("local-dev")
class MyIntegrationTest {}
```
Spring Boot resolves the group and activates all constituent sub-profiles during the test execution.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Create a Multi-Document YAML File (Easy)</strong></summary>

**Task:**  
Convert a project with separate `application.yaml` and `application-dev.yaml` into a single multi-document `application.yaml` using `---` and `spring.config.activate.on-profile: dev`. Verify it boots identically.
</details>

<details>
<summary><strong>Exercise 2: Configure a Profile Group (Easy)</strong></summary>

**Task:**  
Define a profile group `enterprise-prod` in `application.yaml` bundling `prod`, `security`, and `metrics`. Boot with `--spring.profiles.active=enterprise-prod` and verify all three profiles are active in logs.
</details>

<details>
<summary><strong>Exercise 3: Test Boolean Profile Activation (Medium)</strong></summary>

**Task:**  
Create a document with `spring.config.activate.on-profile: "aws & !local"`. Write two tests: one with `activeProfiles = {"aws", "prod"}` (asserting property is present) and one with `activeProfiles = {"aws", "local"}` (asserting property is absent).
</details>

<details>
<summary><strong>Exercise 4: Verify Fail-Fast on Invalid Config (Medium)</strong></summary>

**Task:**  
Intentionally place `spring.profiles.active: prod` inside a profile-specific sub-document. Start the application and observe the `InvalidConfigDataPropertyException` thrown by Spring Boot.
</details>

<details>
<summary><strong>Exercise 5: Production Safety Guard Aspect/Runner (Hard)</strong></summary>

**Task:**  
Implement an `ApplicationRunner` that checks active profiles and throws a fatal exception if `h2-database` is active while `prod` is also active, preventing accidental in-memory database usage in production.
</details>

<details>
<summary><strong>Exercise 6: Dynamic Profile Switching via System Properties (Hard)</strong></summary>

**Task:**  
Write a custom test execution listener that dynamically inspects JVM memory flags and activates either `high-memory` or `low-memory` profile before the `ApplicationContext` refreshes.
</details>

---

## 12. Summary / Cheat Sheet

| Feature | Syntax | Behavior |
| :--- | :--- | :--- |
| **Document Separator** | `---` | Delimits distinct configuration blocks in YAML |
| **Profile Activation** | `spring.config.activate.on-profile: prod` | Activates document only when `prod` is active |
| **Boolean Expression** | `on-profile: "cloud & (staging | prod)"` | Complex composite environment gating |
| **Profile Group** | `spring.profiles.group.prod: "db,cloud"` | Aliases multiple profiles into one bundle |
| **Test Activation** | `@ActiveProfiles("test")` | Activates profile in JUnit 5 `@SpringBootTest` |
| **List Rule** | Lists do NOT merge in YAML | Profile lists overwrite default lists completely |

---

## 13. Further Reading
- [Official Spring Boot Reference: Multi-document Files](https://docs.spring.io/spring-boot/docs/current/reference/html/features.html#features.external-config.files.multi-document)
- [Spring Boot Profile Groups Documentation](https://docs.spring.io/spring-boot/docs/current/reference/html/features.html#features.profiles.groups)
- [Config Data Migration Guide (Boot 2.4+)](https://spring.io/blog/2020/08/14/config-file-processing-in-spring-boot-2-4)
