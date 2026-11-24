---
title: "Scope in Python: The LEGB Rule, global, and nonlocal"
slug: "scope-legb-global-nonlocal"
category: "functions"
difficulty: "intermediate"
tags: ["python", "functions", "scope", "legb", "global", "nonlocal", "namespaces"]
prerequisites: ["00-defining-functions", "01-fundamentals/01-variables-names-and-objects"]
related_topics: ["04-closures", "06-decorators"]
python_version: "3.8+"
tested_with: "CPython 3.14.0"
free_threading_support: "Yes"
---

# Scope in Python: The LEGB Rule, global, and nonlocal

## 1. Executive Summary & Core Value Proposition

Scope defines the visibility, lifetime, and resolution rules of variable identifiers within a program. In Python, variable lookups strictly follow the **LEGB Rule**, traversing four hierarchical namespace boundaries:
1. **L (Local):** Names assigned inside the currently executing function or lambda.
2. **E (Enclosing):** Names defined in the local scope of any statically enclosing/nested functions (closures).
3. **G (Global):** Names defined at the top-level of the current module file.
4. **B (Built-in):** Pre-loaded Python built-in names (`len`, `range`, `Exception`, `print`).

While Python allows freely *reading* names from outer scopes, *rebinding* (assigning) a name inside a function defaults to creating a new local variable. Modifying outer variables requires explicit scoping keywords:
- **`global`:** Instructs Python to rebind a name directly in the module's top-level global namespace.
- **`nonlocal` (PEP 3104):** Instructs Python to rebind a name in the nearest enclosing lexical function scope, bypassing the local scope without polluting the global namespace.

---

## 2. Syntax, Mechanics & Visual Architecture

### The LEGB Search Hierarchy

```
LEGB Lookup Sequence:

  +-----------------------------------------------------------+
  | Built-in Scope (B): builtins (len, sum, open, range...)   |
  |   ^                                                       |
  |   | (Fallback if not found in Global)                     |
  |   +---------------------------------------------------+   |
  |   | Global Scope (G): Module-level variables          |   |
  |   |   ^                                               |   |
  |   |   | (Fallback if not found in Enclosing)          |   |
  |   |   +-------------------------------------------+   |   |
  |   |   | Enclosing Scope (E): Outer function       |   |   |
  |   |   |   ^                                       |   |   |
  |   |   |   | (Fallback if not found in Local)      |   |   |
  |   |   |   +-----------------------------------+   |   |   |
  |   |   |   | Local Scope (L): Current function |   |   |   |
  |   |   |   +-----------------------------------+   |   |   |
  |   |   +-------------------------------------------+   |   |
  |   +---------------------------------------------------+   |
  +-----------------------------------------------------------+
```

### The `UnboundLocalError` Mechanism

In Python, scoping is determined **statically at compile time**, not dynamically at runtime. If a variable is assigned anywhere within a function body, CPython marks that identifier as **local** for the entire function:

```python
x = 10

def broken():
    print(x)  # UnboundLocalError: local variable 'x' referenced before assignment
    x = 20    # The mere existence of this assignment marks 'x' as LOCAL throughout broken()!
```

---

## 3. Real-World Code Examples

### Example 1: Tracing the Full LEGB Hierarchy

```python
# B: Built-in 'len' is pre-loaded in builtins
# G: Global variable
level = "GLOBAL"

def outer_enclosing():
    # E: Enclosing variable
    level = "ENCLOSING"
    
    def inner_local():
        # L: Local variable
        level = "LOCAL"
        print(f"Inside local     : {level} (length: {len(level)})")
        
    inner_local()
    print(f"Inside enclosing : {level}")

outer_enclosing()
print(f"Inside global    : {level}")
```

**Output:**
```
Inside local     : LOCAL (length: 5)
Inside enclosing : ENCLOSING
Inside global    : GLOBAL
```

---

### Example 2: Rebinding Module State with `global`

```python
# Module-level connection pool
active_connections: int = 0

def open_connection():
    global active_connections  # Declares intent to rebind global name
    active_connections += 1
    print(f"Connection opened. Active: {active_connections}")

def close_connection():
    global active_connections
    if active_connections > 0:
        active_connections -= 1
    print(f"Connection closed. Active: {active_connections}")

open_connection()
open_connection()
close_connection()
print(f"Final global active connections: {active_connections}")
```

**Output:**
```
Connection opened. Active: 1
Connection opened. Active: 2
Connection closed. Active: 1
Final global active connections: 1
```

---

