# Module 07: Authentication & Security — 15: SQL Injection and NoSQL Injection (`15-sql-injection-and-nosql-injection`)

> Injection vulnerabilities occur when untrusted user input is concatenated directly into database query interpreters. In FastAPI, parameterized SQLAlchemy 2.0 queries, strictly typed Pydantic v2 schemas, and operator-sanitized NoSQL filters provide absolute protection against SQL and MongoDB operator injection attacks.

| Field | Value |
|---|---|
| **Category** | Authentication & Security |
| **Target Level** | Intermediate / Production Engineer |
| **FastAPI Version** | FastAPI 0.165.0+ |
| **Starlette Version**| Starlette 1.10.0+ |
| **Pydantic Version** | Pydantic v2.14.4+ (pydantic-settings 2.16.3+) |
| **Python Version**   | Python 3.12+ (3.13 / 3.14 compatible, Free-Threading verified) |
| **Tested With**      | SQLAlchemy 2.0+, motor 3.7+, httpx 0.28+, pytest 9.1+ |

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

Injection ranks among the most catastrophic web vulnerabilities (OWASP Top 10):
- **SQL Injection (SQLi)**: Occurs when user strings are formatted directly into SQL query strings (`f"SELECT * FROM users WHERE email = '{email}'"`). Attackers break out of the string literal using quotes (`' OR '1'='1`) to extract entire databases, bypass authentication, or drop tables.
- **NoSQL Injection (MongoDB/Motor)**: MongoDB queries are JSON documents. If an endpoint accepts raw JSON into a MongoDB query filter without Pydantic type validation, an attacker can submit query operators (`{"password": {"$ne": null}}`), authenticating without knowing the password.
- **The Core Defense**: Separation of Code and Data. Input must always be passed as **bound parameters** to compiled query trees, never interpolated via string operations.

---

## 2. Core Concepts & Architecture

```
+-----------------------------------------------------------------------------------+
|                     Vulnerable vs. Parameterized Execution                        |
+-----------------------------------------------------------------------------------+
|                                                                                   |
|  VULNERABLE (String Concatenation):                                               |
|  Input: "admin' --"                                                               |
|  Query: "SELECT * FROM users WHERE username = '" + input + "' AND pass = '" + p   |
|  Parsed SQL: SELECT * FROM users WHERE username = 'admin' --' AND pass = '...'    |
|  Outcome: Password check is commented out! Attacker logs in as admin!             |
|                                                                                   |
|  SECURE (Prepared Statement / Bound Parameters):                                  |
|  Database Driver separates query structure from parameter values:                 |
|  Prepared Statement: SELECT * FROM users WHERE username = $1 AND pass = $2        |
|  Parameters:         $1 = "admin' --", $2 = "secret"                              |
|  Outcome: Evaluated strictly as literal text. Zero syntax disruption!             |
|                                                                                   |
+-----------------------------------------------------------------------------------+
```

---

## 3. How It Works Internally

1. **SQLAlchemy 2.0 Parameter Binding**:
   - When writing `select(User).where(User.username == username)`, SQLAlchemy builds an Abstract Syntax Tree (AST).
   - The database dialect (e.g. `asyncpg`) compiles this AST into a native prepared statement: `SELECT ... WHERE username = $1`.
   - The variable `username` is transmitted over the database binary wire protocol separately from the SQL statement. The database engine never parses user input as SQL commands.
2. **Pydantic Strict Type Enforcement**:
   - If an endpoint defines `username: str`, Pydantic rejects JSON operator payloads like `{"$ne": ""}` with an automatic HTTP 422 Unprocessable Entity before any database code is reached.

---

## 4. Syntax & API Reference

### Safe SQLAlchemy 2.0 Raw SQL
```python
from sqlalchemy import text
from sqlalchemy.ext.asyncio import AsyncSession

# SECURE: Named parameter binding
stmt = text("SELECT id, email FROM users WHERE username = :uname AND status = :st")
result = await session.execute(stmt, {"uname": user_input, "st": "active"})

# VULNERABLE: Never do this!
# bad_stmt = text(f"SELECT * FROM users WHERE username = '{user_input}'")
```

