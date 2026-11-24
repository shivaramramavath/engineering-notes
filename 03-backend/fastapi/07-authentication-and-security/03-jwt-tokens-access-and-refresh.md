# Module 07: Authentication & Security — 03: JWT Tokens: Access and Refresh (`03-jwt-tokens-access-and-refresh`)

> JSON Web Tokens (RFC 7519) provide cryptographically signed, stateless identity claims for distributed web systems. In production FastAPI architectures, security is achieved by pairing short-lived access tokens with long-lived refresh tokens enforced by automated token rotation and reuse detection.

| Field | Value |
|---|---|
| **Category** | Authentication & Security |
| **Target Level** | Intermediate / Production Engineer |
| **FastAPI Version** | FastAPI 0.165.0+ |
| **Starlette Version**| Starlette 1.10.0+ |
| **Pydantic Version** | Pydantic v2.14.4+ (pydantic-settings 2.16.3+) |
| **Python Version**   | Python 3.12+ (3.13 / 3.14 compatible, Free-Threading verified) |
| **Tested With**      | PyJWT 2.10+, cryptography 44.0+, httpx 0.28+, pytest 9.1+ |

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

A JSON Web Token (JWT) is an open, industry-standard (RFC 7519) compact URL-safe representation of claims transferred between two parties.
- **Stateless Verification**: Microservices verify signatures locally using shared secrets (HMAC) or public cryptographic keys (RSA, ECDSA), avoiding database hits on every API call.
- **The Stateless Revocation Dilemma**: Because a JWT is self-contained and valid until its expiration (`exp`), it cannot be trivially revoked without introducing state.
- **The Dual-Token Solution**:
  1. **Access Token**: Short TTL (5 to 15 minutes), passed in HTTP `Authorization: Bearer <token>` header, held in memory.
  2. **Refresh Token**: Long TTL (7 to 30 days), stored in an `HttpOnly`, `Secure`, `SameSite=Strict` cookie or secure storage, tracked in a revocation registry (Redis/SQL), and rotated upon every use.

---

## 2. Core Concepts & Architecture

```
+-----------------------------------------------------------------------------------+
|                        JWT Structure: Base64URL Encoded                           |
+-----------------------------------------------------------------------------------+
|  Header (Algorithm & Key ID)  |  Payload (Registered & Custom Claims) | Signature |
|  {"alg":"HS256","typ":"JWT"}  |  {"sub":"usr_1","exp":1711900000}     | HMAC-SHA  |
+-----------------------------------------------------------------------------------+

Refresh Token Rotation (RTR) Flow:

Client App (React/Mobile)                 FastAPI Auth Server            Redis / DB Store
     │                                            │                             │
     │ 1. POST /auth/refresh (Old Token R1)       │                             │
     ├───────────────────────────────────────────>│                             │
     │                                            │ 2. Check R1 in Store        │
     │                                            ├────────────────────────────>│
     │                                            │ <── Status: Active ─────────┤
     │                                            │                             │
     │                                            │ 3. Invalidate R1, Issue R2  │
     │                                            ├────────────────────────────>│
     │                                            │                             │
     │ 4. Return New (Access Token A2, Refresh R2)│                             │
     │<───────────────────────────────────────────┤                             │
     │                                            │                             │
     │ *ATTACK SCENARIO: Stolen R1 Replayed*      │                             │
     │ POST /auth/refresh (Replayed Token R1)     │                             │
     ├───────────────────────────────────────────>│                             │
     │                                            │ 5. Check R1 in Store        │
     │                                            ├────────────────────────────>│
     │                                            │ <── ALREADY REVOKED! ───────┤
     │                                            │                             │
     │                                            │ 6. REVOKE ENTIRE FAMILY!    │
     │                                            ├────────────────────────────>│
     │ 7. HTTP 401 Unauthorized (Force Logout)    │                             │
     │<───────────────────────────────────────────┤                             │
```

---

## 3. How It Works Internally

1. **Standard Claims Validation**:
   - `sub` (Subject): The principal identifier (e.g. user ID).
   - `exp` (Expiration Time): Unix epoch timestamp. Must be verified with leeway (clock skew tolerance).
   - `nbf` (Not Before): Token cannot be accepted before this timestamp.
   - `iat` (Issued At): When the token was generated.
   - `jti` (JWT ID): Unique UUID per token, vital for tracking refresh token families and blacklists.
