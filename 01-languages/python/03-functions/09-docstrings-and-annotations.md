---
title: "Docstrings, Type Annotations, and PEP 649/749 in Python"
slug: "docstrings-and-annotations"
category: "functions"
difficulty: "intermediate"
tags: ["python", "functions", "docstrings", "type-annotations", "pep-257", "pep-484", "pep-649", "pep-749", "annotationlib"]
prerequisites: ["00-defining-functions", "01-fundamentals/08-comments-and-docstrings"]
related_topics: ["11-type-hints-and-static-typing/00-type-system-overview", "05-editor-and-debugger-setup"]
python_version: "3.10+"
tested_with: "CPython 3.14.0"
free_threading_support: "Yes"
---

# Docstrings, Type Annotations, and PEP 649/749 in Python

## 1. Executive Summary & Core Value Proposition

In modern Python, function signatures serve as both human-readable documentation and machine-checkable contracts. This dual capability is achieved through:
1. **Docstrings (PEP 257):** Multi-line string literals placed as the first statement in a function body, exposing runtime documentation through `__doc__` and `help()`.
2. **Type Annotations (PEP 3107 & PEP 484):** Optional syntactic metadata attached to parameters and return types, powering IDE autocompletion, static type checkers (Mypy, Pyright), and runtime validation engines (Pydantic, FastAPI).
3. **Deferred Evaluation (PEP 649 & PEP 749 in Python 3.14+):** Resolves the long-standing tension between runtime performance and forward references by compiling annotations into deferred code objects evaluated on-demand via `annotationlib`, replacing the stringifying semantics of `from __future__ import annotations` (PEP 563).

---

## 2. Syntax, Mechanics & Visual Architecture

### Docstring Styles Comparison

```python
# Google Style (Concise, highly readable in code editors):
def connect(host: str, port: int = 5432) -> bool:
    """Establishes database connection.

    Args:
        host: Hostname or IP address of the database.
        port: Connection port (default: 5432).

    Returns:
        True if handshake succeeded.

    Raises:
        ConnectionError: If network socket fails.
    """

# Sphinx / reStructuredText Style (Standard for ReadTheDocs / Sphinx):
def connect(host: str, port: int = 5432) -> bool:
    """Establishes database connection.

    :param host: Hostname or IP address.
    :param port: Connection port.
    :return: True if handshake succeeded.
    :raises ConnectionError: If socket fails.
    """
```

### The Evolution of Annotation Evaluation

```
Timeline of Python Annotation Evaluation:

Python 3.0 - 3.6 (Eager Execution):
  def f(node: Node): ...  --> NameError if 'Node' not yet defined!
  
Python 3.7 - 3.13 (PEP 563 Future Import):
  from __future__ import annotations
  def f(node: Node): ...  --> Stored as STRING "Node" in __annotations__.
                              Breaks runtime tools (Pydantic) without eval().
                              
Python 3.14+ (PEP 649 & PEP 749 - Deferred Evaluation):
  def f(node: Node): ...  --> Stored as a compiled code descriptor!
                              Evaluated lazily ONLY when inspected via annotationlib.
                              Eliminates import-time execution while preserving real types.
```

---

## 3. Real-World Code Examples

### Example 1: Google-Style Docstring with Runtime `help()` Inspection

```python
def calculate_tax(subtotal: float, tax_rate: float = 0.08) -> float:
    """Calculates final tax amount rounded to two decimal places.

    Args:
        subtotal: The pre-tax monetary amount in USD.
        tax_rate: The tax percentage expressed as a decimal (default 0.08).

    Returns:
        The total tax amount rounded to 2 decimal places.

    Raises:
        ValueError: If subtotal or tax_rate is strictly negative.
    """
    if subtotal < 0 or tax_rate < 0:
        raise ValueError("Financial amounts cannot be negative.")
    return round(subtotal * tax_rate, 2)

print("Tax on $100:", calculate_tax(100.0))
print("Raw __doc__ attribute:\n", calculate_tax.__doc__.strip().split("\n")[0])
```

**Output:**
```
Tax on $100: 8.0
Raw __doc__ attribute:
 Calculates final tax amount rounded to two decimal places.
```

---

### Example 2: Introspecting Annotations with `inspect.get_annotations()`

Since Python 3.10, the official, idiomatic mechanism to inspect annotations is `inspect.get_annotations()` (rather than accessing `__annotations__` directly).

```python
import inspect

def provision_cluster(
    cluster_name: str,
    node_count: int = 3,
    high_availability: bool = True
) -> dict[str, str | int]:
    """Provisions an orchestration cluster."""
    return {"cluster": cluster_name, "nodes": node_count}

annotations = inspect.get_annotations(provision_cluster)

print("--- Extracted Annotations ---")
for param_name, type_hint in annotations.items():
    print(f"  {param_name:<18}: {type_hint}")
```

