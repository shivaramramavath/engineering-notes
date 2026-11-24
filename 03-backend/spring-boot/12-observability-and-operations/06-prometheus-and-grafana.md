# Prometheus and Grafana Integration

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
An application running in production emits thousands of discrete data points every second: CPU utilization, GC pause times, active database connections, HTTP request rates, and downstream latency distributions. Without a dedicated time-series monitoring stack:
- Operators cannot visualize historical trends (e.g., gradual memory leaks over weeks).
- Engineering teams cannot establish Service Level Objectives (SLOs) or calculate accurate 95th/99th percentile response times.
- Automated alerts cannot proactively notify on-call engineers before system outages occur.

### Historical Evolution
Before Prometheus, systems relied on push-based metrics architectures like Graphite, Ganglia, or StatsD. Push systems suffered from high network fan-in bottlenecks when thousands of servers flooded centralized collectors.

Prometheus revolutionized monitoring using a **pull-based scraping model** combined with a multi-dimensional data model (labels) and the powerful **PromQL** query language. 

In Spring Boot, Micrometer's `micrometer-registry-prometheus` automatically translates all internal Spring Boot meters into Prometheus exposition format at `/actuator/prometheus`. Coupled with Grafana, this provides an industry-standard, vendor-neutral observability powerhouse.

### Production Relevance
The Prometheus + Grafana stack provides:
- Real-time visualization of the **Four Golden Signals**: Latency, Traffic, Errors, and Saturation.
- Comprehensive JVM diagnostics: Garbage collection pause frequencies, metaspace utilization, and thread states.
- Connection pool visibility: HikariCP active vs idle connections and connection acquisition timeout rates.

---

## 2. Core Concepts

### Architectural Foundations
1. **Prometheus Scraping**: A pull-based HTTP mechanism where Prometheus periodically queries `/actuator/prometheus` (e.g. every 15s) and stores the resulting time-series into its local TSDB (Time Series Database).
2. **Prometheus Exposition Format**: Plaintext text-based protocol containing metric names, label key-values, and 64-bit float values (e.g., `http_server_requests_seconds_count{method="GET",status="200"} 1420`).
3. **PromQL**: Prometheus Query Language used to calculate rates, aggregations, percentiles, and alerts.
4. **Grafana Dashboards**: Visual frontend querying Prometheus to render graphs, gauges, heatmaps, and alert notifications.
5. **Histogram Buckets (`le` tag)**: Cumulative counter buckets used to calculate exact server-side percentiles (`histogram_quantile(0.99, ...)`).

---

## 3. How It Works Internally

### Scrape Pipeline & Visualization Architecture

```
+------------------------------------------------------------------------------------+
|                         SPRING BOOT 3 APPLICATION                                  |
|                                                                                    |
|  Micrometer Meters (Timer, Counter, Gauge, HikariCP, JVM)                          |
|                             |                                                      |
|                             v                                                      |
|  PrometheusMeterRegistry converts meters to Prometheus text format                 |
|                             |                                                      |
|  Exposed at: GET /actuator/prometheus (Port 9090)                                 |
+------------------------------------------------------------------------------------+
                                ^
                                | (Scrapes every 15 seconds)
+------------------------------------------------------------------------------------+
|                         PROMETHEUS SERVER                                          |
|                                                                                    |
|  1. Scrape Engine reads HTTP response                                              |
|  2. Appends scrape metadata (instance, job)                                        |
|  3. Writes to Time Series Database (TSDB) storage engine                          |
|  4. Evaluates Alerting Rules continuously                                          |
+------------------------------------------------------------------------------------+
                                ^
                                | (PromQL Queries: rate, sum, histogram_quantile)
+------------------------------------------------------------------------------------+
|                         GRAFANA DASHBOARD                                          |
|                                                                                    |
|  - Renders JVM Memory, CPU, and Thread Panels                                      |
|  - Displays P95 / P99 HTTP Latency Heatmaps                                        |
|  - Shows HikariCP Active Connection Graphs                                         |
+------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Configuration Reference

### Dependencies (`pom.xml`)
```xml
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-starter-actuator</artifactId>
</dependency>
<dependency>
    <groupId>io.micrometer</groupId>
    <artifactId>micrometer-registry-prometheus</artifactId>
