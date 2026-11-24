---
title: "Hexagonal Architecture and Clean Architecture in FastAPI"
description: "Implement Hexagonal Architecture (Ports and Adapters) and Clean Architecture in FastAPI: decouple business logic from web frameworks and databases, define inbound and outbound ports with Python Protocols, and build swappable adapters."
fastapi_version: "0.115.x+"
python_version: "3.12+"
pydantic_version: "v2.10+"
tested_with: "pydantic 2.10+, fastapi 0.115+, pytest 8.3+"
free_threading_status: "Fully Compatible"
---

# Hexagonal Architecture and Clean Architecture in FastAPI

## 1. Conceptual Foundation & Mental Model

### The Dependency Inversion Imperative
Traditional web application frameworks encourage developers to organize software around data models or transport mechanisms. In such designs, the database is considered the architectural center, with business logic tightly coupled to ORM entities and route handlers.

**Hexagonal Architecture** (also known as **Ports and Adapters**, coined by Alistair Cockburn) and **Clean Architecture** (Robert C. Martin) invert this mindset completely. In these architectures:
1. **The Domain is the Core**: Pure business rules, domain entities, and core calculations reside at the center of the application, entirely free of external dependencies.
2. **Frameworks are Details**: FastAPI, SQLAlchemy, PostgreSQL, Redis, and Stripe are external infrastructural details that exist on the periphery.
3. **The Dependency Rule**: Source code dependencies must point **strictly inward**, toward higher-level policies:
   $$\text{Outer Layers (Adapters)} \longrightarrow \text{Inner Layers (Domain / Use Cases)}$$

```
+---------------------------------------------------------------------------------+
|                       EXTERNAL INFRASTRUCTURE & ADAPTERS                        |
|                                                                                 |
|   [FastAPI Router] (Driving Adapter)       [PostgreSQL DB] (Driven Adapter)     |
|         |                                        ^                              |
|         v (Calls Inbound Port)                   | (Implements Outbound Port)   |
|   +-------------------------------------------------------------------------+   |
|   |                       APPLICATION USE CASES                             |   |
|   |                       (Application Services)                            |   |
|   |   +-----------------------------------------------------------------+   |   |
|   |   |                         DOMAIN CORE                             |   |   |
|   |   |  - Pure Domain Entities (Dataclasses)                           |   |   |
|   |   |  - Domain Value Objects & Invariants                            |   |   |
|   |   |  - Domain Exceptions & Events                                   |   |   |
|   |   +-----------------------------------------------------------------+   |   |
|   +-------------------------------------------------------------------------+   |
|         ^                                        |                              |
|         |                                        v                              |
|   [Kafka Consumer] (Driving Adapter)       [Stripe API Client] (Driven Adapter) |
+---------------------------------------------------------------------------------+
```

### Ports vs. Adapters
- **Port (Interface / Protocol)**: A specification of what an actor requires or provides. In Python, ports are represented as abstract classes or `typing.Protocol` contracts.
  - **Driving (Inbound / Primary) Ports**: Entry points into the application (e.g., `RegisterUserUseCase`).
  - **Driven (Outbound / Secondary) Ports**: Interfaces required by the application to talk to the outside world (e.g., `UserRepositoryPort`, `EmailNotificationPort`).
- **Adapter**: A concrete implementation of a port.
  - **Driving Adapters**: Convert outside triggers into application calls (e.g., a FastAPI route handler or a CLI script invoking a use-case).
  - **Driven Adapters**: Concrete implementations of outbound ports that talk to external infrastructure (e.g., `PostgresUserRepository`, `SendGridNotificationAdapter`).

---

## 2. Architecture & Internal Mechanics

Understanding the inward-pointing dependency graph is essential for building a compliant hexagonal system.