2. **PyJWT Cryptographic Operations**: PyJWT uses Python `cryptography` primitives under the hood (compiled C/Rust bindings), ensuring constant-time signature verification.

---

## 4. Syntax & API Reference

```python
import jwt
from datetime import datetime, timezone, timedelta

SECRET_KEY = "enterprise-production-signing-secret"
ALGORITHM = "HS256"

# Encoding a token
payload = {
    "sub": "user_uuid_12345",
    "exp": datetime.now(timezone.utc) + timedelta(minutes=15),
    "iat": datetime.now(timezone.utc),
    "jti": "token_unique_id",
    "type": "access",
}
token: str = jwt.encode(payload, SECRET_KEY, algorithm=ALGORITHM)

# Decoding and validating
try:
    decoded = jwt.decode(
        token,
        SECRET_KEY,
        algorithms=[ALGORITHM],
        leeway=timedelta(seconds=10),
        options={"require": ["exp", "sub", "jti"]}
    )
except jwt.ExpiredSignatureError:
    ... # Token expired
except jwt.InvalidTokenError:
    ... # Signature mismatch or malformed
```

---

## 5. Verified Code Examples

### Example 1: Minimal Token Generator & Extractor

```python
from datetime import datetime, timedelta, timezone
import uuid
import jwt
from fastapi import FastAPI, HTTPException, Security, status
from fastapi.security import HTTPAuthorizationCredentials, HTTPBearer
from pydantic import BaseModel

app = FastAPI()
security = HTTPBearer()

SECRET_KEY = "dev-secret-key-change-in-production-env"
ALGORITHM = "HS256"

class TokenPayload(BaseModel):
    sub: str
    token_type: str
    jti: str

def create_access_token(user_id: str, expires_delta: timedelta = timedelta(minutes=15)) -> str:
    now = datetime.now(timezone.utc)
    payload = {
        "sub": user_id,
        "token_type": "access",
        "jti": str(uuid.uuid4()),
        "iat": now,
        "exp": now + expires_delta,
    }
    return jwt.encode(payload, SECRET_KEY, algorithm=ALGORITHM)

async def get_current_user(creds: HTTPAuthorizationCredentials = Security(security)) -> str:
    try:
        decoded = jwt.decode(
            creds.credentials, SECRET_KEY, algorithms=[ALGORITHM], options={"require": ["exp", "sub"]}
        )
        if decoded.get("token_type") != "access":
            raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="Invalid token type")
        return decoded["sub"]
    except jwt.ExpiredSignatureError:
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="Token has expired")
    except jwt.PyJWTError:
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="Could not validate credentials")
```

### Example 2: Production Dual-Token Lifecycle (Access + Refresh Rotation)

```python
from datetime import datetime, timedelta, timezone
import uuid
from typing import Annotated
from fastapi import APIRouter, Cookie, Depends, HTTPException, Response, status
from pydantic import BaseModel
import jwt

router = APIRouter(prefix="/auth")
JWT_SECRET = "production-jwt-signing-secret"
ALGORITHM = "HS256"

# In-Memory token store (Use Redis in real clusters)
# token_family -> { "current_jti": "...", "user_id": "...", "revoked": False }
REFRESH_FAMILIES: dict[str, dict] = {}

class TokenResponse(BaseModel):
    access_token: str
    token_type: str = "bearer"
    expires_in: int = 900

def issue_token_pair(user_id: str, family_id: str | None = None) -> tuple[str, str, str]:
    family = family_id or str(uuid.uuid4())
    access_jti = str(uuid.uuid4())
    refresh_jti = str(uuid.uuid4())
    now = datetime.now(timezone.utc)

    access_payload = {
        "sub": user_id,
        "jti": access_jti,
        "type": "access",
        "iat": now,
        "exp": now + timedelta(minutes=15),
    }
    refresh_payload = {
        "sub": user_id,
        "jti": refresh_jti,
        "family": family,
        "type": "refresh",
        "iat": now,
        "exp": now + timedelta(days=7),
    }

    access_token = jwt.encode(access_payload, JWT_SECRET, algorithm=ALGORITHM)
    refresh_token = jwt.encode(refresh_payload, JWT_SECRET, algorithm=ALGORITHM)

    # Register in active family store
    REFRESH_FAMILIES[family] = {
        "current_jti": refresh_jti,
        "user_id": user_id,
        "revoked": False,
    }

    return access_token, refresh_token, family

@router.post("/refresh", response_model=TokenResponse)
async def refresh_tokens(
    response: Response,
    refresh_token: Annotated[str | None, Cookie(alias="refresh_token")] = None,
):
    if not refresh_token:
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="Missing refresh token")

    try:
        payload = jwt.decode(refresh_token, JWT_SECRET, algorithms=[ALGORITHM])
        if payload.get("type") != "refresh":
            raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="Invalid token type")
        
        family_id = payload["family"]
        presented_jti = payload["jti"]
        user_id = payload["sub"]
    except jwt.PyJWTError:
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="Invalid or expired token")

    # Threat Detection: Check Token Reuse
    family_record = REFRESH_FAMILIES.get(family_id)
    if not family_record or family_record["revoked"]:
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="Token family revoked")

    if family_record["current_jti"] != presented_jti:
        # REUSE DETECTED: Someone presented an already-exchanged token! Invalidate family!
        family_record["revoked"] = True
        response.delete_cookie("refresh_token")
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Token reuse detected. All sessions terminated for security.",
        )

    # Legitimate refresh: issue new pair
    new_access, new_refresh, _ = issue_token_pair(user_id, family_id=family_id)
    response.set_cookie(
        key="refresh_token",
        value=new_refresh,
        httponly=True,
        secure=True,
        samesite="strict",
        max_age=7 * 86400,
    )
    return TokenResponse(access_token=new_access)
```

