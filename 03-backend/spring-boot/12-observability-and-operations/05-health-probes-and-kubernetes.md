# Health Probes and Kubernetes Integration

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
In container orchestration platforms like Kubernetes, container state (running vs stopped) is insufficient to determine application health. A Spring Boot process might be running (PID active in the container), but:
- The JVM is locked in a deadlock or an unrecoverable infinite loop.
- The application context is still initializing caches or migrating database schemas.
- External dependencies (database connection pool, broker) are temporarily saturated.

If Kubernetes sends traffic while the container is warming up, users receive HTTP 502/503 errors. Conversely, if Kubernetes restarts a container simply because an external database is temporarily down, an entire cluster of pods will restart in a catastrophic **cascading death spiral**.

### Historical Evolution
Prior to Spring Boot 2.3, developers exposed a generic `/actuator/health` endpoint and pointed all Kubernetes probes (startup, liveness, readiness) to the same URL. This often caused disaster: if a downstream database had a network blip, the health check returned `DOWN`, prompting Kubernetes to kill the pod. When hundreds of pods restarted at once, they overwhelmed the recovering database.

Spring Boot 2.3+ introduced **native Kubernetes Probe Groups** and the **ApplicationAvailability** API (`LivenessState` and `ReadinessState`), modernizing health orchestration for cloud-native workloads.

### Production Relevance
Spring Boot's probe architecture ensures:
- **Zero-downtime rolling deployments**: Traffic is only routed when `ReadinessState` transitions to `ACCEPTING_TRAFFIC`.
- **Self-healing from unrecoverable deadlocks**: Kubernetes kills and restarts pods only when internal `LivenessState` breaks (`BROKEN`).
- **Smooth graceful termination**: Pods stop receiving new traffic before beginning cleanup during scaling or deployments.

---

## 2. Core Concepts

### The Kubernetes Probe Triad
1. **Startup Probe**: Determines whether the application process has started. While the startup probe is checking, liveness and readiness probes are disabled. Essential for slow-starting JVM applications.
2. **Liveness Probe (`/actuator/health/liveness`)**: Verifies whether the internal state of the application is healthy. If this fails, Kubernetes **kills and restarts the container**. It must **never** check external dependencies (databases, external REST APIs).
3. **Readiness Probe (`/actuator/health/readiness`)**: Verifies whether the application is currently able to handle client traffic. If this fails, Kubernetes **removes the pod from the Service Endpoints** (no traffic is routed to it), but does **not** restart the pod.
4. **ApplicationAvailability**: Spring Boot abstraction providing access to:
   - `LivenessState`: `CORRECT` or `BROKEN`.
   - `ReadinessState`: `ACCEPTING_TRAFFIC` or `REFUSING_TRAFFIC`.
5. **AvailabilityChangeEvent**: Spring Application Event published to transition availability states programmatically.

---

## 3. How It Works Internally

### Kubernetes Pod Lifecycle & Spring Boot State Machine

```
CONTAINER CREATED
      |
      v
+------------------------------------------------------------------------------------+
| 1. APPLICATION CONTEXT STARTUP                                                     |
|    Spring context initializes beans, runs Flyway migrations, warms up caches.       |
|    ReadinessState = REFUSING_TRAFFIC                                               |
|    LivenessState  = CORRECT                                                        |
|    --> Kubernetes Startup Probe polls /actuator/health/liveness                    |
+------------------------------------------------------------------------------------+
      |
      v
+------------------------------------------------------------------------------------+
| 2. APPLICATION READY (ApplicationReadyEvent published)                             |
|    Spring transitions: ReadinessState = ACCEPTING_TRAFFIC                          |
|    --> Readiness Probe returns HTTP 200 OK                                         |
|    --> Kubernetes adds Pod to Service Endpoints -> Pod receives user traffic      |
+------------------------------------------------------------------------------------+
      |
      | External DB slows down / pool saturated
      v
+------------------------------------------------------------------------------------+
| 3. TEMPORARY OVERLOAD / DEGRADATION                                                |
|    ReadinessState transitions to REFUSING_TRAFFIC                                  |
|    LivenessState remains CORRECT (Application is not dead!)                        |
|    --> Readiness Probe returns HTTP 503 SERVICE UNAVAILABLE                        |
|    --> Kubernetes removes Pod from Service (NO RESTART)                            |
|    --> Pod sheds load, cleans pool, and recovers without JVM reboot                |
+------------------------------------------------------------------------------------+
      |
      | SIGTERM received (Kubernetes rolling update / scale down)
      v
+------------------------------------------------------------------------------------+
| 4. GRACEFUL SHUTDOWN (server.shutdown = graceful)                                  |
|    ReadinessState immediately set to REFUSING_TRAFFIC                              |
|    Kubernetes stops sending new ingress traffic                                    |
|    Application waits up to 30s (spring.lifecycle.timeout-per-shutdown-phase)       |
|    Finishes active in-flight HTTP requests and cleanly closes connection pools     |
|    JVM terminates gracefully (exit code 0)                                         |
+------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Configuration Reference

### Configuration (`application.yml`)

```yaml
server:
  port: 8080
  shutdown: graceful # Enable graceful shutdown for Tomcat/Netty

