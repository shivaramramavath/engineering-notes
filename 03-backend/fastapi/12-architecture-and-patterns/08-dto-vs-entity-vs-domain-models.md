---
title: "DTO vs. Entity vs. Domain Models in FastAPI"
description: "Master the tripartite model separation in FastAPI: decouple Pydantic DTOs (API contracts), SQLAlchemy ORM models (database persistence), and pure Domain models (business logic) to prevent security leaks and architectural decay."
fastapi_version: "0.115.x+"
python_version: "3.12+"
pydantic_version: "v2.10+"
tested_with: "pydantic 2.10+, sqlalchemy 2.0+, fastapi 0.115+, pytest 8.3+"
free_threading_status: "Fully Compatible"
---

# DTO vs. Entity vs. Domain Models in FastAPI

## 1. Conceptual Foundation & Mental Model

### The "One Model to Rule Them All" Anti-Pattern
In entry-level tutorials, developers often attempt to use a single class to serve three completely disparate responsibilities:
1. Validating incoming HTTP JSON requests and formatting outgoing JSON responses.
2. Mapping table schemas, foreign keys, and indexes for an SQL relational database.
3. Encapsulating domain invariants, business rules, and state transitions.

Using tools like SQLModel or forcing Pydantic models directly onto SQLAlchemy tables creates severe architectural liabilities:
- **Security Leaks (Over-Posting & Mass Assignment)**: A malicious user submits a JSON payload containing `{"is_admin": true, "balance": 999999}`. Because the API request model is mapped directly to the database entity, the internal columns are overwritten.
- **Leaked Implementation Details**: Internal database foreign keys (`tenant_id`, `password_hash`, `internal_retry_count`) accidentally leak into public OpenAPI / Swagger documentation and client JSON responses.
- **Coupled Migrations and API Versioning**: You cannot alter a database column name or normalize a table without immediately breaking public API contracts for existing mobile or web clients.

```
Anti-Pattern: The Entangled Single Model
+-------------------------------------------------------------+
|                     User "God" Model                        |
|  - Pydantic BaseModel (HTTP JSON Serializer)                |
|  - SQLAlchemy Declarative (SQL Schema & Foreign Keys)       |
|  - Domain Logic (Business Rules & Calculations)             |
+-------------------------------------------------------------+
   ▲                           ▲                           ▲
   │ Leaks DB fields to API    │ Schema change breaks API  │ Hard to test
```

### The Tripartite Model Separation
Clean architecture enforces a strict tripartite separation of concerns across the three models:

```
+-------------------------------------------------------------------------------+
| 1. DATA TRANSFER OBJECT (DTO) [Pydantic v2 BaseModel]                         |
| - Boundary: Presentation Layer / HTTP Network Boundary                        |
| - Role: Validates incoming request wire format; shapes public response JSON   |
| - Lifespan: Exists only for the duration of the HTTP request                  |
+-------------------------------------------------------------------------------+
                                    |
                                    v (Transformed by Mapper)
+-------------------------------------------------------------------------------+
| 2. PURE DOMAIN MODEL [Python Standard Dataclass]                              |
| - Boundary: Core Business Domain / Application Layer                          |
| - Role: Enforces domain rules, invariants, state machines, and calculations   |
| - Dependencies: ZERO (Pure Python; no Pydantic, no SQLAlchemy, no HTTP)       |
+-------------------------------------------------------------------------------+
                                    |
                                    v (Transformed by Repository)
+-------------------------------------------------------------------------------+
| 3. PERSISTENCE ENTITY [SQLAlchemy 2.0 DeclarativeBase]                        |
| - Boundary: Infrastructure / Database Access Layer                            |
| - Role: Maps relational tables, primary keys, foreign keys, column types, SQL |
| - Dependencies: SQLAlchemy, Database Driver (asyncpg, psycopg3)               |
+-------------------------------------------------------------------------------+
```

---

## 2. Architecture & Internal Mechanics

Understanding how data transforms across boundaries as it flows from an HTTP request to database disk storage and back.

