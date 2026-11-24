# Contract-Driven Inter-Service Communication: OpenAPI and gRPC

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x |
| **Java Version** | 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | OpenAPI Generator 7.11+, Protocol Buffers (proto3), Spring gRPC |

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
In multi-team microservice ecosystems, inter-service integration is historically the largest source of runtime bugs, deployment friction, and unexpected production outages. When services communicate using informal, ad-hoc JSON payloads without authoritative machine-readable contracts, breaking changes slip through unnoticed: a producer service changes a field name from `customerId` to `userId`, removes an optional field, or alters a date format, silently crashing downstream consumer services at runtime.

### The Evolution
**Contract-First (or Schema-First) Development** enforces that machine-readable specifications—either **OpenAPI 3.1 (YAML/JSON)** for RESTful HTTP APIs or **Protocol Buffers (`.proto`)** for gRPC binary RPCs—act as the single, authoritative source of truth. The contract is finalized, versioned, and published before writing implementation code. Build plugins (OpenAPI Generator, `protoc`) automatically generate type-safe server interfaces and client SDK stubs at compile time.

### Relevance in Modern Engineering
With Spring Boot 3, contract-first communication eliminates manual DTO synchronization across microservices. Producer controllers simply implement generated Java interfaces, while consumer services inject generated, type-safe client stubs (`RestClient` SDKs or gRPC blocking stubs) with compile-time type safety.

---

## 2. Core Concepts

- **Authoritative Contract**: A formal schema document (`.yaml` or `.proto`) defining data structures, validation constraints, endpoints, RPC methods, and error envelopes.
- **Contract Repository / Artifact Registry**: Storing shared schema contracts in a dedicated Git repository or publishing compiled DTO JARs to an internal Maven/Nexus/Artifactory registry.
- **OpenAPI Generator Maven Plugin**: Generates Spring MVC `@RestController` interfaces, Java records, and `RestClient`/`WebClient` SDKs from OpenAPI specifications.
- **`protoc` (Protocol Buffer Compiler)**: Compiles `.proto` service definitions into compact binary serializers and gRPC client/server stubs.
- **Backward Compatibility Rules**:
  - Never rename existing fields.
  - Never change field types or tag numbers.
  - New fields must be optional with sensible defaults.
  - Removed fields must be marked `reserved` to prevent future tag reuse.
- **Consumer-Driven Contract Testing (Pact / Spring Cloud Contract)**: Verifying that producers satisfy consumer expectations before deploying to production.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------+
|                        Contract-First Development Pipeline                        |
|                                                                                   |
| 1. Contract Definition (Shared Repo / Nexus Artifact)                             |
|    orders-contract.yaml (OpenAPI)  OR  order_service.proto (Protobuf)             |
|                   |                                                               |
|                   v (Maven / Gradle Build Phase: generate-sources)                |
| 2. Code Generation (Zero Hand-Written DTOs or Interfaces)                         |
|    +-----------------------------+     +-----------------------------+            |
|    | Server Interface            |     | Client SDK / Stubs          |            |
|    | - OrdersApi.java            |     | - OrderClientSdk.java       |            |
|    | - OrderRequest.java (record)|     | - OrderServiceBlockingStub  |            |
|    +-----------------------------+     +-----------------------------+            |
|                   |                                   |                           |
|                   v Implements interface              v Injects generated stub    |
| 3. Producer Microservice (Order Service)       4. Consumer Microservice (Billing) |
|    @RestController                             @Service                           |
|    public class OrderController                public class BillingService {      |
|        implements OrdersApi { ... }                private OrderClientSdk client; |
|                                                }                                  |
|                   ^                                   |                           |
|                   |                                   |                           |
|                   +===== Network Call (REST or gRPC) =+                           |
|                          (Compile-Time Verified Guarantee!)                       |
+-----------------------------------------------------------------------------------+
```

During build time (`mvn compile`), plugins parse the schema specification and output Java sources into `target/generated-sources`. The Producer service implements the generated server interface—if the controller method signature does not strictly match the contract, the Java compiler halts the build. The Consumer service imports the generated client library and invokes methods with compile-time type safety.

---

## 4. Syntax & Usage

### Maven Plugin Configuration (OpenAPI Generator for Server & Client)
```xml
<plugin>
    <groupId>org.openapitools</groupId>
    <artifactId>openapi-generator-maven-plugin</artifactId>
    <version>7.11.0</version>
    <executions>
        <!-- Server Interface Generation -->
        <execution>
            <id>generate-server</id>
            <goals><goal>generate</goal></goals>
            <configuration>
                <inputSpec>${project.basedir}/src/main/resources/contracts/orders.yaml</inputSpec>
                <generatorName>spring</generatorName>
                <apiPackage>com.example.platform.orders.api</apiPackage>
                <modelPackage>com.example.platform.orders.model</modelPackage>
                <configOptions>
                    <interfaceOnly>true</interfaceOnly>
                    <useSpringBoot3>true</useSpringBoot3>
                    <useTags>true</useTags>
                </configOptions>
            </configuration>
        </execution>
    </executions>
