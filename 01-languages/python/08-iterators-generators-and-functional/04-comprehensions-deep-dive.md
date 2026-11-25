# Module 08: Iterators, Generators & Functional Programming (04-comprehensions-deep-dive)

> **Topic Overview**: Comprehensions are Python's most distinctive and expressive syntax for collection construction. Spanning lists, sets, dictionaries, and generator expressions, they combine mapping and filtering into optimized bytecode sequences (`LIST_APPEND`, `MAP_ADD`, `SET_ADD`) that out-perform manual `append()` loops. Understanding nested loops, scope isolation, and the walrus operator (`:=`) is essential for writing clean, fast Python.

---

## Metadata Table

| Field | Value |
|---|---|
| **Category** | Core Language / Functional Syntax |
| **Target Level** | Intermediate |
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

- **Problem Statement**: Constructing transformed or filtered collections using traditional `for` loops requires declaring an empty container, repeatedly looking up the `.append()` or `.add()` attribute on every iteration, and managing multi-line boilerplate that distracts from the core data transformation.
- **Mental Model**: A comprehension is a declarative mathematical set-builder notation: $\{ f(x) \mid x \in S \text{ and } P(x) \}$. In Python, this translates to: `[expr for item in iterable if condition]`.
- **When to Use**:
  - Transforming or filtering sequences into new lists, sets, or dictionaries.
  - Creating key-value mappings from pairs or objects.
  - Flattening 2D data structures.
- **When NOT to Use**: Do not use comprehensions solely for their side effects (e.g., `[print(x) for x in items]`). If the transformation requires complex exception handling, nested branching, or spans more than two lines, refactor into an explicit `for` loop.

---

## 2. Core Concepts

### Comprehension Types & Target Containers

```text
+-------------------+------------------------------------+--------------------------+
| Comprehension     | Syntax                             | Resulting Type           |
+-------------------+------------------------------------+--------------------------+
| List              | [x * 2 for x in seq]               | list                     |
| Set               | {x * 2 for x in seq}               | set (deduplicated)       |
| Dictionary        | {k: v for k, v in pairs}           | dict                     |
| Generator Expr    | (x * 2 for x in seq)               | generator (lazy cursor)  |
+-------------------+------------------------------------+--------------------------+
```

### Scope Isolation in Python 3
In Python 2, comprehension loop target variables leaked into the enclosing local function namespace. In Python 3, all comprehensions execute within their own dedicated, isolated function scope. The iteration variable (e.g., `x` in `[x for x in data]`) **never leaks** into the enclosing scope, preventing variable shadowing bugs.

### The Walrus Operator (`:=`) Exception (PEP 572)
While standard loop variables remain scoped to the comprehension, variables bound using the assignment expression (`:=`) **leak intentionally into the enclosing function scope**:

```python
# 'val' is accessible outside the comprehension!
filtered = [val for x in data if (val := expensive(x)) > 10]
print(val)  # Holds the last assigned value
```

---

## 3. How It Works Internally

CPython compiles comprehensions into a tight C-level loop that uses direct stack-push instructions instead of invoking Python-level method lookup:

```text
Manual Loop:
  1. LOAD_FAST list_obj
  2. LOAD_METHOD 'append'     <--- Attribute lookup on EVERY iteration!
  3. LOAD_FAST item
  4. CALL_METHOD 1
  5. POP_TOP

Comprehension:
  1. LOAD_FAST item
  2. LIST_APPEND 1            <--- Direct C-level append, bypassing attribute lookup!
```

---

## 4. Syntax & API Reference

### Nested Loop Ordering Rule
The order of `for` and `if` clauses in a comprehension **matches the exact order of nested statements in an imperative loop**:

```python
# Nested for loops:
# for row in matrix:
#     for x in row:
#         if x > 0:
#             yield x

# Comprehension:
flattened = [x for row in matrix for x in row if x > 0]
```

### Dictionary Comprehensions
```python
# Syntax: {key_expr: val_expr for item in iterable if condition}
id_map = {user.id: user.name for user in users if user.is_active}
```

---

## 5. Code Examples

