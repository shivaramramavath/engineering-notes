# Relative vs Absolute Imports

- **Topic**: `06-modules-and-packages`
- **Python version**: 3.12+ (tested on CPython 3.14.0)
- **Tested with**: CPython 3.12, 3.13, 3.14 (standard & free-threaded builds)
- **Free-threading notes**: Compatible. Import resolution uses immutable `__package__` and `__name__` strings, thread-safe across interpreters.

---

## 1. Concept Overview

Python supports two distinct syntaxes for importing modules within a codebase:

1. **Absolute Imports**: Specifies the full, unambiguous import path starting from the project's root search directory (as configured in `sys.path`).
   ```python
   from my_package.services.auth import AuthService
   import my_package.utils.crypto
   ```
2. **Relative Imports**: Uses dot notation (`.` and `..`) to reference modules relative to the current module's position within a package hierarchy.
   ```python
   from .models import User          # Sibling module
   from ..database import get_db     # Parent package
   from ...config import SETTINGS    # Grandparent package
   ```

A pervasive trap for Python developers is attempting to run a script containing relative imports directly from the command line (`python path/to/module.py`), which raises:
```
ImportError: attempted relative import with no known parent package
```
Understanding how Python determines package context via `__name__` and `__package__` eliminates this confusion.

```
Package Hierarchy:
my_project/
└── my_package/
    ├── __init__.py
    ├── core.py               <-- from .database import db (Relative sibling)
    ├── database.py
    └── subpkg/
        ├── __init__.py
        └── service.py        <-- from ..core import Engine (Relative parent)
```

---

## 2. Key Mechanics & Architecture

### How Python Resolves Relative Imports
Relative imports do not search `sys.path`. Instead, they compute the target module name based on the calling module's **`__package__`** attribute (or `__name__` if `__package__` is `None`):
1. **Single Dot (`.`)**: Current package (directory enclosing the current module).
2. **Double Dot (`..`)**: Parent package (one level up).
3. **Triple Dot (`...`)**: Grandparent package (two levels up).

### Why Running a File Directly Breaks Relative Imports
When you run a file directly with `python my_package/core.py`:
- Python sets `__name__ = "__main__"`.
- Python sets `__package__ = None` (or `""`).
- Because Python does not know what package `core.py` belongs to, any relative import (like `from .database import db`) fails immediately with `ImportError: attempted relative import with no known parent package`.

### The Solution: Run as a Module via `-m`
Always execute scripts located inside packages as modules using the `-m` flag from the repository root:
```bash
python -m my_package.core
```
This preserves `__package__ = "my_package"` and allows relative imports to resolve seamlessly!

---

## 3. Real-World Code Examples

### Example 1: Absolute vs Relative Import Syntax
```python
# Absolute import (Recommended for readability and refactoring clarity):
# from app.billing.invoices import generate_invoice

# Relative sibling import:
# from .invoices import generate_invoice

# Relative parent import:
# from ..models.user import User

# Relative grandparent import:
# from ...config import DB_CONFIG
```

### Example 2: Inspecting `__name__` and `__package__`
```python
import sys

print(f"Current __name__: {__name__}")
print(f"Current __package__: {__package__}")

# When imported as a module inside a package:
# __name__ == "pkg.sub.module"
# __package__ == "pkg.sub"

# When run as top-level script:
# __name__ == "__main__"
# __package__ is None or ""
```
Output:
```
Current __name__: __main__
Current __package__: None
```

