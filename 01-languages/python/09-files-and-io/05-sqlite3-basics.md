---
title: "SQLite3 Basics in Python"
description: "Comprehensive guide to embedded relational data persistence with Python's standard library sqlite3 module: connection lifecycles, cursors, parameterized queries, transactions, ACID guarantees, custom row factories, and CPython sqlite3 architecture."
module: "09-files-and-io"
order: 5
tags:
  - python
  - sqlite3
  - database
  - sql
  - io
  - transactions
  - acid
difficulty: "Intermediate"
cpython_version: "3.12+"
last_verified: "2026-03"
---

# SQLite3 Basics in Python

[<- Back to Module Index](./README.md)

---

## Table of Contents
1. [What & Why](#what--why)
2. [Core Concepts & Mental Model](#core-concepts--mental-model)
3. [How It Works Internally](#how-it-works-internally)
4. [Syntax & API Reference](#syntax--api-reference)
5. [Code Examples](#code-examples)
6. [Edge Cases, Pitfalls & Anti-Patterns](#edge-cases-pitfalls--anti-patterns)
7. [Performance & Memory Characteristics](#performance--memory-characteristics)
8. [CPython Internals & Bytecode](#cpython-internals--bytecode)
9. [Best Practices & Idiomatic Patterns](#best-practices--idiomatic-patterns)
10. [Real-World Case Study: Resilient WAL-Mode Ingestion Queue](#real-world-case-study-resilient-wal-mode-ingestion-queue)
11. [Interview Questions & Deep-Dive Scenarios](#interview-questions--deep-dive-scenarios)
12. [Practical Exercises & Code Katas](#practical-exercises--code-katas)

---

## What & Why

Python includes a zero-configuration, serverless, self-contained relational database engine directly in its standard library via the `sqlite3` module. Written in ANSI C by D. Richard Hipp, SQLite stores an entire relational database—schemas, tables, indices, triggers, and data—within a single cross-platform disk file (or entirely in transient memory).

### Motivation
Developers frequently need persistent, relational query capabilities without incurring the operational overhead, network latency, and daemon management of standalone database servers like PostgreSQL or MySQL. Plain text files (CSV, JSON) lack ACID transactions, indexing, structured joins, and atomic concurrency controls. Full client-server RDBMS solutions introduce connection pooling, authentication pipelines, network firewalls, and deployment dependencies. `sqlite3` provides full SQL92 compliance, primary/foreign key constraints, B-tree indexed lookups, and transactional guarantees right out of the box with zero external dependencies.

### Real-World Analogy
Consider a personal file cabinet with a mechanical indexing system versus a remote document warehouse staffed by dispatch workers. If you need to retrieve a customer folder by account ID instantly on your desk, opening your personal cabinet drawer (SQLite) requires no phone call, network request, or staff authorization. It operates in your exact physical space (process memory and local disk). However, if 500 branch offices must simultaneously write to that single physical folder, a dedicated computerized warehouse dispatch system (PostgreSQL/MySQL) becomes necessary.

### When to Use vs When Not to Use
- **Use SQLite for:**
  - Local desktop and mobile client storage (CLI tools, desktop apps, embedded sensors).
  - High-performance read-heavy services (caching, configuration lookup, read-only data bundles).
  - Integration testing and local development without standing up containerized database services.
  - Low-to-medium write throughput web applications (thousands of writes per hour using WAL mode).
  - Ephemeral in-memory relational data staging (`:memory:`).
- **Do Not Use SQLite for:**
  - High-frequency distributed concurrent writes across multiple independent server nodes.
  - Giant multi-terabyte datasets exceeding single-node volume capacities.
  - Complex network-level role-based access control (RBAC) managed at the database daemon layer.
  - Applications requiring client-server replication topologies (Active-Active multi-master clusters).

---

## Core Concepts & Mental Model

The Python `sqlite3` driver adheres to PEP 249 (Python Database API Specification v2.0). The model consists of four fundamental components:

```
+-----------------------------------------------------------------------+
| Python Process Runtime                                                |
|                                                                       |
|  +--------------------+         +----------------------------------+  |
|  | sqlite3.Connection | ------> | sqlite3.Cursor                   |  |
|  +--------------------+         +----------------------------------+  |
|            |                                    |                     |
|            | Context Management                 | execute()           |
|            | (Transactions: commit/rollback)    | executemany()       |
|            v                                    v                     |
|  +-----------------------------------------------------------------+  |
|  | CPython C Extension (_sqlite3.c)                               |  |
|  +-----------------------------------------------------------------+  |
|            |                                                          |
|            v                                                          |
|  +-----------------------------------------------------------------+  |
|  | Embedded SQLite Engine (sqlite3.dll / libsqlite3.so)            |  |
|  | - SQL Compiler & VDBE (Virtual Database Engine)                 |  |
|  | - B-Tree Storage Engine                                         |  |
|  | - Pager & OS Interface Layer (VFS)                              |  |
|  +-----------------------------------------------------------------+  |
+------------|----------------------------------------------------------+
             |
             v
   +-------------------+       +--------------------+
   | Database File     | <---> | Write-Ahead Log    |
   | (data.db)         |       | (data.db-wal)      |
   +-------------------+       +--------------------+
```

### Key Abstractions
1. **Connection (`sqlite3.Connection`)**: Represents the physical or in-memory database file handle. It manages database-level configurations (WAL mode, timeouts, autocommit semantics) and transactional boundaries.
2. **Cursor (`sqlite3.Cursor`)**: Traverses the result set of an executed SQL query. It tracks statement execution state, parameters, row buffering, and pagination via `fetchone()`, `fetchmany()`, and `fetchall()`.
3. **Row Factory (`sqlite3.Row` or custom callable)**: Converts raw tuple outputs from C-level SQLite rows into rich data structures (e.g., named dictionaries or dataclasses) without sacrificing tuple performance.
4. **VFS (Virtual File System)**: SQLite's internal operating system abstraction that translates B-tree page requests into POSIX or Win32 file I/O operations (`read()`, `write()`, `fsync()`).

---

## How It Works Internally

### 1. Statement Compilation and the VDBE
When `cursor.execute("SELECT * FROM users WHERE id = ?", (42,))` is called:
1. Python passes the SQL string and parameter tuple to the C extension `_sqlite3`.
2. The SQLite engine compiles the SQL string into bytecode for SQLite's internal Virtual Database Engine (VDBE) via `sqlite3_prepare_v2()`.
3. Parameters are bound to their respective `?` or `:name` placeholders using type-appropriate binding functions (`sqlite3_bind_int64`, `sqlite3_bind_text`, etc.).
4. The VDBE executes the bytecode program step-by-step via `sqlite3_step()`.
5. For each row produced, CPython constructs a Python tuple (or custom row structure) and returns it to the cursor's iterator buffer.
6. The statement handle is finalized or cached for statement reuse via `sqlite3_finalize()` or `sqlite3_reset()`.

### 2. Transaction Management and Autocommit in Python 3.12+
Historically (Python 3.11 and earlier), `sqlite3` managed transactions with implicit DML inspection: executing an `INSERT`, `UPDATE`, or `DELETE` implicitly opened a transaction (`BEGIN`), while DDL statements (`CREATE TABLE`, `DROP`) implicitly committed pending transactions.

In Python 3.12+, PEP 690 introduced explicit `autocommit` control on connections:
- `sqlite3.connect(..., autocommit=False)`: Standard PEP 249 behavior where transactions must be committed explicitly.
- `sqlite3.connect(..., autocommit=True)`: SQLite's native autocommit mode, where every individual statement commits immediately unless wrapped in an explicit `BEGIN ... COMMIT` block.
- `sqlite3.connect(..., autocommit=sqlite3.LEGACY_TRANSACTION_CONTROL)`: Retains pre-3.12 legacy magic behavior.

### 3. Journaling Modes: Rollback Journal vs. WAL (Write-Ahead Logging)
- **Default (Rollback Journal)**: When a transaction writes, original unedited database pages are copied to a separate journal file (`db.db-journal`). Readers are completely blocked while a writer is writing.
- **WAL Mode (`PRAGMA journal_mode=WAL;`)**: Writes append new modified pages to a separate log file (`db.db-wal`). Readers read original pages from the main DB or uncommitted versions from WAL concurrently. **Writers do not block readers, and readers do not block writers.**

---

## Syntax & API Reference

### Primary Connection Constructors and Methods
```python
import sqlite3

conn = sqlite3.connect(
    database=":memory:",            # File path, Path object, or ":memory:"
    timeout=5.0,                   # Seconds to wait for file locks before BusyError
    detect_types=0,                # PARSE_DECLTYPES | PARSE_COLNAMES
    isolation_level=None,          # Controls legacy transaction management
    check_same_thread=True,        # If False, allows cross-thread sharing
    factory=sqlite3.Connection,    # Subclassed Connection type
    cached_statements=128,         # LRU statement cache size
    uri=False,                     # Enables file: URI parsing (e.g. read-only mode)
    autocommit=False               # Python 3.12+ explicit autocommit control
)
```

### Essential Cursor Methods
```python
cursor = conn.cursor()

cursor.execute(sql: str, parameters: tuple | dict = ()) -> sqlite3.Cursor
cursor.executemany(sql: str, seq_of_parameters: Iterable[tuple | dict]) -> sqlite3.Cursor
cursor.executescript(sql_script: str) -> sqlite3.Cursor
cursor.fetchone() -> tuple | sqlite3.Row | None
cursor.fetchmany(size: int = cursor.arraysize) -> list[tuple | sqlite3.Row]
cursor.fetchall() -> list[tuple | sqlite3.Row]
```

### Useful Connection Context Management
```python
# Context management on Connection controls TRANSACTION boundaries:
with conn:
    # Executions inside block automatically COMMIT on exit
    # or ROLLBACK if an unhandled exception is raised
    conn.execute("INSERT INTO users (name) VALUES (?)", ("Alice",))
```

---

## Code Examples

### Example 1: Basic In-Memory Relational Workflow
```python
import sqlite3

def run_basic_demo():
    # In-memory database ceases to exist once connection closes
    conn = sqlite3.connect(":memory:")
    cursor = conn.cursor()

    # DDL: Create table
    cursor.execute("""
        CREATE TABLE products (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            sku TEXT UNIQUE NOT NULL,
            name TEXT NOT NULL,
            price_cents INTEGER NOT NULL CHECK (price_cents >= 0)
        );
    """)

    # DML: Parameterized insertion using positional '?' placeholders
    cursor.execute(
        "INSERT INTO products (sku, name, price_cents) VALUES (?, ?, ?)",
        ("PROD-001", "Mechanical Keyboard", 12900)
    )

    # Retrieval
    cursor.execute("SELECT id, sku, name, price_cents FROM products WHERE sku = ?", ("PROD-001",))
    row = cursor.fetchone()
    print("Fetched Row:", row)
    
    conn.close()

run_basic_demo()
# Output:
# Fetched Row: (1, 'PROD-001', 'Mechanical Keyboard', 12900)
```

### Example 2: Batch Insertion with `executemany()` and Parameter Dictionaries
```python
import sqlite3

def run_batch_demo():
    conn = sqlite3.connect(":memory:")
    cursor = conn.cursor()
    cursor.execute("CREATE TABLE metrics (metric_name TEXT, val REAL, timestamp INTEGER);")

    records = [
        {"name": "cpu_load", "val": 0.42, "ts": 1700000000},
        {"name": "mem_free_mb", "val": 4096.0, "ts": 1700000001},
        {"name": "disk_io_wait", "val": 0.05, "ts": 1700000002},
    ]

    # Named placeholder syntax :name maps cleanly to dictionary keys
    cursor.executemany(
        "INSERT INTO metrics VALUES (:name, :val, :ts)",
        records
    )
    conn.commit()

    cursor.execute("SELECT COUNT(*) FROM metrics")
    total = cursor.fetchone()[0]
    print(f"Total metrics inserted: {total}")
    conn.close()

run_batch_demo()
# Output:
# Total metrics inserted: 3
```

### Example 3: Dictionary Access via `sqlite3.Row` Factory
```python
import sqlite3

def run_row_factory_demo():
    conn = sqlite3.connect(":memory:")
    # Set row_factory to sqlite3.Row for column-name keyed indexing
    conn.row_factory = sqlite3.Row
    cursor = conn.cursor()

    cursor.execute("CREATE TABLE users (id INTEGER PRIMARY KEY, email TEXT, active INTEGER);")
    cursor.execute("INSERT INTO users (email, active) VALUES (?, ?)", ("carol@example.com", 1))
    conn.commit()

    cursor.execute("SELECT id, email, active FROM users WHERE email = ?", ("carol@example.com",))
    user = cursor.fetchone()

    # Supports both index and case-insensitive column name lookup
    print("User ID (index 0):", user[0])
    print("User Email (keyed):", user["email"])
    print("Keys available:", user.keys())
    conn.close()

run_row_factory_demo()
# Output:
# User ID (index 0): 1
# User Email (keyed): carol@example.com
# Keys available: ['id', 'email', 'active']
```

### Example 4: Custom Type Converters and Adapters
```python
import sqlite3
from dataclasses import dataclass
import json

@dataclass
class Coordinate:
    lat: float
    lon: float

# Adapter: Python Object -> SQLite native type (TEXT)
def adapt_coordinate(coord: Coordinate) -> str:
    return f"{coord.lat}:{coord.lon}"

# Converter: SQLite bytes -> Python Object
def convert_coordinate(val: bytes) -> Coordinate:
    lat_str, lon_str = val.decode("utf-8").split(":")
    return Coordinate(lat=float(lat_str), lon=float(lon_str))

def run_type_conversion_demo():
    sqlite3.register_adapter(Coordinate, adapt_coordinate)
    sqlite3.register_converter("coordinate", convert_coordinate)

    # detect_types enables parsing of declared types
    conn = sqlite3.connect(":memory:", detect_types=sqlite3.PARSE_DECLTYPES)
    cursor = conn.cursor()

    cursor.execute("CREATE TABLE waypoints (name TEXT, loc coordinate);")
    home = Coordinate(37.7749, -122.4194)
    cursor.execute("INSERT INTO waypoints (name, loc) VALUES (?, ?)", ("San Francisco", home))
    conn.commit()

    cursor.execute("SELECT name, loc FROM waypoints")
    name, location = cursor.fetchone()
    print(f"Retrieved {name}: Lat={location.lat}, Lon={location.lon}, Type={type(location).__name__}")
    conn.close()

run_type_conversion_demo()
# Output:
# Retrieved San Francisco: Lat=37.7749, Lon=-122.4194, Type=Coordinate
```

### Example 5: Explicit Transactions with Context Managers and Rollbacks
```python
import sqlite3

def run_transaction_demo():
    conn = sqlite3.connect(":memory:")
    cursor = conn.cursor()
    cursor.execute("CREATE TABLE bank_accounts (name TEXT PRIMARY KEY, balance INTEGER);")
    cursor.execute("INSERT INTO bank_accounts VALUES ('Alice', 500), ('Bob', 200)")
    conn.commit()

    def transfer(sender: str, recipient: str, amount: int):
        # Using connection as context manager wraps block in a transaction
        with conn:
            cursor.execute("UPDATE bank_accounts SET balance = balance - ? WHERE name = ?", (amount, sender))
            # Simulate a business rule violation
            cursor.execute("SELECT balance FROM bank_accounts WHERE name = ?", (sender,))
            if cursor.fetchone()[0] < 0:
                raise ValueError(f"Insufficient funds in account: {sender}")
            cursor.execute("UPDATE bank_accounts SET balance = balance + ? WHERE name = ?", (amount, recipient))

    try:
        transfer("Alice", "Bob", 600)
    except ValueError as e:
        print(f"Transaction aborted: {e}")

    # Verify atomic rollback
    cursor.execute("SELECT name, balance FROM bank_accounts")
    print("Balances post-abort:", cursor.fetchall())
    conn.close()

run_transaction_demo()
# Output:
# Transaction aborted: Insufficient funds in account: Alice
# Balances post-abort: [('Alice', 500), ('Bob', 200)]
```

---

## Edge Cases, Pitfalls & Anti-Patterns

### 1. SQL Injection via Formatted Strings
Never use f-strings or `.format()` to inject variables into SQL statements. Always use `?` (positional) or `:param` (named) syntax. SQLite's statement compiler binds parameters safely through `sqlite3_bind_*` C calls, completely isolating data from SQL parsing tokens.

### 2. Thread Safety and `check_same_thread=False`
By default, `sqlite3.connect` sets `check_same_thread=True`. Sharing a connection object between multiple threads can lead to memory corruption in CPython unless serialized. If using connection pooling across worker threads, each thread should open its own discrete connection, or set `check_same_thread=False` accompanied by explicit threading locks (`threading.Lock`).

### 3. Database Locked (`sqlite3.OperationalError: database is locked`)
Occurs when multiple processes or threads attempt to write concurrently under default rollback journal mode. Mitigate by:
1. Setting `PRAGMA journal_mode = WAL;`.
2. Increasing connection timeout: `sqlite3.connect("data.db", timeout=30.0)`.
3. Keeping transactions short and never holding them open during network calls or user interaction.

### Common Mistakes Table

| Bad Code | Good Code | Why | Severity |
| :--- | :--- | :--- | :--- |
| `cursor.execute(f"SELECT * FROM u WHERE id={uid}")` | `cursor.execute("SELECT * FROM u WHERE id=?", (uid,))` | Vulnerable to SQL injection; bypasses statement cache. | Critical |
| `cursor.execute("INSERT ... (val)", (single_val))` | `cursor.execute("INSERT ... (val)", (single_val,))` | `(single_val)` without trailing comma is an expression, not a tuple; unpacks string into chars. | High |
| Omitting `conn.commit()` after manual DML | `conn.commit()` or `with conn:` | Changes remain uncommitted in memory and are lost when process exits. | High |
| Sharing single `conn` across 5 worker threads without locks | Thread-local connections or dedicated worker queue | Causes race conditions in SQLite's C internal parser and memory structures. | High |
| Using default rollback journal in concurrent environments | `conn.execute("PRAGMA journal_mode = WAL")` | High concurrency causes constant `sqlite3.OperationalError: database is locked`. | Medium |
| Calling `cursor.fetchall()` on a 10M row table | Iterate directly: `for row in cursor.execute(...):` | `fetchall()` allocates Python objects for all 10 million rows simultaneously in RAM. | Medium |

---

## Performance & Memory Characteristics

### Computational & Memory Complexity
- **Primary Key Lookups**: $\mathcal{O}(\log N)$ via balanced B-tree page traversals.
- **Unindexed Scans**: $\mathcal{O}(N)$ sequential table scans traversing all database pages.
- **Streaming Result Sets**: Directly iterating `for row in cursor:` consumes $\mathcal{O}(1)$ memory buffer space, pulling pages on-demand from the OS file cache.

### Optimizing Write Throughput: `PRAGMA synchronous`
In default configuration, SQLite issues an `fsync()` system call on every transaction commit, flushing disk write caches to physical media. On spinning platters or consumer SSDs, this caps throughput at 50–100 commits/sec.

```python
# Tuning PRAGMA options for bulk ingestion
conn.execute("PRAGMA journal_mode = WAL;")        # Concurrent readers and writers
conn.execute("PRAGMA synchronous = NORMAL;")       # 10x-50x faster writes; fully safe in WAL mode
conn.execute("PRAGMA cache_size = -64000;")        # 64MB memory page cache (negative = KB)
conn.execute("PRAGMA temp_store = MEMORY;")        # Store indices and temp tables in RAM
```

---

## CPython Internals & Bytecode

### C Source Architecture
In CPython (`Modules/_sqlite/`), the `sqlite3` module bridges Python to the external SQLite3 C library:
- `connection.c` defines `pysqlite_Connection` wrapping the C pointer `sqlite3* db`.
- `cursor.c` defines `pysqlite_Cursor` wrapping `sqlite3_stmt* statement`.
- When `execute()` is called, CPython inspects its internal statement cache (a least-recently-used hash map). If the query string matches a cached statement, it skips `sqlite3_prepare_v2()` and calls `sqlite3_reset()`, saving CPU cycles.

```
Python bytecode for simple execute call:
```
```python
import dis
import sqlite3

def test_query(conn):
    return conn.execute("SELECT 1").fetchone()

dis.dis(test_query)
```
```text
  1           0 RESUME                   0
  2           2 LOAD_FAST                0 (conn)
              4 LOAD_ATTR                1 (execute)
             14 LOAD_CONST               1 ('SELECT 1')
             16 CALL                     1
             24 LOAD_ATTR                3 (fetchone)
             34 CALL                     0
             42 RETURN_VALUE
```
Notice that execution and row extraction are native method calls delegating directly to C extension boundaries without intermediate Python-level bytecode interpretation for row looping.

---

## Best Practices & Idiomatic Patterns

1. **Always Use Context Managers for Transactions**:
   ```python
   with conn:
       conn.execute("UPDATE account SET balance = balance - ? WHERE id = ?", (100, 1))
   ```
2. **Close Connections Promptly**:
   Use `contextlib.closing` or explicitly close connections in `finally` blocks to release file locks.
3. **Foreign Key Enforcement**:
   SQLite disables foreign key constraint checking by default for backwards compatibility. Always enable it explicitly upon opening a connection:
   ```python
   conn.execute("PRAGMA foreign_keys = ON;")
   ```
4. **Use URI Filenames for Read-Only Replicas**:
   ```python
   conn = sqlite3.connect("file:replica.db?mode=ro", uri=True)
   ```

---

## Real-World Case Study: Resilient WAL-Mode Ingestion Queue

Below is a production-grade implementation of an embedded durable task queue with WAL concurrency, foreign key integrity, and batch ingestion.

```python
import sqlite3
from pathlib import Path
from typing import Optional, List, Tuple
import contextlib

class DurableTaskQueue:
    def __init__(self, db_path: Path):
        self.db_path = db_path
        self._init_db()

    def _get_connection(self) -> sqlite3.Connection:
        conn = sqlite3.connect(
            self.db_path,
            timeout=30.0,
            check_same_thread=False
        )
        conn.row_factory = sqlite3.Row
        # Critical production PRAGMAs
        conn.execute("PRAGMA journal_mode = WAL;")
        conn.execute("PRAGMA synchronous = NORMAL;")
        conn.execute("PRAGMA foreign_keys = ON;")
        conn.execute("PRAGMA busy_timeout = 5000;")
        return conn

    def _init_db(self) -> None:
        with contextlib.closing(self._get_connection()) as conn:
            with conn:
                conn.execute("""
                    CREATE TABLE IF NOT EXISTS queues (
                        queue_id INTEGER PRIMARY KEY AUTOINCREMENT,
                        name TEXT UNIQUE NOT NULL
                    );
                """)
                conn.execute("""
                    CREATE TABLE IF NOT EXISTS tasks (
                        task_id INTEGER PRIMARY KEY AUTOINCREMENT,
                        queue_id INTEGER NOT NULL,
                        payload TEXT NOT NULL,
                        status TEXT CHECK(status IN ('pending', 'running', 'completed', 'failed')) DEFAULT 'pending',
                        retry_count INTEGER DEFAULT 0,
                        created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                        FOREIGN KEY (queue_id) REFERENCES queues (queue_id) ON DELETE CASCADE
                    );
                """)
                conn.execute("CREATE INDEX IF NOT EXISTS idx_task_fetch ON tasks(queue_id, status);")

    def enqueue_batch(self, queue_name: str, payloads: List[str]) -> int:
        with contextlib.closing(self._get_connection()) as conn:
            with conn:
                # Upsert queue
                conn.execute("INSERT OR IGNORE INTO queues (name) VALUES (?)", (queue_name,))
                cursor = conn.execute("SELECT queue_id FROM queues WHERE name = ?", (queue_name,))
                queue_id = cursor.fetchone()["queue_id"]

                # Batch insert tasks
                records = [(queue_id, p) for p in payloads]
                conn.executemany("INSERT INTO tasks (queue_id, payload) VALUES (?, ?)", records)
                return len(records)

    def claim_task(self, queue_name: str) -> Optional[Tuple[int, str]]:
        with contextlib.closing(self._get_connection()) as conn:
            with conn:
                cursor = conn.execute("""
                    SELECT task_id, payload FROM tasks
                    JOIN queues ON tasks.queue_id = queues.queue_id
                    WHERE queues.name = ? AND tasks.status = 'pending'
                    ORDER BY task_id ASC
                    LIMIT 1
                """, (queue_name,))
                row = cursor.fetchone()
                if not row:
                    return None
                
                task_id = row["task_id"]
                conn.execute("UPDATE tasks SET status = 'running' WHERE task_id = ?", (task_id,))
                return (task_id, row["payload"])

# Verification
if __name__ == "__main__":
    import tempfile
    with tempfile.TemporaryDirectory() as tmpdir:
        db_file = Path(tmpdir) / "tasks.db"
        queue = DurableTaskQueue(db_file)
        count = queue.enqueue_batch("email_jobs", ["user_1@test.com", "user_2@test.com"])
        print(f"Enqueued {count} jobs")
        
        task = queue.claim_task("email_jobs")
        print(f"Claimed Task -> ID: {task[0]}, Payload: {task[1]}")
```

---

## Interview Questions & Deep-Dive Scenarios

<details>
<summary><strong>1. Junior: Why should you always use parameterized queries instead of Python f-strings in SQLite?</strong></summary>

Parameterized queries (`cursor.execute("SELECT * FROM users WHERE name = ?", (name,))`) prevent SQL injection attacks. String formatting directly concatenates untrusted user input into the SQL statement, allowing attackers to manipulate query logic (e.g. injecting `' OR '1'='1`). Parameter binding also allows SQLite to cache compiled statement execution plans across calls, improving execution speed.
</details>

<details>
<summary><strong>2. Junior: What is the difference between <code>fetchone()</code>, <code>fetchmany()</code>, and <code>fetchall()</code>?</strong></summary>

- `fetchone()` retrieves the next single row from the cursor's result set, returning a tuple (or row object) or `None` if no rows remain.
- `fetchmany(size)` retrieves the next batch of rows as a list, defaulting to `cursor.arraysize`.
- `fetchall()` retrieves all remaining rows into memory at once as a list. For large result sets, `fetchall()` can exhaust system memory.
</details>

<details>
<summary><strong>3. Mid: What is WAL mode in SQLite, and how does it affect read/write concurrency?</strong></summary>

In standard rollback journal mode, writing locks the entire database file, preventing any readers from reading until the transaction finishes. In Write-Ahead Logging (WAL) mode (`PRAGMA journal_mode = WAL;`), modifications are appended to a separate `-wal` file. Readers read undisturbed pages from the original database file while writers append to the log. As a result, readers never block writers, and writers never block readers.
</details>

<details>
<summary><strong>4. Mid: Explain the difference between <code>with connection:</code> and <code>with cursor:</code> in Python's sqlite3 module.</strong></summary>

`with connection:` acts as a **transaction context manager**: if the block exits normally, it calls `connection.commit()`; if an exception is raised, it automatically calls `connection.rollback()`. It does **not** close the connection. Conversely, `with cursor:` handles cursor lifecycle management, closing the cursor upon block exit, but does not commit or roll back any transactions.
</details>

<details>
<summary><strong>5. Senior: What changed regarding transaction management and autocommit in Python 3.12 (PEP 690)?</strong></summary>

Prior to Python 3.12, the `sqlite3` module inspected SQL queries using regex and magic heuristics: it automatically issued `BEGIN` before DML statements (`INSERT`, `UPDATE`, `DELETE`) and committed before DDL (`CREATE`, `DROP`). Python 3.12 introduced an explicit `autocommit` parameter to `sqlite3.connect()`. Setting `autocommit=False` adheres strictly to PEP 249 (manual commit requirements), while `autocommit=True` operates in SQLite's native autocommit mode, eliminating non-standard statement inspection overhead.
</details>

<details>
<summary><strong>6. Senior: How does <code>sqlite3.Row</code> achieve both index-based and name-based column lookup with minimal memory overhead?</strong></summary>

`sqlite3.Row` is a lightweight C extension type that holds a reference to the raw tuple of row values and a shared mapping dictionary from the cursor's description (`col_name -> index`). Index lookups (`row[0]`) index directly into the underlying tuple in $\mathcal{O}(1)$. Key lookups (`row["name"]`) perform a hash lookup on the shared column dictionary to retrieve the index, then index the tuple. Because the column-to-index mapping is shared across all rows produced by the same query, memory footprint is minimal compared to storing full dictionaries per row.
</details>

<details>
<summary><strong>7. Staff: How would you architect high-concurrency writes in a multi-threaded Python service backed by SQLite?</strong></summary>

1. Set `PRAGMA journal_mode = WAL;` and `PRAGMA synchronous = NORMAL;`.
2. Configure a sensible `busy_timeout` (e.g. 5000ms) on connections.
3. Designate a dedicated single background thread with a thread-safe `queue.Queue` to serialize all write transactions via a single persistent connection.
4. Allow all worker threads to open independent, read-only connections (`file:db.sqlite?mode=ro`, `uri=True`) to perform non-blocking parallel queries directly against the WAL-enabled database.
</details>

<details>
<summary><strong>8. Staff: How does SQLite maintain ACID durability if power is lost midway through a WAL checkpoint?</strong></summary>

The WAL file contains a header with a 32-bit salt and checksums calculated over every committed frame. When checkpointing copies frames from the WAL back into the database file, it updates database pages in place. If power cuts out mid-checkpoint, the main DB pages may be inconsistent, but the WAL file remains intact with valid checksums. Upon recovery, SQLite examines the WAL header; if salt and checksums match, it replays the WAL from the beginning, restoring complete transactional consistency without data loss.
</details>

---

## Practical Exercises & Code Katas

### Exercise 1: Implement a Key-Value Store on SQLite
**Requirement:** Create an `SQLiteKV` class supporting `get(key, default=None)`, `set(key, val)`, and `delete(key)` with automatic JSON serialization of values.

<details>
<summary>Show Solution</summary>

```python
import sqlite3
import json
from typing import Any, Optional

class SQLiteKV:
    def __init__(self, db_path: str = ":memory:"):
        self.conn = sqlite3.connect(db_path)
        with self.conn:
            self.conn.execute("""
                CREATE TABLE IF NOT EXISTS kv_store (
                    k TEXT PRIMARY KEY,
                    v TEXT NOT NULL
                );
            """)

    def set(self, key: str, val: Any) -> None:
        serialized = json.dumps(val)
        with self.conn:
            self.conn.execute(
                "INSERT INTO kv_store (k, v) VALUES (?, ?) ON CONFLICT(k) DO UPDATE SET v = excluded.v",
                (key, serialized)
            )

    def get(self, key: str, default: Optional[Any] = None) -> Any:
        cursor = self.conn.execute("SELECT v FROM kv_store WHERE k = ?", (key,))
        row = cursor.fetchone()
        return json.loads(row[0]) if row else default

    def delete(self, key: str) -> bool:
        with self.conn:
            cursor = self.conn.execute("DELETE FROM kv_store WHERE k = ?", (key,))
            return cursor.rowcount > 0

    def close(self):
        self.conn.close()

# Test
kv = SQLiteKV()
kv.set("user:101", {"name": "Alice", "role": "admin"})
print(kv.get("user:101"))
print("Deleted:", kv.delete("user:101"))
print("Default lookup:", kv.get("user:101", "missing"))
kv.close()
```
</details>

### Exercise 2: Custom Aggregator Function
**Requirement:** Use `sqlite3.Connection.create_aggregate()` to calculate the geometric mean of a column directly inside SQL queries.

<details>
<summary>Show Solution</summary>

```python
import sqlite3
import math

class GeometricMean:
    def __init__(self):
        self.log_sum = 0.0
        self.count = 0

    def step(self, value):
        if value is not None and value > 0:
            self.log_sum += math.log(value)
            self.count += 1

    def finalize(self):
        if self.count == 0:
            return None
        return math.exp(self.log_sum / self.count)

conn = sqlite3.connect(":memory:")
conn.create_aggregate("geo_mean", 1, GeometricMean)

conn.execute("CREATE TABLE returns (val REAL);")
conn.executemany("INSERT INTO returns VALUES (?)", [(1.1,), (1.2,), (1.05,)])

cursor = conn.execute("SELECT geo_mean(val) FROM returns")
print("Geometric Mean:", round(cursor.fetchone()[0], 4))
conn.close()
```
</details>

### Exercise 3: Safe Bulk Upsert
**Requirement:** Use SQLite 3.24+ `INSERT ... ON CONFLICT DO UPDATE` syntax with `executemany()` to update inventory counts in bulk without losing existing product descriptions.

<details>
<summary>Show Solution</summary>

```python
import sqlite3

conn = sqlite3.connect(":memory:")
with conn:
    conn.execute("""
        CREATE TABLE inventory (
            sku TEXT PRIMARY KEY,
            stock INTEGER NOT NULL,
            description TEXT NOT NULL
        );
    """)
    conn.execute("INSERT INTO inventory VALUES ('A1', 10, 'Widgets'), ('B2', 5, 'Gadgets')")

updates = [
    ("A1", 15, "Widgets"),       # Existing: update stock
    ("C3", 20, "Sprockets")      # New: insert full row
]

with conn:
    conn.executemany("""
        INSERT INTO inventory (sku, stock, description)
        VALUES (?, ?, ?)
        ON CONFLICT(sku) DO UPDATE SET
            stock = excluded.stock;
    """, updates)

cursor = conn.execute("SELECT * FROM inventory ORDER BY sku")
for row in cursor.fetchall():
    print(row)
conn.close()
```
</details>

### Exercise 4: Streaming Large CSV to SQLite with Minimal Memory
**Requirement:** Stream 500,000 simulated rows into an indexed SQLite table using a generator pipeline without holding more than 1,000 rows in Python memory at any time.

<details>
<summary>Show Solution</summary>

```python
import sqlite3
import itertools

def row_generator(total: int):
    for i in range(total):
        yield (f"user_{i}", f"user_{i}@company.com", i % 100)

def chunked_iterable(iterable, size):
    it = iter(iterable)
    while batch := list(itertools.islice(it, size)):
        yield batch

conn = sqlite3.connect(":memory:")
with conn:
    conn.execute("CREATE TABLE users (username TEXT, email TEXT, dept_id INTEGER);")

batch_size = 1000
for batch in chunked_iterable(row_generator(5000), batch_size):
    with conn:
        conn.executemany("INSERT INTO users VALUES (?, ?, ?)", batch)

cursor = conn.execute("SELECT count(*) FROM users")
print("Total rows streamed:", cursor.fetchone()[0])
conn.close()
```
</details>

### Exercise 5: Transaction Savepoints and Partial Rollbacks
**Requirement:** Implement a nested transaction using SQLite savepoints (`SAVEPOINT sp_name` and `ROLLBACK TO sp_name`) allowing a batch operation to roll back a specific failing item without aborting the parent transaction.

<details>
<summary>Show Solution</summary>

```python
import sqlite3

conn = sqlite3.connect(":memory:")
cursor = conn.cursor()
cursor.execute("CREATE TABLE audit_log (id INTEGER PRIMARY KEY, msg TEXT);")

with conn:
    cursor.execute("INSERT INTO audit_log (msg) VALUES ('Batch Started')")
    
    # Create Savepoint
    cursor.execute("SAVEPOINT sub_step")
    cursor.execute("INSERT INTO audit_log (msg) VALUES ('Sub-step 1 Success')")
    cursor.execute("INSERT INTO audit_log (msg) VALUES ('Sub-step 2 Failing')")
    
    # Roll back only to the savepoint
    cursor.execute("ROLLBACK TO sub_step")
    cursor.execute("RELEASE sub_step")
    
    cursor.execute("INSERT INTO audit_log (msg) VALUES ('Batch Completed Gracefully')")

cursor.execute("SELECT msg FROM audit_log")
print([r[0] for r in cursor.fetchall()])
conn.close()
```
</details>

### Exercise 6: User-Defined Collation Sequence
**Requirement:** Register a custom collation sequence with `create_collation()` that sorts version strings (e.g. "v1.2.10", "v1.2.2") according to semantic versioning rules rather than standard lexicographical order.

<details>
<summary>Show Solution</summary>

```python
import sqlite3
import re

def semver_compare(v1: str, v2: str) -> int:
    def parse(s):
        return [int(x) for x in re.sub(r'[^0-9.]', '', s).split('.')]
    
    p1, p2 = parse(v1), parse(v2)
    if p1 < p2:
        return -1
    elif p1 > p2:
        return 1
    return 0

conn = sqlite3.connect(":memory:")
conn.create_collation("SEMVER", semver_compare)

conn.execute("CREATE TABLE releases (version TEXT);")
conn.executemany("INSERT INTO releases VALUES (?)", [("v1.2.2",), ("v1.10.0",), ("v1.2.10",), ("v1.1.0",)])

cursor = conn.execute("SELECT version FROM releases ORDER BY version COLLATE SEMVER ASC")
print("Sorted Releases:", [r[0] for r in cursor.fetchall()])
conn.close()
```
</details>