</dependency>
```

### Application Configuration (`application.yml`)
```yaml
management:
  server:
    port: 9090 # Isolated management port
  endpoints:
    web:
      exposure:
        include: [ "health", "info", "prometheus" ]
  metrics:
    tags:
      application: ${spring.application.name}
      environment: production
    distribution:
      percentiles-histogram:
        http.server.requests: true # Emits _bucket metrics for PromQL quantiles
      sla:
        http.server.requests: 50ms, 100ms, 250ms, 500ms, 1s
```

### Prometheus Scrape Configuration (`prometheus.yml`)
```yaml
global:
  scrape_interval: 15s
  evaluation_interval: 15s

scrape_configs:
  - job_name: "spring-boot-apps"
    metrics_path: "/actuator/prometheus"
    static_configs:
      - targets: ["order-service:9090", "payment-service:9090"]
```

---

## 5. Practical Examples

### Example 1: Essential Production PromQL Queries

```promql
# 1. Total Request Rate (Requests Per Second across all instances)
sum(rate(http_server_requests_seconds_count[1m])) by (application, uri)

# 2. HTTP 5xx Error Rate Percentage
(
  sum(rate(http_server_requests_seconds_count{status=~"5.."}[1m]))
  /
  sum(rate(http_server_requests_seconds_count[1m]))
) * 100

# 3. 95th Percentile Response Time (Server-side PromQL Quantile)
histogram_quantile(0.95, sum(rate(http_server_requests_seconds_bucket[5m])) by (le, uri))

# 4. HikariCP Connection Pool Saturation Ratio
hikaricp_connections_active / (hikaricp_connections_active + hikaricp_connections_idle)

# 5. JVM Garbage Collection Pause Rate (seconds spent pausing per second)
sum(rate(jvm_gc_pause_seconds_sum[1m])) by (action, cause)

# 6. JVM Heap Memory Utilization Percentage
(jvm_memory_used_bytes{area="heap"} / jvm_memory_max_bytes{area="heap"}) * 100
```

### Example 2: Configuring Custom Prometheus Alert Rules (`alerts.yml`)
```yaml
groups:
  - name: spring-boot-alerts
    rules:
      - alert: HighHttpErrorRate
        expr: (sum(rate(http_server_requests_seconds_count{status=~"5.."}[2m])) / sum(rate(http_server_requests_seconds_count[2m]))) > 0.05
        for: 2m
        labels:
          severity: critical
        annotations:
          summary: "High HTTP 5xx error rate on {{ $labels.application }}"
          description: "Service {{ $labels.application }} has >5% errors over the last 2 minutes."

      - alert: DatabasePoolExhaustion
        expr: (hikaricp_connections_active / (hikaricp_connections_active + hikaricp_connections_idle)) > 0.90
        for: 1m
        labels:
          severity: warning
        annotations:
          summary: "HikariCP pool near saturation on {{ $labels.instance }}"
          description: "Database active connections exceeded 90% of pool capacity."

      - alert: GcPauseSpike
        expr: sum(rate(jvm_gc_pause_seconds_sum[2m])) > 0.4
        for: 1m
        labels:
          severity: critical
        annotations:
          summary: "Severe JVM GC pause on {{ $labels.instance }}"
          description: "JVM is spending >40% of wall-clock time in Stop-The-World GC pauses."
```

### Example 3: Custom Meter Binder for Business Domain
```java
package com.example.observability.metrics;

import io.micrometer.core.instrument.Gauge;
import io.micrometer.core.instrument.MeterRegistry;
import io.micrometer.core.instrument.binder.MeterBinder;
import org.springframework.stereotype.Component;

import java.util.concurrent.atomic.AtomicLong;

@Component
public class SubscriptionMetricsBinder implements MeterBinder {

    private final AtomicLong activeSubscriptions = new AtomicLong(0);

    public void updateActiveSubscriptions(long count) {
        this.activeSubscriptions.set(count);
    }

    @Override
    public void bindTo(MeterRegistry registry) {
        Gauge.builder("business.subscriptions.active", activeSubscriptions, AtomicLong::get)
            .description("Total number of active paying subscriptions")
            .baseUnit("subscriptions")
            .tag("tier", "enterprise")
            .register(registry);
    }
}
```

### Example 4: Anti-Pattern vs. Production Fix (Scrape Failures on Monolithic Instances)
```java
// ANTI-PATTERN: Heavy, dynamic, un-cached Gauge that runs a SQL query during the Prometheus scrape
// Causes: Every 15s Prometheus scrape triggers a slow SELECT COUNT(*) across 10 million rows!
// Result: Database CPU spikes to 100%, Prometheus times out after 10s -> Scrape fails!
@Component
public class BadDatabaseGauge {
    private final JdbcTemplate jdbcTemplate;

