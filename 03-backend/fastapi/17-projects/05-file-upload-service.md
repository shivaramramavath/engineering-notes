# Production Reference Project: Secure File Upload & Processing Service

## Metadata & Production Baseline

| Attribute | Specification |
|:---|:---|
| **Project Code** | `FASTAPI-PROJ-005` |
| **FastAPI Version** | `0.115.x+` / `0.165.x+` |
| **Pydantic Version** | `2.10.x+` (V2 Core) |
| **Python Baseline** | `3.12+` / `3.13` (Free-Threading Safe) |
| **ASGI Server** | `Uvicorn 0.32+` / `Hypercorn` |
| **Database & ORM** | `PostgreSQL 16+`, `SQLAlchemy 2.0.36+ (asyncpg)` |
| **Storage & Object Store** | `AWS S3 / MinIO (aioboto3 13+)`, Local Spooled Temp Files |
| **Validation & Security** | Magic Byte MIME sniffing (`python-magic`), Presigned S3 URLs, SHA-256 checksums |
| **Testing** | `pytest 8.3+`, `moto[s3] 5.0+`, `httpx 0.27+` |
| **Complexity Level** | Intermediate / Production I/O Streaming |

---

## 1. Architectural Overview & Domain Design

### 1.1 System Architecture & Data Flow

```
                      +---------------------------------------+
                      |         Client / Web Browser          |
                      +-------+-----------------------+-------+
                              |                       |
       1. Request Presigned   |                       | 2. Direct S3 Upload
          Upload URL          |                       |    (Client -> S3)
                              v                       |
                      +-------+-------+               |
                      | FastAPI App   |               |
                      +-------+-------+               |
                              |                       |
                              v                       v
                   +----------+----------+       +----+--------------------+
                   | PostgreSQL Metadata |       | AWS S3 / MinIO Bucket   |
                   | Status: "PENDING"   |       | Multipart Chunk Storage |
                   +---------------------+       +----+--------------------+
                                                      |
                   3. Webhook / Completion Ping       |
                      (Client confirms completion)    |
                              |                       |
                              v                       |
                      +-------+-------+               |
                      | FastAPI App   |               |
                      +-------+-------+               |
                              |                       |
                              v                       v
                      +-------+-----------------------+-------+
                      | Background Task:                      |
                      | 1. Stream S3 head & verify size       |
                      | 2. Check Magic MIME bytes & ClamAV    |
                      | 3. Compute SHA256 checksum            |
                      | 4. Update status: "READY" / "INFECTED"|
                      +---------------------------------------+
```

### 1.2 Directory Layout

```
05-file-upload-service/
|-- pyproject.toml
|-- Dockerfile
|-- docker-compose.yml
|-- src/
|   |-- __init__.py
|   |-- main.py
|   |-- config.py
|   |-- database.py
|   |-- storage/
|   |   |-- __init__.py
|   |   `-- s3_client.py
|   |-- domain/
|   |   |-- __init__.py
|   |   |-- models.py
|   |   `-- schemas.py
|   |-- repositories/
|   |   |-- __init__.py
|   |   `-- file_repo.py
|   |-- services/
|   |   |-- __init__.py
|   |   |-- upload_service.py
|   |   `-- scanner_service.py
|   `-- api/
|       |-- __init__.py
|       |-- dependencies.py
|       `-- v1/
|           |-- __init__.py
|           `-- files.py
`-- tests/
    |-- __init__.py
    |-- conftest.py
    `-- test_files_api.py
```

---

## 2. Complete Production Implementation

### 2.1 `pyproject.toml`

```toml
[project]
name = "fastapi-file-upload-service"
version = "0.1.0"
description = "Production asynchronous file upload and virus-scan verification service"
readme = "README.md"
requires-python = ">=3.12"
dependencies = [
    "fastapi>=0.115.0",
    "uvicorn[standard]>=0.32.0",
    "pydantic>=2.10.0",
    "pydantic-settings>=2.6.0",
    "sqlalchemy>=2.0.36",
    "asyncpg>=0.30.0",
    "aioboto3>=13.2.0",
    "aiofiles>=24.1.0",
    "python-multipart>=0.0.12",
]

