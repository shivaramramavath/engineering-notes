# Module 01: Variables, Names & Objects (01-variables-names-and-objects)

> **Topic Overview**: The Python object model: variables as bound references, mutable vs immutable objects, identity (`id()`) vs equality (`==`), the small integer cache, string interning, reference counting, and garbage collection semantics.

---

## Metadata Table

| Field | Value |
|---|---|
| **Category** | Python Fundamentals & Syntax |
| **Target Level** | Beginner to Intermediate |
| **Python Version** | Python 3.10+ (tested on CPython 3.14.0) |
| **Tested With** | CPython 3.14.0, sys module, Ruff 0.17.6 |
| **Free-Threading / Platform** | Supported; covers biased reference counting in PEP 703 |

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
- **Problem Statement**: Developers coming from statically typed languages (such as C++, Java, or Go) often conceptualize variables as "typed boxes" in memory that store raw binary values. In Python, this mental model fails completely, leading to baffling bugs when modifying lists, sharing dictionary arguments, or reassigning variables.
- **Mental Model**: In Python, **variables are not boxes; variables are name tags (references) attached to objects**. Objects reside on the heap and have types; variable names reside in local/global namespaces and have no type. Multiple variables can point to the exact same object simultaneously (aliasing).
- **When to Use**: Understanding reference semantics is required in every single Python program when passing arguments, mutating collections, and managing lifecycle.
- **When NOT to Use**: If you require value-copy semantics (independent copies), you must explicitly perform shallow or deep copies (`copy.copy()`, `copy.deepcopy()`).

---

## 2. Core Concepts
1. **Names as References**: An assignment statement `a = [1, 2, 3]` creates a list object on the heap and binds the identifier `a` in the current namespace to that object's memory address. Assigning `b = a` does *not* copy the list; it binds the identifier `b` to the identical object.
2. **Mutability vs Immutability**:
   - *Immutable Types*: Cannot be altered after creation (`int`, `float`, `bool`, `str`, `tuple`, `frozenset`, `bytes`). Operations that modify an immutable object create an entirely new object on the heap.
   - *Mutable Types*: Can have their internal state altered in-place without changing their memory address or identity (`list`, `dict`, `set`, `bytearray`, custom class instances).
3. **Identity (`is`) vs Equality (`==`)**:
   - `==` evaluates value equality via `__eq__()`.
   - `is` evaluates pointer identity via `id()` (`id(a) == id(b)`).
4. **CPython Small Integer Cache**: CPython pre-allocates and caches integers in the range `[-5, 256]` during startup. Any calculation producing an integer within this range returns the cached singleton object.
5. **Reference Counting & Deallocation**: Every `PyObject` carries an `ob_refcnt` field. When a variable goes out of scope or is reassigned, the refcount decrements. When it hits zero, memory is freed immediately.

---

## 3. How It Works Internally

Memory layout of variable binding and aliasing in CPython:

```text
Namespace (Dictionary / Stack Frame):
┌───────┬────────────────────────┐
│ Name  │ Pointer Target Address │
├───────┼────────────────────────┤
│ 'a'   │ 0x0000021A5890F400 ────┼────────┐
│ 'b'   │ 0x0000021A5890F400 ────┼─────┐  │
└───────┴────────────────────────┘     │  │
                                       ▼  ▼
Heap Memory:
┌──────────────────────────────────────────────────┐
│ PyListObject at 0x0000021A5890F400              │
├──────────────────────────────────────────────────┤
│ ob_refcnt = 2                                    │
│ ob_type   = <class 'list'>                       │
│ ob_size   = 3                                    │
│ ob_item   = [ptr_1, ptr_2, ptr_3]                │
└──────────────────────────────────────────────────┘
```
Mutating `b.append(4)` modifies the object in-place; because `a` points to the identical address, `a` immediately reflects the new element.

---

## 4. Syntax & API Reference

### Introspecting Identity, Types, and Refcounts
```python
import sys

x = [10, 20]

id(x)                    # Unique integer representing object's memory address
type(x)                  # <class 'list'>
isinstance(x, list)      # True
sys.getrefcount(x)       # Returns active reference count (+1 for the function call argument)
```

