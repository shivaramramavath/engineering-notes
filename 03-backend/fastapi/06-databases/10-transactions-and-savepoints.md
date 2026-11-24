---
title: "FastAPI Database Transactions, Savepoints & Isolation Levels"
slug: "transactions-and-savepoints"
description: "Mastering database transactions in SQLAlchemy 2.0 async: explicit blocks, savepoints with begin_nested, isolation levels, and concurrency anomaly defense."
category: "06-databases"
tags: ["fastapi", "sqlalchemy", "transactions", "savepoints", "isolation-levels", "acid", "concurrency"]
difficulty: "Advanced"
date: "2026-03-30"
fastapi_version: "0.165.x"
starlette_version: "1.10.x"
pydantic_version: "2.14.x"
python_version: "3.12+"
tested_with: "Python 3.12, 3.13, 3.14t"
free_threading_support: "Fully Compatible"
---

# FastAPI Database Transactions, Savepoints & Isolation Levels

In mission-critical financial, e-commerce, and multi-tenant applications, executing database operations with strict ACID guarantees (Atomicity, Consistency, Isolation, Durability) is non-negotiable. In SQLAlchemy 2.0 async, mastering transaction boundaries—from explicit `async with session.begin():` blocks to **Savepoints** (`begin_nested()`) and **Isolation Level tuning**—prevents data corruption, eliminates race conditions, and defends against concurrency anomalies.

---

## Conceptual Foundation & Mental Model

### The ACID Guarantees
- **Atomicity**: All operations in a transaction succeed together or fail together. If step 4 of 5 fails, steps 1–3 are completely erased.
- **Consistency**: Database constraints (foreign keys, checks, unique rules) are strictly preserved across state transitions.
- **Isolation**: Concurrent transactions executing simultaneously do not observe uncommitted or conflicting intermediate states.
- **Durability**: Once a transaction commits, its modifications survive server crashes and power loss.

### Savepoints (`begin_nested()`)
A standard transaction is all-or-nothing. However, complex business workflows often require **partial rollback**:
- Step 1: Create Order (Must succeed).
- Step 2: Attempt to charge stored gift card (Optional; may fail).
- If Step 2 fails, roll back to a **Savepoint** without aborting Step 1!

```
BEGIN TRANSACTION;  (async with session.begin())
  INSERT INTO orders ...;
  
  SAVEPOINT sp1;   (async with session.begin_nested())
    INSERT INTO gift_card_charges ...;  ──► FAILS!
  ROLLBACK TO SAVEPOINT sp1;            ──► Gift card rolled back!
  
  INSERT INTO credit_card_charges ...;  ──► SUCCEEDS!
COMMIT;             (Order + Credit Card committed atomically!)
```

---

## Architecture & Internal Mechanics

### SQL Concurrency Anomalies & Isolation Levels

| Isolation Level | Dirty Reads | Non-Repeatable Reads | Phantom Reads | Serialization Anomaly / Write Skew |
| :--- | :---: | :---: | :---: | :---: |
| **Read Uncommitted** | ⚠️ Allowed | ⚠️ Allowed | ⚠️ Allowed | ⚠️ Allowed |
| **Read Committed** *(Default in Postgres)* | 🛡️ Prevented | ⚠️ Allowed | ⚠️ Allowed | ⚠️ Allowed |
| **Repeatable Read** | 🛡️ Prevented | 🛡️ Prevented | 🛡️ Prevented *(in Postgres)* | ⚠️ Allowed (Write Skew) |
| **Serializable** | 🛡️ Prevented | 🛡️ Prevented | 🛡️ Prevented | 🛡️ **Zero Anomalies** |

- **Dirty Read**: Transaction B reads uncommitted data written by Transaction A that is later rolled back.
- **Non-Repeatable Read**: Transaction A reads row X. Transaction B modifies row X and commits. Transaction A re-reads row X and observes different values.
- **Write Skew**: Two concurrent transactions read disjoint rows, satisfy a business rule, but their combined writes violate the invariant (prevented only by `SERIALIZABLE` or explicit locks).

```mermaid
sequenceDiagram
    autonumber
    participant App as FastAPI Route
    participant Session as AsyncSession
    participant DB as PostgreSQL

    App->>Session: async with session.begin():
    Session->>DB: BEGIN TRANSACTION
    App->>Session: db.add(AccountDebit)
    App->>Session: db.add(AccountCredit)
    alt All operations complete cleanly
        App->>Session: Exit context block
        Session->>DB: COMMIT
        Note over DB: Changes permanently written to WAL
    else Unhandled Exception Raised
        App->>Session: Exception bubbles out of context
        Session->>DB: ROLLBACK
        Note over DB: All mutations erased; state restored
    end
```

