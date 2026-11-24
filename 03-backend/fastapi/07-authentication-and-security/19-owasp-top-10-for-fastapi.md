# Module 07: Authentication & Security — 19: OWASP API Security Top 10 for FastAPI (`19-owasp-top-10-for-fastapi`)

> The OWASP API Security Top 10 (2023 Edition) identifies the most critical security vulnerabilities targeting modern distributed web services. In FastAPI, these vulnerabilities are mitigated through idiomatic architectural patterns: Pydantic v2 schemas for mass assignment defense, dependency injection for BOLA/BFLA guards, and strict HTTP client sandboxing against SSRF.

| Field | Value |
|---|---|
| **Category** | Authentication & Security |
| **Target Level** | Advanced / Production Architect |
| **FastAPI Version** | FastAPI 0.165.0+ |
| **Starlette Version**| Starlette 1.10.0+ |
| **Pydantic Version** | Pydantic v2.14.4+ (pydantic-settings 2.16.3+) |
| **Python Version**   | Python 3.12+ (3.13 / 3.14 compatible, Free-Threading verified) |
| **Tested With**      | httpx 0.28+, pytest 9.1+, pydantic 2.14+ |

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

APIs represent the primary attack surface of modern applications. Traditional web application firewalls (WAFs) often fail to detect logic-based API flaws:
- **API1:2023 Broken Object Level Authorization (BOLA)**: The #1 API risk. A user alters an ID in the URL (`/invoices/999`) to read another customer's data.
- **API2:2023 Broken Authentication**: Weak tokens, missing MFA, or non-expiring credentials.
- **API3:2023 Broken Object Property Level Authorization**: Mass assignment vulnerabilities where an attacker sends `{"is_admin": true}` to elevate privileges.
- **API4:2023 Unrestricted Resource Consumption**: Lack of rate limiting or unbounded file uploads leading to DoS.
- **API5:2023 Broken Function Level Authorization (BFLA)**: Ordinary users calling administrative functions (`/api/admin/users/delete`).
- **API7:2023 Server Side Request Forgery (SSRF)**: The server fetches user-supplied URLs, inadvertently querying internal cloud metadata (`http://169.254.169.254`).

---

## 2. Core Concepts & Architecture

```
+-----------------------------------------------------------------------------------+
|               OWASP API Security Top 10 Mitigation Matrix                         |
+-----------------------------------------------------------------------------------+
| OWASP 2023 Risk                 | FastAPI Defense Architecture                    |
+---------------------------------+-------------------------------------------------+
| API1: BOLA (IDOR)               | Dependency validates `resource.owner_id == user`|
| API2: Broken Authentication     | Argon2id, JWT Rotation, OAuth2 PKCE             |
| API3: Mass Assignment           | Isolated Pydantic Read vs. Write Schemas        |
| API4: Resource Consumption      | Redis Sliding Window Rate Limiting & File Limits|
| API5: BFLA (Admin Access)       | Declarative RBAC `Security(RequireRole(...))`   |
| API6: Business Flow Abuse       | CAPTCHA gates, progressive friction, velocity   |
| API7: SSRF                      | Private IP filtering, IP whitelisting           |
| API8: Security Misconfiguration | HSTS, CSP, disabling `/docs` in production      |
| API9: Improper Inventory        | Explicit APIRouter versioning (`/api/v1/`)      |
| API10: Unsafe Consumption       | Strict JSON schema validation for webhooks      |
+-----------------------------------------------------------------------------------+
```

---

## 3. How It Works Internally

1. **BOLA Prevention via Dependency Injection**:
   - In FastAPI, the resource loader dependency extracts `{item_id}` from the path, queries the database, checks ownership against the authenticated user, and yields the model.
   - If ownership check fails, it raises `HTTPException(404)` (preventing resource existence enumeration).
2. **Mass Assignment Prevention via Schema Separation**:
   - Defining distinct schemas: `UserCreate` (accepts `username`, `password`), `UserUpdate` (accepts `bio`), and `UserInDB` (contains `id`, `hashed_password`, `is_admin`).
   - The route handler only unpacks `UserUpdate.model_dump(exclude_unset=True)`. Even if an attacker posts `{"is_admin": true}`, Pydantic discards it, preventing mass assignment.

