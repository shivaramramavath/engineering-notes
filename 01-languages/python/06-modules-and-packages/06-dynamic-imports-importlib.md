# Dynamic Imports with importlib

- **Topic**: `06-modules-and-packages`
- **Python version**: 3.12+ (tested on CPython 3.14.0)
- **Tested with**: CPython 3.12, 3.13, 3.14 (standard & free-threaded builds)
- **Free-threading notes**: Compatible. Dynamic module loading via `importlib` acquires CPython's internal module creation locks, ensuring thread safety across concurrent imports.

---

## 1. Concept Overview

While static import statements (`import foo`, `from bar import baz`) cover standard application dependencies, many production systems require **dynamic importing**:
- Plugin architectures where drivers or extensions are discovered from configuration files or database records at runtime.
- CLI dispatchers that dynamically route commands to submodules.
- Test runners loading test files from arbitrary directories.
- Optional dependencies that are only imported when specific features are invoked.

The Python standard library provides the `importlib` module as the comprehensive, official framework for dynamic imports. It supersedes older mechanisms (such as `__import__()`, `imp`, and `exec`) with clean, programmatic APIs.

Key entry points:
- `importlib.import_module(name, package=None)`: Programmatically import a module by string name.
- `importlib.util.find_spec(name, package=None)`: Discover a module's spec without loading or executing it.
- `importlib.util.spec_from_file_location(name, location)`: Load a module from an arbitrary file path outside `sys.path`.
- `importlib.reload(module)`: Reload an already imported module in-place.

```
Dynamic Import Pipeline:
String name: "plugins.auth_oauth"
               |
               v
  importlib.import_module("plugins.auth_oauth")
               |
               v
  Consults sys.modules cache
               |
        +------+------+
        |             |
     (Cached)     (Not Cached)
        |             |
        |             v
        |      sys.meta_path finders
        |             |
        |             v
        |      ModuleSpec -> Loader.exec_module()
        |             |
        +------+------+
               |
               v
    Returns Module Object
```

---

## 2. Key Mechanics & Architecture

### Loading from an Arbitrary File Path
To import a Python file located anywhere on disk (e.g. user-supplied config script or external plugin):
1. **Create Spec**: Call `importlib.util.spec_from_file_location(module_name, file_path)`.
2. **Create Module**: Call `importlib.util.module_from_spec(spec)`.
3. **Register (Recommended)**: Insert into `sys.modules[module_name] = module` before execution.
4. **Execute**: Call `spec.loader.exec_module(module)`.

```python
import importlib.util
spec = importlib.util.spec_from_file_location("custom_plugin", "/path/to/plugin.py")
module = importlib.util.module_from_spec(spec)
sys.modules["custom_plugin"] = module
spec.loader.exec_module(module)
```

---

## 3. Real-World Code Examples

### Example 1: Dynamic Driver / Plugin Factory
```python
import importlib
from typing import Any

class DriverFactory:
    @staticmethod
    def get_driver(driver_type: str) -> Any:
        module_map = {
            "json": "json",
            "csv": "csv",
        }
        module_name = module_map.get(driver_type)
        if not module_name:
            raise ValueError(f"Unsupported driver: {driver_type}")
        # Dynamically load the module
        return importlib.import_module(module_name)


driver = DriverFactory.get_driver("json")
encoded = driver.dumps({"status": "active"})
print(f"Driver loaded dynamically: {driver.__name__}, Output: {encoded}")
```
Output:
```
Driver loaded dynamically: json, Output: {"status": "active"}
```

### Example 2: Loading Module from Arbitrary Filepath with Verification
```python
import importlib.util
import tempfile
import pathlib

# Create an isolated Python file outside the project tree
with tempfile.NamedTemporaryFile("w", suffix=".py", delete=False) as f:
    f.write(
        "PLUGIN_NAME = 'S3Uploader'\n"
        "def execute(data): return f'Uploaded {data} to S3'\n"
    )
    temp_path = f.name

# Load dynamically using spec_from_file_location
spec = importlib.util.spec_from_file_location("s3_plugin", temp_path)
if spec and spec.loader:
    plugin_mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(plugin_mod)
    print(f"Loaded Plugin: {plugin_mod.PLUGIN_NAME}")
    print(f"Execution: {plugin_mod.execute('report.pdf')}")

pathlib.Path(temp_path).unlink()  # Cleanup
```
Output:
```
Loaded Plugin: S3Uploader
Execution: Uploaded report.pdf to S3
```

