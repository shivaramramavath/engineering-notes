---
title: "Layered Architecture in FastAPI Applications"
description: "Design clean, decoupled, and maintainable enterprise FastAPI applications using the classic layered architecture: Presentation (APIRouter), Service (Business Logic), Repository (Data Access), and Domain Models."
fastapi_version: "0.115.x+"
python_version: "3.12+"
pydantic_version: "v2.10+"
tested_with: "sqlalchemy 2.0+, pydantic 2.10+, fastapi 0.115+"
free_threading_status: "Fully Compatible"
---

# Layered Architecture in FastAPI Applications

## 1. Conceptual Foundation & Mental Model

### The Monolithic Route Handler Anti-Pattern
In trivial FastAPI tutorials, route handler functions frequently perform request decoding, parameter validation, direct database queries via SQLAlchemy sessions, third-party payment gateway HTTP calls, and HTML/email generation all within a single `async def` function. This anti-pattern—known as the **"Fat Route"** or "Smart UI" pattern—leads to:
1. **Zero Test Isolation**: Unit testing business logic requires instantiating complete HTTP test clients, running database engines, and mocking external network connections simultaneously.
2. **Duplicated Business Rules**: When the same business validation (e.g., verifying user credit balance) is needed across an HTTP endpoint, a background Celery worker, and a CLI script, the logic must either be copy-pasted or awkwardly invoked via simulated HTTP requests.
3. **Tight Coupling to Infrastructure**: Changing the database engine from PostgreSQL to MongoDB or swapping SQLAlchemy for an external gRPC service requires rewriting the entire web layer.

### The Layered (N-Tier) Architectural Paradigm
Layered architecture organizes an application into horizontal, unidirectionally dependent slices. Each layer possesses a distinct responsibility and communicates solely with the layer immediately beneath it:

```
+-------------------------------------------------------------+
|               1. PRESENTATION LAYER (API / Web)             |
|  - APIRouter, FastAPI Dependencies (Auth, Rate Limiting)    |
|  - Request deserialization & HTTP status code translation    |
|  - Pydantic In/Out DTOs (Request / Response Schemas)        |
+-------------------------------------------------------------+
                              |
                              v  (Calls Service Interface)
+-------------------------------------------------------------+
|                 2. SERVICE LAYER (Application Logic)        |
|  - Coordinates business workflows & enforces domain rules   |
|  - Transaction demarcation (Commit / Rollback)              |
|  - Dispatches domain events & calls external adapters       |
+-------------------------------------------------------------+
                              |
                              v  (Calls Repository Protocol)
+-------------------------------------------------------------+
|                 3. DATA ACCESS LAYER (Repository)           |
|  - Abstract data persistence interface                      |
|  - SQLAlchemy 2.0 select(), filter(), insert() queries      |
|  - Translates database records into domain representations  |
+-------------------------------------------------------------+
                              |
                              v
+-------------------------------------------------------------+
|                 4. INFRASTRUCTURE & DOMAIN                  |
|  - Pure Domain Entities / Dataclasses                       |
|  - Database Engine, Redis Clients, Third-Party SDKs         |
+-------------------------------------------------------------+
```

### The Strict Dependency Rule
Dependencies flow **strictly downward**:
- The **Presentation Layer** knows about the **Service Layer** and DTO schemas, but knows *nothing* about database tables or raw SQL queries.
- The **Service Layer** knows about domain models, business rules, and repository abstractions, but knows *nothing* about HTTP status codes, FastAPI `Request` objects, or ASGI scopes.
- The **Repository Layer** knows about SQLAlchemy ORM models and queries, but knows *nothing* about HTTP responses or business workflows.

---

## 2. Architecture & Internal Mechanics

The structural boundary between layers is maintained via FastAPI's Dependency Injection system (`Depends()`) and Python's static typing protocols (`typing.Protocol`).

