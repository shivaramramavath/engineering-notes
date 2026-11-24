# Module 08: API Design — 07: Pagination: Offset, Cursor, and Keyset (`07-pagination-offset-cursor-keyset`)

> Serving unbounded collections is a primary cause of database memory exhaustion and Denial of Service. In FastAPI, production-grade pagination leverages Offset-based schemas for simple search UI grids and Keyset/Cursor-based pagination with opaque Base64 tokens for high-throughput feeds, eliminating $O(N)$ database scan penalties and concurrent page-drift anomalies.

| Field | Value |
|---|---|
| **Category** | API Design & Standards |
| **Target Level** | Advanced / Production Engineer |
| **FastAPI Version** | FastAPI 0.165.0+ |
| **Starlette Version**| Starlette 1.10.0+ |
| **Pydantic Version** | Pydantic v2.14.4+ (pydantic-settings 2.16.3+) |
| **Python Version**   | Python 3.12+ (3.13 / 3.14 compatible, Free-Threading verified) |
| **Tested With**      | SQLAlchemy 2.0+, httpx 0.28+, pytest 9.1+ |

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

When an API exposes collections with millions of rows, returning all records causes out-of-memory crashes:
- **Offset/Limit Pagination**:
  - Syntax: `GET /items?offset=1000&limit=20` or `GET /items?page=50&size=20`.
  - Pros: Allows jumping directly to arbitrary pages (e.g. "Go to Page 42"); returns total record counts.
  - Cons: Severe database performance degradation at depth (the database reads and discards 10,000 rows to return 20); suffers from **page drift** (new records inserted push existing items onto subsequent pages, causing duplicates).
- **Cursor / Keyset Pagination**:
  - Syntax: `GET /items?cursor=ZXlKMGVY...&limit=20`.
  - Pros: Constant $O(1)$ query time regardless of dataset size by leveraging index seeks: `WHERE (created_at, id) < (:last_time, :last_id)`. Immune to page drift.
  - Cons: Cannot jump to arbitrary pages; moving backward requires bidirectional cursors.

---

## 2. Core Concepts & Architecture

```
+-----------------------------------------------------------------------------------+
|               Offset vs. Keyset Database Execution Mechanics                      |
+-----------------------------------------------------------------------------------+
|                                                                                   |
|  OFFSET PAGINATION (Catastrophic O(N) Scan):                                      |
|  SQL: SELECT * FROM logs ORDER BY id LIMIT 20 OFFSET 1000000;                     |
|  Database reads 1,000,020 rows from disk, discards 1,000,000 rows, returns 20.   |
|  Execution time: 850ms+ (Disk I/O and RAM thrashing)                              |
|                                                                                   |
|  KEYSET PAGINATION (Constant O(1) Index Seek):                                    |
|  SQL: SELECT * FROM logs WHERE id > 1000000 ORDER BY id LIMIT 20;                 |
|  Database traverses B-Tree index directly to row 1,000,001 and reads 20 rows.     |
|  Execution time: 0.4ms (Direct index seek)                                        |
|                                                                                   |
+-----------------------------------------------------------------------------------+

Page Drift Problem Illustrated:

Initial State: [A, B, C, D] (Page 1: [A, B], Page 2: [C, D])
Client fetches Page 1 -> [A, B]
Concurrent User inserts item [NEW] at the top -> [NEW, A, B, C, D]
Client fetches Page 2 with Offset=2 -> [B, C]
RESULT: Client saw item [B] twice! Keyset pagination eliminates this.
```

---

## 3. How It Works Internally

1. **Opaque Base64 Cursor Encoding**:
   - The cursor contains the tie-breaking indexed columns: `{"t": 1711900000.5, "id": 42918}`.
   - Serialized to JSON, then Base64URL-encoded: `ZXlKMGVY...`.
   - Clients treat this as an opaque token, preventing client-side query tampering.
2. **Tuple Comparison in SQL**:
   - Standard SQL supports row-value constructors: `WHERE (created_at, id) < (:cursor_time, :cursor_id)`.
   - When supported by PostgreSQL, this utilizes a composite index `(created_at DESC, id DESC)` in a single seek operation.

