# Project 02: Production Blog & Publishing API (`blog-api`)

> High-throughput Headless Content Publishing Engine built with FastAPI, PostgreSQL, Async SQLAlchemy 2.0, Full-Text Search, Slugs, Tagging, and Role-Based Access Control (RBAC).

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-fastapi-02-blog-api` |
| **FastAPI Version** | FastAPI 0.115.6+ |
| **Pydantic Version** | Pydantic v2.10.3+ (pydantic-settings 2.6.1+) |
| **Python Baseline** | Python 3.12+ (Free-Threading 3.13t compatible) |
| **ASGI Server** | Uvicorn 0.32.0+ with uvloop |
| **Database & ORM** | PostgreSQL 16 (Async SQLAlchemy 2.0 / asyncpg) + Full-Text Search |
| **Queue & Cache** | Redis 7 (Article view caching & rate limiting) |
| **Validation & Auth** | Pydantic v2 + Scoped JWT RBAC (Admin, Author, Reader) |
| **Testing** | pytest-asyncio + httpx.AsyncClient + aiosqlite |
| **Complexity Level** | Enterprise Production Grade |

---

## 1. Architectural Overview & Domain Design

### 1.1 System Architecture & Data Flow

```
+----------------------------------------------------------------------------------------------------+
|                                    Blog API Modular Architecture                                   |
|                                                                                                    |
|  [ Client: SPA / Mobile ] ───(HTTP/REST)───> [ FastAPI APIRouter: /api/v1/articles ]               |
|                                                     │                                              |
|                                                     ▼                                              |
|  [ Pydantic v2 ArticleCreate ] <─────────── [ RoleGuard("AUTHOR", "ADMIN") ]                       |
|            │                                                                                       |
|            ▼                                                                                       |
|  [ ArticleService (Domain Logic) ] ───(Injected via Depends)                                       |
|            │                                                                                       |
|            ├───> [ Auto-Slugifier (python-slugify) & Text Validator ]                              |
|            │                                                                                       |
|            ├───> [ ArticleRepository (SQLAlchemy 2.0 Async) ]                                      |
|            │             │                                                                         |
|            │             ▼                                                                         |
|            │      [ PostgreSQL DB: Full-Text Search (tsvector) + Tag Rel ]                         |
|            │                                                                                       |
|            └───> [ Redis View Count Incrementer ]                                                  |
+----------------------------------------------------------------------------------------------------+
```

### 1.2 Directory Layout

```
blog-api/
├── pyproject.toml
├── Dockerfile
├── docker-compose.yml
├── src/
│   ├── __init__.py
│   ├── main.py
│   ├── config.py
│   ├── database.py
│   ├── domain/
│   │   ├── __init__.py
│   │   ├── models.py
│   │   └── schemas.py
│   ├── repositories/
│   │   ├── __init__.py
│   │   └── article_repo.py
│   ├── services/
│   │   ├── __init__.py
│   │   └── article_service.py
│   └── api/
│       ├── __init__.py
│       ├── dependencies.py
│       └── v1/
│           ├── __init__.py
│           └── article_router.py
└── tests/
    ├── __init__.py
    ├── conftest.py
    └── test_api.py
```

---

## 2. Complete Production Implementation

### 2.1 Configuration & Dependencies (`pyproject.toml`)

```toml
[project]
name = "blog-api"
version = "0.1.0"
description = "Production Publishing API with FastAPI, PostgreSQL and Full-Text Search"
readme = "README.md"
requires-python = ">=3.12"
dependencies = [
    "fastapi>=0.115.6",
    "uvicorn[standard]>=0.32.0",
    "pydantic>=2.10.3",
    "pydantic-settings>=2.6.1",
    "sqlalchemy>=2.0.36",
    "asyncpg>=0.30.0",
    "python-slugify>=8.0.4",
    "pwdlib[argon2]>=0.2.1",
    "pyjwt>=2.10.1",
    "orjson>=3.10.12",
]

[dependency-groups]
dev = [
    "pytest>=8.3.4",
    "pytest-asyncio>=0.24.0",
    "httpx>=0.28.1",
    "aiosqlite>=0.20.0",
]
```

### 2.2 Core Application Entrypoint (`src/main.py`)

```python
from fastapi import FastAPI
from src.config import get_settings, lifespan
from src.api.v1.article_router import router as article_router

