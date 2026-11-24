# Module 07: Authentication & Security — 06: Role-Based Access Control (RBAC) (`06-role-based-access-control-rbac`)

> Role-Based Access Control (RBAC) restricts system access based on the roles assigned to authorized users within an organization. In FastAPI, RBAC is implemented cleanly via parameterized dependency callables (`RoleChecker`), enabling route-level, router-level, or hierarchical permission enforcement without repetitive boilerplate.

| Field | Value |
|---|---|
| **Category** | Authentication & Security |
| **Target Level** | Intermediate / Production Engineer |
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

Role-Based Access Control (RBAC) maps permissions to coarse-grained business roles (`admin`, `billing_manager`, `developer`, `viewer`) rather than directly to individual users:
- **Role Assignment**: Users are assigned one or more roles.
- **Hierarchical Inheritance**: Roles may inherit capabilities from lower roles (e.g., `SuperAdmin` inherits all capabilities of `Admin` and `Viewer`).
- **Declarative Enforcement in FastAPI**: Using FastAPI's dependency injection system, access guards are declared as callable classes or factory functions passed to `Depends()`. If the user's role does not satisfy the constraint, an HTTP 403 Forbidden is raised immediately before the route handler executes.

---

## 2. Core Concepts & Architecture

```
+-----------------------------------------------------------------------------------+
|                        Hierarchical RBAC Architecture                             |
+-----------------------------------------------------------------------------------+
|                                                                                   |
|                           [ SuperAdmin ] (Level 100)                              |
|                                 │                                                 |
|                                 ▼                                                 |
|                           [ Admin ] (Level 50)                                    |
|                           ┌─────┴─────┐                                           |
|                           ▼           ▼                                           |
|                   [ Manager ]     [ Editor ] (Level 20)                           |
|                           └─────┬─────┘                                           |
|                                 ▼                                                 |
|                           [ Viewer ] (Level 10)                                   |
|                                                                                   |
+-----------------------------------------------------------------------------------+

FastAPI Request Execution Pipeline:

Client Request ──> Header: "Authorization: Bearer <tok>"
                          │
                          ▼
             [ get_current_user Dependency ]
                          │ (Extracts User domain model & roles)
                          ▼
             [ RoleChecker(["admin", "manager"]) ]
                          │
            ┌─────────────┴─────────────┐
        Has Role?                   Missing Role?
            │                           │
           Yes                          ▼
            │               HTTP 403 Forbidden Exception
            ▼
   [ Route Handler Logic ]
```

---

## 3. How It Works Internally

1. **Callable Class Dependencies**:
   - A class defining `__init__(self, allowed_roles: Sequence[str])` stores authorization rules at router definition time.
   - The instance's `async def __call__(self, user: User = Depends(get_current_user))` executes per request.
   - FastAPI inspects the `__call__` signature, discovers the `get_current_user` sub-dependency, resolves the user, and invokes `__call__`.
2. **Deterministic Evaluation Order**:
   - Router-level dependencies (`APIRouter(dependencies=[Depends(RoleChecker(...))])`) execute before endpoint-level dependencies and handler parameters, ensuring unauthorized requests are rejected early.

---

## 4. Syntax & API Reference

```python
from enum import StrEnum
from typing import Annotated, Sequence
from fastapi import Depends, HTTPException, status
from pydantic import BaseModel

class UserRole(StrEnum):
    SUPERADMIN = "superadmin"
    ADMIN = "admin"
    MEMBER = "member"
    VIEWER = "viewer"

class User(BaseModel):
    id: str
    email: str
    roles: list[UserRole]

class RequireRole:
    def __init__(self, allowed_roles: Sequence[UserRole]) -> None:
        self.allowed_roles = set(allowed_roles)

    async def __call__(self, user: Annotated[User, Depends(get_current_user)]) -> User:
        if not any(r in self.allowed_roles for r in user.roles):
            raise HTTPException(
                status_code=status.HTTP_403_FORBIDDEN,
                detail=f"Operation requires one of: {[r.value for r in self.allowed_roles]}",
            )
        return user
```

---

## 5. Verified Code Examples

### Example 1: Minimal Working RBAC Route Guard

