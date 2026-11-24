# DTO, Entity, and Domain Model Separation in Spring Boot

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Version** | 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.x |
| **Tested With** | MapStruct 1.6+, Jackson, Spring Data JPA, ArchUnit |

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
10. [Interview Questions & Deep Dives](#10-interview-questions--deep-dives)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### The Problem
In novice or hastily built Spring Boot applications, a single class often serves multiple conflicting masters: it is annotated with `@Entity` for JPA persistence, decorated with `@JsonProperty` for Jackson JSON serialization, marked with `@NotBlank` for web validation, and stuffed with business logic methods. This monolithic entity model causes catastrophic problems:
1. **Security Vulnerabilities**: Attackers exploit mass-assignment over-posting to overwrite fields like `isAdmin` or `balance`.
2. **Runtime Exceptions**: Jackson attempts to serialize uninitialized Hibernate lazy-loading collections outside of transactions, triggering `LazyInitializationException` or infinite recursive JSON loops (`StackOverflowError`).
3. **API Brittleness**: Renaming a database column directly breaks external client API contracts.

### The Evolution
Modern clean enterprise architecture enforces a **Strict 3-Model Separation**:
1. **Data Transfer Objects (DTOs)**: Immutable transport carriers tailored specifically to external clients (REST, GraphQL, Kafka).
2. **Domain Models**: Pure Java objects encapsulating business rules, invariants, and calculations.
3. **Persistence Entities**: Classes mapping directly to relational database schemas and ORM lifecycles.

### Production Relevance
Separating DTOs, Domain Models, and Entities ensures complete API stability, guards against parameter tampering, isolates database migrations from client contracts, and enables high-speed compile-time mapping via **MapStruct**.

---

## 2. Core Concepts

### The Three-Tier Model Representation Lifecycle

```
+-----------------------------------------------------------------------------------------+
|                         THREE-MODEL SEPARATION ARCHITECTURE                             |
+-----------------------------------------------------------------------------------------+
|                                                                                         |
|  [ HTTP Request JSON ]                                                                  |
|           |                                                                             |
|           v                                                                             |
|  +-----------------------------------------------------------------------------------+  |
|  | 1. DATA TRANSFER OBJECT (DTO)                                                     |  |
|  |    - Java 21 Record (Immutable, flat, serializable)                               |  |
|  |    - Bean Validation annotations (@Valid, @NotNull, @Email)                       |  |
|  |    - Contract defined strictly by API specification (OpenAPI / Swagger)           |  |
|  +-----------------------------------------------------------------------------------+  |
|           | (Mapped via MapStruct)                                                      |
|           v                                                                             |
|  +-----------------------------------------------------------------------------------+  |
|  | 2. DOMAIN MODEL / AGGREGATE ROOT                                                  |  |
|  |    - Pure Java POJO / Rich Entity (No JPA / No Jackson)                           |  |
|  |    - Encapsulates invariants, business calculations, state transitions            |  |
|  |    - Communicates with Domain Services and Domain Events                          |  |
|  +-----------------------------------------------------------------------------------+  |
|           | (Mapped via MapStruct / Persistence Adapter)                                |
|           v                                                                             |
|  +-----------------------------------------------------------------------------------+  |
|  | 3. PERSISTENCE ENTITY                                                             |  |
|  |    - ORM-specific class (@Entity, @Table, @Column, @ManyToOne)                     |  |
|  |    - Managed by Hibernate / Spring Data JPA                                        |  |
|  |    - Mirrors PostgreSQL / MySQL relational table structures                       |  |
|  +-----------------------------------------------------------------------------------+  |
|           |                                                                             |
|           v                                                                             |
|  [ Relational Database Tables ]                                                         |
+-----------------------------------------------------------------------------------------+
```

### Key Model Characteristics
- **DTO (Transport Boundary)**: Optimized for network transmission. Uses Java 21 `record`s. Never contains business logic.
- **Domain Model (Core Boundary)**: Optimized for correctness and business rule enforcement. Never contains ORM or presentation annotations.
- **Persistence Entity (Storage Boundary)**: Optimized for SQL execution, relational mapping, and Hibernate dirty checking.

---

## 3. How It Works Internally

### Compile-Time Object Mapping with MapStruct

```
[ Developer defines Interface ]
           |
           v
@Mapper(componentModel = "spring")
public interface UserMapper {
    UserDomain toDomain(CreateUserRequestDto dto);
    UserResponseDto toDto(UserDomain domain);
    UserJpaEntity toEntity(UserDomain domain);
    UserDomain fromEntity(UserJpaEntity entity);
}
           |
           v (javac Annotation Processing Phase)
[ MapStruct APT Compiler Plugin ]
           |
           v (Generates High-Performance Java Bytecode)
public class UserMapperImpl implements UserMapper {
    // Generates plain Java getter/setter mapping code:
    // NO reflection! NO runtime overhead! Instant JIT optimization!
    @Override
    public UserDomain toDomain(CreateUserRequestDto dto) {
        if (dto == null) return null;
        return new UserDomain(dto.name(), dto.email());
    }
}
```

1. **Zero Runtime Reflection**: Unlike older reflection-based mappers (such as ModelMapper or Dozer), **MapStruct** executes as an annotation processor during `javac` compilation.
2. **Compile-Time Type Safety**: If a property name is misspelled or types cannot be safely converted, MapStruct fails the build at compile-time with line-number errors.
3. **Garbage Collection Optimization**: Generated mapping methods allocate short-lived objects that are rapidly collected in Young Gen via JVM escape analysis.

---

## 4. Syntax & Usage

### 1. DTO Record Definition

```java
package com.example.user.dto;

import jakarta.validation.constraints.Email;
import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.Size;

public record CreateUserRequest(
    @NotBlank @Size(min = 2, max = 100) String name,
    @NotBlank @Email String email
) {}

public record UserResponse(
    String id,
    String name,
    String email,
    String status
) {}
```

### 2. MapStruct Mapper Specification

```java
package com.example.user.mapper;

import com.example.user.domain.User;
import com.example.user.dto.CreateUserRequest;
import com.example.user.dto.UserResponse;
import com.example.user.entity.UserJpaEntity;
import org.mapstruct.Mapper;
import org.mapstruct.Mapping;
import org.mapstruct.MappingConstants;

@Mapper(componentModel = MappingConstants.ComponentModel.SPRING)
public interface UserMapper {

    // DTO -> Domain
    @Mapping(target = "id", ignore = true)
    @Mapping(target = "status", constant = "PENDING")
    User toDomain(CreateUserRequest request);

    // Domain -> Response DTO
    UserResponse toResponseDto(User domain);

    // Domain -> JPA Entity
    UserJpaEntity toEntity(User domain);

    // JPA Entity -> Domain
    User toDomain(UserJpaEntity entity);
}
```

---

## 5. Practical Examples

### Example 1: Full 3-Model Flow (DTO $\to$ Domain $\to$ Entity)

```java
// 1. DOMAIN MODEL (Pure business rules)
package com.example.account.domain;

import java.math.BigDecimal;
import java.util.Objects;
import java.util.UUID;

public class BankAccount {
    private final UUID id;
    private final String ownerEmail;
    private BigDecimal balance;

    public BankAccount(UUID id, String ownerEmail, BigDecimal initialDeposit) {
        this.id = Objects.requireNonNull(id);
        this.ownerEmail = Objects.requireNonNull(ownerEmail);
        if (initialDeposit.compareTo(BigDecimal.ZERO) < 0) {
            throw new IllegalArgumentException("Initial deposit cannot be negative");
        }
        this.balance = initialDeposit;
    }

    public void withdraw(BigDecimal amount) {
        if (amount.compareTo(this.balance) > 0) {
            throw new InsufficientBalanceException("Cannot withdraw " + amount + " from balance " + this.balance);
        }
        this.balance = this.balance.subtract(amount);
    }

    public UUID getId() { return id; }
    public String getOwnerEmail() { return ownerEmail; }
    public BigDecimal getBalance() { return balance; }
}
```

```java
// 2. PERSISTENCE ENTITY (Database schema mapping)
package com.example.account.entity;

import jakarta.persistence.*;
import java.math.BigDecimal;
import java.util.UUID;

@Entity
@Table(name = "bank_accounts")
public class BankAccountJpaEntity {
    @Id
    @Column(name = "account_id")
    private UUID id;

    @Column(name = "owner_email", nullable = false)
    private String ownerEmail;

    @Column(name = "current_balance", nullable = false)
    private BigDecimal balance;

    @Version
    private Long version; // Optimistic locking

    protected BankAccountJpaEntity() {}
    public BankAccountJpaEntity(UUID id, String ownerEmail, BigDecimal balance) {
        this.id = id;
        this.ownerEmail = ownerEmail;
        this.balance = balance;
    }

    public UUID getId() { return id; }
    public String getOwnerEmail() { return ownerEmail; }
    public BigDecimal getBalance() { return balance; }
}
```

```java
// 3. APPLICATION SERVICE (Orchestrating the transition)
package com.example.account.service;

import com.example.account.domain.BankAccount;
import com.example.account.dto.WithdrawRequestDto;
import com.example.account.dto.AccountResponseDto;
import com.example.account.entity.BankAccountJpaEntity;
import com.example.account.mapper.AccountMapper;
import com.example.account.repository.BankAccountJpaRepository;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

@Service
@Transactional
public class AccountApplicationService {

    private final BankAccountJpaRepository repository;
    private final AccountMapper mapper;

    public AccountApplicationService(BankAccountJpaRepository repository, AccountMapper mapper) {
        this.repository = repository;
        this.mapper = mapper;
    }

    public AccountResponseDto processWithdrawal(WithdrawRequestDto request) {
        // Step A: Load Entity from DB & convert to Domain Model
        BankAccountJpaEntity entity = repository.findById(request.accountId())
                .orElseThrow(() -> new AccountNotFoundException(request.accountId()));
        BankAccount account = mapper.toDomain(entity);

        // Step B: Execute Business Invariant on Pure Domain Model
        account.withdraw(request.amount());

        // Step C: Map Domain Model back to Entity and Save
        BankAccountJpaEntity updatedEntity = mapper.toEntity(account);
        repository.save(updatedEntity);

        // Step D: Return Clean DTO to Web Controller
        return mapper.toResponseDto(account);
    }
}
```

### Example 2: Preventing the Mass-Assignment Security Vulnerability

```java
// BAD: Directly accepting entity in Controller endpoint
@PostMapping("/users")
public UserEntity createUserDangerous(@RequestBody UserEntity user) {
    // Malicious attacker sends:
    // {"name": "Mallory", "email": "m@hack.org", "role": "ADMIN", "isVerified": true}
    // Jackson automatically binds role="ADMIN", granting administrative privileges!
    return userRepository.save(user);
}

// GOOD: Strictly bound DTO record permitting only white-listed fields
public record CreateUserSafeDto(String name, String email) {}

@PostMapping("/users")
public UserResponseDto createUserSafe(@Valid @RequestBody CreateUserSafeDto dto) {
    // Attacker cannot inject "role" or "isVerified" because they do NOT exist on the DTO!
    return userService.register(dto);
}
```

### Example 3: Eliminating `LazyInitializationException` via DTO Projections

```java
package com.example.order.repository;

import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Query;
import org.springframework.data.repository.query.Param;

import java.math.BigDecimal;
import java.util.List;
import java.util.UUID;

// Interface DTO Projection: Spring Data generates SQL selecting ONLY these 3 columns
public interface OrderSummaryProjection {
    UUID getId();
    String getCustomerName();
    BigDecimal getTotal();
}

public interface OrderRepository extends JpaRepository<OrderEntity, UUID> {

    @Query("""
        SELECT o.id AS id, c.name AS customerName, o.totalAmount AS total
        FROM OrderEntity o JOIN o.customer c
        WHERE o.status = :status
        """)
    List<OrderSummaryProjection> findSummariesByStatus(@Param("status") String status);
}
```

### Example 4: Anti-Pattern (Returning JPA Entities to Web Layer) vs Fixed Architecture

#### The Anti-Pattern: Exposing Hibernate Entities Directly
```java
// BAD: Controller returns Entity with @OneToMany relationship
@RestController
public class BadDepartmentController {
    @Autowired private DepartmentRepository repo;

    @GetMapping("/departments/{id}")
    public DepartmentEntity getDepartment(@PathVariable Long id) {
        // Jackson attempts to serialize department.getEmployees()
        // If lazy: throws LazyInitializationException outside @Transactional!
        // If eager: Jackson serializes employee.getDepartment().getEmployees()...
        // Result: StackOverflowError / Heap memory exhaustion!
        return repo.findById(id).orElseThrow();
    }
}
```

#### Production Fix: Explicit Mapping to Flat DTO Record
```java
public record DepartmentDto(Long id, String name, int employeeCount) {}

@RestController
public class CleanDepartmentController {
    private final DepartmentService service;

    public CleanDepartmentController(DepartmentService service) {
        this.service = service;
    }

    @GetMapping("/departments/{id}")
    public ResponseEntity<DepartmentDto> getDepartment(@PathVariable Long id) {
        return ResponseEntity.ok(service.getDepartmentDto(id));
    }
}
```

### Example 5: Custom Type Conversion in MapStruct

```java
package com.example.order.mapper;

import com.example.order.domain.OrderId;
import org.mapstruct.Mapper;
import org.mapstruct.Named;

import java.util.UUID;

@Mapper(componentModel = "spring")
public interface OrderMappingUtils {

    @Named("orderIdToString")
    default String orderIdToString(OrderId orderId) {
        return orderId != null ? orderId.value().toString() : null;
    }

    @Named("stringToOrderId")
    default OrderId stringToOrderId(String id) {
        return id != null ? new OrderId(UUID.fromString(id)) : null;
    }
}
```

---

## 6. Best Practices

1. **Use Java 21 Records for All Inbound and Outbound DTOs**: Records are immutable, have concise syntax, and prevent accidental state mutation.
2. **Never Return an `@Entity` from a `@RestController` Endpoint**: Always map to a dedicated response DTO.
3. **Use MapStruct for Object Mapping**: Avoid manual boilerplate mappings and slow reflection-based libraries (ModelMapper).
4. **Define Separate DTOs for Creation, Update, and Read**: Do not reuse `CreateUserRequest` for `UpdateUserRequest` (update often requires optional fields or IDs).
5. **Put Validation Annotations on DTOs, Invariants on Domain Models**: Bean Validation (`@NotBlank`, `@Email`) validates syntax at the HTTP edge; Domain model constructors validate business invariants (`balance >= 0`).
6. **Enforce Boundary Rules via ArchUnit**: Add automated tests to ensure controllers never reference entity classes.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| **Annotating JPA Entities with `@JsonProperty`** | Couples internal database column naming to public API contracts; renaming a column breaks mobile/web clients. | Keep Jackson annotations exclusively on DTO records. |
| **Mass-Assignment Over-Posting** | Binding HTTP JSON directly to JPA entity allows attackers to overwrite administrative fields. | Always bind requests to explicit, minimal DTO records. |
| **`LazyInitializationException` in Controllers**| Serializing entities with lazy collections outside an active Hibernate session fails. | Map entities to DTOs inside the `@Transactional` service layer before returning. |
| **Cyclic Dependency Infinite JSON Recursion** | Bidirectional `@OneToMany` and `@ManyToOne` entities cause Jackson to recurse infinitely until `StackOverflowError`. | DTO records break object cycles by flattening associations (e.g., returning `customerId` instead of `CustomerEntity`). |
| **Using Reflection-Based Mappers (Dozer/ModelMapper)** | Significant CPU overhead and silent runtime conversion failures. | Switch to MapStruct for compile-time verified, zero-reflection mapping. |
| **Reusing One DTO for All Endpoints** | A single `UserDto` with 30 nullable fields creates confusion over which fields are required for which endpoint. | Create specialized DTOs: `CreateUserCmd`, `UpdateUserCmd`, `UserCardView`. |

---

## 8. Performance & Security Notes

### Performance
- **Database I/O Optimization**: By projecting directly into DTOs (`SELECT new OrderSummaryDto(...)`), queries fetch only the required 4 columns instead of 25 columns and 4 join tables, dramatically reducing database network bandwidth and JVM memory allocation.
- **Escape Analysis**: Small DTO records created inside service methods are optimized by the JVM C2 JIT compiler and allocated on the CPU stack instead of the heap.

### Security
- **Data Masking at DTO Boundary**: Sensitive columns stored in entities (e.g., `hashedPassword`, `ssn`, `taxIdentifier`) are completely omitted from response DTO records, eliminating the risk of accidental PII leakage in API responses.

---

## 9. Comparison / When to Use What

| Architectural Dimension | Data Transfer Object (DTO) | Domain Model | Persistence Entity |
|---|---|---|---|
| **Location** | `com.app.dto` (Web / Inbound) | `com.app.domain` (Core) | `com.app.entity` (Infrastructure) |
| **Java Type** | Java 21 `record` | Rich Class / POJO | Class with `@Entity` |
| **Mutability** | Strictly Immutable | State changes via methods | Mutable (Getters/Setters for JPA) |
| **Annotations** | `@Valid`, `@NotNull`, Jackson | Zero framework annotations | `@Table`, `@Column`, `@Id`, Hibernate |
| **Primary Purpose** | Transport across boundaries | Enforce business rules | Relational schema persistence |

---

## 10. Interview Questions & Deep Dives

<details>
<summary><strong>1. What is the "Mass-Assignment Vulnerability" and how does DTO separation mitigate it?</strong></summary>

Mass-assignment occurs when an HTTP framework binds incoming request fields directly onto a database entity.
If an application exposes `UserEntity` as the `@RequestBody`:
```java
@PostMapping("/profile")
public void updateProfile(@RequestBody UserEntity user) { repo.save(user); }
```
An attacker can submit `{"id": 42, "role": "ADMIN", "accountBalance": 1000000}` along with legitimate fields. Spring/Jackson will automatically bind these values, allowing unauthorized privilege escalation.
**Mitigation**: By accepting an explicit `UpdateProfileRequestDto(String bio, String phone)`, the compiler strictly limits which fields can be updated.
</details>

<details>
<summary><strong>2. Why does MapStruct perform significantly faster than ModelMapper or Dozer?</strong></summary>

- **ModelMapper / Dozer**: Inspect class properties at runtime using Java Reflection and dynamic bytecode generation. Every mapping operation incurs reflective method lookup overhead, object inspection, and boxing/unboxing penalties.
- **MapStruct**: Executes during compilation as an annotation processor. It generates standard, plain Java source code (`target.setName(source.getName())`). At runtime, it executes identical to hand-written getter/setter code with zero reflection overhead, allowing the JVM JIT compiler to inline the method calls.
</details>

<details>
<summary><strong>3. How does DTO separation solve the Hibernate `LazyInitializationException`?</strong></summary>

Hibernate proxies throw `LazyInitializationException` when a caller attempts to access an uninitialized lazy collection after the database transaction (`Session`) has closed.
When returning entities directly from controllers, Jackson serializes fields outside the transaction, triggering this exception.
When using DTO separation:
- The Service layer maps the entity to a DTO *inside* the `@Transactional` boundary.
- If related data is needed, it is explicitly loaded and copied to the DTO.
- The controller receives a fully populated, decoupled DTO record with no Hibernate proxy handles.
</details>

<details>
<summary><strong>4. Why are Java 21 Records recommended for DTOs over standard classes?</strong></summary>

1. **Immutability**: Records are shallowly immutable; once created, fields cannot be altered.
2. **Boilerplate Reduction**: Automatically generates canonical constructors, accessors, `equals()`, `hashCode()`, and `toString()`.
3. **Serialization Friendly**: Jackson 2.12+ supports Java records natively without requiring special creator annotations.
4. **Clarity of Intent**: Communicates unambiguously that the object is purely a transparent data carrier.
</details>

<details>
<summary><strong>5. Should a Domain Model be allowed to import Jackson or JPA annotations?</strong></summary>

No. In Clean and Hexagonal Architecture, the Domain Model represents pure business logic.
- Importing JPA annotations (`@Entity`, `@Table`) couples business logic to relational database schemas and ORM lifecycle quirks.
- Importing Jackson annotations (`@JsonProperty`, `@JsonFormat`) couples the domain model to external REST serialization formats.
The Domain Model should consist of pure Java POJOs, completely independent of external frameworks.
</details>

<details>
<summary><strong>6. What is the difference between Class DTO Projections and Interface DTO Projections in Spring Data JPA?</strong></summary>

- **Interface Projections**: Spring Data dynamically creates a JDK proxy at runtime for an interface with getter methods. It executes queries and maps columns to getters.
- **Class (Record) Projections**: Uses the `SELECT new com.example.MyDto(c.id, c.name) FROM ...` JPQL constructor syntax. It instantiates the concrete DTO directly without proxy overhead, resulting in higher throughput.
</details>

<details>
<summary><strong>7. How do you handle mapping deeply nested entity graphs to flat API DTOs?</strong></summary>

Using MapStruct's dot-notation mapping syntax:
```java
@Mapping(target = "customerName", source = "order.customer.personalInfo.fullName")
@Mapping(target = "deliveryCity", source = "order.shippingAddress.city")
OrderFlatSummaryDto toFlatDto(OrderEntity order);
```
MapStruct automatically generates the null-safe navigation checks (`if (order.getCustomer() != null ...)`).
</details>

<details>
<summary><strong>8. When is it acceptable to bypass the Domain Model and map directly from Persistence Entities to DTOs?</strong></summary>

In read-only query operations (CQRS Read side). When a user requests a paginated table view, running logic through rich domain aggregate models introduces unnecessary overhead. Mapping directly from JPA Entities or SQL Projections directly to Response DTOs provides the highest possible read performance.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Create a Validated User Registration DTO Record</strong></summary>

**Objective**: Model an immutable registration request with email and password constraints.

**Solution**:
```java
public record RegisterUserRequest(
    @NotBlank(message = "Username cannot be blank")
    @Size(min = 3, max = 30)
    String username,

    @NotBlank(message = "Email is required")
    @Email(message = "Must be a valid email address")
    String email,

    @NotBlank(message = "Password is required")
    @Size(min = 8, message = "Password must be at least 8 characters")
    String password
) {}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Define a Basic MapStruct Mapper</strong></summary>

**Objective**: Create a MapStruct interface mapping `ProductEntity` to `ProductResponseDto`.

**Solution**:
```java
@Mapper(componentModel = "spring")
public interface ProductMapper {
    @Mapping(target = "productId", source = "id")
    @Mapping(target = "displayPrice", source = "price")
    ProductResponseDto toDto(ProductEntity entity);
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Implement Constructor Expression DTO Projection in Spring Data</strong></summary>

**Objective**: Write a Spring Data JPA repository query that instantiates a Record DTO directly in JPQL.

**Solution**:
```java
public record UserSummaryRecord(UUID id, String email, long totalOrders) {}

public interface UserRepository extends JpaRepository<UserEntity, UUID> {

    @Query("""
        SELECT new com.example.dto.UserSummaryRecord(u.id, u.email, COUNT(o.id))
        FROM UserEntity u LEFT JOIN u.orders o
        GROUP BY u.id, u.email
        """)
    List<UserSummaryRecord> fetchAllUserSummaries();
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): MapStruct Mapping with Custom Default and Formatting</strong></summary>

**Objective**: Use MapStruct to format an `Instant` field to an ISO-8601 string and apply a default status if null.

**Solution**:
```java
@Mapper(componentModel = "spring")
public interface OrderMapper {

    @Mapping(target = "formattedDate", source = "createdAt", dateFormat = "yyyy-MM-dd'T'HH:mm:ssXXX")
    @Mapping(target = "status", source = "status", defaultValue = "UNKNOWN")
    OrderDetailsDto toDetailsDto(OrderEntity entity);
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Implement Bi-directional Mapping for Nested Aggregate Entities</strong></summary>

**Objective**: Map an `OrderAggregate` containing a list of `OrderItem` domain models to JPA entities with parent-child back-references properly initialized.

**Solution**:
```java
@Mapper(componentModel = "spring")
public abstract class ComplexOrderMapper {

    @Mapping(target = "items", source = "items")
    public abstract OrderJpaEntity toEntity(Order domain);

    @AfterMapping
    protected void linkChildItems(@MappingTarget OrderJpaEntity parentEntity) {
        if (parentEntity.getItems() != null) {
            for (OrderItemJpaEntity item : parentEntity.getItems()) {
                item.setOrder(parentEntity); // Set bidirectional relationship link!
            }
        }
    }
}
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Write an ArchUnit Rule Banning Entities in Web Controllers</strong></summary>

**Objective**: Write an ArchUnit test asserting that no method in `@RestController` classes takes or returns a class annotated with `@Entity`.

**Solution**:
```java
@AnalyzeClasses(packages = "com.example")
public class ModelSeparationArchitectureTest {

    @ArchTest
    public static final ArchRule controllers_must_not_expose_jpa_entities =
        methods().that().areDeclaredInClassesThat().areAnnotatedWith(RestController.class)
            .should(new ArchCondition<JavaMethod>("not take or return @Entity types") {
                @Override
                public void check(JavaMethod method, ConditionEvents events) {
                    if (method.getRawReturnType().isAnnotatedWith(Entity.class)) {
                        events.add(SimpleConditionEvent.violated(method, "Returns entity: " + method.getRawReturnType()));
                    }
                    for (JavaClass param : method.getRawParameterTypes()) {
                        if (param.isAnnotatedWith(Entity.class)) {
                            events.add(SimpleConditionEvent.violated(method, "Accepts entity parameter: " + param));
                        }
                    }
                }
            });
}
```
</details>

---

## 12. Summary & Cheat Sheet

```
+----------------------------------------------------------------------------------------------------+
| DTO, DOMAIN, AND ENTITY SEPARATION CHEAT SHEET                                                     |
+----------------------------------------------------------------------------------------------------+
| Model Type          | Recommended Java Type                     | Core Responsibility              |
+---------------------+-------------------------------------------+----------------------------------+
| DTO (Inbound)       | Java 21 record with @Valid                | Validates client input syntax;   |
|                     |                                           | prevents mass-assignment attacks |
+---------------------+-------------------------------------------+----------------------------------+
| DTO (Outbound)      | Java 21 record                            | Formats API responses; prevents  |
|                     |                                           | lazy loading & data leaks        |
+---------------------+-------------------------------------------+----------------------------------+
| Domain Model        | Rich POJO Class                           | Enforces business invariants and |
|                     | (Zero framework annotations)              | domain calculations              |
+---------------------+-------------------------------------------+----------------------------------+
| Persistence Entity  | Class with @Entity, @Table, @Id           | Maps to database tables; managed |
|                     |                                           | by Hibernate Unit of Work        |
+---------------------+-------------------------------------------+----------------------------------+
| Object Mapper       | MapStruct (componentModel = "spring")     | Fast, compile-time verified,     |
|                     |                                           | zero-reflection object conversion|
+----------------------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [MapStruct Complete Reference Guide](https://mapstruct.org/documentation/stable/reference/html/)
- [Martin Fowler on Data Transfer Object (DTO)](https://martinfowler.com/eaaCatalog/dataTransferObject.html)
- [OWASP: Mass Assignment Vulnerability Prevention](https://cheatsheetseries.owasp.org/cheatsheets/Mass_Assignment_Cheat_Sheet.html)
- [Baeldung: Spring Data JPA Projections Guide](https://www.baeldung.com/spring-data-jpa-projections)
- [ArchUnit User Guide: Custom Predicates and Rules](https://www.archunit.org/user-guide/html/000_Index.html)