---

## Comprehensive Syntax & Core Mechanics

### 1. Explicit Transaction Context Block
```python
# The cleanest pattern for transactional boundaries:
async with session_factory() as session:
    async with session.begin():
        # Transaction is started automatically
        session.add(Item(name="Widget"))
        # Exiting block automatically calls await session.commit()
    # If any exception occurs inside block, it calls await session.rollback()!
```

### 2. Savepoints via `begin_nested()`
```python
async with session.begin(): # Outer transaction
    session.add(User(name="Alice"))
    await session.flush()

    try:
        async with session.begin_nested(): # SAVEPOINT savepoint_1
            session.add(InvalidAudit())
            await session.flush()
    except Exception:
        # Rolled back to SAVEPOINT savepoint_1; User(Alice) is still intact!
        pass

    # Commits User(Alice) successfully
```

### 3. Configuring Engine Isolation Level
```python
from sqlalchemy.ext.asyncio import create_async_engine

# Engine configured for financial ledger serializability
serializable_engine = create_async_engine(
    "postgresql+asyncpg://user:pass@localhost:5432/db",
    isolation_level="SERIALIZABLE" # Strict mathematical consistency
)
```

---

## Practical Implementation (5 Complete Examples)

### Example 1: Multi-Step Atomic Bank Account Transfer
```python
from fastapi import FastAPI, Depends, HTTPException, status
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy import select, update
from pydantic import BaseModel

app = FastAPI(title="Banking Ledger")

class TransferPayload(BaseModel):
    from_account_id: int
    to_account_id: int
    amount: float

@app.post("/transfers", status_code=status.HTTP_200_OK)
async def transfer_money(payload: TransferPayload, db: AsyncSession = Depends(get_db)):
    if payload.amount <= 0:
        raise HTTPException(status_code=400, detail="Amount must be positive")

    # Atomic transaction block
    async with db.begin():
        # 1. Lock sender account with Pessimistic Lock (FOR UPDATE)
        sender_stmt = (
            select(Account)
            .where(Account.id == payload.from_account_id)
            .with_for_update()
        )
        sender = (await db.execute(sender_stmt)).scalar_one_or_none()
        
        if not sender:
            raise HTTPException(status_code=404, detail="Sender account not found")
        if sender.balance < payload.amount:
            raise HTTPException(status_code=400, detail="Insufficient funds")

        # 2. Lock recipient account
        recipient_stmt = (
            select(Account)
            .where(Account.id == payload.to_account_id)
            .with_for_update()
        )
        recipient = (await db.execute(recipient_stmt)).scalar_one_or_none()
        if not recipient:
            raise HTTPException(status_code=404, detail="Recipient account not found")

        # 3. Apply balance mutations atomically
        sender.balance -= payload.amount
        recipient.balance += payload.amount

        # Record audit log
        db.add(TransferAudit(
            from_id=sender.id,
            to_id=recipient.id,
            amount=payload.amount
        ))

    # Leaving async with db.begin() commits automatically!
    return {"status": "transfer_completed", "amount": payload.amount}
```

### Example 2: Savepoint Pattern for Non-Fatal External Syncing
```python
from fastapi import FastAPI, Depends
from sqlalchemy.ext.asyncio import AsyncSession

@app.post("/users/{user_id}/upgrade")
async def upgrade_user(user_id: int, db: AsyncSession = Depends(get_db)):
    async with db.begin():
        user = await db.get(User, user_id)
        user.tier = "pro"

        # Attempt optional webhook record; if constraint fails, rollback ONLY savepoint!
        try:
            async with db.begin_nested():
                db.add(ExternalSyncQueue(user_id=user_id, status="pending"))
                await db.flush()
        except Exception as err:
            print(f"Non-critical sync queue insertion failed: {err}")
            # Main user tier upgrade remains active and will commit!

    return {"status": "upgraded", "tier": "pro"}
```

### Example 3: Handling `SerializationFailure` in Serializable Mode
```python
from fastapi import FastAPI, HTTPException
from sqlalchemy.exc import DBAPIError
import asyncio

# When running in SERIALIZABLE isolation, PostgreSQL may reject concurrent
# conflicting transactions with error: 40001 serialization_failure.
# The correct architectural response is to RETRY the transaction!

async def execute_with_retry(session_factory, max_retries=3):
    for attempt in range(max_retries):
        try:
            async with session_factory() as session:
                async with session.begin():
                    # Perform serializable business logic
                    return await perform_strict_calculation(session)
        except DBAPIError as exc:
            # PostgreSQL code 40001 indicates serialization failure
            if exc.orig and getattr(exc.orig, "sqlstate", None) == "40001":
                if attempt == max_retries - 1:
                    raise HTTPException(status_code=409, detail="Concurrent conflict. Please retry.")
                await asyncio.sleep(0.05 * (2 ** attempt)) # Exponential backoff
            else:
                raise
```