</plugin>
```

---

## 5. Practical Examples

### Example 1: OpenAPI 3.1 Contract (`orders.yaml`)
```yaml
openapi: 3.1.0
info:
  title: Order Service Contract
  version: 1.0.0
paths:
  /orders/{orderId}:
    get:
      summary: Fetch order details
      operationId: getOrderById
      parameters:
        - name: orderId
          in: path
          required: true
          schema:
            type: string
            format: uuid
      responses:
        '200':
          description: Order found
          content:
            application/json:
              schema:
                $ref: '#/components/schemas/OrderDto'
        '404':
          description: Order not found
components:
  schemas:
    OrderDto:
      type: object
      required:
        - orderId
        - amount
        - status
      properties:
        orderId:
          type: string
          format: uuid
        amount:
          type: number
          format: double
        status:
          type: string
          enum: [PENDING, CONFIRMED, CANCELLED]
```

### Example 2: Producer Controller Implementing Generated Contract Interface
```java
package com.example.platform.orders.controller;

import com.example.platform.orders.api.OrdersApi;
import com.example.platform.orders.model.OrderDto;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.RestController;

import java.util.UUID;

@RestController
public class OrderController implements OrdersApi {

    @Override
    public ResponseEntity<OrderDto> getOrderById(UUID orderId) {
        // Implementation strictly complies with generated contract interface
        OrderDto order = new OrderDto();
        order.setOrderId(orderId);
        order.setAmount(199.95);
        order.setStatus(OrderDto.StatusEnum.CONFIRMED);

        return ResponseEntity.ok(order);
    }
}
```

### Example 3: Consumer Invoking Generated Type-Safe Client SDK
```java
package com.example.platform.billing.service;

import com.example.platform.orders.client.api.OrdersApiClient;
import com.example.platform.orders.client.model.OrderDto;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Service;

import java.util.UUID;

@Service
public class BillingReconciliationService {

    private static final Logger log = LoggerFactory.getLogger(BillingReconciliationService.class);
    private final OrdersApiClient ordersApiClient;

    public BillingReconciliationService(OrdersApiClient ordersApiClient) {
        this.ordersApiClient = ordersApiClient;
    }

    public void reconcileOrder(UUID orderId) {
        log.info("Fetching order [{}] using generated contract-first client SDK", orderId);

        // Fully type-safe client call; method name and types enforced at compile time
        OrderDto order = ordersApiClient.getOrderById(orderId);

        log.info("Reconciling billing for order [{}] with amount ${}", order.getOrderId(), order.getAmount());
    }
}
```

### Example 4: Manual DTO Duplication Anti-Pattern and Fix
```java
// ANTI-PATTERN: Manually writing duplicate DTOs in both Producer and Consumer repositories!
// When the producer adds or renames a field, the consumer compiles cleanly but fails silently at runtime!

// FIX: Generate DTOs automatically from the single shared OpenAPI or Protobuf contract file
```

### Example 5: High-Performance gRPC Protocol Buffer Contract (`order_service.proto`)
```protobuf
syntax = "proto3";

package com.example.platform.grpc;

option java_multiple_files = true;
option java_package = "com.example.platform.grpc.v1";

service OrderService {
    rpc GetOrderDetails (GetOrderRequest) returns (OrderResponse);
}

message GetOrderRequest {
    string order_id = 1;
}

