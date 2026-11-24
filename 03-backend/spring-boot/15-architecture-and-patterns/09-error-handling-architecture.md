# Error Handling Architecture in Spring Boot

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Version** | 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.x |
| **Tested With** | RFC 7807 (ProblemDetail), ControllerAdvice, OpenTelemetry |

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
In poorly architected backend applications, error handling is an afterthought. Endpoints return inconsistent response formats (sometimes raw text strings, sometimes custom JSON maps, and sometimes raw 500 HTML error pages leaking internal stack traces and SQL database schemas). Web controllers become bloated with repetitive `try-catch` blocks, business services catch generic `Exception` and swallow failures silently, and client developers struggle to programmatically parse error states.

### The Evolution
Modern clean architecture treats error handling as a first-class architectural cross-cutting concern:
1. **Architectural Exception Taxonomy**: A clear hierarchy distinguishing between **Domain Business Violations** (e.g., `InsufficientFundsException`), **Validation Errors** (e.g., `InvalidEmailFormat`), and **Infrastructure/System Failures** (e.g., `DatabaseConnectionTimeoutException`).
2. **Centralized Presentation Translation**: Using Spring's `@RestControllerAdvice` to translate domain exceptions into standardized HTTP error payloads.
3. **Industry Standard Specifications**: Adopting **RFC 7807 / RFC 9457 (Problem Details for HTTP APIs)**, natively integrated into Spring Boot 3.x via the `ProblemDetail` class.

### Production Relevance
A standardized error handling architecture provides predictable machine-readable error responses for API consumers, prevents catastrophic information leakage (OWASP Top 10), correlates failed client requests with distributed OpenTelemetry traces via `traceId`, and keeps business logic completely free of HTTP presentation plumbing.

---

## 2. Core Concepts

### Architectural Exception Hierarchy & Flow

```
+-----------------------------------------------------------------------------------------+
|                              ERROR HANDLING TAXONOMY                                    |
+-----------------------------------------------------------------------------------------+
|                                                                                         |
|                        +-----------------------------+                                  |
|                        |       ApplicationException  | (Base Unchecked Exception)       |
|                        +-----------------------------+                                  |
|                                       |                                                 |
|          +----------------------------+-----------------------------+                   |
|          |                                                          |                   |
|          v                                                          v                   |
|  +-----------------------------+                    +-------------------------------+   |
|  |    DomainBusinessException  |                    |    InfrastructureException    |   |
|  +-----------------------------+                    +-------------------------------+   |
|          |                                                          |                   |
|          +---> EntityNotFoundException                              +---> DatabaseDown  |
|          +---> InvariantViolationException                          +---> NetworkTimeout|
|          +---> InsufficientBalanceException                         +---> ThirdPartyFail|
|                                                                                         |
+-----------------------------------------------------------------------------------------+
                                           |
                                           v (Propagates through Service & Controller)
+-----------------------------------------------------------------------------------------+
|                    CENTRALIZED PRESENTATION LAYER (@RestControllerAdvice)               |
+-----------------------------------------------------------------------------------------+
|                                                                                         |
|   Translates Java Exceptions into RFC 7807 Standardized JSON Payload:                    |
|   {                                                                                     |
|     "type": "https://api.example.com/errors/insufficient-balance",                      |
|     "title": "Insufficient Balance",                                                    |
|     "status": 422,                                                                      |
|     "detail": "Account 9821 has balance $40.00, requested $100.00",                     |
|     "instance": "/api/v1/accounts/9821/withdraw",                                       |
|     "timestamp": "2026-11-24T18:00:00Z",                                                |
|     "traceId": "4bf92f3577b34da6a3ce929d0e0e4736"                                      |
|   }                                                                                     |
|                                                                                         |
+-----------------------------------------------------------------------------------------+
```

### Key Architectural Invariants
- **Unchecked Exception Hierarchy**: Business exceptions should extend `RuntimeException`. Checked exceptions pollute interface signatures across architectural layers.
- **Layer Cleanliness**: The Domain and Service layers must never reference HTTP status codes (`HttpStatus.NOT_FOUND`) or web classes. They throw domain-specific exceptions.
- **Fail Safe / No Info Leakage**: System errors (e.g., `SQLException`) must never expose database table names, SQL syntax, or JVM stack traces to the public API.

