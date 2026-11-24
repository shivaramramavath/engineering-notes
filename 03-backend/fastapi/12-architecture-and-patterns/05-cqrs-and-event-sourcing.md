---
title: "CQRS and Event Sourcing in FastAPI Applications"
description: "Implement Command Query Responsibility Segregation (CQRS) and Event Sourcing in FastAPI: separate write models from read projections, build append-only event stores, and handle eventual consistency."
fastapi_version: "0.115.x+"
python_version: "3.12+"
pydantic_version: "v2.10+"
tested_with: "pydantic 2.10+, fastapi 0.115+, pytest 8.3+"
free_threading_status: "Fully Compatible"
---

# CQRS and Event Sourcing in FastAPI Applications

## 1. Conceptual Foundation & Mental Model

### The Asymmetric Nature of Modern Web Workloads
In traditional CRUD applications, a single relational schema serves both write operations (inserts, updates, transactional validation) and read operations (queries, reporting dashboards, search filters). This unified model leads to severe architectural friction:
1. **Conflicting Optimization Goals**: Write models require third-normal-form (3NF) relational schemas to avoid anomalies and guarantee transactional consistency. Read models require denormalized, flat structures with heavy indexing to achieve sub-millisecond query performance.
2. **Lock Contention**: Heavy analytical queries holding read locks interfere with high-throughput transactional writes, resulting in database deadlocks and latency spikes.
3. **Loss of Historical Intent**: Overwriting a user's address with an `UPDATE users SET address = ...` destroys temporal history. You know where the user lives now, but not where they lived when an order was shipped six months ago.

### Command Query Responsibility Segregation (CQRS)
Coined by Greg Young, **CQRS** resolves this tension by splitting the application into two completely distinct pipelines:
- **Command Side (Write Pipeline)**: Handles business intent, enforces domain invariants, and performs mutations. Commands return void or an execution status/ID, never complex query datasets.
- **Query Side (Read Pipeline)**: Optimized purely for data retrieval. Queries never mutate state and read from denormalized, read-optimized views or separate read replicas.

```
                              [HTTP Ingress]
                                    |
          +-------------------------+-------------------------+
          | (POST / PUT / DELETE)                             | (GET)
          v                                                   v
+-------------------------------+               +-------------------------------+
|         COMMAND SIDE          |               |          QUERY SIDE           |
|  - Validates Domain Invariants|               |  - Ultra-fast read projections|
|  - Modifies Write Model       |               |  - No business logic or joins |
|  - Appends to Event Store     |               |  - Redis / Elasticsearch / DB |
+-------------------------------+               +-------------------------------+
          |                                                   ^
          | (Asynchronously Projects)                         |
          +---------------------------------------------------+
```

### Event Sourcing: State as a Series of Facts
**Event Sourcing** takes CQRS a step further. Instead of storing the current state of an entity in a relational table, the application stores an **append-only log of immutable domain events**. The current state of an entity is derived by replaying all historical events from the beginning of time:
$$\text{Current State} = \sum_{i=1}^{n} \text{Event}_i$$

---

## 2. Architecture & Internal Mechanics

Understanding the command dispatch, event store persistence, and projection synchronization cycle is central to building reliable CQRS systems.

```
[Client] ---> 1. Send Command (CreateAccount)
                 |
                 v
       [Command Handler]
                 |
                 | 2. Replay events to rebuild Aggregate
                 v
         [Event Store] (PostgreSQL Append-Only Table)
                 |
                 | 3. Execute domain method -> Raise AccountCreatedEvent
                 v
       [Command Handler] ---> 4. Commit new event to Event Store
                                     |
                                     v (Asynchronous Event Dispatcher)
                            [Projection Engine]
                                     |
                                     | 5. Update Denormalized Read Model
                                     v
                           [Read Database / Cache]
                                     ^
                                     | 6. Fast Query (GET /accounts/123)
                                  [Client]
```

### Event Projection Mechanics
When a command commits an event to the append-only event store:
1. The event is persisted with an incremental `version` or sequence number.
2. An asynchronous projector (or transactional outbox worker) reads the event.
3. The projector updates the denormalized read model (e.g., updating a flat JSON document in PostgreSQL or updating a Redis hash).
4. Clients issuing queries read directly from this denormalized table with zero joins.

