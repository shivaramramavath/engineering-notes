# Project Template — FastAPI Backend Curriculum

> **Source of Truth** for all production reference applications in `03-backend/fastapi/17-projects/`. Every project folder must contain a `README.md` conforming to this structure, accompanied by modular architecture specifications and runnable source blocks.

---

```markdown
# Project [XX]: [Project Title] (`[project-slug]`)

> [Comprehensive summary of the application domain, architecture, FastAPI features demonstrated, and production-grade capabilities.]

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-fastapi-[xx]-[slug]` |
| **FastAPI Version** | FastAPI 0.165.0+ |
| **Pydantic Version** | Pydantic v2.14.4+ (pydantic-settings 2.16.3+) |
| **Python Baseline** | Python 3.12+ (Node/Python 3.14 compatible) |
| **ASGI Server** | Uvicorn 0.54+ / FastAPI CLI (`fastapi run`) |
| **Database & ORM** | PostgreSQL 16 (Async SQLAlchemy 2.0 / asyncpg) + Alembic |
| **Queue & Cache** | Redis 7 + Celery / Taskiq / Arq |
| **Validation & Auth** | Pydantic v2 + Argon2id + RS256 JWT / Passkeys |
| **Testing** | pytest-asyncio + httpx.AsyncClient + Testcontainers |
| **Complexity Level** | Enterprise Production Grade |

---

## 1. Architectural Overview & Domain Design

### 1.1 System Architecture & Data Flow
[Detailed explanation of the modular architecture, dependency injection boundaries, ports & adapters, transaction scope, and external integrations.]

```
+----------------------------------------------------------------------------------------------------+
|                                    FastAPI Modular Architecture                                     |
|                                                                                                    |
|  [ Client / API Gateway ] ───(HTTP/REST)───> [ FastAPI APIRouter ]                                  |
|                                                     │                                              |
|                                                     ▼                                              |
|  [ Pydantic v2 Body Validation ] <────────────── [ Depends() Auth Guard ]                          |
|            │                                                                                       |
|            ▼                                                                                       |
|  [ Application Service Layer ] ───(Constructor / Annotated Dependency Injection)                    |
|            │                                                                                       |
|            ├───> [ Repository / Data Access Layer (SQLAlchemy 2.0 Async) ]                         |
|            │             │                                                                         |
|            │             ▼                                                                         |
|            │      [ PostgreSQL DB with Connection Pooling (asyncpg) ]                              |
|            │                                                                                       |
|            └───> [ Background Task / Redis Queue Producers ]                                       |
+----------------------------------------------------------------------------------------------------+
```

### 1.2 Directory Layout
```
[project-slug]/
├── README.md
├── pyproject.toml
├── alembic.ini
├── Dockerfile
├── docker-compose.yml
├── migrations/
│   ├── env.py
│   └── script.py.mako
├── src/
│   ├── __init__.py
│   ├── main.py
│   ├── config.py
│   ├── database.py
│   ├── domain/
│   │   ├── models.py
│   │   └── schemas.py
│   ├── repositories/
│   ├── services/
│   └── api/
│       ├── dependencies.py
│       └── v1/
└── tests/
    ├── conftest.py
    └── test_api.py
```

---

## 2. Complete Production Implementation

[All source code must be provided in full within fenced code blocks with explicit file headers, containing zero ellipses (`...`) or `pass` stubs.]

### 2.1 Configuration & Dependencies (`pyproject.toml`)
```toml
# pyproject.toml
```

### 2.2 Core Application Entrypoint (`src/main.py`)
```python
# src/main.py
```

### 2.3 Settings & Lifespan (`src/config.py`)
```python
# src/config.py
```

### 2.4 Database Engine & Session (`src/database.py`)
```python
# src/database.py
```

### 2.5 Domain Schemas & Models (`src/domain/`)
```python
# Pydantic schemas and SQLAlchemy models
```

### 2.6 Service & Repository Layer (`src/services/`)
```python
# Business logic and database operations
```

### 2.7 API Routers & Dependencies (`src/api/`)
```python
# Endpoints and Annotated dependencies
```

---

## 3. Automated Testing Suite

### 3.1 Test Fixtures (`tests/conftest.py`)
```python
# Pytest async client and database fixtures
```

### 3.2 Integration & API Tests (`tests/test_api.py`)
```python
# Full API test coverage
```

---

## 4. Production Infrastructure & Deployment

### 4.1 Multi-Stage Dockerfile
```dockerfile
# Dockerfile
```

### 4.2 Docker Compose Stack
```yaml
# docker-compose.yml
```

---

## 5. Operational Runbook & Verification
[Exact terminal reproduction commands, health check verifications, sample curl requests, and troubleshooting notes.]
```bash
# Reproduction instructions
```
```
