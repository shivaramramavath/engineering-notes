---
title: "Clean Code and Code Review Checklist for FastAPI Applications"
description: "Production readiness standards, Ruff configuration, strict type checking, and comprehensive code review checklists for enterprise FastAPI codebases."
fastapi_version: "0.115.x+"
python_version: "3.12+"
pydantic_version: "v2.10+"
tested_with: "ruff 0.8+, mypy 1.13+, pytest 8.3+"
free_threading_status: "Fully Compatible"
---

# Clean Code and Code Review Checklist for FastAPI Applications

## 1. Conceptual Foundation & Mental Model

### The Code Review Quality Gate
In high-velocity engineering organizations, code reviews and automated CI quality gates serve as the primary defensive barrier against technical debt, performance degradation, and security vulnerabilities. A FastAPI codebase requires specialized review criteria because asynchronous Python introduces unique concurrency hazards (event loop blocking, connection pool exhaustion) that do not exist in traditional synchronous frameworks like Django or Flask.

```
+-------------------------------------------------------------------------------+
|                    THE 5 PILLARS OF FASTAPI CODE REVIEWS                      |
|                                                                               |
|  1. Async Safety        -> Zero synchronous I/O or CPU blocks in `async def`  |
|  2. Architectural Clean -> Strict boundary separation (DTO vs Domain vs ORM)  |
|  3. Security & OWASP    -> No mass assignment; parameterized queries; auth     |
|  4. Resource Hygiene    -> Deterministic session disposal; connection bounds   |
|  5. Type Safety & Lint  -> Strict Mypy/Pyright compliance; clean Ruff rules    |
+-------------------------------------------------------------------------------+
```

---

## 2. Architecture & Internal Mechanics

The automated review pipeline shifts detection left: static linters and type checkers catch mechanical defects in sub-seconds, allowing human reviewers to focus on architectural invariants and domain logic.

```
[Developer Git Push]
         |
         v
+-------------------------------------------------------------+
|               STAGE 1: AUTOMATED STATIC ANALYSIS            |
|  - Ruff: Linting (E, F, B, UP, S, SIM, ASG) & Formatting    |
|  - Mypy / Pyright: Strict static type checking              |
|  - Import-Linter: Architectural boundary contracts         |
+-------------------------------------------------------------+
         |
         v (All Linters Pass)
+-------------------------------------------------------------+
|               STAGE 2: AUTOMATED TEST SUITE                 |
|  - Pytest with Pytest-Asyncio                               |
|  - Branch Coverage Check (>85%)                             |
|  - Mutation Testing on Domain (Mutmut)                      |
+-------------------------------------------------------------+
         |
         v (CI Green)
+-------------------------------------------------------------+
|               STAGE 3: HUMAN ARCHITECTURAL REVIEW           |
|  - Review against FastAPI Production Readiness Checklist    |
|  - Invariant validation, security boundaries, query plans   |
+-------------------------------------------------------------+
         |
         v
[Merge to Main & Deploy]
```

---

## 3. Comprehensive Syntax & Core Mechanics

### Production-Grade `pyproject.toml` (Ruff & Mypy Strict Configuration)

```toml
[tool.ruff]
target-version = "py312"
line-length = 100

[tool.ruff.lint]
select = [
    "E",    # Pycodestyle errors
    "W",    # Pycodestyle warnings
    "F",    # Pyflakes
    "I",    # Isort (Import sorting)
    "B",    # Flake8-bugbear (Common bug hazards)
    "C4",   # Flake8-comprehensions
    "UP",   # Pyupgrade (Modern Python 3.12+ syntax)
    "ASYNC",# Flake8-async (Asyncio pitfalls like blocking calls)
    "S",    # Flake8-bandit (Security audit)
    "SIM",  # Flake8-simplify
    "TCH",  # Flake8-type-checking (Move imports to TYPE_CHECKING)
    "PTH",  # Flake8-use-pathlib
]
ignore = ["S101"]  # Allow assert in test files

[tool.ruff.lint.isort]
known-first-party = ["app"]

[tool.mypy]
python_version = "3.12"
strict = true
warn_unused_configs = true
disallow_untyped_defs = true
disallow_any_generics = true
no_implicit_optional = true
check_untyped_defs = true
warn_return_any = true
show_error_codes = true
plugins = ["pydantic.mypy"]

[tool.pydantic-mypy]
init_forbid_extra = true
init_typed = true
warn_required_dynamic_aliases = true
```

---

## 4. Practical Implementation: 5 Verified Examples

### Example 1: The Automated Async Safety Review Check
Detecting synchronous blocking calls inside `async def` endpoints before merging.

