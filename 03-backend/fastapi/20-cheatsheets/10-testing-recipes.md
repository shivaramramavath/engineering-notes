# Quick Reference: Async Testing & Pytest Recipes Cheatsheet

## Standard `tests/conftest.py` Fixtures Template

```python
import pytest
import pytest_asyncio
from httpx import ASGITransport, AsyncClient
from sqlalchemy.ext.asyncio import AsyncSession, async_sessionmaker, create_async_engine
from src.database import get_db
from src.domain.models import Base
from src.main import app

TEST_DB_URL = "sqlite+aiosqlite:///:memory:"

@pytest_asyncio.fixture(scope="session")
async def test_engine():
    engine = create_async_engine(TEST_DB_URL)
    async with engine.begin() as conn:
        await conn.run_sync(Base.metadata.create_all)
    yield engine
    await engine.dispose()

@pytest_asyncio.fixture
async def test_session(test_engine) -> AsyncSession:
    # Transactional rollback per test
    async with test_engine.connect() as conn:
        trans = await conn.begin()
        session = AsyncSession(bind=conn, expire_on_commit=False)
        yield session
        await session.close()
        await trans.rollback()

@pytest_asyncio.fixture
async def client(test_session) -> AsyncClient:
    app.dependency_overrides[get_db] = lambda: test_session
    async with AsyncClient(transport=ASGITransport(app=app), base_url="http://test") as ac:
        yield ac
    app.dependency_overrides.clear()
```

---

## Example Test Cases (`tests/test_api.py`)

```python
import pytest
from httpx import AsyncClient

@pytest.mark.asyncio
async def test_create_user(client: AsyncClient):
    payload = {"username": "john_doe", "email": "john@example.com"}
    response = await client.post("/api/v1/users/", json=payload)
    assert response.status_code == 201
    data = response.json()
    assert data["username"] == "john_doe"
    assert "id" in data

@pytest.mark.asyncio
async def test_auth_protected_route(client: AsyncClient):
    # Missing token returns 401 or 403
    response = await client.get("/api/v1/protected")
    assert response.status_code in [401, 403]
```

---

## Pytest Execution Commands

```bash
# Run all tests asynchronously with verbose output
pytest -v

# Run with test coverage report
pytest --cov=src --cov-report=term-missing

# Run only tests matching a specific expression
pytest -k "test_create"
```
