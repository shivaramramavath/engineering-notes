# OpenTelemetry Tracing and Logs in Spring Boot

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
In distributed microservices, a single client interaction can trigger dozens of cascading HTTP, gRPC, and messaging operations across separate services, databases, and message brokers. When a request experiences 2 seconds of latency or fails with an unexpected exception, traditional per-node log files cannot answer:
- Which service along the path introduced the latency?
- What was the end-to-end critical path?
- How can logs emitted by five distinct services be correlated to that exact request?

### Historical Evolution
Earlier ecosystems used proprietary or fragmented libraries:
- **Spring Cloud Sleuth**: Handled trace propagation in Spring Boot 1.x and 2.x, primarily using Zipkin or Brave.
- **OpenTracing & OpenCensus**: Two competing open-source standards for tracing and metrics.
- **OpenTelemetry (OTel)**: Formed under the Cloud Native Computing Foundation (CNCF) by merging OpenTracing and OpenCensus, establishing the industry-standard protocol for metrics, traces, and logs.

In Spring Boot 3.x, **Spring Cloud Sleuth was retired**. Tracing is now handled natively via **Micrometer Tracing** (`micrometer-tracing`), with direct bridges to OpenTelemetry (`micrometer-tracing-bridge-otel`) and standard OTLP (OpenTelemetry Protocol) exporters.

### Production Relevance
OpenTelemetry delivers:
- Standard W3C TraceContext propagation across HTTP and messaging boundaries (`traceparent`, `tracestate`).
- Correlated logging: Automatic injection of `traceId` and `spanId` into SLF4J MDC.
- Native export via gRPC or HTTP directly to the OpenTelemetry Collector, Tempo, Jaeger, or Datadog.

---

## 2. Core Concepts

### Distributed Tracing Building Blocks
1. **Trace**: A directed acyclic graph (DAG) of Spans representing the end-to-end journey of a request through a distributed system. Identified by a global 128-bit `traceId`.
2. **Span**: A contiguous unit of work (e.g., handling an HTTP request, executing a SQL query). Identified by a 64-bit `spanId`. Contains start/end timestamps, tags/attributes, events, and a reference to its `parentSpanId`.
3. **TraceContext**: The data structure holding the `traceId`, `spanId`, and trace flags passed across process boundaries.
4. **Propagator**: Responsible for serializing and deserializing `TraceContext` across network protocols (default: W3C `traceparent` header).
5. **Micrometer Tracing Tracer**: The framework abstraction (`io.micrometer.tracing.Tracer`) that allows creating, scoping, and finishing spans vendor-neutrally.
6. **OTLP Exporter**: Standard protocol (over HTTP/protobuf or gRPC) for shipping telemetry data to an OpenTelemetry Collector.

---

## 3. How It Works Internally

### Request Flow & Context Propagation

```
CLIENT (Browser/Gateway)
  |  HTTP GET /api/orders/101
  |  Headers: [traceparent: 00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01]
  v
+------------------------------------------------------------------------------------+
|                         ORDER SERVICE (SPRING BOOT 3)                              |
|                                                                                    |
|  1. TraceFilter extracts W3C header -> Restores TraceContext                       |
|  2. Micrometer Tracing creates SERVER Span [spanId: 5a8b9c...]                     |
|  3. Injects traceId & spanId into SLF4J MDC (ThreadLocal)                          |
|  4. Log statement outputs: [order-service, 4bf92f..., 5a8b9c...] "Processing order"|
|                                                                                    |
|  5. RestClient / WebClient injects updated traceparent header:                     |
|     [traceparent: 00-4bf92f3577b34da6a3ce929d0e0e4736-5a8b9c...-01]               |
+------------------------------------------------------------------------------------+
  |
  |  HTTP POST /api/inventory/reserve
  v
+------------------------------------------------------------------------------------+
|                       INVENTORY SERVICE (SPRING BOOT 3)                            |
|                                                                                    |
|  1. TraceFilter extracts parentSpanId (5a8b9c...)                                  |
|  2. Creates CHILD Span [spanId: 9f8e7d...]                                         |
|  3. Executes DB transaction -> DB Span                                             |
|  4. Closes Spans -> Batched to OTLP Exporter                                       |
+------------------------------------------------------------------------------------+
  |
  |  OTLP gRPC (Port 4317) / HTTP (Port 4318)
  v
+------------------------------------------------------------------------------------+
|                         OPENTELEMETRY COLLECTOR                                     |
|  Receives -> Batches -> Filters PII -> Ships to Jaeger / Grafana Tempo / Datadog   |
+------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Configuration Reference

### Dependencies (`pom.xml`)
```xml
<!-- Spring Boot Actuator -->
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-starter-actuator</artifactId>
</dependency>

