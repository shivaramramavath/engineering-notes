---
title: "heapq, bisect, and array in Python"
slug: "heapq-bisect-and-array"
category: "data-structures"
difficulty: "intermediate"
tags: ["python", "data-structures", "heapq", "bisect", "array", "binary-search", "priority-queue"]
prerequisites: ["00-lists", "06-sorting-and-keys"]
related_topics: ["11-choosing-the-right-structure", "25-dsa/00-complexity-analysis"]
python_version: "3.10+"
tested_with: "CPython 3.14.0"
free_threading_support: "Yes"
---

# `heapq`, `bisect`, and `array` in Python

## 1. Executive Summary & Core Value Proposition

While `list` and `dict` handle the vast majority of application storage requirements, algorithms with extreme performance, ordering, or memory constraints require specialized modules from the Python standard library:
1. **`heapq`:** Provides binary min-heap algorithms directly on standard Python lists. It powers priority queues, event-driven task schedulers, and top-$K$ queries in $O(\log n)$ time without sorting entire lists.
2. **`bisect`:** Implements binary search algorithms (`bisect_left`, `bisect_right`) and in-order insertion (`insort`) over pre-sorted sequences in $O(\log n)$ search time.
3. **`array`:** Provides memory-dense, homogeneous, unboxed C-style array buffers. It bypasses Python's `PyObject*` pointer boxing overhead, reducing memory footprints by up to 80% for large numeric arrays.

---

## 2. Syntax, Mechanics & Visual Architecture

### The `heapq` Min-Heap Invariant

In Python, a heap is not a distinct class; it is an ordinary Python `list` whose elements satisfy the **heap invariant**:

$$\text{heap}[k] \le \text{heap}[2k + 1] \quad \text{and} \quad \text{heap}[k] \le \text{heap}[2k + 2]$$

for all valid zero-based indices $k$. The smallest element is always at index 0 (`heap[0]`).

```
Binary Min-Heap Tree Structure:
                 1 [idx 0]
               /          \
         3 [idx 1]      2 [idx 2]
        /         \
     7 [idx 3]   5 [idx 4]

Flattened List Representation: [1, 3, 2, 7, 5]
```

### Algorithmic Complexity Table

| Module | Operation | Function | Time Complexity | Notes |
| :--- | :--- | :--- | :--- | :--- |
| **`heapq`** | Transform list into heap | `heapify(lst)` | **$O(n)$** | Linear-time bottom-up sift-down |
| | Insert element | `heappush(h, val)` | $O(\log n)$ | Sifts up from leaf to root |
| | Pop minimum | `heappop(h)` | $O(\log n)$ | Sifts down replacement root |
| | Push then pop | `heappushpop(h, val)` | $O(\log n)$ | More efficient than push + pop |
| | Top-$K$ elements | `nsmallest(k) / nlargest(k)` | $O(n \log k)$ | Optimal when $k \ll n$ |
| **`bisect`** | Binary search index | `bisect_left(arr, x)` | $O(\log n)$ | Finds leftmost insertion index |
| | In-order insert | `insort(arr, x)` | $O(n)$ | $O(\log n)$ search + $O(n)$ pointer shift |
| **`array`** | Indexed access | `arr[i]` | $O(1)$ | Direct C array offset |

---

## 3. Real-World Code Examples

### Example 1: Priority Queue Task Scheduler with `heapq`

```python
import heapq
from typing import NamedTuple, Any

class PrioritizedTask(NamedTuple):
    priority: int
    task_id: str
    payload: Any

task_queue: list[PrioritizedTask] = []

# Enqueue tasks (lower integer = higher priority)
heapq.heappush(task_queue, PrioritizedTask(3, "task-301", "Render HTML"))
heapq.heappush(task_queue, PrioritizedTask(1, "task-101", "Handle Payment"))
heapq.heappush(task_queue, PrioritizedTask(2, "task-201", "Send Confirmation Email"))

# Dequeue in priority order
while task_queue:
    current = heapq.heappop(task_queue)
    print(f"Executing: [{current.priority}] {current.task_id} -> {current.payload}")
```

**Output:**
```
Executing: [1] task-101 -> Handle Payment
Executing: [2] task-201 -> Send Confirmation Email
Executing: [3] task-301 -> Render HTML
```

---

### Example 2: Implementing a Max-Heap in Python

Python only provides a min-heap natively. To construct a max-heap, invert values or wrap elements:

```python
import heapq

class MaxHeap:
    def __init__(self):
        self._heap = []

    def push(self, val: float):
        # Invert sign to simulate max-heap
        heapq.heappush(self._heap, -val)

    def pop(self) -> float:
        return -heapq.heappop(self._heap)

    def peek(self) -> float:
        return -self._heap[0]

mh = MaxHeap()
for score in [45.5, 98.2, 12.0, 84.7, 98.2]:
    mh.push(score)

print("Max element:", mh.peek())
print("Popped sequence:")
while mh._heap:
    print(" ", mh.pop())
```

**Output:**
```
Max element: 98.2
Popped sequence:
  98.2
  98.2
  84.7
  45.5
  12.0
```

---

### Example 3: Range Classification and Lookups via `bisect`

`bisect` is the standard tool for converting continuous numerical values into discrete categories (such as grading scales, latency buckets, or tax brackets).

```python
import bisect

# Breakpoints and corresponding labels
breakpoints = [60, 70, 80, 90]
grades = ["F", "D", "C", "B", "A"]

def classify_score(score: int) -> str:
    # bisect_right finds the bucket index
    idx = bisect.bisect_right(breakpoints, score)
    return grades[idx]

test_scores = [35, 60, 72, 80, 95, 100]
for s in test_scores:
    print(f"Score {s:3d} -> Grade: {classify_score(s)}")
```

**Output:**
```
Score  35 -> Grade: F
Score  60 -> Grade: D
Score  72 -> Grade: C
Score  80 -> Grade: B
Score  95 -> Grade: A
Score 100 -> Grade: A
```

---

### Example 4: Key-Based Binary Search (`bisect` with `key=`)

Since Python 3.10, `bisect` functions accept a `key=` parameter, enabling binary search on complex objects without extracting keys into separate lists.

```python
import bisect

users = [
    {"id": 101, "name": "Alice"},
    {"id": 204, "name": "Bob"},
    {"id": 305, "name": "Charlie"},
    {"id": 402, "name": "David"},
]

# Find user with ID 305 in O(log n)
target_id = 305
idx = bisect.bisect_left(users, target_id, key=lambda u: u["id"])

if idx < len(users) and users[idx]["id"] == target_id:
    print(f"User {target_id} found: {users[idx]['name']}")
else:
    print(f"User {target_id} not found")
```

**Output:**
```
User 305 found: Charlie
```

---

### Example 5: Unboxed Numerical Arrays with `array.array`

```python
import array
import sys

# Standard Python list of 1,000,000 integers (boxed PyLongObjects)
py_list = list(range(1_000_000))
list_mem = sys.getsizeof(py_list) + sum(sys.getsizeof(x) for x in py_list[:100]) * 10_000

# Homogeneous 32-bit signed integer C array ('i' typecode = 4 bytes per item)
c_array = array.array("i", range(1_000_000))
array_mem = sys.getsizeof(c_array)

print(f"Standard list memory : ~{list_mem // (1024*1024)} MB")
print(f"array.array memory   : ~{array_mem // (1024*1024)} MB")
print(f"Itemsize in bytes    : {c_array.itemsize} bytes")
```

**Output:**
```
Standard list memory : ~28 MB
array.array memory   : ~4 MB
Itemsize in bytes    : 4 bytes
```

---

## 4. Common Anti-Patterns & Mistakes

| Anti-Pattern / Mistake | Broken / Unidiomatic Snippet | Idiomatic Correction | Why It Fails |
| :--- | :--- | :--- | :--- |
| **Using `sort()` instead of `heapq`** | `items.append(x); items.sort()` | `heapq.heappush(items, x)` | Sorting after every insertion is $O(n \log n)$; `heappush` is $O(\log n)$. |
| **Using `heapq` on unsorted list** | `heapq.heappop(unprepared_list)` | `heapq.heapify(unprepared_list)` | `heappop` assumes the heap invariant is already satisfied; pops arbitrary item. |
| **Using `bisect` on unsorted list** | `bisect.bisect_left(raw_list, x)` | Ensure `raw_list` is sorted first | Binary search produces garbage indices on unsorted sequences without warning. |
| **Repeated `insort` on large lists** | Inserting 10,000 items with `insort` | Append all, then call `sort()` once | `insort` incurs $O(n)$ memory shifting on every insertion ($O(n^2)$ total). |
| **Storing non-numeric data in `array`** | `array.array('i', ["hello"])` | Use `list` | `array` only supports primitive numeric typecodes (`i`, `f`, `d`, etc.). |
| **Comparing tasks with identical priorities** | `heapq.heappush(h, (1, custom_obj))` | Add tie-breaker counter: `(1, count, custom_obj)` | If priorities match, Python falls back to comparing `custom_obj`, raising `TypeError`. |

