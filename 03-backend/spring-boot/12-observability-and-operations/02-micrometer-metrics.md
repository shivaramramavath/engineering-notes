# Micrometer Metrics in Spring Boot

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
Modern microservice ecosystems require high-resolution dimensional metrics to evaluate latency, throughput, error rates, and resource utilization. In early application frameworks, developers instrumented code against vendor-specific APIs (such as Graphite, StatsD, or Dropwizard Metrics). This created severe vendor lock-in: switching from Graphite to Prometheus or Datadog required modifying hundreds of instrumentation points across dozens of microservices.

### Historical Evolution
Micrometer was created by VMware/Pivotal as the "SLF4J for application metrics". Instead of binding directly to a concrete monitoring backend, developers instrument their code against the vendor-neutral Micrometer API (`MeterRegistry`, `Counter`, `Timer`, `Gauge`).

With Spring Boot 2.0, Micrometer became the default metrics facade across the entire Spring portfolio. 

In Spring Boot 3.x, Micrometer expanded further with the **Micrometer Observation API** (`micrometer-observation`), unifying metrics, distributed tracing, and logging under a single contextual lifecycle abstraction.

### Production Relevance
Micrometer empowers engineering teams to:
- Capture multidimensional metrics with low-overhead dimensional tags (`http.server.requests{method="POST",status="200",uri="/api/orders"}`).
- Export to multiple backends simultaneously (e.g., Prometheus for alerting, Datadog for executive dashboards).
- Calculate client-side and server-side percentiles (p50, p95, p99) and SLA bucket distributions.
- Instrument custom business KPIs (e.g., shopping cart checkout totals, payment gateway latency distributions).

---

## 2. Core Concepts

### Primary Meter Primitives
1. **MeterRegistry**: The central coordinator where meters are created, registered, and retrieved. Each backend provides an implementation (e.g., `PrometheusMeterRegistry`, `SimpleMeterRegistry`).
2. **Counter**: A monotonically increasing cumulative metric used to measure event counts or rates (e.g., total requests, errors, items processed). Counters cannot decrease.
3. **Timer**: Measures both short-duration latencies and event frequencies simultaneously (e.g., HTTP request durations, database query times). Provides total time, count, max, and percentiles.
4. **Gauge**: An instantaneous, sampleable snapshot of a numerical value that can go up and down (e.g., active thread count, queue depth, cache size, memory usage).
5. **DistributionSummary**: Tracks the distribution of non-time events (e.g., payload size in bytes, batch job item counts).
6. **Tags (Dimensions)**: Key-value metadata attached to meters allowing multi-dimensional filtering, grouping, and slice-and-dice queries in PromQL or Datadog.
7. **Observation API**: Introduced in Micrometer 1.10 / Spring Boot 3 to observe operations once and generate both metrics and distributed trace spans automatically.

---

## 3. How It Works Internally

### Micrometer MeterRegistry Architecture & Collection Flow

```
+------------------------------------------------------------------------------------+
|                               APPLICATION THREAD                                  |
|          OrderService.processOrder() -> Timer.record(duration)                     |
+------------------------------------------------------------------------------------+
                                         |
                                         v
+------------------------------------------------------------------------------------+
|                         MICROMETER TIMER IMPLEMENTATION                            |
|             (e.g., CumulativeHistogramTimer / StepTimer)                           |
|  - Updates LongAdder counters (lock-free thread-safe increments)                   |
|  - Updates ring buffer / histogram buckets for percentile calculations             |
|  - Computes decaying maximum over configurable step intervals                      |
+------------------------------------------------------------------------------------+
                                         |
                                         v
+------------------------------------------------------------------------------------+
|                         COMPOSITE METER REGISTRY                                    |
|         Fans out meter data to registered backend registries                        |
+------------------------------------------------------------------------------------+
                   |                                           |
                   v                                           v
+------------------------------------+       +------------------------------------+
|     PROMETHEUS METER REGISTRY      |       |      DATADOG METER REGISTRY        |
|  - Formats data into Prometheus    |       |  - Buffers metrics into StepBuckets|
|    exposition format text          |       |  - Pushes metrics via HTTP POST   |
|  - Scraped pull via /actuator/     |       |    to Datadog API every 10s       |
|    prometheus                      |       |                                    |
+------------------------------------+       +------------------------------------+
```

