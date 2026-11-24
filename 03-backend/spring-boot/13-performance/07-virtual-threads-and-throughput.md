# Virtual Threads and Throughput Scaling in Spring Boot

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x |
| **Spring Framework Version** | 6.2.x |
| **Java Version** | 21 LTS |
| **Module** | 13-performance |
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
In the traditional Java concurrency model ("thread-per-request"), each Java `Thread` maps 1:1 directly to an underlying Operating System (OS) platform thread. While this provides a clean, sequential programming style:
- OS platform threads are heavy: each consumes approximately 1MB of stack memory and incurs high OS kernel scheduling overhead.
- A standard server running on 4GB RAM is practically limited to 200–500 platform threads before exhausting system memory or context-switching capacity.
- In modern I/O-bound microservices where threads spend 95% of their time waiting on database queries, REST calls, or Kafka messages, platform threads sit completely idle, wasting hardware resources.

To circumvent this, reactive programming (Spring WebFlux, Project Reactor) emerged. However, reactive programming requires a non-blocking mental model, difficult debugging, cryptic stack traces, and complete incompatibility with traditional blocking JDBC drivers.

### Historical Evolution
**Project Loom** in **Java 21 LTS** revolutionized the Java platform by introducing **Virtual Threads** (JEP 444). Virtual Threads are lightweight, user-mode threads managed entirely by the HotSpot JVM rather than the OS kernel.

Starting in **Spring Boot 3.2**, enabling Virtual Threads across the entire application—embedded Tomcat/Jetty web servers, `@Async` tasks, Spring MVC dispatchers, and messaging listeners—requires just a single configuration flag:
`spring.threads.virtual.enabled=true`.

### Production Relevance
Virtual Threads deliver the "Holy Grail" of server engineering:
- **Maximum Hardware Throughput**: Scale from 200 concurrent requests to **50,000+ concurrent requests** on a single node.
- **Synchronous, Blocking Code Style**: Developers write clean, readable sequential code (`try-catch`, standard `for` loops) with full debuggability and traditional stack traces.
- **Instant Reactive-Level Throughput**: Achieve the throughput of non-blocking reactive engines without adopting complex reactive APIs.

---

## 2. Core Concepts

### Architectural Primitives
1. **Platform Thread**: A heavy OS-scheduled thread (1:1 mapping). Costly to create, limited in quantity.
2. **Virtual Thread**: A lightweight JVM-managed thread ($M:N$ mapping). Millions can exist concurrently. Stack frames reside in the JVM heap and expand/shrink dynamically.
3. **Carrier Thread**: A physical platform thread (managed by a JVM `ForkJoinPool`) that temporarily executes a Virtual Thread.
4. **Mounting & Unmounting**:
   - **Mounting**: A carrier thread begins executing a virtual thread.
   - **Unmounting (Yielding)**: When the virtual thread performs a blocking I/O operation (e.g. socket read, JDBC query, `Thread.sleep`), the JVM unmounts it from the carrier thread. The carrier thread immediately executes another virtual thread!
5. **Thread Pinning**: Occurs when a virtual thread cannot unmount during a blocking operation because it is executing inside a `synchronized` block/method or a native JNI call, temporarily stalling the carrier thread.

---

## 3. How It Works Internally

### Virtual Thread Mounting and Unmounting Cycle

```
HOTSPOT JVM RUNTIME (JAVA 21)
+------------------------------------------------------------------------------------+
|  VIRTUAL THREADS RUN QUEUE (Thousands of lightweight virtual threads in heap)       |
|  [ VT-1 ]  [ VT-2 ]  [ VT-3 ]  [ VT-4 ] ... [ VT-50,000 ]                          |
+------------------------------------------------------------------------------------+
                                         |
                                         v
+------------------------------------------------------------------------------------+
|  CARRIER THREAD POOL (ForkJoinPool - Exactly equal to CPU Core Count, e.g. 8 Cores)|
|  [ Carrier-1 ]  [ Carrier-2 ]  [ Carrier-3 ] ... [ Carrier-8 ]                     |
+------------------------------------------------------------------------------------+

EXECUTION TIMELINE:
1. Carrier-1 MOUNTS VT-1 -> Executes OrderService.placeOrder()
2. VT-1 invokes restClient.get() (Blocking Socket Read)
3. JVM intercepts socket call -> COPIES VT-1 stack frame to JVM HEAP
4. Carrier-1 UNMOUNTS VT-1 -> Carrier-1 is instantly FREE!
5. Carrier-1 immediately MOUNTS VT-2 from run queue and executes code!
6. OS signals network data arrived for VT-1 -> JVM places VT-1 back on run queue
7. ANY available carrier thread (e.g. Carrier-4) MOUNTS VT-1 and resumes execution!
```