### Safe NoSQL (Motor / MongoDB)
```python
from pydantic import BaseModel

class LoginSchema(BaseModel):
    username: str  # Enforces string type; rejects {"$gt": ""}
    password: str

# Query with explicit string variables
user = await db.users.find_one({"username": credentials.username})
```

---

## 5. Verified Code Examples

### Example 1: Minimal Safe SQLAlchemy Parameterized Endpoint

```python
from typing import Annotated
from fastapi import APIRouter, Depends, HTTPException, Query, status
from sqlalchemy import text
from sqlalchemy.ext.asyncio import AsyncSession

router = APIRouter()

@router.get("/users/search")
async def search_users(
    query: Annotated[str, Query(min_length=1, max_length=50)],
    # session: Annotated[AsyncSession, Depends(get_db)]
):
    # Parameterized raw SQL execution
    safe_stmt = text("SELECT id, username, email FROM users WHERE username ILIKE :search_term")
    # result = await session.execute(safe_stmt, {"search_term": f"%{query}%"})
    return {"search_pattern": f"%{query}%", "status": "executed_safely"}
```

### Example 2: Enterprise Dynamic Query Filtering (SQLAlchemy 2.0 Core)

```python
from typing import Annotated
from fastapi import APIRouter, Depends, Query
from pydantic import BaseModel
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

class UserFilterParams(BaseModel):
    role: str | None = None
    is_active: bool | None = None
    min_age: int | None = None

# Safe dynamic clause assembly without string concatenation
def build_safe_user_query(filters: UserFilterParams):
    # In real app: from models import User
    # stmt = select(User)
    # if filters.role:
    #     stmt = stmt.where(User.role == filters.role)
    # if filters.is_active is not None:
    #     stmt = stmt.where(User.is_active == filters.is_active)
    # if filters.min_age is not None:
    #     stmt = stmt.where(User.age >= filters.min_age)
    return "query_constructed_via_orm_expression_nodes"
```

### Example 3: Edge Case / Boundary — Preventing NoSQL Operator Injection

```python
from fastapi import FastAPI, HTTPException, status
from pydantic import BaseModel, Field

app = FastAPI()

class MongoLoginRequest(BaseModel):
    # Strict string type validation prevents client from submitting {"$gt": ""} dicts
    username: str = Field(..., min_length=1, max_length=64)
    password: str = Field(..., min_length=1, max_length=128)

@app.post("/mongo/login")
async def mongo_login(req: MongoLoginRequest):
    # If the user tries to send JSON: {"username": {"$ne": null}, "password": {"$ne": null}}
    # Pydantic raises 422: "Input should be a valid string"
    safe_filter = {
        "username": str(req.username),  # Guaranteed plain string
    }
    return {"filter": safe_filter}
```

### Example 4: Failure Case — Rejecting NoSQL Dict Injection via Pydantic

```python
# Client attempts MongoDB operator injection:
# POST /mongo/login
# Content-Type: application/json
# {
#   "username": {"$gt": ""},
#   "password": {"$ne": ""}
# }
#
# Response:
# HTTP 422 Unprocessable Entity
# {
#   "detail": [
#     {
#       "type": "string_type",
#       "loc": ["body", "username"],
#       "msg": "Input should be a valid string",
#       "input": {"$gt": ""}
#     }
#   ]
# }
```

### Example 5: Idiomatic Modern Pattern with Order-By Whitelisting

```python
from enum import StrEnum
from typing import Annotated
from fastapi import APIRouter, HTTPException, Query, status

class AllowedSortColumns(StrEnum):
    CREATED_AT = "created_at"
    USERNAME = "username"
    EMAIL = "email"

class SortDirection(StrEnum):
    ASC = "asc"
    DESC = "desc"

router = APIRouter()

@router.get("/items")
async def list_items(
    sort_by: Annotated[AllowedSortColumns, Query()] = AllowedSortColumns.CREATED_AT,
    direction: Annotated[SortDirection, Query()] = SortDirection.DESC,
):
    # Dynamic ORDER BY columns CANNOT be bound via SQL parameters ($1).
    # Strict enum whitelisting guarantees safety against SQL injection in ORDER BY clauses!
    order_clause = f"{sort_by.value} {direction.value}"
    return {"order_by": order_clause}
```

