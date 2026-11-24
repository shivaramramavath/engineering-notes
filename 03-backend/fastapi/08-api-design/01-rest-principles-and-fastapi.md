# Module 08: API Design — 01: REST Principles and FastAPI (`01-rest-principles-and-fastapi`)

> Representational State Transfer (REST) is an architectural style governing distributed hypermedia systems. FastAPI aligns with Fielding's REST constraints and the Richardson Maturity Model by decoupling client state, automating OpenAPI representation schemas, and enforcing declarative content negotiation.

| Field | Value |
|---|---|
| **Category** | API Design & Standards |
| **Target Level** | Intermediate / Production Architect |
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

Architect Roy Fielding introduced REST in his 2000 doctoral dissertation. REST is an architectural style, not a protocol or standard. It defines six guiding constraints:
1. **Client-Server**: Separation of user interface concerns from data storage concerns.
2. **Stateless**: Every request from client to server must contain all of the information necessary to understand and complete the request; session state is stored entirely on the client.
3. **Cacheable**: Responses must implicitly or explicitly define themselves as cacheable or non-cacheable (`Cache-Control`, `ETag`).
4. **Uniform Interface**: Resource identification in requests, manipulation of resources through representations, self-descriptive messages, and hypermedia as the engine of application state (HATEOAS).
5. **Layered System**: Hierarchical layers (proxies, CDNs, gateways) operate transparently without client awareness.
6. **Code-on-Demand (Optional)**: Client functionality can be extended via downloadable applets or scripts.

FastAPI operationalizes these constraints by providing declarative HTTP routing, automated schema negotiation via OpenAPI 3.1, and Pydantic data serialization.

---

## 2. Core Concepts & Architecture

```
+-----------------------------------------------------------------------------------+
|               Richardson Maturity Model in FastAPI Architecture                   |
+-----------------------------------------------------------------------------------+
|  Level 3: Hypermedia Controls (HATEOAS)                                           |
|  - Responses include transition links (`_links: {"cancel": "/orders/1/cancel"}`)  |
|                                                                                   |
|  Level 2: HTTP Verbs & Status Codes                                               |
|  - GET, POST, PUT, DELETE, PATCH paired with 200, 201, 204, 400, 404, 409, 422    |
|                                                                                   |
|  Level 1: Distinct Resources                                                      |
|  - Unique URIs for individual entities: `/orders/123`, `/customers/456`           |
|                                                                                   |
|  Level 0: The Swamp of POX (Plain Old XML/JSON)                                   |
|  - Single URI `/api` accepting RPC commands via POST: `{"action": "deleteOrder"}` |
+-----------------------------------------------------------------------------------+
```

---

## 3. How It Works Internally

1. **Routing Mechanics**:
   - `fastapi.APIRouter` parses incoming paths and maps the exact HTTP method (`GET`, `POST`, `PUT`, `DELETE`) to the registered ASGI route endpoint.
   - Path parameters (`/users/{user_id}`) compile to regular expressions during application initialization for sub-microsecond matching.
2. **Automatic OpenAPI Serialization**:
   - FastAPI inspects Pydantic response models, HTTP status codes, and query/path signatures to compile a unified OpenAPI 3.1 JSON document.
   - Content-Type headers (`application/json`) and HTTP status semantics are enforced natively.

---

## 4. Syntax & API Reference

```python
from fastapi import APIRouter, FastAPI, HTTPException, Response, status
from pydantic import BaseModel, Field

router = APIRouter(prefix="/v1/products", tags=["Products"])

class ProductRead(BaseModel):
    id: int
    name: str
    price: float

class ProductCreate(BaseModel):
    name: str = Field(..., min_length=1, max_length=100)
    price: float = Field(..., gt=0)

@router.post("", response_model=ProductRead, status_code=status.HTTP_201_CREATED)
async def create_product(product: ProductCreate, response: Response):
    new_product = ProductRead(id=42, **product.model_dump())
    response.headers["Location"] = f"/v1/products/{new_product.id}"
    return new_product
```

---

## 5. Verified Code Examples

### Example 1: Minimal Resource-Oriented CRUD Router