---

## 4. Syntax & Configuration Reference

### Enabling Virtual Threads in Spring Boot 3.2+ (`application.yml`)

```yaml
spring:
  threads:
    virtual:
      enabled: true # Activates Virtual Threads for Tomcat, @Async, TaskSchedulers, and MVC

server:
  tomcat:
    threads:
      # In Virtual Thread mode, Tomcat spawns a new Virtual Thread per request!
      # max and min-spare platform thread limits are bypassed automatically.
      max-connections: 50000
      accept-count: 10000
```

### Detecting Carrier Thread Pinning via JVM Flags
```bash
# Add this flag during development/staging to log stack traces of pinned threads
JAVA_OPTS="-Djdk.tracePinnedThreads=full"
```

---

## 5. Practical Examples

### Example 1: Verifying Virtual Thread Execution in Spring MVC
```java
package com.example.performance.virtualthreads;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

@RestController
@RequestMapping("/api/threads")
public class ThreadInspectionController {

    private static final Logger log = LoggerFactory.getLogger(ThreadInspectionController.class);

    @GetMapping("/current")
    public String checkCurrentThread() {
        Thread current = Thread.currentThread();
        boolean isVirtual = current.isVirtual();

        log.info("Request handled by thread: [name='{}', isVirtual={}]", 
            current.getName(), isVirtual);

        // When spring.threads.virtual.enabled=true, returns:
        // "Thread: VirtualThread[#34,tomcat-handler-0]/runnable@ForkJoinPool-1-worker-1, Is Virtual: true"
        return "Thread: " + current + ", Is Virtual: " + isVirtual;
    }
}
```

### Example 2: Concurrent Fan-Out with Structured Concurrency (Java 21)
```java
package com.example.performance.virtualthreads;

import org.springframework.stereotype.Service;

import java.time.Duration;
import java.util.concurrent.StructuredTaskScope;
import java.util.function.Supplier;

@Service
public class AggregationService {

    public record DashboardData(String userProfile, String accountBalance, String recommendations) {}

    public DashboardData fetchDashboard(String userId) {
        // StructuredTaskScope.ShutdownOnFailure cancels sibling tasks if any one task fails!
        try (var scope = new StructuredTaskScope.ShutdownOnFailure()) {

            // Spawns 3 lightweight Virtual Threads concurrently!
            Supplier<String> profileSubtask = scope.fork(() -> fetchProfile(userId));
            Supplier<String> balanceSubtask = scope.fork(() -> fetchBalance(userId));
            Supplier<String> recoSubtask    = scope.fork(() -> fetchRecommendations(userId));

            // Wait for all subtasks to complete or fail
            scope.join();
            scope.throwIfFailed(); // Propagate exception if any task failed

            // Aggregate results cleanly
            return new DashboardData(
                profileSubtask.get(),
                balanceSubtask.get(),
                recoSubtask.get()
            );
        } catch (Exception ex) {
            throw new RuntimeException("Failed to aggregate dashboard data", ex);
        }
    }

    private String fetchProfile(String id) throws InterruptedException {
        Thread.sleep(100); // Simulates blocking REST call
        return "ProfileData-" + id;
    }

    private String fetchBalance(String id) throws InterruptedException {
        Thread.sleep(150); // Simulates blocking DB call
        return "Balance-$10,500";
    }

    private String fetchRecommendations(String id) throws InterruptedException {
        Thread.sleep(120); // Simulates blocking Recommendation engine
        return "Recommendations-[ItemA, ItemB]";
    }
}
```

