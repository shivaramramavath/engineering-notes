# Production Reference Architecture: High-Concurrency E-Commerce Backend

## Metadata & Production Baseline

| Attribute | Specification |
|:---|:---|
| **Project Code** | `SPRING-PROJ-004` |
| **Spring Boot Version** | `3.4.x` / `4.x` |
| **Spring Framework Version** | `6.2.x` / `7.0.x` |
| **Java Baseline** | `Java 21` / `Java 25 LTS` |
| **Build Tool** | `Maven 3.9+` |
| **Concurrency & Locks** | `PESSIMISTIC_WRITE` (PostgreSQL `FOR UPDATE`), Redis Distributed Locks |
| **Messaging & Events** | `Apache Kafka 3.7+`, Transactional Outbox Pattern |
| **Testing** | `JUnit 5.11+`, `Testcontainers 1.20+`, `PostgreSQL`, `Kafka` |
| **Complexity Level** | Advanced / Enterprise |

---

## 1. Architectural Overview & Domain Design

### 1.1 System Architecture & Data Flow

```
                      +---------------------------------------+
                      |         Web / Mobile Checkout         |
                      +-------------------+-------------------+
                                          |
                                          | 1. POST /api/v1/orders (Idempotency Key)
                                          v
                      +---------------------------------------+
                      |       OrderProcessingController       |
                      +-------------------+-------------------+
                                          |
                                          v
                      +---------------------------------------+
                      |  OrderPlacementService (@Transactional)|
                      +-------------------+-------------------+
                                          |
                          +---------------+---------------+
                          |                               |
                          v                               v
             +-------------------------+     +-------------------------+
             | Inventory Item (PESSIM- |     | Outbox Event Record     |
             | ISTIC_WRITE Lock held)  |     | (Atomic local DB write) |
             +------------+------------+     +------------+------------+
                          |                               |
                          v                               v
             +-------------------------+     +-------------------------+
             | PostgreSQL 16 Database  |     | Kafka Relay Worker      |
             | (Deadlock-Free Row Lock)|     | Streams to orders-topic |
             +-------------------------+     +-------------------------+
```

### 1.2 Directory Layout

```
04-ecommerce-backend/
|-- pom.xml
|-- Dockerfile
|-- compose.yaml
`-- src/
    |-- main/
    |   |-- java/com/example/ecommerce/
    |   |   |-- EcommerceApplication.java
    |   |   |-- domain/
    |   |   |   |-- Order.java
    |   |   |   |-- OrderItem.java
    |   |   |   `-- InventoryStock.java
    |   |   |-- dto/
    |   |   |   |-- PlaceOrderCommand.java
    |   |   |   `-- OrderConfirmationResponse.java
    |   |   |-- repository/
    |   |   |   |-- OrderRepository.java
    |   |   |   `-- InventoryStockRepository.java
    |   |   |-- service/
    |   |   |   `-- OrderPlacementService.java
    |   |   `-- web/
    |   |       `-- OrderController.java
    |   `-- resources/
    |       |-- application.yaml
    |       `-- db/migration/
    |           `-- V1__init_ecommerce_schema.sql
    `-- test/
        `-- java/com/example/ecommerce/
            `-- HighConcurrencyOrderPlacementTest.java
