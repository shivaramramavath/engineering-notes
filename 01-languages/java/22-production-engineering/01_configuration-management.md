# Configuration Management in Java

> Enterprise patterns for externalized, immutable, and environment-aware configuration in modern Java applications: 12-Factor App design, hierarchy resolution, validation, reloadability, and cloud config sources.

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced / Enterprise Architecture |
| **Prerequisites** | [00_logging](00_logging.md), [03_secrets-management](../21-security/03_secrets-management.md), [04_oop](../04-oop/README.md) |
| **Time to Read** | 35 minutes |
| **Status** | Verified Production Guide |
| **Java Baseline** | Java 17 LTS / Java 21 LTS / Java 25 LTS |

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
10. [Interview Questions & Answers](#10-interview-questions--answers)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### What is Enterprise Configuration Management?
Configuration management is the architectural practice of externalizing all runtime variables, operational thresholds, network endpoints, feature flags, and environment credentials from compiled application binaries.

In enterprise software engineering:
- The compiled artifact (`.jar` or container image) must be **strictly immutable and identical** across all deployment stages: Local Development $\to$ CI/CD $\to$ Staging $\to$ Production.
- Changing behavior between environments is achieved solely by altering external configuration, never by rebuilding code.

### The Twelve-Factor App "Config" Principle
The Twelve-Factor App methodology states:
> *"An application\'s config is everything that is likely to vary between deploys (staging, production, developer environments). Strictly separate config from code."*

Litmus Test: Can the codebase be made open source right now without compromising any passwords, secrets, or internal infrastructure endpoints? If not, configuration management has failed.

---

## 2. Core Concepts

### Configuration Precedence Hierarchy
Production Java applications resolve configuration by cascading through multiple prioritized sources. When the same key exists in multiple locations, higher-priority sources override lower-priority sources:

```
[ Priority 1 (Highest) ] -> CLI Arguments (--server.port=8080)
[ Priority 2           ] -> JVM System Properties (-Dserver.port=8080)
[ Priority 3           ] -> OS Environment Variables (SERVER_PORT=8080)
[ Priority 4           ] -> External Config File (/etc/config/app.yml)
[ Priority 5           ] -> Packaged Config File (classpath:application.properties)
[ Priority 6 (Lowest)  ] -> Built-in Hardcoded Defaults
```

### Strongly-Typed Configuration Objects (Records)
Rather than scattering raw string lookups (`config.get("database.timeout")`) throughout business services, modern Java applications bind configuration trees into **immutable Java records** with fail-fast validation at application startup.

### Environment Profiles
Applications define profile-specific defaults:
- `application.properties` (base defaults shared everywhere)
- `application-dev.properties` (local mocks, verbose logging)
- `application-prod.properties` (production connection pools, minimal logging)

---

## 3. How It Works Internally

### Property Resolution Mechanism
When an application initializes its configuration subsystem:

```
+--------------------------------------------------------------------------+
|                  PROPERTY RESOLUTION PIPELINE                            |
|                                                                          |
| Key Lookup: "database.connection-timeout"                                |
|   |                                                                      |
|   +--> Check CLI Args (`--database.connection-timeout=5000`)             |
|   |    (Found? Return value)                                             |
|   |                                                                      |
|   +--> Check System Properties (`-Ddatabase.connection-timeout=...`)     |
|   |                                                                      |
|   +--> Check Normalized Env Var (`DATABASE_CONNECTION_TIMEOUT=...`)      |
|   |    (Convert dots and hyphens to uppercase underscores)               |
|   |                                                                      |
|   +--> Check Local Config File on Classpath                              |
|   |                                                                      |
|   v                                                                      |
| Validate Constraints (@Positive, @NotNull) & Construct Immutable Record  |
+--------------------------------------------------------------------------+
```

### Environment Variable Normalization (Relaxed Binding)
Operating systems impose strict constraints on environment variable names (Unix shells forbid dots and hyphens). Java configuration engines implement **relaxed binding**:
- A property named `app.security.token-timeout` can be supplied via:
  - System property: `-Dapp.security.token-timeout=30m`
  - Environment variable: `APP_SECURITY_TOKEN_TIMEOUT=30m`
  - JSON/YAML: `app: { security: { token-timeout: 30m } }`

---

## 4. Syntax & Usage

### 1. Declaring Typed Configuration via Java Records
```java
import java.time.Duration;
import java.util.Objects;

public record DatabaseSettings(
    String jdbcUrl,
    String username,
    int maxPoolSize,
    Duration connectionTimeout
) {
    public DatabaseSettings {
        Objects.requireNonNull(jdbcUrl, "jdbcUrl cannot be null");
        Objects.requireNonNull(username, "username cannot be null");
        if (maxPoolSize < 1 || maxPoolSize > 100) {
            throw new IllegalArgumentException("maxPoolSize must be between 1 and 100");
        }
        if (connectionTimeout == null || connectionTimeout.isNegative()) {
            throw new IllegalArgumentException("connectionTimeout must be positive");
        }
    }
}
```

---

## 5. Practical Examples

### Example 1: Self-Contained Hierarchical Configuration Engine
**Goal:** Implement a clean, zero-dependency property loader supporting System Properties, Environment Variables, and Fallback Defaults with type conversion.

```java
import java.time.Duration;
import java.util.Optional;

public class ConfigurationEngine {

    public static String getString(String key, String defaultValue) {
        return resolveRaw(key).orElse(defaultValue);
    }

    public static int getInt(String key, int defaultValue) {
        return resolveRaw(key)
                .map(Integer::parseInt)
                .orElse(defaultValue);
    }

    public static Duration getDuration(String key, Duration defaultValue) {
        return resolveRaw(key)
                .map(Duration::parse) // Supports ISO-8601 strings like PT30S
                .orElse(defaultValue);
    }

    public static boolean getBoolean(String key, boolean defaultValue) {
        return resolveRaw(key)
                .map(Boolean::parseBoolean)
                .orElse(defaultValue);
    }

    private static Optional<String> resolveRaw(String key) {
        // Priority 1: JVM System Property (-Dkey=value)
        String sysProp = System.getProperty(key);
        if (sysProp != null && !sysProp.isBlank()) {
            return Optional.of(sysProp);
        }

        // Priority 2: OS Environment Variable (dots and hyphens converted to underscores)
        String envKey = key.replace('.', '_').replace('-', '_').toUpperCase();
        String envVal = System.getenv(envKey);
        if (envVal != null && !envVal.isBlank()) {
            return Optional.of(envVal);
        }

        return Optional.empty();
    }
}
```

### Example 2: Immutable Application Configuration with Startup Validation
**Goal:** Bootstrap application configuration into a domain record and validate all invariants immediately upon startup (fail-fast principle).

```java
import java.time.Duration;

public record AppConfiguration(
    ServerConfig server,
    DatabaseConfig database
) {
    public record ServerConfig(int port, String host) {}
    public record DatabaseConfig(String url, int poolSize, Duration timeout) {}

    public static AppConfiguration load() {
        int port = ConfigurationEngine.getInt("server.port", 8080);
        String host = ConfigurationEngine.getString("server.host", "0.0.0.0");
        
        String dbUrl = ConfigurationEngine.getString("database.url", null);
        if (dbUrl == null || dbUrl.isBlank()) {
            throw new IllegalStateException("CRITICAL STARTUP FAILURE: database.url is mandatory!");
        }

        int poolSize = ConfigurationEngine.getInt("database.pool-size", 10);
        Duration timeout = ConfigurationEngine.getDuration("database.timeout", Duration.ofSeconds(30));

        ServerConfig serverConfig = new ServerConfig(port, host);
        DatabaseConfig databaseConfig = new DatabaseConfig(dbUrl, poolSize, timeout);

        return new AppConfiguration(serverConfig, databaseConfig);
    }
}
```

### Example 3: Dynamic Property Reloading with AtomicReference
**Goal:** Support dynamic reconfiguration of non-critical operational properties (like circuit breaker thresholds or rate limits) without restarting the JVM.

```java
import java.util.concurrent.atomic.AtomicReference;

public class DynamicFeatureSettings {
    public record FeatureFlags(boolean newCheckoutEnabled, int maxRetries) {}

    private final AtomicReference<FeatureFlags> currentFlags = 
        new AtomicReference<>(new FeatureFlags(false, 3));

    public FeatureFlags getFlags() {
        return currentFlags.get();
    }

    public void updateFlags(boolean newCheckout, int maxRetries) {
        // Thread-safe atomic pointer swap
        currentFlags.set(new FeatureFlags(newCheckout, maxRetries));
    }
}
```

### Example 4: Failure Case — Insecure and Fragile Configuration
```java
// ANTI-PATTERN: Hardcoded Endpoints and Raw In-Place Lookups
public void processPayment() {
    // BUG 1: Fragile raw lookup without validation; NullPointerException if missing!
    int timeout = Integer.parseInt(System.getProperty("pay.timeout")); 
    
    // BUG 2: Hardcoded staging endpoint in production code!
    String url = "https://staging-api.paymentgateway.com/v1"; 
}
```

---

## 6. Best Practices

1. **Fail Fast on Missing Configurations**: If a required property is missing or invalid, crash the JVM at startup with a clear error message. Never proceed with default null values.
2. **Bind to Immutable Records**: Never pass `java.util.Properties` or config maps into business logic. Parse into strongly-typed records.
3. **Follow Relaxed Environment Variable Mapping**: Support uppercase underscored names (`DATABASE_URL`) alongside dotted names (`database.url`).
4. **Never Log Secrets**: Redact sensitive configuration items (passwords, tokens) when printing configuration summaries during startup.
5. **Keep Container Images Environment-Agnostic**: Build one single Docker image and inject configuration via Kubernetes ConfigMaps and Secrets.
6. **Prefer ISO-8601 for Durations**: Use standard duration formatting (`PT5S`, `PT10M`) instead of ambiguous raw integers (`timeout=5000` is ambiguous between seconds and milliseconds).

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Secure Fix |
| :--- | :--- | :--- |
| **Using raw milliseconds integers (`timeout=5000`)** | Developers confuse seconds vs. milliseconds, causing 1000x misconfigurations. | Use `java.time.Duration` or ISO-8601 strings (`PT5S`). |
| **Baking environment-specific config files into images** | Requires rebuilding images to change staging to production. | Externalize config via environment variables or volumes. |
| **Silent fallbacks for critical properties** | Missing production DB URL falls back to local SQLite/H2 silently. | Throw `IllegalStateException` on missing mandatory keys. |
| **Scattering `System.getenv()` calls across codebase** | Makes code impossible to unit test and audit. | Centralize configuration in a dedicated bootstrap module. |
| **Mutable configuration classes** | Threads can mutate configuration state at runtime causing race conditions. | Use Java records with immutable fields. |
| **Hardcoding `localhost` defaults** | Containerized microservices fail to connect across Docker networks. | Default to `0.0.0.0` or require explicit service discovery URLs. |

---

## 8. Performance & Security Notes

### Startup Parsing vs. Runtime Performance
Property parsing and string transformations should occur once during JVM initialization:
- Resolving environment variables or system properties takes a few microseconds, but doing so inside high-throughput business methods creates unnecessary string allocations.
- Parse once into immutable records, inject the records via dependency injection or constructors, and access primitive fields with zero lookup overhead.

---

## 9. Comparison / When to Use What

| Configuration Source | Precedence | Mutability | Best For |
| :--- | :--- | :--- | :--- |
| **CLI Arguments (`--key=val`)** | Highest | Fixed at launch | One-off overrides, local debugging, container entrypoints |
| **System Properties (`-Dkey=val`)**| High | Fixed at launch | JVM tuning, container runtime overrides |
| **Environment Variables** | High | Fixed at launch | Kubernetes ConfigMaps, Docker Compose, 12-Factor apps |
| **Dynamic Config Server (Spring Cloud / Consul)** | Moderate | Hot-Reloadable | Feature flags, circuit breaker thresholds, runtime tuning |
| **Packaged Files (`application.properties`)** | Lowest | Fixed in build | Sane baseline defaults across all environments |

---

## 10. Interview Questions & Answers

### Q1: What is the 12-Factor App principle regarding configuration?
<details>
<summary>View Answer</summary>
The 12-Factor App methodology mandates that configuration must be strictly separated from code. An application\'s codebase must be identical across all deploys, meaning the exact same compiled container image or JAR is deployed to development, staging, and production. All environment-varying values (database URLs, external API credentials, cache hosts) must be externalized and injected via the operating system environment or runtime config providers.
</details>

### Q2: What is "relaxed binding" in configuration management?
<details>
<summary>View Answer</summary>
Relaxed binding is the ability of a configuration engine to match configuration keys across different naming conventions interchangeably. For example, a property declared in Java as `spring.datasource.max-active` can be provided via an environment variable as `SPRING_DATASOURCE_MAX_ACTIVE`, via command-line argument as `--spring.datasource.maxActive`, or via YAML as `spring: { datasource: { max-active: 10 } }`.
</details>

### Q3: Why should configuration values be bound to immutable records rather than queried via `System.getProperty()` on demand?
<details>
<summary>View Answer</summary>
Directly calling `System.getProperty()` throughout business logic creates tight coupling, makes unit testing difficult (requires mutating global JVM state), lacks type safety, and prevents early validation of constraints. Binding into immutable Java records centralizes all configuration logic, validates all invariants at startup (fail-fast), and allows easy mocking during unit tests by simply instantiating the record with test parameters.
</details>

### Q4: How should duration properties (like timeouts) be represented in configuration files?
<details>
<summary>View Answer</summary>
Durations should never be stored as raw ambiguous numbers (e.g. `timeout=30`), which leads to confusion between seconds and milliseconds. Instead, use ISO-8601 strings (e.g., `PT30S`, `PT5M`) or explicit unit suffixes (e.g., `30s`, `500ms`, `2h`) parsed directly into `java.time.Duration`. This makes configuration self-documenting and eliminates unit conversion bugs.
</details>

### Q5: What is the "Fail-Fast" principle in application bootstrapping?
<details>
<summary>View Answer</summary>
The fail-fast principle states that if an application cannot function correctly due to missing or invalid prerequisites (such as a missing database URL, an invalid port number, or an unparseable secret), it should terminate immediately at startup with an informative error message. Silently defaulting missing properties or continuing with incomplete state leads to runtime failures during production traffic.
</details>

### Q6: How can runtime configuration be reloaded safely without restarting the JVM?
<details>
<summary>View Answer</summary>
Runtime configuration can be reloaded safely using immutable configuration snapshots swapped via `AtomicReference`. When a notification (e.g. webhook, Spring Cloud Bus, or file watcher) indicates new configuration is available, the application parses and validates the entire new configuration graph. If validation succeeds, it atomically updates the `AtomicReference`. Running worker threads either finish with their current snapshot or read the new immutable reference, avoiding race conditions or half-updated state.
</details>

### Q7: Why are Java System Properties considered less secure than Environment Variables for credentials?
<details>
<summary>View Answer</summary>
System properties are passed via command-line flags (`-Dpassword=secret`). On Unix and Linux operating systems, command-line arguments of all running processes are visible in plain text to any local user via commands like `ps aux` or by reading `/proc/<pid>/cmdline`. Environment variables are restricted to the process and its authorized parent/child hierarchy and do not appear in process listings.
</details>

### Q8: How should sensitive passwords be handled when printing configuration on application startup?
<details>
<summary>View Answer</summary>
Startup configuration loggers must implement sanitization or masking filters. Any property key matching sensitive keywords (e.g., `password`, `secret`, `key`, `token`, `credential`, `auth`) must have its value replaced with asterisks (`******`) or redacted markers (`[PROTECTED]`) before being printed to logs.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build an Environment Variable Type Converter
**Task:** Implement a converter method that parses strings like `10s`, `500ms`, `2m`, and `1h` into `java.time.Duration`.
<details>
<summary>View Solution</summary>

```java
import java.time.Duration;

public class DurationParser {
    public static Duration parse(String input) {
        if (input == null || input.isBlank()) {
            throw new IllegalArgumentException("Duration string cannot be null or blank");
        }
        String clean = input.trim().toLowerCase();
        if (clean.endsWith("ms")) {
            long ms = Long.parseLong(clean.substring(0, clean.length() - 2).trim());
            return Duration.ofMillis(ms);
        } else if (clean.endsWith("s")) {
            long s = Long.parseLong(clean.substring(0, clean.length() - 1).trim());
            return Duration.ofSeconds(s);
        } else if (clean.endsWith("m")) {
            long m = Long.parseLong(clean.substring(0, clean.length() - 1).trim());
            return Duration.ofMinutes(m);
        } else if (clean.endsWith("h")) {
            long h = Long.parseLong(clean.substring(0, clean.length() - 1).trim());
            return Duration.ofHours(h);
        }
        return Duration.parse(input); // Fallback to standard ISO-8601
    }
}
```
</details>

### Exercise 2: Cascading Properties Resolver
**Task:** Write a property resolver that checks in order: JVM System Properties $\to$ OS Environment Variables $\to$ Default Map.
<details>
<summary>View Solution</summary>

```java
import java.util.Map;

public class CascadingResolver {
    public static String resolve(String key, Map<String, String> defaults) {
        // 1. System property
        String val = System.getProperty(key);
        if (val != null && !val.isBlank()) return val;

        // 2. Env var
        String envKey = key.replace('.', '_').replace('-', '_').toUpperCase();
        val = System.getenv(envKey);
        if (val != null && !val.isBlank()) return val;

        // 3. Defaults
        return defaults.get(key);
    }
}
```
</details>

### Exercise 3: Validated Port Range Configuration Record
**Task:** Create a record `ServerBinding` that ensures `port` is between 1024 and 65535, and `host` is not blank.
<details>
<summary>View Solution</summary>

```java
import java.util.Objects;

public record ServerBinding(String host, int port) {
    public ServerBinding {
        Objects.requireNonNull(host, "host cannot be null");
        if (host.isBlank()) {
            throw new IllegalArgumentException("host cannot be blank");
        }
        if (port < 1024 || port > 65535) {
            throw new IllegalArgumentException("Port must be between 1024 and 65535 (non-privileged range)");
        }
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Strict Separation**: Build immutable binaries; inject all environment differences via config.
- **Hierarchy Order**: CLI Args $\to$ System Props $\to$ Env Vars $\to$ Config Files $\to$ Defaults.
- **Fail Fast**: Abort JVM initialization immediately if required properties are missing.
- **Immutable Records**: Parse configuration once into validated Java records.
- **Durations**: Use ISO-8601 or explicit time units (`PT10S`, `500ms`) with `java.time.Duration`.
- **Mask Secrets**: Redact passwords and tokens when dumping configuration on startup.

---

## 13. Further Reading
- The Twelve-Factor App: [Config Factor](https://12factor.net/config)
- MicroProfile Config Specification: [Eclipse MicroProfile Config](https://microprofile.io/project/eclipse/microprofile-config/)
- Spring Boot Documentation: [Externalized Configuration](https://docs.spring.io/spring-boot/docs/current/reference/html/features.html#features.external-config)
- ISO-8601 Duration Format: [W3C Duration Specifications](https://www.w3.org/TR/xmlschema-2/#duration)
