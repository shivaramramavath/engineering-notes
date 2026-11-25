---
title: "Subinterpreters and Per-Interpreter GIL: Architecture and Isolation (PEP 684 / PEP 554)"
description: "Mastering Python subinterpreters: PEP 684 per-interpreter GIL architecture, PEP 554 high-level API, isolated sys.modules state, inter-interpreter channels, and comparing subinterpreters vs multiprocessing."
module: "12-concurrency"
order: 11
tags:
  - python
  - concurrency
  - subinterpreters
  - pep-684
  - pep-554
  - per-interpreter-gil
  - parallelism
  - isolation
difficulty: "Advanced"
cpython_version: "3.12+"
---

# Subinterpreters and Per-Interpreter GIL: Architecture and Isolation (PEP 684 / PEP 554)

Historically, Python developers seeking parallel execution were forced to choose between two extremes:
1. **Threads**: Shared memory and fast startup, but crippled by a single Global Interpreter Lock (GIL) preventing multi-core CPU scaling.
2. **Processes (`multiprocessing`)**: True multi-core parallelism, but high memory overhead (~30 MB per process), slow startup times, and complex IPC serialization.

In Python 3.12, CPython introduced a revolutionary middle ground: **PEP 684 (A Per-Interpreter GIL)**. Combined with the emerging standard library **`interpreters` module (PEP 554)**, subinterpreters allow multiple independent Python interpreter instances to run within a **single operating system process**, each with its own isolated GIL, running in true parallel execution across multiple CPU cores.

---

## Table of Contents