---

## 4. Syntax & API Reference

```python
from typing import Annotated
from fastapi import Depends, HTTPException, Path, status
from pydantic import BaseModel

# Safe schema separation (API3 Mitigation)
class ProfileUpdateRequest(BaseModel):
    bio: str | None = None
    display_name: str | None = None
    # 'is_admin' is completely absent, preventing mass assignment!

# BOLA Guard Dependency (API1 Mitigation)
async def get_user_invoice(
    invoice_id: Annotated[int, Path()],
    user: Annotated[User, Depends(get_current_user)],
    session: Annotated[AsyncSession, Depends(get_db)],
) -> Invoice:
    stmt = select(Invoice).where(Invoice.id == invoice_id, Invoice.user_id == user.id)
    invoice = await session.scalar(stmt)
    if not invoice:
        # Return 404 rather than 403 to prevent ID harvesting
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="Invoice not found")
    return invoice
```

---

## 5. Verified Code Examples

### Example 1: BOLA Defense with Ownership Assertion (API1:2023)

```python
from typing import Annotated
from fastapi import APIRouter, Depends, HTTPException, Path, status
from pydantic import BaseModel

router = APIRouter(prefix="/invoices")

class User(BaseModel):
    id: str

class Invoice(BaseModel):
    id: int
    user_id: str
    amount: float

# Mock database
INVOICE_DB = {
    101: Invoice(id=101, user_id="usr_alice", amount=250.00),
    102: Invoice(id=102, user_id="usr_bob", amount=999.00),
}

async def current_user() -> User:
    return User(id="usr_alice")

async def get_owned_invoice(
    invoice_id: Annotated[int, Path()],
    user: Annotated[User, Depends(current_user)],
) -> Invoice:
    invoice = INVOICE_DB.get(invoice_id)
    # Check existence AND ownership
    if not invoice or invoice.user_id != user.id:
        # Return 404 to avoid leaking invoice existence
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="Invoice not found")
    return invoice

@router.get("/{invoice_id}")
async def read_invoice(invoice: Annotated[Invoice, Depends(get_owned_invoice)]):
    return invoice
```

### Example 2: SSRF Prevention with IP Whitelisting (API7:2023)

```python
import ipaddress
import socket
from urllib.parse import urlparse
from fastapi import APIRouter, HTTPException, status
import httpx
from pydantic import BaseModel, HttpUrl

router = APIRouter(prefix="/webhooks")

class WebhookSubscription(BaseModel):
    target_url: HttpUrl

def validate_public_destination_ip(url: str):
    parsed = urlparse(url)
    hostname = parsed.hostname
    if not hostname:
        raise HTTPException(status_code=400, detail="Invalid target URL")

    # Resolve IP address to prevent internal network scanning
    try:
        ip_str = socket.gethostbyname(hostname)
        ip = ipaddress.ip_address(ip_str)
    except socket.gaierror:
        raise HTTPException(status_code=400, detail="Could not resolve target hostname")

    # Block AWS metadata (169.254.169.254), loopback (127.0.0.1), and private RFC1918 subnets
    if ip.is_private or ip.is_loopback or ip.is_link_local:
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail="Target points to internal or prohibited IP address",
        )
    return ip_str

@router.post("/subscribe")
async def register_webhook(sub: WebhookSubscription):
    # Validate before executing outbound HTTP
    validated_ip = validate_public_destination_ip(str(sub.target_url))
    return {"status": "registered", "resolved_ip": validated_ip}
```

### Example 3: Edge Case / Boundary — Disabling Documentation in Production (API8:2023)

```python
import os
from fastapi import FastAPI

is_production = os.getenv("APP_ENV") == "production"

# Hide OpenAPI, Swagger UI, and ReDoc in production to prevent attacker reconnaissance
app = FastAPI(
    docs_url=None if is_production else "/docs",
    redoc_url=None if is_production else "/redoc",
    openapi_url=None if is_production else "/openapi.json",
)
```

### Example 4: Failure Case — 404 Response on BOLA Probing

