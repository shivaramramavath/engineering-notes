# Module 07: Authentication & Security — 04: OAuth2 with Password Flow (`04-oauth2-with-password-flow`)

> The OAuth2 Password Flow (Resource Owner Password Credentials — ROPC) is natively integrated into FastAPI via `OAuth2PasswordBearer` and `OAuth2PasswordRequestForm`. While deprecated by RFC 6749 for untrusted third-party clients, it powers first-party authentication and populates Swagger UI's interactive "Authorize" modal.

| Field | Value |
|---|---|
| **Category** | Authentication & Security |
| **Target Level** | Intermediate / Production Engineer |
| **FastAPI Version** | FastAPI 0.165.0+ |
| **Starlette Version**| Starlette 1.10.0+ |
| **Pydantic Version** | Pydantic v2.14.4+ (pydantic-settings 2.16.3+) |
| **Python Version**   | Python 3.12+ (3.13 / 3.14 compatible, Free-Threading verified) |
| **Tested With**      | python-multipart 0.0.20+, httpx 0.28+, pytest 9.1+ |

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

FastAPI provides first-class support for OAuth2 specifications. The Resource Owner Password Credentials flow allows a client to exchange a user's plain credentials (`username` and `password`) directly for an access token:
- **FastAPI Core Integration**: Using `OAuth2PasswordBearer` informs FastAPI that protected routes require a Bearer token obtained from a specific relative or absolute `tokenUrl`.
- **OpenAPI Schema Binding**: FastAPI automatically generates the `securitySchemes` block in `/openapi.json`, illuminating the green **"Authorize"** button in Swagger UI.
- **Form Data Requirement**: The OAuth2 standard specifies that token endpoints must accept credentials encoded as `application/x-www-form-urlencoded` rather than JSON. This requires `python-multipart` to be installed.

---

## 2. Core Concepts & Architecture

```
+-----------------------------------------------------------------------------------+
|                        OAuth2 Password Flow Architecture                          |
+-----------------------------------------------------------------------------------+
|  Client (Browser / Swagger UI)            FastAPI Endpoint          Auth Service  |
|         │                                        │                         │      |
|         │ 1. POST /token (Form-URLEncoded)       │                         │      |
|         │    username="alice"&password="secret"  │                         │      |
|         ├───────────────────────────────────────>│                         │      |
|         │                                        │ 2. Verify Credentials   │      |
|         │                                        ├────────────────────────>│      |
|         │                                        │ <── User OK ────────────┤      |
|         │ 3. 200 OK: {"access_token":"...",      │                         │      |
|         │             "token_type":"bearer"}     │                         │      |
|         │<───────────────────────────────────────┤                         │      |
|         │                                        │                         │      |
|         │ 4. GET /users/me                       │                         │      |
|         │    Header: Authorization: Bearer <tok> │                         │      |
|         ├───────────────────────────────────────>│                         │      |
|         │                                        │ 5. Validate Token       │      |
|         │                                        │    Extract Subject      │      |
|         │ 6. 200 OK: User Profile Response       │                         │      |
|         │<───────────────────────────────────────┤                         │      |
+-----------------------------------------------------------------------------------+
```

---

## 3. How It Works Internally

1. **`OAuth2PasswordBearer`**:
   - Inherits from `fastapi.security.base.SecurityBase`.
   - Its `__call__(request: Request)` inspects `request.headers.get("Authorization")`.
   - Validates that the scheme is `Bearer`. Extracts the token string or raises `HTTPException(401)`.
2. **`OAuth2PasswordRequestForm`**:
   - A dependency class that extracts standard OAuth2 form parameters from the ASGI request body: `grant_type`, `username`, `password`, `scope`, `client_id`, and `client_secret`.
   - Uses `fastapi.params.Form()` internally, which delegates form parsing to Starlette and `python-multipart`.

---

## 4. Syntax & API Reference

```python
from typing import Annotated
from fastapi import Depends, FastAPI, HTTPException, status
from fastapi.security import OAuth2PasswordBearer, OAuth2PasswordRequestForm
from pydantic import BaseModel

app = FastAPI()

# 1. Scheme Definition
oauth2_scheme = OAuth2PasswordBearer(
    tokenUrl="token",  # Relative or absolute endpoint URL
    scheme_name="OAuth2Password",
    scopes={"me": "Read own info", "admin": "Full system control"},
    auto_error=True
)

# 2. Standard OAuth2 Token Response Model
class Token(BaseModel):
    access_token: str
    token_type: str = "bearer"

# 3. Token Issuance Endpoint
@app.post("/token", response_model=Token)
async def login(form_data: Annotated[OAuth2PasswordRequestForm, Depends()]):
    user = authenticate(form_data.username, form_data.password)
    ...
```