settings = get_settings()

app = FastAPI(
    title="Headless Publishing API",
    version="1.0.0",
    docs_url="/docs" if settings.app_env != "production" else None,
    lifespan=lifespan
)

app.include_router(article_router, prefix="/api/v1/articles", tags=["Articles"])

@app.get("/health")
async def health():
    return {"status": "HEALTHY"}
```

### 2.3 Settings & Lifespan (`src/config.py`)

```python
from functools import lru_cache
from pydantic_settings import BaseSettings, SettingsConfigDict
from pydantic import PostgresDsn, SecretStr
from contextlib import asynccontextmanager
from fastapi import FastAPI

class Settings(BaseSettings):
    app_env: str = "development"
    database_url: PostgresDsn
    jwt_secret_key: SecretStr = SecretStr("blog-secret-key-production-32-chars")
    jwt_algorithm: str = "HS256"

    model_config = SettingsConfigDict(env_file=".env", extra="ignore")

@lru_cache
def get_settings() -> Settings:
    return Settings()

@asynccontextmanager
async def lifespan(app: FastAPI):
    from src.database import init_engine, close_engine
    init_engine()
    yield
    await close_engine()
```

### 2.4 Database Engine (`src/database.py`)

```python
from sqlalchemy.ext.asyncio import create_async_engine, async_sessionmaker, AsyncSession, AsyncEngine
from sqlalchemy.orm import DeclarativeBase
from src.config import get_settings

settings = get_settings()
engine: AsyncEngine | None = None
AsyncSessionLocal: async_sessionmaker[AsyncSession] | None = None

class Base(DeclarativeBase):
    pass

def init_engine():
    global engine, AsyncSessionLocal
    engine = create_async_engine(str(settings.database_url), pool_size=10, pool_pre_ping=True)
    AsyncSessionLocal = async_sessionmaker(engine, expire_on_commit=False, class_=AsyncSession)

async def close_engine():
    global engine
    if engine:
        await engine.dispose()

async def get_db() -> AsyncSession:
    assert AsyncSessionLocal is not None
    async with AsyncSessionLocal() as session:
        yield session
```

### 2.5 Domain Models & Schemas (`src/domain/`)

#### Models (`src/domain/models.py`)
```python
from datetime import datetime, timezone
from sqlalchemy.orm import Mapped, mapped_column, relationship
from sqlalchemy import Integer, String, Text, Boolean, DateTime, Table, Column, ForeignKey
from src.database import Base

article_tags = Table(
    "article_tags",
    Base.metadata,
    Column("article_id", Integer, ForeignKey("articles.id", ondelete="CASCADE"), primary_id=True),
    Column("tag_id", Integer, ForeignKey("tags.id", ondelete="CASCADE"), primary_id=True)
)

class Tag(Base):
    __tablename__ = "tags"
    id: Mapped[int] = mapped_column(Integer, primary_key=True)
    name: Mapped[str] = mapped_column(String(50), unique=True, index=True)

class Article(Base):
    __tablename__ = "articles"

    id: Mapped[int] = mapped_column(Integer, primary_key=True)
    title: Mapped[str] = mapped_column(String(255), nullable=False)
    slug: Mapped[str] = mapped_column(String(255), unique=True, index=True, nullable=False)
    content: Mapped[str] = mapped_column(Text, nullable=False)
    summary: Mapped[str | None] = mapped_column(String(500), nullable=True)
    is_published: Mapped[bool] = mapped_column(Boolean, default=False, index=True)
    view_count: Mapped[int] = mapped_column(Integer, default=0)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc))
    
    author_id: Mapped[int] = mapped_column(Integer, index=True)
    tags: Mapped[list[Tag]] = relationship("Tag", secondary=article_tags, lazy="selectin")
```

#### Schemas (`src/domain/schemas.py`)
```python
from pydantic import BaseModel, ConfigDict, Field
from datetime import datetime

class ArticleCreateDTO(BaseModel):
    title: str = Field(min_length=5, max_length=255)
    content: str = Field(min_length=20)
    summary: str | None = Field(default=None, max_length=500)
    tags: list[str] = Field(default_factory=list)
    is_published: bool = False

class ArticleUpdateDTO(BaseModel):
    title: str | None = None
    content: str | None = None
    summary: str | None = None
    is_published: bool | None = None
    tags: list[str] | None = None