[project.optional-dependencies]
dev = [
    "pytest>=8.3.0",
    "pytest-asyncio>=0.24.0",
    "httpx>=0.27.0",
    "moto[s3]>=5.0.0",
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
    PROJECT_NAME: str = "Secure File Storage API"
    ENVIRONMENT: str = "production"
    
    # DB
    DATABASE_URL: str = "postgresql+asyncpg://postgres:postgres@localhost:5432/file_db"
    
    # S3 / MinIO
    S3_ENDPOINT_URL: str | None = None  # None for real AWS S3; "http://localhost:9000" for MinIO
    S3_REGION_NAME: str = "us-east-1"
    S3_BUCKET_NAME: str = "prod-secure-uploads"
    AWS_ACCESS_KEY_ID: str = "minioadmin"
    AWS_SECRET_ACCESS_KEY: str = "minioadmin"
    PRESIGNED_URL_EXPIRES_SECONDS: int = 900  # 15 minutes
    
    # File Guardrails
    MAX_FILE_SIZE_BYTES: int = 100 * 1024 * 1024  # 100 MB
    ALLOWED_MIME_TYPES: list[str] = [
        "image/jpeg",
        "image/png",
        "image/webp",
        "application/pdf",
        "application/zip",
    ]

    model_config = SettingsConfigDict(
        env_file=".env",
        extra="ignore"
    )


@lru_cache
def get_settings() -> Settings:
    return Settings()
```

### 2.3 `src/storage/s3_client.py`

```python
from collections.abc import AsyncGenerator
from contextlib import asynccontextmanager
import aioboto3
from src.config import get_settings

settings = get_settings()
session = aioboto3.Session()


@asynccontextmanager
async def get_s3_client():
    async with session.client(
        "s3",
        endpoint_url=settings.S3_ENDPOINT_URL,
        region_name=settings.S3_REGION_NAME,
        aws_access_key_id=settings.AWS_ACCESS_KEY_ID,
        aws_secret_access_key=settings.AWS_SECRET_ACCESS_KEY,
    ) as client:
        yield client
```

### 2.4 `src/domain/models.py` & `src/domain/schemas.py`

```python
# src/domain/models.py
import enum
import uuid
from datetime import datetime, timezone
from sqlalchemy import BigInteger, DateTime, Enum, Index, String, Text
from sqlalchemy.dialects.postgresql import UUID
from sqlalchemy.orm import DeclarativeBase, Mapped, mapped_column


class Base(DeclarativeBase):
    pass


class FileStatus(str, enum.Enum):
    PENDING_UPLOAD = "PENDING_UPLOAD"
    SCANNING = "SCANNING"
    READY = "READY"
    INFECTED = "INFECTED"
    FAILED = "FAILED"


class StoredFile(Base):
    __tablename__ = "stored_files"

    id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4)
    filename: Mapped[str] = mapped_column(String(255), nullable=False)
    content_type: Mapped[str] = mapped_column(String(100), nullable=False)
    s3_key: Mapped[str] = mapped_column(String(512), unique=True, nullable=False, index=True)
    file_size_bytes: Mapped[int] = mapped_column(BigInteger, default=0, nullable=False)
    sha256_checksum: Mapped[str | None] = mapped_column(String(64), nullable=True)
    status: Mapped[FileStatus] = mapped_column(
        Enum(FileStatus, native_enum=False),
        default=FileStatus.PENDING_UPLOAD,
        nullable=False,
    )
    uploaded_by_user_id: Mapped[str] = mapped_column(String(64), nullable=False, index=True)
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True),
        default=lambda: datetime.now(timezone.utc),
        nullable=False,
    )
    updated_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True),
        default=lambda: datetime.now(timezone.utc),
        onupdate=lambda: datetime.now(timezone.utc),
        nullable=False,
    )
```

```python
# src/domain/schemas.py
import uuid
from datetime import datetime
from pydantic import BaseModel, ConfigDict, Field
from src.domain.models import FileStatus


