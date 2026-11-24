---
title: "Copying: Shallow vs Deep Copies in Python"
slug: "copying-shallow-vs-deep"
category: "data-structures"
difficulty: "intermediate"
tags: ["python", "data-structures", "copy", "shallow-copy", "deep-copy", "memory", "cyclic-references"]
prerequisites: ["01-fundamentals/01-variables-names-and-objects", "00-lists", "02-dictionaries"]
related_topics: ["05-oop/01-init-new-and-instance-lifecycle", "13-python-internals"]
python_version: "3.8+"
tested_with: "CPython 3.14.0"
free_threading_support: "Yes"
---

# Copying: Shallow vs Deep Copies in Python

## 1. Executive Summary & Core Value Proposition

In Python, assignment statements (`b = a`) never copy data; they merely bind a new identifier to an existing heap object. When working with mutable compound objects (such as nested lists, dictionaries, or custom class instances), developers must explicitly copy structures to prevent unintended mutation side effects across components.

Python provides two distinct copying paradigms via the built-in `copy` module:
1. **Shallow Copy (`copy.copy()`, `list.copy()`, `lst[:]`):** Constructs a brand new outer container, but populates it with **references** (pointers) to the original child elements.
2. **Deep Copy (`copy.deepcopy()`):** Constructs a brand new outer container, and then **recursively duplicates** every child object, grandchild object, and nested reference, ensuring total structural isolation.

Understanding the boundary between shallow and deep copying is critical for avoiding subtle state corruption, data race conditions in concurrent programs, and catastrophic performance penalties when duplicating large object graphs.

---

## 2. Syntax, Mechanics & Visual Architecture

### The Three Tiers of Assignment and Duplication

```python
import copy

# 1. Reference Binding (Aliasing)
b = a                  # Zero copying. 'b' and 'a' point to the identical heap address.

# 2. Shallow Copy
s = copy.copy(a)       # New outer container; shared inner references.
s = a.copy()           # Built-in method for dict, list, set.
s = a[:]               # Slice idiom for list, tuple.

# 3. Deep Copy
d = copy.deepcopy(a)   # Full recursive clone of entire object graph.
```

### Visual Memory Layout Comparison

```
Original Object Graph:
List A: [ ptr0, ptr1 ]
           |      |
           v      v
        [1, 2]  [3, 4]  (Nested Lists)

Case 1: Shallow Copy (copy.copy(A))
List B: [ ptr0, ptr1 ]  <-- NEW outer list object, BUT points to SAME nested lists!
           |      |
           +------+---> Points to original [1, 2] and [3, 4]
           
           Result: B[0][0] = 99  --> A[0][0] ALSO BECOMES 99!

Case 2: Deep Copy (copy.deepcopy(A))
List C: [ ptrX, ptrY ]  <-- NEW outer list object
           |      |
           v      v
        [1, 2]  [3, 4]  <-- BRAND NEW inner list objects!
        
           Result: C[0][0] = 99  --> A[0][0] REMAINS 1!
```

### The Memo Dictionary in `deepcopy`

To prevent infinite loops when copying cyclic data structures (e.g., an object containing a reference to itself), `copy.deepcopy()` maintains an internal `memo` dictionary (`memo = {}`) tracking all previously copied objects:

$$\text{memo}[\text{id(orig\_obj)}] = \text{new\_cloned\_obj}$$

If an object has already been copied in the current traversal, `deepcopy` returns the cached clone immediately.

---

## 3. Real-World Code Examples

### Example 1: Reference vs Shallow vs Deep Copy Mutations

```python
import copy

# Nested configuration payload
original = {
    "app": "gateway",
    "ports": [80, 443],
    "features": {"ssl": True, "auth": "jwt"}
}

# 1. Reference alias
alias = original

# 2. Shallow copy
shallow = original.copy()

# 3. Deep copy
deep = copy.deepcopy(original)

# Mutate nested structure
original["ports"].append(8080)
original["features"]["auth"] = "oauth2"
original["app"] = "gateway-v2"

print("--- After Mutations ---")
print("Original :", original)
print("Alias    :", alias)       # Mirrors ALL changes
print("Shallow  :", shallow)     # Outer 'app' unchanged, inner 'ports' & 'features' MUTATED!
print("Deep     :", deep)        # Completely UNCHANGED and isolated!
```