---

## 3. How It Works Internally

### DispatcherServlet Exception Interception Lifecycle

```
[ HTTP Client Request ]
           |
           v
[ DispatcherServlet ]
           |
           v
[ Controller ] ---> [ Service Layer ] (throws InsufficientBalanceException)
           |                |
           +<---------------+ (Exception bubbles up unhandled)
           |
           v
[ DispatcherServlet catches Exception ]
           |
           v
[ HandlerExceptionResolverComposite ]
           |
           +---> [ ExceptionHandlerExceptionResolver ]
           |            |
           |            v
           |     Scans @RestControllerAdvice beans for matching @ExceptionHandler
           |            |
           |            v
           |     [ GlobalExceptionHandler.handleBalanceException() ]
           |            |
           |            v
           |     Constructs RFC 7807 ProblemDetail object
           |
           v
[ HttpMessageConverter (Jackson) ] ---> Emits application/problem+json response
```

1. When an exception is thrown in the service layer, it propagates through the controller unhandled.
2. Spring MVC's **DispatcherServlet** catches the exception in its `processHandlerException` method.
3. The **ExceptionHandlerExceptionResolver** searches for methods annotated with `@ExceptionHandler` inside classes marked with `@RestControllerAdvice`.
4. It resolves the most specific matching handler based on the Java exception type hierarchy.
5. The handler returns a Spring 3.x `ProblemDetail` object, which is serialized to JSON with Content-Type `application/problem+json`.

---

## 4. Syntax & Usage

### 1. Enabling Native RFC 7807 in `application.yaml`

```yaml
spring:
  mvc:
    problemdetails:
      enabled: true
```

### 2. Standardized Global `@RestControllerAdvice`

```java
package com.example.common.error;

import org.springframework.http.HttpStatus;
import org.springframework.http.ProblemDetail;
import org.springframework.web.bind.annotation.ExceptionHandler;
import org.springframework.web.bind.annotation.RestControllerAdvice;
import org.springframework.web.servlet.mvc.method.annotation.ResponseEntityExceptionHandler;

import java.net.URI;
import java.time.Instant;

@RestControllerAdvice
public class GlobalExceptionHandler extends ResponseEntityExceptionHandler {

    @ExceptionHandler(EntityNotFoundException.class)
    public ProblemDetail handleNotFound(EntityNotFoundException ex) {
        ProblemDetail problem = ProblemDetail.forStatusAndDetail(HttpStatus.NOT_FOUND, ex.getMessage());
        problem.setTitle("Resource Not Found");
        problem.setType(URI.create("https://api.example.com/errors/not-found"));
        problem.setProperty("timestamp", Instant.now());
        problem.setProperty("errorCode", "ERR_RESOURCE_NOT_FOUND");
        return problem;
    }
}
```

---

## 5. Practical Examples

### Example 1: Domain Exception Taxonomy

```java
package com.example.domain.exception;

// Base domain exception
public abstract class DomainException extends RuntimeException {
    private final String errorCode;

    protected DomainException(String errorCode, String message) {
        super(message);
        this.errorCode = errorCode;
    }

    public String getErrorCode() { return errorCode; }
}

// Concrete business errors
public class EntityNotFoundException extends DomainException {
    public EntityNotFoundException(String entityType, Object id) {
        super("ENTITY_NOT_FOUND", entityType + " with identifier [" + id + "] was not found.");
    }
}

public class BusinessRuleViolationException extends DomainException {
    public BusinessRuleViolationException(String ruleName, String message) {
        super("RULE_VIOLATION_" + ruleName, message);
    }
}
```

### Example 2: Handling Jakarta Bean Validation Errors with Field Granularity

