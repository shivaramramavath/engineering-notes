# Batching & Bulk Operations: JDBC Batching, Sequence Allocators & High-Throughput Ingestion

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | `03-spring-data-jpa-fundamentals.md`, `06-jpql-native-and-specifications.md`, `14-transactions.md` |
| **Estimated Reading Time** | 30 minutes |
| **Status** | Active |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework** | 6.2.x / 7.0.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven / Gradle |
| **Tested With** | Hibernate 6.6.x, PostgreSQL 16, Spring Boot 3.4.x |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
   - [5.1 Production Batch Configuration with Ordered Inserts and Updates](#51-production-batch-configuration-with-ordered-inserts-and-updates)
   - [5.2 Sequence Pooling Allocation with allocationSize](#52-sequence-pooling-allocation-with-allocationsize)
   - [5.3 Chunked Entity Ingestion with Periodic Flush and Clear](#53-chunked-entity-ingestion-with-periodic-flush-and-clear)
   - [5.4 Anti-Pattern: GenerationType.IDENTITY Disables Batching](#54-anti-pattern-generationtypeidentity-disables-batching)
   - [5.5 High-Speed Bulk Processing with Hibernate StatelessSession](#55-high-speed-bulk-processing-with-hibernate-statelesssession)
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
Inserting or updating 50,000 entities naively through standard JPA repository loops (`for (...) repo.save(entity)`) takes minutes, consumes gigabytes of heap memory, and risks crashing the JVM with an `OutOfMemoryError`. Each `INSERT` or `UPDATE` is sent as a standalone network packet to the database, creating massive round-trip network latency. Furthermore, Hibernate tracks all 50,000 entity instances and their dirty-checking snapshots inside the First-Level Cache simultaneously.

### Historical Evolution
JDBC natively supports batch operations via `PreparedStatement.addBatch()` and `executeBatch()`, which packs dozens of SQL statements into a single network packet. Hibernate provides JDBC batching support, but historically required complex configuration (`order_inserts`, `order_updates`) to prevent batch fragmentation. Most importantly, developers frequently hit a hidden roadblock: using **`GenerationType.IDENTITY` silently disables JDBC batch inserts**, forcing Hibernate into single-statement execution.

### Real-World Relevance
Bulk data ingestion, daily CSV/Excel imports, batch financial reconciliations, and background data synchronization pipelines depend on batching. Configuring JDBC batching and sequence pooling increases insertion speeds from 200 records/second to over 25,000 records/second.

---

## 2. Core Concepts

### JDBC Batching
Instead of sending an individual SQL statement per entity, JDBC groups multiple parameter sets into an internal buffer and dispatches them to the database engine in a single network round-trip.

### The Identity Batching Trap
`GenerationType.IDENTITY` relies on the database's auto-incrementing column (e.g., PostgreSQL `SERIAL` / `IDENTITY`, MySQL `AUTO_INCREMENT`). Because the primary key is generated only *after* the row is physically inserted into the table, Hibernate must execute the `INSERT` immediately upon `persist()` to obtain the generated ID and assign it to the entity. **This completely disables JDBC batching for inserts!**

### Sequence Hi/Lo & Pooled Allocation
`GenerationType.SEQUENCE` uses database sequences (`CREATE SEQUENCE ...`). By configuring `allocationSize = 50`, Hibernate fetches a block of 50 IDs in a single database sequence call and allocates them in-memory across the next 50 entities. Because entity IDs are assigned prior to executing SQL, Hibernate can safely queue and batch all 50 `INSERT` statements together.

### Hibernate StatelessSession
A specialized, lightweight session abstraction designed specifically for bulk operations. A `StatelessSession` has:
- No First-Level Cache (L1).
- No dirty checking.
- No second-level cache interaction.
- Immediate, direct execution of SQL statements.

---

## 3. How It Works Internally

```
+------------------------------------------------------------------------------------+
|                         JDBC Batching Execution Mechanics                          |
+------------------------------------------------------------------------------------+
|                                                                                    |
|  Hibernate ActionQueue (Sorted by entity type and statement type)                  |
|  +-------------------------------------------------------------------------------+ |
|  | InsertAction(User#1) -> InsertAction(User#2) -> ... -> InsertAction(User#50)  | |
|  +-------------------------------------------------------------------------------+ |
|         |                                                                          |
|         v                                                                          |
|  PreparedStatement.addBatch()                                                      |
|  - Injects parameters for User 1 into JDBC batch buffer                            |
|  - Injects parameters for User 2 into JDBC batch buffer                            |
|  - ... repeats up to hibernate.jdbc.batch_size (e.g. 50)                           |
|         |                                                                          |
|         v (When batch buffer hits 50 items)                                        |
|  PreparedStatement.executeBatch()                                                  |
|  - Dispatches 1 single network packet containing 50 inserts to PostgreSQL/MySQL    |
|  - Database executes batch in parallel / bulk engine                               |
|  - Single network ACK response returned to application                             |
|                                                                                    |
+------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### Enabling JDBC Batching in `application.yaml`
```yaml
spring:
  jpa:
    properties:
      hibernate:
        jdbc:
          batch_size: 50
        order_inserts: true
        order_updates: true
        batch_versioned_data: true
  datasource:
    hikari:
      data-source-properties:
        # PostgreSQL driver optimization: rewrites batched inserts into multi-row VALUES
        reWriteBatchedInserts: true
```

---

## 5. Practical Examples

### 5.1 Production Batch Configuration with Ordered Inserts and Updates
```yaml
# =========================================================================
# PRODUCTION BATCHING CONFIGURATION (PostgreSQL)
# =========================================================================
spring:
  jpa:
    properties:
      hibernate:
        jdbc:
          batch_size: 50
        # Orders statement execution by entity class to prevent batch fragmentation
        order_inserts: true
        order_updates: true
        # Allows versioned (@Version) optimistic locking updates to participate in batching
        batch_versioned_data: true
        # Logs metrics verifying batch execution
        generate_statistics: true
  datasource:
    url: jdbc:postgresql://localhost:5432/mydb?reWriteBatchedInserts=true
```

### 5.2 Sequence Pooling Allocation with allocationSize
```java
package com.example.batch.domain;

import jakarta.persistence.*;

@Entity
@Table(name = "invoices")
public class Invoice {

    /**
     * CRITICAL FOR BATCHING:
     * allocationSize = 50 informs Hibernate that the database sequence increments by 50.
     * Hibernate queries nextval(invoice_seq) once, and assigns IDs in-memory for the next 50 entities!
     * This allows JDBC batching to function at maximum speed!
     */
    @Id
    @GeneratedValue(strategy = GenerationType.SEQUENCE, generator = "invoice_seq_gen")
    @SequenceGenerator(
        name = "invoice_seq_gen",
        sequenceName = "invoice_seq",
        allocationSize = 50
    )
    private Long id;

    @Column(nullable = false)
    private String invoiceNumber;

    @Column(nullable = false)
    private double amount;

    protected Invoice() {}

    public Invoice(String invoiceNumber, double amount) {
        this.invoiceNumber = invoiceNumber;
        this.amount = amount;
    }

    public Long getId() { return id; }
}
```

### 5.3 Chunked Entity Ingestion with Periodic Flush and Clear
```java
package com.example.batch.service;

import com.example.batch.domain.Invoice;
import jakarta.persistence.EntityManager;
import jakarta.persistence.PersistenceContext;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.util.List;

@Service
public class BulkInvoiceIngestionService {

    @PersistenceContext
    private EntityManager entityManager;

    private static final int BATCH_SIZE = 50;

    /**
     * Ingests 100,000 records without running out of heap memory.
     * Flushes SQL batches and clears the L1 Cache every 50 records.
     */
    @Transactional
    public void ingestInvoices(List<InvoiceDataDto> dtos) {
        for (int i = 0; i < dtos.size(); i++) {
            InvoiceDataDto dto = dtos.get(i);
            Invoice invoice = new Invoice(dto.number(), dto.amount());
            entityManager.persist(invoice);

            if (i > 0 && i % BATCH_SIZE == 0) {
                // 1. Flush: Sends the pending 50 statements as a single JDBC batch to DB
                entityManager.flush();
                // 2. Clear: Evicts the 50 managed entities from L1 Cache (frees JVM heap!)
                entityManager.clear();
            }
        }
        // Flush any remaining items in the final partial batch
        entityManager.flush();
        entityManager.clear();
    }
}
```

### 5.4 Anti-Pattern: GenerationType.IDENTITY Disables Batching
```java
// =========================================================================
// ANTI-PATTERN: GenerationType.IDENTITY used with JDBC Batching
// =========================================================================
@Entity
public class BadBatchItem {
    // DISASTER: IDENTITY forces Hibernate to immediately issue an INSERT
    // for every single persist() call in order to get the generated ID!
    // hibernate.jdbc.batch_size is COMPLETELY DISABLED!
    // Inserting 10,000 items executes 10,000 independent network round trips!
    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;
    
    private String name;
}

// =========================================================================
// PRODUCTION FIX: GenerationType.SEQUENCE with matching allocationSize
// =========================================================================
@Entity
public class GoodBatchItem {
    // OPTIMAL: Pre-allocates sequence IDs in memory; enables 100% JDBC batching!
    @Id
    @GeneratedValue(strategy = GenerationType.SEQUENCE, generator = "item_seq")
    @SequenceGenerator(name = "item_seq", sequenceName = "item_seq", allocationSize = 50)
    private Long id;
    
    private String name;
}
```

### 5.5 High-Speed Bulk Processing with Hibernate StatelessSession
```java
package com.example.batch.service;

import com.example.batch.domain.Invoice;
import jakarta.persistence.EntityManagerFactory;
import org.hibernate.SessionFactory;
import org.hibernate.StatelessSession;
import org.hibernate.Transaction;
import org.springframework.stereotype.Service;

import java.util.Iterator;
import java.util.stream.Stream;

@Service
public class UltraFastStatelessIngestionService {

    private final SessionFactory sessionFactory;

    public UltraFastStatelessIngestionService(EntityManagerFactory emf) {
        this.sessionFactory = emf.unwrap(SessionFactory.class);
    }

    /**
     * StatelessSession bypasses First-Level Caching, dirty checking, and event listeners.
     * Ideal for ultra-high-speed ETL pipelines inserting 500,000+ records.
     */
    public void ingestStreamStateless(Stream<Invoice> invoiceStream) {
        try (StatelessSession session = sessionFactory.openStatelessSession()) {
            Transaction tx = session.beginTransaction();
            session.setJdbcBatchSize(100);

            Iterator<Invoice> iterator = invoiceStream.iterator();
            while (iterator.hasNext()) {
                // Inserts immediately without caching or dirty tracking
                session.insert(iterator.next());
            }

            tx.commit();
        }
    }
}
```

---

## 6. Best Practices

1. **Always Use `GenerationType.SEQUENCE` for Batched Entities**: Never use `GenerationType.IDENTITY` if the entity is expected to undergo batch insertions.
2. **Set `order_inserts = true` and `order_updates = true`**: Prevents batch fragmentation when alternating between parent and child entity insertions.
3. **Configure Database Driver Batch Rewriting**: For PostgreSQL, add `reWriteBatchedInserts=true`; for MySQL, add `rewriteBatchedStatements=true` to the JDBC URL to merge individual inserts into multi-row SQL syntax (`VALUES (...), (...)`).
4. **Always Call `flush()` Followed by `clear()`**: In standard `EntityManager` batch loops, clear the persistence context every `batch_size` records to prevent JVM heap exhaustion.
5. **Use `StatelessSession` for Pure ETL / Migration Jobs**: If you do not need entity cascades, dirty checking, or optimistic locking checks, `StatelessSession` provides the highest possible insertion throughput.
6. **Prefer `@Modifying` Queries for Bulk Updates**: Update millions of records using a single `UPDATE ... WHERE` statement rather than iterating and modifying managed entities.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Using `GenerationType.IDENTITY`** | Hibernate must execute single SQL statements immediately to fetch IDs, disabling batching entirely. | Migrate to `GenerationType.SEQUENCE` with `allocationSize >= 25`. |
| **Omitting `entityManager.clear()` in Large Loops** | The First-Level Cache retains every persisted entity, causing an `OutOfMemoryError`. | Call `entityManager.flush(); entityManager.clear();` every batch interval. |
| **Missing `order_inserts = true`** | Interleaving inserts of different entity types breaks the JDBC batch buffer, triggering premature flushes. | Enable `order_inserts` and `order_updates` in properties. |
| **Missing `reWriteBatchedInserts=true` (PostgreSQL)** | Driver sends multiple individual `INSERT` commands over the socket instead of a consolidated multi-row statement. | Add `?reWriteBatchedInserts=true` to the PostgreSQL JDBC URL. |
| **Mismatched Database Sequence Increment** | If `@SequenceGenerator(allocationSize = 50)` but the database sequence was created with `INCREMENT BY 1`, duplicate key collisions occur! | Ensure the database sequence has `INCREMENT BY 50`. |
| **Executing Heavy Batch Inserts Across Long Transactions** | Holding locks on millions of rows for minutes degrades database performance and blocks other users. | Chunk bulk processing into smaller, independent transactions (e.g., 5,000 records per transaction). |

---

## 8. Performance & Security Notes

### Performance
- **Network Round-Trip Compression**: A batch size of 50 reduces 50,000 round-trips to just 1,000 round-trips, slashing total execution time from minutes to seconds.
- **Multi-Row SQL Generation**: When `reWriteBatchedInserts=true` is enabled, PostgreSQL converts batch statements into:
  ```sql
  INSERT INTO invoices (id, amount) VALUES (1, 100), (2, 200), (3, 300)...;
  ```
  This reduces query parsing overhead on the database server.

### Security
- **Denial of Service via Huge Unchunked Batches**: Accepting bulk CSV uploads with 500,000 rows without streaming or pagination will exhaust server heap memory and trigger GC freeze. Enforce stream-based chunking.

---

## 9. Comparison / When to Use What

| Technique | Memory Overhead | Complexity | Speed | Best Use Case |
| :--- | :--- | :--- | :--- | :--- |
| **EntityManager Loop (No Batch)** | Very High | Lowest | Very Slow (~200 ops/s) | Few records (< 10) |
| **EntityManager + Batch Size + Clear** | Low (Bounded by batch) | Moderate | **Fast (~10,000 ops/s)** | Standard domain business batch |
| **StatelessSession** | **Near Zero** | Moderate | **Very Fast (~25,000 ops/s)** | High-speed data import / ETL |
| **Spring `JdbcClient` Batch Update** | **Zero** | Moderate | **Maximum (~40,000 ops/s)** | Raw SQL bulk ingestion |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why does <code>GenerationType.IDENTITY</code> disable JDBC batch inserts in Hibernate?</strong></summary>

`GenerationType.IDENTITY` relies on the database generating the primary key when a row is physically inserted. In JPA, every managed entity must have a non-null identifier. To satisfy this requirement, Hibernate cannot defer the `INSERT` to a batch; it must immediately execute the SQL statement to read back the generated ID from the database, breaking JDBC batching.
</details>

<details>
<summary><strong>2. How does <code>allocationSize</code> work in <code>@SequenceGenerator</code>?</strong></summary>

`allocationSize` configures sequence pooling (the Hi/Lo or pooled-lo algorithm). When set to `50`, Hibernate queries the database sequence once (`SELECT nextval(...)`), and reserves the next 50 integer IDs to assign in-memory to new entities without querying the database again. Once those 50 IDs are used, it queries the sequence for the next block.
</details>

<details>
<summary><strong>3. What is the role of <code>hibernate.order_inserts</code> and <code>hibernate.order_updates</code>?</strong></summary>

A JDBC batch buffer can only hold statements of the exact same SQL structure. If your code persists `Order 1`, `Item 1`, `Order 2`, `Item 2`, Hibernate would have to flush and switch the batch buffer on each step, rendering batching useless. Enabling `order_inserts` sorts the action queue so that all `Order` inserts execute together in one batch, followed by all `Item` inserts in the next batch.
</details>

<details>
<summary><strong>4. Why must you call both <code>entityManager.flush()</code> and <code>entityManager.clear()</code> during bulk loops?</strong></summary>

- `flush()` executes the currently queued JDBC batch statements and sends them to the database.
- `clear()` detaches all managed entities from the First-Level Cache (Persistence Context). Without `clear()`, the persistence context continues to hold every entity in memory, eventually resulting in an `OutOfMemoryError`.
</details>

<details>
<summary><strong>5. What is a Hibernate <code>StatelessSession</code> and when should you use it?</strong></summary>

A `StatelessSession` is a command-oriented alternative to the standard Hibernate `Session`. It does not implement a First-Level Cache, does not perform automatic dirty checking, and ignores cascades. It is ideal for high-throughput ETL pipelines or batch migrations where maintaining managed entity states in memory is unnecessary.
</details>

<details>
<summary><strong>6. What does <code>reWriteBatchedInserts=true</code> do in PostgreSQL JDBC?</strong></summary>

By default, the PostgreSQL JDBC driver sends batch statements as multiple individual queries within a single packet. Setting `reWriteBatchedInserts=true` instructs the driver to rewrite the batch into a single multi-row `INSERT INTO ... VALUES (...), (...), (...)` statement, reducing query parsing and execution time on the PostgreSQL server.
</details>

<details>
<summary><strong>7. How do you verify that JDBC batching is actually working in integration tests?</strong></summary>

Enable Hibernate statistics via `spring.jpa.properties.hibernate.generate_statistics=true` and inspect `sessionFactory.getStatistics().getPrepareStatementCount()`. If 100 entities were inserted and batching is active with size 50, exactly 2 statements will be prepared instead of 100.
</details>

<details>
<summary><strong>8. Can optimistic locking with <code>@Version</code> participate in JDBC batch updates?</strong></summary>

Yes, provided you enable `spring.jpa.properties.hibernate.jdbc.batch_versioned_data=true`. Hibernate uses the JDBC `executeBatch()` return codes (an array of affected row counts) to verify that every batched update affected exactly 1 row, throwing an exception if any entity's version had changed.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Sequence Entity with Batching Allocator</strong></summary>

**Task**: Create an entity `TransactionRecord` with a sequence generator configured with an allocation size of 25.

```java
@Entity
public class TransactionRecord {
    @Id
    @GeneratedValue(strategy = GenerationType.SEQUENCE, generator = "tx_seq")
    @SequenceGenerator(name = "tx_seq", sequenceName = "tx_seq", allocationSize = 25)
    private Long id;

    private String reference;
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Batch Configuration Setup</strong></summary>

**Task**: Write the `application.yaml` properties to configure a batch size of 30, order inserts, and order updates.

```yaml
spring:
  jpa:
    properties:
      hibernate:
        jdbc:
          batch_size: 30
        order_inserts: true
        order_updates: true
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Chunked Flush and Clear Implementation</strong></summary>

**Task**: Implement a service method that takes a list of 500 entity instances, persists them, and flushes/clears the `EntityManager` every 50 entities.

```java
@Transactional
public void persistChunked(List<LogEntry> entries) {
    for (int i = 0; i < entries.size(); i++) {
        entityManager.persist(entries.get(i));
        if (i > 0 && i % 50 == 0) {
            entityManager.flush();
            entityManager.clear();
        }
    }
    entityManager.flush();
    entityManager.clear();
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Bulk Deletion with @Modifying Query</strong></summary>

**Task**: Write a repository method that deletes all audit logs older than a given date in a single SQL statement without loading them into memory.

```java
@Modifying(clearAutomatically = true)
@Transactional
@Query("DELETE FROM AuditLog a WHERE a.createdAt < :cutoff")
int deleteLogsOlderThan(@Param("cutoff") Instant cutoff);
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Batch Verification Test with Hibernate Statistics</strong></summary>

**Task**: Write a test persisting 100 entities and assert via `Statistics` that exactly 2 JDBC batches were executed when batch size is 50.

```java
@Test
@Transactional
void testBatchExecutionCount() {
    Statistics stats = entityManagerFactory.unwrap(SessionFactory.class).getStatistics();
    stats.clear();

    for (int i = 0; i < 100; i++) {
        entityManager.persist(new GoodBatchItem("Item-" + i));
    }
    entityManager.flush();

    // Verify batch statement execution
    assertThat(stats.getPrepareStatementCount()).isEqualTo(2);
}
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): High-Speed Ingestion with Spring Data JdbcClient</strong></summary>

**Task**: Implement a bulk insertion method using `JdbcClient` or `NamedParameterJdbcTemplate.batchUpdate()` to insert 5,000 raw records at maximum speed.

```java
public void bulkInsertUsers(List<UserRecord> users) {
    String sql = "INSERT INTO users (username, email) VALUES (?, ?)";
    jdbcTemplate.batchUpdate(sql, new BatchPreparedStatementSetter() {
        @Override
        public void setValues(PreparedStatement ps, int i) throws SQLException {
            ps.setString(1, users.get(i).username());
            ps.setString(2, users.get(i).email());
        }
        @Override
        public int getBatchSize() { return users.size(); }
    });
}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------------+
| Golden Rule 1: NEVER use GenerationType.IDENTITY for batched entities (disables batch)|
| Golden Rule 2: ALWAYS use GenerationType.SEQUENCE with allocationSize >= 25           |
| Config:        hibernate.jdbc.batch_size: 50, order_inserts: true, order_updates: true|
| Driver Config: reWriteBatchedInserts=true (PostgreSQL), rewriteBatchedStatements=true |
| Memory Guard:  In batch loops, call em.flush() then em.clear() every batch_size items |
| Pure Bulk/ETL: Use Hibernate StatelessSession to bypass L1 cache and dirty checks     |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Hibernate 6.6 User Guide: Batch Processing](https://docs.jboss.org/hibernate/orm/6.6/userguide/html_single/Hibernate_User_Guide.html#batch)
- [Vlad Mihalcea: How to Batch INSERT and UPDATE Statements with Hibernate](https://vladmihalcea.com/how-to-batch-insert-and-update-statements-with-hibernate/)
- [PostgreSQL JDBC Driver Batch Execution Optimizations](https://jdbc.postgresql.org/documentation/query/#performing-batch-updates)
- [Hibernate StatelessSession Reference](https://docs.jboss.org/hibernate/orm/6.6/userguide/html_single/Hibernate_User_Guide.html#stateless-session)
