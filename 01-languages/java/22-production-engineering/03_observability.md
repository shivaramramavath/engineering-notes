# Observability in Java: Metrics, Tracing, and Health

> Building production-grade observability for enterprise Java: Micrometer metrics instrumentation, OpenTelemetry distributed tracing (W3C Trace Context), and liveness/readiness probes in Kubernetes.

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced / Enterprise Architecture |
| **Prerequisites** | [00_logging](00_logging.md), [14_concurrency](../14-concurrency/README.md), [11_io](../11-io-and-networking/README.md) |
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

### What is Observability?
Observability is the measure of how well the internal execution states of a system can be inferred solely from knowledge of its external telemetry outputs.

In modern distributed microservice architectures, observability is anchored on the **Three Pillars of Observability**:
1. **Metrics**: Aggregable numerical measurements recorded over time intervals (request rates, latency histograms, error counts, memory utilization).
2. **Distributed Tracing**: Tracking the lifecycle and causal path of a request across dozens of microservices, queues, and databases.
3. **Structured Logs**: Context-rich, timestamped event records detailing specific execution anomalies.

### Why Observability is Essential
In distributed cloud environments:
- An issue reported by a user (e.g. "Checkout failed") might involve 15 microservices, 4 databases, and 2 message queues.
- Without distributed tracing, engineers waste hours inspecting unrelated logs across different pods.
- With end-to-end tracing and standardized metrics, root cause identification drops from hours to seconds.

---

## 2. Core Concepts

### Micrometer: The SLF4J of Metrics
Just as SLF4J decouples logging code from the logging implementation, **Micrometer** is the vendor-neutral instrumentation facade for application metrics.
- Program against Micrometer core interfaces: `Counter`, `Gauge`, `Timer`, `DistributionSummary`.
- Export transparently to Prometheus, Datadog, InfluxDB, AWS CloudWatch, or OpenTelemetry.

### Metric Types
- **Counter**: Monotonically increasing value representing event counts (e.g., total HTTP requests, completed orders). Can reset to zero only on process restart.
- **Gauge**: Instantaneous value reflecting current state (e.g., active thread pool count, current queue depth, heap memory usage).
- **Timer**: Measures both short-duration latencies and frequency of events (e.g., HTTP request duration, database query time). Computes percentiles ($p50, p95, p99$) and throughput.
- **Distribution Summary**: Tracks distribution of non-time events (e.g., payload size in bytes).

### Distributed Tracing & W3C Trace Context
Distributed tracing instruments operations via **Spans** grouped into a **Trace**:
- **Trace ID**: 16-byte unique identifier shared across all services handling the request.
- **Span ID**: 8-byte identifier representing a discrete unit of work within a single service.
- **W3C Trace Context**: The standardized HTTP header specification (`traceparent: 00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01`) used to propagate trace context across network boundaries.

---

## 3. How It Works Internally

### OpenTelemetry Distributed Trace Propagation
```
[ Frontend Service ]
   Span: "checkout" (TraceId: 4bf92..., SpanId: 00f06...)
          |
          | HTTP POST /orders
          | Header: traceparent: 00-4bf92...-00f06...-01
          v
[ Order Service ]
   Span: "process-order" (TraceId: 4bf92..., ParentSpanId: 00f06..., SpanId: 5a8e1...)
          |
          | Kafka Message: Header traceparent
          v
[ Payment Service ]
   Span: "charge-card" (TraceId: 4bf92..., ParentSpanId: 5a8e1..., SpanId: c4b2a...)
```

When an HTTP client initiates a request:
1. The OpenTelemetry propagator serializes the current Span context into the `traceparent` HTTP header.
2. The downstream service extracts the `traceparent` header, establishes the caller\'s span as the `parentSpanId`, and creates a new local span.
3. Spans are reported asynchronously to an OpenTelemetry Collector (via gRPC/OTLP) without blocking business threads.

---

## 4. Syntax & Usage

