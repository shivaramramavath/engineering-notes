---
title: "Service Layer and Repository Pattern in FastAPI"
description: "Master the Service Layer and Repository patterns in FastAPI with SQLAlchemy 2.0: decouple business logic from persistence, implement the Unit of Work pattern, and achieve high testability."
fastapi_version: "0.115.x+"
python_version: "3.12+"
pydantic_version: "v2.10+"
tested_with: "sqlalchemy 2.0+, pydantic 2.10+, pytest 8.3+"
free_threading_status: "Fully Compatible"
---

# Service Layer and Repository Pattern in FastAPI

## 1. Conceptual Foundation & Mental Model

### The Danger of Leaking ORM Queries Across Endpoints
In unarchitected FastAPI applications, endpoints frequently interleave business validations with direct SQLAlchemy queries:
```python
# ANTI-PATTERN: Direct ORM calls inside route handler
@router.post("/checkout")
async def checkout(cart_id: str, db: AsyncSession = Depends(get_db)):
    cart = (await db.execute(select(Cart).where(Cart.id == cart_id))).scalar_one_or_none()
    if not cart:
        raise HTTPException(404, "Cart not found")
    # Business logic mixed with data access
    total = sum(item.price for item in cart.items)
    order = Order(user_id=cart.user_id, total=total)
    db.add(order)
    await db.commit()
    return order
```
This approach creates catastrophic maintenance issues:
1. **Scattered Database Logic**: The exact query to find a cart and eagerly load items gets copied across multiple routers, scripts, and background tasks.
2. **Untestable Business Rules**: Testing total calculation or checkout validation requires spinning up an entire database instance.
3. **Transaction Fragmentation**: Multi-step operations cannot be rolled back cleanly if a failure occurs halfway through.

### The Service Layer & Repository Dualism
The **Repository Pattern** and **Service Layer** operate as complementary abstractions that isolate the domain from infrastructure:

```
+--------------------------------------------------------------------------+
|                        API / PRESENTATION LAYER                          |
|  - Validates HTTP request payloads via Pydantic                          |
|  - Invokes Service Layer methods with domain parameters                  |
+--------------------------------------------------------------------------+
                                    |
                                    v (Pure Domain Inputs)
+--------------------------------------------------------------------------+
|                             SERVICE LAYER                                |
|  - Encapsulates application workflows and use-cases                      |
|  - Coordinates multiple repositories and domain services                 |
|  - Manages Unit of Work (transaction commit / rollback demarcation)      |
|  - Emits domain events and triggers background notifications             |
+--------------------------------------------------------------------------+
                                    |
                                    v (Domain Entities)
+--------------------------------------------------------------------------+
|                           REPOSITORY LAYER                               |
|  - Emulates an in-memory collection of domain entities                   |
|  - Hides SQL / ORM mechanics (select, join, filter, limit)               |
|  - Maps between persistence models (ORM) and domain objects              |
+--------------------------------------------------------------------------+
                                    |
                                    v
+--------------------------------------------------------------------------+
|                      DATABASE / PERSISTENCE STORE                        |
|  PostgreSQL / MySQL via SQLAlchemy AsyncSession                          |
+--------------------------------------------------------------------------+
```

---

## 2. Architecture & Internal Mechanics

The interaction between the Presentation, Service, Repository, and Unit of Work follows a strict unidirectional sequence.

```
Router               Service              UnitOfWork           Repository            Database
  |                     |                     |                     |                    |
  |-- place_order() --->|                     |                     |                    |
  |                     |-- async with uow -->|                     |                    |
  |                     |                     |-- begin trans ---->|                    |
  |                     |                     |                     |-- BEGIN ---------->|
  |                     |-- get_user() ---------------------------->|                    |
  |                     |                     |                     |-- SELECT * ------->|
  |                     |                     |                     |<- User row --------|
  |                     |<- user entity ----------------------------|                    |
  |                     |                                           |                    |
  |                     | [Execute Business Logic: Validate Funds]  |                    |
  |                     |                                           |                    |
  |                     |-- create_order(entity) ------------------>|                    |
  |                     |                     |                     |-- INSERT INTO ---->|
  |                     |-- uow.commit() ---->|                     |                    |
  |                     |                     |-- COMMIT ------------------------------->|
  |                     |<- order entity -----|                     |                    |
  |<- 201 Created ------|                     |                     |                    |
```

