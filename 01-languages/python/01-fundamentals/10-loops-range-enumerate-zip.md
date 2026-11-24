# Module 01: Loops: for, while, range, enumerate & zip (10-loops-range-enumerate-zip)

> **Topic Overview**: Iteration constructs in Python: `for` loops over iterables, `while` condition loops, `break` and `continue`, loop `else` clauses, `range()` arithmetic sequences, `enumerate()` indexed iteration, `zip()` parallel iteration with `strict=True`, and iterator exhaustion mechanics.

---

## Metadata Table

| Field | Value |
|---|---|
| **Category** | Python Fundamentals & Syntax |
| **Target Level** | Beginner to Intermediate |
| **Python Version** | Python 3.10+ (tested on CPython 3.14.0) |
| **Tested With** | CPython 3.14.0, itertools, Ruff 0.17.6 |
| **Free-Threading / Platform** | Platform independent |

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
- **Problem Statement**: Iterating over collections using manual index variables (e.g., `for (int i=0; i < len; i++)`) is error-prone, introduces off-by-one errors, and breaks down when iterating over non-indexable data structures (generators, streams, sets, dictionaries).
- **Mental Model**: In Python, `for` loops are strictly **for-each** loops. A `for` loop does not track integer indices; it requests an **iterator** from an iterable object and advances it item-by-item until the iterator signals completion via `StopIteration`.
- **When to Use**: Iterating over lists, tuples, sets, dictionary keys/values, file streams, database cursor rows, and generator pipelines.
- **When NOT to Use**: When a high-level list comprehension or vectorized NumPy operation expresses the transformation more concisely.

---

## 2. Core Concepts
1. **The `for` Loop & Iterator Protocol**:
   - Calling `for item in collection:` calls `iter(collection)` to obtain an iterator.
   - On each cycle, it calls `next(iterator)`. When `StopIteration` is raised, the loop terminates cleanly.
2. **The `while` Loop**: Repeats execution as long as a boolean condition evaluates to truthy.
3. **The Loop `else` Clause**: Python loops uniquely support an `else` block! The `else` block executes **only if the loop terminates normally** (exhausts its sequence or condition becomes false), and is **skipped if the loop terminates via `break`**.
4. **`range(start, stop[, step])`**: An immutable arithmetic sequence object that calculates integers on-demand in $O(1)$ constant memory, regardless of range size (e.g. `range(1_000_000_000)` takes 48 bytes!).
5. **`enumerate(iterable, start=0)`**: Yields `(index, item)` tuples, eliminating manual counter variables.
6. **`zip(*iterables, strict=False)`**: Combines multiple iterables in parallel, yielding tuples of corresponding elements. In Python 3.10+, setting `strict=True` raises `ValueError` if the iterables are of unequal length, preventing silent data truncation.

---

## 3. How It Works Internally

How CPython's `FOR_ITER` instruction advances loops in the virtual machine:

```text
Evaluating: `for item in data:`
            │
            ▼
[ GET_ITER ] -> Calls iter(data) -> PyObject_GetIter(data)
            │
            ▼ (Top of Loop Offset)
[ FOR_ITER ]
       │
       ├─► Calls type(iter)->tp_iternext(iter)
       │      │
       │      ├─► Next item exists:
       │      │     Pushes item onto stack
       │      │     Executes loop body statements
       │      │     Jumps back to FOR_ITER
       │      │
       │      └─► StopIteration raised!
       │            Clears exception
       │            Jumps to instruction after loop (or loop 'else' block)
       │
       ▼
Execution Continues Below Loop
```

---

## 4. Syntax & API Reference

### Loop Signatures
```python
# Standard for-each
for item in iterable:
    ...
else:
    # Runs ONLY if loop completed without 'break'
    ...

# Indexed loop
for idx, item in enumerate(items, start=0):
    ...

# Parallel zip loop (PEP 618 strict mode)
for a, b in zip(list_a, list_b, strict=True):
    ...

# while condition
while condition:
    ...
```

---

## 5. Code Examples

### 1. Minimal / Basic Example: Combining `enumerate` and `zip`
```python
names = ["Alice", "Bob", "Charlie"]
scores = [95, 88, 92]

pairs = []
for idx, (name, score) in enumerate(zip(names, scores), start=1):
    pairs.append(f"{idx}. {name}: {score}")

print(" | ".join(pairs))
```
**Output**:
```text
1. Alice: 95 | 2. Bob: 88 | 3. Charlie: 92
```

---

