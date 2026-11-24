---
title: "Celery Integration with FastAPI"
category: "10-background-and-messaging"
order: 2
tags: ["fastapi", "celery", "redis", "rabbitmq", "distributed-tasks", "async-workers"]
py_version: "3.12+"
fastapi_version: "0.115+"
starlette_version: "0.38+"
pydantic_version: "2.8+"
tested_with: "Python 3.12, 3.13"
---

# Celery Integration with FastAPI

## 1. Conceptual Foundation & Mental Model

**Celery** is the battle-tested, industry-standard distributed task queue for the Python ecosystem. It allows developers to offload long-running, CPU-intensive, or I/O-heavy jobs away from the synchronous/asynchronous HTTP request-response cycle into dedicated worker pools operating across a fleet of independent machines.

Integrating Celery into FastAPI requires an understanding of the architectural division between:
1. **The Web API Producer (FastAPI)**: Serves incoming client HTTP requests. When a long-running action is requested, FastAPI formats a task signature, serializes its arguments (JSON), and publishes a task message into the **Message Broker** (Redis, RabbitMQ). FastAPI does **not** execute the task; it returns an immediate job identifier (`task_id`) to the client.
2. **The Message Broker**: Stores the serialized task messages in queues, ensuring reliable delivery, consumer acknowledgement, and dead-lettering.
3. **The Worker Consumers (Celery Workers)**: Independent Python processes running on separate servers or containers. They pull tasks from the broker, execute the target functions, and store return values/statuses in a **Result Backend** (Redis, PostgreSQL).

### The Mental Model: The Architect and the Fabrication Plants
- **FastAPI** is a corporate blueprint desk answering phone calls from clients.
- When a client orders a 50-story steel skyscraper (a multi-minute batch data export or video encoding job), the desk clerk does not start welding steel beams in the office.
- The desk clerk stamps a standardized work order with a barcode (`task_id`), drops it into a secure courier box (the **Broker**), and hands the client a tracking receipt (`HTTP 202 Accepted`).
- Outside city limits, three massive industrial steel fabrication plants (**Celery Workers**) read orders from the courier box, forge the steel, and upload status reports to a central tracking ledger (the **Result Backend**).

---

## 2. Architecture & Internal Mechanics

```
   Client Browser / Mobile App
              |
              | 1. POST /jobs/export (HTTP Request)
              v
   +-------------------------------------------------------------+
   | FastAPI Web API (Producer)                                  |
   |                                                             |
   | `task = export_report_task.delay(user_id=42, year=2026)`    |
   | Returns: {"task_id": "c5d1..."}, Status: 202 Accepted       |
   +-------------------------------------------------------------+
              |
              | 2. AMQP / Redis Publish (JSON Message)
              v
   +-------------------------------------------------------------+
   | Message Broker (RabbitMQ / Redis)                           |
   | Queue: `default` [ [Msg 1] [Msg 2] [Msg 3] ]                |
   +-------------------------------------------------------------+
              |
              | 3. Worker Prefetch / Consume (ACK on completion)
              v
   +-------------------------------------------------------------+
   | Celery Worker Node (Consumer Process Pool)                  |
   |                                                             |
   | def export_report_task(user_id, year):                      |
   |     # Heavy SQL query, Pandas aggregation, S3 upload        |
   |     return {"s3_url": "https://s3.amazonaws.com/rep.pdf"}  |
   +-------------------------------------------------------------+
              |
              | 4. Write Final Status & Result Payload
              v
   +-------------------------------------------------------------+
   | Result Backend (Redis / PostgreSQL)                         |
   | Key: `celery-task-meta-c5d1...` -> Status: SUCCESS          |
   +-------------------------------------------------------------+
```

### Important Celery Execution Invariant
Celery tasks are fundamentally **synchronous by default**. When a Celery worker receives a task, it executes inside a worker thread or process (prefork pool). Calling `async def` inside Celery requires wrapping the coroutine in `asyncio.run()`, which creates a fresh event loop per task execution.

---

## 3. Comprehensive Syntax & Core Mechanics

