# SQL Track Topic Index (INDEX.md)

This index provides a comprehensive cross-reference for all concepts, statements, clauses, functions, algorithms, and architectural patterns covered across the 15 modules of the SQL track.

---

## 🗂️ Categorical Index

### 1. Engine Architecture & Query Execution
- **Architecture Overview**: [00-setup/01-what-is-sql-and-the-relational-model.md](./00-setup/01-what-is-sql-and-the-relational-model.md)
- **RDBMS Engine Landscape**: [00-setup/02-rdbms-landscape-and-dialects.md](./00-setup/02-rdbms-landscape-and-dialects.md)
- **Installation & Environment Setup**: [00-setup/03-installing-postgresql-mysql-sqlite.md](./00-setup/03-installing-postgresql-mysql-sqlite.md)
- **Client Tools & CLIs**: [00-setup/04-clients-and-tools.md](./00-setup/04-clients-and-tools.md)
- **Sample Databases Reference**: [00-setup/05-sample-databases-used-in-this-track.md](./00-setup/05-sample-databases-used-in-this-track.md)
- **SQL Standards (SQL:2023)**: [00-setup/06-sql-standard-and-how-to-read-the-docs.md](./00-setup/06-sql-standard-and-how-to-read-the-docs.md)
- **Query Execution Lifecycle (Parse, Rewrite, Plan, Exec)**: [00-setup/07-how-a-query-runs.md](./00-setup/07-how-a-query-runs.md)

### 2. SQL Fundamentals & Core Operations
- **Data Types (Integer, Numeric, Text, Temporal, Boolean, Binary)**: [01-fundamentals/01-data-types.md](./01-fundamentals/01-data-types.md)
- **Table Definition (CREATE, ALTER, DROP, TRUNCATE)**: [01-fundamentals/02-create-alter-drop-table.md](./01-fundamentals/02-create-alter-drop-table.md)
- **Constraints (PRIMARY KEY, FOREIGN KEY, UNIQUE, NOT NULL, CHECK)**: [01-fundamentals/03-constraints.md](./01-fundamentals/03-constraints.md)
- **SELECT Statement Structure & Evaluation Order**: [01-fundamentals/04-select-statement.md](./01-fundamentals/04-select-statement.md)
- **Filtering with WHERE & Operators**: [01-fundamentals/05-where-filtering.md](./01-fundamentals/05-where-filtering.md)
- **Ordering & Sorting (ORDER BY, NULLS FIRST/LAST, Collation)**: [01-fundamentals/06-order-by.md](./01-fundamentals/06-order-by.md)
- **Pagination (LIMIT, OFFSET, FETCH FIRST)**: [01-fundamentals/07-limit-offset.md](./01-fundamentals/07-limit-offset.md)
- **Three-Valued Logic & NULL Semantics**: [01-fundamentals/08-null-semantics.md](./01-fundamentals/08-null-semantics.md)
- **Pattern Matching (LIKE, ILIKE, SIMILAR TO, Regex)**: [01-fundamentals/09-pattern-matching.md](./01-fundamentals/09-pattern-matching.md)
- **Scalar Numeric & Math Functions**: [01-fundamentals/10-numeric-functions.md](./01-fundamentals/10-numeric-functions.md)
- **String Manipulation Functions**: [01-fundamentals/11-string-functions.md](./01-fundamentals/11-string-functions.md)
- **Date, Time & Timezone Handling**: [01-fundamentals/12-date-and-time.md](./01-fundamentals/12-date-and-time.md)
- **Conditional Logic (CASE, COALESCE, NULLIF, GREATEST/LEAST)**: [01-fundamentals/13-case-and-conditionals.md](./01-fundamentals/13-case-and-conditionals.md)
- **Data Modification (INSERT, UPDATE, DELETE)**: [01-fundamentals/14-insert-update-delete.md](./01-fundamentals/14-insert-update-delete.md)
- **Upsert Patterns (ON CONFLICT DO UPDATE, MERGE)**: [01-fundamentals/15-upsert-and-merge.md](./01-fundamentals/15-upsert-and-merge.md)
- **SQL Coding Style & Conventions**: [01-fundamentals/16-comments-style-and-formatting-conventions.md](./01-fundamentals/16-comments-style-and-formatting-conventions.md)