### Example 3: Inspecting Module Metadata without Importing
```python
import importlib.util

def check_module_availability(name: str) -> dict[str, str | None]:
    spec = importlib.util.find_spec(name)
    if spec is None:
        return {"available": False, "origin": None}
    return {
        "available": True,
        "name": spec.name,
        "origin": spec.origin,
        "has_submodules": spec.submodule_search_locations is not None
    }


print(f"json: {check_module_availability('json')}")
print(f"fake_pkg: {check_module_availability('fake_nonexistent_pkg')}")
```
Output:
```
json: {'available': True, 'name': 'json', 'origin': '...\\Lib\\json\\__init__.py', 'has_submodules': True}
fake_pkg: {'available': False, 'origin': None}
```

### Example 4: Relative Dynamic Import with `package` Argument
```python
import importlib

# When dynamically importing relative modules (.utils),
# the 'package' argument must be explicitly specified!
try:
    importlib.import_module(".decoder", package="json")
    print("Successfully imported relative module 'json.decoder'")
except ModuleNotFoundError as err:
    print(f"Failed: {err}")
```
Output:
```
Successfully imported relative module 'json.decoder'
```

### Example 5: In-Memory Dynamic Code Execution as a Module
```python
import types

code_str = """
def calculate_tax(amount: float) -> float:
    return amount * 0.15
"""

# Create a blank module object
tax_module = types.ModuleType("virtual_tax_module")
# Execute code inside module namespace
exec(code_str, tax_module.__dict__)

print(f"Module name: {tax_module.__name__}")
print(f"Tax for $100: ${tax_module.calculate_tax(100.0)}")
```
Output:
```
Module name: virtual_tax_module
Tax for $100: $15.0
```

---

## 4. Common Anti-Patterns & Gotchas

| Bad Practice | Root Cause | Clean Solution |
| :--- | :--- | :--- |
| Using `__import__()` directly | Low-level built-in with unintuitive return values for submodules | Use `importlib.import_module()` |
| Omitting `package` argument for relative dynamic imports | Python raises `TypeError: the 'package' argument is required to perform a relative import` | Pass `package="my_pkg"` when module string starts with `.` |
| Forgetting to insert dynamically loaded module into `sys.modules` | Pickling, multiprocessing, and sub-imports may fail to locate the module | Assign `sys.modules[spec.name] = module` before `exec_module` |
| Calling `exec(open(path).read())` instead of `importlib` | Loses module identity (`__file__`, `__spec__`), breaks imports | Use `importlib.util.spec_from_file_location` |
| Catching bare `except Exception:` on dynamic imports | Masks syntax errors or broken internal code in the target module | Catch `ModuleNotFoundError` specifically |
| Relying on `importlib.reload()` to update previously imported functions | Callers that ran `from mod import func` still reference the old function | Re-import or rebind attributes explicitly |

---

## 5. Interview Q&A

<details>
<summary>1. Why is importlib.import_module preferred over __import__?</summary>
<code>importlib.import_module()</code> provides a clean, modern, high-level API. By contrast, <code>__import__()</code> is a low-level interpreter primitive whose argument signatures are confusing: for example, <code>__import__("pkg.sub")</code> returns the top-level <code>pkg</code> object, NOT <code>sub</code>, unless you pass an explicit non-empty <code>fromlist</code> parameter.
</details>

<details>
<summary>2. How do you dynamically load a module from an arbitrary file path?</summary>
1. Obtain a <code>ModuleSpec</code> using <code>importlib.util.spec_from_file_location(name, path)</code>.
2. Instantiate the module object with <code>importlib.util.module_from_spec(spec)</code>.
3. Add to <code>sys.modules[name] = module</code>.
4. Execute the module code using <code>spec.loader.exec_module(module)</code>.
</details>

<details>
<summary>3. How does importlib.util.find_spec differ from importlib.import_module?</summary>
<code>find_spec(name)</code> locates the module specification and checks if the module exists without importing or executing its code. <code>import_module(name)</code> locates, loads, and executes the module, caching it in <code>sys.modules</code>.
</details>

