# Module 07: Authentication & Security — 10: HTTP Basic and Digest Authentication (`10-http-basic-and-digest-auth`)

> HTTP Basic Authentication (RFC 7617) and HTTP Digest Authentication (RFC 7616) represent standard challenge-response mechanisms built directly into the HTTP specification. In FastAPI, `fastapi.security.HTTPBasic` provides seamless integration with browser authentication dialogs and machine endpoints (such as Prometheus `/metrics` scrapers).

| Field | Value |
|---|---|
| **Category** | Authentication & Security |
| **Target Level** | Intermediate / Production Engineer |
| **FastAPI Version** | FastAPI 0.165.0+ |
| **Starlette Version**| Starlette 1.10.0+ |
| **Pydantic Version** | Pydantic v2.14.4+ (pydantic-settings 2.16.3+) |
| **Python Version**   | Python 3.12+ (3.13 / 3.14 compatible, Free-Threading verified) |
| **Tested With**      | httpx 0.28+, pytest 9.1+, secrets/hmac |

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

HTTP Basic Authentication is a simple, standardized protocol defined in RFC 7617:
- **Base64 Transport**: The client encodes `username:password` as an ASCII Base64 string and transmits it via `Authorization: Basic <base64_string>`.
- **Browser Native Dialog**: When a server responds with HTTP 401 and `WWW-Authenticate: Basic realm="Restricted"`, web browsers automatically render an OS-level login prompt.
- **Production Use Cases**:
  1. Internal observability scraping endpoints (Prometheus `/metrics`).
  2. Ingestion webhooks from legacy enterprise systems.
  3. Internal developer documentation staging environments.
- **TLS Mandate**: Because Base64 is encoding, not encryption, Basic Auth MUST NEVER be used over unencrypted HTTP.

---

## 2. Core Concepts & Architecture

```
+-----------------------------------------------------------------------------------+
|                   HTTP Basic Challenge-Response Handshake                         |
+-----------------------------------------------------------------------------------+
|  Client (Browser / Scraper)                       FastAPI Application             |
|         │                                                  │                      |
|         │ 1. GET /metrics                                  │                      |
|         ├─────────────────────────────────────────────────>│                      |
|         │                                                  │                      |
|         │ 2. 401 Unauthorized                              │                      |
|         │    Header: WWW-Authenticate: Basic realm="Ops"   │                      |
|         │<─────────────────────────────────────────────────┤                      |
|         │                                                  │                      |
|         │ 3. GET /metrics                                  │                      |
|         │    Header: Authorization: Basic dXNlcjpzZWNyZXQ= │                      |
|         ├─────────────────────────────────────────────────>│                      |
|         │                                                  │                      |
|         │                                                  │ 4. HTTPBasic Extracts|
|         │                                                  │    Base64 Decodes    |
|         │                                                  │    secrets.compare_digest
|         │ 5. 200 OK (Metrics Payload)                      │                      |
|         │<─────────────────────────────────────────────────┤                      |
+-----------------------------------------------------------------------------------+
```

---

## 3. How It Works Internally

1. **`fastapi.security.HTTPBasic`**:
   - Subclass of `SecurityBase`.
   - Populates OpenAPI `components.securitySchemes` with `type: http`, `scheme: basic`.
   - Inspects `request.headers.get("Authorization")`. If missing or does not start with `Basic `, it raises `HTTPException(401)` with the `WWW-Authenticate: Basic` header (if `auto_error=True`).
2. **`fastapi.security.HTTPBasicCredentials`**:
   - A Pydantic dataclass containing `username: str` and `password: str`.
   - Decodes the raw Base64 string safely and splits on the first colon (`:`).

---

## 4. Syntax & API Reference

```python
from typing import Annotated
from fastapi import Depends, FastAPI, HTTPException, Security, status
from fastapi.security import HTTPBasic, HTTPBasicCredentials
import secrets

app = FastAPI()
security = HTTPBasic(realm="Metrics", auto_error=True)

def authenticate(credentials: Annotated[HTTPBasicCredentials, Depends(security)]) -> str:
    # Constant-time comparison
    is_user_ok = secrets.compare_digest(credentials.username, "ops_user")
    is_pass_ok = secrets.compare_digest(credentials.password, "super_secret_pw")
    
    if not (is_user_ok and is_pass_ok):
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Incorrect username or password",
            headers={"WWW-Authenticate": 'Basic realm="Metrics"'},
        )
    return credentials.username
```

---

## 5. Verified Code Examples

