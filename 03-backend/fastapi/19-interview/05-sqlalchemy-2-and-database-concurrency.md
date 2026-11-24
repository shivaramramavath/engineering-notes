# Technical Interview Mastery: SQLAlchemy 2.0 Async & Database Concurrency

## Overview & Evaluation Criteria

SQLAlchemy 2.0 fundamentally modernized database access in Python, mandating `select()` constructs and introducing async greenlet engines. Interviewers look for mastery over async connection pooling, transaction isolation levels, row locking, and migration safety.

---

## Technical Q&A Deep Dive

<details>
<summary><strong>Q1: What is the purpose of the <code>greenlet</code> library inside SQLAlchemy 2.0 Async? Why is it required?</strong></summary>

### Answer:
- SQLAlchemy's core architecture was originally developed as a deeply synchronous ORM with recursive instrumentation.
- Rather than rewriting the entire million-line codebase from scratch, SQLAlchemy's `AsyncEngine` uses `greenlet` (C-level cooperative micro-threads) to suspend synchronous internal ORM calls and resume them when the underlying asyncio socket I/O completes.
- **The "MissingGreenlet" Error**:
  - Occurs when code triggers lazy loading of an un-loaded relationship (e.g. `user.orders`) outside of an active greenlet-wrapped async context.
  - **Resolution**: Use explicit eager loading (`selectinload` or `joinedload`) or async query joins.

</details>

<details>
<summary><strong>Q2: How does connection pooling work in SQLAlchemy Async with asyncpg? What happens when the pool is exhausted?</strong></summary>

### Answer:
- `create_async_engine(url, pool_size=20, max_overflow=10, pool_timeout=30)`:
  - Maintains up to 20 persistent, reusable PostgreSQL TCP sockets.
  - If all 20 are active and a 21st request arrives, it creates up to 10 additional temporary "overflow" connections (total 30).
  - If all 30 connections are busy and a 31st request arrives, the caller awaits for up to `pool_timeout=30` seconds.
  - If no connection frees up within 30 seconds, a `TimeoutError / QueuePoolLimitExceeded` is raised, returning HTTP 500/503.
- **Production Tip**: Always set `pool_pre_ping=True` to eliminate stale connections terminated by firewall/NAT timeouts.

</details>

<details>
<summary><strong>Q3: Compare Optimistic Concurrency Control (OCC) vs. Pessimistic Concurrency Control in high-contention inventory updates.</strong></summary>

### Answer:
- **Pessimistic Locking (`SELECT ... FOR UPDATE`)**:
  - Locks the database row at the storage engine level until the transaction commits or rolls back.
  - Guarantees serialized access; subsequent concurrent queries on that row block.
  - **Best For**: High contention, critical financial/inventory deductions where conflicts are expected and waiting is preferred over retrying.
- **Optimistic Locking (Version Column)**:
  - Does not hold row locks. Reads current version `N`, updates with `WHERE id = :id AND version = N`, setting `version = N + 1`.
  - If 0 rows are updated, another transaction updated it first; the application detects the conflict and raises HTTP 409 or retries.
  - **Best For**: Low-to-moderate contention, long user think-times (e.g., editing a CMS article for 10 minutes).

</details>

<details>
<summary><strong>Q4: Why should Alembic migrations never execute destructive DDL (like dropping columns or adding non-null columns without defaults) in a single deployment?</strong></summary>

### Answer:
- In production, zero-downtime rolling deployments run Old API pods and New API pods concurrently for several minutes.
- If a migration immediately drops a column `v1` that Old Pods are actively reading, Old Pods will crash with HTTP 500 errors.
- **Expand / Contract (Parallel Run) Pattern**:
  1. **Phase 1 (Expand)**: Add the new column as nullable or with default. Deploy New Pods that read from both or write to both.
  2. **Phase 2 (Migrate Data)**: Backfill historical data in background chunks.
  3. **Phase 3 (Contract)**: Update all pods to use only the new column.
  4. **Phase 4 (Cleanup)**: Drop the old column in a final migration after 100% of old pods are retired.
</details>
