# Jackson Customization & JSON in Spring Boot

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | Java JSON Processing, Spring MVC Web Layer, Java 21 Records |
| **Estimated Reading Time** | 25 minutes |
| **Status** | Production-Ready |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Baseline** | OpenJDK 21 LTS / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | Jackson 2.18, OpenJDK 21.0.2 |

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
9. [Comparison: Custom Serializer vs MixIn vs DTO Projection](#9-comparison-custom-serializer-vs-mixin-vs-dto-projection)
10. [Interview Questions & Deep Dives](#10-interview-questions--deep-dives)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference](#12-summary--quick-reference)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

In modern distributed microservices, JSON is the lingua franca of data interchange. How an application serializes and deserializes payloads directly affects API contract stability, performance, security, and backward compatibility.

### The Problem
Default JSON serialization falls short in production:
1. `java.time.*` instances serialize to arrays of numbers (`[2024, 11, 24]`) instead of standardized ISO-8601 strings.
2. Fields with `null` values bloat network payloads and leak internal schema details.
3. Domain models with circular references (e.g. parent-child entities) trigger infinite recursion and JVM `StackOverflowError`.
4. Sensitive fields (passwords, credit card CVVs, PII) risk accidental exposure.

### The Solution: Jackson & Spring Boot Integration
Spring Boot auto-configures FasterXML Jackson via `JacksonAutoConfiguration`. It provides an opinionated, production-tuned `ObjectMapper` that supports Java 21 records, registers the `JavaTimeModule` by default, enables snake_case or camelCase naming strategies, and exposes clean extension hooks (`Jackson2ObjectMapperBuilderCustomizer`, custom serializers, and `@JsonView`).

---

## 2. Core Concepts

### Mental Model & Architectural Serialization Graph
```
                     Incoming HTTP Body (Raw JSON bytes)
                                      │
                                      ▼
                   MappingJackson2HttpMessageConverter
                                      │
                                      ▼
                    Jackson ObjectMapper Engine
           ┌──────────────────────────┼──────────────────────────┐
           ▼                          ▼                          ▼
   NamingStrategy         DeserializationFeatures         Custom Deserializer
  (snake_case -> camelCase) (FAIL_ON_UNKNOWN_PROPERTIES) (JsonDeserializer<T>)
           │                          │                          │
           └──────────────────────────┼──────────────────────────┘
                                      ▼
                      Instantiates Target Java Record
                                      │
                                      ▼
                      Controller Method Execution
                                      │
                                      ▼
                      Domain Object Output to Client
           ┌──────────────────────────┼──────────────────────────┐
           ▼                          ▼                          ▼
    @JsonInclude             SerializationFeatures          Custom Serializer
  (NON_NULL, NON_EMPTY)   (WRITE_DATES_AS_TIMESTAMPS)     (JsonSerializer<T>)
           │                          │                          │
           └──────────────────────────┼──────────────────────────┘
                                      ▼
                     Outgoing HTTP Body (Clean JSON)
```

### Invariants
1. **Singleton ObjectMapper**: Spring Boot maintains a single shared, thread-safe `ObjectMapper` instance across the entire application context.
2. **Immutable Reconfiguration**: Modifying an `ObjectMapper` after application startup should be avoided; customize it during container configuration via `Jackson2ObjectMapperBuilderCustomizer`.
3. **Record Constructor Matching**: Jackson automatically matches JSON property names to canonical record component names without requiring `@JsonCreator`.

---

## 3. How It Works Internally

### Jackson Auto-Configuration in Spring Boot
The `JacksonAutoConfiguration` class:
1. Scans for existing `ObjectMapper` beans; if missing, it invokes `Jackson2ObjectMapperBuilder`.
2. Gathers all registered `Jackson2ObjectMapperBuilderCustomizer` beans in the context and applies them in order.
3. Automatically registers standard modules on the classpath:
   - `ParameterNamesModule`: For parameter name discovery without explicit annotations.
   - `Jdk8Module`: For `Optional<T>` support.
   - `JavaTimeModule`: For JSR-310 dates (`Instant`, `LocalDate`, `ZonedDateTime`).
4. Re-injects the builder into `MappingJackson2HttpMessageConverter`.

---

## 4. Syntax & Configuration

### Standard `application.yaml` Jackson Tuning
```yaml
spring:
  jackson:
    default-property-inclusion: non_null # Exclude null fields from JSON
    property-naming-strategy: SNAKE_CASE # Maps firstName -> first_name
    date-format: yyyy-MM-dd'T'HH:mm:ss.SSSXXX
    serialization:
      write-dates-as-timestamps: false # Format dates as ISO-8601 strings
      indent-output: false # Disable pretty printing in production for performance
      fail-on-empty-beans: false
    deserialization:
      fail-on-unknown-properties: false # Tolerant reader pattern
      accept-single-value-as-array: true
```

### Programmatic Customization via Customizer Bean
```java
package com.example.web.config;

import com.fasterxml.jackson.databind.DeserializationFeature;
import com.fasterxml.jackson.databind.SerializationFeature;
import org.springframework.boot.autoconfigure.jackson.Jackson2ObjectMapperBuilderCustomizer;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;

@Configuration(proxyBeanMethods = false)
public class JacksonCustomizerConfig {

    @Bean
    public Jackson2ObjectMapperBuilderCustomizer jsonCustomizer() {
        return builder -> builder
                .featuresToDisable(
                        SerializationFeature.WRITE_DATES_AS_TIMESTAMPS,
                        DeserializationFeature.FAIL_ON_UNKNOWN_PROPERTIES
                )
                .featuresToEnable(
                        DeserializationFeature.ACCEPT_EMPTY_STRING_AS_NULL_OBJECT
                );
    }
}
```

---

## 5. Practical Examples

### Example 1: Modern Java 21 Record with Jackson Annotations

```java
package com.example.web.dto;

import com.fasterxml.jackson.annotation.JsonFormat;
import com.fasterxml.jackson.annotation.JsonInclude;
import com.fasterxml.jackson.annotation.JsonProperty;

import java.math.BigDecimal;
import java.time.Instant;
import java.util.UUID;

@JsonInclude(JsonInclude.Include.NON_NULL)
public record TransactionDto(
        @JsonProperty("tx_id")
        UUID transactionId,

        @JsonProperty("amount")
        BigDecimal amount,

        @JsonProperty("currency")
        String currencyCode,

        @JsonFormat(shape = JsonFormat.Shape.STRING, pattern = "yyyy-MM-dd'T'HH:mm:ssXXX", timezone = "UTC")
        Instant processedAt,

        @JsonProperty("metadata")
        String optionalMetadata // Excluded from output JSON if null
) {}
```

### Example 2: Enterprise Custom Serializer & Deserializer for Masked Credit Cards

```java
package com.example.web.jackson;

import com.fasterxml.jackson.core.JsonGenerator;
import com.fasterxml.jackson.core.JsonParser;
import com.fasterxml.jackson.databind.DeserializationContext;
import com.fasterxml.jackson.databind.JsonDeserializer;
import com.fasterxml.jackson.databind.JsonSerializer;
import com.fasterxml.jackson.databind.SerializerProvider;
import org.springframework.boot.jackson.JsonComponent;

import java.io.IOException;

public record MaskedCreditCard(String pan) {}

/**
 * Spring Boot @JsonComponent automatically registers this serializer/deserializer pair
 * into the main ObjectMapper without manual module registration!
 */
@JsonComponent
public class MaskedCreditCardJsonComponent {

    public static class Serializer extends JsonSerializer<MaskedCreditCard> {
        @Override
        public void serialize(MaskedCreditCard card, JsonGenerator gen, SerializerProvider serializers) throws IOException {
            if (card == null || card.pan() == null) {
                gen.writeNull();
                return;
            }
            String raw = card.pan().replaceAll("\\s+", "");
            if (raw.length() >= 4) {
                String masked = "****-****-****-" + raw.substring(raw.length() - 4);
                gen.writeString(masked);
            } else {
                gen.writeString("****");
            }
        }
    }

    public static class Deserializer extends JsonDeserializer<MaskedCreditCard> {
        @Override
        public MaskedCreditCard deserialize(JsonParser p, DeserializationContext ctxt) throws IOException {
            String text = p.getText();
            return new MaskedCreditCard(text != null ? text.trim() : null);
        }
    }
}
```

### Example 3: Edge Case — Breaking Bidirectional Circular References

When working with entities having parent-child relationships, naive serialization causes `InfiniteRecursionException`.

```java
package com.example.web.model;

import com.fasterxml.jackson.annotation.JsonBackReference;
import com.fasterxml.jackson.annotation.JsonManagedReference;

import java.util.ArrayList;
import java.util.List;

public class Department {
    private String name;

    // The forward part of the reference: serialized normally
    @JsonManagedReference
    private List<Employee> employees = new ArrayList<>();

    public String getName() { return name; }
    public List<Employee> getEmployees() { return employees; }
}

public class Employee {
    private String name;

    // The back part of the reference: omitted during serialization to prevent infinite loop
    @JsonBackReference
    private Department department;

    public String getName() { return name; }
    public Department getDepartment() { return department; }
}
```

### Example 4: Anti-Pattern vs Production Fix

#### Anti-Pattern: Instantiating `new ObjectMapper()` in Services
```java
// ANTI-PATTERN: Manually instantiating ObjectMapper loses all Spring Boot configurations!
@Service
public class BadJsonService {
    
    public String toJson(Object obj) throws Exception {
        // Missing JavaTimeModule, missing naming strategy, creates unneeded GC pressure
        ObjectMapper mapper = new ObjectMapper();
        return mapper.writeValueAsString(obj);
    }
}
```

#### Production Fix: Injecting the Spring-Managed `ObjectMapper`
```java
// PRODUCTION FIX: Inject the centrally configured singleton ObjectMapper
@Service
public class GoodJsonService {

    private final ObjectMapper objectMapper;

    public GoodJsonService(ObjectMapper objectMapper) {
        this.objectMapper = objectMapper;
    }

    public String toJson(Object obj) {
        try {
            return objectMapper.writeValueAsString(obj);
        } catch (JsonProcessingException e) {
            throw new RuntimeException("JSON serialization error", e);
        }
    }
}
```

### Example 5: High-Performance `@JsonView` Dynamic Filtering

```java
package com.example.web.view;

import com.fasterxml.jackson.annotation.JsonView;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

public class Views {
    public interface PublicSummary {}
    public interface InternalAdmin extends PublicSummary {}
}

public record UserAccount(
        @JsonView(Views.PublicSummary.class)
        String username,

        @JsonView(Views.PublicSummary.class)
        String displayName,

        @JsonView(Views.InternalAdmin.class)
        String internalAuditNotes,

        @JsonView(Views.InternalAdmin.class)
        String passwordHash
) {}

@RestController
@RequestMapping("/api/v1/users")
public class UserViewController {

    // Serializes ONLY username and displayName
    @GetMapping("/public-profile")
    @JsonView(Views.PublicSummary.class)
    public UserAccount getPublicProfile() {
        return new UserAccount("johndoe", "John Doe", "Flagged for review", "$2a$12$e...");
    }

    // Serializes ALL fields
    @GetMapping("/admin-profile")
    @JsonView(Views.InternalAdmin.class)
    public UserAccount getAdminProfile() {
        return new UserAccount("johndoe", "John Doe", "Flagged for review", "$2a$12$e...");
    }
}
```

---

## 6. Best Practices

1. **Never Create `new ObjectMapper()`**: Always inject the Spring-managed `ObjectMapper` bean to guarantee consistent time formatting, property naming, and module support.
2. **Use `@JsonComponent` for Custom Serializers**: Annotating serializer/deserializer classes with `@JsonComponent` auto-registers them with Spring's `ObjectMapper`.
3. **Adopt the Tolerant Reader Pattern**: Set `spring.jackson.deserialization.fail-on-unknown-properties=false` so downstream services do not crash when upstream services add new fields.
4. **Format Dates Consistently**: Disable `WRITE_DATES_AS_TIMESTAMPS` to ensure dates serialize as standardized ISO-8601 strings.
5. **Use `@JsonIgnore` / MixIns for Sensitive Fields**: Prevent credentials or internal keys from ever appearing in serialized JSON.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Dates Serializing as Numeric Arrays** | Jackson defaults to `[2024, 11, 24]` without `JavaTimeModule` or when timestamps are enabled. | Set `spring.jackson.serialization.write-dates-as-timestamps: false`. |
| **Instantiating `new ObjectMapper()`** | Loses all Spring Boot configurations and `application.yaml` property settings. | Inject the Spring-managed `ObjectMapper` bean. |
| **Circular Reference `StackOverflowError`** | Bidirectional parent-child associations trigger infinite serialization loops. | Use `@JsonManagedReference` / `@JsonBackReference` or `@JsonIgnore`. |
| **Clobbering Property Naming Strategy** | Inconsistent mix of `camelCase` in Java and `snake_case` in JSON without a configured strategy. | Configure `spring.jackson.property-naming-strategy: SNAKE_CASE`. |
| **Mutating `ObjectMapper` at Runtime** | Calling `mapper.configure()` inside request handlers causes thread-safety issues across concurrent requests. | Perform all configuration during startup via `Jackson2ObjectMapperBuilderCustomizer`. |
| **Missing No-Arg Constructor on Classic Java Beans** | Jackson fails to deserialize standard JavaBeans unless a no-arg constructor or `@JsonCreator` exists. | Use Java `record` types or provide a default no-arg constructor. |

---

## 8. Performance & Security Considerations

### Polymorphic Deserialization Flaws
Enabling default typing (`enableDefaultTyping()`) or insecure `@JsonTypeInfo` configurations allows attackers to trigger arbitrary class instantiation and Remote Code Execution (RCE). Never enable polymorphic deserialization on untrusted inputs without a strict `BasicPolymorphicTypeValidator` whitelist.

### GC Allocation Overhead with Pretty Printing
Setting `spring.jackson.serialization.indent-output: true` incurs non-trivial CPU overhead and increases payload sizes by 20-30%. Keep pretty printing disabled in production environments.

---

## 9. Comparison: Custom Serializer vs MixIn vs DTO Projection

| Mechanism | Description | Best Used For |
| :--- | :--- | :--- |
| **Custom Serializer (`JsonSerializer<T>`)** | Complete low-level imperative control over output token stream. | Data masking (credit cards, PII), custom cryptographic formats. |
| **Jackson MixIn** | Applies annotations to third-party classes without modifying their source code. | Annotating immutable external library classes. |
| **DTO Projection (Record)** | Dedicated Java `record` specifically shaped for API contract. | Standard REST APIs (cleanest separation of concerns). |

---

## 10. Interview Questions & Deep Dives

<details>
<summary><strong>Q1: Why is it considered an anti-pattern to call `new ObjectMapper()` in a Spring Boot service?</strong></summary>

**Answer:**
`ObjectMapper` instantiation is CPU- and memory-intensive because it scans and builds extensive serializer/deserializer metadata caches. Furthermore, calling `new ObjectMapper()` bypasses Spring Boot's auto-configuration, losing settings like `JavaTimeModule` (causing date formatting issues), naming strategies, and custom serializers registered in the application context.
</details>

<details>
<summary><strong>Q2: What is `@JsonComponent` in Spring Boot?</strong></summary>

**Answer:**
`@JsonComponent` is a specialized Spring Boot stereotype annotation. Any class annotated with `@JsonComponent` that extends `JsonSerializer`, `JsonDeserializer`, or `KeyDeserializer` is automatically discovered by `JacksonAutoConfiguration` and registered into the primary `ObjectMapper` without requiring manual module construction.
</details>

<details>
<summary><strong>Q3: How does Jackson deserialize Java 21 `record` types without `@JsonCreator`?</strong></summary>

**Answer:**
Jackson 2.12+ has built-in support for Java records. It uses Java Reflection (`Class.isRecord()` and `Class.getRecordComponents()`) to inspect the record's components and their types, matching JSON keys directly to the parameters of the record's canonical constructor without needing explicit annotations.
</details>

<details>
<summary><strong>Q4: What is the purpose of `@JsonView` and how does Spring MVC integrate with it?</strong></summary>

**Answer:**
`@JsonView` allows serializing different subsets of fields from the same object depending on the calling context. When a controller method is annotated with `@JsonView(Views.Public.class)`, Spring MVC instructs the `MappingJackson2HttpMessageConverter` to activate that view filter during serialization, omitting fields designated for internal or administrative views.
</details>

<details>
<summary><strong>Q5: How do `@JsonManagedReference` and `@JsonBackReference` prevent infinite recursion?</strong></summary>

**Answer:**
They manage bidirectional relationships. `@JsonManagedReference` marks the forward part of the relationship (e.g. parent pointing to children) and is serialized normally. `@JsonBackReference` marks the reverse part of the relationship (e.g. child pointing back to parent) and is omitted during serialization, breaking the infinite loop.
</details>

<details>
<summary><strong>Q6: How can you apply Jackson annotations to a class from an external JAR whose source code you cannot modify?</strong></summary>

**Answer:**
Using **Jackson Mix-ins**. You create an abstract class or interface declaring the desired annotations, and register it with the builder:
`builder.mixIn(TargetExternalClass.class, TargetMixIn.class)`.
</details>

<details>
<summary><strong>Q7: What is the impact of setting `FAIL_ON_UNKNOWN_PROPERTIES = false`?</strong></summary>

**Answer:**
It enables the "Tolerant Reader" pattern. If an incoming JSON payload contains attributes not recognized by the target Java class, Jackson ignores them instead of throwing `UnrecognizedPropertyException`. This facilitates zero-downtime rolling upgrades when schema changes occur.
</details>

<details>
<summary><strong>Q8: Why does Jackson require the `JavaTimeModule` for `java.time` types?</strong></summary>

**Answer:**
Java 8 Date and Time API (`java.time.*`) was introduced after Jackson's initial core release. By default, Jackson attempts to serialize them as standard JavaBeans via reflection, resulting in numeric array breakdowns (`[year, month, day]`). The `JavaTimeModule` provides custom serializers and deserializers that parse and format ISO-8601 strings.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Exclude Null Properties Globally in YAML</strong></summary>

**Task**: Write the `application.yaml` property to omit all `null` fields from JSON responses across the entire application.

```yaml
# Solution:
spring:
  jackson:
    default-property-inclusion: non_null
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Snake Case Naming Strategy Configuration</strong></summary>

**Task**: Configure Spring Boot to automatically map Java camelCase properties (`totalRevenue`) to JSON snake_case (`total_revenue`).

```yaml
# Solution:
spring:
  jackson:
    property-naming-strategy: SNAKE_CASE
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Custom Money Serializer with `@JsonComponent`</strong></summary>

**Task**: Create a record `Money(BigDecimal amount, String currency)` and a `@JsonComponent` that serializes it as a formatted string: `"USD 99.50"`.

```java
// Solution:
public record Money(BigDecimal amount, String currency) {}

@JsonComponent
public class MoneyJsonComponent extends JsonSerializer<Money> {
    @Override
    public void serialize(Money value, JsonGenerator gen, SerializerProvider serializers) throws IOException {
        if (value == null) {
            gen.writeNull();
            return;
        }
        gen.writeString(value.currency() + " " + value.amount().setScale(2, RoundingMode.HALF_UP));
    }
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Jackson MixIn for Third-Party Entity</strong></summary>

**Task**: Create a MixIn to ignore the `internalToken` field of a third-party class `ExternalUserProfile`.

```java
// Solution:
public abstract class ExternalUserProfileMixIn {
    @JsonIgnore
    private String internalToken;
}

@Configuration(proxyBeanMethods = false)
public class MixInConfig {
    @Bean
    public Jackson2ObjectMapperBuilderCustomizer mixInCustomizer() {
        return builder -> builder.mixIn(ExternalUserProfile.class, ExternalUserProfileMixIn.class);
    }
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Secure Polymorphic Deserialization Whitelist</strong></summary>

**Task**: Implement a polymorphic `EventNotification` hierarchy (`PaymentEvent`, `LoginEvent`) with a secure `BasicPolymorphicTypeValidator` whitelist preventing arbitrary class deserialization.
</details>

<details>
<summary><strong>Exercise 6 (Hard): High-Throughput Streaming JSON Output</strong></summary>

**Task**: Write an endpoint streaming 100,000 JSON records using Jackson's low-level `SequenceWriter` directly to `HttpServletResponse.getOutputStream()`, keeping memory consumption under 20MB.
</details>

---

## 12. Summary & Quick Reference

- **Single Engine**: Always inject the Spring-configured `ObjectMapper`.
- **Annotation Registration**: Use `@JsonComponent` for custom serializers.
- **Tolerant Reader**: Set `deserialization.fail-on-unknown-properties: false`.
- **Date Formatting**: Set `serialization.write-dates-as-timestamps: false`.
- **Java 21 Records**: Supported natively without extra annotations.

---

## 13. Further Reading

- [Spring Boot Official Documentation: Jackson Auto-configuration](https://docs.spring.io/spring-boot/reference/features/json.html#features.json.jackson)
- [FasterXML Jackson Documentation and Best Practices](https://github.com/FasterXML/jackson)
- [OWASP: Deserialization of Untrusted Data Prevention](https://cheatsheetseries.owasp.org/cheatsheets/Deserialization_Cheat_Sheet.html)
- [Java 21 Records Serialization Semantics](https://docs.oracle.com/en/java/javase/21/docs/specs/serialization/serial-arch.html#record-serialization)
