# Project 10: Multi-Tenant SaaS Platform with Schema Isolation

| Metadata Field | Value |
|---|---|
| **Spring Boot Version** | 3.4.2 |
| **Spring Framework** | 6.2.2 |
| **Java Version** | 21 (LTS) |
| **Build System** | Maven 3.9+ |
| **Key Dependencies** | Spring Boot Starter Web, Spring Boot Starter Data JPA, Flyway Database PostgreSQL, PostgreSQL Driver, Testcontainers |
| **Architecture Style** | Schema-per-Tenant Multi-Tenancy with Dynamic Hibernate Connection Provider & JWT Tenant Context |

---

## 1. Architectural Overview & Domain Design

### 1.1 System Architecture

This SaaS architecture uses **Schema-per-Tenant** isolation within PostgreSQL. An incoming HTTP request carries a `X-Tenant-ID` header (or JWT claim). A servlet filter extracts the tenant ID into a `TenantContext` (`ThreadLocal`). Hibernate 6 utilizes a custom `CurrentTenantIdentifierResolver` and `MultiTenantConnectionProvider` that executes `SET search_path TO <tenant_schema>, public` on pooled connections before handing them to the transaction. Dynamic Flyway migrations ensure that newly onboarded tenants automatically receive their schema and migrations.

```
       [ Client Request ] (Header: X-Tenant-ID: acme_corp)
               │
               ▼
   ┌─────────────────────────────────────────────────────────┐
   │            Spring Web Security & TenantFilter           │
   │  1. Extract 'acme_corp'                                 │
   │  2. TenantContext.setCurrentTenant("acme_corp")         │
   └───────────────────────────┬─────────────────────────────┘
                               │
                               ▼
   ┌─────────────────────────────────────────────────────────┐
   │             Hibernate 6 Multi-Tenancy Layer             │
   │  ┌───────────────────────────────────────────────────┐  │
   │  │ CurrentTenantIdentifierResolver                   │  │
   │  │ -> returns "acme_corp"                            │  │
   │  └─────────────────────────┬─────────────────────────┘  │
   │                            ▼                            │
   │  ┌───────────────────────────────────────────────────┐  │
   │  │ SchemaMultiTenantConnectionProvider               │  │
   │  │ -> Borrow connection from HikariCP Pool           │  │
   │  │ -> Execute: "SET search_path TO acme_corp, public"│  │
   │  └───────────────────────────────────────────────────┘  │
   └───────────────────────────┬─────────────────────────────┘
                               │
                               ▼
   ┌─────────────────────────────────────────────────────────┐
   │                  PostgreSQL Database                    │
   │  ┌────────────────────┐      ┌────────────────────────┐ │
   │  │ Schema: "public"   │      │ Schema: "acme_corp"    │ │
   │  │ - tenants registry │      │ - products             │ │
   │  │                    │      │ - orders               │ │
   │  └────────────────────┘      └────────────────────────┘ │
   └─────────────────────────────────────────────────────────┘
```

### 1.2 Directory Layout

