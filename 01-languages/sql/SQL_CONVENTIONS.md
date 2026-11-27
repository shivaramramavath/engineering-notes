# SQL Curriculum Conventions & Standards (SQL_CONVENTIONS.md)

> Formal document defining the organizational conventions, metadata schemas, code verification rules, and architectural blueprints for the SQL & Relational Database curriculum (`01-languages/sql/`).

---

## 1. Directory & File Organization Standards

1. **NN-kebab-case Hierarchy**:
   - Module directories use zero-padded two-digit prefixes: `00-setup`, `01-fundamentals`, `02-querying`, ..., `14-cheatsheets`.
   - Notes inside modules follow zero-padded two-digit numbering: `01-topic-name.md`, `02-second-topic.md`.
   - Every module directory contains a dedicated `README.md` serving as a syllabus and navigation hub.
2. **Repository Track Boundaries (Markdown-Only)**:
   - Inside `01-languages/sql/`, **ONLY `.md` files are permitted**.
   - Zero `.sql`, `.db`, `.sqlite`, `.sh`, `.py`, `.json`, `.yml`, or binary database files may be stored in this directory.
   - All DDL schemas, DML statements, queries, and shell execution commands must be embedded as fenced Markdown code blocks with explicit language tags (```sql, ```text, ```bash, etc.).
   - All verification scripts and test runners execute in external scratch directories outside the repo.
3. **Canonical Root Files**:
   - `README.md`: Master curriculum index and platform baseline.
   - `INDEX.md`: Complete alphabetical and categorical directory of every note and concept.
   - `ROADMAP.md`: Career learning trajectories (Beginner, Backend Developer, Data Analyst, DBA/Performance, Interview-Focused) with time estimates.
   - `GLOSSARY.md`: Precise definitions for relational database, storage, indexing, and transactional terms.
   - `CONTRIBUTING.md`: Standards for note authors and reviewers.
   - Tracking files: `SQL_FACTS.md`, `SQL_CONVENTIONS.md`, `SQL_DECISIONS.md`, `SQL_AUDIT.md`, `SQL_COMPLETION_PLAN.md`, `SQL_PROGRESS.md`, `SQL_CHANGELOG.md`, `SQL_FINAL_REPORT.md`.

---

## 2. Concept Note Architectural Standard (13 Sections)

Every concept note strictly adheres to the 13-section architectural model:

| Section | Title | Mandatory Contents |
|---|---|---|
| **Header** | Title & Summary | `# Topic Name` followed by a concise 1–2 sentence blockquote |
| **Metadata** | Metadata Table | Level, Prerequisites (relative links), Estimated Time, Status, Primary Dialect, Tested With |
| **TOC** | Table of Contents | Anchor links to all 13 numbered sections |
| **Section 1** | What & Why | Problem statement, mental model, when to use, when NOT to use |
| **Section 2** | Core Concepts | Formal relational theory, axioms, constraints, data model rules |
| **Section 3** | How It Works Internally | Execution order, storage engine behavior, B-Tree/page internals, diagram (ASCII or Mermaid) |
| **Section 4** | Syntax & Usage | Standard ANSI SQL syntax + Dialect Comparison Box (`PostgreSQL` / `MySQL` / `SQLite` / `SQL Server`) |
| **Section 5** | Practical Examples (5) | 1. Basic Setup & Insert<br>2. Real-World Production Query<br>3. Edge Case / Boundary<br>4. Failure Case (Error + Explanation)<br>5. Optimized / Idiomatic Pattern |
| **Section 6** | Best Practices | Production guidelines, sargability, naming, formatting, deterministic sorting |
| **Section 7** | Common Mistakes Table | Markdown table (6+ rows): Mistake, Why It Fails / Consequence, Diagnostic / Error, Correct Fix |
| **Section 8** | Performance & Security | Query plans, indexes, injection defense, privilege minimization |
| **Section 9** | Comparison Matrix | Side-by-side comparison table of alternative approaches / when to use what |
| **Section 10** | Interview Questions | Exactly 8 deep technical questions with collapsible `<details>` answers |
| **Section 11** | Practice Exercises | Exactly 6 graded exercises (2 Easy, 2 Medium, 2 Hard) with verified `<details>` solutions |
| **Section 12** | Summary & Cheat Sheet | High-density bullet list and quick-reference syntax table |
| **Section 13** | Further Reading | Verified official documentation links for PostgreSQL, MySQL, SQLite |

---

## 3. Metadata Table Schema for SQL Notes

```markdown
| Metadata | Details |
| :--- | :--- |
| **Topic** | [Topic Name] |
| **Module** | `[NN-module-name]` |
| **Track** | `01-languages/sql` |
| **Level** | [Beginner / Intermediate / Advanced / Staff] |
| **Prerequisites** | [Relative links to prerequisite notes] |
| **Time to Read** | [e.g., 25–35 minutes] |
| **Primary Dialect**| PostgreSQL 17+ (Standard ANSI SQL:2023 baseline) |
| **Dialects Tested**| SQLite 3.50.4 (executed via Python 3.14 runner), MySQL 8.4 LTS, SQL Server 2022 |
| **Last Updated** | 2026 |
```

---

## 4. Dialect Difference Box Convention

Every concept note's Section 4 (Syntax & Usage) must include a standardized dialect difference box comparing behavior across the four major relational engines:

````markdown
> ### 🌐 Dialect Differences: Feature Name
>
> | Dialect | Syntax / Support Status | Notes & Caveats |
> | :--- | :--- | :--- |
> | **PostgreSQL (17+)** | `SYNTAX ...` | Standard compliance, extensions (`pg_trgm`, `pgvector`). |
> | **MySQL (8.4 LTS)** | `SYNTAX ...` | Engine differences (InnoDB), strict SQL mode considerations. |
> | **SQLite (3.50+)** | `SYNTAX ...` | Dynamic typing caveats, unsupported features fallback. |
> | **SQL Server (2022)**| `SYNTAX ...` | T-SQL equivalents (e.g., `CROSS APPLY`, `OUTPUT`). |
````

---

## 5. Verification & Code Example Rules

1. **Deterministic Execution**:
   - Every example must create its own self-contained sample schema (DDL + deterministic INSERT statements).
   - Queries with multiple possible row orders MUST include an explicit `ORDER BY` clause to make the output 100% deterministic.
2. **Real Verified Output**:
   - All query results presented in code blocks must be extracted directly from real test runs executed via the external scratch runner (`sql_runner.py`).
   - If an engine cannot be executed locally (e.g. SQL Server), mark its subsection `/* Not executed locally: Verified against vendor specification */`.
3. **Execution Tagging**:
   - Explicitly tag modern features with standard years and engine version numbers: `[SQL:2023]`, `[PostgreSQL 17+]`, `[MySQL 8.4+]`, `[SQLite 3.35+]`.

---

## 6. Cross-Linking & Curriculum Integration

- **Backend Tracks**: Cross-link Node.js (`03-backend/nodejs/19-database-access`), NestJS (`03-backend/nestjs/`), Spring Boot (`03-backend/spring-boot/`), and FastAPI (`03-backend/fastapi/`).
- **Python Track**: Link to `01-languages/python/19-database-access/`.
- **DevOps Track**: Link to Docker containerized databases (`04-devops/docker/`).
- **System Design**: Link to data partitioning, replication, and sharding in `07-system-design/`.
- **ORM Boundary**: Do NOT duplicate ORM tutorials (Prisma, TypeORM, Hibernate, SQLAlchemy). Focus purely on SQL, engine internals, and query performance, linking to language tracks for ORM mappings.
