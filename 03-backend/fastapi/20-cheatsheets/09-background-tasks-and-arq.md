# Quick Reference: Background Tasks & ARQ Queueing Cheatsheet

## In-Process BackgroundTasks (Starlette)

```python
from fastapi import BackgroundTasks, FastAPI

app = FastAPI()

def send_welcome_email(email: str, name: str):
    # Lightweight, fire-and-forget in-process operation
    print(f"Sent email to {email}")

@app.post("/register")
async def register_user(email: str, name: str, background_tasks: BackgroundTasks):
    # Add non-blocking in-process task to be executed after response is sent
    background_tasks.add_task(send_welcome_email, email=email, name=name)
    return {"message": "User registered, email will be sent"}
```

---

## Distributed Task Queue (ARQ with Redis)

### 1. Worker Definition (`worker.py`)
```python
from arq.connections import RedisSettings

async def send_heavy_report(ctx, user_id: str, report_type: str):
    # Executed inside dedicated ARQ worker process
    redis = ctx["redis"]
    # Perform intensive task...
    return f"Report {report_type} generated for {user_id}"

async def startup(ctx):
    # Initialize shared clients (e.g. httpx, DB pools)
    pass

async def shutdown(ctx):
    pass

class WorkerSettings:
    functions = [send_heavy_report]
    on_startup = startup
    on_shutdown = shutdown
    redis_settings = RedisSettings(host="localhost", port=6379)
    max_jobs = 20
```

### 2. FastAPI Enqueue Endpoint (`main.py`)
```python
from arq import create_pool
from arq.connections import RedisSettings
from fastapi import FastAPI

app = FastAPI()
redis_settings = RedisSettings(host="localhost", port=6379)

@app.post("/reports/{user_id}")
async def enqueue_report(user_id: str):
    pool = await create_pool(redis_settings)
    job = await pool.enqueue_job("send_heavy_report", user_id=user_id, report_type="ANNUAL")
    await pool.aclose()
    return {"job_id": job.job_id, "status": "QUEUED"}
```

---

## ARQ CLI Execution

```bash
# Start background worker process
arq worker.WorkerSettings

# Run with custom burst mode (terminates when queue is empty)
arq worker.WorkerSettings --burst
```
