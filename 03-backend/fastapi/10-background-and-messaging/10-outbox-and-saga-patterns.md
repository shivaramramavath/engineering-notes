---
title: "Transactional Outbox and Saga Patterns in FastAPI"
category: "10-background-and-messaging"
order: 10
tags: ["fastapi", "outbox-pattern", "saga-pattern", "distributed-transactions", "event-driven", "sqlalchemy"]
py_version: "3.12+"
fastapi_version: "0.115+"
starlette_version: "0.38+"
pydantic_version: "2.8+"
tested_with: "Python 3.12, 3.13"
---

# Transactional Outbox and Saga Patterns in FastAPI

## 1. Conceptual Foundation & Mental Model

In modern microservices and asynchronous backend architectures, a fundamental architectural dilemma emerges whenever a service must **both** update its local database **and** publish a message to a message broker (Kafka, RabbitMQ, Redis). This dilemma is known as the **Dual-Write Problem**.

Consider what happens in a naive implementation:
```python
# THE DUAL-WRITE HAZARD:
async with db.begin():
    await db.execute(insert_order_stmt)
    # What if the app crashes or the network drops RIGHT HERE?
    await kafka_producer.send("orders", order_data)
```
- **Scenario A (DB first, Message second)**: If the database transaction commits successfully, but the network to Kafka drops or the Kubernetes pod is killed before publishing, the database has the order, but downstream services (Inventory, Billing) never receive the event. The system enters an inconsistent state.
- **Scenario B (Message first, DB second)**: If you publish to Kafka first, and then the database transaction fails due to a unique constraint violation or database crash, downstream services fulfill and charge an order that does not exist in the database.

Distributed 2-Phase Commit (2PC / XA transactions) across databases and message brokers is notoriously slow, fragile, and unsupported by modern cloud-native brokers.

To solve this, two enterprise architectural patterns are used:
1. **The Transactional Outbox Pattern**: Guarantees that database updates and message publishing are 100% atomic by saving outbound events into a dedicated `outbox` table **inside the exact same local database transaction**. A separate background relay worker (or CDC tool like Debezium) reads the outbox table and publishes events reliably to the broker.
2. **The Saga Pattern**: Orchestrates distributed multi-service transactions across multiple microservices without locking, using a series of local transactions coordinated either via an **Orchestrator** or **Choreography**, paired with **Compensating Transactions** (undo actions) if a step fails midway.

### The Mental Model: The Certified Mail Logbook
- Naive dual writes is like transferring \$1,000 to a friend in your ledger, and then promising to walk outside to drop a letter in the mailbox. If you trip down the stairs and break your leg, your money is recorded as transferred, but the letter never left the house.
- **Transactional Outbox** is writing the transfer in your ledger, and on the exact same page, writing a signed dispatch notice in an outgoing envelope pocket glued to that exact ledger page (**Single Local DB Transaction**). A courier opens your ledger once every minute, takes out the envelopes, mails them, and stamps "MAILED". If a power outage happens, the envelope stays in the book until the courier resumes.

---

## 2. Architecture & Internal Mechanics

```
   FastAPI HTTP Request: POST /orders
                |
                v
   +-------------------------------------------------------------+
   | Single ACID Database Transaction (PostgreSQL)               |
   |                                                             |
   | 1. INSERT INTO orders (id, user_id, amount)                 |
   | 2. INSERT INTO outbox_messages (id, event_type, payload)    |
   |                                                             |
   | [ COMMIT ] -> Either BOTH succeed, or NEITHER succeeds!     |
   +-------------------------------------------------------------+
                                |
                                v
   +-------------------------------------------------------------+
   | Outbox Relay Worker (Background Poller / Debezium CDC)     |
   |                                                             |
   | Polls: SELECT * FROM outbox_messages WHERE published = FALSE|
   | Publishes message to Kafka / RabbitMQ                       |
   | Marks: UPDATE outbox_messages SET published = TRUE          |
   +-------------------------------------------------------------+
                                |
                                v
   +-------------------------------------------------------------+
   | Message Broker (Kafka / RabbitMQ)                           |
   | Topic: `orders.created`                                     |
   +-------------------------------------------------------------+
```

---

## 3. Comprehensive Syntax & Core Mechanics