```
[Inbound HTTP JSON]
        |
        v
+-------------------------------+
| 1. Request DTO (Pydantic v2)  |  - Field types, string length, regex checks
|    OrderCreateRequestDTO      |  - Deserializes and validates raw JSON input
+-------------------------------+
        |
        | (Service Layer translates DTO to Domain)
        v
+-------------------------------+
| 2. Domain Model (Dataclass)   |  - Enforces business invariants
|    OrderAggregate             |  - Calculates taxes and discounts
|                               |  - Emits OrderPlacedDomainEvent
+-------------------------------+
        |
        | (Repository maps Domain to ORM)
        v
+-------------------------------+
| 3. Persistence Entity (ORM)   |  - SQLAlchemy OrderORM / OrderItemORM
|    OrderTable                 |  - DB column constraints, cascade deletes
+-------------------------------+
        |
        v (SQL Driver)
[PostgreSQL Database Engine]
```

### Transformation Strategies: Where Do Mappings Happen?
1. **DTO $\longleftrightarrow$ Domain**: Managed by the **Presentation Layer** or **Application Service Layer**. The router extracts primitive values or commands from the DTO and passes them into the domain.
2. **Domain $\longleftrightarrow$ Persistence Entity**: Managed strictly inside the **Repository Layer**. External layers never see or touch the SQLAlchemy ORM instances.

---

## 3. Comprehensive Syntax & Core Mechanics

### Comparison Matrix

| Feature | Data Transfer Object (DTO) | Domain Model | Persistence Entity |
| :--- | :--- | :--- | :--- |
| **Technology** | `pydantic.BaseModel` | `@dataclass` or Plain Python Class | SQLAlchemy `DeclarativeBase` |
| **Primary Location** | `app/schemas/` or `app/api/` | `app/domain/models/` | `app/infrastructure/orm/` |
| **Validation Focus** | Network types, formatting, nullability | Business rules, domain invariants, logic | Schema types, table constraints, indices |
| **Identity** | None (Transient message) | Unique domain identity (UUID/ID) | Primary Key (`id`, autoincrement, UUID) |
| **Methods** | Field serializers, validators | Business methods, state transitions | ORM relationships, column definitions |
| **Serialization** | JSON, dict via `model_dump()` | `asdict()` or manual | Serialized only via repository mapping |

---

## 4. Practical Implementation: 5 Verified Examples

### Example 1: The Three Separate Models Defined
Defining a Customer entity across its three representations cleanly.

```python
# 1. PURE DOMAIN MODEL (app/domain/customer.py)
from dataclasses import dataclass
from datetime import datetime, timezone
import uuid

class InactiveCustomerError(Exception):
    pass

@dataclass
class CustomerDomain:
    email: str
    full_name: str
    id: str = ""
    is_active: bool = True
    tier: str = "STANDARD"
    created_at: datetime = ""

    def __post_init__(self):
        if not self.id:
            self.id = str(uuid.uuid4())
        if not self.created_at:
            self.created_at = datetime.now(timezone.utc)

    def upgrade_to_vip(self) -> None:
        if not self.is_active:
            raise InactiveCustomerError("Cannot upgrade an inactive customer to VIP")
        self.tier = "VIP"
```

```python
# 2. PERSISTENCE ENTITY (app/infrastructure/orm/customer_table.py)
from sqlalchemy import Column, String, Boolean, DateTime
from sqlalchemy.orm import declarative_base

Base = declarative_base()

class CustomerORM(Base):
    __tablename__ = "customers"

    id = Column(String(36), primary_key=True)
    email = Column(String(255), nullable=False, unique=True, index=True)
    full_name = Column(String(100), nullable=False)
    is_active = Column(Boolean, nullable=False, default=True)
    tier = Column(String(20), nullable=False, default="STANDARD")
    hashed_password = Column(String(255), nullable=False)  # Sensitive DB-only field!
    created_at = Column(DateTime(timezone=True), nullable=False)
```

```python
# 3. DTOs: PRESENTATION SCHEMAS (app/schemas/customer.py)
from datetime import datetime
from pydantic import BaseModel, EmailStr, Field, ConfigDict

class CustomerCreateRequestDTO(BaseModel):
    """Input contract: Accepts plain password from client."""
    email: EmailStr
    full_name: str = Field(min_length=2, max_length=100)
    password: str = Field(min_length=8, max_length=128)

class CustomerResponseDTO(BaseModel):
    """Output contract: Strictly omits password and internal flags."""
    id: str
    email: EmailStr
    full_name: str
    tier: str
    created_at: datetime
    model_config = ConfigDict(from_attributes=True)
```