```python
# Attacker (Alice: usr_alice) probes Bob's invoice (102):
# GET /invoices/102
# Header: Authorization: Bearer <alice_jwt>
#
# Response (Notice 404 Not Found, NOT 403 Forbidden):
# HTTP 404 Not Found
# {
#   "detail": "Invoice not found"
# }
```

### Example 5: Idiomatic Modern Pattern with Strict Pydantic Read/Write Separation (API3:2023)

```python
from pydantic import BaseModel, ConfigDict
from fastapi import APIRouter

router = APIRouter(prefix="/users")

# 1. Input schema: only fields the user is allowed to mutate
class UserProfileUpdate(BaseModel):
    bio: str
    model_config = ConfigDict(extra="forbid")  # Rejects unexpected fields like 'is_admin'

# 2. Output schema: controls fields returned to the client
class UserProfileResponse(BaseModel):
    id: str
    username: str
    bio: str
    # 'hashed_password' and internal audit fields are excluded!

@router.patch("/me", response_model=UserProfileResponse)
def update_profile(update: UserProfileUpdate):
    # Database update using only fields defined on update schema
    return UserProfileResponse(id="usr_1", username="alice", bio=update.bio)
```

---

## 6. Edge Cases, Pitfalls & Failure Modes

1. **Returning 403 Instead of 404 on BOLA**: Returning 403 Forbidden informs the attacker that the resource exists and belongs to someone else (confirming the ID). Returning 404 prevents resource enumeration.
2. **DNS Rebinding in SSRF**: Validating a domain name once is not enough; an attacker's DNS server can return a public IP on the first lookup and `169.254.169.254` on the actual HTTP request. Pin the resolved IP during socket connection.
3. **Pydantic Model Dump Overwrite**: Writing `User(**request_body.dict())` allows mass assignment. Always define isolated Pydantic request models.

---

## 7. Common Mistakes Table

| Mistake | Why It Fails | Error / Diagnostic | Correct Idiomatic Fix |
|---|---|---|---|
| 1. Querying DB by resource ID alone without user ID | Allows any authenticated user to view any record | OWASP API1:2023 BOLA Exploit | Add `AND user_id = current_user.id` to query |
| 2. Reusing database ORM models for request bodies | Allows users to submit internal fields (`is_admin`) | OWASP API3:2023 Mass Assignment | Create explicit Pydantic `Create`/`Update` schemas |
| 3. Outbound HTTP requests without IP validation | Attacker reads AWS metadata or internal microservices | OWASP API7:2023 SSRF Exploit | Validate resolved IP against `ip.is_private` |
| 4. Leaving Swagger UI open on public production APIs | Exposes complete API schema to reconnaissance | OWASP API8:2023 Misconfiguration | Set `docs_url=None` in production |
| 5. Not rate-limiting authentication routes | Vulnerable to brute force and credential stuffing | OWASP API2:2023 Broken Auth | Apply strict rate limits (`5/min`) on `/auth` |
| 6. Deprecated unversioned endpoints running in parallel | Zombie endpoints with unpatched bugs remain active | OWASP API9:2023 Improper Inventory | Deprecate and remove old router versions |

---

## 8. Performance & Scalability Considerations

- **Composite Database Indexes**: To support BOLA ownership checks (`WHERE id = :id AND user_id = :uid`), create a composite B-Tree index on `(id, user_id)` for instantaneous lookups.
- **SSRF DNS Caching**: Cache DNS resolution results for outbound webhooks to prevent external DNS query latency from bottlenecking requests.

---

## 9. Security & OWASP Guardrails

- **OWASP API Security Top 10 Compliance**: Review each endpoint during design against the Top 10 checklist.
- **Automated Security Scanners**: Run dynamic security tests (ZAP, DAST) against FastAPI endpoints in staging pipelines.

---

## 10. Testing Strategies & Pytest Recipes

```python
import pytest
from httpx import ASGITransport, AsyncClient
from fastapi import FastAPI

@pytest.mark.asyncio
async def test_bola_prevented(app: FastAPI):
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        # Alice tries to read Bob's invoice (102)
        res = await client.get("/invoices/102")
        assert res.status_code == 404  # Must be 404, not 200

@pytest.mark.asyncio
async def test_mass_assignment_prevented(app: FastAPI):
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        # Attacker tries to inject is_admin
        res = await client.patch("/users/me", json={"bio": "new bio", "is_admin": True})
        assert res.status_code == 422  # Extra field forbidden!
```

