# Import System Mechanics and sys.modules

- **Topic**: `06-modules-and-packages`
- **Python version**: 3.12+ (tested on CPython 3.14.0)
- **Tested with**: CPython 3.12, 3.13, 3.14 (standard & free-threaded builds)
- **Free-threading notes**: Compatible. `sys.modules` lookups and writes are protected by an internal per-interpreter lock in CPython to ensure concurrent import safety.

---

## 1. Concept Overview

In Python, every `.py` file is a **module**, and importing a module is a dynamic runtime execution step, not a compile-time static inclusion.

When you execute `import foo`, Python does not re-read or re-parse `foo.py` every time. Instead, it follows a strict sequence:
1. **Cache Lookup**: Python checks `sys.modules` (a global dictionary mapping module names to module objects). If `foo` is present, it returns the cached module object immediately.
2. **Finders & Loaders (PEP 451)**: If `foo` is absent from `sys.modules`, Python queries registered meta path finders (`sys.meta_path`) to locate the module's specification (`ModuleSpec`).
3. **Execution & Caching**: Python allocates a new module object, inserts it into `sys.modules` *before* executing the code (to handle circular imports), executes the module body within the module's namespace, and returns the module.

Understanding `sys.modules`, `sys.path`, and the finder/loader protocol is critical for writing robust plugins, dynamic import loaders, and resolving circular dependencies.

```
                      import foo
                          |
                          v
                 Is 'foo' in sys.modules?
                       /        \
                     YES         NO
                     /            \
           Return cached module    Traverse sys.meta_path finders
                                   (BuiltinFinder, FrozenFinder, PathFinder)
                                          |
                                          v
                                    Found ModuleSpec?
                                       /         \
                                     YES          NO
                                     /             \
                      Create empty module        Raise ModuleNotFoundError
                      Insert into sys.modules
                      Execute module code
                      Return module
```

---

## 2. Key Mechanics & Architecture

### The Core Subsystems
- **`sys.modules`**: A live Python `dict` caching all loaded modules. Mutating this dict directly (e.g. `del sys.modules["foo"]`) allows unloading or mocking modules.
- **`sys.path`**: A list of strings specifying search directories for modules. Initialized from:
  1. The directory of the input script (or current directory `""`).
  2. The `PYTHONPATH` environment variable.
  3. Standard library directories.
  4. Site-packages directories (via `site` module).
- **`sys.meta_path`**: A list of meta path finder objects searched before `sys.path`. Includes:
  - `BuiltinImporter` (for C-level built-in modules like `sys`, `time`).
  - `FrozenImporter` (for frozen bytecode modules).
  - `PathFinder` (searches `sys.path` for filesystem files and zip archives).

---

## 3. Real-World Code Examples

### Example 1: Inspecting `sys.modules` Caching
```python
import sys
import math

print(f"Is 'math' in sys.modules? {'math' in sys.modules}")
first_id = id(sys.modules["math"])

# Re-importing math does NOT re-execute; returns the identical object
import math as math_again
second_id = id(math_again)

print(f"first_id == second_id: {first_id == second_id}")
print(f"Module file location: {sys.modules['math']}")
```
Output:
```
Is 'math' in sys.modules? True
first_id == second_id: True
Module file location: <module 'math' (built-in)>
```

### Example 2: Dynamic Import with `importlib.import_module`
```python
import importlib

def load_serializer(backend_name: str):
    """Dynamically imports a serializer module by string name."""
    try:
        module = importlib.import_module(backend_name)
        return module
    except ModuleNotFoundError as e:
        print(f"Failed to load backend {backend_name}: {e}")
        return None


json_mod = load_serializer("json")
if json_mod:
    data = json_mod.dumps({"status": "active"})
    print(f"Serialized via dynamic import: {data}")

invalid_mod = load_serializer("non_existent_package")
```
Output:
```
Serialized via dynamic import: {"status": "active"}
Failed to load backend non_existent_package: No module named 'non_existent_package'
```

### Example 3: Inspecting `ModuleSpec` with `importlib.util`
```python
import importlib.util

spec = importlib.util.find_spec("json")
print(f"Spec Name: {spec.name}")
print(f"Spec Origin: {spec.origin}")
print(f"Spec Loader: {spec.loader.__class__.__name__}")
print(f"Is Package: {spec.submodule_search_locations is not None}")
```
Output:
```
Spec Name: json
Spec Origin: ...\Lib\json\__init__.py
Spec Loader: SourceFileLoader
Is Package: True
```

