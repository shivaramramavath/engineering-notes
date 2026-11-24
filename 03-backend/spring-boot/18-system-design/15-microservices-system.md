# 15: Designing an Enterprise Microservices System with Spring Boot & Cloud

| Metadata Field | Value |
|---|---|
| **Topic** | Enterprise Microservices Architecture, Saga Orchestration & Distributed Tracing |
| **Spring Boot Version** | 3.4.x |
| **Spring Cloud** | 2024.0.0 (Leyton) |
| **Java Version** | 21 / 25 LTS |
| **Key Middleware** | Spring Cloud Gateway, Kafka (Saga Broker), OpenTelemetry, Resilience4j |

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

A microservices architecture partitions a complex domain into independently deployable, loosely coupled services organized around business boundaries (e.g., Order Service, Payment Service, Inventory Service). 

While microservices enable independent team velocity and technology autonomy, they introduce distributed system challenges:
- **Network Boundaries & Latency**: Synchronous inter-service HTTP calls multiply latencies and cascade failures.
- **Distributed Data Consistency**: Without monolithic ACID transactions, cross-service business processes require the **Saga Pattern** with compensating actions.
- **Observability**: Debugging requires distributed context propagation (**W3C Trace Context**) across dozens of services via **OpenTelemetry** and **Micrometer Tracing**.

---

## 2. Core Concepts

### 2.1 The Saga Pattern for Distributed Transactions
Instead of blocking Two-Phase Commit (2PC):
- **Choreographed Saga**: Services react to Kafka events published by other services (e.g. `OrderCreated` $\to$ `InventoryReserved` $\to$ `PaymentBilled`). Best for simple 2–3 step flows.
- **Orchestrated Saga**: A centralized orchestrator (e.g., `OrderSagaManager`) directs participant services via explicit command messages. Best for complex workflows requiring clear compensation visibility.

### 2.2 API Gateway & Cross-Cutting Concerns
The API Gateway acts as the single entry point for external traffic:
- Authenticates JWT tokens and checks revocation.
- Injects `X-Correlation-ID` and OpenTelemetry trace headers.
- Applies Redis rate limits and circuit breaker fallbacks.

---

## 3. Internals & Architecture

```
        [ External Mobile / Web Clients ]
                        │
                        ▼
   ┌─────────────────────────────────────────┐
   │        Spring Cloud API Gateway         │
   │  - Route Predicates & Rate Limiting     │
   │  - OpenTelemetry Trace Injection        │
   └───────────┬─────────────────────────────┘
               │
               ▼
   ┌─────────────────────────────────────────┐
   │         Order Service (Saga)            │
   │  1. Create Order (status: PENDING)      │
   │  2. Publish "reserve-inventory-command" │
   └───────────┬─────────────────────────────┘
               │
               ▼
   ┌─────────────────────────────────────────┐
   │         Apache Kafka Broker             │
   │  Topic: "inventory-commands"            │
   └───────────┬─────────────────────────────┘
               │
               ▼
   ┌─────────────────────────────────────────┐
   │         Inventory Service               │
   │  3. Reserve Stock Items                 │
   │  4. Publish "inventory-reserved-event"  │
   └───────────┬─────────────────────────────┘
               │
               ▼
   ┌─────────────────────────────────────────┐
   │         Payment Service                 │
   │  5. Bill Credit Card                    │
   │     (If Failure: Compensate Inventory)  │
   └─────────────────────────────────────────┘
```

---

## 4. Syntax & Practical Usage

### 4.1 Micrometer Tracing Dependency
```xml
<dependency>
    <groupId>io.micrometer</groupId>
    <artifactId>micrometer-tracing-bridge-brave</artifactId>
</dependency>
<dependency>
    <groupId>io.zipkin.reporter2</groupId>
    <artifactId>zipkin-reporter-brave</artifactId>
</dependency>
```

---

## 5. 5 Practical Examples

### Example 1: Saga Orchestrator Command Dispatcher

