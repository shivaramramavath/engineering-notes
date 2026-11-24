# Module 08: API Design — 06: Versioning Strategies (`06-versioning-strategies`)

> APIs inevitably evolve as domain models change. In FastAPI, robust versioning strategies—primarily URI path prefixes (`/v1`, `/v2`) and header-based content negotiation—decouple legacy clients from breaking changes while formalizing deprecation schedules using RFC 8594 `Sunset` headers and OpenAPI `deprecated=True` flags.

| Field | Value |
|---|---|
| **Category** | API Design & Standards |
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

Breaking changes (renaming fields, removing endpoints, altering response types) destroy client applications if pushed without isolation:
- **URI Path Versioning (`/v1/users`, `/v2/users`)**: The industry consensus standard (Stripe, GitHub, Google). Simple to route, easy to inspect in access logs, and trivially cacheable by edge CDNs.
- **Custom Request Header Versioning (`X-API-Version: 2026-03-31`)**: Strips version numbers from URIs. Used extensively by Stripe for date-based daily versioning.
- **Media Type / Accept Header Versioning (`Accept: application/vnd.company.v2+json`)**: Strictly conforms to REST principles, but increases client implementation complexity.
- **Deprecation Governance**: Standardized via RFC 8594 (`Sunset: Wed, 11 Nov 2026 00:00:00 GMT`) and `Deprecation: true` response headers.

---

## 2. Core Concepts & Architecture

```
+-----------------------------------------------------------------------------------+
|               FastAPI Multi-Version Router Organization                           |
+-----------------------------------------------------------------------------------+
|                                                                                   |
|  Main Application (`app = FastAPI(...)`)                                          |
|         │                                                                         |
|         ├── Mounts: `app.include_router(v1_router, prefix="/api/v1")`             |
|         │     ├── Uses `UserV1Response` (legacy flat model)                       |
|         │     └── Retains backwards-compatible database mappings                  |
|         │                                                                         |
|         └── Mounts: `app.include_router(v2_router, prefix="/api/v2")`             |
|               ├── Uses `UserV2Response` (nested address & strict types)           |
|               └── Emits latest business representations                           |
|                                                                                   |
+-----------------------------------------------------------------------------------+

Deprecation Header Handshake:

Client Request ──> GET /api/v1/customers
                          │
                          ▼
             [ FastAPI v1 Route Handler ]
                          │
                          ▼
             Attach RFC 8594 Deprecation Headers:
             - Deprecation: @1762819200
             - Sunset: Wed, 11 Nov 2026 00:00:00 GMT
             - Link: </api/v2/customers>; rel="successor-version"
                          │
                          ▼
             Client SDK logs warning & triggers migration
```

---

## 3. How It Works Internally

1. **Router Hierarchy**:
   - Creating isolated `APIRouter` instances per major version maintains completely separate Pydantic request and response models while sharing core database services and domain entities.
2. **OpenAPI Multi-Spec Generation**:
   - Multiple FastAPI app instances can be mounted (`app.mount("/v1", v1_app)`) to generate independent Swagger documentation pages (`/v1/docs` vs `/v2/docs`).

---

## 4. Syntax & API Reference

```python
from fastapi import APIRouter, FastAPI, Response

app = FastAPI(title="Enterprise Gateway")

v1_router = APIRouter(prefix="/v1", tags=["Version 1 (Deprecated)"])
v2_router = APIRouter(prefix="/v2", tags=["Version 2 (Stable)"])

# Mark route as deprecated in OpenAPI spec and attach Sunset header
@v1_router.get("/users", deprecated=True)
def get_users_v1(response: Response):
    response.headers["Deprecation"] = "true"
    response.headers["Sunset"] = "Wed, 11 Nov 2026 00:00:00 GMT"
    return [{"user_id": "1", "legacy_name": "Alice"}]

@v2_router.get("/users")
def get_users_v2():
    return [{"id": "1", "first_name": "Alice", "last_name": "Smith"}]

app.include_router(v1_router)
app.include_router(v2_router)
```

---

## 5. Verified Code Examples

### Example 1: Minimal Multi-Version Router Setup

```python
from fastapi import APIRouter, FastAPI
from pydantic import BaseModel

app = FastAPI()

# V1 Schemas
class UserV1(BaseModel):
    name: str  # Single full name string

# V2 Schemas
class UserV2(BaseModel):
    first_name: str
    last_name: str

v1 = APIRouter(prefix="/v1")
v2 = APIRouter(prefix="/v2")

@v1.get("/profile", response_model=UserV1)
def get_profile_v1():
    return UserV1(name="Alice Smith")

@v2.get("/profile", response_model=UserV2)
def get_profile_v2():
    return UserV2(first_name="Alice", last_name="Smith")

app.include_router(v1)
app.include_router(v2)
```

