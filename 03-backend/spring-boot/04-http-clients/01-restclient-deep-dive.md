# RestClient Deep Dive in Spring Boot 3.2+

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | Java HTTP Clients, RestTemplate, Virtual Threads (Java 21) |
| **Estimated Reading Time** | 25 minutes |
| **Status** | Production-Ready |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Baseline** | OpenJDK 21 LTS / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | JDK HttpClient, Apache HttpComponents 5, OpenJDK 21.0.2 |

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
9. [Comparison: RestClient vs RestTemplate vs WebClient](#9-comparison-restclient-vs-resttemplate-vs-webclient)
10. [Interview Questions & Deep Dives](#10-interview-questions--deep-dives)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference](#12-summary--quick-reference)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

For over 15 years, `RestTemplate` served as Spring's primary synchronous HTTP client. However, as REST APIs grew more sophisticated, its overloaded method signature pattern (`getForObject`, `exchange`, `execute`) became notoriously difficult to read and maintain. Meanwhile, Spring's fluent `WebClient` required bringing in the heavy reactive stack (`spring-boot-starter-webflux`, Project Reactor), which introduced unneeded complexity for standard blocking or virtual-thread-based applications.

### The Problem
- `RestTemplate`: Clunky, parameter-heavy API lacking fluent request builders. In maintenance mode since Spring Framework 5.0.
- `WebClient`: Elegant fluent API, but drags in reactive runtime dependencies and requires non-blocking mental overhead.

### The Solution: `RestClient` (Introduced in Spring Boot 3.2 / Spring 6.1)
`RestClient` is the modern synchronous HTTP client that provides the best of both worlds:
1. **Fluent, Functional API**: Mirrors the intuitive API design of `WebClient`.
2. **Synchronous & Virtual Thread Native**: Runs atop standard blocking I/O, seamlessly pairing with Java 21 Virtual Threads without reactive baggage.
3. **Underlying Engine Agnostic**: Pluggable HTTP request factories (JDK `HttpClient`, Apache HttpClient 5, Jetty Client).
4. **Declarative Error Handling**: Fine-grained HTTP status predicates (`onStatus(predicate, handler)`).

---

## 2. Core Concepts

### Mental Model & Execution Pipeline
```
RestClient.builder()
          │
          ▼
    ClientHttpRequestFactory (JDK HttpClient / Apache HttpComponents)
          │
          ▼
    ClientHttpRequestInterceptor Chain (Logging, Tracing, Auth Injection)
          │
          ▼
    HTTP Request Dispatched over Wire
          │
          ▼
    Raw HTTP Response Received
          │
          ▼
    onStatus() Evaluation Pipeline
    ┌─────────────────────────┴─────────────────────────┐
    ▼                                                   ▼
Status 4xx / 5xx Predicate Matches             2xx Success / Default
Throws Domain Exception                        HttpMessageConverter Deserialization
                                               (MappingJackson2HttpMessageConverter)
                                                        │
                                                        ▼
                                               Returns Strongly Typed Record
```

### Invariants
1. **Synchronous Execution**: Every `retrieve()` or `exchange()` call executes synchronously on the calling thread (ideal for Java 21 Virtual Threads).
2. **Immutable Instances**: `RestClient` instances are thread-safe and immutable once built. Use `.mutate()` to clone and customize configurations for specific services.
3. **No Reactive Dependencies**: `RestClient` belongs to `spring-web` and does NOT require Spring WebFlux or Project Reactor.

---

## 3. How It Works Internally

### The Builder Pipeline
`RestClient.create()` or `RestClient.builder()` configures an instance of `DefaultRestClient`:
1. It registers an `ObservableClientHttpRequestFactory` configured with an underlying client provider (defaults to `JdkClientHttpRequestFactory` in modern Spring Boot).
2. It initializes message converters (`HttpMessageConverters`) for JSON/XML (de)serialization.
3. It chains registered `ClientHttpRequestInterceptor` instances.
4. When executing `.retrieve()`, `DefaultResponseSpec` handles status code inspections. If an unhandled 4xx or 5xx status is encountered, it throws `HttpClientErrorException` or `HttpServerErrorException` with RFC 9457 `ProblemDetail` support.

---

## 4. Syntax & Configuration

### Dependency Setup (`pom.xml`)
`RestClient` is bundled directly inside `spring-boot-starter-web` (Spring Boot 3.2+). No additional starters are required. To use Apache HttpClient 5 as the underlying engine:
```xml
<dependency>
    <groupId>org.apache.httpcomponents.client5</groupId>
    <artifactId>httpclient5</artifactId>
</dependency>
```

### Programmatic Bean Definition
```java
package com.example.clients.config;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.http.client.JdkClientHttpRequestFactory;
import org.springframework.web.client.RestClient;

import java.net.http.HttpClient;
import java.time.Duration;

@Configuration(proxyBeanMethods = false)
public class RestClientConfig {

    @Bean
    public RestClient defaultRestClient(RestClient.Builder builder) {
        // Configure JDK HttpClient with custom timeouts and HTTP/2
        HttpClient httpClient = HttpClient.newBuilder()
                .version(HttpClient.Version.HTTP_2)
                .connectTimeout(Duration.ofSeconds(5))
                .build();

        JdkClientHttpRequestFactory requestFactory = new JdkClientHttpRequestFactory(httpClient);
        requestFactory.setReadTimeout(Duration.ofSeconds(10));

        return builder
                .requestFactory(requestFactory)
                .baseUrl("https://api.partner.com")
                .defaultHeader("Accept", "application/json")
                .build();
    }
}
```

---

## 5. Practical Examples

### Example 1: Basic GET and POST with Java Records

```java
package com.example.clients.service;

import org.springframework.http.MediaType;
import org.springframework.stereotype.Service;
import org.springframework.web.client.RestClient;

import java.math.BigDecimal;
import java.util.List;
import java.util.UUID;

public record OrderDto(UUID id, String item, BigDecimal amount) {}
public record CreateOrderRequest(String item, BigDecimal amount) {}

@Service
public class OrderClientService {

    private final RestClient restClient;

    public OrderClientService(RestClient.Builder builder) {
        this.restClient = builder
                .baseUrl("https://orders-service.internal/api/v1")
                .build();
    }

    public OrderDto getOrderById(UUID orderId) {
        return restClient.get()
                .uri("/orders/{id}", orderId)
                .accept(MediaType.APPLICATION_JSON)
                .retrieve()
                .body(OrderDto.class);
    }

    public OrderDto createOrder(CreateOrderRequest request) {
        return restClient.post()
                .uri("/orders")
                .contentType(MediaType.APPLICATION_JSON)
                .body(request)
                .retrieve()
                .body(OrderDto.class);
    }
}
```

### Example 2: Enterprise Error Handling with `onStatus()` and Domain Exceptions

```java
package com.example.clients.service;

import org.springframework.http.HttpStatus;
import org.springframework.http.HttpStatusCode;
import org.springframework.http.ProblemDetail;
import org.springframework.stereotype.Service;
import org.springframework.web.client.RestClient;

import java.util.UUID;

public class PaymentGatewayException extends RuntimeException {
    private final ProblemDetail problemDetail;

    public PaymentGatewayException(String message, ProblemDetail problemDetail) {
        super(message);
        this.problemDetail = problemDetail;
    }
    public ProblemDetail getProblemDetail() { return problemDetail; }
}

@Service
public class PaymentClientService {

    private final RestClient restClient;

    public PaymentClientService(RestClient.Builder builder) {
        this.restClient = builder
                .baseUrl("https://payments.internal/api/v1")
                .defaultStatusHandler(
                        HttpStatusCode::is4xxClientError,
                        (request, response) -> {
                            // Extract RFC 9457 ProblemDetail from upstream error response
                            ProblemDetail problem = response.bodyTo(ProblemDetail.class);
                            throw new PaymentGatewayException("Client error calling payment gateway", problem);
                        }
                )
                .defaultStatusHandler(
                        HttpStatusCode::is5xxServerError,
                        (request, response) -> {
                            throw new RuntimeException("Upstream payment gateway unavailable. Status: " + response.getStatusCode());
                        }
                )
                .build();
    }

    public String authorizeTransaction(UUID accountId, double amount) {
        return restClient.post()
                .uri("/authorize")
                .body(new AuthRequest(accountId, amount))
                .retrieve()
                .body(String.class);
    }

    record AuthRequest(UUID accountId, double amount) {}
}
```

### Example 3: Edge Case — Dynamic Bearer Token Injection via Interceptor

```java
package com.example.clients.interceptor;

import org.springframework.http.HttpRequest;
import org.springframework.http.client.ClientHttpRequestExecution;
import org.springframework.http.client.ClientHttpRequestInterceptor;
import org.springframework.http.client.ClientHttpResponse;
import org.springframework.stereotype.Component;

import java.io.IOException;

@Component
public class BearerTokenAuthInterceptor implements ClientHttpRequestInterceptor {

    private final TokenCacheService tokenCacheService;

    public BearerTokenAuthInterceptor(TokenCacheService tokenCacheService) {
        this.tokenCacheService = tokenCacheService;
    }

    @Override
    public ClientHttpResponse intercept(HttpRequest request, byte[] body, ClientHttpRequestExecution execution) throws IOException {
        String activeToken = tokenCacheService.getValidToken();
        request.getHeaders().setBearerAuth(activeToken);
        request.getHeaders().set("X-Client-Version", "3.4.0");
        return execution.execute(request, body);
    }
}
```

### Example 4: Anti-Pattern vs Production Fix

#### Anti-Pattern: Using Legacy `RestTemplate.exchange()` with Parameter Bloat
```java
// ANTI-PATTERN: Brittle, cluttered RestTemplate exchange
HttpHeaders headers = new HttpHeaders();
headers.setContentType(MediaType.APPLICATION_JSON);
HttpEntity<CreateUserDto> entity = new HttpEntity<>(dto, headers);

ResponseEntity<UserDto> response = restTemplate.exchange(
    "https://api.example.com/users/{id}",
    HttpMethod.PUT,
    entity,
    UserDto.class,
    userId
); // Clunky, hard to read, easy to mix up parameter order
```

#### Production Fix: Modern Fluent `RestClient`
```java
// PRODUCTION FIX: Readable, self-documenting fluent call
UserDto updatedUser = restClient.put()
    .uri("https://api.example.com/users/{id}", userId)
    .contentType(MediaType.APPLICATION_JSON)
    .body(dto)
    .retrieve()
    .body(UserDto.class);
```

### Example 5: High-Performance Virtual Thread Dispatcher Integration

```java
package com.example.clients.virtualthreads;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Service;
import org.springframework.web.client.RestClient;

import java.util.List;
import java.util.UUID;
import java.util.concurrent.Executors;
import java.util.concurrent.StructuredTaskScope;

@Service
public class ParallelCatalogAggregator {

    private static final Logger log = LoggerFactory.getLogger(ParallelCatalogAggregator.class);
    private final RestClient restClient;

    public ParallelCatalogAggregator(RestClient.Builder builder) {
        this.restClient = builder.baseUrl("https://services.internal").build();
    }

    public record AggregatedProduct(String details, String pricing, String inventory) {}

    /**
     * Executes 3 concurrent HTTP calls over Virtual Threads using Structured Concurrency!
     */
    public AggregatedProduct fetchAggregatedData(UUID productId) throws InterruptedException {
        try (var scope = new StructuredTaskScope.ShutdownOnFailure()) {
            var detailsTask = scope.fork(() -> restClient.get().uri("/products/{id}", productId).retrieve().body(String.class));
            var pricingTask = scope.fork(() -> restClient.get().uri("/pricing/{id}", productId).retrieve().body(String.class));
            var inventoryTask = scope.fork(() -> restClient.get().uri("/inventory/{id}", productId).retrieve().body(String.class));

            scope.join().throwIfFailed(); // Wait for all 3 HTTP calls to complete

            return new AggregatedProduct(detailsTask.get(), pricingTask.get(), inventoryTask.get());
        } catch (Exception ex) {
            log.error("Failed to aggregate catalog data", ex);
            throw new RuntimeException("Aggregation failure", ex);
        }
    }
}
```

---

## 6. Best Practices

1. **Inject `RestClient.Builder`**: Always inject `RestClient.Builder` rather than calling `RestClient.create()`. Spring Boot auto-configures the builder with global message converters, SSL bundles, and observability metrics.
2. **Always Configure Timeouts**: Default JDK `HttpClient` connections have unbounded read timeouts. Always configure explicit connect and read timeouts via `ClientHttpRequestFactory`.
3. **Use `.mutate()` for Domain Clients**: Define a base `RestClient` configured with security and observability, then call `.mutate().baseUrl(...)` to build specialized clients for individual microservices.
4. **Leverage `defaultStatusHandler()` for RFC 9457**: Map 4xx/5xx responses to domain exceptions containing `ProblemDetail` models.
5. **Pair with Virtual Threads**: In Java 21+, run `RestClient` calls concurrently using Virtual Threads or Structured Concurrency (`StructuredTaskScope`) rather than introducing heavy reactive frameworks.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Omitting Read Timeouts on `JdkClientHttpRequestFactory`** | If the remote server hangs without closing the socket, worker threads freeze indefinitely. | Always call `requestFactory.setReadTimeout(Duration.ofSeconds(10))`. |
| **Using `new RestTemplate()` in New Projects** | RestTemplate is in maintenance mode; lacks fluent API and fine-grained error handling. | Migrate to `RestClient`. |
| **Consuming Body Multiple Times in Status Handlers** | `response.getBody()` consumes the input stream; downstream exception handlers find an empty stream. | Use `response.bodyTo(ProblemDetail.class)` once, or buffer responses. |
| **Sharing Mutable State in Interceptors** | `ClientHttpRequestInterceptor` beans are singletons; storing per-request tokens in fields causes race conditions. | Fetch per-request state from `SecurityContextHolder` or thread-safe caches. |
| **Hardcoding Base URLs Across Multiple Services** | Makes environment migration and staging configurations brittle. | Externalize URLs into `@ConfigurationProperties` and apply via `.baseUrl()`. |
| **Assuming `RestClient` is Non-Blocking** | `RestClient` blocks the calling thread. If called inside an event-loop thread (Netty), it crashes the server. | Only use `RestClient` on platform threads or virtual threads. Use `WebClient` on event loops. |

---

## 8. Performance & Security Considerations

### Connection Pool Sizing with Apache HttpClient 5
The default JDK `HttpClient` manages connection pooling internally. If switching to Apache HttpClient 5 via `HttpComponentsClientHttpRequestFactory`, explicitly configure the `PoolingHttpClientConnectionManager` with max total connections (e.g. 200) and max connections per route (e.g. 50) to prevent connection bottlenecking under heavy load.

### SSRF (Server-Side Request Forgery) Prevention
Never pass unvalidated user inputs directly into `restClient.get().uri(userSuppliedUrl)`. Malicious users can target internal cloud metadata endpoints (`http://169.254.169.254/latest/meta-data/`). Always validate URLs against a strict whitelist of approved external domains.

---

## 9. Comparison: RestClient vs RestTemplate vs WebClient

| Feature | `RestClient` | `RestTemplate` | `WebClient` |
| :--- | :--- | :--- | :--- |
| **Introduced In** | Spring Boot 3.2 (Spring 6.1) | Spring 3.0 | Spring 5.0 |
| **API Style** | Fluent & Functional | Overloaded Template Methods | Fluent Reactive |
| **Concurrency Model** | Synchronous (Virtual Thread Native) | Synchronous (Platform Threads) | Non-Blocking Event-Loop (Reactive) |
| **Reactive Stack Required?**| No | No | Yes (`spring-webflux`) |
| **Maintenance Status** | Active (Primary Synchronous Client)| Maintenance Mode | Active (Primary Reactive Client) |
| **Best Used For** | Modern Java 21+ Microservices, MVC | Legacy Spring Boot 2.x apps | WebFlux applications, Reactive Streams |

---

## 10. Interview Questions & Deep Dives

<details>
<summary><strong>Q1: Why did Spring introduce `RestClient` when `RestTemplate` and `WebClient` already existed?</strong></summary>

**Answer:**
`RestTemplate` had become a monolithic class with over 40 overloaded methods, making request building clumsy and error-prone. `WebClient` offered an elegant fluent API, but required the entire reactive stack (Project Reactor, Netty), which was overkill for standard Servlet-based applications. With the arrival of Java 21 Virtual Threads, synchronous blocking code became highly scalable again. `RestClient` provides the modern fluent API of `WebClient` while running synchronously on standard blocking request factories, eliminating the need for reactive dependencies in MVC apps.
</details>

<details>
<summary><strong>Q2: How does `RestClient` handle 4xx and 5xx HTTP error responses by default?</strong></summary>

**Answer:**
By default, if the response status is 4xx, `RestClient` throws `HttpClientErrorException`. If the status is 5xx, it throws `HttpServerErrorException`. Both exceptions inherit from `HttpStatusCodeException` and provide access to the raw response body and headers. Developers can override this behavior using `.onStatus(predicate, errorHandler)` or `.defaultStatusHandler(...)`.
</details>

<details>
<summary><strong>Q3: How do you configure connect and read timeouts on `RestClient`?</strong></summary>

**Answer:**
Timeouts are configured on the underlying `ClientHttpRequestFactory`. For example, with `JdkClientHttpRequestFactory`:
```java
HttpClient httpClient = HttpClient.newBuilder().connectTimeout(Duration.ofSeconds(5)).build();
JdkClientHttpRequestFactory factory = new JdkClientHttpRequestFactory(httpClient);
factory.setReadTimeout(Duration.ofSeconds(10));
RestClient.builder().requestFactory(factory).build();
```
</details>

<details>
<summary><strong>Q4: What is the difference between `.retrieve()` and `.exchange()` in `RestClient`?</strong></summary>

**Answer:**
- `.retrieve()`: High-level, declarative approach. Automatically handles error status codes and directly converts the response body into domain objects via `.body(Class)`.
- `.exchange()`: Low-level imperative approach. Gives direct access to the raw `ClientHttpRequest` and `ClientHttpResponse`, allowing manual status inspection and stream consumption without automated error throwing.
</details>

<details>
<summary><strong>Q5: Can `RestClient` be used with Spring's declarative HTTP Interfaces (`@HttpExchange`)?</strong></summary>

**Answer:**
Yes. Spring Framework 6.1 introduced `HttpServiceProxyFactory.builderFor(RestClientAdapter.create(restClient)).build()`, allowing declarative Java interfaces to be backed directly by `RestClient`.
</details>

<details>
<summary><strong>Q6: Is `RestClient` thread-safe?</strong></summary>

**Answer:**
Yes. Once instantiated via `.build()`, `RestClient` is completely immutable and thread-safe. A single `RestClient` bean can be safely shared across concurrent worker threads.
</details>

<details>
<summary><strong>Q7: How does `RestClient` participate in distributed tracing with Micrometer?</strong></summary>

**Answer:**
When using `RestClient.Builder` provided by Spring Boot auto-configuration, Spring automatically attaches an observation interceptor (`ObservationClientHttpRequestInterceptor`). This propagates W3C traceparent headers (`traceparent`, `baggage`) and records Micrometer HTTP client metrics out of the box.
</details>

<details>
<summary><strong>Q8: How does `.mutate()` work on `RestClient`?</strong></summary>

**Answer:**
`restClient.mutate()` returns a new `RestClient.Builder` pre-populated with all the interceptors, request factories, message converters, and default headers of the parent instance. This allows creating child clients with custom base URLs or headers without rebuilding from scratch.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Create Basic GET Client Call</strong></summary>

**Task**: Write a method that uses `RestClient` to fetch a `UserRecord` from `https://api.example.com/users/{id}`.

```java
// Solution:
public UserRecord fetchUser(UUID id) {
    return restClient.get()
            .uri("https://api.example.com/users/{id}", id)
            .accept(MediaType.APPLICATION_JSON)
            .retrieve()
            .body(UserRecord.class);
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Configure Base URL on Builder</strong></summary>

**Task**: Create a `RestClient` bean mapped to `https://api.github.com` with default `User-Agent: SpringBootApp`.

```java
// Solution:
@Bean
public RestClient gitHubClient(RestClient.Builder builder) {
    return builder
            .baseUrl("https://api.github.com")
            .defaultHeader("User-Agent", "SpringBootApp")
            .build();
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Custom 404 Status Handler</strong></summary>

**Task**: Configure a status handler that catches HTTP 404 and throws a domain `UserNotFoundException` instead of the default `HttpClientErrorException`.

```java
// Solution:
public UserRecord fetchUserSafe(UUID id) {
    return restClient.get()
            .uri("/users/{id}", id)
            .retrieve()
            .onStatus(status -> status.value() == 404, (req, res) -> {
                throw new UserNotFoundException("User not found: " + id);
            })
            .body(UserRecord.class);
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Custom Logging Interceptor</strong></summary>

**Task**: Write a `ClientHttpRequestInterceptor` that logs the HTTP method, URI, and response status code for every outgoing request.

```java
// Solution:
public class LoggingInterceptor implements ClientHttpRequestInterceptor {
    private static final Logger log = LoggerFactory.getLogger(LoggingInterceptor.class);

    @Override
    public ClientHttpResponse intercept(HttpRequest req, byte[] body, ClientHttpRequestExecution exec) throws IOException {
        log.info("Outgoing Request: {} {}", req.getMethod(), req.getURI());
        ClientHttpResponse res = exec.execute(req, body);
        log.info("Response Status: {}", res.getStatusCode());
        return res;
    }
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Structured Concurrency Multi-Service Fetcher</strong></summary>

**Task**: Implement a service using Java 21 `StructuredTaskScope.ShutdownOnFailure` and `RestClient` that queries three distinct microservice endpoints in parallel, aggregating their results or failing fast if any single call throws an exception.
</details>

<details>
<summary><strong>Exercise 6 (Hard): Resilient RestClient with Exponential Backoff Retries</strong></summary>

**Task**: Build a custom `ClientHttpRequestInterceptor` implementing exponential backoff retries for transient 503 and 504 errors, incorporating random jitter to prevent thundering herd problems.
</details>

---

## 12. Summary & Quick Reference

- **Core Innovation**: Fluent, modern synchronous client introduced in Spring Boot 3.2.
- **Java 21 Perfect Fit**: Runs natively on blocking Virtual Threads.
- **No WebFlux Needed**: Part of standard `spring-web`.
- **Status Handlers**: Use `.onStatus(predicate, handler)` for RFC 9457 error mappings.
- **Builder Pattern**: Always inject `RestClient.Builder` to retain Spring Boot observability and SSL autoconfiguration.

---

## 13. Further Reading

- [Spring Framework Reference: RestClient](https://docs.spring.io/spring-framework/reference/integration/rest-clients.html#rest-restclient)
- [Spring Blog: Introducing RestClient](https://spring.io/blog/2023/07/13/new-in-spring-6-1-restclient)
- [RFC 9457: Problem Details for HTTP APIs](https://www.rfc-editor.org/rfc/rfc9457.html)
- [Java 21 Virtual Threads and Structured Concurrency](https://docs.oracle.com/en/java/javase/21/core/virtual-threads.html)