### Example 3: Stateful Encapsulation with `nonlocal` (PEP 3104)

```python
from typing import Callable

def make_rate_limiter(max_limit: int) -> Callable[[], bool]:
    """Factory returning a stateful rate limiter using nonlocal."""
    request_count = 0  # Enclosing scope variable
    
    def attempt_request() -> bool:
        nonlocal request_count  # Rebinds 'request_count' in enclosing scope!
        if request_count < max_limit:
            request_count += 1
            print(f"Request allowed ({request_count}/{max_limit})")
            return True
        else:
            print(f"Rate limit exceeded ({request_count}/{max_limit})!")
            return False
            
    return attempt_request

limiter = make_rate_limiter(max_limit=2)
limiter()
limiter()
limiter()
```

**Output:**
```
Request allowed (1/2)
Request allowed (2/2)
Rate limit exceeded (2/2)!
```

---

### Example 4: Diagnosing and Fixing `UnboundLocalError`

```python
balance = 1000

# BROKEN SCENARIO:
def deposit_broken(amount: int):
    try:
        # Fails because 'balance' is marked local due to '+=' assignment below!
        balance = balance + amount
    except UnboundLocalError as err:
        print("Caught expected scoping error:", err)

deposit_broken(250)

# FIXED SCENARIO 1: Explicit global declaration
def deposit_global(amount: int):
    global balance
    balance += amount
    return balance

print("New balance via global:", deposit_global(250))

# FIXED SCENARIO 2: Pure functional architecture (preferred)
def deposit_pure(current_balance: int, amount: int) -> int:
    return current_balance + amount

print("New balance via pure function:", deposit_pure(balance, 500))
```

**Output:**
```
Caught expected scoping error: cannot access local variable 'balance' where it is not associated with a value
New balance via global: 1250
New balance via pure function: 1750
```

---

### Example 5: Introspecting Namespaces (`locals()`, `globals()`, `__builtins__`)

```python
MODULE_FLAG = "PROD_ACTIVE"

def inspect_namespaces(param_a: int, param_b: str):
    local_calc = param_a * 10
    
    # locals() reflects current local frame variables
    print("Local namespace keys  :", sorted(locals().keys()))
    
    # globals() reflects module-level attributes
    print("Is MODULE_FLAG in globals()?", "MODULE_FLAG" in globals())
    
    # Built-in namespace inspection
    import builtins
    print("Is 'abs' in builtins?     :", hasattr(builtins, "abs"))

inspect_namespaces(5, "telemetry")
```

**Output:**
```
Local namespace keys  : ['local_calc', 'param_a', 'param_b']
Is MODULE_FLAG in globals()? True
Is 'abs' in builtins?     : True
```

---

## 4. Common Anti-Patterns & Mistakes

| Anti-Pattern / Mistake | Broken / Unidiomatic Snippet | Idiomatic Correction | Why It Fails |
| :--- | :--- | :--- | :--- |
| **Shadowing built-in names** | `list = [1, 2, 3]` or `id = 101` | Use `user_id`, `items`, `lst` | Rebinds the built-in name in global/local scope, breaking `list()` or `id()` calls. |
| **Overusing `global` for state** | Multiple functions mutating globals | Pass state explicitly or use classes | Creates tight coupling, hidden side-effects, and breaks unit testing isolation. |
| **Using `nonlocal` for global names** | `nonlocal x` *(when x is module-level)* | Use `global x` | `nonlocal` only searches enclosing functions; raises `SyntaxError: no binding for nonlocal 'x' found`. |
| **UnboundLocal on read-before-write** | `if ready: count += 1` | Initialize or declare `global` | `count += 1` is an assignment; marks `count` local for the entire function. |
| **Expecting `locals()` mutations to persist** | `locals()['x'] = 10` | Assign directly: `x = 10` | In CPython, `locals()` returns a copy/dictionary representation of fast stack locals; modifying it does nothing. |
| **Nested function reading without nonlocal** | Mutating outer scalar: `count += 1` | Add `nonlocal count` | Without `nonlocal`, attempting to rebind an outer variable creates an `UnboundLocalError`. |

---

## 5. Architectural & Design Decisions

### Why Did PEP 3104 Introduce `nonlocal`?
Prior to Python 3.0, there was no way to rebind a variable in an enclosing function scope. Developers were forced to use hacky workarounds, such as storing state in a 1-element list (`count = [0]`) or attaching attributes to functions. PEP 3104 added `nonlocal` to cleanly support stateful closures and functional encapsulation without polluting module globals.

