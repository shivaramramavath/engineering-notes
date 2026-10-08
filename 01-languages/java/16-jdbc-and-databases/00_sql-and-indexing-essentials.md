# SQL and Indexing Essentials

JDBC only *carries* SQL to the database. How fast and how correct your application is depends mostly on the SQL you send and on the **indexes** the database has to answer it. This note is the working knowledge a Java backend developer needs: enough SQL to read and write real queries, and enough indexing to understand why one query is instant and another scans a table.

It also defines the small schema used throughout this module. Examples use **PostgreSQL** syntax.

**Prerequisites:** none beyond basic programming.

---

## 1. The relational model in one minute

Data lives in **tables** (rows and columns). Rows are identified by a **primary key**, and tables relate through **foreign keys**. Constraints make bad data impossible to store.

```sql
CREATE TABLE users (
    id          BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    email       VARCHAR(255) NOT NULL UNIQUE,
    name        VARCHAR(100) NOT NULL,
    created_at  TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT now()
);

CREATE TABLE orders (
    id          BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    user_id     BIGINT NOT NULL REFERENCES users(id),
    total       NUMERIC(12,2) NOT NULL CHECK (total >= 0),
    status      VARCHAR(20) NOT NULL DEFAULT 'NEW',
    created_at  TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT now()
);

CREATE INDEX idx_orders_user_id ON orders(user_id);
```

| Constraint | Guarantees |
|---|---|
| `PRIMARY KEY` | Unique, non-null row identity (and an index) |
| `UNIQUE` | No duplicates (and an index) |
| `NOT NULL` | A value must be present |
| `REFERENCES` (foreign key) | The referenced row exists. You can't delete a user that still has orders (unless cascading) |
| `CHECK` | A condition holds for every row |

**Let the database enforce integrity.** Constraints catch bugs and race conditions that application checks miss, such as two concurrent requests both passing an "email not taken" check, where only a `UNIQUE` constraint reliably stops the second ([Transactions](04_transactions.md)).

### Common column types and Java types

| SQL type | Use for | Java type |
|---|---|---|
| `INTEGER`, `BIGINT` | Counts, IDs | `int`, `long` |
| `NUMERIC(p,s)` / `DECIMAL` | **Money** and exact decimals | `BigDecimal` |
| `VARCHAR(n)`, `TEXT` | Text | `String` |
| `BOOLEAN` | Flags | `boolean` |
| `DATE` | A calendar date | `LocalDate` |
| `TIMESTAMP WITH TIME ZONE` | A **moment in time** | `OffsetDateTime` / `Instant` |
| `TIMESTAMP` (without zone) | A local date-time | `LocalDateTime` |
| `UUID` | Globally unique IDs | `UUID` |
| `JSONB` / `JSON` | Semi-structured data | `String` (or a driver type) |
| `BYTEA` / `BLOB` | Binary data | `byte[]`, `InputStream` |

Never store money in `FLOAT`/`DOUBLE`. See [Date-Time Best Practices](../10-date-and-time/05_date-time-best-practices.md) for which date-time type to choose, and [ResultSet and Data Mapping](03_resultset-and-data-mapping.md) for the exact mappings.

---

## 2. Querying

```sql
-- Create
INSERT INTO users (email, name) VALUES ('asha@example.com', 'Asha');

-- Read
SELECT id, email, name
FROM   users
WHERE  created_at >= now() - interval '7 days'
  AND  name LIKE 'A%'
ORDER  BY created_at DESC
LIMIT  20;

-- Update (ALWAYS with a WHERE, or every row changes)
UPDATE orders SET status = 'SHIPPED' WHERE id = 1001 AND status = 'PAID';

-- Delete (same warning)
DELETE FROM orders WHERE status = 'CANCELLED' AND created_at < now() - interval '1 year';
```

Habits worth building:

- **Name columns** (`SELECT id, email`), don't `SELECT *`. It transfers unneeded data, defeats covering indexes, and breaks when columns are added.
- **Always bound result sets** (`LIMIT`/`FETCH FIRST`) for user-facing queries.
- Check **how many rows an `UPDATE`/`DELETE` affected**. Zero often means "someone else changed it first" ([Transactions](04_transactions.md#optimistic-locking)).

### `NULL` is not a value

`NULL` means "unknown", and comparisons with it are never true:

```sql
WHERE phone = NULL        -- never matches anything
WHERE phone IS NULL       -- correct
WHERE phone <> '123'      -- does NOT match rows where phone is NULL!
SELECT COALESCE(phone, 'n/a') FROM users;    -- default for NULL
```

SQL uses three-valued logic (true / false / unknown), and `WHERE` keeps only rows where the condition is *true*. `NULL`s are also why `NOT IN (subquery)` can unexpectedly return nothing. Prefer `NOT EXISTS`. In Java, `NULL` columns need care when reading ([ResultSet](03_resultset-and-data-mapping.md#null-handling)).

---

## 3. Joins

A join combines rows from two tables on a condition:

```sql
SELECT u.name, o.id, o.total
FROM   users u
JOIN   orders o ON o.user_id = u.id        -- INNER JOIN: only users WITH orders
WHERE  o.status = 'PAID';

SELECT u.name, COUNT(o.id) AS order_count
FROM   users u
LEFT   JOIN orders o ON o.user_id = u.id   -- LEFT JOIN: ALL users, even with no orders (o.* is NULL)
GROUP  BY u.id, u.name;
```

```text
 INNER JOIN     rows that match in BOTH tables
 LEFT  JOIN     all rows from the left table + matches from the right (NULLs where none)
 RIGHT JOIN     mirror image (rarely needed: swap the table order and use LEFT)
 FULL  JOIN     everything from both
 CROSS JOIN     every combination (rows × rows): usually a mistake
```

A join on a **one-to-many** relationship repeats the "one" side's columns for each "many" row. That affects counts and sums (`SUM(u.credit)` after joining orders multiplies it) and is the source of the mapping puzzles in [ResultSet and Data Mapping](03_resultset-and-data-mapping.md#4-joins-and-one-to-many-results).

---

## 4. Aggregation

```sql
SELECT status, COUNT(*) AS n, SUM(total) AS revenue, AVG(total) AS avg_order
FROM   orders
WHERE  created_at >= date_trunc('month', now())
GROUP  BY status
HAVING COUNT(*) > 10                 -- HAVING filters groups; WHERE filters rows
ORDER  BY revenue DESC;
```

- `COUNT(*)` counts rows. `COUNT(col)` counts **non-NULL** values of `col`.
- Every selected column must be in `GROUP BY` or inside an aggregate.
- Logical evaluation order: `FROM/JOIN → WHERE → GROUP BY → HAVING → SELECT → ORDER BY → LIMIT`. That's why you can't use a `SELECT` alias in `WHERE`.

Also worth knowing: subqueries, **CTEs** (`WITH recent AS (...) SELECT ...`) for readable multi-step queries, and **window functions** (`ROW_NUMBER() OVER (PARTITION BY user_id ORDER BY created_at DESC)`) for "latest per group" and running totals.

---

## 5. Indexes: why queries are fast or slow

Without an index, the database answers `WHERE email = 'x'` by reading **every row** (a *sequential scan*). An **index** is a separate sorted structure (usually a **B-tree**) that lets it jump straight to matching rows, like a book's index versus reading every page.

```text
 Without index:  scan 10,000,000 rows  → hundreds of ms to seconds
 With B-tree:    ~3-4 page lookups     → well under a millisecond (if cached)
```

```sql
CREATE INDEX idx_orders_user_id ON orders(user_id);           -- speeds up WHERE user_id = ? and joins
CREATE UNIQUE INDEX idx_users_email ON users(email);          -- (UNIQUE constraint already creates one)
CREATE INDEX idx_orders_status_created ON orders(status, created_at);   -- composite
```

### What to index

- Columns in **`WHERE`** filters that are selective (they cut the table down a lot).
- **Foreign keys** and join columns. PostgreSQL does *not* auto-index the referencing side of a foreign key (MySQL/InnoDB does).
- Columns used in **`ORDER BY`** together with a filter (so the index can supply sorted rows).
- **Don't** index everything. Each index costs **disk space and slows every `INSERT`/`UPDATE`/`DELETE`**, because all indexes must be maintained. Low-selectivity columns (a boolean, a status with 3 values on its own) rarely help.

### Composite indexes and the leftmost-prefix rule

An index on `(status, created_at)` is sorted by `status`, then `created_at` within each status. It helps queries that filter on:

- `status` alone ✔
- `status` and `created_at` ✔ (best)
- `created_at` alone ✘ (not usable efficiently: the leading column is missing)

Put the columns you **filter by equality first**, then range/sort columns. A **covering index** contains every column a query needs, so the table itself isn't touched (an *index-only scan*).

### Things that silently defeat an index

| Query pattern | Problem | Fix |
|---|---|---|
| `WHERE LOWER(email) = ?` | Function applied to the column | Index the expression (`CREATE INDEX ON users (LOWER(email))`) or normalize data on write |
| `WHERE name LIKE '%son'` | Leading wildcard | Full-text/trigram index, or avoid |
| `WHERE user_id = '42'` (wrong type) / implicit casts | Type conversion on the column | Bind the correct Java type ([Prepared Statements](02_statements-and-prepared-statements.md)) |
| `WHERE a = ? OR b = ?` | Single index can't serve both | Separate indexes, `UNION`, or rethink |
| `WHERE created_at::date = ?` | Function on the column | Use a range: `created_at >= ? AND created_at < ?` |
| Large `OFFSET` | Database still reads and discards all skipped rows | Keyset pagination ([Pagination](../25-real-world-patterns/01_pagination.md)) |
| Outdated statistics | Planner picks a bad plan | `ANALYZE` (autovacuum usually does this) |

---

## 6. `EXPLAIN`: asking the database what it will do

```sql
CREATE INDEX idx_orders_user_created ON orders(user_id, created_at DESC);

EXPLAIN ANALYZE
SELECT id, total FROM orders WHERE user_id = 42 ORDER BY created_at DESC LIMIT 10;
```

```text
 Limit  (cost=0.29..8.31 rows=10 ...) (actual time=0.03..0.05 rows=10 loops=1)
   ->  Index Scan Backward using idx_orders_user_created on orders ...
         Index Cond: (user_id = 42)
 Planning Time: 0.1 ms   Execution Time: 0.08 ms
```

(Output shape is illustrative and varies by version.) How to read it:

| You see | Meaning |
|---|---|
| `Seq Scan` on a big table with a selective filter | Missing/unused index |
| `Index Scan` / `Index Only Scan` / `Bitmap Heap Scan` | An index is used |
| Estimated `rows=` far from actual rows | Stale statistics, which leads to wrong plans |
| `Sort` with high cost/`external merge` | Sorting a lot of data: index on the sort columns |
| Nested loop with a huge inner loop count | Possible missing index on the join column |

`EXPLAIN` shows the *plan*; `EXPLAIN ANALYZE` actually **runs** the statement (careful with `UPDATE`/`DELETE`: wrap in a transaction and roll back). Use realistic data volumes: a plan on a 100-row dev table says nothing about 100 million rows.

---

## 7. Normalization, briefly

- **Normalize** to avoid duplicating facts: one place for a customer's name, rows referencing it by ID (1NF: atomic values, 2NF/3NF: every non-key column depends on "the key, the whole key, and nothing but the key").
- **Denormalize deliberately** for read performance (copy a value, precompute a total), accepting the burden of keeping copies in sync. Do it only when measurements justify it.
- Prefer **surrogate keys** (`BIGINT` identity or `UUID`) for primary keys, with natural identifiers (email) as `UNIQUE` columns.

---

## 8. Dialect differences you'll meet

| Feature | PostgreSQL | MySQL | Oracle / SQL standard |
|---|---|---|---|
| Auto IDs | `GENERATED ... AS IDENTITY` / `SERIAL` | `AUTO_INCREMENT` | `GENERATED AS IDENTITY` (12c+) / sequences |
| Limit rows | `LIMIT n` | `LIMIT n` | `FETCH FIRST n ROWS ONLY` |
| Upsert | `INSERT ... ON CONFLICT ... DO UPDATE` | `ON DUPLICATE KEY UPDATE` | `MERGE` |
| Case-sensitive `LIKE`/`=` | Yes | Depends on collation (often no) | Yes |
| DDL in a transaction | Yes | No (implicit commit) | No |
| Default isolation | Read Committed | Repeatable Read | Read Committed |

This is why tests against an in-memory H2 database can pass and production fail. Test against the **real database engine** (for example with Testcontainers: [JDBC Patterns](06_jdbc-patterns.md#8-testing-database-code)).

---

## Common mistakes

| Mistake | Fix |
|---|---|
| `UPDATE`/`DELETE` without `WHERE` | Review it, wrap in a transaction, check the affected count |
| `SELECT *` in application queries | List the columns you need |
| `= NULL` comparisons | `IS NULL` |
| No index on foreign keys / filter columns | Index by access pattern, and check with `EXPLAIN` |
| Too many indexes | Each slows writes. Remove unused ones |
| Functions or casts on indexed columns | Index the expression, or rewrite the predicate |
| Storing money as `DOUBLE` | `NUMERIC` / `BigDecimal` |
| Relying on application-level uniqueness checks | `UNIQUE` constraints |
| Deep `OFFSET` pagination | Keyset pagination |
| Testing queries on tiny data | Test plans with production-like volumes |
| Business logic only in app code with no constraints | Add `NOT NULL`, `CHECK`, `FOREIGN KEY` |

### Debugging slow queries

1. Find the query (slow-query log, `pg_stat_statements`, APM, JDBC logging).
2. Run `EXPLAIN (ANALYZE, BUFFERS)` on realistic data.
3. Look for sequential scans on big tables, bad estimates, big sorts, and enormous join loops.
4. Fix: add/adjust an index, rewrite the predicate, reduce selected columns/rows, update statistics.
5. Re-measure, and keep the query under test or monitoring.

---

## Quick Summary

- Tables + keys + **constraints**: let the database enforce integrity (`UNIQUE`, `NOT NULL`, `FOREIGN KEY`, `CHECK`).
- `NULL` ≠ value: use `IS NULL`, mind three-valued logic. `COUNT(col)` ignores NULLs.
- Joins combine tables: inner (matches only), left (keep all left rows). One-to-many joins repeat the "one" side.
- **Indexes** (B-tree) turn scans into lookups at the price of slower writes. Index filter/join/sort columns, respect the **leftmost-prefix rule**, and avoid functions/casts on indexed columns.
- Use **`EXPLAIN ANALYZE`** with realistic data to see scans vs index use.
- Money → `NUMERIC`; moments → `TIMESTAMP WITH TIME ZONE`; dialects differ, so test on the real engine.

**Next:** [JDBC Fundamentals](01_jdbc-fundamentals.md)