### 1. Celery Application Configuration
```python
from celery import Celery

# Instantiate Celery client
celery_app = Celery(
    "worker_app",
    broker="redis://localhost:6379/0",
    backend="redis://localhost:6379/1"
)

# Production Configuration Settings
celery_app.conf.update(
    task_serializer="json",
    accept_content=["json"],
    result_serializer="json",
    timezone="UTC",
    enable_utc=True,
    task_track_started=True,
    task_time_limit=300,        # Hard kill after 5 minutes (SIGKILL)
    task_soft_time_limit=270,   # Soft warning exception after 4.5 minutes
    worker_prefetch_multiplier=1, # Fair task distribution for long jobs
    task_acks_late=True,        # Only acknowledge message AFTER successful execution
)
```

### 2. Task Declaration
```python
import time
from celery import shared_task

@shared_task(bind=True, max_retries=3, default_retry_delay=10)
def process_data_task(self, data_id: str) -> dict[str, str]:
    try:
        time.sleep(2.0)  # Heavy computation
        return {"status": "completed", "data_id": data_id}
    except Exception as exc:
        # Exponential backoff retry
        raise self.retry(exc=exc, countdown=2 ** self.request.retries)
```

---

## 4. Practical Implementation

### Example 1: Basic FastAPI Task Dispatch and Result Polling
```python
from fastapi import FastAPI, HTTPException, status
from pydantic import BaseModel
from celery.result import AsyncResult
from celery import Celery

celery_client = Celery("tasks", broker="redis://localhost:6379/0", backend="redis://localhost:6379/1")

app = FastAPI()

class TaskPayload(BaseModel):
    account_id: str
    export_format: str

@app.post("/tasks/export", status_code=status.HTTP_202_ACCEPTED)
async def trigger_export_task(payload: TaskPayload) -> dict[str, str]:
    # send_task allows dispatching by string name without importing worker codebase into API
    task = celery_client.send_task(
        "tasks.export_account_data",
        args=[payload.account_id, payload.export_format]
    )
    return {"task_id": task.id, "status": "PENDING"}

@app.get("/tasks/{task_id}")
async def get_task_status(task_id: str) -> dict[str, object]:
    result = AsyncResult(task_id, app=celery_client)
    
    response = {
        "task_id": task_id,
        "state": result.state,
        "ready": result.ready(),
        "successful": result.successful()
    }
    
    if result.ready():
        if result.successful():
            response["result"] = result.result
        else:
            response["error"] = str(result.result)  # Contains exception info
            
    return response
```

### Example 2: Production Asynchronous Database Integration inside Celery
Running async database sessions inside a Celery task requires explicit event loop management:
```python
import asyncio
from celery import shared_task
from sqlalchemy.ext.asyncio import create_async_engine, async_sessionmaker

DATABASE_URL = "postgresql+asyncpg://postgres:secret@localhost:5432/production_db"

async def async_worker_routine(user_id: int) -> dict[str, str]:
    engine = create_async_engine(DATABASE_URL)
    async_session = async_sessionmaker(engine, expire_on_commit=False)
    
    async with async_session() as session:
        # Perform asynchronous database queries
        await asyncio.sleep(0.5)
        
    await engine.dispose()
    return {"user_id": str(user_id), "status": "synced"}

@shared_task(name="tasks.sync_user_record")
def sync_user_record_task(user_id: int) -> dict[str, str]:
    # Safely drive coroutine within synchronous Celery prefork worker
    return asyncio.run(async_worker_routine(user_id))
```

### Example 3: Edge Case — Handling Task Timeouts and Soft Limits
```python
import time
from celery import shared_task
from celery.exceptions import SoftTimeLimitExceeded

@shared_task(bind=True, soft_time_limit=10, time_limit=15)
def fragile_long_computation(self, items_count: int) -> dict[str, str]:
    try:
        for i in range(items_count):
            time.sleep(1.0)  # Heavy work
        return {"status": "SUCCESS"}
    except SoftTimeLimitExceeded:
        # Worker has 5 seconds to cleanly exit, close database handles, and return partial status
        print("Task exceeded soft time limit! Performing emergency cleanup...")
        return {"status": "PARTIAL_TIMEOUT", "processed_before_deadline": True}
```