### 3. Advanced Querying & Analytical Transformations
- **Relational Joins (INNER, LEFT, RIGHT, FULL, CROSS, NATURAL)**: [02-querying/01-joins.md](./02-querying/01-joins.md)
- **Self Joins & Hierarchies**: [02-querying/02-self-joins.md](./02-querying/02-self-joins.md)
- **Set Operations (UNION, INTERSECT, EXCEPT / MINUS)**: [02-querying/03-set-operations.md](./02-querying/03-set-operations.md)
- **Scalar & Correlated Subqueries**: [02-querying/04-subqueries.md](./02-querying/04-subqueries.md)
- **Subquery Quantifiers (EXISTS, IN, ANY, ALL)**: [02-querying/05-exists-in-any-all.md](./02-querying/05-exists-in-any-all.md)
- **Grouping & Aggregation (GROUP BY, HAVING)**: [02-querying/06-aggregation-and-group-by.md](./02-querying/06-aggregation-and-group-by.md)
- **Multi-Dimensional Aggregation (GROUPING SETS, ROLLUP, CUBE)**: [02-querying/07-grouping-sets-rollup-cube.md](./02-querying/07-grouping-sets-rollup-cube.md)
- **Window Functions Foundations (OVER, PARTITION BY, ORDER BY)**: [02-querying/08-window-functions-basics.md](./02-querying/08-window-functions-basics.md)
- **Window Framing (ROWS, RANGE, GROUPS, EXCLUDE)**: [02-querying/09-window-framing.md](./02-querying/09-window-framing.md)
- **Ranking Functions (ROW_NUMBER, RANK, DENSE_RANK, NTILE)**: [02-querying/10-ranking-functions.md](./02-querying/10-ranking-functions.md)
- **Value & Offset Window Functions (LAG, LEAD, FIRST_VALUE, LAST_VALUE)**: [02-querying/11-value-window-functions.md](./02-querying/11-value-window-functions.md)
- **Common Table Expressions (CTEs)**: [02-querying/12-common-table-expressions.md](./02-querying/12-common-table-expressions.md)
- **Recursive CTEs (Hierarchies, Graphs, Sequences)**: [02-querying/13-recursive-ctes.md](./02-querying/13-recursive-ctes.md)
- **Correlated Subqueries vs Lateral Joins (CROSS JOIN LATERAL)**: [02-querying/14-lateral-joins.md](./02-querying/14-lateral-joins.md)
- **Semi-Joins and Anti-Joins**: [02-querying/15-semi-joins-and-anti-joins.md](./02-querying/15-semi-joins-and-anti-joins.md)
- **Conditional Aggregation & Pivot / Unpivot**: [02-querying/16-conditional-aggregation.md](./02-querying/16-conditional-aggregation.md)
- **JSON & JSONB Querying & Indexing**: [02-querying/17-json-and-semi-structured.md](./02-querying/17-json-and-semi-structured.md)
- **Arrays & Composite Types**: [02-querying/18-arrays-and-composite-types.md](./02-querying/18-arrays-and-composite-types.md)
- **Full-Text Search (tsvector, tsquery, GiST/GIN)**: [02-querying/19-full-text-search.md](./02-querying/19-full-text-search.md)
- **Readable & Maintainable SQL Standards**: [02-querying/20-writing-readable-sql.md](./02-querying/20-writing-readable-sql.md)

### 4. Database Design & Modeling
- **Relational Data Modeling & ER Diagrams**: [03-database-design/01-relational-data-modeling.md](./03-database-design/01-relational-data-modeling.md)
- **Normalization (1NF, 2NF, 3NF, BCNF)**: [03-database-design/02-normalization-1nf-to-bcnf.md](./03-database-design/02-normalization-1nf-to-bcnf.md)
- **Pragmatic Denormalization**: [03-database-design/03-denormalization-tradeoffs.md](./03-database-design/03-denormalization-tradeoffs.md)
- **Primary Key Strategies (BIGINT, UUIDv4, UUIDv7, ULID, Natural)**: [03-database-design/04-primary-key-strategies.md](./03-database-design/04-primary-key-strategies.md)
- **Foreign Keys & Referential Integrity**: [03-database-design/05-foreign-keys-and-referential-actions.md](./03-database-design/05-foreign-keys-and-referential-actions.md)
- **Modeling Relationships (1:1, 1:N, N:M, Polymorphic)**: [03-database-design/06-modeling-relationships.md](./03-database-design/06-modeling-relationships.md)
- **Hierarchical & Tree Data Modeling**: [03-database-design/07-modeling-hierarchical-data.md](./03-database-design/07-modeling-hierarchical-data.md)
- **Temporal & History Tables**: [03-database-design/08-temporal-data-and-history.md](./03-database-design/08-temporal-data-and-history.md)
- **Soft Delete Design Patterns**: [03-database-design/09-soft-delete-patterns.md](./03-database-design/09-soft-delete-patterns.md)
- **Multi-Tenancy Architectures**: [03-database-design/10-multi-tenancy-schemas.md](./03-database-design/10-multi-tenancy-schemas.md)
- **Audit Logging & Change Data Capture (CDC)**: [03-database-design/11-audit-logging-patterns.md](./03-database-design/11-audit-logging-patterns.md)
- **Zero-Downtime Schema Migrations**: [03-database-design/12-schema-evolution-and-migrations.md](./03-database-design/12-schema-evolution-and-migrations.md)