### Thread Safety and Lock-Free Accumulation
Micrometer primitives are engineered for minimal overhead in high-throughput hot paths:
- Counters and Timers utilize `java.util.concurrent.atomic.LongAdder` and `DoubleAdder` under the hood to eliminate thread contention across hundreds of virtual or platform threads.
- Gauges hold weak references to the underlying monitored objects by default to prevent memory leaks.

---

## 4. Syntax & Configuration Reference

### Common `application.yml` Metric Settings

```yaml
management:
  metrics:
    enable:
      all: true
      jvm: true
      process: true
      system: true
    distribution:
      percentiles-histogram:
        http.server.requests: true # Enables Prometheus histogram buckets (le tags)
      percentiles:
        http.server.requests: 0.5, 0.9, 0.95, 0.99
      sla:
        http.server.requests: 100ms, 250ms, 500ms, 1s
    tags:
      application: ${spring.application.name}
      region: us-east-1
      environment: production
    export:
      prometheus:
        enabled: true
        step: 1m
```

---

## 5. Practical Examples

### Example 1: Custom Business Metrics with Counter and Timer
```java
package com.example.observability.metrics;

import io.micrometer.core.instrument.Counter;
import io.micrometer.core.instrument.MeterRegistry;
import io.micrometer.core.instrument.Timer;
import org.springframework.stereotype.Service;

import java.util.concurrent.TimeUnit;

@Service
public class PaymentProcessingService {

    private final Counter successCounter;
    private final Counter failureCounter;
    private final Timer paymentLatencyTimer;

    public PaymentProcessingService(MeterRegistry registry) {
        this.successCounter = Counter.builder("payments.processed.total")
            .description("Total number of successfully processed payments")
            .tag("status", "success")
            .register(registry);

        this.failureCounter = Counter.builder("payments.processed.total")
            .description("Total number of failed payments")
            .tag("status", "failed")
            .register(registry);

        this.paymentLatencyTimer = Timer.builder("payments.processing.duration")
            .description("Time taken to process payment transactions")
            .publishPercentiles(0.5, 0.95, 0.99)
            .publishPercentileHistogram()
            .register(registry);
    }

    public void processPayment(String gateway, double amount) {
        long startTime = System.nanoTime();
        try {
            // Simulate payment processing
            executeGatewayCharge(gateway, amount);
            successCounter.increment();
        } catch (Exception ex) {
            failureCounter.increment();
            throw ex;
        } finally {
            paymentLatencyTimer.record(System.nanoTime() - startTime, TimeUnit.NANOSECONDS);
        }
    }

    private void executeGatewayCharge(String gateway, double amount) {
        // Business logic interacting with third-party gateway
    }
}
```

### Example 2: Dynamic Gauge Monitoring In-Flight Queue Depth
```java
package com.example.observability.metrics;

import io.micrometer.core.instrument.Gauge;
import io.micrometer.core.instrument.MeterRegistry;
import org.springframework.stereotype.Component;

import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;

@Component
public class OrderWorkQueueMonitor {

    private final BlockingQueue<String> orderWorkQueue = new LinkedBlockingQueue<>(10_000);

    public OrderWorkQueueMonitor(MeterRegistry registry) {
        // Registers a gauge holding a weak reference to the queue size
        Gauge.builder("orders.workqueue.size", orderWorkQueue, BlockingQueue::size)
            .description("Current number of pending orders waiting for fulfillment")
            .tag("queue.name", "fulfillment-inbound")
            .register(registry);

        Gauge.builder("orders.workqueue.remaining.capacity", orderWorkQueue, BlockingQueue::remainingCapacity)
            .description("Remaining capacity in fulfillment queue")
            .tag("queue.name", "fulfillment-inbound")
            .register(registry);
    }

    public void enqueueOrder(String orderId) {
        orderWorkQueue.offer(orderId);
    }

    public String dequeueOrder() throws InterruptedException {
        return orderWorkQueue.take();
    }
}
```