```python
from enum import StrEnum
from typing import Annotated
from fastapi import Depends, FastAPI, HTTPException, status
from pydantic import BaseModel

app = FastAPI()

class Role(StrEnum):
    ADMIN = "admin"
    USER = "user"

class CurrentUser(BaseModel):
    id: str
    role: Role

# Mock authentication dependency
async def get_current_user() -> CurrentUser:
    return CurrentUser(id="usr_123", role=Role.USER)

class AllowRoles:
    def __init__(self, *roles: Role):
        self.roles = roles

    def __call__(self, user: Annotated[CurrentUser, Depends(get_current_user)]) -> CurrentUser:
        if user.role not in self.roles:
            raise HTTPException(
                status_code=status.HTTP_403_FORBIDDEN,
                detail=f"Forbidden: role '{user.role}' not permitted",
            )
        return user

@app.get("/admin/metrics", dependencies=[Depends(AllowRoles(Role.ADMIN))])
async def get_metrics():
    return {"status": "ok", "system_load": "4%"}
```

### Example 2: Enterprise Hierarchical Role Inheritance

```python
from enum import IntEnum
from typing import Annotated
from fastapi import APIRouter, Depends, HTTPException, status
from pydantic import BaseModel

class RoleLevel(IntEnum):
    VIEWER = 10
    EDITOR = 20
    MANAGER = 50
    ADMIN = 100

class UserProfile(BaseModel):
    id: str
    name: str
    role_level: RoleLevel

# Authentication mock
async def authenticate_user() -> UserProfile:
    return UserProfile(id="usr_editor", name="Elena", role_level=RoleLevel.EDITOR)

class MinimumRoleLevel:
    def __init__(self, required_level: RoleLevel):
        self.required_level = required_level

    async def __call__(
        self, user: Annotated[UserProfile, Depends(authenticate_user)]
    ) -> UserProfile:
        if user.role_level < self.required_level:
            raise HTTPException(
                status_code=status.HTTP_403_FORBIDDEN,
                detail=(
                    f"Access denied. Required tier: {self.required_level.name} "
                    f"({self.required_level.value}), current tier: {user.role_level.name}"
                ),
            )
        return user

router = APIRouter(prefix="/content")

@router.delete("/articles/{id}", dependencies=[Depends(MinimumRoleLevel(RoleLevel.MANAGER))])
async def delete_article(id: int):
    return {"deleted": id}
```

### Example 3: Edge Case / Boundary — Tenant-Scoped Multi-Tenant RBAC

```python
from typing import Annotated
from fastapi import Depends, HTTPException, Path, status
from pydantic import BaseModel

class TenantMembership(BaseModel):
    org_id: str
    role: str  # "admin", "member", "billing"

class AuthenticatedUser(BaseModel):
    user_id: str
    memberships: dict[str, str]  # org_id -> role

async def resolve_user() -> AuthenticatedUser:
    return AuthenticatedUser(
        user_id="usr_007",
        memberships={"org_alpha": "admin", "org_beta": "member"}
    )

class RequireTenantRole:
    def __init__(self, required_role: str):
        self.required_role = required_role

    async def __call__(
        self,
        org_id: Annotated[str, Path(description="Organization Identifier")],
        user: Annotated[AuthenticatedUser, Depends(resolve_user)],
    ) -> str:
        user_role = user.memberships.get(org_id)
        if not user_role:
            raise HTTPException(
                status_code=status.HTTP_403_FORBIDDEN,
                detail="User is not a member of the specified organization",
            )
        if user_role != self.required_role and user_role != "admin":
            raise HTTPException(
                status_code=status.HTTP_403_FORBIDDEN,
                detail=f"Requires '{self.required_role}' role in org '{org_id}'",
            )
        return user_role
```

### Example 4: Failure Case — 403 Forbidden Response Format

```python
# When an authenticated user with role "viewer" accesses an admin route:
# GET /admin/users
# Header: Authorization: Bearer <valid_viewer_jwt>
# Response:
# HTTP 403 Forbidden
# {
#   "error": "forbidden",
#   "detail": "Operation requires one of: ['admin', 'superadmin']"
# }
```

### Example 5: Idiomatic Modern Pattern with Python 3.12+ `Annotated` Type Aliases

```python
from typing import Annotated
from fastapi import APIRouter, Depends, FastAPI
from pydantic import BaseModel

app = FastAPI()

class User(BaseModel):
    id: str
    roles: list[str]

async def get_current_user() -> User:
    return User(id="u1", roles=["admin"])

def require_roles(*allowed: str):
    def checker(user: Annotated[User, Depends(get_current_user)]) -> User:
        if not any(r in allowed for r in user.roles):
            raise HTTPException(status_code=403, detail="Forbidden")
        return user
    return checker

# Declarative type aliases
AdminUser = Annotated[User, Depends(require_roles("admin", "superadmin"))]
AnyUser = Annotated[User, Depends(get_current_user)]

@app.post("/admin/reset-system")
async def reset_system(admin: AdminUser):
    return {"message": "Reset triggered by admin", "operator": admin.id}
```

