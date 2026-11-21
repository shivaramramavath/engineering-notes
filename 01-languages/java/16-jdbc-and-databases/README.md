# 16 · JDBC and Databases

Almost every backend Java application stores its data in a relational database. This module covers the whole path: the SQL and indexing knowledge that decides whether a query takes 2 ms or 20 s, the **JDBC** API that every Java database library is built on, transactions, connection pooling, practical patterns, and finally how ORMs like Hibernate sit on top of it all.

**The central point:** frameworks and ORMs *hide* SQL, connections, and transactions; they don't *remove* them. When production slows down, a pool runs dry, or data goes inconsistent, the fix is always found at the JDBC/SQL level. Learn that level once, and every framework above it becomes understandable.

## Contents

| # | Note | Focus |
|---|------|-------|
| 00 | [SQL and Indexing Essentials](00_sql-and-indexing-essentials.md) | Tables, joins, aggregates, NULL, indexes, `EXPLAIN`, the example schema used in this module |
| 01 | [JDBC Fundamentals](01_jdbc-fundamentals.md) | Drivers, URLs, `DataSource`, `Connection`, `SQLException`, closing resources |
| 02 | [Statements and Prepared Statements](02_statements-and-prepared-statements.md) | Parameters, SQL injection, generated keys, batching |
| 03 | [ResultSet and Data Mapping](03_resultset-and-data-mapping.md) | Reading rows, `NULL`, SQL ↔ Java types, `java.time`, mapping to records, large results |
| 04 | [Transactions](04_transactions.md) | ACID, commit/rollback, isolation levels, locking, deadlocks, retries |
| 05 | [Connection Pooling](05_connection-pooling.md) | Why pools exist, HikariCP, sizing, exhaustion and leaks |
| 06 | [JDBC Patterns](06_jdbc-patterns.md) | Small templates, DAOs, pagination, upsert, N+1, migrations, testing |
| 07 | [From JDBC to ORM](07_from-jdbc-to-orm.md) | JPA/Hibernate concepts, Spring Data, the classic traps, choosing a data-access style |

## The stack

```text
  your code (service / repository)
        │   Spring Data / JPA / jOOQ / JdbcClient   ← optional layers (07)
        ▼
  JDBC API  (java.sql: Connection, PreparedStatement, ResultSet)      01-03
        │
  Connection pool  (HikariCP)                                         05
        │
  JDBC driver  (org.postgresql, mysql-connector-j, ...)               01
        │   network protocol (TCP, usually TLS)
        ▼
  Database server  (SQL engine, indexes, transactions)                00, 04
```

## Suggested path

Read **00 → 01 → 02 → 03 → 04 → 05** in order. Then **06** for patterns and **07** to see what ORMs add (and cost). If you are new to SQL, spend real time on 00, including running the examples.

## Conventions

- Examples use **PostgreSQL** syntax and behavior unless stated. Other databases differ in details (identity columns, `LIMIT`, upserts, defaults); differences are called out where they matter.
- The running schema is `users` and `orders` (defined in note 00).
- Date/time columns map to **`java.time`**, never `java.sql.Timestamp`/`java.util.Date` ([Date-Time Best Practices](../10-date-and-time/05_date-time-best-practices.md)).
- Money is `NUMERIC`/`BigDecimal`, never `double`.

## Prerequisites

[try-with-resources](../06-exceptions-and-debugging/03_try-with-resources.md), [Exception handling](../06-exceptions-and-debugging/README.md), [Collections](../08-collections/README.md), [java.time](../10-date-and-time/00_java-time-overview.md), and [Records](../12-modern-java/01_records.md) for row types.

## Related

[Repository and DAO](../24-design-patterns/04-architecture/02_repository-and-dao.md) · [Pagination](../25-real-world-patterns/01_pagination.md) · [Idempotency](../25-real-world-patterns/04_idempotency.md) · [Integration Testing](../19-testing/03_integration-testing.md) · [Input Validation and Injection](../21-security/00_input-validation-and-injection.md) · [JDBC and persistence interview questions](../28-interview/09_jdbc-and-persistence.md)

**Next module:** [JSON and Data Formats](../17-json-and-data-formats/README.md)
