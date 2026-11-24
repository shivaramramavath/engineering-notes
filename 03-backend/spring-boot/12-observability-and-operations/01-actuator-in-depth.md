# Spring Boot Actuator in Depth

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x |
| **Spring Framework Version** | 6.2.x |
| **Java Version** | 21 LTS |
| **Module** | 12-observability-and-operations |
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
In monolithic applications deployed on physical servers, administrators relied on external shell scripts, OS monitoring agents, and periodic log scraping to assess application health. In modern cloud-native, microservice architectures running hundreds of container replicas across Kubernetes clusters, applications must provide **first-class runtime introspection**. Operations teams need real-time answers to critical operational questions:
- Is the JVM alive and able to accept traffic?
- Are database connection pools saturated?
- Which Spring beans failed initialization or took excessive time to load?
- What environment variables and configuration properties are active in production?

### Historical Evolution
Before Spring Boot, operational endpoints were cobbled together using ad-hoc JMX MBeans or custom HTTP servlets that exposed internal state, often lacking standardized formatting, security filtering, or consistent serialization.

Spring Boot 1.x introduced `spring-boot-starter-actuator` with custom HTTP endpoints. However, in Boot 1.x endpoints were tightly coupled to Spring MVC and lacked protocol independence. 

Spring Boot 2.0 refactored Actuator completely around a reactive, protocol-agnostic endpoint model supporting Spring MVC, Spring WebFlux, and JMX simultaneously. 

Spring Boot 3.x modernizes this foundation for Jakarta EE 10, Java 21 Virtual Threads, GraalVM AOT compilation, and native Kubernetes probe integration.

### Production Relevance
Actuator transforms opaque black-box containers into transparent, observable microservices. It powers:
- Kubernetes Liveness and Readiness probes (`/actuator/health/liveness`, `/actuator/health/readiness`).
- Observability backends (Prometheus, Datadog, Dynatrace) via Micrometer endpoints (`/actuator/prometheus`, `/actuator/metrics`).
- Dynamic runtime operations: log level adjustments without restarts (`/actuator/loggers`), graceful thread dumps (`/actuator/threaddump`), and environment auditing (`/actuator/env`).

---

## 2. Core Concepts

### Architectural Building Blocks
1. **Endpoint (`@Endpoint`)**: A protocol-independent operational component identified by a unique ID (e.g., `health`, `metrics`, `beans`).
2. **Operations**:
   - `@ReadOperation`: HTTP `GET` / JMX attribute read.
   - `@WriteOperation`: HTTP `POST` / JMX attribute write / operation invoke.
   - `@DeleteOperation`: HTTP `DELETE` / JMX invocation.
3. **Web-Specific Endpoints (`@WebEndpoint`)**: Endpoints exposed exclusively over HTTP/REST (bypassing JMX).
4. **JMX-Specific Endpoints (`@JmxEndpoint`)**: Endpoints exposed exclusively via JMX MBeans.
5. **Endpoint Exposure**: Controlling which endpoints are available via configuration properties:
   - `management.endpoints.web.exposure.include`
   - `management.endpoints.jmx.exposure.include`
6. **Management Server Independence**: The ability to isolate Actuator endpoints on a dedicated HTTP port and network interface via `management.server.port`.

---

## 3. How It Works Internally

### Actuator Invocation & Dispatch Pipeline
Actuator abstracts transport mechanisms using `EndpointDiscoverer` and `OperationInvoker`.

