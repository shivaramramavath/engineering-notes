# Module 08: Iterators, Generators & Functional Programming (05-lazy-evaluation)

> **Topic Overview**: Lazy evaluation defers the computation of an expression until its value is strictly required by an active consumer. In Python, laziness is achieved via generators, iterator protocols, thunks, and lazy descriptors (`@functools.cached_property`), enabling the representation of infinite mathematical structures, instant application startup times, and optimal memory conservation.

---

## Metadata Table

| Field | Value |
|---|---|
| **Category** | Core Language / Evaluation Strategies & Architecture |
| **Target Level** | Advanced |
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

- **Problem Statement**: Eager (strict) evaluation computes values immediately upon declaration. When operating on massive datasets, network APIs, or infinite mathematical series, eager evaluation exhausts memory, wastes CPU cycles on values that are never read, and introduces latency during application startup.
- **Mental Model**:
  - **Strict Evaluation**: "Cook all 100 meals upfront before the first customer arrives."
  - **Lazy Evaluation**: "Wait until an order is placed, cook that single meal, serve it, and clean the pan."
- **When to Use**:
  - Infinite or unbounded sequences (prime numbers, Fibonacci, event tickers).
  - Heavy attributes that may not be needed during every execution path (`cached_property`).
  - Complex multi-stage pipelines where earlier termination (`break`, `islice`) is common.
- **When NOT to Use**: Do not use lazy evaluation when operations require random indexing, multiple independent traversals, or when batch vectorization (e.g. NumPy SIMD operations) is significantly faster than scalar on-demand iteration.

---

## 2. Core Concepts

### Lazy Evaluation Mechanics: Thunks & Iterators
In Python, laziness is implemented through three primary patterns:

```text
+-------------------------------------------------------------------------------+
| Lazy Architectural Patterns in Python                                         |
+-------------------------------------------------------------------------------+
| 1. Generator Expressions / Functions                                          |
|    - Computes next item strictly on next() call                               |
|    - Memory: O(1)                                                             |
|                                                                               |
| 2. Thunks (Nullary Callables)                                                 |
|    - def thunk(): return heavy_computation()                                  |
|    - Evaluated only when invoked: val = thunk()                               |
|                                                                               |
| 3. Lazy Descriptors (cached_property)                                         |
|    - Computed on first attribute access, cached in instance __dict__          |
|    - Fast O(1) attribute access on all subsequent reads                       |
+-------------------------------------------------------------------------------+
```

### Lazy vs Memoization
- **Pure Laziness**: Recomputes values on-demand on every traversal (single-pass or repeated computation, zero cached storage).
- **Memoized Laziness**: Defers computation until first requested, but stores the result in memory permanently so subsequent accesses are instant (trade space for time).

---

## 3. How It Works Internally

CPython's `@functools.cached_property` operates by intercepting the standard descriptor lookup:

```text
+---------------------------------------------------------------+
| First Access: instance.heavy_data                             |
+---------------------------------------------------------------+
| 1. Descriptor __get__() is invoked                            |
| 2. Checks instance.__dict__: 'heavy_data' key NOT found       |
| 3. Runs original method: val = self.compute_heavy_data()      |
| 4. Stores in instance.__dict__['heavy_data'] = val            |
| 5. Returns val                                                |
+---------------------------------------------------------------+

+---------------------------------------------------------------+
| Subsequent Accesses: instance.heavy_data                      |
+---------------------------------------------------------------+
| 1. CPython checks instance.__dict__ FIRST                     |
| 2. Key 'heavy_data' IS found                                  |
| 3. Returns cached value directly from __dict__                |
| 4. Descriptor __get__() is COMPLETELY BYPASSED! (O(1) speed)  |
+---------------------------------------------------------------+
```

---

## 4. Syntax & API Reference

### Implementing a Lazy Thunk
```python
class Thunk[T]:
    """Wraps a computation, evaluating it only once when demanded."""
    def __init__(self, computation: Callable[[], T]) -> None:
        self._func: Callable[[], T] | None = computation
        self._value: T | None = None

    def force(self) -> T:
        if self._func is not None:
            self._value = self._func()
            self._func = None  # Free callable and closed-over variables
        assert self._value is not None
        return self._value
```

### The Standard Library `cached_property`
```python
from functools import cached_property

class DataSet:
    @cached_property
    def summary_statistics(self) -> dict:
        # Runs once on first access
        return compute_expensive_summary()
```

---

## 5. Code Examples

