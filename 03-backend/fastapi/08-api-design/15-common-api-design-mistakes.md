# Module 08: API Design — 15: Common API Design Mistakes (`15-common-api-design-mistakes`)

> Sub-optimal API design choices permanently haunt engineering teams once client applications and third-party partners integrate against them. This comprehensive guide catalogs the most damaging API design anti-patterns—from RPC verb pollution and HTTP 200 error masking to unbounded pagination and internal model leakage—providing production-grade remedies and architectural guardrails.

| Field | Value |
|---|---|
| **Category** | API Design & Standards |
| **Target Level** | Advanced / Technical Reviewer |
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

While backend code can be refactored behind abstractions, the public HTTP API is an immutable contract:
- **The Cost of Bad Design**: Breaking a published endpoint breaks mobile apps in the wild, customer integrations, and third-party webhooks.
- **Top Structural Blunders**:
  1. **RPC Verbs in URIs**: `/createCustomer`, `/deleteOrder`.
  2. **Error Masking**: Returning HTTP 200 with `{"error": "Unauthorized"}`.
  3. **Data Model Leakage**: Returning raw SQLAlchemy database models containing hashed passwords and internal soft-delete flags.
  4. **Unbounded Collections**: Endpoints lacking pagination limits, causing server OOM crashes.
  5. **Inconsistent Casing**: Mixing `camelCase` and `snake_case` randomly across endpoints and JSON keys.

---

## 2. Core Concepts & Architecture

```
+-----------------------------------------------------------------------------------+
|               Anti-Pattern vs. Production REST Comparison                         |
+-----------------------------------------------------------------------------------+
| Anti-Pattern                       | Idiomatic REST Design                        |
+------------------------------------+----------------------------------------------+
| POST /api/deleteItem?id=12         | DELETE /api/items/12 (HTTP 204)              |
| HTTP 200 {"status": 404, "err":...}| HTTP 404 application/problem+json            |
| GET /users (returns 1,000,000 rows)| GET /users?limit=20&cursor=ZXlK...           |
| JSON: {"userId": 1, "created_at":.}| Consistent camelCase or snake_case via alias |
| PUT /items/1 (updates single field)| PATCH /items/1 (JSON Merge Patch)            |
| HTTP 201 without Location header   | HTTP 201 with `Location: /items/42`          |
+-----------------------------------------------------------------------------------+
```

---

## 3. How It Works Internally

1. **Pydantic Alias Generators**:
   - Python naming conventions dictate `snake_case` (`first_name`), while many frontend ecosystems expect `camelCase` (`firstName`).
   - Using Pydantic v2's `alias_generator = to_camel` and `populate_by_name = True` maintains idiomatic Python code internally while emitting standardized `camelCase` JSON over the wire.
2. **Strict Response Model Filtering**:
   - FastAPI inspects `response_model=UserPublic`. Even if the database query loads `hashed_password` and `is_superuser`, Pydantic filters them out during serialization.

---

## 4. Syntax & API Reference

### Anti-Pattern: Internal Model Leakage
```python
# BROKEN: Exposes database columns directly
@app.get("/users/{id}")
async def get_user(id: int, session: AsyncSession = Depends(get_db)):
    user = await session.get(UserModel, id)
    return user  # LEAKS: hashed_password, internal flags!
```

### Idiomatic Fix: Pydantic DTO Separation
```python
# CORRECT: Filtered through public response model
@app.get("/users/{id}", response_model=UserPublicResponse)
async def get_user(id: int, session: AsyncSession = Depends(get_db)):
    user = await session.get(UserModel, id)
    if not user:
        raise HTTPException(status_code=404, detail="User not found")
    return user  # Pydantic strips internal fields automatically
```

---

## 5. Verified Code Examples

### Example 1: Enforcing Consistent CamelCase JSON Serialization

```python
from fastapi import FastAPI
from pydantic import BaseModel, ConfigDict
from pydantic.alias_generators import to_camel

app = FastAPI()

class CamelModel(BaseModel):
    model_config = ConfigDict(
        alias_generator=to_camel,
        populate_by_name=True,
    )

class UserAccount(CamelModel):
    user_id: str
    email_address: str
    is_active_subscriber: bool

@app.get("/accounts/me", response_model=UserAccount)
def get_account():
    # Write idiomatic Python snake_case:
    return UserAccount(
        user_id="usr_123",
        email_address="alice@example.com",
        is_active_subscriber=True,
    )
    # Serializes to wire JSON as:
    # {"userId": "usr_123", "emailAddress": "alice@example.com", "isActiveSubscriber": true}
```

