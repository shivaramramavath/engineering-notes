# Task Scheduling and Asynchronous Execution in Spring Boot

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x |
| **Java Version** | 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | `@Scheduled`, `@Async`, `ThreadPoolTaskExecutor`, `ThreadPoolTaskScheduler`, Virtual Threads |

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
Real-world enterprise systems cannot operate on pure request-driven architectures alone. Systems must execute periodic recurring background jobs (clearing expired sessions, generating monthly billing summaries, synchronizing external inventory catalogs) and offload non-blocking operations from HTTP request threads (sending emails, auditing user actions, encoding video files). Without built-in scheduling and asynchronous concurrency abstractions, developers are forced to manually manage raw Java `Thread`, `Timer`, or unmanaged `ExecutorService` instances, introducing memory leaks, thread pool starvation, and lifecycle management nightmares.

### The Evolution
Spring introduced declarative task execution and scheduling via `@EnableScheduling`, `@Scheduled`, `@EnableAsync`, and `@Async`. In Spring Framework 6 and Spring Boot 3, these annotations are fully modernized with native support for standard six-field cron expressions (including macros like `@hourly` and `@daily`), customized `ThreadPoolTaskExecutor` and `ThreadPoolTaskScheduler`, distributed lock coordination (via ShedLock), and seamless adoption of Java 21 Virtual Threads (`spring.threads.virtual.enabled=true`).

### Relevance in Modern Engineering
Declarative asynchronous execution decouples high-throughput web frontends from slow downstream operations, while declarative scheduling eliminates the need for heavyweight external schedulers (like Quartz or Kubernetes CronJobs) for internal application maintenance tasks.

---

## 2. Core Concepts

- **`@Scheduled`**: Marks a method to be executed periodically according to fixed rate, fixed delay, or cron expressions.
- **Fixed Rate vs Fixed Delay**:
  - *Fixed Rate (`fixedRate`)*: Executes tasks at constant time intervals measured from the *start* of the previous execution.
  - *Fixed Delay (`fixedDelay`)*: Executes tasks with a constant delay measured from the *completion* of the previous execution.
  - *Cron Expression (`cron`)*: Standard 6-field pattern (`sec min hour day-of-month month day-of-week`) for calendar-based execution.
- **`@Async`**: Marks a method to be invoked asynchronously on a separate thread; return types can be `void`, `Future<T>`, or `CompletableFuture<T>`.
- **`ThreadPoolTaskExecutor`**: Spring's primary managed wrapper around `java.util.concurrent.ThreadPoolExecutor`, providing lifecycle management, graceful shutdown, and metric instrumentation.
- **Distributed Lock (ShedLock)**: Prevents duplicate execution of `@Scheduled` methods across horizontally scaled multi-instance clusters.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------+
|                        Spring @Async & @Scheduled Internals                       |
|                                                                                   |
| 1. @EnableAsync / @EnableScheduling Post-Processing                               |
|    +-----------------------------+                                                |
|    | AsyncAnnotationBeanPostProc | ---> Wraps @Async beans in Dynamic Proxies     |
|    +-----------------------------+                                                |
|    | ScheduledAnnotationBeanPost | ---> Registers @Scheduled with TaskRegistrar   |
|    +-----------------------------+                                                |
|                                                                                   |
| 2. @Async Execution Path                                                          |
|    Caller ---> [ Spring CGLIB Proxy Interceptor ]                                 |
|                     |                                                             |
|                     v Submits Callable/Runnable to TaskExecutor                   |
|    +-----------------------------------------------------+                        |
|    | ThreadPoolTaskExecutor (or Virtual Thread Executor) |                        |
|    | Worker Thread picks up task -> Executes Real Method |                        |
|    +-----------------------------------------------------+                        |
|                     | Returns CompletableFuture<T> to Caller immediately          |
|                                                                                   |
| 3. @Scheduled Execution Path                                                      |
|    ThreadPoolTaskScheduler Worker Thread (Timer Loop)                             |
|    Evaluates Cron / Trigger -> Invokes Target Bean Method Directly                |
+-----------------------------------------------------------------------------------+
```

During application context bootstrap, `ScheduledAnnotationBeanPostProcessor` scans all managed beans for methods annotated with `@Scheduled` and schedules them against the configured `TaskScheduler`. For `@Async`, `AsyncAnnotationBeanPostProcessor` generates a CGLIB or JDK dynamic proxy around the target bean. When a client invokes an `@Async` method, the proxy intercepts the call, packages the execution as a `Callable` or `Runnable`, and submits it to the designated `TaskExecutor`, returning immediately to the caller.

---

## 4. Syntax & Usage

### Enable Annotations
```java
package com.example.platform.config;

