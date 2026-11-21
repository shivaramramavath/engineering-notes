# Interview Mastery: JDBC, Connection Pooling & Persistence

> Deep-dive technical interview guide for Java Persistence: JDBC internals, HikariCP lock-free pooling, Transaction Isolation Levels, ACID anomalies, JPA entity lifecycle, and N+1 query optimizations.

| Metadata | Value |
|---|---|
| **Level** | Advanced |
| **Prerequisites** | [00_core-java.md](00_core-java.md), [16-jdbc-and-databases](../16-jdbc-and-databases/README.md) |
| **Time to Read** | 35 minutes |
| **Status** | Complete |
| **Java Version** | Java 8 to Java 25 LTS |

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
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

In enterprise backend applications, data persistence forms the foundation of system reliability and transaction integrity.

Technical interviews for senior backend engineers routinely test:
- Why **`PreparedStatement`** neutralizes SQL Injection attacks at the protocol level.
- The mechanics of **HikariCP** and why small connection pools drastically outperform large pools.
- The 4 standard **SQL Transaction Isolation Levels** and their associated concurrency anomalies (Dirty Reads, Non-Repeatable Reads, Phantom Reads).
- The **JPA Entity Lifecycle** (Transient, Managed, Detached, Removed) and the First-Level Persistence Context cache.
- Detecting, resolving, and preventing the infamous **N+1 Query Problem**.

---

## 2. Core Concepts

### 1. Statement vs PreparedStatement
- **`Statement`**: Compiles and executes the raw SQL string on every call. Vulnerable to catastrophic SQL Injection if user inputs are concatenated directly.
- **`PreparedStatement`**: Pre-compiles the SQL template on the database server. User parameters are passed out-of-band as distinct binary data, rendering SQL injection structurally impossible. It also enables query plan caching on the database.

### 2. Transaction Isolation Levels & ACID Anomalies
A transaction ensures **ACID** properties (Atomicity, Consistency, Isolation, Durability).
The ANSI/ISO SQL standard defines 4 isolation levels:

| Isolation Level | Dirty Read | Non-Repeatable Read | Phantom Read |
|---|---|---|---|
| **`READ UNCOMMITTED`** | **Permitted** | Permitted | Permitted |
| **`READ COMMITTED`** (Default in Postgres/Oracle) | Prevented | **Permitted** | Permitted |
| **`REPEATABLE READ`** (Default in MySQL InnoDB) | Prevented | Prevented | **Permitted** (MVCC mitigates) |
| **`SERIALIZABLE`** | Prevented | Prevented | Prevented |

- **Dirty Read**: Transaction A reads uncommitted data written by Transaction B (which later rolls back).
- **Non-Repeatable Read**: Transaction A reads a row, Transaction B updates that row and commits; Transaction A re-reads the row and observes altered values.
- **Phantom Read**: Transaction A queries a range of rows, Transaction B inserts new rows matching that range and commits; Transaction A re-queries and observes "phantom" rows.

### 3. JPA Entity Lifecycle States
1. **Transient (New)**: Instantiated via `new Entity()`; has no primary key and is not tracked by any persistence context.
2. **Managed (Persistent)**: Associated with an active `EntityManager` / Persistence Context. Any mutations to entity fields are automatically detected via **Dirty Checking** and synchronized to the database upon `flush()`.
3. **Detached**: Previously managed, but the persistence context has been closed or the entity was explicitly detached (`em.detach()`).
4. **Removed**: Scheduled for deletion upon transaction commit.

---

## 3. How It Works Internally

### HikariCP Lock-Free Architecture (`ConcurrentBag`)
Most legacy connection pools (Apache DBCP, c3p0) relied on synchronized queues, where threads competed for a single lock to borrow connections.
HikariCP achieves near-zero overhead using **`ConcurrentBag`**:
```
Thread borrows connection:
1. Thread-Local Cache: Checks if current thread previously borrowed a connection -> O(1) lock-free hit!
2. Shared Lock-Free Bag: If empty, checks shared lock-free array of connections via CAS.
3. Synchronous Handoff: If empty, awaits a connection returned by another thread via SynchronousQueue.
```
No synchronized locks are held during normal connection acquisition.

---

## 4. Syntax & Usage

