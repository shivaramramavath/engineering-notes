# SQL Curriculum Track Changelog (SQL_CHANGELOG.md)

All notable changes, restructuring, file movements, and additions to the SQL curriculum track are documented in this file.

---

## [Unreleased] - 2026-11-26

### Architectural Governance & Baselines
- Created `SQL_FACTS.md`: Verified baselines for ISO/IEC 9075:2023 (SQL:2023), PostgreSQL 17/18, MySQL 8.4 LTS, and SQLite 3.50.4.
- Created `SQL_CONVENTIONS.md`: Standards for metadata tables, 4-way dialect difference boxes, 13-section concept note template, and Markdown-only constraints.
- Created `SQL_AUDIT.md`: Formal audit of legacy 203 files (195 placeholder draft stubs identified for consolidation).
- Created `SQL_DECISIONS.md`: Architecture Decision Records (ADRs 001–006).
- Created `SQL_COMPLETION_PLAN.md`: Full 15-module curriculum roadmap.
- Created `SQL_PROGRESS.md`: Real-time tracking checklist for all 15 modules and root files.
- Created `_templates/concept-template.md`: 13-section concept note template.
- Created `_templates/project-template.md`: 4-file reference project template.
- Created `<artifactDir>/scratch/sql_runner.py`: Deterministic verification toolchain using Python 3.14 stdlib SQLite 3.50.4.

### Curriculum Restructuring & Consolidation
- Consolidating legacy 26-directory fragmented layout (`00-setup` to `25-cheatsheets`) into canonical 15-module curriculum (`00-setup` through `14-cheatsheets`).
- Preserving and expanding existing querying logic from `02-querying/` into comprehensive 13-section concept notes.

### Root Documentation (Completed)
- Authored `README.md`: Master track overview, syllabus table across 15 modules, dialect matrix, quick start, and study paths.
- Authored `INDEX.md`: Comprehensive alphabetical (A-Z) and topical directory of all database concepts.
- Authored `ROADMAP.md`: Five structured study trajectories (Complete RDBMS Engineer, Backend Developer, Data Analyst, DBA, Interview Sprint).
- Authored `GLOSSARY.md`: 60+ rigorous definitions across transactions, relational algebra, modeling, and indexing.
- Authored `CONTRIBUTING.md`: Note authoring standards, 13-section format, SQL code block conventions, and verification rules.

### Module 00: Setup & RDBMS Architecture (Completed)
- Cleaned up 4 legacy 10-line stubs in `00-setup/`.
- Authored `00-setup/README.md`: Module overview, learning goals, syllabus, Docker quickstart.
- Authored `00-setup/01-what-is-sql-and-the-relational-model.md` (604 lines): Codd's relational model, sets vs bags, physical data independence, 3VL logic.
- Authored `00-setup/02-rdbms-landscape-and-dialects.md` (592 lines): Storage engines, heap vs clustered indexes, process vs thread concurrency, licensing.
- Authored `00-setup/03-installing-postgresql-mysql-sqlite.md` (538 lines): Multi-engine Docker Compose, configuration files, smoke testing.
- Authored `00-setup/04-clients-and-tools.md` (497 lines): `psql`, `mysql`, `sqlite3` CLI tools, meta-commands, `.pgpass`, scripting with `ON_ERROR_STOP=1`.
- Authored `00-setup/05-sample-databases-used-in-this-track.md` (645 lines): Retail e-commerce, Pagila DVD rental, HR/payroll, double-entry financial ledger schemas.
- Authored `00-setup/06-sql-standard-and-how-to-read-the-docs.md` (580 lines): ISO/IEC 9075:2023 evolution, feature conformance, reading BNF grammars.
- Authored `00-setup/07-how-a-query-runs.md` (576 lines): Parser, analyzer, rewriter, cost-based optimizer, Volcano iterator model, buffer pool, WAL.

