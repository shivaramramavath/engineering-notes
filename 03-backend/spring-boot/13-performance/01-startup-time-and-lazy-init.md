# Spring Boot Startup Time Optimization and Lazy Initialization

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x |
| **Spring Framework Version** | 6.2.x |
| **Java Version** | 21 LTS |
| **Module** | 13-performance |
| **Status** | Complete |

---

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Configuration Reference](#4-syntax--configuration-reference)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices & Production Guidelines](#6-best-practices--production-guidelines)
- [7. Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
- [8. Performance & Security Notes](#8-performance--security-notes)
- [9. Comparison / When to Use What](#9-comparison--when-to-use-what)
- [10. Interview Questions & Answers](#10-interview-questions--answers)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading](#13-further-reading)

---

## 1. What & Why

### Problem Statement
In serverless environments (AWS Lambda, Google Cloud Run) and dynamic autoscaling Kubernetes clusters, **cold-start latency** directly dictates user experience and cloud infrastructure costs. A standard enterprise Spring Boot application containing 500+ beans, Hibernate JPA entities, and third-party integrations typically takes 10 to 30 seconds to boot:
- Slow autoscaling fails to absorb sudden traffic surges, causing 504 Gateway Timeouts during scale-out events.
- Cold starts in serverless functions produce intolerable multi-second latencies on initial invocation.
- Developer feedback loops suffer during local development and integration test execution.

### Historical Evolution
In early Spring Framework architectures, eager singleton initialization was considered an essential virtue: fail-fast verification ensured that missing beans, broken cyclic dependencies, and invalid configurations were caught immediately on process start.

However, modern cloud-native architectures demand rapid elasticity. Spring Boot 2.2 introduced global lazy initialization (`spring.main.lazy-initialization=true`). 

Spring Boot 3.x introduced **Spring Application Startup Tracking** (`ApplicationStartup`), flight recorder integration (`BufferingApplicationStartup`), and Ahead-of-Time (AOT) optimizations that systematically analyze and eliminate startup bottlenecks without sacrificing runtime stability.

### Production Relevance
Mastering startup optimization allows engineering teams to:
- Slash cold-start times from 25 seconds down to under 2.5 seconds on standard JVMs.
- Profile exact bean initialization durations via `/actuator/startup`.
- Strategically exclude unused autoconfigurations, unburdening classpath scanning and reflection overhead.

---

## 2. Core Concepts

### Architectural Building Blocks
1. **Eager vs. Lazy Initialization**:
   - **Eager (Default)**: All singleton beans are instantiated, injected, and initialized (`@PostConstruct`, `afterPropertiesSet`) during `ApplicationContext.refresh()`.
   - **Lazy**: Beans are instantiated only when first requested by another bean or incoming HTTP request.
2. **`spring.main.lazy-initialization=true`**: Globally flips all Spring beans to lazy initialization mode.
3. **`@Lazy(false)`**: Explicit override to force critical beans (e.g., warmup routines, background scheduled tasks, message listeners) to remain eager even when global lazy mode is active.
4. **`ApplicationStartup`**: Spring abstraction that records startup timeline events and step durations (`BufferingApplicationStartup`, `FlightRecorderApplicationStartup`).
5. **Autoconfiguration Exclusion**: Explicitly omitting unnecessary auto-configuration classes to avoid redundant condition evaluations and bean instantiations.

---

## 3. How It Works Internally

### ApplicationContext Startup Sequence & Lazy Resolution

```
APPLICATION CONTEXT REFRESH
  |
  +--> 1. Classpath Scanning & BeanDefinitionRegistryPostProcessor
  |       Scans packages, locates @Component, @Configuration, parses definitions
  |
  +--> 2. PostProcessBeanFactory
  |       [Global Lazy Enabled?]
  |       --> YES: Iterates BeanDefinitions; sets bd.setLazyInit(true)
  |                (Unless bean definition is explicitly @Lazy(false))
  |
  +--> 3. finishBeanFactoryInitialization()
  |       - Eager Mode: Instantiates ALL singletons sequentially in topological order
  |       - Lazy Mode : Instantiates ONLY beans explicitly marked eager (e.g. Listeners)
  |                     Remaining beans remain uninstantiated definitions in memory!
  |
  +--> 4. HTTP REQUEST ARRIVES (Runtime)
          GET /api/orders/101
          DispatcherServlet routes to OrderController
          |
          v
          Spring detects OrderService bean has not been instantiated:
          1. Allocates OrderService instance
          2. Resolves dependencies (OrderRepository, PaymentClient)
          3. Executes @PostConstruct
          4. Returns bean and caches singleton reference in DefaultSingletonBeanRegistry
```

---

## 4. Syntax & Configuration Reference

### Configuration (`application.yml`)

```yaml
spring:
  main:
    lazy-initialization: true # Enables global lazy bean initialization
    banner-mode: "off" # Saves 10-20ms of stdout I/O
  autoconfigure:
    exclude: # Exclude unused starters to save startup condition evaluation
      - org.springframework.boot.autoconfigure.websocket.servlet.WebSocketServletAutoConfiguration
      - org.springframework.boot.autoconfigure.mail.MailSenderAutoConfiguration
      - org.springframework.boot.autoconfigure.jms.JmsAutoConfiguration
  jpa:
    open-in-view: false
    hibernate:
      ddl-auto: none # Never run schema validation on startup in production

management:
  endpoint:
    startup:
      enabled: true
  endpoints:
    web:
      exposure:
        include: [ "health", "info", "startup" ]
```

---

## 5. Practical Examples

### Example 1: Profiling Startup Bottlenecks with `BufferingApplicationStartup`
```java
package com.example.performance.startup;

import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;
import org.springframework.boot.context.metrics.buffering.BufferingApplicationStartup;

@SpringBootApplication
public class OptimizedStartupApplication {

    public static void main(String[] args) {
        SpringApplication app = new SpringApplication(OptimizedStartupApplication.class);
        // Buffers startup timeline events in memory (up to 4096 events)
        app.setApplicationStartup(new BufferingApplicationStartup(4096));
        app.run(args);
    }
}
```

**Querying Startup Metrics via Actuator**:
```bash
curl -X POST http://localhost:9090/actuator/startup
```

```json
{
  "springBootVersion": "3.4.1",
  "timeline": {
    "startTime": "2026-03-24T10:00:00.000Z",
    "events": [
      {
        "endTime": "2026-03-24T10:00:02.450Z",
        "duration": "PT2.45S",
        "startupStep": {
          "name": "spring.beans.instantiate",
          "id": 14,
          "tags": [
            { "key": "beanName", "value": "heavyReportGenerator" }
          ]
        }
      }
    ]
  }
}
```
*Analysis*: `heavyReportGenerator` took 2.45 seconds to initialize. Marking it `@Lazy` moves that cost off the startup critical path.

### Example 2: Strategic Eager Overrides with `@Lazy(false)`
When `spring.main.lazy-initialization: true` is enabled, certain background workers or listeners must still boot immediately:

```java
package com.example.performance.startup;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.context.annotation.Lazy;
import org.springframework.kafka.annotation.KafkaListener;
import org.springframework.scheduling.annotation.Scheduled;
import org.springframework.stereotype.Component;

@Component
@Lazy(false) // MANDATORY: Force eager initialization even when global lazy is active!
public class InboundKafkaConsumerWorker {

    private static final Logger log = LoggerFactory.getLogger(InboundKafkaConsumerWorker.class);

    @KafkaListener(topics = "order-events", groupId = "order-group")
    public void consumeOrderEvent(String event) {
        log.info("Received event on eager consumer: {}", event);
    }

    @Scheduled(fixedRate = 60000)
    public void heartbeat() {
        log.debug("Heartbeat ping from eager worker");
    }
}
```

### Example 3: Defeating JPA Metamodel / Flyway Startup Lag
```yaml
# application.yml
spring:
  jpa:
    # Defers EntityManagerFactory initialization to a background thread!
    # Available in Spring Boot 3 with Hibernate 6.2+
    defer-datasource-initialization: true
    properties:
      hibernate:
        temp:
          use_jdbc_metadata_defaults: false # Skips expensive DB metadata queries on startup
```

### Example 4: Anti-Pattern vs. Production Fix (Heavy Blocking `@PostConstruct`)
```java
// ANTI-PATTERN: Heavy network or disk I/O inside @PostConstruct blocks context startup!
// The entire application startup is halted waiting for remote external service!
@Service
public class BadInitializingBean {

    private final RestClient restClient;

    public BadInitializingBean(RestClient.Builder builder) {
        this.restClient = builder.baseUrl("https://remote-partner.com").build();
    }

    @PostConstruct
    public void init() {
        // ANTI-PATTERN: Synchronous 5-second HTTP call during Spring container boot!
        String catalog = restClient.get().uri("/catalog").retrieve().body(String.class);
        populateLocalCache(catalog);
    }
}

// PRODUCTION FIX: Asynchronous background warm-up triggered after ApplicationReadyEvent
@Service
public class ResilientWarmupBean {

    private static final Logger log = LoggerFactory.getLogger(ResilientWarmupBean.class);
    private final RestClient restClient;

    public ResilientWarmupBean(RestClient.Builder builder) {
        this.restClient = builder.baseUrl("https://remote-partner.com").build();
    }

    @EventListener(ApplicationReadyEvent.class) // Fired AFTER application context is fully started!
    public void onApplicationReady() {
        // Runs on separate worker thread; does NOT block application boot or readiness probe
        CompletableFuture.runAsync(() -> {
            log.info("Asynchronously warming catalog cache in background...");
            try {
                String catalog = restClient.get().uri("/catalog").retrieve().body(String.class);
                populateLocalCache(catalog);
                log.info("Catalog cache successfully initialized.");
            } catch (Exception ex) {
                log.error("Background cache warm-up failed", ex);
            }
        });
    }

    private void populateLocalCache(String catalog) {
        // Store in memory
    }
}
```

### Example 5: High-Performance Component Scanning Pruning
```java
package com.example.performance.config;

import org.springframework.boot.autoconfigure.SpringBootApplication;
import org.springframework.context.annotation.ComponentScan;
import org.springframework.context.annotation.FilterType;

@SpringBootApplication
@ComponentScan(
    basePackages = "com.example.service",
    excludeFilters = {
        // Skip scanning heavy test or mock packages in production
        @ComponentScan.Filter(type = FilterType.REGEX, pattern = "com\\.example\\.service\\.mock\\..*"),
        @ComponentScan.Filter(type = FilterType.REGEX, pattern = "com\\.example\\.service\\.experimental\\..*")
    }
)
public class PrunedApplication {
}
```

---

## 6. Best Practices & Production Guidelines

1. **Profile Before Optimizing**: Never guess what is slowing down startup. Use `BufferingApplicationStartup` and inspect `/actuator/startup` to identify the specific bean instantiation steps that account for 80% of startup time.
2. **Move I/O Out of `@PostConstruct`**: Bean constructors and `@PostConstruct` methods must be lightweight. Never perform database queries, file downloads, or network calls inside `@PostConstruct`. Defer warm-up to `@EventListener(ApplicationReadyEvent.class)`.
3. **Beware of First-Request Latency with Global Lazy**: While `spring.main.lazy-initialization=true` slashes startup time, the very first user request to each endpoint will incur the cost of instantiating that controller and its entire dependency tree. Warm up critical endpoints during Kubernetes startup probes if necessary.
4. **Exclude Unused Autoconfigurations**: If your microservice does not use JMS, WebSockets, or JavaMail, exclude their autoconfigurations. Each autoconfiguration evaluates dozens of `@ConditionalOnClass` conditions during startup.
5. **Disable DDL Generation in Production**: Ensure `spring.jpa.hibernate.ddl-auto=none`. Letting Hibernate validate or auto-update tables on startup creates severe database locking and network roundtrip delays.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Relying on `@PostConstruct` for remote network calls | If the remote dependency is slow or offline, application boot halts and container fails liveness probe. | Move network initialization to an async `ApplicationReadyEvent` listener. |
| Forgetting `@Lazy(false)` on scheduled tasks | With global lazy enabled, `@Scheduled` methods or Kafka listeners never run because the bean is never requested. | Mark background workers and event listeners explicitly with `@Lazy(false)`. |
| Unbounded `@ComponentScan` root package | Scanning `com.example` scans thousands of classes in dependencies, burning CPU on reflection. | Restrict base package to exact domain package: `com.example.orders`. |
| Enabling `ddl-auto: update` in production | Hibernate executes hundreds of metadata queries against the database during startup. | Use Flyway/Liquibase migrations; set `hibernate.ddl-auto=none`. |
| Hidden circular dependencies masked by `@Lazy` | Circular dependencies can silently pass startup tests and fail with `BeanCurrentlyInCreationException` at runtime. | Refactor circular designs or redesign domain service boundaries. |
| Banner and ANSI color overhead in containers | Formatted banner rendering writes to stdout during boot, slowing down serverless startup. | Set `spring.main.banner-mode: "off"`. |

---

## 8. Performance & Security Notes

### Trade-offs: Startup Time vs. Runtime Latency
- Eager initialization: High startup cost ($\approx 15\text{s}$), deterministic and uniform sub-10ms first-request latency.
- Lazy initialization: Low startup cost ($\approx 2\text{s}$), but the first user request to hit an uninitialized bean incurs an initialization penalty of 50–500ms.

### Production Readiness Verification
- In eager mode, configuration errors (e.g. wrong DB password or missing required `@Value` property) crash the app immediately on boot.
- In lazy mode, configuration errors will not manifest until a user hits the affected code path hours later. Mitigate this by running thorough integration test suites in CI/CD.

---

## 9. Comparison / When to Use What

| Technique | Startup Time Reduction | First Request Impact | Recommended Environment |
|---|---|---|---|
| **Eager Initialization (Default)** | Baseline (Slowest) | Zero (Deterministic) | Long-running microservices with high constant load |
| **Global Lazy (`lazy-init=true`)** | 60% – 85% reduction | Moderate initial latency spike | Local development, CI test pipelines, serverless functions |
| **Targeted `@Lazy` on heavy beans** | 20% – 50% reduction | Isolated to that specific feature | Production microservices with known heavy optional beans |
| **GraalVM Native Image (AOT)** | 95% – 98% reduction ($< 50\text{ms}$) | Zero | Serverless AWS Lambda, Google Cloud Run, extreme scale-out |

---

## 10. Interview Questions & Answers

### Q1: What happens under the hood when `spring.main.lazy-initialization=true` is configured?
<details>
<summary>Click to view answer</summary>

During the `ApplicationContext.refresh()` phase, a `BeanFactoryPostProcessor` inspects all registered `BeanDefinition`s. It sets `lazyInit = true` on every bean definition that is not explicitly marked with `@Lazy(false)`. As a result, `DefaultListableBeanFactory.preInstantiateSingletons()` skips instantiating these beans during startup. They are created on-demand when first requested by another bean or HTTP request.
</details>

### Q2: Why can global lazy initialization be risky in production?
<details>
<summary>Click to view answer</summary>

1. **Delayed Failure**: Dependency injection errors, missing `@Value` properties, and cyclic dependencies will not cause startup failure. Instead, they will throw runtime `500 Internal Server Error`s when the first user accesses that code path in production.
2. **First-Request Latency Spike**: The first client to hit an endpoint pays the latency penalty for instantiating the entire dependency graph for that endpoint.
3. **Stalled Background Workers**: Background consumers (`@KafkaListener`, `@Scheduled`) might never be triggered if they are not explicitly marked `@Lazy(false)`.
</details>

### Q3: How do you identify which beans are taking the longest to instantiate during Spring Boot startup?
<details>
<summary>Click to view answer</summary>

Configure `BufferingApplicationStartup` in the `main` method:
```java
SpringApplication app = new SpringApplication(MyApp.class);
app.setApplicationStartup(new BufferingApplicationStartup(2048));
```
Then invoke the `/actuator/startup` endpoint (via HTTP POST). It returns a detailed timeline of every startup step, including `spring.beans.instantiate` steps with their exact durations and bean names.
</details>

### Q4: What is the impact of Hibernate's `ddl-auto` on startup time?
<details>
<summary>Click to view answer</summary>

When `ddl-auto` is set to `validate` or `update`, Hibernate executes metadata queries against the database catalog for every single mapped `@Entity` and table to verify column types, nullability, foreign keys, and indexes. In microservices with 50+ entities, this can add 5 to 15 seconds of startup latency. In production, setting `ddl-auto: none` skips these catalog queries completely.
</details>

### Q5: How does excluding unused Spring Boot autoconfigurations improve startup performance?
<details>
<summary>Click to view answer</summary>

Spring Boot checks dozens of `@ConditionalOnClass`, `@ConditionalOnBean`, and `@ConditionalOnProperty` annotations for every candidate auto-configuration class. Excluded classes (`spring.autoconfigure.exclude`) are bypassed completely during condition evaluation, reducing reflection, class loading, and CPU cycles during the startup phase.
</details>

### Q6: What is the difference between `@PostConstruct` and `@EventListener(ApplicationReadyEvent.class)` regarding startup?
<details>
<summary>Click to view answer</summary>

- `@PostConstruct`: Runs synchronously during bean creation while the `ApplicationContext` is still initializing. Any slow I/O inside `@PostConstruct` directly delays the completion of application startup and delays the Kubernetes readiness probe.
- `ApplicationReadyEvent`: Fires **after** the `ApplicationContext` is fully refreshed and the embedded web server is running. Long-running or async initialization tasks placed here will not block the startup sequence.
</details>

### Q7: Can Class Data Sharing (CDS) improve Spring Boot startup time?
<details>
<summary>Click to view answer</summary>

Yes. Class Data Sharing (AppCDS) dumps parsed JVM class metadata into an optimized archive file (`app-cds.jsa`). On subsequent starts, the JVM memory-maps this archive directly, bypassing class verification and bytecode parsing, typically slashing JVM startup time by 30% to 50%.
</details>

### Q8: How does `@Lazy` work when applied to a single dependency injection point?
<details>
<summary>Click to view answer</summary>

When applied to a constructor parameter or field injection point:
```java
public OrderService(@Lazy HeavyReportService reportService) { ... }
```
Spring injects a CGLIB/dynamic proxy instead of the real bean. The actual `HeavyReportService` is not created until a method is first called on the injected proxy.
</details>

---

## 11. Practice Exercises

### Exercise 1: Configure application.yml for Rapid Cold Start (Easy)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Write an `application.yml` file enabling lazy initialization, disabling the banner, setting ddl-auto to none, and excluding `WebSocketServletAutoConfiguration`.

**Solution**:
```yaml
spring:
  main:
    lazy-initialization: true
    banner-mode: "off"
  jpa:
    hibernate:
      ddl-auto: none
    open-in-view: false
  autoconfigure:
    exclude:
      - org.springframework.boot.autoconfigure.websocket.servlet.WebSocketServletAutoConfiguration
```
</details>

### Exercise 2: Instrument ApplicationStartup in Main Class (Medium)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Modify a standard `SpringApplication` class to attach `BufferingApplicationStartup` with a capacity of 4,096 events.

**Solution**:
```java
package com.example.performance.startup;

import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;
import org.springframework.boot.context.metrics.buffering.BufferingApplicationStartup;

@SpringBootApplication
public class FastBootApplication {

    public static void main(String[] args) {
        SpringApplication app = new SpringApplication(FastBootApplication.class);
        app.setApplicationStartup(new BufferingApplicationStartup(4096));
        app.run(args);
    }
}
```
</details>

### Exercise 3: Implement Async Warmup Service with Fallback (Hard)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Create a `CatalogWarmupService` that starts cache preloading only after `ApplicationReadyEvent`, logs execution duration, and gracefully handles network timeout without failing application health.

**Solution**:
```java
package com.example.performance.startup;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.boot.context.event.ApplicationReadyEvent;
import org.springframework.context.event.EventListener;
import org.springframework.scheduling.annotation.Async;
import org.springframework.stereotype.Service;
import org.springframework.web.client.RestClient;

import java.time.Duration;
import java.time.Instant;
import java.util.concurrent.CompletableFuture;

@Service
public class CatalogWarmupService {

    private static final Logger log = LoggerFactory.getLogger(CatalogWarmupService.class);
    private final RestClient restClient;

    public CatalogWarmupService(RestClient.Builder builder) {
        this.restClient = builder.baseUrl("https://api.internal/catalog").build();
    }

    @EventListener(ApplicationReadyEvent.class)
    public void onStartupComplete() {
        CompletableFuture.runAsync(this::warmupCatalogData);
    }

    private void warmupCatalogData() {
        Instant start = Instant.now();
        log.info("Starting asynchronous catalog warm-up...");
        try {
            String data = restClient.get().uri("/preload")
                .retrieve()
                .body(String.class);

            long elapsed = Duration.between(start, Instant.now()).toMillis();
            log.info("Catalog warm-up completed successfully in {} ms", elapsed);
        } catch (Exception ex) {
            log.warn("Catalog warm-up failed or timed out. Application will fetch on-demand. Reason: {}", 
                ex.getMessage());
        }
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------+-------------------------------------------------------------+
| Property / Annotation                       | Description & Production Rule                               |
+---------------------------------------------+-------------------------------------------------------------+
| spring.main.lazy-initialization: true       | Defers bean instantiation until first use; slashes boot time|
| @Lazy(false)                                | Forces eager instantiation for background listeners/workers |
| spring.autoconfigure.exclude                | Omits unused starters to skip reflection condition checks   |
| BufferingApplicationStartup                 | Buffers startup timeline events for Actuator analysis       |
| /actuator/startup                           | POST endpoint returning exact bean instantiation durations  |
| spring.jpa.hibernate.ddl-auto: none         | Skips slow database catalog inspection on boot             |
| @EventListener(ApplicationReadyEvent.class) | Safe location for non-blocking post-startup cache warm-up   |
+---------------------------------------------+-------------------------------------------------------------+
```

---

## 13. Further Reading
- Spring Boot Official Documentation: Startup Time Optimization
- Spring Framework ApplicationStartup Metrics Reference
- Modernizing Java Microservices: Ahead-of-Time and Lazy Loading
- Martin Fowler: Microservice Cold Starts and Elasticity Trade-offs