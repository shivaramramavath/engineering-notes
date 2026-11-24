# Namespace Packages: PEP 420

- **Topic**: `06-modules-and-packages`
- **Python version**: 3.12+ (tested on CPython 3.14.0)
- **Tested with**: CPython 3.12, 3.13, 3.14 (standard & free-threaded builds)
- **Free-threading notes**: Compatible. Dynamic `_NamespacePath` traversal is thread-safe under CPython's import subsystem.

---

## 1. Concept Overview

Before Python 3.3, every directory in a package hierarchy was required to contain an `__init__.py` file. This made it impossible to split a single logical package (e.g. `acme.analytics`, `acme.auth`, `acme.payments`) across multiple separate repositories or independently installable wheels without complex `pkgutil` or `pkg_resources` monkey-patching.

**PEP 420** introduced **Implicit Namespace Packages** in Python 3.3.

Under PEP 420:
- Any directory on `sys.path` that does **not** contain an `__init__.py` file is automatically recognized as a **namespace package**.
- A namespace package has **no single physical location**: its `__path__` attribute is a dynamic `_NamespacePath` object that continuously aggregates all matching directory portions found across every entry in `sys.path`.
- Independent repositories or packages can contribute submodules to the same top-level namespace (e.g. `company_name`) without colliding or overwriting each other's files.

```
sys.path entry 1 (site-packages/repo_a/):
└── acme/
    └── auth.py              <-- acme.auth

sys.path entry 2 (site-packages/repo_b/):
└── acme/
    └── billing.py           <-- acme.billing

Combined at runtime:
import acme.auth
import acme.billing
acme.__path__ = ['.../repo_a/acme', '.../repo_b/acme']
```

---

## 2. Key Mechanics & Architecture

### Regular Packages vs Namespace Packages

| Dimension | Regular Package | PEP 420 Namespace Package |
| :--- | :--- | :--- |
| **`__init__.py`** | **Required** | **Must be absent** |
| **`__file__` attribute** | Path to `__init__.py` | `None` (no file represents the package) |
| **`__path__` attribute** | Plain `list` of paths | Dynamic iterable `_NamespacePath` |
| **Multi-Directory Aggregation** | No (first match wins, others ignored) | **Yes** (aggregates all matching directories on `sys.path`) |
| **Package-Level Code** | Runs `__init__.py` at import | **No package code can execute** on import |
| **Legacy Alternatives** | `pkgutil.extend_path()`, `pkg_resources` | Native language mechanism (zero boilerplate) |

### The `_NamespacePath` Object
When a namespace package is imported, its `__path__` attribute is not a static Python list. It is an instance of `_NamespacePath`. If `sys.path` is updated dynamically at runtime (e.g. a new directory or plugin path is added), `_NamespacePath` dynamically reflects the new locations without requiring re-import!

---

## 3. Real-World Code Examples

### Example 1: Simulating Split Directory Namespace Package
```python
import sys
import tempfile
import pathlib

# Create two independent directories representing separate repos
temp_dir1 = tempfile.TemporaryDirectory()
temp_dir2 = tempfile.TemporaryDirectory()

dir1 = pathlib.Path(temp_dir1.name)
dir2 = pathlib.Path(temp_dir2.name)

# Repo 1 defines: my_corp/auth.py (NO __init__.py!)
pkg1 = dir1 / "my_corp"
pkg1.mkdir()
(pkg1 / "auth.py").write_text("SERVICE = 'OAuth2'\n")

# Repo 2 defines: my_corp/billing.py (NO __init__.py!)
pkg2 = dir2 / "my_corp"
pkg2.mkdir()
(pkg2 / "billing.py").write_text("CURRENCY = 'USD'\n")

# Add both separate directories to sys.path
sys.path.insert(0, str(dir1))
sys.path.insert(1, str(dir2))

# Import both submodules through the unified namespace!
import my_corp.auth
import my_corp.billing

print(f"Auth Service: {my_corp.auth.SERVICE}")
print(f"Billing Currency: {my_corp.billing.CURRENCY}")
print(f"my_corp.__file__: {my_corp.__file__}")
print(f"my_corp.__path__: {list(my_corp.__path__)}")

# Cleanup
temp_dir1.cleanup()
temp_dir2.cleanup()
sys.path.pop(0)
sys.path.pop(0)
```
Output:
```
Auth Service: OAuth2
Billing Currency: USD
my_corp.__file__: None
my_corp.__path__: ['...\\my_corp', '...\\my_corp']
```

### Example 2: Inspecting `_NamespacePath` Type
```python
import sys

# Import a standard namespace package or inspect its behavior
import importlib.util

spec = importlib.util.find_spec("my_corp")
if spec:
    print(f"Spec loader: {spec.loader}")
    print(f"Spec submodule search locations: {spec.submodule_search_locations}")
```

