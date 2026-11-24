# Quick Reference: Dependency Injection Recipes Cheatsheet

## Fundamental Patterns

### 1. Simple Callable Dependency
```python
from typing import Annotated
from fastapi import Depends

def get_query_token(token: str):
    if token != "secret":
        raise HTTPException(status_code=400, detail="Invalid token")
    return token

@app.get("/items")
def read_items(token: Annotated[str, Depends(get_query_token)]):
    return {"token": token}
```

### 2. Generator Dependency with Cleanup (`yield`)
```python
from collections.abc import AsyncGenerator
from sqlalchemy.ext.asyncio import AsyncSession

async def get_db() -> AsyncGenerator[AsyncSession, None]:
    async with async_session_factory() as session:
        try:
            yield session
            await session.commit()
        except Exception:
            await session.rollback()
            raise
```

### 3. Class-Based Parameterized Dependency
```python
class RoleChecker:
    def __init__(self, allowed_roles: list[str]):
        self.allowed_roles = allowed_roles

    def __call__(self, user: User = Depends(get_current_user)):
        if user.role not in self.allowed_roles:
            raise HTTPException(status_code=403, detail="Forbidden")
        return user

admin_guard = RoleChecker(["ADMIN", "SUPERUSER"])

@app.delete("/system/purge", dependencies=[Depends(admin_guard)])
async def purge_system():
    return {"status": "purged"}
```

---

## Testing Dependency Overrides Pattern

```python
import pytest
from httpx import ASGITransport, AsyncClient
from src.main import app
from src.database import get_db

@pytest.mark.asyncio
async def test_endpoint(test_mock_session):
    # 1. Override production dependency
    app.dependency_overrides[get_db] = lambda: test_mock_session

    async with AsyncClient(transport=ASGITransport(app=app), base_url="http://test") as client:
        resp = await client.get("/users")
        assert resp.status_code == 200

    # 2. Always clean up after test
    app.dependency_overrides.clear()
```
