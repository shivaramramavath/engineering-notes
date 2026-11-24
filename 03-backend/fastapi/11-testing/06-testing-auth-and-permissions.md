---
title: "Testing Authentication, Authorization, and Permissions in FastAPI"
category: "11-testing"
order: 6
tags: ["fastapi", "testing", "authentication", "authorization", "rbac", "jwt", "security"]
py_version: "3.12+"
fastapi_version: "0.115+"
starlette_version: "0.38+"
pydantic_version: "2.8+"
tested_with: "Python 3.12, 3.13"
---

# Testing Authentication, Authorization, and Permissions in FastAPI

## 1. Conceptual Foundation & Mental Model

Security testing in web APIs is frequently neglected: developers test happy paths, assuming that if a user with valid credentials receives `HTTP 200 OK`, the security system works.

In production, the most catastrophic vulnerabilities (e.g. Broken Object Level Authorization - BOLA / IDOR, Broken Function Level Authorization) occur because **negative access boundaries** were never tested. A robust test suite must systematically verify:
1. **Unauthenticated Access (401 Unauthorized)**: Requests with missing, malformed, or expired credentials cannot access protected resources.
2. **Unauthorized Access (403 Forbidden)**: Authenticated users lacking the required Role-Based Access Control (RBAC) permissions or tenant privileges cannot perform actions on endpoints outside their clearance.
3. **IDOR / Tenant Isolation (BOLA)**: User A cannot read, update, or delete resources belonging to User B, even if User A provides a valid JWT.

### The Mental Model: The High-Security Research Facility
- Testing **Authentication** is testing the electronic security turnstile in the lobby. If an unbadged stranger tries to push through (**Missing Token**) or someone presents an expired badge (**Expired JWT**), the turnstile physically locks and sounds an alarm (**`HTTP 401 Unauthorized`**).
- Testing **Role-Based Authorization (RBAC)** is the laboratory door inside the building. A certified lab technician has passed the lobby turnstile, but when they swipe their badge at the *Nuclear Materials Vault* (**Admin Endpoint**), the door buzzer denies access (**`HTTP 403 Forbidden`**).
- Testing **IDOR / Multi-Tenancy** is the personal employee lockers. Every scientist has a locker key, but Scientist Smith's key must never open Scientist Johnson's locker (**Tenant Isolation**).

---

## 2. Architecture & Security Test Matrix

```
                          Incoming Request
                                 |
                                 v
                     Lobby: Authentication Layer
                                 |
                 +---------------+---------------+
                 |                               |
          Invalid / Missing               Valid Bearer JWT
                 |                               |
                 v                               v
          HTTP 401 Unauthorized         Identify Subject: User
                                                 |
                                                 v
                                    Vault: Authorization Layer
                                                 |
                                 +---------------+---------------+
                                 |                               |
                          Lacks Permission                Has Permission
                                 |                               |
                                 v                               v
                         HTTP 403 Forbidden             Check Object Ownership
                                                        (User A owns Resource?)
                                                                 |
                                                 +---------------+---------------+
                                                 |                               |
                                                NO                              YES
                                                 |                               |
                                                 v                               v
                                         HTTP 404 / 403                   HTTP 200 OK
```

### The Security Test Matrix
Every protected endpoint must have at least 4 automated tests:
1. `test_missing_token_returns_401`
2. `test_expired_token_returns_401`
3. `test_insufficient_role_returns_403`
4. `test_cross_tenant_access_returns_404_or_403`

---

## 3. Comprehensive Syntax & Core Mechanics

### 1. Generating Deterministic Test JWTs
```python
import jwt
from datetime import datetime, timezone, timedelta

TEST_JWT_SECRET = "insecure-test-secret-key-32-chars-min"
ALGORITHM = "HS256"

def create_test_token(user_id: str, role: str = "member", expires_delta: timedelta = timedelta(hours=1)) -> str:
    now = datetime.now(timezone.utc)
    payload = {
        "sub": user_id,
        "role": role,
        "iat": now,
        "exp": now + expires_delta
    }
    return jwt.encode(payload, TEST_JWT_SECRET, algorithm=ALGORITHM)
```