### Example 3: Edge Case / Boundary — Clock Skew Handling with Leeway

```python
import jwt
from datetime import timedelta

def safe_verify_with_clock_skew(token: str, secret: str) -> dict:
    # 30-second leeway accounts for clock drift across cloud instances
    return jwt.decode(
        token,
        secret,
        algorithms=["HS256"],
        leeway=timedelta(seconds=30),
        options={
            "verify_exp": True,
            "verify_iat": True,
            "require": ["exp", "sub"],
        }
    )
```

### Example 4: Failure Case — Rejecting Algorithm None / Confusion Exploits

```python
import jwt
from fastapi import HTTPException, status

def verify_token_strict_algorithm(token: str, secret: str) -> dict:
    try:
        # Explicit whitelist prevents 'none' algorithm and asymmetric/symmetric key confusion
        return jwt.decode(
            token,
            secret,
            algorithms=["HS256"],  # Never accept 'none'
        )
    except jwt.InvalidAlgorithmError:
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Disallowed cryptographic signing algorithm",
        )
    except jwt.PyJWTError as e:
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail=str(e))
```

### Example 5: Idiomatic Modern Pattern with Python 3.12+ `Annotated` Dependency

```python
from typing import Annotated
from fastapi import Depends, FastAPI, HTTPException, Security, status
from fastapi.security import HTTPAuthorizationCredentials, HTTPBearer
from pydantic import BaseModel, Field
import jwt

app = FastAPI()
bearer_scheme = HTTPBearer(auto_error=True)

class AuthenticatedUser(BaseModel):
    user_id: str
    email: str
    roles: list[str] = Field(default_factory=list)

async def current_user(
    creds: Annotated[HTTPAuthorizationCredentials, Security(bearer_scheme)]
) -> AuthenticatedUser:
    try:
        claims = jwt.decode(
            creds.credentials,
            "cluster-secret",
            algorithms=["HS256"],
            options={"require": ["sub", "exp"]}
        )
        return AuthenticatedUser(
            user_id=claims["sub"],
            email=claims.get("email", ""),
            roles=claims.get("roles", []),
        )
    except jwt.PyJWTError:
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Invalid authentication token",
            headers={"WWW-Authenticate": "Bearer"},
        )

# Type alias for clean route signatures
CurrentUser = Annotated[AuthenticatedUser, Depends(current_user)]

@app.get("/users/me")
async def get_profile(user: CurrentUser):
    return {"user_id": user.user_id, "email": user.email}
```

---

## 6. Edge Cases, Pitfalls & Failure Modes

1. **Algorithm Confusion (CVE-2015-9235)**: If an application expects RS256 but allows the client to choose HMAC (HS256), an attacker can sign a token using the server's public key as an HMAC secret! Whitelist algorithms strictly.
2. **Clock Drift Between Microservices**: If server A issues a token with timestamp $T$, and server B's clock is 5 seconds behind, server B may reject the token with `ImmatureSignatureError` (`nbf`/`iat` in future). Always configure `leeway=10`.
3. **Bloated Payloads**: Storing large permission trees or user metadata in JWTs increases every HTTP request header size significantly, leading to header overflow (HTTP 431 Request Header Fields Too Large) at edge load balancers.

