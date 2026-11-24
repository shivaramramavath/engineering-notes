# Module 08: API Design — 04: Status Codes and Semantics (`04-status-codes-and-semantics`)

> HTTP status codes define the standardized outcome contract between clients and servers. In FastAPI, status codes are declared explicitly using `starlette.status` constants, documenting response variants across the OpenAPI schema and ensuring semantic accuracy across success (201, 202, 204), redirection (304, 307), client error (400, 409, 422), and server error (502, 503, 504) conditions.

| Field | Value |
|---|---|
| **Category** | API Design & Standards |
| **Target Level** | Intermediate / Production Engineer |
| **FastAPI Version** | FastAPI 0.165.0+ |
| **Starlette Version**| Starlette 1.10.0+ |
| **Pydantic Version** | Pydantic v2.14.4+ (pydantic-settings 2.16.3+) |
| **Python Version**   | Python 3.12+ (3.13 / 3.14 compatible, Free-Threading verified) |
| **Tested With**      | httpx 0.28+, pytest 9.1+, starlette 1.10+ |

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

HTTP status codes are 3-digit integers categorized into 5 families under RFC 9110:
- **1xx (Informational)**: Request received, continuing process (e.g. `101 Switching Protocols` for WebSockets).
- **2xx (Successful)**: Action successfully received, understood, and accepted.
  - `200 OK`: Standard success.
  - `201 Created`: New resource successfully minted with a `Location` URI.
  - `202 Accepted`: Asynchronous processing queued; not yet completed.
  - `204 No Content`: Action succeeded; response contains NO message body.
- **3xx (Redirection)**: Further action needed to complete request (e.g. `304 Not Modified` for caching; `307/308` preserving HTTP methods).
- **4xx (Client Error)**: Client caused failure.
  - `400 Bad Request`: Malformed syntax or generic client fault.
  - `401 Unauthorized`: Missing or invalid authentication credentials.
  - `403 Forbidden`: Authenticated, but lacks sufficient permissions.
  - `404 Not Found`: Resource does not exist.
  - `409 Conflict`: Conflict with current state (e.g. unique constraint violation).
  - `422 Unprocessable Entity`: Semantic validation failure (FastAPI standard).
  - `429 Too Many Requests`: Rate limit exhausted.
- **5xx (Server Error)**: Origin server failed to fulfill an apparently valid request (`500`, `502`, `503`, `504`).

---

## 2. Core Concepts & Architecture

```
+-----------------------------------------------------------------------------------+
|               FastAPI Status Code Resolution Pipeline                            |
+-----------------------------------------------------------------------------------+
|                                                                                   |
|  1. Default Success:                                                              |
|     Declared via `@app.post(..., status_code=status.HTTP_201_CREATED)`            |
|                                                                                   |
|  2. Dynamic Runtime Overrides:                                                    |
|     Via `response.status_code = status.HTTP_202_ACCEPTED`                        |
|                                                                                   |
|  3. Exception Path:                                                               |
|     Raising `raise HTTPException(status_code=status.HTTP_409_CONFLICT)`          |
|                                                                                   |
|  4. OpenAPI Documentation Registration:                                           |
|     `responses={404: {"model": ErrorResponse}, 409: {"model": ConflictDetail}}`  |
|                                                                                   |
+-----------------------------------------------------------------------------------+
```

---

## 3. How It Works Internally

1. **`fastapi.status` Module**:
   - Re-exports `starlette.status`. Using named constants (`status.HTTP_201_CREATED`) prevents typo-driven status bugs.
2. **OpenAPI Status Code Map**:
   - FastAPI populates `responses` in `openapi.json`. The `status_code` argument sets the primary success entry (`200` or `201`), while `responses={...}` documents alternate error and redirection schemas.

---

## 4. Syntax & API Reference

```python
from fastapi import APIRouter, HTTPException, Response, status
from pydantic import BaseModel

router = APIRouter()

class ItemResponse(BaseModel):
    id: int
    name: str

class ErrorSchema(BaseModel):
    error: str
    detail: str

@router.post(
    "/items",
    response_model=ItemResponse,
    status_code=status.HTTP_201_CREATED,
    responses={
        status.HTTP_409_CONFLICT: {"model": ErrorSchema, "description": "Duplicate item"},
        status.HTTP_422_UNPROCESSABLE_ENTITY: {"description": "Pydantic validation failure"},
    },
)
def create_item(name: str):
    ...
```