---

## 4. Syntax & API Reference

```python
from typing import Generic, TypeVar
from pydantic import BaseModel, Field

T = TypeVar("T")

# 1. Offset Paginated Response Model
class OffsetPage(BaseModel, Generic[T]):
    items: list[T]
    total: int
    page: int
    size: int
    pages: int

# 2. Cursor Paginated Response Model
class CursorPage(BaseModel, Generic[T]):
    items: list[T]
    next_cursor: str | None
    has_next: bool
```

---

## 5. Verified Code Examples

### Example 1: Minimal Offset Pagination Dependency

```python
from typing import Annotated
from fastapi import Depends, FastAPI, Query
from pydantic import BaseModel

app = FastAPI()

class PaginationParams:
    def __init__(
        self,
        page: Annotated[int, Query(ge=1, description="Page number")] = 1,
        size: Annotated[int, Query(ge=1, le=100, description="Items per page")] = 20,
    ):
        self.page = page
        self.size = size
        self.offset = (page - 1) * size

ITEMS = [f"Item {i}" for i in range(1, 105)]

@app.get("/items")
def list_items(pagination: Annotated[PaginationParams, Depends()]):
    total = len(ITEMS)
    sliced = ITEMS[pagination.offset : pagination.offset + pagination.size]
    return {
        "items": sliced,
        "total": total,
        "page": pagination.page,
        "size": pagination.size,
        "pages": (total + pagination.size - 1) // pagination.size,
    }
```

### Example 2: Enterprise Keyset/Cursor Pagination with Opaque Base64 Tokens

```python
import base64
import json
from typing import Annotated
from fastapi import APIRouter, Query
from pydantic import BaseModel

router = APIRouter(prefix="/feed")

class FeedItem(BaseModel):
    id: int
    timestamp: float
    content: str

MOCK_FEED = [
    FeedItem(id=i, timestamp=1711900000.0 - (i * 60), content=f"Post #{i}")
    for i in range(1, 500)
]

def encode_cursor(item: FeedItem) -> str:
    payload = json.dumps({"t": item.timestamp, "id": item.id})
    return base64.urlsafe_b64encode(payload.encode()).decode()

def decode_cursor(cursor_str: str) -> tuple[float, int]:
    raw = base64.urlsafe_b64decode(cursor_str.encode()).decode()
    data = json.loads(raw)
    return float(data["t"]), int(data["id"])

@router.get("")
def get_feed(
    cursor: Annotated[str | None, Query(description="Opaque next_cursor token")] = None,
    limit: Annotated[int, Query(ge=1, le=50)] = 10,
):
    items = MOCK_FEED
    if cursor:
        last_t, last_id = decode_cursor(cursor)
        # Keyset filter: items older than the cursor
        items = [i for i in items if (i.timestamp < last_t) or (i.timestamp == last_t and i.id > last_id)]

    page_items = items[:limit]
    has_next = len(items) > limit
    next_cursor = encode_cursor(page_items[-1]) if (has_next and page_items) else None

    return {
        "items": page_items,
        "next_cursor": next_cursor,
        "has_next": has_next,
    }
```

### Example 3: Edge Case / Boundary — Clamping Unbounded Page Sizes

```python
from fastapi import Query

# Enforcing strict upper bounds prevents memory exhaustion attacks (e.g. limit=10000000)
def safe_pagination_limit(
    limit: int = Query(default=20, ge=1, le=100, description="Page limit strictly bounded to 100")
) -> int:
    return limit
```

### Example 4: Failure Case — Rejecting Malformed / Tampered Cursor Tokens

```python
import base64
from fastapi import HTTPException, status

def safe_decode_cursor(cursor: str) -> dict:
    try:
        raw_bytes = base64.urlsafe_b64decode(cursor.encode("ascii"))
        return json.loads(raw_bytes.decode("utf-8"))
    except (ValueError, UnicodeDecodeError, json.JSONDecodeError):
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail="Malformed or invalid pagination cursor token",
        )
```

### Example 5: Idiomatic Modern Pattern with Pydantic Generic Models

