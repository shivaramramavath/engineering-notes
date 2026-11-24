---
title: "Flaky Tests, Isolation, and State Management in FastAPI Test Suites"
description: "Diagnose, isolate, and eliminate test flakiness in FastAPI suites caused by shared database state, dependency override leaks, async race conditions, pytest-xdist concurrency collisions, and clock drift."
fastapi_version: "0.115.x+"
python_version: "3.12+"
pydantic_version: "v2.10+"
tested_with: "pytest 8.3+, pytest-xdist 3.6+, time-machine 2.16+, factory-boy 3.3+"
free_threading_status: "Fully Compatible"
---

# Flaky Tests, Isolation, and State Management in FastAPI Test Suites

## 1. Conceptual Foundation & Mental Model

### The Anatomy of Test Flakiness
A flaky test is a test that exhibits both passing and failing outcomes across different runs without any changes to the application source code. In FastAPI and modern async backend systems, test flakiness is rarely an unpredictable act of chance; it is almost always an architectural symptom of **leaked state, improper test isolation, or non-deterministic asynchronous timing**.

When a CI/CD build fails intermittently, engineering teams lose trust in their deployment safety net, prompting dangerous anti-patterns like unconditional retries (`pytest-rerunfailures`) that mask underlying production bugs such as database deadlocks or memory race conditions.

```
                             SOURCES OF FLAKINESS
                                       |
    +-------------------+--------------+--------------+--------------------+
    |                   |                             |                    |
    v                   v                             v                    v
[Shared DB State]  [Dependency Leak]            [Time/Clock Drift]  [Async Event Timing]
Unique constraint  app.dependency_overrides     datetime.utcnow()   asyncio.sleep()
conflicts across   persisting into              crossing midnight   races; unclosed
parallel workers   subsequent tests             in test runs        tasks across loops
```

### The Invariant of Pure Test Isolation
A robust test suite adheres to the principle of hermetic execution:
$$\forall \text{ Test } T_i, T_j \quad \text{State}(T_i \mid T_j) = \text{State}(T_i \mid \emptyset)$$
Every test must execute as if it were the sole test in existence, regardless of:
1. **Execution Order**: Passing when run alone or in randomized order (`pytest-randomly`).
2. **Execution Concurrency**: Passing when executed across 16 parallel worker processes (`pytest-xdist -n auto`).
3. **Execution Environment**: Passing identically on a developer's macOS/Windows workstation and within an ephemeral Linux CI container.

---

## 2. Architecture & Internal Mechanics

Understanding how state leaks across pytest worker processes and the FastAPI application object is crucial for achieving deterministic isolation.

```
+-----------------------------------------------------------------------------------+
|                           PYTEST TEST RUNNER PROCESS                              |
|                                                                                   |
|  +-------------------------------------+   +------------------------------------+ |
|  | Worker 0 (xdist gw0)                |   | Worker 1 (xdist gw1)               | |
|  | - Separate OS PID                   |   | - Separate OS PID                  | |
|  | - Independent Python Interpreter    |   | - Independent Python Interpreter   | |
|  | - Independent asyncio Event Loop    |   | - Independent asyncio Event Loop   | |
|  | - Isolated DB: test_db_gw0          |   | - Isolated DB: test_db_gw1         | |
|  |                                     |   |                                    | |
|  |   [FastAPI App Instance]            |   |   [FastAPI App Instance]           | |
|  |   app.dependency_overrides = {...}  |   |   app.dependency_overrides = {...} | |
|  |      |                              |   |      |                             | |
|  |      +-- LEAK HAZARD!               |   |      +-- LEAK HAZARD!              | |
|  |          Must clear in teardown     |   |          Must clear in teardown    | |
|  +-------------------------------------+   +------------------------------------+ |
+-----------------------------------------------------------------------------------+
                                          |
                +-------------------------+-------------------------+
                |                                                   |
                v                                                   v
   [PostgreSQL DB: test_db_gw0]                        [PostgreSQL DB: test_db_gw1]
   (Transactions rolled back per test)                 (Transactions rolled back per test)
```