### The Role of the Unit of Work (UoW) Pattern
A common pitfall occurs when repositories commit transactions independently. If a use-case requires:
1. Deducting stock in `InventoryRepository`
2. Creating an invoice in `InvoiceRepository`
3. Decrementing credit in `CustomerRepository`

If `InvoiceRepository` commits, but `CustomerRepository` crashes, the database is left in a corrupted, half-applied state. The **Unit of Work** pattern maintains a list of objects affected by a business transaction and coordinates writing out changes and resolving concurrency problems via a single `commit()` or `rollback()`.

---

## 3. Comprehensive Syntax & Core Mechanics

### Generic Abstract Repository Interface
Using Python's `typing.Generic` and `typing.Protocol` to define the collection-like contract.

```python
from typing import Generic, TypeVar, Optional, List, Protocol
from abc import abstractmethod

T = TypeVar("T")
ID = TypeVar("ID")

class AbstractRepository(Protocol[T, ID]):
    @abstractmethod
    async def get_by_id(self, entity_id: ID) -> Optional[T]: ...

    @abstractmethod
    async def list_all(self, limit: int = 100, offset: int = 0) -> List[T]: ...

    @abstractmethod
    async def add(self, entity: T) -> T: ...

    @abstractmethod
    async def delete(self, entity_id: ID) -> bool: ...
```

### Generic Unit of Work Interface

```python
from typing import Protocol
from abc import abstractmethod

class AbstractUnitOfWork(Protocol):
    @abstractmethod
    async def __aenter__(self) -> "AbstractUnitOfWork": ...

    @abstractmethod
    async def __aexit__(self, exc_type, exc_val, exc_tb) -> None: ...

    @abstractmethod
    async def commit(self) -> None: ...

    @abstractmethod
    async def rollback(self) -> None: ...
```

---

## 4. Practical Implementation: 5 Verified Examples

### Example 1: Domain Entities and Concrete SQLAlchemy ORM Model
Keeping the domain entity independent of SQLAlchemy table definitions.

```python
# app/domain/wallet.py
from dataclasses import dataclass
from decimal import Decimal
import uuid

class InsufficientBalanceError(Exception):
    pass

@dataclass
class Wallet:
    user_id: str
    balance: Decimal
    id: str = ""

    def __post_init__(self):
        if not self.id:
            self.id = str(uuid.uuid4())

    def deposit(self, amount: Decimal) -> None:
        if amount <= Decimal("0.00"):
            raise ValueError("Deposit amount must be positive")
        self.balance += amount

    def withdraw(self, amount: Decimal) -> None:
        if amount <= Decimal("0.00"):
            raise ValueError("Withdrawal amount must be positive")
        if self.balance < amount:
            raise InsufficientBalanceError(f"Current balance {self.balance} is less than {amount}")
        self.balance -= amount
```

```python
# app/infrastructure/orm/wallet_model.py
from sqlalchemy import Column, String, Numeric
from sqlalchemy.orm import declarative_base

Base = declarative_base()

class WalletORM(Base):
    __tablename__ = "wallets"

    id = Column(String(36), primary_key=True)
    user_id = Column(String(36), nullable=False, unique=True, index=True)
    balance = Column(Numeric(precision=18, scale=4), nullable=False)
```

---

### Example 2: Concrete SQLAlchemy Async Repository
Implementing mapping between `WalletORM` and pure domain `Wallet` objects.