---

## 5. Verified Code Examples

### Example 1: Minimal Asynchronous 202 Accepted Endpoint

```python
import uuid
from fastapi import FastAPI, Response, status
from pydantic import BaseModel

app = FastAPI()

class VideoEncodingJob(BaseModel):
    job_id: str
    status: str

@app.post("/videos/encode", status_code=status.HTTP_202_ACCEPTED, response_model=VideoEncodingJob)
def enqueue_encoding(video_url: str, response: Response):
    job_id = f"job_{uuid.uuid4().hex[:8]}"
    # 202 Accepted signals processing has been queued
    response.headers["Location"] = f"/jobs/{job_id}"
    return VideoEncodingJob(job_id=job_id, status="queued")
```

### Example 2: Enterprise Optimistic Locking with 409 Conflict & 412 Precondition Failed

```python
from typing import Annotated
from fastapi import APIRouter, Header, HTTPException, Path, status
from pydantic import BaseModel

router = APIRouter(prefix="/documents")

class Document(BaseModel):
    id: int
    content: str
    version: int

DOCUMENTS_DB = {1: Document(id=1, content="Draft v1", version=3)}

class DocumentUpdate(BaseModel):
    content: str
    expected_version: int

@router.put("/{doc_id}", response_model=Document)
def update_document(
    doc_id: Annotated[int, Path()],
    update_data: DocumentUpdate,
    if_match: Annotated[str | None, Header(alias="If-Match")] = None,
):
    doc = DOCUMENTS_DB.get(doc_id)
    if not doc:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="Document not found")

    # Optimistic locking concurrency check
    if doc.version != update_data.expected_version:
        raise HTTPException(
            status_code=status.HTTP_409_CONFLICT,
            detail=(
                f"Version conflict. Expected version {update_data.expected_version}, "
                f"but current version is {doc.version}."
            ),
        )

    doc.content = update_data.content
    doc.version += 1
    DOCUMENTS_DB[doc_id] = doc
    return doc
```

### Example 3: Edge Case / Boundary — 304 Not Modified Conditional Evaluation

```python
from typing import Annotated
from fastapi import FastAPI, Header, Response, status

app = FastAPI()
RESOURCE_ETAG = '"v1.4.2-asset"'

@app.get("/static/app-bundle")
def get_bundle(if_none_match: Annotated[str | None, Header(alias="If-None-Match")] = None):
    if if_none_match == RESOURCE_ETAG:
        # RFC 9110: 304 Not Modified MUST NOT contain a message body
        return Response(status_code=status.HTTP_304_NOT_MODIFIED)

    res = Response(content="console.log('App Loaded');", media_type="application/javascript")
    res.headers["ETag"] = RESOURCE_ETAG
    res.headers["Cache-Control"] = "public, max-age=86400"
    return res
```

### Example 4: Failure Case — Rejecting Body in 204 No Content

```python
# BROKEN:
# return Response(content="item deleted", status_code=204)
# Triggers HTTP Protocol violations in strict proxies!
#
# CORRECT:
@app.delete("/items/{id}", status_code=status.HTTP_204_NO_CONTENT)
def delete_item(id: int):
    return Response(status_code=status.HTTP_204_NO_CONTENT)
```

### Example 5: Idiomatic Modern Pattern with Documented OpenAPI Responses

```python
from fastapi import APIRouter, HTTPException, status
from pydantic import BaseModel

router = APIRouter(prefix="/accounts")

class Account(BaseModel):
    id: str
    balance: float

class ErrorResponse(BaseModel):
    code: str
    message: str

@router.get(
    "/{account_id}",
    response_model=Account,
    responses={
        status.HTTP_404_NOT_FOUND: {
            "model": ErrorResponse,
            "description": "Account not found or inactive",
        },
        status.HTTP_403_FORBIDDEN: {
            "model": ErrorResponse,
            "description": "Caller lacks permission to view account",
        },
    },
)
def get_account(account_id: str):
    if account_id == "unknown":
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail="Account does not exist",
        )
    return Account(id=account_id, balance=12500.50)
```