### 1. Registering Micrometer Metrics
```java
import io.micrometer.core.instrument.*;
import io.micrometer.core.instrument.simple.SimpleMeterRegistry;

public class MetricsDemo {
    private final MeterRegistry registry = new SimpleMeterRegistry();
    private final Counter orderCounter;
    private final Timer orderTimer;

    public MetricsDemo() {
        // Counter with dimensional tags
        this.orderCounter = Counter.builder("orders.placed")
                .description("Total number of orders placed")
                .tag("region", "us-east")
                .tag("tier", "premium")
                .register(registry);

        // Timer with SLA percentiles
        this.orderTimer = Timer.builder("orders.processing.time")
                .description("Latency of order execution")
                .publishPercentiles(0.5, 0.95, 0.99)
                .register(registry);
    }
}
```

---

## 5. Practical Examples

### Example 1: Instrumenting Latency and Errors with Micrometer
**Goal:** Measure request execution time, count successes/failures, and record gauges using idiomatic Micrometer.

```java
import io.micrometer.core.instrument.*;
import java.util.concurrent.atomic.AtomicInteger;

public class OrderProcessingService {
    private final MeterRegistry registry;
    private final Counter successCounter;
    private final Counter failureCounter;
    private final Timer processingTimer;
    private final AtomicInteger pendingOrders = new AtomicInteger(0);

    public OrderProcessingService(MeterRegistry registry) {
        this.registry = registry;
        
        this.successCounter = registry.counter("orders.completed", "status", "success");
        this.failureCounter = registry.counter("orders.completed", "status", "failure");
        
        this.processingTimer = Timer.builder("orders.duration")
                .description("Order execution latency")
                .publishPercentileHistogram()
                .register(registry);

        // Gauge monitoring live pending order queue depth
        registry.gauge("orders.pending", pendingOrders, AtomicInteger::get);
    }

    public void processOrder(String orderId) {
        pendingOrders.incrementAndGet();
        Timer.Sample sample = Timer.start(registry);
        try {
            executeBusinessLogic(orderId);
            successCounter.increment();
        } catch (Exception e) {
            failureCounter.increment();
            throw e;
        } finally {
            sample.stop(processingTimer);
            pendingOrders.decrementAndGet();
        }
    }

    private void executeBusinessLogic(String orderId) {
        // Business logic here...
    }
}
```

### Example 2: OpenTelemetry Manual Tracing and Span Context Injection
**Goal:** Create custom child spans, record span attributes, handle exceptions, and propagate W3C traceparent headers.

```java
import io.opentelemetry.api.GlobalOpenTelemetry;
import io.opentelemetry.api.trace.Span;
import io.opentelemetry.api.trace.StatusCode;
import io.opentelemetry.api.trace.Tracer;
import io.opentelemetry.context.Scope;

public class TracingPaymentGateway {
    private static final Tracer TRACER = GlobalOpenTelemetry.getTracer("payment-subsystem", "1.0.0");

    public boolean executePayment(String customerId, double amount) {
        // Start span
        Span span = TRACER.spanBuilder("executePayment")
                .setAttribute("customer.id", customerId)
                .setAttribute("payment.amount", amount)
                .startSpan();

        // Make span active on current thread context
        try (Scope scope = span.makeCurrent()) {
            boolean success = callBankApi(customerId, amount);
            span.setAttribute("payment.success", success);
            span.setStatus(StatusCode.OK);
            return success;
        } catch (Exception ex) {
            span.recordException(ex);
            span.setStatus(StatusCode.ERROR, "Payment gateway communication failure");
            throw ex;
        } finally {
            span.end(); // Span must always be ended!
        }
    }

    private boolean callBankApi(String customerId, double amount) {
        return true;
    }
}
```

### Example 3: Kubernetes Liveness and Readiness Probes in Java
**Goal:** Implement zero-dependency HTTP endpoints satisfying Kubernetes container health contracts.

