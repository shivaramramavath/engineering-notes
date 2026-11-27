# SQL Track Audit & Gap Analysis (SQL_AUDIT.md)

**Date:** 2026-11-26  
**Auditor:** SQL & Relational Database Curriculum Engineer & Technical Reviewer  
**Curriculum Scope:** `01-languages/sql/`  

---

## 1. Executive Summary

An exhaustive scan of `01-languages/sql/` revealed:
- **Total Existing Markdown Files:** 203 files across 26 fragmented directories (`00-setup` to `25-cheatsheets`).
- **Draft Stubs:** 195 of 203 files (96%) are empty placeholder stubs of 10–25 lines (`> **Status:** draft stub`).
- **Non-Stub Files:** Only 6 partial concept notes exist in `02-querying/` (`00-select.md` through `05-limit-offset.md`), averaging 90–135 lines. None adhere to the required 13-section format, and all lack metadata tables, ASCII/Mermaid engine diagrams, dialect comparison boxes, 6+ row common mistake tables, 8 interview questions in `<details>`, and 6 exercises in `<details>`.
- **Top-Level `09-sql` Folder:** None exists in the repository.
- **Architectural Misalignment:** The existing 26-module layout fragments topics into single-statement micro-folders (`06-joins`, `07-aggregation`, `08-composing-queries`, `09-window-functions`, `10-advanced-queries`), whereas the canonical target curriculum organizes relational database engineering into 15 cohesive, logically ordered modules (`00-setup` through `14-cheatsheets`).
- **Action Plan:**
  1. Consolidate into the target 15-module curriculum (`00-setup` through `14-cheatsheets`).
  2. Preserve every correct fact, table schema, and query from the existing partial notes in `02-querying/` and expand them into the standardized 13-section concept format.
  3. Author all missing modules, production projects, practice labs, interview Q&A, and high-density cheatsheets.

---

## 2. Existing File Audit Summary Table

| Existing Folder | Files | Average Lines | Status | Findings & Action |
|:---|:---:|:---:|:---:|:---|
| `00-setup/` | 5 | 10 lines | Stubs | 4 stubs + 1 README. Override with 7 comprehensive setup notes + README. |
| `01-fundamentals/` | 7 | 10 lines | Stubs | Stubs only. Rebuild to 16 comprehensive notes + README. |
| `02-querying/` | 13 | 85 lines | Partial | 6 partial notes (`00-select` to `05-limit-offset`) + 7 stubs. Extract correct schemas, expand into 20 comprehensive querying notes. |
| `03-functions/` to `21-production/` | ~120 | 10 lines | Stubs | Fragmented legacy stubs. Consolidate into Modules 03–10. |
| `22-practice/` | ~10 | 10 lines | Stubs | Consolidate into `12-practice-labs/`. |
| `23-projects/` | ~15 | 8 lines | Stubs | Consolidate into `11-projects/` with full 4-file markdown specs. |
| `24-interview/` | 13 | 12 lines | Stubs | Consolidate into `13-interview/` with 10 deep topic Q&A files. |
| `25-cheatsheets/` | 13 | 12 lines | Stubs | Consolidate into `14-cheatsheets/` with 11 numbered reference sheets. |

---

## 3. Deprecations, Anti-Patterns & Safety Issues to Guard Against

1. **`SELECT *` in Production**: Promotes lazy query authoring, defeats covering index scans, inflates network traffic, and creates brittle client bindings.
2. **Dynamic String-Concatenated SQL**: Vulnerable to SQL injection (CWE-89). All application integration must enforce parameterised queries.
3. **`NOT IN` with `NULL`**: Under Three-Valued Logic, a single NULL causes `NOT IN` to return an empty set. Must teach `NOT EXISTS` or `IS NOT NULL`.
4. **`OFFSET` Pagination at Scale**: $O(N)$ sequential scans degrade latency on deep pages. Must contrast with Keyset / cursor-based pagination.
5. **Non-Sargable Column Functions**: `WHERE EXTRACT(YEAR FROM created_at) = 2026` invalidates index scans. Must demonstrate sargable range expressions.
6. **Isolation Level Myths**: Vendors differ from ANSI SQL-92 (e.g. Postgres Repeatable Read prevents phantom reads; MySQL default is Repeatable Read with next-key locking).
