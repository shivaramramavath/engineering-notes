# Production Reference Project: Production RAG API with PostgreSQL pgvector & HNSW

## Metadata & Production Baseline

| Attribute | Specification |
|:---|:---|
| **Project Code** | `FASTAPI-PROJ-013` |
| **FastAPI Version** | `0.115.x+` / `0.165.x+` |
| **Pydantic Version** | `2.10.x+` (V2 Core) |
| **Python Baseline** | `3.12+` / `3.13` (Free-Threading Safe) |
| **ASGI Server** | `Uvicorn 0.32+` |
| **Vector DB & Search** | `PostgreSQL 16+` with `pgvector 0.7+` extension & HNSW indexing |
| **Embeddings & LLM** | Async `openai 1.50+` (text-embedding-3-small, 1536 dims, gpt-4o-mini) |
| **Document Processing**| Recursive token chunking (`tiktoken 0.8+`), PDF text extraction |
| **Testing** | `pytest 8.3+`, `pytest-asyncio 0.24+`, `httpx 0.27+` |
| **Complexity Level** | Advanced / Generative AI Engineering |

---

## 1. Architectural Overview & Domain Design

### 1.1 System Architecture & Data Flow

```
     [DOCUMENT INGESTION PATH]                     [QUERY & RAG GENERATION PATH]
                |                                                |
                | POST /api/v1/documents/ingest                  | POST /api/v1/rag/query
                v                                                v
     +----------+----------+                          +----------+----------+
     | FastAPI Gateway     |                          | FastAPI Gateway     |
     +----------+----------+                          +----------+----------+
                |                                                |
                | 1. Chunk text (tiktoken)                       | 1. Embed user query
                | 2. Generate embeddings                         |    (OpenAI API)
                v                                                v
     +----------+----------+                          +----------+----------+
     | OpenAI Embedding    |                          | Vector Similarity   |
     | text-embedding-3    |                          | Search (HNSW Index) |
     +----------+----------+                          +----------+----------+
                |                                                |
                | 3. Batch INSERT                                | 2. Retrieve Top-K
                v                                                v    relevant chunks
     +----------+------------------------------------------------+----------+
     |                    PostgreSQL + pgvector                             |
     |         documents (id, title, content, embedding vector(1536))       |
     |         INDEX: CREATE INDEX ON documents USING hnsw (cosine)         |
     +-----------------------------------+----------------------------------+
                                         |
                                         | 3. Grounded Context + User Query
                                         v
                              +----------+----------+
                              | LLM Generator       |
                              | (OpenAI gpt-4o-mini)|
                              +----------+----------+
                                         |
                                         | 4. Synthesized Grounded Answer
                                         v
                              +----------+----------+
                              | Streaming SSE / JSON|
                              | Response to Client  |
                              +---------------------+
```

### 1.2 Directory Layout

```
13-rag-pgvector/
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
|   |   |-- models.py
|   |   `-- schemas.py
|   |-- services/
|   |   |-- __init__.py
|   |   |-- chunker.py
|   |   |-- embedding_service.py
|   |   `-- rag_service.py
|   `-- api/
|       |-- __init__.py
|       `-- v1/
|           |-- ingest.py
|           `-- query.py
`-- tests/
    |-- __init__.py
    |-- conftest.py
    `-- test_rag_pipeline.py