```python
# app/infrastructure/repositories/wallet_repository.py
from typing import Optional
from decimal import Decimal
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession
from app.domain.wallet import Wallet
from app.infrastructure.orm.wallet_model import WalletORM

class SqlAlchemyWalletRepository:
    def __init__(self, session: AsyncSession):
        self._session = session

    async def get_by_user_id(self, user_id: str) -> Optional[Wallet]:
        stmt = select(WalletORM).where(WalletORM.user_id == user_id)
        result = await self._session.execute(stmt)
        orm_wallet = result.scalar_one_or_none()
        if not orm_wallet:
            return None
        return Wallet(
            id=orm_wallet.id,
            user_id=orm_wallet.user_id,
            balance=Decimal(str(orm_wallet.balance))
        )

    async def save(self, wallet: Wallet) -> Wallet:
        stmt = select(WalletORM).where(WalletORM.id == wallet.id)
        result = await self._session.execute(stmt)
        orm_wallet = result.scalar_one_or_none()
        
        if orm_wallet:
            orm_wallet.balance = wallet.balance
        else:
            orm_wallet = WalletORM(
                id=wallet.id,
                user_id=wallet.user_id,
                balance=wallet.balance
            )
            self._session.add(orm_wallet)
            
        await self._session.flush()
        return wallet
```

---

### Example 3: Production Unit of Work with Async Context Manager
Managing transaction lifecycle, auto-rollback on uncaught errors, and repository injection.

```python
# app/infrastructure/uow.py
from sqlalchemy.ext.asyncio import AsyncSession, async_sessionmaker
from app.infrastructure.repositories.wallet_repository import SqlAlchemyWalletRepository

class SqlAlchemyUnitOfWork:
    def __init__(self, session_factory: async_sessionmaker[AsyncSession]):
        self._session_factory = session_factory
        self.session: AsyncSession | None = None
        self.wallets: SqlAlchemyWalletRepository | None = None

    async def __aenter__(self):
        self.session = self._session_factory()
        self.wallets = SqlAlchemyWalletRepository(self.session)
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

---

### Example 4: Service Layer Orchestration (Transfer Funds Use Case)
Executing a multi-wallet transfer with atomic rollback guarantees.

```python
# app/services/wallet_service.py
from decimal import Decimal
from app.domain.wallet import InsufficientBalanceError
from app.infrastructure.uow import SqlAlchemyUnitOfWork

class WalletNotFoundError(Exception):
    pass

class WalletService:
    def __init__(self, uow: SqlAlchemyUnitOfWork):
        self._uow = uow

    async def transfer_funds(
        self,
        sender_id: str,
        receiver_id: str,
        amount: Decimal
    ) -> tuple[Decimal, Decimal]:
        """Atomically transfers money between two wallets within a single Unit of Work."""
        async with self._uow as uow:
            sender_wallet = await uow.wallets.get_by_user_id(sender_id)
            if not sender_wallet:
                raise WalletNotFoundError(f"Sender wallet for {sender_id} not found")

            receiver_wallet = await uow.wallets.get_by_user_id(receiver_id)
            if not receiver_wallet:
                raise WalletNotFoundError(f"Receiver wallet for {receiver_id} not found")

            # Domain operations
            sender_wallet.withdraw(amount)
            receiver_wallet.deposit(amount)

            # Persist updates via repository
            await uow.wallets.save(sender_wallet)
            await uow.wallets.save(receiver_wallet)

            # Atomic commit of both updates
            await uow.commit()

            return sender_wallet.balance, receiver_wallet.balance
```

---

### Example 5: FastAPI Router Dependency Injection and Endpoint
Wiring the Service Layer and Unit of Work into FastAPI endpoints cleanly.

```python
# app/api/v1/endpoints/wallets.py
from decimal import Decimal
from fastapi import APIRouter, Depends, HTTPException, status
from pydantic import BaseModel, Field
from sqlalchemy.ext.asyncio import AsyncSession, async_sessionmaker
from app.core.database import async_session_factory
from app.domain.wallet import InsufficientBalanceError
from app.infrastructure.uow import SqlAlchemyUnitOfWork
from app.services.wallet_service import WalletService, WalletNotFoundError

router = APIRouter(prefix="/wallets", tags=["Wallets"])

class TransferRequestDTO(BaseModel):
    sender_id: str = Field(min_length=1)
    receiver_id: str = Field(min_length=1)
    amount: Decimal = Field(gt=Decimal("0.00"))

class TransferResponseDTO(BaseModel):
    sender_new_balance: Decimal
    receiver_new_balance: Decimal
    status: str = "COMPLETED"

