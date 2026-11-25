# Module 08: Iterators, Generators & Functional Programming (08-coroutines-via-generators-history)

> **Topic Overview**: Python's modern `async`/`await` syntax is the culmination of a twenty-year architectural evolution rooted in generators. Understanding this progression—from PEP 342's `.send()` coroutines and PEP 380's `yield from` delegation, to Python 3.4's `@asyncio.coroutine` and Python 3.5's native `async def` (PEP 492)—provides deep insight into event loops, coroutine frame scheduling, and modern async runtimes.

---

## Metadata Table

| Field | Value |
|---|---|
| **Category** | Core Language / Language History & Async Evolution |
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

- **Problem Statement**: Threads incur heavy OS context-switching costs, memory overhead per stack (often 8 MB default), and synchronization deadlocks. Developers needed cooperative multitasking inside a single thread, but early Python lacked language constructs for pausing and resuming execution with two-way communication.
- **Mental Model**:
  - **Phase 1 (Python 2.2, PEP 255)**: Generators can only produce values via `yield`. One-way output.
  - **Phase 2 (Python 2.5, PEP 342)**: Generators can receive values via `.send()`. Two-way communication enabled generator-based coroutines.
  - **Phase 3 (Python 3.3, PEP 380)**: `yield from` allows chaining sub-generators and capturing return values.
  - **Phase 4 (Python 3.4, PEP 3156)**: Early `asyncio` uses `@asyncio.coroutine` and `yield from`.
  - **Phase 5 (Python 3.5+, PEP 492)**: Native coroutines (`async def`, `await`) separate async code from simple sequence generation once and for all.
- **When to Use**: Understanding this history is vital for reading legacy codebases, designing event loop frameworks, understanding why `await` behaves like `yield from`, and debugging coroutine object lifecycle errors.
- **When NOT to Use**: Never write new asynchronous code using generator-based coroutines (`@asyncio.coroutine` or `yield from`); generator-based coroutines were completely removed in Python 3.11.

---

## 2. Core Concepts

### Timeline of Asynchronous Evolution in Python

```text
+-------------------------------------------------------------------------------+
| Python Asynchronous Evolution Timeline                                        |
+-------------------------------------------------------------------------------+
| 2001 (Python 2.2) | PEP 255: Simple Generators (yield item)                  |
|                   | - One-way sequence producer                              |
|                   |                                                           |
| 2005 (Python 2.5) | PEP 342: Coroutines via Enhanced Generators               |
|                   | - Added .send(val), .throw(exc), .close()                |
|                   | - First cooperative multitasking in Twisted/Tornado       |
|                   |                                                           |
| 2009 (Python 3.3) | PEP 380: Syntax for Delegating to a Subgenerator         |
|                   | - yield from iterable                                     |
|                   | - Transparent forwarding of send/throw and return values  |
|                   |                                                           |
| 2014 (Python 3.4) | PEP 3156: Asynchronous I/O Support (asyncio)             |
|                   | - @asyncio.coroutine decorator with yield from            |
|                   |                                                           |
| 2015 (Python 3.5) | PEP 492: Coroutines with async and await Syntax           |
|                   | - Dedicated async def and await keywords                  |
|                   | - Complete separation of generators and native coroutines |
|                   |                                                           |
| 2022 (Python 3.11)| Legacy generator-based coroutines completely REMOVED      |
+-------------------------------------------------------------------------------+
```

### Generator Coroutine vs Native Coroutine
```text
+-----------------------+-----------------------------+-------------------------+
| Property              | Generator-Based (Historic)  | Native Coroutine (Modern)|
+-----------------------+-----------------------------+-------------------------+
| Declaration           | def foo(): yield            | async def foo(): await  |
| Invocation Type       | types.GeneratorType         | types.CoroutineType     |
| Yield Capability      | Can yield values to caller  | Cannot use yield*       |
| Iteration             | Iterable via for / next()   | Not iterable with next()|
| Introspection         | inspect.isgenerator()       | inspect.iscoroutine()   |
+-----------------------+-----------------------------+-------------------------+
* Unless declared as an async generator via 'async def' with 'yield'.
```

