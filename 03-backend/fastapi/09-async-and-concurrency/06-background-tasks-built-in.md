---
title: "Built-in Background Tasks in FastAPI"
category: "09-async-and-concurrency"
order: 6
tags: ["fastapi", "background-tasks", "starlette", "fire-and-forget", "asyncio"]
py_version: "3.12+"
fastapi_version: "0.115+"
starlette_version: "0.38+"
pydantic_version: "2.8+"
tested_with: "Python 3.12, 3.13"
---

# Built-in Background Tasks in FastAPI

## 1. Conceptual Foundation & Mental Model

In web API development, many operations must occur as a consequence of an HTTP request, but do not produce data required by the immediate HTTP response:
- Sending a welcome or password reset email
- Writing an audit trail log to an external logging service
- Invalidating a cached CDN key
- Updating user last-seen timestamps

Making the client wait for these secondary operations wastes client time and degrades p99 latency. 

FastAPI provides a built-in `BackgroundTasks` class (inherited directly from Starlette). Unlike full distributed task queues (Celery, ARQ, Dramatiq, Temporal), FastAPI's `BackgroundTasks` runs **in-process**, executing immediately *after* the HTTP response has been sent back to the client over the network wire.

### The Mental Model: The Fast Food Drive-Through
Imagine ordering food at a drive-through window:
- You pay and the clerk immediately hands you your hot meal and receipt (the HTTP Response).
- As you drive away, the clerk turns around, logs the transaction in the ledger, and restocks napkins in the holder (the `BackgroundTask`).
- You (the client) didn't have to wait 5 extra minutes idling your engine for the clerk to restock napkins.
- **Limitation**: If the building catches fire (server crash/SIGKILL) right after you drive away, the napkins were never restocked. In-process background tasks lack durable disk/broker persistence!

---

## 2. Architecture & Internal Mechanics

```
   Client HTTP Request
           |
           v
   FastAPI Endpoint
   [ Adds task via: background_tasks.add_task(send_email, email) ]
           |
           v
   Response Serialized & Sent to Client (HTTP 200 OK)
   [ Client receives response immediately! ]
           |
           v
   ASGI Transport Completes
           |
           v
   Starlette BackgroundTask Runner
           |
           +-----------------------+-----------------------+
           |                                               |
     Is `async def`?                                 Is sync `def`?
           |                                               |
          YES                                             YES
           |                                               |
           v                                               v
   Awaited directly on                            Dispatched to AnyIO
   Main Event Loop:                               Worker Threadpool:
   `await task()`                                 `run_in_threadpool(task)`
           |                                               |
           +-----------------------+-----------------------+
                                   |
                         Execution Completes
```

### In-Process Execution Mechanics
When you add a background task:
1. `BackgroundTasks` simply appends a tuple of `(callable, args, kwargs)` to an internal list.
2. FastAPI generates the HTTP response and writes it to the ASGI `send` callable.
3. Once the response payload is successfully flushed over the TCP socket, Starlette's `Response.__call__` executes the background tasks sequentially.
4. If a background task is a normal `def`, it runs in the AnyIO threadpool. If it is `async def`, it runs directly on the event loop.

---

## 3. Comprehensive Syntax & Core Mechanics

### 1. Declaring and Adding Tasks
```python
from fastapi import FastAPI, BackgroundTasks

app = FastAPI()

def send_notification_sync(email: str, message: str) -> None:
    # Runs in threadpool after response
    print(f"Sent sync email to {email}: {message}")

async def send_audit_log_async(user_id: int, action: str) -> None:
    # Runs on event loop after response
    print(f"Logged audit async: user={user_id} action={action}")

@app.post("/register")
async def register(email: str, background_tasks: BackgroundTasks):
    # Enqueue tasks to run post-response
    background_tasks.add_task(send_notification_sync, email, "Welcome!")
    background_tasks.add_task(send_audit_log_async, user_id=42, action="USER_REGISTERED")
    
    # Response is returned immediately
    return {"message": "Registration successful"}
```

### 2. Adding Tasks from Dependencies
`BackgroundTasks` can also be injected into FastAPI dependencies:
```python
from fastapi import FastAPI, Depends, BackgroundTasks

app = FastAPI()

def track_request_metric(endpoint: str):
    print(f"Metric logged for {endpoint}")

def audit_dependency(bg: BackgroundTasks):
    bg.add_task(track_request_metric, "/items")

@app.get("/items", dependencies=[Depends(audit_dependency)])
async def list_items():
    return [{"id": 1, "name": "Hammer"}]
```