### 2. Pytest Fixture Matrix for Personas
```python
import pytest
from httpx import AsyncClient

@pytest.fixture
def member_token():
    return create_test_token("user_regular_1", role="member")

@pytest.fixture
def admin_token():
    return create_test_token("user_admin_99", role="admin")

@pytest.fixture
def expired_token():
    return create_test_token("user_expired", role="member", expires_delta=timedelta(seconds=-10))

@pytest.fixture
def auth_headers(member_token: str) -> dict[str, str]:
    return {"Authorization": f"Bearer {member_token}"}

@pytest.fixture
def admin_headers(admin_token: str) -> dict[str, str]:
    return {"Authorization": f"Bearer {admin_token}"}
```

---

## 4. Practical Implementation

### Example 1: Testing RBAC Route Boundaries
```python
import pytest
from httpx import AsyncClient, ASGITransport
from fastapi import FastAPI, Depends, HTTPException, status
from fastapi.security import HTTPBearer, HTTPAuthorizationCredentials
import jwt

app = FastAPI()
security = HTTPBearer()

def get_current_user(creds: HTTPAuthorizationCredentials = Depends(security)) -> dict:
    try:
        payload = jwt.decode(creds.credentials, TEST_JWT_SECRET, algorithms=[ALGORITHM])
        return payload
    except jwt.ExpiredSignatureError:
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="Token expired")
    except jwt.PyJWTError:
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="Invalid token")

def require_admin(user: dict = Depends(get_current_user)):
    if user.get("role") != "admin":
        raise HTTPException(status_code=status.HTTP_403_FORBIDDEN, detail="Admin clearance required")
    return user

@app.delete("/system/purge", dependencies=[Depends(require_admin)])
async def purge_system():
    return {"status": "PURGED"}

# ----------------- Automated Security Suite -----------------

@pytest.mark.anyio
async def test_purge_missing_token_returns_401():
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        resp = await client.delete("/system/purge")
        assert resp.status_code == 401

@pytest.mark.anyio
async def test_purge_expired_token_returns_401(expired_token):
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        resp = await client.delete("/system/purge", headers={"Authorization": f"Bearer {expired_token}"})
        assert resp.status_code == 401
        assert resp.json()["detail"] == "Token expired"

@pytest.mark.anyio
async def test_purge_member_role_returns_403(auth_headers):
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        resp = await client.delete("/system/purge", headers=auth_headers)
        assert resp.status_code == 403
        assert resp.json()["detail"] == "Admin clearance required"

@pytest.mark.anyio
async def test_purge_admin_role_succeeds(admin_headers):
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        resp = await client.delete("/system/purge", headers=admin_headers)
        assert resp.status_code == 200
        assert resp.json()["status"] == "PURGED"
```

### Example 2: Testing IDOR / BOLA (Cross-Tenant Object Isolation)
Testing that User A cannot read or delete documents owned by User B:
```python
from fastapi import FastAPI, Depends, HTTPException, status

app = FastAPI()

# Simulated database store
documents = {
    "doc_alice_1": {"owner": "alice_uuid", "text": "Alice's Secret Medical Data"},
    "doc_bob_1": {"owner": "bob_uuid", "text": "Bob's Public Shopping List"}
}

@app.get("/documents/{doc_id}")
async def get_document(doc_id: str, user: dict = Depends(get_current_user)):
    doc = documents.get(doc_id)
    if not doc:
        raise HTTPException(status_code=404, detail="Document not found")
        
    # Enforce Object-Level Ownership
    if doc["owner"] != user["sub"] and user.get("role") != "admin":
        # Best Practice: Return 404 (or 403) to prevent resource enumeration
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="Document not found")
        
    return doc

@pytest.mark.anyio
async def test_idor_user_cannot_read_other_users_document():
    # Authenticated as Bob
    bob_token = create_test_token("bob_uuid", role="member")
    
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        # Bob attempts to access Alice's document
        resp = await client.get("/documents/doc_alice_1", headers={"Authorization": f"Bearer {bob_token}"})
        
        # Must be rejected (404 or 403), NEVER 200!
        assert resp.status_code == 404
```