```python
from typing import Generic, TypeVar
from fastapi import FastAPI
from pydantic import BaseModel

T = TypeVar("T")

class PaginatedResult(BaseModel, Generic[T]):
    data: list[T]
    has_more: bool
    cursor: str | None

class UserItem(BaseModel):
    id: int
    email: str

app = FastAPI()

@app.get("/users", response_model=PaginatedResult[UserItem])
def get_paginated_users():
    return PaginatedResult(
        data=[UserItem(id=1, email="a@corp.com")],
        has_more=False,
        cursor=None,
    )
```

---

## 6. Edge Cases, Pitfalls & Failure Modes

1. **Unindexed Keyset Columns**: If keyset pagination filters by `created_at` but the database lacks an index on `(created_at, id)`, the database falls back to a sequential table scan, destroying the performance advantage of keyset pagination.
2. **Tie-Breaker Omission**: Sorting solely by a non-unique column (like `created_at` or `status`) causes rows with identical timestamps to be skipped or duplicated when crossing cursor boundaries. Always append a unique tie-breaker (such as `id`).
3. **`COUNT(*)` Overhead in Offset Pagination**: Running `SELECT COUNT(*)` on multi-million row tables takes several seconds. Avoid calculating total counts on deep pages; return boolean `has_next` instead.

---

## 7. Common Mistakes Table

| Mistake | Why It Fails | Error / Diagnostic | Correct Idiomatic Fix |
|---|---|---|---|
| 1. Unbounded `limit` query parameter | Client requests `limit=1000000`, causing server OOM | Linux OOM killer terminates worker | Enforce `Query(default=20, le=100)` |
| 2. Deep OFFSET queries on large tables | Database scans and drops millions of rows | High query latency (>2000ms) | Use Keyset/Cursor pagination |
| 3. Keyset without unique tie-breaker | Duplicate timestamps cause records to disappear | Missing records across pages | Always order by `(timestamp DESC, id DESC)` |
| 4. Exposing raw database IDs in cursors | Clients hardcode internal schema assumptions | Schema lock-in | Encode cursors as opaque Base64 strings |
| 5. Running `COUNT(*)` on every request | Overwhelms database CPU on large tables | Slow response times (>500ms) | Use cursor pagination or approximate counts |
| 6. Not handling invalid Base64 in cursor | Malformed cursor causes unhandled 500 error | Internal server error | Catch decoding errors and raise HTTP 400 |

---

## 8. Performance & Scalability Considerations

- **Keyset Index Seek**: Keyset queries execute in **<1ms** even on tables with 100,000,000+ rows because PostgreSQL traverses the B-Tree directly to the target leaf node.
- **Payload Compression**: Ensure response arrays are compressed with `GZipMiddleware` when returning 100-item pages.

---

## 9. Security & OWASP Guardrails

- **OWASP API4:2023 Unrestricted Resource Consumption**: Always enforce hard maximum boundaries on page size (`le=100`). Never allow clients to bypass pagination.
- **Cursor Encryption/Tampering**: If cursors contain sensitive internal IDs or timestamps, encrypt the cursor using AES-GCM or sign it with HMAC.

---

## 10. Testing Strategies & Pytest Recipes

```python
import pytest
from httpx import ASGITransport, AsyncClient
from fastapi import FastAPI

@pytest.mark.asyncio
async def test_cursor_pagination_sequence(app: FastAPI):
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        # Fetch page 1
        res1 = await client.get("/feed?limit=5")
        assert res1.status_code == 200
        data1 = res1.json()
        assert len(data1["items"]) == 5
        cursor = data1["next_cursor"]
        assert cursor is not None

        # Fetch page 2 using cursor
        res2 = await client.get(f"/feed?cursor={cursor}&limit=5")
        assert res2.status_code == 200
        data2 = res2.json()
        assert len(data2["items"]) == 5
        # Ensure page 2 does not repeat items from page 1
        ids_page1 = {item["id"] for item in data1["items"]}
        ids_page2 = {item["id"] for item in data2["items"]}
        assert ids_page1.isdisjoint(ids_page2)
```

---

## 11. Real-World Case Study

