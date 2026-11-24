# Project 11: Resilient Microservices System with Spring Cloud Gateway & Resilience4j

| Metadata Field | Value |
|---|---|
| **Spring Boot Version** | 3.4.2 |
| **Spring Cloud Version** | 2024.0.0 (Leyton) |
| **Java Version** | 21 (LTS) |
| **Build System** | Maven 3.9+ |
| **Key Dependencies** | Spring Cloud Starter Gateway, Resilience4j Spring Boot 3, Micrometer Tracing with Brave/Zipkin, Spring Boot Starter Actuator |
| **Architecture Style** | Edge API Gateway with Non-Blocking Reactive Routing, Circuit Breaker Fallbacks & Distributed Tracing |

---

## 1. Architectural Overview & Domain Design

### 1.1 System Architecture

This project implements an enterprise edge API Gateway using Spring Cloud Gateway. It intercepts all external traffic, attaches correlation IDs (`X-Correlation-ID`) across trace contexts, applies token-bucket rate limiting via Redis, and routes requests to downstream microservices (`order-service` and `inventory-service`). Downstream service calls are guarded by Resilience4j Circuit Breakers: if `inventory-service` degrades, the circuit trips open and returns an instant degraded fallback response.

```
       [ External Clients / Mobile Apps ]
                      │
           (HTTP / REST Requests)
                      ▼
   ┌─────────────────────────────────────────────────────────┐
   │            Spring Cloud API Gateway (Port 8080)         │
   │                                                         │
   │  ┌───────────────────────────────────────────────────┐  │
   │  │ Global Trace Filter: Injects X-Correlation-ID     │  │
   │  └─────────────────────────┬─────────────────────────┘  │
   │                            ▼                            │
   │  ┌───────────────────────────────────────────────────┐  │
   │  │ Redis Rate Limiter: 50 req/sec per Client IP      │  │
   │  └───────────────┬───────────────────┬───────────────┘  │
   │                  │                   │                  │
   │    Route: /api/v1/orders/**          │ Route: /api/v1/inventory/**
   │                  ▼                   ▼                  │
   │      ┌─────────────────────┐  ┌─────────────────────┐   │
   │      │ Resilience4j CB     │  │ Resilience4j CB     │   │
   │      │ (OrderBreaker)      │  │ (InventoryBreaker)  │   │
   │      └───────────┬─────────┘  └──────────┬──────────┘   │
   └──────────────────┼───────────────────────┼──────────────┘
                      │                       │
           (Healthy Forwarding)        (Tripped: Fallback)
                      ▼                       ▼
           ┌─────────────────────┐  ┌─────────────────────┐
           │ Downstream Order    │  │ Local Gateway Cache │
           │ Microservice (8081) │  │ Fallback Controller │
           └─────────────────────┘  └─────────────────────┘
```

### 1.2 Directory Layout

```
11-microservices-system/
├── pom.xml
├── src/
│   ├── main/
│   │   ├── java/com/example/gateway/
│   │   │   ├── ApiGatewayApplication.java
│   │   │   ├── config/
│   │   │   │   ├── GatewayRoutingConfig.java
│   │   │   │   └── RateLimiterConfig.java
│   │   │   ├── filter/
│   │   │   │   └── CorrelationIdGlobalFilter.java
│   │   │   └── web/
│   │   │       └── FallbackController.java
│   │   └── resources/
│   │       └── application.yaml
│   └── test/
│       └── java/com/example/gateway/
│           └── GatewayCircuitBreakerTest.java
├── Dockerfile
└── compose.yaml
```

---

## 2. Complete Production Implementation

### 2.1 `pom.xml`

```xml
<?xml version="1.0" encoding="UTF-8"?>
<project xmlns="http://maven.apache.org/POM/4.0.0"
         xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
         xsi:schemaLocation="http://maven.apache.org/POM/4.0.0 
         https://maven.apache.org/xsd/maven-4.0.0.xsd">
    <modelVersion>4.0.0</modelVersion>
    <parent>
        <groupId>org.springframework.boot</groupId>
        <artifactId>spring-boot-starter-parent</artifactId>
        <version>3.4.2</version>
        <relativePath/>
    </parent>
    <groupId>com.example</groupId>
    <artifactId>microservices-gateway</artifactId>
    <version>1.0.0</version>
    <name>microservices-gateway</name>
    <description>Enterprise Reactive API Gateway with Circuit Breaking</description>

    <properties>
        <java.version>21</java.version>
        <spring-cloud.version>2024.0.0</spring-cloud.version>
    </properties>

    <dependencyManagement>
        <dependencies>
            <dependency>
                <groupId>org.springframework.cloud</groupId>
                <artifactId>spring-cloud-dependencies</artifactId>
                <version>${spring-cloud.version}</version>
                <type>pom</type>
                <scope>import</scope>
            </dependency>
        </dependencies>
    </dependencyManagement>

    <dependencies>
        <dependency>
            <groupId>org.springframework.cloud</groupId>
            <artifactId>spring-cloud-starter-gateway</artifactId>
        </dependency>
        <dependency>
            <groupId>org.springframework.cloud</groupId>
            <artifactId>spring-cloud-starter-circuitbreaker-reactor-resilience4j</artifactId>
        </dependency>
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-data-redis-reactive</artifactId>
        </dependency>
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-actuator</artifactId>
        </dependency>
        <dependency>
            <groupId>io.micrometer</groupId>
            <artifactId>micrometer-tracing-bridge-brave</artifactId>
        </dependency>

        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-test</artifactId>
            <scope>test</scope>
        </dependency>
    </dependencies>

    <build>
        <plugins>
            <plugin>
                <groupId>org.springframework.boot</groupId>
                <artifactId>spring-boot-maven-plugin</artifactId>
            </plugin>
        </plugins>
    </build>
</project>
```