```python
# app/api/v1/bad_code_review_sample.py
import time
import requests  # FORBIDDEN: Synchronous HTTP library in async def!
from fastapi import APIRouter

router = APIRouter()

# CODE REVIEW FLAG: ASYNC101 / ASYNC102
@router.get("/bad")
async def bad_endpoint():
    # BLOCKING EVENT LOOP HAZARD!
    time.sleep(1.0)
    res = requests.get("https://api.external.com")
    return res.json()

# REMEDIATED CODE:
import httpx
import anyio

@router.get("/good")
async def good_endpoint():
    # 1. Non-blocking sleep
    await anyio.sleep(1.0)
    # 2. Async HTTP client
    async with httpx.AsyncClient() as client:
        res = await client.get("https://api.external.com")
        return res.json()
```

---

### Example 2: The Architectural Boundary Review Check
Verifying that endpoints do not leak ORM models or accept raw dictionaries.

```python
# CODE REVIEW REJECTION:
@router.post("/users")
async def bad_create_user(payload: dict, db: AsyncSession = Depends(get_db)):
    user = UserORM(**payload)  # SECURITY HAZARD: Mass assignment vulnerability!
    db.add(user)
    await db.commit()
    return user  # HAZARD: Leaking password_hash and DetachedInstanceError!

# CODE REVIEW APPROVAL:
from app.schemas.user import UserCreateRequestDTO, UserResponseDTO
from app.services.user_service import UserService

@router.post("/users", response_model=UserResponseDTO, status_code=201)
async def good_create_user(
    payload: UserCreateRequestDTO,
    service: UserService = Depends(get_user_service),
):
    # Validated Pydantic DTO -> Injected Service -> Clean Response DTO
    user = await service.register_user(payload.email, payload.password)
    return UserResponseDTO.model_validate(user)
```

---

### Example 3: Database Session Hygiene Review Check
Ensuring that database sessions are managed via async context managers or dependency injection with guaranteed rollback on exception.

```python
# CODE REVIEW REJECTION: Leaked session without closing
@router.get("/items")
async def bad_items():
    session = AsyncSessionFactory()
    items = await session.execute(select(ItemORM))
    # Forgot session.close()! Connection pool exhausted in 5 minutes!
    return items.scalars().all()

# CODE REVIEW APPROVAL: Clean context manager
@router.get("/items")
async def good_items(session: AsyncSession = Depends(get_db_session)):
    # get_db_session yields session inside try/finally or async with
    stmt = select(ItemORM)
    result = await session.execute(stmt)
    return result.scalars().all()
```

---

### Example 4: The 15-Point Code Review Pull Request Checklist
A standardized markdown checklist embedded into GitHub / GitLab Pull Request templates.

```markdown
<!-- .github/pull_request_template.md -->
## FastAPI Code Review Checklist

### 1. Architecture & Boundaries
- [ ] No SQLAlchemy ORM models are returned directly from route handlers.
- [ ] No `fastapi.HTTPException` is raised inside the Domain or Service layers.
- [ ] Presentation DTOs, Domain Entities, and Persistence ORM models are separate.

### 2. Asynchronous Safety
- [ ] No `time.sleep()`, synchronous `requests`, or blocking file I/O inside `async def`.
- [ ] Heavy CPU-bound tasks are offloaded to `anyio.to_thread` or `ProcessPoolExecutor`.
- [ ] Background tasks do not outlive application lifespan or leak memory.

### 3. Database & Concurrency Hygiene
- [ ] No N+1 query patterns (uses `selectinload` or `joinedload` for relationships).
- [ ] Financial balance updates or inventory deductions use pessimistic/optimistic locks.
- [ ] Transactions are committed by the Service/UoW, never by individual repositories.

### 4. Security & OWASP
- [ ] Incoming request DTOs strictly whitelist permitted fields (no `**payload.dict()`).
- [ ] Endpoints enforce authentication and object-level authorization (IDOR check).
- [ ] Error messages do not leak internal SQL statements or stack traces.

### 5. Testing & Quality
- [ ] Every new endpoint has positive (2xx), negative (4xx), and edge-case tests.
- [ ] `app.dependency_overrides.clear()` is called in test fixture teardowns.
- [ ] Branch coverage does not drop below 85%.
```

---

### Example 5: Automated Pre-Commit Hook Configuration
Ensuring clean code before commits are ever pushed to remote repositories.

