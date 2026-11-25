# Module 08: Iterators, Generators & Functional Programming (01-generators-and-yield-from)

> **Topic Overview**: Generators provide a lightweight, syntax-level mechanism to implement the iterator protocol using the `yield` statement. With `yield from` (PEP 380) and two-way coroutine methods (`send()`, `throw()`, `close()`), generators transform from simple lazy sequence producers into powerful cooperative multitasking state machines.

---

## Metadata Table

| Field | Value |
|---|---|
| **Category** | Core Language / Generators & Coroutines |
| **Target Level** | Intermediate to Advanced |
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

- **Problem Statement**: Writing custom iterator classes requires managing index state, defining boilerplate `__iter__` and `__next__` methods, and manually handling `StopIteration`. Furthermore, delegating iteration across nested sub-iterators requires verbose loops that fail to forward exceptions or return values properly.
- **Mental Model**: A generator function is a resumable function. When called, it does not execute its body; instead, it returns a `generator` object. When resumed (`next()` or `send()`), it executes until it hits `yield`, pauses its execution frame, and emits a value to the caller.
- **When to Use**:
  - Emitting sequences of values computed on-the-fly with $O(1)$ memory.
  - Composing hierarchical pipelines with `yield from`.
  - Bidirectional communication or actor-like message processing with `send()`.
- **When NOT to Use**: Do not use generators if you need random access by index (`gen[i]`), slice access (`gen[1:5]`), or need to iterate over the dataset multiple times without re-running the generator function.

---

## 2. Core Concepts

### Generator Lifecycle & State Machine
Every generator transitions through four runtime states (introspectable via `inspect.getgeneratorstate`):

```text
[Call generator_func()] ──> GEN_CREATED (Frame allocated, paused before 1st opcode)
                                │
                        next() / send(None)
                                │
                                v
                   ┌──────> GEN_RUNNING (Currently executing in CPython eval loop)
                   │            │
       next() / send()          ├── hits yield ────────> GEN_SUSPENDED (Frame saved on heap)
                   │            │
                   └────────────┤
                                └── returns / uncaught error ──> GEN_CLOSED (Frame destroyed)
```

1. **`GEN_CREATED`**: Waiting for first call to `next()` or `.send(None)`.
2. **`GEN_RUNNING`**: Currently actively running bytecode instructions.
3. **`GEN_SUSPENDED`**: Paused at a `yield` expression with local variables preserved on the heap.
4. **`GEN_CLOSED`**: Execution finished or explicitly terminated via `.close()`.

### The `yield from` Expression (PEP 380)
`yield from iterable` does three distinct things:
1. **Transparent Delegation**: Directly yields all items produced by the sub-generator or iterable to the outer caller.
2. **Bidirectional Channeling**: Any `.send()` or `.throw()` invoked on the delegating generator is passed directly through to the active sub-generator.
3. **Return Value Capture**: When the sub-generator completes with `return value`, `yield from` evaluates directly to that return value (`result = yield from subgen()`).

---

## 3. How It Works Internally

CPython suspends and resumes generators by retaining their `PyFrameObject` on the heap rather than popping it from the C call stack:

```text
+---------------------------------------------------------------+
| PyGenObject Internal Structure                                |
+---------------------------------------------------------------+
| ob_refcnt : 1                                                 |
| ob_type   : &PyGen_Type                                       |
| gi_frame  : &PyFrameObject (Preserves locals: x, y, cursor)   |
| gi_code   : &PyCodeObject (Bytecode instructions)             |
| gi_running: 0 (False when suspended)                          |
+---------------------------------------------------------------+

1. When 'yield item' is reached:
   - Sets gi_frame->f_lasti to current instruction offset.
   - Saves value stack state inside frame.
   - Returns 'item' to caller; frame remains alive on heap.
2. When 'next(gen)' is called:
   - Restores registers and value stack from gi_frame.
   - Resumes execution immediately following the yield instruction.
```

---

## 4. Syntax & API Reference

### Two-Way Generator Methods
- `next(gen)` or `gen.send(None)`: Resumes execution up to the next `yield`.
- `gen.send(value)`: Resumes execution, injecting `value` as the result of the paused `yield` expression.
- `gen.throw(typ[, val[, tb]])`: Raises an exception inside the generator at the paused `yield` statement.
- `gen.close()`: Raises `GeneratorExit` inside the generator to trigger `finally` cleanup.

