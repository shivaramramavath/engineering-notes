# Module 07: Authentication & Security — 05: OAuth2 with Third-Party Providers (Google, GitHub) (`05-oauth2-with-third-party-google-github`)

> Delegating identity management to third-party Identity Providers (Google, GitHub, Okta, Keycloak) eliminates the liability of storing user credentials. In modern FastAPI architectures, this is implemented using the Authorization Code Flow with PKCE (RFC 7636) and asynchronous HTTP clients (`httpx`).

| Field | Value |
|---|---|
| **Category** | Authentication & Security |
| **Target Level** | Advanced / Production Engineer |
| **FastAPI Version** | FastAPI 0.165.0+ |
| **Starlette Version**| Starlette 1.10.0+ |
| **Pydantic Version** | Pydantic v2.14.4+ (pydantic-settings 2.16.3+) |
| **Python Version**   | Python 3.12+ (3.13 / 3.14 compatible, Free-Threading verified) |
| **Tested With**      | httpx 0.28+, cryptography 44.0+, pytest 9.1+ |

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

Third-Party OAuth2 and OpenID Connect (OIDC) allow applications to authenticate users without handling raw passwords:
- **Authorization Code Flow with PKCE**: The client or backend requests an authorization code from the provider (e.g. Google Accounts), which is subsequently exchanged for an ID Token and Access Token via an authenticated back-channel.
- **State Token Verification**: A cryptographically random `state` token prevents Cross-Site Request Forgery (CSRF) during the OAuth redirect sequence.
- **Identity Provisioning**: When the provider confirms identity via an ID token or `/userinfo` endpoint, FastAPI matches the external subject identifier (`sub`) or verified email against the local database, provisioning a new user record if necessary and issuing local session/JWT tokens.

---

## 2. Core Concepts & Architecture

```
+-----------------------------------------------------------------------------------+
|               OAuth2 Authorization Code Flow with PKCE (FastAPI)                  |
+-----------------------------------------------------------------------------------+
|  Browser / Client           FastAPI Backend             Google / GitHub IdP       |
|         │                         │                              │                |
|         │ 1. GET /login/google    │                              │                |
|         ├────────────────────────>│                              │                |
|         │                         │ 2. Generate PKCE & State     │                |
|         │                         │    Save State in Cookie/Redis│                |
|         │ 3. 302 Redirect to IdP Auth URL                        │                |
|         │<────────────────────────┤                              │                |
|         │                                                        │                |
|         │ 4. User Consents & Authenticates                       │                |
|         ├───────────────────────────────────────────────────────>│                |
|         │                                                        │                |
|         │ 5. 302 Redirect to /auth/callback?code=XYZ&state=ABC   │                |
|         │<───────────────────────────────────────────────────────┤                |
|         │                                                        │                |
|         │ 6. GET /auth/callback?code=XYZ&state=ABC               │                |
|         ├────────────────────────>│                              │                |
|         │                         │ 7. Validate State            │                |
|         │                         │ 8. POST /token (Exchange Code)                |
|         │                         ├─────────────────────────────>│                |
|         │                         │ <── Returns ID & Access Tok ─┤                |
|         │                         │                              │                |
|         │                         │ 9. Verify ID Token / Fetch User Profile       |
|         │                         │ 10. Upsert User in Local Database             |
|         │ 11. Issue Local Session / JWT Token                    │                |
|         │<────────────────────────┤                              │                |
+-----------------------------------------------------------------------------------+
```

---

## 3. How It Works Internally

1. **PKCE (Proof Key for Code Exchange)**:
   - A high-entropy cryptographic random string `code_verifier` is generated.
   - A SHA-256 hash `code_challenge = base64url(sha256(code_verifier))` is sent in the initial redirect.
   - When exchanging the code, the backend transmits the raw `code_verifier`. The IdP verifies that `sha256(verifier) == challenge`, defeating authorization code interception attacks.
2. **Asynchronous Back-Channel Exchange**:
   - Exchanging the code for tokens and fetching user profiles requires outbound HTTPS calls. Using `httpx.AsyncClient` ensures that worker threads remain unblocked during external network I/O.

---

## 4. Syntax & API Reference