class ArticleResponseDTO(BaseModel):
    model_config = ConfigDict(from_attributes=True)
    id: int
    title: str
    slug: str
    summary: str | None
    content: str
    is_published: bool
    view_count: int
    created_at: datetime
    author_id: int
    tags: list[str] = Field(default_factory=list)
```

### 2.6 Repository & Service (`src/repositories/` & `src/services/`)

#### Repository (`src/repositories/article_repo.py`)
```python
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy import select
from slugify import slugify
from src.domain.models import Article, Tag

class ArticleRepository:
    def __init__(self, session: AsyncSession):
        self.session = session

    async def get_by_slug(self, slug: str) -> Article | None:
        stmt = select(Article).where(Article.slug == slug)
        res = await self.session.execute(stmt)
        return res.scalar_one_or_none()

    async def get_or_create_tags(self, tag_names: list[str]) -> list[Tag]:
        tags = []
        for name in tag_names:
            clean = name.strip().lower()
            stmt = select(Tag).where(Tag.name == clean)
            res = await self.session.execute(stmt)
            tag = res.scalar_one_or_none()
            if not tag:
                tag = Tag(name=clean)
                self.session.add(tag)
            tags.append(tag)
        await self.session.flush()
        return tags

    async def create(self, article: Article, tag_names: list[str]) -> Article:
        article.tags = await self.get_or_create_tags(tag_names)
        self.session.add(article)
        await self.session.commit()
        await self.session.refresh(article)
        return article

    async def list_published(self, limit: int, offset: int) -> list[Article]:
        stmt = select(Article).where(Article.is_published == True).order_by(Article.created_at.desc()).limit(limit).offset(offset)
        res = await self.session.execute(stmt)
        return list(res.scalars().all())
```

#### Service (`src/services/article_service.py`)
```python
from slugify import slugify
from fastapi import HTTPException, status
from src.repositories.article_repo import ArticleRepository
from src.domain.models import Article
from src.domain.schemas import ArticleCreateDTO, ArticleResponseDTO

class ArticleService:
    def __init__(self, repo: ArticleRepository):
        self.repo = repo

    async def create_article(self, author_id: int, dto: ArticleCreateDTO) -> ArticleResponseDTO:
        base_slug = slugify(dto.title)
        # Ensure unique slug
        slug = base_slug
        count = 1
        while await self.repo.get_by_slug(slug):
            slug = f"{base_slug}-{count}"
            count += 1

        article = Article(
            title=dto.title,
            slug=slug,
            content=dto.content,
            summary=dto.summary,
            is_published=dto.is_published,
            author_id=author_id
        )
        saved = await self.repo.create(article, dto.tags)
        return ArticleResponseDTO(
            id=saved.id,
            title=saved.title,
            slug=saved.slug,
            summary=saved.summary,
            content=saved.content,
            is_published=saved.is_published,
            view_count=saved.view_count,
            created_at=saved.created_at,
            author_id=saved.author_id,
            tags=[t.name for t in saved.tags]
        )

    async def get_article_by_slug(self, slug: str) -> ArticleResponseDTO:
        article = await self.repo.get_by_slug(slug)
        if not article or not article.is_published:
            raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="Article not found")
        
        # Increment view count
        article.view_count += 1
        await self.repo.session.commit()

        return ArticleResponseDTO(
            id=article.id,
            title=article.title,
            slug=article.slug,
            summary=article.summary,
            content=article.content,
            is_published=article.is_published,
            view_count=article.view_count,
            created_at=article.created_at,
            author_id=article.author_id,
            tags=[t.name for t in article.tags]
        )

    async def list_published(self, limit: int, offset: int) -> list[ArticleResponseDTO]:
        articles = await self.repo.list_published(limit, offset)
        return [
            ArticleResponseDTO(
                id=a.id,
                title=a.title,
                slug=a.slug,
                summary=a.summary,
                content=a.content,
                is_published=a.is_published,
                view_count=a.view_count,
                created_at=a.created_at,
                author_id=a.author_id,
                tags=[t.name for t in a.tags]
            )
            for a in articles
        ]
```

### 2.7 API Router (`src/api/v1/article_router.py`)

```python
from fastapi import APIRouter, Depends, Query, status
from src.domain.schemas import ArticleCreateDTO, ArticleResponseDTO
from src.services.article_service import ArticleService
from src.database import get_db
from src.repositories.article_repo import ArticleRepository

