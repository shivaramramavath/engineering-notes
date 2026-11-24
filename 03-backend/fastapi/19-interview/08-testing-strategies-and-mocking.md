# Technical Interview Mastery: Asynchronous Testing Strategies & Pytest Recipes

## Overview & Evaluation Criteria

Modern testing for asynchronous Python requires understanding `pytest-asyncio`, lifespan events in `httpx.AsyncClient` vs `TestClient`, transactional rollback fixtures in PostgreSQL, and mocking distributed dependencies cleanly.

---

## Technical Q&A Deep Dive

<details>
<summary><strong>Q1: What is the architectural difference between Starlette's synchronous <code>TestClient</code> and <code>httpx.AsyncClient</code> with <code>ASGITransport</code>?</strong></summary>

### Answer:
- **`TestClient` (Synchronous)**:
  - Built on top of `requests` and Starlette's in-process WSGI-to-ASGI adapter.
  - Runs its own private event loop behind the scenes.
  - **Limitation**: Cannot easily share database connection fixtures or async mock scopes when tests themselves are declared as `async def`.
- **`httpx.AsyncClient(transport=ASGITransport(app=app))`**:
  - Pure asynchronous HTTP client running directly on the test runner's `asyncio` event loop.
  - Fully compatible with `pytest-asyncio` fixtures, async database transactions, and real-time WebSocket mocking.
  - **Note**: Requires explicit `lifespan` context management to trigger FastAPI startup/shutdown handlers during tests.

</details>

<details>
<summary><strong>Q2: How do you achieve 100% test isolation without rebuilding database schemas or re-running Alembic migrations between tests?</strong></summary>

### Answer:
- **Nested Transaction Rollback Pattern**:
  1. Build the database schema once per test session (session-scoped fixture).
  2. For every individual test, open an outer database connection and begin a transaction.
  3. Bind the test session to this transaction.
  4. Inject this session into the route using `app.dependency_overrides[get_db] = lambda: session`.
  5. The test runs its mutations (INSERT, UPDATE).
  6. In fixture cleanup, **always execute `transaction.rollback()`**.
  7. The database remains completely pristine for the next test in sub-millisecond execution time, eliminating costly table truncation or dropping.

```python
@pytest_asyncio.fixture
async def db_session(test_engine):
    async with test_engine.connect() as conn:
        trans = await conn.begin()
        session = AsyncSession(bind=conn, expire_on_commit=False)
        yield session
        await session.close()
        await trans.rollback()
```

</details>

<details>
<summary><strong>Q3: How do you test background tasks triggered by <code>BackgroundTasks.add_task()</code> in FastAPI?</strong></summary>

### Answer:
- In `httpx.AsyncClient` or `TestClient`, Starlette automatically awaits background tasks before the test client returns control to the assertion block.
- **Verification Strategy**:
  1. Use `unittest.mock.patch` or an `AsyncMock` spy on the underlying worker service function.
  2. Execute the API call.
  3. Assert that the mock was awaited with exact expected domain parameters:
     ```python
     mock_mailer.assert_awaited_once_with("user@test.com", "Welcome!")
     ```
</details>