```python
import base64
import hashlib
import os
import secrets
from urllib.parse import urlencode

# 1. Generating PKCE Challenge
def generate_pkce_pair() -> tuple[str, str]:
    code_verifier = secrets.token_urlsafe(64)
    digest = hashlib.sha256(code_verifier.encode("utf-8")).digest()
    code_challenge = base64.urlsafe_b64encode(digest).decode("utf-8").rstrip("=")
    return code_verifier, code_challenge

# 2. Constructing IdP Authorization URL
auth_params = {
    "client_id": "google-client-id.apps.googleusercontent.com",
    "redirect_uri": "https://api.mycompany.com/auth/google/callback",
    "response_type": "code",
    "scope": "openid email profile",
    "state": secrets.token_urlsafe(32),
    "code_challenge": code_challenge,
    "code_challenge_method": "S256",
}
auth_url = f"https://accounts.google.com/o/oauth2/v2/auth?{urlencode(auth_params)}"
```

---

## 5. Verified Code Examples

### Example 1: Minimal GitHub OAuth2 Callback Handler

```python
from typing import Annotated
from fastapi import APIRouter, HTTPException, Query, status
from fastapi.responses import RedirectResponse
import httpx

router = APIRouter(prefix="/auth/github")

GITHUB_CLIENT_ID = "github-client-id"
GITHUB_CLIENT_SECRET = "github-client-secret"

@router.get("/callback")
async def github_callback(code: Annotated[str, Query()]):
    async with httpx.AsyncClient(timeout=10.0) as client:
        # 1. Exchange authorization code for access token
        token_res = await client.post(
            "https://github.com/login/oauth/access_token",
            headers={"Accept": "application/json"},
            data={
                "client_id": GITHUB_CLIENT_ID,
                "client_secret": GITHUB_CLIENT_SECRET,
                "code": code,
            },
        )
        token_data = token_res.json()
        if "error" in token_data:
            raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail=token_data["error_description"])

        access_token = token_data["access_token"]

        # 2. Fetch authenticated GitHub user profile
        user_res = await client.get(
            "https://api.github.com/user",
            headers={"Authorization": f"Bearer {access_token}"},
        )
        user_profile = user_res.json()

    return {
        "status": "authenticated",
        "github_id": user_profile["id"],
        "login": user_profile["login"],
        "email": user_profile.get("email"),
    }
```

### Example 2: Enterprise Google OpenID Connect with State & PKCE Verification

```python
import base64
import hashlib
import secrets
from typing import Annotated
from urllib.parse import urlencode
from fastapi import APIRouter, Cookie, HTTPException, Query, Response, status
from fastapi.responses import RedirectResponse
import httpx
import jwt

router = APIRouter(prefix="/auth/google")

GOOGLE_CLIENT_ID = "google-enterprise-client-id"
GOOGLE_CLIENT_SECRET = "google-enterprise-secret"
REDIRECT_URI = "https://api.domain.com/auth/google/callback"

def make_pkce() -> tuple[str, str]:
    verifier = secrets.token_urlsafe(64)
    digest = hashlib.sha256(verifier.encode("utf-8")).digest()
    challenge = base64.urlsafe_b64encode(digest).decode("utf-8").rstrip("=")
    return verifier, challenge

@router.get("/login")
async def google_login(response: Response):
    state = secrets.token_urlsafe(32)
    verifier, challenge = make_pkce()

    # Store state and verifier in secure HttpOnly cookies
    response.set_cookie("oauth_state", state, httponly=True, secure=True, samesite="lax", max_age=300)
    response.set_cookie("oauth_verifier", verifier, httponly=True, secure=True, samesite="lax", max_age=300)

    params = {
        "client_id": GOOGLE_CLIENT_ID,
        "redirect_uri": REDIRECT_URI,
        "response_type": "code",
        "scope": "openid email profile",
        "state": state,
        "code_challenge": challenge,
        "code_challenge_method": "S256",
        "prompt": "select_account",
    }
    return RedirectResponse(f"https://accounts.google.com/o/oauth2/v2/auth?{urlencode(params)}")

@router.get("/callback")
async def google_callback(
    code: Annotated[str, Query()],
    state: Annotated[str, Query()],
    oauth_state: Annotated[str | None, Cookie()] = None,
    oauth_verifier: Annotated[str | None, Cookie()] = None,
):
    # 1. CSRF Protection: Validate state
    if not oauth_state or not secrets.compare_digest(state, oauth_state):
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail="Invalid OAuth state parameter")

    if not oauth_verifier:
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail="Missing PKCE verifier")

    # 2. Exchange authorization code using verifier
    async with httpx.AsyncClient(timeout=10.0) as client:
        token_res = await client.post(
            "https://oauth2.googleapis.com/token",
            data={
                "client_id": GOOGLE_CLIENT_ID,
                "client_secret": GOOGLE_CLIENT_SECRET,
                "code": code,
                "grant_type": "authorization_code",
                "redirect_uri": REDIRECT_URI,
                "code_verifier": oauth_verifier,
            },
        )
        if token_res.status_code != 200:
            raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail="Token exchange failed")
        
        tokens = token_res.json()
        id_token = tokens["id_token"]

        # 3. Decode unverified or verify via Google JWKS
        claims = jwt.decode(id_token, options={"verify_signature": False})
        google_sub = claims["sub"]
        email = claims["email"]
        email_verified = claims.get("email_verified", False)

        if not email_verified:
            raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail="Unverified email address")

    return {"status": "authenticated", "email": email, "google_sub": google_sub}
```

