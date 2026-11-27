# 04: Clients & Developer Tools

| Property | Value |
|:---|:---|
| **Primary Dialect** | PostgreSQL 17+ (Standard ANSI SQL:2023) |
| **Dialects Compared** | PostgreSQL 17 (`psql`), MySQL 8.4 LTS (`mysql`), SQLite 3.50+ (`sqlite3`), SQL Server 2022 (`sqlcmd`) |
| **Prerequisites** | [03 - Installing PostgreSQL, MySQL & SQLite](./03-installing-postgresql-mysql-sqlite.md) |
| **Target Audience** | Software Engineers, Data Engineers, Database Administrators |
| **Verification Status** | Verified against official CLI tools |

---

## Table of Contents
1. [What & Why](#what--why)
2. [Core Concepts](#core-concepts)
3. [How It Works Internally](#how-it-works-internally)
4. [Syntax & Usage](#syntax--usage)
5. [5 Practical Progressive Examples](#5-practical-progressive-examples)
6. [Best Practices](#best-practices)
7. [Common Mistakes & Gotchas](#common-mistakes--gotchas)
8. [Performance & Security Considerations](#performance--security-considerations)
9. [Comparison Matrix](#comparison-matrix)
10. [8 Real-World Interview Questions](#8-real-world-interview-questions)
11. [6 Graded Practice Exercises](#6-graded-practice-exercises)
12. [Summary & Quick Reference](#summary--quick-reference)
13. [Further Reading](#further-reading)

---

## 1. What & Why

Interacting with a relational database requires a client application that implements the database’s network protocol, transmits queries, and formats tabular results. While Graphical User Interfaces (GUIs) like DBeaver and DataGrip provide convenient visual schema navigation, command-line interfaces (CLIs) like `psql`, `mysql`, and `sqlite3` are the universal tools of production operations, automation scripts, CI/CD pipelines, and remote SSH administration.

A professional engineer must master database CLIs. When a production incident occurs at 3:00 AM, there is rarely a GUI or port-forwarded web interface available—only an SSH bastion host and a terminal. Mastery of CLI meta-commands, output formatters, execution timers, query explainers, and non-interactive scripting separates senior engineers from novices.

Furthermore, configuring connection environments properly using connection URIs, environment variables (`PGHOST`, `PGPORT`), and credential files (`~/.pgpass`, `~/.my.cnf`) prevents hardcoded credentials from leaking into terminal history or source control repositories.

---

## 2. Core Concepts

### 1. SQL Commands vs Meta-Commands
It is vital to distinguish between:
- **Standard SQL Statements**: Sent over the network connection to the server engine for parsing and execution. Must terminate with a semicolon (`;`). Examples: `SELECT`, `INSERT`, `CREATE TABLE`.
- **Client Meta-Commands**: Interpreted directly by the local client program itself before any query is transmitted. They control formatting, inspect metadata catalogs, execute local files, or adjust session variables. They do NOT require semicolons.
  - In `psql`: Prefixed with a backslash (`\dt`, `\d+ table_name`, `\x`, `\timing`).
  - In `sqlite3`: Prefixed with a dot (`.schema`, `.tables`, `.mode markdown`, `.headers on`).
  - In `mysql`: Slash commands or SQL-like built-ins (`status`, `SHOW TABLES;`, `source script.sql`).

### 2. Client Connection Strings & Environment Variables
Clients can establish connections via three standard mechanisms:
1. **CLI Flags**: `psql -h localhost -p 5432 -U engineer -d learning_db`
2. **Connection URIs**: `psql "postgresql://engineer:secretpassword@localhost:5432/learning_db?sslmode=prefer"`
3. **Standard Environment Variables**:
   - PostgreSQL: `PGHOST`, `PGPORT`, `PGUSER`, `PGDATABASE`, `PGPASSWORD`, `PGSSLMODE`
   - MySQL: `MYSQL_HOST`, `MYSQL_TCP_PORT`, `MYSQL_PWD`

### 3. Secure Credential Storage (`.pgpass` and `.my.cnf`)
To connect without interactively typing passwords and without leaking credentials in the shell command history, engines support local configuration files:
- **PostgreSQL `~/.pgpass`**:
  ```text
  # hostname:port:database:username:password
  localhost:5432:learning_db:engineer:secretpassword
  ```
  *(Must be strictly secured with `chmod 600 ~/.pgpass` or PostgreSQL will refuse to read it).*
- **MySQL `~/.my.cnf`**:
  ```ini
  [client]
  user=engineer
  password=secretpassword
  host=localhost
  ```

---

## 3. How It Works Internally

### The Client-Server Protocol Flow (`psql` Frontend/Backend Protocol)

When `psql` or `mysql` connects to a database server, it negotiates a stateful TCP connection:

```
Client (psql)                                    Server (postgres backend)
     |                                                      |
     | ------------ 1. StartupMessage (V3.0) -------------> |
     | <----------- 2. AuthenticationRequest (SCRAM) ------ |
     | ------------ 3. SASLInitialResponse ---------------> |
     | <----------- 4. AuthenticationOk ------------------- |
     | <----------- 5. ParameterStatus (TimeZone, etc.) --- |
     | <----------- 6. ReadyForQuery (Transaction state) -- |
     |                                                      |
     | ------------ 7. Simple Query ("SELECT 1;") ---------> |
     | <----------- 8. RowDescription (Columns, Types) ---- |
     | <----------- 9. DataRow ("1") ---------------------- |
     | <----------- 10. CommandComplete ("SELECT 1") ------ |
     | <----------- 11. ReadyForQuery --------------------- |
```

- **Interactive Mode**: Reads from terminal `stdin`, formats text tables using internal formatting engines (like `libreadline` for command history and tab-completion), and prints to `stdout`.
- **Non-Interactive (Script) Mode**: When output is redirected or piped (`|` or `<`), `psql` disables interactive prompts and suppresses unnecessary decorative formatting.

---

## 4. Syntax & Usage

### Essential Meta-Commands Comparison

| Action / Inspection | PostgreSQL (`psql`) | MySQL (`mysql`) | SQLite (`sqlite3`) | SQL Server (`sqlcmd`) |
|:---|:---|:---|:---|:---|
| **List Tables** | `\dt` | `SHOW TABLES;` | `.tables` | `SELECT name FROM sys.tables;` |
| **Describe Table Schema** | `\d+ table_name` | `DESCRIBE table_name;` | `.schema table_name` | `sp_help table_name;` |
| **List Databases** | `\l` | `SHOW DATABASES;` | `.databases` | `SELECT name FROM sys.databases;` |
| **Toggle Execution Timing** | `\timing` (on/off) | Prefix `pager;` or `SHOW PROFILES;` | `.timer on` | `SET STATISTICS TIME ON;` |
| **Toggle Expanded (Vertical) Display** | `\x` (auto/on/off) | Terminate query with `\G` | `.mode line` | `-y 0` |
| **Execute SQL File** | `\i filename.sql` | `source filename.sql;` | `.read filename.sql` | `-i filename.sql` |
| **Change Output Format** | `\pset format [csv|html|aligned]`| `--batch`, `--xml`, `--html` | `.mode [csv|json|markdown]` | `-s ","` |
| **Quit Client** | `\q` | `exit` or `quit` | `.quit` or `.exit` | `QUIT` |

---

## 5. 5 Practical Progressive Examples

### Example 1: Connecting via Environment & Connection URI
Connecting cleanly without entering passwords interactively.

```bash
# Using standard PostgreSQL environment variables
export PGHOST=localhost
export PGPORT=5432
export PGUSER=engineer
export PGDATABASE=learning_db

# Connect directly (reads credentials from ~/.pgpass)
psql
```
```text
psql (17.4)
Type "help" for help.

learning_db=> 
```

### Example 2: Inspecting Table Details and Indexes with `\d+`
PostgreSQL’s `\d+` command outputs schema details, constraints, indexes, and storage information.

```sql
\d+ accounts
```
```text
                                                              Table "public.accounts"
     Column     |           Type          | Collation | Nullable |           Default           | Storage  | Compression | Stats target | Description 
----------------+-------------------------+-----------+----------+-----------------------------+----------+-------------+--------------+-------------
 account_id     | bigint                  |           | not null | generated always as identity| plain    |             |              | 
 account_number | character varying(34)   |           | not null |                             | extended |             |              | 
 holder_name    | character varying(100)  |           | not null |                             | extended |             |              | 
 balance        | numeric(15,2)           |           | not null | 0.00                        | main     |             |              | 
Indexes:
    "accounts_pkey" PRIMARY KEY, btree (account_id)
    "accounts_account_number_key" UNIQUE CONSTRAINT, btree (account_number)
Check constraints:
    "chk_positive_balance" CHECK (balance >= 0.00)
Access method: heap
```

### Example 3: Expanded Display Mode (`\x` and `\G`)
When queries return wide rows with dozens of columns, standard tabular output wraps illegibly. Expanded display pivots columns vertically.

In `psql`:
```sql
\x on
SELECT account_id, account_number, holder_name, balance FROM accounts LIMIT 1;
```
```text
-[ RECORD 1 ]--+-----------------------------------
account_id     | 1
account_number | US-8829-0012
holder_name    | Samantha Reed
balance        | 14500.50
```

In `mysql`:
```sql
SELECT account_id, account_number, holder_name, balance FROM accounts LIMIT 1\G
```
```text
*************************** 1. row ***************************
    account_id: 1
account_number: US-8829-0012
   holder_name: Samantha Reed
       balance: 14500.50
```

### Example 4: Edge Case: Exporting Clean CSV via CLI Scripting
Exporting query results directly to CSV without interactive headers or formatting junk.

In `psql`:
```bash
psql -U engineer -d learning_db -A -F "," -X -c "SELECT account_id, holder_name, balance FROM accounts;" > accounts_export.csv
```
- `-A` (Unaligned output mode)
- `-F ","` (Field separator is comma)
- `-X` (Ignore `.psqlrc` startup file to ensure clean output)

In `sqlite3`:
```bash
sqlite3 -header -csv sandbox.db "SELECT account_id, holder_name, balance FROM accounts;" > accounts_export.csv
```

### Example 5: Non-Interactive Production Script Execution with Failure Abort
In production deployments, you must ensure that if any statement in a migration script fails, execution immediately halts rather than continuing and corrupting subsequent statements.

In `psql`:
```bash
psql -v ON_ERROR_STOP=1 --single-transaction -f migration_v2.sql
```
- `-v ON_ERROR_STOP=1`: Stops execution immediately on the first error (returns exit code 3).
- `--single-transaction` (or `-1`): Wraps the entire file in `BEGIN ... COMMIT`. If any statement fails, the entire script rolls back cleanly!

---

## 6. Best Practices

1. **Always Use `ON_ERROR_STOP=1` in Scripts**: By default, `psql` logs an error and continues executing the next line of a script. In production deployments, this can leave schemas half-migrated. Always supply `ON_ERROR_STOP=1`.
2. **Never Pass Passwords as CLI Arguments**: Passing passwords via `-p` (MySQL) or CLI strings exposes them in `ps aux` process listings to all local users. Use `.pgpass`, `~/.my.cnf`, or environment variables.
3. **Lock Down Credential Files**: Ensure `~/.pgpass` has permissions `0600` (`chmod 600 ~/.pgpass`). If permissions are looser (e.g., `0644`), `psql` will silently ignore the file for security.
4. **Leverage `.psqlrc` for Productivity**: Configure persistent developer settings in `~/.psqlrc`:
   ```ini
   \set PROMPT1 '%M:%> %n@%/%R%# '
   \timing on
   \pset null '(null)'
   ```
5. **Use GUI Workbenches for Visualization, CLIs for Execution**: Use DBeaver, pgAdmin, or DataGrip for exploratory schema discovery, but always use automated CLI scripts for running migrations and production changes.

---

## 7. Common Mistakes & Gotchas

| Mistake / Anti-Pattern | Why It Fails / Impact | Correct Pattern |
|:---|:---|:---|
| **Omitting Semicolon in `psql`** | `psql` buffers multi-line input until it encounters a semicolon. Pressing Enter without `;` simply opens a continuation prompt (`->`). | Always terminate SQL statements with a semicolon (`;`). Meta-commands do not need semicolons. |
| **Loose Permissions on `~/.pgpass`** | If `~/.pgpass` is readable by other users (mode `644`), `psql` silently ignores it and prompts for a password. | Run `chmod 600 ~/.pgpass`. |
| **Assuming `psql -c` Wraps in a Transaction** | Executing multiple statements in `-c "STMT1; STMT2;"` runs them as separate autocommit transactions unless explicitly enclosed in `BEGIN; ... COMMIT;`. | Explicitly supply `BEGIN; ... COMMIT;` or pass `--single-transaction`. |
| **Piping Binary Outputs with Terminal Garbage** | Running `docker exec -it ... pg_dump > dump.sql` includes carriage returns (`\r\n`) and terminal control characters from the TTY allocation. | Run `docker exec -i ...` (omit `-t`) when redirecting stdout to files. |
| **Confusing `.schema` with `DESCRIBE`** | Running `DESCRIBE table_name;` in SQLite throws a syntax error. | In SQLite, use `.schema table_name` or query `PRAGMA table_info(table_name);`. |
| **Ignoring Client Encoding Settings** | Connecting to a UTF-8 database with a client terminal set to ASCII/Latin-1 causes encoding replacement or corrupted display characters. | Set `\encoding UTF8` in `psql` or `SET NAMES utf8mb4;` in MySQL. |

---

## 8. Performance & Security Considerations

### Performance
- **Paging Large Result Sets**: By default, `psql` routes large query results through a pager (`less`). In scripts or memory-constrained environments, use `\pset pager off` or pass `--no-align` to stream output directly.
- **Fetch Size in GUI Clients**: GUIs like DBeaver and DataGrip limit initial row fetching to 200 or 500 rows. Be aware that a fast query in a GUI might take much longer in a CLI if the GUI only fetched the first 50 rows while the CLI fetches all 1,000,000 rows.

### Security
- **SSH Bastion Forwarding**: Never expose production database ports to the public internet. Use secure SSH tunneling to access private VPC databases:
  ```bash
  ssh -L 5433:internal-db.vpc.local:5432 user@bastion.company.com
  # Then connect locally:
  psql -h localhost -p 5433 -U engineer -d prod_db
  ```

---

## 9. Comparison Matrix

### Database Client Workbenches & Tools

| Tool | Type | License | Strengths | Best Used For |
|:---|:---|:---|:---|:---|
| **`psql`** | CLI | Open Source | Fast, ubiquitous, highly scriptable, powerful meta-commands | Production servers, SSH, automated deployment scripts |
| **`mysql` CLI** | CLI | Open Source | Lightweight, native MariaDB/MySQL support | MySQL administrative scripts, Docker troubleshooting |
| **`sqlite3` CLI** | CLI | Public Domain | Zero dependency, formats markdown/JSON/CSV natively | Embedded development, local data exploration |
| **DBeaver** | GUI | Community (Free) / Commercial | Universal driver support (Postgres, MySQL, Oracle, DuckDB), ER diagrams | General-purpose cross-database developer workbench |
| **DataGrip** | GUI | Commercial (JetBrains) | Unrivaled code completion, refactoring, VCS integration | Heavy daily database development and complex query writing |
| **pgAdmin 4** | Web / GUI | Open Source | PostgreSQL-specific, server performance graphs, maintenance tasks | Dedicated PostgreSQL administration and dashboarding |

---

## 10. 8 Real-World Interview Questions

### Conceptual Questions

#### Q1: What is the purpose of the `.pgpass` file, and why does PostgreSQL enforce strict permission checks on it?
<details>
<summary>Answer</summary>
The `.pgpass` file stores connection credentials (hostname, port, database, username, and password) on the client machine to allow non-interactive authentication.

PostgreSQL enforces strict permissions (it *must* have permissions `0600` or less, readable only by the owning user). If the file is readable by group or world (`0644`), `psql` refuses to read it. This prevents unprivileged local users or malicious processes on a multi-user Unix system from reading database passwords.
</details>

#### Q2: What is the difference between `psql`'s `\d` and `\d+` commands?
<details>
<summary>Answer</summary>
`\d table_name` displays the basic schema: column names, data types, nullability, defaults, and primary/unique/foreign key constraints.

`\d+ table_name` provides extended internal metadata: physical column storage methods (plain, extended, main), toast table references, statistics targets, column-level comments/descriptions, child tables/inheritance, access method (heap), and table options.
</details>

#### Q3: Why is `-v ON_ERROR_STOP=1` critical when executing SQL scripts with `psql` in automated pipelines?
<details>
<summary>Answer</summary>
By default, `psql` follows an "ignore and continue" strategy: if statement 2 of a 10-statement script fails with a syntax or constraint error, `psql` prints an error message to `stderr`, returns a success exit code (0), and continues executing statements 3 through 10. 

In automated CI/CD migration scripts, this can lead to catastrophic partial deployments. Setting `ON_ERROR_STOP=1` forces `psql` to abort execution immediately on the first failure and exit with non-zero status (exit code 3), halting the deployment pipeline.
</details>

#### Q4: How does SQLite’s `.mode` command alter the client’s output serialization?
<details>
<summary>Answer</summary>
The `.mode` meta-command in `sqlite3` switches the tabular presentation format. Supported modes include:
- `box`: Pretty-printed Unicode borders.
- `csv`: Comma-separated values with quoted strings.
- `json`: Array of JSON objects.
- `markdown`: GitHub Flavored Markdown table format.
- `insert`: Generates executable SQL `INSERT INTO` statements.
- `line`: Vertical one-attribute-per-line display.
</details>

### Practical CLI Scripting & Debugging Puzzles

#### Q5: Write a one-line bash command to execute a SQL query on a remote PostgreSQL database, measure execution time, and output the result in pure JSON.
<details>
<summary>Answer</summary>

```bash
psql "postgresql://user:pass@host:5432/db" -t -A -c "SELECT json_agg(t) FROM (SELECT id, name FROM users) t;"
```
*(Option `-t` strips headers/footers, `-A` removes column padding).*
</details>

#### Q6: How do you format a MySQL query result vertically without changing global client settings?
<details>
<summary>Answer</summary>
Terminate the query with `\G` instead of a semicolon (`;`):

```sql
SELECT * FROM orders WHERE order_id = 1001\G
```
</details>

#### Q7: Write a bash script that executes `pg_dump` to create an encrypted or compressed schema-only backup of a PostgreSQL database.
<details>
<summary>Answer</summary>

```bash
#!/usr/bin/env bash
set -euo pipefail

DB_NAME="learning_db"
BACKUP_FILE="schema_$(date +%Y%m%d_%H%M%S).sql.gz"

export PGHOST="localhost"
export PGPORT="5432"
export PGUSER="engineer"

# --schema-only (-s) dumps DDL only, skipping table data
pg_dump -s -d "${DB_NAME}" | gzip > "${BACKUP_FILE}"
echo "Schema backup completed: ${BACKUP_FILE}"
```
</details>

#### Q8: How can you inspect the generated SQL that `psql` sends to system catalogs when executing `\dt` or `\d`?
<details>
<summary>Answer</summary>
Start `psql` with the `-E` (or `--echo-hidden`) flag:

```bash
psql -E -U engineer -d learning_db
```
When you run any meta-command like `\dt`, `psql` will first print the full internal SQL query it executes against `pg_catalog.pg_class` and `pg_catalog.pg_namespace` before displaying the output.
</details>

---

## 11. 6 Graded Practice Exercises

### Beginner Exercises

#### Exercise 1: Exploring Catalogs in `psql`
Connect to your local PostgreSQL instance and use meta-commands to list all tables in the `public` schema, then display the column definitions and constraints of the `accounts` table.
<details>
<summary>Solution</summary>

```sql
\dt public.*
\d accounts
```
</details>

#### Exercise 2: Enabling Timings in SQLite
Using the `sqlite3` CLI, open an in-memory database, enable execution timing, create a table with 10,000 numbers, and verify the elapsed query duration.
<details>
<summary>Solution</summary>

```sql
.timer on
WITH RECURSIVE cnt(x) AS (
    SELECT 1 UNION ALL SELECT x + 1 FROM cnt WHERE x < 10000
)
SELECT COUNT(*) FROM cnt;
```
</details>

### Intermediate Exercises

#### Exercise 3: Automated CSV Generation via Bash
Write a bash script that connects to PostgreSQL and exports the top 5 highest-spending customers to a file named `top_customers.csv` with comma separators and column headers.
<details>
<summary>Solution</summary>

```bash
#!/usr/bin/env bash
psql -U engineer -d learning_db --csv -c "
  SELECT customer_id, first_name, last_name, total_spent 
  FROM customer_spend_summary 
  ORDER BY total_spent DESC 
  LIMIT 5;
" > top_customers.csv
```
</details>

#### Exercise 4: Formatting Markdown Documentation
Using SQLite 3.50+, write a CLI command that inspects the schema of an existing SQLite database and prints the table structure formatted as a Markdown table.
<details>
<summary>Solution</summary>

```bash
sqlite3 -markdown mydata.db "PRAGMA table_info(customers);"
```
</details>

### Advanced Exercises

#### Exercise 5: Atomic Script Execution Pipeline
Construct a production deployment script in bash that executes three migration files (`01.sql`, `02.sql`, `03.sql`) against PostgreSQL. The script must halt immediately if any file fails, rollback all changes made by that file, and log output with ISO timestamps.
<details>
<summary>Solution</summary>

```bash
#!/usr/bin/env bash
set -eo pipefail

export PGHOST="localhost"
export PGPORT="5432"
export PGUSER="deploy_user"
export PGDATABASE="production_db"

MIGRATIONS=("01.sql" "02.sql" "03.sql")

for file in "${MIGRATIONS[@]}"; do
    echo "[$(date -u +'%Y-%m-%dT%H:%M:%SZ')] Applying ${file}..."
    psql -v ON_ERROR_STOP=1 --single-transaction -f "${file}"
    echo "[$(date -u +'%Y-%m-%dT%H:%M:%SZ')] Successfully applied ${file}."
done
echo "All migrations deployed successfully."
```
</details>

#### Exercise 6: Profiling Memory Buffers via CLI
Write a query to run in `psql` that queries `pg_buffercache` (or `pg_stat_database`) to display the cache hit ratio of the current database.
<details>
<summary>Solution</summary>

```sql
SELECT 
    datname,
    blks_read,
    blks_hit,
    ROUND((blks_hit::numeric / NULLIF(blks_hit + blks_read, 0)) * 100, 2) AS cache_hit_ratio_pct
FROM pg_stat_database
WHERE datname = current_database();
```
</details>

---

## 12. Summary & Quick Reference

### Client CLI Quick Flags
| Flag | Description |
|:---|:---|
| `psql -U <user> -d <db>` | Connect with user and database |
| `psql -c "<sql>"` | Execute single SQL command and exit |
| `psql -f <file.sql>` | Execute SQL file |
| `psql -v ON_ERROR_STOP=1` | Abort script on first SQL error |
| `psql -1` / `--single-transaction` | Wrap entire file in single transaction |
| `psql -A -F ","` | Unaligned output with custom delimiter |
| `psql -E` | Echo internal SQL executed by meta-commands |

---

## 13. Further Reading
- PostgreSQL psql Documentation: [Official psql Reference](https://www.postgresql.org/docs/17/app-psql.html)
- MySQL Command-Line Client: [MySQL 8.4 CLI Manual](https://dev.mysql.com/doc/refman/8.4/en/mysql.html)
- SQLite CLI Tutorial: [Command Line Shell For SQLite](https://www.sqlite.org/cli.html)
- [05 - Sample Databases Used in This Track](./05-sample-databases-used-in-this-track.md)
