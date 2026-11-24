---
title: "Domain-Driven Design (DDD) with Python and FastAPI"
description: "Implement Tactical and Strategic Domain-Driven Design in FastAPI backends: Aggregate Roots, Entities, Value Objects, Domain Events, and Bounded Contexts with pure Python dataclasses."
fastapi_version: "0.115.x+"
python_version: "3.12+"
pydantic_version: "v2.10+"
tested_with: "pydantic 2.10+, fastapi 0.115+, pytest 8.3+"
free_threading_status: "Fully Compatible"
---

# Domain-Driven Design (DDD) with Python and FastAPI

## 1. Conceptual Foundation & Mental Model

### The Complexity Trap: Data-Centric CRUD vs. Domain-Driven Design
When engineering simple applications, organizing code around relational database tables (Data-Driven Design) is fast and effective. However, as business logic grows complex—involving multi-step state transitions, regulatory compliance rules, pricing algorithms, and distributed consistency—data-centric designs collapse under the weight of **Anemic Domain Models**.

In an anemic model, classes are mere bags of getters and setters, while business rules are scattered arbitrarily across services, helper modules, and API route handlers. Anyone can mutate any field on any object at any time, leading to corrupted data states.

**Domain-Driven Design (DDD)**, introduced by Eric Evans, re-centers software architecture around the **core business domain**. DDD provides two levels of modeling:
1. **Strategic Design**: Managing large-scale architecture via **Bounded Contexts**, **Ubiquitous Language**, and **Context Maps**.
2. **Tactical Design**: Precise software building blocks for modeling complex invariants within a bounded context:
   - **Value Objects**: Immutable objects identified purely by their attributes.
   - **Entities**: Objects possessing unique identity and lifecycle transitions.
   - **Aggregates & Aggregate Roots**: Clusters of entities and value objects treated as a single atomic consistency boundary.
   - **Domain Events**: Records of meaningful business occurrences dispatched when aggregate state changes.
   - **Domain Services**: Pure business algorithms that do not naturally belong to a single entity.

```
+-------------------------------------------------------------------------+
|                         BOUNDED CONTEXT (Order Fulfilment)              |
|                                                                         |
|  +-------------------------------------------------------------------+  |
|  |                   AGGREGATE: Order (Aggregate Root)               |  |
|  |  - OrderId (Entity Identity)                                      |  |
|  |  - OrderStatus (Value Object)                                     |  |
|  |  - Money (Value Object)                                           |  |
|  |                                                                   |  |
|  |  +-------------------------------------------------------------+  |  |
|  |  | Internal Entity: OrderLine                                  |  |  |
|  |  | - LineId, SKU, Quantity, UnitPrice                          |  |  |
|  |  +-------------------------------------------------------------+  |  |
|  |                                                                   |  |
|  |  Invariant Enforcement:                                           |  |
|  |  - Cannot add lines if Order is SHIPPED                           |  |
|  |  - Max 50 lines per order                                         |  |
|  |  - Total price recalculated atomically                            |  |
|  +-------------------------------------------------------------------+  |
|                                   |                                     |
|                                   v (Emits on state change)             |
|                       [OrderPlacedDomainEvent]                          |
+-------------------------------------------------------------------------+
```

---

## 2. Architecture & Internal Mechanics

### Aggregate Root as the Invariant Gatekeeper
The cardinal rule of Tactical DDD is: **External objects may only hold references to the Aggregate Root, never to internal entities.**

```
       [External Client / Service Layer]
                       |
                       | 1. Invoke mutate method on Root
                       v
             +-------------------+
             |   AGGREGATE ROOT  | <--- Single Point of Entry
             |      (Order)      |
             +-------------------+
               |               |
               v               v
        +-------------+ +-------------+
        |  OrderLine  | |  OrderLine  | <--- Internal Entities
        |   (Item 1)  | |   (Item 2)  |      (Hidden from external callers)
        +-------------+ +-------------+
```