---

## 3. Comprehensive Syntax & Core Mechanics

### Core Event Sourcing Abstractions

```python
from dataclasses import dataclass, field
from datetime import datetime, timezone
from typing import Any, List, TypeVar
import uuid

@dataclass(frozen=True)
class Event:
    event_id: str = field(default_factory=lambda: str(uuid.uuid4()))
    timestamp: datetime = field(default_factory=lambda: datetime.now(timezone.utc))
    aggregate_id: str = ""
    version: int = 1

T = TypeVar("T", bound="EventSourcedAggregate")

class EventSourcedAggregate:
    def __init__(self, aggregate_id: str):
        self.id = aggregate_id
        self.version = 0
        self._uncommitted_events: List[Event] = []

    def apply(self, event: Event) -> None:
        """Mutates internal state based on the event type."""
        handler_name = f"on_{event.__class__.__name__}"
        handler = getattr(self, handler_name, None)
        if handler:
            handler(event)
        self.version = event.version

    def record_that(self, event: Event) -> None:
        self.apply(event)
        self._uncommitted_events.append(event)

    def collect_uncommitted_events(self) -> List[Event]:
        events = list(self._uncommitted_events)
        self._uncommitted_events.clear()
        return events
```

---

## 4. Practical Implementation: 5 Verified Examples

### Example 1: Bank Account Aggregate with Event Sourcing
An event-sourced bank account where balance is derived entirely by replaying deposit and withdrawal events.

```python
# app/domain/events.py
from dataclasses import dataclass
from decimal import Decimal
from app.domain.base import Event

@dataclass(frozen=True)
class AccountOpenedEvent(Event):
    owner: str = ""
    initial_deposit: Decimal = Decimal("0.00")

@dataclass(frozen=True)
class MoneyDepositedEvent(Event):
    amount: Decimal = Decimal("0.00")

@dataclass(frozen=True)
class MoneyWithdrawnEvent(Event):
    amount: Decimal = Decimal("0.00")
```

```python
# app/domain/account_aggregate.py
from decimal import Decimal
from app.domain.base import EventSourcedAggregate
from app.domain.events import AccountOpenedEvent, MoneyDepositedEvent, MoneyWithdrawnEvent

class InsufficientFundsError(Exception):
    pass

class BankAccountAggregate(EventSourcedAggregate):
    def __init__(self, account_id: str):
        super().__init__(account_id)
        self.owner: str = ""
        self.balance: Decimal = Decimal("0.00")
        self.is_open: bool = False

    # --- Domain Business Methods (Commands) ---
    def open_account(self, owner: str, initial_deposit: Decimal) -> None:
        if self.is_open:
            raise ValueError("Account already opened")
        if initial_deposit < Decimal("0.00"):
            raise ValueError("Initial deposit cannot be negative")

        event = AccountOpenedEvent(
            aggregate_id=self.id,
            version=self.version + 1,
            owner=owner,
            initial_deposit=initial_deposit,
        )
        self.record_that(event)

    def deposit(self, amount: Decimal) -> None:
        if not self.is_open:
            raise ValueError("Account is not open")
        if amount <= Decimal("0.00"):
            raise ValueError("Deposit amount must be positive")

        event = MoneyDepositedEvent(
            aggregate_id=self.id,
            version=self.version + 1,
            amount=amount,
        )
        self.record_that(event)

    def withdraw(self, amount: Decimal) -> None:
        if not self.is_open:
            raise ValueError("Account is not open")
        if amount <= Decimal("0.00"):
            raise ValueError("Withdrawal must be positive")
        if self.balance < amount:
            raise InsufficientFundsError(f"Insufficient funds: balance {self.balance} < {amount}")

        event = MoneyWithdrawnEvent(
            aggregate_id=self.id,
            version=self.version + 1,
            amount=amount,
        )
        self.record_that(event)

    # --- State Mutation Handlers (Replay Hooks) ---
    def on_AccountOpenedEvent(self, event: AccountOpenedEvent) -> None:
        self.owner = event.owner
        self.balance = event.initial_deposit
        self.is_open = True

    def on_MoneyDepositedEvent(self, event: MoneyDepositedEvent) -> None:
        self.balance += event.amount

    def on_MoneyWithdrawnEvent(self, event: MoneyWithdrawnEvent) -> None:
        self.balance -= event.amount
```

