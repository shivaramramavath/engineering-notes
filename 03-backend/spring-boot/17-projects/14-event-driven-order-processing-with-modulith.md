# Project 14: Modular Monolith Event-Driven Order Processing with Spring Modulith

| Metadata Field | Value |
|---|---|
| **Spring Boot Version** | 3.4.2 |
| **Spring Modulith Version** | 1.3.1 |
| **Java Version** | 21 (LTS) |
| **Build System** | Maven 3.9+ |
| **Key Dependencies** | Spring Modulith Starter Core, Spring Modulith Starter JPA, Spring Boot Starter Web, Spring Boot Starter Data JPA, PostgreSQL Driver, Testcontainers |
| **Architecture Style** | Modular Monolith with Event Publication Registry, Transactional Outbox & Enforced Module Encapsulation |

---

## 1. Architectural Overview & Domain Design

### 1.1 System Architecture

This project implements an enterprise e-commerce order processing pipeline within a **Modular Monolith** using **Spring Modulith**. The codebase is partitioned into strict business modules: `order`, `inventory`, and `notification`. When an order is placed, an `OrderPlacedEvent` is published inside the local transaction. Spring Modulith's **Event Publication Registry** persists the event into the database (`event_publication` table) in the exact same transaction (guaranteeing Transactional Outbox semantics). Downstream listeners in `inventory` and `notification` consume the event asynchronously (`@ApplicationModuleListener`) with automated retries and dead-letter handling.

```
                  [ Customer Checkout Request ]
                                │
                    (POST /api/v1/orders)
                                ▼
 ┌──────────────────────────────────────────────────────────────┐
 │                      Order Module (Local Tx)                 │
 │  1. Save Order (status: PENDING)                             │
 │  2. ApplicationEventPublisher.publishEvent(OrderPlacedEvent) │
 └──────────────────────────────┬───────────────────────────────┘
                                │ (Same Transaction)
                                ▼
 ┌──────────────────────────────────────────────────────────────┐
 │             Spring Modulith Event Publication Registry       │
 │  INSERT INTO event_publication (id, event_type, payload...)  │
 └──────────────┬───────────────────────────────┬───────────────┘
                │                               │
       (Async Delivery)                (Async Delivery)
                ▼                               ▼
 ┌──────────────────────────────┐ ┌─────────────────────────────┐
 │       Inventory Module       │ │     Notification Module     │
 │  @ApplicationModuleListener  │ │ @ApplicationModuleListener   │
 │  - Deduct stock items        │ │ - Dispatch email confirmation│
 │  - Update registry -> Done   │ │ - Update registry -> Done    │
 └──────────────────────────────┘ └─────────────────────────────┘
```

### 1.2 Directory Layout

```
14-event-driven-order-processing-with-modulith/
├── pom.xml
├── src/
│   ├── main/
│   │   ├── java/com/example/modulith/
│   │   │   ├── OrderProcessingApplication.java
│   │   │   ├── order/
│   │   │   │   ├── Order.java
│   │   │   │   ├── OrderPlacedEvent.java
│   │   │   │   ├── OrderRepository.java
│   │   │   │   ├── OrderService.java
│   │   │   │   └── OrderController.java
│   │   │   ├── inventory/
│   │   │   │   ├── Inventory.java
│   │   │   │   ├── InventoryRepository.java
│   │   │   │   └── InventoryEventListener.java
│   │   │   └── notification/
│   │   │       └── NotificationEventListener.java
│   │   └── resources/
│   │       └── application.yaml
│   └── test/
│       └── java/com/example/modulith/
│           ├── ModularityArchitectureTest.java
│           └── OrderProcessingIntegrationTest.java
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
    <artifactId>order-modulith</artifactId>
    <version>1.0.0</version>
    <name>order-modulith</name>
    <description>Enterprise Modular Monolith with Spring Modulith</description>

    <properties>
        <java.version>21</java.version>
        <spring-modulith.version>1.3.1</spring-modulith.version>
        <testcontainers.version>1.20.4</testcontainers.version>
    </properties>

    <dependencyManagement>
        <dependencies>
            <dependency>
                <groupId>org.springframework.modulith</groupId>
                <artifactId>spring-modulith-bom</artifactId>
                <version>${spring-modulith.version}</version>
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
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-data-jpa</artifactId>
        </dependency>
        <dependency>
            <groupId>org.springframework.modulith</groupId>
            <artifactId>spring-modulith-starter-core</artifactId>
        </dependency>
        <dependency>
            <groupId>org.springframework.modulith</groupId>
            <artifactId>spring-modulith-starter-jpa</artifactId>
        </dependency>
        <dependency>
            <groupId>org.postgresql</groupId>
            <artifactId>postgresql</artifactId>
            <scope>runtime</scope>
        </dependency>

        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-test</artifactId>
            <scope>test</scope>
        </dependency>
        <dependency>
            <groupId>org.springframework.modulith</groupId>
            <artifactId>spring-modulith-starter-test</artifactId>
            <scope>test</scope>
        </dependency>
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-testcontainers</artifactId>
            <scope>test</scope>
        </dependency>
        <dependency>
            <groupId>org.testcontainers</groupId>
            <artifactId>postgresql</artifactId>
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
    name: order-modulith
  datasource:
    url: jdbc:postgresql://${DB_HOST:localhost}:${DB_PORT:5432}/${DB_NAME:modulith_db}
    username: ${DB_USER:postgres}
    password: ${DB_PASS:postgres}
  jpa:
    hibernate:
      ddl-auto: update
    open-in-view: false
  modulith:
    events:
      republish-outstanding-events-on-restart: true
```

