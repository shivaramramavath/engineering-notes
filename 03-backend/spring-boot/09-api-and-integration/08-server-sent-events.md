# Server-Sent Events (SSE) with Spring Boot

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x |
| **Java Version** | 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | `SseEmitter`, `ResponseBodyEmitter`, `MediaType.TEXT_EVENT_STREAM_VALUE` |

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
Many real-time web applications require continuous data pushed from server to client, but never require the client to send messages back over the same channel. Examples include live stock tickers, LLM token streaming, CI/CD build logs, and background job progress bars. Implementing full-duplex WebSockets for these use cases introduces unnecessary architectural complexity: custom framing protocols, proxy/firewall traversal challenges, and manual reconnection logic.

### The Evolution
Server-Sent Events (SSE), formalized as part of the HTML5 standard (W3C), enables a web server to push unformatted or JSON-formatted events asynchronously to a browser over standard HTTP using the `text/event-stream` MIME type. Crucially, browsers feature native `EventSource` JavaScript APIs that handle automatic reconnection, connection drop detection, and event ID resumption (`Last-Event-ID`) out of the box without external client libraries.

### Relevance in Modern Engineering
In the era of Generative AI and LLM token generation (e.g., ChatGPT/Claude streaming), SSE is the industry standard protocol. In Spring Boot 3, developers can stream events imperatively using `SseEmitter` within Spring MVC or reactively using `Flux<ServerSentEvent<T>>` in Spring WebFlux.

---

## 2. Core Concepts

- **`text/event-stream` MIME Type**: Standard HTTP content type signaling a persistent, chunked text stream.
- **SSE Frame Structure**: Standard text-based format comprising four optional field types separated by double newlines (`\n\n`):
  - `event: <custom-event-name>\n`
  - `data: <json-or-text-payload>\n`
  - `id: <unique-event-id>\n`
  - `retry: <reconnect-delay-ms>\n\n`
- **`SseEmitter`**: Spring MVC class (`org.springframework.web.servlet.mvc.method.annotation.SseEmitter`) for asynchronous HTTP streaming.
- **`Last-Event-ID` Header**: HTTP header sent by the client upon reconnecting, allowing the server to replay missed events.
- **Asynchronous Request Processing**: Spring MVC unbinds the container thread (Tomcat worker thread) while the HTTP connection remains open, re-binding threads only when events are emitted.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------+
|                        Spring MVC SseEmitter Architecture                         |
|                                                                                   |
| 1. Client Initiates Stream                                                        |
|    GET /api/v1/stream/logs (Accept: text/event-stream)                            |
|                   |                                                               |
|                   v                                                               |
| 2. Controller Creates & Returns SseEmitter                                        |
|    SseEmitter emitter = new SseEmitter(timeoutMs);                                |
|    return emitter;                                                                |
|                   |                                                               |
|                   v                                                               |
| 3. Container Thread Released (Servlet 3.0+ Async)                                 |
|    AsyncContext started; Tomcat thread returns to pool to serve other requests    |
|                   |                                                               |
|                   v                                                               |
| 4. Asynchronous Event Generation (Background Worker / Kafka Listener)             |
|    emitter.send(SseEmitter.event()                                                |
|             .id("evt_101")                                                        |
|             .name("build-status")                                                 |
|             .data(new StatusDto("IN_PROGRESS", 45)));                             |
|                   |                                                               |
|                   v                                                               |
| 5. Chunked HTTP Response Emitted                                                  |
|    HTTP/1.1 200 OK                                                                |
|    Content-Type: text/event-stream                                                |
|    Transfer-Encoding: chunked                                                     |
|                                                                                   |
|    id: evt_101\n                                                                  |
|    event: build-status\n                                                          |
|    data: {"status":"IN_PROGRESS","progress":45}\n\n                               |
|                   |                                                               |
|                   v                                                               |
| 6. Stream Termination: emitter.complete() or emitter.completeWithError(...)       |
+-----------------------------------------------------------------------------------+
```

When a controller returns an `SseEmitter`, Spring MVC initiates an asynchronous Servlet request (`request.startAsync()`). The underlying container worker thread is immediately released back to the thread pool, avoiding thread starvation. The connection remains open in a chunked transfer state. When `emitter.send()` is called, Spring formats the event according to the W3C SSE standard, flushes the response buffer over the open TCP socket, and keeps the stream alive until `complete()` is called or the timeout expires.

---

## 4. Syntax & Usage

### Maven Dependency
```xml
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-starter-web</artifactId>
</dependency>
```

### Application Properties for Async Timeout
```properties
# Global async request timeout for Spring MVC
spring.mvc.async.request-timeout=180000
```

---

## 5. Practical Examples

### Example 1: Basic LLM Token Streaming Controller
```java
package com.example.platform.sse.controller;