### Example 1: Minimal Prometheus Metrics Endpoint Guard

```python
from typing import Annotated
import secrets
from fastapi import Depends, FastAPI, HTTPException, status
from fastapi.security import HTTPBasic, HTTPBasicCredentials

app = FastAPI()
security = HTTPBasic(realm="Telemetry")

PROMETHEUS_USER = "prometheus_scraper"
PROMETHEUS_PASS = "PrometheusSecretToken2026!"

async def verify_prometheus_credentials(
    credentials: Annotated[HTTPBasicCredentials, Depends(security)]
) -> str:
    correct_username = secrets.compare_digest(credentials.username, PROMETHEUS_USER)
    correct_password = secrets.compare_digest(credentials.password, PROMETHEUS_PASS)

    if not (correct_username and correct_password):
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Unauthorized scraper credentials",
            headers={"WWW-Authenticate": 'Basic realm="Telemetry"'},
        )
    return credentials.username

@app.get("/metrics")
async def get_metrics(_: Annotated[str, Depends(verify_prometheus_credentials)]):
    return "# HELP http_requests_total Total HTTP requests\nhttp_requests_total 42918"
```

### Example 2: Enterprise Database-Backed Basic Auth with Argon2 Hashing

```python
from typing import Annotated
from fastapi import APIRouter, Depends, HTTPException, status
from fastapi.security import HTTPBasic, HTTPBasicCredentials
from argon2 import PasswordHasher
from anyio import to_thread

router = APIRouter(prefix="/internal")
security = HTTPBasic(realm="Internal")
hasher = PasswordHasher()

# Simulated database
INTERNAL_ACCOUNTS = {
    "sync_bot": hasher.hash("SecureBotPassword123"),
}

async def authenticate_internal_bot(
    creds: Annotated[HTTPBasicCredentials, Depends(security)]
) -> str:
    stored_hash = INTERNAL_ACCOUNTS.get(creds.username)
    if not stored_hash:
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Invalid credentials",
            headers={"WWW-Authenticate": 'Basic realm="Internal"'},
        )

    try:
        await to_thread.run_sync(hasher.verify, stored_hash, creds.password)
    except Exception:
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Invalid credentials",
            headers={"WWW-Authenticate": 'Basic realm="Internal"'},
        )

    return creds.username

@router.post("/trigger-sync")
async def trigger_sync(bot: Annotated[str, Depends(authenticate_internal_bot)]):
    return {"status": "sync_started", "triggered_by": bot}
```

### Example 3: Edge Case / Boundary — Optional Basic Auth with `auto_error=False`

```python
from typing import Annotated
from fastapi import Depends, FastAPI
from fastapi.security import HTTPBasic, HTTPBasicCredentials
import secrets

app = FastAPI()
optional_basic = HTTPBasic(auto_error=False)

@app.get("/doc-preview")
async def doc_preview(
    credentials: Annotated[HTTPBasicCredentials | None, Depends(optional_basic)]
):
    if credentials:
        if secrets.compare_digest(credentials.username, "dev") and secrets.compare_digest(credentials.password, "pass"):
            return {"mode": "developer_full_view"}
    return {"mode": "public_restricted_view"}
```

### Example 4: Failure Case — Rejecting Malformed Base64 Header

```python
# If a client sends an invalid Base64 string in the Authorization header:
# GET /metrics
# Header: Authorization: Basic not_valid_base64_%%%
#
# Starlette/FastAPI raises:
# HTTP 401 Unauthorized
# WWW-Authenticate: Basic
# {
#   "detail": "Invalid authentication credentials"
# }
```

### Example 5: Idiomatic Modern Pattern with Python 3.12+ `Annotated` Type Aliases

```python
from typing import Annotated
from fastapi import Depends, FastAPI
from fastapi.security import HTTPBasic, HTTPBasicCredentials
import secrets

app = FastAPI()
security = HTTPBasic()

def require_ops_login(creds: Annotated[HTTPBasicCredentials, Depends(security)]) -> str:
    if not (secrets.compare_digest(creds.username, "admin") and secrets.compare_digest(creds.password, "secret")):
        raise HTTPException(status_code=401, headers={"WWW-Authenticate": "Basic"})
    return creds.username

# Self-documenting type alias
OpsUser = Annotated[str, Depends(require_ops_login)]

@app.get("/system/status")
async def system_status(user: OpsUser):
    return {"status": "healthy", "operator": user}
```

---