```java
import com.sun.net.httpserver.HttpServer;
import java.io.IOException;
import java.io.OutputStream;
import java.net.InetSocketAddress;

public class KubernetesHealthServer {
    private volatile boolean isReady = false;

    public void start(int port) throws IOException {
        HttpServer server = HttpServer.create(new InetSocketAddress(port), 0);

        // Liveness Probe: Is the JVM process deadlocked or out of memory?
        server.createContext("/health/liveness", exchange -> {
            byte[] response = "{"status":"UP"}".getBytes();
            exchange.getResponseHeaders().set("Content-Type", "application/json");
            exchange.sendResponseHeaders(200, response.length);
            try (OutputStream os = exchange.getResponseBody()) {
                os.write(response);
            }
        });

        // Readiness Probe: Is the database connected and ready for traffic?
        server.createContext("/health/readiness", exchange -> {
            if (isReady) {
                byte[] response = "{"status":"READY"}".getBytes();
                exchange.getResponseHeaders().set("Content-Type", "application/json");
                exchange.sendResponseHeaders(200, response.length);
                try (OutputStream os = exchange.getResponseBody()) { os.write(response); }
            } else {
                byte[] response = "{"status":"NOT_READY"}".getBytes();
                exchange.getResponseHeaders().set("Content-Type", "application/json");
                exchange.sendResponseHeaders(503, response.length);
                try (OutputStream os = exchange.getResponseBody()) { os.write(response); }
            }
        });

        server.start();
    }

    public void markReady(boolean ready) {
        this.isReady = ready;
    }
}
```

### Example 4: Failure Case — Metric High Cardinality Explosion
```java
// CRITICAL OBSERVABILITY BUG - HIGH CARDINALITY METRIC:
Counter counter = registry.counter("http.requests", 
    "user_email", request.getEmail(), // EXPONENTIALLY EXPLODES METRIC CARDINALITY!
    "url", request.getRequestURI()    // e.g. /users/1029384 -> unique tag per user!
);
// Ingesting 1,000,000 unique tag combinations crashes Prometheus TSDB!
```

---

## 6. Best Practices

1. **Beware of Metric Cardinality**: Never use unique identifiers (User IDs, UUIDs, email addresses, order IDs) as metric tag values. Use tags only for bounded sets (HTTP status codes, HTTP methods, regions).
2. **Use Timers with Percentiles**: Average latency is misleading. Configure `publishPercentiles(0.5, 0.95, 0.99)` or percentile histograms to detect latency outliers.
3. **Always End Spans in `finally` Blocks**: Unclosed spans leak memory and produce broken trace graphs in collectors.
4. **Propagate W3C Trace Headers**: Standardize on `traceparent` headers across HTTP and message broker boundaries.
5. **Differentiate Liveness vs. Readiness**:
   - Liveness fails only if the pod is completely dead (deadlock, fatal crash) and must be restarted by Kubernetes.
   - Readiness fails if the pod is temporarily unable to serve traffic (database warming up, queue full) and should simply be removed from load balancer rotation.
6. **Correlate Logs with Traces**: Automatically inject `trace_id` and `span_id` into SLF4J MDC so log search tools link directly to trace waterfalls.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Secure Fix |
| :--- | :--- | :--- |
| **High Cardinality Tags in Metrics** | Floods TSDB with millions of time series, causing Prometheus out-of-memory crashes. | Keep tag values bounded; put UUIDs in traces/logs. |
| **Using Gauges for Rate Measurements** | Gauges only take instant snapshots and miss intermediate spikes. | Use `Counter` or `Timer` for rate/throughput. |
| **Restarting pods on external service failure** | Failing liveness probe on downstream DB downtime causes cascading restart storms. | Check external dependencies in **Readiness**, not Liveness. |
| **Forgetting to call `span.end()`** | Trace spans remain open indefinitely, leaking memory and producing corrupt traces. | Call `span.end()` inside a `finally` block. |
| **Manual String Concatenation of Metrics** | Hardcoded metric formats break when switching export backends. | Use Micrometer abstraction layer. |
| **Blocking on Telemetry Export** | Synchronously exporting traces/metrics over network stalls business threads. | Use non-blocking async OTLP exporters. |

---

## 8. Performance & Security Notes

### Overhead of Tracing Instrumentation
Creating spans and recording timestamps incurs minimal overhead ($< 1\text{ microsecond}$ per span) when using OpenTelemetry Java agent bytecode instrumentation.
- In ultra-high-volume services ($> 50,000\text{ req/sec}$), configure **Probabilistic Sampling** or **Tail-Based Sampling** (e.g., sample 5% of successful requests, but 100% of errors and slow requests) to prevent saturating trace storage backends.

---

## 9. Comparison / When to Use What

