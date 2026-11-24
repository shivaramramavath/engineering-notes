# Production Reference Architecture: REST API CRUD with PostgreSQL

## Metadata & Production Baseline

| Attribute | Specification |
|:---|:---|
| **Project Code** | `SPRING-PROJ-001` |
| **Spring Boot Version** | `3.4.x` / `4.x` |
| **Spring Framework Version** | `6.2.x` / `7.0.x` |
| **Java Baseline** | `Java 21` / `Java 25 LTS` |
| **Build Tool** | `Maven 3.9+` |
| **Database & ORM** | `PostgreSQL 16+`, `Spring Data JPA (Hibernate 6.6+)`, `Flyway` |
| **Testing** | `JUnit 5.11+`, `AssertJ`, `Testcontainers 1.20+`, `MockMvc` |
| **Complexity Level** | Beginner to Intermediate |

---

## 1. Architectural Overview & Domain Design

### 1.1 System Architecture & Data Flow

```
                      +---------------------------------------+
                      |         Web / Mobile Client           |
                      +-------------------+-------------------+
                                          |
                                          | HTTP POST/GET/PUT/DELETE
                                          v
                      +---------------------------------------+
                      |       ProductController (REST)        |
                      +-------------------+-------------------+
                                          |
                                          | ProductRequest / Response DTOs
                                          v
                      +---------------------------------------+
                      |     ProductService (@Transactional)   |
                      +-------------------+-------------------+
                                          |
                                          | Product Entity
                                          v
                      +---------------------------------------+
                      |     ProductRepository (Spring Data)   |
                      +-------------------+-------------------+
                                          |
                                          | HikariCP Connection Pool
                                          v
                      +---------------------------------------+
                      |     PostgreSQL 16+ Database           |
                      +---------------------------------------+
```

### 1.2 Directory Layout

```
01-rest-api-crud-with-postgres/
|-- pom.xml
|-- Dockerfile
|-- compose.yaml
`-- src/
    |-- main/
    |   |-- java/com/example/product/
    |   |   |-- ProductApplication.java
    |   |   |-- domain/
    |   |   |   |-- Product.java
    |   |   |   `-- Sku.java
    |   |   |-- dto/
    |   |   |   |-- CreateProductRequest.java
    |   |   |   |-- UpdateProductRequest.java
    |   |   |   `-- ProductResponse.java
    |   |   |-- repository/
    |   |   |   `-- ProductRepository.java
    |   |   |-- service/
    |   |   |   `-- ProductService.java
    |   |   `-- web/
    |   |       |-- ProductController.java
    |   |       `-- GlobalExceptionHandler.java
    |   `-- resources/
    |       |-- application.yaml
    |       `-- db/migration/
    |           `-- V1__init_products_table.sql
    `-- test/
        `-- java/com/example/product/
            |-- ProductIntegrationTest.java
            `-- ProductControllerWebTest.java
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
    <artifactId>product-crud-service</artifactId>
    <version>1.0.0</version>
    <name>product-crud-service</name>
    <description>Production-grade CRUD microservice with Spring Boot and PostgreSQL</description>

    <properties>
        <java.version>21</java.version>
    </properties>

    <dependencies>
        <!-- Web & Validation -->
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-web</artifactId>
        </dependency>
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-validation</artifactId>
        </dependency>

        <!-- Data Access & Migration -->
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-data-jpa</artifactId>
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

        <!-- Observability -->
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-actuator</artifactId>
        </dependency>

        <!-- Testing -->
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-test</artifactId>
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

### 2.2 Application Configuration (`src/main/resources/application.yaml`)

```yaml
server:
  port: 8080
  shutdown: graceful

spring:
  application:
    name: product-crud-service
  threads:
    virtual:
      enabled: true

  datasource:
    url: ${SPRING_DATASOURCE_URL:jdbc:postgresql://localhost:5432/product_db}
    username: ${SPRING_DATASOURCE_USERNAME:postgres}
    password: ${SPRING_DATASOURCE_PASSWORD:postgres}
    driver-class-name: org.postgresql.Driver
    hikari:
      maximum-pool-size: 15
      minimum-idle: 5
      idle-timeout: 300000
      connection-timeout: 20000
      pool-name: ProductHikariPool

  jpa:
    open-in-view: false
    hibernate:
      ddl-auto: validate
    properties:
      hibernate:
        format_sql: false
        jdbc.batch_size: 25
        order_inserts: true
        order_updates: true

  flyway:
    enabled: true
    baseline-on-migrate: true
    locations: classpath:db/migration

management:
  endpoints:
    web:
      exposure:
        include: health,info,metrics
  endpoint:
    health:
      probes:
        enabled: true
```