<!-- Micrometer Tracing Bridge to OpenTelemetry -->
<dependency>
    <groupId>io.micrometer</groupId>
    <artifactId>micrometer-tracing-bridge-otel</artifactId>
</dependency>

<!-- OpenTelemetry Exporter to OTLP (OpenTelemetry Collector) -->
<dependency>
    <groupId>io.opentelemetry</groupId>
    <artifactId>opentelemetry-exporter-otlp</artifactId>
</dependency>
```

### `application.yml` Tracing Configuration
```yaml
management:
  tracing:
    enabled: true
    sampling:
      probability: 1.0 # 100% in staging/dev; use 0.05-0.1 (5-10%) in high-traffic production
    propagation:
      type: [ "W3C" ] # Standard W3C TraceContext headers
  otlp:
    tracing:
      endpoint: "http://otel-collector.monitoring.svc.cluster.local:4318/v1/traces"
      transport: http # http or grpc
      timeout: 2s
  endpoints:
    web:
      exposure:
        include: [ "health", "info", "prometheus" ]

logging:
  pattern:
    level: "%5p [${spring.application.name:},%X{traceId:-},%X{spanId:-}]"
```

---

## 5. Practical Examples

### Example 1: Manual Span Creation with Micrometer `Tracer`
```java
package com.example.observability.tracing;

import io.micrometer.tracing.Span;
import io.micrometer.tracing.Tracer;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Service;

@Service
public class OrderFulfillmentService {

    private static final Logger log = LoggerFactory.getLogger(OrderFulfillmentService.class);
    private final Tracer tracer;

    public OrderFulfillmentService(Tracer tracer) {
        this.tracer = tracer;
    }

    public void processFulfillment(String orderId) {
        // Start a custom business span
        Span customSpan = this.tracer.nextSpan().name("order-fulfillment-step").start();

        try (Tracer.SpanInScope ws = this.tracer.withSpan(customSpan)) {
            customSpan.tag("order.id", orderId);
            customSpan.event("inventory-check-started");

            log.info("Fulfilling order item allocations for {}", orderId);
            executeWarehouseAllocation(orderId);

            customSpan.event("inventory-check-completed");
        } catch (Exception ex) {
            customSpan.error(ex);
            throw ex;
        } finally {
            customSpan.end(); // Span is finalized and scheduled for export
        }
    }

    private void executeWarehouseAllocation(String orderId) {
        // Business logic
    }
}
```

### Example 2: Declarative Tracing with `@Observed` Annotation
```java
package com.example.observability.tracing;

import io.micrometer.observation.annotation.Observed;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Service;

@Service
public class FraudDetectionService {

    private static final Logger log = LoggerFactory.getLogger(FraudDetectionService.class);

    // Creates both Micrometer metrics and an OpenTelemetry span automatically
    @Observed(
        name = "fraud.evaluation",
        contextualName = "evaluate-transaction-risk",
        lowCardinalityKeyValues = {"risk.tier", "standard"}
    )
    public boolean evaluateTransactionRisk(String accountId, double amount) {
        log.info("Evaluating transaction risk for account {} with amount ${}", accountId, amount);

        if (amount > 10_000.0) {
            log.warn("High transaction amount detected, triggering secondary check");
            return false;
        }
        return true;
    }
}
```

### Example 3: Context Propagation via Outbound `RestClient`
```java
package com.example.observability.config;

import org.springframework.boot.web.client.ClientHttpRequestFactories;
import org.springframework.boot.web.client.ClientHttpRequestFactorySettings;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.web.client.RestClient;

import java.time.Duration;

@Configuration(proxyBeanMethods = false)
public class TracedClientConfiguration {

    // Injecting RestClient.Builder ensures Micrometer Tracing interceptors are auto-configured!
    @Bean
    public RestClient shippingRestClient(RestClient.Builder builder) {
        return builder
            .baseUrl("https://shipping.internal.company.com")
            .requestFactory(ClientHttpRequestFactories.get(
                ClientHttpRequestFactorySettings.DEFAULTS
                    .withConnectTimeout(Duration.ofSeconds(1))
                    .withReadTimeout(Duration.ofSeconds(3))
            ))
            .build();
    }
}
```

### Example 4: Anti-Pattern vs. Production Fix (Context Loss in Multi-Threading)
```java
// ANTI-PATTERN: Spawning raw threads or CompletableFuture without propagating TraceContext
// Result: Child thread loses traceId and spanId; logs show empty brackets []
@Service
public class BadAsyncWorker {
    private static final Logger log = LoggerFactory.getLogger(BadAsyncWorker.class);