### 2. Real-World Production Example: Searching with the Loop `else` Clause
```python
def find_service_endpoint(services: list[dict[str, object]], target_name: str) -> str:
    """Locates an active service endpoint using loop else for fallback."""
    for svc in services:
        if svc.get("name") == target_name and svc.get("healthy"):
            endpoint = str(svc.get("url"))
            print(f"Found active endpoint for {target_name}: {endpoint}")
            break
    else:
        # Executes ONLY if the loop completes without finding a matching service
        raise LookupError(f"No healthy service found matching: '{target_name}'")
        
    return endpoint

catalog = [
    {"name": "auth", "url": "https://auth.internal", "healthy": True},
    {"name": "billing", "url": "https://billing.internal", "healthy": False},
]

print(find_service_endpoint(catalog, "auth"))
```
**Output**:
```text
Found active endpoint for auth: https://auth.internal
https://auth.internal
```

---

### 3. Edge Case: Enforcing Strict Parallel Iteration with `zip(..., strict=True)`
```python
headers = ["id", "username", "role"]
row_short = [101, "alex"]  # Missing 3rd element!

try:
    # strict=True raises ValueError instead of silently truncating headers:
    for h, v in zip(headers, row_short, strict=True):
        print(f"{h}: {v}")
except ValueError as exc:
    print(f"Caught expected error: {type(exc).__name__}: {exc}")
```
**Output**:
```text
Caught expected error: ValueError: zip() argument 2 is shorter than argument 1
```

---

### 4. Failure Case: Modifying a List While Iterating Over It
```python
# The classic bug: removing items while iterating over the list causes skipped elements!
numbers = [1, 2, 3, 4, 5, 6]

# Anti-pattern:
# for num in numbers:
#     if num % 2 == 0:
#         numbers.remove(num)

# Correct Pattern: Iterate over a copy or use a list comprehension
clean_numbers = [num for num in numbers if num % 2 != 0]
print("Safely filtered numbers:", clean_numbers)
```
**Output**:
```text
Safely filtered numbers: [1, 3, 5]
```

---

### 5. Idiomatic Pythonic Pattern: Chunking Iterables via `itertools.batched`
```python
import itertools

incoming_event_ids = list(range(1, 11))

# In Python 3.12+, itertools.batched groups iterables into fixed-size batches:
batch_log = []
for batch in itertools.batched(incoming_event_ids, 3):
    batch_log.append(list(batch))

print("Batched chunks:", batch_log)
```
**Output**:
```text
Batched chunks: [[1, 2, 3], [4, 5, 6], [7, 8, 9], [10]]
```

---

## 6. Edge Cases & Pitfalls
1. **Loop Variable Scope Leaks**: In Python, loop index variables (`for x in ...`) leak into the enclosing function or module scope and retain their final value after the loop terminates!
2. **`range()` Object Reusability**: Unlike generators, `range()` objects can be iterated over multiple times, support indexing (`range(10)[3] == 3`), slicing, and $O(1)$ membership testing (`5 in range(10)`).
3. **Modifying Containers During Iteration**: Modifying a dictionary or set while iterating over it triggers `RuntimeError: dictionary changed size during iteration`. Mutating a list in-place does not raise an exception, but silently skips elements due to internal pointer shifting.

---

## 7. Common Mistakes

| Mistake | Why It Fails | Exception / Diagnostic | Correct Pattern |
|---|---|---|---|
| Using `for i in range(len(items)):` | Unpythonic; requires manual indexing lookups | Slower code and index bugs | Use `for item in items:` or `enumerate()` |
| Mutating a list while iterating over it | Elements are skipped as indices shift leftward | Silent data corruption | Iterate over `items.copy()` or use comprehension |
| Omitting `strict=True` in `zip()` | Silently truncates to the shortest iterable | Mismatched data rows | Use `zip(a, b, strict=True)` |
| Confusing loop `else` with `if/else` | Expecting `else` to run when loop condition is false AND after break | Logic errors | Remember: `else` is skipped if loop breaks |
| Creating huge lists with `list(range(10_000_000))` | Allocates gigabytes of RAM in heap memory | `MemoryError` | Iterate directly over `range()` |
| Re-iterating over exhausted generators | Generators yield items once and cannot be reset | Empty second loop | Convert to list if multiple passes are needed |

---

## 8. Performance & Complexity
- **`range()` Memory Overhead**: $O(1)$ constant 48 bytes regardless of start or stop limits.
- **`FOR_ITER` Opcode Dispatch**: CPython optimizes `FOR_ITER` to execute in native C loops with minimal stack manipulation ($~10$ ns per iteration).

---