### 2.3 Database Migration (`src/main/resources/db/migration/V1__init_products_table.sql`)

```sql
CREATE TABLE IF NOT EXISTS products (
    id UUID PRIMARY KEY,
    sku VARCHAR(64) NOT NULL UNIQUE,
    name VARCHAR(255) NOT NULL,
    description TEXT,
    price NUMERIC(12, 2) NOT NULL,
    stock_quantity INT NOT NULL DEFAULT 0,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    version BIGINT NOT NULL DEFAULT 0
);

CREATE INDEX idx_products_sku ON products (sku);
CREATE INDEX idx_products_name ON products (name);
```

### 2.4 Domain Entity & DTOs

```java
package com.example.product.domain;

import jakarta.persistence.*;
import java.math.BigDecimal;
import java.time.Instant;
import java.util.Objects;
import java.util.UUID;

@Entity
@Table(name = "products")
public class Product {

    @Id
    @GeneratedValue(strategy = GenerationType.UUID)
    private UUID id;

    @Column(nullable = false, unique = true, length = 64)
    private String sku;

    @Column(nullable = false)
    private String name;

    @Column(columnDefinition = "TEXT")
    private String description;

    @Column(nullable = false, precision = 12, scale = 2)
    private BigDecimal price;

    @Column(name = "stock_quantity", nullable = false)
    private int stockQuantity;

    @Column(name = "created_at", nullable = false, updatable = false)
    private Instant createdAt;

    @Column(name = "updated_at", nullable = false)
    private Instant updatedAt;

    @Version
    private Long version;

    protected Product() {}

    public Product(String sku, String name, String description, BigDecimal price, int stockQuantity) {
        this.sku = Objects.requireNonNull(sku, "SKU is required");
        this.name = Objects.requireNonNull(name, "Name is required");
        this.description = description;
        this.price = Objects.requireNonNull(price, "Price is required");
        this.stockQuantity = stockQuantity;
        this.createdAt = Instant.now();
        this.updatedAt = Instant.now();
    }

    public void updateDetails(String name, String description, BigDecimal price, int stockQuantity) {
        this.name = Objects.requireNonNull(name);
        this.description = description;
        this.price = Objects.requireNonNull(price);
        this.stockQuantity = stockQuantity;
        this.updatedAt = Instant.now();
    }

    public UUID getId() { return id; }
    public String getSku() { return sku; }
    public String getName() { return name; }
    public String getDescription() { return description; }
    public BigDecimal getPrice() { return price; }
    public int getStockQuantity() { return stockQuantity; }
    public Instant getCreatedAt() { return createdAt; }
    public Instant getUpdatedAt() { return updatedAt; }
}
```

```java
package com.example.product.dto;

import com.example.product.domain.Product;
import jakarta.validation.constraints.*;
import java.math.BigDecimal;
import java.time.Instant;
import java.util.UUID;

public record CreateProductRequest(
    @NotBlank @Pattern(regexp = "^[A-Z0-9_-]{3,64}$", message = "SKU must be uppercase alphanumeric")
    String sku,

    @NotBlank @Size(min = 2, max = 255)
    String name,

    @Size(max = 2000)
    String description,

    @NotNull @DecimalMin(value = "0.01", message = "Price must be greater than zero")
    BigDecimal price,

    @Min(0)
    int stockQuantity
) {}

public record UpdateProductRequest(
    @NotBlank @Size(min = 2, max = 255) String name,
    @Size(max = 2000) String description,
    @NotNull @DecimalMin(value = "0.01") BigDecimal price,
    @Min(0) int stockQuantity
) {}

public record ProductResponse(
    UUID id,
    String sku,
    String name,
    String description,
    BigDecimal price,
    int stockQuantity,
    Instant createdAt,
    Instant updatedAt
) {
    public static ProductResponse fromEntity(Product p) {
        return new ProductResponse(
            p.getId(), p.getSku(), p.getName(), p.getDescription(),
            p.getPrice(), p.getStockQuantity(), p.getCreatedAt(), p.getUpdatedAt()
        );
    }
}
```

### 2.5 Repository & Service

```java
package com.example.product.repository;

import com.example.product.domain.Product;
import org.springframework.data.domain.Page;
import org.springframework.data.domain.Pageable;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.stereotype.Repository;

import java.util.Optional;
import java.util.UUID;

@Repository
public interface ProductRepository extends JpaRepository<Product, UUID> {
    Optional<Product> findBySku(String sku);
    boolean existsBySku(String sku);
    Page<Product> findByNameContainingIgnoreCase(String keyword, Pageable pageable);
}
```