### 1. Database Outbox Model (SQLAlchemy 2.0 Async)
```python
import uuid
from datetime import datetime, timezone
from sqlalchemy import String, Text, Boolean, DateTime
from sqlalchemy.orm import DeclarativeBase, Mapped, mapped_column

class Base(DeclarativeBase):
    pass

class OutboxMessage(Base):
    __tablename__ = "outbox_messages"

    id: Mapped[uuid.UUID] = mapped_column(primary_key=True, default=uuid.uuid4)
    event_type: Mapped[str] = mapped_column(String(100), index=True)
    payload: Mapped[str] = mapped_column(Text)  # JSON serialized data
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc))
    published: Mapped[bool] = mapped_column(Boolean, default=False, index=True)
    published_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)
```

### 2. Atomic Insertion in FastAPI Endpoint
```python
import json
from sqlalchemy.ext.asyncio import AsyncSession

async def create_order_with_outbox(db: AsyncSession, order_data: dict):
    async with db.begin():
        # 1. Mutate business table
        # db.add(Order(...))
        
        # 2. Append event to outbox table IN THE SAME TRANSACTION
        event = OutboxMessage(
            event_type="OrderCreated",
            payload=json.dumps(order_data)
        )
        db.add(event)
        
    # On exit of async with db.begin(), both records are atomically committed!
```

---

## 4. Practical Implementation

### Example 1: Complete FastAPI Outbox Writer Route
```python
import json
import uuid
from fastapi import FastAPI, Depends, status
from pydantic import BaseModel
from sqlalchemy.ext.asyncio import AsyncSession

app = FastAPI()

class OrderCreateRequest(BaseModel):
    user_id: str
    amount: float
    items: list[str]

@app.post("/orders", status_code=status.HTTP_201_CREATED)
async def create_order(
    req: OrderCreateRequest,
    db: AsyncSession = Depends(get_db_session)
):
    order_id = str(uuid.uuid4())
    
    async with db.begin():
        # Write to Order entity (simulated)
        order_record = {
            "order_id": order_id,
            "user_id": req.user_id,
            "amount": req.amount,
            "status": "PENDING"
        }
        
        # Write corresponding Outbox event
        outbox_event = OutboxMessage(
            event_type="ORDER_CREATED",
            payload=json.dumps(order_record)
        )
        db.add(outbox_event)
        
    return {"order_id": order_id, "status": "CREATED_ATOMIC"}
```

### Example 2: Production Asynchronous Outbox Relay Worker Loop
```python
import asyncio
from datetime import datetime, timezone
from sqlalchemy import select, update
from sqlalchemy.ext.asyncio import async_sessionmaker

async def outbox_relay_poller(
    session_factory: async_sessionmaker,
    message_publisher_func,
    stop_event: asyncio.Event
):
    print("Outbox relay worker started...")
    while not stop_event.is_set():
        try:
            async with session_factory() as session:
                # 1. Fetch unpublished outbox messages with row-level lock (SKIP LOCKED)
                # to allow multiple relay worker pods to run concurrently without conflict!
                stmt = (
                    select(OutboxMessage)
                    .where(OutboxMessage.published == False)
                    .order_by(OutboxMessage.created_at)
                    .limit(50)
                    .with_for_update(skip_locked=True)
                )
                result = await session.execute(stmt)
                messages = result.scalars().all()
                
                if not messages:
                    await asyncio.sleep(0.5)
                    continue

                for msg in messages:
                    # 2. Publish to Kafka or RabbitMQ
                    await message_publisher_func(msg.event_type, msg.payload)
                    
                    # 3. Mark as published
                    msg.published = True
                    msg.published_at = datetime.now(timezone.utc)
                    
                await session.commit()
                print(f"Relayed and acknowledged {len(messages)} outbox messages.")
        except asyncio.CancelledError:
            break
        except Exception as exc:
            print(f"Error in outbox relay: {exc}")
            await asyncio.sleep(2.0)
```

