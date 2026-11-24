# BlockHound & Reactive Threading Models

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | `01-reactive-programming-and-reactor.md`, `02-webflux-architecture.md`, `07-reactive-streams-and-backpressure.md` |
| **Estimated Reading Time** | 30 minutes |
| **Status** | Active |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework** | 6.2.x / 7.0.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven / Gradle |
| **Tested With** | `io.projectreactor.tools:blockhound:1.0.9.RELEASE`, `spring-boot-starter-webflux:3.4.x` |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
   - [5.1 Installing and Initializing BlockHound in Test Runtimes](#51-installing-and-initializing-blockhound-in-test-runtimes)
   - [5.2 Custom BlockHound Integration for Third-Party Libraries](#52-custom-blockhound-integration-for-third-party-libraries)
   - [5.3 Offloading Blocking Calls with Schedulers.boundedElastic()](#53-offloading-blocking-calls-with-schedulersboundedelastic)
   - [5.4 Anti-Pattern: Blocking Netty Event Loop Threads](#54-anti-pattern-blocking-netty-event-loop-threads)
   - [5.5 WebFlux Threading Model vs Java 21 Virtual Threads](#55-webflux-threading-model-vs-java-21-virtual-threads)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance & Security Notes](#8-performance--security-notes)
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### Problem Statement
Reactive servers like Spring WebFlux on Netty operate on an event-loop architecture. By default, Netty allocates a small pool of worker threads (typically equal to $2 \times \text{available CPU cores}$). All concurrent requests, pipeline transformations, and socket I/O multiplex across these few threads. If any request handler executes a blocking operation—such as a legacy JDBC call, `Thread.sleep()`, synchronous file I/O, or a locking `synchronized` block—that entire event loop thread is frozen. Just a handful of concurrent blocking calls can completely starve the server, leading to cascading request timeouts and system-wide outages.

### Historical Evolution
Detecting blocking calls manually via code review is notoriously error-prone, as blocking code is often buried deep in third-party libraries (e.g., logging appenders, SSL certificate parsers, or XML validators). Project Reactor introduced **BlockHound**, a JVM bytecode instrumentation agent that dynamically intercepts calls to known blocking methods (`Thread.sleep`, socket reads, file locks) and raises a runtime exception whenever they occur on non-blocking threads.

### Real-World Relevance
In production microservices handling tens of thousands of concurrent connections, a single un-isolated blocking DNS lookup or legacy SDK call can collapse throughput from 50,000 req/sec to less than 10 req/sec. BlockHound acts as a hard quality gate in automated CI/CD pipelines, preventing event loop thread starvation bugs from ever reaching production.

---

## 2. Core Concepts

### Non-Blocking Thread Contract
A thread marked as "non-blocking" by Project Reactor (`io.projectreactor.NonBlocking` marker interface or Netty's `FastThreadLocalThread`). Schedulers like `Schedulers.parallel()` and Netty event loops (`reactor-http-nio-*`) implement this interface.

### BlockHound Agent
A Java instrumentation agent that uses ByteBuddy at JVM startup to rewrite bytecode for blocking JDK methods (e.g., `java.lang.Thread.sleep`, `java.io.FileInputStream.readBytes`, `java.net.SocketInputStream.socketRead0`). If a target method is called on a non-blocking thread, BlockHound throws `BlockingOperationError`.

### Schedulers.boundedElastic()
Reactor's dedicated thread pool for offloading legacy blocking calls. It creates elastic worker threads up to a bounded cap (default: $10 \times \text{CPU cores}$), queues surplus tasks up to a configured threshold (default: 100,000), and reclaims idle threads.

### publishOn vs subscribeOn
- `publishOn(Scheduler)`: Alters the execution context *downstream* from the operator.
- `subscribeOn(Scheduler)`: Alters the thread where the *subscription* and initial stream emission take place, affecting the pipeline upstream up to the next `publishOn`.

---

## 3. How It Works Internally

```
+------------------------------------------------------------------------------------+
|                         BlockHound Bytecode Interception                           |
+------------------------------------------------------------------------------------+
|                                                                                    |
|  Thread Execution Flow         Bytecode Interceptor              BlockHound Check  |
|          |                              |                                |         |
|          | Call FileInputStream.read()  |                                |         |
|          |----------------------------->| Injected Prelude Hook          |         |
|          |                              |------------------------------->|         |
|          |                              |                                | Is Current Thread
|          |                              |                                | NonBlocking?
|          |                              |                                |   |     |   |
|          |                              |                                |  YES   NO  ALLOWED
|          |                              |                                |   |     |   |
|          |                              |<-------------------------------+   |     |   |
|          |                              | Throw BlockingOperationError! <----+     |   |
|          |                              |                                          |   |
|          |                              | Execute Native Socket/File I/O <---------+---+
|          v                              v                                                  |
+------------------------------------------------------------------------------------+
```

1. **JVM Startup Initialization**: `BlockHound.install()` registers a `ClassFileTransformer` via ByteBuddy.
2. **Bytecode Modification**: Classes such as `java.lang.Thread`, `java.io.FileDescriptor`, and `java.net.SocketImpl` are instrumented with guards.
3. **Runtime Thread Validation**: Whenever a guarded method executes, the guard inspects `Thread.currentThread()`.
4. **Exception Signaling**: If `currentThread instanceof NonBlocking` returns `true` and the method has not been explicitly allowed via a `BlockHoundIntegration`, a `BlockingOperationError` is thrown immediately.

---

## 4. Syntax & Usage

### Maven Dependency
```xml
<dependency>
    <groupId>io.projectreactor.tools</groupId>
    <artifactId>blockhound</artifactId>
    <version>1.0.9.RELEASE</version>
    <scope>test</scope>
</dependency>
```

### Installing BlockHound in Tests
```java
import org.junit.jupiter.api.BeforeAll;
import reactor.blockhound.BlockHound;

public class BaseReactiveTest {

    @BeforeAll
    static void initBlockHound() {
        BlockHound.install();
    }
}
```

---

## 5. Practical Examples

### 5.1 Installing and Initializing BlockHound in Test Runtimes
```java
package com.example.reactive.config;

import org.junit.jupiter.api.BeforeAll;
import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;
import reactor.blockhound.BlockHound;
import reactor.blockhound.BlockingOperationError;
import reactor.core.publisher.Mono;
import reactor.core.scheduler.Schedulers;

import java.time.Duration;

import static org.assertj.core.api.Assertions.assertThatThrownBy;

public class BlockHoundBasicTest {

    @BeforeAll
    static void setup() {
        BlockHound.install();
    }

    @Test
    @DisplayName("Verify BlockHound catches Thread.sleep on parallel scheduler")
    void shouldCatchBlockingCallOnParallelScheduler() {
        assertThatThrownBy(() -> {
            Mono.fromCallable(() -> {
                Thread.sleep(100); // Prohibited blocking call!
                return "result";
            })
            .subscribeOn(Schedulers.parallel())
            .block(Duration.ofSeconds(2));
        })
        .hasCauseInstanceOf(BlockingOperationError.class);
    }
}
```

### 5.2 Custom BlockHound Integration for Third-Party Libraries
```java
package com.example.reactive.config;

import com.google.auto.service.AutoService;
import reactor.blockhound.BlockHound;
import reactor.blockhound.integration.BlockHoundIntegration;

/**
 * Custom integration permitting specific safe third-party blocking calls
 * such as secure random initialization or certified cryptographic handshakes.
 */
@AutoService(BlockHoundIntegration.class)
public class CustomSecurityBlockHoundIntegration implements BlockHoundIntegration {

    @Override
    public void applyTo(BlockHound.Builder builder) {
        // Allow SecureRandom seed generation during container warmup
        builder.allowBlockingCallsInside(
            "java.security.SecureRandom",
            "nextBytes"
        );

        // Allow trusted JSON parser reflection metadata cache loading
        builder.allowBlockingCallsInside(
            "com.fasterxml.jackson.databind.ObjectMapper",
            "readValue"
        );
    }
}
```

### 5.3 Offloading Blocking Calls with Schedulers.boundedElastic()
```java
package com.example.reactive.service;

import org.springframework.stereotype.Service;
import reactor.core.publisher.Mono;
import reactor.core.scheduler.Schedulers;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;

@Service
public class DocumentProcessingService {

    /**
     * Reads a legacy file synchronously by offloading execution
     * to the boundedElastic thread pool, keeping event loops free.
     */
    public Mono<String> readLegacyDocument(Path filePath) {
        return Mono.fromCallable(() -> Files.readString(filePath))
            .subscribeOn(Schedulers.boundedElastic())
            .timeout(java.time.Duration.ofSeconds(3))
            .onErrorResume(IOException.class, ex -> 
                Mono.error(new DocumentReadException("Failed to read: " + filePath, ex))
            );
    }

    public static class DocumentReadException extends RuntimeException {
        public DocumentReadException(String message, Throwable cause) {
            super(message, cause);
        }
    }
}
```

### 5.4 Anti-Pattern: Blocking Netty Event Loop Threads
```java
// =========================================================================
// ANTI-PATTERN: Blocking Netty I/O worker with Synchronous RestTemplate or JDBC
// =========================================================================
@RestController
public class BadOrderController {

    private final RestTemplate restTemplate = new RestTemplate();

    @GetMapping("/orders/{id}/inventory")
    public Mono<InventoryResponse> checkInventory(@PathVariable String id) {
        // CRITICAL BUG: Calling synchronous restTemplate inside WebFlux controller pipeline.
        // Executes on the Netty event loop thread ("reactor-http-nio-*")!
        // When 16 concurrent requests block here, the entire Netty server freezes.
        return Mono.fromCallable(() -> 
            restTemplate.getForObject("http://inventory-service/stock/" + id, InventoryResponse.class)
        );
    }
}

// =========================================================================
// PRODUCTION FIX: Non-Blocking WebClient or boundedElastic Offload
// =========================================================================
@RestController
public class GoodOrderController {

    private final WebClient webClient;

    public GoodOrderController(WebClient.Builder builder) {
        this.webClient = builder.baseUrl("http://inventory-service").build();
    }

    @GetMapping("/orders/{id}/inventory")
    public Mono<InventoryResponse> checkInventoryFixed(@PathVariable String id) {
        // NON-BLOCKING: Uses Netty channel pipelines without occupying threads
        return webClient.get()
            .uri("/stock/{id}", id)
            .retrieve()
            .bodyToMono(InventoryResponse.class);
    }
}
```

### 5.5 WebFlux Threading Model vs Java 21 Virtual Threads
```java
package com.example.reactive.service;

import org.springframework.stereotype.Service;
import reactor.core.publisher.Mono;
import reactor.core.scheduler.Schedulers;

import java.util.concurrent.Executors;

@Service
public class VirtualThreadSchedulerBridge {

    /**
     * Spring Boot 3.4+ / Java 21: Bridging Reactor to Java Virtual Threads.
     * When legacy synchronous I/O cannot be rewritten to WebClient,
     * virtual threads offer massive concurrency without consuming OS carrier threads.
     */
    private final reactor.core.scheduler.Scheduler virtualThreadScheduler = 
        Schedulers.fromExecutorService(Executors.newVirtualThreadPerTaskExecutor());

    public Mono<String> executeOnVirtualThread(String externalServiceUrl) {
        return Mono.fromCallable(() -> {
            // Synchronous network request running safely on a lightweight Virtual Thread!
            return executeBlockingHttpRequest(externalServiceUrl);
        })
        .subscribeOn(virtualThreadScheduler);
    }

    private String executeBlockingHttpRequest(String url) {
        // Simulated blocking legacy API call
        try {
            Thread.sleep(50);
            return "{\"status\": \"OK\"}";
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            throw new IllegalStateException("Interrupted", e);
        }
    }
}
```

---

## 6. Best Practices

1. **Install BlockHound in CI Build Profiles**: Add `BlockHound.install()` to a common JUnit 5 `@BeforeAll` hook or extension in integration test suites to catch blocking calls before merging pull requests.
2. **Never Call `.block()` in Application Code**: Prohibit calling `.block()`, `.blockFirst()`, or `.toStream()` outside of test code or `@SpringBootApplication` `main()` entry points.
3. **Bound All Elastic Work**: Always use `Schedulers.boundedElastic()` rather than `Schedulers.elastic()` (which was deprecated due to unbounded thread creation that caused `OutOfMemoryError`).
4. **Isolate Blocking Third-Party Drivers**: When using relational databases without R2DBC (e.g., standard JPA/Hibernate), isolate repository invocations inside `Mono.fromCallable(...).subscribeOn(Schedulers.boundedElastic())`.
5. **Tune BoundedElastic for Target Workloads**: Configure `reactor.schedulers.defaultBoundedElasticSize` and `reactor.schedulers.defaultBoundedElasticQueueSize` via JVM arguments according to database connection pool limits (HikariCP).
6. **Beware of Virtual Thread Pinning**: If running Java 21 virtual threads, avoid `synchronized` blocks inside I/O calls to prevent pinning carrier threads; use `ReentrantLock` instead.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Installing BlockHound After Class Loading** | BlockHound cannot re-instrument classes that have already been loaded into the JVM. | Install BlockHound in static initialization blocks or use the `-javaagent` JVM command line flag. |
| **Blocking Inside `publishOn` Operators** | Misunderstanding operator scope: putting blocking logic before a `publishOn(boundedElastic())` still blocks the event loop. | Place `subscribeOn` or put `publishOn` *before* the operator containing blocking logic. |
| **Silent Swallowing of BlockingOperationError** | Wrapping reactive pipelines in broad `catch (Exception e)` suppresses BlockHound warnings. | Catch specific exceptions only; allow `Error` and `BlockingOperationError` to bubble up. |
| **Unbounded Thread Creation with Custom Executors** | Creating unmanaged thread pools for blocking operations exhausts OS threads and causes native memory crashes. | Use `Schedulers.boundedElastic()` or `Schedulers.fromExecutorService(virtualThreadExecutor)`. |
| **Using `ThreadLocal` in Reactive Pipelines** | In WebFlux, consecutive operators execute on different threads, causing `ThreadLocal` values to be lost or leaked. | Use Reactor's `contextView` and `contextWrite` API for reactive context propagation. |
| **Ignoring DNS Resolution Blocking** | Standard `InetAddress.getByName()` calls synchronously block socket threads during external API lookups. | Use Netty's asynchronous domain name resolver (`DnsAddressResolverGroup`). |

---

## 8. Performance & Security Notes

### Performance
- **Zero BlockHound Overhead in Production**: BlockHound is designed exclusively as a test-time dependency. Do not include it on production classpaths.
- **Event Loop Sizing Invariant**: Netty event loops should remain strictly CPU-bound. Offloading all blocking I/O to `boundedElastic()` preserves $99.9\text{th}$ percentile response latencies under heavy load.

### Security
- **Thread Starvation DoS Prevention**: Malicious requests designed to trigger expensive slow file reads or cryptographic hashing will freeze event loops without thread offloading. Strict thread separation mitigates denial-of-service vulnerabilities.
- **Context Leak Mitigation**: Failing to clear thread context when crossing thread pools can leak security credentials across tenant boundaries.

---

## 9. Comparison / When to Use What

| Architecture / Mechanism | WebFlux Event Loop | Schedulers.boundedElastic() | Virtual Threads (Java 21+) |
| :--- | :--- | :--- | :--- |
| **Thread Type** | Platform Thread (Non-blocking) | Platform Thread (Worker pool) | Lightweight Virtual Thread |
| **Target Workload** | Non-blocking socket I/O, routing | Blocking legacy I/O (JDBC, Disk) | Synchronous blocking code |
| **Thread Count** | Fixed ($2 \times \text{cores}$) | Dynamic capped ($10 \times \text{cores}$) | Millions (Memory bounded) |
| **Blocking Allowed?** | **STRICTLY FORBIDDEN** | Yes (Dedicated isolation) | Yes (Unmounts from carrier) |
| **Context Propagation** | Reactor Context | Reactor Context | InheritableThreadLocal / ScopedValues |

---

## 10. Interview Questions

<details>
<summary><strong>1. How does BlockHound detect blocking calls on reactive threads?</strong></summary>

BlockHound uses ByteBuddy to instrument known blocking JDK methods (`Thread.sleep`, socket reads, synchronized monitors). At runtime, when an instrumented method is invoked, BlockHound inspects whether `Thread.currentThread()` implements the `io.projectreactor.NonBlocking` marker interface. If true, and the call is not explicitly whitelisted, it throws `BlockingOperationError`.
</details>

<details>
<summary><strong>2. What is the difference between <code>publishOn()</code> and <code>subscribeOn()</code> in Reactor?</strong></summary>

`subscribeOn(Scheduler)` dictates the execution context for subscription and all upstream operations up to the next `publishOn()`. `publishOn(Scheduler)` switches the execution context for all *downstream* operators following it in the pipeline chain.
</details>

<details>
<summary><strong>3. Why was <code>Schedulers.elastic()</code> deprecated in favor of <code>boundedElastic()</code>?</strong></summary>

`Schedulers.elastic()` spawned an unbounded number of platform threads on demand. Under heavy backpressure or sudden spikes in blocking I/O, it would spawn thousands of threads, consuming all native system memory and triggering `java.lang.OutOfMemoryError: unable to create new native thread`. `boundedElastic()` enforces strict upper bounds on thread count and task queues.
</details>

<details>
<summary><strong>4. How should legacy JDBC operations be handled in a Spring WebFlux application?</strong></summary>

JDBC operations should be wrapped inside `Mono.fromCallable()` or `Mono.fromRunnable()` and assigned to `Schedulers.boundedElastic()` using `.subscribeOn()`. Alternatively, developers should migrate to non-blocking R2DBC drivers.
</details>

<details>
<summary><strong>5. Can Java 21 Virtual Threads replace Spring WebFlux entirely?</strong></summary>

Virtual threads excel at imperative, synchronous request-per-thread models (Spring MVC), dramatically reducing memory overhead. However, WebFlux still provides superior capabilities for event streaming (SSE, WebSockets), reactive backpressure flow control across distributed systems, and functional composition of complex asynchronous pipelines.
</details>

<details>
<summary><strong>6. What happens if a developer calls <code>Thread.sleep(1000)</code> inside a WebFlux controller method?</strong></summary>

The method executes on a Netty worker thread (`reactor-http-nio-*`). Because there are only $2 \times \text{CPU cores}$ Netty threads, blocking one thread immediately reduces the server's global request handling capacity by $1 / (2 \times \text{cores})$. A few concurrent requests doing this will freeze all incoming connections to the entire server.
</details>

<details>
<summary><strong>7. How do you whitelist a legitimate blocking operation in BlockHound?</strong></summary>

Create an implementation of `BlockHoundIntegration`, implement `applyTo(BlockHound.Builder builder)`, and call `builder.allowBlockingCallsInside("fully.qualified.ClassName", "methodName")`. Register it using Java's `ServiceLoader` via `META-INF/services/reactor.blockhound.integration.BlockHoundIntegration`.
</details>

<details>
<summary><strong>8. Why doesn't <code>ThreadLocal</code> work reliably across reactive pipelines?</strong></summary>

Reactive pipelines frequently switch execution threads across operators (e.g., crossing from a Netty I/O thread to a `boundedElastic` worker thread). Because `ThreadLocal` variables are bound strictly to the physical thread instance, data stored in a `ThreadLocal` on one operator becomes inaccessible or references stale data on downstream operators.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Basic BlockHound Detection Test</strong></summary>

**Task**: Write a unit test that installs BlockHound, executes `Thread.yield()` or `Thread.sleep()` on `Schedulers.parallel()`, and asserts that a `BlockingOperationError` is thrown.

```java
@Test
void testBlockHoundCatchesSleep() {
    BlockHound.install();

    assertThatThrownBy(() -> {
        Mono.delay(Duration.ofMillis(1))
            .publishOn(Schedulers.parallel())
            .doOnNext(tick -> {
                try {
                    Thread.sleep(10);
                } catch (InterruptedException ignored) {}
            })
            .block(Duration.ofSeconds(1));
    }).hasCauseInstanceOf(BlockingOperationError.class);
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Safe Blocking Offloading</strong></summary>

**Task**: Fix the blocking code from Exercise 1 using `Schedulers.boundedElastic()` so that it passes without triggering BlockHound.

```java
@Test
void testOffloadedSleepPasses() {
    BlockHound.install();

    StepVerifier.create(
        Mono.fromCallable(() -> {
            Thread.sleep(10);
            return "SUCCESS";
        }).subscribeOn(Schedulers.boundedElastic())
    )
    .expectNext("SUCCESS")
    .verifyComplete();
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Programmatic BlockHound Whitelist Builder</strong></summary>

**Task**: Configure BlockHound programmatically using `BlockHound.builder()` to allow `java.io.RandomAccessFile.read` inside a specific utility class `SafeDiskReader`.

```java
@Test
void testWhitelistCustomBlockingMethod() {
    BlockHound.builder()
        .allowBlockingCallsInside(SafeDiskReader.class.getName(), "readInternal")
        .install();

    // Invoking SafeDiskReader.readInternal on Schedulers.parallel() will now succeed
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Refactoring Blocking RestTemplate to WebClient</strong></summary>

**Task**: Refactor a synchronous HTTP client method that fetches user details to use non-blocking `WebClient`.

```java
// BEFORE (Blocking)
public User fetchUserBlocking(String id) {
    return restTemplate.getForObject("/users/" + id, User.class);
}

// AFTER (Non-Blocking)
public Mono<User> fetchUserReactive(String id) {
    return webClient.get()
        .uri("/users/{id}", id)
        .retrieve()
        .bodyToMono(User.class);
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Custom ServiceLoader BlockHoundIntegration Module</strong></summary>

**Task**: Implement a production-ready `BlockHoundIntegration` class that permits Jackson reflection metadata access during bootstrap and document the exact `META-INF` file requirement.

```java
package com.example.reactive.test;

import com.google.auto.service.AutoService;
import reactor.blockhound.BlockHound;
import reactor.blockhound.integration.BlockHoundIntegration;

@AutoService(BlockHoundIntegration.class)
public class JacksonBlockHoundIntegration implements BlockHoundIntegration {
    @Override
    public void applyTo(BlockHound.Builder builder) {
        builder.allowBlockingCallsInside("com.fasterxml.jackson.databind.ObjectMapper", "createUnresolvedForwardReference");
        builder.allowBlockingCallsInside("com.fasterxml.jackson.databind.deser.BeanDeserializerFactory", "buildBeanDeserializer");
    }
}
// META-INF/services/reactor.blockhound.integration.BlockHoundIntegration
// Contains: com.example.reactive.test.JacksonBlockHoundIntegration
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Virtual Threads Custom Reactor Scheduler Benchmark Test</strong></summary>

**Task**: Create a custom Reactor `Scheduler` powered by Java 21 `Executors.newVirtualThreadPerTaskExecutor()`, execute 10,000 simulated 10ms blocking I/O calls concurrently, and verify all complete within 2 seconds without exhausting OS threads.

```java
@Test
void testVirtualThreadSchedulerScalability() {
    Scheduler virtualScheduler = Schedulers.fromExecutorService(
        Executors.newVirtualThreadPerTaskExecutor()
    );

    Flux<Integer> pipeline = Flux.range(1, 10_000)
        .flatMap(i -> 
            Mono.fromCallable(() -> {
                Thread.sleep(10); // Simulated blocking call
                return i;
            }).subscribeOn(virtualScheduler)
        );

    StepVerifier.create(pipeline.count())
        .expectNext(10_000L)
        .verifyComplete();
}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------------+
| BlockHound:   Bytecode agent detecting blocking calls on NonBlocking threads          |
| Install:      BlockHound.install() (Run in @BeforeAll or Maven test runner)           |
| Offloading:   .subscribeOn(Schedulers.boundedElastic())                               |
| Pipeline:     publishOn() = downstream effect; subscribeOn() = upstream effect        |
| Whitelist:    builder.allowBlockingCallsInside(ClassName, MethodName)                 |
| Rule:         NEVER block Netty event loops ("reactor-http-nio-*")                    |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Project Reactor BlockHound GitHub Repository](https://github.com/reactor/BlockHound)
- [Project Reactor Schedulers Documentation](https://projectreactor.io/docs/core/release/reference/#schedulers)
- [Netty Threading Model and Architecture](https://netty.io/wiki/thread-model.html)
- [Virtual Threads in Spring Boot 3.x (Spring Blog)](https://spring.io/blog/2023/09/20/hello-virtual-threads)