## 6. Edge Cases, Pitfalls & Failure Modes

1. **Cleartext Transmission Vulnerability**: Transmitting Basic credentials over plain HTTP exposes raw credentials to network sniffers. Modern servers must enforce TLS redirection before authentication checks.
2. **Timing Attacks on Passwords**: Using `credentials.password == "secret"` terminates at the first differing byte, allowing brute-force attackers to reconstruct passwords byte-by-byte. Always use `secrets.compare_digest`.
3. **Missing `WWW-Authenticate` Header**: Omitting the header prevents browsers and scrapers from prompting the user or recognizing the required scheme.

---

## 7. Common Mistakes Table

| Mistake | Why It Fails | Error / Diagnostic | Correct Idiomatic Fix |
|---|---|---|---|
| 1. Using standard `==` string equality | Vulnerable to side-channel timing attacks | Leaked credentials via response timing | Use `secrets.compare_digest` |
| 2. Omitting `WWW-Authenticate` on 401 | Browser does not show native login popup | Blank 401 page without browser modal | Include `headers={"WWW-Authenticate": "Basic"}` |
| 3. Using Basic Auth over plain HTTP | Base64 is easily decoded by network sniffers | Critical security vulnerability | Enforce HTTPS via TLS termination proxy |
| 4. Hardcoding credentials in source code | Git history leaks production passwords | Compromised credentials | Load credentials from `pydantic-settings` |
| 5. Synchronous password hashing on event loop | Blocks asyncio workers during login | Worker timeouts on scraper storms | Use `anyio.to_thread.run_sync` |
| 6. Storing plain password in database | Database breach leaks all internal service passwords | Severe incident | Store Argon2id or Bcrypt hashes |

---

## 8. Performance & Scalability Considerations

- **Base64 Decoding Speed**: Python decodes Base64 in sub-microsecond time. When paired with `secrets.compare_digest` against static in-memory credentials, verification takes <0.01ms.
- **Prometheus Scraper Impact**: High-frequency scrapers (every 5 seconds) should not hit relational databases on every scrape. Cache validated scraper credentials in memory.

---

## 9. Security & OWASP Guardrails

- **OWASP API2:2023 Broken Authentication**: Enforce strong password complexity even for internal scrapers and service accounts.
- **Realm Scoping**: Use descriptive `realm` values (`realm="Prometheus Metrics"`) to isolate browser credential caching domains.

---

## 10. Testing Strategies & Pytest Recipes

```python
import pytest
from httpx import ASGITransport, AsyncClient
from fastapi import FastAPI

@pytest.mark.asyncio
async def test_basic_auth_success(app: FastAPI):
    transport = ASGITransport(app=app)
    # httpx handles Basic Auth tuples natively
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        res = await client.get("/metrics", auth=("prometheus_scraper", "PrometheusSecretToken2026!"))
        assert res.status_code == 200
        assert "http_requests_total" in res.text

@pytest.mark.asyncio
async def test_basic_auth_failure(app: FastAPI):
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        res = await client.get("/metrics", auth=("wrong_user", "wrong_pass"))
        assert res.status_code == 401
        assert "WWW-Authenticate" in res.headers
```

---

## 11. Real-World Case Study

A high-frequency trading platform exposed Prometheus metrics for Kubernetes autoscaling. A junior developer configured `/metrics` with Basic Auth, but checked the password using standard Python equality (`credentials.password == env_pw`). A penetration testing team captured minute timing variations over 50,000 requests and reconstructed the password in 4 hours.
- **Fix**:
  1. Replaced string comparison with `secrets.compare_digest`.
  2. Moved `/metrics` to a dedicated private internal port (not exposed to ingress).
  3. Enforced mutual TLS (mTLS) for Prometheus scrapers.

---

## 12. Technical Interview Questions

<details>
<summary>Q1: How does HTTP Basic Authentication differ mechanically from Bearer Token Authentication?</summary>

**Answer**: In HTTP Basic, the client transmits the actual user credentials (`username:password`) Base64-encoded on every single request. In Bearer Token authentication, the client authenticates once, receives a cryptographically signed token (JWT) with an expiration timestamp, and transmits the token instead of raw credentials.
</details>

<details>
<summary>Q2: What is the purpose of the `realm` attribute in the `WWW-Authenticate: Basic` header?</summary>

**Answer**: The `realm` attribute indicates the scope or protective domain of the resource. Browsers use the realm string to organize and cache credentials; if multiple paths on the same host share the same realm, the browser automatically supplies the cached credentials without re-prompting the user.
</details>