---

## 7. Common Mistakes Table

| Mistake | Why It Fails | Error / Diagnostic | Correct Idiomatic Fix |
|---|---|---|---|
| 1. Setting indefinite access token TTL | Stolen tokens cannot be invalidated | Unauthorized persistent access | Limit access token to 10-15 minutes |
| 2. Storing refresh tokens in `localStorage` | JavaScript can read `localStorage` during XSS | Complete account takeover via XSS | Store in `HttpOnly; Secure; SameSite=Strict` cookie |
| 3. Omission of `algorithms` list in `jwt.decode` | Vulnerable to algorithm downgrade attacks | Security vulnerability (CVE warnings) | Explicitly pass `algorithms=["HS256"]` |
| 4. Missing token type verification | Attacker passes refresh token to access endpoints | Unauthorized access to APIs | Check `payload.get("type") == "access"` |
| 5. Not implementing token reuse detection | Stolen refresh tokens can be used in parallel | Silent credential theft | Invalidate entire family when an old JTI is reused |
| 6. Storing sensitive data (PII/passwords) in JWT | JWT payloads are Base64 encoded, NOT encrypted | Plaintext data leakage | Only store non-sensitive IDs and scopes |

---

## 8. Performance & Scalability Considerations

- **Verification Throughput**: HMAC-SHA256 (`HS256`) achieves ~200,000 verifications/sec per core. RSA-2048 (`RS256`) achieves ~8,000 verifications/sec. For high-volume microservices where tokens are verified internally, consider symmetric HMAC or Ed25519 (`EdDSA`).
- **Memory Overhead**: Keep token payloads below 500 bytes to minimize network bandwidth across microservice hops.

---

## 9. Security & OWASP Guardrails

- **OWASP API2:2023**: Enforce cryptographic validation of claims, verify expiration strictly, and reject unsigned tokens.
- **RFC 8725 JWT Best Current Practices**:
  - Always validate `aud` (Audience) and `iss` (Issuer) in multi-service meshes.
  - Require the `typ` header parameter to distinguish access tokens from ID tokens.
  - Reject tokens with `none` algorithm.

---

## 10. Testing Strategies & Pytest Recipes

```python
import pytest
from httpx import ASGITransport, AsyncClient
from datetime import datetime, timedelta, timezone
import jwt
from fastapi import FastAPI

@pytest.fixture
def make_token():
    def _generator(sub: str, expired: bool = False) -> str:
        delta = timedelta(minutes=-5) if expired else timedelta(minutes=15)
        payload = {
            "sub": sub,
            "type": "access",
            "exp": datetime.now(timezone.utc) + delta,
        }
        return jwt.encode(payload, "test-secret", algorithm="HS256")
    return _generator

@pytest.mark.asyncio
async def test_expired_token_returns_401(app: FastAPI, make_token):
    token = make_token("usr_test", expired=True)
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        res = await client.get("/users/me", headers={"Authorization": f"Bearer {token}"})
        assert res.status_code == 401
        assert "expired" in res.json()["detail"].lower()
```

---

## 11. Real-World Case Study

A SaaS platform with 500,000 daily active users faced a security breach when an attacker gained access to an employee's browser cookies. Because the API used long-lived 30-day access tokens with no revocation mechanism, the attacker retained access for weeks undetected.
- **Architectural Redesign**:
  1. Reduced access token lifetime to 10 minutes.
  2. Implemented Refresh Token Rotation with JTI tracking in Redis.
  3. When an old token was presented by the legitimate user (after the attacker had already exchanged it), the system detected reuse, revoked all tokens for that user ID, logged an audit alert, and prompted for MFA re-authentication.

---

## 12. Technical Interview Questions

<details>
<summary>Q1: What is Refresh Token Rotation (RTR), and how does it prevent token theft?</summary>

**Answer**: RTR is a security mechanism where every time a refresh token is used to obtain a new access token, the auth server invalidates the old refresh token and issues a brand-new refresh token alongside the new access token. If an attacker intercepts a refresh token and uses it, the legitimate user's subsequent request with the same token will trigger a "reuse detection" alarm, allowing the server to immediately invalidate the entire token family and lock out the attacker.
</details>

<details>
<summary>Q2: What is the algorithm confusion vulnerability in JWT libraries?</summary>