```
10-multi-tenant-saas-api/
├── pom.xml
├── src/
│   ├── main/
│   │   ├── java/com/example/saas/
│   │   │   ├── MultiTenantSaasApplication.java
│   │   │   ├── config/
│   │   │   │   ├── TenantContext.java
│   │   │   │   ├── TenantInterceptor.java
│   │   │   │   ├── WebMvcConfig.java
│   │   │   │   ├── HibernateMultiTenancyConfig.java
│   │   │   │   └── DynamicFlywayTenantProvisioner.java
│   │   │   ├── domain/
│   │   │   │   ├── Product.java
│   │   │   │   └── TenantMaster.java
│   │   │   ├── repository/
│   │   │   │   ├── ProductRepository.java
│   │   │   │   └── TenantMasterRepository.java
│   │   │   ├── service/
│   │   │   │   ├── TenantProvisioningService.java
│   │   │   │   └── ProductService.java
│   │   │   └── web/
│   │   │       ├── TenantAdminController.java
│   │   │       ├── ProductController.java
│   │   │       └── dto/
│   │   │           ├── CreateTenantRequest.java
│   │   │           ├── CreateProductRequest.java
│   │   │           └── ProductDto.java
│   │   └── resources/
│   │       ├── application.yaml
│   │       ├── db/migration/default/V1__init_master_schema.sql
│   │       └── db/migration/tenants/V1__init_tenant_schema.sql
│   └── test/
│       └── java/com/example/saas/
│           └── MultiTenantIntegrationTest.java
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
    <artifactId>multi-tenant-saas-api</artifactId>
    <version>1.0.0</version>
    <name>multi-tenant-saas-api</name>
    <description>Enterprise Schema-per-Tenant Multi-Tenant SaaS Backend</description>

    <properties>
        <java.version>21</java.version>
        <testcontainers.version>1.20.4</testcontainers.version>
    </properties>

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
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-validation</artifactId>
        </dependency>
        <dependency>
            <groupId>org.flywaydb</groupId>
            <artifactId>flyway-core</artifactId>
        </dependency>
        <dependency>
            <groupId>org.flywaydb</groupId>
            <artifactId>flyway-database-postgresql</artifactId>
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
    name: multi-tenant-saas
  datasource:
    url: jdbc:postgresql://${DB_HOST:localhost}:${DB_PORT:5432}/${DB_NAME:saas_db}
    username: ${DB_USER:postgres}
    password: ${DB_PASS:postgres}
  jpa:
    hibernate:
      ddl-auto: none
    open-in-view: false
    show-sql: false
  flyway:
    enabled: false # Handled programmatically for master and tenant schemas
```

### 2.3 Flyway Migrations

`db/migration/default/V1__init_master_schema.sql`:
```sql
CREATE TABLE IF NOT EXISTS tenants_master (
    tenant_id VARCHAR(64) PRIMARY KEY,
    company_name VARCHAR(128) NOT NULL,
    schema_name VARCHAR(64) NOT NULL UNIQUE,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);
```

`db/migration/tenants/V1__init_tenant_schema.sql`:
```sql
CREATE TABLE IF NOT EXISTS products (
    id UUID PRIMARY KEY,
    sku VARCHAR(64) NOT NULL UNIQUE,
    name VARCHAR(255) NOT NULL,
    price NUMERIC(10, 2) NOT NULL,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);
```

### 2.4 Multi-Tenancy Context & Hibernate Configuration

```java
package com.example.saas.config;

public final class TenantContext {
    private static final ThreadLocal<String> CURRENT_TENANT = new ThreadLocal<>();
    public static final String DEFAULT_TENANT = "public";

    private TenantContext() {}

    public static void setTenantId(String tenantId) {
        CURRENT_TENANT.set(tenantId);
    }

    public static String getTenantId() {
        return CURRENT_TENANT.get() != null ? CURRENT_TENANT.get() : DEFAULT_TENANT;
    }

    public static void clear() {
        CURRENT_TENANT.remove();
    }
}
```

```java
package com.example.saas.config;

import jakarta.servlet.http.HttpServletRequest;
import jakarta.servlet.http.HttpServletResponse;
import org.springframework.stereotype.Component;
import org.springframework.web.servlet.HandlerInterceptor;

@Component
public class TenantInterceptor implements HandlerInterceptor {

    public static final String TENANT_HEADER = "X-Tenant-ID";

    @Override
    public boolean preHandle(HttpServletRequest request, HttpServletResponse response, Object handler) {
        String tenantId = request.getHeader(TENANT_HEADER);
        if (tenantId != null && !tenantId.isBlank()) {
            TenantContext.setTenantId(tenantId.trim());
        } else {
            TenantContext.setTenantId(TenantContext.DEFAULT_TENANT);
        }
        return true;
    }

    @Override
    public void afterCompletion(HttpServletRequest request, HttpServletResponse response, Object handler, Exception ex) {
        TenantContext.clear();
    }
}
```

