# Compile-Time Type-Safe Bean Mapping with MapStruct

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x |
| **Java Version** | 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | `org.mapstruct:mapstruct:1.6.3`, `org.projectlombok:lombok-mapstruct-binding:0.2.0` |

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
In enterprise applications, architecture mandates strict boundary separation between internal domain entities (JPA entities with lazy relationships, audit timestamps, and table column mappings) and external Data Transfer Objects (DTOs exposed via REST or GraphQL). Historically, developers manually wrote hundreds of lines of tedious, error-prone getter-and-setter mapping code (`dto.setName(entity.getName())`). When teams attempted to automate this with reflection-based libraries (ModelMapper, Dozer), they suffered heavy runtime performance degradation, memory overhead, and silent failures when property names or types diverged.

### The Evolution
MapStruct is an annotation processor plug-in that generates type-safe, high-performance mapping code at **compile time**. It reads plain Java interface declarations annotated with `@Mapper`, verifies field types and names during `javac` compilation, and emits standard Java code that invokes direct getter and setter methods without any runtime reflection. If a field name or type cannot be safely mapped, the Maven/Gradle build fails immediately with a descriptive compiler error.

### Relevance in Modern Engineering
With Java 21 records and Spring Boot 3, MapStruct integrates seamlessly with Spring's dependency injection (`componentModel = "spring"`). It supports record-to-entity conversions, nested property navigation, collection transformations, and immutable constructors with zero runtime overhead.

---

## 2. Core Concepts

- **Compile-Time Annotation Processor**: MapStruct operates inside the Java compiler (`javac`) during the annotation processing phase, producing `*Impl.java` source files in `target/generated-sources/annotations`.
- **`@Mapper(componentModel = "spring")`**: Generates a standard Spring `@Component` bean that can be injected via constructor injection into any service.
- **`@Mapping`**: Configures specific field-level mappings, custom target expressions, date/number formatting, or default fallback values.
- **Java Record Mapping**: MapStruct inspects canonical record components and generates direct constructor invocations for immutable record DTOs.
- **Null Value Mapping Strategy**: Governs how null source objects or fields are handled (`NullValueMappingStrategy.RETURN_DEFAULT`, `NullValuePropertyMappingStrategy.IGNORE`).
- **Custom Decorator / `@AfterMapping`**: Allows programmatic post-processing to inject complex domain logic after automated field copying.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------+
|                        MapStruct Compilation Pipeline                             |
|                                                                                   |
| 1. Developer Writes Interface                                                     |
|    @Mapper(componentModel = "spring")                                             |
|    public interface UserMapper {                                                  |
|        UserDto toDto(UserEntity entity);                                          |
|    }                                                                              |
|                   |                                                               |
|                   v (javac -processor org.mapstruct.ap.MappingProcessor)         |
| 2. MapStruct Annotation Processor Runs                                            |
|    +-----------------------------+                                                |
|    | Inspects Method Signatures  | ---> Compares UserEntity & UserDto properties  |
|    +-----------------------------+                                                |
|                   | Verifies types, constructors, and getters/setters             |
|                   v                                                               |
| 3. Emits Clean Java Source Code (target/generated-sources/annotations)            |
|    @Component                                                                     |
|    public class UserMapperImpl implements UserMapper {                            |
|        @Override                                                                  |
|        public UserDto toDto(UserEntity entity) {                                  |
|            if (entity == null) return null;                                       |
|            return new UserDto(entity.getId(), entity.getName(), ...);             |
|        }                                                                          |
|    }                                                                              |
|                   |                                                               |
|                   v                                                               |
| 4. Runtime: Zero Reflection Overhead (Pure Direct Java Method Invocations)        |
+-----------------------------------------------------------------------------------+
```

During build time, the Java compiler passes abstract syntax trees of classes annotated with `@Mapper` to the MapStruct annotation processor. MapStruct matches properties between source and target classes based on name conventions and type compatibility. It synthesizes a new Java source file (`*Impl.java`) containing direct, explicit assignments, and annotates the class with `@Component`. At runtime, Spring Boot instantiates the generated bean via standard dependency injection with zero reflection overhead.

---

## 4. Syntax & Usage

### Maven Configuration (`pom.xml`)
```xml
<properties>
    <org.mapstruct.version>1.6.3</org.mapstruct.version>
    <lombok.version>1.18.36</lombok.version>