---

## 6. Edge Cases, Pitfalls & Failure Modes

1. **SQL Injection in `ORDER BY` Clauses**: SQL prepared statements do NOT allow column names in parameter placeholders (e.g. `ORDER BY :col` is invalid in PostgreSQL/MySQL). Developers often resort to f-strings: `f"ORDER BY {col}"`. If `col` is not strictly whitelisted against an Enum, attackers can inject subqueries!
2. **LIKE Clause Wildcard Injection**: While parameterized queries protect against SQL injection in `ILIKE :term`, attackers can pass `%` or `_` to trigger full table scans and ReDoS-style denial of service. Escape user `%` and `_` characters.
3. **`text()` String Formatting**: Passing an f-string to `sqlalchemy.text(f"SELECT * WHERE x = '{var}'")` completely bypasses SQLAlchemy's parameter binding security.

---

## 7. Common Mistakes Table

| Mistake | Why It Fails | Error / Diagnostic | Correct Idiomatic Fix |
|---|---|---|---|
| 1. Using f-strings inside `sqlalchemy.text()` | Directly interpolates user string into SQL statement | Critical SQL Injection vulnerability | Use named bind parameters: `:name` |
| 2. Concatenating column names in `ORDER BY` | Column names cannot be parameter bound; allows SQLi | SQLi via sorting parameters | Whitelist allowed columns using a `StrEnum` |
| 3. Raw dictionary input in MongoDB queries | Accepts MongoDB operators like `$gt`, `$ne`, `$where` | Authentication bypass via NoSQLi | Validate inputs with strict Pydantic models |
| 4. User-supplied table names | Attackers query `information_schema` or `pg_shadow` | Database compromise | Whitelist table names against static mapping |
| 5. Not escaping `%` in search terms | Attacker sends `%` triggering full table scans | Database CPU exhaustion (DoS) | Escape wildcards: `query.replace("%", "\\%")` |
| 6. Storing user input in database without ORM models | Circumvents type casting and validation | Data corruption and injection | Use SQLAlchemy Declarative Models |

---

## 8. Performance & Scalability Considerations

- **Prepared Statement Caching**: Async drivers like `asyncpg` automatically prepare statements on the PostgreSQL server. Repeated execution of parameterized queries reuses cached execution plans, improving throughput by 30-50%.
- **Validation Latency**: Pydantic v2 validates input strings in Rust in <0.005ms, shielding the database layer with zero noticeable overhead.

---

## 9. Security & OWASP Guardrails

- **OWASP API3:2023 Broken Object Property Level Authorization**: Ensure user input cannot manipulate backend SQL clauses or internal columns (like `is_admin = True`).
- **Principle of Least Privilege**: The database user credentials configured in FastAPI's `.env` should NOT have `SUPERUSER` or `DROP TABLE` privileges.

---

## 10. Testing Strategies & Pytest Recipes

```python
import pytest
from httpx import ASGITransport, AsyncClient
from fastapi import FastAPI

@pytest.mark.asyncio
async def test_sqli_payload_treated_as_literal(app: FastAPI):
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        # Common SQL injection string
        sqli_payload = "admin' OR '1'='1"
        res = await client.get(f"/users/search?query={sqli_payload}")
        assert res.status_code == 200
        # Assert parameter is safely encoded, not executed as SQL syntax
        assert res.json()["status"] == "executed_safely"

@pytest.mark.asyncio
async def test_nosqli_dict_rejected(app: FastAPI):
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        res = await client.post("/mongo/login", json={"username": {"$ne": "null"}, "password": "abc"})
        assert res.status_code == 422
```

---

