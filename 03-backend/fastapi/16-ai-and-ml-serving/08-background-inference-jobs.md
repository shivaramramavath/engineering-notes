---
title: "Background Inference Jobs and Asynchronous Processing in FastAPI"
description: "Architecting asynchronous batch AI inference: Celery, ARQ, Redis Streams, job polling, webhooks, progress tracking, and GPU worker isolation."
fastapi_version: "0.115+"
python_version: "3.12+"
free_threading_compatible: true
tested_with: "Python 3.12.8, FastAPI 0.115.6, arq 0.26.1, redis-py 5.2.0"
---

# Background Inference Jobs and Asynchronous Processing in FastAPI

## 1. Conceptual Foundation & Mental Model

While lightweight classification tasks and streaming LLM chat can execute within interactive HTTP request-response cycles, many AI/ML workloads are fundamentally **long-running and non-interactive**. Tasks like fine-tuning a model, transcribing a 2-hour audio file with Whisper, generating high-resolution video with diffusion models, or bulk embedding 50,000 PDF documents take anywhere from several minutes to several hours.

Attempting to process these tasks synchronously within standard HTTP requests will cause:
1. Reverse proxy gateway timeouts (e.g. Nginx/ALB 60s timeout).
2. Browser connection drops.
3. Severe memory and CPU starvation on API worker pods.

The **Asynchronous Job Pattern** decouples the HTTP interface from long-running inference execution:
1. **Immediate Job Acceptance (HTTP 202 Accepted)**: The client submits the heavy inference payload. FastAPI validates the schema, generates a unique `job_id`, pushes a message into an asynchronous queue (Redis Streams, ARQ, Celery, AWS SQS), and immediately returns an HTTP 202 response containing the `job_id` and a polling status URL.
2. **Dedicated Background GPU Workers**: Autonomous worker processes (isolated on GPU-optimized nodes) consume tasks from the queue, execute model computation, and write results to a persistent store (PostgreSQL or Redis).
3. **Status Polling or Webhook Notification**: The client periodically polls `/jobs/{job_id}` for progress updates or provides a `webhook_url` that the worker calls upon completion.

```
[ Client: Uploads 500MB Audio File ]
                 │
                 ▼
     [ FastAPI Ingress API ]
                 │
       ├── Saves audio file to S3: raw/audio_123.mp3
       ├── Enqueues job to Redis: {"job_id": "job_9981", "s3_key": "..."}
       └── Returns HTTP 202 Accepted: {"job_id": "job_9981", "status": "QUEUED"}
                 │
                 ▼
     [ Redis Streams / ARQ Queue ]
                 │
                 ▼ (Dedicated GPU Worker Node)
     [ Autonomous Whisper Worker ]
                 │
       ├── Downloads audio from S3
       ├── Executes Transcribe on NVIDIA GPU (Takes 8 minutes)
       ├── Updates job progress in Redis: 25% -> 50% -> 100%
       └── Saves final transcript to PostgreSQL
                 │
                 ▼
[ Webhook Dispatched to Client OR Client Polls /jobs/job_9981 ]
```

---

## 2. Architecture & Internal Mechanics

```
+---------------------------------------------------------------+
|                      FastAPI Web Tier (CPU Nodes)             |
|  - POST /transcribe -> Enqueues Job -> Returns HTTP 202       |
|  - GET /jobs/{job_id} -> Reads status from Redis/Postgres     |
+-------------------------------+-------------------------------+
                                │ Async Enqueue
                                ▼
+---------------------------------------------------------------+
|                   Message Broker / Redis Streams              |
|  Queue: "inference_jobs" (Persistent, ACK-based delivery)     |
+-------------------------------+-------------------------------+
                                │ Pull Task
                                ▼
+---------------------------------------------------------------+
|                  GPU Worker Cluster (NVIDIA Nodes)            |
|  - Autonomous ARQ / Celery Worker                             |
|  - Pre-warmed model loaded in GPU VRAM                        |
|  - Updates job status & progress in Redis every 10s           |
+-------------------------------+-------------------------------+
                                │ Write Results
                                ▼
+---------------------------------------------------------------+
|              Persistent Storage (PostgreSQL & S3)             |
+---------------------------------------------------------------+
```

---

