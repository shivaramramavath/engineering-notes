# Production Reference Architecture: High-Performance URL Shortener

## Metadata & Production Baseline

| Attribute | Specification |
|:---|:---|
| **Project Code** | `SPRING-PROJ-006` |
| **Spring Boot Version** | `3.4.x` / `4.x` |
| **Spring Framework Version** | `6.2.x` / `7.0.x` |
| **Java Baseline** | `Java 21` / `Java 25 LTS` |
| **Build Tool** | `Maven 3.9+` |
| **Caching & Storage** | `Redis 7.2+` (Sub-millisecond 307 Redirects), `PostgreSQL 16+` (Persistent Store) |
| **Encoding Algorithm** | `Base62` bi-directional conversion from 64-bit integer ID |
| **Testing** | `JUnit 5.11+`, `Testcontainers` (PostgreSQL & Redis) |
| **Complexity Level** | Intermediate |

---

## 1. Architectural Overview & Domain Design

### 1.1 System Architecture & Data Flow

```
                      +---------------------------------------+
                      |       Client Browser / HTTP Client    |
                      +-------------------+-------------------+
                                          |
                                          | GET /{shortCode} (e.g. /7bXyZ)
                                          v
                      +---------------------------------------+
                      |        UrlRedirectController          |
                      +-------------------+-------------------+
                                          |
                                          | 1. Checks Redis Cache (<1ms)
                                          v
                      +---------------------------------------+
                      |          Redis Key-Value Cache        |
                      +-------------------+-------------------+
                                          |
                          +---------------+---------------+
                          |                               |
                      (CACHE HIT)                    (CACHE MISS)
                          |                               |
                          v                               v
             +-------------------------+     +-------------------------+
             | 307 Temporary Redirect  |     | Query PostgreSQL        |
             | Immediate HTTP response |     | Populate Redis Cache    |
             +-------------------------+     +-------------------------+
```

### 1.2 Directory Layout

```
06-url-shortener-with-redis/
|-- pom.xml
|-- Dockerfile
|-- compose.yaml
`-- src/
    |-- main/
    |   |-- java/com/example/shortener/
    |   |   |-- ShortenerApplication.java
    |   |   |-- domain/
    |   |   |   `-- ShortUrl.java
    |   |   |-- dto/
    |   |   |   |-- ShortenRequest.java
    |   |   |   `-- ShortenResponse.java
    |   |   |-- util/
    |   |   |   `-- Base62Encoder.java
    |   |   |-- repository/
    |   |   |   `-- ShortUrlRepository.java
    |   |   |-- service/
    |   |   |   `-- UrlShortenerService.java
    |   |   `-- web/
    |   |       `-- UrlController.java
    |   `-- resources/
    |       |-- application.yaml
    |       `-- db/migration/
    |           `-- V1__init_url_schema.sql
    `-- test/
        `-- java/com/example/shortener/
            `-- UrlShortenerIntegrationTest.java
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
    <artifactId>url-shortener-service</artifactId>
    <version>1.0.0</version>
    <name>url-shortener-service</name>

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
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-data-redis</artifactId>
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

### 2.2 Base62 Encoder Utility

```java
package com.example.shortener.util;

public final class Base62Encoder {

    private static final String ALPHABET = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    private static final int BASE = ALPHABET.length();

    private Base62Encoder() {}

    public static String encode(long value) {
        if (value == 0) return "0";
        StringBuilder sb = new StringBuilder();
        long num = value;
        while (num > 0) {
            sb.append(ALPHABET.charAt((int) (num % BASE)));
            num /= BASE;
        }
        return sb.reverse().toString();
    }

    public static long decode(String str) {
        long num = 0;
        for (int i = 0; i < str.length(); i++) {
            num = num * BASE + ALPHABET.indexOf(str.charAt(i));
        }
        return num;
    }
}
```

### 2.3 Domain Entity & Service with Redis Caching

```java
package com.example.shortener.domain;

import jakarta.persistence.*;
import java.time.Instant;

@Entity
@Table(name = "short_urls")
public class ShortUrl {

    @Id
    @GeneratedValue(strategy = GenerationType.SEQUENCE, generator = "url_seq")
    @SequenceGenerator(name = "url_seq", sequenceName = "short_url_id_seq", allocationSize = 1, initialValue = 1000000)
    private Long id;

    @Column(name = "short_code", nullable = false, unique = true, length = 16)
    private String shortCode;

    @Column(name = "original_url", nullable = false, columnDefinition = "TEXT")
    private String originalUrl;

    @Column(name = "created_at", nullable = false, updatable = false)
    private Instant createdAt;

    protected ShortUrl() {}

    public ShortUrl(String originalUrl) {
        this.originalUrl = originalUrl;
        this.createdAt = Instant.now();
    }

    public void setShortCode(String shortCode) {
        this.shortCode = shortCode;
    }

    public Long getId() { return id; }
    public String getShortCode() { return shortCode; }
    public String getOriginalUrl() { return originalUrl; }
}
```

```java
package com.example.shortener.service;

import com.example.shortener.domain.ShortUrl;
import com.example.shortener.repository.ShortUrlRepository;
import com.example.shortener.util.Base62Encoder;
import org.springframework.data.redis.core.StringRedisTemplate;
import org.springframework.http.HttpStatus;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;
import org.springframework.web.server.ResponseStatusException;

import java.time.Duration;

@Service
public class UrlShortenerService {

    private final ShortUrlRepository repository;
    private final StringRedisTemplate redis;