```
+------------------------------------------------------------------------------------+
|                                CLIENT REQUEST                                     |
|               HTTP GET /actuator/custom-status  OR  JMX MBean Invocation           |
+------------------------------------------------------------------------------------+
                                         |
                                         v
+------------------------------------------------------------------------------------+
|                         MANAGEMENT SECURITY FILTER CHAIN                           |
|       (Dedicated SecurityFilterChain bound to management.server.port/base-path)    |
+------------------------------------------------------------------------------------+
                                         |
                                         v
+------------------------------------------------------------------------------------+
|                         ACTUATOR ENDPOINT DISPATCHER                               |
|        WebMvcEndpointHandlerMapping  /  WebFluxEndpointHandlerMapping              |
|   Locates WebOperation / JmxOperation mapped to endpoint ID 'custom-status'        |
+------------------------------------------------------------------------------------+
                                         |
                                         v
+------------------------------------------------------------------------------------+
|                         OPERATION INVOKER PIPELINE                                 |
|  1. Parameter resolution & type conversion (StringToTypeConverter)                 |
|  2. Security evaluation (Role/Authority check)                                     |
|  3. Invocation of @ReadOperation / @WriteOperation method via reflection / AOT     |
+------------------------------------------------------------------------------------+
                                         |
                                         v
+------------------------------------------------------------------------------------+
|                        TARGET COMPONENT EXECUTION                                  |
|         CustomStatusEndpoint.readStatus() -> returns StatusDTO                     |
+------------------------------------------------------------------------------------+
                                         |
                                         v
+------------------------------------------------------------------------------------+
|                        RESPONSE SANITIZATION & SERIALIZATION                       |
|  SanitizingDataRegistry masks sensitive keys (passwords, tokens, API keys)        |
|  Serializes JSON response (HTTP 200 OK / JMX CompositeData)                        |
+------------------------------------------------------------------------------------+
```

### Discovery and Registration
During startup:
1. `EndpointAutoConfiguration` registers endpoint infrastructure beans.
2. `WebEndpointDiscoverer` scans the `ApplicationContext` for classes annotated with `@Endpoint` or `@WebEndpoint`.
3. For each method annotated with `@ReadOperation`, `@WriteOperation`, or `@DeleteOperation`, it builds a `WebOperation` instance.
4. `WebMvcEndpointHandlerMapping` binds these operations to URI templates under `management.endpoints.web.base-path` (default: `/actuator`).

---

## 4. Syntax & Configuration Reference

### Common `application.yml` Properties

```yaml
management:
  server:
    port: 9090 # Isolated management port
    address: 127.0.0.1 # Bind to loopback or internal interface
    base-path: /actuator # Base prefix for all management endpoints
  endpoints:
    enabled-by-default: false # Secure by default: disable all endpoints
    web:
      exposure:
        include: [ "health", "info", "prometheus", "metrics" ] # Expose only required
        exclude: [ "env", "beans" ]
      base-path: /actuator
      cors:
        allowed-origins: "https://ops.internal.domain"
        allowed-methods: [ "GET" ]
    jmx:
      exposure:
        exclude: "*" # Disable JMX exposure in container environments
  endpoint:
    health:
      enabled: true
      show-details: when-authorized
      roles: "ROLE_ACTUATOR_ADMIN"
      probes:
        enabled: true
    info:
      enabled: true
      git:
        mode: full
      env:
        enabled: true
```

---

## 5. Practical Examples

### Example 1: Custom Production Readiness Endpoint with `@Endpoint`
```java
package com.example.observability.actuator;

import org.springframework.boot.actuate.endpoint.annotation.DeleteOperation;
import org.springframework.boot.actuate.endpoint.annotation.Endpoint;
import org.springframework.boot.actuate.endpoint.annotation.ReadOperation;
import org.springframework.boot.actuate.endpoint.annotation.Selector;
import org.springframework.boot.actuate.endpoint.annotation.WriteOperation;
import org.springframework.stereotype.Component;

import java.time.Instant;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

@Component
@Endpoint(id = "circuit-breakers")
public class CircuitBreakerActuatorEndpoint {

    public record BreakerStatus(String state, double failureRatePercent, Instant lastTripTime) {}

    private final Map<String, BreakerStatus> breakers = new ConcurrentHashMap<>();

    public CircuitBreakerActuatorEndpoint() {
        breakers.put("payment-gateway", new BreakerStatus("CLOSED", 0.12, null));
        breakers.put("inventory-service", new BreakerStatus("CLOSED", 0.05, null));
    }

    @ReadOperation
    public Map<String, BreakerStatus> getAllCircuitBreakers() {
        return Map.copyOf(breakers);
    }

    @ReadOperation
    public BreakerStatus getCircuitBreaker(@Selector String name) {
        return breakers.getOrDefault(name, new BreakerStatus("UNKNOWN", 0.0, null));
    }

    @WriteOperation
    public BreakerStatus tripCircuitBreaker(@Selector String name, String forcedState) {
        BreakerStatus updated = new BreakerStatus(forcedState.toUpperCase(), 100.0, Instant.now());
        breakers.put(name, updated);
        return updated;
    }

    @DeleteOperation
    public void resetCircuitBreaker(@Selector String name) {
        breakers.put(name, new BreakerStatus("CLOSED", 0.0, null));
    }
}
```

