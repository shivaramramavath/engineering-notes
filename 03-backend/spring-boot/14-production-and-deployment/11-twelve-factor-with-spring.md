# Twelve-Factor App Methodology with Spring Boot

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Version** | 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.x |
| **Tested With** | Kubernetes 1.31+, Docker 27.x, OCI / GraalVM 21 Native |

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
10. [Interview Questions & Deep Dives](#10-interview-questions--deep-dives)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### The Problem
Traditional enterprise Java applications were heavily coupled to deployment topologies. They relied on stateful in-memory HTTP sessions, hardcoded filesystem directories (`/var/app/data`), machine-specific database connection pools configured in monolithic application servers (JBoss, WebLogic), and complex JNDI lookups. Deploying such workloads across dynamic, ephemerally scheduled Kubernetes pods leads to data loss, configuration drift, noisy-neighbor contention, and brittle operational procedures.

### The Evolution
The **Twelve-Factor App methodology**, authored by Heroku engineers, codified software architecture principles designed for modern cloud-native systems. When combined with Spring Boot 3.x/4.x on Java 21 LTS, the twelve factors transform monolithic, environment-dependent services into stateless, observable, and rapidly scalable containerized microservices.

### Production Relevance
Adhering to Twelve-Factor principles is mandatory for applications running on Kubernetes, AWS ECS/Fargate, Google Cloud Run, and Azure Container Apps. It guarantees that an immutable application artifact can execute identically in local development, QA, staging, and high-availability production clusters without modification.

---

## 2. Core Concepts

The 12 factors mapped directly to Spring Boot and modern cloud runtimes:

```
+-----------------------------------------------------------------------------------------+
|                                  THE TWELVE FACTORS                                     |
+-----------------------------------------------------------------------------------------+
| I.   Codebase          | One codebase tracked in Git, many deploys                      |
| II.  Dependencies      | Explicitly declared & isolated (Maven/Gradle POMs)             |
| III. Config            | Stored in environment variables / ConfigMaps / Secrets         |
| IV.  Backing Services  | Attached resources treated as URLs (DB, Kafka, Redis)          |
| V.   Build, Release    | Strictly separated stages, immutable Docker image artifacts     |
| VI.  Processes         | Stateless & share-nothing; state outsourced to backing datastore|
| VII. Port Binding      | Export services via port binding (Embedded Netty/Tomcat)        |
| VIII.Concurrency       | Scale out via process model (K8s pod replicas, Virtual Threads)|
| IX.  Disposability     | Fast startup and graceful shutdown (SIGTERM handler)           |
| X.   Dev/Prod Parity   | Keep development, staging, and production as similar as possible|
| XI.  Logs              | Treat logs as event streams (stdout JSON via logback-json)     |
| XII. Admin Processes   | Run admin/management tasks as one-off processes (Flyway Job)   |
+-----------------------------------------------------------------------------------------+
```

### Invariants for Spring Boot Microservices
- **Immutable Artifact**: The exact same container image (`ghcr.io/myorg/order-service:1.4.2`) must be deployed across DEV, STAGE, and PROD. Only environment variables, ConfigMaps, and Vault secrets change.
- **Strictly Stateless Application Tier**: No user sessions stored in Java heap memory (`HttpSession`). Authentication is token-based (JWT) or distributed session-backed (Spring Session + Redis).
- **Graceful Termination**: Services must finish in-flight requests within a strict timeout upon receiving `SIGTERM` and exit cleanly with code `0`.

---

## 3. How It Works Internally

### Spring Boot Runtime Binding Architecture

```
                                      [ Environment ]
                                  (Env Vars / K8s ConfigMap)
                                             |
                                             v
[ Docker Container ] ------------> [ StandardEnvironment ]
                                             |
                                   Relaxed Property Binding
                                             |
                                             v
[ Embedded Tomcat/Netty ] <--- [@ConfigurationProperties] ---> [ Spring Beans ]
         |                                                               |
         v                                                               v
  Port Binding (8080)                                           [ Stateless Logic ]
         |                                                               |
    HTTP Traffic                                                  Backing Services
         |                                                    (PostgreSQL, Redis, Kafka)
         v                                                               |
  [ Client Request ]                                             [ Ext. Network ]
```

When Spring Boot initializes:
1. `SpringApplication.run()` instantiates `StandardEnvironment`, assembling property sources from Command Line Args, OS Environment Variables, and local configuration.
2. Spring's **Relaxed Binding** normalizes environment variables like `SPRING_DATASOURCE_URL` to match property paths `spring.datasource.url`.
3. Auto-configuration creates database pools (`HikariDataSource`), message listeners (`ConcurrentKafkaListenerContainerFactory`), and web servers (`TomcatWebServerFactoryCustomizer`) configured solely through external configuration.
4. An embedded servlet container binds directly to `server.port` (default 8080), avoiding external web application servers.
5. All logging is routed directly to `stdout` via `ConsoleAppender` with structured JSON format, ready for container log ingestion agents (FluentBit, Vector).

---

## 4. Syntax & Usage

### Factor III: Environment-Driven Configuration Properties

```java
package com.example.config;

import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.Positive;
import org.springframework.boot.context.properties.ConfigurationProperties;
import org.springframework.validation.annotation.Validated;

import java.time.Duration;

@Validated
@ConfigurationProperties(prefix = "app.order")
public record OrderProcessingProperties(
    @NotBlank String processingQueue,
    @Positive int maxBatchSize,
    Duration workerTimeout,
    RetryConfig retry
) {
    public record RetryConfig(
        int maxAttempts,
        Duration backoffInterval
    ) {}
}
```

Enable via `@ConfigurationPropertiesScan` in the main application class.

### Factor IX: Graceful Shutdown Configuration (`application.yaml`)

```yaml
server:
  port: 8080
  shutdown: graceful

spring:
  lifecycle:
    timeout-per-shutdown-phase: 30s
  threads:
    virtual:
      enabled: true

management:
  endpoints:
    web:
      exposure:
        include: health,info,prometheus
  endpoint:
    health:
      probes:
        enabled: true
      show-details: when_authorized
```

---

## 5. Practical Examples

### Example 1: Factor III & IV - External Backing Services Configuration

```java
package com.example.infrastructure;

import io.lettuce.core.RedisClient;
import io.lettuce.core.RedisURI;
import org.springframework.boot.autoconfigure.data.redis.RedisProperties;
import org.springframework.boot.context.properties.EnableConfigurationProperties;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;

import java.net.URI;

@Configuration
@EnableConfigurationProperties(RedisProperties.class)
public class BackingServiceConfig {

    @Bean(destroyMethod = "shutdown")
    public RedisClient redisClient(RedisProperties properties) {
        // Factor IV: Backing services attached via environment URLs
        String redisUrl = properties.getUrl();
        if (redisUrl != null && !redisUrl.isBlank()) {
            return RedisClient.create(RedisURI.create(URI.create(redisUrl)));
        }
        
        RedisURI uri = RedisURI.builder()
                .withHost(properties.getHost())
                .withPort(properties.getPort())
                .withSsl(properties.getSsl().isEnabled())
                .build();
        return RedisClient.create(uri);
    }
}
```

### Example 2: Factor VI - Stateless Authentication with JWT Validation

```java
package com.example.security;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.security.config.annotation.web.builders.HttpSecurity;
import org.springframework.security.config.annotation.web.configuration.EnableWebSecurity;
import org.springframework.security.config.http.SessionCreationPolicy;
import org.springframework.security.oauth2.jwt.JwtDecoder;
import org.springframework.security.oauth2.jwt.NimbusJwtDecoder;
import org.springframework.security.web.SecurityFilterChain;

@Configuration
@EnableWebSecurity
public class StatelessSecurityConfig {

    @Bean
    public SecurityFilterChain securityFilterChain(HttpSecurity http, JwtDecoder jwtDecoder) throws Exception {
        return http
            .csrf(csrf -> csrf.disable())
            // Factor VI: Share-nothing, completely stateless session policy
            .sessionManagement(sm -> sm.sessionCreationPolicy(SessionCreationPolicy.STATELESS))
            .authorizeHttpRequests(auth -> auth
                .requestMatchers("/actuator/health/**", "/actuator/prometheus").permitAll()
                .anyRequest().authenticated()
            )
            .oauth2ResourceServer(oauth2 -> oauth2.jwt(jwt -> jwt.decoder(jwtDecoder)))
            .build();
    }
}
```

### Example 3: Factor IX - Disposability & Graceful Resource Cleanup

```java
package com.example.worker;

import jakarta.annotation.PreDestroy;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.context.SmartLifecycle;
import org.springframework.stereotype.Component;

import java.util.concurrent.atomic.AtomicBoolean;

@Component
public class QueueConsumptionWorker implements SmartLifecycle {

    private static final Logger log = LoggerFactory.getLogger(QueueConsumptionWorker.class);
    private final AtomicBoolean running = new AtomicBoolean(false);

    @Override
    public void start() {
        running.set(true);
        log.info("Queue consumption worker initialized and accepting tasks.");
    }

    @Override
    public void stop() {
        log.info("SIGTERM received. Draining worker queue cleanly...");
        running.set(false);
    }

    @Override
    public boolean isRunning() {
        return running.get();
    }

    @Override
    public int getPhase() {
        // Run early in startup, stop late in graceful shutdown
        return Integer.MAX_VALUE - 100;
    }

    @PreDestroy
    public void onExit() {
        log.info("Queue worker fully stopped. Releasing thread pool resources.");
    }
}
```

### Example 4: Anti-Pattern (Stateful Memory Leak) vs Twelve-Factor Fix

#### Anti-Pattern: Local Filesystem and In-Memory State
```java
// VIOLATION of Factor VI (Stateless) and Factor XI (Logs as streams)
@RestController
public class BadUploadController {

    private final Map<String, UserSession> activeSessions = new ConcurrentHashMap<>();

    @PostMapping("/upload")
    public String uploadFile(@RequestParam("file") MultipartFile file) throws IOException {
        // Storing on local disk ties processing to this specific pod instance
        File dest = new File("/tmp/uploads/" + file.getOriginalFilename());
        file.transferTo(dest);
        
        // Writing directly to file system log instead of stdout stream
        FileWriter fw = new FileWriter("/var/log/app.log", true);
        fw.write("Uploaded file: " + file.getOriginalFilename() + "\n");
        fw.close();

        return "OK";
    }
}
```

#### Production Fix: S3 Object Storage & Stream Logs
```java
@RestController
public class TwelveFactorUploadController {

    private static final Logger log = LoggerFactory.getLogger(TwelveFactorUploadController.class);
    private final S3Client s3Client;
    private final String bucketName;

    public TwelveFactorUploadController(S3Client s3Client, 
                                        @Value("${app.storage.s3-bucket}") String bucketName) {
        this.s3Client = s3Client;
        this.bucketName = bucketName;
    }

    @PostMapping(value = "/upload", consumes = MediaType.MULTIPART_FORM_DATA_VALUE)
    public ResponseEntity<Map<String, String>> uploadFile(@RequestParam("file") MultipartFile file) throws IOException {
        String objectKey = UUID.randomUUID() + "-" + file.getOriginalFilename();
        
        // Attached backing service (AWS S3) handles persistent blob state
        s3Client.putObject(
            PutObjectRequest.builder().bucket(bucketName).key(objectKey).build(),
            RequestBody.fromInputStream(file.getInputStream(), file.getSize())
        );

        // Logs sent directly to stdout stream as structured events
        log.info("File uploaded successfully to backing store: key={}, size={}", objectKey, file.getSize());

        return ResponseEntity.ok(Map.of("status", "SUCCESS", "key", objectKey));
    }
}
```

### Example 5: Factor XII - Executing Administrative Tasks via Spring Boot CLI/Runner

```java
package com.example.admin;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.boot.CommandLineRunner;
import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;
import org.springframework.context.annotation.Profile;
import org.springframework.jdbc.core.JdbcTemplate;
import org.springframework.stereotype.Component;

@Profile("admin-task")
@Component
public class DataSanitizationTask implements CommandLineRunner {

    private static final Logger log = LoggerFactory.getLogger(DataSanitizationTask.class);
    private final JdbcTemplate jdbcTemplate;

    public DataSanitizationTask(JdbcTemplate jdbcTemplate) {
        this.jdbcTemplate = jdbcTemplate;
    }

    @Override
    public void run(String... args) {
        log.info("Starting Factor XII admin task: One-off PII redaction job.");
        int updatedRows = jdbcTemplate.update(
            "UPDATE customer_records SET ssn_hash = SHA256(raw_ssn) WHERE ssn_hash IS NULL"
        );
        log.info("Redacted {} customer records successfully.", updatedRows);
    }
}
```

Run as a Kubernetes Job:
```yaml
apiVersion: batch/v1
kind: Job
metadata:
  name: customer-redaction-admin-task
spec:
  template:
    spec:
      containers:
      - name: spring-app
        image: ghcr.io/myorg/customer-service:1.4.2
        env:
        - name: SPRING_PROFILES_ACTIVE
          value: "prod,admin-task"
        - name: SPRING_MAIN_WEB_APPLICATION_TYPE
          value: "none"
      restartPolicy: Never
```

---

## 6. Best Practices

1. **Strictly Separate Build and Release**: Never inject runtime secrets into container build images (`Dockerfile`). Use runtime environment variables injected by Kubernetes Secrets or HashiCorp Vault.
2. **Treat Backing Services as URLs**: Always specify backing datastores via composite URLs (`DATABASE_URL=postgresql://user:pass@host:5432/dbname`) rather than hardcoded connection parameters.
3. **Stream All Logs to STDOUT**: Never use file-based log appenders (`RollingFileAppender`). Allow the container runtime (CRI-O, containerd) to capture standard out.
4. **Enforce Port Binding**: Use embedded Tomcat, Jetty, or Netty. Never deploy Spring Boot WAR archives into external application server clusters.
5. **Fast Startup Times**: Utilize Spring Boot 3.x AOT (Ahead-of-Time) compilation and CDS (Class Data Sharing) to keep container initialization under 2 seconds.
6. **Graceful Disposability**: Configure `server.shutdown=graceful` and handle SIGTERM signals to prevent connection reset errors (`502 Bad Gateway`) during rolling deployments.
7. **Stateless Processes**: Do not store any session state in the JVM heap. Rely on distributed stores (Redis, Memcached) or signed cryptographic tokens (JWT).

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| **Hardcoding file paths (`/data/uploads`)** | Containers have ephemeral root filesystems; scaling pods creates split-brain file storage. | Use object storage services (S3, GCS, MinIO) as attached backing services. |
| **Storing user state in `HttpSession`** | When pods scale or restart, subsequent requests land on different pods resulting in logged-out users. | Migrate to Spring Session backed by Redis, or adopt stateless JWTs. |
| **Using `RollingFileAppender`** | Local disk fills up; log collectors like FluentBit fail to track rotated files properly across pod evictions. | Configure Logback with `ConsoleAppender` and structured JSON encoder. |
| **Embedding secrets in `application.yaml`** | Committing database passwords to Git repositories exposes credentials to unauthorized developers. | Reference environment variables (`${SPRING_DATASOURCE_PASSWORD}`) injected via Kubernetes Secrets. |
| **Ignoring SIGTERM signals** | Pod termination forcefully aborts active HTTP transactions, throwing `ECONNRESET` to upstream callers. | Set `server.shutdown=graceful` and configure `preStop` hook sleep in K8s. |
| **Relying on JVM thread affinity or local timers** | Scheduled tasks (`@Scheduled`) run simultaneously on every replica pod, triggering duplicate work. | Use distributed job schedulers like ShedLock, Quartz with JDBC, or Kubernetes CronJobs. |

---

## 8. Performance & Security Notes

### Performance
- **Virtual Threads Integration**: Enable `spring.threads.virtual.enabled=true` (Java 21+) to achieve massive concurrency without dedicating physical OS threads per connection.
- **Connection Pool Tuning**: Configure HikariCP maximum pool size in proportion to pod replica counts. If you run 20 pods with 50 connections each, your database will experience 1,000 active connections.

### Security
- **Least Privilege Execution**: Run Docker containers with non-root security context (`USER 10001:10001`).
- **Read-Only Root Filesystems**: Mount the container root as read-only (`readOnlyRootFilesystem: true`) and provide explicit `emptyDir` mounts only for `/tmp`.
- **Dynamic Secret Rotation**: Use Spring Cloud Vault or AWS Secrets Manager to automatically refresh database credentials without rebuilding images.

---

## 9. Comparison / When to Use What

| Architectural Dimension | Traditional Java EE / Monolith | Twelve-Factor Cloud Native (Spring Boot) |
|---|---|---|
| **Packaging** | WAR / EAR deployed to external server | Executable Fat JAR / OCI Container Image |
| **Session State** | In-Memory `HttpSession` with sticky sessions | Stateless (JWT) or Distributed Redis Session |
| **Configuration** | JNDI / XML files inside deployment archive | Environment variables & Kubernetes ConfigMaps |
| **Scaling Model** | Vertical (Scale Up RAM/CPU of single VM) | Horizontal (Scale Out replicas via HPA) |
| **Disposability** | Slow (Minutes to boot application server) | Fast (Milliseconds with Native Image, 1-3s with JVM) |
| **Logging** | Monolithic local log files on disk | Event streams emitted to `stdout` in JSON format |
| **Admin Tasks** | Web admin console (JMX / WebLogic console) | Ephemeral one-off container jobs (`batch/v1 Job`) |

---

## 10. Interview Questions & Deep Dives

<details>
<summary><strong>1. How does Spring Boot implement Factor III (Config) and what is the exact precedence order of configuration sources?</strong></summary>

Spring Boot achieves Factor III through its `StandardEnvironment` and hierarchical `PropertySource` chain. The resolution order is:
1. Devtools active properties (when devtools is loaded)
2. `@TestPropertySource` in tests
3. Command-line arguments (`--server.port=9090`)
4. Properties from `SPRING_APPLICATION_JSON`
5. ServletConfig and ServletContext init parameters
6. JNDI attributes
7. Java System properties (`System.getProperties()`)
8. OS Environment variables (`SERVER_PORT=9090`)
9. Profile-specific configuration files outside JAR (`config/application-{profile}.yaml`)
10. Profile-specific configuration files inside JAR (`application-{profile}.yaml`)
11. Default application configuration (`application.yaml`)
12. `@PropertySource` annotations on `@Configuration` classes
13. Default properties set via `SpringApplication.setDefaultProperties`

This strict order allows environment variables to reliably override baked-in application defaults.
</details>

<details>
<summary><strong>2. Why is Factor VI (Stateless processes) critical when autoscaling with Kubernetes HPA?</strong></summary>

Kubernetes Horizontal Pod Autoscaler (HPA) dynamically adds and removes pod replicas based on CPU, memory, or custom metrics. When an application stores in-memory state (such as user cart items or upload buffers):
- Pod scale-down kills active user sessions without warning.
- Requests routed across replicas fail because state exists only on node A, not node B (split-brain).
- Sticky-session routing defeats cluster load balancing and creates hot-spots.
By externalizing all state to Redis or PostgreSQL, any pod can process any request interchangeably.
</details>

<details>
<summary><strong>3. Explain the relationship between Factor IX (Disposability) and Spring Boot's graceful shutdown mechanism.</strong></summary>

Factor IX requires that processes can start or stop at a moment's notice with minimal side effects.
When Kubernetes scales down or deploys a new version, it sends a `SIGTERM` signal to the pod.
If `server.shutdown=graceful` is configured:
1. Spring Boot's embedded Tomcat stops accepting new connections on port 8080.
2. In-flight requests are allowed to complete up to `spring.lifecycle.timeout-per-shutdown-phase` (default 30 seconds).
3. Background workers (`SmartLifecycle` components) invoke `stop()`.
4. Spring context publishes `ContextClosedEvent`, closing HikariCP database connections and Kafka consumers cleanly.
5. JVM exits with code 0 before Kubernetes issues `SIGKILL`.
</details>

<details>
<summary><strong>4. How should Factor XI (Logs) be implemented in production Spring Boot microservices?</strong></summary>

Factor XI dictates that an application should never concern itself with routing or storing its output stream.
In Spring Boot:
- Remove all file-based appenders (`FileAppender`, `RollingFileAppender`).
- Configure Logback to use `ConsoleAppender` writing directly to `System.out`.
- Employ structured JSON encoding (e.g., Logstash Logback Encoder or Logback 1.5+ JSON layout) containing standard metadata: `timestamp`, `level`, `traceId`, `spanId`, `thread`, `logger`, and `message`.
- Allow the container infrastructure (Kubernetes log driver -> DaemonSet Vector/FluentBit -> Elasticsearch/OpenSearch) to collect, buffer, and index the logs centrally.
</details>

<details>
<summary><strong>5. What is the difference between Factor IV (Backing Services) and third-party API dependencies?</strong></summary>

A backing service is any service the app consumes over the network as part of its normal operation (databases, message queues, SMTP servers, caching systems). Factor IV specifies that the application should make no distinction between local and third-party services.
For example, whether PostgreSQL is running locally in Docker or as an AWS RDS managed cluster, the application attaches via the same `spring.datasource.url` abstraction without code modifications.
</details>

<details>
<summary><strong>6. How does Spring Boot 3.x Native compilation with GraalVM enhance Factor IX (Disposability)?</strong></summary>

Traditional JVM Spring Boot applications require 2 to 10 seconds to start due to bytecode verification, classloading, and dynamic reflection. Under GraalVM Native Image:
- Ahead-of-Time (AOT) processing generates static native machine code.
- Startup latency drops to 30–80 milliseconds with immediate peak performance (no JIT warmup).
- Memory footprint drops from 300MB+ to under 40MB.
This instant startup allows Kubernetes to handle unexpected traffic spikes instantly and scale down to zero when idle (Knative).
</details>

<details>
<summary><strong>7. How can you implement Factor XII (Admin processes) cleanly in a Spring Boot ecosystem?</strong></summary>

Factor XII mandates that one-off admin tasks (database migrations, data fixes, re-indexing) run in an identical environment to regular long-running processes.
Implementation strategies:
- **Flyway/Liquibase**: Integrated directly into deployment pipelines or run as a standalone Kubernetes Job container using the same base image with a custom command line.
- **Spring Batch / CommandLineRunner**: Implement tasks as isolated beans gated by specific profiles (`@Profile("migration-task")`), running via `java -jar app.jar --spring.main.web-application-type=none`.
</details>

<details>
<summary><strong>8. What is "Dev/Prod Parity" (Factor X) and how do Testcontainers support it in Spring Boot?</strong></summary>

Factor X aims to keep development, staging, and production environments as identical as possible across time, code, and backing services.
In the past, developers used in-memory H2 databases for testing and PostgreSQL for production, leading to dialect bugs and false-positive test suites.
Spring Boot 3.1+ integrates **Testcontainers**:
- Automated spin-up of identical production Docker images (PostgreSQL, Kafka, Redis) during unit and integration tests.
- Uses `@ServiceConnection` to inject container connection properties directly into the Spring Environment, eliminating configuration differences.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Refactor Hardcoded Properties to Twelve-Factor Environment Variables</strong></summary>

**Objective**: Convert an existing `application.properties` file with hardcoded credentials into an environment-driven configuration supporting Docker containers.

**Given**:
```properties
spring.datasource.url=jdbc:postgresql://localhost:5432/orders_db
spring.datasource.username=admin
spring.datasource.password=secret123
app.payment.gateway-url=https://sandbox.stripe.com/v1
```

**Task**: Refactor using Spring property placeholders with sensible defaults and create the equivalent Docker environment variable command.

**Solution**:
```properties
spring.datasource.url=${SPRING_DATASOURCE_URL:jdbc:postgresql://localhost:5432/orders_db}
spring.datasource.username=${SPRING_DATASOURCE_USERNAME:admin}
spring.datasource.password=${SPRING_DATASOURCE_PASSWORD:secret123}
app.payment.gateway-url=${APP_PAYMENT_GATEWAY_URL:https://sandbox.stripe.com/v1}
```
Run with Docker:
```bash
docker run -e SPRING_DATASOURCE_URL="jdbc:postgresql://rds.prod.internal:5432/orders_prod" \
           -e SPRING_DATASOURCE_USERNAME="prod_user" \
           -e SPRING_DATASOURCE_PASSWORD="super-secret-vault-pwd" \
           -p 8080:8080 myapp:1.0.0
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Configure Standard JSON Logging to STDOUT</strong></summary>

**Objective**: Configure Logback to emit structured JSON logs to standard output, adhering to Factor XI.

**Solution**:
Add `logback-spring.xml` in `src/main/resources`:
```xml
<?xml version="1.0" encoding="UTF-8"?>
<configuration>
    <appender name="CONSOLE_JSON" class="ch.qos.logback.core.ConsoleAppender">
        <encoder class="ch.qos.logback.classic.encoder.JsonEncoder"/>
    </appender>

    <root level="INFO">
        <appender-ref ref="CONSOLE_JSON" />
    </root>
</configuration>
```
All log events will stream directly to stdout without intermediate file locks.
</details>

<details>
<summary><strong>Exercise 3 (Medium): Implement Distributed ShedLock for Factor VI Concurrency</strong></summary>

**Objective**: Prevent duplicate execution of a `@Scheduled` task when running across multiple container replicas.

**Solution**:
1. Add dependencies: `shedlock-spring` and `shedlock-provider-jdbc-template`.
2. Configure lock provider:
```java
@Configuration
@EnableScheduling
@EnableSchedulerLock(defaultLockAtMostFor = "10m")
public class SchedulerConfig {
    @Bean
    public LockProvider lockProvider(DataSource dataSource) {
        return new JdbcTemplateLockProvider(
            JdbcTemplateLockProvider.Configuration.builder()
                .withJdbcTemplate(new JdbcTemplate(dataSource))
                .usingDbTime()
                .build()
        );
    }
}
```
3. Annotate the task:
```java
@Component
public class ReportGenerator {

    @Scheduled(cron = "0 0 * * * *")
    @SchedulerLock(name = "ReportGenerator_generateHourlySummary", 
                   lockAtLeastFor = "15s", lockAtMostFor = "5m")
    public void generateHourlySummary() {
        // Runs on exactly ONE pod in the cluster per hour
    }
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Implement Graceful Kubernetes SIGTERM Handling with Delay</strong></summary>

**Objective**: Configure Spring Boot and Kubernetes to allow ingress load balancers to deregister before Spring Boot stops processing requests.

**Solution**:
In `application.yaml`:
```yaml
server:
  shutdown: graceful
spring:
  lifecycle:
    timeout-per-shutdown-phase: 25s
```

In Kubernetes Pod spec:
```yaml
lifecycle:
  preStop:
    exec:
      # Sleep 10s to give Kubernetes endpoints controller time to remove pod from kube-proxy/ingress
      command: ["/bin/sh", "-c", "sleep 10"]
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Implement Factor XII Database Migration as a Pre-Install Job</strong></summary>

**Objective**: Write a Kubernetes Helm/Job manifest that runs Flyway database migrations as a one-off task using the identical application container image before the web pods spin up.

**Solution**:
```yaml
apiVersion: batch/v1
kind: Job
metadata:
  name: order-service-migration
  annotations:
    "helm.sh/hook": pre-install,pre-upgrade
    "helm.sh/hook-weight": "1"
    "helm.sh/hook-delete-policy": before-hook-creation,hook-succeeded
spec:
  template:
    spec:
      restartPolicy: OnFailure
      containers:
      - name: db-migration
        image: ghcr.io/myorg/order-service:2.4.0
        command: ["java"]
        args:
          - "-Dspring.main.web-application-type=none"
          - "-Dspring.flyway.enabled=true"
          - "-jar"
          - "/app/app.jar"
        envFrom:
        - secretRef:
            name: db-credentials
        - configMapRef:
            name: common-env
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Build a Self-Healing Health Indicator with Factor IV Resilience</strong></summary>

**Objective**: Create a custom Spring Boot `HealthIndicator` that checks backing service connectivity and reports degradation without crashing the pod process.

**Solution**:
```java
@Component
public class BackingDatabaseHealthIndicator implements HealthIndicator {

    private final DataSource dataSource;

    public BackingDatabaseHealthIndicator(DataSource dataSource) {
        this.dataSource = dataSource;
    }

    @Override
    public Health health() {
        try (Connection conn = dataSource.getConnection()) {
            if (conn.isValid(1)) {
                return Health.up()
                    .withDetail("database", "PostgreSQL Reachable")
                    .build();
            }
            return Health.down().withDetail("database", "Validation ping failed").build();
        } catch (Exception ex) {
            return Health.down(ex)
                .withDetail("error", "Cannot establish backing connection")
                .build();
        }
    }
}
```
</details>

---

## 12. Summary & Cheat Sheet

```
+------------------------------------------------------------------------------------------------+
| TWELVE-FACTOR SPRING BOOT CHEAT SHEET                                                          |
+------------------------------------------------------------------------------------------------+
| Factor              | Spring Boot Feature / Configuration                                      |
+---------------------+--------------------------------------------------------------------------+
| I. Codebase         | Single Git repo per service; multi-branch deploys                        |
| II. Dependencies    | Maven pom.xml / Gradle build.gradle.kts; no ambient system jars          |
| III. Config         | @ConfigurationProperties, StandardEnvironment, K8s ConfigMaps/Secrets    |
| IV. Backing Svcs    | URLs injected as env vars (SPRING_DATASOURCE_URL, SPRING_KAFKA_BOOTSTRAP) |
| V. Build/Release    | Spring Boot Buildpacks (`bootBuildImage`) -> Immutable OCI image tag     |
| VI. Processes       | Stateless JVM; Spring Session + Redis; Token auth (JWT)                  |
| VII. Port Binding   | Embedded Tomcat/Netty (`server.port=8080`)                               |
| VIII. Concurrency   | Horizontal scaling via K8s HPA; Virtual Threads (`threads.virtual=true`)  |
| IX. Disposability   | Graceful shutdown (`server.shutdown=graceful`); fast boot via AOT/CDS    |
| X. Dev/Prod Parity  | Testcontainers with Docker backing stores in local test suites           |
| XI. Logs            | ConsoleAppender with Logstash JSON streaming directly to STDOUT           |
| XII. Admin Tasks    | CommandLineRunner with `web-application-type=none` run as K8s Jobs      |
+------------------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [The Twelve-Factor App Methodology (Original Specification)](https://12factor.net/)
- [Spring Boot Production-Ready Features Documentation](https://docs.spring.io/spring-boot/docs/current/reference/html/actuator.html)
- [Cloud Native Java: Designing Resilient Systems with Spring Boot, Spring Cloud, and Cloud Foundry](https://www.oreilly.com/library/view/cloud-native-java/9781449374631/)
- [Kubernetes Best Practices for Spring Boot Microservices](https://spring.io/guides/topicals/spring-boot-kubernetes/)
- [Testcontainers: Modern Integration Testing for Twelve-Factor Systems](https://testcontainers.com/)
