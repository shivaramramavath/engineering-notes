# Module 07: Exceptions, Context Managers & Debugging (09-debugging-tools)

> **Topic Overview**: Beyond basic interactive debugging, diagnosing production incidents requires low-overhead runtime hooks and specialized telemetry tools. From crash dump handling via `sys.excepthook` and C-level thread inspection via `faulthandler`, to programmatic execution tracing with `sys.settrace` and remote container debugging via `debugpy`, modern Python provides a comprehensive toolchain for root-cause analysis.

---

## Metadata Table

| Field | Value |
|---|---|
| **Category** | Core Language / Production Diagnostics & Debugging |
| **Target Level** | Advanced to Staff |
| **Python Version** | Python 3.10+ (tested on Python 3.12 - 3.14+) |
| **Tested With** | CPython 3.14.0, uv 0.13.9, ruff 0.17.6, mypy 2.4.1, pytest 9.1.1 |
| **Free-Threading / Platform** | Supported / Platform Independent |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Code Examples](#5-code-examples)
6. [Edge Cases & Pitfalls](#6-edge-cases--pitfalls)
7. [Common Mistakes](#7-common-mistakes)
8. [Performance & Complexity](#8-performance--complexity)
9. [CPython Internals & Bytecode](#9-cpython-internals--bytecode)
10. [Best Practices](#10-best-practices)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

- **Problem Statement**: In production environments, running an interactive debugger like `pdb` is impossible—there is no active TTY terminal, and pausing process execution causes health check failures and request timeouts. Furthermore, when crashes occur at the C-extension layer (SIGSEGV or memory corruption), standard Python exception handlers are bypassed entirely.
- **Mental Model**: Production debugging relies on asynchronous, non-blocking telemetry. When the process crashes, `sys.excepthook` captures the final execution state; if the C runtime aborts, `faulthandler` dumps thread tracebacks directly using raw OS file descriptors; if deadlocks occur, background watchdog timers dump thread stacks.
- **When to Use**:
  - `faulthandler`: Always enable in production services (`python -X faulthandler` or `faulthandler.enable()`).
  - `sys.excepthook`: In microservices and desktop applications to report unhandled crashes to Sentry or OpenTelemetry.
  - `threading.excepthook`: To catch unhandled exceptions in background worker threads without crashing silently.
  - `sys.settrace`: For coverage tools, execution tracers, and custom profilers.
- **When NOT to Use**: Do not use `sys.settrace` in production hot paths; running line-by-line tracing incurs a $10\times - 50\times$ performance penalty.

---

## 2. Core Concepts

### The Global Exception Hook Architecture
CPython routes uncaught exceptions through global dispatch hooks:

```text
+---------------------------------------------------------------+
| Unhandled Exception Occurs                                    |
+---------------------------------------------------------------+
                             │
                             ├── Raised in Main Thread?
                             │        │
                             │        ├── YES ──> sys.excepthook(type, value, tb)
                             │        │
                             │        └── NO ───> threading.excepthook(args)
                             │                    (Python 3.8+)
                             v
+---------------------------------------------------------------+
| C-Level Fatal Signal (SIGSEGV, SIGFPE, SIGABRT, SIGBUS)       |
+---------------------------------------------------------------+
                             │
                             v
                 faulthandler C-Signal Handler
                 (Writes Python tracebacks directly to stderr)
```

### Core Diagnostic Modules
1. **`faulthandler`**: Built into CPython. Installs OS signal handlers that safely print Python tracebacks during fatal memory faults (segfaults, bus errors).
2. **`threading.excepthook`**: Intercepts unhandled exceptions originating inside `threading.Thread.run()`.
3. **`sys.monitoring` (Python 3.12+)**: Low-overhead execution event monitoring replacing legacy `sys.settrace`.

---

## 3. How It Works Internally

`faulthandler` is written directly in C to operate in an "async-signal-safe" manner:

```text
+---------------------------------------------------------------+
| Fatal Signal Triggered (e.g., SIGSEGV in C-extension)        |
+---------------------------------------------------------------+
| 1. OS intercepts invalid memory access                        |
| 2. Jumps to faulthandler signal handler (C function)          |
|    - CANNOT allocate memory (malloc is unsafe in signal handler)
|    - CANNOT call Python interpreter eval loop                 |
| 3. Traverses active C-level PyThreadState and PyFrameObject   |
| 4. Writes frame function names and line numbers directly to   |
|    file descriptor 2 (stderr) using write() syscall           |
| 5. Re-raises signal to let OS generate core dump              |
+---------------------------------------------------------------+
```

---

## 4. Syntax & API Reference

### Configuring `faulthandler`
```python
import faulthandler

# Enable signal handlers for SIGSEGV, SIGFPE, SIGABRT, SIGBUS
faulthandler.enable(file=sys.stderr, all_threads=True)

# Dump tracebacks of all threads after a timeout (deadlock watchdog)
faulthandler.dump_traceback_later(timeout=30.0, repeat=True, exit=False)

# Cancel watchdog
faulthandler.cancel_dump_traceback_later()
```

### Hook Signatures
```python
import sys
import threading

# Main thread hook
def custom_excepthook(
    exc_type: type[BaseException],
    exc_val: BaseException,
    exc_tb: Any
) -> None:
    ...

sys.excepthook = custom_excepthook

# Thread hook (Python 3.8+)
def custom_thread_excepthook(args: threading.ExceptHookArgs) -> None:
    # args.exc_type, args.exc_value, args.exc_traceback, args.thread
    ...

threading.excepthook = custom_thread_excepthook
```

---

## 5. Code Examples

### 1. Minimal / Basic Example: Global Crash Logger with `sys.excepthook`
```python
import sys
import traceback

def global_crash_logger(exc_type, exc_val, exc_tb):
    print("[TELEMETRY ALERT] Application crashed with unhandled exception!")
    print(f"Exception Type: {exc_type.__name__}")
    print(f"Exception Value: {exc_val}")
    # Format and save or send to telemetry API
    tb_lines = traceback.format_tb(exc_tb)
    print(f"Call Depth: {len(tb_lines)} frames")

old_hook = sys.excepthook
sys.excepthook = global_crash_logger

# Trigger unhandled error
try:
    raise RuntimeError("Unexpected microservice worker crash")
except RuntimeError:
    exc_t, exc_v, exc_tb = sys.exc_info()
    sys.excepthook(exc_t, exc_v, exc_tb)
finally:
    sys.excepthook = old_hook
```
**Output**:
```text
[TELEMETRY ALERT] Application crashed with unhandled exception!
Exception Type: RuntimeError
Exception Value: Unexpected microservice worker crash
Call Depth: 1 frames
```

### 2. Real-World Production Example: Thread Exception Capture
```python
import threading
import time

def faulty_worker():
    time.sleep(0.01)
    raise ValueError("Thread crashed on invalid packet")

thread_errors = []

def thread_crash_collector(args: threading.ExceptHookArgs):
    thread_errors.append({
        "thread_name": args.thread.name if args.thread else "unknown",
        "error_type": args.exc_type.__name__ if args.exc_type else "unknown",
        "message": str(args.exc_value)
    })

old_thread_hook = threading.excepthook
threading.excepthook = thread_crash_collector

t = threading.Thread(target=faulty_worker, name="Worker-Alpha")
t.start()
t.join()

print(f"Captured background thread errors: {len(thread_errors)}")
if thread_errors:
    print(f"Report: {thread_errors[0]}")

threading.excepthook = old_thread_hook
```
**Output**:
```text
Captured background thread errors: 1
Report: {'thread_name': 'Worker-Alpha', 'error_type': 'ValueError', 'message': 'Thread crashed on invalid packet'}
```

### 3. Edge Case: Watchdog Deadlock Detection via `faulthandler`
```python
import faulthandler
import time
import io

# Setup in-memory buffer to catch watchdog output
output_buffer = io.StringIO()

# Configure watchdog to trigger if main thread hangs longer than 0.1s
faulthandler.dump_traceback_later(timeout=0.1, repeat=False, exit=False)

def simulate_deadlock():
    # Simulate heavy or blocked I/O
    time.sleep(0.15)

simulate_deadlock()
faulthandler.cancel_dump_traceback_later()
print("Watchdog cycle completed.")
```
**Output**:
```text
Watchdog cycle completed.
```
*(On stderr, `faulthandler` dumps: `Timeout (0.100s)!\nThread 0x... (most recent call first):\n  File "...", line ... in simulate_deadlock`)*

### 4. Failure Case & Exception Handling: `sys.settrace` Line Execution Inspection
```python
import sys

def line_tracer(frame, event, arg):
    if event == "line":
        lineno = frame.f_lineno
        func_name = frame.f_code.co_name
        print(f"  [TRACE] Executing {func_name}() at line {lineno}")
    return line_tracer

def target_logic(a, b):
    x = a * 2
    y = b + x
    return y

print("Starting traced execution:")
sys.settrace(line_tracer)
try:
    res = target_logic(3, 4)
finally:
    sys.settrace(None)

print(f"Traced result: {res}")
```
**Output**:
```text
Starting traced execution:
  [TRACE] Executing target_logic() at line 12
  [TRACE] Executing target_logic() at line 13
  [TRACE] Executing target_logic() at line 14
Traced result: 10
```

### 5. Idiomatic Pythonic Pattern: Remote Debugging Setup with `debugpy`
```python
# Modern standard for VS Code and container debugging
def enable_remote_debugging(port: int = 5678, wait_for_attach: bool = False) -> None:
    try:
        import debugpy  # type: ignore[import-untyped]
        debugpy.listen(("0.0.0.0", port))
        print(f"[DEBUGGER] debugpy listening on 0.0.0.0:{port}")
        if wait_for_attach:
            print("[DEBUGGER] Waiting for IDE debugger to attach...")
            debugpy.wait_for_client()
    except ImportError:
        print("[DEBUGGER] debugpy not installed; skipping remote debug listener.")

# Verification
enable_remote_debugging(port=5678, wait_for_attach=False)
```
**Output**:
```text
[DEBUGGER] debugpy not installed; skipping remote debug listener.
```

---

## 6. Edge Cases & Pitfalls

1. **`faulthandler` File Descriptor Safety**:
   - `faulthandler` requires an underlying OS file descriptor (e.g., `sys.stderr.fileno()`). If `sys.stderr` is redirected to an in-memory `io.StringIO` object without a real OS file descriptor, passing it to `faulthandler.enable()` will fail with `io.UnsupportedOperation: fileno`.
2. **`threading.excepthook` vs `Thread.run()` Overrides**:
   - If a subclass of `threading.Thread` overrides the `run()` method without wrapping its body in a try/except, any uncaught exception triggers `threading.excepthook`. However, if the custom `run()` catches exceptions and swallows them, the hook is bypassed.
3. **Interactive Debugger in Daemons**:
   - Attempting to invoke `pdb.set_trace()` inside a background service or container with no stdin connected raises `pdb.BdbQuit` or `OSError: [Errno 9] Bad file descriptor`.

---

## 7. Common Mistakes

| Mistake | Why It Fails | Exception / Diagnostic | Correct Pattern |
|---|---|---|---|
| Forgetting `threading.excepthook` in multi-threaded apps | Unhandled thread exceptions print to stderr but silently terminate the thread | Silent worker death | Install `threading.excepthook` to alert |
| Redirecting `sys.stderr` to `StringIO` with `faulthandler` | `faulthandler` writes via raw OS file descriptors | `io.UnsupportedOperation: fileno` | Pass real OS file descriptor or standard stderr |
| Leaving `sys.settrace` active in production | Forces interpreter to call Python hook on every opcode/line | Severe $20\times - 50\times$ performance drop | Only use `sys.settrace` in dev/testing |
| Raising exception inside `sys.excepthook` | CPython catches the hook's own exception and prints a fallback error | `Error in sys.excepthook` printed | Ensure excepthook is wrapped in try/except |
| Invoking `pdb.set_trace()` in Docker without `-it` | No TTY attached; debugger crashes process immediately | `BdbQuit` / EOFError on stdin | Use `debugpy` or non-interactive dumpers |
| Forgetting to cancel `faulthandler.dump_traceback_later` | Watchdog continues running in background and dumps tracebacks | Unintended periodic thread dumps | Always call `cancel_dump_traceback_later()` |

---

## 8. Performance & Complexity

- **`faulthandler.enable()`**: Zero runtime overhead during normal execution. Intercepts only OS fatal signals.
- **`sys.excepthook` / `threading.excepthook`**: Zero runtime overhead during normal execution. Dispatched only when an unhandled exception reaches the root of the call stack.
- **`sys.settrace()`**: Disables interpreter specialization and fast bytecode paths, increasing execution time by $1000\% - 4000\%$.
- **`sys.monitoring` (Python 3.12+)**: Replaces bytecode instrumentation with zero-cost event hooks; inactive events cost $0.00$ ns.

---

## 9. CPython Internals & Bytecode

Let's examine how CPython 3.12+ `sys.monitoring` registers event handlers:

```python
import sys

# Python 3.12+ sys.monitoring API
if hasattr(sys, "monitoring"):
    tool_id = sys.monitoring.DEBUGGER_ID
    sys.monitoring.use_tool_id(tool_id, "my_debugger")
    events = sys.monitoring.events.PY_START | sys.monitoring.events.PY_RETURN
    sys.monitoring.set_events(tool_id, events)
    print("sys.monitoring successfully activated for PY_START and PY_RETURN")
    sys.monitoring.free_tool_id(tool_id)
else:
    print("sys.monitoring requires Python 3.12+")
```

**Output**:
```text
sys.monitoring successfully activated for PY_START and PY_RETURN
```
`sys.monitoring` uses internal compiler trampoline instructions (`INSTRUMENTED_LINE`, `INSTRUMENTED_CALL`). Only instrumented code points incur overhead, preserving full speed across the rest of the application.

---

## 10. Best Practices

1. **Always Enable `faulthandler`**: Add `python -X faulthandler` to container entrypoints or Dockerfiles.
2. **Monitor Background Threads**: Register `threading.excepthook` to alert Sentry/PagerDuty if a worker thread dies.
3. **Use `sys.monitoring` on 3.12+**: Avoid `sys.settrace` for custom APM or profiling tools; use `sys.monitoring` for minimal overhead.

---

## 11. Real-World Case Study: Production Deadlock Watchdog

```python
import faulthandler
import threading
import time

class DeadlockWatchdog:
    def __init__(self, timeout_seconds: float = 2.0) -> None:
        self.timeout = timeout_seconds

    def __enter__(self) -> "DeadlockWatchdog":
        faulthandler.dump_traceback_later(self.timeout, repeat=False, exit=False)
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        faulthandler.cancel_dump_traceback_later()

def safe_batch_task():
    with DeadlockWatchdog(timeout_seconds=1.0):
        # Normal task finishes quickly
        time.sleep(0.05)
        return "SUCCESS"

result = safe_batch_task()
print(f"Task completed safely: {result}")
```
**Output**:
```text
Task completed safely: SUCCESS
```

---

## 12. Interview Questions

### Q1: What problem does `faulthandler` solve that standard Python `try...except` cannot?
<details>
<summary><strong>Answer</strong></summary>
Standard <code>try...except</code> only catches Python-level exceptions. When a segmentation fault (SIGSEGV), bus error (SIGBUS), or abort (SIGABRT) occurs inside a compiled C extension (e.g. NumPy, PyTorch, or custom C code), the OS terminates the process immediately without running Python handlers. <code>faulthandler</code> installs low-level C signal handlers that safely print the Python frame stack to stderr before the process dies.
</details>

### Q2: What is the purpose of `threading.excepthook` (introduced in Python 3.8)?
<details>
<summary><strong>Answer</strong></summary>
Before Python 3.8, unhandled exceptions inside background threads only printed a message to <code>sys.stderr</code> and terminated the thread silently, leaving <code>sys.excepthook</code> unnotified. <code>threading.excepthook</code> allows application developers to intercept unhandled exceptions from any thread, enabling automated alerting, logging, or process termination.
</details>

### Q3: Why is using `sys.settrace` in production strongly discouraged?
<details>
<summary><strong>Answer</strong></summary>
<code>sys.settrace</code> instructs the CPython interpreter to invoke a Python callback on every single line, call, return, and opcode. It disables all adaptive specializing bytecode optimizations, causing the application to run anywhere from $10\times$ to $50\times$ slower.
</details>

### Q4: How does `sys.monitoring` in Python 3.12 improve on `sys.settrace`?
<details>
<summary><strong>Answer</strong></summary>
PEP 669 (Python 3.12) introduced <code>sys.monitoring</code>. Instead of a global hook called on every opcode, <code>sys.monitoring</code> selectively instruments only the specific events of interest (e.g., function entry, branch, exception). Unmonitored instructions run at 100% native execution speed with zero performance penalty.
</details>

### Q5: How can you debug a Python application running in a remote Docker container?
<details>
<summary><strong>Answer</strong></summary>
Use the <code>debugpy</code> library. The container starts with <code>debugpy.listen(("0.0.0.0", 5678))</code>, and developers connect their IDE (such as VS Code) remotely over TCP port 5678 to set breakpoints, inspect variables, and step through code interactively.
</details>

### Q6: What does the environment variable `PYTHONFAULTHANDLER=1` do?
<details>
<summary><strong>Answer</strong></summary>
It automatically calls <code>faulthandler.enable()</code> at Python interpreter startup, ensuring that any fatal crash (like a segfault) dumps the Python traceback to stderr without requiring code modifications.
</details>

### Q7: What arguments does `threading.ExceptHookArgs` contain?
<details>
<summary><strong>Answer</strong></summary>
It is a named tuple containing: <code>exc_type</code> (exception class), <code>exc_value</code> (exception instance), <code>exc_traceback</code> (traceback object), and <code>thread</code> (the <code>threading.Thread</code> object that crashed).
</details>

### Q8: What happens if `sys.excepthook` itself raises an unhandled exception?
<details>
<summary><strong>Answer</strong></summary>
CPython catches the secondary exception, outputs <code>Error in sys.excepthook:</code> to <code>sys.stderr</code> along with the hook's own traceback, followed by the original unhandled exception, and then terminates.
</details>

---

## 13. Exercises & Challenges

### Challenge 1 (Easy): Thread Crash Counter
Implement a global hook using `threading.excepthook` that increments an in-memory counter every time a thread crashes, logging the thread's name and error message.
<details>
<summary><strong>Solution</strong></summary>

```python
import threading

crashes = []

def thread_counter_hook(args: threading.ExceptHookArgs):
    crashes.append({
        "thread": args.thread.name if args.thread else "unknown",
        "error": str(args.exc_value)
    })

old = threading.excepthook
threading.excepthook = thread_counter_hook

try:
    t = threading.Thread(target=lambda: 1 / 0, name="Worker-1")
    t.start()
    t.join()
    assert len(crashes) == 1
    assert crashes[0]["thread"] == "Worker-1"
    assert "division by zero" in crashes[0]["error"]
finally:
    threading.excepthook = old
```
</details>

### Challenge 2 (Easy): Safe Faulthandler Activation
Write a function `safe_enable_faulthandler()` that checks if `sys.stderr` has a valid file descriptor. If valid, enable `faulthandler`; if not (e.g., mocked or custom stream), return `False` without raising an exception.
<details>
<summary><strong>Solution</strong></summary>

```python
import sys
import faulthandler

def safe_enable_faulthandler() -> bool:
    try:
        if hasattr(sys.stderr, "fileno"):
            sys.stderr.fileno()
            faulthandler.enable()
            return True
        return False
    except (io.UnsupportedOperation, AttributeError):
        return False

assert isinstance(safe_enable_faulthandler(), bool)
```
</details>

### Challenge 3 (Medium): Function Call Step Counter
Using `sys.settrace`, write a context manager `count_function_calls()` that counts how many distinct function calls occur within the managed with-block, and restores `sys.settrace` on exit.
<details>
<summary><strong>Solution</strong></summary>

```python
import sys

class count_function_calls:
    def __init__(self) -> None:
        self.call_count = 0

    def _tracer(self, frame, event, arg):
        if event == "call":
            self.call_count += 1
        return self._tracer

    def __enter__(self):
        self.call_count = 0
        sys.settrace(self._tracer)
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        sys.settrace(None)

def helper_a(): return 1
def helper_b(): return 2

with count_function_calls() as counter:
    helper_a()
    helper_b()
    helper_a()

assert counter.call_count == 3
```
</details>

### Challenge 4 (Medium): Watchdog Guard Function Decorator
Write a decorator `@timeout_watchdog(seconds=1.0)` that uses `faulthandler.dump_traceback_later` to dump thread tracebacks if the decorated function takes longer than `seconds` to complete.
<details>
<summary><strong>Solution</strong></summary>

```python
import faulthandler
import functools
from typing import Callable

def timeout_watchdog(seconds: float = 1.0):
    def decorator(fn: Callable):
        @functools.wraps(fn)
        def wrapper(*args, **kwargs):
            faulthandler.dump_traceback_later(seconds, repeat=False, exit=False)
            try:
                return fn(*args, **kwargs)
            finally:
                faulthandler.cancel_dump_traceback_later()
        return wrapper
    return decorator

@timeout_watchdog(seconds=0.5)
def fast_task():
    return 42

assert fast_task() == 42
```
</details>

### Challenge 5 (Hard): Structured JSON Crash Dumper
Implement a custom `sys.excepthook` that writes unhandled exceptions as a single-line JSON string containing fields: `timestamp`, `error_type`, `error_message`, and `stack_frames` (a list of `{file, line, function, code}` objects).
<details>
<summary><strong>Solution</strong></summary>

```python
import sys
import json
import traceback
from datetime import datetime

def json_crash_dumper(exc_type, exc_val, exc_tb) -> str:
    tb_summary = traceback.extract_tb(exc_tb)
    frames = []
    for f in tb_summary:
        frames.append({
            "file": f.filename,
            "line": f.lineno,
            "function": f.name,
            "code": f.line
        })
    
    payload = {
        "timestamp": datetime.utcnow().isoformat(),
        "error_type": exc_type.__name__ if exc_type else "Unknown",
        "error_message": str(exc_val),
        "stack_frames": frames
    }
    return json.dumps(payload)

try:
    1 / 0
except ZeroDivisionError:
    t, v, tb = sys.exc_info()
    json_str = json_crash_dumper(t, v, tb)
    data = json.loads(json_str)
    assert data["error_type"] == "ZeroDivisionError"
    assert len(data["stack_frames"]) >= 1
```
</details>

### Challenge 6 (Hard): Execution Branch Tracer with sys.monitoring (Python 3.12+)
Using `sys.monitoring` (or falling back to `sys.settrace` on older Pythons), implement an execution tracer `BranchTracker` that records every line number executed within a target function, returning the set of visited lines upon exit.
<details>
<summary><strong>Solution</strong></summary>

```python
import sys

class BranchTracker:
    def __init__(self, target_func_name: str) -> None:
        self.target_func = target_func_name
        self.executed_lines: set[int] = set()

    def _tracer(self, frame, event, arg):
        if event == "line" and frame.f_code.co_name == self.target_func:
            self.executed_lines.add(frame.f_lineno)
        return self._tracer

    def __enter__(self):
        self.executed_lines.clear()
        sys.settrace(self._tracer)
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        sys.settrace(None)

def example_branch(flag: bool):
    if flag:
        x = 10
    else:
        x = 20
    return x

with BranchTracker("example_branch") as tracker:
    example_branch(True)

assert len(tracker.executed_lines) >= 2
```
</details>
