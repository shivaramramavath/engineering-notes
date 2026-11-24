# Logging Best Practices and Production Discipline

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x |
| **Spring Framework Version** | 6.2.x |
| **Java Version** | 21 LTS |
| **Module** | 12-observability-and-operations |
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
Logging is the most fundamental diagnostic mechanism in software engineering, yet it is frequently the source of major production outages and security vulnerabilities:
- Unchecked debug logging in production exhausts disk space and incurs massive log ingestion costs (Datadog/Elastic).
- Synchronous logging blocks high-throughput worker threads during disk or network I/O contention.
- Plaintext logging of passwords, tokens, or personal identifiers (PII) violates GDPR, HIPAA, and PCI-DSS compliance.
- Unsanitized user inputs in logs enable **Log Injection / CRLF attacks**, corrupting security audit trails.

### Historical Evolution
In early Java systems, developers logged using `System.out.println()` or `java.util.logging`. This evolved through Log4j 1.x, Apache Commons Logging (JCL), and ultimately converged on **SLF4J (Simple Logging Facade for Java)** backed by **Logback** as the default logging implementation in Spring Boot.

Modern Spring Boot 3.x enforces rigorous production logging discipline: parameterized messaging, asynchronous appenders, native structured logging, dynamic log level tuning via Actuator, and automated trace correlation via Micrometer.

### Production Relevance
Adhering to professional logging practices ensures that:
- Incidents can be triaged rapidly with precise, contextual error logs.
- Applications sustain high throughput ($> 50,000$ req/sec) without I/O blocking.
- Customer credentials and confidential business records remain strictly sanitized.

---

## 2. Core Concepts

### The Hierarchy of Log Levels
1. **TRACE**: Extremely fine-grained diagnostic information for deep library/protocol debugging (e.g., individual HTTP header bytes, TCP socket handshakes). Almost never enabled in production.
2. **DEBUG**: Diagnostic events useful during development and localized troubleshooting (e.g., intermediate calculation steps, cache misses, branch conditions).
3. **INFO**: Informational lifecycle milestones representing normal system operation (e.g., service startup complete, batch job started/completed, external configuration loaded). Keep volume moderate.
4. **WARN**: Potentially harmful situations that do not stop application execution but indicate degradation or unexpected conditions (e.g., database connection slow query threshold exceeded, deprecated API called, temporary retry triggered).
5. **ERROR**: Severe error events that impede normal operation or require immediate engineering attention (e.g., unhandled runtime exceptions, payment gateway failure, database connectivity loss).

---

## 3. How It Works Internally

### Log Event Evaluation & Appender Lifecycle

```
APPLICATION THREAD
  |
  |  log.debug("User {} balance: {}", userId, balance);
  v
+------------------------------------------------------------------------------------+
| 1. LEVEL THRESHOLD FILTER CHECK                                                    |
|    Is DEBUG >= Logger's configured effective level (e.g., INFO)?                   |
|    --> NO: Evaluation short-circuits IMMEDIATELY (Zero string formatting!)         |
|    --> YES: Proceed to step 2                                                      |
+------------------------------------------------------------------------------------+
  |
  v (If enabled)
+------------------------------------------------------------------------------------+
| 2. PARAMETERIZED MESSAGE FORMATTING                                                |
|    Replaces '{}' with toString() representations of arguments                      |
|    Captures current thread name, timestamp, and SLF4J MDC Map                      |
|    Creates ILoggingEvent object                                                    |
+------------------------------------------------------------------------------------+
  |
  v
+------------------------------------------------------------------------------------+
| 3. ASYNC BUFFER DISPATCH (AsyncAppender)                                           |
|    Enqueues ILoggingEvent into lock-free RingBuffer (ArrayBlockingQueue)           |
|    Returns execution IMMEDIATELY to application thread (No disk I/O blocking!)     |
+------------------------------------------------------------------------------------+
  |
  v (Worker Daemon Thread)
+------------------------------------------------------------------------------------+
| 4. ENCODER & WRITER EXECUTION                                                      |
|    - StructuredJsonEncoder / PatternLayoutEncoder formats bytes                    |
|    - Writes bytes to stdout (ConsoleAppender) or rolling file                      |
+------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Configuration Reference

### Production `logback-spring.xml` Configuration

```xml
<?xml version="1.0" encoding="UTF-8"?>
<configuration scan="true" scanPeriod="30 seconds">

    <springProperty scope="context" name="appName" source="spring.application.name" defaultValue="spring-app"/>

    <!-- Console Appender -->
    <appender name="CONSOLE" class="ch.qos.logback.core.ConsoleAppender">
        <encoder class="ch.qos.logback.classic.encoder.PatternLayoutEncoder">
            <pattern>%d{yyyy-MM-dd'T'HH:mm:ss.SSSXXX} [%thread] %-5level %logger{36} [%X{traceId:-},%X{spanId:-}] - %msg%n</pattern>
        </encoder>
    </appender>

    <!-- Async Console Wrapper -->
    <appender name="ASYNC_CONSOLE" class="ch.qos.logback.classic.AsyncAppender">
        <appender-ref ref="CONSOLE"/>
        <queueSize>8192</queueSize>
        <discardingThreshold>0</discardingThreshold> <!-- Keep all INFO/WARN/ERROR logs -->
        <neverBlock>true</neverBlock>
        <includeCallerData>false</includeCallerData>
    </appender>

    <!-- Logger Level Hierarchy -->
    <logger name="org.springframework" level="INFO"/>
    <logger name="org.hibernate.SQL" level="WARN"/>
    <logger name="com.zaxxer.hikari" level="INFO"/>
    <logger name="com.example" level="INFO"/>

    <root level="INFO">
        <appender-ref ref="ASYNC_CONSOLE"/>
    </root>