```yaml
# .pre-commit-config.yaml
repos:
  - repo: https://github.com/astral-sh/ruff-pre-commit
    rev: v0.8.0
    hooks:
      - id: ruff
        args: [--fix]
      - id: ruff-format
  - repo: https://github.com/pre-commit/mirrors-mypy
    rev: v1.13.0
    hooks:
      - id: mypy
        additional_dependencies: ["pydantic>=2.10"]
```

---

## 5. Edge Cases, Pitfalls & Failure Modes

1. **Unawaited Coroutines (`RuntimeWarning: coroutine was never awaited`)**: Forgetting `await` on an async database call (`session.commit()` instead of `await session.commit()`). The code executes without syntax errors, but the transaction is never committed, and a warning is printed to stderr. Catch this statically using Ruff rule `ASYNC100`.
2. **Global State Mutation in Routers**: Modifying module-level lists or dictionaries inside route handlers. Under high concurrency across multiple threads, this causes silent race conditions.
3. **Pydantic Model Shadowing**: Reusing attribute names that collide with internal Pydantic methods (e.g., naming a field `schema`, `validate`, or `model_dump`).
4. **Ignoring SQLAlchemy Generator Expressions in Yield**: Writing a database dependency that yields a session without wrapping it in `try ... finally` or `async with`. If an exception occurs, the connection is never closed and leaks indefinitely.

---

## 6. Common Mistakes Table

| Mistake | Root Cause | Impact | Correct Approach |
| :--- | :--- | :--- | :--- |
| **Using `requests` in `async def`** | Habit from Flask/Django | Blocks Uvicorn event loop; throughput drops from 3,000 RPS to 5 RPS. | Use `httpx.AsyncClient` or `aiohttp`. |
| **Omitting `response_model` on Routes** | Avoiding DTO definition | Leaks sensitive internal ORM attributes; breaks OpenAPI documentation. | Explicitly define `response_model=MyResponseDTO`. |
| **Unbounded Query Limits** | Writing `select(Item)` without `.limit()` | Out-of-memory (OOM) crash when database table grows to 1,000,000 rows. | Enforce pagination parameters with strict max limits (`le=100`). |
| **Swallowing Exceptions with `except: pass`** | Lazy error handling | Hides database deadlocks and bugs; makes root-cause analysis impossible. | Catch specific exceptions; log with tracebacks or re-raise domain errors. |
| **Committing Inside Repositories** | Violating Unit of Work | Multi-repository workflows cannot be rolled back atomically on failure. | Service layer or Unit of Work must control `commit()`. |
| **Skipping Strict Type Annotations** | Speed over safety | Silent runtime type bugs; autocompletion and Mypy validation disabled. | Enable strict Mypy and type all arguments and return values. |

---

## 7. Performance & Scalability Considerations

- **Ruff Execution Speed**: Ruff is written in Rust and executes 50x–100x faster than legacy Python linters (Flake8, Black, Isort). Running `ruff check .` across 10,000 files completes in under 0.2 seconds, making it ideal for pre-commit hooks and CI gates.
- **Selective Relationship Loading**: Enforce in code reviews that all SQLAlchemy queries with relationships explicitly declare their loading strategy (`selectinload()` for 1-to-many, `joinedload()` for 1-to-1). Queries that trigger N+1 lazy queries must be rejected in PR review.
- **Python 3.13+ Free-Threading**: Clean code avoiding shared mutable state enables seamless compatibility with free-threaded Python (`3.13t`), unlocking true multi-core parallel processing without code refactoring.

---

## 8. Security & OWASP Guardrails

- **OWASP A03 (Injection Prevention)**: Reject any PR that uses raw SQL string formatting (f-strings). All database queries must use parameterized SQLAlchemy `select()` statements or `text("...").bindparams()`.
- **OWASP A01 (Broken Object-Level Authorization)**: Every query fetching a user resource must filter by the authenticated user's ID or tenant ID. Reject any PR where an ID passed in a URL path parameter is fetched without verifying ownership.

---

## 9. Testing Strategies & Pytest Recipes

```python
# tests/test_code_quality_smoke.py
import subprocess
import pytest

def test_ruff_linting_passes():
    """Asserts that the codebase passes all Ruff linting checks cleanly."""
    result = subprocess.run(["ruff", "check", "app"], capture_output=True, text=True)
    assert result.returncode == 0, f"Ruff linting failed:\n{result.stdout}"

def test_mypy_type_checking_passes():
    """Asserts that Mypy strict type checking passes with zero errors."""
    result = subprocess.run(["mypy", "app"], capture_output=True, text=True)
    assert result.returncode == 0, f"Mypy strict checking failed:\n{result.stdout}"
```

---

## 10. Real-World Case Study / Production Incident