### Example 3: Detecting Namespace Package at Runtime
```python
def is_namespace_package(module) -> bool:
    """Returns True if the given module is a PEP 420 namespace package."""
    return (
        hasattr(module, "__path__")
        and getattr(module, "__file__", None) is None
    )


import json
print(f"Is json a namespace package? {is_namespace_package(json)}")
```
Output:
```
Is json a namespace package? False
```

### Example 4: Legacy `pkgutil.extend_path` vs PEP 420
```python
# In Python 2 / early Python 3, namespace packages required __init__.py with:
"""
from pkgutil import extend_path
__path__ = extend_path(__path__, __name__)
"""
# In modern Python 3.3+, simply DELETE __init__.py entirely.
# PEP 420 handles aggregation automatically at the C-level import system.
```

### Example 5: Dynamic `sys.path` Expansion with `_NamespacePath`
```python
import sys
import tempfile
import pathlib

temp_dir3 = tempfile.TemporaryDirectory()
dir3 = pathlib.Path(temp_dir3.name)
pkg3 = dir3 / "my_corp"
pkg3.mkdir()
(pkg3 / "analytics.py").write_text("METRIC = 'latency'\n")

# Add dir3 to sys.path *after* my_corp was already imported!
sys.path.insert(0, str(dir3))

# Because my_corp.__path__ is a dynamic _NamespacePath,
# it immediately finds the new submodule without reloading!
import my_corp.analytics
print(f"Analytics Metric: {my_corp.analytics.METRIC}")

temp_dir3.cleanup()
sys.path.pop(0)
```
Output:
```
Analytics Metric: latency
```

---

## 4. Common Anti-Patterns & Gotchas

| Bad Practice | Root Cause | Clean Solution |
| :--- | :--- | :--- |
| Accidentally creating an empty `__init__.py` in a namespace package | Converts the directory into a regular package, masking all other portions on `sys.path` | Ensure no `__init__.py` exists in any portion of a namespace package |
| Expecting `__init__.py` code to execute in a namespace package | Namespace packages cannot have `__init__.py` by definition | Place shared setup logic in a concrete submodule like `namespace.common` |
| Checking `module.__file__` on a namespace package | Namespace packages have `__file__ = None`, causing `AttributeError` or `TypeError` | Check `getattr(mod, "__file__", None)` safely |
| Using legacy `pkgutil.extend_path` or `pkg_resources` in modern projects | Outdated, slow, and incompatible with pure PEP 420 namespace packages | Adopt pure PEP 420 implicit namespace packages (no `__init__.py`) |
| Expecting `from namespace import *` to work out of the box | Namespace packages have no `__init__.py` defining `__all__` | Import explicit submodules: `import namespace.submodule` |
| Packaging namespace packages with setuptools without `find_namespace_packages` | Older tools fail to discover packages without `__init__.py` | Use `find_namespace_packages()` in `setup.py` or `pyproject.toml` |

---

## 5. Interview Q&A

<details>
<summary>1. What is a PEP 420 Namespace Package?</summary>
Introduced in Python 3.3, a PEP 420 Namespace Package is a package represented by directories on <code>sys.path</code> that contain NO <code>__init__.py</code> file. It allows a single logical package namespace to be split across multiple independent directories, repositories, and distribution wheels.
</details>

<details>
<summary>2. What happens if one directory in a namespace package accidentally contains an __init__.py?</summary>
Python treats that directory as a <b>regular package</b>. As soon as the regular package is found on <code>sys.path</code>, Python stops searching further entries on <code>sys.path</code> for that package name. All other directories contributing to the namespace package become completely invisible (masked), breaking imports from other wheels.
</details>

<details>
<summary>3. What is the value of __file__ for a namespace package?</summary>
For a PEP 420 namespace package, <code>__file__</code> is <code>None</code> (or not set) because there is no physical file (like <code>__init__.py</code>) that defines the package itself.
</details>

<details>
<summary>4. What type is a namespace package's __path__ attribute?</summary>
In a regular package, <code>__path__</code> is a standard Python <code>list</code> of strings. In a namespace package, <code>__path__</code> is an instance of <code>_NamespacePath</code>, an internal iterable that dynamically re-computes matching directories whenever <code>sys.path</code> changes.
</details>

<details>
<summary>5. Can a namespace package execute initialization code when imported?</summary>
No. Because namespace packages do not have an <code>__init__.py</code> file, importing a top-level namespace package runs zero executable Python code—it merely creates an empty module object with a <code>_NamespacePath</code>.
</details>

<details>
<summary>6. How does setuptools or wheel support namespace packages?</summary>
Modern build tools like Hatch, Flit, and Setuptools natively support PEP 420. In Setuptools, using <code>find_namespace_packages(include=["mycorp.*"])</code> in <code>pyproject.toml</code> instructs the packager to include directories lacking <code>__init__.py</code>.
</details>