### Example 3: Tracking Payloads with DistributionSummary
```java
package com.example.observability.metrics;

import io.micrometer.core.instrument.DistributionSummary;
import io.micrometer.core.instrument.MeterRegistry;
import org.springframework.stereotype.Service;

@Service
public class DocumentIngestionService {

    private final DistributionSummary payloadSizeSummary;

    public DocumentIngestionService(MeterRegistry registry) {
        this.payloadSizeSummary = DistributionSummary.builder("documents.ingested.bytes")
            .description("Distribution of incoming document payload sizes in bytes")
            .baseUnit("bytes")
            .scale(1.0)
            .serviceLevelObjectives(1024, 1024 * 1024, 10 * 1024 * 1024) // 1KB, 1MB, 10MB
            .register(registry);
    }

    public void ingestDocument(String documentId, byte[] content) {
        payloadSizeSummary.record(content.length);
        // Process document bytes
    }
}
```

### Example 4: Anti-Pattern vs. Production Fix (High Cardinality Tags)
```java
// ANTI-PATTERN: Adding unique IDs (UUID, UserID, AccountID) as metric tags
// This creates explosive metric cardinality, exhausts Prometheus / JVM heap memory!
@Service
public class BadMetricsService {
    private final MeterRegistry registry;

    public BadMetricsService(MeterRegistry registry) {
        this.registry = registry;
    }

    public void trackUserLogin(String userId, String ipAddress) {
        // DO NOT DO THIS! Infinite unique combinations will cause an OutOfMemoryError!
        registry.counter("user.login.events", 
            "user_id", userId, 
            "ip", ipAddress
        ).increment();
    }
}

// PRODUCTION FIX: Low cardinality tags + Distribution summary or Logging for identifiers
@Service
public class ScalableMetricsService {
    private final MeterRegistry registry;

    public ScalableMetricsService(MeterRegistry registry) {
        this.registry = registry;
    }

    public void trackUserLogin(String authProvider, String countryCode) {
        // SAFE: Finite set of auth providers and 2-letter country codes
        registry.counter("user.login.events.total",
            "provider", authProvider, // e.g., 'google', 'github', 'saml'
            "country", countryCode    // e.g., 'US', 'DE', 'IN'
        ).increment();
    }
}
```

### Example 5: Unified Telemetry with Micrometer Observation API (Spring Boot 3)
```java
package com.example.observability.metrics;

import io.micrometer.observation.Observation;
import io.micrometer.observation.ObservationRegistry;
import org.springframework.stereotype.Service;

@Service
public class InventoryReservationService {

    private final ObservationRegistry observationRegistry;

    public InventoryReservationService(ObservationRegistry observationRegistry) {
        this.observationRegistry = observationRegistry;
    }

    public String reserveItem(String sku, int quantity) {
        // Automatically creates both Micrometer timer metrics and OpenTelemetry trace spans!
        return Observation.createNotStarted("inventory.reservation", observationRegistry)
            .lowCardinalityKeyValue("sku.category", sku.split("-")[0])
            .highCardinalityKeyValue("sku.id", sku)
            .observe(() -> {
                // Actual transactional reservation
                return executeReservation(sku, quantity);
            });
    }

    private String executeReservation(String sku, int quantity) {
        return "RESERVED-" + sku + "-" + quantity;
    }
}
```

---

## 6. Best Practices & Production Guidelines

