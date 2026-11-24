---
title: "Testing Databases with Transaction Rollbacks and Testcontainers in FastAPI"
category: "11-testing"
order: 5
tags: ["fastapi", "database-testing", "sqlalchemy", "transactions", "testcontainers", "postgresql"]
py_version: "3.12+"
fastapi_version: "0.115+"
starlette_version: "0.38+"
pydantic_version: "2.8+"
tested_with: "Python 3.12, 3.13"
---

# Testing Databases with Transaction Rollbacks and Testcontainers in FastAPI

## 1. Conceptual Foundation & Mental Model

Database testing in backend web APIs presents a classic engineering trade-off:
- **Approach A: In-Memory SQLite (`sqlite:///:memory:`)**: Blazingly fast ($<1\text{ms}$ per test), but suffers from dialect mismatch. SQLite lacks native support for PostgreSQL JSONB operators, enum types, UUID columns, `SELECT ... FOR UPDATE`, and complex window functions. Tests pass on SQLite but fail in production on PostgreSQL.
- **Approach B: Shared Persistent Test Database**: Real PostgreSQL, but tests pollute the database with dirty rows (`user_1`, `order_101`). Truncating all tables (`TRUNCATE TABLE ... CASCADE`) between tests takes 50ms–200ms per test, causing a 1,000-test suite to take 10+ minutes.
- **Approach C: The Modern Gold Standard (Testcontainers + Transaction Rollback)**:
  1. **Testcontainers**: Spin up a real, ephemeral PostgreSQL Docker container once per test session.
  2. **Transaction Rollback (`SAVEPOINT` / Nested Transactions)**: Every test executes inside a database transaction. When the test completes (pass or fail), the fixture unconditionally **rolls back** the transaction. The database state reverts to pristine in $<1\text{ms}$ without truncating tables or dropping schema!

### The Mental Model: The Sandcastle on the Tidal Beach
- Truncating tables between tests is like building a massive stone fortress for every test, and then hiring a wrecking ball to demolish it, rake the rubble, and rebuild the stone walls for the next test. It takes minutes.
- **Transactional Rollback** is building a sandcastle on the beach with an instant **Undo Button**. You carve towers, dig moats, and pour water into the sand (**Test mutations**). The second your stopwatch stops, you press the Undo Button: the sand instantly flattens back to mirror-smooth beach surface (**`ROLLBACK`**). The next builder begins immediately on pristine sand with zero delay.

---

## 2. Architecture & The Nested Transaction (SAVEPOINT) Pattern

```
   Pytest Session Starts
             |
             v
   [ Testcontainers PostgreSQL Container Boots ] (Port 5432)
   - Runs Alembic Migrations once: `alembic upgrade head`
   - Creates global AsyncEngine
             |
             +-----------------------+
             |                       |
             v                       v
   Test A Begins                   Test B Begins
   +-----------------------+       +-----------------------+
   | Open Outer Connection |       | Open Outer Connection |
   | Start Transaction     |       | Start Transaction     |
   | Create `SAVEPOINT`    |       | Create `SAVEPOINT`    |
   |                       |       |                       |
   | Route executes:       |       | Route executes:       |
   | INSERT INTO users...  |       | INSERT INTO orders... |
   | [ App calls commit() ]|       | [ App calls commit() ]|
   | (Releases SAVEPOINT)  |       | (Releases SAVEPOINT)  |
   |                       |       |                       |
   | Teardown:             |       | Teardown:             |
   | `await trans.rollback`|       | `await trans.rollback`|
   | (ALL MUTATIONS GONE!) |       | (ALL MUTATIONS GONE!) |
   +-----------------------+       +-----------------------+
```

### Why Standard Rollback Fails in FastAPI Endpoints
If a FastAPI endpoint contains `await db.commit()`, a simple transaction fixture fails because the endpoint's commit writes the data permanently to disk. 
**The Solution**: Use SQLAlchemy's **Nested Transaction (`SAVEPOINT`)**. When the application code calls `session.commit()`, SQLAlchemy only releases the inner `SAVEPOINT`. The outer transaction remains open, allowing the test fixture's final `await trans.rollback()` to completely wipe away all changes!