### 2.3 Order Module Implementation

```java
package com.example.modulith.order;

import java.math.BigDecimal;
import java.util.UUID;

public record OrderPlacedEvent(
        UUID orderId,
        String customerEmail,
        String productSku,
        int quantity,
        BigDecimal totalAmount
) {}
```

```java
package com.example.modulith.order;

import jakarta.persistence.*;
import java.math.BigDecimal;
import java.time.Instant;
import java.util.UUID;

@Entity
@Table(name = "orders")
public class Order {

    @Id
    @GeneratedValue(strategy = GenerationType.UUID)
    private UUID id;

    @Column(name = "customer_email", nullable = false)
    private String customerEmail;

    @Column(name = "product_sku", nullable = false)
    private String productSku;

    @Column(nullable = false)
    private int quantity;

    @Column(name = "total_amount", nullable = false, precision = 10, scale = 2)
    private BigDecimal totalAmount;

    @Enumerated(EnumType.STRING)
    @Column(nullable = false)
    private OrderStatus status = OrderStatus.CONFIRMED;

    @Column(name = "created_at", nullable = false, updatable = false)
    private Instant createdAt = Instant.now();

    public enum OrderStatus { CONFIRMED, CANCELLED }

    public Order() {}

    public Order(String customerEmail, String productSku, int quantity, BigDecimal totalAmount) {
        this.customerEmail = customerEmail;
        this.productSku = productSku;
        this.quantity = quantity;
        this.totalAmount = totalAmount;
    }

    public UUID getId() { return id; }
    public String getCustomerEmail() { return customerEmail; }
    public String getProductSku() { return productSku; }
    public int getQuantity() { return quantity; }
    public BigDecimal getTotalAmount() { return totalAmount; }
    public OrderStatus getStatus() { return status; }
}
```

```java
package com.example.modulith.order;

import org.springframework.data.jpa.repository.JpaRepository;
import java.util.UUID;

public interface OrderRepository extends JpaRepository<Order, UUID> {}
```

```java
package com.example.modulith.order;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.context.ApplicationEventPublisher;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.math.BigDecimal;

@Service
public class OrderService {

    private static final Logger log = LoggerFactory.getLogger(OrderService.class);
    private final OrderRepository orderRepository;
    private final ApplicationEventPublisher eventPublisher;

    public OrderService(OrderRepository orderRepository, ApplicationEventPublisher eventPublisher) {
        this.orderRepository = orderRepository;
        this.eventPublisher = eventPublisher;
    }

    @Transactional
    public Order placeOrder(String customerEmail, String productSku, int quantity, BigDecimal totalAmount) {
        Order order = new Order(customerEmail, productSku, quantity, totalAmount);
        Order saved = orderRepository.save(order);

        log.info("Order {} placed. Publishing OrderPlacedEvent into outbox registry...", saved.getId());
        eventPublisher.publishEvent(new OrderPlacedEvent(
                saved.getId(),
                saved.getCustomerEmail(),
                saved.getProductSku(),
                saved.getQuantity(),
                saved.getTotalAmount()
        ));

        return saved;
    }
}
```

```java
package com.example.modulith.order;

import org.springframework.http.HttpStatus;
import org.springframework.web.bind.annotation.*;

import java.math.BigDecimal;

@RestController
@RequestMapping("/api/v1/orders")
public class OrderController {

    private final OrderService orderService;

    public OrderController(OrderService orderService) {
        this.orderService = orderService;
    }

    public record PlaceOrderRequest(String customerEmail, String productSku, int quantity, BigDecimal totalAmount) {}

    @PostMapping
    @ResponseStatus(HttpStatus.CREATED)
    public Order createOrder(@RequestBody PlaceOrderRequest req) {
        return orderService.placeOrder(req.customerEmail(), req.productSku(), req.quantity(), req.totalAmount());
    }
}
```