```
[HTTP Request]
      |
      v
+---------------------------------------------------------------+
| Presentation Layer: /api/v1/orders (Router)                   |
| 1. Pydantic validates OrderCreateRequest DTO                  |
| 2. Depends(get_order_service) resolves OrderService instance   |
| 3. Awaits order_service.place_order(customer_id, items)       |
+---------------------------------------------------------------+
      |
      v (Pure Python types / Domain inputs)
+---------------------------------------------------------------+
| Service Layer: OrderService                                   |
| 1. Validates inventory availability via InventoryRepository    |
| 2. Calculates bulk discounts and taxes (Domain Logic)         |
| 3. Invokes PaymentGatewayAdapter.charge(...)                   |
| 4. Saves order via OrderRepository.create(...)                |
| 5. Emits OrderPlacedEvent via BackgroundTasks or Queue        |
+---------------------------------------------------------------+
      |
      v (Domain Entity / Entity ID)
+---------------------------------------------------------------+
| Repository Layer: SqlAlchemyOrderRepository                   |
| 1. Converts domain entity to SQLAlchemy ORM model             |
| 2. Executes async session.add(orm_order)                      |
| 3. Flushes and returns populated domain representation        |
+---------------------------------------------------------------+
      |
      v (Asyncpg SQL Driver)
[PostgreSQL Database]
```

### Decoupling HTTP from Business Exceptions
In a clean layered architecture, the Service Layer never raises `fastapi.HTTPException`. Doing so couples business logic to the HTTP transport protocol. Instead:
1. The Service Layer raises **Domain Exceptions** (e.g., `InsufficientInventoryError`, `OrderNotFoundError`).
2. The Presentation Layer catches these domain exceptions (or registers FastAPI `exception_handlers`) and maps them to appropriate HTTP status codes (`409 Conflict`, `404 Not Found`).

---

## 3. Comprehensive Syntax & Core Mechanics

### Directory Layout for Layered Architecture
```
app/
├── api/                        # Presentation Layer
│   ├── v1/
│   │   ├── endpoints/
│   │   │   ├── orders.py
│   │   │   └── users.py
│   │   └── router.py
│   └── dependencies.py         # HTTP-specific DI wiring
├── services/                   # Service Layer (Use Cases)
│   ├── base.py
│   ├── order_service.py
│   └── user_service.py
├── repositories/               # Repository Layer (Data Access)
│   ├── base.py
│   ├── order_repository.py
│   └── user_repository.py
├── models/                     # Database & Domain Models
│   ├── domain/                 # Pure domain entities
│   │   └── order.py
│   └── orm/                    # SQLAlchemy database tables
│       └── order_table.py
├── schemas/                    # Pydantic DTOs (Request / Response)
│   ├── order.py
│   └── user.py
└── core/                       # Cross-cutting infrastructure
    ├── config.py
    ├── database.py
    └── exceptions.py
```

---

## 4. Practical Implementation: 5 Verified Examples

### Example 1: Domain Entities and Custom Exceptions
Defining pure business objects independent of SQLAlchemy or FastAPI.

```python
# app/core/exceptions.py
class DomainError(Exception):
    """Base exception for all domain logic failures."""
    pass

class InsufficientStockError(DomainError):
    def __init__(self, sku: str, requested: int, available: int):
        self.sku = sku
        self.requested = requested
        self.available = available
        super().__init__(f"SKU {sku} has only {available} units in stock; requested {requested}")

class OrderNotFoundError(DomainError):
    def __init__(self, order_id: str):
        self.order_id = order_id
        super().__init__(f"Order with ID {order_id} does not exist")
```

```python
# app/models/domain/order.py
from dataclasses import dataclass, field
from decimal import Decimal
from datetime import datetime, timezone
from typing import List
import uuid

@dataclass
class OrderItem:
    sku: str
    unit_price: Decimal
    quantity: int

    @property
    def total_price(self) -> Decimal:
        return self.unit_price * self.quantity

@dataclass
class Order:
    customer_id: str
    items: List[OrderItem]
    id: str = field(default_factory=lambda: str(uuid.uuid4()))
    created_at: datetime = field(default_factory=lambda: datetime.now(timezone.utc))
    status: str = "PENDING"

    @property
    def total_amount(self) -> Decimal:
        return sum(item.total_price for item in self.items)
```

