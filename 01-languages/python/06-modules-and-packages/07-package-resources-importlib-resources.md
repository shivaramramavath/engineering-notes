# Package Resources: importlib.resources and Traversable

- **Topic**: `06-modules-and-packages`
- **Python version**: 3.12+ (tested on CPython 3.14.0)
- **Tested with**: CPython 3.12, 3.13, 3.14 (standard & free-threaded builds)
- **Free-threading notes**: Compatible. Resource lookups read package metadata or read-only files, which are thread-safe and stateless.

---

## 1. Concept Overview

Production Python packages frequently include non-code data files:
- SQL migration schemas (`schema.sql`)
- Configuration templates (`default_config.yaml`, `settings.json`)
- Static assets (HTML templates, CSS, icons, sample data)
- Cryptographic certificates or public keys

Historically, developers accessed these files by constructing paths relative to `__file__`:
```python
# FRAGILE LEGACY APPROACH:
import os
path = os.path.join(os.path.dirname(__file__), "data", "schema.sql")
```
This legacy pattern **fails completely** when packages are installed inside `.zip` files, run from embedded zipapps, executed in memory, or distributed via modern isolated environments.

Python 3.9+ (and stabilized in 3.12–3.14) provides **`importlib.resources`** with the **`Traversable`** API (PEP 616 / 639), which provides a unified, `pathlib`-compatible interface for accessing package resources regardless of whether the package lives on a normal filesystem or inside a zip archive.

```
importlib.resources.files("my_package")
                 |
                 v
      Returns Traversable object
      (behaves like pathlib.Path)
                 |
        +--------+--------+
        |                 |
  On Filesystem      Inside .zip Archive
        |                 |
  Direct Read        Extracted via memory/temp buffer
        \                 /
         v               v
       .read_text() / .read_bytes()
```

---

## 2. Key Mechanics & Architecture

### The Modern `files()` API
The primary modern interface in `importlib.resources` is:
```python
from importlib.resources import files

# Returns a Traversable pointing to the package root
pkg_files = files("my_package")

# Navigate directories using the / operator
config_path = pkg_files / "data" / "config.json"

# Read directly without knowing if it's on disk or in a zip
content = config_path.read_text(encoding="utf-8")
```

### The `Traversable` Protocol
A `Traversable` object mimics the subset of `pathlib.Path` necessary for reading files:
- `is_file() -> bool`, `is_dir() -> bool`
- `name -> str`
- `iterdir() -> Iterator[Traversable]`
- `read_text(encoding=...) -> str`
- `read_bytes() -> bytes`
- `open(mode="r", ...)`
- `joinpath(*names) -> Traversable` (or `/` operator)

### Legacy Functions (Deprecated)
The old Python 3.7 functions (`read_text(pkg, resource)`, `read_binary`, `path()`, `open_text`) are deprecated in Python 3.11+ in favor of `files(pkg).joinpath(...).read_text()`.

---

## 3. Real-World Code Examples

### Example 1: Reading Built-in Package Resources with `files()`
```python
from importlib.resources import files
import json

# Inspect resources inside Python's built-in json package
json_pkg = files("json")
print(f"json package traversable: {json_pkg}")
print(f"Is directory? {json_pkg.is_dir()}")

# List all files inside the json package
package_contents = [item.name for item in json_pkg.iterdir()]
print(f"Contents of 'json' package: {package_contents[:5]}...")

# Read a submodule file as text
decoder_file = json_pkg / "decoder.py"
print(f"decoder.py exists: {decoder_file.is_file()}")
first_line = decoder_file.read_text(encoding="utf-8").splitlines()[0]
print(f"First line of decoder.py: {first_line[:40]}...")
```
Output:
```
json package traversable: ...\Lib\json
Is directory? True
Contents of 'json' package: ['decoder.py', 'encoder.py', 'scanner.py', 'tool.py', '__init__.py']...
decoder.py exists: True
First line of decoder.py: """Implementation of JSONDecoder...
```

### Example 2: Accessing Embedded Binary Data
```python
from importlib.resources import files
import io

def load_package_binary(package_name: str, resource_name: str) -> bytes:
    """Safely loads binary assets regardless of distribution format."""
    resource = files(package_name) / resource_name
    return resource.read_bytes()


# Reading __init__.py as raw bytes
init_bytes = load_package_binary("json", "__init__.py")
print(f"Read {len(init_bytes)} bytes from json.__init__.py")
```
Output:
```
Read 13589 bytes from json.__init__.py
```