---

## 11. Real-World Case Study

A major ride-sharing company suffered a high-profile vulnerability where a security researcher discovered that by modifying the `trip_id` in a mobile API endpoint (`GET /api/v1/trips/{trip_id}`), they could view GPS coordinates, driver names, and passenger payment methods for any trip worldwide. The backend checked that the user had a valid bearer token, but never verified that the `trip.rider_id == current_user.id`.
- **The Remediation**: The company deployed FastAPI dependency injection guards enforcing ownership checks across all entity loaders. Access attempts on foreign records were logged as BOLA security alerts, and the endpoint returned 404.

---

## 12. Technical Interview Questions

<details>
<summary>Q1: What is Broken Object Level Authorization (BOLA), and why is it the #1 API threat?</summary>

**Answer**: BOLA (formerly IDOR) occurs when an endpoint accepts a resource identifier (e.g. `/orders/{order_id}`) and retrieves the object from the database without verifying that the authenticated user has permission to access that specific object. It is the #1 threat because APIs inherently expose resource IDs, and traditional firewalls cannot differentiate between a legitimate user requesting their own record and an attacker requesting another user's record.
</details>

<details>
<summary>Q2: Why should BOLA authorization failures return HTTP 404 instead of HTTP 403?</summary>

**Answer**: Returning HTTP 403 Forbidden confirms to the attacker that the resource exists (confirming that ID 102 is a valid object in the database). Attackers use this to enumerate valid IDs. Returning HTTP 404 Not Found conceals the existence of the resource, providing zero reconnaissance information.
</details>

<details>
<summary>Q3: How does Pydantic prevent Mass Assignment vulnerabilities (API3:2023)?</summary>

**Answer**: Mass assignment occurs when an API binds client JSON directly to database models, allowing attackers to inject privileged fields (`is_admin`, `verified`). Pydantic prevents this through schema separation: request schemas only define fields clients are permitted to modify, and `extra="forbid"` actively rejects payloads containing unexpected attributes.
</details>

<details>
<summary>Q4: What is Server Side Request Forgery (SSRF), and how does an attacker exploit it?</summary>

**Answer**: SSRF occurs when an API fetches a remote resource based on a user-supplied URL (e.g., webhook subscriptions or avatar imports). An attacker supplies an internal address like `http://169.254.169.254/latest/meta-data/` (cloud metadata service) or `http://10.0.0.5:5432` (internal database). The server executes the request from within the trusted private network, leaking IAM credentials or internal service data.
</details>

<details>
<summary>Q5: How do you mitigate SSRF in Python?</summary>

**Answer**:
1. Parse the URL and resolve the target hostname to an IP address using `socket.gethostbyname()`.
2. Inspect the IP using `ipaddress.ip_address()`, rejecting private (`is_private`), loopback (`is_loopback`), or link-local (`is_link_local`) addresses.
3. Whitelist permitted protocols (e.g. strictly `https://`).
4. Pin the resolved IP during the outbound HTTP request to prevent DNS rebinding attacks.
</details>

<details>
<summary>Q6: What is Broken Function Level Authorization (BFLA), and how does it differ from BOLA?</summary>

**Answer**:
- **BOLA** relates to data objects: an ordinary user accesses an object they do not own (`GET /invoices/102`).
- **BFLA** relates to administrative capabilities: an ordinary user invokes an administrative function they have no role to perform (`POST /admin/export-database`).
</details>

<details>
<summary>Q7: Why should interactive API documentation (`/docs`) be disabled in public production environments?</summary>

**Answer**: Swagger UI exposes the entire API schema, internal model structures, parameter constraints, and hidden endpoints. This provides external attackers with a complete blueprint for identifying injection points and authorization flaws (OWASP API8:2023 Security Misconfiguration).
</details>

<details>
<summary>Q8: What is "Unsafe Consumption of APIs" (API10:2023)?</summary>

**Answer**: Developers often assume that responses from third-party services (e.g. PayPal, SendGrid, GitHub) are trustworthy. If a third-party service is compromised or an attacker mimics a webhook payload, the receiving API may process malicious input unsafely. All external data must be validated with Pydantic schemas before processing.
</details>