### 5. Transactions, Concurrency & Locking
- **ACID Fundamentals & Engine Invariants**: [04-transactions-and-concurrency/01-acid-properties.md](./04-transactions-and-concurrency/01-acid-properties.md)
- **Transaction Boundaries & Savepoints**: [04-transactions-and-concurrency/02-transaction-control-and-savepoints.md](./04-transactions-and-concurrency/02-transaction-control-and-savepoints.md)
- **ANSI vs Modern Isolation Levels**: [04-transactions-and-concurrency/03-isolation-levels-and-anomalies.md](./04-transactions-and-concurrency/03-isolation-levels-and-anomalies.md)
- **MVCC Architecture & Tuple Visibility**: [04-transactions-and-concurrency/04-mvcc-internals.md](./04-transactions-and-concurrency/04-mvcc-internals.md)
- **Row-Level Locking & Pessimistic Control**: [04-transactions-and-concurrency/05-row-level-locking.md](./04-transactions-and-concurrency/05-row-level-locking.md)
- **Table-Level Locks & Explicit Locking**: [04-transactions-and-concurrency/06-table-level-locks.md](./04-transactions-and-concurrency/06-table-level-locks.md)
- **Deadlock Detection, Prevention & Handling**: [04-transactions-and-concurrency/07-deadlocks.md](./04-transactions-and-concurrency/07-deadlocks.md)
- **Optimistic vs Pessimistic Concurrency Control**: [04-transactions-and-concurrency/08-optimistic-vs-pessimistic-locking.md](./04-transactions-and-concurrency/08-optimistic-vs-pessimistic-locking.md)
- **Advisory Locks & Distributed Application Coordination**: [04-transactions-and-concurrency/09-advisory-locks.md](./04-transactions-and-concurrency/09-advisory-locks.md)
- **Write-Ahead Logging (WAL) & Crash Recovery**: [04-transactions-and-concurrency/10-wal-and-durability.md](./04-transactions-and-concurrency/10-wal-and-durability.md)
- **Vacuuming, Transaction Wraparound & Bloat**: [04-transactions-and-concurrency/11-vacuum-freeze-and-bloat.md](./04-transactions-and-concurrency/11-vacuum-freeze-and-bloat.md)

