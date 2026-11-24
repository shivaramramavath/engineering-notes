# Production Reference Architecture: Blog & Publishing Platform

## Metadata & Production Baseline

| Attribute | Specification |
|:---|:---|
| **Project Code** | `SPRING-PROJ-002` |
| **Spring Boot Version** | `3.4.x` / `4.x` |
| **Spring Framework Version** | `6.2.x` / `7.0.x` |
| **Java Baseline** | `Java 21` / `Java 25 LTS` |
| **Build Tool** | `Maven 3.9+` |
| **Database & ORM** | `PostgreSQL 16+`, `Spring Data JPA`, `Flyway` |
| **Caching & Full-Text** | `PostgreSQL tsvector / GIN Index`, `Caffeine Cache` |
| **Testing** | `JUnit 5.11+`, `AssertJ`, `Testcontainers 1.20+`, `MockMvc` |
| **Complexity Level** | Intermediate |

---

## 1. Architectural Overview & Domain Design

### 1.1 System Architecture & Data Flow

```
                      +---------------------------------------+
                      |         Web / Mobile Client           |
                      +-------------------+-------------------+
                                          |
                                          | Keyset Paged Queries / Mutations
                                          v
                      +---------------------------------------+
                      |      ArticleController & TagController|
                      +-------------------+-------------------+
                                          |
                                          v
                      +---------------------------------------+
                      |     ArticleService (@Transactional)   |
                      +-------------------+-------------------+
                                          |
                          +---------------+---------------+
                          |                               |
                          v                               v
             +-------------------------+     +-------------------------+
             |    ArticleRepository    |     |      TagRepository      |
             |   (Many-to-Many Bridge) |     |  (Slug Unique Index)    |
             +------------+------------+     +------------+------------+
                          |                               |
                          +---------------+---------------+
                                          |
                                          v
                      +---------------------------------------+
                      |     PostgreSQL 16 Database            |
                      |   (tsvector Full-Text Search + GIN)   |
                      +---------------------------------------+
```

### 1.2 Directory Layout

```
02-blog-platform/
|-- pom.xml
|-- Dockerfile
|-- compose.yaml
`-- src/
    |-- main/
    |   |-- java/com/example/blog/
    |   |   |-- BlogApplication.java
    |   |   |-- domain/
    |   |   |   |-- Article.java
    |   |   |   |-- Tag.java
    |   |   |   `-- ArticleStatus.java
    |   |   |-- dto/
    |   |   |   |-- CreateArticleRequest.java
    |   |   |   |-- ArticleSummaryResponse.java
    |   |   |   `-- ArticleDetailResponse.java
    |   |   |-- repository/
    |   |   |   |-- ArticleRepository.java
    |   |   |   `-- TagRepository.java
    |   |   |-- service/
    |   |   |   `-- ArticleService.java
    |   |   `-- web/
    |   |       `-- ArticleController.java
    |   `-- resources/
    |       |-- application.yaml
    |       `-- db/migration/
    |           `-- V1__init_blog_schema.sql
    `-- test/
        `-- java/com/example/blog/
            `-- BlogPlatformIntegrationTest.java
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
    <artifactId>blog-publishing-platform</artifactId>
    <version>1.0.0</version>
    <name>blog-publishing-platform</name>

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

### 2.2 Database Migration (`src/main/resources/db/migration/V1__init_blog_schema.sql`)

```sql
CREATE TABLE IF NOT EXISTS tags (
    id UUID PRIMARY KEY,
    name VARCHAR(64) NOT NULL UNIQUE,
    slug VARCHAR(64) NOT NULL UNIQUE
);

CREATE TABLE IF NOT EXISTS articles (
    id UUID PRIMARY KEY,
    title VARCHAR(255) NOT NULL,
    slug VARCHAR(255) NOT NULL UNIQUE,
    content_markdown TEXT NOT NULL,
    author_id VARCHAR(64) NOT NULL,
    status VARCHAR(32) NOT NULL DEFAULT 'DRAFT',
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    version BIGINT NOT NULL DEFAULT 0
);

CREATE TABLE IF NOT EXISTS article_tags (
    article_id UUID NOT NULL REFERENCES articles (id) ON DELETE CASCADE,
    tag_id UUID NOT NULL REFERENCES tags (id) ON DELETE CASCADE,
    PRIMARY KEY (article_id, tag_id)
);

CREATE INDEX idx_articles_slug ON articles (slug);
CREATE INDEX idx_articles_status_created ON articles (status, created_at DESC);
```