---

### Example 2: The Repository Mapper (ORM $\longleftrightarrow$ Domain)
The repository encapsulates conversion between `CustomerORM` and `CustomerDomain`.

```python
# app/infrastructure/repositories/customer_repository.py
from typing import Optional
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession
from app.domain.customer import CustomerDomain
from app.infrastructure.orm.customer_table import CustomerORM

class CustomerRepository:
    def __init__(self, session: AsyncSession):
        self._session = session

    async def get_by_id(self, customer_id: str) -> Optional[CustomerDomain]:
        stmt = select(CustomerORM).where(CustomerORM.id == customer_id)
        result = await self._session.execute(stmt)
        orm_obj = result.scalar_one_or_none()
        if not orm_obj:
            return None
        return self._to_domain(orm_obj)

    async def save(self, domain_customer: CustomerDomain, hashed_pwd: str) -> None:
        stmt = select(CustomerORM).where(CustomerORM.id == domain_customer.id)
        result = await self._session.execute(stmt)
        orm_obj = result.scalar_one_or_none()

        if orm_obj:
            # Update existing row
            orm_obj.email = domain_customer.email
            orm_obj.full_name = domain_customer.full_name
            orm_obj.is_active = domain_customer.is_active
            orm_obj.tier = domain_customer.tier
        else:
            # Insert new row
            orm_obj = CustomerORM(
                id=domain_customer.id,
                email=domain_customer.email,
                full_name=domain_customer.full_name,
                is_active=domain_customer.is_active,
                tier=domain_customer.tier,
                hashed_password=hashed_pwd,
                created_at=domain_customer.created_at,
            )
            self._session.add(orm_obj)
        await self._session.flush()

    @staticmethod
    def _to_domain(orm: CustomerORM) -> CustomerDomain:
        return CustomerDomain(
            id=orm.id,
            email=orm.email,
            full_name=orm.full_name,
            is_active=orm.is_active,
            tier=orm.tier,
            created_at=orm.created_at,
        )
```

---

### Example 3: Application Service Layer Orchestration
The Service coordinates business logic on the domain model and orchestrates persistence.

```python
# app/services/customer_service.py
from app.domain.customer import CustomerDomain
from app.infrastructure.repositories.customer_repository import CustomerRepository

class CustomerService:
    def __init__(self, repo: CustomerRepository):
        self._repo = repo

    async def register_customer(self, email: str, full_name: str, raw_password: str) -> CustomerDomain:
        # Mock password hashing (in production: bcrypt / argon2)
        hashed_password = f"mock_hash_{raw_password}"

        # Instantiate Domain Model
        customer = CustomerDomain(email=email, full_name=full_name)

        # Persist through repository
        await self._repo.save(customer, hashed_pwd=hashed_password)
        return customer

    async def upgrade_customer_tier(self, customer_id: str) -> CustomerDomain:
        customer = await self._repo.get_by_id(customer_id)
        if not customer:
            raise KeyError(f"Customer {customer_id} not found")

        # Execute pure domain invariant method
        customer.upgrade_to_vip()

        # Save changes
        await self._repo.save(customer, hashed_pwd="")
        return customer
```

---

### Example 4: Presentation Layer (FastAPI Router)
Converting Request DTO $\to$ Service $\to$ Domain Model $\to$ Response DTO.