### Example 2: Dedicated Actuator Security Filter Chain
```java
package com.example.observability.config;

import org.springframework.boot.actuate.autoconfigure.security.servlet.EndpointRequest;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.core.annotation.Order;
import org.springframework.security.config.Customizer;
import org.springframework.security.config.annotation.web.builders.HttpSecurity;
import org.springframework.security.config.annotation.web.configuration.EnableWebSecurity;
import org.springframework.security.web.SecurityFilterChain;

@Configuration(proxyBeanMethods = false)
@EnableWebSecurity
public class ActuatorSecurityConfiguration {

    @Bean
    @Order(1) // Evaluated before business API filter chain
    public SecurityFilterChain actuatorSecurityFilterChain(HttpSecurity http) throws Exception {
        return http
            .securityMatcher(EndpointRequest.toAnyEndpoint())
            .authorizeHttpRequests(auth -> auth
                // Allow public access to Kubernetes liveness and readiness probes
                .requestMatchers(EndpointRequest.to("health")).permitAll()
                .requestMatchers(EndpointRequest.to("info")).permitAll()
                // Restrict Prometheus scraping to metrics scraping role or internal IP
                .requestMatchers(EndpointRequest.to("prometheus")).hasAuthority("SCOPE_metrics:scrape")
                // All other management endpoints require full admin authority
                .anyRequest().hasRole("OPS_ADMIN")
            )
            .httpBasic(Customizer.withDefaults())
            .csrf(csrf -> csrf.disable()) // Actuator endpoints use stateless basic or bearer auth
            .build();
    }
}
```

### Example 3: Sanitizing Sensitive Environment Variables with `SanitizingFunction`
```java
package com.example.observability.actuator;

import org.springframework.boot.actuate.endpoint.SanitizableData;
import org.springframework.boot.actuate.endpoint.SanitizingFunction;
import org.springframework.stereotype.Component;

import java.util.Set;
import java.util.regex.Pattern;

@Component
public class CustomPropertySanitizer implements SanitizingFunction {

    private static final Pattern SENSITIVE_KEY_PATTERN = 
        Pattern.compile("(?i).*(token|secret|credential|private|apikey|password|auth).*");

    private static final Set<String> ALLOWED_EXCEPTIONS = Set.of("auth.provider.url", "auth.mode");

    @Override
    public SanitizableData apply(SanitizableData data) {
        if (data.getKey() == null) {
            return data;
        }

        if (ALLOWED_EXCEPTIONS.contains(data.getKey())) {
            return data;
        }

        if (SENSITIVE_KEY_PATTERN.matcher(data.getKey()).matches()) {
            return data.withValue("****** (REDACTED_BY_SECURITY_POLICY)");
        }

        return data;
    }
}
```

