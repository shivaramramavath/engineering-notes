# Microservices Architecture with Spring Boot: Decomposition and Communication

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x |
| **Java Version** | 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | `RestClient`, `spring-cloud-dependencies:2024.0.x` |

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
Monolithic architectures package an entire application into a single deployable artifact. As engineering teams grow to hundreds of developers, monoliths experience severe organizational and technical friction: code merges create conflicts, a bug in an auxiliary reporting module can bring down core checkout transactions, deploying a minor fix requires redeploying the entire system, and horizontal scaling is inefficient because the entire monolithic heap must be duplicated even if only a single calculation endpoint is under load.

### The Evolution
Microservices architecture decomposes a system into a suite of small, autonomous, independently deployable services organized around business capabilities (Domain-Driven Design). Each microservice owns its private data store (Database-per-Service) and exposes well-defined APIs. Spring Boot 3 provides an optimal runtime foundation for microservices—fast startup times, low memory footprints (especially with GraalVM Native Images or Java 21 Virtual Threads), and rich integration with the Spring Cloud ecosystem.

### Relevance in Modern Engineering
In distributed cloud engineering, microservices enable independent continuous deployment, team autonomy, and polyglot technology selection. However, microservices introduce distributed complexity: network latency, partial failures, distributed transactions, and data consistency challenges that must be engineered defensively.

---

## 2. Core Concepts

- **Bounded Context (DDD)**: An explicit boundary within which a domain model applies; each microservice maps to one (or a few closely aligned) Bounded Contexts.
- **Database-per-Service Pattern**: Strict architectural rule where each microservice owns its private database. Other services must access data via published APIs or event streams, never by querying foreign database tables directly.
- **Synchronous Communication (Request-Response)**: Client sends an HTTP/REST or gRPC request and blocks/waits for an immediate response. Best for query operations where the caller cannot proceed without the result.
- **Asynchronous Communication (Event-Driven)**: Service publishes an event to a message broker (Kafka, RabbitMQ) and continues execution without waiting for consumers. Best for state mutations and cross-boundary workflows.
- **Service Independence**: A microservice must be capable of building, testing, deploying, and scaling independently without requiring lockstep deployments of other services.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------+
|                        Microservice Domain Communication                          |
|                                                                                   |
|  +---------------------------+             +---------------------------+          |
|  | Order Service             |             | Customer Service          |          |
|  | (PostgreSQL: orders_db)   |             | (PostgreSQL: customers_db)|          |
|  |                           |             |                           |          |
|  | 1. Query Customer (Sync)  |             |                           |          |
|  |    GET /customers/{id}    | ----------> | Returns Customer DTO      |          |
|  |    (RestClient over HTTP) | <---------- | (Zero DB coupling)        |          |
|  |                           |             +---------------------------+          |
|  | 2. Order Placed           |                                                    |
|  |    Save to orders_db      |                                                    |
|  |    Publish Event (Async)  |                                                    |
|  +---------------------------+                                                    |
|                |                                                                  |
|                v Kafka Topic: "orders.events"                                     |
|  +-----------------------------------------------------------------------------+  |
|  | Apache Kafka Cluster / Event Bus                                            |  |
|  +-----------------------------------------------------------------------------+  |
|                |                                              |                   |
|                v Consumer Group 1                             v Consumer Group 2  |
|  +---------------------------+             +---------------------------+          |
|  | Payment Service           |             | Notification Service      |          |
|  | (MongoDB: payments_db)    |             | (Redis Cache)             |          |
|  +---------------------------+             +---------------------------+          |
+-----------------------------------------------------------------------------------+
```

When a business operation begins, the `Order Service` evaluates the order. If it needs customer credit status immediately, it performs a lightweight, synchronous HTTP call via `RestClient` to the `Customer Service`. Once the order is persisted locally in `orders_db`, it publishes an `OrderPlacedEvent` to Apache Kafka asynchronously. Independent downstream services (`Payment Service`, `Notification Service`) consume this event independently, updating their private data stores without coupling to the Order Service's runtime availability.

---

## 4. Syntax & Usage

### Maven Dependency Configuration
```xml
<dependencyManagement>
    <dependencies>
        <dependency>
            <groupId>org.springframework.cloud</groupId>
            <artifactId>spring-cloud-dependencies</artifactId>
            <version>2024.0.0</version>
            <type>pom</type>
            <scope>import</scope>
        </dependency>
    </dependencies>
</dependencyManagement>