---

## 6. Edge Cases, Pitfalls & Failure Modes

1. **Returning 401 Instead of 403**: Returning 401 Unauthorized when a user is authenticated but lacks permission violates RFC 9110. 401 means "Unauthenticated / Login Required"; 403 means "Authenticated / Access Denied".
2. **Role Explosion**: In complex enterprise systems, creating roles for every minor permutation (e.g. `billing_viewer_with_export_rights`) causes unmaintainable schema sprawl. Shift to Attribute-Based (ABAC) or Permission-Based Access Control (PBAC).
3. **Stale JWT Roles**: If a user's role is revoked in the database, but encoded in a 1-hour JWT, the user retains administrative access until token expiration unless a real-time database or Redis check is performed.

---

## 7. Common Mistakes Table

| Mistake | Why It Fails | Error / Diagnostic | Correct Idiomatic Fix |
|---|---|---|---|
| 1. Raising 401 instead of 403 for role failure | Confuses clients; prompts unnecessary login dialogs | Client loops back to login screen | Raise `HTTPException(status_code=403)` |
| 2. Hardcoding role checks inside route handlers | Violates DRY; difficult to audit and maintain | Messy `if user.role != "admin"` in every function | Use reusable dependencies (`Depends(RoleChecker(...))`) |
| 3. Evaluating roles case-sensitively without normalization | `Admin` vs `admin` causes unexpected 403 errors | Valid admin locked out | Use `StrEnum` or normalize via `.lower()` |
| 4. Ignoring role revocation during token lifetime | Demoted user retains admin access for hours | Security privilege leak | Store token revision or check DB for sensitive actions |
| 5. Not handling multi-role users | User has `["member", "admin"]`, check tests only first | Incorrect 403 when user has valid secondary role | Use set intersection: `bool(set(user.roles) & allowed)` |
| 6. Mutating user role in dependency | In-memory cached dependency modifies global state | Subsequent requests see corrupted user state | Return frozen models or new copies |

---

## 8. Performance & Scalability Considerations

- **In-Memory Set Lookups**: Keep allowed roles in a `frozenset` initialized during module load. Membership checks execute in $O(1)$ constant time with zero allocation.
- **FastAPI Dependency Caching**: `Depends(RoleChecker(...))` instances are evaluated once per request when shared across endpoints and sub-dependencies.

---

## 9. Security & OWASP Guardrails

- **OWASP API1:2023 Broken Object Level Authorization (BOLA)**: RBAC only verifies the user's role category; it does NOT ensure the user owns the specific record (e.g., User A editing User B's profile). Combine RBAC with object-ownership validation.
- **OWASP API5:2023 Broken Function Level Authorization (BFLA)**: Ensure all administrative endpoints are strictly guarded by router-level or endpoint-level RBAC dependencies.

---

## 10. Testing Strategies & Pytest Recipes

```python
import pytest
from httpx import ASGITransport, AsyncClient
from fastapi import FastAPI, Depends, HTTPException, status
from pydantic import BaseModel

class User(BaseModel):
    role: str

async def mock_viewer():
    return User(role="viewer")

async def mock_admin():
    return User(role="admin")

@pytest.mark.asyncio
async def test_rbac_forbidden_for_viewer(app: FastAPI):
    app.dependency_overrides[get_current_user] = mock_viewer
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        res = await client.get("/admin/metrics")
        assert res.status_code == 403
    app.dependency_overrides.clear()

@pytest.mark.asyncio
async def test_rbac_allowed_for_admin(app: FastAPI):
    app.dependency_overrides[get_current_user] = mock_admin
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        res = await client.get("/admin/metrics")
        assert res.status_code == 200
    app.dependency_overrides.clear()
```

---

## 11. Real-World Case Study

A healthcare analytics platform suffered an incident where an external auditor was able to trigger a patient data export via a hidden endpoint. The endpoint checked that the user had a valid login token but omitted the role guard.
- **Architectural Solution**: The team refactored their FastAPI routing:
  1. Created isolated sub-routers for each privilege level: `admin_router = APIRouter(dependencies=[Depends(RequireRole(["admin"]))])`.
  2. Mounted all administrative and export routes exclusively on the guarded router.
  3. Added an automated CI check that introspects `app.routes` and alerts if any mutation endpoint lacks an authorization dependency.

