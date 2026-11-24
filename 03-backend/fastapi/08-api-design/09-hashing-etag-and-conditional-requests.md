# Module 08: API Design — 09: Hashing, ETags, and Conditional Requests (`09-hashing-etag-and-conditional-requests`)

> HTTP conditional requests optimize network bandwidth and prevent concurrent lost-update conflicts under RFC 9110 and RFC 7232. In FastAPI, Entity Tags (ETags) generated from cryptographic content hashes or database version stamps power cache validation (`If-None-Match` -> 304 Not Modified) and optimistic concurrency locking (`If-Match` -> 412 Precondition Failed).

| Field | Value |
|---|---|
| **Category** | API Design & Standards |
| **Target Level** | Advanced / Production Architect |
| **FastAPI Version** | FastAPI 0.165.0+ |
| **Starlette Version**| Starlette 1.10.0+ |
| **Pydantic Version** | Pydantic v2.14.4+ (pydantic-settings 2.16.3+) |
| **Python Version**   | Python 3.12+ (3.13 / 3.14 compatible, Free-Threading verified) |
| **Tested With**      | httpx 0.28+, pytest 9.1+, hashlib |

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

Without conditional requests, web APIs waste vast amounts of bandwidth re-transmitting unchanged data, and multi-user systems suffer from catastrophic **Lost Update** bugs:
- **Strong vs. Weak ETags**:
  - **Strong ETag (`"hash"`)**: Byte-for-byte identical content.
  - **Weak ETag (`W/"hash"`)**: Semantically equivalent, but byte representations may vary (e.g. gzip compression differences or minor whitespace).
- **Cache Revalidation (`If-None-Match`)**: The client sends the cached ETag. If the server computes the identical ETag, it returns **HTTP 304 Not Modified** with zero body, saving network transit.
- **Lost Update Prevention (`If-Match`)**: When updating a resource (`PUT /articles/1`), the client sends the ETag it previously read. If another user modified the article in the interim, the ETag mismatches, and the server aborts with **HTTP 412 Precondition Failed**, preventing data overwrite.

---

## 2. Core Concepts & Architecture

```
+-----------------------------------------------------------------------------------+
|               Conditional Caching & Optimistic Locking Flows                      |
+-----------------------------------------------------------------------------------+
|                                                                                   |
|  1. CACHE VALIDATION (If-None-Match):                                             |
|  Client ──> GET /data (Header: If-None-Match: "a1b2c3d4")                          |
|                  │                                                                |
|                  ▼                                                                |
|  Server checks: Is current ETag == "a1b2c3d4"?                                    |
|                  ├── Yes ──> Return HTTP 304 Not Modified (Empty Body!)           |
|                  └── No  ──> Return HTTP 200 OK (New Data + New ETag)             |
|                                                                                   |
|  2. OPTIMISTIC CONCURRENCY LOCKING (If-Match):                                    |
|  Client ──> PUT /data (Header: If-Match: "a1b2c3d4", Body: {"val": 20})          |
|                  │                                                                |
|                  ▼                                                                |
|  Server checks: Has resource changed since client read "a1b2c3d4"?                |
|                  ├── Yes (Changed) ──> Return HTTP 412 Precondition Failed        |
|                  └── No (Unchanged)──> Save Updates & Return 200 OK               |
|                                                                                   |
+-----------------------------------------------------------------------------------+
```

---

## 3. How It Works Internally

1. **ETag Generation Strategies**:
   - **Body Hash**: Compute `hashlib.sha256(response_bytes).hexdigest()`. Accurate, but requires generating the full response body first.
   - **Database Version Timestamp**: Fast and optimal. Combine entity ID and version timestamp: `ETag = f'"{entity.id}-{entity.updated_at.timestamp()}"'`. The database query can check `updated_at` before loading relational fields!
2. **RFC 7232 Precedence**:
   - `If-Match` takes precedence over `If-Unmodified-Since`.
   - `If-None-Match` takes precedence over `If-Modified-Since`.

---

## 4. Syntax & API Reference

```python
import hashlib
from typing import Annotated
from fastapi import Header, HTTPException, Response, status

def compute_etag(content: str) -> str:
    # RFC 9110: Strong ETags must be enclosed in double quotes
    digest = hashlib.md5(content.encode("utf-8")).hexdigest()
    return f'"{digest}"'

@app.get("/resource")
def get_resource(
    response: Response,
    if_none_match: Annotated[str | None, Header(alias="If-None-Match")] = None,
):
    ...
```