```python
# app/api/v1/customers.py
from fastapi import APIRouter, Depends, HTTPException, status
from app.schemas.customer import CustomerCreateRequestDTO, CustomerResponseDTO
from app.services.customer_service import CustomerService
from app.domain.customer import InactiveCustomerError

router = APIRouter(prefix="/customers", tags=["Customers"])

# Dummy dependency provider
def get_customer_service() -> CustomerService:
    pass  # pragma: no cover

@router.post("/", response_model=CustomerResponseDTO, status_code=status.HTTP_201_CREATED)
async def create_customer(
    payload: CustomerCreateRequestDTO,
    service: CustomerService = Depends(get_customer_service),
):
    # DTO validates incoming wire types; service executes domain logic
    customer = await service.register_customer(
        email=payload.email,
        full_name=payload.full_name,
        raw_password=payload.password,
    )
    # Map domain output back to Response DTO (safely omitting password hash)
    return CustomerResponseDTO(
        id=customer.id,
        email=customer.email,
        full_name=customer.full_name,
        tier=customer.tier,
        created_at=customer.created_at,
    )

@router.post("/{customer_id}/upgrade-vip", response_model=CustomerResponseDTO)
async def upgrade_customer(
    customer_id: str,
    service: CustomerService = Depends(get_customer_service),
):
    try:
        customer = await service.upgrade_customer_tier(customer_id)
        return CustomerResponseDTO(
            id=customer.id,
            email=customer.email,
            full_name=customer.full_name,
            tier=customer.tier,
            created_at=customer.created_at,
        )
    except KeyError as exc:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail=str(exc))
    except InactiveCustomerError as exc:
        raise HTTPException(status_code=status.HTTP_409_CONFLICT, detail=str(exc))
```

---

### Example 5: Testing Domain Logic Without DB or FastAPI
Demonstrating that domain logic can be verified in microseconds with zero network or database dependencies.

```python
# tests/unit/test_customer_domain.py
import pytest
from app.domain.customer import CustomerDomain, InactiveCustomerError

def test_customer_upgrade_to_vip():
    customer = CustomerDomain(email="alice@example.com", full_name="Alice Smith")
    assert customer.tier == "STANDARD"

    customer.upgrade_to_vip()
    assert customer.tier == "VIP"

def test_inactive_customer_upgrade_raises_error():
    customer = CustomerDomain(email="bob@example.com", full_name="Bob Jones", is_active=False)
    with pytest.raises(InactiveCustomerError, match="Cannot upgrade an inactive customer"):
        customer.upgrade_to_vip()
```

---

## 5. Edge Cases, Pitfalls & Failure Modes

1. **Lazy Loading on Detached ORM Instances**: If a repository returns an ORM model directly to the router, and Pydantic attempts to serialize a related table via `from_attributes=True` outside of an active database session, SQLAlchemy raises `DetachedInstanceError`. Mapping ORM instances to Domain dataclasses inside the repository completely eliminates this bug.
2. **Accidental Mutation of ORM Identity Map**: When using SQLAlchemy ORM models as domain models, calling `user.deactivate()` immediately flags the ORM model as dirty in the session unit of work. If an unhandled exception occurs later in the request, a subsequent flush can inadvertently persist half-completed mutations.
3. **Mass Assignment Vulnerabilities**: If an endpoint accepts `**payload.dict()` and directly initializes an ORM model (`UserORM(**payload.dict())`), any database column present on the table (such as `is_superuser` or `role`) can be set by the client if not explicitly excluded from the Pydantic schema.
4. **Serialization Loops**: Relational ORM models with bidirectional relationships (`Parent.children` and `Child.parent`) cause infinite recursion when serialized directly with standard JSON encoders or naive Pydantic models.

---

## 6. Common Mistakes Table

| Mistake | Root Cause | Impact | Correct Approach |
| :--- | :--- | :--- | :--- |
| **Returning SQLAlchemy ORM from Routes** | Skipping DTO mapping for speed | Leaks database schema, password hashes, and causes `DetachedInstanceError`. | Map ORM records to explicit Pydantic Response DTOs. |
| **Using One Model for Request and DB** | Misunderstanding SQLModel/Pydantic | Over-posting security vulnerabilities; DB migrations break API clients. | Separate models: Request DTO, Domain Model, and SQLAlchemy Table. |
| **Putting Business Logic in Pydantic Validators** | Misusing `@field_validator` | Business rules run during JSON parsing; cannot be called from background workers. | Use Pydantic only for structural format checks; keep domain rules in Domain entities. |
| **Putting Business Logic in ORM Models** | Django/ActiveRecord habits | Mixes persistence with domain rules; requires live DB to unit test. | Place business logic in pure Python dataclasses / Domain Aggregates. |
| **Exposing Foreign Keys in Public DTOs** | Copying internal database columns | Exposes internal database topology to public API consumers. | Expose semantic identifiers or nested resource URLs instead of raw integer IDs. |
| **Passing Request DTOs into Repositories** | Skipping the domain layer | Repositories become coupled to HTTP presentation schemas. | Pass Domain entities or primitive parameters into repository methods. |