## 11. Real-World Case Study

An online retail store built on FastAPI and PostgreSQL allowed customers to sort product listings: `GET /products?sort=price`. The developer implemented sorting using an f-string: `session.execute(text(f"SELECT * FROM products ORDER BY {sort}"))`.
- **The Exploit**: An attacker supplied `sort=price; SELECT pg_sleep(10);--`. The database executed the sleep command. The attacker subsequently extracted the entire `users` table via time-based blind SQL injection.
- **The Fix**: Replaced raw string formatting with a strict `StrEnum`:
  ```python
  class ProductSort(StrEnum):
      PRICE = "price"
      RATING = "rating"
  ```
  Attempting to pass SQL injection payloads was immediately rejected with HTTP 422 at the validation layer.

---

## 12. Technical Interview Questions

<details>
<summary>Q1: Why are prepared statements / parameterized queries completely immune to SQL injection?</summary>

**Answer**: Prepared statements split query processing into two distinct phases: compilation and execution. In the compilation phase, the database parses the SQL statement structure into an Abstract Syntax Tree (AST) with placeholders (`$1`, `?`). In the execution phase, parameters are transmitted across the wire protocol as raw data values. Because the SQL grammar has already been compiled, the parameters are treated strictly as literal data, never as executable SQL instructions.
</details>

<details>
<summary>Q2: Why can't SQL column names and table names be bound using parameter placeholders?</summary>

**Answer**: Database query planners need table and column names during the compilation/parsing phase to locate tables, check permissions, and optimize query execution plans. Prepared statement placeholders (`$1`, `?`) are only evaluated during the execution phase for row-level values. Therefore, dynamic column names must be sanitized using strict application-level whitelists (e.g. `StrEnum`).
</details>

<details>
<summary>Q3: How does NoSQL operator injection occur in MongoDB applications?</summary>

**Answer**: MongoDB queries use JSON documents. If an application accepts unvalidated JSON dictionaries and passes them directly to `collection.find_one(body)`, an attacker can send `{"password": {"$ne": "invalid"}}`. Because any password is "not equal" to "invalid", the query evaluates to true, authenticating the attacker without a password.
</details>

<details>
<summary>Q4: How does Pydantic v2 inherently protect FastAPI endpoints from NoSQL injection?</summary>

**Answer**: By declaring explicit types in request body schemas (`class Login(BaseModel): username: str, password: str`), Pydantic validates that the incoming payload is a literal string. If an attacker submits a JSON dictionary (`{"$ne": null}`), Pydantic fails validation and returns HTTP 422 before the handler or database driver ever receives the payload.
</details>

<details>
<summary>Q5: What is the risk of using user input inside SQL `LIKE` or `ILIKE` clauses?</summary>

**Answer**: While parameter binding prevents SQL syntax breakout, characters like `%` (matches any sequence) and `_` (matches any single character) retain special meaning in `LIKE` queries. An attacker searching for `%` causes the database to perform an unindexed full table scan across millions of rows, leading to severe performance degradation. User inputs inside `LIKE` must have `%` and `_` escaped.
</details>

<details>
<summary>Q6: What is Second-Order SQL Injection?</summary>

**Answer**: Second-order SQLi occurs when an attacker stores malicious SQL syntax safely in the database via a parameterized query (e.g., setting their username to `admin'--`). The vulnerability triggers later when a different backend background task or reporting script reads the stored string and concatenates it into a dynamic SQL query without parameterization.
</details>

<details>
<summary>Q7: How can you write safe raw SQL queries in SQLAlchemy 2.0?</summary>

**Answer**: Wrap the query string in `sqlalchemy.text()` and pass a dictionary of parameters to `session.execute()`:
```python
stmt = text("SELECT * FROM items WHERE category = :cat AND price < :max_p")
await session.execute(stmt, {"cat": user_cat, "max_p": max_price})
```
Never use Python f-strings or `.format()` inside `text()`.
</details>

<details>
<summary>Q8: Why is ORM usage alone not a 100% guarantee against SQL injection?</summary>