### Example 3: Edge Case — Saga Pattern Orchestrator with Compensating Actions
A booking system orchestrator coordinating three microservices (Flight, Hotel, Payment). If Hotel booking fails, execute compensating transactions to refund payment and cancel flight:
```python
import asyncio
from typing import Callable, Any

class SagaStep:
    def __init__(
        self,
        name: str,
        action: Callable[[], Any],
        compensate: Callable[[], Any]
    ):
        self.name = name
        self.action = action
        self.compensate = compensate

class SagaCoordinator:
    def __init__(self):
        self.steps: list[SagaStep] = []
        self.executed_steps: list[SagaStep] = []

    def add_step(self, step: SagaStep):
        self.steps.append(step)

    async def execute(self) -> bool:
        for step in self.steps:
            try:
                print(f"Executing step: {step.name}")
                await step.action()
                self.executed_steps.append(step)
            except Exception as exc:
                print(f"Step '{step.name}' failed: {exc}. Initiating compensating rollbacks...")
                await self._rollback()
                return False
        return True

    async def _rollback(self):
        # Compensating transactions run in REVERSE order
        for step in reversed(self.executed_steps):
            try:
                print(f"Compensating: {step.name}")
                await step.compensate()
            except Exception as roll_exc:
                print(f"CRITICAL: Compensation for '{step.name}' failed: {roll_exc}!")
```

### Example 4: Failure Case — The Dual-Write Disaster
```python
# CATASTROPHIC ANTI-PATTERN: Dual Write without Outbox
@app.post("/bad-checkout")
async def bad_checkout(order_id: str, db, kafka_producer):
    # Step 1: DB Transaction commits
    async with db.begin():
        await db.execute("INSERT INTO orders (id) VALUES (:id)", {"id": order_id})
        
    # Step 2: Outbound network call
    # IF KAFKA BROKER DIES OR POD RESTARTS HERE:
    # Order exists in Postgres, but warehouse NEVER fulfills it!
    # Permanent silent data discrepancy!
    await kafka_producer.send("orders", {"id": order_id})
```

### Example 5: Idiomatic Modern — Choreography Saga Event Listener
In a Choreographed Saga, each service listens for events and triggers its local transaction:
```python
async def handle_order_created_event(event_payload: dict, db_session, outbox_writer):
    order_id = event_payload["order_id"]
    # Local transaction 1: Reserve inventory
    success = await reserve_warehouse_stock(order_id)
    
    if success:
        # Publish next event via outbox
        await outbox_writer.write("INVENTORY_RESERVED", {"order_id": order_id})
    else:
        # Publish failure event triggering upstream compensation
        await outbox_writer.write("INVENTORY_RESERVATION_FAILED", {"order_id": order_id})
```

---

## 5. Edge Cases, Pitfalls & Failure Modes

1. **Table Bloat in High-Volume Outboxes**: An outbox table processing 1,000 writes/sec will accumulate 86 million records per day. If records are merely marked `published = True` without periodic purging, queries slow down and disk space fills up. Implement an automated partitioned table rotation or clean up records after publication.
2. **At-Least-Once Delivery to Broker**: If the outbox worker publishes the message to Kafka, but crashes *before* committing `UPDATE outbox_messages SET published = TRUE`, the replacement worker will publish the message a second time. **Downstream consumers MUST be idempotent**.
3. **Compensating Transaction Failures (Pivot Step)**: In a Saga, if a compensating transaction fails (e.g. refunding the credit card fails due to invalid gateway credentials), the Saga enters a "Zombie State" requiring human administrative intervention.
4. **Outbox Polling Overhead vs CDC**: Polling a PostgreSQL table every 500ms creates continuous database read load. For enterprise-scale systems ($>10,000\text{ events/sec}$), replace polling with Change Data Capture (CDC) via **Debezium**, which tails PostgreSQL's Write-Ahead Log (WAL) directly with zero polling query overhead.

---

## 6. Common Mistakes & Anti-Patterns