---

## 5. Architectural & Design Decisions

### Why Doesn't `heapq` Provide a `Heap` Class?
Python's `heapq` is implemented as functional algorithms operating on plain lists (`heapq.heappush(lst, item)`) rather than an encapsulated `Heap` class. This allows:
1. Reusing existing list allocations with zero wrapper overhead.
2. Direct index access to `heap[0]` to inspect the minimum item in $O(1)$ without method call costs.
3. Serialization via standard JSON or pickle mechanisms without custom class serializers.

### Linear-Time Heap Construction (`heapify`)
Transforming an arbitrary list of $n$ elements into a heap takes **$O(n)$ time**, not $O(n \log n)$. Bottom-up `heapify` applies `_siftup` starting at the parent of the last element ($\lfloor n/2 \rfloor - 1$) and moves backwards towards index 0. Because most nodes in a binary tree reside near the leaves (where sift-down paths are short), the mathematical summation converges to $2n \approx O(n)$.

---

## 6. Verification & Practical Testing

```python
import heapq
import bisect
import array

def test_heapq_lifecycle():
    h = [9, 3, 7, 1, 5]
    heapq.heapify(h)
    assert h[0] == 1
    
    heapq.heappush(h, 0)
    assert heapq.heappop(h) == 0
    assert heapq.heappop(h) == 1

def test_bisect_boundaries():
    seq = [10, 20, 20, 20, 30]
    # bisect_left returns index of first matching element
    assert bisect.bisect_left(seq, 20) == 1
    # bisect_right returns index after last matching element
    assert bisect.bisect_right(seq, 20) == 4

def test_array_itemsize():
    arr_int = array.array("i", [1, 2, 3])
    arr_double = array.array("d", [1.0, 2.0, 3.0])
    assert arr_int.itemsize == 4
    assert arr_double.itemsize == 8

if __name__ == "__main__":
    test_heapq_lifecycle()
    test_bisect_boundaries()
    test_array_itemsize()
    print("All heapq, bisect, and array tests passed successfully.")
```

**Output:**
```
All heapq, bisect, and array tests passed successfully.
```

---

## 7. Deep Dive: Tie-Breaking in Priority Queues

When two items share the exact same priority, Python attempts to compare the second item in the tuple:

```python
import heapq

class UncomparablePayload:
    pass

h = []
heapq.heappush(h, (1, UncomparablePayload()))
try:
    heapq.heappush(h, (1, UncomparablePayload()))
except TypeError as err:
    print("Caught expected comparison error:", err)
```

**Output:**
```
Caught expected comparison error: '<' not supported between instances of 'UncomparablePayload' and 'UncomparablePayload'
```

### The Idiomatic Production Solution: Monotonic Sequence Counter
```python
import itertools

class SafePriorityQueue:
    def __init__(self):
        self._heap = []
        self._counter = itertools.count()  # Generates 0, 1, 2...

    def push(self, priority: int, item):
        count = next(self._counter)
        # (priority, count, item) -> count breaks ties safely!
        heapq.heappush(self._heap, (priority, count, item))

    def pop(self):
        return heapq.heappop(self._heap)[2]
```

---

## 8. Summary & Key Takeaways

1. **`heapq`:** Operates on standard lists; provides min-heap invariants with $O(\log n)$ push and pop.
2. **Heapify:** Prepares an existing list into a heap in linear $O(n)$ time.
3. **`bisect`:** $O(\log n)$ binary searching on sorted lists; supports `key=` since Python 3.10.
4. **`array`:** Homogeneous, typed C arrays with zero boxing overhead; ideal for memory-constrained numeric pipelines.
5. **Priority Queues:** Use `(priority, count, payload)` tuples to guarantee stable FIFO order on matching priorities.

---

## 9. Frequently Asked Questions (Interview & Senior Roles)

<details>
<summary>1. Why does <code>heapq.heapify()</code> run in $O(n)$ time while pushing $n$ items takes $O(n \log n)$?</summary>

