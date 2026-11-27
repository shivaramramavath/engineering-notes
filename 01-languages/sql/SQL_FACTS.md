# SQL Dialects & RDBMS Facts Baseline (SQL_FACTS.md)

| Category | Canonical Baseline |
|:---|:---|
| **SQL Standard** | ISO/IEC 9075:2023 (SQL:2023) |
| **Primary Dialect** | PostgreSQL 17.x (Stable) / 18 (Upcoming) |
| **Secondary Dialects** | MySQL 8.4 LTS, SQLite 3.50.4 (via Python 3.14 stdlib), Microsoft SQL Server 2022 |
| **Analytical Dialect** | DuckDB 1.1+ (where applicable for OLAP/columnar comparisons) |
| **Verification Toolchain** | Python 3.14.0 stdlib SQLite 3.50.4 runner; PostgreSQL 17 / MySQL 8.4 syntax specs |
| **Last Verified** | November 2026 |

---

## 1. RDBMS Versions & Support Lifecycles

| Engine | Version Tested / Referenced | Release Date | Support Status / EOL |
|:---|:---:|:---:|:---|
| **PostgreSQL** | 17.1 (Baseline) / 18 dev | September 2024 | Supported until November 2029 |
| **MySQL** | 8.4 LTS (Baseline) / 9.x Innovation | April 2024 | Premier support until April 2029, Extended to 2032 |
| **SQLite** | 3.50.4 (Active in Python 3.14) | 2024 / 2025 | Active rolling releases |
| **SQL Server** | 2022 (v16.0) | November 2022 | Mainstream support until January 2028 |
| **MariaDB** | 11.4 LTS | May 2024 | Supported until May 2029 |

---

## 2. SQL:2023 (ISO/IEC 9075:2023) Standards Feature Baseline

SQL:2023 is the current official edition of the ISO/IEC standard (adopted June 2023). Key standard additions:
1. **Property Graph Queries (SQL/PGQ - Part 16)**: Graph pattern matching using `MATCH` over relational tables.
2. **Enhanced JSON Integration**: Native `JSON` data type, standardized dot-notation path navigation (`data.store.book`), and `JSON_TABLE()`.
3. **`UNIQUE NULLS DISTINCT` vs `UNIQUE NULLS NOT DISTINCT`**: Clarifies standard null semantics in unique constraints.
4. **General Functions**: Standardized `GREATEST()`, `LEAST()`, `ANY_VALUE()`, `LPAD()`, `RPAD()`, `LTRIM()`, `RTRIM()`, and underscores in numeric literals (`1_000_000`).

---

## 3. Cross-Dialect Feature Support Matrix