### Root Causes of Flakiness in FastAPI Applications
1. **FastAPI `app.dependency_overrides` Persistence**: The `app.dependency_overrides` dictionary is a mutable global mapping on the `FastAPI` instance. If a test overrides `get_current_user` to simulate an admin user and fails to clear the dictionary upon completion, all subsequent tests execute with administrative credentials, causing intermittent authorization assertions to fail.
2. **Database Primary Key / Unique Constraint Collisions**: Hardcoding entity IDs (e.g., `user_id = "test-user-123"`) or static email addresses (`"test@example.com"`) triggers duplicate key violations when tests run in parallel or when previous failed tests leave uncleaned rows.
3. **System Clock Drift & Midnight Rollover**: Assertions calculating `datetime.now()` directly against database `created_at` timestamps fail when tests cross second, minute, or midnight boundaries, or when operating across differing UTC offset environments.
4. **Shared Async Event Loops**: Reusing client sessions or database connection pools across different `pytest-asyncio` event loops triggers `RuntimeError: Task attached to a different loop`.

---

## 3. Comprehensive Syntax & Core Mechanics

### Automatic Dependency Override Teardown
Using an autouse fixture guarantees that `app.dependency_overrides` is restored to an empty state even if an assertion raises an uncaught exception.

```python
import pytest
from app.main import app

@pytest.fixture(autouse=True)
def clean_dependency_overrides():
    """Autouse fixture ensuring pristine dependency mapping for every test."""
    yield
    app.dependency_overrides.clear()
```

### Pytest-xdist Multi-Database Partitioning
When executing `pytest -n auto`, `pytest-xdist` assigns a worker identifier (`gw0`, `gw1`, etc.) accessible via the `worker_id` fixture. We use this identifier to direct each parallel process to a dedicated database schema or database instance.

```python
# tests/conftest.py
import os
import pytest

@pytest.fixture(scope="session")
def database_url(worker_id: str) -> str:
    # Use worker_id to assign an isolated database to each xdist process
    base_url = os.getenv("TEST_DATABASE_BASE_URL", "postgresql+asyncpg://postgres:secret@localhost:5432")
    if worker_id == "master":
        # Single process run (not using xdist)
        return f"{base_url}/test_db"
    return f"{base_url}/test_db_{worker_id}"
```

---

## 4. Practical Implementation: 5 Verified Examples

### Example 1: The Dependency Override Leak Hazard and the Canonical Fix
Demonstrating how an uncleared dependency override bleeds into subsequent test cases and how an autouse fixture prevents it.

```python
# app/dependencies.py
from fastapi import Depends, HTTPException, status

def get_current_user_role() -> str:
    return "guest"

def require_admin(role: str = Depends(get_current_user_role)):
    if role != "admin":
        raise HTTPException(status_code=status.HTTP_403_FORBIDDEN, detail="Access denied")
    return True
```

```python
# tests/test_override_leaks.py
import pytest
from fastapi import FastAPI, Depends
from fastapi.testclient import TestClient
from app.dependencies import require_admin, get_current_user_role

app = FastAPI()

@app.get("/admin/dashboard")
def admin_dashboard(_: bool = Depends(require_admin)):
    return {"status": "admin_granted"}

# --- FIXTURE GUARANTEEING ISOLATION ---
@pytest.fixture(autouse=True)
def reset_overrides():
    # Setup phase: clean state
    app.dependency_overrides.clear()
    yield
    # Teardown phase: guaranteed cleanup
    app.dependency_overrides.clear()

def test_admin_access_with_override():
    # Override guest role to admin
    app.dependency_overrides[get_current_user_role] = lambda: "admin"
    client = TestClient(app)
    response = client.get("/admin/dashboard")
    assert response.status_code == 200

def test_default_guest_access_forbidden():
    # If the previous test leaked its override, this test would fail!
    client = TestClient(app)
    response = client.get("/admin/dashboard")
    assert response.status_code == 403
```

---

### Example 2: Transaction-Rollback Isolation for Async SQLAlchemy
Ensuring zero leftover database state between test runs by wrapping every test inside an outer transaction that rolls back upon completion.

```python
# tests/db_fixtures.py
import pytest
from sqlalchemy.ext.asyncio import create_async_engine, async_sessionmaker, AsyncSession
from app.db.base import Base

TEST_DATABASE_URL = "sqlite+aiosqlite:///:memory:"

@pytest.fixture(scope="session")
async def test_engine():
    engine = create_async_engine(TEST_DATABASE_URL, echo=False)
    async with engine.begin() as conn:
        await conn.run_sync(Base.metadata.create_all)
    yield engine
    await engine.dispose()

@pytest.fixture
async def db_session(test_engine) -> AsyncSession:
    """Provides complete transactional isolation; rolls back all mutations."""
    connection = await test_engine.connect()
    transaction = await connection.begin()
    
    session_factory = async_sessionmaker(
        bind=connection,
        class_=AsyncSession,
        expire_on_commit=False,
    )
    session = session_factory()

    yield session

    await session.close()
    await transaction.rollback()
    await connection.close()
```

