# Packages and __init__.py

- **Topic**: `06-modules-and-packages`
- **Python version**: 3.12+ (tested on CPython 3.14.0)
- **Tested with**: CPython 3.12, 3.13, 3.14 (standard & free-threaded builds)
- **Free-threading notes**: Compatible. Package initialization and namespace dictionary bindings adhere to standard thread-safe import locks.

---

## 1. Concept Overview

In Python, a **package** is simply a directory containing modules and an optional (historically mandatory) `__init__.py` file. Packages allow developers to structure module hierarchies into dot-separated namespaces (e.g. `urllib.request`, `email.mime.text`).

When a package is imported:
1. Python executes `__init__.py` inside the package directory.
2. The `__init__.py` file acts as the package's public API gateway: it controls what attributes are exposed when users write `from mypackage import *` (via `__all__`), pre-imports submodules for user convenience, and sets package-level metadata (such as `__version__`).

A directory **without** `__init__.py` is treated as a **Namespace Package (PEP 420)** (covered in detail in Note 03), which lacks a central initialization file and can span multiple filesystem locations.

```
project/
└── my_package/
    ├── __init__.py          <-- Executed when 'import my_package' runs
    ├── core.py              <-- Submodule: my_package.core
    └── utils.py             <-- Submodule: my_package.utils
```

---

## 2. Key Mechanics & Architecture

### The Role of `__init__.py`
1. **Marks Package Identity**: Formally designates the directory as a regular Python package.
2. **Namespace Binding**: Attributes defined in `__init__.py` become attributes of the package object itself (e.g. `my_package.__version__`).
3. **API Facade**: Submodules can be imported inside `__init__.py` to provide a clean, flattened public API:
   ```python
   # Inside my_package/__init__.py
   from .core import Engine
   from .utils import format_query
   __all__ = ["Engine", "format_query"]
   ```
4. **`__path__` Attribute**: Every package defines a `__path__` list attribute pointing to the filesystem directory where its submodules reside. Standard modules (`.py` files) do NOT have a `__path__` attribute.

---

## 3. Real-World Code Examples

### Example 1: Flattening Internal Architecture via `__init__.py`
```python
# Simulated package initialization logic
# my_framework/__init__.py
"""
from .client import APIClient
from .errors import APIError, RateLimitError

__all__ = ["APIClient", "APIError", "RateLimitError"]
__version__ = "1.2.0"
"""

# Callers can now do:
# from my_framework import APIClient, APIError
# Instead of:
# from my_framework.client import APIClient
```

### Example 2: Inspecting Package Attributes (`__path__`, `__file__`, `__package__`)
```python
import json
import math

# json is a package (directory with __init__.py)
print(f"json.__package__: {json.__package__}")
print(f"json is package (has __path__): {hasattr(json, '__path__')}")
print(f"json.__path__: {json.__path__}")

# math is a single C-module (not a package)
print(f"math is package (has __path__): {hasattr(math, '__path__')}")
```
Output:
```
json.__package__: json
json is package (has __path__): True
json.__path__: ['...\\Lib\\json']
math is package (has __path__): False
```

### Example 3: Controlling `from package import *` via `__all__`
```python
# Inside simulated package namespace
__all__ = ["public_function", "PublicClass"]

def public_function():
    return "public"

class PublicClass:
    pass

def _private_helper():
    return "private"

def unexported_function():
    return "not in __all__"

# When caller runs 'from pkg import *':
# Only 'public_function' and 'PublicClass' are injected into the caller's namespace!
```

### Example 4: Lazy Submodule Import in `__init__.py` via `__getattr__`
```python
# Modern Python 3.7+ allows defining __getattr__ at module/package level!
# This enables lazy-loading expensive submodules only when accessed.

def __getattr__(name: str):
    if name == "heavy_submodule":
        import importlib
        print("Lazy-loading heavy submodule...")
        mod = importlib.import_module(f"{__name__}.heavy_submodule")
        globals()[name] = mod
        return mod
    raise AttributeError(f"module {__name__!r} has no attribute {name!r}")
```