A fast-growing fintech company merged a seemingly harmless pull request containing a new analytics logging utility:
```python
@router.post("/payments")
async def process_payment(payload: PaymentRequest):
    # "Harmless" analytics logging added in PR
    import socket
    socket.gethostbyname("analytics.partner-internal.net")  # SYNCHRONOUS DNS RESOLUTION!
    return await payment_service.charge(payload)
```
During a DNS provider hiccup, the synchronous `socket.gethostbyname()` call blocked for 4.5 seconds. Because it was called inside `async def`, it blocked the single-threaded `uvloop` of each Uvicorn worker process completely. 

Within 30 seconds:
- All 16 Uvicorn workers across the Kubernetes cluster locked up.
- The platform failed to process over \$800,000 in payment transactions.
- Ingress controllers marked all pods unhealthy and terminated them, causing a cascading outage.

**Remediation**:
The engineering team integrated Ruff's `ASYNC` rules (`ASYNC100`, `ASYNC101`, `ASYNC102`) into their mandatory GitHub Actions pull-request gate. Any synchronous socket, sleep, or file call inside `async def` now instantly fails the CI build, permanently preventing event-loop-blocking code from reaching production.

---

## 11. Technical Interview Questions

<details>
<summary>1. What happens if a developer uses `time.sleep(1)` inside an `async def` endpoint in FastAPI?</summary>
In FastAPI, endpoints defined with `async def` execute directly on the single-threaded asyncio event loop (`uvloop`). Calling a synchronous blocking function like `time.sleep(1)` halts the entire event loop thread for that Uvicorn worker process. No other concurrent coroutines, database queries, or incoming HTTP connections assigned to that worker can make progress during that 1 second, causing extreme latency spikes and timeouts across all users.
</details>

<details>
<summary>2. Why does Ruff execute drastically faster than traditional Python linters like Flake8 and Black?</summary>
Ruff is written from the ground up in Rust and uses a unified AST (Abstract Syntax Tree) parser. Instead of running 10 separate Python processes (Black, Isort, Flake8, Bandit, Pyupgrade) that repeatedly parse and serialize Python files, Ruff parses the AST once in native machine code, running hundreds of linting rules in parallel across CPU cores in milliseconds.
</details>

<details>
<summary>3. What is the N+1 query problem in SQLAlchemy, and how do you detect it during code review?</summary>
The N+1 query problem occurs when an application executes 1 query to fetch $N$ parent records, and then executes an additional query for *each* parent record to fetch its child relationship inside a loop (totaling $N + 1$ database queries). During code review, look for relationships accessed on ORM models inside loops or Pydantic schemas without an explicit `selectinload()` or `joinedload()` in the original query.
</details>

<details>
<summary>4. What are the key Ruff linting rule groups every FastAPI project should enable?</summary>
- `E` and `F`: Core PEP 8 and Python syntax errors.
- `B` (flake8-bugbear): Catches mutable default arguments and structural bug hazards.
- `ASYNC` (flake8-async): Detects blocking I/O, synchronous sleeps, and async misuse.
- `S` (flake8-bandit): Detects security hazards (hardcoded passwords, insecure SQL).
- `UP` (pyupgrade): Enforces modern Python 3.12+ syntax (e.g., `list[str]` instead of `typing.List[str]`).
</details>

<details>
<summary>5. Why should code reviewers reject endpoints that accept `payload: dict`?</summary>
Accepting an untyped dictionary bypasses Pydantic schema validation. It disables type safety, allows mass assignment vulnerabilities where attackers inject unauthorized fields, prevents automatic OpenAPI documentation generation, and forces the developer to write manual defensive dictionary parsing logic.
</details>

<details>
<summary>6. What is the difference between Mypy and Pyright?</summary>
Mypy is Python's original reference static type checker, developed by Dropbox and the Python core team. Pyright is Microsoft's ultra-fast static type checker written in TypeScript, powering VS Code's Pylance extension. Pyright is generally faster and offers more aggressive type inference, while Mypy has a wider plugin ecosystem (including the official Pydantic Mypy plugin).
</details>

<details>
<summary>7. How do you prevent a background task from holding onto an open database connection after the response is sent?</summary>
Do not pass the request's `AsyncSession` into `background_tasks.add_task()`. The request session is closed and disposed when the HTTP response returns. Background tasks must instantiate their own independent session via the session factory and manage its lifecycle within their own `async with` context block.
</details>

