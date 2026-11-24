# Module 08: API Design — 02: Resource Naming and URI Design (`02-resource-naming-and-uri-design`)

> Well-architected URIs provide intuitive, predictable, and durable resource identifiers. In FastAPI, resource hierarchies, relationship modeling, and non-CRUD operational transitions are expressed cleanly using plural nouns, kebab-case path conventions, and sub-resource nesting limits.

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

A Uniform Resource Identifier (URI) identifies a discrete computational resource. Consistent URI design reduces developer cognitive load and client SDK complexity:
- **Plural Nouns**: Collections are represented by plural nouns (`/customers`, `/orders`, `/audit-logs`). A specific item is accessed by appending its identifier: `/customers/{customer_id}`.
- **Kebab-Case Paths**: URIs are lowercase and hyphen-separated (`/payment-methods`, `/api-keys`). Underscores and camelCase in paths are discouraged.
- **Nesting Rules (The Two-Level Rule)**: Deeply nested paths like `/orgs/{o}/departments/{d}/teams/{t}/projects/{p}/tasks/{task_id}` are unmaintainable and brittle. Flatten deep relationships: `/tasks/{task_id}`.
- **Modeling Non-CRUD Actions**: Operations like "archive", "cancel", or "publish" can be modeled by reifying the process into a sub-resource: `POST /orders/{id}/cancellation` or by treating state updates via `PATCH /orders/{id}` with `{"status": "cancelled"}`.

---

## 2. Core Concepts & Architecture

```
+-----------------------------------------------------------------------------------+
|               URI Structure & Hierarchy Taxonomy                                  |
+-----------------------------------------------------------------------------------+
|  Resource Type    | Pattern                               | Example               |
+-------------------+---------------------------------------+-----------------------+
| Collection        | `/{resources}`                        | `/customers`          |
| Single Entity     | `/{resources}/{id}`                   | `/customers/cust_123` |
| Sub-Collection    | `/{resources}/{id}/{sub_resources}`   | `/customers/1/orders` |
| Single Sub-Entity | `/{resources}/{id}/{sub_res}/{sub_id}`| `/customers/1/orders/9`|
| Reified Action    | `/{resources}/{id}/{process_noun}`    | `/orders/9/refunds`   |
+-------------------+---------------------------------------+-----------------------+

Flattening Deeply Nested Hierarchies:

AVOID (Deep Nesting > 2 Levels):
GET /organizations/org_1/teams/team_4/repositories/repo_9/branches/main/commits
                                    │
                                    ▼
RECOMMENDED (Flatten to Global Entity Identifier):
GET /repositories/repo_9/commits
GET /commits/{commit_sha}
```

---

## 3. How It Works Internally

1. **Starlette Path Regex Compilation**:
   - When an `APIRouter` registers `/{resource_id}`, Starlette converts the path template into a regular expression: `^/(?P<resource_id>[^/]+)$`.
   - FastAPI parses matched groups and coerces them into Pydantic-annotated path parameter types (e.g., `UUID`, `int`).
2. **Trailing Slash Normalization**:
   - By default, Starlette redirects trailing slash mismatches (`/items/` vs `/items`) with a 307 Temporary Redirect unless explicitly configured. Consistent endpoint naming avoids unnecessary client redirect hops.

---

## 4. Syntax & API Reference

```python
from typing import Annotated
from uuid import UUID
from fastapi import APIRouter, Path, status
from pydantic import BaseModel

router = APIRouter(prefix="/customers", tags=["Customers"])

# Collection endpoint (plural noun)
@router.get("", response_model=list[CustomerRead])
async def list_customers():
    ...

# Item endpoint (UUID path parameter)
@router.get("/{customer_id}", response_model=CustomerRead)
async def get_customer(
    customer_id: Annotated[UUID, Path(description="Globally unique customer identifier")]
):
    ...

# Nested sub-resource endpoint (max 2 levels)
@router.get("/{customer_id}/orders", response_model=list[OrderRead])
async def list_customer_orders(customer_id: UUID):
    ...
```

---

## 5. Verified Code Examples

### Example 1: Minimal Resource & Sub-Resource Router

```python
from typing import Annotated
from fastapi import APIRouter, FastAPI, HTTPException, Path
from pydantic import BaseModel

app = FastAPI()

class Comment(BaseModel):
    id: int
    text: str

class Post(BaseModel):
    id: int
    title: str

POSTS_DB = {1: Post(id=1, title="FastAPI 2026")}
COMMENTS_DB = {1: [Comment(id=101, text="Great post!")]}

@app.get("/posts/{post_id}")
def get_post(post_id: Annotated[int, Path(ge=1)]):
    if post_id not in POSTS_DB:
        raise HTTPException(status_code=404, detail="Post not found")
    return POSTS_DB[post_id]

# Sub-resource representation
@app.get("/posts/{post_id}/comments")
def get_post_comments(post_id: Annotated[int, Path(ge=1)]):
    if post_id not in POSTS_DB:
        raise HTTPException(status_code=404, detail="Post not found")
    return COMMENTS_DB.get(post_id, [])
```

