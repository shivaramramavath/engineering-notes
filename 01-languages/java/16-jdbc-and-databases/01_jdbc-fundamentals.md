# JDBC Fundamentals

**JDBC** (Java Database Connectivity) is the standard Java API for talking to relational databases. It's a set of interfaces in `java.sql` and `javax.sql`; each database vendor supplies a **driver** that implements them. Your code programs against the interfaces, so switching from PostgreSQL to MySQL mostly means a different driver and URL (plus any SQL dialect differences).

Every Java database library, from Hibernate to Spring's `JdbcTemplate` to jOOQ, ultimately calls JDBC. Knowing it is what lets you read their stack traces, logs, and metrics.

**Prerequisites:** [SQL and Indexing Essentials](00_sql-and-indexing-essentials.md), [try-with-resources](../06-exceptions-and-debugging/03_try-with-resources.md).

---

## 1. The pieces

```text
 DriverManager / DataSource ──► Connection ──► PreparedStatement ──► ResultSet
   (how you get a connection)   (a session)     (a SQL command)       (rows back)
```

| Type | Role |
|---|---|
| **Driver** | Vendor-supplied library (a JAR) that speaks the database's wire protocol |
| **`DriverManager`** | Old-style factory: `getConnection(url, user, password)` |
| **`DataSource`** (`javax.sql`) | The **preferred** way to obtain connections. Pools implement it |
| **`Connection`** | One session with the database. Holds transaction state |
| **`Statement` / `PreparedStatement` / `CallableStatement`** | Send SQL. Use `PreparedStatement` ([note 02](02_statements-and-prepared-statements.md)) |
| **`ResultSet`** | A cursor over the rows a query returned ([note 03](03_resultset-and-data-mapping.md)) |
| **`SQLException`** | Everything that goes wrong |

---

## 2. Setting up: driver and URL

Add the driver as a dependency (Maven shown):

```xml
<dependency>
    <groupId>org.postgresql</groupId>
    <artifactId>postgresql</artifactId>
    <version><!-- current release --></version>
</dependency>
```

A JDBC **URL** identifies the database and driver:

| Database | URL pattern |
|---|---|
| PostgreSQL | `jdbc:postgresql://host:5432/dbname` |
| MySQL | `jdbc:mysql://host:3306/dbname` |
| SQL Server | `jdbc:sqlserver://host:1433;databaseName=dbname` |
| Oracle | `jdbc:oracle:thin:@//host:1521/service` |
| H2 (in-memory, tests) | `jdbc:h2:mem:testdb` |
| SQLite | `jdbc:sqlite:/path/to/file.db` |

Connection options go in the URL query string or properties (`?sslmode=require`, `?connectTimeout=5`, and so on). They are **driver-specific**, so check your driver's documentation.

Since JDBC 4.0 (Java 6) drivers register themselves through the `ServiceLoader` mechanism, so the old `Class.forName("org.postgresql.Driver")` line is no longer needed.

---

## 3. A first query

```java
String url = "jdbc:postgresql://localhost:5432/shop";

try (Connection conn = DriverManager.getConnection(url, "app", password);
     PreparedStatement ps = conn.prepareStatement(
             "SELECT id, email, name FROM users WHERE id = ?")) {

    ps.setLong(1, 42);                                     // parameters are 1-based

    try (ResultSet rs = ps.executeQuery()) {
        if (rs.next()) {                                   // move to the first row
            System.out.println(rs.getLong("id") + " " + rs.getString("email"));
        }
    }
}   // everything closed, in reverse order
```

Three things to notice:

1. **Everything is closed with try-with-resources.** `Connection`, `PreparedStatement`, and `ResultSet` all hold database and driver resources.
2. **Parameters use `?` placeholders**, never string concatenation ([note 02](02_statements-and-prepared-statements.md)).
3. `DriverManager` opens a **new physical connection** on every call. That's fine in a demo and wrong in a server (section 4).

### Running statements

| Method | Use for | Returns |
|---|---|---|
| `executeQuery()` | `SELECT` | `ResultSet` |
| `executeUpdate()` | `INSERT`/`UPDATE`/`DELETE`/DDL | Affected row count (`int`). `executeLargeUpdate()` returns `long` |
| `execute()` | Unknown/multiple results | `boolean` (true if the first result is a `ResultSet`) |

