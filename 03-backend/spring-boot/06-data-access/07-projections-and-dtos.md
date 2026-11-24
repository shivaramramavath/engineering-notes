# JPA Projections & DTOs: Interface, Record & Constructor Projections

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | `03-spring-data-jpa-fundamentals.md`, `05-repositories-and-derived-queries.md`, `06-jpql-native-and-specifications.md` |
| **Estimated Reading Time** | 28 minutes |
| **Status** | Active |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework** | 6.2.x / 7.0.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven / Gradle |
| **Tested With** | Spring Data JPA 3.4.x, Hibernate 6.6.x, PostgreSQL 16 |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
   - [5.1 High-Performance Java 21 Record Projections](#51-high-performance-java-21-record-projections)
   - [5.2 Closed Interface-Based Projections](#52-closed-interface-based-projections)
   - [5.3 Dynamic Projections with Generics](#53-dynamic-projections-with-generics)
   - [5.4 Anti-Pattern: Open Projections with SpEL Expressions](#54-anti-pattern-open-projections-with-spel-expressions)
   - [5.5 Constructor Expressions in JPQL with Nested Aggregate Calculations](#55-constructor-expressions-in-jpql-with-nested-aggregate-calculations)
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
In read-heavy applications, fetching entire JPA entities (`SELECT * FROM users`) when a web view or API response only requires two fields (`id` and `name`) is a major performance bottleneck. Loading full entities forces the database to scan and transfer unnecessary columns (including large `TEXT`, `BLOB`, or JSON payloads), triggers unwanted lazy-loading initialization, inflates JVM heap memory, and forces Hibernate to track snapshots in the First-Level Cache for dirty checking even when no mutations will ever occur.

### Historical Evolution
Early JPA developers manually mapped `Object[]` result arrays from queries, creating brittle, error-prone casting code. Spring Data introduced **Interface-based projections**, creating dynamic runtime proxies to expose getter methods for queried columns. In modern Spring Data JPA (3.x) with Java 21, **Record-based projections** and **Constructor expressions** have become the preferred standard, enabling compile-time type safety, zero proxy overhead, and direct instantiation into immutable records.

### Real-World Relevance
High-throughput REST APIs, mobile backend endpoints, and analytical reporting dashboards should rarely return full managed JPA entities. Adopting record and constructor projections reduces database I/O by up to 80%, cuts heap allocations by 60%, and completely eliminates `LazyInitializationException` risks at the API boundary.

---

## 2. Core Concepts

### Projection Categories
1. **Class / Record Projections (DTO Projections)**: Concrete Java 21 `record` or class with a matching constructor. Spring Data inspects constructor parameter names and selects only the corresponding columns in the generated SQL.
2. **Closed Interface Projections**: A Java interface declaring accessor methods (`getId()`, `getEmail()`). Spring Data queries only the declared properties and wraps results in lightweight dynamic proxies.
3. **Open Interface Projections**: An interface using Spring Expression Language (SpEL) annotations (`@Value("#{target.firstName + ' ' + target.lastName}")`). **Warning**: Forces Hibernate to fetch the *entire* entity into memory before evaluating the expression.
4. **Dynamic Projections**: Repository methods declaring a generic type parameter `<T> List<T> findByStatus(String status, Class<T> type)`, allowing the caller to decide at runtime whether to retrieve an entity, a summary record, or an administrative projection.

---

## 3. How It Works Internally

```
+------------------------------------------------------------------------------------+
|                      Record Projection vs Full Entity Loading                      |
+------------------------------------------------------------------------------------+
|                                                                                    |
|  Scenario A: Full Entity Loading (userRepository.findAll())                        |
|  Query: SELECT id, username, password_hash, profile_blob, audit_data, ... FROM users|
|  Memory: Allocates User object + EntityEntry dirty check snapshot in L1 Cache      |
|  Overhead: Heavy memory footprint, tracking overhead, exposes sensitive data       |
|                                                                                    |
|  Scenario B: Record Projection (userRepository.findByStatus(UserSummary.class))   |
|  Query: SELECT id, username FROM users WHERE status = ?                            |
|  Memory: Direct instantiation: new UserSummary(rs.getLong(1), rs.getString(2))     |
|  Overhead: ZERO First-Level Cache tracking, minimal network transfer, IMMUTABLE   |
|                                                                                    |
+------------------------------------------------------------------------------------+
```

1. **Query Construction**: Spring Data inspects the target Projection type (e.g., `UserSummaryRecord`).
2. **Column Projection**: Instead of `SELECT u FROM User u`, Spring Data adjusts the select clause to `SELECT u.id, u.username FROM User u`.
3. **Instantiation**: The JDBC `ResultSet` values are passed directly to the record's canonical constructor. The resulting record is completely detached from the JPA persistence context.

---

## 4. Syntax & Usage

### Declaring a Record Projection
```java
public record UserSummary(Long id, String email, String fullName) {}

public interface UserRepository extends JpaRepository<User, Long> {
    // Spring Data automatically selects only id, email, and fullName
    List<UserSummary> findByActiveTrue();
}
```

### Declaring a Closed Interface Projection
```java
public interface CustomerNameOnly {
    Long getId();
    String getName();
}

public interface CustomerRepository extends JpaRepository<Customer, Long> {
    List<CustomerNameOnly> findByCity(String city);
}
```

---

## 5. Practical Examples

### 5.1 High-Performance Java 21 Record Projections
```java
package com.example.jpa.dto;

import java.math.BigDecimal;
import java.time.Instant;

public record ProductSummaryDto(
    Long id,
    String sku,
    String name,
    BigDecimal price,
    Instant createdAt
) {}
```

```java
package com.example.jpa.repository;

import com.example.jpa.domain.Product;
import com.example.jpa.dto.ProductSummaryDto;
import org.springframework.data.jpa.repository.JpaRepository;

import java.math.BigDecimal;
import java.util.List;
import java.util.Optional;

public interface ProductRepository extends JpaRepository<Product, Long> {

    // Automatically queries only id, sku, name, price, created_at
    List<ProductSummaryDto> findByCategoryName(String categoryName);

    Optional<ProductSummaryDto> findSummaryBySku(String sku);

    List<ProductSummaryDto> findByPriceLessThanEqual(BigDecimal maxPrice);
}
```

### 5.2 Closed Interface-Based Projections
```java
package com.example.jpa.projection;

import org.springframework.beans.factory.annotation.Value;

public interface OrderHeaderProjection {
    Long getId();
    String getOrderNumber();
    String getStatus();

    // Nested projection: maps child customer attributes via proxy
    CustomerInfo getCustomer();

    interface CustomerInfo {
        String getEmail();
        String getName();
    }
}
```

```java
package com.example.jpa.repository;

import com.example.jpa.domain.Order;
import com.example.jpa.projection.OrderHeaderProjection;
import org.springframework.data.jpa.repository.JpaRepository;

import java.util.List;

public interface OrderRepository extends JpaRepository<Order, Long> {
    List<OrderHeaderProjection> findByStatus(String status);
}
```

### 5.3 Dynamic Projections with Generics
```java
package com.example.jpa.repository;

import com.example.jpa.domain.Employee;
import org.springframework.data.jpa.repository.JpaRepository;

import java.util.List;
import java.util.Optional;

public interface EmployeeRepository extends JpaRepository<Employee, Long> {

    /**
     * Dynamic projection: Caller determines the return type at invocation time!
     * Can return Employee (entity), EmployeeSummary (record), or EmployeeIdOnly (interface).
     */
    <T> List<T> findByDepartmentName(String deptName, Class<T> projectionType);

    <T> Optional<T> findById(Long id, Class<T> projectionType);
}
```

```java
// Usage in Service:
List<EmployeeSummaryDto> summaries = employeeRepository.findByDepartmentName(
    "Engineering", 
    EmployeeSummaryDto.class
);

List<EmployeeAdminView> adminViews = employeeRepository.findByDepartmentName(
    "Engineering", 
    EmployeeAdminView.class
);
```

### 5.4 Anti-Pattern: Open Projections with SpEL Expressions
```java
// =========================================================================
// ANTI-PATTERN: Open Projection with SpEL (Performance Trap)
// =========================================================================
public interface BadUserProjection {
    Long getId();

    // PERFORMANCE DISASTER:
    // Because SpEL evaluates '#{target...}', Spring Data CANNOT optimize the SQL query!
    // It issues: SELECT * FROM users (fetches ALL 40 columns including BLOBs/JSON)!
    // Then instantiates the full Entity and runs SpEL reflection in memory!
    @Value("#{target.firstName + ' ' + target.lastName}")
    String getFullName();
}

// =========================================================================
// PRODUCTION FIX: Database-Level Concatenation or Java 21 Record Compact Constructor
// =========================================================================
public record GoodUserSummary(Long id, String firstName, String lastName) {
    // Computed property in pure Java memory with zero SpEL overhead
    public String fullName() {
        return firstName + " " + lastName;
    }
}
```

### 5.5 Constructor Expressions in JPQL with Nested Aggregate Calculations
```java
package com.example.jpa.dto;

import java.math.BigDecimal;

public record DepartmentStatistics(
    String departmentName,
    long employeeCount,
    BigDecimal averageSalary,
    BigDecimal maxSalary
) {}
```

```java
package com.example.jpa.repository;

import com.example.jpa.domain.Department;
import com.example.jpa.dto.DepartmentStatistics;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Query;

import java.util.List;

public interface DepartmentRepository extends JpaRepository<Department, Long> {

    /**
     * Explicit JPQL Constructor Expression:
     * Instantiates DepartmentStatistics directly from aggregate calculations.
     */
    @Query("""
        SELECT new com.example.jpa.dto.DepartmentStatistics(
            d.name,
            COUNT(e),
            COALESCE(AVG(e.salary), 0),
            COALESCE(MAX(e.salary), 0)
        )
        FROM Department d
        LEFT JOIN d.employees e
        GROUP BY d.name
        ORDER BY d.name ASC
        """)
    List<DepartmentStatistics> calculateDepartmentStatistics();
}
```

---

## 6. Best Practices

1. **Prefer Java 21 Records Over Interface Projections**: Records provide immutable value semantics, avoid dynamic proxy overhead, support pattern matching, and integrate cleanly with Jackson.
2. **Never Return Entities from Read-Only APIs**: Exposing JPA entities directly through REST controllers risks leaking sensitive attributes (e.g., password hashes) and triggers `LazyInitializationException` during JSON serialization.
3. **Avoid Open Projections (`@Value`)**: Open projections defeat the purpose of projections by forcing Hibernate to fetch every column of the underlying entity table.
4. **Use Constructor Expressions for Aggregates**: When calculating `COUNT`, `SUM`, `AVG`, or multi-table joins, map directly into a record via `SELECT new com.example.Dto(...)`.
5. **Use Dynamic Projections to Prevent Repository Bloat**: Instead of creating separate query methods (`findSummaryById`, `findDetailById`), use a single generic method `<T> Optional<T> findById(Long id, Class<T> type)`.
6. **Alias Native Query Columns to Record Constructor Names**: When using record projections with native queries, ensure SQL column aliases (`AS myColumn`) match the record component names.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Using Open Projections with SpEL** | Causes full table scan and loads all columns, negating query optimization. | Use Java 21 records with instance helper methods. |
| **Package Typo in JPQL Constructor Expressions** | JPQL requires the fully qualified class name; a typo causes a runtime `ClassNotFoundException` or parse error. | Verify fully qualified package paths or use derived record queries. |
| **Mutating Projection Data** | Projections are read-only; attempts to modify fields have no effect on the database. | Projections are read-models; load the managed entity if updates are required. |
| **Serializing Un-initialized Lazy Fields in Projections** | Calling a getter on an unmapped relationship inside an interface projection triggers `LazyInitializationException`. | Use nested interface projections or explicit `JOIN FETCH`. |
| **Record Component Name Mismatch** | Spring Data derived queries map columns to record components by name; mismatched names result in `null` fields. | Align record component names with entity attribute names. |
| **Overloading Constructor Signatures in DTO Classes** | Hibernate can struggle to select the intended constructor if multiple constructors have ambiguous types. | Use Java records with a single canonical constructor. |

---

## 8. Performance & Security Notes

### Performance
- **Zero L1 Cache Footprint**: Projections bypass Hibernate's First-Level Cache completely. They do not generate dirty-check snapshots, dramatically reducing garbage collection overhead during large queries.
- **Network Bandwidth Reduction**: On tables with wide columns (e.g., text, JSONB, blobs), selecting only needed columns reduces network packet transfer between the database and application tier by orders of magnitude.

### Security
- **Data Encapsulation**: Returning projections prevents mass-assignment and over-fetching vulnerabilities (OWASP API3: Broken Object Property Level Authorization). Sensitive fields like internal keys, password hashes, and audit tokens are never queried or exposed.

---

## 9. Comparison / When to Use What

| Projection Mechanism | Proxy Overhead | Read-Only Enforced | Custom Computed Logic | Best Use Case |
| :--- | :--- | :--- | :--- | :--- |
| **Java 21 Record** | **None (Native class)** | **Yes (Immutable)** | Via record methods | **Standard for all read-only views** |
| **Closed Interface** | Moderate (AOP Proxy) | Yes | No | Quick nested projections |
| **Open Interface (`@Value`)** | High (Proxy + SpEL) | Yes | Yes (SpEL) | **Avoid in production** |
| **Constructor Expression** | None | Yes | Via SQL functions | Aggregates & multi-table joins |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the primary performance benefit of using DTO/Record projections over JPA entities?</strong></summary>

Projections only query the specific columns requested from the database, minimizing I/O and network transfer. Furthermore, projections are instantiated directly without being attached to the JPA persistence context (First-Level Cache). This eliminates dirty-checking snapshot creation and proxy interception, significantly reducing heap memory consumption and GC pressure.
</details>

<details>
<summary><strong>2. What is the difference between a Closed Projection and an Open Projection?</strong></summary>

- **Closed Projection**: An interface that defines getters matching exact entity property names. Spring Data optimizes the underlying query to select *only* those attributes (`SELECT u.id, u.name`).
- **Open Projection**: Uses Spring's `@Value` annotation with SpEL expressions (e.g., `@Value("#{target.firstName + ' ' + target.lastName}")`). Spring Data cannot determine which columns are needed, so it fetches the *entire entity* and computes the value in memory.
</details>

<details>
<summary><strong>3. How does Spring Data JPA handle Java 21 Record projections?</strong></summary>

Spring Data detects that the target class is a Java `record`, inspects its canonical constructor components, matches their names against entity attributes, generates a query selecting only those fields, and instantiates the record using its constructor without requiring proxies or reflection wrappers.
</details>

<details>
<summary><strong>4. How do you implement dynamic projections in Spring Data repositories?</strong></summary>

By adding a generic type parameter `<T>` and accepting `Class<T> type` as a method argument:
```java
<T> List<T> findByStatus(String status, Class<T> type);
```
Callers can dynamically pass `UserSummary.class`, `UserDetails.class`, or `User.class` at runtime.
</details>

<details>
<summary><strong>5. Why is returning JPA entities directly from REST controllers considered an anti-pattern?</strong></summary>

1. **Security**: May accidentally expose sensitive attributes (e.g., password hashes, internal status flags).
2. **Circular References**: Bidirectional associations cause `StackOverflowError` during JSON serialization.
3. **LazyInitializationException**: Jackson attempts to serialize uninitialized lazy collections outside the transaction boundary.
4. **Performance**: Fetches all table columns when only a few are needed.
</details>

<details>
<summary><strong>6. How do constructor expressions work in JPQL?</strong></summary>

They use the `SELECT new com.example.MyDto(p.prop1, p.prop2) FROM ...` syntax. Hibernate calls the constructor of `MyDto` directly using the query results, allowing projection of calculated aggregates (`COUNT`, `AVG`) into strongly-typed DTOs.
</details>

<details>
<summary><strong>7. Can native SQL queries be mapped to Java record projections?</strong></summary>

Yes. In Spring Data JPA 3.x, native queries can project into Java records provided the SQL column aliases in the `SELECT` clause match the component names of the record.
</details>

<details>
<summary><strong>8. Are projections managed by the JPA persistence context?</strong></summary>

No. Projections (interfaces, records, and DTOs) are unmanaged, read-only data structures. Any changes made to their fields or properties are not tracked by Hibernate and will not trigger database updates.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Basic Record Projection</strong></summary>

**Task**: Define a record `UserContactInfo(String email, String phoneNumber)` and a repository method in `UserRepository` to retrieve it by user ID.

```java
public record UserContactInfo(String email, String phoneNumber) {}

public interface UserRepository extends JpaRepository<User, Long> {
    Optional<UserContactInfo> findContactInfoById(Long id);
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Closed Interface Projection</strong></summary>

**Task**: Create an interface projection `BookTitleOnly` with `Long getId()` and `String getTitle()` and add a repository method to find books by author ID.

```java
public interface BookTitleOnly {
    Long getId();
    String getTitle();
}

public interface BookRepository extends JpaRepository<Book, Long> {
    List<BookTitleOnly> findByAuthorId(Long authorId);
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Dynamic Projection Repository Implementation</strong></summary>

**Task**: Write a repository method that allows querying customers by country using a dynamic projection class.

```java
public interface CustomerRepository extends JpaRepository<Customer, Long> {
    <T> List<T> findByCountry(String country, Class<T> projectionType);
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): JPQL Constructor Expression with Date Extraction</strong></summary>

**Task**: Write a JPQL query that projects an `Order` into an `OrderMonthlyReport(int year, int month, long orderCount, BigDecimal totalRevenue)`.

```java
@Query("""
    SELECT new com.example.dto.OrderMonthlyReport(
        YEAR(o.orderDate),
        MONTH(o.orderDate),
        COUNT(o),
        SUM(o.totalAmount)
    )
    FROM Order o
    GROUP BY YEAR(o.orderDate), MONTH(o.orderDate)
    """)
List<OrderMonthlyReport> generateMonthlyReport();
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Native Query with Record Projection in PostgreSQL</strong></summary>

**Task**: Write a native SQL query that joins two tables and maps into a record `InventoryStatusRecord(String sku, String warehouseCode, int availableQuantity)` using column aliases.

```java
public record InventoryStatusRecord(String sku, String warehouseCode, int availableQuantity) {}

public interface InventoryRepository extends JpaRepository<Inventory, Long> {
    @Query(value = """
        SELECT 
            i.sku AS sku,
            w.code AS warehouseCode,
            i.qty_available AS availableQuantity
        FROM inventory i
        JOIN warehouses w ON i.warehouse_id = w.id
        WHERE i.qty_available > 0
        """, nativeQuery = true)
    List<InventoryStatusRecord> findAvailableInventory();
}
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Multi-Level Nested Interface Projection</strong></summary>

**Task**: Create a nested interface projection for an `Invoice` that projects the invoice ID, total, and a list of nested line item summaries (`description`, `amount`).

```java
public interface InvoiceSummary {
    Long getId();
    BigDecimal getTotal();
    List<LineItemSummary> getItems();

    interface LineItemSummary {
        String getDescription();
        BigDecimal getAmount();
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------------+
| Best Choice:  Java 21 Records (Zero proxy overhead, immutable, compile-time checked)  |
| Closed:       interface MyProj { String getName(); } (Spring generates proxy)         |
| Dynamic:      <T> List<T> findByStatus(String s, Class<T> type)                       |
| Constructors: SELECT new com.example.MyRecord(u.id, u.name) FROM User u               |
| Anti-Pattern: Open Projections (@Value SpEL) - forces full table load!                |
| Benefit:      Bypasses L1 cache, reduces memory by 60%, avoids lazy init exceptions   |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Spring Data JPA Projections Documentation](https://docs.spring.io/spring-data/jpa/reference/repositories/projections.html)
- [Vlad Mihalcea: The Best Way to Map Projections with Spring Data JPA](https://vladmihalcea.com/spring-data-jpa-dto-projection/)
- [Java 21 Records in Enterprise Spring Architecture](https://spring.io/blog/2021/01/25/records-and-runtime-reflection-in-spring)
- [Hibernate 6.6 SQM Instantiation Expressions](https://docs.jboss.org/hibernate/orm/6.6/userguide/html_single/Hibernate_User_Guide.html#hql-select-clause)