### Example 3: Simulating Relative Import Resolution Algorithm
```python
def resolve_relative_import(calling_module_name: str, level: int, target_name: str) -> str:
    """Simulates how Python resolves 'from .target import ...'"""
    parts = calling_module_name.split(".")
    if level > len(parts):
        raise ImportError("attempted relative import beyond top-level package")
    parent_parts = parts[:-level]
    if target_name:
        return ".".join(parent_parts + [target_name])
    return ".".join(parent_parts)


# In module 'my_app.billing.services'
# from .models import Order  -> level=1
print(resolve_relative_import("my_app.billing.services", 1, "models"))

# from ..database import db   -> level=2
print(resolve_relative_import("my_app.billing.services", 2, "database"))

# from ...config import cfg   -> level=3
print(resolve_relative_import("my_app.billing.services", 3, "config"))
```
Output:
```
my_app.billing.models
my_app.database
my_app.config
```

### Example 4: The Beyond Top-Level Package Trap
```python
# Attempting to go up more levels than the package hierarchy contains:
try:
    resolve_relative_import("my_app.core", 3, "config")
except ImportError as err:
    print(f"Caught invalid relative import: {err}")
```
Output:
```
Caught invalid relative import: attempted relative import beyond top-level package
```

### Example 5: PEP 8 Style Guide Recommendations
PEP 8 explicitly states:
> *"Absolute imports are recommended, as they are usually more readable and tend to be better behaved (or at least give better error messages) if the import system is incorrectly configured... However, explicit relative imports are an acceptable alternative to absolute imports, especially when dealing with complex package layouts."*

---

## 4. Common Anti-Patterns & Gotchas

| Bad Practice | Root Cause | Clean Solution |
| :--- | :--- | :--- |
| Running `python package/sub/script.py` directly | Sets `__package__ = None`, breaking relative imports | Run from root using `python -m package.sub.script` |
| Using implicit relative imports: `import sibling` | Removed in Python 3; searches `sys.path` instead of sibling | Use explicit relative (`from . import sibling`) or absolute |
| Relative imports beyond the top-level package (`....root`) | Reaches outside the package root | Use absolute imports for top-level modules |
| Modifying `sys.path.append("..")` to fix relative import errors | Fragile hack that pollutes global interpreter state | Structure code cleanly as an installable package (`pip install -e .` or `uv`) |
| Bare `import .module` syntax | Python syntax error; `import .foo` is invalid syntax | Must use `from . import foo` or `from .foo import bar` |
| Deep relative paths (`from .....util import x`) | Highly fragile; breaking on any folder reorganization | Switch to absolute imports: `from my_app.util import x` |

---

## 5. Interview Q&A

<details>
<summary>1. What is the fundamental difference between relative and absolute imports?</summary>
Absolute imports specify the full path to a module starting from a root directory present on <code>sys.path</code> (e.g. <code>import app.models.user</code>). Relative imports use leading dots (<code>.</code>, <code>..</code>) to locate modules relative to the current module's position within a package without consulting <code>sys.path</code>.
</details>

<details>
<summary>2. Why does running python package/module.py fail with "attempted relative import with no known parent package"?</summary>
When a Python file is run directly as a script, CPython sets its <code>__name__ = "__main__"</code> and sets <code>__package__ = None</code>. Because relative imports calculate the target path by inspecting <code>__package__</code>, the absence of a known parent package causes Python to raise an <code>ImportError</code> immediately.
</details>

<details>
<summary>3. How does the -m CLI flag fix relative import failures?</summary>
Running <code>python -m package.module</code> instructs Python to load the module through the standard import system rather than executing the file directly. This sets <code>__name__ = "__main__"</code> while preserving <code>__package__ = "package"</code>, enabling all relative imports within the module to resolve correctly.
</details>

<details>
<summary>4. What happened to implicit relative imports in Python 3?</summary>
In Python 2, writing <code>import utils</code> inside <code>package/foo.py</code> would first search for <code>package/utils.py</code> before searching the standard library. In Python 3 (PEP 328), implicit relative imports were completely removed. All relative imports must be explicit (e.g. <code>from . import utils</code>).
</details>

<details>
<summary>5. What does PEP 8 recommend regarding relative vs absolute imports?</summary>
PEP 8 recommends absolute imports because they are more readable, make code origins obvious at a glance, and provide clear error messages. However, explicit relative imports are accepted for internal package cohesion to avoid hardcoding the top-level package name.
</details>

