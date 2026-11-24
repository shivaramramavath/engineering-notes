---
title: "Common FastAPI Database Mistakes & Debugging"
slug: "common-database-mistakes"
description: "Comprehensive guide to debugging and preventing the most dangerous database pitfalls in FastAPI: MissingGreenlet, connection leaks, deadlocks, and migration traps."
category: "06-databases"
tags: ["fastapi", "sqlalchemy", "debugging", "pitfalls", "anti-patterns", "database", "concurrency"]
difficulty: "Advanced"
date: "2026-03-30"
fastapi_version: "0.165.x"
starlette_version: "1.10.x"
pydantic_version: "2.14.x"
python_version: "3.12+"
tested_with: "Python 3.12, 3.13, 3.14t"
free_threading_support: "Fully Compatible"
---

# Common FastAPI Database Mistakes & Debugging

Asynchronous database programming introduces a paradigm shift. Subtly misconfigured session lifecycles, unhandled greenlet exceptions, synchronous driver usage on the event loop, unindexed foreign keys, and silent migration data drops can cause catastrophic production failures—including application freezes, connection pool exhaustion, and data loss.

---

## Conceptual Foundation & Mental Model

Database errors in async FastAPI stem from violating one of five core architectural boundaries:
1. **The Concurrency Boundary**: Executing synchronous database operations (`psycopg2`, sync SQLAlchemy) inside `async def` endpoints blocks the entire event loop.
2. **The Attribute Access Boundary**: Relying on lazy loading (`user.orders`) triggers `MissingGreenlet` because Python attribute access cannot be awaited.
3. **The Lifecycle Boundary**: Passing request-scoped database sessions into `BackgroundTasks` where they are accessed *after* the session has already closed.
4. **The Transaction Boundary**: Locking resources in inconsistent order across concurrent transactions, causing database deadlocks.
5. **The Schema Evolution Boundary**: Blindly running `alembic revision --autogenerate` on column renames, resulting in accidental `DROP COLUMN` data wipes.

```
Common Database Traps:
1. sync_driver in async def  ──► Blocks Event Loop  ──► Throughput drops to 0
2. user.orders (lazy load)   ──► MissingGreenlet   ──► HTTP 500 Crash
3. db passed to BackgroundTask ──► Session Closed    ──► InterfaceError
4. Lock A then B vs B then A ──► Deadlock 40P01     ──► Transaction Aborted
5. autogenerate rename       ──► DROP COLUMN        ──► Data Wiped!
```

---

## Architecture & Internal Mechanics

The table below catalogs the most dangerous database mistakes, their technical signatures, and their immediate production remedies:

| Mistake | Root Cause | Observable Symptom | Production Remediation |
| :--- | :--- | :--- | :--- |
| **`MissingGreenlet`** | Lazy loading relationship in async context | `sqlalchemy.exc.MissingGreenlet: greenlet_spawn has not been called` | Use `selectinload()` or `joinedload()`; set `lazy="raise"`. |
| **Connection Leak** | Missing `try ... finally: session.close()` | `TimeoutError: QueuePool limit of size 20 overflow 10 reached` | Use `async with async_session_factory() as session:`. |
| **Sync DB in Async Route** | Using synchronous `psycopg2` | Latency spikes to seconds under 10 concurrent users; CPU drops to 0% | Use `postgresql+asyncpg` or `postgresql+psycopg` (v3). |
| **Background Task Session Crash** | Session closed before background task runs | `sqlalchemy.exc.InvalidRequestError: Session is closed` | Background task must create its own independent session. |
| **Deadlock on Concurrent Updates** | Transactions lock rows in differing order | `asyncpg.exceptions.DeadlockDetectedError: deadlock detected (40P01)` | Sort IDs before locking (`ORDER BY id` / `sorted(ids)`). |

---

## Practical Implementation (5 Complete Bug & Fix Examples)

### Example 1: The MissingGreenlet Error (Bug & Production Fix)
```python
# ❌ BUG: Attempting to serialize lazy-loaded relationship in async!
from fastapi import FastAPI, Depends
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy import select

app = FastAPI()

# class User(Base):
#     orders: Mapped[list["Order"]] = relationship() # Defaults to lazy="select"

# @app.get("/users/{user_id}", response_model=UserReadWithOrders)
# async def get_user(user_id: int, db: AsyncSession = Depends(get_db)):
#     user = (await db.execute(select(User).where(User.id == user_id))).scalar_one()
#     # Pydantic accesses user.orders -> CRASH: MissingGreenlet!
#     return user

# ✅ FIX: Eager load with selectinload and set lazy="raise" on model!
from sqlalchemy.orm import selectinload

@app.get("/users/{user_id}", response_model=UserReadWithOrders)
async def get_user_fixed(user_id: int, db: AsyncSession = Depends(get_db)):
    query = (
        select(User)
        .options(selectinload(User.orders))
        .where(User.id == user_id)
    )
    result = await db.execute(query)
    user = result.scalar_one_or_none()
    return user
```

