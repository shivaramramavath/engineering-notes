---
title: "TaskGroups and Structured Concurrency in FastAPI"
category: "09-async-and-concurrency"
order: 5
tags: ["fastapi", "taskgroup", "structured-concurrency", "asyncio", "python-311", "exceptiongroup"]
py_version: "3.12+"
fastapi_version: "0.115+"
starlette_version: "0.38+"
pydantic_version: "2.8+"
tested_with: "Python 3.12, 3.13"
---

# TaskGroups and Structured Concurrency in FastAPI

## 1. Conceptual Foundation & Mental Model

Historically, concurrent programming in Python via `asyncio.gather()` or unbounded `asyncio.create_task()` suffered from the classic flaws of **unstructured concurrency**. When multiple concurrent tasks are spawned independently, their lifetimes are dissociated from the lexical scope that launched them. If one task crashes, the other tasks continue running as orphaned background zombies, leaking memory, sockets, and database connections.

Python 3.11 introduced **Structured Concurrency** via `asyncio.TaskGroup` (heavily inspired by Trio and Martin Sústrik's structured concurrency thesis). Structured concurrency enforces a strict invariant:
> *If a block of code spawns concurrent tasks, the block cannot exit until all child tasks have completed, succeeded, or been cancelled.*

### The Mental Model: The Parent-Child Field Trip
- In **Unstructured Concurrency (`asyncio.create_task`)**, a teacher (parent function) releases 30 schoolchildren (tasks) into an amusement park and leaves the park without waiting. If child #5 breaks an arm (raises an exception), the teacher leaves anyway. The other 29 children wander the park unsupervised forever (orphaned tasks leaking resources).
- In **Structured Concurrency (`asyncio.TaskGroup`)**, the teacher enters the amusement park through a secured turnstile (`async with asyncio.TaskGroup() as tg:`). The turnstile gate physically locks. 
  - All children must return to the gate before the teacher is permitted to leave.
  - If child #5 breaks an arm, an emergency alarm sounds: the park instantly recalls and cancels the other 29 children (`task.cancel()`). Once all children have assembled back at the gate, the teacher exits and reports the emergency report (`ExceptionGroup`).

---

## 2. Architecture & Internal Mechanics

```
               Enter: `async with asyncio.TaskGroup() as tg:`
                                     |
               +---------------------+---------------------+
               |                     |                     |
     tg.create_task(coro_1) tg.create_task(coro_2) tg.create_task(coro_3)
               |                     |                     |
            RUNNING               CRASHES!              RUNNING
               |                     |                     |
               |             Raises Exception              |
               |                     |                     |
               |             TaskGroup Catches             |
               |                     |                     |
               +<----+---------------+---------------+---->+
                     |                               |
                     v                               v
             Signals Cancel                  Signals Cancel
             (Task 1 Cancelled)              (Task 3 Cancelled)
                     |                               |
                     +---------------+---------------+
                                     |
                         Waits for all to settle
                                     |
                         Collects All Exceptions
                                     |
                                     v
                          Raises `ExceptionGroup`
```

### ExceptionGroup and `except*` Syntax
When one or more tasks fail inside a `TaskGroup`, Python packages the errors into an `ExceptionGroup`. Python 3.11+ provides the `except*` syntax to catch specific error types out of the group without unpacking them manually:
```python
try:
    async with asyncio.TaskGroup() as tg:
        tg.create_task(task_a())
        tg.create_task(task_b())
except* ValueError as eg:
    print(f"Handled ValueErrors: {eg.exceptions}")
except* httpx.HTTPError as eg:
    print(f"Handled Network Errors: {eg.exceptions}")
```

---

## 3. Comprehensive Syntax & Core Mechanics

### 1. Basic TaskGroup Usage
```python
import asyncio
from typing import Any

async def fetch_user(uid: int) -> dict[str, Any]:
    await asyncio.sleep(0.02)
    return {"id": uid, "name": f"User {uid}"}

async def fetch_orders(uid: int) -> list[str]:
    await asyncio.sleep(0.03)
    return ["order_101", "order_102"]

async def load_profile(uid: int) -> dict[str, Any]:
    # Both tasks run concurrently within structured bounds
    async with asyncio.TaskGroup() as tg:
        t_user = tg.create_task(fetch_user(uid))
        t_orders = tg.create_task(fetch_orders(uid))

    # Here, both tasks are 100% guaranteed to be finished
    return {
        "user": t_user.result(),
        "orders": t_orders.result()
    }
```

### 2. Timeouts with `asyncio.timeout`
Structured concurrency pairs naturally with scoped timeouts:
```python
import asyncio

async def bounded_workflow():
    try:
        # Strict 1.5s deadline across all tasks in the group
        async with asyncio.timeout(1.5):
            async with asyncio.TaskGroup() as tg:
                tg.create_task(asyncio.sleep(0.5))
                tg.create_task(asyncio.sleep(0.8))
    except TimeoutError:
        print("Workflow exceeded 1.5s deadline! All child tasks cancelled cleanly.")
```

---

## 4. Practical Implementation

### Example 1: Basic Concurrent Service Aggregator
```python
import asyncio
from fastapi import FastAPI, HTTPException

app = FastAPI()

async def get_exchange_rate(pair: str) -> float:
    await asyncio.sleep(0.02)
    return 1.085

async def get_fee_percentage(user_id: int) -> float:
    await asyncio.sleep(0.01)
    return 0.015

@app.get("/checkout/preview")
async def checkout_preview(user_id: int, pair: str = "EURUSD") -> dict[str, float]:
    async with asyncio.TaskGroup() as tg:
        rate_task = tg.create_task(get_exchange_rate(pair))
        fee_task = tg.create_task(get_fee_percentage(user_id))

    return {
        "pair_rate": rate_task.result(),
        "fee_pct": fee_task.result()
    }
```

### Example 2: Production Multi-Cloud Health Checker
```python
import asyncio
import httpx
from fastapi import FastAPI, status
from fastapi.responses import JSONResponse

app = FastAPI()

async def check_endpoint(client: httpx.AsyncClient, name: str, url: str) -> dict[str, str]:
    try:
        resp = await client.get(url, timeout=2.0)
        return {"service": name, "status": "UP" if resp.status_code == 200 else "DEGRADED"}
    except Exception as exc:
        return {"service": name, "status": "DOWN", "error": str(exc)}

@app.get("/health/deep")
async def deep_health_check() -> JSONResponse:
    services = {
        "auth_provider": "https://auth.internal.corp/healthz",
        "billing_engine": "https://billing.internal.corp/healthz",
        "search_index": "https://search.internal.corp/healthz",
    }
    
    async with httpx.AsyncClient() as client:
        tasks = {}
        async with asyncio.TaskGroup() as tg:
            for name, url in services.items():
                tasks[name] = tg.create_task(check_endpoint(client, name, url))
                
        results = {name: task.result() for name, task in tasks.items()}
        all_up = all(r["status"] == "UP" for r in results.values())
        
        return JSONResponse(
            status_code=status.HTTP_200_OK if all_up else status.HTTP_503_SERVICE_UNAVAILABLE,
            content={"healthy": all_up, "details": results}
        )
```

### Example 3: Edge Case — Handling Partial Failures and ExceptionGroup in FastAPI
```python
import asyncio
from fastapi import FastAPI, HTTPException, status

app = FastAPI()

async def fetch_mandatory_auth():
    await asyncio.sleep(0.01)
    return {"token": "valid"}

async def fetch_flaky_recommendations():
    await asyncio.sleep(0.02)
    raise ConnectionResetError("Recommendation engine connection dropped")

@app.get("/user/dashboard")
async def user_dashboard():
    try:
        async with asyncio.TaskGroup() as tg:
            auth_task = tg.create_task(fetch_mandatory_auth())
            rec_task = tg.create_task(fetch_flaky_recommendations())
    except* ConnectionResetError:
        # Structured handling of specific grouped error
        raise HTTPException(
            status_code=status.HTTP_502_BAD_GATEWAY,
            detail="Recommendation subsystem failed"
        )
    except* Exception as eg:
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Unhandled internal errors: {eg.exceptions}"
        )

    return {"auth": auth_task.result(), "recs": rec_task.result()}
```

### Example 4: Failure Case — `asyncio.gather` Leaking Orphan Tasks
```python
import asyncio
from fastapi import FastAPI

app = FastAPI()

# ANTI-PATTERN: gather without return_exceptions=True leaks running coroutines!
async def slow_writer():
    try:
        await asyncio.sleep(10.0)
        print("SLOW WRITER FINISHED!")  # Runs even after client receives 500!
    except asyncio.CancelledError:
        print("SLOW WRITER CLEANED UP")

async def exploding_task():
    await asyncio.sleep(0.01)
    raise ValueError("Immediate disaster!")

@app.get("/gather-leak")
async def gather_leak():
    # When exploding_task fails, gather raises ValueError IMMEDIATELY.
    # But slow_writer is NOT cancelled! It keeps running in the background!
    await asyncio.gather(slow_writer(), exploding_task())
    return {"status": "never reached"}
```

### Example 5: Idiomatic Modern — Safe Isolation Pattern
```python
import asyncio
from typing import Any
from fastapi import FastAPI

app = FastAPI()

async def resilient_child(coro: Any, fallback: Any) -> Any:
    try:
        return await coro
    except Exception as exc:
        return fallback

@app.get("/safe-aggregate")
async def safe_aggregate():
    # By wrapping children in resilient error boundaries, TaskGroup guarantees completion
    async with asyncio.TaskGroup() as tg:
        t1 = tg.create_task(resilient_child(asyncio.sleep(0.02), fallback=None))
        t2 = tg.create_task(resilient_child(asyncio.sleep(0.01), fallback="cached_default"))

    return {"t1": t1.result(), "t2": t2.result()}
```

---

## 5. Edge Cases, Pitfalls & Failure Modes

1. **Accessing `.result()` Before TaskGroup Exits**: Calling `task.result()` inside the `async with asyncio.TaskGroup()` block will raise `asyncio.InvalidStateError: Result is not ready.` Results can only be accessed after exiting the context manager.
2. **Double Raising of ExceptionGroup**: If you re-raise inside an `except*` block, a new filtered `ExceptionGroup` containing the unhandled exceptions is propagated.
3. **TaskGroup is Not Reusable**: Once an `asyncio.TaskGroup` context manager exits, you cannot call `tg.create_task()` on it again. It raises `RuntimeError: TaskGroup is closed`.
4. **Cancellation Cascades**: If one child task raises an exception, ALL other child tasks in that TaskGroup are immediately sent `task.cancel()`. If those child tasks do not catch `asyncio.CancelledError` or take too long to handle cancellation, the TaskGroup waits for their shutdown.

---

## 6. Common Mistakes & Anti-Patterns

| Anti-Pattern | Root Cause | Impact | Correct Practice |
|:---|:---|:---|:---|
| Calling `task.result()` inside `async with tg:` | Trying to read result before task finishes | `InvalidStateError: Result is not ready` | Read `.result()` only after the context manager block |
| Using `asyncio.gather()` for critical pipelines | Legacy habit from Python 3.7 | Orphan tasks run unmonitored after failures | Migrate to `asyncio.TaskGroup()` |
| Swallowing all errors with raw `except Exception:` | Hiding TaskGroup failure details | Hides multiple simultaneous task errors | Use Python 3.11 `except*` syntax |
| Nesting TaskGroups without timeouts | Unbounded task trees | Single hanging task hangs entire HTTP request | Pair `TaskGroup` with `asyncio.timeout()` |
| Manually calling `task.cancel()` on siblings | Re-implementing TaskGroup logic | Fragile manual state coordination | Let `TaskGroup` cancel siblings automatically |
| Reusing a `TaskGroup` instance across requests | Misunderstanding lifecycle scope | `RuntimeError: TaskGroup is closed` | Create a fresh `TaskGroup` per request or operation |

---

## 7. Performance & Scalability Considerations

- **Task Allocation Speed**: Creating tasks inside a `TaskGroup` via `tg.create_task()` has essentially zero overhead compared to `asyncio.create_task()`, but provides deterministic cleanup guarantees.
- **ExceptionGroup Handling**: In high-throughput error scenarios, building `ExceptionGroup` hierarchies creates tracebacks for each failed task. Ensure exception handling filters out routine validation errors before they escalate to task cancellations.

---

## 8. Security & OWASP Guardrails

- **Slowloris Upstream Amplification**: If your FastAPI route spawns 10 concurrent requests to upstream third-party microservices using a TaskGroup, and one third-party service hangs, your client connection stays open. Always wrap the entire `TaskGroup` in `asyncio.timeout(deadline_seconds)`.

---

## 9. Testing Strategies & Pytest Recipes

```python
import pytest
import asyncio
from httpx import AsyncClient, ASGITransport
from fastapi import FastAPI, HTTPException

app = FastAPI()

async def subtask_ok():
    return "ok"

async def subtask_fail():
    raise ValueError("Subtask crashed")

@app.get("/task-group-endpoint")
async def task_group_endpoint():
    try:
        async with asyncio.TaskGroup() as tg:
            t1 = tg.create_task(subtask_ok())
            t2 = tg.create_task(subtask_fail())
    except* ValueError:
        raise HTTPException(status_code=400, detail="Subtask validation failed")
    return {"t1": t1.result()}

@pytest.mark.anyio
async def test_task_group_exception_mapped_cleanly():
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        resp = await client.get("/task-group-endpoint")
        assert resp.status_code == 400
        assert resp.json() == {"detail": "Subtask validation failed"}
```

---

## 10. Real-World Case Study / Production Incident

**The Incident**: A travel booking API utilized `asyncio.gather(*hotel_queries)` to query 15 flight and hotel vendors concurrently. During an internet transit routing outage, 2 of the 15 vendors took 45 seconds to timeout. Although `asyncio.gather` had an outer timeout, when the timeout triggered, the other 13 vendor queries were not cancelled. They continued consuming outbound sockets, exhausting the host's Linux ephemeral port range (`TIME_WAIT` socket exhaustion), resulting in 100% API downtime.

**The Fix**:
Migrated from `gather` to `asyncio.TaskGroup` wrapped in `asyncio.timeout`:
```python
async with asyncio.timeout(2.5):
    async with asyncio.TaskGroup() as tg:
        tasks = [tg.create_task(query_vendor(v)) for v in vendors]
```
When the 2.5s timeout fired, `TaskGroup` automatically sent cancellation signals to all 15 vendor queries immediately, releasing all outbound TCP sockets instantly and preventing socket exhaustion.

---

## 11. Technical Interview Questions & Answers

<details>
<summary>1. What is Structured Concurrency, and how is it implemented in Python 3.11+?</summary>
Structured Concurrency is a programming paradigm where concurrent tasks have clear entry and exit points strictly bounded by lexical code blocks. In Python 3.11+, it is implemented via <code>asyncio.TaskGroup</code>. The context manager cannot exit until all child tasks finish. If any child task raises an exception, all other active child tasks in the group are immediately cancelled.
</details>

<details>
<summary>2. Why does <code>asyncio.gather()</code> violate the principles of structured concurrency?</summary>
With <code>asyncio.gather()</code>, if one task raises an exception, <code>gather</code> immediately returns that exception to the caller, leaving all other running tasks executing in the background unmonitored. This leads to orphaned tasks and resource leaks.
</details>

<details>
<summary>3. What is an <code>ExceptionGroup</code> and how does <code>except*</code> handle it?</summary>
An <code>ExceptionGroup</code> is a composite exception introduced in Python 3.11 that bundles multiple concurrent exceptions together. The <code>except*</code> syntax allows handling specific exception types contained within the group without catching the entire group manually.
</details>

<details>
<summary>4. What happens if you try to call <code>task.result()</code> before the <code>async with asyncio.TaskGroup()</code> block exits?</summary>
It raises <code>asyncio.InvalidStateError</code> because tasks inside the TaskGroup are still executing or scheduled. Tasks are only guaranteed to have completed once the TaskGroup context manager exits cleanly.
</details>

<details>
<summary>5. Can you use <code>asyncio.TaskGroup</code> with <code>asyncio.timeout</code>?</summary>
Yes. Wrapping an <code>async with asyncio.TaskGroup()</code> inside an <code>async with asyncio.timeout(seconds)</code> applies a unified deadline across all tasks in the group. If the deadline expires, the entire task group is cleanly cancelled.
</details>

<details>
<summary>6. If one task fails in a TaskGroup, does it wait for the other tasks to cancel before raising?</summary>
Yes. The TaskGroup issues cancellation requests to all sibling tasks and then waits for all tasks to cleanly exit (handling their cancellation) before bundling the resulting exceptions into an <code>ExceptionGroup</code> and raising it.
</details>

<details>
<summary>7. How do you implement a fallback strategy in a TaskGroup without crashing the group?</summary>
Wrap the child coroutine in a helper function that catches exceptions and returns a default value or error object, preventing unhandled exceptions from reaching the TaskGroup boundary.
</details>

<details>
<summary>8. How does TaskGroup behave if a child task catches <code>asyncio.CancelledError</code> and ignores it?</summary>
If a child task catches <code>CancelledError</code> and does not re-raise it or exit, the TaskGroup will hang indefinitely at the end of its context block waiting for that task to terminate.
</details>

---

## 12. Practical Exercises & Solutions

### Exercise 1 (Easy): Basic Concurrent Pair Evaluator
**Task**: Write an async route `/sum-metrics` that launches two mock data fetchers concurrently using `asyncio.TaskGroup` and returns their sum.
<details>
<summary>Solution</summary>

```python
import asyncio
from fastapi import FastAPI

app = FastAPI()

async def fetch_metric_a() -> int:
    await asyncio.sleep(0.01)
    return 150

async def fetch_metric_b() -> int:
    await asyncio.sleep(0.01)
    return 250

@app.get("/sum-metrics")
async def sum_metrics():
    async with asyncio.TaskGroup() as tg:
        t_a = tg.create_task(fetch_metric_a())
        t_b = tg.create_task(fetch_metric_b())
        
    return {"total": t_a.result() + t_b.result()}
```
</details>

### Exercise 2 (Easy): Deadline Enforced TaskGroup
**Task**: Implement an endpoint `/fetch-with-deadline` that runs three concurrent tasks with a strict 0.5s timeout. If the timeout triggers, return HTTP 504.
<details>
<summary>Solution</summary>

```python
import asyncio
from fastapi import FastAPI, HTTPException, status

app = FastAPI()

async def quick_task():
    await asyncio.sleep(0.1)
    return "fast"

async def slow_task():
    await asyncio.sleep(1.0)
    return "slow"

@app.get("/fetch-with-deadline")
async def fetch_with_deadline():
    try:
        async with asyncio.timeout(0.5):
            async with asyncio.TaskGroup() as tg:
                t1 = tg.create_task(quick_task())
                t2 = tg.create_task(slow_task())
    except TimeoutError:
        raise HTTPException(
            status_code=status.HTTP_504_GATEWAY_TIMEOUT,
            detail="Processing exceeded 0.5 second deadline"
        )
    return {"res": t1.result()}
```
</details>

### Exercise 3 (Medium): Fan-Out Fan-In Search Aggregator
**Task**: Build a search aggregator `/search` that queries 5 independent data providers in parallel with `TaskGroup`. Use an isolation wrapper so that if 2 providers fail, the remaining 3 return their results cleanly.
<details>
<summary>Solution</summary>

```python
import asyncio
from typing import Any
from fastapi import FastAPI

app = FastAPI()

async def safe_provider_query(provider_id: int) -> dict[str, Any]:
    try:
        if provider_id == 3:
            raise RuntimeError("Provider 3 offline")
        await asyncio.sleep(0.02)
        return {"provider": provider_id, "items": [f"item_{provider_id}_A", f"item_{provider_id}_B"]}
    except Exception as exc:
        return {"provider": provider_id, "error": str(exc), "items": []}

@app.get("/search")
async def search():
    tasks = []
    async with asyncio.TaskGroup() as tg:
        for i in range(1, 6):
            tasks.append(tg.create_task(safe_provider_query(i)))
            
    results = [t.result() for t in tasks]
    return {"results": results}
```
</details>

### Exercise 4 (Medium): Custom `except*` Error Mapper
**Task**: Build an async route that handles an `ExceptionGroup` containing both `FileNotFoundError` and `PermissionError`, mapping each to appropriate HTTP status codes (404 and 403).
<details>
<summary>Solution</summary>

```python
import asyncio
from fastapi import FastAPI, HTTPException

app = FastAPI()

async def verify_file_access():
    raise FileNotFoundError("Target document not found")

@app.get("/document/check")
async def check_doc():
    try:
        async with asyncio.TaskGroup() as tg:
            tg.create_task(verify_file_access())
    except* FileNotFoundError:
        raise HTTPException(status_code=404, detail="Document not found")
    except* PermissionError:
        raise HTTPException(status_code=403, detail="Access denied")
    return {"status": "ok"}
```
</details>

### Exercise 5 (Hard): Dynamic Worker Pool with TaskGroup
**Task**: Implement a batch processing endpoint `/batch-process` that takes a list of 50 item IDs, and processes them in concurrent batches of 10 using `TaskGroup` until all are completed.
<details>
<summary>Solution</summary>

```python
import asyncio
from fastapi import FastAPI

app = FastAPI()

async def process_single_item(item_id: int) -> dict[str, int]:
    await asyncio.sleep(0.02)
    return {"id": item_id, "processed": item_id * 10}

@app.post("/batch-process")
async def batch_process(items: list[int]):
    batch_size = 10
    all_results = []
    
    for i in range(0, len(items), batch_size):
        chunk = items[i:i + batch_size]
        async with asyncio.TaskGroup() as tg:
            tasks = [tg.create_task(process_single_item(item_id)) for item_id in chunk]
        all_results.extend([t.result() for t in tasks])
        
    return {"total_processed": len(all_results), "items": all_results}
```
</details>

### Exercise 6 (Hard): Cancellation-Aware Cleanup Guard
**Task**: Create an asynchronous background task that performs a multi-step database migration. If a parent TaskGroup cancels the task midway, it must catch `asyncio.CancelledError`, execute a clean rollback, and re-raise.
<details>
<summary>Solution</summary>

```python
import asyncio
from fastapi import FastAPI, HTTPException

app = FastAPI()

class DatabaseMigrationWorker:
    def __init__(self):
        self.rolled_back = False

    async def run_migration(self):
        try:
            print("Migration Step 1: Schema applied")
            await asyncio.sleep(0.05)
            print("Migration Step 2: Data backfilled")
            await asyncio.sleep(0.5)  # Will be cancelled here
            print("Migration Step 3: Indexes built")
        except asyncio.CancelledError:
            print("Task cancelled! Initiating emergency database rollback...")
            self.rolled_back = True
            # Re-raise to honor structured concurrency cancellation contract
            raise

async def faulty_sibling():
    await asyncio.sleep(0.02)
    raise RuntimeError("Storage disk corrupted!")

@app.post("/run-migration")
async def run_migration():
    worker = DatabaseMigrationWorker()
    try:
        async with asyncio.TaskGroup() as tg:
            tg.create_task(worker.run_migration())
            tg.create_task(faulty_sibling())
    except* RuntimeError:
        return {
            "status": "aborted",
            "rolled_back": worker.rolled_back
        }
```
</details>