spring:
  lifecycle:
    timeout-per-shutdown-phase: 30s # Time to drain in-flight requests

management:
  server:
    port: 9090 # Isolated management port
  endpoints:
    web:
      exposure:
        include: [ "health", "info", "prometheus" ]
  endpoint:
    health:
      probes:
        enabled: true # Force enables probes even if running outside K8s
      show-details: when-authorized
      group:
        liveness:
          include: [ "livenessState", "diskSpace" ]
        readiness:
          include: [ "readinessState", "db", "redis" ]
```

### Kubernetes Pod Spec (`deployment.yaml`)

```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: order-service
spec:
  replicas: 3
  template:
    spec:
      containers:
        - name: app
          image: order-service:latest
          ports:
            - containerPort: 8080 # Business port
            - containerPort: 9090 # Actuator port
          # 1. Startup Probe: Allows up to 60s for cold JVM start
          startupProbe:
            httpGet:
              path: /actuator/health/liveness
              port: 9090
            failureThreshold: 30
            periodSeconds: 2
          # 2. Liveness Probe: Checks if JVM is broken (restart if fails)
          livenessProbe:
            httpGet:
              path: /actuator/health/liveness
              port: 9090
            initialDelaySeconds: 5
            periodSeconds: 10
            timeoutSeconds: 2
            failureThreshold: 3
          # 3. Readiness Probe: Checks if pod can accept traffic
          readinessProbe:
            httpGet:
              path: /actuator/health/readiness
              port: 9090
            initialDelaySeconds: 5
            periodSeconds: 5
            timeoutSeconds: 2
            failureThreshold: 2
```

---

## 5. Practical Examples

### Example 1: Programmatically Tripping Readiness on Circuit Breaker Open
```java
package com.example.observability.probes;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.boot.availability.AvailabilityChangeEvent;
import org.springframework.boot.availability.ReadinessState;
import org.springframework.context.ApplicationEventPublisher;
import org.springframework.stereotype.Service;

@Service
public class GracefulTrafficManager {

    private static final Logger log = LoggerFactory.getLogger(GracefulTrafficManager.class);
    private final ApplicationEventPublisher eventPublisher;

    public GracefulTrafficManager(ApplicationEventPublisher eventPublisher) {
        this.eventPublisher = eventPublisher;
    }

    public void takeOutOfRotation(String reason) {
        log.warn("Taking application out of rotation. Reason: {}", reason);
        // Publishes event causing /actuator/health/readiness to return HTTP 503
        AvailabilityChangeEvent.publish(eventPublisher, this, ReadinessState.REFUSING_TRAFFIC);
    }

    public void putBackInRotation() {
        log.info("Putting application back into rotation. Resuming traffic acceptance.");
        // Transitions readiness back to HTTP 200 OK
        AvailabilityChangeEvent.publish(eventPublisher, this, ReadinessState.ACCEPTING_TRAFFIC);
    }
}
```

### Example 2: Handling Fatal Invariants with LivenessState Breakdown
```java
package com.example.observability.probes;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.boot.availability.AvailabilityChangeEvent;
import org.springframework.boot.availability.LivenessState;
import org.springframework.context.ApplicationEventPublisher;
import org.springframework.stereotype.Component;