---

## 3. How It Works Internally

Under the hood, native coroutines (`async def`) and generators share CPython's internal frame suspension infrastructure:

```text
+---------------------------------------------------------------+
| CPython PyCoroutineObject vs PyGenObject                      |
+---------------------------------------------------------------+
| Both wrap a PyFrameObject on the heap.                        |
|                                                               |
| 1. 'await future' in bytecode compiles to:                    |
|    - GET_AWAITABLE (verifies object implements __await__)     |
|    - LOAD_CONST None                                          |
|    - SEND                                                     |
|    - YIELD_VALUE (suspends coroutine frame back to event loop)|
|                                                               |
| 2. Event loop (selector/epoll) waits for I/O socket readiness |
| 3. When socket is ready, event loop calls coroutine.send(res) |
| 4. Coroutine frame resumes, reads 'res', and continues!       |
+---------------------------------------------------------------+
```

---

## 4. Syntax & API Reference

### Simulating an Event Loop with Historical Generators (Python 2.5 / 3.3 Style)
```python
def task(name: str, steps: int):
    for i in range(1, steps + 1):
        print(f"[{name}] Step {i}")
        yield  # Yields control back to the scheduler

def mini_scheduler(*tasks):
    queue = list(tasks)
    while queue:
        current = queue.pop(0)
        try:
            next(current)
            queue.append(current)  # Re-enqueue paused task
        except StopIteration:
            pass  # Task completed
```

---

## 5. Code Examples

### 1. Minimal / Basic Example: Toy Cooperative Multitasking Scheduler
```python
from collections import deque
from typing import Generator

def worker(task_name: str, iterations: int) -> Generator[None, None, None]:
    for i in range(1, iterations + 1):
        print(f"Task '{task_name}': step {i}/{iterations}")
        yield  # Cooperative yield of CPU time

def event_loop(*tasks: Generator[None, None, None]) -> None:
    task_queue = deque(tasks)
    while task_queue:
        current_task = task_queue.popleft()
        try:
            next(current_task)
            task_queue.append(current_task)
        except StopIteration:
            pass

# Run two cooperative tasks concurrently in a single thread
event_loop(worker("Alpha", 3), worker("Beta", 2))
```
**Output**:
```text
Task 'Alpha': step 1/3
Task 'Beta': step 1/2
Task 'Alpha': step 2/3
Task 'Beta': step 2/2
Task 'Alpha': step 3/3
```

### 2. Real-World Production Example: The `__await__` Protocol Under the Hood
```python
import inspect

class CustomAwaitable:
    """Demonstrates how native 'await' delegates to the iterator protocol!"""
    def __init__(self, value: str) -> None:
        self.value = value

    def __await__(self):
        # PEP 492: __await__ MUST return an iterator!
        print("[AWAITABLE] Pausing coroutine frame via __await__ yield")
        yield f"SUSPENDED_WITH_{self.value}"
        print("[AWAITABLE] Resumed coroutine frame")
        return f"RESULT_{self.value}"

async def main_coroutine():
    print("1. Starting native coroutine")
    res = await CustomAwaitable("TOKEN_101")
    print(f"2. Resumed with: {res}")
    return "DONE"

# Drive the native coroutine manually just like an old generator!
coro = main_coroutine()
print("Coro type:", type(coro).__name__)
assert inspect.iscoroutine(coro)

# Step 1: Advance to awaitable yield
suspended_val = coro.send(None)
print("Event loop received suspended value:", suspended_val)

# Step 2: Resume with result
try:
    coro.send(None)
except StopIteration as return_event:
    print("Coroutine completed with final return:", return_event.value)
```
**Output**:
```text
Coro type: coroutine
1. Starting native coroutine
[AWAITABLE] Pausing coroutine frame via __await__ yield
Event loop received suspended value: SUSPENDED_WITH_TOKEN_101
[AWAITABLE] Resumed coroutine frame
2. Resumed with: RESULT_TOKEN_101
Coroutine completed with final return: DONE
```