### Production JDBC with Try-With-Resources & Batch Updates

```java
import java.sql.*;
import java.util.List;

public class ProductionJdbcTemplate {
    public record UserRecord(long id, String email, String role) {}

    public static void batchInsertUsers(Connection conn, List<UserRecord> users) throws SQLException {
        String sql = "INSERT INTO users (id, email, role) VALUES (?, ?, ?)";

        // Disable auto-commit to manage transaction boundary
        conn.setAutoCommit(false);

        try (PreparedStatement ps = conn.prepareStatement(sql)) {
            int batchCount = 0;
            for (UserRecord user : users) {
                ps.setLong(1, user.id());
                ps.setString(2, user.email());
                ps.setString(3, user.role());
                ps.addBatch();

                if (++batchCount % 1000 == 0) {
                    ps.executeBatch(); // Send batch of 1000 to database in single round-trip
                }
            }
            ps.executeBatch(); // Flush remaining
            conn.commit();     // Commit transaction
        } catch (SQLException e) {
            conn.rollback();   // Rollback transaction on failure
            throw e;
        } finally {
            conn.setAutoCommit(true);
        }
    }
}
```

---

## 5. Practical Examples

### Example 1: Transaction Rollback with Savepoints

```java
import java.sql.*;

public class SavepointInterviewDemo {
    public static void executeTransfer(Connection conn) throws SQLException {
        conn.setAutoCommit(false);
        Savepoint sp = null;

        try {
            // Step 1: Deduct from Account A
            updateBalance(conn, 1, -500.0);

            // Create intermediate checkpoint
            sp = conn.setSavepoint("AccountADeducted");

            // Step 2: Attempt credit to Account B (simulated error)
            updateBalance(conn, 2, 500.0);
            if (true) throw new SQLException("Network failure contacting Account B bank!");

            conn.commit();
        } catch (SQLException ex) {
            if (sp != null) {
                // Rollback strictly to savepoint (preserving Account A audit log if needed)
                conn.rollback(sp);
                System.out.println("Rolled back to savepoint: AccountADeducted");
            }
            conn.rollback(); // Complete rollback
        } finally {
            conn.setAutoCommit(true);
        }
    }

    private static void updateBalance(Connection conn, int accId, double delta) throws SQLException {
        // SQL update statement...
    }
}
```

### Example 2: HikariCP High-Throughput Pool Sizing Formula

```java
// HikariCP Recommended Pool Sizing Formula:
// pool_size = (core_count * 2) + effective_spindle_count
//
// On a 16-core server with an SSD disk array:
// pool_size = (16 * 2) + 1 = 33 connections!
//
// ANTI-PATTERN: Setting pool size to 500 connections causes CPU context switching thrashing
// and degrades overall throughput on the database server!

public class HikariConfigDemo {
    public static void configurePool() {
        // Sample HikariConfig configuration:
        // config.setMaximumPoolSize(32);
        // config.setMinimumIdle(16);
        // config.setConnectionTimeout(30000); // 30s timeout
        // config.setIdleTimeout(600000);       // 10m idle
    }
}
```

### Example 3: Simulating and Resolving the N+1 Query Problem

```java
public class NPlusOneProblemDemo {
    // SCENARIO: An Author has many Books.
    // Naive fetch:
    // Query 1: SELECT * FROM authors; (Returns N authors)
    // Query 2..N+1: For each author: SELECT * FROM books WHERE author_id = ?;
    // Total queries executed: 1 + N! (Catastrophic performance degradation)

    // SOLUTION 1: JPQL / HQL JOIN FETCH
    // "SELECT DISTINCT a FROM Author a LEFT JOIN FETCH a.books"
    // Executes in exactly 1 single SQL JOIN query!

    // SOLUTION 2: JPA EntityGraph
    // @EntityGraph(attributePaths = {"books"})
    // List<Author> findAll();
}
```

### Example 4: Large Dataset Streaming via JDBC Fetch Size

