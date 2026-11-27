# SQL & Relational Database Engineering Track

Welcome to the comprehensive, production-grade **SQL & Relational Database Engineering** curriculum. This curriculum spans modern relational database architecture, ANSI SQL:2023 standards, practical database design, query optimization, transactional concurrency control, performance profiling, and enterprise data modeling.

---

## 🎯 Curriculum Philosophy & Approach

1. **PostgreSQL 17+ Primary Baseline**: All queries, architectures, and engine internals are standardized against modern PostgreSQL (v17+) while maintaining rigorous ANSI SQL:2023 compatibility.
2. **First-Class Multi-Dialect Comparison**: Every concept note includes dedicated comparison analysis across **PostgreSQL 17+**, **MySQL 8.4 LTS**, **SQLite 3.50+**, and **Microsoft SQL Server 2022**.
3. **Internals-First Pedagogy**: Rather than merely memorizing syntax, you will learn relational algebra, query parsing, cost-based planning, join physical algorithms (Nested Loop, Hash, Merge Join), write-ahead logging (WAL), MVCC tuple visibility, and B-Tree disk layout.
4. **Production Rigor & Anti-Patterns**: Every topic explores edge cases, 3-valued boolean logic pitfalls, NULL semantics, transaction isolation anomalies, index pollution, sargability, and defense against SQL injection.
5. **Deterministic Verification**: Every query example and practice problem is deterministically executable and produces real, reproducible output.

---

## 📚 Master Syllabus (15 Canonical Modules)

| # | Module Directory | Focus & Topic Coverage | Notes / Spec Count |
|:---:|:---|:---|:---:|
| **00** | [00-setup](./00-setup/README.md) | **Setup, Engines & Query Lifecycle**: Relational theory, engine landscape, client tooling (psql, CLI, GUI), sample databases, SQL:2023 standard, and query lifecycle (parse -> rewrite -> plan -> execute). | 7 notes + README |
| **01** | [01-fundamentals](./01-fundamentals/README.md) | **SQL Fundamentals & Core DDL/DML**: Strict data types, table DDL, constraints, SELECT, filtering, sorting, 3VL logic, scalar functions, CASE expressions, CRUD, UPSERT/MERGE, and RETURNING. | 16 notes + README |
| **02** | [02-querying](./02-querying/README.md) | **Advanced Querying & Analytical SQL**: INNER/LEFT/RIGHT/FULL/CROSS/NATURAL joins, set ops, subqueries, aggregation, window functions, recursive CTEs, LATERAL joins, JSON/JSONB, and pagination. | 20 notes + README |
| **03** | [03-database-design](./03-database-design/README.md) | **Relational Modeling & Schema Architecture**: Entity-Relationship design, 1NF to BCNF normalization, key strategies (surrogate, natural, UUIDv7), schema evolution, audit logging, soft-deletes, and migrations. | 12 notes + README |
| **04** | [04-transactions-and-concurrency](./04-transactions-and-concurrency/README.md) | **ACID, Concurrency & Locking**: ACID guarantees, savepoints, standard vs engine isolation levels, MVCC tuple visibility, row/table/advisory locks, deadlocks, SELECT FOR UPDATE, and vacuuming. | 11 notes + README |
| **05** | [05-indexing-and-performance](./05-indexing-and-performance/README.md) | **Indexing, Optimization & Profiling**: B-Tree, GIN, GiST, BRIN, EXPLAIN ANALYZE reading, cost-based planner statistics, join algorithms, sargability, table partitioning, bloat remediation, and connection pooling. | 14 notes + README |
| **06** | [06-programmability](./06-programmability/README.md) | **Views, Routines, Triggers & Extensions**: Standard and materialized views, stored procedures and functions, row/statement triggers, user-defined types (UDTs), cursors, dynamic SQL, and extensions (pgvector, PostGIS). | 10 notes + README |
| **07** | [07-administration-and-security](./07-administration-and-security/README.md) | **Administration, Security & Governance**: Role-Based Access Control (RBAC), Row-Level Security (RLS), backup and PITR restore, WAL replication and HA, slow query auditing, COPY data pipes, and SQL injection defense. | 11 notes + README |
| **08** | [08-dialects-and-engines](./08-dialects-and-engines/README.md) | **Dialect Deep Dives & Specialty Engines**: Dialect quirks (Postgres, MySQL, SQLite, T-SQL), query translation, OLAP engines (DuckDB, ClickHouse), Star/Snowflake modeling, and relational vs NoSQL trade-offs. | 11 notes + README |
| **09** | [09-sql-from-applications](./09-sql-from-applications/README.md) | **Application Integration & ORMs**: Parameterized queries, connection pool sizing, migration frameworks (Flyway, Liquibase, Prisma, Alembic), integration testing (Testcontainers), and N+1 prevention. | 8 notes + README |
| **10** | [10-real-world-patterns](./10-real-world-patterns/README.md) | **Production Enterprise Query Patterns**: Keyset pagination, tenant isolation, soft-delete filtering, queue processing (`FOR UPDATE SKIP LOCKED`), rate limiters, double-entry financial ledgers, and GDPR compliance. | 13 notes + README |
| **11** | [11-projects](./11-projects/README.md) | **Production Reference Projects**: 11 end-to-end relational systems (E-commerce, Banking Ledger, Social Graph, Multi-Tenant SaaS, Warehouse, etc.) featuring complete schemas, queries, and tuning specs. | 11 projects (44 files) |
| **12** | [12-practice-labs](./12-practice-labs/README.md) | **Graded Practice Problem Sets**: 150+ progressively challenging query exercises spanning basic filtering to multi-stage recursive CTEs and analytical window transforms. | 150+ exercises |
| **13** | [13-interview](./13-interview/README.md) | **Technical Interview Preparation**: 10 comprehensive interview preparation guides covering fundamentals, complex joins, window functions, schema design, concurrency, indexing, and live coding. | 10 guide modules |
| **14** | [14-cheatsheets](./14-cheatsheets/README.md) | **High-Density Operational Cheatsheets**: 11 quick-reference cheat sheets for SELECT clauses, join diagrams, window functions, DDL constraints, EXPLAIN interpretation, CLI commands, and dialect tables. | 11 reference sheets |

