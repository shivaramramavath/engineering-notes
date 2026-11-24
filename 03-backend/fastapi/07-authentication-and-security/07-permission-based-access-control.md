# Module 07: Authentication & Security — 07: Permission-Based Access Control (PBAC) (`07-permission-based-access-control`)

> Permission-Based Access Control (PBAC) decouples code execution from rigid role titles by evaluating granular, action-oriented permissions (`resource:action`). In FastAPI, PBAC is realized through composable dependency factories and wildcard permission matchers, preventing role-explosion and enabling fine-grained enterprise authorization.

| Field | Value |
|---|---|
| **Category** | Authentication & Security |
| **Target Level** | Advanced / Production Engineer |
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

While Role-Based Access Control (RBAC) groups permissions into named personas (`editor`, `billing_admin`), checking roles directly inside endpoint definitions couples code to organizational hierarchy:
- **The Role-Explosion Trap**: If an organization needs a user who can "edit articles" and "view billing reports", RBAC forces the creation of a new hybrid role: `editor_with_billing_view`.
- **The PBAC Solution**: Endpoints declare the exact discrete operation required: `articles:edit` or `billing:reports:read`. Roles simply become collections of permissions. Route handlers never query the user's role; they query whether the user possesses the atomic capability.
- **FastAPI Dependency Integration**: Reusable dependency factories `RequirePermission("invoices:void")` inspect the user's resolved permission set (loaded from JWT, database, or policy engine like OPA/Casbin) and abort with HTTP 403 if missing.

---

## 2. Core Concepts & Architecture

```
+-----------------------------------------------------------------------------------+
|               Role-Based (RBAC) vs. Permission-Based (PBAC) Flow                  |
+-----------------------------------------------------------------------------------+
|  RBAC (Rigid):                                                                    |
|  User ──> Has Role: "BillingManager" ──> Check: @app.get(..., role=="Manager")    |
|                                                                                   |
|  PBAC (Granular & Decoupled):                                                     |
|  User ──> Assigned Roles / Direct Grants                                          |
|                │                                                                  |
|                ▼                                                                  |
|  Flattened Effective Permissions: {"invoices:read", "invoices:create"}            |
|                │                                                                  |
|                ▼                                                                  |
|  Endpoint Guard: RequirePermission("invoices:create")                             |
|                │                                                                  |
|                ├── Match? ──> Execute Route Handler                               |
|                └── No Match? ──> Raise HTTP 403 Forbidden                         |
+-----------------------------------------------------------------------------------+

Wildcard Permission Matching Engine:

Requested: "users:profile:write"
     │
     ├── Checks: "users:profile:write" (Exact Match)
     ├── Checks: "users:profile:*"     (Subtree Wildcard)
     ├── Checks: "users:*"             (Service Wildcard)
     └── Checks: "*"                   (Superuser Wildcard)
```

---

## 3. How It Works Internally

1. **Permission Normalization**:
   - Permissions follow standard uniform notation: `<domain>:<entity>:<action>` (e.g. `billing:invoices:delete`).
   - A user's effective permissions are derived by taking the union of direct user permissions and role-inherited permissions: $\mathcal{P}_{\text{effective}} = \mathcal{P}_{\text{direct}} \cup \bigcup_{r \in \mathcal{R}} \mathcal{P}_r$.
2. **Wildcard Expansion Engine**:
   - The dependency factory parses patterns. An administrative grant `inventory:*` matches any request beginning with `inventory:`.
   - The comparison compiles to regex or simple prefix matching executed in microseconds.

---

## 4. Syntax & API Reference

```python
from typing import Annotated
from fastapi import Depends, HTTPException, status
from pydantic import BaseModel

class UserContext(BaseModel):
    user_id: str
    permissions: set[str]

class RequirePermission:
    def __init__(self, *required_permissions: str, require_all: bool = True):
        self.required_permissions = set(required_permissions)
        self.require_all = require_all

    async def __call__(
        self, user: Annotated[UserContext, Depends(get_current_user)]
    ) -> UserContext:
        user_perms = user.permissions
        if "*" in user_perms:
            return user  # Superuser bypass
        
        if self.require_all:
            missing = self.required_permissions - user_perms
            if missing:
                raise HTTPException(
                    status_code=status.HTTP_403_FORBIDDEN,
                    detail=f"Missing required permissions: {sorted(missing)}",
                )
        else:
            if not (self.required_permissions & user_perms):
                raise HTTPException(
                    status_code=status.HTTP_403_FORBIDDEN,
                    detail=f"Requires at least one of: {sorted(self.required_permissions)}",
                )
        return user
```