---

### Example 2: Repository Layer with Abstract Protocol
Decoupling database persistence from business logic using `typing.Protocol`.

```python
# app/repositories/order_repository.py
from typing import Protocol, Optional, List
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession
from app.models.domain.order import Order, OrderItem
from app.models.orm.order_table import OrderORM, OrderItemORM

class OrderRepositoryProtocol(Protocol):
    async def get_by_id(self, order_id: str) -> Optional[Order]: ...
    async def save(self, order: Order) -> Order: ...

class SqlAlchemyOrderRepository:
    def __init__(self, session: AsyncSession):
        self._session = session

    async def get_by_id(self, order_id: str) -> Optional[Order]:
        stmt = select(OrderORM).where(OrderORM.id == order_id)
        result = await self._session.execute(stmt)
        orm_order = result.scalar_one_or_none()
        if not orm_order:
            return None
            
        items = [
            OrderItem(sku=i.sku, unit_price=i.unit_price, quantity=i.quantity)
            for i in orm_order.items
        ]
        return Order(
            id=orm_order.id,
            customer_id=orm_order.customer_id,
            items=items,
            status=orm_order.status,
            created_at=orm_order.created_at,
        )

    async def save(self, order: Order) -> Order:
        orm_order = OrderORM(
            id=order.id,
            customer_id=order.customer_id,
            status=order.status,
            total_amount=order.total_amount,
            created_at=order.created_at,
        )
        for item in order.items:
            orm_order.items.append(
                OrderItemORM(sku=item.sku, unit_price=item.unit_price, quantity=item.quantity)
            )
        self._session.add(orm_order)
        await self._session.flush()
        return order
```

---

### Example 3: Service Layer Orchestration
Enforcing business invariants, coordinating repositories, and executing transactional logic without any HTTP dependencies.

```python
# app/services/order_service.py
from decimal import Decimal
from typing import List, Tuple
from app.core.exceptions import InsufficientStockError, OrderNotFoundError
from app.models.domain.order import Order, OrderItem
from app.repositories.order_repository import OrderRepositoryProtocol

class OrderService:
    def __init__(self, order_repo: OrderRepositoryProtocol):
        self._order_repo = order_repo

    async def create_order(
        self,
        customer_id: str,
        items_raw: List[Tuple[str, Decimal, int]]
    ) -> Order:
        # 1. Enforce business invariants
        order_items = []
        for sku, price, qty in items_raw:
            if qty <= 0:
                raise ValueError(f"Quantity for {sku} must be greater than zero")
            # Business logic: clearance items check or simulated stock limit
            if sku.startswith("OUT_OF_STOCK"):
                raise InsufficientStockError(sku=sku, requested=qty, available=0)
            order_items.append(OrderItem(sku=sku, unit_price=price, quantity=qty))

        # 2. Construct domain entity
        new_order = Order(customer_id=customer_id, items=order_items)

        # 3. Persist via repository abstraction
        saved_order = await self._order_repo.save(new_order)
        return saved_order

    async def fetch_order(self, order_id: str) -> Order:
        order = await self._order_repo.get_by_id(order_id)
        if not order:
            raise OrderNotFoundError(order_id)
        return order
```

---

### Example 4: Presentation Layer (FastAPI Routers and DTOs)
Handling HTTP validation, mapping domain models to response schemas, and converting domain errors to HTTP responses.