---

## 3. Comprehensive Syntax & Core Mechanics

### 1. Testcontainers Setup (Postgres Container in Pytest)
```python
import pytest
from testcontainers.postgres import PostgresContainer
from sqlalchemy.ext.asyncio import create_async_engine

# Session-scoped container: boots once for entire test suite
@pytest.fixture(scope="session")
def postgres_container():
    # Launches real Docker container with PostgreSQL 16
    with PostgresContainer("postgres:16-alpine") as postgres:
        yield postgres

@pytest.fixture(scope="session")
async def db_engine(postgres_container):
    # Convert psycopg2 URL to asyncpg URL
    sync_url = postgres_container.get_connection_url()
    async_url = sync_url.replace("postgresql://", "postgresql+asyncpg://")
    
    engine = create_async_engine(async_url, echo=False)
    # Run migrations once
    async with engine.begin() as conn:
        await conn.run_sync(Base.metadata.create_all)
        
    yield engine
    await engine.dispose()
```

### 2. The Nested Transaction Rollback Fixture
```python
from sqlalchemy.ext.asyncio import AsyncSession
from typing import AsyncGenerator

@pytest.fixture
async def db_session(db_engine) -> AsyncGenerator[AsyncSession, None]:
    async with db_engine.connect() as connection:
        # Start outer transaction
        transaction = await connection.begin()
        
        # Bind session to connection; join_transaction_mode="create_savepoint"
        # ensures endpoint commit() only releases the savepoint!
        session = AsyncSession(
            bind=connection,
            expire_on_commit=False,
            join_transaction_mode="create_savepoint"
        )
        
        yield session
        
        # Teardown: Close session and rollback outer transaction
        await session.close()
        await transaction.rollback()
```

---

## 4. Practical Implementation

### Example 1: Full FastAPI Integration Test with Transactional Rollback
```python
import pytest
from httpx import AsyncClient, ASGITransport
from fastapi import FastAPI, Depends, status
from pydantic import BaseModel, EmailStr
from sqlalchemy import select, String
from sqlalchemy.orm import DeclarativeBase, Mapped, mapped_column
from sqlalchemy.ext.asyncio import AsyncSession

class Base(DeclarativeBase):
    pass

class UserModel(Base):
    __tablename__ = "users"
    id: Mapped[int] = mapped_column(primary_key=True)
    email: Mapped[str] = mapped_column(String, unique=True, index=True)

app = FastAPI()

def get_db():
    raise NotImplementedError("Replaced in test")

class UserCreate(BaseModel):
    email: EmailStr

@app.post("/users", status_code=status.HTTP_201_CREATED)
async def create_user(payload: UserCreate, db: AsyncSession = Depends(get_db)):
    user = UserModel(email=payload.email)
    db.add(user)
    await db.commit()  # Simulates production endpoint committing to database
    return {"id": user.id, "email": user.email}

@pytest.mark.anyio
async def test_user_creation_is_isolated(db_session: AsyncSession):
    # Wire rollback session to get_db
    app.dependency_overrides[get_db] = lambda: db_session
    
    transport = ASGITransport(app=app)
    try:
        async with AsyncClient(transport=transport, base_url="http://test") as client:
            resp = await client.post("/users", json={"email": "alice@example.com"})
            assert resp.status_code == 201
            assert resp.json()["email"] == "alice@example.com"
            
            # Query within test confirms user exists
            stmt = select(UserModel).where(UserModel.email == "alice@example.com")
            result = await db_session.execute(stmt)
            assert result.scalar_one_or_none() is not None
    finally:
        app.dependency_overrides.clear()
```

### Example 2: Verifying Isolation Between Sequential Tests
```python
@pytest.mark.anyio
async def test_subsequent_test_sees_pristine_database(db_session: AsyncSession):
    # This test executes AFTER test_user_creation_is_isolated!
    # Because previous test rolled back, alice@example.com MUST NOT exist!
    stmt = select(UserModel).where(UserModel.email == "alice@example.com")
    result = await db_session.execute(stmt)
    user = result.scalar_one_or_none()
    
    assert user is None, "Database leakage detected! Rollback fixture failed!"
```