### Example 3: Anti-Pattern vs. Production Fix (Carrier Thread Pinning)
```java
// ANTI-PATTERN: Synchronized method executing blocking I/O
// Problem: 'synchronized' prevents the HotSpot JVM from unmounting the virtual thread!
// The underlying carrier thread (OS thread) is pinned and cannot run other virtual threads!
@Service
public class PinnedSyncService {

    private final RestClient restClient;

    public PinnedSyncService(RestClient.Builder builder) {
        this.restClient = builder.baseUrl("https://partner.internal").build();
    }

    public synchronized String callPartnerService() {
        // CARRIER THREAD PINNED: Synchronized block + blocking socket read!
        return restClient.get().uri("/data").retrieve().body(String.class);
    }
}

// PRODUCTION FIX: Replace synchronized with ReentrantLock
@Service
public class UnpinnedLockService {

    private final RestClient restClient;
    private final ReentrantLock lock = new ReentrantLock();

    public UnpinnedLockService(RestClient.Builder builder) {
        this.restClient = builder.baseUrl("https://partner.internal").build();
    }

    public String callPartnerService() {
        lock.lock(); // ReentrantLock fully supports Virtual Thread unmounting!
        try {
            // When restClient blocks on socket, virtual thread UNMOUNTS cleanly!
            return restClient.get().uri("/data").retrieve().body(String.class);
        } finally {
            lock.unlock();
        }
    }
}
```

### Example 4: Virtual Thread TaskExecutor for `@Async` Tasks
```java
package com.example.performance.config;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.core.task.SimpleAsyncTaskExecutor;
import org.springframework.core.task.TaskExecutor;
import org.springframework.scheduling.annotation.EnableAsync;

@Configuration
@EnableAsync
public class AsyncVirtualThreadConfiguration {

    @Bean
    public TaskExecutor taskExecutor() {
        // SimpleAsyncTaskExecutor in Virtual Thread mode spawns a new Virtual Thread per @Async task!
        SimpleAsyncTaskExecutor executor = new SimpleAsyncTaskExecutor("async-vt-");
        executor.setVirtualThreads(true);
        return executor;
    }
}
```

### Example 5: Protecting Downstream Systems with `Semaphore` Throttling
Because Virtual Threads allow 50,000 requests to run concurrently, downstream databases or third-party APIs can be easily overwhelmed without pooling. Replace thread pools with semaphores:

```java
package com.example.performance.virtualthreads;

import org.springframework.stereotype.Service;

import java.util.concurrent.Semaphore;

@Service
public class ThrottledPartnerGateway {

    // Restricts maximum concurrent outgoing HTTP requests to 50
    private final Semaphore concurrencyGate = new Semaphore(50);

    public String queryPartnerWithThrottle() {
        try {
            concurrencyGate.acquire();
            try {
                return executePartnerCall();
            } finally {
                concurrencyGate.release();
            }
        } catch (InterruptedException ex) {
            Thread.currentThread().interrupt();
            throw new RuntimeException("Interrupted while waiting for gateway permit", ex);
        }
    }

    private String executePartnerCall() {
        // Blocking HTTP call
        return "SUCCESS";
    }
}
```

---

## 6. Best Practices & Production Guidelines