### 6. Indexing & Query Performance
- **B-Tree Index Internals & Page Layout**: [05-indexing-and-performance/01-b-tree-indexes.md](./05-indexing-and-performance/01-b-tree-indexes.md)
- **Composite Indexes & Column Ordering**: [05-indexing-and-performance/02-composite-indexes.md](./05-indexing-and-performance/02-composite-indexes.md)
- **Specialized Indexes (Hash, GIN, GiST, BRIN, Bloom)**: [05-indexing-and-performance/03-gin-gist-brin-indexes.md](./05-indexing-and-performance/03-gin-gist-brin-indexes.md)
- **Partial & Filtered Indexes**: [05-indexing-and-performance/04-partial-indexes.md](./05-indexing-and-performance/04-partial-indexes.md)
- **Expression & Functional Indexes**: [05-indexing-and-performance/05-expression-indexes.md](./05-indexing-and-performance/05-expression-indexes.md)
- **Covering Indexes & Index-Only Scans**: [05-indexing-and-performance/06-covering-indexes-and-index-only-scans.md](./05-indexing-and-performance/06-covering-indexes-and-index-only-scans.md)
- **EXPLAIN & EXPLAIN ANALYZE Reading**: [05-indexing-and-performance/07-explain-analyze-deep-dive.md](./05-indexing-and-performance/07-explain-analyze-deep-dive.md)
- **Query Planner Statistics & Histogram Buckets**: [05-indexing-and-performance/08-query-planner-and-statistics.md](./05-indexing-and-performance/08-query-planner-and-statistics.md)
- **Physical Join Algorithms (Nested Loop, Hash, Merge)**: [05-indexing-and-performance/09-join-algorithms.md](./05-indexing-and-performance/09-join-algorithms.md)
- **Index Maintenance, Bloat & REINDEX CONCURRENTLY**: [05-indexing-and-performance/10-index-maintenance-and-bloat.md](./05-indexing-and-performance/10-index-maintenance-and-bloat.md)
- **Sargability & Query Anti-Patterns**: [05-indexing-and-performance/11-sargability-and-query-antipatterns.md](./05-indexing-and-performance/11-sargability-and-query-antipatterns.md)
- **Table Partitioning (Range, List, Hash)**: [05-indexing-and-performance/12-table-partitioning.md](./05-indexing-and-performance/12-table-partitioning.md)
- **Connection Pooling & Query Caching**: [05-indexing-and-performance/13-connection-pooling-and-caching.md](./05-indexing-and-performance/13-connection-pooling-and-caching.md)
- **Diagnosing Slow Queries & pg_stat_statements**: [05-indexing-and-performance/14-benchmarking-and-profiling.md](./05-indexing-and-performance/14-benchmarking-and-profiling.md)

---

## 🔤 Alphabetical Index (A–Z)

### A
- **ACID**: [04-transactions-and-concurrency/01-acid-properties.md](./04-transactions-and-concurrency/01-acid-properties.md)
- **Advisory Locks**: [04-transactions-and-concurrency/09-advisory-locks.md](./04-transactions-and-concurrency/09-advisory-locks.md)
- **ALTER TABLE**: [01-fundamentals/02-create-alter-drop-table.md](./01-fundamentals/02-create-alter-drop-table.md)
- **Anti-Joins**: [02-querying/15-semi-joins-and-anti-joins.md](./02-querying/15-semi-joins-and-anti-joins.md)
- **Arrays**: [02-querying/18-arrays-and-composite-types.md](./02-querying/18-arrays-and-composite-types.md)
- **Audit Logging**: [03-database-design/11-audit-logging-patterns.md](./03-database-design/11-audit-logging-patterns.md)

### B
- **B-Tree Index**: [05-indexing-and-performance/01-b-tree-indexes.md](./05-indexing-and-performance/01-b-tree-indexes.md)
- **Backups & PITR**: [07-administration-and-security/03-backup-and-restore.md](./07-administration-and-security/03-backup-and-restore.md)
- **BCNF**: [03-database-design/02-normalization-1nf-to-bcnf.md](./03-database-design/02-normalization-1nf-to-bcnf.md)
- **Bloat (Table & Index)**: [04-transactions-and-concurrency/11-vacuum-freeze-and-bloat.md](./04-transactions-and-concurrency/11-vacuum-freeze-and-bloat.md), [05-indexing-and-performance/10-index-maintenance-and-bloat.md](./05-indexing-and-performance/10-index-maintenance-and-bloat.md)
- **BRIN Index**: [05-indexing-and-performance/03-gin-gist-brin-indexes.md](./05-indexing-and-performance/03-gin-gist-brin-indexes.md)

### C
- **CASE Expression**: [01-fundamentals/13-case-and-conditionals.md](./01-fundamentals/13-case-and-conditionals.md)
- **CHECK Constraint**: [01-fundamentals/03-constraints.md](./01-fundamentals/03-constraints.md)
- **ClickHouse (OLAP)**: [08-dialects-and-engines/06-olap-engines-duckdb-clickhouse.md](./08-dialects-and-engines/06-olap-engines-duckdb-clickhouse.md)
- **COALESCE**: [01-fundamentals/13-case-and-conditionals.md](./01-fundamentals/13-case-and-conditionals.md)
- **Common Table Expressions (CTE)**: [02-querying/12-common-table-expressions.md](./02-querying/12-common-table-expressions.md)
- **Composite Index**: [05-indexing-and-performance/02-composite-indexes.md](./05-indexing-and-performance/02-composite-indexes.md)
- **Connection Pooling**: [05-indexing-and-performance/13-connection-pooling-and-caching.md](./05-indexing-and-performance/13-connection-pooling-and-caching.md)
- **Covering Index**: [05-indexing-and-performance/06-covering-indexes-and-index-only-scans.md](./05-indexing-and-performance/06-covering-indexes-and-index-only-scans.md)
- **CROSS JOIN**: [02-querying/01-joins.md](./02-querying/01-joins.md)
- **CROSS JOIN LATERAL**: [02-querying/14-lateral-joins.md](./02-querying/14-lateral-joins.md)
- **CUBE**: [02-querying/07-grouping-sets-rollup-cube.md](./02-querying/07-grouping-sets-rollup-cube.md)