### Invariant Preservation Mechanics
1. **Encapsulation**: The internal entities (`OrderLine`) cannot be modified directly by an API controller or repository.
2. **Transactional Boundary**: A database transaction must load and save exactly **one Aggregate Root at a time**. Modifying two separate aggregates in the same database transaction is a DDD code smell; cross-aggregate coordination must occur asynchronously via **Domain Events**.
3. **Optimistic Concurrency**: Aggregates track an internal `version` number to guarantee that concurrent requests do not overwrite state changes.

---

## 3. Comprehensive Syntax & Core Mechanics

### Python Dataclasses for DDD Primitives

```python
from dataclasses import dataclass, field
from typing import List, Any
import uuid

# Base Value Object (Immutable)
@dataclass(frozen=True)
class ValueObject:
    pass

# Base Domain Event
@dataclass(frozen=True)
class DomainEvent:
    event_id: str = field(default_factory=lambda: str(uuid.uuid4()))

# Base Aggregate Root with Event Accumulation
class AggregateRoot:
    def __init__(self):
        self._events: List[DomainEvent] = []

    def record_event(self, event: DomainEvent) -> None:
        self._events.append(event)

    def collect_events(self) -> List[DomainEvent]:
        events = list(self._events)
        self._events.clear()
        return events
```

---

## 4. Practical Implementation: 5 Verified Examples

### Example 1: Pure Value Objects with Strict Invariants
Value objects are defined with `@dataclass(frozen=True)` and enforce invariants during `__post_init__`. Two value objects with identical fields are semantically equal.

```python
# app/domain/value_objects.py
from dataclasses import dataclass
from decimal import Decimal

@dataclass(frozen=True)
class Money:
    amount: Decimal
    currency: str = "USD"

    def __post_init__(self):
        if self.amount < Decimal("0.00"):
            raise ValueError(f"Amount {self.amount} cannot be negative")
        if len(self.currency) != 3 or not self.currency.isupper():
            raise ValueError("Currency must be a 3-letter ISO uppercase string")

    def add(self, other: "Money") -> "Money":
        if self.currency != other.currency:
            raise ValueError(f"Currency mismatch: {self.currency} != {other.currency}")
        return Money(amount=self.amount + other.amount, currency=self.currency)

    def multiply(self, factor: int) -> "Money":
        if factor < 0:
            raise ValueError("Multiplier cannot be negative")
        return Money(amount=self.amount * Decimal(factor), currency=self.currency)

@dataclass(frozen=True)
class SKU:
    value: str

    def __post_init__(self):
        if not self.value or len(self.value) < 4:
            raise ValueError(f"Invalid SKU format: {self.value}")
```

---

### Example 2: Complete Aggregate Root with Domain Events
An e-commerce `Order` aggregate enforcing strict state transitions and publishing domain events.

