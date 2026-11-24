# Asynchronous Requests & Server-Sent Events (SSE) in Spring MVC

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | Servlet 6.0 Async, Reactive Streams Basics, Spring MVC Controllers |
| **Estimated Reading Time** | 25 minutes |
| **Status** | Production-Ready |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Baseline** | OpenJDK 21 LTS / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | Tomcat 10.1, OpenJDK 21.0.2 |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Configuration](#4-syntax--configuration)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-Common-Mistakes--pitfalls)
8. [Performance & Security Considerations](#8-performance--security-considerations)
9. [Comparison: SSE vs WebSockets vs Long Polling](#9-comparison-sse-vs-websockets-vs-long-polling)
10. [Interview Questions & Deep Dives](#10-interview-questions--deep-dives)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference](#12-summary--quick-reference)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

In traditional synchronous Servlet request processing, each incoming HTTP connection occupies a dedicated container worker thread (`http-nio-8080-exec-*`) for the entire duration of the request.

### The Problem
When dealing with long-running operations (calling slow external third-party payment APIs, processing large reports, or streaming continuous realtime updates):
1. **Thread Starvation**: A spike of 200 concurrent slow requests exhausts the entire Tomcat worker thread pool, causing the server to reject all new incoming connections.
2. **High Memory Overhead**: In traditional thread models, each blocked platform thread retains 1MB of stack memory plus context-switching CPU overhead.
3. **Realtime Bottleneck**: Polling for updates creates unnecessary network chatter and high server load.

### The Solution: Asynchronous Processing & Server-Sent Events
Spring MVC leverages Servlet 3.0+ asynchronous processing constructs (`DeferredResult`, `Callable`, `CompletableFuture`, `ResponseBodyEmitter`, and `SseEmitter`). The container worker thread is immediately returned to the pool while the HTTP socket connection remains open. When background work completes or events occur, the response is written back to the client. Server-Sent Events (SSE) enables unidirectionally streaming realtime events (like AI text generation tokens or stock tickers) over standard HTTP.

---

## 2. Core Concepts

### Mental Model: Synchronous vs Asynchronous Servlet Execution
```
SYNCHRONOUS SERVLET EXECUTION:
Client Request ──► [Tomcat Worker Thread #1 Occupied] ───────────────► Client Response
                   (Waits 5 seconds for slow database / API call...)
                   Thread #1 CANNOT serve other requests during this time!

───────────────────────────────────────────────────────────────────

ASYNCHRONOUS SERVLET EXECUTION (DeferredResult / SSE):
Client Request ──► [Tomcat Worker Thread #1]
                         │
                         ├─► Calls request.startAsync()
                         ├─► Hands off processing to TaskExecutor / Event Broker
                         └─► Worker Thread #1 RELEASED back to Tomcat pool!
                                      │
                         (Background thread processes work...)
                                      │
                                      ▼
                   [Event Dispatched / Result Ready]
                         │
                         ▼
                   [Tomcat Worker Thread #2 Dispatched]
                         │
                         └─► Writes Response & Dispatches Socket Close
```

### Invariants
1. **Thread Release Invariant**: Asynchronous handlers release the original servlet container thread before business logic finishes.
2. **Timeout Handling**: All asynchronous requests must declare explicit timeouts; if the background task hangs, a timeout handler must cleanly release the client socket.
3. **SSE Unidirectionality**: SSE operates over standard HTTP (`text/event-stream`). Communication is strictly server-to-client; client-to-server messaging uses standard HTTP POST endpoints.

---

## 3. How It Works Internally

### `WebAsyncManager` and `DeferredResult` Lifecycle
1. The controller returns a `DeferredResult<T>`.
2. Spring's `DeferredResultMethodReturnValueHandler` invokes `WebAsyncUtils.getAsyncManager(request).startDeferredResultProcessing(...)`.
3. The underlying servlet container starts async processing (`request.startAsync()`), holding the TCP socket open without holding a worker thread.
4. When your background thread calls `deferredResult.setResult(value)`:
   - Spring dispatches an internal async completion event to the servlet container.
   - Tomcat allocates a worker thread from its pool to write the result through `HttpMessageConverter` and commit the HTTP response.

### `SseEmitter` Protocol
`SseEmitter` extends `ResponseBodyEmitter`. It sets the response `Content-Type: text/event-stream;charset=UTF-8` and writes data formatted per the W3C EventSource specification:
```
event: order-status
id: 104
data: {"status": "SHIPPED"}

```

---

## 4. Syntax & Configuration

### Standard `application.yaml` Async Configuration
```yaml
spring:
  mvc:
    async:
      request-timeout: 30000 # 30 seconds default async request timeout
  task:
    execution:
      pool:
        core-size: 8
        max-size: 32
        queue-capacity: 100
      thread-name-prefix: async-mvc-
```

---

## 5. Practical Examples

### Example 1: Non-Blocking Long-Running Operation with `DeferredResult`

```java
package com.example.web.async;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;
import org.springframework.web.context.request.async.DeferredResult;

import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ForkJoinPool;

@RestController
@RequestMapping("/api/v1/async-jobs")
public class DeferredResultJobController {

    private static final Logger log = LoggerFactory.getLogger(DeferredResultJobController.class);

    @GetMapping("/compute")
    public DeferredResult<ResponseEntity<String>> runLongComputation() {
        log.info("Incoming request handled by Tomcat worker thread: {}", Thread.currentThread().getName());

        // 10-second timeout; fallback response on expiration
        DeferredResult<ResponseEntity<String>> output = new DeferredResult<>(10_000L);

        output.onTimeout(() -> {
            log.warn("Async computation timed out!");
            output.setErrorResult(
                    ResponseEntity.status(HttpStatus.REQUEST_TIMEOUT).body("Computation timed out after 10s")
            );
        });

        output.onCompletion(() -> log.info("Async request completed successfully."));

        // Process computation in dedicated background executor
        CompletableFuture.runAsync(() -> {
            try {
                log.info("Processing computation in background worker: {}", Thread.currentThread().getName());
                Thread.sleep(2000); // Emulate heavy work
                output.setResult(ResponseEntity.ok("Heavy computation finished successfully!"));
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
                output.setErrorResult(ResponseEntity.internalServerError().body("Interrupted"));
            }
        });

        log.info("Tomcat worker thread {} released immediately back to pool!", Thread.currentThread().getName());
        return output;
    }
}
```

### Example 2: Realtime Streaming Updates via Server-Sent Events (`SseEmitter`)

```java
package com.example.web.async;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.http.MediaType;
import org.springframework.web.bind.annotation.*;
import org.springframework.web.servlet.mvc.method.annotation.SseEmitter;

import java.io.IOException;
import java.time.LocalTime;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;

@RestController
@RequestMapping("/api/v1/realtime")
public class RealtimeStockTickerController {

    private static final Logger log = LoggerFactory.getLogger(RealtimeStockTickerController.class);
    private final Map<String, SseEmitter> activeEmitters = new ConcurrentHashMap<>();

    @GetMapping(value = "/stocks/{symbol}/stream", produces = MediaType.TEXT_EVENT_STREAM_VALUE)
    public SseEmitter streamStockPrices(@PathVariable String symbol) {
        // Emitter timeout: 60 seconds
        SseEmitter emitter = new SseEmitter(60_000L);
        String clientId = symbol + "-" + System.currentTimeMillis();
        activeEmitters.put(clientId, emitter);

        emitter.onCompletion(() -> {
            log.info("SSE connection completed for client: {}", clientId);
            activeEmitters.remove(clientId);
        });

        emitter.onTimeout(() -> {
            log.info("SSE connection timed out for client: {}", clientId);
            emitter.complete();
            activeEmitters.remove(clientId);
        });

        emitter.onError(ex -> {
            log.warn("SSE error on client {}: {}", clientId, ex.getMessage());
            activeEmitters.remove(clientId);
        });

        // Send immediate initial handshake event
        try {
            emitter.send(SseEmitter.event()
                    .name("INIT")
                    .id("0")
                    .data("Connected to realtime stream for " + symbol));
        } catch (IOException e) {
            emitter.completeWithError(e);
        }

        return emitter;
    }

    /**
     * Broadcast price update to all connected clients.
     */
    public void broadcastPrice(String symbol, double price) {
        activeEmitters.forEach((id, emitter) -> {
            if (id.startsWith(symbol)) {
                try {
                    emitter.send(SseEmitter.event()
                            .name("PRICE_UPDATE")
                            .id(String.valueOf(System.currentTimeMillis()))
                            .data(Map.of("symbol", symbol, "price", price, "time", LocalTime.now().toString())));
                } catch (IOException ex) {
                    emitter.complete();
                    activeEmitters.remove(id);
                }
            }
        });
    }
}
```

### Example 3: Edge Case — Java 21 Virtual Threads vs Async MVC

In Java 21+, virtual threads (`spring.threads.virtual.enabled=true`) provide million-scale concurrent blocking threads at near-zero memory cost. When using virtual threads, traditional `DeferredResult` is often no longer necessary for simple I/O tasks because blocking the virtual thread does not exhaust OS platform threads.

```java
package com.example.web.async;

import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

import java.util.concurrent.TimeUnit;

@RestController
@RequestMapping("/api/v1/loom-demo")
public class VirtualThreadBlockingController {

    /**
     * With spring.threads.virtual.enabled=true, standard synchronous blocking calls
     * run on Virtual Threads. Tomcat spawns a lightweight virtual thread per request,
     * achieving high concurrency without complex DeferredResult boilerplate!
     */
    @GetMapping("/blocking-io")
    public String performBlockingIo() throws InterruptedException {
        // Blocks virtual thread cleanly without starving underlying carrier thread
        TimeUnit.SECONDS.sleep(3);
        return "Executed on VirtualThread=" + Thread.currentThread().isVirtual();
    }
}
```

### Example 4: Anti-Pattern vs Production Fix

#### Anti-Pattern: Retaining Dead `SseEmitter` Instances in Memory
```java
// ANTI-PATTERN: Leaking memory by never removing disconnected emitters
private List<SseEmitter> emitters = new CopyOnWriteArrayList<>();

@GetMapping("/stream")
public SseEmitter badStream() {
    SseEmitter emitter = new SseEmitter();
    emitters.add(emitter); // NEVER REMOVED on client disconnect! Causes Memory Leak!
    return emitter;
}
```

#### Production Fix: Explicit Lifecycle Cleanup Hooks
```java
// PRODUCTION FIX: Register onCompletion, onTimeout, and onError handlers
private final Set<SseEmitter> emitters = ConcurrentHashMap.newKeySet();

@GetMapping("/stream")
public SseEmitter goodStream() {
    SseEmitter emitter = new SseEmitter(30_000L);
    emitters.add(emitter);

    // Guaranteed cleanup on disconnect or expiration
    emitter.onCompletion(() -> emitters.remove(emitter));
    emitter.onTimeout(() -> {
        emitter.complete();
        emitters.remove(emitter);
    });
    emitter.onError(e -> emitters.remove(emitter));

    return emitter;
}
```

### Example 5: High-Performance Generative AI Streaming Endpoint (Token-by-Token)

```java
package com.example.web.async;

import org.springframework.http.MediaType;
import org.springframework.web.bind.annotation.*;
import org.springframework.web.servlet.mvc.method.annotation.ResponseBodyEmitter;

import java.io.IOException;
import java.util.concurrent.CompletableFuture;

@RestController
@RequestMapping("/api/v1/ai")
public class GenerativeAiStreamingController {

    @GetMapping(value = "/generate", produces = MediaType.TEXT_PLAIN_VALUE)
    public ResponseBodyEmitter streamAiCompletion(@RequestParam String prompt) {
        ResponseBodyEmitter emitter = new ResponseBodyEmitter(60_000L);

        CompletableFuture.runAsync(() -> {
            try {
                String[] sampleTokens = "The quick brown fox jumps over the lazy dog.".split(" ");
                for (String token : sampleTokens) {
                    emitter.send(token + " ");
                    Thread.sleep(150); // Simulate token generation delay
                }
                emitter.complete(); // Finish stream cleanly
            } catch (Exception ex) {
                emitter.completeWithError(ex);
            }
        });

        return emitter;
    }
}
```

---

## 6. Best Practices

1. **Always Register Lifecycle Callbacks**: Always register `onCompletion()`, `onTimeout()`, and `onError()` on `SseEmitter` and `ResponseBodyEmitter` to prevent memory leaks from abandoned connections.
2. **Configure Custom TaskExecutor for Async MVC**: Provide a dedicated `ThreadPoolTaskExecutor` for `mvc.async` instead of relying on default single-thread or unbounded executors.
3. **Use Virtual Threads in Java 21+**: Set `spring.threads.virtual.enabled=true` to simplify I/O-bound synchronous handlers without needing complex asynchronous boilerplate.
4. **Enforce Timeouts**: Never instantiate unbounded emitters (`new SseEmitter(-1L)`). Explicitly pass reasonable timeout intervals (e.g. 30,000ms to 120,000ms).
5. **Handle Client Disconnects Gracefully**: When sending data over an emitter, wrap `emitter.send()` in a `try-catch` block catching `IOException` (e.g. client closed browser tab) and cleanly remove the emitter.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Leaking Emitters in Memory** | Failing to remove emitters from maps on timeout/error causes `OutOfMemoryError` over time. | Attach cleanup callbacks to `onCompletion`, `onTimeout`, and `onError`. |
| **Reverse Proxy Buffering (NGINX)** | NGINX buffers chunked SSE responses, holding events until buffer fills (breaking realtime). | Send header `X-Accel-Buffering: no` or configure `proxy_buffering off` in NGINX. |
| **Missing Request Timeout Configuration** | Long-running async requests hang forever if background tasks deadlock, leaking sockets. | Set `spring.mvc.async.request-timeout: 30s` in YAML. |
| **Calling `emitter.send()` after `emitter.complete()`** | Throws `IllegalStateException: ResponseBodyEmitter has already completed`. | Guard against sending on completed or closed emitters. |
| **Sending Unformatted Text over SSE** | Sending raw text without `SseEmitter.event()` violates the W3C EventSource spec; browsers reject events. | Use `SseEmitter.event().name(...).data(...)`. |
| **Using SSE for Bidirectional Communication** | SSE is strictly unidirectionally server-to-client; it cannot receive messages from the client. | Use WebSockets or gRPC for true bidirectional communication. |

---

## 8. Performance & Security Considerations

### Reverse Proxy Buffering & Heartbeats
Intermediate reverse proxies (NGINX, Cloudflare, Envoy) automatically terminate idle HTTP connections after 60-120 seconds. To keep SSE connections active through firewalls:
1. Schedule a periodic "heartbeat" ping event (comment or empty event) every 15-30 seconds.
2. Send header `X-Accel-Buffering: no` to prevent NGINX from buffering events in memory.

### Connection Limits in HTTP/1.1
Browsers enforce a strict limit of 6 concurrent HTTP/1.1 connections per domain. Having multiple tabs open with active SSE streams quickly exhausts browser socket limits. Serve SSE connections over **HTTP/2**, where multiplexing allows unlimited streams over a single TCP connection.

---

## 9. Comparison: SSE vs WebSockets vs Long Polling

| Feature | Server-Sent Events (SSE) | WebSockets | Long Polling |
| :--- | :--- | :--- | :--- |
| **Directionality** | Unidirectional (Server $\rightarrow$ Client) | Full Duplex Bidirectional | Unidirectional (Simulated) |
| **Protocol** | Standard HTTP/1.1 or HTTP/2 | Custom WS / WSS Protocol | Standard HTTP |
| **Firewall / Proxy Compatibility** | Seamless (Standard HTTP) | Can be blocked by proxies/firewalls | Seamless (Standard HTTP) |
| **Automatic Reconnection** | Built-in browser `EventSource` support | Requires manual client JS reconnect logic | Manual client loop |
| **Overhead** | Very Low | Extremely Low | High (Repeated HTTP handshakes) |
| **Best Used For** | AI streaming, dashboards, stock tickers, notifications | Real-time gaming, collaborative chat, trading desks | Legacy fallback |

---

## 10. Interview Questions & Deep Dives

<details>
<summary><strong>Q1: What happens under the hood in Tomcat when a controller returns `DeferredResult`?</strong></summary>

**Answer:**
When a controller returns `DeferredResult`, Spring invokes `request.startAsync()`. The container marks the `HttpServletRequest` as asynchronous, detaches the current Tomcat worker thread, and returns that thread back to the container's thread pool. The client TCP connection remains open. When your application thread invokes `deferredResult.setResult(data)`, Spring dispatches an async resume event, causing Tomcat to allocate a worker thread to serialize the result and flush the HTTP response.
</details>

<details>
<summary><strong>Q2: How does Server-Sent Events (SSE) format events according to the W3C standard?</strong></summary>

**Answer:**
SSE responses have `Content-Type: text/event-stream`. Each event consists of plaintext lines terminated by a double newline (`\n\n`):
```
event: event-type-name\n
id: unique-event-id\n
retry: 5000\n
data: {"payload": "content"}\n\n
```
Browsers using the native `EventSource` API parse these fields automatically and trigger matching event listeners.
</details>

<details>
<summary><strong>Q3: How do Virtual Threads in Java 21 change the necessity of `DeferredResult` in Spring Boot?</strong></summary>

**Answer:**
Historically, asynchronous APIs like `DeferredResult` were needed because platform threads are expensive (~1MB stack, limited to a few hundred). Blocking a platform thread on slow I/O caused thread pool exhaustion. In Java 21+ with `spring.threads.virtual.enabled=true`, Tomcat executes each request on a lightweight virtual thread. Blocking a virtual thread unmounts it from the carrier OS thread, allowing millions of concurrent requests without thread starvation. As a result, `DeferredResult` is largely unnecessary for simple waiting; it remains relevant primarily for decoupled event-driven architectures.
</details>

<details>
<summary><strong>Q4: Why does NGINX sometimes break Server-Sent Events, and how do you fix it?</strong></summary>

**Answer:**
NGINX enables response buffering by default. When an SSE event is emitted, NGINX holds the small packet in its internal buffer until enough data accumulates to justify a network packet, breaking realtime streaming. To fix it, disable buffering for the location in NGINX configuration (`proxy_buffering off;`) or emit the header `X-Accel-Buffering: no` in the Spring response.
</details>

<details>
<summary><strong>Q5: What is the purpose of the `retry` field in an SSE stream?</strong></summary>

**Answer:**
The `retry` field specifies the reconnection time in milliseconds (e.g. `retry: 3000\n`). If the network connection drops, the browser's native `EventSource` client will automatically wait for the specified interval before attempting to reconnect, sending the last received event ID in the `Last-Event-ID` header to resume seamlessly.
</details>

<details>
<summary><strong>Q6: How does `ResponseBodyEmitter` differ from `SseEmitter`?</strong></summary>

**Answer:**
- `ResponseBodyEmitter`: A generic emitter for streaming multiple objects over an open HTTP connection using Spring's `HttpMessageConverter` pipeline.
- `SseEmitter`: Extends `ResponseBodyEmitter` specifically tailored to the W3C Server-Sent Events protocol, formatting payloads with `data:`, `event:`, and `id:` fields and defaulting to `text/event-stream`.
</details>

<details>
<summary><strong>Q7: Can a controller method return `CompletableFuture<T>` directly?</strong></summary>

**Answer:**
Yes. Spring MVC natively supports `CompletableFuture<T>`. Spring automatically attaches completion and exception callbacks, converting it internally into asynchronous servlet processing without requiring manual `DeferredResult` management.
</details>

<details>
<summary><strong>Q8: What happens if an async request exceeds the timeout configured in `spring.mvc.async.request-timeout`?</strong></summary>

**Answer:**
Spring triggers the `onTimeout()` callback registered on `DeferredResult` or `SseEmitter`. If no error result is provided by the callback, Spring raises an `AsyncRequestTimeoutException`, which is caught by `HandlerExceptionResolver` and translated into an HTTP 503 (Service Unavailable) or HTTP 408 (Request Timeout) error.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Basic `CompletableFuture` Controller</strong></summary>

**Task**: Write an endpoint `/api/async-greeting` that asynchronously computes and returns `"Hello Async World!"` using `CompletableFuture.supplyAsync()`.

```java
// Solution:
@RestController
public class AsyncGreetingController {
    @GetMapping("/api/async-greeting")
    public CompletableFuture<String> getGreeting() {
        return CompletableFuture.supplyAsync(() -> "Hello Async World!");
    }
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Simple SSE Ping Stream</strong></summary>

**Task**: Create an endpoint `/api/ping-stream` that emits a single SSE event `"pong"` and immediately completes.

```java
// Solution:
@GetMapping(value = "/api/ping-stream", produces = MediaType.TEXT_EVENT_STREAM_VALUE)
public SseEmitter ping() throws IOException {
    SseEmitter emitter = new SseEmitter();
    emitter.send(SseEmitter.event().data("pong"));
    emitter.complete();
    return emitter;
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): SSE Heartbeat Scheduler</strong></summary>

**Task**: Implement a scheduled service that sends an SSE comment `":keepalive\n\n"` every 15 seconds to a collection of active emitters.

```java
// Solution:
@Component
public class SseHeartbeatService {
    private final Set<SseEmitter> emitters = ConcurrentHashMap.newKeySet();

    public void register(SseEmitter emitter) {
        emitters.add(emitter);
        emitter.onCompletion(() -> emitters.remove(emitter));
    }

    @Scheduled(fixedRate = 15000)
    public void sendHeartbeats() {
        emitters.forEach(emitter -> {
            try {
                emitter.send(SseEmitter.event().comment("keepalive"));
            } catch (IOException e) {
                emitters.remove(emitter);
            }
        });
    }
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): DeferredResult with Custom Timeout Handler</strong></summary>

**Task**: Write a `DeferredResult<String>` endpoint with a 3-second timeout returning `"TIMEOUT_EXPIRED"` with HTTP 408 on expiration.

```java
// Solution:
@GetMapping("/api/timeout-demo")
public DeferredResult<ResponseEntity<String>> timeoutDemo() {
    DeferredResult<ResponseEntity<String>> result = new DeferredResult<>(3000L);
    result.onTimeout(() -> {
        result.setErrorResult(ResponseEntity.status(HttpStatus.REQUEST_TIMEOUT).body("TIMEOUT_EXPIRED"));
    });
    return result;
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Resumable SSE Stream with `Last-Event-ID`</strong></summary>

**Task**: Build an SSE endpoint that inspects the incoming `Last-Event-ID` HTTP header, retrieves missed events from an in-memory event buffer, and replays them to the reconnecting client before streaming live events.
</details>

<details>
<summary><strong>Exercise 6 (Hard): Multi-Client Realtime Chat Room via SSE and POST</strong></summary>

**Task**: Build a chat room service with an SSE endpoint for receiving incoming messages and an HTTP POST endpoint for publishing messages, ensuring thread-safe fan-out broadcasting and clean eviction of closed sockets.
</details>

---

## 12. Summary & Quick Reference

- **`DeferredResult<T>`**: Releases servlet worker thread while waiting for background task completion.
- **`SseEmitter`**: Unidirectional server-to-client streaming over standard HTTP (`text/event-stream`).
- **Memory Invariant**: Always clean up emitter collections in `onCompletion`, `onTimeout`, and `onError`.
- **Java 21 Synergy**: Use Virtual Threads (`spring.threads.virtual.enabled=true`) for synchronous blocking I/O.
- **Proxy Invariant**: Send `X-Accel-Buffering: no` to prevent intermediate proxies from buffering SSE events.

---

## 13. Further Reading

- [Spring Framework Reference: Asynchronous Request Processing](https://docs.spring.io/spring-framework/reference/web/webmvc/mvc-ann-async.html)
- [W3C Server-Sent Events Specification](https://html.spec.whatwg.org/multipage/server-sent-events.html)
- [Embracing Virtual Threads in Spring Boot 3.2+](https://spring.io/blog/2023/09/20/embracing-virtual-threads)
- [MDN Web Docs: Using Server-Sent Events](https://developer.mozilla.org/en-US/docs/Web/API/Server-sent_events/Using_server-sent_events)