### Example 3: Edge Case — Testing Tampered Signatures and Algorithm Confusion
```python
@pytest.mark.anyio
async def test_tampered_jwt_signature_rejected():
    # Generate legitimate token
    valid_token = create_test_token("user_101", role="member")
    
    # Tamper with the signature bytes (last segment)
    parts = valid_token.split(".")
    tampered_token = f"{parts[0]}.{parts[1]}.tampered_signature_bytes"
    
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        resp = await client.get("/dashboard/metrics", headers={"Authorization": f"Bearer {tampered_token}"})
        assert resp.status_code == 401
```

### Example 4: Failure Case — Testing Only the Happy Path
```python
# CATASTROPHIC ANTI-PATTERN
def test_security_lazy():
    # Only tests that an admin can access the endpoint.
    # MISSING: Tests for unauthenticated, regular user, and tampered tokens!
    # If a developer accidentally deletes require_admin, this test STILL PASSES!
    resp = client.get("/admin/purge", headers=admin_headers)
    assert resp.status_code == 200
```

### Example 5: Idiomatic Modern — Parametric Permission Matrix Verification
```python
import pytest

@pytest.mark.anyio
@pytest.mark.parametrize("role, expected_status", [
    ("guest", 403),
    ("member", 403),
    ("editor", 403),
    ("manager", 200),
    ("admin", 200)
])
async def test_permission_matrix_across_roles(role, expected_status):
    token = create_test_token(f"user_{role}", role=role)
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        resp = await client.post("/reports/export", headers={"Authorization": f"Bearer {token}"})
        assert resp.status_code == expected_status
```

---

## 5. Edge Cases, Pitfalls & Failure Modes

1. **Algorithm Switching Attacks (`none` Algorithm)**: Older JWT libraries permitted `"alg": "none"`. Modern tests should assert that tokens with `"alg": "none"` and empty signatures are rejected with `HTTP 401`.
2. **Clock Skew During Expiration Checks**: If the test machine's clock drifts or tests run across Leap Seconds, tight expiration checks can fail. In tests, use explicit timedelta offsets (e.g. $+3600\text{s}$ or $-60\text{s}$).
3. **Information Disclosure via 403 vs 404 in IDOR Tests**: In IDOR / BOLA testing, returning `403 Forbidden` confirms to an attacker that the targeted resource ID exists! Returning `HTTP 404 Not Found` prevents resource enumeration attacks. Test that the API returns 404 for unowned objects.
4. **Scope Creep across OAuth2 Scopes**: If your API uses OAuth2 scopes (`SecurityScopes`), test that a token with scope `read:users` cannot perform `write:users` actions.

---

## 6. Common Mistakes & Anti-Patterns

| Anti-Pattern | Root Cause | Consequence | Correct Practice |
|:---|:---|:---|:---|
| Only testing happy paths (200 OK) | Lazy testing habits | Authorization regressions go unnoticed in CI | Test negative cases (401, 403, 404) |
| Hardcoding production JWT secrets in tests | Copying production configs | Exposes production signing keys | Use a dedicated dummy test secret |
| Returning 403 on unowned resources (IDOR) | Lack of resource enumeration awareness | Attackers enumerate existing IDs | Return 404 for resources not owned by caller |
| Missing expired token test | Assuming JWT library works out of the box | Expired tokens accepted if verification disabled | Explicitly test negative timestamps |
| Mocking out authentication globally | Disabling auth in `conftest.py` | Security logic is never tested in integration | Test with real tokens; mock only for unit tests |
| Missing test for `none` algorithm vulnerability | Trusting client headers | Critical authentication bypass vulnerabilities | Assert `none` algorithm tokens are rejected |

---

## 7. Performance & Scalability Considerations

- **HMAC vs RSA in Tests**: In production, asymmetric cryptography (RS256/Ed25519) is often used for JWT verification. Generating RSA keys in tests takes 50ms per keypair. In test suites, generate a single shared RSA keypair in a session-scoped fixture or use symmetric HS256 for fast unit tests.
- **In-Memory JWT Generation**: Minting a test JWT with `pyjwt` takes $<10\text{ microseconds}$, adding zero perceptible latency to integration tests.