### Static Name Resolution (`LOAD_FAST` vs `LOAD_GLOBAL`)
CPython's compiler analyzes scopes during AST generation:
- Local variables are assigned integer array offsets and accessed via the ultra-fast `LOAD_FAST` opcode ($O(1)$ array offset on the C stack frame).
- Global and built-in variables are accessed via `LOAD_GLOBAL`, which performs dictionary lookups ($O(1)$ hash table lookup, optimized in 3.11+ via inline caching).

---

## 6. Verification & Practical Testing

```python
def test_legb_resolution():
    val = "global"
    def outer():
        val = "enclosing"
        def inner():
            return val
        return inner()
    assert outer() == "enclosing"

def test_nonlocal_mutation():
    def make_accumulator():
        total = 0
        def add(x):
            nonlocal total
            total += x
            return total
        return add
    acc = make_accumulator()
    assert acc(10) == 10
    assert acc(20) == 30

def test_unbound_local_trap():
    x = 5
    def f():
        nonlocal_x_failed = False
        try:
            _ = x
            x = 10  # Makes x local!
        except UnboundLocalError:
            nonlocal_x_failed = True
        return nonlocal_x_failed
    assert f() is True

if __name__ == "__main__":
    test_legb_resolution()
    test_nonlocal_mutation()
    test_unbound_local_trap()
    print("All scope and LEGB tests passed successfully.")
```

**Output:**
```
All scope and LEGB tests passed successfully.
```

---

## 7. Deep Dive: Bytecode Disassembly (`LOAD_FAST` vs `LOAD_DEREF`)

```python
import dis

def make_adder(n):
    def adder(x):
        return x + n
    return adder

dis.dis(make_adder)
```

In the disassembly:
1. `make_adder` compiles `n` into a **cell object** using `STORE_DEREF`.
2. Inside `adder`, `x` is accessed via `LOAD_FAST 0` (local stack).
3. `n` is accessed via `LOAD_DEREF 0` (dereferencing the shared closure cell pointer).

This allows enclosing scopes to share memory cells between outer and inner functions seamlessly.

---

## 8. Summary & Key Takeaways

1. **LEGB Rule:** Python resolves names in order: Local $\rightarrow$ Enclosing $\rightarrow$ Global $\rightarrow$ Built-in.
2. **Assignment Is Local:** Any assignment to a variable inside a function makes it local for the *entire* function body.
3. **`global` Keyword:** Binds a variable to the module's top-level namespace.
4. **`nonlocal` Keyword:** Binds a variable to the nearest enclosing function scope (closures).
5. **Fast Execution:** Local variables are optimized via `LOAD_FAST` array indexing, making local access faster than global access.

---

## 9. Frequently Asked Questions (Interview & Senior Roles)

<details>
<summary>1. What causes an `UnboundLocalError` and how is it resolved?</summary>

It occurs when a function attempts to read a local variable before it has been assigned. If an assignment statement (`x = ...` or `x += ...`) exists anywhere in the function, Python marks `x` as local for the entire function. If code attempts to read `x` before that assignment executes, Python raises `UnboundLocalError`. Fix by declaring `global x`, `nonlocal x`, or passing `x` as an explicit parameter.
</details>

<details>
<summary>2. What is the difference between `global` and `nonlocal`?</summary>

- `global` directs Python to resolve and rebind the identifier in the top-level module namespace (`globals()`).
- `nonlocal` directs Python to resolve and rebind the identifier in the nearest enclosing outer function scope (`__closure__`), excluding module globals.
</details>

<details>
<summary>3. Can `nonlocal` bind to a variable defined at module level?</summary>

No. `nonlocal` requires that the target variable exists in an *enclosing function* scope. If no matching enclosing function scope contains the variable, Python raises `SyntaxError: no binding for nonlocal '<name>' found`.
</details>

<details>
<summary>4. Why is accessing local variables faster than accessing global variables in CPython?</summary>

Local variables are determined at compile time and mapped to a fixed-size C array on the call frame (`f_localsplus`), accessed in a single CPU instruction via `LOAD_FAST`. Global variables require hash lookups in `f_globals` and `__builtins__` dictionaries via `LOAD_GLOBAL`.
</details>

<details>
<summary>5. Can you modify local variables by assigning to `locals()['x'] = 10`?</summary>

No. In CPython, `locals()` creates a new dictionary snapshot of the fast local variables array. Mutating this dictionary does not write changes back to the actual CPU evaluation frame.
</details>

<details>
<summary>6. How does class definition scope interact with methods?</summary>