```java
import java.sql.*;

public class JdbcStreamingDemo {
    public static void streamLargeTable(Connection conn) throws SQLException {
        String sql = "SELECT * FROM transaction_history";

        try (PreparedStatement ps = conn.prepareStatement(sql,
                ResultSet.TYPE_FORWARD_ONLY, ResultSet.CONCUR_READ_ONLY)) {

            // Crucial: setFetchSize tells JDBC driver to stream rows in chunks of 500
            // rather than buffering the entire 10,000,000 row result set into Java heap memory!
            ps.setFetchSize(500);

            try (ResultSet rs = ps.executeQuery()) {
                long count = 0;
                while (rs.next()) {
                    long id = rs.getLong("id");
                    count++;
                }
                System.out.println("Streamed " + count + " rows without OutOfMemoryError!");
            }
        }
    }
}
```

### Example 5: JPA First-Level Cache Repeatable Read Behavior

```java
public class FirstLevelCacheDemo {
    public static void demonstrateCache(jakarta.persistence.EntityManager em) {
        // Query 1: Hits Database -> Caches Entity in Persistence Context
        // User u1 = em.find(User.class, 1L);

        // Query 2: Identical primary key in same transaction
        // User u2 = em.find(User.class, 1L);

        // Zero SQL sent to database for second call!
        // assert u1 == u2 : "First level cache guarantees identity equality!";
    }
}
```

---

## 6. Best Practices

1. **Always Use `PreparedStatement` with Parameter Markers (`?`)**: Never concatenate untrusted strings into SQL statements.
2. **Size Connection Pools Conservatively**: Follow HikariCP's formula: `(cores * 2) + 1`. Over-provisioning connections causes disk I/O thrashing and CPU lock contention on the database.
3. **Always Set `setFetchSize()` on Large Queries**: When processing millions of rows, set fetch size (e.g. 500) to stream results instead of loading the entire table into heap memory.
4. **Solve N+1 with `JOIN FETCH` or Batch Fetching**: Always verify generated SQL queries using `show-sql: true` or DataSource proxies to prevent N+1 query storms.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Fix |
|---|---|---|
| String concatenation in SQL queries | Allows attackers to break SQL grammar and execute malicious commands (SQL Injection) | Always use `PreparedStatement` with `?` placeholders. |
| Forgetting to close `ResultSet`, `Statement`, or `Connection` | Leaks database handles, exhausting the connection pool | Use Try-With-Resources for all JDBC objects. |
| Running queries inside application loops | Executes hundreds of individual network hops to database | Use SQL `IN (...)` clause or batch updates (`addBatch`). |
| Keeping transactions open during slow external API calls | Holds open database connections, starving the connection pool | Move external HTTP/RPC calls outside the transaction boundary. |
| Modifying detached JPA entities expecting auto-update | Persistence context is closed; dirty checking is disabled | Call `em.merge(entity)` to re-attach. |

---

## 8. Performance & Security Notes

- **Prepared Statement Caching**: Enterprise JDBC drivers cache compiled statement handles on both client and server. Reusing identical SQL strings allows the database optimizer to skip parsing and query plan generation.
- **Connection Leak Detection**: HikariCP supports `leakDetectionThreshold=2000` (ms). If a thread holds a connection without returning it to the pool for longer than 2 seconds, HikariCP captures a stack trace and logs a warning.
- **SQL Injection Defense in Depth**: In addition to `PreparedStatement`, enforce database user least-privilege principles (read-only grants, schema restrictions).

---

## 9. Comparison / When to Use What

| Technology | Level of Abstraction | SQL Control | Performance | Best For |
|---|---|---|---|---|
| **Raw JDBC** | Low (Direct driver calls) | Complete manual control | Maximum | Ultra-low latency, custom batch ETL |
| **Spring `JdbcTemplate`** | Medium (Handles boilerplate & closing) | Full SQL control | High | High-throughput services, complex SQL |
| **JDBI / MyBatis** | Medium (SQL mapped to interfaces) | Clean SQL mapping | High | Domain mapping with explicit SQL |
| **JPA / Hibernate** | High (Object-Relational Mapping) | Automated / HQL | Moderate | Complex enterprise domain models |

---

## 10. Interview Questions

<details>
<summary>1. How does a `PreparedStatement` prevent SQL Injection at the database protocol level?</summary>

When a `PreparedStatement` is created, the SQL command structure with parameter placeholders (`?`) is transmitted to the database and compiled into an executable query plan. The parameter values are subsequently sent in a separate protocol packet. The database treats the parameter values strictly as literal data, never interpreting quotes or special characters as SQL syntax.
</details>