```java
int updated = ps.executeUpdate();     // e.g. "UPDATE orders SET status=? WHERE id=?"
if (updated == 0) { /* nothing matched */ }
```

---

## 4. `DataSource` and why you don't use `DriverManager` in servers

Opening a connection involves a TCP handshake, usually TLS, authentication, and server-side session setup, which can take **tens of milliseconds or more**. Databases also limit concurrent connections (PostgreSQL's default `max_connections` is 100). So servers keep a **pool** of open connections and lend them out ([Connection Pooling](05_connection-pooling.md)).

A pool is exposed as a `javax.sql.DataSource`:

```java
HikariConfig cfg = new HikariConfig();
cfg.setJdbcUrl("jdbc:postgresql://localhost:5432/shop");
cfg.setUsername("app");
cfg.setPassword(password);
cfg.setMaximumPoolSize(10);

DataSource ds = new HikariDataSource(cfg);      // create ONCE, share application-wide

try (Connection conn = ds.getConnection()) {    // borrows from the pool
    ...
}                                                // close() RETURNS it to the pool; it is not physically closed
```

Code should depend on `DataSource` (or `Connection` passed in), not on how it's configured. Frameworks (Spring Boot, Quarkus, Jakarta EE) create and manage the `DataSource` for you from configuration.

---

## 5. Closing resources

Connections, statements, and result sets must be closed. A leaked connection from a pool is **never returned**, and enough leaks exhaust the pool, so the application hangs ([Connection Pooling](05_connection-pooling.md#4-failure-modes)).

- Close in reverse order of creation. try-with-resources does this for you.
- Closing a `Connection` closes its statements, and closing a `Statement` closes its `ResultSet`s, but don't rely on that. Be explicit, because pooled connections are *returned*, not closed, and a pool may not clean up statements immediately.
- Never return a `ResultSet` (or a lazy stream over it) out of the method that opened the connection. Map to objects first ([note 03](03_resultset-and-data-mapping.md)).

```java
// WRONG: connection and statement leak if executeQuery() throws, and the ResultSet is unusable after return
Connection c = ds.getConnection();
ResultSet rs = c.createStatement().executeQuery(sql);
return rs;
```

---

## 6. `SQLException`

Every JDBC failure is a (checked) `SQLException`. It carries more than a message:

```java
try {
    ps.executeUpdate();
} catch (SQLException e) {
    e.getSQLState();     // 5-character standard code, e.g. "23505" (unique violation)
    e.getErrorCode();    // vendor-specific number
    e.getMessage();      // human-readable (driver/DB specific)
    e.getNextException();// chained exceptions (e.g. batch failures)
    e.getCause();
}
```

**Branch on the SQLState or subclass, never on message text.**

| SQLState class | Meaning | Examples |
|---|---|---|
| `08xxx` | Connection problem | Server down, connection lost |
| `22xxx` | Data exception | Value too long, bad numeric/date format |
| `23xxx` | **Integrity constraint violation** | `23505` unique violation, `23503` foreign-key violation, `23502` NOT NULL |
| `40xxx` | Transaction rollback | `40001` serialization failure, `40P01` deadlock (PostgreSQL) |
| `42xxx` | Syntax error or access rule | `42P01` undefined table, `42601` syntax error |

JDBC 4 also provides a subclass hierarchy that is often easier to catch than SQLState codes:

| Subclass | Meaning | Typical handling |
|---|---|---|
| `SQLIntegrityConstraintViolationException` | Constraint violated (support varies by driver) | Translate to a domain error ("email already used") |
| `SQLTransientException` (`SQLTransactionRollbackException`, `SQLTimeoutException`, `SQLTransientConnectionException`) | **Might succeed if retried** | Retry the transaction with backoff ([Transactions](04_transactions.md#6-deadlocks-and-retries)) |
| `SQLNonTransientException` (syntax error, bad data) | **Retry won't help** | Fix the code/data |
| `SQLRecoverableException` | Recoverable after recovery steps (reconnect) | Get a fresh connection |
| `BatchUpdateException` | A batch partly failed | Inspect update counts ([note 02](02_statements-and-prepared-statements.md#5-batching)) |

How drivers populate these varies. SQLState is the most portable signal, but check your database's documentation. Don't leak `SQLException` through your whole application: translate it at the data-access boundary ([JDBC Patterns](06_jdbc-patterns.md#6-error-translation)).

---

## 7. Timeouts: always set them

Without timeouts, a slow or dead database ties up threads indefinitely.

| Timeout | Where | What it limits |
|---|---|---|
| Connect/login timeout | URL/driver property or pool config | Time to establish the connection |
| Socket/network timeout | Driver property, `Connection.setNetworkTimeout` | Time to wait on the network for a response |
| Query timeout | `Statement.setQueryTimeout(seconds)` | Total time for one statement (throws `SQLTimeoutException`) |
| Pool acquire timeout | Pool config (`connectionTimeout` in HikariCP) | Time to wait for a free pooled connection |

```java
ps.setQueryTimeout(5);        // seconds
```

Make the layers consistent: the *query* timeout should fire before the *socket* timeout, which should fire before any upstream request timeout, so failures are clean.

---

## 8. Other basics

**Metadata:** `conn.getMetaData()` (`DatabaseMetaData`) describes the database and driver (product name, supported features, tables, columns). `rs.getMetaData()` (`ResultSetMetaData`) describes the columns of a result, useful for generic tools.

**Local development:**

```bash
docker run --name pg -e POSTGRES_PASSWORD=secret -e POSTGRES_DB=shop -p 5432:5432 -d postgres
```

Prefer a real engine in Docker (or Testcontainers in tests) over H2 for anything beyond toy examples, because SQL dialects and behavior differ ([SQL Essentials](00_sql-and-indexing-essentials.md#8-dialect-differences-youll-meet)).

**Never hard-code credentials.** Load them from configuration or a secrets manager ([Secrets Management](../21-security/03_secrets-management.md)), and use TLS to the database in real environments. Give the application's database user only the privileges it needs.

**JDBC versions:** JDBC 4.2 (Java 8) added `java.time` types and `executeLargeUpdate`; JDBC 4.3 (Java 9) added sharding APIs and minor changes. Driver support varies, so check yours.

**Virtual threads:** JDBC calls are blocking, which is fine on virtual threads ([Virtual Threads](../14-concurrency/13_virtual-threads-and-structured-concurrency.md)). The **connection pool size**, not the thread count, is what limits database concurrency. Old drivers that block inside `synchronized` could pin carrier threads before Java 24, so use a recent driver.

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Using `DriverManager.getConnection` per request | A pooled `DataSource` |
| Not closing `Connection`/`PreparedStatement`/`ResultSet` | try-with-resources, always |
| Concatenating values into SQL | `?` parameters ([note 02](02_statements-and-prepared-statements.md)) |
| Returning a `ResultSet` from a method | Map rows to objects inside the `try` |
| Catching `Exception` and printing the stack trace | Handle `SQLException` by SQLState/subclass, then log with context |
| No timeouts | Set connect, socket, query, and pool timeouts |
| Parsing `e.getMessage()` to decide behavior | `getSQLState()` or exception subclasses |
| Hard-coded URL and password | Configuration/secrets |
| Testing only against H2 | Test on your production engine |
| Sharing one `Connection` between threads | `Connection` isn't thread-safe: one per unit of work |

### Debugging

- `No suitable driver found for jdbc:...` → driver JAR missing from the classpath, or a malformed URL.
- `Connection refused` / timeout → host, port, firewall, or the database isn't running.
- `password authentication failed` → credentials, or the server's auth config (`pg_hba.conf` for PostgreSQL).
- `relation "x" does not exist` (SQLState `42P01`) → wrong schema/database, or the migration didn't run.
- Increase logging at the driver or pool level, or wrap the `DataSource` with a query-logging proxy during development. Take care never to log sensitive parameter values in production.

---

## Quick Summary

- JDBC is a standard API (`java.sql`); a **driver** per database implements it. A **URL** picks the database.
- Get connections from a pooled **`DataSource`**, not `DriverManager`. Create it once and share it.
- Flow: `Connection` → `PreparedStatement` → `ResultSet`. Close everything with **try-with-resources**.
- `executeQuery` for `SELECT`, `executeUpdate` for changes (check the row count).
- `SQLException`: use **SQLState** (`23505` unique, `40001`/`40P01` retryable, `08` connection) or subclasses, not message text.
- Set **timeouts**, keep credentials out of code, and test on the real database engine.

**Next:** [Statements and Prepared Statements](02_statements-and-prepared-statements.md)
