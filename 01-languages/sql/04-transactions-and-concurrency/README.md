# Module 04: Transactions, Concurrency & Locking

## Overview

In a single-user database, state transitions are trivial. In production enterprise systems, thousands of concurrent client sessions execute reads, writes, schema migrations, and batch operations against the same tables, rows, and indexes simultaneously.

**Transactions, Concurrency Control, and Recovery** form the foundational bedrock that allows relational databases to guarantee data integrity in the face of concurrent operations and sudden hardware or software crashes. This module provides a rigorous, deep dive into:
- The exact physical and theoretical definitions of the **ACID** properties (Atomicity, Consistency, Isolation, Durability).
- The transaction lifecycle: explicit vs implicit transaction demarcation, commit protocols, and savepoints.
- ANSI SQL:1992 transaction isolation levels versus physical reality, including anomalous phenomena (Dirty Read, Non-Repeatable Read, Phantom Read, Read Skew, Write Skew).
- **Multi-Version Concurrency Control (MVCC)**: tuple versioning (`xmin`, `xmax`, undo logs vs append-only heaps), snapshot isolation, and read-view mechanics.
- Two-Phase Locking (2PL), lock modes, row vs table locking trees, and deadlock detection algorithms.
- Explicit concurrency control: `SELECT FOR UPDATE`, `FOR NO KEY UPDATE`, `SKIP LOCKED`, `NOWAIT`, and building high-throughput transactional job queues.
- Optimistic Concurrency Control (OCC) vs Pessimistic Locking patterns in high-contention enterprise systems.
- Concurrency anti-patterns, the devastating impact of long-running idle transactions on table/index bloat, and the limits of two-phase commit (2PC) in distributed databases.

---

## Module Syllabus

| Number | Note | Core Concepts Covered | Key Engine Mechanics |
|:---|:---|:---|:---|
| **01** | [ACID Properties Deep Dive](01-acid.md) | Atomicity, Consistency, Isolation, Durability | WAL (`pg_wal` / `ib_logfile`), dirty page flushing, `fsync`, invariant checks |
| **02** | [Transaction Control](02-transaction-control.md) | `BEGIN`, `COMMIT`, `ROLLBACK`, `SAVEPOINT` | Transaction IDs (`txid`), nested transactions myth, subtransactions (`SubTrans`), rollbacks |
| **03** | [Isolation Levels & Anomalies](03-isolation-levels-and-anomalies.md) | Read Committed, Repeatable Read, Serializable | Dirty Read, Fuzzy Read, Phantom Read, Serialization Anomaly, Write Skew |
| **04** | [MVCC Explained](04-mvcc-explained.md) | Multi-Version Concurrency Control | `xmin`/`xmax`, tuple header flags, InnoDB Undo Segments vs Postgres dead tuples |
| **05** | [Locking Mechanisms](05-locking-mechanisms.md) | Shared, Exclusive, Intent locks, Lock Queues | Heavyweight table locks, row locks, lock manager hash table, queue starvation |
| **06** | [Deadlocks & Diagnosis](06-deadlocks-and-diagnosis.md) | Wait-For Graphs, Cycle Detection | `deadlock_timeout`, directed cycles, Lock Ordering discipline, `pg_stat_activity` |
| **07** | [Optimistic vs Pessimistic Concurrency](07-optimistic-vs-pessimistic-concurrency.md) | OCC vs 2PL, Version columns, CAS | Lost update mitigation, retry loops, contention tipping points, reservation tickets |
| **08** | [SELECT FOR UPDATE & Job Queues](08-select-for-update-skip-locked-and-queues.md) | Lock strengths, `SKIP LOCKED`, `NOWAIT` | Lock-free worker dequeue, `FOR NO KEY UPDATE`, transactional outbox pattern |
| **09** | [Idempotency & Retries](09-idempotency-and-retries.md) | Idempotency keys, exponential backoff, jitter | Transient failure classification, duplicate request deduplication, distributed replay |
| **10** | [Long Transactions & Bloat](10-long-transactions-and-bloat.md) | `idle in transaction`, snapshot holding | Vacuum freeze blocking, table/index bloat, transaction ID wraparound emergency |
| **11** | [Distributed Transactions Overview](11-distributed-transactions-overview.md) | 2PC (`PREPARE TRANSACTION`), Saga, Outbox | Coordinator failure, dual-write hazard, consensus vs atomic commitment, CDC |

---

## Architecture of a Transactional Database Engine

