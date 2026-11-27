# SQL Learning Roadmap (ROADMAP.md)

This roadmap outlines five tailored learning trajectories through the 15 modules of the SQL track. Choose the trajectory that aligns with your engineering role, timeline, and professional goals.

---

## 🗺️ Trajectory Matrix

| Path | Target Audience | Estimated Hours | Core Modules | Capstone Milestone |
|:---|:---|:---:|:---|:---|
| **1. Complete RDBMS Engineer** | Full-stack & systems engineers seeking mastery | 120–160 hrs | All Modules `00` through `14` | Build all 11 Projects |
| **2. Backend Developer** | Microservices, API & backend application engineers | 60–80 hrs | `00`, `01`, `02`, `03`, `04`, `05`, `09`, `10`, `11` | Projects 02, 03, 08 |
| **3. Data & Analytics Engineer** | Data analysts, analytics engineers, BI developers | 40–55 hrs | `00`, `01`, `02`, `06`, `08`, `11` (OLAP), `12`, `14` | Projects 06, 09 |
| **4. DBA & Performance Engineer**| Infrastructure, SRE, and performance specialists | 60–75 hrs | `00`, `03`, `04`, `05`, `07`, `08`, `10`, `11` | Project 10 (Tuning Lab) |
| **5. Interview Preparation Sprint**| Engineers preparing for technical interviews | 25–35 hrs | `01`, `02`, `04`, `05`, `10`, `13`, `14` | Complete Module 13 |

---

## 🧭 Detailed Learning Paths

### 1. The Complete RDBMS Engineer (Full Mastery)
*Goal: Deep foundational understanding from relational calculus and parser pipelines to storage engine internals and enterprise distributed patterns.*

- **Phase 1: Foundations & Core Execution (Weeks 1–2)**
  - Module `00-setup`: Relational model, engine architectures, parse/plan/exec lifecycle.
  - Module `01-fundamentals`: 3VL logic, strict typing, DDL, constraints, filtering, CASE, upserts.
  - Labs: Complete Module 12 Beginner Query Sets.
- **Phase 2: Analytical SQL & Schema Architecture (Weeks 3–5)**
  - Module `02-querying`: Joins, window functions, framing, recursive CTEs, LATERAL joins, JSONB.
  - Module `03-database-design`: Normalization (1NF–BCNF), surrogate vs UUIDv7, audit trails, migrations.
  - Projects: Project 01 (Library System) & Project 02 (E-Commerce Storefront).
- **Phase 3: Engine Internals, Concurrency & Performance (Weeks 6–9)**
  - Module `04-transactions-and-concurrency`: MVCC tuple visibility, ACID, deadlocks, locks, WAL.
  - Module `05-indexing-and-performance`: B-Tree layout, EXPLAIN ANALYZE, join algorithms, partitioning.
  - Projects: Project 03 (Banking Ledger) & Project 10 (Query Optimization Lab).
- **Phase 4: Advanced Systems, Programmability & Operations (Weeks 10–12)**
  - Module `06-programmability`: Triggers, procedures, materialized views, pgvector.
  - Module `07-administration-and-security`: RBAC, RLS, PITR backups, HA replication, SQL injection.
  - Module `08-dialects-and-engines`: PostgreSQL vs MySQL vs SQLite vs SQL Server, DuckDB/ClickHouse.
  - Module `09-sql-from-applications`: Connection pooling, ORM boundary design, Testcontainers.
  - Module `10-real-world-patterns`: Keyset pagination, job queues (`SKIP LOCKED`), double-entry ledgers.
- **Phase 5: Capstone Synthesis & Interview Readiness (Weeks 13–14)**
  - Module `11-projects`: Projects 07 (Ride-Sharing Dispatch) & 08 (Multi-Tenant SaaS Platform).
  - Module `13-interview`: Work through all 10 deep technical interview domains.
  - Module `14-cheatsheets`: Retain operational cheatsheets for daily engineering reference.

---

### 2. Backend Application Engineer Path
*Focus: Schema design, transactional data integrity, ORM hygiene, and high-performance querying.*

1. **Step 1: The Basics (10 hrs)**
   - `00-setup`: Client tools and query execution fundamentals.
   - `01-fundamentals`: Strict constraints, 3VL logic pitfalls, upsert (`ON CONFLICT DO UPDATE`), and `RETURNING`.
2. **Step 2: Practical Querying (15 hrs)**
   - `02-querying`: Mastering INNER/LEFT joins, window functions (`ROW_NUMBER`, `DENSE_RANK`), and CTEs.
3. **Step 3: Relational Modeling & Schema Migrations (12 hrs)**
   - `03-database-design`: Normalization, UUIDv7 vs BIGINT keys, foreign key cascading, and zero-downtime migration scripts.
4. **Step 4: Concurrency & Transaction Safety (15 hrs)**
   - `04-transactions-and-concurrency`: Preventing race conditions, `SELECT ... FOR UPDATE`, lost updates, and isolation level tradeoffs.