import org.springframework.context.annotation.Configuration;
import org.springframework.scheduling.annotation.EnableAsync;
import org.springframework.scheduling.annotation.EnableScheduling;

@Configuration
@EnableAsync
@EnableScheduling
public class AsyncScheduleConfig {
}
```

### Essential Cron Expression Syntax
```
┌───────────── second (0-59)
│ ┌───────────── minute (0-59)
│ │ ┌───────────── hour (0-23)
│ │ │ ┌───────────── day of the month (1-31)
│ │ │ │ ┌───────────── month (1-12 or JAN-DEC)
│ │ │ │ │ ┌───────────── day of the week (0-7 or MON-SUN, 0 or 7 is Sunday)
│ │ │ │ │ │
* * * * * *
```
Examples:
- `0 0 2 * * *` = Every day at 02:00:00 AM
- `0 */15 * * * *` = Every 15 minutes
- `@daily` / `@midnight` / `@hourly` = Spring Boot supported macro shorthands

---

## 5. Practical Examples

### Example 1: Configured ThreadPoolTaskExecutor
```java
package com.example.platform.async.config;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.scheduling.concurrent.ThreadPoolTaskExecutor;

import java.util.concurrent.Executor;
import java.util.concurrent.ThreadPoolExecutor;

@Configuration
public class CustomExecutorConfig {

    @Bean(name = "platformTaskExecutor")
    public Executor platformTaskExecutor() {
        ThreadPoolTaskExecutor executor = new ThreadPoolTaskExecutor();
        executor.setCorePoolSize(8);
        executor.setMaxPoolSize(32);
        executor.setQueueCapacity(500);
        executor.setThreadNamePrefix("platform-worker-");
        
        // CallerRunsPolicy throttles incoming submissions when queue is saturated
        executor.setRejectedExecutionHandler(new ThreadPoolExecutor.CallerRunsPolicy());
        executor.setWaitForTasksToCompleteOnShutdown(true);
        executor.setAwaitTerminationSeconds(60);
        executor.initialize();
        return executor;
    }
}
```

### Example 2: Non-Blocking Service Returning `CompletableFuture<T>`
```java
package com.example.platform.async.service;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.scheduling.annotation.Async;
import org.springframework.stereotype.Service;

import java.util.concurrent.CompletableFuture;

@Service
public class CreditScoringService {

    private static final Logger log = LoggerFactory.getLogger(CreditScoringService.class);

    public record CreditReport(String customerId, int score, String tier) {}

    @Async("platformTaskExecutor")
    public CompletableFuture<CreditReport> evaluateCreditScore(String customerId) {
        log.info("Calculating credit score for customer: {} on thread: {}",
                customerId, Thread.currentThread().getName());

        try {
            Thread.sleep(800); // Simulate network latency calling credit bureaus
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }

        CreditReport report = new CreditReport(customerId, 785, "EXCELLENT");
        return CompletableFuture.completedFuture(report);
    }
}
```

### Example 3: Scheduled Recurring Maintenance Tasks
```java
package com.example.platform.scheduling;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.scheduling.annotation.Scheduled;
import org.springframework.stereotype.Component;

import java.time.Instant;
import java.util.concurrent.TimeUnit;

@Component
public class CacheEvictionScheduleService {

    private static final Logger log = LoggerFactory.getLogger(CacheEvictionScheduleService.class);

    // Runs every 10 seconds with 5-second initial delay after boot
    @Scheduled(initialDelay = 5, fixedRate = 10, timeUnit = TimeUnit.SECONDS)
    public void purgeStaleSessions() {
        log.info("Purging stale user sessions at: {}", Instant.now());
    }

    // Runs 30 seconds after the COMPLETION of the previous execution
    @Scheduled(fixedDelay = 30000)
    public void syncProductCatalog() {
        log.info("Synchronizing catalog changes from inventory warehouse");
    }