## 3. Comprehensive Syntax & Core Mechanics

### Setting Up ARQ (Async Redis Queue) for FastAPI
ARQ is an asynchronous job queue for Python built on `asyncio` and `redis-py`. Unlike Celery, ARQ is natively asynchronous, non-blocking, and integrates seamlessly with FastAPI.

```python
# Worker Definition: worker.py
import asyncio
from arq.connections import RedisSettings

async def run_audio_transcription(ctx, job_id: str, s3_key: str):
    redis = ctx["redis"]
    print(f"Starting heavy inference for job {job_id}...")

    for progress in [25, 50, 75, 100]:
        await asyncio.sleep(2.0) # Simulated heavy GPU processing
        await redis.set(f"job:{job_id}:progress", progress)

    result = f"Transcribed text for {s3_key}"
    await redis.set(f"job:{job_id}:status", "COMPLETED")
    await redis.set(f"job:{job_id}:result", result)
    return result

class WorkerSettings:
    functions = [run_audio_transcription]
    redis_settings = RedisSettings(host="localhost", port=6379)
```

---

## 4. Practical Implementation: 5 Production Scenarios

### Scenario 1: Complete Asynchronous Job Dispatch and Polling API
```python
from fastapi import FastAPI, HTTPException, status
from pydantic import BaseModel
from arq import create_pool
from arq.connections import RedisSettings
from contextlib import asynccontextmanager
import uuid

arq_pool = None

@asynccontextmanager
async def lifespan(app: FastAPI):
    global arq_pool
    arq_pool = await create_pool(RedisSettings(host="localhost", port=6379))
    yield
    await arq_pool.close()

app = FastAPI(lifespan=lifespan)

class InferenceRequest(BaseModel):
    document_url: str

class JobCreationResponse(BaseModel):
    job_id: str
    status: str
    status_url: str

@app.post("/jobs/transcribe", status_code=status.HTTP_202_ACCEPTED, response_model=JobCreationResponse)
async def submit_transcription_job(payload: InferenceRequest):
    job_id = str(uuid.uuid4())

    # Dispatch job to ARQ background worker
    await arq_pool.enqueue_job(
        "run_audio_transcription",
        job_id=job_id,
        s3_key=payload.document_url,
        _job_id=job_id
    )

    # Initialize job state in Redis
    await arq_pool.set(f"job:{job_id}:status", "QUEUED")
    await arq_pool.set(f"job:{job_id}:progress", 0)

    return JobCreationResponse(
        job_id=job_id,
        status="QUEUED",
        status_url=f"/jobs/{job_id}/status"
    )

@app.get("/jobs/{job_id}/status")
async def get_job_status(job_id: str):
    job_status = await arq_pool.get(f"job:{job_id}:status")
    if not job_status:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="Job ID not found")

    progress = await arq_pool.get(f"job:{job_id}:progress")
    result = await arq_pool.get(f"job:{job_id}:result")

    return {
        "job_id": job_id,
        "status": job_status.decode("utf-8"),
        "progress_percent": int(progress) if progress else 0,
        "result": result.decode("utf-8") if result else None
    }
```

### Scenario 2: Webhook Dispatcher upon Inference Completion
```python
import httpx
import orjson
import asyncio

async def notify_client_webhook(webhook_url: str, job_id: str, result_payload: dict):
    # Retry webhook up to 3 times with exponential backoff
    async with httpx.AsyncClient(timeout=10.0) as client:
        payload = {
            "event": "INFERENCE_COMPLETED",
            "job_id": job_id,
            "data": result_payload
        }
        for attempt in range(1, 4):
            try:
                resp = await client.post(
                    webhook_url,
                    content=orjson.dumps(payload),
                    headers={"Content-Type": "application/json"}
                )
                if resp.status_code in [200, 201, 204]:
                    print(f"Webhook delivered for job {job_id}")
                    return
            except Exception as e:
                print(f"Webhook delivery attempt {attempt} failed: {e}")
            await asyncio.sleep(2 ** attempt)
```