| Anti-Pattern | Root Cause | Impact | Correct Practice |
|:---|:---|:---|:---|
| Dual writes without transaction isolation | Assuming network never fails | Lost messages, inventory desynchronization | Use the Transactional Outbox Pattern |
| Outbox polling without `SKIP LOCKED` | Concurrent relay workers reading same rows | Lock contention, duplicate dispatches | Use `SELECT ... FOR UPDATE SKIP LOCKED` |
| Never purging published outbox rows | Missing archival policy | Gigabyte table bloat; index performance collapses | Delete records immediately or partition by day |
| Non-idempotent Saga downstream consumers | Assuming outbox relay delivers exactly once | Duplicate billing or stock deductions | Enforce unique transaction keys on consumers |
| Missing compensation logic in Sagas | Forgetting distributed rollback steps | Partial state committed when downstream fails | Implement explicit compensating actions |
| Using 2-Phase Commit (2PC) over HTTP/REST | Attempting synchronous locking across services | Network timeouts cascade into global service freeze | Use asynchronous Sagas with eventual consistency |

---

## 7. Performance & Scalability Considerations

- **Row Locking with `SKIP LOCKED`**: In PostgreSQL, `SELECT ... FOR UPDATE SKIP LOCKED` allows multiple independent relay worker pods to read distinct batches from the outbox table concurrently without blocking each other.
- **CDC with Debezium**: Using Debezium to read PostgreSQL WAL logs bypasses database query engines entirely, streaming outbox changes to Kafka with $<5\text{ms}$ latency and zero database CPU overhead.

---

## 8. Security & OWASP Guardrails

- **Outbox Payload Sanitization**: Never write unredacted credit card numbers (PANs) or raw passwords into outbox payloads. Outbox tables are often mirrored into Kafka streams, search indexes, and data lakes.

---

## 9. Testing Strategies & Pytest Recipes

```python
import pytest
from sqlalchemy.ext.asyncio import create_async_engine, async_sessionmaker
from sqlalchemy import select

@pytest.mark.anyio
async def test_outbox_message_committed_atomically():
    engine = create_async_engine("sqlite+aiosqlite:///:memory:")
    async_session = async_sessionmaker(engine, expire_on_commit=False)
    
    async with engine.begin() as conn:
        await conn.run_sync(Base.metadata.create_all)
        
    async with async_session() as session:
        async with session.begin():
            # Add message
            msg = OutboxMessage(event_type="TEST_EVENT", payload='{"test": 1}')
            session.add(msg)
            
    # Verify persistence
    async with async_session() as session:
        res = await session.execute(select(OutboxMessage).where(OutboxMessage.event_type == "TEST_EVENT"))
        record = res.scalar_one()
        assert record.published is False
        assert record.payload == '{"test": 1}'
```

---

## 10. Real-World Case Study / Production Incident

**The Incident**: A high-end sneaker marketplace crashed during a limited-edition flash drop. 10,000 customers were charged $250 on Stripe, but only 4,200 orders were created in PostgreSQL. Over 5,800 customers were charged without receiving order confirmations.

**Root Cause**: The checkout endpoint used the naive dual-write pattern:
```python
# Charge credit card via Stripe
await stripe_client.charge(token, 250)
# Then write to Postgres
await db.execute("INSERT INTO orders ...")
```
Under peak concurrency, the PostgreSQL connection pool became saturated and began rejecting queries with `TooManyConnectionsError`. The Stripe charges had succeeded, but the database writes crashed!

**The Fix**:
1. Re-architected checkout using the **Transactional Outbox Pattern**:
   - The endpoint creates an Order with status `PENDING` and an Outbox event `ORDER_INITIATED` inside a single local transaction.
   - An asynchronous worker consumes the outbox event and charges Stripe.
   - If Stripe fails, a compensating transaction marks the order as `PAYMENT_FAILED`.
2. Financial integrity was permanently secured: no card was ever charged without a guaranteed tracking record.

---

## 11. Technical Interview Questions & Answers

<details>
<summary>1. What is the Dual-Write Problem in microservices?</summary>
The Dual-Write Problem occurs when an application must update two separate distributed data stores (e.g. updating a relational database and publishing to a message broker like Kafka). Because atomic distributed transactions (2PC) across heterogeneous systems are impractical, a failure between the two operations leaves the system in an inconsistent state.
</details>

<details>
<summary>2. How does the Transactional Outbox Pattern solve the Dual-Write Problem?</summary>
It eliminates the dual write by saving both the business entity mutation and the outbound event notification into the <b>exact same local relational database</b> inside a single ACID transaction. A separate relay process reads the outbox table and reliably publishes the messages to the broker.
</details>