---

## 5. Verified Code Examples

### Example 1: Minimal Working Password Flow with Swagger UI

```python
from typing import Annotated
from fastapi import Depends, FastAPI, HTTPException, status
from fastapi.security import OAuth2PasswordBearer, OAuth2PasswordRequestForm
from pydantic import BaseModel

app = FastAPI(title="OAuth2 Password Flow Demo")
oauth2_scheme = OAuth2PasswordBearer(tokenUrl="token")

class TokenResponse(BaseModel):
    access_token: str
    token_type: str

FAKE_USERS_DB = {"admin": "supersecretpass"}

@app.post("/token", response_model=TokenResponse)
async def login(form_data: Annotated[OAuth2PasswordRequestForm, Depends()]):
    stored_password = FAKE_USERS_DB.get(form_data.username)
    if not stored_password or stored_password != form_data.password:
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail="Incorrect username or password",
        )
    return TokenResponse(access_token=f"valid-token-for-{form_data.username}", token_type="bearer")

@app.get("/users/me")
async def read_users_me(token: Annotated[str, Depends(oauth2_scheme)]):
    return {"token": token, "status": "authenticated"}
```

### Example 2: Enterprise Password Flow with Argon2 and JWT Generation

```python
from datetime import datetime, timedelta, timezone
from typing import Annotated
from fastapi import APIRouter, Depends, HTTPException, status
from fastapi.security import OAuth2PasswordBearer, OAuth2PasswordRequestForm
from pydantic import BaseModel
import jwt
from argon2 import PasswordHasher

router = APIRouter(tags=["Authentication"])
oauth2_scheme = OAuth2PasswordBearer(tokenUrl="/auth/token")
hasher = PasswordHasher()

SECRET = "super-secret-cluster-key"
ALGORITHM = "HS256"

# Database mock
MOCK_USER = {
    "username": "charlie@enterprise.com",
    "password_hash": hasher.hash("SecurePass2026!"),
    "id": "usr_9988",
    "is_active": True,
}

class Token(BaseModel):
    access_token: str
    token_type: str = "bearer"
    expires_in: int

@router.post("/auth/token", response_model=Token)
async def login_for_access_token(
    form_data: Annotated[OAuth2PasswordRequestForm, Depends()]
):
    if form_data.username != MOCK_USER["username"]:
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail="Invalid credentials")
    
    try:
        hasher.verify(MOCK_USER["password_hash"], form_data.password)
    except Exception:
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail="Invalid credentials")

    exp = datetime.now(timezone.utc) + timedelta(minutes=15)
    token = jwt.encode(
        {"sub": MOCK_USER["id"], "exp": exp, "scope": " ".join(form_data.scopes)},
        SECRET,
        algorithm=ALGORITHM,
    )
    return Token(access_token=token, token_type="bearer", expires_in=900)
```

### Example 3: Edge Case / Boundary — Handling `client_id` and `client_secret` in Form

```python
from typing import Annotated
from fastapi import Depends, HTTPException, status
from fastapi.security import OAuth2PasswordRequestForm

ALLOWED_CLIENTS = {"mobile_app_ios": "ios_sec_4455", "desktop_electron": "elec_sec_9911"}

async def authenticate_with_client_credentials(
    form_data: Annotated[OAuth2PasswordRequestForm, Depends()]
):
    # Validating optional OAuth2 Client Authentication
    if form_data.client_id:
        expected_secret = ALLOWED_CLIENTS.get(form_data.client_id)
        if not expected_secret or expected_secret != form_data.client_secret:
            raise HTTPException(
                status_code=status.HTTP_401_UNAUTHORIZED,
                detail="Invalid client credentials",
            )
    return form_data
```

### Example 4: Failure Case — Rejecting JSON Payloads on OAuth2 Token Route

```python
# The OAuth2 standard strictly forbids application/json on token endpoints
# If a client sends JSON, FastAPI/Starlette will raise 422 Unprocessable Entity
# Request: POST /token Content-Type: application/json {"username":"a","password":"b"}
# Response:
# 422 Unprocessable Entity
# {
#   "detail": [
#     {"type": "missing", "loc": ["body", "username"], "msg": "Field required"}
#   ]
# }
```

### Example 5: Idiomatic Modern Pattern with `OAuth2PasswordBearer` and Scopes