### Example 2: The Connection Pool Exhaustion Trap (Bug & Fix)
```python
# ❌ BUG: Leaking connections by failing to close sessions on errors!
# async def bad_get_db():
#     session = session_factory()
#     # If an exception occurs in the endpoint, session.close() is NEVER called!
#     yield session
#     await session.close()

# ✅ FIX: Always wrap session lifecycle in an async context manager!
from typing import AsyncGenerator
from sqlalchemy.ext.asyncio import AsyncSession

async def safe_get_db() -> AsyncGenerator[AsyncSession, None]:
    async with session_factory() as session:
        try:
            yield session
            await session.commit()
        except Exception:
            await session.rollback()
            raise
        finally:
            await session.close() # Guaranteed closure even on client disconnects!
```

### Example 3: The BackgroundTask Closed Session Disaster
```python
from fastapi import FastAPI, Depends, BackgroundTasks
from sqlalchemy.ext.asyncio import AsyncSession

app = FastAPI()

# ❌ BUG: Passing request-scoped session to background task!
# @app.post("/bad-order")
# async def bad_order(bg: BackgroundTasks, db: AsyncSession = Depends(safe_get_db)):
#     # safe_get_db closes the session as soon as response returns!
#     bg.add_task(db.execute, text("INSERT INTO audits ...")) # CRASH: Session is closed!
#     return {"status": "ok"}

# ✅ FIX: Background tasks must spawn their own isolated session!
async def run_audit_in_background(order_id: int):
    async with session_factory() as session:
        async with session.begin():
            session.add(AuditLog(order_id=order_id, action="created"))
            # Session commits and closes independently

@app.post("/good-order")
async def good_order(bg: BackgroundTasks, db: AsyncSession = Depends(safe_get_db)):
    # Pass ONLY primitive IDs or data
    bg.add_task(run_audit_in_background, order_id=101)
    return {"status": "ok"}
```

### Example 4: Deadlock Prevention via Deterministic Lock Ordering
```python
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy import select

# ❌ BUGGY: Two concurrent transfers can lock accounts in reverse order:
# Request 1 transfers from Account 1 to Account 2 (Locks 1, then waits for 2)
# Request 2 transfers from Account 2 to Account 1 (Locks 2, then waits for 1)
# Result: DEADLOCK! PostgreSQL aborts one transaction!

# ✅ FIX: Always sort account IDs before locking!
async def safe_transfer(session: AsyncSession, from_id: int, to_id: int, amount: float):
    # Sort IDs deterministically: always lock lower ID first!
    first_id, second_id = sorted([from_id, to_id])

    async with session.begin():
        # 1. Lock first account
        await session.execute(
            select(Account).where(Account.id == first_id).with_for_update()
        )
        # 2. Lock second account
        await session.execute(
            select(Account).where(Account.id == second_id).with_for_update()
        )
        
        # Now safely update balances without deadlock risk!
```

### Example 5: Accidental Column Drop on Alembic Autogenerate
```python
# ❌ DANGEROUS AUTOGENERATED MIGRATION:
# def upgrade():
#     op.drop_column('users', 'phone') # 💥 Wipes live phone numbers!
#     op.add_column('users', sa.Column('mobile_phone', sa.String()))

# ✅ SAFE MANUAL REFACTORING:
def upgrade():
    # Rename column without dropping data!
    op.alter_column('users', 'phone', new_column_name='mobile_phone')

def downgrade():
    op.alter_column('users', 'mobile_phone', new_column_name='phone')
```

---

## Anti-Patterns & Common Traps

| Anti-Pattern | Operational Hazard | Production Solution |
| :--- | :--- | :--- |
| **Setting `expire_on_commit=True` in async** | Triggers `MissingGreenlet` when reading object attributes after commit. | Set `expire_on_commit=False` in `async_sessionmaker`. |
| **Omitting `pool_pre_ping=True` on engines** | Stale connections dropped by firewalls trigger random 500 errors. | Always configure `pool_pre_ping=True`. |
| **Executing N+1 queries inside Pydantic schemas** | Drastically degrades endpoint latency under list views. | Use `selectinload()` and test with `QueryCounter`. |
| **Calling `session.commit()` inside repository methods** | Destroys atomic multi-repository transactions. | Keep commits at the Unit of Work or dependency layer. |

---

## Performance & Resource Characteristics

- **Connection Pool Leak Impact**: A leak of 1 connection per 1,000 requests exhausts a 30-connection pool after 30,000 requests, permanently taking the API offline until restart.
- **Deadlock Frequency**: In high-velocity transaction systems, sorting entity IDs prior to row-level locking reduces database deadlocks by **99.9%**.

---

## Edge Cases, Failure Modes & Gotchas

1. **Client Cancellation Mid-Query**: If a user cancels an HTTP request while a heavy query is running, asyncio cancels the coroutine. Wrapped `finally: await session.close()` blocks still execute to release the connection back to the pool.
2. **SQLite Locking in Unit Tests**: Using parallel test runners (`pytest -n auto`) against a file-based SQLite database causes `sqlite3.OperationalError: database is locked`. Use in-memory SQLite with `StaticPool` or isolated Testcontainers.

---

## Security Implications & OWASP Alignment

