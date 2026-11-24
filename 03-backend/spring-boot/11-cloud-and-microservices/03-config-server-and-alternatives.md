# Centralized Configuration: Spring Cloud Config Server and Modern Alternatives

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x |
| **Java Version** | 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | `spring-cloud-config-server:4.2.x`, `spring-cloud-starter-config` |

---

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Usage](#4-syntax--usage)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices](#6-best-practices)
- [7. Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
- [8. Performance & Security Notes](#8-performance--security-notes)
- [9. Comparison / When to Use What](#9-comparison--when-to-use-what)
- [10. Interview Questions](#10-interview-questions)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading](#13-further-reading)

---

## 1. What & Why

### The Problem
Managing configuration properties across dozens of microservices deployed across multiple environments (Development, Staging, Production) introduces significant operational hazards. Packaging `application.properties` directly inside Docker containers forces developers to rebuild and redeploy container images just to change a database connection pool size, toggle a feature flag, or update a timeout threshold. Storing production passwords or API keys in Git repositories violates security compliance, while updating configuration across 50 running pod instances manually is prone to human error and configuration drift.

### The Evolution
The Twelve-Factor App methodology mandates strict separation of configuration from code. **Spring Cloud Config Server** provides an HTTP resource-based API for externalized configuration backed by version-controlled Git repositories, HashiCorp Vault, or JDBC databases. Clients load their environment-specific configuration dynamically during bootstrap and can refresh properties at runtime via `@RefreshScope` without restarting the JVM.

### Relevance in Modern Engineering
While Spring Cloud Config Server remains popular in VM and traditional multi-cloud setups, modern container orchestrators like Kubernetes offer native primitives: **ConfigMaps** and **Secrets**. Spring Boot 3 natively bridges both approaches, supporting `spring.config.import` for Config Server alongside Spring Cloud Kubernetes for native ConfigMap mounting.

---

## 2. Core Concepts

- **Config Server**: A centralized Spring Boot application exposing REST endpoints (`/{application}/{profile}[/{label}]`) that serve property configurations stored in Git, SVN, Vault, or database backends.
- **Config Client**: A microservice that contacts the Config Server at startup to load its configuration properties into Spring's `Environment`.
- **`spring.config.import`**: The modern Spring Boot 3 configuration property replacing legacy `bootstrap.yml` for importing external configuration sources (e.g., `spring.config.import=optional:configserver:http://localhost:8888`).
- **`@RefreshScope`**: Spring Cloud annotation placed on beans allowing them to be reloaded and re-injected dynamically when an Actuator `/actuator/refresh` POST request is issued.
- **Spring Cloud Bus**: Broadcasts configuration refresh events across all microservice instances via a message broker (RabbitMQ/Kafka) so a single webhook updates the entire cluster.
- **HashiCorp Vault Integration**: Securely injecting encrypted secrets and database credentials directly into microservice memory without writing secrets to Git.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------+
|                        Spring Cloud Config Server Flow                            |
|                                                                                   |
| 1. Git Repository (Single Source of Truth)                                        |
|    - order-service.yml (Common configs)                                          |
|    - order-service-prod.yml (Production overrides)                                |
|                   |                                                               |
|                   v Clones / Fetches via JGit                                     |
| 2. Spring Cloud Config Server (Port 8888)                                         |
|    Exposes: GET /order-service/prod                                               |
|                   |                                                               |
|                   v HTTP JSON Response with PropertySources                       |
| 3. Microservice Client Bootstrap (Spring Boot 3)                                  |
|    application.yml: spring.config.import=configserver:http://config-server:8888   |
|                   |                                                               |
|                   v                                                               |
| 4. Environment Assembly                                                           |
|    ConfigServerPropertySourceLocator merges remote properties into Environment    |
|                   |                                                               |
|                   v                                                               |
| 5. Runtime Dynamic Refresh                                                        |
|    Git commit pushed -> Webhook triggers POST /actuator/busrefresh                |
|    Spring Cloud Bus emits RefreshRemoteApplicationEvent over Kafka/RabbitMQ       |
|    @RefreshScope beans reload with updated values with ZERO JVM RESTART!          |
+-----------------------------------------------------------------------------------+
```

During application startup, Spring Boot 3's `ConfigData` subsystem parses `spring.config.import`. If configured with `configserver:`, it issues an HTTP request to the Config Server passing its `spring.application.name` and active profiles (`spring.profiles.active`). The Config Server pulls the latest Git commit, merges hierarchical property files (`application.yml` $\to$ `order-service.yml` $\to$ `order-service-prod.yml`), and returns a sanitized JSON payload. The client injects these properties into its `Environment`. When properties change in Git, a POST request to `/actuator/refresh` clears the cache of all `@RefreshScope` beans, re-instantiating them with updated values on the next method call.

---

## 4. Syntax & Usage

### Maven Dependency (Config Server)
```xml
<dependency>
    <groupId>org.springframework.cloud</groupId>
    <artifactId>spring-cloud-config-server</artifactId>
</dependency>
```

### Config Server Application (`@EnableConfigServer`)
```java
package com.example.platform.configserver;

import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;
import org.springframework.cloud.config.server.EnableConfigServer;

@SpringBootApplication
@EnableConfigServer
public class ConfigServerApplication {
    public static void main(String[] args) {
        SpringApplication.run(ConfigServerApplication.class, args);
    }
}
```

### Config Server Configuration (`application.yml`)
```yaml
server:
  port: 8888
spring:
  application:
    name: config-server
  cloud:
    config:
      server:
        git:
          uri: https://github.com/example-org/platform-config-repo
          default-label: main
          search-paths: '{application}'
          clone-on-start: true
```

---

## 5. Practical Examples

### Example 1: Client Application Configuration via `spring.config.import`
```yaml
# src/main/resources/application.yml (Client Microservice)
spring:
  application:
    name: order-service
  profiles:
    active: prod
  config:
    # Optional prefix prevents startup failure if Config Server is temporarily unreachable
    import: "optional:configserver:http://localhost:8888"

management:
  endpoints:
    web:
      exposure:
        include: refresh, health, info
```

### Example 2: Dynamic Property Refresh with `@RefreshScope`
```java
package com.example.platform.order.service;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.cloud.context.config.annotation.RefreshScope;
import org.springframework.stereotype.Service;

@Service
@RefreshScope // Re-instantiated dynamically when /actuator/refresh is triggered
public class DynamicDiscountService {

    private static final Logger log = LoggerFactory.getLogger(DynamicDiscountService.class);

    @Value("${order.discount.percentage:0.0}")
    private double discountPercentage;

    @Value("${order.feature.flash-sale.enabled:false}")
    private boolean flashSaleEnabled;

    public double calculateDiscount(double orderTotal) {
        log.info("Calculating discount. Rate: {}%, FlashSale: {}", discountPercentage, flashSaleEnabled);
        if (!flashSaleEnabled) {
            return 0.0;
        }
        return orderTotal * (discountPercentage / 100.0);
    }
}
```

### Example 3: Legacy `bootstrap.yml` Anti-Pattern and Fix
```yaml
# ANTI-PATTERN: Using bootstrap.yml in Spring Boot 3!
# Legacy Spring Boot 2.x relied on spring-cloud-starter-bootstrap to initialize configs.
# In Spring Boot 3, bootstrap context is DISABLED by default and causes configuration confusion!

# FIX: Delete bootstrap.yml and configure spring.config.import in application.yml
# application.yml:
spring:
  config:
    import: "configserver:http://config-server:8888"
```

### Example 4: Secure Secrets Integration with HashiCorp Vault
```yaml
# Config Server configuration combining Git for non-sensitive properties and Vault for secrets
spring:
  cloud:
    config:
      server:
        composite:
          - type: git
            uri: https://github.com/example-org/config-repo
          - type: vault
            host: 127.0.0.1
            port: 8200
            scheme: https
            kv-version: 2
```

### Example 5: High-Performance Health Indicator for Config Server
```java
package com.example.platform.configserver.health;

import org.springframework.boot.actuate.health.Health;
import org.springframework.boot.actuate.health.HealthIndicator;
import org.springframework.cloud.config.server.environment.EnvironmentRepository;
import org.springframework.stereotype.Component;

@Component
public class ConfigGitHealthIndicator implements HealthIndicator {

    private final EnvironmentRepository environmentRepository;

    public ConfigGitHealthIndicator(EnvironmentRepository environmentRepository) {
        this.environmentRepository = environmentRepository;
    }

    @Override
    public Health health() {
        try {
            // Test that the backing Git repository can be reached and read
            var env = environmentRepository.findOne("healthcheck", "default", "main");
            return Health.up().withDetail("gitSource", env.getProfiles()).build();
        } catch (Exception e) {
            return Health.down().withDetail("error", e.getMessage()).build();
        }
    }
}
```

---

## 6. Best Practices

1. **Use `spring.config.import` Instead of `bootstrap.yml`**: Spring Boot 3 standardized configuration import through `spring.config.import`; remove deprecated `spring-cloud-starter-bootstrap`.
2. **Use the `optional:` Prefix Wisely**: Use `optional:configserver:` in local development so the microservice starts even if the config server is offline; omit `optional:` in production so the service fails fast if its authoritative configuration is missing.
3. **Never Store Plaintext Secrets in Git Repositories**: Encrypt sensitive properties using symmetric keys (`encrypt.key`) or delegate secrets management entirely to HashiCorp Vault or AWS Secrets Manager.
4. **Use `@RefreshScope` Sparingly**: Placing `@RefreshScope` on beans that hold heavy stateful connections (like database `DataSource` or thread pools) can cause resource leaks or broken connection pools during refresh.
5. **Adopt Kubernetes Native Config on Kubernetes**: If deployed on Kubernetes, evaluate mounting `ConfigMaps` and `Secrets` directly as volumes or environment variables rather than deploying a dedicated Config Server.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Retaining `bootstrap.yml` without bootstrap starter | Ignored silently in Spring Boot 3; properties are not loaded | Migrate all imports to `application.yml` via `spring.config.import`. |
| Committing unencrypted database passwords to Git | Credentials exposed in version control history | Use Spring Cloud Config encryption (`{cipher}...`) or HashiCorp Vault. |
| Placing `@RefreshScope` on `DataSource` bean | Refreshing the scope destroys the connection pool, breaking active transactions | Only place `@RefreshScope` on lightweight configuration or service beans. |
| Omitting label/branch name | Config Server defaults to `master`, failing on repositories using `main` | Set `spring.cloud.config.server.git.default-label=main`. |
| Missing Actuator `/actuator/refresh` endpoint exposure | Webhook cannot trigger dynamic property reload | Set `management.endpoints.web.exposure.include=refresh`. |
| Deploying a single Config Server instance in production | Becomes a single point of failure (SPOF) for all microservice startups | Run multiple Config Server instances behind an internal load balancer. |

---

## 8. Performance & Security Notes

- **Git Polling & Local Caching**: Set `clone-on-start=true` on the Config Server so it clones the repository to local disk during startup, eliminating Git network latency on subsequent client requests.
- **Actuator Security**: Secure the `/actuator/refresh` and `/actuator/busrefresh` endpoints behind Spring Security role-based access control (RBAC) to prevent unauthorized configuration reloading.
- **Fail-Fast Mode**: In production, configure `spring.cloud.config.fail-fast=true` so microservices abort immediately if the Config Server is unreachable, preventing services from operating on default fallback values.

---

## 9. Comparison / When to Use What

| Dimension | Spring Cloud Config Server | Kubernetes ConfigMaps & Secrets | HashiCorp Vault |
|---|---|---|---|
| **Storage Backend** | Git Repository / Vault | Etcd in Kubernetes cluster | Encrypted Storage Engine |
| **Refresh Mechanism** | `@RefreshScope` via Actuator | Volume mount auto-update or pod restart | Leased dynamic credentials |
| **Platform Lock-In** | None (Runs anywhere) | Locked to Kubernetes | None (Universal API) |
| **Secret Management** | Requires encryption configuration | Base64 encoded (Plaintext in etcd) | Enterprise-grade KMS & rotation |
| **Optimal Use Case** | VM deployments, multi-cloud | Kubernetes-native workloads | High-security financial secrets |

---

## 10. Interview Questions

<details>
<summary>1. How does Spring Boot 3 handle external configuration loading compared to Spring Boot 2?</summary>
Spring Boot 2 relied on a separate "bootstrap" execution phase driven by `bootstrap.yml` and `spring-cloud-starter-bootstrap`. Spring Boot 3 deprecated this phase and unified configuration imports into `application.yml` using `spring.config.import=configserver:http://...`, eliminating the dual-context bootstrap complexity.
</details>

<details>
<summary>2. How does the `@RefreshScope` annotation work internally?</summary>
`@RefreshScope` creates a proxy around the bean. Behind the proxy sits a `GenericScope` cache storing the real bean instance. When an Actuator `/actuator/refresh` request is received, the `ContextRefresher` updates the `Environment` and clears the `GenericScope` cache. The next time any method on the bean is invoked, Spring detects the missing instance in cache and creates a fresh instance injected with the updated environment properties.
</details>

<details>
<summary>3. Why is placing `@RefreshScope` on a DataSource bean dangerous?</summary>
A `DataSource` manages long-lived physical TCP connections, transactions, and connection pools (HikariCP). Re-instantiating the `DataSource` bean mid-execution can terminate in-flight database transactions, orphan pooled connections, and cause severe connection pool leaks.
</details>

<details>
<summary>4. What is Spring Cloud Bus and how does it relate to configuration refresh?</summary>
Without Spring Cloud Bus, updating configuration requires issuing an HTTP POST to `/actuator/refresh` on every single running microservice instance individually. Spring Cloud Bus links all service instances together via a message broker (Kafka or RabbitMQ). When a single `/actuator/busrefresh` request is sent, the event is broadcast across the bus, refreshing all cluster nodes simultaneously.
</details>

<details>
<summary>5. What does the `optional:` prefix in `spring.config.import` do?</summary>
The `optional:` prefix specifies that if the imported configuration source (e.g., the Config Server) is unreachable or fails to return configuration, the application should continue booting rather than throwing a fatal startup exception.
</details>

<details>
<summary>6. How do you encrypt sensitive properties in Spring Cloud Config Server?</summary>
By installing Java Cryptography Extension (JCE) and configuring a symmetric encryption key (`encrypt.key`) on the Config Server. Secrets can be encrypted via the `/encrypt` endpoint and stored in Git prefixed with `{cipher}` (e.g., `password: '{cipher}AQA78...fg=='`). The Config Server decrypts the value on-the-fly before sending it to authorized clients.
</details>

<details>
<summary>7. What are the advantages of Kubernetes ConfigMaps over Spring Cloud Config Server?</summary>
Kubernetes ConfigMaps are native to the container orchestrator. They require no additional JVMs to be maintained, eliminate the network dependency on a central Config Server at startup, integrate with standard CI/CD deployment manifests (Helm/Kustomize), and can be mounted directly as environment variables or files.
</details>

<details>
<summary>8. How does Spring Cloud Config Server resolve property hierarchy?</summary>
Properties are resolved from least specific to most specific. The server evaluates:
1. `application.yml` (shared across all apps)
2. `application-{profile}.yml` (shared for profile)
3. `{application}.yml` (app specific)
4. `{application}-{profile}.yml` (app and profile specific).
Properties defined in more specific files override properties from generic files.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1 (Easy): Configure @EnableConfigServer</summary>
Write a Spring Boot main class annotated with `@EnableConfigServer`.
</details>

<details>
<summary>Exercise 2 (Easy): Configure client spring.config.import</summary>
Write an `application.yml` importing configuration from `http://localhost:8888` for application `"payment-service"`.
</details>

<details>
<summary>Exercise 3 (Medium): Implement a @RefreshScope service</summary>
Create a service annotated with `@RefreshScope` injecting a dynamic tax rate property and exposing a method returning the calculated tax.
</details>

<details>
<summary>Exercise 4 (Medium): Configure multiple Git search paths</summary>
Configure `application.yml` on the Config Server with Git search paths matching `{application}` subdirectories.
</details>

<details>
<summary>Exercise 5 (Hard): Composite environment repository with Git and Vault</summary>
Configure the Config Server with a composite repository pulling non-sensitive properties from a public Git repo and database passwords from HashiCorp Vault.
</details>

<details>
<summary>Exercise 6 (Hard): Integration test testing dynamic refresh</summary>
Write an integration test using `TestPropertyValues` and `ContextRefresher` that mutates an environment property, refreshes the context, and asserts that the `@RefreshScope` bean returns the new value.
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------+
|                       Centralized Config Cheat Sheet                            |
+---------------------------------------------------------------------------------+
| Server Annotation:  @EnableConfigServer                                         |
| Client Config:      spring.config.import=configserver:http://localhost:8888     |
| Optional Flag:      spring.config.import=optional:configserver:...              |
| Deprecated Trap:    bootstrap.yml is retired in Boot 3; use application.yml     |
| Dynamic Refresh:    @RefreshScope on bean + POST /actuator/refresh              |
| Cluster Refresh:    POST /actuator/busrefresh (Spring Cloud Bus)                |
| Security:           Encrypt with {cipher} prefix or use HashiCorp Vault         |
| Kubernetes Rule:    Prefer K8s ConfigMaps/Secrets for native container clusters |
+---------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Official Spring Cloud Config Reference](https://docs.spring.io/spring-cloud-config/reference/)
- [Spring Boot 3 Config Data Import Guide](https://spring.io/blog/2020/08/14/config-file-processing-in-spring-boot-2-4)
- [The Twelve-Factor App: Config](https://12factor.net/config)
