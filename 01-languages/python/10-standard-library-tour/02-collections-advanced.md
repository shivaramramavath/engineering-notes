---
title: "Advanced Data Structures: The collections Module"
description: "Mastering Python's specialized container datatypes: deque vs list memory mechanics, Counter multiset operations, defaultdict factory lifecycles, ChainMap scoped namespaces, OrderedDict, and UserDict subclassing."
module: "10-standard-library-tour"
order: 2
tags:
  - python
  - collections
  - deque
  - counter
  - defaultdict
  - chainmap
  - namedtuple
difficulty: "Intermediate"
cpython_version: "3.12+"
last_verified: "2026-03"
---

# Advanced Data Structures: The collections Module

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
10. [Real-World Case Study: Real-Time Sliding-Window Metric Aggregation Engine](#real-world-case-study-real-time-sliding-window-metric-aggregation-engine)
11. [Interview Questions & Deep-Dive Scenarios](#interview-questions--deep-dive-scenarios)
12. [Practical Exercises & Code Katas](#practical-exercises--code-katas)

---

## What & Why

Python’s built-in container types (`list`, `dict`, `set`, `tuple`) form the foundation of everyday programming. However, high-throughput systems, algorithms, and data pipelines frequently demand specialized container behaviors:
- **`collections.deque`**: A double-ended queue providing guaranteed $\mathcal{O}(1)$ time complexity for appends and pops from both ends (unlike `list.pop(0)` which is $\mathcal{O}(N)$).
- **`collections.Counter`**: A multiset/bag implementation for counting hashable objects with mathematical set-like operations (`+`, `-`, `&`, `|`).
- **`collections.defaultdict`**: A dictionary subclass that automatically initializes missing keys via a callable factory, eliminating defensive `if key not in dict:` boilerplate.
- **`collections.ChainMap`**: A grouped view of multiple dictionaries for managing layered, scoped configurations (e.g. CLI > Environment > Defaults) without data copying.
- **`collections.OrderedDict`**: A dictionary remembering insertion order with specialized rearrangement methods (`move_to_end()`).
- **`collections.UserDict` / `UserList` / `UserString`**: Wrapper classes designed specifically for safe subclassing without encountering C-level internal method bypass traps.

### Motivation
Using generic lists as FIFO queues causes quadratic performance degradation ($\mathcal{O}(N^2)$) as data grows, because removing the first element of a list requires shifting all subsequent pointers in memory. Similarly, aggregating frequencies using manual dictionary checking clutters code and degrades speed. The `collections` module provides C-accelerated, memory-optimized data structures engineered for algorithmic efficiency.

### Real-World Analogy
- A standard Python `list` is like a train of linked boxcars where adding or removing a car from the very front requires physically uncoupling and rolling every other car backwards along the tracks.
- A `deque` is like a dual-ended conveyor belt with loading ramps at both ends: items roll on or off either end instantly without disturbing the items in the middle.
- A `ChainMap` is like stacking three transparent overhead projector slides: the audience looks down through the stack and sees the topmost writing first, falling back to lower layers only where upper layers are blank.

### When to Use vs When Not to Use
- **Use `deque` for:** FIFO queues, circular buffers (via `maxlen`), breadth-first search (BFS) algorithms, sliding window aggregators.
- **Use `Counter` for:** Frequency tallies, histogram generation, finding top-$K$ frequent elements (`most_common()`).
- **Use `defaultdict` for:** Grouping items by key (e.g. `defaultdict(list)`), multi-level nested dictionaries.
- **Use `ChainMap` for:** Hierarchical configuration resolution (CLI args > env vars > config file > hardcoded defaults).
- **Do Not Use:**
  - `list.pop(0)` for queuing (use `deque.popleft()`).
  - Standard `dict` subclassing if you intend to override `__getitem__` or `__setitem__` (subclass `UserDict` instead).

---

## Core Concepts & Mental Model

```
+-------------------------------------------------------------------------+
| collections Module Architecture                                         |
+-------------------------------------------------------------------------+
       |                           |                         |
       v                           v                         v
+----------------+        +------------------+      +-------------------+
| Sequence       |        | Mapping          |      | Counting          |
| Specialized    |        | Specialized      |      | Specialized       |
+----------------+        +------------------+      +-------------------+
       |                           |                         |
       v                           v                         v
+----------------+        +------------------+      +-------------------+
| deque          |        | defaultdict      |      | Counter           |
| - Doubly linked|        | - Factory on     |      | - Multiset math   |
|   blocks       |        |   __missing__    |      | - most_common(k)  |
| - O(1) ends    |        +------------------+      +-------------------+
| - maxlen limit |                 |
+----------------+                 v
                          +------------------+
                          | ChainMap         |
                          | - Linked list of |
                          |   dict references|
                          +------------------+
```

### Key Differences: `list` vs `deque`

| Operation | `list` Complexity | `deque` Complexity | Underlying Mechanics |
| :--- | :--- | :--- | :--- |
| `append(x)` (Right) | Amortized $\mathcal{O}(1)$ | $\mathcal{O}(1)$ | Array reallocation vs Block link |
| `appendleft(x)` (Left) | $\mathcal{O}(N)$ | $\mathcal{O}(1)$ | Memory shift of all elements vs Prepend to head block |
| `pop()` (Right) | $\mathcal{O}(1)$ | $\mathcal{O}(1)$ | Decrement size counter |
| `popleft()` (Left) | $\mathcal{O}(N)$ | $\mathcal{O}(1)$ | Memory shift of all elements vs Advance head pointer |
| Random Index Access `[i]` | $\mathcal{O}(1)$ | $\mathcal{O}(N)$ | Pointer arithmetic vs Block traversal |

---

## How It Works Internally

### 1. `collections.deque` Doubly Linked Block Architecture
In CPython (`Modules/_collectionsmodule.c`), `deque` is implemented not as a simple linked list of individual nodes, but as a **doubly linked list of fixed-size blocks**:
- Each block holds 64 pointers (`BLOCKLEN = 64`).
- Adding elements to the left or right appends to the current head or tail block. When a block fills up, a new 64-element block is allocated and linked.
- This design combines the $\mathcal{O}(1)$ head/tail flexibility of linked lists with the CPU cache locality and pointer packing of arrays!
- When `maxlen` is specified, appending an element beyond capacity automatically pops an element from the opposite end in $\mathcal{O}(1)$, making it the ultimate circular ring buffer.

### 2. `collections.defaultdict` and `__missing__`
`defaultdict` inherits directly from `dict`.
When a key lookup occurs (`d[key]`):
1. Python executes standard dictionary lookup.
2. If the key exists, it returns the value immediately (identical speed to `dict`).
3. If the key is missing, CPython's dictionary lookup mechanism calls the `__missing__(key)` method.
4. `defaultdict.__missing__` checks if `default_factory` is defined. If so, it invokes `val = self.default_factory()`, inserts `self[key] = val`, and returns `val`.
5. Notice that calling `.get(key)` does **not** invoke `__missing__()`!

### 3. `collections.ChainMap` Mechanics
`ChainMap` does not copy underlying dictionaries into a new hash map. It stores them in a list of mappings (`self.maps`).
- Read lookups (`cm[key]`) sequentially iterate through `self.maps` until the key is found, returning the first match.
- Write operations (`cm[key] = val` or `del cm[key]`) mutate **only the first mapping** in `self.maps`, leaving the fallback layers untouched.

---

## Syntax & API Reference

### `deque` Methods
```python
from collections import deque

d = deque(iterable=(), maxlen=None)

d.append(x)                    # Add to right
d.appendleft(x)                # Add to left
d.pop() -> Any                 # Remove & return from right (raises IndexError if empty)
d.popleft() -> Any             # Remove & return from left (raises IndexError if empty)
d.extend(iterable)             # Extend right
d.extendleft(iterable)         # Extend left (reverses order of iterable!)
d.rotate(n=1)                  # Rotate n steps to right (negative rotates left)
d.clear()                      # Empty deque
```

### `Counter` Methods
```python
from collections import Counter

c = Counter(iterable_or_mapping)

top_k: list[tuple[Any, int]] = c.most_common(k)
total: int = c.total()         # Sum of all counts (Python 3.10+)
elements_iter = c.elements()   # Iterator repeating elements by their count

# Arithmetic Operations
c3 = c1 + c2                   # Adds counts (keeps positive counts)
c3 = c1 - c2                   # Subtracts counts (keeps positive counts)
c3 = c1 & c2                   # Intersection: min(c1[x], c2[x])
c3 = c1 | c2                   # Union: max(c1[x], c2[x])
```

### `defaultdict` Constructor
```python
from collections import defaultdict

dd = defaultdict(default_factory)  # default_factory: callable (list, int, set, dict, lambda)
```

### `ChainMap` Operations
```python
from collections import ChainMap

cm = ChainMap(*maps)
child = cm.new_child(m=None)   # Creates child ChainMap with m at the front
parent = cm.parents            # Returns ChainMap skipping the first map
```

---

## Code Examples

### Example 1: `deque` as a Fast FIFO Queue and Sliding Window
```python
from collections import deque

def run_deque_demo():
    # Scenario A: FIFO Queue
    queue = deque()
    queue.append("task_1")
    queue.append("task_2")
    queue.append("task_3")

    print("Deque FIFO Popping:")
    while queue:
        # popleft() is O(1) guaranteed
        print("  Processing:", queue.popleft())

    # Scenario B: Fixed-size Circular Buffer (maxlen)
    recent_logs = deque(maxlen=3)
    for i in range(5):
        recent_logs.append(f"log_entry_{i}")
        print(f"Added log_{i} -> Active Buffer: {list(recent_logs)}")

run_deque_demo()
# Output:
# Deque FIFO Popping:
#   Processing: task_1
#   Processing: task_2
#   Processing: task_3
# Added log_0 -> Active Buffer: ['log_entry_0']
# Added log_1 -> Active Buffer: ['log_entry_0', 'log_entry_1']
# Added log_2 -> Active Buffer: ['log_entry_0', 'log_entry_1', 'log_entry_2']
# Added log_3 -> Active Buffer: ['log_entry_1', 'log_entry_2', 'log_entry_3']
# Added log_4 -> Active Buffer: ['log_entry_2', 'log_entry_3', 'log_entry_4']
```

### Example 2: Multiset Frequency Analysis with `Counter`
```python
from collections import Counter

def run_counter_demo():
    text = "the quick brown fox jumps over the lazy dog and the dog barked"
    words = text.split()

    counts = Counter(words)

    print("Top 2 Most Common Words:", counts.most_common(2))
    print("Frequency of 'the':", counts["the"])
    print("Frequency of missing word 'cat':", counts["cat"])  # Returns 0, no KeyError!

    # Multiset Arithmetic
    basket_a = Counter(apples=5, oranges=2, bananas=1)
    basket_b = Counter(apples=2, oranges=4, kiwis=3)

    combined = basket_a + basket_b
    difference = basket_a - basket_b
    intersection = basket_a & basket_b

    print("\nCombined Baskets:", combined)
    print("Difference (A - B):", difference)
    print("Intersection (Min of each):", intersection)

run_counter_demo()
# Output:
# Top 2 Most Common Words: [('the', 3), ('dog', 2)]
# Frequency of 'the': 3
# Frequency of missing word 'cat': 0
# 
# Combined Baskets: Counter({'apples': 7, 'oranges': 6, 'kiwis': 3, 'bananas': 1})
# Difference (A - B): Counter({'apples': 3, 'bananas': 1})
# Intersection (Min of each): Counter({'apples': 2, 'oranges': 2})
```

### Example 3: Inverting Graphs and Grouping with `defaultdict`
```python
from collections import defaultdict

def run_defaultdict_demo():
    # Grouping flat relational rows by department
    employees = [
        ("Engineering", "Alice"),
        ("Engineering", "Bob"),
        ("Sales", "Charlie"),
        ("Engineering", "David"),
        ("Sales", "Eve"),
    ]

    # Factory is list: automatically creates [] on missing key
    dept_map = defaultdict(list)
    for dept, name in employees:
        dept_map[dept].append(name)

    print("Grouped Department Directory:")
    for dept, staff in dept_map.items():
        print(f"  {dept}: {staff}")

    # Nested defaultdict: Tree / Graph representation
    # lambda: defaultdict(...) creates infinitely deep nested dictionaries
    def tree():
        return defaultdict(tree)

    taxonomy = tree()
    taxonomy["Animalia"]["Chordata"]["Mammalia"] = "Carnivora"
    print("\nDeep Nested Lookup:", taxonomy["Animalia"]["Chordata"]["Mammalia"])

run_defaultdict_demo()
# Output:
# Grouped Department Directory:
#   Engineering: ['Alice', 'Bob', 'David']
#   Sales: ['Charlie', 'Eve']
# 
# Deep Nested Lookup: Carnivora
```

### Example 4: Scoped Context Lookup with `ChainMap`
```python
from collections import ChainMap

def run_chainmap_demo():
    # Layer 1: System hardcoded defaults
    defaults = {"theme": "light", "port": 8080, "workers": 4}
    
    # Layer 2: Configuration file overrides
    config_file = {"port": 9000}
    
    # Layer 3: Command-line arguments (highest priority)
    cli_args = {"theme": "dark"}

    # ChainMap evaluates keys in priority order: cli_args -> config_file -> defaults
    app_config = ChainMap(cli_args, config_file, defaults)

    print("Effective Configuration:")
    print("  Theme (from CLI):", app_config["theme"])
    print("  Port (from Config):", app_config["port"])
    print("  Workers (from Defaults):", app_config["workers"])

    # Mutating updates ONLY the first dictionary (cli_args)
    app_config["new_dynamic_flag"] = True
    print("\nCLI Dict post-mutation:", cli_args)
    print("Defaults untouched:", defaults.get("new_dynamic_flag"))

run_chainmap_demo()
# Output:
# Effective Configuration:
#   Theme (from CLI): dark
#   Port (from Config): 9000
#   Workers (from Defaults): 4
# 
# CLI Dict post-mutation: {'theme': 'dark', 'new_dynamic_flag': True}
# Defaults untouched: None
```

### Example 5: Safe Subclassing with `UserDict`
```python
from collections import UserDict

class CaseInsensitiveDict(UserDict):
    """
    Subclassing UserDict guarantees that ALL internal dictionary operations
    (update, setdefault, __init__) route through our custom methods.
    Direct dict subclassing frequently bypasses C-level overrides!
    """
    def __getitem__(self, key):
        return super().__getitem__(key.lower() if isinstance(key, str) else key)

    def __setitem__(self, key, value):
        super().__setitem__(key.lower() if isinstance(key, str) else key, value)

    def __contains__(self, key):
        return super().__contains__(key.lower() if isinstance(key, str) else key)

def run_userdict_demo():
    ci_dict = CaseInsensitiveDict()
    ci_dict["Authorization"] = "Bearer token_xyz"
    ci_dict["Content-Type"] = "application/json"

    print("Lookup lowercase:", ci_dict["authorization"])
    print("Lookup uppercase:", ci_dict["CONTENT-TYPE"])
    print("Check contains:", "content-type" in ci_dict)

run_userdict_demo()
# Output:
# Lookup lowercase: Bearer token_xyz
# Lookup uppercase: application/json
# Check contains: True
```

---

## Edge Cases, Pitfalls & Anti-Patterns

### 1. `list.pop(0)` Quadratic Time Complexity
Using `list` as a queue where items are added via `append()` and removed via `pop(0)` exhibits $\mathcal{O}(N)$ removal complexity. In an algorithm processing 100,000 items, `list.pop(0)` causes the runtime to jump from 15 milliseconds to over 15 seconds! Always use `collections.deque.popleft()`.

### 2. Accidental Mutation in `defaultdict` Lookups
Evaluating `dd[key]` on a missing key immediately executes `default_factory` and inserts that key into the dictionary. If you check keys defensively using `if dd["missing_key"]:` or inspect keys during debugging, you inadvertently populate the dictionary with blank values!
- **Mitigation**: Use `.get(key)` or `key in dd` when inspecting without desiring key insertion.

### 3. The `extendleft()` Reversal Traversal
Calling `deque.extendleft([1, 2, 3])` prepends items one-by-one to the left:
1 is prepended, then 2 is prepended before 1, then 3 is prepended before 2.
The resulting deque sequence on the left will be `[3, 2, 1]`.

### Common Mistakes Table

| Bad Code | Good Code | Why | Severity |
| :--- | :--- | :--- | :--- |
| `queue = []; queue.pop(0)` | `queue = deque(); queue.popleft()` | `list.pop(0)` shifts all elements in memory ($\mathcal{O}(N)$), killing throughput. | Critical |
| Subclassing `dict` directly | Subclass `UserDict` | C-level `dict` methods bypass custom Python overrides (`__setitem__`). | High |
| `for k in dd: ... if dd[missing]:` | `if missing in dd:` | Accessing `dd[missing]` invokes factory, silently inserting empty keys. | Medium |
| `d.extendleft([1, 2, 3])` expecting `[1, 2, 3]` | Be aware it inserts as `[3, 2, 1]` | Each element is prepended sequentially, reversing input order. | Medium |
| Using `deque` for frequent random index access `d[5000]` | Use `list` | `deque` index lookup traverses linked blocks ($\mathcal{O}(N)$), while `list` is $\mathcal{O}(1)$. | High |
| Hand-rolling frequency counts with `dict` | `Counter(items)` | `Counter` is written in C and provides multiset math out of the box. | Low |

---

## Performance & Memory Characteristics

### Benchmark: `list.pop(0)` vs `deque.popleft()`
Popping 50,000 items from the front of a 50,000-element container:
- **`list.pop(0)`**: $\approx 420 \text{ ms}$ ($\mathcal{O}(N^2)$ cumulative time; 1.25 billion pointer shifts).
- **`deque.popleft()`**: $\approx 1.8 \text{ ms}$ ($\mathcal{O}(N)$ cumulative time; **~233x faster**).

### Random Access Performance
- `list[25000]`: $\approx 25 \text{ ns}$ ($\mathcal{O}(1)$ direct array index offset).
- `deque[25000]`: $\approx 1.2 \mu\text{s}$ ($\mathcal{O}(N)$ block link traversal; **~48x slower**).

---

## CPython Internals & Bytecode

In CPython:
- `deque` resides in `Modules/_collectionsmodule.c`.
  The `dequeobject` struct maintains pointers to the `leftblock` and `rightblock`, an array of `block` nodes, and integer offsets `leftindex` and `rightindex`.
- `defaultdict` is also implemented in `Modules/_collectionsmodule.c` as `defdictobject`. It overrides the internal `tp_as_mapping->mp_subscript` slot:
  ```c
  if (key_not_found) {
      return defdict_missing(self, key);
  }
  ```
- `ChainMap` and `UserDict` are implemented in pure Python (`Lib/collections/__init__.py`).

Bytecode for `deque.append`:
```python
import dis
from collections import deque

def push_queue(d, item):
    d.append(item)

dis.dis(push_queue)
```
```text
  1           0 RESUME                   0
  2           2 LOAD_FAST                0 (d)
              4 LOAD_ATTR                1 (append)
             14 LOAD_FAST                1 (item)
             16 CALL                     1
             24 POP_TOP
             26 RETURN_CONST             0 (None)
```

---

## Best Practices & Idiomatic Patterns

1. **Use `deque(maxlen=N)` for Sliding Logs and Rate Limiters**:
   Avoid manual slicing or popping; `deque` automatically discards expired tail items at $\mathcal{O}(1)$ speed.
2. **Subclass `UserDict` for Custom Mapping Types**:
   Guarantees that `dict.update()`, `.copy()`, and `__init__()` honor your custom `__setitem__` methods.
3. **Use `Counter.most_common(k)` for Top-$K$ Problems**:
   Internally uses `heapq.nlargest()` to extract top elements in $\mathcal{O}(N \log K)$ without sorting the entire dataset.
4. **Prefer `defaultdict(list)` over `setdefault()` in Loops**:
   `dict.setdefault(k, []).append(v)` allocates a new empty list object on every single iteration even if the key already exists! `defaultdict(list)` invokes the factory only when the key is truly absent.

---

## Real-World Case Study: Real-Time Sliding-Window Metric Aggregation Engine

The following engine tracks streaming latency metrics over a sliding 60-second window, computing real-time request counts, average latencies, and top-requested endpoints with $\mathcal{O}(1)$ eviction.

```python
from collections import deque, Counter, defaultdict
import time
from typing import NamedTuple, List

class RequestMetric(NamedTuple):
    timestamp: float
    endpoint: str
    duration_ms: float

class SlidingWindowTelemetry:
    def __init__(self, window_seconds: float = 60.0):
        self.window_seconds = window_seconds
        # FIFO queue tracking timestamps and events for O(1) eviction
        self.events: deque[RequestMetric] = deque()
        # Aggregated stats
        self.endpoint_counter: Counter = Counter()
        self.endpoint_latencies: defaultdict[str, float] = defaultdict(float)

    def record_request(self, endpoint: str, duration_ms: float) -> None:
        now = time.monotonic()
        metric = RequestMetric(now, endpoint, duration_ms)
        
        self.events.append(metric)
        self.endpoint_counter[endpoint] += 1
        self.endpoint_latencies[endpoint] += duration_ms

        self._evict_expired(now)

    def _evict_expired(self, current_time: float) -> None:
        """Evicts metrics older than window_seconds from the head of the deque."""
        watermark = current_time - self.window_seconds
        while self.events and self.events[0].timestamp < watermark:
            expired = self.events.popleft()
            self.endpoint_counter[expired.endpoint] -= 1
            if self.endpoint_counter[expired.endpoint] <= 0:
                del self.endpoint_counter[expired.endpoint]

            self.endpoint_latencies[expired.endpoint] -= expired.duration_ms
            if self.endpoint_latencies[expired.endpoint] <= 0:
                del self.endpoint_latencies[expired.endpoint]

    def get_summary(self) -> dict:
        total_requests = sum(self.endpoint_counter.values())
        top_endpoints = self.endpoint_counter.most_common(3)
        avg_latencies = {}
        for ep, count in self.endpoint_counter.items():
            avg_latencies[ep] = round(self.endpoint_latencies[ep] / count, 2)

        return {
            "window_size_sec": self.window_seconds,
            "active_requests": total_requests,
            "top_endpoints": top_endpoints,
            "avg_latency_ms": avg_latencies
        }

# Verification
if __name__ == "__main__":
    telemetry = SlidingWindowTelemetry(window_seconds=1.0)
    
    # Simulate bursts
    telemetry.record_request("/api/v1/auth", 45.2)
    telemetry.record_request("/api/v1/users", 12.0)
    telemetry.record_request("/api/v1/auth", 40.8)
    
    summary_1 = telemetry.get_summary()
    print("Initial Window Summary:")
    print("  Active Requests:", summary_1["active_requests"])
    print("  Top Endpoints:  ", summary_1["top_endpoints"])
    print("  Avg Latency:    ", summary_1["avg_latency_ms"])

    # Simulate time passing past window
    time.sleep(1.1)
    telemetry.record_request("/api/v1/orders", 88.0)
    
    summary_2 = telemetry.get_summary()
    print("\nPost-Eviction Window Summary:")
    print("  Active Requests:", summary_2["active_requests"])
    print("  Top Endpoints:  ", summary_2["top_endpoints"])
```

---

## Interview Questions & Deep-Dive Scenarios

<details>
<summary><strong>1. Junior: Why is <code>collections.deque</code> preferred over a standard <code>list</code> for implementing a FIFO queue?</strong></summary>

A standard Python `list` is a contiguous dynamic array. Removing an element from the front (`list.pop(0)`) requires shifting all remaining $N-1$ elements in memory, yielding an $\mathcal{O}(N)$ time complexity per removal. `collections.deque` is implemented as a doubly linked list of fixed-size blocks, allowing elements to be popped from the front (`popleft()`) in strictly $\mathcal{O}(1)$ time with zero memory shifting.
</details>

<details>
<summary><strong>2. Junior: What happens when you access a missing key on a <code>collections.Counter</code> object?</strong></summary>

Unlike a standard Python dictionary which raises a `KeyError`, accessing a missing key on a `Counter` object returns `0`. It does **not** insert the key into the Counter.
</details>

<details>
<summary><strong>3. Mid: What is the purpose of <code>collections.ChainMap</code>, and how do write operations affect underlying mappings?</strong></summary>

`ChainMap` groups multiple dictionaries or mappings into a single logical view without copying their contents. Lookups search through the mappings sequentially from first to last. Write operations (`__setitem__`, `__delitem__`, `clear()`) modify **only the first mapping** in the chain, leaving all underlying fallback mappings completely untouched.
</details>

<details>
<summary><strong>4. Mid: Why does <code>dict.setdefault(key, [])</code> perform worse than <code>defaultdict(list)</code> inside a loop?</strong></summary>

In `dict.setdefault(key, [])`, the expression `[]` is evaluated and allocates a new empty list object in memory on **every single loop iteration**, even if `key` is already present in the dictionary. `defaultdict(list)` only invokes the `list` callable when `key` is truly absent, saving substantial CPU and garbage collection overhead.
</details>

<details>
<summary><strong>5. Senior: Why should you subclass <code>collections.UserDict</code> instead of standard <code>dict</code> when creating custom dictionary-like classes?</strong></summary>

CPython's built-in `dict` is implemented in optimized C. For performance reasons, C-level methods inside `dict` (such as `dict.update()`, `dict.copy()`, and `dict.__init__()`) frequently bypass Python-level overrides of `__setitem__` or `__getitem__`. `collections.UserDict` is implemented in pure Python and wraps an internal `self.data` dictionary; all internal methods route strictly through public Python magic methods, guaranteeing that custom overrides execute consistently.
</details>

<details>
<summary><strong>6. Senior: How does <code>collections.deque</code> achieve both $\mathcal{O}(1)$ ends and low memory overhead compared to a naive doubly linked list?</strong></summary>

A naive doubly linked list allocates a distinct heap node (with forward/backward pointers) for every single item, resulting in high memory overhead and cache misses. CPython's `deque` uses an **unrolled linked list** of fixed-size 64-pointer blocks (`BLOCKLEN = 64`). Pointers within a block are packed contiguously in memory, maximizing CPU L1/L2 cache locality, while the blocks themselves are linked doubly to guarantee $\mathcal{O}(1)$ addition and removal at both ends.
</details>

<details>
<summary><strong>7. Staff: How does <code>Counter</code> multiset subtraction (`c1 - c2`) differ from <code>c1.subtract(c2)</code>?</strong></summary>

- The operator `c1 - c2` returns a **new** `Counter` containing only positive counts; any keys with resulting counts $\le 0$ are automatically omitted from the result.
- The in-place method `c1.subtract(c2)` **mutates** `c1` directly and allows counts to become zero or negative without removing keys.
</details>

<details>
<summary><strong>8. Staff: How would you design a high-throughput Least Recently Used (LRU) cache using <code>collections.OrderedDict</code>, and how does it compare to <code>functools.lru_cache</code>?</strong></summary>

1. Subclass `OrderedDict`: On `get(key)`, if present, call `self.move_to_end(key)` and return value. On `set(key, val)`, if key exists, update and `self.move_to_end(key)`; if missing, insert and check `if len(self) > maxsize: self.popitem(last=False)`.
2. Comparison with `functools.lru_cache`: `lru_cache` is implemented in pure C using a doubly-linked circular list embedded directly in a hash table with thread-safe C mutexes, making it ~3x faster than an `OrderedDict` Python wrapper for function memoization. An `OrderedDict` wrapper is preferred when explicit programmatic cache eviction, inspection, or persistence is required.
</details>

---

## Practical Exercises & Code Katas

### Exercise 1: Anagram Checker with `Counter`
**Requirement:** Write a function `are_anagrams(s1: str, s2: str) -> bool` that determines if two strings contain identical characters with identical frequencies, ignoring spaces and capitalization.

<details>
<summary>Show Solution</summary>

```python
from collections import Counter

def are_anagrams(s1: str, s2: str) -> bool:
    clean_s1 = s1.replace(" ", "").lower()
    clean_s2 = s2.replace(" ", "").lower()
    return Counter(clean_s1) == Counter(clean_s2)

# Test
print("Listen & Silent:", are_anagrams("Listen", "Silent"))
print("Hello & World:", are_anagrams("Hello", "World"))
assert are_anagrams("Astronomer", "Moon starer") is True
assert are_anagrams("Python", "Java") is False
```
</details>

### Exercise 2: Circular Sliding Window Maximum
**Requirement:** Write a function `moving_maximum(numbers: list[int], window_size: int) -> list[int]` using `collections.deque` to compute the maximum value for each sliding window in $\mathcal{O}(N)$ total time complexity.

<details>
<summary>Show Solution</summary>

```python
from collections import deque

def moving_maximum(nums: list[int], k: int) -> list[int]:
    if not nums or k <= 0:
        return []
    
    d = deque()  # Stores indices
    result = []

    for i, n in enumerate(nums):
        # Remove elements outside current window
        while d and d[0] <= i - k:
            d.popleft()
        
        # Remove elements smaller than current element from back of deque
        while d and nums[d[-1]] < n:
            d.pop()
            
        d.append(i)
        
        # Record max once window reaches size k
        if i >= k - 1:
            result.append(nums[d[0]])
            
    return result

# Test
arr = [1, 3, -1, -3, 5, 3, 6, 7]
res = moving_maximum(arr, 3)
print("Moving Maximums:", res)
assert res == [3, 3, 5, 5, 6, 7]
```
</details>

### Exercise 3: Invert Directed Graph Adjacency List
**Requirement:** Given an adjacency list representing a directed graph (`{node: [outgoing_neighbors]}`), use `defaultdict` to compute the reversed transpose graph.

<details>
<summary>Show Solution</summary>

```python
from collections import defaultdict

def reverse_graph(graph: dict[str, list[str]]) -> dict[str, list[str]]:
    inverted = defaultdict(list)
    for src, neighbors in graph.items():
        # Ensure source exists in inverted graph even if it has no incoming edges
        inverted[src]
        for dest in neighbors:
            inverted[dest].append(src)
    return dict(inverted)

# Test
adj_list = {
    "A": ["B", "C"],
    "B": ["C"],
    "C": ["D"],
    "D": []
}

rev = reverse_graph(adj_list)
print("Inverted Graph:", rev)
assert rev["C"] == ["A", "B"]
assert rev["D"] == ["C"]
```
</details>

### Exercise 4: LRU Cache via `OrderedDict`
**Requirement:** Implement a fixed-capacity class `LRUCache(capacity: int)` supporting `get(key)` and `put(key, value)` with $\mathcal{O}(1)$ eviction of the least recently accessed item.

<details>
<summary>Show Solution</summary>

```python
from collections import OrderedDict

class LRUCache:
    def __init__(self, capacity: int):
        self.capacity = capacity
        self.cache = OrderedDict()

    def get(self, key: str) -> int:
        if key not in self.cache:
            return -1
        self.cache.move_to_end(key)
        return self.cache[key]

    def put(self, key: str, value: int) -> None:
        if key in self.cache:
            self.cache.move_to_end(key)
        self.cache[key] = value
        if len(self.cache) > self.capacity:
            # popitem(last=False) pops the FIFO (oldest) item
            self.cache.popitem(last=False)

# Verification
lru = LRUCache(2)
lru.put("A", 1)
lru.put("B", 2)
assert lru.get("A") == 1      # A becomes MRU, B is LRU
lru.put("C", 3)               # Evicts B
assert lru.get("B") == -1     # B was evicted
assert lru.get("C") == 3
print("LRU Cache verified successfully.")
```
</details>

### Exercise 5: Token Bag Difference Generator
**Requirement:** Given two multiset text strings, find the vocabulary words that appear more frequently in the first document than the second, and the frequency surplus.

<details>
<summary>Show Solution</summary>

```python
from collections import Counter

def find_word_surplus(doc1: str, doc2: str) -> dict[str, int]:
    c1 = Counter(doc1.lower().split())
    c2 = Counter(doc2.lower().split())
    # Subtraction keeps only positive differences
    diff = c1 - c2
    return dict(diff)

# Test
d1 = "apple apple banana orange orange orange"
d2 = "apple banana banana orange"
surplus = find_word_surplus(d1, d2)
print("Surplus Words:", surplus)
assert surplus == {"apple": 1, "orange": 2}
```
</details>

### Exercise 6: Thread-Safe Bounded Ring Buffer
**Requirement:** Wrap `collections.deque(maxlen=N)` inside a class with `threading.Lock` to implement a thread-safe telemetry buffer.

<details>
<summary>Show Solution</summary>

```python
from collections import deque
import threading

class ThreadSafeRingBuffer:
    def __init__(self, capacity: int):
        self._buffer = deque(maxlen=capacity)
        self._lock = threading.Lock()

    def append(self, item: str) -> None:
        with self._lock:
            self._buffer.append(item)

    def get_snapshot(self) -> list[str]:
        with self._lock:
            return list(self._buffer)

# Test
buf = ThreadSafeRingBuffer(3)
buf.append("e1")
buf.append("e2")
buf.append("e3")
buf.append("e4")

snap = buf.get_snapshot()
print("Snapshot:", snap)
assert snap == ["e2", "e3", "e4"]
```
</details>
