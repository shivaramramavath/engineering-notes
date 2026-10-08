# Statements and Prepared Statements

`PreparedStatement` is how you send SQL with **parameters**. Using it correctly gives you three things at once: **protection from SQL injection**, correct **type handling** (dates, decimals, `NULL`), and **better performance** (the database can reuse parsed plans, and you can batch). Concatenating values into SQL strings gives you none of them.

**Prerequisites:** [JDBC Fundamentals](01_jdbc-fundamentals.md), [SQL and Indexing Essentials](00_sql-and-indexing-essentials.md).

---

## 1. `Statement`, `PreparedStatement`, `CallableStatement`

| Type | Use |
|---|---|
| `Statement` | Static SQL with no parameters (DDL, fixed scripts). Avoid for anything with variable input |
| **`PreparedStatement`** | SQL with `?` placeholders. **The default choice** |
| `CallableStatement` | Calling stored procedures and functions |

```java
String sql = "SELECT id, email FROM users WHERE email = ? AND created_at > ?";

try (PreparedStatement ps = conn.prepareStatement(sql)) {
    ps.setString(1, email);                                  // 1-based index
    ps.setObject(2, OffsetDateTime.now().minusDays(30));    // java.time via setObject (JDBC 4.2)
    try (ResultSet rs = ps.executeQuery()) { ... }
}
```

---

## 2. SQL injection: the problem

```java
// NEVER DO THIS
String sql = "SELECT * FROM users WHERE email = '" + email + "'";
conn.createStatement().executeQuery(sql);
```

If `email` is `x' OR '1'='1`, the query becomes:

```sql
SELECT * FROM users WHERE email = 'x' OR '1'='1'      -- returns every user
```

and with `'; DROP TABLE users; --` the attacker can run their own statements (depending on the driver and settings). SQL injection consistently ranks among the most damaging web vulnerabilities, and it lets attackers read, change, or delete data and sometimes run commands on the server.

### Why parameters fix it

```java
PreparedStatement ps = conn.prepareStatement("SELECT id FROM users WHERE email = ?");
ps.setString(1, email);          // sent as DATA, never parsed as SQL
```

The SQL text (with `?`) and the values travel **separately**. The database parses the statement structure first, and the value can only ever be a value, whatever characters it contains. There is nothing to "escape".

### What parameters **cannot** do

Placeholders stand for **values** only, not SQL structure:

```java
"SELECT * FROM ? WHERE id = ?"            // table name: not allowed
"SELECT * FROM users ORDER BY ?"          // column name: it sorts by the constant value, not the column
"SELECT * FROM users ORDER BY name ?"     // direction: not allowed
```

For dynamic identifiers (sort column, table, direction), **validate against an allow-list** you control and then place the *known-good* text into the SQL:

```java
private static final Map<String, String> SORTABLE = Map.of(
        "name", "name", "created", "created_at", "email", "email");

String column = SORTABLE.get(requestedSort);                      // null if not allowed
if (column == null) throw new IllegalArgumentException("Bad sort field");
String dir = "desc".equalsIgnoreCase(requestedDir) ? "DESC" : "ASC";

String sql = "SELECT id, name FROM users ORDER BY " + column + " " + dir + " LIMIT ?";
```

Never pass user text into SQL structure, even "after escaping". More in [Input Validation and Injection](../21-security/00_input-validation-and-injection.md). Also run the application with a **least-privilege database account**, so a mistake costs less.

---

## 3. Setting parameters

```java
ps.setString(1, "Asha");
ps.setInt(2, 30);
ps.setLong(3, 42L);
ps.setBigDecimal(4, new BigDecimal("19.99"));     // money: BigDecimal, never double
ps.setBoolean(5, true);

ps.setObject(6, LocalDate.of(2024, 3, 10));              // DATE
ps.setObject(7, LocalDateTime.of(2024, 3, 10, 9, 30));   // TIMESTAMP (no zone)
ps.setObject(8, OffsetDateTime.now(ZoneOffset.UTC));     // TIMESTAMP WITH TIME ZONE
ps.setObject(9, UUID.randomUUID());                      // UUID (driver support: PostgreSQL yes)
```