```java
package com.example.common.error;

import org.springframework.http.HttpHeaders;
import org.springframework.http.HttpStatusCode;
import org.springframework.http.ProblemDetail;
import org.springframework.http.ResponseEntity;
import org.springframework.validation.FieldError;
import org.springframework.web.bind.MethodArgumentNotValidException;
import org.springframework.web.bind.annotation.RestControllerAdvice;
import org.springframework.web.context.request.WebRequest;
import org.springframework.web.servlet.mvc.method.annotation.ResponseEntityExceptionHandler;

import java.net.URI;
import java.util.Map;
import java.util.stream.Collectors;

@RestControllerAdvice
public class ValidationExceptionHandler extends ResponseEntityExceptionHandler {

    @Override
    protected ResponseEntity<Object> handleMethodArgumentNotValid(
            MethodArgumentNotValidException ex, 
            HttpHeaders headers, 
            HttpStatusCode status, 
            WebRequest request) {

        ProblemDetail problem = ProblemDetail.forStatusAndDetail(status, "Invalid request payload attributes.");
        problem.setTitle("Validation Failure");
        problem.setType(URI.create("https://api.example.com/errors/validation-failed"));

        // Extract field-level errors into a structured map
        Map<String, String> invalidFields = ex.getBindingResult().getFieldErrors().stream()
                .collect(Collectors.toMap(
                        FieldError::getField,
                        error -> error.getDefaultMessage() != null ? error.getDefaultMessage() : "Invalid value",
                        (existing, replacement) -> existing // Deduplicate
                ));

        problem.setProperty("invalidFields", invalidFields);
        return handleExceptionInternal(ex, problem, headers, status, request);
    }
}
```

### Example 3: Injecting OpenTelemetry `traceId` into Error Responses

```java
package com.example.common.error;

import io.opentelemetry.api.trace.Span;
import org.springframework.http.HttpStatus;
import org.springframework.http.ProblemDetail;
import org.springframework.web.bind.annotation.ExceptionHandler;
import org.springframework.web.bind.annotation.RestControllerAdvice;

import java.net.URI;
import java.time.Instant;

@RestControllerAdvice
public class TracedExceptionHandler {

    @ExceptionHandler(Exception.class)
    public ProblemDetail handleUnhandledSystemError(Exception ex) {
        // Obfuscate internal error: do NOT return ex.getMessage() to external callers!
        ProblemDetail problem = ProblemDetail.forStatusAndDetail(
                HttpStatus.INTERNAL_SERVER_ERROR, 
                "An unexpected internal error occurred. Please contact support with the trace ID."
        );
        problem.setTitle("Internal Server Error");
        problem.setType(URI.create("https://api.example.com/errors/internal-error"));
        problem.setProperty("timestamp", Instant.now());

        // Extract active OpenTelemetry distributed traceId
        String traceId = Span.current().getSpanContext().getTraceId();
        if (traceId != null && !traceId.isBlank()) {
            problem.setProperty("traceId", traceId);
        }

        return problem;
    }
}
```

### Example 4: Anti-Pattern (Catch-and-Return String / 200 OK with Error) vs RFC 7807

#### The Anti-Pattern: HTTP 200 with Error JSON and Stacktrace Leaks
```java
// BAD: Returns 200 OK with custom status flag and leaks internal database details!
@PostMapping("/pay")
public Map<String, Object> badPaymentEndpoint(@RequestBody PaymentRequest req) {
    try {
        paymentService.charge(req);
        return Map.of("success", true);
    } catch (SQLException e) {
        // VIOLATIONS:
        // 1. HTTP 200 OK for a failure!
        // 2. Leaking raw SQL database error message!
        return Map.of(
            "success", false, 
            "error", e.getMessage(), 
            "sqlState", e.getSQLState()
        );
    }
}
```

#### Production Fix: Proper HTTP Status Codes and ProblemDetail
```java
// GOOD: Proper HTTP 422 Unprocessable Entity with RFC 7807 payload
@PostMapping("/pay")
public ResponseEntity<PaymentResponse> cleanPaymentEndpoint(@Valid @RequestBody PaymentRequest req) {
    // Let exceptions bubble up naturally!
    PaymentResponse res = paymentService.charge(req);
    return ResponseEntity.ok(res);
}
```

### Example 5: Custom ProblemDetail Builder Pattern

