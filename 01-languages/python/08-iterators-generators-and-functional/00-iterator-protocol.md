# Module 08: Iterators, Generators & Functional Programming (00-iterator-protocol)

> **Topic Overview**: The iterator protocol is the foundational mechanism powering Python's iteration model (`for` loops, comprehensions, unpacking, and generator pipelines). By decoupling data storage from sequential traversal via the `__iter__` and `__next__` dunder methods, Python enables efficient, lazy consumption of unbounded data streams with constant $O(1)$ memory overhead.

---

## Metadata Table

| Field | Value |
|---|---|
| **Category** | Core Language / Iteration & Protocols |
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

- **Problem Statement**: Loading an entire dataset into memory to iterate over its elements causes out-of-memory crashes when dealing with large files, database result sets, or continuous network streams. Furthermore, forcing developers to access collections solely by numerical index (`collection[i]`) breaks down for sets, trees, graphs, and hardware sensors.
- **Mental Model**:
  - An **Iterable** is a container that promises: "I can produce an iterator when asked" (`__iter__()`).
  - An **Iterator** is a stateful cursor that promises: "I yield the next item or raise `StopIteration` when exhausted" (`__next__()`).
- **When to Use**:
  - Whenever designing custom collection types or stream readers.
  - Streaming file, database, or network records item by item.
  - Implementing infinite or algorithmic mathematical series.
- **When NOT to Use**: Do not implement custom iterators when a standard generator function (`yield`) or generator expression provides the same behavior with far less boilerplate.

---

## 2. Core Concepts

### Iterable vs Iterator Formal Taxonomy
In Python's data model:

```text
+-------------------------------------------------------+
| Iterable Protocol                                     |
| Implements: __iter__() -> Iterator                    |
| (Or legacy fallback: __getitem__(index))              |
+-------------------------------------------------------+
                           │
                           │ iter(obj)
                           v
+-------------------------------------------------------+
| Iterator Protocol                                     |
| Implements:                                           |
|   1. __next__() -> next value OR raises StopIteration |
|   2. __iter__() -> returns self (self-iterability)    |
+-------------------------------------------------------+
```

1. **Self-Iterability Rule**: Every iterator **must** implement `__iter__()` and return `self`. This allows iterators themselves to be passed directly to `for` loops, `zip()`, or `enumerate()`.
2. **Single-Pass Exhaustion**: Unlike collections (lists, tuples, sets) which can be iterated multiple times independently, an iterator is typically a consumable cursor. Once it raises `StopIteration`, subsequent calls to `next()` must continue raising `StopIteration`.
3. **The Two-Argument `iter()` Form**: `iter(callable, sentinel)` repeatedly calls `callable()` without arguments until the return value equals `sentinel`, automatically raising `StopIteration`.

---

## 3. How It Works Internally

When CPython executes a `for item in sequence:` loop, it invokes the `GET_ITER` and `FOR_ITER` bytecode instructions:

```text
+---------------------------------------------------------------+
| CPython Execution of 'for item in sequence:'                 |
+---------------------------------------------------------------+
| 1. GET_ITER instruction:                                      |
|    - Calls PyObject_GetIter(sequence)                         |
|    - Checks sequence->ob_type->tp_iter                        |
|    - Returns an iterator instance                             |
| 2. FOR_ITER instruction (start of loop iteration):            |
|    - Calls iterator->ob_type->tp_iternext(iterator)           |
|    - If item returned: pushes item to stack, enters loop body |
|    - If StopIteration raised: clears exception, jumps past    |
|      the loop body (or into 'else' clause if finished)        |
+---------------------------------------------------------------+
```

---

## 4. Syntax & API Reference

### Implementing Custom Iterators
```python
class CountDown:
    def __init__(self, start: int) -> None:
        self.current = start

    def __iter__(self) -> "CountDown":
        # Iterators return self in __iter__
        return self

    def __next__(self) -> int:
        if self.current <= 0:
            raise StopIteration
        val = self.current
        self.current -= 1
        return val
```

### Built-In Iterator Helpers
- `iter(iterable)`: Retrieves an iterator from an iterable object.
- `iter(callable, sentinel)`: Creates a sentinel-terminated iterator.
- `next(iterator[, default])`: Retrieves the next item; returns `default` if iterator is exhausted (instead of raising `StopIteration`).

---

## 5. Code Examples

