# Reactive Streams & Backpressure Strategies in Project Reactor

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | Reactive Programming Basics, Java Concurrency, TCP Flow Control |
| **Estimated Reading Time** | 25 minutes |
| **Status** | Production-Ready |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Baseline** | OpenJDK 21 LTS / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | Project Reactor 3.7, OpenJDK 21.0.2 |

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
9. [Comparison: Backpressure Overflow Strategies](#9-comparison-backpressure-overflow-strategies)
10. [Interview Questions & Deep Dives](#10-interview-questions--deep-dives)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference](#12-summary--quick-reference)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

In distributed computing, producers and consumers rarely run at the exact same velocity. A database query or high-speed Kafka topic can produce 100,000 events per second, while a downstream consumer writing to disk, calling an external payment API, or rendering a mobile client UI may only process 500 events per second.

### The Problem: Fast Producer vs Slow Consumer
Without flow control:
1. **Unbounded Buffering**: The consumer buffers unprocessed events in memory until the JVM runs out of heap space (`OutOfMemoryError`).
2. **Buffer Bloat & Latency**: Large in-memory queues cause severe latency spikes, stale data processing, and unpredictable GC pauses.
3. **Data Loss**: Dropping messages arbitrarily without informing the producer corrupts data pipelines.

### The Solution: Reactive Streams Backpressure
**Backpressure** is a mechanism where a consumer signals its current capacity back up to the producer. Instead of the producer pushing data blindly, the consumer controls the flow by pulling demand: *"Send me 10 items; when I'm ready, I'll ask for 10 more."* In Project Reactor, backpressure is baked into the `Subscription.request(n)` contract, providing programmatic flow control strategies (`onBackpressureBuffer`, `onBackpressureDrop`, `onBackpressureLatest`, `limitRate`).

---

## 2. Core Concepts

### Mental Model: The Pull-Push Hybrid Protocol
```
   Subscriber (Consumer)                                Publisher (Producer)
            │                                                     │
            │ 1. subscribe(Subscriber)                            │
            │────────────────────────────────────────────────────►│
            │                                                     │
            │ 2. onSubscribe(Subscription)                        │
            │◄────────────────────────────────────────────────────│
            │                                                     │
            │ 3. request(2)  [DEMAND SIGNAL: Pull 2 items]        │
            │────────────────────────────────────────────────────►│
            │                                                     │ Emits only 2 items!
            │ 4. onNext(Item #1)                                  │
            │◄────────────────────────────────────────────────────│
            │ 5. onNext(Item #2)                                  │
            │◄────────────────────────────────────────────────────│
            │                                                     │ Producer PAUSES!
            │ (Consumer processes Item #1 and #2...)              │ (No more emissions allowed)
            │                                                     │
            │ 6. request(1)  [DEMAND SIGNAL: Pull 1 item]         │
            │────────────────────────────────────────────────────►│
            │ 7. onNext(Item #3)                                  │
            │◄────────────────────────────────────────────────────│
```

### Invariants
1. **The Demand Rule**: A `Publisher` MUST NOT emit more items than the cumulative demand signaled via `Subscription.request(n)`.
2. **Long.MAX_VALUE Signifies Unbounded Demand**: Calling `request(Long.MAX_VALUE)` switches the stream into pure push mode, effectively disabling backpressure.
3. **Terminal Signaling**: When an error or completion occurs, no further items can be requested or emitted.

---

## 3. How It Works Internally

### The `Subscription.request(long n)` Engine
Under the hood in Project Reactor:
1. When a subscriber subscribes, Reactor's `Operators` wrap the downstream subscriber in a specialized operator subscriber (e.g. `FluxMapSubscriber`).
2. Demand requests bubble up the operator chain via `s.request(n)` until they reach the source publisher (e.g. `FluxRange` or an R2DBC driver).
3. The source maintains an atomic integer/long demand counter (`requested`).
4. Each `onNext(item)` decrements the counter. When `requested == 0`, the source pauses emissions and waits for the next `request(n)` call.

---

## 4. Syntax & Configuration

### Key Backpressure Operators in Project Reactor

| Operator | Semantics |
| :--- | :--- |
| `limitRate(n)` | Batches upstream demand: requests $n$ items, re-requests when 75% consumed |
| `onBackpressureBuffer(max)` | Buffers up to `max` items in memory; throws `Exceptions.failWithOverflow` if exceeded |
| `onBackpressureDrop(consumer)` | Drops items emitted by upstream when downstream has no demand |
| `onBackpressureLatest()` | Keeps only the single latest element, discarding older unconsumed elements |
| `onBackpressureError()` | Emits an `Exceptions.failWithOverflow` immediately if demand is zero |

---

## 5. Practical Examples

### Example 1: Bounded Rate Limiting via `limitRate()`

`limitRate(n)` prevents downstream consumers from accidentally requesting `Long.MAX_VALUE` (unbounded push mode).

```java
package com.example.reactive.backpressure;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import reactor.core.publisher.Flux;
import reactor.core.scheduler.Schedulers;

public class LimitRateExample {

    private static final Logger log = LoggerFactory.getLogger(LimitRateExample.class);

    public static void main(String[] args) throws InterruptedException {
        Flux.range(1, 1000)
                .log() // Logs request(n) demand signals!
                // Batches demand: requests 10 items initially, re-requests 7 items once 75% are consumed
                .limitRate(10)
                .publishOn(Schedulers.boundedElastic())
                .subscribe(item -> {
                    try {
                        Thread.sleep(50); // Simulate slow processing
                        log.info("Processed item: {}", item);
                    } catch (InterruptedException ignored) {}
                });

        Thread.sleep(10000);
    }
}
```

### Example 2: Bounded Buffer with Eviction Strategy (`onBackpressureBuffer`)

```java
package com.example.reactive.backpressure;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import reactor.core.publisher.BufferOverflowStrategy;
import reactor.core.publisher.Flux;
import reactor.core.publisher.Mono;

import java.time.Duration;

public class BoundedBufferStrategyExample {

    private static final Logger log = LoggerFactory.getLogger(BoundedBufferStrategyExample.class);

    public static void main(String[] args) throws InterruptedException {
        // Fast producer emitting an item every 2ms
        Flux<Long> sensorFeed = Flux.interval(Duration.ofMillis(2))
                .onBackpressureBuffer(
                        20, // Buffer capacity: hold max 20 items in heap
                        droppedItem -> log.warn("Buffer full! Dropped oldest item: {}", droppedItem),
                        BufferOverflowStrategy.DROP_OLDEST // Evict oldest to make room for newest
                );

        // Slow consumer taking 50ms per item
        sensorFeed
                .concatMap(val -> Mono.delay(Duration.ofMillis(50)).thenReturn(val))
                .subscribe(
                        item -> log.info("Successfully ingested sensor tick: {}", item),
                        err -> log.error("Stream crashed with error", err)
                );

        Thread.sleep(3000);
    }
}
```

### Example 3: Edge Case — Handling Live Streams with `onBackpressureLatest()`

In real-time trading dashboards or IoT telemetry, stale intermediate values are worthless; the user only cares about the **most recent** price or temperature reading.

```java
package com.example.reactive.backpressure;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import reactor.core.publisher.Flux;
import reactor.core.publisher.Mono;

import java.time.Duration;

public class LatestValueStrategyExample {

    private static final Logger log = LoggerFactory.getLogger(LatestValueStrategyExample.class);

    public static void main(String[] args) throws InterruptedException {
        // Live stock ticker pushing 100 updates/sec
        Flux<String> liveMarketFeed = Flux.interval(Duration.ofMillis(10))
                .map(seq -> "AAPL Stock Tick #" + seq)
                // Retains only the absolute latest emitted tick if consumer is busy!
                .onBackpressureLatest();

        // Slow rendering client (takes 100ms per render)
        liveMarketFeed
                .concatMap(tick -> Mono.delay(Duration.ofMillis(100)).thenReturn(tick))
                .subscribe(tick -> log.info("Rendered to UI: {}", tick));

        Thread.sleep(2000);
    }
}
```

### Example 4: Anti-Pattern vs Production Fix

#### Anti-Pattern: Unbounded In-Memory Buffer
```java
// ANTI-PATTERN: Default buffer() without bounds on fast source
fastEventSource
    .buffer() // Buffers ALL items indefinitely into a List<T> in memory!
    // If producer runs faster than consumer, triggers OutOfMemoryError!
    .subscribe(batch -> slowDatabase.saveAll(batch));
```

#### Production Fix: Time/Size Bounded Buffering
```java
// PRODUCTION FIX: Window or buffer with both time and size limits
fastEventSource
    .bufferTimeout(500, Duration.ofMillis(200)) // Max 500 items OR flush every 200ms
    .concatMap(batch -> slowDatabase.saveAll(batch))
    .subscribe();
```

### Example 5: Custom `BaseSubscriber` with Fine-Grained Pull Demand

```java
package com.example.reactive.backpressure;

import org.reactivestreams.Subscription;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import reactor.core.publisher.BaseSubscriber;
import reactor.core.publisher.Flux;

public class CustomPullSubscriberDemo {

    private static final Logger log = LoggerFactory.getLogger(CustomPullSubscriberDemo.class);

    public static void main(String[] args) {
        Flux<Integer> numbers = Flux.range(1, 100);

        numbers.subscribe(new BaseSubscriber<Integer>() {
            private int consumedInBatch = 0;
            private static final int BATCH_SIZE = 5;

            @Override
            protected void hookOnSubscribe(Subscription subscription) {
                log.info("Subscribed! Requesting initial batch of {}", BATCH_SIZE);
                request(BATCH_SIZE); // Initial pull demand
            }

            @Override
            protected void hookOnNext(Integer value) {
                log.info("Processing value: {}", value);
                consumedInBatch++;

                if (consumedInBatch == BATCH_SIZE) {
                    log.info("Batch finished! Requesting next {} items...", BATCH_SIZE);
                    consumedInBatch = 0;
                    request(BATCH_SIZE); // Request next batch only when current is done!
                }
            }

            @Override
            protected void hookOnComplete() {
                log.info("All items processed cleanly!");
            }
        });
    }
}
```

---

## 6. Best Practices

1. **Use `limitRate()` to Protect Downstream Pipelines**: Place `.limitRate(100)` before computationally expensive or I/O-heavy operators to batch demand and avoid memory bursts.
2. **Never Buffer Unbounded Streams**: Never use unbounded `buffer()` or `onBackpressureBuffer()` without a fixed maximum capacity limit.
3. **Use `onBackpressureLatest()` for Realtime Telemetry**: For metrics, GPS tracking, and financial quotes, discard intermediate stale values in favor of the latest measurement.
4. **Log Demand Signals with `.log()`**: When debugging backpressure, place `.log()` in the pipeline to inspect `request(n)`, `onNext()`, and `cancel()` signals in log output.
5. **Pair Fast Streams with `bufferTimeout()`**: Group high-throughput events into micro-batches (`bufferTimeout(100, Duration.ofMillis(50))`) for bulk database insertion.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **`Exceptions.failWithOverflow` / OverflowException** | Downstream cannot keep up and `onBackpressureBuffer` ran out of capacity. | Increase capacity, add a drop strategy, or optimize consumer processing. |
| **Accidental Unbounded Demand (`Long.MAX_VALUE`)** | Using standard `subscribe(consumer)` without intermediate operators pulls everything at once. | Use `.limitRate(n)` or a custom `BaseSubscriber`. |
| **Blocking Inside `concatMap`** | Freezes the pipeline; demand signals stop bubbling upstream. | Use non-blocking asynchronous mapping (`flatMap` or `concatMap` with publishers). |
| **Silent Data Loss with `onBackpressureDrop()`** | Items are discarded silently without notification or auditing. | Pass a callback: `onBackpressureDrop(item -> auditDrop(item))`. |
| **Mismatched Buffer Sizes** | Setting buffer size smaller than producer burst size causes constant drop cascades. | Tune buffer capacity to accommodate typical burst peaks. |
| **Ignoring Network TCP Backpressure** | Assuming backpressure only applies in memory; network sockets also enforce flow control. | Allow WebFlux to propagate socket backpressure back to database cursors. |

---

## 8. Performance & Security Considerations

### Memory Allocation Under Buffer Bloat
Retaining thousands of unconsumed objects in an `onBackpressureBuffer` forces them into Old Generation heap space, increasing garbage collection pause times. Set strict thresholds and prefer dropping or sampling strategies under extreme overload.

### Slowloris DoS Protection via Backpressure
Malicious clients reading HTTP responses extremely slowly (1 byte/sec) trigger backpressure upstream. WebFlux pauses reading from the database, preventing server memory exhaustion. Enforce idle timeouts to terminate deliberately slow client connections.

---

## 9. Comparison: Backpressure Overflow Strategies

| Strategy | Behavior on Buffer Full | Data Loss? | Latency Impact | Best Used For |
| :--- | :--- | :--- | :--- | :--- |
| **`onBackpressureBuffer(max)`** | Holds up to `max` items; fails if full | No (until fail) | Increases | Mission-critical financial events |
| **`DROP_OLDEST`** | Discards oldest item to make room for newest | Yes (Oldest) | Bounded | Sliding window metrics, logs |
| **`DROP_LATEST`** | Discards newest arriving item | Yes (Newest) | Bounded | Non-critical event tracking |
| **`onBackpressureLatest()`** | Keeps only 1 latest item; discards rest | Yes (Intermediate)| Minimal | Stock tickers, IoT sensor updates |
| **`onBackpressureDrop()`** | Drops anything emitted when demand is 0 | Yes | Minimal | High-volume clickstream data |

---

## 10. Interview Questions & Deep Dives

<details>
<summary><strong>Q1: What is Backpressure in the context of the Reactive Streams specification?</strong></summary>

**Answer:**
Backpressure is a flow-control feedback mechanism between a consumer (`Subscriber`) and a producer (`Publisher`). Under the Reactive Streams specification, a publisher is forbidden from pushing items until the subscriber explicitly signals capacity via `Subscription.request(long n)`. This ensures that a fast producer cannot overwhelm a slow consumer with more data than its memory or processing capacity can handle.
</details>

<details>
<summary><strong>Q2: What happens if a subscriber calls `subscription.request(Long.MAX_VALUE)`?</strong></summary>

**Answer:**
Calling `request(Long.MAX_VALUE)` signals effectively infinite demand. The publisher switches from pull-push mode into pure push mode, emitting items as fast as it can generate them without waiting for further demand signals. If intermediate operators do not buffer or rate-limit the stream, backpressure protection is disabled.
</details>

<details>
<summary><strong>Q3: How does the `limitRate(int prefetch)` operator work?</strong></summary>

**Answer:**
`limitRate(n)` acts as a demand regulator. It intercepts downstream demand and requests exactly `n` elements from upstream. It tracks consumption: once approximately 75% of the prefetch batch (or a configurable low-tide percentage) has been emitted downstream, it automatically issues another `request(0.75 * n)` upstream. This keeps a steady stream flowing without memory spikes.
</details>

<details>
<summary><strong>Q4: What is the difference between `onBackpressureDrop()` and `onBackpressureLatest()`?</strong></summary>

**Answer:**
- `onBackpressureDrop()`: Discards any incoming item emitted by upstream when the downstream consumer has requested 0 items.
- `onBackpressureLatest()`: When downstream demand is 0, it holds on to the **single most recent item** emitted. If a newer item arrives before demand is signaled, it replaces the old one. Once the consumer issues `request(1)`, it immediately receives the latest value.
</details>

<details>
<summary><strong>Q5: What exception is thrown when an `onBackpressureBuffer` threshold is breached without a drop strategy?</strong></summary>

**Answer:**
`reactor.core.Exceptions.OverflowException` (or `io.projectreactor: The receiver is unable to keep up, buffer full`). This terminates the reactive stream with an `onError` signal.
</details>

<details>
<summary><strong>Q6: How does Backpressure propagate across network boundaries in Spring WebFlux?</strong></summary>

**Answer:**
WebFlux uses Reactor Netty, which links Reactive Streams demand to TCP socket flow control. If the remote HTTP client stops reading, the client TCP window shrinks to 0. Netty stops reading from the application buffer. WebFlux ceases issuing `Subscription.request(n)` demand to the upstream publisher (e.g. R2DBC database driver), which pauses reading rows from the database socket cursor.
</details>

<details>
<summary><strong>Q7: Can a `Publisher` choose to ignore `Subscription.request(n)`?</strong></summary>

**Answer:**
No. According to Rule 1.1 of the Reactive Streams specification, a publisher MUST NOT emit more items than the cumulative requested demand. Violating this rule violates the specification and causes downstream operators to throw `IllegalStateException`.
</details>

<details>
<summary><strong>Q8: What is `BaseSubscriber` and when should you extend it?</strong></summary>

**Answer:**
`BaseSubscriber<T>` is Project Reactor's recommended base class for custom subscribers. It provides lifecycle hooks (`hookOnSubscribe`, `hookOnNext`, `hookOnError`, `hookOnComplete`) and utility methods like `request(n)` and `cancel()`, making it easy to implement manual, fine-grained demand management.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Regulate Stream with `limitRate`</strong></summary>

**Task**: Apply `limitRate(50)` to a range flux from 1 to 500.

```java
// Solution:
Flux<Integer> regulated = Flux.range(1, 500).limitRate(50);
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Drop Excess Items with Audit Logging</strong></summary>

**Task**: Write a pipeline that drops excess elements from a fast interval flux and prints a warning message for each dropped item.

```java
// Solution:
Flux<Long> droppedFlux = Flux.interval(Duration.ofMillis(1))
        .onBackpressureDrop(item -> System.out.println("Dropped: " + item));
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): StepVerifier Request Assertions</strong></summary>

**Task**: Write a test using `StepVerifier.create(flux, 2)` verifying that the pipeline emits 2 items, pauses, and then emits 2 more when requested via `thenRequest(2)`.

```java
// Solution:
@Test
void testStepVerifierBackpressure() {
    Flux<Integer> flux = Flux.range(1, 10);
    StepVerifier.create(flux, 2)
            .expectNext(1, 2)
            .thenRequest(2)
            .expectNext(3, 4)
            .thenCancel()
            .verify();
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Micro-Batching with `bufferTimeout`</strong></summary>

**Task**: Group incoming elements into batches of maximum 20 items or flush every 100ms, whichever happens first.

```java
// Solution:
Flux<List<String>> batched = eventStream
        .bufferTimeout(20, Duration.ofMillis(100));
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Dynamic Demand-Throttled Image Processor</strong></summary>

**Task**: Build a `BaseSubscriber` that queries system CPU utilization; if CPU load < 70%, it requests 10 items; if CPU load >= 70%, it requests only 1 item at a time, implementing adaptive backpressure.
</details>

<details>
<summary><strong>Exercise 6 (Hard): Custom Resilient Overflow Buffer Publisher</strong></summary>

**Task**: Implement a custom `Publisher` operator that spools overflowing items to an external temporary disk file when the in-memory buffer reaches capacity, draining disk items back into the stream when downstream demand resumes.
</details>

---

## 12. Summary & Quick Reference

- **Reactive Rule**: Fast producers must not overwhelm slow consumers.
- **Demand Flow**: Controlled via `Subscription.request(n)`.
- **`limitRate(n)`**: Batches demand and regulates prefetch chunks.
- **Drop Strategies**: `onBackpressureBuffer`, `onBackpressureDrop`, `onBackpressureLatest`.
- **End-to-End**: In WebFlux, backpressure flows from client TCP sockets all the way to database cursors.

---

## 13. Further Reading

- [Reactive Streams Specification](https://github.com/reactive-streams/reactive-streams-jvm/blob/v1.0.4/README.md)
- [Project Reactor Core: Backpressure and Reshaping Requests](https://projectreactor.io/docs/core/release/reference/#producing)
- [Understanding Reactive Backpressure in Spring WebFlux](https://spring.io/blog/2016/07/20/notes-on-reactive-programming-part-ii-writing-some-code)
- [Spring WebFlux Flow Control Architecture](https://docs.spring.io/spring-framework/reference/web/webflux.html)