```java
package com.example.common.error;

import org.springframework.http.HttpStatus;
import org.springframework.http.ProblemDetail;

import java.net.URI;
import java.time.Instant;
import java.util.Map;

public final class ProblemDetailBuilder {

    private final HttpStatus status;
    private final String title;
    private final String detail;
    private URI type;
    private String errorCode;
    private Map<String, Object> extensions;

    public static ProblemDetailBuilder of(HttpStatus status, String title, String detail) {
        return new ProblemDetailBuilder(status, title, detail);
    }

    private ProblemDetailBuilder(HttpStatus status, String title, String detail) {
        this.status = status;
        this.title = title;
        this.detail = detail;
    }

    public ProblemDetailBuilder withType(String uri) {
        this.type = URI.create(uri);
        return this;
    }

    public ProblemDetailBuilder withErrorCode(String code) {
        this.errorCode = code;
        return this;
    }

    public ProblemDetail build() {
        ProblemDetail problem = ProblemDetail.forStatusAndDetail(status, detail);
        problem.setTitle(title);
        if (type != null) problem.setType(type);
        problem.setProperty("timestamp", Instant.now());
        if (errorCode != null) problem.setProperty("errorCode", errorCode);
        return problem;
    }
}
```

---

## 6. Best Practices

1. **Adopt RFC 7807 / RFC 9457**: Standardize all API error outputs on Spring Boot 3.x's `ProblemDetail` specification (`application/problem+json`).
2. **Never Catch `Exception` and Return HTTP 200 OK**: Returning `200 OK` with an error message in the payload breaks HTTP client caching, monitoring metrics, and API gateway routing.
3. **Never Expose Internal Stack Traces in Production**: Configure `server.error.include-stacktrace=never` and sanitize all `500 Internal Server Error` details.
4. **Include OpenTelemetry `traceId` in Error Responses**: Enables client engineers to quote a specific trace ID when submitting support tickets, allowing instant log lookup.
5. **Separate Business Exceptions from Infrastructure Exceptions**: Business exceptions result in client-correctable 4xx codes (`400`, `404`, `409`, `422`); system/network failures result in 5xx codes (`500`, `502`, `503`).
6. **Extend `ResponseEntityExceptionHandler` in ControllerAdvice**: Ensures that all standard Spring MVC exceptions (unsupported HTTP methods, missing request parameters, type mismatches) are automatically handled cleanly.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| **Catching `Exception` in Service and Swallowing It** | Prevents Spring's `TransactionInterceptor` from triggering a database rollback, corrupting data. | Either rethrow as a domain exception or allow unchecked exceptions to bubble up. |
| **Leaking Database Stack Traces (SQL Injection clues)** | Attackers exploit exposed table names, column structures, and constraint names to craft SQL injection exploits. | Log full traces internally; return sanitized generic error messages in production. |
| **Scattering `try-catch` Across Every Controller Method** | Duplicates boilerplate error-handling logic across dozens of controller classes. | Remove controller `try-catch` blocks; delegate entirely to a single `@RestControllerAdvice`. |
| **Using Checked Exceptions Everywhere** | Forces checked exception signatures through every intermediate interface and lambda expression. | Base your exception taxonomy on `RuntimeException`. |
| **Omitting Field-Level Validation Details** | Client receives generic "Validation Failed" without knowing which of the 20 form fields was invalid. | Parse `MethodArgumentNotValidException` to return a map of invalid field names and error messages. |
| **Overlapping `@ExceptionHandler` Methods** | Defining two handlers that both match the same exception hierarchy leads to non-deterministic handler selection. | Ensure exception handler hierarchies are explicit and non-overlapping. |

---

## 8. Performance & Security Notes

### Performance
- **Java Stack Trace Generation Cost**: Instantiating a Java `Throwable` incurs a significant performance penalty due to JVM native `fillInStackTrace()` traversal.
- **Optimization for High-Frequency Business Exceptions**: For expected business rejections (e.g., rate limits or user authentication failures), override `fillInStackTrace()`:
  ```java
  public class LightweightBusinessException extends RuntimeException {
      @Override
      public synchronized Throwable fillInStackTrace() {
          return this; // Skips native stack walk; 10x faster instantiation!
      }
  }
  ```

### Security
- **OWASP Information Exposure Prevention**: Never return raw Java exception class names (e.g., `org.hibernate.exception.ConstraintViolationException`) to public clients. Return a mapped domain error code (e.g., `EMAIL_ALREADY_EXISTS`).

---

## 9. Comparison / When to Use What

| Error Representation | Standard Compliance | Client Developer Experience | Machine Parseable |
|---|---|---|---|
| **Plain Text Strings** | None | Extremely Poor | No (Requires regex scraping) |
| **Custom Map / JSON POJO**| Ad-hoc (Varies per team) | Moderate | Moderate (Requires custom client models) |
| **RFC 7807 (`ProblemDetail`)**| IETF / Spring Boot Standard | Excellent (Universal tooling support) | 100% Native & Standardized |
| **HTTP Status Code Only** | HTTP RFC | Minimal (No error explanation) | Limited |

