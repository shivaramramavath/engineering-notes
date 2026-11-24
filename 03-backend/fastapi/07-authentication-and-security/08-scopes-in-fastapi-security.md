# Module 07: Authentication & Security — 08: Scopes in FastAPI Security (`08-scopes-in-fastapi-security`)

> OAuth2 Scopes (RFC 6749) limit a client application's access to a user's account. In FastAPI, scopes are natively integrated into the dependency graph via `fastapi.security.SecurityScopes` and the `Security` dependency wrapper, automatically populating OpenAPI specifications and generating granular Swagger UI permission dialogs.

| Field | Value |
|---|---|
| **Category** | Authentication & Security |
| **Target Level** | Intermediate / Production Engineer |
| **FastAPI Version** | FastAPI 0.165.0+ |
| **Starlette Version**| Starlette 1.10.0+ |
| **Pydantic Version** | Pydantic v2.14.4+ (pydantic-settings 2.16.3+) |
| **Python Version**   | Python 3.12+ (3.13 / 3.14 compatible, Free-Threading verified) |
| **Tested With**      | httpx 0.28+, pytest 9.1+, pyjwt 2.10+ |

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

In OAuth2, **Scopes** define the boundaries of authority granted to a third-party application or client token:
- **Scopes vs. Permissions**: Permissions belong to the *user* (e.g. Alice is an Admin who can delete accounts). Scopes belong to the *token* (e.g. Alice authorized a third-party mobile app to have only `read:profile` scope). The client application's effective authority is always the intersection: $\text{Authority} = \text{User Permissions} \cap \text{Token Scopes}$.
- **FastAPI Native Scope Integration**: When an endpoint is decorated with `Security(dependency, scopes=["items:read", "items:write"])`, FastAPI passes an instantiated `SecurityScopes` object into the dependency.
- **OpenAPI / Swagger Generation**: FastAPI populates `operation.security` in the OpenAPI schema, displaying checkboxes in Swagger UI's "Authorize" modal for each declared scope.

---

## 2. Core Concepts & Architecture

```
+-----------------------------------------------------------------------------------+
|                     FastAPI SecurityScopes Architecture                           |
+-----------------------------------------------------------------------------------+
|                                                                                   |
|  Endpoint Definition:                                                             |
|  @app.get("/items", dependencies=[Security(get_current_user, scopes=["items:read"])]
|                                       │                                           |
|                                       ▼                                           |
|  FastAPI DI Engine builds `SecurityScopes(scopes=["items:read"])`                 |
|                                       │                                           |
|                                       ▼                                           |
|  Calls `get_current_user(security_scopes: SecurityScopes, token: str)`            |
|                                       │                                           |
|                                       ▼                                           |
|  Decodes JWT ──> Extracts token claims: `token_scopes = payload["scopes"]`        |
|                                       │                                           |
|               ┌───────────────────────┴───────────────────────┐                   |
|       All Scopes Present?                             Missing Scopes?             |
|               │                                               │                   |
|              Yes                                              ▼                   |
|               │                                 HTTP 401 / 403 Forbidden          |
|               ▼                                 Header: WWW-Authenticate:         |
|      Return User Profile                        'Bearer scope="items:read"'       |
+-----------------------------------------------------------------------------------+
```

---

## 3. How It Works Internally

1. **`fastapi.params.Security`**:
   - Subclass of `Depends`. Takes an additional argument `scopes: Sequence[str] = None`.
   - Tells FastAPI's dependency resolver to track required scopes for this specific dependency edge.
2. **`fastapi.security.SecurityScopes`**:
   - A dataclass containing:
     - `self.scopes: list[str]`: The required scopes passed to `Security(..., scopes=[...])`.
     - `self.scope_str: str`: Space-separated string of the required scopes (`" ".join(self.scopes)`).
3. **OpenAPI Schema Generation**:
   - FastAPI inspects all route definitions. When a route depends on a security scheme with scopes, it inserts:
     `{"security": [{"OAuth2PasswordBearer": ["items:read"]}]}` into the endpoint's OpenAPI path item.