</configuration>
```

---

## 5. Practical Examples

### Example 1: Parameterized Logging vs. Costly String Concatenation
```java
package com.example.observability.logging;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Service;

import java.util.List;

@Service
public class OrderAuditService {

    private static final Logger log = LoggerFactory.getLogger(OrderAuditService.class);

    public void processItems(String orderId, List<String> items) {
        // CORRECT: Parameterized logging. No string allocation occurs if DEBUG is disabled!
        log.debug("Processing order {} containing {} items", orderId, items.size());

        // CORRECT: Guarding expensive operations before passing to logger
        if (log.isDebugEnabled()) {
            log.debug("Detailed item payload for order {}: {}", orderId, serializeComplexPayload(items));
        }

        // CORRECT: Logging exceptions with stack traces (Pass Throwable as LAST parameter without '{}')
        try {
            validateItems(items);
        } catch (IllegalArgumentException ex) {
            log.error("Validation failed for order {}: {}", orderId, ex.getMessage(), ex);
        }
    }

    private String serializeComplexPayload(List<String> items) {
        // Expensive JSON transformation
        return String.join(",", items);
    }

    private void validateItems(List<String> items) {
        if (items.isEmpty()) throw new IllegalArgumentException("Items cannot be empty");
    }
}
```

### Example 2: Preventing Log Injection / CRLF Sanitization
```java
package com.example.observability.logging;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Service;

@Service
public class SecurityAuditService {

    private static final Logger log = LoggerFactory.getLogger(SecurityAuditService.class);

    public void recordLoginAttempt(String rawUsername, boolean success) {
        // Attackers may pass "admin\n2026-03-24 INFO - User admin granted root privileges"
        String sanitizedUsername = sanitizeForLogging(rawUsername);

        if (success) {
            log.info("Login success for user [{}]", sanitizedUsername);
        } else {
            log.warn("Login failure for user [{}]", sanitizedUsername);
        }
    }

    public static String sanitizeForLogging(String value) {
        if (value == null) return "null";
        // Replaces Carriage Return and Line Feed to prevent forged log entries
        return value.replace('\n', '_').replace('\r', '_').trim();
    }
}
```

### Example 3: Automatic PII & Secret Redaction Filter
```java
package com.example.observability.logging;

import ch.qos.logback.classic.pattern.MessageConverter;
import ch.qos.logback.classic.spi.ILoggingEvent;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class PiiRedactionConverter extends MessageConverter {

    // Regex matching Bearer tokens and passwords
    private static final Pattern SENSITIVE_PATTERN = 
        Pattern.compile("(?i)(password|secret|bearer\\s+token|authorization)\\s*[:=]\\s*[\"']?([^\\s\"',;]+)[\"']?");

    @Override
    public String convert(ILoggingEvent event) {
        String originalMessage = event.getFormattedMessage();
        if (originalMessage == null || originalMessage.isEmpty()) {
            return originalMessage;
        }

        Matcher matcher = SENSITIVE_PATTERN.matcher(originalMessage);
        if (matcher.find()) {
            return matcher.replaceAll("$1=***REDACTED***");
        }
        return originalMessage;
    }
}
```

### Example 4: Anti-Pattern vs. Production Fix (Catch-Log-Rethrow Anti-Pattern)
```java
// ANTI-PATTERN: "Log and Rethrow"
// Leads to duplicate log entries across every architectural layer, spamming logs with identical stack traces!
@Service
public class BadServiceLayer {
    private static final Logger log = LoggerFactory.getLogger(BadServiceLayer.class);