<details>
<summary>3. Why is <code>SELECT ... FOR UPDATE SKIP LOCKED</code> critical when running multiple outbox relay workers?</summary>
Without <code>SKIP LOCKED</code>, multiple concurrent relay worker processes attempting to query unpublished outbox rows will block waiting on each other's locks, causing lock contention. <code>SKIP LOCKED</code> tells PostgreSQL to bypass rows currently locked by other workers, allowing each worker to instantly claim an independent batch of rows.
</details>

<details>
<summary>4. What is the difference between an Orchestrated Saga and a Choreographed Saga?</summary>
In an <b>Orchestrated Saga</b>, a central coordinator service sends commands to individual participant services and listens for replies, determining whether to proceed or trigger compensating rollbacks. In a <b>Choreographed Saga</b>, there is no central controller: each microservice listens for domain events from other services and independently executes its local transaction and publishes the next event.
</details>

<details>
<summary>5. What is a Compensating Transaction in the Saga Pattern?</summary>
A compensating transaction is an explicit business action that semantically reverses the effects of a previously committed local transaction when a downstream step in the Saga fails (e.g. releasing reserved warehouse inventory or issuing a credit card refund).
</details>

<details>
<summary>6. Does the Transactional Outbox Pattern guarantee Exactly-Once or At-Least-Once delivery to the broker?</summary>
It guarantees <b>At-Least-Once delivery</b>. If the relay worker publishes the event to Kafka but crashes before marking the outbox row as <code>published = True</code> in the database, the row will be re-read and published again upon recovery. Downstream consumers must implement idempotency.
</details>

<details>
<summary>7. What is Change Data Capture (CDC) and how does it optimize the Outbox Pattern?</summary>
CDC tools (like Debezium) read database transaction logs (e.g. PostgreSQL WAL) directly from disk instead of issuing polling SQL queries (<code>SELECT * FROM outbox</code>). This reduces database query load to zero and streams outbox events to Kafka with sub-second latency.
</details>

<details>
<summary>8. In a Saga, what is the "Pivot Step"?</summary>
The Pivot Step is the point of no return in a Saga. Steps executed *before* the pivot can be compensated (rolled back). Once the pivot step successfully commits, the Saga cannot be rolled back; all subsequent steps *must* be retried until they succeed.
</details>

---

## 12. Practical Exercises & Solutions

### Exercise 1 (Easy): Outbox SQLAlchemy Table Schema
**Task**: Define an asynchronous SQLAlchemy 2.0 declarative model for an `OutboxEvent` table containing fields for `id` (UUID), `topic`, `payload` (JSON string), and `is_delivered` (Boolean).
<details>
<summary>Solution</summary>

```python
import uuid
from sqlalchemy import String, Text, Boolean
from sqlalchemy.orm import DeclarativeBase, Mapped, mapped_column

class Base(DeclarativeBase):
    pass

class OutboxEvent(Base):
    __tablename__ = "outbox_events"

    id: Mapped[uuid.UUID] = mapped_column(primary_key=True, default=uuid.uuid4)
    topic: Mapped[str] = mapped_column(String(128), index=True)
    payload: Mapped[str] = mapped_column(Text)
    is_delivered: Mapped[bool] = mapped_column(Boolean, default=False, index=True)
```
</details>

### Exercise 2 (Easy): Atomic Outbox Writer Utility
**Task**: Write an async helper function `save_with_outbox(session, business_model, event_topic, event_dict)` that adds both the business entity and an outbox event to the session and commits.
<details>
<summary>Solution</summary>

```python
import json
from sqlalchemy.ext.asyncio import AsyncSession

async def save_with_outbox(session: AsyncSession, business_model, event_topic: str, event_dict: dict):
    async with session.begin():
        session.add(business_model)
        outbox_entry = OutboxEvent(
            topic=event_topic,
            payload=json.dumps(event_dict),
            is_delivered=False
        )
        session.add(outbox_entry)
```
</details>

### Exercise 3 (Medium): Outbox Poller Query with `SKIP LOCKED`
**Task**: Write an asynchronous query using SQLAlchemy 2.0 that selects up to 25 undelivered outbox events ordered by ID, using `with_for_update(skip_locked=True)`.
<details>
<summary>Solution</summary>

