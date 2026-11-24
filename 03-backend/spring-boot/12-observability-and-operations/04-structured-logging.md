# Structured Logging in Spring Boot

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
In traditional Java applications, logs were formatted as human-readable plaintext strings (e.g., `2026-03-24 10:15:30 [main] INFO OrderService - Order 9821 processed`). In production container platforms (Kubernetes, AWS ECS) shipping gigabytes of logs per minute into search engines like Elasticsearch, OpenSearch, Grafana Loki, or AWS CloudWatch:
- Plaintext logs require complex, fragile regex parsers (Grok filters) to extract fields like timestamps, thread names, log levels, and custom business keys.
- Multi-line stack traces break into dozens of individual log lines, destroying log context.
- Parsing overhead in log forwarders (Fluentbit, Logstash, Vector) consumes significant CPU and introduces ingestion lag.

### Historical Evolution
Historically, Spring developers had to pull in third-party libraries such as `logstash-logback-encoder` and maintain lengthy, verbose `logback-spring.xml` files with complex XML encoders just to output valid JSON.

**Spring Boot 3.4 introduced Native Structured Logging**. With Boot 3.4+, developers can emit standardized JSON logs directly out of the box using simple configuration properties, with built-in support for:
- **ECS** (Elastic Common Schema)
- **Logstash**
- **GELF** (Graylog Extended Log Format)

### Production Relevance
Native structured logging provides:
- Machine-parseable, single-line JSON log events by default.
- Seamless correlation with OpenTelemetry (`traceId`, `spanId`) and SLF4J MDC fields.
- Zero external XML configuration needed for 95% of enterprise production use cases.

---

## 2. Core Concepts

### Architectural Foundations
1. **Structured Log Event**: A log message represented as a key-value JSON document with standard schema fields:
   - `@timestamp`: ISO-8601 formatted event time.
   - `log.level` / `level`: Standard severity (INFO, WARN, ERROR).
   - `message`: The primary human-readable log message.
   - `service.name`: Identifying service name.
   - `trace.id` & `span.id`: Distributed tracing context.
2. **Built-in Formats (Boot 3.4+)**:
   - `ecs`: Elastic Common Schema (ideal for Elasticsearch / Kibana).
   - `logstash`: Standard Logstash JSON encoder layout.
   - `gelf`: Graylog JSON format with compressed keys.
3. **MDC (Mapped Diagnostic Context)**: Thread-local key-value map maintained by SLF4J, automatically merged into the structured JSON root object.
4. **StructuredLoggingFormatter**: The SPI interface in Spring Boot 3.4 allowing custom schema serialization without touching Logback internals.

---

## 3. How It Works Internally

### Native Structured Logging Pipeline (Spring Boot 3.4+)

```
+------------------------------------------------------------------------------------+
|                               APPLICATION CODE                                    |
|         log.info("Order processed successfully for customer {}", customerId);      |
+------------------------------------------------------------------------------------+
                                         |
                                         v
+------------------------------------------------------------------------------------+
|                         SLF4J LOGGING API (MDC CONTEXT)                            |
|             MDC.put("orderId", "9821");  MDC.put("tenantId", "corp-1");            |
|             TraceContext injected traceId="4bf9..." & spanId="00f0..."             |
+------------------------------------------------------------------------------------+
                                         |
                                         v
+------------------------------------------------------------------------------------+
|                       LOGBACK ILoggingEvent (IN-MEMORY)                            |
|  - Timestamp, Level, ThreadName, LoggerName, FormattedMessage, StackTrace, MDC Map |
+------------------------------------------------------------------------------------+
                                         |
                                         v
+------------------------------------------------------------------------------------+
|                 SPRING BOOT 3.4 StructuredLogFormatter (ECS / Logstash)            |
|  Maps ILoggingEvent -> JSON Document:                                              |
|  {                                                                                 |
|    "@timestamp": "2026-03-24T10:15:30.123Z",                                       |
|    "log.level": "INFO",                                                            |
|    "message": "Order processed successfully for customer C-12",                    |
|    "service.name": "order-service",                                                |
|    "trace.id": "4bf92f3577b34da6a3ce929d0e0e4736",                                 |
|    "span.id": "00f067aa0ba902b7",                                                  |
|    "mdc": { "orderId": "9821", "tenantId": "corp-1" }                              |
|  }                                                                                 |
+------------------------------------------------------------------------------------+
                                         |
                                         v
+------------------------------------------------------------------------------------+
|                       STANDARD OUT (stdout) / CONTAINER CONSOLE                    |
|       Scraped by Kubernetes daemon (Vector, FluentBit) -> OpenSearch / Loki        |
+------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Configuration Reference

### Native Configuration (`application.yml`) in Spring Boot 3.4+

```yaml
spring:
  application:
    name: order-service

