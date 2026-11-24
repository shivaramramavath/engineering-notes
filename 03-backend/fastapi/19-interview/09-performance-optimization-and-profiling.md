# Technical Interview Mastery: Performance Optimization & Profiling

## Overview & Evaluation Criteria

Performance interviews test a candidate's ability to diagnose real-world latency spikes, CPU profiling with `py-spy`, event loop lag measurement, async memory leak identification, and response serialization optimizations.

---

## Technical Q&A Deep Dive

<details>
<summary><strong>Q1: How do you measure and alert on Event Loop Lag in an asynchronous FastAPI service?</strong></summary>

### Answer:
- **Event Loop Lag Definition**: The delay between when a scheduled coroutine or timer was supposed to execute and when the event loop actually gave it CPU execution time.
- If lag spikes from 1ms to 2,000ms, it proves that a synchronous blocking operation or CPU-heavy loop is hijacking the single thread.
- **Implementation Recipe**:
```python
import asyncio
import time

async def monitor_event_loop_lag(interval: float = 0.5):
    while True:
        start = time.perf_counter()
        await asyncio.sleep(interval)
        actual_delay = time.perf_counter() - start
        lag = actual_delay - interval
        if lag > 0.05:  # Lag exceeds 50ms
            logger.warning("Event loop lag detected: %.2f ms", lag * 1000)
```
- In production, expose `event_loop_lag_seconds` as a Prometheus Gauge metric.

</details>

<details>
<summary><strong>Q2: How does <code>ORJSONResponse</code> improve throughput over FastAPI's default <code>JSONResponse</code>? What are the serialization edge cases?</strong></summary>

### Answer:
- **Default `JSONResponse`**:
  - Uses Python's standard `json.dumps()` in CPython or `jsonable_encoder`.
  - Serializing complex datetimes, UUIDs, and nested dataclasses requires repeated Python recursive function calls.
- **`ORJSONResponse`**:
  - Uses `orjson`, written in Rust with SIMD (Single Instruction, Multiple Data) optimizations.
  - Natively parses and serializes `datetime`, `UUID`, `dataclasses`, and `numpy` arrays directly into raw bytes in C/Rust without Python helper callbacks.
  - Achieves 3x to 6x faster serialization speeds and lower memory allocations.
- **Edge Case**: `orjson` strictly enforces UTF-8 compliance and does not support arbitrary custom non-serializable objects without an explicit `default` serializer function.

</details>

<details>
<summary><strong>Q3: How do you profile a running FastAPI production pod without restarting it or adding invasive logging?</strong></summary>

### Answer:
- Use **`py-spy`**, a zero-overhead sampling profiler written in Rust for Python programs.
- It attaches to the running Python process ID (PID) from outside using OS kernel system calls (`process_vm_readv` on Linux).
- **Commands**:
  1. `py-spy top --pid <PID>`: Displays live CPU and function call percentages in a top-like interface.
  2. `py-spy record -o flamegraph.svg --pid <PID> --duration 30`: Generates an interactive SVG flamegraph showing exact bottlenecks (e.g. regex compilation, JSON encoding, greenlet switching).
  3. Safe for production: Leaves target process overhead at < 1-2%.
</details>