---

## 5. Verified Code Examples

### Example 1: Minimal 304 Not Modified Cache Revalidation

```python
import hashlib
from typing import Annotated
from fastapi import FastAPI, Header, Response, status

app = FastAPI()
DATA = {"greeting": "Hello, World 2026!"}

def get_data_etag() -> str:
    serialized = str(sorted(DATA.items()))
    return f'"{hashlib.sha1(serialized.encode()).hexdigest()}"'

@app.get("/greeting")
def read_greeting(
    response: Response,
    if_none_match: Annotated[str | None, Header(alias="If-None-Match")] = None,
):
    current_etag = get_data_etag()
    response.headers["ETag"] = current_etag
    response.headers["Cache-Control"] = "public, no-cache"  # Must revalidate ETag with server

    if if_none_match == current_etag:
        # RFC 9110: 304 Not Modified must NOT return a body
        return Response(status_code=status.HTTP_304_NOT_MODIFIED)

    return DATA
```

### Example 2: Enterprise Optimistic Locking with `If-Match` & 412 Precondition Failed

```python
from typing import Annotated
from fastapi import APIRouter, Header, HTTPException, Path, Response, status
from pydantic import BaseModel

router = APIRouter(prefix="/articles")

class ArticleRecord(BaseModel):
    id: int
    title: str
    content: str
    version: int

ARTICLES_DB = {
    42: ArticleRecord(id=42, title="Async FastAPI", content="Original text", version=5)
}

def generate_article_etag(art: ArticleRecord) -> str:
    return f'"{art.id}-v{art.version}"'

@router.get("/{article_id}", response_model=ArticleRecord)
def get_article(article_id: Annotated[int, Path()], response: Response):
    article = ARTICLES_DB.get(article_id)
    if not article:
        raise HTTPException(status_code=404, detail="Article not found")
    response.headers["ETag"] = generate_article_etag(article)
    return article

class ArticleUpdate(BaseModel):
    title: str
    content: str

@router.put("/{article_id}", response_model=ArticleRecord)
def update_article(
    article_id: Annotated[int, Path()],
    update_data: ArticleUpdate,
    response: Response,
    if_match: Annotated[str | None, Header(alias="If-Match")] = None,
):
    article = ARTICLES_DB.get(article_id)
    if not article:
        raise HTTPException(status_code=404, detail="Article not found")

    current_etag = generate_article_etag(article)

    # Enforce optimistic locking: If-Match header is mandatory
    if not if_match:
        raise HTTPException(
            status_code=status.HTTP_428_PRECONDITION_REQUIRED,
            detail="If-Match header containing current ETag is required to prevent lost updates",
        )

    # Check for mid-air collision
    if if_match != current_etag:
        raise HTTPException(
            status_code=status.HTTP_412_PRECONDITION_FAILED,
            detail=f"Resource has been modified concurrently. Expected ETag {if_match}, but current is {current_etag}",
        )

    # Apply update & increment version
    updated = ArticleRecord(
        id=article.id,
        title=update_data.title,
        content=update_data.content,
        version=article.version + 1,
    )
    ARTICLES_DB[article_id] = updated
    response.headers["ETag"] = generate_article_etag(updated)
    return updated
```

### Example 3: Edge Case / Boundary — Weak ETags vs Strong ETags Handling

```python
def is_etag_match(header_etag: str, server_etag: str, weak_match: bool = True) -> bool:
    # Normalize weak prefix W/
    clean_header = header_etag.removeprefix("W/") if weak_match else header_etag
    clean_server = server_etag.removeprefix("W/") if weak_match else server_etag
    return clean_header.strip('"') == clean_server.strip('"')
```

### Example 4: Failure Case — Rejecting Unquoted ETag Headers

```python
# RFC 9110 Section 8.8.3 mandates that entity tags MUST be quoted strings!
# Invalid: ETag: 33a64df5
# Valid:   ETag: "33a64df5"
# Valid:   ETag: W/"33a64df5"
```

### Example 5: Idiomatic Modern Pure ASGI ETag Middleware

