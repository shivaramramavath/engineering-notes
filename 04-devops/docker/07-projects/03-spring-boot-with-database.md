<!--
================================================================================
Metadata:
  Title: Reference Project 03 - Production Java Spring Boot with MySQL
  Track: Docker / DevOps
  Difficulty: Intermediate to Advanced
  Estimated Time: 65 minutes
  Prerequisites: 03-dockerfile/04-multi-stage-builds.md, 06-production/10-resource-management-and-oom.md
  Key Terminology: Spring Boot 3, Java 21, Layered JARs, JVM Container Support, Spring Actuator, MySQL 8, Non-Root JRE
================================================================================
-->

# Reference Project 03: Production Java Spring Boot with MySQL

[<- 02-python-fastapi-stack.md](./02-python-fastapi-stack.md) | [Back to Projects Overview](./README.md) | [Next Project: 04-nginx-reverse-proxy-with-tls-local.md ->](./04-nginx-reverse-proxy-with-tls-local.md)

---

## 1. Overview & Architecture

Java enterprise applications have historically suffered from large container footprints, slow cold starts, and JVM memory misconfigurations that triggered silent kernel OOM terminations. This reference project implements a modern Java 21 Spring Boot 3.3 microservice connected to MySQL 8.0, showcasing:
1. **Layered JAR Docker Extraction**: Separating OS dependencies, Spring Boot loader, third-party libraries, and application class files into distinct cacheable Docker layers.
2. **Container-Aware JVM Ergonomics**: Dynamic heap sizing via `-XX:MaxRAMPercentage` aligned with Docker cgroup memory limits.
3. **Production Health Probing**: Integrating Spring Boot Actuator `/actuator/health` directly into Docker health checks.

### Architectural Diagram

```
+-----------------------------------------------------------------------------------+
|                                  HOST MACHINE                                     |
|                                                                                   |
|    HTTP Client / Actuator (:8080)                                                 |
|           |                                                                       |
|           v                                                                       |
|  +-----------------------------------------------------------------------------+  |
|  |                          DOCKER NETWORK: backend-net                        |  |
|  |                                                                             |  |
|  |   +---------------------------------------------------------------------+   |  |
|  |   |                       SERVICE: spring-app                           |   |  |
|  |   |  Eclipse Temurin JRE 21 (Alpine)                                    |   |  |
|  |   |  User: spring (UID 10001)                                           |   |  |
|  |   |  Memory Limit: 1024M | Heap: 768M (75% MaxRAMPercentage)            |   |  |
|  |   |  Healthcheck: curl /actuator/health/readiness                       |   |  |
|  |   +---------------------------------+-----------------------------------+   |  |
|  |                                     |                                       |  |
|  |                                     v TCP Connection (JDBC: 3306)           |  |
|  |   +---------------------------------------------------------------------+   |  |
|  |   |                       SERVICE: mysql-db                             |   |  |
|  |   |  MySQL 8.4 LTS Server                                               |   |  |
|  |   |  Healthcheck: mysqladmin ping                                       |   |  |
|  |   |  Volume: mysql_data mounted at /var/lib/mysql                       |   |  |
|  |   +---------------------------------+-----------------------------------+   |  |
|  |                                     |                                       |  |
|  |                                     v                                       |  |
|  |                         [VOLUME: mysql_data]                                |  |
|  +-----------------------------------------------------------------------------+  |
+-----------------------------------------------------------------------------------+
```

---

## 2. Directory Structure & File Listing

```
spring-boot-mysql-stack/
├── .dockerignore
├── .env.example
├── compose.yaml
├── Dockerfile
├── pom.xml
└── src/
    └── main/
        ├── java/
        │   └── com/
        │       └── company/
        │           └── app/
        │               ├── Application.java
        │               ├── controller/
        │               │   └── ProductController.java
        │               ├── model/
        │               │   └── Product.java
        │               └── repository/
        │                   └── ProductRepository.java
        └── resources/
            └── application.yml
```

---

## 3. Complete Implementation

### File: `pom.xml`

```xml
<?xml version="1.0" encoding="UTF-8"?>
<project xmlns="http://maven.apache.org/POM/4.0.0"
         xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
         xsi:schemaLocation="http://maven.apache.org/POM/4.0.0 https://maven.apache.org/xsd/maven-4.0.0.xsd">
    <modelVersion>4.0.0</modelVersion>

    <parent>
        <groupId>org.springframework.boot</groupId>
        <artifactId>spring-boot-starter-parent</artifactId>
        <version>3.3.0</version>
        <relativePath/>
    </parent>

    <groupId>com.company</groupId>
    <artifactId>spring-docker-demo</artifactId>
    <version>1.0.0</version>
    <name>spring-docker-demo</name>

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
            <artifactId>spring-boot-starter-data-jpa</artifactId>
        </dependency>
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-actuator</artifactId>
        </dependency>
        <dependency>
            <groupId>com.mysql</groupId>
            <artifactId>mysql-connector-j</artifactId>
            <scope>runtime</scope>
        </dependency>
    </dependencies>

    <build>
        <plugins>
            <plugin>
                <groupId>org.springframework.boot</groupId>
                <artifactId>spring-boot-maven-plugin</artifactId>
                <configuration>
                    <!-- Enables Spring Boot Layered JAR extraction -->
                    <layers>
                        <enabled>true</enabled>
                    </layers>
                </configuration>
            </plugin>
        </plugins>
    </build>
</project>
```