### Scenario 3: Real-Time Job Progress Streaming via Server-Sent Events (SSE)
```python
from fastapi import FastAPI, Request
from fastapi.responses import StreamingResponse
import asyncio
import redis.asyncio as aioredis
import orjson

app = FastAPI()
redis = aioredis.from_url("redis://localhost:6379/0")

async def progress_stream(job_id: str, request: Request):
    while True:
        if await request.is_disconnected():
            break

        status = await redis.get(f"job:{job_id}:status")
        progress = await redis.get(f"job:{job_id}:progress")

        data = {
            "job_id": job_id,
            "status": status.decode() if status else "UNKNOWN",
            "progress": int(progress) if progress else 0
        }
        yield f"data: {orjson.dumps(data).decode()}\n\n"

        if status and status.decode() in ["COMPLETED", "FAILED"]:
            break

        await asyncio.sleep(1.0)

@app.get("/jobs/{job_id}/stream")
async def stream_job_progress(job_id: str, request: Request):
    return StreamingResponse(progress_stream(job_id, request), media_type="text/event-stream")
```

### Scenario 4: Dead Letter Queue (DLQ) for Poison Pill Inference Tasks
```python
async def handle_failed_inference_job(redis, job_id: str, error_message: str):
    # Move failed job metadata to Dead Letter Queue (DLQ) for manual inspection
    dlq_record = {
        "job_id": job_id,
        "error": error_message,
        "failed_at": time.time()
    }
    await redis.lpush("dlq:inference_failures", orjson.dumps(dlq_record))
    await redis.set(f"job:{job_id}:status", "FAILED")
```

### Scenario 5: GPU Node Horizontal Autoscaling (KEDA with Redis Queue Depth)
```yaml
# KEDA ScaledObject: Dynamically scale GPU worker pods based on queue length
apiVersion: keda.sh/v1alpha1
kind: ScaledObject
metadata:
  name: gpu-inference-scaler
  namespace: ml-workloads
spec:
  scaleTargetRef:
    name: whisper-gpu-workers
  minReplicaCount: 0  # Scale down to zero when queue is empty (Save GPU costs!)
  maxReplicaCount: 10 # Scale up to 10 GPU instances under heavy backlog
  triggers:
    - type: redis
      metadata:
        address: redis-cluster.internal:6379
        listName: arq:queue
        listLength: "5" # Add 1 worker pod for every 5 queued inference jobs
```

---

## 5. Edge Cases, Pitfalls & Failure Modes

1. **Worker OOM-Crash Leaving Jobs in Zombie "IN_PROGRESS" State**:
   If a GPU worker runs out of VRAM while processing job 123, the operating system kills the worker process (`SIGKILL`). The job remains indefinitely marked as `IN_PROGRESS` in Redis. Workers must maintain heartbeats, and a watchdog task must mark jobs whose heartbeats have expired as `FAILED` or re-enqueue them.

2. **Poison Pill Tasks (Repeatedly Crashing Workers)**:
   A malformed audio file or corrupted tensor that triggers a CUDA segmentation fault will crash the worker. If the queue retries the task indefinitely, the task will crash every worker in the cluster in sequence. Enforce maximum retry limits (e.g. `max_tries = 3`) and move failing tasks to a Dead Letter Queue.

3. **Client Polling DDOS on Status Endpoints**:
   If 10,000 clients submit jobs and poll `/jobs/{id}/status` every 50 milliseconds, the polling traffic will overwhelm the API and database. Enforce a minimum polling interval (e.g. 2-5 seconds), return `Retry-After: 5` response headers, or migrate to SSE / Webhooks.

4. **Redis Memory Exhaustion from Stored Inference Results**:
   Storing multi-megabyte inference results (e.g. high-resolution images or large JSON arrays) directly in Redis keys will exhaust Redis RAM. Store large result artifacts in object storage (Amazon S3) and store only the S3 URL in Redis.

---

## 6. Anti-Patterns & Common Mistakes