```

---

## 2. Complete Production Implementation

### 2.1 Build Configuration (`pom.xml`)

```xml
<?xml version="1.0" encoding="UTF-8"?>
<project xmlns="http://maven.apache.org/POM/4.0.0"
         xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
         xsi:schemaLocation="http://maven.apache.org/POM/4.0.0 https://maven.apache.org/xsd/maven-4.0.0.xsd">
    <modelVersion>4.0.0</modelVersion>

    <parent>
        <groupId>org.springframework.boot</groupId>
        <artifactId>spring-boot-starter-parent</artifactId>
        <version>3.4.1</version>
        <relativePath/>
    </parent>

    <groupId>com.example</groupId>
    <artifactId>ecommerce-backend</artifactId>
    <version>1.0.0</version>
    <name>ecommerce-backend</name>

    <properties>
        <java.version>21</java.version>
    </properties>

    <dependencies>
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-web</artifactId>
        </dependency>
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-validation</artifactId>
        </dependency>
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-data-jpa</artifactId>
        </dependency>
        <dependency>
            <groupId>org.springframework.kafka</groupId>
            <artifactId>spring-kafka</artifactId>
        </dependency>
        <dependency>
            <groupId>org.postgresql</groupId>
            <artifactId>postgresql</artifactId>
            <scope>runtime</scope>
        </dependency>
        <dependency>
            <groupId>org.flywaydb</groupId>
            <artifactId>flyway-core</artifactId>
        </dependency>
        <dependency>
            <groupId>org.flywaydb</groupId>
            <artifactId>flyway-database-postgresql</artifactId>
        </dependency>

        <!-- Testing -->
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-test</artifactId>
            <scope>test</scope>
        </dependency>
        <dependency>
            <groupId>org.testcontainers</groupId>
            <artifactId>postgresql</artifactId>
            <scope>test</scope>
        </dependency>
        <dependency>
            <groupId>org.testcontainers</groupId>
            <artifactId>kafka</artifactId>
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

### 2.2 Database Migration (`src/main/resources/db/migration/V1__init_ecommerce_schema.sql`)

```sql
CREATE TABLE IF NOT EXISTS inventory_stock (
    product_id VARCHAR(64) PRIMARY KEY,
    available_quantity INT NOT NULL CHECK (available_quantity >= 0),
    reserved_quantity INT NOT NULL DEFAULT 0,
    version BIGINT NOT NULL DEFAULT 0
);

CREATE TABLE IF NOT EXISTS orders (
    id UUID PRIMARY KEY,
    customer_id VARCHAR(64) NOT NULL,
    total_amount NUMERIC(12, 2) NOT NULL,
    status VARCHAR(32) NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE TABLE IF NOT EXISTS outbox_events (
    id UUID PRIMARY KEY,
    aggregate_type VARCHAR(64) NOT NULL,
    aggregate_id VARCHAR(64) NOT NULL,
    event_type VARCHAR(64) NOT NULL,
    payload JSONB NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX idx_orders_customer ON orders (customer_id);
```

### 2.3 Domain Models & Pessimistic Lock Repository

```java
package com.example.ecommerce.domain;

import jakarta.persistence.*;
import java.util.Objects;

@Entity
@Table(name = "inventory_stock")
public class InventoryStock {

    @Id
    @Column(name = "product_id", length = 64)
    private String productId;

    @Column(name = "available_quantity", nullable = false)
    private int availableQuantity;

    @Column(name = "reserved_quantity", nullable = false)
    private int reservedQuantity;

    @Version
    private Long version;

    protected InventoryStock() {}

    public InventoryStock(String productId, int initialStock) {
        this.productId = Objects.requireNonNull(productId);
        this.availableQuantity = initialStock;
        this.reservedQuantity = 0;
    }

    public void deduct(int count) {
        if (count <= 0) throw new IllegalArgumentException("Quantity must be positive");
        if (this.availableQuantity < count) {
            throw new IllegalStateException("Insufficient inventory for product: " + productId);
        }
        this.availableQuantity -= count;
        this.reservedQuantity += count;
    }

    public String getProductId() { return productId; }
    public int getAvailableQuantity() { return availableQuantity; }
}
```

```java
package com.example.ecommerce.repository;

import com.example.ecommerce.domain.InventoryStock;
import jakarta.persistence.LockModeType;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Lock;
import org.springframework.data.jpa.repository.Query;
import org.springframework.data.repository.query.Param;

import java.util.Optional;

public interface InventoryStockRepository extends JpaRepository<InventoryStock, String> {

    // Pessimistic Write Lock: Generates "SELECT ... FOR UPDATE" in PostgreSQL
    // Serializes concurrent purchases on the exact same product SKU without race conditions!
    @Lock(LockModeType.PESSIMISTIC_WRITE)
    @Query("SELECT i FROM InventoryStock i WHERE i.productId = :pid")
    Optional<InventoryStock> findByIdForUpdate(@Param("pid") String productId);
}
```

### 2.4 Transactional Placement Service with Outbox

