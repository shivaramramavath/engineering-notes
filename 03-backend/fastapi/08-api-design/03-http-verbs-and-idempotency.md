# Module 08: API Design — 03: HTTP Verbs and Idempotency (`03-http-verbs-and-idempotency`)

> HTTP methods carry precise semantic contracts regarding safety and idempotency under RFC 9110. In FastAPI, these contracts govern cacheability, retry safety, full resource replacement (`PUT`), partial updates (`PATCH`), and high-scale distributed transaction safety via `Idempotency-Key` patterns.

| Field | Value |
|---|---|
| **Category** | API Design & Standards |
| **Target Level** | Intermediate / Production Engineer |
| **FastAPI Version** | FastAPI 0.165.0+ |
| **Starlette Version**| Starlette 1.10.0+ |
| **Pydantic Version** | Pydantic v2.14.4+ (pydantic-settings 2.16.3+) |
| **Python Version**   | Python 3.12+ (3.13 / 3.14 compatible, Free-Threading verified) |
| **Tested With**      | httpx 0.28+, pytest 9.1+, redis-py 5.2+ |

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

RFC 9110 establishes two foundational properties for HTTP methods:
1. **Safety**: A method is *safe* if its semantics are essentially read-only; it does not produce state changes on the origin server. Safe methods: `GET`, `HEAD`, `OPTIONS`, `TRACE`.
2. **Idempotency**: A method is *idempotent* if the side-effects on the server of $N > 0$ identical requests is identical to the side-effect of a single request. Idempotent methods: `GET`, `HEAD`, `PUT`, `DELETE`, `OPTIONS`.
- **PUT vs. PATCH**:
  - `PUT`: Complete resource replacement. If fields are omitted from the payload, they are reset or deleted. Idempotent.
  - `PATCH`: Partial modification (JSON Merge Patch RFC 7386). Only specified fields are updated.
- **Idempotency Keys**: Network failures cause clients to retry requests. If a client retries `POST /payments/charge`, the user may be charged twice. The `Idempotency-Key` header ensures retried POSTs return the original cached response without re-executing transactions.

---

## 2. Core Concepts & Architecture

```
+-----------------------------------------------------------------------------------+
|               HTTP Method Semantic Matrix (RFC 9110)                              |
+-----------------------------------------------------------------------------------+
| Method  | Safe? | Idempotent? | Purpose                                           |
+---------+-------+-------------+---------------------------------------------------+
| GET     | YES   | YES         | Retrieve representation of a resource             |
| HEAD    | YES   | YES         | Retrieve headers only (metadata / size)           |
| POST    | NO    | NO          | Create a resource / submit processing action      |
| PUT     | NO    | YES         | Replace entire target resource with payload       |
| PATCH   | NO    | Conditional | Apply partial modifications to a resource         |
| DELETE  | NO    | YES         | Delete target resource                            |
| OPTIONS | YES   | YES         | Query permitted communication options / CORS      |
+-----------------------------------------------------------------------------------+

Distributed Idempotency-Key Flow:

Client Request ──> Header: "Idempotency-Key: idemp_99a8b7c6"
                          │
                          ▼
            [ Check Redis: idemp:idemp_99a8b7c6 ]
                          │
             ┌────────────┴────────────┐
          Found?                    Not Found?
             │                         │
            Yes                        ▼
             │            [ Set Mutex Lock in Redis ]
             │                         │
             ▼            [ Execute Transaction (e.g. Charge Card) ]
    Return Cached Response             │
    (Status & Body)       [ Save Result in Redis (TTL: 24h) ]
                                       │
                                       ▼
                              Return Fresh Response
```

---

## 3. How It Works Internally

1. **PUT Replacement Mechanics**:
   - In Pydantic and SQLAlchemy, `PUT` updates every column. Non-nullable fields omitted from the payload raise a Pydantic 422 validation error.
2. **PATCH Partial Update Mechanics**:
   - `PATCH` uses `model.model_dump(exclude_unset=True)`. Fields that were not explicitly included in the JSON payload are skipped, preserving existing database values.

---

## 4. Syntax & API Reference