<dependencies>
    <dependency>
        <groupId>org.springframework.boot</groupId>
        <artifactId>spring-boot-starter-web</artifactId>
    </dependency>
    <dependency>
        <groupId>org.springframework.kafka</groupId>
        <artifactId>spring-kafka</artifactId>
    </dependency>
</dependencies>
```

---

## 5. Practical Examples

### Example 1: Resilient Synchronous Inter-Service Client via `RestClient`
```java
package com.example.platform.client;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.http.HttpStatusCode;
import org.springframework.stereotype.Component;
import org.springframework.web.client.RestClient;

import java.time.Duration;

public record CustomerSummary(String id, String name, String tier, boolean active) {}

@Component
public class CustomerServiceClient {

    private static final Logger log = LoggerFactory.getLogger(CustomerServiceClient.class);
    private final RestClient restClient;

    public CustomerServiceClient(
            RestClient.Builder builder,
            @Value("${services.customer.url:http://customer-service:8081}") String baseUrl) {
        
        this.restClient = builder
                .baseUrl(baseUrl)
                .connectTimeout(Duration.ofSeconds(2))
                .readTimeout(Duration.ofSeconds(3))
                .build();
    }

    public CustomerSummary getCustomer(String customerId) {
        log.info("Fetching customer profile [{}] via synchronous HTTP", customerId);

        return restClient.get()
                .uri("/api/v1/customers/{id}", customerId)
                .retrieve()
                .onStatus(HttpStatusCode::is4xxClientError, (req, resp) -> {
                    throw new IllegalArgumentException("Customer not found: " + customerId);
                })
                .onStatus(HttpStatusCode::is5xxServerError, (req, resp) -> {
                    throw new IllegalStateException("Customer service downstream failure: " + resp.getStatusCode());
                })
                .body(CustomerSummary.class);
    }
}
```

### Example 2: Domain Service Balancing Sync Query and Async Event Mutation
```java
package com.example.platform.order.service;

import com.example.platform.client.CustomerServiceClient;
import com.example.platform.client.CustomerSummary;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.kafka.core.KafkaTemplate;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.util.UUID;

public record OrderPlacedEvent(String orderId, String customerId, double amount) {}

@Service
public class OrderProcessingService {

    private static final Logger log = LoggerFactory.getLogger(OrderProcessingService.class);
    private final CustomerServiceClient customerClient;
    private final KafkaTemplate<String, OrderPlacedEvent> kafkaTemplate;

    public OrderProcessingService(
            CustomerServiceClient customerClient,
            KafkaTemplate<String, OrderPlacedEvent> kafkaTemplate) {
        this.customerClient = customerClient;
        this.kafkaTemplate = kafkaTemplate;
    }

    @Transactional
    public String placeOrder(String customerId, double amount) {
        // 1. Synchronous validation: must verify customer is active before proceeding
        CustomerSummary customer = customerClient.getCustomer(customerId);
        if (!customer.active()) {
            throw new IllegalStateException("Cannot place order for suspended customer: " + customerId);
        }

        String orderId = UUID.randomUUID().toString();
        log.info("Persisting order [{}] for active customer [{}]", orderId, customer.name());

        // 2. Persist order in local DB...

        // 3. Asynchronous event publication: notify payment, fulfillment, inventory
        OrderPlacedEvent event = new OrderPlacedEvent(orderId, customerId, amount);
        kafkaTemplate.send("orders.v1", orderId, event);

        return orderId;
    }
}
```

### Example 3: Shared Database Anti-Pattern and Fix
```java
// ANTI-PATTERN: Service A directly querying Service B's database tables via foreign DataSource.
// Breaks data encapsulation, prevents Service B from altering its database schema,
// and creates tight runtime database coupling!
// @Autowired DataSource customerDatabase; // VIOLATION!

// FIX: Strictly encapsulate database behind Service B's REST/gRPC API or replicate via Kafka CDC
```

### Example 4: Long Synchronous Chains Anti-Pattern (Anti-Pattern Fix)
```java
// ANTI-PATTERN: Chained synchronous HTTP calls:
// OrderService -> calls -> PaymentService -> calls -> FraudService -> calls -> NotificationService
// If FraudService takes 3 seconds, all upstream HTTP threads stall, amplifying latency and risking failure cascades!

// FIX: Short-circuit sync calls to strictly essential validation; offload mutations to asynchronous events
```

### Example 5: High-Performance Resilient Fallback Cache
```java
package com.example.platform.client;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.cache.annotation.Cacheable;
import org.springframework.stereotype.Component;

@Component
public class CachingCustomerClient {