### 1. Minimal / Basic Example: Custom Range Iterator
```python
class StepRange:
    def __init__(self, start: int, stop: int, step: int = 1) -> None:
        self.current = start
        self.stop = stop
        self.step = step

    def __iter__(self) -> "StepRange":
        return self

    def __next__(self) -> int:
        if (self.step > 0 and self.current >= self.stop) or (self.step < 0 and self.current <= self.stop):
            raise StopIteration
        result = self.current
        self.current += self.step
        return result

# Iterate via for loop
values = [x for x in StepRange(0, 10, 3)]
print("StepRange values:", values)

# Direct consumption with next()
it = StepRange(10, 12, 1)
print("Item 1:", next(it))
print("Item 2:", next(it))
print("Item 3 (with default):", next(it, "EXHAUSTED"))
```
**Output**:
```text
StepRange values: [0, 3, 6, 9]
Item 1: 10
Item 2: 11
Item 3 (with default): EXHAUSTED
```

### 2. Real-World Production Example: Chunked Fixed-Size Binary Reader
```python
import io

def chunked_stream_reader(stream: io.BytesIO, chunk_size: int = 4):
    """Sentinel-based iterator reading binary streams in fixed byte chunks."""
    # iter(callable, sentinel) reads until empty bytes b"" is returned
    reader = iter(lambda: stream.read(chunk_size), b"")
    return reader

binary_payload = io.BytesIO(b"ABCDEFGHIJKLMNO")
chunks = []
for chunk in chunked_stream_reader(binary_payload, chunk_size=4):
    chunks.append(chunk)

print("Extracted Chunks:", chunks)
```
**Output**:
```text
Extracted Chunks: [b'ABCD', b'EFGH', b'IJKL', b'MNO']
```

### 3. Edge Case: Separation of Iterable Collection and Iterator
```python
class FibonacciSequence:
    """Reusable Iterable Collection: can be iterated multiple times!"""
    def __init__(self, limit: int) -> None:
        self.limit = limit

    def __iter__(self) -> "_FibonacciIterator":
        # Returns a fresh, independent iterator each time __iter__ is called
        return _FibonacciIterator(self.limit)

class _FibonacciIterator:
    def __init__(self, limit: int) -> None:
        self.limit = limit
        self.count = 0
        self.a, self.b = 0, 1

    def __iter__(self) -> "_FibonacciIterator":
        return self

    def __next__(self) -> int:
        if self.count >= self.limit:
            raise StopIteration
        val = self.a
        self.a, self.b = self.b, self.a + self.b
        self.count += 1
        return val

fib = FibonacciSequence(5)
print("Run 1:", list(fib))
print("Run 2:", list(fib))  # Reusable because __iter__ returns fresh cursor!
```
**Output**:
```text
Run 1: [0, 1, 1, 2, 3]
Run 2: [0, 1, 1, 2, 3]
```

### 4. Failure Case & Exception Handling: Exhausted Iterator Reuse
```python
numbers = iter([1, 2, 3])

# First consumption
first_pass = list(numbers)
print("First pass:", first_pass)

# Second consumption on exhausted iterator
second_pass = list(numbers)
print("Second pass:", second_pass)

# Direct next() call on exhausted iterator raises StopIteration
try:
    next(numbers)
except StopIteration:
    print("Caught expected StopIteration on consumed iterator.")
```
**Output**:
```text
First pass: [1, 2, 3]
Second pass: []
Caught expected StopIteration on consumed iterator.
```

### 5. Idiomatic Pythonic Pattern: Infinite Circular Iterator
```python
class CircularBuffer:
    def __init__(self, items: list[str]) -> None:
        if not items:
            raise ValueError("Buffer cannot be empty")
        self.items = items
        self.index = 0

    def __iter__(self) -> "CircularBuffer":
        return self

    def __next__(self) -> str:
        val = self.items[self.index]
        self.index = (self.index + 1) % len(self.items)
        return val

rotator = CircularBuffer(["red", "green", "blue"])
pattern = [next(rotator) for _ in range(7)]
print("Circular sequence:", pattern)
```
**Output**:
```text
Circular sequence: ['red', 'green', 'blue', 'red', 'green', 'blue', 'red']
```

---

## 6. Edge Cases & Pitfalls

1. **Combining Container and Iterator in One Class**:
   - If an iterable class implements `__next__` on itself without separate state, it becomes a single-use container. The first `for` loop consumes it, leaving subsequent loops empty. Separate the container (`Iterable`) from the cursor (`Iterator`).
2. **Legacy `__getitem__` Fallback**:
   - If an object lacks `__iter__`, Python checks for `__getitem__`. It creates an iterator that calls `obj[0]`, `obj[1]`, `obj[2]` until an `IndexError` is raised. Relying on this is discouraged; always implement `__iter__`.