import org.springframework.http.MediaType;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RequestParam;
import org.springframework.web.bind.annotation.RestController;
import org.springframework.web.servlet.mvc.method.annotation.SseEmitter;

import java.io.IOException;
import java.util.concurrent.Executors;

@RestController
@RequestMapping("/api/v1/ai")
public class LlmTokenStreamingController {

    @GetMapping(value = "/generate", produces = MediaType.TEXT_EVENT_STREAM_VALUE)
    public SseEmitter streamTokens(@RequestParam String prompt) {
        // Create emitter with 60-second timeout
        SseEmitter emitter = new SseEmitter(60_000L);

        Executors.newVirtualThreadPerTaskExecutor().submit(() -> {
            try {
                String[] tokens = "Spring Boot 3 natively empowers high throughput Server Sent Events".split(" ");
                for (int i = 0; i < tokens.length; i++) {
                    Thread.sleep(150); // Simulate token generation latency

                    SseEmitter.SseEventBuilder event = SseEmitter.event()
                            .id(String.valueOf(i))
                            .name("token")
                            .data(tokens[i] + " ");

                    emitter.send(event);
                }
                emitter.complete();
            } catch (Exception e) {
                emitter.completeWithError(e);
            }
        });

        return emitter;
    }
}
```

### Example 2: Enterprise Multi-Client Subscription Manager
```java
package com.example.platform.sse.service;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Service;
import org.springframework.web.servlet.mvc.method.annotation.SseEmitter;

import java.io.IOException;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

@Service
public class TradeStreamManager {

    private static final Logger log = LoggerFactory.getLogger(TradeStreamManager.class);
    private final Map<String, SseEmitter> activeEmitters = new ConcurrentHashMap<>();

    public SseEmitter subscribe(String clientId) {
        SseEmitter emitter = new SseEmitter(120_000L); // 2 minutes

        activeEmitters.put(clientId, emitter);
        log.info("Client subscribed to trade stream: {}. Total active: {}", clientId, activeEmitters.size());

        emitter.onCompletion(() -> {
            log.info("Emitter completed for client: {}", clientId);
            activeEmitters.remove(clientId);
        });

        emitter.onTimeout(() -> {
            log.warn("Emitter timed out for client: {}", clientId);
            emitter.complete();
            activeEmitters.remove(clientId);
        });

        emitter.onError(e -> {
            log.error("Emitter error for client: {}", clientId, e);
            activeEmitters.remove(clientId);
        });

        return emitter;
    }

    public void broadcastTradeEvent(String tradeJson) {
        activeEmitters.forEach((clientId, emitter) -> {
            try {
                emitter.send(SseEmitter.event()
                        .name("trade-executed")
                        .data(tradeJson));
            } catch (IOException e) {
                log.warn("Failed to send trade to client {}. Removing dead emitter.", clientId);
                emitter.complete();
                activeEmitters.remove(clientId);
            }
        });
    }
}
```

### Example 3: Resumable Streams with `Last-Event-ID`
```java
package com.example.platform.sse.controller;

import org.springframework.http.MediaType;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RequestHeader;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;
import org.springframework.web.servlet.mvc.method.annotation.SseEmitter;

import java.io.IOException;
import java.util.List;
import java.util.concurrent.Executors;

@RestController
@RequestMapping("/api/v1/orders")
public class ResumableOrderEventController {

    record OrderEvent(long eventId, String status) {}