```python
from typing import Annotated
from fastapi import FastAPI, HTTPException, Path, Response, status
from pydantic import BaseModel

app = FastAPI()

class Item(BaseModel):
    name: str
    description: str | None = None

DATABASE: dict[int, Item] = {1: Item(name="Notebook", description="Spiral bound")}

@app.get("/items/{item_id}", response_model=Item)
def get_item(item_id: Annotated[int, Path()]):
    if item_id not in DATABASE:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="Item not found")
    return DATABASE[item_id]

@app.delete("/items/{item_id}", status_code=status.HTTP_204_NO_CONTENT)
def delete_item(item_id: Annotated[int, Path()]):
    if item_id not in DATABASE:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="Item not found")
    del DATABASE[item_id]
    return Response(status_code=status.HTTP_204_NO_CONTENT)
```

### Example 2: Enterprise Level 3 Hypermedia (HATEOAS) API

```python
from typing import Annotated
from fastapi import APIRouter, HTTPException, Path, Request, status
from pydantic import BaseModel, HttpUrl

router = APIRouter(prefix="/orders")

class Link(BaseModel):
    href: str
    rel: str
    method: str

class OrderResponse(BaseModel):
    id: int
    status: str
    total: float
    links: list[Link]

@router.get("/{order_id}", response_model=OrderResponse)
async def get_order(order_id: Annotated[int, Path()], request: Request):
    base_url = str(request.base_url).rstrip("/")
    current_status = "pending"
    
    # Dynamic hypermedia link state machine
    links = [
        Link(href=f"{base_url}/orders/{order_id}", rel="self", method="GET"),
    ]
    if current_status == "pending":
        links.extend([
            Link(href=f"{base_url}/orders/{order_id}/payment", rel="payment", method="POST"),
            Link(href=f"{base_url}/orders/{order_id}/cancel", rel="cancel", method="POST"),
        ])
    elif current_status == "completed":
        links.append(Link(href=f"{base_url}/orders/{order_id}/receipt", rel="receipt", method="GET"))

    return OrderResponse(id=order_id, status=current_status, total=149.99, links=links)
```

### Example 3: Edge Case / Boundary — Content Negotiation (`Accept` Header)

```python
from fastapi import FastAPI, Header, HTTPException, Response

app = FastAPI()

@app.get("/export/report")
def export_report(accept: str = Header(default="application/json")):
    if "text/csv" in accept:
        csv_data = "id,name,value\n1,Alpha,100\n2,Beta,200"
        return Response(content=csv_data, media_type="text/csv")
    elif "application/json" in accept or "*/*" in accept:
        return [{"id": 1, "name": "Alpha", "value": 100}, {"id": 2, "name": "Beta", "value": 200}]
    else:
        raise HTTPException(
            status_code=406,
            detail="Not Acceptable: Supported media types are application/json and text/csv",
        )
```

### Example 4: Failure Case — Rejecting RPC Verbs in REST URIs

```python
# ANTI-PATTERN: RPC-style endpoint
# POST /api/deleteItem?id=5
#
# RESTFUL IDIOMATIC DESIGN:
# DELETE /items/5
# Response:
# HTTP 204 No Content
```

### Example 5: Idiomatic Modern Pattern with Python 3.12+ `Annotated` Response Headers

```python
from typing import Annotated
from fastapi import APIRouter, Response, status
from pydantic import BaseModel

router = APIRouter(prefix="/articles")

class Article(BaseModel):
    id: int
    title: str

@router.post("", status_code=status.HTTP_201_CREATED, response_model=Article)
def create_article(article_data: Article, response: Response):
    new_article = Article(id=101, title=article_data.title)
    # RFC 9110: 201 Created SHOULD include Location header pointing to the created resource
    response.headers["Location"] = f"/articles/{new_article.id}"
    return new_article
```

---

## 6. Edge Cases, Pitfalls & Failure Modes