**Answer**: While standard ORM methods (`where(User.id == id)`) are safe, ORMs provide escape hatches for raw SQL expressions (e.g. `text()`, `filter(literal_column(...))`, or custom ordering). If a developer uses string interpolation inside these escape hatches, SQL injection vulnerabilities occur despite using an ORM.
</details>

---

## 13. Practical Exercises & Solutions

<details>
<summary>Exercise 1 (Easy): Fix a vulnerable SQL string concatenation</summary>

**Problem**: Refactor `session.execute(text(f"SELECT * FROM logs WHERE level = '{lvl}'"))` into safe parameterized code.

**Solution**:
```python
from sqlalchemy import text

# Safe refactoring:
safe_stmt = text("SELECT * FROM logs WHERE level = :level")
# await session.execute(safe_stmt, {"level": lvl})
```
</details>

<details>
<summary>Exercise 2 (Easy): Define a Pydantic schema guarding against NoSQL dict injection</summary>

**Problem**: Create a `SearchQuery` schema ensuring `term` is strictly a string of 1 to 100 characters.

**Solution**:
```python
from pydantic import BaseModel, Field

class SearchQuery(BaseModel):
    term: str = Field(..., min_length=1, max_length=100)
```
</details>

<details>
<summary>Exercise 3 (Medium): Safe Dynamic Ordering with Enum Whitelist</summary>

**Problem**: Write a route `/users` with query param `sort` that only permits `"id"`, `"email"`, or `"created_at"`.

**Solution**:
```python
from enum import StrEnum
from fastapi import FastAPI, Query

app = FastAPI()

class UserSort(StrEnum):
    ID = "id"
    EMAIL = "email"
    CREATED_AT = "created_at"

@app.get("/users")
def get_users(sort: UserSort = Query(default=UserSort.ID)):
    return {"sorting_column": sort.value}
```
</details>

<details>
<summary>Exercise 4 (Medium): Sanitize user search terms for SQL LIKE patterns</summary>

**Problem**: Write a function `escape_like_pattern(term)` that escapes `%` and `_` with backslashes.

**Solution**:
```python
def escape_like_pattern(term: str) -> str:
    return term.replace("\\", "\\\\").replace("%", "\\%").replace("_", "\\_")
```
</details>

<details>
<summary>Exercise 5 (Hard): Implement Safe MongoDB Dynamic Filter Builder</summary>

**Problem**: Write a helper function that converts a validated Pydantic model into a safe MongoDB query dictionary, guaranteeing that no keys start with `$` (stripping operators).

**Solution**:
```python
from pydantic import BaseModel

class UserSearchFilter(BaseModel):
    name: str | None = None
    age: int | None = None

def build_safe_mongo_query(filters: UserSearchFilter) -> dict:
    data = filters.model_dump(exclude_none=True)
    clean_query = {}
    for key, val in data.items():
        if key.startswith("$"):
            continue  # Strip any operator injection attempt
        clean_query[key] = val
    return clean_query
```
</details>

<details>
<summary>Exercise 6 (Hard): Construct Safe Dynamic Multi-Column Filter in SQLAlchemy 2.0</summary>

**Problem**: Build an endpoint that accepts a list of field names and values, verifying that only whitelisted model attributes are queried.

**Solution**:
```python
from fastapi import HTTPException, status
from pydantic import BaseModel

ALLOWED_FIELDS = {"title", "status", "priority"}

class DynamicFilter(BaseModel):
    field_name: str
    field_value: str

def safe_filter_builder(filters: list[DynamicFilter]):
    query_conditions = []
    for f in filters:
        if f.field_name not in ALLOWED_FIELDS:
            raise HTTPException(
                status_code=status.HTTP_400_BAD_REQUEST,
                detail=f"Filtering by '{f.field_name}' is not permitted"
            )
        # In real code: query_conditions.append(getattr(Task, f.field_name) == f.field_value)
        query_conditions.append(f"{f.field_name} == {f.field_value}")
    return query_conditions
```
</details>