<details>
<summary>6. Can you write import .foo or import ..bar?</summary>
No. The syntax <code>import .foo</code> is a syntax error in Python. Relative imports must always use the <code>from ... import ...</code> form (e.g. <code>from . import foo</code> or <code>from .foo import Bar</code>).
</details>

<details>
<summary>7. What is __spec__.parent in modern Python?</summary>
In Python 3.6+, module specifications (<code>ModuleSpec</code>) provide <code>__spec__.parent</code>, which Python uses as the fallback for <code>__package__</code> when determining the parent package during relative import resolution.
</details>

<details>
<summary>8. What error is raised when relative import steps outside the package root?</summary>
Python raises <code>ImportError: attempted relative import beyond top-level package</code>.
</details>

---

## 6. Practical Challenges

<details>
<summary>Challenge 1: Safe Relative Import Resolver</summary>

Write a utility `resolve_import(calling_pkg: str, dot_string: str) -> str` that converts relative import syntax (like `"..utils"`) into its canonical absolute import path.

```python
def resolve_import(calling_pkg: str, rel_path: str) -> str:
    dots = len(rel_path) - len(rel_path.lstrip("."))
    target = rel_path.lstrip(".")
    parts = calling_pkg.split(".")
    if dots > len(parts):
        raise ImportError("attempted relative import beyond top-level package")
    base = parts[:len(parts) - (dots - 1)]
    return ".".join(base + ([target] if target else []))

assert resolve_import("app.billing.services", ".models") == "app.billing.services.models"
assert resolve_import("app.billing.services", "..db") == "app.billing.db"
```
</details>

<details>
<summary>Challenge 2: Detect Running as Script vs Module</summary>

Write a guard snippet that detects whether a file is being run directly without package context and displays instructions to run with `-m`.

```python
def assert_package_context():
    if __name__ == "__main__" and not __package__:
        return "RUN_VIA_MODULE_FLAG_REQUIRED"
    return "OK"

assert assert_package_context() == "RUN_VIA_MODULE_FLAG_REQUIRED"
```
</details>

<details>
<summary>Challenge 3: Beyond Top-Level Guard</summary>

Write a test verifying that importing 3 levels up in a 2-level package raises `ImportError`.

```python
def test_beyond_top():
    try:
        resolve_import("app.core", "...config")
        assert False, "Should raise ImportError"
    except ImportError as e:
        assert "beyond top-level" in str(e)

test_beyond_top()
```
</details>

<details>
<summary>Challenge 4: Programmatic Module Injection with Package Context</summary>

Create an in-memory module with `__package__ = "mypkg.sub"` and verify relative import resolution works.

```python
import types

mod = types.ModuleType("mypkg.sub.worker")
mod.__package__ = "mypkg.sub"
assert mod.__package__ == "mypkg.sub"
```
</details>

<details>
<summary>Challenge 5: Extract Package Root from Dotted Name</summary>

Implement `get_package_root(module_name: str) -> str` that extracts the top-level package name.

```python
def get_package_root(module_name: str) -> str:
    return module_name.split(".")[0]

assert get_package_root("urllib.request") == "urllib"
assert get_package_root("os") == "os"
```
</details>

<details>
<summary>Challenge 6: Relative Import Syntax Parser</summary>

Write a regex or parser that extracts the dot count and target name from strings like `from ...core import Client`.

```python
import re

def parse_relative_syntax(statement: str) -> tuple[int, str]:
    match = re.match(r"from\s+(\.+)([a-zA-Z0-9_]*)\s+import", statement)
    if not match:
        raise ValueError("Invalid relative import statement")
    dots, target = match.groups()
    return len(dots), target

assert parse_relative_syntax("from ..services import Auth") == (2, "services")
assert parse_relative_syntax("from . import config") == (1, "")
```
</details>
