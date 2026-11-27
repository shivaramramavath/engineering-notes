# SQL & Relational Database Engineering Glossary (GLOSSARY.md)

A precise, reference-grade glossary of foundational and advanced terms across relational database engineering, ANSI SQL:2023, query optimization, storage engines, and concurrency control.

---

### 1. Concurrency, Transactions & Storage
- **ACID**: A set of four fundamental properties guaranteed by database transactions:
  - **Atomicity**: All operations within a transaction succeed completely or are completely aborted (all-or-nothing).
  - **Consistency**: A transaction transitions the database from one valid state to another, satisfying all defined schema constraints, cascades, and triggers.
  - **Isolation**: Concurrent execution of transactions yields the same database state as if they were executed serially.
  - **Durability**: Once committed, transaction mutations persist even in the event of an immediate system crash, power outage, or OS panic (guaranteed via WAL).
- **Advisory Lock**: An application-level lock explicitly acquired and released via database functions (e.g., `pg_advisory_lock`), having no binding to any physical table row.
- **Auto-Commit**: A database client/driver mode where every individual SQL statement is automatically wrapped in its own `BEGIN ... COMMIT` block.
- **Checkpoint**: A periodic storage engine flush process where all modified in-memory buffer pages (dirty pages) are written to persistent disk storage, allowing WAL segments prior to the checkpoint to be recycled.
- **Deadlock**: A circular wait state between two or more concurrent transactions where each transaction holds a lock that another requires, preventing all of them from proceeding until the engine terminates one transaction.
- **Dirty Read**: An isolation anomaly where transaction A reads uncommitted data written by concurrent transaction B. If transaction B subsequently rolls back, transaction A operated on phantom, non-existent state.
- **MVCC (Multi-Version Concurrency Control)**: A concurrency control method where readers do not lock writers, and writers do not lock readers. Each update or delete creates a new immutable version (tuple) of the row with creation and expiration transaction identifiers (`xmin`/`xmax` in PostgreSQL).
- **Non-Repeatable Read (Fuzzy Read)**: An anomaly where transaction A reads a row, transaction B updates or deletes that row and commits, and transaction A re-reads the row to find the values changed or missing.
- **Optimistic Concurrency Control (OCC)**: A concurrency management design where transactions execute without acquiring locks, validating before commit (typically via a `version` or `updated_at` column) that no concurrent modification has taken place.
- **Pessimistic Concurrency Control (PCC)**: A concurrency management design where transactions acquire explicit locks on accessed records (e.g., `SELECT ... FOR UPDATE`) before performing modifications, blocking conflicting access.
- **Phantom Read**: An anomaly where transaction A executes a query returning a set of rows satisfying a condition, transaction B inserts or deletes rows matching that condition and commits, and transaction A re-executes the query to find different rows.
- **Point-In-Time Recovery (PITR)**: The ability to restore a database to the exact millisecond before a critical fault by rolling forward a base physical backup using continuous archived Write-Ahead Logs (WAL).
- **Savepoint**: A marker within an active transaction that allows partial rollback of subsequent statements without aborting the entire enclosing transaction (`SAVEPOINT sp1; ROLLBACK TO sp1;`).
- **Serialization Anomaly (Write Skew)**: An anomaly under Snapshot Isolation where two concurrent transactions read overlapping state, make disjoint writes that together violate an integrity rule, and commit successfully because neither updated the same physical row.
- **Two-Phase Commit (2PC)**: A distributed consensus protocol guaranteeing atomic transactions across multiple autonomous database nodes via a Prepare phase followed by a Commit phase.
- **Vacuuming / Garbage Collection**: The background process in MVCC engines that scans table pages to identify and reclaim dead tuples (versions no longer visible to any active transaction) and freeze old transaction IDs.
- **WAL (Write-Ahead Logging)**: An append-only log protocol ensuring durability. Before any modified data page is flushed to table files on disk, the mutation must be securely recorded in the sequential WAL on disk.

---