### Example 4: Failure Case — Passing SQLAlchemy Models Across Task Boundaries
```python
from fastapi import FastAPI
from celery import shared_task

app = FastAPI()

# CATASTROPHIC ANTI-PATTERN
@shared_task
def bad_email_task(user_model):
    # FAILS: user_model is a live SQLAlchemy instance!
    # Pickling error, unmapped instance error, or stale data hazard!
    send_email(user_model.email)

@app.post("/users/bad-dispatch")
async def bad_dispatch(db):
    user = await db.get_user(1)
    # Crashes or leaks open DB handles across network serializer!
    bad_email_task.delay(user)
```

### Example 5: Idiomatic Modern — Task Signature Chaining and Canvas
```python
from celery import chain, chord, group
from fastapi import FastAPI, status

app = FastAPI()

@app.post("/pipelines/process-order", status_code=status.HTTP_202_ACCEPTED)
async def process_order_pipeline(order_id: str):
    # Celery Canvas allows complex orchestration
    # 1. Validate payment -> 2. In parallel: pack items & send invoice -> 3. Dispatch shipment
    pipeline = chain(
        celery_client.signature("tasks.verify_payment", args=[order_id]),
        group(
            celery_client.signature("tasks.reserve_warehouse_stock", args=[order_id]),
            celery_client.signature("tasks.generate_pdf_receipt", args=[order_id])
        ),
        celery_client.signature("tasks.dispatch_carrier", args=[order_id])
    )
    
    async_res = pipeline.apply_async()
    return {"pipeline_id": async_res.id, "status": "PIPELINE_STARTED"}
```

---

## 5. Edge Cases, Pitfalls & Failure Modes

1. **State Desynchronization (Database Lag)**: If FastAPI dispatches a task `process_order.delay(order_id)` *before* the surrounding database transaction has committed, the Celery worker can pick up the task and query the database before the new row is visible, raising `NotFoundError`! **Remedy**: Always dispatch Celery tasks *after* database transactions commit.
2. **Worker Memory Leaks in Prefork Mode**: Python C-extensions (like NumPy, PIL, or TensorFlow) frequently leak memory. If workers run indefinitely, their RSS memory grows until the OS triggers an OOM kill. Use `celery worker --max-tasks-per-child=50` to automatically recycle worker processes after handling 50 tasks.
3. **Deadlocks in Async-to-Sync Bridges**: Calling `asyncio.get_event_loop().run_until_complete()` inside Celery when an event loop is already running or partially closed causes deadlocks. Use `asyncio.run()` with fresh engines.
4. **Result Backend Accumulation**: If tasks return megabytes of data into Redis without setting `result_expires`, Redis memory will climb until full. Configure `result_expires = 3600` (1 hour) or use S3 for large task artifacts.

---

## 6. Common Mistakes & Anti-Patterns

| Anti-Pattern | Root Cause | Consequence | Correct Practice |
|:---|:---|:---|:---|
| Dispatching before DB transaction commits | Calling `.delay()` inside uncommitted transaction | Celery worker cannot find database row | Dispatch task in FastAPI after `await db.commit()` |
| Passing complex objects/ORM models to `.delay()` | Lack of serialization understanding | `EncodeError` / stale database properties | Pass primitive IDs (`user_id: int`) only |
| Running infinite worker processes without recycling | Python C-library memory fragmentation | Worker nodes killed by Linux OOM killer | Set `worker_max_tasks_per_child = 100` |
| Calling `.get()` inside FastAPI route | Blocking the web server for worker result | Defeats asynchronous architecture; freezes request | Return task ID immediately; let client poll or use WebSockets |
| Default `worker_prefetch_multiplier=4` on slow tasks | Fast worker hoards 4 long-running tasks | Other idle workers starve while queue stalls | Set `worker_prefetch_multiplier=1` |
| Storing massive payloads in Redis result backend | Treating Redis as an object storage system | Redis OOM crash | Upload large files to S3/GCS; store only URLs |

---

## 7. Performance & Scalability Considerations