```java
package com.example.saas.config;

import org.hibernate.context.spi.CurrentTenantIdentifierResolver;
import org.hibernate.engine.jdbc.connections.spi.MultiTenantConnectionProvider;
import org.springframework.boot.autoconfigure.orm.jpa.HibernatePropertiesCustomizer;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;

import javax.sql.DataSource;
import java.sql.Connection;
import java.sql.SQLException;
import java.sql.Statement;
import java.util.Map;

@Configuration
public class HibernateMultiTenancyConfig {

    @Bean
    public CurrentTenantIdentifierResolver<String> currentTenantIdentifierResolver() {
        return new CurrentTenantIdentifierResolver<>() {
            @Override
            public String resolveCurrentTenantIdentifier() {
                return TenantContext.getTenantId();
            }

            @Override
            public boolean validateExistingCurrentSessions() {
                return true;
            }
        };
    }

    @Bean
    public MultiTenantConnectionProvider<String> multiTenantConnectionProvider(DataSource dataSource) {
        return new MultiTenantConnectionProvider<>() {
            @Override
            public Connection getAnyConnection() throws SQLException {
                return dataSource.getConnection();
            }

            @Override
            public void releaseAnyConnection(Connection connection) throws SQLException {
                connection.close();
            }

            @Override
            public Connection getConnection(String tenantIdentifier) throws SQLException {
                Connection connection = getAnyConnection();
                try (Statement statement = connection.createStatement()) {
                    // Set PostgreSQL search path to the tenant schema
                    statement.execute("SET search_path TO \"" + tenantIdentifier + "\", public");
                }
                return connection;
            }

            @Override
            public void releaseConnection(String tenantIdentifier, Connection connection) throws SQLException {
                try (Statement statement = connection.createStatement()) {
                    statement.execute("SET search_path TO public");
                }
                connection.close();
            }

            @Override
            public boolean supportsAggressiveRelease() {
                return true;
            }

            @Override
            public boolean isUnwrappableAs(Class<?> unwrapType) {
                return false;
            }

            @Override
            public <T> T unwrap(Class<T> unwrapType) {
                return null;
            }
        };
    }

    @Bean
    public HibernatePropertiesCustomizer hibernatePropertiesCustomizer(
            CurrentTenantIdentifierResolver<String> tenantResolver,
            MultiTenantConnectionProvider<String> connectionProvider) {
        return hibernateProperties -> {
            hibernateProperties.put(org.hibernate.cfg.AvailableSettings.MULTI_TENANT_IDENTIFIER_RESOLVER, tenantResolver);
            hibernateProperties.put(org.hibernate.cfg.AvailableSettings.MULTI_TENANT_CONNECTION_PROVIDER, connectionProvider);
        };
    }
}
```

### 2.5 Domain, Repository & Dynamic Flyway Provisioner

```java
package com.example.saas.domain;

import jakarta.persistence.*;
import java.math.BigDecimal;
import java.time.Instant;
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

    @Column(nullable = false, precision = 10, scale = 2)
    private BigDecimal price;

    @Column(name = "created_at", nullable = false, updatable = false)
    private Instant createdAt = Instant.now();

    public Product() {}

    public Product(String sku, String name, BigDecimal price) {
        this.sku = sku;
        this.name = name;
        this.price = price;
    }

    public UUID getId() { return id; }
    public String getSku() { return sku; }
    public String getName() { return name; }
    public BigDecimal getPrice() { return price; }
}
```

```java
package com.example.saas.domain;

import jakarta.persistence.*;
import java.time.Instant;

@Entity
@Table(name = "tenants_master", schema = "public")
public class TenantMaster {

    @Id
    @Column(name = "tenant_id", length = 64)
    private String tenantId;

    @Column(name = "company_name", nullable = false, length = 128)
    private String companyName;

    @Column(name = "schema_name", nullable = false, length = 64, unique = true)
    private String schemaName;

    @Column(name = "created_at", nullable = false, updatable = false)
    private Instant createdAt = Instant.now();

    public TenantMaster() {}

    public TenantMaster(String tenantId, String companyName, String schemaName) {
        this.tenantId = tenantId;
        this.companyName = companyName;
        this.schemaName = schemaName;
    }

    public String getTenantId() { return tenantId; }
    public String getCompanyName() { return companyName; }
    public String getSchemaName() { return schemaName; }
}
```