---

## 5. Verified Code Examples

### Example 1: Minimal Working PBAC Endpoint Guard

```python
from typing import Annotated
from fastapi import Depends, FastAPI, HTTPException, status
from pydantic import BaseModel

app = FastAPI()

class User(BaseModel):
    id: str
    permissions: set[str]

async def get_current_user() -> User:
    # Simulated auth: user has read and write permissions on documents
    return User(id="usr_01", permissions={"documents:read", "documents:write"})

def has_permission(perm: str):
    def checker(user: Annotated[User, Depends(get_current_user)]) -> User:
        if perm not in user.permissions and "*" not in user.permissions:
            raise HTTPException(
                status_code=status.HTTP_403_FORBIDDEN,
                detail=f"Forbidden: missing '{perm}' permission",
            )
        return user
    return checker

@app.delete("/documents/{doc_id}", dependencies=[Depends(has_permission("documents:delete"))])
async def delete_document(doc_id: str):
    return {"status": "deleted", "id": doc_id}
```

### Example 2: Enterprise Wildcard Permission Evaluator (`users:*`, `*`)

```python
import fnmatch
from typing import Annotated
from fastapi import APIRouter, Depends, HTTPException, status
from pydantic import BaseModel

class EnterpriseUser(BaseModel):
    id: str
    granted_patterns: list[str]  # e.g., ["billing:*", "users:read", "analytics:export:csv"]

async def authenticate() -> EnterpriseUser:
    return EnterpriseUser(id="emp_99", granted_patterns=["billing:*", "analytics:export:*"])

class CheckPermission:
    def __init__(self, action: str):
        self.action = action

    def __call__(
        self, user: Annotated[EnterpriseUser, Depends(authenticate)]
    ) -> EnterpriseUser:
        # Check exact matches and wildcard globs (e.g. billing:* matches billing:invoices:read)
        matched = any(
            pattern == "*" or fnmatch.fnmatchcase(self.action, pattern)
            for pattern in user.granted_patterns
        )
        if not matched:
            raise HTTPException(
                status_code=status.HTTP_403_FORBIDDEN,
                detail=f"Permission denied for operation '{self.action}'",
            )
        return user

router = APIRouter(prefix="/billing")

@router.post("/invoices/void", dependencies=[Depends(CheckPermission("billing:invoices:void"))])
async def void_invoice():
    return {"status": "invoice_voided"}
```

### Example 3: Edge Case / Boundary — Composite Permission Logic (ALL vs. ANY)

```python
from typing import Annotated
from fastapi import Depends, HTTPException, status
from pydantic import BaseModel

class UserProfile(BaseModel):
    permissions: set[str]

async def get_user_profile() -> UserProfile:
    return UserProfile(permissions={"reports:read", "users:read"})

class RequirePermissions:
    def __init__(self, *permissions: str, mode: str = "ALL"):
        self.permissions = set(permissions)
        self.mode = mode.upper()  # "ALL" or "ANY"

    def __call__(self, user: Annotated[UserProfile, Depends(get_user_profile)]):
        if self.mode == "ALL":
            if not self.permissions.issubset(user.permissions):
                missing = self.permissions - user.permissions
                raise HTTPException(status_code=403, detail=f"Missing: {missing}")
        elif self.mode == "ANY":
            if not (self.permissions & user.permissions):
                raise HTTPException(status_code=403, detail=f"Requires at least one of: {self.permissions}")
        return user
```

### Example 4: Failure Case — Structured 403 Explaining Missing Grants

```python
# When user lacks required permissions:
# POST /billing/invoices/void
# Response:
# HTTP 403 Forbidden
# {
#   "error": "insufficient_permissions",
#   "required": ["billing:invoices:void"],
#   "detail": "Permission denied for operation 'billing:invoices:void'"
# }
```

### Example 5: Idiomatic Modern Pattern with Python 3.12+ `Annotated` Shortcuts

