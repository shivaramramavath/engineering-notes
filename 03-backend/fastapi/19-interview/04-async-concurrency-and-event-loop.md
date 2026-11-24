# Technical Interview Mastery: Asynchronous Concurrency & Event Loop Mechanics

## Overview & Evaluation Criteria

Understanding Python's `asyncio` event loop, cooperative multitasking, task scheduling, CPU vs. I/O bottlenecks, and event loop blocking is essential for high-throughput backend engineering.

---

## Technical Q&A Deep Dive

<details>
<summary><strong>Q1: What actually happens when you accidentally call a blocking synchronous library (like <code>requests.get()</code> or <code>time.sleep()</code>) inside an <code>async def</code> route in FastAPI?</strong></summary>

### Answer:
- An `async def` function runs **directly on the main ASGI event loop thread**.
- When `time.sleep(5)` or `requests.get()` runs, it performs a blocking syscall on the thread executing the event loop.
- **Catastrophic Consequence**:
  - The event loop **completely halts**. It cannot schedule any other ready coroutines, process incoming HTTP sockets, or service pending timers.
  - All concurrent users across all endpoints experience a complete freeze for that duration.
- **Remedy**:
  1. Replace with asynchronous libraries: `httpx.AsyncClient` or `asyncio.sleep()`.
  2. If a blocking third-party library must be used, offload it explicitly:
     ```python
     result = await asyncio.to_thread(blocking_sync_function, arg1)
     ```
  3. Or declare the endpoint as synchronous `def` so FastAPI automatically offloads it to the worker thread pool.

</details>

<details>
<summary><strong>Q2: Compare <code>asyncio.gather()</code> vs <code>asyncio.TaskGroup()</code> (Python 3.11+). Why is <code>TaskGroup</code> preferred for production concurrency?</strong></summary>

### Answer:
- **`asyncio.gather(*tasks)`**:
  - Launches multiple coroutines concurrently.
  - **Flaw (Structured Concurrency Violation)**: If one coroutine raises an exception, other running coroutines in `gather` continue executing in the background ("dangling / orphaned tasks"), leaking resources or performing partial side-effects.
- **`asyncio.TaskGroup()`**:
  - Implements **Structured Concurrency**.
  - If any task inside the `async with asyncio.TaskGroup() as tg:` block raises an exception, the task group automatically cancels all other running tasks inside the group and waits for their termination before re-raising an `ExceptionGroup`.
  - Guarantees zero orphaned background tasks.

</details>

<details>
<summary><strong>Q3: How does Python 3.13 Free-Threading (PEP 703 / nogil) change FastAPI's concurrency model?</strong></summary>

### Answer:
- In standard CPython, the Global Interpreter Lock (GIL) permits only one OS thread to execute Python bytecode at a time, limiting true parallel CPU scaling.
- Under **Free-Threading (`python3.13t`)**:
  - The GIL is completely disabled.
  - Pure Python CPU-bound calculations run in parallel across all CPU cores when using multi-threading.
  - Thread-pool offloaded operations (`asyncio.to_thread` / `def` routes) achieve genuine multi-core hardware scaling without needing separate OS child processes (`workers`).
- **Caveat**: C-extensions and third-party libraries must be verified as thread-safe under free-threading.

</details>

<details>
<summary><strong>Q4: How do you safely stream a huge 10GB file download without consuming 10GB of RAM on the server?</strong></summary>

### Answer:
- Use FastAPI's `StreamingResponse` with an asynchronous generator that yields chunks (e.g. 64KB):
```python
import aiofiles
from fastapi.responses import StreamingResponse

async def file_stream_generator(file_path: str, chunk_size: int = 65536):
    async with aiofiles.open(file_path, mode="rb") as f:
        while chunk := await f.read(chunk_size):
            yield chunk

@app.get("/download/large")
async def download_file():
    return StreamingResponse(
        file_stream_generator("/data/large.iso"),
        media_type="application/octet-stream",
    )
```
- Memory usage stays constant at $\approx 64\text{KB}$ regardless of whether the file is 100MB or 100GB.
</details>