```
+-----------------------------------------------------------------------------------------+
|                                    CLIENT APPLICATION                                    |
+-----------------------------------------------------------------------------------------+
                                             |  SQL Statements (BEGIN / DML / COMMIT)
                                             v
+-----------------------------------------------------------------------------------------+
|                                     ENGINE CORE                                         |
|                                                                                         |
|  +---------------------------+       +-----------------------------------------------+  |
|  |       LOCK MANAGER        |       |            MVCC & SNAPSHOT MANAGER            |  |
|  | - Heavyweight Lock Table  |       | - Active Transaction List (`SnapshotData`)    |  |
|  | - Row Lock Markers        |       | - Visibility Checking Rules (`xmin` / `xmax`) |  |
|  | - Wait-For Graph (Cycle)  |       | - Undo Log Pointers (InnoDB / Oracle)         |  |
|  +---------------------------+       +-----------------------------------------------+  |
|                |                                              |                         |
|                v                                              v                         |
|  +-----------------------------------------------------------------------------------+  |
|  |                       BUFFER POOL / SHARED BUFFERS                                |  |
|  |  [ Page 1 (Clean) ]  [ Page 2 (Dirty) ]  [ Page 3 (Dirty) ]  [ Page 4 (Clean) ]   |  |
|  +-----------------------------------------------------------------------------------+  |
|            |                                              |                             |
|            | 1. WAL Log Record written BEFORE data flush   | 2. Checkpointer flushes     |
|            v                                              v    dirty pages in background|
|  +-----------------------------------+        +--------------------------------------+  |
|  |        WAL / REDO LOG BUFFER      |        |              DATA FILES              |  |
|  |    (Sequential fsync on COMMIT)   |        |             (Random I/O)             |  |
|  +-----------------------------------+        +--------------------------------------+  |
+-----------------------------------------------------------------------------------------+
```

---

## Dialect Support Matrix: Concurrency & Transaction Primitives

| Feature | PostgreSQL 17+ | MySQL 8.4 LTS (InnoDB) | SQLite 3.50+ | SQL Server 2022 |
|:---|:---|:---|:---|:---|
| **Default Isolation Level** | `READ COMMITTED` | `REPEATABLE READ` | `SERIALIZABLE` (file lock) | `READ COMMITTED` (Lock-based) |
| **MVCC Implementation** | Heap-based (`xmin`/`xmax`, dead tuples cleaned by VACUUM) | Undo Log-based (Rollback segments in ibdata/undo tablespaces) | WAL-based (1 writer, N readers; whole DB level) | Row-versioning in `tempdb` (RCSI / Snapshot) |
| **True Serializable** | SSI (Serializable Snapshot Isolation - no locking) | 2PL (Locks every row & gap via shared locks) | Single-writer serialization | 2PL (Range locks on index key ranges) |
| **`SELECT ... FOR UPDATE`** | Supported (`FOR UPDATE`, `FOR NO KEY UPDATE`, etc.) | Supported (`FOR UPDATE`, `FOR SHARE`) | Not supported (locks entire database upon write) | Supported via table hints (`WITH (UPDLOCK, ROWLOCK)`) |
| **`SKIP LOCKED`** | Supported | Supported | Not supported | Supported (`WITH (READPAST)`) |
| **Nested Transactions** | Emulated via `SAVEPOINT` (No true nesting) | Emulated via `SAVEPOINT` (`BEGIN` auto-commits outer!) | Emulated via `SAVEPOINT` | Named transactions (Syntax only, flat commit!) |
| **Distributed 2PC** | `PREPARE TRANSACTION` / `COMMIT PREPARED` | `XA PREPARE` / `XA COMMIT` | Not supported | MS DTC / Two-Phase Commit |

---

## Recommended Learning Order

1. **Foundations**: Start with **01-acid.md** and **02-transaction-control.md** to understand the mathematical requirements of transactions and their control syntax.
2. **Isolation & Concurrency Engine**: Study **03-isolation-levels-and-anomalies.md** and **04-mvcc-explained.md** side-by-side to master how snapshots eliminate read locks while preventing anomalies.
3. **Pessimistic Control & Deadlocks**: Work through **05-locking.md**, **06-deadlocks-and-diagnosis.md**, and **08-select-for-update-skip-locked-and-queues.md** to write battle-tested transactional code.
4. **Resilience & Systems Engineering**: Conclude with **07-optimistic-vs-pessimistic-concurrency.md**, **09-idempotency-and-retries.md**, **10-long-transactions-and-bloat.md**, and **11-distributed-transactions-overview.md**.

Cross-reference:
- [04-devops/linux/03-system/05-storage-and-filesystems.md](../../04-devops/linux/03-system/05-storage-and-filesystems.md) for OS page caches, direct I/O, and `fsync()`.
- [07-system-design/03-data-storage/](../../07-system-design/) for distributed consensus (Raft/Paxos) and CAP/PACELC tradeoffs.