| Feature | Standard Edition | PostgreSQL 17 | MySQL 8.4 LTS | SQLite 3.50.4 | SQL Server 2022 |
|:---|:---:|:---:|:---:|:---:|:---:|
| **`MERGE` Statement** | SQL:2003 | Yes (PG 15+, PG 17 adds `RETURNING` & `merge_action()`) | No (`ON DUPLICATE KEY UPDATE`) | No (`ON CONFLICT DO UPDATE`) | Yes (since 2008) |
| **Window Functions** | SQL:2003 | Yes (PG 8.4+) | Yes (8.0+) | Yes (3.25.0+) | Yes (2005+ / 2012+ frames) |
| **Window Frame Exclusions** | SQL:2003 | Yes (`EXCLUDE CURRENT ROW`, etc.) | Partial | Yes (3.28.0+) | No |
| **JSON Data Type** | SQL:2016 / 2023 | Yes (`json` & `jsonb` binary) | Yes (`JSON` internal binary) | Dynamic (JSON functions on text) | Yes (JSON type in Azure/2022+) |
| **`JSON_TABLE()`** | SQL:2016 / 2023 | Yes (Added in PG 17) | Yes (8.0+) | No (Uses `json_each()` / `json_tree()`) | Yes (2016+) |
| **Native UPSERT** | Dialect | `ON CONFLICT (...) DO UPDATE` | `ON DUPLICATE KEY UPDATE` | `ON CONFLICT (...) DO UPDATE` | `MERGE` statement |
| **`RETURNING` Clause** | SQL:2016 | Yes (on INSERT/UPDATE/DELETE/MERGE) | No | Yes (3.35.0+ on INSERT/UPDATE/DELETE) | No (Uses `OUTPUT` clause) |
| **Generated Columns** | SQL:2016 | `STORED` (PG 12+); Virtual in dev | `VIRTUAL` & `STORED` (5.7+) | `VIRTUAL` & `STORED` (3.31.0+) | `AS (expr) [PERSISTED]` |
| **Recursive CTEs** | SQL:1999 | Yes (`WITH RECURSIVE`) | Yes (`WITH RECURSIVE` 8.0+) | Yes (`WITH RECURSIVE` 3.8.3+) | Yes (`WITH` CTE anchor/recursive) |
| **`LATERAL` Joins** | SQL:2008 | Yes (`LEFT JOIN LATERAL`) | Yes (`LATERAL` 8.0.14+) | No (Emulated via subqueries) | Yes (`CROSS APPLY` / `OUTER APPLY`) |
| **`FILTER` in Aggregates**| SQL:2003 | Yes (`COUNT(*) FILTER (WHERE ...)`) | No (Must use `CASE WHEN`) | Yes (3.30.0+) | No (Must use `CASE WHEN`) |
| **`GROUPING SETS` / `CUBE`**| SQL:1999 | Yes (`GROUPING SETS`, `ROLLUP`, `CUBE`) | `ROLLUP` only | No | Yes (`GROUPING SETS`, `ROLLUP`, `CUBE`) |
| **UUIDv7 (Time-Ordered)**| RFC 9562 | PG 18 native; PG 17 via `pg_uuidv7` | Via expression/app | Via app | Via app |
| **Strict Tables** | Dialect | Yes (standard typed DDL) | Yes (SQL mode `STRICT_TRANS_TABLES`) | Yes (`STRICT` tables 3.37.0+) | Yes (standard typed DDL) |

---

## 4. Key Performance, Indexing & Dialect Caveats

1. **`SELECT *` in Production**:
   - Defeats covering indexes (forces heap/clustered table lookups).
   - Incurs excessive I/O, network bandwidth, and memory allocation.
   - Breaks applications when columns are added, removed, or reordered.
2. **String Concatenation & SQL Injection**:
   - Dynamic concatenation (`"SELECT ... WHERE id = " + input`) creates critical CWE-89 injection vulnerabilities.
   - Production mandates parameterised queries / prepared statements (`$1`, `?`, or `@p1`).
3. **`NOT IN` with `NULL` Values**:
   - Under Three-Valued Logic (3VL), if the subquery returns even a single `NULL`, `x NOT IN (subquery)` evaluates to `UNKNOWN` for all rows, returning an empty result set!
   - Production replacement: `NOT EXISTS` or `WHERE col IS NOT NULL`.
4. **`OFFSET` Pagination at Scale**:
   - `OFFSET 1000000 LIMIT 20` forces the database engine to scan and discard 1,000,000 physical rows before returning 20 rows ($O(N)$ time complexity).
   - Production replacement: Keyset (cursor-based) pagination (`WHERE (created_at, id) < (:last_time, :last_id) ORDER BY created_at DESC, id DESC LIMIT 20`).
5. **Functions on Indexed Columns (Non-Sargable Queries)**:
   - `WHERE YEAR(created_at) = 2026` or `WHERE lower(email) = 'user@example.com'` prevents B-Tree index range scans.
   - Production replacement: Expression / functional indexes, or range conditions (`WHERE created_at >= '2026-01-01' AND created_at < '2027-01-01'`).
6. **Transaction Isolation Realities**:
   - In PostgreSQL, "Read Committed" uses snapshot reads per statement; "Repeatable Read" uses snapshot read per transaction and prevents phantom reads using MVCC (unlike the SQL-92 definition where Repeatable Read permits phantoms).
   - In MySQL InnoDB, Repeatable Read is the default and prevents phantom reads using Next-Key locking.