### Example 2: Enterprise Reified Business Action (Reifying Actions into Resources)

```python
from datetime import datetime, timezone
from typing import Annotated
from fastapi import APIRouter, HTTPException, Path, status
from pydantic import BaseModel, Field

router = APIRouter(prefix="/invoices", tags=["Invoices"])

class CancellationRecord(BaseModel):
    cancellation_id: str
    invoice_id: int
    reason: str
    cancelled_at: datetime

class CancelInvoiceRequest(BaseModel):
    reason: str = Field(..., min_length=5, max_length=200)

# Rather than POST /invoices/123/cancel (RPC verb),
# reify into a sub-resource: POST /invoices/123/cancellations
@router.post(
    "/{invoice_id}/cancellations",
    response_model=CancellationRecord,
    status_code=status.HTTP_201_CREATED,
)
def cancel_invoice(
    invoice_id: Annotated[int, Path()],
    request_data: CancelInvoiceRequest,
):
    # Process invoice cancellation
    return CancellationRecord(
        cancellation_id="cncl_9988",
        invoice_id=invoice_id,
        reason=request_data.reason,
        cancelled_at=datetime.now(timezone.utc),
    )
```

### Example 3: Edge Case / Boundary — Handling Matrix Parameters vs Query Strings

```python
from typing import Annotated
from fastapi import FastAPI, Query

app = FastAPI()

# REST anti-pattern: matrix parameters (/users;status=active;role=admin)
# REST best practice: standardized query string parameters
@app.get("/users")
def query_users(
    status: Annotated[str | None, Query(description="Filter by user status")] = None,
    role: Annotated[str | None, Query(description="Filter by user role")] = None,
):
    return {"filter_status": status, "filter_role": role}
```

### Example 4: Failure Case — Rejecting Verb-Based RPC URIs

```python
# POOR URI DESIGN:
# POST /api/runPaymentProcess
# GET /api/fetch_user_by_id?id=12
#
# CLEAN REST DESIGN:
# POST /payments
# GET /users/12
```

### Example 5: Idiomatic Modern Pattern with Kebab-Case Multi-Word Sub-Resources

```python
from typing import Annotated
from fastapi import APIRouter, Path
from pydantic import BaseModel

router = APIRouter(prefix="/credit-cards", tags=["Payment Methods"])

class SecurityAuditRecord(BaseModel):
    audit_id: str
    status: str

# Kebab-case path for multi-word sub-resource
@router.get("/{card_id}/security-audits", response_model=list[SecurityAuditRecord])
def list_card_audits(card_id: Annotated[str, Path()]):
    return [SecurityAuditRecord(audit_id="aud_1", status="passed")]
```

---

## 6. Edge Cases, Pitfalls & Failure Modes

1. **Inconsistent Casing**: Mixing casing conventions (e.g. `/userProfiles/`, `/user_profiles/`, `/user-profiles/`) creates fragmented client SDKs and cache fragmentation. Enforce strict `kebab-case` for URIs.
2. **Deeply Nested Hierarchy Traps**: Writing routes like `/companies/{c_id}/warehouses/{w_id}/aisles/{a_id}/shelves/{s_id}/items/{i_id}` makes client requests excessively verbose and couples routing to internal database relational depth.
3. **Singular vs Plural Inconsistency**: Having `/user/{id}` alongside `/products/{id}` violates the Principle of Least Astonishment. Use plural nouns universally for collections.

---

## 7. Common Mistakes Table

| Mistake | Why It Fails | Error / Diagnostic | Correct Idiomatic Fix |
|---|---|---|---|
| 1. Mixing singular and plural collections | Inconsistent API ergonomics | `/user/1` vs `/orders/1` | Enforce plural nouns universally: `/users/1` |
| 2. CamelCase or snake_case in URI paths | Breaks RFC 3986 recommendations and casing standards | `/user_profiles` or `/userProfiles` | Use kebab-case: `/user-profiles` |
| 3. Deep nesting (> 2 levels) | Brittle URIs; overly coupled to relational schema | `/a/{id}/b/{id}/c/{id}/d/{id}` | Flatten: `/d/{id}` or `/c/{id}/d` |
| 4. Including file extensions in URIs (`/users.json`) | Violates content negotiation standards | Obsolete RPC convention | Use `Accept: application/json` header |
| 5. Trailing slash discrepancies | Causes unnecessary 307 Redirects | `/items/` redirects to `/items` | Standardize without trailing slash |
| 6. Using path parameters for filtering | Breaks URI resource identity | `/users/active` (where active is filter) | Use query params: `/users?status=active` |