| Observability Component | Data Model | Cost / Retention | Primary Purpose |
| :--- | :--- | :--- | :--- |
| **Metrics (Micrometer / Prometheus)**| Aggregated time-series counters & gauges | Inexpensive / Long-term (1 year) | Alerting, SLA tracking, dashboards |
| **Distributed Traces (OpenTelemetry)** | DAG of timed spans with metadata | Expensive / Short-term (7–14 days) | Pinpointing latency bottlenecks & root causes |
| **Structured Logs (Logback / JSON)** | Discrete event text/JSON records | Moderate / Medium-term (30 days) | Forensic diagnosis, debugging, audit trails |

---

## 10. Interview Questions & Answers

### Q1: What is the difference between a Counter and a Gauge in Micrometer?
<details>
<summary>View Answer</summary>
A `Counter` is a monotonically increasing cumulative metric that represents the count of events (such as total requests served or exceptions thrown). It only ever increases (or resets to zero on application restart). A `Gauge` represents the instantaneous value of an arbitrary variable at the moment it is sampled (such as current queue size, active thread count, or heap memory usage). Gauges can fluctuate up and down arbitrarily.
</details>

### Q2: What is the "Cardinality Explosion" problem in metrics databases?
<details>
<summary>View Answer</summary>
In dimensional metrics systems (Prometheus, Datadog), every unique combination of metric name and key-value tag pairs forms an independent time series in the database. If a developer includes high-cardinality values (such as user IDs, UUIDs, or unbounded URL parameters) as metric tags, the number of active time series explodes into the millions. This exhausts database RAM, severely degrades query performance, and can crash the metrics collector or incur exorbitant cloud telemetry bills.
</details>

### Q3: How do Kubernetes Liveness and Readiness probes differ, and why should external dependencies not be checked in liveness probes?
<details>
<summary>View Answer</summary>
- **Liveness Probe**: Determines if the application process is running and healthy. If it fails, Kubernetes terminates and restarts the container pod.
- **Readiness Probe**: Determines if the application is ready to accept incoming user traffic. If it fails, Kubernetes removes the pod from service load balancers without restarting it.
External dependencies (like remote databases or downstream APIs) must *never* be evaluated in liveness probes. If a database goes down temporarily, all application pods would fail their liveness probes simultaneously, causing Kubernetes to enter a catastrophic cascading restart loop across all pods.
</details>

### Q4: What is the W3C Trace Context standard?
<details>
<summary>View Answer</summary>
W3C Trace Context is a universal specification for distributed tracing context propagation across heterogeneous systems. It defines two mandatory HTTP headers:
1. `traceparent`: Encodes the protocol version, 16-byte `trace_id`, 8-byte `parent_span_id`, and trace flags (such as whether the trace is sampled).
2. `tracestate`: Transmits vendor-specific routing and state metadata across boundaries without collision.
</details>

### Q5: How does OpenTelemetry Context propagation work across asynchronous thread boundaries in Java?
<details>
<summary>View Answer</summary>
OpenTelemetry relies on `io.opentelemetry.context.Context`, which wraps thread-local storage. When dispatching tasks to background executors, thread pools, or `CompletableFuture`, the active context is captured on the initiating thread via `Context.current()`. The task is wrapped using `context.wrap(runnable)` or `Context.current().makeCurrent()`. When the background thread executes the task, it temporarily attaches the captured context, ensuring child spans link correctly to the parent trace.
</details>

### Q6: Why are percentiles ($p95, p99$) preferred over average (mean) latency?
<details>
<summary>View Answer</summary>
Average latency obscures tail latency anomalies. If 99 requests complete in 10ms and 1 request stalls for 10 seconds due to a stop-the-world GC pause, the average latency is ~110ms, which appears acceptable. However, 1% of users experienced a catastrophic 10-second delay. Percentiles ($p99 = 10\text{s}$) explicitly reveal the worst-case experience of users, making them essential for Service Level Objectives (SLOs).
</details>

### Q7: What is OpenTelemetry Baggage?
<details>
<summary>View Answer</summary>
Baggage is contextual key-value metadata that is propagated alongside traces across network boundaries, but unlike Span attributes, Baggage is not automatically indexed on spans. It allows business metadata (e.g., `account_tier=platinum`) to travel with a request across 10 microservices so that downstream services can make routing or priority decisions without re-querying user databases.
</details>