---

### Example 3: Deterministic Time Freezing with `time-machine`
Replacing system clock lookups with a frozen, deterministic temporal reference to eliminate time-zone and midnight-rollover flakiness.

```python
# app/services/subscription.py
from datetime import datetime, timezone
from pydantic import BaseModel

class Subscription(BaseModel):
    user_id: str
    expires_at: datetime

    def is_active(self) -> bool:
        now = datetime.now(timezone.utc)
        return self.expires_at > now
```

```python
# tests/test_subscription_time.py
from datetime import datetime, timezone
import pytest
import time_machine
from app.services.subscription import Subscription

# Freeze time at an exact nanosecond timestamp
FROZEN_TIME = datetime(2026, 6, 15, 12, 0, 0, tzinfo=timezone.utc)

@time_machine.travel(FROZEN_TIME, tick=False)
def test_subscription_active_state():
    # Active: Expires 1 hour into the future relative to frozen time
    sub_active = Subscription(
        user_id="user_1",
        expires_at=datetime(2026, 6, 15, 13, 0, 0, tzinfo=timezone.utc)
    )
    assert sub_active.is_active() is True

    # Expired: Expired 1 second in the past relative to frozen time
    sub_expired = Subscription(
        user_id="user_2",
        expires_at=datetime(2026, 6, 15, 11, 59, 59, tzinfo=timezone.utc)
    )
    assert sub_expired.is_active() is False
```

---

### Example 4: Randomized Unique Data Generation with Factory-Boy
Preventing duplicate unique key collisions in multi-worker `pytest-xdist` runs by using sequence-based factory models.

```python
# tests/factories.py
import factory
from pydantic import BaseModel

class UserPayload(BaseModel):
    username: str
    email: str
    tenant_id: str

class UserPayloadFactory(factory.Factory):
    class Meta:
        model = UserPayload

    # Sequence ensures guaranteed unique strings across test invocations
    username = factory.Sequence(lambda n: f"user_{n}_{factory.Faker('uuid4').generate()}")
    email = factory.Sequence(lambda n: f"user_{n}@isolated-testing.internal")
    tenant_id = factory.Faker("uuid4")
```

```python
# tests/test_user_registration.py
from tests.factories import UserPayloadFactory

def test_user_creation_with_unique_data():
    # Generates a guaranteed unique user payload
    payload1 = UserPayloadFactory()
    payload2 = UserPayloadFactory()
    
    assert payload1.username != payload2.username
    assert payload1.email != payload2.email
```

---

### Example 5: Detecting and Quarantining Flaky Tests in CI
A custom Pytest plugin / configuration recipe to detect flakiness during CI builds using `pytest-randomly` and strict warnings.

```toml
# pyproject.toml
[tool.pytest.ini_options]
minversion = "8.0"
addopts = """
    --strict-markers
    --strict-config
    --randomly-seed=last
    -W error::pytest.PytestUnraisableExceptionWarning
    -W error::RuntimeWarning
"""
markers = [
    "flaky: marks tests under quarantine for flakiness investigation",
    "integration: marks slow integration tests",
]
```

```python
# tests/test_async_race_conditions.py
import asyncio
import pytest

@pytest.mark.asyncio
async def test_deterministic_task_coordination():
    """Avoid relying on arbitrary asyncio.sleep() delays for coordination."""
    ready_event = asyncio.Event()
    results = []

    async def worker():
        await ready_event.wait()
        results.append("work_completed")

    task = asyncio.create_task(worker())
    
    # Deterministic signal instead of `await asyncio.sleep(0.01)`
    ready_event.set()
    await task
    
    assert results == ["work_completed"]
```

---

## 5. Edge Cases, Pitfalls & Failure Modes

1. **`asyncio.sleep` as a Synchronization Primitive**: Writing `await asyncio.sleep(0.05)` to "wait for a background task or worker to finish" is the number one cause of flaky tests in CI environments. Under high CPU load in CI, 50ms is often insufficient, causing random assertion failures. Use `asyncio.Event` or explicit polling loops with timeouts (`anyio.wait_for(...)`).
2. **SQLite In-Memory Concurrency Limitations**: Using `:memory:` SQLite databases across multiple threads in pytest causes `sqlite3.OperationalError: database is locked`. Use file-based SQLite with unique worker filenames or test against PostgreSQL Docker containers.
3. **Module-Level Cached Singletons**: If an application instantiates a global HTTP client (`client = httpx.AsyncClient()`) at module import time, multiple test cases will attempt to reuse the same socket connections across differing event loops, raising `RuntimeError: Event loop is closed`.
4. **Timezone Assumptions in Date Parsing**: Writing tests assuming local timezone is UTC. If a test passes locally (e.g., UTC+5:30) but fails in GitHub Actions (UTC), datetime strings lacking explicit timezone offsets (`+00:00` or `Z`) will fail date comparisons.