### Example 3: Edge Case — Testing PostgreSQL Native Features (JSONB & Full-Text Search)
When testing PostgreSQL features that fail on SQLite:
```python
from sqlalchemy.dialects.postgresql import JSONB
from sqlalchemy import Column, Integer, select

class AuditLog(Base):
    __tablename__ = "audit_logs"
    id = Column(Integer, primary_key=True)
    metadata_json = Column(JSONB)  # Native Postgres JSONB type

@pytest.mark.anyio
async def test_postgres_jsonb_containment_operator(db_session: AsyncSession):
    log = AuditLog(metadata_json={"tags": ["security", "auth"], "ip": "10.0.0.1"})
    db_session.add(log)
    await db_session.flush()
    
    # JSONB containment operator (@>) works only on PostgreSQL
    stmt = select(AuditLog).where(AuditLog.metadata_json.contains({"ip": "10.0.0.1"}))
    res = await db_session.execute(stmt)
    assert res.scalar_one().id == log.id
```

### Example 4: Failure Case — Using In-Memory SQLite with PostgreSQL Migrations
```python
# CATASTROPHIC ANTI-PATTERN
# Running Alembic migrations designed for PostgreSQL against SQLite:
# alembic upgrade head (contains ALTER TABLE ... RENAME COLUMN or JSONB)
# FAILS: OperationalError: near "JSONB": syntax error!
# SQLite cannot parse PostgreSQL DDL dialects!
```

### Example 5: Idiomatic Modern — Ephemeral Database per Pytest Worker with `pytest-xdist`
When running tests in parallel (`pytest -n 4`), sharing a single database can cause deadlocks if non-transactional DDL operations occur. Use distinct template databases:
```python
@pytest.fixture(scope="session")
def worker_database_url(request, postgres_container):
    # Pytest-xdist provides worker_id ('gw0', 'gw1', 'master')
    worker_id = getattr(request.config, "workerinput", {}).get("workerid", "master")
    base_url = postgres_container.get_connection_url().replace("postgresql://", "postgresql+asyncpg://")
    # Separate database per parallel worker process
    return f"{base_url}_{worker_id}"
```

---

## 5. Edge Cases, Pitfalls & Failure Modes

1. **Auto-Increment ID Gaps**: Rolling back an `INSERT` statement in PostgreSQL does **NOT** roll back the sequence counter (`users_id_seq`). The sequence continues incrementing ($1, 2, 3$). Tests must never assert exact hardcoded IDs (`assert user.id == 1`); always assert `user.id is not None` or `user.id > 0`.
2. **DDL Statements Cannot Roll Back in Some Engines**: While PostgreSQL supports transactional DDL (`CREATE TABLE`, `DROP COLUMN` roll back safely), MySQL does not. Running schema migrations inside rollback tests on MySQL permanently commits changes.
3. **Database Connection Pool Exhaustion in Parallel Tests**: If `pytest-xdist` runs with `-n 8`, 8 worker processes connect simultaneously. If each worker configures a pool size of 20 connections, PostgreSQL reaches 160 connections. Set `max_connections=300` in the testcontainer.
4. **Third-Party Background Workers Accessing Rollback Data**: If a test creates a database row and dispatches a Celery task, the Celery worker connects via a separate database connection outside the test's transaction. The Celery worker **cannot see the uncommitted row**! Database tests involving background workers must use dedicated test databases or synchronous mocks.

---

## 6. Common Mistakes & Anti-Patterns