```
                    [Driving Side]                                  [Driven Side]
                                          +------------------+
                                          |   DOMAIN CORE    |
                                          | (Entities, Rules)|
                                          +------------------+
                                                   ^
                                                   | (Imports)
                                          +------------------+
                                          |   USE CASES      |
                                          | (App Services)   |
                                          +------------------+
                                            ^              ^
                (Calls Inbound Port)       /                \ (Implements Outbound Port)
                                          /                  \
+----------------------+                 /                    \    +-----------------------+
|  FASTAPI CONTROLLER  | ---------------+                      +---|  POSTGRES REPOSITORY  |
|  (Driving Adapter)   |                                           |  (Driven Adapter)     |
+----------------------+                                           +-----------------------+
```

### Boundary Independence
The domain core contains zero imports from:
- `fastapi` or `starlette`
- `sqlalchemy`, `pydantic`, or `databases`
- `requests`, `httpx`, or any third-party SDKs

If you run `pip freeze` in the domain package, it requires only the Python standard library.

---

## 3. Comprehensive Syntax & Core Mechanics

### Directory Structure for Clean / Hexagonal Architecture
```
app/
├── domain/                      # 1. Domain Core (Zero external dependencies)
│   ├── models/
│   │   ├── user.py
│   │   └── value_objects.py
│   └── exceptions.py
├── application/                 # 2. Application Layer (Use Cases & Ports)
│   ├── ports/
│   │   ├── inbound/             # Driving Ports (Use-Case interfaces)
│   │   │   └── create_user.py
│   │   └── outbound/            # Driven Ports (Storage, Network, Email)
│   │       ├── user_repository.py
│   │       └── notification_service.py
│   └── use_cases/               # Use Case Implementations
│       └── create_user_service.py
└── infrastructure/              # 3. Adapters (Web, DB, Third-party)
    ├── adapters/
    │   ├── driven/
    │   │   ├── postgres_user_repo.py
    │   │   └── sendgrid_notifier.py
    │   └── driving/
    │       └── web/             # FastAPI Presentation Layer
    │           ├── v1/
    │           │   ├── controllers.py
    │           │   └── schemas.py
    │           └── dependencies.py
    └── config/
```

---

## 4. Practical Implementation: 5 Verified Examples

### Example 1: Pure Domain Core (Entities and Value Objects)
Pure Python domain models encapsulating business logic with standard dataclasses.

```python
# app/domain/models/user.py
from dataclasses import dataclass
from datetime import datetime, timezone
import re
import uuid

class DomainError(Exception):
    pass

class InvalidEmailError(DomainError):
    pass

@dataclass(frozen=True)
class Email:
    value: str

    def __post_init__(self):
        email_regex = r"^[\w\.-]+@[\w\.-]+\.\w+$"
        if not re.match(email_regex, self.value):
            raise InvalidEmailError(f"'{self.value}' is not a valid email address")

@dataclass
class User:
    email: Email
    full_name: str
    id: str = ""
    is_active: bool = True
    created_at: datetime = ""

    def __post_init__(self):
        if not self.id:
            object.__setattr__(self, "id", str(uuid.uuid4()))
        if not self.created_at:
            object.__setattr__(self, "created_at", datetime.now(timezone.utc))

    def deactivate(self) -> None:
        if not self.is_active:
            raise DomainError("User is already deactivated")
        self.is_active = False
```

---

### Example 2: Driven (Outbound) Ports
Defining outbound contracts using `typing.Protocol`.

```python
# app/application/ports/outbound/user_repository.py
from typing import Protocol, Optional
from app.domain.models.user import User, Email

class UserRepositoryPort(Protocol):
    async def get_by_id(self, user_id: str) -> Optional[User]: ...
    async def get_by_email(self, email: Email) -> Optional[User]: ...
    async def save(self, user: User) -> User: ...
```

```python
# app/application/ports/outbound/notification_service.py
from typing import Protocol
from app.domain.models.user import User

class NotificationServicePort(Protocol):
    async def send_welcome_email(self, user: User) -> None: ...
```

---

### Example 3: Application Use Case (Driving Port Implementation)
Orchestrating the creation of a user without any knowledge of HTTP or SQL.