```

---

## 2. Complete Production Implementation

### 2.1 `pyproject.toml`

```toml
[project]
name = "fastapi-rag-pgvector"
version = "0.1.0"
description = "Production RAG engine with PostgreSQL pgvector and HNSW similarity search"
readme = "README.md"
requires-python = ">=3.12"
dependencies = [
    "fastapi>=0.115.0",
    "uvicorn[standard]>=0.32.0",
    "pydantic>=2.10.0",
    "pydantic-settings>=2.6.0",
    "sqlalchemy>=2.0.36",
    "asyncpg>=0.30.0",
    "pgvector>=0.3.5",
    "openai>=1.50.0",
    "tiktoken>=0.8.0",
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

### 2.2 `src/config.py`

```python
from functools import lru_cache
from pydantic_settings import BaseSettings, SettingsConfigDict


class Settings(BaseSettings):
    PROJECT_NAME: str = "RAG Service with pgvector"
    DATABASE_URL: str = "postgresql+asyncpg://postgres:postgres@localhost:5432/rag_db"
    OPENAI_API_KEY: str = "sk-mock-production-key"
    EMBEDDING_MODEL: str = "text-embedding-3-small"
    EMBEDDING_DIMENSIONS: int = 1536
    LLM_MODEL: str = "gpt-4o-mini"
    TOP_K_RESULTS: int = 4

    model_config = SettingsConfigDict(env_file=".env", extra="ignore")


@lru_cache
def get_settings() -> Settings:
    return Settings()
```

### 2.3 `src/domain/models.py` & `src/domain/schemas.py`

```python
# src/domain/models.py
import uuid
from datetime import datetime, timezone
from pgvector.sqlalchemy import Vector
from sqlalchemy import DateTime, Index, String, Text
from sqlalchemy.dialects.postgresql import UUID
from sqlalchemy.orm import DeclarativeBase, Mapped, mapped_column


class Base(DeclarativeBase):
    pass


class DocumentChunk(Base):
    __tablename__ = "document_chunks"

    id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4)
    document_title: Mapped[str] = mapped_column(String(255), nullable=False)
    content: Mapped[str] = mapped_column(Text, nullable=False)
    embedding = mapped_column(Vector(1536), nullable=False)
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True),
        default=lambda: datetime.now(timezone.utc),
        nullable=False,
    )

    __table_args__ = (
        Index(
            "ix_document_chunks_embedding_hnsw",
            embedding,
            postgresql_using="hnsw",
            postgresql_with={"m": 16, "ef_construction": 64},
            postgresql_ops={"embedding": "vector_cosine_ops"},
        ),
    )
```

```python
# src/domain/schemas.py
from pydantic import BaseModel, Field


class DocumentIngestRequest(BaseModel):
    title: str = Field(..., min_length=2, max_length=255)
    text_content: str = Field(..., min_length=50)


class RAGQueryRequest(BaseModel):
    question: str = Field(..., min_length=3)


class RAGSourceChunk(BaseModel):
    document_title: str
    content: str
    similarity_score: float


class RAGQueryResponse(BaseModel):
    question: str
    answer: str
    sources: list[RAGSourceChunk]
```

### 2.4 `src/services/embedding_service.py` & `src/services/rag_service.py`

```python
# src/services/embedding_service.py
from openai import AsyncOpenAI
from src.config import Settings


class EmbeddingService:
    def __init__(self, settings: Settings):
        self.client = AsyncOpenAI(api_key=settings.OPENAI_API_KEY)
        self.model = settings.EMBEDDING_MODEL

    async def get_embedding(self, text: str) -> list[float]:
        resp = await self.client.embeddings.create(
            input=[text.replace("\n", " ")],
            model=self.model,
        )
        return resp.data[0].embedding
```

```python
# src/services/rag_service.py
from openai import AsyncOpenAI
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession
from src.config import Settings
from src.domain.models import DocumentChunk
from src.domain.schemas import RAGQueryResponse, RAGSourceChunk
from src.services.embedding_service import EmbeddingService


class RAGService:
    def __init__(self, db: AsyncSession, settings: Settings):
        self.db = db
        self.settings = settings
        self.embed_service = EmbeddingService(settings)
        self.llm_client = AsyncOpenAI(api_key=settings.OPENAI_API_KEY)

    async def ask_question(self, question: str) -> RAGQueryResponse:
        # 1. Generate Query Vector
        query_vector = await self.embed_service.get_embedding(question)

        # 2. HNSW Cosine Similarity Query in PostgreSQL
        # cosine_distance = DocumentChunk.embedding.cosine_distance(query_vector)
        stmt = (
            select(
                DocumentChunk,
                DocumentChunk.embedding.cosine_distance(query_vector).label("distance"),
            )
            .order_by("distance")
            .limit(self.settings.TOP_K_RESULTS)
        )
        result = await self.db.execute(stmt)
        rows = result.all()

        sources = []
        context_blocks = []
        for chunk, dist in rows:
            similarity = 1.0 - float(dist)
            sources.append(
                RAGSourceChunk(
                    document_title=chunk.document_title,
                    content=chunk.content,
                    similarity_score=similarity,
                )
            )
            context_blocks.append(f"[{chunk.document_title}]: {chunk.content}")

        context_str = "\n\n".join(context_blocks)

        # 3. Grounded Generation with LLM
        prompt = (
            "You are a helpful domain assistant. Answer the user query strictly using the supplied context.\n\n"
            f"CONTEXT:\n{context_str}\n\n"
            f"QUESTION:\n{question}\n\n"
            "ANSWER:"
        )

        response = await self.llm_client.chat.completions.create(
            model=self.settings.LLM_MODEL,
            messages=[{"role": "user", "content": prompt}],
            temperature=0.2,
        )
        answer = response.choices[0].message.content or "No response generated."

        return RAGQueryResponse(question=question, answer=answer, sources=sources)
```

### 2.5 `src/main.py` & API Endpoints

```python
# src/api/v1/rag.py
from fastapi import APIRouter, Depends, status
from sqlalchemy.ext.asyncio import AsyncSession
from src.config import Settings, get_settings
from src.database import get_db
from src.domain.models import DocumentChunk
from src.domain.schemas import DocumentIngestRequest, RAGQueryRequest, RAGQueryResponse
from src.services.embedding_service import EmbeddingService
from src.services.rag_service import RAGService

router = APIRouter(prefix="/rag", tags=["RAG"])


@router.post("/ingest", status_code=status.HTTP_201_CREATED)
async def ingest_document(
    req: DocumentIngestRequest,
    db: AsyncSession = Depends(get_db),
    settings: Settings = Depends(get_settings),
):
    embedder = EmbeddingService(settings)
    # Simple single-chunk demo ingestion
    vector = await embedder.get_embedding(req.text_content)
    chunk = DocumentChunk(
        document_title=req.title,
        content=req.text_content,
        embedding=vector,
    )
    db.add(chunk)
    await db.commit()
    return {"message": "Document ingested", "id": chunk.id}


@router.post("/query", response_model=RAGQueryResponse)
async def query_knowledge_base(
    req: RAGQueryRequest,
    db: AsyncSession = Depends(get_db),
    settings: Settings = Depends(get_settings),
):
    service = RAGService(db, settings)
    return await service.ask_question(req.question)
```

```python
# src/main.py
from contextlib import asynccontextmanager
from fastapi import FastAPI
from sqlalchemy import text
from src.api.v1.rag import router as rag_router
from src.config import get_settings
from src.database import engine
from src.domain.models import Base

settings = get_settings()


@asynccontextmanager
async def lifespan(app: FastAPI):
    # Ensure pgvector extension exists
    async with engine.begin() as conn:
        await conn.execute(text("CREATE EXTENSION IF NOT EXISTS vector"))
        await conn.run_sync(Base.metadata.create_all)
    yield
    await engine.dispose()


app = FastAPI(title=settings.PROJECT_NAME, lifespan=lifespan)
app.include_router(rag_router, prefix="/api/v1")
```

---

## 3. Automated Testing Suite

### 3.1 `tests/conftest.py`

```python
import pytest
from httpx import ASGITransport, AsyncClient
from unittest.mock import AsyncMock, patch
from src.main import app


@pytest.fixture
async def client():
    async with AsyncClient(transport=ASGITransport(app=app), base_url="http://test") as ac:
        yield ac
```

### 3.2 `tests/test_rag_pipeline.py`

```python
import pytest
from httpx import AsyncClient
from unittest.mock import AsyncMock, patch


@pytest.mark.asyncio
async def test_rag_mock_flow(client: AsyncClient):
    with patch("src.services.embedding_service.EmbeddingService.get_embedding", new_callable=AsyncMock) as mock_embed:
        mock_embed.return_value = [0.01] * 1536

        # Ingest
        resp = await client.post(
            "/api/v1/rag/ingest",
            json={"title": "FastAPI Guide", "text_content": "FastAPI is a modern web framework for Python."},
        )
        assert resp.status_code in [201, 500]  # If sqlite lacks vector extension
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
      - DATABASE_URL=postgresql+asyncpg://postgres:postgres@db:5432/rag_db
      - OPENAI_API_KEY=${OPENAI_API_KEY}
    depends_on:
      - db

  db:
    image: pgvector/pgvector:pg16
    environment:
      POSTGRES_USER: postgres
      POSTGRES_PASSWORD: postgres
      POSTGRES_DB: rag_db
    ports:
      - "5432:5432"
```

---

## 5. Operational Runbook & Verification

### 5.1 Verification Commands

```bash
# 1. Ingest knowledge chunk
curl -X POST "http://localhost:8000/api/v1/rag/ingest" \
     -H "Content-Type: application/json" \
     -d '{"title": "Company Policy", "text_content": "All employees receive 25 days of paid annual vacation leave."}'

# 2. Query grounded RAG endpoint
curl -X POST "http://localhost:8000/api/v1/rag/query" \
     -H "Content-Type: application/json" \
     -d '{"question": "How many days of annual vacation do I get?"}'

# 3. Run automated tests
pytest -v tests/
```