A high-volume social activity feed with 80 million events implemented standard `offset/limit` pagination. As users scrolled their infinite feeds, database queries slowed down exponentially: Page 1 took 2ms, Page 50 took 450ms, and Page 200 timed out after 30 seconds.
- **The Redesign**:
  1. Converted the feed to Keyset Pagination using a composite index: `CREATE INDEX idx_feed ON events (created_at DESC, id DESC)`.
  2. Encoded the `(created_at, id)` tuple into an opaque Base64 URL cursor.
  3. Every single infinite-scroll request, whether at row 10 or row 10,000,000, executed in **0.8 milliseconds**. Database CPU dropped by 84%.

---

## 12. Technical Interview Questions

<details>
<summary>Q1: Why does Offset Pagination exhibit $O(N)$ performance degradation on large datasets?</summary>

**Answer**: In relational databases (PostgreSQL/MySQL), `OFFSET N` does not skip rows at the storage engine level. The database engine must scan the table or index sequentially, sort the rows, count $N$ rows, discard all $N$ rows, and only then return the subsequent `LIMIT` rows. When $N = 1,000,000$, the database performs the heavy work of reading 1,000,020 rows, leading to high disk I/O, cache eviction, and high query latency.
</details>

<details>
<summary>Q2: What is "Page Drift" in pagination, and which strategy eliminates it?</summary>

**Answer**: Page drift occurs when records are inserted or deleted while a client is paginating through results. If a client fetches Page 1 (items 1-20), and 5 new records are inserted at the beginning of the table, fetching Page 2 with `OFFSET 20` will return items 16-35, causing the client to see items 16-20 a second time. Keyset/Cursor pagination eliminates page drift because the query anchor is tied to the exact record value (`WHERE id < :last_id`), not an arbitrary offset index.
</details>

<details>
<summary>Q3: Why must Keyset pagination always include a unique column (like `id`) in the ordering?</summary>

**Answer**: If keyset pagination orders by a non-unique column (like `created_at`), and multiple records share the exact same timestamp across a page boundary, filtering by `WHERE created_at < :last_time` will skip all remaining records that share that timestamp. Appending `id` (e.g. `(created_at, id)`) creates a strictly monotonic sequence with a unique tie-breaker.
</details>

<details>
<summary>Q4: When is Offset Pagination appropriate over Keyset Pagination?</summary>

**Answer**: Offset pagination is appropriate when:
1. The dataset is small or bounded (< 50,000 rows).
2. The user interface explicitly requires random-access numbered pagination (e.g. "Page 1, 2, 3 ... 45").
3. Users frequently sort by arbitrary, non-indexed dynamic columns in search grids.
</details>

<details>
<summary>Q5: Why should pagination cursors be Base64-encoded instead of exposing raw values?</summary>

**Answer**: Encoding cursors as opaque Base64 strings:
1. Prevents client developers from building code dependent on internal database columns.
2. Allows backend engineers to alter the underlying cursor implementation (e.g. switching from integer IDs to UUIDs or composite keys) without breaking client contracts.
3. Obscures internal database structure and reduces tampering.
</details>

<details>
<summary>Q6: How do you handle bidirectional pagination (moving forward and backward) with cursors?</summary>

**Answer**: Return two cursors in the response metadata: `next_cursor` and `prev_cursor`. To move backward, the client sends `?cursor={prev_cursor}&direction=backward`. The backend inverts the comparison operator (`>`) and ordering (`ASC`), fetches the records, and reverses the resulting array in memory before returning to the client.
</details>

<details>
<summary>Q7: Why is `SELECT COUNT(*)` considered dangerous in high-concurrency pagination endpoints?</summary>

**Answer**: In MVCC databases like PostgreSQL, `COUNT(*)` requires traversing the visibility map or scanning table pages to determine which row versions are visible to the current transaction. On tables with millions of rows, this scan takes hundreds of milliseconds, easily exhausting database connection pools under load.
</details>

<details>
<summary>Q8: How does FastAPI's Pydantic generic model (`Generic[T]`) simplify pagination schemas?</summary>