```python
# app/application/use_cases/create_user.py
from dataclasses import dataclass
from app.domain.models.user import User, Email, DomainError
from app.application.ports.outbound.user_repository import UserRepositoryPort
from app.application.ports.outbound.notification_service import NotificationServicePort

class UserAlreadyExistsError(DomainError):
    pass

@dataclass(frozen=True)
class CreateUserCommand:
    email: str
    full_name: str

class CreateUserUseCase:
    def __init__(
        self,
        user_repo: UserRepositoryPort,
        notifier: NotificationServicePort,
    ):
        self._user_repo = user_repo
        self._notifier = notifier

    async def execute(self, command: CreateUserCommand) -> User:
        email_vo = Email(command.email)
        
        # Verify uniqueness
        existing_user = await self._user_repo.get_by_email(email_vo)
        if existing_user:
            raise UserAlreadyExistsError(f"User with email {command.email} already registered")

        # Create pure domain entity
        new_user = User(email=email_vo, full_name=command.full_name)
        
        # Persist via outbound port
        saved_user = await self._user_repo.save(new_user)
        
        # Notify via outbound port
        await self._notifier.send_welcome_email(saved_user)
        
        return saved_user
```

---

### Example 4: Driven Adapters (Postgres and Email Mock)
Concrete infrastructure implementations fulfilling the outbound ports.

```python
# app/infrastructure/adapters/driven/in_memory_user_repo.py
from typing import Optional
from app.domain.models.user import User, Email
from app.application.ports.outbound.user_repository import UserRepositoryPort

class InMemoryUserRepository(UserRepositoryPort):
    def __init__(self):
        self._storage: dict[str, User] = {}

    async def get_by_id(self, user_id: str) -> Optional[User]:
        return self._storage.get(user_id)

    async def get_by_email(self, email: Email) -> Optional[User]:
        for user in self._storage.values():
            if user.email.value.lower() == email.value.lower():
                return user
        return None

    async def save(self, user: User) -> User:
        self._storage[user.id] = user
        return user
```

```python
# app/infrastructure/adapters/driven/logging_notifier.py
import logging
from app.domain.models.user import User
from app.application.ports.outbound.notification_service import NotificationServicePort

logger = logging.getLogger(__name__)

class LoggingNotificationAdapter(NotificationServicePort):
    async def send_welcome_email(self, user: User) -> None:
        logger.info("Outbound notification sent to %s for user %s", user.email.value, user.id)
```

---

### Example 5: Driving Adapter (FastAPI Controller & Composition Root)
FastAPI functions strictly as an HTTP adapter translating incoming JSON into commands and invoking the use case.

```python
# app/infrastructure/adapters/driving/web/v1/controllers.py
from datetime import datetime
from fastapi import APIRouter, Depends, HTTPException, status
from pydantic import BaseModel, Field
from app.domain.models.user import InvalidEmailError
from app.application.use_cases.create_user import (
    CreateUserUseCase, CreateUserCommand, UserAlreadyExistsError
)
from app.infrastructure.adapters.driven.in_memory_user_repo import InMemoryUserRepository
from app.infrastructure.adapters.driven.logging_notifier import LoggingNotificationAdapter

router = APIRouter(prefix="/users", tags=["Users"])

# Pydantic DTO (Transport Layer)
class UserCreateRequestDTO(BaseModel):
    email: str = Field(min_length=5, max_length=255)
    full_name: str = Field(min_length=1, max_length=100)

class UserResponseDTO(BaseModel):
    id: str
    email: str
    full_name: str
    is_active: bool
    created_at: datetime

# Composition Root (Dependency Wiring)
_singleton_repo = InMemoryUserRepository()
_singleton_notifier = LoggingNotificationAdapter()

def get_create_user_use_case() -> CreateUserUseCase:
    return CreateUserUseCase(user_repo=_singleton_repo, notifier=_singleton_notifier)

@router.post("/", response_model=UserResponseDTO, status_code=status.HTTP_201_CREATED)
async def create_user_endpoint(
    payload: UserCreateRequestDTO,
    use_case: CreateUserUseCase = Depends(get_create_user_use_case),
):
    command = CreateUserCommand(email=payload.email, full_name=payload.full_name)
    try:
        user = await use_case.execute(command)
        return UserResponseDTO(
            id=user.id,
            email=user.email.value,
            full_name=user.full_name,
            is_active=user.is_active,
            created_at=user.created_at,
        )
    except InvalidEmailError as exc:
        raise HTTPException(status_code=status.HTTP_422_UNPROCESSABLE_ENTITY, detail=str(exc))
    except UserAlreadyExistsError as exc:
        raise HTTPException(status_code=status.HTTP_409_CONFLICT, detail=str(exc))
```