```python
from typing import Annotated
from fastapi import Depends, FastAPI, HTTPException, Security, status
from fastapi.security import OAuth2PasswordBearer, SecurityScopes
import jwt

app = FastAPI()

oauth2_scheme = OAuth2PasswordBearer(
    tokenUrl="token",
    scopes={
        "read:reports": "Read financial analytics",
        "write:reports": "Publish financial data",
    },
)

async def get_current_user_with_scopes(
    security_scopes: SecurityScopes,
    token: Annotated[str, Depends(oauth2_scheme)],
) -> dict:
    try:
        claims = jwt.decode(token, "secret", algorithms=["HS256"])
    except jwt.PyJWTError:
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="Invalid token")

    token_scopes = claims.get("scopes", [])
    for required_scope in security_scopes.scopes:
        if required_scope not in token_scopes:
            raise HTTPException(
                status_code=status.HTTP_403_FORBIDDEN,
                detail=f"Not enough permissions: {required_scope} required",
            )
    return claims

@app.get("/reports")
async def get_reports(
    user: Annotated[dict, Security(get_current_user_with_scopes, scopes=["read:reports"])]
):
    return {"data": "confidential_reports"}
```

---

## 6. Edge Cases, Pitfalls & Failure Modes

1. **Missing `python-multipart` Dependency**: Attempting to use `OAuth2PasswordRequestForm` without `python-multipart` causes runtime crashes: `RuntimeError: Form data requires "python-multipart" to be installed.`
2. **Relative `tokenUrl` Path Issues**: If `tokenUrl="token"` is relative, Swagger UI constructs the URL relative to the browser path. If Swagger is at `/docs`, it queries `/docs/token` instead of `/token`. Always use root-relative paths like `tokenUrl="/auth/token"`.
3. **HTTP 400 vs. 401 on Token Endpoint**: The OAuth2 RFC 6749 mandates returning HTTP **400 Bad Request** with `{"error": "invalid_grant"}` when password authentication fails on the token endpoint, NOT HTTP 401.

---

## 7. Common Mistakes Table

| Mistake | Why It Fails | Error / Diagnostic | Correct Idiomatic Fix |
|---|---|---|---|
| 1. Missing `python-multipart` | Starlette cannot parse form body | `RuntimeError: Form data requires "python-multipart"` | Install `uv add python-multipart` |
| 2. Sending JSON to `/token` | Endpoint expects `x-www-form-urlencoded` | 422 Unprocessable Entity (missing form fields) | Send request body as URL-encoded form data |
| 3. Returning 401 instead of 400 from `/token` | Violates OAuth2 RFC 6749 Section 5.2 | Non-standard OAuth2 client errors | Return 400 Bad Request with `invalid_grant` detail |
| 4. Hardcoding absolute domain in `tokenUrl` | Fails across staging, preview, and prod domains | CORS errors or broken Swagger UI auth | Use root-relative path (e.g. `tokenUrl="/auth/token"`) |
| 5. Failing to extract scopes in form | `form_data.scopes` is ignored | User scopes never embedded in token | Parse `form_data.scopes` and include in JWT claims |
| 6. Not using `Security` when requiring scopes | `SecurityScopes` parameter in dependency is empty | Endpoint fails to enforce permissions | Wrap dependency with `Security(..., scopes=[...])` |

---

## 8. Performance & Scalability Considerations

- **Form Parsing Overhead**: Form URL-encoded bodies are small (<1 KB) and parsed in microseconds without heavy JSON serialization overhead.
- **Rate Limiting**: The `/token` endpoint must be heavily rate-limited (e.g., 5 attempts/minute per IP) using Redis or SlowAPI to prevent credential brute-forcing.

---

## 9. Security & OWASP Guardrails

- **OAuth 2.0 Security Best Current Practice (BCP)**: The Password Grant is considered legacy. It SHOULD NOT be used for new public client applications (SPAs, mobile apps). Modern public apps should use **Authorization Code Flow with PKCE**.
- **First-Party Exception**: ROPC remains acceptable for strictly trusted internal first-party services and automated integration test harnesses.

---

## 10. Testing Strategies & Pytest Recipes

```python
import pytest
from httpx import ASGITransport, AsyncClient
from fastapi import FastAPI

@pytest.mark.asyncio
async def test_oauth2_token_endpoint(app: FastAPI):
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        # Must submit as data (form-urlencoded), NOT json
        response = await client.post(
            "/token",
            data={"username": "admin", "password": "supersecretpass"},
            headers={"Content-Type": "application/x-www-form-urlencoded"}
        )
        assert response.status_code == 200
        data = response.json()
        assert "access_token" in data
        assert data["token_type"] == "bearer"
```

---

## 11. Real-World Case Study