<details>
<summary>2. Why does HikariCP recommend small connection pool sizes (e.g. 10 to 30) rather than hundreds?</summary>

Because a database server has a finite number of CPU cores and disk I/O channels. When 500 connections query the database simultaneously, the database OS spends most of its time performing thread context switches and disk head seeks, degrading throughput. Sizing the pool to match physical hardware constraints (`cores * 2 + spindles`) ensures tasks execute without queue thrashing, delivering maximum aggregate TPS.
</details>

<details>
<summary>3. What is the difference between Non-Repeatable Read and Phantom Read?</summary>

- **Non-Repeatable Read**: Occurs on **existing rows**. Transaction A reads a row; Transaction B updates or deletes that same row and commits. Transaction A re-reads the row and finds modified or deleted data.
- **Phantom Read**: Occurs on **range queries**. Transaction A reads a set of rows matching a condition (`WHERE age > 30`); Transaction B inserts **new rows** matching that condition and commits. Transaction A re-reads and discovers new "phantom" rows.
</details>

<details>
<summary>4. What is the N+1 Query Problem in ORM frameworks and how do you resolve it?</summary>

The N+1 problem occurs when querying a parent entity with a one-to-many relationship. The ORM executes 1 query to fetch $N$ parent records, and then executes $N$ separate queries to fetch the children of each parent ($1 + N$ queries).
**Resolutions**:
1. Use `JOIN FETCH` in JPQL/HQL to retrieve parents and children in a single SQL JOIN.
2. Use `@EntityGraph` (JPA 2.1+).
3. Use Hibernate's `@BatchSize(size = 50)` to fetch children in batches using `IN (...)`.
</details>

<details>
<summary>5. Explain the 4 JPA Entity Lifecycle States.</summary>

1. **Transient**: Created using `new`; not associated with an `EntityManager` and has no database representation.
2. **Managed**: Associated with an active `EntityManager`. Changes to fields are tracked via dirty checking and synchronized upon flush/commit.
3. **Detached**: Previously managed, but the persistence context has closed or the entity was detached. Changes are not tracked.
4. **Removed**: Marked for SQL `DELETE` upon transaction commit.
</details>

<details>
<summary>6. What is the First-Level Cache in JPA/Hibernate?</summary>

The First-Level Cache is the **Persistence Context** bound to the current `EntityManager` or transaction. It guarantees:
1. **Identity Equality**: Multiple lookups for the same primary key in the same transaction return the exact same Java object reference (`a == b`).
2. **Query Reduction**: Subsequent calls to `em.find(User.class, 1L)` do not query the database if the entity is already in the persistence context.
</details>

<details>
<summary>7. What is Optimistic Locking vs Pessimistic Locking?</summary>

- **Optimistic Locking**: Assumes collisions are rare. Uses a `@Version` column (number or timestamp). When updating, the SQL includes `WHERE version = currentVersion`. If another transaction updated the row first, zero rows are affected, throwing `OptimisticLockException`.
- **Pessimistic Locking**: Assumes collisions are frequent. Locks the row at the database level using `SELECT ... FOR UPDATE` (Pessimistic Write lock), blocking all other transactions from modifying or reading until the lock is released.
</details>

<details>
<summary>8. How do you stream millions of rows from a database without causing an `OutOfMemoryError`?</summary>

By default, JDBC drivers buffer the entire query result set into memory before returning control to Java. To stream:
1. Set the statement result set type to forward-only: `ResultSet.TYPE_FORWARD_ONLY`.
2. Configure a fetch size: `statement.setFetchSize(500)`.
3. For MySQL, set `statement.setFetchSize(Integer.MIN_VALUE)` to enable streaming mode.
4. Process rows iteratively inside a `while (rs.next())` loop without accumulating them into memory.
</details>

---

## 11. Practice Exercises

### Exercise 1: Implement an In-Memory Connection Pool
Create a simple connection pool wrapper around a `BlockingQueue<Connection>`.

<details>
<summary>Solution</summary>