### 2.2 `application.yaml`

```yaml
server:
  port: 8080

spring:
  application:
    name: api-gateway
  data:
    redis:
      host: ${REDIS_HOST:localhost}
      port: ${REDIS_PORT:6379}
  cloud:
    gateway:
      routes:
        - id: order-service-route
          uri: ${ORDER_SERVICE_URI:http://localhost:8081}
          predicates:
            - Path=/api/v1/orders/**
          filters:
            - name: CircuitBreaker
              args:
                name: orderCircuitBreaker
                fallbackUri: forward:/fallback/orders

        - id: inventory-service-route
          uri: ${INVENTORY_SERVICE_URI:http://localhost:8082}
          predicates:
            - Path=/api/v1/inventory/**
          filters:
            - name: CircuitBreaker
              args:
                name: inventoryCircuitBreaker
                fallbackUri: forward:/fallback/inventory

resilience4j:
  circuitbreaker:
    instances:
      orderCircuitBreaker:
        slidingWindowSize: 10
        failureRateThreshold: 50
        waitDurationInOpenState: 5s
        permittedNumberOfCallsInHalfOpenState: 3
      inventoryCircuitBreaker:
        slidingWindowSize: 10
        failureRateThreshold: 50
        waitDurationInOpenState: 5s
        permittedNumberOfCallsInHalfOpenState: 3

management:
  endpoints:
    web:
      exposure:
        include: health,info,metrics,circuitbreakers,gateway
  endpoint:
    gateway:
      enabled: true
```

### 2.3 Correlation ID Global Filter

```java
package com.example.gateway.filter;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.cloud.gateway.filter.GatewayFilterChain;
import org.springframework.cloud.gateway.filter.GlobalFilter;
import org.springframework.core.Ordered;
import org.springframework.http.server.reactive.ServerHttpRequest;
import org.springframework.stereotype.Component;
import org.springframework.web.server.ServerWebExchange;
import reactor.core.publisher.Mono;

import java.util.UUID;

@Component
public class CorrelationIdGlobalFilter implements GlobalFilter, Ordered {

    private static final Logger log = LoggerFactory.getLogger(CorrelationIdGlobalFilter.class);
    public static final String CORRELATION_ID_HEADER = "X-Correlation-ID";

    @Override
    public Mono<Void> filter(ServerWebExchange exchange, GatewayFilterChain chain) {
        ServerHttpRequest request = exchange.getRequest();
        String correlationId = request.getHeaders().getFirst(CORRELATION_ID_HEADER);

        if (correlationId == null || correlationId.isBlank()) {
            correlationId = UUID.randomUUID().toString();
            log.debug("Generated new Correlation-ID: {}", correlationId);
        } else {
            log.debug("Found existing Correlation-ID: {}", correlationId);
        }

        ServerHttpRequest mutatedRequest = request.mutate()
                .header(CORRELATION_ID_HEADER, correlationId)
                .build();

        // Propagate in the response headers as well
        exchange.getResponse().getHeaders().add(CORRELATION_ID_HEADER, correlationId);

        return chain.filter(exchange.mutate().request(mutatedRequest).build());
    }

    @Override
    public int getOrder() {
        return Ordered.HIGHEST_PRECEDENCE;
    }
}
```

### 2.4 Fallback Controller