<details>
<summary>7. What was the legacy way to create namespace packages before Python 3.3?</summary>
Prior to Python 3.3, developers had to use <code>pkgutil.extend_path(__path__, __name__)</code> or <code>pkg_resources.declare_namespace(__name__)</code> inside an explicit <code>__init__.py</code> file in every distributed portion of the package.
</details>

<details>
<summary>8. How do you distinguish between a missing module and a namespace package without submodules?</summary>
Using <code>importlib.util.find_spec(name)</code>: if the name corresponds to a namespace package, <code>spec.submodule_search_locations</code> is a <code>_NamespacePath</code>, and <code>spec.origin</code> is <code>None</code>. If the module does not exist, <code>find_spec()</code> returns <code>None</code>.
</details>

---

## 6. Practical Challenges

<details>
<summary>Challenge 1: Runtime Namespace Detector</summary>

Write a function `inspect_package_type(mod_or_name)` that classifies a module as `"namespace_package"`, `"regular_package"`, or `"single_file_module"`.

```python
import importlib
import importlib.util

def inspect_package_type(target) -> str:
    if isinstance(target, str):
        target = importlib.import_module(target)
    if not hasattr(target, "__path__"):
        return "single_file_module"
    if getattr(target, "__file__", None) is None:
        return "namespace_package"
    return "regular_package"

import json
import math
assert inspect_package_type(json) == "regular_package"
assert inspect_package_type(math) == "single_file_module"
```
</details>

<details>
<summary>Challenge 2: Multi-Directory Namespace Aggregator Test</summary>

Simulate two temporary directories contributing to `cloud_tools` and verify that `list(cloud_tools.__path__)` contains both directory paths.

```python
import sys
import tempfile
import pathlib

with tempfile.TemporaryDirectory() as d1, tempfile.TemporaryDirectory() as d2:
    p1 = pathlib.Path(d1) / "cloud_tools"
    p2 = pathlib.Path(d2) / "cloud_tools"
    p1.mkdir()
    p2.mkdir()
    (p1 / "aws.py").write_text("PROVIDER = 'aws'\n")
    (p2 / "gcp.py").write_text("PROVIDER = 'gcp'\n")

    sys.path.insert(0, d1)
    sys.path.insert(1, d2)
    try:
        import cloud_tools.aws
        import cloud_tools.gcp
        assert cloud_tools.aws.PROVIDER == "aws"
        assert cloud_tools.gcp.PROVIDER == "gcp"
        assert len(list(cloud_tools.__path__)) == 2
    finally:
        sys.path.pop(0)
        sys.path.pop(0)
```
</details>

<details>
<summary>Challenge 3: Masking Detection Utility</summary>

Write a test that verifies an `__init__.py` masks secondary directories on `sys.path`.

```python
with tempfile.TemporaryDirectory() as d1, tempfile.TemporaryDirectory() as d2:
    p1 = pathlib.Path(d1) / "masked_pkg"
    p2 = pathlib.Path(d2) / "masked_pkg"
    p1.mkdir()
    p2.mkdir()
    # d1 has __init__.py -> regular package!
    (p1 / "__init__.py").write_text("# Regular package\n")
    (p2 / "secondary.py").write_text("OK = True\n")

    sys.path.insert(0, d1)
    sys.path.insert(1, d2)
    try:
        import masked_pkg
        # Attempting to import secondary should fail because d1 is a regular package
        try:
            import masked_pkg.secondary
            assert False, "Should fail because regular package does not search d2"
        except ModuleNotFoundError:
            pass
    finally:
        sys.path.pop(0)
        sys.path.pop(0)
```
</details>

<details>
<summary>Challenge 4: Spec Inspector for Namespace Packages</summary>

Write a function `is_namespace_spec(name: str) -> bool` using `importlib.util.find_spec` that checks if a module is a namespace package without importing it.

```python
import importlib.util

def is_namespace_spec(name: str) -> bool:
    spec = importlib.util.find_spec(name)
    if spec is None:
        return False
    return (
        spec.submodule_search_locations is not None
        and spec.origin is None
    )
```
</details>

<details>
<summary>Challenge 5: Extract All Contributed Paths for Namespace</summary>

Given an imported namespace package, extract all contributing filesystem directories as `pathlib.Path` objects.

```python
from pathlib import Path

def get_namespace_contributing_paths(pkg) -> list[Path]:
    if getattr(pkg, "__file__", None) is not None:
        raise ValueError("Not a namespace package")
    return [Path(p) for p in pkg.__path__]
```
</details>

<details>
<summary>Challenge 6: Programmatic Namespace Module Synthesis</summary>

Synthesize an in-memory namespace package using `types.ModuleType` with `__file__ = None` and a custom list for `__path__`.

```python
import types

ns_mod = types.ModuleType("virtual_ns")
ns_mod.__file__ = None
ns_mod.__path__ = ["/virtual/path1", "/virtual/path2"]

assert getattr(ns_mod, "__file__") is None
assert len(ns_mod.__path__) == 2
```
</details>