### 1. Minimal / Basic Example: Infinite Sieve of Eratosthenes
```python
from typing import Iterator

def prime_generator() -> Iterator[int]:
    """Lazy infinite prime number generator using a dynamic composites map."""
    composites: dict[int, list[int]] = {}
    candidate = 2
    while True:
        if candidate not in composites:
            # candidate is prime!
            yield candidate
            # Record first multiple
            composites[candidate * candidate] = [candidate]
        else:
            for factor in composites[candidate]:
                composites.setdefault(candidate + factor, []).append(factor)
            del composites[candidate]
        candidate += 1

# Lazily pull only the first 6 primes from an infinite stream
import itertools
first_primes = list(itertools.islice(prime_generator(), 6))
print("First 6 Primes:", first_primes)
```
**Output**:
```text
First 6 Primes: [2, 3, 5, 7, 11, 13]
```

### 2. Real-World Production Example: Lazy Heavy Resource Loader
```python
from functools import cached_property
import time

class MLModelGateway:
    def __init__(self, model_path: str) -> None:
        self.model_path = model_path
        print(f"[INIT] Gateway initialized for {model_path} (Model weights NOT loaded yet)")

    @cached_property
    def model_weights(self) -> dict[str, list[float]]:
        print(f"[LOADING] Simulating 1.5GB tensor weights loading from {self.model_path}...")
        time.sleep(0.05)  # Simulate I/O
        return {"layer_1": [0.12, 0.45], "layer_2": [0.89, 0.33]}

    def predict(self, input_vector: list[float]) -> float:
        # Accesses weights lazily
        weights = self.model_weights["layer_1"]
        return sum(w * x for w, x in zip(weights, input_vector))

gateway = MLModelGateway("/models/bert_v3.bin")
print("Startup complete. Calling predict()...")
res1 = gateway.predict([1.0, 2.0])
print(f"Prediction 1: {res1:.2f}")

print("Calling predict() a second time (should be instant, weights cached)...")
res2 = gateway.predict([2.0, 3.0])
print(f"Prediction 2: {res2:.2f}")
```
**Output**:
```text
[INIT] Gateway initialized for /models/bert_v3.bin (Model weights NOT loaded yet)
Startup complete. Calling predict()...
[LOADING] Simulating 1.5GB tensor weights loading from /models/bert_v3.bin...
Prediction 1: 1.02
Calling predict() a second time (should be instant, weights cached)...
Prediction 2: 1.59
```

### 3. Edge Case: Mutating State in Lazy Streams
```python
def numbers_generator(data_source: list[int]):
    for item in data_source:
        yield item * 2

source = [10, 20, 30]
stream = numbers_generator(source)

# Mutating original list BEFORE consuming stream!
source.append(40)

# The lazy generator reflects the mutation because it was evaluated on-demand!
result = list(stream)
print("Evaluated stream after mutation:", result)
```
**Output**:
```text
Evaluated stream after mutation: [20, 40, 60, 80]
```

### 4. Failure Case & Exception Handling: Late-Binding Exception in Lazy Stream
```python
def faulty_stream():
    yield 100
    yield 200
    # Bug happens at item 3
    raise ZeroDivisionError("Lazy evaluation triggered deferred bug")

# Generator creation succeeds without raising an exception!
stream = faulty_stream()
print("Stream object instantiated cleanly.")

print("First item:", next(stream))
print("Second item:", next(stream))

try:
    print("Third item:", next(stream))
except ZeroDivisionError as err:
    print(f"Deferred Exception caught at point of consumption: {err}")
```
**Output**:
```text
Stream object instantiated cleanly.
First item: 100
Second item: 200
Deferred Exception caught at point of consumption: Lazy evaluation triggered deferred bug
```

### 5. Idiomatic Pythonic Pattern: Lazy Evaluation Pipeline with Sentinel
```python
from typing import Callable, Any

class LazySequence:
    """Wraps an infinite formula, yielding values on demand."""
    def __init__(self, formula: Callable[[int], Any]) -> None:
        self.formula = formula

    def __getitem__(self, index: int) -> Any:
        if index < 0:
            raise IndexError("Negative indexing not supported on lazy sequences")
        return self.formula(index)

# Fibonacci formula defined as a lazy sequence
def fib_formula(n: int) -> int:
    a, b = 0, 1
    for _ in range(n):
        a, b = b, a + b
    return a

lazy_fib = LazySequence(fib_formula)
print("10th Fibonacci number:", lazy_fib[10])
print("20th Fibonacci number:", lazy_fib[20])
```
**Output**:
```text
10th Fibonacci number: 55
20th Fibonacci number: 6765
```

---

## 6. Edge Cases & Pitfalls

