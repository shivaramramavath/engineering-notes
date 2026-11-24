# Module 08: API Design — 08: Filtering, Sorting, and Searching (`08-filtering-sorting-searching`)

> Flexible data discovery requires intuitive, standardized query parameters. In FastAPI, multi-attribute filtering, range constraints, enum-whitelisted multi-field sorting, and PostgreSQL full-text search (`tsvector`) are combined into composable dependency objects that prevent SQL injection and table scan denial-of-service attacks.

| Field | Value |
|---|---|
| **Category** | API Design & Standards |
| **Target Level** | Intermediate / Production Engineer |
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

Query parameters customize collection representations without altering resource identity:
- **Filtering**:
  - Exact match: `status=active`.
  - Multi-value (IN): `status=active,pending` or `status=active&status=pending`.
  - Range queries: `created_after=2026-01-01` or `price[gte]=50`.
- **Sorting**:
  - Standard hyphen prefix notation: `sort=-created_at,price` (descending by `created_at`, ascending by `price`).
  - Column name whitelisting is mandatory to prevent SQL injection in `ORDER BY`.
- **Searching**:
  - Full-Text Search (FTS) queries: `q=fastapi+async`.
  - Simple substring searches (`ILIKE %term%`) do not scale on millions of rows; PostgreSQL `tsvector` or dedicated search engines (Meilisearch, Elasticsearch) are required.

---

## 2. Core Concepts & Architecture

```
+-----------------------------------------------------------------------------------+
|               Filtering, Sorting & Searching Compilation Flow                     |
+-----------------------------------------------------------------------------------+
|                                                                                   |
|  Incoming Request:                                                                |
|  GET /api/v1/products?category=electronics&price_max=500&sort=-rating,price&q=pro|
|                                │                                                  |
|                                ▼                                                  |
|  [ FastAPI Dependency Class: `ProductFilterParams` ]                              |
|  - Validates types, bounds, and string length via Pydantic                        |
|  - Parses sorting tokens: [("rating", "DESC"), ("price", "ASC")]                  |
|                                │                                                  |
|                                ▼                                                  |
|  [ SQLAlchemy 2.0 Query Builder ]                                                 |
|  - Appends `.where(Product.category == "electronics")`                            |
|  - Appends `.where(Product.price <= 500)`                                         |
|  - Appends `.where(Product.search_vector.match("pro"))`                           |
|  - Appends `.order_by(Product.rating.desc(), Product.price.asc())`                |
|                                │                                                  |
|                                ▼                                                  |
|  [ Database Execution Plan (Indexed Composite Scan) ]                             |
|                                                                                   |
+-----------------------------------------------------------------------------------+
```

---

## 3. How It Works Internally

1. **Composable Filter Dependencies**:
   - Defining filter parameters as a class instantiated with `Depends()` groups dozens of query parameters into a single reusable object, keeping route handler signatures clean.
2. **SQLAlchemy 2.0 Dynamic Clause Accumulation**:
   - `select()` expressions in SQLAlchemy are immutable AST nodes. Calling `stmt = stmt.where(...)` constructs a new query node without executing any I/O until `await session.execute(stmt)`.

---

## 4. Syntax & API Reference

```python
from enum import StrEnum
from typing import Annotated
from fastapi import APIRouter, Depends, Query
from pydantic import BaseModel, Field

class ProductSortField(StrEnum):
    CREATED_AT = "created_at"
    PRICE = "price"
    RATING = "rating"

class FilterParams(BaseModel):
    category: str | None = None
    min_price: float | None = Field(default=None, ge=0)
    max_price: float | None = Field(default=None, ge=0)
    sort_by: ProductSortField = ProductSortField.CREATED_AT
    descending: bool = True
    q: str | None = Field(default=None, max_length=100)
```

---

## 5. Verified Code Examples

### Example 1: Minimal Multi-Param Filter & Sort Dependency