### 3. Edge Case: Treating Native Coroutines as Iterables
```python
async def sample_async():
    return 42

coro = sample_async()

try:
    # Native coroutines do NOT implement the iterable protocol!
    next(coro)  # type: ignore[call-overload]
except TypeError as err:
    print(f"Captured expected TypeError: {err}")

# Coroutine must be awaited or driven via send():
try:
    coro.send(None)
except StopIteration as ret:
    print(f"Completed via send(None) -> result: {ret.value}")
```
**Output**:
```text
Captured expected TypeError: 'coroutine' object is not an iterator
Completed via send(None) -> result: 42
```

### 4. Failure Case & Exception Handling: Unawaited Coroutine Warning
```python
import warnings

async def critical_cleanup():
    print("Performing critical database rollback")

with warnings.catch_warnings(record=True) as recorded:
    warnings.simplefilter("always")

    # Calling an async function without awaiting it only CREATES the coroutine object!
    coro_obj = critical_cleanup()
    del coro_obj  # Garbage collected without ever running!

if recorded:
    w = recorded[0]
    print(f"Warning issued: {w.category.__name__}")
    print(f"Message: {w.message}")
```
**Output**:
```text
Warning issued: RuntimeWarning
Message: coroutine 'critical_cleanup' was never awaited
```

### 5. Idiomatic Pythonic Pattern: Bridging Generators to Async Streams
```python
import asyncio
from typing import AsyncIterator

async def async_counter(limit: int) -> AsyncIterator[int]:
    """Modern Python 3.6+ asynchronous generator."""
    for i in range(1, limit + 1):
        await asyncio.sleep(0.01)
        yield i * 10

async def main():
    results = []
    # Consume async generator via async for
    async for val in async_counter(3):
        results.append(val)
    print("Async generator results:", results)

asyncio.run(main())
```
**Output**:
```text
Async generator results: [10, 20, 30]
```

---

## 6. Edge Cases & Pitfalls

1. **`yield from` Is NOT Allowed in `async def`**:
   - Inside a native coroutine (`async def`), using `yield from` raises `SyntaxError: 'yield from' inside async function`. You must use `await`.
2. **`await` Requires an Awaitable**:
   - Writing `await [1, 2, 3]` raises `TypeError: object list can't be used in 'await' expression`. An object is only awaitable if it is a native coroutine, an object with an `__await__()` method, or a task/future.
3. **The Unawaited Coroutine Hazard**:
   - Calling an `async def` function creates a coroutine object on the heap but **executes zero code**. If you forget `await`, the body never runs, and Python issues a `RuntimeWarning: coroutine was never awaited`.

---

## 7. Common Mistakes

| Mistake | Why It Fails | Exception / Diagnostic | Correct Pattern |
|---|---|---|---|
| Calling `async def` function without `await` | Returns coroutine object; function body never executes | `RuntimeWarning: coroutine was never awaited` | `await my_func()` |
| Using `yield from` inside `async def` | Native coroutines do not support generator delegation syntax | `SyntaxError: 'yield from' inside async function` | Use `await` |
| Calling `next()` on a native coroutine | Native coroutines are not iterators; they lack `__next__` | `TypeError: 'coroutine' object is not an iterator` | Drive via `send(None)` or `await` |
| Using legacy `@asyncio.coroutine` in Python 3.11+ | Generator-based coroutine decorator was removed in Python 3.11 | `AttributeError: module 'asyncio' has no attribute 'coroutine'` | Use `async def` |
| Yielding values inside `async def` expecting normal return | Transforms the coroutine into an `async_generator` | `TypeError: object async_generator can't be used in 'await'` | Use `async for` or return scalar |
| Blocking the event loop with `time.sleep()` | Pauses entire OS thread; starves all concurrent coroutines | Latency spike / frozen event loop | Use `await asyncio.sleep()` |

---

## 8. Performance & Complexity

- **Coroutine Frame Allocation**: Allocating a `PyCoroutineObject` on the heap takes $\approx 40-70$ ns.
- **Suspension / Resume**: Suspending an `await` and resuming it via `.send()` costs $\approx 30$ ns (orders of magnitude faster than OS thread context switches, which cost $1-5\ \mu\text{s}$).
- **Memory Footprint**: A native coroutine frame consumes $\approx 1$ KB of RAM, allowing a single Python process to comfortably hold 100,000+ concurrent idle connections.

