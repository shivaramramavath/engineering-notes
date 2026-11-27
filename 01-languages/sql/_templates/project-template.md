# Project Template: Production Relational Database Engineering

Each production reference project in `11-projects/` adheres to a strict 4-file markdown structure:

```
11-projects/XX-project-name/
├── README.md                 # Architectural Overview, Problem Statement & ER Diagram
├── 01-schema-and-seed.md     # Production DDL, Constraints, Seed Data & Indices
├── 02-queries.md             # Core Business Queries, Analytics, Window Functions & Reports
├── 03-run-and-results.md     # Verified Real Execution Outputs, ASCII Tables & Logs
└── 04-tuning-and-limitations.md # EXPLAIN Plans, Index Optimizations, Bottlenecks & Scale
```

---

## README.md Structure

```markdown
# Project XX: [Project Title]

## 1. Domain & Business Requirements
- Scenario context and domain problem statement.
- Functional requirements (ACID transaction rules, reporting SLAs).
- Scalability targets (100k+ transactions/day, low-latency reads).

## 2. Entity-Relationship Architecture
- Entity definitions, Cardinality (1:1, 1:N, M:N).
- Mermaid ER Diagram (`erDiagram`).

## 3. Technology Stack & Dialects
- Primary dialect: PostgreSQL 17 / SQLite 3.50.4.
- Indexing strategies (B-Tree, Partial, Composite).
```

---

## 01-schema-and-seed.md Structure
- Full copy-pasteable DDL statements with strict constraints (`NOT NULL`, `CHECK`, `FOREIGN KEY ON DELETE RESTRICT/CASCADE`).
- Deterministic seed datasets (at least 20–50 rows across relational entities).

---

## 02-queries.md Structure
- 6–10 production-grade business queries:
  - OLTP transactional queries (insert with constraint checks).
  - Multi-table JOIN queries with aggregation.
  - Advanced analytical queries (Window functions, CTEs).
  - Defensive error-checking queries.

---

## 03-run-and-results.md Structure
- Exact ASCII tables capturing the real output of every query from `02-queries.md` executed against the verified database runner.

---

## 04-tuning-and-limitations.md Structure
- `EXPLAIN ANALYZE` or `EXPLAIN QUERY PLAN` outputs.
- Before/After query optimization demonstrations.
- Trade-offs, concurrency locking behaviors (`SELECT ... FOR UPDATE`), and architectural limitations.