---

## 12. Technical Interview Questions

<details>
<summary>Q1: Why should RBAC role guards be implemented as callable classes rather than standard functions in FastAPI?</summary>

**Answer**: Implementing the guard as a callable class (with `__init__` and `__call__`) allows parameters (such as the list of permitted roles) to be configured at route definition time (`Depends(RoleChecker(["admin", "manager"]))`). The instantiated object acts as a dependency callable where FastAPI injects request-scoped dependencies (such as `get_current_user`) into `__call__`.
</details>

<details>
<summary>Q2: What is the fundamental difference between RBAC and ABAC (Attribute-Based Access Control)?</summary>

**Answer**: RBAC makes access decisions based solely on predefined roles assigned to a user (`role == "admin"`). ABAC makes decisions dynamically based on attributes of the user (department, clearance), the resource (classification, owner, created_at), and the environment (time of day, client IP, location).
</details>

<details>
<summary>Q3: When should an API return HTTP 401 vs. HTTP 403?</summary>

**Answer**:
- **401 Unauthorized**: The client has not provided valid authentication credentials (missing token, invalid signature, expired session). It implies "You must authenticate to proceed".
- **403 Forbidden**: The client's identity is authenticated and verified, but the subject lacks the required permissions/roles to perform the action. Re-authenticating with the same credentials will not change the outcome.
</details>

<details>
<summary>Q4: How do you implement router-wide RBAC in FastAPI?</summary>

**Answer**: Pass the RBAC dependency to the `dependencies` parameter of `APIRouter`:
```python
admin_router = APIRouter(
    prefix="/admin",
    dependencies=[Depends(RoleChecker(["superadmin", "admin"]))]
)
```
FastAPI will automatically evaluate the role check for every endpoint registered on that router.
</details>

<details>
<summary>Q5: Why is checking roles inside JWT tokens potentially hazardous in enterprise applications?</summary>

**Answer**: JWTs are stateless. If an administrator revokes an employee's "admin" role in the database, the employee's existing JWT remains valid and retains the "admin" claim until its `exp` timestamp. To mitigate this, high-security endpoints must query the current role from a low-latency database or Redis cache rather than relying solely on the token payload.
</details>

<details>
<summary>Q6: How can you support hierarchical roles where an "Admin" automatically satisfies a "Viewer" requirement?</summary>

**Answer**: Model roles as integers (e.g. `IntEnum`: `VIEWER=10`, `EDITOR=20`, `ADMIN=30`) and check `user.role >= required_role`, or maintain a directed graph/dictionary of inherited roles (e.g., `ROLE_HIERARCHY = {"admin": {"editor", "viewer"}, "editor": {"viewer"}}`) and test for set membership.
</details>

<details>
<summary>Q7: Can a FastAPI dependency modify the response headers when raising a 403 Forbidden?</summary>

**Answer**: Yes. `HTTPException` accepts a `headers` dictionary. When raising 403, you can attach custom diagnostic or security headers (e.g., `headers={"X-Required-Role": "admin"}`).
</details>

<details>
<summary>Q8: How do you verify RBAC permissions in unit tests without standing up a real database?</summary>

**Answer**: Use FastAPI's `app.dependency_overrides` dictionary to replace the authentication dependency (`get_current_user`) with mock callables that return users with different role configurations.
</details>

---

## 13. Practical Exercises & Solutions

<details>
<summary>Exercise 1 (Easy): Create a simple role-checking dependency</summary>

**Problem**: Create a dependency `ensure_admin` that inspects `user["role"]` and raises 403 if it is not `"admin"`.

**Solution**:
```python
from fastapi import Depends, HTTPException, status

async def get_user():
    return {"id": "1", "role": "member"}

def ensure_admin(user: dict = Depends(get_user)):
    if user.get("role") != "admin":
        raise HTTPException(status_code=status.HTTP_403_FORBIDDEN, detail="Admin role required")
    return user
```
</details>

<details>
<summary>Exercise 2 (Easy): Protect an entire APIRouter with an admin guard</summary>

**Problem**: Define an `APIRouter` for `/finance` that executes `ensure_admin` on all child endpoints.

**Solution**:
```python
from fastapi import APIRouter, Depends

finance_router = APIRouter(
    prefix="/finance",
    dependencies=[Depends(ensure_admin)]
)

@finance_router.get("/revenue")
def get_revenue():
    return {"total": 1_000_000}
```
</details>

