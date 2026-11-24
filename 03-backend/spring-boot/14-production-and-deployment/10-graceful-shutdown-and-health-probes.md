# Graceful Shutdown and Kubernetes Health Probes Coordination

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x |
| **Spring Framework Version** | 6.2.x |
| **Java Version** | 21 LTS |
| **Module** | 14-production-and-deployment |
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
- [9. Comparison / Termination Strategy Matrix](#9-comparison--termination-strategy-matrix)
- [10. Interview Questions & Answers](#10-interview-questions--answers)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading](#13-further-reading)

---

## 1. What & Why

### Problem Statement
In containerized Kubernetes environments, pods are ephemeral: they are created, scaled, rescheduled, and terminated continuously during deployments, node upgrades, and horizontal scaling. Without meticulous coordination between the Kubernetes orchestration layer and the Spring Boot JVM runtime:
- In-flight HTTP transactions are brutally severed mid-flight, returning HTTP 502/504 errors to end users.
- Database connections are abruptly disconnected mid-commit, leaving orphaned locks and uncommitted transactions in the database engine.
- Asynchronous tasks and Kafka message consumers are killed while holding messages uncommitted, causing duplicate processing or message loss.

### Historical Evolution
Prior to Spring Boot 2.3, terminating a Spring Boot application immediately triggered `context.close()`, abruptly closing network sockets and terminating worker threads. Developers had to write complex custom Tomcat connector listeners to drain requests.

Spring Boot 2.3 introduced native **Graceful Shutdown** (`server.shutdown=graceful`) across all supported embedded web servers (Tomcat, Jetty, Reactor Netty, Undertow). 

In **Spring Boot 3.4**, this is tightly integrated with:
- Phased lifecycle shutdowns (`SmartLifecycle`).
- Asynchronous task executor and scheduler draining.
- Clean coordination with Kubernetes container lifecycle hooks (`preStop`).

### Production Relevance
Implementing coordinated graceful shutdown guarantees:
- **Zero In-Flight Request Drops**: Existing HTTP requests are allowed a configurable window (e.g. 30 seconds) to complete and flush responses to clients.
- **Clean Message Acknowledgment**: Kafka and RabbitMQ consumers finish processing the current message batch and commit offsets cleanly before stopping.
- **Resource Deallocation**: Database connection pools, thread pools, and cache clients close gracefully without leaking connection handles.

---

## 2. Core Concepts

### Key Architectural Phases
1. **Immediate Shutdown (Default)**: The web server stops accepting new connections and forcibly terminates all active threads immediately.
2. **Graceful Shutdown (`server.shutdown=graceful`)**:
   - The web server stops accepting *new* incoming TCP connections.
   - The web server allows *existing, in-flight* requests to complete processing.
   - Once all active requests complete—or when `spring.lifecycle.timeout-per-shutdown-phase` elapses—the server shuts down.
3. **`SmartLifecycle` Phase Hierarchy**: Spring components shut down in reverse order of their `phase` value. Web servers shut down first, followed by message consumers, then service layer pools, and finally datasources.
4. **Kubernetes `preStop` Hook**: An executable command or HTTP call triggered by the kubelet *before* sending `SIGTERM` to the container process.
5. **`terminationGracePeriodSeconds`**: The total time Kubernetes grants a pod to shut down before forcibly killing it with `SIGKILL` (exit code 137).

---

## 3. How It Works Internally

### The Zero-Downtime Pod Termination Dance

```
EVENT: KUBERNETES DEPLOYMENT ROLLOUT (Pod Termination Initiated)
  |
  +==================================================================================+
  | KUBERNETES CONTROL PLANE ASYNC ACTIONS (Time = 0s)                               |
  +==================================================================================+
  | Action A: Endpoint Controller removes Pod IP from Service Endpoints & Ingress    |
  | Action B: Kubelet executes container preStop hook                                |
  +==================================================================================+
                                         |
                                         v
+------------------------------------------------------------------------------------+
| 1. KUBERNETES preStop HOOK: "sleep 15"                                             |
|    - Container process remains running for 15 seconds.                             |
|    - Kube-Proxy, NGINX Ingress, and AWS ALB update their internal routing tables.  |
|    - Ensures ZERO new requests are routed to this pod!                             |
+------------------------------------------------------------------------------------+
                                         |
                                         v (After 15s sleep completes)
+------------------------------------------------------------------------------------+
| 2. KUBELET SENDS SIGTERM (Signal 15)                                               |
|    - HotSpot JVM registers shutdown hook                                           |
|    - Spring Boot initiates ApplicationContext.close()                              |
+------------------------------------------------------------------------------------+
                                         |
                                         v
+------------------------------------------------------------------------------------+
| 3. SPRING BOOT GRACEFUL WEB SERVER SHUTDOWN (Phase = SmartLifecycle.DEFAULT_PHASE)|
|    - Tomcat stops listening on port 8080 & 9090                                    |
|    - Rejects any new TCP connection requests                                       |
|    - Waits up to 30s (timeout-per-shutdown-phase) for active requests to finish    |
+------------------------------------------------------------------------------------+
                                         |
                                         v
+------------------------------------------------------------------------------------+
| 4. ASYNC TASK & MESSAGE CONSUMER DRAINING                                          |
|    - KafkaListenerContainer stops polling; commits in-flight offsets                |
|    - ThreadPoolTaskExecutor drains queued tasks                                    |
+------------------------------------------------------------------------------------+
                                         |
                                         v
+------------------------------------------------------------------------------------+
| 5. DATASOURCE & CACHE CLOSURE                                                      |
|    - HikariCP closes all idle and returned connections cleanly                     |
|    - JVM exits with code 0 (Success)                                               |
+------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Configuration Reference

### Application Configuration (`application.yml`)

```yaml
server:
  # 1. Enable graceful shutdown on embedded web server (Tomcat / Netty)
  shutdown: graceful

spring:
  lifecycle:
    # 2. Maximum time allowed for in-flight requests to complete
    timeout-per-shutdown-phase: 30s
  task:
    execution:
      shutdown:
        # 3. Ensure @Async tasks complete before executor terminates
        await-termination: true
        await-termination-period: 20s

management:
  endpoint:
    health:
      probes:
        enabled: true # Exposes /actuator/health/liveness and readiness
```

### Kubernetes Pod Spec Configuration (`deployment.yaml`)

```yaml
spec:
  # Total grace period must exceed preStop sleep (15s) + Spring shutdown timeout (30s) + 15s buffer = 60s
  terminationGracePeriodSeconds: 60
  containers:
    - name: app
      image: registry.internal/order-service:1.2.0
      ports:
        - containerPort: 8080
        - containerPort: 9090
      lifecycle:
        preStop:
          exec:
            command: ["sh", "-c", "sleep 15"]
      readinessProbe:
        httpGet:
          path: /actuator/health/readiness
          port: 9090
        periodSeconds: 5
```

---

## 5. Practical Examples

### Example 1: Configuring Asynchronous Task Executor Draining
Ensure that background tasks spawned via `@Async` are not interrupted mid-execution during shutdown:

```java
package com.example.production.shutdown;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.scheduling.annotation.EnableAsync;
import org.springframework.scheduling.concurrent.ThreadPoolTaskExecutor;

import java.util.concurrent.Executor;

@Configuration
@EnableAsync
public class GracefulAsyncConfiguration {

    @Bean(name = "orderAsyncExecutor")
    public Executor orderAsyncExecutor() {
        ThreadPoolTaskExecutor executor = new ThreadPoolTaskExecutor();
        executor.setCorePoolSize(10);
        executor.setMaxPoolSize(25);
        executor.setQueueCapacity(500);
        executor.setThreadNamePrefix("order-async-");

        // CRITICAL FOR ZERO-DROPPED TASKS:
        executor.setWaitForTasksToCompleteOnShutdown(true); // Wait for active tasks
        executor.setAwaitTerminationSeconds(25); // Wait at most 25 seconds

        executor.initialize();
        return executor;
    }
}
```

### Example 2: Graceful Message Consumer Shutdown in Spring Kafka
```java
package com.example.production.shutdown;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.kafka.config.ConcurrentKafkaListenerContainerFactory;
import org.springframework.kafka.core.ConsumerFactory;

@Configuration
public class GracefulKafkaConfiguration {

    @Bean
    public ConcurrentKafkaListenerContainerFactory<String, Object> kafkaListenerContainerFactory(
        ConsumerFactory<String, Object> consumerFactory) {

        ConcurrentKafkaListenerContainerFactory<String, Object> factory =
            new ConcurrentKafkaListenerContainerFactory<>();
        factory.setConsumerFactory(consumerFactory);

        // Ensures consumer container finishes in-flight record processing and commits offsets
        factory.getContainerProperties().setShutdownTimeout(15000); // 15 seconds

        return factory;
    }
}
```

### Example 3: Verifying Graceful Shutdown in Application Logs
When `SIGTERM` is delivered to the Spring Boot application, inspect the shutdown logs:

```text
2026-03-24T10:15:30.010Z  INFO 1 --- [order-service] [ionShutdownHook] o.s.b.w.e.t.GracefulShutdown : Commencing graceful shutdown. Waiting for active requests to complete
2026-03-24T10:15:33.450Z  INFO 1 --- [order-service] [tomcat-handler-4] c.e.service.OrderService    : Finished long-running order processing for order #9821
2026-03-24T10:15:33.452Z  INFO 1 --- [order-service] [ionShutdownHook] o.s.b.w.e.t.GracefulShutdown : Graceful shutdown complete
2026-03-24T10:15:33.460Z  INFO 1 --- [order-service] [ionShutdownHook] o.s.c.support.DefaultLifecycleProcessor : Stopping beans in phase 2147483647
2026-03-24T10:15:33.470Z  INFO 1 --- [order-service] [ionShutdownHook] com.zaxxer.hikari.HikariDataSource       : OrdersHikariPool - Shutdown initiated...
2026-03-24T10:15:33.490Z  INFO 1 --- [order-service] [ionShutdownHook] com.zaxxer.hikari.HikariDataSource       : OrdersHikariPool - Shutdown completed.
2026-03-24T10:15:33.500Z  INFO 1 --- [order-service] [ionShutdownHook] c.e.o.OrderServiceApplication            : Destroying Spring ApplicationContext
```
*Result*: Tomcat waited 3.44 seconds for active order #9821 to complete, closed database pools, and exited cleanly with zero errors!

### Example 4: Anti-Pattern vs. Production Fix (Immediate Hard Termination)
```yaml
# ANTI-PATTERN: Default immediate shutdown + short termination grace period
server:
  shutdown: immediate # Kills worker threads instantly! In-flight user requests drop!

# deployment.yaml
spec:
  terminationGracePeriodSeconds: 10 # Kubernetes sends SIGKILL after only 10s!

# PRODUCTION FIX: Coordinated Graceful Draining
server:
  shutdown: graceful
spring:
  lifecycle:
    timeout-per-shutdown-phase: 30s

# deployment.yaml
spec:
  terminationGracePeriodSeconds: 60
  containers:
    - name: app
      lifecycle:
        preStop:
          exec:
            command: ["sh", "-c", "sleep 15"]
```

### Example 5: Custom Shutdown Progress Indicator Bean
```java
package com.example.production.shutdown;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.context.SmartLifecycle;
import org.springframework.stereotype.Component;

import java.util.concurrent.atomic.AtomicBoolean;

@Component
public class CustomResourceDrainer implements SmartLifecycle {

    private static final Logger log = LoggerFactory.getLogger(CustomResourceDrainer.class);
    private final AtomicBoolean running = new AtomicBoolean(false);

    @Override
    public void start() {
        log.info("CustomResourceDrainer initialized and running.");
        running.set(true);
    }

    @Override
    public void stop() {
        log.warn("CustomResourceDrainer: Flushing telemetry and draining background workers...");
        try {
            Thread.sleep(1000); // Simulate buffer flush
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }
        running.set(false);
        log.info("CustomResourceDrainer: Flush complete.");
    }

    @Override
    public boolean isRunning() {
        return running.get();
    }

    @Override
    public int getPhase() {
        // Runs BEFORE database connection pool closes (phase < Integer.MAX_VALUE)
        return 1000;
    }
}
```

---

## 6. Best Practices & Production Guidelines

1. **Calculate the Grace Period Equation Correctly**:
   $$\text{terminationGracePeriodSeconds} > \text{preStop sleep} + \text{timeout-per-shutdown-phase} + \text{buffer}$$
   Example: $15\text{s sleep} + 30\text{s timeout} + 15\text{s buffer} = \mathbf{60\text{ seconds}}$.
2. **Always Configure `setWaitForTasksToCompleteOnShutdown(true)`**: Ensure all Spring `ThreadPoolTaskExecutor` beans are configured to await completion, preventing background tasks from being killed mid-execution.
3. **Never Rely on In-Flight Keep-Alive Connections for New Work**: During shutdown, Tomcat stops accepting new connections and sends `Connection: close` headers in responses to existing keep-alive connections, prompting clients to reconnect to healthy sibling pods.
4. **Coordinate Messaging Consumers**: Set `shutdownTimeout` on Kafka and RabbitMQ container listener factories to allow active message batches to complete and commit offsets.
5. **Monitor Termination Logs in CloudWatch/Loki**: Ensure that pod terminations log `Graceful shutdown complete`. If you see logs abruptly cutting off or ending with `SIGKILL`, your `terminationGracePeriodSeconds` is too short.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Omitting `server.shutdown: graceful` | Default is `immediate`; Tomcat cuts active connections instantly on `SIGTERM`. | Set `server.shutdown: graceful` in `application.yml`. |
| `terminationGracePeriodSeconds` too short | Kubernetes sends `SIGKILL` after 30s while the app is still sleeping in `preStop` or draining requests. | Increase grace period to $\ge 60\text{ seconds}$. |
| Missing `preStop` hook | Ingress load balancers continue sending new traffic for 3 seconds after `SIGTERM`, returning 502 Bad Gateway. | Add `lifecycle.preStop.exec: ["sh", "-c", "sleep 15"]`. |
| Infinite task execution blocking shutdown | A stuck background thread blocks `context.close()` until Kubernetes sends `SIGKILL`. | Set strict timeouts: `spring.lifecycle.timeout-per-shutdown-phase: 30s`. |
| Unmanaged raw `new Thread()` allocations | Raw threads created outside Spring manage zero lifecycle hooks and are killed abruptly. | Use Spring-managed `TaskExecutor` beans. |
| Forgetting to close Redis / Kafka pools | Leaves idle socket connections hanging on remote servers, consuming broker memory. | Let Spring Boot lifecycle destroy beans cleanly. |

---

## 8. Performance & Security Notes

### Impact on Rolling Deployment Speed
- Increasing graceful shutdown timeouts (e.g. 15s preStop + 30s drain = 45s per pod) increases the total time required for a rolling deployment across 10 pods.
- Mitigate this by tuning Kubernetes rolling update parameters: set `maxSurge: 50%` so Kubernetes boots multiple new replacement pods in parallel while old pods drain gracefully.

---

## 9. Comparison / Termination Strategy Matrix

| Dimension | Default Immediate Shutdown | Spring Boot Graceful (`server.shutdown=graceful`) | Coordinated Graceful (Spring + K8s preStop) |
|---|---|---|---|
| **In-Flight HTTP Requests** | **Abruptly Terminated (502 Error)**| Drained cleanly up to 30s | **Drained cleanly up to 30s** |
| **New Ingress Requests** | Rejected immediately | Rejected (May cause 502 during routing lag)| **Safely routed to sibling pods (Zero 502s)**|
| **Background Tasks** | Killed mid-execution | Drained if configured | **Drained cleanly** |
| **Database Transactions**| Corrupted / Abrupt disconnect | Committed or rolled back cleanly | **Committed or rolled back cleanly** |
| **Recommended Environment**| Local Development Only | Standalone VMs | **100% of Production Kubernetes Clusters** |

---

## 10. Interview Questions & Answers

### Q1: Why does Spring Boot's `server.shutdown=graceful` alone NOT prevent HTTP 502 errors in Kubernetes?
<details>
<summary>Click to view answer</summary>

When a pod enters termination, Kubernetes sends `SIGTERM` to the container and concurrently notifies the Endpoint controller to remove the pod IP from Service endpoints. Because network propagation across Kube-Proxy iptables and Ingress load balancers takes 1 to 5 seconds, an Ingress controller may continue routing new HTTP requests to the pod *after* Spring Boot has received `SIGTERM` and closed its listening socket, resulting in HTTP 502 Bad Gateway errors.  
A **`preStop: sleep 15` hook** delays sending `SIGTERM` to Spring Boot, ensuring that the network routing layer completely removes the pod from service before Spring Boot stops listening on port 8080.
</details>

### Q2: What happens when `server.shutdown=graceful` is enabled in Spring Boot?
<details>
<summary>Click to view answer</summary>

1. The embedded web server (Tomcat, Jetty, Netty) stops accepting new connection requests.
2. The server inspects active in-flight request threads.
3. It allows active requests to finish processing up to the limit configured by `spring.lifecycle.timeout-per-shutdown-phase` (default: 30s).
4. Once all active requests complete (or the timeout expires), the web server stops and Spring proceeds to shut down remaining application beans.
</details>

### Q3: How do you mathematically calculate `terminationGracePeriodSeconds` in a Kubernetes manifest?
<details>
<summary>Click to view answer</summary>

The formula is:
$$\text{terminationGracePeriodSeconds} = \text{preStop sleep time} + \text{Spring shutdown phase timeout} + \text{Buffer}$$
Example:
- `preStop: sleep 15` (15 seconds)
- `spring.lifecycle.timeout-per-shutdown-phase: 30s` (30 seconds)
- Safety buffer (15 seconds)
$$\text{terminationGracePeriodSeconds} = 15 + 30 + 15 = 60 \text{ seconds}$$
If this value is too low, Kubernetes will issue `SIGKILL` while the application is still draining requests.
</details>

### Q4: How does Spring Boot ensure database connection pools are not closed while active HTTP requests are using them?
<details>
<summary>Click to view answer</summary>

Spring Framework uses the **`SmartLifecycle` phase hierarchy**. Beans with a higher phase value start later and shut down earlier. The embedded web server has a high phase (`SmartLifecycle.DEFAULT_PHASE = 2147483647`), while the DataSource and TransactionManager have lower phase values. Spring stops the web server first and waits for in-flight requests to complete *before* it closes the HikariCP DataSource.
</details>

### Q5: How do you ensure Spring `@Async` tasks complete during application shutdown?
<details>
<summary>Click to view answer</summary>

On the `ThreadPoolTaskExecutor` bean:
1. Call `executor.setWaitForTasksToCompleteOnShutdown(true)`.
2. Call `executor.setAwaitTerminationSeconds(20)` (setting a maximum wait timeout).
This instructs the executor's underlying `ExecutorService` to execute `shutdown()` and `awaitTermination()` during the Spring context closure.
</details>

### Q6: What HTTP header does Tomcat send during graceful shutdown for active keep-alive connections?
<details>
<summary>Click to view answer</summary>

Tomcat adds the `Connection: close` header to the HTTP response of any in-flight request completing during the graceful shutdown phase. This instructs the client (or upstream API Gateway) to immediately close its persistent TCP socket and establish a new connection to a different, healthy server replica.
</details>

### Q7: What is the difference between `SIGTERM` (Signal 15) and `SIGKILL` (Signal 9)?
<details>
<summary>Click to view answer</summary>

- `SIGTERM`: A termination signal sent to the process requesting a graceful exit. The JVM catches `SIGTERM` via its registered shutdown hooks, allowing Spring Boot to execute clean draining routines.
- `SIGKILL`: A kernel-level forced kill signal that cannot be caught, handled, or ignored by the JVM. The operating system destroys the process immediately, terminating threads mid-instruction and dropping all active connections.
</details>

### Q8: How do you gracefully stop a Spring Kafka message listener?
<details>
<summary>Click to view answer</summary>

Configure the container factory's `setShutdownTimeout(15000)`. When the application context closes, the listener container stops polling Kafka, allows the current message or batch being processed to complete, commits the consumer offset to the broker, and cleanly closes the consumer connection.
</details>

---

## 11. Practice Exercises

### Exercise 1: Configure Graceful Shutdown in application.yml (Easy)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Write an `application.yml` enabling graceful shutdown with a 25-second phase timeout and enabling Actuator health probes.

**Solution**:
```yaml
server:
  shutdown: graceful

spring:
  lifecycle:
    timeout-per-shutdown-phase: 25s

management:
  endpoint:
    health:
      probes:
        enabled: true
```
</details>

### Exercise 2: Configure Complete Kubernetes Pod Lifecycle Block (Medium)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Write the `lifecycle` and `terminationGracePeriodSeconds` spec in a Kubernetes `Deployment` that sleeps for 20 seconds on `preStop` and allows a 70-second total grace period.

**Solution**:
```yaml
spec:
  terminationGracePeriodSeconds: 70
  template:
    spec:
      containers:
        - name: app
          lifecycle:
            preStop:
              exec:
                command: ["sh", "-c", "sleep 20"]
```
</details>

### Exercise 3: Custom SmartLifecycle Bean for Long-Running Batch Worker Draining (Hard)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Implement a custom Spring component implementing `SmartLifecycle` that safely signals a long-running while-loop batch worker to stop and waits up to 10 seconds for it to exit cleanly before Spring closes the DataSource.

**Solution**:
```java
package com.example.production.shutdown;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.context.SmartLifecycle;
import org.springframework.stereotype.Component;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;

@Component
public class ManagedBatchWorker implements SmartLifecycle, Runnable {

    private static final Logger log = LoggerFactory.getLogger(ManagedBatchWorker.class);
    private final AtomicBoolean running = new AtomicBoolean(false);
    private final CountDownLatch workerFinished = new CountDownLatch(1);
    private Thread workerThread;

    @Override
    public void start() {
        running.set(true);
        workerThread = new Thread(this, "batch-worker-1");
        workerThread.start();
        log.info("Batch worker started.");
    }

    @Override
    public void run() {
        while (running.get()) {
            try {
                // Perform chunk processing
                Thread.sleep(500);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
                break;
            }
        }
        log.info("Batch worker exited main processing loop.");
        workerFinished.countDown();
    }

    @Override
    public void stop() {
        log.warn("Signaling batch worker to stop gracefully...");
        running.set(false);
        try {
            boolean cleanExit = workerFinished.await(10, TimeUnit.SECONDS);
            if (!cleanExit) {
                log.error("Batch worker timed out during shutdown. Forcing interrupt.");
                workerThread.interrupt();
            }
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }
    }

    @Override
    public boolean isRunning() {
        return running.get();
    }

    @Override
    public int getPhase() {
        // Must stop before DataSource (DataSource phase = 0)
        return 500;
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------+-------------------------------------------------------------+
| Parameter / Concept                         | Rule & Production Value                                     |
+---------------------------------------------+-------------------------------------------------------------+
| server.shutdown: graceful                   | Mandatory to drain active in-flight HTTP requests           |
| spring.lifecycle.timeout-per-shutdown-phase | 30s (Maximum time to await active request completion)       |
| preStop: sleep 15                           | Pauses JVM SIGTERM to let Ingress withdraw endpoints        |
| terminationGracePeriodSeconds               | Set to 60s (preStop 15s + Spring timeout 30s + 15s buffer)  |
| Connection: close                           | Header automatically emitted by Tomcat during draining      |
| SmartLifecycle Phase                        | Controls ordered shutdown: Web -> Queues -> DB              |
| await-termination: true                     | Mandatory on TaskExecutors to prevent dropping async tasks  |
+---------------------------------------------+-------------------------------------------------------------+
```

---

## 13. Further Reading
- Spring Boot Documentation: Graceful Shutdown
- Kubernetes Best Practices: Terminating with Grace (Google Cloud Blog)
- Spring Framework Reference: Customizing Lifecycle with SmartLifecycle
- Zero Downtime Deployments: Coordinating Pod Draining and Ingress Proxies