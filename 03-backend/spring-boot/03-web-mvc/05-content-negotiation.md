# Content Negotiation in Spring MVC

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate to Advanced |
| **Prerequisites** | HTTP RFC 9110, Spring MVC Controllers, HttpMessageConverter |
| **Estimated Reading Time** | 22 minutes |
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
9. [Comparison: Header vs Parameter Negotiation](#9-comparison-header-vs-parameter-negotiation)
10. [Interview Questions & Deep Dives](#10-interview-questions--deep-dives)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference](#12-summary--quick-reference)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

In enterprise architectures, an API frequently serves heterogeneous consumers: web browsers expecting HTML or JSON, mobile clients requesting lightweight compact representations, enterprise legacy systems requiring XML, or business analytics tools requesting CSV/PDF downloads.

### The Problem
Hardcoding response representations (e.g., returning only JSON) forces services to create redundant endpoints (`/api/users/json`, `/api/users/xml`, `/api/users/csv`). This multiplies routing complexity, complicates security policies, and violates standard HTTP specifications.

### The Solution: HTTP Content Negotiation
HTTP RFC 9110 defines content negotiation: clients indicate their acceptable formats using the `Accept` header (e.g., `Accept: application/json, application/xml;q=0.9`), and servers select the most suitable representation. Spring MVC abstracts this mechanism through `ContentNegotiationManager` and `HttpMessageConverter`, providing seamless transformation between domain objects and diverse media types without polluting business controllers.

---

## 2. Core Concepts

### Mental Model & Content Negotiation Pipeline
```
               Client sends HTTP Request:
               GET /api/v1/reports/2024 HTTP/1.1
               Accept: application/xml;q=0.8, application/json;q=1.0
                                │
                                ▼
                   ContentNegotiationManager
               Resolves requested MediaType list:
               [application/json (weight 1.0), application/xml (weight 0.8)]
                                │
                                ▼
               Controller Handler returns Domain Object:
                    FinancialReport (Java Record)
                                │
                                ▼
               RequestResponseBodyMethodProcessor
               Matches requested MediaTypes against registered HttpMessageConverters:
               • MappingJackson2HttpMessageConverter (can write application/json? YES)
               • MappingJackson2XmlHttpMessageConverter (can write application/xml? YES)
                                │
                                ▼
               Selected Converter: MappingJackson2HttpMessageConverter
               Writes JSON payload to OutputStream
               Sets Response Header: Content-Type: application/json
```

### Invariants
1. **Quality Values (`q` factor)**: When multiple media types are requested, the client's `q` factor (from 0.0 to 1.0) dictates precedence.
2. **406 Not Acceptable**: If the client specifies an `Accept` header that none of the registered converters can satisfy, Spring returns HTTP 406 (`Not Acceptable`).
3. **No Suffix Matching**: Spring Boot 3 has removed path extension suffix matching (`.json` or `.xml` in URIs) due to security risks (RFD attacks); use headers or query parameters instead.

---

## 3. How It Works Internally

### `ContentNegotiationManager` Strategies
The `ContentNegotiationManager` resolves media types by querying an ordered list of `ContentNegotiationStrategy` implementations:

1. **`HeaderContentNegotiationStrategy` (Default)**: Inspects the incoming `Accept` header.
2. **`ParameterContentNegotiationStrategy`**: Inspects a designated query parameter (e.g., `?format=json` or `?format=xml`).
3. **`FixedContentNegotiationStrategy`**: Provides fallback default media types if no negotiation input is provided.

### Converter Selection
The `AbstractMessageConverterMethodProcessor` iterates over all registered `HttpMessageConverter` beans. For each converter:
- It checks `converter.canWrite(clazz, mediaType)`.
- The first converter that can serialize the Java class into the negotiated `MediaType` is chosen to render the HTTP response.

---

## 4. Syntax & Configuration

### Enabling Query Parameter Content Negotiation in JavaConfig

```java
package com.example.web.config;

import org.springframework.context.annotation.Configuration;
import org.springframework.http.MediaType;
import org.springframework.web.servlet.config.annotation.ContentNegotiationConfigurer;
import org.springframework.web.servlet.config.annotation.WebMvcConfigurer;

@Configuration(proxyBeanMethods = false)
public class ContentNegotiationConfig implements WebMvcConfigurer {

    @Override
    public void configureContentNegotiation(ContentNegotiationConfigurer configurer) {
        configurer
                // Favor the Accept header (standard HTTP behavior)
                .favorParameter(true)
                .parameterName("format") // Enables ?format=json or ?format=xml
                .ignoreAcceptHeader(false)
                .defaultContentType(MediaType.APPLICATION_JSON)
                .mediaType("json", MediaType.APPLICATION_JSON)
                .mediaType("xml", MediaType.APPLICATION_XML)
                .mediaType("csv", MediaType.valueOf("text/csv"));
    }
}
```

### Adding XML Support via Jackson (`pom.xml`)
```xml
<dependency>
    <groupId>com.fasterxml.jackson.dataformat</groupId>
    <artifactId>jackson-dataformat-xml</artifactId>
</dependency>
```

---

## 5. Practical Examples

### Example 1: Multi-Format REST Controller (JSON & XML via Single Endpoint)

```java
package com.example.web.controller;

import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PathVariable;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

import java.math.BigDecimal;
import java.time.Instant;
import java.util.UUID;

// Automatically serialized to JSON or XML depending on client Accept header
public record InvoiceDto(
        UUID invoiceId,
        String customerNumber,
        BigDecimal totalAmount,
        Instant issueDate
) {}

@RestController
@RequestMapping("/api/v1/invoices")
public class InvoiceController {

    @GetMapping("/{id}")
    public ResponseEntity<InvoiceDto> getInvoice(@PathVariable UUID id) {
        InvoiceDto invoice = new InvoiceDto(
                id,
                "CUST-8812",
                new BigDecimal("1250.75"),
                Instant.now()
        );
        return ResponseEntity.ok(invoice);
    }
}
```

### Example 2: Custom `HttpMessageConverter` for CSV Export

```java
package com.example.web.converter;

import com.example.web.controller.InvoiceDto;
import org.springframework.http.HttpInputMessage;
import org.springframework.http.HttpOutputMessage;
import org.springframework.http.MediaType;
import org.springframework.http.converter.AbstractHttpMessageConverter;
import org.springframework.http.converter.HttpMessageNotReadableException;
import org.springframework.http.converter.HttpMessageNotWritableException;
import org.springframework.stereotype.Component;

import java.io.IOException;
import java.io.OutputStreamWriter;
import java.nio.charset.StandardCharsets;

@Component
public class InvoiceCsvHttpMessageConverter extends AbstractHttpMessageConverter<InvoiceDto> {

    public InvoiceCsvHttpMessageConverter() {
        super(new MediaType("text", "csv", StandardCharsets.UTF_8));
    }

    @Override
    protected boolean supports(Class<?> clazz) {
        return InvoiceDto.class.isAssignableFrom(clazz);
    }

    @Override
    protected InvoiceDto readInternal(Class<? extends InvoiceDto> clazz, HttpInputMessage inputMessage) {
        throw new UnsupportedOperationException("CSV deserialization not supported");
    }

    @Override
    protected void writeInternal(InvoiceDto invoice, HttpOutputMessage outputMessage) throws IOException {
        outputMessage.getHeaders().setContentType(new MediaType("text", "csv", StandardCharsets.UTF_8));
        try (OutputStreamWriter writer = new OutputStreamWriter(outputMessage.getBody(), StandardCharsets.UTF_8)) {
            writer.write("invoiceId,customerNumber,totalAmount,issueDate\n");
            writer.write(String.format("%s,%s,%s,%s\n",
                    invoice.invoiceId(),
                    invoice.customerNumber(),
                    invoice.totalAmount(),
                    invoice.issueDate()));
            writer.flush();
        }
    }
}
```

### Example 3: Edge Case — Explicit `produces` Guarding vs Negotiation Fallbacks

When an endpoint contains sensitive or binary data that must never be converted to XML:

```java
package com.example.web.controller;

import org.springframework.http.MediaType;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

import java.util.Map;

@RestController
@RequestMapping("/api/v1/system-metrics")
public class SystemMetricsController {

    /**
     * By specifying produces = APPLICATION_JSON_VALUE, this method explicitly
     * rejects Accept: application/xml with HTTP 406 Not Acceptable, preventing
     * misinterpretation of structured telemetry.
     */
    @GetMapping(produces = MediaType.APPLICATION_JSON_VALUE)
    public Map<String, Object> getRawMetrics() {
        return Map.of("cpuLoad", 0.42, "heapUsedBytes", 1024 * 1024 * 128);
    }
}
```

### Example 4: Anti-Pattern vs Production Fix

#### Anti-Pattern: Manual Format Branching Inside Controllers
```java
// ANTI-PATTERN: Polluting controller logic with content type checking
@GetMapping("/report")
public ResponseEntity<?> downloadReport(@RequestParam String format) {
    ReportData data = reportService.generate();
    if ("xml".equalsIgnoreCase(format)) {
        return ResponseEntity.ok(xmlMapper.writeValueAsString(data));
    } else if ("csv".equalsIgnoreCase(format)) {
        return ResponseEntity.ok(csvGenerator.generate(data));
    }
    return ResponseEntity.ok(data); // Returns JSON
}
```

#### Production Fix: Leverage `HttpMessageConverter` and Negotiation Config
```java
// PRODUCTION FIX: Return domain model; Spring handles serialization automatically
@GetMapping(value = "/report", produces = {
        MediaType.APPLICATION_JSON_VALUE,
        MediaType.APPLICATION_XML_VALUE,
        "text/csv"
})
public ResponseEntity<ReportData> downloadReport() {
    return ResponseEntity.ok(reportService.generate());
}
```

### Example 5: High-Performance Binary Protocol Buffers Negotiation

```java
package com.example.web.config;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.http.converter.protobuf.ProtobufHttpMessageConverter;

@Configuration(proxyBeanMethods = false)
public class ProtobufNegotiationConfig {

    /**
     * Enables automatic Protobuf binary serialization when client sends:
     * Accept: application/x-protobuf
     */
    @Bean
    public ProtobufHttpMessageConverter protobufHttpMessageConverter() {
        return new ProtobufHttpMessageConverter();
    }
}
```

---

## 6. Best Practices

1. **Rely on the `Accept` Header**: Follow standard HTTP protocol conventions. Use the `Accept` header as the primary content negotiation strategy.
2. **Provide Query Parameter Escape Hatch**: In APIs frequented by browser users or automated batch scripts, support `?format=json` or `?format=xml` via `favorParameter(true)`.
3. **Register Custom Converters via `extendMessageConverters`**: When adding a custom converter in `WebMvcConfigurer`, use `extendMessageConverters` rather than `configureMessageConverters` to preserve Spring Boot's built-in Jackson converters.
4. **Set Explicit `defaultContentType`**: Always specify a sensible fallback (typically `application/json`) when the client does not specify an `Accept` header.
5. **Enforce Media Types in API Contracts**: Use explicit `produces` attributes on sensitive endpoints to prevent unexpected content negotiation.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Using Path Extensions (`.json`, `.xml`) in Spring Boot 3** | Suffix matching is permanently disabled for security; requests to `/users.json` return 404. | Use query parameter `?format=json` or the `Accept` header. |
| **Overriding Default Converters in `configureMessageConverters`** | Using `configureMessageConverters` clears all built-in converters (Jackson JSON, String, Resource). | Use `extendMessageConverters()` to append custom converters safely. |
| **Missing Jackson XML Dependency** | Client requests `Accept: application/xml`, but Jackson XML starter is absent; throws HTTP 406. | Add `jackson-dataformat-xml` to project dependencies. |
| **XML Root Element Annotation Missing** | Older XML serializers require `@XmlRootElement` or `@JacksonXmlRootElement` on classes. | Ensure records/DTOs are compatible with Jackson XML annotations if needed. |
| **Hardcoding `Content-Type` Header in Response** | Calling `response.setContentType(...)` manually overrides the negotiation engine and bypasses converters. | Let Spring MVC set headers through `ResponseEntity` or `@RequestMapping(produces = ...)`. |
| **Returning Raw Strings with `produces = application/json`** | Spring serializes the string literally with quotes (`"{\"id\":1}"`), resulting in double-escaped JSON. | Return strongly typed DTOs/records rather than raw JSON strings. |

---

## 8. Performance & Security Considerations

### Reflected File Download (RFD) Attacks
Suffix pattern matching (`/download.bat`) historically allowed attackers to trick users into downloading malicious executables via content-negotiated endpoints. Spring Boot 3 disables suffix pattern matching entirely to eliminate this vulnerability class.

### Memory Optimization for Large Formats
Serializing huge datasets into XML or CSV can create massive in-memory DOM structures. For reports exceeding thousands of rows, implement streaming converters using Jackson `SequenceWriter` or direct stream piping.

---

## 9. Comparison: Header vs Parameter Negotiation

| Criteria | Header Negotiation (`Accept: ...`) | Parameter Negotiation (`?format=...`) |
| :--- | :--- | :--- |
| **Standard Alignment** | Strict RFC 9110 HTTP standard | Non-standard URL query extension |
| **Browser Compatibility** | Requires custom client code/Fetch API | Works directly in address bar |
| **Caching (CDN/Proxy)** | Requires `Vary: Accept` header | Cached cleanly by URL path |
| **Security Surface** | Minimal | Low |
| **Primary Use Case** | Microservice-to-microservice REST APIs | Public developer portals & browser downloads |

---

## 10. Interview Questions & Deep Dives

<details>
<summary><strong>Q1: Why did Spring Framework 6 and Spring Boot 3 drop support for suffix-based content negotiation (e.g., `/items.json`)?</strong></summary>

**Answer:**
Suffix pattern matching was deprecated and removed for three major reasons:
1. **Security**: It exposed applications to Reflected File Download (RFD) attacks and path-confusion vulnerabilities in reverse proxies.
2. **Ambiguity**: Suffixes conflicted with URI variable extraction (e.g., `/files/{fileName}` where `fileName` contains dots).
3. **HTTP Standards**: RFC 9110 defines content negotiation through headers, not file extensions.
</details>

<details>
<summary><strong>Q2: What is the purpose of the `Vary: Accept` HTTP header in content negotiation?</strong></summary>

**Answer:**
When an endpoint serves multiple representations (e.g. JSON and XML) from the same URL based on the `Accept` header, downstream HTTP caches and CDNs must be informed. The `Vary: Accept` header instructs intermediate caches to cache responses separately for different `Accept` header values, preventing a client requesting XML from receiving a cached JSON payload.
</details>

<details>
<summary><strong>Q3: What HTTP status code is returned when a client requests an unsupported media type in the `Accept` header?</strong></summary>

**Answer:**
HTTP 406 (Not Acceptable). This indicates that the server cannot produce a response matching any of the acceptable media types specified in the request's `Accept` header.
</details>

<details>
<summary><strong>Q4: How does Spring MVC order multiple media types when a client specifies quality values (`q` factor)?</strong></summary>

**Answer:**
Spring parses the `Accept` header into a list of `MediaType` objects and sorts them using `MediaType.sortByQualityValue()`. Types with higher `q` values (e.g. `q=1.0`) are placed first. When `q` values are equal, more specific types (e.g. `application/json`) take precedence over wildcards (e.g. `application/*` or `*/*`).
</details>

<details>
<summary><strong>Q5: What is the difference between `configureMessageConverters` and `extendMessageConverters` in `WebMvcConfigurer`?</strong></summary>

**Answer:**
- `configureMessageConverters`: Replaces Spring's default list of converters entirely. Calling it without manually re-registering default converters disables Jackson and String serialization.
- `extendMessageConverters`: Invoked after Spring has already registered all default converters. It allows adding custom converters or modifying existing ones without clearing defaults.
</details>

<details>
<summary><strong>Q6: How do you configure Spring Boot to default to JSON when no `Accept` header is sent?</strong></summary>

**Answer:**
In a `WebMvcConfigurer`, override `configureContentNegotiation` and call `configurer.defaultContentType(MediaType.APPLICATION_JSON)`.
</details>

<details>
<summary><strong>Q7: Can a single controller method produce different responses based on custom media types?</strong></summary>

**Answer:**
Yes. You can specify custom vendor media types: `@GetMapping(produces = {"application/vnd.company.v1+json", "application/vnd.company.v2+json"})`. Spring's content negotiation will match the exact vendor MIME type specified by the client.
</details>

<details>
<summary><strong>Q8: What interface must be implemented to create a custom serializer for a new media type?</strong></summary>

**Answer:**
Implement `org.springframework.http.converter.HttpMessageConverter<T>`, or extend the convenient base class `AbstractHttpMessageConverter<T>`, overriding `supports()`, `readInternal()`, and `writeInternal()`.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Add XML Support to Spring Boot</strong></summary>

**Task**: Specify the Maven dependency required to enable XML content negotiation via Jackson.

```xml
<!-- Solution: -->
<dependency>
    <groupId>com.fasterxml.jackson.dataformat</groupId>
    <artifactId>jackson-dataformat-xml</artifactId>
</dependency>
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Enable Query Parameter Format Selection</strong></summary>

**Task**: Configure `WebMvcConfigurer` to support selecting format via `?mediaType=json` or `?mediaType=xml`.

```java
// Solution:
@Configuration
public class WebConfig implements WebMvcConfigurer {
    @Override
    public void configureContentNegotiation(ContentNegotiationConfigurer configurer) {
        configurer.favorParameter(true)
                  .parameterName("mediaType")
                  .mediaType("json", MediaType.APPLICATION_JSON)
                  .mediaType("xml", MediaType.APPLICATION_XML);
    }
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Controller with Explicit MediaType Fallback</strong></summary>

**Task**: Write an endpoint `/api/data` that produces only JSON or plain text, returning 406 for XML.

```java
// Solution:
@RestController
public class DataController {
    @GetMapping(
        value = "/api/data",
        produces = {MediaType.APPLICATION_JSON_VALUE, MediaType.TEXT_PLAIN_VALUE}
    )
    public ResponseEntity<String> getData() {
        return ResponseEntity.ok("Sample payload");
    }
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Custom YAML HttpMessageConverter</strong></summary>

**Task**: Implement an `AbstractHttpMessageConverter` that serializes objects to `application/x-yaml` using SnakeYAML or Jackson YAML mapper.

```java
// Solution:
public class YamlHttpMessageConverter extends AbstractHttpMessageConverter<Object> {
    private final ObjectMapper yamlMapper = new ObjectMapper(new YAMLFactory());

    public YamlHttpMessageConverter() {
        super(new MediaType("application", "x-yaml"));
    }

    @Override
    protected boolean supports(Class<?> clazz) { return true; }

    @Override
    protected Object readInternal(Class<?> clazz, HttpInputMessage input) throws IOException {
        return yamlMapper.readValue(input.getBody(), clazz);
    }

    @Override
    protected void writeInternal(Object o, HttpOutputMessage output) throws IOException {
        yamlMapper.writeValue(output.getBody(), o);
    }
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Dynamic Content Negotiation Header Decorator</strong></summary>

**Task**: Implement a servlet filter that inspects incoming requests and adds the `Vary: Accept` header to all responses whenever content negotiation is triggered.
</details>

<details>
<summary><strong>Exercise 6 (Hard): Protocol Buffer & JSON Dual-Mode API Endpoint</strong></summary>

**Task**: Build a complete Spring MVC endpoint serving Google Protocol Buffers for mobile clients and standard JSON for web browsers, verifying content negotiation via automated `@WebMvcTest`.
</details>

---

## 12. Summary & Quick Reference

- **Standard HTTP Flow**: Client sends `Accept` header with optional `q` factor weights.
- **Spring Abstraction**: `ContentNegotiationManager` + `HttpMessageConverter`.
- **Enabling XML**: Add `jackson-dataformat-xml`.
- **Query Parameter**: Enable via `configurer.favorParameter(true)`.
- **Golden Rule**: Always use `extendMessageConverters` to add custom converters without losing defaults.

---

## 13. Further Reading

- [Spring Framework Documentation: Content Negotiation](https://docs.spring.io/spring-framework/reference/web/webmvc/mvc-config/content-negotiation.html)
- [RFC 9110: HTTP Semantics - Content Negotiation](https://www.rfc-editor.org/rfc/rfc9110.html#section-12)
- [Jackson Dataformat XML Project](https://github.com/FasterXML/jackson-dataformat-xml)
- [OWASP: Preventing Reflected File Download (RFD) Attacks](https://owasp.org/www-community/attacks/Reflected_File_Download)