<details>
<summary>8. Why should code reviews enforce that all route parameters have explicit validation constraints?</summary>
Unbounded parameters allow denial-of-service (DoS) attacks. For example, a string parameter without `max_length` can be sent a 50MB string, saturating memory. An integer `limit` parameter without `le=100` can be passed `?limit=10000000`, causing the database to execute a massive table scan and crash the server with an Out-of-Memory (OOM) error.
</details>

---

## 12. Practical Exercises & Solutions

<details>
<summary>Exercise 1 (Easy): Identify the Asynchronous Code Review Smells</summary>

**Task**: Identify the three major performance/async smells in this endpoint:
```python
@router.get("/bad")
async def process_data():
    time.sleep(2)
    f = open("data.txt", "r")
    data = f.read()
    f.close()
    return {"data": data}
```

**Solution**:
1. `time.sleep(2)` blocks the entire event loop thread; must be `await anyio.sleep(2)`.
2. Synchronous file I/O `open()` blocks the event loop; must use `aiofiles` or offload via `anyio.to_thread.run_sync`.
3. Raw file handle opened without context manager; if an error occurs, the file descriptor leaks.
</details>

<details>
<summary>Exercise 2 (Easy): Configure Basic Ruff Linting</summary>

**Task**: Write a minimal `pyproject.toml` snippet enabling Ruff with `E`, `F`, `B`, and `ASYNC` rule groups for Python 3.12.

**Solution**:
```toml
[tool.ruff]
target-version = "py312"

[tool.ruff.lint]
select = ["E", "F", "B", "ASYNC"]
```
</details>

<details>
<summary>Exercise 3 (Medium): Refactor Unbounded Query Endpoint</summary>

**Task**: Refactor this unsafe endpoint to enforce safe pagination with a max limit of 100:
```python
@router.get("/users")
async def list_users(skip: int = 0, limit: int = 10000, db: AsyncSession = Depends(get_db)):
    return (await db.execute(select(UserORM).offset(skip).limit(limit))).scalars().all()
```

**Solution**:
```python
from fastapi import Query

@router.get("/users", response_model=list[UserResponseDTO])
async def list_users(
    skip: int = Query(default=0, ge=0),
    limit: int = Query(default=20, gt=0, le=100),
    db: AsyncSession = Depends(get_db)
):
    stmt = select(UserORM).offset(skip).limit(limit)
    result = await db.execute(stmt)
    records = result.scalars().all()
    return [UserResponseDTO.model_validate(r) for r in records]
```
</details>

<details>
<summary>Exercise 4 (Medium): Enforce Eager Loading to Prevent N+1</summary>

**Task**: Given an `AuthorORM` with a `books` relationship, write a query that loads all authors along with their books using `selectinload` to prevent N+1 query execution.

**Solution**:
```python
from sqlalchemy import select
from sqlalchemy.orm import selectinload

async def get_authors_with_books(session: AsyncSession):
    stmt = (
        select(AuthorORM)
        .options(selectinload(AuthorORM.books))
        .limit(50)
    )
    result = await session.execute(stmt)
    return result.scalars().all()
```
</details>

<details>
<summary>Exercise 5 (Hard): AST Linter for Blocking Calls</summary>

**Task**: Write a Python function using `ast` that analyzes a Python file and returns an error if `time.sleep` is called inside any `async def` function.

**Solution**:
```python
import ast

def find_blocking_sleep(code_str: str) -> list[int]:
    tree = ast.parse(code_str)
    violations = []

    for node in ast.walk(tree):
        if isinstance(node, ast.AsyncFunctionDef):
            for sub_node in ast.walk(node):
                if isinstance(sub_node, ast.Call):
                    func = sub_node.func
                    if isinstance(func, ast.Attribute) and func.attr == "sleep":
                        if isinstance(func.value, ast.Name) and func.value.id == "time":
                            violations.append(sub_node.lineno)
    return violations
```
</details>

<details>
<summary>Exercise 6 (Hard): Pre-Merge CI Shell Script</summary>

**Task**: Write a bash script `run_ci_quality_gate.sh` that runs Ruff formatting check, Ruff linter, Mypy strict mode, and Pytest with 85% coverage enforcement, exiting with code 1 if any step fails.

**Solution**:
```bash
#!/usr/bin/env bash
set -e

echo "=== 1. Checking Code Formatting with Ruff ==="
ruff format --check app tests

echo "=== 2. Running Ruff Linter ==="
ruff check app tests

echo "=== 3. Running Mypy Static Type Checker ==="
mypy app

echo "=== 4. Running Pytest with Branch Coverage ==="
pytest --cov=app --cov-branch --cov-fail-under=85 tests

echo "=== ALL QUALITY GATES PASSED SUCCESSFULLY ==="
```
</details>
