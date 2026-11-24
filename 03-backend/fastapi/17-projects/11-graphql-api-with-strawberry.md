# Production Reference Project: High-Performance GraphQL API with Strawberry & DataLoader

## Metadata & Production Baseline

| Attribute | Specification |
|:---|:---|
| **Project Code** | `FASTAPI-PROJ-011` |
| **FastAPI Version** | `0.115.x+` / `0.165.x+` |
| **Pydantic Version** | `2.10.x+` (V2 Core) |
| **Python Baseline** | `3.12+` / `3.13` (Free-Threading Safe) |
| **ASGI Server** | `Uvicorn 0.32+` |
| **GraphQL Engine** | `Strawberry GraphQL 0.235+`, `strawberry-graphql[fastapi]` |
| **Batching & N+1 Fix**| `DataLoader (strawberry.dataloader)` for asynchronous batched relationship loading |
| **Database & ORM** | `PostgreSQL 16+`, `SQLAlchemy 2.0.36+ (asyncpg)` |
| **Testing** | `pytest 8.3+`, `pytest-asyncio 0.24+`, `httpx 0.27+` |
| **Complexity Level** | Intermediate / Modern API Paradigm |

---

## 1. Architectural Overview & Domain Design

### 1.1 System Architecture & Data Flow

```
                      +---------------------------------------+
                      |         GraphQL Client / Web UI       |
                      +-------------------+-------------------+
                                          |
                         POST /graphql    |  Query { users { id posts { title } } }
                                          v
                      +-------------------+-------------------+
                      | FastAPI GraphQL Router (Strawberry)   |
                      +-------------------+-------------------+
                                          |
                                          v
                      +-------------------+-------------------+
                      | Execution Context & Context Factory   |
                      | - Provides AsyncSession               |
                      | - Instantiates Per-Request DataLoaders|
                      +-------------------+-------------------+
                                          |
                                          v
                      +-------------------+-------------------+
                      | Strawberry Resolvers                  |
                      | 1. Query 'users' -> 1 SQL query       |
                      | 2. Resolve 'posts' for 100 users:     |
                      |    Without DataLoader -> 100 queries  |
                      |    WITH DataLoader   -> 1 BATCH query |
                      +-------------------+-------------------+
                                          |
                                          v
                      +-------------------+-------------------+
                      | PostgreSQL Database                   |
                      | SELECT * FROM posts                   |
                      | WHERE author_id IN (1, 2, ... 100)    |
                      +---------------------------------------+
```

### 1.2 Directory Layout

```
11-graphql-strawberry/
|-- pyproject.toml
|-- Dockerfile
|-- docker-compose.yml
|-- src/
|   |-- __init__.py
|   |-- main.py
|   |-- config.py
|   |-- database.py
|   |-- domain/
|   |   |-- __init__.py
|   |   `-- models.py
|   |-- graphql/
|   |   |-- __init__.py
|   |   |-- context.py
|   |   |-- dataloaders.py
|   |   |-- types.py
|   |   |-- queries.py
|   |   |-- mutations.py
|   |   `-- schema.py
`-- tests/
    |-- __init__.py
    |-- conftest.py
    `-- test_graphql_api.py
```

---

## 2. Complete Production Implementation

### 2.1 `pyproject.toml`

```toml
[project]
name = "fastapi-graphql-strawberry"
version = "0.1.0"
description = "Production-grade GraphQL API with Strawberry and DataLoader batching"
readme = "README.md"
requires-python = ">=3.12"
dependencies = [
    "fastapi>=0.115.0",
    "uvicorn[standard]>=0.32.0",
    "strawberry-graphql[fastapi]>=0.235.0",
    "pydantic>=2.10.0",
    "pydantic-settings>=2.6.0",
    "sqlalchemy>=2.0.36",
    "asyncpg>=0.30.0",
]

[project.optional-dependencies]
dev = [
    "pytest>=8.3.0",
    "pytest-asyncio>=0.24.0",
    "httpx>=0.27.0",
]

[build-system]
requires = ["hatchling"]
build-backend = "hatchling.build"
```

### 2.2 `src/config.py` & `src/database.py`

