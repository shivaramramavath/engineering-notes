# OpenFeign Integration in Spring Boot (Spring Cloud)

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | Declarative HTTP Interfaces, Spring Cloud Basics, Dynamic Proxies |
| **Estimated Reading Time** | 22 minutes |
| **Status** | Production-Ready |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Baseline** | OpenJDK 21 LTS / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | Spring Cloud 2024.0.x, OpenFeign 13.x, OpenJDK 21.0.2 |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Configuration](#4-syntax--configuration)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance & Security Considerations](#8-performance--security-considerations)
9. [Comparison: OpenFeign vs Spring 6 HTTP Interfaces](#9-comparison-openfeign-vs-spring-6-http-interfaces)
10. [Interview Questions & Deep Dives](#10-interview-questions--deep-dives)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference](#12-summary--quick-reference)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

In enterprise Spring Cloud microservice ecosystems, services communicate constantly across networks. Writing low-level HTTP clients for dozens of internal services is repetitive and error-prone.

### The Problem
Traditional HTTP invocation:
1. Requires manual boilerplate to build URLs, serialize JSON, parse headers, and extract status codes.
2. Needs explicit integration with client-side load balancers (Spring Cloud LoadBalancer) and service discovery registries (Eureka, Consul).
3. Lacks a unified contract between upstream producers and downstream consumers.

### The Solution: Spring Cloud OpenFeign
OpenFeign is a declarative web service client developed by Netflix and maintained by the OpenFeign community. In Spring Cloud, `@EnableFeignClients` and `@FeignClient` allow developers to declare Java interfaces using familiar Spring MVC annotations (`@GetMapping`, `@PostMapping`, `@PathVariable`). Spring Cloud automatically generates an implementation proxy wired directly to Spring Cloud LoadBalancer, Micrometer observability, and Apache HttpClient connection pools.

---

## 2. Core Concepts

### Mental Model & Execution Flow
```
        You write an interface:
        @FeignClient(name = "billing-service", path = "/api/v1")
        public interface BillingClient {
            @GetMapping("/invoices/{id}")
            InvoiceDto getInvoice(@PathVariable("id") UUID id);
        }
                          │
                          ▼
            Spring Cloud Proxy Generation
                          │
                          ▼
        Caller executes: billingClient.getInvoice(id)
                          │
                          ▼
             Spring Cloud LoadBalancer
    (Resolves "billing-service" via Eureka/Consul -> 10.0.1.42:8080)
                          │
                          ▼
             RequestInterceptor Chain
    (Injects Bearer Token, Correlation ID)
                          │
                          ▼
           Underlying HTTP Client Engine
    (Apache HttpClient 5 / OkHttp / Default)
                          │
                          ▼
             Target Service Response
                          │
                          ▼
             Custom ErrorDecoder (if 4xx/5xx)
             OR Jackson Decoder (Maps JSON -> InvoiceDto)
```

### Invariants
1. **Spring MVC Contract**: Spring Cloud OpenFeign configures Feign to recognize standard Spring MVC annotations (`@RequestMapping`, `@RequestParam`) instead of native Feign annotations.
2. **Mandatory Parameter Names**: In OpenFeign, `@PathVariable("id")` and `@RequestParam("query")` MUST explicitly specify the variable name in quotes to prevent bytecode parameter erasure bugs.
3. **Load Balanced by Default**: When `name = "service-id"` is used without a `url` attribute, Feign delegates to Spring Cloud LoadBalancer to resolve instances via service discovery.

---

## 3. How It Works Internally

### Feign Client Registration Mechanics
1. `@EnableFeignClients` imports `FeignClientsRegistrar`.
2. It scans the classpath for interfaces annotated with `@FeignClient`.
3. For each interface, it registers a `FeignClientFactoryBean` definition.
4. When injected into a service, `FeignClientFactoryBean` creates a JDK dynamic proxy with Feign's `ReflectiveFeign` engine:
   - Configures `Contract` (SpringMvcContract).
   - Configures `Encoder` (SpringEncoder using Spring's `HttpMessageConverters`).
   - Configures `Decoder` (SpringDecoder).
   - Configures `ErrorDecoder` (Custom error mapping).
   - Wraps the target URL with `FeignBlockingLoadBalancerClient`.

---

## 4. Syntax & Configuration

### Dependency Setup (`pom.xml`)
```xml
<dependencyManagement>
    <dependencies>
        <dependency>
            <groupId>org.springframework.cloud</groupId>
            <artifactId>spring-cloud-dependencies</artifactId>
            <version>2024.0.0</version> <!-- Current Spring Cloud release train -->
            <type>pom</type>
            <scope>import</scope>
        </dependency>
    </dependencies>
</dependencyManagement>

<dependencies>
    <dependency>
        <groupId>org.springframework.cloud</groupId>
        <artifactId>spring-cloud-starter-openfeign</artifactId>
    </dependency>
    <!-- Use Apache HttpClient 5 for connection pooling -->
    <dependency>
        <groupId>io.github.openfeign</groupId>
        <artifactId>feign-hc5</artifactId>
    </dependency>
</dependencies>
```

### Enabling Feign Clients in Main Class
```java
@SpringBootApplication
@EnableFeignClients(basePackages = "com.example.clients")
public class Application {
    public static void main(String[] args) {
        SpringApplication.run(Application.class, args);
    }
}
```

### Standard `application.yaml` Feign Configuration
```yaml
spring:
  cloud:
    openfeign:
      client:
        config:
          default:
            connect-timeout: 3000
            read-timeout: 5000
            logger-level: basic # NONE, BASIC, HEADERS, FULL
          billing-service: # Service-specific override
            read-timeout: 10000
            error-decoder: com.example.clients.feign.CustomErrorDecoder
      compression:
        request:
          enabled: true
        response:
          enabled: true
      httpclient:
        hc5:
          enabled: true # Enable Apache HttpClient 5 connection pooling
```

---

## 5. Practical Examples

### Example 1: Basic `@FeignClient` with Discovery and Direct URL Fallback

```java
package com.example.clients.feign;

import org.springframework.cloud.openfeign.FeignClient;
import org.springframework.web.bind.annotation.*;

import java.math.BigDecimal;
import java.util.UUID;

public record AccountDto(UUID id, String owner, BigDecimal balance) {}
public record TransferRequest(UUID targetAccountId, BigDecimal amount) {}

@FeignClient(
    name = "account-service",
    url = "${services.account.url:}", // If set, uses direct URL; if empty, uses Service Discovery
    path = "/api/v1/accounts"
)
public interface AccountFeignClient {

    @GetMapping("/{id}")
    AccountDto getAccountById(@PathVariable("id") UUID id);

    @PostMapping("/{id}/transfers")
    void executeTransfer(@PathVariable("id") UUID id, @RequestBody TransferRequest request);
}
```

### Example 2: Custom RequestInterceptor for Dynamic Bearer Token Propagation

```java
package com.example.clients.feign;

import feign.RequestInterceptor;
import feign.RequestTemplate;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.security.core.Authentication;
import org.springframework.security.core.context.SecurityContextHolder;

@Configuration(proxyBeanMethods = false)
public class FeignAuthInterceptorConfig {

    @Bean
    public RequestInterceptor oauth2FeignRequestInterceptor() {
        return (RequestTemplate template) -> {
            // Propagate active Bearer token from current security context
            Authentication auth = SecurityContextHolder.getContext().getAuthentication();
            if (auth != null && auth.getCredentials() instanceof String token) {
                template.header("Authorization", "Bearer " + token);
            }
            template.header("X-Source-Service", "OrderProcessingService");
        };
    }
}
```

### Example 3: Edge Case — Custom `ErrorDecoder` for RFC 9457 Problem Details

When the downstream service returns HTTP 4xx or 5xx, Feign throws a generic `FeignException`. A custom `ErrorDecoder` translates this into domain exceptions.

```java
package com.example.clients.feign;

import com.fasterxml.jackson.databind.ObjectMapper;
import feign.Response;
import feign.codec.ErrorDecoder;
import org.springframework.http.ProblemDetail;
import org.springframework.stereotype.Component;

import java.io.InputStream;

public class RemoteServiceException extends RuntimeException {
    private final ProblemDetail problemDetail;

    public RemoteServiceException(String message, ProblemDetail problemDetail) {
        super(message);
        this.problemDetail = problemDetail;
    }
    public ProblemDetail getProblemDetail() { return problemDetail; }
}

@Component
public class CustomFeignErrorDecoder implements ErrorDecoder {

    private final ObjectMapper objectMapper = new ObjectMapper();
    private final ErrorDecoder defaultDecoder = new Default();

    @Override
    public Exception decode(String methodKey, Response response) {
        if (response.body() != null) {
            try (InputStream bodyIs = response.body().asInputStream()) {
                ProblemDetail problem = objectMapper.readValue(bodyIs, ProblemDetail.class);
                return new RemoteServiceException("Remote microservice returned error", problem);
            } catch (Exception ignored) {
                // Fall back to default Feign error decoding
            }
        }
        return defaultDecoder.decode(methodKey, response);
    }
}
```

### Example 4: Anti-Pattern vs Production Fix

#### Anti-Pattern: Using Default Feign Client without Connection Pool
```yaml
# ANTI-PATTERN: Default Feign uses JDK HttpURLConnection
# Creates a new TCP connection on every request, leading to port exhaustion
spring:
  cloud:
    openfeign:
      httpclient:
        hc5:
          enabled: false # BAD! Leaves unpooled connection engine active
```

#### Production Fix: Enable Apache HttpClient 5 or OkHttp
```yaml
# PRODUCTION FIX: Enable Apache HC5 with connection pool management
spring:
  cloud:
    openfeign:
      httpclient:
        hc5:
          enabled: true
          max-connections: 500
          max-connections-per-route: 50
```

### Example 5: High-Performance Fallback via Resilience4j Integration

```java
package com.example.clients.feign;

import org.springframework.cloud.openfeign.FallbackFactory;
import org.springframework.cloud.openfeign.FeignClient;
import org.springframework.stereotype.Component;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PathVariable;

import java.util.UUID;

@FeignClient(
    name = "pricing-service",
    fallbackFactory = PricingClientFallbackFactory.class
)
public interface PricingFeignClient {
    @GetMapping("/pricing/{id}")
    Double getProductPrice(@PathVariable("id") UUID id);
}

@Component
class PricingClientFallbackFactory implements FallbackFactory<PricingFeignClient> {
    @Override
    public PricingFeignClient create(Throwable cause) {
        return (id) -> {
            // Safe fallback value when pricing service is down
            return 9.99; // Default price
        };
    }
}
```

---

## 6. Best Practices

1. **Explicit Parameter Names**: Always specify the annotation value in quotes: `@PathVariable("id")`, `@RequestParam("filter")`. Omitting quotes causes runtime binding failure unless compiled with `-parameters`.
2. **Always Enable Apache HttpClient 5**: Default `HttpURLConnection` does not pool connections; always include `feign-hc5` and set `spring.cloud.openfeign.httpclient.hc5.enabled: true`.
3. **Always Configure Timeouts**: Never rely on default timeouts. Always configure `connect-timeout` and `read-timeout` globally or per-client in YAML.
4. **Implement Custom `ErrorDecoder`**: Translate downstream HTTP 4xx/5xx responses into meaningful internal domain exceptions.
5. **Consider Spring 6 HTTP Interfaces for New Apps**: If your project does not need Spring Cloud Eureka or Ribbon, consider native Spring 6 `@HttpExchange` interfaces backed by `RestClient` to reduce dependencies.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Omitting Attribute Value in `@PathVariable`** | Without `-parameters` flag, Feign throws `IllegalStateException: PathVariable annotation was empty`. | Always write `@PathVariable("id") Long id`. |
| **Using Default Unpooled Connection Engine** | High throughput exhausts ephemeral TCP ports (`java.net.BindException`). | Add `feign-hc5` and enable HttpClient 5. |
| **Circular Feign Client Dependencies** | Microservice A defines a Feign client to B, which defines one to A, causing context scan deadlock. | Refactor shared contracts into clean API boundary libraries. |
| **Mixing Spring MVC and Feign Native Annotations** | Spring Cloud OpenFeign expects `@GetMapping`, NOT native Feign `@RequestLine`. | Use standard Spring MVC annotations unless explicitly configuring Feign contract. |
| **Logging Level Set to FULL in Production** | `logger-level: FULL` logs entire request/response bodies and headers, leaking passwords and tokens. | Set `logger-level: BASIC` in production. |
| **Forgetting `@EnableFeignClients`** | Spring fails to discover `@FeignClient` interfaces, throwing `NoSuchBeanDefinitionException`. | Add `@EnableFeignClients` to `@SpringBootApplication` configuration class. |

---

## 8. Performance & Security Considerations

### Memory Leaks with Large Response Payloads
Feign buffers the entire response body into memory as a `byte[]` by default before decoding. For large file downloads (>50MB), avoid using Feign. Use `RestClient` or `WebClient` with streaming response bodies instead.

### Header Leaks in RequestInterceptors
When implementing a `RequestInterceptor` that copies headers from the incoming `HttpServletRequest` to outgoing Feign calls, never copy sensitive hop-by-hop headers (`Host`, `Content-Length`, `Transfer-Encoding`). Copy only explicitly whitelisted business headers (`Authorization`, `X-Correlation-ID`).

---

## 9. Comparison: OpenFeign vs Spring 6 HTTP Interfaces

| Feature | Spring Cloud OpenFeign | Spring 6 HTTP Interfaces (`@HttpExchange`) |
| :--- | :--- | :--- |
| **Starters Required** | `spring-cloud-starter-openfeign` | Zero (Included in `spring-web`) |
| **Service Discovery** | Native Eureka/Consul integration | Requires manual LoadBalancer setup |
| **Underlying Engine** | Apache HttpClient 5 / OkHttp | `RestClient` or `WebClient` |
| **Virtual Threads Support** | Requires thread pool customization | Native First-Class Support |
| **Contract Annotations** | Spring MVC (`@GetMapping`, etc.) | HTTP Exchanges (`@GetExchange`, etc.) |
| **Industry Trend** | Established Legacy Standard | Modern Greenfield Standard |

---

## 10. Interview Questions & Deep Dives

<details>
<summary><strong>Q1: How does Spring Cloud OpenFeign integrate with Spring Cloud LoadBalancer?</strong></summary>

**Answer:**
When a `@FeignClient` defines a service name (e.g. `name = "order-service"`) without a fixed URL, Feign replaces the target URI host with `http://order-service`. When a request executes, `FeignBlockingLoadBalancerClient` intercepts the call, queries `LoadBalancerClient` to fetch an active service instance from Eureka or Consul, replaces `order-service` with the chosen instance's IP and port (e.g. `10.0.1.42:8080`), and dispatches the request.
</details>

<details>
<summary><strong>Q2: Why must you explicitly specify the name in `@PathVariable("id")` in OpenFeign interfaces?</strong></summary>

**Answer:**
In standard Spring MVC controllers, Spring's runtime argument resolver can infer parameter names from class bytecode if compiled with the `-parameters` flag. However, OpenFeign's contract parser processes interfaces during bean definition scanning and historically relies strictly on the annotation's `value()` attribute. If omitted, Feign throws an `IllegalStateException: PathVariable annotation was empty on param 0`.
</details>

<details>
<summary><strong>Q3: What is the purpose of `ErrorDecoder` in OpenFeign?</strong></summary>

**Answer:**
By default, whenever an HTTP response status is not between 200 and 299, OpenFeign throws a `FeignException`. Implementing a custom `ErrorDecoder` allows developers to intercept the raw `feign.Response`, parse error JSON bodies (like RFC 9457 `ProblemDetail`), and throw specific domain exceptions (e.g. `AccountNotFoundException`, `InsufficientFundsException`) instead of generic client exceptions.
</details>

<details>
<summary><strong>Q4: How do you configure request and response GZIP compression in OpenFeign?</strong></summary>

**Answer:**
In `application.yaml`:
```yaml
spring:
  cloud:
    openfeign:
      compression:
        request:
          enabled: true
          min-request-size: 2048
        response:
          enabled: true
```
</details>

<details>
<summary><strong>Q5: What are the different Feign logging levels and when should each be used?</strong></summary>

**Answer:**
- `NONE`: No logging (default).
- `BASIC`: Logs only the request method, URL, response status code, and execution time (recommended for production).
- `HEADERS`: Logs basic information plus request and response headers.
- `FULL`: Logs headers, body, and metadata for both requests and responses (development only; causes security leaks and memory overhead in production).
</details>

<details>
<summary><strong>Q6: How does OpenFeign handle inheritance of interfaces?</strong></summary>

**Answer:**
OpenFeign supports interface inheritance. You can define a common base interface with shared endpoints and extend it in client interfaces. However, `@FeignClient` annotations should remain on the leaf interface.
</details>

<details>
<summary><strong>Q7: Can OpenFeign clients be mocked during `@WebMvcTest` unit testing?</strong></summary>

**Answer:**
Yes. In Spring Boot test suites, Feign client interfaces can be mocked using `@MockBean MyFeignClient myFeignClient`, allowing tests to simulate downstream service responses without spinning up HTTP servers.
</details>

<details>
<summary><strong>Q8: What is the difference between `@FeignClient(name = "...")` and `@FeignClient(url = "...")`?</strong></summary>

**Answer:**
- `name`: Identifies a logical service in a service discovery registry (Eureka, Consul). Requests are routed through Spring Cloud LoadBalancer.
- `url`: Specifies an explicit physical base URL (e.g. `https://api.thirdparty.com`). When `url` is present, service discovery and load balancing are bypassed.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Basic Feign Client Interface</strong></summary>

**Task**: Define a `@FeignClient` for a service `catalog-service` with an endpoint `GET /items/{id}` returning an `ItemDto`.

```java
// Solution:
@FeignClient(name = "catalog-service", path = "/items")
public interface CatalogClient {
    @GetMapping("/{id}")
    ItemDto getItemById(@PathVariable("id") Long id);
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Configure Feign Timeouts in YAML</strong></summary>

**Task**: Write the `application.yaml` snippet setting global Feign connect timeout to 4s and read timeout to 6s.

```yaml
# Solution:
spring:
  cloud:
    openfeign:
      client:
        config:
          default:
            connect-timeout: 4000
            read-timeout: 6000
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Correlation ID RequestInterceptor</strong></summary>

**Task**: Implement a `RequestInterceptor` that reads `X-Correlation-ID` from the current incoming `HttpServletRequest` attributes and adds it to the outgoing Feign request template.

```java
// Solution:
@Component
public class CorrelationIdFeignInterceptor implements RequestInterceptor {
    @Override
    public void apply(RequestTemplate template) {
        ServletRequestAttributes attrs = (ServletRequestAttributes) RequestContextHolder.getRequestAttributes();
        if (attrs != null) {
            String correlationId = attrs.getRequest().getHeader("X-Correlation-ID");
            if (correlationId != null) {
                template.header("X-Correlation-ID", correlationId);
            }
        }
    }
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Custom ErrorDecoder for 404</strong></summary>

**Task**: Implement an `ErrorDecoder` that throws `ResourceNotFoundException` when the downstream status is 404.

```java
// Solution:
public class NotFoundErrorDecoder implements ErrorDecoder {
    @Override
    public Exception decode(String methodKey, Response response) {
        if (response.status() == 404) {
            return new ResourceNotFoundException("Resource not found from: " + methodKey);
        }
        return new Default().decode(methodKey, response);
    }
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): FallbackFactory with Detailed Exception Logging</strong></summary>

**Task**: Implement a `FallbackFactory<OrderClient>` that logs the specific exception cause and returns a stubbed order with status `"DEGRADED_SERVICE"`.
</details>

<details>
<summary><strong>Exercise 6 (Hard): Mutual TLS (mTLS) Apache HC5 Feign Client Configuration</strong></summary>

**Task**: Programmatically configure an Apache HttpClient 5 `CloseableHttpClient` bean with client SSL keystore and CA truststore, wiring it directly into Feign's `feign.hc5.ApacheHttp5Client`.
</details>

---

## 12. Summary & Quick Reference

- **Key Dependency**: `spring-cloud-starter-openfeign` + `feign-hc5`.
- **Annotation**: `@FeignClient(name = "service-name")` + Spring MVC annotations.
- **Strict Rule**: Always explicitly name parameters: `@PathVariable("id")`.
- **Performance Invariant**: Always enable Apache HC5 connection pooling.
- **Alternative**: For new projects without Spring Cloud, prefer Spring 6 `@HttpExchange` interfaces.

---

## 13. Further Reading

- [Spring Cloud OpenFeign Official Documentation](https://docs.spring.io/spring-cloud-openfeign/docs/current/reference/html/)
- [OpenFeign Community GitHub Repository](https://github.com/OpenFeign/feign)
- [Spring Cloud LoadBalancer Reference Guide](https://docs.spring.io/spring-cloud-commons/docs/current/reference/html/#spring-cloud-loadbalancer)
- [Migrating from OpenFeign to Spring 6 HTTP Interfaces](https://spring.io/guides)