1. **RPC-Over-HTTP ("Swamp of POX")**: Using POST for every operation (e.g. `POST /getUser`, `POST /deleteUser`, `POST /updateUser`) violates REST Level 1 and 2, preventing edge caching and breaking HTTP idempotency semantics.
2. **Over-Engineering HATEOAS**: Strict Level 3 HATEOAS requires enormous payload serialization overhead and client complexity. Most modern enterprise mobile/SPA architectures prefer Level 2 (standard REST with OpenAPI documentation).
3. **Missing `Location` Header on 201 Created**: When a resource is successfully created, RFC 9110 mandates or strongly recommends returning a `Location` response header indicating the URI of the newly created resource.

---

## 7. Common Mistakes Table

| Mistake | Why It Fails | Error / Diagnostic | Correct Idiomatic Fix |
|---|---|---|---|
| 1. Putting verbs in URIs (`/getUsers`, `/deleteUser`) | Violates resource-based URI design (nouns, not verbs) | Cluttered, non-standard API surface | Use HTTP methods on nouns: `GET /users`, `DELETE /users/1` |
| 2. Returning HTTP 200 with an error object | Client sees 200 OK; monitoring logs report false success | `{"status": "error", "code": 404}` | Use real HTTP status codes: `HTTP 404 Not Found` |
| 3. Omitting `Location` header on 201 Created | Client must guess URI of newly created resource | Missing standard discovery header | Set `response.headers["Location"] = f"/res/{id}"` |
| 4. Returning response body on HTTP 204 No Content | Violates HTTP RFC 9110; causes client parsing errors | `ProtocolError: Response with 204 must not have body` | Return `Response(status_code=204)` with no body |
| 5. Storing session state in server memory | Breaks horizontal scaling across multiple instances | Sticky session requirement | Store state on client (JWT) or distributed cache (Redis) |
| 6. Not handling `Accept` header conflicts | Server returns JSON when client explicitly asked for XML/CSV | HTTP 406 Not Acceptable ignored | Implement proper content negotiation |

---

## 8. Performance & Scalability Considerations

- **Stateless Scaling**: Conforming strictly to REST's statelessness constraint allows FastAPI instances behind an ALB/Nginx to scale horizontally to hundreds of replicas with zero sticky-session affinity requirements.
- **Cacheability**: Proper use of `Cache-Control: public, max-age=3600` and `ETag` headers allows edge CDNs (Cloudflare, CloudFront) to serve 90%+ of GET traffic without hitting Uvicorn workers.

---

## 9. Security & OWASP Guardrails

- **OWASP API5:2023**: Ensure REST methods (`DELETE`, `PUT`) are protected by explicit authorization dependencies, never assuming obscurity shields endpoints.
- **Strict Method Checking**: FastAPI automatically returns `405 Method Not Allowed` with an `Allow` header if a client sends `POST` to a route registered only as `GET`.

---

## 10. Testing Strategies & Pytest Recipes

```python
import pytest
from httpx import ASGITransport, AsyncClient
from fastapi import FastAPI

@pytest.mark.asyncio
async def test_restful_crud_lifecycle(app: FastAPI):
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        # 1. Create (POST -> 201 Created with Location header)
        create_res = await client.post("/v1/products", json={"name": "Keyboard", "price": 49.99})
        assert create_res.status_code == 201
        assert "location" in create_res.headers

        # 2. Delete (DELETE -> 204 No Content)
        del_res = await client.delete("/items/1")
        assert del_res.status_code == 204
        assert del_res.text == ""
```

---

## 11. Real-World Case Study

An online banking platform's legacy backend exposed 400 endpoints, all mapped as `POST /service` with payloads like `{"action": "GET_BALANCE", "account_id": 123}`. When the company expanded globally, caching was impossible because all requests were POSTs. Origin servers buckled under 15,000 requests/sec.
- **Migration to REST**:
  1. Refactored to resource URIs: `GET /accounts/{account_id}/balance`.
  2. Attached `Cache-Control: private, max-age=15` and `ETag` headers.
  3. Reverse proxies cached balance queries for 15 seconds.
  Origin database load dropped by 78%, and client response latency dropped from 220ms to 8ms.

---

## 12. Technical Interview Questions