    public void executeStep() {
        try {
            performDatabaseWrite();
        } catch (Exception ex) {
            log.error("Database write failed!", ex); // LOG ENTRY 1
            throw ex; // Re-throws to controller, which logs it AGAIN (LOG ENTRY 2)!
        }
    }
}

// PRODUCTION FIX: Either handle and log, OR rethrow with context (NEVER BOTH)
@Service
public class CleanServiceLayer {

    public void executeStep() {
        try {
            performDatabaseWrite();
        } catch (DataAccessException ex) {
            // Re-throw wrapped in high-level domain exception. Let the @RestControllerAdvice log it ONCE!
            throw new OrderProcessingException("Failed to persist order step", ex);
        }
    }
}
```

### Example 5: Dynamic Log Level Adjustment via Spring Boot Actuator
```bash
# Production Incident Triage: Temporarily enable DEBUG on a specific package without restarting JVM!

# 1. View current log level for package
curl -X GET http://localhost:9090/actuator/loggers/com.example.billing

# Output:
# {"configuredLevel":"INFO","effectiveLevel":"INFO"}

# 2. Change log level to DEBUG dynamically
curl -X POST http://localhost:9090/actuator/loggers/com.example.billing \
     -H "Content-Type: application/json" \
     -d '{"configuredLevel":"DEBUG"}'

# 3. Triage complete: Reset log level back to INFO
curl -X POST http://localhost:9090/actuator/loggers/com.example.billing \
     -H "Content-Type: application/json" \
     -d '{"configuredLevel":"INFO"}'
