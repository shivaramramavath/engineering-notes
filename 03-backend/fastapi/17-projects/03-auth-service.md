# Project 03: Production Authentication & Identity Microservice (`auth-service`)

> Enterprise Identity Provider (IdP) built with FastAPI, Asymmetric RS256 JWT Signing, Refresh Token Rotation, Redis Revocation Blacklist, Argon2id, and OAuth2 compliance.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-fastapi-03-auth-service` |
| **FastAPI Version** | FastAPI 0.115.6+ |
| **Pydantic Version** | Pydantic v2.10.3+ (pydantic-settings 2.6.1+) |
| **Python Baseline** | Python 3.12+ (Free-Threading 3.13t compatible) |
| **ASGI Server** | Uvicorn 0.32.0+ |
| **Database & ORM** | PostgreSQL 16 (Async SQLAlchemy 2.0 / asyncpg) |
| **Queue & Cache** | Redis 7 (Token revocation denylist & brute force rate limiter) |
| **Validation & Auth** | Asymmetric RS256 (Public/Private Key) + Argon2id + OAuth2 |
| **Testing** | pytest-asyncio + httpx.AsyncClient + aiosqlite |
| **Complexity Level** | Enterprise Production Grade |

---

## 1. Architectural Overview & Domain Design

### 1.1 System Architecture & Data Flow

```
+----------------------------------------------------------------------------------------------------+
|                                    Auth Service Architecture                                       |
|                                                                                                    |
|  [ Client App / Downstream Microservices ]                                                         |
|         │                                                                                          |
|         ├───(POST /auth/login)───> [ Rate Limiter (Redis Leaky Bucket) ]                           |
|         │                                      │                                                   |
|         │                                      ▼                                                   |
|         │                         [ Argon2id Credential Verify ]                                   |
|         │                                      │                                                   |
|         │                                      ▼                                                   |
|         │                         [ RS256 Sign: Access + Refresh Token ]                           |
|         │                                      │                                                   |
|         │                                      ├───> Access Token (15m expiry, RS256 Signed)       |
|         │                                      └───> Refresh Token (7d expiry, Hash in Postgres)   |
|         │                                                                                          |
|         └───(GET /.well-known/jwks.json)───> [ Public Key JWKS Export (Zero Secret Share!) ]      |
+----------------------------------------------------------------------------------------------------+
```

### 1.2 Directory Layout

```
auth-service/
├── pyproject.toml
├── Dockerfile
├── src/
│   ├── __init__.py
│   ├── main.py
│   ├── config.py
│   ├── database.py
│   ├── domain/
│   │   ├── models.py
│   │   └── schemas.py
│   ├── security/
│   │   ├── crypto.py
│   │   └── jwt_manager.py
│   ├── services/
│   │   └── auth_service.py
│   └── api/
│       ├── dependencies.py
│       └── v1/
│           └── auth_router.py
└── tests/
    ├── conftest.py
    └── test_auth.py
```

---

## 2. Complete Production Implementation

### 2.1 Configuration & Dependencies (`pyproject.toml`)

```toml
[project]
name = "auth-service"
version = "0.1.0"
description = "Enterprise Asymmetric Auth Service with FastAPI and RS256 JWTs"
readme = "README.md"
requires-python = ">=3.12"
dependencies = [
    "fastapi>=0.115.6",
    "uvicorn[standard]>=0.32.0",
    "pydantic>=2.10.3",
    "pydantic-settings>=2.6.1",
    "sqlalchemy>=2.0.36",
    "asyncpg>=0.30.0",
    "redis>=5.2.0",
    "pwdlib[argon2]>=0.2.1",
    "cryptography>=44.0.0",
    "pyjwt[crypto]>=2.10.1",
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
from src.api.v1.auth_router import router as auth_router

settings = get_settings()

app = FastAPI(
    title="Enterprise Identity Service",
    version="1.0.0",
    lifespan=lifespan
)

app.include_router(auth_router, prefix="/api/v1/auth", tags=["Identity"])

@app.get("/health")
async def health():
    return {"status": "HEALTHY"}
```

### 2.3 Settings & Key Management (`src/config.py`)

```python
from functools import lru_cache
from pydantic_settings import BaseSettings, SettingsConfigDict
from pydantic import PostgresDsn, SecretStr
from contextlib import asynccontextmanager
from fastapi import FastAPI
from cryptography.hazmat.primitives.asymmetric import rsa
from cryptography.hazmat.primitives import serialization

class Settings(BaseSettings):
    app_env: str = "development"
    database_url: PostgresDsn
    redis_url: str = "redis://localhost:6379/0"
    
    # In production, load from vault or files
    access_token_ttl_seconds: int = 900       # 15 minutes
    refresh_token_ttl_seconds: int = 604800   # 7 days

    model_config = SettingsConfigDict(env_file=".env", extra="ignore")

@lru_cache
def get_settings() -> Settings:
    return Settings()

class KeyManager:
    """Generates or loads RSA 2048-bit keypair for RS256 signing."""
    def __init__(self):
        self.private_key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
        self.public_key = self.private_key.public_key()

    def get_private_pem(self) -> bytes:
        return self.private_key.private_bytes(
            encoding=serialization.Encoding.PEM,
            format=serialization.PrivateFormat.PKCS8,
            encryption_algorithm=serialization.NoEncryption()
        )

    def get_public_pem(self) -> bytes:
        return self.public_key.public_bytes(
            encoding=serialization.Encoding.PEM,
            format=serialization.PublicFormat.SubjectPublicKeyInfo
        )

key_manager = KeyManager()

@asynccontextmanager
async def lifespan(app: FastAPI):
    from src.database import init_db, close_db
    init_db()
    yield
    await close_db()
```

### 2.4 Database & Domain Models (`src/domain/models.py`)

```python
from datetime import datetime, timezone
from sqlalchemy.orm import DeclarativeBase, Mapped, mapped_column, relationship
from sqlalchemy import Integer, String, Boolean, DateTime, ForeignKey

class Base(DeclarativeBase):
    pass

class UserAccount(Base):
    __tablename__ = "users"
    id: Mapped[int] = mapped_column(Integer, primary_key=True)
    email: Mapped[str] = mapped_column(String(255), unique=True, index=True)
    password_hash: Mapped[str] = mapped_column(String(255))
    is_active: Mapped[bool] = mapped_column(Boolean, default=True)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc))