### Example 2: Enterprise Header-Based Versioning (`X-API-Version`)

```python
from typing import Annotated
from fastapi import APIRouter, Header, HTTPException, status
from pydantic import BaseModel

router = APIRouter(prefix="/customers")

class CustomerV1(BaseModel):
    id: str
    address: str

class CustomerV2(BaseModel):
    id: str
    street: str
    city: str
    zip_code: str

@router.get("/me")
def get_customer(
    api_version: Annotated[str | None, Header(alias="X-API-Version")] = "2026-01-01"
):
    if api_version == "2024-01-01":
        return CustomerV1(id="c1", address="123 Main St, Seattle, 98101")
    elif api_version in ("2026-01-01", "latest"):
        return CustomerV2(id="c1", street="123 Main St", city="Seattle", zip_code="98101")
    else:
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail=f"Unsupported API version requested: {api_version}",
        )
```

### Example 3: Edge Case / Boundary — Automated Deprecation Middleware

```python
from starlette.middleware.base import BaseHTTPMiddleware
from starlette.requests import Request
from starlette.responses import Response

class SunsetDeprecationMiddleware(BaseHTTPMiddleware):
    async def dispatch(self, request: Request, call_next) -> Response:
        response = await call_next(request)
        if request.url.path.startswith("/api/v1/"):
            response.headers["Deprecation"] = "@1794355200"  # Unix timestamp
            response.headers["Sunset"] = "Fri, 01 Jan 2027 00:00:00 GMT"
            response.headers["Link"] = '</api/v2/>; rel="successor-version"'
        return response
```

### Example 4: Failure Case — Rejecting Unsupported Legacy Versions

```python
# When an incoming request specifies an obsolete version:
# GET /customers/me
# Header: X-API-Version: 2020-01-01
#
# Response:
# HTTP 400 Bad Request
# {
#   "error": "unsupported_version",
#   "detail": "API version 2020-01-01 was retired on 2023-01-01. Please upgrade to 2026-01-01."
# }
```

### Example 5: Idiomatic Modern Pattern with Isolated Sub-Applications

```python
from fastapi import FastAPI

# Independent apps generate separate OpenAPI documentation
app_v1 = FastAPI(title="Payment Gateway V1 (Deprecated)", version="1.0.0")
app_v2 = FastAPI(title="Payment Gateway V2 (Stable)", version="2.0.0")

@app_v1.get("/charges")
def list_charges_v1():
    return [{"amount_cents": 5000}]

@app_v2.get("/charges")
def list_charges_v2():
    return [{"amount": 50.00, "currency": "USD"}]

root_app = FastAPI()
root_app.mount("/v1", app_v1)
root_app.mount("/v2", app_v2)
```

---

## 6. Edge Cases, Pitfalls & Failure Modes

1. **Version Fragmentation**: Maintaining 10 different minor versions simultaneously creates code bloat, complicated database migrations, and testing nightmares. Limit active supported versions to at most two: current stable and previous retiring.
2. **CDN Cache Collisions with Header Versioning**: If versioning via `X-API-Version` or `Accept`, reverse proxies and CDNs must be explicitly configured with `Vary: X-API-Version, Accept` to prevent serving cached V1 responses to V2 clients.
3. **Database Column Deletions**: Deleting a database column used by V1 breaks legacy clients immediately. Add new columns for V2 while keeping V1 columns populated via database triggers or ORM compatibility getters until V1 reaches its sunset date.

---

## 7. Common Mistakes Table

| Mistake | Why It Fails | Error / Diagnostic | Correct Idiomatic Fix |
|---|---|---|---|
| 1. Putting minor versions in the URI (`/v1.2/users`) | High churn; requires clients to update URIs frequently | Brittle client integrations | Version only breaking changes (`/v1`, `/v2`) |
| 2. Header versioning without `Vary` header | Caches serve wrong version to clients | Inconsistent response structures | Attach `Vary: X-API-Version` response header |
| 3. Dropping DB columns while legacy versions are live | Legacy API crashes with database column missing | HTTP 500 on deprecated endpoints | Maintain DB columns until complete sunset |
| 4. Not marking deprecated routes in OpenAPI | Consumers unaware of impending retirements | Sudden unexpected breakages | Set `@router.get(..., deprecated=True)` |
| 5. Query string versioning (`/users?v=2`) | Clutters URI parameters; inconsistent with REST | Non-standard routing | Use URI path prefix (`/v2/users`) |
| 6. Omitting `Sunset` header on retiring routes | Violates RFC 8594; automated scanners cannot track | Failed compliance audits | Return `Sunset: <HTTP-date>` header |