```python
# app/schemas/order.py
from decimal import Decimal
from typing import List
from datetime import datetime
from pydantic import BaseModel, Field, ConfigDict

class OrderItemCreateDTO(BaseModel):
    sku: str = Field(min_length=3, max_length=50)
    unit_price: Decimal = Field(gt=Decimal("0.00"))
    quantity: int = Field(gt=0, le=1000)

class OrderCreateRequestDTO(BaseModel):
    customer_id: str = Field(min_length=1)
    items: List[OrderItemCreateDTO] = Field(min_length=1)

class OrderResponseDTO(BaseModel):
    id: str
    customer_id: str
    status: str
    total_amount: Decimal
    created_at: datetime
    model_config = ConfigDict(from_attributes=True)
```

```python
# app/api/v1/endpoints/orders.py
from fastapi import APIRouter, Depends, HTTPException, status
from sqlalchemy.ext.asyncio import AsyncSession
from app.core.database import get_db_session
from app.core.exceptions import InsufficientStockError, OrderNotFoundError
from app.schemas.order import OrderCreateRequestDTO, OrderResponseDTO
from app.repositories.order_repository import SqlAlchemyOrderRepository
from app.services.order_service import OrderService

router = APIRouter(prefix="/orders", tags=["Orders"])

# Dependency Injection Factory
def get_order_service(session: AsyncSession = Depends(get_db_session)) -> OrderService:
    repository = SqlAlchemyOrderRepository(session)
    return OrderService(repository)

@router.post("/", response_model=OrderResponseDTO, status_code=status.HTTP_201_CREATED)
async def create_order_endpoint(
    payload: OrderCreateRequestDTO,
    service: OrderService = Depends(get_order_service),
):
    try:
        raw_items = [(i.sku, i.unit_price, i.quantity) for i in payload.items]
        order = await service.create_order(payload.customer_id, raw_items)
        return OrderResponseDTO(
            id=order.id,
            customer_id=order.customer_id,
            status=order.status,
            total_amount=order.total_amount,
            created_at=order.created_at,
        )
    except InsufficientStockError as exc:
        raise HTTPException(status_code=status.HTTP_409_CONFLICT, detail=str(exc))
    except ValueError as exc:
        raise HTTPException(status_code=status.HTTP_422_UNPROCESSABLE_ENTITY, detail=str(exc))

@router.get("/{order_id}", response_model=OrderResponseDTO)
async def get_order_endpoint(
    order_id: str,
    service: OrderService = Depends(get_order_service),
):
    try:
        order = await service.fetch_order(order_id)
        return OrderResponseDTO(
            id=order.id,
            customer_id=order.customer_id,
            status=order.status,
            total_amount=order.total_amount,
            created_at=order.created_at,
        )
    except OrderNotFoundError as exc:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail=str(exc))
```

---

### Example 5: Pure Unit Testing of the Service Layer (Zero DB / Zero HTTP)
Demonstrating how layered architecture allows testing business logic in nanoseconds using pure in-memory mocks without SQLite, PostgreSQL, or TestClient.

```python
# tests/unit/test_order_service.py
import pytest
from decimal import Decimal
from app.core.exceptions import InsufficientStockError
from app.models.domain.order import Order
from app.services.order_service import OrderService

class InMemoryOrderRepository:
    """Fast in-memory fake repository fulfilling OrderRepositoryProtocol."""
    def __init__(self):
        self.orders: dict[str, Order] = {}

    async def get_by_id(self, order_id: str):
        return self.orders.get(order_id)

    async def save(self, order: Order):
        self.orders[order.id] = order
        return order

@pytest.mark.asyncio
async def test_order_creation_calculates_total_correctly():
    fake_repo = InMemoryOrderRepository()
    service = OrderService(fake_repo)

    items = [
        ("SKU-KEYBOARD", Decimal("100.00"), 2),
        ("SKU-MOUSE", Decimal("50.00"), 1),
    ]

    order = await service.create_order("customer_42", items)

    assert order.customer_id == "customer_42"
    assert order.total_amount == Decimal("250.00")
    assert order.status == "PENDING"
    assert len(fake_repo.orders) == 1

@pytest.mark.asyncio
async def test_order_creation_rejects_out_of_stock_items():
    fake_repo = InMemoryOrderRepository()
    service = OrderService(fake_repo)

    items = [("OUT_OF_STOCK_ITEM", Decimal("20.00"), 5)]

    with pytest.raises(InsufficientStockError) as exc_info:
        await service.create_order("customer_42", items)

    assert exc_info.value.sku == "OUT_OF_STOCK_ITEM"
    assert len(fake_repo.orders) == 0
```

