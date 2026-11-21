# Layered Architecture in Modern Java

> Structuring enterprise applications into clean, cohesive layers: Presentation (API), Business (Domain/Service), Persistence (Data Access), and Database tiers with strict boundary separation and DTO mapping.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core Architecture / Structural |
| **Prerequisites** | [00_dependency-injection](00_dependency-injection.md), [00_solid-principles](../23-design-and-clean-code/00_solid-principles.md) |
| **Time to Read** | 35 minutes |
| **Status** | Verified Production Guide |
| **Java Baseline** | Java 17 LTS / Java 21 LTS / Java 25 LTS |

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
10. [Interview Questions & Answers](#10-interview-questions--answers)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### What is Layered Architecture?
Layered Architecture (N-Tier Architecture) is the most widely adopted architectural style for enterprise software. It organizes an application into **horizontal, logical layers**, where each layer has a specific, well-defined role and responsibility.

The Standard 4-Tier Model:
1. **Presentation Layer (Web / API / Controller)**: Handles HTTP requests, JSON serialization, REST endpoints, and authentication tokens.
2. **Business / Service Layer**: Encapsulates core business rules, transactional boundaries, and workflow orchestration.
3. **Persistence / Data Access Layer (Repository / DAO)**: Interacts with databases, executes SQL, and maps tabular records to entities.
4. **Database Tier**: The underlying relational (PostgreSQL) or NoSQL storage engine.

### Why Strict Layering is Vital
Without architectural layering, applications degenerate into a "Big Ball of Mud":
- Controllers execute raw SQL queries directly.
- Persistence entities leak into HTTP JSON responses, exposing internal database schemas and password hashes to clients.
- Business rules are duplicated across controllers, batch jobs, and database triggers.

---

## 2. Core Concepts

### The Strict Layering Rule (Dependency Direction)
Dependencies in a layered system must flow **strictly downward**:
```
[ Presentation Layer ] ---> depends on ---> [ Service Layer ]
                                                  |
                                                  v
                                     [ Persistence Layer ]
                                                  |
                                                  v
                                         [ Database ]
```
- A higher layer may only depend on the layer immediately beneath it (or through defined domain abstractions).
- Lower layers **never** depend on higher layers (the persistence layer has zero knowledge of HTTP or REST).

### Data Transfer Objects (DTOs) vs. Entities
- **Domain Entity (`UserEntity`)**: Represents the internal database table structure. Contains database IDs, audit timestamps, and relational foreign keys.
- **Request/Response DTO (`UserResponse`)**: Modeled as an immutable Java **Record**. Represents only the specific fields the external client is authorized to see.
- Entities must **never** be returned directly from controller endpoints.

---

## 3. How It Works Internally

### Cross-Layer Data Flow
```
Client HTTP Request
        |
        v
[ Presentation Layer: UserController ]
  1. Validates Request DTO (record)
  2. Calls userService.registerUser(dto)
        |
        v
[ Service Layer: UserService ]
  1. Enforces business rules (duplicate email check)
  2. Manages @Transactional boundary
  3. Maps DTO -> Domain Entity
  4. Calls userRepository.save(entity)
        |
        v
[ Persistence Layer: UserRepository ]
  1. Executes PreparedStatement SQL
  2. Maps ResultSet -> Domain Entity
        |
        v
[ Database (PostgreSQL) ]
```

---

## 4. Syntax & Usage

### 1. DTO to Entity Mapping with Records
```java
// Presentation DTO
public record RegisterUserRequest(String email, String rawPassword) {}
public record UserResponse(long id, String email) {}

// Domain Entity
public class UserEntity {
    private Long id;
    private String email;
    private String passwordHash;
    // Getters and setters...
}
```

---

## 5. Practical Examples

### Example 1: Full 3-Tier Enterprise Registration Flow
**Goal:** Implement a completely separated Presentation $\to$ Service $\to$ Persistence architecture.

```java
import java.util.Objects;
import java.util.Optional;

// ==========================================
// Tier 1: Presentation Layer
// ==========================================
public record CreateAccountDto(String email, String password) {}
public record AccountDto(long id, String email) {}

public class AccountController {
    private final AccountService service;

    public AccountController(AccountService service) {
        this.service = Objects.requireNonNull(service);
    }

    public AccountDto handleCreateAccount(CreateAccountDto request) {
        // Controller validates syntax and delegates to business service
        if (request.email() == null || !request.email().contains("@")) {
            throw new IllegalArgumentException("Malformed email address");
        }
        return service.createAccount(request);
    }
}

// ==========================================
// Tier 2: Business Service Layer
// ==========================================
public class AccountService {
    private final AccountRepository repository;

    public AccountService(AccountRepository repository) {
        this.repository = Objects.requireNonNull(repository);
    }

    public AccountDto createAccount(CreateAccountDto dto) {
        // Business Rule: Email uniqueness
        if (repository.findByEmail(dto.email()).isPresent()) {
            throw new IllegalStateException("Email already registered!");
        }

        // Domain transformation
        String hash = "HASHED_" + dto.password(); // Simulated hash
        AccountEntity entity = new AccountEntity(null, dto.email(), hash);
        AccountEntity saved = repository.save(entity);

        return new AccountDto(saved.getId(), saved.getEmail());
    }
}

// ==========================================
// Tier 3: Persistence Layer
// ==========================================
public class AccountEntity {
    private Long id;
    private String email;
    private String passwordHash;

    public AccountEntity(Long id, String email, String hash) {
        this.id = id; this.email = email; this.passwordHash = hash;
    }
    public Long getId() { return id; }
    public String getEmail() { return email; }
    public void setId(Long id) { this.id = id; }
}

public interface AccountRepository {
    Optional<AccountEntity> findByEmail(String email);
    AccountEntity save(AccountEntity entity);
}
```

---

## 6. Best Practices

1. **Never Leak Entities to Controllers**: Always map domain entities to immutable record DTOs before returning from the service layer.
2. **Keep Controllers Thin**: Controllers should only handle HTTP status codes, deserialization, and validation. Zero business logic.
3. **Place Transaction Boundaries at the Service Layer**: Mark service methods with `@Transactional`; persistence methods should participate in the service transaction.
4. **Use Constructor Dependency Injection**: Wire each layer downward through constructor parameters.
5. **Define Repository Interfaces**: Ensure services depend on repository interfaces, not concrete SQL implementations.
6. **Avoid Sinkhole Anti-Pattern**: If a service method does nothing except call `repository.findAll()`, consider whether a separate layer is truly adding value or if clean CQRS queries are better.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Secure Fix |
| :--- | :--- | :--- |
| **Exposing JPA Entities in REST APIs** | Leaks internal database schema and triggers lazy-loading `LazyInitializationException`. | Map to immutable Java Record DTOs. |
| **Business logic in Controllers** | Logic cannot be reused by batch jobs or CLI commands; hard to test. | Move all business rules into Service layer. |
| **Circular dependencies between layers** | Architecture becomes tightly coupled and unmaintainable. | Enforce strict downward dependency flow. |
| **Database queries in Presentation layer** | Controllers opening database connections bypass service transactions and security. | Route all persistence through Service and Repository. |

---

## 8. Performance & Security Notes

### Zero Serialization Leaks with DTO Records
Returning JPA entities directly to Jackson serialization can accidentally expose password hashes, social security numbers, or internal foreign keys in JSON payloads.
Mapping entities to concise Java records (`UserResponse`) ensures **strict data minimization** and eliminates serialization recursion loops.

---

## 9. Comparison / When to Use What

| Architecture Style | Complexity | Flexibility | Best For |
| :--- | :--- | :--- | :--- |
| **Layered Architecture** | Low to Moderate | Moderate | Standard CRUD, medium enterprise apps |
| **Hexagonal / Ports & Adapters**| Moderate to High | High | Domain-heavy DDD, long-lived core models |
| **Clean / Onion Architecture** | High | Maximum | Core domains with multiple delivery channels |

---

## 10. Interview Questions & Answers

### Q1: What is the primary difference between a Domain Entity and a Data Transfer Object (DTO)?
<details>
<summary>View Answer</summary>
A Domain Entity represents a persistent business object modeled after the database schema, holding internal IDs, relationships, and lifecycle state. A DTO (Data Transfer Object) is an immutable data carrier modeled specifically for the needs of the client interface (HTTP REST, gRPC). Using DTOs prevents over-fetching, protects internal database schemas from external exposure, avoids lazy-loading exceptions, and decouples API contracts from database migrations.
</details>

### Q2: What is the "Architecture Sinkhole Anti-Pattern" in layered architecture?
<details>
<summary>View Answer</summary>
The Architecture Sinkhole occurs when requests pass straight through multiple layers with zero business logic or transformation performed at each layer (e.g. Controller calls Service, Service calls Repository, Repository calls Database, all simply forwarding the call). While some pass-through is normal in CRUD apps, if 80% of methods are sinkholes, the architecture has too many layers.
</details>

### Q3: Why should `@Transactional` boundaries be managed at the Service Layer rather than the Repository Layer?
<details>
<summary>View Answer</summary>
Business transactions frequently involve coordinating multiple repository operations (e.g. `orderRepo.save(order)` AND `inventoryRepo.decrement(stock)`). If transactions are placed on repositories, each call commits in an isolated transaction. If the second call fails, the first cannot be rolled back. Placing `@Transactional` on the service method ensures the entire business unit of work succeeds or rolls back atomically.
</details>

### Q4: What is the difference between "Closed" and "Open" layers in Layered Architecture?
<details>
<summary>View Answer</summary>
- **Closed Layer**: A request must pass through the layer immediately beneath it and cannot skip layers (e.g. Presentation MUST go through Service to reach Persistence). Preserves strict isolation.
- **Open Layer**: A request is permitted to bypass an intermediate layer and call a lower layer directly (e.g. Presentation calling Persistence directly for read-only queries). Increases performance, but weakens architectural boundaries.
</details>

### Q5: Can Java Records be used across all layers?
<details>
<summary>View Answer</summary>
Records are ideal for DTOs in the presentation layer and value objects in the service layer because they are immutable. However, traditional ORMs (like Hibernate/JPA) historically required mutable entities with no-arg constructors and proxies, making records less suitable as JPA entities. Modern architectures use records for DTOs and immutable domain models, and plain classes or JDBC row mappers for persistence.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a Pure DTO Mapper
**Task:** Create a static mapper method converting `UserEntity` to `UserSummaryDto`.
<details>
<summary>View Solution</summary>

```java
public record UserSummaryDto(long id, String displayName) {}

public class UserMapper {
    public static UserSummaryDto toDto(AccountEntity entity) {
        return new UserSummaryDto(entity.getId(), entity.getEmail().split("@")[0]);
    }
}
```
</details>

### Exercise 2: Enforce Downward Layering Test
**Task:** Write an architectural assertion (conceptually matching ArchUnit) ensuring controllers do not reference repositories directly.
<details>
<summary>View Solution</summary>

```java
// Conceptual ArchUnit rule:
// classes().that().resideInAPackage("..controller..")
//     .should().onlyDependOnClassesThat().resideInAnyPackage("..service..", "java..");
```
</details>

### Exercise 3: Service Layer Transaction Coordinator
**Task:** Write a service method coordinating payment and order creation.
<details>
<summary>View Solution</summary>

```java
public class CheckoutCoordinator {
    private final PaymentGateway payment;
    private final OrderRepository orders;

    public CheckoutCoordinator(PaymentGateway p, OrderRepository o) {
        this.payment = p; this.orders = o;
    }

    public void process(Order o, String card) {
        payment.charge(card, o.amount().doubleValue());
        orders.save(o);
    }
}
```
</details>

---


### 11.4 Hexagonal Architecture (Ports and Adapters) vs Layered Architecture

While traditional 3-tier Layered Architecture structures code horizontally by technical function (Presentation -> Service -> Persistence), **Hexagonal Architecture** (introduced by Alistair Cockburn) places domain logic strictly at the core and decouples all external technologies via Ports and Adapters:

```
+-------------------------------------------------------------------------------+
|                           DRIVING / PRIMARY SIDE                              |
|   +-----------------------+     +-----------------------+                     |
|   |   REST Controller     |     |   CLI Command Handler |                     |
|   |       (Adapter)       |     |       (Adapter)       |                     |
|   +-----------+-----------+     +-----------+-----------+                     |
|               |                             |                                 |
|               +-------------->+<------------+                                 |
|                               |                                               |
|                    +----------v----------+                                    |
|                    |   OrderUseCases     | (Driving Port: Interface)          |
|                    +----------+----------+                                    |
|                               |                                               |
|            +------------------v------------------+                            |
|            |           DOMAIN CORE               |                            |
|            | - Entities (Order, OrderLine)       |                            |
|            | - Value Objects (Money, Currency)   |                            |
|            | - Domain Services (PricingEngine)   |                            |
|            +------------------+------------------+                            |
|                               |                                               |
|                    +----------v----------+                                    |
|                    |   OrderRepository   | (Driven Port: Interface)           |
|                    |   NotificationPort  |                                    |
|                    +----------+----------+                                    |
|                               |                                               |
|               +---------------+---------------+                               |
|               |                               |                               |
|   +-----------v-----------+     +-------------v-------------+                 |
|   | PostgresOrderStorage  |     |   KafkaNotificationSender |                 |
|   |       (Adapter)       |     |          (Adapter)        |                 |
|   +-----------------------+     +---------------------------+                 |
|                           DRIVEN / SECONDARY SIDE                             |
+-------------------------------------------------------------------------------+
```

#### Package-by-Layer vs Package-by-Feature
In enterprise Java development, organizing code by architectural layer often degrades modularity:

| Metric | Package-by-Layer | Package-by-Feature (Screaming Architecture) |
| :--- | :--- | :--- |
| **Directory Structure** | `com.app.controllers`, `com.app.services`, `com.app.repositories` | `com.app.orders`, `com.app.billing`, `com.app.users` |
| **Encapsulation** | Every class must be `public` to allow cross-package access | Domain classes and repositories can be package-private (`default`) |
| **High Cohesion** | Low; a single feature change touches every package | High; modifying an order feature touches only `com.app.orders` |
| **Extraction to Microservices**| Extremely painful; requires splitting every single package | Trivial; slice out the feature package into its own module/repo |
| **Coupling** | High risk of unintended cross-domain imports | Enforced by module boundaries (`module-info.java`) |

#### Enforcing Architectural Boundaries with ArchUnit
To prevent junior developers or rushed commits from violating layered boundaries, configure automated unit tests with ArchUnit:

```java
import com.tngtech.archunit.core.importer.ClassFileImporter;
import com.tngtech.archunit.lang.ArchRule;
import static com.tngtech.archunit.lang.syntax.ArchRuleDefinition.classes;
import static com.tngtech.archunit.library.Architectures.layeredArchitecture;

public class ArchitectureRulesTest {
    @Test
    void enforceLayerDependencies() {
        var importedClasses = new ClassFileImporter().importPackages("com.example.app");

        ArchRule layerRule = layeredArchitecture()
            .consideringAllDependencies()
            .layer("Controller").definedBy("..controller..")
            .layer("Service").definedBy("..service..")
            .layer("Persistence").definedBy("..repository..")
            .whereLayer("Controller").mayNotBeAccessedByAnyLayer()
            .whereLayer("Service").mayOnlyBeAccessedByLayers("Controller")
            .whereLayer("Persistence").mayOnlyBeAccessedByLayers("Service");

        layerRule.check(importedClasses);
    }
}
```

## 12. Summary / Cheat Sheet

- **Standard 4 Tiers**: Presentation $\to$ Service $\to$ Persistence $\to$ Database.
- **Strict Downward Flow**: Dependencies only point downward; lower layers never know higher layers.
- **DTOs vs Entities**: Always return immutable record DTOs to controllers; never leak entities.
- **Thin Controllers**: Controllers handle HTTP and validation; Services handle business rules.
- **Transactions**: Manage `@Transactional` boundaries at the Service layer.
- **Decoupling**: Wire layers using interfaces and Constructor Dependency Injection.

---

## 13. Further Reading
- Software Architecture Patterns: [Mark Richards (O'Reilly)](https://www.oreilly.com/library/view/software-architecture-patterns/9781491971437/)
- Martin Fowler: [Patterns of Enterprise Application Architecture](https://martinfowler.com/books/eaa.html)
- ArchUnit: [Unit Testing Java Architecture](https://www.archunit.org/)