---

## 6. Common Mistakes Table

| Mistake | Root Cause | Impact | Correct Approach |
| :--- | :--- | :--- | :--- |
| **Using `pytest-rerunfailures` as a Solution** | Hiding flakiness instead of fixing root cause | Masks race conditions, memory leaks, and deadlocks; degrades CI build confidence. | Treat flakiness as a production severity bug; isolate and fix state leaks. |
| **Omitting Override Cleanup** | Modifying `app.dependency_overrides` without clearing | Leaked mocks break unrelated tests depending on execution order. | Use an autouse fixture yielding and calling `app.dependency_overrides.clear()`. |
| **Hardcoding Unique Identifiers** | Reusing strings like `"admin@test.com"` | Tests fail with primary key/unique constraint violations under `pytest-xdist`. | Generate unique dynamic IDs with `uuid.uuid4()` or `factory-boy` sequences. |
| **Relying on Arbitrary `sleep()` Delays** | Guessing async task completion timing | Tests pass locally but fail in CI runners under CPU throttling. | Synchronize using `asyncio.Event` or polling assertions with a strict timeout. |
| **Reusing Event Loops Across Tests** | Scope of event loop fixture configured as session | `RuntimeError: Task attached to a different loop` or closed event loop errors. | Scope async event loops to `function` level (default in `pytest-asyncio` 0.24+). |
| **Committing DB Transactions in Tests** | Calling `await session.commit()` in test body | Pollutes database for subsequent tests, breaking test independence. | Wrap test execution in an outer transaction that rolls back upon fixture teardown. |

---

## 7. Performance & Scalability Considerations

- **Parallel Scaling with `pytest-xdist`**: Running tests with `pytest -n auto` achieves linear scaling across CPU cores only if database state is partitioned per worker. Use worker-specific schemas or database names (`test_db_{worker_id}`).
- **In-Memory Schema Caching**: Instead of executing full database migrations (`alembic upgrade head`) before every test, apply migrations once per session per worker database, and truncate or roll back tables between individual tests.
- **Python 3.13+ Free-Threading**: Under free-threaded Python, CPU-bound fixture initialization and Pydantic schema validation execute concurrently across threads without GIL pauses, speeding up test suite execution.

---

## 8. Security & OWASP Guardrails

- **Test Credential Leak Prevention**: Never store real third-party API tokens (Stripe, Twilio, AWS) in test fixtures or mock configurations. Enforce strict dummy values (`sk_test_fake123`) to prevent accidental live API calls during flaky test runs.
- **OWASP A01 (Broken Access Control Verification)**: Ensure test isolation verifies that permission resets function reliably. A test suite with uncleared admin overrides creates false confidence that authorization barriers work when they may be entirely bypassed.

---

## 9. Testing Strategies & Pytest Recipes

```python
# tests/test_isolation_verification.py
import pytest
from httpx import AsyncClient, ASGITransport
from app.main import app

@pytest.mark.asyncio
async def test_order_independent_execution_probe():
    """Verify that route handles fresh invocations without residual state."""
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        # Request should succeed regardless of whether previous tests ran
        res = await client.get("/api/v1/health")
        assert res.status_code == 200
        assert res.json()["status"] == "ok"
```

---

## 10. Real-World Case Study / Production Incident

A financial analytics enterprise experienced random CI pipeline failures that delayed deployments by days. Approximately 1 in every 8 CI builds failed on a test named `test_calculate_monthly_yield`.

**Root Cause Investigation**:
1. Running `pytest --randomly-seed=...` revealed that `test_calculate_monthly_yield` failed only when executed *after* `test_upgrade_user_subscription`.
2. `test_upgrade_user_subscription` modified a global application dependency:
   ```python
   app.dependency_overrides[get_exchange_rate_provider] = MockCustomRates()
   ```
3. Because the author did not clear `app.dependency_overrides` upon completion, `test_calculate_monthly_yield` executed using the mock rates from the preceding test instead of the standard pricing engine.
4. When pytest executed tests in the default alphabetical order, the test passed. When `pytest-xdist` or `pytest-randomly` shuffled the test order, the build broke.

