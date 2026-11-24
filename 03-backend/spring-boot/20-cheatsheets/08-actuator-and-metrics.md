# Spring Boot Actuator & Micrometer Metrics Cheatsheet

| Metadata | Details |
|---|---|
| **Topic** | Actuator Endpoints & Micrometer Metrics Cheatsheet |
| **Track** | Java Spring Boot |
| **Difficulty** | Intermediate |
| **Prerequisites** | Java 21, Observability, Prometheus, Grafana |
| **Java Version** | Java 21 / 25 LTS |
| **Spring Boot** | 3.4.x / 4.x |
| **Tested With** | Micrometer 1.14+, Prometheus, Micrometer Tracing |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [Internals & Architecture](#3-internals--architecture)
4. [Syntax & Categorized Reference](#4-syntax--categorized-reference)
5. [5 Practical Production Examples](#5-5-practical-production-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes to Avoid](#7-common-mistakes-to-avoid)
8. [Performance & Security Considerations](#8-performance--security-considerations)
9. [Comparison Table: Meter Types](#9-comparison-table-meter-types)
10. [Technical Interview Questions & Answers](#10-technical-interview-questions--answers)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading & References](#13-further-reading--references)

---

## 1. What & Why
Spring Boot Actuator brings production-ready monitoring, metrics gathering, and operational introspection to Spring applications. Powered by Micrometer—the dimensional metrics facade for Java—it formats internal application health, JVM garbage collection, thread pools, and business telemetry for consumption by Prometheus, Datadog, and Grafana.

This cheatsheet outlines endpoint exposures, health indicator implementations, Micrometer meter instrumentation, and Kubernetes readiness/liveness probe configurations.

---

## 2. Core Concepts
- **Actuator Endpoints**: Built-in HTTP and JMX inspection points (`/actuator/health`, `/actuator/metrics`, `/actuator/prometheus`).
- **Health Indicators**: Declarative components reporting operational status (`UP`, `DOWN`, `OUT_OF_SERVICE`).
- **Kubernetes Probes**: Native liveness (`/actuator/health/liveness`) and readiness (`/actuator/health/readiness`) checks.
- **Micrometer Meters**: Dimensional instruments: `Counter` (monotonically increasing), `Timer` (latencies), `Gauge` (instantaneous snapshot), and `DistributionSummary` (payload sizes).
- **Tags & Dimensions**: Key-value metadata attached to meters enabling Prometheus slice-and-dice queries.

---

## 3. Internals & Architecture
Micrometer MeterRegistry & Actuator Pipeline:

```
[Application Code / Spring MVC / HikariCP]
                 |
                 v
+-------------------------------------------------------------+
|                     MeterRegistry Facade                    |
|  1. Counter.builder("http.server.requests")                 |
|  2. Timer.Sample.start(registry) -> stop(timer)            |
|  3. Tags ("uri", "/orders", "status", "200")                |
+-------------------------------------------------------------+
                 |
                 v
+-------------------------------------------------------------+
|                PrometheusMeterRegistry                      |
|  - In-memory dimensional sample stores                      |
|  - Aggregates percentiles, sum, and count                   |
+-------------------------------------------------------------+
                 |
                 v
+-------------------------------------------------------------+
|               Actuator Prometheus Endpoint                  |
|               GET /actuator/prometheus                      |
|  - Converts meters to OpenMetrics / Prometheus text format   |
+-------------------------------------------------------------+
                 |
                 v
[Prometheus Server Scraping (e.g. Every 15s) -> Grafana Dashboard]
```

---

## 4. Syntax & Categorized Reference

### 4.1 Production Actuator Configuration (application.yaml)
```yaml
management:
  server:
    port: 9090 # Run actuator on isolated management port
  endpoints:
    web:
      base-path: /actuator
      exposure:
        include: health,info,prometheus,metrics
  endpoint:
    health:
      show-details: when_authorized
      probes:
        enabled: true
      group:
        readiness:
          include: readinessState,db,redis
        liveness:
          include: livenessState
  metrics:
    tags:
      application: ${spring.application.name}
      environment: ${ENVIRONMENT:production}
    distribution:
      percentiles-histogram:
        http.server.requests: true
```

### 4.2 Standard Built-in Actuator Endpoints
| Endpoint | Method | Purpose |
|---|---|---|
| `/actuator/health` | GET | Aggregated health status (UP/DOWN) |
| `/actuator/health/liveness` | GET | Kubernetes liveness probe (checks internal deadlock) |
| `/actuator/health/readiness` | GET | Kubernetes readiness probe (checks DB & downstream connections) |
| `/actuator/prometheus` | GET | OpenMetrics / Prometheus scrape endpoint |
| `/actuator/metrics` | GET | Lists available metric names |
| `/actuator/metrics/{name}` | GET | Details of a specific metric (tags, sum, count) |
| `/actuator/info` | GET | Application build metadata (git commit, version) |

---

## 5. 5 Practical Production Examples

### Example 1: Custom Production HealthIndicator
```java
@Component
public class PaymentGatewayHealthIndicator implements HealthIndicator {

    private final RestClient restClient;

    public PaymentGatewayHealthIndicator(RestClient.Builder builder,
                                         @Value("${payment.gateway.url}") String gatewayUrl) {
        this.restClient = builder.baseUrl(gatewayUrl).build();
    }

    @Override
    public Health health() {
        try {
            long start = System.currentTimeMillis();
            restClient.get().uri("/ping").retrieve().toBodilessEntity();
            long latencyMs = System.currentTimeMillis() - start;

            return Health.up()
                    .withDetail("gateway", "Stripe API")
                    .withDetail("latencyMs", latencyMs)
                    .build();
        } catch (Exception ex) {
            return Health.down()
                    .withDetail("gateway", "Stripe API")
                    .withDetail("error", ex.getMessage())
                    .build();
        }
    }
}
```

### Example 2: Micrometer Counter for Business Events
```java
@Service
public class OrderEventService {

    private final Counter successfulOrdersCounter;
    private final Counter failedOrdersCounter;

    public OrderEventService(MeterRegistry registry) {
        this.successfulOrdersCounter = Counter.builder("business.orders.completed")
                .description("Total number of successfully processed orders")
                .tag("service", "order-service")
                .register(registry);

        this.failedOrdersCounter = Counter.builder("business.orders.failed")
                .description("Total number of failed order checkouts")
                .register(registry);
    }

    public void recordSuccess() {
        successfulOrdersCounter.increment();
    }

    public void recordFailure(String reason) {
        failedOrdersCounter.increment();
    }
}
```

### Example 3: Measuring Latency with Micrometer Timer
```java
@Service
public class ReportGenerator {

    private final Timer reportGenerationTimer;

    public ReportGenerator(MeterRegistry registry) {
        this.reportGenerationTimer = Timer.builder("reports.generation.time")
                .description("Time taken to assemble and render PDF reports")
                .publishPercentiles(0.5, 0.95, 0.99)
                .register(registry);
    }

    public byte[] generateReport(UUID reportId) {
        return reportGenerationTimer.record(() -> {
            // Intensive computation
            return assemblePdf(reportId);
        });
    }

    private byte[] assemblePdf(UUID id) {
        return new byte[1024];
    }
}
```

### Example 4: Micrometer Gauge for In-Memory Queue Size
```java
@Component
public class QueueMonitor {

    private final BlockingQueue<Task> taskQueue = new LinkedBlockingQueue<>();

    public QueueMonitor(MeterRegistry registry) {
        Gauge.builder("system.task.queue.size", taskQueue, Collection::size)
                .description("Current number of pending tasks in memory")
                .register(registry);
    }

    public void enqueue(Task task) {
        taskQueue.offer(task);
    }
}
```

### Example 5: Custom Micrometer Observation with Tracing
```java
@Service
public class ShippingService {

    private final ObservationRegistry observationRegistry;

    public ShippingService(ObservationRegistry observationRegistry) {
        this.observationRegistry = observationRegistry;
    }

    public ShippingLabel createShippingLabel(UUID orderId) {
        return Observation.createNotStarted("shipping.label.create", observationRegistry)
                .lowCardinalityKeyValue("orderType", "EXPEDITED")
                .observe(() -> executeShippingProviderCall(orderId));
    }

    private ShippingLabel executeShippingProviderCall(UUID orderId) {
        return new ShippingLabel(orderId, "TRACK-12345");
    }
}
```

---

## 6. Best Practices
1. **Isolate Management Port**: Configure `management.server.port=9090` so internal metrics endpoints are inaccessible from public ingress load balancers.
2. **Avoid High-Cardinality Tags**: Never use user IDs, timestamps, or arbitrary UUIDs as metric tag values; this exhausts Prometheus memory.
3. **Use Kubernetes Probes Correctly**: Never include external dependencies in the Liveness probe; only use external checks in the Readiness probe.
4. **Enable Percentile Histograms**: Configure `management.metrics.distribution.percentiles-histogram` for SLI/SLO monitoring.
5. **Use Gauges for Instantaneous Quantities**: Track active connections, thread pool capacity, or queue depths with `Gauge`.

---

## 7. Common Mistakes to Avoid

| Mistake | Consequence | Remedy |
|---|---|---|
| Tagging metrics with UUIDs/user IDs | Explodes metric cardinality, crashing Prometheus server | Restrict tags to bounded enums and fixed categories |
| Putting database checks in Liveness probe | DB restart causes Kubernetes to prematurely kill application pods in crash loop | Place database health indicators in Readiness probe group |
| Exposing all actuator endpoints to the public | Leaks passwords, env vars, thread dumps, and heap memory | Restrict `exposure.include: health,info,prometheus` |
| Calling slow remote APIs inside synchronous health checks | Slows down actuator response and triggers false timeouts | Use aggressive timeouts or cached health statuses |
| Re-registering meters dynamically on every request | Causes memory leaks and garbage collection pressure | Register meters once during bean initialization |
| Forgetting to enable `probes.enabled: true` | Kubernetes defaults to `/actuator/health` without distinguishing readiness/liveness | Explicitly enable `management.endpoint.health.probes.enabled=true` |

---

## 8. Performance & Security Considerations
- **Prometheus Scraping Overhead**: A high scrape frequency (e.g. 1s) consumes CPU. Standardize on 15s to 30s intervals.
- **Security Authorization**: Secure actuator endpoints using Spring Security: `.requestMatchers("/actuator/**").hasRole("OPS_ADMIN")`.
- **Health Caching**: Configure `management.endpoint.health.cache.time-to-live: 10s` to prevent denial-of-service from frequent health pollers.

---

## 9. Comparison Table: Meter Types

| Meter Type | Purpose | Behavior | Prometheus Type |
|---|---|---|---|
| `Counter` | Monotonically increasing count | Only increases (reset to 0 on restart) | Counter |
| `Gauge` | Instantaneous current value | Can fluctuate up and down | Gauge |
| `Timer` | Duration and short task event rate | Computes count, total time, and max | Summary / Histogram |
| `DistributionSummary` | Distribution of non-time values | Tracks payload sizes, item counts | Histogram |

---

## 10. Technical Interview Questions & Answers

<details>
<summary>1. What is the fundamental difference between Liveness and Readiness probes?</summary>
The **Liveness probe** detects if the application is running (e.g. deadlocks, internal crashes). If it fails, Kubernetes restarts the pod. The **Readiness probe** detects if the application is ready to accept HTTP traffic (e.g. database connected, warm caches). If it fails, Kubernetes removes the pod from the service load balancer without killing it.
</details>

<details>
<summary>2. Why are high-cardinality tags dangerous in Micrometer?</summary>
Each unique combination of tag key-values creates an independent time series in Prometheus. Adding UUIDs or user IDs creates millions of distinct metric series, exhausting memory and crashing the Prometheus server.
</details>

<details>
<summary>3. What is the role of Micrometer Observation API in Spring Boot 3?</summary>
The Observation API provides a unified abstraction that records both metrics (via Micrometer) and distributed traces (via OpenTelemetry / Brave) with a single instrumentation point.
</details>

<details>
<summary>4. How does `management.server.port` enhance security?</summary>
By binding actuator endpoints to a distinct network port (e.g. 9090), infrastructure teams can block port 9090 at the external ingress or load balancer while leaving port 8080 open for public API traffic.
</details>

<details>
<summary>5. How do you implement a custom HealthIndicator?</summary>
Implement the `HealthIndicator` interface, override the `health()` method, and return `Health.up()` or `Health.down()` enriched with contextual metadata details.
</details>

<details>
<summary>6. What is the difference between a `Counter` and a `Gauge`?</summary>
A `Counter` records values that only increment over time (e.g. total HTTP requests, error counts). A `Gauge` samples instantaneous values that can rise and fall (e.g. active thread count, memory usage).
</details>

<details>
<summary>7. How do you enable Prometheus scraping in Spring Boot?</summary>
Add the `micrometer-registry-prometheus` dependency and configure `management.endpoints.web.exposure.include=health,prometheus`.
</details>

<details>
<summary>8. How does `Timer.Sample` measure latency across asynchronous boundaries?</summary>
Start a sample with `Timer.start(registry)`, execute the asynchronous operation, and when the task completes (e.g. in a callback or future), invoke `sample.stop(timer)`.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary>Exercise 1: Configure isolated actuator port and probes</summary>
Write an `application.yaml` snippet configuring port 9090 and enabling Kubernetes readiness and liveness probe groups.
</details>

<details>
<summary>Exercise 2: Implement a custom Redis health indicator</summary>
Write a `HealthIndicator` that checks Redis connectivity with a 1000ms timeout and reports status UP or DOWN.
</details>

<details>
<summary>Exercise 3: Instrument an order payment timer</summary>
Use `Timer.builder()` to measure the latency of a third-party payment gateway call with percentiles 0.95 and 0.99.
</details>

<details>
<summary>Exercise 4: Create a gauge monitoring an in-memory cache size</summary>
Register a `Gauge` in a service monitoring the current element count of a `ConcurrentHashMap`.
</details>

<details>
<summary>Exercise 5: Add common metric tags</summary>
Configure `management.metrics.tags` in YAML to append `region: us-east-1` and `service: customer-api` to all emitted metrics.
</details>

<details>
<summary>Exercise 6: Test health endpoint with MockMvc</summary>
Write an integration test asserting that `GET /actuator/health` returns HTTP 200 and JSON containing `"status": "UP"`.
</details>

---

## 12. Summary & Cheat Sheet

```
+---------------------------------------------------------------------------------+
|                   ACTUATOR & METRICS CHEAT SHEET                                |
+---------------------------------------------------------------------------------+
| PORT ISOLATION | management.server.port=9090                                    |
| EXPOSURE       | management.endpoints.web.exposure.include=health,prometheus     |
| PROBES         | management.endpoint.health.probes.enabled=true                 |
| CUSTOM HEALTH  | public Health health() { return Health.up().build(); }          |
| COUNTER        | Counter.builder("name").tag("k", "v").register(reg).increment()|
| TIMER          | timer.record(() -> executeBusinessTask());                      |
| GAUGE          | Gauge.builder("queue.size", queue, Collection::size).register() |
+---------------------------------------------------------------------------------+
```

---

## 13. Further Reading & References
- [Spring Boot Actuator Official Reference](https://docs.spring.io/spring-boot/docs/current/reference/html/actuator.html)
- [Micrometer Concepts & Documentation](https://micrometer.io/docs/concepts)
- [Prometheus Best Practices: Metric and Label Naming](https://prometheus.io/docs/practices/naming/)
- [Kubernetes Container Probes Guide](https://kubernetes.io/docs/tasks/configure-pod-container/configure-liveness-readiness-startup-probes/)