</properties>

<dependencies>
    <dependency>
        <groupId>org.mapstruct</groupId>
        <artifactId>mapstruct</artifactId>
        <version>${org.mapstruct.version}</version>
    </dependency>
</dependencies>

<build>
    <plugins>
        <plugin>
            <groupId>org.apache.maven.plugins</groupId>
            <artifactId>maven-compiler-plugin</artifactId>
            <version>3.13.0</version>
            <configuration>
                <annotationProcessorPaths>
                    <!-- Lombok must precede MapStruct if using Lombok annotations -->
                    <path>
                        <groupId>org.projectlombok</groupId>
                        <artifactId>lombok</artifactId>
                        <version>${lombok.version}</version>
                    </path>
                    <path>
                        <groupId>org.projectlombok</groupId>
                        <artifactId>lombok-mapstruct-binding</artifactId>
                        <version>0.2.0</version>
                    </path>
                    <path>
                        <groupId>org.mapstruct</groupId>
                        <artifactId>mapstruct-processor</artifactId>
                        <version>${org.mapstruct.version}</version>
                    </path>
                </annotationProcessorPaths>
            </configuration>
        </plugin>
    </plugins>
</build>
```

---

## 5. Practical Examples

### Example 1: Basic Entity to Java Record DTO Mapper
```java
package com.example.platform.mapper;

import org.mapstruct.Mapper;
import org.mapstruct.Mapping;
import org.mapstruct.MappingConstants;

import java.time.Instant;
import java.util.UUID;

// Domain Entity
class OrderEntity {
    private UUID id;
    private String customerReference;
    private long totalCents;
    private Instant createdAt;

    public UUID getId() { return id; }
    public void setId(UUID id) { this.id = id; }
    public String getCustomerReference() { return customerReference; }
    public void setCustomerReference(String ref) { this.customerReference = ref; }
    public long getTotalCents() { return totalCents; }
    public void setTotalCents(long totalCents) { this.totalCents = totalCents; }
    public Instant getCreatedAt() { return createdAt; }
    public void setCreatedAt(Instant createdAt) { this.createdAt = createdAt; }
}

// Immutable Record DTO
record OrderResponseDto(
        UUID orderId,
        String customerRef,
        double totalAmountDollars,
        Instant createdTimestamp
) {}

@Mapper(componentModel = MappingConstants.ComponentModel.SPRING)
public interface OrderMapper {

    @Mapping(target = "orderId", source = "id")
    @Mapping(target = "customerRef", source = "customerReference")
    @Mapping(target = "totalAmountDollars", expression = "java(entity.getTotalCents() / 100.0)")
    @Mapping(target = "createdTimestamp", source = "createdAt")
    OrderResponseDto toDto(OrderEntity entity);
}
```

### Example 2: Nested Object and Collection Mapping
```java
package com.example.platform.mapper;

import org.mapstruct.Mapper;
import org.mapstruct.Mapping;
import org.mapstruct.MappingConstants;

import java.util.List;

class CustomerEntity {
    private String name;
    private AddressEntity address;
    private List<OrderEntity> orders;
    // Getters and setters omitted for brevity
    public String getName() { return name; }
    public AddressEntity getAddress() { return address; }
    public List<OrderEntity> getOrders() { return orders; }
}

class AddressEntity {
    private String street;
    private String city;
    public String getStreet() { return street; }
    public String getCity() { return city; }
}

record CustomerDto(String name, String streetAddress, String cityName, List<OrderResponseDto> orders) {}

@Mapper(componentModel = MappingConstants.ComponentModel.SPRING, uses = {OrderMapper.class})
public interface CustomerMapper {

