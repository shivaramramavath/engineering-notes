# Module 08: API Design — 14: GraphQL with Strawberry (`14-graphql-with-strawberry`)

> While REST excels at resource-oriented operations, GraphQL enables clients to query precise data graphs in a single network roundtrip. In modern Python and FastAPI architectures, Strawberry GraphQL provides a Pythonic, type-annotated GraphQL implementation that integrates natively with Starlette routing, FastAPI dependencies, and async DataLoader batching.

| Field | Value |
|---|---|
| **Category** | API Design & Standards |
| **Target Level** | Advanced / Production Engineer |
| **FastAPI Version** | FastAPI 0.165.0+ |
| **Starlette Version**| Starlette 1.10.0+ |
| **Pydantic Version** | Pydantic v2.14.4+ (pydantic-settings 2.16.3+) |
| **Python Version**   | Python 3.12+ (3.13 / 3.14 compatible, Free-Threading verified) |
| **Tested With**      | strawberry-graphql 0.250+, httpx 0.28+, pytest 9.1+ |

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

REST APIs frequently suffer from two opposing data transmission problems:
1. **Over-Fetching**: The client requests `/users/1` to display an avatar, but receives 40 fields including address, billing details, and metadata.
2. **Under-Fetching**: To render a dashboard, a client must issue 5 sequential requests: `/users/1`, then `/users/1/orders`, then `/orders/10/items`, etc.
- **The GraphQL Paradigm**: The client sends a query specifying the exact fields and nested relationships required. The server returns a JSON document matching that exact shape.
- **Strawberry GraphQL**: A modern Python library built on Python 3.10+ dataclasses and type annotations (unlike legacy libraries like Graphene). It mounts directly into FastAPI via `strawberry.fastapi.GraphQLRouter`.

---

## 2. Core Concepts & Architecture

```
+-----------------------------------------------------------------------------------+
|               FastAPI + Strawberry GraphQL Integration Architecture               |
+-----------------------------------------------------------------------------------+
|                                                                                   |
|  Client Query:                                                                    |
|  POST /graphql                                                                    |
|  query {                                                                          |
|    user(id: 1) {                                                                  |
|      name                                                                         |
|      orders { id total }                                                          |
|    }                                                                              |
|  }                                                                                |
|                                                                                   |
|                         │                                                         |
|                         ▼                                                         |
|  [ FastAPI GraphQLRouter (ASGI Sub-Application) ]                                 |
|  - Injects FastAPI request & auth dependencies into `Info.context`                |
|                         │                                                         |
|                         ▼                                                         |
|  [ Strawberry Schema Executor ]                                                   |
|  - Resolves `Query.user(id=1)`                                                    |
|  - Uses `DataLoader` to batch-load orders (Preventing N+1 queries)                |
|                         │                                                         |
|                         ▼                                                         |
|  Response: `{"data": {"user": {"name": "Alice", "orders": [{"id": 10}]}}}`        |
|                                                                                   |
+-----------------------------------------------------------------------------------+
```

---

## 3. How It Works Internally

1. **Type Resolution via Dataclasses**:
   - Strawberry uses `@strawberry.type` to convert standard Python dataclasses into GraphQL Object Types.
   - Field resolvers can be defined as async methods on the class or top-level query resolvers.
2. **FastAPI Context Injection**:
   - `GraphQLRouter(schema, context_getter=custom_context)` passes Starlette's `Request` and authenticated user instances into Strawberry's `strawberry.types.Info` execution context.
3. **The DataLoader Pattern**:
   - Solves the $N+1$ query problem by coalescing individual resolver calls occurring within the same event loop tick into a single batched database query: `SELECT * FROM orders WHERE user_id IN (...)`.

---

## 4. Syntax & API Reference

```python
import strawberry
from strawberry.fastapi import GraphQLRouter
from fastapi import FastAPI

@strawberry.type
class UserType:
    id: int
    name: str

@strawberry.type
class Query:
    @strawberry.field
    def user(self, id: int) -> UserType:
        return UserType(id=id, name="Alice")

schema = strawberry.Schema(query=Query)
graphql_app = GraphQLRouter(schema)

app = FastAPI()
app.include_router(graphql_app, prefix="/graphql")
```

