# Statements and Prepared Statements

> Parameterized PreparedStatements are the critical defense against SQL injection and the foundation of database execution plan caching and batch processing.

| Metadata | Details |
|---|---|
| **Level** | Core / Intermediate |
| **Prerequisites** | JDBC Fundamentals, SQL Basics |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **JDK Baseline** | Java 17 / 21 / 25 LTS |

---

## Table of Contents
1. [1. What & Why](#1-what--why)
2. [2. Core Concepts](#2-core-concepts)
3. [3. How It Works Internally: Server-Side Precompilation & Wire Protocols](#3-how-it-works-internally-server-side-precompilation--wire-protocols)
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
In relational database systems, executing a SQL string involves parsing SQL text, verifying semantic identifiers, optimizing access paths, compiling an execution plan, and finally executing the plan across table pages.

Using raw `Statement` with string concatenation incurs two severe problems:
1. **Catastrophic SQL Injection Vulnerabilities**: Malicious user input can alter SQL statement syntax, leading to data exfiltration or table destruction.
2. **CPU Thrashing on the Database Engine**: Every unique string requires the database query planner to re-parse and re-compile execution plans from scratch.

`PreparedStatement` solves both problems fundamentally: it separates SQL code structure from untrusted parameter data, and enables database engines to compile and cache execution plans once and execute them repeatedly with varying parameter bindings.

---

## 2. Core Concepts
- **Statement**: Represents a basic SQL command container. Sends raw SQL text to the database engine on every execution. Suitable only for static DDL commands without user parameters.
- **PreparedStatement**: Extends `Statement`. Pre-compiles parameterized SQL templates containing parameter placeholders (`?`). Parameters are transmitted out-of-band via binary wire protocol.
- **CallableStatement**: Extends `PreparedStatement`. Used to execute database stored procedures and user-defined functions with `IN`, `OUT`, and `INOUT` parameters.
- **Parameter Placeholders (`?`)**: Standard JDBC positional parameter markers (1-based index).
- **Batch Processing**: Grouping multiple parameter sets into a single network packet transmission via `addBatch()` and `executeBatch()`, drastically reducing network latency.
- **Generated Keys**: Retrieving auto-generated surrogate keys (e.g., `BIGINT GENERATED ALWAYS AS IDENTITY`) without issuing a separate `SELECT` query.

---

## 3. How It Works Internally: Server-Side Precompilation & Wire Protocols

When using a Type 4 driver (like PostgreSQL or MySQL), `PreparedStatement` communicates via an extended query wire protocol rather than simple text:

```
Java Client                                Database Server
    |                                             |
    |------- 1. Parse (SQL with $1, $2) --------->| Parser / Lexer
    |                                             | Query Planner
    |<------ 2. ParseComplete (Plan Cached) ------| Creates Generic Plan
    |                                             |
    |------- 3. Bind (Binary Parameter Values) -->| Substitutes values into plan
    |------- 4. Execute ------------------------->| Executes optimized plan
    |<------ 5. Data Rows / CommandComplete ------| Returns ResultSet / UpdateCount
    |                                             |
```

### PostgreSQL Extended Query Protocol Steps:
1. **Parse**: The driver sends the SQL template string to the backend. The backend compiles the AST (Abstract Syntax Tree) and caches the generic plan.
2. **Bind**: The driver transmits binary parameter values corresponding to parameters `$1, $2, \dots`. Because parameters are sent strictly as typed binary payloads, user input can **never** be interpreted as executable SQL syntax.
3. **Execute**: The server runs the cached plan against the bound parameters and streams results back.
4. **Server-Side Statement Cache**: Modern drivers (e.g., PgJDBC with `prepareThreshold=5`) automatically promote client-side prepared statements to named server-side prepared statements after repeated executions, bypassing server parsing entirely.

---

## 4. Syntax & Usage

### 4.1 Safe Parameterized Queries
```java
package com.example.jdbc;

import java.math.BigDecimal;
import java.sql.*;
import java.time.Instant;

public final class PreparedStatementDemo {

    public static void executeTransfer(Connection conn, long fromAcc, long toAcc, BigDecimal amount) throws SQLException {
        String deductSql = "UPDATE accounts SET balance = balance - ? WHERE id = ? AND balance >= ?";
        String creditSql = "UPDATE accounts SET balance = balance + ? WHERE id = ?";

        // PreparedStatement with 1-based indexed parameter binding
        try (PreparedStatement psDeduct = conn.prepareStatement(deductSql);
             PreparedStatement psCredit = conn.prepareStatement(creditSql)) {

            // Bind deduction parameters
            psDeduct.setBigDecimal(1, amount);
            psDeduct.setLong(2, fromAcc);
            psDeduct.setBigDecimal(3, amount);

            int rows = psDeduct.executeUpdate();
            if (rows == 0) {
                throw new SQLException("Insufficient funds or account not found for ID: " + fromAcc);
            }

            // Bind credit parameters
            psCredit.setBigDecimal(1, amount);
            psCredit.setLong(2, toAcc);
            psCredit.executeUpdate();
        }
    }
}
```

### 4.2 High-Throughput Batch Processing
```java
package com.example.jdbc;

import java.sql.Connection;
import java.sql.PreparedStatement;
import java.sql.SQLException;
import java.util.List;

public final class BatchInsertDemo {
    public record OrderItem(long orderId, String sku, int quantity, double price) {}

    public static void insertOrderItemsBatch(Connection conn, List<OrderItem> items) throws SQLException {
        String sql = "INSERT INTO order_items (order_id, sku, quantity, unit_price) VALUES (?, ?, ?, ?)";

        try (PreparedStatement ps = conn.prepareStatement(sql)) {
            int batchCount = 0;
            for (OrderItem item : items) {
                ps.setLong(1, item.orderId());
                ps.setString(2, item.sku());
                ps.setInt(3, item.quantity());
                ps.setDouble(4, item.price());
                ps.addBatch();

                // Flush every 500 records to balance memory consumption and network packets
                if (++batchCount % 500 == 0) {
                    ps.executeBatch();
                }
            }
            if (batchCount % 500 != 0) {
                ps.executeBatch(); // Flush remaining
            }
        }
    }
}
```

---

## 5. Practical Examples

### Example 1: Basic Parameter Binding with java.time and BigDecimal
```java
package com.example.jdbc;

import java.math.BigDecimal;
import java.sql.*;
import java.time.Instant;

public final class DateTimeBindingDemo {
    public static void logAuditRecord(Connection conn, String action, String user, Instant timestamp) throws SQLException {
        String sql = "INSERT INTO audit_logs (action, username, event_time) VALUES (?, ?, ?)";
        try (PreparedStatement ps = conn.prepareStatement(sql)) {
            ps.setString(1, action);
            ps.setString(2, user);
            // Binding modern Instant via Timestamp conversion or setObject
            ps.setTimestamp(3, Timestamp.from(timestamp));
            ps.executeUpdate();
        }
    }
}
```

### Example 2: Real-World Batch Insertion with Transaction Control
```java
package com.example.jdbc;

import java.sql.*;
import java.util.Arrays;

public final class TransactionalBatchService {
    public static void processBatch(Connection conn) throws SQLException {
        boolean originalAutoCommit = conn.getAutoCommit();
        conn.setAutoCommit(false); // Begin manual transaction

        String sql = "INSERT INTO metric_samples (metric_name, val, recorded_at) VALUES (?, ?, NOW())";
        try (PreparedStatement ps = conn.prepareStatement(sql)) {
            for (int i = 0; i < 1000; i++) {
                ps.setString(1, "cpu_usage");
                ps.setDouble(2, Math.random() * 100.0);
                ps.addBatch();
            }

            int[] updateCounts = ps.executeBatch();
            System.out.println("Batch executed successfully. Elements: " + updateCounts.length);
            conn.commit(); // Commit transaction
        } catch (SQLException ex) {
            conn.rollback(); // Rollback on failure
            throw ex;
        } finally {
            conn.setAutoCommit(originalAutoCommit);
        }
    }
}
```

### Example 3: Edge Case - Handling IN Clauses with Dynamic Parameters
```java
package com.example.jdbc;

import java.sql.*;
import java.util.List;

public final class DynamicInClauseDemo {
    // In standard JDBC, `WHERE id IN (?)` cannot accept a List or Array directly on all databases.
    public static void queryWithInClause(Connection conn, List<Long> ids) throws SQLException {
        if (ids.isEmpty()) return;

        // Generate ? placeholders dynamically: "?, ?, ?"
        StringBuilder sb = new StringBuilder("SELECT id, name FROM users WHERE id IN (");
        for (int i = 0; i < ids.size(); i++) {
            sb.append(i == 0 ? "?" : ", ?");
        }
        sb.append(")");

        try (PreparedStatement ps = conn.prepareStatement(sb.toString())) {
            for (int i = 0; i < ids.size(); i++) {
                ps.setLong(i + 1, ids.get(i)); // 1-based parameter indexing
            }
            try (ResultSet rs = ps.executeQuery()) {
                while (rs.next()) {
                    System.out.println("User: " + rs.getString("name"));
                }
            }
        }
    }
}
```

### Example 4: Failure Case - SQL Injection Demonstration
```java
// VULNERABLE: Direct string concatenation
public static void vulnerableLogin(Connection conn, String userInput, String passInput) throws SQLException {
    Statement stmt = conn.createStatement();
    // If userInput is: admin' --
    String sql = "SELECT * FROM users WHERE username = '" + userInput + "' AND password = '" + passInput + "'";
    // Evaluates to: SELECT * FROM users WHERE username = 'admin' --' AND password = '...'
    // Bypasses password check completely!
    ResultSet rs = stmt.executeQuery(sql);
}

// SAFE: PreparedStatement
public static void safeLogin(Connection conn, String userInput, String passInput) throws SQLException {
    String sql = "SELECT * FROM users WHERE username = ? AND password = ?";
    try (PreparedStatement ps = conn.prepareStatement(sql)) {
        ps.setString(1, userInput);
        ps.setString(2, passInput);
        try (ResultSet rs = ps.executeQuery()) {
            // Parameter data cannot alter SQL grammar
        }
    }
}
```

---

## 6. Best Practices
1. **Always Use PreparedStatement for DML and Queries**: Never use string concatenation to embed variables into SQL strings.
2. **Batch Large Inserts and Updates**: Use `addBatch()` and `executeBatch()` with chunks of 200–1000 items to avoid network round-trip overhead.
3. **Use Explicit Parameter Setters**: Call type-safe methods like `setLong`, `setString`, `setBigDecimal` instead of `setObject()` whenever possible.
4. **Turn Off AutoCommit During Batches**: Disabling autocommit prevents the database from writing a transaction log commit record for each individual row.
5. **Set Fetch Size on Large ResultSets**: Call `statement.setFetchSize(500)` when iterating over large tables to prevent loading millions of rows into JVM heap memory.
6. **Reuse PreparedStatements within Loops**: Create the `PreparedStatement` once outside the loop and clear parameters via `ps.clearParameters()` or simply re-bind them.
7. **Always Close Statements in Try-with-Resources**: Avoid cursor leaks on the database server.
8. **Check executeBatch Return Array**: Verify the returned `int[]` array for `Statement.EXECUTE_FAILED`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Correction |
|---|---|---|
| String concatenation in SQL | Allows severe SQL injection attacks | Use `PreparedStatement` with `?` markers |
| Using 0-based index for parameters | JDBC parameter indexes start at 1; throws `SQLException` | Always use 1-based indexing (`ps.setString(1, ...)`)|
| Forgetting to call `executeBatch()` | Queued statements remain in client memory and are never sent to DB | Always invoke `ps.executeBatch()` |
| Passing huge unbatched lists to `IN (?)` | Exceeds database maximum parameter limits (e.g., 32,767 in PostgreSQL) | Batch IDs into chunks of 500 or use temporary tables |
| Creating PreparedStatement inside tight loop | Defeats statement caching and wastes JVM memory | Create statement once before loop, rebind parameters |
| Failing to disable autocommit in batches | Each batch item incurs separate disk flush on DB | Execute `conn.setAutoCommit(false)` before batch |
| Calling `executeBatch()` without checking return codes | Hides partial batch execution failures | Inspect `int[]` return array for `Statement.EXECUTE_FAILED` |
| Calling `rs.close()` but leaving `ps` open | Causes resource leak in database backend | Enclose in try-with-resources |

---

## 8. Performance & Security Notes
- **Query Plan Reusability**: PreparedStatement enables database engines to compile an execution plan once. For 10,000 rapid queries, this eliminates seconds of parsing and planning overhead.
- **Binary Wire Protocol**: Prepared statements transfer parameters in compact binary representation (e.g., 4 bytes for integer, 8 bytes for double) instead of ASCII string representations, reducing network serialization cost.
- **Client-Side vs Server-Side Statements**: Some drivers (e.g., MySQL Connector/J) emulate prepared statements on the client side by default (`useServerPrepStmts=false`). For true server-side precompilation, configure server-side flags in JDBC URL.
- **Batch Error Handling**: When a statement in a batch fails, `BatchUpdateException` is thrown. The method `ex.getUpdateCounts()` indicates which statements succeeded before the failure.
- **Parameter Sensitivity**: Occasionally, generic plans can choose suboptimal paths if the planner doesn't evaluate column histogram values. Modern engines evaluate specific plans on first execution before falling back to generic plans.

---

## 9. Comparison / When to Use What

| Feature | Statement | PreparedStatement | CallableStatement |
|---|---|---|---|
| **SQL Injection Safe** | No (requires manual escaping) | Yes (immune by design) | Yes |
| **Execution Plan Caching** | No (parsed every execution) | Yes (reusable generic plan) | Yes |
| **Batch Processing** | Yes (concatenates raw SQL) | Yes (efficient parameter arrays)| No |
| **Stored Procedures** | Limited | Limited | Native (`OUT` parameters) |
| **Best Used For** | Static DDL (`CREATE`, `DROP`) | 99% of DML and SELECTs | Legacy database stored procedures |

---

## 10. Interview Questions

<details>
<summary>1. How does PreparedStatement prevent SQL Injection at the database protocol level?</summary>
PreparedStatement transmits the SQL template and the parameter values separately across the network protocol. The database engine parses and compiles the SQL syntax into an execution plan first. When parameter values are bound in a subsequent step, they are treated strictly as literal data values in memory cells, making it syntactically impossible for input strings like <code>' OR '1'='1</code> to modify the abstract syntax tree or command structure.
</details>

<details>
<summary>2. Why are JDBC parameter indexes 1-based instead of 0-based?</summary>
JDBC was designed to align with the ANSI SQL standard, which historically uses 1-based indexing for ordinal positions of columns and parameters in relational catalogs. Starting at index 1 also differentiates positional parameters from array offsets in C/Java.
</details>

<details>
<summary>3. What is the performance impact of autocommit when executing batch updates?</summary>
When autocommit is enabled (the default), the database engine must commit a separate transaction and synchronously flush its Write-Ahead Log (WAL) to disk for every individual batch statement. Disabling autocommit (<code>conn.setAutoCommit(false)</code>) wraps the entire batch in a single transaction, requiring only one disk flush at the end, improving throughput by orders of magnitude.
</details>

<details>
<summary>4. What does the int[] array returned by executeBatch() represent?</summary>
Each element in the returned <code>int[]</code> array corresponds to the update count of a statement in the batch in the order added. Values $\ge 0$ denote the number of rows affected. <code>Statement.SUCCESS_NO_INFO (-2)</code> indicates successful execution where the row count was not tracked. <code>Statement.EXECUTE_FAILED (-3)</code> denotes failure (in drivers that continue batch execution after errors).
</details>

<details>
<summary>5. How do you handle a variable number of parameters in an IN clause with PreparedStatement?</summary>
Standard JDBC does not allow binding a Java List or collection to a single <code>?</code> placeholder. You must either dynamically construct the SQL string with an exact number of <code>?</code> placeholders matching the collection size, chunk the collection into fixed batches (e.g., 50), or use database-specific array binding (e.g., <code>conn.createArrayOf("BIGINT", ids)</code> and <code>WHERE id = ANY(?)</code> in PostgreSQL).
</details>

<details>
<summary>6. What is the difference between client-side prepared statements and server-side prepared statements?</summary>
In client-side prepared statements, the JDBC driver escapes strings and substitutes parameters locally into raw SQL before sending text to the server. In server-side prepared statements, the driver sends the template to the server to be parsed and prepared (assigning a statement handle), and subsequent executions transmit only the handle and binary parameter values, saving database CPU.
</details>

<details>
<summary>7. What happens if you call addBatch() with a SELECT query on a PreparedStatement?</summary>
The JDBC specification specifies that <code>addBatch()</code> and <code>executeBatch()</code> are designed for statements returning update counts (DML statements like INSERT, UPDATE, DELETE). If a query that produces a <code>ResultSet</code> is added to a batch, calling <code>executeBatch()</code> will throw a <code>SQLException</code>.
</details>

<details>
<summary>8. How do you retrieve generated primary keys when executing an INSERT via PreparedStatement?</summary>
You must pass <code>Statement.RETURN_GENERATED_KEYS</code> as the second argument to <code>conn.prepareStatement(sql, Statement.RETURN_GENERATED_KEYS)</code>. After calling <code>executeUpdate()</code>, call <code>ps.getGeneratedKeys()</code> which returns a <code>ResultSet</code> containing the generated keys.
</details>

<details>
<summary>9. What is Statement pooling in connection pools like HikariCP?</summary>
Statement pooling caches open <code>PreparedStatement</code> instances across transactions on the same physical connection. When an application prepares the exact same SQL string, the pool returns the existing prepared statement rather than re-allocating driver structures and requesting server plan re-parsing.
</details>

<details>
<summary>10. What is the function of ps.clearParameters()?</summary>
<code>clearParameters()</code> immediately clears all parameter bindings on the current <code>PreparedStatement</code>, allowing it to be safely reused with completely new parameter values without retaining references to earlier objects in driver memory.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Write a robust batch update method that updates employee salaries and handles BatchUpdateException.</summary>

```java
public static void updateSalariesBatch(Connection conn, Map<Long, BigDecimal> raises) throws SQLException {
    String sql = "UPDATE employees SET salary = salary + ? WHERE id = ?";
    boolean autoCommit = conn.getAutoCommit();
    conn.setAutoCommit(false);

    try (PreparedStatement ps = conn.prepareStatement(sql)) {
        for (var entry : raises.entrySet()) {
            ps.setBigDecimal(1, entry.getValue());
            ps.setLong(2, entry.getKey());
            ps.addBatch();
        }
        int[] results = ps.executeBatch();
        conn.commit();
        System.out.println("Updated employees: " + results.length);
    } catch (BatchUpdateException bue) {
        conn.rollback();
        System.err.println("Batch failed! Completed before error: " + bue.getUpdateCounts().length);
        throw bue;
    } finally {
        conn.setAutoCommit(autoCommit);
    }
}
```
</details>

<details>
<summary>Exercise 2: Write a method using PostgreSQL array binding (ANY) instead of dynamic IN clauses.</summary>

```java
public static void findUsersByIdArray(Connection conn, Long[] userIds) throws SQLException {
    String sql = "SELECT id, name FROM users WHERE id = ANY(?)";
    Array sqlArray = conn.createArrayOf("BIGINT", userIds);
    try (PreparedStatement ps = conn.prepareStatement(sql)) {
        ps.setArray(1, sqlArray);
        try (ResultSet rs = ps.executeQuery()) {
            while (rs.next()) {
                System.out.println("User: " + rs.getString("name"));
            }
        }
    } finally {
        sqlArray.free(); // Free native driver memory
    }
}
```
</details>

<details>
<summary>Exercise 3: Demonstrate how to call a stored procedure returning an OUT parameter via CallableStatement.</summary>

```java
public static BigDecimal getCustomerTotalSpend(Connection conn, long customerId) throws SQLException {
    String callSql = "{call calculate_customer_spend(?, ?)}";
    try (CallableStatement cs = conn.prepareCall(callSql)) {
        cs.setLong(1, customerId);
        cs.registerOutParameter(2, Types.NUMERIC);
        cs.execute();
        return cs.getBigDecimal(2);
    }
}
```
</details>

<details>
<summary>Exercise 4: Implement an idempotent upsert method using ON CONFLICT / ON DUPLICATE KEY.</summary>

```java
public static void upsertUserPreference(Connection conn, long userId, String prefKey, String prefVal) throws SQLException {
    String sql = """
        INSERT INTO user_preferences (user_id, pref_key, pref_val)
        VALUES (?, ?, ?)
        ON CONFLICT (user_id, pref_key)
        DO UPDATE SET pref_val = EXCLUDED.pref_val, updated_at = CURRENT_TIMESTAMP
        """;
    try (PreparedStatement ps = conn.prepareStatement(sql)) {
        ps.setLong(1, userId);
        ps.setString(2, prefKey);
        ps.setString(3, prefVal);
        ps.executeUpdate();
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet
- **PreparedStatement**: Always use for queries with user parameters to guarantee SQL injection immunity.
- **Parameter Markers**: Use `?` placeholders with 1-based indexing (`ps.setString(1, ...)`).
- **Batch Processing**: Combine `ps.addBatch()` with `ps.executeBatch()` in chunks of 500 rows.
- **Transaction Boundary**: Always disable autocommit (`conn.setAutoCommit(false)`) before batch processing.
- **Resource Management**: PreparedStatement and ResultSet must be closed via try-with-resources.
- **Generated Keys**: Pass `Statement.RETURN_GENERATED_KEYS` to capture surrogate primary keys.

---

## 13. Further Reading
- OWASP Top 10: SQL Injection Prevention Cheat Sheet
- PostgreSQL Extended Query Protocol Specification
- MySQL Connector/J Performance Options: `rewriteBatchedStatements`
- *Java Persistence with Spring Data and Hibernate* by Christian Bauer