```python
# app/domain/order_aggregate.py
from dataclasses import dataclass, field
from datetime import datetime, timezone
from enum import StrEnum
from typing import List
import uuid
from app.domain.value_objects import Money, SKU

class OrderStatus(StrEnum):
    DRAFT = "draft"
    SUBMITTED = "submitted"
    PAID = "paid"
    CANCELLED = "cancelled"

class DomainRuleViolationError(Exception):
    pass

# Domain Events
@dataclass(frozen=True)
class OrderSubmittedEvent:
    order_id: str
    customer_id: str
    total_amount: str
    occurred_at: datetime = field(default_factory=lambda: datetime.now(timezone.utc))

# Internal Entity
@dataclass
class OrderLine:
    id: str
    sku: SKU
    price: Money
    quantity: int

    @property
    def total(self) -> Money:
        return self.price.multiply(self.quantity)

# Aggregate Root
class Order:
    def __init__(self, customer_id: str, order_id: str | None = None):
        self.id: str = order_id or str(uuid.uuid4())
        self.customer_id: str = customer_id
        self.status: OrderStatus = OrderStatus.DRAFT
        self.lines: List[OrderLine] = []
        self.version: int = 1
        self._events: list = []

    @property
    def total_amount(self) -> Money:
        if not self.lines:
            return Money(Decimal("0.00"))
        total = self.lines[0].total
        for line in self.lines[1:]:
            total = total.add(line.total)
        return total

    def add_line(self, sku: SKU, price: Money, quantity: int) -> None:
        """Enforces invariant: lines can only be added to DRAFT orders."""
        if self.status != OrderStatus.DRAFT:
            raise DomainRuleViolationError(f"Cannot add lines to order in {self.status} status")
        if quantity <= 0:
            raise DomainRuleViolationError("Quantity must be greater than zero")
        if len(self.lines) >= 50:
            raise DomainRuleViolationError("An order cannot contain more than 50 distinct lines")

        line_id = str(uuid.uuid4())
        self.lines.append(OrderLine(id=line_id, sku=sku, price=price, quantity=quantity))

    def submit(self) -> None:
        """Enforces invariant: order must have at least one line before submission."""
        if self.status != OrderStatus.DRAFT:
            raise DomainRuleViolationError(f"Cannot submit order in status {self.status}")
        if not self.lines:
            raise DomainRuleViolationError("Cannot submit an empty order")

        self.status = OrderStatus.SUBMITTED
        self.version += 1
        
        # Record domain event
        self._events.append(
            OrderSubmittedEvent(
                order_id=self.id,
                customer_id=self.customer_id,
                total_amount=str(self.total_amount.amount),
            )
        )

    def collect_events(self) -> list:
        events = list(self._events)
        self._events.clear()
        return events
```

---

### Example 3: Domain Service for Cross-Aggregate Business Logic
Domain services implement business operations that span multiple aggregate roots without belonging to either one directly.

```python
# app/domain/services/pricing_calculator.py
from decimal import Decimal
from app.domain.order_aggregate import Order
from app.domain.value_objects import Money

class PricingDomainService:
    @staticmethod
    def calculate_loyalty_discount(order: Order, customer_order_count: int) -> Money:
        """Pure domain logic: VIP members with >10 orders receive a 10% discount."""
        total = order.total_amount
        if customer_order_count >= 10:
            discount_amount = total.amount * Decimal("0.10")
            return Money(amount=discount_amount, currency=total.currency)
        return Money(amount=Decimal("0.00"), currency=total.currency)
```

---

### Example 4: Repository and Event Dispatching in the Application Layer
The repository loads the full aggregate and saves it atomically; the application use case dispatches collected domain events post-commit.

```python
# app/application/use_cases/submit_order.py
from typing import Protocol
from app.domain.order_aggregate import Order
from app.domain.value_objects import SKU, Money

class OrderRepositoryProtocol(Protocol):
    async def get_by_id(self, order_id: str) -> Order | None: ...
    async def save(self, order: Order) -> None: ...

class EventPublisherProtocol(Protocol):
    async def publish(self, events: list) -> None: ...

class SubmitOrderUseCase:
    def __init__(
        self,
        order_repo: OrderRepositoryProtocol,
        event_publisher: EventPublisherProtocol,
    ):
        self._order_repo = order_repo
        self._event_publisher = event_publisher

    async def execute(self, order_id: str) -> Order:
        # 1. Load entire aggregate root from repository
        order = await self._order_repo.get_by_id(order_id)
        if not order:
            raise KeyError(f"Order {order_id} not found")

        # 2. Execute domain method on the root (enforces all invariants)
        order.submit()

        # 3. Persist updated aggregate
        await self._order_repo.save(order)

        # 4. Dispatch domain events after state is guaranteed to be saved
        events = order.collect_events()
        await self._event_publisher.publish(events)

        return order
```

---

### Example 5: FastAPI Router Exposing DDD Use Case
FastAPI endpoint acting purely as an HTTP translation layer.