An enterprise enterprise developer platform used FastAPI to build an internal developer portal.
- **The Challenge**: Third-party OAuth providers were too slow for CLI development tools (`my-tool login`).
- **The Solution**: Implemented `OAuth2PasswordRequestForm` on `/v1/cli/token`. The CLI securely prompted developers for corporate credentials over TLS, received a 12-hour JWT, and stored it in the OS secure keychain. Swagger UI seamlessly leveraged the identical `/v1/cli/token` URL, providing zero-friction interactive API exploration for internal developers.

---

## 12. Technical Interview Questions

<details>
<summary>Q1: Why does FastAPI's `OAuth2PasswordRequestForm` require `python-multipart`?</summary>

**Answer**: The OAuth2 standard (RFC 6749) mandates that token requests use `application/x-www-form-urlencoded` payloads rather than JSON. Under the hood, Starlette relies on the `python-multipart` library to parse form streams and multipart data from ASGI scopes. Without it, Starlette raises a `RuntimeError`.
</details>

<details>
<summary>Q2: What is the significance of the `tokenUrl` parameter in `OAuth2PasswordBearer`?</summary>

**Answer**: `tokenUrl` serves two distinct purposes:
1. It registers the token endpoint URL in the generated OpenAPI schema (`components.securitySchemes.OAuth2PasswordBearer.flows.password.tokenUrl`).
2. Swagger UI reads this value and directs the login credentials entered in its "Authorize" modal to this exact URL when requesting tokens.
</details>

<details>
<summary>Q3: Why does OAuth2 RFC 6749 mandate HTTP 400 Bad Request instead of HTTP 401 for invalid credentials at the token endpoint?</summary>

**Answer**: RFC 6749 Section 5.2 explicitly specifies that token request errors (such as invalid credentials, invalid grant, or unsupported grant type) must be returned with an HTTP 400 Bad Request status code accompanied by an error JSON payload (`{"error": "invalid_grant"}`). HTTP 401 is reserved for requests where client authentication itself failed (e.g. bad `client_secret`).
</details>

<details>
<summary>Q4: Why has the OAuth Working Group deprecated the Resource Owner Password Credentials (ROPC) grant?</summary>

**Answer**: ROPC forces the client application to directly handle, process, and potentially store the user's plaintext password. This eliminates the primary benefit of OAuth (delegation without sharing credentials), trains users to enter passwords into arbitrary software, and makes implementing Multi-Factor Authentication (MFA) cumbersome.
</details>

<details>
<summary>Q5: How do scopes work with `OAuth2PasswordRequestForm`?</summary>

**Answer**: A client can request specific scopes by passing a space-separated string in the `scope` form parameter (e.g., `scope="read write"`). FastAPI's `OAuth2PasswordRequestForm` automatically splits this string into a list of strings accessible via `form_data.scopes`.
</details>

<details>
<summary>Q6: What happens if you specify a relative path for `tokenUrl` versus an absolute path?</summary>

**Answer**: A relative path without a leading slash (e.g., `token`) resolves relative to the current browser URL in Swagger UI (`/docs/token`), which typically results in 404s. A root-relative path (e.g., `/token` or `/auth/token`) correctly resolves from the domain root regardless of where documentation is hosted.
</details>

<details>
<summary>Q7: How does Swagger UI authenticate subsequent requests after logging in via the "Authorize" modal?</summary>

**Answer**: Swagger UI captures the `access_token` from the JSON response and automatically attaches it to the `Authorization: Bearer <access_token>` header on all subsequent "Try it out" HTTP requests executed within the documentation interface.
</details>

<details>
<summary>Q8: Can `OAuth2PasswordBearer` be configured to not raise an automatic 401 error if credentials are missing?</summary>

**Answer**: Yes, by initializing `OAuth2PasswordBearer(tokenUrl="...", auto_error=False)`. When `auto_error=False`, the dependency returns `None` instead of raising an `HTTPException(401)` when the `Authorization` header is missing, enabling optional authentication or fallback schemes.
</details>

---

## 13. Practical Exercises & Solutions

<details>
<summary>Exercise 1 (Easy): Create a basic OAuth2 token endpoint returning bearer tokens</summary>

**Problem**: Build a route `/oauth/token` using `OAuth2PasswordRequestForm` that validates username `dev` and password `fastapi` and returns a bearer token.

**Solution**:
```python
from typing import Annotated
from fastapi import Depends, FastAPI, HTTPException, status
from fastapi.security import OAuth2PasswordRequestForm

app = FastAPI()

@app.post("/oauth/token")
async def generate_token(form: Annotated[OAuth2PasswordRequestForm, Depends()]):
    if form.username != "dev" or form.password != "fastapi":
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail="invalid_grant")
    return {"access_token": "token_12345", "token_type": "bearer"}
```
</details>