### Example 4: Anti-Pattern vs. Production Fix (Blocking Health Indicator)
```java
// ANTI-PATTERN: Blocking the health indicator thread with a slow, synchronous, un-timeouted HTTP call
// Causes /actuator/health to hang, leading Kubernetes to terminate the container!
@Component
public class BadExternalServiceHealthIndicator implements HealthIndicator {
    private final RestTemplate restTemplate = new RestTemplate();

    @Override
    public Health health() {
        // Anti-pattern: No socket timeout, runs on the main HTTP thread during probe check
        String result = restTemplate.getForObject("https://slow-partner.com/ping", String.class);
        return Health.up().withDetail("partner", result).build();
    }
}

// PRODUCTION FIX: Health check with strict client timeouts and asynchronous caching
@Component
public class ResilientExternalServiceHealthIndicator implements HealthIndicator {

    private final RestClient partnerClient;
    private volatile boolean isPartnerAvailable = true;
    private volatile Instant lastChecked = Instant.EPOCH;

    public ResilientExternalServiceHealthIndicator(RestClient.Builder builder) {
        this.partnerClient = builder
            .baseUrl("https://slow-partner.com")
            .requestFactory(ClientHttpRequestFactories.get(
                ClientHttpRequestFactorySettings.DEFAULTS
                    .withConnectTimeout(Duration.ofMillis(500))
                    .withReadTimeout(Duration.ofMillis(1000))
            ))
            .build();
    }

    @Scheduled(fixedRate = 10000) // Poll asynchronously every 10 seconds
    public void backgroundHealthCheck() {
        try {
            partnerClient.get().uri("/ping").retrieve().toBodilessEntity();
            isPartnerAvailable = true;
        } catch (Exception ex) {
            isPartnerAvailable = false;
        }
        lastChecked = Instant.now();
    }

    @Override
    public Health health() {
        // Return cached state immediately without blocking probe thread
        if (isPartnerAvailable) {
            return Health.up().withDetail("lastChecked", lastChecked).build();
        }
        return Health.down().withDetail("reason", "Partner unavailable at " + lastChecked).build();
    }
}
```

### Example 5: High-Performance Dynamic Logger Adjustment Endpoint
```java
package com.example.observability.actuator;

import org.slf4j.LoggerFactory;
import org.springframework.boot.actuate.endpoint.annotation.Endpoint;
import org.springframework.boot.actuate.endpoint.annotation.ReadOperation;
import org.springframework.boot.actuate.endpoint.annotation.WriteOperation;
import org.springframework.boot.logging.LogLevel;
import org.springframework.boot.logging.LoggingSystem;
import org.springframework.stereotype.Component;

import java.util.Map;

@Component
@Endpoint(id = "fast-loggers")
public class HighPerformanceLoggerEndpoint {

    private final LoggingSystem loggingSystem;

    public HighPerformanceLoggerEndpoint() {
        this.loggingSystem = LoggingSystem.get(ClassLoader.getSystemClassLoader());
    }

    @ReadOperation
    public Map<String, String> getRootLevel() {
        return Map.of("ROOT", "CONFIGURED");
    }

    @WriteOperation
    public String setLogLevel(String packageName, String level) {
        LogLevel targetLevel = LogLevel.valueOf(level.toUpperCase());
        loggingSystem.setLogLevel(packageName, targetLevel);
        LoggerFactory.getLogger(HighPerformanceLoggerEndpoint.class)
            .info("Dynamically adjusted log level for [{}] to [{}]", packageName, targetLevel);
        return "SUCCESS: Set " + packageName + " to " + targetLevel;
    }
}
```

---

## 6. Best Practices & Production Guidelines

1. **Separate Management Port**: Always set `management.server.port` to an independent port (e.g., `8081` or `9090`). In Kubernetes, expose port `8080` to the Ingress controller and keep `9090` strictly internal to the cluster network.
2. **Never Expose Wildcards**: Avoid `management.endpoints.web.exposure.include: "*"`. Explicitly whitelist only required endpoints (`health,info,prometheus`).
3. **Protect Mutable Operations**: Endpoints with `@WriteOperation` or `@DeleteOperation` (such as `/actuator/loggers` or `/actuator/restart`) must require strict administrative privileges (`ROLE_OPS_ADMIN`) and audit logging.
4. **Non-Blocking Health Checks**: Health indicators must never execute unconstrained blocking operations or multi-second SQL queries. Use caching, background polling, or strict timeouts $\le 500\text{ ms}$.
5. **Git Commit Metadata**: Leverage `git-commit-id-maven-plugin` to populate `/actuator/info` with the exact Git SHA, branch, and build timestamp for instant artifact traceability during incident triage.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Exposing `/actuator/env` publicly | Exposes internal hostnames, JVM system properties, and potentially unmasked credentials. | Restrict exposure (`include: health,info,prometheus`) and secure `/actuator/env` behind strict RBAC and sanitization. |
| Co-locating Actuator on the public API port | Exposes management endpoints to public internet scanners and crawlers. | Configure `management.server.port: 8081` and block `8081` at the edge API Gateway / Kubernetes Ingress. |
| Hanging Kubernetes probes via heavy HealthIndicator | A slow external database or microservice query causes `/actuator/health` to exceed the K8s timeout (1-3s), causing K8s to reboot healthy pods. | Separate probes into Liveness and Readiness groups. Cache external dependency checks asynchronously. |
| Forgetting `@Order` on Actuator `SecurityFilterChain` | If the business API security chain is evaluated first, actuator endpoints may inherit public or business auth policies inappropriately. | Set `@Order(1)` on the management security filter chain and match exclusively with `EndpointRequest.toAnyEndpoint()`. |
| Disabling Actuator in Native Image builds | Missing reflection or runtime metadata for custom `@Endpoint` classes leads to `ClassNotFoundException` at runtime in GraalVM. | Annotate custom endpoints with `@RegisterReflectionForBinding` or rely on Spring Boot 3 AOT hints. |
| Inadvertently logging secrets via Actuator `/env` | Default sanitizers may miss custom variable names like `STRIPE_SK_LIVE`. | Implement a custom `SanitizingFunction` matching custom regex patterns for proprietary credential names. |

