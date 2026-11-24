# Type Conversion & Formatting SPI: Converters, Formatters & PropertyEditors

> A deep architectural analysis of Spring's type conversion subsystem: `ConversionService`, `Converter<S, T>`, `GenericConverter`, `Formatter<T>`, `@DateTimeFormat`, custom converters in Spring Boot, and why legacy `PropertyEditor` singletons cause catastrophic concurrency bugs.

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [IoC Container & ApplicationContext Internals](file:///03-backend/spring-boot/01-core-spring/01-ioc-container-and-application-context.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.0.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | OpenJDK 25.0.1, Spring Boot 3.4.0 |

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
In web and enterprise applications, incoming data arrives primarily as raw text strings: HTTP path variables (`/orders/{id}`), query parameters (`?date=2026-05-12`), form data, and configuration files (`timeout=500ms`). Java code, however, operates on strongly typed domain objects (`UUID`, `LocalDate`, `Duration`, `Money`). Without a unified, thread-safe type conversion subsystem, developers are forced to manually parse strings in every controller and service.

### Historical Evolution
- **JavaBeans Specification (1996)**: Introduced `java.beans.PropertyEditor`. Inherently stateful and not thread-safe; required creating new instances per request.
- **Spring 3.0 (2009)**: Introduced the modern, stateless, thread-safe `ConversionService` SPI (`Converter`, `GenericConverter`, `Formatter`).
- **Spring Boot (2014-Present)**: Deeply integrates `FormattingConversionService` into `@ConfigurationProperties` binding, Spring MVC parameter parsing, and SpEL evaluation.

### Real-World Relevance
Understanding Spring's type conversion engine allows developers to seamlessly bind complex custom types (strongly typed IDs, currency representations, encrypted tokens) from HTTP endpoints directly into controller method parameters.

---

## 2. Core Concepts

### 1. The `Converter<S, T>` Interface
A strongly typed, stateless SPI interface:
```java
@FunctionalInterface
public interface Converter<S, T> {
    T convert(S source);
}
```
Converts an object of source type `S` to target type `T`. It is thread-safe and can be registered as a shared singleton in the container.

### 2. `ConverterFactory<S, R>`
A factory for converting from source `S` to a family of target types extending `R` (e.g., `StringToEnumConverterFactory` converts any string to any Java `Enum`).

### 3. The `Formatter<T>` Interface
A specialized SPI focused strictly on **String-to-Object** and **Object-to-String** transformations with locale support:
- `String print(T object, Locale locale)`: Serializes object to localized string.
- `T parse(String text, Locale locale)`: Deserializes localized string into object.

### 4. `ConversionService`
The central runtime engine (`org.springframework.core.convert.ConversionService`). It acts as a facade managing a registry of converters and executes conversion requests:
```java
boolean canConvert(Class<?> sourceType, Class<?> targetType);
<T> T convert(Object source, Class<T> targetType);
```

---

## 3. How It Works Internally

### ConversionService Lookup and Execution Flow

```
HTTP Request: GET /api/transactions/TX-9988?amount=$1,250.50
                               |
                               v
               [ RequestMappingHandlerAdapter ]
               - Detects parameter: `TransactionId id`
               - Detects parameter: `@NumberFormat Money amount`
                               |
                               v
               [ FormattingConversionService ]
               1. Constructs ConverterCacheKey(String.class, TransactionId.class)
               2. Checks internal `Map<ConverterCacheKey, GenericConverter>`
                               |
                               v
+-------------------------------------------------------------------------+
|                  Converter Resolution Pipeline                          |
|  - Matches: `StringToTransactionIdConverter`                            |
|  - Invokes: `converter.convert("TX-9988")`                              |
|  - Matches: `CurrencyFormatter` (uses `@NumberFormat` metadata)         |
|  - Invokes: `formatter.parse("$1,250.50", Locale.US)`                   |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|       Invokes Controller Method with Strongly Typed Domain Objects      |
|  `getTransaction(TransactionId id, Money amount)`                       |
+-------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### Implementing a Custom Converter
```java
public record OrderId(UUID value) {}

@Component
public class StringToOrderIdConverter implements Converter<String, OrderId> {
    @Override
    public OrderId convert(String source) {
        return new OrderId(UUID.fromString(source));
    }
}
```

### Formatting with Annotations
```java
public class BookingDto {
    @DateTimeFormat(iso = DateTimeFormat.ISO.DATE)
    private LocalDate checkInDate;

    @NumberFormat(style = NumberFormat.Style.CURRENCY)
    private BigDecimal depositAmount;
}
```

---

## 5. Practical Examples

### Example 1: Strongly Typed Domain ID Converter
```java
package com.example.demo.converter;

import org.springframework.core.convert.converter.Converter;
import org.springframework.stereotype.Component;
import org.springframework.web.bind.annotation.*;

import java.util.UUID;

// Strongly typed domain record ID
public record CustomerId(UUID value) {
    public static CustomerId of(String raw) {
        return new CustomerId(UUID.fromString(raw));
    }
}

// Global Spring Converter
@Component
public class StringToCustomerIdConverter implements Converter<String, CustomerId> {
    @Override
    public CustomerId convert(String source) {
        return CustomerId.of(source);
    }
}

// Controller using domain ID directly in method signature
@RestController
@RequestMapping("/api/customers")
class CustomerController {

    @GetMapping("/{id}")
    public String getCustomer(@PathVariable CustomerId id) {
        // Path variable string automatically converted to CustomerId!
        return "Customer successfully resolved: " + id.value();
    }
}
```

### Example 2: Masked Credit Card Formatter with Locale Support
```java
package com.example.demo.formatter;

import org.springframework.format.Formatter;
import org.springframework.stereotype.Component;

import java.text.ParseException;
import java.util.Locale;

public record MaskedCard(String rawNumber, String lastFour) {}

@Component
public class CreditCardFormatter implements Formatter<MaskedCard> {

    @Override
    public MaskedCard parse(String text, Locale locale) throws ParseException {
        String sanitized = text.replaceAll("[\\s-]", "");
        if (sanitized.length() < 13 || sanitized.length() > 19) {
            throw new ParseException("Invalid card number length", 0);
        }
        String lastFour = sanitized.substring(sanitized.length() - 4);
        return new MaskedCard(sanitized, lastFour);
    }

    @Override
    public String print(MaskedCard card, Locale locale) {
        return "****-****-****-" + card.lastFour();
    }
}
```

### Example 3: Registering Converters Globally via `WebMvcConfigurer`
```java
package com.example.demo.config;

import com.example.demo.converter.StringToCustomerIdConverter;
import com.example.demo.formatter.CreditCardFormatter;
import org.springframework.context.annotation.Configuration;
import org.springframework.format.FormatterRegistry;
import org.springframework.web.servlet.config.annotation.WebMvcConfigurer;

@Configuration(proxyBeanMethods = false)
public class WebConversionConfig implements WebMvcConfigurer {

    private final StringToCustomerIdConverter customerIdConverter;
    private final CreditCardFormatter creditCardFormatter;

    public WebConversionConfig(StringToCustomerIdConverter customerIdConverter,
                               CreditCardFormatter creditCardFormatter) {
        this.customerIdConverter = customerIdConverter;
        this.creditCardFormatter = creditCardFormatter;
    }

    @Override
    public void addFormatters(FormatterRegistry registry) {
        // Explicitly register converters and formatters
        registry.addConverter(customerIdConverter);
        registry.addFormatter(creditCardFormatter);
    }
}
```

### Example 4: Legacy `PropertyEditor` Concurrency Anti-Pattern & Remediation
```java
// ANTI-PATTERN: Reusing a PropertyEditor instance as a Spring Singleton
@Component
public class DangerousDateEditor extends java.beans.PropertyEditorSupport {

    // FLAW: SimpleDateFormat is NOT thread-safe!
    // Storing state inside a singleton causes race conditions and corrupted dates under load!
    private final java.text.SimpleDateFormat format = new java.text.SimpleDateFormat("yyyy-MM-dd");

    @Override
    public void setAsText(String text) {
        try {
            setValue(format.parse(text)); // Shared state mutation across multiple threads!
        } catch (Exception e) {
            throw new IllegalArgumentException(e);
        }
    }
}

// REMEDIATION: Implement stateless, thread-safe Converter<String, LocalDate>
@Component
public class SafeDateConverter implements Converter<String, java.time.LocalDate> {

    // DateTimeFormatter is 100% immutable and thread-safe
    private static final java.time.format.DateTimeFormatter FORMATTER = 
        java.time.format.DateTimeFormatter.ISO_LOCAL_DATE;

    @Override
    public java.time.LocalDate convert(String source) {
        // Stateless execution: Zero shared state, perfectly safe for concurrent threads
        return java.time.LocalDate.parse(source, FORMATTER);
    }
}
```

### Example 5: Generic Converter Factory for Generic Domain Enums
```java
package com.example.demo.factory;

import org.springframework.core.convert.converter.Converter;
import org.springframework.core.convert.converter.ConverterFactory;
import org.springframework.stereotype.Component;

public interface CodedEnum {
    int getCode();
}

public enum OrderStatus implements CodedEnum {
    PENDING(10), CONFIRMED(20), SHIPPED(30);
    private final int code;
    OrderStatus(int code) { this.code = code; }
    @Override public int getCode() { return code; }
}

// Factory converting Integer codes to ANY enum implementing CodedEnum
@Component
public class IntegerToCodedEnumConverterFactory implements ConverterFactory<Integer, CodedEnum> {

    @Override
    public <T extends CodedEnum> Converter<Integer, T> getConverter(Class<T> targetType) {
        return new IntegerToCodedEnumConverter<>(targetType);
    }

    private static class IntegerToCodedEnumConverter<T extends CodedEnum> implements Converter<Integer, T> {
        private final Class<T> enumType;

        public IntegerToCodedEnumConverter(Class<T> enumType) {
            this.enumType = enumType;
        }

        @Override
        public T convert(Integer source) {
            for (T constant : enumType.getEnumConstants()) {
                if (constant.getCode() == source) {
                    return constant;
                }
            }
            throw new IllegalArgumentException("Unknown code " + source + " for " + enumType.getSimpleName());
        }
    }
}
```

---

## 6. Best Practices

1. **Always Use `Converter<S, T>` Over `PropertyEditor`**: Never write new `PropertyEditorSupport` classes. `PropertyEditor` is stateful and prone to concurrency corruption; `Converter` is completely stateless and thread-safe.
2. **Use Java Time (`java.time.*`) Over `java.util.Date`**: Leverage modern Java 8+ date/time classes with `@DateTimeFormat(iso = ISO.DATE_TIME)` to guarantee thread safety.
3. **Register Custom Converters as `@Component`**: In Spring Boot, any bean implementing `Converter` or `Formatter` is automatically discovered and added to the `FormattingConversionService`.
4. **Use `ConverterFactory` for Class Hierarchies**: When writing conversion logic that applies to multiple related classes (e.g., all enums, all entity IDs), use `ConverterFactory<S, R>` instead of separate individual converters.
5. **Handle Malformed Inputs with `IllegalArgumentException`**: If a converter fails to parse input, throw `IllegalArgumentException`. Spring automatically maps it to a `TypeMismatchException` or HTTP 400 Bad Request.
6. **Separate Jackson Deserializers from Spring Converters**: Remember: `Converter` handles URL paths, query parameters, and `@Value` properties. It does NOT deserialize JSON request bodies; JSON is handled by Jackson (`JsonDeserializer`).

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Using `SimpleDateFormat` in singleton converters** | `SimpleDateFormat` is mutable and not thread-safe; concurrent requests corrupt parsed dates. | Use `java.time.format.DateTimeFormatter` (immutable). |
| **Expecting `Converter` to parse JSON bodies** | Spring MVC uses `HttpMessageConverters` (Jackson) for `@RequestBody`, bypassing `ConversionService`. | Configure custom Jackson deserializers for JSON payloads. |
| **Throwing checked exceptions from `convert()`** | The `Converter.convert()` method contract does not permit checked exceptions. | Throw `IllegalArgumentException` with a clear message. |
| **Stateful instance variables in a `Converter`** | `Converter` beans are singletons; storing per-request data in fields causes race conditions. | Keep converters completely stateless. |
| **Missing converter registration** | Custom converter not annotated with `@Component` or added to `WebMvcConfigurer` is never called. | Add `@Component` or register in `addFormatters()`. |
| **Circular conversion loops in `GenericConverter`** | A custom converter attempting to convert type A to B which delegates back to B to A causes `StackOverflowError`. | Avoid recursive conversion delegation. |

---

## 8. Performance & Security Notes

### Converter Lookup Caching
The `ConversionService` caches resolved converters in an internal `ConcurrentHashMap` keyed by `(SourceType, TargetType)`:
- Type conversion incurs near-zero lookup overhead after the initial resolution.

### Security Input Normalization
Converters often process user-supplied parameters from URLs. Always validate and sanitize strings inside `convert()`:
- Bound string lengths to avoid ReDoS or memory allocation attacks when parsing custom data types.

---

## 9. Comparison / When to Use What

| Mechanism | Target | Thread-Safe? | Handles JSON? | Best For |
| :--- | :--- | :---: | :---: | :--- |
| **`Converter<S, T>`** | Any type S -> Any type T | **Yes** | No | General type transformation, path variables |
| **`Formatter<T>`** | String <-> Object (Localized) | **Yes** | No | UI form fields, localized dates/numbers |
| **`PropertyEditor`** | String <-> Object (JavaBeans) | **NO (Stateful)**| No | Legacy J2EE / Spring 2.x only |
| **Jackson `JsonDeserializer`**| JSON text -> Java Object | **Yes** | **Yes** | HTTP `@RequestBody` payload parsing |

---

## 10. Interview Questions

<details>
<summary><strong>Q1: What is the difference between `Converter<S, T>` and `Formatter<T>` in Spring?</strong></summary>

**Answer:**  
- `Converter<S, T>`: A general-purpose SPI for converting between any two arbitrary types `S` and `T` (e.g., `String` to `CustomerId`, `Long` to `Instant`). It has no concept of locale or formatting style.
- `Formatter<T>`: A specialized SPI specifically for converting between `String` and type `T` with full support for internationalization (`Locale`) and formatting annotations (like `@DateTimeFormat` or `@NumberFormat`).
</details>

<details>
<summary><strong>Q2: Why was `PropertyEditor` replaced by `ConversionService`?</strong></summary>

**Answer:**  
`PropertyEditor` (from `java.beans`) is inherently **stateful**: it stores the converted value internally in an instance variable (`setValue()` / `getValue()`). Consequently, `PropertyEditor` instances are not thread-safe and cannot be shared as Spring singletons. `ConversionService` and `Converter<S, T>` are completely **stateless and thread-safe**, allowing a single shared instance to serve concurrent requests across the entire application.
</details>

<details>
<summary><strong>Q3: Does a Spring `Converter` handle JSON deserialization for `@RequestBody`?</strong></summary>

**Answer:**  
**No.** `ConversionService` handles type conversion for HTTP path variables (`@PathVariable`), query parameters (`@RequestParam`), request headers, form data, and `@ConfigurationProperties`. Deserialization of HTTP JSON payloads (`@RequestBody`) is handled by `HttpMessageConverter` implementations, specifically Jackson (`MappingJackson2HttpMessageConverter`).
</details>

<details>
<summary><strong>Q4: How does Spring Boot automatically register custom converters?</strong></summary>

**Answer:**  
Spring Boot's auto-configuration detects any bean in the `ApplicationContext` that implements `Converter`, `GenericConverter`, or `Formatter` and automatically registers it into the application's global `FormattingConversionService` during bootstrap.
</details>

<details>
<summary><strong>Q5: What is a `ConverterFactory<S, R>` and when should it be used?</strong></summary>

**Answer:**  
`ConverterFactory` is used when you need to convert from a single source type `S` to an entire class hierarchy or family of classes rooted at `R`. The canonical example is `StringToEnumConverterFactory`: instead of writing a separate converter for every single enum in the project, one factory converts `String` to any class extending `Enum<?>`.
</details>

<details>
<summary><strong>Q6: How does Spring handle conversion errors in `@PathVariable`?</strong></summary>

**Answer:**  
If a converter throws an `IllegalArgumentException` or `TypeMismatchException` when parsing a path variable, Spring MVC halts controller execution and throws `MethodArgumentTypeMismatchException`, which by default results in an HTTP 400 Bad Request response.
</details>

<details>
<summary><strong>Q7: What is `GenericConverter` in Spring?</strong></summary>

**Answer:**  
`GenericConverter` is the most flexible, low-level conversion SPI in Spring. Unlike `Converter<S, T>`, which converts between two fixed types, `GenericConverter` can convert between multiple source/target pairs and provides access to `TypeDescriptor` metadata, allowing inspection of field annotations (e.g., reading `@DateTimeFormat` parameters).
</details>

<details>
<summary><strong>Q8: How do you format a `LocalDate` property using ISO standards in a Spring DTO?</strong></summary>

**Answer:**  
Annotate the field with `@DateTimeFormat`:
```java
@DateTimeFormat(iso = DateTimeFormat.ISO.DATE)
private LocalDate birthDate;
```
Spring's `FormattingConversionService` uses this metadata to parse strings formatted as `YYYY-MM-DD` automatically.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Implement a StringToUUID Converter (Easy)</strong></summary>

**Task:**  
Create a custom `Converter<String, UUID>` that safely parses UUID strings. Register it as a `@Component` and write a test converting `"123e4567-e89b-12d3-a456-426614174000"`.
</details>

<details>
<summary><strong>Exercise 2: PathVariable with Domain Record ID (Easy)</strong></summary>

**Task:**  
Create a record `TrackingNumber(String code)`. Write a converter `StringToTrackingNumberConverter`. Build a controller method `@GetMapping("/track/{trackingNumber}")` accepting `TrackingNumber` directly, and assert it works via MockMvc.
</details>

<details>
<summary><strong>Exercise 3: Custom Formatter with Locale Currency (Medium)</strong></summary>

**Task:**  
Implement a `Formatter<BigDecimal>` that parses currency strings (e.g., "$100.50" in US, "100,50 €" in Germany) according to the request's `Locale`.
</details>

<details>
<summary><strong>Exercise 4: Universal StringToEnum ConverterFactory (Medium)</strong></summary>

**Task:**  
Implement a `ConverterFactory<String, Enum>` that performs case-insensitive matching when converting string parameters to any Java enum.
</details>

<details>
<summary><strong>Exercise 5: Benchmark Converter vs Jackson Deserializer (Hard)</strong></summary>

**Task:**  
Write a benchmark measuring the throughput of converting 100,000 strings to `UUID` via Spring's `ConversionService` vs parsing inside a custom Jackson deserializer.
</details>

<details>
<summary><strong>Exercise 6: GenericConverter with Custom Annotation Metadata (Hard)</strong></summary>

**Task:**  
Create a custom annotation `@EncryptedHex`. Implement a `GenericConverter` that inspects the target `TypeDescriptor` for `@EncryptedHex` and automatically hex-decodes the string into a `byte[]` only when the annotation is present.
</details>

---

## 12. Summary / Cheat Sheet

| SPI / Interface | Signature | Key Characteristic |
| :--- | :--- | :--- |
| **`Converter<S, T>`** | `T convert(S source)` | Stateless, thread-safe, general conversion |
| **`Formatter<T>`** | `print(T, Locale)` / `parse(String, Locale)` | Localized String <-> Object transformation |
| **`ConverterFactory<S, R>`** | `Converter<S, T> getConverter(Class<T>)` | Converts to entire class hierarchies (Enums) |
| **`GenericConverter`** | `convert(Object, TypeDescriptor, TypeDescriptor)` | Low-level, full access to annotation metadata |
| **`@DateTimeFormat`** | `@DateTimeFormat(iso = ISO.DATE)` | Declarative date parsing in DTOs/parameters |
| **`@NumberFormat`** | `@NumberFormat(style = Style.CURRENCY)` | Declarative number/currency formatting |
| **Automatic Registration** | Mark with `@Component` in Spring Boot | Discovered and added to `ConversionService` |

---

## 13. Further Reading
- [Official Spring Framework Reference: Spring Type Conversion](https://docs.spring.io/spring-framework/reference/core/validation/convert.html)
- [Spring ConversionService Interface Javadoc](https://docs.spring.io/spring-framework/docs/current/javadoc-api/org/springframework/core/convert/ConversionService.html)
- [Java Time (JSR-310) Formatting](https://docs.oracle.com/en/java/javase/21/docs/api/java.base/java/time/format/DateTimeFormatter.html)