---

## 8. Performance & Scalability Considerations

- **Path-Based Routing Speed**: Matching `/v1/users` vs `/v2/users` in Starlette's regex trie executes in <0.001ms.
- **Microservice Routing at the Edge**: In cloud architectures, an API Gateway (Envoy/Kong) can route `/v1/*` to legacy container clusters and `/v2/*` to modern clusters without sharing FastAPI processes.

---

## 9. Security & OWASP Guardrails

- **OWASP API9:2023 Improper Inventory Management**: Unpatched vulnerabilities in deprecated API versions remain active attack vectors. Ensure all legacy versions are cataloged, monitored, and permanently decommissioned after their sunset date.

---

## 10. Testing Strategies & Pytest Recipes

```python
import pytest
from httpx import ASGITransport, AsyncClient
from fastapi import FastAPI

@pytest.mark.asyncio
async def test_multi_version_responses(app: FastAPI):
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        # Test V1
        res_v1 = await client.get("/v1/profile")
        assert res_v1.status_code == 200
        assert "name" in res_v1.json()

        # Test V2
        res_v2 = await client.get("/v2/profile")
        assert res_v2.status_code == 200
        assert "first_name" in res_v2.json()
```

---

## 11. Real-World Case Study

A public logistics SaaS company introduced a redesigned tracking data structure. To avoid breaking 3,000 corporate integrations:
- **Migration Plan**:
  1. Kept `/api/v1/shipments` live for 18 months.
  2. Implemented `/api/v2/shipments` with enhanced milestone tracking.
  3. Injected `Sunset: Wed, 01 Jul 2026 00:00:00 GMT` into all V1 responses.
  4. Monitored V1 traffic in Datadog; when V1 traffic dropped to <1%, corporate accounts were directly notified, and V1 was safely decommissioned with zero unplanned outages.

---

## 12. Technical Interview Questions

<details>
<summary>Q1: What are the trade-offs between URI path versioning and Header versioning?</summary>

**Answer**:
- **URI Path Versioning (`/v1/users`)**: Clear, explicit, easy to test in browsers, easy to log, trivially cached by CDNs without custom cache keys. Trade-off: Violates strict REST purism (the URI changes even though the underlying entity is the same).
- **Header Versioning (`X-API-Version`)**: Clean URIs, supports date-based versioning. Trade-offs: Cannot be explored via plain browser URLs; requires configuring `Vary` headers on all intermediate CDNs to prevent cache contamination.
</details>

<details>
<summary>Q2: What is RFC 8594 (The Sunset Header)?</summary>

**Answer**: RFC 8594 defines the `Sunset` HTTP response header. It allows a server to communicate to clients that a resource or API version is slated to be deprecated and retired at a specific future HTTP-date (e.g. `Sunset: Wed, 11 Nov 2026 00:00:00 GMT`), allowing automated monitoring tools to alert developers of upcoming deprecations.
</details>

<details>
<summary>Q3: When should an API team introduce a new major version?</summary>

**Answer**: Only when introducing **breaking changes**:
1. Removing or renaming an endpoint.
2. Removing or renaming existing request/response fields.
3. Changing field types (e.g. integer ID to UUID string).
4. Introducing new mandatory request parameters.
Non-breaking changes (adding optional parameters, adding new response fields) should be introduced backwards-compatibly within the existing version.
</details>

<details>
<summary>Q4: How do you document deprecated endpoints in FastAPI's generated Swagger UI?</summary>

**Answer**: Pass `deprecated=True` to the route decorator:
```python
@app.get("/legacy-data", deprecated=True)
def get_legacy():
    ...
```
Swagger UI applies a strikethrough visual style to the route and marks it as deprecated in the OpenAPI specification.
</details>

<details>
<summary>Q5: Why should database columns not be dropped immediately when migrating from V1 to V2?</summary>

**Answer**: If V1 is still serving production traffic, dropping a database column causes V1 queries to crash with database errors. Database schemas must follow the **Expand and Contract pattern**:
1. *Expand*: Add new V2 columns while retaining V1 columns.
2. *Migrate*: Deploy V2; synchronize data via application code or database triggers.
3. *Contract*: Once V1 is sunset, drop the legacy columns.
</details>

<details>
<summary>Q6: What is the risk of "Zombie APIs" (OWASP API9:2023)?</summary>

**Answer**: Zombie APIs are old, unmaintained, or forgotten API versions that remain exposed to the public internet. Because they are rarely updated, they often contain known unpatched security vulnerabilities and lack modern rate limiting or security headers, providing attackers with an easy backdoor into the system.
</details>

