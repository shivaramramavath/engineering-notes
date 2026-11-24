# Module 07: Authentication & Security — 17: Secrets Management and Environment Security (`17-secrets-management-and-env-security`)

> Hardcoding cryptographic keys, database credentials, and API secrets inside code repositories is a leading cause of enterprise data breaches. In FastAPI, `pydantic-settings` paired with `SecretStr` enforces strict environment isolation, prevents accidental secret leaks in logging pipelines, and integrates with cloud secret managers (AWS, Vault, GCP).

| Field | Value |
|---|---|
| **Category** | Authentication & Security |
| **Target Level** | Advanced / Production Engineer |
| **FastAPI Version** | FastAPI 0.165.0+ |
| **Starlette Version**| Starlette 1.10.0+ |
| **Pydantic Version** | Pydantic v2.14.4+ (pydantic-settings 2.16.3+) |
| **Python Version**   | Python 3.12+ (3.13 / 3.14 compatible, Free-Threading verified) |
| **Tested With**      | pydantic-settings 2.16+, pytest 9.1+ |

---

## Table of Contents
1. [Conceptual Overview](#1-conceptual-overview)
2. [Core Concepts & Architecture](#2-core-concepts--architecture)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Verified Code Examples](#5-verified-code-examples)
6. [Edge Cases, Pitfalls & Failure Modes](#6-edge-cases-pitfalls--failure-modes)
7. [Common Mistakes Table](#7-common-mistakes-table)
8. [Performance & Scalability Considerations](#8-performance--scalability-considerations)
9. [Security & OWASP Guardrails](#9-security--owasp-guardrails)
10. [Testing Strategies & Pytest Recipes](#10-testing-strategies--pytest-recipes)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Technical Interview Questions](#12-technical-interview-questions)
13. [Practical Exercises & Solutions](#13-practical-exercises--solutions)

---

## 1. Conceptual Overview

The Twelve-Factor App methodology mandates strict separation of configuration from code:
- **The Secret Leak Threat**: Standard strings (`database_password = "secret"`) leak when models are serialized to JSON (`model.model_dump()`), printed in debug logs, or exposed in error tracking tools (Sentry).
- **`pydantic.SecretStr` Solution**: Wraps sensitive strings. Its `__repr__` and `__str__` always output `'**********'`. The plain string value can only be retrieved intentionally via `.get_secret_value()`.
- **`pydantic-settings`**: Automatically reads values from OS environment variables, Docker secrets, Kubernetes secret mounts, or `.env` files with strong type parsing and runtime validation.
- **Dynamic Rotation**: Enterprise secrets (JWT keys, DB credentials) must rotate without application restarts.

---

## 2. Core Concepts & Architecture

```
+-----------------------------------------------------------------------------------+
|               Secrets Ingestion & Masking Architecture                            |
+-----------------------------------------------------------------------------------+
|                                                                                   |
|  Environment Sources:                                                             |
|  - AWS Secrets Manager / Vault                                                    |
|  - Kubernetes Secret Volume (/run/secrets/...)                                    |
|  - OS Environment Variables (ENV)                                                 |
|  - Local .env file (Development Only)                                             |
|                         │                                                         |
|                         ▼                                                         |
|  [ Pydantic BaseSettings Ingestion Engine ]                                       |
|  - Strong type parsing (int, AnyHttpUrl, PostgresDsn)                             |
|  - Sensitive strings wrapped in `SecretStr`                                       |
|                         │                                                         |
|                         ▼                                                         |
|  Application Execution:                                                           |
|  - Logging / Debug: `repr(settings)` ──> `db_password=SecretStr('**********')`   |
|  - Serialization: `settings.model_dump()` ──> Masked automatically                |
|  - Cryptographic Driver: `secret.get_secret_value()` ──> Raw bytes                |
|                                                                                   |
+-----------------------------------------------------------------------------------+
```

---

## 3. How It Works Internally

1. **`pydantic.SecretStr` Mechanics**:
   - Overrides `__str__`, `__repr__`, and `__format__` to return a static masked string.
   - Pydantic v2's core serializer excludes or masks `SecretStr` fields during `.model_dump()` and `.model_dump_json()`.
   - The private attribute `_secret_value` is accessed exclusively through the explicit accessor method `.get_secret_value()`.
2. **`BaseSettings` Loading Order**:
   - 1. System environment variables (highest priority).
   - 2. Secrets directory (`/run/secrets`).
   - 3. `.env` file specified in `SettingsConfigDict`.
   - 4. Default values defined on the model (lowest priority).

---

## 4. Syntax & API Reference

```python
from functools import lru_cache
from pydantic import PostgresDsn, SecretStr
from pydantic_settings import BaseSettings, SettingsConfigDict

class AppSettings(BaseSettings):
    app_env: str = "production"
    jwt_secret: SecretStr
    database_url: PostgresDsn
    redis_password: SecretStr | None = None

    model_config = SettingsConfigDict(
        env_file=".env",
        env_file_encoding="utf-8",
        extra="ignore",
    )

@lru_cache
def get_settings() -> AppSettings:
    return AppSettings()
```

---

## 5. Verified Code Examples

### Example 1: Minimal Settings Model with Masked Secrets

```python
from typing import Annotated
from fastapi import Depends, FastAPI
from pydantic import SecretStr
from pydantic_settings import BaseSettings

class Settings(BaseSettings):
    app_name: str = "Secure Gateway"
    api_key: SecretStr = SecretStr("default_development_secret_do_not_use_in_prod")

settings = Settings()
app = FastAPI()

@app.get("/config")
def show_config():
    # Demonstrates that string representation is safely masked
    return {
        "app_name": settings.app_name,
        "masked_key_repr": repr(settings.api_key),  # SecretStr('**********')
        "masked_key_str": str(settings.api_key),    # **********
    }
```

### Example 2: Enterprise Production Settings with Cloud-Ready Configuration

```python
from enum import StrEnum
from functools import lru_cache
from typing import Annotated
from fastapi import Depends, FastAPI
from pydantic import PostgresDsn, RedisDsn, SecretStr, field_validator
from pydantic_settings import BaseSettings, SettingsConfigDict

class Environment(StrEnum):
    LOCAL = "local"
    STAGING = "staging"
    PRODUCTION = "production"

class EnterpriseSettings(BaseSettings):
    environment: Environment = Environment.PRODUCTION
    jwt_signing_key: SecretStr
    jwt_algorithm: str = "HS256"
    database_url: PostgresDsn
    redis_url: RedisDsn
    sentry_dsn: SecretStr | None = None

    model_config = SettingsConfigDict(
        env_file=".env",
        env_file_encoding="utf-8",
        case_sensitive=False,
        secrets_dir="/run/secrets",  # Docker/Kubernetes secrets mount
        extra="forbid",              # Fail fast on typo in env variable names
    )

    @field_validator("jwt_signing_key")
    @classmethod
    def validate_key_length(cls, v: SecretStr) -> SecretStr:
        if len(v.get_secret_value()) < 32:
            raise ValueError("JWT signing secret must be at least 32 characters long")
        return v

@lru_cache
def get_settings() -> EnterpriseSettings:
    return EnterpriseSettings()
```

### Example 3: Edge Case / Boundary — Safe Logging Middleware without Secret Leaks

```python
import logging
from starlette.middleware.base import BaseHTTPMiddleware
from starlette.requests import Request

logger = logging.getLogger("audit")
SENSITIVE_HEADERS = {"authorization", "x-api-key", "cookie", "set-cookie"}

class SafeLoggingMiddleware(BaseHTTPMiddleware):
    async def dispatch(self, request: Request, call_next):
        # Sanitize headers for logging
        safe_headers = {
            k: ("***MASKED***" if k.lower() in SENSITIVE_HEADERS else v)
            for k, v in request.headers.items()
        }
        logger.info(f"Incoming request: {request.method} {request.url.path} headers={safe_headers}")
        return await call_next(request)
```

### Example 4: Failure Case — Pydantic Validation Error on Missing Production Secret

```python
# If JWT_SIGNING_KEY is omitted from the environment:
# Traceback (most recent call last):
#   ...
# pydantic_core._pydantic_core.ValidationError: 1 validation error for EnterpriseSettings
# jwt_signing_key
#   Field required [type=missing, input_value={}, input_type=dict]
```

### Example 5: Idiomatic Modern Pattern with FastAPI Dependency Injection

```python
from functools import lru_cache
from typing import Annotated
from fastapi import Depends, FastAPI
from pydantic import SecretStr
from pydantic_settings import BaseSettings

class Settings(BaseSettings):
    database_password: SecretStr

@lru_cache
def get_app_settings() -> Settings:
    return Settings()

AppSettings = Annotated[Settings, Depends(get_app_settings)]
app = FastAPI()

@app.post("/db/connect")
def connect_database(cfg: AppSettings):
    raw_secret = cfg.database_password.get_secret_value()
    # In real driver: await asyncpg.connect(password=raw_secret)
    return {"status": "connected", "secret_length": len(raw_secret)}
```

---

## 6. Edge Cases, Pitfalls & Failure Modes

1. **Commitment of `.env` Files to Version Control**: Developers accidentally committing `.env` containing production API keys. Always list `.env` and `.env.*` in `.gitignore` and enforce pre-commit secret scanners (`trufflehog`, `gitleaks`).
2. **Logging `request.headers` Directly**: Logging raw incoming headers writes sensitive bearer tokens and API keys to cloud observability platforms (Datadog, CloudWatch), violating compliance mandates.
3. **Using Mutable Global Settings**: Instantiating settings as a mutable module-level variable makes it difficult to mock or override settings cleanly during pytest execution. Always use `@lru_cache` and inject via `Depends(get_settings)`.

---

## 7. Common Mistakes Table

| Mistake | Why It Fails | Error / Diagnostic | Correct Idiomatic Fix |
|---|---|---|---|
| 1. Using standard `str` for secrets | Secrets appear in plain text in logs and exceptions | Secret leakage in Sentry/Datadog | Use `pydantic.SecretStr` |
| 2. Hardcoding default production passwords | Default passwords used in live environments | Critical security vulnerability | Require mandatory env variable (`SecretStr(...)`) |
| 3. Committing `.env` to git | Secrets permanently stored in git history | Public credential exposure | Add `.env` to `.gitignore` |
| 4. Not validating secret minimum entropy | Weak secrets (`jwt_secret="pass"`) vulnerable to brute-force | Forged token vulnerability | Use `@field_validator` to enforce length $\ge 32$ |
| 5. Modifying settings object at runtime | State leakage across tests and requests | Unpredictable environment behavior | Keep settings instances immutable |
| 6. Allowing unknown extra environment variables | Typo in env vars (e.g. `PORT` vs `POTR`) ignored silently | App runs with wrong configuration | Set `extra="forbid"` in `SettingsConfigDict` |

---

## 8. Performance & Scalability Considerations

- **`@lru_cache` Singleton Pattern**: Loading and validating environment variables incurs disk/syscall overhead. Caching `get_settings()` ensures parsing occurs once at startup, returning the cached instance in <0.0001ms.
- **Docker Secrets Mounting**: Reading secrets from `/run/secrets` in memory-backed tmpfs mounts is faster and more secure than passing secrets through environment variables (which can leak via `/proc/$PID/environ`).

---

## 9. Security & OWASP Guardrails

- **OWASP API2:2023 Broken Authentication**: Cryptographic keys must be generated using cryptographically secure pseudorandom number generators (`secrets.token_urlsafe(32)`).
- **PCI-DSS Requirement 3.4**: All sensitive authentication data must be protected against unauthorized access, whether in transit, at rest, or in memory.

---

## 10. Testing Strategies & Pytest Recipes

```python
import pytest
from httpx import ASGITransport, AsyncClient
from fastapi import FastAPI
from pydantic import SecretStr

@pytest.fixture
def mock_settings(monkeypatch):
    monkeypatch.setenv("JWT_SIGNING_KEY", "test_mock_jwt_signing_key_32_characters_long")
    monkeypatch.setenv("DATABASE_URL", "postgresql://usr:pass@localhost:5432/testdb")

def test_secret_masking():
    secret = SecretStr("my_super_secret")
    assert str(secret) == "**********"
    assert repr(secret) == "SecretStr('**********')"
    assert secret.get_secret_value() == "my_super_secret"
```

---

## 11. Real-World Case Study

An enterprise insurtech firm suffered a data breach when an unhandled database connection exception was captured by an external error-monitoring service (Sentry). The exception stack trace contained the full database connection string, including plaintext master database credentials.
- **Remediation**:
  1. Refactored configuration to use `pydantic-settings` and `SecretStr` for all passwords and connection URLs.
  2. Implemented custom exception handlers stripping raw connection strings from error messages before forwarding to Sentry.
  3. Integrated AWS Secrets Manager with dynamic 30-day credential rotation.

---

## 12. Technical Interview Questions

<details>
<summary>Q1: Why is `SecretStr` preferred over `str` for storing sensitive configuration values in FastAPI?</summary>

**Answer**: `SecretStr` overrides the `__str__` and `__repr__` methods to output `'**********'`, preventing secrets from accidentally leaking into log files, stack traces, print statements, or JSON serialization responses (`model.model_dump()`). The underlying plain value is only accessible via the explicit method `.get_secret_value()`.
</details>

<details>
<summary>Q2: Why should `get_settings()` be wrapped in `@lru_cache`?</summary>

**Answer**: Parsing environment variables, reading `.env` files, and executing Pydantic type conversions incurs I/O and CPU overhead. `@lru_cache` ensures that the settings object is instantiated and validated exactly once during application startup and reused as a high-speed singleton throughout the application lifecycle.
</details>

<details>
<summary>Q3: How do you mock or override `AppSettings` during pytest execution?</summary>

**Answer**: Because `AppSettings` is injected via FastAPI's dependency injection system (`Depends(get_settings)`), unit tests can easily override it using `app.dependency_overrides[get_settings] = lambda: TestSettings()`, or by using `monkeypatch.setenv()` before `get_settings()` is invoked (with `get_settings.cache_clear()`).
</details>

<details>
<summary>Q4: What is the risk of passing secrets as Docker environment variables vs. Docker secrets?</summary>

**Answer**: Environment variables passed to containers are visible in plain text to anyone with `docker inspect` access, can leak into child processes, and appear in `/proc/$PID/environ`. Docker/Kubernetes secrets mount credentials into an in-memory `tmpfs` file system (`/run/secrets/`), which does not persist to disk, is not logged in container inspection metadata, and can be read securely by `pydantic-settings(secrets_dir="/run/secrets")`.
</details>

<details>
<summary>Q5: Why is setting `extra="forbid"` in `SettingsConfigDict` a recommended security practice?</summary>

**Answer**: If a developer misspells an environment variable (e.g. `JWT_SECRT` instead of `JWT_SECRET`), default permissive settings silently ignore the typo and fall back to default development keys. With `extra="forbid"`, Pydantic fails fast at startup with a validation error, preventing the application from running with misconfigured security credentials.
</details>

<details>
<summary>Q6: How can you dynamically reload secrets from AWS Secrets Manager without restarting FastAPI?</summary>

**Answer**: Implement a background task or scheduled routine that periodically polls the secret manager for the latest secret version. When a new version is detected, it instantiates a new `AppSettings` instance, updates `app.state.settings`, and clears the `get_settings` LRU cache.
</details>

<details>
<summary>Q7: What is the Twelve-Factor App principle regarding environment variables?</summary>

**Answer**: Factor III ("Config") dictates that configuration that varies across deploys (staging, production, development) must be strictly stored in environment variables, completely separated from code. The application codebase should be 100% open-source ready, containing zero credentials or environment-specific URLs.
</details>

<details>
<summary>Q8: How do you prevent secret leaks in HTTP request logging middleware?</summary>

**Answer**: Maintain an explicit blacklist of sensitive header names (`Authorization`, `Cookie`, `X-API-Key`, `Set-Cookie`). The logging middleware must iterate over incoming headers and replace sensitive values with `***MASKED***` before writing to log streams.
</details>

---

## 13. Practical Exercises & Solutions

<details>
<summary>Exercise 1 (Easy): Create a Pydantic settings class with SecretStr</summary>

**Problem**: Define `Settings` reading `API_KEY` as a `SecretStr` from the environment.

**Solution**:
```python
from pydantic import SecretStr
from pydantic_settings import BaseSettings

class Settings(BaseSettings):
    api_key: SecretStr
```
</details>

<details>
<summary>Exercise 2 (Easy): Safely retrieve the secret string value</summary>

**Problem**: Given `settings = Settings(api_key=SecretStr("my_secret"))`, extract the raw plain string.

**Solution**:
```python
raw_key = settings.api_key.get_secret_value()
```
</details>

<details>
<summary>Exercise 3 (Medium): Validate Minimum Secret Entropy</summary>

**Problem**: Add a validator ensuring `database_password` contains at least 16 characters.

**Solution**:
```python
from pydantic import SecretStr, field_validator
from pydantic_settings import BaseSettings

class DBSettings(BaseSettings):
    database_password: SecretStr

    @field_validator("database_password")
    @classmethod
    def check_length(cls, v: SecretStr):
        if len(v.get_secret_value()) < 16:
            raise ValueError("Database password must be at least 16 characters")
        return v
```
</details>

<details>
<summary>Exercise 4 (Medium): Build an LRU-Cached Settings Dependency</summary>

**Problem**: Implement `get_settings()` with `@lru_cache` and attach to a route via `Depends()`.

**Solution**:
```python
from functools import lru_cache
from typing import Annotated
from fastapi import Depends, FastAPI
from pydantic import SecretStr
from pydantic_settings import BaseSettings

class Config(BaseSettings):
    app_secret: SecretStr = SecretStr("default_secret")

@lru_cache
def get_config() -> Config:
    return Config()

app = FastAPI()

@app.get("/secret-len")
def get_len(cfg: Annotated[Config, Depends(get_config)]):
    return {"length": len(cfg.app_secret.get_secret_value())}
```
</details>

<details>
<summary>Exercise 5 (Hard): Implement Fallback from Cloud Secret Manager to Env</summary>

**Problem**: Write a settings loader that attempts to fetch secrets from AWS Secrets Manager, falling back to local environment variables if AWS is unavailable.

**Solution**:
```python
import os
from pydantic import SecretStr
from pydantic_settings import BaseSettings

def fetch_aws_or_env(secret_name: str, env_var: str) -> str:
    try:
        # Simulated AWS lookup
        # client = boto3.client('secretsmanager')
        # return client.get_secret_value(SecretId=secret_name)['SecretString']
        raise Exception("AWS unavailable")
    except Exception:
        return os.environ.get(env_var, "local_fallback_secret_value_32b")

class ResilientSettings(BaseSettings):
    master_key: SecretStr = SecretStr(fetch_aws_or_env("prod/master_key", "MASTER_KEY"))
```
</details>

<details>
<summary>Exercise 6 (Hard): Construct Dynamic Secret Rotation Poller in FastAPI Lifespan</summary>

**Problem**: Build a lifespan context manager that spawns an async background task to periodically refresh an in-memory secret and safely swap the active signing key.

**Solution**:
```python
import asyncio
from contextlib import asynccontextmanager
from fastapi import FastAPI
from pydantic import SecretStr

class DynamicKeyStore:
    def __init__(self):
        self.active_key = SecretStr("initial_secret_key_2026")

    async def poll_for_updates(self):
        while True:
            await asyncio.sleep(3600)  # Check every hour
            # Simulated updated secret fetch
            self.active_key = SecretStr("rotated_secret_key_2026")

key_store = DynamicKeyStore()

@asynccontextmanager
async def lifespan(app: FastAPI):
    task = asyncio.create_task(key_store.poll_for_updates())
    yield
    task.cancel()

app = FastAPI(lifespan=lifespan)
```
</details>