```java
package com.example.saas.repository;

import com.example.saas.domain.Product;
import org.springframework.data.jpa.repository.JpaRepository;
import java.util.UUID;

public interface ProductRepository extends JpaRepository<Product, UUID> {}
```

```java
package com.example.saas.service;

import com.example.saas.domain.TenantMaster;
import jakarta.annotation.PostConstruct;
import org.flywaydb.core.Flyway;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Service;

import javax.sql.DataSource;

@Service
public class DynamicFlywayTenantProvisioner {

    private static final Logger log = LoggerFactory.getLogger(DynamicFlywayTenantProvisioner.class);
    private final DataSource dataSource;

    public DynamicFlywayTenantProvisioner(DataSource dataSource) {
        this.dataSource = dataSource;
    }

    @PostConstruct
    public void migrateMasterSchema() {
        log.info("Migrating public master tenant registry schema...");
        Flyway.configure()
                .dataSource(dataSource)
                .schemas("public")
                .locations("classpath:db/migration/default")
                .baselineOnMigrate(true)
                .load()
                .migrate();
    }

    public void provisionTenantSchema(String schemaName) {
        log.info("Provisioning and running Flyway migrations for tenant schema '{}'...", schemaName);
        Flyway.configure()
                .dataSource(dataSource)
                .schemas(schemaName)
                .locations("classpath:db/migration/tenants")
                .baselineOnMigrate(true)
                .load()
                .migrate();
    }
}
```

### 2.6 REST Controllers

```java
package com.example.saas.web.dto;

import jakarta.validation.constraints.DecimalMin;
import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.NotNull;
import java.math.BigDecimal;

public record CreateProductRequest(
        @NotBlank String sku,
        @NotBlank String name,
        @NotNull @DecimalMin("0.01") BigDecimal price
) {}
```

```java
package com.example.saas.web;

import com.example.saas.domain.Product;
import com.example.saas.repository.ProductRepository;
import com.example.saas.web.dto.CreateProductRequest;
import jakarta.validation.Valid;
import org.springframework.http.HttpStatus;
import org.springframework.web.bind.annotation.*;

import java.util.List;

@RestController
@RequestMapping("/api/v1/products")
public class ProductController {

    private final ProductRepository productRepository;

    public ProductController(ProductRepository productRepository) {
        this.productRepository = productRepository;
    }

    @PostMapping
    @ResponseStatus(HttpStatus.CREATED)
    public Product createProduct(@Valid @RequestBody CreateProductRequest request) {
        Product product = new Product(request.sku(), request.name(), request.price());
        return productRepository.save(product);
    }

    @GetMapping
    public List<Product> listProducts() {
        return productRepository.findAll();
    }
}
```

---

## 3. Automated Testing Suite

