---
title: "Concurrency Hazards: Race Conditions, Deadlocks, and Starvation in FastAPI"
category: "09-async-and-concurrency"
order: 8
tags: ["fastapi", "concurrency-hazards", "race-conditions", "deadlocks", "locks", "starvation"]
py_version: "3.12+"
fastapi_version: "0.115+"
starlette_version: "0.38+"
pydantic_version: "2.8+"
tested_with: "Python 3.12, 3.13"
---

# Concurrency Hazards: Race Conditions, Deadlocks, and Starvation in FastAPI

## 1. Conceptual Foundation & Mental Model

A common misconception among Python developers is that asynchronous single-threaded code (`asyncio`) is immune to **race conditions** because only one coroutine executes at any given millisecond. 

This assumption is dangerously false.

While `asyncio` code is immune to *preemptive* race conditions (where the operating system interrupts execution between individual CPU bytecode instructions), it is acutely vulnerable to **cooperative race conditions** across suspension points (`await`). Whenever a coroutine awaits an asynchronous operation (a database query, cache fetch, or network call), it yields control back to the event loop. In the intervening time, another concurrent coroutine handling a different HTTP request can mutate the shared state, creating Time-Of-Check to Time-Of-Use (TOCTOU) bugs, double-spend vulnerabilities, and corrupted in-memory structures.

### The Three Classic Hazards in FastAPI
1. **Async Race Conditions (TOCTOU)**: Coroutine A checks inventory balance (`balance = 100`). While Coroutine A awaits payment authorization, Coroutine B checks the same balance (`balance = 100`) and subtracts 100. Coroutine A resumes and also subtracts 100. The account balance is now $-100$ (overdrawn).
2. **Async Deadlocks**: Coroutine 1 acquires Lock A and attempts to acquire Lock B. Simultaneously, Coroutine 2 acquires Lock B and attempts to acquire Lock A. Both coroutines suspend waiting on each other; neither yields; the requests hang forever.
3. **Event Loop Starvation**: A coroutine performs a long-running CPU computation or blocking call without yielding, preventing the loop from advancing other waiting tasks.

---

## 2. Architecture & Internal Mechanics

### Anatomy of an Async Race Condition
```
Time   Coroutine 1 (POST /withdraw)          Coroutine 2 (POST /withdraw)
 |
 |     Reads Account Balance: $100
 |     Is Balance >= $100? YES
 |     Awaits Stripe Payment Call...
 |     ----------------------------> Yields Loop Control!
 |                                           Reads Account Balance: $100
 |                                           Is Balance >= $100? YES
 |                                           Awaits Stripe Payment Call...
 |     <---------------------------- Resumes!
 |     Deducts $100 -> Balance is $0
 |     Completes successfully.
 |                                           <---------------------------- Resumes!
 |                                           Deducts $100 -> Balance is -$100 (CORRUPTED!)
 v                                           Double-spend achieved!
```

---

## 3. Comprehensive Syntax & Core Mechanics

### 1. `asyncio.Lock` for Critical Sections
```python
import asyncio

shared_counter = 0
counter_lock = asyncio.Lock()

async def safe_increment():
    global shared_counter
    async with counter_lock:
        # Atomic across suspension points
        current = shared_counter
        await asyncio.sleep(0.01)  # Yielding while holding lock
        shared_counter = current + 1
```

### 2. Lock Invariants in Multi-Worker Architectures
`asyncio.Lock` only synchronizes coroutines **within the same single process**. If your FastAPI application runs under Uvicorn with `--workers 4` or across multiple Kubernetes replicas, `asyncio.Lock` provides **ZERO** protection across processes. Distributed synchronization requires Redis/Postgres row-level locks (`SELECT FOR UPDATE`).

---

## 4. Practical Implementation

### Example 1: Basic In-Memory Race Condition and Fix
```python
import asyncio
from fastapi import FastAPI

app = FastAPI()

# Buggy Unprotected State
unsafe_counter = 0

@app.post("/unsafe-increment")
async def unsafe_inc() -> dict[str, int]:
    global unsafe_counter
    temp = unsafe_counter
    await asyncio.sleep(0.001)  # Context switch opportunity
    unsafe_counter = temp + 1
    return {"counter": unsafe_counter}

# Safe Protected State
safe_counter = 0
mutex = asyncio.Lock()

@app.post("/safe-increment")
async def safe_inc() -> dict[str, int]:
    global safe_counter
    async with mutex:
        temp = safe_counter
        await asyncio.sleep(0.001)
        safe_counter = temp + 1
        return {"counter": safe_counter}
```

