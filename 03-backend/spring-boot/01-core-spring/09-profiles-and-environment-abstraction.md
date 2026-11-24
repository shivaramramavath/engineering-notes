# Profiles & Environment Abstraction

> A comprehensive architectural guide to Spring's `Environment` abstraction: `PropertySource` hierarchies, profile activation mechanisms, complex boolean profile expressions, configuration precedence, and custom property source integrations.

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [IoC Container & ApplicationContext Internals](file:///03-backend/spring-boot/01-core-spring/01-ioc-container-and-application-context.md), [JavaConfig, @Configuration & @Bean Internals](file:///03-backend/spring-boot/01-core-spring/05-javaconfig-and-at-configuration.md) |
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
Enterprise applications run across radically different deployment environments: local developer workstations, CI/CD test runners, ephemeral staging clusters, and production data centers. In local development, an application should connect to an embedded H2 database and log verbose SQL; in production, it must authenticate against a hardened PostgreSQL cluster with SSL certificates and emit JSON-structured telemetry. Hardcoding configurations or baking environment-specific logic into application code creates critical security vulnerabilities and operational brittleness.

### Historical Evolution
- **Pre-Spring 3.1**: Developers relied on complex Maven build filtering and packaging separate WAR files per environment (`app-dev.war`, `app-prod.war`), violating the fundamental Cloud-Native principle of building once and running anywhere.
- **Spring 3.1**: Introduced the unified `Environment` abstraction and bean `@Profile` annotations, enabling a single immutable binary artifact to adapt to any environment at runtime.
- **Spring 5.1 & Boot 2.4+**: Added boolean profile expressions (`dev & !cloud`) and modernized configuration data imports (`spring.config.import`).

### Real-World Relevance
Spring's `Environment` abstraction provides a unified, consistent API for accessing system properties, environment variables, command-line flags, and configuration files, ensuring 12-Factor App compliance across modern cloud platforms like Kubernetes.

---

## 2. Core Concepts

### 1. The `Environment` Interface
The `Environment` (`org.springframework.core.env.Environment`) represents the environment in which the application is running. It models two primary operational concepts:
1. **Profiles**: Named logical groups of bean definitions that are registered only when the given profile is active (e.g., `dev`, `staging`, `prod`).
2. **Properties**: Key-value pairs sourced from an ordered hierarchy of `PropertySources` (OS env vars, system properties, YAML files).

### 2. The `@Profile` Annotation
Can be placed on `@Component`, `@Configuration`, or individual `@Bean` methods.
- Single profile: `@Profile("dev")`
- Inverted profile: `@Profile("!prod")` (active when `prod` is NOT active)
- Boolean logic expressions: `@Profile("cloud & (staging | prod)")`

### 3. Property Source Precedence
When querying a property (`environment.getProperty("server.port")`), Spring searches an ordered chain of `PropertySource` instances. The first source containing the requested key wins.

---

## 3. How It Works Internally

### Property Resolution Hierarchy (High to Low Precedence)

```
+-------------------------------------------------------------------------+
|                  1. Command-Line Arguments (`--server.port=9090`)       |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                  2. Java System Properties (`-Dserver.port=8081`)       |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                  3. OS Environment Variables (`SERVER_PORT=8082`)       |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                  4. Profile-Specific Config Files (External)            |
|                     (`config/application-prod.yaml`)                    |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                  5. Profile-Specific Config Files (Classpath)           |
|                     (`src/main/resources/application-prod.yaml`)        |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                  6. Application Config Files (Classpath)                |
|                     (`src/main/resources/application.yaml`)             |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                  7. Default Properties / In-Code Defaults               |
+-------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### Activating Profiles
Profiles can be activated via multiple channels:
1. **Command line**: `--spring.profiles.active=dev,debug`
2. **JVM System property**: `-Dspring.profiles.active=prod`
3. **Environment variable**: `SPRING_PROFILES_ACTIVE=staging`
4. **Configuration YAML**:
   ```yaml
   spring:
     profiles:
       active: dev
   ```

### Profile Boolean Expressions
```java
// Active ONLY if 'cloud' profile is active AND 'local' profile is NOT active
@Configuration(proxyBeanMethods = false)
@Profile("cloud & !local")
public class CloudInfrastructureConfiguration {}
```

---

## 5. Practical Examples

### Example 1: Environment-Specific Bean Implementations
```java
package com.example.demo.profile;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.context.annotation.Profile;

public interface MailGateway {
    void send(String to, String subject, String body);
}

// Development implementation: Prints to stdout
class LocalConsoleMailGateway implements MailGateway {
    @Override
    public void send(String to, String subject, String body) {
        System.out.printf("[LOCAL CONSOLE MAIL] To: %s | Subject: %s%n", to, subject);
    }
}

// Production implementation: Connects to external SMTP cluster
class ProductionSmtpMailGateway implements MailGateway {
    @Override
    public void send(String to, String subject, String body) {
        System.out.printf("[PROD SMTP] Dispatched to TLS mail cluster: %s%n", to);
    }
}

@Configuration(proxyBeanMethods = false)
public class MailGatewayConfiguration {

    @Bean
    @Profile("dev | test") // Active in local dev or automated tests
    public MailGateway localMailGateway() {
        return new LocalConsoleMailGateway();
    }

    @Bean
    @Profile("prod") // Active ONLY in production
    public MailGateway productionMailGateway() {
        return new ProductionSmtpMailGateway();
    }
}
```

### Example 2: Complex Boolean Profile Expressions
```java
package com.example.demo.profile;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.context.annotation.Profile;

@Configuration(proxyBeanMethods = false)
public class SecurityAuditConfiguration {

    // Active in AWS or Azure, but NEVER in local development
    @Bean
    @Profile("(aws | azure) & !local")
    public String enterpriseAuditLogger() {
        return "CloudWatch-and-EventHub-Audit-Engine";
    }
}
```

### Example 3: Programmatic Property and Profile Inspection via `Environment`
```java
package com.example.demo.env;

import org.springframework.core.env.Environment;
import org.springframework.stereotype.Service;

import java.util.Arrays;

@Service
public class SystemHealthReporter {

    private final Environment environment;

    public SystemHealthReporter(Environment environment) {
        this.environment = environment;
    }

    public void reportEnvironmentState() {
        String[] activeProfiles = environment.getActiveProfiles();
        String[] defaultProfiles = environment.getDefaultProfiles();
        
        System.out.println("Active Profiles: " + Arrays.toString(activeProfiles));
        System.out.println("Default Profiles: " + Arrays.toString(defaultProfiles));

        // Type-safe property reading with fallback
        int maxConnections = environment.getProperty("app.datasource.max-pool-size", Integer.class, 10);
        boolean debugEnabled = environment.getProperty("app.debug.enabled", Boolean.class, false);

        System.out.printf("Max Pool: %d | Debug: %b%n", maxConnections, debugEnabled);
    }
}
```

### Example 4: Profile Anti-Pattern & Remediation (In-Code Profile Branching)
```java
// ANTI-PATTERN: Hardcoding profile checks inside business methods
@Service
public class BadPaymentService {

    @Autowired
    private Environment environment;

    public void charge(double amount) {
        // FLAW: Violates Open/Closed Principle; code is polluted with environment checks!
        if (Arrays.asList(environment.getActiveProfiles()).contains("dev")) {
            System.out.println("Skipping real charge in dev: $" + amount);
        } else {
            // Real Stripe API call
            System.out.println("Executing production Stripe charge: $" + amount);
        }
    }
}

// REMEDIATION: Decouple via Polymorphic Beans and @Profile
interface PaymentProcessor {
    void charge(double amount);
}

@Component
@Profile("dev")
class MockPaymentProcessor implements PaymentProcessor {
    @Override public void charge(double amount) { System.out.println("Mock charge: $" + amount); }
}

@Component
@Profile("prod")
class StripePaymentProcessor implements PaymentProcessor {
    @Override public void charge(double amount) { System.out.println("Stripe charge: $" + amount); }
}

@Service
public class CleanPaymentService {
    private final PaymentProcessor processor;

    // Clean constructor DI: Class is completely agnostic of active profile!
    public CleanPaymentService(PaymentProcessor processor) {
        this.processor = processor;
    }

    public void charge(double amount) {
        processor.charge(amount);
    }
}
```

### Example 5: Custom PropertySource Registration (In-Memory Vault Simulator)
```java
package com.example.demo.customsource;

import org.springframework.context.ApplicationContextInitializer;
import org.springframework.context.ConfigurableApplicationContext;
import org.springframework.core.env.MapPropertySource;

import java.util.Map;

// Custom initializer that inserts a secure PropertySource into the Environment
public class SecureVaultEnvironmentInitializer 
    implements ApplicationContextInitializer<ConfigurableApplicationContext> {

    @Override
    public void initialize(ConfigurableApplicationContext applicationContext) {
        // Simulate reading credentials from an external secrets vault
        Map<String, Object> vaultSecrets = Map.of(
            "app.security.jwt-secret", "super-secret-vault-key-998877",
            "app.database.password", "vault-pg-password-12345"
        );

        // Add as the HIGHEST priority property source
        var propertySource = new MapPropertySource("customVaultSecrets", vaultSecrets);
        applicationContext.getEnvironment().getPropertySources().addFirst(propertySource);

        System.out.println("SecureVaultPropertySource attached to Environment.");
    }
}
```

---

## 6. Best Practices

1. **Avoid In-Code Profile Branching**: Never write `if (env.acceptsProfiles("prod"))` inside business domain logic. Abstract the difference into separate beans and use `@Profile` on the class definitions.
2. **Default Profile as Fallback**: Understand that if no profile is explicitly activated, Spring activates the `default` profile. Use `@Profile("default")` for local fallbacks.
3. **Use OS Environment Variables for Cloud Secrets**: Follow 12-Factor App guidelines: pass database passwords, API keys, and sensitive tokens via OS environment variables (`APP_SECURITY_JWT_SECRET`) rather than committing them into `application.yaml`.
4. **Leverage Profile Groups in Spring Boot**: In `application.yaml`, group related profiles into cohesive bundles:
   ```yaml
   spring:
     profiles:
       group:
         production:
           - "prod"
           - "metrics"
           - "cloud"
   ```
5. **Always Set `matchIfMissing` in Custom Conditions**: When combining profiles with properties, ensure default states are predictable without relying on implicit fallbacks.
6. **Keep Profiles Coarse-Grained**: Avoid creating micro-profiles (`tenant1-region2-qa3`). Use configuration properties for fine-grained toggles and reserve `@Profile` for distinct operational environments (`local`, `dev`, `prod`).

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Case-sensitivity in profile names** | Setting `SPRING_PROFILES_ACTIVE=Prod` while code declares `@Profile("prod")` fails to match (profiles are case-sensitive). | Always standardize on lowercase kebab-case (`prod`, `dev-local`). |
| **Hardcoding database credentials in profile YAML** | Committing `application-prod.yaml` containing database credentials to git leaks secrets. | Inject secrets via OS environment variables or external vaults. |
| **Assuming `@Profile` applies to all beans in class** | Placing `@Profile("prod")` on a `@Configuration` class gates all `@Bean` methods, but child classes must also be verified. | Apply `@Profile` to individual `@Bean` methods if only part of the config is profile-dependent. |
| **Typo in profile expression** | Writing `@Profile("dev, prod")` uses legacy comma syntax instead of modern boolean syntax (`dev | prod`). | Use valid boolean operators: `&`, `|`, `!`. |
| **Profile-specific YAML overriding default keys completely** | Expecting sub-lists in `application-prod.yaml` to merge with `application.yaml` (lists in YAML replace, not merge). | Be explicit with array/list configurations in profile-specific YAMLs. |
| **Accidental activation of test profiles in production** | Packing `application-test.yaml` into production JARs can cause test mocks to activate if profiles are misspelled. | Ensure test YAMLs reside exclusively in `src/test/resources`. |

---

## 8. Performance & Security Notes

### Preventing Information Disclosure in Actuator
Actuator's `/actuator/env` endpoint exposes all active profiles and properties:
- By default, Spring Boot sanitizes keys containing `password`, `secret`, `key`, and `token` with `******`.
- Ensure `/actuator/env` is restricted to authorized operations administrators via Spring Security.

### Startup Performance
Evaluating profiles occurs during the bean definition phase before instantiation. Having dozens of profile-specific configurations has near-zero runtime performance cost; only the matching bean definitions are retained in memory.

---

## 9. Comparison / When to Use What

| Feature / Mechanism | Scope | Primary Use Case | Best For |
| :--- | :--- | :--- | :--- |
| **`@Profile`** | High-level environment | Coarse-grained environment isolation | `dev`, `test`, `prod` bean selection |
| **`@ConditionalOnProperty`** | Fine-grained feature toggle | Specific feature enablement via key | Enabling/disabling specific plugins or caches |
| **OS Environment Variables** | Runtime deployment | Passing secrets and infrastructure URIs | Kubernetes ConfigMaps and Secrets |
| **`application.yaml`** | Application defaults | Standard application configuration | Default timeouts, thread pool sizes |

---

## 10. Interview Questions

<details>
<summary><strong>Q1: What is the Spring `Environment` abstraction and what two primary concepts does it manage?</strong></summary>

**Answer:**  
The `Environment` is an architectural interface (`org.springframework.core.env.Environment`) representing the runtime context of the application. It manages:
1. **Profiles**: Logical groups of bean definitions that are conditionally activated based on the environment (e.g., `dev`, `prod`).
2. **Properties**: Unified key-value access across an ordered hierarchy of `PropertySources` (system properties, environment variables, YAML/properties files).
</details>

<details>
<summary><strong>Q2: What is the property source evaluation order in Spring Boot?</strong></summary>

**Answer:**  
From highest to lowest precedence:
1. Command-line arguments (`--server.port=9090`)
2. Java System properties (`-Dserver.port=8081`)
3. OS environment variables (`SERVER_PORT=8082`)
4. Profile-specific application properties outside the JAR (`config/application-{profile}.yaml`)
5. Profile-specific application properties packaged inside the JAR (`application-{profile}.yaml`)
6. Standard application properties outside the JAR (`config/application.yaml`)
7. Standard application properties inside the JAR (`application.yaml`)
8. `@PropertySource` annotations on `@Configuration` classes
9. Default properties via `SpringApplication.setDefaultProperties()`
</details>

<details>
<summary><strong>Q3: How do boolean profile expressions work in `@Profile`?</strong></summary>

**Answer:**  
Since Spring 5.1, `@Profile` supports boolean operators:
- `&` (AND): `@Profile("cloud & aws")`
- `|` (OR): `@Profile("dev | test")`
- `!` (NOT): `@Profile("!prod")`
- Parentheses: `@Profile("(aws | gcp) & !local")`
</details>

<details>
<summary><strong>Q4: What profile is activated if no active profile is explicitly set?</strong></summary>

**Answer:**  
Spring activates the **`default`** profile. You can configure beans to run specifically in this fallback state using `@Profile("default")`. The default profile name can be customized via `spring.profiles.default`.
</details>

<details>
<summary><strong>Q5: Why is checking `environment.acceptsProfiles(...)` inside service methods considered bad practice?</strong></summary>

**Answer:**  
It couples business domain logic directly to deployment environment names, violates the Single Responsibility and Open/Closed Principles, makes testing difficult, and litters the codebase with procedural `if/else` checks. The clean approach is to use polymorphic interfaces with separate beans annotated with `@Profile`.
</details>

<details>
<summary><strong>Q6: How can you dynamically register a custom `PropertySource` before bean definitions are processed?</strong></summary>

**Answer:**  
Implement an `ApplicationContextInitializer<ConfigurableApplicationContext>`. Inside `initialize()`, access `applicationContext.getEnvironment().getPropertySources()` and call `.addFirst(customPropertySource)` or `.addLast(...)`. Register the initializer via `spring.factories` or `SpringApplication.addInitializers()`.
</details>

<details>
<summary><strong>Q7: What is Spring Boot Profile Grouping?</strong></summary>

**Answer:**  
Profile Grouping allows defining composite profile aliases in `application.yaml`. For example, defining:
```yaml
spring.profiles.group.prod: "monitoring,security,cloud"
```
means activating `--spring.profiles.active=prod` automatically activates all three sub-profiles (`monitoring`, `security`, and `cloud`).
</details>

<details>
<summary><strong>Q8: How does Spring Boot map uppercase environment variables to camelCase or dot-notation properties (Relaxed Binding)?</strong></summary>

**Answer:**  
Spring Boot uses **Relaxed Binding**:
- Dots (`.`) are replaced with underscores (`_`).
- Hyphens (`-`) are omitted or converted to underscores.
- Casing is converted to uppercase.  
For example, the property `spring.datasource.max-active` can be overridden by the environment variable `SPRING_DATASOURCE_MAXACTIVE` or `SPRING_DATASOURCE_MAX_ACTIVE`.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Configure Dual Mail Gateways via Profiles (Easy)</strong></summary>

**Task:**  
Create a `MailSender` interface. Implement `DevMailSender` (`@Profile("dev")`) and `ProdMailSender` (`@Profile("prod")`). Run the application with `--spring.profiles.active=dev` and assert that `DevMailSender` is injected.
</details>

<details>
<summary><strong>Exercise 2: Test Active Profile Fallback (Easy)</strong></summary>

**Task:**  
Create a bean with `@Profile("default")`. Boot the application without setting any active profiles. Verify via Actuator `/actuator/beans` that the bean is loaded. Then set `--spring.profiles.active=staging` and verify it is NOT loaded.
</details>

<details>
<summary><strong>Exercise 3: Complex Boolean Expression Testing (Medium)</strong></summary>

**Task:**  
Create a bean annotated with `@Profile("aws & !dev")`. Write three `@SpringBootTest` test classes activating: (1) `aws,prod`, (2) `aws,dev`, and (3) `azure,prod`. Verify the bean is present ONLY in scenario 1.
</details>

<details>
<summary><strong>Exercise 4: Verify Property Precedence (Medium)</strong></summary>

**Task:**  
Define `app.timeout=10` in `application.yaml`. Run the application with `-Dapp.timeout=20` and `--app.timeout=30`. Print the resolved property value to prove that the command-line argument (30) overrides the system property (20) and YAML (10).
</details>

<details>
<summary><strong>Exercise 5: Build a Secrets Vault `PropertySource` (Hard)</strong></summary>

**Task:**  
Write an `ApplicationContextInitializer` that fetches simulated encrypted credentials from a remote map, decrypts them, and injects them as a `MapPropertySource` at index 0 of the `PropertySources` list before any Spring beans are created.
</details>

<details>
<summary><strong>Exercise 6: ArchUnit Rule Banning In-Code Profile Checks (Hard)</strong></summary>

**Task:**  
Write an ArchUnit test ensuring that no class in `com.example.service` or `com.example.controller` invokes `Environment.getActiveProfiles()` or `Environment.acceptsProfiles()`, enforcing pure polymorphic profile design.
</details>

---

## 12. Summary / Cheat Sheet

| Mechanism / Command | Syntax Example | Behavior / Precedence |
| :--- | :--- | :--- |
| **CLI Argument** | `--spring.profiles.active=prod` | Highest priority profile activation |
| **System Property** | `-Dspring.profiles.active=prod` | Overrides environment variables |
| **Environment Variable**| `SPRING_PROFILES_ACTIVE=prod` | 12-factor cloud platform standard |
| **Boolean Logic** | `@Profile("(aws | gcp) & !local")` | Rich composite profile gating |
| **Default Profile** | `@Profile("default")` | Active only when zero profiles set |
| **Relaxed Binding** | `SERVER_PORT` -> `server.port` | Automatic case/delimiter mapping |
| **Profile Groups** | `spring.profiles.group.prod: "db,cloud"`| Aliases multiple profiles under one tag |

---

## 13. Further Reading
- [Official Spring Framework Reference: Environment Abstraction](https://docs.spring.io/spring-framework/reference/core/beans/environment.html)
- [Spring Boot Externalized Configuration Reference](https://docs.spring.io/spring-boot/docs/current/reference/html/features.html#features.external-config)
- [The Twelve-Factor App: Config](https://12factor.net/config)