logging:
  structured:
    format:
      console: ecs # Options: ecs, logstash, gelf, or custom class name
      file: logstash
    ecs:
      service:
        name: ${spring.application.name}
        version: "1.4.2"
        environment: "production"
  level:
    root: INFO
    com.example.orders: DEBUG
  include-application-name: true
```

### Traditional Logstash Logback XML (For Boot 3.0–3.3 or Legacy Migration)
```xml
<!-- src/main/resources/logback-spring.xml -->
<configuration>
    <appender name="JSON_CONSOLE" class="ch.qos.logback.core.ConsoleAppender">
        <encoder class="net.logstash.logback.encoder.LogstashEncoder">
            <includeMdcKeyName>traceId</includeMdcKeyName>
            <includeMdcKeyName>spanId</includeMdcKeyName>
            <includeMdcKeyName>tenantId</includeMdcKeyName>
            <customFields>{"app":"order-service","env":"production"}</customFields>
        </encoder>
    </appender>
    <root level="INFO">
        <appender-ref ref="JSON_CONSOLE"/>
    </root>
</configuration>
```

---

## 5. Practical Examples

### Example 1: Enriching Logs with MDC Filter
```java
package com.example.observability.logging;

import jakarta.servlet.FilterChain;
import jakarta.servlet.ServletException;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.servlet.http.HttpServletResponse;
import org.slf4j.MDC;
import org.springframework.core.Ordered;
import org.springframework.core.annotation.Order;
import org.springframework.stereotype.Component;
import org.springframework.web.filter.OncePerRequestFilter;

import java.io.IOException;
import java.util.UUID;

@Component
@Order(Ordered.HIGHEST_PRECEDENCE)
public class RequestCorrelationMdcFilter extends OncePerRequestFilter {

    private static final String CORRELATION_HEADER = "X-Correlation-ID";
    private static final String TENANT_HEADER = "X-Tenant-ID";

    @Override
    protected void doFilterInternal(HttpServletRequest request,
                                    HttpServletResponse response,
                                    FilterChain filterChain) throws ServletException, IOException {
        String correlationId = request.getHeader(CORRELATION_HEADER);
        if (correlationId == null || correlationId.isBlank()) {
            correlationId = UUID.randomUUID().toString();
        }

        String tenantId = request.getHeader(TENANT_HEADER);
        if (tenantId == null || tenantId.isBlank()) {
            tenantId = "anonymous";
        }

        try {
            MDC.put("correlationId", correlationId);
            MDC.put("tenantId", tenantId);
            MDC.put("clientIp", request.getRemoteAddr());
            MDC.put("httpMethod", request.getMethod());
            MDC.put("requestUri", request.getRequestURI());

            response.setHeader(CORRELATION_HEADER, correlationId);
            filterChain.doFilter(request, response);
        } finally {
            // CRITICAL: Always clean up MDC on pooled threads to prevent context leakage!
            MDC.clear();
        }
    }
}
```

### Example 2: Emitting Custom Structured Arguments with SLF4J 2 Fluent API
```java
package com.example.observability.logging;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Service;

import java.math.BigDecimal;

@Service
public class CheckoutService {

    private static final Logger log = LoggerFactory.getLogger(CheckoutService.class);