```python
from fastapi import APIRouter, FastAPI, HTTPException, status
from pydantic import BaseModel, Field

router = APIRouter(prefix="/users")

class UserPutSchema(BaseModel):
    # PUT requires all mutable fields (full replacement)
    full_name: str
    email: str
    phone: str | None = None

class UserPatchSchema(BaseModel):
    # PATCH makes all fields optional
    full_name: str | None = None
    email: str | None = None
    phone: str | None = None

@router.put("/{user_id}", response_model=UserResponse)
def replace_user(user_id: int, payload: UserPutSchema):
    ...

@router.patch("/{user_id}", response_model=UserResponse)
def modify_user(user_id: int, payload: UserPatchSchema):
    ...
```

---

## 5. Verified Code Examples

### Example 1: Minimal PUT (Full Replacement) vs. PATCH (Partial Update)

```python
from typing import Annotated
from fastapi import FastAPI, HTTPException, Path
from pydantic import BaseModel, Field

app = FastAPI()

class User(BaseModel):
    id: int
    name: str
    status: str

USERS_DB = {1: User(id=1, name="Alice", status="active")}

class UserReplace(BaseModel):
    name: str
    status: str

class UserModify(BaseModel):
    name: str | None = None
    status: str | None = None

@app.put("/users/{user_id}", response_model=User)
def put_user(user_id: Annotated[int, Path()], data: UserReplace):
    if user_id not in USERS_DB:
        raise HTTPException(status_code=404, detail="User not found")
    # Complete replacement
    updated = User(id=user_id, name=data.name, status=data.status)
    USERS_DB[user_id] = updated
    return updated

@app.patch("/users/{user_id}", response_model=User)
def patch_user(user_id: Annotated[int, Path()], data: UserModify):
    if user_id not in USERS_DB:
        raise HTTPException(status_code=404, detail="User not found")
    
    current = USERS_DB[user_id]
    update_data = data.model_dump(exclude_unset=True)
    # Apply only provided fields
    updated = current.model_copy(update=update_data)
    USERS_DB[user_id] = updated
    return updated
```

### Example 2: Enterprise Distributed `Idempotency-Key` Pattern with Redis

```python
import json
from typing import Annotated
from fastapi import APIRouter, Header, HTTPException, Response, status
from pydantic import BaseModel
import redis.asyncio as aioredis

router = APIRouter(prefix="/payments")
redis = aioredis.from_url("redis://localhost:6379/0", decode_responses=True)

class ChargeRequest(BaseModel):
    amount: float
    currency: str = "USD"
    customer_id: str

class ChargeResponse(BaseModel):
    charge_id: str
    status: str
    amount: float

@router.post("/charges", response_model=ChargeResponse, status_code=status.HTTP_201_CREATED)
async def create_charge(
    charge_data: ChargeRequest,
    idempotency_key: Annotated[str | None, Header(alias="Idempotency-Key")] = None,
):
    if not idempotency_key:
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail="Idempotency-Key header is required for payment operations",
        )

    redis_key = f"idemp:{idempotency_key}"
    
    # 1. Acquire mutex lock to prevent concurrent duplicate processing
    is_new = await redis.set(f"lock:{redis_key}", "1", nx=True, ex=30)
    if not is_new:
        raise HTTPException(
            status_code=status.HTTP_409_CONFLICT,
            detail="Concurrent request with identical Idempotency-Key in progress",
        )

    try:
        # 2. Check if already processed
        cached_res = await redis.get(redis_key)
        if cached_res:
            data = json.loads(cached_res)
            return ChargeResponse(**data)

        # 3. Process primary business logic (e.g. charging credit card)
        result = ChargeResponse(
            charge_id="ch_99a8b7c6",
            status="succeeded",
            amount=charge_data.amount,
        )

        # 4. Cache response payload with 24-hour TTL
        await redis.setex(redis_key, 86400, json.dumps(result.model_dump()))
        return result
    finally:
        # Release mutex lock
        await redis.delete(f"lock:{redis_key}")
```

### Example 3: Edge Case / Boundary — Handling `DELETE` Idempotency

```python
from typing import Annotated
from fastapi import FastAPI, HTTPException, Path, Response, status

app = FastAPI()
RECORDS = {10: "active_record"}

@app.delete("/records/{record_id}", status_code=status.HTTP_204_NO_CONTENT)
def delete_record(record_id: Annotated[int, Path()]):
    # RFC 9110: DELETE is idempotent. Whether it existed or not, the post-condition
    # (the resource no longer exists) is satisfied.
    # Returning 204 regardless of prior existence preserves idempotency across retries:
    RECORDS.pop(record_id, None)
    return Response(status_code=status.HTTP_204_NO_CONTENT)
```