### Module 01: SQL Fundamentals & Core DDL/DML (Completed)
- Cleaned up 4 legacy 10-line stubs in `01-fundamentals/`.
- Authored `01-fundamentals/README.md`: Module overview, roadmap, learning outcomes.
- Authored `01-fundamentals/01-data-types.md` (583 lines): Integer types, exact numeric vs float, character/text types, temporal types, data alignment, TOAST.
- Authored `01-fundamentals/02-create-alter-drop-table.md` (527 lines): DDL lifecycle, CTAS, metadata-only fast alters, truncate vs delete, lock hierarchies.
- Authored `01-fundamentals/03-constraints.md` (623 lines): PK, FK, unique, check, not null, deferrable constraints, foreign key indexing requirement.
- Authored `01-fundamentals/04-select-statement.md` (573 lines): 9-phase logical query processing pipeline, alias scope, Distinct On.
- Authored `01-fundamentals/05-where-filtering.md` (549 lines): Comparison operators, boolean precedence, sargability, short-circuit non-determinism.
- Authored `01-fundamentals/06-order-by.md` (530 lines): ASC/DESC, NULLS FIRST/LAST defaults, collation, physical sort algorithms, index elimination.
- Authored `01-fundamentals/07-limit-offset.md` (525 lines): Offset degradation, sliding window problem, keyset pagination, ANSI Fetch First With Ties.
- Authored `01-fundamentals/08-null-semantics.md` (600 lines): Three-valued logic truth tables, null in checks vs where, NOT IN (NULL) trap, null bitmap.
- Authored `01-fundamentals/09-pattern-matching.md` (489 lines): LIKE, wildcards, escaping, ILIKE, SIMILAR TO, POSIX regex, trigram GIN indexes.
- Authored `01-fundamentals/10-numeric-functions.md` (543 lines): Round, trunc, ceil, floor, integer division traps, safe division, modulo, random generation.
- Authored `01-fundamentals/11-string-functions.md` (548 lines): 1-based indexing, concat, trimming, padding, octet length vs char length, split_part.
- Authored `01-fundamentals/12-date-and-time.md` (521 lines): Timestamptz, date_trunc, extract, intervals, transaction time vs real clock time, timezones.
- Authored `01-fundamentals/13-case-and-conditionals.md` (572 lines): Simple vs searched CASE, coalesce, nullif, greatest/least, conditional aggregation.
- Authored `01-fundamentals/14-insert-update-delete.md` (553 lines): Multi-row insert, update with joins, returning clause, MVCC tuple writes, HOT updates.
- Authored `01-fundamentals/15-upsert-and-merge.md` (574 lines): Speculative insertion, ON CONFLICT DO UPDATE/NOTHING, EXCLUDED table, ANSI MERGE.
- Authored `01-fundamentals/16-comments-style-and-formatting-conventions.md` (594 lines): SQLFluff style guides, identifier casing, trailing commas, production hygiene.

### 2026-11-26 — Module 02: Advanced Querying & Analytical SQL (Complete)
- Authored `02-querying/README.md`: Module syllabus, learning outcomes, and analytical roadmap.
- Authored `02-querying/01-joins.md`: Inner, Left, Right, Full, Cross, Natural joins, physical execution algorithms (Hash, Merge, Nested Loop).
- Authored `02-querying/02-self-joins.md`: Adjacency list, manager-subordinate, category parent, unique pairs (`a.id < b.id`), multi-level chains.
- Authored `02-querying/03-set-operations.md`: UNION, UNION ALL, INTERSECT, EXCEPT, precedence rules, set vs bag semantics.
- Authored `02-querying/04-subqueries.md`: Uncorrelated vs correlated subqueries, scalar context, derived tables, query decorrelation.
- Authored `02-querying/05-exists-in-any-all.md`: Semi-joins, 3VL null trap with NOT IN, ANY, ALL, vacuous truth.
- Authored `02-querying/06-aggregation-and-group-by.md`: COUNT, SUM, AVG, MIN, MAX, GROUP BY, HAVING, HashAggregate vs GroupAggregate.
- Authored `02-querying/07-grouping-sets-rollup-cube.md`: Multi-dimensional aggregation, GROUPING SETS, ROLLUP, CUBE, GROUPING().
- Authored `02-querying/08-window-functions-basics.md`: Window functions vs aggregates, OVER, PARTITION BY, ORDER BY, execution phase.
- Authored `02-querying/09-window-framing.md`: ROWS, RANGE, GROUPS, frame bounds, EXCLUDE clause, sliding ring buffers.
- Authored `02-querying/10-ranking-functions.md`: ROW_NUMBER, RANK, DENSE_RANK, NTILE, tie handling, deduplication.
- Authored `02-querying/11-value-window-functions.md`: LAG, LEAD, FIRST_VALUE, LAST_VALUE, NTH_VALUE, IGNORE NULLS, sessionization.
- Authored `02-querying/12-common-table-expressions.md`: WITH clause, CTE inlining vs materialization, writable CTEs.
- Authored `02-querying/13-recursive-ctes.md`: WITH RECURSIVE, anchor/recursive members, trees, graph traversal, cycle detection.
- Authored `02-querying/14-lateral-joins.md`: CROSS JOIN LATERAL, LEFT JOIN LATERAL, CROSS APPLY, top-N per category.
- Authored `02-querying/15-semi-joins-and-anti-joins.md`: Relational algebra of semi and anti joins, execution plans, deduplication.
- Authored `02-querying/16-conditional-aggregation.md`: FILTER clause, CASE inside aggregates, matrix pivoting and unpivoting.
- Authored `02-querying/17-json-and-semi-structured.md`: JSON vs JSONB, operators (->, ->>, @>), SQL/JSON path, GIN indexing.
- Authored `02-querying/18-arrays-and-composite-types.md`: Native arrays, unnest, array_agg, operators (&&, @>, ||), composite UDTs.
- Authored `02-querying/19-full-text-search.md`: tsvector, tsquery, Snowball stemming, GIN indexing, ts_rank, ts_headline.
- Authored `02-querying/20-writing-readable-sql.md`: Code refactoring, SQL onion removal, CTE pipelines, style standards.