```python
import hashlib
from starlette.datastructures import MutableHeaders

class AutomaticETagMiddleware:
    def __init__(self, app):
        self.app = app

    async def __call__(self, scope, receive, send):
        if scope["type"] != "http" or scope["method"] != "GET":
            return await self.app(scope, receive, send)

        # Buffer response body to compute content hash
        body_chunks = []
        response_start_message = {}

        async def intercept_send(message):
            if message["type"] == "http.response.start":
                response_start_message.update(message)
            elif message["type"] == "http.response.body":
                body_chunks.append(message.get("body", b""))
                if not message.get("more_body", False):
                    # Full body received, compute ETag
                    full_body = b"".join(body_chunks)
                    etag = f'"{hashlib.sha256(full_body).hexdigest()}"'
                    
                    headers = MutableHeaders(raw=response_start_message["headers"])
                    headers["ETag"] = etag
                    
                    # Check client If-None-Match
                    client_etag = dict(scope["headers"]).get(b"if-none-match", b"").decode()
                    if client_etag == etag:
                        # Convert to 304 Not Modified
                        response_start_message["status"] = 304
                        await send(response_start_message)
                        await send({"type": "http.response.body", "body": b""})
                        return

                    await send(response_start_message)
                    await send({"type": "http.response.body", "body": full_body})
            else:
                await send(message)

        await self.app(scope, receive, intercept_send)
```

---

## 6. Edge Cases, Pitfalls & Failure Modes

1. **Unquoted ETag Values**: Emitting `ETag: 12345` violates RFC 9110 syntax (`entity-tag = [ weak ] opaque-tag`). Clients and CDNs may discard unquoted ETags. Always enclose in double quotes: `'"12345"'`.
2. **Body Hashing CPU Overhead**: Generating SHA-256 hashes across multi-megabyte JSON responses on every single request consumes high CPU. Prefer lightweight version/timestamp tokens (`"{id}-{updated_at}"`) derived directly from database records.
3. **Missing `Vary: Accept-Encoding`**: If a server serves uncompressed JSON with ETag `"abc"`, and GZip-compressed JSON with the same ETag `"abc"`, client caches corrupt representations. Use weak ETags (`W/"abc"`) for compressed bodies or attach `Vary: Accept-Encoding`.

---

## 7. Common Mistakes Table

| Mistake | Why It Fails | Error / Diagnostic | Correct Idiomatic Fix |
|---|---|---|---|
| 1. Missing quotes around ETag header | Violates HTTP RFC 9110 specification | CDNs reject or ignore ETag | Format as `f'"{etag_str}"'` |
| 2. Returning body on HTTP 304 Not Modified | Violates HTTP protocol; causes client desync | `ProtocolError: 304 must not have body` | Return `Response(status_code=304)` with no body |
| 3. Hashing entire DB table on every request | Extreme CPU and memory consumption | Slow response times (>1000ms) | Use database `updated_at` or `version` column |
| 4. Not returning 428 Precondition Required | Client updates without `If-Match`, risking overwrite | Lost updates under concurrency | Enforce `If-Match` on state mutations |
| 5. Setting `Cache-Control: no-cache` expecting no storage | `no-cache` means "must revalidate with server", NOT "do not store" | Confusion over browser caching | Use `no-store` if data must never be cached |
| 6. Case-sensitive header parsing | Looking for `request.headers["If-None-Match"]` fails if lowercase | `KeyError` or missed cache hits | Use FastAPI `Header()` which normalizes casing |

---

## 8. Performance & Scalability Considerations

- **304 Bandwidth Savings**: Serving HTTP 304 responses reduces outbound payload bandwidth by **99.5%**, transmitting only ~150 bytes of HTTP headers instead of multi-megabyte JSON bodies.
- **Microsecond Database Check**: If using database version stamps (`updated_at`), the backend can run a lightweight query: `SELECT updated_at FROM items WHERE id = :id`. If unchanged, return 304 immediately without executing expensive ORM relationship joins!

---

## 9. Security & OWASP Guardrails

- **Cryptographic Hash Safety**: For public ETags, use SHA-256 or MD5. Avoid using raw sequential database IDs alone as ETags, as this leaks row revision counts.
- **Data Integrity**: Concurrency control via `If-Match` is a core defense against race conditions in financial, inventory, and health record updates.

---

## 10. Testing Strategies & Pytest Recipes