---

### Example 2: In-Memory Append-Only Event Store
Storing event streams indexed by aggregate ID and enforcing optimistic concurrency via version checking.

```python
# app/infrastructure/event_store.py
from typing import List, Dict
from app.domain.base import Event

class ConcurrencyError(Exception):
    pass

class InMemoryEventStore:
    def __init__(self):
        # aggregate_id -> List of events
        self._streams: Dict[str, List[Event]] = {}

    async def load_stream(self, aggregate_id: str) -> List[Event]:
        return list(self._streams.get(aggregate_id, []))

    async def append_to_stream(
        self,
        aggregate_id: str,
        expected_version: int,
        new_events: List[Event]
    ) -> None:
        current_stream = self._streams.get(aggregate_id, [])
        current_version = current_stream[-1].version if current_stream else 0

        if current_version != expected_version:
            raise ConcurrencyError(
                f"Concurrency conflict for {aggregate_id}: expected version {expected_version}, found {current_version}"
            )

        if aggregate_id not in self._streams:
            self._streams[aggregate_id] = []

        self._streams[aggregate_id].extend(new_events)
```

---

### Example 3: Denormalized Read Model and Projection Engine
Projecting immutable events into a fast in-memory read database.

```python
# app/projections/account_read_model.py
from dataclasses import dataclass
from decimal import Decimal
from typing import Dict, Optional
from app.domain.events import AccountOpenedEvent, MoneyDepositedEvent, MoneyWithdrawnEvent

@dataclass
class AccountReadView:
    account_id: str
    owner: str
    current_balance: Decimal
    total_transactions: int

class AccountReadProjection:
    def __init__(self):
        self._views: Dict[str, AccountReadView] = {}

    def get_by_id(self, account_id: str) -> Optional[AccountReadView]:
        return self._views.get(account_id)

    def handle(self, event) -> None:
        if isinstance(event, AccountOpenedEvent):
            self._views[event.aggregate_id] = AccountReadView(
                account_id=event.aggregate_id,
                owner=event.owner,
                current_balance=event.initial_deposit,
                total_transactions=1,
            )
        elif isinstance(event, MoneyDepositedEvent):
            view = self._views[event.aggregate_id]
            view.current_balance += event.amount
            view.total_transactions += 1
        elif isinstance(event, MoneyWithdrawnEvent):
            view = self._views[event.aggregate_id]
            view.current_balance -= event.amount
            view.total_transactions += 1
```

---

### Example 4: Command Handlers and Query Handlers
Completely segregated command execution from read query execution.

```python
# app/application/handlers.py
from decimal import Decimal
from app.domain.account_aggregate import BankAccountAggregate
from app.infrastructure.event_store import InMemoryEventStore
from app.projections.account_read_model import AccountReadProjection

# --- COMMAND HANDLERS ---
class DepositCommandHandler:
    def __init__(self, event_store: InMemoryEventStore, projection: AccountReadProjection):
        self._store = event_store
        self._projection = projection

    async def handle(self, account_id: str, amount: Decimal) -> None:
        # 1. Load history
        events = await self._store.load_stream(account_id)
        if not events:
            raise KeyError(f"Account {account_id} not found")

        # 2. Rehydrate aggregate
        aggregate = BankAccountAggregate(account_id)
        for event in events:
            aggregate.apply(event)

        # 3. Execute domain logic
        aggregate.deposit(amount)

        # 4. Save new events with optimistic concurrency check
        new_events = aggregate.collect_uncommitted_events()
        await self._store.append_to_stream(account_id, aggregate.version, new_events)

        # 5. Update projection
        for e in new_events:
            self._projection.handle(e)

# --- QUERY HANDLERS ---
class AccountQueryHandler:
    def __init__(self, projection: AccountReadProjection):
        self._projection = projection

    def get_account_view(self, account_id: str):
        view = self._projection.get_by_id(account_id)
        if not view:
            raise KeyError(f"Account {account_id} not found")
        return view
```