---

## 10. Interview Questions & Deep Dives

<details>
<summary><strong>1. What is RFC 7807 (Problem Details for HTTP APIs) and what core fields does it define?</strong></summary>

RFC 7807 (updated by RFC 9457) is an IETF specification defining a standardized JSON format for reporting errors from HTTP APIs (`application/problem+json`).
It defines five core standard members:
- `type`: A URI reference that identifies the problem type (e.g., `"https://api.example.com/errors/out-of-stock"`).
- `title`: A short, human-readable summary of the problem type (e.g., `"Out of Stock"`).
- `status`: The HTTP status code generated by the origin server (e.g., `422`).
- `detail`: A human-readable explanation specific to this occurrence of the problem.
- `instance`: A URI reference that identifies the specific occurrence of the problem (e.g., `"/orders/1234/items/56"`).
It also permits custom extension properties (such as `timestamp`, `traceId`, `invalidFields`).
</details>

<details>
<summary><strong>2. How does Spring Boot 3.x natively support RFC 7807?</strong></summary>

Spring Framework 6 / Spring Boot 3.x introduced:
1. `org.springframework.http.ProblemDetail`: A first-class model class implementing all RFC 7807 fields and extensible key-value properties.
2. `spring.mvc.problemdetails.enabled=true`: A configuration flag that automatically instructs Spring MVC's built-in `ResponseEntityExceptionHandler` to render all internal framework exceptions (e.g., 404, 405, 415, 400) as RFC 7807 `application/problem+json` payloads.
</details>

<details>
<summary><strong>3. What is the execution order when multiple `@ExceptionHandler` methods match an exception?</strong></summary>

Spring MVC's `ExceptionHandlerExceptionResolver` selects the most **specific** exception type according to the standard Java class inheritance hierarchy.
For example, if you have:
- `@ExceptionHandler(DomainException.class)`
- `@ExceptionHandler(EntityNotFoundException.class)` (where `EntityNotFoundException extends DomainException`)
If an `EntityNotFoundException` is thrown, Spring guarantees that the `EntityNotFoundException` handler will execute. If an unmapped `DomainException` subclass is thrown, it falls back to the `DomainException` handler.
</details>

<details>
<summary><strong>4. Why should you extend `ResponseEntityExceptionHandler` in your `@RestControllerAdvice` class?</strong></summary>

Spring MVC defines dozens of internal framework exceptions (e.g., `HttpRequestMethodNotSupportedException`, `HttpMediaTypeNotSupportedException`, `MissingServletRequestParameterException`, `MethodArgumentNotValidException`).
If you create a blank `@RestControllerAdvice` without extending `ResponseEntityExceptionHandler`, these standard exceptions bypass your custom handlers or produce non-standard responses. Extending it ensures all built-in web exceptions are consistently formatted according to your global rules.
</details>

<details>
<summary><strong>5. How do you correlate client-facing error responses with backend server logs?</strong></summary>

By propagating a unique **Correlation ID / Trace ID**:
1. OpenTelemetry or Micrometer Tracing assigns a distributed `traceId` to the incoming request.
2. In the global exception handler, retrieve the active `traceId`:
   `problem.setProperty("traceId", Tracer.currentSpan().context().traceId());`
3. The server logs the full error stacktrace tagged with the same `traceId` via SLF4J MDC.
4. When a client encounters an error, they provide the `traceId` to support, enabling engineers to search centralized logging (OpenSearch, Grafana Loki) for the exact trace.
</details>

<details>
<summary><strong>6. What is the difference between `@ControllerAdvice` and `@RestControllerAdvice`?</strong></summary>

- `@ControllerAdvice`: Used in traditional Spring MVC with server-rendered HTML views (Thymeleaf, JSP). Exception handler methods typically return a `ModelAndView` representing an error HTML template.
- `@RestControllerAdvice`: A composite annotation combining `@ControllerAdvice` and `@ResponseBody`. It indicates that the return value of `@ExceptionHandler` methods should be bound directly to the web response body and serialized to JSON.
</details>