---

## 8. Security & OWASP Guardrails

- **OWASP API Top 10 Compliance**:
  - API1:2023 - Broken Object Level Authorization (BOLA)
  - API2:2023 - Broken Authentication
  - API5:2023 - Broken Function Level Authorization
  - Automated security tests for every endpoint are the primary defense against these top 3 API vulnerabilities.

---

## 9. Testing Strategies & Pytest Recipes

```python
import pytest
from httpx import AsyncClient, ASGITransport

@pytest.mark.anyio
async def test_jwt_none_algorithm_exploit_is_blocked():
    # Hand-craft an unsigned JWT with alg="none"
    header = "eyJhbGciOiJub25lIiwidHlwIjoiSldUIn0"  # {"alg":"none","typ":"JWT"}
    payload = "eyJzdWIiOiJhZG1pbiIsInJvbGUiOiJhZG1pbiJ9" # {"sub":"admin","role":"admin"}
    none_token = f"{header}.{payload}."
    
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        resp = await client.get("/admin/metrics", headers={"Authorization": f"Bearer {none_token}"})
        assert resp.status_code == 401
```

---

## 10. Real-World Case Study / Production Incident

**The Incident**: A telemedicine startup suffered a massive data breach where 120,000 patient psychiatric medical records were scraped. The hacker discovered that by changing the URL from `GET /api/patients/1001/notes` to `GET /api/patients/1002/notes`, they could read every patient's private doctor notes.

**Root Cause**: The route handler verified authentication:
```python
@app.get("/api/patients/{patient_id}/notes")
async def get_patient_notes(patient_id: int, user: User = Depends(get_current_user)):
    # AUTHENTICATION WAS TESTED AND PASSED!
    # BUT AUTHORIZATION WAS COMPLETELY MISSING!
    return await db.get_notes(patient_id)
```
The test suite had 40 tests verifying that invalid JWTs returned 401. However, **nobody had ever written an IDOR test** verifying that Patient A could not read Patient B's notes!

**The Fix**:
1. Added object-level ownership checks inside a dependency:
   ```python
   if user.id != patient_id and user.role != "doctor":
       raise HTTPException(status_code=404)
   ```
2. Mandated an automated IDOR test for every single customer-facing endpoint in CI, preventing any future pull request from merging without cross-tenant verification.

---

## 11. Technical Interview Questions & Answers

<details>
<summary>1. What is the difference between testing Authentication (401) and testing Authorization (403)?</summary>
<b>Authentication (401 Unauthorized)</b> tests verify that identity is established: requests without a token, with an expired token, or with a tampered signature must be rejected. <b>Authorization (403 Forbidden)</b> tests verify that an authenticated identity has the required roles, permissions, or clearances to perform the requested operation.
</details>

<details>
<summary>2. What is BOLA / IDOR and how do you write an integration test to catch it?</summary>
BOLA (Broken Object Level Authorization) / IDOR (Insecure Direct Object Reference) occurs when an authenticated user can access another user's private resources by manipulating an ID in the URL. To test for it: instantiate two separate user tokens (User A and User B), create a resource owned by User A, and assert that User B receives <code>HTTP 404</code> or <code>HTTP 403</code> when attempting to read or mutate it.
</details>

<details>
<summary>3. Why should an IDOR violation return <code>HTTP 404 Not Found</code> instead of <code>HTTP 403 Forbidden</code>?</summary>
Returning <code>HTTP 403 Forbidden</code> reveals to an attacker that the targeted resource ID exists in the database, enabling resource enumeration attacks. Returning <code>HTTP 404 Not Found</code> masks the existence of the resource, treating an unowned object the same as a non-existent object.
</details>

<details>
<summary>4. What is the JWT <code>none</code> algorithm vulnerability and how do you test for it?</summary>
The <code>none</code> algorithm vulnerability occurs when a JWT validator accepts tokens signed with <code>{"alg": "none"}</code> and an empty signature, allowing attackers to forge arbitrary administrative tokens. You test for it by submitting an unsigned token with <code>alg: none</code> and asserting the API returns <code>HTTP 401 Unauthorized</code>.
</details>