**Remediation**:
The platform team introduced an autouse fixture in `conftest.py` enforcing `app.dependency_overrides.clear()`. They also integrated `pytest-randomly` into their pull-request validation gate to permanently prevent order-dependent tests from merging into the codebase.

---

## 11. Technical Interview Questions

<details>
<summary>1. What is the fundamental root cause of flaky tests in asynchronous FastAPI applications?</summary>
The root cause is almost always unisolated shared state or non-deterministic timing. Specifically:
1. Leaked dependencies via uncleared `app.dependency_overrides`.
2. Residual data in shared database tables due to missing rollback transactions.
3. Race conditions created by using arbitrary `asyncio.sleep()` instead of deterministic synchronization primitives like `asyncio.Event`.
4. Tests relying on mutable global singletons or the system clock.
</details>

<details>
<summary>2. How does `pytest-randomly` help discover order-dependent test flakiness?</summary>
`pytest-randomly` executes test files and individual test methods in a randomized order using a pseudo-random seed. If a test passes only when run after a specific setup test (or fails when run after a test that leaks state), running tests in randomized order immediately exposes the implicit coupling. The seed can then be used to reproduce the failure deterministically.
</details>

<details>
<summary>3. Why is transactional rollback preferable to truncating database tables between tests?</summary>
Truncating tables requires issuing DDL/DML statements (`TRUNCATE TABLE ...`) across all tables, which is slow and requires resetting foreign key checks. Transactional rollback simply executes `ROLLBACK` on an outer transaction begun at the start of the test. The rollback takes less than a millisecond, leaves zero residual data, and does not require re-running table creation or schema migrations.
</details>

<details>
<summary>4. What problems occur when using `datetime.now()` directly in test assertions?</summary>
System clock calls are non-deterministic. If a test checks that an item expires "tomorrow" or runs across midnight, milliseconds elapsed during test execution can cause off-by-one second mismatches, date boundary crossovers, or timezone discrepancies between local workstations and UTC-configured CI servers. Using tools like `time-machine` freezes the clock at a constant, reproducible instant.
</details>

<details>
<summary>5. How should database isolation be handled when running tests with `pytest-xdist`?</summary>
Each `pytest-xdist` worker process must connect to an independent database or schema. By inspecting the `worker_id` fixture (e.g., `gw0`, `gw1`), test session fixtures can provision separate databases (`test_db_gw0`, `test_db_gw1`) or separate Postgres schemas, preventing concurrent workers from overwriting or locking each other's test records.
</details>

<details>
<summary>6. Why is `pytest-rerunfailures` considered an anti-pattern when used globally?</summary>
Configuring tests to automatically retry upon failure masks real defects such as database deadlocks, connection pool timeouts, and asynchronous race conditions that will manifest under production concurrency. Retries should only be permitted as a temporary quarantine measure for third-party flaky network dependencies, never for internal domain logic.
</details>

<details>
<summary>7. What is an autouse fixture in pytest, and why is it recommended for dependency override cleanup?</summary>
An `autouse=True` fixture executes automatically for every test in its scope without needing to be explicitly requested in the test signature. Using an autouse fixture for `app.dependency_overrides.clear()` guarantees that even if a developer forgets to clean up an override or if a test throws an unhandled assertion error midway through execution, cleanup is guaranteed in the fixture's `yield` teardown block.
</details>

<details>
<summary>8. How do you prevent event loop mismatch errors (`RuntimeError: Task attached to a different loop`) in pytest-asyncio?</summary>
Ensure that async fixtures and client instances do not outlive the event loop. In modern `pytest-asyncio` (0.24+), the default event loop policy creates a new event loop per test function. Session-scoped async fixtures must either be carefully managed or all async client fixtures should be scoped to `function` to match the lifetime of the test's event loop.
</details>

---

## 12. Practical Exercises & Solutions

<details>
<summary>Exercise 1 (Easy): Universal Dependency Override Autouse Fixture</summary>

**Task**: Write an autouse pytest fixture in `conftest.py` that guarantees `app.dependency_overrides` is empty before and after every test function.

**Solution**:
```python
import pytest
from app.main import app

@pytest.fixture(autouse=True)
def ensure_clean_overrides():
    # Clear prior to test execution
    app.dependency_overrides.clear()
    yield
    # Clear after test execution (even if test fails)
    app.dependency_overrides.clear()
```
</details>

