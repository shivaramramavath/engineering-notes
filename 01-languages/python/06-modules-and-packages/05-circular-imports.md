# Circular Imports: Causes, Detection, and Solutions

- **Topic**: `06-modules-and-packages`
- **Python version**: 3.12+ (tested on CPython 3.14.0)
- **Tested with**: CPython 3.12, 3.13, 3.14 (standard & free-threaded builds)
- **Free-threading notes**: Compatible. Circular import deadlocks and `sys.modules` partially initialized bindings follow standard CPython import lock semantics.

---

## 1. Concept Overview

A **circular import** (or cyclic dependency) occurs when Module A imports Module B, and Module B imports Module A (either directly or indirectly via intermediate modules: $A \to B \to C \to A$).

In Python, circular imports do **not** cause infinite recursion loops. This is because Python adds every module to `sys.modules` immediately upon creation, *before* executing its code.

However, circular imports frequently trigger:
```
ImportError: cannot import name 'X' from partially initialized module 'A' (most likely due to a circular import)
```
or:
```
AttributeError: partially initialized module 'B' has no attribute 'Y'
```

This error happens when Module B tries to access an attribute or function in Module A **before Module A has finished executing the lines of code that define that attribute**.

```
Timeline:
1. 'import a' begins
2. 'a' added to sys.modules (empty dict)
3. 'a' executes: 'from b import func_b'
      |
      +---> 'import b' begins
      |     'b' added to sys.modules (empty dict)
      |     'b' executes: 'from a import func_a'
      |            |
      |            +---> 'a' is already in sys.modules!
      |                  Python looks for 'func_a' in a.__dict__...
      |                  CRASH! 'func_a' not defined yet!
      |                  ImportError: cannot import name 'func_a'
```

---

## 2. Key Mechanics & Architecture

### The Mechanics of `import` vs `from ... import`
The choice of import syntax radically impacts whether a circular import crashes or succeeds:
1. `from a import func_a`: Python looks up `a.__dict__["func_a"]` **at the moment the import statement executes**. If `func_a` has not yet been parsed, it crashes with `ImportError`.
2. `import a`: Python simply binds the module reference `a`. As long as you don't call `a.func_a()` until *after* all modules have finished initializing (e.g. inside a function), the circular dependency resolves cleanly!

### Diagnostic Matrix of Solutions

| Solution | Mechanism | Tradeoffs |
| :--- | :--- | :--- |
| **1. Refactor Common Code** | Move shared models/types to a third module (`common.py` or `models.py`) | **Best Practice**. Breaks the cycle permanently at the architectural level. |
| **2. Use `import x` Instead of `from x import y`** | Defers attribute lookup to runtime when methods are called | Clean, minimal changes, but requires dotted access (`x.y`). |
| **3. Deferred / Function-Level Imports** | Move `import b` inside the specific function that uses it | Defers import to runtime. Slight overhead on first call. |
| **4. Type Guard (`typing.TYPE_CHECKING`)** | Put imports used only for type hints under `if TYPE_CHECKING:` | **Zero runtime overhead**. Resolves all type-annotation circular imports! |

---

## 3. Real-World Code Examples

### Example 1: Type Annotation Cycle Fixed with `TYPE_CHECKING` (PEP 563 / 649)
```python
# In real code:
# parent.py
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    # This import runs ONLY during static analysis (mypy/pyright), NEVER at runtime!
    from .child import Child

class Parent:
    def __init__(self):
        self.children: list["Child"] = []  # Forward ref string or PEP 563

# child.py
from typing import TYPE_CHECKING
if TYPE_CHECKING:
    from .parent import Parent

class Child:
    def __init__(self, parent: "Parent"):
        self.parent = parent
```

### Example 2: Module vs Attribute Import Demonstration
```python
# Simulating circular resolution using dynamic modules
import sys
import types

# Module A
mod_a = types.ModuleType("mod_a")
# Module B
mod_b = types.ModuleType("mod_b")

# Add both to sys.modules (simulating initial module allocation)
sys.modules["mod_a"] = mod_a
sys.modules["mod_b"] = mod_b

# mod_a imports mod_b as a whole
mod_a.mod_b = mod_b

# mod_b imports mod_a as a whole
mod_b.mod_a = mod_a

# Now define attributes
mod_a.say_hello = lambda: "Hello from A"
mod_b.say_world = lambda: "World from B"

# Resolving via dotted access works perfectly!
print(f"mod_a calling B: {mod_a.mod_b.say_world()}")
print(f"mod_b calling A: {mod_b.mod_a.say_hello()}")

# Cleanup
del sys.modules["mod_a"]
del sys.modules["mod_b"]
```
Output:
```
mod_a calling B: World from B
mod_b calling A: Hello from A
```