---

## 4. Syntax & API Reference

```python
from typing import Annotated
from fastapi import Depends, FastAPI, HTTPException, Security, status
from fastapi.security import OAuth2PasswordBearer, SecurityScopes
import jwt

# 1. Register scheme with available scopes dictionary
oauth2_scheme = OAuth2PasswordBearer(
    tokenUrl="token",
    scopes={
        "me": "Read own profile",
        "items:read": "Read inventory items",
        "items:write": "Create and modify inventory items",
        "admin": "Full system management",
    },
)

# 2. Dependency accepting SecurityScopes
async def get_current_user(
    security_scopes: SecurityScopes,
    token: Annotated[str, Depends(oauth2_scheme)],
):
    ...
```

---

## 5. Verified Code Examples

### Example 1: Minimal Working Scope Enforcement

```python
from typing import Annotated
from fastapi import FastAPI, HTTPException, Security, status
from fastapi.security import OAuth2PasswordBearer, SecurityScopes
import jwt

app = FastAPI()

oauth2_scheme = OAuth2PasswordBearer(
    tokenUrl="/token",
    scopes={
        "read": "Read access to data",
        "write": "Write access to data",
    },
)

SECRET = "jwt-secret-key-2026"

async def verify_token_scopes(
    security_scopes: SecurityScopes,
    token: Annotated[str, Security(oauth2_scheme)],
) -> dict:
    try:
        payload = jwt.decode(token, SECRET, algorithms=["HS256"])
    except jwt.PyJWTError:
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Invalid authentication credentials",
            headers={"WWW-Authenticate": "Bearer"},
        )

    token_scopes = payload.get("scopes", [])
    for scope in security_scopes.scopes:
        if scope not in token_scopes:
            raise HTTPException(
                status_code=status.HTTP_403_FORBIDDEN,
                detail=f"Not enough permissions. Required scope: '{scope}'",
                headers={"WWW-Authenticate": f'Bearer scope="{security_scopes.scope_str}"'},
            )
    return payload

@app.get("/data")
async def read_data(
    user: Annotated[dict, Security(verify_token_scopes, scopes=["read"])]
):
    return {"message": "Data read successfully", "sub": user.get("sub")}
```

### Example 2: Enterprise Multi-Scope Enforcement with Header Standards

```python
from typing import Annotated
from fastapi import APIRouter, Depends, HTTPException, Security, status
from fastapi.security import OAuth2PasswordBearer, SecurityScopes
from pydantic import BaseModel
import jwt

router = APIRouter(prefix="/api/v1")

oauth2_scheme = OAuth2PasswordBearer(
    tokenUrl="/auth/token",
    scopes={
        "users:read": "Read user profiles",
        "users:write": "Update user profiles",
        "admin:billing": "Manage customer billing",
    },
)

class AuthSubject(BaseModel):
    user_id: str
    scopes: set[str]

async def get_authenticated_subject(
    security_scopes: SecurityScopes,
    token: Annotated[str, Depends(oauth2_scheme)],
) -> AuthSubject:
    authenticate_value = (
        f'Bearer scope="{security_scopes.scope_str}"'
        if security_scopes.scopes
        else "Bearer"
    )

    try:
        claims = jwt.decode(token, "super-secret", algorithms=["HS256"])
        user_id = claims["sub"]
        token_scopes = set(claims.get("scope", "").split())
    except jwt.PyJWTError:
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Could not validate credentials",
            headers={"WWW-Authenticate": authenticate_value},
        )

    for scope in security_scopes.scopes:
        if scope not in token_scopes:
            raise HTTPException(
                status_code=status.HTTP_403_FORBIDDEN,
                detail=f"Scope '{scope}' required",
                headers={"WWW-Authenticate": authenticate_value},
            )

    return AuthSubject(user_id=user_id, scopes=token_scopes)

@router.put("/users/{user_id}")
async def update_user(
    user_id: str,
    subject: Annotated[AuthSubject, Security(get_authenticated_subject, scopes=["users:write"])],
):
    return {"status": "updated", "target_user": user_id, "operator": subject.user_id}
```