| Anti-Pattern | Operational Impact | Correct Pattern |
| :--- | :--- | :--- |
| Running 3-minute inference tasks in standard HTTP routes | Gateway timeouts (504) and client connection drops | Return HTTP 202 Accepted and offload to queue |
| Storing large generated files (audio/video) in Redis keys | Redis memory exhaustion and slow replication | Upload results to S3; store only S3 URLs in Redis |
| Polling without exponential backoff or minimum interval | Clients overwhelm API with thousands of status requests | Set `Retry-After: 5` header or use SSE/Webhooks |
| Unbounded job retries on worker segmentation faults | Poison pill tasks crash all GPU workers in cluster | Enforce `max_tries=3` and route failures to DLQ |
| Running GPU models on same pods as FastAPI web server | Inference CPU/GPU spikes freeze web request handling | Separate web API nodes (CPU) from worker nodes (GPU) |
| Zombie jobs on worker failure | Tasks remain permanently stuck in "PROCESSING" state | Implement worker heartbeats with auto-reclamation |

---

## 7. Performance & Scalability Considerations

- **Scale to Zero (KEDA)**: GPU instances (like AWS `g5.xlarge` or `p4d.24xlarge`) are expensive ($1 to $32 per hour). Deploying KEDA (Kubernetes Event-driven Autoscaling) allows worker deployments to scale down to **0 replicas** when the job queue is empty, cutting infrastructure costs by 70-90% during idle night hours.
- **Persistent Volume Caching**: Mount shared network storage (EFS/FSx) on worker nodes so that model weights are loaded locally from disk rather than downloaded over the internet on pod scale-up.

---

## 8. Security & OWASP Guardrails

- **Webhook SSRF Mitigation**: When calling client-supplied `webhook_url` endpoints, validate the URL against private IP blacklists (RFC 1918 addresses: `10.0.0.0/8`, `192.168.0.0/16`, `127.0.0.1`, AWS metadata `169.254.169.254`) to prevent Server-Side Request Forgery.
- **HMAC Webhook Signatures**: Sign outgoing webhook payloads using HMAC-SHA256 (`X-Hub-Signature-256`) so clients can cryptographically verify that notifications originated from your service.

---

## 9. Testing Strategies & Pytest Recipes

```python
import pytest
from httpx import AsyncClient, ASGITransport
from main import app

@pytest.mark.asyncio
async def test_job_submission_and_status():
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        # 1. Submit job
        submit_resp = await client.post(
            "/jobs/transcribe",
            json={"document_url": "s3://bucket/test.mp3"}
        )
        assert submit_resp.status_code == 202
        data = submit_resp.json()
        assert "job_id" in data
        assert data["status"] == "QUEUED"

        # 2. Check initial status
        job_id = data["job_id"]
        status_resp = await client.get(f"/jobs/{job_id}/status")
        assert status_resp.status_code == 200
        assert status_resp.json()["job_id"] == job_id
```

---

## 10. Real-World Production Incident

**Incident**: A generative AI video rendering service experienced a complete outage when its Redis queue ran out of memory, causing the entire API to crash with `OOM command not allowed when used memory > 'maxmemory'`.

**Root Cause**: The engineering team stored completed video rendering outputs (averaging 45MB each) directly in Redis string keys:
```python
# Fatal Flaw in worker:
await redis.set(f"job:{job_id}:result", raw_video_bytes)
```
After processing 2,000 video jobs, Redis memory consumption reached 100% of the instance's 8GB RAM, rejecting all new write commands and halting job ingestion.

**Resolution**: Updated workers to stream generated video files directly to Amazon S3 and store only the resulting S3 presigned URL in Redis. Flushed the bloated Redis keys. Memory utilization dropped from 8GB to 14MB.

---

## 11. Technical Interview Questions

<details>
<summary>1. When should an API endpoint return HTTP 202 Accepted instead of processing synchronously?</summary>

An endpoint should return HTTP 202 Accepted when the requested task cannot be completed within an interactive HTTP request duration (typically operations exceeding 2 to 5 seconds). Examples include video rendering, large audio transcription, fine-tuning ML models, and bulk document indexing. The server accepts the payload, enqueues the job, and provides a tracking ID, allowing the client to poll or await webhooks while preventing gateway timeouts.
</details>

<details>
<summary>2. What is ARQ and how does it differ from Celery for FastAPI applications?</summary>

- **Celery**: A mature distributed task queue designed primarily for synchronous Python. Celery tasks are synchronous functions running on multiprocessing pools. Integrating with async FastAPI often requires bridging sync and async boundaries.
- **ARQ**: A modern task queue built natively on Python's `asyncio` and `redis-py`. Worker functions are native coroutines (`async def`), allowing workers to manage thousands of concurrent I/O operations with minimal memory overhead, perfectly matching FastAPI's asynchronous architecture.
</details>