---

## 4. Practical Implementation

### Example 1: Basic Email Dispatcher
```python
import asyncio
from fastapi import FastAPI, BackgroundTasks
from pydantic import BaseModel, EmailStr

app = FastAPI()

class UserRegisterSchema(BaseModel):
    username: str
    email: EmailStr

async def send_welcome_email(email: str, username: str):
    # Simulate network latency of sending email
    await asyncio.sleep(1.0)
    print(f"Email delivered to {username} <{email}>")

@app.post("/signup")
async def signup(user: UserRegisterSchema, bg: BackgroundTasks):
    bg.add_task(send_welcome_email, user.email, user.username)
    return {"status": "created", "note": "Welcome email will arrive shortly"}
```

### Example 2: Production Audit Trail Logger with Error Protection
```python
import logging
from typing import Any
from fastapi import FastAPI, BackgroundTasks, Request
from pydantic import BaseModel

logger = logging.getLogger("audit")
app = FastAPI()

class OrderCreate(BaseModel):
    order_id: str
    amount: float

def write_audit_record(record: dict[str, Any]) -> None:
    try:
        # Simulate writing to audit log file or external Splunk/Datadog agent
        logger.info("AUDIT_LOG: %s", record)
    except Exception as exc:
        logger.error("Failed to write audit record: %s", exc)

@app.post("/orders")
async def create_order(order: OrderCreate, request: Request, bg: BackgroundTasks):
    # Process business logic
    order_record = {
        "order_id": order.order_id,
        "amount": order.amount,
        "client_ip": request.client.host if request.client else "unknown",
        "action": "CREATE_ORDER"
    }
    
    # Enqueue audit task
    bg.add_task(write_audit_record, order_record)
    
    return {"status": "processed", "order_id": order.order_id}
```

### Example 3: Edge Case — Chaining Multiple Dependent Background Tasks
```python
import asyncio
from fastapi import FastAPI, BackgroundTasks

app = FastAPI()

async def pipeline_step_one(job_id: str) -> None:
    await asyncio.sleep(0.1)
    print(f"Job {job_id}: Step 1 finished")

async def pipeline_step_two(job_id: str) -> None:
    await asyncio.sleep(0.1)
    print(f"Job {job_id}: Step 2 finished")

async def run_pipeline(job_id: str):
    await pipeline_step_one(job_id)
    await pipeline_step_two(job_id)

@app.post("/jobs/trigger")
async def trigger_job(job_id: str, bg: BackgroundTasks):
    bg.add_task(run_pipeline, job_id)
    return {"job_id": job_id, "status": "queued_in_process"}
```

### Example 4: Failure Case — Fatal Loop Starvation in Background Task
```python
import time
from fastapi import FastAPI, BackgroundTasks

app = FastAPI()

# CATASTROPHIC ANTI-PATTERN
async def bad_background_worker():
    # Calling blocking time.sleep inside async def blocks the loop!
    # Even though this runs AFTER the response is sent, it freezes the entire
    # server from answering the NEXT request!
    time.sleep(5.0)

@app.post("/bad-bg")
async def bad_bg(bg: BackgroundTasks):
    bg.add_task(bad_background_worker)
    return {"status": "enqueued"}
```

### Example 5: Idiomatic Modern — Response Object Background Task Assignment
You can also attach background tasks directly to a `Response` instance:
```python
from fastapi import FastAPI, Response
from fastapi.responses import JSONResponse
from starlette.background import BackgroundTask

app = FastAPI()

def cleanup_session_file(session_id: str):
    print(f"Cleaning up temp files for session: {session_id}")

@app.get("/export")
async def export_data(session_id: str) -> Response:
    # Single background task directly bound to Response
    task = BackgroundTask(cleanup_session_file, session_id=session_id)
    return JSONResponse(
        content={"data": "report_contents"},
        background=task
    )
```

---

## 5. Edge Cases, Pitfalls & Failure Modes