---

## 5. Edge Cases, Pitfalls & Failure Modes

1. **Leaking SQLAlchemy Models to the Presentation Layer**: Returning raw ORM instances directly from endpoints (`return orm_order`) forces Pydantic to read lazily loaded relationships outside an active session, triggering `DetachedInstanceError: Parent instance <OrderORM> is not bound to a Session`. Always map ORM records to domain models or DTOs inside the repository/service layer.
2. **Global Exception Mapping Omission**: Relying on manual `try/except HTTPException` in every router endpoint creates boilerplate and risks unhandled domain exceptions leaking as generic HTTP 500 errors. Register FastAPI exception handlers for domain errors globally (`@app.exception_handler(DomainError)`).
3. **Transaction Ownership Confusion**: If repositories commit transactions internally (`await session.commit()`), composite operations involving multiple repositories (e.g., deducting inventory and creating an order) cannot be rolled back atomically. The **Service Layer** or **Unit of Work** must manage transaction boundaries.
4. **Anemic Domain Models**: Creating domain entities that are merely passive data bags while writing procedural helper functions defeats the purpose of domain modeling. Place domain invariants (e.g., `order.total_amount`) directly on the domain class.

---

## 6. Common Mistakes Table

| Mistake | Root Cause | Impact | Correct Approach |
| :--- | :--- | :--- | :--- |
| **Importing FastAPI in the Service Layer** | Raising `HTTPException(404)` inside services | Couples business logic to HTTP; breaks CLI commands, background workers, and event subscribers. | Raise custom Python domain exceptions; map to HTTP status codes in the presentation layer. |
| **Injecting `AsyncSession` Directly into Routers** | Calling `session.execute(select(...))` in route functions | Skips business rule validation; makes unit testing impossible without live databases. | Inject a Service or Repository into the router; keep database mechanics encapsulated. |
| **Repositories Committing Transactions** | Calling `session.commit()` inside repository methods | Prevents multi-repository atomic transactions; leaves database in corrupted state on partial failure. | Use the Unit of Work pattern or let the Service layer control transaction commits. |
| **Reusing Pydantic DTOs as Database Entities** | Trying to make one class serve as API contract and DB model | Schema migrations break external API clients; DB constraints pollute public API documentation. | Keep separate Pydantic DTOs for HTTP I/O and pure domain models or SQLAlchemy tables. |
| **Circular Dependencies Between Layers** | Repositories importing Services or Routers importing ORM models | Python `ImportError: cannot import name` at startup. | Enforce strict downward dependency flow: Router $\to$ Service $\to$ Repository $\to$ Model. |
| **Over-Engineering Trivial CRUD** | Creating 5 layers for a simple 1-table lookup | Boilerplate fatigue and degraded developer velocity for simple read-only APIs. | Apply layered architecture pragmatically; simple endpoints can utilize lightweight query services. |

---

## 7. Performance & Scalability Considerations

- **Mapping Overhead**: Converting records from SQLAlchemy ORM $\to$ Domain Dataclasses $\to$ Pydantic DTOs incurs a minor CPU serialization cost. In ultra-high-throughput read-heavy endpoints, utilize SQLAlchemy's `mappings()` or optimized projection queries returning Pydantic models directly via `TypeAdapter`.
- **Async Execution**: Ensure all repository calls are fully asynchronous using `AsyncSession`. A synchronous call inside a repository blocks the main thread and halts the entire Uvicorn worker.
- **Python 3.13+ Free-Threading**: Under free-threaded Python, pure domain logic and Pydantic validation scale across native OS threads without GIL lock contention, making layered architectures significantly faster during CPU-heavy business processing.