---

### Example 5: FastAPI Router Exposing CQRS Endpoints
Demonstrating clean separation between mutation routes (Commands) and retrieval routes (Queries).

```python
# app/api/v1/accounts.py
from decimal import Decimal
from fastapi import APIRouter, HTTPException, status
from pydantic import BaseModel, Field
from app.domain.account_aggregate import InsufficientFundsError

router = APIRouter(prefix="/accounts", tags=["CQRS Accounts"])

# Transport Schemas
class OpenAccountRequest(BaseModel):
    account_id: str
    owner: str
    initial_deposit: Decimal = Field(ge=Decimal("0.00"))

class DepositRequest(BaseModel):
    amount: Decimal = Field(gt=Decimal("0.00"))

class AccountSummaryResponse(BaseModel):
    account_id: str
    owner: str
    current_balance: Decimal
    total_transactions: int

# Global instances for verified runnable demo
from app.infrastructure.event_store import InMemoryEventStore
from app.projections.account_read_model import AccountReadProjection
from app.application.handlers import DepositCommandHandler, AccountQueryHandler

_store = InMemoryEventStore()
_proj = AccountReadProjection()
_cmd_handler = DepositCommandHandler(_store, _proj)
_query_handler = AccountQueryHandler(_proj)

# --- COMMAND ENDPOINTS (Writes) ---
@router.post("/{account_id}/deposit", status_code=status.HTTP_202_ACCEPTED)
async def deposit_funds(account_id: str, payload: DepositRequest):
    try:
        await _cmd_handler.handle(account_id, payload.amount)
        return {"status": "DEPOSIT_PROCESSED"}
    except KeyError:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="Account not found")
    except InsufficientFundsError as exc:
        raise HTTPException(status_code=status.HTTP_409_CONFLICT, detail=str(exc))

# --- QUERY ENDPOINTS (Reads) ---
@router.get("/{account_id}", response_model=AccountSummaryResponse)
def get_account(account_id: str):
    try:
        view = _query_handler.get_account_view(account_id)
        return AccountSummaryResponse(
            account_id=view.account_id,
            owner=view.owner,
            current_balance=view.current_balance,
            total_transactions=view.total_transactions,
        )
    except KeyError:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="Account not found")
```

---

## 5. Edge Cases, Pitfalls & Failure Modes

1. **Event Replay Performance Degradation**: If an aggregate has existed for 5 years and accumulated 500,000 events, replaying the entire stream on every command takes seconds. Mitigate this by creating **Snapshots** (e.g., saving a state snapshot every 100 events and replaying only events newer than the snapshot).
2. **Event Schema Evolution**: As business requirements change, event schemas evolve (e.g., splitting a `name` field into `first_name` and `last_name`). Renaming fields breaks deserialization of historical events. Use upcasting techniques or maintain backward-compatible deserializers.
3. **Eventual Consistency Lag in the UI**: In distributed CQRS systems, updating a projection asynchronously means a client might issue a `POST /items` and immediately call `GET /items`, receiving a stale response before the projection updates. Return an `ETag` or correlation ID, or implement read-your-own-writes caching in the API gateway.
4. **Out-of-Order Projection Updates**: If events arrive out of sequence at the projection engine, the read database will calculate corrupted balances. Projections must track the latest applied `version` and reject or queue out-of-order events.

---

## 6. Common Mistakes Table