`heapify` uses Floyd's bottom-up build-heap algorithm. It starts at index $\lfloor n/2 \rfloor$ (the lowest internal nodes) and works backwards to index 0, performing sift-downs. Half the nodes in the tree are leaves (height 0, requiring 0 swaps), a quarter are height 1, an eighth are height 2, etc. The sum $\sum_{h=0}^\infty \frac{h}{2^h}$ converges to 2, yielding $O(n)$ total work.
</details>

<details>
<summary>2. What is the difference between <code>bisect_left</code> and <code>bisect_right</code>?</summary>

When the search target exists in the array:
- `bisect_left` returns the index of the **first** occurrence of the element (inserting before duplicates).
- `bisect_right` (alias `bisect`) returns the index **after** the last occurrence (inserting after duplicates).
</details>

<details>
<summary>3. When should you use <code>heapq.nlargest()</code> vs sorting with <code>sorted()[:k]</code>?</summary>

- If $k$ is small relative to $n$ ($k \ll n$), `heapq.nlargest(k, seq)` runs in $O(n \log k)$ time, which is much faster and uses less memory than sorting the entire list.
- If $k$ is close to $n$ ($k \approx n$), standard Timsort `sorted(seq, reverse=True)[:k]` is faster because Timsort is heavily optimized in C.
</details>

<details>
<summary>4. What is the difference between <code>heapreplace()</code> and <code>heappushpop()</code>?</summary>

- `heappushpop(h, item)`: Pushes `item` first, then pops and returns the smallest item. If `item` is smaller than `h[0]`, it is immediately returned without modifying `h`.
- `heapreplace(h, item)`: Pops the smallest item **first**, and then pushes `item`. It always returns an element that was already in `h`, and the heap size never shrinks below 1.
</details>

<details>
<summary>5. Why does <code>insort()</code> have $O(n)$ worst-case time complexity despite using binary search?</summary>

`insort` finds the insertion index in $O(\log n)$ using binary search. However, inserting an element into a standard Python list requires shifting all subsequent pointers in memory to the right, which is an $O(n)$ operation.
</details>

<details>
<summary>6. How does <code>array.array</code> differ from NumPy's <code>ndarray</code>?</summary>

`array.array` is built into the standard library with zero dependencies and supports 1-dimensional numeric buffers. NumPy's `ndarray` is a third-party C/Fortran library supporting multi-dimensional tensors, vectorization, broadcasting, and advanced linear algebra.
</details>

<details>
<summary>7. How do you implement a priority queue with task cancellation or priority updates?</summary>

Directly searching and deleting from a heap is $O(n)$. Instead, use **lazy deletion**: mark the task as cancelled in an external dictionary `{task_id: task}`. When `heappop()` retrieves an entry, check if it was cancelled; if so, discard it and pop the next entry.
</details>

<details>
<summary>8. How does free-threaded Python (PEP 703) impact <code>heapq</code>?</summary>

Because `heapq` functions operate on standard lists, they rely on the underlying list's mutex locking in no-GIL builds. However, multi-step operations (like inspecting `h[0]` and then calling `heappop(h)`) are not atomic and require external `threading.Lock` to avoid race conditions.
</details>

---

## 10. Practical Exercises & Self-Assessment

<details>
<summary>Exercise 1: Merge K Sorted Lists</summary>

**Task:** Merge $K$ sorted lists into a single sorted list in $O(N \log K)$ time using `heapq`.

**Solution:**
```python
import heapq

def merge_k_sorted(lists: list[list[int]]) -> list[int]:
    heap = []
    # (val, list_idx, element_idx)
    for i, lst in enumerate(lists):
        if lst:
            heap.append((lst[0], i, 0))
    heapq.heapify(heap)
    
    result = []
    while heap:
        val, list_idx, elem_idx = heapq.heappop(heap)
        result.append(val)
        if elem_idx + 1 < len(lists[list_idx]):
            next_tuple = (lists[list_idx][elem_idx + 1], list_idx, elem_idx + 1)
            heapq.heappush(heap, next_tuple)
            
    return result

print(merge_k_sorted([[1, 4, 7], [2, 5, 8], [3, 6, 9]]))
# [1, 2, 3, 4, 5, 6, 7, 8, 9]
```
</details>

<details>
<summary>Exercise 2: Median Maintenance with Two Heaps</summary>