---

## 8. Security & OWASP Guardrails

- **OWASP A01 (Broken Access Control)**: Enforce authorization checks in the **Service Layer** rather than relying exclusively on FastAPI route dependencies. If authorization is checked solely in the router, background workers or internal scripts calling the service bypass security policies.
- **OWASP A03 (Injection Prevention)**: Encapsulating database access within repositories ensures raw SQL string formatting is strictly prohibited; all queries utilize parameterized SQLAlchemy constructs.

---

## 9. Testing Strategies & Pytest Recipes

```python
# tests/conftest.py
import pytest
from unittest.mock import AsyncMock
from app.services.order_service import OrderService
from app.api.v1.endpoints.orders import get_order_service
from app.main import app

@pytest.fixture
def mock_order_service():
    service = AsyncMock(spec=OrderService)
    app.dependency_overrides[get_order_service] = lambda: service
    yield service
    app.dependency_overrides.clear()
```

---

## 10. Real-World Case Study / Production Incident

A fast-growing logistics SaaS application wrote all core business logic inside FastAPI route handlers. When the company expanded to support IoT warehouse scanners communicating via MQTT and periodic batch billing jobs run via Celery, engineers had no way to reuse the order validation logic.

Developers began duplicating the 300-line validation script across the Celery tasks and MQTT handlers. Within 6 months:
1. A discount calculation bug was fixed in the HTTP endpoint but forgotten in the Celery billing task.
2. The company under-billed enterprise customers by \$140,000 over three billing cycles.
3. Database deadlocks occurred because the Celery workers and HTTP handlers updated order items in opposite locking orders.

**The Fix**:
The team refactored the application into a strict layered architecture:
- Extracted all order invariants into an `OrderService`.
- Replaced direct database queries with an `OrderRepository` wrapped in a Unit of Work.
- The HTTP router, Celery task, and MQTT consumer all called the exact same `OrderService.process_order()` method.
- Complete unit test coverage of the service layer took under 2 seconds to execute, preventing future billing discrepancies.

---

## 11. Technical Interview Questions

<details>
<summary>1. Why should the Service Layer never raise `fastapi.HTTPException`?</summary>
Raising `HTTPException` inside the service layer couples the business logic to the HTTP transport protocol. If the service is subsequently reused in a CLI command, a background Celery worker, an event subscriber, or a gRPC handler, raising an HTTP-specific exception makes no semantic sense and forces the caller to catch and inspect HTTP status codes rather than domain-specific failure reasons.
</details>

<details>
<summary>2. What is the difference between a DTO (Data Transfer Object) and a Domain Entity?</summary>
A DTO (typically a Pydantic model in FastAPI) represents the serialized contract of data entering or leaving the system over the network (request and response payloads). It validates input formats, handles serialization, and shields the internal domain structure from external clients. A Domain Entity represents an identifiable core business object (often a dataclass or Python object) that encapsulates business rules, invariants, and behaviors.
</details>

<details>
<summary>3. Why should the Repository Layer return Domain Models instead of SQLAlchemy ORM objects?</summary>
Returning raw ORM instances leaks database infrastructure details into higher layers. If a service or router accesses an un-loaded relationship on a detached ORM instance outside an active session, SQLAlchemy throws a `DetachedInstanceError`. Furthermore, modifying an ORM object inadvertently marks it dirty, potentially committing unintended database mutations during subsequent flushes.
</details>

<details>
<summary>4. How does Python's `typing.Protocol` enable the Dependency Inversion Principle (DIP)?</summary>
`typing.Protocol` defines structural typing (duck typing) contracts for dependencies. By having the Service Layer depend on an abstract Protocol (e.g., `OrderRepositoryProtocol`) rather than a concrete implementation (e.g., `SqlAlchemyOrderRepository`), the concrete implementation can be swapped for an in-memory repository or mock during testing without modifying the service code.
</details>