### 1. Minimal / Basic Example: The Four Comprehension Forms
```python
source = [1, 2, 2, 3, 4, 4, 5]

# 1. List comprehension
c_list = [x * 10 for x in source if x % 2 != 0]

# 2. Set comprehension (automatic deduplication)
c_set = {x * 10 for x in source if x % 2 != 0}

# 3. Dict comprehension
c_dict = {f"k_{x}": x ** 2 for x in source}

# 4. Generator expression (lazy)
c_gen = (x * 10 for x in source if x % 2 != 0)

print("List:", c_list)
print("Set: ", sorted(c_set))
print("Dict:", c_dict)
print("Gen: ", list(c_gen))
```
**Output**:
```text
List: [10, 30, 50]
Set:  [10, 30, 50]
Dict: {'k_1': 1, 'k_2': 4, 'k_3': 9, 'k_4': 16, 'k_5': 25}
Gen:  [10, 30, 50]
```

### 2. Real-World Production Example: Inverting Mappings and Filtering Payloads
```python
# Inverting an authorization role-to-users mapping
role_assignments = {
    "admin": ["alice", "bob"],
    "editor": ["charlie"],
    "viewer": ["dave", "alice"]
}

# Comprehension flattening and inverting: user -> list of roles
user_roles: dict[str, list[str]] = {}
for role, users in role_assignments.items():
    for user in users:
        user_roles.setdefault(user, []).append(role)

# Using dict comprehension to create user -> primary role uppercase mapping
primary_role_map = {
    user: roles[0].upper()
    for user, roles in user_roles.items()
    if len(roles) > 0
}

print("Inverted User Roles:", user_roles)
print("Primary Role Map:   ", primary_role_map)
```
**Output**:
```text
Inverted User Roles: {'alice': ['admin', 'viewer'], 'bob': ['admin'], 'charlie': ['editor'], 'dave': ['viewer']}
Primary Role Map:    {'alice': 'ADMIN', 'bob': 'ADMIN', 'charlie': 'EDITOR', 'dave': 'VIEWER'}
```

### 3. Edge Case: Scope Isolation Verification
```python
x = "GLOBAL_X"

# Comprehension iteration variable 'x'
result = [x * 2 for x in [1, 2, 3]]

print("Comprehension result:", result)
print("Global 'x' preserved:", x)  # Still "GLOBAL_X", never overwritten!
```
**Output**:
```text
Comprehension result: [2, 4, 6]
Global 'x' preserved: GLOBAL_X
```

### 4. Failure Case & Exception Handling: Unpacking Mismatch in Comprehension
```python
pairs = [(1, "A"), (2, "B"), (3,)]  # Third item has only 1 element!

try:
    bad_dict = {k: v for k, v in pairs}  # type: ignore[misc]
except ValueError as err:
    print(f"Captured expected unpacking error: {err}")
```
**Output**:
```text
Captured expected unpacking error: not enough values to unpack (expected 2, got 1)
```

### 5. Idiomatic Pythonic Pattern: Avoiding Duplicate Computations with Walrus (`:=`)
```python
def compute_score(text: str) -> int:
    # Simulate non-trivial computation
    return sum(ord(c) for c in text)

words = ["python", "go", "rust", "typescript", "c"]

# Without walrus: compute_score(w) would be called TWICE (in if and in expr)
# With walrus: computed ONCE, stored in 'score', filtered, and returned!
high_scores = [
    f"{w}: {score}"
    for w in words
    if (score := compute_score(w)) > 450
]

print("High scores:", high_scores)
print("Last bound 'score' leaked to scope:", score)
```
**Output**:
```text
High scores: ['python: 674', 'rust: 462', 'typescript: 1109']
Last bound 'score' leaked to scope: 99
```

---

## 6. Edge Cases & Pitfalls

1. **Nested Comprehension Reading Direction**:
   - `[x for row in matrix for x in row]` flattens rows into items. Writing `[x for x in row for row in matrix]` fails with `NameError: name 'row' is not defined`. Read left-to-right as nested loops.
2. **Conditional Expression vs Filter Clause**:
   - `[x if x > 0 else 0 for x in data]`: Ternary operator transforms every element (list retains original length).
   - `[x for x in data if x > 0]`: Filter clause selects only matching elements (list shrinks).