**Output:**
```
--- After Mutations ---
Original : {'app': 'gateway-v2', 'ports': [80, 443, 8080], 'features': {'ssl': True, 'auth': 'oauth2'}}
Alias    : {'app': 'gateway-v2', 'ports': [80, 443, 8080], 'features': {'ssl': True, 'auth': 'oauth2'}}
Shallow  : {'app': 'gateway', 'ports': [80, 443, 8080], 'features': {'ssl': True, 'auth': 'oauth2'}}
Deep     : {'app': 'gateway', 'ports': [80, 443], 'features': {'ssl': True, 'auth': 'jwt'}}
```

---

### Example 2: Handling Cyclic Graphs Safely with `deepcopy`

```python
import copy

class Node:
    def __init__(self, name: str):
        self.name = name
        self.neighbors: list["Node"] = []
    
    def add_edge(self, neighbor: "Node"):
        self.neighbors.append(neighbor)

# Create cyclic graph: A -> B -> A
node_a = Node("NodeA")
node_b = Node("NodeB")
node_a.add_edge(node_b)
node_b.add_edge(node_a)

# deepcopy handles cycles without recursion limit crashes
cloned_a = copy.deepcopy(node_a)

print(f"Original A id: {id(node_a)}, Cloned A id: {id(cloned_a)}")
print(f"Cloned B points back to Cloned A? {cloned_a.neighbors[0].neighbors[0] is cloned_a}")
```

**Output:**
```
Original A id: 2184594248464, Cloned A id: 2184594248224
Cloned B points back to Cloned A? True
```

---

### Example 3: Custom Copying Protocol (`__copy__` and `__deepcopy__`)

```python
import copy

class DatabaseSession:
    def __init__(self, session_id: str, connection_string: str, query_cache: dict):
        self.session_id = session_id
        self.connection_string = connection_string
        self.query_cache = query_cache
        self.connection_open = True
    
    def __copy__(self):
        """Custom shallow copy: shares connection and cache."""
        return DatabaseSession(f"{self.session_id}-copy", self.connection_string, self.query_cache)
    
    def __deepcopy__(self, memo):
        """Custom deep copy: clones cache, but establishes independent session."""
        new_session = DatabaseSession(
            f"{self.session_id}-deep-fork",
            self.connection_string,
            copy.deepcopy(self.query_cache, memo)
        )
        memo[id(self)] = new_session
        return new_session

s1 = DatabaseSession("sess-01", "pg://localhost:5432", {"SELECT 1": "OK"})
s1_shallow = copy.copy(s1)
s1_deep = copy.deepcopy(s1)

print("Original session id :", s1.session_id)
print("Shallow copy session:", s1_shallow.session_id)
print("Deep copy session   :", s1_deep.session_id)
```

**Output:**
```
Original session id : sess-01
Shallow copy session: sess-01-copy
Deep copy session   : sess-01-deep-fork
```

---

### Example 4: Shallow Copy Performance vs Deep Copy Overhead

```python
import time
import copy

# Complex nested structure: 10,000 records
records = [{"id": i, "tags": ["python", "data"], "meta": {"score": i}} for i in range(10_000)]

# Benchmark shallow copy
t0 = time.perf_counter()
shallow_copy = [r.copy() for r in records]
t_shallow = time.perf_counter() - t0

# Benchmark deep copy
t0 = time.perf_counter()
deep_copy = copy.deepcopy(records)
t_deep = time.perf_counter() - t0

print(f"Shallow copy time: {t_shallow*1000:.2f} ms")
print(f"Deep copy time   : {t_deep*1000:.2f} ms (Slower by {t_deep/t_shallow:.1f}x)")
```

**Output:**
```
Shallow copy time: 2.15 ms
Deep copy time   : 48.62 ms (Slower by 22.6x)
```

---

### Example 5: Copying Immutable Objects (CPython Identity Optimizations)

CPython optimizes `copy.copy()` and `copy.deepcopy()` for immutable types: if an object is immutable (e.g. `int`, `str`, `tuple` containing only immutables), Python does not allocate a copy; it simply returns the original object reference!