```java
package com.example.ecommerce.service;

import com.example.ecommerce.domain.InventoryStock;
import com.example.ecommerce.domain.Order;
import com.example.ecommerce.dto.OrderConfirmationResponse;
import com.example.ecommerce.dto.PlaceOrderCommand;
import com.example.ecommerce.repository.InventoryStockRepository;
import com.example.ecommerce.repository.OrderRepository;
import com.fasterxml.jackson.databind.ObjectMapper;
import org.springframework.jdbc.core.simple.JdbcClient;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.math.BigDecimal;
import java.util.UUID;

@Service
public class OrderPlacementService {

    private final InventoryStockRepository inventoryRepo;
    private final OrderRepository orderRepo;
    private final JdbcClient jdbcClient;
    private final ObjectMapper mapper;

    public OrderPlacementService(InventoryStockRepository inventoryRepo, 
                                 OrderRepository orderRepo, 
                                 JdbcClient jdbcClient, 
                                 ObjectMapper mapper) {
        this.inventoryRepo = inventoryRepo;
        this.orderRepo = orderRepo;
        this.jdbcClient = jdbcClient;
        this.mapper = mapper;
    }

    @Transactional
    public OrderConfirmationResponse placeOrder(PlaceOrderCommand cmd) {
        // Step 1: Acquire Pessimistic Row Lock (FOR UPDATE)
        InventoryStock stock = inventoryRepo.findByIdForUpdate(cmd.productId())
                .orElseThrow(() -> new IllegalArgumentException("Product not found: " + cmd.productId()));

        // Step 2: Invariant Check & In-Memory Deduction
        stock.deduct(cmd.quantity());

        // Step 3: Insert Order
        UUID orderId = UUID.randomUUID();
        Order order = new Order(orderId, cmd.customerId(), cmd.totalAmount(), "CONFIRMED");
        orderRepo.save(order);

        // Step 4: Write Outbox Event inside the exact same DB transaction!
        writeOutboxEvent(orderId, cmd);

        return new OrderConfirmationResponse(orderId, "CONFIRMED", cmd.totalAmount());
    }

    private void writeOutboxEvent(UUID orderId, PlaceOrderCommand cmd) {
        try {
            String jsonPayload = mapper.writeValueAsString(cmd);
            jdbcClient.sql("""
                INSERT INTO outbox_events (id, aggregate_type, aggregate_id, event_type, payload)
                VALUES (:id, 'Order', :aggId, 'OrderPlacedEvent', :payload::jsonb)
                """)
                .param("id", UUID.randomUUID())
                .param("aggId", orderId.toString())
                .param("payload", jsonPayload)
                .update();
        } catch (Exception ex) {
            throw new RuntimeException("Outbox serialization failure", ex);
        }
    }
}
```

### 2.5 REST Controller

```java
package com.example.ecommerce.web;

import com.example.ecommerce.dto.OrderConfirmationResponse;
import com.example.ecommerce.dto.PlaceOrderCommand;
import com.example.ecommerce.service.OrderPlacementService;
import jakarta.validation.Valid;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.net.URI;

@RestController
@RequestMapping("/api/v1/orders")
public class OrderController {

    private final OrderPlacementService orderPlacementService;

    public OrderController(OrderPlacementService orderPlacementService) {
        this.orderPlacementService = orderPlacementService;
    }

    @PostMapping
    public ResponseEntity<OrderConfirmationResponse> placeOrder(@Valid @RequestBody PlaceOrderCommand command) {
        OrderConfirmationResponse response = orderPlacementService.placeOrder(command);
        return ResponseEntity.created(URI.create("/api/v1/orders/" + response.orderId())).body(response);
    }
}
```

---

## 3. Automated Testing Suite

