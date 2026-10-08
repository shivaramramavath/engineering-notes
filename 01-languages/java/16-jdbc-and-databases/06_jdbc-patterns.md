# JDBC Patterns

Raw JDBC is verbose, and the same few mistakes recur: leaked connections, missing rollbacks, SQL built by concatenation, N+1 queries, offset pagination that gets slower every page. This note collects the patterns that tame the boilerplate and avoid the traps. Many of them are what frameworks like Spring's `JdbcClient`, JDBI, and jOOQ do for you, so understanding them also shows you what those libraries are *for*.

**Prerequisites:** all earlier notes in this module, plus [Functional Patterns](../09-functional-java/08_functional-patterns.md) (the execute-around idea).

---

## 1. A small JDBC helper: reduce the boilerplate

The "get connection, prepare, bind, execute, map, close" ceremony is identical every time. Capture it once with functional interfaces:

```java
@FunctionalInterface interface RowMapper<T> { T map(ResultSet rs) throws SQLException; }
@FunctionalInterface interface TxWork<T>   { T apply(Connection c) throws SQLException; }

final class Jdbc {
    private final DataSource ds;
    Jdbc(DataSource ds) { this.ds = ds; }

    // --- queries and updates on a borrowed connection -------------------------------------------
    <T> List<T> query(String sql, RowMapper<T> mapper, Object... params) throws SQLException {
        try (Connection c = ds.getConnection()) { return query(c, sql, mapper, params); }
    }

    <T> Optional<T> queryOne(String sql, RowMapper<T> mapper, Object... params) throws SQLException {
        List<T> rows = query(sql, mapper, params);
        return rows.isEmpty() ? Optional.empty() : Optional.of(rows.get(0));
    }

    int update(String sql, Object... params) throws SQLException {
        try (Connection c = ds.getConnection()) { return update(c, sql, params); }
    }

    // --- the same operations on a connection you already hold (inside a transaction) -------------
    static <T> List<T> query(Connection c, String sql, RowMapper<T> mapper, Object... params) throws SQLException {
        try (PreparedStatement ps = c.prepareStatement(sql)) {
            bind(ps, params);
            try (ResultSet rs = ps.executeQuery()) {
                List<T> out = new ArrayList<>();
                while (rs.next()) out.add(mapper.map(rs));
                return out;
            }
        }
    }

    static int update(Connection c, String sql, Object... params) throws SQLException {
        try (PreparedStatement ps = c.prepareStatement(sql)) {
            bind(ps, params);
            return ps.executeUpdate();
        }
    }

    // --- transactions ----------------------------------------------------------------------------
    <T> T inTransaction(TxWork<T> work) throws SQLException {
        try (Connection c = ds.getConnection()) {
            c.setAutoCommit(false);
            try {
                T result = work.apply(c);
                c.commit();
                return result;
            } catch (SQLException | RuntimeException e) {
                c.rollback();
                throw e;
            }
        }
    }

    private static void bind(PreparedStatement ps, Object[] params) throws SQLException {
        for (int i = 0; i < params.length; i++) ps.setObject(i + 1, params[i]);   // null needs a typed setNull on some drivers
    }
}
```

Usage reads like intent instead of plumbing:

```java
Optional<User> user = jdbc.queryOne(
        "SELECT id, email, name, created_at FROM users WHERE email = ?", Mappers::user, email);

jdbc.inTransaction(c -> {
    Jdbc.update(c, "UPDATE accounts SET balance = balance - ? WHERE id = ? AND balance >= ?", amt, from, amt);
    Jdbc.update(c, "UPDATE accounts SET balance = balance + ? WHERE id = ?", amt, to);
    return null;
});
```

This is the **execute-around** pattern: the helper owns the resource lifecycle, so callers *cannot* forget to close or roll back. Treat the helper above as a teaching sketch. In real projects use a library (Spring `JdbcClient`/`JdbcTemplate`, JDBI, jOOQ) that adds named parameters, null-type handling, exception translation, and battle-testing.