```java
package com.example.microservices.saga;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.kafka.core.KafkaTemplate;
import org.springframework.stereotype.Service;

import java.math.BigDecimal;
import java.util.Map;
import java.util.UUID;

@Service
public class OrderSagaOrchestrator {

    private static final Logger log = LoggerFactory.getLogger(OrderSagaOrchestrator.class);
    private final KafkaTemplate<String, Object> kafkaTemplate;

    public OrderSagaOrchestrator(KafkaTemplate<String, Object> kafkaTemplate) {
        this.kafkaTemplate = kafkaTemplate;
    }

    public void startOrderSaga(UUID orderId, String sku, int qty, BigDecimal amount) {
        log.info("Starting Saga for Order {}. Step 1: Reserving Inventory...", orderId);

        Map<String, Object> command = Map.of(
                "orderId", orderId.toString(),
                "sku", sku,
                "quantity", qty,
                "amount", amount
        );

        kafkaTemplate.send("saga-inventory-commands", orderId.toString(), command);
    }

    public void handleInventoryFailed(UUID orderId, String reason) {
        log.warn("Saga Step 1 Failed for Order {}: {}. Triggering Compensation...", orderId, reason);
        // Mark order as CANCELLED in database
    }

    public void handleInventorySuccess(UUID orderId, BigDecimal amount) {
        log.info("Saga Step 1 Succeeded for Order {}. Step 2: Charging Payment...", orderId);
        kafkaTemplate.send("saga-payment-commands", orderId.toString(), Map.of(
                "orderId", orderId.toString(),
                "amount", amount
        ));
    }

    public void handlePaymentFailed(UUID orderId, String sku, int qty) {
        log.warn("Saga Step 2 (Payment) Failed for Order {}. Emitting Compensating Inventory Release...", orderId);
        kafkaTemplate.send("saga-inventory-compensate-commands", orderId.toString(), Map.of(
                "sku", sku,
                "quantity", qty
        ));
    }
}
```

### Example 2: Inventory Participant with Compensating Action

```java
package com.example.microservices.participant;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.kafka.annotation.KafkaListener;
import org.springframework.kafka.core.KafkaTemplate;
import org.springframework.stereotype.Component;

import java.util.Map;

@Component
public class InventorySagaParticipant {

    private static final Logger log = LoggerFactory.getLogger(InventorySagaParticipant.class);
    private final KafkaTemplate<String, Object> kafkaTemplate;

    public InventorySagaParticipant(KafkaTemplate<String, Object> kafkaTemplate) {
        this.kafkaTemplate = kafkaTemplate;
    }

    @KafkaListener(topics = "saga-inventory-commands", groupId = "inventory-saga-group")
    public void onReserveCommand(Map<String, Object> command) {
        String orderId = (String) command.get("orderId");
        String sku = (String) command.get("sku");
        int qty = (Integer) command.get("quantity");

        log.info("Inventory Participant: Reserving {} units of {}", qty, sku);

        // Execute inventory deduction in DB...
        boolean stockAvailable = true;

        if (stockAvailable) {
            kafkaTemplate.send("saga-events", orderId, Map.of(
                    "eventType", "INVENTORY_RESERVED",
                    "orderId", orderId
            ));
        } else {
            kafkaTemplate.send("saga-events", orderId, Map.of(
                    "eventType", "INVENTORY_RESERVATION_FAILED",
                    "orderId", orderId,
                    "reason", "Insufficient stock"
            ));
        }
    }

    @KafkaListener(topics = "saga-inventory-compensate-commands", groupId = "inventory-saga-group")
    public void onCompensateCommand(Map<String, Object> command) {
        String sku = (String) command.get("sku");
        int qty = (Integer) command.get("quantity");
        log.warn("COMPENSATION ACTION: Restoring {} units of SKU {} to inventory pool.", qty, sku);
        // Increment stock back up in DB
    }
}
```

### Example 3: Distributed Tracing & W3C Propagation Configuration

```yaml
# application.yaml
management:
  tracing:
    sampling:
      probability: 1.0 # 100% trace sampling for development/testing
    propagation:
      type: W3C
  zipkin:
    tracing:
      endpoint: http://localhost:9411/api/v2/spans
logging:
  pattern:
    level: "%5p [${spring.application.name:},%X{traceId:-},%X{spanId:-}]"
```

### Example 4: RestClient with Automatic Trace Propagation

```java
package com.example.microservices.config;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.web.client.RestClient;

@Configuration
public class TracedRestClientConfig {

    @Bean
    public RestClient restClient(RestClient.Builder builder) {
        // Spring Boot automatically configures RestClient.Builder with Micrometer tracing interceptors
        return builder.baseUrl("http://downstream-service").build();
    }
}
```

### Example 5: Spring Cloud Gateway Correlation Filter