---

## 5. Edge Cases, Pitfalls & Failure Modes

1. **Framework Leakage into Domain**: Importing Pydantic's `BaseModel` or SQLAlchemy's `declarative_base` inside the `domain/` directory violates clean architecture. Use standard Python `dataclasses` for domain entities.
2. **DTO Leaks to Use Cases**: Passing Pydantic request models directly into use case methods (`use_case.execute(request_dto)`) couples the use case to Pydantic and the HTTP schema. Always unpack DTOs into primitive commands or value objects.
3. **Driving Port Bypassing**: Allowing a controller to directly access an outbound repository to perform a quick query bypasses business policies and use cases.
4. **Adapter Incompleteness**: Writing concrete adapters that do not fully implement every method declared on the `typing.Protocol` port will cause runtime `AttributeError` when alternate paths are called.

---

## 6. Common Mistakes Table

| Mistake | Root Cause | Impact | Correct Approach |
| :--- | :--- | :--- | :--- |
| **Importing FastAPI in the Domain** | Raising `HTTPException` inside entities | Domain logic cannot run outside HTTP contexts (CLI, workers). | Raise custom domain exceptions; let the web adapter map them to HTTP status codes. |
| **Using Pydantic Models as Entities** | Avoiding dataclass boilerplate | Domain couples to Pydantic internals and validation mechanics. | Use pure Python `@dataclass` for domain entities; reserve Pydantic for DTOs. |
| **Passing Pydantic DTOs into Use Cases** | Developer laziness | Use cases become dependent on HTTP presentation schemas. | Map Pydantic DTOs to primitive `@dataclass(frozen=True)` Commands. |
| **Creating Anemic Domain Entities** | Placing all logic in Use Cases | Entities become passive data structures; rules scatter everywhere. | Encapsulate invariants and state transitions directly on domain entities. |
| **Coupling Repositories to ORM Output** | Returning SQLAlchemy instances from adapters | Exposes database internals; triggers `DetachedInstanceError`. | Adapters must map ORM tables to pure Domain entities before returning. |
| **Over-Engineering Simple CRUD** | Applying 4-layer Hexagonal architecture to a 2-field table | Massive boilerplate and slower delivery for trivial applications. | Reserve Hexagonal architecture for rich domain logic; use simpler layering for basic CRUD. |

---

## 7. Performance & Scalability Considerations

- **Mapping Overhead**: Converting from Database ORM $\to$ Domain Dataclass $\to$ Pydantic DTO introduces object instantiation overhead. For high-volume streaming endpoints (e.g., CSV exports), provide a bypass query port that streams data directly.
- **Python 3.13+ Free-Threading**: Clean architecture isolates pure, thread-safe domain logic. In free-threaded Python (`3.13t`), running complex domain calculations across multiple threads achieves true linear multi-core parallelism without GIL contention.

---

## 8. Security & OWASP Guardrails

- **OWASP A01 (Broken Object-Level Authorization)**: Enforce authorization checks inside the **Application Use Case** (e.g., verifying that `current_user.id == target_resource.owner_id`), not solely in FastAPI route dependencies. This ensures authorization cannot be bypassed by CLI tools or message queue consumers.
- **Data Validation at the Gate**: Validate transport formats in the driving adapter (Pydantic) and business invariants in the domain core (Value Objects).

