# Module 03: Relational Modeling & Schema Architecture

## Overview
A high-performance, resilient relational database begins long before any query is executed: it starts at the schema architecture layer. Mistakes made in data modeling—poor key selection, missing foreign key indexes, flawed normalisation, polymorphic associations, or uncontrolled lock-taking during schema migrations—incur compound technical debt that degrades application latency, corrupts financial data, and causes catastrophic production downtime.

This module provides an exhaustive, production-grade guide to relational database design. From formal mathematical normalisation (1NF through BCNF) to real-world schema patterns (temporal tables, soft-delete tombstoning, multi-tenancy, and zero-downtime expand/contract migrations), this syllabus equips software architects and backend engineers with the principles required to build durable, scalable relational systems.

---

## Learning Objectives
By completing this module, you will master:
1. **Entity-Relationship Modeling**: Conceptual, logical, and physical modeling using standard Crow's Foot notation and Mermaid diagrams.
2. **Formal Normalisation & Pragmatic Denormalisation**: Eliminating insertion, update, and deletion anomalies via 1NF, 2NF, 3NF, and BCNF, while knowing exactly when and how to denormalize for read-heavy workloads.
3. **Primary Key Engineering**: Evaluating natural vs surrogate vs composite keys, sequential UUIDv7 vs random UUIDv4, ULID, and their physical B-Tree fragmentation impact.
4. **Relational Constraints & Data Integrity**: Declarative integrity enforcement via foreign key cascades, `CHECK` constraints, PostgreSQL exclusion constraints (`EXCLUDE USING gist`), and deferred constraints.
5. **Real-World Schema Patterns**: Implementing audit logging, Change Data Capture (CDC), Slowly Changing Dimensions (SCD Type 1/2/4), soft deletes, and multi-tenant isolation strategies.
6. **Zero-Downtime Schema Migrations**: Safe DDL execution, non-blocking lock acquisition, multi-phase expand/contract patterns, and production schema versioning tooling.

---

## Module Syllabus

| # | Note | Core Concepts Covered |
| :--- | :--- | :--- |
| **01** | [`01-er-modelling.md`](./01-er-modelling.md) | Conceptual, logical, and physical modeling; Chen vs Crow's Foot; cardinality & participation |
| **02** | [`02-normalisation.md`](./02-normalisation.md) | 1NF, 2NF, 3NF, BCNF formal definitions, functional dependencies, Armstrong's axioms, anomaly elimination |
| **03** | [`03-denormalisation-and-tradeoffs.md`](./03-denormalisation-and-tradeoffs.md) | Read vs write tradeoffs, materialized views, summary rollups, cache invalidation, data divergence |
| **04** | [`04-keys.md`](./04-keys.md) | Natural vs surrogate vs composite, UUIDv4 vs UUIDv7, ULID, auto-increment IDENTITY, B-Tree fragmentation |
| **05** | [`05-relationships.md`](./05-relationships.md) | 1:1, 1:N, M:N junction tables, self-referential trees, polymorphic associations anti-pattern |
| **06** | [`06-constraint-design-and-integrity.md`](./06-constraint-design-and-integrity.md) | CHECK, FK cascades, UNIQUE, partial constraints, PostgreSQL exclusion constraints, DEFERRABLE constraints |
| **07** | [`07-naming-conventions.md`](./07-naming-conventions.md) | Table & column casing, singular vs plural, standard audit fields (`created_at`, `updated_at`), constraint naming |
| **08** | [`08-schema-patterns.md`](./08-schema-patterns.md) | Soft deletes (`deleted_at`), audit trails/CDC, temporal tables (SCD 1/2/4), multi-tenancy models |
| **09** | [`09-data-type-choices-and-pitfalls.md`](./09-data-type-choices-and-pitfalls.md) | Exact numeric vs float, `VARCHAR(n)` vs `TEXT`, `TIMESTAMPTZ` vs `TIMESTAMP`, ENUM vs lookup tables |
| **10** | [`10-schema-migrations-and-versioning.md`](./10-schema-migrations-and-versioning.md) | Zero-downtime expand/contract migrations, table locks (`ACCESS EXCLUSIVE`), migration runners (Flyway, Prisma) |
| **11** | [`11-designing-for-evolution.md`](./11-designing-for-evolution.md) | Adding non-nullable columns safely, safe column type alterations, deprecation pipelines |
| **12** | [`12-documenting-schemas.md`](./12-documenting-schemas.md) | Data dictionaries, `COMMENT ON`, `information_schema` introspection, Mermaid ER generation, schema diffs |

---

## Dialect Standards Across This Module
Unless explicitly stated otherwise:
- **PostgreSQL 17+** is the primary dialect for DDL features (identity columns, exclusion constraints, table partitioning, and data types).
- **Cross-Dialect Comparisons** are provided in every note comparing PostgreSQL 17, MySQL 8.4 LTS, SQLite 3.50+, and SQL Server 2022.
- All code blocks are standard SQL and follow production linter standards (SQLFluff).