    public void runTask() {
        // BAD: Thread switch strips MDC and Tracer context
        CompletableFuture.runAsync(() -> {
            log.info("Executing async task"); // traceId is NULL!
        });
    }
}

// PRODUCTION FIX: Context propagation using ContextExecutorService or Observation
@Service
public class ResilientAsyncWorker {
    private static final Logger log = LoggerFactory.getLogger(ResilientAsyncWorker.class);
    private final Tracer tracer;
    private final ExecutorService tracedExecutor;

    public ResilientAsyncWorker(Tracer tracer) {
        this.tracer = tracer;
        // Wraps executor with context propagation
        this.tracedExecutor = Executors.newFixedThreadPool(4);
    }

    public void runTask() {
        Span currentSpan = tracer.currentSpan();

        tracedExecutor.submit(() -> {
            // Re-establish span scope on worker thread
            try (Tracer.SpanInScope ws = tracer.withSpan(currentSpan)) {
                log.info("Executing async task with preserved context"); // traceId preserved!
            }
        });
    }
}
```

### Example 5: Custom OpenTelemetry Baggage Injection
```java
package com.example.observability.tracing;

import io.micrometer.tracing.BaggageInScope;
import io.micrometer.tracing.Tracer;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Service;

@Service
public class TenantBaggageService {

    private static final Logger log = LoggerFactory.getLogger(TenantBaggageService.class);
    private final Tracer tracer;

    public TenantBaggageService(Tracer tracer) {
        this.tracer = tracer;
    }

