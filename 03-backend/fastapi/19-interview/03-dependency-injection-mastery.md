# Technical Interview Mastery: FastAPI Dependency Injection Deep Dive

## Overview & Evaluation Criteria

FastAPI's Dependency Injection (DI) system is its crowning architectural achievement. Interview questions evaluate candidates on DI resolution mechanics, sub-dependency caching, cleanup order in generator dependencies (`yield`), request-scoped transactions, and testing overrides (`dependency_overrides`).

---

## Technical Q&A Deep Dive

<details>
<summary><strong>Q1: How does FastAPI resolve nested dependencies internally? What is the role of <code>use_cache=True</code>?</strong></summary>

### Answer:
- **Dependency Graph (DAG)**: When a route is invoked, FastAPI builds a directed acyclic graph of all parameters declared with `Depends()`.
- It performs a topological sort to resolve dependencies from leaves up to the root route function.
- **`use_cache=True` (Default)**:
  - If multiple dependencies or route handlers in the same request declare `Depends(get_db_session)`, FastAPI executes `get_db_session` *exactly once*.
  - It caches the resulting value in an internal dictionary keyed by the callable `(get_db_session, scope)`.
  - All subsequent references in that request receive the identical instance.
  - Setting `use_cache=False` forces re-execution on every injection point (useful for generating unique random nonces or fresh transient instances).

</details>

<details>
<summary><strong>Q2: Explain the exact execution order and lifecycle of a generator dependency using <code>yield</code> (e.g. database sessions). What happens if an unhandled exception occurs in the route?</strong></summary>

### Answer:
```python
async def get_db():
    session = AsyncSession()
    try:
        yield session # Execution pauses here; control transfers to route
        await session.commit()
    except Exception:
        await session.rollback()
        raise
    finally:
        await session.close()
```
- **Lifecycle Flow**:
  1. Code before `yield` runs *before* the route handler.
  2. The yielded value (`session`) is injected into the route handler.
  3. The route handler executes and produces a response.
  4. Code after `yield` executes *after* the response is generated.
- **Exception Propagation**:
  - If an unhandled exception occurs in the route handler, FastAPI re-raises that exception inside the generator at the point of `yield` (equivalent to calling `generator.throw(exc)`).
  - The `except Exception:` block catches it, allowing rollback and cleanup before the exception propagates to Starlette exception handlers.

</details>

<details>
<summary><strong>Q3: How does FastAPI's DI enable seamless unit and integration testing without mocking frameworks?</strong></summary>

### Answer:
- FastAPI exposes the `app.dependency_overrides` dictionary.
- During tests, developers map the production dependency callable directly to a test fixture callable:
```python
app.dependency_overrides[get_db] = lambda: test_in_memory_session
```
- **Advantages over `unittest.mock.patch`**:
  1. **Refactoring-Safe**: Survives module renaming and import path reorganizations.
  2. **Type Safe**: The override function can be statically verified.
  3. **No Patch Leaks**: Restored cleanly after each test via `app.dependency_overrides.clear()`.

</details>

<details>
<summary><strong>Q4: Can a class be used as a dependency in FastAPI? What are the architectural advantages of class-based dependencies?</strong></summary>

### Answer:
Yes. Any Python callable can be used with `Depends()`. A class is a callable (its `__init__` or `__call__` executes on instantiation):
```python
class CommonPagination:
    def __init__(self, page: int = 1, size: int = 20):
        self.page = page
        self.size = size
        self.offset = (page - 1) * size

@app.get("/items")
def list_items(pagination: CommonPagination = Depends(CommonPagination)):
    return {"offset": pagination.offset, "limit": pagination.size}
```
- **Advantages**:
  - Encapsulates common query logic (pagination, search filters, sorting) into reusable, strongly-typed domain objects.
  - Supports dependency inheritance and parameterized factories (e.g. `RoleChecker(["ADMIN", "SUPERUSER"])`).
</details>
