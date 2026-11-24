# 03: Spring Web MVC & REST APIs Technical Interview Q&A

| Metadata Field | Value |
|---|---|
| **Topic** | DispatcherServlet Lifecycle, RFC 9457 ProblemDetail, Interceptors & Content Negotiation |
| **Spring Boot Version** | 3.4.x |
| **Java Version** | 21 / 25 LTS |
| **Target Level** | Senior / Lead / Staff Backend Engineer |
| **Key Focus Areas** | `DispatcherServlet`, `HandlerAdapter`, Filters vs Interceptors, RFC 9457 `ProblemDetail` |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [Internals & Architecture](#3-internals--architecture)
4. [Syntax & Practical Usage](#4-syntax--practical-usage)
5. [5 Practical Examples](#5-5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Pitfalls & Anti-Patterns](#7-common-pitfalls--anti-patterns)
8. [Performance & Security Considerations](#8-performance--security-considerations)
9. [Comparison / Decision Matrix](#9-comparison--decision-matrix)
10. [Real-World Interview Q&A](#10-real-world-interview-qa)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference Cheatsheet](#12-summary--quick-reference-cheatsheet)
13. [Further Reading & Official Resources](#13-further-reading--official-resources)

---

## 1. What & Why

Spring Web MVC is the most widely deployed REST framework in enterprise Java. While almost every Java developer can write `@RestController` and `@GetMapping`, senior and staff-level interviews focus on the inner workings of the HTTP request processing pipeline:

Interviewers look for mastery over the `DispatcherServlet` lifecycle, how `HandlerMapping` matches URLs to controller methods, how `HandlerAdapter` binds and validates arguments, the architectural differences between Servlet `Filter`s and Spring `HandlerInterceptor`s, modern RFC 7807 / RFC 9457 `ProblemDetail` specification compliance, and HTTP Content Negotiation.

---

## 2. Core Concepts

### 2.1 The `DispatcherServlet` Request Pipeline
1. **Request Ingestion**: Servlet container forwards HTTP request to `DispatcherServlet.doDispatch()`.
2. **HandlerMapping**: Finds the matching `HandlerExecutionChain` (contains the handler method + registered `HandlerInterceptor`s).
3. **HandlerExecutionChain Pre-Handle**: Runs `interceptor.preHandle()` in sequential order.
4. **HandlerAdapter**: Resolves arguments (`@RequestBody`, `@PathVariable`), performs Jakarta Validation, and invokes the controller method.
5. **HandlerExecutionChain Post-Handle**: Runs `interceptor.postHandle()`.
6. **Exception Resolution**: If an error occurred, `HandlerExceptionResolver` (`@ExceptionHandler` / `ResponseEntityExceptionHandler`) catches it.
7. **HandlerExecutionChain After-Completion**: Runs `interceptor.afterCompletion()` (useful for cleanup and metric tracking).

### 2.2 RFC 9457 `ProblemDetail` Error Standard
Spring Boot 3 natively adopts RFC 7807 / RFC 9457 `application/problem+json` standardizing error responses:
```json
{
  "type": "https://api.example.com/errors/not-found",
  "title": "Resource Not Found",
  "status": 404,
  "detail": "Customer with ID 42 was not found",
  "instance": "/api/v1/customers/42"
}
```

---

## 3. Internals & Architecture

```
       [ Inbound HTTP Request: POST /api/v1/orders ]
                           │
                           ▼
  ┌─────────────────────────────────────────────────────────┐
  │                 Servlet Container Filters               │
  │   - SecurityFilter (Auth), CorsFilter, TracingFilter    │
  └────────────────────────┬────────────────────────────────┘
                           │
                           ▼
  ┌─────────────────────────────────────────────────────────┐
  │             DispatcherServlet.doDispatch()              │
  └────────────────────────┬────────────────────────────────┘
                           │
                           ▼
  ┌─────────────────────────────────────────────────────────┐
  │ 1. HandlerMapping -> Resolves HandlerExecutionChain     │
  └────────────────────────┬────────────────────────────────┘
                           │
                           ▼
  ┌─────────────────────────────────────────────────────────┐
  │ 2. HandlerInterceptor.preHandle() (Returns true/false)  │
  └────────────────────────┬────────────────────────────────┘
                           │
                           ▼
  ┌─────────────────────────────────────────────────────────┐
  │ 3. HandlerAdapter (RequestMappingHandlerAdapter)        │
  │    - Resolves @RequestBody, @PathVariable, Validates    │
  │    - Invokes Controller Method -> returns response      │
  └────────────────────────┬────────────────────────────────┘
                           │
                           ▼
  ┌─────────────────────────────────────────────────────────┐
  │ 4. HandlerInterceptor.afterCompletion()                 │
  └────────────────────────┬────────────────────────────────┘
                           │
                           ▼
                 [ HTTP Response Sent ]
```

---

## 4. Syntax & Practical Usage

### 4.1 Global Exception Handler with `ProblemDetail`
```java
@RestControllerAdvice
public class GlobalExceptionHandler extends ResponseEntityExceptionHandler {
    @ExceptionHandler(ResourceNotFoundException.class)
    public ProblemDetail handleNotFound(ResourceNotFoundException ex) {
        ProblemDetail problem = ProblemDetail.forStatusAndDetail(HttpStatus.NOT_FOUND, ex.getMessage());
        problem.setTitle("Resource Not Found");
        problem.setProperty("timestamp", Instant.now());
        return problem;
    }
}
```

---

## 5. 5 Practical Examples

### Example 1: Modern REST Controller with Jakarta Validation 3.0

```java
package com.example.web.controller;

import jakarta.validation.Valid;
import jakarta.validation.constraints.*;
import org.springframework.http.HttpStatus;
import org.springframework.web.bind.annotation.*;

import java.math.BigDecimal;
import java.util.UUID;

@RestController
@RequestMapping("/api/v1/accounts")
public class AccountRestController {

    public record CreateAccountRequest(
            @NotBlank(message = "Account name is mandatory")
            @Size(min = 3, max = 50)
            String name,

            @NotBlank(message = "Owner email is required")
            @Email(message = "Invalid email format")
            String email,

            @NotNull(message = "Initial deposit cannot be null")
            @DecimalMin(value = "10.00", message = "Minimum deposit is 10.00")
            BigDecimal initialDeposit
    ) {}

    public record AccountResponse(UUID id, String name, String email, BigDecimal balance) {}

    @PostMapping
    @ResponseStatus(HttpStatus.CREATED)
    public AccountResponse createAccount(@Valid @RequestBody CreateAccountRequest request) {
        return new AccountResponse(UUID.randomUUID(), request.name(), request.email(), request.initialDeposit());
    }
}
```

### Example 2: RFC 9457 `ResponseEntityExceptionHandler` Implementation

```java
package com.example.web.exception;

import org.springframework.http.*;
import org.springframework.validation.FieldError;
import org.springframework.web.bind.MethodArgumentNotValidException;
import org.springframework.web.bind.annotation.ExceptionHandler;
import org.springframework.web.bind.annotation.RestControllerAdvice;
import org.springframework.web.context.request.WebRequest;
import org.springframework.web.servlet.mvc.method.annotation.ResponseEntityExceptionHandler;

import java.net.URI;
import java.time.Instant;
import java.util.Map;
import java.util.stream.Collectors;

@RestControllerAdvice
public class GlobalRestExceptionHandler extends ResponseEntityExceptionHandler {

    @Override
    protected ResponseEntity<Object> handleMethodArgumentNotValid(
            MethodArgumentNotValidException ex, HttpHeaders headers, HttpStatusCode status, WebRequest request) {

        ProblemDetail problem = ProblemDetail.forStatusAndDetail(status, "One or more validation constraints failed");
        problem.setTitle("Validation Failure");
        problem.setType(URI.create("https://api.example.com/errors/validation"));

        Map<String, String> errors = ex.getBindingResult().getFieldErrors().stream()
                .collect(Collectors.toMap(
                        FieldError::getField,
                        f -> f.getDefaultMessage() != null ? f.getDefaultMessage() : "Invalid value",
                        (v1, v2) -> v1
                ));

        problem.setProperty("invalidFields", errors);
        problem.setProperty("timestamp", Instant.now());

        return ResponseEntity.status(status).body(problem);
    }

    @ExceptionHandler(IllegalArgumentException.class)
    public ProblemDetail handleIllegalArgument(IllegalArgumentException ex) {
        ProblemDetail problem = ProblemDetail.forStatusAndDetail(HttpStatus.BAD_REQUEST, ex.getMessage());
        problem.setTitle("Illegal Argument");
        return problem;
    }
}
```

### Example 3: High-Performance Custom `HandlerInterceptor`

```java
package com.example.web.interceptor;

import jakarta.servlet.http.HttpServletRequest;
import jakarta.servlet.http.HttpServletResponse;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Component;
import org.springframework.web.servlet.HandlerInterceptor;

@Component
public class ExecutionTimeInterceptor implements HandlerInterceptor {

    private static final Logger log = LoggerFactory.getLogger(ExecutionTimeInterceptor.class);
    private static final String START_TIME = "request_start_time";

    @Override
    public boolean preHandle(HttpServletRequest request, HttpServletResponse response, Object handler) {
        request.setAttribute(START_TIME, System.currentTimeMillis());
        return true; // continue down the chain
    }

    @Override
    public void afterCompletion(HttpServletRequest request, HttpServletResponse response, Object handler, Exception ex) {
        Long start = (Long) request.getAttribute(START_TIME);
        if (start != null) {
            long duration = System.currentTimeMillis() - start;
            log.info("Request [{}] {} completed in {} ms with status {}",
                    request.getMethod(), request.getRequestURI(), duration, response.getStatus());
        }
    }
}
```

### Example 4: WebMvcConfigurer Interceptor Registration

```java
package com.example.web.config;

import com.example.web.interceptor.ExecutionTimeInterceptor;
import org.springframework.context.annotation.Configuration;
import org.springframework.web.servlet.config.annotation.InterceptorRegistry;
import org.springframework.web.servlet.config.annotation.WebMvcConfigurer;

@Configuration
public class WebMvcConfig implements WebMvcConfigurer {

    private final ExecutionTimeInterceptor timeInterceptor;

    public WebMvcConfig(ExecutionTimeInterceptor timeInterceptor) {
        this.timeInterceptor = timeInterceptor;
    }

    @Override
    public void addInterceptors(InterceptorRegistry registry) {
        registry.addInterceptor(timeInterceptor)
                .addPathPatterns("/api/**")
                .excludePathPatterns("/api/public/**", "/actuator/**");
    }
}
```

### Example 5: Custom Argument Resolver (`HandlerMethodArgumentResolver`)

```java
package com.example.web.resolver;

import org.springframework.core.MethodParameter;
import org.springframework.stereotype.Component;
import org.springframework.web.bind.support.WebDataBinderFactory;
import org.springframework.web.context.request.NativeWebRequest;
import org.springframework.web.method.support.HandlerMethodArgumentResolver;
import org.springframework.web.method.support.ModelAndViewContainer;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@Component
public class CurrentUserArgumentResolver implements HandlerMethodArgumentResolver {

    @Retention(RetentionPolicy.RUNTIME)
    public @interface CurrentUser {}

    public record AuthenticatedUser(String userId, String tenantId) {}

    @Override
    public boolean supportsParameter(MethodParameter parameter) {
        return parameter.hasParameterAnnotation(CurrentUser.class) &&
               parameter.getParameterType().equals(AuthenticatedUser.class);
    }

    @Override
    public Object resolveArgument(MethodParameter parameter, ModelAndViewContainer mavContainer,
                                  NativeWebRequest webRequest, WebDataBinderFactory binderFactory) {
        String userId = webRequest.getHeader("X-User-ID");
        String tenantId = webRequest.getHeader("X-Tenant-ID");
        return new AuthenticatedUser(
                userId != null ? userId : "anonymous",
                tenantId != null ? tenantId : "public"
        );
    }
}
```

---

## 6. Best Practices

- **Adopt RFC 9457 `ProblemDetail` Universally**: Replace unstructured error JSON maps (`{"message": "error"}`) with standard `ProblemDetail` responses.
- **Enable Strict ProblemDetail in `application.yaml`**:
  ```yaml
  spring:
    mvc:
      problemdetails:
        enabled: true
  ```
- **Filter for Security, Interceptor for Application Context**: Use Servlet `Filter`s for low-level concerns (CORS, TLS, Rate Limiting, JWT byte extraction). Use `HandlerInterceptor`s when Spring MVC context or handler method reflection is required.
- **Always Validate with `@Valid` on Controller Parameters**: Avoid manual imperative validation checks inside service layers.
- **Never Catch `Exception` and Suppress Stack Traces**: Log unexpected 500 errors with full stack traces while masking internal error details from client JSON payloads.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Why It Fails | Correct Approach |
|---|---|---|
| **Catching Exceptions Manually in Controller** | Pollutes every controller method with repetitive `try/catch` boilerplate. | Centralize error handling in `@RestControllerAdvice`. |
| **Returning Raw Entity Models Directly** | Exposes database schema, risks cyclic JSON serialization recursion, and leaks sensitive fields. | Always map entities to immutable DTOs (`record`). |
| **Using Filter for Controller Method Reflection** | Servlet `Filter` executes before `DispatcherServlet`; it cannot inspect controller annotations or method signatures. | Use Spring `HandlerInterceptor`. |
| **Modifying Request Body in Interceptor** | `HttpServletRequest.getInputStream()` can only be read once; consuming it breaks downstream controller argument resolvers. | Wrap request in `ContentCachingRequestWrapper`. |
| **Hardcoding HTTP 200 with Error JSON** | Violates HTTP semantics and prevents standard API Gateway error monitoring. | Return proper HTTP status codes (`400`, `401`, `403`, `404`, `409`, `422`). |
| **Missing Content-Negotiation Fallbacks** | Clients sending unfamiliar `Accept` headers receive unexpected 406 Not Acceptable errors. | Configure default content type in `WebMvcConfigurer`. |

---

## 8. Performance & Security Considerations

### 8.1 Async Request Processing with Virtual Threads
Under Spring Boot 3.4 on Java 21, enable virtual threads to handle blocking I/O inside controllers without thread pool exhaustion:
```yaml
spring:
  threads:
    virtual:
      enabled: true
```

### 8.2 Safe Content Caching for Audit Logging
Use `ContentCachingRequestWrapper` in a filter if request body logging is required:
```java
ContentCachingRequestWrapper wrapped = new ContentCachingRequestWrapper(request);
filterChain.doFilter(wrapped, response);
byte[] body = wrapped.getContentAsByteArray();
```

---

## 9. Comparison / Decision Matrix

| Dimension | Servlet `Filter` | Spring `HandlerInterceptor` |
|---|---|---|
| **Specification** | Jakarta Servlet API | Spring Web MVC Framework |
| **Lifecycle Point** | Before & after `DispatcherServlet` | Around the Controller handler execution |
| **Spring Bean Access**| Requires `@Component` / `DelegatingFilterProxy`| Native Spring Bean |
| **Handler Method Access**| No (Raw request/response only) | Yes (`Object handler` cast to `HandlerMethod`)|
| **Best Used For** | Authentication, TLS, Gzip compression, CORS | Audit timing, tenancy context, custom annotations |

---

## 10. Real-World Interview Q&A

<details>
<summary><strong>1. Walk through the complete lifecycle of a request entering `DispatcherServlet`.</strong></summary>

The request enters `doDispatch()`. `HandlerMapping` matches the URL to a `HandlerExecutionChain`. Interceptor `preHandle()` methods execute. `HandlerAdapter` resolves method parameters (body, headers, path variables) and invokes the `@RestController` method. Interceptor `postHandle()` executes. If an exception occurs, `HandlerExceptionResolver` converts it into an HTTP response. Finally, `afterCompletion()` executes for cleanup.
</details>

<details>
<summary><strong>2. What is the difference between a Servlet Filter and a Spring HandlerInterceptor?</strong></summary>

A Servlet Filter is part of the Jakarta Servlet container and operates before the request enters `DispatcherServlet`. A `HandlerInterceptor` is part of Spring MVC and runs inside `DispatcherServlet`, giving it direct access to the target `HandlerMethod` (controller method reflection) and Spring application context.
</details>

<details>
<summary><strong>3. How does Spring Boot 3 implement the RFC 9457 `ProblemDetail` specification?</strong></summary>

Spring Boot 3 introduces `ProblemDetail` (RFC 7807/9457 compliance) natively in `ResponseEntityExceptionHandler`. It standardizes error payloads with fields like `type`, `title`, `status`, `detail`, `instance`, and custom properties, returning the `application/problem+json` media type.
</details>

<details>
<summary><strong>4. Why can't you read the HTTP request body multiple times in a Filter or Interceptor?</strong></summary>

The servlet `InputStream` is a forward-only stream. Once read (e.g. for logging), it cannot be re-read by Spring's `HttpMessageConverter` to deserialize `@RequestBody`. To read it multiple times, wrap the request in `ContentCachingRequestWrapper`.
</details>

<details>
<summary><strong>5. What is the role of `HandlerMethodArgumentResolver`?</strong></summary>

It resolves controller method parameters dynamically. When a controller method has custom parameters (e.g. `@CurrentUser AuthenticatedUser user`), the custom resolver extracts data from headers, session, or JWT and binds it directly to the method argument.
</details>

<details>
<summary><strong>6. How does Content Negotiation work in Spring MVC?</strong></summary>

When a request arrives, Spring checks the `Accept` header (e.g. `application/json` vs `application/xml`) or format query parameter. It iterates through registered `HttpMessageConverter`s and selects the one capable of serializing the response into the requested MIME type.
</details>

<details>
<summary><strong>7. What happens when `@Valid` fails on a `@RequestBody` parameter?</strong></summary>

Spring throws `MethodArgumentNotValidException` containing all field validation errors. If unhandled, it maps to HTTP 400 Bad Request. In `@RestControllerAdvice`, it should be overridden in `ResponseEntityExceptionHandler` to return a structured RFC 9457 `ProblemDetail`.
</details>

<details>
<summary><strong>8. What is the difference between `@Controller` and `@RestController`?</strong></summary>

`@Controller` is designed for traditional MVC applications rendering server-side HTML views (via Thymeleaf or JSP). `@RestController` is a convenience annotation combining `@Controller` and `@ResponseBody`, ensuring all method return values are serialized directly to HTTP response bodies (JSON/XML).
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Create Custom `HandlerMethodArgumentResolver`</strong></summary>

**Prompt**: Implement a resolver that extracts `X-Client-Version` header and injects it into a `ClientVersion` controller method parameter.

**Solution**:
Inspect Example 5: implement `supportsParameter` checking for custom annotation or type, and return header string in `resolveArgument`.
</details>

<details>
<summary><strong>Exercise 2: Implement RFC 9457 Validation Exception Handler</strong></summary>

**Prompt**: Override `handleMethodArgumentNotValid` in a `@RestControllerAdvice` class returning `ProblemDetail` with invalid fields map.

**Solution**:
Inspect Example 2: extract `getFieldErrors()` and call `problem.setProperty("invalidFields", map)`.
</details>

<details>
<summary><strong>Exercise 3: Register Custom Interceptor with Path Exclusions</strong></summary>

**Prompt**: Register an interceptor that applies to `/api/v1/**` but excludes `/api/v1/auth/**`.

**Solution**:
Inspect Example 4: override `addInterceptors()` in `WebMvcConfigurer`.
</details>

<details>
<summary><strong>Exercise 4: Test ProblemDetail Response with MockMvc</strong></summary>

**Prompt**: Write a MockMvc test verifying that submitting an invalid request body returns `application/problem+json` with status 400.

**Solution**:
```java
mockMvc.perform(post("/api/v1/accounts").contentType(MediaType.APPLICATION_JSON).content("{}"))
        .andExpect(status().isBadRequest())
        .andExpect(content().contentType(MediaType.APPLICATION_PROBLEM_JSON))
        .andExpect(jsonPath("$.title").value("Validation Failure"));
```
</details>

<details>
<summary><strong>Exercise 5: Add Response Header in `afterCompletion`</strong></summary>

**Prompt**: Why does calling `response.setHeader()` in `interceptor.afterCompletion()` fail to modify the client response?

**Solution**:
By the time `afterCompletion()` executes, the response stream has already been committed and flushed to the client. Headers must be added in `preHandle()` or via a Servlet `Filter`.
</details>

<details>
<summary><strong>Exercise 6: Implement Custom Validator Annotation</strong></summary>

**Prompt**: Create a custom constraint `@ValidIban` with a `ConstraintValidator` that checks IBAN format.

**Solution**:
Declare `@Constraint(validatedBy = IbanValidator.class)` annotation, implement `ConstraintValidator<ValidIban, String>`, and validate length and checksum.
</details>

---

## 12. Summary & Quick Reference Cheatsheet

```
┌────────────────────────────────────────────────────────────────────────┐
│                   SPRING WEB MVC & REST CHEATSHEET                     │
├────────────────────────────────────────────────────────────────────────┤
│ 1. Lifecycle: Filter -> DispatcherServlet -> Interceptor -> Controller │
│ 2. Errors: RFC 9457 ProblemDetail via @RestControllerAdvice            │
│ 3. Filter vs Interceptor: Filter for byte/security; Interceptor for MVC│
│ 4. Validation: Jakarta Validation 3.0 via @Valid on records            │
│ 5. Arguments: Custom HandlerMethodArgumentResolver for domain headers  │
│ 6. Async: Java 21 Virtual Threads natively support blocking MVC calls  │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 13. Further Reading & Official Resources

- [Spring Web MVC Reference Documentation](https://docs.spring.io/spring-framework/reference/web/webmvc.html)
- [RFC 9457: Problem Details for HTTP APIs](https://www.rfc-editor.org/rfc/rfc9457.html)
- [Jakarta Bean Validation 3.0 Specification](https://beanvalidation.org/3.0/)
- [Spring Framework HandlerInterceptor API](https://docs.spring.io/spring-framework/docs/current/javadoc-api/org/springframework/web/servlet/HandlerInterceptor.html)