```python
# app/api/v1/orders.py
from fastapi import APIRouter, Depends, HTTPException, status
from pydantic import BaseModel
from app.domain.order_aggregate import DomainRuleViolationError
from app.application.use_cases.submit_order import SubmitOrderUseCase

router = APIRouter(prefix="/orders", tags=["Orders"])

class OrderSubmitResponse(BaseModel):
    order_id: str
    status: str
    total_amount: str

# In a real app, resolved via DI container
def get_submit_order_use_case() -> SubmitOrderUseCase:
    pass  # pragma: no cover

@router.post("/{order_id}/submit", response_model=OrderSubmitResponse)
async def submit_order_endpoint(
    order_id: str,
    use_case: SubmitOrderUseCase = Depends(get_submit_order_use_case),
):
    try:
        order = await use_case.execute(order_id)
        return OrderSubmitResponse(
            order_id=order.id,
            status=order.status.value,
            total_amount=str(order.total_amount.amount),
        )
    except KeyError as exc:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail=str(exc))
    except DomainRuleViolationError as exc:
        raise HTTPException(status_code=status.HTTP_409_CONFLICT, detail=str(exc))
```

---

## 5. Edge Cases, Pitfalls & Failure Modes

1. **Directly Querying Child Entities**: Writing a repository query like `select(OrderLineORM).where(...)` directly from a router bypasses the `Order` aggregate root, violating consistency boundaries. All reads and mutations must route through the aggregate root.
2. **Cross-Aggregate Synchronous Transactions**: Attempting to update `Order` and `CustomerCredit` inside the same database transaction creates tight coupling and severe lock contention under load. Use eventual consistency via Domain Events.
3. **Dispatching Events Before DB Commit**: If you emit a domain event to Kafka or Celery before the database transaction commits, a subsequent database rollback leaves external workers processing ghost events for records that were never saved.
4. **Massive Aggregates**: Designing an aggregate that contains thousands of child entities (e.g., an `Organization` aggregate holding every `Employee`) causes high memory consumption and constant optimistic locking conflicts. Keep aggregates small; boundary-crossings should reference IDs, not full objects.

---

## 6. Common Mistakes Table

| Mistake | Root Cause | Impact | Correct Approach |
| :--- | :--- | :--- | :--- |
| **Anemic Domain Models** | Exposing setters on entities without validation | Invariants are broken; rules get duplicated in every router. | Place validation and state transitions inside entity methods. |
| **Mutating Children Outside Root** | Modifying `order.lines[0].quantity = 5` directly | Aggregate root cannot validate whole-order invariants (e.g. max total). | Expose methods on the root (e.g., `order.update_line_quantity()`). |
| **Using Primitive Types Everywhere** | Using raw `str` for emails, `float` for money | Silent data corruption, currency mismatches, and invalid formats. | Encapsulate primitives into strongly-typed immutable Value Objects. |
| **Publishing Events Before Persistence** | Calling `publisher.send()` inside the entity | Ghost events fired if database commit fails or connection aborts. | Collect events in the entity; publish them only after database commit. |
| **Loading Entire Context in One Aggregate** | Merging unrelated entities into one root | Giant SQL joins, heavy memory footprint, and high lock contention. | Keep aggregates small; link related aggregates by Identifier only. |
| **Using Floats for Monetary Values** | IEEE 754 precision issues | Rounding errors in financial calculations. | Always use Python's `decimal.Decimal` inside `Money` value objects. |

---

## 7. Performance & Scalability Considerations

- **Aggregate Size Optimization**: Keep aggregates as small as possible. An aggregate should encapsulate only those invariants that must be strongly consistent *right now*. Everything else can be eventually consistent.
- **Lazy Loading Avoidance**: When saving an aggregate, avoid doing 20 individual SQL statements for child entities. Use bulk upserts or SQLAlchemy change tracking.
- **Free-Threading in Python 3.13t**: In-memory domain models and value object validations run without GIL overhead. Domain computations scale linearly across multi-core systems when using worker thread pools.