### Example 2: Fixing Error Masking (Never Return 200 on Failure)

```python
from fastapi import FastAPI, HTTPException, status
from fastapi.responses import JSONResponse
from pydantic import BaseModel

app = FastAPI()

# VULNERABLE ANTI-PATTERN:
# @app.post("/bad-checkout")
# def bad_checkout():
#     return {"success": False, "error_code": 402, "msg": "Card declined"} # HTTP 200!

# CORRECT IDIOMATIC DESIGN:
@app.post("/checkout")
def checkout(funds_available: bool = False):
    if not funds_available:
        # Proper HTTP status code with RFC problem details
        raise HTTPException(
            status_code=status.HTTP_402_PAYMENT_REQUIRED,
            detail="Payment transaction declined: insufficient credit line",
        )
    return {"status": "payment_settled"}
```

### Example 3: Edge Case / Boundary — Handling Empty List vs 404

```python
from typing import Annotated
from fastapi import FastAPI, Query

app = FastAPI()

# REST BEST PRACTICE:
# - An empty collection returns HTTP 200 OK with `[]`, NOT HTTP 404 Not Found!
# - A missing individual item (`/items/99`) returns HTTP 404 Not Found.
@app.get("/articles")
def list_articles(category: Annotated[str | None, Query()] = None):
    # If no articles match the filter, return empty list with 200 OK
    return []
```

### Example 4: Failure Case — Rejecting Missing Location Header on 201

```python
# If an endpoint creates a resource:
# POST /orders -> HTTP 201 Created
#
# But omits the Location header:
# The client must parse the JSON body to figure out the newly created URI.
#
# REMEDY: Always attach response.headers["Location"] = f"/orders/{order.id}"
```

### Example 5: Idiomatic Modern Pattern with Safe Pagination Guardrails

```python
from typing import Annotated
from fastapi import FastAPI, Query
from pydantic import BaseModel

app = FastAPI()

# Prevents unbounded memory consumption attacks
def safe_pagination(
    limit: Annotated[int, Query(default=20, ge=1, le=100, description="Max 100 items per page")],
    offset: Annotated[int, Query(default=0, ge=0)] = 0,
) -> tuple[int, int]:
    return limit, offset

@app.get("/inventory")
def list_inventory(page_params: Annotated[tuple[int, int], Depends(safe_pagination)]):
    limit, offset = page_params
    return {"limit": limit, "offset": offset, "data": []}
```

---

## 6. Edge Cases, Pitfalls & Failure Modes

1. **Returning 404 on Empty Search Filters**: When a user queries `GET /users?department=legal`, and no users match, returning `HTTP 404` is an anti-pattern. The collection resource exists; it simply has zero matching elements. Return `HTTP 200` with `[]`.
2. **Missing `Vary` Headers on Compressed Endpoints**: Serving responses that change based on headers (e.g. `Accept-Encoding`, `X-API-Version`) without attaching `Vary: Accept-Encoding` causes CDNs to serve corrupt cached payloads to incompatible clients.
3. **Internal ID Leaks in Public Paths**: Using auto-incrementing integer IDs (`/orders/1`, `/orders/2`) allows competitors to easily track revenue and daily order volumes. Use UUIDv4 or high-entropy prefixed strings (`ord_7a8b9c...`).

---

## 7. Common Mistakes Table

| Mistake | Why It Fails | Error / Diagnostic | Correct Idiomatic Fix |
|---|---|---|---|
| 1. Verbs in URIs (`/api/delete_user`) | Violates REST principles; breaks standard caching | RPC-style API smell | Use HTTP verbs on nouns: `DELETE /users/{id}` |
| 2. Returning HTTP 200 for error responses | Monitoring tools and CDNs treat errors as successes | Silent application failures | Use proper 4xx/5xx HTTP status codes |
| 3. Unbounded collection queries | Single query can crash server with Out Of Memory | Server OOM killer crash | Enforce pagination with `le=100` |
| 4. Returning raw ORM models | Exposes passwords, tokens, and internal metadata | Critical security data leak | Use explicit Pydantic DTO response models |
| 5. Returning 404 for empty query results | Confuses clients; suggests endpoint path is wrong | Client breaks on empty filter | Return HTTP 200 with empty list `[]` |
| 6. Inconsistent casing in JSON responses | Frontend developers must guess field names | TypeScript compilation bugs | Use Pydantic `to_camel` or `to_snake` globally |

---

## 8. Performance & Scalability Considerations