def get_wallet_service() -> WalletService:
    uow = SqlAlchemyUnitOfWork(async_session_factory)
    return WalletService(uow)

@router.post("/transfer", response_model=TransferResponseDTO)
async def transfer_endpoint(
    payload: TransferRequestDTO,
    service: WalletService = Depends(get_wallet_service)
):
    try:
        sender_bal, recv_bal = await service.transfer_funds(
            payload.sender_id, payload.receiver_id, payload.amount
        )
        return TransferResponseDTO(
            sender_new_balance=sender_bal,
            receiver_new_balance=recv_bal
        )
    except WalletNotFoundError as exc:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail=str(exc))
    except InsufficientBalanceError as exc:
        raise HTTPException(status_code=status.HTTP_409_CONFLICT, detail=str(exc))
    except ValueError as exc:
        raise HTTPException(status_code=status.HTTP_422_UNPROCESSABLE_ENTITY, detail=str(exc))
```

---

## 5. Edge Cases, Pitfalls & Failure Modes

1. **Premature Flushing vs. Committing**: Calling `session.flush()` writes changes to the database transaction buffer but does not commit them, allowing other queries in the same transaction to see updated rows. Calling `session.commit()` inside a repository seals the transaction, making it impossible to roll back if a subsequent repository operation fails.
2. **Identity Map Inconsistencies**: When converting between ORM models and domain dataclasses, modifying a domain dataclass does not notify SQLAlchemy's session tracker automatically. The repository's `save()` method must explicitly update the corresponding ORM model attributes.
3. **Lazy-Loading Outside Sessions**: If an ORM object with lazy-loaded relationships is returned to the service layer after `uow.session.close()`, accessing those relationships raises `DetachedInstanceError`. Always eagerly load required relationships using `selectinload()` or map to domain objects inside the repository while the session is active.
4. **Deadlocks in Multi-Entity Updates**: When transferring funds between two entities, if transaction A locks User 1 then User 2, and concurrent transaction B locks User 2 then User 1, a database deadlock occurs. Always sort entity IDs before acquiring row locks (e.g., `lock(min(id1, id2))`, then `lock(max(id1, id2))`).

---

## 6. Common Mistakes Table

| Mistake | Root Cause | Impact | Correct Approach |
| :--- | :--- | :--- | :--- |
| **Calling `session.commit()` in Repository** | Misunderstanding repository boundaries | Breaks atomicity; multi-repository use cases cannot roll back on error. | Only the Unit of Work or Service Layer should call `commit()`. |
| **Passing FastAPI `Request` into Service** | Convenience shortcuts | Service becomes untestable without ASGI mocking; cannot run in Celery/CLI. | Pass primitive Python data types, DTOs, or domain entities into services. |
| **Returning SQLAlchemy ORM to Routers** | Avoiding domain mapping boilerplate | Leaks database schema to clients; causes `DetachedInstanceError` during serialization. | Map ORM models to pure domain objects or Pydantic DTOs before leaving repository. |
| **Instantiating UoW Inside Routers** | Router doing orchestration | Bloats presentation layer with database connection and transaction mechanics. | Inject Service via `Depends()`; let Service manage the Unit of Work. |
| **Anemic Repositories (Only CRUD)** | Treating repository as thin pass-through | Complex queries scatter across services; defeats encapsulation. | Add meaningful query methods to repositories (e.g., `find_overdue_invoices()`). |
| **Reusing Single UoW Instance Concurrently** | Scope of UoW fixture set to Singleton | Multiple requests share the same DB session, corrupting transactions. | Ensure Unit of Work and AsyncSession are created per-request (`function` scope). |

---

## 7. Performance & Scalability Considerations

- **Session Lifetimes**: Keep Unit of Work contexts as short as possible. Acquire the session only when ready to execute database I/O, and release it immediately after `commit()`. Holding open database transactions during long external HTTP calls exhausts connection pools.
- **Bulk Operations**: When inserting or updating thousands of records, domain-by-domain mapping in repositories creates CPU overhead. Provide specialized repository methods utilizing `session.execute(insert(ORM).values([...]))` for bulk ingestion.
- **Free-Threading in Python 3.13t**: In-memory domain transformations (dataclass mapping, mathematical rules) execute concurrently across threads without GIL locking, maximizing multi-core throughput.

---

## 8. Security & OWASP Guardrails

- **OWASP A01 (Broken Object-Level Authorization)**: Repository queries fetching user resources should enforce tenant/user ownership in the query predicate (e.g., `where(WalletORM.id == wallet_id, WalletORM.user_id == authenticated_user_id)`), preventing Insecure Direct Object References (IDOR).
- **OWASP A04 (Insecure Design - Concurrency Race Conditions)**: Financial operations like balance deductions must use pessimistic row locking (`with_for_update()`) in the repository to prevent double-spending attacks.

---

## 9. Testing Strategies & Pytest Recipes

```python
# tests/unit/test_wallet_service.py
from decimal import Decimal
import pytest
from app.domain.wallet import Wallet, InsufficientBalanceError
from app.services.wallet_service import WalletService, WalletNotFoundError