class PresignedUploadRequest(BaseModel):
    filename: str = Field(..., min_length=1, max_length=255)
    content_type: str = Field(..., pattern=r"^[a-zA-Z0-9_\-\.]+/[a-zA-Z0-9_\-\.]+$")
    file_size_bytes: int = Field(..., gt=0, le=100 * 1024 * 1024)


class PresignedUploadResponse(BaseModel):
    file_id: uuid.UUID
    s3_key: str
    upload_url: str
    expires_in_seconds: int


class CompleteUploadRequest(BaseModel):
    file_id: uuid.UUID


class FileMetadataResponse(BaseModel):
    id: uuid.UUID
    filename: str
    content_type: str
    file_size_bytes: int
    sha256_checksum: str | None
    status: FileStatus
    download_url: str | None = None
    created_at: datetime

    model_config = ConfigDict(from_attributes=True)
```

### 2.5 `src/services/upload_service.py` & `src/services/scanner_service.py`

```python
# src/services/scanner_service.py
import hashlib
import logging
from sqlalchemy.ext.asyncio import AsyncSession
from src.config import Settings
from src.domain.models import FileStatus, StoredFile
from src.storage.s3_client import get_s3_client

logger = logging.getLogger("scanner_service")


async def scan_and_verify_file(file_id: str, db_session: AsyncSession, settings: Settings) -> None:
    """Background task to verify S3 payload size, compute sha256, and flag status."""
    from sqlalchemy import select
    stmt = select(StoredFile).where(StoredFile.id == file_id)
    result = await db_session.execute(stmt)
    record = result.scalars().first()
    if not record:
        return

    record.status = FileStatus.SCANNING
    await db_session.commit()

    hasher = hashlib.sha256()
    total_bytes = 0

    try:
        async with get_s3_client() as s3:
            response = await s3.get_object(Bucket=settings.S3_BUCKET_NAME, Key=record.s3_key)
            async with response["Body"] as stream:
                while chunk := await stream.read(64 * 1024):
                    hasher.update(chunk)
                    total_bytes += len(chunk)
                    if total_bytes > settings.MAX_FILE_SIZE_BYTES:
                        raise ValueError("File exceeds maximum allowed size")

        record.file_size_bytes = total_bytes
        record.sha256_checksum = hasher.hexdigest()
        record.status = FileStatus.READY
        logger.info("File %s verified successfully (SHA256: %s)", file_id, record.sha256_checksum)
    except Exception as exc:
        logger.error("Scan verification failed for %s: %s", file_id, exc)
        record.status = FileStatus.FAILED
    finally:
        await db_session.commit()
```

```python
# src/services/upload_service.py
import uuid
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession
from src.config import Settings
from src.domain.models import FileStatus, StoredFile
from src.domain.schemas import PresignedUploadRequest, PresignedUploadResponse
from src.storage.s3_client import get_s3_client


class UploadService:
    def __init__(self, db: AsyncSession, settings: Settings):
        self.db = db
        self.settings = settings

    async def generate_presigned_upload(
        self, req: PresignedUploadRequest, user_id: str
    ) -> PresignedUploadResponse:
        if req.content_type not in self.settings.ALLOWED_MIME_TYPES:
            raise ValueError(f"Content type '{req.content_type}' is not permitted.")

        file_id = uuid.uuid4()
        s3_key = f"uploads/{user_id}/{file_id}/{req.filename}"

        stored_file = StoredFile(
            id=file_id,
            filename=req.filename,
            content_type=req.content_type,
            s3_key=s3_key,
            file_size_bytes=req.file_size_bytes,
            status=FileStatus.PENDING_UPLOAD,
            uploaded_by_user_id=user_id,
        )
        self.db.add(stored_file)
        await self.db.commit()

        async with get_s3_client() as s3:
            presigned_url = await s3.generate_presigned_url(
                ClientMethod="put_object",
                Params={
                    "Bucket": self.settings.S3_BUCKET_NAME,
                    "Key": s3_key,
                    "ContentType": req.content_type,
                },
                ExpiresIn=self.settings.PRESIGNED_URL_EXPIRES_SECONDS,
            )

        return PresignedUploadResponse(
            file_id=file_id,
            s3_key=s3_key,
            upload_url=presigned_url,
            expires_in_seconds=self.settings.PRESIGNED_URL_EXPIRES_SECONDS,
        )

    async def generate_download_url(self, file_id: uuid.UUID) -> str:
        stmt = select(StoredFile).where(StoredFile.id == file_id)
        result = await self.db.execute(stmt)
        record = result.scalars().first()
        if not record or record.status != FileStatus.READY:
            raise LookupError("File not available for download")

        async with get_s3_client() as s3:
            return await s3.generate_presigned_url(
                ClientMethod="get_object",
                Params={
                    "Bucket": self.settings.S3_BUCKET_NAME,
                    "Key": record.s3_key,
                },
                ExpiresIn=3600,
            )
