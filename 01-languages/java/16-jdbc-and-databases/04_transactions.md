# Database Transactions and Isolation Levels

> ACID transaction guarantees, programmatic transaction demarcation, and ANSI SQL isolation anomalies in high-concurrency Java enterprise architectures.

| Metadata | Details |
|---|---|
| **Level** | Core / Advanced |
| **Prerequisites** | JDBC Fundamentals, Concurrency Basics |
| **Time to Read** | 30 minutes |
| **Status** | Complete |
| **JDK Baseline** | Java 17 / 21 / 25 LTS |

---

## Table of Contents
1. [1. What & Why](#1-what--why)
2. [2. Core Concepts](#2-core-concepts)
3. [3. How It Works Internally: WAL, Lock Managers & MVCC](#3-how-it-works-internally-wal-lock-managers--mvcc)
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
In enterprise business applications, a single logical business operation frequently requires updating multiple relational records across different tables. If an error occurs halfway through—such as debiting account A before credit to account B fails—the database must guarantee that partial changes are cleanly undone.

A **Transaction** is an atomic unit of work providing ACID guarantees (Atomicity, Consistency, Isolation, Durability). In JDBC, transaction control is managed directly on the `Connection` object via `setAutoCommit(false)`, `commit()`, and `rollback()`. Concurrently running transactions face concurrency phenomena (dirty reads, non-repeatable reads, phantom reads, and lost updates). Selecting the proper isolation level balances strict data correctness against transaction throughput and lock contention.

---

## 2. Core Concepts
- **ACID Guarantees**:
  - **Atomicity**: All operations succeed or none do ("all-or-nothing").
  - **Consistency**: Database transitions from one valid state satisfying all declarative constraints to another.
  - **Isolation**: Concurrent transactions execute without mutual interference.
  - **Durability**: Committed data survives system crashes, power failures, and OS halts.
- **AutoCommit**: JDBC defaults to `autoCommit = true`, committing every individual SQL statement automatically. Disabling it initiates a manual transaction boundary.
- **Savepoints**: Intermediate checkpoint markers within an uncommitted transaction allowing partial rollback without aborting the entire transaction.
- **Isolation Levels (ANSI SQL)**:
  1. `TRANSACTION_READ_UNCOMMITTED`: Allows reading uncommitted changes (dirty reads).
  2. `TRANSACTION_READ_COMMITTED`: Reads only committed data; allows non-repeatable reads and phantom reads.
  3. `TRANSACTION_REPEATABLE_READ`: Guarantees consistent reads within the same transaction; prevents non-repeatable reads.
  4. `TRANSACTION_SERIALIZABLE`: Highest isolation; eliminates all concurrency anomalies by enforcing serializable order.

---

## 3. How It Works Internally: WAL, Lock Managers & MVCC

Under the hood, relational engines ensure Atomicity and Durability using a Write-Ahead Log (WAL / Redo Log) and maintain Isolation via Multi-Version Concurrency Control (MVCC) and Lock Managers:

```
+-------------------------------------------------------+
|                 JDBC Client Thread                    |
|       conn.setAutoCommit(false); ps.executeUpdate();  |
+-------------------------------------------------------+
                           |
                           v
+-------------------------------------------------------+
|             Database Engine MVCC & Engine             |
|  1. Writes undo/redo record into WAL Buffer Cache     |
|  2. Modifies data page in Shared Buffers memory       |
|  3. Transaction ID assigned (xmin, xmax in Postgres)  |
+-------------------------------------------------------+
                           |
                  conn.commit();
                           |
                           v
+-------------------------------------------------------+
|              Synchronous WAL Disk Flush               |
|  Flushes WAL buffer to physical SSD disk (fsync)      |
|  Transaction status marked COMMITTED in pg_xact       |
+-------------------------------------------------------+
```

### MVCC vs Two-Phase Locking (2PL)
- **Two-Phase Locking (2PL)**: Traditional locking engines (e.g., standard SQL Server without RCSI). Readers acquire Shared (S) locks, blocking writers; writers acquire Exclusive (X) locks, blocking readers and writers. High lock contention.
- **Multi-Version Concurrency Control (MVCC)**: Modern engines (PostgreSQL, MySQL InnoDB, Oracle). "Readers never block writers, and writers never block readers." When a row is modified, a new version is created with a timestamp/transaction ID. Readers query the snapshot matching their transaction start time without locking table rows.

---

## 4. Syntax & Usage

### 4.1 Programmatic JDBC Transaction Demarcation
```java
package com.example.jdbc;

import java.math.BigDecimal;
import java.sql.Connection;
import java.sql.PreparedStatement;
import java.sql.SQLException;
import javax.sql.DataSource;

public final class BankTransferService {

    public static void transferFunds(DataSource ds, long fromId, long toId, BigDecimal amount) throws SQLException {
        String debitSql = "UPDATE accounts SET balance = balance - ? WHERE id = ? AND balance >= ?";
        String creditSql = "UPDATE accounts SET balance = balance + ? WHERE id = ?";

        try (Connection conn = ds.getConnection()) {
            // Step 1: Disable autocommit to demarcate transaction start
            conn.setAutoCommit(false);
            conn.setTransactionIsolation(Connection.TRANSACTION_READ_COMMITTED);

            try (PreparedStatement psDebit = conn.prepareStatement(debitSql);
                 PreparedStatement psCredit = conn.prepareStatement(creditSql)) {

                // Execute Debit
                psDebit.setBigDecimal(1, amount);
                psDebit.setLong(2, fromId);
                psDebit.setBigDecimal(3, amount);
                int debitRows = psDebit.executeUpdate();
                if (debitRows == 0) {
                    throw new IllegalStateException("Insufficient balance or sender account not found: " + fromId);
                }

                // Execute Credit
                psCredit.setBigDecimal(1, amount);
                psCredit.setLong(2, toId);
                int creditRows = psCredit.executeUpdate();
                if (creditRows == 0) {
                    throw new IllegalStateException("Recipient account not found: " + toId);
                }

                // Step 2: Explicitly commit all changes
                conn.commit();
                System.out.println("Transfer committed successfully.");

            } catch (Exception ex) {
                // Step 3: Explicitly rollback on any failure
                conn.rollback();
                System.err.println("Transaction rolled back due to error: " + ex.getMessage());
                throw (ex instanceof SQLException sqlEx) ? sqlEx : new SQLException(ex);
            } finally {
                // Step 4: Always restore autocommit before returning connection to pool!
                conn.setAutoCommit(true);
            }
        }
    }
}
```

### 4.2 Savepoint Mechanics
```java
package com.example.jdbc;

import java.sql.*;

public final class SavepointDemo {
    public static void executeWithSavepoint(Connection conn) throws SQLException {
        conn.setAutoCommit(false);

        try (PreparedStatement ps1 = conn.prepareStatement("INSERT INTO logs (msg) VALUES ('Task A')");
             PreparedStatement ps2 = conn.prepareStatement("INSERT INTO logs (msg) VALUES ('Task B')")) {

            ps1.executeUpdate();
            Savepoint sp = conn.setSavepoint("Savepoint_After_A");

            try {
                ps2.executeUpdate();
                // If special condition triggers, rollback only Task B:
                boolean taskBFallbackNeeded = true;
                if (taskBFallbackNeeded) {
                    conn.rollback(sp); // Task A remains intact!
                }
            } catch (SQLException ex) {
                conn.rollback(sp); // Undo only Task B
            }

            conn.commit(); // Task A is committed
        } finally {
            conn.setAutoCommit(true);
        }
    }
}
```

---

## 5. Practical Examples

### Example 1: Basic Isolation Level Configuration
```java
package com.example.jdbc;

import java.sql.Connection;
import java.sql.SQLException;

public final class IsolationDemo {
    public static void configureSerializable(Connection conn) throws SQLException {
        if (conn.getMetaData().supportsTransactionIsolationLevel(Connection.TRANSACTION_SERIALIZABLE)) {
            conn.setTransactionIsolation(Connection.TRANSACTION_SERIALIZABLE);
        } else {
            conn.setTransactionIsolation(Connection.TRANSACTION_REPEATABLE_READ);
        }
    }
}
```

### Example 2: Real-World Serializable Retry Loop
```java
package com.example.jdbc;

import java.sql.Connection;
import java.sql.SQLException;
import javax.sql.DataSource;

public final class SerializableRetryManager {
    // When using SERIALIZABLE isolation, concurrent conflicts throw serialization failure (SQLState 40001)
    public static void executeSerializableWithRetry(DataSource ds, RunnableTx tx, int maxRetries) throws SQLException {
        int attempts = 0;
        while (true) {
            attempts++;
            try (Connection conn = ds.getConnection()) {
                conn.setAutoCommit(false);
                conn.setTransactionIsolation(Connection.TRANSACTION_SERIALIZABLE);
                try {
                    tx.run(conn);
                    conn.commit();
                    return; // Success
                } catch (SQLException ex) {
                    conn.rollback();
                    // PostgreSQL 40001 = serialization_failure, 40P01 = deadlock_detected
                    if ("40001".equals(ex.getSQLState()) && attempts < maxRetries) {
                        System.out.printf("Serialization conflict, retrying attempt %d...%n", attempts);
                        try { Thread.sleep((long) (Math.random() * 50 * attempts)); } catch (InterruptedException ignored) {}
                        continue;
                    }
                    throw ex;
                }
            }
        }
    }

    @FunctionalInterface
    public interface RunnableTx {
        void run(Connection conn) throws SQLException;
    }
}
```

### Example 3: Edge Case - Read-Only Transaction Hint
```java
package com.example.jdbc;

import java.sql.Connection;
import java.sql.SQLException;

public final class ReadOnlyTxOptimization {
    public static void configureReadOnly(Connection conn) throws SQLException {
        conn.setAutoCommit(false);
        // Informs driver and DB engine that no writes will occur;
        // Allows engine to optimize locks and route queries to read-replicas
        conn.setReadOnly(true);
    }
}
```

### Example 4: Failure Case - Connection Leak with Unclosed Transaction
```java
// ANTI-PATTERN: Forgetting rollback on exception leaves locks open
public void brokenTransaction(DataSource ds) throws SQLException {
    Connection conn = ds.getConnection();
    conn.setAutoCommit(false);
    // If exception occurs here, connection is returned to pool WITH PENDING LOCKS!
    // Next thread borrowing connection inherits dirty transaction state!
    throw new RuntimeException("Unexpected crash");
}
```

---

## 6. Best Practices
1. **Always Reset AutoCommit in Finally**: Always restore `conn.setAutoCommit(true)` before closing connection to prevent poisoning connection pool.
2. **Always Rollback in Catch Block**: Never return an uncommitted connection to a connection pool without rolling back.
3. **Keep Transactions as Short as Possible**: Holding database locks across network calls, file I/O, or heavy CPU tasks cripples database concurrency.
4. **Acquire Locks in Consistent Order**: Always update entities in a deterministic order (e.g., sorted by primary key) to avoid deadlocks.
5. **Use Read-Committed as Default**: `READ_COMMITTED` provides optimal balance of concurrency and consistency for 95% of enterprise applications.
6. **Implement Retry Logic for Serializable/Deadlocks**: When using `SERIALIZABLE` or optimistic locking, anticipate SQLState `40001` and retry.
7. **Mark Reporting Queries as Read-Only**: Use `conn.setReadOnly(true)` to assist replication routing and eliminate write lock overhead.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Correction |
|---|---|---|
| Returning uncommitted connection to pool | Next thread that borrows connection inherits uncommitted state | Always call `rollback()` on error and reset `autoCommit` |
| Long-running operations inside transaction | Holds row locks and exhaust connection pool slots | Move external REST calls and heavy CPU outside transaction |
| Assuming `SERIALIZABLE` means zero concurrency errors | Triggers serialization failures on conflict | Implement automatic exponential backoff retry loop |
| Inconsistent lock acquisition order | Thread A locks row 1 then 2; Thread B locks 2 then 1; causes deadlock | Sort keys before acquiring locks |
| Modifying isolation level mid-transaction | Many database engines reject changing isolation level during active transaction | Set isolation level before executing statements |
| Relying on autocommit for multi-step workflows | Partial failures leave database corrupted | Wrap multi-step workflows in `setAutoCommit(false)` |
| Missing `FOR UPDATE` in Read-Committed | Allows lost updates during read-modify-write workflows | Use optimistic locking or `SELECT ... FOR UPDATE` |
| Forgetting to release savepoints | Accumulates memory structures in database engine | Call `conn.releaseSavepoint(sp)` when done |

---

## 8. Performance & Security Notes
- **WAL fsync Bottleneck**: Every `conn.commit()` forces the database engine to synchronously flush write-ahead log buffers to physical storage (`fsync`). High transaction rates require battery-backed NVMe storage or group commit batching.
- **Deadlock Detection**: Database engines run background deadlock detection cycles (e.g., every 1,000 ms). When cycles are found, one transaction is forcibly aborted with an error (PostgreSQL SQLState `40P01`).
- **Distributed Transactions (XA)**: Two-Phase Commit (`javax.transaction.xa.XAResource`) coordinates transactions across multiple heterogeneous databases or message queues, but incurs substantial coordinator latency. Avoid unless strictly required.
- **Connection Poisoning**: If an application checks in a connection with dirty transaction state, HikariCP executes a safety rollback and resets state automatically, logging an eviction warning.

---

## 9. Comparison / When to Use What

| ANSI Isolation Level | Dirty Read | Non-Repeatable Read | Phantom Read | Concurrency Throughput | Best Use Case |
|---|---|---|---|---|---|
| **Read Uncommitted** | Possible | Possible | Possible | Maximum | Non-critical analytics / dirty telemetry |
| **Read Committed** | Prevented | Possible | Possible | High | Standard enterprise CRUD microservices |
| **Repeatable Read** | Prevented | Prevented | Prevented (in MVCC) | Moderate | Financial statements, consistent multi-table reads |
| **Serializable** | Prevented | Prevented | Prevented | Lowest (Abort rate) | Complex financial balance reconciliations |

---

## 10. Interview Questions

<details>
<summary>1. What are the three classic ANSI SQL concurrency read phenomena?</summary>
1. <b>Dirty Read</b>: Transaction A reads data modified by Transaction B before B has committed. If B rolls back, A has read data that never existed.<br>
2. <b>Non-Repeatable Read (Fuzzy Read)</b>: Transaction A reads a row, Transaction B modifies or deletes that row and commits, and Transaction A reads the row again, observing modified values.<br>
3. <b>Phantom Read</b>: Transaction A queries a range of rows matching a predicate, Transaction B inserts a new row matching that predicate and commits, and Transaction A re-executes the query, observing a new "phantom" row.
</details>

<details>
<summary>2. How does MVCC prevent readers from blocking writers and writers from blocking readers?</summary>
In an MVCC database engine (like PostgreSQL or InnoDB), updating or deleting a row does not overwrite the existing disk block in place; it writes a new version of the row tagged with transaction visibility IDs. A reading transaction queries a point-in-time snapshot of the database corresponding to its start timestamp, reading the older immutable row version without acquiring locks, while a concurrent writer creates new row versions simultaneously.
</details>

<details>
<summary>3. Why must an application implement a retry loop when using SERIALIZABLE isolation?</summary>
Serializable isolation guarantees that concurrent transactions produce the same result as if executed sequentially. In modern optimistic MVCC engines (Serializable Snapshot Isolation - SSI), the database does not acquire coarse table locks; instead, it tracks dependency graphs. If concurrent read-write dependencies form a dangerous cycle, the engine aborts one of the conflicting transactions with a serialization failure (SQLState 40001), expecting the application to retry.
</details>

<details>
<summary>4. What is a Savepoint in JDBC, and how is it used?</summary>
A <code>Savepoint</code> defines an intermediate checkpoint within a transaction. By calling <code>conn.setSavepoint("sp1")</code>, the application can later invoke <code>conn.rollback(sp1)</code> to undo only changes executed after that savepoint, preserving earlier statements within the transaction before eventually calling <code>conn.commit()</code>.
</details>

<details>
<summary>5. What is the difference between Optimistic Locking and Pessimistic Locking?</summary>
<b>Pessimistic Locking</b> uses database locks (e.g., <code>SELECT ... FOR UPDATE</code>) to lock rows at read time, blocking concurrent access until transaction commit. Best for high-contention, short-duration transactions.<br>
<b>Optimistic Locking</b> does not lock rows at read time; instead, it tracks a version number (<code>version INT</code>) and checks <code>WHERE id = :id AND version = :version</code> on update. Best for low-contention, long-duration business workflows.
</details>

<details>
<summary>6. What happens if a connection is returned to a connection pool while a transaction is still active?</summary>
If returned with uncommitted modifications, the connection retains database locks. Production connection pools like HikariCP intercept <code>close()</code>, detect uncommitted transactions, invoke <code>rollback()</code>, reset <code>autoCommit = true</code>, and emit a warning log to prevent connection state poisoning.
</details>

<details>
<summary>7. How does Two-Phase Commit (2PC / XA) work across distributed databases?</summary>
In 2PC, a Transaction Manager coordinates multiple resource managers: Phase 1 (Prepare): The coordinator asks all nodes to prepare and write changes to disk; each votes YES or NO. Phase 2 (Commit): If all vote YES, coordinator commands COMMIT; if any node votes NO or times out, coordinator commands ROLLBACK across all nodes.
</details>

<details>
<summary>8. Why is it dangerous to perform HTTP calls inside a database transaction?</summary>
HTTP requests across the internet or local networks have unpredictable latency (tens to thousands of milliseconds) and can stall due to network hiccups. Holding a database transaction open during an external network call keeps row locks and connection pool slots occupied, causing rapid connection pool exhaustion and deadlocking the entire backend.
</details>

<details>
<summary>9. What is a Lost Update anomaly, and which isolation level prevents it?</summary>
A lost update occurs when Transaction A and Transaction B simultaneously read a row with balance 100, Transaction A subtracts 20 and writes 80, and Transaction B adds 50 and writes 150, overwriting and losing Transaction A's update. Prevented by <code>REPEATABLE READ</code>, <code>SERIALIZABLE</code>, or pessimistic <code>SELECT ... FOR UPDATE</code>.
</details>

<details>
<summary>10. What is connection poisoning in pooled architectures?</summary>
Connection poisoning occurs when application code modifies connection session attributes (such as isolation level, catalog, read-only mode, or transaction state) without restoring default values before closing the connection, causing subsequent threads that borrow the connection to execute in an unexpected state.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Write a transactional method implementing pessimistic locking with SELECT FOR UPDATE.</summary>

```java
public static void deductStockPessimistic(Connection conn, long itemId, int qty) throws SQLException {
    String selectSql = "SELECT stock_count FROM inventory WHERE id = ? FOR UPDATE";
    String updateSql = "UPDATE inventory SET stock_count = stock_count - ? WHERE id = ?";

    try (PreparedStatement psSelect = conn.prepareStatement(selectSql);
         PreparedStatement psUpdate = conn.prepareStatement(updateSql)) {

        psSelect.setLong(1, itemId);
        try (ResultSet rs = psSelect.executeQuery()) {
            if (!rs.next()) throw new IllegalStateException("Item not found: " + itemId);
            int currentStock = rs.getInt(1);
            if (currentStock < qty) throw new IllegalStateException("Insufficient inventory: " + currentStock);
        }

        psUpdate.setInt(1, qty);
        psUpdate.setLong(2, itemId);
        psUpdate.executeUpdate();
    }
}
```
</details>

<details>
<summary>Exercise 2: Implement an optimistic concurrency control update checking row version.</summary>

```java
public static boolean updateOrderOptimistic(Connection conn, long orderId, String newStatus, long expectedVersion) throws SQLException {
    String sql = "UPDATE orders SET status = ?, version = version + 1 WHERE id = ? AND version = ?";
    try (PreparedStatement ps = conn.prepareStatement(sql)) {
        ps.setString(1, newStatus);
        ps.setLong(2, orderId);
        ps.setLong(3, expectedVersion);
        int rowsUpdated = ps.executeUpdate();
        return rowsUpdated > 0; // True if update succeeded without concurrent conflict
    }
}
```
</details>

<details>
<summary>Exercise 3: Write a transaction template executing a Supplier functional interface safely.</summary>

```java
public final class TxTemplate {
    public static <T> T executeInTransaction(DataSource ds, TxAction<T> action) throws SQLException {
        try (Connection conn = ds.getConnection()) {
            conn.setAutoCommit(false);
            try {
                T result = action.execute(conn);
                conn.commit();
                return result;
            } catch (Exception ex) {
                conn.rollback();
                throw (ex instanceof SQLException sqlEx) ? sqlEx : new SQLException(ex);
            } finally {
                conn.setAutoCommit(true);
            }
        }
    }

    @FunctionalInterface
    public interface TxAction<T> {
        T execute(Connection conn) throws Exception;
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet
- **ACID**: Atomicity (all-or-nothing), Consistency (rules satisfied), Isolation (concurrency control), Durability (WAL fsync).
- **Transaction Demarcation**: `conn.setAutoCommit(false)`, `conn.commit()`, `conn.rollback()`.
- **Cleanup**: Always reset `conn.setAutoCommit(true)` in a `finally` block before connection close.
- **Isolation Hierarchy**: `READ_COMMITTED` (standard) $ightarrow$ `REPEATABLE_READ` $ightarrow$ `SERIALIZABLE` (requires retries).
- **Concurrency Control**: MVCC provides non-blocking reads; use `SELECT ... FOR UPDATE` or version checking for write protection.
- **External Calls**: Never invoke external REST endpoints or long compute loops inside active database transactions.

---

## 13. Further Reading
- ANSI/ISO SQL Standard: Transaction Isolation Models
- PostgreSQL Documentation: Concurrency Control and MVCC Internals
- *Designing Data-Intensive Applications* by Martin Kleppmann (Transactions and Consistency)
- *High-Performance Java Persistence* by Vlad Mihalcea (Transactions and Concurrency Control)