## 9. CPython Internals & Bytecode: The `FOR_ITER` Opcode
```python
import dis

def loop_example(data: list[int]) -> int:
    total = 0
    for x in data:
        total += x
    return total

dis.dis(loop_example)
```
Disassembly:
```text
  1           0 RESUME                   0
              2 LOAD_CONST               1 (0)
              4 STORE_FAST               1 (total)

  2           6 LOAD_FAST                0 (data)
              8 GET_ITER                 # Creates PySeqIterObject
        >>   10 FOR_ITER                 6 (to 24)
             12 STORE_FAST               2 (x)
             14 LOAD_FAST                1 (total)
             16 LOAD_FAST                2 (x)
             18 BINARY_OP                0 (+)
             22 STORE_FAST               1 (total)
             24 JUMP_BACKWARD            8 (to 10)

  3     >>   26 LOAD_FAST                1 (total)
             28 RETURN_VALUE
```

---

## 10. Best Practices
1. Never write `for i in range(len(items)):`; iterate directly over items or use `enumerate()`.
2. Always pass `strict=True` to `zip()` when input iterables are expected to be of identical length.
3. Use the loop `else` clause when implementing search loops with fallback handling.
4. Use `itertools.batched` for batch processing operations.

---

## 11. Real-World Case Study: Eliminating Silent Ingestion Truncation with `strict=True`
A data migration pipeline processed thousands of CSV files containing customer records. The code paired CSV header names with row values:
```python
record = dict(zip(headers, row))
```
When a malformed CSV export omitted the final column in 5,000 rows, `zip()` silently discarded the last header (`account_status`), assigning defaults that marked paid accounts as delinquent!
1. The engineering team upgraded the pipeline to `zip(headers, row, strict=True)`.
2. When malformed rows were encountered, `ValueError` was raised immediately.
3. The pipeline isolated the corrupt rows to a dead-letter queue, preventing customer data corruption.

---

## 12. Interview Questions

### Q1: How does a Python `for` loop actually work under the hood?
<details>
<summary><strong>Answer</strong></summary>
A Python `for` loop operates on the Iterator Protocol:
1. It passes the target object to `iter(obj)` to obtain an iterator (invoking `obj.__iter__()`).
2. Inside the C evaluation loop, the `FOR_ITER` opcode repeatedly calls `next(iterator)` (invoking `iterator.__next__()`).
3. Each returned value is bound to the target variable and the loop body executes.
4. When the iterator raises `StopIteration`, CPython catches the exception internally and cleanly exits the loop.
</details>

### Q2: When does the `else` block of a Python loop execute?
<details>
<summary><strong>Answer</strong></summary>
The `else` block of a `for` or `while` loop executes when the loop completes its natural lifecycle:
- In a `for` loop: When the iterator is completely exhausted.
- In a `while` loop: When the condition becomes falsy.
If the loop is exited prematurely via `break` (or `return` / an unhandled exception), the `else` block is completely skipped.
</details>

### Q3: Why is `zip(a, b, strict=True)` preferred in modern Python?
<details>
<summary><strong>Answer</strong></summary>
By default, `zip()` stops as soon as the shortest iterable is exhausted, silently discarding any remaining elements from longer iterables. Introduced in Python 3.10 (PEP 618), `strict=True` asserts that all input iterables have the exact same length, raising a `ValueError` if any length mismatch occurs, preventing silent data truncation bugs.
</details>

### Q4: Why does `range(1_000_000_000)` consume almost zero memory in Python 3?
<details>
<summary><strong>Answer</strong></summary>
In Python 3, `range()` does not allocate an in-memory list of numbers. It returns an immutable `range` object that stores only three integers: `start`, `stop`, and `step`. Elements are computed lazily on-the-fly when requested by an iterator, consuming a fixed $O(1)$ memory footprint (~48 bytes) regardless of range span.
</details>

### Q5: What happens to the loop variable after a `for` loop finishes?
<details>
<summary><strong>Answer</strong></summary>
Unlike languages like C++ or Java where loop variables are scoped strictly to the loop body, Python does not have block-level scoping. The variable used in `for x in ...` exists in the enclosing function or module namespace and retains its final bound value even after the loop terminates.
</details>

### Q6: Why should you avoid modifying a list while iterating over it?
<details>
<summary><strong>Answer</strong></summary>
A list iterator tracks an internal integer index offset into the list's memory array. If you remove an element during iteration, all subsequent elements shift left by one index. On the next iteration, the iterator increments its index offset, effectively skipping the element immediately following the deleted item.
</details>

### Q7: How does `itertools.zip_longest` differ from built-in `zip()`?
<details>
<summary><strong>Answer</strong></summary>
Standard `zip()` truncates to the shortest iterable (unless `strict=True`). `itertools.zip_longest` continues iterating until the *longest* iterable is exhausted, filling in missing values for shorter iterables with a configurable `fillvalue` (defaulting to `None`).
</details>