---

## 9. CPython Internals & Bytecode

Let's compare the disassembly of `await` vs `yield from`:

```python
import dis

async def native_coro(fut):
    return await fut

dis.dis(native_coro)
```

**Bytecode output**:
```text
  1           0 RESUME                   0

  2           2 LOAD_FAST                0 (fut)
              4 GET_AWAITABLE            0
              6 LOAD_CONST               0 (None)
        >>    8 SEND                     3 (to 16)
             10 YIELD_VALUE              1
             12 RESUME                   1
             14 JUMP_BACKWARD            4 (to 8)
        >>   16 RETURN_VALUE
```
Notice `GET_AWAITABLE` followed by `SEND` and `YIELD_VALUE`:
`await` translates directly into the same internal `SEND` and `YIELD_VALUE` loop that powered `yield from`, validating that native async is built upon the same fundamental generator frame suspension engine.

---

## 10. Best Practices

1. **Use `async def` for All Modern Asynchronous Code**: Never attempt to construct generator-based coroutines.
2. **Always Run Linter Checks for Unawaited Coroutines**: Configure Ruff (`ASYNC` rules) to detect unawaited coroutines at build time.
3. **Use Async Generators for Streaming I/O**: When streaming asynchronous records, define `async def` with `yield` and consume via `async for`.

---

## 11. Real-World Case Study: Manual Coroutine Event Loop with Socket Polling

```python
import selectors
import socket
from typing import Generator

# Low-level demonstration of how generators laid the groundwork for asyncio
class EventLoopSimulator:
    def __init__(self) -> None:
        self.selector = selectors.DefaultSelector()
        self.tasks: list[Generator] = []

    def add_task(self, coro: Generator) -> None:
        self.tasks.append(coro)

    def run_until_complete(self) -> None:
        while self.tasks:
            current = self.tasks.pop(0)
            try:
                # Step the coroutine forward
                next(current)
                self.tasks.append(current)
            except StopIteration:
                pass

def simulated_network_task(task_id: int):
    print(f"[Task {task_id}] Socket write initiated")
    yield  # Simulate waiting for socket writable
    print(f"[Task {task_id}] Socket read pending")
    yield  # Simulate waiting for socket readable
    print(f"[Task {task_id}] Completed response processing")

loop = EventLoopSimulator()
loop.add_task(simulated_network_task(1))
loop.add_task(simulated_network_task(2))
loop.run_until_complete()
```
**Output**:
```text
[Task 1] Socket write initiated
[Task 2] Socket write initiated
[Task 1] Socket read pending
[Task 2] Socket read pending
[Task 1] Completed response processing
[Task 2] Completed response processing
```

---

## 12. Interview Questions

### Q1: How did generators lead to the development of `async`/`await` in Python?
<details>
<summary><strong>Answer</strong></summary>
Python 2.5 added <code>.send()</code>, allowing values to be injected into suspended generator frames. Developers realized this enabled cooperative multitasking: a generator yields a future/socket, the event loop waits for I/O, and then sends the result back into the generator. PEP 380 (<code>yield from</code>) enabled delegating to sub-generators. Finally, PEP 492 introduced <code>async def</code> and <code>await</code> as first-class syntax to eliminate ambiguity between data-producing generators and asynchronous control-flow coroutines.
</details>

### Q2: What is the purpose of the `__await__()` dunder method?
<details>
<summary><strong>Answer</strong></summary>
Introduced in PEP 492, <code>__await__()</code> makes any custom object "awaitable." It must return an iterator (typically by yielding). When Python executes <code>await obj</code>, it calls <code>obj.__await__()</code> and steps through the returned iterator until completion, capturing the final return value.
</details>