1. **Process Crash Loss (No Durability)**: If the host machine loses power, the Kubernetes pod gets evicted (OOMKilled), or Uvicorn restarts, all pending in-memory background tasks are lost forever without recovery. **Never use `BackgroundTasks` for financial transactions or mission-critical notifications.**
2. **Sequential Execution of Tasks**: If you add 5 tasks via `bg.add_task()`, FastAPI executes them **sequentially**, not concurrently. If Task 1 takes 10 seconds, Task 2 will not start until Task 1 completes.
3. **Loop Blocking in `async def` Tasks**: Declaring background tasks as `async def` still binds them to the main event loop. Blocking calls inside them will freeze the server for incoming requests.
4. **Unhandled Exceptions in Background Tasks**: If a background task raises an unhandled exception, Starlette catches and logs it to `stderr`, but the client will not know because the HTTP 200 response was already sent. Always wrap background tasks in `try / except` blocks.

---

## 6. Common Mistakes & Anti-Patterns

| Anti-Pattern | Root Cause | Impact | Correct Practice |
|:---|:---|:---|:---|
| Using `BackgroundTasks` for video encoding or ML | In-process execution of heavy CPU work | Chokes web server memory and CPU | Use a distributed queue (Celery, ARQ, Temporal) |
| Using `BackgroundTasks` for payments | Lack of persistence or retries | Data loss on pod restarts | Use persistent message brokers (RabbitMQ, SQS) |
| Adding slow sync calls in `async def` task | Blocking loop post-response | Freezes server from handling subsequent requests | Use standard `def` for sync tasks or `to_thread.run_sync` |
| Assuming tasks run concurrently | Misunderstanding sequential list iteration | Total execution time is additive | Wrap in `asyncio.TaskGroup` if concurrency is needed |
| Missing error handling in background tasks | Assuming background tasks cannot fail | Silent failures without alerting | Implement explicit logging and alerting |
| Passing mutable request state to tasks | Request scope is destroyed post-response | Race conditions or stale references | Pass primitive values or copies |

---

## 7. Performance & Scalability Considerations

- **Zero Serialization Overhead**: Unlike Celery/Redis, FastAPI's `BackgroundTasks` requires no JSON serialization, Redis network roundtrip, or broker polling, making it nearly instantaneous ($<10\mu s$ overhead).
- **Memory Footprint**: Pending tasks live in RAM. If an endpoint receives 10,000 req/sec and enqueues tasks faster than they complete, memory will grow until the process crashes.
- **Worker Concurrency**: If background tasks are declared as standard `def`, they share the AnyIO threadpool with sync endpoints.

---

## 8. Security & OWASP Guardrails

- **Resource Exhaustion via Unbounded Enqueueing**: Malicious users flooding an endpoint can cause thousands of background tasks to stack up, consuming server memory. Enforce rate limiting.
- **Leaking Sensitive Context**: Do not pass sensitive objects (like raw database connection handles or HTTP session contexts) into background tasks if they are subject to closure or disposal.

---

## 9. Testing Strategies & Pytest Recipes

```python
import pytest
from httpx import AsyncClient, ASGITransport
from fastapi import FastAPI, BackgroundTasks

app = FastAPI()
task_executed = {"ran": False}

def dummy_background_job():
    task_executed["ran"] = True

@app.post("/trigger")
async def trigger(bg: BackgroundTasks):
    bg.add_task(dummy_background_job)
    return {"status": "ok"}

@pytest.mark.anyio
async def test_background_task_executes_in_test():
    task_executed["ran"] = False
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        res = await client.post("/trigger")
        assert res.status_code == 200
        assert res.json() == {"status": "ok"}
        
    # With ASGITransport, the background task completes before the client context exits
    assert task_executed["ran"] is True
```

---

## 10. Real-World Case Study / Production Incident

**The Incident**: A newsletter startup used FastAPI's `BackgroundTasks` to send email blasts to 25,000 subscribers when an author clicked "Publish". The API responded with `HTTP 200 Published` in 5ms. However, 30 seconds later, the Kubernetes node was terminated due to a scheduled cluster upgrade. Only 400 of the 25,000 emails were sent. The remaining 24,600 emails vanished without an audit trail, and subscribers never received the newsletter.