<details>
<summary>Exercise 2 (Easy): Protect a route using the token from Exercise 1</summary>

**Problem**: Create an endpoint `/secure/data` protected by `OAuth2PasswordBearer(tokenUrl="/oauth/token")`.

**Solution**:
```python
from typing import Annotated
from fastapi import Depends, HTTPException, status
from fastapi.security import OAuth2PasswordBearer

oauth2_scheme = OAuth2PasswordBearer(tokenUrl="/oauth/token")

@app.get("/secure/data")
async def get_secure_data(token: Annotated[str, Depends(oauth2_scheme)]):
    if token != "token_12345":
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="Invalid token")
    return {"status": "authorized"}
```
</details>

<details>
<summary>Exercise 3 (Medium): Implement Scope Validation from Request Form</summary>

**Problem**: Extend the token endpoint to ensure requested scopes are valid (`read`, `write`), returning only granted scopes in the response.

**Solution**:
```python
from typing import Annotated
from fastapi import Depends, FastAPI, HTTPException, status
from fastapi.security import OAuth2PasswordRequestForm

app = FastAPI()
VALID_SCOPES = {"read", "write"}

@app.post("/token")
async def login_with_scopes(form: Annotated[OAuth2PasswordRequestForm, Depends()]):
    if form.username != "user" or form.password != "pass":
        raise HTTPException(status_code=400, detail="invalid_grant")
    
    granted = [s for s in form.scopes if s in VALID_SCOPES]
    return {
        "access_token": "valid_token",
        "token_type": "bearer",
        "scope": " ".join(granted)
    }
```
</details>

<details>
<summary>Exercise 4 (Medium): Implement Optional Authentication using `auto_error=False`</summary>

**Problem**: Create a route `/articles` that displays premium articles if a valid token is provided, or public articles if no token is passed.

**Solution**:
```python
from typing import Annotated
from fastapi import Depends, FastAPI
from fastapi.security import OAuth2PasswordBearer

app = FastAPI()
optional_auth = OAuth2PasswordBearer(tokenUrl="/token", auto_error=False)

@app.get("/articles")
async def list_articles(token: Annotated[str | None, Depends(optional_auth)]):
    if token == "premium_token":
        return {"tier": "premium", "articles": ["Pro Article 1", "Pro Article 2"]}
    return {"tier": "public", "articles": ["Free Article 1"]}
```
</details>

<details>
<summary>Exercise 5 (Hard): Implement Custom Password Request Form supporting TOTP MFA</summary>

**Problem**: Build a custom form class subclassing Starlette form handling that accepts `username`, `password`, and an optional 6-digit `mfa_code`.

**Solution**:
```python
from fastapi import Form, HTTPException, status
from pydantic import BaseModel

class OAuth2PasswordAndMfaForm:
    def __init__(
        self,
        username: str = Form(...),
        password: str = Form(...),
        mfa_code: str | None = Form(default=None),
        grant_type: str = Form(default="password"),
        scope: str = Form(default=""),
    ):
        self.username = username
        self.password = password
        self.mfa_code = mfa_code
        self.grant_type = grant_type
        self.scopes = scope.split() if scope else []

@app.post("/auth/mfa-token")
async def login_mfa(form_data: Annotated[OAuth2PasswordAndMfaForm, Depends()]):
    if form_data.username == "admin" and form_data.password == "adminpass":
        if form_data.mfa_code != "123456":
            raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail="Invalid MFA Code")
        return {"access_token": "admin_mfa_granted_token", "token_type": "bearer"}
    raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail="invalid_grant")
```
</details>

<details>
<summary>Exercise 6 (Hard): Construct an OpenID Connect (OIDC) Metadata Integration</summary>

**Problem**: Implement an endpoint that mimics standard OpenID Connect discovery at `/.well-known/openid-configuration` pointing to your OAuth2 endpoints.

**Solution**:
```python
from fastapi import FastAPI, Request

app = FastAPI()

@app.get("/.well-known/openid-configuration")
async def openid_configuration(request: Request):
    base_url = str(request.base_url).rstrip("/")
    return {
        "issuer": base_url,
        "token_endpoint": f"{base_url}/auth/token",
        "authorization_endpoint": f"{base_url}/auth/authorize",
        "userinfo_endpoint": f"{base_url}/auth/userinfo",
        "jwks_uri": f"{base_url}/auth/.well-known/jwks.json",
        "response_types_supported": ["code", "token"],
        "subject_types_supported": ["public"],
        "id_token_signing_alg_values_supported": ["RS256", "HS256"],
        "scopes_supported": ["openid", "profile", "email"],
    }
```
</details>