<details>
<summary>Q3: Why is Base64 encoding NOT considered encryption?</summary>

**Answer**: Base64 is a reversible binary-to-text representation scheme that requires no secret key to decode. Anyone intercepting a Base64 string can decode it instantly. Therefore, Basic Auth provides zero confidentiality without underlying TLS/HTTPS encryption.
</details>

<details>
<summary>Q4: What is HTTP Digest Authentication, and why is it rarely used in modern cloud APIs?</summary>

**Answer**: RFC 7616 Digest Authentication uses MD5/SHA-256 challenge-response nonces to prove password ownership without transmitting the password over the wire. It is rarely used today because TLS is ubiquitous (making transport encryption standard), and Digest authentication is complex, stateful (managing nonces and counts), incompatible with modern one-way password hashing algorithms (Argon2), and superseded by OAuth2/JWT.
</details>

<details>
<summary>Q5: Why should `secrets.compare_digest` be used instead of standard string equality?</summary>

**Answer**: Standard string comparison terminates on the first mismatched byte, creating measurable timing discrepancies. `secrets.compare_digest` compares byte sequences in constant time, preventing timing-based side-channel attacks.
</details>

<details>
<summary>Q6: How does `fastapi.security.HTTPBasic` interact with Swagger UI?</summary>

**Answer**: It populates the OpenAPI `securitySchemes` with `type: http, scheme: basic`. In Swagger UI, clicking "Authorize" prompts for a username and password, which Swagger UI Base64-encodes and sends in the `Authorization: Basic ...` header.
</details>

<details>
<summary>Q7: How do you support both Basic Auth and Bearer Token Auth on the same endpoint?</summary>

**Answer**: Initialize both schemes with `auto_error=False`. In a composite dependency, check for `HTTPBasicCredentials` first; if absent, inspect `HTTPAuthorizationCredentials` (Bearer). If both are absent, raise HTTP 401 with headers advertising both schemes.
</details>

<details>
<summary>Q8: Can Basic Auth credentials be passed in URLs?</summary>

**Answer**: Historically, URLs supported `http://user:pass@host/path`. However, RFC 3986 and modern browsers deprecate and actively strip inline credentials in URLs due to severe security risks (phishing, URL spoofing, and access log leakage).
</details>

---

## 13. Practical Exercises & Solutions

<details>
<summary>Exercise 1 (Easy): Create a basic auth protected `/health/admin` route</summary>

**Problem**: Protect `/health/admin` requiring username `root` and password `toor` using `HTTPBasic`.

**Solution**:
```python
from typing import Annotated
from fastapi import FastAPI, HTTPException, Security, status
from fastapi.security import HTTPBasic, HTTPBasicCredentials
import secrets

app = FastAPI()
security = HTTPBasic()

@app.get("/health/admin")
def admin_health(creds: Annotated[HTTPBasicCredentials, Security(security)]):
    if not (secrets.compare_digest(creds.username, "root") and secrets.compare_digest(creds.password, "toor")):
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, headers={"WWW-Authenticate": "Basic"})
    return {"status": "all_systems_go"}
```
</details>

<details>
<summary>Exercise 2 (Easy): Customize the HTTP Basic Realm</summary>

**Problem**: Configure `HTTPBasic` to return `WWW-Authenticate: Basic realm="Internal Production Gateway"`.

**Solution**:
```python
from fastapi import FastAPI, HTTPException, Security
from fastapi.security import HTTPBasic, HTTPBasicCredentials

app = FastAPI()
basic = HTTPBasic(realm="Internal Production Gateway")

@app.get("/secure")
def secure_route(creds: HTTPBasicCredentials = Security(basic)):
    raise HTTPException(status_code=401, headers={"WWW-Authenticate": 'Basic realm="Internal Production Gateway"'})
```
</details>

<details>
<summary>Exercise 3 (Medium): Dual Authenticator (Basic Auth OR API Key)</summary>

**Problem**: Create a dependency that accepts either HTTP Basic Auth or an `X-API-Key` header.

