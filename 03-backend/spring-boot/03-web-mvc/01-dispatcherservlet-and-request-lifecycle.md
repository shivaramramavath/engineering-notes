# DispatcherServlet & Request Lifecycle in Spring MVC

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | Java Servlet 6.0, Spring IoC Container, HTTP Protocols |
| **Estimated Reading Time** | 25 minutes |
| **Status** | Production-Ready |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Baseline** | OpenJDK 21 LTS / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | Tomcat 10.1, Jakarta Servlet 6.0, OpenJDK 21.0.2 |

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
9. [Comparison: DispatcherServlet vs WebFlux HttpHandler](#9-comparison-dispatcherservlet-vs-webflux-httphandler)
10. [Interview Questions & Deep Dives](#10-interview-questions--deep-dives)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference](#12-summary--quick-reference)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

In traditional Java EE applications, each URL typically mapped directly to an individual Servlet class or JAX-RS resource registered in `web.xml`. This led to massive duplication of cross-cutting logic like authentication, parameter parsing, error formatting, view resolution, and locale resolution.

### The Problem
Without a centralized routing gateway:
1. Every endpoint must implement boilerplate HTTP method parsing, encoding detection, and exception catching.
2. Intercepting or transforming requests consistently before controller entry requires chaining cumbersome raw servlet filters.
3. Content negotiation and exception-to-HTTP mapping are fragmented across the codebase.

### The Solution: The Front Controller Pattern
Spring Web MVC implements the **Front Controller** pattern via `DispatcherServlet`. A single central servlet receives every incoming HTTP request, orchestrates parameter resolution, maps URLs to handler methods via `HandlerMapping`, invokes interceptors, delegates execution to `HandlerAdapter`, resolves exceptions through `HandlerExceptionResolver`, and coordinates response rendering.

---

## 2. Core Concepts

### Mental Model & Architectural Workflow
```
                       Incoming HTTP Request (TCP / Port 8080)
                                      │
                                      ▼
                      Servlet Filter Chain (Security, Tracing)
                                      │
                                      ▼
                           DispatcherServlet.doDispatch()
                                      │
                   ┌──────────────────┴──────────────────┐
                   ▼                                     ▼
        1. HandlerMapping.getHandler()         2. HandlerExecutionChain
        Finds target Controller method          Pre-Handle Interceptors
                   │                                     │
                   └──────────────────┬──────────────────┘
                                      ▼
                         3. HandlerAdapter.handle()
                       (RequestMappingHandlerAdapter)
                                      │
                   ┌──────────────────┼──────────────────┐
                   ▼                  ▼                  ▼
          Argument Resolvers    Controller Method   ReturnValue Handlers
       (@RequestBody, @PathVariable) (Your Business Logic) (@ResponseBody, ResponseEntity)
                                      │
                                      ▼
                         4. HandlerExecutionChain
                        Post-Handle Interceptors
                                      │
                                      ▼
                      5. HandlerExceptionResolver (if error)
                     (ExceptionHandlerExceptionResolver)
                                      │
                                      ▼
                         6. HandlerExecutionChain
                        After-Completion Interceptors
                                      │
                                      ▼
                        HTTP Response Dispatched
```

### Invariants
1. **Thread-Bound Context**: By default in standard Servlet 6.0, each request is bound to a single worker thread (or virtual thread in Java 21+), with thread-local context maintained via `RequestContextHolder`.
2. **Single Entry Point**: In Spring Boot Web, `DispatcherServlet` is mapped to root `/` with registration priority `1`.
3. **Execution Chain Integrity**: If an interceptor's `preHandle()` returns `false`, the execution chain halts immediately, and subsequent interceptors or controllers are bypassed.

---

## 3. How It Works Internally

### The Core `doDispatch()` Method
Inside `org.springframework.web.servlet.DispatcherServlet`, the heart of Spring MVC executes:

```java
protected void doDispatch(HttpServletRequest request, HttpServletResponse response) throws Exception {
    HttpServletRequest processedRequest = request;
    HandlerExecutionChain mappedHandler = null;
    boolean multipartRequestParsed = false;
    WebAsyncManager asyncManager = WebAsyncUtils.getAsyncManager(request);

    try {
        ModelAndView mv = null;
        Exception dispatchException = null;

        try {
            // Step 1: Check and parse multipart (file upload) requests
            processedRequest = checkMultipart(request);
            multipartRequestParsed = (processedRequest != request);

            // Step 2: Determine handler for current request
            mappedHandler = getHandler(processedRequest);
            if (mappedHandler == null) {
                noHandlerFound(processedRequest, response);
                return;
            }

            // Step 3: Determine handler adapter for target handler
            HandlerAdapter ha = getHandlerAdapter(mappedHandler.getHandler());

            // Step 4: Apply preHandle() on registered interceptors
            if (!mappedHandler.applyPreHandle(processedRequest, response)) {
                return; // Chain interrupted
            }

            // Step 5: Actually invoke the controller handler method
            mv = ha.handle(processedRequest, response, mappedHandler.getHandler());

            if (asyncManager.isConcurrentHandlingStarted()) {
                return; // Handled asynchronously (SSE, DeferredResult)
            }

            applyDefaultViewName(processedRequest, mv);

            // Step 6: Apply postHandle() on interceptors
            mappedHandler.applyPostHandle(processedRequest, response, mv);
        } catch (Exception ex) {
            dispatchException = ex;
        } catch (Throwable err) {
            dispatchException = new ServletException("Handler dispatch failed", err);
        }

        // Step 7: Process result or exception through HandlerExceptionResolvers
        processDispatchResult(processedRequest, response, mappedHandler, mv, dispatchException);
    } finally {
        if (asyncManager.isConcurrentHandlingStarted()) {
            if (mappedHandler != null) {
                mappedHandler.applyAfterConcurrentHandlingStarted(processedRequest, response);
            }
        } else {
            // Clean up multipart resources if applicable
            if (multipartRequestParsed) {
                cleanupMultipart(processedRequest);
            }
        }
    }
}
```

---

## 4. Syntax & Configuration

### Customizing `DispatcherServlet` in `application.yaml`
```yaml
spring:
  mvc:
    servlet:
      load-on-startup: 1 # Initializes servlet at container startup rather than on first request
      path: /api/v1 # Remaps DispatcherServlet root (optional)
    throw-exception-if-no-handler-found: true
  web:
    resources:
      add-mappings: false # Disable default static resource mappings if building pure REST APIs
```

### Programmatic Registration via `DispatcherServletRegistrationBean`
```java
package com.example.web.config;

import org.springframework.boot.autoconfigure.web.servlet.DispatcherServletRegistrationBean;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.web.servlet.DispatcherServlet;

@Configuration(proxyBeanMethods = false)
public class DispatcherServletConfig {

    @Bean
    public DispatcherServletRegistrationBean dispatcherServletRegistration(DispatcherServlet dispatcherServlet) {
        DispatcherServletRegistrationBean registration = new DispatcherServletRegistrationBean(dispatcherServlet, "/");
        registration.setName("customDispatcherServlet");
        registration.setLoadOnStartup(1);
        return registration;
    }
}
```

---

## 5. Practical Examples

### Example 1: Basic Custom Handler Interceptor with Execution Timing

```java
package com.example.web.interceptor;

import jakarta.servlet.http.HttpServletRequest;
import jakarta.servlet.http.HttpServletResponse;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Component;
import org.springframework.web.servlet.HandlerInterceptor;
import org.springframework.web.servlet.ModelAndView;

@Component
public class RequestPerformanceInterceptor implements HandlerInterceptor {

    private static final Logger log = LoggerFactory.getLogger(RequestPerformanceInterceptor.class);
    private static final String START_TIME_ATTR = "request_start_timestamp_ns";

    @Override
    public boolean preHandle(HttpServletRequest request, HttpServletResponse response, Object handler) {
        request.setAttribute(START_TIME_ATTR, System.nanoTime());
        return true; // Continue execution chain
    }

    @Override
    public void postHandle(HttpServletRequest request, HttpServletResponse response, Object handler, ModelAndView modelAndView) {
        // Invoked after handler method returns, before view rendering
    }

    @Override
    public void afterCompletion(HttpServletRequest request, HttpServletResponse response, Object handler, Exception ex) {
        Long startTime = (Long) request.getAttribute(START_TIME_ATTR);
        if (startTime != null) {
            long durationMs = (System.nanoTime() - startTime) / 1_000_000;
            log.info("URI: {} | Method: {} | Status: {} | Duration: {}ms",
                    request.getRequestURI(), request.getMethod(), response.getStatus(), durationMs);
        }
    }
}
```

### Example 2: Enterprise Custom Argument Resolver for Security Context

```java
package com.example.web.resolver;

import org.springframework.core.MethodParameter;
import org.springframework.stereotype.Component;
import org.springframework.web.bind.support.WebDataBinderFactory;
import org.springframework.web.context.request.NativeWebRequest;
import org.springframework.web.method.support.HandlerMethodArgumentResolver;
import org.springframework.web.method.support.ModelAndViewContainer;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

@Target(ElementType.PARAMETER)
@Retention(RetentionPolicy.RUNTIME)
public @interface CurrentTenant {
}

public record TenantContext(String tenantId, String region) {}

@Component
public class TenantContextArgumentResolver implements HandlerMethodArgumentResolver {

    @Override
    public boolean supportsParameter(MethodParameter parameter) {
        return parameter.hasParameterAnnotation(CurrentTenant.class) &&
               parameter.getParameterType().equals(TenantContext.class);
    }

    @Override
    public Object resolveArgument(MethodParameter parameter,
                                  ModelAndViewContainer mavContainer,
                                  NativeWebRequest webRequest,
                                  WebDataBinderFactory binderFactory) {
        String tenantId = webRequest.getHeader("X-Tenant-ID");
        if (tenantId == null || tenantId.isBlank()) {
            tenantId = "default-public";
        }
        String region = webRequest.getHeader("X-Region");
        return new TenantContext(tenantId, region != null ? region : "us-east-1");
    }
}
```

### Example 3: Edge Case — Handling `NoHandlerFoundException` for 404 REST Uniformity

By default, unmatched requests are forwarded to Tomcat's default servlet or the `/error` controller. To capture 404s inside standard `@ExceptionHandler` methods:

```yaml
# application.yaml
spring:
  mvc:
    throw-exception-if-no-handler-found: true
  web:
    resources:
      add-mappings: false # Crucial: Static resource mapping catch-all must be disabled
```

```java
package com.example.web.error;

import org.springframework.http.HttpStatus;
import org.springframework.http.ProblemDetail;
import org.springframework.web.bind.annotation.ExceptionHandler;
import org.springframework.web.bind.annotation.RestControllerAdvice;
import org.springframework.web.servlet.NoHandlerFoundException;

import java.net.URI;
import java.time.Instant;

@RestControllerAdvice
public class GlobalNotFoundExceptionHandler {

    @ExceptionHandler(NoHandlerFoundException.class)
    public ProblemDetail handleNoHandlerFound(NoHandlerFoundException ex) {
        ProblemDetail problem = ProblemDetail.forStatusAndDetail(
                HttpStatus.NOT_FOUND,
                "The requested resource endpoint does not exist: " + ex.getRequestURL()
        );
        problem.setTitle("Endpoint Not Found");
        problem.setType(URI.create("https://api.example.com/errors/not-found"));
        problem.setProperty("timestamp", Instant.now());
        problem.setProperty("httpMethod", ex.getHttpMethod());
        return problem;
    }
}
```

### Example 4: Anti-Pattern vs Production Fix

#### Anti-Pattern: Accessing Raw `HttpServletRequest` inside Business Services
```java
// ANTI-PATTERN: Leaking Servlet API into domain business logic!
@Service
public class OrderService {
    
    // Tightly couples domain code to Servlet HTTP runtime!
    // Impossible to test cleanly in unit tests without extensive mocking.
    public void processOrder(HttpServletRequest request) {
        String authHeader = request.getHeader("Authorization");
        // Business logic...
    }
}
```

#### Production Fix: Decoupling via Clean Value Objects and Argument Resolvers
```java
// PRODUCTION FIX: Controller extracts request details; domain remains pure POJO
@RestController
@RequestMapping("/orders")
public class OrderController {

    private final OrderService orderService;

    public OrderController(OrderService orderService) {
        this.orderService = orderService;
    }

    @PostMapping
    public ResponseEntity<OrderResponse> createOrder(@RequestHeader("Authorization") String token,
                                                     @RequestBody OrderRequest request) {
        // Pure domain call with standard Java record
        return ResponseEntity.ok(orderService.processOrder(token, request));
    }
}
```

### Example 5: High-Performance Virtual Thread Dispatcher Configuration (Java 21+)

```yaml
# application.yaml
spring:
  threads:
    virtual:
      enabled: true # Enables virtual threads for Tomcat worker threads
server:
  tomcat:
    threads:
      max: 200 # Ignored when virtual threads are enabled (unbounded virtual threads used)
```

```java
package com.example.web.config;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RestController;

@RestController
public class VirtualThreadProbeController {

    private static final Logger log = LoggerFactory.getLogger(VirtualThreadProbeController.class);

    @GetMapping("/api/thread-info")
    public String getThreadInfo() {
        Thread current = Thread.currentThread();
        log.info("Processing request on: isVirtual={}, name={}", current.isVirtual(), current.getName());
        return "Executed on VirtualThread=" + current.isVirtual() + " [" + current.getName() + "]";
    }
}
```

---

## 6. Best Practices

1. **Keep `load-on-startup = 1`**: Ensures `DispatcherServlet` initializes its handler mappings, adapters, and bean cache during application boot rather than causing latency on the very first user HTTP request.
2. **Favor Interceptors for HTTP Cross-Cutting Concerns**: Use `HandlerInterceptor` instead of raw `ServletFilter` when you need access to the target `HandlerMethod` (reflection metadata, annotations).
3. **Use Argument Resolvers for Parameter Injection**: Encapsulate repetitive header parsing, token decoding, or tenant resolution into custom `HandlerMethodArgumentResolver` beans.
4. **Never Block on `RequestContextHolder` Across Async Boundaries**: `RequestContextHolder` relies on `ThreadLocal`. If handing off work to custom thread pools, manually propagate context or use `InheritableThreadLocal`.
5. **Enable Virtual Threads**: In Java 21+, set `spring.threads.virtual.enabled=true` to handle high I/O concurrency without thread pool starvation.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Using `HandlerInterceptor` to Modify Request Body** | The HTTP input stream cannot be re-read after being consumed by an interceptor. | Use `RequestBodyAdvice` or a caching `ServletFilter` (`ContentCachingRequestWrapper`). |
| **Assuming Interceptor Executes on 404s** | If no handler method matches the URI, `getHandler()` returns null and controller interceptors are never invoked. | Use `Filter` for universal request inspection or enable `throw-exception-if-no-handler-found`. |
| **Mutating Interceptor Instance State** | Interceptors are singleton Spring beans shared across all incoming concurrent requests; instance variables cause race conditions. | Keep interceptor classes stateless; use `request.setAttribute()` for per-request state. |
| **Leaving `load-on-startup` at Default (-1)** | Causes the first HTTP request after deployment to suffer high cold-start latency. | Set `spring.mvc.servlet.load-on-startup=1`. |
| **Expecting `postHandle` to Run on Exceptions** | If the controller method throws an unhandled exception, `postHandle` is skipped completely. | Put cleanup logic in `afterCompletion()`, which is guaranteed to execute. |
| **Bypassing DispatcherServlet with Custom Servlets** | Direct `@WebServlet` registrations lack access to Spring MVC interceptors, content negotiation, and advice. | Integrate endpoints into standard `@RestController` architectures. |

---

## 8. Performance & Security Considerations

### Virtual Thread Pinning in Servlet Containers
In Spring Boot 3.2+ on Java 21+, virtual threads (`spring.threads.virtual.enabled=true`) assign a virtual thread per incoming HTTP socket connection. Avoid `synchronized` blocks inside database drivers or third-party libraries during blocking I/O to prevent carrier thread pinning. Use `ReentrantLock` instead.

### Request Smuggling & Path Normalization
`DispatcherServlet` utilizes `PathPatternParser` by default in modern Spring Boot, which strictly enforces URI RFC compliance, rejects ambiguous double-slashes (`//`), and prevents path traversal attacks before handler execution.

---

## 9. Comparison: DispatcherServlet vs WebFlux HttpHandler

| Feature | `DispatcherServlet` (Spring MVC) | `HttpHandler` / `DispatcherHandler` (WebFlux) |
| :--- | :--- | :--- |
| **Runtime Engine** | Servlet 6.0 Container (Tomcat, Jetty) | Reactive Server (Reactor Netty, Undertow) |
| **Concurrency Model** | Thread-per-request (or Virtual Threads) | Event-loop non-blocking (Reactor) |
| **Context Propagation** | `ThreadLocal` / `RequestContextHolder` | Reactor `ContextView` (Subscriber context) |
| **Backpressure Support** | No (blocking I/O) | Native Reactive Streams backpressure |
| **Best Used For** | Standard relational DB apps, JPA, CRUD | Low-latency streaming, SSE, high-throughput microservices |

---

## 10. Interview Questions & Deep Dives

<details>
<summary><strong>Q1: What is the exact sequence of lifecycle callbacks executed by `DispatcherServlet` during an HTTP request?</strong></summary>

**Answer:**
1. Incoming request enters `DispatcherServlet.doDispatch()`.
2. Multipart detection (`checkMultipart`).
3. `HandlerMapping.getHandler()` resolves the `HandlerExecutionChain` (containing interceptors and handler).
4. `HandlerAdapter` is resolved for the handler.
5. All registered interceptors have their `preHandle()` methods invoked in order.
6. Handler method is invoked (`HandlerAdapter.handle()`), resolving arguments and executing business code.
7. Interceptors have their `postHandle()` methods invoked in reverse order.
8. View or ResponseBody rendering occurs.
9. Interceptors have their `afterCompletion()` invoked, regardless of whether execution succeeded or failed.
</details>

<details>
<summary><strong>Q2: What is the architectural difference between a Servlet `Filter` and a Spring `HandlerInterceptor`?</strong></summary>

**Answer:**
- **Servlet Filter**: Defined by the Jakarta Servlet specification, runs outside `DispatcherServlet`. Operates at the raw byte/stream level. Cannot access Spring MVC specific constructs (like which `@RestController` or method is being called).
- **HandlerInterceptor**: Defined by Spring MVC, runs inside `DispatcherServlet`. Receives the `Object handler` parameter (which casts to `HandlerMethod`), giving full reflection access to controller method annotations, return types, and Spring context beans.
</details>

<details>
<summary><strong>Q3: How does Spring Boot 3 handle path pattern matching differently from older Spring Boot 2 versions?</strong></summary>

**Answer:**
Spring Boot 3 uses `PathPatternParser` by default instead of the older `AntPathMatcher`. `PathPatternParser` is pre-parsed, heavily optimized for modern URI path matching, provides 2-4x higher matching throughput, and has strict security defaults against URI ambiguity.
</details>

<details>
<summary><strong>Q4: Why does `RequestContextHolder` lose context inside asynchronous tasks spawned via `CompletableFuture`?</strong></summary>

**Answer:**
`RequestContextHolder` defaults to a standard `ThreadLocal` strategy. When work is submitted to a separate executor thread, the thread-local storage is not propagated. To propagate it, you must use a decorating task executor, pass context explicitly, or switch to `InheritableThreadLocal` (which still fails with reused pooled threads).
</details>

<details>
<summary><strong>Q5: What happens if an interceptor's `preHandle()` returns `false`?</strong></summary>

**Answer:**
If `preHandle()` returns `false`, `DispatcherServlet` halts the execution chain immediately. The controller method is **not** called. Any interceptors that had already executed `preHandle()` returning `true` will have their `afterCompletion()` methods triggered to clean up state, but no `postHandle()` will execute.
</details>

<details>
<summary><strong>Q6: How does `RequestMappingHandlerAdapter` resolve method arguments?</strong></summary>

**Answer:**
It iterates over a registered list of `HandlerMethodArgumentResolver` beans. For each method parameter, it calls `resolver.supportsParameter(param)`. The first resolver returning `true` executes `resolver.resolveArgument(...)` to transform HTTP headers, query parameters, path variables, or body streams into typed Java objects.
</details>

<details>
<summary><strong>Q7: Can you register multiple `DispatcherServlet` instances in a single Spring Boot application?</strong></summary>

**Answer:**
Yes. You can define multiple `DispatcherServletRegistrationBean` instances, mapping one servlet to `/api/*` and another to `/admin/*`, each with its own configuration, interceptors, and filters if needed.
</details>

<details>
<summary><strong>Q8: What is the role of `WebAsyncManager` in `DispatcherServlet`?</strong></summary>

**Answer:**
`WebAsyncManager` coordinates non-blocking asynchronous servlet processing (Servlet 3.0+ async). When a controller returns `Callable`, `DeferredResult`, or `CompletableFuture`, `WebAsyncManager` releases the original container worker thread back to the servlet pool while keeping the client HTTP connection open until the background task completes.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Configure `DispatcherServlet` Load On Startup</strong></summary>

**Task**: Write the `application.yaml` property setting that instructs Spring Boot to initialize the `DispatcherServlet` immediately when Tomcat starts.

```yaml
# Solution:
spring:
  mvc:
    servlet:
      load-on-startup: 1
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Register a Custom Interceptor</strong></summary>

**Task**: Implement a `WebMvcConfigurer` that registers an interceptor `HeaderAuditInterceptor` for all paths starting with `/api/**` except `/api/public/**`.

```java
// Solution:
@Configuration
public class WebMvcConfig implements WebMvcConfigurer {
    @Autowired
    private HeaderAuditInterceptor interceptor;

    @Override
    public void addInterceptors(InterceptorRegistry registry) {
        registry.addInterceptor(interceptor)
                .addPathPatterns("/api/**")
                .excludePathPatterns("/api/public/**");
    }
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Custom Argument Resolver for API Version Header</strong></summary>

**Task**: Build a `HandlerMethodArgumentResolver` that resolves an `@ApiVersion int version` parameter from the HTTP header `X-API-Version`, defaulting to `1` if absent.

```java
// Solution:
@Component
public class ApiVersionArgumentResolver implements HandlerMethodArgumentResolver {
    @Override
    public boolean supportsParameter(MethodParameter parameter) {
        return parameter.hasParameterAnnotation(ApiVersion.class) &&
               parameter.getParameterType().equals(int.class);
    }

    @Override
    public Object resolveArgument(MethodParameter parameter,
                                  ModelAndViewContainer mavContainer,
                                  NativeWebRequest webRequest,
                                  WebDataBinderFactory binderFactory) {
        String header = webRequest.getHeader("X-API-Version");
        if (header != null && !header.isBlank()) {
            try { return Integer.parseInt(header); } catch (NumberFormatException ignored) {}
        }
        return 1;
    }
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Enforce Request Correlation ID in Interceptor</strong></summary>

**Task**: Build an interceptor that inspects `X-Correlation-ID`. If missing, generate a new UUID. Add the ID to both MDC logging context and the outgoing `HttpServletResponse` headers.

```java
// Solution:
@Component
public class CorrelationIdInterceptor implements HandlerInterceptor {
    private static final String CORRELATION_HEADER = "X-Correlation-ID";

    @Override
    public boolean preHandle(HttpServletRequest req, HttpServletResponse res, Object handler) {
        String correlationId = req.getHeader(CORRELATION_HEADER);
        if (correlationId == null || correlationId.isBlank()) {
            correlationId = UUID.randomUUID().toString();
        }
        MDC.put("correlationId", correlationId);
        res.setHeader(CORRELATION_HEADER, correlationId);
        return true;
    }

    @Override
    public void afterCompletion(HttpServletRequest req, HttpServletResponse res, Object handler, Exception ex) {
        MDC.remove("correlationId");
    }
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Request Response Body Audit Logger</strong></summary>

**Task**: Implement a production-grade audit filter using `ContentCachingRequestWrapper` and `ContentCachingResponseWrapper` that safely reads and logs request and response JSON payloads without exhausting the servlet input streams.
</details>

<details>
<summary><strong>Exercise 6 (Hard): Custom Dynamic HandlerMapping Implementation</strong></summary>

**Task**: Implement a custom `HandlerMapping` bean that registers routes dynamically from a database table at runtime without requiring application restart.
</details>

---

## 12. Summary & Quick Reference

- **Pattern**: Front Controller pattern coordinates all web traffic.
- **Key Method**: `DispatcherServlet.doDispatch()` manages lookup, interceptors, execution, and exceptions.
- **Interceptors**: `preHandle()` $\rightarrow$ Handler $\rightarrow$ `postHandle()` $\rightarrow$ Rendering $\rightarrow$ `afterCompletion()`.
- **Java 21 Virtual Threads**: Enable via `spring.threads.virtual.enabled=true`.

---

## 13. Further Reading

- [Spring Framework Reference: Web MVC DispatcherServlet](https://docs.spring.io/spring-framework/reference/web/webmvc/mvc-servlet.html)
- [Jakarta Servlet 6.0 Specification](https://jakarta.ee/specifications/servlet/6.0/)
- [Spring MVC HandlerMapping and Interceptors](https://docs.spring.io/spring-framework/reference/web/webmvc/mvc-servlet/special-bean-types.html)
- [High-Performance Spring Boot with Virtual Threads](https://spring.io/blog/2023/09/20/embracing-virtual-threads)
