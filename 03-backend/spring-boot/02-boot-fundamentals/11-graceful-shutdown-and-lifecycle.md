# Graceful Shutdown & Lifecycle in Spring Boot

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | ApplicationContext Lifecycle, SmartLifecycle, Thread Pools |
| **Estimated Reading Time** | 22 minutes |
| **Status** | Production-Ready |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Baseline** | OpenJDK 21 LTS / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | Tomcat 10.1, Netty 4.1, Undertow 2.3, OpenJDK 21.0.2 |

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
9. [Comparison: Graceful vs Abrupt Termination](#9-comparison-graceful-vs-abrupt-termination)
10. [Interview Questions & Deep Dives](#10-interview-questions--deep-dives)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference](#12-summary--quick-reference)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

In a production microservices architecture, instances are continuously scaled, deployed, restarted, or evacuated by orchestrators like Kubernetes. An abrupt shutdown abruptly terminates TCP connections, leaves database transactions hanging, drops in-flight HTTP requests, and truncates asynchronous processing tasks halfway through completion.

### The Problem
When a container orchestrator terminates a pod (e.g., `kubectl delete pod` or rolling deployment), it issues a `SIGTERM` signal. If an application immediately exits:
1. Active HTTP clients receive `ECONNRESET` or `502 Bad Gateway` from ingress controllers.
2. Background message listeners (JMS, Kafka, RabbitMQ) drop batches without acknowledging or roll back partially processed database records.
3. Thread pools terminate worker threads abruptly, potentially corrupting shared state or leaking external socket connections.

### The Solution: Graceful Shutdown
Spring Boot provides native graceful shutdown support across embedded web servers (Tomcat, Jetty, Undertow, Reactor Netty), Spring TaskExecutors, and container components. Upon receiving a termination signal:
1. The web server immediately stops accepting new incoming connections.
2. Existing active requests are given a configurable grace period (e.g., 30 seconds) to complete normally.
3. Background tasks drain queued jobs.
4. The `ApplicationContext` triggers ordered destruction hooks (`SmartLifecycle.stop()`, `@PreDestroy`, `DisposableBean.destroy()`) in reverse dependency order.

---

## 2. Core Concepts

### Mental Model & Invariants
```
                      SIGTERM / SIGINT Signal Received
                                     │
                                     ▼
                      JVM Shutdown Hook Triggered
                                     │
                                     ▼
                    Phase 1: Ingress Traffic Halt
               Embedded Web Server stops accepting new requests
               Liveness probe remains UP, Readiness probe flips to OUT_OF_SERVICE
                                     │
                                     ▼
                    Phase 2: In-Flight Drain Phase
            Active requests execute up to `server.shutdown.grace-period`
            Thread pools reject new tasks; finish executing queued work
                                     │
                                     ▼
                   Phase 3: SmartLifecycle Stop Phase
           Ordered beans (Kafka consumers, batch pollers) stop in descending phase order
                                     │
                                     ▼
                   Phase 4: Bean Destruction Phase
          @PreDestroy -> DisposableBean.destroy() -> destroy-method
          DataSources close pooled connections -> Cache connections release
                                     │
                                     ▼
                         JVM Exit Code 0
```

### Invariants
1. **No Ingress Re-entry**: Once the shutdown phase begins, embedded web connectors return HTTP connection close headers and refuse new socket handshakes.
2. **Deterministic Phasing**: `SmartLifecycle` beans with higher `getPhase()` values are stopped *before* beans with lower phase values.
3. **Hard Timeout Cap**: If requests or tasks exceed the configured grace period, the runtime forcibly terminates remaining workers to prevent the process from hanging indefinitely.

---

## 3. How It Works Internally

### The JVM Shutdown Hook Registration
During startup in `SpringApplication.run()`, Spring registers a JVM shutdown hook via `Runtime.getRuntime().addShutdownHook(...)` when `registerShutdownHook` is true (default).

```java
// SpringApplication.java simplified trace
private void registerShutdownHook() {
    Runtime.getRuntime().addShutdownHook(new Thread(() -> {
        this.context.close();
    }, "SpringApplicationShutdownHook"));
}
```

### Web Server Graceful Shutdown Callbacks
Each embedded web server implements the `org.springframework.boot.web.server.GracefulShutdownCallback`:

1. **Tomcat (`TomcatWebServer`)**: Pauses connectors so the OS kernel socket accept queue stops dispatching to worker threads. It marks connectors into the `PAUSED` state and waits on an internal latch until the worker thread pool count drops to 0 or timeout expires.
2. **Reactor Netty (`NettyWebServer`)**: Closes the listening channel (`DisposableServer.dispose()`) and executes `GracefulShutdown` on the event loop groups.
3. **Undertow (`UndertowWebServer`)**: Suspends connectors and waits for the active request counter to reach zero.

---

## 4. Syntax & Configuration

### Essential `application.yaml` Configuration

```yaml
server:
  port: 8080
  shutdown: graceful # Options: immediate (default), graceful

spring:
  lifecycle:
    timeout-per-shutdown-phase: 30s # Global timeout for SmartLifecycle bean phases
  task:
    execution:
      shutdown:
        await-termination: true
        await-termination-period: 20s
    scheduling:
      shutdown:
        await-termination: true
        await-termination-period: 15s

management:
  endpoint:
    shutdown:
      enabled: true # Exposes POST /actuator/shutdown
  endpoints:
    web:
      exposure:
        include: health,info,shutdown
```

---

## 5. Practical Examples

### Example 1: Basic Web Server Graceful Shutdown Verification

```java
package com.example.shutdown;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RequestParam;
import org.springframework.web.bind.annotation.RestController;

import java.util.concurrent.TimeUnit;

@SpringBootApplication
@RestController
public class GracefulShutdownDemoApplication {

    private static final Logger log = LoggerFactory.getLogger(GracefulShutdownDemoApplication.class);

    public static void main(String[] args) {
        SpringApplication.run(GracefulShutdownDemoApplication.class, args);
    }

    @GetMapping("/api/slow-work")
    public String slowOperation(@RequestParam(defaultValue = "10") int seconds) throws InterruptedException {
        log.info("Starting long operation for {} seconds on thread {}", seconds, Thread.currentThread().getName());
        TimeUnit.SECONDS.sleep(seconds);
        log.info("Completed long operation on thread {}", Thread.currentThread().getName());
        return "Work completed successfully";
    }
}
```

### Example 2: Enterprise Custom SmartLifecycle with Strict Phased Ordering

```java
package com.example.shutdown.lifecycle;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.context.SmartLifecycle;
import org.springframework.stereotype.Component;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * MessageConsumerLifecycle represents an ingress queue listener.
 * It must stop BEFORE database connections or caches shut down.
 *
 * Phase Ordering Rule:
 * Lower phase = starts first, stops last.
 * Higher phase = starts last, stops first.
 * Web server typically runs at Integer.MAX_VALUE - 1024.
 */
@Component
public class KafkaIngressListenerLifecycle implements SmartLifecycle {

    private static final Logger log = LoggerFactory.getLogger(KafkaIngressListenerLifecycle.class);
    private final AtomicBoolean running = new AtomicBoolean(false);

    @Override
    public void start() {
        log.info("Starting Kafka Ingress Listener...");
        running.set(true);
    }

    @Override
    public void stop() {
        log.info("Synchronous stop called on Kafka Ingress Listener");
        running.set(false);
    }

    @Override
    public void stop(Runnable callback) {
        log.info("Graceful asynchronous stop initiated for Kafka Ingress Listener...");
        new Thread(() -> {
            try {
                // Emulate committing offsets and flushing buffers cleanly
                log.info("Committing consumer offsets to broker...");
                Thread.sleep(2000);
                running.set(false);
                log.info("Offsets committed. Notifying Spring container.");
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
                log.warn("Shutdown interrupted prematurely");
            } finally {
                callback.run(); // Crucial: must invoke to complete container shutdown step
            }
        }, "kafka-graceful-stop-worker").start();
    }

    @Override
    public boolean isRunning() {
        return running.get();
    }

    @Override
    public int getPhase() {
        // High phase number: Starts after DB/infra, stops BEFORE DB/infra
        return 10000;
    }

    @Override
    public boolean isAutoStartup() {
        return true;
    }
}
```

### Example 3: Edge Case — Handling Graceful Shutdown on ThreadPoolTaskExecutor with Virtual Threads

```java
package com.example.shutdown.config;

import org.springframework.boot.autoconfigure.task.TaskExecutionAutoConfiguration;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.core.task.support.TaskExecutorAdapter;
import org.springframework.scheduling.concurrent.ThreadPoolTaskExecutor;

import java.time.Duration;
import java.util.concurrent.Executors;

@Configuration(proxyBeanMethods = false)
public class ThreadPoolGracefulConfig {

    @Bean
    public ThreadPoolTaskExecutor customPlatformExecutor() {
        ThreadPoolTaskExecutor executor = new ThreadPoolTaskExecutor();
        executor.setCorePoolSize(16);
        executor.setMaxPoolSize(64);
        executor.setQueueCapacity(500);
        executor.setThreadNamePrefix("platform-worker-");
        
        // Critical graceful shutdown configuration
        executor.setWaitForTasksToCompleteOnShutdown(true);
        executor.setAwaitTerminationSeconds(25); // Must be strictly LESS than Kubernetes terminationGracePeriodSeconds
        executor.initialize();
        return executor;
    }

    @Bean(name = TaskExecutionAutoConfiguration.APPLICATION_TASK_EXECUTOR_BEAN_NAME)
    public TaskExecutorAdapter virtualThreadExecutor() {
        // Virtual threads auto-close gracefully via AutoCloseable on modern Java 21 Executors
        return new TaskExecutorAdapter(Executors.newVirtualThreadPerTaskExecutor());
    }
}
```

### Example 4: Anti-Pattern vs Production Fix

#### Anti-Pattern: Long-Running Task in `@PreDestroy` Blocking Context Closure
```java
// ANTI-PATTERN: Doing heavy network calls or unbounded sleeps in @PreDestroy
@Component
public class BadShutdownHandler {
    
    @PreDestroy
    public void cleanup() {
        // Blocks the single Spring shutdown hook thread!
        // If other beans need to stop, they are blocked behind this.
        // If an external service is down, this stalls until container orchestrator issues SIGKILL.
        try {
            HttpClient.newHttpClient().send(
                HttpRequest.newBuilder(URI.create("https://slow-audit-service.internal/flush")).build(),
                HttpResponse.BodyHandlers.discarding()
            );
        } catch (Exception e) {
            // Ignored, bad cleanup
        }
    }
}
```

#### Production Fix: Utilizing `SmartLifecycle` with Asynchronous Non-blocking Callback
```java
// PRODUCTION FIX: Use SmartLifecycle stop(Runnable callback) with bounded timeout
@Component
public class GoodShutdownHandler implements SmartLifecycle {

    private static final Logger log = LoggerFactory.getLogger(GoodShutdownHandler.class);
    private final AtomicBoolean active = new AtomicBoolean(false);

    @Override
    public void start() {
        active.set(true);
    }

    @Override
    public void stop(Runnable callback) {
        CompletableFuture.runAsync(() -> {
            try {
                log.info("Flushing audit logs to remote collector...");
                // Bounded client request
                // In production, use standard RestClient with a 3s read timeout
            } catch (Exception ex) {
                log.error("Failed to cleanly flush audit logs", ex);
            }
        }).orTimeout(4, TimeUnit.SECONDS)
          .whenComplete((res, ex) -> {
              active.set(false);
              callback.run();
          });
    }

    @Override
    public void stop() {
        stop(() -> {});
    }

    @Override
    public boolean isRunning() {
        return active.get();
    }

    @Override
    public int getPhase() {
        return 500;
    }
}
```

### Example 5: Kubernetes Pod Lifecycle Integration & Readiness Probe Coordination

```java
package com.example.shutdown.k8s;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.boot.availability.ApplicationAvailability;
import org.springframework.boot.availability.AvailabilityChangeEvent;
import org.springframework.boot.availability.ReadinessState;
import org.springframework.context.ApplicationEventPublisher;
import org.springframework.context.event.ContextClosedEvent;
import org.springframework.context.event.EventListener;
import org.springframework.stereotype.Component;

/**
 * Ensures Kubernetes ingress drops pod traffic BEFORE the web server begins closing connections.
 */
@Component
public class KubernetesReadinessGraceCoordinator {

    private static final Logger log = LoggerFactory.getLogger(KubernetesReadinessGraceCoordinator.class);

    private final ApplicationEventPublisher eventPublisher;
    private final ApplicationAvailability availability;

    public KubernetesReadinessGraceCoordinator(ApplicationEventPublisher eventPublisher,
                                               ApplicationAvailability availability) {
        this.eventPublisher = eventPublisher;
        this.availability = availability;
    }

    @EventListener(ContextClosedEvent.class)
    public void onContextClosed(ContextClosedEvent event) {
        log.warn("Application context closing detected! Actively setting ReadinessState to REFUSING_TRAFFIC.");
        
        // Flips /actuator/health/readiness to OUT_OF_SERVICE
        AvailabilityChangeEvent.publish(eventPublisher, this, ReadinessState.REFUSING_TRAFFIC);
        
        try {
            // Sleep 5-10 seconds to allow kube-proxy and Ingress controllers (like NGINX/Envoy)
            // to discover the unready status and remove endpoint from routing table.
            log.info("Pausing 5s for ingress route propagation before web connectors unbind...");
            Thread.sleep(5000);
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }
    }
}
```

---

## 6. Best Practices

1. **Synchronize with Orchestrator Grace Periods**:
   If Kubernetes `spec.terminationGracePeriodSeconds` is `60s`, set:
   - Ingress propagation sleep: `5s`
   - `spring.lifecycle.timeout-per-shutdown-phase`: `25s`
   - `server.shutdown`: `graceful`
   - Total shutdown must finish in `< 55s` to avoid receiving an abrupt `SIGKILL`.
2. **Order Bean Lifecycles Deliberately**:
   Consumers/Ingress (`phase = 10000`) $\rightarrow$ Business Services (`phase = 5000`) $\rightarrow$ Data Sources / Clients (`phase = 0`).
3. **Use `AvailabilityChangeEvent` for Probes**:
   Mark the pod unready prior to shutting down sockets so external load balancers cease dispatching new transactions.
4. **Always Set ThreadPool Timeouts**:
   Never enable `setWaitForTasksToCompleteOnShutdown(true)` without specifying `setAwaitTerminationSeconds(...)`. Unbounded queues can hang shutdown indefinitely.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Omitting `server.shutdown: graceful`** | Default is `immediate`; Tomcat and Netty drop socket connections instantaneously upon SIGTERM. | Explicitly add `server.shutdown: graceful` to `application.yaml`. |
| **K8s Grace Period Shorter than App Grace Period** | Kubernetes sends `SIGKILL` at $T=30s$, abruptly terminating the JVM while Spring is still waiting on the $45s$ grace timer. | Configure `terminationGracePeriodSeconds` in Helm/Deployment to be at least $15s$ larger than Spring's total timeout. |
| **Closing DataSource in `@PreDestroy` Before Workers Complete** | Async workers executing tasks fail with `SQLException: Connection pool has been shut down`. | Assign thread pool execution beans lower/higher phases or inject them cleanly so Spring manages destruction ordering. |
| **Ignoring the Callback in `stop(Runnable)`** | Failing to call `callback.run()` inside `SmartLifecycle.stop(Runnable)` causes the container to wait until phase timeout expires. | Always invoke `callback.run()` in a `finally` block or completion stage. |
| **Using `System.exit()` from Worker Threads** | Bypasses standard Spring lifecycle phases if shutdown hooks are overridden or misconfigured. | Use `SpringApplication.exit(context, () -> exitCode)` to guarantee standard lifecycle execution. |
| **Neglecting Ingress Network Sync Time** | Pod turns off HTTP connector immediately, but K8s `kube-proxy` takes 2-4 seconds to sync endpoint removal, causing 502s on arriving requests. | Use a K8s `preStop` hook (`sleep 5`) or an availability listener to delay socket closure. |

---

## 8. Performance & Security Considerations

### Virtual Threads (Project Loom)
Virtual threads running tasks under `Executors.newVirtualThreadPerTaskExecutor()` automatically implement `AutoCloseable`. Closing the executor calls `join()` on all active virtual threads. However, ensure virtual threads are not pinned on native locks or IO calls that ignore interruption, which would stall shutdown.

### Actuator Endpoint Security
The `/actuator/shutdown` endpoint exposes an HTTP POST that halts the entire service:
- Keep it disabled by default (`management.endpoint.shutdown.enabled=false`).
- If enabled, require `ADMIN` role authentication via Spring Security and restrict it to internal management subnets.

---

## 9. Comparison: Graceful vs Abrupt Termination

| Feature | `server.shutdown: immediate` | `server.shutdown: graceful` |
| :--- | :--- | :--- |
| **In-Flight HTTP Requests** | Truncated / Socket Reset | Allowed to complete up to timeout |
| **New Incoming Connections** | Rejected immediately | Rejected or closed with Connection: Close |
| **Active DB Transactions** | Rolled back abruptly on socket disconnect | Allowed to commit or rollback cleanly |
| **Shutdown Duration** | ~50ms - 200ms | 0ms - `timeout-per-shutdown-phase` |
| **Orchestrator Readiness** | N/A | Integrates with Kubernetes probes |
| **Suitable For** | Unit/Integration tests, local mock CLI tools | Production enterprise microservices |

---

## 10. Interview Questions & Deep Dives

<details>
<summary><strong>Q1: What happens under the hood when a Spring Boot application receives a SIGTERM signal?</strong></summary>

**Answer:**
When the JVM receives `SIGTERM`, it invokes all registered shutdown hooks. Spring Boot's shutdown hook initiates `ApplicationContext.close()`. This fires a `ContextClosedEvent`, signals the web server to stop accepting new requests, enters the graceful drain phase for active HTTP calls, stops all `SmartLifecycle` beans in descending order of their phase values, executes `@PreDestroy` methods and `DisposableBean.destroy()` implementations, and finally closes the underlying `BeanFactory` and releases resources like HikariCP connection pools.
</details>

<details>
<summary><strong>Q2: How does Tomcat implement graceful shutdown inside Spring Boot?</strong></summary>

**Answer:**
In `TomcatWebServer`, Spring calls `tomcat.getService().findConnectors()`, pausing each connector. Pausing halts the `Acceptor` threads, preventing the OS socket backlog from accepting new TCP connections. It then monitors the connector's worker executor (thread pool) using an internal polling loop until the active thread count drops to zero or the `grace-period` timeout expires.
</details>

<details>
<summary><strong>Q3: How do you coordinate Spring Boot graceful shutdown with Kubernetes rolling updates to achieve zero 502/504 errors?</strong></summary>

**Answer:**
A rolling update causes Kubernetes to remove the Pod IP from the Service endpoints list while simultaneously sending `SIGTERM`. Because endpoint distribution across nodes (`kube-proxy`, Envoy) takes 2-5 seconds, requests may still arrive at the pod *after* `SIGTERM`. To prevent 502 errors:
1. Add a `preStop` lifecycle hook in the Pod spec: `lifecycle: { preStop: { exec: { command: ["sh", "-c", "sleep 5"] } } }`.
2. Configure Spring Boot: `server.shutdown=graceful` and `spring.lifecycle.timeout-per-shutdown-phase=25s`.
3. Set pod `terminationGracePeriodSeconds=40s` (covering the 5s preStop + 25s Spring grace + buffer).
</details>

<details>
<summary><strong>Q4: What is the difference between `Lifecycle` and `SmartLifecycle` interfaces?</strong></summary>

**Answer:**
`Lifecycle` only provides `start()`, `stop()`, and `isRunning()`, and requires explicit manual invocation unless wrapped by a container trigger. `SmartLifecycle` extends `Lifecycle` and adds:
- `isAutoStartup()`: whether the container automatically calls `start()` during context refresh.
- `getPhase()`: determines startup and shutdown sequence (higher phases start later, stop earlier).
- `stop(Runnable callback)`: allows asynchronous cleanup, notifying Spring when done.
</details>

<details>
<summary><strong>Q5: What happens if an active HTTP request exceeds the graceful shutdown timeout?</strong></summary>

**Answer:**
When the timeout specified by `spring.lifecycle.timeout-per-shutdown-phase` (or server-specific grace period) expires, the server forcibly terminates the worker threads processing the requests, closing the client sockets with an incomplete response (`TCP RST`), and proceeds to bean destruction.
</details>

<details>
<summary><strong>Q6: Why is `proxyBeanMethods = false` recommended in configuration classes containing lifecycle beans?</strong></summary>

**Answer:**
`proxyBeanMethods = false` skips CGLIB subclass generation, improving startup time and reducing memory footprint. As long as bean methods do not directly invoke each other expecting singleton memoization, disabling it is safe and recommended in modern Spring Boot.
</details>

<details>
<summary><strong>Q7: Can you trigger a graceful shutdown programmatically from within an application controller?</strong></summary>

**Answer:**
Yes. You can inject `ConfigurableApplicationContext` and trigger it in a separate thread via `SpringApplication.exit(context, () -> exitCode)`. A separate thread must be used so that the HTTP request calling the endpoint can complete its response before the context closes.
</details>

<details>
<summary><strong>Q8: How does Spring Boot handle background tasks submitted to `TaskScheduler` during shutdown?</strong></summary>

**Answer:**
If `spring.task.scheduling.shutdown.await-termination` is set to `true`, the scheduler stops firing new executions and waits up to `await-termination-period` for currently running scheduled executions to finish.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Enable Graceful Shutdown in `application.yaml`</strong></summary>

**Task**: Create an `application.yaml` snippet that enables graceful shutdown for an embedded Tomcat server with a 15-second grace period and exposes the actuator shutdown endpoint.

```yaml
# Solution:
server:
  shutdown: graceful

spring:
  lifecycle:
    timeout-per-shutdown-phase: 15s

management:
  endpoint:
    shutdown:
      enabled: true
  endpoints:
    web:
      exposure:
        include: health,shutdown
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Programmatic Context Close Endpoint</strong></summary>

**Task**: Write an `@RestController` endpoint `POST /api/admin/exit` that returns a confirmation JSON payload and asynchronously initiates context closure after 500ms.

```java
// Solution:
@RestController
public class ExitController {
    @Autowired
    private ConfigurableApplicationContext context;

    @PostMapping("/api/admin/exit")
    public ResponseEntity<Map<String, String>> exitApp() {
        CompletableFuture.delayedExecutor(500, TimeUnit.MILLISECONDS).execute(() -> {
            SpringApplication.exit(context, () -> 0);
        });
        return ResponseEntity.ok(Map.of("status", "SHUTDOWN_INITIATED"));
    }
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Implement an Async Cleanup SmartLifecycle</strong></summary>

**Task**: Create a `FileUploaderTracker` implementing `SmartLifecycle` with `phase = 2000`. In its `stop(Runnable callback)`, flush an in-memory buffer to disk on a background thread and then trigger `callback.run()`.

```java
// Solution:
@Component
public class FileUploaderTracker implements SmartLifecycle {
    private final AtomicBoolean running = new AtomicBoolean(false);

    @Override
    public void start() { running.set(true); }

    @Override
    public void stop() { stop(() -> {}); }

    @Override
    public void stop(Runnable callback) {
        CompletableFuture.runAsync(() -> {
            // Emulate disk sync
            try { Thread.sleep(1000); } catch (InterruptedException ignored) {}
        }).whenComplete((v, t) -> {
            running.set(false);
            callback.run();
        });
    }

    @Override public boolean isRunning() { return running.get(); }
    @Override public int getPhase() { return 2000; }
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Verify Shutdown Behavior with an Integration Test</strong></summary>

**Task**: Write a unit or integration test using `ApplicationContextRunner` verifying that a custom bean's `@PreDestroy` method is invoked upon closing the runner.

```java
// Solution:
class GracefulShutdownTest {
    static class TestBean {
        boolean cleanedUp = false;
        @PreDestroy void clean() { cleanedUp = true; }
    }

    @Test
    void testPreDestroyInvoked() {
        ApplicationContextRunner runner = new ApplicationContextRunner()
            .withUserConfiguration(TestBean.class);

        runner.run(context -> {
            TestBean bean = context.getBean(TestBean.class);
            org.assertj.core.api.Assertions.assertThat(bean.cleanedUp).isFalse();
            context.close();
            org.assertj.core.api.Assertions.assertThat(bean.cleanedUp).isTrue();
        });
    }
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Zero-Downtime Deployment Simulation Harness</strong></summary>

**Task**: Build a test client using Java 21 `HttpClient` that pumps 10 concurrent requests/sec to `/api/slow-work` while a simulated `SIGTERM` shutdown is issued. Validate that all submitted requests prior to shutdown complete with HTTP 200 and none fail with 502/connection reset.
</details>

<details>
<summary><strong>Exercise 6 (Hard): Custom Multi-Phase Coordination Engine</strong></summary>

**Task**: Implement 3 distinct `SmartLifecycle` beans (`IngressListener` at Phase 3000, `WorkerPool` at Phase 2000, `DatabaseManager` at Phase 1000). Record their shutdown invocation order into a shared concurrent log and assert that shutdown executed in exact order: IngressListener $\rightarrow$ WorkerPool $\rightarrow$ DatabaseManager.
</details>

---

## 12. Summary & Quick Reference

- **Key Flag**: `server.shutdown: graceful` (defaults to `immediate`).
- **Phase Timeout**: `spring.lifecycle.timeout-per-shutdown-phase` (default `30s`).
- **Phasing Invariant**: Higher phase values start later and shut down earlier.
- **Kubernetes Rule of Thumb**: `Pod terminationGracePeriodSeconds` > `preStop sleep` + `Spring phase timeout`.

---

## 13. Further Reading

- [Spring Boot Official Documentation: Graceful Shutdown](https://docs.spring.io/spring-boot/reference/web/graceful-shutdown.html)
- [Kubernetes Pod Lifecycle & Termination Sequence](https://kubernetes.io/docs/concepts/workloads/pods/pod-lifecycle/#pod-termination)
- [Java Virtual Machine Shutdown Hooks Guide](https://docs.oracle.com/en/java/javase/21/docs/api/java.base/java/lang/Runtime.html#addShutdownHook(java.lang.Thread))
- [Spring Framework SmartLifecycle API Contract](https://docs.spring.io/spring-framework/docs/current/javadoc-api/org/springframework/context/SmartLifecycle.html)