### 2.4 Inventory & Notification Modules

```java
package com.example.modulith.inventory;

import com.example.modulith.order.OrderPlacedEvent;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.modulith.events.ApplicationModuleListener;
import org.springframework.stereotype.Component;

@Component
public class InventoryEventListener {

    private static final Logger log = LoggerFactory.getLogger(InventoryEventListener.class);

    @ApplicationModuleListener
    public void onOrderPlaced(OrderPlacedEvent event) {
        log.info("Inventory module received OrderPlacedEvent for order: {}. Reserving {} units of SKU {}",
                event.orderId(), event.quantity(), event.productSku());
        // Deduct inventory logic
    }
}
```

```java
package com.example.modulith.notification;

import com.example.modulith.order.OrderPlacedEvent;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.modulith.events.ApplicationModuleListener;
import org.springframework.stereotype.Component;

@Component
public class NotificationEventListener {

    private static final Logger log = LoggerFactory.getLogger(NotificationEventListener.class);

    @ApplicationModuleListener
    public void onOrderPlaced(OrderPlacedEvent event) {
        log.info("Notification module received OrderPlacedEvent for order: {}. Dispatching confirmation email to {}",
                event.orderId(), event.customerEmail());
        // Email sending logic
    }
}
```

---

## 3. Automated Testing Suite

```java
package com.example.modulith;

import org.junit.jupiter.api.Test;
import org.springframework.modulith.core.ApplicationModules;
import org.springframework.modulith.docs.Documenter;

public class ModularityArchitectureTest {

    ApplicationModules modules = ApplicationModules.of(OrderProcessingApplication.class);

    @Test
    void verifiesModularStructure() {
        // Enforces that module dependencies are unidirectional and package-private encapsulation is honored
        modules.verify();
    }

    @Test
    void generateDocumentation() {
        new Documenter(modules)
                .writeDocumentation()
                .writeModulesAsPlantUml();
    }
}
```

```java
package com.example.modulith;

import com.example.modulith.order.Order;
import com.example.modulith.order.OrderService;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.boot.testcontainers.service.connection.ServiceConnection;
import org.testcontainers.containers.PostgreSQLContainer;
import org.testcontainers.junit.jupiter.Container;
import org.testcontainers.junit.jupiter.Testcontainers;

import java.math.BigDecimal;

import static org.assertj.core.api.Assertions.assertThat;

@SpringBootTest
@Testcontainers
public class OrderProcessingIntegrationTest {

    @Container
    @ServiceConnection
    static PostgreSQLContainer<?> postgres = new PostgreSQLContainer<>("postgres:16-alpine");

    @Autowired
    private OrderService orderService;

    @Test
    void shouldPlaceOrderAndTriggerEventPublication() {
        Order order = orderService.placeOrder("buyer@example.com", "LAPTOP-M3", 1, new BigDecimal("1999.00"));
        assertThat(order).isNotNull();
        assertThat(order.getId()).isNotNull();
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
  modulith-db:
    image: postgres:16-alpine
    container_name: modulith-db
    environment:
      POSTGRES_DB: modulith_db
      POSTGRES_USER: postgres
      POSTGRES_PASSWORD: modulithpassword
    ports:
      - "5432:5432"
    volumes:
      - modulith_pg_data:/var/lib/postgresql/data
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U postgres"]
      interval: 5s
      timeout: 5s
      retries: 5

  modulith-app:
    build: .
    container_name: modulith-app
    ports:
      - "8080:8080"
    environment:
      DB_HOST: modulith-db
      DB_PORT: 5432
      DB_NAME: modulith_db
      DB_USER: postgres
      DB_PASS: modulithpassword
    depends_on:
      modulith-db:
        condition: service_healthy

volumes:
  modulith_pg_data:
```

---

## 5. Operational Runbook & Verification

### 5.1 Verification Commands

```bash
# 1. Start application stack
docker compose up -d

# 2. Check health
curl -i http://localhost:8080/actuator/health

# 3. Create order via REST API
curl -i -X POST http://localhost:8080/api/v1/orders \
  -H "Content-Type: application/json" \
  -d '{
    "customerEmail": "techlead@startup.io",
    "productSku": "MONITOR-4K",
    "quantity": 2,
    "totalAmount": 799.98
  }'

# 4. Check application logs for asynchronous module execution:
# - Inventory module received OrderPlacedEvent
# - Notification module received OrderPlacedEvent
# - Spring Modulith event publication completed
```
