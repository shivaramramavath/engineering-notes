# JdbcClient & JdbcTemplate: Modern Fluent SQL in Spring Boot

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | `01-core-spring/01-ioc-container-and-application-context.md`, SQL fundamentals (`09-sql`) |
| **Estimated Reading Time** | 28 minutes |
| **Status** | Active |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework** | 6.2.x / 7.0.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven / Gradle |
| **Tested With** | PostgreSQL 16, HikariCP 5.1.x, Spring Boot 3.4.x |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
   - [5.1 Modern Querying with JdbcClient and Java 21 Records](#51-modern-querying-with-jdbcclient-and-java-21-records)
   - [5.2 Enterprise Parameter Binding and Batch Updates](#52-enterprise-parameter-binding-and-batch-updates)
   - [5.3 Edge Case: Dynamic Filtering and IN-Clause Expansion](#53-edge-case-dynamic-filtering-and-in-clause-expansion)
   - [5.4 Anti-Pattern: String Concatenation and SQL Injection](#54-anti-pattern-string-concatenation-and-sql-injection)
   - [5.5 High-Throughput Streaming with KeyHolder and Stream Queries](#55-high-throughput-streaming-with-keyholder-and-stream-queries)
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
Standard JDBC (`java.sql.Connection`, `PreparedStatement`, `ResultSet`) requires excessive boilerplate: manually opening and closing connections, handling checked `SQLException`s, tracking parameter indexes, and writing verbose loops to map column values. While Spring's venerable `JdbcTemplate` solved the connection management and exception translation problems, its overloaded methods (`query(String, RowMapper, Object...)`) suffer from cluttered syntax, type unsafety, and clumsy parameter handling when dealing with named parameters.

### Historical Evolution
Spring Framework 2.0 introduced `NamedParameterJdbcTemplate` to alleviate positional parameter errors. In Spring Framework 6.1 (Spring Boot 3.2+), the team introduced **`JdbcClient`**—a modern, unified, fluent synchronous database client inspired by `WebClient` and `RestClient`. `JdbcClient` unifies positional and named parameter binding, seamlessly maps relational result sets directly into immutable Java 21 records via constructor reflection, and retains the robust exception translation and transaction synchronization of `JdbcTemplate`.

### Real-World Relevance
While full ORM frameworks like Hibernate/JPA excel at complex object-graph mutations, high-performance transactional microservices, reporting pipelines, and CQRS read-models benefit significantly from executing raw, highly optimized SQL. `JdbcClient` provides native SQL performance with zero ORM overhead, clean type mapping into Java records, and total protection against SQL injection.

---

## 2. Core Concepts

### JdbcClient
A fluent, thread-safe abstraction over `NamedParameterJdbcTemplate` and `JdbcTemplate`. It provides step-by-step method chaining: specifying SQL, binding parameters, and specifying return types or custom row mappers.

### DataAccessException Hierarchy
Spring wraps vendor-specific checked `SQLException`s into an unchecked, descriptive runtime exception hierarchy rooted at `org.springframework.dao.DataAccessException` (e.g., `DuplicateKeyException`, `DataIntegrityViolationException`, `CannotAcquireLockException`).

### Automatic Record Mapping
`JdbcClient` detects Java 21 `record` classes and maps SQL columns to record constructor components by matching column names to record component names (case-insensitively, handling `snake_case` to `camelCase` conversions).

### KeyHolder
A utility used during `INSERT` statements to retrieve auto-generated primary keys (e.g., `SERIAL`, `IDENTITY`, or database sequences) without requiring secondary `SELECT` statements.

---

## 3. How It Works Internally

```
+------------------------------------------------------------------------------------+
|                             JdbcClient Architecture                                |
+------------------------------------------------------------------------------------+
|                                                                                    |
|  Caller Code                                                                       |
|       |                                                                            |
|       | jdbcClient.sql("SELECT ... WHERE id = :id").param("id", 42).query(User.class)
|       v                                                                            |
|  DefaultJdbcClient                                                                 |
|       | (Parses statement, resolves named parameters)                              |
|       v                                                                            |
|  NamedParameterJdbcTemplate / JdbcTemplate                                         |
|       |                                                                            |
|       +---> DataSourceUtils.getConnection(dataSource)                              |
|       |     (Fetches or binds connection to current Spring Transaction)            |
|       |                                                                            |
|       +---> PreparedStatementCreatorFactory                                        |
|       |     (Substitutes named params ':id' with JDBC '?' placeholders)            |
|       |                                                                            |
|       +---> PreparedStatement.executeQuery()                                       |
|       |                                                                            |
|       +---> DataClassRowMapper (Maps ResultSet columns into Java Record)           |
|       |                                                                            |
|       +---> SQLErrorCodeSQLExceptionTranslator                                     |
|             (Translates vendor SQL errors to DataAccessException if failure)       |
|                                                                                    |
+------------------------------------------------------------------------------------+
```

1. **Statement Parsing**: `JdbcClient` inspects the SQL string for named parameters (e.g., `:id`).
2. **Transaction Synchronization**: It requests a connection via `DataSourceUtils`, ensuring enlistment in any ambient `@Transactional` context.
3. **Execution**: It compiles the query into a JDBC `PreparedStatement` and sets bound parameters.
4. **Mapping**: `DataClassRowMapper` (or `SingleColumnRowMapper`) constructs the target record or primitive type using reflection over constructor parameters.
5. **Resource Teardown**: Statements and result sets are guaranteed closed in `finally` blocks, and connections are released back to the HikariCP pool.

---

## 4. Syntax & Usage

### Injecting and Instantiating JdbcClient
```java
@Configuration
public class DatabaseConfig {

    @Bean
    public JdbcClient jdbcClient(DataSource dataSource) {
        return JdbcClient.create(dataSource);
    }
}
```

### Fluent Query Patterns
```java
// Query for a single mapped record
Optional<Customer> customer = jdbcClient.sql("SELECT id, name, email FROM customers WHERE id = :id")
    .param("id", customerId)
    .query(Customer.class)
    .optional();

// Query for a list
List<Customer> list = jdbcClient.sql("SELECT id, name, email FROM customers WHERE active = :active")
    .param("active", true)
    .query(Customer.class)
    .list();

// Insert with Generated Key
GeneratedKeyHolder keyHolder = new GeneratedKeyHolder();
jdbcClient.sql("INSERT INTO customers (name, email) VALUES (:name, :email)")
    .param("name", "Alice")
    .param("email", "alice@example.com")
    .update(keyHolder, "id");
```

---

## 5. Practical Examples

### 5.1 Modern Querying with JdbcClient and Java 21 Records
```java
package com.example.db.repository;

import org.springframework.jdbc.core.simple.JdbcClient;
import org.springframework.stereotype.Repository;

import java.math.BigDecimal;
import java.time.Instant;
import java.util.List;
import java.util.Optional;

@Repository
public class ProductJdbcRepository {

    private final JdbcClient jdbcClient;

    public ProductJdbcRepository(JdbcClient jdbcClient) {
        this.jdbcClient = jdbcClient;
    }

    public record ProductRecord(
        Long id,
        String sku,
        String name,
        BigDecimal price,
        Instant createdAt
    ) {}

    public Optional<ProductRecord> findBySku(String sku) {
        return jdbcClient.sql("""
                SELECT id, sku, name, price, created_at
                FROM products
                WHERE sku = :sku
                """)
            .param("sku", sku)
            .query(ProductRecord.class)
            .optional();
    }

    public List<ProductRecord> findProductsCheaperThan(BigDecimal maxPrice) {
        return jdbcClient.sql("""
                SELECT id, sku, name, price, created_at
                FROM products
                WHERE price <= :maxPrice
                ORDER BY price ASC
                """)
            .param("maxPrice", maxPrice)
            .query(ProductRecord.class)
            .list();
    }
}
```

### 5.2 Enterprise Parameter Binding and Batch Updates
```java
package com.example.db.repository;

import org.springframework.jdbc.core.simple.JdbcClient;
import org.springframework.jdbc.support.GeneratedKeyHolder;
import org.springframework.stereotype.Repository;
import org.springframework.transaction.annotation.Transactional;

import java.util.List;

@Repository
public class OrderRepository {

    private final JdbcClient jdbcClient;

    public OrderRepository(JdbcClient jdbcClient) {
        this.jdbcClient = jdbcClient;
    }

    public record OrderItem(String sku, int quantity, double unitPrice) {}
    public record CreateOrderCommand(Long customerId, List<OrderItem> items) {}

    @Transactional
    public Long createOrder(CreateOrderCommand command) {
        GeneratedKeyHolder keyHolder = new GeneratedKeyHolder();

        // 1. Insert parent order
        jdbcClient.sql("""
                INSERT INTO orders (customer_id, status, created_at)
                VALUES (:customerId, 'CREATED', NOW())
                """)
            .param("customerId", command.customerId())
            .update(keyHolder, "id");

        Number generatedId = keyHolder.getKey();
        if (generatedId == null) {
            throw new IllegalStateException("Failed to obtain generated order ID");
        }
        Long orderId = generatedId.longValue();

        // 2. Insert line items
        for (OrderItem item : command.items()) {
            jdbcClient.sql("""
                    INSERT INTO order_items (order_id, sku, quantity, unit_price)
                    VALUES (:orderId, :sku, :quantity, :unitPrice)
                    """)
                .param("orderId", orderId)
                .param("sku", item.sku())
                .param("quantity", item.quantity())
                .param("unitPrice", item.unitPrice())
                .update();
        }

        return orderId;
    }
}
```

### 5.3 Edge Case: Dynamic Filtering and IN-Clause Expansion
```java
package com.example.db.repository;

import org.springframework.jdbc.core.simple.JdbcClient;
import org.springframework.stereotype.Repository;

import java.util.Collections;
import java.util.List;

@Repository
public class AccountSearchRepository {

    private final JdbcClient jdbcClient;

    public AccountSearchRepository(JdbcClient jdbcClient) {
        this.jdbcClient = jdbcClient;
    }

    public record AccountSummary(Long id, String accountType, String status) {}

    public List<AccountSummary> findAccountsInStatuses(List<String> statuses) {
        if (statuses == null || statuses.isEmpty()) {
            return Collections.emptyList();
        }

        // JdbcClient automatically expands Collections passed to named parameters into SQL IN (?, ?, ...)
        return jdbcClient.sql("""
                SELECT id, account_type, status
                FROM accounts
                WHERE status IN (:statuses)
                ORDER BY id ASC
                """)
            .param("statuses", statuses)
            .query(AccountSummary.class)
            .list();
    }
}
```

### 5.4 Anti-Pattern: String Concatenation and SQL Injection
```java
// =========================================================================
// ANTI-PATTERN: Dynamic SQL via String Concatenation (SQL Injection Vulnerability)
// =========================================================================
@Repository
public class VulnerableUserRepository {
    private final JdbcClient jdbcClient;

    public VulnerableUserRepository(JdbcClient jdbcClient) {
        this.jdbcClient = jdbcClient;
    }

    public List<UserRecord> searchUsers(String userInput) {
        // CATASTROPHIC SECURITY BUG: An attacker can input "' OR '1'='1" or DROP TABLE!
        String sql = "SELECT id, username FROM users WHERE username = '" + userInput + "'";
        return jdbcClient.sql(sql)
            .query(UserRecord.class)
            .list();
    }
}

// =========================================================================
// PRODUCTION FIX: Parameterized SQL with JdbcClient Named Bindings
// =========================================================================
@Repository
public class SecureUserRepository {
    private final JdbcClient jdbcClient;

    public SecureUserRepository(JdbcClient jdbcClient) {
        this.jdbcClient = jdbcClient;
    }

    public List<UserRecord> searchUsers(String userInput) {
        // SECURE: Database engine compiles statement and treats parameter strictly as literal data
        return jdbcClient.sql("SELECT id, username FROM users WHERE username = :username")
            .param("username", userInput)
            .query(UserRecord.class)
            .list();
    }
}
```

### 5.5 High-Throughput Streaming with KeyHolder and Stream Queries
```java
package com.example.db.service;

import org.springframework.jdbc.core.simple.JdbcClient;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.io.Writer;
import java.util.stream.Stream;

@Service
public class ReportExportService {

    private final JdbcClient jdbcClient;

    public ReportExportService(JdbcClient jdbcClient) {
        this.jdbcClient = jdbcClient;
    }

    public record AuditLogRow(Long id, String action, String principal, String timestamp) {}

    /**
     * Streams 1,000,000+ rows directly from cursor to output writer without loading into memory.
     * Note: Requires active transaction to keep ResultSet cursor open.
     */
    @Transactional(readOnly = true)
    public void exportAuditLogsToCsv(Writer writer) {
        try (Stream<AuditLogRow> stream = jdbcClient.sql("""
                SELECT id, action, principal, log_timestamp AS timestamp
                FROM audit_logs
                ORDER BY id ASC
                """)
            .query(AuditLogRow.class)
            .stream()) {

            stream.forEach(row -> {
                try {
                    writer.write(String.format("%d,%s,%s,%s%n",
                        row.id(), row.action(), row.principal(), row.timestamp()));
                } catch (Exception e) {
                    throw new RuntimeException("Stream write error", e);
                }
            });
        }
    }
}
```

---

## 6. Best Practices

1. **Prefer `JdbcClient` over `JdbcTemplate` for New Code**: Use the modern fluent API for cleaner syntax, automatic record mapping, and unified parameter handling.
2. **Always Use Named Parameters**: Prefer `:paramName` over positional `?` placeholders. It prevents positional offset bugs when modifying SQL queries.
3. **Map Directly to Java 21 Records**: Leverage constructor reflection with immutable records to eliminate manual `RowMapper` implementations.
4. **Always Close Result Streams Inside `try-with-resources`**: When using `.stream()`, the underlying database cursor remains open until the stream closes. Always encapsulate stream consumption in `try (Stream<T> s = ...)`.
5. **Mark Read-Only Queries with `@Transactional(readOnly = true)`**: This hints the underlying database driver to optimize connection buffers and avoids unnecessary dirty-checking overhead.
6. **Use KeyHolder Safely**: When retrieving auto-generated keys, verify column casing (e.g., PostgreSQL requires lower-case `"id"`, while Oracle typically returns upper-case `"ID"`).

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **String Concatenation in SQL** | Opens critical SQL injection vulnerabilities and breaks database PreparedStatement caching. | Always use `:param` bindings or positional `?` parameters. |
| **Forgetting `@Transactional` with `.stream()`** | As soon as the query executes, the connection closes, causing `SqlFeatureNotSupportedException` or closed ResultSet errors. | Enclose `.stream()` operations in `@Transactional(readOnly = true)`. |
| **Record Property / Column Name Mismatch** | If the SQL query aliases do not match record component names, mapping sets fields to `null`. | Use SQL column aliases (`AS my_column`) matching record components. |
| **Using `single()` When No Rows Exist** | `.single()` throws `EmptyResultDataAccessException` if zero rows are returned. | Use `.optional()` to safely handle zero-or-one row semantics. |
| **Using `single()` When Multiple Rows Exist** | `.single()` throws `IncorrectResultSizeDataAccessException` if more than one row returns. | Use `.list()` or add `LIMIT 1` to the SQL query. |
| **Executing Heavy Batch Updates Loop-by-Loop** | Running individual `.update()` calls in a loop causes high network round-trip latency. | Use `batchUpdate()` on underlying `NamedParameterJdbcTemplate` for bulk inserts. |

---

## 8. Performance & Security Notes

### Performance
- **Zero Reflection Caching Overhead**: `DataClassRowMapper` caches record constructor handles using `MethodHandle`, resulting in near-native object instantiation speeds.
- **Statement Caching**: Parameterized queries allow PostgreSQL and MySQL to cache compiled query execution plans, dramatically reducing parsing CPU cycles.

### Security
- **OWASP Top 10 Mitigation (A03: Injection)**: `JdbcClient` automatically translates parameters into prepared statement variables, completely isolating input data from query parsing logic.
- **Principle of Least Privilege**: Ensure the database user used by the Spring Boot application possesses only `SELECT`, `INSERT`, `UPDATE`, `DELETE` grants, without administrative `DROP` or `ALTER` capabilities.

---

## 9. Comparison / When to Use What

| Feature | JdbcClient | JdbcTemplate | Spring Data JPA |
| :--- | :--- | :--- | :--- |
| **API Style** | Fluent chained calls | Overloaded callback methods | Declarative Repositories / ORM |
| **Parameter Binding** | Positional & Named unified | Positional only (Named requires sub-class) | Derived & JPQL parameters |
| **Record Mapping** | Automatic (Reflection/Constructors) | Requires manual `DataClassRowMapper` | Full Entity lifecycle (Proxy-based) |
| **Startup / Memory Overhead** | Ultra-low (Zero entity model parsing) | Ultra-low | High (Metamodel, bytecode enhancement) |
| **Best For** | High-throughput SQL, microservices, CQRS | Legacy code maintenance | Complex domain graphs, enterprise CRUD |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is <code>JdbcClient</code> and how does it improve upon <code>JdbcTemplate</code>?</strong></summary>

Introduced in Spring Framework 6.1 (Spring Boot 3.2), `JdbcClient` is a fluent synchronous client that wraps both `JdbcTemplate` and `NamedParameterJdbcTemplate`. It replaces hundreds of overloaded template methods with a readable chain (`sql().param().query()`), unifies positional and named parameters, and natively maps results to Java records without requiring explicit `RowMapper` boilerplate.
</details>

<details>
<summary><strong>2. How does <code>JdbcClient</code> map query results directly into Java 21 records?</strong></summary>

Under the hood, `JdbcClient` uses `DataClassRowMapper`. It inspects the record's canonical constructor, maps database column names (converting `snake_case` to `camelCase`) to constructor parameter names, extracts typed values from the `ResultSet`, and invokes the constructor using optimized reflection.
</details>

<details>
<summary><strong>3. Why does consuming a query <code>.stream()</code> require an enclosing transaction?</strong></summary>

In JDBC, streaming rows via a cursor requires the underlying `Connection` and `ResultSet` to remain open during iteration. Outside an active `@Transactional` boundary, Spring closes the connection immediately after the query method returns, causing downstream stream operations to fail with a "Connection/ResultSet is closed" error.
</details>

<details>
<summary><strong>4. How does Spring's <code>DataAccessException</code> hierarchy benefit application developers?</strong></summary>

`DataAccessException` is an unchecked runtime exception hierarchy that abstracts away proprietary vendor error codes. Whether using PostgreSQL, Oracle, or MySQL, foreign key violations translate to `DataIntegrityViolationException`, and unique constraint violations translate to `DuplicateKeyException`, decoupling business code from database-specific drivers.
</details>

<details>
<summary><strong>5. How does <code>JdbcClient</code> handle SQL <code>IN</code> clauses with dynamic lists?</strong></summary>

When a `Collection` is passed to a named parameter (`.param("ids", List.of(1, 2, 3))`), `JdbcClient` automatically expands the named parameter into the corresponding number of JDBC positional placeholders (`IN (?, ?, ?)`), eliminating manual string concatenation.
</details>

<details>
<summary><strong>6. What is the difference between <code>.single()</code> and <code>.optional()</code> in <code>JdbcClient</code>?</strong></summary>

`.single()` expects exactly one row; if zero rows are returned, it throws `EmptyResultDataAccessException`, and if more than one row returns, it throws `IncorrectResultSizeDataAccessException`. `.optional()` returns `Optional.empty()` when zero rows are found, making it safer for nullable lookups.
</details>

<details>
<summary><strong>7. How do you retrieve auto-generated database keys using <code>JdbcClient</code>?</strong></summary>

You instantiate a `GeneratedKeyHolder` and pass it to the `.update(keyHolder, "id")` method along with the name of the primary key column. After execution, `keyHolder.getKey()` yields the generated identifier.
</details>

<details>
<summary><strong>8. Is <code>JdbcClient</code> thread-safe?</strong></summary>

Yes, `JdbcClient` instances are thread-safe and can be injected as singleton Spring beans. Each query execution creates its own statement and result set scope bound to the calling thread's transaction context.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Basic Record Query with JdbcClient</strong></summary>

**Task**: Create a record `UserDto(Long id, String username)` and a repository method using `JdbcClient` to find a user by ID returning `Optional<UserDto>`.

```java
public record UserDto(Long id, String username) {}

public Optional<UserDto> findById(Long id) {
    return jdbcClient.sql("SELECT id, username FROM users WHERE id = :id")
        .param("id", id)
        .query(UserDto.class)
        .optional();
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Insert and Row Count Assertion</strong></summary>

**Task**: Write a repository method that inserts a new role `(name, description)` and returns `true` if exactly one row was affected.

```java
public boolean insertRole(String name, String description) {
    int rows = jdbcClient.sql("INSERT INTO roles (name, description) VALUES (:name, :description)")
        .param("name", name)
        .param("description", description)
        .update();
    return rows == 1;
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Generated Key Retrieval</strong></summary>

**Task**: Implement a method that inserts a department and returns its generated `Long` ID using `GeneratedKeyHolder`.

```java
public Long insertDepartment(String title) {
    GeneratedKeyHolder keyHolder = new GeneratedKeyHolder();
    jdbcClient.sql("INSERT INTO departments (title) VALUES (:title)")
        .param("title", title)
        .update(keyHolder, "id");
    return keyHolder.getKey().longValue();
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Custom RowMapper Implementation</strong></summary>

**Task**: When database columns require complex transformation (e.g., parsing a comma-separated permissions string into `List<String>`), provide a custom lambda `RowMapper` to `JdbcClient`.

```java
public record UserWithPermissions(Long id, String username, List<String> permissions) {}

public List<UserWithPermissions> findAllWithPermissions() {
    return jdbcClient.sql("SELECT id, username, permissions_csv FROM users")
        .query((rs, rowNum) -> new UserWithPermissions(
            rs.getLong("id"),
            rs.getString("username"),
            Arrays.asList(rs.getString("permissions_csv").split(","))
        ))
        .list();
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Dynamic Multi-Filter Query Construction</strong></summary>

**Task**: Implement a repository search method that conditionally appends filters for `status`, `minSalary`, and `departmentId` using `JdbcClient` without SQL injection.

```java
public List<EmployeeRecord> searchEmployees(String status, BigDecimal minSalary, Long deptId) {
    StringBuilder sql = new StringBuilder("SELECT id, name, salary, status, dept_id FROM employees WHERE 1=1 ");
    Map<String, Object> params = new HashMap<>();

    if (status != null) {
        sql.append("AND status = :status ");
        params.put("status", status);
    }
    if (minSalary != null) {
        sql.append("AND salary >= :minSalary ");
        params.put("minSalary", minSalary);
    }
    if (deptId != null) {
        sql.append("AND dept_id = :deptId ");
        params.put("deptId", deptId);
    }

    var spec = jdbcClient.sql(sql.toString());
    params.forEach(spec::param);
    return spec.query(EmployeeRecord.class).list();
}
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Large Dataset Streaming to JSON Lines</strong></summary>

**Task**: Write a transactional service method that streams 100,000 order records from `JdbcClient` and flushes them to an `OutputStream` in NDJSON format without exceeding heap memory.

```java
@Transactional(readOnly = true)
public void streamOrdersAsNdjson(OutputStream os, ObjectMapper objectMapper) {
    try (Stream<OrderRecord> stream = jdbcClient.sql("SELECT id, total, status FROM orders")
            .query(OrderRecord.class)
            .stream();
         BufferedWriter writer = new BufferedWriter(new OutputStreamWriter(os, StandardCharsets.UTF_8))) {

        stream.forEach(order -> {
            try {
                writer.write(objectMapper.writeValueAsString(order));
                writer.newLine();
            } catch (IOException e) {
                throw new UncheckedIOException(e);
            }
        });
        writer.flush();
    } catch (IOException e) {
        throw new UncheckedIOException(e);
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------------+
| Instantiation:  JdbcClient.create(dataSource)                                         |
| Query Single:   jdbcClient.sql(query).param("k", v).query(MyRecord.class).optional() |
| Query List:     jdbcClient.sql(query).param("k", v).query(MyRecord.class).list()     |
| Query Stream:   jdbcClient.sql(query).query(MyRecord.class).stream() [@Transactional] |
| Update/Delete:  jdbcClient.sql(query).param("k", v).update()                          |
| Insert Key:     jdbcClient.sql(query).update(keyHolder, "id")                         |
| Collections:    .param("ids", List.of(1, 2, 3)) expands automatically to IN (?, ?, ?) |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Spring Framework 6.x JdbcClient Reference Documentation](https://docs.spring.io/spring-framework/reference/data-access/jdbc/core.html#jdbc-JdbcClient)
- [Spring Data Access Exception Hierarchy](https://docs.spring.io/spring-framework/reference/data-access/dao.html)
- [Java 21 Records and Modern Data Modeling](https://openjdk.org/jeps/395)
- [PostgreSQL Performance Tuning for JDBC Drivers](https://jdbc.postgresql.org/documentation/query/)