3. **Set Comprehension Deduplication**:
   - `{x.lower() for x in ["A", "a", "B"]}` results in `{'a', 'b'}`. Length of resulting set may be shorter than the input list.

---

## 7. Common Mistakes

| Mistake | Why It Fails | Exception / Diagnostic | Correct Pattern |
|---|---|---|---|
| Using comprehension solely for side effects | `[print(x) for x in seq]` builds a useless list of `[None, None, ...]` in memory | Wasted memory / unidiomatic | Use standard `for x in seq: print(x)` |
| Confusing ternary operator with filter clause | Writing `if condition` before `for` without `else` | `SyntaxError: invalid syntax` | Use `[x if c else y for x in s]` |
| Reversing nested loop order | `[x for x in row for row in m]` attempts to read `row` before defining it | `NameError: name 'row' is not defined` | `[x for row in m for x in row]` |
| Assuming `:=` variable remains isolated | Walrus assignments leak intentionally into outer function scope | Overwritten outer local variable | Choose distinct variable name for walrus |
| Over-nesting (3+ levels of loops/conditions) | Impenetrable code that is impossible to debug or inspect | Readability violation (PEP 8) | Refactor into multi-line generator function |
| Modifying the collection being iterated | Modifying a list during comprehension iteration causes missed elements | Undefined traversal / skip bugs | Iterate over a copy or slice |

---

## 8. Performance & Complexity

- **Time Complexity**: $O(N)$ for single-level comprehensions; $O(R \times C)$ for 2D matrix flattening.
- **Bytecode Optimization**: Because CPython uses `LIST_APPEND` (an internal opcode that appends directly to the C-level array without method resolution), list comprehensions run $\approx 25-35\%$ faster than manual `for` loops calling `list.append()`.
- **Memory Overhead**: List comprehensions allocate the list buffer upfront and resize dynamically using CPython's list growth formula ($~1.125\times$).

---

## 9. CPython Internals & Bytecode

Let's compare the bytecode of a list comprehension vs a manual `for` loop:

```python
import dis

def comp_version(data):
    return [x * 2 for x in data if x > 0]

dis.dis(comp_version)
```

**Bytecode output**:
```text
  1           0 RESUME                   0

  2           2 LOAD_CONST               1 (<code object <listcomp> at ...>)
              4 MAKE_FUNCTION            0
             12 LOAD_FAST                0 (data)
             14 GET_ITER
             16 CALL                     1
             24 RETURN_VALUE

Disassembly of <code object <listcomp>>:
  2           0 RESUME                   0
              2 BUILD_LIST               0
              4 LOAD_FAST                0 (.0)
        >>    6 FOR_ITER                 9 (to 26)
              8 STORE_FAST               1 (x)
             10 LOAD_FAST                1 (x)
             12 LOAD_CONST               0 (0)
             14 COMPARE_OP              68 (>)
             18 POP_JUMP_IF_FALSE        3 (to 24)
             20 LOAD_FAST                1 (x)
             22 LOAD_CONST               1 (2)
             24 BINARY_OP                5 (*)
             28 LIST_APPEND              2
             30 JUMP_BACKWARD           13 (to 6)
        >>   32 END_FOR
             34 RETURN_VALUE
```
Notice `LIST_APPEND 2` at offset 28. It appends the evaluated expression directly onto the list being built without any method lookup or python frame switching.

---

## 10. Best Practices

1. **Limit to 2 Clauses**: As a general rule of thumb, do not write comprehensions with more than two `for` or `if` clauses.
2. **Use Walrus for Expensive Computations**: When filtering by a computed property that is also needed in the output, use `(v := func(x))` in the `if` clause.
3. **Prefer Generator Expressions for Aggregations**: Pass generator expressions directly into `sum()`, `min()`, `max()`, and `any()` without enclosing brackets: `sum(x * x for x in data)`.

---

## 11. Real-World Case Study: Normalizing Database Query Results