**Answer**: If a server uses asymmetric keys (RS256) where the public key is known to everyone, but the verification logic accepts any algorithm specified in the token header, an attacker can change the header to `HS256` and sign the token using the server's public key as an HMAC secret key. When the server validates the token with `HS256` using its public key string, the signature matches! This is prevented by explicitly enforcing `algorithms=["RS256"]` in the verification call.
</details>

<details>
<summary>Q3: Why should tokens include the `jti` (JWT ID) claim?</summary>

**Answer**: The `jti` claim provides a unique identifier for each token. It enables:
1. Building token blacklists in Redis (storing just the `jti` with a TTL equal to the token's remaining lifetime).
2. Tracking refresh token families to implement reuse detection.
3. Preventing replay attacks in one-time use tokens (like password reset links).
</details>

<details>
<summary>Q4: When should you use asymmetric keys (RS256/ES256) versus symmetric keys (HS256)?</summary>

**Answer**: Use symmetric keys (`HS256`) when the issuing service and verifying service are identical or share a secure internal environment, optimizing for high throughput. Use asymmetric keys (`RS256`, `ES256`, `EdDSA`) when multiple distributed microservices, external clients, or third parties need to verify tokens independently without knowing the private signing key.
</details>

<details>
<summary>Q5: Why is clock skew handling (`leeway`) necessary in production distributed systems?</summary>

**Answer**: In cloud environments, server clocks can drift by a few seconds despite NTP synchronization. If an auth server issues a token at timestamp $T$, and a consumer server's clock is 2 seconds behind, the token's `nbf` or `iat` appears to be in the future, causing valid requests to be rejected. Setting `leeway=10` allows a 10-second buffer to absorb minor clock variances.
</details>

<details>
<summary>Q6: What is the consequence of storing JWTs in browser `localStorage` vs. `HttpOnly` cookies?</summary>

**Answer**: Any JavaScript code executing on a page (including malicious third-party dependencies, injected scripts, or XSS exploits) has unrestricted read access to `localStorage`. In contrast, cookies marked `HttpOnly` cannot be read or accessed by client-side JavaScript, rendering them immune to credential extraction via XSS.
</details>

<details>
<summary>Q7: How do you implement instant token revocation for a stateless JWT?</summary>

**Answer**: While pure stateless JWTs cannot be revoked, practical systems maintain a fast distributed cache (Redis) containing a blacklist of revoked `jti` IDs or a user-level `revoked_before` timestamp. Verification checks the signature locally, then performs an $O(1)$ lookup in Redis. Setting the Redis TTL to match the token's remaining `exp` ensures the cache size remains bounded.
</details>

<details>
<summary>Q8: Why must you verify the `token_type` claim in an endpoint dependency?</summary>

**Answer**: If the `type` claim is not checked, an attacker or confused client can send a long-lived refresh token in the `Authorization: Bearer` header of a standard API endpoint. If the signature and expiration are valid, the request would succeed, effectively bypassing the 10-minute access token limit.
</details>

---

## 13. Practical Exercises & Solutions

<details>
<summary>Exercise 1 (Easy): Create a function to generate a 10-minute access token</summary>

**Problem**: Write a function `generate_token(user_id, role, secret)` that outputs an HS256 JWT expiring in 10 minutes.

**Solution**:
```python
from datetime import datetime, timedelta, timezone
import uuid
import jwt

def generate_token(user_id: str, role: str, secret: str) -> str:
    now = datetime.now(timezone.utc)
    payload = {
        "sub": user_id,
        "role": role,
        "type": "access",
        "jti": str(uuid.uuid4()),
        "iat": now,
        "exp": now + timedelta(minutes=10),
    }
    return jwt.encode(payload, secret, algorithm="HS256")
```
</details>

<details>
<summary>Exercise 2 (Easy): Extract user claims safely with exception handling</summary>

**Problem**: Write a function that extracts `sub` and `role` from a token, returning `None` if the token is expired or invalid.

**Solution**:
```python
import jwt

def safe_extract_claims(token: str, secret: str) -> dict | None:
    try:
        payload = jwt.decode(token, secret, algorithms=["HS256"])
        return {"user_id": payload["sub"], "role": payload.get("role")}
    except jwt.PyJWTError:
        return None
```
</details>

<details>
<summary>Exercise 3 (Medium): Build a Redis-Backed JTI Blacklist Dependency</summary>

**Problem**: Create a FastAPI dependency that verifies a JWT and checks if its `jti` exists in a simulated Redis blacklist.

**Solution**:
```python
from datetime import datetime, timezone
from typing import Annotated
from fastapi import Depends, HTTPException, Security, status
from fastapi.security import HTTPAuthorizationCredentials, HTTPBearer
import jwt

bearer = HTTPBearer()
REDIS_BLACKLIST: set[str] = {"blacklisted_jti_001"}
SECRET = "secret-key"

async def verify_non_revoked_token(
    creds: Annotated[HTTPAuthorizationCredentials, Security(bearer)]
) -> dict:
    try:
        payload = jwt.decode(creds.credentials, SECRET, algorithms=["HS256"])
        jti = payload.get("jti")
        if not jti or jti in REDIS_BLACKLIST:
            raise HTTPException(
                status_code=status.HTTP_401_UNAUTHORIZED,
                detail="Token has been revoked",
            )
        return payload
    except jwt.PyJWTError:
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="Invalid token")
```
</details>

<details>
<summary>Exercise 4 (Medium): Build an Asymmetric ES256 Token Signer and Verifier</summary>

**Problem**: Implement token generation and verification using ECDSA with P-256 curve (`ES256`).

**Solution**:
```python
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives import serialization
from datetime import datetime, timedelta, timezone
import jwt

# Generate EC private key
private_key = ec.generate_private_key(ec.SECP256R1())
public_key = private_key.public_key()

def sign_es256(sub: str) -> str:
    payload = {
        "sub": sub,
        "exp": datetime.now(timezone.utc) + timedelta(minutes=15),
    }
    return jwt.encode(payload, private_key, algorithm="ES256")

def verify_es256(token: str) -> dict:
    return jwt.decode(token, public_key, algorithms=["ES256"])
```
</details>

<details>
<summary>Exercise 5 (Hard): Implement Sliding Session Invalidation via `user_revoked_at`</summary>

**Problem**: Implement global user session invalidation where changing a password updates `user.tokens_revoked_at` in the database, invalidating all previously issued JWTs without storing every JTI.

**Solution**:
```python
from datetime import datetime, timezone
from typing import Annotated
from fastapi import Depends, HTTPException, Security, status
from fastapi.security import HTTPAuthorizationCredentials, HTTPBearer
import jwt

bearer = HTTPBearer()
# Simulated database record
USER_DB = {
    "usr_42": {
        "tokens_revoked_at": datetime(2026, 3, 31, 12, 0, 0, tzinfo=timezone.utc)
    }
}

async def verify_user_session(
    creds: Annotated[HTTPAuthorizationCredentials, Security(bearer)]
) -> str:
    payload = jwt.decode(creds.credentials, "secret", algorithms=["HS256"])
    user_id = payload["sub"]
    token_iat = datetime.fromtimestamp(payload["iat"], tz=timezone.utc)

    user = USER_DB.get(user_id)
    if not user:
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="User not found")

    revoked_at = user["tokens_revoked_at"]
    if token_iat < revoked_at:
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Session invalidated due to password change or security reset",
        )
    return user_id
```
</details>

<details>
<summary>Exercise 6 (Hard): Build an Audited Multi-Tenant Token Validator</summary>

**Problem**: Create a dependency that validates tenant isolation by enforcing `iss` (Issuer), `aud` (Audience), and matching the tenant ID in the path `/orgs/{org_id}/...` against token claims.

**Solution**:
```python
from typing import Annotated
from fastapi import Depends, HTTPException, Path, Security, status
from fastapi.security import HTTPAuthorizationCredentials, HTTPBearer
import jwt

bearer = HTTPBearer()

async def verify_tenant_isolation(
    org_id: Annotated[str, Path()],
    creds: Annotated[HTTPAuthorizationCredentials, Security(bearer)],
) -> dict:
    try:
        payload = jwt.decode(
            creds.credentials,
            "secret",
            algorithms=["HS256"],
            issuer="https://auth.enterprise.com",
            audience="https://api.enterprise.com",
        )
    except jwt.PyJWTError as e:
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail=f"Invalid token: {e}")

    token_org_id = payload.get("org_id")
    if token_org_id != org_id:
        raise HTTPException(
            status_code=status.HTTP_403_FORBIDDEN,
            detail=f"Cross-tenant access forbidden. Token org '{token_org_id}' cannot access '{org_id}'",
        )
    return payload
```
</details>
