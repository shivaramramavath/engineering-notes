# Spring Ecosystem Overview & Architectural Landscape

> An executive architectural map of the Spring landscape—spanning Spring Framework, Spring Boot, Spring Data, Spring Security, Spring Cloud, Spring Modulith, and Spring AI—and how they assemble into modern enterprise backends.

| Metadata | Value |
| :--- | :--- |
| **Level** | Beginner |
| **Prerequisites** | [Java Language Foundations](file:///01-languages/java/) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.0.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | OpenJDK 25.0.1, Spring Boot 3.4.0 |

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
In enterprise Java development, building production applications requires coordinating multiple cross-cutting infrastructure concerns: dependency injection, configuration management, database connection pooling, transaction orchestration, object-relational mapping, authentication and authorization, asynchronous messaging, and operational observability. Implementing these primitives from scratch leads to tight coupling, brittle boilerplate code, and non-standardized architectures.

### Historical Evolution
- **Spring Framework 1.x-2.x (2003-2007)**: Introduced Inversion of Control (IoC) and Aspect-Oriented Programming (AOP) via heavy XML configuration files (`applicationContext.xml`), displacing heavyweight EJB 2.x session beans.
- **Spring Framework 3.x-4.x (2009-2015)**: Shifted to type-safe JavaConfig (`@Configuration`, `@Bean`) and annotation-driven component scanning (`@Component`, `@Autowired`).
- **Spring Boot 1.x-2.x (2014-2022)**: Revolutionized developer productivity by introducing opinionated auto-configuration, starter dependency management (`spring-boot-starter-*`), embedded servlet containers (Tomcat, Jetty), and Actuator metrics.
- **Spring Boot 3.x-4.x (2022-Present)**: Mandated Java 17+ baseline (optimized for Java 21/25 LTS), migrated to the `jakarta.*` namespace, introduced native Ahead-of-Time (AOT) GraalVM compilation, integrated native Virtual Threads (Project Loom), adopted built-in structured logging, and added first-class Spring AI and Spring Modulith modules.

### Real-World Relevance
Spring Boot powers the majority of global enterprise financial, e-commerce, telecommunications, and healthcare backends. It provides an opinionated runtime container that eliminates manual infrastructure wiring while retaining total architectural flexibility.

---

## 2. Core Concepts

### Architectural Layers of the Spring Ecosystem
1. **Spring Framework**: The foundational runtime providing the IoC container, dependency injection, AOP proxying, event broadcasting, resource abstraction, and the core Spring MVC web engine.
2. **Spring Boot**: The opinionated convention-over-configuration layer providing pre-configured starters, conditional auto-configuration, embedded application servers, externalized configuration binding, and Actuator monitoring.
3. **Spring Data**: Consistent data access abstraction covering relational databases (JPA, JDBC, R2DBC) and NoSQL stores (MongoDB, Redis, Cassandra, Elasticsearch).
4. **Spring Security**: De facto enterprise security framework providing authentication filter chains, OAuth2 authorization servers/resource servers, passkey support, and method-level access control.
5. **Spring Cloud**: Tooling for distributed systems: API gateways, circuit breakers (Resilience4j), service discovery, distributed tracing, and externalized config.
6. **Spring Modulith**: First-class architectural library for building verified, domain-driven modular monoliths with persistent outbox events.
7. **Spring AI**: Portable abstraction across commercial and open-source Large Language Models (LLMs), vector databases, embedding generation, tool calling, and RAG pipelines.

---

## 3. How It Works Internally

### Ecosystem Component Relationship Flow

```
+-------------------------------------------------------------------------+
|                       Enterprise Client Layer                           |
|         (Web Browsers, Mobile Apps, Microservices, IoT Devices)         |
+------------------------------------+------------------------------------+
                                     | HTTP / REST / SSE / WebSockets / gRPC
                                     v
+-------------------------------------------------------------------------+
|                  Spring Boot Web & Security Layer                       |
|  +-------------------------------------------------------------------+  |
|  | Spring Security FilterChain (OAuth2, JWT, Passkeys, CORS, CSRF)   |  |
|  +---------------------------------+---------------------------------+  |
|                                    v                                    |
|  +---------------------------------+---------------------------------+  |
|  | DispatcherServlet / WebFlux (Controllers, Validation, ProblemDetail)|  |
|  +---------------------------------+---------------------------------+  |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                     Domain & Application Layer                          |
|  +-------------------------------------------------------------------+  |
|  | Services, Business Domain Records, Aggregates, Event Publishers   |  |
|  | Spring Modulith Module Verification & Outbox Publication          |  |
|  +---------------------------------+---------------------------------+  |
+------------------------------------+------------------------------------+
                                     |
         +---------------------------+---------------------------+
         |                                                       |
         v                                                       v
+---------------------------------+     +---------------------------------+
| Spring Data Persistence Layer   |     | Spring Messaging & AI Layer     |
| - Spring Data JPA (Hibernate 6) |     | - Spring for Apache Kafka       |
| - Spring Data JDBC / JdbcClient |     | - Spring AMQP (RabbitMQ)        |
| - Redis (Cache / Sorted Sets)   |     | - Spring AI (ChatClient, Vector)|
+----------------+----------------+     +----------------+----------------+
                 |                                       |
                 v                                       v
         [ PostgreSQL DB ]                       [ Kafka / OpenAI ]
```

---

## 4. Syntax & Usage

### Maven BOM Dependency Management
In modern Spring Boot projects, the root `pom.xml` uses the Spring Boot Starter Parent or the dependency management BOM (`spring-boot-dependencies`) to guarantee version compatibility across all ecosystem libraries without manual version tags:

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
        <version>3.4.0</version>
        <relativePath/>
    </parent>

    <groupId>com.learning.springboot</groupId>
    <artifactId>ecosystem-demo</artifactId>
    <version>0.0.1-SNAPSHOT</version>

    <properties>
        <java.version>25</java.version>
    </properties>

    <dependencies>
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-web</artifactId>
        </dependency>
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-actuator</artifactId>
        </dependency>
    </dependencies>
</project>
```

---

## 5. Practical Examples

### Example 1: Minimal Spring Boot Standalone Application
- **Goal:** Demonstrate the single-class entry point combining configuration, component scanning, and embedded servlet initialization.

```java
package com.learning.springboot;

import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RestController;

@SpringBootApplication
@RestController
public class EcosystemOverviewApplication {

    @GetMapping("/api/health")
    public HealthStatus health() {
        return new HealthStatus("UP", "Spring Boot 3.4.x running on Java 25");
    }

    public static void main(String[] args) {
        SpringApplication.run(EcosystemOverviewApplication.class, args);
    }

    public record HealthStatus(String status, String details) {}
}
```

- **Console Log Output:**
```text
  .   ____          _            __ _ _
 /\\ / ___'_ __ _ _(_)_ __  __ _ \ \ \ \
( ( )\___ | '_ | '_| | '_ \/ _` | \ \ \ \
 \\/  ___)| |_)| | | | | || (_| |  ) ) ) )
  '  |____| .__|_| |_|_| |_\__, | / / / /
 =========|_|==============|___/=/_/_/_/
 :: Spring Boot ::                (v3.4.0)

2026-11-24T18:00:00.123+05:30  INFO 18402 --- [main] c.l.s.EcosystemOverviewApplication      : Starting EcosystemOverviewApplication using Java 25.0.1
2026-11-24T18:00:01.450+05:30  INFO 18402 --- [main] o.s.b.w.embedded.tomcat.TomcatWebServer  : Tomcat initialized with port 8080 (http)
2026-11-24T18:00:01.890+05:30  INFO 18402 --- [main] c.l.s.EcosystemOverviewApplication      : Started EcosystemOverviewApplication in 1.952 seconds (process running for 2.311)
```

- **Explanation:** `@SpringBootApplication` bundles `@Configuration`, `@EnableAutoConfiguration`, and `@ComponentScan`. `SpringApplication.run()` creates the ApplicationContext, initializes the embedded Tomcat web server on port 8080, and exposes the HTTP endpoint returning a JSON record.

---

### Example 2: Inspecting Auto-Configured Ecosystem Beans
- **Goal:** Programmatically inspect which core ecosystem beans were automatically wired into the container.

```java
package com.learning.springboot;

import org.springframework.boot.CommandLineRunner;
import org.springframework.context.ApplicationContext;
import org.springframework.stereotype.Component;
import java.util.Arrays;

@Component
public class EcosystemInspectorRunner implements CommandLineRunner {

    private final ApplicationContext context;

    public EcosystemInspectorRunner(ApplicationContext context) {
        this.context = context;
    }

    @Override
    public void run(String... args) {
        System.out.println("=== Spring Ecosystem Core Beans Loaded ===");
        String[] beans = context.getBeanDefinitionNames();
        Arrays.stream(beans)
              .filter(name -> name.contains("dispatcherServlet") || name.contains("objectMapper") || name.contains("conversionService"))
              .forEach(name -> System.out.println("-> Discovered: " + name + " [" + context.getBean(name).getClass().getSimpleName() + "]"));
    }
}
```

- **Console Output:**
```text
=== Spring Ecosystem Core Beans Loaded ===
-> Discovered: dispatcherServlet [DispatcherServlet]
-> Discovered: objectMapper [ObjectMapper]
-> Discovered: mvcConversionService [DefaultFormattingConversionService]
```

- **Explanation:** Demonstrates that Spring Boot auto-detects dependencies on the classpath (`spring-boot-starter-web`) and registers production-grade beans (Jackson's `ObjectMapper`, Spring's `DispatcherServlet`) without manual XML or Java configuration.

---

### Example 3: Edge Case – Managing Submodule Dependency Conflicts
- **Goal:** Safely exclude embedded Tomcat when swapping to an alternative embedded container (e.g., Undertow or Jetty).

```xml
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-starter-web</artifactId>
    <exclusions>
        <exclusion>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-tomcat</artifactId>
        </exclusion>
    </exclusions>
</dependency>
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-starter-undertow</artifactId>
</dependency>
```

- **Console Output on Startup:**
```text
2026-11-24T18:00:02.100+05:30  INFO 18402 --- [main] o.s.b.w.e.u.UndertowServletWebServer     : Undertow started on port 8080 (http) with context path ''
```

- **Explanation:** Demonstrates the modularity of Spring Boot starters. Excluding Tomcat automatically flips auto-configuration conditions (`@ConditionalOnClass`), enabling the Undertow web server factory without breaking any web MVC controllers.

---

### Example 4: Failure & Anti-Pattern – Classpath Collision with Mixed Boot Versions
- **Goal:** Demonstrate the catastrophic failure when transitive library versions diverge from the parent BOM.

```xml
<!-- ANTI-PATTERN: Hardcoding child module version overriding parent BOM -->
<dependency>
    <groupId>org.springframework.security</groupId>
    <artifactId>spring-security-core</artifactId>
    <version>5.7.0</version> <!-- INCOMPATIBLE with Spring Boot 3.4 / Jakarta EE -->
</dependency>
```

- **Console Output / Exception:**
```text
java.lang.NoClassDefFoundError: javax/servlet/Filter
    at org.springframework.security.web.SecurityFilterChain.<clinit>(SecurityFilterChain.java:35)
    at org.springframework.boot.autoconfigure.security.servlet.SpringBootWebSecurityConfiguration.defaultSecurityFilterChain(SpringBootWebSecurityConfiguration.java:56)
Caused by: java.lang.ClassNotFoundException: javax.servlet.Filter
    ... 45 more
Action:
Update your dependencies to rely on Spring Boot's dependency management BOM.
```

- **Explanation & Fix:** Spring Boot 3.x+ requires `jakarta.servlet.*`. Forcing a legacy 5.x Spring Security artifact looks for `javax.servlet.*` which does not exist in Tomcat 10+, crashing startup. The fix is to omit the `<version>` tag entirely and let `spring-boot-dependencies` govern compatibility.

---

### Example 5: Modern High-Performance Ecosystem Integration (Virtual Threads)
- **Goal:** Activate native Virtual Threads (Java 21/25 Loom) across the entire Spring web ecosystem using one property.

```properties
# application.properties
spring.threads.virtual.enabled=true
```

```java
package com.learning.springboot;

import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RestController;

@RestController
public class VirtualThreadDemoController {

    @GetMapping("/api/thread-info")
    public ThreadInfo getThread() {
        Thread current = Thread.currentThread();
        return new ThreadInfo(
            current.toString(),
            current.isVirtual(),
            Runtime.version().feature()
        );
    }

    public record ThreadInfo(String threadName, boolean isVirtual, int javaFeatureVersion) {}
}
```

- **Console / HTTP Response Output:**
```json
{
  "threadName": "VirtualThread[#48,tomcat-handler-0]/runnable@ForkJoinPool-1-worker-1",
  "isVirtual": true,
  "javaFeatureVersion": 25
}
```

- **Explanation:** On Java 21+, setting `spring.threads.virtual.enabled=true` instructs Spring Boot to configure Tomcat's protocol handler and `@Async` task executors to dispatch each incoming HTTP request onto an ephemeral Java Virtual Thread, scaling concurrent request capacity to hundreds of thousands without blocking OS threads.

---

## 6. Best Practices

1. **Rely on Parent BOMs**: Never hardcode version tags on official Spring starters or managed third-party dependencies (Hibernate, Jackson, Flyway).
2. **Explicit Starter Selection**: Use specialized starters (`spring-boot-starter-validation`, `spring-boot-starter-actuator`) rather than pulling in monolithic dependency bundles.
3. **Embrace Immutable Records**: Use Java 17+ `record` types for all DTOs, configuration properties (`@ConfigurationProperties`), and immutable event payloads.
4. **Prefer Constructor Injection**: Mark all injected dependencies as `private final` and inject them via constructor parameters.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Mixing `javax.*` and `jakarta.*` Imports** | Spring Boot 3+ compiles against Jakarta EE 10+; legacy `javax.*` classes cause `NoClassDefFoundError` | Replace all `javax.persistence.*`, `javax.servlet.*`, `javax.validation.*` imports with `jakarta.*` |
| **Hardcoding Starter Dependency Versions** | Overrides Spring Boot BOM dependency alignment, causing runtime binary incompatibility | Remove `<version>` tags from `<dependency>` blocks; allow parent BOM to resolve versions |
| **Placing Main Application in Root Default Package** | Disables `@ComponentScan` for child packages, causing missing bean injection failures | Always locate `Application.java` in a named root package (e.g. `com.company.project`) |
| **Misunderstanding Starter Composition** | Including both `spring-boot-starter-web` and `spring-boot-starter-webflux` defaults to blocking MVC Tomcat | Choose one primary server runtime per application or understand the servlet fallback rules |
| **Neglecting Native Observability** | Running production apps without `spring-boot-starter-actuator` leaves ops blind during outages | Always include Actuator and configure health probes (`/actuator/health/liveness`) |
| **Over-using Global `@Enable*` Annotations** | Manually declaring `@EnableWebMvc` overrides Spring Boot's auto-configured Jackson and formatting settings | Avoid manual `@EnableWebMvc` unless intentionally taking full low-level control of MVC infrastructure |

---

## 8. Performance & Security Notes

- **Startup Latency**: Auto-configuration evaluates hundreds of conditional classes on startup. In serverless or scale-to-zero environments, utilize Spring AOT compilation with GraalVM Native Image for sub-50ms cold starts.
- **Memory Footprint**: Standard JVM containers require 250MB-512MB RAM for metadata, JIT compiler caches, and metaspace. AOT Native images reduce footprint to 30MB-60MB.
- **Dependency Vulnerability Scanning**: Always integrate OWASP Dependency-Check or Snyk into Maven/Gradle CI pipelines to detect known CVEs in transitive starters.

---

## 9. Comparison / When to Use What

| Dimension | Spring Framework Alone | Spring Boot (Standard) | Spring Boot Native (GraalVM) |
|:---|:---|:---|:---|
| **Configuration** | Manual XML or JavaConfig classes | Convention over configuration auto-starters | Build-time AOT pre-computed configuration |
| **Web Server** | Deployed as WAR to external Tomcat/JBoss | Embedded Tomcat/Jetty inside executable JAR | Self-contained native binary executable |
| **Startup Time** | 3-10 seconds | 1-3 seconds | 0.03-0.08 seconds (Instantaneous) |
| **Memory Usage** | Moderate to High (JVM) | Moderate (250MB+) | Minimal (35MB - 70MB) |
| **Best Used For** | Legacy enterprise maintenance | 95% of enterprise microservices and APIs | Serverless functions (AWS Lambda), CLI tools |

---

## 10. Interview Questions

<details>
<summary><strong>Q1: What is the mechanical difference between Spring Framework and Spring Boot?</strong></summary>

### Answer:
Spring Framework is the foundational inversion-of-control container providing core dependency injection, transaction abstractions, and MVC routing. Spring Boot is an opinionated packaging and deployment framework built on top of the Spring Framework. Boot provides auto-configuration (`@EnableAutoConfiguration`), dependency management BOMs (starters), embedded servlet containers, and operational telemetry (Actuator), eliminating the need for boilerplate configuration and external web server deployments.
</details>

<details>
<summary><strong>Q2: What happens behind the scenes when <code>@SpringBootApplication</code> is placed on a class?</strong></summary>

### Answer:
`@SpringBootApplication` is a meta-annotation composed of three critical annotations:
1. `@SpringBootConfiguration`: A specialized form of `@Configuration` identifying the class as a source of bean definitions.
2. `@EnableAutoConfiguration`: Instructs Spring Boot to scan `META-INF/spring/org.springframework.boot.autoconfigure.AutoConfiguration.imports` and conditionally register beans based on classpath contents.
3. `@ComponentScan`: Activates recursive package scanning for `@Component`, `@Service`, `@Repository`, and `@RestController` classes starting from the package of the annotated class.
</details>

<details>
<summary><strong>Q3: Why did Spring Boot 3 migrate from <code>javax.*</code> to <code>jakarta.*</code>?</strong></summary>

### Answer:
Oracle transferred the Java EE trademarks and specifications to the Eclipse Foundation in 2017. As part of the governance agreement, the `javax` package namespace was restricted. The Eclipse Foundation introduced Jakarta EE, and starting with Jakarta EE 9, all specifications (Servlet, JPA, Bean Validation, JMS) were migrated to the `jakarta.*` package namespace. Spring Boot 3 baseline required Jakarta EE 10, necessitating the universal migration.
</details>

<details>
<summary><strong>Q4: How does Spring Boot manage dependency versions without developers specifying <code>&lt;version&gt;</code> tags?</strong></summary>

### Answer:
Via the Maven `<dependencyManagement>` mechanism inherited from `spring-boot-dependencies` (or `spring-boot-starter-parent`). The BOM defines tested, compatible version properties for hundreds of libraries. When a developer declares `<groupId>org.springframework.boot</groupId><artifactId>spring-boot-starter-data-jpa</artifactId>`, Maven checks the parent BOM's dependencyManagement table and injects the curated version.
</details>

<details>
<summary><strong>Q5: What are the trade-offs of enabling Java Virtual Threads in Spring Boot 3.4+?</strong></summary>

### Answer:
- **Pros**: Virtual Threads (Loom) enable standard synchronous blocking code (`RestClient`, JPA, JDBC) to achieve the high concurrent connection density of reactive programming without reactive complexity or callback hell.
- **Cons**: If third-party libraries use `synchronized` blocks around long-running I/O operations (instead of `ReentrantLock`), they pin the underlying carrier thread, neutralizing throughput benefits. Furthermore, unbounded thread creation can overwhelm downstream database connection pools.
</details>

<details>
<summary><strong>Q6: How does Spring Modulith differ from standard multi-module Maven projects?</strong></summary>

### Answer:
Multi-module Maven projects enforce isolation at build-time through physical project boundaries, which often increases build complexity and deployment friction. Spring Modulith allows building a single deployment monolith while enforcing architectural domain boundaries at runtime and test-time via code introspection (`ApplicationModules.of(Application.class).verify()`). It also provides an integrated event publication registry for asynchronous outbox communication between modules.
</details>

<details>
<summary><strong>Q7: What is Spring AI and how does it fit into the ecosystem?</strong></summary>

### Answer:
Spring AI is an official Spring project providing portable client abstractions for interacting with generative AI models. It abstracts prompt engineering, ChatModels, EmbeddingModels, VectorStores (pgvector, Milvus, Qdrant), document ingestion ETL pipelines, and tool/function calling into idiomatic Spring beans, decoupling business logic from proprietary AI vendor SDKs.
</details>

<details>
<summary><strong>Q8: What is the purpose of Spring Boot's <code>spring-boot-docker-compose</code> module?</strong></summary>

### Answer:
Introduced in Spring Boot 3.1, this development dependency automatically discovers a `compose.yaml` file in the project root. On application startup, it starts required infrastructure containers (e.g. PostgreSQL, Redis, Kafka) and automatically injects connection properties (URLs, ports, credentials) into the Spring Environment, tearing them down on shutdown.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Create a Health Endpoint Record</strong></summary>

**Problem:** Write a Spring REST controller method that returns an immutable record containing the current system timestamp, application name, and uptime status.

**Solution:**
```java
package com.learning.springboot;

import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RestController;
import java.time.Instant;

@RestController
public class HealthController {

    @GetMapping("/api/status")
    public AppStatus getStatus() {
        return new AppStatus("EcosystemApp", Instant.now(), true);
    }

    public record AppStatus(String appName, Instant timestamp, boolean active) {}
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Programmatic Custom Banner</strong></summary>

**Problem:** Configure `SpringApplication` to disable the default startup console banner programmatically.

**Solution:**
```java
package com.learning.springboot;

import org.springframework.boot.Banner;
import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;

@SpringBootApplication
public class SilentApplication {
    public static void main(String[] args) {
        SpringApplication app = new SpringApplication(SilentApplication.class);
        app.setBannerMode(Banner.Mode.OFF);
        app.run(args);
    }
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Conditional Auto-Configuration Filter</strong></summary>

**Problem:** Implement a configuration bean that is only registered if a specific custom property `app.feature.ai-enabled=true` is present in `application.properties`.

**Solution:**
```java
package com.learning.springboot;

import org.springframework.boot.autoconfigure.condition.ConditionalOnProperty;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;

@Configuration
public class OptionalFeatureConfiguration {

    @Bean
    @ConditionalOnProperty(name = "app.feature.ai-enabled", havingValue = "true")
    public String aiServiceEngine() {
        return "AI Feature Engine Activated";
    }
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Verify Module Boundaries with Spring Modulith</strong></summary>

**Problem:** Write a JUnit 5 test verifying that application module dependencies comply with modularity constraints using Spring Modulith.

**Solution:**
```java
package com.learning.springboot;

import org.junit.jupiter.api.Test;
import org.springframework.modulith.core.ApplicationModules;

class ModularityTest {

    @Test
    void verifyModularityRules() {
        ApplicationModules modules = ApplicationModules.of(EcosystemOverviewApplication.class);
        modules.verify();
    }
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Custom Dynamic Port Allocator Runner</strong></summary>

**Problem:** Implement an `ApplicationListener<WebServerInitializedEvent>` that captures the random assigned port when `server.port=0` and logs the full local access URL.

**Solution:**
```java
package com.learning.springboot;

import org.springframework.boot.web.context.WebServerInitializedEvent;
import org.springframework.context.ApplicationListener;
import org.springframework.stereotype.Component;

@Component
public class PortReporterListener implements ApplicationListener<WebServerInitializedEvent> {

    @Override
    public void onApplicationEvent(WebServerInitializedEvent event) {
        int port = event.getWebServer().getPort();
        System.out.println("===> Service successfully allocated random port: http://localhost:" + port);
    }
}
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Build-Time AOT Reflection Hints Generator</strong></summary>

**Problem:** Implement a `RuntimeHintsRegistrar` that registers reflection hints for an external third-party DTO record to ensure GraalVM Native Image compatibility.

**Solution:**
```java
package com.learning.springboot;

import org.springframework.aot.hint.MemberCategory;
import org.springframework.aot.hint.RuntimeHints;
import org.springframework.aot.hint.RuntimeHintsRegistrar;

public class CustomDtoRuntimeHints implements RuntimeHintsRegistrar {

    @Override
    public void registerHints(RuntimeHints hints, ClassLoader classLoader) {
        hints.reflection().registerType(
            ExternalPayloadDto.class,
            MemberCategory.INVOKE_DECLARED_CONSTRUCTORS,
            MemberCategory.INVOKE_PUBLIC_METHODS
        );
    }

    public record ExternalPayloadDto(String id, String payload) {}
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Spring Framework**: Core IoC container, AOP proxies, transaction abstractions, Spring MVC.
- **Spring Boot**: Auto-configuration, opinionated starters, embedded servers, Actuator telemetry.
- **Jakarta EE 10+**: Universal namespace shift from `javax.*` to `jakarta.*`.
- **Java 21/25 Baseline**: Built for modern Records, Pattern Matching, and Virtual Threads (`spring.threads.virtual.enabled=true`).
- **Dependency Rule**: Never hardcode library versions; let `spring-boot-starter-parent` manage compatibility.

---

## 13. Further Reading
- [Official Spring Boot Documentation](https://docs.spring.io/spring-boot/docs/current/reference/html/)
- [Spring Framework Reference Documentation](https://docs.spring.io/spring-framework/reference/)
- [Spring Modulith Documentation](https://spring.io/projects/spring-modulith)
- [Spring AI Project Guide](https://spring.io/projects/spring-ai)
