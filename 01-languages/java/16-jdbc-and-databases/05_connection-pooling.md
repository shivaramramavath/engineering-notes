# Connection Pooling

Opening a database connection is expensive: a TCP handshake, usually TLS, authentication, and server-side session setup. Databases also cap how many connections they accept, and each connection costs the server memory (PostgreSQL even runs a process per connection). A **connection pool** keeps a bounded set of open connections and **lends** them out, so requests reuse connections instead of creating them.

Pools are also where many production incidents surface: "the app is hanging", "timeouts after 30 seconds", "too many connections". Knowing how a pool behaves turns those from mysteries into a short checklist.

**Prerequisites:** [JDBC Fundamentals](01_jdbc-fundamentals.md), [Transactions](04_transactions.md), [Executors and Thread Pools](../14-concurrency/10_executors-and-thread-pools.md).

---

## 1. How a pool works

```text
   request threads                    pool (max 10)                     database
  ┌──────────────┐  getConnection()  ┌────────────────────┐
  │ thread A ────┼──────────────────►│ idle  idle  idle   │  ◄──── physical connections,
  │ thread B ────┼──────────────────►│ busy  busy  busy   │        opened once and reused
  │ thread C ────┼──── waits ... ───►│ (none free)        │
  └──────────────┘  close() = return └────────────────────┘
```

1. `ds.getConnection()` hands out an **idle** connection, or opens a new one if the pool is below its maximum, or **waits** (up to a timeout) if all are in use.
2. The connection you receive is a **proxy**. `conn.close()` doesn't close the physical connection. It **returns** it to the pool (resetting transaction state, auto-commit, etc.).
3. The pool opens connections, retires old ones, and checks that idle ones are still alive.

This is why "always close your connection" is a rule about **returning** it, and why forgetting is catastrophic: a connection that is never returned is gone from the pool for good.

---

## 2. HikariCP

**HikariCP** is the de-facto standard JDBC pool and the default in Spring Boot. Standalone use:

```java
HikariConfig cfg = new HikariConfig();
cfg.setJdbcUrl("jdbc:postgresql://db:5432/shop");
cfg.setUsername("app");
cfg.setPassword(secret);

cfg.setMaximumPoolSize(10);            // upper bound on connections
cfg.setConnectionTimeout(3_000);       // ms to wait for a free connection before failing (default 30 s)
cfg.setMaxLifetime(25 * 60_000);       // retire connections after this (default 30 min)
cfg.setLeakDetectionThreshold(10_000); // log a stack trace if a connection is held longer than this (default off)
cfg.setPoolName("shop-main");

HikariDataSource ds = new HikariDataSource(cfg);    // ONE per database, for the whole application
// ... on shutdown:
ds.close();
```

In Spring Boot it's configuration:

```properties
spring.datasource.url=jdbc:postgresql://db:5432/shop
spring.datasource.hikari.maximum-pool-size=10
spring.datasource.hikari.connection-timeout=3000
spring.datasource.hikari.max-lifetime=1500000
```

The settings that matter most:

| Setting | Default | Meaning and guidance |
|---|---|---|
| `maximumPoolSize` | 10 | Max connections (idle + in use). The most important knob ([section 3](#3-sizing-the-pool)) |
| `minimumIdle` | same as max | Idle connections kept ready. HikariCP recommends leaving it unset for a **fixed-size pool** |
| `connectionTimeout` | 30,000 ms | How long a caller waits for a connection. Lower it so overload fails fast instead of stacking up threads |
| `maxLifetime` | 30 min | Max age of a connection. Set it **shorter than any database/proxy/firewall idle limit** so the pool retires connections before something kills them |
| `idleTimeout` | 10 min | Retire idle connections (only matters when `minimumIdle < maximumPoolSize`) |
| `keepaliveTime` | disabled | Periodically ping idle connections to keep them alive through network devices |
| `validationTimeout` | 5,000 ms | Time allowed for a liveness check |
| `leakDetectionThreshold` | disabled | A debugging aid: logs where a connection was borrowed if it's not returned in time |

Modern drivers support `Connection.isValid()`, so you don't need a custom test query (`connectionTestQuery`). Defaults can change between versions, so check the version you use.

Other pools exist (Apache Commons DBCP, Tomcat JDBC, Agroal, Vibur, the legacy c3p0), and application servers provide their own. The concepts are identical.

---

## 3. Sizing the pool

**Bigger is not better.** A database can only do useful work on roughly as many queries at once as it has CPU cores and I/O capacity. Past that point, extra connections just add context switching, lock contention, and memory use, and make everything slower.

Starting points:

- A common heuristic (from the HikariCP project's pool-sizing guide): `connections ≈ (database CPU cores × 2) + effective spindle count`. Treat it as a **starting point to measure from**, not a law. For many services a pool of **10-20** per application instance is plenty.
- **Little's Law:** concurrent connections needed ≈ **throughput × average connection hold time**. If you serve 500 transactions/s and each holds a connection 20 ms, you need about `500 × 0.02 = 10` connections *on average*, plus headroom for bursts.
- Shorter transactions need fewer connections. **Speeding up queries and shortening transactions shrinks the pool you need.**

The arithmetic that surprises people:

```text
 total connections to the database  =  pool size × number of application instances (and each tool, job, replica...)

 20 instances × pool of 20  =  400 connections   → exceeds PostgreSQL's default max_connections (100)
```

Account for every client of the database (other services, migrations, admin tools). If the totals don't fit, **reduce per-instance pools**, or put a **server-side pooler** in front (PgBouncer, ProxySQL, cloud-provider proxies). Poolers in *transaction* mode multiplex many client connections over few server connections, but can restrict session-level features (session variables, server-side prepared statements, advisory locks, `LISTEN/NOTIFY`), so check your pooler's documentation and driver settings.

### Relation to your thread pools

If a web server has 200 request threads and the connection pool has 10, at most 10 requests touch the database at once and the rest **wait** on the pool. That is often the *right* behavior (it protects the database), but it means the pool size effectively caps your database-bound concurrency. Size thread pools, connection pools, and downstream limits **together** ([Executors and Thread Pools](../14-concurrency/10_executors-and-thread-pools.md#4-sizing-a-pool)).

With **virtual threads** you can have thousands of threads waiting on the pool. The pool still bounds database concurrency, so don't increase the pool to "match" the thread count ([Virtual Threads](../14-concurrency/13_virtual-threads-and-structured-concurrency.md#3-using-them-well)).

---

## 4. Failure modes

### Pool exhaustion

```text
HikariPool-1 - Connection is not available, request timed out after 30000ms.
```

All connections are in use and a caller waited the full `connectionTimeout`. The pool is just the messenger. Find **why connections aren't coming back fast enough**:

| Cause | Look for |
|---|---|
| **Connection leak**: not closed on some path | Missing try-with-resources; use `leakDetectionThreshold` to get the stack trace of the borrower |
| **Long transactions** | Slow queries, large batch work, or a `@Transactional` method holding a connection while calling a remote service |
| **Slow queries** | Missing indexes, lock waits ([SQL Essentials](00_sql-and-indexing-essentials.md#6-explain-asking-the-database-what-it-will-do)) |
| **Database itself slow or blocked** | Locks, saturated CPU/IO, failing over |
| **Pool too small for the real load** | Pending/waiting threads high while queries are fast and the database is idle |
| **A thread waiting for a connection while holding one** (nested acquisition) | Self-deadlock: pool of N and N threads each needing a second connection |
| **Slow clients** holding a streaming result ([ResultSet](03_resultset-and-data-mapping.md#5-large-results-and-fetch-size)) | Long-running exports on the main pool |

Increasing the pool size without finding the cause usually just moves the failure, because the database becomes the next bottleneck.

### Stale or broken connections

`Connection reset`, `This connection has been closed`, `broken pipe`: a firewall, load balancer, or the database dropped an idle connection. Prevent with `maxLifetime` shorter than the network/database idle timeout, `keepaliveTime`, and a pool that validates connections (all modern pools do). After a database failover, expect a burst of errors until the pool replaces connections. Retry idempotent operations.

### Startup and failover

If the database is down at startup, pools can fail fast or retry depending on configuration (`initializationFailTimeout` in HikariCP). Decide deliberately which behavior your deployment wants.

### Transaction state leakage

A connection returned to the pool should be clean. Pools reset auto-commit, isolation, and read-only flags, and roll back uncommitted work, but custom session state (`SET` variables, temporary tables, advisory locks) can leak to the next borrower. Avoid session-level state, or reset it yourself.

---

## 5. Monitoring

HikariCP exposes metrics (through JMX or Micrometer/Prometheus). Watch these:

| Metric | Healthy | Warning sign |
|---|---|---|
| **Active** connections | Below max most of the time | Pinned at max |
| **Idle** connections | Some available | Zero idle for long periods |
| **Pending / waiting threads** | ~0 | Persistently > 0: callers are queueing |
| **Acquire time** (wait for a connection) | Sub-millisecond to milliseconds | Growing toward `connectionTimeout` |
| **Usage time** (how long connections are held) | Matches your query/transaction time | Spikes: slow queries or long transactions |
| **Timeouts** | 0 | Any |

On the database side (`pg_stat_activity` in PostgreSQL, `SHOW PROCESSLIST` in MySQL), look at the connection count by state: many **`idle in transaction`** connections mean application code opened a transaction and isn't finishing it ([Transactions](04_transactions.md#debugging)).

In a thread dump ([Troubleshooting Playbook](../22-production-engineering/05_troubleshooting-playbook.md)), exhaustion shows as many request threads parked in `HikariPool.getConnection`, while the threads *holding* connections show what is slow.

---

## 6. Good habits

- **One `DataSource` per database, created once**, shut down on application stop. Never create a pool per request.
- **Borrow late, return early:** acquire a connection right before the database work and release immediately after. Don't hold it while calling other services, parsing big responses, or sleeping.
- **try-with-resources every `Connection`**, or let the framework manage it.
- **Set timeouts** at every level: pool acquire, socket, query ([JDBC Fundamentals](01_jdbc-fundamentals.md#7-timeouts-always-set-them)), and make them consistent so failures are fast and clear.
- **Separate pools for separate workloads** when needed (a bulkhead): a small pool for background/batch jobs and reporting so they can't starve interactive requests ([Concurrency Patterns](../14-concurrency/15_concurrency-patterns.md#7-bulkhead-isolate-failure-domains)). Route read-only queries to replicas with their own pool.
- **Don't change the pool size from folklore.** Measure active/pending connections under realistic load, then adjust.
- Keep driver and pool versions current. Pool/driver bugs and defaults change.

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Creating a `DataSource`/pool per request | One shared pool |
| Connection not closed on an error path | try-with-resources |
| Raising the pool size to fix timeouts | Find leaks, long transactions, slow queries first |
| Pool size × instances exceeding the database's `max_connections` | Compute the total, shrink pools, or add a pooler |
| Default 30 s `connectionTimeout` under overload (threads pile up) | A smaller timeout and fail fast |
| `maxLifetime` longer than a firewall/DB idle timeout | Shorter than every idle limit in the path |
| Holding a connection across remote calls/user waits | Narrow the connection/transaction scope |
| One pool for OLTP requests and heavy reports/batches | Separate pools |
| Relying on session state (`SET`, temp tables) across borrows | Avoid or reset it |
| Increasing pool size to match virtual-thread count | The pool is the intended limiter |
| No pool metrics or alerts | Export active/idle/pending/acquire-time and alert on waiting threads |

### Debugging flow

```text
"Connection is not available, request timed out"  /  hangs under load
   ├─ Metrics: active == max, pending > 0?      → exhaustion (confirm)
   ├─ Thread dump: who holds connections, and what are they doing (slow SQL? remote call? idle?)
   ├─ leakDetectionThreshold logs: a borrower that never returns                 → leak
   ├─ Database: pg_stat_activity — idle in transaction / long-running / blocked   → long tx, locks
   └─ Queries fast but waiting still high, DB idle?                               → pool too small for real concurrency
```

---

## Quick Summary

- A pool keeps a bounded set of open connections and lends them out. **`close()` returns the connection**. It doesn't close it.
- **HikariCP** is the standard. Know `maximumPoolSize`, `connectionTimeout`, `maxLifetime`, `leakDetectionThreshold`.
- **Smaller pools are usually better.** Size from throughput × hold time (Little's Law), and multiply by instance count against the database's connection limit.
- "Connection is not available" means connections aren't coming back: **leaks, long transactions, slow queries**, or a slow database. Bigger pools rarely fix it.
- Borrow late and return early, set timeouts, isolate workloads with separate pools, and monitor active/pending/acquire-time.
- With virtual threads, the pool, not the thread count, is the concurrency limit to the database.

**Next:** [JDBC Patterns](06_jdbc-patterns.md)