```java
package com.example.product.service;

import com.example.product.domain.Product;
import com.example.product.dto.CreateProductRequest;
import com.example.product.dto.ProductResponse;
import com.example.product.dto.UpdateProductRequest;
import com.example.product.repository.ProductRepository;
import org.springframework.data.domain.Page;
import org.springframework.data.domain.Pageable;
import org.springframework.http.HttpStatus;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;
import org.springframework.web.server.ResponseStatusException;

import java.util.UUID;

@Service
@Transactional
public class ProductService {

    private final ProductRepository productRepository;

    public ProductService(ProductRepository productRepository) {
        this.productRepository = productRepository;
    }

    public ProductResponse createProduct(CreateProductRequest req) {
        if (productRepository.existsBySku(req.sku())) {
            throw new ResponseStatusException(HttpStatus.CONFLICT, "Product with SKU " + req.sku() + " already exists");
        }
        Product entity = new Product(req.sku(), req.name(), req.description(), req.price(), req.stockQuantity());
        Product saved = productRepository.save(entity);
        return ProductResponse.fromEntity(saved);
    }

    @Transactional(readOnly = true)
    public ProductResponse getProductById(UUID id) {
        return productRepository.findById(id)
                .map(ProductResponse::fromEntity)
                .orElseThrow(() -> new ResponseStatusException(HttpStatus.NOT_FOUND, "Product not found with id: " + id));
    }

    @Transactional(readOnly = true)
    public Page<ProductResponse> searchProducts(String keyword, Pageable pageable) {
        if (keyword != null && !keyword.isBlank()) {
            return productRepository.findByNameContainingIgnoreCase(keyword, pageable).map(ProductResponse::fromEntity);
        }
        return productRepository.findAll(pageable).map(ProductResponse::fromEntity);
    }

    public ProductResponse updateProduct(UUID id, UpdateProductRequest req) {
        Product product = productRepository.findById(id)
                .orElseThrow(() -> new ResponseStatusException(HttpStatus.NOT_FOUND, "Product not found with id: " + id));

        product.updateDetails(req.name(), req.description(), req.price(), req.stockQuantity());
        // Managed entity dirty checking triggers automatic UPDATE statement
        return ProductResponse.fromEntity(product);
    }

    public void deleteProduct(UUID id) {
        if (!productRepository.existsById(id)) {
            throw new ResponseStatusException(HttpStatus.NOT_FOUND, "Product not found with id: " + id);
        }
        productRepository.deleteById(id);
    }
}
```

### 2.6 REST Controller & Error Handling

```java
package com.example.product.web;

import com.example.product.dto.CreateProductRequest;
import com.example.product.dto.ProductResponse;
import com.example.product.dto.UpdateProductRequest;
import com.example.product.service.ProductService;
import jakarta.validation.Valid;
import org.springframework.data.domain.Page;
import org.springframework.data.domain.Pageable;
import org.springframework.data.web.PageableDefault;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.net.URI;
import java.util.UUID;

@RestController
@RequestMapping("/api/v1/products")
public class ProductController {

    private final ProductService productService;

    public ProductController(ProductService productService) {
        this.productService = productService;
    }

    @PostMapping
    public ResponseEntity<ProductResponse> createProduct(@Valid @RequestBody CreateProductRequest request) {
        ProductResponse response = productService.createProduct(request);
        return ResponseEntity.created(URI.create("/api/v1/products/" + response.id())).body(response);
    }

    @GetMapping("/{id}")
    public ResponseEntity<ProductResponse> getProduct(@PathVariable UUID id) {
        return ResponseEntity.ok(productService.getProductById(id));
    }

    @GetMapping
    public ResponseEntity<Page<ProductResponse>> listProducts(
            @RequestParam(required = false) String search,
            @PageableDefault(size = 20, sort = "createdAt") Pageable pageable) {
        return ResponseEntity.ok(productService.searchProducts(search, pageable));
    }

    @PutMapping("/{id}")
    public ResponseEntity<ProductResponse> updateProduct(
            @PathVariable UUID id, 
            @Valid @RequestBody UpdateProductRequest request) {
        return ResponseEntity.ok(productService.updateProduct(id, request));
    }

    @DeleteMapping("/{id}")
    public ResponseEntity<Void> deleteProduct(@PathVariable UUID id) {
        productService.deleteProduct(id);
        return ResponseEntity.noContent().build();
    }
}
```

---

## 3. Automated Testing Suite

### 3.1 Integration Test with Testcontainers