```java
package com.example.saas;

import com.example.saas.domain.Product;
import com.example.saas.repository.ProductRepository;
import com.example.saas.service.DynamicFlywayTenantProvisioner;
import org.junit.jupiter.api.BeforeAll;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.TestInstance;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.boot.test.web.client.TestRestTemplate;
import org.springframework.boot.testcontainers.service.connection.ServiceConnection;
import org.springframework.http.*;
import org.testcontainers.containers.PostgreSQLContainer;
import org.testcontainers.junit.jupiter.Container;
import org.testcontainers.junit.jupiter.Testcontainers;

import java.math.BigDecimal;
import java.util.Map;

import static org.assertj.core.api.Assertions.assertThat;

@SpringBootTest(webEnvironment = SpringBootTest.WebEnvironment.RANDOM_PORT)
@Testcontainers
@TestInstance(TestInstance.Lifecycle.PER_CLASS)
public class MultiTenantIntegrationTest {

    @Container
    @ServiceConnection
    static PostgreSQLContainer<?> postgres = new PostgreSQLContainer<>("postgres:16-alpine");

    @Autowired
    private TestRestTemplate restTemplate;

    @Autowired
    private DynamicFlywayTenantProvisioner provisioner;

    @BeforeAll
    void setupTenants() {
        provisioner.provisionTenantSchema("tenant_apple");
        provisioner.provisionTenantSchema("tenant_google");
    }

    @Test
    void shouldIsolateDataBetweenTenants() {
        // 1. Post Product to tenant Apple
        HttpHeaders appleHeaders = new HttpHeaders();
        appleHeaders.set("X-Tenant-ID", "tenant_apple");
        HttpEntity<Map<String, Object>> appleReq = new HttpEntity<>(Map.of(
                "sku", "IPHONE-15",
                "name", "iPhone 15 Pro",
                "price", 999.00
        ), appleHeaders);

        ResponseEntity<Product> appleResp = restTemplate.postForEntity("/api/v1/products", appleReq, Product.class);
        assertThat(appleResp.getStatusCode()).isEqualTo(HttpStatus.CREATED);

        // 2. Query products for tenant Google (should be empty!)
        HttpHeaders googleHeaders = new HttpHeaders();
        googleHeaders.set("X-Tenant-ID", "tenant_google");
        HttpEntity<Void> googleReq = new HttpEntity<>(googleHeaders);

        ResponseEntity<Product[]> googleResp = restTemplate.exchange("/api/v1/products", HttpMethod.GET, googleReq, Product[].class);
        assertThat(googleResp.getStatusCode()).isEqualTo(HttpStatus.OK);
        assertThat(googleResp.getBody()).isEmpty();

        // 3. Query products for tenant Apple (should have 1 product)
        ResponseEntity<Product[]> appleQueryResp = restTemplate.exchange("/api/v1/products", HttpMethod.GET, new HttpEntity<>(appleHeaders), Product[].class);
        assertThat(appleQueryResp.getBody()).hasSize(1);
        assertThat(appleQueryResp.getBody()[0].getSku()).isEqualTo("IPHONE-15");
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
  saas-postgres:
    image: postgres:16-alpine
    container_name: saas-postgres
    environment:
      POSTGRES_DB: saas_db
      POSTGRES_USER: postgres
      POSTGRES_PASSWORD: saaspassword
    ports:
      - "5432:5432"
    volumes:
      - saas_pg_data:/var/lib/postgresql/data
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U postgres"]
      interval: 5s
      timeout: 5s
      retries: 5

  saas-api:
    build: .
    container_name: saas-api
    ports:
      - "8080:8080"
    environment:
      DB_HOST: saas-postgres
      DB_PORT: 5432
      DB_NAME: saas_db
      DB_USER: postgres
      DB_PASS: saaspassword
    depends_on:
      saas-postgres:
        condition: service_healthy

volumes:
  saas_pg_data:
```

---

## 5. Operational Runbook & Verification

### 5.1 Verification Commands

```bash
# 1. Start application stack
docker compose up -d

# 2. Check health
curl -i http://localhost:8080/actuator/health

# 3. Create a product in tenant 'acme'
curl -i -X POST http://localhost:8080/api/v1/products \
  -H "Content-Type: application/json" \
  -H "X-Tenant-ID: tenant_acme" \
  -d '{
    "sku": "WIDGET-01",
    "name": "Acme Industrial Anvil",
    "price": 49.99
  }'

# 4. Read products in tenant 'globex' (verify empty isolation)
curl -i -X GET http://localhost:8080/api/v1/products \
  -H "X-Tenant-ID: tenant_globex"

# 5. Read products in tenant 'acme' (verify product is returned)
curl -i -X GET http://localhost:8080/api/v1/products \
  -H "X-Tenant-ID: tenant_acme"
```
