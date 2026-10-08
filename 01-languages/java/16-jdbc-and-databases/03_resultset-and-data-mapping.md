# ResultSet and Data Mapping

A `ResultSet` is a **cursor over the rows** a query returned. You advance it one row at a time and read columns by name or position. This note covers reading rows correctly (especially `NULL`), how SQL types map to Java types (including `java.time`), turning rows into objects, handling joins, and processing large results without running out of memory.

**Prerequisites:** [JDBC Fundamentals](01_jdbc-fundamentals.md), [Statements and Prepared Statements](02_statements-and-prepared-statements.md), [Records](../12-modern-java/01_records.md).

---

## 1. Reading rows

```java
String sql = "SELECT id, email, name, created_at FROM users WHERE name LIKE ?";

try (PreparedStatement ps = conn.prepareStatement(sql)) {
    ps.setString(1, "A%");
    try (ResultSet rs = ps.executeQuery()) {
        while (rs.next()) {                                  // advances; false when no more rows
            long id         = rs.getLong("id");              // by column label
            String email    = rs.getString("email");
            String name     = rs.getString(3);               // or by 1-based index
            OffsetDateTime created = rs.getObject("created_at", OffsetDateTime.class);
        }
    }
}
```

- The cursor starts **before the first row**. You must call `next()` before reading anything. Forgetting it gives an error such as "ResultSet not positioned properly".
- Columns are read by **label** (the `AS` alias if you gave one, otherwise the column name) or by **1-based index**. Labels are clearer and survive reordering. Indexes are marginally faster.
- A single-row query: `if (rs.next()) { ... } else { /* not found */ }`.
- Read each column **once**, in a left-to-right order if possible. Some drivers don't allow rereading streams or large objects.
- Use the same `ResultSet` only inside its `try` block. Closing the statement or connection invalidates it ([JDBC Fundamentals](01_jdbc-fundamentals.md#5-closing-resources)).

### `NULL` handling

Primitive getters can't return `null`. A SQL `NULL` becomes `0`, `false`, or `0.0`, which is silently wrong:

```java
int age = rs.getInt("age");          // returns 0 when the column is NULL: is the person 0, or unknown?
if (rs.wasNull()) { ... }            // true if the LAST column read was NULL
```

Safer approaches:

```java
// 1. Read as a wrapper type via getObject(label, Class)
Integer age = rs.getObject("age", Integer.class);          // null when NULL

// 2. Or check wasNull() immediately after the read
int raw = rs.getInt("age");
Integer age2 = rs.wasNull() ? null : raw;

// Object types already return null
String phone = rs.getString("phone");                      // null when NULL
BigDecimal discount = rs.getBigDecimal("discount");        // null when NULL
```

Any nullable column should be read in a null-aware way and modeled as a nullable type or `Optional` in your domain code (as a **return** type, not a field: [Optional](../09-functional-java/03_optional.md)).

---

## 2. Types: SQL ↔ Java

| SQL type | Java type | Getter / binding |
|---|---|---|
| `SMALLINT`, `INTEGER` | `short`, `int` / `Integer` | `getInt`, `getObject(.., Integer.class)` |
| `BIGINT` | `long` / `Long` | `getLong` |
| `NUMERIC`, `DECIMAL` | **`BigDecimal`** | `getBigDecimal` |
| `REAL`, `DOUBLE PRECISION` | `float`, `double` | `getFloat`, `getDouble` (approximate: **not for money**) |
| `VARCHAR`, `TEXT`, `CHAR` | `String` | `getString` |
| `BOOLEAN` | `boolean` / `Boolean` | `getBoolean` |
| `DATE` | **`LocalDate`** | `getObject(.., LocalDate.class)` |
| `TIME` | `LocalTime` | `getObject(.., LocalTime.class)` |
| `TIMESTAMP` (no zone) | **`LocalDateTime`** | `getObject(.., LocalDateTime.class)` |
| `TIMESTAMP WITH TIME ZONE` | **`OffsetDateTime`** (→ `Instant`) | `getObject(.., OffsetDateTime.class)` |
| `UUID` | `UUID` | `getObject(.., UUID.class)` (PostgreSQL) |
| `BYTEA` / `BLOB` | `byte[]` / `InputStream` | `getBytes`, `getBinaryStream` |
| `JSON` / `JSONB` | `String` (parse with Jackson) | `getString` (driver-specific types exist) |
| `ARRAY` | `java.sql.Array` → Java array | `getArray(..).getArray()` |

### Dates and times: use `java.time`

JDBC 4.2 requires support for `LocalDate`, `LocalTime`, `LocalDateTime`, `OffsetTime`, and `OffsetDateTime`. Avoid the legacy `java.sql.Date`/`Time`/`Timestamp` in new code: their behavior depends on the JVM's default time zone ([Date-Time Overview](../10-date-and-time/00_java-time-overview.md#5-working-with-legacy-code)).

```java
LocalDate birthday   = rs.getObject("birthday", LocalDate.class);
OffsetDateTime paid  = rs.getObject("paid_at", OffsetDateTime.class);
Instant paidInstant  = paid == null ? null : paid.toInstant();       // Instant isn't a mandated JDBC type
```

Rules of thumb ([Date-Time Best Practices](../10-date-and-time/05_date-time-best-practices.md#5-interchange-apis-json-databases)):

- A **moment** (`created_at`, `paid_at`) → `TIMESTAMP WITH TIME ZONE` ↔ `OffsetDateTime`/`Instant`. Databases normally store this as UTC.
- A **calendar date** (`birthday`) → `DATE` ↔ `LocalDate`.
- A **local date-time without a zone** → `TIMESTAMP` ↔ `LocalDateTime`. Don't use it as a timestamp of when something happened.
- Behavior of `TIMESTAMP WITHOUT TIME ZONE` vs `WITH TIME ZONE`, and of the session time zone, differs by database and driver. Read yours.
- `Timestamp` round-trip precision depends on the column type: a column keeping microseconds returns different values than a nanosecond `Instant` you wrote. Truncate before comparing.

### Money and numbers

`NUMERIC` → `BigDecimal`. Compare with `compareTo`, not `equals` (`2.0` vs `2.00`), and use `setScale`/`RoundingMode` explicitly ([Numeric Precision](../01-fundamentals/08_numeric-precision-and-math.md)).

### Booleans, enums, and flags

`BOOLEAN` is portable in modern databases, but some (older Oracle, MySQL `TINYINT(1)`) use numbers or chars. For Java enums, store the **name** as `VARCHAR` (with a `CHECK` constraint) and map with `Status.valueOf(rs.getString("status"))`. Don't store the ordinal, because reordering the enum silently corrupts the data ([Enums](../04-oop/11_enums.md)).

---

## 3. Mapping rows to objects

Read rows into **immutable value types** (records) inside the `try`, and return plain collections:

```java
public record User(long id, String email, String name, Instant createdAt) {}

static User mapUser(ResultSet rs) throws SQLException {
    return new User(
            rs.getLong("id"),
            rs.getString("email"),
            rs.getString("name"),
            rs.getObject("created_at", OffsetDateTime.class).toInstant());
}

List<User> findByNamePrefix(DataSource ds, String prefix) throws SQLException {
    String sql = "SELECT id, email, name, created_at FROM users WHERE name LIKE ? ORDER BY id";
    try (Connection c = ds.getConnection(); PreparedStatement ps = c.prepareStatement(sql)) {
        ps.setString(1, prefix + "%");
        try (ResultSet rs = ps.executeQuery()) {
            List<User> out = new ArrayList<>();
            while (rs.next()) out.add(mapUser(rs));
            return out;
        }
    }
}
```

Extracting a **row mapper** function (`ResultSet → T`) keeps mapping separate from query plumbing, and it's the shape that libraries such as Spring's `JdbcClient`/`RowMapper` and JDBI use ([JDBC Patterns](06_jdbc-patterns.md#1-a-small-jdbc-helper-reduce-the-boilerplate)).

Habits:

- **Select only the columns you map** (no `SELECT *`).
- When joining tables with overlapping column names (`id`, `created_at`), use **aliases** (`u.id AS user_id, o.id AS order_id`) and read by alias. `getString("id")` on a join with two `id` columns silently returns the first.
- Map to **domain-friendly types**, not raw JDBC types: enums, `Instant`, value objects.
- Handle "no row" explicitly: return `Optional<User>` from lookup-by-key methods.

---

## 4. Joins and one-to-many results

A join of a parent with its children returns **one row per child**, with the parent's columns repeated:

```sql
SELECT u.id AS user_id, u.name, o.id AS order_id, o.total
FROM   users u
LEFT   JOIN orders o ON o.user_id = u.id
WHERE  u.id = ANY(?)
ORDER  BY u.id, o.id;
```

```text
 user_id | name  | order_id | total
 --------+-------+----------+-------
   1     | Asha  |  101     | 20.00
   1     | Asha  |  102     | 35.50     ← "Asha" repeated for each order
   2     | Ravi  |  NULL    | NULL      ← LEFT JOIN: user without orders
```

To build `User(orders=[...])` objects, **group the rows**:

```java
record OrderLine(long id, BigDecimal total) {}
record UserWithOrders(long id, String name, List<OrderLine> orders) {}

Map<Long, UserWithOrders> byId = new LinkedHashMap<>();             // keeps the query's order
while (rs.next()) {
    long userId = rs.getLong("user_id");
    UserWithOrders u = byId.get(userId);
    if (u == null) {                                                // first row for this user
        u = new UserWithOrders(userId, rs.getString("name"), new ArrayList<>());
        byId.put(userId, u);
    }
    long orderId = rs.getLong("order_id");
    if (!rs.wasNull()) {                                            // LEFT JOIN: order columns may be NULL
        u.orders().add(new OrderLine(orderId, rs.getBigDecimal("total")));
    }
}
List<UserWithOrders> result = List.copyOf(byId.values());
```

Alternatives and trade-offs:

| Approach | Queries | Notes |
|---|---|---|
| One **join** and group in Java | 1 | Parent columns repeated per child. Row count grows with the data. Pagination by parent is awkward |
| **Two queries**: parents, then `WHERE parent_id = ANY(?)` for children, grouped in Java | 2 | No duplication, and pagination by parent is easy. The usual best choice for lists |
| Query children **once per parent** in a loop | **N+1** | Simple, and **slow**: one round trip per parent ([JDBC Patterns](06_jdbc-patterns.md#5-the-n1-problem)) |

This is exactly what ORMs automate, and where their N+1 problem comes from ([From JDBC to ORM](07_from-jdbc-to-orm.md)).

---

## 5. Large results and fetch size

By default many drivers **load the entire result into memory** before `executeQuery()` returns. Reading 10 million rows that way means an `OutOfMemoryError`.

Ask the driver to **stream** the result in chunks with a **fetch size**:

```java
conn.setAutoCommit(false);                       // PostgreSQL: cursor-based fetching requires autocommit OFF
try (PreparedStatement ps = conn.prepareStatement(
        "SELECT id, email FROM users",
        ResultSet.TYPE_FORWARD_ONLY, ResultSet.CONCUR_READ_ONLY)) {
    ps.setFetchSize(1_000);                      // rows per round trip
    try (ResultSet rs = ps.executeQuery()) {
        while (rs.next()) {
            process(rs.getLong("id"), rs.getString("email"));   // constant memory
        }
    }
}
conn.commit();
```

Driver behavior differs, so check yours:

| Database/driver | How streaming works |
|---|---|
| PostgreSQL | `setFetchSize(n)` **and** autocommit off (and a forward-only result) |
| MySQL Connector/J | Server-side cursor with `useCursorFetch=true` + `setFetchSize(n)`, or row-by-row streaming with `setFetchSize(Integer.MIN_VALUE)` (older style). Check current docs |
| Oracle, SQL Server | `setFetchSize(n)` generally effective |

Tips for big reads:

- **Process row by row** and write results out as you go ([File Processing Patterns](../11-io-and-networking/02_file-processing-patterns.md)). Don't collect everything into a list.
- Long streaming reads hold a connection (and often a transaction/snapshot) for the duration. That ties up a pooled connection ([Connection Pooling](05_connection-pooling.md)). Consider a dedicated pool or a read replica for exports.
- Prefer **keyset pagination** over a huge streaming cursor for user-facing paging ([Pagination](../25-real-world-patterns/01_pagination.md)).
- `Statement.setMaxRows(n)` limits rows on the client side, but put the limit in the SQL (`LIMIT`) so the database stops early.

---

## 6. Large objects, metadata, and other `ResultSet` kinds

**Large objects:** for `BLOB`/`BYTEA`/`CLOB`, stream instead of materializing: `InputStream in = rs.getBinaryStream("data")` (consume it before moving to the next row or column, and close it). For files of significant size, consider storing them in object storage and keeping a reference in the database.

**`ResultSetMetaData`:** describes the columns of any result (names, types), useful for generic tools such as exporters and admin screens:

```java
ResultSetMetaData md = rs.getMetaData();
for (int i = 1; i <= md.getColumnCount(); i++) {
    System.out.println(md.getColumnLabel(i) + " : " + md.getColumnTypeName(i));
}
```

**Scrollable and updatable result sets** (`TYPE_SCROLL_INSENSITIVE`, `CONCUR_UPDATABLE`, `rs.absolute(n)`, `rs.updateString(...)`) exist, but are rarely used and have inconsistent support and performance. Prefer forward-only reads and explicit `UPDATE` statements.

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Reading before calling `next()` | Always `while (rs.next())` / `if (rs.next())` |
| `getInt` on a nullable column (silent `0`) | `getObject(label, Integer.class)` or `wasNull()` |
| Using `double` for money columns | `BigDecimal` |
| `java.sql.Timestamp`/`Date` in new code | `OffsetDateTime`/`Instant`/`LocalDate` |
| Returning the `ResultSet` out of the method | Map to records/objects inside the `try` |
| `SELECT *` | Select the columns you map |
| Same column name from two joined tables | Alias and read by alias |
| Loading a huge result into a `List` | Stream with a fetch size (and autocommit off for PostgreSQL) |
| Looping over parents and querying children each time | One batched query (`= ANY(?)`) and grouping |
| Storing enum ordinals | Store names |
| Comparing `BigDecimal`s with `equals` | `compareTo` |
| Holding a streaming result while doing slow work per row | Hand off to a queue, or read faster and process elsewhere |

### Debugging

- `PSQLException: ... column "x" does not exist` / `The column name x was not found in this ResultSet` → wrong label (alias vs name), or the column isn't in the `SELECT`.
- `Bad value for type int` / conversion errors → the SQL type doesn't match the getter. Check the column type and use the right mapping (or `getObject(label, Class)`).
- Dates off by hours → mixing `TIMESTAMP` with `TIMESTAMP WITH TIME ZONE`, or legacy `Timestamp` using the JVM default zone.
- `OutOfMemoryError` during a query → no fetch size/streaming. Heap-dump to confirm the `ResultSet` buffers.
- Wrong data in joined results → duplicate column names, or aggregates after a one-to-many join multiplying rows.

---

## Quick Summary

- A `ResultSet` is a forward cursor: **`next()` first**, read by label or 1-based index, and use it only inside its `try`.
- **`NULL` + primitives = silent zero.** Use `getObject(label, Wrapper.class)` or `wasNull()`.
- Types: `NUMERIC` → `BigDecimal`; moments → `OffsetDateTime`/`Instant`; dates → `LocalDate`; local date-times → `LocalDateTime`; enums stored as **names**.
- Map rows to **records** with a small row-mapper function, select only needed columns, and alias duplicate names.
- One-to-many joins repeat the parent per child, so **group** the rows (or use two queries). Per-row child queries cause **N+1**.
- For big results, **stream with a fetch size** (PostgreSQL also needs autocommit off) and process row by row.

**Next:** [Transactions](04_transactions.md)