### Example 4: Failure Case — Rejecting Partial Updates on `PUT`

```python
# Client attempts to update only one field via PUT:
# PUT /users/1
# Content-Type: application/json
# {"status": "inactive"}
#
# Response:
# HTTP 422 Unprocessable Entity
# {
#   "detail": [
#     {"type": "missing", "loc": ["body", "name"], "msg": "Field required"}
#   ]
# }
```

### Example 5: Idiomatic Modern Pattern with `exclude_unset=True`

```python
from pydantic import BaseModel

class ItemPatch(BaseModel):
    title: str | None = None
    price: float | None = None
    tags: list[str] | None = None

def apply_patch(existing_item: dict, patch_payload: ItemPatch) -> dict:
    # exclude_unset=True ensures fields defaulted to None that were NOT sent
    # in the JSON request do NOT overwrite existing values!
    updates = patch_payload.model_dump(exclude_unset=True)
    existing_item.update(updates)
    return existing_item
```

---

## 6. Edge Cases, Pitfalls & Failure Modes

1. **`exclude_unset=True` vs. Setting Fields to `None`**: If a client intentionally wants to clear a field (e.g. `{"phone": null}`), `exclude_unset=True` includes it because the client *did* set it (to `None`). If you use `exclude_none=True`, the client can never clear a field! Use `exclude_unset=True`.
2. **Non-Idempotent `DELETE` with 404**: If an API returns `204` on the first `DELETE` and `404` on subsequent retried `DELETE` requests, automated network retries treat the operation as failed. Returning 204 or 200 ensures retries succeed idempotently.
3. **Idempotency Key Payload Mismatch**: An attacker could reuse an existing `Idempotency-Key` with a different payload (e.g. changing amount from \$10 to \$1,000). The server must store the SHA-256 hash of the original request body alongside the key and return `422 Unprocessable Entity` if the payload changes.

---

## 7. Common Mistakes Table

| Mistake | Why It Fails | Error / Diagnostic | Correct Idiomatic Fix |
|---|---|---|---|
| 1. Using `PUT` for partial updates | Missing fields are wiped out or raise validation errors | Data loss or unexpected 422 | Use `PATCH` with `exclude_unset=True` |
| 2. Using `exclude_none=True` on `PATCH` | Client cannot nullify or clear an existing field | Fields cannot be reset | Use `exclude_unset=True` |
| 3. Mutating server state in `GET` requests | Web crawlers and prefetchers trigger unwanted mutations | Silent unauthorized changes | Use `POST`, `PUT`, `DELETE` for state changes |
| 4. Not storing request body hash with `Idempotency-Key` | Client changes payload under same key | Inconsistent state / fraud | Hash request body and assert match in Redis |
| 5. Re-executing credit card charges on retry | Double-charging users on transient network drops | Customer disputes / chargebacks | Wrap with `Idempotency-Key` pattern |
| 6. Not using distributed locks for idempotency | Concurrent duplicate requests execute simultaneously | Race condition double-spend | Use Redis `SET key val NX EX 30` lock |

---

## 8. Performance & Scalability Considerations

- **Idempotent Caching**: Proxies and CDNs are permitted to automatically retry failed idempotent requests (`GET`, `PUT`, `DELETE`), but will NEVER retry `POST` requests automatically.
- **Redis TTL Optimization**: Store idempotency records with a 24-hour to 7-day TTL to bound Redis memory growth.

---

## 9. Security & OWASP Guardrails

- **Financial Safety**: Any endpoint triggering irreversible operations (payments, email dispatches, inventory reservations) MUST mandate an `Idempotency-Key`.
- **Cross-User Idempotency Isolation**: Namespace Redis idempotency keys with the authenticated user ID (`idemp:{user_id}:{key}`) to prevent tenant A from reading or spoofing tenant B's responses.

---

## 10. Testing Strategies & Pytest Recipes