- **Prefetch Multiplier Tuning**: For fast tasks ($<10\text{ms}$), set `prefetch_multiplier=4` to eliminate network roundtrips. For heavy tasks ($>2\text{s}$), set `prefetch_multiplier=1` and `task_acks_late=True` to ensure fair distribution across worker nodes.
- **Worker Concurrency Modes**:
  - `prefork` (multiprocessing): Best for CPU-bound tasks (image processing, data science).
  - `gevent` / `eventlet`: Best for thousands of concurrent synchronous network I/O tasks.
  - `threads`: Good balance for multi-threaded C-libraries (NumPy).

---

## 8. Security & OWASP Guardrails

- **Pickle Vulnerability**: Older Celery configurations defaulted to `pickle` serialization, allowing attackers who compromise Redis to execute arbitrary code on worker nodes. Modern Celery strictly defaults to `json`. Never enable `task_serializer = "pickle"`.
- **Worker Isolation**: Celery workers execute outside the web DMZ. Ensure worker instances run with least-privilege cloud IAM roles.

---

## 9. Testing Strategies & Pytest Recipes

```python
import pytest
from httpx import AsyncClient, ASGITransport
from fastapi import FastAPI
from unittest.mock import patch

app = FastAPI()

@app.post("/trigger-job")
async def trigger_job():
    from celery import Celery
    c = Celery(broker="redis://localhost:6379/0")
    t = c.send_task("tasks.dummy_task", args=[10])
    return {"task_id": t.id}

@pytest.mark.anyio
async def test_celery_task_dispatched_without_running_real_worker():
    # Mock celery task dispatch to prevent requiring real Redis during unit tests
    with patch("celery.Celery.send_task") as mock_send:
        mock_send.return_value.id = "mocked-task-uuid-123"
        
        transport = ASGITransport(app=app)
        async with AsyncClient(transport=transport, base_url="http://test") as client:
            resp = await client.post("/trigger-job")
            assert resp.status_code == 200
            assert resp.json() == {"task_id": "mocked-task-uuid-123"}
            
        mock_send.assert_called_once_with("tasks.dummy_task", args=[10])
```

---

## 10. Real-World Case Study / Production Incident

**The Incident**: A fintech invoice generation service crashed every Monday morning at 9:00 AM. Uvicorn web workers stopped answering health checks, resulting in 502 Bad Gateway errors across the entire application.

**Root Cause**: A developer implemented a PDF download endpoint as:
```python
@app.get("/invoices/{invoice_id}/pdf")
async def download_invoice_pdf(invoice_id: str):
    # ANTI-PATTERN: Synchronously waiting for Celery task in FastAPI!
    task = generate_pdf_task.delay(invoice_id)
    pdf_bytes = task.get(timeout=30)  # BLOCKS THE EVENT LOOP!
    return Response(content=pdf_bytes, media_type="application/pdf")
```
When hundreds of users clicked "Download PDF" on Monday morning, every Uvicorn thread was stuck blocked inside `task.get()`. The entire event loop froze.

**The Fix**:
1. Converted the endpoint to asynchronous status polling:
   - `POST /invoices/{id}/generate` -> Returns `task_id` with `HTTP 202 Accepted`.
   - `GET /invoices/{id}/status` -> Polls `AsyncResult(task_id).state`.
   - `GET /invoices/{id}/download` -> Fetches the generated PDF directly from Amazon S3 once completed.
2. Web API response times plummeted from 8 seconds to 4 milliseconds.

---

## 11. Technical Interview Questions & Answers

<details>
<summary>1. Why should you never call <code>task.get()</code> inside a FastAPI route handler?</summary>
<code>task.get()</code> is a synchronous, blocking call that halts execution until the worker finishes the task. Calling it inside an <code>async def</code> route freezes the single-threaded event loop, preventing all other concurrent requests from being processed. You should return the task ID immediately (HTTP 202) and let the client poll or receive a WebSocket notification.
</details>

<details>
<summary>2. What is the race condition that occurs when dispatching a Celery task inside an uncommitted database transaction?</summary>
If the task message is pushed to the broker before the database transaction commits, a worker can pick up the message and query the database before the transaction completes. Because the database row is not yet committed, the worker fails with a "Row Not Found" error. Tasks should only be dispatched after the transaction has committed.
</details>