@Component
public class FatalCorruptionDetector {

    private static final Logger log = LoggerFactory.getLogger(FatalCorruptionDetector.class);
    private final ApplicationEventPublisher eventPublisher;

    public FatalCorruptionDetector(ApplicationEventPublisher eventPublisher) {
        this.eventPublisher = eventPublisher;
    }

    public void onUnrecoverableStateDetected(String diagnostic) {
        log.error("CRITICAL FATAL CORRUPTION: {}. Requesting Kubernetes pod restart!", diagnostic);
        // Instructs Kubernetes to KILL and RESTART this container
        AvailabilityChangeEvent.publish(eventPublisher, this, LivenessState.BROKEN);
    }
}
```

### Example 3: Custom Readiness Contributor for Cache Warm-Up
```java
package com.example.observability.probes;

import org.springframework.boot.actuate.health.Health;
import org.springframework.boot.actuate.health.HealthIndicator;
import org.springframework.stereotype.Component;

@Component("cacheWarmupHealthIndicator")
public class CacheWarmupHealthIndicator implements HealthIndicator {

    private volatile boolean isCacheWarmed = false;
    private volatile int loadedEntries = 0;

    public void markReady(int entries) {
        this.loadedEntries = entries;
        this.isCacheWarmed = true;
    }

    @Override
    public Health health() {
        if (!isCacheWarmed) {
            return Health.down()
                .withDetail("reason", "In-memory cache pre-warming in progress")
                .build();
        }
        return Health.up()
            .withDetail("entriesLoaded", loadedEntries)
            .build();
    }
}
```

### Example 4: Anti-Pattern vs. Production Fix (The Cascading Death Spiral)
```java
// ANTI-PATTERN: Including external database or remote microservice in the LIVENESS probe!
// Scenario: Database undergoes failover or network hiccup.
// Result: /actuator/health/liveness fails -> Kubernetes KILLS all 50 pods simultaneously!
// All 50 pods restart and flood the recovering database with startup connections -> CLUSTER COLLAPSE!
@Configuration
public class BadProbeConfiguration {
    // DO NOT DO THIS!
    // management.endpoint.health.group.liveness.include: livenessState, db, externalApi
}

// PRODUCTION FIX: Strict separation of concerns between Liveness and Readiness
// Liveness = ONLY internal JVM health (memory, deadlocks, livenessState).
// Readiness = Internal health + external dependencies (db, redis, message broker).
```
```yaml
# application.yml (Production Fix)
management:
  endpoint:
    health:
      group:
        liveness:
          include:
            - livenessState
        readiness:
          include:
            - readinessState
            - db
            - redis
            - diskSpace
```

### Example 5: Graceful Draining Verification Filter
```java
package com.example.observability.probes;

import jakarta.servlet.FilterChain;
import jakarta.servlet.ServletException;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.servlet.http.HttpServletResponse;
import org.springframework.boot.availability.ApplicationAvailability;
import org.springframework.boot.availability.ReadinessState;
import org.springframework.core.Ordered;
import org.springframework.core.annotation.Order;
import org.springframework.stereotype.Component;
import org.springframework.web.filter.OncePerRequestFilter;

import java.io.IOException;

@Component
@Order(Ordered.HIGHEST_PRECEDENCE)
public class GracefulShutdownGateFilter extends OncePerRequestFilter {

    private final ApplicationAvailability availability;

    public GracefulShutdownGateFilter(ApplicationAvailability availability) {
        this.availability = availability;
    }