**Solution**:
```python
from typing import Annotated
from fastapi import Depends, HTTPException, Security, status
from fastapi.security import HTTPBasic, HTTPBasicCredentials, APIKeyHeader
import secrets

basic_auth = HTTPBasic(auto_error=False)
api_key = APIKeyHeader(name="X-API-Key", auto_error=False)

async def authenticate_dual(
    basic_creds: Annotated[HTTPBasicCredentials | None, Security(basic_auth)],
    key: Annotated[str | None, Security(api_key)],
) -> str:
    if key and secrets.compare_digest(key, "valid_api_key_123"):
        return "machine_client"
    if basic_creds:
        if secrets.compare_digest(basic_creds.username, "admin") and secrets.compare_digest(basic_creds.password, "pass"):
            return basic_creds.username
    raise HTTPException(
        status_code=status.HTTP_401_UNAUTHORIZED,
        detail="Must provide valid Basic credentials or X-API-Key header",
        headers={"WWW-Authenticate": "Basic, ApiKey"}
    )
```
</details>

<details>
<summary>Exercise 4 (Medium): Build an In-Memory Scraper Credential Registry</summary>

**Problem**: Maintain a dictionary of authorized scraper usernames and hashed passwords, validating incoming scrapers against it.

**Solution**:
```python
import hashlib
import hmac
from typing import Annotated
from fastapi import Depends, HTTPException, status
from fastapi.security import HTTPBasic, HTTPBasicCredentials

security = HTTPBasic()
REGISTRY = {
    "prometheus": hashlib.sha256(b"prom_pass").hexdigest(),
    "datadog": hashlib.sha256(b"datadog_pass").hexdigest(),
}

async def verify_scraper(creds: Annotated[HTTPBasicCredentials, Depends(security)]):
    stored_hash = REGISTRY.get(creds.username)
    if not stored_hash:
        raise HTTPException(status_code=401, headers={"WWW-Authenticate": "Basic"})
    
    computed = hashlib.sha256(creds.password.encode()).hexdigest()
    if not hmac.compare_digest(computed, stored_hash):
        raise HTTPException(status_code=401, headers={"WWW-Authenticate": "Basic"})
    return creds.username
```
</details>

<details>
<summary>Exercise 5 (Hard): Implement Scraper Rate Limiting by Basic Auth Username</summary>

**Problem**: Implement a dependency that tracks request timestamps in a dictionary per Basic Auth username, rejecting users with 429 if they exceed 10 requests per minute.

**Solution**:
```python
import time
from typing import Annotated
from fastapi import Depends, HTTPException, status
from fastapi.security import HTTPBasic, HTTPBasicCredentials

security = HTTPBasic()
REQUEST_HISTORY: dict[str, list[float]] = {}

def rate_limit_basic_user(creds: Annotated[HTTPBasicCredentials, Depends(security)]) -> str:
    now = time.time()
    user = creds.username
    history = REQUEST_HISTORY.setdefault(user, [])
    
    # Prune timestamps older than 60 seconds
    REQUEST_HISTORY[user] = [t for t in history if now - t < 60]
    
    if len(REQUEST_HISTORY[user]) >= 10:
        raise HTTPException(status_code=status.HTTP_429_TOO_MANY_REQUESTS, detail="Scrape quota exceeded")
    
    REQUEST_HISTORY[user].append(now)
    return user
```
</details>

<details>
<summary>Exercise 6 (Hard): Construct Mutual Authentication Simulator (Basic + IP CIDR + User-Agent Verification)</summary>

**Problem**: Build a strict enterprise webhook ingestion dependency verifying Basic credentials, verifying that the client IP is in `192.168.0.0/16`, and verifying a designated `User-Agent` signature.

**Solution**:
```python
import ipaddress
import secrets
from typing import Annotated
from fastapi import Depends, Header, HTTPException, Request, Security, status
from fastapi.security import HTTPBasic, HTTPBasicCredentials

security = HTTPBasic()
ALLOWED_SUBNET = ipaddress.ip_network("192.168.0.0/16")

async def enterprise_webhook_guard(
    request: Request,
    creds: Annotated[HTTPBasicCredentials, Security(security)],
    user_agent: Annotated[str, Header()],
) -> str:
    # 1. IP Check
    client_ip = ipaddress.ip_address(request.client.host if request.client else "127.0.0.1")
    if client_ip not in ALLOWED_SUBNET:
        raise HTTPException(status_code=status.HTTP_403_FORBIDDEN, detail="IP not allowed")

    # 2. User-Agent signature check
    if not user_agent.startswith("EnterpriseWebhook/"):
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail="Invalid User-Agent")

    # 3. Basic Auth verification
    if not (secrets.compare_digest(creds.username, "wh_user") and secrets.compare_digest(creds.password, "wh_secret")):
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, headers={"WWW-Authenticate": "Basic"})

    return creds.username
```
</details>
