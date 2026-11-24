# 06. Testcontainers & @ServiceConnection in Spring Boot

| Metadata | Value |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | `01-testing-fundamentals-in-spring.md`, Docker Basics, Spring Data JDBC/JPA |
| **Estimated Reading Time** | 40 minutes |
| **Status** | Complete |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | OpenJDK 21, Spring Boot 3.4.1, Testcontainers 1.20 |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance & Security Notes](#8-performance--security-notes)
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### Problem Statement
For decades, developers relied on in-memory mocks and embedded databases (like H2 or Fongo) for integration testing. While fast, this approach introduces critical risks:
- H2 lacks PostgreSQL-specific data types (JSONB, UUID, Array types, geometric types).
- SQL dialects, full-text search syntax, locking behavior, and transaction isolation levels differ significantly between embedded mocks and production engines.
- In-memory embedded brokers (such as embedded Kafka or mock Redis) fail to replicate real network latency, cluster rebalancing, or partition leadership behavior.
- Tests pass in CI using H2, but immediately fail upon deployment to production against real PostgreSQL or MySQL databases.

**Testcontainers** provides lightweight, throwaway instances of real Docker containers (PostgreSQL, Redis, Kafka, RabbitMQ, Elasticsearch, LocalStack) directly managed within JUnit test lifecycles.

In **Spring Boot 3.1+**, the framework introduced **`@ServiceConnection`**, completely eliminating verbose `@DynamicPropertySource` boilerplate and automatically wiring container connection properties into Spring Boot.

### Historical Evolution
- **Testcontainers 1.x (2015-2022)**: Managed Docker containers via Java, but required verbose `@DynamicPropertySource` methods to manually map dynamic host ports to `spring.datasource.url`, `spring.data.redis.port`, etc.
- **Spring Boot 3.1**: Introduced `@ServiceConnection`, which automatically discovers container types and binds their connection details directly to the Spring environment.
- **Spring Boot 3.4+ / 4.x**: Extended `@ServiceConnection` support to over 25 container types (including Cassandra, Couchbase, Neo4j, OpenSearch, Zipkin) and introduced development-time Testcontainers via `TestcontainersConfiguration`.

### Real-World Relevance
Testcontainers is the gold standard for high-confidence integration testing across enterprise Java ecosystems, ensuring tests run against the exact same database engines and middleware used in production.

---

## 2. Core Concepts

### Architectural Contracts
- **`org.testcontainers.containers.GenericContainer`**: Base class representing any Docker container image.
- **Specialized Modules**: Optimized pre-packaged container classes (`PostgreSQLContainer`, `KafkaContainer`, `RedisContainer`).
- **`@Testcontainers`**: JUnit 5 extension that manages the lifecycle of annotated container fields.
- **`@Container`**: Marks a container field for automated startup and teardown by the JUnit extension.
- **`@ServiceConnection`**: Spring Boot annotation that inspects the running container and registers connection properties (`ConnectionDetails`) automatically into the `ApplicationContext`.
- **Ryuk Container**: An internal cleanup container automatically launched by Testcontainers to guarantee that all test containers are killed and pruned, even if the JVM crashes abruptly.
- **Container Reuse**: Feature allowing developer machines to reuse already-running containers across test runs, slashing startup latency to near zero.

### The Mental Model
```
[ JUnit 5 Test Class ]
         │
         ├── 1. @Container @ServiceConnection
         │      static PostgreSQLContainer<?> postgres = new PostgreSQLContainer<>("postgres:16");
         │
         ▼
[ Docker Daemon ] ──► Starts real PostgreSQL container on random port (e.g., 5432 -> 32789)
         │
         ▼
[ Spring Boot ApplicationContext ]
         │
         ├── 2. @ServiceConnection detects PostgreSQLContainer:
         │      Automatically configures:
         │        spring.datasource.url = jdbc:postgresql://localhost:32789/test
         │        spring.datasource.username = test
         │        spring.datasource.password = test
         │
         ▼
[ Integration Test Executes against REAL PostgreSQL! ]
```

### Invariants
1. Containers declared `static` are shared across all test methods in a class (or across classes if using singleton patterns), starting once before all tests.
2. Containers declared non-static start and stop for **every individual test method**, introducing severe execution time penalties.
3. `@ServiceConnection` takes precedence over properties defined in `application.yml` or `@TestPropertySource`.

---

## 3. How It Works Internally

### The `@ServiceConnection` Auto-Discovery Mechanism
1. Spring Boot provides a service loader mechanism for **`ContainerConnectionDetailsFactory`**.
2. When the test context prepares beans, `ServiceConnectionContextCustomizer` locates all fields annotated with `@ServiceConnection`.
3. It queries registered factories (e.g., `PostgreSqlContainerConnectionDetailsFactory`).
4. The factory queries the container instance (`postgres.getJdbcUrl()`, `postgres.getUsername()`, etc.).
5. Constructs a `JdbcConnectionDetails` bean and registers it in the `BeanFactory`.
6. Spring Boot's `DataSourceAutoConfiguration` uses `JdbcConnectionDetails` directly, completely bypassing property parsing.

---

## 4. Syntax & Usage

### 1. Minimal Maven Dependencies (`pom.xml`)
```xml
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-testcontainers</artifactId>
    <scope>test</scope>
</dependency>
<dependency>
    <groupId>org.testcontainers</groupId>
    <artifactId>junit-jupiter</artifactId>
    <scope>test</scope>
</dependency>
<dependency>
    <groupId>org.testcontainers</groupId>
    <artifactId>postgresql</artifactId>
    <scope>test</scope>
</dependency>
```

### 2. Standard `@ServiceConnection` Usage
```java
package com.example.testing;

import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.boot.testcontainers.service.connection.ServiceConnection;
import org.testcontainers.containers.PostgreSQLContainer;
import org.testcontainers.junit.jupiter.Container;
import org.testcontainers.junit.jupiter.Testcontainers;

import javax.sql.DataSource;
import java.sql.Connection;

import static org.assertj.core.api.Assertions.assertThat;

@SpringBootTest
@Testcontainers
class PostgresServiceConnectionTest {

    @Container
    @ServiceConnection // No @DynamicPropertySource required!
    static PostgreSQLContainer<?> postgres = new PostgreSQLContainer<>("postgres:16-alpine");

    @Autowired
    private DataSource dataSource;

    @Test
    void shouldConnectToRealPostgresContainer() throws Exception {
        try (Connection connection = dataSource.getConnection()) {
            assertThat(connection.getMetaData().getDatabaseProductName()).isEqualTo("PostgreSQL");
        }
    }
}
```

---

## 5. Practical Examples

### Example 1: Multi-Container Setup (PostgreSQL + Redis)
An integration test verifying data persistence in PostgreSQL alongside distributed caching in Redis using multiple `@ServiceConnection` instances.

```java
package com.example.testing;

import com.example.service.ProductCatalogService;
import com.redis.testcontainers.RedisContainer;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.boot.testcontainers.service.connection.ServiceConnection;
import org.testcontainers.containers.PostgreSQLContainer;
import org.testcontainers.junit.jupiter.Container;
import org.testcontainers.junit.jupiter.Testcontainers;

import static org.assertj.core.api.Assertions.assertThat;

@SpringBootTest
@Testcontainers
class MultiContainerCatalogTest {

    @Container
    @ServiceConnection
    static PostgreSQLContainer<?> postgres = new PostgreSQLContainer<>("postgres:16-alpine");

    @Container
    @ServiceConnection
    static RedisContainer redis = new RedisContainer("redis:7.2-alpine");

    @Autowired
    private ProductCatalogService catalogService;

    @Test
    void shouldPersistInPostgresAndCacheInRedis() {
        catalogService.createProduct("P-100", "Ergonomic Chair", 250.0);

        // First call fetches from PostgreSQL and populates Redis cache
        var product1 = catalogService.getProduct("P-100");
        // Second call served directly from Redis
        var product2 = catalogService.getProduct("P-100");

        assertThat(product1.name()).isEqualTo("Ergonomic Chair");
        assertThat(product2).isEqualTo(product1);
    }
}
```

### Example 2: Event-Driven Kafka Testing with `@ServiceConnection`
Testing message production and consumption using real Apache Kafka containers.

```java
package com.example.testing;

import com.example.event.OrderEventConsumer;
import com.example.event.OrderEventProducer;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.boot.testcontainers.service.connection.ServiceConnection;
import org.testcontainers.containers.KafkaContainer;
import org.testcontainers.junit.jupiter.Container;
import org.testcontainers.junit.jupiter.Testcontainers;
import org.testcontainers.utility.DockerImageName;

import java.time.Duration;

import static org.awaitility.Awaitility.await;

@SpringBootTest
@Testcontainers
class KafkaIntegrationTest {

    @Container
    @ServiceConnection
    static KafkaContainer kafka = new KafkaContainer(
        DockerImageName.parse("confluentinc/cp-kafka:7.6.1")
    );

    @Autowired
    private OrderEventProducer producer;

    @Autowired
    private OrderEventConsumer consumer;

    @Test
    void shouldPublishAndConsumeKafkaMessage() {
        producer.sendOrderCreated("ORD-555");

        await().atMost(Duration.ofSeconds(10))
            .untilAsserted(() -> assertThat(consumer.getReceivedOrders()).contains("ORD-555"));
    }
}
```

### Example 3: Edge Case – The Singleton Container Pattern (Zero Context Reboots)
Sharing a single set of containers across **all** test classes in the test suite to maximize Spring context caching and eliminate container startup overhead.

```java
package com.example.testing.base;

import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.boot.testcontainers.service.connection.ServiceConnection;
import org.testcontainers.containers.PostgreSQLContainer;

@SpringBootTest
public abstract class AbstractSharedContainerTest {

    // Started manually once in static initializer -> shared across ALL test classes!
    @ServiceConnection
    protected static final PostgreSQLContainer<?> POSTGRES;

    static {
        POSTGRES = new PostgreSQLContainer<>("postgres:16-alpine")
            .withReuse(true); // Enables local container reuse
        POSTGRES.start();
    }
}
```
```java
// Subclasses simply extend the base class:
class UserIntegrationTest extends AbstractSharedContainerTest {
    @Test void testUser() {}
}

class BillingIntegrationTest extends AbstractSharedContainerTest {
    @Test void testBilling() {}
}
```

### Example 4: Failure Anti-Pattern & Production Fix
**Anti-Pattern**: Using non-static container definitions with `@Container`, forcing Docker to spin up and tear down fresh containers for every individual test method.

```java
// SEVERE PERFORMANCE ANTI-PATTERN:
// Spends 15 seconds starting and stopping Docker containers for EACH test method!
@Testcontainers
class SlowTest {
    @Container
    PostgreSQLContainer<?> postgres = new PostgreSQLContainer<>("postgres:16"); // Non-static!

    @Test void testA() {}
    @Test void testB() {}
}
```

**Production Fix**:
Always declare container instances as `static`.

```java
// FAST & REUSABLE: Container starts ONCE for the entire test class!
@Testcontainers
class FastTest {
    @Container
    static PostgreSQLContainer<?> postgres = new PostgreSQLContainer<>("postgres:16");
}
```

### Example 5: Development-Time Testcontainers (`TestcontainersConfiguration`)
Running your Spring Boot application locally during development against live Docker containers without writing docker-compose files.

```java
package com.example;

import org.springframework.boot.SpringApplication;
import org.springframework.boot.test.context.TestConfiguration;
import org.springframework.boot.testcontainers.service.connection.ServiceConnection;
import org.springframework.context.annotation.Bean;
import org.testcontainers.containers.PostgreSQLContainer;

@TestConfiguration(proxyBeanMethods = false)
public class TestcontainersConfiguration {

    @Bean
    @ServiceConnection
    public PostgreSQLContainer<?> postgresContainer() {
        return new PostgreSQLContainer<>("postgres:16-alpine").withReuse(true);
    }
}

// Development Runner Application:
class TestApplication {
    public static void main(String[] args) {
        SpringApplication.from(Application::main)
            .with(TestcontainersConfiguration.class)
            .run(args);
    }
}
```

---

## 6. Best Practices

1. **Always Declare Containers as `static`**: Guarantees containers are started once per test class rather than per test method.
2. **Use `@ServiceConnection` Everywhere**: Eliminates fragile `@DynamicPropertySource` string mappings.
3. **Use the Singleton Container Pattern for Large Suites**: Start containers in a shared static base class to share containers across the entire test suite.
4. **Use Lightweight Alpine Images**: Use `postgres:16-alpine` or `redis:7.2-alpine` to reduce container download times and memory footprint.
5. **Enable Container Reuse Locally**: Configure `testcontainers.reuse.enable=true` in `~/.testcontainers.properties` and add `.withReuse(true)` to skip restarts during local TDD cycles.
6. **Use Specific Image Tags**: Never use `latest` tags; pin exact container tags (e.g., `postgres:16.4-alpine`) to ensure deterministic test builds.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Non-static container field | Containers restart for every single test method, multiplying test times by 10x. | Add `static` modifier to the `@Container` field. |
| Using `latest` image tags | Docker pulls unpredictable upstream image updates that can break test compatibility. | Pin exact image versions: `postgres:16.4-alpine`. |
| Port collision with local services | Mapping fixed host ports (`.withFixedExposedPort(5432, 5432)`) collides with local dev databases. | Let Testcontainers assign dynamic random host ports. |
| Missing `@Testcontainers` annotation | JUnit 5 ignores `@Container` fields; container is never started, leading to connection timeouts. | Ensure class is annotated with `@Testcontainers`. |
| Docker socket permission denied | CI build agent fails to communicate with `/var/run/docker.sock`. | Grant docker socket permissions or configure `DOCKER_HOST`. |
| OOM in CI from too many containers | Running multiple heavy containers simultaneously exhausts runner memory. | Share containers using the singleton pattern. |

---

## 8. Performance & Security Notes

- **Ryuk Cleanup Safety**: Testcontainers automatically runs the `testcontainers/ryuk` watchdog container. Even if tests are forcefully aborted (`kill -9` or CI timeout), Ryuk cleans up orphaned test containers and networks.
- **Local Container Reuse**: Adding `testcontainers.reuse.enable=true` to `~/.testcontainers.properties` allows developers to restart tests in < 500ms by connecting to already-running containers.
- **Virtual Threads Integration**: Database and messaging network I/O to container ports unmounts carrier threads smoothly in Java 21+.

---

## 9. Comparison / When to Use What

| Feature | In-Memory (H2 / Embedded Kafka) | Testcontainers with `@ServiceConnection` | Dedicated Remote Test Env |
|---|---|---|---|---|
| **Real Engine Fidelity**| Low (Dialect differences) | **100% (Real production image)** | 100% |
| **Startup Speed** | Instant (< 50ms) | Fast (1-3s with reuse) | Zero startup |
| **Isolation** | Moderate | **Maximum (Isolated container)** | Poor (Shared state) |
| **Maintenance** | Low | Low (Managed in code) | High (Infrastructure ops) |

---

## 10. Interview Questions

<details>
<summary>1. What problem does Spring Boot 3.1's `@ServiceConnection` solve?</summary>

**Answer:**
Prior to Spring Boot 3.1, using Testcontainers required writing boilerplate `@DynamicPropertySource` methods to manually extract dynamic container host ports and map them to Spring properties (e.g., `registry.add("spring.datasource.url", container::getJdbcUrl)`).
`@ServiceConnection` automates this entirely: it inspects the container type, discovers its connection details, and registers a `ConnectionDetails` bean directly into the `ApplicationContext`, eliminating all manual property wiring.
</details>

<details>
<summary>2. Why should container fields annotated with `@Container` almost always be `static`?</summary>

**Answer:**
If a container field is non-static (instance-level), JUnit 5's `@Testcontainers` extension stops and recreates the container before and after **every single test method**. Since starting a container takes several seconds, a test class with 5 methods would take over a minute to run. Declaring the container `static` ensures it starts once before all test methods in the class and stops after the class finishes.
</details>

<details>
<summary>3. What is the role of the Ryuk container in Testcontainers?</summary>

**Answer:**
Ryuk is an automated cleanup container started by Testcontainers. It establishes a socket connection with the Testcontainers Java library. If the JVM terminates abnormally, crashes with an OutOfMemory error, or is cancelled by a CI pipeline, Ryuk detects the dead socket and immediately deletes all containers, networks, and volumes created during that test session, preventing Docker resource leaks.
</details>

<details>
<summary>4. How does the Singleton Container Pattern improve test suite performance?</summary>

**Answer:**
Instead of starting separate containers per test class, the Singleton Container Pattern initializes containers once in a shared static base class and starts them manually via `.start()`. All integration test classes inherit from this base class, sharing the running containers and preserving Spring's `ApplicationContext` cache, which reduces test suite execution times from minutes to seconds.
</details>

<details>
<summary>5. How can you use Testcontainers for local development instead of just tests?</summary>

**Answer:**
Spring Boot 3.1+ introduced development-time Testcontainers. By creating a `@TestConfiguration` class with `@ServiceConnection` container beans and launching the app via `SpringApplication.from(Main::main).with(TestcontainersConfiguration.class).run(args)`, developers can run the real application locally against throwaway Docker containers without manually setting up Docker Compose.
</details>

<details>
<summary>6. How does container reuse work in Testcontainers?</summary>

**Answer:**
By adding `.withReuse(true)` to a container and setting `testcontainers.reuse.enable=true` in `~/.testcontainers.properties`, Testcontainers computes a hash of the container configuration. If a matching container is already running on the local Docker daemon, Testcontainers attaches to it immediately instead of creating a new one, reducing startup time to near zero.
</details>

<details>
<summary>7. What happens if `@DataJpaTest` is used with `@ServiceConnection` and a PostgreSQL container?</summary>

**Answer:**
By default, `@DataJpaTest` attempts to replace the database with embedded H2. When using `@ServiceConnection` with PostgreSQL, you must explicitly annotate the test class with `@AutoConfigureTestDatabase(replace = AutoConfigureTestDatabase.Replace.NONE)` to instruct Spring not to replace the container DataSource.
</details>

<details>
<summary>8. How do you test Kafka consumer offsets and lag using Testcontainers?</summary>

**Answer:**
By launching a real `KafkaContainer`, publishing messages via `KafkaTemplate`, and using an assertion library like **Awaitility** to wait until the consumer processes the records and commits its consumer offsets.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Basic PostgreSQL `@ServiceConnection` (Easy)</summary>

**Task**: Write an integration test using Spring Boot and Testcontainers that starts a PostgreSQL container and verifies that a JPA repository can save and find an entity.

**Solution**:
```java
@SpringBootTest
@Testcontainers
class PostgresRepositoryIntegrationTest {

    @Container
    @ServiceConnection
    static PostgreSQLContainer<?> postgres = new PostgreSQLContainer<>("postgres:16-alpine");

    @Autowired
    private ItemRepository repository;

    @Test
    void shouldSaveAndFindItem() {
        Item item = repository.save(new Item("Widget", 19.99));
        assertThat(repository.findById(item.getId())).isPresent();
    }
}
```
</details>

<details>
<summary>Exercise 2: Redis `@ServiceConnection` Test (Easy)</summary>

**Task**: Create a test verifying that `StringRedisTemplate` can store and retrieve a key-value pair from a real Redis container.

**Solution**:
```java
@SpringBootTest
@Testcontainers
class RedisServiceConnectionTest {

    @Container
    @ServiceConnection
    static RedisContainer redis = new RedisContainer("redis:7.2-alpine");

    @Autowired
    private StringRedisTemplate redisTemplate;

    @Test
    void shouldSetAndGetValue() {
        redisTemplate.opsForValue().set("greeting", "Hello Redis");
        assertThat(redisTemplate.opsForValue().get("greeting")).isEqualTo("Hello Redis");
    }
}
```
</details>

<details>
<summary>Exercise 3: Custom Initialization Script (Medium)</summary>

**Task**: Configure a `PostgreSQLContainer` to execute an initialization SQL script (`schema.sql`) from the test classpath on startup.

**Solution**:
```java
@Container
@ServiceConnection
static PostgreSQLContainer<?> postgres = new PostgreSQLContainer<>("postgres:16-alpine")
    .withInitScript("test-schema.sql");
```
</details>

<details>
<summary>Exercise 4: RabbitMQ Messaging Verification (Medium)</summary>

**Task**: Configure a `RabbitMQContainer` with `@ServiceConnection` and verify that a message published via `RabbitTemplate` is received by a `@RabbitListener`.

**Solution**:
```java
@SpringBootTest
@Testcontainers
class RabbitIntegrationTest {

    @Container
    @ServiceConnection
    static RabbitMQContainer rabbit = new RabbitMQContainer("rabbitmq:3.13-alpine");

    @Autowired
    private RabbitTemplate rabbitTemplate;

    @Autowired
    private MessageReceiver receiver;

    @Test
    void shouldDeliverMessage() {
        rabbitTemplate.convertAndSend("test-queue", "Payload");
        await().atMost(Duration.ofSeconds(5))
            .untilAsserted(() -> assertThat(receiver.getMessage()).isEqualTo("Payload"));
    }
}
```
</details>

<details>
<summary>Exercise 5: Shared Singleton Base Class (Hard)</summary>

**Task**: Implement an abstract base class that starts PostgreSQL and Redis once for the entire test suite and verifies that two distinct test classes execute against the same container instances.

**Solution**:
```java
@SpringBootTest
public abstract class SharedContainerBase {

    @ServiceConnection
    protected static final PostgreSQLContainer<?> POSTGRES;

    @ServiceConnection
    protected static final RedisContainer REDIS;

    static {
        POSTGRES = new PostgreSQLContainer<>("postgres:16-alpine");
        REDIS = new RedisContainer("redis:7.2-alpine");
        POSTGRES.start();
        REDIS.start();
    }
}

class TestSuitePartOne extends SharedContainerBase {
    @Test void testPartOne() {}
}

class TestSuitePartTwo extends SharedContainerBase {
    @Test void testPartTwo() {}
}
```
</details>

<details>
<summary>Exercise 6: Development-Time Testcontainers Configuration (Hard)</summary>

**Task**: Write a `TestcontainersConfiguration` class that defines `@ServiceConnection` beans for PostgreSQL and Kafka with reuse enabled for local developer workstation execution.

**Solution**:
```java
@TestConfiguration(proxyBeanMethods = false)
public class DevTestcontainersConfig {

    @Bean
    @ServiceConnection
    public PostgreSQLContainer<?> postgres() {
        return new PostgreSQLContainer<>("postgres:16-alpine").withReuse(true);
    }

    @Bean
    @ServiceConnection
    public KafkaContainer kafka() {
        return new KafkaContainer(DockerImageName.parse("confluentinc/cp-kafka:7.6.1")).withReuse(true);
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Real Engine Fidelity**: Testcontainers replaces embedded mocks with real production Docker images.
- **`@ServiceConnection`**: Auto-configures Spring Boot connection properties directly from the container.
- **Always Static Containers**: Ensure `@Container` fields are `static` to prevent per-method container restarts.
- **Singleton Pattern**: Share running containers across test classes in an abstract base class to save minutes in CI.
- **Container Reuse**: Enable `.withReuse(true)` locally to achieve sub-second test start times.

---

## 13. Further Reading
- [Testcontainers Official Java Guide](https://java.testcontainers.org/)
- [Spring Boot Testcontainers Documentation](https://docs.spring.io/spring-boot/docs/current/reference/html/features.html#features.testing.testcontainers)
- [Spring Boot 3.1 Service Connection Feature Guide](https://spring.io/blog/2023/06/23/improved-testcontainers-support-in-spring-boot-3-1)
- [AtomicJar: Testcontainers Best Practices](https://atomicjar.com/)
- [Docker Documentation: Managing Local Containers](https://docs.docker.com/)