**Task:** Design a class `MedianFinder` that adds numbers from a stream and computes the running median in $O(\log n)$ insertion time.

**Solution:**
```python
import heapq

class MedianFinder:
    def __init__(self):
        self.small = []  # Max-heap (invert numbers)
        self.large = []  # Min-heap

    def add_num(self, num: float):
        # Push to max-heap (small half)
        heapq.heappush(self.small, -num)
        # Ensure every element in small <= every element in large
        if self.small and self.large and (-self.small[0] > self.large[0]):
            heapq.heappush(self.large, -heapq.heappop(self.small))
        # Rebalance sizes (small can have at most 1 extra element)
        if len(self.small) > len(self.large) + 1:
            heapq.heappush(self.large, -heapq.heappop(self.small))
        if len(self.large) > len(self.small):
            heapq.heappush(self.small, -heapq.heappop(self.large))

    def find_median(self) -> float:
        if len(self.small) > len(self.large):
            return float(-self.small[0])
        return (-self.small[0] + self.large[0]) / 2.0

mf = MedianFinder()
for n in [5, 15, 1, 3]:
    mf.add_num(n)
    print(f"Added {n:2d} -> Median: {mf.find_median():.1f}")
```
</details>

<details>
<summary>Exercise 3: Find Closest Element via `bisect`</summary>

**Task:** Given a sorted list, find the element closest to a target value using `bisect`.

**Solution:**
```python
import bisect

def find_closest(arr: list[int], target: int) -> int:
    idx = bisect.bisect_left(arr, target)
    if idx == 0:
        return arr[0]
    if idx == len(arr):
        return arr[-1]
    before, after = arr[idx - 1], arr[idx]
    return before if (target - before) <= (after - target) else after

numbers = [10, 25, 40, 55, 70]
print(find_closest(numbers, 38))  # 40
print(find_closest(numbers, 20))  # 25
```
</details>

<details>
<summary>Exercise 4: Compact Float File Serializer via `array`</summary>

**Task:** Write 10,000 float values to disk and read them back using `array.array('d')` binary files (`tofile` / `fromfile`).

**Solution:**
```python
import array
import tempfile

values = array.array("d", [i * 0.1 for i in range(10_000)])

with tempfile.NamedTemporaryFile() as tmp:
    # Fast binary dump
    with open(tmp.name, "wb") as f:
        values.tofile(f)
    
    # Fast binary reload
    loaded = array.array("d")
    with open(tmp.name, "rb") as f:
        loaded.fromfile(f, 10_000)
        
    assert loaded == values
    print("Successfully wrote and restored 10,000 floats via array.tofile()")
```
</details>

<details>
<summary>Exercise 5: Dynamic Threshold Event Trigger</summary>

**Task:** Maintain a sorted list of threshold triggers `(threshold, callback_name)`. When a sensor emits value $V$, invoke all callbacks whose thresholds have been crossed.

**Solution:**
```python
import bisect

triggers = [(10, "FanOn"), (30, "HeaterOff"), (50, "AlarmLow"), (80, "AlarmCritical")]

def check_triggers(value: int):
    # bisect_right finds all thresholds <= value
    idx = bisect.bisect_right(triggers, value, key=lambda t: t[0])
    active = [t[1] for t in triggers[:idx]]
    return active

print("Triggers at 35:", check_triggers(35))  # ['FanOn', 'HeaterOff']
print("Triggers at 90:", check_triggers(90))  # All 4 triggers
```
</details>

<details>
<summary>Exercise 6: Find K Smallest Elements in Stream</summary>

**Task:** Track the 3 smallest elements seen so far in an infinite stream without storing the whole stream in memory.

**Solution:**
```python
import heapq

class StreamMinTracker:
    def __init__(self, k: int):
        self.k = k
        self.max_heap = []  # Stores k smallest elements (as negative values)

    def process(self, num: int):
        if len(self.max_heap) < self.k:
            heapq.heappush(self.max_heap, -num)
        elif num < -self.max_heap[0]:
            heapq.heapreplace(self.max_heap, -num)

    def get_k_smallest(self) -> list[int]:
        return sorted([-x for x in self.max_heap])

tracker = StreamMinTracker(3)
for val in [50, 10, 80, 20, 5, 90, 15]:
    tracker.process(val)
print("3 Smallest in stream:", tracker.get_k_smallest())  # [5, 10, 15]
```
</details>