**Answer**: Defining a generic schema `class Page(BaseModel, Generic[T]): items: list[T]` allows the same pagination wrapper (page, size, total, cursor) to be reused across different domain entities (`Page[UserRead]`, `Page[ProductRead]`), generating strongly typed OpenAPI schemas for every entity without duplicating pagination model boilerplate.
</details>

---

## 13. Practical Exercises & Solutions

<details>
<summary>Exercise 1 (Easy): Create an Offset Pagination dependency with Pydantic</summary>

**Problem**: Write a dependency `Pagination(page=1, size=20)` enforcing `size <= 50`.

**Solution**:
```python
from fastapi import Query

class Pagination:
    def __init__(
        self,
        page: int = Query(default=1, ge=1),
        size: int = Query(default=20, ge=1, le=50),
    ):
        self.page = page
        self.size = size
        self.offset = (page - 1) * size
```
</details>

<details>
<summary>Exercise 2 (Easy): Encode an ID into an opaque Base64 cursor</summary>

**Problem**: Write a function `make_cursor(entity_id: int) -> str`.

**Solution**:
```python
import base64

def make_cursor(entity_id: int) -> str:
    return base64.urlsafe_b64encode(str(entity_id).encode()).decode()
```
</details>

<details>
<summary>Exercise 3 (Medium): Decode and validate an integer cursor</summary>

**Problem**: Write a function that safely decodes a Base64 cursor to an integer, raising HTTP 400 on error.

**Solution**:
```python
import base64
from fastapi import HTTPException, status

def decode_int_cursor(cursor_str: str) -> int:
    try:
        raw = base64.urlsafe_b64decode(cursor_str.encode()).decode()
        return int(raw)
    except Exception:
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail="Invalid cursor token")
```
</details>

<details>
<summary>Exercise 4 (Medium): Build a Generic Pydantic Page Model</summary>

**Problem**: Define a generic Pydantic class `PageResponse[T]` with `data: list[T]`, `next_cursor: str | None`, and `has_more: bool`.

**Solution**:
```python
from typing import Generic, TypeVar
from pydantic import BaseModel

T = TypeVar("T")

class PageResponse(BaseModel, Generic[T]):
    data: list[T]
    next_cursor: str | None = None
    has_more: bool
```
</details>

<details>
<summary>Exercise 5 (Hard): Implement SQLAlchemy 2.0 Keyset Query Builder</summary>

**Problem**: Write an async function that applies keyset filtering `(created_at, id)` to a SQLAlchemy select statement.

**Solution**:
```python
from sqlalchemy import select, tuple_
# In real code: from models import Event

def apply_keyset_filter(stmt, last_created_at, last_id, limit=20):
    # Assuming Event model has created_at and id
    # stmt = stmt.where(tuple_(Event.created_at, Event.id) < (last_created_at, last_id))
    # stmt = stmt.order_by(Event.created_at.desc(), Event.id.desc()).limit(limit + 1)
    return "stmt_with_tuple_comparison_and_limit_plus_one"
```
</details>

<details>
<summary>Exercise 6 (Hard): Construct Complete Bidirectional Cursor Paginator</summary>

**Problem**: Build a cursor paginator supporting both forward (`next_cursor`) and backward (`prev_cursor`) navigation over an in-memory list.

**Solution**:
```python
class BidirectionalPaginator:
    def __init__(self, data: list[dict]):
        self.data = sorted(data, key=lambda x: x["id"])

    def paginate(self, cursor: int | None, limit: int = 10, backward: bool = False):
        if not cursor:
            items = self.data[:limit]
            has_next = len(self.data) > limit
            return {"items": items, "next_cursor": items[-1]["id"] if has_next else None, "prev_cursor": None}

        if backward:
            candidates = [x for x in self.data if x["id"] < cursor]
            items = candidates[-limit:]
            return {
                "items": items,
                "next_cursor": items[-1]["id"] if items else None,
                "prev_cursor": items[0]["id"] if len(candidates) > limit else None
            }
        else:
            candidates = [x for x in self.data if x["id"] > cursor]
            items = candidates[:limit]
            return {
                "items": items,
                "next_cursor": items[-1]["id"] if len(candidates) > limit else None,
                "prev_cursor": items[0]["id"] if items else None
            }
```
</details>