JDBC 4.2 requires drivers to support `LocalDate`, `LocalTime`, `LocalDateTime`, `OffsetTime`, and `OffsetDateTime` through `setObject`. `Instant` and `ZonedDateTime` are driver-dependent. Convert an `Instant` with `OffsetDateTime.ofInstant(instant, ZoneOffset.UTC)` ([Date-Time Best Practices](../10-date-and-time/05_date-time-best-practices.md#5-interchange-apis-json-databases)).

### `NULL`

```java
ps.setNull(3, Types.VARCHAR);                    // explicit: the type is needed by some drivers
ps.setString(3, null);                           // typed setters with null generally work
ps.setObject(3, null);                           // may fail on some drivers: they can't infer the type
```

Optional values: `if (phone == null) ps.setNull(i, Types.VARCHAR); else ps.setString(i, phone);`. For primitives use the wrapper type and `setNull`.

### `LIKE` and wildcards

The parameter holds the whole pattern. Add the `%` yourself, and escape any wildcards that came from the user:

```java
String escaped = term.replace("\\", "\\\\").replace("%", "\\%").replace("_", "\\_");
PreparedStatement ps = conn.prepareStatement("SELECT id, name FROM users WHERE name LIKE ? ESCAPE '\\'");
ps.setString(1, "%" + escaped + "%");
```

(Leading-`%` patterns can't use a normal B-tree index, see [SQL Essentials](00_sql-and-indexing-essentials.md#things-that-silently-defeat-an-index).)

### `IN` lists

A placeholder is a single value, so `IN (?)` with a list doesn't work directly. Options:

```java
// 1. Generate one placeholder per element (stay under the driver/DB parameter limit)
String marks = ids.stream().map(x -> "?").collect(Collectors.joining(", "));
PreparedStatement ps = conn.prepareStatement("SELECT id, name FROM users WHERE id IN (" + marks + ")");
for (int i = 0; i < ids.size(); i++) ps.setLong(i + 1, ids.get(i));

// 2. PostgreSQL: pass an array and use = ANY(?)
Array arr = conn.createArrayOf("bigint", ids.toArray());
PreparedStatement ps2 = conn.prepareStatement("SELECT id, name FROM users WHERE id = ANY(?)");
ps2.setArray(1, arr);
```

The number of parameters per statement is limited (sometimes to around a thousand on some databases, tens of thousands on others). **Chunk big lists** into batches.

---

## 4. Generated keys

To get the auto-generated primary key after an `INSERT`:

```java
String sql = "INSERT INTO users (email, name) VALUES (?, ?)";

try (PreparedStatement ps = conn.prepareStatement(sql, Statement.RETURN_GENERATED_KEYS)) {
    ps.setString(1, email);
    ps.setString(2, name);
    ps.executeUpdate();

    try (ResultSet keys = ps.getGeneratedKeys()) {
        if (keys.next()) return keys.getLong(1);
    }
}
```

On PostgreSQL (and some others) a clearer alternative is to ask the database to return it:

```java
try (PreparedStatement ps = conn.prepareStatement(
        "INSERT INTO users (email, name) VALUES (?, ?) RETURNING id, created_at")) {
    ps.setString(1, email);
    ps.setString(2, name);
    try (ResultSet rs = ps.executeQuery()) {          // run it as a query: it returns a row
        rs.next();
        return rs.getLong("id");
    }
}
```

`RETURNING` can return any columns (`id`, `created_at`, defaults), saving a second query.

---

## 5. Batching

Sending 10,000 inserts one at a time costs 10,000 network round trips. **Batching** sends many parameter sets together:

```java
String sql = "INSERT INTO orders (user_id, total, status) VALUES (?, ?, ?)";

conn.setAutoCommit(false);                      // one transaction for the whole load
try (PreparedStatement ps = conn.prepareStatement(sql)) {
    int count = 0;
    for (OrderRow o : rows) {
        ps.setLong(1, o.userId());
        ps.setBigDecimal(2, o.total());
        ps.setString(3, o.status());
        ps.addBatch();                          // queue this parameter set

        if (++count % 1_000 == 0) {
            ps.executeBatch();                  // send in chunks, so memory and packet sizes stay bounded
        }
    }
    ps.executeBatch();                          // the remainder
    conn.commit();
} catch (SQLException e) {
    conn.rollback();
    throw e;
}
```

Details:

- `executeBatch()` returns an `int[]` of update counts. `Statement.SUCCESS_NO_INFO` (-2) means "succeeded, count unknown". `EXECUTE_FAILED` (-3) means that item failed.
- On failure you get a `BatchUpdateException` with `getUpdateCounts()`. Whether the remaining items ran depends on the driver, so wrap the batch in a **transaction** and roll back as a unit ([Transactions](04_transactions.md)).
- Chunk the batches (a few hundred to a few thousand rows). Huge batches use lots of memory and can hit packet limits.
- Many drivers only get the **real speedup** with a driver option that rewrites batches into multi-row statements or uses the protocol's batch support: for example `reWriteBatchedInserts=true` on the PostgreSQL driver and `rewriteBatchedStatements=true` on MySQL Connector/J. Check your driver's documentation, and measure.
- For truly bulk loads, use the database's native tool: PostgreSQL `COPY` (through the driver's `CopyManager`), MySQL `LOAD DATA`, SQL Server bulk copy.
- Batching works for `INSERT`/`UPDATE`/`DELETE`, not `SELECT`.

---

## 6. Calling stored procedures

```java
try (CallableStatement cs = conn.prepareCall("{ call transfer_funds(?, ?, ?) }")) {
    cs.setLong(1, fromId);
    cs.setLong(2, toId);
    cs.setBigDecimal(3, amount);
    cs.execute();
}

// Functions / OUT parameters
try (CallableStatement cs = conn.prepareCall("{ ? = call order_total(?) }")) {
    cs.registerOutParameter(1, Types.NUMERIC);
    cs.setLong(2, orderId);
    cs.execute();
    BigDecimal total = cs.getBigDecimal(1);
}
```

The `{ call ... }` escape syntax is portable. Procedure syntax and semantics (especially PostgreSQL functions vs procedures) vary by database. Prefer plain SQL in the application unless stored logic is an existing part of your system.

---

## 7. Dynamic queries, safely

Optional search filters are the most common reason people start concatenating. Do it by building the **structure** from fixed fragments and collecting **values** in a parameter list:

```java
StringBuilder sql = new StringBuilder("SELECT id, email, name FROM users WHERE 1 = 1");
List<Object> params = new ArrayList<>();

if (name != null)  { sql.append(" AND name ILIKE ?");        params.add("%" + name + "%"); }
if (since != null) { sql.append(" AND created_at >= ?");     params.add(since); }
if (email != null) { sql.append(" AND email = ?");           params.add(email); }
sql.append(" ORDER BY id LIMIT ? OFFSET ?");
params.add(limit); params.add(offset);

try (PreparedStatement ps = conn.prepareStatement(sql.toString())) {
    for (int i = 0; i < params.size(); i++) ps.setObject(i + 1, params.get(i));
    ...
}
```

Every piece of text appended to `sql` is a literal in your code, and all user input goes through `params`. For complex dynamic queries, a query builder (jOOQ, Spring's `JdbcClient`, JPA Criteria/Specifications) is safer and more readable than hand-rolled `StringBuilder` logic ([From JDBC to ORM](07_from-jdbc-to-orm.md)).

JDBC has **no named parameters** (`:name`). Libraries such as Spring's `NamedParameterJdbcTemplate` and JDBI add them.

---

## 8. Prepared statement performance and reuse

- The database **parses and plans** a statement once, so repeated execution with different parameters avoids re-parsing. Drivers and databases may cache prepared statements per connection (PostgreSQL's driver switches to server-side prepared statements after a threshold of repeated executions).
- Reuse one `PreparedStatement` across a loop (and batch) instead of preparing the same SQL repeatedly in a loop.
- Inlining literals (`WHERE id = 42`) produces a *different* SQL text for every value, which defeats statement caching and floods the cache. Parameters keep the text identical.
- Plans built for one parameter value can be suboptimal for others (skewed data). This "parameter sniffing" behavior is database-specific and uncommon, but worth knowing when a query is fast in a SQL console and slow from the app.
- Other `Statement` knobs: `setFetchSize` (see [note 03](03_resultset-and-data-mapping.md#5-large-results-and-fetch-size)), `setMaxRows`, `setQueryTimeout`.

---

## Common mistakes

| Mistake | Fix |
|---|---|
| String concatenation or `String.format` to build SQL with user values | `?` parameters |
| Trying to parameterize table/column names or `ORDER BY` | Allow-list lookup, then append the safe text |
| `setObject(i, null)` failing on some drivers | `setNull(i, Types.X)` |
| Forgetting `LIKE` wildcard escaping from user input | Escape `%`, `_`, `\` |
| `IN (?)` with a list | Generated placeholders or `= ANY(?)` |
| One-by-one inserts for bulk loads | Batch in chunks inside one transaction |
| Not checking `executeUpdate`'s count | `0` often means "no matching row" |
| Using `double` for money parameters | `BigDecimal` |
| Passing `java.util.Date` or `String` for dates | `java.time` types via `setObject` |
| Building a new `PreparedStatement` per row in a loop | Prepare once, execute many (or batch) |
| Logging SQL with parameter values containing secrets/PII | Mask or avoid |

### Debugging

- `Syntax error at or near "?"` → you parameterized something that isn't a value (identifier, keyword, or `LIMIT` in some dialects).
- `The column index is out of range` / `No value specified for parameter N` → the number of `?` and `set...` calls differ (common with dynamic SQL).
- `operator does not exist: character varying = integer` → wrong Java type bound to a parameter. Bind the correct type, because implicit casts also kill index use.
- A batch that is "not faster" → missing driver rewrite option, autocommit left on, or batches too small.
- Query fast in `psql`, slow from Java → compare bound parameter *types* and the plan (`EXPLAIN` with the same parameter types), plus fetch size and network latency.

---

## Quick Summary

- Use **`PreparedStatement` with `?` placeholders** for every query with variable input. Values travel separately from SQL, which eliminates SQL injection.
- Parameters are **values only**. Allow-list anything structural (column, table, sort direction).
- Bind real types: `BigDecimal` for money, `java.time` for dates, `setNull` for nulls. Escape `LIKE` wildcards. Handle `IN` with generated placeholders or `ANY(?)`.
- Get generated keys with `RETURN_GENERATED_KEYS` (or `RETURNING` on PostgreSQL).
- **Batch** bulk writes in chunks inside one transaction, and enable your driver's batch rewrite option. Use native bulk-load tools for huge loads.
- Build dynamic filters from fixed SQL fragments plus a parameter list, or use a query builder.

**Next:** [ResultSet and Data Mapping](03_resultset-and-data-mapping.md)