---

## 8. Performance & Security Notes

### Garbage Collection & Memory Impact
Actuator endpoints such as `/actuator/beans`, `/actuator/heapdump`, and `/actuator/threaddump` generate substantial object allocations:
- `/actuator/beans` serializes the entire `ApplicationContext` graph into memory. Never poll `/actuator/beans` in automated monitoring loops.
- `/actuator/heapdump` triggers a full `HotSpotDiagnosticMXBean.dumpHeap()` which forces a Stop-The-World (STW) garbage collection phase and writes hundreds of megabytes directly to the disk. Access must be restricted strictly to authorized diagnostic engineers.

### OWASP & Compliance Considerations
Actuator endpoints are a top target for automated vulnerability scanners (CVE-2022-22965 Spring4Shell exploits actively probed `/actuator/env`). Ensure that:
- Write operations enforce CSRF or HTTP Basic/Bearer authentication with nonces.
- CORS on `/actuator/*` is either disabled or restricted to trusted internal administrative origins.

---

## 9. Comparison / When to Use What

| Operational Need | Actuator HTTP Endpoint | Micrometer / Prometheus | JMX MBeans | Kubernetes Native Probes |
|---|---|---|---|---|
| **Pod Liveness & Readiness** | Yes (`/actuator/health/liveness`) | No (metrics are polled, not gating traffic) | No (containers lack JMX ports) | Built-in via HTTP GET probe |
| **Historical Metric Aggregation** | No (instantaneous snapshot only) | Yes (`/actuator/prometheus` scraped by TSDB) | No | No |
| **Dynamic Runtime Config Changes** | Yes (`/actuator/loggers`) | No | Yes (via JConsole / VisualVM) | No (requires rolling restart) |
| **Container Observability** | Ideal (JSON over HTTP) | Ideal | Deprecated in Cloud Native | Built-in |

---

## 10. Interview Questions & Answers

### Q1: What is the difference between an Endpoint and a Controller in Spring Boot?
<details>
<summary>Click to view answer</summary>

An ordinary Spring `@Controller` or `@RestController` is tightly coupled to the Spring MVC or WebFlux servlet/reactive HTTP pipeline. In contrast, an Actuator `@Endpoint` is **transport-agnostic**. The same `@Endpoint` class and its `@ReadOperation` methods can be exposed concurrently over HTTP (via `WebMvcEndpointHandlerMapping`), over reactive HTTP (via WebFlux), and over JMX MBeans without changing a single line of business logic.
</details>

### Q2: How does Spring Boot isolate management endpoints onto a separate port?
<details>
<summary>Click to view answer</summary>

When `management.server.port` is set to a value distinct from `server.port`, Spring Boot creates a **child `WebServer`** within a specialized child `ApplicationContext` (or dedicated WebServer lifecycle). This secondary web server binds to the configured port and hosts only the management filter chain and Actuator mappings, ensuring that operational traffic cannot be accessed through the public web server port.
</details>

### Q3: How do you implement a custom `@Endpoint` that accepts path variables?
<details>
<summary>Click to view answer</summary>