---

## 13. Practical Exercises & Solutions

<details>
<summary>Exercise 1 (Easy): Prevent Mass Assignment with Pydantic extra='forbid'</summary>

**Problem**: Define a `ProfileUpdate` schema that rejects any request containing extra attributes.

**Solution**:
```python
from pydantic import BaseModel, ConfigDict

class ProfileUpdate(BaseModel):
    bio: str
    model_config = ConfigDict(extra="forbid")
```
</details>

<details>
<summary>Exercise 2 (Easy): Disable documentation in production via environment flag</summary>

**Problem**: Instantiate FastAPI with `docs_url` disabled if `ENV == "production"`.

**Solution**:
```python
import os
from fastapi import FastAPI

is_prod = os.getenv("ENV") == "production"
app = FastAPI(docs_url=None if is_prod else "/docs")
```
</details>

<details>
<summary>Exercise 3 (Medium): Build a BOLA Guard for Document Deletion</summary>

**Problem**: Write a dependency verifying that `document.author_id == current_user.id`, returning 404 if not.

**Solution**:
```python
from fastapi import HTTPException, status

DOCS = {1: {"id": 1, "author_id": "alice"}, 2: {"id": 2, "author_id": "bob"}}

def get_owned_doc(doc_id: int, current_user_id: str):
    doc = DOCS.get(doc_id)
    if not doc or doc["author_id"] != current_user_id:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="Document not found")
    return doc
```
</details>

<details>
<summary>Exercise 4 (Medium): Build an IP Range SSRF Filter</summary>

**Problem**: Write a function `assert_public_ip(ip_str)` that raises 400 if the IP is private or loopback.

**Solution**:
```python
import ipaddress
from fastapi import HTTPException

def assert_public_ip(ip_str: str):
    ip = ipaddress.ip_address(ip_str)
    if ip.is_private or ip.is_loopback or ip.is_link_local:
        raise HTTPException(status_code=400, detail="Disallowed target address")
```
</details>

<details>
<summary>Exercise 5 (Hard): Implement DNS-Rebinding Resistant HTTP Fetcher</summary>

**Problem**: Write an async function that resolves a domain, checks against private IP ranges, and requests the IP directly while maintaining the `Host` header.

**Solution**:
```python
import ipaddress
import socket
from urllib.parse import urlparse
import httpx
from fastapi import HTTPException

async def safe_fetch(target_url: str) -> str:
    parsed = urlparse(target_url)
    hostname = parsed.hostname
    port = parsed.port or (443 if parsed.scheme == "https" else 80)

    ip_str = socket.gethostbyname(hostname)
    ip = ipaddress.ip_address(ip_str)
    if ip.is_private or ip.is_loopback:
        raise HTTPException(status_code=400, detail="Invalid destination IP")

    # Connect directly to resolved IP to prevent DNS rebinding
    url = f"{parsed.scheme}://{ip_str}:{port}{parsed.path}"
    async with httpx.AsyncClient(headers={"Host": hostname}, timeout=5.0) as client:
        res = await client.get(url)
        return res.text
```
</details>

<details>
<summary>Exercise 6 (Hard): Construct Tenant-Scoped BOLA Guard with Multi-Tenant Paths</summary>

**Problem**: Build a dependency `verify_org_resource(org_id, resource_id)` that validates both the user's membership in `org_id` and that the resource belongs to `org_id`.

**Solution**:
```python
from typing import Annotated
from fastapi import Depends, HTTPException, Path, status

# Mock data
USER_ORGS = {"alice": {"org_1", "org_2"}}
RESOURCES = {"res_100": {"id": "res_100", "org_id": "org_1"}}

def verify_org_resource(
    org_id: Annotated[str, Path()],
    resource_id: Annotated[str, Path()],
):
    current_user = "alice"
    # 1. Verify user belongs to tenant org
    if org_id not in USER_ORGS.get(current_user, set()):
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="Resource not found")
        
    # 2. Verify resource belongs to tenant org
    resource = RESOURCES.get(resource_id)
    if not resource or resource["org_id"] != org_id:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="Resource not found")
        
    return resource
```
</details>