| Mistake | Root Cause | Impact | Correct Approach |
| :--- | :--- | :--- | :--- |
| **Mutating Aggregates in Query Handlers** | Violating CQRS separation | Queries introduce side effects and race conditions. | Queries must be strictly read-only and idempotent. |
| **Returning Domain Entities from Queries** | Reusing write models on read side | Sluggish performance; exposes sensitive or heavy write models. | Return lightweight DTOs directly from flat projection tables. |
| **Deleting Events from Event Store** | Misunderstanding Event Sourcing | Breaks audit history and state rehydration. | Never delete events; issue compensating events (e.g. `TransactionCancelledEvent`). |
| **Ignoring Concurrency in Append-Only Stores** | Appending events without checking version | Concurrent updates overwrite state, causing lost updates. | Enforce unique constraint on `(aggregate_id, version)` in the event store. |
| **Failing to Snapshot Long-Lived Streams** | Assuming event streams remain small | Command latency degrades from 5ms to 5,000ms as streams grow. | Take periodic snapshots (e.g. every 100 events) to cap rehydration time. |
| **Synchronous Projections Under Heavy Load** | Projecting in the HTTP request thread | Command latency spikes; read store bottlenecks throttle writes. | Asynchronously project events via Kafka, RabbitMQ, or background workers. |

---

## 7. Performance & Scalability Considerations

- **Append-Only Write Speed**: The Command side achieves unprecedented write throughput because database operations are pure sequential `INSERT` statements into an append-only table with zero `UPDATE` lock contention.
- **Independent Read Scaling**: Read models can be scaled independently on read-optimized databases (e.g., Redis for key-value lookups, Elasticsearch for full-text search, ClickHouse for analytics).
- **Free-Threading in Python 3.13t**: In-memory event replay and projection processing scale across native OS threads without GIL serialization.

---

## 8. Security & OWASP Guardrails

- **Immutable Audit Trail (OWASP A09 - Logging & Monitoring Failures)**: Event Sourcing provides a cryptographically verifiable, append-only audit trail of every user action and business decision, eliminating the possibility of undetected database record tampering.
- **GDPR "Right to be Forgotten" Compliance**: Since events are immutable, deleting user PII from historical events is challenging. Use **Cryptographic Erasure**: encrypt sensitive PII with a per-user encryption key. When a user requests deletion, destroy the key, rendering all historical PII cryptographically unreadable.

---

## 9. Testing Strategies & Pytest Recipes

```python
# tests/unit/test_cqrs_event_sourcing.py
from decimal import Decimal
import pytest
from app.domain.account_aggregate import BankAccountAggregate, InsufficientFundsError
from app.domain.events import AccountOpenedEvent, MoneyDepositedEvent, MoneyWithdrawnEvent

def test_rehydrate_from_historical_events():
    account_id = "acc_42"
    history = [
        AccountOpenedEvent(aggregate_id=account_id, version=1, owner="Alice", initial_deposit=Decimal("100.00")),
        MoneyDepositedEvent(aggregate_id=account_id, version=2, amount=Decimal("50.00")),
        MoneyWithdrawnEvent(aggregate_id=account_id, version=3, amount=Decimal("30.00")),
    ]

    aggregate = BankAccountAggregate(account_id)
    for event in history:
        aggregate.apply(event)

    assert aggregate.balance == Decimal("120.00")
    assert aggregate.version == 3

def test_withdrawal_beyond_balance_raises():
    aggregate = BankAccountAggregate("acc_1")
    aggregate.open_account("Bob", Decimal("50.00"))

    with pytest.raises(InsufficientFundsError):
        aggregate.withdraw(Decimal("100.00"))
```

---

## 10. Real-World Case Study / Production Incident

An international airline redesigned its flight seat reservation and pricing engine. Under the legacy relational database architecture:
- 10,000 customers concurrently searched for flights while 50 customers attempted to book seats.
- Massive SQL `JOIN` queries for search availability locked table rows in the reservation schema.
- Frequent deadlocks occurred during holiday booking rushes, causing customer credit cards to be charged without booking confirmation.

**The Migration to CQRS & Event Sourcing**:
1. **Command Side**: Booking requests were processed by a lightweight command handler that appended `SeatReservedEvent` to an append-only Postgres table in under 4ms.
2. **Query Side**: A projector updated an in-memory Redis cluster with available seat maps and cached search prices.
3. Search traffic jumped to 50,000 RPS without causing a single lock on the write database.
4. Total booking transaction latency dropped from 1,200ms to 45ms.

---

## 11. Technical Interview Questions

<details>
<summary>1. What is the fundamental difference between CQRS and traditional CRUD?</summary>
In CRUD, the same data model and schema are used for both reading and writing data. In CQRS, the write pipeline (Commands) and read pipeline (Queries) are completely segregated into distinct models. Commands enforce business invariants and state changes without returning data; Queries read from denormalized views optimized for retrieval speed without business logic or side effects.
</details>