<details>
<summary>3. What is a "Poison Pill" task in a distributed background queue and how do you protect against it?</summary>

A poison pill task is a malformed or adversarial task payload that consistently causes worker processes to crash (e.g. triggering an unhandled C++ segmentation fault or memory allocation panic). When the worker dies, the queue manager detects failure and re-enqueues the task, which promptly crashes the next worker, causing a cluster-wide cascading failure. Mitigations include enforcing a strict retry limit (e.g. `max_tries = 3`) and moving failed tasks to a Dead Letter Queue (DLQ).
</details>

<details>
<summary>4. What are "Zombie Jobs" and how do worker heartbeats prevent them?</summary>

A zombie job occurs when a worker claims a job and marks it as `IN_PROGRESS`, but the worker pod is forcefully killed (e.g. node preemption, hardware failure, or SIGKILL) before it can update the job status. The job remains permanently stuck in `IN_PROGRESS`. Worker heartbeats require active workers to periodically update a timestamp in Redis (`job:{id}:heartbeat`). If a watchdog detects that a heartbeat has expired, it reclaims the job and reschedules it.
</details>

<details>
<summary>5. How does KEDA (Kubernetes Event-driven Autoscaling) scale GPU workers efficiently?</summary>

KEDA monitors queue depth metrics (e.g. number of pending messages in a Redis list or AWS SQS queue). When the queue length is zero, KEDA scales the GPU worker Deployment down to 0 replicas, eliminating expensive cloud GPU idle costs. When tasks are enqueued, KEDA dynamically scales up worker pods proportionally to queue backlog.
</details>

<details>
<summary>6. Why should you avoid storing raw binary inference outputs directly in Redis?</summary>

Redis is an in-memory key-value store optimized for high-speed metadata and cache operations. Storing large binary files (e.g. 50MB audio/video files) consumes expensive volatile RAM, increases garbage collection and snapshotting overhead, and risks triggering Redis out-of-memory eviction policies. Large artifacts should always be stored in object storage (Amazon S3), storing only the lightweight URI in Redis.
</details>

<details>
<summary>7. How do you prevent SSRF attacks when dispatching webhooks to user-provided URLs?</summary>

When an application calls user-provided webhook URLs:
1. Resolve the hostname to an IP address.
2. Verify that the IP address does not belong to private or loopback ranges (`10.0.0.0/8`, `172.16.0.0/12`, `192.168.0.0/16`, `127.0.0.1`, `169.254.169.254`).
3. Disallow HTTP redirects to private IPs.
4. Execute outbound requests through an egress proxy with strict firewall rules.
</details>

<details>
<summary>8. What is the advantage of using Server-Sent Events (SSE) over HTTP polling for job status tracking?</summary>

With HTTP polling, the client repeatedly opens and closes HTTP connections (e.g. every second), generating high network overhead and server CPU load even when status has not changed. With SSE, a single connection remains open, and the server pushes updates only when progress increments or state changes, reducing network traffic and latency.
</details>

---

## 12. Practical Exercises & Solutions

### Exercise 1 (Easy): Basic HTTP 202 Accepted Response
**Task**: Build a FastAPI endpoint `/jobs/start` that accepts a payload, generates a UUID `job_id`, and returns HTTP 202 with status `QUEUED`.
<details>
<summary>Solution</summary>

```python
import uuid
from fastapi import FastAPI, status
from pydantic import BaseModel

app = FastAPI()

class JobResponse(BaseModel):
    job_id: str
    status: str

@app.post("/jobs/start", status_code=status.HTTP_202_ACCEPTED, response_model=JobResponse)
async def start_job():
    job_id = str(uuid.uuid4())
    return JobResponse(job_id=job_id, status="QUEUED")
```
</details>

### Exercise 2 (Easy): Redis Job Progress Setter
**Task**: Write an async helper function that updates a job's progress percentage in Redis with a 24-hour expiration time.
<details>
<summary>Solution</summary>

```python
import redis.asyncio as aioredis

async def update_job_progress(redis: aioredis.Redis, job_id: str, progress: int):
    key = f"job:{job_id}:progress"
    await redis.set(key, progress, ex=86400)
```
</details>