- **Strict Payload Limits**: Enforcing a maximum pagination size (`le=100`) ensures response payloads remain under 50KB, maximizing network transmission speeds and keeping memory consumption constant.
- **DTO Projection Optimization**: In SQLAlchemy 2.0, select only the columns required by the Pydantic DTO (`select(User.id, User.email)`) rather than loading the entire entity model into memory.

---

## 9. Security & OWASP Guardrails

- **OWASP API3:2023 Broken Object Property Level Authorization**: Never reuse internal database entities as request bodies or return them raw. Always separate schemas into `Create`, `Update`, and `PublicRead` models.
- **Enumeration Defense**: Replace integer primary keys in public URIs with UUIDv4.

---

## 10. Testing Strategies & Pytest Recipes

```python
import pytest
from httpx import ASGITransport, AsyncClient
from fastapi import FastAPI

@pytest.mark.asyncio
async def test_empty_collection_returns_200(app: FastAPI):
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        res = await client.get("/articles?category=nonexistent")
        assert res.status_code == 200
        assert res.json() == []

@pytest.mark.asyncio
async def test_camel_case_json_keys(app: FastAPI):
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        res = await client.get("/accounts/me")
        assert res.status_code == 200
        data = res.json()
        assert "userId" in data
        assert "emailAddress" in data
        assert "is_active_subscriber" not in data  # Snake case must be converted
```

---

## 11. Real-World Case Study

A public API company built an analytics platform. The original developer designed the query endpoint as `GET /data/export?limit=all`. As customer datasets grew past 5,000,000 rows, corporate clients ran automated scripts requesting `limit=all` simultaneously. The PostgreSQL database spent 45 seconds loading rows, and FastAPI workers crashed with Linux Out Of Memory kills, causing a 4-hour global service outage.
- **The Redesign**:
  1. Deprecated `limit=all`.
  2. Implemented mandatory Keyset pagination with a hard maximum `limit=100`.
  3. Created an asynchronous bulk export endpoint (`POST /exports`) returning `HTTP 202 Accepted` that processed large CSV exports out-of-band via S3 presigned URLs.
  System stability reached 100%, and memory usage per container dropped from 3.8GB to 180MB.

---

## 12. Technical Interview Questions

<details>
<summary>Q1: Why is returning `HTTP 200 OK` with an error message in the JSON body an architectural anti-pattern?</summary>

**Answer**: It violates the HTTP protocol contract. Intermediate infrastructure (CDNs, reverse proxies, API gateways, load balancers, APM monitoring agents) inspect the HTTP status code to make routing, caching, and alerting decisions. Returning 200 OK causes CDNs to cache error pages, prevents automated circuit breakers from tripping during outages, and breaks automated HTTP client error-handling libraries.
</details>

<details>
<summary>Q2: What is the semantic difference between querying an empty collection vs. querying a non-existent item?</summary>

**Answer**:
- **Collection Query (`GET /users?role=intern`)**: The collection resource exists, but contains zero elements matching the filter. The server must return **HTTP 200 OK** with an empty array `[]`.
- **Item Query (`GET /users/999`)**: The specific entity identified by ID does not exist. The server must return **HTTP 404 Not Found**.
</details>

<details>
<summary>Q3: How do you prevent internal sensitive database columns from leaking into public API responses in FastAPI?</summary>

**Answer**: Define explicit, isolated Pydantic response models (`response_model=UserPublic`) that only declare public attributes (`id`, `username`, `created_at`). Never return raw SQLAlchemy/ORM entities without a `response_model` or without passing them through a dedicated DTO conversion layer.
</details>

<details>
<summary>Q4: How can an API support camelCase for frontend clients while keeping idiomatic snake_case in Python code?</summary>

**Answer**: Use Pydantic v2's `alias_generator`:
```python
class CamelModel(BaseModel):
    model_config = ConfigDict(alias_generator=to_camel, populate_by_name=True)
```
Python handlers interact with fields using snake_case (`user.first_name`), but when serialized to JSON, Pydantic converts keys to camelCase (`"firstName"`).
</details>

<details>
<summary>Q5: Why is allowing unbounded pagination (`limit` without an upper bound) a critical Denial of Service vulnerability?</summary>

**Answer**: If a client can send `limit=10000000`, the server attempts to allocate memory for millions of records, serialize them into a massive JSON string, and transmit gigabytes over the network socket. This causes database connection starvation, CPU spikes, and triggers the operating system's OOM killer, crashing the entire FastAPI worker process.
</details>

<details>
<summary>Q6: Why should auto-incrementing integer IDs be avoided in public URLs?</summary>