    public UrlShortenerService(ShortUrlRepository repository, StringRedisTemplate redis) {
        this.repository = repository;
        this.redis = redis;
    }

    @Transactional
    public String shorten(String originalUrl) {
        // 1. Insert row to allocate sequence ID
        ShortUrl entity = new ShortUrl(originalUrl);
        ShortUrl saved = repository.save(entity);

        // 2. Base62 encode primary key ID
        String shortCode = Base62Encoder.encode(saved.getId());
        saved.setShortCode(shortCode);

        // 3. Warm Redis cache for 7 days
        redis.opsForValue().set("url:" + shortCode, originalUrl, Duration.ofDays(7));

        return shortCode;
    }

    public String resolveOriginalUrl(String shortCode) {
        // Step 1: Redis Cache Read (<1ms)
        String cached = redis.opsForValue().get("url:" + shortCode);
        if (cached != null) {
            return cached;
        }

        // Step 2: Database fallback
        ShortUrl entity = repository.findByShortCode(shortCode)
                .orElseThrow(() -> new ResponseStatusException(HttpStatus.NOT_FOUND, "Short URL does not exist"));

        // Step 3: Populate Redis for future callers
        redis.opsForValue().set("url:" + shortCode, entity.getOriginalUrl(), Duration.ofDays(7));
        return entity.getOriginalUrl();
    }
}
```

### 2.4 REST Controller (307 Temporary Redirect)

```java
package com.example.shortener.web;

import com.example.shortener.service.UrlShortenerService;
import jakarta.validation.constraints.NotBlank;
import org.hibernate.validator.constraints.URL;
import org.springframework.http.HttpHeaders;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.net.URI;
import java.util.Map;

public record ShortenRequest(@NotBlank @URL(message = "Invalid URL format") String originalUrl) {}

@RestController
public class UrlController {

    private final UrlShortenerService shortenerService;

    public UrlController(UrlShortenerService shortenerService) {
        this.shortenerService = shortenerService;
    }

    @PostMapping("/api/v1/urls")
    public ResponseEntity<Map<String, String>> createShortUrl(@RequestBody ShortenRequest request) {
        String shortCode = shortenerService.shorten(request.originalUrl());
        return ResponseEntity.status(HttpStatus.CREATED).body(Map.of(
            "shortCode", shortCode,
            "shortUrl", "http://localhost:8080/" + shortCode
        ));
    }

    // High-performance 307 Temporary Redirect
    @GetMapping("/{shortCode}")
    public ResponseEntity<Void> redirectToOriginal(@PathVariable String shortCode) {
        String originalUrl = shortenerService.resolveOriginalUrl(shortCode);
        return ResponseEntity.status(HttpStatus.TEMPORARY_REDIRECT)
                .location(URI.create(originalUrl))
                .header(HttpHeaders.CACHE_CONTROL, "no-cache")
                .build();
    }
}
```

---

## 3. Automated Testing Suite

```java
package com.example.shortener;

import com.example.shortener.service.UrlShortenerService;
import com.example.shortener.util.Base62Encoder;
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

import java.util.Map;

import static org.assertj.core.api.Assertions.assertThat;

@SpringBootTest(webEnvironment = SpringBootTest.WebEnvironment.RANDOM_PORT)
@Testcontainers
class UrlShortenerIntegrationTest {

    @Container
    @ServiceConnection
    static PostgreSQLContainer<?> postgres = new PostgreSQLContainer<>("postgres:16-alpine");

    @Autowired private TestRestTemplate restTemplate;

    @Test
    void shouldCreateShortUrlAndRedirect() {
        ResponseEntity<Map> createResp = restTemplate.postForEntity(
                "/api/v1/urls",
                Map.of("originalUrl", "https://spring.io/projects/spring-boot"),
                Map.class
        );
        assertThat(createResp.getStatusCode()).isEqualTo(HttpStatus.CREATED);
        String shortCode = (String) createResp.getBody().get("shortCode");

        // Verify redirect
        ResponseEntity<Void> redirectResp = restTemplate.getForEntity("/" + shortCode, Void.class);
        assertThat(redirectResp.getStatusCode()).isEqualTo(HttpStatus.TEMPORARY_REDIRECT);
        assertThat(redirectResp.getHeaders().getLocation().toString()).isEqualTo("https://spring.io/projects/spring-boot");
    }

    @Test
    void testBase62EncodingBiDirectional() {
        long original = 98124912L;
        String encoded = Base62Encoder.encode(original);
        long decoded = Base62Encoder.decode(encoded);
        assertThat(decoded).isEqualTo(original);
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
  url-shortener:
    build: .
    ports:
      - "8080:8080"
    environment:
      - SPRING_DATASOURCE_URL=jdbc:postgresql://postgres-url:5432/shortener_db
      - SPRING_DATA_REDIS_HOST=redis-url
    depends_on:
      - postgres-url
      - redis-url

  postgres-url:
    image: postgres:16-alpine
    environment:
      - POSTGRES_DB=shortener_db
      - POSTGRES_USER=shortener_user
      - POSTGRES_PASSWORD=shortener_secret

  redis-url:
    image: redis:7.2-alpine
    ports:
      - "6379:6379"
```

---

## 5. Operational Runbook & Verification

```bash
# 1. Shorten URL
curl -X POST http://localhost:8080/api/v1/urls \
     -H "Content-Type: application/json" \
     -d '{"originalUrl": "https://docs.spring.io/spring-boot/docs/current/reference/html/"}' | jq .

# 2. Test 307 Temporary Redirect
curl -I http://localhost:8080/4c91
```