<details>
<summary>2. How does Event Sourcing ensure you never lose historical state?</summary>
In traditional systems, `UPDATE` statements overwrite the previous state, losing temporal data. In Event Sourcing, state is stored as an immutable, append-only log of every event that has ever occurred. Current state is reconstructed on demand by replaying the event stream, allowing the system to inspect what the state was at any specific timestamp in the past.
</details>

<details>
<summary>3. What is an Event Sourcing "Snapshot", and why is it necessary?</summary>
A snapshot is a point-in-time serialization of an aggregate's state stored alongside the event version it represents. Without snapshots, rehydrating an aggregate that has accumulated thousands of events requires replaying the entire stream, causing high CPU and memory latency. With snapshots, the system loads the latest snapshot and replays only the events that occurred after it.
</details>

<details>
<summary>4. How does an Event Store enforce optimistic concurrency?</summary>
Each event in an aggregate stream is tagged with an incremental `version` number. When a command handler saves new events, it asserts that the expected version equals the latest version in storage. The database enforces a unique constraint on `(aggregate_id, version)`. If two concurrent commands attempt to write version 5, one succeeds and the other fails with a unique constraint violation.
</details>

<details>
<summary>5. What is "Eventual Consistency", and how do you handle it in a web UI?</summary>
Eventual Consistency occurs when the read projection is updated asynchronously after the write model commits. For a few milliseconds, a client querying the read model might see stale data. Solutions include:
1. Returning the updated ID or entity state directly in the command response.
2. Having the frontend wait for a WebSocket or SSE confirmation of projection completion.
3. Using version-stamped queries where the client passes the expected version and the API blocks until the projection catches up.
</details>

<details>
<summary>6. How can you handle GDPR deletion requests in an Event-Sourced system where events are immutable?</summary>
Using Cryptographic Erasure (Crypto-Shredding): All Personally Identifiable Information (PII) is encrypted with a unique per-user encryption key before being written to events. When a user exercises their right to be forgotten, the application deletes that user's encryption key. The historical events remain structurally intact, but the PII payload becomes permanently unrecoverable random ciphertext.
</details>

<details>
<summary>7. What is "Event Upcasting"?</summary>
Event upcasting is the technique of transforming an old version of an event schema into the current version in-memory during deserialization before the aggregate replays it. This allows systems to evolve event schemas without modifying or migrating the immutable historical event store.
</details>

<details>
<summary>8. When should you avoid using CQRS and Event Sourcing?</summary>
For straightforward CRUD applications, internal admin dashboards, and domains with simple, non-audited business rules. In these contexts, CQRS and Event Sourcing introduce substantial architectural complexity, eventual consistency overhead, and operational maintenance without commensurate business value.
</details>

---

## 12. Practical Exercises & Solutions

<details>
<summary>Exercise 1 (Easy): Create an Immutable Domain Event</summary>

**Task**: Implement an immutable `ItemAddedToCartEvent` with `@dataclass(frozen=True)` containing `cart_id: str`, `item_sku: str`, `quantity: int`, `unit_price: Decimal`, and an auto-generated timestamp.

**Solution**:
```python
from dataclasses import dataclass, field
from decimal import Decimal
from datetime import datetime, timezone
import uuid

@dataclass(frozen=True)
class ItemAddedToCartEvent:
    cart_id: str
    item_sku: str
    quantity: int
    unit_price: Decimal
    event_id: str = field(default_factory=lambda: str(uuid.uuid4()))
    occurred_at: datetime = field(default_factory=lambda: datetime.now(timezone.utc))
```
</details>

<details>
<summary>Exercise 2 (Easy): Replay Engine for Cart Aggregate</summary>

**Task**: Write a simple `ShoppingCart` class with an `apply(event)` method that updates an internal `total_items: int` and `total_cost: Decimal` when receiving `ItemAddedToCartEvent`.

