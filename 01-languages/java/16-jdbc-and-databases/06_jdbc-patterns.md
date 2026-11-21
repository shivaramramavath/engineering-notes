# JDBC Architectural Patterns

> Production design patterns for clean persistence architecture: Repository, DAO, Unit of Work, Keyset Paging, and Transient Error Retry.

| Metadata | Details |
|---|---|
| **Level** | Core / Advanced |
| **Prerequisites** | JDBC Fundamentals, Java Generics, OOP Patterns |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **JDK Baseline** | Java 17 / 21 / 25 LTS |

---

## Table of Contents
1. [1. What & Why](#1-what--why)
2. [2. Core Concepts](#2-core-concepts)
3. [3. How It Works Internally: Data Access Layer Architecture](#3-how-it-works-internally-data-access-layer-architecture)
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
Writing raw JDBC statements directly within business services creates tight coupling, duplicated boilerplate, and leaky abstractions. When database access code is scattered across controllers or business components, changing a database column, switching dialect, or tuning transaction isolation requires refactoring hundreds of files.

Architectural patterns—including the **Data Access Object (DAO)**, **Repository Pattern**, **Unit of Work**, **RowMapper abstraction**, and **Keyset Cursor Paging**—isolate persistence logic into clean, testable, and reusable components. These patterns decouple business domain models from SQL relational representations and provide centralized locations for query optimization, retry handling, and metrics collection.

---

## 2. Core Concepts
- **Data Access Object (DAO)**: A structural pattern that encapsulates all access to a data source. Maps directly to relational database tables or views.
- **Repository Pattern**: A domain-driven design (DDD) pattern that provides an in-memory collection abstraction for accessing domain aggregates, decoupling domain entities from table schemas.
- **Unit of Work**: Maintains a list of business objects affected by a business transaction and coordinates writing out changes and resolving concurrency conflicts.
- **Template Method Pattern**: Eliminates repetitive connection acquisition, error translation, and resource closing boilerplate (e.g., Spring's `JdbcTemplate` or custom `JdbcExecutor`).
- **Keyset Pagination Pattern**: High-performance cursor-based pagination utilizing tuple comparison (`WHERE (created_at, id) < (:last_ts, :last_id)`).
- **Transient Error Retry**: Pattern that intercepts transient network or concurrency errors (e.g., deadlock `40P01`, serialization failure `40001`) and retries the operation with exponential backoff.

---

## 3. How It Works Internally: Data Access Layer Architecture

A clean enterprise persistence layer strictly separates concerns across architectural boundaries:

```
+-------------------------------------------------------+
|             Business Service Layer                    |
|       (OrderService, AccountTransferService)          |
+-------------------------------------------------------+
                           |
       Domain Aggregate Objects / Java Records
                           |
                           v
+-------------------------------------------------------+
|             Repository / DAO Layer                    |
|       (OrderRepository, AccountDao)                   |
|       - Encapsulates SQL statements                   |
|       - Executes parameter binding                    |
+-------------------------------------------------------+
                           |
       RowMapper / ResultExtractor Interfaces
                           |
                           v
+-------------------------------------------------------+
|             JDBC Execution Infrastructure             |
|       (HikariDataSource, Connection, PreparedStatement) |
+-------------------------------------------------------+
```

### Decoupling Data Flow:
1. **Service Layer**: Never imports `java.sql.*`. Operates purely on immutable domain records or entities.
2. **Repository Implementation**: Takes a `DataSource`, executes SQL via `PreparedStatement`, handles mapping through functional `RowMapper` lambdas, and translates checked `SQLException` into unchecked domain exceptions (e.g., `DataAccessException`, `EntityNotFoundException`).
3. **Connection Demarcation**: Transaction boundaries are coordinated either via contextual thread-local connection holders or explicit Unit of Work containers.

---

## 4. Syntax & Usage

### 4.1 Production Repository Implementation with Java 21 Records
```java
package com.example.jdbc.repository;

import java.math.BigDecimal;
import java.sql.*;
import java.time.Instant;
import java.util.*;
import javax.sql.DataSource;

public record Account(long id, String accountNumber, String holderName, BigDecimal balance, Instant createdAt) {}

public interface AccountRepository {
    Optional<Account> findById(long id);
    Optional<Account> findByAccountNumber(String accNumber);
    Account create(String accNumber, String holderName, BigDecimal initialBalance);
    boolean updateBalance(long id, BigDecimal newBalance);
}

public final class JdbcAccountRepository implements AccountRepository {
    private final DataSource dataSource;

    public JdbcAccountRepository(DataSource dataSource) {
        this.dataSource = Objects.requireNonNull(dataSource);
    }

    private static Account mapRow(ResultSet rs) throws SQLException {
        return new Account(
            rs.getLong("id"),
            rs.getString("account_number"),
            rs.getString("holder_name"),
            rs.getBigDecimal("balance"),
            rs.getTimestamp("created_at").toInstant()
        );
    }

    @Override
    public Optional<Account> findById(long id) {
        String sql = "SELECT id, account_number, holder_name, balance, created_at FROM accounts WHERE id = ?";
        try (Connection conn = dataSource.getConnection();
             PreparedStatement ps = conn.prepareStatement(sql)) {
            ps.setLong(1, id);
            try (ResultSet rs = ps.executeQuery()) {
                if (rs.next()) {
                    return Optional.of(mapRow(rs));
                }
                return Optional.empty();
            }
        } catch (SQLException ex) {
            throw new RuntimeException("Database failure finding account by ID: " + id, ex);
        }
    }

    @Override
    public Optional<Account> findByAccountNumber(String accNumber) {
        String sql = "SELECT id, account_number, holder_name, balance, created_at FROM accounts WHERE account_number = ?";
        try (Connection conn = dataSource.getConnection();
             PreparedStatement ps = conn.prepareStatement(sql)) {
            ps.setString(1, accNumber);
            try (ResultSet rs = ps.executeQuery()) {
                if (rs.next()) {
                    return Optional.of(mapRow(rs));
                }
                return Optional.empty();
            }
        } catch (SQLException ex) {
            throw new RuntimeException("Database failure finding account: " + accNumber, ex);
        }
    }

    @Override
    public Account create(String accNumber, String holderName, BigDecimal initialBalance) {
        String sql = "INSERT INTO accounts (account_number, holder_name, balance) VALUES (?, ?, ?) RETURNING id, created_at";
        try (Connection conn = dataSource.getConnection();
             PreparedStatement ps = conn.prepareStatement(sql)) {
            ps.setString(1, accNumber);
            ps.setString(2, holderName);
            ps.setBigDecimal(3, initialBalance);

            try (ResultSet rs = ps.executeQuery()) {
                if (rs.next()) {
                    long id = rs.getLong("id");
                    Instant created = rs.getTimestamp("created_at").toInstant();
                    return new Account(id, accNumber, holderName, initialBalance, created);
                }
                throw new SQLException("Failed to retrieve generated account fields");
            }
        } catch (SQLException ex) {
            throw new RuntimeException("Database error creating account: " + accNumber, ex);
        }
    }

    @Override
    public boolean updateBalance(long id, BigDecimal newBalance) {
        String sql = "UPDATE accounts SET balance = ? WHERE id = ?";
        try (Connection conn = dataSource.getConnection();
             PreparedStatement ps = conn.prepareStatement(sql)) {
            ps.setBigDecimal(1, newBalance);
            ps.setLong(2, id);
            return ps.executeUpdate() > 0;
        } catch (SQLException ex) {
            throw new RuntimeException("Database error updating account balance: " + id, ex);
        }
    }
}
```

---

## 5. Practical Examples

### Example 1: Basic Keyset Pagination Pattern Implementation
```java
package com.example.jdbc.repository;

import java.sql.*;
import java.time.Instant;
import java.util.*;
import javax.sql.DataSource;

public final class KeysetPaginationService {
    public record TransactionRecord(long id, long accountId, double amount, Instant txTimestamp) {}

    public static List<TransactionRecord> fetchNextPage(
            DataSource ds, long accountId, Instant lastTimestamp, Long lastId, int pageSize) throws SQLException {

        // When requesting first page, lastTimestamp and lastId are null
        String sql;
        if (lastTimestamp == null || lastId == null) {
            sql = """
                SELECT id, account_id, amount, tx_timestamp
                FROM account_transactions
                WHERE account_id = ?
                ORDER BY tx_timestamp DESC, id DESC
                LIMIT ?
                """;
        } else {
            sql = """
                SELECT id, account_id, amount, tx_timestamp
                FROM account_transactions
                WHERE account_id = ?
                  AND (tx_timestamp, id) < (?, ?)
                ORDER BY tx_timestamp DESC, id DESC
                LIMIT ?
                """;
        }

        try (Connection conn = ds.getConnection();
             PreparedStatement ps = conn.prepareStatement(sql)) {

            int idx = 1;
            ps.setLong(idx++, accountId);
            if (lastTimestamp != null && lastId != null) {
                ps.setTimestamp(idx++, Timestamp.from(lastTimestamp));
                ps.setLong(idx++, lastId);
            }
            ps.setInt(idx, pageSize);

            List<TransactionRecord> results = new ArrayList<>();
            try (ResultSet rs = ps.executeQuery()) {
                while (rs.next()) {
                    results.add(new TransactionRecord(
                        rs.getLong("id"),
                        rs.getLong("account_id"),
                        rs.getDouble("amount"),
                        rs.getTimestamp("tx_timestamp").toInstant()
                    ));
                }
            }
            return results;
        }
    }
}
```

### Example 2: Real-World Transient Fault Retry Handler
```java
package com.example.jdbc.pattern;

import java.sql.SQLException;
import java.util.Set;

public final class TransientDbRetryHandler {
    // Known transient error SQLState codes:
    // 40001: serialization_failure
    // 40P01: deadlock_detected
    // 08006: connection_failure_during_transaction
    private static final Set<String> TRANSIENT_SQL_STATES = Set.of("40001", "40P01", "08006");

    @FunctionalInterface
    public interface SqlCallable<T> {
        T call() throws SQLException;
    }

    public static <T> T executeWithRetry(SqlCallable<T> operation, int maxAttempts, long initialBackoffMs) throws SQLException {
        int attempt = 0;
        while (true) {
            attempt++;
            try {
                return operation.call();
            } catch (SQLException ex) {
                if (TRANSIENT_SQL_STATES.contains(ex.getSQLState()) && attempt < maxAttempts) {
                    long backoff = (long) (initialBackoffMs * Math.pow(2, attempt - 1) + (Math.random() * 20));
                    System.err.printf("[WARN] Transient error [%s]. Retrying attempt %d after %d ms...%n",
                            ex.getSQLState(), attempt, backoff);
                    try { Thread.sleep(backoff); } catch (InterruptedException ignored) {}
                    continue;
                }
                throw ex; // Re-throw non-transient or exhausted exception
            }
        }
    }
}
```

### Example 3: Edge Case - Handling Partial Updates with Dynamic SQL Builder
```java
package com.example.jdbc.pattern;

import java.sql.*;
import java.util.*;

public final class DynamicUpdateBuilder {
    public static int updateCustomerFields(Connection conn, long customerId, Map<String, Object> fieldsToUpdate) throws SQLException {
        if (fieldsToUpdate.isEmpty()) return 0;

        StringBuilder sb = new StringBuilder("UPDATE customers SET ");
        List<Object> values = new ArrayList<>();

        int i = 0;
        for (var entry : fieldsToUpdate.entrySet()) {
            if (i > 0) sb.append(", ");
            sb.append(entry.getKey()).append(" = ?");
            values.add(entry.getValue());
            i++;
        }
        sb.append(", updated_at = NOW() WHERE id = ?");
        values.add(customerId);

        try (PreparedStatement ps = conn.prepareStatement(sb.toString())) {
            for (int idx = 0; idx < values.size(); idx++) {
                ps.setObject(idx + 1, values.get(idx));
            }
            return ps.executeUpdate();
        }
    }
}
```

---

## 6. Best Practices
1. **Never Leak `java.sql.*` Types to Domain/Service Layers**: Repositories must map relational data to POJOs, DTOs, or Java Records.
2. **Translate Checked SQLExceptions to Unchecked Runtime Exceptions**: Decouple service layers from JDBC-specific checked exceptions.
3. **Use Keyset Pagination for High-Scale APIs**: Avoid `OFFSET` pagination on datasets with more than a few thousand records.
4. **Centralize Row Mapping Logic**: Define reusable static `RowMapper` lambdas or methods to avoid duplicating column extraction code.
5. **Implement Retry Logic for Transient Failures**: Protect mission-critical transactions from deadlocks and network blips with exponential backoff retries.
6. **Program Against Interfaces**: Define repository interfaces (`AccountRepository`) so mock implementations can be injected easily into unit tests.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Correction |
|---|---|---|
| Leaking `ResultSet` to service layer | Keeps database connection open outside repository scope | Map rows into domain objects before returning |
| Scattering raw SQL strings across application | High duplication; schema refactoring becomes a nightmare | Centralize SQL strings inside dedicated Repository classes |
| Deep `OFFSET` pagination in public endpoints | Causes database CPU saturation and slow API response | Implement Keyset (seek) pagination using cursor tokens |
| Forgetting to wrap multi-repository calls in transaction | Partial repository writes cause data inconsistency | Use Unit of Work or declarative transaction manager |
| Failing to retry on SQLState `40001` / `40P01` | Aborts legitimate user transactions under concurrent load | Add automated exponential backoff retry loop |
| Re-inventing dynamic SQL concatenation | Introduces SQL injection if column names are unvalidated | Whitelist dynamic column names against schema constants |
| Swallowing SQLExceptions in catch blocks | Leaves caller blind to database write failures | Wrap and propagate as domain runtime exception |
| Instantiating new Repository per request | Wastes GC memory; repositories are stateless singletons | Register repositories as singletons in DI container |

---

## 8. Performance & Security Notes
- **Column Whitelisting in Dynamic Queries**: If building dynamic `ORDER BY` or `SET` clauses, never concatenate raw user input strings as column names. Always validate against an enumerated whitelist of allowed column identifiers.
- **Garbage Collection Optimization**: Java 21 Records generate compact heap representations and allow JVM escape analysis to eliminate temporary object allocations during row mapping loops.
- **Connection Affinity**: When multiple repository methods participate in a single business transaction, they must share the exact same physical `Connection`. This is accomplished using thread-local contextual connection holders.

---

## 9. Comparison / When to Use What

| Pattern | Scope | Key Benefit | Trade-Off |
|---|---|---|---|
| **DAO** | 1:1 with DB Tables | Fine-grained CRUD control, straightforward mapping | Tight coupling to relational table structure |
| **Repository** | 1:1 with Domain Aggregates | Decouples business logic from persistence mechanism | Requires aggregate root orchestration |
| **Unit of Work** | Business Transaction | Tracks changes, optimizes batch commits, handles rollbacks | Higher architectural complexity |
| **Template Method** | JDBC Execution | Eliminates try-with-resources and error handling boilerplate | Inversion of control |

---

## 10. Interview Questions

<details>
<summary>1. What is the fundamental difference between the DAO pattern and the Repository pattern?</summary>
The <b>DAO pattern</b> is data-centric; it provides a 1:1 abstraction over a specific database table or view, focusing strictly on CRUD operations and relational persistence details.<br>
The <b>Repository pattern</b> is domain-centric (from Domain-Driven Design); it provides an in-memory collection-like abstraction for a Domain Aggregate Root, encapsulating the complex persistence of the aggregate and its child entities across multiple tables.
</details>

<details>
<summary>2. How does Keyset Pagination eliminate the O(N) performance degradation of OFFSET pagination?</summary>
<code>OFFSET N</code> forces the database engine to traverse, sort, and discard all preceding $N$ records before returning the requested page. Keyset pagination uses an indexed composite seek predicate (<code>WHERE (created_at, id) < (:last_ts, :last_id)</code>) that leverages B-tree indexes to jump directly to the target record in $O(\log N)$ time, maintaining constant response latency regardless of whether accessing page 1 or page 50,000.
</details>

<details>
<summary>3. Why should checked SQLExceptions be translated into unchecked exceptions in Repository classes?</summary>
<code>SQLException</code> is a checked exception coupled directly to JDBC and vendor wire protocols. Forcing service and business layers to catch or declare <code>SQLException</code> violates separation of concerns and prevents switching persistence technologies (e.g., to NoSQL or memory). Translating into unchecked domain exceptions (e.g., <code>DataAccessException</code>) allows clean error propagation.
</details>

<details>
<summary>4. What is the Unit of Work pattern and how does it prevent inconsistent transaction writes?</summary>
The Unit of Work pattern tracks all domain entities created, modified, or deleted during a business transaction. At transaction commit time, it opens a single JDBC connection, coordinates database writes in a deterministic order to prevent deadlocks, executes statements as an atomic batch, and commits or rolls back as a single atomic unit.
</details>

<details>
<summary>5. How can multiple repositories share a single JDBC transaction without passing Connection objects explicitly?</summary>
In modern frameworks (like Spring) or custom architectures, connection sharing is achieved using a <code>ThreadLocal<Connection></code> contextual holder or Scoped Values in Java 21+. When a transactional boundary is initiated, a connection is acquired from the DataSource and bound to the current thread. Repositories retrieve this contextual connection; when the transaction concludes, the holder commits and returns the connection to the pool.
</details>

<details>
<summary>6. Which SQLState codes represent transient errors that can be safely retried?</summary>
Common transient error SQLState codes include:
- <code>40001</code>: Serialization failure (transaction conflict in SERIALIZABLE isolation).
- <code>40P01</code>: Deadlock detected (one transaction was aborted by the database engine).
- <code>08006</code>: Connection failure during transaction (temporary network disruption).
</details>

<details>
<summary>7. Why is dynamic SQL construction in repositories dangerous even when using PreparedStatements for values?</summary>
While <code>PreparedStatement</code> parameters (<code>?</code>) safely sanitize literal values, they cannot be used for SQL identifiers such as table names, column names, or sort directions (<code>ASC</code>/<code>DESC</code>). If column names are concatenated dynamically from user inputs, attackers can inject arbitrary SQL. Column names must always be validated against a strict whitelist.
</details>

<details>
<summary>8. How do Java 21 Records improve data access layer implementation?</summary>
Java Records provide transparent, immutable data carriers with concise syntax, built-in <code>equals</code>, <code>hashCode</code>, and <code>toString</code>. Their compact memory footprint reduces heap allocations during high-volume query mapping, and their immutability guarantees thread safety across service boundaries.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Implement an audited repository method that captures row count and query execution duration.</summary>

```java
public static <T> T timedQuery(String operationName, SqlSupplier<T> query) throws SQLException {
    long start = System.nanoTime();
    try {
        return query.get();
    } finally {
        long durationMs = (System.nanoTime() - start) / 1_000_000;
        System.out.printf("[METRICS] %s executed in %d ms%n", operationName, durationMs);
    }
}

@FunctionalInterface
public interface SqlSupplier<T> {
    T get() throws SQLException;
}
```
</details>

<details>
<summary>Exercise 2: Write a safe column whitelister for dynamic sorting.</summary>

```java
public final class SafeSorter {
    private static final Set<String> ALLOWED_SORT_COLUMNS = Set.of("id", "created_at", "total_amount", "customer_id");

    public static String buildOrderClause(String userRequestedCol, boolean ascending) {
        if (!ALLOWED_SORT_COLUMNS.contains(userRequestedCol.toLowerCase())) {
            throw new IllegalArgumentException("Invalid sort column: " + userRequestedCol);
        }
        return "ORDER BY " + userRequestedCol + (ascending ? " ASC" : " DESC");
    }
}
```
</details>

<details>
<summary>Exercise 3: Implement a generic RowMapper for a Customer entity.</summary>

```java
public record Customer(long id, String email, String name, boolean active) {}

public final class CustomerRowMapper implements RowMapper<Customer> {
    @Override
    public Customer mapRow(ResultSet rs, int rowNum) throws SQLException {
        return new Customer(
            rs.getLong("id"),
            rs.getString("email"),
            rs.getString("name"),
            rs.getBoolean("active")
        );
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet
- **DAO Pattern**: 1:1 mapping with relational tables; encapsulates low-level SQL operations.
- **Repository Pattern**: Domain aggregate abstraction; decouples business domain models from database schemas.
- **Keyset Pagination**: Seek using `WHERE (col, id) < (:val, :last_id)` to ensure $O(\log N)$ paging speed.
- **Transient Retries**: Automatically retry on SQLState `40001` (serialization failure) and `40P01` (deadlock).
- **Dynamic SQL**: Never concatenate untrusted column names; always whitelist against schema constants.
- **Exception Boundary**: Translate checked `SQLException` into unchecked domain runtime exceptions.

---

## 13. Further Reading
- *Patterns of Enterprise Application Architecture* by Martin Fowler (Repository, DAO, Unit of Work)
- *Domain-Driven Design: Tackling Complexity in the Heart of Software* by Eric Evans
- *High-Performance Java Persistence* by Vlad Mihalcea
- Spring Framework: Data Access Architecture and Exception Translation