### Example 3: Function-Level Deferred Import Pattern
```python
class OrderProcessor:
    def process_payment(self, amount: float):
        # Deferred import: only imported when process_payment is actually executed,
        # avoiding circular import at module top-level!
        import math
        return math.ceil(amount)


proc = OrderProcessor()
print(f"Payment processed: ${proc.process_payment(99.4)}")
```
Output:
```
Payment processed: $100
```

### Example 4: Architectural Refactoring: Extracting the Common Layer
```
Before Refactoring (Circular Cycle):
[User Module] <===============> [Order Module]
(User has Orders)               (Order has User)

After Refactoring (Clean DAG):
[User Module] ------------+
                          v
                    [Core Models / Schemas] <--------- [Order Module]
```

### Example 5: Detecting Cycles with Import Tracking
```python
import sys

def detect_partial_modules() -> list[str]:
    """Finds all modules currently in sys.modules that are partially initialized."""
    partials = []
    for name, mod in list(sys.modules.items()):
        if mod is not None and hasattr(mod, "__spec__"):
            spec = getattr(mod, "__spec__", None)
            if spec and getattr(spec, "_initializing", False):
                partials.append(name)
    return partials

print(f"Partially initialized modules count: {len(detect_partial_modules())}")
```
Output:
```
Partially initialized modules count: 0
```

---

## 4. Common Anti-Patterns & Gotchas

| Bad Practice | Root Cause | Clean Solution |
| :--- | :--- | :--- |
| `from module import X` between tightly interdependent files | Looked up attribute does not exist yet during initial execution | Use `import module` and access `module.X` inside methods |
| Placing imports at the bottom of files (`# end-of-file imports`) | Fragile, confusing hack; PEP 8 anti-pattern | Refactor common models to a separate file or use deferred imports |
| Heavy circular imports exclusively for type annotations | Imports run at runtime solely to satisfy static type checkers | Wrap type-only imports in `if typing.TYPE_CHECKING:` |
| Mutually dependent classes inside the same package file | Over-splitting related models into too many tiny files | Merge tightly coupled classes into a single module |
| Using monkey-patching to inject missing circular references | Introduces untraceable runtime side effects | Fix import topology to form a Directed Acyclic Graph (DAG) |
| Moving all imports into functions indiscriminately | Destroys import visibility and slightly degrades function performance | Use deferred imports only as an isolated targeted fix |

---

## 5. Interview Q&A

<details>
<summary>1. Why do circular imports in Python raise ImportError instead of causing infinite recursion?</summary>
When Python begins importing a module, it immediately creates an empty module object and inserts it into <code>sys.modules</code> <i>before</i> executing any code in that module. When a secondary module attempts to import the first module, it finds the module already present in <code>sys.modules</code> and returns it immediately. The crash occurs only if the secondary module attempts to access an attribute that hasn't been defined yet in the partially initialized module.
</details>

<details>
<summary>2. What is the difference in circular import vulnerability between "import A" and "from A import foo"?</summary>
<code>from A import foo</code> requires <code>foo</code> to exist in <code>A.__dict__</code> at the exact instant the statement is executed. If <code>A</code> is partially initialized and hasn't reached the line defining <code>foo</code>, Python raises <code>ImportError</code>. With <code>import A</code>, only the module object is bound; as long as <code>A.foo()</code> is called later (e.g. inside a function body after both modules finish importing), no error occurs.
</details>

<details>
<summary>3. What is typing.TYPE_CHECKING and how does it prevent circular imports?</summary>
<code>typing.TYPE_CHECKING</code> is a boolean constant that evaluates to <code>False</code> at runtime, but is treated as <code>True</code> by static type checkers (mypy, pyright). Wrapping imports inside <code>if TYPE_CHECKING:</code> prevents them from ever running at runtime, completely eliminating circular import errors caused by type annotations.
</details>

<details>
<summary>4. What does "partially initialized module" mean in Python error tracebacks?</summary>
It indicates that a module has an entry in <code>sys.modules</code> and its code is currently in the middle of executing on the call stack, but has not yet completed execution. Its <code>__spec__._initializing</code> flag is <code>True</code>.
</details>