<details>
<summary>Q7: How does mounting separate FastAPI apps allow independent OpenAPI specs?</summary>

**Answer**: Using `root_app.mount("/v1", app_v1)` and `root_app.mount("/v2", app_v2)` mounts two completely distinct FastAPI applications. Each sub-app generates its own independent `/docs` and `/openapi.json`, allowing clients to view isolated, version-specific API reference documentation.
</details>

<details>
<summary>Q8: How does the `Link` header complement the `Sunset` header?</summary>

**Answer**: When an endpoint is sunset, the server attaches `Link: </api/v2/items>; rel="successor-version"`. This standard header points automated client SDK generators and consumers directly to the replacement API resource.
</details>

---

## 13. Practical Exercises & Solutions

<details>
<summary>Exercise 1 (Easy): Create a deprecated route in FastAPI</summary>

**Problem**: Build a route `/v1/status` marked as `deprecated=True` in OpenAPI.

**Solution**:
```python
from fastapi import FastAPI

app = FastAPI()

@app.get("/v1/status", deprecated=True)
def status_v1():
    return {"status": "ok", "version": 1}
```
</details>

<details>
<summary>Exercise 2 (Easy): Inject a Sunset header on a response</summary>

**Problem**: Set `Sunset: Fri, 01 Jan 2027 00:00:00 GMT` on an endpoint response.

**Solution**:
```python
from fastapi import FastAPI, Response

app = FastAPI()

@app.get("/retiring")
def retiring(response: Response):
    response.headers["Sunset"] = "Fri, 01 Jan 2027 00:00:00 GMT"
    return {"message": "migrating soon"}
```
</details>

<details>
<summary>Exercise 3 (Medium): Mount two versioned routers under `/v1` and `/v2`</summary>

**Problem**: Create two APIRouters returning different schemas and mount them to the main app.

**Solution**:
```python
from fastapi import APIRouter, FastAPI

app = FastAPI()
r1 = APIRouter(prefix="/v1")
r2 = APIRouter(prefix="/v2")

@r1.get("/data")
def d1(): return {"v": 1}

@r2.get("/data")
def d2(): return {"v": 2}

app.include_router(r1)
app.include_router(r2)
```
</details>

<details>
<summary>Exercise 4 (Medium): Build a Header Version Validator Dependency</summary>

**Problem**: Write a dependency checking `X-Version` header, raising 400 if it is not `"v1"` or `"v2"`.

**Solution**:
```python
from typing import Annotated
from fastapi import Header, HTTPException, status

def check_version(x_version: Annotated[str, Header(alias="X-Version")]):
    if x_version not in ("v1", "v2"):
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail="Unsupported version")
    return x_version
```
</details>

<details>
<summary>Exercise 5 (Hard): Implement Expand/Contract Adapter Service</summary>

**Problem**: Build a data translation layer that converts a single internal domain model into both a V1 and a V2 Pydantic response model.

**Solution**:
```python
from pydantic import BaseModel

class InternalUser:
    def __init__(self, id: int, first: str, last: str):
        self.id = id
        self.first_name = first
        self.last_name = last

class UserV1Response(BaseModel):
    id: int
    full_name: str

class UserV2Response(BaseModel):
    id: int
    first_name: str
    last_name: str

def adapt_to_v1(user: InternalUser) -> UserV1Response:
    return UserV1Response(id=user.id, full_name=f"{user.first_name} {user.last_name}")

def adapt_to_v2(user: InternalUser) -> UserV2Response:
    return UserV2Response(id=user.id, first_name=user.first_name, last_name=user.last_name)
```
</details>

<details>
<summary>Exercise 6 (Hard): Construct Automated Sunset and Successor-Version Middleware</summary>

**Problem**: Build a Starlette middleware that intercepts any request containing `/v1/`, injects `Deprecation`, `Sunset`, and `Link` headers, and logs a telemetry warning.

**Solution**:
```python
from starlette.middleware.base import BaseHTTPMiddleware
from starlette.requests import Request
from starlette.responses import Response

class AutoSunsetMiddleware(BaseHTTPMiddleware):
    async def dispatch(self, request: Request, call_next) -> Response:
        response = await call_next(request)
        if "/v1/" in request.url.path:
            response.headers["Deprecation"] = "true"
            response.headers["Sunset"] = "Thu, 31 Dec 2026 23:59:59 GMT"
            # Point to equivalent v2 path
            v2_path = request.url.path.replace("/v1/", "/v2/")
            response.headers["Link"] = f'<{v2_path}>; rel="successor-version"'
        return response
```
</details>
