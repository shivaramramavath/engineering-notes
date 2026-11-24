# Embedded Web Server Tuning: Tomcat and Netty in Spring Boot

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
In Spring Boot applications, the embedded web server (Apache Tomcat by default in Spring MVC, Eclipse Jetty, or Reactor Netty in WebFlux) is the primary ingress gateway for all network traffic. Misconfiguring the web server produces severe production bottlenecks:
- Setting worker threads too high exhausts OS memory and causes extreme CPU context switching.
- Setting connection limits (`max-connections`, `accept-count`) too low results in dropped TCP connections (`Connection reset by peer`) during traffic surges.
- Misconfigured HTTP keep-alive timeouts hold server sockets open indefinitely, blocking new clients from connecting.
- Netty byte buffer allocator leaks in reactive applications exhaust native direct memory, triggering fatal `OutOfMemoryError: Direct buffer memory`.

### Historical Evolution
Before Spring Boot, Java applications were packaged as `.war` archives and deployed onto shared external servlet containers (standalone Tomcat, WebLogic, WebSphere), where container tuning was managed externally by system administrators.

Spring Boot revolutionized Java architecture by **embedding** the web container directly into the executable fat JAR. Developers are now directly responsible for fine-tuning connector thread pools, TCP socket listen queues, and HTTP/2 multiplexing within `application.yml`.

### Production Relevance
Tuning embedded Tomcat and Netty guarantees:
- Microservices comfortably absorb traffic spikes up to 50,000+ concurrent connections.
- HTTP keep-alive reuse reduces connection latency by over 70% between API Gateways and microservices.
- Native direct memory allocation remains strictly bounded under heavy reactive streaming load.

---

## 2. Core Concepts

### Tomcat Connector Architecture Components
1. **Acceptor Thread**: Listens on the OS server socket and accepts incoming raw TCP connection requests.
2. **Poller Thread (NIO)**: Uses OS multiplexing (`epoll` on Linux, `kqueue` on macOS) to monitor open TCP sockets for incoming data readiness.
3. **Worker Thread Pool (`max-threads` / `min-spare`)**: Platform threads that read HTTP request payloads from sockets, parse HTTP headers, and invoke the Spring `DispatcherServlet`.
4. **`max-connections`**: The maximum number of simultaneous TCP connections that the server will accept and maintain.
5. **`accept-count` (Listen Backlog)**: The operating system socket queue length for incoming connection requests when all worker threads are busy and `max-connections` is reached.
6. **Reactor Netty (WebFlux)**: Non-blocking, event-driven network runtime built on Netty's EventLoopGroup model (`epoll`/`kqueue`), where a small pool of worker threads (typically matching CPU core count) handles tens of thousands of concurrent connections.

---

## 3. How It Works Internally

### Apache Tomcat NIO Connector Request Pipeline

```
CLIENT (Browser / API Gateway)
  |  Sends TCP SYN packet
  v
+------------------------------------------------------------------------------------+
| 1. OS TCP LISTEN BACKLOG (accept-count: default 100)                               |
|    Kernel manages queue of completed 3-way handshakes.                             |
|    --> FULL: Kernel drops SYN or rejects with "Connection refused"!                |
+------------------------------------------------------------------------------------+
  |
  v
+------------------------------------------------------------------------------------+
| 2. TOMCAT ACCEPTOR THREAD (server.tomcat.acceptor-thread-count: 1)                 |
|    Calls serverSocket.accept() -> Checks if active connections < max-connections   |
|    Registers socket channel with NIO Poller thread.                                |
+------------------------------------------------------------------------------------+
  |
  v
+------------------------------------------------------------------------------------+
| 3. TOMCAT NIO POLLER THREAD (epoll)                                                |
|    Selects sockets ready for I/O (bytes arrived on wire)                           |
|    Hands off socket to Worker Thread Pool (Executor).                              |
+------------------------------------------------------------------------------------+
  |
  v
+------------------------------------------------------------------------------------+
| 4. TOMCAT WORKER THREAD POOL (server.tomcat.threads.max: default 200)             |
|    - Parses HTTP headers and body                                                  |
|    - Calls DispatcherServlet.doService() -> Controller                             |
|    - Serializes response bytes and writes back to socket                           |
+------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Configuration Reference

### Production Tomcat Tuning (`application.yml`)

```yaml
server:
  port: 8080
  # Enable HTTP/2 for stream multiplexing
  http2:
    enabled: true
  tomcat:
    # 1. Thread Pool Configuration
    threads:
      max: 200 # Standard platform threads (Omit if Virtual Threads enabled)
      min-spare: 20 # Warm worker threads kept alive
    # 2. Connection and Backlog Limits
    max-connections: 8192 # Max concurrent open TCP sockets
    accept-count: 1000 # OS TCP listen backlog queue
    # 3. Keep-Alive Configuration
    keep-alive-timeout: 60000 # 60 seconds (Match or exceed upstream API Gateway)
    max-keep-alive-requests: 100 # Requests per keep-alive connection
    # 4. Request Buffer Limits
    max-http-form-post-size: 2MB
    max-swallow-size: 2MB
  # Enable GZIP Compression for payload speedup
  compression:
    enabled: true
    mime-types: text/html,text/xml,text/plain,application/json,application/xml
    min-response-size: 1024 # Only compress payloads > 1KB