    public void completeCheckout(String orderId, String customerId, BigDecimal totalAmount) {
        // SLF4J 2 Fluent Logging API natively adds key-values to structured JSON outputs!
        log.atInfo()
            .setMessage("Order payment finalized successfully")
            .addKeyValue("order_id", orderId)
            .addKeyValue("customer_id", customerId)
            .addKeyValue("total_amount", totalAmount)
            .addKeyValue("currency", "USD")
            .log();
    }
}
```

### Example 3: Custom Spring Boot 3.4 `StructuredLogFormatter`
```java
package com.example.observability.logging;

import ch.qos.logback.classic.spi.ILoggingEvent;
import org.springframework.boot.logging.structured.StructuredLogFormatter;

import java.time.Instant;
import java.util.LinkedHashMap;
import java.util.Map;

public class CustomCloudWatchStructuredFormatter implements StructuredLogFormatter<ILoggingEvent> {

    @Override
    public String format(ILoggingEvent event) {
        Map<String, Object> json = new LinkedHashMap<>();
        json.put("timestamp", Instant.ofEpochMilli(event.getTimeStamp()).toString());
        json.put("severity", event.getLevel().toString());
        json.put("logger", event.getLoggerName());
        json.put("thread", event.getThreadName());
        json.put("message", event.getFormattedMessage());

        if (event.getMDCPropertyMap() != null && !event.getMDCPropertyMap().isEmpty()) {
            json.put("context", event.getMDCPropertyMap());
        }

        if (event.getThrowableProxy() != null) {
            json.put("exception_class", event.getThrowableProxy().getClassName());
            json.put("exception_message", event.getThrowableProxy().getMessage());
        }

        // Minimal JSON serializer output
        return toJsonString(json) + "\n";
    }

    private String toJsonString(Map<String, Object> map) {
        // Standard JSON serialization logic
        StringBuilder sb = new StringBuilder("{");
        map.forEach((k, v) -> sb.append("\"").append(k).append("\":\"").append(v).append("\","));
        if (sb.length() > 1) sb.deleteCharAt(sb.length() - 1);
        sb.append("}");
        return sb.toString();
    }
}
```

### Example 4: Anti-Pattern vs. Production Fix (MDC Leakage Across Async Boundaries)
```java
// ANTI-PATTERN: MDC is ThreadLocal. Calling @Async or CompletableFuture strips MDC context!
@Service
public class BadNotificationWorker {
    private static final Logger log = LoggerFactory.getLogger(BadNotificationWorker.class);

    public void sendEmail(String to) {
        // MDC has "correlationId" here on the web thread
        CompletableFuture.runAsync(() -> {
            // Worker thread! MDC IS EMPTY! correlationId is lost in logs!
            log.info("Sending email to {}", to);
        });
    }
}

// PRODUCTION FIX: TaskDecorator to copy MDC to async execution threads
@Configuration
public class AsyncMdcConfiguration {

    @Bean
    public TaskDecorator mdcCopyingTaskDecorator() {
        return runnable -> {
            Map<String, String> contextMap = MDC.getCopyOfContextMap();
            return () -> {
                try {
                    if (contextMap != null) {
                        MDC.setContextMap(contextMap);
                    }
                    runnable.run();
                } finally {
                    MDC.clear();
                }
            };
        };
    }

    @Bean
    public ThreadPoolTaskExecutor taskExecutor(TaskDecorator mdcCopyingTaskDecorator) {
        ThreadPoolTaskExecutor executor = new ThreadPoolTaskExecutor();
        executor.setCorePoolSize(8);
        executor.setTaskDecorator(mdcCopyingTaskDecorator);
        executor.initialize();
        return executor;
    }
}
```

### Example 5: High-Throughput Async Logback Appender Configuration
```xml
<!-- src/main/resources/logback-spring.xml -->
<configuration>
    <appender name="CONSOLE" class="ch.qos.logback.core.ConsoleAppender">
        <encoder class="ch.qos.logback.classic.encoder.PatternLayoutEncoder">
            <pattern>%msg%n</pattern>
        </encoder>
    </appender>

    <!-- AsyncAppender prevents disk/stdout bottlenecks from blocking request threads -->
    <appender name="ASYNC_CONSOLE" class="ch.qos.logback.classic.AsyncAppender">
        <appender-ref ref="CONSOLE"/>
        <queueSize>10000</queueSize>
        <discardingThreshold>0</discardingThreshold> <!-- Never discard INFO/WARN logs -->
        <neverBlock>true</neverBlock> <!-- Drop trace/debug if ring buffer is full -->
        <includeCallerData>false</includeCallerData> <!-- Avoid expensive stack frame inspection -->
    </appender>

    <root level="INFO">
        <appender-ref ref="ASYNC_CONSOLE"/>
    </root>