<details>
<summary>5. How can you test token expiration without calling <code>time.sleep()</code>?</summary>
By minting a test token whose <code>exp</code> claim is explicitly set in the past (e.g. <code>now - timedelta(seconds=10)</code>). When passed to the endpoint, the JWT decoder immediately raises <code>ExpiredSignatureError</code> in sub-milliseconds without waiting.
</details>

<details>
<summary>6. How does Pytest parameterization improve RBAC test coverage?</summary>
Using <code>@pytest.mark.parametrize("role, expected_status", [...])</code>, a single test function can evaluate an entire matrix of user roles (Guest, Member, Editor, Manager, Admin) against an endpoint, ensuring permissions are strictly enforced without duplicating test code.
</details>

<details>
<summary>7. What is OAuth2 Scope testing in FastAPI?</summary>
FastAPI's <code>Security(get_current_user, scopes=["items:write"])</code> verifies fine-grained OAuth2 scopes. Tests must verify that a token with <code>items:read</code> receives <code>HTTP 401/403</code> when attempting to access a route requiring <code>items:write</code>.
</details>

<details>
<summary>8. Why should you avoid using a global <code>dependency_overrides</code> mock for authentication across the entire test suite?</summary>
Overriding authentication globally bypasses the security layer entirely. While convenient for business logic tests, it prevents testing authentication failures, expired tokens, role boundaries, and IDOR vulnerabilities. Security tests must run against the real authentication dependencies with controlled test tokens.
</details>

---

## 12. Practical Exercises & Solutions

### Exercise 1 (Easy): Test Token Generator Utility
**Task**: Write a Python function `mint_jwt(sub: str, role: str, is_expired: bool = False) -> str` that produces a valid or expired JWT string for testing.
<details>
<summary>Solution</summary>

```python
import jwt
from datetime import datetime, timezone, timedelta

SECRET = "test-secret-key-12345"

def mint_jwt(sub: str, role: str, is_expired: bool = False) -> str:
    now = datetime.now(timezone.utc)
    exp = now - timedelta(minutes=5) if is_expired else now + timedelta(hours=1)
    payload = {"sub": sub, "role": role, "exp": exp, "iat": now}
    return jwt.encode(payload, SECRET, algorithm="HS256")
```
</details>

### Exercise 2 (Easy): Missing Bearer Header Test
**Task**: Write an integration test using `AsyncClient` verifying that a protected endpoint `/profile` returns HTTP 401 when no `Authorization` header is provided.
<details>
<summary>Solution</summary>

```python
import pytest
from httpx import AsyncClient, ASGITransport
from fastapi import FastAPI, Depends, HTTPException, status
from fastapi.security import HTTPBearer

app = FastAPI()
security = HTTPBearer()

@app.get("/profile")
async def profile(token = Depends(security)):
    return {"status": "ok"}

@pytest.mark.anyio
async def test_missing_auth_header():
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        resp = await client.get("/profile")
        assert resp.status_code == 403 or resp.status_code == 401
```
</details>

### Exercise 3 (Medium): Testing Expired Token Rejection
**Task**: Using `mint_jwt(..., is_expired=True)`, write a test asserting that passing an expired token to an endpoint returns HTTP 401 with detail `"Token has expired"`.
<details>
<summary>Solution</summary>

```python
import pytest
from httpx import AsyncClient, ASGITransport

@pytest.mark.anyio
async def test_expired_token_rejected():
    expired_token = mint_jwt("user_101", "member", is_expired=True)
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        resp = await client.get("/profile", headers={"Authorization": f"Bearer {expired_token}"})
        assert resp.status_code == 401
```
</details>

### Exercise 4 (Medium): Role Hierarchy Enforcement Test
**Task**: Build a parameterized test that verifies that an endpoint requiring role `"admin"` allows access to `"admin"`, but returns HTTP 403 for `"viewer"` and `"editor"`.
<details>
<summary>Solution</summary>