```

### Production Reactor Netty Tuning (`application.yml`)
```yaml
# For Spring WebFlux applications
spring:
  main:
    web-application-type: reactive

server:
  port: 8080
  netty:
    connection-timeout: 2000 # 2 seconds
    idle-timeout: 30s
```

---

## 5. Practical Examples

### Example 1: Programmatic Tomcat Customizer with Protocol Handler Tuning
```java
package com.example.performance.server;

import org.apache.coyote.http11.AbstractHttp11Protocol;
import org.springframework.boot.web.embedded.tomcat.TomcatServletWebServerFactory;
import org.springframework.boot.web.server.WebServerFactoryCustomizer;
import org.springframework.stereotype.Component;

import java.time.Duration;

@Component
public class HardenedTomcatWebServerCustomizer 
    implements WebServerFactoryCustomizer<TomcatServletWebServerFactory> {

    @Override
    public void customize(TomcatServletWebServerFactory factory) {
        factory.addConnectorCustomizers(connector -> {
            var protocol = (AbstractHttp11Protocol<?>) connector.getProtocolHandler();

            // Set TCP socket connection timeouts
            protocol.setConnectionTimeout((int) Duration.ofSeconds(20).toMillis());
            protocol.setKeepAliveTimeout((int) Duration.ofSeconds(65).toMillis());
            protocol.setMaxKeepAliveRequests(500);

            // TCP No Delay (Disables Nagle's algorithm for sub-millisecond response latency)
            protocol.setTcpNoDelay(true);

            // Compress responses
            protocol.setCompression("on");
            protocol.setCompressionMinSize(1024);
        });
    }
}
```

### Example 2: Configuring Reactor Netty Event Loop Thread Sizing
```java
package com.example.performance.server;

import org.springframework.boot.web.embedded.netty.NettyReactiveWebServerFactory;
import org.springframework.boot.web.server.WebServerFactoryCustomizer;
import org.springframework.stereotype.Component;
import reactor.netty.resources.LoopResources;

@Component
public class CustomNettyLoopCustomizer 
    implements WebServerFactoryCustomizer<NettyReactiveWebServerFactory> {

    @Override
    public void customize(NettyReactiveWebServerFactory factory) {
        // Dedicate worker threads equal to available CPU cores for pure non-blocking I/O
        int cores = Runtime.getRuntime().availableProcessors();
        LoopResources loops = LoopResources.create("prod-event-loop", 1, cores, true);

        factory.addServerCustomizers(httpServer -> 
            httpServer.runOn(loops)
                .tcpConfiguration(tcpServer -> 
                    tcpServer.doOnChannelInit((observer, channel, remoteAddress) -> {
                        // Native channel socket options
                    })
                )
        );
    }
}
```

### Example 3: Diagnosing Tomcat Worker Thread Exhaustion via Actuator
When users complain of 504 Gateway Timeouts, query Tomcat thread metrics:

```bash
# Query active Tomcat worker threads
curl -s http://localhost:9090/actuator/metrics/tomcat.threads.busy
# Output: {"measurements":[{"statistic":"VALUE","value":200.0}]}

curl -s http://localhost:9090/actuator/metrics/tomcat.threads.current
# Output: {"measurements":[{"statistic":"VALUE","value":200.0}]}
```
*Diagnosis*: `tomcat.threads.busy == tomcat.threads.current == 200` (100% thread pool saturation). All worker threads are blocked on a slow downstream resource (e.g. saturated database or slow external API).

### Example 4: Anti-Pattern vs. Production Fix (Setting `max-threads: 2000`)
```yaml
# ANTI-PATTERN: Blindly cranking up worker threads to fix thread exhaustion
# Result: 2,000 platform threads consume 2GB RAM just for thread stacks.
# OS spends 80% of CPU time in kernel context switching -> Cascading server freeze!
server:
  tomcat:
    threads:
      max: 2000 # CATASTROPHIC IN PRODUCTION!