    // Cron job: Runs at 03:00 AM every Sunday
    @Scheduled(cron = "0 0 3 * * SUN")
    public void generateWeeklyAuditReport() {
        log.info("Generating and archiving weekly compliance audit reports");
    }
}
```

### Example 4: The Self-Invocation `@Async` / `@Scheduled` Anti-Pattern and Fix
```java
// ANTI-PATTERN: Calling an @Async or @Scheduled method from within the same class.
// Because the call bypasses the Spring CGLIB proxy, it executes SYNCHRONOUSLY on the caller thread!
// @Service
// public class ReportService {
//     public void generate() {
//         this.runAsyncReport(); // Proxy bypassed! Runs synchronously!
//     }
//     @Async
//     public void runAsyncReport() { ... }
// }

// FIX: Inject the bean into a distinct service or invoke via self-injected proxy
package com.example.platform.async.service;

import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.context.annotation.Lazy;
import org.springframework.scheduling.annotation.Async;
import org.springframework.stereotype.Service;

import java.util.concurrent.CompletableFuture;

@Service
public class ReportOrchestratorService {

    private final ReportWorkerService workerService;

    public ReportOrchestratorService(ReportWorkerService workerService) {
        this.workerService = workerService;
    }

    public void processReport(String reportId) {
        // Cross-bean boundary invocation ensures the Spring CGLIB proxy intercepts the call
        workerService.executeReportGeneration(reportId);
    }
}

@Service
class ReportWorkerService {

    @Async("platformTaskExecutor")
    public CompletableFuture<Void> executeReportGeneration(String reportId) {
        // Correctly executes on dedicated background thread
        return CompletableFuture.completedFuture(null);
    }
}
```

### Example 5: Clustered Scheduling with ShedLock (Preventing Duplicate Execution)
```java
package com.example.platform.scheduling;

import net.javacrumbs.shedlock.spring.annotation.EnableSchedulerLock;
import net.javacrumbs.shedlock.spring.annotation.SchedulerLock;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.context.annotation.Configuration;
import org.springframework.scheduling.annotation.Scheduled;
import org.springframework.stereotype.Component;

@Configuration
@EnableSchedulerLock(defaultLockAtMostFor = "10m")
class ShedLockConfig {}

@Component
public class ClusteredBillingScheduler {

    private static final Logger log = LoggerFactory.getLogger(ClusteredBillingScheduler.class);

