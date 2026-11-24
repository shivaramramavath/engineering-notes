# First Spring Boot Application: Complete Walkthrough

> An end-to-end architectural walkthrough of creating, configuring, executing, and dissecting your first Spring Boot 3.4+ application on Java 21, uncovering the exact bootstrap sequence of `SpringApplication.run()` from JVM entry to HTTP request servicing.

| Metadata | Value |
| :--- | :--- |
| **Level** | Beginner |
| **Prerequisites** | [Project Setup with Initializr, Maven & Gradle](file:///03-backend/spring-boot/00-setup/03-project-setup-with-initializr-maven-gradle.md) |
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
In traditional Java enterprise development, running a basic web service required writing an explicit deployment descriptor (`web.xml`), manually packaging a WAR file, downloading and configuring an external application server (Tomcat, GlassFish, WildFly), and configuring complex classloader hierarchies.

### Historical Evolution
- **The Monolithic Servlet Container (1999-2013)**: Applications were deployed *into* containers. Server management was isolated from application code, making local development and automated testing painful.
- **The Embedded Revolution (2014-Present)**: Spring Boot reversed this paradigm: the container is embedded *inside* the application. An executable JAR containing an embedded Tomcat or Jetty instance can be executed anywhere with a single `java -jar` command.
- **Modern Cloud-Native Boot 3.4+ (2024-Present)**: Leveraging Java 21+, modern Spring Boot applications use native record classes for immutable DTOs, execute HTTP requests across virtual threads, and bootstrap in under 1 second.

### Real-World Relevance
Understanding the exact startup sequence of `SpringApplication.run()` is critical for debugging bootstrap failures, optimizing container cold starts in Kubernetes, and configuring enterprise startup listeners.

---

## 2. Core Concepts

### 1. `@SpringBootApplication`
This meta-annotation combines three fundamental Spring annotations:
- `@SpringBootConfiguration`: Specialized form of `@Configuration` designating the class as a primary source of bean definitions.
- `@EnableAutoConfiguration`: Directs Spring Boot to inspect classpath dependencies and register matching auto-configuration beans.
- `@ComponentScan`: Instructs Spring to scan the current package and all sub-packages for `@Component`, `@Service`, `@Repository`, and `@RestController` classes.

### 2. The `main()` Method and `SpringApplication.run()`
The standard Java entry point delegates execution to Spring Boot:
```java
public static void main(String[] args) {
    SpringApplication.run(DemoApplication.class, args);
}
```
This single invocation instantiates the `SpringApplication` instance, prepares the `Environment`, instantiates the `ApplicationContext`, triggers auto-configurations, starts the embedded Tomcat web server, and invokes startup runners.

### 3. Package Placement Convention
Because `@ComponentScan` scans package-downward, the class annotated with `@SpringBootApplication` must reside in the root package of your project (e.g., `com.example.app`). Placing it in a sub-package causes Spring to miss beans defined in sibling packages.

---

## 3. How It Works Internally

### The `SpringApplication.run()` Bootstrap Lifecycle

```
+-------------------------------------------------------------------------+
|                  1. Entry Point: public static void main()              |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|              2. Instantiate SpringApplication Instance                  |
|  - Determine ApplicationType: SERVLET, REACTIVE, or NONE                |
|  - Load BootstrapRegistryInitializers & ApplicationContextInitializers  |
|  - Load ApplicationListeners from `META-INF/spring.factories`           |
|  - Deduce main application class                                        |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                 3. Prepare Environment & Fire Events                    |
|  - Construct StandardServletEnvironment                                 |
|  - Load `application.properties` / `application.yaml`                   |
|  - Bind command-line arguments (`--server.port=9090`)                   |
|  - Fire `ApplicationEnvironmentPreparedEvent`                           |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|               4. Print Banner & Create ApplicationContext               |
|  - Render Spring Boot ASCII banner (or custom banner.txt)               |
|  - Instantiate AnnotationConfigServletWebServerApplicationContext       |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                5. Prepare Context & Register Core Beans                 |
|  - Apply ApplicationContextInitializers                                 |
|  - Register `@SpringBootApplication` primary source                    |
|  - Fire `ApplicationPreparedEvent`                                      |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                    6. Refresh ApplicationContext                        |
|  - Execute BeanFactoryPostProcessors (Process `@Configuration`)         |
|  - Evaluate Auto-Configuration conditionals (`@ConditionalOnClass`...)  |
|  - Instantiate Embedded Tomcat & bind server port (e.g., 8080)          |
|  - Instantiate singleton beans and inject dependencies                  |
|  - Start embedded Tomcat connector                                      |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                7. Post-Processing & Application Ready                   |
|  - Execute `CommandLineRunner` and `ApplicationRunner` beans            |
|  - Fire `ApplicationReadyEvent`                                         |
|  - Application is now listening for HTTP traffic on port 8080!          |
+-------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### The Modern Application Bootstrap Class
```java
package com.example.demo;

import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;

@SpringBootApplication
public class DemoApplication {

    public static void main(String[] args) {
        SpringApplication.run(DemoApplication.class, args);
    }
}
```

### Application Configuration (`src/main/resources/application.yaml`)
```yaml
server:
  port: 8080
  shutdown: graceful

spring:
  application:
    name: demo-service
  threads:
    virtual:
      enabled: true # Enable Java 21 Virtual Threads for all web requests
```

---

## 5. Practical Examples

### Example 1: Minimal Production REST Service with Java 21 Records
```java
package com.example.demo.api;

import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.time.Instant;
import java.util.UUID;

// 1. Immutable Data Transfer Object using Java 21 Record
public record SystemStatus(
    UUID instanceId,
    String status,
    String environment,
    Instant timestamp
) {}

// 2. Modern Spring REST Controller
@RestController
@RequestMapping("/api/v1/status")
public class StatusController {

    private final UUID instanceId = UUID.randomUUID();

    @GetMapping
    public ResponseEntity<SystemStatus> getStatus() {
        SystemStatus status = new SystemStatus(
            instanceId,
            "UP",
            "production",
            Instant.now()
        );
        return ResponseEntity.ok(status);
    }
}
```

### Example 2: Constructor Dependency Injection with Service Layer
```java
package com.example.demo.service;

import org.springframework.stereotype.Service;
import java.util.List;

public record Product(String id, String name, double price) {}

@Service
public class ProductService {

    // Immutable in-memory catalog
    private final List<Product> catalog = List.of(
        new Product("P100", "Enterprise Gateway", 499.99),
        new Product("P200", "Cloud Connector", 199.50)
    );

    public List<Product> listProducts() {
        return catalog;
    }

    public Product findById(String id) {
        return catalog.stream()
            .filter(p -> p.id().equalsIgnoreCase(id))
            .findFirst()
            .orElseThrow(() -> new IllegalArgumentException("Product not found: " + id));
    }
}
```
```java
package com.example.demo.controller;

import com.example.demo.service.Product;
import com.example.demo.service.ProductService;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.util.List;

@RestController
@RequestMapping("/api/v1/products")
public class ProductController {

    private final ProductService productService;

    // Idiomatic Constructor Injection (no @Autowired needed on single constructor)
    public ProductController(ProductService productService) {
        this.productService = productService;
    }

    @GetMapping
    public ResponseEntity<List<Product>> getProducts() {
        return ResponseEntity.ok(productService.listProducts());
    }

    @GetMapping("/{id}")
    public ResponseEntity<Product> getProductById(@PathVariable String id) {
        return ResponseEntity.ok(productService.findById(id));
    }
}
```

### Example 3: Running Startup Initialization via `ApplicationRunner`
```java
package com.example.demo.config;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.boot.ApplicationArguments;
import org.springframework.boot.ApplicationRunner;
import org.springframework.stereotype.Component;

@Component
public class CacheWarmupRunner implements ApplicationRunner {

    private static final Logger log = LoggerFactory.getLogger(CacheWarmupRunner.class);

    @Override
    public void run(ApplicationArguments args) {
        log.info("Application context initialized. Parsing startup arguments...");
        if (args.containsOption("warmup")) {
            log.info("Warmup flag detected. Pre-warming application cache entries...");
        } else {
            log.info("Standard startup mode. Skipping cache pre-warm.");
        }
    }
}
```

### Example 4: Startup Failure Handling and Custom Exit Codes
```java
package com.example.demo;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.boot.ExitCodeGenerator;
import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;
import org.springframework.context.annotation.Bean;

@SpringBootApplication
public class ResilientApplication {

    private static final Logger log = LoggerFactory.getLogger(ResilientApplication.class);

    public static void main(String[] args) {
        try {
            SpringApplication.run(ResilientApplication.class, args);
        } catch (Exception ex) {
            log.error("Fatal exception during bootstrap: {}", ex.getMessage());
            // JVM will exit with custom code from ExitCodeGenerator
        }
    }

    @Bean
    public ExitCodeGenerator customExitCodeGenerator() {
        return () -> 42; // Returns exit code 42 upon controlled abnormal termination
    }
}
```

### Example 5: Customizing the `SpringApplication` Instance Programmatically
```java
package com.example.demo;

import org.springframework.boot.Banner;
import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;
import org.springframework.core.env.MapPropertySource;

import java.util.Map;

@SpringBootApplication
public class CustomBootstrapApplication {

    public static void main(String[] args) {
        SpringApplication app = new SpringApplication(CustomBootstrapApplication.class);
        
        // Disable ASCII banner in CI/CD terminal logs
        app.setBannerMode(Banner.Mode.OFF);
        
        // Inject programmatic fallback properties
        app.setDefaultProperties(Map.of(
            "server.port", "8085",
            "spring.application.name", "programmatic-bootstrap"
        ));
        
        app.run(args);
    }
}
```

---

## 6. Best Practices

1. **Place `@SpringBootApplication` at the Root Package**: Avoid scan failures by locating your main class at the root package (e.g., `com.example.app`).
2. **Always Use Constructor Injection**: Avoid field injection (`@Autowired private MyService myService;`). Constructor injection guarantees immutability, facilitates unit testing, and prevents uninitialized fields.
3. **Model DTOs with Java Records**: Use Java 21 `record` for all request bodies and response payloads. Records are immutable, type-safe, and automatically serialize to clean JSON.
4. **Enable Virtual Threads**: Set `spring.threads.virtual.enabled=true` in `application.yaml` to handle tens of thousands of concurrent HTTP connections without thread pool starvation.
5. **Prefer `ApplicationRunner` over `CommandLineRunner`**: `ApplicationRunner` provides structured access to option arguments (`--key=value`) and non-option arguments via `ApplicationArguments`.
6. **Graceful Shutdown**: Always configure `server.shutdown=graceful` in production to allow active requests to finish before Tomcat closes socket connections.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Placing Main Class in Sub-package** | `@ComponentScan` only scans the package containing the main class and its children; sibling packages are completely ignored. | Move `Application.java` to the parent root package (e.g., `com.example.service`). |
| **Port Conflict (`PortInUseException`)** | Port 8080 is already bound by another local process, preventing Tomcat from starting. | Change port via `server.port=8081` or pass `--server.port=0` for an ephemeral random port. |
| **Heavy Logic in Constructor** | Executing blocking I/O (database queries, network calls) inside bean constructors blocks application startup. | Move initialization logic to an `ApplicationRunner` or `@EventListener(ApplicationReadyEvent.class)`. |
| **Field Injection (`@Autowired`)** | Prevents unit testing without reflection, hides dependency complexity, and violates object immutability. | Use explicit constructor injection; Spring will inject arguments automatically. |
| **Missing Default Package** | Placing classes in the "default" (unnamed root) package causes Spring component scanning to attempt scanning every JAR on the entire classpath, crashing startup. | Always define an explicit package name (e.g., `package com.example.demo;`). |
| **Uncaught Exception in `main()`** | Silently swallowing exceptions in `main()` prevents container orchestrators (Kubernetes) from detecting startup failures. | Allow bootstrap exceptions to propagate, or catch and call `System.exit(SpringApplication.exit(ctx))`. |

---

## 8. Performance & Security Notes

### Startup Time Profiling
Spring Boot provides the `ApplicationStartup` abstraction to record startup phase metrics:
```java
SpringApplication app = new SpringApplication(DemoApplication.class);
app.setApplicationStartup(new BufferingApplicationStartup(2048));
app.run(args);
```
Startup execution steps can then be queried via the Actuator `/actuator/startup` endpoint to identify beans with slow initialization times.

### Security Defaults
By default, adding `spring-boot-starter-security` automatically locks down ALL endpoints with HTTP Basic authentication and generates a random UUID password in the console. Do not deploy to production without defining an explicit `SecurityFilterChain`.

---

## 9. Comparison / When to Use What

| Startup Mechanism | Trigger Timing | Arguments Received | Best For |
| :--- | :--- | :--- | :--- |
| **`@PostConstruct`** | When specific bean is created | None | Internal bean state validation |
| **`CommandLineRunner`** | After Context refreshes, before app is "Ready" | Raw `String[] args` | Quick scripts, simple CLI argument checking |
| **`ApplicationRunner`** | After Context refreshes, before app is "Ready" | Structured `ApplicationArguments` | Enterprise initialization, reading parsed flags |
| **`ApplicationReadyEvent`** | Context fully booted, Web server listening | Event object | Publishing "ready" telemetry, warm-up caches |

---

## 10. Interview Questions

<details>
<summary><strong>Q1: What three annotations are combined inside `@SpringBootApplication`?</strong></summary>

**Answer:**  
1. `@SpringBootConfiguration`: Marks the class as a configuration class providing bean definitions.
2. `@EnableAutoConfiguration`: Enables Spring Boot's opinionated auto-configuration based on classpath JARs.
3. `@ComponentScan`: Scans the current package and its sub-packages for `@Component`, `@Service`, `@Repository`, and `@RestController` annotations.
</details>

<details>
<summary><strong>Q2: What happens under the hood when `SpringApplication.run()` is called?</strong></summary>

**Answer:**  
1. Instantiates a `SpringApplication` and determines the web environment type (`SERVLET`).
2. Configures the `Environment` (loads properties, parses CLI flags).
3. Instantiates the `ApplicationContext` (`AnnotationConfigServletWebServerApplicationContext`).
4. Runs `BeanFactoryPostProcessors` and evaluates auto-configurations.
5. Initializes and starts the embedded web container (Tomcat) on the configured port.
6. Instantiates and wires all singleton beans.
7. Executes all `CommandLineRunner` and `ApplicationRunner` beans.
8. Publishes `ApplicationReadyEvent`.
</details>

<details>
<summary><strong>Q3: Why is constructor injection preferred over field injection with `@Autowired`?</strong></summary>

**Answer:**  
1. **Immutability**: Dependencies can be declared `final`, ensuring thread safety and preventing reassignment.
2. **Testability**: Dependencies can be easily mocked or passed in standard unit tests without requiring Mockito reflection or Spring contexts.
3. **Null-Safety**: Prevents `NullPointerException` because the compiler mandates all required arguments at instantiation time.
4. **Code Smell Detection**: Having too many constructor parameters immediately signals a violation of the Single Responsibility Principle.
</details>

<details>
<summary><strong>Q4: How does Spring Boot differentiate between a web application and a CLI application?</strong></summary>

**Answer:**  
Spring Boot checks the classpath for specific indicator classes:
- If `org.springframework.web.context.ConfigurableWebApplicationContext` and `DispatcherServlet` are present, it selects `WebApplicationType.SERVLET`.
- If `org.springframework.web.reactive.DispatcherHandler` is present without Servlet classes, it selects `WebApplicationType.REACTIVE`.
- Otherwise, it selects `WebApplicationType.NONE`, running as a console/CLI application without starting an embedded web server.
</details>

<details>
<summary><strong>Q5: What is the difference between `CommandLineRunner` and `ApplicationRunner`?</strong></summary>

**Answer:**  
Both run immediately after the ApplicationContext has refreshed and before the application is declared ready. The difference is argument parsing:
- `CommandLineRunner` passes raw `String... args`.
- `ApplicationRunner` passes an `ApplicationArguments` instance, which provides convenient access to option flags (e.g., `--port=8080`), parsed keys, and non-option arguments.
</details>

<details>
<summary><strong>Q6: How can you run a Spring Boot application on a random free port, and how do you discover that port?</strong></summary>

**Answer:**  
Set `server.port=0` in `application.properties` or `application.yaml`. During startup, the embedded server binds to an ephemeral OS-assigned port. In code or tests, you can retrieve the bound port by injecting `@LocalServerPort int port` or reading `environment.getProperty("local.server.port")`.
</details>

<details>
<summary><strong>Q7: Can a Spring Boot application run without an embedded Tomcat server?</strong></summary>

**Answer:**  
Yes. You can:
1. Exclude `spring-boot-starter-tomcat` and include `spring-boot-starter-jetty` or `spring-boot-starter-undertow`.
2. Use `spring-boot-starter-webflux`, which boots an embedded Netty server by default.
3. Exclude web starters entirely to run a non-web console/batch application (`WebApplicationType.NONE`).
</details>

<details>
<summary><strong>Q8: What is graceful shutdown in Spring Boot, and how is it configured?</strong></summary>

**Answer:**  
Graceful shutdown stops the embedded web server from accepting new HTTP connections while granting currently executing in-flight requests a grace period to complete. It is enabled with:
```yaml
server:
  shutdown: graceful
spring:
  lifecycle:
    timeout-per-shutdown-phase: 30s
```
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Create a Hello World REST Endpoint (Easy)</strong></summary>

**Task:**  
Bootstrap a Spring Boot application. Create a `@RestController` with a `@GetMapping("/hello")` returning a record `record Greeting(String message, Instant timestamp)`. Verify using `curl http://localhost:8080/hello`.
</details>

<details>
<summary><strong>Exercise 2: Implement Startup Metric Logger (Easy)</strong></summary>

**Task:**  
Create an `ApplicationRunner` bean that logs the active Spring profiles, the total number of beans defined in the `ApplicationContext`, and the Java runtime version upon startup.
</details>

<details>
<summary><strong>Exercise 3: Ephemeral Port Testing (Medium)</strong></summary>

**Task:**  
Configure `server.port=0`. Write a `@SpringBootTest(webEnvironment = SpringBootTest.WebEnvironment.RANDOM_PORT)` test. Inject `@LocalServerPort int port` and use `RestClient` to verify an endpoint responds on the ephemeral port.
</details>

<details>
<summary><strong>Exercise 4: Graceful Shutdown Verification (Medium)</strong></summary>

**Task:**  
Enable `server.shutdown=graceful`. Create an endpoint `/api/long-task` that sleeps for 5 seconds. Trigger the endpoint, immediately send a `SIGTERM` (`kill <PID>`), and verify that the request completes with HTTP 200 before the application terminates.
</details>

<details>
<summary><strong>Exercise 5: Build a Custom ExitCodeGenerator (Hard)</strong></summary>

**Task:**  
Implement an `ExitCodeGenerator` bean that listens for a custom fatal business condition. Trigger `SpringApplication.exit(ctx, exitCodeGenerators)` and verify that the operating system shell returns the exact expected non-zero exit code (`echo $?`).
</details>

<details>
<summary><strong>Exercise 6: Programmatic Application Builder (Hard)</strong></summary>

**Task:**  
Refactor `main()` to use `SpringApplicationBuilder` to define a hierarchy of parent and child ApplicationContexts, disabling the web environment on the parent while enabling Servlet on the child.
</details>

---

## 12. Summary / Cheat Sheet

| Feature / Pattern | Syntax / Code Snippet | Purpose |
| :--- | :--- | :--- |
| **Main Bootstrap** | `SpringApplication.run(App.class, args);` | Boots embedded server & IoC context |
| **Record DTO** | `public record Response(String id, Instant time) {}` | Immutable, clean JSON serialization |
| **Constructor DI** | `public MyController(MyService service) { ... }` | Safe, immutable dependency injection |
| **Random Ephemeral Port** | `server.port: 0` | Binds to free dynamic OS port |
| **Graceful Shutdown** | `server.shutdown: graceful` | Finishes active requests on SIGTERM |
| **Virtual Threads** | `spring.threads.virtual.enabled: true` | Offloads request threads to virtual threads |
| **Startup Runner** | `implements ApplicationRunner` | Executes custom startup code before ready |

---

## 13. Further Reading
- [Official Spring Boot Application Reference](https://docs.spring.io/spring-boot/docs/current/reference/html/features.html#features.spring-application)
- [Spring Framework IoC Container Internals](https://docs.spring.io/spring-framework/reference/core/beans.html)
- [Java 21 Record Specification (JEP 395)](https://openjdk.org/jeps/395)