| Anti-Pattern | Root Cause | Impact | Correct Practice |
|:---|:---|:---|:---|
| Testing PostgreSQL code against SQLite in-memory | Avoiding Docker dependency | Fails on JSONB, Enums, UUIDs, window functions | Use Testcontainers with real PostgreSQL |
| Asserting hardcoded primary key IDs (`id == 1`) | Ignoring sequence behavior | Tests fail when run in different orders | Assert `id > 0` or capture dynamic IDs |
| Truncating tables between every test | Missing nested transaction knowledge | Test suite becomes 50x slower | Use `join_transaction_mode="create_savepoint"` |
| Forgetting `expire_on_commit=False` in AsyncSession | Default SQLAlchemy attribute expiration | `MissingGreenlet` error when reading attributes | Always set `expire_on_commit=False` |
| Calling `db.commit()` in fixtures | Missing rollback boundary | Pollutes database across tests | Use `flush()` in fixtures; savepoint rollback in teardown |
| Running container boot per test function | Misunderstanding fixture scopes | 5-second container startup penalty per test | Make container fixture `scope="session"` |

---

## 7. Performance & Scalability Considerations

- **Rollback Speed**: Rolling back a nested transaction (`SAVEPOINT`) takes less than 0.5 milliseconds in PostgreSQL, allowing a suite of 2,000 real database tests to complete in under 15 seconds.
- **Docker Volume in RAM (`tmpfs`)**: Configuring PostgreSQL Testcontainers with a `tmpfs` volume in RAM eliminates disk write latency, speeding up parallel test suites by 3x–5x.

---

## 8. Security & OWASP Guardrails

- **SQL Injection Verification in Tests**: Write automated integration tests that feed malicious SQL injection strings (e.g. `' OR '1'='1`) into query parameters to verify that SQLAlchemy parameterized queries properly escape characters.

---

## 9. Testing Strategies & Pytest Recipes

```python
import pytest
from sqlalchemy import select

@pytest.mark.anyio
async def test_sql_injection_resilience(db_session: AsyncSession):
    # Attempt SQL injection string in query
    malicious_email = "admin@corp.com' OR '1'='1"
    
    # Parameterized query safely treats string literally
    stmt = select(UserModel).where(UserModel.email == malicious_email)
    result = await db_session.execute(stmt)
    assert result.scalar_one_or_none() is None
```

---

## 10. Real-World Case Study / Production Incident

**The Incident**: A healthcare analytics backend used SQLite in-memory for their CI pipeline. All 800 tests passed in 12 seconds in GitHub Actions. After deploying to production on AWS Aurora PostgreSQL, the reporting engine crashed on every call with `ProgrammingError: operator does not exist: jsonb @> unknown`.

**Root Cause**: The developers used SQLite for testing because "it's fast and easy". SQLite permitted custom Python functions to mock JSON queries. However, the production query relied on PostgreSQL's native `@>` GIN-indexed JSONB containment operator, which had a subtle type-casting bug that SQLite's loose dynamic typing had masked completely.

**The Fix**:
1. Replaced SQLite with **Testcontainers PostgreSQL 16**.
2. Configured session-scoped container with `join_transaction_mode="create_savepoint"` rollbacks.
3. Test suite execution time only increased from 12 seconds to 18 seconds, but eliminated all dialect bugs permanently.

---

## 11. Technical Interview Questions & Answers

<details>
<summary>1. What is the advantage of using Testcontainers over an in-memory SQLite database for testing?</summary>
In-memory SQLite lacks feature parity with production databases like PostgreSQL (it does not support JSONB operators, strict enums, UUID columns, <code>SELECT ... FOR UPDATE</code>, custom extensions like pgvector, or full-text search). Testcontainers runs real, containerized PostgreSQL instances, eliminating dialect mismatch bugs while maintaining ephemeral isolation.
</details>

<details>
<summary>2. How does the Nested Transaction (<code>SAVEPOINT</code>) pattern work in SQLAlchemy testing?</summary>
The test fixture begins an outer transaction on a connection, and instantiates an <code>AsyncSession</code> configured with <code>join_transaction_mode="create_savepoint"</code>. When application endpoint code calls <code>await session.commit()</code>, SQLAlchemy releases only the inner <code>SAVEPOINT</code>. The outer transaction remains open, allowing the fixture teardown to execute <code>await transaction.rollback()</code>, which completely reverts all mutations.
</details>