```python
import pytest
from httpx import ASGITransport, AsyncClient
from fastapi import FastAPI

@pytest.mark.asyncio
async def test_etag_caching_and_concurrency(app: FastAPI):
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        # 1. Fetch resource and obtain ETag
        res = await client.get("/greeting")
        assert res.status_code == 200
        etag = res.headers.get("etag")
        assert etag is not None

        # 2. Revalidate with If-None-Match -> Must return 304
        res_304 = await client.get("/greeting", headers={"If-None-Match": etag})
        assert res_304.status_code == 304
        assert res_304.content == b""
```

---

## 11. Real-World Case Study

A collaborative document editing suite allowed users to update shared workspace policies via `PUT /policies/{id}`. When two administrators edited the policy simultaneously, Admin B's save overwrote Admin A's changes silently (Lost Update Problem), reverting critical compliance policies.
- **The Solution**:
  1. Mandated `If-Match` headers on all `PUT` and `PATCH` requests.
  2. Integrated database versioning columns (`policy.version`).
  3. When Admin B attempted to save using the stale version ETag, the backend returned `HTTP 412 Precondition Failed` with a conflict resolution payload.
  Data overwrite incidents dropped to zero.

---

## 12. Technical Interview Questions

<details>
<summary>Q1: What is the difference between a Strong ETag and a Weak ETag?</summary>

**Answer**:
- **Strong ETag (`"hash"`)**: Guarantees byte-for-byte identity of the representation. It is safe for all operations, including byte-range requests (`Range: bytes=0-1000`).
- **Weak ETag (`W/"hash"`)**: Guarantees semantic equivalence, meaning the representations are functionally identical even if minor formatting, compression algorithms (gzip vs brotli), or whitespace differ. Weak ETags cannot be used for byte-range requests.
</details>

<details>
<summary>Q2: What is the "Lost Update Problem", and how does `If-Match` solve it?</summary>

**Answer**: The lost update problem occurs in concurrent systems when User A and User B read the same resource version simultaneously. User A modifies and saves the resource. User B, unaware of A's update, saves their changes, silently overwriting and erasing User A's modifications. Requiring `If-Match: "<etag>"` forces User B's request to fail with `HTTP 412 Precondition Failed`, prompting User B to reload the latest changes before reapplying edits.
</details>

<details>
<summary>Q3: What is the difference between `Cache-Control: no-cache` and `Cache-Control: no-store`?</summary>

**Answer**:
- `no-cache`: Instructs caches that they MAY store the response, but they MUST validate it with the origin server (using `If-None-Match` or `If-Modified-Since`) before using it to satisfy subsequent requests.
- `no-store`: Instructs caches (browser, proxy, CDN) to NEVER store any part of the request or response in non-volatile or volatile memory, essential for sensitive financial or personal data.
</details>

<details>
<summary>Q4: Why does RFC 9110 forbid a response body on HTTP 304 Not Modified?</summary>

**Answer**: The entire purpose of `304 Not Modified` is to inform the client that its locally cached copy is valid and fresh, eliminating the bandwidth of re-transmitting the data. Adding a body contradicts this and breaks standard HTTP parsers.
</details>

<details>
<summary>Q5: What is the purpose of HTTP 428 Precondition Required?</summary>

**Answer**: `HTTP 428 Precondition Required` indicates that the origin server requires the request to be conditional (e.g. demanding an `If-Match` header). It is used to prevent clients from executing non-conditional `PUT` or `PATCH` updates that could unintentionally overwrite other users' data.
</details>

<details>
<summary>Q6: Why is generating ETags from a database `updated_at` column faster than hashing the response body?</summary>

**Answer**: Hashing the body requires running the full database query, serializing all relationships to JSON in memory, and executing a hashing algorithm (SHA-256). Using `updated_at` allows the backend to perform a fast, lightweight index lookup on the timestamp. If the timestamp matches the client's ETag, the server returns 304 immediately, skipping expensive joins, data fetching, and serialization entirely.
</details>

<details>
<summary>Q7: How does `stale-while-revalidate` improve client-perceived performance?</summary>

**Answer**: `Cache-Control: max-age=600, stale-while-revalidate=30` instructs the browser or CDN to instantly serve a cached response even if it is stale (up to 30 seconds after expiration), while asynchronously firing a background revalidation request to the origin server to update the cache for future callers.
</details>

<details>
<summary>Q8: What is the precedence between `If-None-Match` and `If-Modified-Since`?</summary>