<details>
<summary>Exercise 2 (Easy): Freezing Time for Token Expiration Testing</summary>

**Task**: Write a test using `time-machine` that freezes the clock at `2026-01-01 00:00:00 UTC`, issues an access token that expires in 15 minutes, and asserts that the token is valid at 14 minutes and expired at 16 minutes.

**Solution**:
```python
from datetime import datetime, timezone, timedelta
import time_machine

FROZEN_TIME = datetime(2026, 1, 1, 0, 0, 0, tzinfo=timezone.utc)

def is_token_valid(token_exp: datetime) -> bool:
    return datetime.now(timezone.utc) < token_exp

def test_token_expiration_window():
    with time_machine.travel(FROZEN_TIME, tick=False) as traveller:
        token_exp = datetime.now(timezone.utc) + timedelta(minutes=15)
        
        # At frozen time: valid
        assert is_token_valid(token_exp) is True
        
        # Advance 14 minutes: still valid
        traveller.shift(timedelta(minutes=14))
        assert is_token_valid(token_exp) is True
        
        # Advance another 2 minutes (total 16 mins): expired!
        traveller.shift(timedelta(minutes=2))
        assert is_token_valid(token_exp) is False
```
</details>

<details>
<summary>Exercise 3 (Medium): Partitioning Test Databases with xdist worker_id</summary>

**Task**: Write a session-scoped pytest fixture `db_url` that inspects `worker_id` and returns a worker-specific SQLite database connection string (e.g., `sqlite:///test_gw0.db` or `sqlite:///test_master.db`).

**Solution**:
```python
import pytest

@pytest.fixture(scope="session")
def worker_db_url(worker_id: str) -> str:
    if worker_id == "master":
        db_name = "test_master.db"
    else:
        db_name = f"test_{worker_id}.db"
        
    return f"sqlite:///{db_name}"
```
</details>

<details>
<summary>Exercise 4 (Medium): Replacing Flaky Sleep with asyncio.Event</summary>

**Task**: Refactor the flaky test below to use deterministic synchronization instead of `asyncio.sleep`:
```python
# Flaky test
async def test_flaky_background_job():
    job_done = False
    async def run_job():
        nonlocal job_done
        job_done = True
    asyncio.create_task(run_job())
    await asyncio.sleep(0.01) # FLAKY!
    assert job_done is True
```

**Solution**:
```python
import asyncio
import pytest

@pytest.mark.asyncio
async def test_deterministic_background_job():
    done_event = asyncio.Event()

    async def run_job():
        done_event.set()

    asyncio.create_task(run_job())
    
    # Wait with timeout to prevent hanging if job fails
    await asyncio.wait_for(done_event.wait(), timeout=1.0)
    assert done_event.is_set() is True
```
</details>

<details>
<summary>Exercise 5 (Hard): Async SQLAlchemy Transaction Rollback Fixture</summary>

**Task**: Implement a complete async pytest fixture `transactional_session` that binds an `AsyncSession` to an open transaction, yields the session to the test, and rolls back the transaction upon completion.

**Solution**:
```python
import pytest
from sqlalchemy.ext.asyncio import create_async_engine, async_sessionmaker, AsyncSession

@pytest.fixture
async def transactional_session():
    engine = create_async_engine("sqlite+aiosqlite:///:memory:")
    connection = await engine.connect()
    trans = await connection.begin()

    session_factory = async_sessionmaker(
        bind=connection,
        class_=AsyncSession,
        expire_on_commit=False,
    )
    session = session_factory()

    try:
        yield session
    finally:
        await session.close()
        await trans.rollback()
        await connection.close()
        await engine.dispose()
```
</details>

<details>
<summary>Exercise 6 (Hard): Custom Pytest Flaky Test Detection Fixture</summary>

**Task**: Write a pytest hook or wrapper fixture that runs a designated test 10 times consecutively in memory. If the test passes in some iterations and fails in others, raise an explicit `AssertionError("Flakiness detected: test results were non-deterministic across iterations")`.

**Solution**:
```python
import pytest

def assert_hermetic_stability(test_func, iterations: int = 10):
    results = []
    for i in range(iterations):
        try:
            test_func()
            results.append(True)
        except Exception:
            results.append(False)
            
    passed_count = sum(results)
    if 0 < passed_count < iterations:
        raise AssertionError(
            f"Flakiness detected in {test_func.__name__}: passed {passed_count}/{iterations} runs!"
        )
    if passed_count == 0:
        raise AssertionError(f"Test {test_func.__name__} failed all {iterations} runs.")
```
</details>