3. **`StopIteration` Inside Generators (PEP 479)**:
   - In modern Python, raising `StopIteration` inside a generator does **not** cleanly terminate the generator; it is transformed into a `RuntimeError: generator raised StopIteration`. In generators, use a simple `return` to terminate.

---

## 7. Common Mistakes

| Mistake | Why It Fails | Exception / Diagnostic | Correct Pattern |
|---|---|---|---|
| Forgetting `return self` in iterator's `__iter__` | Passing the iterator to `for` fails; violates self-iterability rule | `TypeError: iter() returned non-iterator` | Always `return self` in iterator's `__iter__` |
| Calling `len()` on custom iterator | Iterators are dynamic and have unknown future lengths | `TypeError: object of type '...' has no len()` | Keep iterators length-agnostic or implement collections.abc |
| Expecting exhausted iterators to reset | Consumed iterators remain exhausted indefinitely | Empty collections on second iteration | Instantiate a new iterator |
| Infinite loop in `iter(callable, sentinel)` | If `callable()` never produces `sentinel`, it loops indefinitely | Process hangs / 100% CPU | Ensure callable reaches sentinel condition |
| Raising exceptions other than `StopIteration` for loop end | Loops only terminate on `StopIteration`; other errors crash the loop | Unhandled exception crash | Raise `StopIteration` to signal clean completion |
| Returning `None` to signal iteration end | `None` is treated as a valid value; loop continues indefinitely | Infinite loop yielding `None` | Raise `StopIteration` |

---

## 8. Performance & Complexity

- **Time Complexity**: Invoking `__next__()` is $O(1)$ amortized for algorithmic and sequence iterators.
- **Space Complexity**: Pure iterators consume $O(1)$ memory regardless of dataset size (only tracking internal state pointers).
- **CPython Specialization**: The `FOR_ITER` instruction uses adaptive bytecode caches to call the underlying C-level `tp_iternext` slot directly, avoiding Python method resolution overhead.

---

## 9. CPython Internals & Bytecode

Let's examine how Python executes a `for` loop over an iterator:

```python
import dis

def loop_demo(items):
    for x in items:
        pass

dis.dis(loop_demo)
```

**Bytecode output**:
```text
  1           0 RESUME                   0

  2           2 LOAD_FAST                0 (items)
              4 GET_ITER
        >>    6 FOR_ITER                 4 (to 16)
              8 STORE_FAST               1 (x)

  3          10 JUMP_BACKWARD            3 (to 6)

  2     >>   14 END_FOR
        >>   16 RETURN_CONST             0 (None)
```
Notice the tight instruction loop:
1. `GET_ITER`: Extracts iterator from `items`.
2. `FOR_ITER`: Calls `tp_iternext`. If an item is available, pushes it to stack. When exhausted, jumps directly forward to offset `14` (`END_FOR`), clearing the internal `StopIteration` exception cleanly.

---

## 10. Best Practices

1. **Separate Containers from Iterators**: Containers implement `__iter__() -> NewIterator`; iterators implement `__iter__() -> self` and `__next__()`.
2. **Leverage `iter(callable, sentinel)`**: Use the two-argument `iter()` form when polling sockets, reading files, or consuming queue items until a termination sentinel.
3. **Use Default with `next()`**: Write `next(it, default)` instead of wrapping `next(it)` in `try...except StopIteration` for concise fallback logic.

---

## 11. Real-World Case Study: Paginated REST API Result Streamer

```python
from typing import Iterator

class PaginatedAPIIterator:
    """Streams individual records across multiple paginated HTTP requests."""
    def __init__(self, endpoint: str, total_pages: int = 3) -> None:
        self.endpoint = endpoint
        self.total_pages = total_pages
        self.current_page = 1
        self.buffer: list[dict] = []

    def __iter__(self) -> Iterator[dict]:
        return self

    def _fetch_page(self, page: int) -> list[dict]:
        # Simulated paginated API response
        print(f"[HTTP GET] Fetching {self.endpoint}?page={page}")
        return [{"id": page * 10 + i, "title": f"Item_{page}_{i}"} for i in range(2)]

    def __next__(self) -> dict:
        while not self.buffer:
            if self.current_page > self.total_pages:
                raise StopIteration
            self.buffer = self._fetch_page(self.current_page)
            self.current_page += 1

        return self.buffer.pop(0)

# Client code iterates seamlessly across pages without knowing batch boundaries
api_stream = PaginatedAPIIterator("/api/v1/records", total_pages=3)
all_items = []
for item in api_stream:
    all_items.append(item["title"])

print(f"Total items streamed: {len(all_items)}")
print(f"Items: {all_items}")
```
**Output**:
```text
[HTTP GET] Fetching /api/v1/records?page=1
[HTTP GET] Fetching /api/v1/records?page=2
[HTTP GET] Fetching /api/v1/records?page=3
Total items streamed: 6
Items: ['Item_1_0', 'Item_1_1', 'Item_2_0', 'Item_2_1', 'Item_3_0', 'Item_3_1']
```