# PRODUCTION FIX: Enable Virtual Threads in Java 21 LTS
# Virtual threads consume < 1KB each in heap; handles 50,000 concurrent requests smoothly!
spring:
  threads:
    virtual:
      enabled: true

server:
  tomcat:
    max-connections: 50000
    accept-count: 5000
```

### Example 5: Enabling HTTP/2 Multiplexing with TLS
```yaml
server:
  port: 8443
  http2:
    enabled: true # Eliminates head-of-line blocking by multiplexing streams over 1 TCP connection
  ssl:
    enabled: true
    key-store: "classpath:keystore.p12"
    key-store-password: "${KEYSTORE_SECRET}"
    key-store-type: PKCS12
```

---

## 6. Best Practices & Production Guidelines

1. **Keep `keep-alive-timeout` Longer than Upstream Ingress/ALB**: If an upstream AWS Application Load Balancer or NGINX ingress has an idle timeout of 60 seconds, configure Tomcat's `keep-alive-timeout` to **65 seconds**. This prevents race conditions where Tomcat closes a connection just as the load balancer sends a new request.
2. **Enable Virtual Threads Instead of Raising `max-threads`**: In Spring Boot 3.2+, never increase `server.tomcat.threads.max` beyond 200. Enable `spring.threads.virtual.enabled=true` to let Tomcat scale to 50,000 concurrent requests safely.
3. **Set `accept-count` Adequately for Traffic Bursts**: The default `accept-count` (100) is too small for sudden viral spikes. Set `accept-count: 1000` to buffer incoming TCP connections while worker threads churn through queues.
4. **Enable Response Compression (GZIP)**: Compressing JSON payloads reduces network bandwidth consumption by 60%–80% for payloads $> 1\text{KB}$. Always set `server.compression.min-response-size: 1024` to avoid wasting CPU compressing tiny responses.
5. **Always Set `tcpNoDelay: true`**: Ensure Nagle's algorithm is disabled. Nagle's algorithm delays small packets to combine them into larger frames, adding artificial 40ms latency delays to microservice REST exchanges.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Cranking `max-threads` to 1,000+ | High thread count exhausts OS memory and causes severe CPU context switching thrashing. | Enable Virtual Threads or fix downstream blocking bottlenecks. |
| Inadequate `accept-count` (100) | Sudden burst of connections fills the OS backlog, causing immediate `Connection Refused` errors. | Increase `server.tomcat.accept-count: 1000`. |
| Tomcat keep-alive shorter than Load Balancer | Causes intermittent HTTP 502 Bad Gateway errors due to race conditions during connection reuse. | Set Tomcat keep-alive 5s longer than ALB/NGINX timeout. |
| Blocking the Netty Event Loop in WebFlux | Calling `Thread.sleep()` or blocking JDBC inside WebFlux freezes the entire Netty event loop for all users. | Offload blocking calls to `Schedulers.boundedElastic()`. |
| Compressing tiny payloads (< 500 bytes) | GZIP overhead makes tiny payloads larger and wastes CPU cycles. | Set `server.compression.min-response-size: 1024`. |
| Missing Direct Memory limits in Netty | Netty allocates off-heap buffers without bounds until Linux OOM killer kills the container. | Set `-XX:MaxDirectMemorySize=256m` in container JVM flags. |

---

## 8. Performance & Security Notes

### Slowloris DoS Protection
- Attackers send HTTP headers extremely slowly (e.g. 1 byte every 10 seconds) to hold worker threads open indefinitely.
- Protect Tomcat using `server.tomcat.connection-timeout: 5000` (5 seconds). Any socket failing to complete header transmission within 5 seconds is forcibly closed.

### Memory Impact of Large Request Buffers
- Setting `server.tomcat.max-http-form-post-size: 50MB` allows clients to upload large payloads into memory.
- If 100 concurrent clients upload 50MB simultaneously, Tomcat will allocate 5GB of heap memory, triggering an `OutOfMemoryError`. Enforce streaming uploads or cap buffer sizes to 2MB.

---

## 9. Comparison / When to Use What

| Feature | Embedded Apache Tomcat | Embedded Reactor Netty |
|---|---|---|
| **Programming Paradigm** | Servlet (Spring MVC / Blocking) | Reactive (Spring WebFlux / Non-blocking) |
| **Default Worker Threads** | 200 platform threads | CPU Core Count (e.g. 8 threads) |
| **Virtual Threads Support** | **Native first-class (Spring Boot 3.2+)** | Not applicable (Netty is already non-blocking) |
| **Max Concurrent Sockets** | High (8,192 default) | Extremely High (100,000+) |
| **Direct Memory Usage** | Low (Heap-based buffers) | High (Netty PooledByteBufAllocator) |
| **Best Used For** | 90% of enterprise CRUD / REST microservices | Real-time streaming, SSE, WebSockets, gateway proxies |

---

## 10. Interview Questions & Answers

### Q1: What is the difference between `max-connections` and `accept-count` in Tomcat?
<details>
<summary>Click to view answer</summary>

- `max-connections`: The maximum number of active TCP connections that Tomcat's NIO Poller can manage concurrently (default: 8,192). Once reached, the server stops accepting new connections from the OS listen queue.
- `accept-count`: The operating system's TCP listen backlog queue length (default: 100). When `max-connections` is saturated, incoming TCP connections wait in this OS kernel queue until connections free up. If this queue overflows, incoming connections are dropped or rejected with `Connection refused`.
</details>

### Q2: Why does Tomcat keep-alive timeout need to be synchronized with upstream load balancers?
<details>
<summary>Click to view answer</summary>

Load balancers (e.g. AWS ALB, NGINX) maintain persistent keep-alive connections to backend servers. If Tomcat's `keep-alive-timeout` is shorter than the load balancer's idle timeout, Tomcat may send a TCP `FIN` packet to close the socket at the exact moment the load balancer routes a new user request onto that connection. This causes an HTTP 502 Bad Gateway error. Setting Tomcat's keep-alive timeout 5 seconds longer than the load balancer prevents this race condition.
</details>

### Q3: What is the impact of Nagle's algorithm on microservices, and how is it disabled?
<details>
<summary>Click to view answer</summary>

Nagle's algorithm buffers small TCP packets until a full-sized packet (MTU $\approx 1500$ bytes) is accumulated or an ACK is received from the remote peer. In REST/JSON microservices exchanging small payloads, this introduces artificial 40ms latency delays. It is disabled by setting `tcpNoDelay: true` on the connector, instructing the OS to transmit packets immediately.
</details>

### Q4: How does Reactor Netty's EventLoop architecture differ from Tomcat's thread pool?
<details>
<summary>Click to view answer</summary>

Tomcat (without Virtual Threads) assigns a dedicated worker thread to each incoming HTTP request for the duration of its execution ("thread-per-request").  
Reactor Netty runs on an event loop architecture where a small fixed number of threads (typically 1 per CPU core) handle all network I/O. Connections are registered with non-blocking selectors (`epoll`). When I/O events occur, the event loop thread dispatches the event and moves on without blocking.
</details>

### Q5: What happens if you execute a blocking call inside a Spring WebFlux controller?
<details>
<summary>Click to view answer</summary>

In Spring WebFlux, the controller method executes on one of the few Netty event loop threads (e.g. one of 8 threads on an 8-core CPU). If a method calls `Thread.sleep()` or a blocking JDBC query, that event loop thread is frozen. All other thousands of concurrent connections assigned to that event loop thread stall completely, causing catastrophic system-wide latency.
</details>

### Q6: How does HTTP/2 improve performance over HTTP/1.1 in Spring Boot?
<details>
<summary>Click to view answer</summary>

HTTP/1.1 requires a separate TCP connection for every parallel request or suffers from head-of-line blocking. HTTP/2 introduces **binary framing** and **stream multiplexing**, allowing hundreds of concurrent bidirectional requests and responses to be interleaved across a single persistent TCP connection. It also features HPACK header compression, reducing header bandwidth overhead.
</details>

### Q7: What metric in Micrometer indicates Tomcat thread pool saturation?
<details>
<summary>Click to view answer</summary>

Compare `tomcat.threads.busy` against `tomcat.threads.config.max`. When `tomcat.threads.busy` equals `tomcat.threads.config.max` (e.g. 200/200), all worker threads are occupied. Check `tomcat.threads.current` to see how many threads have been spawned by the pool.
</details>

### Q8: What is Netty's `PooledByteBufAllocator` and how can it cause memory leaks?
<details>
<summary>Click to view answer</summary>

Netty allocates off-heap direct memory buffers (`ByteBuf`) using a jemalloc-style memory pool to avoid JVM garbage collection overhead during high-speed network I/O. If application code or a custom filter allocates a `ByteBuf` but fails to invoke `ReferenceCountUtil.release()`, the native memory block is never returned to the pool, resulting in a native direct memory leak until the container process crashes.
</details>

---

## 11. Practice Exercises

### Exercise 1: Configure Production Tomcat Connector in application.yml (Easy)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Configure Tomcat in `application.yml` with a max of 300 threads, 30 min-spare threads, 10,000 max connections, and 2,000 accept count.

**Solution**:
```yaml
server:
  tomcat:
    threads:
      max: 300
      min-spare: 30
    max-connections: 10000
    accept-count: 2000