    public void executeWithTenant(String tenantId, Runnable task) {
        // Baggage propagates downstream across ALL subsequent HTTP/Kafka calls
        try (BaggageInScope baggage = this.tracer.createBaggageInScope("tenant-id", tenantId)) {
            log.info("Executing operations within tenant baggage context");
            task.run();
        }
    }
}
```

---

## 6. Best Practices & Production Guidelines

1. **Use Sampling Rates Carefully**: Never use `sampling.probability: 1.0` in high-throughput production (10k+ req/sec). Set probability to `0.01`–`0.1` (1% to 10%) or deploy head/tail-based sampling in the OpenTelemetry Collector.
2. **Propagate Context through RestClient/WebClient Builders**: Always inject `RestClient.Builder` or `WebClient.Builder` from the Spring context. Creating instances manually (`new RestTemplate()`) breaks tracing propagation headers.
3. **Guard Against High-Cardinality Span Tags**: Keep span attributes bounded. Never place dynamic payload bodies or passwords into span tags.
4. **Use Baggage Conservatively**: Baggage entries are serialized into every downstream network header. High amounts of baggage waste network bandwidth and can exceed HTTP header size limits (e.g., 8KB).
5. **Always End Spans in `finally` Blocks**: If an exception occurs before `span.end()`, the span leaks in memory and is never exported to the collector.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Creating `new RestTemplate()` | Bypasses Spring Boot auto-configuration; W3C `traceparent` headers are not attached to outbound HTTP requests. | Inject and customize `RestClient.Builder` or `RestTemplateBuilder`. |
| Loss of TraceId in `CompletableFuture` | Thread switching clears `ThreadLocal` MDC context, resulting in orphan logs without trace correlation. | Use Spring `@Async` or decorate executors with `ContextSnapshotFactory`. |
| Not closing `SpanInScope` | Leaks span context onto pooled threads, causing subsequent requests to incorrectly inherit previous trace IDs. | Always manage `SpanInScope` with a `try-with-resources` block. |
| Forgetting `span.end()` | The span is never flushed to the batch exporter and remains open indefinitely. | Always place `span.end()` inside a `finally` block. |
| Using outdated `spring-cloud-starter-sleuth` in Boot 3 | Incompatible with Spring Boot 3; causes startup failure or conflicts. | Replace with `micrometer-tracing-bridge-otel`. |
| Exporter endpoint network timeouts | Slow or unreachable OTLP Collector blocks worker threads or exhausts buffer memory. | Set aggressive timeouts (`timeout: 2s`) and use asynchronous batching exporters. |

---

## 8. Performance & Security Notes

### Overhead of Tracing
- OpenTelemetry spans create small short-lived objects. At 100% sampling under heavy load, trace generation can consume 2-5% of CPU and allocate memory in Eden.
- Use `management.tracing.sampling.probability` to balance observability resolution with CPU budget.

### Security & Compliance
- Spans often appear in third-party SaaS UIs (Datadog, Dynatrace, Honeycomb). Never tag spans with PII (Social Security numbers, credit card tokens, plain passwords).
- Filter sensitive query parameters in the OpenTelemetry Collector using the `redactionprocessor`.

---

## 9. Comparison / When to Use What

| Feature | Micrometer Tracing (OTel Bridge) | Spring Cloud Sleuth (Legacy) | Pure OpenTelemetry Java Agent |
|---|---|---|---|
| **Spring Boot 3 Support** | Native standard | Deprecated / Unsupported | Supported via external JVM `-javaagent` |
| **Configuration Model** | Standard `application.yml` | Standard `application.yml` | JVM flags & environment variables |
| **Startup Overhead** | Zero bytecode transformation | Bytecode reflection | Dynamic bytecode manipulation at startup |
| **Native Image (GraalVM)**| Fully supported AOT | No | Limited / Complex |
| **Observation API Integration**| Native (`@Observed`, `Observation`) | Not available | Separate |

---

## 10. Interview Questions & Answers

### Q1: What replaced Spring Cloud Sleuth in Spring Boot 3?
<details>
<summary>Click to view answer</summary>

Spring Cloud Sleuth was retired in Spring Boot 3 and replaced by **Micrometer Tracing**. Micrometer Tracing is a vendor-neutral facade that supports multiple tracer implementations via bridges:
- `micrometer-tracing-bridge-otel` (for OpenTelemetry)
- `micrometer-tracing-bridge-brave` (for Zipkin/Brave)
It integrates natively with the Spring Boot 3 `ObservationRegistry`.
</details>

### Q2: What is the format of the standard W3C `traceparent` header?
<details>
<summary>Click to view answer</summary>

The W3C `traceparent` header consists of 4 hyphen-separated fields:
`version-trace_id-parent_id-trace_flags`
Example: `00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01`
- `00`: W3C version.
- `4bf92f...`: 128-bit Trace ID (32 hex characters).
- `00f067...`: 64-bit Parent Span ID (16 hex characters).
- `01`: Trace flags (`01` indicates sampled, `00` not sampled).
</details>

### Q3: How does TraceContext propagate into SLF4J log lines?
<details>
<summary>Click to view answer</summary>

Micrometer Tracing automatically binds to SLF4J's **MDC (Mapped Diagnostic Context)** via a `CurrentTraceContext.ScopeDecorator`. When a span becomes active, its `traceId` and `spanId` are written to MDC `ThreadLocal` storage. The logging encoder (Logback) references these keys via `%X{traceId}` and `%X{spanId}`.
</details>

### Q4: What is the difference between Span Tags and Span Baggage?
<details>
<summary>Click to view answer</summary>

- **Span Tags (Attributes)**: Local to the specific span where they are added. They are recorded in the span payload and exported, but are **not** forwarded across network calls to downstream microservices.
- **Span Baggage**: Key-value pairs that travel across process boundaries alongside the trace context in HTTP headers (W3C Baggage header). Every downstream service receives and propagates the baggage.
</details>

### Q5: How do you handle trace context propagation when switching to a Virtual Thread in Java 21?
<details>
<summary>Click to view answer</summary>

Virtual Threads do not automatically inherit `ThreadLocal` state unless explicitly created via `Thread.ofVirtual().inheritInheritableThreadLocals(true)`. In Spring Boot 3, using `ContextSnapshotFactory` or the Observation API ensures that trace context is captured on the initiating thread and restored on the Virtual Thread.
</details>

### Q6: What is the role of an OpenTelemetry Collector in enterprise architectures?
<details>
<summary>Click to view answer</summary>

The OpenTelemetry Collector is an out-of-process proxy that receives, processes, and exports telemetry data. It offloads processing from application pods by:
- Buffering and batching telemetry.
- Scrubbing sensitive PII before export.
- Implementing tail-based sampling (retaining 100% of failed traces while dropping normal traces).
- Fan-out exporting to multiple monitoring backends simultaneously.
</details>

### Q7: Why is `SpanInScope` wrapped in a `try-with-resources` block?
<details>
<summary>Click to view answer</summary>

`Tracer.withSpan(span)` sets the span as current on the active thread and returns a `SpanInScope` (which implements `AutoCloseable`). Closing `SpanInScope` restores the previous span context on the thread. Without `try-with-resources`, if an exception occurs, the thread remains permanently tied to that span, corrupting subsequent traces executed by that pooled thread.
</details>

### Q8: What is tail-based sampling and how does it differ from head-based sampling?
<details>
<summary>Click to view answer</summary>

- **Head-based sampling**: The sampling decision is made at the start of the trace (at the ingress API Gateway) based on a configured percentage (e.g., 5%). If an error occurs in a non-sampled trace, the error trace is lost.
- **Tail-based sampling**: All spans are captured and buffered in memory by the OpenTelemetry Collector until the trace completes. The collector inspects the entire trace: if any span has an HTTP 500 error or latency $> 2\text{s}$, the entire trace is retained; otherwise, it can be dropped.
</details>

---

## 11. Practice Exercises

### Exercise 1: Configure Custom MDC Trace Fields (Easy)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Update Logback configuration to include the application name, trace ID, and span ID in all log outputs.

**Solution**:
```xml
<!-- src/main/resources/logback-spring.xml -->
<configuration>
    <appender name="CONSOLE" class="ch.qos.logback.core.ConsoleAppender">
        <encoder>
            <pattern>%d{yyyy-MM-dd HH:mm:ss.SSS} [%thread] %highlight(%-5level) [%X{traceId:-},%X{spanId:-}] %cyan(%logger{36}) - %msg%n</pattern>
        </encoder>
    </appender>
    <root level="INFO">
        <appender-ref ref="CONSOLE"/>
    </root>
