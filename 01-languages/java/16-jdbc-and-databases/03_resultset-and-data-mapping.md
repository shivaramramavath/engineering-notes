# ResultSet and Data Mapping

> Efficient cursor streaming, type mapping, and object mapping patterns transform raw relational tabular data into strongly-typed Java domain models.

| Metadata | Details |
|---|---|
| **Level** | Core / Intermediate |
| **Prerequisites** | JDBC Fundamentals, Java Generics, Records |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **JDK Baseline** | Java 17 / 21 / 25 LTS |

---

## Table of Contents
1. [1. What & Why](#1-what--why)
2. [2. Core Concepts](#2-core-concepts)
3. [3. How It Works Internally: Cursor Navigation & Fetch Size](#3-how-it-works-internally-cursor-navigation--fetch-size)
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
A database query does not return Java objects; it streams raw tabular bytes organized into rows and columns over a network socket. The JDBC `ResultSet` interface provides the programmatic cursor that navigates these rows and converts SQL datatypes into Java types.

Handling `ResultSet` properly is the foundation of high-throughput data processing. Misconfigured fetch sizes cause client OutOfMemoryErrors or excessive network latency. Forgetting `wasNull()` silently converts null database balances into zeros. Understanding cursor navigation, type mapping strategies, and functional `RowMapper` abstractions empowers developers to build high-performance data access layers with zero bloat.

---

## 2. Core Concepts
- **Cursor**: A pointer maintained by the JDBC driver and database engine that tracks the current row in a tabular result set. Initially positioned before the first row.
- **`next()`**: Advances the cursor to the next row. Returns `true` if a row exists, or `false` when the end of the stream is reached.
- **Fetch Size (`setFetchSize`)**: The number of rows fetched from the database over the network in a single round-trip.
- **ResultSet Types**:
  - `TYPE_FORWARD_ONLY`: Cursor can only move forward (default, highest performance).
  - `TYPE_SCROLL_INSENSITIVE`: Cursor can scroll backward/forward; changes made by others are not visible.
  - `TYPE_SCROLL_SENSITIVE`: Cursor can scroll freely; reflects live database updates.
- **ResultSet Concurrency**: `CONCUR_READ_ONLY` (read-only) vs `CONCUR_UPDATABLE` (allows updating database rows via the cursor).
- **Type Mapping**: Mapping SQL types (`VARCHAR`, `TIMESTAMP`, `NUMERIC`) to standard Java representations (`String`, `Instant`, `BigDecimal`).
- **Functional RowMapper**: An abstraction that decouples row navigation logic from domain model construction.

---

## 3. How It Works Internally: Cursor Navigation & Fetch Size

When a client executes a query returning 1,000,000 rows, the database does not dump all rows into JVM memory simultaneously. Instead, communication is controlled via cursor buffers and fetch sizes:

```
Database Server                                      JVM / JDBC Driver
+--------------------+                               +--------------------+
| Result Table Rows  |                               | Client Heap Memory |
| [Row 1, Row 2...]  |                               | [ResultSet Window] |
+--------------------+                               +--------------------+
          |                                                    ^
          |==== Network Packet (FetchSize = 100 Rows) =======> | Rows 1 - 100
          |                                                    | next() reads heap
          |                                                    | Rows exhausted!
          |==== Network Packet (FetchSize = 100 Rows) =======> | Rows 101 - 200
```

### Driver Memory Buffering
1. **Default Fetch Size**: In PostgreSQL, the default fetch size is `0`, meaning the driver attempts to fetch **all** matching rows into client memory at once! On large tables, this triggers an immediate `java.lang.OutOfMemoryError: Java heap space`.
2. **Streaming Mode**: In MySQL, setting `statement.setFetchSize(Integer.MIN_VALUE)` activates streaming row-by-row mode. In PostgreSQL, setting `conn.setAutoCommit(false)` and `statement.setFetchSize(500)` forces server-side named cursor buffering, streaming batches of 500 rows over the network.
3. **Cursor State**: The cursor is initially positioned before the first row (index 0). Invoking `rs.next()` moves the cursor to index 1. Accessing column data before calling `next()` throws `SQLException: Before start of result set`.

---

## 4. Syntax & Usage

### 4.1 Safe Type-Safe Row Extraction
```java
package com.example.jdbc;

import java.math.BigDecimal;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.time.Instant;
import java.time.OffsetDateTime;

public final class ResultSetExtractor {

    public record CustomerOrder(
        long id,
        String orderCode,
        BigDecimal totalAmount,
        Instant placedAt,
        Integer discountPercent // Nullable column
    ) {}

    public static CustomerOrder extractOrder(ResultSet rs) throws SQLException {
        long id = rs.getLong("id");
        String code = rs.getString("order_code");
        BigDecimal amount = rs.getBigDecimal("total_amount");

        // Proper mapping of TIMESTAMP WITH TIME ZONE
        OffsetDateTime odt = rs.getObject("placed_at", OffsetDateTime.class);
        Instant placedAt = odt != null ? odt.toInstant() : null;

        // Proper nullable integer extraction
        int discountVal = rs.getInt("discount_percent");
        Integer discount = rs.wasNull() ? null : discountVal;

        return new CustomerOrder(id, code, amount, placedAt, discount);
    }
}
```

### 4.2 Generic Functional RowMapper Pattern
```java
package com.example.jdbc;

import java.sql.Connection;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.util.ArrayList;
import java.util.List;

@FunctionalInterface
public interface RowMapper<T> {
    T mapRow(ResultSet rs, int rowNum) throws SQLException;
}

public final class JdbcTemplateHelper {
    public static <T> List<T> query(Connection conn, String sql, RowMapper<T> mapper, Object... params) throws SQLException {
        try (PreparedStatement ps = conn.prepareStatement(sql)) {
            for (int i = 0; i < params.length; i++) {
                ps.setObject(i + 1, params[i]);
            }
            try (ResultSet rs = ps.executeQuery()) {
                List<T> results = new ArrayList<>();
                int rowNum = 0;
                while (rs.next()) {
                    results.add(mapper.mapRow(rs, ++rowNum));
                }
                return results;
            }
        }
    }
}
```

---

## 5. Practical Examples

### Example 1: Basic Stream Processing of Millions of Rows without OOM
```java
package com.example.jdbc;

import java.sql.*;

public final class LargeResultStreamer {
    public static void streamLargeTable(Connection conn) throws SQLException {
        // Crucial: Autocommit must be false for PostgreSQL to use server cursor
        conn.setAutoCommit(false);

        String sql = "SELECT id, payload FROM high_volume_events ORDER BY id";
        try (PreparedStatement ps = conn.prepareStatement(sql)) {
            // Set fetch size to 1,000 rows per network round trip
            ps.setFetchSize(1000);

            try (ResultSet rs = ps.executeQuery()) {
                long count = 0;
                while (rs.next()) {
                    long id = rs.getLong("id");
                    String payload = rs.getString("payload");
                    count++;
                    if (count % 10000 == 0) {
                        System.out.printf("Processed %d rows without heap exhaustion%n", count);
                    }
                }
            }
        } finally {
            conn.setAutoCommit(true);
        }
    }
}
```

### Example 2: Real-World Multi-Table Join Mapping (1-to-Many Aggregation)
```java
package com.example.jdbc;

import java.sql.ResultSet;
import java.sql.SQLException;
import java.util.*;

public final class OrderAggregateMapper {
    public record OrderWithItems(long orderId, String customer, List<String> itemSkus) {}

    public static List<OrderWithItems> mapJoinedOrders(ResultSet rs) throws SQLException {
        Map<Long, OrderWithItems> orderMap = new LinkedHashMap<>();

        while (rs.next()) {
            long orderId = rs.getLong("order_id");
            String customer = rs.getString("customer_name");
            String sku = rs.getString("item_sku");

            orderMap.computeIfAbsent(orderId, id -> new OrderWithItems(id, customer, new ArrayList<>()))
                    .itemSkus().add(sku);
        }

        return new ArrayList<>(orderMap.values());
    }
}
```

### Example 3: Edge Case - Inspecting Column Metadata Dynamically
```java
package com.example.jdbc;

import java.sql.*;

public final class DynamicResultPrinter {
    public static void printResultSet(ResultSet rs) throws SQLException {
        ResultSetMetaData meta = rs.getMetaData();
        int columnCount = meta.getColumnCount();

        for (int i = 1; i <= columnCount; i++) {
            System.out.printf("[%s : %s] ", meta.getColumnLabel(i), meta.getColumnTypeName(i));
        }
        System.out.println();

        while (rs.next()) {
            for (int i = 1; i <= columnCount; i++) {
                System.out.print(rs.getObject(i) + "	");
            }
            System.out.println();
        }
    }
}
```

---

## 6. Best Practices
1. **Always Configure Fetch Size on Large Queries**: Never rely on driver defaults when reading thousands of rows; set `ps.setFetchSize(500)`.
2. **Use Column Labels (`getColumnLabel`) Instead of Indices**: Column names or labels make code resilient to SQL column reordering.
3. **Use Modern Java Types with `getObject`**: Use `rs.getObject("timestamp_col", OffsetDateTime.class)` for modern time handling.
4. **Never Retain References to ResultSet Outside Query Scope**: Never return a raw `ResultSet` across architectural boundaries; map rows to domain objects or records immediately.
5. **Close ResultSet Promptly**: Always enclose `ResultSet` in try-with-resources to release database server cursor buffers immediately.
6. **Mind Nullable Primitives**: Always inspect `rs.wasNull()` when retrieving primitives.
7. **Use Strongly-Typed Records**: Leverage Java 21 Records for clean, immutable data transfer containers.
8. **Avoid Excessive Object Allocations**: In high-throughput ETL loops, reuse transient buffers where possible.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Correction |
|---|---|---|
| Accessing `rs.getString()` before calling `rs.next()` | Cursor is positioned before the first row; throws `SQLException` | Always invoke `rs.next()` inside an `if` or `while` |
| Calling `rs.next()` multiple times per iteration | Skips rows silently in loop | Call `rs.next()` strictly once per loop condition |
| Running out of memory on large queries | Driver buffers all rows into JVM heap by default | Configure `fetchSize` and disable `autoCommit` |
| Returning raw `ResultSet` to UI or service layer | Keeps database connection open, leaking connections | Map rows into immutable DTOs / Records |
| Using 0-based column indexing | JDBC column indices start at 1; throws `SQLException` | Use 1-based indexing or column labels |
| Assuming `getString()` returns null-safe empty string | Returns `null` when SQL value is NULL | Add null checks or wrap in `Optional` |
| Confusing `getColumnName` with `getColumnLabel` | `getColumnName` ignores SQL `AS` aliases in some drivers | Always use `getColumnLabel` to support aliases |
| Modifying ResultSet while iterating concurrently | ResultSet is strictly single-threaded | Synchronize access or map to thread-safe collections |

---

## 8. Performance & Security Notes
- **Heap Allocation Overhead**: Creating millions of intermediate objects inside a `while (rs.next())` loop puts pressure on young generation GC. Reuse mutable value containers or stream directly to disk/network when exporting data.
- **Fetch Size Tuning**:
  - Small fetch size (e.g., 10): High network latency overhead due to hundreds of small TCP packets.
  - Large fetch size (e.g., 50,000): High JVM heap memory usage.
  - Sweet spot: Typically 500 to 2,000 rows for enterprise queries.
- **Scrollable ResultSets Overhead**: `TYPE_SCROLL_INSENSITIVE` forces the driver to cache entire result sets in temporary memory on the client or server, degrading performance. Use only when random navigation is strictly mandatory.
- **Memory Footprint**: Type 4 drivers allocate byte arrays for network wire frames. A 100,000-row result set with 20 columns can easily consume > 150 MB heap.

---

## 9. Comparison / When to Use What

| Mapping Strategy | Performance | Type Safety | Boilerplate | Best Use Case |
|---|---|---|---|---|
| **Raw ResultSet Access** | Maximum (zero reflection) | Manual | High | High-throughput streaming, ETL pipelines |
| **Functional RowMapper** | High (compiled lambdas) | Compile-time safe | Low | Spring `JdbcClient`, custom lightweight repositories |
| **Java 21 Records** | High (compact memory) | Compile-time safe | Minimal | Standard domain entity extraction |
| **Reflection / ORM** | Moderate (reflection/proxies) | Runtime validated | Lowest | Complex relational graphs with nested joins |

---

## 10. Interview Questions

<details>
<summary>1. What is the initial position of a JDBC ResultSet cursor, and what happens if you read a column immediately?</summary>
The cursor is initially positioned before the first row (at position 0). If you attempt to call a getter like <code>rs.getString("name")</code> before calling <code>rs.next()</code>, the driver throws a <code>SQLException</code> stating that the cursor is before the start of the result set.
</details>

<details>
<summary>2. Why does reading a large table with raw JDBC often cause an OutOfMemoryError in PostgreSQL?</summary>
By default, the PostgreSQL JDBC driver fetches all matching rows of a query into client JVM heap memory at once. If a table contains millions of rows, the driver buffers all rows into Java byte arrays simultaneously, exhausting heap space. To fix this, set <code>conn.setAutoCommit(false)</code> and <code>ps.setFetchSize(1000)</code> to enable server-side cursor streaming.
</details>

<details>
<summary>3. What is the difference between getColumnName() and getColumnLabel() in ResultSetMetaData?</summary>
<code>getColumnName()</code> returns the underlying database table column name as defined in the schema. <code>getColumnLabel()</code> returns the label specified in the SQL query using the <code>AS</code> clause (or the column name if no alias was provided). In application code, always use <code>getColumnLabel()</code> to respect query projections and aliases.
</details>

<details>
<summary>4. How does rs.wasNull() work under the hood?</summary>
JDBC maintains an internal boolean flag in the <code>ResultSet</code> cursor state. Whenever any column getter is executed, the driver inspects the wire payload byte. If the byte indicates SQL NULL, the getter returns a default primitive value (e.g., 0, false), and sets the internal flag to <code>true</code>. Calling <code>rs.wasNull()</code> returns the status of this flag for the most recently read column.
</details>

<details>
<summary>5. What are the performance implications of using scrollable ResultSets (TYPE_SCROLL_INSENSITIVE)?</summary>
Scrollable result sets allow bidirectional navigation (<code>previous()</code>, <code>first()</code>, <code>absolute()</code>). To support this, the JDBC driver or database server must buffer the entire query result set in memory or on temporary disk space, incurring substantial memory overhead compared to forward-only streams.
</details>

<details>
<summary>6. How do modern JDBC drivers support java.time types like Instant and LocalDate?</summary>
Since JDBC 4.2, <code>ResultSet.getObject()</code> supports target type parameters. You can directly call <code>rs.getObject("col", LocalDate.class)</code> for <code>DATE</code> columns, <code>rs.getObject("col", LocalTime.class)</code> for <code>TIME</code>, and <code>rs.getObject("col", OffsetDateTime.class)</code> for <code>TIMESTAMP WITH TIME ZONE</code>.
</details>

<details>
<summary>7. What happens to an open ResultSet when its parent Statement or Connection is closed?</summary>
According to the JDBC specification, closing a <code>Statement</code> automatically closes all open <code>ResultSet</code> instances associated with it. Similarly, closing a <code>Connection</code> closes all associated statements. However, relying on cascading close is an anti-pattern; always close each resource explicitly via try-with-resources.
</details>

<details>
<summary>8. How do you map a 1-to-many relationship using a single SQL JOIN query without N+1 queries?</summary>
Execute a single <code>SELECT</code> with an <code>INNER JOIN</code> or <code>LEFT JOIN</code> between parent and child tables. In the <code>while (rs.next())</code> loop, use a <code>Map<ParentId, ParentObject></code> to aggregate child records into a list on the parent object, avoiding separate queries for each parent record.
</details>

<details>
<summary>9. What is the difference between CONCUR_READ_ONLY and CONCUR_UPDATABLE?</summary>
<code>CONCUR_READ_ONLY</code> specifies that the <code>ResultSet</code> cannot be used to modify data. <code>CONCUR_UPDATABLE</code> allows calling methods like <code>rs.updateString("name", "New")</code> and <code>rs.updateRow()</code> to modify underlying database rows directly through the cursor, which requires underlying unique key identification and holds server-side write locks.
</details>

<details>
<summary>10. How does rs.isBeforeFirst() differ from checking rs.next()?</summary>
<code>rs.isBeforeFirst()</code> returns <code>true</code> if the cursor is at the initial position and the result set contains at least one row, without advancing the cursor. <code>rs.next()</code> actually advances the cursor to the next row.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Implement a generic RowMapper for a Product record.</summary>

```java
public record Product(long id, String sku, BigDecimal price, int stockQuantity) {}

public static final RowMapper<Product> PRODUCT_MAPPER = (rs, rowNum) -> new Product(
    rs.getLong("id"),
    rs.getString("sku"),
    rs.getBigDecimal("price"),
    rs.getInt("stock_quantity")
);
```
</details>

<details>
<summary>Exercise 2: Write a robust method that reads a table with fetch size control and writes records to a CSV file stream.</summary>

```java
public static void exportToCsv(Connection conn, PrintWriter writer) throws SQLException {
    conn.setAutoCommit(false);
    String sql = "SELECT id, sku, price FROM products";
    try (PreparedStatement ps = conn.prepareStatement(sql)) {
        ps.setFetchSize(500);
        try (ResultSet rs = ps.executeQuery()) {
            writer.println("id,sku,price");
            while (rs.next()) {
                writer.printf("%d,%s,%s%n",
                    rs.getLong("id"),
                    rs.getString("sku"),
                    rs.getBigDecimal("price"));
            }
        }
    } finally {
        conn.setAutoCommit(true);
    }
}
```
</details>

<details>
<summary>Exercise 3: Write a helper that checks if a table has any rows matching a condition using LIMIT 1.</summary>

```java
public static boolean existsByEmail(DataSource ds, String email) throws SQLException {
    String sql = "SELECT 1 FROM users WHERE email = ? LIMIT 1";
    try (Connection conn = ds.getConnection();
         PreparedStatement ps = conn.prepareStatement(sql)) {
        ps.setString(1, email);
        try (ResultSet rs = ps.executeQuery()) {
            return rs.next(); // True if at least 1 row returned
        }
    }
}
```
</details>

<details>
<summary>Exercise 4: Implement a generic Stream-based query method using Spliterators.</summary>

```java
public static <T> Stream<T> streamQuery(Connection conn, String sql, RowMapper<T> mapper) throws SQLException {
    PreparedStatement ps = conn.prepareStatement(sql);
    ps.setFetchSize(500);
    ResultSet rs = ps.executeQuery();

    Iterator<T> iterator = new Iterator<>() {
        @Override
        public boolean hasNext() {
            try {
                return rs.next();
            } catch (SQLException e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        public T next() {
            try {
                return mapper.mapRow(rs, rs.getRow());
            } catch (SQLException e) {
                throw new RuntimeException(e);
            }
        }
    };

    return StreamSupport.stream(
        Spliterators.spliteratorUnknownSize(iterator, Spliterator.ORDERED | Spliterator.NONNULL),
        false
    ).onClose(() -> {
        try {
            rs.close();
            ps.close();
        } catch (SQLException ignored) {}
    });
}
```
</details>

---

## 12. Summary / Cheat Sheet
- **Cursor Positioning**: Cursor starts *before* first row; always call `rs.next()` before reading.
- **Fetch Size**: Set `ps.setFetchSize(500)` to stream records efficiently without memory exhaustion.
- **Null Checking**: Call `rs.wasNull()` immediately after reading primitive values.
- **Modern Types**: Use `rs.getObject(col, OffsetDateTime.class)` for date/time columns.
- **Column Aliases**: Use `rs.getMetaData().getColumnLabel(i)` to retrieve SQL `AS` aliases.
- **Aggregation**: Map 1-to-many joined rows using `Map<Id, Entity>` inside the cursor loop.

---

## 13. Further Reading
- Oracle Java Documentation: ResultSet and ResultSetMetaData Specification
- PostgreSQL JDBC Driver: Tuning Fetch Size and Cursor Streaming
- *High-Performance Java Persistence* by Vlad Mihalcea
- Spring Framework: JdbcTemplate and RowMapper Implementation Internals