```

---

## 6. Best Practices & Production Guidelines

1. **Always Use Parameterized Logging**: Never write `log.info("Order: " + id + " for " + user)`. Use `log.info("Order: {} for {}", id, user)`. String concatenation evaluates eagerly even when the log level is disabled.
2. **Log Exceptions Correctly**: Pass the `Throwable` as the final method argument without a placeholder: `log.error("Failed to process transaction {}: {}", txId, ex.getMessage(), ex)`.
3. **Never Log Secrets or PII**: Ensure credit cards, plaintext passwords, OAuth tokens, and SSNs are never logged. Use automated masking filters.
4. **Prevent Log and Rethrow**: Either handle the exception and log it, or propagate it up the stack. Do not log an error and then rethrow it to a caller who also logs it.
5. **Always Use AsyncAppender**: Wrap console and file appenders with Logback's `AsyncAppender` to prevent slow I/O operations from blocking request worker threads.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| String concatenation in log statements | Creates unnecessary `StringBuilder` object allocations even if log level is off. | Use SLF4J parameterized syntax (`{}`). |
| Catch, Log, and Rethrow | Causes duplicate identical stack traces across every layer of the architecture. | Log only at the architectural boundary (e.g., `@RestControllerAdvice`). |
| Omitting Throwable from `log.error()` | `log.error("Error: " + ex.getMessage())` discards the stack trace, making debugging impossible. | Pass the exception instance as the last parameter: `log.error("Error", ex)`. |
| Unsanitized user inputs in logs | Allows CRLF log injection attacks, spoofing audit logs. | Sanitize inputs by stripping or escaping `\r` and `\n`. |
| Setting root log level to DEBUG | Floods storage, overwhelms log forwarders, degrades application throughput by 40-70%. | Keep ROOT at `INFO`; selectively enable `DEBUG` via Actuator during incidents. |
| Forgetting to clear MDC | Servlet threads reuse pooled threads, causing correlation IDs to leak across different users. | Always invoke `MDC.clear()` in a `finally` block. |

---

## 8. Performance & Security Notes

### Benchmarking Parameterized vs. Concatenated Logging
- When `log.isDebugEnabled() == false`:
  - `log.debug("User " + user.getId() + " status " + user.getStatus())`: Incurs string building and object allocations on **every request**.
  - `log.debug("User {} status {}", user.getId(), user.getStatus())`: Incurs zero string concatenation; only checks an integer log level comparison ($\approx 2$ CPU cycles).

### OWASP Log Forgery (CRLF Injection)
- Attackers exploit unescaped log inputs to inject forged records into system logs. In compliance audits (SOC2, PCI-DSS), unescaped logging of user-supplied fields is considered a significant vulnerability.
- Always sanitize inputs or adopt structured JSON logging where strings are automatically escaped.

---

## 9. Comparison / When to Use What

| Log Level | Intended Audience | Frequency | Should Trigger Alert? |
|---|---|---|---|
| **TRACE** | Framework & Library Authors | Extreme (packet/byte level) | No |
| **DEBUG** | Developers during active triage | High (per-method diagnostics) | No |
| **INFO** | Operations & DevOps | Low (system milestones) | No |
| **WARN** | On-Call Engineers | Very Low (degradations/retries) | Maybe (if rate spikes) |
| **ERROR** | On-Call Engineers & SREs | Exceptional (failures/panics) | **Yes** (P1/P2 paging alert) |

---

## 10. Interview Questions & Answers

### Q1: Why is parameterized logging in SLF4J faster than string concatenation?
<details>
<summary>Click to view answer</summary>

String concatenation (`"Processing " + order.getId()`) evaluates the string concatenation eagerly before calling the method, allocating memory on the heap even if the logger's level is disabled (e.g., logging at DEBUG when root is INFO). Parameterized logging (`log.debug("Processing {}", order.getId())`) checks the log level first; if disabled, it exits immediately without formatting or allocating strings.
</details>

### Q2: What is the "Log and Rethrow" anti-pattern and why is it problematic?
<details>
<summary>Click to view answer</summary>

The anti-pattern occurs when a method catches an exception, logs it with `log.error()`, and then throws the exception (or a wrapped exception) to the caller. If each layer repeats this (Repository $\rightarrow$ Service $\rightarrow$ Controller), the exact same exception is logged 3 times with 3 huge stack traces. This clutters log files, inflates storage, and obscures the root cause.
</details>

### Q3: How do you dynamically change log levels in production without restarting the application?
<details>
<summary>Click to view answer</summary>

Using the Spring Boot Actuator `/actuator/loggers` endpoint. Sending an HTTP `POST` to `/actuator/loggers/{packageName}` with a JSON body `{"configuredLevel": "DEBUG"}` dynamically reconfigures Logback's logger hierarchy in memory without requiring a rolling restart.
</details>

### Q4: How does Logback's `AsyncAppender` work and what are its key configuration parameters?
<details>
<summary>Click to view answer</summary>

`AsyncAppender` decouples application threads from I/O writes using an in-memory `ArrayBlockingQueue`. Application threads put log events onto the queue and return immediately. A background worker thread drains the queue and writes events to the underlying appender. Key settings:
- `queueSize`: Buffer capacity (default 256; recommend 8192 for high load).
- `discardingThreshold`: Percentage of queue capacity below which TRACE/DEBUG/INFO logs are dropped if full (`0` disables dropping).
- `neverBlock`: If true, non-critical logs are discarded if the queue fills rather than blocking the caller.
</details>

### Q5: What is CRLF Log Injection and how do you protect against it?
<details>
<summary>Click to view answer</summary>

CRLF injection occurs when untrusted input containing Carriage Return (`\r`) and Line Feed (`\n`) characters is logged. The attacker injects new lines to forge fake log entries (e.g., mimicking audit logs). Protection: Sanitize all user inputs by replacing `\r` and `\n` with safe characters (e.g. `_`), or use structured JSON logging where newlines are automatically escaped as `\n`.
</details>

### Q6: Why should you avoid `includeCallerData: true` in production Logback configs?
<details>
<summary>Click to view answer</summary>

To determine the class name, method name, and line number where the log call occurred, the JVM must create a snapshot of the current thread's stack trace (`new Throwable().getStackTrace()`) and inspect the call frames. This is computationally expensive and can reduce application throughput by up to 5x.
</details>

### Q7: How do you log an exception so that its full stack trace is printed?
<details>
<summary>Click to view answer</summary>

Pass the exception as the last argument in the logging call **without** providing a `{}` placeholder for it:
```java
log.error("Failed to process transaction id {}: {}", txId, ex.getMessage(), ex);
```
SLF4J automatically detects that the final parameter is a `Throwable` and prints the stack trace beneath the formatted message.
</details>

### Q8: What is the purpose of SLF4J MDC?
<details>
<summary>Click to view answer</summary>

MDC (Mapped Diagnostic Context) allows developers to store contextual key-value pairs (like `userId`, `tenantId`, `traceId`) in a thread-local map. Once set, these values are automatically included in every subsequent log statement on that thread without needing to pass them manually into each log call.
</details>

---

## 11. Practice Exercises

### Exercise 1: Refactor Inefficient Logging Code (Easy)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Refactor the following method to use parameterized logging, clean exception logging, and prevent string concatenation overhead.

```java
// Original Bad Code
public void badLog(User user, Exception ex) {
    log.debug("User details: " + user.toString() + " at timestamp " + System.currentTimeMillis());
    log.error("Error occurred: " + ex.getMessage());
}
```

**Solution**:
```java
public void goodLog(User user, Exception ex) {
    if (log.isDebugEnabled()) {
        log.debug("User details: {} at timestamp {}", user, System.currentTimeMillis());
    }
    log.error("Operation failed for user {}: {}", user.getId(), ex.getMessage(), ex);
}
```
</details>

### Exercise 2: Create a Sensitive Data Log Masker (Medium)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Implement a utility method that masks email addresses in log messages, replacing `john.doe@example.com` with `j***e@example.com`.

**Solution**:
```java
package com.example.observability.logging;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class EmailLogMasker {

    private static final Pattern EMAIL_PATTERN = 
        Pattern.compile("\\b([a-zA-Z0-9])([a-zA-Z0-9._%+-]*)([a-zA-Z0-9])@([a-zA-Z0-9.-]+\\.[a-zA-Z]{2,})\\b");

    public static String maskEmails(String message) {
        if (message == null || message.isBlank()) return message;

        Matcher matcher = EMAIL_PATTERN.matcher(message);
        StringBuilder sb = new StringBuilder();
        while (matcher.find()) {
            String firstChar = matcher.group(1);
            String lastChar = matcher.group(3);
            String domain = matcher.group(4);
            matcher.appendReplacement(sb, firstChar + "***" + lastChar + "@" + domain);
        }
        matcher.appendTail(sb);
        return sb.toString();
    }
}
```
</details>

### Exercise 3: Complete Logback Async Appender Configuration (Hard)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Write a complete `logback-spring.xml` file configuring an asynchronous rolling file appender with 100MB max file size, 30-day retention, 3GB total cap, and non-blocking discard policy for DEBUG logs under pressure.

**Solution**:
```xml
<?xml version="1.0" encoding="UTF-8"?>
<configuration>

    <property name="LOG_PATH" value="/var/log/apps"/>

    <appender name="ROLLING_FILE" class="ch.qos.logback.core.rolling.RollingFileAppender">
        <file>${LOG_PATH}/application.log</file>
        <rollingPolicy class="ch.qos.logback.core.rolling.SizeAndTimeBasedRollingPolicy">
            <fileNamePattern>${LOG_PATH}/archived/app-%d{yyyy-MM-dd}.%i.log.gz</fileNamePattern>
            <maxFileSize>100MB</maxFileSize>
            <maxHistory>30</maxHistory>
            <totalSizeCap>3GB</totalSizeCap>
        </rollingPolicy>
        <encoder class="ch.qos.logback.classic.encoder.PatternLayoutEncoder">
            <pattern>%d{yyyy-MM-dd'T'HH:mm:ss.SSSXXX} [%thread] %-5level %logger{36} [%X{traceId:-}] - %msg%n</pattern>
        </encoder>
    </appender>

    <appender name="ASYNC_FILE" class="ch.qos.logback.classic.AsyncAppender">
        <appender-ref ref="ROLLING_FILE"/>
        <queueSize>16384</queueSize>
        <discardingThreshold>20</discardingThreshold> <!-- Discard TRACE/DEBUG when 80% full -->
        <neverBlock>true</neverBlock>
        <includeCallerData>false</includeCallerData>
    </appender>

    <root level="INFO">
        <appender-ref ref="ASYNC_FILE"/>
    </root>

</configuration>
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------+-------------------------------------------------------------+
| Practice / Principle                        | Description & Implementation Rule                           |
+---------------------------------------------+-------------------------------------------------------------+
| Parameterized Logging                       | Always log with {} placeholders; never string concatenate   |
| Exception Logging                           | Pass Throwable as last param without placeholder: log(m, e)|
| Log and Rethrow Anti-Pattern                | Either log or rethrow; never both                           |
| AsyncAppender                               | Always use AsyncAppender in production to isolate I/O       |
| Dynamic Loggers                             | Adjust levels at runtime via /actuator/loggers              |
| CRLF Sanitization                           | Strip \r and \n from untrusted input before logging         |
| MDC Hygiene                                 | Always clear MDC in finally blocks on pooled worker threads |
+---------------------------------------------+-------------------------------------------------------------+
```

---

## 13. Further Reading
- SLF4J User Manual and FAQ
- Logback Architecture Documentation
- OWASP Log Injection Prevention Cheat Sheet
- The Twelve-Factor App: Logs as Event Streams