```python
import pytest
from httpx import AsyncClient, ASGITransport

@pytest.mark.anyio
@pytest.mark.parametrize("role, expected_status", [
    ("viewer", 403),
    ("editor", 403),
    ("admin", 200)
])
async def test_role_clearance(role, expected_status):
    token = mint_jwt("user_test", role=role)
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        resp = await client.get("/admin/metrics", headers={"Authorization": f"Bearer {token}"})
        assert resp.status_code == expected_status
```
</details>

### Exercise 5 (Hard): Complete BOLA / IDOR Isolation Test Suite
**Task**: Create an API with a private note storage endpoint `GET /notes/{note_id}`. In pytest, create Note 1 owned by User A. Assert that User A receives 200 OK, while User B receives 404 Not Found.
<details>
<summary>Solution</summary>

```python
import pytest
from httpx import AsyncClient, ASGITransport
from fastapi import FastAPI, Depends, HTTPException, status
from fastapi.security import HTTPBearer, HTTPAuthorizationCredentials
import jwt

app = FastAPI()
security = HTTPBearer()
notes_db = {"note_1": {"owner": "user_a", "content": "Confidential A"}}

def get_current_user(creds: HTTPAuthorizationCredentials = Depends(security)):
    payload = jwt.decode(creds.credentials, SECRET, algorithms=["HS256"])
    return payload["sub"]

@app.get("/notes/{note_id}")
async def get_note(note_id: str, user_id: str = Depends(get_current_user)):
    note = notes_db.get(note_id)
    if not note or note["owner"] != user_id:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="Note not found")
    return note

@pytest.mark.anyio
async def test_bola_isolation_between_tenants():
    token_a = mint_jwt("user_a", "member")
    token_b = mint_jwt("user_b", "member")
    
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        # Owner User A can read
        res_a = await client.get("/notes/note_1", headers={"Authorization": f"Bearer {token_a}"})
        assert res_a.status_code == 200
        assert res_a.json()["content"] == "Confidential A"
        
        # User B cannot read Note 1
        res_b = await client.get("/notes/note_1", headers={"Authorization": f"Bearer {token_b}"})
        assert res_b.status_code == 404
```
</details>

### Exercise 6 (Hard): OAuth2 Security Scopes Verification
**Task**: Implement and test a route requiring scope `analytics:write`. Assert that a token with only `analytics:read` receives HTTP 403, while a token with `analytics:write` receives HTTP 200.
<details>
<summary>Solution</summary>

```python
import pytest
from httpx import AsyncClient, ASGITransport
from fastapi import FastAPI, Security, HTTPException, status
from fastapi.security import SecurityScopes, HTTPBearer, HTTPAuthorizationCredentials
import jwt

app = FastAPI()
security = HTTPBearer()

def verify_scopes(security_scopes: SecurityScopes, creds: HTTPAuthorizationCredentials = Depends(security)):
    payload = jwt.decode(creds.credentials, SECRET, algorithms=["HS256"])
    token_scopes = payload.get("scopes", [])
    for scope in security_scopes.scopes:
        if scope not in token_scopes:
            raise HTTPException(
                status_code=status.HTTP_403_FORBIDDEN,
                detail=f"Missing required scope: {scope}"
            )
    return payload

@app.post("/analytics/report")
async def write_report(user = Security(verify_scopes, scopes=["analytics:write"])):
    return {"status": "written"}

@pytest.mark.anyio
async def test_oauth2_scope_enforcement():
    # Token with read-only scope
    read_token = jwt.encode({"sub": "user_1", "scopes": ["analytics:read"]}, SECRET, algorithm="HS256")
    # Token with write scope
    write_token = jwt.encode({"sub": "user_2", "scopes": ["analytics:write"]}, SECRET, algorithm="HS256")
    
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        # Read-only token rejected
        res_fail = await client.post("/analytics/report", headers={"Authorization": f"Bearer {read_token}"})
        assert res_fail.status_code == 403
        
        # Write token accepted
        res_ok = await client.post("/analytics/report", headers={"Authorization": f"Bearer {write_token}"})
        assert res_ok.status_code == 200
```
</details>