### Q3: Why did Python separate native coroutines (`async def`) from generators?
<details>
<summary><strong>Answer</strong></summary>
Prior to PEP 492, coroutines were regular generators decorated with <code>@asyncio.coroutine</code>. This caused confusing bugs: developers could accidentally iterate over coroutines with <code>for x in coro:</code> or call <code>next(coro)</code>, and refactoring regular generator functions could inadvertently turn them into coroutines. Dedicated syntax created clear boundaries, improved compiler optimizations, and enabled specialized debug warnings.
</details>

### Q4: What triggers the `RuntimeWarning: coroutine was never awaited`?
<details>
<summary><strong>Answer</strong></summary>
When an <code>async def</code> function is called, CPython creates a coroutine object. If that object is garbage collected without its <code>cr_frame</code> ever having been stepped to completion or awaited, CPython's destructor detects that the coroutine was abandoned and emits a <code>RuntimeWarning</code>.
</details>

### Q5: What is the difference between a native coroutine and an asynchronous generator?
<details>
<summary><strong>Answer</strong></summary>
A native coroutine is declared with <code>async def</code> and contains <code>await</code> (or returns a scalar); it can be awaited with <code>await coro</code>. An asynchronous generator is declared with <code>async def</code> and contains <code>yield</code>; it produces a stream of values and must be consumed using <code>async for item in gen:</code>.
</details>

### Q6: What was the significance of PEP 380 (`yield from`) in early asyncio?
<details>
<summary><strong>Answer</strong></summary>
PEP 380 allowed a coroutine to delegate execution to a sub-coroutine (<code>yield from sub_coro()</code>). Crucially, it forwarded <code>send()</code> and <code>throw()</code> transparently and captured the sub-generator's return value from <code>StopIteration.value</code>, providing the exact mechanics that <code>await</code> later formalized.
</details>

### Q7: Can a native coroutine be driven without using the `asyncio` module?
<details>
<summary><strong>Answer</strong></summary>
Yes. At the language level, a coroutine object can be manually driven by invoking <code>coro.send(None)</code> in a loop until it raises <code>StopIteration</code>. <code>asyncio</code> is simply a standard library event loop implementation built on top of this language-level protocol.
</details>

### Q8: When were generator-based coroutines (`@asyncio.coroutine`) permanently removed from Python?
<details>
<summary><strong>Answer</strong></summary>
They were deprecated in Python 3.8 and completely removed in Python 3.11. In Python 3.11+, only native coroutines declared via <code>async def</code> are supported.
</details>

---

## 13. Exercises & Challenges

### Challenge 1 (Easy): Identify Coroutine vs Generator
Write a function `classify_callable(fn)` that inspects a callable and returns `"COROUTINE_FUNCTION"`, `"GENERATOR_FUNCTION"`, or `"REGULAR_FUNCTION"` using the `inspect` module.
<details>
<summary><strong>Solution</strong></summary>

```python
import inspect

def classify_callable(fn) -> str:
    if inspect.iscoroutinefunction(fn):
        return "COROUTINE_FUNCTION"
    elif inspect.isgeneratorfunction(fn):
        return "GENERATOR_FUNCTION"
    else:
        return "REGULAR_FUNCTION"

async def async_fn(): pass
def gen_fn(): yield 1
def regular_fn(): pass

assert classify_callable(async_fn) == "COROUTINE_FUNCTION"
assert classify_callable(gen_fn) == "GENERATOR_FUNCTION"
assert classify_callable(regular_fn) == "REGULAR_FUNCTION"
```
</details>

### Challenge 2 (Easy): Manual Coroutine Driver
Write a helper function `drive_coroutine(coro)` that steps through a native coroutine using `.send(None)` until it finishes, returning the coroutine's final return value without importing `asyncio`.
<details>
<summary><strong>Solution</strong></summary>

```python
def drive_coroutine(coro):
    while True:
        try:
            coro.send(None)
        except StopIteration as e:
            return e.value

async def calculate():
    return 100 + 42

result = drive_coroutine(calculate())
assert result == 142
```
</details>

### Challenge 3 (Medium): Custom Awaitable Sleeping Timer
Implement a custom awaitable class `ManualSleep(ticks: int)` implementing `__await__` that yields `ticks` times before returning `"AWAKE"`. Test it inside a native coroutine driven by a simple loop.
<details>
<summary><strong>Solution</strong></summary>