### Example 2: Production Safe Balance Deduction with Database Row Locking
```python
from fastapi import FastAPI, HTTPException, status
from pydantic import BaseModel
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy import select, update
from sqlalchemy.orm import declarative_base, Mapped, mapped_column

Base = declarative_base()

class Account(Base):
    __tablename__ = "accounts"
    id: Mapped[int] = mapped_column(primary_key=True)
    balance_cents: Mapped[int] = mapped_column()

app = FastAPI()

class WithdrawRequest(BaseModel):
    account_id: int
    amount_cents: int

@app.post("/accounts/withdraw")
async def withdraw_funds(req: WithdrawRequest, db: AsyncSession):
    async with db.begin():
        # SELECT ... FOR UPDATE acquires a pessimistic row lock in PostgreSQL
        # Preventing concurrent transactions from reading this row until committed!
        stmt = (
            select(Account)
            .where(Account.id == req.account_id)
            .with_for_update()
        )
        result = await db.execute(stmt)
        account = result.scalar_one_or_none()
        
        if not account:
            raise HTTPException(status_code=404, detail="Account not found")
            
        if account.balance_cents < req.amount_cents:
            raise HTTPException(status_code=400, detail="Insufficient funds")
            
        # Safe atomic update
        account.balance_cents -= req.amount_cents
        
    return {"new_balance": account.balance_cents}
```

### Example 3: Edge Case — Deadlock Prevention via Deterministic Lock Ordering
```python
import asyncio
from fastapi import FastAPI, HTTPException

app = FastAPI()

# Simulated resource locks
locks = {
    "resource_A": asyncio.Lock(),
    "resource_B": asyncio.Lock(),
}

@app.post("/transfer-resources")
async def transfer_resources(source: str, target: str):
    if source not in locks or target not in locks:
        raise HTTPException(status_code=400, detail="Unknown resource")
        
    # ANTI-DEADLOCK RULE: Always acquire locks in deterministic lexicographical order!
    # Even if one request transfers A -> B and another transfers B -> A,
    # both will lock A first, then B, making deadlocks mathematically impossible!
    first, second = sorted([source, target])
    
    async with locks[first]:
        async with locks[second]:
            await asyncio.sleep(0.05)  # Critical work
            return {"status": "transferred", "first": first, "second": second}
```

### Example 4: Failure Case — Cross-Request Deadlock
```python
import asyncio
from fastapi import FastAPI

app = FastAPI()
lock_1 = asyncio.Lock()
lock_2 = asyncio.Lock()

# DEADLOCK HAZARD: Non-deterministic acquisition order
@app.get("/deadlock-endpoint-1")
async def endpoint_1():
    async with lock_1:
        await asyncio.sleep(0.01)  # Context switch
        async with lock_2:         # Waits for lock_2 (held by endpoint_2)
            return {"status": "1"}

@app.get("/deadlock-endpoint-2")
async def endpoint_2():
    async with lock_2:
        await asyncio.sleep(0.01)  # Context switch
        async with lock_1:         # Waits for lock_1 (held by endpoint_1)
            return {"status": "2"}
```

### Example 5: Idiomatic Modern — Read-Write Lock (Fair Reader-Writer)
```python
import asyncio
from contextlib import asynccontextmanager

class AsyncRWLock:
    def __init__(self):
        self._readers = 0
        self._write_lock = asyncio.Lock()
        self._reader_lock = asyncio.Lock()

    @asynccontextmanager
    async def read_lock(self):
        async with self._reader_lock:
            self._readers += 1
            if self._readers == 1:
                await self._write_lock.acquire()
        try:
            yield
        finally:
            async with self._reader_lock:
                self._readers -= 1
                if self._readers == 0:
                    self._write_lock.release()

    @asynccontextmanager
    async def write_lock(self):
        async with self._write_lock:
            yield
```

---

## 5. Edge Cases, Pitfalls & Failure Modes