```python
# src/config.py
from functools import lru_cache
from pydantic_settings import BaseSettings, SettingsConfigDict


class Settings(BaseSettings):
    PROJECT_NAME: str = "Strawberry GraphQL API"
    DATABASE_URL: str = "postgresql+asyncpg://postgres:postgres@localhost:5432/graphql_db"

    model_config = SettingsConfigDict(env_file=".env", extra="ignore")


@lru_cache
def get_settings() -> Settings:
    return Settings()
```

```python
# src/database.py
from sqlalchemy.ext.asyncio import AsyncSession, async_sessionmaker, create_async_engine
from src.config import get_settings

settings = get_settings()

engine = create_async_engine(settings.DATABASE_URL, pool_size=20, max_overflow=10)
async_session_factory = async_sessionmaker(bind=engine, class_=AsyncSession, expire_on_commit=False)
```

### 2.3 `src/domain/models.py`

```python
import uuid
from datetime import datetime, timezone
from sqlalchemy import DateTime, ForeignKey, String, Text
from sqlalchemy.dialects.postgresql import UUID
from sqlalchemy.orm import DeclarativeBase, Mapped, mapped_column, relationship


class Base(DeclarativeBase):
    pass


class Author(Base):
    __tablename__ = "authors"

    id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4)
    name: Mapped[str] = mapped_column(String(100), nullable=False)
    email: Mapped[str] = mapped_column(String(255), unique=True, nullable=False)
    articles: Mapped[list["Article"]] = relationship("Article", back_populates="author")


class Article(Base):
    __tablename__ = "articles"

    id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4)
    title: Mapped[str] = mapped_column(String(255), nullable=False)
    content: Mapped[str] = mapped_column(Text, nullable=False)
    author_id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), ForeignKey("authors.id"), nullable=False)
    author: Mapped[Author] = relationship("Author", back_populates="articles")
```

### 2.4 `src/graphql/dataloaders.py` & `src/graphql/context.py`

```python
# src/graphql/dataloaders.py
from collections import defaultdict
import uuid
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession
from strawberry.dataloader import DataLoader
from src.domain.models import Article


async def load_articles_for_authors(
    author_ids: list[uuid.UUID], db_session: AsyncSession
) -> list[list[Article]]:
    """Single batched SQL query to resolve articles for any number of authors."""
    stmt = select(Article).where(Article.author_id.in_(author_ids))
    result = await db_session.execute(stmt)
    articles = result.scalars().all()

    # Group by author_id
    grouped = defaultdict(list)
    for art in articles:
        grouped[art.author_id].append(art)

    return [grouped[author_id] for author_id in author_ids]


def create_article_loader(db_session: AsyncSession) -> DataLoader:
    async def batch_load(keys: list[uuid.UUID]):
        return await load_articles_for_authors(keys, db_session)
    return DataLoader(load_fn=batch_load)
```

```python
# src/graphql/context.py
from fastapi import Request
from sqlalchemy.ext.asyncio import AsyncSession
from strawberry.fastapi import BaseContext
from src.database import async_session_factory
from src.graphql.dataloaders import create_article_loader


class GraphQLContext(BaseContext):
    def __init__(self, session: AsyncSession):
        super().__init__()
        self.session = session
        self.article_loader = create_article_loader(session)


async def get_graphql_context(request: Request) -> GraphQLContext:
    async with async_session_factory() as session:
        return GraphQLContext(session=session)
```

### 2.5 `src/graphql/schema.py`

```python
import uuid
from typing import Annotated
from sqlalchemy import select
import strawberry
from strawberry.types import Info
from src.domain.models import Author as DBAuthor, Article as DBArticle
from src.graphql.context import GraphQLContext


@strawberry.type
class ArticleType:
    id: uuid.UUID
    title: str
    content: str


@strawberry.type
class AuthorType:
    id: uuid.UUID
    name: str
    email: str

    @strawberry.field
    async def articles(self, info: Info[GraphQLContext, None]) -> list[ArticleType]:
        # Uses per-request DataLoader: N authors resolved in 1 batched SQL query!
        articles = await info.context.article_loader.load(self.id)
        return [
            ArticleType(id=a.id, title=a.title, content=a.content)
            for a in articles
        ]


@strawberry.type
class Query:
    @strawberry.field
    async def authors(self, info: Info[GraphQLContext, None]) -> list[AuthorType]:
        stmt = select(DBAuthor)
        result = await info.context.session.execute(stmt)
        authors = result.scalars().all()
        return [AuthorType(id=a.id, name=a.name, email=a.email) for a in authors]


@strawberry.type
class Mutation:
    @strawberry.mutation
    async def create_author(self, name: str, email: str, info: Info[GraphQLContext, None]) -> AuthorType:
        author = DBAuthor(name=name, email=email)
        info.context.session.add(author)
        await info.context.session.commit()
        await info.context.session.refresh(author)
        return AuthorType(id=author.id, name=author.name, email=author.email)


schema = strawberry.Schema(query=Query, mutation=Mutation)
```