    @GetMapping(value = "/stream", produces = MediaType.TEXT_EVENT_STREAM_VALUE)
    public SseEmitter streamOrderEvents(
            @RequestHeader(value = "Last-Event-ID", required = false, defaultValue = "0") long lastEventId) {

        SseEmitter emitter = new SseEmitter(300_000L);

        Executors.newVirtualThreadPerTaskExecutor().submit(() -> {
            try {
                // Replay missed events from history buffer if client reconnected
                List<OrderEvent> missedEvents = fetchEventsSince(lastEventId);
                for (OrderEvent event : missedEvents) {
                    emitter.send(SseEmitter.event()
                            .id(String.valueOf(event.eventId()))
                            .name("order-update")
                            .reconnectTime(5000) // Instruct browser to reconnect after 5s if dropped
                            .data(event));
                }
                emitter.complete();
            } catch (IOException e) {
                emitter.completeWithError(e);
            }
        });

        return emitter;
    }

    private List<OrderEvent> fetchEventsSince(long id) {
        return List.of(new OrderEvent(id + 1, "PROCESSED"), new OrderEvent(id + 2, "SHIPPED"));
    }
}
```

### Example 4: Memory Leak from Uncleaned Emitters (Anti-Pattern Fix)
```java
// ANTI-PATTERN: Adding SseEmitters to a static collection without attaching onCompletion/onTimeout callbacks.
// When clients disconnect or browser tabs close, the emitter remains in memory forever, causing OutOfMemoryError.
// static List<SseEmitter> emitters = new ArrayList<>();
// emitters.add(emitter); // LEAK!

// FIX: Always attach cleanup callbacks to remove disconnected emitters from storage
package com.example.platform.sse.util;

import org.springframework.web.servlet.mvc.method.annotation.SseEmitter;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;

public class SafeEmitterRegistry {

    private final Set<SseEmitter> emitters = ConcurrentHashMap.newKeySet();

    public SseEmitter register(long timeout) {
        SseEmitter emitter = new SseEmitter(timeout);
        emitters.add(emitter);

        Runnable cleanup = () -> emitters.remove(emitter);
        emitter.onCompletion(cleanup);
        emitter.onTimeout(cleanup);
        emitter.onError(e -> cleanup.run());

        return emitter;
    }
}
```

### Example 5: High-Performance Heartbeat / Keep-Alive Scheduled Worker
```java
package com.example.platform.sse.service;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.scheduling.annotation.Scheduled;
import org.springframework.stereotype.Component;
import org.springframework.web.servlet.mvc.method.annotation.SseEmitter;

import java.io.IOException;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;

@Component
public class SseHeartbeatService {

    private static final Logger log = LoggerFactory.getLogger(SseHeartbeatService.class);
    private final Set<SseEmitter> activeConnections = ConcurrentHashMap.newKeySet();

    public void track(SseEmitter emitter) {
        activeConnections.add(emitter);
        emitter.onCompletion(() -> activeConnections.remove(emitter));
        emitter.onTimeout(() -> activeConnections.remove(emitter));
    }