### Example 4: Hot Reloading a Module with `importlib.reload`
```python
import importlib
import types

# Create a temporary in-memory module
dummy = types.ModuleType("temp_module")
dummy.version = "1.0.0"
import sys
sys.modules["temp_module"] = dummy

print(f"Initial version: {sys.modules['temp_module'].version}")

# Mutate underlying state and reload
dummy.version = "2.0.0"
importlib.reload(dummy)
print(f"Version after reload: {sys.modules['temp_module'].version}")

# Cleanup
del sys.modules["temp_module"]
```
Output:
```
Initial version: 1.0.0
Version after reload: 2.0.0
```

### Example 5: Custom Virtual In-Memory MetaPathFinder
```python
import sys
import types
from importlib.abc import MetaPathFinder, Loader
from importlib.machinery import ModuleSpec

class VirtualMemoryLoader(Loader):
    def __init__(self, code: str):
        self.code = code

    def create_module(self, spec):
        return None  # Default module creation

    def exec_module(self, module):
        exec(self.code, module.__dict__)


class VirtualMemoryFinder(MetaPathFinder):
    def __init__(self, virtual_modules: dict[str, str]):
        self.modules = virtual_modules

    def find_spec(self, fullname, path, target=None):
        if fullname in self.modules:
            code = self.modules[fullname]
            return ModuleSpec(fullname, VirtualMemoryLoader(code), origin="virtual_memory")
        return None


# Install custom finder into sys.meta_path
virtual_registry = {"virtual_config": "API_KEY = 'secret-999'\nDEBUG = True"}
finder = VirtualMemoryFinder(virtual_registry)
sys.meta_path.insert(0, finder)

# Now we can import the virtual module seamlessly!
import virtual_config
print(f"Virtual config imported: KEY={virtual_config.API_KEY}, DEBUG={virtual_config.DEBUG}")

# Cleanup
sys.meta_path.remove(finder)
del sys.modules["virtual_config"]
```
Output:
```
Virtual config imported: KEY=secret-999, DEBUG=True
```

---

## 4. Common Anti-Patterns & Gotchas

| Bad Practice | Root Cause | Clean Solution |
| :--- | :--- | :--- |
| Modifying `sys.path` using relative paths like `sys.path.append("..")` | Current working directory changes break relative imports | Resolve absolute paths using `Path(__file__).resolve().parent` |
| Calling `reload()` expecting old references to update | Existing objects holding imported functions retain the old references | Rebind imported attributes after calling `importlib.reload` |
| Using `__import__()` directly in modern code | Low-level primitive with complex argument signatures | Use high-level `importlib.import_module()` |
| Circular imports at module top-level | Module A imports B while B imports A before declarations exist | Move import inside function or use `import B` instead of `from B import x` |
| Deleting keys from `sys.modules` without clearing child submodules | Leaves parent package in an inconsistent state | Recursively remove all submodules matching prefix `pkg.` |
| Mutating `sys.path` inside library modules | Pollutes global interpreter state for all host applications | Use package relative imports or package entry points |

---

## 5. Interview Q&A

<details>
<summary>1. Describe the exact step-by-step process of the Python import statement.</summary>
1. Python checks <code>sys.modules</code> to see if the module is already cached. If so, it returns it.
2. If not, Python iterates through <code>sys.meta_path</code> finders (e.g. <code>BuiltinImporter</code>, <code>PathFinder</code>).
3. The matching finder returns a <code>ModuleSpec</code> object describing how to load the module.
4. Python creates a new module object (<code>types.ModuleType</code>) and immediately adds it to <code>sys.modules</code>.
5. The loader executes the module's code within the module's <code>__dict__</code> namespace.
6. If execution succeeds, the module object is returned; if an error occurs, it is removed from <code>sys.modules</code>.
</details>

<details>
<summary>2. Why does Python insert a module into sys.modules BEFORE executing its code?</summary>
To resolve circular imports. If Module A imports Module B, and Module B imports Module A, Module B finds Module A already present in <code>sys.modules</code> (even though it is only partially initialized), preventing an infinite import recursion loop.
</details>