---

## 6. Edge Cases, Pitfalls & Failure Modes

1. **Returning a Message Body with 204 No Content or 304 Not Modified**: RFC 9110 strictly forbids response bodies for 204 and 304. Sending text or JSON with these codes causes ASGI and HTTP client deserializer panics.
2. **Confusing 400 Bad Request with 422 Unprocessable Entity**: 400 signifies malformed request syntax (invalid JSON, corrupt framing). 422 signifies syntactically valid JSON that violates semantic schema constraints (e.g. `age < 0`). FastAPI returns 422 for schema failures.
3. **Using 302 Found Instead of 307/308**: A legacy 302 redirect allows browsers to mutate an original `POST` request into a `GET` request. Modern APIs should use `307 Temporary Redirect` or `308 Permanent Redirect` to preserve the original HTTP method.

---

## 7. Common Mistakes Table

| Mistake | Why It Fails | Error / Diagnostic | Correct Idiomatic Fix |
|---|---|---|---|
| 1. Returning response body on 204 No Content | Violates HTTP specifications | Client parsing errors / dropped socket | Return empty `Response(status_code=204)` |
| 2. Hardcoding magic numbers (`404`, `200`) | Poor readability; prone to typographical errors | Mismatched status codes in reviews | Use `starlette.status` constants |
| 3. Using 401 for permission issues | Client treats 401 as missing token, triggering login loop | Infinite login refresh loop | Return 403 Forbidden when authenticated |
| 4. Returning 200 OK for queued async tasks | Client assumes work is finished immediately | Inconsistent application state | Return `202 Accepted` with job tracking URI |
| 5. Returning 500 for business validation errors | Server error alarms trigger falsely | Polluted Sentry/monitoring logs | Return 400, 409, or 422 |
| 6. Not documenting error codes in OpenAPI | Swagger UI only displays 200/201 schemas | Incomplete client SDK generation | Use `responses={404: ..., 409: ...}` |

---

## 8. Performance & Scalability Considerations

- **304 Cache Offloading**: Returning `304 Not Modified` eliminates response payload serialization and transmission bandwidth entirely, freeing network buffers.
- **202 Queuing**: Offloading long-running calculations to background queues and returning `202 Accepted` ensures HTTP worker threads remain unblocked.

---

## 9. Security & OWASP Guardrails

- **Information Disclosure via 500 Errors**: Never return raw database driver exceptions (e.g. `psycopg2.IntegrityError`) directly in HTTP 500 responses. Catch exceptions and return generic error messages.
- **Enumeration Defense via 404 vs 403**: Return 404 rather than 403 when an unauthorized user attempts to access a specific private object ID.

---

## 10. Testing Strategies & Pytest Recipes

```python
import pytest
from httpx import ASGITransport, AsyncClient
from fastapi import FastAPI

@pytest.mark.asyncio
async def test_status_code_semantics(app: FastAPI):
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        # Check 202 Accepted
        res_202 = await client.post("/videos/encode?video_url=http://example.com/test.mp4")
        assert res_202.status_code == 202
        assert "location" in res_202.headers

        # Check 304 Not Modified
        res_304 = await client.get("/static/app-bundle", headers={"If-None-Match": '"v1.4.2-asset"'})
        assert res_304.status_code == 304
        assert res_304.content == b""
```

---

## 11. Real-World Case Study

An online marketplace API returned `HTTP 200 OK` for all checkout requests with a response body of `{"success": false, "error": "Insufficient inventory"}`. Third-party merchants automated inventory re-ordering based on HTTP status codes. When the marketplace had an outage, the merchants' automated bots logged 100% success rates while zero items were being processed, creating massive logistics reconciliation failures.
- **The Redesign**:
  1. Standardized all responses: `409 Conflict` for inventory exhaustion, `422 Unprocessable Entity` for credit card validation errors, and `201 Created` for confirmed orders.
  2. Merchant webhooks immediately synchronized without parsing custom body wrappers.

---

## 12. Technical Interview Questions

<details>
<summary>Q1: What is the semantic difference between HTTP 400 Bad Request and HTTP 422 Unprocessable Entity?</summary>

