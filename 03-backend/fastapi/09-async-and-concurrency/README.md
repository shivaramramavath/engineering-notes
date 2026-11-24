# Module 09: Async, Concurrency & Threadpool Mechanics

Welcome to **Module 09: Async, Concurrency & Threadpool Mechanics**. This module delves into CPython's event loop, AnyIO structured concurrency, threadpool offloading, and preventing event loop starvation.

---

## Module Overview

| File | Title | Description |
| :--- | :--- | :--- |
| [01-asyncio-for-fastapi-developers.md](./01-asyncio-for-fastapi-developers.md) | **Asyncio Fundamentals** | Event loop, tasks, coroutines, and cooperative multitasking. |
| [02-threadpool-and-sync-endpoints.md](./02-threadpool-and-sync-endpoints.md) | **Threadpool Mechanics** | How FastAPI executes `def` endpoints on AnyIO threadpools. |
| [03-anyio-and-structured-concurrency.md](./03-anyio-and-structured-concurrency.md) | **AnyIO & Task Groups** | Structured concurrency, task groups, and cancel scopes. |
| [04-httpx-async-clients-and-pooling.md](./04-httpx-async-clients-and-pooling.md) | **Async HTTP Clients (HTTPX)** | Reusable `httpx.AsyncClient` connection pools and timeouts. |
| [05-concurrency-limits-and-semaphores.md](./05-concurrency-limits-and-semaphores.md) | **Semaphores & Rate Throttling**| Protecting downstream dependencies using `asyncio.Semaphore`. |
| [06-timeouts-cancellation-and-retries.md](./06-timeouts-cancellation-and-retries.md) | **Timeouts & Cancellation** | `asyncio.timeout` and resilient retry policies with Tenacity. |
| [07-blocking-code-detection.md](./07-blocking-code-detection.md) | **Blocking Code Detection** | Profiling event loop lag and catching blocking calls in dev. |
| [08-cpu-bound-work-with-process-pools.md](./08-cpu-bound-work-with-process-pools.md) | **CPU-Bound Work & Process Pools**| Multiprocessing and ProcessPoolExecutor for heavy calculations. |
| [09-async-context-managers.md](./09-async-context-managers.md) | **Async Context Managers** | Managing asynchronous resources with `__aenter__` and `__aexit__`. |
| [10-async-testing-pitfalls.md](./10-async-testing-pitfalls.md) | **Async Testing Pitfalls** | Event loop scope conflicts, unawaited coroutines, and pytest fixtures. |