---

## 8. Performance & Scalability Considerations

- **URL Normalization**: Ensure API gateways strip trailing slashes and normalize lowercase paths to maximize CDN cache hit ratios.
- **Route Compilation**: Keep path templates concise to minimize Starlette's regex group matching overhead.

---

## 9. Security & OWASP Guardrails

- **Path Traversal Attacks**: Path parameters accepting strings must never be fed directly into filesystem paths without validation (e.g. `/files/{filename}` where filename is `../../etc/passwd`). Validate with Pydantic regex or UUIDs.
- **Enumeration Resistance**: Use UUIDv4 or high-entropy prefixed identifiers (`usr_7a8b9c...`) in paths instead of auto-incrementing sequential integers (1, 2, 3) to prevent BOLA enumeration.

---

## 10. Testing Strategies & Pytest Recipes

```python
import pytest
from httpx import ASGITransport, AsyncClient
from fastapi import FastAPI

@pytest.mark.asyncio
async def test_kebab_case_uri_resolution(app: FastAPI):
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        res = await client.get("/credit-cards/card_123/security-audits")
        assert res.status_code == 200
        assert isinstance(res.json(), list)
```

---

## 11. Real-World Case Study

A cloud infrastructure platform designed its early APIs around Kubernetes-style deep hierarchies: `/clusters/{c}/namespaces/{n}/deployments/{d}/pods/{p}/containers/{container_id}`. Client SDKs required 6 parameters just to inspect container logs, and refactoring cluster tenancy broke every API integration.
- **The Redesign**:
  1. Assigned global UUID identifiers to all containers (`cnt_998877`).
  2. Flattened the API: `GET /containers/{container_id}/logs`.
  3. Preserved filtering via query parameters: `GET /containers?cluster_id=c1&namespace=n1`.
  Client SDK lines of code dropped by 60%, and API request latency decreased by 18ms.

---

## 12. Technical Interview Questions

<details>
<summary>Q1: Why is kebab-case preferred over snake_case or camelCase in URI path design?</summary>

**Answer**: Domain names and URIs are traditionally case-insensitive in many network layers and DNS systems. Hyphens (`-`) are standard word delimiters in URLs (e.g. in search engine optimization). Underscores (`_`) can be partially obscured by hyperlinks in browser interfaces, while camelCase introduces subtle capitalization errors across case-insensitive file servers and proxies.
</details>

<details>
<summary>Q2: How should an engineer handle business operations that do not map directly to CRUD?</summary>

**Answer**: Two standard patterns:
1. **State Update via PATCH**: Treat the operation as a state change on the entity: `PATCH /orders/123` with payload `{"status": "cancelled"}`.
2. **Reification (Creation of a Sub-Resource)**: Turn the action into a first-class noun resource: `POST /orders/123/cancellations` or `POST /transactions/txn_1/refunds`. This preserves REST Level 2 semantics and allows the audit trail of the action to be inspected later.
</details>

<details>
<summary>Q3: When should information be placed in Path Parameters versus Query Parameters?</summary>

**Answer**:
- **Path Parameters**: Used for **Resource Identification** (locating a specific entity in a hierarchy), e.g. `/users/{user_id}/orders/{order_id}`.
- **Query Parameters**: Used for **Filtering, Sorting, Pagination, and Search**, e.g. `/orders?status=shipped&sort=date&limit=20`.
</details>

<details>
<summary>Q4: What is the "Two-Level Rule" in REST API design?</summary>

**Answer**: A URI should rarely exceed two levels of resource nesting: `/parents/{parent_id}/children/{child_id}`. Deeper nesting (`/a/{id}/b/{id}/c/{id}/d/{id}`) tightly couples URIs to database foreign key hierarchies, makes client calls overly complex, and prevents resources from having clean global identifiers. Deeper sub-resources should be flattened to top-level resources.
</details>

<details>
<summary>Q5: Why should database auto-incrementing integer IDs be avoided in public URI path parameters?</summary>

**Answer**: Sequential integer IDs (1, 2, 3...) leak business intelligence (e.g. a competitor can track daily order volume by creating test accounts) and facilitate BOLA/IDOR enumeration attacks. Public URIs should use UUIDv4 or prefixed identifiers (e.g. `cus_9a8b7c6d`).
</details>

<details>
<summary>Q6: How does FastAPI handle trailing slashes in route paths by default?</summary>

**Answer**: Starlette and FastAPI normalize trailing slashes by issuing an HTTP 307 Temporary Redirect if a client requests `/users/` when the route was registered as `/users` (or vice versa). To eliminate redirect latency, APIs should strictly standardize on routes without trailing slashes.
</details>