1. **Deferred Exceptions (Late Blame)**:
   - When an invalid argument is passed to a generator function, the error is **not** raised when calling the function—it is raised later when the caller first invokes `next()`. To validate eagerly, separate the public function from the private generator.
2. **`cached_property` Thread-Safety**:
   - In standard Python, `cached_property` does not synchronize the initial computation across multiple threads. If two threads access `instance.cached_prop` simultaneously, both threads may execute the expensive computation.
3. **Closing External Connections in Lazy Streams**:
   - If a generator yields items from an open file or database cursor, closing the underlying resource before the generator finishes will cause subsequent `next()` calls to crash with `ValueError: I/O operation on closed file`.

---

## 7. Common Mistakes

| Mistake | Why It Fails | Exception / Diagnostic | Correct Pattern |
|---|---|---|---|
| Eagerly validating arguments inside generator | Validation error only surfaces when first item is read | Late failure far from origin | Validate in outer function; yield in helper |
| Using `cached_property` on class attributes | `cached_property` only works on instances; fails on class access | Returns property descriptor object | Access via instances |
| Sharing a lazy generator between multiple threads | Concurrent calls to `next()` on same generator cause race conditions | `ValueError: generator already executing` | Thread-local iterators or locks |
| Mutating source collections while iterating lazily | Modifying list size during streaming iteration skips/duplicates items | Silent data corruption | Use immutable source or snapshot |
| Forgetting that `cached_property` locks instance `__dict__` | Overwriting or mutating cached value without invalidating cache | Stale data bugs | `del instance.prop` to clear cache |
| Assuming lazy streams can be pickled | Open generator frames with active C pointers cannot be serialized | `TypeError: cannot pickle 'generator' object` | Materialize state or store seeds |

---

## 8. Performance & Complexity

- **Computation Complexity**: Postpones $O(K)$ computation until item $K$ is reached. If only $M < K$ items are consumed, computation cost drops from $O(K)$ to $O(M)$.
- **Memory Complexity**: Pure lazy streams consume $O(1)$ memory; memoized lazy streams (`cached_property`) consume $O(1)$ on access and $O(N)$ for cached storage.
- **Cache Invalidation Overhead**: Clearing a `cached_property` is as simple as `del instance.__dict__["attr"]`, running in $O(1)$.

---

## 9. CPython Internals & Bytecode

Let's inspect how `@cached_property` bypasses descriptor dispatch after the first call:

```python
from functools import cached_property

class Demo:
    @cached_property
    def data(self):
        return [1, 2, 3]

d = Demo()
print("Before access, in __dict__:", "data" in d.__dict__)

# First access triggers descriptor __get__
_ = d.data
print("After access, in __dict__:", "data" in d.__dict__)
print("Value directly from __dict__:", d.__dict__["data"])
```

**Output**:
```text
Before access, in __dict__: False
After access, in __dict__: True
Value directly from __dict__: [1, 2, 3]
```
Because `cached_property` is a **non-data descriptor** (it does not implement `__set__`), CPython's attribute lookup rules dictate that instance `__dict__` takes precedence over non-data descriptors on all subsequent attribute accesses!

---

## 10. Best Practices

1. **Eager Validation, Lazy Execution**: Split generator functions into a public non-generator validator and a private generator implementation.
2. **Use `cached_property` for Heavy Invariants**: Use `cached_property` for expensive attributes that do not change over the instance lifecycle.
3. **Invalidate Cache Cleanly**: To recompute a `cached_property`, use `del instance.property_name`.

---

## 11. Real-World Case Study: Eager Argument Validation with Lazy Streaming

```python
from typing import Iterator

def read_log_records(filepath: str, max_lines: int) -> Iterator[str]:
    """Public API: Validates arguments immediately before returning a lazy stream."""
    if not filepath.endswith(".log"):
        raise ValueError(f"Invalid file extension: '{filepath}' (Expected .log)")
    if max_lines <= 0:
        raise ValueError("max_lines must be positive")

    # Delegate to private generator function
    return _read_log_records_gen(filepath, max_lines)

def _read_log_records_gen(filepath: str, max_lines: int) -> Iterator[str]:
    # Lazy execution begins here
    for i in range(max_lines):
        yield f"[{filepath}] Line {i+1}: Payload OK"

# Case A: Validation fails IMMEDIATELY at call time
try:
    stream = read_log_records("data.txt", 10)
except ValueError as err:
    print(f"Eager error caught at function invocation: {err}")

# Case B: Lazy streaming runs properly
valid_stream = read_log_records("service.log", 2)
print("Stream object created successfully.")
for line in valid_stream:
    print(" ", line)
```
**Output**:
```text
Eager error caught at function invocation: Invalid file extension: 'data.txt' (Expected .log)
Stream object created successfully.
  [service.log] Line 1: Payload OK
  [service.log] Line 2: Payload OK
```

