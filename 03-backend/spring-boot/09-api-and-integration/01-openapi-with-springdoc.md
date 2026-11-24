# OpenAPI 3.1 with SpringDoc Starter

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x |
| **Java Version** | 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | `org.springdoc:springdoc-openapi-starter-webmvc-ui:2.8.0` |

---

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Usage](#4-syntax--usage)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices](#6-best-practices)
- [7. Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
- [8. Performance & Security Notes](#8-performance--security-notes)
- [9. Comparison / When to Use What](#9-comparison--when-to-use-what)
- [10. Interview Questions](#10-interview-questions)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading](#13-further-reading)

---

## 1. What & Why

### The Problem
Historically, maintaining API documentation required manual synchronization between code and external specification files. Developers frequently altered endpoint signatures, added required query parameters, or shifted response schemas without updating the documentation. This divergence led to broken client SDKs, frustrated consumer teams, and elevated support overhead. Earlier solutions like SpringFox struggled with the architectural shift to Spring Boot 3, modular Jakarta EE packages, and modern WebMvc changes.

### The Evolution
OpenAPI 3.1 represents the convergence of the OpenAPI Specification (OAS) and the JSON Schema 2020-12 draft, supporting full JSON Schema polymorphism, type arrays, and webhooks. In the Spring ecosystem, `springdoc-openapi` emerged as the standard OpenAPI 3 library for Spring Boot 3.x, leveraging bytecode analysis, Spring MVC metadata, and swagger-core v2 annotations to dynamically produce OAS 3.1 documents alongside an interactive Swagger UI.

### Relevance in Modern Engineering
In microservice and platform architectures, contracts are foundational. SpringDoc provides zero-configuration generation of living specifications directly from Java records, controllers, and validation annotations (`jakarta.validation.constraints.*`), powering automated client generation, gateway routing, and interactive contract testing.

---

## 2. Core Concepts

- **OpenAPI Specification (OAS 3.1)**: Machine-readable JSON/YAML description of HTTP APIs, defining endpoints, parameters, request bodies, authentication flows, and response payloads.
- **SpringDoc Starter**: An auto-configured library (`springdoc-openapi-starter-webmvc-ui`) that registers endpoints (by default `/v3/api-docs` and `/swagger-ui/index.html`) using Spring Boot autoconfiguration.
- **GroupedOpenApi**: A configuration construct allowing developers to segment endpoints into distinct API groups (e.g., Public APIs, Internal Operations, Partner Integrations) based on URL paths or packages.
- **Customizer Beans**: Functional interfaces (`OpenApiCustomizer`, `OperationCustomizer`) allowing programmatic modification of schemas, operations, or headers at runtime.
- **Schema Resolution**: Reflection-based mapping of Java classes and records to OpenAPI schemas, automatically detecting validation constraints like `@NotNull`, `@Size`, and `@Pattern`.

---

## 3. How It Works Internally

```
+---------------------------------------------------------------------------------+
|                       SpringDoc Boot Initialization Engine                      |
|                                                                                 |
| 1. ApplicationContext Starts                                                    |
|    +-----------------------------+                                              |
|    | Scan Spring MVC Handlers    | ---> Reads @RestController & @RequestMapping |
|    +-----------------------------+                                              |
|                   |                                                             |
|                   v                                                             |
| 2. Bytecode & Parameter Parsing                                                 |
|    +-----------------------------+                                              |
|    | Spring Method Introspection | ---> Resolves Record Components & Validations|
|    +-----------------------------+                                              |
|                   |                                                             |
|                   v                                                             |
| 3. Schema Assembly & Customization                                              |
|    +-----------------------------+                                              |
|    | OpenAPI Bean Generation     | ---> Applies OpenApiCustomizer & Components  |
|    +-----------------------------+                                              |
|                   |                                                             |
|                   v                                                             |
| 4. Endpoint Serving                                                             |
|    +---------------------------------------------------------------------+      |
|    | GET /v3/api-docs (JSON/YAML)  <==>  GET /swagger-ui/index.html (UI) |      |
|    +---------------------------------------------------------------------+      |
+---------------------------------------------------------------------------------+
```

When Spring Boot initializes, SpringDoc registers a custom handler mapping that inspects all `RequestMappingHandlerMapping` beans. It extracts HTTP methods, URI templates, headers, parameters, and return types. Next, it passes schema types through `ModelConverters` (from Swagger Core), which parses Java records and classes into OpenAPI `Schema` objects. Finally, all registered `OpenApiCustomizer` and `OperationCustomizer` beans execute sequentially before the finalized `OpenAPI` object is serialized and cached.

---

## 4. Syntax & Usage

### Maven Dependency
```xml
<dependency>
    <groupId>org.springdoc</groupId>
    <artifactId>springdoc-openapi-starter-webmvc-ui</artifactId>
    <version>2.8.0</version>
</dependency>
```

### Essential Application Properties
```properties
# SpringDoc Configuration
springdoc.api-docs.path=/v3/api-docs
springdoc.swagger-ui.path=/swagger-ui.html
springdoc.swagger-ui.operations-sorter=alpha
springdoc.swagger-ui.tags-sorter=alpha
springdoc.swagger-ui.display-request-duration=true
springdoc.show-actuator=false
```

---

## 5. Practical Examples

### Example 1: Basic Global OpenAPI Configuration Bean
```java
package com.example.platform.config;

import io.swagger.v3.oas.models.ExternalDocumentation;
import io.swagger.v3.oas.models.OpenAPI;
import io.swagger.v3.oas.models.info.Contact;
import io.swagger.v3.oas.models.info.Info;
import io.swagger.v3.oas.models.info.License;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;

@Configuration
public class OpenApiConfig {

    @Bean
    public OpenAPI customOpenAPI() {
        return new OpenAPI()
                .info(new Info()
                        .title("Platform Payment Engine API")
                        .version("v1.0.0")
                        .description("Core transaction processing and ledger management APIs.")
                        .contact(new Contact().name("API Platform Team").email("platform@example.com"))
                        .license(new License().name("Apache 2.0").url("https://springdoc.org")))
                .externalDocs(new ExternalDocumentation()
                        .description("Architecture Decision Records (ADRs)")
                        .url("https://docs.internal.example.com/adr"));
    }
}
```

### Example 2: Documenting Controllers with Java Records and Validation Constraints
```java
package com.example.platform.controller;

import io.swagger.v3.oas.annotations.Operation;
import io.swagger.v3.oas.annotations.Parameter;
import io.swagger.v3.oas.annotations.media.Content;
import io.swagger.v3.oas.annotations.media.Schema;
import io.swagger.v3.oas.annotations.responses.ApiResponse;
import io.swagger.v3.oas.annotations.responses.ApiResponses;
import io.swagger.v3.oas.annotations.tags.Tag;
import jakarta.validation.Valid;
import jakarta.validation.constraints.DecimalMin;
import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.NotNull;
import org.springframework.http.HttpStatus;
import org.springframework.http.ProblemDetail;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.math.BigDecimal;
import java.util.UUID;

@RestController
@RequestMapping("/api/v1/payments")
@Tag(name = "Payments", description = "Operations for authorizing and capturing financial transactions")
public class PaymentController {

    public record PaymentRequest(
            @Schema(description = "Customer UUID", example = "a1b2c3d4-e5f6-7a8b-9c0d-1e2f3a4b5c6d")
            @NotNull UUID customerId,

            @Schema(description = "Transaction monetary value", example = "149.50")
            @DecimalMin("0.01") BigDecimal amount,

            @Schema(description = "Three-letter ISO-4217 currency code", example = "USD")
            @NotBlank String currency
    ) {}

    public record PaymentResponse(
            @Schema(description = "Unique ledger transaction ID", example = "tx_987654321")
            String transactionId,
            @Schema(description = "Execution status", example = "SETTLED")
            String status
    ) {}

    @Operation(summary = "Authorize a new payment", description = "Submits a payment instruction to the payment gateway.")
    @ApiResponses({
            @ApiResponse(responseCode = "201", description = "Payment successfully authorized",
                    content = @Content(schema = @Schema(implementation = PaymentResponse.class))),
            @ApiResponse(responseCode = "400", description = "Validation error",
                    content = @Content(schema = @Schema(implementation = ProblemDetail.class))),
            @ApiResponse(responseCode = "422", description = "Business rule violation",
                    content = @Content(schema = @Schema(implementation = ProblemDetail.class)))
    })
    @PostMapping
    public ResponseEntity<PaymentResponse> authorizePayment(
            @Parameter(description = "Idempotency key for preventing duplicate charges")
            @RequestHeader(value = "Idempotency-Key", required = true) String idempotencyKey,
            @Valid @RequestBody PaymentRequest request) {
        
        PaymentResponse response = new PaymentResponse("tx_" + UUID.randomUUID().toString().substring(0, 8), "SETTLED");
        return ResponseEntity.status(HttpStatus.CREATED).body(response);
    }
}
```

### Example 3: Segmenting APIs with GroupedOpenApi
```java
package com.example.platform.config;

import org.springdoc.core.models.GroupedOpenApi;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;

@Configuration
public class ApiGroupingConfig {

    @Bean
    public GroupedOpenApi publicApi() {
        return GroupedOpenApi.builder()
                .group("public-v1")
                .pathsToMatch("/api/v1/**")
                .pathsToExclude("/api/v1/internal/**")
                .build();
    }

    @Bean
    public GroupedOpenApi internalApi() {
        return GroupedOpenApi.builder()
                .group("internal-v1")
                .pathsToMatch("/api/v1/internal/**")
                .build();
    }
}
```

### Example 4: Security Scheme Customization (JWT Bearer Token Anti-Pattern Fix)
```java
// ANTI-PATTERN: Forgetting to declare SecuritySchemes, requiring developers to write documentation comments instructing curl users to manually pass Authorization headers.
// FIX: Programmatically register the JWT bearer security scheme and apply it globally.

package com.example.platform.config;

import io.swagger.v3.oas.models.Components;
import io.swagger.v3.oas.models.OpenAPI;
import io.swagger.v3.oas.models.security.SecurityRequirement;
import io.swagger.v3.oas.models.security.SecurityScheme;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;

@Configuration
public class SecurityOpenApiConfig {

    private static final String SECURITY_SCHEME_NAME = "BearerAuth";

    @Bean
    public OpenAPI securityOpenAPI() {
        return new OpenAPI()
                .addSecurityItem(new SecurityRequirement().addList(SECURITY_SCHEME_NAME))
                .components(new Components()
                        .addSecuritySchemes(SECURITY_SCHEME_NAME,
                                new SecurityScheme()
                                        .name(SECURITY_SCHEME_NAME)
                                        .type(SecurityScheme.Type.HTTP)
                                        .scheme("bearer")
                                        .bearerFormat("JWT")
                                        .description("Provide valid Bearer JWT access token.")));
    }
}
```

### Example 5: High-Performance Customizer for Automated Correlation ID Headers
```java
package com.example.platform.config;

import io.swagger.v3.oas.models.media.StringSchema;
import io.swagger.v3.oas.models.parameters.Parameter;
import org.springdoc.core.customizers.OperationCustomizer;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.web.method.HandlerMethod;

@Configuration
public class CorrelationHeaderCustomizerConfig {

    @Bean
    public OperationCustomizer addCorrelationIdHeader() {
        return (operation, handlerMethod) -> {
            Parameter correlationHeader = new Parameter()
                    .in("header")
                    .name("X-Correlation-Id")
                    .description("Distributed tracing correlation identifier")
                    .required(false)
                    .schema(new StringSchema().example("c4a8f15d-8b43-412f-90e6-58be167d3419"));

            operation.addParametersItem(correlationHeader);
            return operation;
        };
    }
}
```

---

## 6. Best Practices

1. **Use Records for DTOs**: SpringDoc seamlessly recognizes Java record component accessors and extracts types without boilerplate getters.
2. **Standardize Error Schemas**: Map RFC 7807 `ProblemDetail` into global responses so all 4xx/5xx responses share an identical OpenAPI schema contract.
3. **Disable Swagger UI in Production**: Restrict UI availability in production environments using conditional properties: `springdoc.swagger-ui.enabled=false`.
4. **Leverage GroupedOpenApi for Multiple Audiences**: Separate public external client contracts from internal operational endpoints to prevent accidental leakage of sensitive back-office routes.
5. **Enforce OpenAPI Spec Generation at Build Time**: Use `springdoc-openapi-maven-plugin` to generate static `openapi.json` files during the verify phase for CI schema validation.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Retaining obsolete `springfox` dependencies | Causes classpath collisions and runtime `ClassNotFoundException` in Spring Boot 3 | Remove all `io.springfox` dependencies; migrate exclusively to `org.springdoc:springdoc-openapi-starter-webmvc-ui`. |
| Missing `@NotNull` or `@NotBlank` on record components | Generated schema marks fields as optional, leading to client drift | Annotate record components with `jakarta.validation.constraints` annotations. |
| Hardcoding server URLs in OpenAPI beans | Swagger UI points to localhost even when deployed in staging or Kubernetes clusters | Omit static `Server` lists or dynamically compute host headers using request-aware customizers. |
| Exposing actuator endpoints in public OpenAPI specs | Reveals sensitive health, metric, and environment endpoints | Set `springdoc.show-actuator=false` (default) or isolate actuator paths into an internal group. |
| Duplicating documentation across annotations and code | Out-of-sync documentation creates maintenance friction | Let SpringDoc infer types, parameters, and defaults naturally from Spring MVC annotations. |
| Forgetting to document multipart file uploads | OpenAPI renders request body as JSON instead of `multipart/form-data` | Annotate file parameter with `@Parameter(content = @Content(mediaType = MediaType.MULTIPART_FORM_DATA_VALUE))`. |

---

## 8. Performance & Security Notes

- **Documentation Caching**: SpringDoc compiles and caches the OpenAPI schema on first request. Ensure pre-caching occurs if cold-start latency is a constraint in serverless environments.
- **OWASP Information Disclosure**: Prevent exposing stack traces or inner exception class names in Swagger schemas. Sanitize all error responses via `@RestControllerAdvice`.
- **RBAC on Swagger UI**: Secure `/swagger-ui/**` and `/v3/api-docs/**` behind Spring Security authorization roles (e.g., `hasAuthority('SWAGGER_VIEW')`) in non-public environments.

---

## 9. Comparison / When to Use What

| Dimension | SpringDoc OpenAPI (Code-First) | OpenAPI Generator (Spec-First) | Spring RestDocs (Test-Driven) |
|---|---|---|---|
| **Approach** | Annotate controllers and records | Write YAML/JSON spec first, generate code | Write MockMvc tests, generate snippets |
| **Speed of Iteration** | Extremely fast; direct code refactoring | Medium; requires schema editing & maven rebuild | Slowest; requires comprehensive test authoring |
| **Contract Rigor** | Dependent on developer discipline | Maximum; schema is single source of truth | High; verified against real test assertions |
| **Primary Use Case** | Agile microservices, rapid internal APIs | Enterprise platform APIs, multi-language teams | Strict enterprise governance, public SDKs |

---

## 10. Interview Questions

<details>
<summary>1. How does SpringDoc differ from legacy SpringFox in Spring Boot 3?</summary>
SpringFox relied on Spring MVC's legacy path matching, deprecated javax packages, and internal reflection mechanisms that ceased functioning in Spring Boot 3 / Jakarta EE. SpringDoc was architected from the ground up for OpenAPI 3, Jakarta EE, and modern Spring Boot autoconfigurations, featuring active maintenance, native image support, and seamless integration with Spring WebMvc and WebFlux.
</details>

<details>
<summary>2. How does SpringDoc infer schema properties from Java records?</summary>
Java records provide canonical record components at the bytecode level. SpringDoc inspects the record component names, types, and annotations (such as `@NotNull`, `@Size`, `@Pattern`) via reflection, mapping them to OpenAPI 3.1 properties and setting `required = true` whenever non-null constraints are discovered.
</details>

<details>
<summary>3. What is the role of OpenApiCustomizer?</summary>
`OpenApiCustomizer` is a functional callback interface that allows programmatic modification of the aggregated `OpenAPI` object before it is served or serialized. It is commonly used to inject common security definitions, environment servers, license headers, and organization-wide metadata.
</details>

<details>
<summary>4. How can you expose Actuator endpoints in the OpenAPI documentation?</summary>
Actuator endpoints can be exposed by setting `springdoc.show-actuator=true` in `application.properties`. When enabled, SpringDoc inspects actuator endpoint mappings and documents them under an Actuator tag.
</details>

<details>
<summary>5. How does SpringDoc handle generic response wrappers like ResponseEntity&lt;T&gt; or Page&lt;T&gt;?</summary>
SpringDoc unwraps container classes such as `ResponseEntity<T>`, `HttpEntity<T>`, and `CompletableFuture<T>` to expose the underlying type `T` in the schema. For Spring Data's `Page<T>`, SpringDoc provides `springdoc.model-and-view-allowed=true` and custom pageable converters to map pagination query parameters (`page`, `size`, `sort`) cleanly.
</details>

<details>
<summary>6. How can you disable Swagger UI in production while retaining OpenAPI JSON for automated tools?</summary>
Set `springdoc.swagger-ui.enabled=false` while keeping `springdoc.api-docs.enabled=true`. This prevents UI asset exposure while enabling API gateways or observability agents to fetch raw JSON schemas.
</details>

<details>
<summary>7. What is the difference between @Operation and @ApiResponse annotations?</summary>
`@Operation` describes the HTTP method, summary, detailed description, and tags for a specific endpoint. `@ApiResponse` defines specific HTTP status codes (e.g., 200, 400, 500) and describes the structure and MIME type of the payload returned for each status.
</details>

<details>
<summary>8. How do you generate the OpenAPI specification during Maven/Gradle builds?</summary>
You use the `springdoc-openapi-maven-plugin` or `springdoc-openapi-gradle-plugin`. The build plugin starts the Spring Boot application in a background process, issues a GET request to `/v3/api-docs`, writes the resulting JSON/YAML to the target directory, and shuts down the application.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1 (Easy): Register an OpenAPI bean with contact and license metadata</summary>
Create a `@Configuration` class named `ApiDocConfig` that returns an `OpenAPI` bean defining API title "Inventory Service", version "2.1.0", and a contact email "support@example.com".
</details>

<details>
<summary>Exercise 2 (Easy): Document a health check endpoint with Swagger annotations</summary>
Author a `@RestController` with a GET `/health` endpoint annotated with `@Operation` and `@ApiResponse(responseCode = "200")` returning a record `HealthStatus(String status, long uptimeMs)`.
</details>

<details>
<summary>Exercise 3 (Medium): Configure grouped APIs for Admin and Public routes</summary>
Configure two `GroupedOpenApi` beans: one matching `/admin/**` with group name "admin-api", and another matching `/public/**` with group name "public-api".
</details>

<details>
<summary>Exercise 4 (Medium): Implement an OperationCustomizer to add an X-Request-Timestamp header</summary>
Write an `OperationCustomizer` bean that inspects all mutating operations (POST, PUT, DELETE) and injects an optional header parameter `X-Request-Timestamp` formatted as ISO-8601 string.
</details>

<details>
<summary>Exercise 5 (Hard): Implement Global RFC 7807 ProblemDetail response mappings</summary>
Configure an `OpenApiCustomizer` that automatically injects a 500 Internal Server Error response referencing `ProblemDetail` into every operation defined across the application.
</details>

<details>
<summary>Exercise 6 (Hard): Build-time static OpenAPI schema generation configuration</summary>
Construct a complete Maven `pom.xml` snippet configuring `springdoc-openapi-maven-plugin` tied to the `integration-test` phase to generate `openapi.json` without requiring a pre-running server.
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------+
|                         SpringDoc OpenAPI Quick Reference                       |
+---------------------------------------------------------------------------------+
| Dependency:        springdoc-openapi-starter-webmvc-ui                          |
| Endpoints:         /v3/api-docs (Raw OAS 3.1 JSON)                              |
|                    /swagger-ui/index.html (Interactive UI)                      |
| Key Annotations:   @Tag(name, description)         -> Logical grouping          |
|                    @Operation(summary, desc)       -> Endpoint behavior         |
|                    @ApiResponse(responseCode, ...) -> Status & Schema mapping   |
|                    @Parameter(description, ...)    -> Param & header metadata   |
|                    @Schema(example, description)   -> Model property details    |
| Security:          Components.addSecuritySchemes   -> Bearer / OAuth2 / ApiKey  |
| Build Plugin:      springdoc-openapi-maven-plugin  -> Static file emission      |
+---------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Official SpringDoc Documentation](https://springdoc.org/)
- [OpenAPI Specification 3.1.0](https://spec.openapis.org/oas/v3.1.0)
- [RFC 7807 Problem Details for HTTP APIs](https://datatracker.ietf.org/doc/html/rfc7807)