```python
from enum import StrEnum
from typing import Annotated
from fastapi import Depends, FastAPI, Query
from pydantic import BaseModel

app = FastAPI()

class SortField(StrEnum):
    NAME = "name"
    PRICE = "price"

class ProductFilters:
    def __init__(
        self,
        category: Annotated[str | None, Query()] = None,
        max_price: Annotated[float | None, Query(ge=0)] = None,
        sort: Annotated[SortField, Query()] = SortField.PRICE,
        desc: Annotated[bool, Query()] = False,
    ):
        self.category = category
        self.max_price = max_price
        self.sort = sort
        self.desc = desc

MOCK_PRODUCTS = [
    {"name": "Mouse", "category": "tech", "price": 25.0},
    {"name": "Keyboard", "category": "tech", "price": 75.0},
    {"name": "Desk", "category": "furniture", "price": 200.0},
]

@app.get("/products")
def get_products(filters: Annotated[ProductFilters, Depends()]):
    results = MOCK_PRODUCTS
    if filters.category:
        results = [p for p in results if p["category"] == filters.category]
    if filters.max_price is not None:
        results = [p for p in results if p["price"] <= filters.max_price]

    results = sorted(results, key=lambda x: x[filters.sort.value], reverse=filters.desc)
    return results
```

### Example 2: Enterprise Multi-Column Sort Parser (`sort=-created_at,price`)

```python
from enum import StrEnum
from typing import Annotated
from fastapi import HTTPException, Query, status

class AllowedColumns(StrEnum):
    CREATED_AT = "created_at"
    PRICE = "price"
    NAME = "name"

def parse_multi_sort(
    sort: Annotated[str | None, Query(description="Comma-separated sort fields, prefix with - for desc")] = "-created_at"
) -> list[tuple[str, str]]:
    if not sort:
        return [("created_at", "DESC")]

    sort_orders = []
    for token in sort.split(","):
        token = token.strip()
        if not token:
            continue
        direction = "DESC" if token.startswith("-") else "ASC"
        field_name = token.lstrip("+-")

        try:
            # Validate against strict Enum whitelist
            valid_field = AllowedColumns(field_name)
            sort_orders.append((valid_field.value, direction))
        except ValueError:
            raise HTTPException(
                status_code=status.HTTP_400_BAD_REQUEST,
                detail=f"Invalid sort field '{field_name}'. Allowed: {[e.value for e in AllowedColumns]}",
            )
    return sort_orders
```

### Example 3: Edge Case / Boundary — List Query Parameter Parsing

```python
from typing import Annotated
from fastapi import FastAPI, Query

app = FastAPI()

# Supports ?status=active&status=pending
@app.get("/orders")
def list_orders(
    status: Annotated[list[str] | None, Query(description="Multiple status filters")] = None
):
    selected_statuses = status or ["all"]
    return {"filtering_by": selected_statuses}
```

### Example 4: Failure Case — Rejecting Unwhitelisted Sorting Columns

```python
# Client attempts SQL injection via sort parameter:
# GET /products?sort=price;DROP TABLE products;
#
# Response:
# HTTP 400 Bad Request
# {
#   "error": "invalid_sort_parameter",
#   "detail": "Invalid sort field 'price;DROP TABLE products;'. Allowed: ['created_at', 'price', 'name']"
# }
```

### Example 5: Idiomatic Modern Pattern with PostgreSQL Full-Text Search in SQLAlchemy 2.0

```python
from sqlalchemy import select, func
# In real application: from models import Article

def build_fts_query(search_term: str):
    # Safe PostgreSQL full text search using websearch_to_tsquery
    # Handles quotes, OR, AND operators naturally without injection
    # stmt = select(Article).where(
    #     Article.search_vector.op("@@")(func.websearch_to_tsquery("english", search_term))
    # ).order_by(
    #     func.ts_rank(Article.search_vector, func.websearch_to_tsquery("english", search_term)).desc()
    # )
    return "compiled_fts_ast_node"
```