### Example 5: Deprecation Warnings for Package Attributes
```python
import warnings

def __getattr__(name: str):
    if name == "OldAPIClient":
        warnings.warn(
            "OldAPIClient is deprecated; use ModernAPIClient instead.",
            DeprecationWarning,
            stacklevel=2
        )
        # Return new client
        return ModernAPIClient
    raise AttributeError(f"module {__name__!r} has no attribute {name!r}")

class ModernAPIClient:
    pass
```

---

## 4. Common Anti-Patterns & Gotchas

| Bad Practice | Root Cause | Clean Solution |
| :--- | :--- | :--- |
| Heavy initialization logic or network calls inside `__init__.py` | Runs synchronously on every import, stalling startup time | Keep `__init__.py` lightweight; load resources lazily on first access |
| Missing `__all__` in `__init__.py` | `from pkg import *` dumps internal imports and helper variables | Define explicit `__all__ = [...]` list of exported symbols |
| Writing `from . import *` inside `__init__.py` | Obscures origins of symbols; risks name collisions | Import specific classes/functions explicitly: `from .sub import Target` |
| Modifying global state or monkey-patching in `__init__.py` | Causes spooky action at a distance for any consumer importing the package | Require explicit configuration calls (e.g. `pkg.configure()`) |
| Creating circular dependencies by importing submodules that import the parent package | Partially initialized parent package raises `ImportError: cannot import name` | Use absolute imports or import modules inside functions |
| Leaving empty `__init__.py` files when PEP 420 namespace packages are intended | Causes directory to be treated as a regular package instead of a namespace package | Delete `__init__.py` if multi-directory namespace packages are desired |

---

## 5. Interview Q&A

<details>
<summary>1. What is the fundamental difference between a module and a package in Python?</summary>
A module is a single Python file (or compiled C extension) that can be imported. A package is a special type of module that can contain submodules and subpackages. At runtime, a package is identified by having a <code>__path__</code> attribute (a list of filesystem directories searched for submodules), whereas a standard module has no <code>__path__</code>.
</details>

<details>
<summary>2. Is __init__.py strictly required in modern Python?</summary>
No. Since Python 3.3 (PEP 420), directories without <code>__init__.py</code> are recognized as <b>Implicit Namespace Packages</b>. However, <code>__init__.py</code> remains mandatory for <b>regular packages</b> where you need initialization code, API flattening, custom <code>__all__</code>, or module-level <code>__getattr__</code>.
</details>

<details>
<summary>3. What does __all__ define in a package's __init__.py?</summary>
<code>__all__</code> is a sequence of strings defining the public interface of the package. When a user runs <code>from package import *</code>, only the names listed in <code>__all__</code> are imported into the caller's namespace. It also guides linters, documentation generators (Sphinx), and IDE autocomplete.
</details>

<details>
<summary>4. How does PEP 562 (module-level __getattr__) benefit __init__.py?</summary>
PEP 562 allows defining <code>__getattr__(name: str)</code> and <code>__dir__()</code> directly at module/package scope. In <code>__init__.py</code>, this enables:
1. <b>Lazy Loading</b>: Submodules are only imported when a user accesses <code>pkg.heavy_module</code>.
2. <b>Deprecation Warnings</b>: Issuing warnings when deprecated attributes or classes are accessed.
</details>

<details>
<summary>5. What does the __file__ attribute indicate, and when is it None?</summary>
<code>__file__</code> indicates the filesystem path from which the module or package was loaded. In namespace packages (which have no single source file) or modules loaded from C code / memory without filesystem backing, <code>__file__</code> may be absent or <code>None</code>.
</details>

<details>
<summary>6. Why is putting heavy database connections in __init__.py an anti-pattern?</summary>
Every time any submodule of the package is imported (e.g. <code>import mypkg.utils</code>), Python must first execute <code>mypkg/__init__.py</code>. Running database connections, file I/O, or thread spawns in <code>__init__.py</code> slows down test execution, CLI commands, and toolchain processes.
</details>

<details>
<summary>7. How does Python find submodules of a package?</summary>
When importing <code>pkg.sub</code>, Python looks at <code>pkg.__path__</code> (a list of directory paths). The <code>PathFinder</code> scans those directories specifically for <code>sub.py</code>, <code>sub/__init__.py</code>, or compiled extensions.
</details>