### File: `src/main/resources/application.yml`

```yaml
server:
  port: 8080
  shutdown: graceful

spring:
  application:
    name: spring-docker-demo
  datasource:
    url: jdbc:mysql://${DB_HOST:mysql-db}:${DB_PORT:3306}/${DB_NAME:productdb}?useSSL=false&allowPublicKeyRetrieval=true&serverTimezone=UTC
    username: ${DB_USER:appuser}
    password: ${DB_PASSWORD:secretpass}
    hikari:
      maximum-pool-size: 15
      minimum-idle: 5
      connection-timeout: 30000
  jpa:
    hibernate:
      ddl-auto: update
    properties:
      hibernate:
        dialect: org.hibernate.dialect.MySQLDialect

management:
  endpoints:
    web:
      exposure:
        include: health,info,metrics,prometheus
  endpoint:
    health:
      probes:
        enabled: true
      show-details: always
```

### File: `src/main/java/com/company/app/Application.java`

```java
package com.company.app;

import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;

@SpringBootApplication
public class Application {
    public static void main(String[] args) {
        SpringApplication.run(Application.class, args);
    }
}
```

### File: `src/main/java/com/company/app/model/Product.java`

```java
package com.company.app.model;

import jakarta.persistence.*;
import java.math.BigDecimal;

@Entity
@Table(name = "products")
public class Product {
    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @Column(nullable = false)
    private String name;

    @Column(nullable = false)
    private BigDecimal price;

    public Product() {}
    public Product(String name, BigDecimal price) {
        this.name = name;
        this.price = price;
    }

    public Long getId() { return id; }
    public String getName() { return name; }
    public void setName(String name) { this.name = name; }
    public BigDecimal getPrice() { return price; }
    public void setPrice(BigDecimal price) { this.price = price; }
}
```

### File: `src/main/java/com/company/app/repository/ProductRepository.java`

```java
package com.company.app.repository;

import com.company.app.model.Product;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.stereotype.Repository;

@Repository
public interface ProductRepository extends JpaRepository<Product, Long> {}
```

### File: `src/main/java/com/company/app/controller/ProductController.java`

```java
package com.company.app.controller;

import com.company.app.model.Product;
import com.company.app.repository.ProductRepository;
import org.springframework.http.HttpStatus;
import org.springframework.web.bind.annotation.*;

import java.util.List;

@RestController
@RequestMapping("/api/products")
public class ProductController {

    private final ProductRepository repository;

    public ProductController(ProductRepository repository) {
        this.repository = repository;
    }

    @GetMapping
    public List<Product> getAll() {
        return repository.findAll();
    }

    @PostMapping
    @ResponseStatus(HttpStatus.CREATED)
    public Product create(@RequestBody Product product) {
        return repository.save(product);
    }
}
```

### File: `Dockerfile`

```dockerfile
# syntax=docker/dockerfile:1.7

# -------------------------------------------------------------
# Stage 1: Maven Build with Layer Cache
# -------------------------------------------------------------
FROM maven:3.9.6-eclipse-temurin-21-alpine AS builder

WORKDIR /build

COPY pom.xml .
# Download dependencies with cache mount to accelerate rebuilds
RUN --mount=type=cache,target=/root/.m2 \
    mvn dependency:go-offline -B

COPY src/ ./src/
RUN --mount=type=cache,target=/root/.m2 \
    mvn clean package -DskipTests

# Extract Layered JAR contents
WORKDIR /build/extracted
RUN java -Djarmode=layertools -jar /build/target/*.jar extract

# -------------------------------------------------------------
# Stage 2: Hardened Production JRE Runtime
# -------------------------------------------------------------
FROM eclipse-temurin:21-jre-alpine AS runtime

WORKDIR /app

# Create unprivileged system user
RUN addgroup -g 10001 -S spring && \
    adduser -u 10001 -S spring -G spring

# Install curl for Actuator health checks
RUN apk add --no-cache curl

# Copy extracted layers in order of change frequency (least frequent to most frequent)
COPY --from=builder --chown=spring:spring /build/extracted/dependencies/ ./
COPY --from=builder --chown=spring:spring /build/extracted/spring-boot-loader/ ./
COPY --from=builder --chown=spring:spring /build/extracted/snapshot-dependencies/ ./
COPY --from=builder --chown=spring:spring /build/extracted/application/ ./

USER 10001:10001

# Dynamic JVM Memory Options based on container limits
ENV JAVA_TOOL_OPTIONS="-XX:+UseContainerSupport \
                       -XX:MaxRAMPercentage=75.0 \
                       -XX:InitialRAMPercentage=50.0 \
                       -XX:+ExitOnOutOfMemoryError \
                       -Djava.security.egd=file:/dev/./urandom"

HEALTHCHECK --interval=15s --timeout=3s --start-period=30s --retries=3 \
  CMD curl -f http://127.0.0.1:8080/actuator/health/readiness || exit 1

EXPOSE 8080

ENTRYPOINT ["java", "org.springframework.boot.loader.launch.JarLauncher"]
```

