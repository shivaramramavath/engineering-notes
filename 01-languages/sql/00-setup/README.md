# Module 00: Setup, Engines & Query Lifecycle

Welcome to **Module 00: Setup, Engines & Query Lifecycle**. This foundational module establishes the mathematical principles, architectural foundations, environment setups, and query execution pipelines that power all relational database management systems (RDBMS).

---

## 🎯 Module Overview & Objectives

By the end of this module, you will understand:
1. **The Relational Model**: Edgar F. Codd’s 1970 relational theory, relational algebra operations (projection, selection, join, cartesian product), and why declarative SQL abstracts physical storage details.
2. **The Modern Database Landscape**: Architectural contrasts between PostgreSQL, MySQL (InnoDB), SQLite, Microsoft SQL Server, and embedded/OLAP engines.
3. **Environment Installation**: Setting up local sandboxes, production-grade Docker containers for PostgreSQL 17 and MySQL 8.4 LTS, and using SQLite 3.50+ for zero-dependency local runs.
4. **Command-Line & GUI Client Mastery**: Configuring `psql`, `mysql`, and `sqlite3`, using environment connection strings, `.pgpass`, formatting switches, and developer GUI workbenches.
5. **Canonical Track Datasets**: The schemas, entities, constraints, and data dictionaries of the sample datasets (E-commerce, Pagila/DVD, HR, Financial Ledger) used across all subsequent modules.
6. **SQL Standards & Documentation Literacy**: Navigating ISO/IEC 9075:2023, understanding SQL-92 through SQL:2023 feature codes, and reading database engine manuals efficiently.
7. **The Query Execution Engine**: The internal journey of an SQL statement through parsing, rewriting, cost-based planning, execution, buffer cache management, and write-ahead logging (WAL).

---

## 📚 Module Curriculum

| Note | Title | Description |
|:---:|:---|:---|
| **01** | [What Is SQL & The Relational Model](./01-what-is-sql-and-the-relational-model.md) | Relational calculus, sets vs multisets, declarative abstraction, and relational invariants. |
| **02** | [RDBMS Landscape & Dialects](./02-rdbms-landscape-and-dialects.md) | Deep comparative breakdown of PostgreSQL, MySQL, SQLite, and SQL Server. |
| **03** | [Installing PostgreSQL, MySQL & SQLite](./03-installing-postgresql-mysql-sqlite.md) | Production Docker Compose, package installations, config parameters, and smoke tests. |
| **04** | [Clients & Developer Tools](./04-clients-and-tools.md) | Mastering `psql`, `mysql`, `sqlite3`, environment variables, profiling flags, and IDEs. |
| **05** | [Sample Databases Used in This Track](./05-sample-databases-used-in-this-track.md) | Complete schemas, ER diagrams, seed queries, and data definitions for our reference DBs. |
| **06** | [SQL Standard & How to Read the Docs](./06-sql-standard-and-how-to-read-the-docs.md) | ISO/IEC 9075:2023 evolution, feature conformance, and deciphering engine docs. |
| **07** | [How a Query Runs: The Internal Lifecycle](./07-how-a-query-runs.md) | Parser ASTs, query rewriting, cost-based optimizer mechanics, executor nodes, and buffer pool. |

---

## 🛠️ Quick Environment Verification

You can instantly test your local execution environment with Python 3's built-in SQLite engine:

```bash
python -c "import sqlite3; con = sqlite3.connect(':memory:'); print('SQLite Version:', con.execute('select sqlite_version()').fetchone()[0])"
```

Or spin up our multi-engine Docker Compose stack:

```yaml
# docker-compose.yml
services:
  postgres:
    image: postgres:17
    environment:
      POSTGRES_USER: engineer
      POSTGRES_PASSWORD: secretpassword
      POSTGRES_DB: learning_db
    ports:
      - "5432:5432"

  mysql:
    image: mysql:8.4
    environment:
      MYSQL_ROOT_PASSWORD: secretpassword
      MYSQL_DATABASE: learning_db
      MYSQL_USER: engineer
      MYSQL_PASSWORD: secretpassword
    ports:
      - "3306:3306"
```

---

[Next: 01 - What Is SQL & The Relational Model →](./01-what-is-sql-and-the-relational-model.md)