---

## 5. Code Examples

### 1. Minimal / Basic Example: Object Aliasing Demonstration
```python
a = [1, 2, 3]
b = a  # Both identifiers bind to the same list object

b.append(4)

print(f"a: {a}")
print(f"b: {b}")
print(f"id(a) == id(b): {id(a) == id(b)}")
print(f"a is b: {a is b}")
```
**Output**:
```text
a: [1, 2, 3, 4]
b: [1, 2, 3, 4]
id(a) == id(b): True
a is b: True
```

---

### 2. Real-World Production Example: Defending Against Accidental Mutation in Functions
```python
from collections.abc import Sequence

# Vulnerable: mutating parameter in place affects caller's data
def normalize_tags_mutating(tags: list[str]) -> list[str]:
    for i in range(len(tags)):
        tags[i] = tags[i].strip().lower()
    return tags

# Production-Grade: pure function returning a new list without mutating input
def normalize_tags_safe(tags: Sequence[str]) -> list[str]:
    """Returns a new normalized list without mutating caller's collection."""
    return [t.strip().lower() for t in tags]

raw_data = ["  Admin  ", "ENGINEER ", "DevOps"]
processed = normalize_tags_safe(raw_data)

print(f"Original list untouched: {raw_data}")
print(f"Processed new list:      {processed}")
```
**Output**:
```text
Original list untouched: ['  Admin  ', 'ENGINEER ', 'DevOps']
Processed new list:      ['admin', 'engineer', 'devops']
```

---