### Q8: What is the difference between Tail-based sampling and Head-based sampling in distributed tracing?
<details>
<summary>View Answer</summary>
- **Head-based sampling**: Decides whether to trace a request at the very beginning of the request lifecycle (e.g. randomly sample 5% of requests). It saves bandwidth early, but might miss rare, critical errors that occur at the end of unsampled requests.
- **Tail-based sampling**: Buffers all spans in memory at an OpenTelemetry Collector until the entire trace completes. The collector evaluates whether the trace encountered an HTTP 500 error, exceeded a latency threshold, or satisfied specific criteria, retaining 100% of errors while discarding mundane successful traces.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a Latency Tracking AutoCloseable Timer
**Task:** Create an `AutoCloseable` helper `TrackedTimer` that starts a Micrometer timer sample on instantiation and automatically records latency upon exiting a try-with-resources block.
<details>
<summary>View Solution</summary>

```java
import io.micrometer.core.instrument.MeterRegistry;
import io.micrometer.core.instrument.Timer;

public class TrackedTimer implements AutoCloseable {
    private final Timer.Sample sample;
    private final Timer timer;

    public TrackedTimer(MeterRegistry registry, String timerName, String... tags) {
        this.sample = Timer.start(registry);
        this.timer = registry.timer(timerName, tags);
    }

    @Override
    public void close() {
        sample.stop(timer);
    }
}

// Usage:
// try (var ignored = new TrackedTimer(registry, "db.query", "table", "users")) {
//     database.query();
// }
```
</details>

### Exercise 2: OpenTelemetry W3C Header Injector
**Task:** Implement a custom header injector that formats the current span into a standard W3C `traceparent` string.
<details>
<summary>View Solution</summary>

```java
import io.opentelemetry.api.trace.Span;
import io.opentelemetry.api.trace.SpanContext;

public class TraceParentInjector {
    public static String formatTraceParent(Span span) {
        SpanContext ctx = span.getSpanContext();
        if (!ctx.isValid()) {
            return "";
        }
        return String.format("00-%s-%s-%s",
                ctx.getTraceId(),
                ctx.getSpanId(),
                ctx.getTraceFlags().asHex());
    }
}
```
</details>

### Exercise 3: Resilient Composite Health Check Aggregator
**Task:** Write a simple composite readiness aggregator that evaluates a set of registered health indicator lambdas and returns HTTP 200 only if all report healthy.
<details>
<summary>View Solution</summary>

```java
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.function.BooleanSupplier;

public class HealthAggregator {
    private final Map<String, BooleanSupplier> checks = new ConcurrentHashMap<>();

    public void registerCheck(String name, BooleanSupplier check) {
        checks.put(name, check);
    }

    public boolean isAllHealthy() {
        for (Map.Entry<String, BooleanSupplier> entry : checks.entrySet()) {
            try {
                if (!entry.getValue().getAsBoolean()) {
                    return false;
                }
            } catch (Exception ex) {
                return false;
            }
        }
        return true;
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Three Pillars**: Metrics (numeric over time), Traces (causal waterfalls), Logs (event context).
- **Micrometer**: Vendor-neutral facade for Counters, Gauges, and Timers.
- **Percentiles**: Monitor $p95$ and $p99$ with `publishPercentiles()` rather than average latency.
- **Avoid Cardinality Disasters**: Never use UUIDs, emails, or unbounded values in metric tags.
- **OpenTelemetry**: Standardize on spans and W3C `traceparent` headers across microservices.
- **Probes**: Liveness restarts containers on deadlocks; Readiness decouples traffic on dependencies.

---

## 13. Further Reading
- Micrometer Documentation: [Official Micrometer Concepts & Guides](https://micrometer.io/docs/concepts)
- OpenTelemetry Java: [OpenTelemetry Java SDK Documentation](https://opentelemetry.io/docs/languages/java/)
- W3C Recommendation: [Trace Context Specification](https://www.w3.org/TR/trace-context/)
- Kubernetes Documentation: [Configure Liveness, Readiness and Startup Probes](https://kubernetes.io/docs/tasks/configure-pod-container/configure-liveness-readiness-startup-probes/)