**The Fix**:
1. Recognized that `BackgroundTasks` is designed for ephemeral, lightweight side effects (like updating cache or metrics), not reliable multi-minute batch jobs.
2. Replaced the in-process task with an event dispatch to Amazon SQS / Celery:
   ```python
   @app.post("/publish")
   async def publish_newsletter(newsletter_id: int):
       # Push message to durable queue broker
       await sqs_client.send_message(QueueUrl=QUEUE_URL, MessageBody=str(newsletter_id))
       return {"status": "published"}
   ```

---

## 11. Technical Interview Questions & Answers

<details>
<summary>1. When does a FastAPI <code>BackgroundTask</code> execute?</summary>
It executes immediately <b>after</b> the HTTP response has been sent to the client over the network wire. The client does not wait for the background task to complete.
</details>

<details>
<summary>2. What is the fundamental difference between FastAPI <code>BackgroundTasks</code> and a task queue like Celery or ARQ?</summary>
FastAPI <code>BackgroundTasks</code> runs <b>in-process</b> in memory on the same machine and worker process serving the request. It has no durability, retries, or broker persistence: if the server restarts or crashes, pending tasks are permanently lost. Celery/ARQ use out-of-process distributed workers with durable brokers (Redis, RabbitMQ).
</details>

<details>
<summary>3. If you add three background tasks to <code>BackgroundTasks</code>, do they run concurrently or sequentially?</summary>
They run <b>sequentially</b> in the order they were added. Starlette iterates over the registered tasks one by one.
</details>

<details>
<summary>4. How does FastAPI execute a background task declared as standard <code>def</code> vs <code>async def</code>?</summary>
An <code>async def</code> background task is awaited directly on the main event loop thread. A standard <code>def</code> background task is dispatched to the AnyIO worker threadpool.
</details>

<details>
<summary>5. What happens if a background task raises an unhandled exception?</summary>
Because the HTTP response has already been sent to the client, the client does not receive an HTTP 500 error. Starlette catches the exception and logs it to <code>stderr</code>, but execution of subsequent background tasks in that same request may be interrupted depending on where the failure occurred.
</details>

<details>
<summary>6. Can you inject <code>BackgroundTasks</code> into a FastAPI dependency?</summary>
Yes. You can declare <code>def my_dep(bg: BackgroundTasks): bg.add_task(...)</code>. FastAPI shares the same <code>BackgroundTasks</code> instance across dependencies and the route handler.
</details>

<details>
<summary>7. How do you attach a single background task to a custom <code>Response</code> object?</summary>
Instantiate <code>starlette.background.BackgroundTask(func, *args, **kwargs)</code> and pass it to the response constructor: <code>Response(content=..., background=task)</code>.
</details>

<details>
<summary>8. Why should you avoid executing heavy CPU tasks (e.g., video transcoding) in <code>BackgroundTasks</code>?</summary>
Because it runs inside the same process space as your API server. CPU-intensive operations will monopolize CPU cores, causing request processing latency to spike for all API clients.
</details>

---

## 12. Practical Exercises & Solutions

### Exercise 1 (Easy): Basic Post-Response Metric Counter
**Task**: Build an endpoint `/vote` that increments a global in-memory vote counter inside a background task, returning the vote confirmation immediately.
<details>
<summary>Solution</summary>

```python
from fastapi import FastAPI, BackgroundTasks

app = FastAPI()
vote_stats = {"total_votes": 0}

def increment_votes():
    vote_stats["total_votes"] += 1

@app.post("/vote")
async def cast_vote(bg: BackgroundTasks):
    bg.add_task(increment_votes)
    return {"message": "Vote accepted"}

@app.get("/votes")
async def get_votes():
    return vote_stats
```
</details>

### Exercise 2 (Easy): Multi-Task Registration
**Task**: Create a user signup endpoint that enqueues two separate background tasks: sending a welcome notification and recording an analytics sign-up event.
<details>
<summary>Solution</summary>

```python
from fastapi import FastAPI, BackgroundTasks

app = FastAPI()

def send_welcome(email: str):
    print(f"Sending email to {email}")

def log_analytics(email: str):
    print(f"Analytics: User signed up with {email}")

@app.post("/register")
async def register(email: str, bg: BackgroundTasks):
    bg.add_task(send_welcome, email)
    bg.add_task(log_analytics, email)
    return {"status": "success"}
```
</details>