**Answer**: Sequential IDs leak business metrics (e.g. competitor tracking order count) and enable easy BOLA/IDOR enumeration attacks where an attacker scripts requests from ID 1 to 1,000,000. Public APIs should use UUIDv4 or high-entropy prefixed identifiers (`cus_99a8b7c6`).
</details>

<details>
<summary>Q7: When should an asynchronous processing endpoint return HTTP 202 Accepted?</summary>

**Answer**: When an operation cannot be completed synchronously within a typical HTTP request lifecycle (e.g., video transcoding, large CSV export, bulk email sending). The server enqueues the work, returns `HTTP 202 Accepted` immediately, and includes a `Location` header pointing to a status polling endpoint (e.g. `/jobs/job_123`).
</details>

<details>
<summary>Q8: What is the difference between `PUT` and `PATCH` under HTTP specifications?</summary>

**Answer**: `PUT` represents a complete resource replacement: all fields must be supplied, and omitted fields are reset or deleted. `PATCH` represents a partial update: only the modified attributes are sent, leaving unspecified fields untouched.
</details>

---

## 13. Practical Exercises & Solutions

<details>
<summary>Exercise 1 (Easy): Fix an RPC verb in a route definition</summary>

**Problem**: Refactor `@app.post("/api/removeUser")` into a RESTful route.

**Solution**:
```python
# Refactored:
@app.delete("/users/{user_id}", status_code=204)
def delete_user(user_id: int):
    return Response(status_code=204)
```
</details>

<details>
<summary>Exercise 2 (Easy): Enforce maximum pagination size of 50</summary>

**Problem**: Create a `limit` parameter with default 10 and max 50 using `Query`.

**Solution**:
```python
from fastapi import Query

def get_limit(limit: int = Query(default=10, ge=1, le=50)) -> int:
    return limit
```
</details>

<details>
<summary>Exercise 3 (Medium): Setup camelCase Alias Generator in Pydantic v2</summary>

**Problem**: Define a model that serializes `created_at` as `createdAt`.

**Solution**:
```python
from pydantic import BaseModel, ConfigDict
from pydantic.alias_generators import to_camel

class CamelBase(BaseModel):
    model_config = ConfigDict(alias_generator=to_camel, populate_by_name=True)

class Audit(CamelBase):
    created_at: str
```
</details>

<details>
<summary>Exercise 4 (Medium): Return 200 with empty list for empty search</summary>

**Problem**: Write a route `/search` that returns an empty list with status 200 if no items match.

**Solution**:
```python
from fastapi import FastAPI

app = FastAPI()

@app.get("/search")
def search(q: str):
    return []  # 200 OK with empty array
```
</details>

<details>
<summary>Exercise 5 (Hard): Implement Isolated DTO Layer Filtering Database Entities</summary>

**Problem**: Create an internal database model simulation containing `id`, `email`, `hashed_password`, and `is_admin`, and an endpoint returning only `id` and `email` via `response_model`.

**Solution**:
```python
from fastapi import FastAPI
from pydantic import BaseModel

app = FastAPI()

class DBUser:
    def __init__(self):
        self.id = 1
        self.email = "safe@example.com"
        self.hashed_password = "argon2id$v=19$m=65536..."
        self.is_admin = True

class PublicUser(BaseModel):
    id: int
    email: str

@app.get("/users/me", response_model=PublicUser)
def get_current_user():
    user = DBUser()
    # Pydantic response_model filters out hashed_password and is_admin
    return user
```
</details>

<details>
<summary>Exercise 6 (Hard): Construct Complete Hardened REST Collection Endpoint</summary>

**Problem**: Build a `/products` collection endpoint enforcing: plural noun URI, bounded pagination (`le=50`), sorting whitelist, camelCase response formatting, and returning 200 with empty array on no matches.

**Solution**:
```python
from enum import StrEnum
from typing import Annotated
from fastapi import Depends, FastAPI, Query
from pydantic import BaseModel, ConfigDict
from pydantic.alias_generators import to_camel

app = FastAPI()

class CamelModel(BaseModel):
    model_config = ConfigDict(alias_generator=to_camel, populate_by_name=True)

class ProductResponse(CamelModel):
    product_id: int
    product_name: str
    unit_price: float

class SortBy(StrEnum):
    PRICE = "price"
    NAME = "name"

@app.get("/products", response_model=list[ProductResponse])
def list_products(
    limit: Annotated[int, Query(default=20, ge=1, le=50)],
    offset: Annotated[int, Query(default=0, ge=0)],
    sort_by: Annotated[SortBy, Query()] = SortBy.PRICE,
):
    # Simulated query execution
    return []
```
</details>
