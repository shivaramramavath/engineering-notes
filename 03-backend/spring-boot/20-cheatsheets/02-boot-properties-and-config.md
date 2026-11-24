# Spring Boot Common Properties and Configuration Cheatsheet

| Metadata | Details |
|---|---|
| **Topic** | Common Properties & YAML Configuration Cheatsheet |
| **Track** | Java Spring Boot |
| **Difficulty** | Intermediate |
| **Prerequisites** | Java 21, Spring Boot Architecture, YAML/Properties |
| **Java Version** | Java 21 / 25 LTS |
| **Spring Boot** | 3.4.x / 4.x |
| **Tested With** | Maven 3.9+, Spring Boot Starter Web |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [Internals & Architecture](#3-internals--architecture)
4. [Syntax & Categorized Configuration Properties](#4-syntax--categorized-configuration-properties)
5. [5 Practical Production Examples](#5-5-practical-production-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes to Avoid](#7-common-mistakes-to-avoid)
8. [Performance & Security Considerations](#8-performance--security-considerations)
9. [Comparison Table: Configuration Formats & Strategies](#9-comparison-table-configuration-formats--strategies)
10. [Technical Interview Questions & Answers](#10-technical-interview-questions--answers)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading & References](#13-further-reading--references)

---

## 1. What & Why
Spring Boot externalizes configuration to allow identical application binaries to run across development, staging, and production environments without recompilation. Configuration properties control embedded servlet containers, connection pooling (HikariCP), logging levels, actuator endpoints, and third-party integrations.

Understanding configuration precedence, binding rules (Relaxed Binding), and canonical YAML keys prevents runtime crashes, misconfigured thread pools, and security breaches.

---

## 2. Core Concepts
- **Relaxed Binding**: Spring Boot binds environment variables (`APP_SERVER_PORT`), system properties (`-Dapp.server.port`), and YAML keys (`app.server-port`) seamlessly to `@ConfigurationProperties`.
- **Property Precedence Order**: Command line arguments override JVM system properties, which override OS environment variables, which override file-based configurations (`application-{profile}.yaml` > `application.yaml`).
- **Profile Activation**: Controlled via `spring.profiles.active` or profile-specific config imports (`spring.config.import`).
- **Type-Safe Binding**: Using Java 21 immutable records annotated with `@ConfigurationProperties`.

---

## 3. Internals & Architecture
How Spring Boot loads and resolves configuration files into the `Environment`:

```
+-------------------------------------------------------------+
|               ConfigFileApplicationListener                 |
|             (ConfigDataEnvironmentPostProcessor)            |
+-------------------------------------------------------------+
                             |
         Loads sources in strict precedence hierarchy
                             |
                             v
+-------------------------------------------------------------+
| 1. Command Line Args (--server.port=8080)                   |
| 2. SPRING_APPLICATION_JSON                                  |
| 3. System Properties (System.getProperties())              |
| 4. OS Environment Variables (APP_SERVER_PORT)               |
| 5. Profile-specific files (application-prod.yaml)           |
| 6. Default application files (application.yaml)             |
+-------------------------------------------------------------+
                             |
                             v
+-------------------------------------------------------------+
|                 PropertySourcesPropertyResolver             |
|    - Evaluates placeholders (${DATABASE_URL:localhost})     |
|    - Type conversion via ConversionService                  |
+-------------------------------------------------------------+
                             |
                             v
+-------------------------------------------------------------+
|             @ConfigurationProperties Binding                |
|    - Validates JSR-380 constraints (@NotNull, @Min)         |
|    - Injects immutable records into Spring Context          |
+-------------------------------------------------------------+
```

---

## 4. Syntax & Categorized Configuration Properties

### 4.1 Server & Embedded Container
```yaml
server:
  port: 8080
  address: 0.0.0.0
  shutdown: graceful
  servlet:
    context-path: /api
  tomcat:
    threads:
      max: 200
      min-spare: 20
    connection-timeout: 20000ms
    max-http-form-post-size: 2MB
```

### 4.2 Datasource & Connection Pool (HikariCP)
```yaml
spring:
  datasource:
    url: jdbc:postgresql://localhost:5432/app_db
    username: ${DB_USER:postgres}
    password: ${DB_PASS:secret}
    driver-class-name: org.postgresql.Driver
    hikari:
      maximum-pool-size: 20
      minimum-idle: 5
      idle-timeout: 300000
      connection-timeout: 20000
      max-lifetime: 1200000
      pool-name: ProductionHikariPool
```

### 4.3 JPA & Hibernate
```yaml
spring:
  jpa:
    open-in-view: false
    hibernate:
      ddl-auto: validate
    properties:
      hibernate:
        dialect: org.hibernate.dialect.PostgreSQLDialect
        format_sql: false
        jdbc:
          batch_size: 50
          order_inserts: true
          order_updates: true
```

### 4.4 Actuator & Observability
```yaml
management:
  endpoints:
    web:
      exposure:
        include: health,info,prometheus,metrics
      base-path: /actuator
  endpoint:
    health:
      show-details: when_authorized
      probes:
        enabled: true
  metrics:
    export:
      prometheus:
        enabled: true
```

---

## 5. 5 Practical Production Examples

### Example 1: Type-Safe Immutable Configuration Record
```java
@ConfigurationProperties(prefix = "app.security.jwt")
@Validated
public record JwtProperties(
    @NotBlank String issuer,
    @NotBlank String secretKey,
    @Positive Duration accessTokenTtl,
    @Positive Duration refreshTokenTtl
) {}
```

### Example 2: Enabling Configuration Properties in Boot 3.4
```java
@SpringBootApplication
@ConfigurationPropertiesScan
public class CoreApplication {
    public static void main(String[] args) {
        SpringApplication.run(CoreApplication.class, args);
    }
}
```

### Example 3: Multi-Document Profile Configuration in YAML
```yaml
spring:
  application:
    name: customer-service
  profiles:
    default: dev

---
spring:
  config:
    activate:
      on-profile: dev
server:
  port: 8080
logging:
  level:
    root: INFO
    org.example: DEBUG

---
spring:
  config:
    activate:
      on-profile: prod
server:
  port: 443
logging:
  level:
    root: WARN
    org.example: INFO
```

### Example 4: Virtual Thread Configuration
```yaml
spring:
  threads:
    virtual:
      enabled: true
```

### Example 5: Graceful Shutdown Configuration
```yaml
server:
  shutdown: graceful
spring:
  lifecycle:
    timeout-per-shutdown-phase: 30s
```

---

## 6. Best Practices
1. **Always Disable `open-in-view`**: Set `spring.jpa.open-in-view=false` to prevent silent database queries during view/controller rendering.
2. **Favor YAML over `.properties`**: YAML provides structural nesting, lists, and multi-document profile separation.
3. **Use Environment Variable Placeholders**: Provide sensible defaults (e.g. `${PORT:8080}`) to allow flexible local execution and container overrides.
4. **Leverage `@ConfigurationProperties` over `@Value`**: Group related properties in validated records for type safety.
5. **Never Store Plaintext Secrets**: Inject credentials via environment variables, Kubernetes Secrets, or HashiCorp Vault.

---

## 7. Common Mistakes to Avoid

| Mistake | Consequence | Remedy |
|---|---|---|
| Leaving `open-in-view: true` | Leaks database connections into web layer, exhausting pool under load | Explicitly configure `spring.jpa.open-in-view: false` |
| Using tabs in `application.yaml` | Fails YAML parsing with `ParserException` | Use strict 2-space indentation |
| Storing DB passwords in Git | Security compromise and leakage | Use environment variable interpolation `${DB_PASSWORD}` |
| Relying on `ddl-auto: update` in production | Risk of silent schema corruption and data loss | Use `validate` or `none` and apply migrations via Flyway/Liquibase |
| Forgetting `spring.threads.virtual.enabled` on Java 21 | Application runs on legacy platform threads, missing scale benefits | Set `spring.threads.virtual.enabled: true` in Spring Boot 3.2+ |
| Missing `@ConfigurationPropertiesScan` | Type-safe configuration classes are not registered as beans | Add `@ConfigurationPropertiesScan` to the main class |

---

## 8. Performance & Security Considerations
- **HikariCP Tuning**: Avoid setting `maximum-pool-size` too high. Typical formula: `(CPU cores * 2) + disk spindles` (often 10–30 connections per pod).
- **Actuator Endpoint Exposure**: Never expose `include: "*"` in production; restrict to `health,info,prometheus`.
- **Sensitive Key Masking**: Configure `management.endpoint.env.show-values: when_authorized` with regex masks to hide API tokens and passwords.

---

## 9. Comparison Table: Configuration Formats & Strategies

| Strategy | Validation | Dynamic Reload | Type Safety | Multi-Document Profiles |
|---|---|---|---|---|
| `application.properties` | No | No | No (String based) | No |
| `application.yaml` | Hierarchical | No | No | Yes (`---` document separator) |
| `@ConfigurationProperties` | Yes (JSR-380) | With Cloud Config | Yes (Strong Java types) | Supported via profile imports |
| `@Value` | Manual only | No | Weak (Type coercion) | No |

---

## 10. Technical Interview Questions & Answers

<details>
<summary>1. What is the priority order between application.yaml and command line arguments?</summary>
Command line arguments (`--server.port=9090`) have higher precedence than configuration files (`application.yaml`), overriding all file-based property values.
</details>

<details>
<summary>2. How does Spring Boot relaxed binding work?</summary>
Relaxed binding allows property names to be written in kebab-case (`app.my-property`), camelCase (`app.myProperty`), snake_case (`app.my_property`), or uppercase environment format (`APP_MYPROPERTY`), all resolving to the same Java field.
</details>

<details>
<summary>3. Why should `spring.jpa.open-in-view` be disabled in REST APIs?</summary>
Open-In-View keeps the database connection and Hibernate session open until the HTTP response is fully written, creating connection starvation under high concurrency.
</details>

<details>
<summary>4. What does `spring.config.import` accomplish in Spring Boot 2.4+?</summary>
It enables modular configuration file imports from relative paths, directories, or external configuration servers (e.g. `spring.config.import: optional:configserver:` or `file:./secrets.yaml`).
</details>

<details>
<summary>5. How do you configure graceful shutdown in Spring Boot?</summary>
Set `server.shutdown=graceful` and configure `spring.lifecycle.timeout-per-shutdown-phase=30s` to allow active in-flight requests to finish before the JVM terminates.
</details>

<details>
<summary>6. How do you bind complex collections like maps or lists in YAML?</summary>
In YAML, lists are defined with hyphens (`- item`) and maps are nested key-values. In `@ConfigurationProperties`, bind them to `List<String>` or `Map<String, ConfigObject>`.
</details>

<details>
<summary>7. What is the effect of `spring.profiles.active` vs `spring.profiles.default`?</summary>
`spring.profiles.default` is active only when no active profile is specified. As soon as `spring.profiles.active` is set, default profiles are disabled unless explicitly included.
</details>

<details>
<summary>8. How do you validate `@ConfigurationProperties` records?</summary>
Add `@Validated` on the record class and standard validation annotations (`@NotBlank`, `@Min`, `@Pattern`) on record components.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary>Exercise 1: Create a validated Redis configuration record</summary>
Define a Java record `RedisProperties` with `host`, `port`, `password`, and `timeout`, validated with `@Positive` and `@NotBlank`.
</details>

<details>
<summary>Exercise 2: Setup profile-based YAML separation</summary>
Create `application-dev.yaml` and `application-prod.yaml` configuring distinct database URLs and logging levels.
</details>

<details>
<summary>Exercise 3: Configure Prometheus metrics export</summary>
Configure `management.endpoints` to expose Prometheus metrics while protecting all other actuator endpoints.
</details>

<details>
<summary>Exercise 4: Implement virtual thread verification</summary>
Enable virtual threads in YAML and print `Thread.currentThread().isVirtual()` from a controller endpoint to verify.
</details>

<details>
<summary>Exercise 5: Externalize configuration with environment variables</summary>
Pass database credentials to a test Spring application using `SPRING_DATASOURCE_URL` and verify resolution.
</details>

<details>
<summary>Exercise 6: Configure HikariCP connection leak detection</summary>
Configure `spring.datasource.hikari.leak-detection-threshold=5000` and write a test that holds a connection for 6 seconds to trigger the warning.
</details>

---

## 12. Summary & Cheat Sheet

```
+---------------------------------------------------------------------------------+
|               SPRING BOOT CONFIGURATION CHEAT SHEET                             |
+---------------------------------------------------------------------------------+
| SERVER       | server.port=8080, server.shutdown=graceful                       |
| VIRTUAL THRD | spring.threads.virtual.enabled=true                              |
| DATASOURCE   | spring.datasource.url=jdbc:postgresql://host/db                 |
| HIKARI       | spring.datasource.hikari.maximum-pool-size=20                    |
| JPA          | spring.jpa.open-in-view=false, ddl-auto=validate                 |
| LOGGING      | logging.level.org.springframework=WARN                           |
| ACTUATOR     | management.endpoints.web.exposure.include=health,prometheus     |
+---------------------------------------------------------------------------------+
```

---

## 13. Further Reading & References
- [Spring Boot Common Application Properties Reference](https://docs.spring.io/spring-boot/docs/current/reference/html/application-properties.html)
- [Externalized Configuration Official Documentation](https://docs.spring.io/spring-boot/docs/current/reference/html/features.html#features.external-config)
- [HikariCP Configuration Best Practices](https://github.com/brettwooldridge/HikariCP#gear-configuration-knobs-baby)
- [Spring Boot Virtual Threads Support](https://docs.spring.io/spring-boot/docs/current/reference/html/features.html#features.spring-application.virtual-threads)