<details>
<summary>Q7: Should plural or singular nouns be used for collection URIs?</summary>

**Answer**: Plural nouns are standard across the software industry (e.g. `/users`, `/accounts`, `/subscriptions`). It clearly signifies that the endpoint represents a collection of resources, while appending an ID (`/users/42`) naturally represents an item within that collection.
</details>

<details>
<summary>Q8: How should bulk operations (e.g., bulk update or bulk delete) be represented in URI design?</summary>

**Answer**: Map bulk operations to the collection endpoint:
- `PATCH /items`: Accepts an array of item update objects.
- `DELETE /items`: Accepts an array of item IDs in the request body (or via query filter `?ids=1,2,3`).
- Alternatively, reify into a batch job: `POST /batch-jobs` with type `item_deletion`.
</details>

---

## 13. Practical Exercises & Solutions

<details>
<summary>Exercise 1 (Easy): Create a kebab-case collection router</summary>

**Problem**: Define an APIRouter for `shipping-addresses` with a GET route.

**Solution**:
```python
from fastapi import APIRouter

router = APIRouter(prefix="/shipping-addresses", tags=["Shipping"])

@router.get("")
def list_shipping_addresses():
    return []
```
</details>

<details>
<summary>Exercise 2 (Easy): Flatten a 3-level deep path</summary>

**Problem**: Refactor `/orgs/{o_id}/teams/{t_id}/members/{m_id}` into a flattened route.

**Solution**:
```python
from fastapi import APIRouter

router = APIRouter(prefix="/members", tags=["Members"])

@router.get("/{member_id}")
def get_member(member_id: str):
    return {"member_id": member_id}
```
</details>

<details>
<summary>Exercise 3 (Medium): Reify a "Publish" Action into a Resource</summary>

**Problem**: Create an endpoint that models publishing an article as a `POST /articles/{id}/publications` resource.

**Solution**:
```python
from datetime import datetime, timezone
from fastapi import FastAPI, status
from pydantic import BaseModel

app = FastAPI()

class Publication(BaseModel):
    article_id: int
    published_at: datetime

@app.post("/articles/{article_id}/publications", status_code=status.HTTP_201_CREATED)
def publish_article(article_id: int):
    return Publication(article_id=article_id, published_at=datetime.now(timezone.utc))
```
</details>

<details>
<summary>Exercise 4 (Medium): Enforce UUIDv4 Path Parameters with Pydantic</summary>

**Problem**: Write a route `/tenants/{tenant_id}` that validates `tenant_id` as a valid UUIDv4.

**Solution**:
```python
from uuid import UUID
from fastapi import FastAPI, Path

app = FastAPI()

@app.get("/tenants/{tenant_id}")
def get_tenant(tenant_id: UUID = Path(...)):
    return {"tenant_uuid": str(tenant_id)}
```
</details>

<details>
<summary>Exercise 5 (Hard): Implement Sub-Resource Routing with Context Injection</summary>

**Problem**: Implement sub-resource routing where `/departments/{dept_id}/employees` injects the verified `Department` model into the employee listing handler.

**Solution**:
```python
from typing import Annotated
from fastapi import APIRouter, Depends, HTTPException, Path
from pydantic import BaseModel

class Department(BaseModel):
    id: int
    name: str

DEPTS = {1: Department(id=1, name="Engineering")}

def get_department(dept_id: Annotated[int, Path()]):
    dept = DEPTS.get(dept_id)
    if not dept:
        raise HTTPException(status_code=404, detail="Department not found")
    return dept

router = APIRouter(prefix="/departments/{dept_id}/employees")

@router.get("")
def list_employees(dept: Annotated[Department, Depends(get_department)]):
    return {"department": dept.name, "employees": ["Alice", "Bob"]}
```
</details>

<details>
<summary>Exercise 6 (Hard): Construct Bulk Operation Resource Handler</summary>

**Problem**: Build a `POST /batch-deletions` endpoint accepting a list of entity IDs, returning an asynchronous batch tracking resource with HTTP 202 Accepted.

**Solution**:
```python
import uuid
from fastapi import FastAPI, status
from pydantic import BaseModel

app = FastAPI()

class BatchDeleteRequest(BaseModel):
    entity_type: str
    target_ids: list[str]

class BatchJobStatus(BaseModel):
    job_id: str
    status: str
    item_count: int

@app.post("/batch-deletions", status_code=status.HTTP_202_ACCEPTED, response_model=BatchJobStatus)
def create_batch_deletion(batch_req: BatchDeleteRequest):
    job_id = f"job_{uuid.uuid4().hex[:8]}"
    # In real code: enqueue background deletion task
    return BatchJobStatus(job_id=job_id, status="queued", item_count=len(batch_req.target_ids))
```
</details>