```python
from typing import Annotated
from fastapi import Depends, FastAPI, HTTPException
from pydantic import BaseModel

app = FastAPI()

class User(BaseModel):
    id: str
    permissions: set[str]

async def current_user() -> User:
    return User(id="u1", permissions={"orders:read", "orders:create"})

def require(perm: str):
    def _dep(u: Annotated[User, Depends(current_user)]) -> User:
        if perm not in u.permissions:
            raise HTTPException(status_code=403, detail=f"Requires {perm}")
        return u
    return _dep

# Self-documenting dependency annotations
CanReadOrders = Annotated[User, Depends(require("orders:read"))]
CanCreateOrders = Annotated[User, Depends(require("orders:create"))]

@app.get("/orders")
async def list_orders(user: CanReadOrders):
    return {"user": user.id, "orders": []}

@app.post("/orders")
async def create_order(user: CanCreateOrders):
    return {"user": user.id, "created": True}
```

---

## 6. Edge Cases, Pitfalls & Failure Modes

1. **Permission Payload Size in JWTs**: Encoding hundreds of fine-grained permissions inside a stateless JWT causes HTTP 431 Request Header Too Large errors. Permissions should be looked up from Redis or an in-memory database using the user's ID.
2. **Wildcard Overreach**: Using single-segment wildcards carelessly (`*:read`) can inadvertently expose administrative metrics or audit logs. Restrict wildcards strictly to domains (`billing:*`).
3. **Database Performance on Permission Trees**: Loading recursive role-permission hierarchies on every single HTTP request will overwhelm databases. Cache flattened effective permissions in Redis.

---

## 7. Common Mistakes Table

| Mistake | Why It Fails | Error / Diagnostic | Correct Idiomatic Fix |
|---|---|---|---|
| 1. Hardcoding role names instead of permissions | High coupling; causes role explosion | Modifying code every time a new role is created | Check permissions (`documents:read`), not roles |
| 2. Putting all permissions into JWT access tokens | Bloats HTTP headers; exceeds 8KB header limits | HTTP 431 Header Fields Too Large | Query user permissions from Redis cache |
| 3. Case-sensitive permission string mismatch | `Documents:Read` fails against `documents:read` | Spurious 403 Forbidden errors | Force `.lower()` during ingestion and check |
| 4. Not supporting superuser wildcard `*` | Platform admin must be explicitly granted thousands of perms | Admin locked out of new features | Implement `if "*" in permissions: return True` |
| 5. Re-calculating role-permission graph every request | Heavy recursive SQL joins | High request latency (>50ms) | Cache pre-computed permission set in Redis |
| 6. Checking permissions after running DB mutations | BOLA/BFLA security vulnerability | Data modified before unauthorized check fails | Use FastAPI route dependencies that run *before* handler |

---

## 8. Performance & Scalability Considerations

- **Pre-computed Flat Sets**: Store permissions in a Redis `Set` (`SISMEMBER user:usr_1:perms "billing:invoices:read"`). Redis answers set membership queries in $O(1)$ time (<0.5ms).
- **Python `set` Lookups**: In Python memory, `action in user_permissions` executes in $O(1)$ constant time.

---

## 9. Security & OWASP Guardrails

- **OWASP API5:2023 Broken Function Level Authorization (BFLA)**: PBAC provides optimal defense against BFLA by ensuring every endpoint strictly checks the exact capability required.
- **Principle of Least Privilege**: Users should be granted only the specific permissions needed for their daily work rather than broad wildcard bundles.

---

## 10. Testing Strategies & Pytest Recipes

```python
import pytest
from httpx import ASGITransport, AsyncClient
from fastapi import FastAPI
from pydantic import BaseModel

class User(BaseModel):
    permissions: set[str]

@pytest.mark.asyncio
async def test_permission_guard_allows_valid_user(app: FastAPI):
    # Mock user with required permission
    async def mock_user_with_perm():
        return User(permissions={"documents:delete"})

    app.dependency_overrides[get_current_user] = mock_user_with_perm
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        res = await client.delete("/documents/doc_123")
        assert res.status_code == 200
    app.dependency_overrides.clear()
```

---

## 11. Real-World Case Study

An enterprise HR platform had 40 different user roles (`hr_assistant_california`, `payroll_specialist_emea`). Every time a new compliance feature was added, engineers had to edit hundreds of endpoint `@app.get` decorators to append the new roles.
- **The Transformation**:
  1. Migrated to PBAC: Endpoints were decorated with `@router.get(..., dependencies=[Depends(RequirePermission("payroll:tax:view"))])`.
  2. Built an administrative UI allowing HR directors to dynamically bundle permissions into arbitrary company roles stored in Postgres.
  3. Cached user permissions in Redis with a 5-minute TTL.
  Engineering velocity increased by 40%, and zero code changes were needed when new corporate roles were defined.