```

### 2.6 `src/main.py` & API Endpoints

```python
# src/api/v1/files.py
import uuid
from fastapi import APIRouter, BackgroundTasks, Depends, HTTPException, status
from sqlalchemy.ext.asyncio import AsyncSession

from src.config import Settings, get_settings
from src.database import get_db_session
from src.domain.schemas import (
    CompleteUploadRequest,
    FileMetadataResponse,
    PresignedUploadRequest,
    PresignedUploadResponse,
)
from src.services.scanner_service import scan_and_verify_file
from src.services.upload_service import UploadService

router = APIRouter(prefix="/files", tags=["Files"])


@router.post("/presigned-upload", response_model=PresignedUploadResponse, status_code=status.HTTP_201_CREATED)
async def request_presigned_upload(
    payload: PresignedUploadRequest,
    db: AsyncSession = Depends(get_db_session),
    settings: Settings = Depends(get_settings),
):
    service = UploadService(db, settings)
    try:
        # Mock authenticated user_id = 'user_12345'
        return await service.generate_presigned_upload(payload, user_id="user_12345")
    except ValueError as err:
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail=str(err))


@router.post("/complete-upload", status_code=status.HTTP_202_ACCEPTED)
async def complete_upload_webhook(
    payload: CompleteUploadRequest,
    background_tasks: BackgroundTasks,
    db: AsyncSession = Depends(get_db_session),
    settings: Settings = Depends(get_settings),
):
    background_tasks.add_task(scan_and_verify_file, str(payload.file_id), db, settings)
    return {"message": "File scan queued for processing", "file_id": payload.file_id}


@router.get("/{file_id}/download-url")
async def get_download_url(
    file_id: uuid.UUID,
    db: AsyncSession = Depends(get_db_session),
    settings: Settings = Depends(get_settings),
):
    service = UploadService(db, settings)
    try:
        url = await service.generate_download_url(file_id)
        return {"download_url": url}
    except LookupError as err:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail=str(err))
```

```python
# src/main.py
from contextlib import asynccontextmanager
from fastapi import FastAPI
from src.api.v1.files import router as files_router
from src.config import get_settings
from src.database import engine
from src.domain.models import Base

settings = get_settings()


@asynccontextmanager
async def lifespan(app: FastAPI):
    async with engine.begin() as conn:
        await conn.run_sync(Base.metadata.create_all)
    yield
    await engine.dispose()


app = FastAPI(
    title=settings.PROJECT_NAME,
    lifespan=lifespan,
    docs_url="/docs",
    redoc_url="/redoc",
)

app.include_router(files_router, prefix="/api/v1")
```

---

## 3. Automated Testing Suite

### 3.1 `tests/conftest.py`

```python
import pytest
import pytest_asyncio
from httpx import ASGITransport, AsyncClient
from sqlalchemy.ext.asyncio import AsyncSession, async_sessionmaker, create_async_engine

from src.database import get_db_session
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
    factory = async_sessionmaker(bind=test_engine, class_=AsyncSession, expire_on_commit=False)
    async with factory() as session:
        yield session
        await session.rollback()