### 2.3 Domain Entities & DTOs

```java
package com.example.blog.domain;

import jakarta.persistence.*;
import java.time.Instant;
import java.util.*;

@Entity
@Table(name = "tags")
public class Tag {

    @Id
    @GeneratedValue(strategy = GenerationType.UUID)
    private UUID id;

    @Column(nullable = false, unique = true, length = 64)
    private String name;

    @Column(nullable = false, unique = true, length = 64)
    private String slug;

    protected Tag() {}

    public Tag(String name, String slug) {
        this.name = Objects.requireNonNull(name);
        this.slug = Objects.requireNonNull(slug);
    }

    public UUID getId() { return id; }
    public String getName() { return name; }
    public String getSlug() { return slug; }
}
```

```java
package com.example.blog.domain;

import jakarta.persistence.*;
import java.time.Instant;
import java.util.*;

@Entity
@Table(name = "articles")
public class Article {

    @Id
    @GeneratedValue(strategy = GenerationType.UUID)
    private UUID id;

    @Column(nullable = false)
    private String title;

    @Column(nullable = false, unique = true)
    private String slug;

    @Column(name = "content_markdown", nullable = false, columnDefinition = "TEXT")
    private String contentMarkdown;

    @Column(name = "author_id", nullable = false)
    private String authorId;

    @Enumerated(EnumType.STRING)
    @Column(nullable = false)
    private ArticleStatus status;

    @ManyToMany(fetch = FetchType.LAZY)
    @JoinTable(
        name = "article_tags",
        joinColumns = @JoinColumn(name = "article_id"),
        inverseJoinColumns = @JoinColumn(name = "tag_id")
    )
    private Set<Tag> tags = new HashSet<>();

    @Column(name = "created_at", nullable = false, updatable = false)
    private Instant createdAt;

    @Column(name = "updated_at", nullable = false)
    private Instant updatedAt;

    @Version
    private Long version;

    protected Article() {}

    public Article(String title, String slug, String contentMarkdown, String authorId) {
        this.title = Objects.requireNonNull(title);
        this.slug = Objects.requireNonNull(slug);
        this.contentMarkdown = Objects.requireNonNull(contentMarkdown);
        this.authorId = Objects.requireNonNull(authorId);
        this.status = ArticleStatus.DRAFT;
        this.createdAt = Instant.now();
        this.updatedAt = Instant.now();
    }

    public void publish() {
        this.status = ArticleStatus.PUBLISHED;
        this.updatedAt = Instant.now();
    }

    public void updateContent(String title, String contentMarkdown) {
        this.title = title;
        this.contentMarkdown = contentMarkdown;
        this.updatedAt = Instant.now();
    }

    public void setTags(Set<Tag> newTags) {
        this.tags = newTags;
    }

    public UUID getId() { return id; }
    public String getTitle() { return title; }
    public String getSlug() { return slug; }
    public String getContentMarkdown() { return contentMarkdown; }
    public String getAuthorId() { return authorId; }
    public ArticleStatus getStatus() { return status; }
    public Set<Tag> getTags() { return Collections.unmodifiableSet(tags); }
    public Instant getCreatedAt() { return createdAt; }
}

public enum ArticleStatus { DRAFT, PUBLISHED, ARCHIVED }
```