---

## 9. Testing Strategies & Pytest Recipes

```python
# tests/unit/test_clean_use_case.py
import pytest
from app.domain.models.user import InvalidEmailError
from app.application.use_cases.create_user import (
    CreateUserUseCase, CreateUserCommand, UserAlreadyExistsError
)
from app.infrastructure.adapters.driven.in_memory_user_repo import InMemoryUserRepository

class FakeNotificationAdapter:
    def __init__(self):
        self.sent_users = []

    async def send_welcome_email(self, user):
        self.sent_users.append(user)

@pytest.mark.asyncio
async def test_create_user_successful():
    repo = InMemoryUserRepository()
    notifier = FakeNotificationAdapter()
    use_case = CreateUserUseCase(user_repo=repo, notifier=notifier)

    cmd = CreateUserCommand(email="test@example.com", full_name="Alice Smith")
    user = await use_case.execute(cmd)

    assert user.email.value == "test@example.com"
    assert len(notifier.sent_users) == 1
    assert notifier.sent_users[0].id == user.id

@pytest.mark.asyncio
async def test_create_user_invalid_email_raises_domain_error():
    repo = InMemoryUserRepository()
    notifier = FakeNotificationAdapter()
    use_case = CreateUserUseCase(user_repo=repo, notifier=notifier)

    cmd = CreateUserCommand(email="not-an-email", full_name="Alice")
    with pytest.raises(InvalidEmailError):
        await use_case.execute(cmd)
```

---

## 10. Real-World Case Study / Production Incident

A healthcare software company built its initial patient billing portal directly using FastAPI and SQLAlchemy models inside route handlers. Two years later, the enterprise required:
1. An HL7/FHIR hospital integration consuming messages from an Apache Kafka broker.
2. An offline emergency CLI tool for paramedics.

Because all the billing rules, insurance copay calculations, and drug interaction validations were embedded in FastAPI endpoint decorators, the company could not run the logic from Kafka or the CLI. Developers spent 9 months copy-pasting endpoints into background workers, resulting in three major production incidents where copays differed depending on whether a patient booked via the website or an HL7 hospital feed.

**The Solution**:
The engineering team migrated to Clean Hexagonal Architecture:
- Extracted copay and insurance rules into pure domain entities and use cases.
- Implemented three driving adapters:
  1. `FastAPI Web Adapter` (for the browser portal)
  2. `Kafka Consumer Adapter` (for HL7 messages)
  3. `Click CLI Adapter` (for the offline paramedic tool)
- All three adapters invoked the identical `ProcessBillingUseCase`, guaranteeing 100% consistent financial calculations across all channels.

---

## 11. Technical Interview Questions

<details>
<summary>1. What is the difference between a Driving (Primary) Adapter and a Driven (Secondary) Adapter?</summary>
A Driving (Primary) Adapter initiates actions on the application by translating external requests into calls to inbound use-case ports (e.g., FastAPI HTTP routers, CLI commands, Kafka message consumers). A Driven (Secondary) Adapter is invoked by the application to interact with external systems by implementing outbound ports (e.g., PostgreSQL database repositories, Redis caches, SendGrid email services).
</details>

<details>
<summary>2. Why does Clean Architecture forbid importing Pydantic or SQLAlchemy in the Domain Core?</summary>
The Domain Core represents timeless business rules that should outlive any specific framework or library. If the domain imports SQLAlchemy or Pydantic, changes to those third-party libraries (e.g., upgrading from Pydantic v1 to v2 or altering ORM mechanics) can break core business logic. Furthermore, it forces tests of pure domain rules to depend on external libraries.
</details>

<details>
<summary>3. What is the "Dependency Inversion Principle" in the context of Hexagonal Architecture?</summary>
High-level modules (the domain and application use cases) should not depend on low-level modules (database adapters, HTTP routers). Both should depend on abstractions (ports/protocols). The abstraction is owned by the high-level application layer, and the low-level infrastructure layer implements it, inverting the traditional dependency direction.
</details>