    private static final Logger log = LoggerFactory.getLogger(CachingCustomerClient.class);
    private final CustomerServiceClient delegateClient;

    public CachingCustomerClient(CustomerServiceClient delegateClient) {
        this.delegateClient = delegateClient;
    }

    // Cache customer tier data locally in L1 Caffeine to eliminate network round-trips for repeat calls
    @Cacheable(value = "customerSummaries", key = "#customerId", unless = "#result == null")
    public CustomerSummary getCustomerCached(String customerId) {
        log.info("Cache miss for customer [{}]. Calling remote service.", customerId);
        return delegateClient.getCustomer(customerId);
    }
}
```

---

## 6. Best Practices

1. **Enforce Database-per-Service**: Never share databases between microservices; shared tables create hidden coupling that prevents independent deployments.
2. **Favor Asynchronous Events for State Mutations**: Use Kafka/RabbitMQ for side-effects and cross-domain workflows; use synchronous REST/gRPC strictly for read queries.
3. **Set Strict HTTP Timeouts**: Always configure tight connection (1-2s) and read timeouts (2-3s) on `RestClient` to prevent slow downstream services from exhausting web thread pools.
4. **Design for Partial Failure**: Assume any downstream service can crash at any moment; implement circuit breakers (Resilience4j) and fallback caches.
5. **Standardize on Distributed Tracing**: Propagate W3C trace contexts (`traceparent`) across all HTTP headers and Kafka records via Micrometer Tracing.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Sharing a single database across multiple microservices | Schema migrations in Service A break Service B; tight coupling destroys independence | Enforce Database-per-Service; expose data via APIs or Kafka streams. |
| Deep synchronous call chains (A $\to$ B $\to$ C $\to$ D) | Multiplies latency and creates single points of failure (99% availability $\times 4 = 96\%$) | Decouple workflows into asynchronous event-driven steps. |
| Omitting timeouts on HTTP clients | Unresponsive downstream services cause upstream worker threads to hang, crashing the app | Configure `connectTimeout` and `readTimeout` on all `RestClient` builders. |
| Using 2-Phase Commit (XA/2PC) across microservices | Locks distributed database rows across network partitions, violating CAP theorem | Adopt the Saga Pattern with compensating transactions. |
| Decomposing into "Nano-services" (too granular) | Over-fragmentation introduces excessive network latency and operational complexity | Decompose along Domain-Driven Design Bounded Contexts. |
| Entity leaking across API boundaries | Exposing internal JPA entities forces consumers to adapt to internal schema changes | Map internal entities to clean, versioned DTO records. |

---

## 8. Performance & Security Notes

- **Network Latency Amplification**: In-memory method calls take nanoseconds; cross-service HTTP calls take 10-50ms. Minimize inter-service synchronous calls via local caching.
- **Mutual TLS (mTLS)**: Enforce mTLS across internal service-to-service communication to verify caller identity and encrypt traffic in transit.
- **Token Relay (OAuth2 Bearer)**: Propagate user JWT tokens across microservice hops via HTTP `Authorization: Bearer <token>` headers to preserve user security context.

---

## 9. Comparison / When to Use What

| Dimension | Microservices | Monolith (Modular Monolith) | Serverless (FaaS) |
|---|---|---|---|
| **Deployment** | Independent per service | Single deployable artifact | Ephemeral per-function |
| **Data Architecture** | Database-per-Service | Single shared database | Stateless / Cloud DB |
| **Team Structure** | Independent cross-functional teams | Single co-located team | Event-driven teams |
| **Operational Cost** | High (K8s, mesh, tracing, logs) | Low (Single JVM/DB) | Pay-per-invocation |
| **Best For** | Large scale, 50+ engineers | Early-stage, < 50 engineers | Bursty event tasks |

---

## 10. Interview Questions

<details>
<summary>1. What is the Database-per-Service pattern and why is it mandatory in microservices?</summary>
Database-per-Service dictates that each microservice owns its private database; no other service may read or write to that database directly. It is mandatory because sharing a database creates tight coupling at the schema level: altering a column in Service A can break Service B without warning, preventing teams from deploying services independently.
</details>

<details>
<summary>2. What is latency amplification in synchronous microservice chains?</summary>
If Service A calls Service B, which calls Service C, which calls Service D synchronously, the total response time is the sum of all network and processing latencies. Furthermore, availability decreases multiplicatively (e.g., four services with 99% availability yield an overall availability of $0.99^4 \approx 96\%$).
</details>

<details>
<summary>3. When should synchronous communication (REST/gRPC) be used versus asynchronous communication (Kafka)?</summary>
Synchronous communication is appropriate for read-only queries where the caller cannot proceed without the response (e.g., checking if a user exists). Asynchronous communication is preferred for state-changing commands and workflows (e.g., placing an order, sending a receipt), decoupling producers from consumer availability.
</details>

<details>
<summary>4. What is a Bounded Context in Domain-Driven Design?</summary>
A Bounded Context defines a specific linguistic and conceptual boundary within which a domain model applies consistently. For example, a "Product" in the Catalog Bounded Context represents descriptions, images, and marketing copy, while in the Shipping Bounded Context it represents dimensions, weight, and hazmat classifications.
</details>

<details>
<summary>5. How do you prevent thread exhaustion when calling slow external microservices?</summary>
By setting strict HTTP timeouts (connect and read timeouts), employing non-blocking reactive clients or Java 21 Virtual Threads, and wrapping calls with Resilience4j Circuit Breakers and Bulkheads to fail fast when services degrade.
</details>

<details>
<summary>6. How does distributed tracing solve observability in microservices?</summary>
In a distributed call graph, a single user request traverses multiple services. Distributed tracing injects a correlation identifier (`traceId`) and hop identifier (`spanId`) into HTTP headers (W3C `traceparent`). All participating services log and report telemetry with this ID, allowing developers to reconstruct the entire request path in tools like Zipkin or Jaeger.
</details>

<details>
<summary>7. What are the operational trade-offs of microservices compared to modular monoliths?</summary>
Microservices trade organizational scalability (independent team deployments) for operational complexity: distributed transactions (Sagas), eventual consistency, network latency, distributed security (mTLS/JWT relay), container orchestration (Kubernetes), and complex observability infrastructure.
</details>

<details>
<summary>8. How do you propagate security credentials across microservice boundaries?</summary>
Using the Token Relay pattern. When Service A calls Service B, it extracts the incoming JWT access token from the `SecurityContext` and injects it into the outbound HTTP request's `Authorization: Bearer <token>` header, allowing downstream services to validate the caller's identity and claims.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1 (Easy): Configure RestClient with strict timeouts</summary>
Write a Spring `@Bean` creating a `RestClient` with a 1-second connect timeout and 2-second read timeout.
</details>

<details>
<summary>Exercise 2 (Easy): Define a domain event record</summary>
Create an immutable Java record `OrderCreatedEvent` containing order ID, customer ID, and timestamp.
</details>

<details>
<summary>Exercise 3 (Medium): Implement inter-service synchronous client with error mapping</summary>
Implement a client fetching user profile details that maps HTTP 404 to `UserNotFoundException` and HTTP 500 to `ServiceUnavailableException`.
</details>

<details>
<summary>Exercise 4 (Medium): Build an asynchronous event-driven workflow</summary>
Write an `OrderService` that saves an order locally and publishes an event to Kafka, and an `InventoryService` that listens to the topic and reserves inventory.
</details>

<details>
<summary>Exercise 5 (Hard): Token Relay Interceptor</summary>
Write a `ClientHttpRequestInterceptor` that extracts the current Spring Security Bearer token from `SecurityContextHolder` and attaches it to outbound HTTP requests.
</details>

<details>
<summary>Exercise 6 (Hard): Integration test testing inter-service failure handling</summary>
Write a test using MockRestServiceServer that simulates a 504 Gateway Timeout from an external customer service and verifies that the calling service handles the exception cleanly.
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------+
|                       Microservices Architecture Summary                        |
+---------------------------------------------------------------------------------+
| Rule #1:            Database-per-Service (Never share databases across services)|
| Sync Calls:         RestClient / gRPC for read queries (Strict timeouts mandatory)|
| Async Calls:        Kafka / RabbitMQ for state mutations and domain events      |
| Dual-Write Hazard:  Use Transactional Outbox pattern to avoid lost events       |
| Transactions:       Saga Pattern with compensating transactions (No 2PC/XA)     |
| Observability:      W3C Traceparent headers propagated via Micrometer Tracing   |
| Decomposition Rule: Decompose along Domain-Driven Design Bounded Contexts       |
+---------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Building Microservices, 2nd Edition (Sam Newman)](https://www.oreilly.com/library/view/building-microservices-2nd/9781492034018/)
- [Microservices Patterns (Chris Richardson)](https://microservices.io/)
- [Domain-Driven Design (Eric Evans)](https://www.domainlanguage.com/ddd/)