### 2. Querying, Relational Algebra & Syntax
- **3VL (Three-Valued Logic)**: The logical system employed by SQL where boolean expressions evaluate to `TRUE`, `FALSE`, or `UNKNOWN` (NULL). Crucially, `UNKNOWN AND FALSE` is `FALSE`, but `UNKNOWN AND TRUE` is `UNKNOWN`.
- **Anti-Join**: A relational operation returning rows from the left relation that have NO matching rows in the right relation (commonly implemented via `NOT EXISTS` or `LEFT JOIN ... WHERE right.id IS NULL`).
- **Cartesian Product**: The set of all possible ordered pairs $(a, b)$ formed by combining every row from table A with every row from table B (produced by `CROSS JOIN` or comma joins without filter conditions).
- **Coalesce**: A scalar function returning the first non-NULL expression from an argument list (`COALESCE(val1, val2, default)`).
- **Correlated Subquery**: A subquery that references columns from the outer query. Conceptually evaluated once per candidate row of the outer query.
- **CTE (Common Table Expression)**: A named temporary result set defined within the scope of a single SQL statement using the `WITH` clause.
- **DENSE_RANK()**: A window ranking function that assigns contiguous rankings to ordered rows without leaving gaps when duplicate values occur (e.g., 1, 2, 2, 3).
- **Full Outer Join**: A relational operation returning all matched rows from both tables, plus unmatched rows from the left table with NULLs, plus unmatched rows from the right table with NULLs.
- **HAVING Clause**: A filtering clause evaluated strictly *after* the `GROUP BY` aggregation step, operating on aggregated expressions (`HAVING COUNT(*) > 5`).
- **Idempotency**: The mathematical property that applying an operation multiple times produces the identical result as applying it once. In SQL, upserts and migration scripts strive for idempotency.
- **Inner Join**: A relational operation returning only rows where the join predicate evaluates to `TRUE` between the participating relations.
- **Keyset Pagination (Seek Method)**: A pagination pattern that fetches rows using an indexed column comparison (`WHERE (created_at, id) < ($last_time, $last_id) ORDER BY created_at DESC, id DESC LIMIT 20`), avoiding the performance cost of high `OFFSET` values.
- **Lateral Join**: A join type (`CROSS JOIN LATERAL` in Postgres) that permits a subquery or set-returning function on the right-hand side to reference columns from preceding tables on the left-hand side.
- **Left Outer Join**: A relational operation returning all rows from the left table, matched with corresponding rows from the right table where the predicate is true, or padded with NULLs where no match exists.
- **Nullif**: A scalar function returning `NULL` if two arguments evaluate to equal values, otherwise returning the first argument (`NULLIF(val, 0)` to prevent division by zero).
- **Recursive CTE**: A CTE that references its own name to iteratively evaluate hierarchical structures, graph paths, or synthetic sequences until an anchor condition terminates.
- **Semi-Join**: A relational operation returning rows from the left table that have at least one match in the right table, without duplicating rows or exposing right-hand columns (e.g., `EXISTS` or `IN`).
- **Set Operations**: Mathematical operations combining the results of two queries with identical column counts and compatible types: `UNION` (distinct), `UNION ALL` (with duplicates), `INTERSECT`, and `EXCEPT` / `MINUS`.
- **Window Function**: A function that performs calculations across a set of table rows related to the current row without collapsing the rows into a single summary output row (using the `OVER (...)` clause).
- **Window Frame**: The dynamic boundary of rows evaluated by a window function relative to the current row, specified via `ROWS`, `RANGE`, or `GROUPS` between `UNBOUNDED PRECEDING`, `CURRENT ROW`, and `UNBOUNDED FOLLOWING`.

---