### 3. Edge Case: The Small Integer Cache Invariant
```python
# Integers from -5 to 256 are pre-allocated singletons:
x = 250
y = 250
print(f"250 is 250: {x is y}")  # True (same singleton)

# Larger integers outside the cache:
large_a = 10000
large_b = 10000
# In interactive REPL or separate frames, large integers have distinct memory addresses:
print(f"large_a == large_b: {large_a == large_b}")
print(f"id(large_a) == id(large_b): {id(large_a) == id(large_b)}")
```
**Output**:
```text
250 is 250: True
large_a == large_b: True
id(large_a) == id(large_b): True
```
*(Note: In module compilation units, constant folding can merge identical literals in the same code object's `co_consts`, but relying on `is` for integers remains an anti-pattern).*

---

### 4. Failure Case: Attempting to Mutate an Immutable Tuple Containing a Mutable List
```python
# The classic Python interview puzzle:
complex_tuple: tuple[int, list[int]] = (1, [10, 20])

try:
    # Attempting in-place augmentation on the inner list:
    complex_tuple[1] += [30]  # type: ignore[index]
except TypeError as exc:
    print(f"Caught expected exception: {type(exc).__name__}: {exc}")

# Crucial observation: The inner list WAS modified despite the exception!
print(f"Tuple state after exception: {complex_tuple}")
```
**Output**:
```text
Caught expected exception: TypeError: 'tuple' object does not support item assignment
Tuple state after exception: (1, [10, 20, 30])
```
*Why this occurs*: The `+=` operator executes `__iadd__` on the list, successfully mutating it in place. Then, it attempts to assign the result back to `complex_tuple[1]`, which triggers `TypeError` because tuples are immutable!

---

### 5. Idiomatic Pythonic Pattern: Explicit Copying via Slicing or Factory
```python
original_matrix = [[1, 2], [3, 4]]

# Shallow copy: outer list is new, inner lists are still shared!
shallow_copy = original_matrix.copy()
shallow_copy[0].append(99)
print("Shallow copy shares nested list:", original_matrix[0])

# Deep copy: fully independent clones across all levels
import copy
deep_matrix = [[1, 2], [3, 4]]
deep_copy = copy.deepcopy(deep_matrix)
deep_copy[0].append(999)
print("Deep copy preserves nested original:", deep_matrix[0])
print("Deep copy mutated independently:", deep_copy[0])
```
**Output**:
```text
Shallow copy shares nested list: [1, 2, 99]
Deep copy preserves nested original: [1, 2]
Deep copy mutated independently: [1, 2, 999]
```

---

## 6. Edge Cases & Pitfalls
1. **Re-binding vs In-Place Mutation**:
   - `x = x + [1]` creates an entirely new list object and rebinds `x` (does not affect aliases).
   - `x += [1]` or `x.append(1)` invokes `__iadd__`, mutating the object in-place (alters all aliases).
2. **`is` with Floats and Strings**: Never use `is` to compare floats or user-provided strings. While small strings can be interned, dynamic strings (`"".join(...)`) reside at distinct addresses.
3. **Reference Cycles**: When Object A references Object B and Object B references Object A, reference counts never drop to zero. CPython's cyclic garbage collector (`gc` module) is required to detect and collect these unreachable cycles.

---

## 7. Common Mistakes

| Mistake | Why It Fails | Exception / Diagnostic | Correct Pattern |
|---|---|---|---|
| Using `is` for integer comparisons | Works for small integers (-5 to 256), fails for larger numbers | Unpredictable logic bugs | Always use `==` for values |
| Assuming `b = a` copies a list | Both names bind to the exact same heap object | Accidental state mutation across variables | Use `b = a.copy()` or `list(a)` |
| Putting mutable objects inside tuples as hash keys | Tuples are hashable only if all elements are hashable | `TypeError: unhashable type: 'list'` | Use tuples containing only immutable types |
| Modifying a dictionary while iterating over it | Changes hash table size during iteration | `RuntimeError: dictionary changed size during iteration` | Iterate over `list(d.keys())` |
| Overwriting built-in names (`list = [1, 2]`) | Shadows the built-in type in local scope | `TypeError: 'list' object is not callable` | Use descriptive identifiers (`items`, `values`) |
| Creating matrix with `[[0] * 3] * 3` | Repeats the reference to the identical row list 3 times | Mutating one cell alters all rows simultaneously | Use `[[0 for _ in range(3)] for _ in range(3)]` |

---

## 8. Performance & Complexity
- **Binding / Assignment**: $O(1)$ constant time (pointer assignment in namespace dict).
- **`id(x)` Lookup**: $O(1)$ constant time (reads `(uintptr_t)x` in C).
- **Memory Overhead**: An empty CPython object (`object()`) consumes 16 bytes. An integer consumes 28 bytes (due to arbitrary precision digit arrays). A list consumes 56 bytes + 8 bytes per item pointer.

---

## 9. CPython Internals & Bytecode: The `LOAD_FAST` and `STORE_FAST` Opcodes
In CPython bytecode, variable assignments inside functions bypass dictionary lookups entirely:
- `STORE_FAST 0 (a)`: Stores the pointer directly into an internal C array offset `fastlocals[0]`.
- `LOAD_FAST 0 (a)`: Fetches the pointer with a single array index lookup ($O(1)$ memory fetch).

```python
import dis

def swap(a: int, b: int) -> tuple[int, int]:
    a, b = b, a
    return a, b

dis.dis(swap)
```
Disassembly:
```text
  1           0 RESUME                   0

  2           2 LOAD_FAST                1 (b)
              4 LOAD_FAST                0 (a)
              6 STORE_FAST               0 (a)
              8 STORE_FAST               1 (b)

  3          10 LOAD_FAST                0 (a)
             12 LOAD_FAST                1 (b)
             14 BUILD_TUPLE              2
             16 RETURN_VALUE
```

---

## 10. Best Practices
1. Default to immutable data structures (`tuple`, `frozenset`, frozen `dataclass`) to eliminate unwanted side effects.
2. Treat function arguments as borrowed references; never mutate input collections unless explicitly documented.
3. Compare singletons (`None`, `True`, `False`) with `is`; compare numbers and values with `==`.
4. Avoid shadowing Python built-in identifiers (`id`, `type`, `list`, `dict`, `str`, `int`).

---

## 11. Real-World Case Study: Diagnosing Shared State Bugs in Web Sessions
A SaaS backend service maintained a cache of user authorization permissions. When users logged out, their permissions dictionary was cleared via `perms.clear()`. However, customer support reported that active administrators were mysteriously losing their permissions:
1. Investigation revealed that the request handler assigned `session_perms = default_permissions` instead of creating an independent copy.
2. When any user logged out and called `session_perms.clear()`, it mutated the shared global `default_permissions` object in-place!
3. The fix replaced assignment with `default_permissions.copy()`, and frozen dataclasses were introduced to prevent future in-place mutation.

---

## 12. Interview Questions

### Q1: What does it mean that Python variables are "names, not boxes"?
<details>
<summary><strong>Answer</strong></summary>
In languages like C or Go, a variable represents a fixed memory address with a fixed type that holds binary data. In Python, objects reside on the heap and possess their own type and value. Variables are merely reference identifiers (names) in a namespace dictionary or local frame array that store pointers to those heap objects. Multiple names can point to the same object simultaneously.
</details>

### Q2: What is the difference between `is` and `==`?
<details>
<summary><strong>Answer</strong></summary>
`==` checks for value equality by invoking the object's `__eq__()` method, evaluating whether two objects represent the same data. `is` checks for object identity by comparing memory addresses (`id(a) == id(b)`), evaluating whether two identifiers reference the exact same object instance on the heap.
</details>

### Q3: What is the small integer cache in CPython?
<details>
<summary><strong>Answer</strong></summary>
During interpreter startup, CPython pre-allocates an array of small integer objects for values between `-5` and `256` inclusive. Whenever an integer in this range is created or returned by an operation, CPython returns a reference to the existing cached singleton rather than allocating a new object on the heap.
</details>

### Q4: Explain the behavior of `t = (1, [2, 3]); t[1] += [4]`.
<details>
<summary><strong>Answer</strong></summary>
This triggers a `TypeError: 'tuple' object does not support item assignment`, yet the list is mutated to `[2, 3, 4]`. The augmented assignment operator `+=` first calls `list.__iadd__`, which successfully mutates the list in-place. Next, it attempts to assign the returned list reference back into index 1 of the tuple. Because tuples are immutable, the assignment step raises `TypeError`.
</details>

### Q5: What is the difference between a shallow copy and a deep copy?
<details>
<summary><strong>Answer</strong></summary>
A shallow copy (`copy.copy()` or `list.copy()`) creates a new outer container object, but populates it with references to the exact same child objects contained in the original. A deep copy (`copy.deepcopy()`) recursively traverses and clones every nested container and object, resulting in a completely independent object graph.
</details>

### Q6: How does CPython manage memory for objects without reference cycles?
<details>
<summary><strong>Answer</strong></summary>
CPython uses reference counting as its primary memory management mechanism. Every object has an `ob_refcnt` field. Whenever a new reference is created (e.g. assignment, argument passing), the count increments. When a reference is deleted or goes out of scope, the count decrements. When `ob_refcnt` reaches zero, CPython immediately invokes the object's deallocator (`tp_dealloc`), releasing memory back to the mimalloc or pymalloc arena.
</details>

### Q7: Why is `[[0] * 3] * 3` dangerous when creating a 2D matrix?
<details>
<summary><strong>Answer</strong></summary>
The inner expression `[0] * 3` creates a single list `[0, 0, 0]`. The outer expression `* 3` creates a new outer list containing three references to that *same* inner list object. Modifying `matrix[0][0] = 1` updates the shared list, causing all three rows to display `[1, 0, 0]`.
</details>

### Q8: What is string interning and when does CPython perform it?
<details>
<summary><strong>Answer</strong></summary>
String interning ensures that only one copy of distinct immutable string objects is stored in an internal dictionary table. CPython automatically interns string literals that look like Python identifiers (alphanumeric plus underscores) at compile time. Interned strings allow $O(1)$ pointer comparison with `is` rather than $O(n)$ character-by-character equality checks.
</details>

---

## 13. Exercises & Challenges

### Challenge 1 (Easy): Identity vs Equality Inspector
Write a function `compare_objects(a, b)` that returns a dictionary reporting whether `a == b` and whether `a is b`.
<details>
<summary><strong>Solution</strong></summary>

```python
from typing import Any

def compare_objects(a: Any, b: Any) -> dict[str, bool]:
    return {
        "equal_values": a == b,
        "identical_instances": a is b,
    }

print("Comparing [1] and [1]:", compare_objects([1], [1]))
print("Comparing None and None:", compare_objects(None, None))
```
</details>

### Challenge 2 (Easy): Safe 2D Matrix Generator
Write a function `create_matrix(rows, cols, default_val=0)` that creates a true 2D matrix where each row is an independently allocated list.
<details>
<summary><strong>Solution</strong></summary>

```python
def create_matrix(rows: int, cols: int, default_val: int = 0) -> list[list[int]]:
    return [[default_val for _ in range(cols)] for _ in range(rows)]

m = create_matrix(3, 3)
m[0][0] = 99
print("Row 0:", m[0])
print("Row 1 (unaffected):", m[1])
```
</details>

### Challenge 3 (Medium): Finding Reference Count Changes
Write a function `track_refcount(obj)` that captures the exact reference count of an object before and after appending it to a local list.
<details>
<summary><strong>Solution</strong></summary>

```python
import sys

def track_refcount(obj: object) -> tuple[int, int]:
    initial = sys.getrefcount(obj)
    holder = [obj]
    after = sys.getrefcount(obj)
    del holder
    return initial, after

test_obj = object()
print("Refcounts (initial, with holder):", track_refcount(test_obj))
```
</details>

### Challenge 4 (Medium): Detecting Shared Nested Objects
Write a function `has_shared_references(nested_list)` that checks whether any two sublists in a list of lists reference the exact same underlying object in memory.
<details>
<summary><strong>Solution</strong></summary>

```python
from typing import Any

def has_shared_references(nested_list: list[list[Any]]) -> bool:
    seen_ids = set()
    for sublist in nested_list:
        sub_id = id(sublist)
        if sub_id in seen_ids:
            return True
        seen_ids.add(sub_id)
    return False

safe_list = [[1], [1]]
aliased_list = [[1]] * 2
print("Safe list has shared:", has_shared_references(safe_list))
print("Aliased list has shared:", has_shared_references(aliased_list))
```
</details>

### Challenge 5 (Hard): Detecting Reference Cycles
Write a script that creates a self-referencing object graph and uses the `gc` module to verify that a circular reference exists.
<details>
<summary><strong>Solution</strong></summary>

```python
import gc

class Node:
    def __init__(self, name: str) -> None:
        self.name = name
        self.neighbor: Node | None = None

def verify_cycle_creation() -> bool:
    n1 = Node("A")
    n2 = Node("B")
    n1.neighbor = n2
    n2.neighbor = n1  # Circular reference created

    # Both objects are now tracked by cyclic GC
    return gc.is_tracked(n1) and gc.is_tracked(n2)

print("Cycle objects tracked by GC:", verify_cycle_creation())
```
</details>

### Challenge 6 (Hard): Custom Weak Reference Cache
Implement a cache using `weakref.ref` that stores objects without preventing their garbage collection when all external strong references are deleted.
<details>
<summary><strong>Solution</strong></summary>

```python
from typing import Any
import weakref

class WeakCache:
    def __init__(self) -> None:
        self._cache: dict[str, weakref.ref[Any]] = {}

    def set(self, key: str, value: Any) -> None:
        self._cache[key] = weakref.ref(value)

    def get(self, key: str) -> Any | None:
        ref = self._cache.get(key)
        if ref is not None:
            return ref()
        return None

class Resource:
    def __init__(self, name: str) -> None:
        self.name = name

cache = WeakCache()
res = Resource("DatabaseConn")
cache.set("conn", res)
print("Cached while alive:", cache.get("conn").name)
del res
print("Cached after deletion:", cache.get("conn"))
```
</details>