    public BadDatabaseGauge(MeterRegistry registry, JdbcTemplate jdbcTemplate) {
        this.jdbcTemplate = jdbcTemplate;
        // ANTI-PATTERN: Direct query executed ON EVERY SCRAPE!
        registry.gauge("database.total.users", this, 
            b -> jdbcTemplate.queryForObject("SELECT COUNT(*) FROM users", Long.class));
    }
}

// PRODUCTION FIX: Asynchronous background cache for heavy gauges
@Component
public class ResilientDatabaseGauge {
    private final JdbcTemplate jdbcTemplate;
    private final AtomicLong cachedUserCount = new AtomicLong(0);

    public ResilientDatabaseGauge(MeterRegistry registry, JdbcTemplate jdbcTemplate) {
        this.jdbcTemplate = jdbcTemplate;
        // SAFE: Reads cached in-memory AtomicLong instantly during scrape (zero DB load)
        registry.gauge("database.total.users", cachedUserCount, AtomicLong::get);
    }

    @Scheduled(fixedRate = 60000) // Refreshes once per minute in background
    public void refreshGaugeInBackground() {
        try {
            Long count = jdbcTemplate.queryForObject("SELECT COUNT(*) FROM users", Long.class);
            if (count != null) {
                cachedUserCount.set(count);
            }
        } catch (Exception ex) {
            // Log and retain last known good value
        }
    }
}
```

### Example 5: Production Grafana Dashboard JSON Snippet (JVM Memory Panel)
```json
{
  "title": "JVM Heap Memory Usage",
  "type": "timeseries",
  "datasource": "Prometheus",
  "targets": [
    {
      "expr": "jvm_memory_used_bytes{area=\"heap\", application=\"$application\"}",
      "legendFormat": "Used Heap - {{instance}}"
    },
    {
      "expr": "jvm_memory_committed_bytes{area=\"heap\", application=\"$application\"}",
      "legendFormat": "Committed Heap - {{instance}}"
    },
    {
      "expr": "jvm_memory_max_bytes{area=\"heap\", application=\"$application\"}",
      "legendFormat": "Max Heap - {{instance}}"
    }
  ],
  "fieldConfig": {
    "defaults": {
      "unit": "bytes"
    }
  }
}
```

---

## 6. Best Practices & Production Guidelines

1. **Enable Percentiles Histograms for Key Endpoints**: Always configure `percentiles-histogram: true` for `http.server.requests`. Prometheus calculates true global percentiles across replicas using histogram buckets (`_bucket` with `le` tags).
2. **Never Average Percentiles**: In Grafana dashboards, never use `avg(http_server_requests_seconds{quantile="0.95"})`. Averages of percentiles are mathematically invalid. Always calculate quantiles over histograms using `histogram_quantile(0.95, sum(rate(...)) by (le))`.
3. **Use Scrape Intervals Between 10s and 30s**: A 15-second scrape interval offers the optimal balance between high-resolution alerting and Prometheus storage footprint.
4. **Scrape via Internal Port 9090**: Prometheus should scrape `management.server.port: 9090` over internal cluster networks, keeping metrics endpoints shielded from public internet ingress.
5. **Decouple Metric Gauges from Heavy I/O**: A Prometheus scrape must complete within 2–5 seconds. If a gauge executes a slow database query or external REST call, it can cause scrape timeouts. Cache gauge values asynchronously.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Calculating average of percentiles in Grafana | Mathematically invalid; skews latency spikes and masks true tail latency. | Use `histogram_quantile()` on histogram buckets. |
| Forgetting `micrometer-registry-prometheus` dependency | `/actuator/prometheus` returns HTTP 404 because the Prometheus exposition endpoint is not registered. | Add `micrometer-registry-prometheus` to `pom.xml`. |
| Unbounded scrape response size (Cardinality explosion) | Scraping `/actuator/prometheus` downloads a 100MB payload, exceeding scrape timeouts and crashing Prometheus. | Enforce low-cardinality tags; use `MeterFilter` to limit maximum series. |
| Slow Gauge executing synchronous DB queries | Scrape exceeds `scrape_timeout: 10s`, resulting in missing metrics and false alerts. | Compute heavy gauge values on a background `@Scheduled` thread. |
| Missing `application` common tag | Difficult to distinguish metrics when multiple distinct microservices report to the same Prometheus server. | Set `management.metrics.tags.application: ${spring.application.name}`. |
| Not monitoring connection pool timeouts | HikariCP pool exhaustion occurs silently until user requests fail with connection timeouts. | Add Prometheus alerts for `hikaricp_connections_pending > 0`. |

---

## 8. Performance & Security Notes

### Prometheus Scrape Performance
- The Prometheus endpoint serializes meters into plaintext text format on demand. For an application with 2,000 active meters, generating the exposition output takes under 10 milliseconds and consumes negligible CPU.
- Ensure that gzip compression is enabled if scraping over WAN networks (`server.compression.enabled=true`).

### Security & Access Control
- The `/actuator/prometheus` endpoint reveals internal architectural details (database pool sizes, JVM versions, cache hit rates, internal hostnames).
- Shield port 9090 behind Kubernetes NetworkPolicies or mutual TLS (mTLS), or require HTTP Basic/Bearer authentication with a dedicated Prometheus scraper service account.

---

## 9. Comparison / When to Use What

| Monitoring Feature | Prometheus + Grafana | AWS CloudWatch | Datadog / Dynatrace |
|---|---|---|---|
| **Architecture** | Pull-based (Scraped) | Push-based | Push-based (Agent daemon) |
| **Hosting Model** | Self-hosted / Open Source | Managed AWS native | Commercial SaaS |
| **Query Language** | PromQL (High flexibility) | CloudWatch Insights / Math | Proprietary query language |
| **Cost Model** | Infrastructure / Storage cost | Pay-per-metric & query | Pay-per-host / per-metric |
| **Spring Boot Support** | Native first-class | Via CloudWatch MeterRegistry | Via Datadog MeterRegistry |

---

## 10. Interview Questions & Answers

### Q1: How does Prometheus scrape metrics from Spring Boot?
<details>
<summary>Click to view answer</summary>

Prometheus uses a **pull model**. At periodic intervals (e.g. every 15s), the Prometheus server executes an HTTP `GET` request against the application's `/actuator/prometheus` endpoint. The Spring Boot application, powered by `PrometheusMeterRegistry`, formats all registered Micrometer meters into the standard Prometheus plaintext exposition format and streams the response back to Prometheus.
</details>

### Q2: Why is `avg(http_server_requests_seconds{quantile="0.95"})` an anti-pattern in Grafana?
<details>
<summary>Click to view answer</summary>

Percentiles are non-linear statistical distributions; you cannot mathematically average percentiles across multiple instances or time windows (e.g., the average of the 95th percentiles of two servers is not the 95th percentile of the combined traffic). The correct approach is to export histogram buckets (`percentiles-histogram: true`) and use PromQL's `histogram_quantile(0.95, sum(rate(..._bucket[5m])) by (le))` function.
</details>

### Q3: What is the difference between `_count`, `_sum`, and `_bucket` in Prometheus timers?
<details>
<summary>Click to view answer</summary>

When a Micrometer `Timer` is exported with histogram support:
- `_count`: The total number of events recorded (used to calculate request rate).
- `_sum`: The cumulative total duration of all events (used with `_count` to calculate average latency: `rate(_sum) / rate(_count)`).
- `_bucket`: A series of counters with an `le` (less-than-or-equal) label tracking how many events fell into specific latency buckets (e.g. $\le 100\text{ms}, \le 500\text{ms}$), used for calculating quantiles.
</details>

### Q4: How do you monitor HikariCP connection pool health using Prometheus?
<details>
<summary>Click to view answer</summary>

Spring Boot automatically binds HikariCP metrics when Actuator and Micrometer are present. Key metrics include:
- `hikaricp_connections_active`: Connections currently in use by active transactions.
- `hikaricp_connections_idle`: Connections waiting in the pool.
- `hikaricp_connections_pending`: Threads currently blocked waiting for a connection (indicates pool exhaustion!).
- `hikaricp_connections_timeout_total`: Count of connection acquisition timeouts.
</details>

### Q5: What does the PromQL `rate()` function do and why is it preferred over `irate()`?
<details>
<summary>Click to view answer</summary>

- `rate(v[range])`: Calculates the per-second average rate of increase of a counter over a specified time window (e.g. `[5m]`). It smooths out transient spikes and is resilient to counter resets (restarts), making it ideal for alerting and trend analysis.
- `irate(v[range])`: Calculates the instantaneous rate using only the last two data points within the window. While responsive to immediate spikes, it is too volatile for alerting rules.
</details>

### Q6: How do you identify a memory leak in Spring Boot using Grafana?
<details>
<summary>Click to view answer</summary>

Look at the `jvm_memory_used_bytes{area="heap"}` graph over several days. In a healthy JVM, memory forms a "sawtooth" pattern: memory rises as objects allocate and drops back to a consistent baseline after major/full GC. In a memory leak, the **post-GC baseline continuously climbs upward** until it reaches `jvm_memory_max_bytes`, resulting in frequent GC thrashing and eventually an `OutOfMemoryError`.
</details>

### Q7: Can Prometheus scrape Spring Boot if Actuator is on an isolated port?
<details>
<summary>Click to view answer</summary>

Yes. In `prometheus.yml`, configure the scrape job target to point directly to the management port (e.g. `order-service:9090`) and set `metrics_path: /actuator/prometheus`. In Kubernetes, this can be automated using Prometheus Operator `PodMonitor` or `ServiceMonitor` Custom Resources targeting the management port name.
</details>

### Q8: What is the "Four Golden Signals" framework by Google SRE?
<details>
<summary>Click to view answer</summary>

The Four Golden Signals are:
1. **Latency**: Time taken to service a request (e.g., P95/P99 HTTP duration).
2. **Traffic**: Demand placed on the system (e.g., requests per second).
3. **Errors**: Rate of failed requests (e.g., HTTP 5xx responses).
4. **Saturation**: How full the most constrained system resources are (e.g., CPU, memory, database connection pool, queue depth).
</details>

---

## 11. Practice Exercises

### Exercise 1: Write PromQL Query for P99 Latency by Route (Easy)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Write a PromQL expression that computes the 99th percentile response time for each unique HTTP URI over a 5-minute sliding window.

**Solution**:
```promql
histogram_quantile(0.99, sum(rate(http_server_requests_seconds_bucket[5m])) by (le, uri))
```
</details>

### Exercise 2: Prometheus Operator ServiceMonitor YAML (Medium)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Author a Kubernetes `ServiceMonitor` manifest for the Prometheus Operator that scrapes the `management` port of services labeled `app: billing-service` every 10 seconds.

**Solution**:
```yaml
apiVersion: monitoring.coreos.com/v1
kind: ServiceMonitor
metadata:
  name: billing-service-monitor
  labels:
    release: prometheus