```java
package com.example.product;

import com.example.product.dto.CreateProductRequest;
import com.example.product.dto.ProductResponse;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.boot.test.web.client.TestRestTemplate;
import org.springframework.boot.testcontainers.service.connection.ServiceConnection;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.testcontainers.containers.PostgreSQLContainer;
import org.testcontainers.junit.jupiter.Container;
import org.testcontainers.junit.jupiter.Testcontainers;

import java.math.BigDecimal;
import java.util.UUID;

import static org.assertj.core.api.Assertions.assertThat;

@SpringBootTest(webEnvironment = SpringBootTest.WebEnvironment.RANDOM_PORT)
@Testcontainers
class ProductIntegrationTest {

    @Container
    @ServiceConnection
    static PostgreSQLContainer<?> postgres = new PostgreSQLContainer<>("postgres:16-alpine");

    @Autowired
    private TestRestTemplate restTemplate;

    @Test
    void shouldCreateAndRetrieveProductSuccessfully() {
        String uniqueSku = "SKU-" + UUID.randomUUID().toString().substring(0, 8).toUpperCase();
        CreateProductRequest request = new CreateProductRequest(
                uniqueSku, "Mechanical Keyboard", "Cherry MX Blue switches", BigDecimal.valueOf(129.99), 45
        );

        // 1. Create Product
        ResponseEntity<ProductResponse> createResponse = restTemplate.postForEntity(
                "/api/v1/products", request, ProductResponse.class
        );
        assertThat(createResponse.getStatusCode()).isEqualTo(HttpStatus.CREATED);
        ProductResponse created = createResponse.getBody();
        assertThat(created).isNotNull();
        assertThat(created.sku()).isEqualTo(uniqueSku);

        // 2. Fetch by ID
        ResponseEntity<ProductResponse> getResponse = restTemplate.getForEntity(
                "/api/v1/products/" + created.id(), ProductResponse.class
        );
        assertThat(getResponse.getStatusCode()).isEqualTo(HttpStatus.OK);
        assertThat(getResponse.getBody().name()).isEqualTo("Mechanical Keyboard");
    }
}
```

---

## 4. Production Infrastructure & Deployment

### 4.1 Multi-Stage `Dockerfile`

```dockerfile
# Stage 1: Build stage with Maven and Eclipse Temurin JDK 21
FROM eclipse-temurin:21-jdk-alpine AS builder
WORKDIR /workspace
COPY pom.xml .
COPY src src
RUN ./mvnw clean package -DskipTests

# Stage 2: Minimal Distroless / JRE Runtime
FROM eclipse-temurin:21-jre-alpine
WORKDIR /app
RUN addgroup -S appgroup && adduser -S appuser -G appgroup
USER appuser:appgroup

COPY --from=builder /workspace/target/*.jar app.jar

ENV JAVA_TOOL_OPTIONS="-XX:+UseG1GC -XX:MaxRAMPercentage=75.0 -XX:+ExitOnOutOfMemoryError"
EXPOSE 8080

ENTRYPOINT ["java", "-jar", "app.jar"]
```

### 4.2 `compose.yaml` (Docker Compose)

```yaml
services:
  product-service:
    build: .
    ports:
      - "8080:8080"
    environment:
      - SPRING_DATASOURCE_URL=jdbc:postgresql://postgres-db:5432/product_db
      - SPRING_DATASOURCE_USERNAME=postgres
      - SPRING_DATASOURCE_PASSWORD=secretpassword
    depends_on:
      postgres-db:
        condition: service_healthy

  postgres-db:
    image: postgres:16-alpine
    environment:
      - POSTGRES_DB=product_db
      - POSTGRES_USER=postgres
      - POSTGRES_PASSWORD=secretpassword
    ports:
      - "5432:5432"
    volumes:
      - pgdata:/var/lib/postgresql/data
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U postgres -d product_db"]
      interval: 5s
      timeout: 5s
      retries: 5

volumes:
  pgdata:
```

---

## 5. Operational Runbook & Verification

### 5.1 Verification Commands

```bash
# 1. Start application stack with Docker Compose
docker compose up -d

# 2. Check health probes
curl -s http://localhost:8080/actuator/health | jq .

# 3. Create a product
curl -X POST http://localhost:8080/api/v1/products \
     -H "Content-Type: application/json" \
     -d '{
       "sku": "PROD-KB-001",
       "name": "Wireless Ergonomic Keyboard",
       "description": "Split mechanical keyboard with wrist rest",
       "price": 149.99,
       "stockQuantity": 100
     }' | jq .

# 4. Search products with pagination
curl -s "http://localhost:8080/api/v1/products?search=Keyboard&page=0&size=10" | jq .
```