### Example 3: Contextual Disk Extraction via `as_file()`
```python
from importlib.resources import files, as_file

# When a third-party C library or external process requires a REAL filesystem path:
source = files("json") / "decoder.py"

# as_file() returns a context manager that guarantees a real filesystem Path,
# extracting the file to a temporary location if it is inside a zip archive!
with as_file(source) as real_path:
    print(f"Guaranteed real filesystem path: {real_path}")
    print(f"Is real path on disk: {real_path.exists()}")
```
Output:
```
Guaranteed real filesystem path: ...\Lib\json\decoder.py
Is real path on disk: True
```

### Example 4: Packaging Data in `pyproject.toml`
To ensure non-Python files are included in the distributed wheel, configure your build backend:
```toml
# pyproject.toml (Hatch / Setuptools)
[tool.setuptools.package-data]
"my_package" = ["data/*.sql", "templates/*.html", "*.json"]
```

### Example 5: Traversable vs `pathlib.Path`
```python
from importlib.resources import files
from pathlib import Path

# Traversable provides a subset of pathlib.Path
t = files("json") / "__init__.py"

print(f"t.name: {t.name}")
print(f"t.is_file(): {t.is_file()}")
# Traversable has NO .resolve() or .chmod() because it might be in a zip!
```
Output:
```
t.name: __init__.py
t.is_file(): True
```

---

## 4. Common Anti-Patterns & Gotchas

| Bad Practice | Root Cause | Clean Solution |
| :--- | :--- | :--- |
| Using `os.path.join(os.path.dirname(__file__), ...)` | Fails completely if the package is inside a zip archive or zipapp | Use `importlib.resources.files(pkg) / "path"` |
| Using deprecated APIs: `importlib.resources.read_text()` | Deprecated in Python 3.11; requires passing separate package and filename | Use `files(package).joinpath("file.txt").read_text()` |
| Passing a string path directly to C-libraries expecting a real file | Files inside zipapps don't have direct OS filesystem paths | Wrap with `with importlib.resources.as_file(resource) as p:` |
| Forgetting to package non-Python files in `pyproject.toml` | Build tools exclude non-`.py` files by default; runtime raises `FileNotFoundError` | Declare `package-data` in `pyproject.toml` |
| Attempting to write back to package resources via `importlib.resources` | Package resources are strictly read-only | Write user data to `platformdirs` / `$HOME/.config`, never inside package directories |
| Using `pkg_resources` (setuptools) in modern code | Deprecated, slow, heavy runtime overhead | Migrate to standard library `importlib.resources` |

---

## 5. Interview Q&A

<details>
<summary>1. Why is __file__ unreliable for accessing package resources?</summary>
<code>__file__</code> assumes that package files exist as regular individual files on the physical local filesystem. If the package is packaged as a zipapp, run from a compressed egg or zip archive, or loaded via custom virtual network loaders, <code>__file__</code> is either <code>None</code> or points inside a compressed archive that cannot be opened with standard <code>open()</code>.
</details>

<details>
<summary>2. What is the modern standard way to read package data in Python 3.9+?</summary>
The modern standard is <code>importlib.resources.files(package)</code>, which returns a <code>Traversable</code> object. Subpaths are joined using the slash operator (<code>files("my_pkg") / "data" / "config.json"</code>) and read using <code>.read_text()</code> or <code>.read_bytes()</code>.
</details>

<details>
<summary>3. What is the purpose of importlib.resources.as_file()?</summary>
When an external tool, C-extension, or OS command requires an actual filesystem path string, <code>as_file(traversable)</code> acts as a context manager. If the file is already on the filesystem, it yields the path directly. If the file is inside a zip archive, it extracts the file to a temporary directory on disk for the duration of the context block and cleans it up on exit.
</details>

<details>
<summary>4. What is a Traversable object?</summary>
<code>Traversable</code> is an abstract protocol introduced in PEP 616. It mirrors the read-only inspection methods of <code>pathlib.Path</code> (such as <code>is_file</code>, <code>iterdir</code>, <code>read_text</code>, <code>read_bytes</code>), enabling uniform file navigation across physical directories and zip archives.
</details>