<details>
<summary>Q1: What are the four levels of the Richardson Maturity Model?</summary>

**Answer**:
- **Level 0 (The Swamp of POX)**: Single URI, single HTTP method (typically POST) used for all remote procedure calls (RPC).
- **Level 1 (Resources)**: Individual URIs for distinct resources (e.g. `/orders/1`, `/customers/2`), but still relying on a single HTTP method.
- **Level 2 (HTTP Verbs)**: Correct usage of standard HTTP verbs (`GET`, `POST`, `PUT`, `DELETE`, `PATCH`) and standard HTTP status codes (`200`, `201`, `204`, `404`, `409`).
- **Level 3 (Hypermedia Controls / HATEOAS)**: Responses contain hypermedia links informing the client of allowable state transitions and related resource URLs dynamically.
</details>

<details>
<summary>Q2: What is HATEOAS, and why is it rarely fully adopted in commercial REST APIs?</summary>

**Answer**: HATEOAS (Hypermedia As The Engine Of Application State) means the client interacts with a REST application entirely through hypermedia links provided dynamically by the server. Commercial APIs rarely implement strict HATEOAS because modern mobile and Single Page Applications (React/iOS) require strongly typed schemas and static routing definitions (like OpenAPI/TypeScript). Parsing dynamic links adds payload bloat and complex client-side state-machine logic.
</details>

<details>
<summary>Q3: What makes a REST API "Stateless"?</summary>

**Answer**: The stateless constraint dictates that no client session context is stored on the server between requests. Every single incoming request must contain all credentials, identifiers, and parameters necessary to authenticate, authorize, and fulfill the transaction. This enables servers to process requests in isolation and scale horizontally without session replication.
</details>

<details>
<summary>Q4: When should a REST API return HTTP 201 Created vs. HTTP 200 OK on a POST request?</summary>

**Answer**: Return **HTTP 201 Created** when the request successfully creates a new persistent resource with its own URI, accompanied by a `Location` header pointing to that resource. Return **HTTP 200 OK** when a POST request executes an action, calculation, or query that does not result in the creation of a distinct new persistent entity (or returns a processing result).
</details>

<details>
<summary>Q5: What is the purpose of the `Location` response header?</summary>

**Answer**: RFC 9110 specifies that when an HTTP 201 Created response is emitted, the `Location` header provides the absolute or relative URI identifying the newly created resource, enabling clients to immediately fetch or reference it without parsing the response body.
</details>

<details>
<summary>Q6: Why is returning `{"status": "error", "code": 404}` with an HTTP 200 status code an anti-pattern?</summary>

**Answer**: It breaks the HTTP protocol contract. Intermediate infrastructure (CDNs, reverse proxies, API gateways, monitoring agents) rely on the HTTP status code in the header to determine whether a request succeeded or failed. Returning 200 OK masks the failure, causes caches to store error pages as valid responses, and prevents automated alerting systems from detecting outages.
</details>

<details>
<summary>Q7: How does FastAPI implement HTTP 405 Method Not Allowed?</summary>

**Answer**: When a request URI matches a known route path, but the client sends an unsupported HTTP method (e.g. sending `PUT` to a route that only registered `GET`), Starlette's router intercepts the call and automatically responds with HTTP 405 Method Not Allowed and attaches an `Allow` header listing the permitted methods (e.g. `Allow: GET, HEAD`).
</details>

<details>
<summary>Q8: How does REST content negotiation work via the `Accept` header?</summary>

**Answer**: The client specifies its desired response MIME types in the `Accept` header with optional quality factors (e.g. `Accept: application/json, text/csv;q=0.8`). The server inspects this header and formats the response accordingly. If the server cannot satisfy any of the requested representations, it must return **HTTP 406 Not Acceptable**.
</details>

---

## 13. Practical Exercises & Solutions

<details>
<summary>Exercise 1 (Easy): Create a 201 Created response with Location header</summary>

**Problem**: Build a route `/widgets` that returns 201 and sets `Location: /widgets/99`.