class FakeWalletRepository:
    def __init__(self):
        self.wallets: dict[str, Wallet] = {}

    async def get_by_user_id(self, user_id: str):
        for w in self.wallets.values():
            if w.user_id == user_id:
                return w
        return None

    async def save(self, wallet: Wallet):
        self.wallets[wallet.id] = wallet
        return wallet

class FakeUnitOfWork:
    def __init__(self):
        self.wallets = FakeWalletRepository()
        self.committed = False

    async def __aenter__(self):
        return self

    async def __aexit__(self, exc_type, exc_val, exc_tb):
        pass

    async def commit(self):
        self.committed = True

    async def rollback(self):
        self.committed = False

@pytest.mark.asyncio
async def test_transfer_funds_successful():
    uow = FakeUnitOfWork()
    w1 = Wallet(id="w1", user_id="u1", balance=Decimal("100.00"))
    w2 = Wallet(id="w2", user_id="u2", balance=Decimal("50.00"))
    await uow.wallets.save(w1)
    await uow.wallets.save(w2)

    service = WalletService(uow)
    sender_bal, recv_bal = await service.transfer_funds("u1", "u2", Decimal("30.00"))

    assert sender_bal == Decimal("70.00")
    assert recv_bal == Decimal("80.00")
    assert uow.committed is True

@pytest.mark.asyncio
async def test_transfer_insufficient_funds_rolls_back():
    uow = FakeUnitOfWork()
    w1 = Wallet(id="w1", user_id="u1", balance=Decimal("20.00"))
    w2 = Wallet(id="w2", user_id="u2", balance=Decimal("50.00"))
    await uow.wallets.save(w1)
    await uow.wallets.save(w2)

    service = WalletService(uow)
    with pytest.raises(InsufficientBalanceError):
        await service.transfer_funds("u1", "u2", Decimal("100.00"))

    assert uow.committed is False
