# Module 01: SQL Fundamentals & Core DDL/DML

Welcome to **Module 01: SQL Fundamentals & Core DDL/DML**. This module establishes mastery over the foundational syntax, data typing systems, integrity constraints, data definition language (DDL), basic data querying, three-valued logic (3VL), scalar transformations, and data modification operations (DML).

---

## 🎯 Module Overview & Objectives

By the end of this module, you will understand:
1. **Relational Data Types**: Fixed vs variable strings (`CHAR`, `VARCHAR`, `TEXT`), exact arbitrary-precision numbers (`NUMERIC` / `DECIMAL`) vs IEEE-754 floats (`REAL`, `DOUBLE`), temporal handling (`TIMESTAMP WITH TIME ZONE`, `INTERVAL`), booleans, and binary payloads.
2. **Table Schema Evolution (DDL)**: `CREATE TABLE`, `ALTER TABLE` (column additions, type mutations, constraint attachments), `DROP TABLE`, and `TRUNCATE TABLE`.
3. **Declarative Integrity Constraints**: Enforcing schema invariants at the engine level via `PRIMARY KEY`, `FOREIGN KEY`, `UNIQUE`, `NOT NULL`, and `CHECK`.
4. **The Anatomy of a SELECT Query**: Logical versus physical order of operations (`FROM` -> `WHERE` -> `GROUP BY` -> `HAVING` -> `SELECT` -> `ORDER BY` -> `LIMIT`).
5. **Three-Valued Logic (3VL) & NULL Semantics**: Truth tables (`TRUE`, `FALSE`, `UNKNOWN`), `IS NULL` vs `= NULL`, the subtle traps of `NOT IN (NULL)`, and nullable arithmetic.
6. **Scalar Transformations**: Numeric math, string processing (`SUBSTRING`, `TRIM`, `LPAD`, regex), date/time arithmetic, and conditional branches (`CASE`, `COALESCE`, `NULLIF`).
7. **Data Modification Language (DML)**: Multi-row `INSERT`, atomic conditional `UPDATE`, targeted `DELETE`, idempotent UPSERT (`ON CONFLICT DO UPDATE` / `MERGE`), and atomic primary key retrieval via `RETURNING`.

---

## 📚 Module Curriculum

| Note | Title | Description |
|:---:|:---|:---|
| **01** | [Relational Data Types](./01-data-types.md) | Strict typing, storage footprints, integer ranges, exact numeric vs float, and text. |
| **02** | [Table Definition: CREATE, ALTER, DROP & TRUNCATE](./02-create-alter-drop-table.md) | DDL lifecycle, safe column additions, table locks, and truncate vs delete. |
| **03** | [Data Integrity & Constraints](./03-constraints.md) | Primary keys, foreign keys, unique indexes, not null, and complex check expressions. |
| **04** | [The SELECT Statement & Logical Evaluation Order](./04-select-statement.md) | Anatomy of a query, alias availability, and logical vs physical evaluation. |
| **05** | [Filtering Rows with WHERE & Operators](./05-where-filtering.md) | Comparison operators, boolean chaining (`AND`/`OR`), precedence, and sargability. |
| **06** | [Sorting Results with ORDER BY](./06-order-by.md) | Sorting algorithms, multiple columns, collations, and `NULLS FIRST` vs `NULLS LAST`. |
| **07** | [Pagination: LIMIT, OFFSET & FETCH FIRST](./07-limit-offset.md) | Offset pagination mechanics, high-offset performance degradation, and ANSI `FETCH`. |
| **08** | [Three-Valued Logic & NULL Semantics](./08-null-semantics.md) | 3VL truth tables, unknown propagation, `= NULL` vs `IS NULL`, and `NOT IN` hazards. |
| **09** | [Pattern Matching: LIKE, ILIKE & Regex](./09-pattern-matching.md) | Wildcards (`%`, `_`), case-insensitive matching, SIMILAR TO, and POSIX regular expressions. |
| **10** | [Scalar Numeric & Mathematical Functions](./10-numeric-functions.md) | Rounding modes, integer division, modulo, absolute values, and power/log functions. |
| **11** | [String Functions & Text Manipulation](./11-string-functions.md) | Concatenation, trimming, case conversion, substrings, replacement, and padding. |
| **12** | [Date, Time & Timezone Handling](./12-date-and-time.md) | ISO-8601 formatting, timestamps with time zones, epoch extraction, intervals, and leap years. |
| **13** | [Conditional Expressions: CASE, COALESCE & NULLIF](./13-case-and-conditionals.md) | Simple vs searched `CASE`, null-fallback chaining, and zero-division guards. |
| **14** | [Data Modification: INSERT, UPDATE, DELETE & RETURNING](./14-insert-update-delete.md) | Multi-row batch inserts, update with joins, deletes with limits, and `RETURNING`. |
| **15** | [Upsert Patterns: ON CONFLICT & MERGE](./15-upsert-and-merge.md) | Atomic insert-or-update, conflict targets, idempotency, and ANSI SQL:2008 `MERGE`. |
| **16** | [SQL Comments, Style & Formatting Conventions](./16-comments-style-and-formatting-conventions.md) | Production styling rules, identifier conventions, line wrapping, and linting. |

---

[Next: 01 - Relational Data Types →](./01-data-types.md)
