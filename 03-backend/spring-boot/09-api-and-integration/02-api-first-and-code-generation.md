# API-First Development and Code Generation

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x |
| **Java Version** | 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | `openapi-generator-maven-plugin:7.11.0` |

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
In distributed enterprise architectures, multiple teams often consume and produce HTTP services simultaneously. When adopting a code-first approach, backend engineers implement controllers first and generate documentation afterwards. This workflow introduces severe friction: frontend and mobile teams are blocked waiting for backend deployments, changes to request/response models trigger unexpected breaking changes, and cross-team contract reviews happen too late in the software lifecycle.

### The Evolution
API-First development reverses this sequence: the OpenAPI Specification (OAS 3.0/3.1) file acts as the formal, authoritative contract negotiated and finalized before any code is written. Tools like OpenAPI Generator compile this YAML/JSON specification into server interfaces, client SDKs, and data transfer models (records/POJOs) during build time.

### Relevance in Modern Engineering
With Spring Boot 3 and Java 21, API-First development guarantees strict compliance between interface contracts and implementations. Generated interfaces leverage `interface` defaults and Spring annotations (`@RequestMapping`, `@RequestParam`, `@Valid`), leaving developers free to implement only domain business logic in clean `@RestController` classes.

---

## 2. Core Concepts

- **Authoritative Contract**: An OpenAPI YAML/JSON document stored in version control that serves as the single source of truth across engineering teams.
- **OpenAPI Generator Plugin**: A Maven/Gradle plugin (`openapi-generator-maven-plugin`) that reads the OAS file and emits Java interfaces, model classes, and validation annotations.
- **Delegate Pattern (`useSpringBoot3=true`, `delegatePattern=true`)**: Generator architecture where the controller interface delegates execution to an injectable `@Service` or delegate component, decoupling routing from business logic.
- **Model immutability with Java Records**: Modern generator option emitting Java records or immutable POJOs for DTO schemas.
- **Generated Sources Directory**: Output directory (e.g., `target/generated-sources/openapi`) added to the compiler classpath so generated code is never manually modified or committed to Git.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------+
|                        API-First Code Generation Pipeline                         |
|                                                                                   |
| 1. Contract Authoring                                                             |
|    +-----------------------------+                                                |
|    | src/main/resources/api.yaml | ---> OpenAPI 3.1 Contract                      |
|    +-----------------------------+                                                |
|                   |                                                               |
|                   v (mvn generate-sources)                                        |
| 2. OpenAPI Generator Engine                                                       |
|    +-----------------------------+                                                |
|    | Mustache Templates Engine   | ---> Applies spring / java21 template mappings |
|    +-----------------------------+                                                |
|                   |                                                               |
|                   v                                                               |
| 3. Generated Java Artifacts (target/generated-sources/openapi)                    |
|    +-----------------------------+     +-----------------------------+            |
|    | Models (e.g. OrderDto)      |     | Api Interface (OrdersApi)   |            |
|    +-----------------------------+     +-----------------------------+            |
|                   \                           /                                   |
|                    v                         v                                    |
| 4. Developer Implementation                                                       |
|    +---------------------------------------------------------------------+        |
|    | @RestController public class OrderController implements OrdersApi   |        |
|    +---------------------------------------------------------------------+        |
+-----------------------------------------------------------------------------------+
```

During the `generate-sources` phase of the build, the Maven plugin invokes the generator core. It parses the YAML schema into an AST (Abstract Syntax Tree), applies Mustache code templates configured for `spring` language generator, evaluates generator options (such as `interfaceOnly=true`), and emits Java source files into `target/generated-sources/openapi`. The `build-helper-maven-plugin` then binds this directory as a recognized compile source root.

---

## 4. Syntax & Usage

### Maven Plugin Configuration
```xml
<plugin>
    <groupId>org.openapitools</groupId>
    <artifactId>openapi-generator-maven-plugin</artifactId>
    <version>7.11.0</version>
    <executions>
        <execution>
            <goals>
                <goal>generate</goal>
            </goals>
            <configuration>
                <inputSpec>${project.basedir}/src/main/resources/openapi/orders-api.yaml</inputSpec>
                <generatorName>spring</generatorName>
                <apiPackage>com.example.platform.api</apiPackage>
                <modelPackage>com.example.platform.model</modelPackage>
                <supportingFilesToGenerate>ApiUtil.java</supportingFilesToGenerate>
                <configOptions>
                    <useSpringBoot3>true</useSpringBoot3>
                    <interfaceOnly>true</interfaceOnly>
                    <useTags>true</useTags>
                    <openApiNullable>false</openApiNullable>
                    <generateConstructorWithAllArgs>true</generateConstructorWithAllArgs>
                    <useBeanValidation>true</useBeanValidation>
                    <skipDefaultInterface>true</skipDefaultInterface>
                </configOptions>
            </configuration>
        </execution>
    </executions>