1. **Strictly Enforce Low Cardinality**: Never use User IDs, Order UUIDs, credit card numbers, email addresses, or timestamps as tags. Tags must belong to a small, finite domain (status codes, HTTP methods, service names, regions).
2. **Follow Prometheus Naming Conventions**: Use base units in snake_case (e.g., `http_server_requests_seconds`, `jvm_memory_used_bytes`, `orders_processed_total`).
3. **Prefer Micrometer Observation API in Boot 3**: Use `ObservationRegistry` for new business operations to get distributed tracing and metrics with zero code duplication.
4. **Use MeterFilter for Production Guardrails**: Register a `MeterFilter` bean to cap maximum cardinality, deny unexpected metrics, or remap tags dynamically.
5. **Beware of Gauge Memory Retention**: Never register a Gauge against an object whose reference cannot be garbage collected. Use weak references or static helper methods.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Tagging with UUIDs or user emails | Creates unbounded timeseries in Prometheus, leading to scrape timeouts, cluster crashes, and high SaaS bills. | Move high-cardinality data to distributed tracing spans or structured logs. |
| Using Gauges for counting events | Gauges fluctuate and miss intermediate spikes between Prometheus scrape intervals (e.g., 15s–60s). | Use a `Counter` for event counts; PromQL `rate()` computes the accurate per-second rate. |
| Instantiating Meters inside hot methods | Calling `registry.counter(...)` on every request incurs map lookups and tag array allocations. | Declare meters as `final` fields or cache them in instance variables. |
| Ignoring percentiles histogram configuration | Prometheus `histogram_quantile()` queries fail or return NaN because histogram buckets (`le`) were not published. | Set `management.metrics.distribution.percentiles-histogram.http.server.requests=true`. |
| Gauge returning NaN or throwing Exception | An exception inside the Gauge lambda breaks the entire Prometheus scrape request with HTTP 500. | Handle exceptions inside the gauge calculation lambda and return `0.0` or `-1.0` on failure. |
| Forgetting to register meters to `MeterRegistry` | Creating a `Counter.builder(...)` without `.register(registry)` causes the metric to silently discard all increments. | Always invoke `.register(registry)` at initialization. |

---

## 8. Performance & Security Notes

### Overhead of Metrics Collection
- Micrometer primitives (`LongAdder`, `DoubleAdder`) add less than 15 nanoseconds of overhead per invocation, making them safe for critical latency paths.
- Server-side percentile calculations (`publishPercentiles`) use HdrHistogram under the hood. Avoid publishing percentiles on hundreds of distinct metrics; prefer exporting histogram buckets (`percentiles-histogram=true`) and calculating quantiles server-side in Prometheus.

### Security & Compliance
- Metrics must never leak Personally Identifiable Information (PII) or secrets via tags or metric names.
- Ensure `/actuator/prometheus` and `/actuator/metrics` are gated behind appropriate network policies or OAuth2 scopes.

---

## 9. Comparison / When to Use What

| Metric Primitive | Primary Purpose | Thread Safety | Example Use Case |
|---|---|---|---|
| **Counter** | Monotonically increasing rate/count | Lock-free (`LongAdder`) | Total HTTP 500 errors, messages consumed |
| **Timer** | Latency distribution & event count | Lock-free + ring buffer | REST endpoint response time, DB transaction time |
| **Gauge** | Instantaneous value snapshot | Dependent on supplier | Active connection pool size, heap memory used |
| **DistributionSummary**| Non-time quantity distribution | Lock-free buckets | Request body payload size, Kafka batch size |
| **Observation** | Unified metrics + trace span | Contextual lifecycle | End-to-end service boundary operation |

---

## 10. Interview Questions & Answers

### Q1: What is the fundamental difference between a Counter and a Gauge in Micrometer?
<details>
<summary>Click to view answer</summary>

A **Counter** is a cumulative metric whose value only increases over time (or is reset to zero on process restart). It is used to calculate event rates (e.g., requests per second) via Prometheus `rate()` functions. 

A **Gauge** represents an instantaneous snapshot of a value that can increase or decrease arbitrarily (e.g., current queue size, CPU temperature, active thread count). Gauges do not track historical rates; they only report current state when sampled.
</details>