```java
package com.example.ecommerce;

import com.example.ecommerce.domain.InventoryStock;
import com.example.ecommerce.dto.PlaceOrderCommand;
import com.example.ecommerce.repository.InventoryStockRepository;
import com.example.ecommerce.service.OrderPlacementService;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.boot.testcontainers.service.connection.ServiceConnection;
import org.testcontainers.containers.PostgreSQLContainer;
import org.testcontainers.junit.jupiter.Container;
import org.testcontainers.junit.jupiter.Testcontainers;

import java.math.BigDecimal;
import java.util.concurrent.*;
import java.util.concurrent.atomic.AtomicInteger;

import static org.assertj.core.api.Assertions.assertThat;

@SpringBootTest
@Testcontainers
class HighConcurrencyOrderPlacementTest {

    @Container
    @ServiceConnection
    static PostgreSQLContainer<?> postgres = new PostgreSQLContainer<>("postgres:16-alpine");

    @Autowired private OrderPlacementService orderService;
    @Autowired private InventoryStockRepository inventoryRepo;

    private static final String PRODUCT_SKU = "PROD-IPHONE-15";

    @BeforeEach
    void setUp() {
        inventoryRepo.deleteAll();
        // Seed exactly 10 units of stock
        inventoryRepo.save(new InventoryStock(PRODUCT_SKU, 10));
    }

    @Test
    void shouldPreventOverSellingUnderHighConcurrency() throws Exception {
        int threadCount = 20; // 20 concurrent threads trying to buy 1 item each
        ExecutorService executor = Executors.newFixedThreadPool(threadCount);
        CountDownLatch startLatch = new CountDownLatch(1);
        CountDownLatch doneLatch = new CountDownLatch(threadCount);

        AtomicInteger successOrders = new AtomicInteger(0);
        AtomicInteger rejectedOrders = new AtomicInteger(0);

        for (int i = 0; i < threadCount; i++) {
            final String custId = "cust-" + i;
            executor.submit(() -> {
                try {
                    startLatch.await(); // Simultaneous thunder herd!
                    orderService.placeOrder(new PlaceOrderCommand(PRODUCT_SKU, 1, custId, BigDecimal.valueOf(999.00)));
                    successOrders.incrementAndGet();
                } catch (Exception ex) {
                    rejectedOrders.incrementAndGet();
                } finally {
                    doneLatch.countDown();
                }
            });
        }

        startLatch.countDown(); // Release threads
        doneLatch.await(10, TimeUnit.SECONDS);

        // Exactly 10 orders must succeed; exactly 10 orders must be rejected!
        assertThat(successOrders.get()).isEqualTo(10);
        assertThat(rejectedOrders.get()).isEqualTo(10);

        InventoryStock finalStock = inventoryRepo.findById(PRODUCT_SKU).orElseThrow();
        assertThat(finalStock.getAvailableQuantity()).isEqualTo(0);
    }
}
```

---

## 4. Production Infrastructure & Deployment

### 4.1 Multi-Stage `Dockerfile`

```dockerfile
FROM eclipse-temurin:21-jdk-alpine AS build
WORKDIR /app
COPY pom.xml .
COPY src src
RUN ./mvnw clean package -DskipTests

FROM eclipse-temurin:21-jre-alpine
WORKDIR /app
RUN addgroup -S appgroup && adduser -S appuser -G appgroup
USER appuser:appgroup
COPY --from=build /app/target/*.jar app.jar
ENV JAVA_TOOL_OPTIONS="-XX:+UseG1GC -XX:MaxRAMPercentage=75.0"
EXPOSE 8080
ENTRYPOINT ["java", "-jar", "app.jar"]
```

### 4.2 `compose.yaml`

```yaml
services:
  ecommerce-api:
    build: .
    ports:
      - "8080:8080"
    environment:
      - SPRING_DATASOURCE_URL=jdbc:postgresql://postgres-ecom:5432/ecom_db
      - SPRING_KAFKA_BOOTSTRAP_SERVERS=kafka:9092
    depends_on:
      postgres-ecom:
        condition: service_healthy

  postgres-ecom:
    image: postgres:16-alpine
    environment:
      - POSTGRES_DB=ecom_db
      - POSTGRES_USER=ecom_user
      - POSTGRES_PASSWORD=ecom_secret
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U ecom_user -d ecom_db"]
      interval: 5s
      timeout: 5s
      retries: 5
```

---

## 5. Operational Runbook & Verification

```bash
# 1. Place order
curl -X POST http://localhost:8080/api/v1/orders \
     -H "Content-Type: application/json" \
     -d '{
       "productId": "PROD-IPHONE-15",
       "quantity": 1,
       "customerId": "cust-8912",
       "totalAmount": 999.00
     }' | jq .
```
