# jOOQ & MyBatis in Spring Boot: Type-Safe SQL & SQL Mapping Alternatives

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate / Advanced |
| **Prerequisites** | `01-jdbc-template-and-jdbc-client.md`, SQL Complex Joins & Analytic Functions |
| **Estimated Reading Time** | 29 minutes |
| **Status** | Active |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework** | 6.2.x / 7.0.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven / Gradle |
| **Tested With** | jOOQ 3.19.x, MyBatis 3.5.x / MyBatis-Spring-Boot 3.0.x, PostgreSQL 16 |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
   - [5.1 Type-Safe SQL Querying with jOOQ DSLContext](#51-type-safe-sql-querying-with-jooq-dslcontext)
   - [5.2 Dynamic Complex Queries with jOOQ Conditions](#52-dynamic-complex-queries-with-jooq-conditions)
   - [5.3 MyBatis Mapper Interface with Dynamic XML SQL](#53-mybatis-mapper-interface-with-dynamic-xml-sql)
   - [5.4 Anti-Pattern: String-Based SQL vs Type-Safe jOOQ Refactoring](#54-anti-pattern-string-based-sql-vs-type-safe-jooq-refactoring)
   - [5.5 jOOQ and Spring Boot Transaction Integration](#55-jooq-and-spring-boot-transaction-integration)
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
While full ORM frameworks like Hibernate/JPA excel at transactional business domain models, they struggle with data-oriented tasks: complex analytical reporting, multi-table aggregations, window functions, common table expressions (CTEs), and bulk set operations. Writing native SQL strings inside JPA `@Query` loses compile-time type safety: renaming a database column in SQL breaks code at runtime instead of compile time.

### Historical Evolution
To bridge the gap between raw SQL and Java code, two distinct philosophies emerged:
1. **jOOQ (Java Object Oriented Querying)**: Generates Java code directly from your database schema. Queries are constructed using a fluent, strongly typed Java DSL that mimics SQL syntax 1:1, guaranteeing that any schema change produces a compile error immediately.
2. **MyBatis (formerly iBATIS)**: A lightweight SQL mapper framework that separates SQL statements into external XML templates or annotations while mapping parameters and result sets into POJOs.

### Real-World Relevance
For data-intensive microservices, reporting engines, financial transaction reconciliations, and CQRS read architectures, jOOQ and MyBatis offer native SQL performance, total control over generated SQL, and zero ORM caching or proxy overhead.

---

## 2. Core Concepts

### jOOQ: Database-First Type Safety
jOOQ reads your live database schema (or Flyway migration scripts) during the build and generates Java classes representing tables, columns, records, keys, and stored procedures:
- `DSLContext`: The primary entry point for constructing and executing jOOQ queries.
- Type-Safe DSL: `create.select(USERS.ID, USERS.EMAIL).from(USERS)...` ensures typos in table or column names fail at compile time.
- Direct Record Mapping: Maps result sets directly into generated records, custom POJOs, or Java 21 `record` classes.

### MyBatis: SQL Mapping & Dynamic Templates
MyBatis keeps raw SQL intact but provides a templating engine:
- `@Mapper`: Interface declaring data access methods.
- Dynamic SQL Tags: `<if>`, `<choose>`, `<when>`, `<otherwise>`, `<foreach>`, and `<trim>` allow declarative conditional SQL construction.
- Result Maps: XML configurations that map complex relational joins to nested Java object trees.

---

## 3. How It Works Internally

```
+------------------------------------------------------------------------------------+
|                         jOOQ Code Generation & Query Pipeline                      |
+------------------------------------------------------------------------------------+
|                                                                                    |
|  Build Time:                                                                       |
|  PostgreSQL / Flyway Scripts ---> jOOQ Codegen Maven Plugin ---> Generated Java Classes
|                                                                  (Tables, Records, Keys)
|                                                                                    |
|  Runtime Execution:                                                                |
|  DSLContext.select(...)                                                            |
|       |                                                                            |
|       v                                                                            |
|  jOOQ Query AST Compilation                                                        |
|  - Validates types, table aliases, and parameters                                  |
|  - Renders dialect-specific SQL string                                             |
|       |                                                                            |
|       v                                                                            |
|  SpringTransactionProvider / DataSourceConnectionProvider                          |
|  - Borrows Connection from active Spring @Transactional context                    |
|       |                                                                            |
|       v                                                                            |
|  PreparedStatement.executeQuery() -> Mapped to Java 21 Records                     |
|                                                                                    |
+------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### Maven Dependency for jOOQ
```xml
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-starter-jooq</artifactId>
</dependency>
```

### Basic jOOQ Query
```java
@Repository
public class UserJooqRepository {

    private final DSLContext dsl;

    public UserJooqRepository(DSLContext dsl) {
        this.dsl = dsl;
    }

    public List<UserRecord> findActiveUsers() {
        return dsl.selectFrom(USERS)
            .where(USERS.STATUS.eq("ACTIVE"))
            .fetchInto(UserRecord.class);
    }
}
```

---

## 5. Practical Examples

### 5.1 Type-Safe SQL Querying with jOOQ DSLContext
```java
package com.example.jooq.repository;

import org.jooq.DSLContext;
import org.springframework.stereotype.Repository;

import java.math.BigDecimal;
import java.util.List;

// Imports statically generated jOOQ table metadata
import static com.example.jooq.generated.Tables.ORDERS;
import static com.example.jooq.generated.Tables.CUSTOMERS;

@Repository
public class OrderAnalyticsRepository {

    private final DSLContext dsl;

    public OrderAnalyticsRepository(DSLContext dsl) {
        this.dsl = dsl;
    }

    public record CustomerSpendingDto(
        Long customerId,
        String email,
        BigDecimal totalSpent,
        int orderCount
    ) {}

    public List<CustomerSpendingDto> getTopSpendingCustomers(BigDecimal minSpend) {
        return dsl.select(
                CUSTOMERS.ID,
                CUSTOMERS.EMAIL,
                org.jooq.impl.DSL.sum(ORDERS.TOTAL_AMOUNT).as("totalSpent"),
                org.jooq.impl.DSL.count(ORDERS.ID).as("orderCount")
            )
            .from(CUSTOMERS)
            .join(ORDERS).on(ORDERS.CUSTOMER_ID.eq(CUSTOMERS.ID))
            .groupBy(CUSTOMERS.ID, CUSTOMERS.EMAIL)
            .having(org.jooq.impl.DSL.sum(ORDERS.TOTAL_AMOUNT).greaterOrEqual(minSpend))
            .orderBy(org.jooq.impl.DSL.sum(ORDERS.TOTAL_AMOUNT).desc())
            .limit(10)
            .fetchInto(CustomerSpendingDto.class); // Directly maps into Java 21 record!
    }
}
```

### 5.2 Dynamic Complex Queries with jOOQ Conditions
```java
package com.example.jooq.repository;

import org.jooq.Condition;
import org.jooq.DSLContext;
import org.jooq.impl.DSL;
import org.springframework.stereotype.Service;

import java.math.BigDecimal;
import java.util.List;

import static com.example.jooq.generated.Tables.PRODUCTS;

@Service
public class ProductSearchService {

    private final DSLContext dsl;

    public ProductSearchService(DSLContext dsl) {
        this.dsl = dsl;
    }

    public record SearchCriteria(String keyword, BigDecimal minPrice, BigDecimal maxPrice, String category) {}
    public record ProductDto(Long id, String name, BigDecimal price, String category) {}

    public List<ProductDto> searchProducts(SearchCriteria criteria) {
        // Start with an empty, neutral condition (WHERE 1=1)
        Condition condition = DSL.noCondition();

        if (criteria.keyword() != null && !criteria.keyword().isBlank()) {
            condition = condition.and(PRODUCTS.NAME.containsIgnoreCase(criteria.keyword()));
        }
        if (criteria.minPrice() != null) {
            condition = condition.and(PRODUCTS.PRICE.greaterOrEqual(criteria.minPrice()));
        }
        if (criteria.maxPrice() != null) {
            condition = condition.and(PRODUCTS.PRICE.lessOrEqual(criteria.maxPrice()));
        }
        if (criteria.category() != null) {
            condition = condition.and(PRODUCTS.CATEGORY.eq(criteria.category()));
        }

        return dsl.select(PRODUCTS.ID, PRODUCTS.NAME, PRODUCTS.PRICE, PRODUCTS.CATEGORY)
            .from(PRODUCTS)
            .where(condition)
            .orderBy(PRODUCTS.PRICE.asc())
            .fetchInto(ProductDto.class);
    }
}
```

### 5.3 MyBatis Mapper Interface with Dynamic XML SQL
```java
package com.example.mybatis.mapper;

import org.apache.ibatis.annotations.Mapper;
import org.apache.ibatis.annotations.Param;

import java.util.List;

@Mapper
public interface UserMapper {

    record UserEntity(Long id, String username, String email, String status) {}

    // Method bound to external XML mapping definition
    List<UserEntity> searchUsersDynamic(
        @Param("username") String username,
        @Param("status") String status,
        @Param("roles") List<String> roles
    );
}
```

```xml
<?xml version="1.0" encoding="UTF-8" ?>
<!DOCTYPE mapper PUBLIC "-//mybatis.org//DTD Mapper 3.0//EN" 
  "http://mybatis.org/dtd/mybatis-3-mapper.dtd">

<mapper namespace="com.example.mybatis.mapper.UserMapper">

    <select id="searchUsersDynamic" resultType="com.example.mybatis.mapper.UserMapper$UserEntity">
        SELECT u.id, u.username, u.email, u.status
        FROM users u
        <where>
            <if test="username != null and username != ''">
                AND u.username LIKE CONCAT('%', #{username}, '%')
            </if>
            <if test="status != null">
                AND u.status = #{status}
            </if>
            <if test="roles != null and roles.size() > 0">
                AND u.role IN
                <foreach item="role" collection="roles" open="(" separator="," close=")">
                    #{role}
                </foreach>
            </if>
        </where>
        ORDER BY u.id DESC
    </select>

</mapper>
```

### 5.4 Anti-Pattern: String-Based SQL vs Type-Safe jOOQ Refactoring
```java
// =========================================================================
// ANTI-PATTERN: Raw String SQL in Custom Repositories (Brittle & Untyped)
// =========================================================================
@Repository
public class FragileReportRepository {
    @Autowired private JdbcClient jdbcClient;

    public List<SalesReport> generateReport() {
        // DANGEROUS: If a DBA renames 'ord_total' to 'total_amount',
        // Java code compiles cleanly! Fails in PRODUCTION at 3 AM!
        String sql = "SELECT cust_id, SUM(ord_total) AS revenue FROM orders GROUP BY cust_id";
        return jdbcClient.sql(sql).query(SalesReport.class).list();
    }
}

// =========================================================================
// PRODUCTION FIX: Type-Safe jOOQ Compilation Enforcement
// =========================================================================
@Repository
public class TypeSafeReportRepository {
    @Autowired private DSLContext dsl;

    public List<SalesReport> generateReport() {
        // SAFE: If column is renamed, Maven build FAILS IMMEDIATELY!
        // ORDERS.TOTAL_AMOUNT is a generated Java field!
        return dsl.select(ORDERS.CUSTOMER_ID, DSL.sum(ORDERS.TOTAL_AMOUNT).as("revenue"))
            .from(ORDERS)
            .groupBy(ORDERS.CUSTOMER_ID)
            .fetchInto(SalesReport.class);
    }
}
```

### 5.5 jOOQ and Spring Boot Transaction Integration
```java
package com.example.jooq.service;

import org.jooq.DSLContext;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.math.BigDecimal;

import static com.example.jooq.generated.Tables.ACCOUNTS;

@Service
public class TransferService {

    private final DSLContext dsl;

    public TransferService(DSLContext dsl) {
        this.dsl = dsl;
    }

    /**
     * Spring Boot's 'spring-boot-starter-jooq' automatically binds jOOQ's DSLContext
     * to Spring's PlatformTransactionManager via SpringTransactionProvider!
     * @Transactional coordinates jOOQ queries with 100% ACID consistency.
     */
    @Transactional
    public void transferFunds(Long fromId, Long toId, BigDecimal amount) {
        // Deduct from source
        dsl.update(ACCOUNTS)
            .set(ACCOUNTS.BALANCE, ACCOUNTS.BALANCE.minus(amount))
            .where(ACCOUNTS.ID.eq(fromId))
            .execute();

        // Credit to target
        dsl.update(ACCOUNTS)
            .set(ACCOUNTS.BALANCE, ACCOUNTS.BALANCE.plus(amount))
            .where(ACCOUNTS.ID.eq(toId))
            .execute();
    }
}
```

---

## 6. Best Practices

1. **Use jOOQ for Read-Heavy & Complex SQL Querying**: Combine Spring Data JPA for simple entity CRUD/mutations with jOOQ for complex multi-table analytical reporting (CQRS).
2. **Automate jOOQ Code Generation in Maven Build**: Integrate `jooq-codegen-maven` to run after Flyway migrations during the build phase (`generate-sources`), ensuring classes always match the schema.
3. **Map Directly to Java 21 Records**: Use jOOQ's `.fetchInto(MyRecord.class)` to deserialize query results directly into immutable record components.
4. **Leverage Spring Boot Auto-Configuration**: Use `spring-boot-starter-jooq`. Spring automatically configures the `DSLContext`, `ConnectionProvider`, and binds jOOQ to ambient `@Transactional` boundaries.
5. **Use MyBatis When Teams Prefer Raw SQL / DBA-Managed XML**: If DBAs write and optimize raw SQL templates independently of Java code, MyBatis XML mappers provide clear separation of concerns.
6. **Prefer Dynamic jOOQ Conditions Over MyBatis XML**: Composing type-safe `Condition` objects in Java is much less error-prone than debugging XML `<choose>` / `<if>` tags.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Manual Connection Management in jOOQ** | Instantiating `DSL.using(connection)` directly breaks Spring transaction synchronization. | Inject Spring-managed `DSLContext` bean. |
| **Committing Code with Stale jOOQ Classes** | If database schema evolves and `jooq-codegen` is not executed, runtime queries fail. | Bind code generator to Maven build lifecycle (`compile` phase). |
| **XML Typo in MyBatis Mappers** | Errors in XML `<select id="...">` or parameter property names are discovered only at runtime. | Write automated integration tests for every MyBatis mapper. |
| **Over-Using jOOQ for Simple Single-Entity CRUD** | Writing explicit `DSL.insertInto(...)` and `DSL.update(...)` for 50 entities creates boilerplate compared to `JpaRepository`. | Use Spring Data JPA or Spring Data JDBC for simple entity persistence. |
| **SQL Injection in MyBatis `${param}` Syntax** | Using `${param}` performs direct string substitution instead of prepared statement binding (`#{param}`). | Always use `#{param}` for parameter binding. |
| **Ignoring Dialect Settings** | jOOQ defaults to generic SQL if dialect is omitted, generating sub-optimal queries. | Ensure `spring.jooq.sql-dialect` matches your database (e.g. `Postgres`). |

---

## 8. Performance & Security Notes

### Performance
- **Zero ORM Overhead**: Neither jOOQ nor MyBatis maintains a persistence context, generates bytecode proxies, or performs dirty checking. Object mapping is direct and instantaneous.
- **Dialect-Specific Query Generation**: jOOQ generates native SQL optimized for your specific engine (e.g. utilizing PostgreSQL `RETURNING`, `LATERAL JOIN`, or window functions).

### Security
- **Type-Safe SQL Injection Defense**: jOOQ represents all user parameters as typed bind variables (`Param<T>`), completely eliminating SQL injection vulnerabilities.
- **MyBatis `#` vs `$**`: In MyBatis, `#{value}` uses prepared statements, while `${value}` performs raw string interpolation. Prohibit `${value}` in code review linters.

---

## 9. Comparison / When to Use What

| Criteria | jOOQ | MyBatis | Spring Data JPA | JdbcClient |
| :--- | :--- | :--- | :--- | :--- |
| **Type Safety** | **Maximum (Compile-Time)** | Low (XML / Annotations) | High (Entity Metamodel) | Low (SQL Strings) |
| **Code Generation** | **Required (From DB)** | None | None | None |
| **Dynamic SQL** | **Type-Safe Conditions** | Dynamic XML Tags | Specifications (Verbose) | Manual String Concatenation |
| **ORM / Proxy Overhead** | **Zero** | **Zero** | High (L1 Cache, Proxies) | **Zero** |
| **Best Used For** | Complex SQL, analytics, CQRS | DBA-driven raw SQL templates | Transactional domain models | Lightweight queries without codegen |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the fundamental value proposition of jOOQ over Spring Data JPA?</strong></summary>

jOOQ embraces SQL as a first-class citizen. Instead of trying to abstract SQL away into object-relational graphs, jOOQ generates strongly typed Java classes from your database schema. This allows developers to write complex SQL (joins, subqueries, CTEs, window functions) directly in Java with compile-time type safety: if a column is renamed or dropped, the code fails to compile immediately.
</details>

<details>
<summary><strong>2. How does jOOQ integrate with Spring Boot transactions?</strong></summary>

Spring Boot provides `spring-boot-starter-jooq`, which automatically configures a `SpringTransactionProvider` and `DataSourceConnectionProvider`. When a jOOQ query executes within a `@Transactional` method, jOOQ automatically acquires the physical connection bound to the current Spring transaction and participates in its commit/rollback lifecycle.
</details>

<details>
<summary><strong>3. What is the difference between <code>#{param}</code> and <code>${param}</code> in MyBatis?</strong></summary>

- `#{param}` tells MyBatis to create a JDBC `PreparedStatement` parameter (`?`) and safely bind the value, preventing SQL injection.
- `${param}` performs direct, unescaped string substitution into the SQL statement before compilation. It should only be used for dynamic table or column names, and introduces severe SQL injection risks if exposed to user input.
</details>

<details>
<summary><strong>4. Can jOOQ and Spring Data JPA be used together in the same application?</strong></summary>

Yes, this is a popular architectural pattern (often in CQRS). Spring Data JPA is used for the **Command / Write model** (business entities, state transitions, dirty checking), while jOOQ is used for the **Query / Read model** (complex reports, aggregations, paginated search screens) querying the same database tables.
</details>

<details>
<summary><strong>5. How does jOOQ handle mapping query results into Java 21 Records?</strong></summary>

jOOQ provides `.fetchInto(MyRecord.class)`. It inspects the record's constructor, matches query column names (or aliases) to record component names, extracts values from the JDBC result set, and instantiates the immutable record directly.
</details>

<details>
<summary><strong>6. What is the primary disadvantage of using jOOQ?</strong></summary>

jOOQ requires a code generation step during the build, requiring a running database or Flyway migration scripts during compilation. Additionally, while jOOQ's open-source edition supports open-source databases (PostgreSQL, MySQL, SQLite), supporting commercial databases (Oracle, SQL Server, DB2) requires a paid commercial license.
</details>

<details>
<summary><strong>7. How does MyBatis handle dynamic SQL?</strong></summary>

MyBatis provides XML tags (`<if>`, `<choose>`, `<when>`, `<otherwise>`, `<trim>`, `<where>`, `<foreach>`). When a query is invoked, the OGNL engine evaluates these tags against the input arguments and dynamically constructs the final SQL statement before binding parameters.
</details>

<details>
<summary><strong>8. Why is jOOQ's <code>Condition</code> superior to manual SQL string concatenation for dynamic filters?</strong></summary>

jOOQ's `Condition` objects represent abstract syntax tree (AST) nodes. They can be combined using `.and()` and `.or()` without worrying about dangling `AND` operators or misplaced parentheses. Furthermore, all parameters attached to conditions are automatically isolated as prepared statement binds.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Basic jOOQ Select Query</strong></summary>

**Task**: Write a repository method using `DSLContext` that selects all users with a status of `"ACTIVE"` ordered by `username`.

```java
public List<UserRecord> getActiveUsers(DSLContext dsl) {
    return dsl.selectFrom(USERS)
        .where(USERS.STATUS.eq("ACTIVE"))
        .orderBy(USERS.USERNAME.asc())
        .fetch();
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): MyBatis Annotation-Based Query</strong></summary>

**Task**: Declare a MyBatis mapper interface with an `@Select` annotation to find a user by ID.

```java
@Mapper
public interface UserMapper {
    @Select("SELECT id, username, email FROM users WHERE id = #{id}")
    UserDto findById(@Param("id") Long id);
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Dynamic jOOQ Search Query</strong></summary>

**Task**: Implement a jOOQ method that dynamically adds conditions for `minSalary` and `departmentId` if they are non-null.

```java
public List<EmployeeDto> searchEmployees(DSLContext dsl, BigDecimal minSalary, Long deptId) {
    Condition condition = DSL.noCondition();
    if (minSalary != null) condition = condition.and(EMPLOYEES.SALARY.ge(minSalary));
    if (deptId != null) condition = condition.and(EMPLOYEES.DEPT_ID.eq(deptId));

    return dsl.select(EMPLOYEES.ID, EMPLOYEES.NAME, EMPLOYEES.SALARY)
        .from(EMPLOYEES)
        .where(condition)
        .fetchInto(EmployeeDto.class);
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): MyBatis Dynamic Foreach IN-Clause</strong></summary>

**Task**: Write a MyBatis XML snippet that iterates over a `List<Long> ids` to generate an `IN (...)` clause.

```xml
<select id="findByIds" resultType="User">
    SELECT * FROM users WHERE id IN
    <foreach item="id" collection="ids" open="(" separator="," close=")">
        #{id}
    </foreach>
</select>
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Window Function and Ranking in jOOQ</strong></summary>

**Task**: Write a jOOQ query using `DSL.rowNumber().over()` to calculate employee salary ranks within each department.

```java
public List<RankedEmployee> getEmployeeRanks(DSLContext dsl) {
    return dsl.select(
            EMPLOYEES.NAME,
            EMPLOYEES.DEPT_ID,
            EMPLOYEES.SALARY,
            DSL.rowNumber().over(
                DSL.partitionBy(EMPLOYEES.DEPT_ID).orderBy(EMPLOYEES.SALARY.desc())
            ).as("salaryRank")
        )
        .from(EMPLOYEES)
        .fetchInto(RankedEmployee.class);
}
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): jOOQ Maven Codegen Configuration</strong></summary>

**Task**: Write the Maven XML plugin configuration snippet to configure `jooq-codegen-maven` to generate classes from a local PostgreSQL database into package `com.example.db.jooq`.

```xml
<plugin>
    <groupId>org.jooq</groupId>
    <artifactId>jooq-codegen-maven</artifactId>
    <configuration>
        <generator>
            <database>
                <name>org.jooq.meta.postgres.PostgresDatabase</name>
                <inputSchema>public</inputSchema>
            </database>
            <target>
                <packageName>com.example.db.jooq</packageName>
                <directory>target/generated-sources/jooq</directory>
            </target>
        </generator>
    </configuration>
</plugin>
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------------+
| jOOQ:         Type-safe SQL query builder generated from DB schema.                   |
| Best For:     Complex joins, window functions, analytics, CQRS read models.           |
| Syntax:       dsl.select(USERS.NAME).from(USERS).where(USERS.ID.eq(1)).fetchInto(...)  |
| MyBatis:      SQL mapper framework separating SQL into XML templates or annotations.  |
| Best For:     DBA-managed raw SQL queries with dynamic XML conditions.                |
| Injection:    MyBatis #{param} = safe bind; ${param} = unsafe raw string substitution!|
| Synergy:      Combine JPA (Write/Command) with jOOQ (Read/Query) for optimal CQRS!    |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [jOOQ Official Reference Documentation](https://www.jooq.org/doc/latest/manual/)
- [MyBatis-Spring-Boot-Starter Documentation](https://mybatis.org/spring-boot-starter/mybatis-spring-boot-autoconfigure/)
- [Vlad Mihalcea: Why You Should Use jOOQ with Spring Boot](https://vladmihalcea.com/why-you-should-use-jooq-with-spring/)
- [Spring Boot jOOQ Integration Guide](https://docs.spring.io/spring-boot/reference/data/sql.html#data.sql.jooq)
