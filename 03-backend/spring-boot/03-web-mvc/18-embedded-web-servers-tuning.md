# Embedded Web Servers Tuning in Spring Boot (Tomcat, Jetty, Undertow)

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | Servlet 6.0 Architecture, JVM Thread Pools, TCP Sockets & OS Networking |
| **Estimated Reading Time** | 25 minutes |
| **Status** | Production-Ready |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Baseline** | OpenJDK 21 LTS / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | Tomcat 10.1, Jetty 12, Undertow 2.3, OpenJDK 21.0.2 |

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
9. [Comparison: Tomcat vs Jetty vs Undertow](#9-comparison-tomcat-vs-jetty-vs-undertow)
10. [Interview Questions & Deep Dives](#10-interview-questions--deep-dives)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference](#12-summary--quick-reference)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

Spring Boot popularized the paradigm shift from deploying applications onto external standalone application servers (WAR files into standalone Tomcat/WebSphere) to packaging the application server directly inside the executable JAR.

### The Problem
Default server configurations are tuned for conservative, general-purpose local development, not high-throughput production:
1. Default thread pools (e.g. 200 platform threads) can saturate CPU or exhaust memory under traffic spikes.
2. Unbounded accept queues and connection limits lead to socket backlogs and hidden connection drops.
3. Keep-alive timeouts that are misaligned with upstream load balancers cause 502 Bad Gateway errors.

### The Solution: Embedded Web Server Tuning
Spring Boot abstracts embedded server creation via `ServletWebServerFactory` (e.g., `TomcatServletWebServerFactory`). Developers can configure thread pools, TCP keep-alives, HTTP/2, connection limits, and custom connectors declaratively via `application.yaml` or programmatically via `WebServerFactoryCustomizer`.

---

## 2. Core Concepts

### Mental Model: The Tomcat Embedded Socket Pipeline
```
               Client HTTP Connection Requests (TCP Port 8080)
                                      │
                                      ▼
                        OS Kernel Socket Backlog
                     (server.tomcat.accept-count = 100)
                     Queues incoming SYN handshakes
                                      │
                                      ▼
                      Tomcat NIO Connector Acceptor
                     (server.tomcat.threads.max-connections = 8192)
                     Maintains active open TCP sockets
                                      │
                                      ▼
                       Worker Thread Pool (Executor)
                     (server.tomcat.threads.max = 200)
                     [Worker-1] [Worker-2] ... [Worker-200]
                     OR Java 21 Virtual Thread Per Request
                                      │
                                      ▼
                       DispatcherServlet.doDispatch()
```

### Invariants
1. **Three-Tier Queue Architecture**: Connections flow from OS Kernel Backlog (`accept-count`) $\rightarrow$ Poller/Connection Pool (`max-connections`) $\rightarrow$ Worker Threads (`threads.max`).
2. **Keep-Alive Alignment Invariant**: The embedded server's keep-alive timeout MUST be strictly **greater than** the upstream reverse proxy's (ALB, NGINX, Cloudflare) idle timeout to prevent race-condition connection closures.
3. **Virtual Threads Override**: When `spring.threads.virtual.enabled=true`, Tomcat assigns an unbounded virtual thread per request, rendering `server.tomcat.threads.max` obsolete.

---

## 3. How It Works Internally

### The `ServletWebServerFactory` Lifecycle
1. During `ApplicationContext.refresh()`, Spring invokes `ServletWebServerApplicationContext.onRefresh()`.
2. It looks up a `ServletWebServerFactory` bean (default `TomcatServletWebServerFactory`).
3. It queries all registered `WebServerFactoryCustomizer<TomcatServletWebServerFactory>` beans in the container and applies property configurations.
4. It calls `factory.getWebServer(servletContextInitializer)`.
5. Tomcat's embedded engine (`StandardServer`, `StandardService`, `Connector`, `StandardEngine`, `StandardHost`, `StandardContext`) is programmatically instantiated, connectors are bound to the network port, and acceptor threads start listening.

---

## 4. Syntax & Configuration

### Comprehensive `application.yaml` Tomcat Tuning
```yaml
server:
  port: 8080
  shutdown: graceful # Give active requests 30s to finish before hard kill
  http2:
    enabled: true # Enable HTTP/2 multiplexing
  compression:
    enabled: true
    mime-types: text/html,text/xml,text/plain,text/css,text/javascript,application/javascript,application/json
    min-response-size: 2048 # Only compress payloads >= 2KB
  tomcat:
    threads:
      max: 200 # Maximum worker platform threads (ignored if virtual threads enabled)
      min-spare: 20 # Warm worker threads kept alive
    max-connections: 8192 # Maximum concurrent TCP sockets held open
    accept-count: 100 # OS socket backlog queue capacity
    connection-timeout: 20000ms # 20s socket timeout
    keep-alive-timeout: 65000ms # Must exceed upstream ALB/NGINX idle timeout (typically 60s)
    max-http-form-post-size: 2MB
```

### Swapping Embedded Web Server to Undertow or Jetty (`pom.xml`)
```xml
<!-- Exclude Tomcat and include Undertow for lightweight low-memory workloads -->
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-starter-web</artifactId>
    <exclusions>
        <exclusion>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-tomcat</artifactId>
        </exclusion>
    </exclusions>
</dependency>

<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-starter-undertow</artifactId>
</dependency>
```

---

## 5. Practical Examples

### Example 1: Programmatic Tomcat Customizer with Dual Ports (HTTP & HTTPS)

```java
package com.example.web.server;

import org.apache.catalina.connector.Connector;
import org.springframework.boot.web.embedded.tomcat.TomcatServletWebServerFactory;
import org.springframework.boot.web.server.WebServerFactoryCustomizer;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;

@Configuration(proxyBeanMethods = false)
public class DualPortTomcatServerConfig {

    /**
     * Primary port is HTTPS (configured in YAML).
     * This customizer adds a secondary HTTP connector on port 8080 that redirects to HTTPS.
     */
    @Bean
    public WebServerFactoryCustomizer<TomcatServletWebServerFactory> tomcatCustomizer() {
        return factory -> {
            Connector secondaryConnector = new Connector(TomcatServletWebServerFactory.DEFAULT_PROTOCOL);
            secondaryConnector.setPort(8080);
            secondaryConnector.setScheme("http");
            secondaryConnector.setSecure(false);
            factory.addAdditionalTomcatConnectors(secondaryConnector);
        };
    }
}
```

### Example 2: Enterprise Low-Level Tomcat APR / Native Tuning

```java
package com.example.web.server;

import org.apache.coyote.http11.AbstractHttp11Protocol;
import org.springframework.boot.web.embedded.tomcat.TomcatServletWebServerFactory;
import org.springframework.boot.web.server.WebServerFactoryCustomizer;
import org.springframework.stereotype.Component;

@Component
public class HighThroughputTomcatCustomizer implements WebServerFactoryCustomizer<TomcatServletWebServerFactory> {

    @Override
    public void customize(TomcatServletWebServerFactory factory) {
        factory.addConnectorCustomizers(connector -> {
            var protocol = connector.getProtocolHandler();
            if (protocol instanceof AbstractHttp11Protocol<?> httpProtocol) {
                // Keep-alive settings to prevent 502 errors behind AWS ALB (ALB default idle is 60s)
                httpProtocol.setKeepAliveTimeout(65_000); // 65 seconds
                httpProtocol.setMaxKeepAliveRequests(10_000);
                
                // TCP Socket buffer optimizations
                httpProtocol.setConnectionUploadTimeout(30_000);
                httpProtocol.setDisableUploadTimeout(false);
                
                // Max header size (protect against massive header cookies)
                httpProtocol.setMaxHeaderCount(100);
                httpProtocol.setMaxHttpRequestHeaderSize(16 * 1024); // 16KB
            }
        });
    }
}
```

### Example 3: Edge Case — Java 21 Virtual Threads (Project Loom) Server Tuning

```yaml
# application.yaml
spring:
  threads:
    virtual:
      enabled: true # Automatically configures Tomcat to use Virtual Threads!
```

```java
package com.example.web.server;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.boot.CommandLineRunner;
import org.springframework.stereotype.Component;

@Component
public class VirtualThreadServerProbe implements CommandLineRunner {

    private static final Logger log = LoggerFactory.getLogger(VirtualThreadServerProbe.class);

    @Override
    public void run(String... args) {
        log.info("Embedded Web Server initialized with Java 21 Virtual Threads: {}",
                Thread.ofVirtual().name("probe").unstarted(() -> {}).isVirtual());
    }
}
```

### Example 4: Anti-Pattern vs Production Fix

#### Anti-Pattern: Tomcat Keep-Alive Timeout Shorter than Load Balancer Idle Timeout
```yaml
# ANTI-PATTERN: Tomcat closes socket while AWS ALB thinks it's still open!
server:
  tomcat:
    keep-alive-timeout: 30000ms # 30s
# AWS ALB default idle timeout: 60s
# If a request arrives at ALB at T=30.001s, ALB forwards to Tomcat, which already closed socket!
# Result: Intermittent HTTP 502 Bad Gateway / Connection Reset errors under load!
```

#### Production Fix: Align Tomcat Keep-Alive Greater Than Upstream Reverse Proxy
```yaml
# PRODUCTION FIX: Tomcat keep-alive must exceed load balancer idle timeout by at least 5 seconds
server:
  tomcat:
    keep-alive-timeout: 65000ms # 65s (ALB is 60s)
```

### Example 5: High-Performance Undertow Non-Blocking Server Tuning

```yaml
# application.yaml for Undertow
server:
  undertow:
    threads:
      io: 16 # IO threads handling socket events (1 per CPU core)
      worker: 256 # Worker threads executing blocking tasks (8x IO threads)
    buffer-size: 16384 # 16KB buffer per direct memory allocation
    direct-buffers: true # Use off-heap native memory for zero-copy socket transfers
    max-http-post-size: 10485760 # 10MB
```

---

## 6. Best Practices

1. **Keep-Alive Alignment Invariant**: Ensure `server.tomcat.keep-alive-timeout` is at least 5 seconds longer than the idle timeout of upstream load balancers (AWS ALB, NGINX, Envoy).
2. **Enable Virtual Threads in Java 21+**: Set `spring.threads.virtual.enabled=true` to eliminate platform thread pool sizing bottlenecks.
3. **Enable GZIP / HTTP Compression for Large Text Payloads**: Set `server.compression.enabled=true` with a `min-response-size: 2048` threshold.
4. **Cap Max Connections & Accept Count**: Tune `max-connections` and `accept-count` based on container memory and CPU limits to fail fast instead of freezing under DoS attacks.
5. **Enable Graceful Shutdown**: Configure `server.shutdown: graceful` and `spring.lifecycle.timeout-per-shutdown-phase: 30s` to prevent dropping in-flight requests during deployments.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Keep-Alive Shorter than ALB Idle Timeout** | Triggers intermittent `502 Bad Gateway` errors due to race-condition socket closures. | Set Tomcat keep-alive to 65s+ (exceeding ALB's 60s). |
| **Unbounded Compression on Small Payloads** | Compressing small JSON payloads (<1KB) increases byte size and wastes CPU cycles. | Set `server.compression.min-response-size: 2048`. |
| **Worker Thread Pool Starvation** | Synchronous blocking calls exhaust 200 platform threads, causing connection timeouts. | Enable Java 21 virtual threads or tune thread pool limits. |
| **Misconfiguring Off-Heap Buffers in Undertow** | Undertow direct buffers consume native memory outside the JVM heap, causing container OOM kills. | Size container limits to include off-heap memory. |
| **Missing HTTP/2 Configuration** | High frontend multiplexing suffers head-of-line blocking on single HTTP/1.1 connections. | Set `server.http2.enabled: true`. |
| **Running as Root in Docker Container** | Binding to port 80 requires root privileges, violating container security best practices. | Run as non-root user and bind embedded server to port 8080. |

---

## 8. Performance & Security Considerations

### HTTP/2 Head-of-Line Blocking & TLS Overhead
Enabling HTTP/2 (`server.http2.enabled: true`) provides request multiplexing over a single TCP connection. However, HTTP/2 requires TLS in browser environments. In Kubernetes clusters with TLS termination at Ingress, keep plaintext HTTP/1.1 between Ingress and pods, or enable h2c (HTTP/2 cleartext) if latency is critical.

### Slowloris Attack Defense
Slowloris attacks open thousands of HTTP connections and send headers very slowly (1 byte every 10 seconds), holding worker threads hostage. Protect against this by tuning `server.tomcat.connection-timeout: 5000` (5s) and `max-header-count: 100`.

---

## 9. Comparison: Tomcat vs Jetty vs Undertow

| Feature | Apache Tomcat | Eclipse Jetty | Red Hat Undertow |
| :--- | :--- | :--- | :--- |
| **Default in Spring Boot?** | Yes | No | No |
| **Memory Footprint** | Moderate (~35MB baseline) | Low (~25MB baseline) | Lowest (~20MB baseline) |
| **Architecture** | Hybrid NIO / Worker Pool | Flexible Asynchronous NIO | Non-blocking NIO + Worker pool |
| **Virtual Threads Support** | Native (First-class in Boot 3.2+) | Native | Native |
| **WebSocket Support** | Excellent (JSR-356) | Best-in-Class (High scale) | Excellent |
| **Best Used For** | Standard enterprise applications | WebSocket-intensive apps | Microservices with tight RAM limits |

---

## 10. Interview Questions & Deep Dives

<details>
<summary><strong>Q1: What is the architectural difference between `max-connections`, `accept-count`, and `threads.max` in Tomcat?</strong></summary>

**Answer:**
1. **`accept-count`**: The operating system's TCP socket listen backlog queue. When all Tomcat connection slots are full, new incoming TCP SYN packets queue in this OS backlog. If this queue fills, the OS rejects connections with `Connection Refused`.
2. **`max-connections`**: The maximum number of active TCP connections Tomcat's NIO connector poller will accept and hold open simultaneously (default 8192).
3. **`threads.max`**: The maximum number of worker platform threads actively executing request processing logic (default 200). Idle connections wait in the poller until a worker thread becomes available.
</details>

<details>
<summary><strong>Q2: Why does an upstream AWS Application Load Balancer return intermittent 502 Bad Gateway errors if Tomcat's keep-alive timeout is misconfigured?</strong></summary>

**Answer:**
AWS ALB maintains persistent HTTP keep-alive connections to backend targets with a default idle timeout of 60 seconds. If Tomcat's `keep-alive-timeout` is shorter (e.g. 30 seconds), Tomcat closes the socket unilaterally when idle. If the ALB sends a new incoming client request at the exact moment Tomcat closes the TCP socket, the ALB receives a `TCP RST` packet from Tomcat. Because the ALB cannot know if the request was partially processed, it returns an HTTP 502 Bad Gateway to the user. Setting Tomcat's keep-alive timeout to 65s guarantees the ALB closes idle connections first.
</details>

<details>
<summary><strong>Q3: How does setting `spring.threads.virtual.enabled=true` change Tomcat's thread allocation model in Spring Boot 3.2+?</strong></summary>

**Answer:**
In standard mode, Tomcat allocates an OS platform thread from a bounded pool (`server.tomcat.threads.max=200`). When virtual threads are enabled, Spring Boot configures Tomcat's connector protocol with a `VirtualThreadExecutor` (`Executors.newVirtualThreadPerTaskExecutor()`). Every incoming HTTP request is assigned its own lightweight virtual thread. Because virtual threads unmount from carrier threads during blocking I/O, the pool is unbounded and concurrency is no longer limited by platform thread counts.
</details>

<details>
<summary><strong>Q4: How do you programmatically customize the embedded web server in Spring Boot?</strong></summary>

**Answer:**
Implement the `WebServerFactoryCustomizer<ConfigurableServletWebServerFactory>` interface (or server-specific variants like `WebServerFactoryCustomizer<TomcatServletWebServerFactory>`). Register it as a Spring `@Bean` or `@Component`. Spring Boot's web context discovers and applies it during server factory instantiation.
</details>

<details>
<summary><strong>Q5: What are the benefits of switching from Tomcat to Undertow in containerized environments?</strong></summary>

**Answer:**
Undertow has a significantly smaller memory footprint (often 10-15MB less heap at startup), supports high-performance direct off-heap buffer allocation, and uses an XNIO non-blocking model. For dense microservice deployments (e.g. running 50+ pods on a single Kubernetes node), the memory savings can be substantial.
</details>

<details>
<summary><strong>Q6: How does HTTP/2 server push work in Spring Boot, and why was it deprecated in modern web standards?</strong></summary>

**Answer:**
HTTP/2 Server Push allowed the server to preemptively send resources (CSS, JS) to the client cache before the client requested them. It was deprecated by Chrome and the IETF because it often wasted bandwidth by pushing assets already in the client cache and introduced complex race conditions. Modern architectures prefer `103 Early Hints` or standard preload links.
</details>

<details>
<summary><strong>Q7: What is the purpose of response compression (`server.compression.enabled=true`)?</strong></summary>

**Answer:**
It instructs the embedded server to compress outgoing responses using GZIP if the client sends `Accept-Encoding: gzip`. This reduces payload sizes across the network by 60-80% for text, JSON, and HTML, improving mobile load latency at the cost of slight server CPU compression overhead.
</details>

<details>
<summary><strong>Q8: How does graceful shutdown interact with the embedded web server?</strong></summary>

**Answer:**
When `server.shutdown=graceful` is set, upon receiving a SIGTERM signal, the embedded web server immediately pauses its connectors (refusing new socket connections) and waits up to `spring.lifecycle.timeout-per-shutdown-phase` for currently active HTTP requests to complete normally before tearing down the application context.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Enable HTTP Response Compression in YAML</strong></summary>

**Task**: Enable response compression for JSON and HTML payloads with a minimum size of 1024 bytes.

```yaml
# Solution:
server:
  compression:
    enabled: true
    min-response-size: 1024
    mime-types: application/json,text/html
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Configure Tomcat Keep-Alive Timeout</strong></summary>

**Task**: Configure `application.yaml` to set Tomcat's keep-alive timeout to 70 seconds and max connections to 5000.

```yaml
# Solution:
server:
  tomcat:
    keep-alive-timeout: 70000ms
    max-connections: 5000
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Custom WebServerFactoryCustomizer Bean</strong></summary>

**Task**: Write a `WebServerFactoryCustomizer` that sets the server's port to 8443 and disables the server header to prevent banner leakage.

```java
// Solution:
@Configuration(proxyBeanMethods = false)
public class ServerCustomizerConfig {
    @Bean
    public WebServerFactoryCustomizer<ConfigurableServletWebServerFactory> webServerCustomizer() {
        return factory -> {
            factory.setPort(8443);
            if (factory instanceof TomcatServletWebServerFactory tomcat) {
                tomcat.addConnectorCustomizers(connector -> {
                    connector.setProperty("server", ""); // Strips "Server: Apache-Coyote" header
                });
            }
        };
    }
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Switch Web Starter from Tomcat to Jetty</strong></summary>

**Task**: Show the Maven XML configuration to exclude `spring-boot-starter-tomcat` and include `spring-boot-starter-jetty`.

```xml
<!-- Solution: -->
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-starter-web</artifactId>
    <exclusions>
        <exclusion>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-tomcat</artifactId>
        </exclusion>
    </exclusions>
</dependency>
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-starter-jetty</artifactId>
</dependency>
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Access Log Valve Configuration with Masking</strong></summary>

**Task**: Programmatically configure an `AccessLogValve` on embedded Tomcat that logs request timings and client IPs, while regex-masking any sensitive query parameters like `token` or `password`.
</details>

<details>
<summary><strong>Exercise 6 (Hard): Embedded Server Native SSL / SNI Multi-Domain Host Setup</strong></summary>

**Task**: Configure an embedded Tomcat instance supporting multiple SSL/TLS certificates using Server Name Indication (SNI) across two distinct domains (`api.domain-a.com` and `api.domain-b.com`).
</details>

---

## 12. Summary & Quick Reference

- **Keep-Alive Rule**: Server keep-alive timeout > Upstream Load Balancer idle timeout.
- **Virtual Threads**: Enable via `spring.threads.virtual.enabled=true`.
- **Graceful Shutdown**: Set `server.shutdown: graceful`.
- **Customization Hook**: Implement `WebServerFactoryCustomizer<T>`.
- **Compression**: Enable with `server.compression.enabled=true` and set `min-response-size: 2048`.

---

## 13. Further Reading

- [Spring Boot Reference: Embedded Web Servers](https://docs.spring.io/spring-boot/reference/web/embedded-container.html)
- [Apache Tomcat 10.1 Configuration Reference: HTTP Connector](https://tomcat.apache.org/tomcat-10.1-doc/config/http.html)
- [Undertow Web Server Documentation](https://undertow.io/undertow-docs/undertow-docs-2.0.0/)
- [AWS Knowledge Center: Troubleshooting HTTP 502 Errors in Application Load Balancers](https://repost.aws/knowledge-center/elb-alb-troubleshoot-502-errors)