**Output:**
```
--- Extracted Annotations ---
  cluster_name      : <class 'str'>
  node_count        : <class 'int'>
  high_availability : <class 'bool'>
  return            : dict[str, str | int]
```

---

### Example 3: Forward References with `from __future__ import annotations`

Without deferred annotations, referencing a class before it is defined raises a runtime `NameError`.

```python
from __future__ import annotations  # Enables PEP 563 stringified annotations

class TreeNode:
    def __init__(self, value: int, left: TreeNode | None = None, right: TreeNode | None = None):
        self.value = value
        self.left = left
        self.right = right

    def clone_with_parent(self, parent: TreeNode | None) -> TreeNode:
        return TreeNode(self.value, self.left, self.right)

# Under PEP 563, annotations are strings
print("Raw __annotations__:", TreeNode.clone_with_parent.__annotations__)
```

**Output:**
```
Raw __annotations__: {'parent': 'TreeNode | None', 'return': 'TreeNode'}
```

---

### Example 4: Python 3.14+ Deferred Annotations (`annotationlib`)

In Python 3.14+, `annotationlib` provides the standardized engine for inspecting annotations under PEP 649 / PEP 749.

```python
import sys

def service_handler(payload: str, timeout: int = 30) -> bool:
    return True

# Python 3.14+ inspection with annotationlib
if sys.version_info >= (3, 14):
    import annotationlib
    # Format: VALUE (evaluates types), FORWARDREF (evaluates with placeholders), SOURCE (returns code/strings)
    resolved = annotationlib.get_annotations(service_handler, format=annotationlib.Format.VALUE)
    print("Resolved annotations via annotationlib:", resolved)
else:
    import inspect
    print("Standard inspect:", inspect.get_annotations(service_handler))
```

**Output:**
```
Resolved annotations via annotationlib: {'payload': <class 'str'>, 'timeout': <class 'int'>, 'return': <class 'bool'>}
```

---

### Example 5: Runtime Type Verification Decorator Using Annotations

```python
import functools
import inspect
from typing import Callable, Any

def validate_call_types(fn: Callable) -> Callable:
    """Decorator checking that arguments match type annotations at runtime."""
    sig = inspect.signature(fn)
    
    @functools.wraps(fn)
    def wrapper(*args: Any, **kwargs: Any) -> Any:
        bound = sig.bind(*args, **kwargs)
        for name, value in bound.arguments.items():
            expected_type = sig.parameters[name].annotation
            if expected_type is not inspect.Parameter.empty:
                if not isinstance(value, expected_type):
                    raise TypeError(
                        f"Argument '{name}' expected {expected_type.__name__}, got {type(value).__name__}"
                    )
        return fn(*args, **kwargs)
    return wrapper

@validate_call_types
def set_bandwidth_limit(interface: str, limit_mbps: int) -> str:
    return f"Interface {interface} capped at {limit_mbps} Mbps"

print(set_bandwidth_limit("eth0", 1000))

try:
    set_bandwidth_limit("eth0", "invalid_number")  # type: ignore
except TypeError as err:
    print("Caught validation error:", err)
```

**Output:**
```
Interface eth0 capped at 1000 Mbps
Caught validation error: Argument 'limit_mbps' expected int, got str
```

---

## 4. Common Anti-Patterns & Mistakes

| Anti-Pattern / Mistake | Broken / Unidiomatic Snippet | Idiomatic Correction | Why It Fails |
| :--- | :--- | :--- | :--- |
| **Direct `fn.__annotations__` access** | `types = fn.__annotations__` | `inspect.get_annotations(fn)` | Fails to resolve inherited or forward-referenced annotations correctly. |
| **Redundant comments duplicating docstrings** | Commenting `# Returns result` inside body | Document in docstring | Inline comments are invisible to `help()` and documentation generators. |
| **Quoting types manually in 3.10+** | `def f(x: 'MyClass'):` | `from __future__ import annotations` or 3.14+ | String quotes are messy; modern Python supports bare class names. |
| **Assuming type hints enforce types** | `def f(x: int):` *(expects runtime type check)* | Annotations do **not** check types at runtime | Python remains dynamically typed; annotations are purely metadata unless validated by tools. |
| **Out-of-sync docstring arguments** | Docstring lists `param3` which was renamed | Keep docstrings synchronized with signatures | Misleading documentation is worse than no documentation. |
| **Using `pass` after docstring** | `def f(): """Doc""" pass` | Remove `pass` | The docstring is already a valid statement; `pass` is dead code. |