message OrderResponse {
    string order_id = 1;
    double amount = 2;
    string status = 3;
    // Reserved tag numbers prevent re-using deleted field IDs
    reserved 4, 7 to 9;
}
```

---

## 6. Best Practices

1. **Treat Contracts as Single Source of Truth**: Keep schema files (`.yaml`, `.proto`) in version control; never modify generated Java classes directly.
2. **Publish Contract Artifacts to Nexus/Artifactory**: Package generated DTOs and client interfaces into a Maven JAR (e.g., `orders-api-contract:1.2.0.jar`) so consumers import versioned dependencies.
3. **Never Remove Fields Without Deprecation**: When updating contracts, add new fields as optional; mark deprecated fields with `deprecated: true` before removing them in future major versions.
4. **Use Strict Linting in CI Pipelines**: Integrate `spectral` (OpenAPI) or `buf` (Protobuf) in pull request checks to prevent breaking changes and enforce style conventions.
5. **Always Set `useSpringBoot3=true` in OpenAPI Generator**: Ensure generated code outputs `jakarta.*` packages rather than obsolete `javax.*` imports.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Hand-writing DTOs on both client and server | Field names drift out of sync, causing silent runtime deserialization errors | Generate DTOs automatically from OpenAPI or Protobuf contracts. |
| Reusing deleted Protobuf field numbers (tags) | Deserializer maps incoming binary data to wrong fields, corrupting data silently | Use `reserved <tag-number>;` to permanently retire removed tags. |
| Making breaking changes without versioning | Upgrading producer breaks existing consumers that have not yet updated | Maintain backward compatibility or publish new major versions (`/api/v2/`). |
| Missing `build-helper-maven-plugin` | IDE fails to recognize `target/generated-sources` as compile source roots | Add `build-helper-maven-plugin` with `add-source` goal. |
| Omitting `operationId` in OpenAPI paths | Generator produces arbitrary generic method names like `ordersOrderIdGet()` | Define explicit, camelCase `operationId` in the YAML specification. |
| Committing generated Java files to Git | Produces merge conflicts and duplicates source files across branches | Add `target/generated-sources` to `.gitignore`. |

---

## 8. Performance & Security Notes

- **Binary vs Text Serialization**: Protocol Buffers serialize up to 80% smaller and 5-10x faster than OpenAPI JSON, reducing CPU usage and network egress.
- **Contract Schema Validation**: Validate incoming payloads against the OpenAPI schema using request validation filters (`OpenApiValidationFilter`) to reject malformed requests at the boundary.
- **Contract Deprecation Lifecycle**: Maintain at least one previous minor version compatibility window (e.g., $N-1$) so consumers can upgrade independently.

---

## 9. Comparison / When to Use What

| Feature | OpenAPI 3.1 (REST) | Protocol Buffers (gRPC) | GraphQL Schema |
|---|---|---|---|
| **Format** | Human-readable JSON / YAML | Compact Binary Wire Format | Human-readable Schema (SDL) |
| **Transport** | HTTP/1.1 or HTTP/2 | HTTP/2 Multiplexing (Mandatory) | HTTP POST |
| **Performance** | Moderate (JSON serialization) | Maximum (Sub-millisecond) | Moderate |
| **Browser Support**| Universal | Requires gRPC-Web proxy | Universal |
| **Best For** | Public APIs, mobile/web clients | Internal microservice mesh | Rich frontends with flexible queries |

---

## 10. Interview Questions

<details>
<summary>1. What is Contract-First development and why is it superior to Code-First in microservices?</summary>
In Contract-First development, the machine-readable API specification (OpenAPI or Protobuf) is written and agreed upon *before* any implementation code is authored. In Code-First, code is written first and documentation is extracted afterwards. Contract-First is superior in microservices because it allows client and server teams to develop in parallel, enforces architectural governance, eliminates runtime deserialization mismatches through compile-time code generation, and enables automated breaking-change detection in CI/CD.
</details>

<details>
<summary>2. How does Protobuf maintain backward compatibility when fields are removed?</summary>
When a field is removed from a `.proto` file, its tag number must never be reused for a different field in the future, as older clients transmitting that tag would cause data corruption. To prevent tag reuse, Protobuf provides the `reserved` keyword (e.g., `reserved 4, 8;`), instructing the compiler to reject any future definition attempting to use those numbers.
</details>

<details>
<summary>3. What is the role of Consumer-Driven Contract Testing (e.g. Pact)?</summary>
Consumer-Driven Contract Testing allows consumer services to publish a contract defining exactly what endpoints and fields they consume from a producer. The producer runs automated tests against this contract in its CI pipeline. If the producer alters a field that breaks a consumer's expectations, the producer's build fails before code reaches production.
</details>

<details>
<summary>4. What are the essential options to configure in openapi-generator-maven-plugin for Spring Boot 3?</summary>
- `<generatorName>spring</generatorName>`
- `<useSpringBoot3>true</useSpringBoot3>` (Ensures `jakarta.*` package generation)
- `<interfaceOnly>true</interfaceOnly>` (Generates clean interfaces without boilerplate controller stubs)
- `<useTags>true</useTags>` (Organizes operations into distinct interfaces based on OpenAPI tags)
</details>

<details>
<summary>5. How should contracts be shared across different microservice repositories?</summary>
The enterprise best practice is to maintain contracts in a central repository or inside the producer's repository, compile the contract into a lightweight client JAR containing generated DTOs and interfaces, and publish the JAR to an internal Maven/Artifact registry (Nexus/Artifactory). Consumers declare a standard Maven dependency on the client JAR.
</details>

<details>
<summary>6. What is the difference between breaking and non-breaking contract changes?</summary>
- *Non-Breaking Changes*: Adding an optional field, adding a new endpoint, adding a new optional query parameter.
- *Breaking Changes*: Renaming an existing field, changing a field's data type, deleting an endpoint, adding a *required* field to request bodies, or removing enum values.
</details>

<details>
<summary>7. How does operationId in an OpenAPI contract impact the generated Java code?</summary>
The `operationId` directly dictates the method name in the generated Java interface. For example, `operationId: findOrderDetails` produces `ResponseEntity<OrderDto> findOrderDetails(...)`.
</details>

<details>
<summary>8. How do you automate breaking-change detection in CI pipelines?</summary>
Using schema diff tools like `oasdiff` (for OpenAPI) or `buf breaking` (for Protobuf). The CI pipeline compares the pull request contract against the published production contract; if breaking changes are detected without a major version bump, the pull request is blocked automatically.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1 (Easy): Create a minimal OpenAPI 3.1 YAML file</summary>
Write a specification defining GET `/users/{id}` returning a `UserDto(id, email)`.
</details>

<details>
<summary>Exercise 2 (Easy): Configure OpenAPI Generator for interfaceOnly</summary>
Configure `openapi-generator-maven-plugin` with `interfaceOnly=true` and `useSpringBoot3=true`.
</details>

<details>
<summary>Exercise 3 (Medium): Implement generated interface in a @RestController</summary>
Implement the generated `UsersApi` interface in a Spring `@RestController` returning a mock user record.
</details>

<details>
<summary>Exercise 4 (Medium): Define a Protobuf service with reserved tags</summary>
Write a `.proto` file defining a `PaymentService` with method `ProcessPayment`, reserving tags 3 and 5.
</details>

<details>
<summary>Exercise 5 (Hard): Multi-module Maven setup with dedicated API contract module</summary>
Create a Maven multi-module project where `order-contract` compiles the OpenAPI spec into a JAR consumed by both `order-producer` and `order-consumer`.
</details>

<details>
<summary>Exercise 6 (Hard): Automated breaking change detection script</summary>
Write a shell or Python CI verification script using `oasdiff` that compares `current-api.yaml` against `previous-api.yaml` and fails if breaking changes exist.
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------+
|                    Contract-First Communication Summary                         |
+---------------------------------------------------------------------------------+
| Invariant:          Contract (.yaml or .proto) is the single source of truth    |
| Generator:          openapi-generator-maven-plugin with useSpringBoot3=true     |
| Server Pattern:     @RestController implements GeneratedApiInterface            |
| Consumer Pattern:   Inject generated Client SDK / gRPC BlockingStub             |
| Protobuf Safety:    Use 'reserved' keyword to permanently retire deleted tags   |
| Breaking Changes:   Never rename fields; add new fields as optional             |
| CI Governance:      Enforce breaking-change checks via oasdiff or buf           |
| Artifact Delivery:  Publish client DTO JARs to internal Maven / Nexus registry  |
+---------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [OpenAPI Specification 3.1.0](https://spec.openapis.org/oas/v3.1.0)
- [Protocol Buffers Language Guide](https://protobuf.dev/programming-guides/proto3/)
- [OpenAPI Generator Maven Plugin Documentation](https://openapi-generator.tech/docs/plugins/openapi-generator-maven-plugin/)