Path variables in Actuator endpoints are defined using the `@Selector` annotation on method parameters. For example:
```java
@ReadOperation
public MetricData getMetric(@Selector String metricName) { ... }
```
When invoked over HTTP, `/actuator/myendpoint/{metricName}` resolves the path segment directly into the `metricName` parameter.
</details>

### Q4: Why is exposing `/actuator/heapdump` dangerous in production, and how should it be mitigated?
<details>
<summary>Click to view answer</summary>

`/actuator/heapdump` invokes `HotSpotDiagnosticMXBean` which:
1. Pauses all application threads (STW pause) while capturing the heap snapshot.
2. Generates a massive file (equal to the allocated JVM heap size, potentially 4GB–32GB), leading to disk exhaustion on ephemeral container volumes.
3. Exposes all plaintext secrets, tokens, and database passwords residing in memory.
Mitigation: Disable it completely via `management.endpoint.heapdump.enabled=false`, or restrict access strictly to emergency roles over VPN.
</details>

### Q5: How does Actuator integrate with Kubernetes Liveness and Readiness probes?
<details>
<summary>Click to view answer</summary>

Spring Boot detects when it runs inside a Kubernetes container (via environment variables or `spring.main.cloud-platform=kubernetes`) and automatically enables probe groups:
- `/actuator/health/liveness`: Checks internal application liveness state (`LivenessState.CORRECT`).
- `/actuator/health/readiness`: Verifies readiness state (`ReadinessState.ACCEPTING_TRAFFIC`), checking connection pools and subsystem health.
</details>

### Q6: What is the purpose of `SanitizingFunction` in Spring Boot 3?
<details>
<summary>Click to view answer</summary>

`SanitizingFunction` is a functional interface in Spring Boot Actuator that inspects key-value pairs exposed through endpoints like `/actuator/env` and `/actuator/configprops`. It allows developers to customize masking logic, replacing sensitive values (passwords, tokens, credentials) with sanitized placeholders like `******` before the payload is rendered to JSON.
</details>

### Q7: Can Actuator endpoints be compiled to native executables with GraalVM?
<details>
<summary>Click to view answer</summary>

Yes. Spring Boot 3 AOT engine analyzes `@Endpoint`, `@ReadOperation`, `@WriteOperation`, and endpoint return types during compilation, generating GraalVM reflection configuration and bean definitions ahead of time. However, dynamic reflective access to custom payload DTOs must be registered with `@RegisterReflectionForBinding` if they are not directly declared in method signatures.
</details>

### Q8: What happens if an external dependency fails in an `@Endpoint` read operation?
<details>
<summary>Click to view answer</summary>

If an unhandled exception is thrown during an `@ReadOperation`, Actuator catches the exception and returns an HTTP `500 Internal Server Error` with a standardized JSON error structure. For health indicators, throwing an exception automatically transitions the component's status to `Status.DOWN`, which by default causes the parent `/actuator/health` endpoint to return HTTP `503 Service Unavailable`.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build an Uptime & Deployment Metadata Endpoint (Easy)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Create a custom `@Endpoint(id = "deployment")` that exposes the application startup instant, total uptime in seconds, and JVM version.

**Solution**:
```java
package com.example.observability.actuator;

import org.springframework.boot.actuate.endpoint.annotation.Endpoint;
import org.springframework.boot.actuate.endpoint.annotation.ReadOperation;
import org.springframework.stereotype.Component;

import java.lang.management.ManagementFactory;
import java.time.Duration;
import java.time.Instant;
import java.util.Map;

@Component
@Endpoint(id = "deployment")
public class DeploymentMetadataEndpoint {

    private final Instant startupTime = Instant.now();

    @ReadOperation
    public Map<String, Object> getDeploymentInfo() {
        long uptimeMillis = ManagementFactory.getRuntimeMXBean().getUptime();
        return Map.of(
            "startedAt", startupTime.toString(),
            "uptimeSeconds", Duration.ofMillis(uptimeMillis).toSeconds(),
            "jvmVersion", Runtime.version().toString(),
            "availableProcessors", Runtime.getRuntime().availableProcessors()
        );
    }
}
```
</details>