<details>
<summary>4. What is a "Command" object in a Clean Architecture use case?</summary>
A Command is an immutable Data Transfer Object (typically a `@dataclass(frozen=True)`) containing the primitive arguments required to execute an application use case. It decouples the use case from transport-specific objects like FastAPI `Request` instances or Pydantic HTTP schemas.
</details>

<details>
<summary>5. How does Python's `typing.Protocol` enable the definition of Outbound Ports?</summary>
`typing.Protocol` provides structural subtyping (compile-time duck typing). An outbound port defined as a `Protocol` specifies the required methods and signatures without forcing adapters to inherit from an abstract base class, allowing loose coupling and simple mock generation during unit tests.
</details>

<details>
<summary>6. Where should cross-cutting concerns like logging and distributed tracing be placed in Hexagonal Architecture?</summary>
Cross-cutting concerns can be implemented as Decorators or Middlewares wrapping the Ports or Adapters. For example, a `LoggingUserRepositoryDecorator` can implement `UserRepositoryPort`, log the execution time of queries, and delegate the actual work to `PostgresUserRepository`.
</details>

<details>
<summary>7. What is a "Composition Root" in Hexagonal Architecture?</summary>
The Composition Root is the centralized location where all dependencies are wired together at application startup. In FastAPI, the composition root is typically configured via module factories or FastAPI's dependency injection container (`app/infrastructure/dependencies.py`), which instantiates concrete adapters and passes them into use cases.
</details>

<details>
<summary>8. How do you handle database transactions across multiple outbound ports in Hexagonal Architecture?</summary>
By introducing a `UnitOfWorkPort` outbound abstraction. The application use case enters the `async with uow:` context, executes operations against ports attached to that unit of work, and explicitly calls `await uow.commit()`.
</details>

---

## 12. Practical Exercises & Solutions

<details>
<summary>Exercise 1 (Easy): Create a Pure Domain Value Object</summary>

**Task**: Create an immutable value object `Money` using `@dataclass(frozen=True)` that stores a `Decimal` amount and a 3-letter currency code `str`. Enforce that amount cannot be negative and currency must be uppercase.

**Solution**:
```python
from dataclasses import dataclass
from decimal import Decimal

@dataclass(frozen=True)
class Money:
    amount: Decimal
    currency: str

    def __post_init__(self):
        if self.amount < Decimal("0.00"):
            raise ValueError("Amount cannot be negative")
        if len(self.currency) != 3 or not self.currency.isupper():
            raise ValueError("Currency must be a 3-letter uppercase ISO code")

    def add(self, other: "Money") -> "Money":
        if self.currency != other.currency:
            raise ValueError(f"Cannot add different currencies: {self.currency} and {other.currency}")
        return Money(amount=self.amount + other.amount, currency=self.currency)
```
</details>

<details>
<summary>Exercise 2 (Easy): Define an Outbound Storage Port</summary>

**Task**: Define an outbound port `FileStoragePort` using `typing.Protocol` with methods `async def upload(filename: str, data: bytes) -> str` and `async def delete(filename: str) -> bool`.

**Solution**:
```python
from typing import Protocol

class FileStoragePort(Protocol):
    async def upload(self, filename: str, data: bytes) -> str:
        """Uploads file bytes and returns the public access URL."""
        ...

    async def delete(self, filename: str) -> bool:
        """Deletes file and returns True if successful."""
        ...
```
</details>

<details>
<summary>Exercise 3 (Medium): In-Memory Adapter for FileStoragePort</summary>

**Task**: Implement an in-memory adapter `InMemoryFileStorageAdapter` satisfying `FileStoragePort`.

