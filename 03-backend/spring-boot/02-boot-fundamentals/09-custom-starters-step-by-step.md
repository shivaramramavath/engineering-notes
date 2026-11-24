# Developing Custom Spring Boot Starters: Step-by-Step

> The definitive blueprint for authoring production-grade custom Spring Boot starters: the two-module architecture (`autoconfigure` + `starter`), `AutoConfiguration.imports` registration, `@ConditionalOnMissingBean` back-off patterns, configuration metadata generation, and integration testing with `ApplicationContextRunner`.

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [Auto-Configuration Mechanics](file:///03-backend/spring-boot/02-boot-fundamentals/01-auto-configuration-mechanics.md), [Starters Architecture & Dependency Management](file:///03-backend/spring-boot/02-boot-fundamentals/02-starters-architecture-and-dependency-management.md), [Configuration Properties Deep Dive](file:///03-backend/spring-boot/02-boot-fundamentals/04-configuration-properties-deep-dive.md) |
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
In enterprise engineering organizations with dozens of microservices, every team needs standard cross-cutting capabilities: centralized authentication, custom metric emitters, proprietary API gateways, and standardized database auditing. If each team copies and pastes configuration boilerplate into their own services, configurations drift, security vulnerabilities remain unpatched, and upgrading shared libraries becomes a logistical nightmare.

### Historical Evolution
- **Shared Library Jars (pre-Boot)**: Teams created shared JARs with utility classes, but consumers had to write verbose `@Configuration` classes to wire the beans manually.
- **Spring Boot Starters (2014-Present)**: Standardized the "drop-in" paradigm: simply adding a single dependency to `pom.xml` automatically configures all beans, reads configuration properties, and sets up healthchecks.

### Real-World Relevance
Authoring custom starters is the primary mechanism through which Platform Engineering and Architecture teams distribute hardened, certified infrastructure code across enterprise microservice fleets.

---

## 2. Core Concepts

### 1. The Two-Module Architecture
Production-grade starters must strictly follow the official Spring layout:
```text
acme-sms-spring-boot-starter-parent/
├── pom.xml
├── acme-sms-spring-boot-autoconfigure/   <-- Module 1: Code & AutoConfiguration
│   ├── pom.xml
│   └── src/main/java/...
└── acme-sms-spring-boot-starter/        <-- Module 2: Empty Coordination POM
    └── pom.xml
```
- **`autoconfigure` module**: Contains the actual `@AutoConfiguration` class, `@ConfigurationProperties` record, conditions, and business logic.
- **`starter` module**: An empty aggregation POM that pulls in the `autoconfigure` module plus all necessary third-party client JARs.

### 2. Naming Rule
Always name custom starters **`{name}-spring-boot-starter`** (e.g., `acme-sms-spring-boot-starter`). Never use `spring-boot-starter-*` (reserved for official Spring artifacts).

### 3. The Back-Off Pattern (`@ConditionalOnMissingBean`)
Every bean provided by a custom starter must be annotated with `@ConditionalOnMissingBean`. This ensures consumers can override your starter's default implementation simply by defining their own `@Bean`.

---

## 3. How It Works Internally

### Custom Starter Packaging and Consumer Injection

```
+-------------------------------------------------------------------------+
|                  Consumer Project `pom.xml`                             |
|  <dependency>                                                           |
|      <groupId>com.acme.platform</groupId>                              |
|      <artifactId>acme-sms-spring-boot-starter</artifactId>             |
|  </dependency>                                                          |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                  Transitive Dependency Resolution                       |
|  1. Pulls in `acme-sms-spring-boot-autoconfigure.jar`                   |
|  2. Pulls in `twilio-sdk.jar` (third-party client)                      |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                  Spring Boot Bootstrap Phase                            |
|  - `AutoConfigurationImportSelector` scans all classpath JARs          |
|  - Discovers:                                                           |
|    `META-INF/spring/org.springframework.boot.autoconfigure...imports`   |
|    inside `acme-sms-spring-boot-autoconfigure.jar`                      |
|  - Evaluates Conditions:                                                |
|    * `@ConditionalOnClass(TwilioClient.class)` -> Passed                |
|    * `@ConditionalOnProperty(prefix="acme.sms", name="enabled")` -> Pass|
|    * `@ConditionalOnMissingBean(SmsService.class)` -> Passed            |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                  Bean Registered & Injected into Consumer               |
|  - Consumer injects `SmsService` with ZERO configuration code!          |
+-------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### Module 2: Empty Starter Coordination POM
```xml
<!-- acme-sms-spring-boot-starter/pom.xml -->
<project xmlns="http://maven.apache.org/POM/4.0.0">
    <modelVersion>4.0.0</modelVersion>
    <parent>
        <groupId>com.acme.platform</groupId>
        <artifactId>acme-sms-starter-parent</artifactId>
        <version>1.0.0</version>
    </parent>

    <artifactId>acme-sms-spring-boot-starter</artifactId>
    <description>Acme Corporate SMS Starter</description>

    <dependencies>
        <!-- 1. Include the autoconfigure module -->
        <dependency>
            <groupId>com.acme.platform</groupId>
            <artifactId>acme-sms-spring-boot-autoconfigure</artifactId>
            <version>${project.version}</version>
        </dependency>

        <!-- 2. Include required third-party libraries -->
        <dependency>
            <groupId>com.twilio.sdk</groupId>
            <artifactId>twilio</artifactId>
            <version>10.6.0</version>
        </dependency>
    </dependencies>
</project>
```

---

## 5. Practical Examples

### Example 1: Full Implementation of Autoconfigure Module

#### A. The Client Contract and Implementation
```java
// acme-sms-spring-boot-autoconfigure/src/main/java/com/acme/sms/SmsService.java
package com.acme.sms;

public interface SmsService {
    void sendSms(String recipient, String message);
}
```
```java
// acme-sms-spring-boot-autoconfigure/src/main/java/com/acme/sms/TwilioSmsService.java
package com.acme.sms;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class TwilioSmsService implements SmsService {

    private static final Logger log = LoggerFactory.getLogger(TwilioSmsService.class);
    private final String accountSid;
    private final String fromNumber;

    public TwilioSmsService(String accountSid, String fromNumber) {
        this.accountSid = accountSid;
        this.fromNumber = fromNumber;
    }

    @Override
    public void sendSms(String recipient, String message) {
        log.info("[ACME-SMS] Dispatching via Twilio (Account: {}) from {} to {}: {}", 
            accountSid, fromNumber, recipient, message);
    }
}
```

#### B. The Configuration Properties Record
```java
// acme-sms-spring-boot-autoconfigure/src/main/java/com/acme/sms/autoconfigure/AcmeSmsProperties.java
package com.acme.sms.autoconfigure;

import jakarta.validation.constraints.NotBlank;
import org.springframework.boot.context.properties.ConfigurationProperties;
import org.springframework.boot.context.properties.bind.DefaultValue;
import org.springframework.validation.annotation.Validated;

import java.time.Duration;

@ConfigurationProperties(prefix = "acme.sms")
@Validated
public record AcmeSmsProperties(
    @NotBlank(message = "Twilio Account SID is mandatory")
    String accountSid,

    @NotBlank(message = "Twilio Auth Token is mandatory")
    String authToken,

    @NotBlank(message = "Sender phone number is mandatory")
    String fromNumber,

    @DefaultValue("5s")
    Duration timeout
) {}
```

#### C. The AutoConfiguration Class
```java
// acme-sms-spring-boot-autoconfigure/src/main/java/com/acme/sms/autoconfigure/AcmeSmsAutoConfiguration.java
package com.acme.sms.autoconfigure;

import com.acme.sms.SmsService;
import com.acme.sms.TwilioSmsService;
import org.springframework.boot.autoconfigure.AutoConfiguration;
import org.springframework.boot.autoconfigure.condition.ConditionalOnClass;
import org.springframework.boot.autoconfigure.condition.ConditionalOnMissingBean;
import org.springframework.boot.autoconfigure.condition.ConditionalOnProperty;
import org.springframework.boot.context.properties.EnableConfigurationProperties;
import org.springframework.context.annotation.Bean;

@AutoConfiguration
@ConditionalOnClass(SmsService.class)
@ConditionalOnProperty(prefix = "acme.sms", name = "enabled", havingValue = "true", matchIfMissing = true)
@EnableConfigurationProperties(AcmeSmsProperties.class)
public class AcmeSmsAutoConfiguration {

    @Bean
    @ConditionalOnMissingBean // Back-off pattern: yields if user defines their own SmsService bean
    public SmsService smsService(AcmeSmsProperties properties) {
        return new TwilioSmsService(properties.accountSid(), properties.fromNumber());
    }
}
```

#### D. The Imports Registry File
Create file: `acme-sms-spring-boot-autoconfigure/src/main/resources/META-INF/spring/org.springframework.boot.autoconfigure.AutoConfiguration.imports`
```text
com.acme.sms.autoconfigure.AcmeSmsAutoConfiguration
```

### Example 2: Consumer Application Usage
In a separate enterprise microservice:
```yaml
# application.yaml
acme:
  sms:
    enabled: true
    account-sid: "AC1234567890abcdef"
    auth-token: "secret-token-xyz"
    from-number: "+18005550199"
    timeout: 3s
```
```java
package com.example.demo.service;

import com.acme.sms.SmsService;
import org.springframework.stereotype.Service;

@Service
public class OrderNotificationService {

    // Automatically wired from the custom starter!
    private final SmsService smsService;

    public OrderNotificationService(SmsService smsService) {
        this.smsService = smsService;
    }

    public void notifyCustomer(String phone, String orderId) {
        smsService.sendSms(phone, "Your order " + orderId + " has shipped!");
    }
}
```

### Example 3: User Override Demonstration (Back-Off Verification)
If the consumer team wants to mock SMS in local development:
```java
@Configuration
public class LocalMockConfig {

    // User declares custom SmsService bean
    @Bean
    public SmsService customMockSmsService() {
        return (recipient, message) -> System.out.println("MOCK SMS to " + recipient + ": " + message);
    }
}
// RESULT: AcmeSmsAutoConfiguration sees @ConditionalOnMissingBean and silently backs off!
```

### Example 4: Custom Starter Anti-Pattern & Remediation
```java
// ANTI-PATTERN 1: Putting starter code inside the consumer's base package
package com.acme.app; // FLAW: If consumer uses com.acme.app, @ComponentScan eagerly scans this!
@AutoConfiguration
public class BadStarterAutoConfig { ... }

// ANTI-PATTERN 2: Omitting @ConditionalOnMissingBean
@AutoConfiguration
public class RigidAutoConfig {
    @Bean
    public SmsService smsService() { return new TwilioSmsService(); } 
    // FLAW: If user defines custom bean, application CRASHES with NoUniqueBeanDefinitionException!
}

// REMEDIATION:
// 1. Always place auto-configuration in an un-scanned package (e.g., com.acme.sms.autoconfigure)
// 2. Always add @ConditionalOnMissingBean to every provided factory method!
```

### Example 5: Comprehensive Starter Testing with `ApplicationContextRunner`
```java
package com.acme.sms.autoconfigure;

import com.acme.sms.SmsService;
import org.junit.jupiter.api.Test;
import org.springframework.boot.autoconfigure.AutoConfigurations;
import org.springframework.boot.test.context.runner.ApplicationContextRunner;

import static org.assertj.core.api.Assertions.assertThat;

class AcmeSmsAutoConfigurationTest {

    private final ApplicationContextRunner contextRunner = new ApplicationContextRunner()
        .withConfiguration(AutoConfigurations.of(AcmeSmsAutoConfiguration.class));

    @Test
    void shouldRegisterDefaultServiceWhenPropertiesPresent() {
        contextRunner
            .withPropertyValues(
                "acme.sms.account-sid=AC12345",
                "acme.sms.auth-token=token123",
                "acme.sms.from-number=+1555"
            )
            .run(context -> {
                assertThat(context).hasSingleBean(SmsService.class);
            });
    }

    @Test
    void shouldNotLoadWhenDisabledByProperty() {
        contextRunner
            .withPropertyValues("acme.sms.enabled=false")
            .run(context -> {
                assertThat(context).doesNotHaveBean(SmsService.class);
            });
    }

    @Test
    void shouldBackOffWhenUserProvidesCustomBean() {
        contextRunner
            .withUserConfiguration(CustomConfig.class)
            .withPropertyValues("acme.sms.account-sid=AC12345", "acme.sms.auth-token=t", "acme.sms.from-number=+1")
            .run(context -> {
                assertThat(context).hasSingleBean(SmsService.class);
                assertThat(context).getBean(SmsService.class).isSameAs(CustomConfig.CUSTOM_INSTANCE);
            });
    }

    static class CustomConfig {
        static final SmsService CUSTOM_INSTANCE = (r, m) -> {};
        @org.springframework.context.annotation.Bean
        SmsService userSmsService() { return CUSTOM_INSTANCE; }
    }
}
```

---

## 6. Best Practices

1. **Follow the Naming Convention**: Always name starters `{name}-spring-boot-starter`. Never use `spring-boot-starter-*`.
2. **Always Use `@ConditionalOnMissingBean`**: Allow consumers to customize or mock your starter's beans without having to exclude the entire auto-configuration.
3. **Include `spring-boot-configuration-processor` in the Autoconfigure Module**: Ensures that consumers get IDE auto-completion for your custom properties.
4. **Keep the Starter Module Free of Java Code**: The `{name}-spring-boot-starter` module should only contain a `pom.xml` aggregating dependencies.
5. **Set `matchIfMissing = true` on Enable Flags**: If your starter is included in `pom.xml`, the default assumption is that the feature should be active unless explicitly disabled (`enabled=false`).
6. **Test with `ApplicationContextRunner`**: Verify auto-configuration loading, condition failures, and user back-off patterns in fast, isolated unit tests.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Using `spring.factories` in Boot 3** | Spring Boot 3 ignores `EnableAutoConfiguration` declarations in `spring.factories`. | Place class name in `META-INF/spring/...AutoConfiguration.imports`. |
| **Omitting `@ConditionalOnMissingBean`** | If a consumer defines a bean of the same type, startup crashes with `NoUniqueBeanDefinitionException`. | Add `@ConditionalOnMissingBean` to all `@Bean` methods. |
| **Placing starter code in consumer package** | Component scanning eagerly discovers auto-configuration as a standard `@Configuration`, breaking ordering. | Keep starter package names distinct (e.g., `com.acme.starter.autoconfigure`). |
| **Missing `optional` on configuration processor** | Packaging the configuration processor into consumer runtime fat JARs bloats artifact size. | Mark `<optional>true</optional>` in `pom.xml`. |
| **Hardcoding starter version numbers in child modules** | Causes version drift between starter and autoconfigure modules. | Use `${project.version}` in multi-module POMs. |
| **Not validating properties** | If consumer misconfigures properties, errors only occur during live user requests. | Add `@Validated` and JSR-380 constraints on `@ConfigurationProperties`. |

---

## 8. Performance & Security Notes

### Dependency Minimization
Starter modules must be conservative with transitive dependencies. Avoid bundling heavy, unused transitive libraries (e.g., pulling in entire AWS SDK bundles when only S3 client is needed).

### Security Credential Isolation
When auto-configuring third-party SDKs (Twilio, Stripe, AWS), ensure credentials bound from `@ConfigurationProperties` are never logged in plaintext during bootstrap.

---

## 9. Comparison / When to Use What

| Architecture Pattern | Autowiring Effort for Consumer | Customization Flexibility | Best For |
| :--- | :---: | :---: | :--- |
| **Plain Shared Library** | High (Consumer writes `@Bean`) | High | Internal domain models, static utilities |
| **Custom Spring Boot Starter**| **Zero (Drop-in dependency)** | **Exceptional (`@Conditional`)**| Corporate security, tracing, auditing, clients |
| **Copy-Paste Code** | Manual / Brittle | None | **Anti-pattern; prohibited** |

---

## 10. Interview Questions

<details>
<summary><strong>Q1: What are the two modules required for a professional Spring Boot Starter and why?</strong></summary>

**Answer:**  
1. **`{name}-spring-boot-autoconfigure`**: Contains the actual Java code, `@AutoConfiguration` classes, `@ConfigurationProperties`, and conditions.
2. **`{name}-spring-boot-starter`**: An empty coordination POM that aggregates the `autoconfigure` module plus the required third-party client dependencies.
This separation allows consumers to import only the auto-configuration logic if they want to manage client library versions independently.
</details>

<details>
<summary><strong>Q2: Where is a custom auto-configuration class registered in Spring Boot 3?</strong></summary>

**Answer:**  
In the file:
`src/main/resources/META-INF/spring/org.springframework.boot.autoconfigure.AutoConfiguration.imports`  
as a single newline-separated entry containing the fully qualified class name.
</details>

<details>
<summary><strong>Q3: What is the purpose of `@ConditionalOnMissingBean` in a custom starter?</strong></summary>

**Answer:**  
It provides the **back-off pattern**. It guarantees that the starter's auto-configured bean is only registered if the consumer has not already declared their own bean of that type in the `ApplicationContext`. This allows consumers to override default behavior seamlessly without configuration conflicts.
</details>

<details>
<summary><strong>Q4: Why must a custom starter never be named `spring-boot-starter-*`?</strong></summary>

**Answer:**  
The `spring-boot-starter-*` naming prefix is strictly reserved for official Spring Boot starters maintained by the Spring team. Custom and community starters must be named `{project}-spring-boot-starter` to prevent naming collisions and avoid misleading developers.
</details>

<details>
<summary><strong>Q5: How do you provide IDE auto-completion for your custom starter's properties?</strong></summary>

**Answer:**  
Include `spring-boot-configuration-processor` as an `<optional>true</optional>` dependency in the `autoconfigure` module's `pom.xml`. During compilation, it generates `META-INF/spring-configuration-metadata.json`, which IDEs parse to provide auto-completion in `application.yaml`.
</details>

<details>
<summary><strong>Q6: How do you test a custom starter without deploying it to an artifactory?</strong></summary>

**Answer:**  
Use `ApplicationContextRunner` from `spring-boot-test`. It boots an isolated in-memory `ApplicationContext` in under 50ms, allowing you to test condition evaluation, property overrides, and user bean back-off patterns without launching a web server.
</details>

<details>
<summary><strong>Q7: What happens if an auto-configuration class has no conditional annotations?</strong></summary>

**Answer:**  
It will always execute and attempt to register its beans unconditionally whenever the starter is on the classpath, preventing consumers from disabling or overriding its components.
</details>

<details>
<summary><strong>Q8: Can a custom auto-configuration enforce that it runs after an official Spring Boot auto-configuration?</strong></summary>

**Answer:**  
Yes, using the `after` attribute on `@AutoConfiguration`:
```java
@AutoConfiguration(after = DataSourceAutoConfiguration.class)
```
This guarantees that Spring Boot's data source is fully initialized before your custom auto-configuration evaluates its conditions.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Create an Autoconfigure Module (Easy)</strong></summary>

**Task:**  
Create a Maven module `acme-greeting-spring-boot-autoconfigure`. Add a `GreetingService` interface, an implementation, an `@AutoConfiguration` class, and register it in `AutoConfiguration.imports`.
</details>

<details>
<summary><strong>Exercise 2: Create the Starter Aggregation POM (Easy)</strong></summary>

**Task:**  
Create `acme-greeting-spring-boot-starter` with a `pom.xml` depending on `acme-greeting-spring-boot-autoconfigure`. Build both modules with `./mvnw clean install`.
</details>

<details>
<summary><strong>Exercise 3: Test Starter with `ApplicationContextRunner` (Medium)</strong></summary>

**Task:**  
Write a JUnit 5 test in the autoconfigure module using `ApplicationContextRunner`. Assert that `hasSingleBean(GreetingService.class)` is true when property `acme.greeting.enabled=true`.
</details>

<details>
<summary><strong>Exercise 4: Verify Consumer Override (Medium)</strong></summary>

**Task:**  
In the test from Exercise 3, supply a user configuration providing a custom `GreetingService` bean. Assert that the auto-configured bean backs off and only the user bean is present in the context.
</details>

<details>
<summary><strong>Exercise 5: Add JSR-380 Validation and Metadata Processor (Hard)</strong></summary>

**Task:**  
Add `spring-boot-configuration-processor` to `pom.xml`. Annotate the properties record with `@Validated` and `@NotBlank`. Compile and verify `spring-configuration-metadata.json` is generated in `target/classes/META-INF/`.
</details>

<details>
<summary><strong>Exercise 6: Multi-Module Integration Test Application (Hard)</strong></summary>

**Task:**  
Create a standalone Spring Boot test application module that imports `acme-greeting-spring-boot-starter`. Run an integration test verifying that injecting `GreetingService` into a controller works with zero manual `@Configuration` classes.
</details>

---

## 12. Summary / Cheat Sheet

| Step | Artifact / Action | File Location |
| :---: | :--- | :--- |
| **1** | Define Client Logic | `com.acme.{feature}.*` |
| **2** | Create Properties Record | `@ConfigurationProperties(prefix = "acme.{feature}")` |
| **3** | Create AutoConfiguration | `@AutoConfiguration` with `@ConditionalOnMissingBean` |
| **4** | Register in Imports | `META-INF/spring/...AutoConfiguration.imports` |
| **5** | Add Metadata Processor | `spring-boot-configuration-processor` (`optional`) |
| **6** | Create Empty Starter POM | `{name}-spring-boot-starter/pom.xml` |
| **7** | Test with Runner | `new ApplicationContextRunner().withConfiguration(...)` |

---

## 13. Further Reading
- [Official Spring Boot Reference: Creating Your Own Starter](https://docs.spring.io/spring-boot/docs/current/reference/html/features.html#features.developing-auto-configuration.custom-starter)
- [Spring Boot AutoConfiguration.imports Specification](https://github.com/spring-projects/spring-boot/wiki/Spring-Boot-2.7-Release-Notes#new-autoconfiguration-annotation)
- [Spring Boot Testing with ApplicationContextRunner](https://docs.spring.io/spring-boot/docs/current/reference/html/features.html#features.developing-auto-configuration.testing)