### Example 4: Testing Savepoints with Rollback
```python
import pytest
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy import select

@pytest.mark.anyio
async def test_savepoint_partial_rollback(db_session: AsyncSession):
    async with db_session.begin():
        db_session.add(Customer(id=1, name="Primary Customer"))
        await db_session.flush()

        # Savepoint fails
        try:
            async with db_session.begin_nested():
                db_session.add(Customer(id=1, name="Duplicate ID Customer")) # Duplicate PK!
                await db_session.flush()
        except Exception:
            pass # Savepoint rolled back cleanly!

        # Primary customer should still be present in transaction
        res = await db_session.execute(select(Customer).where(Customer.id == 1))
        assert res.scalar_one().name == "Primary Customer"
```

### Example 5: Setting Isolation Level Dynamically per Session
```python
from sqlalchemy.ext.asyncio import AsyncSession

# When the engine default is Read Committed, but a specific report requires Repeatable Read:
async def get_repeatable_read_session(session: AsyncSession) -> AsyncSession:
    # Set transaction characteristics dynamically for the current session
    await session.connection(
        execution_options={"isolation_level": "REPEATABLE READ"}
    )
    return session
```

---

## Anti-Patterns & Common Traps

| Anti-Pattern | Fatal Consequence | Production Solution |
| :--- | :--- | :--- |
| **Manual `commit()` inside `async with session.begin():`** | Raises `InvalidRequestError: Transaction already started`. | Let the context manager call `commit()` automatically upon exit. |
| **Deadlocks from inconsistent locking order** | Process A locks User 1 then User 2; Process B locks User 2 then User 1 $\rightarrow$ Deadlock! | Always lock rows in consistent, sorted order (`ORDER BY id`). |
| **Using `SERIALIZABLE` without retry logic** | PostgreSQL will abort transactions with 40001 errors under concurrent writes. | Implement exponential backoff retry loops for serializable transactions. |
| **Holding transactions open across network I/O** | Holds database locks and pool slots for seconds while awaiting Stripe or AWS S3. | Complete external HTTP calls *before* starting the database transaction block. |

---

## Performance & Resource Characteristics

- **Lock Contention**: `with_for_update()` holds exclusive row-level locks until transaction commit. Keep transaction blocks tightly focused on SQL operations (< 5ms) to avoid queueing other requests.
- **Savepoint Overhead**: Savepoints issue `SAVEPOINT <name>` SQL commands; nesting 2–3 levels has negligible overhead (< 0.1ms).

---

## Edge Cases, Failure Modes & Gotchas

1. **Deadlock Detection**: When two transactions deadlock, PostgreSQL automatically detects the cycle and aborts one with `DeadlockDetectedError` (SQLSTATE `40P01`). Always catch this error and retry.
2. **Autocommit on Read Operations**: Read-only `SELECT` queries still open an implicit transaction in SQLAlchemy. In high-read services, configure `session.begin()` explicitly only for writes, or use read-replicas.

---

## Security Implications & OWASP Alignment

- **Financial Integrity & Double-Spend Attacks**: Failing to use pessimistic locking (`with_for_update()`) allows malicious actors to execute two concurrent withdrawal requests simultaneously, spending funds twice.
- **Consistent Audit Logging**: Use transactions to guarantee that business actions and their security audit records are committed together atomically.

---

## Modern Tooling Integration

- **SQLAlchemy 2.0**: Modern context manager support via `async with session.begin():`.
- **asyncpg**: Native binary protocol support for PostgreSQL transaction controls and savepoints.

---

## Real-World Case Study / Production Incident

A ride-sharing platform allowed drivers to accept ride requests. Two drivers frequently clicked "Accept Ride" on the same request at the same instant.
The code was:
```python
ride = await db.get(Ride, ride_id)
if ride.status == "available":
    ride.status = "assigned"
    ride.driver_id = current_driver.id
    await db.commit()
```
Because both requests read `ride.status == "available"` concurrently before either committed, both drivers were told they won the ride, causing conflicting drivers to arrive at the passenger's pickup location.