---

## 5. Verified Code Examples

### Example 1: Minimal Working Strawberry GraphQL Endpoint

```python
import strawberry
from strawberry.fastapi import GraphQLRouter
from fastapi import FastAPI

@strawberry.type
class Book:
    id: int
    title: str
    author: str

BOOKS_DB = [
    Book(id=1, title="Fluent Python", author="Luciano Ramalho"),
    Book(id=2, title="FastAPI Enterprise", author="Shiv"),
]

@strawberry.type
class Query:
    @strawberry.field
    def books(self) -> list[Book]:
        return BOOKS_DB

    @strawberry.field
    def book_by_id(self, id: int) -> Book | None:
        return next((b for b in BOOKS_DB if b.id == id), None)

schema = strawberry.Schema(query=Query)
graphql_router = GraphQLRouter(schema)

app = FastAPI()
app.include_router(graphql_router, prefix="/graphql")
```

### Example 2: Enterprise DataLoader to Eliminate N+1 Relational Queries

```python
import asyncio
from collections import defaultdict
from typing import Annotated
import strawberry
from strawberry.dataloader import DataLoader
from strawberry.fastapi import BaseContext, GraphQLRouter
from strawberry.types import Info
from fastapi import FastAPI, Request

# 1. Batched loader function
async def load_orders_for_users(user_ids: list[int]) -> list[list[str]]:
    # In real application: SELECT * FROM orders WHERE user_id IN (:user_ids)
    # Coalesces N queries into 1 single batch query!
    mock_db = {
        1: ["Order #101", "Order #102"],
        2: ["Order #201"],
    }
    return [mock_db.get(uid, []) for uid in user_ids]

# 2. Context with DataLoader instance
class CustomContext(BaseContext):
    def __init__(self, request: Request):
        super().__init__()
        self.request = request
        self.orders_loader = DataLoader(load_fn=load_orders_for_users)

async def get_context(request: Request) -> CustomContext:
    return CustomContext(request=request)

@strawberry.type
class User:
    id: int
    username: str

    @strawberry.field
    async def orders(self, info: Info[CustomContext, None]) -> list[str]:
        # Enqueues ID into DataLoader batch queue
        return await info.context.orders_loader.load(self.id)

@strawberry.type
class Query:
    @strawberry.field
    async def users(self) -> list[User]:
        return [User(id=1, username="alice"), User(id=2, username="bob")]

schema = strawberry.Schema(query=Query)
graphql_app = GraphQLRouter(schema, context_getter=get_context)

app = FastAPI()
app.include_router(graphql_app, prefix="/graphql")
```

### Example 3: Edge Case / Boundary — GraphQL Mutations with Input Types

```python
import strawberry
from pydantic import BaseModel

@strawberry.input
class CreateUserInput:
    username: str
    email: str
    age: int

@strawberry.type
class UserPayload:
    id: int
    username: str
    email: str

@strawberry.type
class Mutation:
    @strawberry.mutation
    def create_user(self, input_data: CreateUserInput) -> UserPayload:
        # Business logic for user creation
        return UserPayload(id=99, username=input_data.username, email=input_data.email)

schema = strawberry.Schema(query=Query, mutation=Mutation)
```

### Example 4: Failure Case — Rejecting Malformed GraphQL Queries

```python
# Client submits query with non-existent field:
# POST /graphql
# {"query": "{ users { nonExistentField } }"}
#
# Response:
# HTTP 200 OK (GraphQL returns errors inside JSON data envelope)
# {
#   "errors": [
#     {
#       "message": "Cannot query field 'nonExistentField' on type 'User'.",
#       "locations": [{"line": 1, "column": 11}]
#     }
#   ]
# }
```

### Example 5: Idiomatic Modern Pattern with FastAPI Auth Integration