```java
package com.example.blog.dto;

import com.example.blog.domain.Article;
import com.example.blog.domain.Tag;
import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.NotEmpty;
import jakarta.validation.constraints.Size;

import java.time.Instant;
import java.util.Set;
import java.util.UUID;
import java.util.stream.Collectors;

public record CreateArticleRequest(
    @NotBlank @Size(min = 5, max = 255) String title,
    @NotBlank String contentMarkdown,
    @NotBlank String authorId,
    @NotEmpty Set<String> tagNames
) {}

public record ArticleDetailResponse(
    UUID id,
    String title,
    String slug,
    String contentMarkdown,
    String authorId,
    String status,
    Set<String> tags,
    Instant createdAt
) {
    public static ArticleDetailResponse fromEntity(Article a) {
        return new ArticleDetailResponse(
            a.getId(), a.getTitle(), a.getSlug(), a.getContentMarkdown(),
            a.getAuthorId(), a.getStatus().name(),
            a.getTags().stream().map(Tag::getName).collect(Collectors.toSet()),
            a.getCreatedAt()
        );
    }
}
```

### 2.4 Repositories & Service

```java
package com.example.blog.repository;

import com.example.blog.domain.Article;
import com.example.blog.domain.ArticleStatus;
import org.springframework.data.domain.Pageable;
import org.springframework.data.domain.Slice;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Query;
import org.springframework.data.repository.query.Param;

import java.time.Instant;
import java.util.Optional;
import java.util.UUID;

public interface ArticleRepository extends JpaRepository<Article, UUID> {

    @Query("SELECT a FROM Article a LEFT JOIN FETCH a.tags WHERE a.slug = :slug")
    Optional<Article> findBySlugWithTags(@Param("slug") String slug);

    // Keyset pagination (cursor-based pagination for high performance)
    @Query("""
        SELECT a FROM Article a 
        WHERE a.status = :status AND a.createdAt < :cursor 
        ORDER BY a.createdAt DESC
        """)
    Slice<Article> findPublishedBefore(@Param("status") ArticleStatus status, @Param("cursor") Instant cursor, Pageable pageable);
}
```

```java
package com.example.blog.service;

import com.example.blog.domain.Article;
import com.example.blog.domain.Tag;
import com.example.blog.dto.ArticleDetailResponse;
import com.example.blog.dto.CreateArticleRequest;
import com.example.blog.repository.ArticleRepository;
import com.example.blog.repository.TagRepository;
import org.springframework.http.HttpStatus;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;
import org.springframework.web.server.ResponseStatusException;

import java.text.Normalizer;
import java.util.*;

@Service
@Transactional
public class ArticleService {

    private final ArticleRepository articleRepository;
    private final TagRepository tagRepository;

    public ArticleService(ArticleRepository articleRepository, TagRepository tagRepository) {
        this.articleRepository = articleRepository;
        this.tagRepository = tagRepository;
    }

    public ArticleDetailResponse createArticle(CreateArticleRequest req) {
        String slug = toSlug(req.title()) + "-" + UUID.randomUUID().toString().substring(0, 6);

        Article article = new Article(req.title(), slug, req.contentMarkdown(), req.authorId());

        Set<Tag> resolvedTags = new HashSet<>();
        for (String tagName : req.tagNames()) {
            Tag tag = tagRepository.findByName(tagName)
                    .orElseGet(() -> tagRepository.save(new Tag(tagName, toSlug(tagName))));
            resolvedTags.add(tag);
        }
        article.setTags(resolvedTags);

        Article saved = articleRepository.save(article);
        return ArticleDetailResponse.fromEntity(saved);
    }

    @Transactional(readOnly = true)
    public ArticleDetailResponse getBySlug(String slug) {
        return articleRepository.findBySlugWithTags(slug)
                .map(ArticleDetailResponse::fromEntity)
                .orElseThrow(() -> new ResponseStatusException(HttpStatus.NOT_FOUND, "Article not found: " + slug));
    }

    public void publishArticle(UUID id) {
        Article article = articleRepository.findById(id)
                .orElseThrow(() -> new ResponseStatusException(HttpStatus.NOT_FOUND));
        article.publish();
    }

    private String toSlug(String input) {
        return Normalizer.normalize(input, Normalizer.Form.NFD)
                .replaceAll("[^\\w\\s-]", "")
                .trim()
                .toLowerCase()
                .replaceAll("\\s+", "-");
    }
}
```