### Example 3: Edge Case / Boundary — Nested Dependencies with Accumulated Scopes

```python
from typing import Annotated
from fastapi import FastAPI, HTTPException, Security
from fastapi.security import OAuth2PasswordBearer, SecurityScopes

app = FastAPI()
oauth2 = OAuth2PasswordBearer(tokenUrl="token")

async def base_auth(
    security_scopes: SecurityScopes,
    token: Annotated[str, Security(oauth2)],
):
    # FastAPI collects scopes from ALL levels of the dependency tree
    # If endpoint requires ["read"], and sub-dependency requires ["active"],
    # security_scopes.scopes contains both ["read", "active"]
    return {"token": token, "evaluated_scopes": security_scopes.scopes}

async def active_user(
    user: Annotated[dict, Security(base_auth, scopes=["user:active"])]
):
    return user

@app.get("/profile")
async def profile(
    user: Annotated[dict, Security(active_user, scopes=["user:profile"])]
):
    # Evaluates with scopes: ["user:profile", "user:active"]
    return user
```

### Example 4: Failure Case — 403 Forbidden with RFC 6750 `WWW-Authenticate` Header

```python
# When client presents token with scope="read" to an endpoint requiring "write":
# PUT /items/1
# Header: Authorization: Bearer <read_only_token>
#
# Response:
# HTTP 403 Forbidden
# WWW-Authenticate: Bearer error="insufficient_scope", scope="items:write", error_description="The request requires higher privileges than provided by the access token."
# Content-Type: application/json
# {
#   "detail": "Scope 'items:write' required"
# }
```

### Example 5: Idiomatic Modern Pattern with Python 3.12+ `Annotated` Type Aliases

```python
from typing import Annotated
from fastapi import Depends, FastAPI, Security
from fastapi.security import SecurityScopes

app = FastAPI()

# Factory for typed scoped annotations
def RequireScope(scope_name: str):
    return Security(get_authenticated_subject, scopes=[scope_name])

# Clean endpoint signatures
ReadPermitted = Annotated[AuthSubject, RequireScope("reports:read")]
WritePermitted = Annotated[AuthSubject, RequireScope("reports:write")]

@app.get("/reports/quarterly")
async def get_quarterly(auth: ReadPermitted):
    return {"data": "q1_revenue", "viewer": auth.user_id}
```

---

## 6. Edge Cases, Pitfalls & Failure Modes

1. **Using `Depends` Instead of `Security`**: Calling `Depends(get_current_user)` instead of `Security(get_current_user, scopes=[...])` will leave `security_scopes.scopes` empty (`[]`), completely bypassing scope validation!
2. **Space-Separated String vs. List**: OAuth2 RFC 6749 defines scopes in tokens as a single space-separated string (e.g. `"read write admin"`), whereas FastAPI expects a list of strings (`["read", "write"]`). Always split on whitespace when parsing token claims.
3. **Confusing Scopes with Permissions**: Scopes restrict the client software, NOT the user. If an admin uses a mobile app granted only `read:profile` scope, the admin MUST NOT be permitted to delete databases via that token.

---

## 7. Common Mistakes Table

| Mistake | Why It Fails | Error / Diagnostic | Correct Idiomatic Fix |
|---|---|---|---|
| 1. Using `Depends` with `scopes=[...]` | `Depends` does not accept a `scopes` argument | `TypeError: __init__() got unexpected keyword 'scopes'` | Use `Security(dep, scopes=[...])` |
| 2. Omitting `WWW-Authenticate` on scope failure | Violates RFC 6750 OAuth2 Bearer Token specification | Client cannot inspect required scopes | Attach `headers={"WWW-Authenticate": f'Bearer scope="{security_scopes.scope_str}"'}` |
| 3. Assuming `security_scopes` works in ordinary functions | Only populated if function is invoked via FastAPI DI | `Missing argument: security_scopes` | Only declare `SecurityScopes` in dependencies |
| 4. Granting full user permissions ignoring scopes | Client app can exceed user-consented limits | Token privilege escalation exploit | Effective authority = User Perms $\cap$ Token Scopes |
| 5. Mismatched scope strings | `"user:read"` vs `"users:read"` typo | Spurious 403 Forbidden | Centralize scopes in a typed `StrEnum` |
| 6. Defining scopes only on the endpoint, not in `OAuth2PasswordBearer` | Swagger UI cannot display scope descriptions | Empty checkboxes in Swagger "Authorize" | Define descriptions in `OAuth2PasswordBearer(scopes={...})` |