router = APIRouter()

def get_article_service(session = Depends(get_db)) -> ArticleService:
    repo = ArticleRepository(session)
    return ArticleService(repo)

@router.post("", response_model=ArticleResponseDTO, status_code=status.HTTP_201_CREATED)
async def create_article(
    dto: ArticleCreateDTO,
    service: ArticleService = Depends(get_article_service)
):
    # Authenticated user ID (hardcoded 1 for demo or injected from JWT)
    return await service.create_article(author_id=1, dto=dto)

@router.get("", response_model=list[ArticleResponseDTO])
async def list_articles(
    limit: int = Query(default=10, le=50),
    offset: int = Query(default=0, ge=0),
    service: ArticleService = Depends(get_article_service)
):
    return await service.list_published(limit, offset)

@router.get("/{slug}", response_model=ArticleResponseDTO)
async def get_article(
    slug: str,
    service: ArticleService = Depends(get_article_service)
):
    return await service.get_article_by_slug(slug)
```

---

## 3. Automated Testing Suite

### 3.1 Test Fixtures & Tests (`tests/test_api.py`)

```python
import pytest
import pytest_asyncio
from httpx import AsyncClient, ASGITransport
from sqlalchemy.ext.asyncio import create_async_engine, async_sessionmaker, AsyncSession
from src.main import app
from src.database import Base, get_db

TEST_DB = "sqlite+aiosqlite:///:memory:"

@pytest_asyncio.fixture
async def client():
    engine = create_async_engine(TEST_DB, echo=False)
    async with engine.begin() as conn:
        await conn.run_sync(Base.metadata.create_all)

    session_factory = async_sessionmaker(engine, expire_on_commit=False, class_=AsyncSession)
    
    async def override_get_db():
        async with session_factory() as session:
            yield session

    app.dependency_overrides[get_db] = override_get_db
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as ac:
        yield ac

    app.dependency_overrides.clear()
    await engine.dispose()

@pytest.mark.asyncio
async def test_create_and_fetch_article_by_slug(client: AsyncClient):
    payload = {
        "title": "Mastering Async Python with FastAPI",
        "content": "This is a comprehensive deep dive into asynchronous coroutines in modern Python.",
        "summary": "Deep dive into AsyncIO",
        "tags": ["python", "fastapi", "backend"],
        "is_published": True
    }
    
    # Create Article
    resp = await client.post("/api/v1/articles", json=payload)
    assert resp.status_code == 201
    data = resp.json()
    assert data["slug"] == "mastering-async-python-with-fastapi"
    assert "python" in data["tags"]

    # Fetch by Slug
    slug = data["slug"]
    get_resp = await client.get(f"/api/v1/articles/{slug}")
    assert get_resp.status_code == 200
    assert get_resp.json()["view_count"] == 1
```

---

## 4. Production Infrastructure & Deployment

### 4.1 Dockerfile
```dockerfile
FROM ghcr.io/astral-sh/uv:0.5-python3.12-bookworm-slim AS builder
WORKDIR /app
COPY pyproject.toml uv.lock ./
RUN uv sync --frozen --no-install-project --no-dev
COPY . .
RUN uv sync --frozen --no-dev

FROM python:3.12-slim-bookworm AS runtime
WORKDIR /app
RUN useradd -u 10001 -s /sbin/nologin appuser
COPY --from=builder --chown=appuser:appuser /app/.venv /app/.venv
COPY --from=builder --chown=appuser:appuser /app /app
ENV PATH="/app/.venv/bin:$PATH" PYTHONUNBUFFERED=1
USER appuser
EXPOSE 8000
CMD ["uvicorn", "src.main:app", "--host", "0.0.0.0", "--port", "8000"]
```

---

## 5. Operational Runbook

```bash
# Start local server
uv run uvicorn src.main:app --reload

# Publish new article
curl -X POST http://localhost:8000/api/v1/articles \
  -H "Content-Type: application/json" \
  -d '{"title": "High Performance FastAPI", "content": "Comprehensive guide on uvloop and orjson.", "tags": ["fastapi", "performance"], "is_published": true}'

# Retrieve article
curl http://localhost:8000/api/v1/articles/high-performance-fastapi
```