### File: `.dockerignore`

```
target/
.git
.gitignore
.env*
Dockerfile
compose*.yaml
README.md
```

### File: `.env.example`

```bash
DB_NAME=productdb
DB_USER=appuser
DB_PASSWORD=SuperSecretMySqlPassword2026!
```

### File: `compose.yaml`

```yaml
version: "3.8"

services:
  spring-app:
    build:
      context: .
      dockerfile: Dockerfile
    image: company/spring-demo:1.0.0
    restart: unless-stopped
    ports:
      - "8080:8080"
    environment:
      - DB_HOST=mysql-db
      - DB_PORT=3306
      - DB_NAME=${DB_NAME:-productdb}
      - DB_USER=${DB_USER:-appuser}
      - DB_PASSWORD=${DB_PASSWORD:-SuperSecretMySqlPassword2026!}
    depends_on:
      mysql-db:
        condition: service_healthy
    networks:
      - backend-net
    deploy:
      resources:
        limits:
          cpus: "1.5"
          memory: 1024M
        reservations:
          cpus: "0.5"
          memory: 512M

  mysql-db:
    image: mysql:8.4
    restart: unless-stopped
    environment:
      MYSQL_DATABASE: ${DB_NAME:-productdb}
      MYSQL_USER: ${DB_USER:-appuser}
      MYSQL_PASSWORD: ${DB_PASSWORD:-SuperSecretMySqlPassword2026!}
      MYSQL_ROOT_PASSWORD: RootSecretPassword2026!
    volumes:
      - mysql_data:/var/lib/mysql
    healthcheck:
      test: ["CMD", "mysqladmin", "ping", "-h", "localhost", "-u", "${DB_USER:-appuser}", "-p${DB_PASSWORD:-SuperSecretMySqlPassword2026!}"]
      interval: 5s
      timeout: 3s
      retries: 6
      start_period: 20s
    networks:
      - backend-net
    deploy:
      resources:
        limits:
          cpus: "1.0"
          memory: 768M

networks:
  backend-net:
    driver: bridge

volumes:
  mysql_data:
```

---

## 4. Step-by-Step Verification & Testing

### Step 1: Build and Launch Stack

```bash
mkdir -p spring-boot-mysql-stack && cd spring-boot-mysql-stack
cp .env.example .env
docker compose up -d --build
docker compose ps
```

### Step 2: Verify Actuator Health & Probes

```bash
curl -s http://localhost:8080/actuator/health
```
*Output*:
```json
{
  "status": "UP",
  "components": {
    "db": {
      "status": "UP",
      "details": {
        "database": "MySQL",
        "validationQuery": "isValid()"
      }
    },
    "diskSpace": { "status": "UP" },
    "ping": { "status": "UP" },
    "readinessState": { "status": "UP" }
  }
}
```

### Step 3: Test Product Creation and Querying

```bash
# 1. Create a Product
curl -s -X POST http://localhost:8080/api/products \
  -H "Content-Type: application/json" \
  -d '{"name": "Mechanical Keyboard", "price": 149.99}'

# 2. Query All Products
curl -s http://localhost:8080/api/products
# Output: [{"id":1,"name":"Mechanical Keyboard","price":149.99}]
```

### Step 4: Verify Effective JVM Container Memory Allocation

```bash
docker compose exec spring-app java -XX:+PrintFlagsFinal -version | grep -E "MaxHeapSize|InitialHeapSize"
```
*Notice that `MaxHeapSize` is automatically calculated to approximately 768MB (75% of the 1024MB container memory limit).*

### Step 5: Teardown

```bash
docker compose down -v
```

---

## 5. Production Considerations

1. **Layered JAR Cache Invalidation**: By copying `dependencies/` first and `application/` last, changes to application Java code build in seconds because the 150MB of third-party Spring JARs are served instantly from Docker's build cache.
2. **Graceful Shutdown (`server.shutdown: graceful`)**: Allows active HTTP requests to complete within a timeout window before terminating threads.
3. **Database Migration with Flyway/Liquibase**: In production, replace `ddl-auto: update` with versioned Flyway migrations.

---

[<- 02-python-fastapi-stack.md](./02-python-fastapi-stack.md) | [Back to Projects Overview](./README.md) | [Next Project: 04-nginx-reverse-proxy-with-tls-local.md ->](./04-nginx-reverse-proxy-with-tls-local.md)