<details>
<summary>3. What is a ModuleSpec (PEP 451)?</summary>
Introduced in Python 3.4 (PEP 451), <code>ModuleSpec</code> is an object encapsulating all import-related metadata for a module: its canonical name, loader, origin (filepath or built-in tag), submodule search locations (if it's a package), and whether it has been initialized.
</details>

<details>
<summary>4. What is the difference between sys.meta_path and sys.path_hooks?</summary>
- <code>sys.meta_path</code>: A list of top-level finders consulted first for every import.
- <code>sys.path_hooks</code>: A list of callables used by <code>PathFinder</code> to find loaders for specific filesystem entries or archive formats (like zip files).
</details>

<details>
<summary>5. How does importlib.reload() work under the hood?</summary>
<code>importlib.reload(module)</code> re-reads and re-executes the module's source file within the *existing* module object's <code>__dict__</code>. It overwrites updated attributes in-place, but does NOT update references that callers already imported via <code>from module import func</code>.
</details>

<details>
<summary>6. What happens if sys.modules[name] is assigned None?</summary>
In CPython, setting <code>sys.modules[name] = None</code> causes subsequent imports of <code>name</code> to immediately raise a <code>ModuleNotFoundError</code>. This technique is often used to simulate a missing dependency in test environments.
</details>

<details>
<summary>7. What is the role of site.py in sys.path initialization?</summary>
During Python startup, the <code>site</code> module is imported automatically (unless <code>-S</code> is passed). It scans standard system locations for `.pth` files and appends user and global `site-packages` directories to <code>sys.path</code>.
</details>

<details>
<summary>8. How do you import a module from an arbitrary file path not on sys.path?</summary>
Use <code>importlib.util.spec_from_file_location(name, filepath)</code> to generate a spec, then <code>importlib.util.module_from_spec(spec)</code> to create the module, and finally <code>spec.loader.exec_module(module)</code>.
</details>

---

## 6. Practical Challenges

<details>
<summary>Challenge 1: Safe Optional Dependency Loader</summary>

Write a utility function `optional_import(module_name: str, fallback=None)` that imports a module dynamically or returns `fallback` if unavailable.

```python
import importlib

def optional_import(module_name: str, fallback=None):
    try:
        return importlib.import_module(module_name)
    except ModuleNotFoundError:
        return fallback

math_mod = optional_import("math")
assert math_mod is not None
missing_mod = optional_import("fake_lib_123", fallback="missing")
assert missing_mod == "missing"
```
</details>

<details>
<summary>Challenge 2: Load Module from Arbitrary Filepath</summary>

Implement `load_module_from_path(name: str, file_path: str)` using `importlib.util.spec_from_file_location`.

```python
import importlib.util
from pathlib import Path

def load_module_from_path(name: str, file_path: str):
    spec = importlib.util.spec_from_file_location(name, file_path)
    if spec is None or spec.loader is None:
        raise ImportError(f"Cannot load module from {file_path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module

# Test with temporary file
import tempfile
with tempfile.NamedTemporaryFile("w", suffix=".py", delete=False) as f:
    f.write("GREETING = 'hello from file'\n")
    temp_path = f.name

mod = load_module_from_path("custom_temp", temp_path)
assert mod.GREETING == "hello from file"
```
</details>

<details>
<summary>Challenge 3: Submodule Purge Utility</summary>

Write a function `purge_package(pkg_name: str)` that removes a package and all its loaded child modules from `sys.modules`.

```python
import sys
import types

def purge_package(pkg_name: str):
    to_delete = [
        name for name in sys.modules
        if name == pkg_name or name.startswith(f"{pkg_name}.")
    ]
    for name in to_delete:
        del sys.modules[name]

# Setup mock modules
sys.modules["mockpkg"] = types.ModuleType("mockpkg")
sys.modules["mockpkg.sub"] = types.ModuleType("mockpkg.sub")
purge_package("mockpkg")
assert "mockpkg" not in sys.modules
assert "mockpkg.sub" not in sys.modules
```
</details>

<details>
<summary>Challenge 4: Mocking Missing Dependency via sys.modules</summary>

Temporarily block an existing module (e.g. `json`) by setting its `sys.modules` entry to `None` inside a context manager `block_module(name)`.

```python
import sys
from contextlib import contextmanager

@contextmanager
def block_module(name: str):
    original = sys.modules.get(name)
    sys.modules[name] = None
    try:
        yield
    finally:
        if original is not None:
            sys.modules[name] = original
        else:
            del sys.modules[name]

with block_module("math"):
    try:
        import math
        assert False, "Should fail"
    except ModuleNotFoundError:
        pass

import math
assert math.sqrt(4) == 2.0
```
</details>

<details>
<summary>Challenge 5: Track Imported Modules Context Manager</summary>

Create a context manager `track_new_imports()` that returns a list of all module names loaded into `sys.modules` during the execution of the block.

```python
import sys
from contextlib import contextmanager

@contextmanager
def track_new_imports():
    before = set(sys.modules.keys())
    new_imports = []
    try:
        yield new_imports
    finally:
        after = set(sys.modules.keys())
        new_imports.extend(after - before)

with track_new_imports() as loaded:
    import colorsys
assert "colorsys" in loaded
```
</details>

<details>
<summary>Challenge 6: Find Module Origin Filepath</summary>

Write a function `get_module_origin(mod_name: str) -> str | None` using `importlib.util.find_spec` that finds where a module is located on disk without importing it.

```python
import importlib.util

def get_module_origin(mod_name: str) -> str | None:
    spec = importlib.util.find_spec(mod_name)
    if spec:
        return spec.origin
    return None

origin = get_module_origin("json")
assert origin is not None and origin.endswith(".py")
assert get_module_origin("non_existent_lib") is None
```
</details>