### Capturing Sub-Generator Return Values
```python
def sub_worker():
    yield 1
    yield 2
    return "RESULT_TOKEN"

def delegator():
    # 'result' receives the string returned by sub_worker's return statement
    result = yield from sub_worker()
    yield f"Completed with: {result}"
```

---

## 5. Code Examples

### 1. Minimal / Basic Example: Bidirectional Communication via `send()`
```python
from typing import Generator

def running_average() -> Generator[float, float, None]:
    total = 0.0
    count = 0
    average = 0.0
    while True:
        # yield 'average' to caller, receive 'val' from gen.send(val)
        val = yield average
        total += val
        count += 1
        average = total / count

avg_gen = running_average()
# Prime the generator (advance to first yield)
print("Initial baseline:", next(avg_gen))

print("After sending 10:", avg_gen.send(10.0))
print("After sending 20:", avg_gen.send(20.0))
print("After sending 30:", avg_gen.send(30.0))
avg_gen.close()
```
**Output**:
```text
Initial baseline: 0.0
After sending 10: 10.0
After sending 20: 15.0
After sending 30: 20.0
```

### 2. Real-World Production Example: Hierarchical Tree Flattening with Return Values
```python
from typing import Any, Generator

def flatten_tree(node: dict | list | Any) -> Generator[Any, None, int]:
    """Recursively yields leaf values and returns the total leaf count."""
    leaf_count = 0
    if isinstance(node, dict):
        for val in node.values():
            count = yield from flatten_tree(val)
            leaf_count += count
    elif isinstance(node, list):
        for item in node:
            count = yield from flatten_tree(item)
            leaf_count += count
    else:
        yield node
        leaf_count = 1

    return leaf_count

def coordinator(data):
    total = yield from flatten_tree(data)
    yield f"SUMMARY: Streamed {total} leaves"

tree_data = {
    "section_a": [10, 20, [30, 40]],
    "section_b": {"sub_1": 50, "sub_2": [60, 70]}
}

for item in coordinator(tree_data):
    print("Streamed:", item)
```
**Output**:
```text
Streamed: 10
Streamed: 20
Streamed: 30
Streamed: 40
Streamed: 50
Streamed: 60
Streamed: 70
Streamed: SUMMARY: Streamed 7 leaves
```

### 3. Edge Case: Handling `GeneratorExit` on `close()`
```python
def managed_generator():
    try:
        print("[SETUP] Allocating resource")
        yield "Step 1"
        yield "Step 2"
    except GeneratorExit:
        print("[CLEANUP] Caught GeneratorExit; releasing resource safely")
        # In GeneratorExit, returning or finishing is required; yielding here raises RuntimeError!
    finally:
        print("[FINALLY] Cleanup guaranteed on termination")

gen = managed_generator()
print("First item:", next(gen))
print("Explicitly closing generator...")
gen.close()
```
**Output**:
```text
[SETUP] Allocating resource
First item: Step 1
Explicitly closing generator...
[CLEANUP] Caught GeneratorExit; releasing resource safely
[FINALLY] Cleanup guaranteed on termination
```

### 4. Failure Case & Exception Handling: Unprimed Generator Send
```python
def accumulator():
    val = yield "READY"
    yield f"Received: {val}"

acc = accumulator()
try:
    # Cannot send non-None value to a newly created generator!
    acc.send(100)  # type: ignore[arg-type]
except TypeError as err:
    print(f"Captured TypeError: {err}")

# Correct priming:
print("Primed correctly:", next(acc))
print("Sent value:", acc.send(100))
```
**Output**:
```text
Captured TypeError: can't send non-None value to a just-started generator
Primed correctly: READY
Sent value: Received: 100
```

### 5. Idiomatic Pythonic Pattern: Generator State Introspection
```python
import inspect

def state_reporter():
    yield 1
    yield 2

g = state_reporter()
print("State before start:", inspect.getgeneratorstate(g))

next(g)
print("State after first next():", inspect.getgeneratorstate(g))

next(g)
print("State after second next():", inspect.getgeneratorstate(g))

try:
    next(g)
except StopIteration:
    pass

print("State after exhaustion:", inspect.getgeneratorstate(g))
```
**Output**:
```text
State before start: GEN_CREATED
State after first next(): GEN_SUSPENDED
State after second next(): GEN_SUSPENDED
State after exhaustion: GEN_CLOSED
```

