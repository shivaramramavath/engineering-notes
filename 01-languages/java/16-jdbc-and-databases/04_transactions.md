# Transactions

A **transaction** groups several database operations into one all-or-nothing unit. Either every change in it becomes permanent (**commit**), or none of them do (**rollback**). Without transactions, a crash or exception halfway through "debit one account, credit another" leaves money missing. With concurrent users, interleaved reads and writes produce results no serial execution would give.

Transactions are the most important correctness tool a database offers, and the source of most subtle data bugs when misunderstood.

**Prerequisites:** [JDBC Fundamentals](01_jdbc-fundamentals.md), [Statements and Prepared Statements](02_statements-and-prepared-statements.md), [Thread Safety](../14-concurrency/02_thread-safety.md) (the same race-condition ideas apply).

---

## 1. ACID

| Property | Meaning |
|---|---|
| **Atomicity** | All changes in the transaction happen, or none do |
| **Consistency** | The transaction moves the database from one valid state to another (constraints hold) |
| **Isolation** | Concurrent transactions don't see each other's half-finished work (to a degree you choose) |
| **Durability** | After commit, the data survives crashes |

Atomicity and durability come from the database engine (a write-ahead log). **Consistency** is a joint effort of your constraints and your logic. **Isolation** is a trade-off you configure ([section 3](#3-isolation-levels-and-anomalies)).

---

## 2. Transactions in JDBC

By default a `Connection` is in **auto-commit** mode: *every statement is its own transaction*, committed immediately. To group statements, turn it off:

```java
void transfer(DataSource ds, long fromId, long toId, BigDecimal amount) throws SQLException {
    try (Connection conn = ds.getConnection()) {
        conn.setAutoCommit(false);                       // BEGIN
        try {
            debit(conn, fromId, amount);                 // all statements use THE SAME connection
            credit(conn, toId, amount);
            conn.commit();                               // make it permanent
        } catch (SQLException | RuntimeException e) {
            conn.rollback();                             // undo everything since the last commit
            throw e;
        }
    }
}

private void debit(Connection conn, long id, BigDecimal amount) throws SQLException {
    String sql = "UPDATE accounts SET balance = balance - ? WHERE id = ? AND balance >= ?";
    try (PreparedStatement ps = conn.prepareStatement(sql)) {
        ps.setBigDecimal(1, amount);
        ps.setLong(2, id);
        ps.setBigDecimal(3, amount);
        if (ps.executeUpdate() != 1) {
            throw new InsufficientFundsException(id);    // no row updated: unknown account or not enough money
        }
    }
}
```

Key points:

- **A transaction belongs to a single `Connection`.** Every statement that should be part of it must use that same connection. With a pool, calling `ds.getConnection()` again gives a *different* connection, outside your transaction.
- `commit()` ends the transaction and starts the next one implicitly. `rollback()` discards it.
- **Always roll back on failure.** If you close a connection with uncommitted work, the behavior is vendor-dependent. Pools typically roll back on return, but don't rely on it.
- The `balance >= ?` condition inside the `UPDATE` makes the check **atomic with the change**. Reading the balance first and then updating (check-then-act) would race ([Thread Safety](../14-concurrency/02_thread-safety.md#1-the-problem-a-race-condition-you-can-reproduce)).
- Pools reset auto-commit when a connection is returned, but if you manage connections manually, restore it: `conn.setAutoCommit(true)`.
- **Savepoints** allow partial rollback inside a transaction:

```java
Savepoint sp = conn.setSavepoint("before-optional");
try { optionalStep(conn); }
catch (SQLException e) { conn.rollback(sp); }       // undo only the optional step
conn.releaseSavepoint(sp);
```

- **DDL** (`CREATE TABLE`, `ALTER`) is transactional in PostgreSQL, but causes an **implicit commit** in MySQL and Oracle. Know your database.

---

## 3. Isolation levels and anomalies

When transactions run concurrently, these things can go wrong:

| Anomaly | What happens |
|---|---|
| **Dirty read** | You read data another transaction hasn't committed (and might roll back) |
| **Non-repeatable read** | You read the same row twice and get different values, because another transaction committed a change in between |
| **Phantom read** | You run the same query twice and get **different sets of rows** (another transaction inserted/deleted matching rows) |
| **Lost update** | Two transactions read a value, both compute a new one, and the second write overwrites the first |
| **Write skew** | Two transactions read overlapping data, each writes something different based on it, and together they violate a rule neither would break alone |

The SQL standard defines four **isolation levels**, each preventing more anomalies at more cost:

| Level | Dirty read | Non-repeatable read | Phantom |
|---|---|---|---|
| `READ UNCOMMITTED` | possible | possible | possible |
| `READ COMMITTED` | prevented | possible | possible |
| `REPEATABLE READ` | prevented | prevented | possible (standard) |
| `SERIALIZABLE` | prevented | prevented | prevented |

```java
conn.setTransactionIsolation(Connection.TRANSACTION_REPEATABLE_READ);   // set before starting work
```

Real databases differ from the textbook, and most use **MVCC** (multi-version concurrency control), where readers see a consistent snapshot and don't block writers:

| Database | Default | Notes |
|---|---|---|
| PostgreSQL | **Read Committed** | `READ UNCOMMITTED` behaves as Read Committed. `REPEATABLE READ` is snapshot isolation (no phantoms), and concurrent conflicting updates fail with a serialization error (`40001`). `SERIALIZABLE` also detects write skew |
| MySQL (InnoDB) | **Repeatable Read** | Uses gap/next-key locks to limit phantoms |
| Oracle | **Read Committed** | Offers `SERIALIZABLE` (snapshot-based) and read-only |
| SQL Server | **Read Committed** (locking by default) | `READ_COMMITTED_SNAPSHOT` and `SNAPSHOT` options use row versioning |

How to choose:

- **Read Committed** (the common default) is fine for most operations *if* you protect read-modify-write sequences explicitly (section 4).
- Move to **Repeatable Read/Serializable** for operations where a consistent view across several reads matters (reports, invariants spanning rows). Be ready to **retry** on serialization failures ([section 6](#6-deadlocks-and-retries)).
- Higher isolation = more conflicts, retries, or blocking. Don't raise it globally "to be safe".

---

## 4. Concurrency control: locks and versions

The classic bug at Read Committed:

```java
// Two requests run this at the same time
BigDecimal stock = selectStock(conn, productId);        // both read 1
if (stock.signum() > 0) updateStock(conn, productId, stock.subtract(BigDecimal.ONE));   // both write 0: sold twice
```

This is the lost update/oversell problem. There are three robust fixes.

### Make it a single atomic statement (best when possible)

```sql
UPDATE products SET stock = stock - 1 WHERE id = ? AND stock > 0     -- check the update count: 0 → sold out
```

### Pessimistic locking: lock the row first

```sql
SELECT stock FROM products WHERE id = ? FOR UPDATE      -- other transactions block (or fail) on this row until commit
```

```java
conn.setAutoCommit(false);
// SELECT ... FOR UPDATE, then check and UPDATE, then commit: the row stays locked in between
```

- `FOR UPDATE NOWAIT` fails immediately if the row is locked, and `FOR UPDATE SKIP LOCKED` **skips** locked rows, which makes a database table usable as a **work queue** where many workers each claim different jobs:

```sql
SELECT id, payload FROM jobs
WHERE  status = 'PENDING'
ORDER  BY id
LIMIT  10
FOR UPDATE SKIP LOCKED;          -- then mark them IN_PROGRESS and commit
```

- Locks are held until the transaction ends, so keep these transactions **short**. Lock rows in a consistent order to avoid deadlocks.

### Optimistic locking

Assume conflicts are rare. Don't lock. Add a **version** column and make the update conditional on it:

```sql
UPDATE orders
SET    status = ?, version = version + 1
WHERE  id = ? AND version = ?;           -- 0 rows updated → someone else changed it
```

```java
int updated = ps.executeUpdate();
if (updated == 0) {
    throw new OptimisticLockException("Order " + id + " was modified by someone else: reload and retry");
}
```

Users read the row (with its `version`), edit it for as long as they like (no locks held), and the write succeeds only if nobody changed it in the meantime. It suits web forms and low-conflict updates. JPA provides it via `@Version` ([From JDBC to ORM](07_from-jdbc-to-orm.md)).

| | Pessimistic | Optimistic |
|---|---|---|
| Blocks others | Yes (until commit) | No |
| Best when | Conflicts are frequent or retries are expensive | Conflicts are rare |
| Failure mode | Waiting, deadlocks | Failed update → retry/reload |

And remember **constraints**: a `UNIQUE` index is the only reliable guard against two concurrent "insert if not exists" operations.

---

## 5. Where transaction boundaries belong

- **Business-operation scope:** a transaction should wrap one *unit of work* (place an order = reserve stock + insert order + insert items), usually at the **service** layer, not inside each DAO method. DAO methods then take the shared `Connection` or participate in the current transaction ([JDBC Patterns](06_jdbc-patterns.md)).
- **Keep transactions short.** An open transaction holds locks, a snapshot, and a pooled connection. Never wait for user input, call remote services, send emails, or sleep inside one. Do that before or after.
- **Don't swallow exceptions** and then commit. If a statement fails mid-transaction, PostgreSQL marks the transaction as aborted (`current transaction is aborted, commands ignored until end of transaction block`). Roll back, or use a savepoint.
- **Read-only transactions:** `conn.setReadOnly(true)` is a hint that lets some databases and pools optimize (and route to replicas).
- **Idempotency:** commit and *then* tell the outside world, and design retries so a repeated request doesn't apply twice ([Idempotency](../25-real-world-patterns/04_idempotency.md)).

---

## 6. Deadlocks and retries

A **deadlock** occurs when two transactions each hold a lock the other needs. The database detects the cycle and **aborts one transaction** with an error (PostgreSQL SQLState `40P01`, MySQL error 1213, Oracle `ORA-00060`):

```text
 Tx A: UPDATE accounts ... WHERE id = 1;   (locks row 1)    Tx B: UPDATE accounts ... WHERE id = 2;  (locks row 2)
 Tx A: UPDATE accounts ... WHERE id = 2;   (waits for B)     Tx B: UPDATE accounts ... WHERE id = 1;  (waits for A)  → deadlock
```

Prevention: **access rows in a consistent order** (always lower account ID first), keep transactions short, and use the lowest isolation level that is correct. The same ideas as application-level deadlocks ([Deadlock, Livelock, Starvation](../14-concurrency/14_deadlock-livelock-starvation.md)).

**Deadlocks and serialization failures are expected under load**, so the correct response is to **retry the whole transaction**:

```java
@FunctionalInterface interface SqlWork<T> { T run() throws SQLException; }

static <T> T withRetry(int maxAttempts, SqlWork<T> work) throws SQLException {
    for (int attempt = 1; ; attempt++) {
        try {
            return work.run();                              // the work opens its own connection and transaction
        } catch (SQLException e) {
            if (!isRetryable(e) || attempt >= maxAttempts) throw e;
            try { Thread.sleep(ThreadLocalRandom.current().nextLong(10, 50L * attempt)); }   // backoff with jitter
            catch (InterruptedException ie) { Thread.currentThread().interrupt(); throw e; }
        }
    }
}

static boolean isRetryable(SQLException e) {
    String state = e.getSQLState();
    return e instanceof SQLTransientException            // includes SQLTransactionRollbackException
        || "40001".equals(state)                         // serialization failure
        || "40P01".equals(state);                        // deadlock (PostgreSQL)
}
```

The retry must **re-run the entire transaction** (not just the failed statement), and the work must be safe to repeat: no external side effects inside it. See [Retry and Backoff](../25-real-world-patterns/02_retry-and-backoff.md).

---

## 7. Transactions in frameworks

Frameworks manage the connection and the begin/commit/rollback for you. In **Spring**:

```java
@Service
class OrderService {
    @Transactional                                  // begin → run → commit (or rollback on failure)
    public void placeOrder(NewOrder order) { ... }
}
```

What the annotation hides, you now know: one connection is bound to the thread for the duration, all repository calls reuse it, and commit/rollback happens when the method exits. Gotchas:

- **Rollback rules:** by default Spring rolls back on **unchecked** exceptions (and `Error`), and **commits** on checked exceptions unless you configure `rollbackFor`.
- **Self-invocation:** `@Transactional` works through a **proxy**, so a call from one method to another on `this` bypasses it ([the self-invocation trap](../13-advanced-language-features/02_dynamic-proxies.md#7-the-self-invocation-trap)).
- **Propagation** (`REQUIRED` by default: join the existing transaction; `REQUIRES_NEW`: suspend it and start another connection and transaction) and **isolation** can be set per method.
- The transaction is **thread-bound**. Work moved to another thread (`@Async`, executor, `CompletableFuture`) runs **outside** it ([ThreadLocal](../14-concurrency/09_thread-local.md#4-thread-locals-and-asynchronous-code)).
- Transaction scope is *also* connection-hold time, so a `@Transactional` method that calls a slow API holds a pool connection that whole time ([Connection Pooling](05_connection-pooling.md)).

### Across services and databases

A transaction covers **one database**. Updating a database and sending a message or calling another service can't be made atomic by a local transaction. Distributed transactions (XA/two-phase commit) exist but are slow and fragile. Common alternatives: the **transactional outbox** (write the event into the same database transaction, and publish it afterwards), **sagas** with compensating actions, and **idempotent** consumers ([Idempotency](../25-real-world-patterns/04_idempotency.md), [Background Jobs](../25-real-world-patterns/05_background-jobs.md)).

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Assuming auto-commit is off | It's **on** by default. Call `setAutoCommit(false)` for multi-statement work |
| Not rolling back on exceptions | `try/catch` with `rollback()`, or a framework |
| Using different pooled connections for steps of one logical transaction | One `Connection` for the whole unit of work |
| Read-then-write without protection (check-then-act) | Atomic `UPDATE ... WHERE`, `FOR UPDATE`, or a version column |
| Long transactions with remote calls or user waits | Keep them short and DB-only |
| Treating a deadlock or `40001` as a fatal error | Retry the whole transaction with backoff |
| Raising isolation globally "to be safe" | Use the lowest correct level, retry on conflicts |
| Catching `SQLException` inside the transaction and continuing | In PostgreSQL the transaction is aborted. Roll back or use a savepoint |
| Assuming `@Transactional` applies to self-calls or other threads | It works through proxies and thread-bound connections |
| Checked exception in Spring `@Transactional` method and expecting rollback | Configure `rollbackFor` |
| Relying on application checks for uniqueness | `UNIQUE` constraints, and handle `23505` |
| Doing side effects (emails, HTTP calls) before commit | Do them after, or via an outbox |

### Debugging

- Intermittent wrong data (oversold stock, duplicate rows, lost updates) → a missing atomic statement, lock, version, or constraint. Reproduce with two concurrent requests.
- Many connections `idle in transaction` (PostgreSQL: `SELECT * FROM pg_stat_activity WHERE state = 'idle in transaction'`) → code opened a transaction and isn't finishing it (an exception path without rollback, or long non-DB work inside).
- Blocked queries → find blockers (`pg_locks`, `SHOW ENGINE INNODB STATUS`, `sys.dm_tran_locks`) and the statements holding them.
- Frequent deadlock errors → inconsistent lock order across code paths. Log the transaction's statements and compare.
- `current transaction is aborted...` → an earlier statement failed and the code kept going. Roll back.

---

## Quick Summary

- A transaction is **all-or-nothing** (ACID). JDBC connections start in **auto-commit**. Use `setAutoCommit(false)` + `commit()` / `rollback()` on **one connection**.
- Isolation levels trade anomalies for concurrency: **Read Committed** (common default), Repeatable Read, Serializable. Defaults differ by database (PostgreSQL/Oracle: Read Committed; MySQL: Repeatable Read).
- Prevent lost updates with an **atomic `UPDATE ... WHERE`**, **`SELECT ... FOR UPDATE`** (pessimistic, `SKIP LOCKED` for queues), or a **version column** (optimistic). Rely on **constraints** for uniqueness.
- Keep transactions **short**, and DB-only. No remote calls inside.
- **Deadlocks and serialization failures happen**: lock in a consistent order and **retry the whole transaction** with backoff.
- `@Transactional` is a proxy over exactly this: mind rollback rules, self-invocation, and thread boundaries. Cross-system consistency needs an outbox/saga, not a bigger transaction.

**Next:** [Connection Pooling](05_connection-pooling.md)