### 2026-11-26 - Module 03: Database Design & Schema Architecture Completed
- Authored `03-database-design/README.md`: Module overview, learning objectives, navigation index, ER conventions.
- Authored `03-database-design/01-er-modelling.md`: Conceptual, logical, physical models, Chen vs Crow's Foot, cardinality/modality, FK indexing.
- Authored `03-database-design/02-normalisation.md`: 1NF, 2NF, 3NF, BCNF, functional dependencies, Armstrong's axioms, anomaly elimination.
- Authored `03-database-design/03-denormalisation-and-tradeoffs.md`: Materialized views, stored generated columns, CDC, cache dilution, read/write ratios.
- Authored `03-database-design/04-keys.md`: Natural vs surrogate vs composite keys, UUIDv4 vs UUIDv7, ULID, B-Tree fragmentation, dual-key pattern.
- Authored `03-database-design/05-relationships.md`: 1:1, 1:N, M:N junction tables, tree hierarchies, polymorphic anti-pattern, exclusive arcs, shared supertypes.
- Authored `03-database-design/06-constraint-design-and-integrity.md`: NOT NULL, CHECK, UNIQUE NULLS NOT DISTINCT, partial unique indexes, EXCLUDE USING gist, DEFERRABLE constraints.
- Authored `03-database-design/07-naming-conventions.md`: Lowercase snake_case, singular vs plural, role-prefixed FKs, enterprise audit columns, identifier limits.
- Authored `03-database-design/08-schema-patterns.md`: Soft deletes, JSONB delta shadow tables, SCD Types 1-4, multi-tenancy models, Row-Level Security (RLS).
- Authored `03-database-design/09-data-type-choices-and-pitfalls.md`: NUMERIC vs FLOAT, VARCHAR vs TEXT, TIMESTAMPTZ vs TIMESTAMP, ENUM traps, column alignment padding.
- Authored `03-database-design/10-schema-migrations-and-versioning.md`: DDL locking hierarchy, lock queue blockage, lock_timeout, Expand/Contract migrations, CREATE INDEX CONCURRENTLY.
- Authored `03-database-design/11-designing-for-evolution.md`: Additive changes, Postel's Law for DBs, table rewrites vs metadata-only changes, hot/cold vertical partitioning, deprecation phases.
- Authored `03-database-design/12-documenting-schemas.md`: In-database documentation (COMMENT ON), information_schema vs pg_catalog, automated ER diagrams, data dictionaries, GDPR PII tagging.

### 2026-11-26 - Module 04: Transactions, Concurrency & MVCC Completed
- Authored `04-transactions-and-concurrency/README.md`: Module overview, ACID principles, concurrency roadmap, and architecture diagram.
- Authored `04-transactions-and-concurrency/01-acid.md`: Atomicity, Consistency, Isolation, Durability, WAL fsync, dirty page flushing, ARIES algorithm.
- Authored `04-transactions-and-concurrency/02-transaction-control.md`: BEGIN, COMMIT, ROLLBACK, SAVEPOINT, SubTrans overhead, transactional DDL across dialects.
- Authored `04-transactions-and-concurrency/03-isolation-levels-and-anomalies.md`: Read Committed, Repeatable Read, Serializable, Dirty/Fuzzy/Phantom reads, Write Skew, SSI.
- Authored `04-transactions-and-concurrency/04-mvcc-explained.md`: Multi-Version Concurrency Control, xmin/xmax, heap tuples vs InnoDB undo segments, HOT optimization.
- Authored `04-transactions-and-concurrency/05-locking.md`: Heavyweight table locks, row locks, lock manager hash table, lock queue head-of-line blockage.
- Authored `04-transactions-and-concurrency/06-deadlocks-and-diagnosis.md`: Coffman conditions, Wait-For Graphs, cycle detection, deterministic lock ordering, log forensics.
- Authored `04-transactions-and-concurrency/07-optimistic-vs-pessimistic-concurrency.md`: OCC vs 2PL, version columns, Compare-and-Swap (CAS), contention tipping point, retry storms.
- Authored `04-transactions-and-concurrency/08-select-for-update-skip-locked-and-queues.md`: Row lock modes, NOWAIT, SKIP LOCKED, transactional job queues, outbox pattern.
- Authored `04-transactions-and-concurrency/09-idempotency-and-retries.md`: Idempotency keys, transient vs permanent errors, Exponential Backoff with Full Jitter.
- Authored `04-transactions-and-concurrency/10-long-transactions-and-bloat.md`: Snapshot holding, xmin horizon pinning, vacuum starvation, TXID wraparound, pg_repack vs VACUUM FULL.
- Authored `04-transactions-and-concurrency/11-distributed-transactions-overview.md`: Distributed 2PC (PREPARE TRANSACTION), in-doubt crisis, Distributed SQL (Paxos/Raft/TrueTime), Saga pattern, Outbox + CDC.