```python
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

async def fetch_undelivered_batch(session: AsyncSession) -> list[OutboxEvent]:
    stmt = (
        select(OutboxEvent)
        .where(OutboxEvent.is_delivered == False)
        .order_by(OutboxEvent.id)
        .limit(25)
        .with_for_update(skip_locked=True)
    )
    result = await session.execute(stmt)
    return list(result.scalars().all())
```
</details>

### Exercise 4 (Medium): Simple Orchestrated Saga Engine
**Task**: Implement a 2-step Saga orchestrator (`ChargeCard` -> `ReserveInventory`). If `ReserveInventory` raises an exception, invoke `RefundCard`.
<details>
<summary>Solution</summary>

```python
class PaymentSaga:
    def __init__(self, amount: float, item_id: str):
        self.amount = amount
        self.item_id = item_id
        self.charged = False

    async def execute(self):
        # Step 1: Charge Card
        print(f"Charging card ${self.amount}")
        self.charged = True
        
        # Step 2: Reserve Inventory
        try:
            print(f"Attempting to reserve {self.item_id}...")
            # Simulate failure
            raise RuntimeError("Item out of stock!")
        except Exception as exc:
            print(f"Inventory reservation failed: {exc}")
            await self.compensate()
            return False
        return True

    async def compensate(self):
        if self.charged:
            print(f"COMPENSATING: Refunding card ${self.amount}")
            self.charged = False
```
</details>

### Exercise 5 (Hard): Complete Outbox Relay Worker Task
**Task**: Build an asynchronous background task that connects to PostgreSQL, reads undelivered outbox records using `skip_locked=True`, simulates publishing each to a broker, marks them as delivered, and commits.
<details>
<summary>Solution</summary>

```python
import asyncio
from sqlalchemy.ext.asyncio import async_sessionmaker

async def run_outbox_relay(session_factory: async_sessionmaker, broker_publisher_fn, stop_event: asyncio.Event):
    while not stop_event.is_set():
        try:
            async with session_factory() as session:
                async with session.begin():
                    stmt = (
                        select(OutboxEvent)
                        .where(OutboxEvent.is_delivered == False)
                        .limit(50)
                        .with_for_update(skip_locked=True)
                    )
                    res = await session.execute(stmt)
                    records = res.scalars().all()
                    
                    if not records:
                        await asyncio.sleep(0.5)
                        continue
                        
                    for rec in records:
                        # Publish to broker
                        await broker_publisher_fn(rec.topic, rec.payload)
                        rec.is_delivered = True
                        
            await asyncio.sleep(0.1)
        except asyncio.CancelledError:
            break
        except Exception as exc:
            print(f"Relay error: {exc}")
            await asyncio.sleep(1.0)
```
</details>

### Exercise 6 (Hard): Multi-Participant Saga Coordinator with Timeout
**Task**: Build a generalized asynchronous Saga Coordinator that accepts arbitrary forward and compensating coroutines, executing them with a per-step timeout and executing all registered compensations in reverse order upon any failure.
<details>
<summary>Solution</summary>

```python
import asyncio
from typing import Callable, Coroutine, Any

class SagaStep:
    def __init__(self, name: str, forward: Callable[[], Coroutine], compensate: Callable[[], Coroutine], timeout: float = 3.0):
        self.name = name
        self.forward = forward
        self.compensate = compensate
        self.timeout = timeout

class ResilientSagaCoordinator:
    def __init__(self):
        self.steps: list[SagaStep] = []
        self.completed: list[SagaStep] = []

    def add(self, step: SagaStep):
        self.steps.append(step)

    async def run(self) -> bool:
        for step in self.steps:
            try:
                print(f"Saga: Running {step.name}")
                async with asyncio.timeout(step.timeout):
                    await step.forward()
                self.completed.append(step)
            except Exception as exc:
                print(f"Saga step {step.name} failed: {exc}. Rolling back...")
                await self._compensate_all()
                return False
        return True

    async def _compensate_all(self):
        for step in reversed(self.completed):
            try:
                print(f"Saga: Compensating {step.name}")
                async with asyncio.timeout(5.0):
                    await step.compensate()
            except Exception as roll_err:
                print(f"CRITICAL: Failed to compensate {step.name}: {roll_err}")
```
</details>