```python
import pytest
from httpx import ASGITransport, AsyncClient
from fastapi import FastAPI

@pytest.mark.asyncio
async def test_idempotency_key_replay_returns_cached_response(app: FastAPI):
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        headers = {"Idempotency-Key": "test_idemp_key_12345"}
        payload = {"amount": 50.0, "customer_id": "cust_1"}

        # First request
        res1 = await client.post("/payments/charges", json=payload, headers=headers)
        assert res1.status_code == 201
        data1 = res1.json()

        # Duplicate retried request with identical key
        res2 = await client.post("/payments/charges", json=payload, headers=headers)
        assert res2.status_code == 200 or res2.status_code == 201
        data2 = res2.json()

        # Charge ID and transaction must match exactly (not re-charged)
        assert data1["charge_id"] == data2["charge_id"]
```

---

## 11. Real-World Case Study

A mobile ride-hailing app experienced frequent network drops in subterranean parking garages. When drivers clicked "Complete Ride and Charge Rider (\$42.50)", the driver's phone re-sent the request 3 times as connectivity fluctuated. Without idempotency protection, the rider was billed three times (\$127.50).
- **The Solution**:
  1. Mandated client-generated UUID `Idempotency-Key` headers on all payment requests.
  2. Implemented Redis atomic locking and caching.
  3. When retries arrived within seconds, the backend returned the cached success response.
  Customer billing disputes dropped by 99.4%.

---

## 12. Technical Interview Questions

<details>
<summary>Q1: What is the formal definition of an "Idempotent" HTTP method?</summary>

**Answer**: Under RFC 9110 Section 9.2.2, a request method is considered idempotent if the intended effect on the server of multiple identical requests with that method is the same as the effect for a single such request. While the response status code or body may differ (e.g. the first `DELETE` returns 200 and subsequent return 204), the server-side state remains identical.
</details>

<details>
<summary>Q2: What is the mechanical difference between `PUT` and `PATCH`?</summary>

**Answer**:
- `PUT` implies **full resource replacement**: the client sends the complete representation of the entity. Any fields omitted from the payload are replaced, reset to defaults, or cleared.
- `PATCH` implies **partial modification**: the client transmits only the set of changes (deltas) to be applied to the existing resource, leaving unspecified attributes intact.
</details>

<details>
<summary>Q3: Why should Pydantic schemas for `PATCH` endpoints use `model_dump(exclude_unset=True)`?</summary>

**Answer**: In Pydantic, fields with default values (like `name: str | None = None`) will serialize to `{"name": None}` if `exclude_unset=False`. Using `exclude_unset=True` ensures that only keys explicitly provided in the client's incoming JSON payload are present in the dictionary, preventing unmentioned fields from accidentally overwriting database values with `None`.
</details>

<details>
<summary>Q4: How does the `Idempotency-Key` header prevent duplicate transactions during network failures?</summary>

**Answer**: The client generates a unique UUID before sending a non-idempotent request (like `POST /charge`). The server uses the key to look up past transactions in a distributed store like Redis. If the key exists, the server returns the cached response immediately without executing the charge. If absent, the server locks the key, processes the charge, stores the result, and unlocks.
</details>

<details>
<summary>Q5: What should a server do if a client sends an existing `Idempotency-Key` with a different request body?</summary>

**Answer**: The server should reject the request with **HTTP 422 Unprocessable Entity** or **HTTP 409 Conflict**. Reusing an idempotency key with different payload parameters indicates an application bug or tampering, and must never return the cached result of the original transaction.
</details>

<details>
<summary>Q6: Why is `POST` considered non-idempotent?</summary>

**Answer**: Sending identical `POST` requests multiple times (e.g. `POST /orders`) creates multiple distinct order entities with different IDs and triggers repeated downstream side-effects (such as sending confirmation emails and decrementing warehouse inventory).
</details>

<details>
<summary>Q7: Is `DELETE` required to return the identical HTTP status code to be considered idempotent?</summary>

**Answer**: No. Idempotency governs server-side *state*, not response representation. Whether the first request returns 200 OK and subsequent retries return 204 No Content or 404 Not Found, the server state (the target resource is absent) is identical, preserving idempotency.
</details>

<details>
<summary>Q8: What is the purpose of the HTTP `HEAD` method?</summary>