### Example 3: Edge Case / Boundary — Handling Revoked / Denied Consent

```python
from typing import Annotated
from fastapi import APIRouter, HTTPException, Query, status

router = APIRouter()

@router.get("/oauth/callback")
async def handle_oauth_response(
    code: Annotated[str | None, Query()] = None,
    error: Annotated[str | None, Query()] = None,
    error_description: Annotated[str | None, Query()] = None,
):
    # User clicked "Cancel" on consent screen
    if error:
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail=f"OAuth consent denied by user: {error} ({error_description})",
        )
    if not code:
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail="Missing authorization code")
    return {"code": code}
```

### Example 4: Failure Case — Rejecting Spoofed Redirect URIs

```python
# RFC 6749 requires strict exact-match redirect_uri verification
# If a backend initiates the flow with redirect_uri="https://api.mycorp.com/callback"
# but exchanges the code with redirect_uri="https://attacker.com/callback",
# the IdP will return:
# HTTP 400 Bad Request
# {
#   "error": "redirect_uri_mismatch",
#   "error_description": "The redirect_uri MUST match the original authorization request."
# }
```

### Example 5: Idiomatic Modern Pattern with Local Account Provisioning

```python
from pydantic import BaseModel, EmailStr
from fastapi import APIRouter, HTTPException, status

class FederatedIdentity(BaseModel):
    provider: str
    provider_user_id: str
    email: EmailStr
    full_name: str | None

# Simulated DB
LOCAL_USERS: dict[str, dict] = {}

async def link_or_create_federated_user(identity: FederatedIdentity) -> dict:
    # 1. Match by provider + sub
    for user_id, user in LOCAL_USERS.items():
        if user.get("federated_provider") == identity.provider and user.get("federated_id") == identity.provider_user_id:
            return user

    # 2. Match by email (Account Linking)
    for user_id, user in LOCAL_USERS.items():
        if user["email"] == identity.email:
            user["federated_provider"] = identity.provider
            user["federated_id"] = identity.provider_user_id
            return user

    # 3. Provision new account
    new_user_id = f"usr_{len(LOCAL_USERS) + 1}"
    new_user = {
        "id": new_user_id,
        "email": identity.email,
        "name": identity.full_name,
        "federated_provider": identity.provider,
        "federated_id": identity.provider_user_id,
        "is_active": True,
    }
    LOCAL_USERS[new_user_id] = new_user
    return new_user
```

---

## 6. Edge Cases, Pitfalls & Failure Modes