```python
class ManualSleep:
    def __init__(self, ticks: int) -> None:
        self.ticks = ticks

    def __await__(self):
        for i in range(self.ticks):
            yield f"TICK_{i+1}"
        return "AWAKE"

async def demo():
    status = await ManualSleep(3)
    return status

coro = demo()
yielded = []
while True:
    try:
        val = coro.send(None)
        yielded.append(val)
    except StopIteration as e:
        final_result = e.value
        break

assert yielded == ["TICK_1", "TICK_2", "TICK_3"]
assert final_result == "AWAKE"
```
</details>

### Challenge 4 (Medium): Micro Task Scheduler with Values
Build a generator-based cooperative scheduler `TaskScheduler` that accepts generators yielding `(command, arg)` tuples (e.g. `("SLEEP", 2)` or `("PRINT", "hello")`), executing the commands and scheduling tasks accordingly.
<details>
<summary><strong>Solution</strong></summary>

```python
class TaskScheduler:
    def __init__(self) -> None:
        self.tasks: list[tuple[int, any]] = []

    def spawn(self, gen):
        self.tasks.append((0, gen))

    def run(self):
        logs = []
        clock = 0
        while self.tasks:
            # Sort by wake time
            self.tasks.sort(key=lambda t: t[0])
            wake_time, gen = self.tasks.pop(0)
            clock = max(clock, wake_time)
            try:
                cmd, arg = next(gen)
                if cmd == "LOG":
                    logs.append((clock, arg))
                    self.tasks.append((clock, gen))
                elif cmd == "SLEEP":
                    self.tasks.append((clock + arg, gen))
            except StopIteration:
                pass
        return logs

def task_a():
    yield ("LOG", "A1")
    yield ("SLEEP", 2)
    yield ("LOG", "A2")

def task_b():
    yield ("LOG", "B1")
    yield ("SLEEP", 1)
    yield ("LOG", "B2")

scheduler = TaskScheduler()
scheduler.spawn(task_a())
scheduler.spawn(task_b())
output = scheduler.run()

assert output == [(0, "A1"), (0, "B1"), (1, "B2"), (2, "A2")]
```
</details>

### Challenge 5 (Hard): Async Generator Consumer Pipeline
Using modern Python 3.10+, write an asynchronous generator `async_numbers(n)` that asynchronously yields numbers `1` to `n`. Write a consumer function `async_sum_squares(async_gen)` using `async for` that computes the sum of their squares.
<details>
<summary><strong>Solution</strong></summary>

```python
import asyncio

async def async_numbers(n: int):
    for i in range(1, n + 1):
        await asyncio.sleep(0.001)
        yield i

async def async_sum_squares(async_gen):
    total = 0
    async for num in async_gen:
        total += num * num
    return total

async def test_main():
    res = await async_sum_squares(async_numbers(4))
    # 1^2 + 2^2 + 3^2 + 4^2 = 1 + 4 + 9 + 16 = 30
    assert res == 30

asyncio.run(test_main())
```
</details>

### Challenge 6 (Hard): Coroutine Exception Forwarding Inspector
Implement a custom coroutine runner `RobustRunner` that drives an async function with multiple nested `await` steps. If an external error is injected into the runner via `throw_error(exc)`, ensure the exception is injected cleanly into the currently paused awaitable and either caught or properly bubbled up.
<details>
<summary><strong>Solution</strong></summary>

```python
class StepperAwaitable:
    def __await__(self):
        try:
            yield "PAUSED"
        except ValueError:
            return "RECOVERED_FROM_VALUE_ERROR"
        return "NORMAL_RESUME"

async def nested_task():
    res = await StepperAwaitable()
    return f"Finished with: {res}"

# Step 1: Initialize
coro = nested_task()
assert coro.send(None) == "PAUSED"

# Step 2: Inject error directly into paused coroutine frame
try:
    coro.throw(ValueError("External fault"))
except StopIteration as e:
    assert e.value == "Finished with: RECOVERED_FROM_VALUE_ERROR"
```
</details>