```python
import copy

text = "Immutable string content"
text_copy = copy.copy(text)
text_deep = copy.deepcopy(text)

print("String copy is same object?", text_copy is text)
print("String deep is same object?", text_deep is text)

# Tuple containing immutable elements
tup = (1, 2, 3)
print("Immutable tuple is same object?", copy.deepcopy(tup) is tup)

# Tuple containing MUTABLE elements
tup_mutable = (1, [2, 3])
tup_deep = copy.deepcopy(tup_mutable)
print("Tuple with list is same object?", tup_deep is tup_mutable)
print("Nested list identical?", tup_deep[1] is tup_mutable[1])
```

**Output:**
```
String copy is same object? True
String deep is same object? True
Immutable tuple is same object? True
Tuple with list is same object? False
Nested list identical? False
```

---

## 4. Common Anti-Patterns & Mistakes

| Anti-Pattern / Mistake | Broken / Unidiomatic Snippet | Idiomatic Correction | Why It Fails |
| :--- | :--- | :--- | :--- |
| **Assuming shallow copy isolates nested state** | `b = a.copy(); b[0].append(1)` | `b = copy.deepcopy(a)` | Inner objects remain shared; modifying child mutates original. |
| **Calling `deepcopy` on large datasets** | `copy.deepcopy(giant_dataset)` | Shallow copy or slice processing | `deepcopy` traverses entire object graph with memo lookups, causing extreme CPU overhead. |
| **Deep-copying system resources** | `copy.deepcopy(open_file_obj)` | Pass path, reopen file | File descriptors, sockets, and thread locks cannot be serialized or duplicated. |
| **Using `b = a` expecting a copy** | `new_state = state; new_state.clear()` | `new_state = state.copy()` | `b = a` is pointer aliasing; clearing `new_state` clears `state`. |
| **Neglecting `memo` in custom `__deepcopy__`** | `__deepcopy__(self, memo): ...` *(omits memo)* | Always pass `memo` to nested calls | Breaks cyclic reference detection, leading to infinite recursion `RecursionError`. |
| **Using JSON serialization as generic deepcopy** | `json.loads(json.dumps(obj))` | `copy.deepcopy(obj)` | Drops non-JSON types (tuples become lists, sets fail, datetimes turn into strings). |

---

## 5. Architectural & Design Decisions

### Why Doesn't Python Deep-Copy by Default?
In C++, value semantics are default: assigning or passing a struct by value copies all bytes. In Python, reference semantics are uniform: everything is a pointer. Defaulting to deep copying would make everyday operations like `items.append(x)` or `func(data)` prohibitively expensive ($O(N)$ graph traversals on every invocation). Uniform reference semantics ensure predictable $O(1)$ pointer passing.

### The Pickle Protocol Connection
`copy.deepcopy()` is architecturally modeled on Python's `pickle` serialization protocol. It inspects `__getstate__`, `__setstate__`, `__reduce__`, or `__dict__` to reconstruct arbitrary objects. Objects that cannot be pickled (such as live database connections, open sockets, generator objects, and C extensions holding raw pointers) typically fail under `deepcopy`.

---

## 6. Verification & Practical Testing

```python
import copy

def test_shallow_vs_deep():
    original = [[1, 2], [3, 4]]
    shallow = copy.copy(original)
    deep = copy.deepcopy(original)
    
    # Mutate top level
    shallow.append([5, 6])
    assert len(original) == 2  # Original top-level unchanged
    
    # Mutate nested child
    original[0][0] = 99
    assert shallow[0][0] == 99  # Shallow saw nested change
    assert deep[0][0] == 1      # Deep was completely isolated

def test_cyclic_deepcopy():
    class Cyclic:
        def __init__(self):
            self.self_ref = self
    c = Cyclic()
    c_clone = copy.deepcopy(c)
    assert c_clone is not c
    assert c_clone.self_ref is c_clone

if __name__ == "__main__":
    test_shallow_vs_deep()
    test_cyclic_deepcopy()
    print("All shallow and deep copy tests passed successfully.")
```

**Output:**
```
All shallow and deep copy tests passed successfully.
```

---

## 7. Deep Dive: The `memo` Dictionary Implementation

When `deepcopy` executes, it internally uses a dictionary mapping `id(obj)` to the cloned instance:

```python
def deepcopy(x, memo=None, _nil=[]):
    if memo is None:
        memo = {}
    d = id(x)
    y = memo.get(d, _nil)
    if y is not _nil:
        return y
    ...
```

When writing custom `__deepcopy__(self, memo)` methods:
1. First, register `memo[id(self)] = new_instance` **before** copying child attributes.
2. Recursively call `copy.deepcopy(child, memo)`.
3. If a circular reference points back to `self`, step 1 ensures it returns `new_instance` immediately, resolving the cycle.

---

## 8. Summary & Key Takeaways

1. **Aliasing (`b = a`):** New name pointing to the exact same memory address.
2. **Shallow Copy (`copy.copy(a)`):** New outer container; all inner elements remain shared references.
3. **Deep Copy (`copy.deepcopy(a)`):** Fully independent recursive duplicate of the entire object graph.
4. **Cycle Safety:** `deepcopy` tracks object IDs in a `memo` dictionary to handle cyclic references safely.
5. **Immutable Optimization:** CPython skips copying immutable objects (`int`, `str`, immutable `tuple`), returning the original reference.

---

## 9. Frequently Asked Questions (Interview & Senior Roles)

<details>
<summary>1. Why does <code>copy.copy((1, 2, 3))</code> return the exact same object reference?</summary>

Because tuples containing only immutable items cannot be mutated. Creating a new memory allocation for an immutable object with identical contents is wasteful. CPython returns the existing object directly, satisfying the contract with zero memory allocation.
</details>

<details>
<summary>2. How does <code>copy.deepcopy()</code> prevent infinite recursion on self-referential objects?</summary>

It accepts a `memo` dictionary that maps `id(original_object)` to `new_copied_object`. Whenever `deepcopy` encounters an object whose ID is already in `memo`, it returns the previously created copy rather than recursing again.
</details>

<details>
<summary>3. What is the performance danger of using <code>copy.deepcopy()</code> indiscriminately?</summary>

`deepcopy` traverses every branch of the object graph, evaluating attributes, inspecting dictionaries, creating new objects, and recording entries in `memo`. On large data structures (e.g., ASTs, large matrices, complex ORM models), this can cause severe CPU bottlenecks and multi-megabyte memory churn.
</details>

<details>
<summary>4. Why does <code>json.loads(json.dumps(data))</code> fail as a generic deepcopy substitute?</summary>

JSON serialization does not support arbitrary Python types. Sets are rejected, tuples are converted to lists, keys in dictionaries must be strings, custom class instances fail unless custom encoders are provided, and cyclic references raise `ValueError: Circular reference detected`.
</details>

<details>
<summary>5. How can a custom class control how it is copied?</summary>

By implementing:
- `__copy__(self)`: for shallow copying.
- `__deepcopy__(self, memo)`: for deep copying, ensuring that `memo` is passed down to any recursive `copy.deepcopy(attr, memo)` calls.
</details>

<details>
<summary>6. Does shallow copying a list duplicate its elements?</summary>

No. It duplicates the contiguous array of **pointers** (`PyObject*`), but each pointer inside the new list points to the exact same object in heap memory as the original list.
</details>

<details>
<summary>7. What happens if you try to <code>deepcopy</code> an object containing an open network socket or file?</summary>

It raises an exception (typically `TypeError: cannot pickle '_io.BufferedReader' object` or similar). Operating system resources tied to kernel file descriptors cannot be cloned by Python's userspace memory allocator.
</details>

<details>
<summary>8. How does free-threaded Python (PEP 703) impact <code>deepcopy</code> operations?</summary>

Because `deepcopy` traverses arbitrary object graphs, traversing shared state that is concurrently being modified by another thread in a no-GIL build can lead to race conditions. Developers must ensure the source object graph is either thread-confined or locked during deep copy operations.
</details>

---

## 10. Practical Exercises & Self-Assessment

<details>
<summary>Exercise 1: Diagnostic Mutation Audit</summary>

**Task:** Predict what the following code prints:
```python
a = [[10], [20]]
b = a[:]
b[0].append(99)
b.append([30])
print(a)
```