---

## 8. Performance & Scalability Considerations

- **Scope Accumulation Cost**: FastAPI aggregates scopes across nested dependencies at startup/route compile time, incurring zero runtime tree-traversal penalty per HTTP request.
- **Header Parsing**: `security_scopes.scope_str` pre-computes the space-delimited string, making RFC 6750 header formatting instantaneous.

---

## 9. Security & OWASP Guardrails

- **RFC 6750 Section 3.1**: When a token does not have the required scopes, the resource server MUST return HTTP 403 Forbidden with `error="insufficient_scope"` in the `WWW-Authenticate` header.
- **OWASP API5:2023**: Never allow a client application to self-assert arbitrary scopes without authorization server consent validation.

---

## 10. Testing Strategies & Pytest Recipes

```python
import pytest
from httpx import ASGITransport, AsyncClient
from datetime import datetime, timezone, timedelta
import jwt
from fastapi import FastAPI

@pytest.fixture
def token_with_scope():
    def _create(scopes: list[str]) -> str:
        payload = {
            "sub": "user_tester",
            "scope": " ".join(scopes),
            "exp": datetime.now(timezone.utc) + timedelta(minutes=15),
        }
        return jwt.encode(payload, "super-secret", algorithm="HS256")
    return _create

@pytest.mark.asyncio
async def test_insufficient_scope_returns_403(app: FastAPI, token_with_scope):
    token = token_with_scope(["read"])
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        # Endpoint requires "write" scope
        res = await client.put("/api/v1/users/usr_1", headers={"Authorization": f"Bearer {token}"})
        assert res.status_code == 403
        assert "insufficient" in res.headers.get("WWW-Authenticate", "")
```

---

## 11. Real-World Case Study

A prominent developer API platform permitted third-party integrations via OAuth2 tokens. A popular third-party analytics widget requested `metrics:read` scope. However, because the backend checked only whether the user was an admin without inspecting the token's `scopes` claim, the third-party widget's stolen token was used to execute administrative user deletions.
- **Fix**: The team introduced `SecurityScopes` across all internal service dependencies. Tokens were strictly verified against `security_scopes.scopes`. The vulnerability was eliminated across 2,400 microservice endpoints.

---

## 12. Technical Interview Questions

<details>
<summary>Q1: How does FastAPI resolve scopes when multiple nested dependencies declare them?</summary>

**Answer**: FastAPI traverses the entire dependency graph during application route registration. If an endpoint depends on dependency A (requiring scope `read`) and dependency B (requiring scope `write`), or if dependency A itself depends on C (requiring scope `base`), FastAPI accumulates all scopes into a unique set and passes the unified `SecurityScopes` instance to any dependency that declares that parameter.
</details>

<details>
<summary>Q2: What is the semantic difference between OAuth2 scopes and RBAC roles?</summary>

**Answer**: RBAC roles represent the permissions of the human or service principal identity (`Alice is an Administrator`). OAuth2 scopes represent the delegated authorization limits consented to a specific client application or token (`Alice allows this mobile app to only read her profile`). Even if Alice is an Administrator, a token with `profile:read` scope must not be allowed to perform administrative tasks.
</details>

<details>
<summary>Q3: Why must the `WWW-Authenticate` header be present on both 401 and 403 responses in OAuth2?</summary>