---

## 7. Performance & Scalability Considerations

- **Mapping Overhead**: Converting records from ORM $\to$ Domain $\to$ DTO takes a negligible fraction of a millisecond in Python 3.12+ (typically 2–5 microseconds per object). In standard API workflows, this is dwarfed by network and database I/O (10–50ms).
- **High-Throughput Projection Optimization**: For read-only bulk reporting endpoints returning 50,000 records, bypass the Domain layer and use SQLAlchemy's `session.stream()` or raw column mappings to construct Pydantic DTOs directly via `TypeAdapter.validate_python()`.
- **Free-Threading in Python 3.13t**: Pure domain dataclasses and Pydantic v2 (compiled in Rust) run without the GIL, allowing multi-threaded FastAPI workers to map and validate high-concurrency requests across multiple CPU cores simultaneously.

---

## 8. Security & OWASP Guardrails

- **OWASP A01 (Broken Object-Level Authorization)**: Strict separation prevents Mass Assignment. The `UserCreateRequestDTO` defines exactly which fields the user is permitted to provide; administrative attributes (`is_admin`, `verified`) are excluded from the request schema and can only be set by authorized domain methods.
- **Data Protection & PII Masking**: By separating the Persistence Entity from the Response DTO, sensitive fields like `password_hash`, `ssn`, or `stripe_customer_id` are never included in the outgoing DTO definition, eliminating accidental credential exposure in API responses.

---

## 9. Testing Strategies & Pytest Recipes

```python
# tests/test_dto_isolation.py
import pytest
from pydantic import ValidationError
from app.schemas.customer import CustomerCreateRequestDTO

def test_request_dto_rejects_short_password():
    with pytest.raises(ValidationError) as exc:
        CustomerCreateRequestDTO(
            email="test@example.com",
            full_name="Valid Name",
            password="short"  # Less than 8 chars
        )
    assert "password" in str(exc.value)

def test_request_dto_ignores_extra_malicious_fields():
    # Demonstrates mass assignment defense
    payload = {
        "email": "test@example.com",
        "full_name": "Valid Name",
        "password": "valid_secure_password",
        "is_admin": True,  # Malicious field injected
        "balance": 1000000,
    }
    dto = CustomerCreateRequestDTO.model_validate(payload)
    # Injected fields are rejected or ignored; not present on DTO
    assert not hasattr(dto, "is_admin")
    assert not hasattr(dto, "balance")
```

---

## 10. Real-World Case Study / Production Incident

A popular gig-economy delivery startup built their FastAPI backend using a single Pydantic-SQLAlchemy hybrid model. Every API endpoint accepted a Pydantic model that inherited directly from the SQLAlchemy ORM table.

**The Exploit**:
A tech-savvy delivery courier noticed that when updating their bank payout profile via `PUT /api/v1/couriers/me`, the endpoint executed:
```python
@router.put("/couriers/me")
async def update_profile(payload: CourierModel, db: AsyncSession = Depends(get_db)):
    courier = await db.get(CourierModel, current_user.id)
    for field, value in payload.model_dump(exclude_unset=True).items():
        setattr(courier, field, value)
    await db.commit()
```
The courier inspected the OpenAPI specification and noticed an internal column named `base_pay_multiplier` (default `1.0`). By crafting a manual HTTP request:
```json
{
  "full_name": "John Doe",
  "base_pay_multiplier": 5.0
}
```
The courier successfully multiplied their delivery earnings by 5x. By the time finance detected the anomaly 3 months later, over \$180,000 had been overpaid across dozens of couriers sharing the exploit on private forums.

**The Fix**:
The team instituted a strict policy prohibiting ORM models from ever appearing in router signatures:
1. Replaced the generic update payload with a strict `CourierProfileUpdateRequestDTO` containing only `phone_number` and `vehicle_type`.
2. All administrative columns were moved to a separate domain method accessible only by internal administrative services.

---

## 11. Technical Interview Questions

<details>
<summary>1. What is the difference between a DTO, a Domain Model, and a Persistence Entity?</summary>
A DTO (Data Transfer Object) defines the network contract for request and response payloads, handling serialization and format validation. A Domain Model encapsulates core business logic, invariants, and state transitions using pure Python without infrastructure dependencies. A Persistence Entity (ORM model) maps database table schemas, foreign keys, and indexes for data storage.
</details>