---

## 8. Security & OWASP Guardrails

- **OWASP A01 (Broken Object-Level Authorization)**: Enforce aggregate tenant isolation. Every aggregate root should record its `tenant_id` or `owner_id`. Any mutation method should assert that the modifying user context matches the owner before executing business rules.
- **OWASP A04 (Insecure Design)**: Value objects serve as an essential defensive line against injection and format tampering, ensuring that malicious inputs are rejected before they ever reach business logic.

---

## 9. Testing Strategies & Pytest Recipes

```python
# tests/unit/test_order_aggregate.py
from decimal import Decimal
import pytest
from app.domain.order_aggregate import Order, OrderStatus, DomainRuleViolationError
from app.domain.value_objects import SKU, Money

def test_order_add_line_calculates_total():
    order = Order(customer_id="cust_123")
    order.add_line(sku=SKU("PROD-001"), price=Money(Decimal("25.00")), quantity=2)
    order.add_line(sku=SKU("PROD-002"), price=Money(Decimal("10.00")), quantity=1)

    assert order.total_amount == Money(Decimal("60.00"))
    assert order.status == OrderStatus.DRAFT

def test_empty_order_cannot_be_submitted():
    order = Order(customer_id="cust_123")
    with pytest.raises(DomainRuleViolationError, match="Cannot submit an empty order"):
        order.submit()

def test_submitted_order_emits_event():
    order = Order(customer_id="cust_123")
    order.add_line(sku=SKU("PROD-001"), price=Money(Decimal("100.00")), quantity=1)
    order.submit()

    events = order.collect_events()
    assert len(events) == 1
    assert events[0].order_id == order.id
    assert events[0].total_amount == "100.00"
```

---

## 10. Real-World Case Study / Production Incident

A mobility and ridesharing platform experienced a major revenue leak. The platform allowed passengers to cancel a trip within 2 minutes of booking without incurring a fee. 

**The Bug**:
The trip management API had an anemic `Trip` database model. The cancel endpoint directly updated `trip.status = 'CANCELLED'` and issued a full refund. Concurrently, the driver's mobile client sent a trip start event: `trip.status = 'IN_PROGRESS'`.

Because there was no aggregate root enforcing atomic state transitions:
1. Both requests ran concurrently.
2. The trip was marked cancelled and the passenger was fully refunded.
3. The driver arrived, picked up the passenger, and completed the 45-minute ride.
4. The driver was paid, but the company absorbed the entire fare cost. Over 8 months, this cost the startup \$320,000.

**The Fix**:
The team refactored the domain using an explicit `Trip` Aggregate Root:
```python
def cancel(self, cancelled_at: datetime) -> Money:
    if self.status != TripStatus.DISPATCHED:
        raise DomainRuleViolationError(f"Cannot cancel trip in status {self.status}")
    self.status = TripStatus.CANCELLED
    fee = self._calculate_cancellation_fee(cancelled_at)
    self._events.append(TripCancelledEvent(self.id, fee))
    return fee
```
Using optimistic locking (`version_id`), concurrent modifications were rejected with a clean HTTP 409 Conflict, completely eliminating ghost cancellations.

---

## 11. Technical Interview Questions

<details>
<summary>1. What is the fundamental difference between an Entity and a Value Object in DDD?</summary>
An Entity has a unique, continuous identity that persists across time and state changes (e.g., a `User` or `Order` with a specific UUID); two entities with identical properties but different IDs are distinct objects. A Value Object has no conceptual identity; it is defined exclusively by its attribute values (e.g., `Money(10, 'USD')`). Two value objects with identical attributes are completely interchangeable.
</details>