### 2.5 REST Controller

```java
package com.example.blog.web;

import com.example.blog.dto.ArticleDetailResponse;
import com.example.blog.dto.CreateArticleRequest;
import com.example.blog.service.ArticleService;
import jakarta.validation.Valid;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.net.URI;
import java.util.UUID;

@RestController
@RequestMapping("/api/v1/articles")
public class ArticleController {

    private final ArticleService articleService;

    public ArticleController(ArticleService articleService) {
        this.articleService = articleService;
    }

    @PostMapping
    public ResponseEntity<ArticleDetailResponse> create(@Valid @RequestBody CreateArticleRequest request) {
        ArticleDetailResponse response = articleService.createArticle(request);
        return ResponseEntity.created(URI.create("/api/v1/articles/" + response.slug())).body(response);
    }

    @GetMapping("/{slug}")
    public ResponseEntity<ArticleDetailResponse> get(@PathVariable String slug) {
        return ResponseEntity.ok(articleService.getBySlug(slug));
    }

    @PostMapping("/{id}/publish")
    public ResponseEntity<Void> publish(@PathVariable UUID id) {
        articleService.publishArticle(id);
        return ResponseEntity.noContent().build();
    }
}
```

---

## 3. Automated Testing Suite

```java
package com.example.blog;

import com.example.blog.dto.ArticleDetailResponse;
import com.example.blog.dto.CreateArticleRequest;
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

import java.util.Set;

import static org.assertj.core.api.Assertions.assertThat;

@SpringBootTest(webEnvironment = SpringBootTest.WebEnvironment.RANDOM_PORT)
@Testcontainers
class BlogPlatformIntegrationTest {

    @Container
    @ServiceConnection
    static PostgreSQLContainer<?> postgres = new PostgreSQLContainer<>("postgres:16-alpine");

    @Autowired private TestRestTemplate restTemplate;

    @Test
    void shouldCreateArticleAndResolveTagsCorrectly() {
        CreateArticleRequest req = new CreateArticleRequest(
            "Building Scalable Systems in Java 21",
            "# Modern Architecture\nVirtual threads make I/O high-throughput.",
            "author-42",
            Set.of("Java", "Architecture", "SpringBoot")
        );

        ResponseEntity<ArticleDetailResponse> resp = restTemplate.postForEntity(
            "/api/v1/articles", req, ArticleDetailResponse.class
        );

        assertThat(resp.getStatusCode()).isEqualTo(HttpStatus.CREATED);
        ArticleDetailResponse body = resp.getBody();
        assertThat(body).isNotNull();
        assertThat(body.tags()).containsExactlyInAnyOrder("Java", "Architecture", "SpringBoot");
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
  blog-api:
    build: .
    ports:
      - "8080:8080"
    environment:
      - SPRING_DATASOURCE_URL=jdbc:postgresql://postgres-blog:5432/blog_db
      - SPRING_DATASOURCE_USERNAME=blog_user
      - SPRING_DATASOURCE_PASSWORD=blog_secret
    depends_on:
      postgres-blog:
        condition: service_healthy

  postgres-blog:
    image: postgres:16-alpine
    environment:
      - POSTGRES_DB=blog_db
      - POSTGRES_USER=blog_user
      - POSTGRES_PASSWORD=blog_secret
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U blog_user -d blog_db"]
      interval: 5s
      timeout: 5s
      retries: 5
```

---

## 5. Operational Runbook & Verification

```bash
# 1. Spin up stack
docker compose up -d

# 2. Create article with tags
curl -X POST http://localhost:8080/api/v1/articles \
     -H "Content-Type: application/json" \
     -d '{
       "title": "Introduction to Spring Boot 3.4",
       "contentMarkdown": "# Hello World\nTesting out the new features.",
       "authorId": "usr-101",
       "tagNames": ["Java", "Spring", "Backend"]
     }' | jq .

# 3. Retrieve article by slug
curl -s http://localhost:8080/api/v1/articles/introduction-to-spring-boot-34-xxxxxx | jq .
```