<details>
<summary>2. What is a "Mass Assignment" (or Over-Posting) vulnerability, and how does model separation prevent it?</summary>
Mass assignment occurs when client-supplied JSON is bound directly to database entities without filtering, allowing an attacker to overwrite sensitive internal columns (e.g., `is_admin`, `role`, `account_balance`). Separate DTOs prevent this by defining explicit white-lists of permitted input fields, ensuring that internal database columns cannot be deserialized from incoming network requests.
</details>

<details>
<summary>3. Why does returning SQLAlchemy ORM models from FastAPI endpoints cause `DetachedInstanceError`?</summary>
FastAPI serializes responses into JSON after the route handler function returns. If the route handler used an `AsyncSession` context that closed upon exit, accessing any lazily loaded relationships on the ORM model during serialization occurs outside an active database session, causing SQLAlchemy to raise `DetachedInstanceError`.
</details>

<details>
<summary>4. What is the role of `model_config = ConfigDict(from_attributes=True)` in Pydantic v2?</summary>
`from_attributes=True` (formerly `orm_mode=True` in Pydantic v1) allows Pydantic to read data attributes directly from object properties (like dataclass attributes or ORM getters: `obj.email`) rather than requiring a dictionary lookup (`obj["email"]`). This enables seamless mapping from domain models or ORM records into response DTOs.
</details>

<details>
<summary>5. Where should mapping between ORM entities and Domain models occur?</summary>
Mapping between ORM entities and Domain models should occur exclusively inside the **Repository Layer**. Higher layers (Services and Presentation) should never import or interact with SQLAlchemy ORM entities.
</details>

<details>
<summary>6. Why shouldn't domain models import Pydantic's `BaseModel`?</summary>
Domain models represent enduring business rules that should be decoupled from specific third-party libraries. If domain models inherit from `BaseModel`, changes to Pydantic's internal mechanics or validation behaviors can alter core business logic. Furthermore, Pydantic is designed for parsing and serialization, whereas domain models are designed for invariant encapsulation and state mutation.
</details>

<details>
<summary>7. How do you handle read-only reporting queries that require data from 10 different tables without violating clean architecture?</summary>
For complex analytical or reporting read queries, forcing data through multiple domain aggregates introduces excessive mapping overhead. In CQRS, the read pipeline can bypass domain aggregates completely: the repository executes an optimized projection query and maps the result directly into a dedicated Read DTO.
</details>

<details>
<summary>8. How do you prevent sensitive database fields (e.g., password hashes) from leaking into OpenAPI documentation?</summary>
By ensuring that endpoint response schemas (`response_model=...`) reference explicit Response DTOs that do not define those fields. Because FastAPI generates OpenAPI documentation directly from the response DTO schema, unincluded fields never appear in Swagger documentation or network responses.
</details>

---

## 12. Practical Exercises & Solutions

<details>
<summary>Exercise 1 (Easy): Create a Response DTO that Excludes Password Hash</summary>

**Task**: Given a domain object `User(id="1", username="admin", password_hash="secret_hash")`, write a Pydantic v2 `UserPublicDTO` with `from_attributes=True` that serializes only `id` and `username`.

**Solution**:
```python
from pydantic import BaseModel, ConfigDict
from dataclasses import dataclass

@dataclass
class User:
    id: str
    username: str
    password_hash: str

class UserPublicDTO(BaseModel):
    id: str
    username: str
    model_config = ConfigDict(from_attributes=True)

user = User(id="1", username="admin", password_hash="secret_hash")
dto = UserPublicDTO.model_validate(user)
assert dto.model_dump() == {"id": "1", "username": "admin"}
assert "password_hash" not in dto.model_dump()
```
</details>

<details>
<summary>Exercise 2 (Easy): Mapper Function from ORM to Domain</summary>

**Task**: Write a standalone mapper function `orm_to_domain(orm: ProductORM) -> ProductDomain` converting an ORM instance to a pure dataclass.

**Solution**:
```python
from dataclasses import dataclass
from decimal import Decimal

@dataclass
class ProductDomain:
    id: str
    name: str
    price: Decimal

class ProductORM:
    def __init__(self, id, name, price):
        self.id = id
        self.name = name
        self.price = price

def orm_to_domain(orm: ProductORM) -> ProductDomain:
    return ProductDomain(
        id=orm.id,
        name=orm.name,
        price=Decimal(str(orm.price)),
    )
```
</details>