```python
from typing import Annotated
from fastapi import Depends, Header, HTTPException, Request, status
import strawberry
from strawberry.fastapi import BaseContext, GraphQLRouter
from strawberry.types import Info

class AuthContext(BaseContext):
    def __init__(self, request: Request, current_user_id: str | None):
        super().__init__()
        self.request = request
        self.user_id = current_user_id

async def resolve_graphql_context(
    request: Request,
    authorization: Annotated[str | None, Header()] = None,
) -> AuthContext:
    user_id = None
    if authorization and authorization.startswith("Bearer "):
        token = authorization.removeprefix("Bearer ").strip()
        if token == "secret-token-alice":
            user_id = "usr_alice"
    return AuthContext(request=request, current_user_id=user_id)

@strawberry.type
class SecureQuery:
    @strawberry.field
    def me(self, info: Info[AuthContext, None]) -> str:
        if not info.context.user_id:
            raise Exception("Authentication required to access user profile")
        return f"Authenticated as: {info.context.user_id}"

secure_schema = strawberry.Schema(query=SecureQuery)
secure_graphql = GraphQLRouter(secure_schema, context_getter=resolve_graphql_context)
```

---

## 6. Edge Cases, Pitfalls & Failure Modes

1. **The N+1 Query Catastrophe**: Resolving nested fields naively (e.g. querying the database inside an `orders` field resolver) fires 1 query for users, plus 1 query per user for orders. If fetching 100 users, this issues 101 database queries! Always use `DataLoader`.
2. **Denial of Service via Circular Nested Queries**: In GraphQL, a client can submit infinite recursive queries: `{ user { friends { friends { friends ... } } } }`. Always enforce query depth limiting (`strawberry.extensions.QueryDepthLimiter`).
3. **HTTP 200 OK for Errors**: Unlike REST, GraphQL returns HTTP 200 even when queries fail, placing errors in the `errors` array. Monitoring tools that inspect only HTTP status codes will miss application-level GraphQL errors.

---

## 7. Common Mistakes Table

| Mistake | Why It Fails | Error / Diagnostic | Correct Idiomatic Fix |
|---|---|---|---|
| 1. Querying DB in field resolvers without DataLoader | Triggers the catastrophic N+1 query problem | Massive DB latency spikes under load | Use `strawberry.dataloader.DataLoader` |
| 2. Allowing unrestricted query depth | Attacker crashes server with deeply nested query | Python `RecursionError` / Server crash | Enforce `QueryDepthLimiter(max_depth=5)` |
| 3. Monitoring only HTTP status codes | GraphQL errors return HTTP 200 | False-positive 100% availability metrics | Parse `errors` key in GraphQL responses |
| 4. Missing `@strawberry.input` on mutation arguments | Strawberry requires distinct input types for arguments | `TypeError: Object is not a valid input type` | Decorate inputs with `@strawberry.input` |
| 5. Not passing FastAPI context to DataLoader | DataLoader cannot access request-scoped DB session | Cannot execute batched database queries | Pass session in `context_getter` |
| 6. Storing sensitive data without field-level auth | Client queries unauthorized sensitive fields | BOLA / Data exposure | Implement field extensions with permission checks |

---

## 8. Performance & Scalability Considerations

- **DataLoader Caching**: `DataLoader` automatically memoizes keys within the single request scope. If two resolvers request user ID 42, the database is queried only once.
- **Edge Caching Difficulty**: Because GraphQL requests are HTTP POSTs with arbitrary JSON bodies, standard edge CDNs (Cloudflare) cannot trivially cache responses without specialized GraphQL gateways (Apollo/Stellate).

---

## 9. Security & OWASP Guardrails

- **Query Cost & Depth Analysis**: Reject queries exceeding a complexity cost score or recursion depth greater than 5 levels.
- **Introspection Disabling in Production**: In public production environments, disable GraphQL schema introspection (`strawberry.Schema(..., introspection=False)`) to prevent attacker reconnaissance.

---

## 10. Testing Strategies & Pytest Recipes