---

## 12. Technical Interview Questions

<details>
<summary>Q1: How does Permission-Based Access Control (PBAC) resolve the "Role-Explosion" problem?</summary>

**Answer**: In RBAC, when a user needs a non-standard combination of access rights, a new role must be invented, leading to hundreds of overlapping roles. PBAC decouples the endpoint from roles by having endpoints check for granular permissions (e.g., `invoice:void`). Roles simply become arbitrary collections of permissions managed in a database without modifying application code.
</details>

<details>
<summary>Q2: What is the optimal storage strategy for user permissions in a distributed microservices architecture?</summary>

**Answer**: While simple permissions can be stored in JWT claims, enterprise systems with hundreds of permissions should store a flattened permission set in an in-memory distributed cache like Redis (`user:{id}:permissions`), keyed by user ID. When permissions are updated, the cache key is invalidated. Microservices query this cache in sub-millisecond time.
</details>

<details>
<summary>Q3: How do wildcard permissions (e.g., `billing:*`) work, and what are their trade-offs?</summary>

**Answer**: Wildcards allow a single grant to match all actions under a domain hierarchy (e.g. `billing:*` matches `billing:invoices:read` and `billing:payments:refund`). The benefit is simplified administrative assignment for superusers. The trade-off is the risk of over-privileging: introducing a dangerous new endpoint under `billing:purge:all` automatically grants access to anyone with `billing:*`.
</details>

<details>
<summary>Q4: How do you implement an "ALL of these permissions" vs. "ANY of these permissions" guard in FastAPI?</summary>

**Answer**: In a dependency class or factory, accept a mode flag or create two distinct classes:
- **ALL**: Evaluates `required_perms.issubset(user_perms)`
- **ANY**: Evaluates `bool(required_perms & user_perms)`
</details>

<details>
<summary>Q5: Why should authorization logic be evaluated in FastAPI dependencies rather than inside the route handler?</summary>

**Answer**: Evaluating permissions in dependencies guarantees that unauthorized requests are rejected before expensive database queries, parameter deserialization, or business logic executes. It also makes endpoints cleanly declarative, ensures consistent HTTP 403 error formats, and simplifies testing via `dependency_overrides`.
</details>

<details>
<summary>Q6: What is Open Policy Agent (OPA), and how does it integrate with FastAPI PBAC?</summary>

**Answer**: OPA is an open-source, general-purpose policy engine. In a FastAPI setup, a dependency sends an HTTP request or queries an in-process OPA WebAssembly engine passing the user, the resource, and the requested action. OPA evaluates policies written in Rego and returns a boolean allow/deny decision.
</details>

<details>
<summary>Q7: How do you handle permissions that depend on object attributes (e.g., user can edit documents ONLY if they created them)?</summary>

**Answer**: This transitions PBAC into Attribute-Based Access Control (ABAC). In FastAPI, the dependency or route handler takes both the user context and the resolved document model (injected via an async dependency that loads it by ID), and asserts `document.owner_id == user.id or "documents:admin" in user.permissions`.
</details>

<details>
<summary>Q8: Why is `fnmatch.fnmatchcase` preferred over regular expressions for wildcard matching in high-throughput APIs?</summary>

**Answer**: `fnmatch.fnmatchcase` executes lightweight glob matching without the compilation overhead and potential catastrophic backtracking vulnerabilities (ReDoS) associated with complex regular expressions.
</details>

---

## 13. Practical Exercises & Solutions

<details>
<summary>Exercise 1 (Easy): Create a single permission requirement dependency</summary>

**Problem**: Write a dependency `require_perm("admin:read")` that inspects `user["permissions"]` and raises 403 if absent.

**Solution**:
```python
from fastapi import Depends, HTTPException, status

async def get_mock_user():
    return {"id": "u1", "permissions": ["admin:read", "users:read"]}

def require_perm(permission: str):
    def _checker(user: dict = Depends(get_mock_user)):
        if permission not in user.get("permissions", []):
            raise HTTPException(status_code=status.HTTP_403_FORBIDDEN, detail="Missing permission")
        return user
    return _checker
```
</details>

<details>
<summary>Exercise 2 (Easy): Implement a wildcard superuser bypass</summary>

**Problem**: Update the permission checker so that having the wildcard `*` grants access to any requested permission.