---

## 6. Edge Cases & Pitfalls

1. **Yielding Inside `GeneratorExit` Handler**:
   - If a generator catches `GeneratorExit` and attempts to execute another `yield`, CPython immediately terminates the process with `RuntimeError: generator ignored GeneratorExit`.
2. **Priming Is Mandatory**:
   - You cannot invoke `.send(value)` on a generator in the `GEN_CREATED` state with any value other than `None`. It must be primed with `next(g)` or `g.send(None)` to advance execution to the first `yield` expression.
3. **Sub-generator Exceptions**:
   - When using `yield from subgen()`, if an unhandled exception occurs inside `subgen()`, it propagates directly out of the `yield from` line into the caller, exactly as if the code had been written inline.

---

## 7. Common Mistakes

| Mistake | Why It Fails | Exception / Diagnostic | Correct Pattern |
|---|---|---|---|
| Sending non-None value to unstarted generator | Generator has not reached a `yield` to receive input | `TypeError: can't send non-None value to a just-started generator` | Call `next(g)` or `g.send(None)` first |
| Attempting to `yield` inside `except GeneratorExit` | Python forbids yielding when a generator is being closed | `RuntimeError: generator ignored GeneratorExit` | Only perform cleanup, do not yield |
| Using `for x in subgen(): yield x` instead of `yield from` | Verbose; fails to propagate `.send()`, `.throw()`, or capture return values | Broken delegation / lost return values | Use `yield from subgen()` |
| Returning value and expecting `next()` to yield it | `return val` raises `StopIteration(val)`; it is not yielded | Value swallowed into StopIteration | `yield val` to emit; `return` for subgen result |
| Expecting generator to reset | Once closed or exhausted, execution cannot be resumed | Raises `StopIteration` immediately | Create a new generator instance |
| Modifying mutable state passed through `send` | Can lead to subtle race conditions or unexpected mutations | Logic bugs | Pass immutable values or clean copies |

---

## 8. Performance & Complexity

- **Yield / Resume Dispatch**: Suspending or resuming a generator costs $\approx 25-45$ ns (far faster than thread context switching).
- **Memory Footprint**: A suspended generator keeps only its single `PyFrameObject` alive on the heap ($\approx 1$ KB), avoiding large sequence allocations.
- **`yield from` Optimization**: Python 3 optimizes `yield from` at the C level with direct pointer chaining (`SEND` bytecode), bypassing nested Python frame evaluation overhead.

---

## 9. CPython Internals & Bytecode

Let's inspect how `yield` and `yield from` compile into bytecode:

```python
import dis

def delegator(sub):
    val = yield from sub
    yield val

dis.dis(delegator)
```

**Bytecode output**:
```text
  1           0 RESUME                   0

  2           2 LOAD_FAST                0 (sub)
              4 GET_YIELD_FROM_ITER
              6 LOAD_CONST               0 (None)
        >>    8 SEND                     3 (to 16)
             10 YIELD_VALUE              1
             12 RESUME                   1
             14 JUMP_BACKWARD            4 (to 8)
        >>   16 STORE_FAST               1 (val)

  3          18 LOAD_FAST                1 (val)
             20 YIELD_VALUE              0
             22 RESUME                   0
             24 POP_TOP
             26 RETURN_CONST             0 (None)
```
Notice the specialized CPython 3.12+ instructions:
- `GET_YIELD_FROM_ITER`: Validates and extracts iterator from `sub`.
- `SEND`: Sends the top-of-stack value into the sub-iterator. If sub-iterator is exhausted with `StopIteration(res)`, it pops the iterator and jumps to offset `16`, storing `res` into `val`.
- `YIELD_VALUE`: Emits the value to the caller and pauses the frame.

---

## 10. Best Practices

1. **Adopt `yield from` Everywhere**: Never manually write `for item in sub: yield item`.
2. **Use Decorators to Auto-Prime Coroutines**: If designing consumer generators with `send()`, use a decorator that automatically invokes `next(gen)`.
3. **Always Clean Up with `try...finally`**: Enclose generator bodies in `try...finally` so that `gen.close()` triggers proper resource reclamation.

---

## 11. Real-World Case Study: Coroutine-Based Event Stream Processor