1. [What & Why](#what--why)
2. [Core Concepts & Mental Model](#core-concepts--mental-model)
3. [The Per-Interpreter GIL Revolution (PEP 684)](#the-per-interpreter-gil-revolution-pep-684)
4. [How It Works Internally: Memory Isolation & Channels](#how-it-works-internally-memory-isolation--channels)
5. [Syntax & API Reference](#syntax--api-reference)
6. [Code Examples](#code-examples)
7. [Edge Cases, Pitfalls & Anti-Patterns](#edge-cases-pitfalls--anti-patterns)
8. [Performance & Memory Characteristics](#performance--memory-characteristics)
9. [CPython Internals & Bytecode](#cpython-internals--bytecode)
10. [Best Practices & Idiomatic Patterns](#best-practices--idiomatic-patterns)
11. [Real-World Case Study: Secure Multi-Tenant Plugin & Code Execution Sandbox](#real-world-case-study-secure-multi-tenant-plugin--code-execution-sandbox)
12. [Interview Questions & Deep-Dive Scenarios](#interview-questions--deep-dive-scenarios)
13. [Practical Exercises & Code Katas](#practical-exercises--code-katas)

---

## What & Why

What makes subinterpreters uniquely valuable?
- **Single Process Address Space**: Runs entirely within a single OS PID. Deploying subinterpreters in Kubernetes or serverless environments avoids OS process limits and memory quota fragmentation.
- **True Multi-Core Parallelism**: Because each subinterpreter owns an independent GIL, two OS threads running code in two subinterpreters execute simultaneously on separate physical CPU cores.
- **Hermetic Memory Isolation**: Each subinterpreter has its own `sys.modules`, global variables, and builtins. Mutating global state in Subinterpreter A cannot pollute or corrupt Subinterpreter B.
- **Fast Startup**: Spinning up a subinterpreter takes ~5 to 10 ms (10x faster than spawning a new OS Python process).

---

## Core Concepts & Mental Model

### Architectural Comparison: The Concurrency Spectrum

```mermaid
graph TD
    subgraph 1. Multithreading (Single Process, Single GIL)
        P1["Process"] --> GIL1["Single GIL"]
        GIL1 --> T1["Thread A"]
        GIL1 --> T2["Thread B"]
        Note1["Shared memory; Serialized CPU execution"]
    end

    subgraph 2. Subinterpreters (Single Process, Multiple GILs)
        P2["Process (Single PID)"]
        P2 --> InterpA["Subinterpreter A\n(Private GIL & Private Modules)"]
        P2 --> InterpB["Subinterpreter B\n(Private GIL & Private Modules)"]
        InterpA --> TA["Thread 1"]
        InterpB --> TB["Thread 2"]
        TA -.->|Channel IPC| TB
        Note2["True parallel CPU execution inside single OS process!"]
    end

    subgraph 3. Multiprocessing (Multiple OS Processes)
        ProcA["Process 1 (PID 1001)"]
        ProcB["Process 2 (PID 1002)"]
        ProcA -.->|OS Pipe / Sockets| ProcB
        Note3["Separate memory spaces; Highest isolation; Heavy startup"]
    end
```

---

## The Per-Interpreter GIL Revolution (PEP 684)

Subinterpreters have existed in CPython's C-API since Python 1.5 (`Py_NewInterpreter()`). However, for 25 years, all subinterpreters shared the **same single global GIL**, rendering them useless for CPU parallelism.

Eric Snow's **PEP 684** (landed in Python 3.12) dismantled this constraint by moving the GIL from the global process runtime structure (`_PyRuntime`) into the individual interpreter state struct (`PyInterpreterState`):
```c
// CPython 3.12+ struct PyInterpreterState
struct _is {
    struct _is *next;
    struct pythreads {
        struct _gil_runtime_state gil; // PER-INTERPRETER GIL!
    } _threads;
    PyObject *modules; // Private sys.modules
    // ...
};
```

---

## How It Works Internally: Memory Isolation & Channels

### Complete State Isolation
Inside a subinterpreter:
- `sys.modules` is completely private: importing a module in one subinterpreter does not import or warm the cache in another.
- Global variables (`globals()`) are physically isolated in separate heap dictionaries.
- Built-in types and exceptions are distinct instances per interpreter.

### Inter-Interpreter Channels
Because subinterpreters cannot safely pass raw Python heap object references without risking cross-interpreter refcount race conditions, PEP 554 defines **Channels**:
- Supported payloads: Immutable primitives (`int`, `float`, `str`, `bytes`, `None`, and tuples of primitives).
- Payloads are passed across in-memory buffers with minimal serialization overhead.

---

## Syntax & API Reference

### 1. The Low-Level `_interpreters` Module (Python 3.12+)

```python
import _interpreters

# 1. Create subinterpreter with isolated GIL:
interp_id = _interpreters.create()

# 2. Execute string of Python code inside isolated subinterpreter:
_interpreters.run_string(interp_id, "print('Running in isolated interpreter!')")

# 3. Destroy subinterpreter:
_interpreters.destroy(interp_id)
```

### 2. Inter-Interpreter Channels

```python
# 1. Create a channel
cid = _interpreters.channel_create()

# 2. Send payload into channel
_interpreters.channel_send(cid, "CRITICAL_PAYLOAD")

# 3. Receive payload from channel
msg = _interpreters.channel_recv(cid)
_interpreters.channel_destroy(cid)
```

---

## Code Examples

### Example 1: Basic Subinterpreter Creation and Execution

```python
import _interpreters
import os

print(f"[Main] Primary Process PID: {os.getpid()}")

# Create isolated subinterpreter
interp_id = _interpreters.create()
print(f"[Main] Created Subinterpreter ID: {interp_id}")

# Execute isolated code
script = """
import os
import sys

# Verify process ID is identical (single OS process!):
print(f"  [Subinterpreter] Running inside PID: {os.getpid()}")
print(f"  [Subinterpreter] Python version: {sys.version.split()[0]}")
"""

_interpreters.run_string(interp_id, script)

# Teardown
_interpreters.destroy(interp_id)
print("[Main] Subinterpreter destroyed cleanly.")
# Output:
# [Main] Primary Process PID: 9410
# [Main] Created Subinterpreter ID: 1
#   [Subinterpreter] Running inside PID: 9410
#   [Subinterpreter] Python version: 3.12.x
# [Main] Subinterpreter destroyed cleanly.
```

### Example 2: Verifying Module and Global State Isolation

Demonstrating that mutating global module state in a subinterpreter has zero impact on the host process:

```python
import _interpreters

# Define a module-level variable in main process
import math
math.custom_flag = "ORIGINAL_VALUE"  # type: ignore[attr-defined]

interp_id = _interpreters.create()

# Subinterpreter imports math and attempts to read / mutate it:
isolation_test_script = """
import math

# In subinterpreter, math is freshly loaded! custom_flag does NOT exist:
has_flag = hasattr(math, 'custom_flag')
print(f"  [Subinterpreter] Can see main math.custom_flag? {has_flag}")

# Mutate math inside subinterpreter
math.subinterpreter_flag = 'SUB_VALUE'
"""

_interpreters.run_string(interp_id, isolation_test_script)

print(f"[Main] Main math.custom_flag intact: {getattr(math, 'custom_flag', None)}")
print(f"[Main] Can main see subinterpreter_flag? {hasattr(math, 'subinterpreter_flag')}")

_interpreters.destroy(interp_id)
# Output:
#   [Subinterpreter] Can see main math.custom_flag? False
# [Main] Main math.custom_flag intact: ORIGINAL_VALUE
# [Main] Can main see subinterpreter_flag? False
```

### Example 3: Multi-Core CPU Parallelism with Concurrent Subinterpreters

Running two subinterpreters on separate OS threads, executing CPU-bound math in parallel across separate physical cores:

```python
import _interpreters
import threading
import time

def run_worker_in_subinterpreter(interp_id: int, iterations: int):
    code = f"""
total = 0
for i in range({iterations}):
    total += i
"""
    _interpreters.run_string(interp_id, code)

def benchmark_subinterpreters():
    interp1 = _interpreters.create()
    interp2 = _interpreters.create()
    iterations = 20_000_000

    # Sequential baseline
    t0 = time.perf_counter()
    run_worker_in_subinterpreter(interp1, iterations)
    run_worker_in_subinterpreter(interp2, iterations)
    dur_seq = time.perf_counter() - t0
    print(f"Sequential Subinterpreters: {dur_seq:.3f}s")

    # Parallel multithreaded execution across independent GILs!
    t0 = time.perf_counter()
    t1 = threading.Thread(target=run_worker_in_subinterpreter, args=(interp1, iterations))
    t2 = threading.Thread(target=run_worker_in_subinterpreter, args=(interp2, iterations))
    
    t1.start(); t2.start()
    t1.join(); t2.join()
    dur_parallel = time.perf_counter() - t0
    print(f"Parallel Subinterpreters:   {dur_parallel:.3f}s")

    speedup = dur_seq / dur_parallel
    print(f"Observed Multi-Core Speedup: {speedup:.2f}x (True CPU Parallelism!)")

    _interpreters.destroy(interp1)
    _interpreters.destroy(interp2)

# benchmark_subinterpreters()
```

### Example 4: Inter-Interpreter Channel Communication

```python
import _interpreters
import threading

def subinterpreter_consumer(interp_id: int, channel_id: int):
    code = f"""
import _interpreters
msg = _interpreters.channel_recv({channel_id})
print(f"  [Subinterpreter] Received from channel: '{{msg}}'")
"""
    _interpreters.run_string(interp_id, code)

# Create resources
interp_id = _interpreters.create()
channel_id = _interpreters.channel_create()

# Start consumer thread
t = threading.Thread(target=subinterpreter_consumer, args=(interp_id, channel_id))
t.start()

# Main process sends data across in-memory channel:
print("[Main] Sending payload across channel...")
_interpreters.channel_send(channel_id, "AUTH_TOKEN_XYZ_9901")

t.join()

# Cleanup
_interpreters.channel_destroy(channel_id)
_interpreters.destroy(interp_id)
print("[Main] Channel and Subinterpreter torn down.")
# Output:
# [Main] Sending payload across channel...
#   [Subinterpreter] Received from channel: 'AUTH_TOKEN_XYZ_9901'
# [Main] Channel and Subinterpreter torn down.
```

---

## Edge Cases, Pitfalls & Anti-Patterns

### Common Mistakes Table

| Mistake | Bad Code | Correct Idiom | Why It Fails |
|:---|:---|:---|:---|
| **Passing raw Python objects across channels** | Sending custom class instance or open file | Pass primitives (`str`, `bytes`, `int`, `tuple`) | Subinterpreters have separate heaps; sharing pointers causes memory corruption and segfaults. |
| **Forgetting to destroy subinterpreters** | Dropping `interp_id` without `destroy()` | Always call `_interpreters.destroy(interp_id)` | Memory for the subinterpreter's `sys.modules` and heap leaks within the process. |
| **Importing non-isolated C-extensions** | Importing legacy C-extensions with global static state | Use extensions compiled with multi-phase initialization (PEP 489) | C-extensions with global static C variables collide across subinterpreters. |
| **Assuming subinterpreters run in separate OS processes** | Relying on subinterpreters to survive C-level segfaults | Use `multiprocessing` for hard fault boundaries | If a C-extension segfaults inside a subinterpreter, the entire host OS process dies. |
| **Attempting to access variables across scopes** | Expecting global `x` to exist in subinterpreter | Pass data explicitly via channels | Subinterpreters do not share global or module namespaces. |

---

## Performance & Memory Characteristics

### Comparative Metrics: Threads vs Subinterpreters vs Processes

| Concurrency Model | Creation Latency | Memory per Worker | Address Space | Multi-Core CPU Parallelism | Fault Isolation |
|:---|:---|:---|:---|:---|:---|
| **Threads (`threading`)** | **~50 µs** | **~64 KB - 8 MB** | Shared | No (Single GIL in standard build) | Zero (C-crash kills process) |
| **Subinterpreters (PEP 684)** | **~5 - 10 ms** | **~1 - 2 MB** | **Shared (Single PID)** | **Yes (Per-Interpreter GIL!)** | State Isolated (C-crash kills process) |
| **Processes (`multiprocessing`)**| **~150 - 300 ms** | **~25 - 50 MB** | Isolated | **Yes (Separate Processes)** | **Full (Process crash isolated)** |

Subinterpreters provide **25x lower memory overhead** and **30x faster startup times** than separate operating system processes, while unlocking true multi-core CPU parallelism.

---

## CPython Internals & Bytecode

### How the Per-Interpreter GIL Dispatches in C

In CPython 3.12+, the evaluation loop (`Python/ceval.c`) retrieves the GIL pointer from the current thread's active interpreter state rather than a global variable:

```c
// CPython 3.12 ceval.c
static void take_gil(PyThreadState *tstate) {
    // Retrieves the GIL associated strictly with the current interpreter!
    struct _gil_runtime_state *gil = &tstate->interp->_threads.gil;
    
    MUTEX_LOCK(gil->mutex);
    // Thread acquires its private interpreter GIL without locking other subinterpreters!
}
```

Because each `PyInterpreterState` contains its own dedicated `gil` mutex, threads assigned to different subinterpreters execute their CPython bytecode loops in parallel without contention.

---

## Best Practices & Idiomatic Patterns

1. **Use Subinterpreters for Multi-Tenant Untrusted Plugins**: Ideal for SaaS platforms that execute untrusted third-party Python scripts with complete state isolation.
2. **Always Communicate via Channels**: Treat subinterpreters as independent microservices communicating via message passing.
3. **Verify C-Extension Multi-Phase Initialization (PEP 489)**: Ensure external libraries support `PyModInitFunc` and avoid global C variables.
4. **Wrap Lifecycles in `try/finally`**: Always call `_interpreters.destroy()` to prevent memory leaks in long-running processes.

---

## Real-World Case Study: Secure Multi-Tenant Plugin & Code Execution Sandbox

Below is an enterprise plugin runner that dynamically executes third-party user plugins in isolated subinterpreters with strict memory boundaries:

```python
import _interpreters
import time

class PluginExecutionResult:
    def __init__(self, plugin_name: str, output: str, duration_ms: float) -> None:
        self.plugin_name = plugin_name
        self.output = output
        self.duration_ms = duration_ms

class IsolatedPluginHost:
    """Enterprise plugin host executing third-party code in isolated subinterpreters."""

    def execute_plugin(self, plugin_name: str, user_code: str) -> PluginExecutionResult:
        interp_id = _interpreters.create()
        channel_id = _interpreters.channel_create()
        
        t0 = time.perf_counter()
        
        # Inject harness code that captures output and sends over channel:
        harness = f"""
import _interpreters
import sys
from io import StringIO

# Redirect stdout to capture plugin prints
old_stdout = sys.stdout
sys.stdout = buffer = StringIO()

try:
{self._indent_code(user_code)}
    captured = buffer.getvalue().strip()
    _interpreters.channel_send({channel_id}, captured)
finally:
    sys.stdout = old_stdout
"""
        try:
            _interpreters.run_string(interp_id, harness)
            output = _interpreters.channel_recv(channel_id)
        except Exception as exc:
            output = f"EXECUTION_ERROR: {exc}"
        finally:
            elapsed = (time.perf_counter() - t0) * 1000
            _interpreters.channel_destroy(channel_id)
            _interpreters.destroy(interp_id)

        return PluginExecutionResult(plugin_name, output, elapsed)

    @staticmethod
    def _indent_code(code: str) -> str:
        return "\n".join("    " + line for line in code.strip().splitlines())

# Demonstration: Executing two user-submitted plugins with isolated globals
host = IsolatedPluginHost()

plugin_a_code = """
score = 42 * 2
print(f"Plugin A Computed Score: {score}")
"""

plugin_b_code = """
# In plugin B, 'score' from Plugin A is completely invisible!
has_score = 'score' in globals()
print(f"Plugin B Can See Plugin A Score? {has_score}")
"""

res_a = host.execute_plugin("DiscountCalculator", plugin_a_code)
res_b = host.execute_plugin("SecurityScanner", plugin_b_code)

print(f"[{res_a.plugin_name}] ({res_a.duration_ms:.1f}ms): {res_a.output}")
print(f"[{res_b.plugin_name}] ({res_b.duration_ms:.1f}ms): {res_b.output}")
# Output:
# [DiscountCalculator] (...ms): Plugin A Computed Score: 84
# [SecurityScanner] (...ms): Plugin B Can See Plugin A Score? False
```

---

## Interview Questions & Deep-Dive Scenarios

<details>
<summary>1. What is PEP 684 and why was it necessary for subinterpreter parallelism?</summary>

Prior to Python 3.12, all subinterpreters shared a single global GIL located in the process runtime structure. While they had isolated `sys.modules`, threads executing code in different subinterpreters still had to acquire the same mutex, preventing multi-core CPU scaling. PEP 684 relocated the GIL into the per-interpreter state structure, giving each subinterpreter an independent GIL and allowing true parallel execution across CPU cores.
</details>

<details>
<summary>2. How do subinterpreters differ from `multiprocessing`?</summary>

- **Process Model**: `multiprocessing` creates separate operating system processes with separate PIDs and separate memory spaces. Subinterpreters exist within a **single operating system process**.
- **Resource Footprint**: Subinterpreters use 1-2 MB of RAM vs 25-50 MB for an OS process, and start up in ~5ms vs ~150ms.
- **Fault Tolerance**: If a process crashes (e.g. C-level segfault), only that child process dies. If code inside a subinterpreter causes a C-level segfault, the **entire host process dies**.
</details>

<details>
<summary>3. Can two subinterpreters share Python objects directly by pointer?</summary>

No. Sharing raw Python heap objects across subinterpreters would reintroduce reference counting race conditions and violate GIL isolation. Communication must occur through channels or shared memory buffers using serialized or immutable primitive payloads.
</details>

<details>
<summary>4. What is PEP 489 (Multi-Phase Initialization) and why is it critical for subinterpreters?</summary>

Legacy C-extensions used single-phase initialization (`Py_InitModule`), which stored extension state in global static C variables. When multiple subinterpreters import the same C-extension, they mutate the same static C memory, causing crashes. PEP 489 introduces multi-phase initialization, allowing C-extensions to allocate per-interpreter module state structures.
</details>

<details>
<summary>5. What data types can be sent across an inter-interpreter channel?</summary>

Channels currently support immutable primitive types: `int`, `float`, `str`, `bytes`, `None`, and tuples containing only these primitives. Complex mutable objects (`dict`, `list`, custom classes) cannot be sent directly without serialization.
</details>

<details>
<summary>6. How does subinterpreter memory isolation benefit multi-tenant applications?</summary>

Because each subinterpreter gets a completely clean, isolated `sys.modules` and global dictionary, multiple customer scripts can import different versions of modules, mutate module attributes, or set global flags without leaking data or interfering with other tenants in the same server process.
</details>

<details>
<summary>7. What is the status of the `interpreters` module in the standard library?</summary>

In Python 3.12 and 3.13, the low-level API is exposed as `_interpreters` (with a leading underscore indicating an unstable/internal API). PEP 554 proposes standardizing the high-level `interpreters` module for public production use in upcoming Python releases.
</details>

<details>
<summary>8. How do subinterpreters compare to Python 3.13 free-threaded Python (no-GIL)?</summary>

- **Subinterpreters**: Enforce complete memory isolation between interpreters, with each interpreter having its own private GIL. Ideal for independent plugin sandboxes and isolated worker tasks.
- **Free-Threading (`python3.13t`)**: Removes the GIL entirely across all threads in a shared memory heap. Ideal for workloads requiring large, shared data structures (like deep learning tensors and Pandas dataframes).
</details>

---

## Practical Exercises & Code Katas

### Exercise 1: Subinterpreter Math Evaluator
Write a function `eval_in_subinterpreter(expression: str) -> str` that calculates a math string inside an isolated subinterpreter and returns the result string.

<details>
<summary>Solution</summary>

```python
import _interpreters

def eval_in_subinterpreter(expr: str) -> str:
    interp_id = _interpreters.create()
    chan_id = _interpreters.channel_create()
    try:
        code = f"""
import _interpreters
res = str(eval('{expr}'))
_interpreters.channel_send({chan_id}, res)
"""
        _interpreters.run_string(interp_id, code)
        return _interpreters.channel_recv(chan_id)
    finally:
        _interpreters.channel_destroy(chan_id)
        _interpreters.destroy(interp_id)

print("Evaluated result:", eval_in_subinterpreter("40 + 2"))
# Output: Evaluated result: 42
```
</details>

### Exercise 2: State Pollution Verification
Demonstrate that modifying `sys.path` in a subinterpreter leaves the parent process's `sys.path` completely unchanged.

<details>
<summary>Solution</summary>

```python
import _interpreters
import sys

original_path_len = len(sys.path)
interp_id = _interpreters.create()

code = """
import sys
sys.path.append('/custom/fake/directory')
print('Subinterpreter path len:', len(sys.path))
"""
_interpreters.run_string(interp_id, code)

print("Parent process path len:    ", len(sys.path))
assert len(sys.path) == original_path_len
_interpreters.destroy(interp_id)
print("State isolation verified successfully!")
```
</details>

### Exercise 3: Parallel Subinterpreter Hash Compute
Launch 2 subinterpreters on separate threads to calculate SHA-256 hashes of different strings in parallel.

<details>
<summary>Solution</summary>

```python
import _interpreters
import threading

def hash_worker(interp_id: int, payload: str, chan_id: int):
    code = f"""
import _interpreters
import hashlib
digest = hashlib.sha256(b'{payload}').hexdigest()
_interpreters.channel_send({chan_id}, digest)
"""
    _interpreters.run_string(interp_id, code)

interp1 = _interpreters.create()
interp2 = _interpreters.create()
chan1 = _interpreters.channel_create()
chan2 = _interpreters.channel_create()

t1 = threading.Thread(target=hash_worker, args=(interp1, "alpha", chan1))
t2 = threading.Thread(target=hash_worker, args=(interp2, "beta", chan2))

t1.start(); t2.start()
t1.join(); t2.join()

print("Hash 1:", _interpreters.channel_recv(chan1)[:16])
print("Hash 2:", _interpreters.channel_recv(chan2)[:16])

_interpreters.channel_destroy(chan1); _interpreters.channel_destroy(chan2)
_interpreters.destroy(interp1); _interpreters.destroy(interp2)
```
</details>

### Exercise 4: Clean Subinterpreter Context Manager
Implement an asynchronous or synchronous context manager `managed_subinterpreter()` that automatically creates and destroys subinterpreters.

<details>
<summary>Solution</summary>

```python
from contextlib import contextmanager
import _interpreters
from typing import Iterator

@contextmanager
def managed_subinterpreter() -> Iterator[int]:
    interp_id = _interpreters.create()
    try:
        yield interp_id
    finally:
        _interpreters.destroy(interp_id)

with managed_subinterpreter() as iid:
    _interpreters.run_string(iid, "print('Running inside managed context!')")
```
</details>

### Exercise 5: Channel Buffer Overload Test
Demonstrate sending and receiving multiple messages through an inter-interpreter channel.

<details>
<summary>Solution</summary>

```python
import _interpreters

cid = _interpreters.channel_create()
for i in range(3):
    _interpreters.channel_send(cid, f"MSG_{i}")

for _ in range(3):
    print("Popped from channel:", _interpreters.channel_recv(cid))

_interpreters.channel_destroy(cid)
# Output:
# Popped from channel: MSG_0
# Popped from channel: MSG_1
# Popped from channel: MSG_2
```
</details>

### Exercise 6: Verifying Subinterpreter Thread Identities
Verify that threads executing within subinterpreters report valid thread identifiers.

<details>
<summary>Solution</summary>

```python
import _interpreters
import threading

interp = _interpreters.create()
code = """
import threading
print('Subinterpreter Thread Ident:', threading.get_ident())
"""
t = threading.Thread(target=lambda: _interpreters.run_string(interp, code))
t.start()
t.join()
_interpreters.destroy(interp)
```
</details>