- **Denial of Service via Connection Leaks (OWASP API4:2023)**: Connection pool starvation denies service to legitimate users. Deterministic context managers guarantee connection recovery.
- **Race Condition Exploitation**: Inconsistent locking permits balance double-spending and inventory overdrafts.

---

## Modern Tooling Integration

- **pg_activity / pg_stat_activity**: Command-line monitor for inspecting active locks and deadlocks on PostgreSQL.
- **Alembic check**: Enforces that models match migration files in CI.

---

## Real-World Case Study / Production Incident

An online learning platform deployed an update adding student progress badges. A developer added `student.badges` to the Pydantic response schema. Because badges were lazy-loaded, the first user request threw `MissingGreenlet`.
The developer "fixed" it by wrapping the attribute access in `asyncio.to_thread`:
```python
badges = await asyncio.to_thread(lambda: student.badges)
```
In production, under 500 concurrent users, this spawned 500 worker threads, each checking out separate database connections to read badges, completely exhausting the database connection pool within 30 seconds.

**Remediation**:
1. Replaced the thread-pool hack with `.options(selectinload(Student.badges))`.
2. Total database queries dropped from 501 to exactly 2.
3. Database connection pool utilization dropped from 100% to 12%.

---

## Hands-On Exercises & Challenge Problems

### Warmup (2 Exercises)
1. Reproduce a `MissingGreenlet` error intentionally by accessing an unloaded relationship in an async route.
2. Fix the route using `selectinload()`.

### Intermediate (2 Exercises)
3. Write a test demonstrating that an unhandled `HTTPException` inside a route handler successfully triggers `session.rollback()` in the session dependency.
4. Implement a background task that safely performs database mutations using an independent session factory.

### Production-Grade (2 Exercises)
5. Build a simulation of two concurrent transactions transferring funds between Account 1 and Account 2 in opposite directions, verifying that un-ordered locking triggers a PostgreSQL deadlock (40P01), and then fix it using deterministic ID sorting.
6. Design and implement a custom SQLAlchemy event listener that raises an error in development if any query is executed without an active transaction or if a session is garbage-collected without being closed.

---

## Interview Mastery (8 Q&As)

### Junior Level
**Q1: What is the most common cause of `sqlalchemy.exc.MissingGreenlet` in FastAPI?**
*Answer:* Attempting to access a relationship attribute (e.g. `user.orders`) that was not eagerly loaded in the original query. The ORM tries to issue an implicit lazy-loading query synchronously, which fails in an async context.

**Q2: Why must you configure `expire_on_commit=False` in `async_sessionmaker`?**
*Answer:* By default, SQLAlchemy expires object attributes upon commit. Accessing expired attributes later triggers an implicit synchronous refresh query, causing a `MissingGreenlet` error. `expire_on_commit=False` prevents attributes from expiring.

**Q3: How do you prevent database connection leaks in FastAPI?**
*Answer:* By managing the session lifecycle inside an asynchronous context manager (`async with session_factory() as session: try: yield session finally: await session.close()`) within a FastAPI dependency.

### Senior Level
**Q4: Why does passing a request-scoped `AsyncSession` into `BackgroundTasks` fail?**
*Answer:* The request-scoped session dependency closes as soon as the HTTP response is sent. `BackgroundTasks` execute *after* the response has finished, attempting to run queries against an already-closed session, resulting in an `InvalidRequestError`.

**Q5: What causes a PostgreSQL `40P01 deadlock detected` error and how do you resolve it?**
*Answer:* A deadlock occurs when Transaction A locks Row 1 and waits for Row 2, while Transaction B locks Row 2 and waits for Row 1. The resolution is to enforce **deterministic lock ordering**: always acquire locks in the exact same order across all endpoints (e.g. `ORDER BY id`).

**Q6: What is the danger of running synchronous `psycopg2` inside an `async def` route?**
*Answer:* Because the route runs directly on the main asyncio event loop, the synchronous database driver blocks the event loop thread during every query, halting execution for all concurrent requests across the entire server.

### Staff / Architect Level
**Q7: How do you architect an enterprise database access layer that guarantees zero `MissingGreenlet` errors across a team of 40 developers?**
*Answer:*
1. **Model Hardening**: Enforce `lazy="raise"` on every `relationship()` declaration in all SQLAlchemy models. This turns lazy-loading attempts into immediate, informative errors at the exact query site during development.
2. **Repository Encapsulation**: Require all data access to go through Repositories that encapsulate explicit `selectinload()` / `joinedload()` options.
3. **Automated CI Tests**: Write integration tests that serialize all Pydantic response schemas against mock entities, failing builds if any relationship access raises an error.

**Q8: What is the risk of using `create_engine` vs `create_async_engine` when connecting through PgBouncer?**
*Answer:* When connecting asynchronously through PgBouncer in `transaction` pooling mode, `create_async_engine` with `asyncpg` will fail with duplicate prepared statement errors unless configured with `statement_cache_size=0`. Furthermore, client-side connection pooling should be disabled using `poolclass=NullPool` so PgBouncer can manage connection multiplexing effectively.