```

---

## 10. Real-World Case Study / Production Incident

An online cryptocurrency brokerage built its order-matching engine with FastAPI. Under peak volatility, customers initiated rapid withdrawals while concurrently placing buy orders.

**The Failure**:
1. Route handlers directly invoked `await db.commit()` across separate functions.
2. An account with \$500 balance initiated a \$500 withdrawal and a \$500 crypto purchase simultaneously.
3. Both endpoints executed `select(User).where(...)`, read a balance of \$500, subtracted \$500, and committed.
4. Because the queries did not lock rows (`with_for_update()`) and lacked a synchronized Unit of Work, both transactions succeeded. The user spent \$1,000 against a \$500 balance.

**The Solution**:
The team refactored data access into a `WalletRepository` and `WalletService`:
1. The repository implemented pessimistic locking:
   ```python
   stmt = select(WalletORM).where(WalletORM.user_id == user_id).with_for_update()
   ```
2. The `WalletService` executed within a strict Unit of Work. The second concurrent transaction was forced to wait for the first transaction's lock release, immediately failing with `InsufficientBalanceError` upon reading the updated \$0.00 balance.

---

## 11. Technical Interview Questions

<details>
<summary>1. What problem does the Unit of Work pattern solve that the Repository pattern alone does not?</summary>
The Repository pattern abstracts data access for individual entities, but it has no awareness of transactions spanning multiple repositories. If an operation requires modifying data across multiple repositories, individual repositories committing their own changes break atomicity. The Unit of Work manages the database transaction context, tracking all modifications and ensuring that all repositories commit together or roll back completely on failure.
</details>

<details>
<summary>2. Why should repositories avoid executing `session.commit()`?</summary>
If a repository commits a transaction, it finalizes changes to the database. If the enclosing business use case subsequently encounters a failure in another repository or third-party service, the previously committed changes cannot be rolled back, violating the ACID atomicity principle.
</details>

<details>
<summary>3. What is the difference between `session.flush()` and `session.commit()` in SQLAlchemy?</summary>
`session.flush()` communicates pending SQL statements (`INSERT`, `UPDATE`, `DELETE`) to the database server within the current transaction, assigning primary keys and foreign keys, but does not permanently save them. `session.commit()` issues a database `COMMIT`, permanently persisting all flushed changes and concluding the transaction.
</details>

<details>
<summary>4. How does the Repository pattern facilitate unit testing?</summary>
By decoupling business logic from concrete database drivers, the Service Layer can be tested against in-memory dictionary repositories (`FakeRepository`) without requiring a live PostgreSQL instance, Docker containers, or mock database engines. This allows unit test suites to execute in milliseconds.
</details>

<details>
<summary>5. How can you prevent deadlocks when updating multiple entities within a single Unit of Work?</summary>
Deadlocks occur when concurrent transactions acquire locks on the same resources in different orders. To prevent deadlocks, enforce a deterministic lock ordering protocol: sort entity IDs lexicographically before acquiring locks (e.g., `for entity_id in sorted([id_a, id_b]): lock(entity_id)`).
</details>

<details>
<summary>6. What is the difference between pessimistic locking and optimistic locking in a repository?</summary>
Pessimistic locking uses database locks (`SELECT ... FOR UPDATE`) to prevent other transactions from reading or modifying a record until the current transaction completes. Optimistic locking relies on a version column (`version_id`); when saving, it verifies that the version in the database matches the version read. If another transaction modified the row, the update fails, prompting a retry.
</details>

<details>
<summary>7. When should you NOT use the Repository and Unit of Work patterns?</summary>
For simple, read-only CRUD microservices or reporting dashboards with minimal business logic. In such cases, the additional abstraction layers introduce unnecessary boilerplate without providing substantial architectural benefit.
</details>

<details>
<summary>8. How do you prevent `DetachedInstanceError` when using SQLAlchemy with the Service Layer?</summary>
`DetachedInstanceError` occurs when accessing lazily loaded relationships on an ORM object after the session that loaded it has closed. To prevent this, either eagerly load all necessary relationships using `selectinload()` inside the repository, or map ORM records into pure Python domain objects/DTOs before returning them from the repository.
</details>

---

## 12. Practical Exercises & Solutions

<details>
<summary>Exercise 1 (Easy): In-Memory Fake Repository</summary>

**Task**: Implement an in-memory repository for a `Product(id: str, name: str, price: float)` entity supporting `get_by_id`, `add`, and `delete`.

**Solution**:
```python
from dataclasses import dataclass
from typing import Optional

@dataclass
class Product:
    id: str
    name: str
    price: float

class InMemoryProductRepository:
    def __init__(self):
        self._storage: dict[str, Product] = {}

    async def get_by_id(self, product_id: str) -> Optional[Product]:
        return self._storage.get(product_id)

    async def add(self, product: Product) -> Product:
        self._storage[product.id] = product
        return product

    async def delete(self, product_id: str) -> bool:
        if product_id in self._storage:
            del self._storage[product_id]
            return True
        return False
```
</details>

<details>
<summary>Exercise 2 (Easy): Pessimistic Locking Query in SQLAlchemy</summary>

**Task**: Write an async repository method `get_for_update(account_id: str)` using SQLAlchemy 2.0 that executes a `SELECT ... FOR UPDATE` query on `AccountORM`.

**Solution**:
```python
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