```
</details>

### Exercise 2: Programmatically Enable GZIP Compression for JSON (Medium)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Write a `WebServerFactoryCustomizer` bean that enables GZIP compression for all responses larger than 2KB with MIME type `application/json`.

**Solution**:
```java
package com.example.performance.server;

import org.springframework.boot.web.embedded.tomcat.TomcatServletWebServerFactory;
import org.springframework.boot.web.server.Compression;
import org.springframework.boot.web.server.WebServerFactoryCustomizer;
import org.springframework.stereotype.Component;
import org.springframework.util.unit.DataSize;

@Component
public class JsonCompressionCustomizer 
    implements WebServerFactoryCustomizer<TomcatServletWebServerFactory> {

    @Override
    public void customize(TomcatServletWebServerFactory factory) {
        Compression compression = new Compression();
        compression.setEnabled(true);
        compression.setMinResponseSize(DataSize.ofKilobytes(2));
        compression.setMimeTypes(new String[]{"application/json"});
        factory.setCompression(compression);
    }
}
```
</details>

### Exercise 3: Safe Offloading of Blocking Work in Spring WebFlux (Hard)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: In a Spring WebFlux controller, safely execute a legacy blocking database query without stalling the Netty event loop using `Schedulers.boundedElastic()`.

**Solution**:
```java
package com.example.performance.server;