5. **Step 5: Indexing & Query Tuning (12 hrs)**
   - `05-indexing-and-performance`: B-Tree multi-column index ordering, sargability, reading `EXPLAIN ANALYZE`.
6. **Step 6: Application Boundaries & Real-World Patterns (14 hrs)**
   - `09-sql-from-applications`: Connection pool tuning, avoiding N+1 queries, writing integration tests with Testcontainers.
   - `10-real-world-patterns`: Keyset pagination, background worker queues (`FOR UPDATE SKIP LOCKED`), and double-entry accounting.
   - Capstone: Build Project 02 (E-Commerce) and Project 08 (Multi-Tenant SaaS).

---

### 3. Data & Analytics Engineer Path
*Focus: Analytical transformations, dimensional modeling, window functions, and OLAP processing.*

1. **Step 1: Advanced SQL Transformations (15 hrs)**
   - `01-fundamentals`: Date/time math, string functions, and CASE expressions.
   - `02-querying`: Full JOIN coverage, window framing (`ROWS BETWEEN`), ranking, multi-level aggregation (`GROUPING SETS`, `ROLLUP`, `CUBE`), and recursive CTEs.
2. **Step 2: Dimensional Modeling & Warehousing (15 hrs)**
   - `03-database-design`: Star and Snowflake schemas, Fact vs Dimension tables, slowly changing dimensions (SCD Types 1, 2, 3).
   - `08-dialects-and-engines`: Columnar engines (DuckDB, ClickHouse) vs row-oriented OLTP.
3. **Step 3: Analytical Materialization (10 hrs)**
   - `06-programmability`: Materialized views, concurrent refresh patterns, and analytical triggers.
4. **Step 4: Hands-on Analytical Projects (15 hrs)**
   - Project 06: HR & Payroll Analytics Engine.
   - Project 09: Star Schema Data Warehouse.
   - Module 12: Solve all 50 Advanced Practice Labs.

---

### 4. Database Administrator (DBA) & Performance Tuning Path
*Focus: Engine internals, locking, storage layout, query plans, backup/recovery, and high availability.*

1. **Step 1: Engine Architecture & Storage Layout (12 hrs)**
   - `00-setup`: Query planner, parser, rewriter, and executor phases.
   - `04-transactions-and-concurrency`: Write-Ahead Log (WAL), checkpoints, dirty pages, MVCC tuple header flags, vacuuming, and transaction ID wraparound prevention.
2. **Step 2: Deep Indexing & Optimizer Mechanics (20 hrs)**
   - `05-indexing-and-performance`: B-Tree page fill factors, GiST, GIN, BRIN, planner statistics (histograms, MCVs), join physical strategies (Hash Join, Merge Join, Nested Loop), index bloat, and `REINDEX CONCURRENTLY`.
3. **Step 3: Administration, Security & High Availability (18 hrs)**
   - `07-administration-and-security`: Physical vs logical backups, Point-In-Time Recovery (PITR), streaming replication, synchronous vs asynchronous commit, failover strategies, Row-Level Security, and audit logging.
4. **Step 4: Real-World Troubleshooting Lab (15 hrs)**
   - Project 10: High-Volume Database Performance & Tuning Lab (diagnosing locks, missing indexes, and buffer cache misses).

---

### 5. Technical Interview Sprint Path
*Focus: Rapid mastery of high-frequency interview questions, query puzzles, and system design scenarios.*

- **Day 1–2: Fundamentals & Tricky Semantics (4 hrs)**
  - Review `01-fundamentals/08-null-semantics.md` (3VL logic, `NOT IN (NULL)` trap).
  - Review `01-fundamentals/13-case-and-conditionals.md` (conditional aggregation).
- **Day 3–4: Join Mechanics & Pitfalls (5 hrs)**
  - Review `02-querying/01-joins.md` and `02-querying/15-semi-joins-and-anti-joins.md`.
  - Practice query problems: Finding missing rows (`LEFT JOIN ... WHERE IS NULL` vs `NOT EXISTS`).
- **Day 5–7: Window Functions Mastery (6 hrs)**
  - Review `02-querying/08-window-functions-basics.md` through `11-value-window-functions.md`.
  - Top interview patterns: Finding the Nth highest salary, running totals, consecutive logins, gap-and-island problems.
- **Day 8–9: Transactions, Concurrency & Indexing (6 hrs)**
  - Review `04-transactions-and-concurrency/03-isolation-levels-and-anomalies.md` (Dirty read, non-repeatable read, phantom read, serialization anomaly).
  - Review `05-indexing-and-performance/01-b-tree-indexes.md` & `05-indexing-and-performance/11-sargability-and-query-antipatterns.md`.
- **Day 10–12: Comprehensive Interview Q&A (8 hrs)**
  - Work through `13-interview/` notes 01 through 10.
  - Review `14-cheatsheets/` for fast visual recall of syntax, execution order, and dialect quirks.
