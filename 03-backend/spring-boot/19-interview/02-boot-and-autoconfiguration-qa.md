# 02: Spring Boot Internals & Auto-Configuration Technical Interview Q&A

| Metadata Field | Value |
|---|---|
| **Topic** | Spring Boot Internals, Auto-Configuration Engine, Conditionals & Starters |
| **Spring Boot Version** | 3.4.x |
| **Java Version** | 21 / 25 LTS |
| **Target Level** | Senior / Lead / Staff Backend Engineer |
| **Key Focus Areas** | `AutoConfiguration.imports`, `@ConditionalOnMissingBean`, Custom Starters, Properties Order |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [Internals & Architecture](#3-internals--architecture)
4. [Syntax & Practical Usage](#4-syntax--practical-usage)
5. [5 Practical Examples](#5-5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Pitfalls & Anti-Patterns](#7-common-pitfalls--anti-patterns)
8. [Performance & Security Considerations](#8-performance--security-considerations)
9. [Comparison / Decision Matrix](#9-comparison--decision-matrix)
10. [Real-World Interview Q&A](#10-real-world-interview-qa)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference Cheatsheet](#12-summary--quick-reference-cheatsheet)
13. [Further Reading & Official Resources](#13-further-reading--official-resources)

---

## 1. What & Why

Before Spring Boot, building enterprise Java applications required configuring hundreds of lines of boilerplate XML or Java config: setting up `DispatcherServlet`, `InternalResourceViewResolver`, `DataSourceTransactionManager`, and connection pools manually. 

Spring Boot revolutionized the ecosystem through **Opinionated Auto-Configuration**: automatically guessing and configuring the beans an application requires based on the JARs detected on the classpath and defined properties.

Senior engineers must know how the auto-configuration engine works under the hood: how Spring Boot 3 replaced `spring.factories` with `META-INF/spring/org.springframework.boot.autoconfigure.AutoConfiguration.imports`, how conditional annotations (`@ConditionalOnClass`, `@ConditionalOnMissingBean`) resolve bean evaluation order, how configuration property binding works, and how to build a production custom starter.

---

## 2. Core Concepts

### 2.1 The Auto-Configuration Discovery Mechanism
In Spring Boot 3.x, `@EnableAutoConfiguration` uses `ImportCandidates.load()` to read classes listed in:
`META-INF/spring/org.springframework.boot.autoconfigure.AutoConfiguration.imports`

Each auto-configuration class is evaluated sequentially based on ordering rules:
- `@AutoConfiguration(before = ..., after = ...)`
- `@AutoConfigureBefore` / `@AutoConfigureAfter`
- `@AutoConfigureOrder`

### 2.2 Conditional Evaluation Engine
Before any bean inside an `@AutoConfiguration` class is instantiated, the container evaluates `@Conditional` expressions:
- `@ConditionalOnClass`: Is a required library (e.g. `HikariDataSource.class`) present on the runtime classpath?
- `@ConditionalOnMissingBean`: Has the developer defined a custom bean of this type? If yes, auto-configuration backs off.
- `@ConditionalOnProperty`: Is a feature flag enabled in `application.yaml`?

---

## 3. Internals & Architecture

```
 ┌────────────────────────────────────────────────────────┐
 │ 1. Main Application starts with @SpringBootApplication │
 └───────────────────────────┬────────────────────────────┘
                             │
                             ▼
 ┌────────────────────────────────────────────────────────┐
 │ 2. Read AutoConfiguration.imports File                 │
 │    (META-INF/spring/...AutoConfiguration.imports)      │
 └───────────────────────────┬────────────────────────────┘
                             │
                             ▼
 ┌────────────────────────────────────────────────────────┐
 │ 3. Evaluate Classpath Conditions                       │
 │    @ConditionalOnClass(DataSource.class)               │
 └─────────────┬────────────────────────────┬─────────────┘
               │ (True)                     │ (False)
               ▼                            ▼
 ┌───────────────────────────┐   ┌────────────────────────┐
 │ 4. Evaluate User Beans    │   │ Discard Auto-Config    │
 │    @ConditionalOnMissing- │   └────────────────────────┘
 │    Bean(DataSource.class) │
 └─────────────┬─────────────┘
               │
         ┌─────┴─────────────────────┐
         ▼ (User did not define)     ▼ (User defined bean)
 ┌───────────────────────────┐  ┌─────────────────────────┐
 │ Instantiate Default Bean  │  │ Back Off: User Bean Wins│
 │ (e.g. HikariDataSource)  │  └─────────────────────────┘
 └───────────────────────────┘
```

---

## 4. Syntax & Practical Usage

### 4.1 `AutoConfiguration.imports` File Location
```
src/main/resources/
└── META-INF/
    └── spring/
        └── org.springframework.boot.autoconfigure.AutoConfiguration.imports
```
Content:
```
com.example.starter.custom.MyCustomAutoConfiguration
```

---

## 5. 5 Practical Examples

### Example 1: Production Custom Starter Auto-Configuration

```java
package com.example.starter.custom;

import org.springframework.boot.autoconfigure.AutoConfiguration;
import org.springframework.boot.autoconfigure.condition.ConditionalOnClass;
import org.springframework.boot.autoconfigure.condition.ConditionalOnMissingBean;
import org.springframework.boot.autoconfigure.condition.ConditionalOnProperty;
import org.springframework.boot.context.properties.EnableConfigurationProperties;
import org.springframework.context.annotation.Bean;

@AutoConfiguration
@ConditionalOnClass(CustomAuditClient.class)
@EnableConfigurationProperties(CustomAuditProperties.class)
@ConditionalOnProperty(prefix = "app.audit", name = "enabled", havingValue = "true", matchIfMissing = true)
public class CustomAuditAutoConfiguration {

    @Bean
    @ConditionalOnMissingBean
    public CustomAuditClient customAuditClient(CustomAuditProperties properties) {
        return new CustomAuditClient(properties.getEndpoint(), properties.getApiKey());
    }
}
```

### Example 2: Type-Safe `@ConfigurationProperties` with Validation

```java
package com.example.starter.custom;

import jakarta.validation.constraints.NotBlank;
import org.springframework.boot.context.properties.ConfigurationProperties;
import org.springframework.validation.annotation.Validated;

@Validated
@ConfigurationProperties(prefix = "app.audit")
public class CustomAuditProperties {

    private boolean enabled = true;

    @NotBlank(message = "Audit endpoint URL must not be blank")
    private String endpoint = "https://audit.internal.corp/events";

    @NotBlank(message = "Audit API key must be provided")
    private String apiKey;

    public boolean isEnabled() { return enabled; }
    public void setEnabled(boolean enabled) { this.enabled = enabled; }
    public String getEndpoint() { return endpoint; }
    public void setEndpoint(String endpoint) { this.endpoint = endpoint; }
    public String getApiKey() { return apiKey; }
    public void setApiKey(String apiKey) { this.apiKey = apiKey; }
}
```

### Example 3: Client Library Class

```java
package com.example.starter.custom;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class CustomAuditClient {

    private static final Logger log = LoggerFactory.getLogger(CustomAuditClient.class);
    private final String endpoint;
    private final String apiKey;

    public CustomAuditClient(String endpoint, String apiKey) {
        this.endpoint = endpoint;
        this.apiKey = apiKey;
    }

    public void logEvent(String eventType, String payload) {
        log.info("Sending audit event [{}] to {}: {}", eventType, endpoint, payload);
    }
}
```

### Example 4: Custom Condition Implementation

```java
package com.example.starter.condition;

import org.springframework.context.annotation.Condition;
import org.springframework.context.annotation.ConditionContext;
import org.springframework.core.type.AnnotatedTypeMetadata;

public class OnProductionEnvCondition implements Condition {

    @Override
    public boolean matches(ConditionContext context, AnnotatedTypeMetadata metadata) {
        String activeProfiles = context.getEnvironment().getProperty("spring.profiles.active", "");
        return activeProfiles.contains("prod") || activeProfiles.contains("production");
    }
}
```

### Example 5: Testing Auto-Configuration with `ApplicationContextRunner`

```java
package com.example.starter;

import com.example.starter.custom.CustomAuditAutoConfiguration;
import com.example.starter.custom.CustomAuditClient;
import org.junit.jupiter.api.Test;
import org.springframework.boot.autoconfigure.AutoConfigurations;
import org.springframework.boot.test.context.runner.ApplicationContextRunner;

import static org.assertj.core.api.Assertions.assertThat;

public class CustomAuditAutoConfigurationTest {

    private final ApplicationContextRunner contextRunner = new ApplicationContextRunner()
            .withConfiguration(AutoConfigurations.of(CustomAuditAutoConfiguration.class));

    @Test
    void shouldRegisterAuditClientByDefault() {
        contextRunner
                .withPropertyValues("app.audit.api-key=test-key-123")
                .run(context -> {
                    assertThat(context).hasSingleBean(CustomAuditClient.class);
                });
    }

    @Test
    void shouldBackOffWhenUserProvidesCustomBean() {
        contextRunner
                .withPropertyValues("app.audit.api-key=test-key-123")
                .withBean("userAuditClient", CustomAuditClient.class, () -> new CustomAuditClient("http://custom", "key"))
                .run(context -> {
                    assertThat(context).hasSingleBean(CustomAuditClient.class);
                    assertThat(context).hasBean("userAuditClient");
                });
    }

    @Test
    void shouldNotLoadWhenDisabledViaProperty() {
        contextRunner
                .withPropertyValues("app.audit.enabled=false")
                .run(context -> {
                    assertThat(context).doesNotHaveBean(CustomAuditClient.class);
                });
    }
}
```

---

## 6. Best Practices

- **Migrate to `AutoConfiguration.imports`**: Spring Boot 3 deprecated `spring.factories` for auto-configuration; always place imports in `META-INF/spring/org.springframework.boot.autoconfigure.AutoConfiguration.imports`.
- **Always Include `@ConditionalOnMissingBean` on Factory Methods**: Allow consumer developers to override default starter beans with their own implementations.
- **Use `ApplicationContextRunner` for Unit Tests**: Never spin up full integration tests (`@SpringBootTest`) to test auto-configuration; `ApplicationContextRunner` evaluates conditions in milliseconds.
- **Enable Configuration Annotation Processor**: Add `spring-boot-configuration-processor` so IDEs provide autocompletion for custom properties.
- **Order Auto-Configurations Explicitly**: Use `@AutoConfiguration(after = DataSourceAutoConfiguration.class)` when dependencies between auto-configurations exist.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Why It Fails | Correct Approach |
|---|---|---|
| **Using `spring.factories` in Boot 3** | Spring Boot 3 ignores `spring.factories` for auto-configuration classes, leading to silent failure. | Use `META-INF/spring/org.springframework.boot.autoconfigure.AutoConfiguration.imports`. |
| **Missing `@ConditionalOnMissingBean`** | Causes `ConflictingBeanDefinitionException` if a user attempts to declare their own bean. | Always annotate auto-configured `@Bean` methods with `@ConditionalOnMissingBean`. |
| **Scanning Auto-Config via `@ComponentScan`** | Auto-configuration classes should never be component-scanned; it causes premature bean loading. | Keep auto-configuration classes in a package outside application component scan roots. |
| **Using `@Value` Instead of Properties Class** | Fragile property resolution, no IDE auto-completion, and missing startup validation. | Use `@ConfigurationProperties` with `@Validated`. |
| **Ignoring Property Precedence Order** | Developers are confused when an environment variable overrides their `application.yaml`. | Understand that CLI arguments and OS environment variables override `application.yaml`. |
| **Circular Auto-Configuration Order** | Defining circular `@AutoConfigureAfter` relationships causes container initialization deadlock. | Enforce clean unidirectional auto-configuration ordering. |

---

## 8. Performance & Security Considerations

### 8.1 Startup Optimization with Condition Evaluation Report
To inspect why an auto-configuration ran (or backed off), start the JVM with `--debug` and inspect the `CONDITIONS EVALUATION REPORT`.

### 8.2 External Configuration Precedence Order (Top 5)
1. Command-line arguments (`--server.port=9090`)
2. Java System properties (`-Dserver.port=9090`)
3. OS environment variables (`SERVER_PORT=9090`)
4. Profile-specific `application-{profile}.yaml`
5. Default `application.yaml`

---

## 9. Comparison / Decision Matrix

| Dimension | Standard `@Configuration` | `@AutoConfiguration` |
|---|---|---|
| **Discovery** | Component scan | `AutoConfiguration.imports` file |
| **Evaluation Timing** | Early (User configuration phase) | Late (After all user beans are registered) |
| **Target Audience** | Application business logic | Reusable shared starters and libraries |
| **Ordering Control** | `@Order` (Limited effect on config) | `@AutoConfigureBefore` / `@AutoConfigureAfter` |

---

## 10. Real-World Interview Q&A

<details>
<summary><strong>1. How does Spring Boot discover auto-configuration classes in Spring Boot 3?</strong></summary>

Spring Boot 3 uses `ImportCandidates.load()` to scan the classpath for `META-INF/spring/org.springframework.boot.autoconfigure.AutoConfiguration.imports`. This text file contains a list of fully qualified auto-configuration class names (replacing the legacy `spring.factories` mechanism).
</details>

<details>
<summary><strong>2. What is the purpose of `@ConditionalOnMissingBean`?</strong></summary>

It tells the auto-configuration engine to register the default bean ONLY IF the developer has not already defined a bean of that type in their own `@Configuration` classes, providing seamless customization and extensibility.
</details>

<details>
<summary><strong>3. How does Spring Boot evaluate condition order between user beans and auto-configured beans?</strong></summary>

User configuration classes registered via `@ComponentScan` are processed first. Auto-configuration classes are processed afterwards in a distinct phase, ensuring user beans take precedence over default auto-configurations.
</details>

<details>
<summary><strong>4. How do you test auto-configuration classes without starting a full web server?</strong></summary>

Use `ApplicationContextRunner`. It creates a lightweight, non-web application context in memory in a few milliseconds and allows asserting which beans were created or backed off using fluent assertions: `assertThat(context).hasSingleBean(...)`.
</details>

<details>
<summary><strong>5. What is the difference between `@AutoConfiguration` and `@Configuration`?</strong></summary>

`@Configuration` classes are loaded during regular component scanning. `@AutoConfiguration` is a specialized annotation for auto-configuration classes imported via `AutoConfiguration.imports` that supports ordering attributes like `before`, `after`, and `beforeName`.
</details>

<details>
<summary><strong>6. What is the property precedence order between OS environment variables and `application.yaml`?</strong></summary>

OS environment variables have higher precedence than `application.yaml`. A setting in `SERVER_PORT` will always override `server.port` specified in `application.yaml`.
</details>

<details>
<summary><strong>7. How do you exclude a specific auto-configuration class?</strong></summary>

In code: `@SpringBootApplication(exclude = {DataSourceAutoConfiguration.class})`.
In properties: `spring.autoconfigure.exclude=org.springframework.boot.autoconfigure.jdbc.DataSourceAutoConfiguration`.
</details>

<details>
<summary><strong>8. How do you debug why an auto-configuration class did not activate?</strong></summary>

Run the application with `--debug` or set `logging.level.org.springframework.boot.autoconfigure=DEBUG`. Inspect the `CONDITIONS EVALUATION REPORT` printed in the startup logs under "Negative matches".
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Create a Custom Auto-Configuration Starter</strong></summary>

**Prompt**: Create an auto-configuration class that registers a `GreeterService` bean only if `app.greeter.enabled=true`.

**Solution**:
Inspect Example 1: use `@AutoConfiguration`, `@ConditionalOnProperty`, and `@ConditionalOnMissingBean`.
</details>

<details>
<summary><strong>Exercise 2: Write an Auto-Configuration Test with `ApplicationContextRunner`</strong></summary>

**Prompt**: Write a unit test asserting that `GreeterService` is absent when property `app.greeter.enabled` is false.

**Solution**:
Inspect Example 5: use `contextRunner.withPropertyValues("app.greeter.enabled=false").run(...)`.
</details>

<details>
<summary><strong>Exercise 3: Register Auto-Configuration in Spring Boot 3 Imports File</strong></summary>

**Prompt**: Create the correct file structure in `src/main/resources` and register `com.example.MyAutoConfig`.

**Solution**:
Create `META-INF/spring/org.springframework.boot.autoconfigure.AutoConfiguration.imports` and write `com.example.MyAutoConfig`.
</details>

<details>
<summary><strong>Exercise 4: Implement Relaxed Binding Property Class</strong></summary>

**Prompt**: Create a property class supporting kebab-case in YAML (`app.api-key`) and camelCase in Java (`apiKey`).

**Solution**:
Inspect Example 2: use `@ConfigurationProperties(prefix = "app")`; Spring automatically maps `api-key` to `apiKey`.
</details>

<details>
<summary><strong>Exercise 5: Exclude DataSourceAutoConfiguration</strong></summary>

**Prompt**: Disable Spring Boot's automatic database detection in `application.yaml`.

**Solution**:
```yaml
spring:
  autoconfigure:
    exclude:
      - org.springframework.boot.autoconfigure.jdbc.DataSourceAutoConfiguration
```
</details>

<details>
<summary><strong>Exercise 6: Inspect Negative Matches in Conditions Report</strong></summary>

**Prompt**: Start a Spring Boot application with `--debug` and locate the reason `RabbitAutoConfiguration` did not activate.

**Solution**:
Inspect console output: `RabbitAutoConfiguration: Did not match: @ConditionalOnClass did not find required class 'com.rabbitmq.client.Channel'`.
</details>

---

## 12. Summary & Quick Reference Cheatsheet

```
┌────────────────────────────────────────────────────────────────────────┐
│               SPRING BOOT AUTO-CONFIGURATION CHEATSHEET                │
├────────────────────────────────────────────────────────────────────────┤
│ 1. Registration: META-INF/spring/...AutoConfiguration.imports          │
│ 2. Annotation: @AutoConfiguration(before=..., after=...)               │
│ 3. Back-Off: @ConditionalOnMissingBean ensures user beans take priority │
│ 4. Testing: ApplicationContextRunner for sub-second condition tests    │
│ 5. Properties: @ConfigurationProperties with @Validated for type-safety│
│ 6. Debugging: Pass --debug to view CONDITIONS EVALUATION REPORT        │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 13. Further Reading & Official Resources

- [Spring Boot Auto-Configuration Reference](https://docs.spring.io/spring-boot/reference/using/auto-configuration.html)
- [Creating Your Own Auto-Configuration Starter](https://docs.spring.io/spring-boot/reference/features/developing-auto-configuration.html)
- [Externalized Configuration Order in Spring Boot](https://docs.spring.io/spring-boot/reference/features/external-config.html)
- [Testing Auto-Configurations with ApplicationContextRunner](https://docs.spring.io/spring-boot/reference/testing/spring-boot-applications.html#testing.spring-boot-applications.auto-configured-tests)
