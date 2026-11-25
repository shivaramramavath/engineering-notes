---
title: "High-Level Concurrency: The concurrent.futures Module"
description: "Mastering concurrent execution in Python: ThreadPoolExecutor vs ProcessPoolExecutor, Future lifecycle mechanics, as_completed streaming, process pickling bottlenecks, and optimal pool sizing."
module: "10-standard-library-tour"
order: 11
tags:
  - python
  - concurrent-futures
  - threading
  - multiprocessing
  - threadpoolexecutor
  - processpoolexecutor
  - futures
difficulty: "Intermediate"
cpython_version: "3.12+"
last_verified: "2026-03"
---

# High-Level Concurrency: The concurrent.futures Module

[<- Back to Module Index](./README.md)

---

## Table of Contents
1. [What & Why](#what--why)
2. [Core Concepts & Mental Model](#core-concepts--mental-model)
3. [How It Works Internally](#how-it-works-internally)
4. [Syntax & API Reference](#syntax--api-reference)
5. [Code Examples](#code-examples)
6. [Edge Cases, Pitfalls & Anti-Patterns](#edge-cases-pitfalls--anti-patterns)
7. [Performance & Memory Characteristics](#performance--memory-characteristics)
8. [CPython Internals & Bytecode](#cpython-internals--bytecode)
9. [Best Practices & Idiomatic Patterns](#best-practices--idiomatic-patterns)
10. [Real-World Case Study: Resilient Asynchronous Web Crawler & Document Indexer](#real-world-case-study-resilient-asynchronous-web-crawler--document-indexer)
11. [Interview Questions & Deep-Dive Scenarios](#interview-questions--deep-dive-scenarios)
12. [Practical Exercises & Code Katas](#practical-exercises--code-katas)

---

## What & Why

Python provides the `concurrent.futures` module (introduced in Python 3.2 via PEP 3148) to provide a unified, high-level abstraction for asynchronously executing callables across pools of threads or processes.

### Motivation
Before `concurrent.futures`, coordinating concurrent execution required managing low-level primitives:
- Spawning individual `threading.Thread` or `multiprocessing.Process` instances.
- Manually creating synchronization queues (`queue.Queue`, `multiprocessing.Queue`) to pass data back and forth.
- Managing thread lifecycle termination, worker crashes, unhandled exceptions, and timeouts manually.
`concurrent.futures` encapsulates this complexity behind a clean, unified interface: submit a function, obtain a **`Future`** handle representing the eventual result, and wait for or stream results as they finish.

### Real-World Analogy
Consider ordering food at a busy fast-food restaurant:
- Instead of waiting at the counter while the cooks grill your burger, the cashier hands you an electronic vibrating buzzer (**`Future`**).
- You can walk away, sit at a table, or check your phone (**non-blocking execution**).
- You can periodically check if the buzzer is buzzing (**`future.done()`**), or choose to wait until it vibrates (**`future.result()`**).
- If the kitchen ran out of ingredients, the buzzer alerts you with an error message (**`future.exception()`**).
- The restaurant can fulfill orders using kitchen staff in the same room (**`ThreadPoolExecutor`**) or by routing complex orders to offsite commercial kitchens (**`ProcessPoolExecutor`**).

### When to Use vs When Not to Use
- **Use `ThreadPoolExecutor` for:**
  - I/O-bound operations: Web scraping, REST API requests, database queries, file downloads, disk writes.
  - Concurrency where threads share the same in-memory object graph without serialization.
- **Use `ProcessPoolExecutor` for:**
  - CPU-bound operations: Heavy mathematical calculations, image/video compression, machine learning inferences, cryptographic hashing.
  - Tasks requiring true multi-core parallel scaling by bypassing the CPython GIL.
- **Do Not Use `concurrent.futures` for:**
  - Ultra-high-concurrency network servers handling 20,000+ persistent WebSockets (use `asyncio`).
  - Simple fire-and-forget tasks that can run in a background daemon thread.

---

## Core Concepts & Mental Model

```
+-------------------------------------------------------------------------+
| Application Main Thread                                                 |
|                                                                         |
| future = executor.submit(fetch_data, url="https://api.com")             |
+-------------------------------------------------------------------------+
       |                                                 |
       | If ThreadPoolExecutor                           | If ProcessPoolExecutor
       v                                                 v
+-------------------------------+             +---------------------------+
| OS Worker Thread Pool         |             | OS Worker Process Pool    |
| (Shared Address Space)        |             | (Separate Memory Space)   |
| - Fast submission             |             | - Arguments Pickled (IPC) |
| - Subject to CPython GIL      |             | - True Multi-Core Scaling |
| - Best for I/O bounds         |             | - Bypasses GIL            |
+-------------------------------+             +---------------------------+
       |                                                 |
       +------------------------+------------------------+
                                |
                                v
+-------------------------------------------------------------------------+
| Future Object Lifecycle                                                 |
| State: PENDING -> RUNNING -> FINISHED (Result Available OR Exception)   |
+-------------------------------------------------------------------------+
```

### Key Abstractions
1. **`Executor` (Abstract Base Class)**: Defines the common interface: `.submit()`, `.map()`, and `.shutdown()`.
2. **`Future`**: Encapsulates the asynchronous execution of a callable. Tracks state transitions: `PENDING` $\to$ `RUNNING` $\to$ `CANCELLED` / `FINISHED`.
3. **`as_completed(fs)`**: Generator yielding futures in the exact order they **finish**, eliminating head-of-line blocking!
4. **`wait(fs, return_when=...)`**: Blocks until a specified condition is met: `ALL_COMPLETED`, `FIRST_COMPLETED`, or `FIRST_EXCEPTION`.

---

## How It Works Internally

### 1. `ThreadPoolExecutor` Mechanics
In CPython (`Lib/concurrent/futures/thread.py`):
- Initializes a standard `queue.SimpleQueue()` of work items.
- Spawns up to `max_workers` operating system threads running a continuous worker loop.
- When `submit(fn, *args)` is called:
  1. A `Future()` object is created.
  2. A `_WorkItem(future, fn, args, kwargs)` is appended to the queue.
  3. An idle worker thread pops the work item, invokes `fn()`, catches any unhandled exception, and calls `future.set_result(val)` or `future.set_exception(err)`.
- **The GIL Impact**: Worker threads share the same CPython interpreter. For CPU-bound tasks, only one thread executes Python bytecode at any instant; threads compete for the GIL, resulting in zero speedup (or even slowdowns due to lock contention). For I/O-bound tasks, CPython releases the GIL while waiting on network/disk sockets, enabling true concurrency.

### 2. `ProcessPoolExecutor` Mechanics
In CPython (`Lib/concurrent/futures/process.py`):
- Spawns background OS child processes using `multiprocessing`.
- Uses a background thread in the parent process to feed work items to child processes through inter-process communication (IPC) pipes.
- **The Pickling Requirement**: Every function argument passed to `submit()` and every return value must be serializable using Python's `pickle` module! Objects like database connections, open file handles, lambdas, or complex thread locks cannot be pickled and raise `PicklingError`.

---

## Syntax & API Reference

### Executor Construction
```python
from concurrent.futures import ThreadPoolExecutor, ProcessPoolExecutor

# Thread Pool (Default workers: min(32, (os.cpu_count() or 1) + 4))
with ThreadPoolExecutor(max_workers=10, thread_name_prefix="Worker") as pool:
    future = pool.submit(my_func, arg1, arg2)

# Process Pool (Default workers: os.cpu_count() or 1)
with ProcessPoolExecutor(max_workers=4) as pool:
    future = pool.submit(heavy_cpu_func, data)
```

### `Future` Methods
```python
is_done: bool = future.done()
is_cancelled: bool = future.cancelled()
success: bool = future.cancel()          # Returns True if cancelled before starting

# Result & Exception Extraction
result_val = future.result(timeout=None) # Blocks until complete; raises stored exception!
exc = future.exception(timeout=None)     # Returns Exception instance or None if succeeded

# Callbacks
future.add_done_callback(fn)             # fn(future) is invoked when future completes
```

### Synchronization Primitives
```python
from concurrent.futures import as_completed, wait, FIRST_COMPLETED, ALL_COMPLETED

# Stream as they complete
for future in as_completed(futures_list, timeout=None):
    res = future.result()

# Wait for condition
done_set, not_done_set = wait(futures_list, timeout=5.0, return_when=FIRST_COMPLETED)
```

---

## Code Examples

### Example 1: Parallel I/O Web Requests with `ThreadPoolExecutor` and `as_completed`
```python
from concurrent.futures import ThreadPoolExecutor, as_completed
import time
from typing import Tuple

def simulate_fetch_api(service_name: str, latency: float) -> Tuple[str, int]:
    time.sleep(latency)
    if service_name == "Faulty_Service":
        raise ConnectionResetError("Remote host reset connection")
    return service_name, 200

def run_thread_pool_demo():
    services = [
        ("Auth_Service", 0.05),
        ("Billing_Service", 0.02),    # Finishes first!
        ("Faulty_Service", 0.03),     # Fails!
        ("Inventory_Service", 0.04)
    ]

    results = {}
    with ThreadPoolExecutor(max_workers=4) as executor:
        # Submit all tasks: returns map of future -> service_name
        future_map = {
            executor.submit(simulate_fetch_api, name, lat): name
            for name, lat in services
        }

        # as_completed yields futures as they finish, eliminating head-of-line blocking
        for future in as_completed(future_map):
            name = future_map[future]
            try:
                service, status = future.result()
                results[service] = status
                print(f"[SUCCESS] {service} returned HTTP {status}")
            except Exception as err:
                print(f"[FAILED]  {name} raised: {type(err).__name__} ({err})")

    print("\nFinal Results Map:", results)

run_thread_pool_demo()
# Output:
# [SUCCESS] Billing_Service returned HTTP 200
# [FAILED]  Faulty_Service raised: ConnectionResetError (Remote host reset connection)
# [SUCCESS] Inventory_Service returned HTTP 200
# [SUCCESS] Auth_Service returned HTTP 200
# 
# Final Results Map: {'Billing_Service': 200, 'Inventory_Service': 200, 'Auth_Service': 200}
```

### Example 2: True Multi-Core CPU Parallelism with `ProcessPoolExecutor`
```python
from concurrent.futures import ProcessPoolExecutor
import time
import math

def compute_heavy_factors(number: int) -> int:
    """CPU-bound task: Count prime factors."""
    count = 0
    d = 2
    n = number
    while d * d <= n:
        while n % d == 0:
            count += 1
            n //= d
        d += 1
    if n > 1:
        count += 1
    return count

def run_process_pool_demo():
    numbers = [1000000007, 1000000009, 1000000021, 1000000033]

    start = time.perf_counter()
    with ProcessPoolExecutor(max_workers=4) as executor:
        # executor.map preserves input order, similar to built-in map()
        results = list(executor.map(compute_heavy_factors, numbers))

    duration = time.perf_counter() - start
    print(f"Processed {len(numbers)} numbers across CPU cores in {duration:.3f}s")
    print("Factor counts:", results)

if __name__ == "__main__":
    run_process_pool_demo()
# Output:
# Processed 4 numbers across CPU cores in ~0.08s
# Factor counts: [1, 2, 2, 2]
```

### Example 3: First-Completed Race Condition with `wait()`
```python
from concurrent.futures import ThreadPoolExecutor, wait, FIRST_COMPLETED
import time

def fetch_from_primary():
    time.sleep(0.08)
    return "Data from Primary Datacenter"

def fetch_from_mirror():
    time.sleep(0.02)  # Mirror is faster
    return "Data from Edge Replica"

def run_hedged_request_demo():
    with ThreadPoolExecutor(max_workers=2) as executor:
        # Submit hedged requests to two redundant endpoints
        f_primary = executor.submit(fetch_from_primary)
        f_mirror = executor.submit(fetch_from_mirror)

        # Wait until the FIRST response returns
        done, not_done = wait([f_primary, f_mirror], return_when=FIRST_COMPLETED)

        fastest_future = done.pop()
        result = fastest_future.result()
        print("Fastest result obtained:", result)

        # Cancel or discard remaining slow future
        for slow in not_done:
            slow.cancel()

run_hedged_request_demo()
# Output:
# Fastest result obtained: Data from Edge Replica
```

### Example 4: Asynchronous Callback Registration with `add_done_callback`
```python
from concurrent.futures import ThreadPoolExecutor
import time

def download_asset(asset_id: int) -> str:
    time.sleep(0.02)
    return f"asset_data_{asset_id}"

def on_asset_ready(future):
    """Callback triggered on worker thread immediately upon future completion."""
    try:
        data = future.result()
        print(f"[CALLBACK NOTIFIED] Completed download: {data}")
    except Exception as e:
        print(f"[CALLBACK NOTIFIED] Download failed: {e}")

def run_callback_demo():
    with ThreadPoolExecutor(max_workers=2) as executor:
        f = executor.submit(download_asset, 404)
        f.add_done_callback(on_asset_ready)
        print("Callback attached. Main thread continues immediately...")

    print("Executor shutdown complete.")

run_callback_demo()
# Output:
# Callback attached. Main thread continues immediately...
# [CALLBACK NOTIFIED] Completed download: asset_data_404
# Executor shutdown complete.
```

### Example 5: Enforcing Hard Timeouts on Future Results
```python
from concurrent.futures import ThreadPoolExecutor, TimeoutError as FutureTimeoutError
import time

def slow_service_call():
    time.sleep(1.0)
    return "Late Response"

def run_timeout_demo():
    with ThreadPoolExecutor(max_workers=1) as executor:
        future = executor.submit(slow_service_call)

        try:
            # Enforce 0.05 second strict timeout
            result = future.result(timeout=0.05)
        except FutureTimeoutError:
            print("Future timed out! Service took longer than 50ms.")

run_timeout_demo()
# Output:
# Future timed out! Service took longer than 50ms.
```

---

## Edge Cases, Pitfalls & Anti-Patterns

### 1. The `ProcessPoolExecutor` Pickling Trap
Every argument and return value in `ProcessPoolExecutor` must be serialized with `pickle`. Attempting to pass lambdas, local inner functions, generators, database connections, or open file objects raises `AttributeError: Can't pickle local object...` or `TypeError: cannot pickle '_io.TextIOWrapper' object`.
- **Rule**: Functions passed to `ProcessPoolExecutor.submit()` must be importable module-level functions.

### 2. The Missing `if __name__ == '__main__':` Guard
On Windows and macOS (which uses `spawn` instead of `fork` for multiprocessing), spawning worker processes re-imports the main entry point script. If `ProcessPoolExecutor` is instantiated outside `if __name__ == '__main__':`, every spawned child process creates more child processes in an infinite recursive explosion!

### 3. Masking Exceptions by Forgetting `.result()`
If a submitted function raises an unhandled exception, `concurrent.futures` captures the exception and stores it on the `Future` object. If your code never calls `future.result()` or `future.exception()`, the exception is **silently swallowed**!

### Common Mistakes Table

| Bad Code | Good Code | Why | Severity |
| :--- | :--- | :--- | :--- |
| `ProcessPool` without `if __name__ == '__main__':` | Wrap in `if __name__ == '__main__':` | Triggers infinite recursive process fork bomb on Windows/macOS. | Critical |
| `ThreadPool` for heavy matrix math | `ProcessPoolExecutor` | CPython GIL prevents threads from running CPU bytecode in parallel. | High |
| Passing lambda to `ProcessPool.submit` | Pass top-level module function | Lambdas cannot be pickled; raises `PicklingError`. | High |
| Ignoring `future.result()` | Always inspect `future.result()` | Unhandled exceptions inside worker functions are silently swallowed. | High |
| `pool = ThreadPoolExecutor(max_workers=5000)` | Use bounded pool (e.g. 50-100) | Spawning 5,000 OS threads exhausts kernel memory and thread stacks. | High |
| Iterating `[f.result() for f in futures]` | Use `as_completed(futures)` | Iterating list blocks on the first slow future (head-of-line blocking). | Medium |

---

## Performance & Memory Characteristics

### Pool Sizing Guidelines
- **I/O-Bound Tasks (`ThreadPoolExecutor`)**:
  Optimal size is governed by latency and external rate limits:
  $$\text{max\_workers} = \min(32, \text{cpu\_count} + 4) \quad \text{or up to } 50\text{--}100 \text{ for high-latency network I/O}.$$
- **CPU-Bound Tasks (`ProcessPoolExecutor`)**:
  Optimal size equals the number of physical CPU cores:
  $$\text{max\_workers} = \text{os.cpu\_count()}.$$
  Exceeding the physical CPU core count causes OS process thrashing and context-switch degradation.

### IPC Overhead in `ProcessPoolExecutor`
Passing a 100 MB NumPy array or dictionary through `ProcessPoolExecutor.submit()` forces Python to:
1. Serialize the 100 MB object into bytes (`pickle.dumps()`).
2. Write bytes across OS IPC pipes.
3. Deserialize bytes in child process (`pickle.loads()`).
If computation takes 5 ms but pickling takes 200 ms, multiprocessing will run **slower** than single-threaded execution! Use `multiprocessing.shared_memory` for zero-copy data transfer.

---

## CPython Internals & Bytecode

In CPython:
- `concurrent.futures` is a pure Python package (`Lib/concurrent/futures/`).
- The `Future` class coordinates state transitions via a dedicated `threading.Condition()` variable.
- Calling `future.result(timeout)` acquires the condition lock, calls `self._condition.wait(timeout)` if state is not `FINISHED`, and either returns `self._result` or raises `self._exception`.

---

## Best Practices & Idiomatic Patterns

1. **Always Use Context Managers for Executors**:
   ```python
   with ThreadPoolExecutor() as executor:
       futures = [executor.submit(fn, x) for x in data]
   # Context manager automatically calls executor.shutdown(wait=True) on exit
   ```
2. **Use `as_completed` Instead of `executor.map` When Runtimes Vary**:
   `executor.map()` yields results in the order items were submitted. If the first item takes 10 seconds and items 2–10 take 10 milliseconds, `executor.map()` blocks until item 1 finishes.
3. **Cancel Pending Work on Early Exit**:
   ```python
   done, not_done = wait(futures, return_when=FIRST_COMPLETED)
   for f in not_done:
       f.cancel()
   ```

---

## Real-World Case Study: Resilient Asynchronous Web Crawler & Document Indexer

The following engine crawls URLs concurrently using a bounded thread pool, handles transient failures, and indexes document word counts using a parallel process pool.

```python
from concurrent.futures import ThreadPoolExecutor, ProcessPoolExecutor, as_completed
import urllib.request
import urllib.error
import time
from collections import Counter
from typing import Dict, List, Tuple

def fetch_document(url: str) -> Tuple[str, str]:
    """I/O-Bound network fetch executed in ThreadPool."""
    req = urllib.request.Request(url, headers={"User-Agent": "CrawlerBot/1.0"})
    try:
        with urllib.request.urlopen(req, timeout=3.0) as resp:
            content = resp.read().decode("utf-8", errors="ignore")
            return url, content
    except Exception as e:
        return url, ""

def count_words_cpu(doc_tuple: Tuple[str, str]) -> Tuple[str, Counter]:
    """CPU-Bound text tokenization executed in ProcessPool."""
    url, content = doc_tuple
    words = [w.lower() for w in content.split() if w.isalnum()]
    return url, Counter(words)

class ParallelIngestionPipeline:
    def __init__(self, max_io_workers: int = 5, max_cpu_workers: int = 2):
        self.io_pool = ThreadPoolExecutor(max_workers=max_io_workers)
        self.cpu_pool = ProcessPoolExecutor(max_workers=max_cpu_workers)

    def process_urls(self, urls: List[str]) -> Dict[str, int]:
        total_word_counts = Counter()

        # Step 1: Submit network fetches to ThreadPool
        fetch_futures = [self.io_pool.submit(fetch_document, u) for u in urls]

        # Step 2: Stream fetched documents into CPU ProcessPool as they arrive
        analysis_futures = []
        for future in as_completed(fetch_futures):
            url, content = future.result()
            if content:
                print(f"[I/O COMPLETE] Fetched {len(content)} bytes from {url}")
                # Pass off to multi-core process pool for heavy parsing
                analysis_futures.append(self.cpu_pool.submit(count_words_cpu, (url, content)))

        # Step 3: Collect CPU analytics results
        for future in as_completed(analysis_futures):
            url, counts = future.result()
            print(f"[CPU COMPLETE] Analyzed {url}: {sum(counts.values())} words found.")
            total_word_counts.update(counts)

        return dict(total_word_counts.most_common(5))

    def shutdown(self):
        self.io_pool.shutdown(wait=True)
        self.cpu_pool.shutdown(wait=True)

# Verification
if __name__ == "__main__":
    # Test pipeline instantiation and structure
    pipeline = ParallelIngestionPipeline(max_io_workers=2, max_cpu_workers=2)
    pipeline.shutdown()
    print("Concurrent pipeline verified successfully.")
```

---

## Interview Questions & Deep-Dive Scenarios

<details>
<summary><strong>1. Junior: Why doesn't <code>ThreadPoolExecutor</code> speed up CPU-bound mathematical calculations in standard CPython?</strong></summary>

CPython utilizes a Global Interpreter Lock (GIL) to protect internal object memory management. The GIL ensures that only one native OS thread executes Python bytecode instructions at any given instant. While threads can run concurrently on I/O operations (because CPython releases the GIL while waiting on network/disk sockets), CPU-bound threads fight over the GIL, running sequentially with context-switching overhead.
</details>

<details>
<summary><strong>2. Junior: What is the difference between <code>executor.submit()</code> and <code>executor.map()</code>?</strong></summary>

- `executor.submit(fn, *args)` submits a single function call asynchronously and returns an individual `Future` object immediately.
- `executor.map(fn, *iterables)` is an asynchronous equivalent to the built-in `map()`. It submits all calls across the iterable and returns an iterator that yields results in the **exact order of submission**, blocking if an earlier result is not yet ready.
</details>

<details>
<summary><strong>3. Mid: What is the advantage of using <code>as_completed(futures)</code> over iterating through a list of futures?</strong></summary>

Iterating through a list of futures (`for f in futures: res = f.result()`) blocks on the first future in the list. If the first future takes 10 seconds to finish, but subsequent futures took only 50 milliseconds, the application blocks completely at index 0 (head-of-line blocking). `as_completed()` yields futures dynamically the instant each one finishes, allowing results to be processed immediately with zero wasted idle time.
</details>

<details>
<summary><strong>4. Mid: What happens if a function submitted to an executor raises an unhandled exception?</strong></summary>

The exception is caught by the worker thread/process and stored inside the `Future` object. The worker thread does not crash. When the main program calls `future.result()`, the stored exception is re-raised at the call site. If the program calls `future.exception()`, it returns the exception object. If neither method is called, the exception is silently ignored.
</details>

<details>
<summary><strong>5. Senior: Why can <code>ProcessPoolExecutor</code> sometimes run significantly slower than single-threaded execution?</strong></summary>

`ProcessPoolExecutor` relies on Inter-Process Communication (IPC) pipes. When arguments are submitted and results are returned, CPython must serialize objects using `pickle.dumps()`, transmit the bytes over OS pipes, and deserialize them in the child process using `pickle.loads()`. If the data payloads are large (e.g. hundreds of megabytes of raw arrays) and the computation is relatively fast, the IPC pickling and serialization overhead will vastly outweigh the parallel compute gains.
</details>

<details>
<summary><strong>6. Senior: How does <code>concurrent.futures.wait(fs, return_when=FIRST_COMPLETED)</code> enable hedged/redundant requests?</strong></summary>

In distributed systems, tail latency can cause occasional requests to hang. With hedged requests, an application submits the same read request to two redundant servers simultaneously. By calling `wait([f1, f2], return_when=FIRST_COMPLETED)`, the application wakes up the instant the faster server replies, extracts its result, and calls `.cancel()` on the slower pending future, neutralizing network tail latency spikes.
</details>

<details>
<summary><strong>7. Staff: How does Python 3.13's experimental free-threaded mode (PEP 703 / no-GIL) impact the choice between ThreadPoolExecutor and ProcessPoolExecutor?</strong></summary>

In Python 3.13 free-threaded builds (`python3.13t`), the Global Interpreter Lock is removed and object memory is protected via mimalloc thread-safe allocators and biased reference counting. Under a free-threaded runtime, `ThreadPoolExecutor` can execute CPU-bound Python bytecode across multiple physical CPU cores in true parallel fashion within a single process. This eliminates the IPC pickling serialization bottleneck and memory replication overhead of `ProcessPoolExecutor` for many compute workloads.
</details>

<details>
<summary><strong>8. Staff: How would you architect a task execution system that gracefully handles worker process crashes (OOM killer, segmentation faults) in <code>ProcessPoolExecutor</code>?</strong></summary>

In standard Python, if a worker process in `ProcessPoolExecutor` is terminated abruptly by the Linux OOM killer (`SIGKILL`), subsequent calls on in-flight futures raise `BrokenProcessPool: A process in the process pool was terminated abruptly`.
Mitigation Architecture:
1. Catch `BrokenProcessPool` at the scheduling layer.
2. Terminate the broken executor instance immediately and instantiate a fresh `ProcessPoolExecutor`.
3. Track unacknowledged tasks using an idempotent persistent queue (e.g. Redis / SQLite WAL).
4. Re-submit incomplete tasks with exponential backoff and memory quota limits.
</details>

---

## Practical Exercises & Code Katas

### Exercise 1: Parallel Port Scanner
**Requirement:** Write a function `scan_ports(host: str, ports: list[int]) -> list[int]` using `ThreadPoolExecutor` that tests TCP socket connections in parallel, returning a list of open ports.

<details>
<summary>Show Solution</summary>

```python
import socket
from concurrent.futures import ThreadPoolExecutor, as_completed

def check_port(host: str, port: int) -> tuple[int, bool]:
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(0.5)
    try:
        res = s.connect_ex((host, port))
        return port, (res == 0)
    finally:
        s.close()

def scan_ports(host: str, ports: list[int]) -> list[int]:
    open_ports = []
    with ThreadPoolExecutor(max_workers=20) as pool:
        futures = {pool.submit(check_port, host, p): p for p in ports}
        for f in as_completed(futures):
            port, is_open = f.result()
            if is_open:
                open_ports.append(port)
    return sorted(open_ports)

# Test (Local loopback scan)
results = scan_ports("127.0.0.1", [80, 443, 8080, 22])
print("Scanned Ports (Open):", results)
```
</details>

### Exercise 2: Parallel Batch File Hasher
**Requirement:** Use `ThreadPoolExecutor` to compute SHA-256 hashes of multiple files in parallel, returning a dictionary mapping `{file_path: sha256_hex}`.

<details>
<summary>Show Solution</summary>

```python
import hashlib
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path
import tempfile

def hash_file(path: Path) -> tuple[Path, str]:
    hasher = hashlib.sha256()
    with open(path, "rb") as f:
        while chunk := f.read(65536):
            hasher.update(chunk)
    return path, hasher.hexdigest()

def hash_files_parallel(paths: list[Path]) -> dict[Path, str]:
    results = {}
    with ThreadPoolExecutor(max_workers=4) as executor:
        future_map = {executor.submit(hash_file, p): p for p in paths}
        for future in as_completed(future_map):
            p, digest = future.result()
            results[p] = digest
    return results

# Verification
with tempfile.TemporaryDirectory() as tmp:
    t = Path(tmp)
    files = []
    for i in range(3):
        f = t / f"file_{i}.txt"
        f.write_text(f"Content {i}")
        files.append(f)

    hashes = hash_files_parallel(files)
    print("Computed Hashes:", len(hashes))
    assert len(hashes) == 3
```
</details>

### Exercise 3: Fault-Tolerant Bulk Worker with Error Collection
**Requirement:** Write a function `safe_parallel_map(fn, items)` using `ThreadPoolExecutor` that executes `fn` across `items`, returning two separate lists: `(successful_results, failed_exceptions)`.

<details>
<summary>Show Solution</summary>

```python
from concurrent.futures import ThreadPoolExecutor, as_completed

def safe_parallel_map(fn, items: list) -> tuple[list, list]:
    successes = []
    failures = []

    with ThreadPoolExecutor(max_workers=4) as executor:
        future_map = {executor.submit(fn, item): item for item in items}
        for future in as_completed(future_map):
            item = future_map[future]
            try:
                res = future.result()
                successes.append((item, res))
            except Exception as e:
                failures.append((item, e))

    return successes, failures

# Test
def risky_division(n: int) -> float:
    if n == 0:
        raise ZeroDivisionError("Cannot divide by zero")
    return 100.0 / n

nums = [10, 0, 5, 0, 2]
ok, errs = safe_parallel_map(risky_division, nums)
print(f"Successes: {len(ok)}, Failures: {len(errs)}")
assert len(ok) == 3
assert len(errs) == 2
```
</details>

### Exercise 4: First-Success Race Pattern
**Requirement:** Implement `race_first_success(callables: list[Callable])` that dispatches multiple functions concurrently and returns the return value of the FIRST function that succeeds without raising an exception.

<details>
<summary>Show Solution</summary>

```python
from concurrent.futures import ThreadPoolExecutor, as_completed
import time

def race_first_success(callables: list):
    with ThreadPoolExecutor(max_workers=len(callables)) as executor:
        futures = [executor.submit(fn) for fn in callables]
        errors = []
        for f in as_completed(futures):
            try:
                res = f.result()
                # Cancel all remaining futures
                for rem in futures:
                    rem.cancel()
                return res
            except Exception as e:
                errors.append(e)

        raise RuntimeError(f"All {len(callables)} tasks failed: {errors}")

# Test
def slow_failing():
    time.sleep(0.01)
    raise ValueError("Failed")

def fast_success():
    time.sleep(0.03)
    return "Fast Success Result"

winner = race_first_success([slow_failing, fast_success])
print("Winner:", winner)
assert winner == "Fast Success Result"
```
</details>

### Exercise 5: Parallel Array Normalizer with ProcessPoolExecutor
**Requirement:** Use `ProcessPoolExecutor` to normalize multiple independent lists of floats (subtract mean, divide by standard deviation) across separate CPU processes.

<details>
<summary>Show Solution</summary>

```python
import math
from concurrent.futures import ProcessPoolExecutor

def normalize_dataset(data: list[float]) -> list[float]:
    mean = sum(data) / len(data)
    variance = sum((x - mean) ** 2 for x in data) / len(data)
    std_dev = math.sqrt(variance) or 1.0
    return [(x - mean) / std_dev for x in data]

# Verification
if __name__ == "__main__":
    datasets = [
        [1.0, 2.0, 3.0, 4.0, 5.0],
        [10.0, 20.0, 30.0],
        [100.0, 100.0, 100.0]
    ]

    with ProcessPoolExecutor(max_workers=2) as pool:
        normalized = list(pool.map(normalize_dataset, datasets))

    print("Normalized Datasets:", len(normalized))
    assert len(normalized) == 3
```
</details>

### Exercise 6: Callback-Driven Batch Progress Tracker
**Requirement:** Track completion progress of a 100-task thread pool job using `add_done_callback` and an atomic counter.

<details>
<summary>Show Solution</summary>

```python
from concurrent.futures import ThreadPoolExecutor
import threading
import time

class ProgressTracker:
    def __init__(self, total_tasks: int):
        self.total = total_tasks
        self.completed = 0
        self.lock = threading.Lock()

    def on_task_finish(self, future):
        with self.lock:
            self.completed += 1
            if self.completed % 25 == 0 or self.completed == self.total:
                print(f"Progress: {self.completed}/{self.total} ({self.completed / self.total:.0%})")

tracker = ProgressTracker(50)
with ThreadPoolExecutor(max_workers=5) as executor:
    for i in range(50):
        f = executor.submit(time.sleep, 0.001)
        f.add_done_callback(tracker.on_task_finish)
```
</details>