### Exercise 2: Add Custom Health Indicator for Redis Cluster (Medium)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Implement a custom `HealthIndicator` that checks Redis connectivity using a 500ms timeout and reports cluster node count.

**Solution**:
```java
package com.example.observability.actuator;

import org.springframework.boot.actuate.health.Health;
import org.springframework.boot.actuate.health.HealthIndicator;
import org.springframework.data.redis.connection.RedisConnection;
import org.springframework.data.redis.connection.RedisConnectionFactory;
import org.springframework.stereotype.Component;

@Component
public class RedisClusterHealthIndicator implements HealthIndicator {

    private final RedisConnectionFactory connectionFactory;

    public RedisClusterHealthIndicator(RedisConnectionFactory connectionFactory) {
        this.connectionFactory = connectionFactory;
    }

    @Override
    public Health health() {
        try (RedisConnection connection = connectionFactory.getConnection()) {
            String pingResult = connection.ping();
            if ("PONG".equalsIgnoreCase(pingResult)) {
                return Health.up()
                    .withDetail("ping", "PONG")
                    .withDetail("clientName", connection.getClientName())
                    .build();
            }
            return Health.down().withDetail("ping", pingResult).build();
        } catch (Exception ex) {
            return Health.down(ex).withDetail("error", "Unable to ping Redis cluster").build();
        }
    }
}
```
</details>

### Exercise 3: Filter Actuator Endpoints via Custom Security Matcher (Hard)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Configure Spring Security so that `/actuator/prometheus` is accessible only from internal subnet `10.0.0.0/8`, `/actuator/health` is public, and all other actuator endpoints require mutual TLS (X.509 certificate) or HTTP Basic admin credentials.

**Solution**:
```java
package com.example.observability.config;

import org.springframework.boot.actuate.autoconfigure.security.servlet.EndpointRequest;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.core.annotation.Order;
import org.springframework.security.authorization.AuthorizationDecision;
import org.springframework.security.config.Customizer;
import org.springframework.security.config.annotation.web.builders.HttpSecurity;
import org.springframework.security.web.SecurityFilterChain;
import org.springframework.security.web.util.matcher.IpAddressMatcher;

@Configuration
public class HardenedActuatorSecurity {

    private static final IpAddressMatcher INTERNAL_SUBNET = new IpAddressMatcher("10.0.0.0/8");

    @Bean
    @Order(1)
    public SecurityFilterChain actuatorFilter(HttpSecurity http) throws Exception {
        return http
            .securityMatcher(EndpointRequest.toAnyEndpoint())
            .authorizeHttpRequests(auth -> auth
                .requestMatchers(EndpointRequest.to("health", "info")).permitAll()
                .requestMatchers(EndpointRequest.to("prometheus")).access((authentication, context) ->
                    new AuthorizationDecision(INTERNAL_SUBNET.matches(context.getRequest())))
                .anyRequest().hasRole("SUPER_ADMIN")
            )
            .httpBasic(Customizer.withDefaults())
            .csrf(csrf -> csrf.disable())
            .build();
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+-----------------------------------+-------------------------------------------------------------+
| Property / Annotation             | Functionality / Description                                 |
+-----------------------------------+-------------------------------------------------------------+
| @Endpoint(id = "name")            | Declares a protocol-agnostic Actuator endpoint              |
| @ReadOperation                    | Maps to HTTP GET / JMX Attribute read                       |
| @WriteOperation                   | Maps to HTTP POST / JMX Operation invoke                    |
| @DeleteOperation                  | Maps to HTTP DELETE / JMX Operation delete                  |
| @Selector                         | Resolves path variable segment in endpoint URI               |
| management.server.port            | Binds Actuator to dedicated operational port                |
| management.endpoints.web.exposure | Controls whitelisted HTTP endpoints (e.g., health, prometheus)|
| SanitizingFunction                | Mask sensitive keys/values in /actuator/env & configprops   |
+-----------------------------------+-------------------------------------------------------------+
```

---

## 13. Further Reading
- Spring Boot Official Actuator Documentation: Production-ready Features
- RFC 7807: Problem Details for HTTP APIs
- CNCF Kubernetes Container Probes Architecture Guide
- Micrometer Documentation: Instrumenting JVM Applications