    // Every 15 seconds send an SSE comment or heartbeat to prevent proxy timeout
    @Scheduled(fixedRate = 15000)
    public void sendHeartbeat() {
        for (SseEmitter emitter : activeConnections) {
            try {
                // SSE comments start with a colon (: ping) and are ignored by browser EventSource
                emitter.send(SseEmitter.event().comment("heartbeat-ping"));
            } catch (IOException e) {
                log.debug("Removing dead emitter during heartbeat sweep");
                emitter.complete();
                activeConnections.remove(emitter);
            }
        }
    }
}
```

---

## 6. Best Practices

1. **Always Implement Completion & Timeout Callbacks**: Emitters that lack `onCompletion()` and `onTimeout()` handlers will cause severe memory leaks as disconnected sockets accumulate in heap storage.
2. **Send Periodic Heartbeat Pings**: Corporate proxies, AWS ALBs, and Cloudflare drop idle HTTP connections after 60 seconds. Send an SSE comment (`: ping\n\n`) every 15-30 seconds to keep the socket alive.
3. **Use Virtual Threads for Dispatch**: Java 21 Virtual Threads (`Executors.newVirtualThreadPerTaskExecutor()`) are ideal for background emitters waiting on downstream events.
4. **Specify `reconnectTime`**: Configure the reconnect interval via `.reconnectTime(5000)` so browser clients automatically back off rather than hammering the server if the connection drops.
5. **Handle Client Disconnects Defensively**: Catch `IOException` (ClientAbortException / Broken pipe) when calling `emitter.send()` and immediately remove the emitter from active collections.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Omitting `produces = MediaType.TEXT_EVENT_STREAM_VALUE` | Browser treats response as standard JSON, waiting for connection to close before parsing | Add `produces = MediaType.TEXT_EVENT_STREAM_VALUE` to `@GetMapping`. |
| Leaking dead emitters in collections | Client closes tab, but emitter remains in map, eventually causing JVM `OutOfMemoryError` | Remove emitter in `onCompletion()`, `onTimeout()`, and `onError()` callbacks. |
| Forgetting heartbeats behind reverse proxies | Nginx or AWS ALB terminates the idle connection with a 504 Gateway Timeout | Implement a `@Scheduled` heartbeat emitting comments (`: ping`) every 15 seconds. |
| Blocking Tomcat container thread | Thread sits waiting for business logic, exhausting the HTTP worker pool | Return `SseEmitter` immediately; execute event emissions on separate background threads. |
| Sending raw non-JSON objects without Jackson | Serialization errors abort the stream unexpectedly | Ensure emitted objects are serializable records or DTOs configured for Jackson. |
| Browser connection limit reached over HTTP/1.1 | Browsers enforce a limit of 6 simultaneous open SSE connections per domain over HTTP/1.1 | Deploy over HTTP/2, which supports hundreds of multiplexed SSE streams per domain. |

---

## 8. Performance & Security Notes

- **HTTP/2 Multiplexing**: Over HTTP/1.1, browsers enforce a strict limit of 6 open connections per domain, meaning 6 open SSE tabs will freeze the application. Deploying over HTTP/2 eliminates this constraint completely.
- **Buffer Bloat & Backpressure**: If the server pushes events faster than the client can consume them, the operating system TCP send buffer fills up, increasing JVM memory consumption.
- **Authorization**: Secure SSE endpoints using standard Spring Security filters. Because the standard browser `EventSource` API does not natively support custom headers, authenticate via HttpOnly session cookies or query parameter tokens validated in custom filters.

---

## 9. Comparison / When to Use What

| Dimension | Server-Sent Events (SSE) | WebSockets | HTTP Long Polling |
|---|---|---|---|
| **Direction** | Server-to-Client Only | Bidirectional (Full Duplex) | Bidirectional (Simulated) |
| **Protocol** | Standard HTTP/1.1 or HTTP/2 | WebSocket Protocol (ws://) | Standard HTTP |
| **Reconnection** | Native browser auto-reconnect | Manual client-side logic | Loop-based manual polling |
| **Firewall / Proxy** | Traverses standard HTTP ports | Sometimes blocked by firewalls | Traverses standard HTTP ports |
| **Ideal For** | LLM token streaming, tickers, logs | Chat, collaborative gaming | Legacy fallback only |

---

## 10. Interview Questions

<details>
<summary>1. How does Server-Sent Events (SSE) differ from WebSockets?</summary>
SSE is a unidirectional (server-to-client) streaming protocol over standard HTTP using the `text/event-stream` format. It features native browser reconnection and event ID resumption. WebSockets is a full-duplex, bidirectional protocol over a dedicated TCP connection, requiring custom sub-protocols (like STOMP) and manual reconnection handling.
</details>

<details>
<summary>2. How does Spring MVC manage thread consumption when an SseEmitter is kept open for minutes?</summary>
Spring MVC relies on Servlet 3.0 asynchronous request processing (`AsyncContext`). When a controller returns an `SseEmitter`, the initial Tomcat/Jetty worker thread is returned immediately to the pool. Threads are only dispatched temporarily when `emitter.send()` writes data to the client's output stream.
</details>

<details>
<summary>3. Why do browsers have a 6-connection limit with SSE, and how is it resolved?</summary>
The HTTP/1.1 specification restricts browsers to a maximum of 6 concurrent persistent TCP connections per host domain. If a user opens 6 tabs with active SSE connections, all subsequent HTTP requests to that domain freeze. Upgrading the server to HTTP/2 eliminates this restriction by multiplexing all streams over a single TCP connection.
</details>

<details>
<summary>4. What is the role of the Last-Event-ID header?</summary>
When a network disconnection occurs, the browser's native `EventSource` automatically reconnects and includes the `Last-Event-ID` header containing the ID of the last event successfully received. The server inspects this header and replays any missed events from an internal event log or Kafka offset.
</details>

<details>
<summary>5. Why are periodic heartbeats essential for production SSE connections?</summary>
Intermediary network infrastructure (reverse proxies like Nginx, AWS Application Load Balancers, Cloudflare) automatically terminate idle TCP connections that show no activity for 30-60 seconds. Periodic heartbeat comments (`: keep-alive\n\n`) keep the connection active without interfering with client event listeners.
</details>

<details>
<summary>6. How do you prevent memory leaks when managing thousands of concurrent SseEmitters?</summary>
Store emitters in thread-safe collections (`ConcurrentHashMap`) and register cleanup routines on `emitter.onCompletion()`, `emitter.onTimeout()`, and `emitter.onError()`. If an `IOException` occurs during `emitter.send()`, catch it, mark the emitter complete, and remove it from active storage.
</details>

<details>
<summary>7. Can you send custom event types with SseEmitter?</summary>
Yes. Using `SseEmitter.event().name("my-custom-event").data(payload)`, the event is labeled with `event: my-custom-event`. In the browser, clients attach listeners specifically to that event type: `eventSource.addEventListener("my-custom-event", handler)`.
</details>

<details>
<summary>8. How do you pass authentication tokens to an SSE endpoint if EventSource doesn't support headers?</summary>
Standard browser `EventSource` cannot send custom headers (like `Authorization: Bearer <token>`). Common solutions include using HttpOnly secure cookies, passing a short-lived single-use ticket as a query parameter (`/stream?ticket=xyz`), or using polyfill libraries (like `fetch-event-source`) that allow custom HTTP headers.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1 (Easy): Create a simple time-emitting SSE controller</summary>
Write a `@RestController` endpoint returning an `SseEmitter` that emits the current server timestamp every second for 5 seconds and then completes.
</details>

<details>
<summary>Exercise 2 (Easy): Configure SSE custom event name and ID</summary>
Build an endpoint that emits an event with name `"user-login"`, unique UUID ID, and a JSON payload.
</details>

<details>
<summary>Exercise 3 (Medium): Implement an SSE-based build progress tracker</summary>
Create a service that emits progress updates (0% to 100%) for a simulated long-running background task, ensuring the emitter completes properly upon task finish.
</details>

<details>
<summary>Exercise 4 (Medium): Build a thread-safe broadcaster with automated cleanup</summary>
Implement a singleton `SseBroadcaster` maintaining active emitters in a `CopyOnWriteArrayList` with proper `onCompletion` and `onTimeout` cleanup.
</details>

<details>
<summary>Exercise 5 (Hard): Implement event replay with Last-Event-ID</summary>
Create an endpoint that stores the last 100 emitted events in a circular buffer and replays missed events to reconnecting clients based on `Last-Event-ID`.
</details>

<details>
<summary>Exercise 6 (Hard): Reactive SSE Streaming with Spring WebFlux</summary>
Write a WebFlux controller endpoint returning `Flux<ServerSentEvent<StockQuote>>` that streams simulated real-time stock prices every 500ms using Reactor operators.
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------+
|                         Server-Sent Events (SSE) Summary                        |
+---------------------------------------------------------------------------------+
| MIME Type:          text/event-stream                                           |
| Class:              org.springframework.web.servlet.mvc.method.annotation.SseEmitter |
| Method Return:      return sseEmitter; (Asynchronous Servlet 3.0 processing)    |
| Builder API:        SseEmitter.event().id("1").name("evt").reconnectTime(3000)  |
| Browser API:        new EventSource('/stream')                                  |
| Disconnect Trap:    Always attach onCompletion() and onTimeout() callbacks      |
| Network Rule:       Send heartbeat comments (: ping) every 15s to bypass ALBs   |
| Browser Trap:       HTTP/1.1 limits to 6 connections; deploy over HTTP/2        |
+---------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [W3C Server-Sent Events Specification](https://html.spec.whatwg.org/multipage/server-sent-events.html)
- [MDN Using Server-Sent Events](https://developer.mozilla.org/en-US/docs/Web/API/Server-sent_events/Using_server-sent_events)
- [Spring WebMvc Asynchronous Request Processing](https://docs.spring.io/spring-framework/reference/web/webmvc/mvc-controller/ann-async.html)