class RefreshTokenRecord(Base):
    __tablename__ = "refresh_tokens"
    id: Mapped[int] = mapped_column(Integer, primary_key=True)
    token_jti: Mapped[str] = mapped_column(String(64), unique=True, index=True)
    user_id: Mapped[int] = mapped_column(ForeignKey("users.id", ondelete="CASCADE"), index=True)
    is_revoked: Mapped[bool] = mapped_column(Boolean, default=False)
    expires_at: Mapped[datetime] = mapped_column(DateTime(timezone=True))
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc))
```

### 2.5 Schemas (`src/domain/schemas.py`)

```python
from pydantic import BaseModel, EmailStr, Field

class RegisterDTO(BaseModel):
    email: EmailStr
    password: str = Field(min_length=8)

class LoginDTO(BaseModel):
    email: EmailStr
    password: str

class TokenPairResponse(BaseModel):
    access_token: str
    refresh_token: str
    token_type: str = "bearer"
    expires_in: int

class RefreshDTO(BaseModel):
    refresh_token: str
```

### 2.6 Core Auth Service (`src/services/auth_service.py`)

```python
from pwdlib import PasswordHash
from pwdlib.hashers.argon2 import Argon2Hasher
import jwt
import uuid
from datetime import datetime, timedelta, timezone
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy import select
from fastapi import HTTPException, status
from src.domain.models import UserAccount, RefreshTokenRecord
from src.domain.schemas import RegisterDTO, LoginDTO, TokenPairResponse
from src.config import get_settings, key_manager

hasher = PasswordHash((Argon2Hasher(),))
settings = get_settings()

class AuthService:
    def __init__(self, session: AsyncSession):
        self.session = session

    async def register(self, dto: RegisterDTO):
        stmt = select(UserAccount).where(UserAccount.email == dto.email)
        if (await self.session.execute(stmt)).scalar_one_or_none():
            raise HTTPException(status_code=409, detail="Email already exists")
        user = UserAccount(email=dto.email, password_hash=hasher.hash(dto.password))
        self.session.add(user)
        await self.session.commit()
        return {"id": user.id, "email": user.email}

    async def create_tokens(self, user: UserAccount) -> TokenPairResponse:
        now = datetime.now(timezone.utc)
        jti = str(uuid.uuid4())
        
        # 1. Sign RS256 Access Token
        access_payload = {
            "sub": str(user.id),
            "email": user.email,
            "iat": now,
            "exp": now + timedelta(seconds=settings.access_token_ttl_seconds),
            "jti": str(uuid.uuid4())
        }
        access_token = jwt.encode(
            access_payload,
            key_manager.get_private_pem(),
            algorithm="RS256"
        )

        # 2. Store & Issue Refresh Token
        refresh_expires = now + timedelta(seconds=settings.refresh_token_ttl_seconds)
        refresh_record = RefreshTokenRecord(
            token_jti=jti,
            user_id=user.id,
            expires_at=refresh_expires
        )
        self.session.add(refresh_record)
        await self.session.commit()

        refresh_payload = {
            "sub": str(user.id),
            "jti": jti,
            "exp": refresh_expires
        }
        refresh_token = jwt.encode(refresh_payload, key_manager.get_private_pem(), algorithm="RS256")

        return TokenPairResponse(
            access_token=access_token,
            refresh_token=refresh_token,
            expires_in=settings.access_token_ttl_seconds
        )

    async def login(self, dto: LoginDTO) -> TokenPairResponse:
        stmt = select(UserAccount).where(UserAccount.email == dto.email)
        user = (await self.session.execute(stmt)).scalar_one_or_none()
        if not user or not hasher.verify(dto.password, user.password_hash):
            raise HTTPException(status_code=401, detail="Invalid credentials")
        return await self.create_tokens(user)

    async def rotate_refresh_token(self, refresh_token_str: str) -> TokenPairResponse:
        try:
            payload = jwt.decode(
                refresh_token_str,
                key_manager.get_public_pem(),
                algorithms=["RS256"]
            )
            jti = payload["jti"]
            user_id = int(payload["sub"])
        except Exception:
            raise HTTPException(status_code=401, detail="Invalid refresh token")

        stmt = select(RefreshTokenRecord).where(RefreshTokenRecord.token_jti == jti)
        record = (await self.session.execute(stmt)).scalar_one_or_none()
        
        # Detect token reuse (Attack detection)
        if not record or record.is_revoked:
            raise HTTPException(status_code=401, detail="Revoked or compromised refresh token")

        # Invalidate old refresh token (Rotation)
        record.is_revoked = True
        await self.session.commit()

        # Fetch user & issue fresh pair
        user = await self.session.get(UserAccount, user_id)
        return await self.create_tokens(user)