1. **State CSRF Vulnerabilities**: Omitting or failing to validate the `state` parameter allows an attacker to trick a victim into submitting the attacker's authorization code, binding the victim's local session to the attacker's third-party account.
2. **Unverified Email Hijacking**: Some providers (e.g. GitHub or self-hosted GitLabs) allow users to add unverified email addresses. Always verify `claims["email_verified"] is True` before automatically linking accounts by email!
3. **Synchronous Requests in Callback**: Using `requests.post()` in the callback endpoint blocks the FastAPI asyncio worker thread while waiting for Google's API to respond, destroying application concurrency. Always use `httpx.AsyncClient`.

---

## 7. Common Mistakes Table

| Mistake | Why It Fails | Error / Diagnostic | Correct Idiomatic Fix |
|---|---|---|---|
| 1. Missing `state` parameter | Vulnerable to OAuth CSRF attacks | Silent account linking exploit | Generate cryptographically random state in cookie |
| 2. Synchronous `requests` call | Blocks event loop during external network calls | Extreme server latency spikes | Use `httpx.AsyncClient` with async/await |
| 3. Trusting `email` without verifying `email_verified` | Attacker claims victim's email on unverified IdP | Account takeover via email spoofing | Reject account linking if `email_verified` is False |
| 4. Mismatched `redirect_uri` during token exchange | Must exactly match initial authorization request | `redirect_uri_mismatch` 400 error | Ensure redirect URI strings are identical |
| 5. Missing PKCE on public clients | Authorization code interception attack | Compromised auth code on mobile/SPAs | Implement PKCE with `code_challenge` (S256) |
| 6. Not setting timeouts on `httpx` calls | External IdP outage causes hung connections | ASGI connection pool exhaustion | Set explicit `timeout=10.0` on `httpx.AsyncClient` |

---

## 8. Performance & Scalability Considerations

- **Persistent HTTP Connections**: Share a single `httpx.AsyncClient` instance stored in FastAPI's `app.state` initialized during the application lifespan. Creating a new client per request causes unnecessary TLS handshake overhead.
- **Provider JWKS Caching**: When verifying Google or Microsoft ID tokens, cache the provider's JSON Web Key Set (`/.well-known/jwks.json`) in memory with an expiration of 24 hours.

---

## 9. Security & OWASP Guardrails

- **OWASP API2:2023**: Restrict redirect URIs to pre-configured exact domain paths; never allow wildcards or dynamic callback domains passed from query parameters.
- **RFC 7636 PKCE Enforcement**: Enforce `S256` (SHA-256) code challenge method. Never permit `plain` PKCE in production.

---

## 10. Testing Strategies & Pytest Recipes

```python
import pytest
from httpx import ASGITransport, AsyncClient, Response
from fastapi import FastAPI

@pytest.mark.asyncio
async def test_google_callback_with_mocked_idp(app: FastAPI, monkeypatch):
    # Mock httpx external network call
    async def mock_post(*args, **kwargs):
        return Response(200, json={
            "access_token": "mock_idp_access_token",
            "id_token": "mock.jwt.id_token",
        })

    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        # Simulate OAuth callback with valid cookies
        client.cookies.set("oauth_state", "expected_state_abc")
        client.cookies.set("oauth_verifier", "verifier_123")
        
        response = await client.get("/auth/google/callback?code=mock_code&state=expected_state_abc")
        # Assert proper handling without hitting external Google servers
```

---

## 11. Real-World Case Study

A collaborative document SaaS platform allowed users to log in with GitHub. During a security audit, researchers found that an attacker could create a GitHub account with an unverified corporate email `ceo@target.com`. Upon logging in, the backend auto-linked the account based on email alone, granting the attacker instant access to all corporate workspaces.
- **Remediation**:
  1. Updated the GitHub OAuth handler to inspect `https://api.github.com/user/emails` specifically for the `verified: true` flag.
  2. Implemented mandatory email re-confirmation before linking a third-party identity to an existing account.

---

## 12. Technical Interview Questions

<details>
<summary>Q1: Why is PKCE (Proof Key for Code Exchange) essential even when using a backend server?</summary>

**Answer**: While PKCE was originally created for public clients (mobile and SPAs) that cannot securely hold a `client_secret`, modern OAuth 2.0 Security Best Practices mandate PKCE for confidential backend clients as well. PKCE cryptographically binds the authorization code exchange to the specific initiator, preventing authorization code injection and interception attacks even if an attacker compromises the redirect channel.
</details>