```java
package com.example.microservices.gateway;

import org.springframework.cloud.gateway.filter.GatewayFilterChain;
import org.springframework.cloud.gateway.filter.GlobalFilter;
import org.springframework.core.Ordered;
import org.springframework.stereotype.Component;
import org.springframework.web.server.ServerWebExchange;
import reactor.core.publisher.Mono;

import java.util.UUID;

@Component
public class CorrelationIdFilter implements GlobalFilter, Ordered {

    public static final String CORRELATION_ID = "X-Correlation-ID";

    @Override
    public Mono<Void> filter(ServerWebExchange exchange, GatewayFilterChain chain) {
        String correlationId = exchange.getRequest().getHeaders().getFirst(CORRELATION_ID);
        if (correlationId == null || correlationId.isBlank()) {
            correlationId = UUID.randomUUID().toString();
        }

        var mutatedRequest = exchange.getRequest().mutate().header(CORRELATION_ID, correlationId).build();
        exchange.getResponse().getHeaders().add(CORRELATION_ID, correlationId);

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

- **Use Asynchronous Messaging for Inter-Service Workflows**: Decouple microservices using Kafka or RabbitMQ instead of synchronous chained REST calls ($A \to B \to C \to D$).
- **Mandate Compensating Actions in Sagas**: Every forward action (e.g. `reserveStock()`) must have an idempotent compensating counterpart (`releaseStock()`).
- **Enforce Single Database per Service**: Never share a single PostgreSQL database across multiple microservices; shared tables create tight coupling and block independent schema migrations.
- **Propagate W3C Trace Context**: Ensure all HTTP headers and Kafka records carry `traceparent` and `tracestate` headers so spans stitch together in Zipkin/Jaeger.
- **Contract Testing with Pact**: Use consumer-driven contract tests to verify API compatibility between microservices before deploying to staging.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Why It Fails | Correct Approach |
|---|---|---|
| **Shared Database Across Services** | Any schema change breaks multiple microservices; teams lose deployment independence. | Database-per-service pattern. |
| **Distributed 2PC (Two-Phase Commit)** | Blocking protocols reduce system availability to the lowest common denominator and destroy performance. | Use Sagas (Eventual Consistency) with compensating transactions. |
| **Deep Synchronous REST Call Chains** | If Service D latency increases to 2s, Services A, B, and C all stall and exhaust thread pools. | Decouple using asynchronous event streams. |
| **Missing Compensations in Sagas** | When a payment fails, inventory remains locked forever, draining available stock. | Implement automatic compensating rollback events. |
| **No Correlation ID Header** | Diagnosing an error across 20 services requires manual cross-referencing of timestamps. | Inject `X-Correlation-ID` at the API Gateway and propagate downstream. |
| **Service Granularity Too Fine ("Nanoservices")** | Hundreds of tiny single-function microservices create overwhelming network and operational overhead. | Organize services by bounded contexts (DDD); prefer Modular Monolith initially. |

---

## 8. Performance & Security Considerations

### 8.1 Mutual TLS (mTLS) with Service Mesh (Istio)
Encrypt all service-to-service communication within the Kubernetes cluster using automated mTLS via Istio or Linkerd sidecars.

### 8.2 Trace Sampling Configuration
At 100k QPS, capturing 100% of traces generates terabytes of telemetry data. Configure sampling probability:
```yaml
management:
  tracing:
    sampling:
      probability: 0.05 # Sample 5% of production requests