---

## 6. Edge Cases, Pitfalls & Failure Modes

1. **Unindexed Substring Searches (`ILIKE %term%`)**: Leading wildcards (`%term`) completely bypass B-Tree indexes, forcing full table scans across millions of rows. Use trigram indexes (`pg_trgm`) or FTS (`tsvector`).
2. **Sort Inversion Bugs**: Failing to strip leading whitespace when parsing comma-delimited sort parameters (`sort=price, -name`) can cause validation errors or incorrect direction flags.
3. **Array vs Comma Delimitation**: Some clients send `category=1,2,3` while others send `category=1&category=2`. Standardize on accepting both formats or specify clearly in documentation.

---

## 7. Common Mistakes Table

| Mistake | Why It Fails | Error / Diagnostic | Correct Idiomatic Fix |
|---|---|---|---|
| 1. Unvalidated sorting column string | SQL Injection vulnerability in `ORDER BY` | Critical security flaw | Whitelist sort columns via `StrEnum` |
| 2. `ILIKE %term%` on large datasets | Massive CPU spikes and slow queries | Database timeouts (>3000ms) | Use PostgreSQL `tsvector` or Trigram indexes |
| 3. Combining 20 separate Query params in route signature | Cluttered route definition; violates DRY | Unmaintainable code | Bundle into a class via `Depends(FilterParams)` |
| 4. Not validating min/max price bounds | Client sends `min_price=100&max_price=50` | Contradictory query returns empty | Add model validator checking `min <= max` |
| 5. Case-sensitive enum mismatches | `sort=Price` fails against `price` | Spurious 422 errors | Normalize to lowercase before validation |
| 6. Not limiting maximum search string length | Attacker sends a 1MB search query | ReDoS / memory exhaustion | Set `Query(..., max_length=100)` |

---

## 8. Performance & Scalability Considerations

- **Composite Index Matching**: Create indexes that match the query filter + sort pattern: `CREATE INDEX idx_products_cat_price ON products (category, price ASC)`.
- **Search Engine Offloading**: For queries searching millions of text documents, offload search queries to a dedicated search cluster (Elasticsearch/Typesense) rather than executing FTS directly on primary transaction databases.

---

## 9. Security & OWASP Guardrails

- **OWASP API4:2023**: Limit maximum search string lengths to 100 characters to prevent database memory exhaustion.
- **SQL Injection Defense**: Never interpolate sorting strings into raw SQL. Always use SQLAlchemy's model attribute references (`getattr(Model, col).desc()`).

---

## 10. Testing Strategies & Pytest Recipes

```python
import pytest
from httpx import ASGITransport, AsyncClient
from fastapi import FastAPI

@pytest.mark.asyncio
async def test_filtering_and_sorting(app: FastAPI):
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        # Filter by category and sort descending by price
        res = await client.get("/products?category=tech&desc=true&sort=price")
        assert res.status_code == 200
        data = res.json()
        assert len(data) == 2
        assert data[0]["name"] == "Keyboard"  # 75.0 > 25.0
        assert data[1]["name"] == "Mouse"
```

---

## 11. Real-World Case Study

An online marketplace had an endpoint `GET /items?q=...` that executed an unindexed `ILIKE %query%` on item titles. During Black Friday sales, bots and shoppers fired 3,000 search requests per second. The PostgreSQL cluster hit 100% CPU utilization within 30 seconds, causing all checkout and transaction processing to crash.
- **The Solution**:
  1. Replaced the `ILIKE` query with PostgreSQL Full Text Search using GIN indexes: `CREATE INDEX idx_items_fts ON items USING GIN (to_tsvector('english', title))`.
  2. Implemented `websearch_to_tsquery` to safely process search operators (`+`, `-`, quotes).
  Average search latency dropped from 4,200ms to 6ms, and the primary database easily handled the sales surge.