<details>
<summary>5. Who owns database transaction management (Commit / Rollback) in a layered architecture?</summary>
The Service Layer (or an associated Unit of Work coordinator) owns transaction demarcation. If individual repositories commit transactions, it becomes impossible to execute multi-repository workflows atomically (e.g., deducting user balance in `UserRepository` and saving order in `OrderRepository`).
</details>

<details>
<summary>6. How do you handle domain exceptions cleanly across all FastAPI endpoints without repetitive try/except blocks?</summary>
By registering global FastAPI exception handlers using `@app.exception_handler(DomainError)`. When any service raises a subclass of `DomainError`, the handler automatically intercepts it, logs the error, and returns a standardized JSON response with the appropriate HTTP status code (e.g., 404 for Not Found, 409 for Conflict).
</details>

<details>
<summary>7. What is an Anemic Domain Model, and why is it considered an architectural code smell?</summary>
An anemic domain model consists of entities containing only properties and getters/setters without any business logic, while all validation and transformation logic is written procedurally inside service classes. This degrades maintainability because invariants are scattered across multiple services rather than encapsulated within the entity itself.
</details>

<details>
<summary>8. How do FastAPI dependencies (`Depends()`) facilitate layered architecture?</summary>
FastAPI's dependency injection container allows routers to declare dependencies on service factories. The factory instantiates the database session, injects it into the repository, injects the repository into the service, and provides the fully assembled service to the route handler, ensuring loose coupling and effortless test mocking via `app.dependency_overrides`.
</details>

---

## 12. Practical Exercises & Solutions

<details>
<summary>Exercise 1 (Easy): Create a Domain Exception Hierarchy</summary>

**Task**: Create a base exception `BankingError` and two specific domain exceptions: `AccountNotFoundError(account_id: str)` and `InsufficientBalanceError(account_id: str, balance: float, amount: float)`.

**Solution**:
```python
class BankingError(Exception):
    """Base domain exception for banking operations."""
    pass

class AccountNotFoundError(BankingError):
    def __init__(self, account_id: str):
        self.account_id = account_id
        super().__init__(f"Bank account {account_id} not found")

class InsufficientBalanceError(BankingError):
    def __init__(self, account_id: str, balance: float, amount: float):
        self.account_id = account_id
        self.balance = balance
        self.amount = amount
        super().__init__(f"Account {account_id} balance {balance} insufficient for withdrawal of {amount}")
```
</details>

<details>
<summary>Exercise 2 (Easy): Global Exception Handler for Domain Errors</summary>

**Task**: Write a FastAPI application registering an exception handler for `AccountNotFoundError` that returns an HTTP 404 with a structured JSON body `{"error": "ACCOUNT_NOT_FOUND", "message": "..."}`.

**Solution**:
```python
from fastapi import FastAPI, Request, status
from fastapi.responses import JSONResponse

app = FastAPI()

@app.exception_handler(AccountNotFoundError)
async def account_not_found_handler(request: Request, exc: AccountNotFoundError):
    return JSONResponse(
        status_code=status.HTTP_404_NOT_FOUND,
        content={
            "error": "ACCOUNT_NOT_FOUND",
            "message": str(exc),
            "account_id": exc.account_id,
        }
    )
```
</details>

<details>
<summary>Exercise 3 (Medium): Repository Protocol and In-Memory Implementation</summary>

**Task**: Define a `UserRepoProtocol` with `async def get_by_email(email: str) -> Optional[User]` and `async def save(user: User) -> User`. Implement a complete in-memory version using a dictionary.

**Solution**:
```python
from typing import Protocol, Optional
from dataclasses import dataclass

@dataclass
class User:
    id: str
    email: str
    is_active: bool = True

class UserRepoProtocol(Protocol):
    async def get_by_email(self, email: str) -> Optional[User]: ...
    async def save(self, user: User) -> User: ...

class InMemoryUserRepo:
    def __init__(self):
        self._data: dict[str, User] = {}

    async def get_by_email(self, email: str) -> Optional[User]:
        for user in self._data.values():
            if user.email.lower() == email.lower():
                return user
        return None

    async def save(self, user: User) -> User:
        self._data[user.id] = user
        return user
```
</details>