**Solution**:
```python
class InMemoryFileStorageAdapter:
    def __init__(self):
        self.files: dict[str, bytes] = {}

    async def upload(self, filename: str, data: bytes) -> str:
        self.files[filename] = data
        return f"https://mock-cdn.local/{filename}"

    async def delete(self, filename: str) -> bool:
        if filename in self.files:
            del self.files[filename]
            return True
        return False
```
</details>

<details>
<summary>Exercise 4 (Medium): Use Case with Inbound Command</summary>

**Task**: Create a `UploadAvatarUseCase` that accepts an `UploadAvatarCommand(user_id: str, filename: str, data: bytes)`, checks that the file size is under 2MB (2,097,152 bytes), and uploads it via `FileStoragePort`.

**Solution**:
```python
from dataclasses import dataclass

@dataclass(frozen=True)
class UploadAvatarCommand:
    user_id: str
    filename: str
    data: bytes

class UploadAvatarUseCase:
    MAX_SIZE = 2 * 1024 * 1024  # 2MB

    def __init__(self, storage: FileStoragePort):
        self.storage = storage

    async def execute(self, cmd: UploadAvatarCommand) -> str:
        if len(cmd.data) > self.MAX_SIZE:
            raise ValueError("Avatar file size exceeds 2MB limit")
        file_path = f"avatars/{cmd.user_id}_{cmd.filename}"
        return await self.storage.upload(file_path, cmd.data)
```
</details>

<details>
<summary>Exercise 5 (Hard): FastAPI Driving Adapter with Dependency Override</summary>

**Task**: Implement a FastAPI router endpoint for `POST /users/{user_id}/avatar` that accepts a multipart file, constructs `UploadAvatarCommand`, invokes `UploadAvatarUseCase`, and returns the uploaded URL. Wire the dependency cleanly.

**Solution**:
```python
from fastapi import APIRouter, Depends, UploadFile, File, HTTPException, status

router = APIRouter()

# Concrete composition
_storage = InMemoryFileStorageAdapter()

def get_upload_avatar_use_case() -> UploadAvatarUseCase:
    return UploadAvatarUseCase(storage=_storage)

@router.post("/users/{user_id}/avatar")
async def upload_avatar_endpoint(
    user_id: str,
    file: UploadFile = File(...),
    use_case: UploadAvatarUseCase = Depends(get_upload_avatar_use_case),
):
    contents = await file.read()
    cmd = UploadAvatarCommand(user_id=user_id, filename=file.filename or "avatar.png", data=contents)
    try:
        url = await use_case.execute(cmd)
        return {"avatar_url": url}
    except ValueError as exc:
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail=str(exc))
```
</details>

<details>
<summary>Exercise 6 (Hard): Decorator Pattern for Outbound Port (Audit Logging)</summary>

**Task**: Implement a decorator `AuditedFileStorageAdapter` that implements `FileStoragePort`, logs every upload and delete operation with execution duration, and delegates the actual call to any underlying `FileStoragePort` implementation.

**Solution**:
```python
import time
import logging

logger = logging.getLogger(__name__)

class AuditedFileStorageAdapter:
    def __init__(self, inner: FileStoragePort):
        self._inner = inner

    async def upload(self, filename: str, data: bytes) -> str:
        start = time.perf_counter()
        logger.info("AUDIT: Starting upload for %s (%d bytes)", filename, len(data))
        try:
            url = await self._inner.upload(filename, data)
            elapsed = (time.perf_counter() - start) * 1000
            logger.info("AUDIT: Upload completed for %s in %.2fms. URL: %s", filename, elapsed, url)
            return url
        except Exception as exc:
            logger.error("AUDIT: Upload failed for %s: %s", filename, exc)
            raise

    async def delete(self, filename: str) -> bool:
        start = time.perf_counter()
        logger.info("AUDIT: Deleting %s", filename)
        result = await self._inner.delete(filename)
        elapsed = (time.perf_counter() - start) * 1000
        logger.info("AUDIT: Deletion for %s finished in %.2fms (success=%s)", filename, elapsed, result)
        return result
```
</details>