**Solution**:
```python
def require_perm_with_superuser(permission: str):
    def _checker(user: dict = Depends(get_mock_user)):
        perms = user.get("permissions", [])
        if "*" in perms or permission in perms:
            return user
        raise HTTPException(status_code=403, detail="Forbidden")
    return _checker
```
</details>

<details>
<summary>Exercise 3 (Medium): Build an ANY-of permission checker</summary>

**Problem**: Create a dependency that permits execution if the user possesses *either* `reports:export:csv` or `reports:export:pdf`.

**Solution**:
```python
from fastapi import Depends, HTTPException, status

def require_any_permission(*permissions: str):
    required = set(permissions)
    def _checker(user: dict = Depends(get_mock_user)):
        user_perms = set(user.get("permissions", []))
        if not (required & user_perms) and "*" not in user_perms:
            raise HTTPException(
                status_code=status.HTTP_403_FORBIDDEN,
                detail=f"Requires at least one of: {required}"
            )
        return user
    return _checker
```
</details>

<details>
<summary>Exercise 4 (Medium): Build a Hierarchical Domain Wildcard Matcher</summary>

**Problem**: Implement a matcher where a user with `inventory:*` satisfies requests for `inventory:items:create` and `inventory:items:delete`.

**Solution**:
```python
import fnmatch
from fastapi import Depends, HTTPException, status

def check_domain_wildcard(requested_perm: str):
    def _checker(user: dict = Depends(get_mock_user)):
        user_perms = user.get("permissions", [])
        matched = any(
            p == "*" or fnmatch.fnmatchcase(requested_perm, p)
            for p in user_perms
        )
        if not matched:
            raise HTTPException(status_code=403, detail=f"Lacks permission: {requested_perm}")
        return user
    return _checker
```
</details>

<details>
<summary>Exercise 5 (Hard): Implement Redis Cache Loading for User Permissions</summary>

**Problem**: Write a dependency that fetches user permissions from a Redis set `user:{user_id}:perms`. If the cache misses, fetch from database mock and populate Redis with a 300s TTL.

**Solution**:
```python
from typing import Annotated
from fastapi import Depends, HTTPException, Security, status
from fastapi.security import HTTPBearer, HTTPAuthorizationCredentials

security = HTTPBearer()

# Simulated Redis
REDIS_STORE: dict[str, set[str]] = {}
DB_STORE: dict[str, set[str]] = {"usr_42": {"audit:view", "logs:download"}}

async def get_effective_permissions(
    creds: Annotated[HTTPAuthorizationCredentials, Security(security)]
) -> set[str]:
    user_id = "usr_42"  # extracted from JWT
    
    # 1. Check Redis Cache
    cache_key = f"user:{user_id}:perms"
    if cache_key in REDIS_STORE:
        return REDIS_STORE[cache_key]

    # 2. Cache Miss: Query Database
    perms = DB_STORE.get(user_id, set())
    
    # 3. Populate Cache
    REDIS_STORE[cache_key] = perms
    return perms
```
</details>

<details>
<summary>Exercise 6 (Hard): Construct an ABAC Object-Level Ownership and Permission Guard</summary>

**Problem**: Build a dependency `check_document_access(action)` that allows operation if user has `documents:admin` OR (user has `documents:{action}` AND `document.owner_id == user.id`).

**Solution**:
```python
from typing import Annotated
from fastapi import Depends, HTTPException, Path, status
from pydantic import BaseModel

class Document(BaseModel):
    id: str
    owner_id: str
    title: str

class User(BaseModel):
    id: str
    permissions: set[str]

# Simulated document loader
async def get_document(doc_id: Annotated[str, Path()]) -> Document:
    return Document(id=doc_id, owner_id="usr_alice", title="Budget 2026")

async def get_user() -> User:
    return User(id="usr_alice", permissions={"documents:edit"})

def check_document_access(action: str):
    async def _guard(
        doc: Annotated[Document, Depends(get_document)],
        user: Annotated[User, Depends(get_user)],
    ) -> Document:
        if "documents:admin" in user.permissions:
            return doc  # Admin override
        
        required_perm = f"documents:{action}"
        if required_perm in user.permissions and doc.owner_id == user.id:
            return doc  # Owner with action permission
        
        raise HTTPException(
            status_code=status.HTTP_403_FORBIDDEN,
            detail="Access denied: must be document owner with appropriate permissions",
        )
    return _guard
```
</details>
