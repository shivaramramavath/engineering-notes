# Module 14: High Performance & Latency Optimization

Welcome to **Module 14: High Performance & Latency Optimization**. This module examines the runtime limits of ASGI, optimizing JSON serialization, tuning connection pools, eliminating event loop blocking, and leveraging free-threaded Python 3.14t.

---

## Module Overview

| File | Title | Description |
| :--- | :--- | :--- |
| [01-uvicorn-gunicorn-workers-and-tuning.md](./01-uvicorn-gunicorn-workers-and-tuning.md) | **Worker Process Tuning** | Sizing Uvicorn/Gunicorn worker processes, backlog limits, and worker lifecycle. |
| [02-event-loop-health-and-uvloop.md](./02-event-loop-health-and-uvloop.md) | **uvloop & Event Loop Health** | C-based event loops, loop lag detection, and keeping latency sub-millisecond. |
| [03-orjson-and-fast-serialisation.md](./03-orjson-and-fast-serialisation.md) | **Fast JSON with orjson** | Replacing standard json with Rust-backed `orjson` and `ORJSONResponse`. |
| [04-pydantic-performance-patterns.md](./04-pydantic-performance-patterns.md) | **Pydantic Optimization** | Skipping redundant validation, utilizing `model_dump()`, and Rust core speeds. |
| [05-database-performance.md](./05-database-performance.md) | **Database Optimization** | Preventing N+1 queries, compiled statement caching, and index utilization. |
| [06-caching-strategies.md](./06-caching-strategies.md) | **Caching Strategies** | HTTP Cache-Control, ETag validation, Redis caching, and cache stampede protection. |
| [07-connection-pool-tuning.md](./07-connection-pool-tuning.md) | **Connection Pool Tuning** | Sizing database and HTTP client connection pools under high concurrency. |
| [08-streaming-and-memory-efficiency.md](./08-streaming-and-memory-efficiency.md) | **Streaming & Memory** | Chunked transfer encoding, avoiding large memory allocations, and generator responses. |
| [09-free-threaded-python-and-fastapi.md](./09-free-threaded-python-and-fastapi.md) | **Free-Threaded Python (No-GIL)**| Running FastAPI on Python 3.14t, thread safety, and AnyIO threadpool scaling. |
| [10-load-testing-and-benchmarking-correctly.md](./10-load-testing-and-benchmarking-correctly.md)| **Benchmarking Correctly** | Scientific load testing, warmups, avoiding coordinated omission, and p99 metrics. |
| [11-common-performance-pitfalls.md](./11-common-performance-pitfalls.md) | **Common Performance Pitfalls** | The top 10 performance traps: blocking I/O, sync requests in async def, and unindexed joins. |