</configuration>
```

---

## 6. Best Practices & Production Guidelines

1. **Log to Stdout in Containers**: In Kubernetes and Docker, always log directly to `console` (stdout). Never write logs to container local disks; let the cluster daemon (Fluentbit/Vector) ship stdout to central storage.
2. **Standardize on ECS (Elastic Common Schema)**: ECS provides an industry-standard naming schema (`@timestamp`, `log.level`, `service.name`, `trace.id`), allowing unified log searching across heterogeneous polyglot services.
3. **Always Clear MDC in `finally` Blocks**: Servlet containers reuse worker threads. Failing to clear MDC will contaminate subsequent unrelated requests with old correlation IDs and user identifiers.
4. **Use AsyncAppender for Production**: Synchronous stdout writing can block application threads under heavy I/O contention. An `AsyncAppender` with a non-blocking queue isolates application execution from logging latencies.
5. **Never Include Caller Data in Production**: Setting `includeCallerData=true` forces the JVM to take a stack snapshot for every single log event to resolve the class and line number, incurring severe CPU penalties.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| String concatenation inside log statements | Allocates strings even if the log level (e.g. DEBUG) is disabled. | Use parameterized logging: `log.debug("Value: {}", val)`. |
| Losing MDC on `@Async` methods | `ThreadLocal` context does not cross thread pool boundaries. | Configure a `TaskDecorator` to copy MDC to child threads. |
| Forgetting `MDC.clear()` in servlet filters | Reused thread pool threads inherit previous users' session IDs and customer IDs. | Place `MDC.clear()` inside the `finally` block of your filter. |
| Multi-line logs in Kubernetes | Unstructured stack traces get split across separate lines by container runtimes. | Enable structured JSON logging (`logging.structured.format.console=ecs`). |
| Logging sensitive PII (credit cards, passwords) | Plaintext secrets are ingested into central log stores, violating PCI-DSS and GDPR. | Use log masking converters or SLF4J ValueMasking encoders. |
| `includeCallerData: true` in high throughput | Up to 10x slower execution due to JVM stack trace traversal on every log call. | Keep `includeCallerData: false` in production configurations. |

---

## 8. Performance & Security Notes

### CPU & Allocation Overhead
- JSON serialization involves object allocations for keys and values. Spring Boot 3.4's built-in formatters are heavily optimized to reuse byte buffers and avoid intermediate Map allocations where possible.
- In benchmarks, structured logging with `AsyncAppender` achieves $> 300,000$ events per second per core with $< 1\text{ms}$ latency impact.

### Log Injection (OWASP Top 10)
- Attackers can inject CRLF characters (`\r\n`) into unvalidated user inputs (e.g., username), forging fake log entries or splitting lines in plaintext logs.
- Structured JSON logging completely mitigates log injection because newlines and control characters are escaped into JSON strings (`\n`) within the message field.

---

## 9. Comparison / When to Use What

| Feature | Plaintext Console Pattern | Spring Boot 3.4 Native Structured (ECS) | Third-Party logstash-logback-encoder |
|---|---|---|---|
| **Human Readability (Terminal)** | Excellent | Poor (Raw JSON string) | Poor (Raw JSON string) |
| **Elasticsearch / Loki Ingestion** | Requires Grok regex parsing | Instant ingestion (zero parsing) | Instant ingestion (zero parsing) |
| **Configuration Overhead** | Simple pattern string | 1 line: `logging.structured.format.console: ecs` | Verbose 50-line `logback-spring.xml` |
| **Trace Correlation (OTel)** | Manual pattern tokens | Automatic (`trace.id`, `span.id`) | Requires custom provider config |
| **Maintenance** | Built-in | Built-in Spring Boot standard | External third-party dependency |

---

## 10. Interview Questions & Answers

### Q1: What is Spring Boot 3.4's Native Structured Logging feature?
<details>
<summary>Click to view answer</summary>

Introduced in Spring Boot 3.4, Native Structured Logging enables applications to output production-grade JSON logs without requiring external libraries (like `logstash-logback-encoder`) or complex XML configuration. By setting `logging.structured.format.console: ecs` (or `logstash`/`gelf`), Spring Boot automatically configures Logback to emit machine-readable JSON containing timestamps, log levels, service metadata, and MDC trace IDs.
</details>

### Q2: How does SLF4J MDC work under the hood?
<details>
<summary>Click to view answer</summary>

MDC (Mapped Diagnostic Context) is backed by a `ThreadLocal<Map<String, String>>`. When `MDC.put(key, value)` is called, the entry is stored in the current thread's thread-local map. Whenever a log event is created on that thread, Logback reads the thread's MDC map and attaches all key-value pairs to the log event payload.
</details>

### Q3: Why is structured logging superior to plaintext logging in containerized environments?
<details>
<summary>Click to view answer</summary>

1. Eliminates complex, brittle Grok regex parsing in log shippers (Fluentbit/Logstash).
2. Prevents multi-line stack traces from being fragmented into disconnected log entries.
3. Automatically embeds dimensional metadata (`service.name`, `trace.id`, `tenant.id`) as native JSON attributes that search indexes can query and filter with high performance.
</details>

### Q4: How do you prevent thread leakage when using MDC in Spring Boot web applications?
<details>
<summary>Click to view answer</summary>

Because web servers (Tomcat, Jetty) maintain a thread pool and reuse worker threads across multiple HTTP requests, any data left in MDC will leak into the next request handled by that thread. You must always invoke `MDC.clear()` inside a `finally` block within a servlet `Filter` or Spring `HandlerInterceptor`.
</details>

### Q5: What is the difference between SLF4J's `addKeyValue()` and MDC?
<details>
<summary>Click to view answer</summary>

- `MDC.put()` binds key-values to the entire thread lifetime until explicitly removed. Every subsequent log statement on that thread will include those MDC fields.
- `log.atInfo().addKeyValue("key", "val").log()` binds the key-value **exclusively to that single log event**, without polluting the thread-local state.
</details>

### Q6: How does Logback's `AsyncAppender` improve application throughput?
<details>
<summary>Click to view answer</summary>

Standard appenders write to console or disk synchronously on the executing application thread. Under heavy traffic or slow I/O, application threads block waiting for writes to finish. `AsyncAppender` places log events onto a lock-free memory ring buffer (`ArrayBlockingQueue`) and delegates actual writing to a background worker daemon thread, decoupling application latency from I/O latency.
</details>

### Q7: How does structured logging prevent Log Injection vulnerabilities?
<details>
<summary>Click to view answer</summary>

In plaintext logs, if user input contains CRLF (`\r\n`), it creates a new line in the log file, allowing an attacker to forge arbitrary log statements (e.g., fake "ADMIN LOGIN SUCCESS"). In structured logging, the JSON encoder escapes `\r` and `\n` into literal `\r\n` characters within the JSON `"message"` string field, ensuring the log event remains a single valid JSON document.
</details>

### Q8: What is Elastic Common Schema (ECS) and why is it popular?
<details>
<summary>Click to view answer</summary>

ECS is an open-source specification created by Elastic that standardizes field names across diverse logging, metric, and security sources. Standard fields like `@timestamp`, `log.level`, `service.name`, `host.hostname`, and `error.stack_trace` allow centralized SIEM and analytics dashboards to query across polyglot microservices without needing custom field mapping per application.
</details>

---

## 11. Practice Exercises

### Exercise 1: Configure Native ECS Console Logging in Spring Boot 3.4 (Easy)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Write an `application.yml` file configuring native ECS structured console logging, including service name and environment.

**Solution**:
```yaml
spring:
  application:
    name: billing-service