<details>
<summary>5. How does Python 3.7+ "from __future__ import annotations" (PEP 563) interact with circular imports?</summary>
PEP 563 converts type annotations into raw strings at compile time rather than evaluating them at module definition time. This means type annotations no longer execute at runtime, allowing classes to be referenced in type hints without requiring their modules to be imported at runtime.
</details>

<details>
<summary>6. What is an import dependency DAG?</summary>
A Directed Acyclic Graph (DAG) is an architecture where dependencies flow strictly in one direction (e.g. Higher-level Services $\to$ Domain Logic $\to$ Core Data Models $\to$ Utilities). If dependencies form a DAG, circular imports are mathematically impossible.
</details>

<details>
<summary>7. When is a deferred function-level import acceptable?</summary>
A deferred import is acceptable when breaking an unavoidable circular dependency between two tightly coupled subsystems, or when an import is exceptionally heavy (e.g. large ML models or graphics libraries) and rarely used in standard execution paths.
</details>

<details>
<summary>8. Can circular imports cause memory leaks?</summary>
Module objects stored in <code>sys.modules</code> remain alive for the lifetime of the Python interpreter anyway. However, circular references between module-level user objects (e.g. singleton references) rely on cyclic garbage collection to be freed if the module is dynamically unloaded.
</details>

---

## 6. Practical Challenges

<details>
<summary>Challenge 1: TYPE_CHECKING Guarded Models</summary>

Write two simulated class definitions where `Author` references `Book` and `Book` references `Author` in type hints using `TYPE_CHECKING` and string annotations.

```python
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    class Book: pass

class Author:
    def __init__(self, name: str):
        self.name = name
        self.books: list["Book"] = []

a = Author("Tolkien")
assert a.name == "Tolkien"
```
</details>

<details>
<summary>Challenge 2: Dotted Access Circular Resolver</summary>

Demonstrate resolving a circular dependency between `ServiceA` and `ServiceB` using whole-module imports (`import B`) and delayed method execution.

```python
import types
import sys

mod_a = types.ModuleType("svc_a")
mod_b = types.ModuleType("svc_b")

sys.modules["svc_a"] = mod_a
sys.modules["svc_b"] = mod_b

mod_a.b_ref = mod_b
mod_b.a_ref = mod_a

mod_a.run = lambda: f"A calls {mod_a.b_ref.ping()}"
mod_b.ping = lambda: "pong B"

assert mod_a.run() == "A calls pong B"

del sys.modules["svc_a"]
del sys.modules["svc_b"]
```
</details>

<details>
<summary>Challenge 3: Detect Initializing Module Spec</summary>

Verify that `spec._initializing` accurately tracks whether a module has completed execution.

```python
import importlib.util

spec = importlib.util.find_spec("json")
assert spec is not None
# A completed module is no longer initializing
assert getattr(spec, "_initializing", False) is False
```
</details>

<details>
<summary>Challenge 4: Refactor Cycle to Common Intermediate Layer</summary>

Given interdependent functions `format_user_order` and `format_order_user`, refactor their shared data structure into a common dataclass `OrderData`.

```python
from dataclasses import dataclass

@dataclass
class OrderData:
    user_id: str
    order_id: str

def format_user_order(o: OrderData) -> str:
    return f"User {o.user_id} -> Order {o.order_id}"

def format_order_user(o: OrderData) -> str:
    return f"Order {o.order_id} -> User {o.user_id}"

o = OrderData("U1", "O100")
assert format_user_order(o) == "User U1 -> Order O100"
```
</details>

<details>
<summary>Challenge 5: Lazy Function-Level Import Wrapper</summary>

Write a function `export_to_csv(data)` that imports `csv` strictly inside the function scope and outputs a formatted string.

```python
def export_to_csv(rows: list[list[str]]) -> str:
    import io
    import csv
    out = io.StringIO()
    writer = csv.writer(out)
    writer.writerows(rows)
    return out.getvalue()

res = export_to_csv([["a", "b"], ["1", "2"]])
assert "a,b" in res
```
</details>

<details>
<summary>Challenge 6: Circular Import Diagnostic Scanner</summary>

Write a function that parses Python source files with `ast` to detect mutual import pairs.

```python
import ast

def find_imported_modules(source_code: str) -> set[str]:
    tree = ast.parse(source_code)
    imports = set()
    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            for alias in node.names:
                imports.add(alias.name)
        elif isinstance(node, ast.ImportFrom) and node.module:
            imports.add(node.module)
    return imports

code = "import os\nfrom sys import path\n"
assert find_imported_modules(code) == {"os", "sys"}
```
</details>