1. **Never Pool Virtual Threads**: Platform threads were pooled (`ThreadPoolExecutor`) because creating them was expensive. Virtual Threads are cheap and ephemeral ($< 1\mu\text{s}$ creation time, $< 1\text{KB}$ memory). Create a new Virtual Thread per task; never wrap Virtual Threads in a `newFixedThreadPool()`.
2. **Eliminate `synchronized` Blocks Around Blocking I/O**: Replace `synchronized (lock)` with `java.util.concurrent.locks.ReentrantLock`. `ReentrantLock` allows the JVM to unmount virtual threads during socket waits, preventing carrier thread pinning.
3. **Use Semaphores to Limit Concurrency**: In platform-thread applications, thread pool sizes (e.g. 200) naturally rate-limited outbound traffic. In Virtual Thread systems, use `Semaphore` to protect databases and external partners from being flooded by 50,000 concurrent requests.
4. **Be Cautious with `ThreadLocal` Memory**: If your code stores large objects (e.g. 5MB buffers) in `ThreadLocal`, spawning 50,000 virtual threads will allocate 250GB of heap memory, causing an instant `OutOfMemoryError`. Keep `ThreadLocal` values small or adopt Java 21 **Scoped Values**.
5. **Enable `-Djdk.tracePinnedThreads=full` in Staging**: Run load tests with this JVM flag to detect third-party libraries that still use `synchronized` blocks around blocking calls.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Pooling Virtual Threads | Defeats Loom architecture; introduces queue management overhead for zero gain. | Instantiate per task: `Thread.ofVirtual().start(task)`. |
| Pinned carrier threads | `synchronized` blocks hold carrier OS thread during blocking I/O, stalling other virtual threads. | Refactor code to use `ReentrantLock`. |
| Memory bloat via `ThreadLocal` buffers | 50,000 virtual threads each holding a 1MB `ThreadLocal` byte buffer consume 50GB RAM. | Avoid large `ThreadLocal` buffers; use object pooling or Scoped Values. |
| Database connection pool starvation | 10,000 virtual threads attempt to query DB simultaneously; all timeout against a pool of 20. | Use a `Semaphore` to limit concurrent database access. |
| Using Virtual Threads for CPU-bound tasks | Virtual Threads provide zero benefit for heavy crypto/math computation, adding scheduler overhead. | Use standard platform threads (`ForkJoinPool`) for CPU-bound work. |
| Overlooking native JNI method calls | Native C/C++ library invocations pin carrier threads during execution. | Ensure JNI native calls complete quickly or run on dedicated platform threads. |

---

## 8. Performance & Security Notes

### CPU-Bound vs. I/O-Bound Workloads
- **I/O-Bound Workloads (REST, JDBC, Kafka)**: Virtual Threads deliver dramatic 10x–50x throughput increases because carrier threads spend 99% of time running active tasks rather than waiting on network sockets.
- **CPU-Bound Workloads (Image processing, video encoding, cryptography)**: Virtual Threads offer **no performance benefit**. If all CPU cores are 100% utilized calculating instructions, no concurrency model can exceed physical hardware limits.

### Denial of Service Resilience
- In traditional Spring MVC, an attacker opening 500 slow HTTP connections (Slowloris attack) exhausts the Tomcat thread pool (`max-threads=200`), taking the application completely offline.
- With Virtual Threads enabled, 500 slow connections allocate negligible heap memory ($< 1\text{MB}$ total), leaving the application completely responsive to legitimate users.

---

## 9. Comparison / When to Use What

| Feature | Platform Threads (Traditional) | Virtual Threads (Java 21) | Reactive (WebFlux / RxJava) |
|---|---|---|---|
| **Programming Model** | Synchronous / Imperative | **Synchronous / Imperative** | Asynchronous / Functional |
| **Max Concurrent Tasks** | 200 – 1,000 | **1,000,000+** | 1,000,000+ |
| **Memory Per Thread** | $\approx 1\text{ MB}$ (OS Stack) | **$\approx 1\text{ KB}$ (JVM Heap)** | No threads (Event loop callbacks) |
| **Blocking I/O Support** | Yes (Blocks OS thread) | **Yes (Unmounts Carrier)** | No (Blocking breaks event loop!) |
| **Debugging & Stack Traces** | Easy & Intuitive | **Easy & Intuitive** | Complex / Difficult to trace |
| **JDBC / JPA Compatibility**| 100% Native | **100% Native** | Requires R2DBC |

---

## 10. Interview Questions & Answers

### Q1: What is the fundamental difference between a Platform Thread and a Virtual Thread in Java 21?
<details>
<summary>Click to view answer</summary>

