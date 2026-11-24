# Distributed Tracing with Micrometer Tracing and OpenTelemetry

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x |
| **Java Version** | 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | `micrometer-tracing-bridge-otel:1.4.x`, `opentelemetry-exporter-otlp`, W3C Trace Context |

---

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Usage](#4-syntax--usage)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices](#6-best-practices)
- [7. Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
- [8. Performance & Security Notes](#8-performance--security-notes)
- [9. Comparison / When to Use What](#9-comparison--when-to-use-what)
- [10. Interview Questions](#10-interview-questions)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading](#13-further-reading)

---

## 1. What & Why

### The Problem
In monolithic systems, troubleshooting a user error or latency bottleneck is straightforward: developers inspect a single log file on a single server, following sequential thread execution logs. In distributed microservice architectures, a single user click (e.g., placing an order) fans out across dozens of independent services over HTTP and Kafka. If an order fails or takes 4 seconds, traditional isolated log files are useless—engineers have no way to correlate which specific log lines across 15 separate services belong to that specific customer's request.

### The Evolution
Distributed tracing solves this by tagging every request with a globally unique **Trace ID** that travels across network hops. Historically, Spring Cloud Sleuth provided tracing in Spring Boot 2.x. In **Spring Boot 3, Spring Cloud Sleuth was retired** and its core functionality was migrated directly into **Micrometer Tracing** (part of the core Spring observability portfolio). Micrometer Tracing serves as a vendor-neutral abstraction supporting two primary tracer bridges: **OpenTelemetry (OTel)** and **OpenZipkin Brave**, natively adopting the standard **W3C Trace Context (`traceparent`)** specification.

### Relevance in Modern Engineering
Distributed tracing is mandatory for production microservices. Micrometer Tracing automatically instruments Spring MVC controllers, `RestClient`, `WebClient`, `@KafkaListener`, and Spring Data repositories, streaming spans via OTLP (OpenTelemetry Protocol) to backends like Jaeger, Zipkin, Tempo, or Datadog.

---

## 2. Core Concepts

- **Trace**: The end-to-end representation of a single request journey across a distributed system, identified by a globally unique 128-bit hex string (`traceId`).
- **Span**: A single discrete unit of work within a trace (e.g., an HTTP handler execution, an SQL query, or a Kafka publish), identified by a `spanId`.
- **W3C Trace Context (`traceparent`)**: Standard HTTP header format:
  `version-trace_id-parent_span_id-trace_flags`
  (e.g., `00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01`).
- **Baggage**: Key-value metadata pairs propagated alongside the trace context across all downstream services (e.g., `tenant-id`, `user-id`).
- **Sampling Probability**: Configurable percentage of traces exported to backends (e.g., 10% sampling in high-traffic production to prevent storage saturation).
- **Tracer & ObservationRegistry**: Core Spring Boot 3 abstractions for programmatically creating, managing, and annotating spans.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------+
|                        W3C Trace Context Propagation Flow                         |
|                                                                                   |
| 1. Client HTTP Request Enters Order Service                                       |
|    POST /api/v1/orders                                                            |
|    (If no traceparent header exists, Micrometer Tracing generates Trace ID: T1)   |
|                   |                                                               |
|                   v Current Span: S1 (OrderController)                            |
| 2. Order Service Logs with MDC Injection                                          |
|    SLF4J Log Format: [order-service, T1, S1] "Persisting order 101"              |
|                   |                                                               |
|                   v                                                               |
| 3. Outbound HTTP Call via RestClient to Payment Service                           |
|    Micrometer Tracing Interceptor automatically injects W3C Header:               |
|    traceparent: 00-T1-S1-01                                                       |
|                   |                                                               |
|                   v Network Hop over HTTP                                         |
| 4. Payment Service Receives Request                                               |
|    Extracts traceparent header: Keeps Trace ID = T1, Creates Child Span: S2       |
|    SLF4J Log Format: [payment-service, T1, S2] "Processing charge for order 101"  |
|                   |                                                               |
|                   v                                                               |
| 5. OTLP Exporter Streams Spans Asynchronously                                     |
|    Spans [S1, S2] sent via gRPC / HTTP to OpenTelemetry Collector / Jaeger        |
+-----------------------------------------------------------------------------------+
```

When an HTTP request enters an application, Spring Boot's `ObservationHandler` extracts the W3C `traceparent` header. If absent, it generates a new 128-bit `traceId` and an initial root `spanId`. Micrometer Tracing automatically injects `traceId` and `spanId` into SLF4J's Mapped Diagnostic Context (MDC), ensuring that every log statement emitted by the thread prints the trace identifiers. When the application issues an outbound call via an instrumented `RestClient` or Kafka producer, tracing interceptors inject the current trace context into the outbound headers.

---

## 4. Syntax & Usage

### Maven Dependencies (Spring Boot 3 + OpenTelemetry Bridge)
```xml
<!-- Core Actuator -->
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-starter-actuator</artifactId>
</dependency>
<!-- Micrometer Tracing Bridge to OpenTelemetry -->
<dependency>
    <groupId>io.micrometer</groupId>
    <artifactId>micrometer-tracing-bridge-otel</artifactId>
</dependency>
<!-- OpenTelemetry Protocol (OTLP) Exporter -->
<dependency>
    <groupId>io.opentelemetry</groupId>
    <artifactId>opentelemetry-exporter-otlp</artifactId>
</dependency>
```

### Essential Application Configuration (`application.yml`)
```yaml
management:
  tracing:
    sampling:
      probability: 1.0 # 100% sampling for dev; set to 0.1 (10%) for high-volume prod
    propagation:
      type: W3C # Standard W3C Trace Context (traceparent)
  otlp:
    tracing:
      endpoint: http://otel-collector:4318/v1/traces # OTLP HTTP receiver

logging:
  pattern:
    level: "%5p [${spring.application.name:},%X{traceId:-},%X{spanId:-}]"
```

---

## 5. Practical Examples

### Example 1: Instrumenting `RestClient` for Automatic Header Propagation
```java
package com.example.platform.tracing.config;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.web.client.RestClient;

@Configuration
public class TracingClientConfig {

    // Injecting RestClient.Builder ensures Micrometer Tracing automatically attaches
    // the TracingClientHttpRequestInterceptor for W3C header propagation
    @Bean
    public RestClient restClient(RestClient.Builder builder) {
        return builder.build();
    }
}
```

### Example 2: Programmatic Custom Spans using `Tracer`
```java
package com.example.platform.tracing.service;

import io.micrometer.tracing.Span;
import io.micrometer.tracing.Tracer;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Service;

@Service
public class FraudEvaluationService {

    private static final Logger log = LoggerFactory.getLogger(FraudEvaluationService.class);
    private final Tracer tracer;

    public FraudEvaluationService(Tracer tracer) {
        this.tracer = tracer;
    }

    public boolean evaluateFraudRisk(String customerId, double amount) {
        // Create custom child span around heavy computation
        Span customSpan = tracer.nextSpan().name("fraud-risk-algorithm").start();

        try (Tracer.SpanInScope ws = tracer.withSpan(customSpan.start())) {
            customSpan.tag("customer.id", customerId);
            customSpan.tag("transaction.amount", String.valueOf(amount));

            log.info("Executing algorithmic fraud scoring in custom span");

            // Perform complex risk calculation
            boolean isHighRisk = amount > 10_000;
            customSpan.event(isHighRisk ? "FLAGGED_RISK" : "CLEARED");

            return isHighRisk;
        } catch (Exception e) {
            customSpan.error(e);
            throw e;
        } finally {
            customSpan.end(); // Always terminate span in finally block
        }
    }
}
```

### Example 3: Legacy Spring Cloud Sleuth Anti-Pattern and Fix
```xml
<!-- ANTI-PATTERN: Attempting to use spring-cloud-starter-sleuth in Spring Boot 3!
     Sleuth is completely INCOMPATIBLE with Spring Boot 3 and will fail on startup! -->

<!-- FIX: Replace with micrometer-tracing-bridge-otel and opentelemetry-exporter-otlp in pom.xml -->
<dependencies>
    <dependency>
        <groupId>org.springframework.boot</groupId>
        <artifactId>spring-boot-starter-actuator</artifactId>
    </dependency>
    <dependency>
        <groupId>io.micrometer</groupId>
        <artifactId>micrometer-tracing-bridge-otel</artifactId>
    </dependency>
</dependencies>
```

### Example 4: Propagating Custom Baggage Fields (e.g. `tenant-id`)
```java
package com.example.platform.tracing.config;

import io.micrometer.tracing.BaggageManager;
import io.micrometer.tracing.BaggageInScope;
import org.springframework.stereotype.Service;

@Service
public class TenantBaggageService {

    private final BaggageManager baggageManager;

    public TenantBaggageService(BaggageManager baggageManager) {
        this.baggageManager = baggageManager;
    }

    public void attachTenantContext(String tenantId, Runnable task) {
        // Injects tenant-id into trace baggage; automatically propagated in HTTP headers
        try (BaggageInScope baggage = baggageManager.createBaggageInScope("tenant-id", tenantId)) {
            task.run();
        }
    }
}
```

```yaml
# application.yml: Allow 'tenant-id' to be propagated over network headers
management:
  tracing:
    baggage:
      remote-fields:
        - tenant-id
      correlation:
        fields:
          - tenant-id
```

### Example 5: High-Performance Observation API (`ObservationRegistry`)
```java
package com.example.platform.tracing.service;

import io.micrometer.observation.Observation;
import io.micrometer.observation.ObservationRegistry;
import org.springframework.stereotype.Service;

@Service
public class OrderCalculationService {

    private final ObservationRegistry observationRegistry;

    public OrderCalculationService(ObservationRegistry observationRegistry) {
        this.observationRegistry = observationRegistry;
    }

    public double calculateOrderTotal(String orderId) {
        // High-level Spring 6 Observation API: handles tracing span AND metrics timer simultaneously!
        return Observation.createNotStarted("order.calculation", observationRegistry)
                .lowCardinalityKeyValue("order.type", "ONLINE")
                .highCardinalityKeyValue("order.id", orderId)
                .observe(() -> {
                    // Business logic
                    return 149.95;
                });
    }
}
```

---

## 6. Best Practices

1. **Adopt W3C Trace Context Standard**: Use `management.tracing.propagation.type=W3C`; avoid proprietary or deprecated B3 multi-header formats unless integrating with legacy services.
2. **Always Inject `RestClient.Builder`**: Never instantiate `new RestClient()` manually; always use the Spring-managed `RestClient.Builder` so tracing interceptors are attached automatically.
3. **Use Conservative Sampling in Production**: Setting `probability: 1.0` in high-throughput production ($100k\text{ req/sec}$) will saturate OTLP collectors and network bandwidth; use `0.05` or `0.1` (5-10%).
4. **Always Close Custom Spans in `finally` Blocks**: Unclosed spans cause memory leaks and corrupt parent-child span trees for subsequent operations on the same thread.
5. **Use Low-Cardinality KeyValues for Metrics**: When using the `Observation` API, keep high-cardinality values (e.g., UUIDs, customer IDs) out of metric tags; place them in `highCardinalityKeyValue` (which applies only to tracing spans).

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Importing `spring-cloud-starter-sleuth` in Spring Boot 3 | Classpath collisions and startup failure; Sleuth is dead | Migrate to `micrometer-tracing-bridge-otel`. |
| Instantiating `new RestClient()` without builder | Outbound HTTP requests lack `traceparent` headers; trace chain breaks | Inject and use `RestClient.Builder`. |
| Unclosed programmatic custom span | Span tree corrupted; subsequent spans become nested children of the unclosed span | Call `span.end()` inside a mandatory `finally` block. |
| High cardinality values in metric tags | Putting customer UUIDs into metric tags causes Prometheus memory explosion | Put UUIDs in `highCardinalityKeyValue` (tracing only). |
| Forgetting to configure logging pattern with `%X{traceId}` | Logs do not display trace IDs, making correlation impossible | Configure logging pattern: `[%X{traceId},%X{spanId}]`. |
| Lost trace context in asynchronous thread pools | Thread boundary switches without propagating MDC context | Use `ContextExecutorService` or `DelegatingSecurityContextAsyncTaskExecutor`. |

---

## 8. Performance & Security Notes

- **OTLP Exporter Non-Blocking I/O**: The OpenTelemetry OTLP exporter batches spans in memory and flushes them asynchronously over gRPC/Protobuf, adding negligible overhead to HTTP requests.
- **Data Privacy & PII in Spans**: Never attach sensitive personally identifiable information (credit card numbers, passwords, health data) as span tags; span attributes are stored in plaintext in tracing dashboards.
- **Trace Sampling Overhead**: Trace creation consumes minor CPU for UUID generation. Sampling determines whether spans are exported to the network.

---

## 9. Comparison / When to Use What

| Feature | OpenTelemetry (OTel) Bridge | Brave Bridge (Zipkin) | Spring Cloud Sleuth (Legacy) |
|---|---|---|---|
| **Industry Standard** | CNCF Global Standard | Zipkin Community Standard | Obsolete Spring Project |
| **Protocol** | OTLP (gRPC / HTTP Protobuf) | B3 / Zipkin JSON | B3 JSON |
| **Backend Support** | Universal (Jaeger, Tempo, Datadog) | Primarily Zipkin | Zipkin |
| **Spring Compatibility**| Spring Boot 3+ Native | Spring Boot 3+ Native | Spring Boot 2.x Only |
| **Best For** | Modern cloud-native microservices | Lightweight Zipkin-only setups | Legacy migration only |

---

## 10. Interview Questions

<details>
<summary>1. What happened to Spring Cloud Sleuth in Spring Boot 3?</summary>
Spring Cloud Sleuth was retired in Spring Boot 3. Its core tracing capabilities were rewritten and merged into **Micrometer Tracing** (part of the core Spring Framework and Micrometer project). This moves distributed tracing into the foundational metrics/observation layer, providing a unified `Observation` API for both metrics and traces.
</details>

<details>
<summary>2. What is the W3C Trace Context specification and what does the `traceparent` header contain?</summary>
W3C Trace Context is the international standard for distributed trace propagation across vendors. The `traceparent` header is formatted as `version-traceId-parentId-traceFlags` (e.g., `00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01`), where `00` is the version, followed by a 128-bit trace ID, a 64-bit parent span ID, and flags (where `01` means sampled).
</details>

<details>
<summary>3. What is the difference between Trace ID and Span ID?</summary>
A `traceId` represents the entire end-to-end transaction journey spanning all microservices from initial entry to completion. A `spanId` represents a single contiguous unit of execution within that journey on a specific service, method, or network hop.
</details>

<details>
<summary>4. How does Micrometer Tracing integrate with SLF4J logging?</summary>
Micrometer Tracing automatically populates the SLF4J Mapped Diagnostic Context (MDC) with `traceId` and `spanId`. By configuring the logging pattern to include `%X{traceId:-}` and `%X{spanId:-}`, every log message emitted during request processing automatically prints the active trace identifiers.
</details>

<details>
<summary>5. What is Baggage in distributed tracing?</summary>
Baggage represents arbitrary key-value pairs (e.g., `tenant-id`, `country-code`, `user-role`) that are attached to the trace context and transported across all downstream service boundaries over network headers, making business context available to all microservices without altering payload contracts.
</details>

<details>
<summary>6. Why is sampling necessary in high-volume production systems?</summary>
At enterprise scale (e.g., billions of requests per day), storing and exporting 100% of traces would overwhelm network bandwidth, saturate tracing backend storage (Elasticsearch/Tempo), and incur exorbitant cloud costs. Sampling (e.g., 5% or adaptive sampling) captures statistically representative traces and all error traces while reducing telemetry volume by 95%.
</details>

<details>
<summary>7. What is the unified `Observation` API in Spring Framework 6?</summary>
The `Observation` API (`ObservationRegistry`) unifies metrics, distributed tracing, and logging into a single programmatic abstraction. When a developer observes an operation via `Observation.observe()`, Spring automatically records a Timer metric in Micrometer and starts/stops a Span in Micrometer Tracing simultaneously.
</details>

<details>
<summary>8. How do you propagate trace context across asynchronous thread boundaries in Spring?</summary>
Because MDC and tracing contexts are stored in `ThreadLocal`, switching threads (e.g., submitting tasks to an `ExecutorService`) drops the trace context. To propagate context, wrap the task executor with `ContextExecutorService` or configure `TaskDecorator` to copy the MDC and tracing scope to child threads.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1 (Easy): Configure OTel bridge and 100% sampling</summary>
Write an `application.yml` configuring `micrometer-tracing-bridge-otel` with 1.0 sampling probability and W3C propagation.
</details>

<details>
<summary>Exercise 2 (Easy): Configure log pattern with traceId and spanId</summary>
Configure `logging.pattern.level` in `application.properties` to display `[service-name,traceId,spanId]`.
</details>

<details>
<summary>Exercise 3 (Medium): Create programmatic custom child span</summary>
Write a service method that injects `Tracer`, creates a custom child span named `"database-bulk-insert"`, tags it with record count, and closes it in a `finally` block.
</details>

<details>
<summary>Exercise 4 (Medium): Configure and propagate tenant baggage</summary>
Configure application properties to allow baggage field `tenantId` to be propagated across remote HTTP headers.
</details>

<details>
<summary>Exercise 5 (Hard): Instrument an async worker pool with ContextPropagatingTaskDecorator</summary>
Implement a `TaskDecorator` for a `ThreadPoolTaskExecutor` that copies the current `Tracer` span and MDC context to background threads.
</details>

<details>
<summary>Exercise 6 (Hard): Integration test asserting traceparent header injection</summary>
Write a Spring Boot test using `MockRestServiceServer` that asserts that outbound `RestClient` calls contain a valid `traceparent` header matching the active trace ID.
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------+
|                       Micrometer Tracing Summary                                |
+---------------------------------------------------------------------------------+
| Replaced:           Spring Cloud Sleuth (Retired in Spring Boot 3)              |
| Dependencies:       micrometer-tracing-bridge-otel + opentelemetry-exporter-otlp|
| Header Standard:    W3C Trace Context (traceparent: 00-<trace>-<span>-<flag>)   |
| Logging Pattern:    %5p [${spring.application.name:},%X{traceId:-},%X{spanId:-}]|
| Outbound Client:    Always use RestClient.Builder (Autoconfigured interceptor)  |
| Programmatic API:   Tracer.nextSpan() or Observation.createNotStarted()         |
| Sampling Rule:      Set probability to 0.05 - 0.10 in high-volume production    |
| Invariant:          Always close custom spans in finally blocks                 |
+---------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Official Micrometer Tracing Reference Documentation](https://micrometer.io/docs/tracing)
- [W3C Trace Context Recommendation](https://www.w3.org/TR/trace-context/)
- [OpenTelemetry Java Documentation](https://opentelemetry.io/docs/languages/java/)