<details>
<summary>Exercise 3 (Medium): Pydantic DTO with Custom Field Transformation</summary>

**Task**: Create an `OrderResponseDTO` where the total amount in cents (`total_cents: int`) from the domain model is serialized into a formatted dollar string (e.g. `1050` $\to$ `"$10.50"`).

**Solution**:
```python
from pydantic import BaseModel, field_serializer

class OrderResponseDTO(BaseModel):
    order_id: str
    total_cents: int

    @field_serializer("total_cents")
    def format_as_dollars(self, total_cents: int) -> str:
        dollars = total_cents / 100.0
        return f"${dollars:.2f}"
```
</details>

<details>
<summary>Exercise 4 (Medium): Enforcing Invariants in Domain Model vs Input Formatting in DTO</summary>

**Task**: Implement a `BookRoomRequestDTO` that validates date strings are in `YYYY-MM-DD` format. Implement a `BookingDomain` model that asserts `checkout_date > checkin_date`, raising `ValueError("Checkout must be after checkin")`.

**Solution**:
```python
from datetime import date
from pydantic import BaseModel
from dataclasses import dataclass

class BookRoomRequestDTO(BaseModel):
    checkin: date
    checkout: date

@dataclass
class BookingDomain:
    room_id: str
    checkin: date
    checkout: date

    def __post_init__(self):
        if self.checkout <= self.checkin:
            raise ValueError("Checkout must be after checkin")
```
</details>

<details>
<summary>Exercise 5 (Hard): Repository with Bidirectional ORM-Domain Mapping</summary>

**Task**: Implement a mock repository with `get(id)` and `save(domain_entity)` that maps a complex `InvoiceDomain(id, number, lines: list[InvoiceLine])` to `InvoiceORM` and `InvoiceLineORM`.

**Solution**:
```python
from dataclasses import dataclass
from typing import List

@dataclass
class InvoiceLine:
    description: str
    amount: float

@dataclass
class InvoiceDomain:
    id: str
    number: str
    lines: List[InvoiceLine]

class MockInvoiceRepo:
    def __init__(self):
        self._orm_tables = {}

    def save(self, invoice: InvoiceDomain):
        # Map domain to mock ORM structure
        orm_lines = [{"desc": l.description, "amt": l.amount} for l in invoice.lines]
        self._orm_tables[invoice.id] = {
            "id": invoice.id,
            "number": invoice.number,
            "lines": orm_lines
        }

    def get(self, invoice_id: str) -> InvoiceDomain | None:
        data = self._orm_tables.get(invoice_id)
        if not data:
            return None
        domain_lines = [InvoiceLine(description=l["desc"], amount=l["amt"]) for l in data["lines"]]
        return InvoiceDomain(id=data["id"], number=data["number"], lines=domain_lines)
```
</details>

<details>
<summary>Exercise 6 (Hard): FastAPI Controller with Strict DTO Transformation Pipeline</summary>

**Task**: Implement a complete FastAPI route `POST /invoices` that accepts `InvoiceCreateDTO`, delegates creation to a mock service returning `InvoiceDomain`, and responds with `InvoiceResponseDTO` that includes a calculated `total_amount` field computed from the domain lines.

**Solution**:
```python
from fastapi import APIRouter, status
from pydantic import BaseModel
from typing import List
import uuid

router = APIRouter()

class InvoiceLineDTO(BaseModel):
    description: str
    amount: float

class InvoiceCreateDTO(BaseModel):
    number: str
    lines: List[InvoiceLineDTO]

class InvoiceResponseDTO(BaseModel):
    id: str
    number: str
    total_amount: float

@router.post("/invoices", response_model=InvoiceResponseDTO, status_code=status.HTTP_201_CREATED)
async def create_invoice(payload: InvoiceCreateDTO):
    # In real app: convert to domain, save in repo
    invoice_id = str(uuid.uuid4())
    total = sum(l.amount for l in payload.lines)
    
    return InvoiceResponseDTO(
        id=invoice_id,
        number=payload.number,
        total_amount=total,
    )
```
</details>