Class bodies define a local scope, but methods defined inside the class do **not** have the class body as an enclosing scope! For example:
```python
class MyClass:
    val = 42
    def method(self):
        return val  # NameError: name 'val' is not defined (must use self.val or MyClass.val)
```
</details>

<details>
<summary>7. What is a "cell object" in CPython scope mechanics?</summary>

A cell object (`PyCellObject`) is a heap container holding a reference to a variable that is shared between an enclosing scope and one or more closures. It allows both scopes to read and mutate the variable through an extra level of indirection even after the outer function has returned.
</details>

<details>
<summary>8. How does free-threaded Python (PEP 703) handle `global` and `nonlocal` variables?</summary>

In free-threaded Python (no-GIL), global and nonlocal variables shared across concurrent threads are susceptible to data races. Modifying shared state requires explicit thread synchronization (e.g. `threading.Lock`) because atomic bytecode guarantees do not prevent high-level race conditions.
</details>

---

## 10. Practical Exercises & Self-Assessment

<details>
<summary>Exercise 1: Diagnostic Scope Puzzle</summary>

**Task:** Predict the output of the following code:
```python
x = 1
def f1():
    x = 2
    def f2():
        nonlocal x
        x = 3
        print("f2:", x)
    f2()
    print("f1:", x)
f1()
print("global:", x)
```

**Solution:**
```
f2: 3
f1: 3
global: 1
```
`nonlocal x` modifies `x` in `f1`'s scope from 2 to 3, but leaves the global `x` untouched at 1.
</details>

<details>
<summary>Exercise 2: Stateful Transaction ID Generator</summary>

**Task:** Create a generator function `make_tx_generator(prefix: str)` that produces incrementing transaction IDs like `"TX-1"`, `"TX-2"` using `nonlocal`.

**Solution:**
```python
def make_tx_generator(prefix: str):
    tx_id = 0
    def next_tx():
        nonlocal tx_id
        tx_id += 1
        return f"{prefix}-{tx_id}"
    return next_tx

gen = make_tx_generator("ORD")
print(gen())  # ORD-1
print(gen())  # ORD-2
```
</details>

<details>
<summary>Exercise 3: Benchmark Local vs Global Access</summary>

**Task:** Measure the execution time difference between summing numbers in a local variable vs summing in a global variable over 5,000,000 iterations.

**Solution:**
```python
import time

# Global variable
global_counter = 0
def test_global():
    global global_counter
    for _ in range(5_000_000):
        global_counter += 1

# Local variable
def test_local():
    local_counter = 0
    for _ in range(5_000_000):
        local_counter += 1
    return local_counter

t0 = time.perf_counter(); test_global(); t_glob = time.perf_counter() - t0
t0 = time.perf_counter(); test_local(); t_loc = time.perf_counter() - t0

print(f"Global time: {t_glob*1000:.2f} ms")
print(f"Local time : {t_loc*1000:.2f} ms (Faster by {(t_glob/t_loc - 1)*100:.1f}%)")
```
</details>

<details>
<summary>Exercise 4: Scope Shadowing Fix</summary>

**Task:** Fix the following code where a function parameter accidentally shadows a built-in function:
```python
def print_ids(id: int, list: list):
    print(id, list)
```

**Solution:**
```python
def print_ids(entity_id: int, items: list):
    print(entity_id, items)
```
</details>

<details>
<summary>Exercise 5: Reversible Stack Context with Nonlocal</summary>

**Task:** Implement an undoable counter using `nonlocal` that supports `increment()` and `undo()`.

**Solution:**
```python
def create_undoable_counter(start: int = 0):
    current = start
    history = []

    def inc(delta: int = 1):
        nonlocal current
        history.append(current)
        current += delta
        return current

    def undo():
        nonlocal current
        if history:
            current = history.pop()
        return current

    return inc, undo

inc, undo = create_undoable_counter()
print(inc(10))  # 10
print(inc(5))   # 15
print(undo())   # 10
```
</details>

<details>
<summary>Exercise 6: Detect Scoping in Lambda</summary>

**Task:** Explain what happens when a lambda references an enclosing loop variable, and fix it using default argument binding.

**Solution:**
```python
# BROKEN (Late binding): All functions see final value 2
funcs = [lambda: i for i in range(3)]
print([f() for f in funcs])  # [2, 2, 2]

# FIXED (Early binding via default argument):
funcs_fixed = [lambda i=i: i for i in range(3)]
print([f() for f in funcs_fixed])  # [0, 1, 2]
```
</details>