1. **Process Boundaries and False Security**: Developers using `asyncio.Lock` assuming it works across multiple Uvicorn workers. Each worker process has its own separate memory space; `asyncio.Lock` does not synchronize across processes.
2. **Locking During Unbounded Network Calls**: Holding an in-memory lock while making an outbound HTTP request to a slow third-party service locks up all other requests. If the third-party service hangs, all incoming requests queue behind the lock indefinitely.
3. **Deadlocks in Lifespan Shutdown**: If background tasks are holding locks when Uvicorn signals shutdown, and the shutdown handler attempts to acquire the same lock, the application hangs and refuses to exit until killed with `SIGKILL`.
4. **Starvation of Writers**: In simple Read-Write locks, a continuous stream of incoming read requests can keep reader counts greater than 0 indefinitely, starving write requests forever.

---

## 6. Common Mistakes & Anti-Patterns

| Anti-Pattern | Root Cause | Impact | Correct Practice |
|:---|:---|:---|:---|
| Using `asyncio.Lock` across multi-process workers | Believing in-memory locks work across processes | Silent race conditions in production | Use Redis distributed locks or Postgres row locks |
| Holding locks over external HTTP calls | Wrapping slow I/O inside critical sections | Massive request queuing and timeouts | Keep critical sections minimal; lock data only |
| Inconsistent lock ordering | Acquiring resources in arbitrary sequences | Classical deadlock (ABBA deadlocks) | Enforce strict lexicographical lock ordering |
| Forgetting `async with` on locks | Calling `lock.acquire()` without `finally: lock.release()` | Permanent deadlock on any exception | Always use `async with lock:` |
| Mutating global lists/dicts across yields | Believing GIL prevents race conditions | Corrupted dictionary states / data loss | Synchronize via locks or use immutable data |
| Non-reentrant lock deadlocks | Coroutine calling itself while holding lock | Immediate self-deadlock | Use reentrant locking logic or refactor structure |

---

## 7. Performance & Scalability Considerations

- **Lock Contention**: When 500 concurrent requests contend for a single lock, throughput drops to serialized single-threaded speed ($1 / T_{critical}$). Keep critical sections bounded to $<1$ millisecond.
- **Pessimistic vs Optimistic Locking**: For database balance deductions, pessimistic locking (`SELECT ... FOR UPDATE`) prevents conflicts at the cost of database row locking overhead. Optimistic locking (`WHERE version = 5`) provides higher concurrency for read-heavy workloads.

---

## 8. Security & OWASP Guardrails

- **Race Condition Vulnerabilities (OWASP Top 10 - Broken Access Control)**: TOCTOU race conditions allow users to duplicate coupons, gift card balances, or inventory purchases. Always enforce atomic database transactions or distributed locks for business-critical state mutations.

---

## 9. Testing Strategies & Pytest Recipes

```python
import pytest
import asyncio
from httpx import AsyncClient, ASGITransport
from fastapi import FastAPI

app = FastAPI()
counter = 0
lock = asyncio.Lock()

@app.post("/increment")
async def inc():
    global counter
    async with lock:
        val = counter
        await asyncio.sleep(0.001)
        counter = val + 1
    return {"counter": counter}

@pytest.mark.anyio
async def test_race_condition_immunity():
    global counter
    counter = 0
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        # Fire 50 concurrent increments
        tasks = [client.post("/increment") for _ in range(50)]
        responses = await asyncio.gather(*tasks)
        
        assert all(r.status_code == 200 for r in responses)
        # Without lock, counter would be much less than 50
        assert counter == 50
```

---

## 10. Real-World Case Study / Production Incident

**The Incident**: A cryptocurrency exchange suffered a $220,000 loss in 12 minutes due to an automated exploit script. An attacker created an account with $500 and submitted 40 concurrent withdrawal requests within 8 milliseconds.

**Root Cause**: The withdrawal endpoint checked the account balance:
```python
balance = await db.get_balance(user_id)
if balance >= amount:
    await crypto_wallet.send_funds(destination, amount)  # 800ms network call
    await db.deduct_balance(user_id, amount)
```
Because the balance check and the balance deduction were separated by an 800ms `await crypto_wallet.send_funds()`, all 40 concurrent requests saw a balance of $500 and approved the withdrawal before the first deduction executed.

**The Fix**:
1. Switched to strict database row-level locking with atomic balance deduction inside a single transaction before dispatching the outbound blockchain transaction.
2. Added an idempotency key requirement on all withdrawal endpoints.

---

## 11. Technical Interview Questions & Answers

