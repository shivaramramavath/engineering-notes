# HTTP Interfaces & Declarative Clients in Spring Boot 3+

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | Spring MVC, RestClient, Dynamic Proxies, Java Annotations |
| **Estimated Reading Time** | 22 minutes |
| **Status** | Production-Ready |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Baseline** | OpenJDK 21 LTS / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | OpenJDK 21.0.2, Spring 6.2 / Boot 3.4.0 |

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
9. [Comparison: HTTP Interfaces vs OpenFeign vs Retrofit](#9-comparison-http-interfaces-vs-openfeign-vs-retrofit)
10. [Interview Questions & Deep Dives](#10-interview-questions--deep-dives)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference](#12-summary--quick-reference)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

Historically, when building microservice architectures in Spring, developers had to choose between writing repetitive, boilerplate HTTP client wrapper code (`RestClient.get().uri(...)...`) or adopting external declarative client libraries like **Spring Cloud OpenFeign** or Square's **Retrofit**.

### The Problem with Third-Party Declarative Clients
- **Spring Cloud OpenFeign**: Brings in massive Spring Cloud dependencies, relies on legacy reflection patterns, lacks native Virtual Threads integration, and requires heavy auto-configuration.
- **Manual Boilerplate**: Manually writing client classes wrapping `RestClient` or `WebClient` for 50+ remote endpoints leads to tedious, repetitive boilerplate.

### The Solution: Native Spring HTTP Interfaces (`@HttpExchange`)
Spring Framework 6 and Spring Boot 3 introduced **HTTP Interfaces**. You define a standard Java interface annotated with `@HttpExchange`, `@GetExchange`, `@PostExchange`, `@PutExchange`, and `@DeleteExchange`. At runtime, Spring's `HttpServiceProxyFactory` generates a dynamic proxy implementing that interface, backed by `RestClient` (synchronous) or `WebClient` (reactive). No external libraries or Spring Cloud starters are required.

---

## 2. Core Concepts

### Mental Model & Proxy Architecture
```
        You write a clean Java Interface:
        public interface CustomerClient {
            @GetExchange("/customers/{id}")
            CustomerDto getCustomer(@PathVariable UUID id);
        }
                          │
                          ▼
        HttpServiceProxyFactory.builderFor(RestClientAdapter.create(restClient))
                               .build()
                               .createClient(CustomerClient.class)
                          │
                          ▼
            JDK Dynamic Proxy Generated ($Proxy42)
                          │
       ┌──────────────────┴──────────────────┐
       ▼                                     ▼
Caller invokes client.getCustomer(id)   Method invocation intercepted
                                        Resolves path, params, headers
                                                     │
                                                     ▼
                                        Delegates to RestClient / WebClient
                                                     │
                                                     ▼
                                        Raw HTTP Request over Wire
```

### Invariants
1. **Underlying Adapter Flexibility**: The exact same interface can be backed by `RestClientAdapter` (for synchronous blocking/virtual-thread execution) or `WebClientAdapter` (for reactive streaming execution).
2. **Type-Safe Contract**: Method parameters (`@PathVariable`, `@RequestParam`, `@RequestBody`, `@RequestHeader`) map directly to HTTP request components.
3. **No Spring Cloud Starter Needed**: HTTP Interfaces are part of core `spring-web`.

---

## 3. How It Works Internally

### The `HttpServiceProxyFactory` Pipeline
1. `HttpServiceProxyFactory` uses standard JDK reflection (`java.lang.reflect.Proxy.newProxyInstance()`) to create a proxy implementing your client interface.
2. An `InvocationHandler` maps each method call to an `HttpServiceMethod`:
   - Inspects `@GetExchange`, `@PostExchange`, `@PutExchange`, `@DeleteExchange`, or `@PatchExchange`.
   - Resolves URI templates using `UriBuilderFactory`.
   - Binds method parameters using registered argument resolvers (`PathVariableArgumentResolver`, `RequestParamArgumentResolver`, `RequestBodyArgumentResolver`).
3. Executes the call through the configured `HttpClientAdapter` (`RestClientAdapter` or `WebClientAdapter`).
4. Deserializes the return type (Java Record, `ResponseEntity<T>`, `Optional<T>`, `List<T>`, or `void`).

---

## 4. Syntax & Configuration

### Available Exchange Annotations

| Annotation | HTTP Method | Example |
| :--- | :--- | :--- |
| `@HttpExchange` | Generic / Base path | `@HttpExchange("/api/v1/orders")` |
| `@GetExchange` | GET | `@GetExchange("/{id}")` |
| `@PostExchange` | POST | `@PostExchange` |
| `@PutExchange` | PUT | `@PutExchange("/{id}")` |
| `@DeleteExchange`| DELETE | `@DeleteExchange("/{id}")` |
| `@PatchExchange` | PATCH | `@PatchExchange("/{id}")` |

---

## 5. Practical Examples

### Example 1: Declarative Synchronous HTTP Client with `RestClient`

```java
package com.example.clients.declarative;

import org.springframework.web.bind.annotation.PathVariable;
import org.springframework.web.bind.annotation.RequestParam;
import org.springframework.web.service.annotation.DeleteExchange;
import org.springframework.web.service.annotation.GetExchange;
import org.springframework.web.service.annotation.HttpExchange;
import org.springframework.web.service.annotation.PostExchange;

import java.math.BigDecimal;
import java.util.List;
import java.util.UUID;

public record ProductDto(UUID id, String name, BigDecimal price) {}
public record CreateProductRequest(String name, BigDecimal price) {}

@HttpExchange("/api/v1/products")
public interface ProductClient {

    @GetExchange
    List<ProductDto> listProducts(@RequestParam(defaultValue = "0") int page,
                                 @RequestParam(defaultValue = "20") int size);

    @GetExchange("/{id}")
    ProductDto getProductById(@PathVariable UUID id);

    @PostExchange
    ProductDto createProduct(CreateProductRequest request);

    @DeleteExchange("/{id}")
    void deleteProduct(@PathVariable UUID id);
}
```

### Example 2: Bean Configuration Factory Setup (Spring Boot 3.4 Style)

```java
package com.example.clients.config;

import com.example.clients.declarative.ProductClient;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.web.client.RestClient;
import org.springframework.web.client.support.RestClientAdapter;
import org.springframework.web.service.invoker.HttpServiceProxyFactory;

@Configuration(proxyBeanMethods = false)
public class DeclarativeClientConfig {

    @Bean
    public ProductClient productClient(RestClient.Builder builder) {
        RestClient restClient = builder
                .baseUrl("https://catalog-service.internal")
                .defaultHeader("X-Client-App", "OrderService")
                .build();

        // Build the dynamic proxy factory backed by RestClient
        HttpServiceProxyFactory factory = HttpServiceProxyFactory
                .builderFor(RestClientAdapter.create(restClient))
                .build();

        return factory.createClient(ProductClient.class);
    }
}
```

### Example 3: Edge Case — Dynamic Header Injection and ResponseEntity Inspection

```java
package com.example.clients.declarative;

import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.PathVariable;
import org.springframework.web.bind.annotation.RequestHeader;
import org.springframework.web.service.annotation.GetExchange;
import org.springframework.web.service.annotation.HttpExchange;

import java.util.UUID;

@HttpExchange("/api/v1/secure-vault")
public interface SecureVaultClient {

    /**
     * Supports returning ResponseEntity to inspect status codes and headers directly.
     */
    @GetExchange("/secrets/{key}")
    ResponseEntity<String> getSecret(
            @PathVariable String key,
            @RequestHeader("X-Vault-Token") String vaultToken,
            @RequestHeader(name = "X-Correlation-ID", required = false) String correlationId);
}
```

### Example 4: Anti-Pattern vs Production Fix

#### Anti-Pattern: Using Heavy Spring Cloud OpenFeign for Basic HTTP Calls
```xml
<!-- ANTI-PATTERN: Pulling in Spring Cloud OpenFeign just to make simple HTTP calls -->
<dependency>
    <groupId>org.springframework.cloud</groupId>
    <artifactId>spring-cloud-starter-openfeign</artifactId>
    <!-- Heavy dependency tree, slower startup, complex configuration -->
</dependency>
```

#### Production Fix: Use Native Spring HTTP Interfaces
```java
// PRODUCTION FIX: Core Spring 6 @HttpExchange interface backed by RestClient
@HttpExchange("/users")
public interface UserClient {
    @GetExchange("/{id}")
    UserDto getUser(@PathVariable Long id);
}
// Zero extra dependencies required beyond spring-boot-starter-web!
```

### Example 5: High-Performance Virtual Thread Concurrency with Declarative Clients

```java
package com.example.clients.service;

import com.example.clients.declarative.ProductClient;
import com.example.clients.declarative.ProductDto;
import org.springframework.stereotype.Service;

import java.util.List;
import java.util.UUID;
import java.util.concurrent.Executors;

@Service
public class BulkCatalogFetcher {

    private final ProductClient productClient;

    public BulkCatalogFetcher(ProductClient productClient) {
        this.productClient = productClient;
    }

    public List<ProductDto> fetchMultipleProductsInParallel(List<UUID> productIds) {
        // Leverages Java 21 Virtual Threads to execute declarative calls in parallel!
        try (var executor = Executors.newVirtualThreadPerTaskExecutor()) {
            var futures = productIds.stream()
                    .map(id -> executor.submit(() -> productClient.getProductById(id)))
                    .toList();

            return futures.stream()
                    .map(java.util.concurrent.Future::join)
                    .toList();
        }
    }
}
```

---

## 6. Best Practices

1. **Back with `RestClient` for Blocking Apps**: Use `RestClientAdapter.create(restClient)` for standard MVC microservices. It works natively with Java 21 Virtual Threads.
2. **Back with `WebClient` for Reactive Apps**: Use `WebClientAdapter.create(webClient)` only if the interface methods return `Mono<T>` or `Flux<T>`.
3. **Configure Timeouts on the Underlying Client**: Configure connect and read timeouts on the `RestClient` or `WebClient` builder before passing it to `HttpServiceProxyFactory`.
4. **Use Java Records for DTOs**: Pair HTTP interfaces with immutable Java records for request and response payloads.
5. **Decouple Interfaces from Implementation**: Treat HTTP client interfaces as API contracts. Store them in client-shared library JARs if multiple microservices consume the same service.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Using `@RequestMapping` Instead of `@HttpExchange`** | Spring MVC's `@RequestMapping` is for server-side routing; HTTP interfaces require `@HttpExchange`. | Use `@HttpExchange`, `@GetExchange`, `@PostExchange`. |
| **Missing Parameter Annotations** | Methods like `getUser(Long id)` fail to bind unless annotated with `@PathVariable` or `@RequestParam`. | Always annotate parameters explicitly with `@PathVariable`, `@RequestParam`, or `@RequestBody`. |
| **Forgetting to Build Proxy Bean** | Defining the interface without registering a proxy bean in `@Configuration` causes `NoSuchBeanDefinitionException`. | Register a bean using `factory.createClient(MyClient.class)`. |
| **Calling Reactive Methods with `RestClientAdapter`** | If an interface method returns `Mono<T>`, backing it with `RestClientAdapter` throws `IllegalArgumentException`. | Use `WebClientAdapter` for reactive return types. |
| **Omitting Root Path on Class-Level `@HttpExchange`** | Results in missing base paths or invalid concatenated URLs. | Define base paths cleanly (e.g. `@HttpExchange("/api/v1")`). |
| **Attempting to Autowire Unregistered Interface** | HTTP interfaces are NOT Spring components by default; they require manual factory registration. | Define `@Bean` methods in configuration or use custom registrar. |

---

## 8. Performance & Security Considerations

### Reflection Overhead of JDK Dynamic Proxies
JDK dynamic proxies introduce minimal invocation overhead (nanoseconds per call), which is completely dwarfed by network socket I/O (milliseconds). Caching method metadata during `HttpServiceProxyFactory` initialization ensures runtime execution is near-instantaneous.

### URL Injection & SSRF Protection
Never bind full remote URLs dynamically from untrusted user inputs. The `@HttpExchange` base URL should be fixed in configuration, and path variables should be strictly typed (e.g. `UUID` or `Long`) to prevent path traversal or SSRF attacks.

---

## 9. Comparison: HTTP Interfaces vs OpenFeign vs Retrofit

| Feature | Spring HTTP Interfaces | Spring Cloud OpenFeign | Square Retrofit |
| :--- | :--- | :--- | :--- |
| **Origin Framework** | Core Spring Framework 6+ | Spring Cloud | Square / Android |
| **Additional Starters**| Zero (Built into `spring-web`) | `spring-cloud-starter-openfeign` | `retrofit2` + Jackson converters |
| **Underlying Engine** | `RestClient` or `WebClient` | Ribbon / Apache HttpClient / OkHttp | OkHttp |
| **Virtual Threads** | Native First-Class Support | Requires custom executor tuning | Requires custom thread pool |
| **Reactive Return Types**| Supported via `WebClientAdapter` | Poor / Broken | Requires RxJava adapter |
| **Maintenance & Future**| Primary official Spring strategy | Legacy / Maintenance mode | Third-party Android focus |

---

## 10. Interview Questions & Deep Dives

<details>
<summary><strong>Q1: How do Spring 6 HTTP Interfaces differ from Spring Cloud OpenFeign?</strong></summary>

**Answer:**
Spring HTTP Interfaces are built directly into core `spring-web` (Spring Framework 6 / Spring Boot 3), requiring zero Spring Cloud dependencies. They use standard `@HttpExchange` annotations and decouple the declaration from the underlying HTTP client engine via adapters (`RestClientAdapter` or `WebClientAdapter`). In contrast, OpenFeign is an external Netflix/OpenFeign port requiring Spring Cloud infrastructure, has heavier startup overhead, and lacks native integration with modern Spring 6 `RestClient`.
</details>

<details>
<summary><strong>Q2: How does `HttpServiceProxyFactory` instantiate an HTTP interface at runtime?</strong></summary>

**Answer:**
It uses Java's built-in `java.lang.reflect.Proxy.newProxyInstance()`. When a method on the interface is invoked, the proxy's `InvocationHandler` inspects the method's exchange annotations, resolves URL templates and path variables, formats the request body, and delegates execution to the underlying `HttpClientAdapter` (`RestClientAdapter` or `WebClientAdapter`).
</details>

<details>
<summary><strong>Q3: Can an HTTP interface return `ResponseEntity<T>`?</strong></summary>

**Answer:**
Yes. If an interface method declares `ResponseEntity<T>` as its return type, Spring populates the entity with the HTTP status code, response headers, and deserialized body. If the return type is `void`, Spring executes the call and discards the response body.
</details>

<details>
<summary><strong>Q4: How do you configure timeouts on an HTTP interface client?</strong></summary>

**Answer:**
Timeouts cannot be set on the `@HttpExchange` annotations directly. Instead, you configure connect and read timeouts on the underlying `RestClient` or `WebClient` instance passed into the `RestClientAdapter` or `WebClientAdapter` during proxy factory creation.
</details>

<details>
<summary><strong>Q5: What happens if an HTTP interface call receives a 404 or 500 error from the remote server?</strong></summary>

**Answer:**
When backed by `RestClientAdapter`, the underlying `RestClient` executes its configured status handlers. By default, 4xx responses throw `HttpClientErrorException` and 5xx responses throw `HttpServerErrorException`. You can customize error handling by configuring `.defaultStatusHandler()` on the `RestClient.Builder`.
</details>

<details>
<summary><strong>Q6: Can you pass custom HTTP headers dynamically to a method in an HTTP interface?</strong></summary>

**Answer:**
Yes. You can annotate method parameters with `@RequestHeader("Header-Name") String value` or pass a complete `@RequestHeader HttpHeaders headers` map to inject dynamic headers at call time.
</details>

<details>
<summary><strong>Q7: How do HTTP interfaces support file uploads?</strong></summary>

**Answer:**
You can declare a parameter `@RequestPart("file") MultipartFile file` or `Resource file` with a `@PostExchange(contentType = MediaType.MULTIPART_FORM_DATA_VALUE)` annotation. Spring will serialize the request as `multipart/form-data`.
</details>

<details>
<summary><strong>Q8: What is the purpose of `RestClientAdapter` vs `WebClientAdapter`?</strong></summary>

**Answer:**
- `RestClientAdapter`: Backs the interface with synchronous, blocking execution via `RestClient` (ideal for standard Spring MVC apps and Java 21 Virtual Threads).
- `WebClientAdapter`: Backs the interface with non-blocking, reactive execution via `WebClient`, supporting methods returning `Mono<T>` or `Flux<T>`.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Declare Basic HTTP Interface</strong></summary>

**Task**: Define an interface `WeatherClient` with a method fetching current weather from `GET /weather?city={city}` returning a `WeatherDto`.

```java
// Solution:
@HttpExchange("/weather")
public interface WeatherClient {
    @GetExchange
    WeatherDto getCurrentWeather(@RequestParam("city") String city);
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Register WeatherClient Proxy Bean</strong></summary>

**Task**: Write the `@Configuration` bean definition instantiating `WeatherClient` backed by `RestClient`.

```java
// Solution:
@Bean
public WeatherClient weatherClient(RestClient.Builder builder) {
    RestClient restClient = builder.baseUrl("https://api.weather.com").build();
    HttpServiceProxyFactory factory = HttpServiceProxyFactory
            .builderFor(RestClientAdapter.create(restClient))
            .build();
    return factory.createClient(WeatherClient.class);
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): POST Request with RequestBody and Headers</strong></summary>

**Task**: Declare an interface method for creating an alert via `POST /alerts`, passing an `AlertRequest` body and an `X-Priority` header, returning HTTP 201 `ResponseEntity<Void>`.

```java
// Solution:
@HttpExchange("/alerts")
public interface AlertClient {
    @PostExchange
    ResponseEntity<Void> triggerAlert(
            @RequestHeader("X-Priority") String priority,
            @RequestBody AlertRequest request);
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Custom Error Handler on Declarative Client</strong></summary>

**Task**: Configure the underlying `RestClient.Builder` of a declarative client to throw a domain `ResourceNotFoundException` whenever any interface method encounters HTTP 404.

```java
// Solution:
@Bean
public OrderClient orderClient(RestClient.Builder builder) {
    RestClient restClient = builder
            .baseUrl("https://orders.internal")
            .defaultStatusHandler(
                    status -> status.value() == 404,
                    (req, res) -> { throw new ResourceNotFoundException("Remote entity not found"); }
            )
            .build();
    return HttpServiceProxyFactory.builderFor(RestClientAdapter.create(restClient))
            .build()
            .createClient(OrderClient.class);
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Generic Client Registrar BeanPostProcessor</strong></summary>

**Task**: Build a custom `ImportBeanDefinitionRegistrar` or scanner that automatically discovers all interfaces annotated with `@HttpExchange` in a package and registers dynamic proxy beans into the `ApplicationContext` without writing manual `@Bean` factory methods.
</details>

<details>
<summary><strong>Exercise 6 (Hard): Reactive HTTP Interface with WebClientAdapter</strong></summary>

**Task**: Implement an interface `RealtimeStockFeedClient` with a method returning `Flux<StockTick>` from `@GetExchange(value = "/stream", accept = "text/event-stream")`, configure it with `WebClientAdapter`, and write a test subscribing to the stream.
</details>

---

## 12. Summary & Quick Reference

- **Core Annotations**: `@HttpExchange`, `@GetExchange`, `@PostExchange`, `@PutExchange`, `@DeleteExchange`.
- **Factory Class**: `HttpServiceProxyFactory`.
- **Synchronous Adapter**: `RestClientAdapter.create(restClient)`.
- **Reactive Adapter**: `WebClientAdapter.create(webClient)`.
- **Zero Third-Party Dependencies**: Native to Spring Boot 3 / Spring Framework 6.

---

## 13. Further Reading

- [Spring Framework Reference: HTTP Interfaces](https://docs.spring.io/spring-framework/reference/integration/rest-clients.html#rest-http-interface)
- [Spring Blog: HTTP Interfaces in Spring 6](https://spring.io/blog/2022/10/12/new-in-spring-6-http-interfaces)
- [Java Dynamic Proxies API Specification](https://docs.oracle.com/en/java/javase/21/docs/api/java.base/java/lang/reflect/Proxy.html)
- [Replacing Spring Cloud OpenFeign with Spring 6 HTTP Interfaces](https://spring.io/guides)