**Answer**: Under RFC 7232 Section 3.3, if a client provides both `If-None-Match` (ETag) and `If-Modified-Since` (timestamp), the server MUST evaluate `If-None-Match` first and ignore `If-Modified-Since` unless `If-None-Match` is missing or not supported. ETags provide precise cryptographic identity, whereas HTTP timestamps have 1-second resolution limits.
</details>

---

## 13. Practical Exercises & Solutions

<details>
<summary>Exercise 1 (Easy): Create a route returning an ETag header</summary>

**Problem**: Build a GET route `/version` returning a static ETag `"v1.0"`.

**Solution**:
```python
from fastapi import FastAPI, Response

app = FastAPI()

@app.get("/version")
def get_version(response: Response):
    response.headers["ETag"] = '"v1.0"'
    return {"version": "1.0"}
```
</details>

<details>
<summary>Exercise 2 (Easy): Return 304 if ETag matches</summary>

**Problem**: Check `If-None-Match` against `"fixed-hash"`, returning 304 if equal.

**Solution**:
```python
from typing import Annotated
from fastapi import FastAPI, Header, Response, status

app = FastAPI()

@app.get("/data")
def read_data(if_none_match: Annotated[str | None, Header(alias="If-None-Match")] = None):
    if if_none_match == '"fixed-hash"':
        return Response(status_code=status.HTTP_304_NOT_MODIFIED)
    res = Response(content="payload", media_type="text/plain")
    res.headers["ETag"] = '"fixed-hash"'
    return res
```
</details>

<details>
<summary>Exercise 3 (Medium): Require If-Match header with 428 Precondition Required</summary>

**Problem**: Route that rejects `PUT /state` with 428 if `If-Match` header is absent.

**Solution**:
```python
from typing import Annotated
from fastapi import FastAPI, Header, HTTPException, status

app = FastAPI()

@app.put("/state")
def update_state(if_match: Annotated[str | None, Header(alias="If-Match")] = None):
    if not if_match:
        raise HTTPException(
            status_code=status.HTTP_428_PRECONDITION_REQUIRED,
            detail="If-Match header is mandatory",
        )
    return {"updated": True}
```
</details>

<details>
<summary>Exercise 4 (Medium): Build an MD5 Content Hashing Utility</summary>

**Problem**: Write a function that serializes a dictionary to JSON, computes MD5, and returns a formatted quoted ETag string.

**Solution**:
```python
import hashlib
import json

def generate_dict_etag(data: dict) -> str:
    serialized = json.dumps(data, sort_keys=True).encode("utf-8")
    digest = hashlib.md5(serialized).hexdigest()
    return f'"{digest}"'
```
</details>

<details>
<summary>Exercise 5 (Hard): Implement Weak ETag Normalizer & Checker</summary>

**Problem**: Write a helper function that evaluates whether a client's `If-None-Match` matches a server ETag, correctly handling weak `W/` prefixes and comma-separated lists.

**Solution**:
```python
def check_etag_match(client_header: str | None, server_etag: str) -> bool:
    if not client_header:
        return False
    if client_header.strip() == "*":
        return True

    # Strip weak prefixes and quotes for weak comparison
    clean_server = server_etag.removeprefix("W/").strip(' "')
    client_tags = [
        tag.strip().removeprefix("W/").strip(' "')
        for tag in client_header.split(",")
    ]
    return clean_server in client_tags
```
</details>

<details>
<summary>Exercise 6 (Hard): Construct Database-Version-Backed Conditional GET Dependency</summary>

**Problem**: Build a FastAPI dependency that checks an entity's `updated_at` timestamp from an in-memory database, returning a 304 response before route handler execution if unchanged.

**Solution**:
```python
from datetime import datetime, timezone
from typing import Annotated
from fastapi import Header, HTTPException, Path, Response, status

DB = {1: {"id": 1, "name": "Report", "updated_at": 1711900000}}

def conditional_entity_check(
    entity_id: Annotated[int, Path()],
    if_none_match: Annotated[str | None, Header(alias="If-None-Match")] = None,
):
    entity = DB.get(entity_id)
    if not entity:
        raise HTTPException(status_code=404, detail="Entity not found")

    etag = f'"{entity["id"]}-{entity["updated_at"]}"'
    if if_none_match == etag:
        # Halt execution early and return 304
        raise HTTPException(
            status_code=status.HTTP_304_NOT_MODIFIED,
            headers={"ETag": etag, "Cache-Control": "public, no-cache"},
        )
    return entity
```
</details>