    @Override
    protected void doFilterInternal(HttpServletRequest request,
                                    HttpServletResponse response,
                                    FilterChain filterChain) throws ServletException, IOException {
        // If pod is shutting down and already refusing traffic, reject non-actuator requests immediately
        if (availability.getReadinessState() == ReadinessState.REFUSING_TRAFFIC 
            && !request.getRequestURI().startsWith("/actuator")) {
            response.setStatus(HttpServletResponse.SC_SERVICE_UNAVAILABLE);
            response.setHeader("Connection", "close");
            response.getWriter().write("{\"error\": \"Instance is shutting down. Route to sibling pod.\"}");
            return;
        }

        filterChain.doFilter(request, response);
    }
}
```

---

## 6. Best Practices & Production Guidelines

1. **Never Put External Dependencies in Liveness Probes**: A liveness failure restarts the container. If your database is down, restarting your application container will not fix the database—it will only saturate it with connection attempts. Put external checks in `readiness` only!
2. **Always Use a Startup Probe for Java Applications**: Cold JVM starts with JIT compilation can take 15–45 seconds. Without a startup probe, liveness probes with low initial delays will kill the container before it finishes booting.
3. **Configure `server.shutdown: graceful`**: Gives in-flight HTTP requests a configured timeout window (e.g. 30s) to complete processing before the JVM exits during pod termination.
4. **Isolate Management Port**: Place probes on `management.server.port: 9090`. This ensures that even if application worker threads on port `8080` are temporarily saturated, Kubernetes probes can still be answered promptly on the management port.
5. **Set Sensible Probe Thresholds**: Avoid `periodSeconds: 1` or `timeoutSeconds: 1`. In high-load scenarios, transient GC pauses can cause false-positive probe failures. Use `timeoutSeconds: 3` and `failureThreshold: 3`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Putting database check in Liveness probe | Causes cluster-wide cascading restart when database experiences brief network hiccup. | Keep database in `readiness` group only. |
| Omission of Startup Probe | Kubernetes liveness probe kills slow-booting JVM during Spring context initialization. | Configure `startupProbe` with high `failureThreshold` (e.g. 30 $\times$ 2s). |
| Ignoring graceful shutdown | Kubernetes sends `SIGKILL` after `terminationGracePeriodSeconds`, aborting user requests mid-flight. | Set `server.shutdown: graceful` and configure `timeout-per-shutdown-phase`. |
| Pointing all probes to `/actuator/health` | Bypasses probe separation; treats temporary readiness degradation as fatal liveness failure. | Point specifically to `/actuator/health/liveness` and `/actuator/health/readiness`. |
| Aggressive 1-second probe timeout | A minor Garbage Collection pause (e.g. 1.2s STW) trips the probe and triggers a pod restart. | Set `timeoutSeconds: 3` or higher. |
| Missing `Connection: close` during shutdown | Keep-alive HTTP connections from API Gateways may continue sending requests to a terminating pod. | Ensure graceful shutdown rejects new keep-alive connections. |

---

## 8. Performance & Security Notes

### Probe Polling Impact
- Kubernetes probes poll endpoints every 2 to 10 seconds per container. Across 100 replicas, this results in hundreds of HTTP requests per minute hitting `/actuator/health/*`.
- Ensure custom health indicators do not perform full table scans or allocate large byte arrays.
- Probes should evaluate lightweight in-memory flags or cached states.

### Network & Security Boundary
- Probes should be accessible to the Kubernetes kubelet without requiring public credentials.
- Use `management.server.port: 9090` so that Kubernetes cluster networking can route kubelet probe checks internally while ingress controllers expose only port `8080`.

---

## 9. Comparison / When to Use What

| Scenario | Startup Probe | Liveness Probe | Readiness Probe |
|---|---|---|---|
| **JVM cold start & bean loading** | **Yes** (protects boot phase) | Inactive until startup succeeds | Inactive until startup succeeds |
| **Deadlock / Infinite loop detected** | No | **Yes** (triggers pod restart) | Irrelevant (pod will be restarted) |
| **Database temporarily overloaded** | No | No (must stay UP!) | **Yes** (takes pod out of routing) |
| **Rolling deployment pod drain** | No | No | **Yes** (transitions to REFUSING) |
| **Action taken on failure** | Container killed & retried | Container killed & restarted | Traffic detached from pod |

---

## 10. Interview Questions & Answers

### Q1: What is the difference between a Liveness probe and a Readiness probe in Kubernetes?
<details>
<summary>Click to view answer</summary>

- **Liveness Probe**: Asks "Is the container runtime internally healthy?" If it fails, Kubernetes kills the container and initiates a container restart.
- **Readiness Probe**: Asks "Can the container currently handle incoming user traffic?" If it fails, Kubernetes stops sending network traffic to the pod by removing its IP from the Service endpoints, but does **not** restart the container.
</details>

### Q2: Why should an external database connection NEVER be included in a Kubernetes Liveness probe?
<details>
<summary>Click to view answer</summary>

If the external database has a momentary outage or latency spike, all connected Spring Boot pods will fail their liveness checks simultaneously. Kubernetes will kill all pods at once. When the database recovers, all 50+ pods will boot simultaneously, overwhelming the database with connection handshakes, causing it to crash again in a catastrophic cascading failure (death spiral).
</details>

### Q3: How does Spring Boot's `ApplicationAvailability` API work?
<details>
<summary>Click to view answer</summary>

`ApplicationAvailability` is an injectable Spring interface providing current states for:
- `getLivenessState()`: Returns `LivenessState.CORRECT` or `LivenessState.BROKEN`.
- `getReadinessState()`: Returns `ReadinessState.ACCEPTING_TRAFFIC` or `ReadinessState.REFUSING_TRAFFIC`.
Developers can change states at runtime by publishing an `AvailabilityChangeEvent`.
</details>

### Q4: What happens during Spring Boot's graceful shutdown phase?
<details>
<summary>Click to view answer</summary>

When `server.shutdown: graceful` is enabled:
1. The embedded web server stops accepting new connections.
2. The readiness state transitions to `REFUSING_TRAFFIC`.
3. The server waits for active in-flight requests to complete up to `spring.lifecycle.timeout-per-shutdown-phase` (default: 30s).
4. Once in-flight requests finish, the `ApplicationContext` is closed and resources (thread pools, datasources) are destroyed cleanly.
</details>

### Q5: How does Kubernetes know when a Spring Boot pod is ready during rolling updates?
<details>
<summary>Click to view answer</summary>

The Kubernetes deployment controller polls `/actuator/health/readiness`. When the Spring Boot application finishes loading all beans and warm-up logic, it publishes an `ApplicationReadyEvent`, which sets `ReadinessState.ACCEPTING_TRAFFIC`. The readiness endpoint returns HTTP 200 OK. Only then does Kubernetes route traffic to the new pod and terminate the old replica.
</details>

### Q6: What is a Startup Probe, and why is it critical for Spring Boot?
<details>
<summary>Click to view answer</summary>

A `startupProbe` disables liveness and readiness checks until the container has finished its initial startup sequence. Because JVM applications often require 20–40 seconds to warm up and run database migrations, a startup probe prevents the liveness probe from prematurely killing the container during startup.
</details>

### Q7: Can Kubernetes probe endpoints be reached on a separate port?
<details>
<summary>Click to view answer</summary>

Yes. By setting `management.server.port: 9090`, Spring Boot opens a secondary web server dedicated to Actuator. In the Kubernetes `Deployment` YAML, the `httpGet` probe specifies `port: 9090`, ensuring operational traffic is decoupled from public API traffic on port `8080`.
</details>

### Q8: What HTTP status codes does `/actuator/health/readiness` return?
<details>
<summary>Click to view answer</summary>

- `HTTP 200 OK`: When all components in the readiness group report `UP` (`ACCEPTING_TRAFFIC`).
- `HTTP 503 Service Unavailable`: When any component in the group reports `DOWN` or `OUT_OF_SERVICE` (`REFUSING_TRAFFIC`).
</details>

---

## 11. Practice Exercises

### Exercise 1: Configure Custom Health Groups in application.yml (Easy)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Configure `application.yml` to enable health probes and create a custom `readiness` group containing `readinessState`, `db`, and `diskSpace`.

**Solution**:
```yaml
management:
  endpoint:
    health:
      probes:
        enabled: true
      group:
        liveness:
          include: livenessState
        readiness:
          include:
            - readinessState
            - db
            - diskSpace
```
</details>

### Exercise 2: Programmatic Traffic Pause via REST Endpoint (Medium)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Create an administrative `@RestController` with endpoints to manually toggle `ReadinessState` between `ACCEPTING_TRAFFIC` and `REFUSING_TRAFFIC`.

**Solution**:
```java
package com.example.observability.probes;

import org.springframework.boot.availability.AvailabilityChangeEvent;
import org.springframework.boot.availability.ReadinessState;
import org.springframework.context.ApplicationEventPublisher;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

@RestController
@RequestMapping("/admin/traffic")
public class TrafficToggleController {

    private final ApplicationEventPublisher publisher;

    public TrafficToggleController(ApplicationEventPublisher publisher) {
        this.publisher = publisher;
    }

    @PostMapping("/pause")
    public ResponseEntity<String> pauseTraffic() {
        AvailabilityChangeEvent.publish(publisher, this, ReadinessState.REFUSING_TRAFFIC);
        return ResponseEntity.ok("Readiness transitioned to REFUSING_TRAFFIC");
    }

    @PostMapping("/resume")
    public ResponseEntity<String> resumeTraffic() {
        AvailabilityChangeEvent.publish(publisher, this, ReadinessState.ACCEPTING_TRAFFIC);
        return ResponseEntity.ok("Readiness transitioned to ACCEPTING_TRAFFIC");
    }
}
```
</details>

### Exercise 3: Complete Kubernetes Deployment YAML with Proper Probes (Hard)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Write a production-grade Kubernetes `Deployment` manifest for a Spring Boot service with startup, liveness, and readiness probes on port 9090 with graceful shutdown termination periods.

**Solution**:
```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: payment-service
  labels:
    app: payment-service
spec:
  replicas: 4
  strategy:
    rollingUpdate:
      maxSurge: 1
      maxUnavailable: 0
  selector:
    matchLabels:
      app: payment-service
  template:
    metadata:
      labels:
        app: payment-service
    spec:
      terminationGracePeriodSeconds: 45
      containers:
        - name: payment-app
          image: payment-service:2.4.0
          ports:
            - name: http
              containerPort: 8080
            - name: management
              containerPort: 9090
          startupProbe:
            httpGet:
              path: /actuator/health/liveness
              port: 9090
            failureThreshold: 20
            periodSeconds: 3
          livenessProbe:
            httpGet:
              path: /actuator/health/liveness
              port: 9090
            initialDelaySeconds: 5
            periodSeconds: 10
            timeoutSeconds: 3
            failureThreshold: 3
          readinessProbe:
            httpGet:
              path: /actuator/health/readiness
              port: 9090
            initialDelaySeconds: 5
            periodSeconds: 5
            timeoutSeconds: 2
            failureThreshold: 2
```
</details>

---

## 12. Summary / Cheat Sheet

```
+-----------------------------------+-------------------------------------------------------------+
| Probe / State Concept             | Purpose & Production Rule                                   |
+-----------------------------------+-------------------------------------------------------------+
| Startup Probe                     | Disables other probes until cold JVM finishes booting       |
| Liveness Probe                    | Checks internal JVM state ONLY; failure kills & restarts pod|
| Readiness Probe                   | Checks if ready for traffic (includes DB); failure sheds load|
| LivenessState.CORRECT / BROKEN    | Spring Boot internal health state representation            |
| ReadinessState.ACCEPTING / REFUSING| Controls whether pod receives Kubernetes ingress traffic   |
| server.shutdown: graceful         | Waits up to 30s to drain in-flight HTTP requests            |
| management.server.port            | Isolates probe traffic from public business API port        |
+-----------------------------------+-------------------------------------------------------------+
```

---

## 13. Further Reading
- Spring Boot Documentation: Kubernetes Probes
- Kubernetes Documentation: Configure Liveness, Readiness and Startup Probes
- Google Cloud Blog: Kubernetes Best Practices - Setting Up Health Checks
- Martin Fowler: Patterns of Distributed Systems - Heartbeat and Liveness Detection