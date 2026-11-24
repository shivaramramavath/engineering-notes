# Externalized Configuration & Property Hierarchy

> Master Spring Boot's externalized configuration subsystem: the 17-level property priority order, properties vs. YAML mechanics, relaxed binding rules, duration and data-size unit conversions, and secure secret injection.

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [Profiles & Environment Abstraction](file:///03-backend/spring-boot/01-core-spring/09-profiles-and-environment-abstraction.md) |
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
Hardcoding configuration parameters (database URLs, thread pool sizes, API keys, cache timeouts) inside Java source code is a critical design flaw. Changing a timeout requires re-compiling bytecode, running test suites, and rebuilding container images. Furthermore, committing credentials to version control creates immediate security vulnerabilities.

### Historical Evolution
- **Spring 1.x - 2.x**: Relied on `PropertyPlaceholderConfigurer` reading standalone `.properties` files from the classpath.
- **Spring Boot 1.x - 2.x**: Introduced hierarchical externalized configuration, YAML support via SnakeYAML, and Relaxed Binding.
- **Spring Boot 3.x - 3.4+**: Added native config tree support (Kubernetes Volume mounts), config data imports (`spring.config.import`), and type-safe duration (`10s`, `500ms`) and memory size (`10MB`) parsing.

### Real-World Relevance
Adhering to the **12-Factor App (III. Config)** methodology requires strict separation of configuration from code. Spring Boot externalizes configuration across an ordered 17-level precedence hierarchy, allowing the same immutable fat JAR to seamlessly run in development, staging, and production environments.

---

## 2. Core Concepts

### 1. The 17-Level Precedence Hierarchy
When Spring Boot resolves a property, it checks sources in strict order (highest priority overrides lowest):
1. DevTools global settings (`~/.config/spring-boot/spring-boot-devtools.properties`)
2. `@TestPropertySource` annotations on test classes
3. `@SpringBootTest#properties` annotation attributes
4. **Command-line arguments** (e.g., `--server.port=9090`)
5. Properties from `SPRING_APPLICATION_JSON` (embedded JSON in env var or system property)
6. `ServletConfig` init parameters
7. `ServletContext` init parameters
8. JNDI attributes from `java:comp/env`
9. **Java System properties** (e.g., `-Dserver.port=8081`)
10. **OS Environment variables** (e.g., `SERVER_PORT=8082`)
11. `RandomValuePropertySource` (`${random.int}`, `${random.uuid}`)
12. Profile-specific application properties outside the packaged JAR (`config/application-{profile}.yaml`)
13. Profile-specific application properties packaged inside the JAR (`application-{profile}.yaml`)
14. Application properties outside the packaged JAR (`config/application.yaml`)
15. Application properties packaged inside the JAR (`application.yaml`)
16. `@PropertySource` annotations on `@Configuration` classes
17. Default properties (`SpringApplication.setDefaultProperties`)

### 2. Properties vs. YAML
- **`application.properties`**: Flat key-value format. Supported by standard Java; allows `@PropertySource` loading.
- **`application.yaml`**: Hierarchical, structured format. Highly readable for nested configurations. Note: `@PropertySource` does *not* natively load YAML files without a custom factory.

### 3. Relaxed Binding Rules
Spring Boot matches configuration keys flexibly across four common styles:
- **Kebab-case (Standard)**: `app.my-service.retry-count`
- **CamelCase**: `app.myService.retryCount`
- **Underscore**: `app.my_service.retry_count`
- **Uppercase (Env vars)**: `APP_MYSERVICE_RETRYCOUNT` or `APP_MY_SERVICE_RETRY_COUNT`

### 4. Built-in Type Conversion: Units
Spring Boot converts string units automatically:
- **Durations**: `10ms`, `5s`, `2m`, `1h`, `1d` -> `java.time.Duration`
- **Data Sizes**: `500B`, `10KB`, `20MB`, `2GB` -> `org.springframework.util.unit.DataSize`

---

## 3. How It Works Internally

### Property Loading & Relaxed Binding Engine

```
Source 1: CLI (`--app.timeout=10s`)
Source 2: Env (`APP_TIMEOUT=5s`)
Source 3: YAML (`app.timeout: 2s`)
                   |
                   v
+-------------------------------------------------------------------------+
|                  PropertySourcesPropertyResolver                        |
|  - Queries `PropertySources` list in descending priority order          |
|  - Match found at Source 1 (CLI): "10s"                                 |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                  Type Conversion & Unit Parsing                         |
|  - Target field: `Duration timeout`                                     |
|  - `StringToDurationConverter` parses "10s"                             |
|  - Result: `Duration.ofSeconds(10)`                                     |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                  Injected into Bean Field / Constructor                 |
+-------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### Comprehensive `application.yaml`
```yaml
server:
  port: 8080
  shutdown: graceful

app:
  name: "order-service"
  timeout: 5s              # Automatically parses to Duration
  max-payload-size: 15MB   # Automatically parses to DataSize
  features:
    ai-enabled: true
    regions:
      - "us-east-1"
      - "eu-central-1"
```

### Injecting via `@Value` with Fallbacks
```java
@Service
public class GatewayClient {

    @Value("${app.timeout:3s}") // Defaults to 3s if app.timeout is unset
    private Duration timeout;

    @Value("${app.max-payload-size:10MB}")
    private DataSize maxPayload;

    @Value("#{systemProperties['user.timezone'] ?: 'UTC'}")
    private String timezone;
}
```

---

## 5. Practical Examples

### Example 1: Full-Featured Type-Safe Property Injection
```java
package com.example.demo.config;

import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Service;
import org.springframework.util.unit.DataSize;

import java.time.Duration;

@Service
public class FileUploadManager {

    private final String serviceName;
    private final Duration uploadTimeout;
    private final DataSize maxFileSize;

    // Idiomatic constructor injection with @Value
    public FileUploadManager(
        @Value("${app.name:file-service}") String serviceName,
        @Value("${app.upload.timeout:30s}") Duration uploadTimeout,
        @Value("${app.upload.max-size:50MB}") DataSize maxFileSize
    ) {
        this.serviceName = serviceName;
        this.uploadTimeout = uploadTimeout;
        this.maxFileSize = maxFileSize;
    }

    public void printConfig() {
        System.out.printf("Service: %s | Timeout: %d ms | Max Bytes: %d%n",
            serviceName, uploadTimeout.toMillis(), maxFileSize.toBytes());
    }
}
```

### Example 2: Relaxed Binding Demonstration across Operating Systems
```java
package com.example.demo.binding;

import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Component;

@Component
public class DatabaseConnectionProps {

    // Can be satisfied by:
    // 1. application.yaml:   app.database.connection-pool-size: 20
    // 2. application.yaml:   app.database.connectionPoolSize: 20
    // 3. Environment var:    APP_DATABASE_CONNECTIONPOOLSIZE=20
    // 4. Environment var:    APP_DATABASE_CONNECTION_POOL_SIZE=20
    // 5. Command-line flag:  --app.database.connection-pool-size=20
    @Value("${app.database.connection-pool-size:10}")
    private int poolSize;

    public int getPoolSize() { return poolSize; }
}
```

### Example 3: Random Value Generator for Ephemeral Ports and Tokens
```yaml
# application.yaml
app:
  test:
    session-token: "${random.uuid}"
    ephemeral-port: "${random.int[9000,9999]}"
    secret-bytes: "${random.value}"
```
```java
@Component
public class EphemeralSessionConfig {
    @Value("${app.test.session-token}")
    private String token;

    @Value("${app.test.ephemeral-port}")
    private int port;
}
```

### Example 4: YAML List Override Anti-Pattern & Remediation
```yaml
# application.yaml (Default base configuration)
app:
  allowed-ips:
    - "10.0.0.1"
    - "10.0.0.2"

# application-prod.yaml (Profile-specific configuration)
app:
  allowed-ips:
    - "192.168.1.100"

# FLAW: Developers expect allowed-ips to be [10.0.0.1, 10.0.0.2, 192.168.1.100]!
# TRUTH: In YAML, lists DO NOT MERGE! The list in application-prod.yaml
# COMPLETELY OVERWRITES the base list, leaving only [192.168.1.100]!

# REMEDIATION: If merging is required, use distinct keys or a Map/Set
app:
  common-ips:
    - "10.0.0.1"
    - "10.0.0.2"
  prod-ips:
    - "192.168.1.100"
```

### Example 5: Overriding Configuration via `SPRING_APPLICATION_JSON`
In containerized Kubernetes deployments, passing complex JSON strings into a single environment variable overrides nested properties effortlessly:
```bash
# Docker / Kubernetes deployment command
docker run -e SPRING_APPLICATION_JSON='{"server":{"port":9095},"app":{"timeout":"12s"}}' my-app:latest
```
Spring Boot parses the JSON string and registers all nested keys at Priority Level 5, cleanly overriding `application.yaml` defaults.

---

## 6. Best Practices

1. **Standardize on Kebab-Case in YAML/Properties**: Always write keys as `app.database.connection-timeout` in configuration files. Spring Boot's relaxed binding translates this to camelCase Java records automatically.
2. **Never Commit Plaintext Secrets**: Inject database passwords, JWT private keys, and external API credentials via OS environment variables (`DATABASE_PASSWORD`) or external vaults.
3. **Always Provide Sensible Defaults in `@Value`**: Write `@Value("${app.cache.ttl:60s}")` so applications boot gracefully without requiring dummy properties during unit tests.
4. **Use Explicit Units for Durations and Data Sizes**: Write `timeout: 10s` and `max-size: 50MB` instead of raw integers like `timeout: 10000`, eliminating unit ambiguity (milliseconds vs seconds).
5. **Remember That YAML Lists Do Not Merge**: When defining arrays or lists across profiles, understand that profile-specific lists completely overwrite default lists.
6. **Prefer `@ConfigurationProperties` Over Multiple `@Value` Annotations**: If injecting more than three related properties, bundle them into a type-safe `@ConfigurationProperties` record.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Assuming YAML lists merge across profiles** | YAML lists overwrite completely rather than appending elements. | Restructure into maps or distinct property keys. |
| **Using `@PropertySource` on YAML files** | Spring's default `@PropertySource` does not support YAML; throws `IllegalStateException`. | Use `application.yaml` or implement a custom `YamlPropertySourceFactory`. |
| **Missing default value in `@Value`** | If property is absent, application crashes on boot with `IllegalArgumentException: Could not resolve placeholder`. | Provide default fallback: `@Value("${key:defaultValue}")`. |
| **Case mismatches in environment variables** | Typing `App_Timeout` instead of `APP_TIMEOUT` fails relaxed binding on some Linux distributions. | Use standard ALL_CAPS with underscores: `APP_TIMEOUT`. |
| **Unit ambiguity** | Declaring `cache.ttl: 60` leaving developers guessing whether it means 60 seconds, 60 minutes, or 60 ms. | Use unit suffixes: `60s`, `60m`, `60ms`. |
| **Colon syntax errors in YAML** | Forgetting a space after the colon (`port:8080` instead of `port: 8080`) results in malformed YAML syntax errors. | Ensure a whitespace character follows every colon in YAML. |

---

## 8. Performance & Security Notes

### Sensitive Data Masking
Spring Boot Actuator automatically sanitizes values from `/actuator/env`:
- Keys containing `password`, `secret`, `key`, `token`, or `credentials` are masked with `******`.
- Ensure `/actuator/env` is protected behind Spring Security role checks.

### Startup Resolution Speed
Property resolution occurs strictly in memory from an ordered `PropertySources` list. Property lookups take sub-microsecond time; however, deep SpEL expressions inside `@Value` incur minor reflection parsing costs.

---

## 9. Comparison / When to Use What

| Format / Source | Hierarchy Level | Strengths | Best For |
| :--- | :---: | :--- | :--- |
| **CLI Arguments** | Level 4 | Highest operational override | Emergency production port/debug overrides |
| **OS Env Vars** | Level 10 | 12-factor cloud standard | Kubernetes Secrets & ConfigMaps |
| **`application.yaml`** | Level 15 | Hierarchical, readable, structured | Default application configurations |
| **`application.properties`** | Level 15 | Standard flat Java format | Legacy apps, supported by `@PropertySource` |

---

## 10. Interview Questions

<details>
<summary><strong>Q1: What are the top 5 highest precedence property sources in Spring Boot?</strong></summary>

**Answer:**  
1. DevTools global settings (`~/.config/spring-boot/spring-boot-devtools.properties`)
2. `@TestPropertySource` on test classes
3. `@SpringBootTest#properties`
4. **Command-line arguments** (`--server.port=9090`)
5. Properties from **`SPRING_APPLICATION_JSON`**
</details>

<details>
<summary><strong>Q2: What is Relaxed Binding in Spring Boot?</strong></summary>

**Answer:**  
Relaxed Binding is Spring Boot's mechanism for mapping property names across different casing and delimiter styles flexibly. For example, a Java property `connectionTimeout` in a configuration class can be bound from:
- `app.connection-timeout` (kebab-case)
- `app.connectionTimeout` (camelCase)
- `app.connection_timeout` (underscore)
- `APP_CONNECTIONTIMEOUT` or `APP_CONNECTION_TIMEOUT` (uppercase environment variables).
</details>

<details>
<summary><strong>Q3: Why doesn't `@PropertySource` load YAML files by default in Spring?</strong></summary>

**Answer:**  
Standard `@PropertySource` uses Java's built-in `DefaultPropertySourceFactory`, which relies on `java.util.Properties` (a flat key-value parser that only understands `.properties` and `.xml` files). To load YAML files via `@PropertySource`, you must provide a custom `PropertySourceFactory` that uses Spring's `YamlPropertySourceLoader`.
</details>

<details>
<summary><strong>Q4: How do list properties behave when defined in both `application.yaml` and `application-prod.yaml`?</strong></summary>

**Answer:**  
In Spring Boot YAML configuration, **lists do not merge**. The list defined in the higher-precedence file (`application-prod.yaml`) completely replaces and overwrites the entire list from `application.yaml`.
</details>

<details>
<summary><strong>Q5: What is `SPRING_APPLICATION_JSON` and when is it useful?</strong></summary>

**Answer:**  
`SPRING_APPLICATION_JSON` is an environment variable (or system property) that accepts a raw JSON string. Spring Boot parses the JSON payload into property key-value pairs at Priority Level 5. It is particularly useful in cloud containers (Docker/Kubernetes) to override multiple complex, nested configuration properties through a single environment variable.
</details>

<details>
<summary><strong>Q6: How does Spring Boot automatically convert string duration properties to `java.time.Duration`?</strong></summary>

**Answer:**  
Through `StringToDurationConverter`. It recognizes suffixes:
- `ns` (nanoseconds)
- `ms` (milliseconds)
- `s` (seconds)
- `m` (minutes)
- `h` (hours)
- `d` (days)  
If no suffix is provided, milliseconds are assumed by default (or the unit specified by `@DurationUnit`).
</details>

<details>
<summary><strong>Q7: What does the `${random.uuid}` property source provide?</strong></summary>

**Answer:**  
It is powered by `RandomValuePropertySource`. When referenced (e.g., `app.instance-id=${random.uuid}`), Spring Boot dynamically generates a cryptographically random UUID at startup. It also supports `${random.int}`, `${random.long}`, and `${random.int[min,max]}`.
</details>

<details>
<summary><strong>Q8: How do you supply a default fallback value in `@Value`?</strong></summary>

**Answer:**  
Use the colon (`:`) separator after the property key:
```java
@Value("${app.max-retries:3}")
private int maxRetries;
```
If `app.max-retries` is not defined in any property source, Spring injects the fallback value `3`.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Duration and DataSize Injection (Easy)</strong></summary>

**Task:**  
Define `app.cache.ttl=45s` and `app.cache.max-heap=256MB` in `application.yaml`. Inject them into a `@Component` using `Duration` and `DataSize` types. Verify their millisecond and byte values in a unit test.
</details>

<details>
<summary><strong>Exercise 2: Command-Line Override Verification (Easy)</strong></summary>

**Task:**  
Set `server.port=8080` in `application.yaml`. Run the application passing `--server.port=9090`. Verify in console logs that Tomcat binds to port 9090.
</details>

<details>
<summary><strong>Exercise 3: Test Relaxed Binding with Environment Variable (Medium)</strong></summary>

**Task:**  
Write a `@SpringBootTest` test passing an environment variable `APP_MAX_ATTEMPTS=5` via `@SpringBootTest(properties = "APP_MAX_ATTEMPTS=5")`. Inject `@Value("${app.max-attempts}")` and assert it equals `5`.
</details>

<details>
<summary><strong>Exercise 4: Verify YAML List Overwrite Behavior (Medium)</strong></summary>

**Task:**  
Define a list with 3 elements in `application.yaml`. In `application-test.yaml`, define the same list with 1 element. Run a test with `@ActiveProfiles("test")` and assert that the injected list size is 1, not 4.
</details>

<details>
<summary><strong>Exercise 5: Dynamic JSON Configuration via `SPRING_APPLICATION_JSON` (Hard)</strong></summary>

**Task:**  
Configure a test executing with system property `SPRING_APPLICATION_JSON={"app":{"worker":{"threads":16,"name":"CustomWorker"}}}`. Assert that both nested properties are successfully mapped.
</details>

<details>
<summary><strong>Exercise 6: Custom YAML PropertySourceFactory (Hard)</strong></summary>

**Task:**  
Implement a custom `PropertySourceFactory` using `YamlPropertySourceLoader`. Use it with `@PropertySource(value = "classpath:custom.yaml", factory = YamlPropertySourceFactory.class)` to prove that YAML files can be loaded via `@PropertySource`.
</details>

---

## 12. Summary / Cheat Sheet

| Priority Level | Property Source | Example Syntax |
| :---: | :--- | :--- |
| **4** | Command-Line Arguments | `--server.port=9090` |
| **5** | Embedded JSON Env Var | `SPRING_APPLICATION_JSON='{"port":9090}'` |
| **9** | Java System Properties | `-Dserver.port=8081` |
| **10** | OS Environment Variables | `SERVER_PORT=8082` |
| **12** | Profile YAML outside JAR | `config/application-prod.yaml` |
| **13** | Profile YAML inside JAR | `src/main/resources/application-prod.yaml` |
| **14** | Default YAML outside JAR | `config/application.yaml` |
| **15** | Default YAML inside JAR | `src/main/resources/application.yaml` |
| **Duration Units** | Suffix strings | `100ms`, `30s`, `5m`, `1h` -> `Duration` |
| **DataSize Units** | Suffix strings | `512B`, `10KB`, `50MB`, `1GB` -> `DataSize` |

---

## 13. Further Reading
- [Official Spring Boot Externalized Configuration Reference](https://docs.spring.io/spring-boot/docs/current/reference/html/features.html#features.external-config)
- [Spring Boot Relaxed Binding Rules](https://docs.spring.io/spring-boot/docs/current/reference/html/features.html#features.external-config.typesafe-configuration-properties.relaxed-binding)
- [The Twelve-Factor App: Config](https://12factor.net/config)