    // Flatten nested address properties into flat DTO
    @Mapping(target = "streetAddress", source = "address.street")
    @Mapping(target = "cityName", source = "address.city")
    CustomerDto toDto(CustomerEntity customer);

    List<CustomerDto> toDtoList(List<CustomerEntity> customers);
}
```

### Example 3: Partial Entity Update with `@MappingTarget`
```java
package com.example.platform.mapper;

import org.mapstruct.*;

record UpdateProfileRequest(String displayName, String bio, String phoneNumber) {}

class UserProfileEntity {
    private String displayName;
    private String bio;
    private String phoneNumber;
    private String email; // Should remain untouched

    public String getDisplayName() { return displayName; }
    public void setDisplayName(String d) { this.displayName = d; }
    public String getBio() { return bio; }
    public void setBio(String b) { this.bio = b; }
    public String getPhoneNumber() { return phoneNumber; }
    public void setPhoneNumber(String p) { this.phoneNumber = p; }
    public String getEmail() { return email; }
    public void setEmail(String e) { this.email = e; }
}

@Mapper(componentModel = MappingConstants.ComponentModel.SPRING)
public interface UserProfileMapper {

    // Only update non-null fields from the request into the existing entity
    @BeanMapping(nullValuePropertyMappingStrategy = NullValuePropertyMappingStrategy.IGNORE)
    @Mapping(target = "email", ignore = true)
    void updateEntityFromDto(UpdateProfileRequest request, @MappingTarget UserProfileEntity targetEntity);
}
```

### Example 4: Unmapped Target Properties Causing Silent Failures (Anti-Pattern Fix)
```java
// ANTI-PATTERN: Default configuration ignoring unmapped target properties.
// When a developer adds a new security-critical field to the DTO, it remains null silently!
// @Mapper(componentModel = "spring") // DANGEROUS!

// FIX: Enforce unmappedTargetPolicy = ReportingPolicy.ERROR so compilation fails if fields are omitted
package com.example.platform.mapper;

import org.mapstruct.Mapper;
import org.mapstruct.Mapping;
import org.mapstruct.MappingConstants;
import org.mapstruct.ReportingPolicy;

@Mapper(
        componentModel = MappingConstants.ComponentModel.SPRING,
        unmappedTargetPolicy = ReportingPolicy.ERROR // Build FAILS if any target field is missed
)
public interface StrictAccountMapper {

    record AccountEntity(String id, String iban, String owner, String currency) {}
    record AccountDto(String id, String iban, String owner, String currency) {}

    AccountDto toDto(AccountEntity entity);
}
```

### Example 5: High-Performance Custom Post-Processing with `@AfterMapping`
```java
package com.example.platform.mapper;

import org.mapstruct.AfterMapping;
import org.mapstruct.Mapper;
import org.mapstruct.MappingConstants;
import org.mapstruct.MappingTarget;

import java.util.Base64;

class SecretEntity {
    private String id;
    private byte[] rawKey;
    public String getId() { return id; }
    public byte[] getRawKey() { return rawKey; }
}

class SecretDto {
    private String id;
    private String encodedKey;
    public String getId() { return id; }
    public void setId(String id) { this.id = id; }
    public String getEncodedKey() { return encodedKey; }
    public void setEncodedKey(String k) { this.encodedKey = k; }
}

@Mapper(componentModel = MappingConstants.ComponentModel.SPRING)
public abstract class SecretMapper {

    public abstract SecretDto toDto(SecretEntity entity);