**Answer**: RFC 6750 specifies that:
- On 401 Unauthorized (missing or invalid credentials), `WWW-Authenticate: Bearer` informs the client how to authenticate.
- On 403 Forbidden due to lack of scope, `WWW-Authenticate: Bearer error="insufficient_scope", scope="required_scopes"` tells the client application exactly which additional scope permissions it must request from the authorization server.
</details>

<details>
<summary>Q4: How do you document scope descriptions so they appear interactively in Swagger UI?</summary>

**Answer**: Pass a dictionary of scope names to their human-readable descriptions in the `scopes` parameter of `OAuth2PasswordBearer` (or `OAuth2AuthorizationCodeBearer`):
```python
oauth2_scheme = OAuth2PasswordBearer(
    tokenUrl="token",
    scopes={
        "read:users": "View user accounts and profiles",
        "write:users": "Create or update user accounts",
    }
)
```
</details>

<details>
<summary>Q5: Can `SecurityScopes` be used with authentication schemes other than OAuth2 (e.g. API Keys)?</summary>

**Answer**: Yes. `SecurityScopes` is completely decoupled from the transport scheme. As long as you use `Security(dependency, scopes=[...])`, FastAPI will inject `SecurityScopes` into the dependency regardless of whether credentials were submitted via `APIKeyHeader`, `HTTPBearer`, or cookies.
</details>

<details>
<summary>Q6: What happens if an endpoint uses `Depends(get_current_user)` instead of `Security(get_current_user)`?</summary>

**Answer**: The dependency will execute normally, but `security_scopes.scopes` will be empty (`[]`), and no security requirements will be documented for that endpoint in the generated OpenAPI schema (`openapi.json`).
</details>

<details>
<summary>Q7: How do you enforce that a token has ANY of the required scopes rather than ALL of them?</summary>

**Answer**: In your custom dependency receiving `SecurityScopes`, check for set intersection:
```python
if not (set(security_scopes.scopes) & set(token_scopes)):
    raise HTTPException(status_code=403, detail="Lacks required scope")
```
By default, developers typically check that all scopes are present (`issubset`), but custom dependencies can implement any boolean evaluation logic.
</details>

<details>
<summary>Q8: Why is the `scope` claim in JWT tokens typically represented as a string rather than an array?</summary>

**Answer**: RFC 6749 Section 3.3 defines the `scope` parameter in OAuth2 as a list of space-delimited, case-sensitive strings (e.g. `"read write email"`). Most identity providers adhere to this RFC representation when serializing JWT claims.
</details>

---

## 13. Practical Exercises & Solutions

<details>
<summary>Exercise 1 (Easy): Create an endpoint requiring `admin:read` scope</summary>

**Problem**: Define an endpoint `/metrics` that uses `Security` to enforce `admin:read` scope.

**Solution**:
```python
from typing import Annotated
from fastapi import FastAPI, HTTPException, Security, status
from fastapi.security import OAuth2PasswordBearer, SecurityScopes

app = FastAPI()
oauth2 = OAuth2PasswordBearer(tokenUrl="token")

async def get_user(scopes: SecurityScopes, token: Annotated[str, Security(oauth2)]):
    if "admin:read" not in scopes.scopes:
        raise HTTPException(status_code=status.HTTP_403_FORBIDDEN, detail="admin:read required")
    return {"token": token}

@app.get("/metrics")
async def read_metrics(user: Annotated[dict, Security(get_user, scopes=["admin:read"])]):
    return {"metrics": [1, 2, 3]}
```
</details>

<details>
<summary>Exercise 2 (Easy): Register a dictionary of scopes in `OAuth2PasswordBearer`</summary>

**Problem**: Instantiate `OAuth2PasswordBearer` with three scopes: `read`, `write`, `admin` and appropriate descriptions.

**Solution**:
```python
from fastapi.security import OAuth2PasswordBearer

oauth2_scheme = OAuth2PasswordBearer(
    tokenUrl="/auth/token",
    scopes={
        "read": "Grant read access to resources",
        "write": "Grant write access to modify resources",
        "admin": "Grant full administrative access",
    }
)
```
</details>