---

## 5. Architectural & Design Decisions

### Why Aren't Type Annotations Enforced by Default?
Python's core philosophy prioritizes dynamic typing, duck typing, and developer agility. Enforcing type checks at runtime in the CPython virtual machine would:
1. Incur substantial performance penalties (up to 300% slowdown on function calls).
2. Break duck typing (e.g. passing a mock object or custom sequence into a function expecting a list).
Instead, Python separates **static verification** (Mypy, Pyright) from **runtime execution**, reserving runtime validation for boundary layers (Pydantic, FastAPI).

---

## 6. Verification & Practical Testing

```python
import inspect

def test_annotation_extraction():
    def compute(a: int, b: float = 1.0) -> float:
        """Computes a product."""
        return a * b
        
    ann = inspect.get_annotations(compute)
    assert ann["a"] is int
    assert ann["b"] is float
    assert ann["return"] is float
    assert compute.__doc__ == "Computes a product."

def test_empty_annotations():
    def unannotated(x): pass
    assert inspect.get_annotations(unannotated) == {}

if __name__ == "__main__":
    test_annotation_extraction()
    test_empty_annotations()
    print("All docstring and annotation tests passed successfully.")
```

**Output:**
```
All docstring and annotation tests passed successfully.
```

---

## 7. Deep Dive: PEP 649 & PEP 749 (`annotationlib` Architecture)

Under PEP 649 (introduced in Python 3.14):
1. Annotations are compiled into a separate function/code descriptor (`annotate` function) attached to the function object.
2. If `__annotations__` is never accessed, the code descriptor is never executed ($O(0)$ import-time overhead).
3. When `annotationlib.get_annotations(obj, format=...)` is called, it executes the descriptor in the defining scope, resolving the real types cleanly.

This architectural breakthrough completely eliminates the need for stringified annotations while supporting forward references.

---

## 8. Summary & Key Takeaways

1. **Docstrings:** Expose runtime documentation accessible via `help()` and `obj.__doc__`.
2. **Annotation Extraction:** Always use `inspect.get_annotations(fn)` (or `annotationlib` in 3.14+).
3. **No Runtime Enforcement:** Annotations do not enforce types automatically; use Mypy statically or Pydantic dynamically.
4. **Deferred Annotations:** PEP 649/749 defers evaluation until inspection time, resolving forward references with zero startup latency.
5. **Standard Formats:** Standardize on Google or Sphinx docstring formatting for enterprise maintainability.

---

## 9. Frequently Asked Questions (Interview & Senior Roles)

<details>
<summary>1. Do type annotations impact runtime execution speed in Python?</summary>

In Python 3.13 and earlier, annotations were evaluated when the module loaded, adding a small import-time overhead. In Python 3.14+ (PEP 649), annotations are compiled into lazy code descriptors that are never executed unless explicitly inspected, resulting in zero import-time performance penalty. During function execution, annotations are completely ignored by the interpreter and carry zero runtime cost.
</details>

<details>
<summary>2. What is the difference between PEP 563 and PEP 649?</summary>

- **PEP 563 (`from __future__ import annotations`):** Converts all annotation expressions into string literals at compile time. This solves forward references, but breaks runtime tools (Pydantic, FastAPI) that require actual type objects, forcing them to call `eval()`.
- **PEP 649 (Python 3.14+):** Compiles annotations into a lazy descriptor function. Annotations remain real Python expressions evaluated only when inspected via `annotationlib`.
</details>

<details>
<summary>3. Why should you use `inspect.get_annotations(fn)` instead of `fn.__annotations__`?</summary>

Directly reading `fn.__annotations__` fails if the function was wrapped by decorators, if annotations were inherited, or if `from __future__ import annotations` stringified the types. `inspect.get_annotations()` handles unwrapping, string resolution, and backward compatibility automatically.
</details>

<details>
<summary>4. How does FastAPI utilize type annotations?</summary>

FastAPI introspects route function signatures using `inspect.get_annotations()`. It uses parameter type annotations (`item: Item`, `limit: int = 10`) to automatically parse request bodies, validate types via Pydantic, convert data types, and generate OpenAPI documentation schemas.
</details>

<details>
<summary>5. What is the difference between a comment and a docstring?</summary>

A comment (`# ...`) is completely stripped by the Python tokenizer during lexical analysis and does not exist in compiled bytecode. A docstring is a string literal stored in the code object's constants and attached to `fn.__doc__`, making it accessible at runtime.
</details>

<details>
<summary>6. Can a function have docstrings on individual arguments?</summary>

No. Python syntax does not support docstrings on parameters. Parameter documentation must be placed inside the main function docstring (using Google or Sphinx style formatting).
</details>