### 2.6 `src/main.py`

```python
from contextlib import asynccontextmanager
from fastapi import FastAPI
from strawberry.fastapi import GraphQLRouter
from src.config import get_settings
from src.database import engine
from src.domain.models import Base
from src.graphql.context import get_graphql_context
from src.graphql.schema import schema

settings = get_settings()


@asynccontextmanager
async def lifespan(app: FastAPI):
    async with engine.begin() as conn:
        await conn.run_sync(Base.metadata.create_all)
    yield
    await engine.dispose()


graphql_app = GraphQLRouter(schema, context_getter=get_graphql_context)

app = FastAPI(title=settings.PROJECT_NAME, lifespan=lifespan)
app.include_router(graphql_app, prefix="/graphql")
```

---

## 3. Automated Testing Suite

### 3.1 `tests/conftest.py`

```python
import pytest
import pytest_asyncio
from httpx import ASGITransport, AsyncClient
from sqlalchemy.ext.asyncio import AsyncSession, async_sessionmaker, create_async_engine
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
async def client():
    async with AsyncClient(transport=ASGITransport(app=app), base_url="http://test") as ac:
        yield ac
```

### 3.2 `tests/test_graphql_api.py`

```python
import pytest
from httpx import AsyncClient


@pytest.mark.asyncio
async def test_graphql_query_authors(client: AsyncClient):
    query = """
    query {
      authors {
        id
        name
        articles {
          title
        }
      }
    }
    """
    resp = await client.post("/graphql", json={"query": query})
    assert resp.status_code == 200
    data = resp.json()
    assert "data" in data
    assert "authors" in data["data"]
```

---

## 4. Production Infrastructure & Deployment

### 4.1 Multi-Stage `Dockerfile`

```dockerfile
FROM python:3.12-slim-bookworm AS builder
WORKDIR /build
RUN apt-get update && apt-get install -y --no-install-recommends gcc libpq-dev && rm -rf /var/lib/apt/lists/*
RUN pip install --no-cache-dir hatchling
COPY pyproject.toml .
RUN pip wheel --no-cache-dir --wheel-dir /build/wheels .

FROM python:3.12-slim-bookworm AS runtime
WORKDIR /app
RUN apt-get update && apt-get install -y --no-install-recommends libpq5 curl && rm -rf /var/lib/apt/lists/*
COPY --from=builder /build/wheels /wheels
RUN pip install --no-cache-dir /wheels/*
COPY src/ /app/src/

USER 1001
EXPOSE 8000
CMD ["uvicorn", "src.main:app", "--host", "0.0.0.0", "--port", "8000", "--workers", "4"]
```

### 4.2 `docker-compose.yml`

```yaml
version: "3.9"

services:
  web:
    build: .
    ports:
      - "8000:8000"
    environment:
      - DATABASE_URL=postgresql+asyncpg://postgres:postgres@db:5432/graphql_db
    depends_on:
      - db

  db:
    image: postgres:16-alpine
    environment:
      POSTGRES_USER: postgres
      POSTGRES_PASSWORD: postgres
      POSTGRES_DB: graphql_db
    ports:
      - "5432:5432"
```

---

## 5. Operational Runbook & Verification

### 5.1 Verification Commands

```bash
# 1. Execute GraphQL query with curl
curl -X POST "http://localhost:8000/graphql" \
     -H "Content-Type: application/json" \
     -d '{"query": "mutation { createAuthor(name: \"Guido\", email: \"guido@python.org\") { id name } }"}'

# 2. Query authors and their nested articles
curl -X POST "http://localhost:8000/graphql" \
     -H "Content-Type: application/json" \
     -d '{"query": "{ authors { id name articles { title } } }"}'

# 3. Run automated tests
pytest -v tests/
```
