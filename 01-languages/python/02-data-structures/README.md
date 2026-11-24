# Module 02: Data Structures

## Overview

Module 02 provides comprehensive, engineering-grade coverage of Python's built-in and standard library data structures. Rather than treating collections as simple arrays and associative maps, this module inspects the internal C structures, memory models, algorithmic complexities, and design trade-offs of every container in the Python language.

From the amortized $O(1)$ mechanics of `PyListObject` and compact hash table architecture of `PyDictObject`, to specialized containers like `collections.deque`, `heapq`, and typed unboxed `array.array`, this module equips engineers with the deep knowledge needed to build high-throughput, memory-conscious systems.

---

## Complete Module Syllabus

| # | Note Title | Slug | Key Topics Covered | Status |
| :-: | :--- | :--- | :--- | :-: |
| 00 | **Lists in Python** | `00-lists.md` | Pointer array layout, mutability, indexing, $O(1)$ amortized append, in-place mutations, sorting, common aliasing pitfalls | Verified |
| 01 | **Tuples and NamedTuples** | `01-tuples-and-namedtuples.md` | Immutability, singleton comma rule, hashability requirements, memory efficiency, `typing.NamedTuple`, `_replace()` | Verified |
| 02 | **Dictionaries in Python** | `02-dictionaries.md` | Compact hash table layout, insertion order preservation, view objects (`keys`, `items`), PEP 584 merge operators (`\|`, `\|=`) | Verified |
| 03 | **Sets and Frozensets** | `03-sets-and-frozensets.md` | Mathematical set theory operators vs methods, uniqueness, `frozenset` as dict keys, dummy deletion tombstones | Verified |
| 04 | **Slicing and Indexing** | `04-slicing-and-indexing.md` | Half-open intervals `[start:stop:step]`, sequence reversal `[::-1]`, `slice` objects, `indices()`, in-place slice mutation | Verified |
| 05 | **Unpacking (Starred Expressions)** | `05-unpacking.md` | Extended iterable unpacking (`*rest`), PEP 3132 rules, nested unpacking, `*args` and `**kwargs`, PEP 448 dictionary merges | Verified |
| 06 | **Sorting and Keys** | `06-sorting-and-keys.md` | `list.sort()` vs `sorted()`, key functions (`key=f`), Timsort & Powersort algorithms, stability, `operator.attrgetter` | Verified |
| 07 | **Comprehensions** | `07-comprehensions.md` | List, set, dict, and generator expressions, filtering vs ternary branching, isolated scope, bytecode speedups | Verified |
| 08 | **Copying: Shallow vs Deep** | `08-copying-shallow-vs-deep.md` | Reference aliasing (`b = a`), shallow copy (`copy()`), `copy.deepcopy()`, memo dictionary for cycles, `__deepcopy__` hooks | Verified |
| 09 | **The collections Module** | `09-collections-module.md` | `deque` ring buffers, `Counter` multiset arithmetic, `defaultdict` missing factories, `OrderedDict`, `ChainMap` scoping | Verified |
| 10 | **heapq, bisect, and array** | `10-heapq-bisect-and-array.md` | Min-heaps, $O(n)$ `heapify`, priority queues, $O(\log n)$ binary search with `bisect`, unboxed typed C buffers with `array` | Verified |
| 11 | **Choosing the Right Data Structure** | `11-choosing-the-right-structure.md` | Comprehensive Big-O complexity matrix, memory consumption audit, decision flowchart, architectural trade-offs | Verified |
| 12 | **Dict and Set Internals** | `12-dict-and-set-internals.md` | CPython compact layout, perturbation probing, 2/3 load factor, SipHash-2-4 (PEP 456), PEP 412 key-sharing dictionaries | Verified |
| 13 | **List Internals and Dynamic Growth** | `13-list-internals-and-growth.md` | `PyListObject` struct, geometric over-allocation formula, amortized $O(1)$ proof, buffer shrinkage, header free list | Verified |

---

## Learning Outcomes

After completing Module 02, you will be able to:
1. Explain the low-level memory difference between contiguous C value arrays and Python's pointer arrays.
2. Select the optimal container (`list`, `deque`, `set`, `dict`, `heapq`, `array`) based on algorithmic complexity and memory density.
3. Utilize modern Python features such as dictionary merge operators (`|`), extended unpacking (`*rest`), and typed `NamedTuple` records.
4. Architect high-speed priority queues and ring buffers using `heapq` and `collections.deque` without third-party dependencies.
5. Explain CPython's compact hash table architecture, SipHash security against HashDoS attacks, and key-sharing instance optimization.
6. Mathematically prove why CPython list appending is amortized $O(1)$ and predict exact buffer resize thresholds.

---

## Prerequisites & Cross-Links

- **Prerequisites:** [Module 01: Fundamentals & Syntax](../01-fundamentals/README.md) for object pointers, mutability, and numeric types.
- **Next Up:** [Module 03: Functions & Callables](../03-functions/README.md) for arguments, closures, decorators, and functional abstractions.
- **Downstream Foundations:**
  - FastAPI Track (`03-backend/fastapi`): Builds directly on dictionaries, NamedTuples, and unpacking.
  - Machine Learning Track (`05-ai/machine-learning`): Builds on typed unboxed arrays, slicing, and memory layouts.