### Q2: Why is "metric cardinality" the most dangerous operational risk in Micrometer?
<details>
<summary>Click to view answer</summary>

Metric cardinality is the total number of unique time-series generated by all combinations of tag keys and values. If a tag contains unbounded values (such as User IDs, IP addresses, or UUIDs), each unique value creates a new time-series in memory. In Prometheus, this leads to exponential RAM usage, scrape timeouts, and eventual Out-Of-Memory crashes.
</details>

### Q3: How do you configure SLA boundaries and percentile histograms in Spring Boot?
<details>
<summary>Click to view answer</summary>

Via `application.yml` under `management.metrics.distribution`:
```yaml
management:
  metrics:
    distribution:
      percentiles-histogram:
        http.server.requests: true
      sla:
        http.server.requests: 50ms, 100ms, 200ms, 500ms
```
This generates Prometheus histogram buckets (`http_server_requests_seconds_bucket{le="0.1"}`) enabling `histogram_quantile(0.99, ...)` queries in Grafana.
</details>

### Q4: How does Micrometer's Observation API bridge metrics and distributed tracing?
<details>
<summary>Click to view answer</summary>

The `Observation API` (`io.micrometer.observation.ObservationRegistry`) provides a single lifecycle abstraction for an operation. When an `Observation` is started and stopped:
1. `ObservationHandler<TimerContext>` creates Micrometer Timers and Counters.
2. `ObservationHandler<TraceContext>` creates OpenTelemetry or Brave trace spans.
Low-cardinality key-values become both metric tags and span attributes; high-cardinality key-values become span attributes only, preventing metric cardinality explosion.
</details>

### Q5: What is a `MeterFilter` and when would you use one?
<details>
<summary>Click to view answer</summary>

A `MeterFilter` is a strategy bean used to intercept, modify, rename, or suppress meters during registration. Common use cases:
- Denying meters matching specific prefix patterns (e.g., `MeterFilter.denyNameStartsWith("jvm")`).
- Appending common tags to all meters.
- Capping maximum tag cardinality using `MeterFilter.maximumAllowableTags(...)`.
</details>

### Q6: Can you track Virtual Threads using Micrometer in Java 21?
<details>
<summary>Click to view answer</summary>

Standard JVM thread metrics track platform threads (`jvm.threads.live`, `jvm.threads.peak`). For Virtual Threads, monitoring focuses on carrier thread pool saturation and pinned virtual threads via JFR events (`jdk.VirtualThreadPinned`) or custom executors monitored via `ExecutorServiceMetrics.monitor(registry, executor, "virtual-executor")`.
</details>

### Q7: Why do Gauges use WeakReferences in Micrometer?
<details>
<summary>Click to view answer</summary>

If a `MeterRegistry` held strong references to monitored objects (e.g., custom cache instances or temporary connection objects), those objects could never be collected by the Garbage Collector, causing severe memory leaks. By default, Micrometer holds a `WeakReference` so that when the monitored object is no longer referenced elsewhere, it can be reclaimed cleanly.
</details>

### Q8: What is the difference between client-side percentiles and server-side Prometheus percentiles?
<details>
<summary>Click to view answer</summary>

Client-side percentiles (`publishPercentiles(0.99)`) calculate the 99th percentile directly inside the JVM using an HdrHistogram. However, client-side percentiles **cannot be aggregated across multiple microservice replicas** (you cannot average percentiles mathematically). 

Server-side percentiles (`publishPercentileHistogram()`) publish raw bucket counts (`le`), allowing Prometheus to calculate accurate multi-instance global percentiles using `histogram_quantile(0.99, sum(rate(...)) by (le))`.
</details>

---

## 11. Practice Exercises

### Exercise 1: Instrument an Order Fulfillment Counter (Easy)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Create an `OrderMetrics` component that counts fulfilled orders tagged by delivery type (`STANDARD`, `EXPRESS`, `OVERNIGHT`).