import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PathVariable;
import org.springframework.web.bind.annotation.RestController;
import reactor.core.publisher.Mono;
import reactor.core.scheduler.Schedulers;

@RestController
public class ReactiveLegacyBridgeController {

    @GetMapping("/reactive/orders/{id}")
    public Mono<String> getOrderData(@PathVariable String id) {
        // Mono.fromCallable executes on boundedElastic thread pool,
        // leaving the Netty event loop completely free to handle other traffic!
        return Mono.fromCallable(() -> executeLegacyBlockingQuery(id))
            .subscribeOn(Schedulers.boundedElastic());
    }

    private String executeLegacyBlockingQuery(String id) throws InterruptedException {
        // Simulates 200ms blocking JDBC call
        Thread.sleep(200);
        return "OrderDetails-" + id;
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------+-------------------------------------------------------------+
| Property / Setting                          | Recommended Production Value & Description                  |
+---------------------------------------------+-------------------------------------------------------------+
| server.tomcat.threads.max                   | 200 (Default; switch to Virtual Threads in Boot 3.2+)       |
| server.tomcat.threads.min-spare             | 20 - 50 (Keeps pool pre-warmed for traffic spikes)          |
| server.tomcat.max-connections               | 8192 - 20000 (Max concurrent open TCP sockets)              |
| server.tomcat.accept-count                  | 1000 (OS listen backlog queue length)                       |
| server.tomcat.keep-alive-timeout            | 65000 (Must be 5s longer than upstream Load Balancer)       |
| server.http2.enabled                        | true (Enables stream multiplexing and binary framing)       |
| server.compression.enabled                  | true (Enables GZIP for responses > 1KB)                      |
| Schedulers.boundedElastic()                 | Mandatory when bridging blocking I/O inside WebFlux/Netty    |
+---------------------------------------------+-------------------------------------------------------------+
```

---

## 13. Further Reading
- Apache Tomcat 10 Configuration Reference: The HTTP NIO Connector
- Project Reactor Netty Reference Guide: EventLoop Architecture
- Spring Boot Documentation: Customizing Embedded Servlet Containers
- High Performance Browser Networking: Optimizing TCP & HTTP/2