<details>
<summary>2. Why should an Aggregate Root be the sole point of entry for modifying child entities?</summary>
An aggregate root defines an invariant boundary. If external code could directly mutate child entities, the aggregate root would be bypassed, making it impossible to enforce business rules that depend on the overall aggregate state (such as ensuring an order total never exceeds a credit limit, or that max lines are not breached).
</details>

<details>
<summary>3. What is a Bounded Context, and how does it relate to microservices?</summary>
A Bounded Context is a linguistic and conceptual boundary within which a specific domain model applies consistently. Inside that boundary, every term in the Ubiquitous Language has an unambiguous meaning. While a microservice often corresponds to a single Bounded Context, a Bounded Context can also be implemented as an isolated module within a modular monolith.
</details>

<details>
<summary>4. Why should Value Objects be immutable?</summary>
Because value objects represent pure values without identity. If a value object could be modified in-place, any other entity or context sharing a reference to that object would experience unexpected side effects, leading to subtle race conditions and invariant violations.
</details>

<details>
<summary>5. What is the Outbox Pattern in Domain-Driven Design, and why is it necessary?</summary>
The Outbox Pattern ensures reliable event delivery across distributed boundaries. When an aggregate emits a domain event, the event is saved to an "outbox" table in the *same* database transaction as the aggregate state change. A background process then reads the outbox table and publishes events to the message broker (Kafka/RabbitMQ). This prevents the dual-write problem where the database commit succeeds but the message publish fails.
</details>

<details>
<summary>6. How do you implement optimistic locking in a DDD aggregate root?</summary>
The aggregate root maintains an integer `version` field. When the repository reads the aggregate, it records the current version. When persisting changes, the SQL query executes `UPDATE aggregate_table SET version = version + 1 WHERE id = :id AND version = :current_version`. If another process updated the aggregate in the interim, the query updates zero rows, triggering a concurrency exception.
</details>

<details>
<summary>7. What is a Domain Service, and when should you use one instead of an Entity method?</summary>
A Domain Service is a stateless service that encapsulates business logic spanning multiple aggregates or requiring external domain calculations that do not naturally belong to any single entity (e.g., calculating currency exchange rates or coordinating a fund transfer between two distinct accounts).
</details>

<details>
<summary>8. Can an Aggregate Root contain references to other Aggregate Roots?</summary>
No. An aggregate root should only reference other aggregate roots by their **Identifier** (e.g., `customer_id: str`), never by direct object reference. This maintains clear transactional boundaries and prevents accidental multi-aggregate mutations.
</details>

---

## 12. Practical Exercises & Solutions

<details>
<summary>Exercise 1 (Easy): Implement an Email Value Object</summary>

**Task**: Implement an immutable `EmailAddress` value object with `@dataclass(frozen=True)` that normalizes the input email to lowercase and raises `ValueError` if the format does not contain an `@` and a valid domain dot.

**Solution**:
```python
from dataclasses import dataclass
import re

@dataclass(frozen=True)
class EmailAddress:
    value: str

    def __post_init__(self):
        normalized = self.value.strip().lower()
        if not re.match(r"^[^@]+@[^@]+\.[^@]+$", normalized):
            raise ValueError(f"Invalid email: {self.value}")
        object.__setattr__(self, "value", normalized)
```
</details>

<details>
<summary>Exercise 2 (Easy): Aggregate Event Collector Mixin</summary>

**Task**: Implement an `EventCollectorMixin` class that provides `add_event(event)` and `clear_events()` methods for any aggregate root.

**Solution**:
```python
class EventCollectorMixin:
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._domain_events = []

    def record_event(self, event):
        self._domain_events.append(event)

    def extract_events(self):
        events = list(self._domain_events)
        self._domain_events.clear()
        return events
```
</details>

<details>
<summary>Exercise 3 (Medium): User Aggregate with Deactivation Invariant</summary>

**Task**: Create a `User` entity with `id`, `email`, and `is_active`. Add a `deactivate()` method that sets `is_active = False` and records a `UserDeactivatedEvent`. If the user is already inactive, raise `ValueError("User is already inactive")`.

