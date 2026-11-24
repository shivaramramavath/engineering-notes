# Spring Boot Actuator Fundamentals

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate to Advanced |
| **Prerequisites** | Spring Boot Fundamentals, Auto-Configuration, REST APIs |
| **Estimated Reading Time** | 25 minutes |
| **Status** | Production-Ready |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Baseline** | OpenJDK 21 LTS / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | OpenJDK 21.0.2, Micrometer 1.14 |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Configuration](#4-syntax--configuration)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance & Security Considerations](#8-performance--security-considerations)
9. [Comparison: Built-in vs Custom Endpoints](#9-comparison-built-in-vs-custom-endpoints)
10. [Interview Questions & Deep Dives](#10-interview-questions--deep-dives)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference](#12-summary--quick-reference)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

In production environments, black-box software is an operational liability. Engineers need runtime observability into application health, database connectivity, heap memory consumption, thread pool saturation, HTTP metrics, and active configuration without requiring a restart or debugger attachment.

### The Problem
Without standardized production endpoints:
1. Every team writes ad-hoc `/ping` or `/status` controllers with inconsistent schemas.
2. Ops teams cannot automate Kubernetes liveness and readiness probe checks.
3. Troubleshooting memory leaks or thread starvation requires invasive JVM dumps instead of lightweight diagnostics.

### The Solution: Spring Boot Actuator
`spring-boot-starter-actuator` brings production-ready operational endpoints to your microservices. It automatically exposes standard, secure, technology-agnostic endpoints for health probes, metrics (Micrometer), environment inspection, loglevel switching at runtime, and thread dumps.

---

## 2. Core Concepts

### Mental Model & Architecture
```
                         HTTP / JMX Request
                                  │
                                  ▼
                    Management Web Security Layer
          (Restricts access to internal management ports/roles)
                                  │
                                  ▼
                   Endpoint Handler Mapping Layer
          (@Endpoint, @WebEndpoint, @EndpointWebExtension)
                                  │
           ┌──────────────────────┼──────────────────────┐
           ▼                      ▼                      ▼
    Health Indicator       Metrics Registry       Environment / Info
 (HealthContributorRegistry) (Micrometer MeterRegistry)  (InfoContributor)
           │                      │                      │
           ▼                      ▼                      ▼
  Liveness / Readiness      Prometheus Scrape       Runtime App Data
```

### Invariants
1. **Disabled & Unexposed by Default**: For security reasons, almost all web endpoints are hidden by default, except `/actuator/health`.
2. **Readiness vs Liveness Separation**: Actuator strictly delineates between whether an application is alive (JVM healthy, not deadlocked) vs ready (able to serve user traffic, dependent databases reachable).
3. **Pluggable Technology Layer**: Endpoints are protocol-agnostic. The same `@Endpoint` class can be exposed over HTTP (REST) or JMX simultaneously.

---

## 3. How It Works Internally

### Endpoint Discovery and Registration
Spring Boot's `EndpointAutoConfiguration` scans for beans annotated with `@Endpoint`, `@WebEndpoint`, or `@JmxEndpoint`. 

1. **Endpoint Model**: An endpoint is parsed into operations:
   - `@ReadOperation` $\rightarrow$ HTTP `GET`
   - `@WriteOperation` $\rightarrow$ HTTP `POST`
   - `@DeleteOperation` $\rightarrow$ HTTP `DELETE`
2. **Web Handler Mapping**: Actuator registers an internal `WebMvcEndpointHandlerMapping` (for Spring MVC) or `WebFluxEndpointHandlerMapping` (for WebFlux) mounted under the configured management base path (`/actuator` by default).
3. **HealthAggregator**: The `HealthEndpoint` delegates to a `HealthContributorRegistry`. It queries registered `HealthIndicator` beans (e.g., `DataSourceHealthIndicator`, `DiskSpaceHealthIndicator`) concurrently and aggregates their statuses (`UP`, `DOWN`, `OUT_OF_SERVICE`, `UNKNOWN`) using a configurable status hierarchy.

---

## 4. Syntax & Configuration

### Dependency Setup (`pom.xml`)
```xml
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-starter-actuator</artifactId>
</dependency>
```

### Standard `application.yaml` Configuration
```yaml
management:
  server:
    port: 8081 # Run management endpoints on a separate internal network port
  endpoints:
    web:
      base-path: /actuator
      exposure:
        include: health,info,metrics,prometheus,env,loggers
        exclude: shutdown
  endpoint:
    health:
      show-details: when-authorized
      show-components: always
      probes:
        enabled: true # Automatically enables livenessState and readinessState
    loggers:
      enabled: true
  info:
    env:
      enabled: true
    git:
      mode: full
    build:
      enabled: true
```

---

## 5. Practical Examples

### Example 1: Basic Health Indicator with Custom Business Logic

```java
package com.example.actuator.health;

import org.springframework.boot.actuate.health.Health;
import org.springframework.boot.actuate.health.HealthIndicator;
import org.springframework.stereotype.Component;

import java.io.File;

@Component
public class DiskQuotaHealthIndicator implements HealthIndicator {

    private static final long MIN_DISK_SPACE_BYTES = 500 * 1024 * 1024; // 500MB

    @Override
    public Health health() {
        File partition = new File("/");
        long usableSpace = partition.getUsableSpace();
        long totalSpace = partition.getTotalSpace();

        if (usableSpace < MIN_DISK_SPACE_BYTES) {
            return Health.down()
                    .withDetail("error", "Disk space threshold breached")
                    .withDetail("usableBytes", usableSpace)
                    .withDetail("totalBytes", totalSpace)
                    .withDetail("freePercent", (double) usableSpace / totalSpace * 100.0)
                    .build();
        }

        return Health.up()
                .withDetail("usableBytes", usableSpace)
                .withDetail("totalBytes", totalSpace)
                .withDetail("freePercent", (double) usableSpace / totalSpace * 100.0)
                .build();
    }
}
```

### Example 2: Enterprise Custom Actuator Endpoint with Read & Write Operations

```java
package com.example.actuator.endpoint;

import org.springframework.boot.actuate.endpoint.annotation.DeleteOperation;
import org.springframework.boot.actuate.endpoint.annotation.Endpoint;
import org.springframework.boot.actuate.endpoint.annotation.ReadOperation;
import org.springframework.boot.actuate.endpoint.annotation.Selector;
import org.springframework.boot.actuate.endpoint.annotation.WriteOperation;
import org.springframework.stereotype.Component;

import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

/**
 * Exposes /actuator/feature-flags to inspect and toggle runtime flags dynamically.
 */
@Component
@Endpoint(id = "feature-flags")
public class FeatureFlagsEndpoint {

    private final Map<String, Boolean> flags = new ConcurrentHashMap<>();

    public FeatureFlagsEndpoint() {
        flags.put("new-checkout-flow", false);
        flags.put("experimental-recommendations", true);
        flags.put("dark-mode-api", true);
    }

    @ReadOperation
    public Map<String, Boolean> getAllFlags() {
        return Map.copyOf(flags);
    }

    @ReadOperation
    public Map<String, Object> getFlag(@Selector String flagName) {
        Boolean state = flags.get(flagName);
        if (state == null) {
            return Map.of("error", "Flag not found", "flag", flagName);
        }
        return Map.of("flag", flagName, "enabled", state);
    }

    @WriteOperation
    public Map<String, Object> updateFlag(String flagName, boolean enabled) {
        flags.put(flagName, enabled);
        return Map.of("status", "UPDATED", "flag", flagName, "enabled", enabled);
    }

    @DeleteOperation
    public Map<String, Object> deleteFlag(@Selector String flagName) {
        Boolean removed = flags.remove(flagName);
        return Map.of("status", removed != null ? "REMOVED" : "NOT_FOUND", "flag", flagName);
    }
}
```

### Example 3: Edge Case — Custom Readiness State Trigger for Cache Warming

```java
package com.example.actuator.readiness;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.boot.availability.ApplicationAvailability;
import org.springframework.boot.availability.AvailabilityChangeEvent;
import org.springframework.boot.availability.ReadinessState;
import org.springframework.boot.context.event.ApplicationReadyEvent;
import org.springframework.context.ApplicationEventPublisher;
import org.springframework.context.event.EventListener;
import org.springframework.stereotype.Component;

import java.util.concurrent.CompletableFuture;

/**
 * Marks the application unready until heavy cache warm-up completes.
 */
@Component
public class CacheWarmingReadinessCoordinator {

    private static final Logger log = LoggerFactory.getLogger(CacheWarmingReadinessCoordinator.class);

    private final ApplicationEventPublisher publisher;
    private final ApplicationAvailability availability;

    public CacheWarmingReadinessCoordinator(ApplicationEventPublisher publisher, ApplicationAvailability availability) {
        this.publisher = publisher;
        this.availability = availability;
    }

    @EventListener(ApplicationReadyEvent.class)
    public void warmCacheOnStartup() {
        // Intentionally keep readiness in REFUSING_TRAFFIC while cache warms up
        AvailabilityChangeEvent.publish(publisher, this, ReadinessState.REFUSING_TRAFFIC);
        log.info("Application ready event received: setting ReadinessState to REFUSING_TRAFFIC for cache warmup");

        CompletableFuture.runAsync(() -> {
            try {
                // Simulate intensive pre-computation / cache warming
                Thread.sleep(3000);
                log.info("Cache warming successfully completed!");
                // Transition to ACCEPTING_TRAFFIC so K8s readiness probe passes
                AvailabilityChangeEvent.publish(publisher, this, ReadinessState.ACCEPTING_TRAFFIC);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
                log.error("Cache warming interrupted", e);
            }
        });
    }
}
```

### Example 4: Anti-Pattern vs Production Fix

#### Anti-Pattern: Exposing Sensitive Environment Variables via `/actuator/env`
```yaml
# ANTI-PATTERN: Exposing all endpoints with default sanitization
management:
  endpoints:
    web:
      exposure:
        include: "*" # Dangerous: exposes env, beans, heapdump, threaddump, httpexchanges
```

#### Production Fix: Explicit Endpoint Whitelist and Keys Sanitization
```yaml
# PRODUCTION FIX: Strict exposure and key masking
management:
  server:
    port: 8081 # Bound to internal interface or isolated management network
  endpoints:
    web:
      exposure:
        include: health,info,prometheus # Only what monitoring and ingress need
  endpoint:
    env:
      show-values: when-authorized
      roles: ACTUATOR_ADMIN
      keys-to-sanitize:
        - password
        - secret
        - key
        - token
        - .*(credential|jwt|private).*
```

### Example 5: High-Performance Custom Meter with Micrometer Integration

```java
package com.example.actuator.metrics;

import io.micrometer.core.instrument.Counter;
import io.micrometer.core.instrument.MeterRegistry;
import io.micrometer.core.instrument.Timer;
import org.springframework.stereotype.Service;

import java.util.concurrent.Callable;

@Service
public class OrderProcessingMetricsService {

    private final Counter orderSuccessCounter;
    private final Counter orderFailureCounter;
    private final Timer orderExecutionTimer;

    public OrderProcessingMetricsService(MeterRegistry registry) {
        this.orderSuccessCounter = Counter.builder("app.orders.processed")
                .tag("status", "success")
                .description("Total successfully processed orders")
                .register(registry);

        this.orderFailureCounter = Counter.builder("app.orders.processed")
                .tag("status", "failure")
                .description("Total failed orders")
                .register(registry);

        this.orderExecutionTimer = Timer.builder("app.orders.execution.time")
                .description("Time taken to process complete order transaction")
                .publishPercentileHistogram(true)
                .register(registry);
    }

    public <T> T recordOrderExecution(Callable<T> operation) throws Exception {
        try {
            T result = orderExecutionTimer.recordCallable(operation);
            orderSuccessCounter.increment();
            return result;
        } catch (Exception ex) {
            orderFailureCounter.increment();
            throw ex;
        }
    }
}
```

---

## 6. Best Practices

1. **Separate Management Port**: Run Actuator on a different internal port (`management.server.port=8081`) to prevent external load balancers and clients from accessing management endpoints.
2. **Never Expose Wildcard (`*`) in Production**: Explicitly list needed endpoints (`include: health,info,prometheus`).
3. **Use Kubernetes Probes Correctly**:
   - Liveness probe: `/actuator/health/liveness` (Should only fail if JVM is deadlocked or unrecoverable).
   - Readiness probe: `/actuator/health/readiness` (Fails if database is down or service is warming up).
4. **Sanitize Secrets**: Ensure regex patterns cover all custom secret properties (`keys-to-sanitize`).
5. **Secure with Spring Security**: Require dedicated role-based authentication (`ROLE_ACTUATOR_ADMIN`) for sensitive endpoints like `/actuator/env` or `/actuator/loggers`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Using Root `/actuator/health` for K8s Liveness Probe** | If downstream DB goes down, the entire pod is killed and restarted in an endless restart loop. | Use `/actuator/health/liveness` for liveness and `/actuator/health/readiness` for readiness. |
| **Exposing `management.endpoints.web.exposure.include=*`** | Leaks system properties, credentials, environment variables, and heap dumps to the public internet. | Restrict to `health,info,prometheus` and isolate the management port. |
| **Blocking HealthIndicators with Long Network Timeouts** | A synchronous HTTP or DB check inside `HealthIndicator` hangs K8s probe calls, causing pod restarts. | Enforce strict timeouts (e.g., 1-2 seconds) on internal health indicator checks. |
| **Forgetting `@Selector` on Path Variables** | `@ReadOperation` with a method parameter returns HTTP 400 or fails to match sub-paths. | Annotate path variables with `@Selector` in `@Endpoint` classes. |
| **Assuming Health Details are Hidden by Default** | If configured incorrectly, stack traces and internal hostnames leak in `Health` response bodies. | Set `management.endpoint.health.show-details: when-authorized`. |
| **Misconfiguring Micrometer Meter Names** | Using non-standard naming conventions breaks Prometheus scrape formatting. | Use dot-notation in Micrometer (`app.orders.total`); Micrometer translates it to `app_orders_total` for Prometheus. |

---

## 8. Performance & Security Considerations

### Memory Overhead of Actuator Metrics
Prometheus metrics maintain histograms and tag cardinality in memory. High-cardinality tags (e.g., embedding User IDs or UUIDs into metric tags) leads to unbounded memory consumption and JVM OutOfMemoryError. Never use dynamically generated strings as metric tag values.

### Heap Dump Endpoint Danger
The `/actuator/heapdump` endpoint triggers a full JVM stop-the-world garbage collection and streams gigabytes of raw heap data over HTTP. It must be strictly protected or disabled entirely in production.

---

## 9. Comparison: Built-in vs Custom Endpoints

| Criteria | Built-in Actuator Endpoints | Custom `@Endpoint` Implementation | Custom `@RestController` under `/api` |
| :--- | :--- | :--- | :--- |
| **Protocol Support** | HTTP & JMX natively | HTTP & JMX natively | HTTP only |
| **Port Isolation** | Automatically uses `management.server.port` | Automatically uses `management.server.port` | Requires manual security/networking routing |
| **Security Configuration** | Integrated into `EndpointRequest.toAnyEndpoint()` | Integrated into Actuator security matchers | Standard Spring Security matchers |
| **Format & Extensibility** | Standardized JSON schemas | Highly customizable | Manual JSON serialization |
| **Primary Purpose** | Infrastructure monitoring & ops | Domain ops (feature toggles, cache eviction) | Business application APIs |

---

## 10. Interview Questions & Deep Dives

<details>
<summary><strong>Q1: What is the architectural difference between Kubernetes Liveness and Readiness probes in Spring Boot Actuator?</strong></summary>

**Answer:**
Liveness represents internal JVM viability: whether the process is alive or deadlocked. If liveness fails, Kubernetes restarts the pod. Readiness represents external traffic availability: whether the service can currently process requests. If readiness fails, Kubernetes removes the pod from load balancer endpoints without restarting it. Spring Boot separates these into `LivenessState` (CORRECT vs BROKEN) and `ReadinessState` (ACCEPTING_TRAFFIC vs REFUSING_TRAFFIC).
</details>

<details>
<summary><strong>Q2: How do you dynamically change log levels at runtime without restarting the application?</strong></summary>

**Answer:**
By exposing the `/actuator/loggers` endpoint and issuing an HTTP POST to `/actuator/loggers/{package.name}` with payload `{"configuredLevel": "DEBUG"}`. Spring Boot's `LoggersEndpoint` updates the underlying logging framework (Logback/Log4j2) logging level dynamically in memory.
</details>

<details>
<summary><strong>Q3: How does Actuator aggregate health statuses from multiple `HealthIndicator` beans?</strong></summary>

**Answer:**
`HealthEndpoint` queries all beans registered in `HealthContributorRegistry`. The results are passed to a `StatusAggregator` (default `SimpleStatusAggregator`), which ranks statuses by severity order: `DOWN` > `OUT_OF_SERVICE` > `UP` > `UNKNOWN`. If any single indicator reports `DOWN`, the overall application health status becomes `DOWN` (HTTP 503).
</details>

<details>
<summary><strong>Q4: What annotations are used to develop a custom Actuator endpoint and its operations?</strong></summary>

**Answer:**
- `@Endpoint(id = "my-endpoint")`: Declares the bean as an Actuator endpoint.
- `@ReadOperation`: Maps to HTTP GET.
- `@WriteOperation`: Maps to HTTP POST.
- `@DeleteOperation`: Maps to HTTP DELETE.
- `@Selector`: Marks a parameter as part of the URI path (e.g., `/actuator/my-endpoint/{item}`).
</details>

<details>
<summary><strong>Q5: Why should you run Actuator on an independent port using `management.server.port`?</strong></summary>

**Answer:**
Running Actuator on a dedicated port (e.g., 8081) allows network engineers to configure firewall rules, security groups, or container ingress controllers to block external internet traffic to port 8081 while leaving the application port (8080) open. This eliminates accidental exposure of internal endpoints.
</details>

<details>
<summary><strong>Q6: How can you protect sensitive properties from being displayed in `/actuator/env`?</strong></summary>

**Answer:**
Use `management.endpoint.env.show-values: when-authorized` and configure regex patterns in `management.endpoint.env.keys-to-sanitize`. Any property key matching words like `password`, `secret`, `key`, or `token` has its value replaced with `******`.
</details>

<details>
<summary><strong>Q7: What is the role of Micrometer in Spring Boot Actuator?</strong></summary>

**Answer:**
Micrometer is an application metrics facade (analogous to SLF4J for logging). Actuator uses Micrometer's `MeterRegistry` to collect dimensional metrics (counters, timers, gauges) and formats them for export to monitoring systems like Prometheus, Datadog, InfluxDB, or CloudWatch without coupling your code to a vendor SDK.
</details>

<details>
<summary><strong>Q8: Can an `@Endpoint` be restricted to web-only or JMX-only?</strong></summary>

**Answer:**
Yes. Use `@WebEndpoint` to expose an endpoint exclusively over HTTP, or `@JmxEndpoint` to expose it exclusively via JMX MBeans. `@Endpoint` exposes it across all configured technologies.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Expose Health and Prometheus Endpoints</strong></summary>

**Task**: Write an `application.yaml` snippet that isolates Actuator on port 9090 and exposes only the `health` and `prometheus` endpoints.

```yaml
# Solution:
management:
  server:
    port: 9090
  endpoints:
    web:
      exposure:
        include: health,prometheus
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Custom Info Contributor</strong></summary>

**Task**: Create an `InfoContributor` component that adds build timestamp and deployment region to `/actuator/info`.

```java
// Solution:
@Component
public class CustomDeploymentInfoContributor implements InfoContributor {
    @Override
    public void contribute(Info.Builder builder) {
        builder.withDetail("deployment", Map.of(
            "region", System.getenv().getOrDefault("DEPLOY_REGION", "us-east-1"),
            "startupTime", System.currentTimeMillis()
        ));
    }
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): External Service Health Indicator</strong></summary>

**Task**: Write a `HealthIndicator` for an external payment gateway that issues a fast ping check with a 1-second timeout.

```java
// Solution:
@Component
public class PaymentGatewayHealthIndicator implements HealthIndicator {
    private final RestClient restClient = RestClient.builder()
            .requestFactory(new JdkClientHttpRequestFactory(HttpClient.newBuilder()
                    .connectTimeout(Duration.ofSeconds(1))
                    .build()))
            .build();

    @Override
    public Health health() {
        try {
            ResponseEntity<Void> response = restClient.get()
                    .uri("https://api.payment-gateway.internal/health")
                    .retrieve()
                    .toBodilessEntity();
            return response.getStatusCode().is2xxSuccessful() 
                    ? Health.up().build() 
                    : Health.down().withDetail("status", response.getStatusCode()).build();
        } catch (Exception ex) {
            return Health.down(ex).build();
        }
    }
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Custom Health Status Code Mapping</strong></summary>

**Task**: Configure a custom health status `DEGRADED` that maps to HTTP status code 200 (so the container remains up during partial degradation).

```yaml
# Solution:
management:
  endpoint:
    health:
      status:
        order: DOWN, OUT_OF_SERVICE, DEGRADED, UP, UNKNOWN
        http-mapping:
          DEGRADED: 200
          DOWN: 503
          OUT_OF_SERVICE: 503
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Dynamic Cache Eviction Endpoint</strong></summary>

**Task**: Build a `@WebEndpoint` with ID `cache-ops` that provides a `@WriteOperation` accepting a `cacheName` parameter and clears the specified Spring cache via `CacheManager`.
</details>

<details>
<summary><strong>Exercise 6 (Hard): High-Throughput Request In-Flight Meter</strong></summary>

**Task**: Build a servlet `Filter` that tracks in-flight HTTP requests using a Micrometer `Gauge` or `AtomicInteger`, and publish alerts if concurrent in-flight requests exceed 200.
</details>

---

## 12. Summary & Quick Reference

- **Core Dependency**: `spring-boot-starter-actuator`
- **Default Exposed Endpoints**: `/actuator/health` only
- **Separate Port**: `management.server.port`
- **Kubernetes Probes**: `/actuator/health/liveness` and `/actuator/health/readiness`
- **Custom Endpoint**: `@Endpoint(id = "name")` with `@ReadOperation`, `@WriteOperation`, `@DeleteOperation`

---

## 13. Further Reading

- [Spring Boot Actuator Official Reference](https://docs.spring.io/spring-boot/reference/actuator/index.html)
- [Micrometer Application Monitoring Documentation](https://micrometer.io/docs)
- [Kubernetes Container Probes Overview](https://kubernetes.io/docs/tasks/configure-pod-container/configure-liveness-readiness-startup-probes/)
- [Production-Ready Microservices Architecture with Spring](https://docs.spring.io/spring-boot/reference/actuator/monitoring.html)