spec:
  selector:
    matchLabels:
      app: billing-service
  endpoints:
    - port: management
      path: /actuator/prometheus
      interval: 10s
      scrapeTimeout: 5s
```
</details>

### Exercise 3: Complete Alertmanager Rule for Connection Saturation (Hard)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Create a Prometheus alerting rule that triggers a `critical` alert if any Spring Boot instance has pending HikariCP threads waiting for connections for more than 30 seconds.

**Solution**:
```yaml
groups:
  - name: database-alerts
    rules:
      - alert: HikariCpThreadsBlocked
        expr: hikaricp_connections_pending > 0
        for: 30s
        labels:
          severity: critical
          team: backend-platform
        annotations:
          summary: "Database connection starvation on instance {{ $labels.instance }}"
          description: "There are {{ $value }} threads currently blocked waiting for a database connection in HikariCP pool on {{ $labels.application }}."
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------+-------------------------------------------------------------+
| Concept / Metric                            | Usage & PromQL Description                                  |
+---------------------------------------------+-------------------------------------------------------------+
| /actuator/prometheus                        | Endpoint exposing Micrometer meters in Prometheus format    |
| http_server_requests_seconds_bucket         | Histogram buckets (`le`) for calculating quantiles          |
| histogram_quantile(0.95, sum(rate(...)))     | Server-side calculation of 95th percentile response time   |
| hikaricp_connections_pending                | Critical saturation metric (threads blocked waiting for DB) |
| jvm_gc_pause_seconds_sum                    | Time spent in Stop-The-World GC pauses                      |
| percentiles-histogram: true                 | Required configuration to publish histogram bucket series   |
+---------------------------------------------+-------------------------------------------------------------+
```

---

## 13. Further Reading
- Prometheus Official Documentation: PromQL Basics
- Grafana Official Documentation: Building JVM & Spring Dashboards
- Google SRE Book: Monitoring Distributed Systems
- Micrometer Prometheus Registry Reference Guide