---

## 2. Repositories (DAOs) and transaction scope

Keep SQL in a **repository/DAO** class per aggregate, and keep business logic and transaction boundaries in a **service** ([Repository and DAO](../24-design-patterns/04-architecture/02_repository-and-dao.md)):

```java
final class AccountRepository {
    Optional<Account> findForUpdate(Connection c, long id) throws SQLException {
        return Jdbc.query(c, "SELECT id, balance FROM accounts WHERE id = ? FOR UPDATE",
                          Mappers::account, id).stream().findFirst();
    }
    void updateBalance(Connection c, long id, BigDecimal newBalance) throws SQLException {
        Jdbc.update(c, "UPDATE accounts SET balance = ? WHERE id = ?", newBalance, id);
    }
}

final class TransferService {
    private final Jdbc jdbc; private final AccountRepository accounts;

    void transfer(long from, long to, BigDecimal amount) throws SQLException {
        jdbc.inTransaction(c -> {                          // ONE connection/transaction for the whole operation
            Account a = accounts.findForUpdate(c, Math.min(from, to)).orElseThrow();   // lock in a consistent order
            Account b = accounts.findForUpdate(c, Math.max(from, to)).orElseThrow();
            ...
            return null;
        });
    }
}
```

Repository methods take the **`Connection`** (so several can join one transaction) or, in frameworks, rely on a **thread-bound transaction** that they join automatically ([Transactions](04_transactions.md#7-transactions-in-frameworks)). Don't open a new connection per repository call inside one business operation. That breaks atomicity and burns pool connections.

---

## 3. Pagination

### Offset pagination (simple, scales badly)

```sql
SELECT id, total, created_at FROM orders WHERE user_id = ? ORDER BY created_at DESC, id DESC LIMIT ? OFFSET ?
```

The database must **read and discard** all `OFFSET` rows, so page 1,000 is far slower than page 1. Results also **shift** if rows are inserted while the user pages (duplicates or skipped items).

### Keyset (seek) pagination: stable and fast

Remember the last row of the previous page and ask for rows *after* it:

```sql
SELECT id, total, created_at
FROM   orders
WHERE  user_id = ?
  AND  (created_at, id) < (?, ?)          -- row-value comparison (PostgreSQL, MySQL 8+); else expand with OR
ORDER  BY created_at DESC, id DESC
LIMIT  ?;                                  -- ask for pageSize + 1 to know whether another page exists
```

```java
List<OrderRow> rows = jdbc.query(sql, Mappers::order, userId, lastCreatedAt, lastId, pageSize + 1);
boolean hasNext = rows.size() > pageSize;
List<OrderRow> page = hasNext ? rows.subList(0, pageSize) : rows;
Cursor next = hasNext ? new Cursor(page.get(page.size() - 1).createdAt(), page.get(page.size() - 1).id()) : null;
```

Back it with a matching index: `CREATE INDEX ON orders (user_id, created_at DESC, id DESC)`. Always include a **unique tiebreaker** (`id`) in the sort. The cost is that you can't jump to "page 57", which is fine for feeds and infinite scroll. More in [Pagination](../25-real-world-patterns/01_pagination.md).

---

## 4. Upserts and idempotent writes

"Insert, or update if it exists" done as *select, then insert-or-update* is a race. Use the database's atomic upsert:

```sql
-- PostgreSQL
INSERT INTO page_views (page_id, day, views) VALUES (?, ?, 1)
ON CONFLICT (page_id, day) DO UPDATE SET views = page_views.views + 1;

-- MySQL
INSERT INTO page_views (page_id, day, views) VALUES (?, ?, 1)
ON DUPLICATE KEY UPDATE views = views + 1;

-- SQL standard / Oracle / SQL Server: MERGE
```

Idempotent inserts (safe to retry) use a **unique key** plus "do nothing on conflict":

```sql
INSERT INTO processed_events (event_id) VALUES (?) ON CONFLICT (event_id) DO NOTHING
-- executeUpdate() == 1 → first time: process it.   == 0 → duplicate: skip.
```

Do the claim-and-process inside one transaction so a crash doesn't mark an event processed without doing the work ([Idempotency](../25-real-world-patterns/04_idempotency.md)). Counters should use `SET views = views + 1` in SQL rather than read-modify-write in Java ([Transactions](04_transactions.md#4-concurrency-control-locks-and-versions)).

---

## 5. The N+1 problem

The single most common performance bug in data access:

```java
List<User> users = findUsers();                          // 1 query
for (User u : users) {
    u.setOrders(findOrdersForUser(u.id()));              // N more queries: one per user
}
```

For 1,000 users that's 1,001 round trips. At 2 ms each it's 2 seconds, and it grows with the data. It's invisible in tests with 3 rows.

**Fixes:**

```java
// A. Batch the second query: 2 queries total
Map<Long, List<Order>> ordersByUser = jdbc.query(
        "SELECT id, user_id, total FROM orders WHERE user_id = ANY(?)", Mappers::order, userIdsArray)
        .stream().collect(Collectors.groupingBy(Order::userId));

// B. Or a JOIN and group the rows in Java ([ResultSet note](03_resultset-and-data-mapping.md#4-joins-and-one-to-many-results))
```

How to find it:

- Count queries per request (logs, APM, a datasource proxy), and **fail tests if a request issues more than an expected number of queries**.
- A request spending its time in "many tiny, similar queries" is the signature.

ORMs create N+1 *implicitly* through lazy loading ([From JDBC to ORM](07_from-jdbc-to-orm.md#4-the-classic-traps)).

---

## 6. Error translation

Don't let `SQLException` leak through the application. Translate it at the repository boundary into errors your domain understands:

```java
try {
    jdbc.update("INSERT INTO users (email, name) VALUES (?, ?)", email, name);
} catch (SQLException e) {
    if ("23505".equals(e.getSQLState())) {                      // unique violation
        throw new DuplicateEmailException(email, e);            // callers can show "email already registered"
    }
    if (isRetryable(e)) throw new TransientDataAccessException(e);
    throw new DataAccessFailure("Failed to insert user", e);    // unchecked: nothing the caller can fix
}
```

- Branch on **SQLState/subclass**, never on message text ([JDBC Fundamentals](01_jdbc-fundamentals.md#6-sqlexception)).
- To tell *which* constraint fired, use the constraint name, which many drivers expose (for example PostgreSQL's driver exposes it through its server error message). It is driver-specific.
- Wrap in **unchecked** exceptions at this boundary so higher layers aren't forced to declare `throws SQLException` ([Exception Handling Patterns](../06-exceptions-and-debugging/05_exception-handling-patterns.md)). Frameworks ship such a translation layer (Spring's `DataAccessException` hierarchy).
- Pair the unique-violation handler with a **`UNIQUE` constraint** as the real guard. The application check is only for a friendly message.

---

## 7. Schema migrations

The schema is code, so version it. Use a migration tool such as **Flyway** or **Liquibase**:

```text
 db/migration/
   V1__create_users.sql
   V2__create_orders.sql
   V3__add_orders_status_index.sql
```

Principles:

- **Versioned, ordered, immutable.** Once a migration has run anywhere shared, never edit it. Add a new one.
- Run migrations as part of **deployment** (by one process, before or at app start), not by every instance racing.
- **Don't let the ORM generate the production schema** (`hbm2ddl.auto=update` and similar are for experiments).
- **Make changes backward-compatible** so old and new application versions can run against the schema during a rolling deploy (the *expand/contract* approach): add a nullable column → deploy code that writes both → backfill (in batches) → make it required → later drop the old column.
- Big tables: add indexes without long locks (`CREATE INDEX CONCURRENTLY` on PostgreSQL), and batch backfills.
- Keep migrations tested: they run in your integration-test database too.

---

## 8. Testing database code

- **Test against the real database engine.** In-memory H2 behaves differently (types, `ON CONFLICT`, locking, isolation, JSON, case sensitivity), so tests pass and production fails. **Testcontainers** starts a throwaway real database in Docker. A special JDBC URL is enough for many setups:

```java
String url = "jdbc:tc:postgresql:16:///testdb";      // Testcontainers JDBC URL: starts a PostgreSQL 16 container (needs the Testcontainers dependency)
DataSource ds = new HikariDataSource(configFor(url));
// apply migrations (Flyway), then run tests
```

(Testcontainers' Java API has evolved across versions. See its documentation for the current module setup.)

- **Apply real migrations** to the test database so tests exercise the real schema.
- **Isolate tests:** truncate tables between tests, or run each test in a transaction that is rolled back (only works when the code under test doesn't commit its own transaction).
- **Test the constraints and edge cases:** unique violations, foreign keys, `NULL`s, optimistic-lock conflicts, empty results.
- **Test concurrency** where it matters: two threads doing the "reserve stock" operation, and assert no oversell ([Testing Patterns](../19-testing/04_testing-patterns.md)).
- **Assert query counts** for key endpoints, to catch N+1 regressions.
- Use realistic **data volumes** in a separate performance environment to validate indexes and `EXPLAIN` plans.

See [Integration Testing](../19-testing/03_integration-testing.md).

---

## 9. Observability and security checklist

- **Log slow queries** (database slow-query log, `pg_stat_statements`, APM spans) and include the SQL *shape*, not sensitive parameter values.
- **Expose metrics:** pool active/pending, query latency histograms, error counts by SQLState ([Connection Pooling](05_connection-pooling.md#5-monitoring)).
- **Least privilege:** the application's database user shouldn't own the schema, create tables, or have superuser rights. Use a separate migration user.
- **Secrets** from a secrets manager, **TLS** to the database ([Secrets Management](../21-security/03_secrets-management.md)).
- **Parameterize everything** ([Prepared Statements](02_statements-and-prepared-statements.md#2-sql-injection-the-problem)), and allow-list dynamic identifiers.
- **Time-box queries:** query timeouts, statement timeouts on the server for application roles.
- **Plan for failure:** retries for transient errors, idempotent writes, connection-loss handling.

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Copy-pasted connection/statement boilerplate with subtle leaks | A helper (or library) that owns the lifecycle |
| A new connection per repository call inside one business operation | One connection/transaction per unit of work |
| Offset pagination for large or deep lists | Keyset pagination with a unique tiebreaker and a matching index |
| Select-then-insert-or-update | Atomic upsert, with a unique constraint |
| N+1 queries | Batch (`ANY(?)`) or join, and assert query counts in tests |
| Leaking `SQLException` through layers | Translate at the repository boundary |
| Branching on error message text | SQLState / subclass |
| ORM-generated or hand-edited production schema | Versioned migrations (Flyway/Liquibase) |
| Editing an already-applied migration | Add a new migration |
| Testing only on H2 | Testcontainers with the real engine |
| Logging parameter values containing PII/secrets | Log SQL shape and IDs only |
| DDL and long backfills in one blocking migration on a big table | Expand/contract, concurrent index builds, batched backfills |

---

## Quick Summary

- Wrap the connection/statement/result lifecycle once (**execute-around**), or use a library (`JdbcClient`, JDBI, jOOQ).
- Put SQL in repositories and **transaction boundaries in the service**. One connection per unit of work.
- Prefer **keyset pagination** over deep offsets. Use **atomic upserts** and unique constraints for idempotent writes.
- **N+1** = one query per row of a previous query. Fix with batched or joined queries, and guard with query-count tests.
- **Translate** `SQLException` by SQLState at the boundary into domain/unchecked exceptions.
- Version the schema with **migrations**, test on the **real database** (Testcontainers), and monitor slow queries and the pool.

**Next:** [From JDBC to ORM](07_from-jdbc-to-orm.md)