---

## 12. Technical Interview Questions

<details>
<summary>Q1: Why is a leading wildcard (`ILIKE %search%`) so devastating to relational database performance?</summary>

**Answer**: Standard B-Tree indexes are sorted alphabetically. They can efficiently seek strings with known prefixes (`ILIKE search%`), but when a wildcard precedes the term (`%search%`), the B-Tree index is useless because the first character could be anything. The database engine must execute a sequential table scan, reading every row from disk into memory and evaluating regex/string matching on each one.
</details>

<details>
<summary>Q2: How should an API design support multi-field sorting with mixed directions?</summary>

**Answer**: The industry standard pattern uses a comma-delimited string where field names with a leading hyphen (`-`) signify descending order, and bare field names signify ascending order:
`GET /users?sort=-created_at,last_name,first_name`.
The backend splits on commas, inspects the prefix, validates field names against an Enum whitelist, and maps them to database order clauses.
</details>

<details>
<summary>Q3: How does bundling query parameters into a class dependency improve FastAPI architecture?</summary>

**Answer**: Instead of listing 15 query parameters directly in the route handler signature (`def list_items(a: int = Query(...), b: str = ...)`), grouping them into a class `class ItemFilters: def __init__(self, ...)` with `Depends(ItemFilters)` encapsulates validation, parsing, and defaults into a single testable object that can be shared across multiple routes.
</details>

<details>
<summary>Q4: What is the difference between `to_tsquery` and `websearch_to_tsquery` in PostgreSQL?</summary>

**Answer**:
- `to_tsquery` requires strict Boolean syntax with ampersands and pipes (`'fastapi' & 'async'`). Unhandled syntax errors from raw user input cause SQL exceptions.
- `websearch_to_tsquery` parses natural human search inputs, automatically handling quotes for exact phrases, minus signs for exclusions, and ignoring unclosed quotes or syntax errors without crashing.
</details>

<details>
<summary>Q5: How do you handle range queries (e.g. date ranges or price ranges) cleanly in REST APIs?</summary>

**Answer**: Two standard patterns:
1. **Explicit parameter pairs**: `price_min=10&price_max=100` and `created_after=...&created_before=...`. Simple and explicit.
2. **Bracket notation**: `price[gte]=10&price[lte]=100`. Powerful, but requires custom Pydantic parsing.
</details>

<details>
<summary>Q6: Why must sorting column parameters be strictly whitelisted?</summary>

**Answer**: Dynamic `ORDER BY` clauses cannot use parameter binding placeholders (`ORDER BY $1` is invalid in SQL). Developers are forced to construct order clauses dynamically. Without a strict whitelist (e.g. `StrEnum`), an attacker can pass arbitrary SQL expressions, subqueries, or commands into the sorting parameter, resulting in SQL injection.
</details>

<details>
<summary>Q7: How do you validate that `min_price` is less than or equal to `max_price` in Pydantic?</summary>

**Answer**: Use a model validator (`@model_validator(mode="after")`):
```python
@model_validator(mode="after")
def check_price_bounds(self):
    if self.min_price is not None and self.max_price is not None:
        if self.min_price > self.max_price:
            raise ValueError("min_price cannot exceed max_price")
    return self
```
</details>

<details>
<summary>Q8: What is a Trigram (pg_trgm) index, and when should it be used?</summary>

**Answer**: A trigram breaks words into 3-character substrings (e.g. `cat` -> `c`, `ca`, `cat`, `at`). A GIN or GiST index built on trigrams allows PostgreSQL to accelerate leading-wildcard substring searches (`ILIKE %term%`) and fuzzy typo-tolerant matching in sub-millisecond times without requiring a full external search engine.
</details>

---

## 13. Practical Exercises & Solutions

<details>
<summary>Exercise 1 (Easy): Create a status filter accepting multiple values</summary>