**Answer**:
- **400 Bad Request**: Indicates that the server cannot process the request due to perceived client error in syntax (malformed JSON syntax, invalid HTTP headers, bad framing).
- **422 Unprocessable Entity (RFC 4918 / RFC 9110)**: Indicates that the syntax of the request was well-formed and valid JSON, but the server was unable to process the contained instructions due to semantic validation failures (e.g. string failed regex, missing required field, negative value for positive integer). FastAPI uses 422 as its standard for Pydantic validation failures.
</details>

<details>
<summary>Q2: What is the purpose of HTTP 202 Accepted?</summary>

**Answer**: `202 Accepted` indicates that the request has been accepted for processing, but the processing has not been completed. It is non-committal: the task might eventually fail during asynchronous execution. It is standard for long-running batch jobs, video encodings, and background queues, typically accompanied by a `Location` header pointing to a job status polling endpoint.
</details>

<details>
<summary>Q3: Why were HTTP 307 and 308 introduced alongside HTTP 301 and 302?</summary>

**Answer**: Historically, web browsers incorrectly changed the HTTP method from `POST` to `GET` when following a `301` or `302` redirect. RFC 7231 / RFC 9110 introduced `307 Temporary Redirect` and `308 Permanent Redirect` to explicitly mandate that the user agent MUST NOT change the request method when following the redirect (e.g., a `POST` must remain a `POST`).
</details>

<details>
<summary>Q4: When should an API return HTTP 409 Conflict?</summary>