**Solution:**
`a` prints `[[10, 99], [20]]`.
- `b = a[:]` is a shallow copy. `b` has its own outer list, but its elements point to the same inner lists `[10]` and `[20]`.
- `b[0].append(99)` mutates the shared inner list `[10]`, affecting `a`.
- `b.append([30])` mutates `b`'s outer list only, leaving `len(a)` unchanged at 2.
</details>

<details>
<summary>Exercise 2: Write a Safe Dictionary Clone Utility</summary>

**Task:** Write a function `clone_dict_one_level(d: dict)` that shallow copies the dictionary and shallow copies all values that are lists or dictionaries.

**Solution:**
```python
def clone_dict_one_level(d: dict) -> dict:
    result = {}
    for k, v in d.items():
        if isinstance(v, list):
            result[k] = v.copy()
        elif isinstance(v, dict):
            result[k] = v.copy()
        else:
            result[k] = v
    return result

data = {"scores": [1, 2], "meta": {"env": "prod"}}
cloned = clone_dict_one_level(data)
cloned["scores"].append(3)
assert len(data["scores"]) == 2  # Original isolated!
```
</details>

<details>
<summary>Exercise 3: Implement Deep Copy for Binary Trees</summary>

**Task:** Implement a `TreeNode` class with `val`, `left`, `right`, and a custom `clone()` method that performs recursive deep copying without using the `copy` module.

**Solution:**
```python
class TreeNode:
    def __init__(self, val, left=None, right=None):
        self.val = val
        self.left = left
        self.right = right

    def clone(self) -> "TreeNode":
        return TreeNode(
            self.val,
            self.left.clone() if self.left else None,
            self.right.clone() if self.right else None
        )

root = TreeNode(1, TreeNode(2), TreeNode(3))
cloned_root = root.clone()
assert cloned_root.left is not root.left
assert cloned_root.left.val == 2
```
</details>

<details>
<summary>Exercise 4: Circular Linked List Deep Copy</summary>

**Task:** Create a circular linked list of 3 nodes and clone it using `copy.deepcopy()`. Verify that the circular reference is preserved in the clone without referencing the original nodes.

**Solution:**
```python
import copy

class ListNode:
    def __init__(self, val):
        self.val = val
        self.next = None

n1 = ListNode(1)
n2 = ListNode(2)
n3 = ListNode(3)
n1.next = n2; n2.next = n3; n3.next = n1  # Circular loop

clone = copy.deepcopy(n1)
assert clone is not n1
assert clone.next is not n2
assert clone.next.next.next is clone  # Circle preserved!
```
</details>

<details>
<summary>Exercise 5: Benchmark Shallow vs JSON vs Deepcopy</summary>

**Task:** Write a benchmark comparing `list.copy()`, `json.loads(json.dumps())`, and `copy.deepcopy()` for a nested dictionary.

**Solution:**
```python
import copy
import json
import time

payload = {"id": 1, "items": list(range(100)), "config": {"active": True}}

# copy.deepcopy
t0 = time.perf_counter()
for _ in range(10_000):
    _ = copy.deepcopy(payload)
t_deep = time.perf_counter() - t0

# JSON serialization
t0 = time.perf_counter()
for _ in range(10_000):
    _ = json.loads(json.dumps(payload))
t_json = time.perf_counter() - t0

print(f"Deepcopy time: {t_deep*1000:.2f} ms")
print(f"JSON copy time: {t_json*1000:.2f} ms")
```
</details>

<details>
<summary>Exercise 6: Custom Copy Hook with Transient Attribute Stripping</summary>

**Task:** Implement a class `SecureSession` where `__copy__` and `__deepcopy__` strip out an attribute `auth_token` for security purposes.

**Solution:**
```python
import copy

class SecureSession:
    def __init__(self, username: str, auth_token: str):
        self.username = username
        self.auth_token = auth_token

    def __deepcopy__(self, memo):
        # Omit auth_token during cloning
        return SecureSession(self.username, auth_token="STRIPPED")

session = SecureSession("shiva", "SECRET_TOKEN_999")
cloned_session = copy.deepcopy(session)

print(f"Original token: {session.auth_token}")
print(f"Cloned token  : {cloned_session.auth_token}")
assert cloned_session.auth_token == "STRIPPED"
```
</details>