### D
- **Deadlocks**: [04-transactions-and-concurrency/07-deadlocks.md](./04-transactions-and-concurrency/07-deadlocks.md)
- **DELETE**: [01-fundamentals/14-insert-update-delete.md](./01-fundamentals/14-insert-update-delete.md)
- **DENSE_RANK**: [02-querying/10-ranking-functions.md](./02-querying/10-ranking-functions.md)
- **DuckDB**: [08-dialects-and-engines/06-olap-engines-duckdb-clickhouse.md](./08-dialects-and-engines/06-olap-engines-duckdb-clickhouse.md)

### E
- **EXCEPT / MINUS**: [02-querying/03-set-operations.md](./02-querying/03-set-operations.md)
- **EXISTS**: [02-querying/05-exists-in-any-all.md](./02-querying/05-exists-in-any-all.md)
- **EXPLAIN ANALYZE**: [05-indexing-and-performance/07-explain-analyze-deep-dive.md](./05-indexing-and-performance/07-explain-analyze-deep-dive.md)
- **Expression Index**: [05-indexing-and-performance/05-expression-indexes.md](./05-indexing-and-performance/05-expression-indexes.md)

### F
- **FETCH FIRST**: [01-fundamentals/07-limit-offset.md](./01-fundamentals/07-limit-offset.md)
- **Foreign Keys**: [03-database-design/05-foreign-keys-and-referential-actions.md](./03-database-design/05-foreign-keys-and-referential-actions.md)
- **FULL OUTER JOIN**: [02-querying/01-joins.md](./02-querying/01-joins.md)
- **Full-Text Search**: [02-querying/19-full-text-search.md](./02-querying/19-full-text-search.md)

### G
- **GIN Index**: [05-indexing-and-performance/03-gin-gist-brin-indexes.md](./05-indexing-and-performance/03-gin-gist-brin-indexes.md)
- **GiST Index**: [05-indexing-and-performance/03-gin-gist-brin-indexes.md](./05-indexing-and-performance/03-gin-gist-brin-indexes.md)
- **GROUP BY**: [02-querying/06-aggregation-and-group-by.md](./02-querying/06-aggregation-and-group-by.md)
- **GROUPING SETS**: [02-querying/07-grouping-sets-rollup-cube.md](./02-querying/07-grouping-sets-rollup-cube.md)

### H
- **Hash Join**: [05-indexing-and-performance/09-join-algorithms.md](./05-indexing-and-performance/09-join-algorithms.md)
- **HAVING Clause**: [02-querying/06-aggregation-and-group-by.md](./02-querying/06-aggregation-and-group-by.md)
- **High Availability**: [07-administration-and-security/04-replication-and-high-availability.md](./07-administration-and-security/04-replication-and-high-availability.md)

### I
- **ILIKE**: [01-fundamentals/09-pattern-matching.md](./01-fundamentals/09-pattern-matching.md)
- **IN Operator**: [02-querying/05-exists-in-any-all.md](./02-querying/05-exists-in-any-all.md)
- **Index-Only Scan**: [05-indexing-and-performance/06-covering-indexes-and-index-only-scans.md](./05-indexing-and-performance/06-covering-indexes-and-index-only-scans.md)
- **INNER JOIN**: [02-querying/01-joins.md](./02-querying/01-joins.md)
- **INSERT**: [01-fundamentals/14-insert-update-delete.md](./01-fundamentals/14-insert-update-delete.md)
- **INTERSECT**: [02-querying/03-set-operations.md](./02-querying/03-set-operations.md)
- **Isolation Levels**: [04-transactions-and-concurrency/03-isolation-levels-and-anomalies.md](./04-transactions-and-concurrency/03-isolation-levels-and-anomalies.md)

