# Production Configuration and Profiles in Spring Boot

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x |
| **Spring Framework Version** | 6.2.x |
| **Java Version** | 21 LTS |
| **Module** | 14-production-and-deployment |
| **Status** | Complete |

---

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Configuration Reference](#4-syntax--configuration-reference)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices & Production Guidelines](#6-best-practices--production-guidelines)
- [7. Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
- [8. Performance & Security Notes](#8-performance--security-notes)
- [9. Comparison / When to Use What](#9-comparison--when-to-use-what)
- [10. Interview Questions & Answers](#10-interview-questions--answers)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading](#13-further-reading)

---

## 1. What & Why

### Problem Statement
A Spring Boot application configured for local developer convenience is fundamentally hazardous if deployed directly to production:
- In-memory databases (H2), devtools automatic restarts, and detailed stack trace error pages (`server.error.include-stacktrace=always`) expose sensitive database schemas and internal code frames to attackers.
- Passwords, API tokens, and private keys hardcoded in plain configuration files leak into version control repositories.
- Permissive CORS policies, exposed Actuator write operations, and unconstrained thread limits introduce severe security and stability risks.

### Historical Evolution
In early Spring systems, environment-specific configurations required separate XML files or complex Maven profile filters that replaced properties at build time, violating the Twelve-Factor App principle of **strict separation of config from code**.

Spring Boot introduced **Spring Profiles** and layered externalized configuration. 

In **Spring Boot 3.4**, configuration hardening is elevated with:
- Config data trees (`spring.config.import`).
- Type-safe, validated `@ConfigurationProperties` with Jakarta Validation.
- Automated secret masking via `SanitizingFunction`.
- Cloud-native environment variable binding with relaxed property parsing rules.

### Production Relevance
A hardened production configuration guarantees that:
- Devtools, H2 consoles, and debug endpoints are completely deactivated.
- Configuration errors fail fast during application startup before taking traffic.
- Secrets are injected strictly at runtime via environment variables or secret managers without ever touching disk.

---

## 2. Core Concepts

### Foundations of Production Configuration
1. **Spring Profiles (`@Profile`)**: Mechanism to segregate configuration beans and property files by deployment stage (`dev`, `staging`, `prod`).
2. **Configuration Property Precedence**: The 17-level property resolution hierarchy in Spring Boot (command-line arguments $\rightarrow$ `SPRING_APPLICATION_JSON` $\rightarrow$ OS environment variables $\rightarrow$ profile-specific config $\rightarrow$ application.yml).
3. **Fail-Fast Validation**: Using `@Validated` alongside `@ConfigurationProperties` to enforce that required database URLs, API keys, and port numbers satisfy strict constraints on startup.
4. **Relaxed Binding**: Spring Boot's ability to seamlessly map uppercase underscore environment variables (`ORDER_SERVICE_DATABASE_URL`) to kebab-case properties (`order-service.database-url`).
5. **Config Data Import (`spring.config.import`)**: Modern mechanism to import external configuration trees from HashiCorp Vault, AWS Secrets Manager, or Kubernetes ConfigMaps.

---

## 3. How It Works Internally

### Property Resolution Precedence Hierarchy

```
LOWEST PRECEDENCE
  |
  +-- 1. Default properties (SpringApplication.setDefaultProperties)
  +-- 2. @PropertySource annotations on @Configuration classes
  +-- 3. Application properties packaged inside the JAR (application.yml)
  +-- 4. Profile-specific application properties inside the JAR (application-prod.yml)
  +-- 5. Application properties OUTSIDE the packaged JAR (config/application.yml)
  +-- 6. Profile-specific properties OUTSIDE the packaged JAR (config/application-prod.yml)
  +-- 7. OS Environment Variables (e.g. DATABASE_URL, SERVER_PORT)
  +-- 8. Java System Properties (-Dserver.port=8080)
  +-- 9. JNDI attributes (java:comp/env)
  +-- 10. ServletContext / ServletConfig init parameters
  +-- 11. SPRING_APPLICATION_JSON (inline JSON in env or CLI)
  +-- 12. Command Line Arguments (--server.port=8080)
  |
HIGHEST PRECEDENCE (Overrides all above)
```

---

## 4. Syntax & Configuration Reference

### Production Hardened Configuration (`application-prod.yml`)

```yaml
# Production Environment Hardening
server:
  port: 8080
  shutdown: graceful # Graceful request draining
  error:
    include-message: never # Never reveal exception message to clients
    include-binding-errors: never
    include-stacktrace: never # Never leak internal stack traces
    include-exception: false

spring:
  # Disable devtools if accidentally included in runtime classpath
  devtools:
    restart:
      enabled: false
    livereload:
      enabled: false
  # JPA / Hibernate production settings
  jpa:
    open-in-view: false # Eliminate connection leaks
    show-sql: false # Never dump SQL to console in prod
    hibernate:
      ddl-auto: none # Migrations handled strictly via Flyway/Liquibase
    properties:
      hibernate:
        format_sql: false
        jdbc:
          batch_size: 50
  # Data Source (Strictly injected via environment variables)
  datasource:
    url: ${DATABASE_URL}
    username: ${DATABASE_USERNAME}
    password: ${DATABASE_PASSWORD}
    hikari:
      maximum-pool-size: 20
      minimum-idle: 20
      connection-timeout: 3000
      leak-detection-threshold: 4000

# Actuator Hardening
management:
  server:
    port: 9090 # Isolated internal management port
    address: 127.0.0.1
  endpoints:
    web:
      exposure:
        include: [ "health", "info", "prometheus" ]
        exclude: [ "env", "beans", "heapdump" ]
  endpoint:
    health:
      show-details: when-authorized
      roles: "ROLE_OPS_ADMIN"
```

---

## 5. Practical Examples

### Example 1: Fail-Fast Configuration Properties with Jakarta Validation
```java
package com.example.production.config;

import jakarta.validation.constraints.Max;
import jakarta.validation.constraints.Min;
import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.NotNull;
import org.hibernate.validator.constraints.URL;
import org.springframework.boot.context.properties.ConfigurationProperties;
import org.springframework.validation.annotation.Validated;

import java.time.Duration;

@Validated // Triggers fail-fast startup validation
@ConfigurationProperties(prefix = "app.payment-gateway")
public record PaymentGatewayProperties(
    @NotBlank(message = "Gateway API key must not be blank")
    String apiKey,

    @URL(message = "Target endpoint must be a valid URL")
    String endpointUrl,

    @Min(value = 1, message = "Timeout must be at least 1 second")
    @Max(value = 10, message = "Timeout must not exceed 10 seconds")
    int timeoutSeconds,

    @NotNull(message = "Retry backoff duration must be defined")
    Duration retryBackoff
) {}
```

If `APP_PAYMENT_GATEWAY_API_KEY` is missing in the production environment, Spring Boot halts immediately during boot:
```text
***************************
APPLICATION FAILED TO START
***************************
Description:
Binding to target org.springframework.boot.context.properties.bind.BindResult failed:
    Property: app.payment-gateway.apiKey
    Value: "null"
    Reason: Gateway API key must not be blank
Action:
Update your application's configuration
```

### Example 2: Activating Production Profile Securely in Containers
```dockerfile
# Safe production profile activation inside container
FROM eclipse-temurin:21-jre-jammy

WORKDIR /app
COPY target/order-service.jar app.jar

# Enforce production profile via environment variable
ENV SPRING_PROFILES_ACTIVE=prod

EXPOSE 8080 9090
USER 1001:1001

ENTRYPOINT ["java", "-jar", "/app/app.jar"]
```

### Example 3: Dynamic Profile-Specific Bean Registration
```java
package com.example.production.service;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.context.annotation.Profile;

public interface EmailService {
    void sendEmail(String to, String body);
}

@Configuration
public class EmailConfiguration {

    private static final Logger log = LoggerFactory.getLogger(EmailConfiguration.class);

    // Development / Local Mock Email Service
    @Bean
    @Profile("!prod") // Active in all profiles EXCEPT production
    public EmailService mockEmailService() {
        return (to, body) -> log.info("[DEV MOCK] Sending email to {}: {}", to, body);
    }

    // Hardened AWS SES / SendGrid Production Email Service
    @Bean
    @Profile("prod") // Active ONLY in production
    public EmailService productionEmailService() {
        return (to, body) -> {
            log.info("[PROD] Dispatching authenticated transactional email to {}", to);
            // Calls Amazon SES SDK with production credentials
        };
    }
}
```

### Example 4: Anti-Pattern vs. Production Fix (Committing Secrets to Version Control)
```yaml
# ANTI-PATTERN: Hardcoding credentials directly in application.yml
# When pushed to GitHub, tokens and passwords are compromised in commit history!
spring:
  datasource:
    url: jdbc:postgresql://prod-db.internal:5432/orders
    username: postgres
    password: SuperSecretProductionPassword123! # NEVER COMMIT THIS!

# PRODUCTION FIX: Externalize via Environment Variables or Secret Vault
spring:
  datasource:
    url: ${DATABASE_URL}
    username: ${DATABASE_USER}
    password: ${DATABASE_PASSWORD}
```

### Example 5: Custom Sanitization of Proprietary Secrets in Actuator
```java
package com.example.production.config;

import org.springframework.boot.actuate.endpoint.SanitizableData;
import org.springframework.boot.actuate.endpoint.SanitizingFunction;
import org.springframework.stereotype.Component;

import java.util.regex.Pattern;

@Component
public class ProprietarySecretSanitizer implements SanitizingFunction {

    private static final Pattern SENSITIVE_REGEX = 
        Pattern.compile("(?i).*(stripe|jwt|vault|aws|private|encryption|signature).*");

    @Override
    public SanitizableData apply(SanitizableData data) {
        if (data.getKey() != null && SENSITIVE_REGEX.matcher(data.getKey()).matches()) {
            return data.withValue("****** (REDACTED_BY_ENTERPRISE_POLICY)");
        }
        return data;
    }
}
```

---

## 6. Best Practices & Production Guidelines

1. **Always Set `server.error.include-stacktrace: never`**: Never expose Java exception stack traces to external HTTP clients. Stack traces reveal library versions, database table names, and internal file paths to attackers.
2. **Never Check Secrets into Git**: Inject all passwords, API keys, and certificates via environment variables (`SPRING_APPLICATION_JSON`, Kubernetes Secrets, or AWS Secrets Manager).
3. **Use Fail-Fast `@Validated` Configuration**: Enforce validation rules on `@ConfigurationProperties`. If an essential environment variable is missing, fail application boot immediately rather than failing mid-transaction hours later.
4. **Isolate Management Traffic**: In production, configure `management.server.port: 9090` and bind it to internal cluster networks, keeping Actuator strictly unreachable from public internet ingress.
5. **Always Set `hibernate.ddl-auto: none`**: Never allow Hibernate to automatically validate, update, or create database tables on production startup. All database migrations must be executed deterministically via Flyway or Liquibase.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Leaving stack traces enabled in error responses | Attackers inspect stack traces to identify vulnerable library versions and internal network paths. | Set `server.error.include-stacktrace: never`. |
| Committing `application-prod.yml` with plaintext secrets | Secrets leak into Git history, resulting in compromised credentials. | Reference environment variables: `${DATABASE_PASSWORD}`. |
| Using `spring.jpa.hibernate.ddl-auto=update` in prod | Hibernate modifies production tables during boot, locking tables and causing data loss. | Use Flyway/Liquibase; set `ddl-auto: none`. |
| Forgetting `@Validated` on configuration classes | Missing properties evaluate to `null`, causing `NullPointerException`s at runtime under user load. | Annotate `@ConfigurationProperties` with `@Validated` and Jakarta constraints. |
| Exposing all Actuator endpoints via wildcard `"*"` | Public crawlers access `/actuator/env`, exposing internal system variables and architecture. | Explicitly whitelist required endpoints: `include: [ "health", "info", "prometheus" ]`. |
| Hardcoding `spring.profiles.active` inside `application.yml` | Prevents runtime overrides by container orchestrators. | Set active profile via `SPRING_PROFILES_ACTIVE=prod` environment variable. |

---

## 8. Performance & Security Notes

### Startup Verification vs. Runtime Surprises
- Validating configuration properties with `@Validated` adds less than 2 milliseconds to startup time.
- Detecting a malformed URL or missing Stripe API key on boot prevents unrecoverable transaction failures during production operations.

### OWASP Security Configuration Checklist
- Ensure H2 web console is disabled (`spring.h2.console.enabled=false`).
- Ensure Spring Boot Devtools is omitted from production deployment artifacts.
- Verify CORS is restricted to explicit white-listed domain origins.

---

## 9. Comparison / When to Use What

| Environment | Active Profile | `ddl-auto` | Error Stacktrace | Actuator Port |
|---|---|---|---|---|
| **Local Development** | `dev` | `update` or `none` | `always` | Same port (8080) |
| **Integration Testing** | `test` | `create-drop` | `always` | Random port (`0`) |
| **Staging / UAT** | `staging` | `validate` | `never` | Isolated port (9090) |
| **Production** | **`prod`** | **`none` (Flyway/Liquibase)** | **`never`** | **Isolated port (9090)** |

---

## 10. Interview Questions & Answers

### Q1: What is the configuration property precedence order in Spring Boot?
<details>
<summary>Click to view answer</summary>

Spring Boot resolves configuration across 17 distinct levels. Highest priority overrides lowest priority:
1. Command Line Arguments (`--server.port=8080`).
2. `SPRING_APPLICATION_JSON` (embedded JSON string).
3. Java System Properties (`-Dserver.port=8080`).
4. OS Environment Variables (`SERVER_PORT=8080`).
5. Profile-specific application properties outside the JAR (`config/application-prod.yml`).
6. Profile-specific application properties inside the JAR (`application-prod.yml`).
7. Standard `application.yml` outside the JAR.
8. Standard `application.yml` packaged inside the JAR.
</details>

### Q2: How does Relaxed Binding work with Environment Variables in Spring Boot?
<details>
<summary>Click to view answer</summary>

Relaxed binding allows property names in `application.yml` to be mapped flexibly to environment variables. Kebab-case properties (`spring.datasource.max-lifetime`) are automatically matched to uppercase underscore environment variables (`SPRING_DATASOURCE_MAX_LIFETIME`). Dots and hyphens become underscores, and characters are converted to uppercase.
</details>

### Q3: Why should `server.error.include-stacktrace` be set to `never` in production?
<details>
<summary>Click to view answer</summary>

If an unhandled exception occurs (e.g. `SQLException`), returning the stack trace to the HTTP client leaks internal details: database engine and version, table and column names, file system paths, and third-party library versions. Malicious actors use this intelligence to execute targeted exploits. Setting `include-stacktrace: never` returns a standardized RFC 7807 Problem Detail response without sensitive internal traces.
</details>

### Q4: How do you enforce fail-fast validation on `@ConfigurationProperties`?
<details>
<summary>Click to view answer</summary>

Annotate the `@ConfigurationProperties` class with `@Validated` (from `org.springframework.validation.annotation`) and apply standard Jakarta Validation annotations (`@NotNull`, `@NotBlank`, `@Min`, `@Max`, `@URL`) to its fields or record components. If any property violates its constraints during application startup, Spring Boot throws a `BindValidationException` and halts application startup immediately.
</details>

### Q5: What is the role of `spring.config.import` in Spring Boot 3?
<details>
<summary>Click to view answer</summary>

Introduced in Spring Boot 2.4 and refined in Boot 3, `spring.config.import` allows applications to import external configuration trees from arbitrary sources, such as external directories (`file:./config/`), Kubernetes ConfigMaps, or remote secret vaults (`vault://secret/orders`). It supports optional imports using the `optional:` prefix (e.g., `optional:configserver:`).
</details>

### Q6: Why should `spring.jpa.hibernate.ddl-auto` NEVER be set to `update` in production?
<details>
<summary>Click to view answer</summary>

`ddl-auto: update` instructs Hibernate to compare the in-memory entity metamodel against the live database catalog and execute `ALTER TABLE` statements on startup. In production:
1. It acquires exclusive table locks, causing connection pool starvation.
2. It can alter column types or drop constraints unexpectedly.
3. It cannot handle renaming columns or migrating data safely.
All production schema modifications must be version-controlled using migration tools like Flyway or Liquibase.
</details>

### Q7: How does Spring Boot mask sensitive configuration properties in `/actuator/env`?
<details>
<summary>Click to view answer</summary>

Spring Boot Actuator uses a built-in `SanitizingFunction` that inspects property keys. If a key matches default sensitive keywords (such as `password`, `secret`, `key`, `token`, `credential`, `vcap_services`), its value is replaced with `******`. Developers can register custom `SanitizingFunction` beans to sanitize proprietary keys.
</details>

### Q8: What is the purpose of Spring Boot DevTools and why must it be excluded from production?
<details>
<summary>Click to view answer</summary>

Spring Boot DevTools provides live reload, automatic restart on class changes, and relaxed template caching during development. In production, these features consume unnecessary memory, trigger unwanted file system watchers, and introduce security vulnerabilities (e.g. remote debug access). DevTools is configured as `<optional>true</optional>` in Maven to prevent it from leaking into production JARs.
</details>

---

## 11. Practice Exercises

### Exercise 1: Create a Hardened Production application-prod.yml (Easy)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Write an `application-prod.yml` that disables OSIV, disables stack traces in errors, disables SQL logging, and sets Hibernate `ddl-auto` to `none`.

**Solution**:
```yaml
server:
  error:
    include-stacktrace: never
    include-message: never

spring:
  jpa:
    open-in-view: false
    show-sql: false
    hibernate:
      ddl-auto: none
```
</details>

### Exercise 2: Type-Safe Kafka Configuration Properties with Validation (Medium)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Create an immutable `@ConfigurationProperties` record for Kafka with validated bootstrap servers, topic name, and consumer concurrency ($\ge 1$).

**Solution**:
```java
package com.example.production.config;

import jakarta.validation.constraints.Min;
import jakarta.validation.constraints.NotBlank;
import org.springframework.boot.context.properties.ConfigurationProperties;
import org.springframework.validation.annotation.Validated;

@Validated
@ConfigurationProperties(prefix = "app.messaging.kafka")
public record KafkaProductionProperties(
    @NotBlank(message = "Bootstrap servers string is required")
    String bootstrapServers,

    @NotBlank(message = "Target order topic must be configured")
    String orderTopic,

    @Min(value = 1, message = "Consumer concurrency must be at least 1")
    int concurrency
) {}
```
</details>

### Exercise 3: Dynamic Profile-Aware DataSource Router (Hard)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Implement a configuration that registers a Mock embedded H2 DataSource when the active profile is `local`, but requires a PostgreSQL Hikari DataSource with strict validation when active profile is `prod`.

**Solution**:
```java
package com.example.production.config;

import com.zaxxer.hikari.HikariConfig;
import com.zaxxer.hikari.HikariDataSource;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.context.annotation.Profile;
import org.springframework.jdbc.datasource.embedded.EmbeddedDatabaseBuilder;
import org.springframework.jdbc.datasource.embedded.EmbeddedDatabaseType;

import javax.sql.DataSource;

@Configuration
public class ProfileAwareDataSourceConfiguration {

    @Bean
    @Profile("local")
    public DataSource localEmbeddedDataSource() {
        return new EmbeddedDatabaseBuilder()
            .setType(EmbeddedDatabaseType.H2)
            .build();
    }

    @Bean
    @Profile("prod")
    public DataSource productionPostgresDataSource(
        @Value("${DATABASE_URL}") String url,
        @Value("${DATABASE_USER}") String user,
        @Value("${DATABASE_PASSWORD}") String password) {

        HikariConfig config = new HikariConfig();
        config.setJdbcUrl(url);
        config.setUsername(user);
        config.setPassword(password);
        config.setMaximumPoolSize(20);
        config.setMinimumIdle(20);
        config.setPoolName("ProductionHikariPool");

        return new HikariDataSource(config);
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------+-------------------------------------------------------------+
| Setting / Property                          | Production Recommended Rule                                 |
+---------------------------------------------+-------------------------------------------------------------+
| server.error.include-stacktrace: never      | Never leak internal stack traces to HTTP clients            |
| spring.jpa.open-in-view: false              | Prevent connection pool exhaustion during JSON serialization|
| spring.jpa.hibernate.ddl-auto: none         | All database changes managed via Flyway/Liquibase           |
| @ConfigurationProperties + @Validated      | Fail-fast startup if environment variables are missing      |
| Relaxed Binding                             | SPRING_DATASOURCE_URL maps to spring.datasource.url         |
| management.server.port: 9090                | Isolate Actuator traffic from public ingress port           |
| SPRING_PROFILES_ACTIVE=prod                 | Activate production profile strictly via environment        |
+---------------------------------------------+-------------------------------------------------------------+
```

---

## 13. Further Reading
- Spring Boot Official Documentation: Externalized Configuration
- Spring Boot Documentation: Production-ready Features
- The Twelve-Factor App: Configuration Management
- OWASP Top 10: Security Misconfiguration Prevention Cheat Sheet