<details>
<summary><strong>7. How can you optimize the performance of high-throughput business exceptions?</strong></summary>

The primary overhead of Java exceptions is the native JVM stack frame traversal performed by `Throwable.fillInStackTrace()`.
For expected, high-frequency domain exceptions (such as `InvalidCredentialsException` or `RateLimitExceededException`), you can override `fillInStackTrace()` to do nothing:
```java
@Override
public synchronized Throwable fillInStackTrace() { return this; }
```
This eliminates the stack walk overhead, making exception instantiation virtually as fast as allocating a standard Java object.
</details>

<details>
<summary><strong>8. Why should you never use HTTP 200 OK for business error responses?</strong></summary>

1. **Caching Violations**: Browsers, CDNs (Cloudflare), and HTTP proxies will cache the "error" response as a valid page, serving an error screen to future users.
2. **Monitoring & Alerting Failure**: Site Reliability Engineering (SRE) systems track HTTP 4xx and 5xx error rates to measure SLOs and trigger alerts. Returning 200 OK masks outages.
3. **API Standards Compliance**: Violates HTTP specification semantics.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Create a Domain Exception with Error Code</strong></summary>

**Objective**: Write a custom unchecked `AccountSuspendedException` containing a suspended timestamp and reason.

**Solution**:
```java
public class AccountSuspendedException extends RuntimeException {
    private final String accountId;
    private final Instant suspendedAt;

    public AccountSuspendedException(String accountId, Instant suspendedAt, String reason) {
        super("Account " + accountId + " was suspended on " + suspendedAt + ": " + reason);
        this.accountId = accountId;
        this.suspendedAt = suspendedAt;
    }

    public String getAccountId() { return accountId; }
    public Instant getSuspendedAt() { return suspendedAt; }
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Implement an Exception Handler for Resource Not Found</strong></summary>

**Objective**: Write a `@RestControllerAdvice` method that catches `AccountNotFoundException` and returns an HTTP 404 `ProblemDetail`.

**Solution**:
```java
@RestControllerAdvice
public class AccountErrorHandler {

    @ExceptionHandler(AccountNotFoundException.class)
    public ProblemDetail handleAccountNotFound(AccountNotFoundException ex) {
        ProblemDetail problem = ProblemDetail.forStatusAndDetail(HttpStatus.NOT_FOUND, ex.getMessage());
        problem.setTitle("Account Not Found");
        problem.setType(URI.create("https://api.example.com/errors/account-not-found"));
        return problem;
    }
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Format Validation Errors into RFC 7807 Field Map</strong></summary>

**Objective**: Override `handleMethodArgumentNotValid` to map all violated DTO fields into a `Map<String, String>` inside `ProblemDetail`.

**Solution**:
```java
@Override
protected ResponseEntity<Object> handleMethodArgumentNotValid(
        MethodArgumentNotValidException ex, 
        HttpHeaders headers, 
        HttpStatusCode status, 
        WebRequest request) {

    ProblemDetail problem = ProblemDetail.forStatusAndDetail(status, "Validation failed for request parameters");
    problem.setTitle("Bad Request");

    Map<String, String> fieldErrors = ex.getBindingResult().getFieldErrors().stream()
            .collect(Collectors.toMap(
                    FieldError::getField,
                    FieldError::getDefaultMessage,
                    (existing, next) -> existing
            ));

    problem.setProperty("errors", fieldErrors);
    return ResponseEntity.status(status).body(problem);
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Build a High-Performance Stackless Exception</strong></summary>

**Objective**: Implement a `RateLimitExceededException` that skips stack trace generation to maximize performance under high load.

**Solution**:
```java
public class RateLimitExceededException extends RuntimeException {
    public RateLimitExceededException(String message) {
        super(message);
    }