</plugin>
```

---

## 5. Practical Examples

### Example 1: OpenAPI 3.1 YAML Contract (`orders-api.yaml`)
```yaml
openapi: 3.1.0
info:
  title: Order Fulfillment API
  version: 1.0.0
  description: API for creating and querying enterprise fulfillment orders.
paths:
  /orders:
    post:
      summary: Place a new order
      operationId: createOrder
      tags:
        - Orders
      requestBody:
        required: true
        content:
          application/json:
            schema:
              $ref: '#/components/schemas/CreateOrderRequest'
      responses:
        '201':
          description: Order accepted
          content:
            application/json:
              schema:
                $ref: '#/components/schemas/OrderResponse'
components:
  schemas:
    CreateOrderRequest:
      type: object
      required:
        - customerId
        - items
      properties:
        customerId:
          type: string
          format: uuid
        items:
          type: array
          minItems: 1
          items:
            $ref: '#/components/schemas/OrderItem'
    OrderItem:
      type: object
      required:
        - sku
        - quantity
      properties:
        sku:
          type: string
        quantity:
          type: integer
          minimum: 1
    OrderResponse:
      type: object
      required:
        - orderId
        - status
      properties:
        orderId:
          type: string
          format: uuid
        status:
          type: string
          enum: [PENDING, CONFIRMED, REJECTED]
```

### Example 2: Controller Implementing the Generated Interface
```java
package com.example.platform.controller;

import com.example.platform.api.OrdersApi;
import com.example.platform.model.CreateOrderRequest;
import com.example.platform.model.OrderResponse;
import com.example.platform.service.OrderFulfillmentService;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.RestController;

@RestController
public class OrderController implements OrdersApi {

    private final OrderFulfillmentService fulfillmentService;

    public OrderController(OrderFulfillmentService fulfillmentService) {
        this.fulfillmentService = fulfillmentService;
    }

    @Override
    public ResponseEntity<OrderResponse> createOrder(CreateOrderRequest createOrderRequest) {
        OrderResponse response = fulfillmentService.processOrder(createOrderRequest);
        return ResponseEntity.status(HttpStatus.CREATED).body(response);
    }
}
```

### Example 3: Delegate Pattern Implementation
```java
// Configuration option: <delegatePattern>true</delegatePattern>
package com.example.platform.delegate;

import com.example.platform.api.OrdersApiDelegate;
import com.example.platform.model.CreateOrderRequest;
import com.example.platform.model.OrderResponse;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.stereotype.Component;

import java.util.UUID;

@Component
public class OrdersApiDelegateImpl implements OrdersApiDelegate {

    @Override
    public ResponseEntity<OrderResponse> createOrder(CreateOrderRequest request) {
        OrderResponse response = new OrderResponse();
        response.setOrderId(UUID.randomUUID());
        response.setStatus(OrderResponse.StatusEnum.CONFIRMED);
        return new ResponseEntity<>(response, HttpStatus.CREATED);
    }
}
```

### Example 4: Custom Type Mapping (Anti-Pattern Fix)
```xml
<!-- ANTI-PATTERN: Allowing the generator to produce java.time.OffsetDateTime when domain models strictly require java.time.Instant -->
<!-- FIX: Explicitly specify importMappings and typeMappings in the plugin configuration -->
<configuration>
    <typeMappings>
        <typeMapping>DateTime=Instant</typeMapping>
    </typeMappings>
    <importMappings>
        <importMapping>Instant=java.time.Instant</importMapping>
    </importMappings>