---

## 12. Interview Questions

### Q1: What is the primary difference between a strict evaluation strategy and a lazy evaluation strategy?
<details>
<summary><strong>Answer</strong></summary>
Strict (eager) evaluation computes expressions and allocates result structures immediately when the code is executed. Lazy (on-demand) evaluation defers expression evaluation until the result is explicitly requested by a consumer, avoiding unnecessary computations and memory allocations for unused elements.
</details>

### Q2: Why does Python's `cached_property` operate so quickly on subsequent attribute accesses?
<details>
<summary><strong>Answer</strong></summary>
<code>cached_property</code> is a non-data descriptor (it does not define <code>__set__</code>). On first access, its <code>__get__</code> computes the value and writes it directly into the instance's <code>__dict__</code> under the attribute name. On subsequent accesses, Python's attribute resolution order checks instance <code>__dict__</code> before non-data descriptors, returning the value directly with standard dictionary lookup speed.
</details>

### Q3: How do you reset or invalidate a `cached_property` value on an instance?
<details>
<summary><strong>Answer</strong></summary>
Use the <code>del</code> statement on the attribute: <code>del instance.property_name</code>. This removes the key from the instance's <code>__dict__</code>, forcing the descriptor's <code>__get__</code> method to recompute the value on the next access.
</details>

### Q4: Why is eager argument validation important when authoring generator functions?
<details>
<summary><strong>Answer</strong></summary>
If argument checks are placed inside a generator function containing <code>yield</code>, calling the function does not execute any code—it returns a generator object. The validation check only runs when <code>next()</code> is first called, which may be far away in time and space, confusing callers. Separating into a public validator function that returns a private generator solves this problem.
</details>

### Q5: Can generator objects be serialized with `pickle`?
<details>
<summary><strong>Answer</strong></summary>
No. Standard CPython generator objects contain active execution frames with pointers to C evaluation state and open bytecode offsets, which cannot be serialized by <code>pickle</code>. Attempting to do so raises <code>TypeError: cannot pickle 'generator' object</code>.
</details>

### Q6: What is a "thunk" in computer science and how is it implemented in Python?
<details>
<summary><strong>Answer</strong></summary>
A thunk is a parameterless callable (nullary function or closure) that wraps an expression to delay its evaluation. In Python, it is typically implemented as a zero-argument lambda or function: <code>thunk = lambda: expensive_computation(x, y)</code>, which is evaluated on-demand by calling <code>thunk()</code>.
</details>

### Q7: What is the difference between a generator expression and a list comprehension in terms of evaluation?
<details>
<summary><strong>Answer</strong></summary>
A list comprehension evaluates strictly and eagerly, producing a fully populated <code>list</code> in memory. A generator expression evaluates lazily, producing a <code>generator</code> object that yields one item at a time on demand with $O(1)$ memory.
</details>

### Q8: What concurrency hazard exists with `functools.cached_property`?
<details>
<summary><strong>Answer</strong></summary>
<code>cached_property</code> does not employ a mutex lock during initial calculation. In multi-threaded environments, if multiple threads access the unpopulated property simultaneously, the expensive calculation may be executed multiple times concurrently by different threads.
</details>

---

## 13. Exercises & Challenges

### Challenge 1 (Easy): Infinite Arithmetic Sequence
Implement a lazy generator `arithmetic_progression(start, step)` that yields an infinite sequence of numbers starting at `start` with increment `step`.
<details>
<summary><strong>Solution</strong></summary>

```python
def arithmetic_progression(start: float, step: float):
    curr = start
    while True:
        yield curr
        curr += step

import itertools
seq = list(itertools.islice(arithmetic_progression(10, 2.5), 4))
assert seq == [10.0, 12.5, 15.0, 17.5]
```
</details>

### Challenge 2 (Easy): Invalidate Cached Property
Given a class `Circle` with a `@cached_property` computing `area`, write code that demonstrates accessing the area, verifying it is cached in `__dict__`, invalidating the cache via `del`, and verifying it recomputes properly.
<details>
<summary><strong>Solution</strong></summary>

```python
from functools import cached_property
import math

class Circle:
    def __init__(self, radius: float) -> None:
        self.radius = radius

    @cached_property
    def area(self) -> float:
        return math.pi * (self.radius ** 2)

c = Circle(10)
assert "area" not in c.__dict__
a1 = c.area
assert "area" in c.__dict__

# Invalidate cache
del c.area
assert "area" not in c.__dict__

# Mutate radius and re-read
c.radius = 20
a2 = c.area
assert a2 == math.pi * 400
```
</details>

