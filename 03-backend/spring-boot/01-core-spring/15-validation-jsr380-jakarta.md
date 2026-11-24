# Validation with Jakarta Bean Validation (JSR-380) & Spring

> Master enterprise input validation: Jakarta Validation 3.0 annotations on Java 21 records, custom `@ConstraintValidator` implementations, cross-field validation, validation groups, method-level service enforcement with `@Validated`, and RFC 7807 problem details error mapping.

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [IoC Container & ApplicationContext Internals](file:///03-backend/spring-boot/01-core-spring/01-ioc-container-and-application-context.md), [First Application Walkthrough](file:///03-backend/spring-boot/00-setup/05-first-application-walkthrough.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.0.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | OpenJDK 25.0.1, Spring Boot 3.4.0, Hibernate Validator 8.0 |

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
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### Problem Statement
Unvalidated or poorly sanitized user inputs are the root cause of the majority of enterprise software failures: SQL injection, cross-site scripting (XSS), data corruption, and arithmetic overflows. Writing repetitive manual validation code (`if (dto.getEmail() == null || !dto.getEmail().contains("@"))`) in every controller and service is fragile, violates DRY, and leads to inconsistent error messages.

### Historical Evolution
- **Pre-JSR 303 (2000s)**: Custom validation frameworks or manual `if/else` checks scattered across controllers and services.
- **JSR-303 & JSR-380 (Bean Validation 1.0 & 2.0)**: Standardized declarative annotations (`javax.validation.*`).
- **Jakarta Bean Validation 3.0 (Spring Boot 3+)**: Migrated to `jakarta.validation.*`, deeply integrated with Hibernate Validator 8, Java 21 records, and Spring 6's RFC 7807 ProblemDetail error format.

### Real-World Relevance
Standardized validation guarantees that invalid data is rejected at the architecture's outer perimeter before triggering database transactions or business logic.

---

## 2. Core Concepts

### 1. Standard Jakarta Constraints
- `@NotNull`: CharSequence, Collection, Map or Array must not be null.
- `@NotEmpty`: Must not be null and size/length must be $> 0$.
- `@NotBlank`: String must not be null, length $> 0$, and must contain at least one non-whitespace character.
- `@Size(min = 2, max = 50)`: Enforces boundary size on Strings, Collections, or Maps.
- `@Min(value)` / `@Max(value)`: Numerical limits on integers and decimals.
- `@Email`: Validates email syntax.
- `@Pattern(regexp = "...")`: Regex validation.
- `@Positive` / `@PositiveOrZero` / `@Negative`: Numerical polarity checks.
- `@Past` / `@Future`: Date and time validation.

### 2. `@Valid` vs. `@Validated`
- **`@Valid` (`jakarta.validation.Valid`)**: Standard Jakarta annotation. Instructs the validator to validate the annotated object or trigger cascading validation on nested objects.
- **`@Validated` (`org.springframework.validation.annotation.Validated`)**: Spring-specific annotation.
  1. Enables **Validation Groups** (`@Validated(OnCreate.class)`).
  2. Enables **Method-Level Validation** when placed on service classes (intercepted by `MethodValidationPostProcessor`).

### 3. Custom Constraint Validators
Developers can create custom annotations meta-annotated with `@Constraint(validatedBy = MyValidator.class)` and implement `ConstraintValidator<A, T>`.

---

## 3. How It Works Internally

### Validation Interception Pipeline

```
A. WEB LAYER VALIDATION (SPRING MVC)
HTTP POST /api/users (JSON Payload)
               |
               v
[ DispatcherServlet ] ---> [ RequestMappingHandlerAdapter ]
                                     |
                         Jackson deserializes JSON into UserDto record
                                     |
                                     v
                 [ Validator (LocalValidatorFactoryBean) ]
                 - Validates @NotNull, @Email, @Size
                 - Gathers constraint violations
                                     |
               +---------------------+---------------------+
               | Valid                                     | Invalid
               v                                           v
[ UserController.createUser() ]             [ Throws MethodArgumentNotValidException ]
                                                           |
                                                           v
                                            [ @ExceptionHandler returns RFC 7807 ]
                                              HTTP 400 Bad Request with field errors

B. SERVICE LAYER METHOD VALIDATION (AOP PROXY)
Caller ---> [ CGLIB Proxy (MethodValidationInterceptor) ] ---> [ Target Service ]
                                     |
                         Inspects @Validated on class
                         Validates method parameters
                                     |
                         Invalid? -> Throws ConstraintViolationException
```

---

## 4. Syntax & Usage

### Validating a Java 21 Record
```java
public record CreateUserRequest(
    @NotBlank(message = "Username cannot be blank")
    @Size(min = 3, max = 30, message = "Username must be between 3 and 30 characters")
    String username,

    @NotBlank(message = "Email is required")
    @Email(message = "Invalid email format")
    String email,

    @Min(value = 18, message = "User must be at least 18 years old")
    int age
) {}
```

### Enabling Validation in Controller
```java
@RestController
@RequestMapping("/api/v1/users")
public class UserController {

    @PostMapping
    public ResponseEntity<Void> createUser(@Valid @RequestBody CreateUserRequest request) {
        // Only executes if request is 100% valid!
        return ResponseEntity.status(HttpStatus.CREATED).build();
    }
}
```

---

## 5. Practical Examples

### Example 1: Full-Featured Record DTO with Standard Constraints
```java
package com.example.demo.dto;

import jakarta.validation.Valid;
import jakarta.validation.constraints.*;

import java.math.BigDecimal;
import java.time.LocalDate;
import java.util.List;

public record OrderItemDto(
    @NotBlank String sku,
    @Positive int quantity,
    @Positive BigDecimal unitPrice
) {}

public record PlaceOrderDto(
    @NotBlank(message = "Customer ID is required")
    String customerId,

    @NotNull(message = "Delivery date must be provided")
    @Future(message = "Delivery date must be in the future")
    LocalDate requestedDeliveryDate,

    @NotEmpty(message = "Order must contain at least one item")
    @Valid // Cascades validation to each OrderItemDto in the list!
    List<OrderItemDto> items
) {}
```

### Example 2: Custom Constraint Annotation & Validator
```java
package com.example.demo.validation;

import jakarta.validation.Constraint;
import jakarta.validation.ConstraintValidator;
import jakarta.validation.ConstraintValidatorContext;
import jakarta.validation.Payload;

import java.lang.annotation.*;

// 1. Define the Constraint Annotation
@Target({ElementType.FIELD, ElementType.PARAMETER, ElementType.RECORD_COMPONENT})
@Retention(RetentionPolicy.RUNTIME)
@Constraint(validatedBy = StrongPasswordValidator.class)
@Documented
public @interface StrongPassword {
    String message() default "Password must be at least 8 characters long and contain at least one uppercase letter, one digit, and one special character";
    Class<?>[] groups() default {};
    Class<? extends Payload>[] payload() default {};
}

// 2. Implement the ConstraintValidator
class StrongPasswordValidator implements ConstraintValidator<StrongPassword, String> {

    private static final String PASSWORD_PATTERN = 
        "^(?=.*[0-9])(?=.*[a-z])(?=.*[A-Z])(?=.*[@#$%^&+=!])(?=\\S+$).{8,}$";

    @Override
    public boolean isValid(String password, ConstraintValidatorContext context) {
        if (password == null) {
            return false; // Let @NotNull handle null checks if required
        }
        return password.matches(PASSWORD_PATTERN);
    }
}
```

### Example 3: Validation Groups (Create vs. Update)
```java
package com.example.demo.groups;

import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.NotNull;
import jakarta.validation.constraints.Null;
import org.springframework.validation.annotation.Validated;
import org.springframework.web.bind.annotation.*;

import java.util.UUID;

// Group Marker Interfaces
public interface OnCreate {}
public interface OnUpdate {}

public record ProductDto(
    @Null(groups = OnCreate.class, message = "ID must be null on creation")
    @NotNull(groups = OnUpdate.class, message = "ID is mandatory on update")
    UUID id,

    @NotBlank(groups = {OnCreate.class, OnUpdate.class}, message = "Name is required")
    String name
) {}

@RestController
@RequestMapping("/api/products")
public class ProductController {

    // Validates ONLY OnCreate rules
    @PostMapping
    public void create(@Validated(OnCreate.class) @RequestBody ProductDto dto) {
        System.out.println("Created product: " + dto.name());
    }

    // Validates ONLY OnUpdate rules
    @PutMapping("/{id}")
    public void update(@PathVariable UUID id, @Validated(OnUpdate.class) @RequestBody ProductDto dto) {
        System.out.println("Updated product: " + dto.id());
    }
}
```

### Example 4: Cross-Field Class-Level Constraint
```java
package com.example.demo.crossfield;

import jakarta.validation.Constraint;
import jakarta.validation.ConstraintValidator;
import jakarta.validation.ConstraintValidatorContext;
import jakarta.validation.Payload;

import java.lang.annotation.*;

// Custom Class-Level Annotation
@Target(ElementType.TYPE)
@Retention(RetentionPolicy.RUNTIME)
@Constraint(validatedBy = FieldsMatchValidator.class)
@Documented
public @interface FieldsMatch {
    String message() default "Fields do not match";
    String field();
    String fieldMatch();
    Class<?>[] groups() default {};
    Class<? extends Payload>[] payload() default {};
}

// Target Record
@FieldsMatch(field = "password", fieldMatch = "confirmPassword", message = "Passwords must match")
public record PasswordResetDto(String password, String confirmPassword) {}

// Validator using Reflection
class FieldsMatchValidator implements ConstraintValidator<FieldsMatch, Object> {
    private String fieldName;
    private String fieldMatchName;

    @Override
    public void initialize(FieldsMatch constraintAnnotation) {
        this.fieldName = constraintAnnotation.field();
        this.fieldMatchName = constraintAnnotation.fieldMatch();
    }

    @Override
    public boolean isValid(Object value, ConstraintValidatorContext context) {
        try {
            Object fieldValue = value.getClass().getMethod(fieldName).invoke(value);
            Object fieldMatchValue = value.getClass().getMethod(fieldMatchName).invoke(value);

            boolean match = fieldValue != null && fieldValue.equals(fieldMatchValue);
            if (!match) {
                context.disableDefaultConstraintViolation();
                context.buildConstraintViolationWithTemplate(context.getDefaultConstraintMessageTemplate())
                    .addPropertyNode(fieldMatchName)
                    .addConstraintViolation();
            }
            return match;
        } catch (Exception e) {
            return false;
        }
    }
}
```

### Example 5: Method-Level Validation in Services
```java
package com.example.demo.service;

import jakarta.validation.constraints.Min;
import jakarta.validation.constraints.NotBlank;
import org.springframework.stereotype.Service;
import org.springframework.validation.annotation.Validated;

@Service
@Validated // MANDATORY: Enables MethodValidationInterceptor proxying on this service!
public class TransferService {

    public void transferMoney(
        @NotBlank(message = "Source account must be specified") String fromAccount,
        @NotBlank(message = "Destination account must be specified") String toAccount,
        @Min(value = 1, message = "Transfer amount must be at least $1") double amount
    ) {
        // If fromAccount is blank or amount < 1, Spring throws ConstraintViolationException immediately!
        System.out.printf("Transferred $%.2f from %s to %s%n", amount, fromAccount, toAccount);
    }
}
```

---

## 6. Best Practices

1. **Always Use `@NotBlank` for Strings**: Prefer `@NotBlank` over `@NotNull` or `@NotEmpty` for text inputs; it prevents strings composed solely of whitespace from passing.
2. **Remember the `@Valid` Cascade on Nested Objects**: Annotating a parent object with `@Valid` will NOT validate elements in a child collection unless the collection field itself is annotated with `@Valid`.
3. **Use Validation Groups for Dual-Purpose DTOs**: Use `@Validated(OnCreate.class)` and `@Validated(OnUpdate.class)` to prevent maintaining separate CreateDTO and UpdateDTO classes when fields are identical except for ID presence.
4. **Annotate Service Classes with `@Validated`**: Without `@Validated` at the class level, constraints placed on service method parameters are completely ignored by Spring.
5. **Keep Custom Validators Lightweight**: Do NOT make network calls (e.g., verifying user against external CRM) inside `ConstraintValidator.isValid()`. Validation should be synchronous, in-memory, and deterministic.
6. **Map Exceptions to RFC 7807 `ProblemDetail`**: Handle `MethodArgumentNotValidException` in `@RestControllerAdvice` to emit structured RFC 7807 JSON detailing all field violations.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Using `@NotNull` on Strings** | An empty string (`""`) or whitespace (`"   "`) satisfies `@NotNull`, allowing empty inputs. | Use `@NotBlank`. |
| **Omitting `@Valid` on nested objects** | Children in `List<Item>` or nested DTOs are not validated even if the parent has `@Valid`. | Add `@Valid` to the field/record component. |
| **Omitting `@Validated` on service classes** | Constraints on service method arguments are ignored because no AOP proxy interceptor is created. | Add `@Validated` to the `@Service` class. |
| **Confusing exception types** | Web validation throws `MethodArgumentNotValidException`, while service method validation throws `ConstraintViolationException`. | Handle both distinct exceptions in `@RestControllerAdvice`. |
| **Heavy I/O in `ConstraintValidator`** | Querying a remote database or REST API inside `isValid()` stalls request processing. | Perform business existence checks in the service layer, not in Bean Validation. |
| **Missing `@RequestBody` with `@Valid`** | Omitting `@RequestBody` results in empty DTOs that fail validation immediately. | Pair `@Valid @RequestBody MyDto dto`. |

---

## 8. Performance & Security Notes

### Input Sanitization against XSS & Injection
Bean validation validates length, format, and patterns, but does not sanitize HTML characters:
- Combine `@Pattern` or custom validators to reject unescaped `<script>` tags in text inputs.

### Pre-Compiled Regex Patterns
In custom validators, compile regular expressions as `static final Pattern` fields. Re-compiling regex inside `isValid()` on every request degrades throughput significantly.

---

## 9. Comparison / When to Use What

| Feature / Annotation | Scope | Origin | Best For |
| :--- | :--- | :--- | :--- |
| **`@Valid`** | Parameter / Field | Jakarta EE Standard | Web controller DTO validation, nested cascade |
| **`@Validated`** | Class / Parameter | Spring Framework | Activating service method validation, groups |
| **Spring `Validator`** | Programmatic Bean | Spring Framework | Complex dynamic multi-object stateful checks |
| **Jakarta Constraint** | Declarative Metadata | Jakarta Standard | Reusable format rules (`@Email`, `@Size`) |

---

## 10. Interview Questions

<details>
<summary><strong>Q1: What is the difference between `@Valid` and `@Validated`?</strong></summary>

**Answer:**  
- `@Valid` is a standard Jakarta EE annotation (`jakarta.validation.Valid`). It triggers validation on parameters and cascades validation to nested objects/collections.
- `@Validated` is a Spring-specific annotation (`org.springframework.validation.annotation.Validated`). It adds two capabilities:
  1. **Validation Groups**: Allows activating specific validation subsets (e.g., `@Validated(OnCreate.class)`).
  2. **Method-Level Validation**: When placed on a class (e.g., `@Service`), it enables Spring AOP proxy interception (`MethodValidationPostProcessor`) to validate method arguments and return values.
</details>

<details>
<summary><strong>Q2: What is the difference between `@NotNull`, `@NotEmpty`, and `@NotBlank`?</strong></summary>

**Answer:**  
- `@NotNull`: Value must not be `null` (permits `""` and `"   "`).
- `@NotEmpty`: Value must not be `null` and its length/size must be $> 0$ (permits `"   "`).
- `@NotBlank`: Value must not be `null`, must have length $> 0$, and must contain at least one non-whitespace character (rejects `""` and `"   "`). Applies strictly to character sequences.
</details>

<details>
<summary><strong>Q3: How do you cascade validation to nested objects in a List?</strong></summary>

**Answer:**  
Place the `@Valid` annotation on the collection field:
```java
public record OrderDto(
    @NotEmpty List<@Valid OrderItemDto> items // In Java 21 / Bean Validation 2.0+
) {}
```
Spring will iterate through every element in the collection and evaluate its constraint annotations.
</details>

<details>
<summary><strong>Q4: What exception is thrown when validation fails in a Spring MVC `@RestController`?</strong></summary>

**Answer:**  
Spring MVC throws **`MethodArgumentNotValidException`** when an `@Valid @RequestBody` argument fails validation. It contains a `BindingResult` listing all field errors and global errors.
</details>

<details>
<summary><strong>Q5: What exception is thrown when validation fails in a `@Service` method?</strong></summary>

**Answer:**  
When method-level validation fails on a service annotated with `@Validated`, Spring throws **`jakarta.validation.ConstraintViolationException`**.
</details>

<details>
<summary><strong>Q6: Can a custom `ConstraintValidator` inject Spring beans?</strong></summary>

**Answer:**  
Yes. Spring manages `ConstraintValidator` instances through `LocalValidatorFactoryBean`. Validators can use constructor dependency injection or autowiring to inject Spring-managed beans (e.g., `UserRepository`).
</details>

<details>
<summary><strong>Q7: What is a Class-Level Constraint and when would you use it?</strong></summary>

**Answer:**  
A class-level constraint is an annotation applied to the class or record itself (`@Target(ElementType.TYPE)`). It is used for **cross-field validation** where the validation rule depends on comparing multiple fields of the same object (e.g., verifying that `password` equals `confirmPassword`, or that `startDate` is before `endDate`).
</details>

<details>
<summary><strong>Q8: What are Validation Groups in Jakarta Bean Validation?</strong></summary>

**Answer:**  
Validation Groups allow categorizing constraint annotations into logical sets using marker interfaces (e.g., `OnCreate`, `OnUpdate`). Constraints without an explicit group belong to the `Default` group. By passing a group to `@Validated(OnCreate.class)`, only constraints tagged with that group are evaluated during the operation.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Basic DTO Constraints (Easy)</strong></summary>

**Task:**  
Create a `UserRegistrationDto` record with `username` (not blank, 3-20 chars), `email` (valid email format), and `age` (min 18). Write a `@WebMvcTest` asserting that invalid payloads return HTTP 400 Bad Request.
</details>

<details>
<summary><strong>Exercise 2: Implement a Custom `@PostalCode` Validator (Easy)</strong></summary>

**Task:**  
Create a custom constraint annotation `@ValidPostalCode` and validator that checks if a string matches the format `^[0-9]{5}(-[0-9]{4})?$`. Verify with valid and invalid values.
</details>

<details>
<summary><strong>Exercise 3: Validation Groups Implementation (Medium)</strong></summary>

**Task:**  
Create an `AccountDto` where `id` must be null for creation (`OnCreate`), but mandatory for update (`OnUpdate`). Build two controller endpoints testing both groups.
</details>

<details>
<summary><strong>Exercise 4: Cross-Field Date Range Validator (Medium)</strong></summary>

**Task:**  
Implement a class-level constraint `@ValidDateRange` on a record `EventDto(LocalDate startDate, LocalDate endDate)`. Assert that `startDate` is strictly before `endDate`.
</details>

<details>
<summary><strong>Exercise 5: Service Method Validation (Hard)</strong></summary>

**Task:**  
Create a service class with `@Validated`. Define a method `calculateDiscount(@Min(1) int count, @DecimalMin("0.01") double price)`. Assert that calling the method with invalid parameters throws `ConstraintViolationException`.
</details>

<details>
<summary><strong>Exercise 6: RFC 7807 Problem Details Controller Advice (Hard)</strong></summary>

**Task:**  
Write a `@RestControllerAdvice` that catches `MethodArgumentNotValidException` and returns a standard RFC 7807 `ProblemDetail` with a map of field names to error messages under the `"invalidParams"` extension property.
</details>

---

## 12. Summary / Cheat Sheet

| Annotation | Target | Purpose |
| :--- | :--- | :--- |
| **`@NotBlank`** | String field / component | Not null, length > 0, non-whitespace |
| **`@NotNull`** | Any object | Not null |
| **`@Valid`** | Parameter / Field | Triggers validation / cascades to nested objects |
| **`@Validated`** | Class / Parameter | Enables service method validation & groups |
| **`@Constraint`** | Custom annotation | Links annotation to `ConstraintValidator` class |
| **`MethodArgumentNotValidException`** | Controller error | Thrown when `@Valid @RequestBody` fails |
| **`ConstraintViolationException`** | Service error | Thrown when `@Validated` service method fails |

---

## 13. Further Reading
- [Official Jakarta Bean Validation 3.0 Specification](https://jakarta.ee/specifications/bean-validation/3.0/)
- [Hibernate Validator 8.0 Reference Guide](https://docs.jboss.org/hibernate/validator/8.0/reference/en-US/html_single/)
- [RFC 7807: Problem Details for HTTP APIs](https://datatracker.ietf.org/doc/html/rfc7807)