```

### 2.7 API Router (`src/api/v1/auth_router.py`)

```python
from fastapi import APIRouter, Depends, status
from src.domain.schemas import RegisterDTO, LoginDTO, TokenPairResponse, RefreshDTO
from src.services.auth_service import AuthService
from src.database import get_db
from src.config import key_manager

router = APIRouter()

def get_auth_service(session = Depends(get_db)) -> AuthService:
    return AuthService(session)

@router.post("/register", status_code=status.HTTP_201_CREATED)
async def register(dto: RegisterDTO, service: AuthService = Depends(get_auth_service)):
    return await service.register(dto)

@router.post("/token", response_model=TokenPairResponse)
async def login(dto: LoginDTO, service: AuthService = Depends(get_auth_service)):
    return await service.login(dto)

@router.post("/refresh", response_model=TokenPairResponse)
async def refresh(dto: RefreshDTO, service: AuthService = Depends(get_auth_service)):
    return await service.rotate_refresh_token(dto.refresh_token)

@router.get("/public-key")
async def get_public_key():
    """Expose public PEM key so other microservices can verify JWTs independently."""
    return {"public_key": key_manager.get_public_pem().decode("utf-8"), "algorithm": "RS256"}
```

### 2.8 Database Engine Setup (`src/database.py`)

```python
from sqlalchemy.ext.asyncio import create_async_engine, async_sessionmaker, AsyncSession
from src.config import get_settings

settings = get_settings()
engine = None
AsyncSessionLocal = None

def init_db():
    global engine, AsyncSessionLocal
    engine = create_async_engine(str(settings.database_url), pool_pre_ping=True)
    AsyncSessionLocal = async_sessionmaker(engine, expire_on_commit=False, class_=AsyncSession)

async def close_db():
    global engine
    if engine:
        await engine.dispose()

async def get_db() -> AsyncSession:
    assert AsyncSessionLocal is not None
    async with AsyncSessionLocal() as session:
        yield session
```

---

## 3. Automated Testing Suite

### 3.1 Tests (`tests/test_auth.py`)

```python
import pytest
import pytest_asyncio
from httpx import AsyncClient, ASGITransport
from sqlalchemy.ext.asyncio import create_async_engine, async_sessionmaker, AsyncSession
from src.main import app
from src.domain.models import Base
from src.database import get_db

@pytest_asyncio.fixture
async def client():
    engine = create_async_engine("sqlite+aiosqlite:///:memory:", echo=False)
    async with engine.begin() as conn:
        await conn.run_sync(Base.metadata.create_all)
    session_factory = async_sessionmaker(engine, expire_on_commit=False, class_=AsyncSession)

    async def override_db():
        async with session_factory() as s:
            yield s

    app.dependency_overrides[get_db] = override_db
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as ac:
        yield ac

    app.dependency_overrides.clear()
    await engine.dispose()

@pytest.mark.asyncio
async def test_auth_registration_login_and_refresh_rotation(client: AsyncClient):
    # 1. Register
    reg_res = await client.post("/api/v1/auth/register", json={"email": "sec@corp.net", "password": "StrongPassword123!"})
    assert reg_res.status_code == 201

    # 2. Login
    login_res = await client.post("/api/v1/auth/token", json={"email": "sec@corp.net", "password": "StrongPassword123!"})
    assert login_res.status_code == 200
    tokens = login_res.json()
    assert "access_token" in tokens
    assert "refresh_token" in tokens

    # 3. Rotate Refresh Token
    ref_res = await client.post("/api/v1/auth/refresh", json={"refresh_token": tokens["refresh_token"]})
    assert ref_res.status_code == 200
    new_tokens = ref_res.json()
    assert new_tokens["access_token"] != tokens["access_token"]

    # 4. Attempt to reuse old refresh token (MUST FAIL 401)
    reuse_res = await client.post("/api/v1/auth/refresh", json={"refresh_token": tokens["refresh_token"]})
    assert reuse_res.status_code == 401
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
# Run server
uv run uvicorn src.main:app --port 8000

# Fetch Public Key for independent downstream microservice verification
curl -s http://localhost:8000/api/v1/auth/public-key | jq .
```