**Solution**:
```python
from fastapi import FastAPI, Response, status

app = FastAPI()

@app.post("/widgets", status_code=status.HTTP_201_CREATED)
def create_widget(response: Response):
    response.headers["Location"] = "/widgets/99"
    return {"id": 99, "name": "Gear"}
```
</details>

<details>
<summary>Exercise 2 (Easy): Return clean 204 No Content on deletion</summary>

**Problem**: Create a DELETE route `/widgets/{id}` returning an empty 204 response.

**Solution**:
```python
from fastapi import FastAPI, Response, status

app = FastAPI()

@app.delete("/widgets/{id}", status_code=status.HTTP_204_NO_CONTENT)
def delete_widget(id: int):
    return Response(status_code=status.HTTP_204_NO_CONTENT)
```
</details>

<details>
<summary>Exercise 3 (Medium): Implement Content Negotiation between JSON and Plain Text</summary>

**Problem**: Create a route `/time` that returns JSON if `Accept: application/json` and plain text if `Accept: text/plain`.

**Solution**:
```python
from datetime import datetime, timezone
from fastapi import FastAPI, Header, HTTPException, Response

app = FastAPI()

@app.get("/time")
def get_time(accept: str = Header(default="application/json")):
    now_str = datetime.now(timezone.utc).isoformat()
    if "text/plain" in accept:
        return Response(content=now_str, media_type="text/plain")
    elif "application/json" in accept or "*/*" in accept:
        return {"current_time": now_str}
    raise HTTPException(status_code=406, detail="Not Acceptable")
```
</details>

<details>
<summary>Exercise 4 (Medium): Build a Simple HATEOAS Link Formatter</summary>

**Problem**: Write a function that attaches self and related links to an invoice dictionary.

**Solution**:
```python
def format_invoice_hateoas(inv_id: int, base_url: str) -> dict:
    return {
        "invoice_id": inv_id,
        "amount": 250.0,
        "_links": {
            "self": f"{base_url}/invoices/{inv_id}",
            "customer": f"{base_url}/invoices/{inv_id}/customer",
            "pdf": f"{base_url}/invoices/{inv_id}/pdf",
        }
    }
```
</details>

<details>
<summary>Exercise 5 (Hard): Implement Sub-Resource Routing for 1-to-Many Relationships</summary>

**Problem**: Construct an `APIRouter` hierarchy for `/authors/{author_id}/books` with CRUD operations matching REST standards.

**Solution**:
```python
from fastapi import APIRouter, HTTPException, status
from pydantic import BaseModel

router = APIRouter(prefix="/authors/{author_id}/books")

class Book(BaseModel):
    id: int
    title: str

BOOKS_DB = {1: [Book(id=10, title="Python 101")]}

@router.get("", response_model=list[Book])
def list_author_books(author_id: int):
    return BOOKS_DB.get(author_id, [])

@router.post("", status_code=status.HTTP_201_CREATED, response_model=Book)
def add_author_book(author_id: int, book: Book):
    author_books = BOOKS_DB.setdefault(author_id, [])
    author_books.append(book)
    return book
```
</details>

<details>
<summary>Exercise 6 (Hard): Construct Content Negotiation with RFC 7807 Error Responses</summary>

**Problem**: Build a custom exception handler that inspects `Accept` and formats HTTP 404 errors as either RFC 7807 `application/problem+json` or XML.

**Solution**:
```python
from fastapi import FastAPI, HTTPException, Request, Response
import json

app = FastAPI()

@app.exception_handler(HTTPException)
async def custom_rfc7807_handler(request: Request, exc: HTTPException):
    accept = request.headers.get("Accept", "")
    if "application/xml" in accept:
        xml_content = f"<problem><status>{exc.status_code}</status><detail>{exc.detail}</detail></problem>"
        return Response(content=xml_content, media_type="application/problem+xml", status_code=exc.status_code)
    
    # Default problem+json
    data = {
        "type": "about:blank",
        "title": "HTTP Error",
        "status": exc.status_code,
        "detail": exc.detail,
        "instance": str(request.url.path),
    }
    return Response(
        content=json.dumps(data),
        media_type="application/problem+json",
        status_code=exc.status_code,
    )
```
</details>