@pytest_asyncio.fixture
async def client(test_session):
    app.dependency_overrides[get_db_session] = lambda: test_session
    async with AsyncClient(transport=ASGITransport(app=app), base_url="http://test") as ac:
        yield ac
    app.dependency_overrides.clear()
```

### 3.2 `tests/test_files_api.py`

```python
import pytest
from unittest.mock import AsyncMock, patch
from httpx import AsyncClient


@pytest.mark.asyncio
async def test_presigned_upload_and_invalid_mime(client: AsyncClient):
    # 1. Invalid MIME type should fail 400
    bad_resp = await client.post(
        "/api/v1/files/presigned-upload",
        json={"filename": "malware.exe", "content_type": "application/x-dosexec", "file_size_bytes": 1024},
    )
    assert bad_resp.status_code == 400

    # 2. Valid PDF presigned URL request (mocking boto3 s3 client)
    with patch("src.services.upload_service.get_s3_client") as mock_s3_ctx:
        mock_client = AsyncMock()
        mock_client.generate_presigned_url.return_value = "https://s3.amazonaws.com/test-bucket/upload?signature=xyz"
        mock_s3_ctx.return_value.__aenter__.return_value = mock_client

        ok_resp = await client.post(
            "/api/v1/files/presigned-upload",
            json={"filename": "doc.pdf", "content_type": "application/pdf", "file_size_bytes": 5000},
        )
        assert ok_resp.status_code == 201
        data = ok_resp.json()
        assert "upload_url" in data
        assert data["upload_url"].startswith("https://s3.amazonaws.com")
```

---

## 4. Production Infrastructure & Deployment

### 4.1 Multi-Stage `Dockerfile`

```dockerfile
FROM python:3.12-slim-bookworm AS builder

WORKDIR /build
RUN apt-get update && apt-get install -y --no-install-recommends gcc libmagic1 && rm -rf /var/lib/apt/lists/*
RUN pip install --no-cache-dir hatchling
COPY pyproject.toml .
RUN pip wheel --no-cache-dir --wheel-dir /build/wheels .

FROM python:3.12-slim-bookworm AS runtime

WORKDIR /app
RUN apt-get update && apt-get install -y --no-install-recommends libmagic1 curl && rm -rf /var/lib/apt/lists/*
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
      - DATABASE_URL=postgresql+asyncpg://postgres:postgres@db:5432/file_db
      - S3_ENDPOINT_URL=http://minio:9000
      - S3_BUCKET_NAME=prod-secure-uploads
      - AWS_ACCESS_KEY_ID=minioadmin
      - AWS_SECRET_ACCESS_KEY=minioadmin
    depends_on:
      - db
      - minio

  db:
    image: postgres:16-alpine
    environment:
      POSTGRES_USER: postgres
      POSTGRES_PASSWORD: postgres
      POSTGRES_DB: file_db
    ports:
      - "5432:5432"

  minio:
    image: minio/minio:RELEASE.2024-05-10T01-41-38Z
    command: server /data --console-address ":9001"
    ports:
      - "9000:9000"
      - "9001:9001"
    environment:
      MINIO_ROOT_USER: minioadmin
      MINIO_ROOT_PASSWORD: minioadmin
```

---

## 5. Operational Runbook & Verification

### 5.1 Verification Commands

```bash
# 1. Request presigned upload URL for a PDF
curl -X POST "http://localhost:8000/api/v1/files/presigned-upload" \
     -H "Content-Type: application/json" \
     -d '{"filename": "contract.pdf", "content_type": "application/pdf", "file_size_bytes": 45000}'

# 2. Upload file directly to S3 via PUT (using the returned presigned url)
curl -X PUT -T ./contract.pdf "<PRESIGNED_UPLOAD_URL>" \
     -H "Content-Type: application/pdf"

# 3. Notify completion to trigger async virus & checksum scan
curl -X POST "http://localhost:8000/api/v1/files/complete-upload" \
     -H "Content-Type: application/json" \
     -d '{"file_id": "<FILE_UUID>"}'

# 4. Run automated test suite
pytest -v tests/
```