### J
- **JSON / JSONB**: [02-querying/17-json-and-semi-structured.md](./02-querying/17-json-and-semi-structured.md)
- **Join Algorithms**: [05-indexing-and-performance/09-join-algorithms.md](./05-indexing-and-performance/09-join-algorithms.md)

### K
- **Keyset Pagination**: [10-real-world-patterns/01-keyset-pagination.md](./10-real-world-patterns/01-keyset-pagination.md)

### L
- **LAG / LEAD**: [02-querying/11-value-window-functions.md](./02-querying/11-value-window-functions.md)
- **LATERAL Join**: [02-querying/14-lateral-joins.md](./02-querying/14-lateral-joins.md)
- **LEFT JOIN**: [02-querying/01-joins.md](./02-querying/01-joins.md)
- **LIMIT / OFFSET**: [01-fundamentals/07-limit-offset.md](./01-fundamentals/07-limit-offset.md)
- **Locking (Row & Table)**: [04-transactions-and-concurrency/05-row-level-locking.md](./04-transactions-and-concurrency/05-row-level-locking.md), [04-transactions-and-concurrency/06-table-level-locks.md](./04-transactions-and-concurrency/06-table-level-locks.md)

### M
- **Materialized Views**: [06-programmability/02-materialized-views.md](./06-programmability/02-materialized-views.md)
- **Merge Join**: [05-indexing-and-performance/09-join-algorithms.md](./05-indexing-and-performance/09-join-algorithms.md)
- **MERGE Statement**: [01-fundamentals/15-upsert-and-merge.md](./01-fundamentals/15-upsert-and-merge.md)
- **Multi-Tenancy**: [03-database-design/10-multi-tenancy-schemas.md](./03-database-design/10-multi-tenancy-schemas.md)
- **MVCC**: [04-transactions-and-concurrency/04-mvcc-internals.md](./04-transactions-and-concurrency/04-mvcc-internals.md)

### N
- **Nested Loop Join**: [05-indexing-and-performance/09-join-algorithms.md](./05-indexing-and-performance/09-join-algorithms.md)
- **Normalization**: [03-database-design/02-normalization-1nf-to-bcnf.md](./03-database-design/02-normalization-1nf-to-bcnf.md)
- **NOT NULL**: [01-fundamentals/03-constraints.md](./01-fundamentals/03-constraints.md)
- **NULL Semantics**: [01-fundamentals/08-null-semantics.md](./01-fundamentals/08-null-semantics.md)

### O
- **ON CONFLICT DO UPDATE**: [01-fundamentals/15-upsert-and-merge.md](./01-fundamentals/15-upsert-and-merge.md)
- **Optimistic Concurrency**: [04-transactions-and-concurrency/08-optimistic-vs-pessimistic-locking.md](./04-transactions-and-concurrency/08-optimistic-vs-pessimistic-locking.md)
- **ORDER BY**: [01-fundamentals/06-order-by.md](./01-fundamentals/06-order-by.md)
- **OVER Clause**: [02-querying/08-window-functions-basics.md](./02-querying/08-window-functions-basics.md)

### P
- **Partial Index**: [05-indexing-and-performance/04-partial-indexes.md](./05-indexing-and-performance/04-partial-indexes.md)
- **Partitioning**: [05-indexing-and-performance/12-table-partitioning.md](./05-indexing-and-performance/12-table-partitioning.md)
- **Pessimistic Concurrency**: [04-transactions-and-concurrency/08-optimistic-vs-pessimistic-locking.md](./04-transactions-and-concurrency/08-optimistic-vs-pessimistic-locking.md)
- **pgvector**: [06-programmability/09-extensions-pgvector-postgis.md](./06-programmability/09-extensions-pgvector-postgis.md)
- **PostGIS**: [06-programmability/09-extensions-pgvector-postgis.md](./06-programmability/09-extensions-pgvector-postgis.md)
- **PRIMARY KEY**: [01-fundamentals/03-constraints.md](./01-fundamentals/03-constraints.md), [03-database-design/04-primary-key-strategies.md](./03-database-design/04-primary-key-strategies.md)

### Q
- **Query Planner**: [05-indexing-and-performance/08-query-planner-and-statistics.md](./05-indexing-and-performance/08-query-planner-and-statistics.md)
- **Queue Tables (SKIP LOCKED)**: [10-real-world-patterns/04-job-queue-with-skip-locked.md](./10-real-world-patterns/04-job-queue-with-skip-locked.md)