**Remediation**:
Added pessimistic row locking:
```python
async with db.begin():
    stmt = select(Ride).where(Ride.id == ride_id).with_for_update()
    ride = (await db.execute(stmt)).scalar_one()
    if ride.status == "available":
        ride.status = "assigned"
        ride.driver_id = current_driver.id
    else:
        raise HTTPException(status_code=409, detail="Ride already accepted by another driver.")
```
Double-assignment anomalies dropped to **zero**.

---

## Hands-On Exercises & Challenge Problems

### Warmup (2 Exercises)
1. Write an explicit transaction using `async with session.begin():` that inserts two rows.
2. Verify that raising an exception inside the block rolls back both rows.

### Intermediate (2 Exercises)
3. Implement a funds transfer endpoint that locks sender and recipient rows using `.with_for_update()` sorted by ID to prevent deadlocks.
4. Implement a `begin_nested()` savepoint pattern that attempts to insert an optional record and cleanly rolls back on duplicate key error without aborting the main transaction.

### Production-Grade (2 Exercises)
5. Build an automated concurrency stress test using `asyncio.gather` that launches 20 concurrent withdrawal attempts against a \$100 account balance, verifying that total withdrawals never exceed \$100 and no race conditions occur.
6. Design an automatic retry decorator `@retry_on_serialization_failure(max_retries=5)` that catches PostgreSQL `40001` serialization errors and retries the transaction with randomized jitter.

---

## Interview Mastery (8 Q&As)

### Junior Level
**Q1: What does `async with session.begin():` do in SQLAlchemy?**
*Answer:* It starts a database transaction context manager. If the block completes without error, it automatically calls `await session.commit()`. If an exception occurs, it automatically calls `await session.rollback()`.

**Q2: What is an SQL Savepoint?**
*Answer:* A point within an active transaction that allows partial rollback of subsequent operations using `ROLLBACK TO SAVEPOINT` without aborting the entire transaction.

**Q3: What is the default isolation level in PostgreSQL?**
*Answer:* `READ COMMITTED`.

### Senior Level
**Q4: What is the difference between Optimistic Locking and Pessimistic Locking?**
*Answer:*
- **Pessimistic Locking** (`SELECT ... FOR UPDATE`): Locks the database row at read time, blocking other transactions from reading with lock or modifying the row until commit. Best for high-contention operations (e.g. seat booking, inventory).
- **Optimistic Locking**: Does not lock rows on read. Adds a `version_id` column and verifies on update: `UPDATE table SET ..., version = version + 1 WHERE id = 1 AND version = expected_version`. If another transaction changed the version, the update affects 0 rows, triggering an application-level retry. Best for low-contention operations.

**Q5: How does `session.begin_nested()` implement savepoints in SQLAlchemy?**
*Answer:* It issues an SQL `SAVEPOINT <name>` command. If the nested block completes cleanly, it releases the savepoint. If an exception occurs inside the block, it issues `ROLLBACK TO SAVEPOINT <name>`, restoring state to the savepoint while keeping the outer transaction active.

**Q6: What causes a database deadlock and how do you prevent it?**
*Answer:* Deadlocks occur when Transaction A holds a lock on Row 1 and waits for Row 2, while Transaction B holds a lock on Row 2 and waits for Row 1. The primary prevention strategy is **consistent lock ordering**: ensure all transactions acquire locks in the exact same deterministic order (e.g. sorting entity IDs in ascending order before locking).

### Staff / Architect Level
**Q7: What is Write Skew and in which isolation level does it occur?**
*Answer:* Write Skew occurs in `REPEATABLE READ` isolation when two concurrent transactions read overlapping data, each independently make a decision that satisfies a constraint, and write to disjoint rows, but the combined result violates the constraint.
*Example*: A clinic requires at least one doctor on call. Doctors Alice and Bob are both on call. Alice requests leave: transaction reads "2 doctors on call", approves leave, and removes Alice. Simultaneously, Bob requests leave: transaction reads "2 doctors on call", approves leave, and removes Bob. Both commit. Now 0 doctors are on call!
*Solution*: Enforce `SERIALIZABLE` isolation or explicitly lock all doctor rows using `FOR UPDATE`.

**Q8: Why should you avoid two-phase commits (2PC) across microservices in favor of the Saga Pattern?**
*Answer:* Two-Phase Commit (2PC) is a synchronous, blocking protocol. If one coordinator or node fails during the prepare/commit phase, locks are held across all participating databases indefinitely, degrading availability and throughput (violating the CAP theorem). The **Saga Pattern** uses asynchronous event-driven choreography or orchestration: each service executes a local transaction, and if a downstream step fails, compensating transactions are published to undo preceding steps asynchronously without distributed locking.