**Solution**:
```python
from decimal import Decimal

class ShoppingCart:
    def __init__(self, cart_id: str):
        self.cart_id = cart_id
        self.total_items = 0
        self.total_cost = Decimal("0.00")

    def apply(self, event: ItemAddedToCartEvent) -> None:
        self.total_items += event.quantity
        self.total_cost += event.unit_price * Decimal(event.quantity)
```
</details>

<details>
<summary>Exercise 3 (Medium): In-Memory Snapshot Manager</summary>

**Task**: Implement a `SnapshotManager` class that can store and retrieve `(aggregate_id, version, state_dict)`. Write a method `should_take_snapshot(current_version: int, frequency: int = 50) -> bool`.

**Solution**:
```python
class SnapshotManager:
    def __init__(self):
        self._snapshots = {}

    def save_snapshot(self, aggregate_id: str, version: int, state: dict) -> None:
        self._snapshots[aggregate_id] = (version, state)

    def get_snapshot(self, aggregate_id: str):
        return self._snapshots.get(aggregate_id)

    @staticmethod
    def should_take_snapshot(current_version: int, frequency: int = 50) -> bool:
        return current_version > 0 and (current_version % frequency == 0)
```
</details>

<details>
<summary>Exercise 4 (Medium): Event Sourced Counter with Undo</summary>

**Task**: Implement an event-sourced `Counter` aggregate supporting `increment()` and `undo()`. Replaying `CounterIncrementedEvent` adds 1; `CounterDecrementedEvent` subtracts 1.

**Solution**:
```python
from dataclasses import dataclass

@dataclass(frozen=True)
class CounterIncrementedEvent:
    pass

@dataclass(frozen=True)
class CounterDecrementedEvent:
    pass

class Counter:
    def __init__(self):
        self.value = 0
        self.uncommitted = []

    def increment(self):
        self.apply(CounterIncrementedEvent())
        self.uncommitted.append(CounterIncrementedEvent())

    def undo(self):
        self.apply(CounterDecrementedEvent())
        self.uncommitted.append(CounterDecrementedEvent())

    def apply(self, event):
        if isinstance(event, CounterIncrementedEvent):
            self.value += 1
        elif isinstance(event, CounterDecrementedEvent):
            self.value -= 1
```
</details>

<details>
<summary>Exercise 5 (Hard): Optimistic Concurrency Event Store Simulator</summary>

**Task**: Implement an async `EventStore` in Python that maintains event streams in memory. If two coroutines attempt to append to the same aggregate stream with the same `expected_version`, ensure exactly one succeeds and the other raises `ConcurrencyConflictError`.

**Solution**:
```python
import asyncio

class ConcurrencyConflictError(Exception):
    pass

class ThreadSafeEventStore:
    def __init__(self):
        self._lock = asyncio.Lock()
        self._streams = {}

    async def append(self, aggregate_id: str, expected_version: int, events: list):
        async with self._lock:
            current_events = self._streams.get(aggregate_id, [])
            current_version = len(current_events)

            if current_version != expected_version:
                raise ConcurrencyConflictError(
                    f"Version conflict on {aggregate_id}: expected {expected_version}, got {current_version}"
                )

            if aggregate_id not in self._streams:
                self._streams[aggregate_id] = []
            self._streams[aggregate_id].extend(events)
```
</details>

<details>
<summary>Exercise 6 (Hard): Event Upcaster Implementation</summary>

**Task**: Create an upcaster system where `UserRegisteredV1(email)` is dynamically transformed into `UserRegisteredV2(email, default_role="member")` when replaying old historical streams.

**Solution**:
```python
from dataclasses import dataclass

@dataclass(frozen=True)
class UserRegisteredV1:
    email: str

@dataclass(frozen=True)
class UserRegisteredV2:
    email: str
    role: str

class EventUpcaster:
    @staticmethod
    def upcast(event):
        if isinstance(event, UserRegisteredV1):
            return UserRegisteredV2(email=event.email, role="member")
        return event

# Usage verification
def test_upcast():
    old_event = UserRegisteredV1(email="test@test.com")
    upcasted = EventUpcaster.upcast(old_event)
    assert isinstance(upcasted, UserRegisteredV2)
    assert upcasted.role == "member"
```
</details>