<details>
<summary>1. Does Python's single-threaded <code>asyncio</code> protect code from race conditions?</summary>
No. While it protects against preemptive OS thread race conditions within a single bytecode instruction, it does NOT protect against cooperative race conditions across suspension points (<code>await</code>). Any time a coroutine awaits, another coroutine can run and modify shared state.
</details>

<details>
<summary>2. Why will an <code>asyncio.Lock</code> fail to prevent race conditions when running FastAPI with Uvicorn <code>--workers 4</code>?</summary>
Each Uvicorn worker is a separate operating system process with its own private Python interpreter and memory space. An <code>asyncio.Lock</code> exists only in the memory of the single process that created it. Coroutines running in worker 2 know nothing about locks held in worker 1.
</details>

<details>
<summary>3. What is an ABBA deadlock in asynchronous code and how do you prevent it?</summary>
An ABBA deadlock occurs when Task 1 acquires Lock A and waits for Lock B, while Task 2 acquires Lock B and waits for Lock A. You prevent it by enforcing a <b>deterministic global lock ordering</b> (e.g. always acquiring locks in alphabetical or numerical ID order).
</details>

<details>
<summary>4. What is a TOCTOU race condition in the context of FastAPI APIs?</summary>
Time-Of-Check to Time-Of-Use. A coroutine checks a condition (e.g., verifying user credit balance), then yields execution via an <code>await</code> before mutating the state. During the await, another request changes the state, invalidating the initial check and corrupting the system.
</details>

<details>
<summary>5. How does PostgreSQL <code>SELECT ... FOR UPDATE</code> solve concurrent update race conditions?</summary>
It acquires an exclusive row-level lock on the queried database rows. Any concurrent database transaction attempting to read or update those same rows is forced to wait until the locking transaction issues a <code>COMMIT</code> or <code>ROLLBACK</code>.
</details>

<details>
<summary>6. What is the danger of holding an <code>asyncio.Lock</code> while awaiting an external third-party HTTP API?</summary>
If the external HTTP API experiences latency spikes or network timeouts (e.g., 30 seconds), your lock remains held for 30 seconds. Every other concurrent request requiring that lock is blocked, causing massive request backlogs and server exhaustion.
</details>

<details>
<summary>7. What is Lock Starvation in async applications?</summary>
Starvation occurs when a runnable task is perpetually denied access to a shared resource or lock because higher-priority or continuously arriving tasks monopolize the resource (e.g., continuous read locks preventing a write lock from ever acquiring).
</details>

<details>
<summary>8. Is <code>asyncio.Lock</code> reentrant in Python?</summary>
No. Standard <code>asyncio.Lock</code> is NOT reentrant. If a coroutine currently holding an <code>asyncio.Lock</code> attempts to acquire it a second time (e.g., through a recursive call or shared utility function), it will deadlock itself forever.
</details>

---

## 12. Practical Exercises & Solutions

### Exercise 1 (Easy): In-Memory Mutex Guard
**Task**: Build an in-memory inventory reservation endpoint `/reserve` that uses an `asyncio.Lock` to ensure an inventory counter never drops below zero.
<details>
<summary>Solution</summary>

```python
import asyncio
from fastapi import FastAPI, HTTPException

app = FastAPI()
inventory_count = 5
inventory_lock = asyncio.Lock()

@app.post("/reserve")
async def reserve_item():
    global inventory_count
    async with inventory_lock:
        if inventory_count <= 0:
            raise HTTPException(status_code=400, detail="Out of stock")
        await asyncio.sleep(0.01)  # Context switch
        inventory_count -= 1
        return {"remaining": inventory_count}
```
</details>

### Exercise 2 (Easy): Non-Blocking Try-Lock
**Task**: Implement an endpoint `/busy-work` that attempts to acquire an `asyncio.Lock` using `locked()`. If the lock is already held by another request, immediately return HTTP 429 Too Many Requests without waiting.
<details>
<summary>Solution</summary>

```python
import asyncio
from fastapi import FastAPI, HTTPException, status

app = FastAPI()
single_task_lock = asyncio.Lock()

@app.post("/busy-work")
async def busy_work():
    if single_task_lock.locked():
        raise HTTPException(
            status_code=status.HTTP_429_TOO_MANY_REQUESTS,
            detail="Another operation is already in progress"
        )
        
    async with single_task_lock:
        await asyncio.sleep(1.0)
        return {"status": "completed"}
```
</details>