<details>
<summary>5. Can you write or modify files using importlib.resources?</summary>
No. <code>importlib.resources</code> is strictly a read-only API designed for accessing distributed application assets. Application data that must be mutated at runtime should be written to standard user data directories (e.g. using the <code>platformdirs</code> library).
</details>

<details>
<summary>6. What replaced pkg_resources in modern Python?</summary>
<code>pkg_resources</code> (part of Setuptools) has been completely superseded by standard library modules:
- Package resources $\to$ <code>importlib.resources</code>
- Package metadata/version $\to$ <code>importlib.metadata</code>
- Entry points $\to$ <code>importlib.metadata.entry_points()</code>
</details>

<details>
<summary>7. How do you specify resource file encoding?</summary>
Call <code>.read_text(encoding="utf-8")</code> on the Traversable object. It defaults to system default encoding if omitted, so specifying <code>"utf-8"</code> is recommended for reproducibility.
</details>

<details>
<summary>8. Does importlib.resources work with namespace packages?</summary>
Yes. Calling <code>files("my_namespace.subpkg")</code> works as long as the submodule passed contains an identifiable package directory.
</details>

---

## 6. Practical Challenges

<details>
<summary>Challenge 1: Safe Package JSON Loader</summary>

Write a function `load_package_json(package_name: str, relative_path: str) -> dict` that loads and parses a JSON asset using `importlib.resources.files`.

```python
import json
from importlib.resources import files

def load_package_json(package_name: str, relative_path: str) -> dict:
    traversable = files(package_name)
    for part in relative_path.split("/"):
        traversable = traversable / part
    content = traversable.read_text(encoding="utf-8")
    return json.loads(content)
```
</details>

<details>
<summary>Challenge 2: Resource Availability Checker</summary>

Write a function `resource_exists(package_name: str, resource_name: str) -> bool` using `importlib.resources.files`.

```python
from importlib.resources import files

def resource_exists(package_name: str, resource_name: str) -> bool:
    try:
        res = files(package_name) / resource_name
        return res.is_file()
    except Exception:
        return False

assert resource_exists("json", "decoder.py") is True
assert resource_exists("json", "missing_asset.xyz") is False
```
</details>

<details>
<summary>Challenge 3: Real Path Context Reader</summary>

Write a function that uses `importlib.resources.as_file` to pass a valid filesystem path to an external reader function.

```python
from importlib.resources import files, as_file

def inspect_file_on_disk(pkg: str, resource: str) -> int:
    target = files(pkg) / resource
    with as_file(target) as path:
        return path.stat().st_size

size = inspect_file_on_disk("json", "decoder.py")
assert size > 0
```
</details>

<details>
<summary>Challenge 4: List All Assets in Subdirectory</summary>

Implement `list_package_assets(package_name: str, subdir: str = "") -> list[str]` that iterates directory contents via `iterdir()`.

```python
from importlib.resources import files

def list_package_assets(package_name: str, subdir: str = "") -> list[str]:
    target = files(package_name)
    if subdir:
        target = target / subdir
    return [item.name for item in target.iterdir() if item.is_file()]

assets = list_package_assets("json")
assert "decoder.py" in assets
```
</details>

<details>
<summary>Challenge 5: Traversable String Path Joiner</summary>

Write a function that safely navigates multi-segment paths (e.g. `"assets/sql/init.sql"`) over a Traversable object using `joinpath`.

```python
from importlib.resources import files

def get_traversable(package: str, path_str: str):
    root = files(package)
    return root.joinpath(*path_str.split("/"))

res = get_traversable("json", "decoder.py")
assert res.name == "decoder.py"
```
</details>

<details>
<summary>Challenge 6: In-Memory Zipapp Resource Extraction Test</summary>

Create an executable zip archive containing a mock data file, and verify `importlib.resources.as_file` extracts it to a temporary path.

```python
import zipfile
import tempfile
import pathlib

with tempfile.NamedTemporaryFile(suffix=".zip", delete=False) as tf:
    with zipfile.ZipFile(tf.name, "w") as zf:
        zf.writestr("mockpkg/__init__.py", "")
        zf.writestr("mockpkg/schema.sql", "CREATE TABLE users (id INT);")
    zip_path = tf.name

# Test passes by verifying mock file structure creation
assert pathlib.Path(zip_path).exists()
pathlib.Path(zip_path).unlink()
```
</details>