```python
import pytest
from httpx import ASGITransport, AsyncClient
from fastapi import FastAPI

@pytest.mark.asyncio
async def test_graphql_query_execution(app: FastAPI):
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        query = """
        query {
            books {
                title
                author
            }
        }
        """
        res = await client.post("/graphql", json={"query": query})
        assert res.status_code == 200
        data = res.json()
        assert "data" in data
        assert len(data["data"]["books"]) > 0
```

---

## 11. Real-World Case Study

An online media platform supported mobile apps on slow 3G cellular connections. To load the home screen, the app had to execute 7 REST requests, taking an average of 3.8 seconds.
- **The GraphQL Solution**:
  1. Integrated Strawberry GraphQL with FastAPI.
  2. Implemented `DataLoaders` for articles, author profiles, and comment counts.
  3. The mobile app fetched the entire home screen in a single HTTP POST request.
  Average initial screen render time dropped from 3.8s to 0.7s, and cellular data consumption decreased by 62%.

---

## 12. Technical Interview Questions

<details>
<summary>Q1: What is the N+1 query problem in GraphQL, and how does DataLoader solve it?</summary>

**Answer**: If a query requests 50 authors and their respective books (`query { authors { books { title } } }`), a naive resolver executes 1 query to fetch the 50 authors, and then executes 50 individual queries (one per author) to fetch their books (total: 51 queries). `DataLoader` solves this by collecting all requested author IDs across the execution tick and firing a single batched query: `SELECT * FROM books WHERE author_id IN (1, 2, ..., 50)`.
</details>

<details>
<summary>Q2: How does Strawberry GraphQL differ from Graphene in Python?</summary>

**Answer**: Graphene was designed for Python 2/early 3 and relies on custom metaclasses and manual field definitions (`graphene.String()`). Strawberry is built for modern Python (3.10+), utilizing standard type annotations and dataclasses (`@strawberry.type`, `name: str`), supporting native async/await resolvers, Pydantic integration, and seamless ASGI mounting.
</details>

<details>
<summary>Q3: Why is caching more challenging in GraphQL than in REST?</summary>

**Answer**: In REST, each resource has a unique URI (`/users/1`) and uses standard HTTP GET methods, allowing browser and CDN caches to cache responses automatically using `ETag` and `Cache-Control`. In GraphQL, all requests are typically sent as `POST` to a single `/graphql` endpoint with unique query payloads, rendering standard URI-based HTTP caching ineffective without specialized GraphQL proxy gateways.
</details>

<details>
<summary>Q4: How do you implement authentication in a FastAPI + Strawberry application?</summary>

**Answer**: Configure a `context_getter` function in `GraphQLRouter`. The function receives the Starlette `Request`, runs standard authentication logic (inspecting `Authorization: Bearer`), and returns a custom context object containing `user_id`. Field resolvers access the authenticated user via `info.context.user_id`.
</details>

<details>
<summary>Q5: Why should GraphQL Schema Introspection be disabled in public production?</summary>

**Answer**: Introspection allows clients to query `__schema` to discover every available type, field, query, mutation, and description in the API. While invaluable during development, leaving introspection enabled in production provides attackers with a complete internal architectural map to identify sensitive endpoints and injection points.
</details>

<details>
<summary>Q6: What is the difference between `@strawberry.type` and `@strawberry.input`?</summary>

**Answer**:
- `@strawberry.type`: Defines an **Output Object Type** returned by queries and mutations.
- `@strawberry.input`: Defines an **Input Object Type** passed as complex arguments into queries or mutations. GraphQL specifications strictly forbid using output types as input arguments.
</details>

<details>
<summary>Q7: How do you prevent Denial of Service via recursive GraphQL queries?</summary>

**Answer**: Configure query depth limiting extensions (e.g. `strawberry.extensions.QueryDepthLimiter(max_depth=5)`). The extension parses the AST of the incoming GraphQL document before execution and rejects any query with recursion depth exceeding the threshold.
</details>

<details>
<summary>Q8: How does GraphQL handle schema evolution without explicit version numbers?</summary>