### Q8: Can a `range()` object be sliced or indexed directly?
<details>
<summary><strong>Answer</strong></summary>
Yes. `range` is a full sequence type that implements `__getitem__` and `__len__`. You can index it directly (`range(0, 100, 2)[5] == 10`), slice it (`range(100)[10:20]`), and test for membership in $O(1)$ time (`50 in range(100)`).
</details>

---

## 13. Exercises & Challenges

### Challenge 1 (Easy): Indexed Enumeration with 1-Based Indexing
Write a function `format_ranked_list(items)` that returns a list of strings formatted as `"1. item"`, `"2. item"` using `enumerate(..., start=1)`.
<details>
<summary><strong>Solution</strong></summary>

```python
def format_ranked_list(items: list[str]) -> list[str]:
    return [f"{idx}. {item}" for idx, item in enumerate(items, start=1)]

print(format_ranked_list(["Python", "Rust", "Go"]))
```
</details>

### Challenge 2 (Easy): Multi-List Parallel Pairing
Write a function `merge_attributes(keys, values)` that takes two equal-length lists and returns a dictionary, raising `ValueError` if their lengths differ using `zip(..., strict=True)`.
<details>
<summary><strong>Solution</strong></summary>

```python
from typing import Any

def merge_attributes(keys: list[str], values: list[Any]) -> dict[str, Any]:
    return dict(zip(keys, values, strict=True))

print("Merged:", merge_attributes(["name", "age"], ["Ada", 36]))
```
</details>

### Challenge 3 (Medium): Chunking Stream Without Third-Party Libraries
Implement a generator function `chunk_list(data, chunk_size)` that yields sublists of length `chunk_size` using a `for` loop and `range()`.
<details>
<summary><strong>Solution</strong></summary>

```python
from collections.abc import Generator
from typing import TypeVar

T = TypeVar("T")

def chunk_list(data: list[T], chunk_size: int) -> Generator[list[T], None, None]:
    for i in range(0, len(data), chunk_size):
        yield data[i : i + chunk_size]

sample = list(range(1, 10))
for chunk in chunk_list(sample, 4):
    print("Chunk:", chunk)
```
</details>

### Challenge 4 (Medium): Linear Search with Loop Else Pattern
Write a function `find_first_prime(numbers)` that finds and returns the first prime number in a list, using loop `else` to return `None` if no prime exists.
<details>
<summary><strong>Solution</strong></summary>

```python
def is_prime(n: int) -> bool:
    if n < 2:
        return False
    for i in range(2, int(n ** 0.5) + 1):
        if n % i == 0:
            return False
    return True

def find_first_prime(numbers: list[int]) -> int | None:
    for num in numbers:
        if is_prime(num):
            return num
    else:
        return None

print("Prime in [4, 6, 8, 9, 11, 14]:", find_first_prime([4, 6, 8, 9, 11, 14]))
print("Prime in [4, 6, 8]:            ", find_first_prime([4, 6, 8]))
```
</details>

### Challenge 5 (Hard): Flattening Nested Jagged Sequences
Write a generator function `flatten_nested(nested_iterable)` that recursively flattens deeply nested sequences of arbitrary depth using `for` loops.
<details>
<summary><strong>Solution</strong></summary>

```python
from collections.abc import Generator, Iterable
from typing import Any

def flatten_nested(nested: Iterable[Any]) -> Generator[Any, None, None]:
    for item in nested:
        if isinstance(item, Iterable) and not isinstance(item, (str, bytes)):
            yield from flatten_nested(item)
        else:
            yield item

jagged = [1, [2, [3, 4], 5], [[6]], 7]
print("Flattened:", list(flatten_nested(jagged)))
```
</details>

### Challenge 6 (Hard): Custom Re-Settable Range Sequence
Implement a lightweight `CustomRange` class that implements `__iter__`, `__getitem__`, and `__len__`, supporting $O(1)$ length lookups and indexing.
<details>
<summary><strong>Solution</strong></summary>

```python
class CustomRange:
    def __init__(self, start: int, stop: int, step: int = 1) -> None:
        if step == 0:
            raise ValueError("Step cannot be zero")
        self.start = start
        self.stop = stop
        self.step = step

    def __len__(self) -> int:
        if (self.step > 0 and self.start >= self.stop) or (self.step < 0 and self.start <= self.stop):
            return 0
        return (self.stop - self.start - (1 if self.step > 0 else -1)) // self.step + 1

    def __getitem__(self, idx: int) -> int:
        length = len(self)
        if idx < 0:
            idx += length
        if not (0 <= idx < length):
            raise IndexError("CustomRange index out of range")
        return self.start + (idx * self.step)

cr = CustomRange(0, 10, 2)
print("Length of CustomRange(0, 10, 2):", len(cr))
print("Item at index 3:                ", cr[3])
```
</details>