    // Even if 10 Kubernetes pods are running, exactly ONE pod acquires the lock and executes this job
    @Scheduled(cron = "0 0 1 * * *") // Daily at 01:00 AM
    @SchedulerLock(
            name = "dailyBillingExecutionLock",
            lockAtLeastFor = "30s",  // Prevents re-execution if clock skew exists between nodes
            lockAtMostFor = "5m"     // Automatically releases lock if node crashes mid-execution
    )
    public void executeDailyBilling() {
        log.info("Lock acquired successfully. Processing daily subscriptions...");
    }
}
```

---

## 6. Best Practices

1. **Always Define a Custom `ThreadPoolTaskExecutor`**: Never rely on Spring Boot's default unconfigured `SimpleAsyncTaskExecutor`, which creates a new unpooled thread for every single `@Async` invocation.
2. **Never Call `@Async` or `@Scheduled` Methods Internally Within the Same Class**: Direct internal calls (`this.method()`) bypass the Spring CGLIB proxy and execute synchronously on the caller thread.
3. **Use ShedLock for Clustered Environments**: Without a distributed lock coordinator, every running Kubernetes pod will execute the same `@Scheduled` job simultaneously, causing duplicate billing or race conditions.
4. **Configure Graceful Shutdown**: Set `setWaitForTasksToCompleteOnShutdown(true)` and `setAwaitTerminationSeconds(30)` to allow in-flight background jobs to complete cleanly during deployments.
5. **Adopt Virtual Threads for I/O Bound Tasks**: In Java 21, enable `spring.threads.virtual.enabled=true` so `@Async` methods run on lightweight Virtual Threads without tuning thread pool boundaries.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Invoking `@Async` method from within the same class (`this.doAsync()`) | Bypasses the Spring CGLIB dynamic proxy; runs synchronously on the calling thread | Extract the method to a separate `@Service` bean and inject it. |
| Forgetting `@EnableAsync` or `@EnableScheduling` | Annotations are ignored silently; tasks never execute or execute synchronously | Add `@EnableAsync` and `@EnableScheduling` on a `@Configuration` class. |
| Unbounded queue capacity (`Integer.MAX_VALUE`) on `ThreadPoolTaskExecutor` | Pool never scales beyond core pool size until queue fills, causing OOM | Set explicit queue capacity (e.g., 200) and specify a `RejectedExecutionHandler`. |
| Unhandled exceptions in `@Async void` methods | Exceptions are lost silently and never propagated to the caller | Implement `AsyncUncaughtExceptionHandler` to log unhandled async errors. |
| Default single-threaded `TaskScheduler` | A long-running `@Scheduled` job blocks all other `@Scheduled` tasks in the app | Configure `spring.task.scheduling.pool.size=5` in `application.properties`. |
| Running `@Scheduled` on multi-node Kubernetes clusters without locks | Every pod runs the job simultaneously, causing duplicate writes | Integrate ShedLock or Quartz with a shared database lock table. |

---

## 8. Performance & Security Notes

- **ThreadPool Saturation**: Always configure a rejection policy (e.g., `ThreadPoolExecutor.CallerRunsPolicy` or `AbortPolicy`). Uncontrolled task submission without backpressure causes heap exhaustion.
- **SecurityContext Propagation**: Security credentials in `SecurityContextHolder` are bound to `ThreadLocal` by default and are lost when transitioning to an `@Async` thread. Use `DelegatingSecurityContextAsyncTaskExecutor` to propagate authentication context across threads.
- **Virtual Threads Impact**: Enabling Virtual Threads allows millions of concurrent asynchronous tasks without dedicating heavy OS kernel threads.

---

## 9. Comparison / When to Use What

| Mechanism | `@Async` + `@Scheduled` | Quartz Scheduler | Message Queue (RabbitMQ / Kafka) |
|---|---|---|---|
| **Architecture** | In-process Spring runtime | Multi-node clustered database | Distributed event log / broker |
| **Persistence** | None (In-memory) | High (JDBC JobStore) | Maximum durability |
| **Dynamic Jobs** | Fixed code annotations | Dynamically added via DB/API | Event-triggered |
| **Cluster Safety** | Requires ShedLock | Built-in cluster management | Built-in partition/consumer groups |
| **Best For** | Internal tasks, cleanups, emails | Complex enterprise scheduling | Event-driven microservices |

---

## 10. Interview Questions

<details>
<summary>1. Why does calling an `@Async` method from another method within the same class run synchronously?</summary>
Spring implements `@Async` via CGLIB or JDK dynamic proxies. When an external bean calls the method, it calls the proxy, which delegates the invocation to an executor thread. When called internally within the same class (`this.method()`), the proxy is bypassed, executing the method directly on the caller thread.
</details>

<details>
<summary>2. What is the difference between `fixedRate` and `fixedDelay` in `@Scheduled`?</summary>
`fixedRate` measures the time between the *start* of the previous execution and the start of the next execution. `fixedDelay` measures the time between the *completion* of the previous execution and the start of the next execution. If a task takes longer than the rate, `fixedRate` runs subsequent tasks immediately (or concurrently if configured), whereas `fixedDelay` always waits for the previous execution to finish.
</details>

<details>
<summary>3. What is the default task executor used by Spring Boot for `@Async` if none is configured?</summary>
In Spring Boot 3, if no custom executor bean is provided, Spring auto-configures a `ThreadPoolTaskExecutor` (or `SimpleAsyncTaskExecutor` in core Spring). However, production applications must always configure a dedicated named `ThreadPoolTaskExecutor` to control pool sizes, queue capacity, and rejection policies.
</details>

<details>
<summary>4. What happens when the work queue of a ThreadPoolTaskExecutor fills up?</summary>
When the queue reaches capacity and the pool has reached `maxPoolSize`, the configured `RejectedExecutionHandler` is invoked. By default, it throws `RejectedExecutionException` (AbortPolicy). With `CallerRunsPolicy`, the calling thread executes the task itself, applying natural backpressure.
</details>

<details>
<summary>5. How do you propagate Spring Security context across `@Async` thread boundaries?</summary>
By wrapping the task executor in a `DelegatingSecurityContextAsyncTaskExecutor`, or by configuring `SecurityContextHolder.setStrategyName(SecurityContextHolder.MODE_INHERITABLETHREADLOCAL)` to propagate the authentication tokens from the parent thread to child threads.
</details>

<details>
<summary>6. How does ShedLock prevent duplicate `@Scheduled` executions in clustered Kubernetes pods?</summary>
ShedLock uses a shared external database table (e.g., in PostgreSQL or Redis) to store lock records containing job name, lock expiration time, and node identifier. When the scheduled trigger fires, each pod attempts to update the lock record; only the first pod succeeds, while the other pods abort execution.
</details>

<details>
<summary>7. How are exceptions handled in `@Async` methods that return `void` versus `CompletableFuture<T>`?</summary>
For methods returning `CompletableFuture<T>`, exceptions are captured inside the future and retrieved via `future.exceptionally()` or `future.get()`. For methods returning `void`, exceptions cannot be returned to the caller; they must be caught by a registered `AsyncUncaughtExceptionHandler` bean.
</details>

<details>
<summary>8. How does Java 21 Virtual Threads affect Spring `@Async` performance?</summary>
Enabling Virtual Threads (`spring.threads.virtual.enabled=true`) replaces OS thread pools with lightweight virtual threads scheduled by the JVM. Asynchronous I/O blocking operations yield the underlying carrier thread, allowing millions of concurrent `@Async` operations without thread pool tuning.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1 (Easy): Create a scheduled job with fixedRate</summary>
Write a Spring component that logs "Heartbeat" every 3 seconds using `@Scheduled(fixedRate = 3000)`.
</details>

<details>
<summary>Exercise 2 (Easy): Configure @EnableAsync and an asynchronous void method</summary>
Configure `@EnableAsync` and write an `@Async` method that simulates a 2-second audit log entry.
</details>

<details>
<summary>Exercise 3 (Medium): Implement an Async method returning CompletableFuture</summary>
Write a service method `CompletableFuture<String> fetchExternalData()` that executes on a custom executor named `"ioExecutor"`.
</details>

<details>
<summary>Exercise 4 (Medium): Build an AsyncUncaughtExceptionHandler</summary>
Implement `AsyncConfigurer` to register a global `AsyncUncaughtExceptionHandler` that logs the method name and exception message of all failing void async methods.
</details>

<details>
<summary>Exercise 5 (Hard): Configure ThreadPoolTaskExecutor with CallerRunsPolicy and Graceful Shutdown</summary>
Configure a `ThreadPoolTaskExecutor` bean with 4 core, 16 max, 100 queue capacity, `CallerRunsPolicy`, and 30-second shutdown timeout.
</details>

<details>
<summary>Exercise 6 (Hard): Integration test testing an @Async method using Awaitility</summary>
Write a `@SpringBootTest` that invokes an asynchronous method and uses `org.awaitility.Awaitility` to assert that the background state changed within 5 seconds.
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------+
|                    Spring @Async & @Scheduled Cheat Sheet                       |
+---------------------------------------------------------------------------------+
| Enable:             @EnableAsync, @EnableScheduling                             |
| Schedulers:         @Scheduled(fixedRate = 1000)  -> Measure from task start    |
|                     @Scheduled(fixedDelay = 1000) -> Measure from task finish   |
|                     @Scheduled(cron = "0 0 2 * * *") -> Calendar-based          |
| Clustered Safety:   @SchedulerLock (ShedLock) on multi-node Kubernetes pods     |
| Async Return Types: void, CompletableFuture<T>                                  |
| Crucial Trap:       Never call @Async internally within same class (bypasses proxy)|
| Thread Pool Config: ThreadPoolTaskExecutor with CallerRunsPolicy                |
| Java 21 Setting:    spring.threads.virtual.enabled=true                         |
+---------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Spring Framework Task Execution and Scheduling](https://docs.spring.io/spring-framework/reference/integration/scheduling.html)
- [ShedLock GitHub Repository](https://github.com/lukas-krecan/ShedLock)
- [Java Concurrency in Practice (Goetz et al.)](https://jcip.net/)