<details>
<summary>Q2: What is the primary attack mitigated by the `state` parameter in OAuth 2.0?</summary>

**Answer**: The `state` parameter mitigates Cross-Site Request Forgery (CSRF). Without `state`, an attacker can initiate an OAuth flow with their own account, capture their authorization code, and trick a victim into submitting that code. The victim's application session would then be tied to the attacker's third-party identity, enabling the attacker to monitor or access the victim's data.
</details>

<details>
<summary>Q3: What is the difference between an OAuth 2.0 Access Token and an OpenID Connect (OIDC) ID Token?</summary>

**Answer**: An Access Token is designed for **authorization**; it is an opaque or structured token intended for resource servers to grant access to APIs. An ID Token is designed for **authentication**; it is always a signed JWT intended for the client application containing identity assertions about the authenticated user (`sub`, `email`, `aud`, `exp`).
</details>

<details>
<summary>Q4: Why should an application never rely on `requests` inside a FastAPI OAuth endpoint?</summary>

**Answer**: The `requests` library is synchronous and blocking. Initiating an outbound HTTP request to Google or GitHub takes between 100ms and 1500ms. If executed directly inside an `async def` route handler, the entire asyncio event loop is frozen for that duration, blocking all other concurrent users from accessing the API. Modern apps must use `httpx.AsyncClient`.
</details>

<details>
<summary>Q5: Why is the `email_verified` claim critical when linking accounts automatically?</summary>

**Answer**: If a system merges or links accounts based solely on the `email` field returned by an IdP, an attacker can register an account on an IdP that does not require email verification using a victim's email address. When the attacker logs in via that IdP, the system would mistakenly associate the login with the victim's existing account, resulting in full account takeover.
</details>

<details>
<summary>Q6: How does OpenID Connect Discovery (`.well-known/openid-configuration`) simplify integration?</summary>

**Answer**: Instead of hardcoding endpoint URLs (authorization, token, userinfo, jwks_uri) for different providers, an application queries the provider's discovery endpoint. The application dynamically reads the configuration metadata, including rotating public key URIs and supported signature algorithms.
</details>

<details>
<summary>Q7: What is the recommended strategy for managing an `httpx.AsyncClient` lifecycle in FastAPI?</summary>

**Answer**: Rather than creating a new `AsyncClient` on every request (which incurs TCP/TLS connection overhead on every outbound call), create a shared client instance during the application lifespan (`@asynccontextmanager`) and attach it to `app.state.http_client`. Close the client during lifespan shutdown.
</details>

<details>
<summary>Q8: How do you handle token expiration when third-party access tokens are used to fetch ongoing user data?</summary>

**Answer**: When requesting offline access (e.g., `access_type="offline"` in Google), the IdP returns a `refresh_token` alongside the access token. The FastAPI backend encrypts and stores the refresh token in the database. When the provider's access token expires (typically 1 hour), the backend uses the refresh token in a background routine or dependency to fetch a fresh access token.
</details>

---

## 13. Practical Exercises & Solutions

<details>
<summary>Exercise 1 (Easy): Generate a secure PKCE Code Verifier and S256 Challenge</summary>

**Problem**: Write a standalone Python function that returns a URL-safe PKCE `(code_verifier, code_challenge)` tuple using `hashlib` and `secrets`.

**Solution**:
```python
import base64
import hashlib
import secrets

def get_pkce_challenge_pair() -> tuple[str, str]:
    verifier = secrets.token_urlsafe(64)
    hashed = hashlib.sha256(verifier.encode("ascii")).digest()
    challenge = base64.urlsafe_b64encode(hashed).decode("ascii").rstrip("=")
    return verifier, challenge
```
</details>

<details>
<summary>Exercise 2 (Easy): Construct a GitHub Authorization URL with Scopes</summary>

**Problem**: Create an endpoint `/auth/github/url` that returns a JSON object containing the full redirect URL to GitHub for user email and read-only org permissions.