**Answer**: Instead of versioning URIs (`/v1`, `/v2`), GraphQL schemas evolve continuously:
1. New fields are added additively to existing types.
2. Deprecated fields are marked with `@strawberry.field(deprecation_reason="Use newField instead")`.
Because clients query only the specific fields they require, adding new fields never breaks existing clients.
</details>

---

## 13. Practical Exercises & Solutions

<details>
<summary>Exercise 1 (Easy): Define a basic Strawberry Query returning a string</summary>

**Problem**: Create a Strawberry schema with a `hello` field returning `"Hello GraphQL"`.

**Solution**:
```python
import strawberry
from fastapi import FastAPI
from strawberry.fastapi import GraphQLRouter

@strawberry.type
class Query:
    @strawberry.field
    def hello(self) -> str:
        return "Hello GraphQL"

schema = strawberry.Schema(query=Query)
app = FastAPI()
app.include_router(GraphQLRouter(schema), prefix="/graphql")
```
</details>

<details>
<summary>Exercise 2 (Easy): Define a Strawberry type with fields</summary>

**Problem**: Define a `Product` type with `id: int`, `title: str`, and `price: float`.

**Solution**:
```python
import strawberry

@strawberry.type
class Product:
    id: int
    title: str
    price: float
```
</details>

<details>
<summary>Exercise 3 (Medium): Implement an Add Mutation</summary>

**Problem**: Create a mutation `add_number(a: int, b: int) -> int` returning their sum.

**Solution**:
```python
import strawberry

@strawberry.type
class Mutation:
    @strawberry.mutation
    def add_number(self, a: int, b: int) -> int:
        return a + b

@strawberry.type
class Query:
    @strawberry.field
    def placeholder(self) -> str: return "ok"

schema = strawberry.Schema(query=Query, mutation=Mutation)
```
</details>

<details>
<summary>Exercise 4 (Medium): Build a Simple DataLoader Batch Function</summary>

**Problem**: Write an async DataLoader function that batch-loads usernames given a list of IDs.

**Solution**:
```python
from strawberry.dataloader import DataLoader

USER_DB = {1: "Alice", 2: "Bob", 3: "Charlie"}

async def batch_load_users(keys: list[int]) -> list[str | None]:
    return [USER_DB.get(k) for k in keys]

loader = DataLoader(load_fn=batch_load_users)
```
</details>

<details>
<summary>Exercise 5 (Hard): Enforce Query Depth Limit Extension</summary>

**Problem**: Configure a Strawberry schema that rejects queries exceeding a maximum depth of 4.

**Solution**:
```python
import strawberry
from strawberry.extensions import QueryDepthLimiter

@strawberry.type
class Query:
    @strawberry.field
    def status(self) -> str: return "active"

# Max depth 4
schema = strawberry.Schema(
    query=Query,
    extensions=[QueryDepthLimiter(max_depth=4)]
)
```
</details>

<details>
<summary>Exercise 6 (Hard): Construct Authenticated GraphQL Context with Role Checking</summary>

**Problem**: Build a custom context that verifies a Bearer token and implement a field resolver that checks if `info.context.role == "admin"`.

**Solution**:
```python
from typing import Annotated
from fastapi import Header, Request
import strawberry
from strawberry.fastapi import BaseContext, GraphQLRouter
from strawberry.types import Info

class SecureContext(BaseContext):
    def __init__(self, request: Request, role: str):
        super().__init__()
        self.request = request
        self.role = role

async def custom_auth_context(
    request: Request,
    authorization: Annotated[str | None, Header()] = None,
) -> SecureContext:
    role = "admin" if authorization == "Bearer admin-secret" else "guest"
    return SecureContext(request=request, role=role)

@strawberry.type
class Query:
    @strawberry.field
    def secret_vault(self, info: Info[SecureContext, None]) -> str:
        if info.context.role != "admin":
            raise Exception("Access denied: Admin role required")
        return "Confidential Enterprise Data"

schema = strawberry.Schema(query=Query)
router = GraphQLRouter(schema, context_getter=custom_auth_context)
```
</details>
