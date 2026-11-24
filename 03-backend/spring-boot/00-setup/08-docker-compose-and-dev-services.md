# Docker Compose Integration & Development Services

> Deep-dive into Spring Boot's built-in `spring-boot-docker-compose` module, `@ServiceConnection` dynamic auto-configuration, container lifecycle orchestration, healthcheck synchronization, and Testcontainers Dev Services for seamless zero-setup local development.

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [Project Setup with Initializr, Maven & Gradle](file:///03-backend/spring-boot/00-setup/03-project-setup-with-initializr-maven-gradle.md), [First Application Walkthrough](file:///03-backend/spring-boot/00-setup/05-first-application-walkthrough.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.0.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | OpenJDK 25.0.1, Spring Boot 3.4.0, Docker Engine 27.x |

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
Onboarding a new developer or running integration tests previously meant maintaining pages of manual wiki documentation: installing PostgreSQL 16, configuring Redis passwords, setting up RabbitMQ vhosts, and synchronizing dynamic port assignments. If the local developer forgot to start Docker containers or had port clashes with existing services, application bootstrap failed immediately.

### Historical Evolution
- **Manual Docker Orchestration (2014-2020)**: Developers manually ran `docker-compose up -d` in a separate terminal before launching Spring Boot, and manually maintained database passwords across both `docker-compose.yml` and `application-dev.yaml`.
- **Testcontainers Integration Testing (2017-Present)**: Testcontainers allowed automated ephemeral container creation inside unit and integration tests, but running the actual application locally in an IDE still required manual steps.
- **Spring Boot Docker Compose & `@ServiceConnection` (Boot 3.1+)**: Spring Boot native integration connects application bootstrap directly to Docker Compose. When `SpringApplication.run()` executes, it automatically detects `compose.yaml`, boots the required containers, inspects dynamic mapped ports, registers credentials into the Spring `Environment`, and cleans up on shutdown.

### Real-World Relevance
Zero-friction "clone-and-run" repository setups eliminate configuration drift and ensure identical infrastructure behavior across developer laptops, CI test runners, and staging environments.

---

## 2. Core Concepts

### 1. `spring-boot-docker-compose`
An official Spring Boot development module. When added to your classpath, Spring Boot:
1. Searches the project root for `compose.yaml`, `compose.yml`, `docker-compose.yaml`, or `docker-compose.yml`.
2. Executes `docker compose up` to start any defined services.
3. Automatically maps exposed container ports and credentials into Spring configuration properties (e.g., `spring.datasource.url`, `spring.data.redis.host`).
4. Optionally stops containers via `docker compose down` when the JVM shuts down.

### 2. `@ServiceConnection`
Introduced in Spring Boot 3.1, `@ServiceConnection` eliminates the need to manually declare `DynamicPropertyRegistry` or hardcode properties in `application.yaml`. When applied to a container definition in tests or dev services, Spring Boot's connection details factory automatically detects the service type (PostgreSQL, MySQL, Redis, RabbitMQ, Kafka, MongoDB, Elasticsearch) and populates all necessary connection beans.

### 3. Container Readiness & Healthchecks
Spring Boot waits for containers to become healthy before proceeding with context bootstrap. If a container definition includes a Docker `healthcheck`, Spring Boot blocks until the status transitions to `healthy`, preventing `ConnectionRefusedException` during Flyway or Liquibase migrations.

---

## 3. How It Works Internally

### Docker Compose Lifecycle during Application Startup

```
+-------------------------------------------------------------------------+
|                  Developer Launches Application                         |
|                      (`SpringApplication.run()`)                        |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|             1. Locate & Validate Compose File                           |
|  - Looks for `compose.yaml` in project root directory                   |
|  - Validates Docker daemon availability over local socket               |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|             2. Orchestrate Container Startup                            |
|  - Issues `docker compose up` command                                  |
|  - Starts PostgreSQL, Redis, Kafka, etc.                                |
|  - Blocks until all services with `healthcheck` report HEALTHY          |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|             3. Extract Dynamic Connection Metadata                      |
|  - Queries Docker API for assigned dynamic host ports                   |
|  - Discovers credentials and database names from environment variables  |
|  - Auto-wires ConnectionDetails beans:                                  |
|    * JdbcConnectionDetails                                              |
|    * RedisConnectionDetails                                             |
|    * RabbitConnectionDetails                                            |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|             4. Refresh ApplicationContext & Run Migrations              |
|  - Flyway executes schema migrations against healthy container          |
|  - Hibernate verifies schema                                            |
|  - Spring Boot starts HTTP server on port 8080                          |
+------------------------------------+------------------------------------+
                                     |
                         JVM Shutdown (SIGTERM / Exit)
                                     |
                                     v
+-------------------------------------------------------------------------+
|             5. Lifecycle Teardown (`stop` or `down`)                    |
|  - Configured via `spring.docker.compose.lifecycle-management`          |
|  - If `start-and-stop`: Executes `docker compose stop`                  |
+-------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### Maven Dependency
DevTools and Docker Compose support should be scoped as runtime dependencies and excluded from production packages:
```xml
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-docker-compose</artifactId>
    <scope>runtime</scope>
    <optional>true</optional>
</dependency>
```

### Gradle Kotlin DSL
```kotlin
dependencies {
    developmentOnly("org.springframework.boot:spring-boot-docker-compose")
}
```

### Configuration Options (`application.yaml`)
```yaml
spring:
  docker:
    compose:
      enabled: true
      file: "compose.yaml"
      # Options: none, start-only, start-and-stop
      lifecycle-management: start-and-stop
      startup:
        # Options: if-not-running, always
        mode: if-not-running
      stop:
        # Options: down, stop
        command: stop
        timeout: 10s
      readiness:
        # Options: tcp, check (relies on compose healthcheck)
        wait: check
        timeout: 60s
```

---

## 5. Practical Examples

### Example 1: Production-Grade `compose.yaml` with Healthchecks
```yaml
services:
  postgres:
    image: 'postgres:16-alpine'
    environment:
      POSTGRES_DB: 'order_db'
      POSTGRES_USER: 'order_user'
      POSTGRES_PASSWORD: 'order_secret_password'
    ports:
      - '5432' # Dynamic host port mapping
    healthcheck:
      test: ['CMD-SHELL', 'pg_isready -U order_user -d order_db']
      interval: 3s
      timeout: 3s
      retries: 5
      start_period: 2s

  redis:
    image: 'redis:7.2-alpine'
    ports:
      - '6379'
    healthcheck:
      test: ['CMD', 'redis-cli', 'ping']
      interval: 3s
      timeout: 2s
      retries: 3

  rabbitmq:
    image: 'rabbitmq:3.13-management-alpine'
    environment:
      RABBITMQ_DEFAULT_USER: 'guest'
      RABBITMQ_DEFAULT_PASS: 'guest'
    ports:
      - '5672'
      - '15672' # Management UI
    healthcheck:
      test: ['CMD', 'rabbitmq-diagnostics', '-q', 'ping']
      interval: 5s
      timeout: 5s
      retries: 5
```

### Example 2: Zero-Config Application Code (No Database URL in YAML)
```yaml
# application.yaml - Notice: NO spring.datasource.url or redis.host required!
spring:
  application:
    name: order-service
  jpa:
    hibernate:
      ddl-auto: validate
    open-in-view: false
  docker:
    compose:
      lifecycle-management: start-and-stop
```
```java
package com.example.demo;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.boot.CommandLineRunner;
import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;
import org.springframework.context.annotation.Bean;

import javax.sql.DataSource;
import java.sql.Connection;

@SpringBootApplication
public class DemoApplication {

    private static final Logger log = LoggerFactory.getLogger(DemoApplication.class);

    public static void main(String[] args) {
        SpringApplication.run(DemoApplication.class, args);
    }

    @Bean
    public CommandLineRunner verifyConnection(DataSource dataSource) {
        return args -> {
            try (Connection conn = dataSource.getConnection()) {
                log.info("Successfully connected to Dockerized Database: {}", conn.getMetaData().getURL());
                log.info("Database Product: {} {}", conn.getMetaData().getDatabaseProductName(), conn.getMetaData().getDatabaseProductVersion());
            }
        };
    }
}
```

### Example 3: Integration Testing with Testcontainers and `@ServiceConnection`
```java
package com.example.demo;

import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.boot.testcontainers.service.connection.ServiceConnection;
import org.testcontainers.containers.PostgreSQLContainer;
import org.testcontainers.junit.jupiter.Container;
import org.testcontainers.junit.jupiter.Testcontainers;

import javax.sql.DataSource;
import java.sql.Connection;
import java.sql.SQLException;

import static org.assertj.core.api.Assertions.assertThat;

@SpringBootTest
@Testcontainers
class OrderServiceApplicationTests {

    // Automatically wires JDBC URL, username, and password into Spring's Environment
    @Container
    @ServiceConnection
    static PostgreSQLContainer<?> postgres = new PostgreSQLContainer<>("postgres:16-alpine");

    @Autowired
    private DataSource dataSource;

    @Test
    void contextLoadsAndDatabaseIsConnected() throws SQLException {
        try (Connection conn = dataSource.getConnection()) {
            assertThat(conn.isValid(1)).isTrue();
        }
    }
}
```

### Example 4: Dev Services Anti-Pattern & Remediation (Missing Healthcheck)
```yaml
# ANTI-PATTERN: Container starts, but database engine is still initializing
services:
  db:
    image: postgres:16-alpine
    ports:
      - '5432:5432'
    # FLAW: No healthcheck! Spring Boot attempts to run Flyway immediately,
    # before PostgreSQL is accepting connections, throwing ConnectionRefusedException!

# REMEDIATION: Add an explicit healthcheck so Spring Boot blocks until DB is truly ready
services:
  db:
    image: postgres:16-alpine
    ports:
      - '5432:5432'
    environment:
      POSTGRES_DB: app_db
      POSTGRES_USER: app_user
      POSTGRES_PASSWORD: secret_password
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U app_user -d app_db"]
      interval: 2s
      timeout: 2s
      retries: 10
      start_period: 2s
```

### Example 5: Dev Mode via `Testcontainers` Desktop (Test Execution Application)
```java
package com.example.demo;

import org.springframework.boot.SpringApplication;
import org.springframework.boot.test.context.TestConfiguration;
import org.springframework.boot.testcontainers.service.connection.ServiceConnection;
import org.springframework.context.annotation.Bean;
import org.testcontainers.containers.PostgreSQLContainer;

// Run this class from the IDE to launch the application locally with Testcontainers!
public class TestApplicationLauncher {

    public static void main(String[] args) {
        SpringApplication.from(DemoApplication::main)
            .with(LocalDevConfiguration.class)
            .run(args);
    }

    @TestConfiguration(proxyBeanMethods = false)
    static class LocalDevConfiguration {

        @Bean
        @ServiceConnection
        PostgreSQLContainer<?> postgresContainer() {
            return new PostgreSQLContainer<>("postgres:16-alpine")
                .withReuse(true); // Reuses running container across IDE runs
        }
    }
}
```

---

## 6. Best Practices

1. **Always Define Container Healthchecks**: Ensure all services in `compose.yaml` declare a robust `healthcheck`. Spring Boot relies on healthchecks to delay application startup until the container is fully initialized.
2. **Use Dynamic Port Binding in Compose**: Expose ports as `- '5432'` instead of `- '5432:5432'`. This prevents local port conflicts and allows multiple branches or projects to run concurrently.
3. **Keep Compose Files in Version Control**: Standardize `compose.yaml` in the project root so any developer can run the project with zero prior software installations beyond Docker.
4. **Use `start-only` for Fast Local Iteration**: If container restart overhead slows down your workflow, configure `spring.docker.compose.lifecycle-management=start-only`. The containers will remain running across JVM restarts.
5. **Leverage `@ServiceConnection`**: Eliminate boilerplate `DynamicPropertySource` declarations in test classes by annotating container fields with `@ServiceConnection`.
6. **Mark as `optional` in Maven**: Always configure `spring-boot-docker-compose` as `<optional>true</optional>` (or Gradle `developmentOnly`) to ensure it is completely omitted from production fat JARs.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Missing Docker Daemon** | Running the app without Docker running on the host throws a fatal bootstrap error. | Start Docker Desktop / Colima, or set `spring.docker.compose.enabled=false`. |
| **No Healthcheck Defined** | Spring Boot proceeds with startup while the database is still initializing, causing Flyway connection failures. | Add a native `healthcheck` block (`pg_isready`, `redis-cli ping`) in `compose.yaml`. |
| **Hardcoding Host Ports (e.g., `5432:5432`)** | Collides with any locally installed database or other running container. | Use dynamic port mapping (e.g., `- '5432'`) and let Spring Boot discover the assigned port. |
| **Docker Compose Leaking into Production** | If packaged into production artifacts, Boot attempts to call `docker compose up` inside production Kubernetes pods, crashing. | Mark dependency `<optional>true</optional>` in Maven or `developmentOnly` in Gradle. |
| **Misspelled Service Image Names** | Using non-standard images prevents Spring Boot from deducing the service type (e.g., custom PostgreSQL image). | Label the service in `compose.yaml` with `org.springframework.boot.service-connection=postgres`. |
| **Slow Docker Startup Timeout** | Heavy containers (Kafka, LocalStack) exceeding the default 120s timeout fail the build. | Increase `spring.docker.compose.readiness.timeout=180s`. |

---

## 8. Performance & Security Notes

### Container Reuse for Fast Test Iteration
Testcontainers supports a `withReuse(true)` flag. When enabled in `~/.testcontainers.properties` (`testcontainers.reuse.enable=true`), containers are not terminated between test runs, dropping subsequent test suite execution time by up to 80%.

### Security Isolation
Do not expose sensitive container ports to the public internet. By default, `compose.yaml` dynamic ports bind to `0.0.0.0`. In production or public Wi-Fi networks, bind to localhost explicitly:
```yaml
ports:
  - '127.0.0.1::5432'
```

---

## 9. Comparison / When to Use What

| Feature / Tool | Mechanism | Scope | Port Handling | Best For |
| :--- | :--- | :--- | :--- | :--- |
| **`spring-boot-docker-compose`** | Runs `compose.yaml` via CLI | Local Development (`main`) | Discovers dynamic or static | Instant "clone & run" for local dev |
| **Testcontainers `@ServiceConnection`** | Spawns JVM-managed containers | Automated Integration Tests | Always dynamic OS ports | CI/CD testing, reliable test isolation |
| **`TestApplicationLauncher`** | Boots app with Testcontainers | Local Development in IDE | Always dynamic OS ports | Development without `compose.yaml` |
| **Manual `docker compose up`** | External terminal process | Entire host | Manually configured | Polyglot or legacy setups |

---

## 10. Interview Questions

<details>
<summary><strong>Q1: What is the primary purpose of the `spring-boot-docker-compose` module?</strong></summary>

**Answer:**  
The `spring-boot-docker-compose` module automatically manages the lifecycle of containerized services during local development. When the Spring Boot application boots, it detects a `compose.yaml` file in the root directory, starts required backing containers (e.g., databases, message queues), awaits their healthiness, dynamically injects connection parameters into the Spring environment, and stops them when the app shuts down.
</details>

<details>
<summary><strong>Q2: How does `@ServiceConnection` work in Spring Boot 3.1+?</strong></summary>

**Answer:**  
`@ServiceConnection` automatically configures Spring Boot `ConnectionDetails` beans (such as `JdbcConnectionDetails`, `RedisConnectionDetails`, or `KafkaConnectionDetails`) directly from a container definition without requiring manual `@DynamicPropertySource` annotations. It inspects the container image and mapped ports to wire datasources seamlessly.
</details>

<details>
<summary><strong>Q3: Why are healthchecks critical in `compose.yaml` when using Spring Boot?</strong></summary>

**Answer:**  
When a container starts, the container process may be active while the database engine inside is still initializing. Without a healthcheck, Spring Boot immediately attempts to initialize the `DataSource` and run database migrations (Flyway/Liquibase), resulting in connection failure. With a healthcheck, Spring Boot waits until the database confirms it is accepting connections.
</details>

<details>
<summary><strong>Q4: How does Spring Boot discover dynamic container ports?</strong></summary>

**Answer:**  
When a port mapping is specified without a host port (e.g., `- '5432'`), Docker assigns a random ephemeral port on the host. Spring Boot queries the Docker API during startup, extracts the assigned host port and IP, and injects them directly into the Spring environment before beans are created.
</details>

<details>
<summary><strong>Q5: What are the lifecycle management options for `spring.docker.compose.lifecycle-management`?</strong></summary>

**Answer:**  
- `none`: Does not start or stop containers (assumes they are already managed externally).
- `start-only`: Starts containers if not running, but leaves them active when the application terminates (ideal for fast IDE restart cycles).
- `start-and-stop`: Starts containers on boot and stops them when the application context closes.
</details>

<details>
<summary><strong>Q6: How can you use a custom Docker image with `@ServiceConnection` if Spring Boot doesn't recognize the image name?</strong></summary>

**Answer:**  
Add a Docker Compose label specifying the connection type:
```yaml
labels:
  org.springframework.boot.service-connection: postgres
```
This tells Spring Boot to treat the container as a PostgreSQL service regardless of its Docker image repository name.
</details>

<details>
<summary><strong>Q7: Can `spring-boot-docker-compose` be safely packaged into a production fat JAR?</strong></summary>

**Answer:**  
No. In production (e.g., Kubernetes), calling Docker Compose commands will fail and is completely unwanted. The dependency should always be marked as `<optional>true</optional>` in Maven or `developmentOnly` in Gradle so it is excluded from final distribution artifacts.
</details>

<details>
<summary><strong>Q8: What is `SpringApplication.from(App::main).with(DevConfig.class).run(args)` used for?</strong></summary>

**Answer:**  
This pattern launches the application for local development while incorporating test configurations (such as Testcontainers `@ServiceConnection` beans) without contaminating production code. It allows developers to run the application with fully managed ephemeral containers straight from their IDE without a separate Compose file.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Create a PostgreSQL Compose File (Easy)</strong></summary>

**Task:**  
Create a `compose.yaml` in your project root with a PostgreSQL 16 container, credentials, and dynamic port mapping `- '5432'`. Add a `healthcheck` utilizing `pg_isready`.
</details>

<details>
<summary><strong>Exercise 2: Verify Dynamic Connection in Logs (Easy)</strong></summary>

**Task:**  
Add `spring-boot-docker-compose` to `pom.xml`. Boot the application. Observe the console logs to confirm that Spring Boot starts Docker Compose, waits for the healthcheck, and logs the assigned dynamic port.
</details>

<details>
<summary><strong>Exercise 3: Configure Redis and RabbitMQ Dev Services (Medium)</strong></summary>

**Task:**  
Extend `compose.yaml` to include Redis and RabbitMQ services alongside PostgreSQL. Verify that Spring Data Redis and Spring AMQP auto-configurations automatically connect without setting host/port properties in `application.yaml`.
</details>

<details>
<summary><strong>Exercise 4: Testcontainers `@ServiceConnection` Integration Test (Medium)</strong></summary>

**Task:**  
Write a `@SpringBootTest` integration test using Testcontainers. Declare a `PostgreSQLContainer` annotated with `@ServiceConnection`. Insert a record via a JPA repository and verify it persists successfully in the test container.
</details>

<details>
<summary><strong>Exercise 5: Multi-Container Integration Test with Compose (Hard)</strong></summary>

**Task:**  
Configure `spring.docker.compose.file` in `src/test/resources/application-test.yaml` pointing to a test-specific compose file. Write a test suite that boots multiple interdependent services (PostgreSQL, Kafka, Schema Registry) and executes an end-to-end integration test.
</details>

<details>
<summary><strong>Exercise 6: TestApplicationLauncher for Container-Free Local Dev (Hard)</strong></summary>

**Task:**  
Create a `src/test/java/.../TestApplicationLauncher.java` class. Use `SpringApplication.from(App::main).with(ContainerConfig.class).run(args)` with `@ServiceConnection` and container reuse enabled. Launch the application from your IDE and verify zero manual Docker setup is required.
</details>

---

## 12. Summary / Cheat Sheet

| Concern | Configuration / Code Snippet | Purpose |
| :--- | :--- | :--- |
| **Add Dependency** | `spring-boot-docker-compose` (`optional`) | Enables automatic compose orchestration |
| **Lifecycle Mode** | `spring.docker.compose.lifecycle-management: start-only` | Keeps containers running across IDE reboots |
| **Healthcheck** | `test: ['CMD-SHELL', 'pg_isready -U user']` | Forces Spring Boot to wait until DB is ready |
| **Dynamic Port** | `- '5432'` (omit host port) | Eliminates port collisions |
| **`@ServiceConnection`** | `@Container @ServiceConnection static PostgreSQLContainer<?> pg;` | Auto-wires connection properties in tests |
| **Custom Image Label** | `org.springframework.boot.service-connection: postgres` | Hints service type for custom Docker images |
| **Test Launcher** | `SpringApplication.from(App::main).with(DevConfig.class).run(args)` | Runs app in IDE powered by Testcontainers |

---

## 13. Further Reading
- [Official Spring Boot Docker Compose Reference](https://docs.spring.io/spring-boot/docs/current/reference/html/features.html#features.docker-compose)
- [Spring Boot Testcontainers & ServiceConnection Documentation](https://docs.spring.io/spring-boot/docs/current/reference/html/features.html#features.testing.testcontainers.service-connections)
- [Official Testcontainers Java Guide](https://java.testcontainers.org/)
- [Docker Compose Specification](https://docs.docker.com/compose/compose-file/)