```python
from typing import Any

raw_sql_rows = [
    {"user_id": "101", "username": " ALICE ", "role": "admin", "is_active": "1"},
    {"user_id": "102", "username": "bob", "role": "user", "is_active": "0"},
    {"user_id": "103", "username": "  CHARLIE", "role": "moderator", "is_active": "1"},
    {"user_id": "104", "username": "dave", "role": "guest", "is_active": "0"}
]

# Clean, normalize, and index active users in a single dictionary comprehension
active_user_directory = {
    int(row["user_id"]): {
        "username": row["username"].strip().lower(),
        "role": row["role"].upper()
    }
    for row in raw_sql_rows
    if row["is_active"] == "1"
}

print(f"Total active users indexed: {len(active_user_directory)}")
for uid, info in active_user_directory.items():
    print(f"  User {uid}: {info}")
```
**Output**:
```text
Total active users indexed: 2
  User 101: {'username': 'alice', 'role': 'ADMIN'}
  User 103: {'username': 'charlie', 'role': 'MODERATOR'}
```

---

## 12. Interview Questions

### Q1: Why are list comprehensions faster than equivalent `for` loops with `.append()`?
<details>
<summary><strong>Answer</strong></summary>
In a manual <code>for</code> loop, Python must execute an attribute lookup on <code>list.append</code> on every single iteration and dispatch a full Python function call. In a list comprehension, the CPython compiler emits the dedicated C-level opcode <code>LIST_APPEND</code>, which directly pushes the item to the internal C array without method lookup overhead.
</details>

### Q2: What is the scoping difference between Python 2 and Python 3 comprehensions?
<details>
<summary><strong>Answer</strong></summary>
In Python 2, list comprehension loop variables leaked into the enclosing local function scope, potentially overwriting existing variables. In Python 3, all comprehensions execute inside an isolated function-like scope, preventing loop variables from polluting or overwriting outer variables.
</details>

### Q3: How does the walrus operator (`:=`) interact with comprehension scoping?
<details>
<summary><strong>Answer</strong></summary>
Under PEP 572, variables assigned via the walrus operator inside a comprehension intentionally leak into the enclosing function scope. This allows values computed during filtering (in the <code>if</code> clause) to be reused outside the comprehension.
</details>

### Q4: How do you read the order of multiple `for` clauses in a nested comprehension?
<details>
<summary><strong>Answer</strong></summary>
Read them from left to right. The order corresponds exactly to the nesting order of equivalent imperative <code>for</code> loops: <code>[x for sublist in matrix for x in sublist]</code> corresponds to <code>for sublist in matrix: for x in sublist: ...</code>.
</details>

### Q5: What is the difference between `[x for x in seq if cond]` and `[x if cond else y for x in seq]`?
<details>
<summary><strong>Answer</strong></summary>
<code>if cond</code> at the end is a filter clause that determines whether the item is included at all; the resulting list may be shorter than <code>seq</code>. <code>x if cond else y</code> at the beginning is a ternary conditional expression that transforms every item; the resulting list has the exact same length as <code>seq</code>.
</details>

### Q6: What does passing a generator expression to `sum(x for x in seq)` achieve over `sum([x for x in seq])`?
<details>
<summary><strong>Answer</strong></summary>
It avoids allocating an intermediate list in memory. The generator expression streams elements directly to <code>sum()</code> in $O(1)$ memory, whereas enclosing square brackets allocates the entire list in RAM before computing the sum.
</details>

### Q7: Can you define a dictionary comprehension where duplicate keys occur?
<details>
<summary><strong>Answer</strong></summary>
Yes. If multiple pairs produce the same key, later assignments overwrite earlier ones. The resulting dictionary contains only the last value associated with that key.
</details>

### Q8: What bytecode opcode does a set comprehension use to add elements?
<details>
<summary><strong>Answer</strong></summary>
It uses the specialized <code>SET_ADD</code> opcode, which inserts the value directly into the internal hash table without invoking the Python <code>set.add()</code> method.
</details>

---

## 13. Exercises & Challenges

### Challenge 1 (Easy): Transpose Matrix
Given a 2D matrix represented as a list of lists `matrix = [[1, 2, 3], [4, 5, 6]]`, write a one-line list comprehension that transposes rows and columns into `[[1, 4], [2, 5], [3, 6]]`.
<details>
<summary><strong>Solution</strong></summary>

