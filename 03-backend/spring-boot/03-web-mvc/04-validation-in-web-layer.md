# Validation in the Web Layer (Jakarta Validation & Spring MVC)

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | Controllers and Request Mapping, Jakarta Bean Validation (JSR 380) |
| **Estimated Reading Time** | 25 minutes |
| **Status** | Production-Ready |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Baseline** | OpenJDK 21 LTS / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | Hibernate Validator 8.0, OpenJDK 21.0.2 |

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
9. [Comparison: `@Valid` vs `@Validated`](#9-comparison-valid-vs-validated)
10. [Interview Questions & Deep Dives](#10-interview-questions--deep-dives)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference](#12-summary--quick-reference)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

In enterprise web services, never trust client input. Untrusted, malformed, or hostile inputs lead to SQL injections, deserialization flaws, data corruption, and catastrophic system crashes.

### The Problem
Manually checking inputs inside every controller method:
1. Clutters controllers with repetitive `if (dto.name() == null || dto.name().isBlank())` logic.
2. Yields inconsistent error formats and error codes across different endpoints.
3. Leaves nested object structures, lists, and query parameters susceptible to unvalidated states.

### The Solution: Declarative Web Layer Validation
Spring Boot integrates Jakarta Bean Validation (Hibernate Validator) directly into the `DispatcherServlet` pipeline. By decorating DTOs and record components with constraints (`@NotNull`, `@Size`, `@Email`, `@Pattern`, `@Positive`) and triggering them with `@Valid` or `@Validated`, Spring executes validation automatically *before* handler methods run. Any constraint violation throws a standard exception (`MethodArgumentNotValidException` or `ConstraintViolationException`) that can be transformed into RFC 9457 `ProblemDetail` responses.

---

## 2. Core Concepts

### Mental Model & Execution Flow
```
                     Incoming HTTP POST /orders (JSON Payload)
                                       │
                                       ▼
                       RequestResponseBodyMethodProcessor
                       (Deserializes JSON -> OrderDto)
                                       │
                                       ▼
                       Trigger Validation: @Valid / @Validated
                                       │
                                       ▼
                   Validator (LocalValidatorFactoryBean)
                     Validates fields, records, and cascades
                                       │
                     ┌─────────────────┴─────────────────┐
                     ▼                                   ▼
              No Violations                       Violations Found
                     │                                   │
                     ▼                                   ▼
         Invokes Controller Method        MethodArgumentNotValidException
       OrderController.createOrder()     (Contains BindingResult with all field errors)
                                                         │
                                                         ▼
                                             @RestControllerAdvice
                                        Maps to RFC 9457 ProblemDetail (400)
```

### Invariants
1. **Validation Precedes Method Execution**: If a `@Valid @RequestBody` fails validation, the controller method is **never executed**.
2. **Cascading Requirement**: Nested child objects and collections are **not** validated unless annotated with `@Valid`.
3. **Difference in Exceptions**:
   - Validation failure on `@RequestBody` / `@ModelAttribute` $\rightarrow$ throws `MethodArgumentNotValidException` (returns `BindingResult`).
   - Validation failure on `@RequestParam` / `@PathVariable` on a `@Validated` class $\rightarrow$ throws `ConstraintViolationException`.

---

## 3. How It Works Internally

### Validation Integration in `DataBinder`
1. During startup, `ValidationAutoConfiguration` creates a `LocalValidatorFactoryBean`, configuring the underlying Hibernate Validator engine.
2. In `RequestResponseBodyMethodProcessor`, after Jackson deserializes the payload into the target object, Spring checks if the parameter is annotated with `@Valid` or an annotation starting with `Valid` (such as Spring's `@Validated`).
3. It obtains the `WebDataBinder` and calls `binder.validate()`.
4. The validator evaluates all constraint annotations on the object graph.
5. If errors are present and the controller method does **not** include `BindingResult` as an adjacent parameter, Spring immediately throws `MethodArgumentNotValidException`.

---

## 4. Syntax & Configuration

### Dependency Setup (`pom.xml`)
```xml
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-starter-validation</artifactId>
</dependency>
```

### Common Jakarta Constraint Annotations

| Constraint | Applicable Types | Semantics |
| :--- | :--- | :--- |
| `@NotNull` | Any Object | Must not be `null` |
| `@NotEmpty` | `CharSequence`, `Collection`, `Map`, `Array` | Not `null` and `size > 0` |
| `@NotBlank` | `CharSequence` | Not `null`, trimmed length $> 0$ |
| `@Size(min, max)` | `CharSequence`, `Collection`, `Map` | Length/size within bounds |
| `@Min` / `@Max` | Numeric types | Minimum / maximum numeric value |
| `@Positive` / `@PositiveOrZero` | Numeric types | Strictly positive / $\ge 0$ |
| `@Email` | `CharSequence` | Valid email syntax |
| `@Pattern(regexp)` | `CharSequence` | Matches regular expression |
| `@Past` / `@Future` | Date / Time types | In past or future relative to now |

---

## 5. Practical Examples

### Example 1: Robust DTO Validation Using Java Records

```java
package com.example.web.validation.dto;

import jakarta.validation.constraints.*;
import java.math.BigDecimal;
import java.time.LocalDate;
import java.util.List;

public record CreateCustomerRequest(
        @NotBlank(message = "Customer name must not be blank")
        @Size(min = 2, max = 100, message = "Name must be between 2 and 100 characters")
        String fullName,

        @NotBlank(message = "Email is required")
        @Email(message = "Must be a well-formed email address")
        String email,

        @NotNull(message = "Date of birth is required")
        @Past(message = "Date of birth must be in the past")
        LocalDate dateOfBirth,

        @NotNull(message = "Initial deposit is required")
        @Positive(message = "Initial deposit must be greater than zero")
        BigDecimal initialDeposit,

        @NotNull(message = "Addresses list cannot be null")
        @Size(min = 1, message = "At least one address must be provided")
        List<@Valid AddressDto> addresses // Cascaded validation onto collection elements
) {}

public record AddressDto(
        @NotBlank String street,
        @NotBlank String city,
        @Pattern(regexp = "^[0-9]{5}(?:-[0-9]{4})?$", message = "Invalid US ZIP code format")
        String postalCode
) {}
```

### Example 2: Controller with `@Valid` and Cascaded Parameter Validation

```java
package com.example.web.validation.controller;

import com.example.web.validation.dto.CreateCustomerRequest;
import jakarta.validation.Valid;
import jakarta.validation.constraints.Min;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.validation.annotation.Validated;
import org.springframework.web.bind.annotation.*;

import java.net.URI;
import java.util.UUID;

@RestController
@RequestMapping("/api/v1/customers")
@Validated // Required for method-level validation on @PathVariable and @RequestParam
public class CustomerValidationController {

    @PostMapping
    @ResponseStatus(HttpStatus.CREATED)
    public ResponseEntity<String> createCustomer(@Valid @RequestBody CreateCustomerRequest request) {
        UUID customerId = UUID.randomUUID();
        return ResponseEntity.created(URI.create("/api/v1/customers/" + customerId))
                .body("Customer created with ID: " + customerId);
    }

    @GetMapping("/{id}")
    public String getCustomerById(
            @PathVariable @Min(value = 1, message = "Customer ID must be greater than zero") Long id) {
        return "Customer details for ID: " + id;
    }
}
```

### Example 3: Enterprise Custom Constraint Annotation with Database Lookup

```java
package com.example.web.validation.custom;

import jakarta.validation.Constraint;
import jakarta.validation.ConstraintValidator;
import jakarta.validation.ConstraintValidatorContext;
import jakarta.validation.Payload;
import org.springframework.stereotype.Component;

import java.lang.annotation.*;

@Documented
@Constraint(validatedBy = UniqueEmailValidator.class)
@Target({ElementType.FIELD, ElementType.RECORD_COMPONENT, ElementType.PARAMETER})
@Retention(RetentionPolicy.RUNTIME)
public @interface UniqueEmail {
    String message() default "Email address is already in use";
    Class<?>[] groups() default {};
    Class<? extends Payload>[] payload() default {};
}

@Component
class UniqueEmailValidator implements ConstraintValidator<UniqueEmail, String> {

    // Spring injects dependencies into ConstraintValidators automatically!
    @Override
    public boolean isValid(String email, ConstraintValidatorContext context) {
        if (email == null || email.isBlank()) {
            return true; // Let @NotBlank handle null/empty checks
        }
        // In real apps, query UserRepository.existsByEmail(email)
        return !email.equalsIgnoreCase("admin@forbidden.com");
    }
}
```

### Example 4: Anti-Pattern vs Production Fix

#### Anti-Pattern: Catching `BindingResult` Inside Every Controller Method
```java
// ANTI-PATTERN: Boilerplate error inspection in every controller
@PostMapping("/users")
public ResponseEntity<?> createUser(@Valid @RequestBody UserDto dto, BindingResult result) {
    if (result.hasErrors()) {
        // Inconsistent custom map, duplicated across dozens of endpoints
        Map<String, String> errors = new HashMap<>();
        result.getFieldErrors().forEach(e -> errors.put(e.getField(), e.getDefaultMessage()));
        return ResponseEntity.badRequest().body(errors);
    }
    return ResponseEntity.ok(userService.save(dto));
}
```

#### Production Fix: Standardized RFC 9457 `ProblemDetail` via `@RestControllerAdvice`
```java
// PRODUCTION FIX: Remove BindingResult from controller; handle centrally in advice
package com.example.web.validation.error;

import org.springframework.http.HttpStatus;
import org.springframework.http.ProblemDetail;
import org.springframework.validation.FieldError;
import org.springframework.web.bind.MethodArgumentNotValidException;
import org.springframework.web.bind.annotation.ExceptionHandler;
import org.springframework.web.bind.annotation.RestControllerAdvice;

import java.net.URI;
import java.time.Instant;
import java.util.Map;
import java.util.stream.Collectors;

@RestControllerAdvice
public class ValidationExceptionHandler {

    @ExceptionHandler(MethodArgumentNotValidException.class)
    public ProblemDetail handleValidationException(MethodArgumentNotValidException ex) {
        ProblemDetail problem = ProblemDetail.forStatusAndDetail(
                HttpStatus.BAD_REQUEST,
                "Request validation failed on one or more attributes."
        );
        problem.setTitle("Validation Failed");
        problem.setType(URI.create("https://api.example.com/errors/validation-failed"));
        problem.setProperty("timestamp", Instant.now());

        Map<String, String> invalidFields = ex.getBindingResult()
                .getFieldErrors()
                .stream()
                .collect(Collectors.toMap(
                        FieldError::getField,
                        FieldError::getDefaultMessage,
                        (existing, replacement) -> existing // Prevent collision on duplicate field errors
                ));

        problem.setProperty("invalidParams", invalidFields);
        return problem;
    }
}
```

### Example 5: Validation Groups for Selective Multi-Stage Validation

```java
package com.example.web.validation.groups;

import jakarta.validation.GroupSequence;
import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.NotNull;
import jakarta.validation.constraints.Null;
import org.springframework.validation.annotation.Validated;
import org.springframework.web.bind.annotation.*;

public interface OnCreate {}
public interface OnUpdate {}

public record UserProfileDto(
        @Null(groups = OnCreate.class, message = "ID must be null during user creation")
        @NotNull(groups = OnUpdate.class, message = "ID is required during update")
        Long id,

        @NotBlank(groups = {OnCreate.class, OnUpdate.class})
        String username
) {}

@RestController
@RequestMapping("/api/v1/profiles")
public class UserProfileController {

    // Validates ONLY OnCreate constraints
    @PostMapping
    public String createProfile(@Validated(OnCreate.class) @RequestBody UserProfileDto dto) {
        return "Created profile for: " + dto.username();
    }

    // Validates ONLY OnUpdate constraints
    @PutMapping
    public String updateProfile(@Validated(OnUpdate.class) @RequestBody UserProfileDto dto) {
        return "Updated profile ID: " + dto.id();
    }
}
```

---

## 6. Best Practices

1. **Remove `BindingResult` from Controllers**: Do not pass `BindingResult` as a parameter. Letting Spring throw `MethodArgumentNotValidException` enables a single, consistent, centralized exception handler.
2. **Use Standard RFC 9457 `ProblemDetail`**: Format validation errors using Spring Boot 3's built-in `ProblemDetail` specification.
3. **Always Cascade with `@Valid` on Nested Objects**: Remember that child collections (`List<@Valid ItemDto>`) and nested objects must have `@Valid` or their constraints will be silently skipped.
4. **Use `@Validated` at Class Level for Path Variables and Query Params**: `@PathVariable` and `@RequestParam` constraints (`@Min`, `@Size`) are only evaluated if the containing class has `@Validated`.
5. **Separate Structural Validation from Business Rules**: Use Jakarta Validation for syntactic/format checks (e.g., email format, not blank). Put complex domain state checks inside domain service entities.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Omitting `@Valid` on Nested Child Objects** | Hibernate Validator skips nested properties unless `@Valid` is explicitly present on the field or record component. | Add `@Valid` to nested objects and collections (e.g., `List<@Valid ChildDto>`). |
| **Expecting Constraints on `@PathVariable` without Class-Level `@Validated`** | `@Min(1) @PathVariable Long id` does nothing unless the controller class is annotated with `@Validated`. | Add `@Validated` to the `@RestController` class. |
| **Using `@NotNull` on String Instead of `@NotBlank`** | An empty string (`""`) or whitespace string (`"   "`) passes `@NotNull` and `@NotEmpty`. | Use `@NotBlank` for all textual user input. |
| **Swallowing Exceptions with `BindingResult`** | If `BindingResult` is in the method signature, Spring does not throw an exception; if you forget to check `hasErrors()`, invalid data proceeds into the service! | Remove `BindingResult` and use `@ExceptionHandler`. |
| **Putting Expensive DB Queries in Constraint Validators** | Synchronous database lookups in validators slow down API response times and can cause database connection pool exhaustion. | Keep validators fast; reserve heavy checks for the service layer or asynchronous pipelines. |
| **Missing `spring-boot-starter-validation` Dependency** | Since Spring Boot 2.3+, validation annotations are not included transitively in `spring-boot-starter-web`. | Explicitly add `spring-boot-starter-validation` to `pom.xml`. |

---

## 8. Performance & Security Considerations

### Regex Denial of Service (ReDoS)
Using unanchored or complex catastrophic backtracking regular expressions inside `@Pattern(regexp = "...")` allows attackers to send malicious strings that consume 100% CPU on the worker thread. Keep regex patterns linear and tested with tools like OWASP Regex Analysis.

### Validator Injection Invariants
Spring wires constraint validator instances as Spring-managed beans. Any bean injected into a `ConstraintValidator` should be thread-safe, as validator instances may be cached by the underlying validation provider.

---

## 9. Comparison: `@Valid` vs `@Validated`

| Feature | `@Valid` (Jakarta Standard) | `@Validated` (Spring-Specific) |
| :--- | :--- | :--- |
| **Origin Specification** | Jakarta Bean Validation (JSR 380) | Spring Framework |
| **Validation Groups Support** | No | Yes (specifies group classes) |
| **Target Locations** | Fields, method parameters, record components | Classes, methods, parameters |
| **Method-Level Interception** | Requires third-party AOP | Supported natively via `MethodValidationPostProcessor` |
| **Usage on Request Body** | Fully supported | Fully supported |
| **Usage on Controller Class** | Not supported | Required for validating `@PathVariable` / `@RequestParam` |

---

## 10. Interview Questions & Deep Dives

<details>
<summary><strong>Q1: Why does `@Min(1) @PathVariable Long id` not work unless `@Validated` is on the controller class?</strong></summary>

**Answer:**
`@Valid` on `@RequestBody` is handled directly by `RequestResponseBodyMethodProcessor` during argument resolution. In contrast, method parameters like `@PathVariable` and `@RequestParam` are not validated by argument resolvers. Adding `@Validated` to the class level triggers Spring's `MethodValidationPostProcessor`, which creates an AOP proxy around the controller to intercept method calls and validate raw parameters via `ExecutableValidator`.
</details>

<details>
<summary><strong>Q2: What is the difference between `MethodArgumentNotValidException` and `ConstraintViolationException`?</strong></summary>

**Answer:**
- `MethodArgumentNotValidException`: Thrown when validation fails on a `@Valid` or `@Validated` complex object argument (like a `@RequestBody` DTO). It contains a `BindingResult` with all `FieldError` and `ObjectError` items.
- `ConstraintViolationException`: Thrown by Spring's AOP method validation proxy when a primitive method parameter (e.g. `@PathVariable` or `@RequestParam`) fails validation. It contains a set of `ConstraintViolation<?>` objects.
</details>

<details>
<summary><strong>Q3: How do you validate elements inside a `List<AddressDto>` collection in a Java record?</strong></summary>

**Answer:**
Use type-use annotations: `List<@Valid AddressDto> addresses`. Jakarta Validation 2.0+ supports container element validation, ensuring that every element in the collection is individually inspected against its constraints.
</details>

<details>
<summary><strong>Q4: Can a custom `ConstraintValidator` inject Spring-managed service or repository beans?</strong></summary>

**Answer:**
Yes. Spring Boot configures `LocalValidatorFactoryBean` with a `SpringConstraintValidatorFactory`. When Hibernate Validator creates an instance of a `ConstraintValidator`, Spring manages its lifecycle, allowing standard constructor or `@Autowired` injection of repositories and configuration beans.
</details>

<details>
<summary><strong>Q5: What are Validation Groups and when should they be used?</strong></summary>

**Answer:**
Validation groups allow applying subsets of constraints depending on the use case. For example, during resource creation (`OnCreate`), an `id` field must be `@Null`, but during resource update (`OnUpdate`), the `id` field must be `@NotNull`. You pass the group class to `@Validated(OnCreate.class)`.
</details>

<details>
<summary><strong>Q6: How does Spring Boot 3 format validation errors using RFC 9457?</strong></summary>

**Answer:**
Spring Boot 3 introduces `ProblemDetail`. When validation fails, a `@RestControllerAdvice` can instantiate `ProblemDetail.forStatusAndDetail(HttpStatus.BAD_REQUEST, "Validation failed")`, attach a map of field error messages via `.setProperty("errors", fieldMap)`, and return it with MIME type `application/problem+json`.
</details>

<details>
<summary><strong>Q7: What is the difference between `@NotNull`, `@NotEmpty`, and `@NotBlank`?</strong></summary>

**Answer:**
- `@NotNull`: Value must not be `null` (allows `""` and `" "`).
- `@NotEmpty`: Value must not be `null` and its length or size must be $> 0$ (allows `" "`).
- `@NotBlank`: Value must not be `null` and must contain at least one non-whitespace character after trimming (rejects `null`, `""`, and `" "`).
</details>

<details>
<summary><strong>Q8: How can you perform cross-field validation (e.g., verifying `password` matches `confirmPassword`)?</strong></summary>

**Answer:**
Apply a custom constraint annotation at the class/record level rather than on individual fields. The custom `ConstraintValidator` receives the entire object, allowing it to compare multiple fields simultaneously.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Basic Record Validation</strong></summary>

**Task**: Write a record `LoginRequest` with `username` (not blank, between 3 and 20 chars) and `password` (not blank, minimum 8 chars).

```java
// Solution:
public record LoginRequest(
        @NotBlank @Size(min = 3, max = 20) String username,
        @NotBlank @Size(min = 8) String password
) {}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Add Method Parameter Validation</strong></summary>

**Task**: Annotate a controller method so that `GET /api/items/{id}` rejects `id <= 0` with HTTP 400.

```java
// Solution:
@RestController
@Validated
public class ItemController {
    @GetMapping("/api/items/{id}")
    public String getItem(@PathVariable @Positive(message = "ID must be positive") Long id) {
        return "Item " + id;
    }
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Cross-Field Password Match Validator</strong></summary>

**Task**: Implement a class-level constraint `@PasswordMatch` that verifies `newPassword` equals `confirmPassword` on a `PasswordResetRequest` record.

```java
// Solution:
@Target(ElementType.TYPE)
@Retention(RetentionPolicy.RUNTIME)
@Constraint(validatedBy = PasswordMatchValidator.class)
public @interface PasswordMatch {
    String message() default "Passwords do not match";
    Class<?>[] groups() default {};
    Class<? extends Payload>[] payload() default {};
}

public record PasswordResetRequest(String newPassword, String confirmPassword) {}

public class PasswordMatchValidator implements ConstraintValidator<PasswordMatch, PasswordResetRequest> {
    @Override
    public boolean isValid(PasswordResetRequest req, ConstraintValidatorContext context) {
        if (req == null || req.newPassword() == null) return true;
        return req.newPassword().equals(req.confirmPassword());
    }
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Custom ProblemDetail Exception Handler</strong></summary>

**Task**: Write an `@ExceptionHandler(ConstraintViolationException.class)` that formats parameter violations into RFC 9457 `ProblemDetail`.

```java
// Solution:
@RestControllerAdvice
public class ConstraintExceptionHandler {
    @ExceptionHandler(ConstraintViolationException.class)
    public ProblemDetail handleConstraintViolation(ConstraintViolationException ex) {
        ProblemDetail problem = ProblemDetail.forStatusAndDetail(
                HttpStatus.BAD_REQUEST, "One or more method parameters failed validation.");
        problem.setTitle("Constraint Violation");
        Map<String, String> errors = ex.getConstraintViolations().stream()
                .collect(Collectors.toMap(
                        v -> v.getPropertyPath().toString(),
                        ConstraintViolation::getMessage,
                        (m1, m2) -> m1
                ));
        problem.setProperty("violations", errors);
        return problem;
    }
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Dynamic Payload Sanitization and Custom Validator</strong></summary>

**Task**: Implement a `@SanitizedText` constraint validator that strips HTML tags and checks that no unescaped script tags exist in user blog comment payloads.
</details>

<details>
<summary><strong>Exercise 6 (Hard): Multi-Tenant Conditional Validation Group Provider</strong></summary>

**Task**: Create a dynamic `GroupSequenceProvider` that applies strict European Union VAT tax validation constraints if `customer.country` is in the EU, and relaxed state tax constraints if the country is US.
</details>

---

## 12. Summary & Quick Reference

- **Core Starter**: `spring-boot-starter-validation`.
- **Trigger on Payloads**: `@Valid @RequestBody MyDto dto`.
- **Trigger on Params**: `@Validated` on Controller Class + `@Min` / `@Pattern` on `@PathVariable` / `@RequestParam`.
- **Cascading**: Always use `@Valid` on child collections and nested records.
- **Handling**: Catch `MethodArgumentNotValidException` centrally in `@RestControllerAdvice`.

---

## 13. Further Reading

- [Jakarta Bean Validation Specification (JSR 380)](https://beanvalidation.org/2.0/spec/)
- [Hibernate Validator Official Reference Guide](https://docs.jboss.org/hibernate/validator/8.0/reference/en-US/html_single/)
- [Spring Framework Validation Documentation](https://docs.spring.io/spring-framework/reference/core/validation.html)
- [RFC 9457: Problem Details for HTTP APIs](https://www.rfc-editor.org/rfc/rfc9457.html)