```java
import java.sql.Connection;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.TimeUnit;

public class BasicConnectionPool {
    private final BlockingQueue<Connection> pool;

    public BasicConnectionPool(int poolSize, ConnectionFactory factory) throws Exception {
        this.pool = new ArrayBlockingQueue<>(poolSize);
        for (int i = 0; i < poolSize; i++) {
            pool.offer(factory.createConnection());
        }
    }

    public Connection borrowConnection(long timeoutMs) throws Exception {
        Connection conn = pool.poll(timeoutMs, TimeUnit.MILLISECONDS);
        if (conn == null) throw new RuntimeException("Connection pool timed out");
        return conn;
    }

    public void returnConnection(Connection conn) {
        if (conn != null) {
            pool.offer(conn);
        }
    }

    @FunctionalInterface
    public interface ConnectionFactory {
        Connection createConnection() throws Exception;
    }
}
```
</details>

### Exercise 2: Diagnose the SQL Injection Vulnerability
Identify the vulnerability and rewrite using `PreparedStatement`:
```java
public User authenticate(String user, String pass) throws SQLException {
    String query = "SELECT * FROM users WHERE username = '" + user + "' AND password = '" + pass + "'";
    Statement stmt = conn.createStatement();
    ResultSet rs = stmt.executeQuery(query);
    // ...
}
```

<details>
<summary>Solution</summary>

**Vulnerability**: An attacker can provide input: `user = "admin' -- "` which rewrites the query to:
`SELECT * FROM users WHERE username = 'admin' -- ' AND password = '...'`
The `--` comments out the password verification, authenticating the attacker as administrator.

**Fix**:
```java
public User authenticate(String user, String pass) throws SQLException {
    String sql = "SELECT id, username, email FROM users WHERE username = ? AND password_hash = ?";
    try (PreparedStatement ps = conn.prepareStatement(sql)) {
        ps.setString(1, user);
        ps.setString(2, hashPassword(pass));
        try (ResultSet rs = ps.executeQuery()) {
            if (rs.next()) {
                return new User(rs.getLong("id"), rs.getString("username"), rs.getString("email"));
            }
        }
    }
    return null;
}
```
</details>

### Exercise 3: Implement an Idempotent Database Upsert
Write a method that inserts or updates an account balance atomically using modern SQL dialect or transaction savepoints.

<details>
<summary>Solution</summary>

```java
import java.sql.Connection;
import java.sql.PreparedStatement;
import java.sql.SQLException;

public class IdempotentUpsert {
    public static void upsertAccountBalance(Connection conn, String accountId, double balance) throws SQLException {
        // PostgreSQL / SQLite UPSERT idiom
        String sql = "INSERT INTO accounts (account_id, balance, updated_at) " +
                     "VALUES (?, ?, CURRENT_TIMESTAMP) " +
                     "ON CONFLICT (account_id) " +
                     "DO UPDATE SET balance = EXCLUDED.balance, updated_at = CURRENT_TIMESTAMP";

        try (PreparedStatement ps = conn.prepareStatement(sql)) {
            ps.setString(1, accountId);
            ps.setDouble(2, balance);
            ps.executeUpdate();
        }
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **PreparedStatement**: Neutralizes SQL Injection via binary parameter separation; enables query plan caching.
- **HikariCP**: Lock-free `ConcurrentBag` connection acquisition; size pool conservatively (`cores * 2 + 1`).
- **Isolation Levels**: `READ COMMITTED` prevents dirty reads; `REPEATABLE READ` prevents non-repeatable reads; `SERIALIZABLE` eliminates all anomalies.
- **N+1 Solution**: Use `JOIN FETCH` or `@EntityGraph` to fetch relations in single SQL queries.
- **Streaming Large Data**: Use `setFetchSize(500)` to stream results without memory exhaustion.

---

## 13. Further Reading

- [Brett Wooldridge: About Pool Sizing (HikariCP Documentation)](https://github.com/brettwooldridge/HikariCP/wiki/About-Pool-Sizing)
- [Vlad Mihalcea: High-Performance Java Persistence](https://vladmihalcea.com/books/high-performance-java-persistence/)
- [Baeldung: Guide to JDBC PreparedStatement](https://www.baeldung.com/java-prepared-statement)
- [ANSI/ISO SQL Transaction Isolation Standards](https://en.wikipedia.org/wiki/Isolation_(database_systems))
