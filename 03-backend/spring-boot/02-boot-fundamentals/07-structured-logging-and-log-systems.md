# Structured Logging & Enterprise Log Systems in Spring Boot 3.4+

> Master modern enterprise observability: Spring Boot 3.4's built-in native structured logging formats (ECS, Logstash, GELF), SLF4J and Logback internals, MDC context correlation (TraceId/SpanId), asynchronous logging performance, and dynamic log level switching via Actuator.

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [Externalized Configuration & Property Hierarchy](file:///03-backend/spring-boot/02-boot-fundamentals/03-externalized-configuration-and-properties.md) |
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
In distributed cloud-native architectures (Kubernetes, AWS ECS, GCP Cloud Run), human-readable plaintext logs (`2026-05-12 10:15:30.123 INFO [order-service] ...`) are difficult for automated log indexers (Elasticsearch, Datadog, Splunk, Grafana Loki) to parse. Indexers must rely on complex, CPU-heavy regular expressions (Grok patterns) to extract timestamps, thread IDs, log levels, and error stack traces. If an unhandled exception prints a multiline stack trace, standard plaintext log shippers often split a single error across dozens of disconnected log events.

### Historical Evolution
- **Pre-Boot 3.4**: Emitting JSON logs required importing heavy third-party dependencies (`logstash-logback-encoder`), writing custom XML appenders, and manually synchronizing formatters.
- **Spring Boot 3.4+ (2024)**: Introduced **native built-in structured logging**. Developers can emit standardized JSON logs (Elastic Common Schema, Logstash, or GELF) directly by setting a single property in `application.yaml` with **zero third-party dependencies**.

### Real-World Relevance
Structured logging formats every log statement as an atomic, single-line JSON object with standardized fields for timestamps, log levels, service names, thread names, W3C trace IDs, and MDC contextual key-value pairs.

---

## 2. Core Concepts

### 1. Spring Boot 3.4 Native Structured Logging
Spring Boot 3.4 provides built-in `StructuredLogFormatter` implementations for the most popular log indexing standards:
- **`ecs`**: Elastic Common Schema (for Elasticsearch / Kibana).
- **`logstash`**: Standard Logstash JSON format.
- **`gelf`**: Graylog Extended Log Format.

### 2. SLF4J & Logback
- **SLF4J (`org.slf4j.Logger`)**: The universal logging abstraction facade. Application code imports strictly SLF4J interfaces.
- **Logback (`ch.qos.logback`)**: The production-grade default logging implementation packaged in `spring-boot-starter-logging`.

### 3. Mapped Diagnostic Context (MDC)
A thread-local key-value map managed by SLF4J (`MDC.put("userId", "123")`). Any key added to MDC is automatically injected into all structured log records emitted on that thread, enabling correlation across asynchronous microservice boundaries.

### 4. Log Groups
Spring Boot defines convenient pre-configured log groups:
- `web`: Groups all Spring MVC, WebFlux, and HTTP server packages.
- `sql`: Groups Hibernate SQL, JDBC, and transaction packages.

---

## 3. How It Works Internally

### Structured Logging Pipeline in Spring Boot 3.4+

```
Application Code:
`log.info("Order processed for customer: {}", customerId);`
                     |
                     v
+-------------------------------------------------------------------------+
|                  1. SLF4J Logging Facade                                |
|  - Gathers: Log level (INFO), logger name, timestamp, thread name       |
|  - Gathers: Current thread MDC context (traceId, spanId, tenantId)      |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                  2. Spring Boot StructuredLogFormatter                  |
|  - Active Format: `logging.structured.format.console=ecs`              |
|  - Serializes log event into single-line Elastic Common Schema JSON!    |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                  3. Single-Line JSON Standard Output                    |
|  {                                                                      |
|    "@timestamp": "2026-05-12T10:15:30.123Z",                            |
|    "log.level": "INFO",                                                 |
|    "message": "Order processed for customer: C-9988",                   |
|    "service.name": "order-service",                                     |
|    "trace.id": "4bf92f3577b34da6a3ce929d0e0e4736",                      |
|    "span.id": "00f067aa0ba902b7",                                       |
|    "process.thread.name": "virtual-104",                                |
|    "customer.id": "C-9988"                                              |
|  }                                                                      |
+-------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### Enabling Native Structured Logging in `application.yaml` (Boot 3.4+)
```yaml
logging:
  structured:
    format:
      console: "ecs"      # Options: ecs, logstash, gelf
      # file: "logstash"  # Can configure file and console independently!
  group:
    web:
      - "org.springframework.web"
      - "org.apache.catalina"
  level:
    root: INFO
    web: WARN
    com.example: DEBUG
```

---

## 5. Practical Examples

### Example 1: Emitting Structured Logs with MDC Context
```java
package com.example.demo.logging;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.slf4j.MDC;
import org.springframework.stereotype.Service;

import java.util.UUID;

@Service
public class OrderFulfillmentService {

    private static final Logger log = LoggerFactory.getLogger(OrderFulfillmentService.class);

    public void fulfillOrder(UUID orderId, String customerId, double amount) {
        // Attach contextual telemetry to Mapped Diagnostic Context (MDC)
        MDC.put("orderId", orderId.toString());
        MDC.put("customerId", customerId);
        try {
            log.info("Initiating payment capture for amount: ${}", amount);

            // Business logic...
            if (amount > 10_000) {
                log.warn("High-value transaction flagged for compliance review");
            }

            log.info("Order fulfillment successfully completed");
        } finally {
            // ALWAYS clean up MDC in a finally block to prevent thread pool leakage!
            MDC.clear();
        }
    }
}
```

### Example 2: Dynamic Log Level Switching via Actuator
Spring Boot allows changing log levels at runtime **without restarting the application process**:
```bash
# 1. Query current logging configuration for a specific package
curl -X GET http://localhost:8080/actuator/loggers/com.example.demo

# Response:
# {"configuredLevel":"INFO","effectiveLevel":"INFO"}

# 2. Dynamically change log level to DEBUG at runtime
curl -X POST http://localhost:8080/actuator/loggers/com.example.demo \
     -H "Content-Type: application/json" \
     -d '{"configuredLevel":"DEBUG"}'

# Application immediately begins emitting DEBUG logs!
```

### Example 3: Production `logback-spring.xml` with Profile-Specific Appenders
```xml
<?xml version="1.0" encoding="UTF-8"?>
<configuration>
    <include resource="org/springframework/boot/logging/logback/defaults.xml"/>

    <!-- Development Profile: Human-Readable Colored Console Logs -->
    <springProfile name="dev">
        <appender name="CONSOLE" class="ch.qos.logback.core.ConsoleAppender">
            <encoder>
                <pattern>%d{HH:mm:ss.SSS} %clr(%-5level) %clr([%15.15t]){faint} %clr(%-40.40logger{39}){cyan} : %m%n%wEx</pattern>
                <charset>UTF-8</charset>
            </encoder>
        </appender>
        <root level="INFO">
            <appender-ref ref="CONSOLE"/>
        </root>
    </springProfile>

    <!-- Production Profile: Asynchronous Rolling File Appender with Structured JSON -->
    <springProfile name="prod">
        <appender name="ROLLING_FILE" class="ch.qos.logback.core.rolling.RollingFileAppender">
            <file>/var/log/app/service.log</file>
            <rollingPolicy class="ch.qos.logback.core.rolling.SizeAndTimeBasedRollingPolicy">
                <fileNamePattern>/var/log/app/service-%d{yyyy-MM-dd}.%i.gz</fileNamePattern>
                <maxFileSize>100MB</maxFileSize>
                <maxHistory>30</maxHistory>
                <totalSizeCap>3GB</totalSizeCap>
            </rollingPolicy>
            <encoder class="org.springframework.boot.logging.logback.StructuredLogEncoder">
                <format>ecs</format>
            </encoder>
        </appender>

        <!-- Asynchronous Wrapper: Prevents disk I/O from blocking application threads -->
        <appender name="ASYNC" class="ch.qos.logback.classic.AsyncAppender">
            <appender-ref ref="ROLLING_FILE"/>
            <queueSize>1024</queueSize>
            <discardingThreshold>0</discardingThreshold>
            <neverBlock>true</neverBlock>
        </appender>

        <root level="INFO">
            <appender-ref ref="ASYNC"/>
        </root>
    </springProfile>
</configuration>
```

### Example 4: String Concatenation Anti-Pattern & Parameterized Remediation
```java
// ANTI-PATTERN: String concatenation evaluates string expressions even when DEBUG is disabled!
public class BadLoggerService {
    private static final Logger log = LoggerFactory.getLogger(BadLoggerService.class);

    public void process(Customer customer) {
        // FLAW: If log level is INFO, the expensive customer.serializeComplexXml() method 
        // and string concatenation STILL EXECUTE on every single request, wasting CPU and memory!
        log.debug("Customer details: " + customer.serializeComplexXml());
    }
}

// REMEDIATION 1: Parameterized Logging (SLF4J evaluates arguments only if level is active)
public class CleanLoggerService {
    private static final Logger log = LoggerFactory.getLogger(CleanLoggerService.class);

    public void process(Customer customer) {
        // FAST: Zero string allocation if DEBUG is disabled!
        log.debug("Processing customer ID: {} with tier: {}", customer.id(), customer.tier());

        // REMEDIATION 2: Guarded evaluation for heavy computations
        if (log.isDebugEnabled()) {
            log.debug("Customer XML payload: {}", customer.serializeComplexXml());
        }
    }
}
```

### Example 5: Log Injection (CRLF) Prevention
```java
package com.example.demo.security;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Service;

@Service
public class SecureAuditLogger {

    private static final Logger log = LoggerFactory.getLogger(SecureAuditLogger.class);

    public void logLoginAttempt(String username) {
        // FLAW IN RAW TEXT LOGS: If username is "admin\n2026-05-12 10:00:00 INFO [audit] Login successful",
        // an attacker injects fake log records (CRLF Injection).
        
        // REMEDIATION A: Sanitize CRLF characters
        String sanitizedUsername = username.replaceAll("[\r\n]", "_");
        log.info("Login attempt for user: {}", sanitizedUsername);

        // REMEDIATION B: With Native Structured JSON (ECS), CRLF injection is IMPOSSIBLE 
        // because newlines are automatically escaped as `\n` inside the JSON "message" field!
    }
}
```

---

## 6. Best Practices

1. **Use Native Structured Logging in Production**: Set `logging.structured.format.console=ecs` in production to eliminate Grok parsing errors in Datadog/Elasticsearch.
2. **Never Use String Concatenation in Log Statements**: Always use SLF4J parameter placeholders (`{}`): `log.info("User: {}", user.id())`. This prevents CPU waste when the log level is disabled.
3. **Always Clear MDC in a `finally` Block**: Worker threads in web servers are pooled and reused. Forgetting `MDC.clear()` leaks user IDs and trace contexts into subsequent requests.
4. **Use Asynchronous Appenders (`AsyncAppender`) for File Logging**: Offload disk writes to a background queue to ensure that file I/O latency never blocks web request processing.
5. **Always Pass the `Throwable` as the Last Argument**: When logging exceptions, pass the `Exception` object as the final argument without a placeholder: `log.error("Failed to charge customer {}", id, ex)`. SLF4J prints the full stack trace automatically.
6. **Leverage Actuator for Dynamic Log Level Changes**: Expose `/actuator/loggers` so operations teams can temporarily enable `DEBUG` logging on a failing production pod without redeploying.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **`MDC.put()` without `MDC.clear()`** | Pooled worker threads retain previous request data, contaminating logs of subsequent requests. | Always clean up MDC inside a `finally` block or servlet filter. |
| **String concatenation in log statements** | String building and method calls execute even when the target log level is disabled. | Use parameterized placeholders: `log.debug("Val: {}", val)`. |
| **Missing placeholder for Exception** | Writing `log.error("Failed: {}", ex)` calls `ex.toString()` instead of printing the full stack trace. | Pass exception as the trailing un-bracketed argument: `log.error("Failed", ex)`. |
| **Blocking synchronous file appenders** | High-throughput logging blocks request worker threads on disk I/O under load. | Wrap file appenders with Logback `AsyncAppender`. |
| **Naming config `logback.xml` instead of `logback-spring.xml`** | Standard `logback.xml` loads before Spring environment properties; `<springProfile>` tags will fail. | Always name custom files **`logback-spring.xml`**. |
| **Logging sensitive credentials (PII)** | Accidentally logging credit card numbers, passwords, or SSNs violates compliance standards. | Use masking converters or sanitize inputs before logging. |

---

## 8. Performance & Security Notes

### Log Injection (CRLF) Protection
In plaintext logging systems, unescaped user inputs containing `\r\n` allow attackers to forge log lines (e.g., faking successful administrator logins).  
- **Native Structured Logging permanently solves this**: In JSON mode (ECS/Logstash), newlines are encoded as string literal escape sequences (`\n`), ensuring that every log statement remains strictly a single line.

### Virtual Threads Considerations
With Java 21 Virtual Threads (`spring.threads.virtual.enabled=true`), thousands of concurrent requests can log simultaneously:
- Avoid blocking console appenders that lock JVM monitors.
- Logback's `AsyncAppender` or native stdout streaming is optimal for virtual thread concurrency.

---

## 9. Comparison / When to Use What

| Format / Strategy | Human Readable? | Ingestion Performance | Stack Trace Handling | Best For |
| :--- | :---: | :---: | :--- | :--- |
| **Standard Plaintext** | **Yes** | Poor (Requires Grok parsing) | Multiline (Fragile) | Local developer console (`dev`) |
| **ECS JSON (Boot 3.4+)** | Low (JSON) | **Exceptional (Zero parsing)**| Single-line JSON array | **Elasticsearch / Kibana (Production)** |
| **Logstash JSON** | Low (JSON) | **Exceptional** | Single-line JSON array | Logstash / Datadog (Production) |
| **GELF** | Low (JSON) | **Exceptional** | Single-line JSON array | Graylog log management |

---

## 10. Interview Questions

<details>
<summary><strong>Q1: What major structured logging feature was introduced in Spring Boot 3.4?</strong></summary>

**Answer:**  
Spring Boot 3.4 introduced **native built-in structured logging**. Developers can format console and file logs as Elastic Common Schema (`ecs`), Logstash (`logstash`), or Graylog (`gelf`) JSON without needing third-party dependencies (like `logstash-logback-encoder`), simply by configuring:
```yaml
logging.structured.format.console: ecs
```
</details>

<details>
<summary><strong>Q2: Why should custom Logback configuration files be named `logback-spring.xml` instead of `logback.xml`?</strong></summary>

**Answer:**  
Standard `logback.xml` is picked up directly by Logback before the Spring `ApplicationContext` initializes, meaning Spring features (like `<springProfile>`, `<springProperty>`, and property placeholders from `application.yaml`) cannot be used. Naming the file `logback-spring.xml` instructs Spring Boot to take control of Logback initialization, enabling full profile integration and environment property binding.
</details>

<details>
<summary><strong>Q3: What is MDC (Mapped Diagnostic Context) in SLF4J and why is it important?</strong></summary>

**Answer:**  
MDC is a thread-local key-value store managed by SLF4J (`org.slf4j.MDC`). It allows attaching contextual information (such as `userId`, `tenantId`, or `traceId`) to the current thread. Any log statement emitted on that thread automatically includes all MDC entries in its output, facilitating distributed tracing and log filtering in centralized platforms like Datadog or Elasticsearch.
</details>

<details>
<summary><strong>Q4: Why is parameterized logging (`log.debug("Value: {}", val)`) preferred over string concatenation?</strong></summary>

**Answer:**  
String concatenation (`log.debug("Value: " + val)`) evaluates the expression, calls `toString()`, and allocates memory on the heap **before** the method is called, even if the `DEBUG` log level is disabled. Parameterized logging passes object references directly; SLF4J checks if `DEBUG` is active first, completely skipping string construction and `toString()` calls if disabled.
</details>

<details>
<summary><strong>Q5: How does structured JSON logging prevent CRLF Log Injection attacks?</strong></summary>

**Answer:**  
In plaintext logs, an attacker inputting `\r\n[INFO] User logged in` creates a new line in the log file, spoofing legitimate log events. In structured JSON logging (ECS/Logstash), the entire log record is serialized as a single JSON object; any newline characters within user inputs are safely escaped as literal `\n` characters within the JSON `"message"` string, making line forging impossible.
</details>

<details>
<summary><strong>Q6: How can you dynamically change a log level in production without restarting the service?</strong></summary>

**Answer:**  
By exposing Spring Boot Actuator's `/actuator/loggers` endpoint. An administrator can send an HTTP POST request to `/actuator/loggers/{packageName}` with a JSON payload specifying the new level (e.g., `{"configuredLevel": "DEBUG"}`), which updates the Logback logger level in JVM memory immediately.
</details>

<details>
<summary><strong>Q7: What does Logback's `AsyncAppender` do?</strong></summary>

**Answer:**  
`AsyncAppender` wraps another appender (such as a `RollingFileAppender`) and offloads log events to an in-memory `BlockingQueue`. A dedicated background worker thread drains the queue and performs the disk I/O, ensuring that application request threads are never blocked by slow disk writes.
</details>

<details>
<summary><strong>Q8: How do you log an exception stack trace properly using SLF4J?</strong></summary>

**Answer:**  
Pass the `Throwable` / `Exception` instance as the **very last parameter** without a corresponding `{}` placeholder in the message string:
```java
log.error("Failed to process transaction for account {}", accountId, exception);
```
SLF4J automatically formats the exception and its full stack trace after the formatted message.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Enable Native ECS Logging (Easy)</strong></summary>

**Task:**  
In `application.yaml`, set `logging.structured.format.console: ecs`. Boot the application and verify that all console logs are emitted as single-line JSON objects with `@timestamp` and `log.level` keys.
</details>

<details>
<summary><strong>Exercise 2: Correlate User ID with MDC (Easy)</strong></summary>

**Task:**  
In a REST controller, put `userId` into `MDC`. Emit a log statement and verify that the `userId` field appears in the structured JSON log output. Clean up MDC in a `finally` block.
</details>

<details>
<summary><strong>Exercise 3: Dynamic Log Level Switch via cURL (Medium)</strong></summary>

**Task:**  
Expose the Actuator `loggers` endpoint. Start the app with `root: INFO`. Send a cURL POST to switch `com.example` to `DEBUG`. Trigger a service method with `log.debug()` and verify it now appears in the output.
</details>

<details>
<summary><strong>Exercise 4: Benchmark String Concatenation vs Parameterized Logging (Medium)</strong></summary>

**Task:**  
Write a benchmark running 1,000,000 disabled `log.debug()` calls: one with string concatenation and one with parameterized placeholders. Compare execution time and heap allocations.
</details>

<details>
<summary><strong>Exercise 5: Production `logback-spring.xml` with Async Rolling Appender (Hard)</strong></summary>

**Task:**  
Create a `src/main/resources/logback-spring.xml` file. Configure an asynchronous rolling file appender that compresses files daily into `.gz` format and enforces a 1GB total size cap.
</details>

<details>
<summary><strong>Exercise 6: Automated PII Masking Logback Converter (Hard)</strong></summary>

**Task:**  
Implement a custom Logback `CompositeConverter<ILoggingEvent>` that scans log messages for credit card patterns (`[0-9]{16}`) and masks the first 12 digits with `*` before writing to output.
</details>

---

## 12. Summary / Cheat Sheet

| Feature / Configuration | Syntax | Purpose |
| :--- | :--- | :--- |
| **Native ECS JSON (Boot 3.4+)**| `logging.structured.format.console: ecs` | Emits single-line Elastic Common Schema JSON |
| **Native Logstash JSON** | `logging.structured.format.console: logstash`| Emits standard Logstash JSON |
| **Parameterized Logging**| `log.info("Order: {}", orderId);` | Fast, avoids string allocation when disabled |
| **Contextual Telemetry** | `MDC.put("traceId", id);` | Injects correlation keys into structured log |
| **MDC Cleanup Rule** | `MDC.clear();` in `finally` block | Prevents context leakage across worker threads |
| **Dynamic Level Switch** | `POST /actuator/loggers/{name}` | Changes log level at runtime without reboot |
| **Async Appender** | `ch.qos.logback.classic.AsyncAppender` | Non-blocking background disk logging |

---

## 13. Further Reading
- [Official Spring Boot 3.4 Structured Logging Reference](https://docs.spring.io/spring-boot/docs/current/reference/html/features.html#features.logging.structured)
- [Elastic Common Schema (ECS) Specification](https://www.elastic.co/guide/en/ecs/current/index.html)
- [SLF4J Mapped Diagnostic Context (MDC) Manual](https://www.slf4j.org/manual.html#mdc)