<details>
<summary>4. How do relative imports work with importlib.import_module?</summary>
When importing a relative path (e.g. <code>.submodule</code>), you must provide the anchor package name via the <code>package</code> argument:
<pre><code>importlib.import_module(".submodule", package="my_app.core")
</code></pre>
Omitting <code>package</code> raises a <code>TypeError</code>.
</details>

<details>
<summary>5. What does spec.loader.exec_module(module) do?</summary>
It compiles and executes the module's Python bytecode within the module's <code>__dict__</code> namespace. If the module has already been executed, calling it again re-executes the module.
</details>

<details>
<summary>6. How can you construct a Python module entirely in memory?</summary>
Instantiate <code>types.ModuleType("module_name")</code> and populate its <code>__dict__</code> directly or via <code>exec(code_string, module.__dict__)</code>.
</details>

<details>
<summary>7. What happens if importlib.import_module() encounters a syntax error in the target file?</summary>
It raises a <code>SyntaxError</code> (not <code>ModuleNotFoundError</code>). Incomplete module state is purged from <code>sys.modules</code>.
</details>

<details>
<summary>8. Can you prevent a dynamically imported module from polluting sys.modules?</summary>
You can delete it from <code>sys.modules</code> after execution: <code>del sys.modules[mod_name]</code>. However, standard pickling and multiprocessing operations rely on <code>sys.modules</code> to look up types defined in that module.
</details>

---

## 6. Practical Challenges

<details>
<summary>Challenge 1: Safe Dynamic Plugin Registry</summary>

Write a `PluginManager` that scans a directory for `.py` files, dynamically imports each using `importlib.util.spec_from_file_location`, and returns a dictionary of `{plugin_name: module}`.

```python
import importlib.util
from pathlib import Path

def load_plugins_from_dir(directory: Path) -> dict:
    plugins = {}
    for p in directory.glob("*.py"):
        if p.name.startswith("_"): continue
        mod_name = p.stem
        spec = importlib.util.spec_from_file_location(mod_name, str(p))
        if spec and spec.loader:
            mod = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(mod)
            plugins[mod_name] = mod
    return plugins
```
</details>

<details>
<summary>Challenge 2: In-Memory Virtual Module Builder</summary>

Write a helper `create_virtual_module(name: str, **attrs)` that returns an initialized `types.ModuleType` object populated with given attributes.

```python
import types

def create_virtual_module(name: str, **attrs):
    mod = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(mod, k, v)
    return mod

vm = create_virtual_module("math_helpers", add=lambda a, b: a + b, PI=3.14159)
assert vm.add(2, 3) == 5
assert vm.PI == 3.14159
```
</details>

<details>
<summary>Challenge 3: Verify Relative Dynamic Import</summary>

Demonstrate importing `json.decoder` dynamically using `import_module` with relative dot syntax.

```python
import importlib

dec_mod = importlib.import_module(".decoder", package="json")
assert hasattr(dec_mod, "JSONDecoder")
```
</details>

<details>
<summary>Challenge 4: Spec Inspector Utility</summary>

Implement `get_module_file(mod_name: str) -> str | None` using `importlib.util.find_spec` without importing the module.

```python
import importlib.util

def get_module_file(mod_name: str) -> str | None:
    spec = importlib.util.find_spec(mod_name)
    return spec.origin if spec else None

assert get_module_file("json") is not None
assert get_module_file("fake_module_xyz") is None
```
</details>

<details>
<summary>Challenge 5: Dynamic Class Instantiator</summary>

Write `instantiate_class(module_path: str, class_name: str, *args, **kwargs)` that imports `module_path`, extracts `class_name`, and returns a new instance.

```python
import importlib

def instantiate_class(module_path: str, class_name: str, *args, **kwargs):
    mod = importlib.import_module(module_path)
    cls = getattr(mod, class_name)
    return cls(*args, **kwargs)

import io
s = instantiate_class("io", "StringIO", "initial content")
assert s.getvalue() == "initial content"
```
</details>

<details>
<summary>Challenge 6: Isolated Code String Execution</summary>

Execute a Python code string inside a fresh module and return the result of calling a specific function inside that module.

```python
import types

def run_isolated_function(code: str, fn_name: str, *args):
    mod = types.ModuleType("isolated")
    exec(code, mod.__dict__)
    fn = getattr(mod, fn_name)
    return fn(*args)

code = "def square(x): return x * x"
assert run_isolated_function(code, "square", 6) == 36
```
</details>