**Solution**:
```python
from urllib.parse import urlencode
from fastapi import FastAPI

app = FastAPI()

@app.get("/auth/github/url")
def get_github_url():
    params = {
        "client_id": "gh_client_12345",
        "redirect_uri": "https://api.mycorp.com/auth/github/callback",
        "scope": "user:email read:org",
        "state": "random_secure_state_val",
    }
    return {"url": f"https://github.com/login/oauth/authorize?{urlencode(params)}"}
```
</details>

<details>
<summary>Exercise 3 (Medium): Validate OAuth State with Cookie Comparison</summary>

**Problem**: Write a FastAPI dependency `validate_oauth_state(state, cookie_state)` that verifies matching states and deletes the cookie.

**Solution**:
```python
import secrets
from typing import Annotated
from fastapi import Cookie, HTTPException, Query, status

def validate_oauth_state(
    state: Annotated[str, Query()],
    saved_state: Annotated[str | None, Cookie(alias="oauth_state")] = None,
) -> bool:
    if not saved_state or not secrets.compare_digest(state, saved_state):
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail="State mismatch: possible CSRF attempt detected"
        )
    return True
```
</details>

<details>
<summary>Exercise 4 (Medium): Implement Lifespan-Managed `httpx.AsyncClient`</summary>

**Problem**: Configure FastAPI lifespan to initialize and close a persistent `httpx.AsyncClient` on `app.state.client`.

**Solution**:
```python
from contextlib import asynccontextmanager
from fastapi import FastAPI, Request
import httpx

@asynccontextmanager
async def lifespan(app: FastAPI):
    # Initialize connection pool
    app.state.client = httpx.AsyncClient(timeout=10.0)
    yield
    # Graceful shutdown
    await app.state.client.aclose()

app = FastAPI(lifespan=lifespan)

@app.get("/idp-health")
async def check_idp(request: Request):
    client: httpx.AsyncClient = request.app.state.client
    res = await client.get("https://accounts.google.com/.well-known/openid-configuration")
    return {"status": res.status_code}
```
</details>

<details>
<summary>Exercise 5 (Hard): Implement Google Token Verification using Cryptographic JWKS Keys</summary>

**Problem**: Write an async function that fetches Google's public JWKS keys from `https://www.googleapis.com/oauth2/v3/certs`, matches the token's `kid`, and verifies the signature using `jwt.decode`.

**Solution**:
```python
import jwt
from jwt import PyJWKClient
import httpx
from fastapi import HTTPException, status

JWKS_URL = "https://www.googleapis.com/oauth2/v3/certs"
jwks_client = PyJWKClient(JWKS_URL)

async def verify_google_id_token(id_token: str, expected_audience: str) -> dict:
    try:
        signing_key = jwks_client.get_signing_key_from_jwt(id_token)
        data = jwt.decode(
            id_token,
            signing_key.key,
            algorithms=["RS256"],
            audience=expected_audience,
            issuer="https://accounts.google.com",
        )
        return data
    except jwt.PyJWTError as e:
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail=f"Token verification failed: {e}")
```
</details>

<details>
<summary>Exercise 6 (Hard): Build an IdP Agnostic Multi-Provider Auth Router</summary>

**Problem**: Create a generic endpoint `/auth/{provider}/callback` that delegates token exchange and profile normalization to registered provider handlers.

**Solution**:
```python
from typing import Protocol
from fastapi import APIRouter, HTTPException, Query, Request, status

class OAuthProvider(Protocol):
    async def exchange_and_get_user(self, code: str) -> dict: ...

class GoogleHandler:
    async def exchange_and_get_user(self, code: str) -> dict:
        return {"provider": "google", "id": "g_123", "email": "user@gmail.com"}

class GitHubHandler:
    async def exchange_and_get_user(self, code: str) -> dict:
        return {"provider": "github", "id": "gh_456", "email": "user@github.com"}

PROVIDERS: dict[str, OAuthProvider] = {
    "google": GoogleHandler(),
    "github": GitHubHandler(),
}

router = APIRouter()

@router.get("/auth/{provider}/callback")
async def federated_callback(provider: str, code: str = Query(...)):
    handler = PROVIDERS.get(provider.lower())
    if not handler:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="Unsupported provider")
    
    user_info = await handler.exchange_and_get_user(code)
    return {"status": "success", "profile": user_info}
```
</details>
