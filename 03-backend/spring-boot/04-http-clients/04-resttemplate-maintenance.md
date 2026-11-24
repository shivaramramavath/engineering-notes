# RestTemplate Maintenance & Migration Guide in Spring Boot

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | RestTemplate, RestClient, Spring Boot Web |
| **Estimated Reading Time** | 22 minutes |
| **Status** | Production-Ready |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Baseline** | OpenJDK 21 LTS / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | OpenJDK 21.0.2, Spring Boot 3.4.0 |

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
9. [Comparison: RestTemplate vs RestClient Side-by-Side](#9-comparison-resttemplate-vs-restclient-side-by-side)
10. [Interview Questions & Deep Dives](#10-interview-questions--deep-dives)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference](#12-summary--quick-reference)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

`RestTemplate` has been the cornerstone of HTTP communication in Spring since version 3.0 (2009). However, with the release of Spring Framework 5.0, the Spring engineering team officially placed `RestTemplate` into **maintenance mode**. In Spring Boot 3.2, the team introduced `RestClient` as its modern, fluent successor.

### The Problem with Legacy `RestTemplate`
1. **Method Bloat**: `RestTemplate` contains over 40 overloaded methods (`getForObject`, `getForEntity`, `postForLocation`, `exchange`, `execute`), making simple parameterized calls verbose and unwieldy.
2. **Maintenance Mode**: While `RestTemplate` is not deprecated and remains supported for backward compatibility, no major new features or API enhancements will be added to it.
3. **Complex Error Handling**: Intercepting and inspecting 4xx/5xx responses requires registering global `ResponseErrorHandler` implementations that lack contextual granularity.

### The Solution: Maintaining and Migrating to `RestClient`
Enterprise codebases cannot be rewritten overnight. Engineering teams must know:
1. How to maintain existing `RestTemplate` instances securely (connection pooling, timeouts, SSL bundles).
2. How to wrap existing `RestTemplate` configurations directly into `RestClient` using `RestClient.create(restTemplate)`.
3. How to systematically migrate legacy template calls to fluent `RestClient` invocations without regression risk.

---

## 2. Core Concepts

### Mental Model: The Bridge Architecture
```
             Existing Legacy Configuration:
                 RestTemplateCustomizer
                 ClientHttpRequestFactory (Apache HttpClient / SSL Bundles)
                 ClientHttpRequestInterceptor Chain
                               │
                               ▼
               Traditional RestTemplate Bean
                               │
                               ▼
            ╔═══════════════════════════════════╗
            ║   RestClient.create(restTemplate) ║  <-- The Zero-Risk Bridge!
            ╚═══════════════════════════════════╝
                               │
                               ▼
              Modern Fluent RestClient Instance
        (Shares existing interceptors, converters & socket pools!)
```

### Invariants
1. **Maintenance Mode is NOT Deprecation**: Spring has stated that `RestTemplate` will NOT be removed in Spring 6.x / Boot 3.x. Existing production code continues to run safely.
2. **Shared Foundation**: Both `RestTemplate` and `RestClient` share the exact same underlying infrastructure: `ClientHttpRequestFactory`, `ClientHttpRequestInterceptor`, and `HttpMessageConverter`.
3. **Instant Wrapping**: Any existing configured `RestTemplate` bean can be converted into a `RestClient` with a single call: `RestClient.create(restTemplate)`.

---

## 3. How It Works Internally

### `RestClient.create(RestTemplate)` Implementation
Inside Spring Framework's `DefaultRestClientBuilder`:
```java
public DefaultRestClientBuilder(RestTemplate restTemplate) {
    Assert.notNull(restTemplate, "RestTemplate must not be null");
    this.requestFactory = restTemplate.getRequestFactory();
    this.interceptors = new ArrayList<>(restTemplate.getInterceptors());
    this.messageConverters = new ArrayList<>(restTemplate.getMessageConverters());
    this.uriBuilderFactory = restTemplate.getUriTemplateHandler();
}
```
When you pass a `RestTemplate` into `RestClient.create(restTemplate)`:
- It extracts the configured `ClientHttpRequestFactory` (preserving socket timeouts, connection pools, and SSL bundles).
- It clones all registered interceptors (MDC correlation tracing, security tokens).
- It copies all configured Jackson message converters and URI template handlers.

---

## 4. Syntax & Configuration

### Legacy `RestTemplateBuilder` Configuration (Maintaining Production)
```java
package com.example.clients.config;

import org.springframework.boot.web.client.RestTemplateBuilder;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.web.client.RestTemplate;

import java.time.Duration;

@Configuration(proxyBeanMethods = false)
public class LegacyRestTemplateConfig {

    @Bean
    public RestTemplate legacyRestTemplate(RestTemplateBuilder builder) {
        return builder
                .setConnectTimeout(Duration.ofSeconds(5))
                .setReadTimeout(Duration.ofSeconds(10))
                .defaultHeader("X-App-Name", "LegacyBillingService")
                .build();
    }
}
```

---

## 5. Practical Examples

### Example 1: Zero-Risk Migration via `RestClient.create(restTemplate)`

```java
package com.example.clients.migration;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.web.client.RestClient;
import org.springframework.web.client.RestTemplate;

@Configuration(proxyBeanMethods = false)
public class RestClientBridgeConfig {

    /**
     * Reuses existing, battle-tested RestTemplate infrastructure (connection pools,
     * custom SSL bundles, authentication interceptors) while exposing a modern RestClient!
     */
    @Bean
    public RestClient modernRestClient(RestTemplate legacyRestTemplate) {
        return RestClient.create(legacyRestTemplate);
    }
}
```

### Example 2: Side-by-Side Migration (GET Request with Path Variables & Headers)

#### The Legacy `RestTemplate` Implementation:
```java
package com.example.clients.legacy;

import org.springframework.http.*;
import org.springframework.stereotype.Service;
import org.springframework.web.client.RestTemplate;

import java.util.UUID;

public record AccountDto(UUID id, String status, double balance) {}

@Service
public class LegacyAccountService {

    private final RestTemplate restTemplate;

    public LegacyAccountService(RestTemplate restTemplate) {
        this.restTemplate = restTemplate;
    }

    public AccountDto getAccount(UUID accountId, String authToken) {
        HttpHeaders headers = new HttpHeaders();
        headers.setBearerAuth(authToken);
        headers.set("X-Tenant-ID", "tenant-alpha");
        HttpEntity<Void> requestEntity = new HttpEntity<>(headers);

        // Clunky exchange call with generic type parameters
        ResponseEntity<AccountDto> response = restTemplate.exchange(
                "https://accounts.internal/api/v1/accounts/{id}",
                HttpMethod.GET,
                requestEntity,
                AccountDto.class,
                accountId
        );

        return response.getBody();
    }
}
```

#### The Migrated `RestClient` Implementation:
```java
package com.example.clients.modern;

import com.example.clients.legacy.AccountDto;
import org.springframework.stereotype.Service;
import org.springframework.web.client.RestClient;

import java.util.UUID;

@Service
public class ModernAccountService {

    private final RestClient restClient;

    public ModernAccountService(RestClient restClient) {
        this.restClient = restClient;
    }

    public AccountDto getAccount(UUID accountId, String authToken) {
        // Clean, readable fluent API call
        return restClient.get()
                .uri("https://accounts.internal/api/v1/accounts/{id}", accountId)
                .bearerAuth(authToken)
                .header("X-Tenant-ID", "tenant-alpha")
                .retrieve()
                .body(AccountDto.class);
    }
}
```

### Example 3: Side-by-Side Migration (POST Request with Error Handling)

#### The Legacy `RestTemplate` POST:
```java
public OrderResult createOrderLegacy(CreateOrderDto dto) {
    try {
        HttpHeaders headers = new HttpHeaders();
        headers.setContentType(MediaType.APPLICATION_JSON);
        HttpEntity<CreateOrderDto> entity = new HttpEntity<>(dto, headers);

        return restTemplate.postForObject(
                "https://orders.internal/api/v1/orders",
                entity,
                OrderResult.class
        );
    } catch (HttpClientErrorException.BadRequest ex) {
        // Manual body parsing required
        throw new DomainValidationException("Invalid order payload", ex);
    } catch (HttpStatusCodeException ex) {
        throw new RuntimeException("HTTP error: " + ex.getStatusCode(), ex);
    }
}
```

#### The Migrated `RestClient` POST:
```java
public OrderResult createOrderModern(CreateOrderDto dto) {
    return restClient.post()
            .uri("https://orders.internal/api/v1/orders")
            .contentType(MediaType.APPLICATION_JSON)
            .body(dto)
            .retrieve()
            .onStatus(HttpStatusCode::is4xxClientError, (req, res) -> {
                ProblemDetail problem = res.bodyTo(ProblemDetail.class);
                throw new DomainValidationException("Invalid order payload", problem);
            })
            .body(OrderResult.class);
}
```

### Example 4: Maintaining Legacy Custom `ResponseErrorHandler`

```java
package com.example.clients.legacy;

import org.springframework.http.HttpStatusCode;
import org.springframework.http.client.ClientHttpResponse;
import org.springframework.web.client.DefaultResponseErrorHandler;

import java.io.IOException;

public class CustomLegacyResponseErrorHandler extends DefaultResponseErrorHandler {

    @Override
    public boolean hasError(ClientHttpResponse response) throws IOException {
        // Do not treat 404 as an error (allow controllers to handle nulls cleanly)
        if (response.getStatusCode().value() == 404) {
            return false;
        }
        return super.hasError(response);
    }
}
```

### Example 5: High-Performance Connection Pool Maintenance for `RestTemplate`

```java
package com.example.clients.config;

import org.apache.hc.client5.http.impl.classic.CloseableHttpClient;
import org.apache.hc.client5.http.impl.classic.HttpClients;
import org.apache.hc.client5.http.impl.io.PoolingHttpClientConnectionManager;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.http.client.HttpComponentsClientHttpRequestFactory;
import org.springframework.web.client.RestTemplate;

import java.time.Duration;

@Configuration(proxyBeanMethods = false)
public class PooledRestTemplateConfig {

    @Bean
    public RestTemplate pooledRestTemplate() {
        PoolingHttpClientConnectionManager connectionManager = new PoolingHttpClientConnectionManager();
        connectionManager.setMaxTotal(200); // 200 total pooled connections
        connectionManager.setDefaultMaxPerRoute(50); // 50 connections per host

        CloseableHttpClient httpClient = HttpClients.custom()
                .setConnectionManager(connectionManager)
                .evictIdleConnections(Duration.ofSeconds(30))
                .build();

        HttpComponentsClientHttpRequestFactory factory = new HttpComponentsClientHttpRequestFactory(httpClient);
        factory.setConnectTimeout(Duration.ofSeconds(5));
        factory.setConnectionRequestTimeout(Duration.ofSeconds(5));

        return new RestTemplate(factory);
    }
}
```

---

## 6. Best Practices

1. **Do Not Rewrite Everything at Once**: Use `RestClient.create(restTemplate)` to bridge existing, heavily tuned `RestTemplate` beans into `RestClient` without changing infrastructure configurations.
2. **Enforce Timeouts on Legacy Templates**: Default `RestTemplate()` has NO connect or read timeouts. Ensure all instances are built via `RestTemplateBuilder` with explicit timeouts.
3. **Migrate New Features to `RestClient`**: Implement all new microservice integrations using `RestClient` or HTTP Interfaces (`@HttpExchange`).
4. **Use `ParameterizedTypeReference` for Generic Collections**: In legacy code, remember that `getForObject(url, List.class)` causes type erasure. Use `exchange(..., new ParameterizedTypeReference<List<MyDto>>() {})`.
5. **Always Pool Connections in Production**: Default `SimpleClientHttpRequestFactory` opens a new TCP handshake on every HTTP request. Always back `RestTemplate` with Apache HttpClient 5 or JDK HttpClient.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Calling `new RestTemplate()` without Timeouts** | Default factory has infinite timeouts; a hanging remote server freezes worker threads permanently. | Use `RestTemplateBuilder.setConnectTimeout(...)`. |
| **Type Erasure on `List<T>` Responses** | `restTemplate.getForObject(url, List.class)` returns `List<LinkedHashMap>` instead of `List<UserDto>`. | Use `ParameterizedTypeReference<List<UserDto>>`. |
| **Panic Refactoring of Stable Code** | Rushing to replace working `RestTemplate` code creates unnecessary regression risk. | Adopt the adapter pattern: bridge via `RestClient.create(restTemplate)`. |
| **Missing Connection Pooling** | Default `SimpleClientHttpRequestFactory` suffers port exhaustion under load. | Configure `HttpComponentsClientHttpRequestFactory`. |
| **Ignoring URL Parameter Encoding** | Concatenating query parameters manually (`url + "?name=" + name`) causes encoding bugs. | Pass URI variables map or use `UriComponentsBuilder`. |
| **Swallowing Error Response Bodies in `ResponseErrorHandler`** | Reading `response.getBody()` in a custom error handler consumes the stream, preventing downstream logging. | Wrap response in a buffering factory or extract needed fields immediately. |

---

## 8. Performance & Security Considerations

### Ephemeral Port Exhaustion
Default `RestTemplate` creates a new TCP socket per request. Under high traffic (1000 req/sec), the host runs out of ephemeral TCP ports (`TIME_WAIT` socket buildup). Always use a connection pooling request factory (`HttpComponentsClientHttpRequestFactory`).

### Server-Side Request Forgery (SSRF)
Never accept target URLs from untrusted user inputs. Always validate endpoints against an allowed host whitelist before executing `restTemplate.exchange()`.

---

## 9. Comparison: RestTemplate vs RestClient Side-by-Side

| Feature | `RestTemplate` | `RestClient` |
| :--- | :--- | :--- |
| **Status** | Maintenance Mode (Since Spring 5.0) | Active Production Focus (Spring 6.1+) |
| **API Style** | Overloaded template methods (`exchange`, `execute`) | Fluent, functional builder pattern |
| **Readability** | Low (Parameter-heavy, verbose) | High (Self-documenting chained methods) |
| **Error Handling** | Global `ResponseErrorHandler` | Declarative per-call `.onStatus(predicate, handler)` |
| **Generic Collections** | Verbose `ParameterizedTypeReference` in `exchange()` | Clean `.body(new ParameterizedTypeReference<>() {})` |
| **Underlying Engine** | `ClientHttpRequestFactory` | `ClientHttpRequestFactory` |

---

## 10. Interview Questions & Deep Dives

<details>
<summary><strong>Q1: What does it mean that `RestTemplate` is in "maintenance mode"? Is it deprecated?</strong></summary>

**Answer:**
`RestTemplate` is **not** deprecated (`@Deprecated` is not present on the class). Maintenance mode means that the Spring team will continue to support bug fixes, critical security patches, and minor compatibility adjustments, but will not add major new features or API enhancements. Spring recommends using `RestClient` for new synchronous development.
</details>

<details>
<summary><strong>Q2: How does `RestClient.create(restTemplate)` facilitate a zero-downtime, safe migration?</strong></summary>

**Answer:**
It acts as an adapter. It extracts the `ClientHttpRequestFactory`, `ClientHttpRequestInterceptor` list, and `HttpMessageConverter` list directly from the existing `RestTemplate` instance and assigns them to the new `RestClient`. This guarantees that existing connection pool sizes, SSL/TLS certificates, authentication headers, and Jackson modules behave identically without rewriting configuration classes.
</details>

<details>
<summary><strong>Q3: Why does `restTemplate.getForObject(url, List.class)` return a list of `LinkedHashMap` instead of the expected domain objects?</strong></summary>

**Answer:**
Due to Java runtime Type Erasure, `List.class` carries no generic type information. Jackson has no way of knowing what type of objects to instantiate inside the list, so it falls back to deserializing JSON objects into `LinkedHashMap`. To resolve this, you must pass a `ParameterizedTypeReference<List<MyDto>>` via `exchange()`, which preserves generic type metadata via an anonymous subclass.
</details>

<details>
<summary><strong>Q4: What is the default `ClientHttpRequestFactory` used by `RestTemplate` if none is configured?</strong></summary>

**Answer:**
`SimpleClientHttpRequestFactory`, which uses the standard JDK `HttpURLConnection`. It does not support connection pooling, opens a new TCP connection on every request, and does not support HTTP/2.
</details>

<details>
<summary><strong>Q5: How do you intercept and log requests in `RestTemplate`?</strong></summary>

**Answer:**
Implement `ClientHttpRequestInterceptor` and register it with `restTemplate.setInterceptors(List.of(myInterceptor))`, or pass it to `RestTemplateBuilder.additionalInterceptors(...)`.
</details>

<details>
<summary><strong>Q6: How does `RestTemplate` handle 4xx and 5xx responses by default?</strong></summary>

**Answer:**
It delegates to `DefaultResponseErrorHandler`, which throws `HttpClientErrorException` on 4xx statuses and `HttpServerErrorException` on 5xx statuses.
</details>

<details>
<summary><strong>Q7: Can `RestTemplate` take advantage of Java 21 Virtual Threads?</strong></summary>

**Answer:**
Yes. Because `RestTemplate` is blocking and synchronous, executing it inside a task managed by `Executors.newVirtualThreadPerTaskExecutor()` unmounts the virtual thread during blocking socket I/O without carrier thread starvation.
</details>

<details>
<summary><strong>Q8: What is the difference between `getForObject()` and `getForEntity()`?</strong></summary>

**Answer:**
- `getForObject()`: Directly deserializes and returns the response body object (e.g. `UserDto`), discarding response headers and status codes.
- `getForEntity()`: Returns a `ResponseEntity<T>` containing the deserialized body, HTTP headers, and the `HttpStatusCode`.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Create RestTemplate with Timeouts via Builder</strong></summary>

**Task**: Write a `@Bean` method building a `RestTemplate` with a 3-second connect timeout and a 5-second read timeout.

```java
// Solution:
@Bean
public RestTemplate timeoutRestTemplate(RestTemplateBuilder builder) {
    return builder
            .setConnectTimeout(Duration.ofSeconds(3))
            .setReadTimeout(Duration.ofSeconds(5))
            .build();
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Wrap RestTemplate into RestClient</strong></summary>

**Task**: Given an existing `RestTemplate` bean, declare a `RestClient` bean that wraps it.

```java
// Solution:
@Bean
public RestClient wrappedRestClient(RestTemplate restTemplate) {
    return RestClient.create(restTemplate);
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Migrate `exchange()` with Generics to `RestClient`</strong></summary>

**Task**: Migrate this legacy call:
`restTemplate.exchange("/items", HttpMethod.GET, null, new ParameterizedTypeReference<List<ItemDto>>() {}).getBody()`
to modern `RestClient`.

```java
// Solution:
List<ItemDto> items = restClient.get()
        .uri("/items")
        .retrieve()
        .body(new ParameterizedTypeReference<List<ItemDto>>() {});
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Custom Error Handler Ignoring 404</strong></summary>

**Task**: Configure a `RestTemplate` with an error handler that returns `null` on HTTP 404 instead of throwing `HttpClientErrorException.NotFound`.

```java
// Solution:
@Bean
public RestTemplate safeNotFoundRestTemplate(RestTemplateBuilder builder) {
    return builder
            .errorHandler(new DefaultResponseErrorHandler() {
                @Override
                public boolean hasError(ClientHttpResponse response) throws IOException {
                    return response.getStatusCode().value() != 404 && super.hasError(response);
                }
            })
            .build();
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Phased Migration Strategy Harness</strong></summary>

**Task**: Implement a feature-flagged service wrapper that dispatches 90% of requests to legacy `RestTemplate` and 10% to migrated `RestClient`, comparing execution latencies and payload equality to verify zero regressions.
</details>

<details>
<summary><strong>Exercise 6 (Hard): Custom Connection Pool Metrics Exporter</strong></summary>

**Task**: Programmatically extract the active/idle lease metrics from Apache HttpClient's `PoolingHttpClientConnectionManager` backing a `RestTemplate` and expose them as a Micrometer `Gauge`.
</details>

---

## 12. Summary & Quick Reference

- **Status**: Maintenance mode (supported, no new features).
- **Modern Successor**: `RestClient` (introduced in Spring Boot 3.2).
- **Instant Adapter**: `RestClient.create(restTemplate)` shares existing connection pools and interceptors.
- **Safety Rule**: Never use default `new RestTemplate()` without timeouts.
- **Generics**: Use `ParameterizedTypeReference` to prevent type erasure on collections.

---

## 13. Further Reading

- [Spring Framework Reference: RestTemplate](https://docs.spring.io/spring-framework/reference/integration/rest-clients.html#rest-resttemplate)
- [Spring Framework Reference: RestClient](https://docs.spring.io/spring-framework/reference/integration/rest-clients.html#rest-restclient)
- [Spring Blog: Upgrading from RestTemplate to RestClient](https://spring.io/blog/2023/07/13/new-in-spring-6-1-restclient)
- [Apache HttpComponents 5 Connection Pooling Guide](https://hc.apache.org/httpcomponents-client-5.3.x/)