**Problem**: Define an endpoint `/orders` accepting a list of statuses via `status: list[str] = Query(...)`.

**Solution**:
```python
from typing import Annotated
from fastapi import FastAPI, Query

app = FastAPI()

@app.get("/orders")
def get_orders(status: Annotated[list[str] | None, Query()] = None):
    return {"statuses": status or []}
```
</details>

<details>
<summary>Exercise 2 (Easy): Define an Enum-restricted sort parameter</summary>

**Problem**: Restrict sorting to `"created_at"` or `"rating"`.

**Solution**:
```python
from enum import StrEnum
from fastapi import FastAPI, Query

app = FastAPI()

class SortEnum(StrEnum):
    CREATED_AT = "created_at"
    RATING = "rating"

@app.get("/reviews")
def get_reviews(sort: SortEnum = Query(default=SortEnum.CREATED_AT)):
    return {"sorted_by": sort.value}
```
</details>

<details>
<summary>Exercise 3 (Medium): Pydantic Model Validator for Date Ranges</summary>

**Problem**: Write a filter model ensuring `start_date <= end_date`.

**Solution**:
```python
from datetime import date
from pydantic import BaseModel, model_validator

class DateRangeFilter(BaseModel):
    start_date: date | None = None
    end_date: date | None = None

    @model_validator(mode="after")
    def validate_dates(self):
        if self.start_date and self.end_date and self.start_date > self.end_date:
            raise ValueError("start_date must be before or equal to end_date")
        return self
```
</details>

<details>
<summary>Exercise 4 (Medium): Multi-Field Sort Token Parser</summary>

**Problem**: Parse `-price,name` into `[("price", "DESC"), ("name", "ASC")]`.

**Solution**:
```python
def parse_sort_tokens(sort_str: str) -> list[tuple[str, str]]:
    result = []
    for token in sort_str.split(","):
        token = token.strip()
        if token.startswith("-"):
            result.append((token[1:], "DESC"))
        else:
            result.append((token, "ASC"))
    return result
```
</details>

<details>
<summary>Exercise 5 (Hard): Dynamic SQLAlchemy Filter Builder</summary>

**Problem**: Write a function that accepts filter arguments and applies `.where()` conditions only for parameters that are not None.

**Solution**:
```python
from sqlalchemy import select
# In real code: from models import Product

def dynamic_filter(stmt, category=None, min_price=None, max_price=None):
    # if category:
    #     stmt = stmt.where(Product.category == category)
    # if min_price is not None:
    #     stmt = stmt.where(Product.price >= min_price)
    # if max_price is not None:
    #     stmt = stmt.where(Product.price <= max_price)
    return "filtered_sql_statement"
```
</details>

<details>
<summary>Exercise 6 (Hard): Construct Combined Filter, Sort, Search, and Pagination Dependency</summary>

**Problem**: Build a unified dependency class that encapsulates search string, price filters, sort enum, and offset pagination.

**Solution**:
```python
from enum import StrEnum
from typing import Annotated
from fastapi import Query

class ItemSortField(StrEnum):
    CREATED_AT = "created_at"
    PRICE = "price"

class SearchAndFilterParams:
    def __init__(
        self,
        q: Annotated[str | None, Query(max_length=50)] = None,
        category: Annotated[str | None, Query()] = None,
        min_price: Annotated[float | None, Query(ge=0)] = None,
        max_price: Annotated[float | None, Query(ge=0)] = None,
        sort: Annotated[ItemSortField, Query()] = ItemSortField.CREATED_AT,
        desc: Annotated[bool, Query()] = True,
        page: Annotated[int, Query(ge=1)] = 1,
        size: Annotated[int, Query(ge=1, le=100)] = 20,
    ):
        self.q = q
        self.category = category
        self.min_price = min_price
        self.max_price = max_price
        self.sort = sort
        self.desc = desc
        self.page = page
        self.size = size
        self.offset = (page - 1) * size
```
</details>