---

## 🗄️ Dialect Matrix & Versions

All materials in this track are benchmarked against specific, verified engine releases:

| Dialect / Engine | Tested Baseline | Execution Role in Track | Architecture Highlights |
|:---|:---|:---|:---|
| **PostgreSQL** | `17.4+` | Primary Reference Engine | Advanced MVCC, Rich GIN/BRIN/GiST indexing, native JSONB, CTE optimization, RLS, pluggable table access methods. |
| **MySQL** | `8.4 LTS` | Secondary RDBMS Baseline | InnoDB storage engine, Clustered Primary Keys, Undo/Redo logging, Buffer Pool tuning, Performance Schema. |
| **SQLite** | `3.50+` | Fast Local Deterministic Runner | Single-file zero-config, WAL mode, dynamic manifest typing, in-memory execution, embedded serverless. |
| **SQL Server** | `2022` | Enterprise T-SQL Comparison | Transact-SQL, Clustered Indexes, Columnstore, Query Store, Temporal Tables, cross-platform Linux support. |

---

## 🧭 Study Paths

Choose your path through the material based on your career focus:

1. **Backend Application Engineer**: Focus on Modules `00` -> `01` -> `02` -> `03` -> `04` -> `05` -> `09` -> `10` -> `11`.
2. **Data Analyst / Analytics Engineer**: Focus on Modules `00` -> `01` -> `02` -> `06` -> `08` -> `12` -> `14`.
3. **Database Administrator (DBA) / Performance Engineer**: Focus on Modules `00` -> `03` -> `04` -> `05` -> `07` -> `08` -> `10`.
4. **Interview Preparation Sprint**: Focus on Modules `01` -> `02` -> `04` -> `05` -> `13` -> `14`.

See [ROADMAP.md](./ROADMAP.md) for full syllabus breakdowns, timelines, and study schedules.

---

## 📖 Navigation & Reference Documents

- [INDEX.md](./INDEX.md) - Complete alphabetical and topical index of all concepts, commands, and functions.
- [ROADMAP.md](./ROADMAP.md) - Detailed learning trajectories, time estimates, and milestones.
- [GLOSSARY.md](./GLOSSARY.md) - 60+ formal definitions of database terms (ACID, MVCC, 3VL, B-Tree, Sargability, etc.).
- [CONTRIBUTING.md](./CONTRIBUTING.md) - Guidelines for authoring notes, verification rules, and dialect format requirements.
- [SQL_FACTS.md](./SQL_FACTS.md) - Authoritative database facts, syntax differences, and baseline references.
- [SQL_CONVENTIONS.md](./SQL_CONVENTIONS.md) - Strict 13-section formatting rules and code quality standards.
- [SQL_PROGRESS.md](./SQL_PROGRESS.md) - Live tracking of completed modules, notes, and milestones.
