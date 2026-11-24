# FastAPI Track Architecture Decision Records (FASTAPI_DECISIONS.md)

> Formal record of architectural decisions, standards, and deprecation policies adopted across the FastAPI curriculum (`03-backend/fastapi/`).

---

## ADR 001: Framework & Runtime Baseline Standards
- **Status**: Accepted
- **Context**: FastAPI evolved significantly with Pydantic v2 and the introduction of the FastAPI CLI. A modern curriculum must target the current LTS and stable lines while clearly distinguishing legacy idioms.
- **Decision**: Target **FastAPI 0.165.x**, **Starlette 1.10.x**, **Pydantic v2.14.x**, and **Python 3.12+ (3.13 / 3.14 compatible)** as the authoritative baseline. Document free-threaded Python 3.14t (No-GIL, PEP 703) capabilities in concurrency notes.
- **Consequences**: All code examples use Pydantic v2 syntax (`ConfigDict`, `field_validator`, `model_dump()`), deprecating v1 patterns (`class Config`, `@validator`).

---

## ADR 002: Dependency Injection using `typing.Annotated`
- **Status**: Accepted
- **Context**: In early FastAPI versions, dependencies were defined via default parameters (`db: Session = Depends(get_db)`). This breaks static type checkers and mypy type inference when default values don't match the declared type.
- **Decision**: All dependencies must be authored using modern `typing.Annotated` (PEP 593):
  ```python
  DbSession = Annotated[AsyncSession, Depends(get_db)]
  def get_user(db: DbSession): ...
  ```
- **Consequences**: Cleaner code reusability, perfect static type inference without `# type: ignore`, and full alignment with FastAPI official recommendations.

---

## ADR 003: Application Lifespan over Deprecated `on_event`
- **Status**: Accepted
- **Context**: Early FastAPI versions used `@app.on_event("startup")` and `@app.on_event("shutdown")`. Starlette and FastAPI officially deprecated `on_event` in favor of ASGI Lifespan context managers.
- **Decision**: All startup/shutdown resource orchestration (database connection pools, Redis clients, ML model loading, background workers) must use:
  ```python
  @asynccontextmanager
  async def lifespan(app: FastAPI):
      # Startup logic
      yield
      # Teardown logic
  ```
- **Consequences**: Guarantees deterministic cleanup order, proper exception handling during teardown, and Starlette 1.x compliance.

---

## ADR 004: SQLAlchemy 2.0 Async Engine & Driver Standard
- **Status**: Accepted
- **Context**: SQLAlchemy 2.0 represents a major paradigm shift to explicit typed queries (`select()`, `session.scalars()`). Legacy 1.4 `session.query()` patterns are deprecated.
- **Decision**: Standardize on `create_async_engine` using `asyncpg` (`postgresql+asyncpg://`) or modern `psycopg` (`postgresql+psycopg://`). Require `sqlalchemy[asyncio]` (incorporating `greenlet`).
- **Consequences**: Eliminates blocking synchronous database calls inside async endpoints, preventing event loop thread starvation.

---

## ADR 005: Packaging & Development Server Tooling (`uv` and FastAPI CLI)
- **Status**: Accepted
- **Context**: The Python packaging ecosystem transitioned to `uv` (Astral), and FastAPI introduced the official `fastapi` CLI command (`fastapi dev`, `fastapi run`) via `fastapi[standard]`.
- **Decision**: Teach and document `uv` as the primary project/dependency tool. Demonstrate both `fastapi dev` and raw `uvicorn main:app --reload` with exact flags and production trade-offs.
- **Consequences**: Modern, lightning-fast developer workflows aligned with contemporary Python backend engineering standards.

---

## ADR 006: Strict Markdown-Only Repository Policy
- **Status**: Accepted
- **Context**: The repository must contain only documentation notes and curriculum markdown files.
- **Decision**: Inside `03-backend/fastapi/`, ONLY `.md` files (and root `tree.txt`) may be created or modified. All source code, Dockerfiles, and configs must be embedded within fenced code blocks. Testing occurs strictly in external scratch environments.
- **Consequences**: Zero repo pollution with ephemeral python artifacts, virtual environments, `.pyc` files, or lockfiles.