### Exercise 3 (Medium): Safe Webhook URL Validator
**Task**: Write a validator function that checks if a webhook URL attempts to target local or private network IP ranges.
<details>
<summary>Solution</summary>

```python
import ipaddress
import socket
from urllib.parse import urlparse

def is_safe_webhook_url(url: str) -> bool:
    parsed = urlparse(url)
    if parsed.scheme not in ["http", "https"]:
        return False
    hostname = parsed.hostname
    if not hostname:
        return False

    try:
        ip = ipaddress.ip_address(socket.gethostbyname(hostname))
        if ip.is_private or ip.is_loopback or ip.is_link_local:
            return False
        return True
    except Exception:
        return False

assert is_safe_webhook_url("https://example.com/webhook") is True
assert is_safe_webhook_url("http://127.0.0.1/hook") is False
assert is_safe_webhook_url("http://169.254.169.254/latest") is False
```
</details>

### Exercise 4 (Medium): Polling Status Endpoint with Retry-After Header
**Task**: Build a route `/jobs/{id}` that returns HTTP 200 if completed, or returns HTTP 200 with a `Retry-After: 5` header if still in progress.
<details>
<summary>Solution</summary>

```python
from fastapi import FastAPI, Response
import redis.asyncio as aioredis

app = FastAPI()
redis = aioredis.from_url("redis://localhost:6379/0")

@app.get("/jobs/{job_id}")
async def check_job(job_id: str, response: Response):
    status = await redis.get(f"job:{job_id}:status")
    status_str = status.decode() if status else "PENDING"

    if status_str != "COMPLETED":
        # Instruct client to wait 5 seconds before polling again
        response.headers["Retry-After"] = "5"

    return {"job_id": job_id, "status": status_str}
```
</details>

### Exercise 5 (Hard): Complete ARQ Asynchronous Job Dispatch Pipeline
**Task**: Write a complete FastAPI application that connects to ARQ in `lifespan`, provides a job submission endpoint returning HTTP 202, and a status check endpoint.
<details>
<summary>Solution</summary>

```python
import uuid
from contextlib import asynccontextmanager
from fastapi import FastAPI, HTTPException, status
from pydantic import BaseModel
from arq import create_pool
from arq.connections import RedisSettings

arq_pool = None

@asynccontextmanager
async def lifespan(app: FastAPI):
    global arq_pool
    arq_pool = await create_pool(RedisSettings())
    yield
    await arq_pool.close()

app = FastAPI(lifespan=lifespan)

class TaskInput(BaseModel):
    prompt: str

@app.post("/tasks/render", status_code=status.HTTP_202_ACCEPTED)
async def submit_render_task(data: TaskInput):
    job_id = str(uuid.uuid4())
    await arq_pool.enqueue_job("render_video_coro", prompt=data.prompt, _job_id=job_id)
    await arq_pool.set(f"job:{job_id}:status", "QUEUED")
    return {"job_id": job_id, "status": "QUEUED"}

@app.get("/tasks/{job_id}")
async def get_task_status(job_id: str):
    stat = await arq_pool.get(f"job:{job_id}:status")
    if not stat:
        raise HTTPException(status_code=404, detail="Task not found")
    return {"job_id": job_id, "status": stat.decode()}
```
</details>

### Exercise 6 (Hard): HMAC-Signed Webhook Dispatcher
**Task**: Build a Python function that sends an HTTP POST webhook containing an HMAC-SHA256 signature in the `X-Hub-Signature-256` header using a shared secret key.
<details>
<summary>Solution</summary>

```python
import hmac
import hashlib
import httpx
import orjson

async def dispatch_signed_webhook(target_url: str, secret_key: str, payload: dict):
    raw_body = orjson.dumps(payload)
    # Compute HMAC SHA256 digest
    signature = hmac.new(
        secret_key.encode("utf-8"),
        raw_body,
        hashlib.sha256
    ).hexdigest()

    headers = {
        "Content-Type": "application/json",
        "X-Hub-Signature-256": f"sha256={signature}"
    }

    async with httpx.AsyncClient(timeout=5.0) as client:
        resp = await client.post(target_url, content=raw_body, headers=headers)
        return resp.status_code
```
</details>