<details>
<summary>3. Why is it dangerous to pass SQLAlchemy ORM model instances as arguments to Celery tasks?</summary>
SQLAlchemy instances cannot be serialized cleanly to JSON. They contain open database connections, transaction states, and pointers. Additionally, data might be mutated between enqueue time and execution time. You should always pass primitive identifiers (e.g. <code>user_id: int</code>) and let the worker fetch fresh data.
</details>

<details>
<summary>4. What does Celery's <code>worker_prefetch_multiplier</code> setting control?</summary>
It controls how many task messages each worker process reserves from the broker in advance. For fast tasks, a higher multiplier improves throughput. For long-running, CPU-intensive tasks, setting it to <code>1</code> prevents a busy worker from hoarding tasks while other workers sit idle.
</details>

<details>
<summary>5. How can you execute an asynchronous coroutine inside a Celery task?</summary>
Because Celery workers run synchronously, coroutines must be executed using Python's <code>asyncio.run(my_coroutine())</code> inside the Celery task function. Ensure async database engines are created and disposed cleanly within the task context.
</details>

<details>
<summary>6. What is the difference between <code>soft_time_limit</code> and <code>time_limit</code> in Celery?</summary>
<code>soft_time_limit</code> raises a <code>SoftTimeLimitExceeded</code> exception inside the running task, giving it a chance to catch the error, log progress, and clean up resources. <code>time_limit</code> is a hard deadline: when it expires, the Celery worker process is forcibly killed by the operating system with <code>SIGKILL</code>.
</details>

<details>
<summary>7. How does setting <code>task_acks_late = True</code> improve task reliability?</summary>
By default, Celery acknowledges the message to the broker the moment the task starts executing. If the worker crashes or the node loses power during execution, the task is lost. With <code>task_acks_late = True</code>, the worker only acknowledges the message after execution finishes, ensuring uncompleted tasks are redelivered if the worker dies.
</details>

<details>
<summary>8. Why is Celery often paired with Flower in production?</summary>
Flower is a real-time web monitoring and administration dashboard for Celery. It visualizes active workers, queue lengths, task success/failure rates, execution times, and allows manual revocation or inspection of tasks in production.
</details>

---

## 12. Practical Exercises & Solutions

### Exercise 1 (Easy): Basic Task Dispatch Endpoint
**Task**: Write a FastAPI route `/tasks/send-sms` that dispatches a simulated Celery task `tasks.send_sms` using `send_task` and returns `{"task_id": "...", "status": "QUEUED"}` with status code 202.
<details>
<summary>Solution</summary>

```python
from fastapi import FastAPI, status
from pydantic import BaseModel
from celery import Celery

app = FastAPI()
celery_client = Celery(broker="redis://localhost:6379/0")

class SMSRequest(BaseModel):
    phone_number: str
    message: str

@app.post("/tasks/send-sms", status_code=status.HTTP_202_ACCEPTED)
async def dispatch_sms(payload: SMSRequest):
    task = celery_client.send_task(
        "tasks.send_sms",
        args=[payload.phone_number, payload.message]
    )
    return {"task_id": task.id, "status": "QUEUED"}
```
</details>

### Exercise 2 (Easy): Task Result Status Checker
**Task**: Implement an endpoint `/tasks/{task_id}/status` that inspects Celery's `AsyncResult` and returns whether the task is PENDING, STARTED, SUCCESS, or FAILURE.
<details>
<summary>Solution</summary>

```python
from fastapi import FastAPI
from celery.result import AsyncResult
from celery import Celery

app = FastAPI()
celery_client = Celery(backend="redis://localhost:6379/1")

@app.get("/tasks/{task_id}/status")
async def get_status(task_id: str):
    res = AsyncResult(task_id, app=celery_client)
    return {
        "task_id": task_id,
        "state": res.state,
        "is_ready": res.ready(),
        "result": res.result if res.successful() else None
    }
```
</details>

### Exercise 3 (Medium): Task Revocation Endpoint
**Task**: Build an administrative route `POST /tasks/{task_id}/cancel` that revokes an in-flight Celery task and sends a `terminate=True` signal to kill the task worker if it is currently executing.
<details>
<summary>Solution</summary>