---

## 12. Interview Questions

### Q1: What is the formal difference between an Iterable and an Iterator in Python?
<details>
<summary><strong>Answer</strong></summary>
An <strong>Iterable</strong> is an object that has an <code>__iter__()</code> method returning an iterator (or a <code>__getitem__()</code> method for sequential indexing). An <strong>Iterator</strong> is an object that implements the iterator protocol: it has a <code>__next__()</code> method that returns the next item or raises <code>StopIteration</code>, and an <code>__iter__()</code> method that returns <code>self</code>.
</details>

### Q2: Why must an iterator's `__iter__()` method return `self`?
<details>
<summary><strong>Answer</strong></summary>
This ensures "self-iterability." When an iterator is passed to functions or statements that expect an iterable (such as <code>for item in iterator:</code> or <code>zip(it1, it2)</code>), Python calls <code>iter()</code> on it. Returning <code>self</code> allows the iterator to be consumed directly in any iteration context.
</details>

### Q3: How does the two-argument form of `iter()` work?
<details>
<summary><strong>Answer</strong></summary>
<code>iter(callable, sentinel)</code> creates a special callable-iterator. Each time <code>__next__()</code> is called, it invokes <code>callable()</code> with no arguments. If the return value equals <code>sentinel</code>, it raises <code>StopIteration</code>; otherwise, it returns the value.
</details>

### Q4: What bytecode instruction executes the loop test and advancement?
<details>
<summary><strong>Answer</strong></summary>
<code>FOR_ITER</code>. It calls the iterator's C-level <code>tp_iternext</code> slot. If an item is returned, it pushes the item to the evaluation stack and continues into the loop body. If <code>StopIteration</code> is detected, it pops the iterator, clears the exception, and jumps forward past the loop body.
</details>

### Q5: Can an iterator be reset to the beginning once exhausted?
<details>
<summary><strong>Answer</strong></summary>
Standard Python iterators cannot be reset. Once an iterator raises <code>StopIteration</code>, it is permanently consumed. To iterate over the underlying data again, a new iterator must be created from the original iterable collection.
</details>

### Q6: What happens if an object implements `__getitem__` but not `__iter__`?
<details>
<summary><strong>Answer</strong></summary>
When <code>iter(obj)</code> is called, Python falls back to creating an internal <code>seqiter</code> object. This iterator attempts to look up integer indices starting at <code>0</code> (<code>obj[0]</code>, <code>obj[1]</code>, etc.) until an <code>IndexError</code> is raised, at which point it terminates with <code>StopIteration</code>.
</details>

### Q7: Why is raising `StopIteration` inside a generator considered an error in modern Python?
<details>
<summary><strong>Answer</strong></summary>
Under PEP 479 (introduced in Python 3.5 and default in 3.7+), raising <code>StopIteration</code> inside a generator is transformed into a <code>RuntimeError</code>. This prevents a secondary exception inside a generator from accidentally masking or terminating an outer loop. Generators should use <code>return</code> to terminate.
</details>

### Q8: How can you retrieve the next item from an iterator without risking an unhandled `StopIteration`?
<details>
<summary><strong>Answer</strong></summary>
Use the built-in <code>next(iterator, default)</code> function with a default value: e.g., <code>val = next(it, None)</code>. If the iterator is exhausted, it returns <code>None</code> instead of raising <code>StopIteration</code>.
</details>

---

## 13. Exercises & Challenges

### Challenge 1 (Easy): Countdown Iterator
Implement an iterator `CountDown(n)` that counts down from `n` to `1` inclusive, raising `StopIteration` when `0` is reached.
<details>
<summary><strong>Solution</strong></summary>

```python
class CountDown:
    def __init__(self, n: int) -> None:
        self.n = n

    def __iter__(self) -> "CountDown":
        return self

    def __next__(self) -> int:
        if self.n <= 0:
            raise StopIteration
        val = self.n
        self.n -= 1
        return val

assert list(CountDown(3)) == [3, 2, 1]
assert list(CountDown(0)) == []
```
</details>