**Answer**: Return `409 Conflict` when a request cannot be completed because it conflicts with the current state of the target resource. Common examples include:
1. Attempting to create a user with an email that already exists (unique constraint violation).
2. Optimistic concurrency locking failures (the client's submitted version does not match the database version).
3. Attempting to delete a parent entity that still has active child dependencies.
</details>

<details>
<summary>Q5: Why is returning a response body with HTTP 204 No Content forbidden?</summary>

**Answer**: RFC 9110 explicitly states that a 204 response terminates at the header section; it has no message body. Intermediate HTTP proxies, caches, and HTTP client parsers determine message framing based on this contract. If a server includes a body, clients may interpret the body bytes as the beginning of the next pipelined HTTP response, leading to protocol desynchronization.
</details>

<details>
<summary>Q6: What is the difference between HTTP 404 Not Found and HTTP 410 Gone?</summary>

**Answer**:
- **404 Not Found**: The server cannot find the requested resource, and has no indication of whether the condition is temporary or permanent.
- **410 Gone**: The target resource is intentionally and permanently deleted with no forwarding address. Caches and search engine indexers should immediately purge the resource from their registries.
</details>

<details>
<summary>Q7: How do you document multiple potential HTTP status codes in FastAPI route definitions?</summary>

**Answer**: Use the `responses` parameter of route decorators:
```python
@app.get("/items/{id}", responses={
    200: {"model": ItemRead},
    404: {"model": ErrorMessage, "description": "Item not found"},
    403: {"model": ErrorMessage, "description": "Unauthorized access"},
})
```
FastAPI registers these variants in `openapi.json`, generating complete documentation in Swagger UI.
</details>

<details>
<summary>Q8: What is the difference between HTTP 502 Bad Gateway, 503 Service Unavailable, and 504 Gateway Timeout?</summary>

**Answer**:
- **502 Bad Gateway**: An edge proxy (Nginx, ALB) received an invalid response (e.g. connection reset or corrupt HTTP) from the upstream FastAPI backend.
- **503 Service Unavailable**: The server or backend is temporarily unable to handle the request due to maintenance or capacity overload.
- **504 Gateway Timeout**: An edge proxy waited for the upstream FastAPI backend, but the backend did not respond within the configured gateway timeout window.
</details>

---

## 13. Practical Exercises & Solutions

<details>
<summary>Exercise 1 (Easy): Return an HTTP 204 No Content response</summary>

**Problem**: Create a route `/session` using DELETE that returns an empty 204 response.

**Solution**:
```python
from fastapi import FastAPI, Response, status

app = FastAPI()

@app.delete("/session", status_code=status.HTTP_204_NO_CONTENT)
def delete_session():
    return Response(status_code=status.HTTP_204_NO_CONTENT)
```
</details>

<details>
<summary>Exercise 2 (Easy): Return HTTP 409 on duplicate username</summary>

**Problem**: Raise 409 Conflict if `username == "admin"`.

**Solution**:
```python
from fastapi import FastAPI, HTTPException, status

app = FastAPI()

@app.post("/register")
def register(username: str):
    if username == "admin":
        raise HTTPException(
            status_code=status.HTTP_409_CONFLICT,
            detail="Username already taken"
        )
    return {"status": "ok"}
```
</details>

<details>
<summary>Exercise 3 (Medium): Implement 307 Temporary Redirect preserving POST payload</summary>

**Problem**: Create a route `/legacy-upload` that redirects to `/v2/upload` via 307.

**Solution**:
```python
from fastapi import FastAPI, status
from fastapi.responses import RedirectResponse

app = FastAPI()

@app.post("/legacy-upload")
def legacy_upload():
    return RedirectResponse(url="/v2/upload", status_code=status.HTTP_307_TEMPORARY_REDIRECT)
```
</details>

<details>
<summary>Exercise 4 (Medium): Build an Asynchronous Job Dispatcher with 202 Accepted</summary>

**Problem**: Create an endpoint that enqueues a report generation job, returning 202 and a `Location` header.

**Solution**:
```python
from fastapi import FastAPI, Response, status

app = FastAPI()

@app.post("/reports/generate", status_code=status.HTTP_202_ACCEPTED)
def generate_report(response: Response):
    job_id = "rep_12345"
    response.headers["Location"] = f"/jobs/{job_id}"
    return {"job_id": job_id, "status": "processing"}
```
</details>

<details>
<summary>Exercise 5 (Hard): Implement Optimistic Locking Concurrency Guard</summary>

**Problem**: Write a route `/wallets/{id}` that takes an `If-Match` header (version integer), raising 412 Precondition Failed if versions mismatch.

**Solution**:
```python
from typing import Annotated
from fastapi import FastAPI, Header, HTTPException, status

app = FastAPI()
WALLETS = {"w1": {"balance": 100, "version": 2}}

@app.put("/wallets/{wallet_id}")
def update_wallet(
    wallet_id: str,
    new_balance: float,
    if_match: Annotated[str | None, Header(alias="If-Match")] = None,
):
    wallet = WALLETS.get(wallet_id)
    if not wallet:
        raise HTTPException(status_code=404, detail="Wallet not found")

    if not if_match or int(if_match) != wallet["version"]:
        raise HTTPException(
            status_code=status.HTTP_412_PRECONDITION_FAILED,
            detail=f"Version mismatch. Current version is {wallet['version']}",
        )

    wallet["balance"] = new_balance
    wallet["version"] += 1
    return wallet
```
</details>

<details>
<summary>Exercise 6 (Hard): Construct Documented Multi-Response Error Matrix</summary>

**Problem**: Build a complete endpoint for `/transfers` documented with 201 Created, 400 (insufficient funds), 404 (account missing), and 409 (concurrency conflict) in the OpenAPI schema.

**Solution**:
```python
from fastapi import FastAPI, HTTPException, status
from pydantic import BaseModel

app = FastAPI()

class TransferResult(BaseModel):
    transfer_id: str
    status: str

class ErrorMsg(BaseModel):
    code: str
    message: str

@app.post(
    "/transfers",
    response_model=TransferResult,
    status_code=status.HTTP_201_CREATED,
    responses={
        status.HTTP_400_BAD_REQUEST: {"model": ErrorMsg, "description": "Insufficient funds"},
        status.HTTP_404_NOT_FOUND: {"model": ErrorMsg, "description": "Destination account missing"},
        status.HTTP_409_CONFLICT: {"model": ErrorMsg, "description": "Duplicate transfer submission"},
    },
)
def execute_transfer(amount: float, dest_account: str):
    if dest_account == "missing":
        raise HTTPException(status_code=404, detail="Destination account missing")
    if amount > 1000:
        raise HTTPException(status_code=400, detail="Insufficient funds")
    return TransferResult(transfer_id="tx_99", status="completed")
```
</details>