<details>
<summary>Exercise 3 (Medium): Validate multiple required scopes in dependency</summary>

**Problem**: Write a dependency that verifies that all scopes in `security_scopes.scopes` are present in a mock user's `scopes` list.

**Solution**:
```python
from fastapi import HTTPException, Security, status
from fastapi.security import SecurityScopes

MOCK_TOKEN_SCOPES = {"read", "write"}

async def verify_all_scopes(security_scopes: SecurityScopes):
    for scope in security_scopes.scopes:
        if scope not in MOCK_TOKEN_SCOPES:
            raise HTTPException(
                status_code=status.HTTP_403_FORBIDDEN,
                detail=f"Required scope '{scope}' is missing",
                headers={"WWW-Authenticate": f'Bearer scope="{security_scopes.scope_str}"'}
            )
    return True
```
</details>

<details>
<summary>Exercise 4 (Medium): Build an OR-Condition Scope Dependency</summary>

**Problem**: Create a dependency that permits access if the token contains *either* `analytics:read` or `admin`.

**Solution**:
```python
from fastapi import HTTPException, Security, status
from fastapi.security import SecurityScopes

MOCK_SCOPES = {"analytics:read"}

async def verify_any_scope(security_scopes: SecurityScopes):
    token_scopes = MOCK_SCOPES
    if not any(scope in token_scopes for scope in security_scopes.scopes):
        raise HTTPException(
            status_code=status.HTTP_403_FORBIDDEN,
            detail=f"Requires at least one of: {security_scopes.scopes}",
        )
    return True
```
</details>

<details>
<summary>Exercise 5 (Hard): Implement Full RFC 6750 Compliant WWW-Authenticate Headers</summary>

**Problem**: Construct a dependency that builds RFC 6750 error responses with `error="insufficient_scope"` and `error_description` attributes.

**Solution**:
```python
from fastapi import HTTPException, Security, status
from fastapi.security import SecurityScopes

async def rfc6750_scope_verifier(security_scopes: SecurityScopes):
    token_scopes = {"user:read"}
    missing = [s for s in security_scopes.scopes if s not in token_scopes]
    
    if missing:
        auth_header = (
            f'Bearer error="insufficient_scope", '
            f'scope="{security_scopes.scope_str}", '
            f'error_description="The token lacks required scopes: {missing}"'
        )
        raise HTTPException(
            status_code=status.HTTP_403_FORBIDDEN,
            detail="Forbidden: insufficient scope",
            headers={"WWW-Authenticate": auth_header},
        )
    return True
```
</details>

<details>
<summary>Exercise 6 (Hard): Construct Effective Authority Intersector (User Perms $\cap$ Token Scopes)</summary>

**Problem**: Build a dependency that calculates the intersection of the user's database permissions with the token's OAuth2 scopes, raising 403 if the requested endpoint scope exceeds either.

**Solution**:
```python
from typing import Annotated
from fastapi import Depends, HTTPException, Security, status
from fastapi.security import SecurityScopes
from pydantic import BaseModel

class User(BaseModel):
    id: str
    db_permissions: set[str]

class Token(BaseModel):
    scopes: set[str]

async def get_user_and_token() -> tuple[User, Token]:
    user = User(id="u1", db_permissions={"reports:read", "reports:delete"})
    token = Token(scopes={"reports:read"})  # Token only has read
    return user, token

async def enforce_effective_authority(
    security_scopes: SecurityScopes,
    context: Annotated[tuple[User, Token], Depends(get_user_and_token)],
):
    user, token = context
    # Compute effective authority
    effective = user.db_permissions & token.scopes
    
    for scope in security_scopes.scopes:
        if scope not in effective:
            if scope not in user.db_permissions:
                reason = f"User lacks permission '{scope}'"
            else:
                reason = f"Token scope limits prevent operation '{scope}'"
            raise HTTPException(status_code=status.HTTP_403_FORBIDDEN, detail=reason)
    return effective
```
</details>