### Challenge 2 (Easy): Sentinel File Block Reader
Using the two-argument form of `iter()`, write a function `read_blocks(stream, block_size=16)` that reads a binary stream until EOF (empty byte string `b""`) and returns the total number of blocks read.
<details>
<summary><strong>Solution</strong></summary>

```python
import io

def read_blocks(stream: io.BytesIO, block_size: int = 16) -> int:
    block_iterator = iter(lambda: stream.read(block_size), b"")
    return sum(1 for _ in block_iterator)

stream = io.BytesIO(b"A" * 50)
assert read_blocks(stream, 16) == 4  # 16 + 16 + 16 + 2 bytes = 4 blocks
```
</details>

### Challenge 3 (Medium): Reusable Reversing Sequence
Implement an iterable class `ReverseList(data: list)` that does not mutate `data` and can be iterated multiple times in reverse order, returning a fresh iterator on each `__iter__()` call.
<details>
<summary><strong>Solution</strong></summary>

```python
class ReverseList:
    def __init__(self, data: list) -> None:
        self.data = data

    def __iter__(self):
        return _ReverseListIterator(self.data)

class _ReverseListIterator:
    def __init__(self, data: list) -> None:
        self.data = data
        self.index = len(data) - 1

    def __iter__(self):
        return self

    def __next__(self):
        if self.index < 0:
            raise StopIteration
        val = self.data[self.index]
        self.index -= 1
        return val

rl = ReverseList([1, 2, 3])
assert list(rl) == [3, 2, 1]
assert list(rl) == [3, 2, 1]  # Reusable!
```
</details>

### Challenge 4 (Medium): Interleaved Zip Iterator
Implement an iterator `Interleave(it1, it2)` that takes two iterators and alternates yielding items: item from `it1`, then `it2`, then `it1`, stopping when either iterator is exhausted.
<details>
<summary><strong>Solution</strong></summary>

```python
class Interleave:
    def __init__(self, it1, it2) -> None:
        self.it1 = iter(it1)
        self.it2 = iter(it2)
        self.turn = 1

    def __iter__(self):
        return self

    def __next__(self):
        if self.turn == 1:
            val = next(self.it1)
            self.turn = 2
            return val
        else:
            val = next(self.it2)
            self.turn = 1
            return val

result = list(Interleave(["a", "b", "c"], [1, 2, 3]))
assert result == ["a", 1, "b", 2, "c", 3]
```
</details>

### Challenge 5 (Hard): Peekable Iterator Wrapper
Implement an iterator wrapper `Peekable(iterable)` that wraps any iterator, providing a `.peek(default=None)` method to inspect the next item without consuming it, while maintaining standard `__next__()` protocol.
<details>
<summary><strong>Solution</strong></summary>

```python
class Peekable:
    _SENTINEL = object()

    def __init__(self, iterable) -> None:
        self.iterator = iter(iterable)
        self._buffered = self._SENTINEL

    def __iter__(self) -> "Peekable":
        return self

    def peek(self, default=None):
        if self._buffered is self._SENTINEL:
            try:
                self._buffered = next(self.iterator)
            except StopIteration:
                return default
        return self._buffered

    def __next__(self):
        if self._buffered is not self._SENTINEL:
            val = self._buffered
            self._buffered = self._SENTINEL
            return val
        return next(self.iterator)

p = Peekable([10, 20, 30])
assert p.peek() == 10
assert p.peek() == 10
assert next(p) == 10
assert p.peek() == 20
assert next(p) == 20
assert next(p) == 30
assert p.peek(default="END") == "END"
```
</details>

### Challenge 6 (Hard): Windowed N-Gram Sliding Iterator
Implement an iterator class `SlidingWindow(iterable, size=3)` that yields overlapping tuples of length `size` with $O(size)$ memory overhead, without converting the entire input iterable into a list.
<details>
<summary><strong>Solution</strong></summary>

```python
from collections import deque

class SlidingWindow:
    def __init__(self, iterable, size: int = 3) -> None:
        if size <= 0:
            raise ValueError("Window size must be positive")
        self.iterator = iter(iterable)
        self.size = size
        self.window = deque()
        self._initialized = False

    def __iter__(self) -> "SlidingWindow":
        return self

    def __next__(self) -> tuple:
        if not self._initialized:
            for _ in range(self.size):
                self.window.append(next(self.iterator))
            self._initialized = True
            return tuple(self.window)

        # Slide window by 1
        next_item = next(self.iterator)
        self.window.popleft()
        self.window.append(next_item)
        return tuple(self.window)

res = list(SlidingWindow([1, 2, 3, 4, 5], size=3))
assert res == [(1, 2, 3), (2, 3, 4), (3, 4, 5)]
```
</details>
