# SQL Curriculum Architecture Decisions (SQL_DECISIONS.md)

| ADR ID | Decision Title | Status | Date |
|:---|:---|:---:|:---:|
| **ADR-001** | Consolidate 26 Fragmented Stubs into 15 Cohesive Modules | Accepted | 2026-11-26 |
| **ADR-002** | Standardize Primary Dialect on PostgreSQL 17 with 4-Way Dialect Boxes | Accepted | 2026-11-26 |
| **ADR-003** | Use Scratch External Python SQLite Runner for Deterministic Verification | Accepted | 2026-11-26 |
| **ADR-004** | Enforce Strict Markdown-Only Output Inside Repo Boundaries | Accepted | 2026-11-26 |
| **ADR-005** | Mandatory Deterministic Datasets and ORDER BY Clauses in Examples | Accepted | 2026-11-26 |
| **ADR-006** | Decouple ORMs and Application Data Mappings via Relative Links | Accepted | 2026-11-26 |

---

## ADR-001: Consolidate 26 Fragmented Stubs into 15 Cohesive Modules
- **Context**: The existing `01-languages/sql/` directory contained 26 directories (`00-setup` to `25-cheatsheets`) where 96% of files were 10-line placeholder stubs, artificially fragmenting core relational querying topics (e.g., joins, aggregation, window functions split into separate directories).
- **Decision**: Restructure into the target 15-module curriculum (`00-setup` through `14-cheatsheets`), aligning relational foundations, analytical querying, physical schema design, transactions, indexing, and programmability into cohesive units.
- **Consequences**: Cleaner navigation, logical pedagogical progression, and zero duplicate topic fragmentation.

## ADR-002: Standardize Primary Dialect on PostgreSQL 17 with 4-Way Dialect Boxes
- **Context**: SQL is an ISO standard (SQL:2023), but production systems run on vendor engines with idiosyncratic syntax, types, and optimizer behaviors.
- **Decision**: Standardize all primary code examples and relational explanations on modern PostgreSQL 17. Accompany every syntax section with a standardized 4-way Dialect Comparison Box comparing PostgreSQL, MySQL 8.4 LTS, SQLite 3.50+, and SQL Server 2022.
- **Consequences**: Readers learn clean, standard-compliant SQL first, while immediately understanding engine-specific gotchas in production.

## ADR-003: Use Scratch External Python SQLite Runner for Deterministic Verification
- **Context**: Quality rules mandate that every query output claimed in notes must be REAL and extracted from execution logs.
- **Decision**: Author an external verification script (`<artifactDir>/scratch/sql_runner.py`) utilizing Python 3.14's built-in SQLite 3.50.4 engine. Execute all reproducible DDL and DML in memory and paste exact formatted ASCII tables. Mark unsupported engine-specific features (e.g. SQL Server T-SQL) as verified against vendor specification.
- **Consequences**: Guarantees zero fabricated outputs while operating cleanly without requiring Docker or root database credentials.

## ADR-004: Enforce Strict Markdown-Only Output Inside Repo Boundaries
- **Context**: Repository rules strictly prohibit committing raw `.sql`, `.db`, `.sh`, `.py`, `.json`, or temporary test files into `01-languages/sql/`.
- **Decision**: All schemas, queries, migrations, and scripts must reside inside fenced Markdown code blocks (`sql`, `bash`, `python`) within `.md` files. All scratch scripts, runners, and temporary databases reside exclusively outside the repository.
- **Consequences**: Clean Git history, markdown linter compliance, zero repository state pollution.

## ADR-005: Mandatory Deterministic Datasets and ORDER BY Clauses in Examples
- **Context**: In relational algebra, tables are sets of unordered tuples. Without `ORDER BY`, results can vary across engine versions and storage scans.
- **Decision**: Every example must construct a minimal, self-contained dataset (DDL + INSERTs) and enforce explicit `ORDER BY` clauses whenever multiple rows are selected.
- **Consequences**: 100% reproducible test outputs for readers across all operating systems.

## ADR-006: Decouple ORMs and Application Data Mappings via Relative Links
- **Context**: Readers often conflate ORM patterns (Prisma, TypeORM, Hibernate, SQLAlchemy) with SQL engine behavior.
- **Decision**: The SQL track focuses purely on SQL queries, relational calculus, schema design, and engine internals. Provide relative links to language tracks (`01-languages/python/19-database-access`, `03-backend/nodejs`, `03-backend/spring-boot`) for ORM mappings.
- **Consequences**: Prevents duplicate documentation and maintains strict domain boundaries.