**Solution**:
```java
package com.example.observability.metrics;

import io.micrometer.core.instrument.MeterRegistry;
import org.springframework.stereotype.Component;

@Component
public class OrderMetrics {

    private final MeterRegistry registry;

    public OrderMetrics(MeterRegistry registry) {
        this.registry = registry;
    }

    public void incrementFulfilled(String deliveryType) {
        registry.counter("orders.fulfilled.total", "delivery_type", deliveryType.toUpperCase()).increment();
    }
}
```
</details>

### Exercise 2: Instrument an Async Task with Micrometer Timer (Medium)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Implement a method that measures the execution time of a `Supplier<CompletableFuture<String>>` using `Timer.Sample`.

**Solution**:
```java
package com.example.observability.metrics;

import io.micrometer.core.instrument.MeterRegistry;
import io.micrometer.core.instrument.Timer;
import org.springframework.stereotype.Component;

import java.util.concurrent.CompletableFuture;
import java.util.function.Supplier;

@Component
public class AsyncMetricsHelper {

    private final MeterRegistry registry;

    public AsyncMetricsHelper(MeterRegistry registry) {
        this.registry = registry;
    }

    public CompletableFuture<String> instrumentAsync(String taskName, Supplier<CompletableFuture<String>> task) {
        Timer.Sample sample = Timer.start(registry);
        Timer timer = registry.timer("async.task.duration", "task", taskName);

        return task.get().whenComplete((result, throwable) -> {
            sample.stop(timer);
        });
    }
}
```
</details>

### Exercise 3: Implement a Custom MeterFilter to Prevent Cardinality Spikes (Hard)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Write a `MeterFilter` bean that caps the maximum number of unique values for the `endpoint` tag on `http.server.requests` to 50, redirecting excess values to an `OTHER` tag.

**Solution**:
```java
package com.example.observability.config;

import io.micrometer.core.instrument.Tag;
import io.micrometer.core.instrument.config.MeterFilter;
import io.micrometer.core.instrument.config.MeterFilterReply;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;

import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;

@Configuration
public class MetricsGuardrailConfiguration {

    @Bean
    public MeterFilter tagCardinalityLimitFilter() {
        Set<String> observedUris = ConcurrentHashMap.newKeySet();
        final int MAX_UNIQUE_URIS = 50;

        return MeterFilter.transformTags(id -> {
            if ("http.server.requests".equals(id.getName())) {
                return id.replaceTags(id.getTagsAsList().stream()
                    .map(tag -> {
                        if ("uri".equals(tag.getKey())) {
                            if (observedUris.size() < MAX_UNIQUE_URIS) {
                                observedUris.add(tag.getValue());
                                return tag;
                            } else if (observedUris.contains(tag.getValue())) {
                                return tag;
                            } else {
                                return Tag.of("uri", "OTHER_OVERFLOW");
                            }
                        }
                        return tag;
                    })
                    .toList());
            }
            return id;
        });
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+-----------------------------------+-------------------------------------------------------------+
| Micrometer Construct              | Purpose & Best Practice                                     |
+-----------------------------------+-------------------------------------------------------------+
| MeterRegistry                     | Central registry for meter creation and lifecycle          |
| Counter                           | Counts monotonically increasing events (requests, errors)   |
| Timer                             | Measures latency and event count simultaneously             |
| Gauge                             | Samples instantaneous numerical state (queues, pools)       |
| DistributionSummary               | Tracks distribution of non-time quantities (bytes, payloads)|
| ObservationRegistry               | Unified facade for metrics and distributed tracing in Boot 3|
| MeterFilter                       | Guardrail for tag renaming, denying, or capping cardinality |
+-----------------------------------+-------------------------------------------------------------+
```

---

## 13. Further Reading
- Micrometer Official Documentation: Concepts & Registries
- Spring Boot Metrics & Observation Reference
- Prometheus Best Practices: Metric and Label Naming
- Google SRE Book: Monitoring Distributed Systems (The 4 Golden Signals)