<details>
<summary>8. Can __init__.py define submodules dynamically?</summary>
Yes. Using <code>types.ModuleType</code> and <code>sys.modules</code>, <code>__init__.py</code> can programmatically construct and inject virtual submodules into its package namespace at runtime.
</details>

---

## 6. Practical Challenges

<details>
<summary>Challenge 1: Flattened Package Facade</summary>

Construct an in-memory package `mockpkg` with `core` and `utils` submodules, using `__all__` in `mockpkg` to export only `RunService` and `VERSION`.

```python
import sys
import types

pkg = types.ModuleType("mockpkg")
core = types.ModuleType("mockpkg.core")
core.RunService = lambda: "running"
core.InternalSecret = 42

pkg.RunService = core.RunService
pkg.VERSION = "1.0.0"
pkg.__all__ = ["RunService", "VERSION"]

sys.modules["mockpkg"] = pkg
sys.modules["mockpkg.core"] = core

assert "RunService" in pkg.__all__
assert "InternalSecret" not in pkg.__all__
```
</details>

<details>
<summary>Challenge 2: Module-Level Lazy Submodule Loader</summary>

Implement a package `__getattr__` that dynamically imports and caches a submodule on first attribute access.

```python
import types
import importlib

dummy_pkg = types.ModuleType("lazy_pkg")

def lazy_getattr(name: str):
    if name == "math":
        mod = importlib.import_module("math")
        dummy_pkg.__dict__[name] = mod
        return mod
    raise AttributeError(name)

dummy_pkg.__getattr__ = lazy_getattr
assert dummy_pkg.math.sqrt(9) == 3.0
```
</details>

<details>
<summary>Challenge 3: Deprecated Attribute Warning Hook</summary>

Implement a package `__getattr__` that raises a `DeprecationWarning` when accessing `legacy_calc(x)` and forwards to `modern_calc(x)`.

```python
import warnings
import types

mod = types.ModuleType("dep_mod")
mod.modern_calc = lambda x: x * 2

def mod_getattr(name):
    if name == "legacy_calc":
        warnings.warn("legacy_calc is deprecated", DeprecationWarning, stacklevel=2)
        return mod.modern_calc
    raise AttributeError(name)

mod.__getattr__ = mod_getattr

with warnings.catch_warnings(record=True) as w:
    warnings.simplefilter("always")
    res = mod.legacy_calc(5)
    assert res == 10
    assert len(w) == 1
    assert issubclass(w[-1].category, DeprecationWarning)
```
</details>

<details>
<summary>Challenge 4: Verify Regular vs Namespace Package</summary>

Write a function `is_regular_package(mod)` that returns `True` if `mod` is a regular package with an `__init__.py` file, and `False` otherwise.

```python
from pathlib import Path

def is_regular_package(mod) -> bool:
    if not hasattr(mod, "__path__") or not hasattr(mod, "__file__"):
        return False
    if mod.__file__ is None:
        return False
    return Path(mod.__file__).name in ("__init__.py", "__init__.pyc")

import json
assert is_regular_package(json) is True
```
</details>

<details>
<summary>Challenge 5: Export Cleaner with __all__ Validation</summary>

Write a test validator `validate_exports(module)` that verifies every name listed in `module.__all__` actually exists in `module.__dict__`.

```python
def validate_exports(mod) -> bool:
    if not hasattr(mod, "__all__"):
        return True
    return all(name in mod.__dict__ for name in mod.__all__)

import json
assert validate_exports(json) is True
```
</details>

<details>
<summary>Challenge 6: Programmatic Submodule Registration</summary>

Programmatically inject a virtual submodule `mypkg.helpers` into `sys.modules` and link it to `mypkg` via `mypkg.helpers = helpers_mod`.

```python
import sys
import types

mypkg = types.ModuleType("mypkg")
mypkg.__path__ = []
helpers = types.ModuleType("mypkg.helpers")
helpers.ping = lambda: "pong"

sys.modules["mypkg"] = mypkg
sys.modules["mypkg.helpers"] = helpers
mypkg.helpers = helpers

assert sys.modules["mypkg.helpers"].ping() == "pong"
assert mypkg.helpers.ping() == "pong"
```
</details>