logging:
  structured:
    format:
      console: ecs
    ecs:
      service:
        name: ${spring.application.name}
        version: "2.1.0"
        environment: "production"
  level:
    root: INFO
```
</details>

### Exercise 2: Create a Request Timing Logging Interceptor (Medium)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Implement a Spring `HandlerInterceptor` that captures HTTP request execution time and logs a structured event with `http_status`, `duration_ms`, and `uri`.

**Solution**:
```java
package com.example.observability.logging;

import jakarta.servlet.http.HttpServletRequest;
import jakarta.servlet.http.HttpServletResponse;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Component;
import org.springframework.web.servlet.HandlerInterceptor;

@Component
public class RequestTimingInterceptor implements HandlerInterceptor {

    private static final Logger log = LoggerFactory.getLogger(RequestTimingInterceptor.class);
    private static final String START_TIME_ATTR = "request_start_time";

    @Override
    public boolean preHandle(HttpServletRequest request, HttpServletResponse response, Object handler) {
        request.setAttribute(START_TIME_ATTR, System.currentTimeMillis());
        return true;
    }

    @Override
    public void afterCompletion(HttpServletRequest request, HttpServletResponse response, Object handler, Exception ex) {
        Long startTime = (Long) request.getAttribute(START_TIME_ATTR);
        if (startTime != null) {
            long duration = System.currentTimeMillis() - startTime;
            log.atInfo()
                .setMessage("HTTP Request Completed")
                .addKeyValue("http_method", request.getMethod())
                .addKeyValue("http_uri", request.getRequestURI())
                .addKeyValue("http_status", response.getStatus())
                .addKeyValue("duration_ms", duration)
                .log();
        }
    }
}
```
</details>

### Exercise 3: Implement Custom PII Masking Pattern Layout (Hard)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Create a custom Logback `CompositeConverter<ILoggingEvent>` that detects 16-digit credit card numbers in log messages and replaces all but the last 4 digits with `****-****-****-1234`.

**Solution**:
```java
package com.example.observability.logging;