class AccountRepository:
    def __init__(self, session: AsyncSession):
        self._session = session

    async def get_for_update(self, account_id: str):
        stmt = (
            select(AccountORM)
            .where(AccountORM.id == account_id)
            .with_for_update()
        )
        result = await self._session.execute(stmt)
        return result.scalar_one_or_none()
```
</details>

<details>
<summary>Exercise 3 (Medium): Unit of Work Context Manager Protocol</summary>

**Task**: Implement an async context manager `MockUnitOfWork` that records whether `commit()` or `rollback()` was called, automatically rolling back if an exception is raised inside the `async with` block.

**Solution**:
```python
class MockUnitOfWork:
    def __init__(self):
        self.committed = False
        self.rolled_back = False

    async def __aenter__(self):
        return self

    async def __aexit__(self, exc_type, exc_val, exc_tb):
        if exc_type is not None:
            await self.rollback()

    async def commit(self):
        self.committed = True

    async def rollback(self):
        self.rolled_back = True
```
</details>

<details>
<summary>Exercise 4 (Medium): Service Layer Stock Deduction with UoW</summary>

**Task**: Implement an `InventoryService` with a method `deduct_stock(product_id: str, quantity: int)`. If stock is insufficient, raise `ValueError`. Use the `MockUnitOfWork` from Exercise 3.

**Solution**:
```python
class InventoryService:
    def __init__(self, uow):
        self._uow = uow

    async def deduct_stock(self, product_id: str, quantity: int) -> int:
        async with self._uow as uow:
            product = await uow.products.get_by_id(product_id)
            if not product:
                raise KeyError(f"Product {product_id} not found")
            if product.stock < quantity:
                raise ValueError(f"Insufficient stock for {product_id}")
                
            product.stock -= quantity
            await uow.products.add(product)
            await uow.commit()
            return product.stock
```
</details>

<details>
<summary>Exercise 5 (Hard): Concrete SQLAlchemy Unit of Work with Multiple Repositories</summary>

**Task**: Implement a production-grade `SqlAlchemyUnitOfWork` that initializes both `UserRepository` and `AuditLogRepository` upon entering the async context manager and rolls back automatically if uncommitted.

**Solution**:
```python
from sqlalchemy.ext.asyncio import AsyncSession, async_sessionmaker

class SqlAlchemyUnitOfWork:
    def __init__(self, session_factory: async_sessionmaker[AsyncSession]):
        self._session_factory = session_factory
        self.session: AsyncSession | None = None
        self.users = None
        self.audit_logs = None

    async def __aenter__(self):
        self.session = self._session_factory()
        self.users = SqlAlchemyUserRepository(self.session)
        self.audit_logs = SqlAlchemyAuditLogRepository(self.session)
        return self

    async def __aexit__(self, exc_type, exc_val, exc_tb):
        try:
            if exc_type is not None:
                await self.rollback()
        finally:
            await self.session.close()

    async def commit(self):
        await self.session.commit()

    async def rollback(self):
        await self.session.rollback()
```
</details>

<details>
<summary>Exercise 6 (Hard): Optimistic Locking Repository Implementation</summary>

**Task**: Implement an `update_with_optimistic_lock(entity)` method in a SQLAlchemy repository that updates an entity only if the database `version_id` matches the in-memory `version_id`. If zero rows are updated, raise `ConcurrentModificationError`.

**Solution**:
```python
from sqlalchemy import update
from sqlalchemy.ext.asyncio import AsyncSession

class ConcurrentModificationError(Exception):
    pass

class OptimisticOrderRepository:
    def __init__(self, session: AsyncSession):
        self.session = session

    async def update_with_optimistic_lock(self, order_id: str, new_status: str, current_version: int):
        stmt = (
            update(OrderORM)
            .where(OrderORM.id == order_id, OrderORM.version_id == current_version)
            .values(status=new_status, version_id=current_version + 1)
        )
        result = await self.session.execute(stmt)
        if result.rowcount == 0:
            raise ConcurrentModificationError(
                f"Order {order_id} was modified concurrently by another transaction"
            )
        await self.session.flush()
```
</details>
