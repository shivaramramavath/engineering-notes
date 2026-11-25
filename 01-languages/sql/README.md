# SQL Mastery

A structured learning and reference repository for SQL, covering PostgreSQL and MySQL from first query to production.

It supports two ways of working:

- **Learning:** follow the numbered folders in order. Each folder's README lists its notes in reading order.
- **Reference:** jump straight to a topic folder, the interview prep, or the cheatsheets.

## Contents

| Section | Folder | What it covers |
|---|---|---|
| [Setup](./00-setup/README.md) | `00-setup/` | Install PostgreSQL and MySQL, pick a client, and load the sample databases used by every later example. |
| [Fundamentals](./01-fundamentals/README.md) | `01-fundamentals/` | How SQL is structured: statement categories, data types, NULL semantics and operators. |
| [Querying](./02-querying/README.md) | `02-querying/` | SELECT through filtering, sorting, deduplication and limiting: the first real queries. |
| [Functions](./03-functions/README.md) | `03-functions/` | Scalar functions and conditional expressions that every later query relies on. |
| [Modifying Data](./04-modifying-data/README.md) | `04-modifying-data/` | INSERT, UPDATE, DELETE, upsert and RETURNING. |
| [Tables and Constraints](./05-tables-and-constraints/README.md) | `05-tables-and-constraints/` | DDL and data integrity: the prerequisite for joins and schema design. |
| [Joins](./06-joins/README.md) | `06-joins/` | Combining tables, from the basic join types to multi-table queries. |
| [Aggregation](./07-aggregation/README.md) | `07-aggregation/` | Summarizing data with aggregate functions and grouping. |
| [Composing Queries](./08-composing-queries/README.md) | `08-composing-queries/` | Building queries out of other queries: subqueries, CTEs, recursion and set operations. |
| [Window Functions](./09-window-functions/README.md) | `09-window-functions/` | Calculations across related rows without collapsing them. |
| [Advanced Queries](./10-advanced-queries/README.md) | `10-advanced-queries/` | Real-world query patterns built from the earlier topics. |
| [Schema Design](./11-schema-design/README.md) | `11-schema-design/` | Designing relational schemas: modeling, normalization and common patterns. |
| [Indexes](./12-indexes/README.md) | `12-indexes/` | How indexes work and how to choose them. |
| [Query Performance](./13-query-performance/README.md) | `13-query-performance/` | How queries execute and how to make slow ones fast. |
| [Transactions](./14-transactions/README.md) | `14-transactions/` | Correctness under concurrency: ACID, isolation, MVCC and locking. |
| [Views and Routines](./15-views-and-routines/README.md) | `15-views-and-routines/` | Server-side logic: views, materialized views, routines and triggers. |
| [Security](./16-security/README.md) | `16-security/` | Protecting data: privileges, injection, row-level security, encryption and auditing. |
| [Migrations](./17-migrations/README.md) | `17-migrations/` | Evolving a schema safely over time. |
| [PostgreSQL](./18-postgresql/README.md) | `18-postgresql/` | PostgreSQL-specific internals and features. |
| [MySQL](./19-mysql/README.md) | `19-mysql/` | MySQL-specific internals and features. |
| [Node.js Integration](./20-nodejs-integration/README.md) | `20-nodejs-integration/` | Using SQL from Node.js: drivers, pooling, transactions, error handling, ORMs and query builders. |
| [Production](./21-production/README.md) | `21-production/` | Running databases in production: replication, partitioning, sharding, backups and monitoring. |
| [Practice](./22-practice/README.md) | `22-practice/` | Exercises by topic, with solutions kept separate to avoid spoilers. |
| [Projects](./23-projects/README.md) | `23-projects/` | Eight capstone databases, ordered by difficulty. |
| [Interview](./24-interview/README.md) | `24-interview/` | Interview preparation by topic. Study after the main learning path. |
| [Cheatsheets](./25-cheatsheets/README.md) | `25-cheatsheets/` | One-page references for fast lookup. |

See [ROADMAP.md](./ROADMAP.md) for suggested paths and [CONTRIBUTING.md](./CONTRIBUTING.md) for how to write notes.

> **Status:** structure complete; individual notes are being written folder by folder.