### Exercise 3 (Medium): Temporary File Cleaner Response
**Task**: Build an endpoint `/download-temp` that creates a temporary file on disk, streams it as a `FileResponse`, and deletes the temporary file in a background task after the download finishes.
<details>
<summary>Solution</summary>

```python
import os
import tempfile
from fastapi import FastAPI
from fastapi.responses import FileResponse
from starlette.background import BackgroundTask

app = FastAPI()

def remove_temp_file(path: str):
    if os.path.exists(path):
        os.remove(path)
        print(f"Removed temp file: {path}")

@app.get("/download-temp")
async def download_temp():
    # Create temp file
    temp = tempfile.NamedTemporaryFile(delete=False, mode="w", suffix=".txt")
    temp.write("Temporary payload data...")
    temp.close()
    
    # Clean up file post-response
    bg_task = BackgroundTask(remove_temp_file, temp.name)
    return FileResponse(
        path=temp.name,
        filename="export.txt",
        background=bg_task
    )
```
</details>

### Exercise 4 (Medium): Dependency-Driven Background Audit
**Task**: Create a FastAPI dependency `audit_logger` that automatically inspects the request method and path, enqueuing a background task to write the audit trail without requiring the route function to interact with `BackgroundTasks`.
<details>
<summary>Solution</summary>

```python
from fastapi import FastAPI, Request, Depends, BackgroundTasks

app = FastAPI()

def persist_audit_event(method: str, path: str):
    print(f"AUDIT TRAIL: {method} {path}")

def audit_logger(request: Request, bg: BackgroundTasks):
    bg.add_task(persist_audit_event, request.method, request.url.path)

@app.get("/secure-data", dependencies=[Depends(audit_logger)])
async def get_secure_data():
    return {"secret": "42"}
```
</details>

### Exercise 5 (Hard): Resilient Background Task with Retries
**Task**: Implement an async background task helper `resilient_background_task(coro_fn, max_retries=3)` that executes an async function with exponential backoff if it fails, logging errors appropriately.
<details>
<summary>Solution</summary>

```python
import asyncio
import logging
from typing import Callable, Any, Coroutine
from fastapi import FastAPI, BackgroundTasks

logger = logging.getLogger("resilient_task")
app = FastAPI()

async def run_with_retries(coro_fn: Callable[..., Coroutine[Any, Any, None]], *args: Any, max_retries: int = 3):
    for attempt in range(1, max_retries + 1):
        try:
            await coro_fn(*args)
            logger.info("Task succeeded on attempt %d", attempt)
            return
        except Exception as exc:
            logger.warning("Attempt %d failed: %s", attempt, exc)
            if attempt == max_retries:
                logger.error("Task failed permanently after %d retries.", max_retries)
                return
            await asyncio.sleep(2 ** attempt * 0.1)

async def unreliable_webhook(url: str):
    # Simulated failure
    raise ConnectionError(f"Could not reach {url}")

@app.post("/trigger-webhook")
async def trigger_webhook(bg: BackgroundTasks):
    bg.add_task(run_with_retries, unreliable_webhook, "https://webhook.site/abc", max_retries=3)
    return {"status": "enqueued_with_retry"}
```
</details>

### Exercise 6 (Hard): Concurrent Background Task Group Execution
**Task**: Because `BackgroundTasks` runs tasks sequentially by default, build a background task runner that takes a list of coroutines and runs them **concurrently** using `asyncio.TaskGroup` inside a single background task wrapper.
<details>
<summary>Solution</summary>

```python
import asyncio
from typing import Coroutine, Any
from fastapi import FastAPI, BackgroundTasks

app = FastAPI()

async def run_concurrently(coroutines: list[Coroutine[Any, Any, Any]]):
    async with asyncio.TaskGroup() as tg:
        for coro in coroutines:
            tg.create_task(coro)

async def send_sms(phone: str):
    await asyncio.sleep(0.05)
    print(f"SMS sent to {phone}")

async def push_notification(device_id: str):
    await asyncio.sleep(0.05)
    print(f"Push notification sent to {device_id}")

@app.post("/notify-all")
async def notify_all(bg: BackgroundTasks):
    # Enqueue a single background task that executes both notifications concurrently
    coros = [send_sms("+15551234"), push_notification("dev_99")]
    bg.add_task(run_concurrently, coros)
    return {"status": "dispatched"}
```
</details>