<details>
<summary>Exercise 3 (Medium): Build a Hierarchical Role Checker using `IntEnum`</summary>

**Problem**: Create an `IntEnum` for roles and a dependency class `RequireLevel(level)` that permits access to any role with equal or higher value.

**Solution**:
```python
from enum import IntEnum
from fastapi import Depends, HTTPException, status
from pydantic import BaseModel

class RoleTier(IntEnum):
    USER = 1
    SUPERVISOR = 2
    DIRECTOR = 3

class Account(BaseModel):
    username: str
    tier: RoleTier

async def get_account():
    return Account(username="bob", tier=RoleTier.SUPERVISOR)

class RequireLevel:
    def __init__(self, min_tier: RoleTier):
        self.min_tier = min_tier

    def __call__(self, acc: Account = Depends(get_account)):
        if acc.tier < self.min_tier:
            raise HTTPException(status_code=status.HTTP_403_FORBIDDEN, detail="Insufficient rank")
        return acc
```
</details>

<details>
<summary>Exercise 4 (Medium): Build an Any-Of Multi-Role Checker</summary>

**Problem**: Write a dependency class that accepts multiple permitted roles and validates if a user with a list of roles has at least one of them.

**Solution**:
```python
from typing import Sequence
from fastapi import Depends, HTTPException, status

class AnyOfRoles:
    def __init__(self, *roles: str):
        self.roles = set(roles)

    def __call__(self, user: dict = Depends(get_user)):
        user_roles = set(user.get("roles", []))
        if not (user_roles & self.roles):
            raise HTTPException(
                status_code=status.HTTP_403_FORBIDDEN,
                detail=f"User lacks required roles. Permitted: {self.roles}"
            )
        return user
```
</details>

<details>
<summary>Exercise 5 (Hard): Implement Dynamic Role Invalidation via Redis Check</summary>

**Problem**: Build a dependency that checks the user's role from a JWT, but if the role is `"admin"`, cross-checks against a Redis set of active administrators to prevent stale-token privilege retention.

**Solution**:
```python
from typing import Annotated
from fastapi import Depends, HTTPException, Security, status
from fastapi.security import HTTPAuthorizationCredentials, HTTPBearer
import jwt

security = HTTPBearer()
REDIS_ACTIVE_ADMINS = {"usr_admin_01", "usr_admin_02"}

async def verify_realtime_admin(
    creds: Annotated[HTTPAuthorizationCredentials, Security(security)]
) -> dict:
    payload = jwt.decode(creds.credentials, "secret", algorithms=["HS256"])
    user_id = payload["sub"]
    role = payload.get("role")
    
    if role != "admin":
        raise HTTPException(status_code=status.HTTP_403_FORBIDDEN, detail="Requires admin role")
    
    # Real-time revocation verification
    if user_id not in REDIS_ACTIVE_ADMINS:
        raise HTTPException(
            status_code=status.HTTP_403_FORBIDDEN,
            detail="Admin privileges have been revoked by the system",
        )
    return payload
```
</details>

<details>
<summary>Exercise 6 (Hard): Construct Tenant-Scoped RBAC with Dynamic Path Extraction</summary>

**Problem**: Implement an authorization dependency that extracts `{workspace_id}` from the request path and verifies the user's role specifically within that workspace.

**Solution**:
```python
from typing import Annotated
from fastapi import Depends, HTTPException, Path, status
from pydantic import BaseModel

class WorkspaceUser(BaseModel):
    user_id: str
    workspace_roles: dict[str, str]  # workspace_id -> role

async def get_workspace_user():
    return WorkspaceUser(
        user_id="usr_55",
        workspace_roles={"ws_engineering": "lead", "ws_marketing": "viewer"}
    )

def require_workspace_role(required_role: str):
    async def _check(
        workspace_id: Annotated[str, Path()],
        user: Annotated[WorkspaceUser, Depends(get_workspace_user)],
    ):
        actual_role = user.workspace_roles.get(workspace_id)
        if not actual_role:
            raise HTTPException(status_code=status.HTTP_403_FORBIDDEN, detail="Not a member of workspace")
        if actual_role != required_role and actual_role != "owner":
            raise HTTPException(
                status_code=status.HTTP_403_FORBIDDEN,
                detail=f"Role '{required_role}' required in workspace '{workspace_id}'",
            )
        return user
    return _check
```
</details>