<details>
<summary>Exercise 4 (Medium): Service Layer Unit Test</summary>

**Task**: Write a `UserService` with a method `register_user(email: str)` that checks if the email already exists via `UserRepoProtocol`. If it does, raise `ValueError("Email already registered")`. Write a unit test verifying this behavior using `InMemoryUserRepo`.

**Solution**:
```python
import uuid
import pytest

class UserService:
    def __init__(self, repo: UserRepoProtocol):
        self._repo = repo

    async def register_user(self, email: str) -> User:
        existing = await self._repo.get_by_email(email)
        if existing:
            raise ValueError("Email already registered")
        new_user = User(id=str(uuid.uuid4()), email=email)
        return await self._repo.save(new_user)

@pytest.mark.asyncio
async def test_register_duplicate_email_raises():
    repo = InMemoryUserRepo()
    service = UserService(repo)
    
    await service.register_user("test@example.com")
    
    with pytest.raises(ValueError, match="Email already registered"):
        await service.register_user("test@example.com")
```
</details>

<details>
<summary>Exercise 5 (Hard): Complete Presentation-Service-Repository Wiring with FastAPI Dependency Injection</summary>

**Task**: Implement the complete FastAPI route dependency injection tree for a `ProductCatalogService` such that the endpoint receives the fully resolved service without creating sessions manually in the route handler.

**Solution**:
```python
from fastapi import APIRouter, Depends
from sqlalchemy.ext.asyncio import AsyncSession
from app.core.database import get_db_session

# Repository
class ProductRepository:
    def __init__(self, session: AsyncSession):
        self.session = session

# Service
class ProductCatalogService:
    def __init__(self, repo: ProductRepository):
        self.repo = repo

# Dependency Provider Chain
def get_product_repository(
    session: AsyncSession = Depends(get_db_session)
) -> ProductRepository:
    return ProductRepository(session)

def get_product_catalog_service(
    repo: ProductRepository = Depends(get_product_repository)
) -> ProductCatalogService:
    return ProductCatalogService(repo)

# Router
router = APIRouter()

@router.get("/products")
async def list_products(
    service: ProductCatalogService = Depends(get_product_catalog_service)
):
    return {"status": "wired_successfully"}
```
</details>

<details>
<summary>Exercise 6 (Hard): Unit of Work Pattern Implementation</summary>

**Task**: Implement an async `UnitOfWork` context manager protocol and concrete SQLAlchemy implementation that manages transaction commits, rollbacks on exceptions, and exposes `user_repo` and `audit_repo` within the atomic transaction scope.

**Solution**:
```python
from typing import Protocol
from sqlalchemy.ext.asyncio import AsyncSession, async_sessionmaker

class UnitOfWorkProtocol(Protocol):
    user_repo: InMemoryUserRepo
    async def __aenter__(self) -> "UnitOfWorkProtocol": ...
    async def __aexit__(self, exc_type, exc_val, exc_tb) -> None: ...
    async def commit(self) -> None: ...
    async def rollback(self) -> None: ...

class SqlAlchemyUnitOfWork:
    def __init__(self, session_factory: async_sessionmaker[AsyncSession]):
        self._session_factory = session_factory
        self.session: AsyncSession | None = None

    async def __aenter__(self):
        self.session = self._session_factory()
        # Initialize repositories with the transactional session
        self.user_repo = SqlAlchemyOrderRepository(self.session)
        return self

    async def __aexit__(self, exc_type, exc_val, exc_tb):
        if exc_type is not None:
            await self.rollback()
        await self.session.close()

    async def commit(self):
        await self.session.commit()

    async def rollback(self):
        await self.session.rollback()
```
</details>