    @Override
    public synchronized Throwable fillInStackTrace() {
        // Return this without calling native fillInStackTrace()
        return this;
    }
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Implement Global Uncaught Exception Sanitization with MDC Trace Injection</strong></summary>

**Objective**: Write an exception handler for `Throwable.class` that logs the full stacktrace with MDC context and returns a sanitized HTTP 500 `ProblemDetail` with `traceId`.

**Solution**:
```java
@RestControllerAdvice
public class SecuritySanitizingErrorHandler {

    private static final Logger log = LoggerFactory.getLogger(SecuritySanitizingErrorHandler.class);

    @ExceptionHandler(Throwable.class)
    public ProblemDetail handleUncaughtFatalError(Throwable ex) {
        String traceId = MDC.get("traceId");
        if (traceId == null || traceId.isBlank()) {
            traceId = UUID.randomUUID().toString();
        }

        // Log the real underlying error internally with stacktrace
        log.error("Unhandled system error occurred. traceId={}", traceId, ex);

        // Sanitize output for external consumers
        ProblemDetail problem = ProblemDetail.forStatusAndDetail(
                HttpStatus.INTERNAL_SERVER_ERROR,
                "A system error occurred. Please quote the traceId when reporting this issue."
        );
        problem.setTitle("Internal Server Error");
        problem.setProperty("traceId", traceId);
        problem.setProperty("timestamp", Instant.now());
        return problem;
    }
}
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Write an Integration Test Asserting RFC 7807 Problem Detail Format</strong></summary>

**Objective**: Use `MockMvc` to verify that submitting an invalid payload returns `application/problem+json` with expected title and status fields.

**Solution**:
```java
@WebMvcTest(controllers = CustomerController.class)
class CustomerControllerValidationTest {

    @Autowired private MockMvc mockMvc;

    @Test
    void shouldReturnRfc7807ProblemDetailOnInvalidInput() throws Exception {
        mockMvc.perform(post("/api/customers")
                .contentType(MediaType.APPLICATION_JSON)
                .content("""
                    {"name": "", "email": "invalid-email-address"}
                    """))
                .andExpect(status().isBadRequest())
                .andExpect(header().string("Content-Type", "application/problem+json"))
                .andExpect(jsonPath("$.title").value("Validation Failure"))
                .andExpect(jsonPath("$.status").value(400))
                .andExpect(jsonPath("$.invalidFields.name").exists())
                .andExpect(jsonPath("$.invalidFields.email").exists());
    }
}
```
</details>

---

## 12. Summary & Cheat Sheet

```
+----------------------------------------------------------------------------------------------------+
| ERROR HANDLING ARCHITECTURE IN SPRING BOOT CHEAT SHEET                                             |
+----------------------------------------------------------------------------------------------------+
| Component           | Spring Implementation                     | Primary Responsibility           |
+---------------------+-------------------------------------------+----------------------------------+
| RFC 7807 Model      | org.springframework.http.ProblemDetail    | Standardized machine-readable    |
|                     |                                           | error payload format             |
+---------------------+-------------------------------------------+----------------------------------+
| Global Interceptor  | @RestControllerAdvice                     | Centralizes exception handling;  |
|                     | extends ResponseEntityExceptionHandler    | eliminates controller try-catch  |
+---------------------+-------------------------------------------+----------------------------------+
| Handler Method      | @ExceptionHandler(MyException.class)      | Maps specific Java exception to  |
|                     |                                           | target HTTP status code & body   |
+---------------------+-------------------------------------------+----------------------------------+
| Observability       | Span.current().getSpanContext().getTraceId| Injects distributed trace ID for |
|                     | or MDC.get("traceId")                     | fast production log correlation  |
+---------------------+-------------------------------------------+----------------------------------+
| High-Perf Exception | Override fillInStackTrace() { return this;}| 10x faster exception creation    |
|                     |                                           | for hot-path business rejections |
+---------------------+-------------------------------------------+----------------------------------+
| Security Rule       | Never leak stacktraces or SQL in 500 JSON | Prevents OWASP info disclosure   |
+----------------------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [RFC 7807: Problem Details for HTTP APIs](https://datatracker.ietf.org/doc/html/rfc7807)
- [RFC 9457: Problem Details for HTTP APIs (Updated Specification)](https://datatracker.ietf.org/doc/html/rfc9457)
- [Spring Framework 6 Reference: Error Responses & ProblemDetail](https://docs.spring.io/spring-framework/reference/web/webmvc/mvc-ann-rest-exceptions.html)
- [OWASP Top 10: Security Misconfiguration & Error Handling](https://owasp.org/Top10/A05_2021-Security_Misconfiguration/)
- [Baeldung: Exception Handling in Spring MVC](https://www.baeldung.com/exception-handling-for-rest-with-spring)