```

---

## 9. Comparison / Decision Matrix

| Dimension | Monolith | Modular Monolith | Microservices Architecture |
|---|---|---|---|
| **Deployment Independence**| None (All or nothing) | None (Single JAR) | Complete (Independent containers) |
| **Data Consistency** | ACID | ACID | Eventual Consistency (Sagas) |
| **Observability Overhead** | Low (Single log file) | Low (Single log file) | High (Distributed Tracing / OpenTelemetry)|
| **Network Overhead** | Zero (In-memory calls) | Zero (In-memory calls) | High (JSON/Protobuf over network) |

---

## 10. Real-World Interview Q&A

<details>
<summary><strong>1. How do you maintain data consistency across microservices without 2PC?</strong></summary>

Use the Saga Pattern. A business process executes a sequence of local transactions across services. Each step emits a domain event. If any step fails, the orchestrator triggers compensating transactions in reverse order to roll back previously committed actions.
</details>

<details>
<summary><strong>2. What is the difference between Choreographed and Orchestrated Sagas?</strong></summary>

In Choreography, participants listen to events and decide their next action without a central coordinator (ideal for simple flows). In Orchestration, a dedicated Saga Manager explicitly directs participants with command messages, maintaining state and handling failures centrally.
</details>

<details>
<summary><strong>3. How does distributed tracing work in Spring Boot 3.4?</strong></summary>

Micrometer Tracing intercepts inbound and outbound HTTP/Kafka communications, attaches a unique `traceId` and `spanId` conforming to the W3C Trace Context specification, and exports spans asynchronously to Zipkin, Jaeger, or OpenTelemetry Collector.
</details>

<details>
<summary><strong>4. Why is the "Database-per-Service" pattern mandatory in microservices?</strong></summary>

If multiple services share a database schema, independent deployment is impossible because a schema migration in Service A can break Service B. Bounding databases forces services to communicate strictly via published APIs.
</details>

<details>
<summary><strong>5. What is the role of an API Gateway in a microservice topology?</strong></summary>

It acts as the single entry point: terminating TLS, validating JWT credentials, injecting correlation IDs, enforcing rate limits, routing requests to internal service addresses, and aggregating data.
</details>

<details>
<summary><strong>6. What is a "Compensating Transaction"?</strong></summary>

A business operation that semantically reverses the effect of a previously committed local transaction (e.g., if `chargeCard()` fails, the compensating action is `unreserveInventory()`).
</details>

<details>
<summary><strong>7. How do you prevent cascading failures in a microservices network?</strong></summary>

Implement circuit breakers (Resilience4j), strict network read/connect timeouts, bulkheads to isolate resource pools, and graceful degraded fallbacks.
</details>

<details>
<summary><strong>8. What is Consumer-Driven Contract Testing (Pact)?</strong></summary>

A testing methodology where API consumers define expectations in contract files. The producer service verifies its endpoints against these contracts in CI/CD before deployment, preventing breaking API changes.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Implement Saga Compensating Consumer</strong></summary>

**Prompt**: Write a Kafka listener that restores stock when a `saga-compensate-inventory` event arrives.

**Solution**:
Inspect Example 2: listen on compensation topic, parse SKU and quantity, and increment inventory in database.
</details>

<details>
<summary><strong>Exercise 2: Configure Micrometer Tracing with Zipkin</strong></summary>

**Prompt**: Configure `application.yaml` to export traces to Zipkin on port 9411 with 100% sampling.

**Solution**:
Inspect Example 3: configure `management.tracing.sampling.probability: 1.0` and `management.zipkin.tracing.endpoint`.
</details>

<details>
<summary><strong>Exercise 3: Implement Gateway Correlation ID Filter</strong></summary>

**Prompt**: Write a reactive `GlobalFilter` injecting `X-Correlation-ID` if missing from request headers.

**Solution**:
Inspect Example 5: generate UUID if header is null, mutate request, and add header to response.
</details>

<details>
<summary><strong>Exercise 4: Test Saga Rollback with Testcontainers Kafka</strong></summary>

**Prompt**: Simulate payment failure and verify the orchestrator emits a compensating event to Kafka.

**Solution**:
Trigger saga, mock payment failure, and assert consumer on `saga-inventory-compensate-commands` receives the rollback payload.
</details>

<details>
<summary><strong>Exercise 5: Add Trace Context to Logback Output</strong></summary>

**Prompt**: Configure Logback pattern to output `[appName,traceId,spanId]` on every log line.

**Solution**:
```xml
<pattern>%d{yyyy-MM-dd HH:mm:ss} %5p [${spring.application.name},%X{traceId:-},%X{spanId:-}] : %m%n</pattern>
```
</details>

<details>
<summary><strong>Exercise 6: Implement Deadlock-Free Saga State Machine</strong></summary>

**Prompt**: Create an `OrderSagaInstance` entity storing `orderId`, `currentStep`, and `sagaStatus` (`STARTED`, `COMPENSATING`, `COMPLETED`, `FAILED`).

**Solution**:
Define JPA entity tracking execution steps and transition state after each event acknowledgement.
</details>

---

## 12. Summary & Quick Reference Cheatsheet

```
┌────────────────────────────────────────────────────────────────────────┐
│                   MICROSERVICES ARCHITECTURE CHEATSHEET                │
├────────────────────────────────────────────────────────────────────────┤
│ 1. Data Isolation: Database-per-service; never share relational tables │
│ 2. Consistency: Saga Pattern (Orchestration or Choreography) via Kafka │
│ 3. Failover: Every forward action must have an idempotent compensation │
│ 4. Observability: Micrometer Tracing + OpenTelemetry W3C headers       │
│ 5. Gateway: Spring Cloud Gateway for auth, rate limiting & correlation │
│ 6. Testing: Consumer-driven contract testing with Pact                 │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 13. Further Reading & Official Resources

- [Spring Cloud Reference Documentation](https://docs.spring.io/spring-cloud/reference/)
- [Microservices Patterns by Chris Richardson](https://microservices.io/)
- [OpenTelemetry Official Documentation](https://opentelemetry.io/)
- [Pact: Consumer-Driven Contract Testing](https://docs.pact.io/)