    @AfterMapping
    protected void applyCustomEncoding(SecretEntity entity, @MappingTarget SecretDto target) {
        if (entity.getRawKey() != null) {
            target.setEncodedKey(Base64.getEncoder().encodeToString(entity.getRawKey()));
        }
    }
}
```

---

## 6. Best Practices

1. **Enforce `unmappedTargetPolicy = ReportingPolicy.ERROR`**: Make unmapped target fields a compiler error to prevent silent data truncation when models evolve.
2. **Use `componentModel = "spring"`**: Always generate Spring components for clean dependency injection and test mocking.
3. **Keep Annotation Processor Order Strict**: In `pom.xml`, Lombok must precede `lombok-mapstruct-binding`, which must precede `mapstruct-processor`.
4. **Use Java Records for DTOs**: MapStruct maps directly to Java record canonical constructors, guaranteeing immutability.
5. **Use `@Named` Qualified Mappings**: Use `@Named` custom qualifier methods when transforming specialized scalar types (e.g., String to PhoneNumber).

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Incorrect annotation processor ordering in `pom.xml` | MapStruct runs before Lombok; getters/setters do not exist yet, causing compilation errors | Place Lombok first, then `lombok-mapstruct-binding`, then `mapstruct-processor`. |
| Missing `componentModel = "spring"` | MapStruct defaults to `Mappers.getMapper()`, preventing Spring `@Autowired` injection | Add `componentModel = "spring"` (or `MappingConstants.ComponentModel.SPRING`). |
| Forgetting `@Mapping(target = "...", ignore = true)` on unmapped fields | Triggers compiler warning or error under strict reporting policies | Explicitly ignore unmapped fields or map them to default expressions. |
| Modifying generated code in `target/generated-sources/` | Changes are erased on the next `mvn clean compile` | Implement custom mapping logic in `@AfterMapping` or abstract mapper classes. |
| Incompatible types without explicit converter method | MapStruct fails compilation: "Can't map property to type..." | Provide an explicit mapping method or expression in `@Mapping`. |
| Triggering JPA lazy-loading during mapping | Mapping nested entity properties outside a transaction triggers `LazyInitializationException` | Fetch required relationships via `JOIN FETCH` or DTO projection queries. |

---

## 8. Performance & Security Notes

- **Zero Reflection Overhead**: Because MapStruct generates direct Java bytecode (`entity.getName()`), execution speed is equivalent to hand-written code and orders of magnitude faster than reflection-based mappers.
- **Garbage Collection Efficiency**: Avoid creating unnecessary intermediate wrapper objects during transformation.
- **Information Leakage Prevention**: Never map internal entity audit fields (passwords, salt hashes, internal DB row IDs) to public DTOs. Enforce strict DTO boundaries.

---

## 9. Comparison / When to Use What

| Feature | MapStruct | ModelMapper | Manual Getters/Setters |
|---|---|---|---|
| **Mechanism** | Compile-time code generation | Runtime reflection | Hand-written code |
| **Performance** | Maximum (Direct method calls) | Slow (Reflection overhead) | Maximum |
| **Type Safety** | Verified at compile time | Fails at runtime | Verified at compile time |
| **Boilerplate** | Minimal (Interface declarations) | Minimal | Massive (Hundreds of lines) |
| **Spring Integration** | Native `@Component` beans | Requires manual bean config | None |

---

## 10. Interview Questions

<details>
<summary>1. How does MapStruct achieve superior performance compared to reflection-based mappers like ModelMapper?</summary>
MapStruct operates strictly as a compile-time annotation processor. It inspects source and target class definitions during `javac` compilation and generates standard Java source code with direct getter, setter, and constructor calls. Because it requires zero reflection or runtime introspection, its performance is identical to manually written code.
</details>

<details>
<summary>2. Why is the annotation processor order in pom.xml critical when using Lombok with MapStruct?</summary>
Lombok generates getters, setters, and constructors at compile time. If MapStruct runs before Lombok, it inspects the Java classes before those methods exist, causing compilation errors. Adding `lombok-mapstruct-binding` and placing Lombok before MapStruct ensures Lombok enhances the AST before MapStruct evaluates it.
</details>

<details>
<summary>3. What is the purpose of unmappedTargetPolicy = ReportingPolicy.ERROR?</summary>
By default, MapStruct issues warnings when a target property is not mapped. Setting the policy to `ERROR` causes the build to fail if any target property is left unmapped. This ensures developers immediately notice when new fields are added to DTOs or entities.
</details>

<details>
<summary>4. How does MapStruct handle mapping to immutable Java records?</summary>
MapStruct inspects the canonical constructor of the record. Instead of calling setters, it matches properties from the source object and passes them directly as arguments into the record constructor.
</details>

<details>
<summary>5. What is the role of @MappingTarget?</summary>
`@MappingTarget` is used for in-place updates. Instead of instantiating a new object, MapStruct copies properties from the source object directly into an existing target instance passed as an argument.
</details>

<details>
<summary>6. How can you implement custom transformation logic that MapStruct cannot deduce automatically?</summary>
By either converting the `@Mapper` interface into an `abstract class` and implementing custom helper methods, using default methods on the interface, using the `expression = "java(...)"` attribute, or annotating methods with `@BeforeMapping` and `@AfterMapping`.
</details>

<details>
<summary>7. How does MapStruct integrate with Spring's dependency injection?</summary>
By specifying `@Mapper(componentModel = "spring")`. The generated implementation class is annotated with Spring's `@Component`, allowing it to be injected into services via `@Autowired` or constructor injection.
</details>

<details>
<summary>8. How do you handle mapping collections of objects in MapStruct?</summary>
You declare a mapping method for individual elements (e.g., `UserDto toDto(User user)`), and a collection method (e.g., `List<UserDto> toDtoList(List<User> users)`). MapStruct automatically uses the element mapping method inside a generated loop to map the entire collection.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1 (Easy): Create a basic MapStruct mapper interface</summary>
Define a mapper interface converting an `EmployeeEntity` to `EmployeeDto` with matching field names using `componentModel = "spring"`.
</details>

<details>
<summary>Exercise 2 (Easy): Map fields with differing names</summary>
Map a property `src.userFullName` to `target.name` using `@Mapping(target = "name", source = "userFullName")`.
</details>

<details>
<summary>Exercise 3 (Medium): Implement an in-place update mapper with null-ignore strategy</summary>
Create a method updating a `ProductEntity` from a `ProductUpdateRequest` where null fields in the request do not overwrite existing entity values.
</details>

<details>
<summary>Exercise 4 (Medium): Map nested child entities</summary>
Configure a mapper that flattens `order.customer.billingAddress.city` into `OrderDto.billingCity`.
</details>

<details>
<summary>Exercise 5 (Hard): Strict compile-time validation with custom enum mapping</summary>
Configure a mapper with `unmappedTargetPolicy = ReportingPolicy.ERROR` and a custom `@ValueMapping` translating legacy string status codes to domain enums.
</details>

<details>
<summary>Exercise 6 (Hard): Integration test testing injected mapper bean</summary>
Write a `@SpringBootTest` that injects the generated MapStruct mapper bean, executes mapping on complex nested objects, and asserts all transformed fields.
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------+
|                           MapStruct Quick Reference                             |
+---------------------------------------------------------------------------------+
| Compiler Plugin:    maven-compiler-plugin with mapstruct-processor              |
| Processor Order:    Lombok -> lombok-mapstruct-binding -> mapstruct-processor   |
| Core Annotation:    @Mapper(componentModel = "spring")                          |
| Property Override:  @Mapping(target = "dtoField", source = "entityField")       |
| Java Expression:    @Mapping(target = "val", expression = "java(e.calc())")     |
| Partial Update:     void update(Dto dto, @MappingTarget Entity entity)          |
| Ignore Nulls:       @BeanMapping(nullValuePropertyMappingStrategy = IGNORE)     |
| Strict Policy:      unmappedTargetPolicy = ReportingPolicy.ERROR                |
| Performance:        Identical to hand-written getters/setters (zero reflection) |
+---------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Official MapStruct Reference Guide](https://mapstruct.org/documentation/stable/reference/html/)
- [MapStruct Examples GitHub Repository](https://github.com/mapstruct/mapstruct-examples)
- [Lombok and MapStruct Integration Guide](https://mapstruct.org/faq/#can-i-use-mapstruct-together-with-project-lombok)
