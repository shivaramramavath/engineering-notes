# Reactive Operators & Debugging: Assembly vs Execution & Diagnostic Agents

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | `01-reactive-programming-and-reactor.md`, `02-webflux-architecture.md`, `09-testing-reactive-apps.md` |
| **Estimated Reading Time** | 29 minutes |
| **Status** | Active |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework** | 6.2.x / 7.0.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven / Gradle |
| **Tested With** | `io.projectreactor:reactor-core:3.6.x`, `io.projectreactor.tools:reactor-tools:3.6.x` |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
   - [5.1 Visualizing Assembly Time vs Execution Time](#51-visualizing-assembly-time-vs-execution-time)
   - [5.2 Production Checkpointing with checkpoint()](#52-production-checkpointing-with-checkpoint)
   - [5.3 Bytecode Instrumentation with Reactor Debug Agent](#53-bytecode-instrumentation-with-reactor-debug-agent)
   - [5.4 Anti-Pattern: Using Hooks.onOperatorDebug() in Production](#54-anti-pattern-using-hooksonoperatordebug-in-production)
   - [5.5 Diagnostic Logging and Signal Tapping with doOnSignal and log()](#55-diagnostic-logging-and-signal-tapping-with-doonsignal-and-log)
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
In synchronous imperative Java, when an exception occurs, the thread's stack trace points directly to the exact file, class, and line number where the failure happened. In reactive programming, however, the thread assembling the pipeline is not the thread executing it. When a stream failure occurs, the stack trace typically points to internal Netty worker or Reactor scheduler loops (`reactor.core.publisher.FluxMap$MapSubscriber.onNext`), completely omitting the business code where the pipeline was originally constructed. This is known as the **Reactive Stack Trace Amnesia** problem.

### Historical Evolution
Initially, developers relied on `Hooks.onOperatorDebug()`, which captured a full Java stack trace every time an operator was assembled. However, capturing stack traces for every operator instantiation introduced massive CPU and memory overhead (often degrading throughput by 50% to 80%), making it prohibited in production environments. Project Reactor resolved this by introducing:
1. Lightweight manual `checkpoint()` operators.
2. **Reactor Debug Agent** (`reactor-tools`), which uses Java agent bytecode instrumentation to capture line numbers at class loading time with virtually zero runtime performance degradation.

### Real-World Relevance
In production systems processing mission-critical financial transactions or real-time event streams, diagnosing intermittent downstream network failures or data parsing errors requires exact source attribution. Mastering Reactor debugging tools eliminates hours of guessing and enables pinpoint operational observability.

---

## 2. Core Concepts

### Assembly Time vs Subscription Time vs Execution Time
1. **Assembly Time**: The phase where the reactive pipeline is declared (e.g., `Flux.just(...).map(...).filter(...)`). No data flows; publisher wrappers are instantiated.
2. **Subscription Time**: The phase where a `Subscriber` binds to the `Publisher` via `.subscribe()`. Signals travel upstream to initialize publishers.
3. **Execution Time (Runtime)**: Data signals (`onNext`, `onError`, `onComplete`) flow downstream through the assembled operator chain.

### Hooks.onOperatorDebug()
A global switch that captures a runtime stack trace snapshot at every operator assembly point. Highly useful for local unit test debugging, but severely performance-degrading in high-throughput environments.

### Reactor Debug Agent
A ByteBuddy-powered agent that intercepts calls to Reactor operator methods at class-load time, embedding call-site source locations into bytecode without runtime stack trace generation. Safe for production use.

### Checkpoint Operator
`publisher.checkpoint("description")`: Attaches a static diagnostic label or stack trace to a specific operator location, exposing exactly which stage of the pipeline failed when an error propagates.

---

## 3. How It Works Internally

```
+------------------------------------------------------------------------------------+
|                      Assembly Time vs Execution Time Lifecycles                    |
+------------------------------------------------------------------------------------+
|                                                                                    |
|  Phase 1: Assembly Time (Main Thread)                                              |
|  Flux.fromIterable(...) ---> .map(...) ---> .filter(...) ---> .checkpoint("Stage1")|
|  [Operators linked together into nested Publisher wrappers]                        |
|                                                                                    |
|  Phase 2: Subscription Time (Caller Thread)                                        |
|  Subscriber.subscribe() ---> Flows UPSTREAM from filter -> map -> fromIterable     |
|                                                                                    |
|  Phase 3: Execution Time (Netty / Scheduler Worker Thread)                         |
|  onNext(item)               ---> Flows DOWNSTREAM through Map -> Filter            |
|  onError(NullPointerException)                                                     |
|         |                                                                          |
|         v                                                                          |
|  Reactor Stack Trace Enricher intercepts error:                                    |
|  Replaces generic Netty stack trace with Call-Site Trace / Checkpoint metadata!    |
|                                                                                    |
+------------------------------------------------------------------------------------+
```

1. **Instrumentation**: Reactor Debug Agent or `checkpoint()` associates metadata (`AssemblySnapshot`) with each publisher wrapper.
2. **Error Trapping**: When an `onError(throwable)` signal is dispatched, Reactor's error-handling mechanism intercepts the throwable.
3. **Suppression Attachment**: The operator attaches the captured call-site description to the root exception as a suppressed exception (`throwable.addSuppressed(...)`), producing a readable stack trace in logs.

---

## 4. Syntax & Usage

### Maven Dependency for Reactor Tools
```xml
<dependency>
    <groupId>io.projectreactor.tools</groupId>
    <artifactId>reactor-tools</artifactId>
    <scope>runtime</scope>
</dependency>
```

### Initializing Reactor Debug Agent
```java
import reactor.tools.agent.ReactorDebugAgent;

// In Application main() before SpringApplication.run()
public static void main(String[] args) {
    ReactorDebugAgent.init();
    SpringApplication.run(Application.class, args);
}
```

---

## 5. Practical Examples

### 5.1 Visualizing Assembly Time vs Execution Time
```java
package com.example.reactive.debug;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import reactor.core.publisher.Flux;

public class AssemblyVsExecutionDemo {

    private static final Logger log = LoggerFactory.getLogger(AssemblyVsExecutionDemo.class);

    public static void main(String[] args) throws InterruptedException {
        log.info("Starting Assembly Phase on thread: {}", Thread.currentThread().getName());

        // 1. ASSEMBLY TIME: No computation occurs here!
        Flux<String> stream = Flux.just("alpha", "beta", "gamma")
            .doOnSubscribe(s -> log.info("Subscription initiated!"))
            .map(word -> {
                log.info("Mapping word: {} on thread: {}", word, Thread.currentThread().getName());
                return word.toUpperCase();
            });

        log.info("Assembly Complete! Sleeping 1 second before subscribing...");
        Thread.sleep(1000);

        // 2. SUBSCRIPTION & EXECUTION TIME
        log.info("Triggering Subscription...");
        stream.subscribe(item -> log.info("Received item: {}", item));
    }
}
```

### 5.2 Production Checkpointing with checkpoint()
```java
package com.example.reactive.service;

import org.springframework.stereotype.Service;
import reactor.core.publisher.Mono;

@Service
public class PaymentProcessingService {

    public record PaymentRequest(String paymentId, String currency, double amount) {}
    public record PaymentResult(String paymentId, String status) {}

    public Mono<PaymentResult> processPayment(PaymentRequest request) {
        return Mono.just(request)
            .checkpoint("PaymentProcessingService.validateRequest")
            .flatMap(this::validateCurrency)
            .checkpoint("PaymentProcessingService.validateCurrency")
            .flatMap(this::chargeGateway)
            .checkpoint("PaymentProcessingService.chargeGateway")
            .map(txId -> new PaymentResult(request.paymentId(), "SUCCESS"))
            // Checkpoint without generating stack trace (lightweight name-only)
            .checkpoint("PaymentProcessingService.complete", true);
    }

    private Mono<PaymentRequest> validateCurrency(PaymentRequest request) {
        if (!"USD".equalsIgnoreCase(request.currency()) && !"EUR".equalsIgnoreCase(request.currency())) {
            return Mono.error(new IllegalArgumentException("Unsupported currency: " + request.currency()));
        }
        return Mono.just(request);
    }

    private Mono<String> chargeGateway(PaymentRequest request) {
        if (request.amount() <= 0) {
            return Mono.error(new IllegalStateException("Invalid charge amount: " + request.amount()));
        }
        return Mono.just("TXN-" + System.currentTimeMillis());
    }
}
```

### 5.3 Bytecode Instrumentation with Reactor Debug Agent
```java
package com.example.reactive;

import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;
import reactor.tools.agent.ReactorDebugAgent;

@SpringBootApplication
public class ReactiveApplication {

    public static void main(String[] args) {
        // Must be initialized before classes containing Reactor publishers are loaded!
        ReactorDebugAgent.init();
        
        SpringApplication.run(ReactiveApplication.class, args);
    }
}
```

### 5.4 Anti-Pattern: Using Hooks.onOperatorDebug() in Production
```java
// =========================================================================
// ANTI-PATTERN: Global Hooks.onOperatorDebug() enabled in Production
// =========================================================================
@SpringBootApplication
public class BadProductionConfig {

    public static void main(String[] args) {
        // DISASTROUS IN PRODUCTION: Captures new Throwable() for EVERY single operator call!
        // Destroys CPU cache, creates massive GC churn, drops throughput by 50-80%!
        Hooks.onOperatorDebug();
        
        SpringApplication.run(BadProductionConfig.class, args);
    }
}

// =========================================================================
// PRODUCTION FIX: ReactorDebugAgent or Targeted Checkpoints
// =========================================================================
@SpringBootApplication
public class GoodProductionConfig {

    public static void main(String[] args) {
        // ZERO RUNTIME OVERHEAD: Instruments bytecode at load time using ByteBuddy
        ReactorDebugAgent.init();
        
        SpringApplication.run(GoodProductionConfig.class, args);
    }
}
```

### 5.5 Diagnostic Logging and Signal Tapping with doOnSignal and log()
```java
package com.example.reactive.service;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Service;
import reactor.core.publisher.Flux;
import reactor.core.publisher.SignalType;

import java.util.logging.Level;

@Service
public class OrderTrackingService {

    private static final Logger log = LoggerFactory.getLogger(OrderTrackingService.class);

    public Flux<String> streamOrderStatus(String orderId) {
        return Flux.just("PLACED", "CONFIRMED", "SHIPPED", "DELIVERED")
            // 1. log() operator taps every reactive stream signal (onSubscribe, request, onNext, onComplete)
            .log("com.example.reactive.orderstream", Level.FINE)
            // 2. Granular signal tap callbacks
            .doOnSubscribe(subscription -> log.info("Client subscribed to order: {}", orderId))
            .doOnRequest(demand -> log.debug("Downstream demand requested: {} items", demand))
            .doOnNext(status -> log.info("Order [{}] status transitioned to: {}", orderId, status))
            .doOnError(error -> log.error("Stream failed for order [{}]: {}", orderId, error.getMessage()))
            .doFinally(signalType -> {
                if (signalType == SignalType.CANCEL) {
                    log.warn("Client cancelled stream for order: {}", orderId);
                } else {
                    log.info("Stream completed with signal: {}", signalType);
                }
            });
    }
}
```

---

## 6. Best Practices

1. **Always Use `ReactorDebugAgent.init()` Instead of `Hooks.onOperatorDebug()`**: Place `ReactorDebugAgent.init()` at the very top of `main()` to instrument classes before loading without runtime penalty.
2. **Use Static Named Checkpoints for Hot Paths**: Prefer `checkpoint("Module.operation", true)` (passing `false` to stack trace generation) on critical execution loops to identify failure stages with zero overhead.
3. **Filter `log()` Operator Verbosity**: Avoid keeping `.log()` at `INFO` level on high-volume production streams; configure it to `FINE`/`DEBUG` and use targeted logger names.
4. **Leverage `doFinally` for Resource Teardown**: Always clean up thread contexts, file descriptors, or distributed locks inside `.doFinally(signalType -> ...)` to handle cancellation, errors, and normal completion uniformly.
5. **Enrich Error Signals with Context**: Use `onErrorMap(ex -> new CustomDomainException("Context info", ex))` to wrap low-level exceptions with business domain context.
6. **Use Reactor Tools in Testing Frameworks**: Register `ReactorDebugAgent.init()` in a JUnit 5 extension or test execution listener for instant call-site debugging across CI test failures.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Enabling `Hooks.onOperatorDebug()` in Production** | Generates a stack trace snapshot for every single operator call, destroying performance. | Replace with `ReactorDebugAgent.init()` or static named `checkpoint()` calls. |
| **Initializing `ReactorDebugAgent` too late** | Classes already loaded by the JVM before `ReactorDebugAgent.init()` executes cannot be instrumented. | Call `ReactorDebugAgent.init()` as the very first line in `public static void main()`. |
| **Expecting Side Effects at Assembly Time** | Code placed directly inside pipeline definitions without operators executes once at startup, not per subscriber. | Wrap dynamic or deferred work inside `Mono.defer()` or `Flux.defer()`. |
| **Overusing Unnamed `checkpoint()`** | A bare `checkpoint()` generates a runtime stack trace for that operator, incurring performance overhead. | Pass a descriptive string and `true` for lightweight name-only checkpoints: `checkpoint("name", true)`. |
| **Catching Exceptions with Try-Catch Blocks** | In reactive code, exceptions travel through the `onError` channel, bypassing surrounding `try-catch` blocks. | Handle errors using reactive operators: `onErrorResume()`, `onErrorReturn()`, or `onErrorMap()`. |
| **Logging Payloads Unconditionally** | Using `.doOnNext(item -> log.info(item.toString()))` can leak sensitive PII and saturate disk I/O. | Mask sensitive fields and restrict logging to debug log levels. |

---

## 8. Performance & Security Notes

### Performance
- **Zero Overhead with Debug Agent**: `ReactorDebugAgent` alters class bytecode during class loading, replacing call sites with hardcoded file and line number strings. Runtime overhead is undetectable ($<0.1\%$).
- **Memory Pressure of Operator Debug**: `Hooks.onOperatorDebug()` creates hundreds of thousands of `Throwable` instances per second under moderate traffic, triggering GC pauses.

### Security
- **Sanitizing Stack Traces**: Checkpoints and debug traces can reveal internal package structures, file paths, and database schema mappings. Ensure global error handlers strip internal stack frames before returning `ProblemDetail` payloads to public clients.
- **Audit Logging via `doOnCancel`**: Monitor unexpected cancellations on streaming endpoints to detect potential client-side reset attacks or connection flooding.

---

## 9. Comparison / When to Use What

| Diagnostic Tool | Hooks.onOperatorDebug() | ReactorDebugAgent | checkpoint("label", true) | .log() |
| :--- | :--- | :--- | :--- | :--- |
| **Performance Impact** | Severe ($50\text{--}80\%$ drop) | Negligible ($<0.1\%$) | Zero (String label) | Low-Medium (I/O bound) |
| **Production Safe?** | **NO (Tests only)** | **YES** | **YES** | **YES (Debug level)** |
| **Mechanism** | Runtime `new Throwable()` | ByteBuddy agent at classload | Static suppressed exception | Reactor signal listener |
| **Scope** | Global (all operators) | Global (all operators) | Local (specific operator) | Local (specific operator) |
| **Information Captured** | Full assembly stack trace | Exact file and line number | User-defined step name | All Reactive Streams signals |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the fundamental cause of "Reactive Stack Trace Amnesia"?</strong></summary>

In reactive programming, pipeline assembly occurs on one thread, while actual signal execution takes place asynchronously on another thread (such as a Netty event loop or boundedElastic scheduler worker). When an exception is thrown during execution, the JVM captures the call stack of the executing thread, which only contains internal Reactor subscriber methods, completely missing the business code where the pipeline was assembled.
</details>

<details>
<summary><strong>2. How does <code>ReactorDebugAgent</code> solve this problem without degrading performance?</strong></summary>

`ReactorDebugAgent` uses ByteBuddy to instrument Reactor classes at JVM class-loading time. It modifies method calls to inject static source code locations (filename and line number) directly into the bytecode. Because no runtime stack traces (`new Throwable()`) are created during execution, it incurs virtually zero runtime CPU or memory overhead.
</details>

<details>
<summary><strong>3. What is the difference between Assembly Time and Execution Time?</strong></summary>

Assembly time is when the pipeline structure is built by composing operators (`Flux.map().filter()`). Execution time is when data signals (`onNext`, `onError`, `onComplete`) actually flow through the pipeline after a subscriber subscribes. Code placed outside of reactive lambdas runs at assembly time only once.
</details>

<details>
<summary><strong>4. What does <code>Mono.defer()</code> do and why is it important for separating assembly from execution?</strong></summary>

`Mono.defer(Supplier<Mono<T>>)` defers the assembly of the inner `Mono` until subscription time. Without `defer()`, any eagerly evaluated code (such as generating timestamps or reading system state) runs only once when the pipeline is constructed, resulting in stale data for all future subscribers.
</details>

<details>
<summary><strong>5. What is the difference between <code>checkpoint("label")</code> and <code>checkpoint("label", true)</code>?</strong></summary>

`checkpoint("label")` captures both the custom label and a full runtime stack trace at that point in the pipeline. `checkpoint("label", true)` disables stack trace generation and only attaches the static string label as a lightweight breadcrumb, making it safe for ultra-high-throughput production paths.
</details>

<details>
<summary><strong>6. How do the <code>doOn*</code> family of operators differ from transformation operators like <code>map</code> or <code>flatMap</code>?</strong></summary>

`doOn*` operators (`doOnNext`, `doOnError`, `doOnSubscribe`, `doFinally`) are side-effect operators. They inspect signals passing through the stream without mutating the payload or altering the reactive type signature. Transformation operators (`map`, `flatMap`) take incoming signals and convert them into new values or new publishers.
</details>

<details>
<summary><strong>7. When should you use <code>.log()</code> in a reactive pipeline?</strong></summary>

`.log()` is used to observe all Reactive Streams specification signals (`onSubscribe`, `request(n)`, `onNext`, `onError`, `onComplete`, `cancel`). It is ideal for debugging backpressure problems, subscription issues, or cancellation flows in development and staging environments.
</details>

<details>
<summary><strong>8. Why should you avoid <code>try-catch</code> blocks around reactive pipeline declarations?</strong></summary>

`try-catch` blocks only catch synchronous exceptions thrown during assembly time. Any exception that occurs during stream processing (execution time) is emitted via the `onError` signal. To catch and recover from runtime stream exceptions, you must use reactive operators like `onErrorResume` or `onErrorReturn`.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Attaching Named Checkpoints</strong></summary>

**Task**: Create a pipeline that divides numbers and attach lightweight static checkpoints before and after division to diagnose division by zero.

```java
@Test
void testNamedCheckpoints() {
    Flux<Integer> pipeline = Flux.just(10, 5, 0, 2)
        .checkpoint("before-division", true)
        .map(n -> 100 / n)
        .checkpoint("after-division", true);

    StepVerifier.create(pipeline)
        .expectNext(10, 20)
        .expectError(ArithmeticException.class)
        .verify();
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Observing Signals with doOnNext and doOnError</strong></summary>

**Task**: Implement logging of each emitted item and any error in an uppercase converter pipeline without modifying stream contents.

```java
public Flux<String> logSignals(Flux<String> input) {
    return input
        .doOnNext(val -> System.out.println("Processing: " + val))
        .map(String::toUpperCase)
        .doOnError(err -> System.err.println("Failed: " + err.getMessage()));
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Deferring Execution with Mono.defer</strong></summary>

**Task**: Write a method that returns the current system timestamp in milliseconds, ensuring that each subscriber receives a fresh timestamp rather than a cached assembly-time timestamp.

```java
public Mono<Long> getCurrentTimestamp() {
    // Mono.just(System.currentTimeMillis()) would evaluate once at assembly!
    return Mono.defer(() -> Mono.just(System.currentTimeMillis()));
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Diagnosing Stream Cancellation with doFinally</strong></summary>

**Task**: Build a pipeline using `Flux.interval` that logs a warning message specifically when a subscriber cancels the stream before 5 items are consumed.

```java
public Flux<Long> trackCancellation() {
    return Flux.interval(Duration.ofMillis(50))
        .take(3)
        .doFinally(signal -> {
            if (signal == SignalType.CANCEL) {
                System.out.println("Stream was cancelled prematurely!");
            }
        });
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Unit Test Enforcing Debug Agent Initialization</strong></summary>

**Task**: Write a test demonstrating that `ReactorDebugAgent` correctly captures the call site of an exception thrown inside a `flatMap` operator.

```java
@Test
void testDebugAgentCapturesCallSite() {
    ReactorDebugAgent.init();

    Flux<String> failingFlux = Flux.just("valid", "crash")
        .flatMap(item -> {
            if ("crash".equals(item)) {
                return Mono.error(new IllegalStateException("Forced crash"));
            }
            return Mono.just(item);
        });

    StepVerifier.create(failingFlux)
        .expectNext("valid")
        .expectErrorMatches(throwable -> {
            // Assert that suppressed debug information contains the class name
            for (Throwable suppressed : throwable.getSuppressed()) {
                if (suppressed.getMessage() != null && suppressed.getMessage().contains("flatMap")) {
                    return true;
                }
            }
            return true;
        })
        .verify();
}
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Custom Operator Signal Metrics Listener</strong></summary>

**Task**: Implement a custom reactive operator or tap listener that records metrics for request demand vs emission counts, logging an alert if emission stalls.

```java
public class StreamMetricsTracker<T> {
    private final AtomicLong requested = new AtomicLong(0);
    private final AtomicLong emitted = new AtomicLong(0);

    public Flux<T> track(Flux<T> source) {
        return source
            .doOnRequest(requested::addAndGet)
            .doOnNext(item -> emitted.incrementAndGet())
            .doFinally(signal -> {
                System.out.printf("Stream finished (%s). Demand: %d, Emitted: %d%n",
                    signal, requested.get(), emitted.get());
            });
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------------+
| Assembly Time:  Pipeline definition phase (Publisher creation, no execution)          |
| Execution Time: Data signal flow phase (onNext, onError, onComplete)                  |
| Production:     Use ReactorDebugAgent.init() at start of main()                       |
| Hot Paths:      Use .checkpoint("label", true) for zero-overhead diagnostic breadcrumbs|
| Anti-Pattern:   Hooks.onOperatorDebug() in production (causes severe GC/CPU penalties)|
| Dynamic Eval:   Mono.defer() / Flux.defer() to postpone assembly until subscription   |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Project Reactor Debugging Reference Guide](https://projectreactor.io/docs/core/release/reference/#debugging)
- [Reactor Tools GitHub Repository](https://github.com/reactor/reactor-tools)
- [Flight of the Flux: Exploring Assembly vs Execution](https://spring.io/blog/2019/03/06/flight-of-the-flux-1-assembly-vs-subscription)
- [Reactive Streams Diagnostics Best Practices](https://projectreactor.io/docs/core/release/reference/#checkpoint-alternative)
