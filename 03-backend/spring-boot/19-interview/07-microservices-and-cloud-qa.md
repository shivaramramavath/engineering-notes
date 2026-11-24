# 07: Microservices & Spring Cloud Technical Interview Q&A

| Metadata Field | Value |
|---|---|
| **Topic** | Spring Cloud 2024.x, Spring Cloud Gateway, Resilience4j & Distributed Tracing |
| **Spring Boot Version** | 3.4.x |
| **Spring Cloud** | 2024.0.0 (Leyton) |
| **Java Version** | 21 / 25 LTS |
| **Target Level** | Senior / Lead / Staff Backend Engineer |
| **Key Focus Areas** | Spring Cloud Gateway, Resilience4j Circuit Breakers, Micrometer Tracing, K8s DNS Discovery |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [Internals & Architecture](#3-internals--architecture)
4. [Syntax & Practical Usage](#4-syntax--practical-usage)
5. [5 Practical Examples](#5-5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Pitfalls & Anti-Patterns](#7-common-pitfalls--anti-patterns)
8. [Performance & Security Considerations](#8-performance--security-considerations)
9. [Comparison / Decision Matrix](#9-comparison--decision-matrix)
10. [Real-World Interview Q&A](#10-real-world-interview-qa)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference Cheatsheet](#12-summary--quick-reference-cheatsheet)
13. [Further Reading & Official Resources](#13-further-reading--official-resources)

---

## 1. What & Why

Operating distributed microservices requires a suite of cloud-native patterns to manage network complexity: reverse proxy edge routing, fault tolerance, circuit breaking, distributed tracing, and configuration centralization.

In modern Spring Cloud ecosystems, legacy Netflix OSS components (Zuul, Hystrix, Ribbon, Eureka) have been completely replaced by modern, high-performance alternatives:
- **Spring Cloud Gateway** (Reactive non-blocking Netty routing)
- **Resilience4j** (Lightweight fault tolerance, circuit breakers, rate limiters)
- **Kubernetes Native Service Discovery & CoreDNS** (Replacing Eureka)
- **Micrometer Tracing with OpenTelemetry** (Replacing Spring Cloud Sleuth)

Staff-level candidates must demonstrate mastery over these modern tools and articulate how to build resilient, observable distributed microservice architectures.

---

## 2. Core Concepts

### 2.1 The Resilience4j Circuit Breaker State Machine
A circuit breaker protects downstream services from cascading collapse:
- **CLOSED**: Normal operation. Requests flow freely. If failure rate exceeds threshold (e.g. 50%), state transitions to **OPEN**.
- **OPEN**: Circuit is tripped. All requests fail immediately with `CallNotPermittedException` (or invoke a local fallback) without calling the downstream network service.
- **HALF_OPEN**: After a configured wait duration (e.g., 5 seconds), a trial number of probe calls (e.g., 3 calls) are allowed through. If successful, state resets to **CLOSED**; if any probe fails, it returns to **OPEN**.

### 2.2 Modern Service Discovery: Kubernetes CoreDNS
In Kubernetes environments, standalone discovery servers like Netflix Eureka are obsolete. Microservices invoke internal services using Kubernetes native DNS records (e.g. `http://order-service.production.svc.cluster.local:8080`), with kube-proxy and CoreDNS handling cluster-internal load balancing.

---

## 3. Internals & Architecture

```
        [ Inbound External API Request ]
                        │
                        ▼
   ┌─────────────────────────────────────────┐
   │        Spring Cloud Gateway (Netty)     │
   │  1. Match Route Predicate (Path / Host) │
   │  2. Apply Global Filter (Correlation ID)│
   │  3. Redis RateLimiter Filter            │
   └────────────────────┬────────────────────┘
                        │
                        ▼
   ┌─────────────────────────────────────────┐
   │        Resilience4j Circuit Breaker     │
   │  Is circuit OPEN or CLOSED?             │
   └──────────┬──────────────────────────────┘
              │
       ┌──────┴──────────────────────┐
       ▼ (CLOSED: Service Healthy)   ▼ (OPEN: Downstream Down)
 ┌───────────────────────────┐ ┌────────────────────────────┐
 │ Forward across Network    │ │ Instant Local Fallback     │
 │ (Order Service via K8s)   │ │ (Return Cached Data / 503) │
 └───────────────────────────┘ └────────────────────────────┘
```

---

## 4. Syntax & Practical Usage

### 4.1 Resilience4j Circuit Breaker Annotation
```java
@CircuitBreaker(name = "inventoryService", fallbackMethod = "inventoryFallback")
public StockDto checkStock(String sku) {
    return restClient.get().uri("/stock/{sku}", sku).retrieve().body(StockDto.class);
}

public StockDto inventoryFallback(String sku, Throwable t) {
    return new StockDto(sku, 0, false); // Graceful fallback
}
```

---

## 5. 5 Practical Examples

### Example 1: Declarative Spring Cloud Gateway Routing Configuration

```yaml
# application.yaml
server:
  port: 8080

spring:
  application:
    name: edge-api-gateway
  cloud:
    gateway:
      routes:
        - id: order-service-route
          uri: lb://order-service # Routed via Spring Cloud LoadBalancer / K8s
          predicates:
            - Path=/api/v1/orders/**
          filters:
            - name: CircuitBreaker
              args:
                name: orderCircuitBreaker
                fallbackUri: forward:/fallback/orders
            - AddRequestHeader=X-Gateway-Origin, EdgeGateway

resilience4j:
  circuitbreaker:
    instances:
      orderCircuitBreaker:
        slidingWindowType: COUNT_BASED
        slidingWindowSize: 10
        minimumNumberOfCalls: 5
        failureRateThreshold: 50.0
        waitDurationInOpenState: 5000ms
        permittedNumberOfCallsInHalfOpenState: 3
```

### Example 2: Resilience4j Circuit Breaker & Retry in Spring Boot Service

```java
package com.example.cloud.service;

import io.github.resilience4j.circuitbreaker.annotation.CircuitBreaker;
import io.github.resilience4j.retry.annotation.Retry;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Service;
import org.springframework.web.client.RestClient;

@Service
public class InventoryClientService {

    private static final Logger log = LoggerFactory.getLogger(InventoryClientService.class);
    private final RestClient restClient;

    public InventoryClientService(RestClient.Builder builder) {
        this.restClient = builder.baseUrl("http://inventory-service").build();
    }

    public record InventoryResponse(String sku, boolean inStock) {}

    @Retry(name = "inventoryRetry")
    @CircuitBreaker(name = "inventoryBreaker", fallbackMethod = "cachedFallback")
    public InventoryResponse checkStock(String sku) {
        log.info("Querying inventory service for SKU: {}", sku);
        return restClient.get()
                .uri("/api/v1/inventory/{sku}", sku)
                .retrieve()
                .body(InventoryResponse.class);
    }

    public InventoryResponse cachedFallback(String sku, Throwable ex) {
        log.warn("Inventory service call failed for SKU {}: {}. Returning degraded fallback.", sku, ex.getMessage());
        return new InventoryResponse(sku, true); // Fallback: Assume in stock
    }
}
```

### Example 3: Resilience4j Configuration (`application.yaml`)

```yaml
resilience4j:
  circuitbreaker:
    instances:
      inventoryBreaker:
        slidingWindowSize: 20
        failureRateThreshold: 50
        waitDurationInOpenState: 10s
        permittedNumberOfCallsInHalfOpenState: 5
        automaticTransitionFromOpenToHalfOpenEnabled: true
  retry:
    instances:
      inventoryRetry:
        maxAttempts: 3
        waitDuration: 500ms
        enableExponentialBackoff: true
        exponentialBackoffMultiplier: 2.0
```

### Example 4: OpenTelemetry Distributed Tracing Propagation

```java
package com.example.cloud.tracing;

import io.micrometer.tracing.Tracer;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Service;

@Service
public class OrderProcessingBusinessService {

    private static final Logger log = LoggerFactory.getLogger(OrderProcessingBusinessService.class);
    private final Tracer tracer;

    public OrderProcessingBusinessService(Tracer tracer) {
        this.tracer = tracer;
    }

    public void executeOrderWorkflow(String orderId) {
        var currentSpan = tracer.currentSpan();
        String traceId = currentSpan != null ? currentSpan.context().traceId() : "NONE";

        log.info("Processing order {} within active W3C trace: {}", orderId, traceId);

        // Child span creation
        var newSpan = tracer.nextSpan().name("payment-calculation").start();
        try (var ws = tracer.withSpan(newSpan)) {
            log.info("Calculating payment fees inside dedicated child span...");
        } finally {
            newSpan.end();
        }
    }
}
```

### Example 5: Gateway Global Correlation Filter

```java
package com.example.cloud.gateway;

import org.springframework.cloud.gateway.filter.GatewayFilterChain;
import org.springframework.cloud.gateway.filter.GlobalFilter;
import org.springframework.core.Ordered;
import org.springframework.stereotype.Component;
import org.springframework.web.server.ServerWebExchange;
import reactor.core.publisher.Mono;

import java.util.UUID;

@Component
public class TraceCorrelationGlobalFilter implements GlobalFilter, Ordered {

    public static final String TRACE_HEADER = "X-Correlation-ID";

    @Override
    public Mono<Void> filter(ServerWebExchange exchange, GatewayFilterChain chain) {
        String correlationId = exchange.getRequest().getHeaders().getFirst(TRACE_HEADER);
        if (correlationId == null || correlationId.isBlank()) {
            correlationId = UUID.randomUUID().toString();
        }

        var mutatedRequest = exchange.getRequest().mutate().header(TRACE_HEADER, correlationId).build();
        exchange.getResponse().getHeaders().add(TRACE_HEADER, correlationId);

        return chain.filter(exchange.mutate().request(mutatedRequest).build());
    }

    @Override
    public int getOrder() {
        return Ordered.HIGHEST_PRECEDENCE;
    }
}
```

---

## 6. Best Practices

- **Ditch Netflix Hystrix & Ribbon**: Both projects are dead; use Resilience4j for circuit breaking and Spring Cloud LoadBalancer for client-side load balancing.
- **Use Kubernetes Native DNS**: Avoid running standalone Eureka clusters in Kubernetes. Rely on Kubernetes Services and CoreDNS for service discovery.
- **Enforce Connect & Read Timeouts**: A circuit breaker cannot protect you if timeouts are infinite. Always set connect timeout $\le 500\text{ms}$ and read timeout $\le 2\text{s}$.
- **Fallback Signatures Must Match**: Resilience4j fallback methods MUST have the exact same return type and arguments as the guarded method, plus an additional `Throwable` parameter at the end.
- **Trace Sampling in Production**: Set trace sampling probability between 1% and 5% (`management.tracing.sampling.probability: 0.05`) in high-volume production systems to avoid storage and network saturation.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Why It Fails | Correct Approach |
|---|---|---|
| **Using Netflix Hystrix in Boot 3** | Incompatible with Spring Boot 3 / Jakarta EE 10; completely abandoned. | Use Resilience4j Spring Boot 3 starter. |
| **Infinite HTTP Timeouts** | A slow downstream dependency causes all upstream worker threads to hang indefinitely. | Configure strict connect (500ms) and read (2s) timeouts on `RestClient`. |
| **Fallback Method Signature Mismatch** | Throws `NoSuchMethodException` at runtime when the circuit breaker trips. | Ensure fallback method matches original signature + `Throwable` parameter. |
| **Running Netflix Eureka in K8s** | Redundant operational overhead; duplicates Kubernetes' native CoreDNS service discovery. | Use Kubernetes DNS (`http://service-name:8080`). |
| **100% Trace Sampling at Scale** | Capturing all traces at 50,000 QPS generates gigabytes of network traffic to Zipkin/Jaeger. | Configure sampling probability to $0.05$ (5%). |
| **Blocking Code in Gateway Filters** | Calling blocking JDBC or synchronous APIs in Spring Cloud Gateway blocks Netty event loops. | Use reactive non-blocking operators (`Mono`/`Flux`) in Gateway filters. |

---

## 8. Performance & Security Considerations

### 8.1 Netty Event Loop Starvation
Spring Cloud Gateway runs on Netty. Never execute blocking I/O (e.g. `Thread.sleep()` or blocking JDBC) inside a Gateway filter, as it halts the shared Netty event loop and freezes the entire edge router.

### 8.2 W3C Trace Context Header Validation
Verify that inbound external `traceparent` headers are sanitized at the gateway to prevent trace injection attacks from untrusted external clients.

---

## 9. Comparison / Decision Matrix

| Dimension | Netflix OSS (Legacy) | Spring Cloud Modern (Boot 3) | Service Mesh (Istio / Envoy) |
|---|---|---|---|
| **Gateway** | Zuul 1 (Blocking) | Spring Cloud Gateway (Reactive) | Envoy Proxy |
| **Circuit Breaking** | Hystrix (Deprecated) | Resilience4j | Envoy Outlier Detection |
| **Service Discovery**| Eureka | Kubernetes CoreDNS | Istio Control Plane (Pilot) |
| **Tracing** | Spring Cloud Sleuth | Micrometer Tracing + OpenTelemetry| Envoy Jaeger Tracing |

---

## 10. Real-World Interview Q&A

<details>
<summary><strong>1. Explain the three states of a Resilience4j Circuit Breaker.</strong></summary>

CLOSED (normal operation; requests execute normally), OPEN (failure threshold exceeded; requests fail fast immediately without calling the downstream network dependency), and HALF_OPEN (after a wait duration, a configurable number of trial requests are sent through to test downstream health; if successful, resets to CLOSED; if failed, reverts to OPEN).
</details>

<details>
<summary><strong>2. Why was Spring Cloud Sleuth replaced by Micrometer Tracing in Spring Boot 3?</strong></summary>

Spring Boot 3 unified observability (metrics, tracing, logs) under the Micrometer project. Micrometer Tracing provides a vendor-neutral facade supporting OpenTelemetry and Brave standards, eliminating duplicate instrumentation libraries.
</details>

<details>
<summary><strong>3. Why should you NOT use Netflix Eureka when deploying Spring Boot to Kubernetes?</strong></summary>

Kubernetes natively provides built-in service discovery, health checking, and internal load balancing via CoreDNS and `kube-proxy`. Running Eureka on Kubernetes is redundant, adds unnecessary JVM memory overhead, and requires complex dual-registration synchronization.
</details>

<details>
<summary><strong>4. How does Spring Cloud Gateway differ from Netflix Zuul 1?</strong></summary>

Zuul 1 is blocking and servlet-based (one thread per connection), causing thread starvation under high concurrency. Spring Cloud Gateway is built on non-blocking Netty and Spring WebFlux, handling tens of thousands of concurrent connections on a handful of event loop threads.
</details>

<details>
<summary><strong>5. What is the difference between a Circuit Breaker and a Rate Limiter?</strong></summary>

A Circuit Breaker protects your system from failing downstream dependencies by failing fast when errors occur. A Rate Limiter protects your own system from being overwhelmed by incoming client traffic by enforcing throughput quotas.
</details>

<details>
<summary><strong>6. What happens if a Resilience4j fallback method does not match the original signature?</strong></summary>

Resilience4j fails to find the fallback method via reflection, throwing an internal `NoSuchMethodException` and bubbling the original downstream exception directly to the caller without executing fallback logic.
</details>

<details>
<summary><strong>7. How do you pass correlation IDs across microservices?</strong></summary>

The API Gateway generates an `X-Correlation-ID` header if not present. Downstream HTTP clients (`RestClient`, `WebClient`) and Kafka message producers automatically propagate the header into outbound network calls.
</details>

<details>
<summary><strong>8. What is a "Bulkhead" pattern in Resilience4j?</strong></summary>

A Bulkhead limits the number of concurrent executions allowed against a specific downstream service. If downstream Service A slows down, the bulkhead prevents it from consuming all available application threads and starving requests to Service B.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Configure Resilience4j Circuit Breaker with Fallback</strong></summary>

**Prompt**: Write a service method calling a remote API with `@CircuitBreaker` and provide a fallback method returning an empty list.

**Solution**:
Inspect Example 2: declare method with `@CircuitBreaker(name = "x", fallbackMethod = "fb")` and define matching `fb(..., Throwable t)`.
</details>

<details>
<summary><strong>Exercise 2: Configure Spring Cloud Gateway Route in YAML</strong></summary>

**Prompt**: Route all requests starting with `/api/v1/customers/**` to `http://customer-service:8080`.

**Solution**:
Inspect Example 1: define route ID, path predicate `Path=/api/v1/customers/**`, and URI `http://customer-service:8080`.
</details>

<details>
<summary><strong>Exercise 3: Test Circuit Breaker Tripping in JUnit</strong></summary>

**Prompt**: Write a test verifying that when a downstream service fails 5 times, the 6th call fails with `CallNotPermittedException`.

**Solution**:
Mock downstream client to throw `HttpServerErrorException`. Call 5 times to trip circuit. Call 6th time and assert `CallNotPermittedException` is thrown.
</details>

<details>
<summary><strong>Exercise 4: Propagate Custom Header in Gateway Filter</strong></summary>

**Prompt**: Write a reactive Gateway filter injecting `X-Tenant-ID: tenant_corp` on all outbound requests.

**Solution**:
Inspect Example 5: mutate request with `.header("X-Tenant-ID", "tenant_corp")`.
</details>

<details>
<summary><strong>Exercise 5: Configure Micrometer Tracing with 5% Sampling</strong></summary>

**Prompt**: Configure `application.yaml` to sample 5% of traces.

**Solution**:
```yaml
management:
  tracing:
    sampling:
      probability: 0.05
```
</details>

<details>
<summary><strong>Exercise 6: Implement Resilience4j Retry with Exponential Backoff</strong></summary>

**Prompt**: Configure `@Retry` to retry up to 3 times with 500ms initial delay and 2.0x multiplier.

**Solution**:
Inspect Example 3: define `maxAttempts: 3`, `waitDuration: 500ms`, `enableExponentialBackoff: true`, `exponentialBackoffMultiplier: 2.0`.
</details>

---

## 12. Summary & Quick Reference Cheatsheet

```
┌────────────────────────────────────────────────────────────────────────┐
│                   SPRING CLOUD & MICROSERVICES CHEATSHEET              │
├────────────────────────────────────────────────────────────────────────┤
│ 1. Gateway: Spring Cloud Gateway (Non-blocking Netty reactive engine)  │
│ 2. Fault Tolerance: Resilience4j CircuitBreaker, Retry, Bulkhead       │
│ 3. Discovery: Kubernetes CoreDNS native discovery (no Eureka in K8s)   │
│ 4. Observability: Micrometer Tracing + OpenTelemetry W3C headers       │
│ 5. Timeouts: Strict connect (500ms) & read (2s) on all RestClients     │
│ 6. Sampling: 1-5% trace sampling probability in high-volume production │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 13. Further Reading & Official Resources

- [Spring Cloud 2024 (Leyton) Reference Documentation](https://docs.spring.io/spring-cloud/reference/)
- [Resilience4j Official Documentation](https://resilience4j.readme.io/)
- [Micrometer Tracing Reference Guide](https://micrometer.io/docs/tracing)
- [Kubernetes DNS for Services and Pods](https://kubernetes.io/docs/concepts/services-networking/dns-pod-service/)