```python
from fastapi import FastAPI
from celery import Celery

app = FastAPI()
celery_client = Celery(broker="redis://localhost:6379/0")

@app.post("/tasks/{task_id}/cancel")
async def cancel_task(task_id: str):
    # Revoke task: prevents execution if pending, or sends SIGTERM if currently running
    celery_client.control.revoke(task_id, terminate=True, signal="SIGTERM")
    return {"task_id": task_id, "action": "REVOKED"}
```
</details>

### Exercise 4 (Medium): Post-Transaction Task Dispatcher Dependency
**Task**: Implement a FastAPI dependency that guarantees a Celery task is only dispatched to the broker after the database transaction has committed cleanly.
<details>
<summary>Solution</summary>

```python
from typing import AsyncIterator
from fastapi import FastAPI, Depends
from celery import Celery

celery_client = Celery(broker="redis://localhost:6379/0")
app = FastAPI()

class TransactionalTaskDispatcher:
    def __init__(self):
        self.pending_tasks = []

    def defer_task(self, name: str, *args, **kwargs):
        self.pending_tasks.append((name, args, kwargs))

    def flush(self):
        for name, args, kwargs in self.pending_tasks:
            celery_client.send_task(name, args=args, kwargs=kwargs)

async def get_dispatcher() -> AsyncIterator[TransactionalTaskDispatcher]:
    dispatcher = TransactionalTaskDispatcher()
    try:
        yield dispatcher
        # In real code: await db.commit() happens before flushing
        dispatcher.flush()
    except Exception:
        # Transaction rolled back: do not dispatch tasks!
        dispatcher.pending_tasks.clear()
        raise

@app.post("/users/checkout")
async def checkout(disp: TransactionalTaskDispatcher = Depends(get_dispatcher)):
    disp.defer_task("tasks.charge_card", customer_id=123, amount=5000)
    return {"status": "checkout_initiated"}
```
</details>

### Exercise 5 (Hard): Celery Task Soft Timeout Fallback Worker
**Task**: Write a complete Celery task definition that catches `SoftTimeLimitExceeded`, flushes partial progress to Redis, and exits cleanly before the hard `time_limit` kills the process.
<details>
<summary>Solution</summary>

```python
import time
from celery import Celery, shared_task
from celery.exceptions import SoftTimeLimitExceeded

celery_app = Celery("workers", broker="redis://localhost:6379/0", backend="redis://localhost:6379/1")

@celery_app.task(bind=True, soft_time_limit=5, time_limit=8)
def resilient_batch_worker(self, total_records: int):
    processed = 0
    try:
        for i in range(total_records):
            time.sleep(1.0)  # Simulate expensive database operation
            processed += 1
        return {"status": "COMPLETED", "processed": processed}
    except SoftTimeLimitExceeded:
        # Clean up database connections, log partial progress
        print(f"Soft limit reached! Saved {processed} records before deadline.")
        return {
            "status": "PARTIAL_SUCCESS",
            "processed": processed,
            "error": "Time limit exceeded; remaining records deferred"
        }
```
</details>

### Exercise 6 (Hard): Celery Canvas Dynamic Chord Aggregator
**Task**: Build a route `/analytics/aggregate` that uses Celery `chord` to process 5 customer accounts in parallel and executes a final callback task that sums their balances into a master report.
<details>
<summary>Solution</summary>

```python
from fastapi import FastAPI, status
from celery import Celery, chord

celery_client = Celery("analytics", broker="redis://localhost:6379/0", backend="redis://localhost:6379/1")
app = FastAPI()

@app.post("/analytics/aggregate", status_code=status.HTTP_202_ACCEPTED)
async def aggregate_analytics():
    account_ids = ["acc_001", "acc_002", "acc_003", "acc_004", "acc_005"]
    
    # 1. Parallel tasks to calculate each account's balance
    header = [
        celery_client.signature("tasks.calculate_balance", args=[acc_id])
        for acc_id in account_ids
    ]
    
    # 2. Callback task that receives list of all 5 balances and computes total
    callback = celery_client.signature("tasks.generate_master_balance_report")
    
    # Dispatch chord
    res = chord(header)(callback)
    return {"chord_id": res.id, "status": "CHORD_DISPATCHED"}
```
</details>