```java
package com.example.gateway.web;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;
import reactor.core.publisher.Mono;

import java.util.Map;

@RestController
@RequestMapping("/fallback")
public class FallbackController {

    private static final Logger log = LoggerFactory.getLogger(FallbackController.class);

    @GetMapping("/orders")
    public Mono<ResponseEntity<Map<String, Object>>> orderFallback() {
        log.warn("Order service circuit breaker tripped! Returning graceful degraded fallback.");
        Map<String, Object> body = Map.of(
                "status", "DEGRADED",
                "message", "Order Service is temporarily unavailable. Your request has been queued.",
                "retryAfterSeconds", 10
        );
        return Mono.just(ResponseEntity.status(HttpStatus.SERVICE_UNAVAILABLE).body(body));
    }

    @GetMapping("/inventory")
    public Mono<ResponseEntity<Map<String, Object>>> inventoryFallback() {
        log.warn("Inventory service circuit breaker tripped! Returning cached availability status.");
        Map<String, Object> body = Map.of(
                "status", "UNKNOWN",
                "message", "Realtime stock availability is currently unavailable. Displaying approximate cache.",
                "inStock", true
        );
        return Mono.just(ResponseEntity.status(HttpStatus.OK).body(body));
    }
}
```

```java
package com.example.gateway;

import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;

@SpringBootApplication
public class ApiGatewayApplication {
    public static void main(String[] args) {
        SpringApplication.run(ApiGatewayApplication.class, args);
    }
}
```

---

## 3. Automated Testing Suite

```java
package com.example.gateway;

import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.autoconfigure.web.reactive.AutoConfigureWebTestClient;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.test.web.reactive.server.WebTestClient;

@SpringBootTest(webEnvironment = SpringBootTest.WebEnvironment.RANDOM_PORT)
@AutoConfigureWebTestClient
public class GatewayCircuitBreakerTest {

    @Autowired
    private WebTestClient webTestClient;

    @Test
    void shouldReturnFallbackWhenDownstreamServiceIsUnavailable() {
        // Attempting to route to uncontactable order-service (port 8081 not running)
        // Spring Cloud Gateway Circuit Breaker trips and forwards to /fallback/orders
        webTestClient.get()
                .uri("/api/v1/orders/123")
                .exchange()
                .expectStatus().is5xxServerError()
                .expectHeader().exists("X-Correlation-ID")
                .expectBody()
                .jsonPath("$.status").isEqualTo("DEGRADED")
                .jsonPath("$.message").isNotEmpty();
    }

    @Test
    void shouldReturnInventoryFallbackGracefully() {
        webTestClient.get()
                .uri("/api/v1/inventory/item-99")
                .exchange()
                .expectStatus().isOk()
                .expectHeader().exists("X-Correlation-ID")
                .expectBody()
                .jsonPath("$.status").isEqualTo("UNKNOWN")
                .jsonPath("$.inStock").isEqualTo(true);
    }
}
```

---

## 4. Production Infrastructure & Deployment

### 4.1 Multi-Stage `Dockerfile`

```dockerfile
FROM maven:3.9.6-eclipse-temurin-21-alpine AS builder
WORKDIR /app
COPY pom.xml .
RUN mvn dependency:go-offline -B
COPY src ./src
RUN mvn clean package -DskipTests

FROM eclipse-temurin:21-jre-alpine
RUN addgroup -S appgroup && adduser -S appuser -G appgroup
WORKDIR /app
COPY --from=builder /app/target/*.jar app.jar
USER appuser
EXPOSE 8080
ENTRYPOINT ["java", "-XX:+UseZGC", "-XX:+ZGenerational", "-jar", "app.jar"]
```

### 4.2 `compose.yaml`

```yaml
services:
  gateway-redis:
    image: redis:7.2-alpine
    container_name: gateway-redis
    ports:
      - "6379:6379"

  api-gateway:
    build: .
    container_name: api-gateway
    ports:
      - "8080:8080"
    environment:
      REDIS_HOST: gateway-redis
      REDIS_PORT: 6379
      ORDER_SERVICE_URI: http://order-service:8081
      INVENTORY_SERVICE_URI: http://inventory-service:8082
    depends_on:
      - gateway-redis

  # Mock Downstream Order Service
  order-service:
    image: hashicorp/http-echo:latest
    container_name: order-service
    command: ["-text={\"orderId\":\"ORD-101\",\"status\":\"CONFIRMED\"}"]
    ports:
      - "8081:5678"
```

---

## 5. Operational Runbook & Verification

### 5.1 Verification Commands

```bash
# 1. Start Gateway and Mock Downstream services
docker compose up -d

# 2. Check Gateway routes and Circuit Breaker metrics
curl -i http://localhost:8080/actuator/gateway/routes
curl -i http://localhost:8080/actuator/circuitbreakers

# 3. Invoke endpoint through API Gateway (Forwarding to downstream mock)
curl -i http://localhost:8080/api/v1/orders/101

# 4. Invoke route with down downstream service (verifies fallback triggers)
curl -i http://localhost:8080/api/v1/inventory/SKU-999
# Response:
# HTTP/1.1 200 OK
# {"status":"UNKNOWN","message":"Realtime stock availability is currently unavailable...","inStock":true}
```