A **Platform Thread** is a thin wrapper around an Operating System thread (1:1 mapping). Its creation is expensive, its stack size is fixed (typically 1MB), and scheduling is handled by the OS kernel.  
A **Virtual Thread** is an application-level thread managed entirely by the HotSpot JVM ($M:N$ mapping). Its stack frames are allocated in the Java heap and resize dynamically. Millions of virtual threads can be scheduled onto a small pool of carrier OS platform threads.
</details>

### Q2: What happens when a Virtual Thread executes a blocking operation like `Socket.read()`?
<details>
<summary>Click to view answer</summary>

In Java 21, the HotSpot runtime intercepts the blocking I/O call. Instead of blocking the underlying OS carrier thread:
1. The virtual thread's call stack is copied to the Java heap.
2. The virtual thread **unmounts** from the carrier thread, transitioning to `WAITING` state.
3. The carrier OS thread is immediately freed to execute other runnable virtual threads.
4. When the OS kernel signals that network bytes are ready, the JVM schedules the virtual thread onto any available carrier thread, restores its stack frames, and resumes execution seamlessly.
</details>

### Q3: What is "Carrier Thread Pinning" and why is it dangerous?
<details>
<summary>Click to view answer</summary>

Carrier thread pinning occurs when a virtual thread enters a blocking state but **cannot unmount** from its carrier thread. This happens when:
- The blocking operation occurs inside a `synchronized` block or method.
- The virtual thread is executing a native method (JNI).
If all carrier threads (which equal the number of CPU cores) become pinned simultaneously, the entire JVM stalls and no other virtual threads can execute.
</details>

### Q4: Why should you NEVER pool Virtual Threads?
<details>
<summary>Click to view answer</summary>

Platform threads were pooled using `ThreadPoolExecutor` because allocating an OS thread involves expensive kernel syscalls and 1MB of memory. In contrast, Virtual Threads cost less than 1KB and take nanoseconds to create. Pooling them introduces unnecessary lock contention, thread lifecycle management, and queue overhead. The intended design pattern is to create a new Virtual Thread per task and let it be garbage collected when finished.
</details>

### Q5: How do you enable Virtual Threads in Spring Boot 3.2+?
<details>
<summary>Click to view answer</summary>

Add `spring.threads.virtual.enabled: true` to `application.yml`. Spring Boot automatically switches embedded Tomcat/Jetty to use a `VirtualThreadExecutor` for incoming HTTP requests, binds `@Async` executors to Virtual Threads, and configures Spring MVC and message listeners to execute on Virtual Threads.
</details>

### Q6: Can Virtual Threads replace Spring WebFlux completely?
<details>
<summary>Click to view answer</summary>

For 95% of standard I/O-bound enterprise applications, **yes**. Virtual Threads provide the high concurrency and scalability of WebFlux while retaining the clean, familiar, synchronous imperative programming model and full compatibility with existing libraries (JPA, JDBC, Spring Security). However, WebFlux remains valuable for reactive stream processing, backpressure handling, and infinite server-sent event (SSE) streams.
</details>

### Q7: What are Scoped Values in Java 21 and how do they relate to Virtual Threads?
<details>
<summary>Click to view answer</summary>

`ScopedValue` (JEP 446 / Project Loom) is a modern alternative to `ThreadLocal`. Because Virtual Threads are spawned in the millions, mutable `ThreadLocal` maps can incur high memory overhead. `ScopedValue` allows immutable data to be safely passed down a call tree for a bounded execution scope, sharing data between parent and child virtual threads with zero memory duplication.
</details>

### Q8: How do you detect carrier thread pinning in production?
<details>
<summary>Click to view answer</summary>

1. Set the JVM system property `-Djdk.tracePinnedThreads=full` (prints stack traces to stdout when pinning occurs).
2. Monitor JDK Flight Recorder (JFR) event `jdk.VirtualThreadPinned`.
3. Track JFR metric `jdk.VirtualThreadSubmitFailed` or inspect carrier thread pool utilization in Micrometer.
</details>

---

## 11. Practice Exercises

### Exercise 1: Enable Virtual Threads in application.yml (Easy)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Write the `application.yml` configuration to enable Virtual Threads in Spring Boot 3.4 and set Tomcat max connections to 20,000.