</configuration>
```

### Example 5: High-Performance Reactive Spring WebClient SDK Generation
```xml
<!-- Generating a type-safe WebClient HTTP Client SDK from the OpenAPI contract for downstream consumers -->
<execution>
    <id>generate-client</id>
    <goals>
        <goal>generate</goal>
    </goals>
    <configuration>
        <inputSpec>${project.basedir}/src/main/resources/openapi/orders-api.yaml</inputSpec>
        <generatorName>java</generatorName>
        <library>webclient</library>
        <apiPackage>com.example.platform.client.api</apiPackage>
        <modelPackage>com.example.platform.client.model</modelPackage>
        <configOptions>
            <useSpringBoot3>true</useSpringBoot3>
            <dateLibrary>java8</dateLibrary>
        </configOptions>
    </configuration>
</execution>
```

---

## 6. Best Practices

1. **Keep Specifications in Version Control**: Treat the OAS YAML file as the absolute single source of truth; never modify generated code directly.
2. **Set `interfaceOnly=true`**: Generate clean Java interfaces containing Spring MVC annotations; avoid generating stub controller implementations.
3. **Automate Compilation via Build Helper**: Use `build-helper-maven-plugin` to register `target/generated-sources/openapi/src/main/java` as a source directory.
4. **Use Strict Linting on Contracts**: Integrate `spectral` or `vacuum` in CI to enforce naming conventions, description completeness, and version semantics before code generation.
5. **Enable `useSpringBoot3=true`**: Ensure generated annotations use `jakarta.*` packages instead of obsolete `javax.*`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Committing generated Java files to Git repository | Creates merge conflicts and duplicates source files across branch merges | Add `target/` and generated code paths to `.gitignore`. |
| Missing `useSpringBoot3=true` | Emits `javax.validation` and `javax.servlet` imports, causing build failure in Spring Boot 3 | Configure `<useSpringBoot3>true</useSpringBoot3>` in plugin config options. |
| Neglecting `build-helper-maven-plugin` | IDE fails to recognize generated classes, showing false compilation errors | Add `build-helper-maven-plugin` with `add-source` goal targeting generated directory. |
| Modifying generated model classes directly | Manual edits are wiped out on the next `mvn clean compile` | Subclass generated models or map them to internal domain entities using MapStruct. |
| Incompatible Date/Time mappings | Leads to serialization mismatches between ISO-8601 strings and Java types | Specify `<dateLibrary>java8</dateLibrary>` and explicit `typeMappings`. |
| Unspecified `operationId` in OpenAPI paths | Generator produces generic method names like `ordersPost()` | Always define explicit, camelCase `operationId` (e.g., `createOrder`). |

---

## 8. Performance & Security Notes

- **Build Time Impact**: Code generation adds seconds to local builds. Optimize by configuring incremental builds or binding generation to specific build profiles.
- **Input Validation Invariants**: Generated models automatically include `@Valid`, `@NotNull`, and `@Pattern` based on OpenAPI schema constraints, defending against injection attacks at the boundary.
- **Contract Integrity**: API-First guarantees that endpoints reject undeclared payload formats by enforcing strict Content-Type headers (`application/json`).

---

## 9. Comparison / When to Use What

| Feature | Code-First (SpringDoc) | API-First (OpenAPI Generator) |
|---|---|---|
| **Source of Truth** | Java Controller / Record Code | OpenAPI YAML / JSON File |
| **Cross-Team Collaboration** | Difficult; specs appear after backend development | Optimal; client & server teams agree upfront |
| **Client SDK Generation** | Post-deployment extraction | Pre-implementation during build |
| **Refactoring Burden** | Low (refactor code directly) | Medium (update YAML, regenerate, adjust impl) |
| **Best For** | Internal monolithic services | Enterprise platforms, microservices, multi-team APIs |

---

## 10. Interview Questions

<details>
<summary>1. What is the fundamental difference between Code-First and API-First development?</summary>
In Code-First, developers write controller logic and data structures, deriving the documentation automatically after implementation. In API-First, the API contract is authored and agreed upon first using OpenAPI specifications, after which server interfaces and client SDKs are generated automatically.
</details>

<details>
<summary>2. Why should developers set interfaceOnly=true when using the OpenAPI Generator for Spring Boot?</summary>
Setting `interfaceOnly=true` produces clean Java interfaces with `@RequestMapping` and validation annotations without generating default controller classes. This leaves the developer free to implement the interface in standard `@RestController` components while maintaining clean separation of concerns.
</details>

<details>
<summary>3. What is the role of build-helper-maven-plugin in API-First workflows?</summary>
The plugin adds the generated sources directory (e.g., `target/generated-sources/openapi/src/main/java`) to the Maven project's compile path, ensuring the Java compiler and IDE locate generated classes seamlessly without committing them to version control.
</details>

<details>
<summary>4. How does the delegate pattern in OpenAPI Generator work?</summary>
When `delegatePattern=true` is enabled, the generator creates a Controller that delegates HTTP requests to an injectable `ApiDelegate` interface. Developers implement the delegate bean rather than the controller, isolating business logic from HTTP mechanics.
</details>

<details>
<summary>5. How do you resolve javax.* vs jakarta.* package collisions with OpenAPI Generator?</summary>
By setting `<useSpringBoot3>true</useSpringBoot3>` in the generator configuration options. This instructs the generator templates to output `jakarta.validation.*`, `jakarta.annotation.*`, and Jakarta EE imports compatible with Spring Boot 3.
</details>

<details>
<summary>6. How can you customize type mappings in OpenAPI Generator?</summary>
Using `<typeMappings>` and `<importMappings>` tags in the plugin configuration. For example, mapping `DateTime=Instant` and importing `java.time.Instant` overrides the default `OffsetDateTime`.
</details>

<details>
<summary>7. How does operationId in the OpenAPI spec influence generated Java code?</summary>
The `operationId` directly dictates the method name generated in the Java interface (e.g., `operationId: findUserById` produces `ResponseEntity<UserDto> findUserById(...)`).
</details>

<details>
<summary>8. How can you prevent breaking changes when updating OpenAPI specs?</summary>
By using schema diff tools (like `oasdiff` or OpenAPI breaking-change checkers) in CI pipelines to compare the new spec against the published production contract before triggering code generation.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1 (Easy): Create a minimal OpenAPI 3.1 YAML file for a health endpoint</summary>
Write an `api.yaml` defining a GET `/ping` endpoint returning a 200 response with schema `{ "pong": true }`.
</details>

<details>
<summary>Exercise 2 (Easy): Configure the OpenAPI Generator Maven plugin</summary>
Write a `pom.xml` snippet configuring `openapi-generator-maven-plugin` to generate server interfaces with `interfaceOnly=true` and `useSpringBoot3=true`.
</details>

<details>
<summary>Exercise 3 (Medium): Implement a generated Spring MVC interface</summary>
Given a generated interface `UsersApi` with method `createUser(UserRequest userRequest)`, implement a `@RestController` that validates the request and returns HTTP 201.
</details>

<details>
<summary>Exercise 4 (Medium): Map custom types for currency amounts</summary>
Configure `typeMappings` to convert OpenAPI schema type `number` with format `currency` to `java.math.BigDecimal`.
</details>

<details>
<summary>Exercise 5 (Hard): Multi-module Maven setup with dedicated API contract module</summary>
Design a Maven multi-module architecture where an `api-contract` module compiles the YAML into a JAR of interfaces and DTOs consumed by both client and server modules.
</details>

<details>
<summary>Exercise 6 (Hard): Generate a reactive WebClient SDK from the same specification</summary>
Configure a secondary execution in `pom.xml` that compiles the OpenAPI spec into an asynchronous `WebClient` client library.
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------+
|                         API-First Generation Summary                            |
+---------------------------------------------------------------------------------+
| Plugin:             openapi-generator-maven-plugin                              |
| Generator Name:     spring (server) or java (client)                            |
| Critical Options:   useSpringBoot3 = true (Jakarta EE support)                  |
|                     interfaceOnly = true (pure interface generation)            |
|                     useTags = true (grouping methods by tag)                    |
| Directory:          target/generated-sources/openapi                            |
| Helper:             build-helper-maven-plugin (adds compile path)               |
| Invariant:          Never edit generated files; modify YAML contract instead    |
+---------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [OpenAPI Generator Official Documentation](https://openapi-generator.tech/)
- [OpenAPI Tools Maven Plugin Guide](https://github.com/OpenAPITools/openapi-generator/tree/master/modules/openapi-generator-maven-plugin)
- [Spectral OpenAPI Linter](https://stoplight.io/open-source/spectral)