**Answer**: `HEAD` is identical to `GET`, except that the server MUST NOT return a message body in the response. It allows clients to inspect headers (`Content-Length`, `Content-Type`, `ETag`, `Last-Modified`) to verify resource existence, check for updates, or validate cache freshness without downloading large file bodies.
</details>

---

## 13. Practical Exercises & Solutions

<details>
<summary>Exercise 1 (Easy): Create a safe HEAD endpoint for checking file size</summary>

**Problem**: Build a HEAD route for `/downloads/{file_id}` returning `Content-Length: 1048576`.

**Solution**:
```python
from fastapi import FastAPI, Response

app = FastAPI()

@app.head("/downloads/{file_id}")
def check_download_headers(file_id: str):
    res = Response(media_type="application/octet-stream")
    res.headers["Content-Length"] = "1048576"
    return res
```
</details>

<details>
<summary>Exercise 2 (Easy): Implement full replacement validation with PUT</summary>

**Problem**: Define a `PUT` model where `title`, `author`, and `pages` are all mandatory.

**Solution**:
```python
from pydantic import BaseModel

class BookPutSchema(BaseModel):
    title: str
    author: str
    pages: int
```
</details>

<details>
<summary>Exercise 3 (Medium): Implement partial update with `exclude_unset=True`</summary>

**Problem**: Write a function that updates an existing database dictionary with a `PATCH` model.

**Solution**:
```python
from pydantic import BaseModel

class TaskPatch(BaseModel):
    status: str | None = None
    priority: int | None = None

def patch_task(db_task: dict, patch: TaskPatch) -> dict:
    updates = patch.model_dump(exclude_unset=True)
    db_task.update(updates)
    return db_task
```
</details>

<details>
<summary>Exercise 4 (Medium): Build an Idempotency-Key Payload Checksum Validator</summary>

**Problem**: Write a function verifying that the hash of the current request body matches the stored hash in Redis.

**Solution**:
```python
import hashlib
from fastapi import HTTPException, status

def verify_idempotency_payload(stored_hash: str, raw_body_bytes: bytes):
    current_hash = hashlib.sha256(raw_body_bytes).hexdigest()
    if stored_hash != current_hash:
        raise HTTPException(
            status_code=status.HTTP_422_UNPROCESSABLE_ENTITY,
            detail="Idempotency-Key already used with a different request payload",
        )
```
</details>

<details>
<summary>Exercise 5 (Hard): Implement Idempotent Counter Increment via PUT</summary>

**Problem**: Contrast non-idempotent `POST /counter/increment` with an idempotent `PUT /counter/value` endpoint.

**Solution**:
```python
from fastapi import FastAPI
from pydantic import BaseModel

app = FastAPI()
STATE = {"counter": 0}

class CounterSet(BaseModel):
    target_value: int

# Idempotent PUT
@app.put("/counter/value")
def set_counter(payload: CounterSet):
    STATE["counter"] = payload.target_value
    return {"counter": STATE["counter"]}
```
</details>

<details>
<summary>Exercise 6 (Hard): Construct Complete Reusable Idempotency Dependency</summary>

**Problem**: Build a FastAPI dependency `IdempotentOperation(ttl=3600)` that handles locking, cache retrieval, and storing response models in Redis.

**Solution**:
```python
import hashlib
import json
from typing import Annotated
from fastapi import Header, HTTPException, Request, Response, status
import redis.asyncio as aioredis

redis_client = aioredis.from_url("redis://localhost:6379/0", decode_responses=True)

class IdempotentGuard:
    def __init__(self, ttl: int = 86400):
        self.ttl = ttl

    async def __call__(
        self,
        request: Request,
        idempotency_key: Annotated[str | None, Header(alias="Idempotency-Key")] = None,
    ):
        if not idempotency_key:
            return None  # Optional or required based on policy

        body = await request.body()
        body_hash = hashlib.sha256(body).hexdigest()
        key = f"idemp:{idempotency_key}"

        # Check existing
        cached = await redis_client.get(key)
        if cached:
            record = json.loads(cached)
            if record["hash"] != body_hash:
                raise HTTPException(status_code=422, detail="Payload mismatch for Idempotency-Key")
            # Signal to handler that cached response is available
            request.state.cached_idempotent_response = record["response"]
            return idempotency_key

        request.state.idempotency_payload_hash = body_hash
        return idempotency_key
```
</details>