</configuration>
```
</details>

### Exercise 2: Tracing Kafka Message Producers and Consumers (Medium)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Configure Spring Kafka to propagate W3C trace headers automatically across Kafka records.

**Solution**:
```java
package com.example.observability.config;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.kafka.core.KafkaTemplate;
import org.springframework.kafka.core.ProducerFactory;

@Configuration
public class KafkaTracingConfiguration {

    // Spring Boot 3 auto-configures observation on KafkaTemplate if micrometer-tracing is on classpath
    @Bean
    public KafkaTemplate<String, Object> kafkaTemplate(ProducerFactory<String, Object> pf) {
        KafkaTemplate<String, Object> template = new KafkaTemplate<>(pf);
        template.setObservationEnabled(true); // Enables span creation and header propagation
        return template;
    }
}
```
</details>

### Exercise 3: Custom OTLP Span Processor for PII Masking (Hard)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Implement an OpenTelemetry `SpanProcessor` bean that intercepts spans before export and replaces any span attribute named `customer.email` with `***@***.com`.

**Solution**:
```java
package com.example.observability.tracing;

import io.opentelemetry.api.common.AttributeKey;
import io.opentelemetry.api.common.Attributes;
import io.opentelemetry.context.Context;
import io.opentelemetry.sdk.trace.ReadWriteSpan;
import io.opentelemetry.sdk.trace.ReadableSpan;
import io.opentelemetry.sdk.trace.SpanProcessor;
import org.springframework.stereotype.Component;

@Component
public class PiiMaskingSpanProcessor implements SpanProcessor {

    private static final AttributeKey<String> EMAIL_KEY = AttributeKey.stringKey("customer.email");

    @Override
    public void onStart(Context parentContext, ReadWriteSpan span) {
        // No-op on start
    }

    @Override
    public boolean isStartRequired() {
        return false;
    }

    @Override
    public void onEnd(ReadableSpan span) {
        if (span instanceof ReadWriteSpan rwSpan) {
            String email = rwSpan.getAttribute(EMAIL_KEY);
            if (email != null) {
                rwSpan.setAttribute(EMAIL_KEY, "[REDACTED_PII]");
            }
        }
    }

    @Override
    public boolean isEndRequired() {
        return true;
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+-----------------------------------+-------------------------------------------------------------+
| Concept / Property                | Purpose & Functionality                                     |
+-----------------------------------+-------------------------------------------------------------+
| micrometer-tracing-bridge-otel    | Spring Boot 3 bridge to OpenTelemetry Tracer                |
| opentelemetry-exporter-otlp       | Exports traces directly to OTLP collector via gRPC/HTTP     |
| management.tracing.sampling       | Configures trace sampling probability (e.g. 0.05 = 5%)      |
| W3C traceparent                   | Standard cross-service header: version-traceId-spanId-flags |
| @Observed                         | Declaratively creates both timer metrics and trace spans    |
| Tracer.SpanInScope                | Binds span to ThreadLocal/MDC; must be closed in finally    |
+-----------------------------------+-------------------------------------------------------------+
```

---

## 13. Further Reading
- OpenTelemetry Official Specification & Java SDK
- Micrometer Tracing Reference Documentation
- W3C TraceContext Recommendation Specification
- CNCF OpenTelemetry Collector Architecture and Design Guide