<details>
<summary>3. Why do PostgreSQL primary key sequences continue incrementing even when transactions are rolled back?</summary>
PostgreSQL sequence generators (<code>nextval()</code>) operate outside transaction boundaries to maximize concurrency and avoid table-level locks. If a transaction draws ID 5 and then rolls back, ID 5 is permanently skipped. Tests must never assert hardcoded IDs like <code>assert user.id == 1</code>.
</details>

<details>
<summary>4. Why must <code>expire_on_commit=False</code> be set on test <code>AsyncSession</code> instances?</summary>
By default, SQLAlchemy marks all object attributes as expired upon commit. In asynchronous code, accessing an expired attribute triggers a lazy-load query; if this occurs outside an active event loop or greenlet context, SQLAlchemy raises a fatal <code>MissingGreenlet</code> exception. Setting <code>expire_on_commit=False</code> keeps object attributes cached in memory.
</details>

<details>
<summary>5. Can a separate background Celery worker process see uncommitted data from a test running inside a rollback transaction?</summary>
No. Relational databases enforce transaction isolation. A separate process operating on a different database connection cannot see uncommitted rows from an in-flight transaction (unless running in dirty read mode, which PostgreSQL does not support). Tests with external worker processes require committed data or in-process mocks.
</details>

<details>
<summary>6. How do you optimize Testcontainers performance to avoid slow CI builds?</summary>
(1) Scope the container fixture to <code>scope="session"</code> so Docker boots only once per test suite. (2) Apply schema migrations once at session startup. (3) Configure the container to mount PostgreSQL data directories onto a <code>tmpfs</code> RAM volume to bypass disk I/O.
</details>

<details>
<summary>7. What is the difference between <code>await session.flush()</code> and <code>await session.commit()</code>?</summary>
<code>flush()</code> sends pending SQL statements (INSERT, UPDATE) to the database engine, generating primary keys and checking constraints, but leaves the transaction open without committing. <code>commit()</code> flushes changes and permanently seals the transaction.
</details>

<details>
<summary>8. How do you handle database migrations in a test environment?</summary>
Execute Alembic migrations programmatically against the containerized test database during session fixture setup using <code>alembic.command.upgrade(alembic_cfg, "head")</code>. This verifies that your real migration scripts apply cleanly to a pristine database.
</details>

---

## 12. Practical Exercises & Solutions

### Exercise 1 (Easy): Verifying Rollback Isolation
**Task**: Write two sequential pytest tests. Test 1 inserts a product with SKU `"PROD-101"` and commits. Test 2 queries for `"PROD-101"` and asserts it does not exist, proving the rollback fixture works.
<details>
<summary>Solution</summary>

```python
import pytest
from sqlalchemy import select

@pytest.mark.anyio
async def test_step_1_insert_product(db_session: AsyncSession):
    p = UserModel(email="prod101@test.com")
    db_session.add(p)
    await db_session.commit()
    assert p.id is not None

@pytest.mark.anyio
async def test_step_2_verify_absence(db_session: AsyncSession):
    stmt = select(UserModel).where(UserModel.email == "prod101@test.com")
    res = await db_session.execute(stmt)
    assert res.scalar_one_or_none() is None
```
</details>

### Exercise 2 (Easy): Asserting on Non-Deterministic Primary Keys
**Task**: Write a test that inserts 3 users sequentially, asserting that all 3 have unique integer IDs greater than 0 without asserting exact values.
<details>
<summary>Solution</summary>

```python
import pytest

@pytest.mark.anyio
async def test_user_ids_are_positive_and_unique(db_session: AsyncSession):
    u1 = UserModel(email="u1@test.com")
    u2 = UserModel(email="u2@test.com")
    u3 = UserModel(email="u3@test.com")
    
    db_session.add_all([u1, u2, u3])
    await db_session.flush()
    
    ids = {u1.id, u2.id, u3.id}
    assert len(ids) == 3
    assert all(i > 0 for i in ids)
```
</details>