```python
from typing import Generator

def event_logger(prefix: str) -> Generator[None, dict, None]:
    """Consumer coroutine: logs events with formatting."""
    print(f"[{prefix}] Logger initialized")
    try:
        while True:
            event = yield
            level = event.get("level", "INFO")
            msg = event.get("message", "")
            print(f"[{prefix}][{level}] {msg}")
    except GeneratorExit:
        print(f"[{prefix}] Logger flushed and shut down")

def event_filter(target_level: str, downstream: Generator[None, dict, None]) -> Generator[None, dict, None]:
    """Filter coroutine: forwards matching events downstream."""
    next(downstream)  # Prime consumer
    try:
        while True:
            evt = yield
            if evt.get("level") == target_level:
                downstream.send(evt)
    finally:
        downstream.close()

# Pipeline wiring: Producer -> Filter -> Logger
logger = event_logger("AuditPipeline")
error_filter = event_filter("ERROR", logger)
next(error_filter)  # Prime filter

events = [
    {"level": "INFO", "message": "User logged in"},
    {"level": "ERROR", "message": "Database connection timeout"},
    {"level": "DEBUG", "message": "Cache hit"},
    {"level": "ERROR", "message": "Payment token expired"}
]

for e in events:
    error_filter.send(e)

error_filter.close()
```
**Output**:
```text
[AuditPipeline] Logger initialized
[AuditPipeline][ERROR] Database connection timeout
[AuditPipeline][ERROR] Payment token expired
[AuditPipeline] Logger flushed and shut down
```

---

## 12. Interview Questions

### Q1: What happens under the hood when a function contains the `yield` keyword?
<details>
<summary><strong>Answer</strong></summary>
During compilation, Python flags the code object with <code>CO_GENERATOR</code>. When the function is called, CPython does not execute any bytecode; instead, it immediately instantiates and returns a <code>PyGenObject</code> wrapping a newly allocated execution frame paused at instruction 0.
</details>

### Q2: What three responsibilities does `yield from` fulfill?
<details>
<summary><strong>Answer</strong></summary>
1. Yields all values from the sub-iterable directly to the caller.
2. Forwards <code>send()</code> and <code>throw()</code> calls from the caller directly into the active sub-generator.
3. Captures the return value of the sub-generator (passed via <code>StopIteration.value</code>) as the expression's result.
</details>

### Q3: What is the difference between `return value` in a generator vs a regular function?
<details>
<summary><strong>Answer</strong></summary>
In a regular function, <code>return value</code> returns the object directly to the caller. In a generator, <code>return value</code> raises <code>StopIteration(value)</code>, attaching the value to the exception instance. This value is captured when consumed via <code>result = yield from subgen()</code>.
</details>

### Q4: Why must a generator be primed before sending a non-None value with `.send()`?
<details>
<summary><strong>Answer</strong></summary>
When a generator is first created (<code>GEN_CREATED</code>), it is paused before the first instruction and has not yet executed up to any <code>yield</code> expression. Because there is no active <code>yield</code> waiting to receive a value, sending a non-None value raises <code>TypeError</code>. Calling <code>next()</code> or <code>.send(None)</code> advances it to its first <code>yield</code>.
</details>

### Q5: What happens when `.close()` is called on a generator?
<details>
<summary><strong>Answer</strong></summary>
CPython raises a <code>GeneratorExit</code> exception inside the generator at the current suspended point. If the generator catches <code>GeneratorExit</code>, it must perform cleanup and exit; if it attempts to yield another value, CPython raises <code>RuntimeError</code>.
</details>

### Q6: Can a generator function be called multiple times?
<details>
<summary><strong>Answer</strong></summary>
Yes. Each call to the generator function creates a completely independent <code>generator</code> object with its own separate frame, instruction pointer, and local variables.
</details>

### Q7: What is the return value of `inspect.getgeneratorstate(gen)` after an exception occurs?
<details>
<summary><strong>Answer</strong></summary>
<code>GEN_CLOSED</code>. When an unhandled exception escapes a generator or when it terminates normally, its state transitions to closed, and its frame reference (<code>gi_frame</code>) is cleared.
</details>