<details>
<summary>7. What happens if a function has an invalid type annotation at runtime?</summary>

In Python 3.13 and earlier without future annotations, an invalid expression (like `def f(x: undefined_type):`) raises a `NameError` immediately when the module is imported. Under Python 3.14+ (PEP 649), the definition succeeds, and the `NameError` is deferred until `annotationlib` is invoked.
</details>

<details>
<summary>8. How do docstrings behave in optimized bytecode (`python -OO`)?</summary>

When running Python with the `-OO` optimization flag, CPython strips all docstrings from code objects to reduce `.pyc` file size and memory footprint. `fn.__doc__` becomes `None`.
</details>

---

## 10. Practical Exercises & Self-Assessment

<details>
<summary>Exercise 1: Standardize a Legacy Docstring to Google Style</summary>

**Task:** Refactor the following docstring into standard Google style:
```python
def query_db(query, pool_size=10):
    # Runs query on pool
    # Returns rows list
    pass
```

**Solution:**
```python
def query_db(query: str, pool_size: int = 10) -> list[dict]:
    """Executes a SQL query against the connection pool.

    Args:
        query: The raw SQL query string to execute.
        pool_size: Maximum active connection pool size (default: 10).

    Returns:
        A list of dictionaries representing database record rows.
    """
    pass
```
</details>

<details>
<summary>Exercise 2: Annotation Coverage Auditor</summary>

**Task:** Write a function `check_annotation_coverage(fn)` that returns the percentage of parameters that have type annotations.

**Solution:**
```python
import inspect

def check_annotation_coverage(fn) -> float:
    sig = inspect.signature(fn)
    total = len(sig.parameters)
    if total == 0:
        return 100.0
    annotated = sum(1 for p in sig.parameters.values() if p.annotation is not inspect.Parameter.empty)
    return (annotated / total) * 100.0

def sample(a: int, b: str, c): pass
print(f"Coverage: {check_annotation_coverage(sample):.1f}%")  # 66.7%
```
</details>

<details>
<summary>Exercise 3: Safe Forward Reference Model</summary>

**Task:** Define a self-referential linked list node using `from __future__ import annotations` without string quoting.

**Solution:**
```python
from __future__ import annotations

class Node:
    def __init__(self, data: int, next_node: Node | None = None):
        self.data = data
        self.next_node = next_node

n1 = Node(1)
n2 = Node(2, n1)
assert n2.next_node.data == 1
```
</details>

<details>
<summary>Exercise 4: Auto-Generate Help Markdown</summary>

**Task:** Write a function `generate_markdown_doc(fn)` that outputs a markdown string containing the function name, signature, and docstring.

**Solution:**
```python
import inspect

def generate_markdown_doc(fn) -> str:
    sig = inspect.signature(fn)
    doc = fn.__doc__.strip() if fn.__doc__ else "No documentation provided."
    return f"### `{fn.__name__}{sig}`\n\n{doc}"

def add(x: int, y: int) -> int:
    """Adds two integers together."""
    return x + y

print(generate_markdown_doc(add))
```
</details>

<details>
<summary>Exercise 5: Inspect Python 3.14 Annotationlib Formats</summary>

**Task:** Demonstrate how `annotationlib` extracts annotations in `Format.VALUE` vs `Format.SOURCE` if running on Python 3.14+.

**Solution:**
```python
import sys

def example(x: int) -> str: return str(x)

if sys.version_info >= (3, 14):
    import annotationlib
    val_ann = annotationlib.get_annotations(example, format=annotationlib.Format.VALUE)
    src_ann = annotationlib.get_annotations(example, format=annotationlib.Format.SOURCE)
    print("Value format :", val_ann)
    print("Source format:", src_ann)
else:
    print("Run on Python 3.14+ to inspect annotationlib formats.")
```
</details>

<details>
<summary>Exercise 6: Enforce Return Type at Runtime</summary>

**Task:** Write a decorator `@enforce_return_type` that checks if the returned value matches the function's return annotation.

**Solution:**
```python
import functools
import inspect

def enforce_return_type(fn):
    sig = inspect.signature(fn)
    expected = sig.return_annotation
    
    @functools.wraps(fn)
    def wrapper(*args, **kwargs):
        res = fn(*args, **kwargs)
        if expected is not inspect.Signature.empty:
            if not isinstance(res, expected):
                raise TypeError(f"Return value expected {expected.__name__}, got {type(res).__name__}")
        return res
    return wrapper

@enforce_return_type
def greeting(name: str) -> str:
    return 123  # type: ignore

try:
    greeting("Shiva")
except TypeError as err:
    print("Caught return type error:", err)
```
</details>