### 3. Database Design, Integrity & Normalization
- **1NF (First Normal Form)**: A relation where all attributes contain only atomic (indivisible) values, and there are no repeating groups or arrays.
- **2NF (Second Normal Form)**: A relation in 1NF where every non-key attribute is fully functionally dependent on the entire primary key (no partial key dependencies).
- **3NF (Third Normal Form)**: A relation in 2NF where no non-key attribute is transitively dependent on the primary key (no non-key attribute depends on another non-key attribute).
- **BCNF (Boyce-Codd Normal Form)**: A stricter variant of 3NF where for every non-trivial functional dependency $X \rightarrow Y$, $X$ must be a superkey.
- **Candidate Key**: A minimal set of columns that uniquely identifies a row in a relation without extraneous attributes.
- **Composite Key**: A primary or candidate key consisting of two or more columns.
- **Denormalization**: The deliberate introduction of redundancy into a normalized schema to optimize read query performance at the cost of write complexity and storage.
- **Foreign Key**: A column or combination of columns in one table whose values are constrained to match a candidate key in another table, enforcing referential integrity.
- **Natural Key**: A primary key formed from existing domain attributes inherent to the entity (e.g., ISO country code, tax identifier).
- **Referential Action**: The behavior triggered on a foreign key when the referenced parent row is updated or deleted: `CASCADE`, `SET NULL`, `SET DEFAULT`, `RESTRICT`, or `NO ACTION`.
- **Soft Delete**: A design pattern where deleted records are not physically removed via `DELETE`, but marked with a boolean flag (`is_deleted`) or timestamp (`deleted_at`).
- **Surrogate Key**: An artificial, meaningless identifier generated solely to act as a unique primary key (e.g., auto-incrementing integer, sequence, UUID).
- **UUIDv7**: A 128-bit time-ordered Universally Unique Identifier encoding a 48-bit Unix epoch millisecond timestamp, providing global uniqueness without degrading B-Tree index locality.

---

### 4. Indexing, Performance & Optimization
- **B-Tree (Balanced Tree)**: The default self-balancing tree data structure utilized by relational databases for indexing, providing $O(\log N)$ searches, sequential range scans, and sorted traversals.
- **Bloat**: The accumulation of dead, unreclaimed tuple space or empty B-Tree index pages caused by frequent `UPDATE` or `DELETE` operations without adequate vacuuming.
- **BRIN (Block Range Index)**: A lightweight index structure in PostgreSQL that stores the minimum and maximum values for physical ranges of table disk blocks (ideal for naturally ordered time-series data).
- **Covering Index**: An index that contains all columns required to satisfy a query (either in the index key or via an `INCLUDE` clause), allowing the engine to execute an Index-Only Scan.
- **EXPLAIN**: A statement that outputs the query execution plan generated by the optimizer, showing expected plan nodes, costs, and row estimates.
- **EXPLAIN ANALYZE**: An execution statement that actually executes the query and returns real runtime performance statistics (actual execution time, loops, rows returned, memory, and buffer hits) alongside planner estimates.
- **GIN (Generalized Inverted Index)**: An index structure that maps composite elements (such as tokens in a full-text search string, elements of an array, or keys/values in a JSONB document) to row locations.
- **GiST (Generalized Search Tree)**: A versatile, extensible tree index framework used for non-scalar data types such as geometric coordinates, IP network ranges, and spatial bounding boxes.
- **Hash Join**: A physical join algorithm where the engine scans the smaller (inner) relation, builds an in-memory hash table on the join key, and then streams the larger (outer) relation, probing the hash table for matches.
- **Index-Only Scan**: A query execution method where the engine retrieves all necessary data directly from the index structure without fetching the corresponding table heap pages.
- **Merge Join**: A physical join algorithm that requires both input relations to be sorted on the join keys, sequentially traversing both sorted streams simultaneously to produce matches.
- **Nested Loop Join**: A physical join algorithm that iterates over each row of an outer relation and scans an inner relation (frequently using an index) for matching rows.
- **Partitioning**: The architectural splitting of a large relational table into smaller physical backing tables (by Range, List, or Hash) while presenting a single unified table interface to queries.
- **Sargability (Search Argument Able)**: A property of a query predicate that allows the optimizer to utilize an index for range scans or equality lookups without having to evaluate expressions or functions on indexed columns.
- **Sequential Scan (Table Scan)**: A query execution method where the engine reads every page and row of a table from disk sequentially.
