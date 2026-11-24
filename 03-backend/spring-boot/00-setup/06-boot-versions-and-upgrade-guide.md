# Spring Boot Versions & Upgrade Guide: 2.x to 3.x & Beyond

> The authoritative enterprise migration guide covering the architectural leap from Spring Boot 2.x to 3.4+ and 4.x: the Jakarta EE namespace migration, Java 17/21/25 baseline requirements, Security 6/7 Lambda DSL transformation, `AutoConfiguration.imports` transition, and automated code migration with OpenRewrite.

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [Spring Ecosystem Overview](file:///03-backend/spring-boot/00-setup/01-spring-ecosystem-overview.md), [Project Setup with Initializr, Maven & Gradle](file:///03-backend/spring-boot/00-setup/03-project-setup-with-initializr-maven-gradle.md) |
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
Enterprise systems running on legacy Spring Boot 2.x and Java 8 or 11 face compounding operational risks: end-of-life support, unpatched CVEs, incompatibility with modern cloud tooling (OCI container optimizations, GraalVM AOT), and missing modern runtime efficiencies like Java 21 Virtual Threads. However, upgrading from Boot 2.x to 3.x represents the largest generational shift in Java history, requiring package namespace renames, security rewrite, and auto-configuration restructuring.

### Historical Evolution
- **Spring Boot 1.x to 2.x (2018)**: Shifted baseline to Java 8, introduced Spring WebFlux (reactive stack), and modernized Actuator endpoints.
- **Spring Boot 2.x to 3.0 (2022)**: Mandated Java 17+ baseline, migrated from `javax.*` to `jakarta.*` (Jakarta EE 10), introduced Ahead-of-Time (AOT) engine, and upgraded to Spring Framework 6.
- **Spring Boot 3.1 to 3.4 (2023-2024)**: Added first-class Docker Compose support, `@ServiceConnection` for Testcontainers, built-in structured logging (ECS/Logstash/GELF), `RestClient` synchronous client, and deep Virtual Threads integration.
- **Spring Boot 4.0 Horizon**: Preparing for Java 25 LTS baseline, Jakarta EE 11, and native modular deployment architectures.

### Real-World Relevance
Migrating critical enterprise codebases requires systematic, risk-mitigated strategies. Knowing how to leverage tools like OpenRewrite transforms multi-month manual refactoring nightmares into repeatable, automated CI tasks.

---

## 2. Core Concepts

### 1. The Jakarta EE Namespace Migration (`javax.*` -> `jakarta.*`)
Due to Oracle's trademark restrictions following the transfer of Java EE to the Eclipse Foundation, all enterprise APIs were renamed:
- `javax.persistence.*` -> `jakarta.persistence.*` (JPA / Hibernate 6)
- `javax.servlet.*` -> `jakarta.servlet.*` (Tomcat 10+ / Servlet 6.0)
- `javax.validation.*` -> `jakarta.validation.*` (Hibernate Validator 8)
- `javax.annotation.*` -> `jakarta.annotation.*`
Any dependency relying on the legacy `javax.*` namespace will fail to load or throw `ClassNotFoundException` at runtime in Spring Boot 3+.

### 2. Spring Security 6/7 Lambda DSL
Legacy Spring Security relied on method chaining with `.and()`:
```java
// LEGACY (Spring Boot 2.x) - REMOVED
http.authorizeRequests()
    .antMatchers("/admin/**").hasRole("ADMIN")
    .and()
    .formLogin();
```
Spring Boot 3+ completely removed `authorizeRequests()`, `antMatchers()`, and `.and()`, replacing them with type-safe, lambdas-first configuration using `authorizeHttpRequests()` and `Customizer.withDefaults()`.

### 3. Auto-Configuration Registration (`AutoConfiguration.imports`)
In Boot 2.x, auto-configuration classes were registered in `META-INF/spring.factories` under `org.springframework.boot.autoconfigure.EnableAutoConfiguration`. In Boot 3+, they MUST be declared in:
`META-INF/spring/org.springframework.boot.autoconfigure.AutoConfiguration.imports`
as a simple newline-delimited list of class names.

---

## 3. How It Works Internally

### Enterprise Migration Architecture: Boot 2.x to Boot 3.4+

```
+-------------------------------------------------------------------------+
|                  Legacy Spring Boot 2.7 Application                     |
|  - Java 8 / 11 Baseline                                                 |
|  - javax.* namespace (javax.persistence, javax.servlet)                 |
|  - Hibernate 5.6 (Legacy dialect, native queries)                       |
|  - Spring Security 5 (WebSecurityConfigurerAdapter, .and() chaining)    |
|  - META-INF/spring.factories auto-configurations                        |
+------------------------------------+------------------------------------+
                                     |
               AUTOMATED MIGRATION VIA OPENREWRITE RECIPES
                                     |
                                     v
+-------------------------------------------------------------------------+
|                  Modern Spring Boot 3.4 Application                     |
|  - Java 21 / 25 LTS Baseline (Records, Pattern Matching, Loom)          |
|  - jakarta.* namespace (Jakarta EE 10 / 11, Servlet 6.0)                |
|  - Hibernate 6.6+ (Semantic query AST, sqldialects)                     |
|  - Spring Security 6.4+ (SecurityFilterChain Bean, Lambda DSL)         |
|  - META-INF/spring/org.springframework.boot.autoconfigure...imports     |
|  - Native Virtual Threads (`spring.threads.virtual.enabled=true`)      |
|  - Native Structured Logging (ECS, Logstash, GELF)                      |
|  - Synchronous RestClient & Declarative HTTP Interfaces                 |
+-------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### OpenRewrite Maven Plugin Configuration
Add OpenRewrite to your `pom.xml` to automatically upgrade source code, dependencies, and configurations:
```xml
<plugin>
    <groupId>org.openrewrite.maven</groupId>
    <artifactId>rewrite-maven-plugin</artifactId>
    <version>5.45.0</version>
    <configuration>
        <activeRecipes>
            <recipe>org.openrewrite.java.spring.boot3.UpgradeSpringBoot_3_4</recipe>
        </activeRecipes>
    </configuration>
    <dependencies>
        <dependency>
            <groupId>org.openrewrite.recipe</groupId>
            <artifactId>rewrite-spring</artifactId>
            <version>5.22.0</version>
        </dependency>
    </dependencies>
</plugin>
```
Run the automated recipe:
```bash
./mvnw rewrite:dryRun  # Review diff without making changes
./mvnw rewrite:run     # Apply changes automatically to source code
```

---

## 5. Practical Examples

### Example 1: Jakarta EE Namespace Transformation
```java
// BEFORE: Spring Boot 2.x (Legacy javax.*)
package com.example.model;

import javax.persistence.Entity;
import javax.persistence.GeneratedValue;
import javax.persistence.GenerationType;
import javax.persistence.Id;
import javax.persistence.Table;
import javax.validation.constraints.NotBlank;
import javax.validation.constraints.Size;

@Entity
@Table(name = "customers")
public class Customer {

    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @NotBlank
    @Size(min = 2, max = 100)
    private String name;

    // Getters, setters, constructors
}

// AFTER: Spring Boot 3.x / 4.x (Modern jakarta.*)
package com.example.model;

import jakarta.persistence.Entity;
import jakarta.persistence.GeneratedValue;
import jakarta.persistence.GenerationType;
import jakarta.persistence.Id;
import jakarta.persistence.Table;
import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.Size;

@Entity
@Table(name = "customers")
public class Customer {

    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @NotBlank
    @Size(min = 2, max = 100)
    private String name;

    // Getters, setters, constructors
}
```

### Example 2: SecurityFilterChain Lambda DSL Transformation
```java
// BEFORE: Spring Boot 2.x (WebSecurityConfigurerAdapter deprecated & removed)
@Configuration
public class LegacySecurityConfig extends WebSecurityConfigurerAdapter {

    @Override
    protected void configure(HttpSecurity http) throws Exception {
        http
            .csrf().disable()
            .authorizeRequests()
                .antMatchers("/public/**").permitAll()
                .antMatchers("/api/admin/**").hasRole("ADMIN")
                .anyRequest().authenticated()
            .and()
            .httpBasic();
    }
}

// AFTER: Spring Boot 3.x / 4.x (SecurityFilterChain Bean with Lambda DSL)
package com.example.config;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.security.config.Customizer;
import org.springframework.security.config.annotation.web.builders.HttpSecurity;
import org.springframework.security.config.annotation.web.configuration.EnableWebSecurity;
import org.springframework.security.config.annotation.web.configurers.AbstractHttpConfigurer;
import org.springframework.security.web.SecurityFilterChain;

@Configuration
@EnableWebSecurity
public class ModernSecurityConfig {

    @Bean
    public SecurityFilterChain securityFilterChain(HttpSecurity http) throws Exception {
        return http
            .csrf(AbstractHttpConfigurer::disable)
            .authorizeHttpRequests(auth -> auth
                .requestMatchers("/public/**").permitAll()
                .requestMatchers("/api/admin/**").hasRole("ADMIN")
                .anyRequest().authenticated()
            )
            .httpBasic(Customizer.withDefaults())
            .build();
    }
}
```

### Example 3: Migrating `RestTemplate` to Modern `RestClient`
```java
// BEFORE: Spring Boot 2.x / legacy RestTemplate
@Service
public class LegacyPaymentClient {
    private final RestTemplate restTemplate;

    public LegacyPaymentClient(RestTemplateBuilder builder) {
        this.restTemplate = builder.build();
    }

    public PaymentResponse process(PaymentRequest request) {
        return restTemplate.postForObject(
            "https://payment.example.com/api/charges",
            request,
            PaymentResponse.class
        );
    }
}

// AFTER: Spring Boot 3.4+ RestClient (Fluent, Type-Safe, High-Performance)
package com.example.client;

import org.springframework.http.MediaType;
import org.springframework.stereotype.Service;
import org.springframework.web.client.RestClient;

public record PaymentRequest(String accountId, double amount) {}
public record PaymentResponse(String transactionId, String status) {}

@Service
public class ModernPaymentClient {

    private final RestClient restClient;

    public ModernPaymentClient(RestClient.Builder builder) {
        this.restClient = builder
            .baseUrl("https://payment.example.com")
            .build();
    }

    public PaymentResponse process(PaymentRequest request) {
        return restClient.post()
            .uri("/api/charges")
            .contentType(MediaType.APPLICATION_JSON)
            .body(request)
            .retrieve()
            .body(PaymentResponse.class);
    }
}
```

### Example 4: Auto-Configuration Registration Anti-Pattern & Fix
```properties
# ANTI-PATTERN: META-INF/spring.factories in Spring Boot 3.x
# BROKEN: Boot 3 completely ignores EnableAutoConfiguration inside spring.factories!
org.springframework.boot.autoconfigure.EnableAutoConfiguration=\
  com.example.autoconfigure.CustomStorageAutoConfiguration

# REMEDIATION: Create file:
# META-INF/spring/org.springframework.boot.autoconfigure.AutoConfiguration.imports
# Add one fully qualified class per line:
com.example.autoconfigure.CustomStorageAutoConfiguration
```

### Example 5: Upgrading Property Key Deprecations in `application.yaml`
```yaml
# BEFORE (Spring Boot 2.x) - DEPRECATED / REMOVED
spring:
  flyway:
    enabled: true
    baseline-on-migrate: true
  redis:
    host: localhost
    port: 6379

# AFTER (Spring Boot 3.4+)
spring:
  flyway:
    enabled: true
    baseline-on-migrate: true
  data:
    redis: # Note the namespace moved under spring.data.redis
      host: localhost
      port: 6379
```

---

## 6. Best Practices

1. **Stepwise Upgrades (2.7 -> 3.0 -> 3.4)**: Never attempt jumping directly from Spring Boot 2.4 to 3.4. First upgrade to **2.7.x**, resolve all deprecation warnings, then jump to 3.0, and finally bump to 3.4.
2. **Use OpenRewrite Before Manual Edits**: Let automated AST rewrite tools handle 95% of tedious import updates (`javax` to `jakarta`) and property key modernizations.
3. **Upgrade to Java 21 LTS First**: Migrate your build and runtime to Java 21 while still on Spring Boot 2.7 (Boot 2.7 supports Java 17 and 21), ensuring JVM compatibility before tackling framework changes.
4. **Audit Third-Party Starters for Jakarta EE Support**: Verify that external libraries (e.g., Swagger/Springfox, Quartz, Camunda, AWS SDK) offer Spring Boot 3-compatible releases before upgrading. Replace deprecated `springfox` with `springdoc-openapi-starter-webmvc-ui`.
5. **Switch from `RestTemplate` to `RestClient`**: Utilize `RestClient` for new synchronous HTTP integrations, benefiting from clean fluent syntax and first-class MockRestServiceServer support.
6. **Replace `spring.factories` with `AutoConfiguration.imports`**: Ensure all internal platform starters conform to the new auto-configuration imports layout.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Using `springfox-boot-starter`** | Springfox is abandoned and only supports `javax.servlet`; crashes immediately with `ClassNotFoundException` in Boot 3. | Migrate to `org.springdoc:springdoc-openapi-starter-webmvc-ui:2.7.+`. |
| **Keeping `antMatchers()` in Security** | `antMatchers()` and `mvcMatchers()` were eliminated in favor of `requestMatchers()`. | Replace with `requestMatchers("/path/**")`. |
| **Old `spring.factories` configurations** | Auto-configuration classes listed in `META-INF/spring.factories` are silently ignored in Boot 3. | Move class names to `META-INF/spring/org.springframework.boot.autoconfigure.AutoConfiguration.imports`. |
| **Mixing `javax.*` and `jakarta.*` dependencies** | Transitive dependencies importing legacy `javax.persistence` create duplicate, non-assignable metadata classes. | Use `./mvnw dependency:tree` to identify legacy transitive dependencies and add exclusions. |
| **Hibernate 6 `UUID` or `Instant` Mapping Differences** | Hibernate 6 changes default database column types for UUID and timestamp mappings. | Set `spring.jpa.properties.hibernate.type.preferred_instant_jdbc_type=TIMESTAMP` if legacy schema compatibility is needed. |
| **Missing `ConstructorBinding` on `@ConfigurationProperties`** | In Boot 3, `@ConstructorBinding` is no longer required on configuration records with a single constructor. Leaving it on class-level can fail if imported from obsolete packages. | Remove `@ConstructorBinding` when using single-constructor records or classes. |

---

## 8. Performance & Security Notes

### CVE Remediation
Upgrading to Spring Boot 3.4+ permanently resolves critical vulnerabilities present in older generations:
- **Spring4Shell (CVE-2022-22965)**: Completely mitigated by modern class-loader access restrictions and Java 17+ module protections.
- **Path Traversal in WebMvc (CVE-2024-38816)**: Patched in Spring Framework 6.1.14 / 6.2.0+.

### GraalVM Native Image Support
Boot 3+ features built-in Ahead-of-Time (AOT) engine. By replacing dynamic reflection with pre-computed reflection hints generated at build time, native compiled executables boot in under 50 milliseconds with <50MB resident memory.

---

## 9. Comparison / When to Use What

| Migration Approach | Effort | Automation Level | Risk of Errors | Best For |
| :--- | :--- | :--- | :--- | :--- |
| **Manual Refactoring** | Very High | 0% | High | Small single-module services (<5 controllers) |
| **OpenRewrite Recipes** | Low | 90% | Very Low | Enterprise multi-repo microservice fleets |
| **Spring Boot Migrator (SBM)** | Medium | 75% | Low | Monoliths requiring automated architectural guidance |

---

## 10. Interview Questions

<details>
<summary><strong>Q1: What is the primary difference between Spring Boot 2.x and Spring Boot 3.x regarding the Java enterprise namespace?</strong></summary>

**Answer:**  
Spring Boot 3.x migrated entirely from the legacy `javax.*` namespace to `jakarta.*` (Jakarta EE 10 / 11). This affects all core enterprise specifications: Servlet API (`jakarta.servlet`), JPA (`jakarta.persistence`), Bean Validation (`jakarta.validation`), and Web Services. Applications must replace all `javax` imports with their `jakarta` equivalents.
</details>

<details>
<summary><strong>Q2: How has Spring Security configuration changed from Boot 2.x to Boot 3.x?</strong></summary>

**Answer:**  
1. `WebSecurityConfigurerAdapter` was completely removed; security configurations now register a `@Bean` returning `SecurityFilterChain`.
2. Method chaining with `.and()` was deprecated/removed in favor of the Lambda DSL (`http.csrf(...).authorizeHttpRequests(...)`).
3. `antMatchers()` and `mvcMatchers()` were consolidated into `requestMatchers()`.
</details>

<details>
<summary><strong>Q3: Where are auto-configuration classes registered in Spring Boot 3.x?</strong></summary>

**Answer:**  
In Spring Boot 3.x, auto-configurations are registered in:
`META-INF/spring/org.springframework.boot.autoconfigure.AutoConfiguration.imports`  
The legacy `META-INF/spring.factories` file is no longer scanned for `EnableAutoConfiguration`.
</details>

<details>
<summary><strong>Q4: Why can't you jump directly from Spring Boot 2.4 to Spring Boot 3.4?</strong></summary>

**Answer:**  
Jumping across multiple minor and major versions simultaneously compounds breaking changes. The official upgrade path requires upgrading first to the terminal 2.x release (**Spring Boot 2.7.x**), resolving all runtime deprecation warnings, updating property names, and then executing the major 3.0 migration.
</details>

<details>
<summary><strong>Q5: What tool can automate the migration of thousands of Java source files from Boot 2.x to Boot 3.x?</strong></summary>

**Answer:**  
**OpenRewrite** (via `rewrite-maven-plugin` or `rewrite-gradle-plugin`). OpenRewrite applies lossless semantic AST (Abstract Syntax Tree) transformations to update `pom.xml` dependencies, rename imports from `javax.*` to `jakarta.*`, and rewrite Spring Security configurations to the modern Lambda DSL.
</details>

<details>
<summary><strong>Q6: What happened to `RestTemplate` in modern Spring Boot?</strong></summary>

**Answer:**  
`RestTemplate` is in maintenance mode. While still available, Spring Framework 6.1 and Boot 3.2 introduced `RestClient`, a modern, fluent, synchronous HTTP client that provides the same functional style as `WebClient` without requiring reactive dependencies or Netty.
</details>

<details>
<summary><strong>Q7: What is the minimum Java version required to run Spring Boot 3.x and 3.4?</strong></summary>

**Answer:**  
Spring Boot 3.x mandates **Java 17** as the absolute minimum baseline, and fully supports **Java 21 LTS** and **Java 25 LTS**.
</details>

<details>
<summary><strong>Q8: What major change occurred with Hibernate in Spring Boot 3?</strong></summary>

**Answer:**  
Spring Boot 3 upgraded to **Hibernate 6**. Hibernate 6 introduced a brand new Semantic Query Model (SQM) AST parser, revised type mappings (affecting UUIDs and dates), deprecated custom legacy dialects, and mandates Jakarta Persistence 3.1.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Audit Dependencies with Maven (Easy)</strong></summary>

**Task:**  
Run `./mvnw dependency:tree` on an existing project. Search for any lingering references to `javax.servlet` or `javax.persistence`. Identify which transitive starter is introducing them.
</details>

<details>
<summary><strong>Exercise 2: Convert Legacy Security Chaining to Lambda DSL (Easy)</strong></summary>

**Task:**  
Take a legacy security snippet using `.and().csrf().disable().and().authorizeRequests()` and manually rewrite it into a `SecurityFilterChain` bean using the modern Lambda DSL and `requestMatchers()`.
</details>

<details>
<summary><strong>Exercise 3: Run OpenRewrite Dry Run (Medium)</strong></summary>

**Task:**  
Configure `rewrite-maven-plugin` with the `org.openrewrite.java.spring.boot3.UpgradeSpringBoot_3_4` recipe. Execute `./mvnw rewrite:dryRun` and inspect the generated `target/rewrite/rewrite.patch` file.
</details>

<details>
<summary><strong>Exercise 4: Migrate RestTemplate to RestClient (Medium)</strong></summary>

**Task:**  
Refactor an existing service that uses `RestTemplate.exchange()` with `ParameterizedTypeReference` to use `RestClient.builder().build()`, utilizing fluent `.uri()`, `.retrieve()`, and `.body(new ParameterizedTypeReference<...>() {})`.
</details>

<details>
<summary><strong>Exercise 5: Auto-Configuration Migration for Custom Starter (Hard)</strong></summary>

**Task:**  
Create a custom starter with an auto-configuration class. Place it in `META-INF/spring.factories`. Boot a test app and verify the bean is NOT created in Boot 3. Then migrate it to `META-INF/spring/org.springframework.boot.autoconfigure.AutoConfiguration.imports` and verify bean registration.
</details>

<details>
<summary><strong>Exercise 6: Hibernate 6 Custom Dialect Cleanup (Hard)</strong></summary>

**Task:**  
In a legacy project migrating to Boot 3, locate legacy custom Hibernate `Dialect` classes. Remove them, relying instead on Hibernate 6's automatic dialect detection and modern `sqldialect` capabilities.
</details>

---

## 12. Summary / Cheat Sheet

| Concern / Component | Spring Boot 2.x | Spring Boot 3.4+ / 4.x |
| :--- | :--- | :--- |
| **Java Baseline** | Java 8 / 11 | Java 17 / 21 / 25 LTS |
| **Namespace** | `javax.*` | `jakarta.*` |
| **Security Configuration** | `WebSecurityConfigurerAdapter`, `.and()` | `SecurityFilterChain` bean, Lambda DSL |
| **Path Matching** | `antMatchers()`, `mvcMatchers()` | `requestMatchers()` |
| **Synchronous Client** | `RestTemplate` | `RestClient` |
| **Auto-Config Registry** | `META-INF/spring.factories` | `META-INF/spring/...AutoConfiguration.imports` |
| **Swagger / OpenAPI** | `springfox-boot-starter` | `springdoc-openapi-starter-webmvc-ui` |
| **Concurrency Model** | Platform thread pool (200 threads) | Virtual Threads (`spring.threads.virtual.enabled`) |

---

## 13. Further Reading
- [Official Spring Boot 3.0 Migration Guide](https://github.com/spring-projects/spring-boot/wiki/Spring-Boot-3.0-Migration-Guide)
- [Official Spring Boot 3.4 Release Notes](https://github.com/spring-projects/spring-boot/wiki/Spring-Boot-3.4-Release-Notes)
- [OpenRewrite Spring Boot Recipes](https://docs.openrewrite.org/recipes/java/spring/boot3)
- [Jakarta EE Official Specification](https://jakarta.ee/specifications/)