### Exercise 3 (Medium): Deadlock-Free Pair Locker
**Task**: Build a function `lock_pair(lock_a, lock_b)` that guarantees deadlock-free acquisition of two named locks by sorting their string identifiers before acquiring them.
<details>
<summary>Solution</summary>

```python
import asyncio
from contextlib import asynccontextmanager
from typing import AsyncIterator

@asynccontextmanager
async def lock_pair(id_a: str, lock_a: asyncio.Lock, id_b: str, lock_b: asyncio.Lock) -> AsyncIterator[None]:
    # Determine deterministic order
    if id_a < id_b:
        first_lock, second_lock = lock_a, lock_b
    else:
        first_lock, second_lock = lock_b, lock_a

    async with first_lock:
        async with second_lock:
            yield
```
</details>

### Exercise 4 (Medium): Bounded Lock with Timeout
**Task**: Write an async endpoint `/process-exclusive` that attempts to acquire a shared lock, but if the lock cannot be acquired within 0.2 seconds, times out and returns HTTP 503.
<details>
<summary>Solution</summary>

```python
import asyncio
from fastapi import FastAPI, HTTPException, status

app = FastAPI()
shared_lock = asyncio.Lock()

@app.post("/process-exclusive")
async def process_exclusive():
    try:
        async with asyncio.timeout(0.2):
            async with shared_lock:
                await asyncio.sleep(0.5)
                return {"result": "exclusive_work_done"}
    except TimeoutError:
        raise HTTPException(
            status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
            detail="Could not acquire operational lock in time"
        )
```
</details>

### Exercise 5 (Hard): Distributed Lock Simulation with TTL
**Task**: Implement an async class `MemoryDistributedLock` that simulates a distributed lock with an auto-expiring TTL to prevent deadlocks if a task crashes or hangs.
<details>
<summary>Solution</summary>

```python
import asyncio
import time
from typing import Optional

class MemoryDistributedLock:
    def __init__(self):
        self._owner: Optional[str] = None
        self._expires_at: float = 0.0
        self._cond = asyncio.Condition()

    async def acquire(self, client_id: str, ttl_seconds: float = 2.0) -> bool:
        async with self._cond:
            now = time.monotonic()
            while self._owner is not None and now < self._expires_at:
                await self._cond.wait()
                now = time.monotonic()
                
            self._owner = client_id
            self._expires_at = now + ttl_seconds
            return True

    async def release(self, client_id: str) -> None:
        async with self._cond:
            if self._owner == client_id:
                self._owner = None
                self._expires_at = 0.0
                self._cond.notify_all()
```
</details>

### Exercise 6 (Hard): Optimistic Concurrency Control Engine
**Task**: Create a FastAPI route that implements Optimistic Concurrency Control (OCC) with automatic retries for updating a document with a version number. If a version mismatch occurs due to a concurrent write, retry up to 3 times before raising HTTP 409 Conflict.
<details>
<summary>Solution</summary>

```python
import asyncio
from fastapi import FastAPI, HTTPException, status
from pydantic import BaseModel

app = FastAPI()

# Shared database record mock
document_store = {
    "doc_1": {"title": "FastAPI Guide", "version": 1}
}

class UpdatePayload(BaseModel):
    title: str

async def mock_db_update(doc_id: str, new_title: str, expected_version: int) -> bool:
    await asyncio.sleep(0.01)  # Simulate DB latency
    doc = document_store[doc_id]
    if doc["version"] != expected_version:
        return False  # Conflict: another request wrote first
    doc["title"] = new_title
    doc["version"] += 1
    return True

@app.put("/documents/{doc_id}")
async def update_document(doc_id: str, payload: UpdatePayload):
    if doc_id not in document_store:
        raise HTTPException(status_code=404, detail="Not found")
        
    max_retries = 3
    for attempt in range(max_retries):
        current_version = document_store[doc_id]["version"]
        success = await mock_db_update(doc_id, payload.title, current_version)
        if success:
            return {"status": "updated", "version": document_store[doc_id]["version"]}
        # Wait small backoff before retrying
        await asyncio.sleep(0.005 * (attempt + 1))
        
    raise HTTPException(
        status_code=status.HTTP_409_CONFLICT,
        detail="Concurrent modification conflict. Please retry."
    )
```
</details>