### Q8: How does `gen.throw(ExcType, value)` behave if the generator does not catch the exception?
<details>
<summary><strong>Answer</strong></summary>
The exception immediately propagates out of the call to <code>gen.throw()</code> into the caller's scope, the generator's state is set to <code>GEN_CLOSED</code>, and its internal frame is torn down.
</details>

---

## 13. Exercises & Challenges

### Challenge 1 (Easy): Basic Number Emitting Generator
Write a generator `evens_up_to(n: int)` that yields all even numbers from `0` up to `n` inclusive.
<details>
<summary><strong>Solution</strong></summary>

```python
def evens_up_to(n: int):
    for i in range(0, n + 1, 2):
        yield i

assert list(evens_up_to(6)) == [0, 2, 4, 6]
assert list(evens_up_to(1)) == [0]
```
</details>

### Challenge 2 (Easy): Coroutine Auto-Primer Decorator
Write a decorator `@coroutine` that automatically advances any consumer generator to its first `yield` expression upon creation.
<details>
<summary><strong>Solution</strong></summary>

```python
import functools

def coroutine(fn):
    @functools.wraps(fn)
    def primer(*args, **kwargs):
        gen = fn(*args, **kwargs)
        next(gen)
        return gen
    return primer

@coroutine
def sink():
    received = yield
    yield f"Got: {received}"

s = sink()
assert s.send(42) == "Got: 42"
```
</details>

### Challenge 3 (Medium): Yield From Sub-Generator Aggregator
Write two generator functions: `fetch_partition(pid)` which yields 3 integers and returns the string `"PARTITION_DONE"`, and `master_reader(partitions)` which uses `yield from` to stream all items and captures the return values into a list.
<details>
<summary><strong>Solution</strong></summary>

```python
def fetch_partition(pid: int):
    yield pid * 10 + 1
    yield pid * 10 + 2
    return f"P_{pid}_DONE"

def master_reader(partition_ids: list[int]):
    statuses = []
    for pid in partition_ids:
        status = yield from fetch_partition(pid)
        statuses.append(status)
    yield statuses

results = list(master_reader([1, 2]))
assert results == [11, 12, 21, 22, ["P_1_DONE", "P_2_DONE"]]
```
</details>

### Challenge 4 (Medium): Resettable Counter Generator via `send()`
Implement a generator `resettable_counter(start=0)` that yields incremental integers, but if an integer is sent via `.send(new_val)`, resets its counter to `new_val`.
<details>
<summary><strong>Solution</strong></summary>

```python
def resettable_counter(start: int = 0):
    val = start
    while True:
        reset_val = yield val
        if reset_val is not None:
            val = reset_val
        else:
            val += 1

c = resettable_counter(10)
assert next(c) == 10
assert next(c) == 11
assert c.send(50) == 50
assert next(c) == 51
```
</details>

### Challenge 5 (Hard): Dynamic Multi-Generator Demultiplexer
Implement a generator `multiplex(*generators)` that uses `yield from` or round-robin `.send()` / `next()` to yield items from multiple input generators simultaneously, terminating only when all input generators are exhausted.
<details>
<summary><strong>Solution</strong></summary>

```python
def multiplex(*iterables):
    active = [iter(it) for it in iterables]
    while active:
        next_active = []
        for it in active:
            try:
                yield next(it)
                next_active.append(it)
            except StopIteration:
                pass
        active = next_active

gen1 = (x for x in ["A1", "A2", "A3"])
gen2 = (x for x in ["B1", "B2"])
gen3 = (x for x in ["C1"])

result = list(multiplex(gen1, gen2, gen3))
assert result == ["A1", "B1", "C1", "A2", "B2", "A3"]
```
</details>

### Challenge 6 (Hard): Exception Intercepting Pipeline Relay
Create a pipeline of two generators `producer()` and `relay()`. Implement `relay()` using `yield from` such that if the caller calls `relay.throw(CustomError)`, the error is cleanly intercepted, logged, and substituted with a fallback item before terminating.
<details>
<summary><strong>Solution</strong></summary>

```python
class PipelineFault(Exception):
    pass

def producer():
    try:
        yield 100
        yield 200
    except PipelineFault:
        yield "FAULT_RECOVERED"

def relay(prod):
    yield from prod

p = producer()
r = relay(p)

assert next(r) == 100
# Throw fault into relay; should be forwarded down to producer
fallback = r.throw(PipelineFault)
assert fallback == "FAULT_RECOVERED"
```
</details>