import ch.qos.logback.classic.pattern.MessageConverter;
import ch.qos.logback.classic.spi.ILoggingEvent;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class CreditCardMaskingConverter extends MessageConverter {

    private static final Pattern CC_PATTERN = 
        Pattern.compile("\\b(?:\\d{4}[- ]?){3}(\\d{4})\\b");

    @Override
    public String convert(ILoggingEvent event) {
        String message = event.getFormattedMessage();
        if (message == null || message.isEmpty()) {
            return message;
        }

        Matcher matcher = CC_PATTERN.matcher(message);
        StringBuilder sb = new StringBuilder();
        while (matcher.find()) {
            String lastFour = matcher.group(1);
            matcher.appendReplacement(sb, "****-****-****-" + lastFour);
        }
        matcher.appendTail(sb);
        return sb.toString();
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------+-------------------------------------------------------------+
| Property / Technique                        | Description / Best Practice                                 |
+---------------------------------------------+-------------------------------------------------------------+
| logging.structured.format.console: ecs      | Spring Boot 3.4+ native Elastic Common Schema JSON output   |
| logging.structured.format.console: logstash | Native Logstash JSON layout format                          |
| MDC.put(key, val) / MDC.clear()             | SLF4J thread-local context; always clear in finally blocks  |
| log.atInfo().addKeyValue(k, v).log()        | SLF4J 2 Fluent API for per-message structured attributes    |
| AsyncAppender                               | Isolates stdout writes from application execution threads   |
| TaskDecorator                               | Bridges MDC context across @Async and executor boundaries   |
+---------------------------------------------+-------------------------------------------------------------+
```

---

## 13. Further Reading
- Spring Boot 3.4 Release Notes: Structured Logging
- Elastic Common Schema (ECS) Specification
- SLF4J 2 Fluent Logging API Reference
- The Twelve-Factor App: Principle XI (Treat logs as event streams)