**Solution**:
```yaml
spring:
  threads:
    virtual:
      enabled: true

server:
  tomcat:
    max-connections: 20000
    accept-count: 5000
```
</details>

### Exercise 2: Refactor Synchronized Code to Prevent Pinning (Medium)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Identify the pinning vulnerability in the following code and refactor it using `ReentrantLock`.

```java
// Vulnerable Code
public class ExternalApiGateway {
    public synchronized String sendData(String payload) {
        return restClient.post().uri("/submit").body(payload).retrieve().body(String.class);
    }
}
```

**Solution**:
```java
package com.example.performance.virtualthreads;

import org.springframework.stereotype.Component;
import org.springframework.web.client.RestClient;

import java.util.concurrent.locks.ReentrantLock;

@Component
public class ExternalApiGateway {

    private final RestClient restClient;
    private final ReentrantLock lock = new ReentrantLock();

    public ExternalApiGateway(RestClient.Builder builder) {
        this.restClient = builder.baseUrl("https://api.partner.com").build();
    }

    public String sendData(String payload) {
        lock.lock(); // ReentrantLock allows Virtual Thread unmounting during socket write/read!
        try {
            return restClient.post()
                .uri("/submit")
                .body(payload)
                .retrieve()
                .body(String.class);
        } finally {
            lock.unlock();
        }
    }
}
```
</details>

### Exercise 3: Parallel URL Scraping with Structured Concurrency (Hard)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Write a service using Java 21 `StructuredTaskScope` that fetches three independent URLs concurrently using Virtual Threads with a total timeout of 2 seconds.

**Solution**:
```java
package com.example.performance.virtualthreads;

import org.springframework.stereotype.Service;
import org.springframework.web.client.RestClient;

import java.time.Instant;
import java.util.List;
import java.util.concurrent.StructuredTaskScope;
import java.util.function.Supplier;

@Service
public class ParallelScraperService {

    private final RestClient restClient = RestClient.create();

    public List<String> scrapeUrls(String url1, String url2, String url3) {
        try (var scope = new StructuredTaskScope.ShutdownOnFailure()) {

            Supplier<String> task1 = scope.fork(() -> restClient.get().uri(url1).retrieve().body(String.class));
            Supplier<String> task2 = scope.fork(() -> restClient.get().uri(url2).retrieve().body(String.class));
            Supplier<String> task3 = scope.fork(() -> restClient.get().uri(url3).retrieve().body(String.class));

            // Wait with a strict 2-second deadline
            scope.joinUntil(Instant.now().plusSeconds(2));
            scope.throwIfFailed(); // Cancel others if any task fails

            return List.of(task1.get(), task2.get(), task3.get());
        } catch (Exception ex) {
            throw new RuntimeException("Parallel scraping failed or timed out", ex);
        }
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------+-------------------------------------------------------------+
| Feature / Concept                           | Rule & Description                                          |
+---------------------------------------------+-------------------------------------------------------------+
| spring.threads.virtual.enabled: true        | Globally switches Spring Boot 3.2+ to Virtual Threads       |
| Ephemeral Threads                           | Never pool Virtual Threads; spawn per task on-demand        |
| Carrier Thread Pool                         | Managed ForkJoinPool with worker threads = CPU Core Count   |
| Unmounting                                  | Virtual thread unmounts from carrier during blocking I/O    |
| Carrier Pinning                             | Triggered by 'synchronized' blocks; fix with ReentrantLock  |
| -Djdk.tracePinnedThreads=full               | JVM flag logging stack traces when pinning occurs           |
| Semaphores                                  | Mandatory to throttle concurrent access to DBs & APIs       |
+---------------------------------------------+-------------------------------------------------------------+
```

---

## 13. Further Reading
- OpenJDK JEP 444: Virtual Threads (Java 21 Specification)
- Spring Boot 3.2 Release Notes: Support for Virtual Threads
- Project Loom: Fibers and Continuations Under the Hood
- Brian Goetz: State of Loom - Concurrency for the Next Decade of Java