### Challenge 3 (Medium): Lazy Matrix Row Streamer
Implement a class `LazyMatrix(rows, cols, cell_func)` where cells are computed on-demand only when a specific row is iterated over, consuming memory only for the active row being inspected.
<details>
<summary><strong>Solution</strong></summary>

```python
class LazyMatrix:
    def __init__(self, rows: int, cols: int, cell_func) -> None:
        self.rows = rows
        self.cols = cols
        self.cell_func = cell_func

    def get_row(self, r: int):
        if not (0 <= r < self.rows):
            raise IndexError("Row index out of bounds")
        return (self.cell_func(r, c) for c in range(self.cols))

matrix = LazyMatrix(1000, 1000, lambda r, c: r * 1000 + c)
row_5 = list(matrix.get_row(5))
assert len(row_5) == 1000
assert row_5[0] == 5000
assert row_5[10] == 5010
```
</details>

### Challenge 4 (Medium): Thread-Safe Lazy Value
Implement a class `ThreadSafeLazy[T]` that wraps a callable, evaluates it lazily on first call to `.get()`, and uses a `threading.Lock` to guarantee that the computation function is executed exactly once even when accessed by multiple threads.
<details>
<summary><strong>Solution</strong></summary>

```python
import threading
from typing import Callable, TypeVar

T = TypeVar("T")

class ThreadSafeLazy:
    def __init__(self, factory: Callable[[], T]) -> None:
        self._factory = factory
        self._value: T | None = None
        self._computed = False
        self._lock = threading.Lock()

    def get(self) -> T:
        if not self._computed:
            with self._lock:
                if not self._computed:
                    self._value = self._factory()
                    self._computed = True
        assert self._value is not None
        return self._value

calc_count = 0
def heavy_calc():
    global calc_count
    calc_count += 1
    return 42

lazy_val = ThreadSafeLazy(heavy_calc)
threads = [threading.Thread(target=lambda: lazy_val.get()) for _ in range(10)]
for t in threads: t.start()
for t in threads: t.join()

assert lazy_val.get() == 42
assert calc_count == 1  # Computed exactly once across all 10 threads!
```
</details>

### Challenge 5 (Hard): Lazy Recursive JSON Node
Create a class `LazyJSONNode` that takes a large raw JSON string. When accessing child keys (`node["user"]["profile"]`), do not parse the entire JSON document; instead, parse and deserialize only the requested branch on-demand.
<details>
<summary><strong>Solution</strong></summary>

```python
import json

class LazyJSONNode:
    def __init__(self, raw_or_parsed) -> None:
        if isinstance(raw_or_parsed, str):
            self._raw = raw_or_parsed
            self._parsed = None
        else:
            self._raw = None
            self._parsed = raw_or_parsed

    def _ensure_parsed(self):
        if self._parsed is None:
            assert self._raw is not None
            self._parsed = json.loads(self._raw)

    def __getitem__(self, key):
        self._ensure_parsed()
        val = self._parsed[key]
        if isinstance(val, (dict, list)):
            return LazyJSONNode(val)
        return val

node = LazyJSONNode('{"status": "ok", "data": {"user": {"id": 101, "name": "Alice"}}}')
assert node["status"] == "ok"
assert node["data"]["user"]["name"] == "Alice"
```
</details>

### Challenge 6 (Hard): Lazy Infinite Prime Spiral Coordinates
Using infinite generators, implement a lazy coordinate stream `ulam_spiral_walk()` that yields the 2D Cartesian integer coordinates `(x, y)` of an infinite square spiral moving counterclockwise starting at `(0, 0)`: `(0, 0) -> (1, 0) -> (1, 1) -> (0, 1) -> (-1, 1) -> (-1, 0) ...`.
<details>
<summary><strong>Solution</strong></summary>

```python
def ulam_spiral_walk():
    x, y = 0, 0
    yield (x, y)
    step_size = 1
    while True:
        # Move East
        for _ in range(step_size):
            x += 1
            yield (x, y)
        # Move North
        for _ in range(step_size):
            y += 1
            yield (x, y)
        step_size += 1
        # Move West
        for _ in range(step_size):
            x -= 1
            yield (x, y)
        # Move South
        for _ in range(step_size):
            y -= 1
            yield (x, y)
        step_size += 1

import itertools
first_steps = list(itertools.islice(ulam_spiral_walk(), 7))
assert first_steps == [(0, 0), (1, 0), (1, 1), (0, 1), (-1, 1), (-1, 0), (-1, -1)]
```
</details>