### R
- **RANK / DENSE_RANK**: [02-querying/10-ranking-functions.md](./02-querying/10-ranking-functions.md)
- **Recursive CTE**: [02-querying/13-recursive-ctes.md](./02-querying/13-recursive-ctes.md)
- **Relational Model**: [00-setup/01-what-is-sql-and-the-relational-model.md](./00-setup/01-what-is-sql-and-the-relational-model.md)
- **Replication**: [07-administration-and-security/04-replication-and-high-availability.md](./07-administration-and-security/04-replication-and-high-availability.md)
- **RETURNING Clause**: [01-fundamentals/14-insert-update-delete.md](./01-fundamentals/14-insert-update-delete.md)
- **ROLLUP**: [02-querying/07-grouping-sets-rollup-cube.md](./02-querying/07-grouping-sets-rollup-cube.md)
- **Row-Level Security (RLS)**: [07-administration-and-security/02-row-level-security.md](./07-administration-and-security/02-row-level-security.md)
- **ROW_NUMBER**: [02-querying/10-ranking-functions.md](./02-querying/10-ranking-functions.md)

### S
- **Sargability**: [05-indexing-and-performance/11-sargability-and-query-antipatterns.md](./05-indexing-and-performance/11-sargability-and-query-antipatterns.md)
- **Savepoints**: [04-transactions-and-concurrency/02-transaction-control-and-savepoints.md](./04-transactions-and-concurrency/02-transaction-control-and-savepoints.md)
- **SELECT**: [01-fundamentals/04-select-statement.md](./01-fundamentals/04-select-statement.md)
- **Semi-Joins**: [02-querying/15-semi-joins-and-anti-joins.md](./02-querying/15-semi-joins-and-anti-joins.md)
- **Set Operations**: [02-querying/03-set-operations.md](./02-querying/03-set-operations.md)
- **Soft Delete**: [03-database-design/09-soft-delete-patterns.md](./03-database-design/09-soft-delete-patterns.md)
- **SQL Injection Defense**: [07-administration-and-security/07-sql-injection-prevention.md](./07-administration-and-security/07-sql-injection-prevention.md)
- **Stored Procedures**: [06-programmability/03-stored-procedures-and-functions.md](./06-programmability/03-stored-procedures-and-functions.md)

### T
- **Table Partitioning**: [05-indexing-and-performance/12-table-partitioning.md](./05-indexing-and-performance/12-table-partitioning.md)
- **Three-Valued Logic (3VL)**: [01-fundamentals/08-null-semantics.md](./01-fundamentals/08-null-semantics.md)
- **Triggers**: [06-programmability/04-triggers.md](./06-programmability/04-triggers.md)
- **TRUNCATE**: [01-fundamentals/02-create-alter-drop-table.md](./01-fundamentals/02-create-alter-drop-table.md)

### U
- **UNION / UNION ALL**: [02-querying/03-set-operations.md](./02-querying/03-set-operations.md)
- **UNIQUE Constraint**: [01-fundamentals/03-constraints.md](./01-fundamentals/03-constraints.md)
- **UPDATE**: [01-fundamentals/14-insert-update-delete.md](./01-fundamentals/14-insert-update-delete.md)
- **UPSERT**: [01-fundamentals/15-upsert-and-merge.md](./01-fundamentals/15-upsert-and-merge.md)
- **UUIDv7**: [03-database-design/04-primary-key-strategies.md](./03-database-design/04-primary-key-strategies.md)

### V
- **Vacuuming**: [04-transactions-and-concurrency/11-vacuum-freeze-and-bloat.md](./04-transactions-and-concurrency/11-vacuum-freeze-and-bloat.md)
- **Views**: [06-programmability/01-views.md](./06-programmability/01-views.md)

### W
- **WHERE Clause**: [01-fundamentals/05-where-filtering.md](./01-fundamentals/05-where-filtering.md)
- **Window Framing**: [02-querying/09-window-framing.md](./02-querying/09-window-framing.md)
- **Window Functions**: [02-querying/08-window-functions-basics.md](./02-querying/08-window-functions-basics.md)
- **Write-Ahead Logging (WAL)**: [04-transactions-and-concurrency/10-wal-and-durability.md](./04-transactions-and-concurrency/10-wal-and-durability.md)