### Exercise 3 (Medium): Testing Unique Constraint Enforcement
**Task**: Write a test verifying that attempting to insert two users with the exact same email address raises an `IntegrityError` from SQLAlchemy.
<details>
<summary>Solution</summary>

```python
import pytest
from sqlalchemy.exc import IntegrityError

@pytest.mark.anyio
async def test_unique_email_constraint_raises_error(db_session: AsyncSession):
    u1 = UserModel(email="duplicate@test.com")
    u2 = UserModel(email="duplicate@test.com")
    
    db_session.add(u1)
    await db_session.flush()
    
    db_session.add(u2)
    with pytest.raises(IntegrityError):
        await db_session.flush()
```
</details>

### Exercise 4 (Medium): Session Fixture with Savepoint Isolation
**Task**: Implement a function-scoped async fixture `savepoint_session(engine)` using `join_transaction_mode="create_savepoint"`.
<details>
<summary>Solution</summary>

```python
import pytest
from sqlalchemy.ext.asyncio import AsyncSession
from typing import AsyncGenerator

@pytest.fixture
async def savepoint_session(db_engine) -> AsyncGenerator[AsyncSession, None]:
    async with db_engine.connect() as conn:
        trans = await conn.begin()
        session = AsyncSession(
            bind=conn,
            expire_on_commit=False,
            join_transaction_mode="create_savepoint"
        )
        yield session
        await session.close()
        await trans.rollback()
```
</details>

### Exercise 5 (Hard): Complete Testcontainers PostgreSQL Fixture
**Task**: Write a complete session-scoped pytest fixture that launches a `PostgresContainer`, applies database schema creation, and cleanly terminates the container.
<details>
<summary>Solution</summary>

```python
import pytest
from testcontainers.postgres import PostgresContainer
from sqlalchemy.ext.asyncio import create_async_engine

@pytest.fixture(scope="session")
def postgres_service():
    container = PostgresContainer("postgres:16-alpine")
    container.start()
    yield container
    container.stop()

@pytest.fixture(scope="session")
async def test_engine(postgres_service):
    raw_url = postgres_service.get_connection_url()
    async_url = raw_url.replace("postgresql://", "postgresql+asyncpg://")
    engine = create_async_engine(async_url, echo=False)
    
    async with engine.begin() as conn:
        await conn.run_sync(Base.metadata.create_all)
        
    yield engine
    await engine.dispose()
```
</details>

### Exercise 6 (Hard): Testing Concurrent Row Updates with Pessimistic Locking
**Task**: Write an integration test that opens two concurrent database sessions on the test engine. Session 1 executes `SELECT ... FOR UPDATE` on an account row. Verify Session 2 cannot acquire the lock until Session 1 completes.
<details>
<summary>Solution</summary>

```python
import pytest
import asyncio
from sqlalchemy import select

@pytest.mark.anyio
async def test_row_level_locking_concurrency(db_engine):
    # Setup row
    async with AsyncSession(db_engine) as init_session:
        user = UserModel(email="locked@test.com")
        init_session.add(user)
        await init_session.commit()
        user_id = user.id

    # Test lock behavior across two distinct connections
    async with db_engine.connect() as conn1, db_engine.connect() as conn2:
        trans1 = await conn1.begin()
        # Session 1 acquires exclusive lock
        stmt1 = select(UserModel).where(UserModel.id == user_id).with_for_update()
        res1 = await conn1.execute(stmt1)
        assert res1.scalar_one() is not None
        
        # Verify Session 2 with NOWAIT raises an error or blocks
        trans2 = await conn2.begin()
        stmt2 = select(UserModel).where(UserModel.id == user_id).with_for_update(nowait=True)
        
        from sqlalchemy.exc import OperationalError
        # In PostgreSQL, nowait=True raises OperationalError if row is locked
        try:
            await conn2.execute(stmt2)
            locked = False
        except OperationalError:
            locked = True
            
        assert locked is True, "Row was not locked by Session 1!"
        
        await trans1.rollback()
        await trans2.rollback()
```
</details>
