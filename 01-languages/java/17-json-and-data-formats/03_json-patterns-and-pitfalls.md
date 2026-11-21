# JSON Patterns, Anti-Patterns, and Pitfalls

> Production patterns for resilient JSON APIs: JSON Merge Patch, schema evolution, polymorphic deserialization security, and cyclic reference mitigation.

| Metadata | Details |
|---|---|
| **Level** | Core / Advanced |
| **Prerequisites** | Jackson Databind, Java Records, RESTful API Design |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **JDK Baseline** | Java 17 / 21 / 25 LTS |

---

## Table of Contents
1. [1. What & Why](#1-what--why)
2. [2. Core Concepts](#2-core-concepts)
3. [3. How It Works Internally: Schema Evolution & Deserialization Vulnerabilities](#3-how-it-works-internally-schema-evolution--deserialization-vulnerabilities)
4. [4. Syntax & Usage](#4-syntax--usage)
5. [5. Practical Examples](#5-practical-examples)
6. [6. Best Practices](#6-best-practices)
7. [7. Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [8. Performance & Security Notes](#8-performance--security-notes)
9. [9. Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [10. Interview Questions](#10-interview-questions)
11. [11. Practice Exercises](#11-practice-exercises)
12. [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [13. Further Reading](#13-further-reading)

---

## 1. What & Why
In distributed microservices, JSON is not merely an in-memory data format; it is the public contractual boundary between decoupled teams, external consumers, and distributed datastores. In a large enterprise, services are deployed independently: a producer might add a new field, a consumer might omit an optional value, or an attacker might attempt to exploit polymorphic deserialization vulnerabilities.

Designing resilient JSON models requires mastering patterns such as **Postel's Law (Tolerant Reader Pattern)**, **JSON Merge Patch (RFC 7396)** for partial updates, **Circular Dependency Breakers**, and **Polymorphic Security Boundaries**. Misunderstanding these patterns leads to brittle client breakage, data corruption during partial updates, or remote code execution.

---

## 2. Core Concepts
- **Tolerant Reader Pattern**: Be liberal in what you accept and conservative in what you send. Consumers should ignore unknown properties rather than crashing.
- **Partial Updates (JSON Merge Patch - RFC 7396)**: Differentiating between an omitted field (leave existing value unchanged) and an explicit `null` value (remove/reset existing value).
- **Cyclic Reference Handling**: Techniques for serializing bi-directional entity graphs (`Parent` $\leftrightarrow$ `Child`) without triggering infinite loops.
- **Polymorphic Whitelisting**: Securing dynamic subclass deserialization using explicit type names (`type = "CREDIT_CARD"`) rather than arbitrary Java class names.
- **Schema Evolution Rules**: Guidelines ensuring new JSON field additions, renames, and type widenings remain backward and forward compatible.
- **Envelope vs Flat Payloads**: Structuring API responses with metadata headers (`data`, `error`, `pagination`) vs flat domain objects.

---

## 3. How It Works Internally: Schema Evolution & Deserialization Vulnerabilities

When distributed systems evolve schemas, serialization contracts experience compatibility friction:

```
Producer (v2) Emits: {"id": 101, "sku": "A1", "newTaxCode": "US-WA"}
                                |
                                v
Consumer (v1) Expects: {"id": 101, "sku": "A1"}
                                |
                 [FAIL_ON_UNKNOWN_PROPERTIES?]
                                |
          +---------------------+---------------------+
          |                                           |
          v (true - Anti-pattern)                     v (false - Resilient)
UnrecognizedPropertyException!                 Ignored gracefully
System Outage / Cascade Failure                Process continues safely
```

### The Inherent Vulnerability of Polymorphism:
When a JSON library deserializes an object dynamically based on a class name provided in the payload (e.g., `"@class": "com.sun.rowset.JdbcRowSetImpl"`), the library uses reflection to instantiate that class. If the class has side effects in its constructor, setters, or getters (such as initiating an LDAP or JNDI lookup), attackers can achieve Remote Code Execution (RCE). Mitigating this requires strictly separating domain polymorphism from arbitrary runtime class loading.

---

## 4. Syntax & Usage

### 4.1 Implementing JSON Merge Patch (RFC 7396) with Optional / JsonNullable
```java
package com.example.json.patterns;

import com.fasterxml.jackson.annotation.JsonInclude;
import com.fasterxml.jackson.databind.ObjectMapper;
import org.openapitools.jackson.nullable.JsonNullable;
import org.openapitools.jackson.nullable.JsonNullableModule;

public final class MergePatchConfig {
    public static ObjectMapper createMergePatchMapper() {
        ObjectMapper mapper = new ObjectMapper();
        mapper.registerModule(new JsonNullableModule());
        mapper.setSerializationInclusion(JsonInclude.Include.NON_NULL);
        return mapper;
    }
}

// DTO distinguishing:
// 1. Omitted field -> JsonNullable.undefined() (Do not change existing value)
// 2. Explicit null -> JsonNullable.of(null) (Clear existing value)
// 3. Provided value -> JsonNullable.of(val) (Update existing value)
public record UpdateUserRequest(
    JsonNullable<String> displayName,
    JsonNullable<String> phoneNumber,
    JsonNullable<String> bio
) {}
```

### 4.2 Handling Cyclic References in Domain Models
```java
package com.example.json.patterns;

import com.fasterxml.jackson.annotation.JsonBackReference;
import com.fasterxml.jackson.annotation.JsonManagedReference;
import java.util.ArrayList;
import java.util.List;

public class Department {
    private String name;

    @JsonManagedReference // Serialized normally as child list
    private List<Employee> employees = new ArrayList<>();

    public Department(String name) { this.name = name; }
    public String getName() { return name; }
    public List<Employee> getEmployees() { return employees; }
}

public class Employee {
    private String name;

    @JsonBackReference // Omitted from serialization to break the cycle!
    private Department department;

    public Employee(String name, Department department) {
        this.name = name;
        this.department = department;
    }
    public String getName() { return name; }
    public Department getDepartment() { return department; }
}
```

---

## 5. Practical Examples

### Example 1: Applying Partial Update in Business Service
```java
package com.example.json.patterns;

public final class UserService {
    public static void applyPatch(User existingUser, UpdateUserRequest patch) {
        // If field was present in JSON:
        if (patch.displayName().isPresent()) {
            existingUser.setDisplayName(patch.displayName().get());
        }
        if (patch.phoneNumber().isPresent()) {
            existingUser.setPhoneNumber(patch.phoneNumber().get()); // Can be null to clear
        }
        if (patch.bio().isPresent()) {
            existingUser.setBio(patch.bio().get());
        }
    }

    public static class User {
        private String displayName;
        private String phoneNumber;
        private String bio;

        public void setDisplayName(String d) { this.displayName = d; }
        public void setPhoneNumber(String p) { this.phoneNumber = p; }
        public void setBio(String b) { this.bio = b; }
    }
}
```

### Example 2: Real-World Safe Polymorphic Audit Event Model
```java
package com.example.json.patterns;

import com.fasterxml.jackson.annotation.*;
import java.time.Instant;

@JsonTypeInfo(
    use = JsonTypeInfo.Id.NAME,
    include = JsonTypeInfo.As.PROPERTY,
    property = "eventType"
)
@JsonSubTypes({
    @JsonSubTypes.Type(value = LoginEvent.class, name = "USER_LOGIN"),
    @JsonSubTypes.Type(value = PaymentEvent.class, name = "PAYMENT_PROCESSED"),
    @JsonSubTypes.Type(value = OrderShippedEvent.class, name = "ORDER_SHIPPED")
})
public sealed interface AuditEvent permits LoginEvent, PaymentEvent, OrderShippedEvent {
    Instant timestamp();
    String actor();
}

public record LoginEvent(Instant timestamp, String actor, String ipAddress) implements AuditEvent {}
public record PaymentEvent(Instant timestamp, String actor, double amount, String txId) implements AuditEvent {}
public record OrderShippedEvent(Instant timestamp, String actor, String trackingNumber) implements AuditEvent {}
```

### Example 3: Edge Case - Standardized API Envelope Structure
```java
package com.example.json.patterns;

import com.fasterxml.jackson.annotation.JsonInclude;
import java.time.Instant;

@JsonInclude(JsonInclude.Include.NON_NULL)
public record ApiResponse<T>(
    boolean success,
    T data,
    ApiError error,
    Instant timestamp
) {
    public static <T> ApiResponse<T> ok(T data) {
        return new ApiResponse<>(true, data, null, Instant.now());
    }

    public static <T> ApiResponse<T> fail(String code, String message) {
        return new ApiResponse<>(false, null, new ApiError(code, message), Instant.now());
    }
}

public record ApiError(String errorCode, String errorMessage) {}
```

---

## 6. Best Practices
1. **Always Disable `FAIL_ON_UNKNOWN_PROPERTIES` in Consumers**: Adhere to the Tolerant Reader Pattern so consumers do not crash when providers add new non-breaking fields.
2. **Use JsonNullable for PATCH Endpoints**: Do not rely on plain `null` to distinguish between "omitted" and "set to null"; use `JsonNullable` or RFC 6902 JSON Patch.
3. **Use Id.NAME for Polymorphic Types**: Always configure `@JsonTypeInfo(use = JsonTypeInfo.Id.NAME)`; never use `Id.CLASS` which exposes internal Java implementation details and creates RCE vulnerabilities.
4. **Never Serialize Raw Hibernate Managed Entities**: Always map entities to explicit DTOs or Records before serialization to prevent lazy-loading recursion and transaction leaks.
5. **Enforce RFC 8785 Canonical Sorting for Cryptographic Signatures**: When signing or hashing JSON payloads, apply RFC 8785 canonical JSON formatting to guarantee deterministic byte outputs.
6. **Explicit Field Naming with `@JsonProperty`**: Decouple public API contract names from internal Java variable refactorings.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Correction |
|---|---|---|
| Crashing on new backend fields | `FAIL_ON_UNKNOWN_PROPERTIES` is enabled by default | Disable `FAIL_ON_UNKNOWN_PROPERTIES` in ObjectMapper |
| Inability to clear fields via PATCH | Plain POJO cannot differentiate between omitted field and `null` | Use `JsonNullable<T>` or JSON Merge Patch |
| Using `@JsonTypeInfo(use = Id.CLASS)` | Enables arbitrary class instantiation leading to RCE | Use `Id.NAME` with explicit `@JsonSubTypes` whitelist |
| Returning raw JPA entities from `@RestController` | Triggers `LazyInitializationException` and cyclic serialization | Map entities to Java 21 Records / DTOs |
| Modifying date formats without versioning | Breaks legacy client parsers expecting ISO-8601 strings | Maintain strict ISO-8601 formatting or version endpoints |
| Cryptographic hashing of raw JSON strings | Key order variance produces different hashes for identical data | Use RFC 8785 canonical JSON sorting |
| Unbounded payload sizes | Callers can exhaust heap memory with 100 MB requests | Enforce max payload size at API gateway/server |
| Using floating-point for currency | IEEE-754 rounding results in `$19.99` becoming `19.990000000000002` | Use `BigDecimal` with string or decimal constraints |

---

## 8. Performance & Security Notes
- **Polymorphic RCE Prevention**: Remote Code Execution via JSON deserialization remains one of Java's most critical CVE categories. By locking down polymorphism to explicit names (`Id.NAME`), attackers cannot force the JVM to instantiate malicious gadget classes.
- **Payload Size Limits**: Enforce strict HTTP request body size limits (e.g., 2 MB in Spring Boot via `spring.servlet.multipart.max-file-size` and web server buffers) to neutralize Denial of Service (DoS) memory exhaustion attempts.
- **Canonical JSON (RFC 8785)**: Standardizes key sorting, whitespace removal, and number formatting, allowing cryptographic hashes and digital signatures to be validated consistently across heterogeneous languages.

---

## 9. Comparison / When to Use What

| Strategy | Complexity | Precision | Best Use Case |
|---|---|---|---|
| **Full Replacement (PUT)** | Low | Full resource | Overwriting entire entities where complete state is known |
| **JSON Merge Patch (RFC 7396)** | Moderate | Field-level | Flat partial updates (profile settings, toggles) |
| **JSON Patch (RFC 6902)** | High | Operation-level | Atomic operations, array re-ordering, fine-grained edits |
| **DTO Projections** | Low | Strict schema | Standard REST endpoints, decoupled domain models |

---

## 10. Interview Questions

<details>
<summary>1. What is Postel's Law (The Tolerant Reader Pattern), and how is it applied in Jackson?</summary>
Postel's Law states: <i>"Be conservative in what you do, be liberal in what you accept from others."</i> In distributed systems, this is implemented as the Tolerant Reader Pattern: a client or service should only parse the specific fields it requires and ignore any unfamiliar or newly introduced properties. In Jackson, this is applied by disabling <code>DeserializationFeature.FAIL_ON_UNKNOWN_PROPERTIES</code>.
</details>

<details>
<summary>2. Why does standard POJO deserialization fail to implement RFC 7396 JSON Merge Patch properly?</summary>
In a standard Java POJO, an uninitialized or omitted field defaults to <code>null</code>. When receiving <code>{"name": "Alice"}</code>, the <code>bio</code> field is <code>null</code>. When receiving <code>{"name": "Alice", "bio": null}</code>, the <code>bio</code> field is also <code>null</code>. The application cannot distinguish whether the client omitted the field (leave existing bio unchanged) or explicitly passed <code>null</code> (clear existing bio). Solutions like <code>JsonNullable<T></code> solve this by tracking three distinct states: undefined, null, and present.
</details>

<details>
<summary>3. Why is serializing JPA / Hibernate managed entities directly into JSON considered an anti-pattern?</summary>
1. <b>Lazy Loading Outages</b>: Jackson accesses getters via reflection; if an association is <code>FetchType.LAZY</code>, accessing it outside an active transaction throws <code>LazyInitializationException</code>.<br>
2. <b>Cyclic Loops</b>: Bidirectional relationships (Parent $\leftrightarrow$ Child) cause infinite serialization loops leading to <code>StackOverflowError</code>.<br>
3. <b>Leaky Abstractions</b>: Database schema details, sensitive password hashes, or internal foreign keys are unintentionally exposed to public clients. Always project to DTOs or Records.
</details>

<details>
<summary>4. What is the difference between @JsonManagedReference and @JsonBackReference?</summary>
They work as a pair to resolve circular references in parent-child relationships. <code>@JsonManagedReference</code> is placed on the parent's collection or reference, and is serialized normally into JSON. <code>@JsonBackReference</code> is placed on the child's back-pointer to the parent, and is completely omitted during serialization, breaking the infinite cycle while still restoring the parent reference during deserialization.
</details>

<details>
<summary>5. How does RFC 8785 Canonical JSON solve cryptographic signature validation issues?</summary>
Standard JSON allows arbitrary whitespace and does not mandate key order; <code>{"a":1,"b":2}</code> and <code>{"b":2, "a":1}</code> represent the identical logical document but yield completely different SHA-256 hashes. RFC 8785 defines deterministic rules: keys must be sorted lexicographically by Unicode code point, all insignificant whitespace removed, and numbers formatted strictly, ensuring identical byte representations for signature validation across different platforms.
</details>

<details>
<summary>6. How does @JsonView support role-based JSON field filtering?</summary>
<code>@JsonView</code> allows defining view interfaces (e.g., <code>PublicView</code>, <code>InternalAdminView</code>). Fields on the DTO are annotated with the appropriate view class. When serializing in a controller, passing the target view interface to the writer ensures only fields marked for that view (or its super-interfaces) are included in the emitted JSON output.
</details>

<details>
<summary>7. What is the purpose of @JsonIdentityInfo?</summary>
<code>@JsonIdentityInfo</code> handles circular or shared object references by assigning an identity property (e.g., an <code>id</code> attribute). The first time an object is encountered, it is serialized fully along with its ID. Any subsequent references to that same object in the object graph are serialized simply as the ID reference, eliminating infinite loops and avoiding duplicate object payloads.
</details>

<details>
<summary>8. How do you prevent JSON injection when logging user input?</summary>
User inputs containing newlines (<code>
</code>), quotes, or control characters can corrupt structured JSON log streams or spoof log entries (Log Injection / CWE-117). Always serialize log arguments using a structured JSON logging framework (such as Logstash Logback Encoder) that automatically sanitizes and escapes string values before writing log events.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Implement a custom Jackson DeserializationProblemHandler to log unrecognized properties gracefully.</summary>

```java
public final class UnknownPropertyLogger extends DeserializationProblemHandler {
    @Override
    public boolean handleUnknownProperty(DeserializationContext ctxt, JsonParser p, JsonDeserializer<?> deserializer, Object beanOrClass, String propertyName) throws IOException {
        System.err.printf("[AUDIT] Unknown JSON property '%s' encountered on class '%s'. Skipping safely.%n",
                propertyName, beanOrClass.getClass().getSimpleName());
        p.skipChildren(); // Skip the unknown token tree
        return true; // Mark as handled so Jackson does not fail
    }
}
// Register on ObjectMapper:
// mapper.addHandler(new UnknownPropertyLogger());
```
</details>

<details>
<summary>Exercise 2: Write a sealed event hierarchy with JsonTypeInfo and test round-trip serialization.</summary>

```java
@JsonTypeInfo(use = JsonTypeInfo.Id.NAME, property = "action")
@JsonSubTypes({
    @JsonSubTypes.Type(value = OrderPlaced.class, name = "ORDER_PLACED"),
    @JsonSubTypes.Type(value = OrderCancelled.class, name = "ORDER_CANCELLED")
})
public sealed interface OrderCommand permits OrderPlaced, OrderCancelled {}

public record OrderPlaced(long orderId, double amount) implements OrderCommand {}
public record OrderCancelled(long orderId, String reason) implements OrderCommand {}
```
</details>

<details>
<summary>Exercise 3: Write a method calculating the RFC 8785 canonical hash of a JsonNode.</summary>

```java
public static String computeDeterministicHash(JsonNode root, ObjectMapper mapper) throws Exception {
    // Sort keys alphabetically
    String canonicalJson = mapper.writeValueAsString(root);
    MessageDigest md = MessageDigest.getInstance("SHA-256");
    byte[] hash = md.digest(canonicalJson.getBytes(StandardCharsets.UTF_8));
    return HexFormat.of().formatHex(hash);
}
```
</details>

---


### 11.4 In-Depth Guide: JSON Schema Validation in Java Pipelines

Validating incoming JSON payloads against a formal JSON Schema (Draft 7 / Draft 2020-12) prevents malformed data from ever reaching downstream business services:

1. **Schema Definition**:
   Define declarative validation constraints (minimum strings, regex patterns, required fields, numeric ranges) in `order-schema.json`.
2. **High-Speed Schema Validation via networknt**:
   Using `com.networknt.schema.JsonSchemaFactory`:
   ```java
   JsonSchemaFactory factory = JsonSchemaFactory.getInstance(SpecVersion.VersionFlag.V202012);
   JsonSchema schema = factory.getSchema(schemaInputStream);
   Set<ValidationMessage> errors = schema.validate(jsonNode);
   if (!errors.isEmpty()) {
       throw new InvalidPayloadException("Validation failed: " + errors);
   }
   ```
3. **Benefits over Bean Validation**:
   JSON Schema validates raw text before object instantiation, preventing expensive Jackson reflection processing on malicious or garbage input payloads.


## 12. Summary / Cheat Sheet
- **Tolerant Reader**: Always disable `FAIL_ON_UNKNOWN_PROPERTIES` for consumer resilience.
- **Partial Updates**: Use `JsonNullable<T>` to distinguish undefined vs explicit null in PATCH operations.
- **Circular References**: Use `@JsonManagedReference` / `@JsonBackReference` or `@JsonIdentityInfo`.
- **Polymorphism Security**: Always use `@JsonTypeInfo(use = Id.NAME)` with explicit `@JsonSubTypes` whitelists.
- **DTO Boundaries**: Never serialize raw JPA entities; project to Java 21 Records.
- **Canonical JSON**: Use RFC 8785 for cryptographic hashing and deterministic byte comparison.

---

## 13. Further Reading
- IETF RFC 7396: JSON Merge Patch Specification
- IETF RFC 8785: JSON Canonicalization Scheme (JCS)
- Martin Fowler: The Tolerant Reader Pattern
- FasterXML Jackson Security Advisories and Polymorphic Typing Documentation
