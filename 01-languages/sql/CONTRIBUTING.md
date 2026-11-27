# Contributing to the SQL Track (CONTRIBUTING.md)

Thank you for contributing to the SQL & Relational Database Engineering curriculum. To maintain production-grade consistency across all notes, reference projects, and practice labs, all contributions must strictly adhere to the standards defined below.

---

## 🎯 Architectural Principles

1. **PostgreSQL 17+ Baseline**: All queries, architectures, and engine explanations must use modern PostgreSQL 17+ as the primary reference implementation, adhering to ANSI SQL:2023 syntax.
2. **First-Class Cross-Dialect Coverage**: Every concept note must explicitly contrast behavior across:
   - **PostgreSQL 17+**
   - **MySQL 8.4 LTS**
   - **SQLite 3.50+**
   - **SQL Server 2022 (T-SQL)**
3. **Internals-First Pedagogy**: Never settle for syntax alone. Explain *why* the engine behaves the way it does: parser trees, planner costs, buffer page layouts, write-ahead logs, and MVCC tuple visibility.
4. **Markdown-Only Policy**: No `.sql`, `.db`, `.py`, or binary files may be committed into `01-languages/sql/`. All SQL schemas and queries must reside in fenced markdown code blocks (` ```sql `).
5. **Deterministic Verification**: Every query example must be tested and confirmed against a real relational database engine (via the scratch verification runner). Never invent query outputs or plan costs.

---

## 📐 Mandatory 13-Section Concept Note Structure

Every concept note (`01-languages/sql/MM-module/NN-topic.md`) must follow this exact 13-section structure without omitting or reordering sections:

1. **Metadata Header**:
   ```markdown
   # Topic Title
   
   | Property | Value |
   |:---|:---|
   | **Primary Dialect** | PostgreSQL 17+ (ANSI SQL:2023) |
   | **Dialects Compared** | PostgreSQL 17, MySQL 8.4 LTS, SQLite 3.50+, SQL Server 2022 |
   | **Prerequisites** | `[Previous Note Title](../MM-module/NN-note.md)` |
   | **Target Audience** | Backend Engineers, Database Architects, Data Analysts |
   | **Verification Status** | Verified against SQLite 3.50+ runner / PostgreSQL 17 spec |
   ```
2. **Table of Contents**: Linked Markdown list of all 13 sections.
3. **What & Why**:
   - Motivation, problem statement, business impact, and historical context.
4. **Core Concepts**:
   - Fundamental definitions, formal relational theory, and mathematical/algebraic properties.
5. **How It Works Internally**:
   - Engine architecture, storage layouts, query planner cost models, memory management.
   - At least one clear ASCII or Mermaid diagram illustrating data flow or page structure.
6. **Syntax & Usage**:
   - Formal BNF or canonical SQL syntax.
   - Mandatory **Cross-Dialect Comparison Matrix** covering all 4 core engines.
7. **5 Practical Progressive Examples**:
   - Example 1: Basic / Foundational query.
   - Example 2: Real-World production scenario.
   - Example 3: Edge Case (handling NULLs, empty sets, or large scale).
   - Example 4: Failure Mode / Anti-Pattern (showing why bad syntax or logic fails).
   - Example 5: Optimized / Performance-tuned production variant.
   - Every example must show realistic schema, query, and expected tabular output.
8. **Best Practices**:
   - Actionable production design rules, naming conventions, indexing requirements, and security guidelines.
9. **Common Mistakes & Gotchas (Table Format)**:
   - A structured table with at least 6 distinct failure modes:
     | Mistake / Anti-Pattern | Why It Fails / Impact | Correct Pattern |
10. **Performance & Security Considerations**:
    - Index utilization, sargability, buffer hit ratios, query cost implications.
    - SQL injection defense, least privilege, data masking, row-level protection.
11. **Comparison Matrix**:
    - Trade-off matrix contrasting the topic with alternative techniques (e.g., `CTE` vs `Subquery` vs `Temporary Table`).
12. **8 Real-World Interview Questions (with `<details>` Solutions)**:
    - 4 Conceptual / Architectural questions.
    - 4 Hands-on query writing / debugging puzzles.
13. **6 Graded Practice Exercises (with `<details>` Solutions)**:
    - 2 Beginner, 2 Intermediate, 2 Advanced problems.
14. **Summary & Quick Reference**:
    - Key takeaways bullet list and condensed syntax cheatsheet.
15. **Further Reading & Official Documentation**:
    - Official links to PostgreSQL documentation, MySQL Reference Manual, SQLite docs, and standard publications.

---

## 💻 Code Block Conventions

- SQL statements: Fenced with ```` ```sql ````.
- SQL keywords: Always `UPPERCASE` (`SELECT`, `INSERT`, `FROM`, `WHERE`, `GROUP BY`).
- Identifiers: Always `snake_case` (`customer_id`, `order_items`, `created_at`).
- Tabular outputs: Fenced with ```` ```text ```` using clean ASCII borders:
  ```text
  +----+------------+---------+
  | id | name       | balance |
  +----+------------+---------+
  |  1 | Alice Chen |  250.00 |
  +----+------------+---------+
  ```

---

## 🧪 Verification Guidelines

- All queries should be run through the scratch Python verification runner:
  ```bash
  python scratch/sql_runner.py "SELECT datetime('now');"
  ```
- If a dialect feature cannot be run locally (e.g., PostgreSQL-specific `LATERAL` joins or SQL Server-specific syntax), mark the output clearly as `Specification verified against PostgreSQL 17 / SQL Server 2022 documentation`. Never invent synthetic execution outputs.