```python
matrix = [[1, 2, 3], [4, 5, 6]]
transposed = [[row[i] for row in matrix] for i in range(len(matrix[0]))]

assert transposed == [[1, 4], [2, 5], [3, 6]]
```
</details>

### Challenge 2 (Easy): Dictionary Inverter with Value Grouping
Write a dictionary comprehension or pipeline that takes `d = {"a": 1, "b": 2, "c": 1}` and produces `{1: ['a', 'c'], 2: ['b']}`.
<details>
<summary><strong>Solution</strong></summary>

```python
d = {"a": 1, "b": 2, "c": 1}
unique_vals = set(d.values())
inverted = {val: [k for k, v in d.items() if v == val] for val in unique_vals}

assert inverted == {1: ['a', 'c'], 2: ['b']}
```
</details>

### Challenge 3 (Medium): Flatten Deeply Nested Heterogeneous List
Write a list comprehension that flattens a list of mixed elements and lists `[1, [2, 3], 4, [5, [6, 7]]]` down to 1 level (unwrapping immediate list children, leaving deeper nested lists intact).
<details>
<summary><strong>Solution</strong></summary>

```python
data = [1, [2, 3], 4, [5, [6, 7]]]
flat_level_1 = [
    item
    for elem in data
    for item in (elem if isinstance(elem, list) else [elem])
]

assert flat_level_1 == [1, 2, 3, 4, 5, [6, 7]]
```
</details>

### Challenge 4 (Medium): Walrus Pattern Matcher
Given a list of filenames `["app.py", "README.md", "main.py", "styles.css", "test.py"]`, use a list comprehension with the walrus operator to extract the base name (without extension) of only `.py` files.
<details>
<summary><strong>Solution</strong></summary>

```python
files = ["app.py", "README.md", "main.py", "styles.css", "test.py"]

py_bases = [
    parts[0]
    for f in files
    if (parts := f.rsplit(".py", 1))[1] == "" and len(parts) == 2
]

assert py_bases == ["app", "main", "test"]
```
</details>

### Challenge 5 (Hard): Frequency Map and Top-K via Comprehension
Given a long text string, write a concise functional pipeline using comprehensions and `collections.Counter` logic (without importing `Counter`) that produces a dictionary of the top 3 most frequent words and their occurrence counts.
<details>
<summary><strong>Solution</strong></summary>

```python
text = "apple banana apple orange banana apple grape apple orange"
words = text.split()
unique_words = set(words)

# Dict comprehension computing counts
counts = {w: sum(1 for item in words if item == w) for w in unique_words}

# Sorted top 3 comprehension
top_3 = {k: counts[k] for k in sorted(counts, key=lambda k: counts[k], reverse=True)[:3]}

assert top_3 == {"apple": 4, "banana": 2, "orange": 2}
```
</details>

### Challenge 6 (Hard): Clean Pivot Table Generator
Implement a function `pivot_table(records: list[dict], row_key: str, col_key: str, val_key: str) -> dict[str, dict[str, float]]` using nested dictionary comprehensions that converts flat records into a 2D pivoted structure.
<details>
<summary><strong>Solution</strong></summary>

```python
records = [
    {"dept": "IT", "quarter": "Q1", "revenue": 100.0},
    {"dept": "IT", "quarter": "Q2", "revenue": 150.0},
    {"dept": "HR", "quarter": "Q1", "revenue": 50.0},
    {"dept": "HR", "quarter": "Q2", "revenue": 60.0},
]

def pivot_table(records, row_key, col_key, val_key):
    rows = sorted(set(r[row_key] for r in records))
    cols = sorted(set(r[col_key] for r in records))
    
    return {
        r_val: {
            c_val: next((r[val_key] for r in records if r[row_key] == r_val and r[col_key] == c_val), 0.0)
            for c_val in cols
        }
        for r_val in rows
    }

pivoted = pivot_table(records, "dept", "quarter", "revenue")
assert pivoted == {
    "HR": {"Q1": 50.0, "Q2": 60.0},
    "IT": {"Q1": 100.0, "Q2": 150.0}
}
```
</details>
