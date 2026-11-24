# Interceptors, Filters & Argument Resolvers in Spring MVC

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | Servlet 6.0, DispatcherServlet Request Lifecycle, Spring IoC Container |
| **Estimated Reading Time** | 25 minutes |
| **Status** | Production-Ready |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Baseline** | OpenJDK 21 LTS / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | Tomcat 10.1, OpenJDK 21.0.2 |

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
9. [Comparison: Filter vs Interceptor vs Argument Resolver](#9-comparison-filter-vs-interceptor-vs-argument-resolver)
10. [Interview Questions & Deep Dives](#10-interview-questions--deep-dives)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference](#12-summary--quick-reference)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

In enterprise application development, cross-cutting concerns (authentication, correlation tracing, rate limiting, request caching, payload transformation, custom header resolution) must be isolated from core business logic.

### The Problem
If cross-cutting concerns are implemented directly inside controllers:
1. Controllers become cluttered with repetitive extraction, token parsing, and validation code.
2. Unit testing controllers requires extensive mocking of HTTP request/response machinery.
3. Choosing the wrong extension point (e.g., trying to read handler annotations inside a servlet filter) leads to architectural violations and fragile code.

### The Solution: Layered Interception Pipeline
Spring MVC provides three distinct, complementary interception and resolution layers:
1. **Servlet `Filter`**: Operates at the low-level Servlet container boundary outside `DispatcherServlet` (ideal for security, compression, request body wrapping).
2. **Spring `HandlerInterceptor`**: Operates inside `DispatcherServlet` before and after controller execution (ideal for metric timing, handler reflection checks).
3. **`HandlerMethodArgumentResolver`**: Operates during controller method invocation to translate request data into strongly typed method parameters.

---

## 2. Core Concepts

### Architectural Pipeline
```
               Incoming HTTP Request (TCP / Port 8080)
                                 │
                                 ▼
                     Servlet Filter Chain (Layer 1)
           • OncePerRequestFilter (Security, CORS, MDC Tracing)
           • ContentCachingRequestWrapper (Payload caching)
                                 │
                                 ▼
                    DispatcherServlet (Front Controller)
                                 │
                                 ▼
                   HandlerExecutionChain (Layer 2)
           • HandlerInterceptor.preHandle()
             (Metrics start timer, HandlerMethod inspection)
                                 │
                                 ▼
             HandlerMethodArgumentResolver (Layer 3)
           • Resolves @CurrentUser, TenantContext, Signature
           • Type coercion and parameter construction
                                 │
                                 ▼
                 Target Controller Handler Method
                                 │
                                 ▼
                   HandlerExecutionChain (Layer 2)
           • HandlerInterceptor.postHandle()
           • HandlerInterceptor.afterCompletion() (Cleanup, metrics stop)
                                 │
                                 ▼
                     Servlet Filter Chain (Layer 1)
           • Filter completes (Response headers flushed)
                                 │
                                 ▼
                        HTTP Response Out
```

---

## 3. How It Works Internally

### 1. The Servlet Filter Lifecycle
A `Filter` implements `jakarta.servlet.Filter`. Spring provides `OncePerRequestFilter`, which guarantees single execution per request dispatch, even across asynchronous dispatches or error forwards. Filters can wrap `HttpServletRequest` and `HttpServletResponse` (e.g., `ContentCachingRequestWrapper`) to allow multiple reads of input streams.

### 2. The `HandlerInterceptor` Lifecycle
Managed by `DispatcherServlet`:
- **`preHandle()`**: Executed before the handler is invoked. Returns `boolean` (`true` to continue, `false` to abort).
- **`postHandle()`**: Executed after the handler method returns, but before view rendering. Not called if an exception was thrown.
- **`afterCompletion()`**: Executed after the complete request finishes (including view rendering and exception resolution). Guaranteed to run, ideal for resource cleanup and metric timing.

### 3. The `HandlerMethodArgumentResolver` Mechanics
Managed by `RequestMappingHandlerAdapter`:
- **`supportsParameter(MethodParameter)`**: Fast inspection of parameter types and annotations. Results are cached per method parameter.
- **`resolveArgument(...)`**: Pulls data from `NativeWebRequest` and constructs the parameter instance passed to the controller method.

---

## 4. Syntax & Configuration

### Registering Filters via `FilterRegistrationBean`
```java
package com.example.web.config;

import com.example.web.filter.CorrelationIdFilter;
import org.springframework.boot.web.servlet.FilterRegistrationBean;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;

@Configuration(proxyBeanMethods = false)
public class FilterRegistrationConfig {

    @Bean
    public FilterRegistrationBean<CorrelationIdFilter> correlationFilterRegistration(CorrelationIdFilter filter) {
        FilterRegistrationBean<CorrelationIdFilter> reg = new FilterRegistrationBean<>(filter);
        reg.setOrder(1); // Lowest integer = highest precedence
        reg.addUrlPatterns("/api/*");
        return reg;
    }
}
```

### Registering Interceptors and Argument Resolvers via `WebMvcConfigurer`
```java
package com.example.web.config;

import com.example.web.interceptor.ExecutionTimeInterceptor;
import com.example.web.resolver.TenantContextArgumentResolver;
import org.springframework.context.annotation.Configuration;
import org.springframework.web.method.support.HandlerMethodArgumentResolver;
import org.springframework.web.servlet.config.annotation.InterceptorRegistry;
import org.springframework.web.servlet.config.annotation.WebMvcConfigurer;

import java.util.List;

@Configuration(proxyBeanMethods = false)
public class WebMvcExtensionConfig implements WebMvcConfigurer {

    private final ExecutionTimeInterceptor interceptor;
    private final TenantContextArgumentResolver resolver;

    public WebMvcExtensionConfig(ExecutionTimeInterceptor interceptor, TenantContextArgumentResolver resolver) {
        this.interceptor = interceptor;
        this.resolver = resolver;
    }

    @Override
    public void addInterceptors(InterceptorRegistry registry) {
        registry.addInterceptor(interceptor)
                .addPathPatterns("/api/**")
                .excludePathPatterns("/api/public/**");
    }

    @Override
    public void addArgumentResolvers(List<HandlerMethodArgumentResolver> resolvers) {
        resolvers.add(resolver);
    }
}
```

---

## 5. Practical Examples

### Example 1: Correlation ID MDC Filter (`OncePerRequestFilter`)

```java
package com.example.web.filter;

import jakarta.servlet.FilterChain;
import jakarta.servlet.ServletException;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.servlet.http.HttpServletResponse;
import org.slf4j.MDC;
import org.springframework.core.Ordered;
import org.springframework.core.annotation.Order;
import org.springframework.stereotype.Component;
import org.springframework.web.filter.OncePerRequestFilter;

import java.io.IOException;
import java.util.UUID;

@Component
@Order(Ordered.HIGHEST_PRECEDENCE)
public class CorrelationIdFilter extends OncePerRequestFilter {

    public static final String CORRELATION_ID_HEADER = "X-Correlation-ID";
    public static final String MDC_KEY = "correlationId";

    @Override
    protected void doFilterInternal(HttpServletRequest request,
                                    HttpServletResponse response,
                                    FilterChain filterChain) throws ServletException, IOException {
        String correlationId = request.getHeader(CORRELATION_ID_HEADER);
        if (correlationId == null || correlationId.isBlank()) {
            correlationId = UUID.randomUUID().toString();
        }

        MDC.put(MDC_KEY, correlationId);
        response.setHeader(CORRELATION_ID_HEADER, correlationId);

        try {
            filterChain.doFilter(request, response);
        } finally {
            MDC.remove(MDC_KEY); // Mandatory cleanup to prevent ThreadLocal memory leaks
        }
    }
}
```

### Example 2: Handler Method Annotation Interceptor

```java
package com.example.web.interceptor;

import jakarta.servlet.http.HttpServletRequest;
import jakarta.servlet.http.HttpServletResponse;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Component;
import org.springframework.web.method.HandlerMethod;
import org.springframework.web.servlet.HandlerInterceptor;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

@Target({ElementType.METHOD, ElementType.TYPE})
@Retention(RetentionPolicy.RUNTIME)
public @interface RateLimited {
    int requestsPerMinute() default 60;
}

@Component
public class RateLimitingInterceptor implements HandlerInterceptor {

    private static final Logger log = LoggerFactory.getLogger(RateLimitingInterceptor.class);

    @Override
    public boolean preHandle(HttpServletRequest request, HttpServletResponse response, Object handler) throws Exception {
        // Interceptors can inspect the target HandlerMethod!
        if (handler instanceof HandlerMethod handlerMethod) {
            RateLimited rateLimit = handlerMethod.getMethodAnnotation(RateLimited.class);
            if (rateLimit == null) {
                rateLimit = handlerMethod.getBeanType().getAnnotation(RateLimited.class);
            }

            if (rateLimit != null) {
                log.info("Enforcing rate limit of {} req/min on method {}",
                        rateLimit.requestsPerMinute(), handlerMethod.getMethod().getName());
                // In production, evaluate token bucket in Redis...
            }
        }
        return true;
    }
}
```

### Example 3: Enterprise Custom Argument Resolver for Authenticated User Principal

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
import java.util.UUID;

@Target(ElementType.PARAMETER)
@Retention(RetentionPolicy.RUNTIME)
public @interface AuthenticatedUser {}

public record UserPrincipal(UUID userId, String username, String role) {}

@Component
public class AuthenticatedUserArgumentResolver implements HandlerMethodArgumentResolver {

    @Override
    public boolean supportsParameter(MethodParameter parameter) {
        return parameter.hasParameterAnnotation(AuthenticatedUser.class) &&
               parameter.getParameterType().equals(UserPrincipal.class);
    }

    @Override
    public Object resolveArgument(MethodParameter parameter,
                                  ModelAndViewContainer mavContainer,
                                  NativeWebRequest webRequest,
                                  WebDataBinderFactory binderFactory) {
        String userIdHeader = webRequest.getHeader("X-User-Id");
        String username = webRequest.getHeader("X-User-Name");
        String role = webRequest.getHeader("X-User-Role");

        if (userIdHeader == null || username == null) {
            return new UserPrincipal(UUID.fromString("00000000-0000-0000-0000-000000000000"), "anonymous", "GUEST");
        }

        return new UserPrincipal(UUID.fromString(userIdHeader), username, role != null ? role : "USER");
    }
}
```

### Example 4: Anti-Pattern vs Production Fix

#### Anti-Pattern: Reading Request Body in Interceptor (Causing Stream Exhaustion)
```java
// ANTI-PATTERN: Attempting to read request body in HandlerInterceptor
@Component
public class BadPayloadAuditInterceptor implements HandlerInterceptor {
    @Override
    public boolean preHandle(HttpServletRequest request, HttpServletResponse response, Object handler) throws Exception {
        // Reads input stream! Once read, Jackson cannot read it again in @RequestBody!
        String body = new String(request.getInputStream().readAllBytes()); // FAILS downstream!
        return true;
    }
}
```

#### Production Fix: Use `RequestBodyAdviceAdapter` or Caching Filter
```java
// PRODUCTION FIX: Use Spring's RequestBodyAdvice to inspect payloads cleanly
package com.example.web.advice;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.core.MethodParameter;
import org.springframework.http.HttpInputMessage;
import org.springframework.http.converter.HttpMessageConverter;
import org.springframework.web.bind.annotation.RestControllerAdvice;
import org.springframework.web.servlet.mvc.method.annotation.RequestBodyAdviceAdapter;

import java.lang.reflect.Type;

@RestControllerAdvice
public class SafePayloadAuditAdvice extends RequestBodyAdviceAdapter {

    private static final Logger log = LoggerFactory.getLogger(SafePayloadAuditAdvice.class);

    @Override
    public boolean supports(MethodParameter methodParameter, Type targetType, Class<? extends HttpMessageConverter<?>> converterType) {
        return true;
    }

    @Override
    public Object afterBodyRead(Object body, HttpInputMessage inputMessage, MethodParameter parameter, Type targetType, Class<? extends HttpMessageConverter<?>> converterType) {
        log.info("Successfully bound payload for {}: {}", parameter.getMethod().getName(), body);
        return body; // Return unmodified object
    }
}
```

### Example 5: High-Performance Payload Caching Filter for Response Logging

```java
package com.example.web.filter;

import jakarta.servlet.FilterChain;
import jakarta.servlet.ServletException;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.servlet.http.HttpServletResponse;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Component;
import org.springframework.web.filter.OncePerRequestFilter;
import org.springframework.web.util.ContentCachingRequestWrapper;
import org.springframework.web.util.ContentCachingResponseWrapper;

import java.io.IOException;
import java.nio.charset.StandardCharsets;

@Component
public class RequestResponseLoggingFilter extends OncePerRequestFilter {

    private static final Logger log = LoggerFactory.getLogger(RequestResponseLoggingFilter.class);

    @Override
    protected void doFilterInternal(HttpServletRequest request,
                                    HttpServletResponse response,
                                    FilterChain filterChain) throws ServletException, IOException {
        ContentCachingRequestWrapper wrappedRequest = new ContentCachingRequestWrapper(request);
        ContentCachingResponseWrapper wrappedResponse = new ContentCachingResponseWrapper(response);

        try {
            filterChain.doFilter(wrappedRequest, wrappedResponse);
        } finally {
            byte[] reqContent = wrappedRequest.getContentAsByteArray();
            byte[] resContent = wrappedResponse.getContentAsByteArray();

            if (reqContent.length > 0) {
                log.info("Request Body: {}", new String(reqContent, StandardCharsets.UTF_8));
            }
            if (resContent.length > 0) {
                log.info("Response Body: {}", new String(resContent, StandardCharsets.UTF_8));
            }

            // CRITICAL: Must copy cached response body back to the real response output stream!
            wrappedResponse.copyBodyToResponse();
        }
    }
}
```

---

## 6. Best Practices

1. **Use `OncePerRequestFilter` for Servlets**: Always inherit from `OncePerRequestFilter` to guarantee that your filter logic executes exactly once per request.
2. **Clean Up `ThreadLocal` / MDC in `finally`**: Always call `MDC.remove()` or `ThreadLocal.remove()` in a `finally` block to prevent thread pool data leakage.
3. **Use Argument Resolvers for Parameter Parsing**: Decouple controllers from raw HTTP headers and query strings by encapsulating resolution into `HandlerMethodArgumentResolver` beans.
4. **Always Call `copyBodyToResponse()`**: When using `ContentCachingResponseWrapper`, forgetting to invoke `wrappedResponse.copyBodyToResponse()` sends a completely blank response body to the client.
5. **Prefer Interceptors for Handler Reflection**: If you need to inspect annotations on controller methods, use a `HandlerInterceptor` instead of trying to reverse-engineer routing in a `Filter`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Reading Request InputStream in Interceptor** | The servlet input stream can only be read once; downstream Jackson deserialization crashes. | Use `RequestBodyAdvice` or `ContentCachingRequestWrapper`. |
| **Forgetting `wrappedResponse.copyBodyToResponse()`** | Client receives empty HTTP response because bytes remain buffered in memory wrapper. | Always call `wrappedResponse.copyBodyToResponse()` in a `finally` block. |
| **MDC Leak in ThreadPool** | Not clearing MDC values causes subsequent requests on the same thread to inherit stale trace IDs. | Always call `MDC.remove()` in a `finally` block of the filter. |
| **Using `Filter` to Check Method Annotations** | Filters run before `DispatcherServlet` identifies the matching controller method. | Use `HandlerInterceptor`, where the `handler` object casts to `HandlerMethod`. |
| **Returning `false` from `preHandle` without Writing Response** | Client socket hangs or receives empty HTTP 200 without explanation. | When returning `false`, write status code and message to `HttpServletResponse` directly. |
| **Storing Request State in Singleton Interceptor Fields** | Interceptors are singletons; instance variables cause race conditions across concurrent threads. | Use `request.setAttribute(key, value)` for per-request state. |

---

## 8. Performance & Security Considerations

### Memory Overhead with `ContentCachingRequestWrapper`
Wrapping requests with `ContentCachingRequestWrapper` buffers the entire payload into the JVM heap. For large file uploads (e.g. 50MB), this can trigger severe GC pressure or OutOfMemoryError. Configure maximum payload limits or bypass caching filters on multipart routes (`request.getContentType().startsWith("multipart/")`).

### Security Context Clearing
If an interceptor or filter establishes a custom security context or authentication token on `SecurityContextHolder`, ensure the filter resets it (`SecurityContextHolder.clearContext()`) to prevent context bleeding across requests.

---

## 9. Comparison: Filter vs Interceptor vs Argument Resolver

| Feature | Servlet `Filter` | `HandlerInterceptor` | `HandlerMethodArgumentResolver` |
| :--- | :--- | :--- | :--- |
| **Container Layer** | Servlet Container (Tomcat/Jetty) | Spring MVC (`DispatcherServlet`) | Spring MVC (`HandlerAdapter`) |
| **Scope of Execution** | Universal (All Servlets & Static Files) | Controller requests only | Target method parameters only |
| **Target Handler Access** | No (Too early in lifecycle) | Yes (`HandlerMethod` reflection) | Yes (`MethodParameter` reflection) |
| **Request/Response Wrapping** | Fully supported (`HttpServletRequestWrapper`) | Not supported | Not supported |
| **Primary Use Cases** | CORS, Security, Compression, MDC | Metric timing, handler authorization | Custom value objects, JWT tokens |

---

## 10. Interview Questions & Deep Dives

<details>
<summary><strong>Q1: When would you choose a Servlet `Filter` over a Spring `HandlerInterceptor`?</strong></summary>

**Answer:**
Choose a **Servlet Filter** when:
1. You need to wrap the `HttpServletRequest` or `HttpServletResponse` (e.g. payload caching or response compression).
2. The logic must run universally for all requests, including static resources, actuator endpoints, or custom non-Spring servlets.
3. You are implementing low-level security (e.g. IP blocking, TLS termination inspection).

Choose a **HandlerInterceptor** when you need access to the target Spring controller method (`HandlerMethod`), its annotations, or the `ModelAndView` before view rendering.
</details>

<details>
<summary><strong>Q2: Why is the input stream of an `HttpServletRequest` readable only once by default?</strong></summary>

**Answer:**
Under the Jakarta Servlet specification, `request.getInputStream()` returns a raw socket input stream. To conserve memory and optimize streaming, the servlet container does not buffer stream bytes in memory. Once read to completion, the stream pointer is at EOF and cannot be reset unless explicitly wrapped in a caching decorator like `ContentCachingRequestWrapper`.
</details>

<details>
<summary><strong>Q3: How does `OncePerRequestFilter` prevent duplicate filter execution?</strong></summary>

**Answer:**
During complex request processing (such as internal request forwards via `RequestDispatcher.forward()` or asynchronous dispatches), standard filters can be invoked multiple times for the same logical request. `OncePerRequestFilter` sets a unique request attribute (`alreadyFilteredAttributeName`) upon its first run. Subsequent passes check this attribute and immediately bypass execution.
</details>

<details>
<summary><strong>Q4: What happens if an interceptor's `preHandle()` returns `false`?</strong></summary>

**Answer:**
`DispatcherServlet` immediately halts the execution chain. The controller method is **not** called. Any preceding interceptors that returned `true` have their `afterCompletion()` methods triggered in reverse order to clean up state, but `postHandle()` is skipped.
</details>

<details>
<summary><strong>Q5: How does Spring MVC cache the result of `HandlerMethodArgumentResolver.supportsParameter()`?</strong></summary>

**Answer:**
`InvocableHandlerMethod` maintains an internal `ConcurrentMap<MethodParameter, HandlerMethodArgumentResolver>`. The first time a controller method is invoked, Spring evaluates `supportsParameter()` across registered resolvers. Once a matching resolver is found, it is cached in the map, making subsequent resolutions an $O(1)$ map lookup.
</details>

<details>
<summary><strong>Q6: In what order do interceptor methods execute relative to controller execution?</strong></summary>

**Answer:**
1. `preHandle()`: Executed in ascending registration order (Interceptor 1, then Interceptor 2).
2. Target Controller Method executes.
3. `postHandle()`: Executed in descending order (Interceptor 2, then Interceptor 1).
4. View / Response rendering.
5. `afterCompletion()`: Executed in descending order (Interceptor 2, then Interceptor 1).
</details>

<details>
<summary><strong>Q7: Can a `HandlerMethodArgumentResolver` be used to validate parameters?</strong></summary>

**Answer:**
Yes. Inside `resolveArgument()`, after extracting and instantiating the target object, you can invoke `WebDataBinder.validate()` or throw validation exceptions directly if incoming values are invalid.
</details>

<details>
<summary><strong>Q8: What is the purpose of `RequestBodyAdvice`?</strong></summary>

**Answer:**
`RequestBodyAdvice` allows intercepting the request body before and after it is converted into an object by an `HttpMessageConverter`. It provides a clean, stream-safe hook to decrypt, validate, or mutate incoming JSON objects without wrapping raw servlet streams.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Basic Security Header Filter</strong></summary>

**Task**: Write a `OncePerRequestFilter` that adds `X-Content-Type-Options: nosniff` to all outgoing HTTP responses.

```java
// Solution:
@Component
public class SecurityHeaderFilter extends OncePerRequestFilter {
    @Override
    protected void doFilterInternal(HttpServletRequest request, HttpServletResponse response, FilterChain filterChain)
            throws ServletException, IOException {
        response.setHeader("X-Content-Type-Options", "nosniff");
        filterChain.doFilter(request, response);
    }
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Measure Controller Execution Duration in Interceptor</strong></summary>

**Task**: Implement a `HandlerInterceptor` that records execution duration using `System.nanoTime()` and prints the elapsed milliseconds in `afterCompletion`.

```java
// Solution:
@Component
public class TimingInterceptor implements HandlerInterceptor {
    @Override
    public boolean preHandle(HttpServletRequest req, HttpServletResponse res, Object handler) {
        req.setAttribute("startTime", System.nanoTime());
        return true;
    }

    @Override
    public void afterCompletion(HttpServletRequest req, HttpServletResponse res, Object handler, Exception ex) {
        Long start = (Long) req.getAttribute("startTime");
        if (start != null) {
            long ms = (System.nanoTime() - start) / 1_000_000;
            System.out.println("Execution time: " + ms + "ms");
        }
    }
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Custom Argument Resolver for Client IP</strong></summary>

**Task**: Build a `HandlerMethodArgumentResolver` that resolves a parameter `@ClientIp String ip` by inspecting `X-Forwarded-For` before falling back to `request.getRemoteAddr()`.

```java
// Solution:
@Target(ElementType.PARAMETER)
@Retention(RetentionPolicy.RUNTIME)
public @interface ClientIp {}

@Component
public class ClientIpArgumentResolver implements HandlerMethodArgumentResolver {
    @Override
    public boolean supportsParameter(MethodParameter parameter) {
        return parameter.hasParameterAnnotation(ClientIp.class) && parameter.getParameterType().equals(String.class);
    }

    @Override
    public Object resolveArgument(MethodParameter parameter, ModelAndViewContainer mavContainer,
                                  NativeWebRequest webRequest, WebDataBinderFactory binderFactory) {
        String xff = webRequest.getHeader("X-Forwarded-For");
        if (xff != null && !xff.isBlank()) {
            return xff.split(",")[0].trim();
        }
        HttpServletRequest req = (HttpServletRequest) webRequest.getNativeRequest();
        return req.getRemoteAddr();
    }
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Enforce Secret API Key via Interceptor</strong></summary>

**Task**: Write an interceptor that checks for header `X-Service-Key=secret123`. If absent or incorrect, return HTTP 401 Unauthorized immediately and abort the execution chain.

```java
// Solution:
@Component
public class ServiceKeyInterceptor implements HandlerInterceptor {
    @Override
    public boolean preHandle(HttpServletRequest request, HttpServletResponse response, Object handler) throws IOException {
        String key = request.getHeader("X-Service-Key");
        if (!"secret123".equals(key)) {
            response.setStatus(HttpServletResponse.SC_UNAUTHORIZED);
            response.getWriter().write("Invalid or missing X-Service-Key");
            return false; // Aborts chain
        }
        return true;
    }
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Distributed Token Bucket Rate Limiting Filter</strong></summary>

**Task**: Implement a servlet filter using Bucket4j or Redis that enforces a 100 requests/minute rate limit per IP address, returning HTTP 429 Too Many Requests with a `Retry-After` header when exceeded.
</details>

<details>
<summary><strong>Exercise 6 (Hard): HMAC Signature Verification Argument Resolver</strong></summary>

**Task**: Build a `HandlerMethodArgumentResolver` that reads incoming request body bytes, verifies an `X-Signature` HMAC-SHA256 signature against an internal secret key, and binds a verified payload record to the controller method.
</details>

---

## 12. Summary & Quick Reference

- **Servlet Filter**: Container level, runs outside Spring MVC. Use for security, CORS, MDC, and wrapping streams.
- **HandlerInterceptor**: Inside `DispatcherServlet`. Use for handler reflection, execution timing, and route guards.
- **ArgumentResolver**: Method parameter level. Use to inject custom domain values, user principals, and tenant contexts.
- **Golden Rule**: Always call `wrappedResponse.copyBodyToResponse()` when wrapping responses!

---

## 13. Further Reading

- [Spring Framework Reference: Handler Interceptors](https://docs.spring.io/spring-framework/reference/web/webmvc/mvc-servlet/special-bean-types.html)
- [Spring MVC HandlerMethodArgumentResolver Contract](https://docs.spring.io/spring-framework/docs/current/javadoc-api/org/springframework/web/method/support/HandlerMethodArgumentResolver.html)
- [Jakarta Servlet Specification: Filters](https://jakarta.ee/specifications/servlet/6.0/)
- [Spring OncePerRequestFilter Javadoc & Architecture](https://docs.spring.io/spring-framework/docs/current/javadoc-api/org/springframework/web/filter/OncePerRequestFilter.html)