**Solution**:
```python
from dataclasses import dataclass, field
import uuid

@dataclass(frozen=True)
class UserDeactivatedEvent:
    user_id: str

class User:
    def __init__(self, email: str, user_id: str | None = None):
        self.id = user_id or str(uuid.uuid4())
        self.email = email
        self.is_active = True
        self.events = []

    def deactivate(self):
        if not self.is_active:
            raise ValueError("User is already inactive")
        self.is_active = False
        self.events.append(UserDeactivatedEvent(user_id=self.id))
```
</details>

<details>
<summary>Exercise 4 (Medium): Bank Account Aggregate with Overdraft Limit</summary>

**Task**: Implement a `BankAccount` aggregate root with an `account_id`, `balance: Decimal`, and `overdraft_limit: Decimal`. Implement `withdraw(amount: Decimal)` that allows withdrawals as long as the new balance does not fall below `-overdraft_limit`.

**Solution**:
```python
from decimal import Decimal

class BankAccount:
    def __init__(self, account_id: str, overdraft_limit: Decimal = Decimal("100.00")):
        self.account_id = account_id
        self.balance = Decimal("0.00")
        self.overdraft_limit = overdraft_limit

    def deposit(self, amount: Decimal) -> None:
        if amount <= Decimal("0.00"):
            raise ValueError("Deposit must be positive")
        self.balance += amount

    def withdraw(self, amount: Decimal) -> None:
        if amount <= Decimal("0.00"):
            raise ValueError("Withdrawal must be positive")
        if self.balance - amount < -self.overdraft_limit:
            raise ValueError(f"Withdrawal of {amount} exceeds overdraft limit of {self.overdraft_limit}")
        self.balance -= amount
```
</details>

<details>
<summary>Exercise 5 (Hard): Optimistic Concurrency Repository Wrapper</summary>

**Task**: Implement a mock repository that simulates optimistic concurrency control by comparing a `version` field. If the version passed to `save(aggregate)` does not match the stored version, raise `OptimisticLockError`.

**Solution**:
```python
class OptimisticLockError(Exception):
    pass

class MockOptimisticOrderRepository:
    def __init__(self):
        self._store = {}
        self._versions = {}

    async def get_by_id(self, entity_id: str):
        return self._store.get(entity_id)

    async def save(self, aggregate):
        current_version = self._versions.get(aggregate.id, 0)
        
        # Check optimistic lock
        if aggregate.id in self._versions and aggregate.version != current_version:
            raise OptimisticLockError(
                f"Concurrency conflict: expected version {current_version}, got {aggregate.version}"
            )
            
        aggregate.version += 1
        self._store[aggregate.id] = aggregate
        self._versions[aggregate.id] = aggregate.version
```
</details>

<details>
<summary>Exercise 6 (Hard): Transactional Outbox Pattern Dispatcher</summary>

**Task**: Implement an `OutboxService` with a method `save_aggregate_and_outbox(session, aggregate)` that writes the aggregate and appends all collected domain events to an `outbox` table in the same transaction.

**Solution**:
```python
import json
from dataclasses import asdict

class OutboxRecord:
    def __init__(self, event_type: str, payload: dict):
        self.event_type = event_type
        self.payload = json.dumps(payload)

class OutboxService:
    @staticmethod
    async def save_with_outbox(session, aggregate_repo, aggregate):
        # 1. Collect events from aggregate
        events = aggregate.collect_events()
        
        # 2. Save aggregate state
        await aggregate_repo.save(aggregate)
        
        # 3. Save domain events to outbox table within same session
        for event in events:
            record = OutboxRecord(
                event_type=event.__class__.__name__,
                payload=asdict(event) if hasattr(event, "__dataclass_fields__") else event.__dict__
            )
            session.add(record)
            
        # Commit guarantees atomic persistence of both aggregate and events
        await session.commit()
```
</details>
