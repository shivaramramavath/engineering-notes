# Module 09: Files, Streams & OS (00-pathlib)

> **Topic Overview**: Introduced in Python 3.4 via PEP 428, `pathlib` provides an object-oriented, cross-platform filesystem abstraction that supersedes the legacy `os.path` string-manipulation module. Offering clean path arithmetic via the division operator (`/`), automatic OS path separator normalization, globbing, and atomic file I/O methods, `pathlib` is the modern standard for filesystem operations in Python.

---

## Metadata Table

| Field | Value |
|---|---|
| **Category** | Core Language / File I/O & Filesystem |
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

- **Problem Statement**: Historically, Python represented filesystem paths as raw string objects. Path manipulation required chaining fragile string calls or deeply nested functions: `os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "data", "file.txt")`. This led to cross-platform bugs (e.g. forward `/` vs backward `\` slashes), confusing string slicing for extensions, and clunky separate calls to `open()`.
- **Mental Model**: `pathlib` treats paths as first-class, immutable semantic domain objects. A `Path` object encapsulates path arithmetic (`parent / "subdir" / "file.txt"`), filesystem introspection (`.is_file()`, `.stat()`), and direct content access (`.read_text()`, `.write_bytes()`).
- **When to Use**: Always. Use `pathlib.Path` for all filesystem interactions, file lookups, directory crawling, and relative path navigation in modern Python.
- **When NOT to Use**: Do not use `pathlib` if operating on low-level OS handles that strictly require integer file descriptors (use `os` module or `fileno()`).

---

## 2. Core Concepts

### Class Hierarchy: Pure vs Concrete Paths
`pathlib` divides path objects into two architectural tiers:

```text
PurePath (String & Syntax manipulation only; no filesystem access)
 ├── PurePosixPath (Linux, macOS, Unix paths with '/')
 └── PureWindowsPath (Windows paths with 'C:\' and '\')
      │
      v (Concrete Subclasses: Connect directly to the underlying OS filesystem)
Path (Instantiates PosixPath or WindowsPath dynamically based on active OS)
 ├── PosixPath
 └── WindowsPath
```

1. **`PurePath`**: Operates strictly on string representations without touching the disk. Safe to run on a Linux server to parse Windows paths or vice-versa.
2. **`Path` (Concrete)**: Inherits from `PurePath` and adds system calls to introspect, read, create, and delete files on the active host operating system.

### Path Arithmetic via Division Operator (`/`)
`Path` overrides `__truediv__` and `__rtruediv__`, allowing strings and paths to be joined naturally:

```python
from pathlib import Path
base = Path("/var/log")
app_log = base / "nginx" / "access.log"  # PosixPath('/var/log/nginx/access.log')
```

---

## 3. How It Works Internally

When `Path(*args)` is instantiated on a running machine, `Path.__new__` dynamically inspects `os.name`:

```text
+---------------------------------------------------------------+
| Path Instantiation Dynamic Dispatch                           |
+---------------------------------------------------------------+
| User calls: p = Path("data", "config.toml")                   |
| 1. Path.__new__() checks os.name:                             |
|    - If os.name == 'posix'   --> returns PosixPath instance   |
|    - If os.name == 'nt'      --> returns WindowsPath instance |
| 2. Canonicalizes path segments:                               |
|    - Collapses consecutive slashes                            |
|    - Normalizes separators to host convention                 |
| 3. Stores normalized path string in internal _raw_path        |
+---------------------------------------------------------------+
```

---

## 4. Syntax & API Reference

### Core Properties
- `p.name`: Final path component (e.g., `"access.log.gz"`).
- `p.stem`: Final component without suffix (e.g., `"access.log"`).
- `p.suffix`: File extension (e.g., `".gz"`).
- `p.suffixes`: List of all extensions (e.g., `[".log", ".gz"]`).
- `p.parent`: Immediate parent directory path.
- `p.parents`: Sequence of all logical ancestors up to the root.
- `p.parts`: Tuple of all path segments.

### Core Filesystem Methods
- `p.exists()`: Returns `True` if file or directory exists.
- `p.is_file()` / `p.is_dir()` / `p.is_symlink()`: Type checks.
- `p.resolve(strict=False)`: Resolves symlinks and relative components into an absolute path.
- `p.mkdir(parents=False, exist_ok=False)`: Creates directory; `parents=True` mimics `mkdir -p`.
- `p.unlink(missing_ok=False)`: Deletes file.
- `p.rmdir()`: Deletes empty directory.
- `p.glob(pattern)` / `p.rglob(pattern)`: Yields matching paths (non-recursive and recursive).
- `p.read_text(encoding="utf-8")` / `p.write_text(data, encoding="utf-8")`: Direct atomic-style file I/O.

---

## 5. Code Examples

### 1. Minimal / Basic Example: Path Navigation and Inspection
```python
from pathlib import Path

# Constructing path
config_path = Path("/home/deploy/app/config.prod.toml")

print("File name:     ", config_path.name)
print("File stem:     ", config_path.stem)
print("File suffix:   ", config_path.suffix)
print("All suffixes:  ", config_path.suffixes)
print("Parent dir:    ", config_path.parent)
print("Grandparent:   ", config_path.parent.parent)
print("Path parts:    ", config_path.parts)
```
**Output**:
```text
File name:      config.prod.toml
File stem:      config.prod
File suffix:    .toml
All suffixes:   ['.prod', '.toml']
Parent dir:     /home/deploy/app
Grandparent:    /home/deploy
Path parts:     ('/', 'home', 'deploy', 'app', 'config.prod.toml')
```

### 2. Real-World Production Example: Safe Workspace Directory Builder
```python
import tempfile
import os
from pathlib import Path

def setup_project_scaffolding(base_dir: Path) -> dict[str, Path]:
    """Sets up a robust, cross-platform project directory tree."""
    dirs = {
        "src": base_dir / "src",
        "tests": base_dir / "tests",
        "logs": base_dir / "var" / "logs"
    }

    # Create directories recursively (mkdir -p equivalent)
    for path in dirs.values():
        path.mkdir(parents=True, exist_ok=True)

    # Write initial files directly via Path methods
    (dirs["src"] / "__init__.py").write_text("# Core package\n", encoding="utf-8")
    (dirs["src"] / "main.py").write_text("print('App initialized')\n", encoding="utf-8")
    (dirs["logs"] / "app.log").write_text("", encoding="utf-8")

    return dirs

# Verify with temporary directory
with tempfile.TemporaryDirectory() as tmp:
    base = Path(tmp) / "demo_project"
    created = setup_project_scaffolding(base)

    print("Project scaffolded at:", base.name)
    all_files = sorted([p.relative_to(base).as_posix() for p in base.rglob("*") if p.is_file()])
    print("Files created:")
    for f in all_files:
        print(f"  - {f}")
```
**Output**:
```text
Project scaffolded at: demo_project
Files created:
  - src/__init__.py
  - src/main.py
  - var/logs/app.log
```

### 3. Edge Case: Pure Paths on Mismatched Operating Systems
```python
from pathlib import PurePosixPath, PureWindowsPath

# Manipulating Windows path on a Linux server (or vice versa)
win_path = PureWindowsPath(r"C:\Users\Admin\Documents\report.xlsx")
print("Windows drive:  ", win_path.drive)
print("Windows root:   ", win_path.root)
print("Converted Posix:", win_path.as_posix())

posix_path = PurePosixPath("/etc/nginx/sites-available/default")
print("Posix parent:   ", posix_path.parent)
```
**Output**:
```text
Windows drive:   C:
Windows root:    \
Converted Posix: C:/Users/Admin/Documents/report.xlsx
Posix parent:    /etc/nginx/sites-available
```

### 4. Failure Case & Exception Handling: Instantiating Concrete Path for Other OS
```python
import os
from pathlib import PosixPath, WindowsPath

try:
    if os.name == "nt":
        # Cannot instantiate PosixPath on Windows!
        PosixPath("/var/log/app.log")
    else:
        # Cannot instantiate WindowsPath on Linux/macOS!
        WindowsPath(r"C:\Windows\System32")
except NotImplementedError as err:
    print(f"Captured expected NotImplementedError: {err}")
```
**Output**:
```text
Captured expected NotImplementedError: cannot instantiate '...' on your system
```

### 5. Idiomatic Pythonic Pattern: Recursive Globbing with Pattern Matching
```python
import tempfile
from pathlib import Path

with tempfile.TemporaryDirectory() as tmp:
    root = Path(tmp)
    # Populate test directory
    (root / "module_a").mkdir()
    (root / "module_b").mkdir()
    (root / "module_a" / "test_one.py").write_text("# t1", encoding="utf-8")
    (root / "module_b" / "test_two.py").write_text("# t2", encoding="utf-8")
    (root / "module_b" / "helper.py").write_text("# h1", encoding="utf-8")

    # rglob finds all matching test files recursively
    test_files = sorted([p.name for p in root.rglob("test_*.py")])
    print("Discovered test suites:", test_files)
```
**Output**:
```text
Discovered test suites: ['test_one.py', 'test_two.py']
```

---

## 6. Edge Cases & Pitfalls

1. **Concrete vs Pure OS Restriction**:
   - `PurePosixPath` and `PureWindowsPath` can be instantiated on any OS. Concrete `PosixPath` and `WindowsPath` can **only** be instantiated on their respective operating systems. Calling `PosixPath()` on Windows raises `NotImplementedError`. Always instantiate `Path()`.
2. **`resolve()` Behavior**:
   - `Path(".").resolve()` resolves symlinks and canonicalizes the path. In Python 3.6+, passing `strict=True` raises `FileNotFoundError` if the path does not exist on disk.
3. **The `.suffix` vs `.suffixes` Trap**:
   - For archive files like `archive.tar.gz`, `path.suffix` is only `".gz"`. `path.stem` is `"archive.tar"`. To inspect all extensions, use `path.suffixes` which returns `['.tar', '.gz']`.

---

## 7. Common Mistakes

| Mistake | Why It Fails | Exception / Diagnostic | Correct Pattern |
|---|---|---|---|
| Instantiating `PosixPath()` or `WindowsPath()` directly | Cannot instantiate foreign OS path concrete classes | `NotImplementedError: cannot instantiate '...'` | Use `Path()` or `PurePath()` |
| String concatenation `path + "/sub"` | `Path` does not support `+` with string | `TypeError: unsupported operand type(s) for +: 'Path' and 'str'` | Use division operator: `path / "sub"` |
| Forgetting `exist_ok=True` in `mkdir()` | Crashes if directory already exists | `FileExistsError: [Errno 17] File exists` | `path.mkdir(parents=True, exist_ok=True)` |
| Omitting encoding in `.read_text()` | Relies on platform-default encoding, corrupting UTF-8 on Windows | `UnicodeDecodeError` / PEP 597 warning | Always specify `encoding="utf-8"` |
| Using `path.suffix` expecting full compound extension | Only captures the final dot extension (e.g., `.gz` from `.tar.gz`) | Truncated extension logic | Use `path.suffixes` for compound extensions |
| Assuming `path.unlink()` deletes directories | `unlink()` only removes files and symlinks | `IsADirectoryError: [Errno 21] Is a directory` | Use `path.rmdir()` or `shutil.rmtree` |

---

## 8. Performance & Complexity

- **Path Object Creation**: $O(K)$ where $K$ is the number of path segments, taking $\approx 1-3\ \mu\text{s}$ (slightly slower than raw string concatenation due to parsing and normalization).
- **Filesystem System Calls**: Methods like `.exists()`, `.stat()`, and `.is_file()` call OS `stat()` syscalls directly via Python's C library wrapper in $O(1)$.
- **Memory Footprint**: A `Path` object allocates $\approx 56$ bytes plus internal string representations.

---

## 9. CPython Internals & Bytecode

Let's inspect how the path division operator (`/`) compiles into bytecode:

```python
from pathlib import Path
import dis

def join_paths(base, sub):
    return base / sub

dis.dis(join_paths)
```

**Bytecode output**:
```text
  1           0 RESUME                   0

  2           2 LOAD_FAST                0 (base)
              4 LOAD_FAST                1 (sub)
              6 BINARY_OP               11 (/)
             10 RETURN_VALUE
```
Notice `BINARY_OP 11 (/)`. CPython invokes `Path.__truediv__(self, other)`:
1. Verifies that `other` is a `str` or `PathLike` object.
2. Appends `other` to `self._parts`.
3. Returns a newly constructed `Path` instance with normalized parts.

---

## 10. Best Practices

1. **Always Specify `encoding="utf-8"`**: Never call `read_text()` or `write_text()` without explicit encoding.
2. **Use `parents=True, exist_ok=True`**: Standardize on `p.mkdir(parents=True, exist_ok=True)` for directory creation.
3. **Accept `os.PathLike` in Signatures**: In public function type annotations, use `os.PathLike[str] | str` to accept both `Path` and raw string paths.

---

## 11. Real-World Case Study: Automated Backup Rotation Utility

```python
import tempfile
import time
from pathlib import Path

def rotate_backups(backup_dir: Path, max_retained: int = 3) -> list[Path]:
    """Finds all .tar.gz archives in backup_dir and purges the oldest, keeping max_retained."""
    if not backup_dir.is_dir():
        raise ValueError(f"Backup directory does not exist: {backup_dir}")

    # Discover and sort by last modified timestamp ascending (oldest first)
    backups = sorted(
        [p for p in backup_dir.glob("*.tar.gz") if p.is_file()],
        key=lambda p: p.stat().st_mtime
    )

    purged = []
    while len(backups) > max_retained:
        oldest = backups.pop(0)
        oldest.unlink()
        purged.append(oldest)

    return purged

# Verification
with tempfile.TemporaryDirectory() as tmp:
    bdir = Path(tmp)
    # Create 5 simulated backup files with staggered timestamps
    for i in range(1, 6):
        bfile = bdir / f"backup_v{i}.tar.gz"
        bfile.write_text(f"archive {i}", encoding="utf-8")
        time.sleep(0.01)

    purged_files = rotate_backups(bdir, max_retained=3)
    remaining_files = sorted([p.name for p in bdir.glob("*.tar.gz")])

    print("Purged oldest files:   ", [p.name for p in purged_files])
    print("Retained active files: ", remaining_files)
    assert len(remaining_files) == 3
```
**Output**:
```text
Purged oldest files:    ['backup_v1.tar.gz', 'backup_v2.tar.gz']
Retained active files:  ['backup_v3.tar.gz', 'backup_v4.tar.gz', 'backup_v5.tar.gz']
```

---

## 12. Interview Questions

### Q1: What is the difference between `PurePath` and `Path` in `pathlib`?
<details>
<summary><strong>Answer</strong></summary>
<code>PurePath</code> provides pure computational path manipulation (joining, parts, extension extraction, parent navigation) without accessing the filesystem. It can be used across operating systems (e.g. manipulating Windows paths on Linux). <code>Path</code> inherits from <code>PurePath</code> and adds concrete I/O system calls (<code>exists()</code>, <code>mkdir()</code>, <code>read_text()</code>, <code>stat()</code>) interacting directly with the host filesystem.
</details>

### Q2: How does `pathlib.Path` achieve path joining using the `/` operator?
<details>
<summary><strong>Answer</strong></summary>
<code>Path</code> implements the <code>__truediv__</code> and <code>__rtruediv__</code> dunder methods. When an expression like <code>path / "subdir"</code> is evaluated, <code>Path.__truediv__</code> receives the string or Path, parses its components, canonicalizes the segments according to OS separator rules, and returns a new <code>Path</code> object.
</details>

### Q3: What is the difference between `path.stem` and `path.name`?
<details>
<summary><strong>Answer</strong></summary>
<code>path.name</code> is the full final filename including extension (e.g., <code>"data.csv"</code>). <code>path.stem</code> is the final filename with the last extension stripped off (e.g., <code>"data"</code>).
</details>

### Q4: How does `pathlib` handle compound extensions like `.tar.gz`?
<details>
<summary><strong>Answer</strong></summary>
<code>path.suffix</code> returns only the final extension: <code>".gz"</code>. <code>path.stem</code> returns <code>"archive.tar"</code>. To inspect all extensions, use <code>path.suffixes</code>, which returns a list: <code>['.tar', '.gz']</code>.
</details>

### Q5: What does `Path.resolve()` do?
<details>
<summary><strong>Answer</strong></summary>
<code>resolve()</code> canonicalizes the path: it resolves all symbolic links, eliminates <code>..</code> and <code>.</code> segments, and converts a relative path into an absolute path.
</details>

### Q6: What is the `os.PathLike` protocol?
<details>
<summary><strong>Answer</strong></summary>
Introduced in PEP 519 (Python 3.6), <code>os.PathLike</code> is an abstract protocol implemented by objects that represent a filesystem path. Any class implementing <code>__fspath__() -> str | bytes</code> satisfies the protocol. Both <code>pathlib.Path</code> and standard library file functions (like <code>open()</code>) support <code>PathLike</code> objects natively.
</details>

### Q7: Why is `missing_ok=True` useful in `path.unlink()`?
<details>
<summary><strong>Answer</strong></summary>
Added in Python 3.8, <code>path.unlink(missing_ok=True)</code> prevents Python from raising a <code>FileNotFoundError</code> if the target file has already been deleted or does not exist, enabling safe idempotent file deletions.
</details>

### Q8: What is the difference between `path.glob("*.py")` and `path.rglob("*.py")`?
<details>
<summary><strong>Answer</strong></summary>
<code>path.glob("*.py")</code> only searches the immediate directory represented by <code>path</code>. <code>path.rglob("*.py")</code> is recursive: it searches <code>path</code> and all nested subdirectories to any arbitrary depth (equivalent to <code>path.glob("**/*.py")</code>).
</details>

---

## 13. Exercises & Challenges

### Challenge 1 (Easy): Extension Replacer
Write a function `change_extension(filepath: Path, new_ext: str) -> Path` that replaces the extension of `filepath` with `new_ext` using `with_suffix()`.
<details>
<summary><strong>Solution</strong></summary>

```python
from pathlib import Path

def change_extension(filepath: Path, new_ext: str) -> Path:
    if not new_ext.startswith("."):
        new_ext = "." + new_ext
    return filepath.with_suffix(new_ext)

p = Path("/docs/notes.txt")
assert change_extension(p, "md") == Path("/docs/notes.md")
assert change_extension(p, ".pdf") == Path("/docs/notes.pdf")
```
</details>

### Challenge 2 (Easy): Safe File Writer
Write a function `safe_write(filepath: Path, content: str)` that automatically creates any missing parent directories before writing the text file using UTF-8 encoding.
<details>
<summary><strong>Solution</strong></summary>

```python
from pathlib import Path
import tempfile

def safe_write(filepath: Path, content: str) -> None:
    filepath.parent.mkdir(parents=True, exist_ok=True)
    filepath.write_text(content, encoding="utf-8")

with tempfile.TemporaryDirectory() as tmp:
    target = Path(tmp) / "nested" / "dir" / "config.json"
    safe_write(target, '{"key": "value"}')
    assert target.is_file()
    assert target.read_text(encoding="utf-8") == '{"key": "value"}'
```
</details>

### Challenge 3 (Medium): Find Largest Files in Tree
Write a function `find_largest_files(root_dir: Path, n: int = 3) -> list[tuple[str, int]]` using `rglob` and `.stat().st_size` that returns the names and byte sizes of the top `n` largest files in the tree.
<details>
<summary><strong>Solution</strong></summary>

```python
from pathlib import Path
import tempfile

def find_largest_files(root_dir: Path, n: int = 3) -> list[tuple[str, int]]:
    files = [
        (p.name, p.stat().st_size)
        for p in root_dir.rglob("*")
        if p.is_file()
    ]
    return sorted(files, key=lambda x: x[1], reverse=True)[:n]

with tempfile.TemporaryDirectory() as tmp:
    r = Path(tmp)
    (r / "small.txt").write_bytes(b"A" * 10)
    (r / "large.txt").write_bytes(b"B" * 500)
    (r / "med.txt").write_bytes(b"C" * 100)

    top = find_largest_files(r, 2)
    assert top[0] == ("large.txt", 500)
    assert top[1] == ("med.txt", 100)
```
</details>

### Challenge 4 (Medium): Directory Tree Size Calculator
Implement a function `get_directory_size_bytes(dir_path: Path) -> int` that recursively sums the byte sizes of all files contained within `dir_path` using `rglob("*")`.
<details>
<summary><strong>Solution</strong></summary>

```python
from pathlib import Path
import tempfile

def get_directory_size_bytes(dir_path: Path) -> int:
    return sum(
        p.stat().st_size
        for p in dir_path.rglob("*")
        if p.is_file()
    )

with tempfile.TemporaryDirectory() as tmp:
    root = Path(tmp)
    (root / "f1.bin").write_bytes(b"12345")
    sub = root / "subdir"
    sub.mkdir()
    (sub / "f2.bin").write_bytes(b"67890")

    assert get_directory_size_bytes(root) == 10
```
</details>

### Challenge 5 (Hard): Path Traversal Jail Security Guard
Write a security function `is_safe_child_path(base_dir: Path, user_provided_rel_path: str) -> Path | None` that resolves `base_dir / user_provided_rel_path`. If the resolved path attempts path traversal to escape `base_dir` (e.g. using `../../etc/passwd`), return `None`; otherwise, return the resolved `Path`.
<details>
<summary><strong>Solution</strong></summary>

```python
from pathlib import Path
import tempfile

def is_safe_child_path(base_dir: Path, user_path: str) -> Path | None:
    canonical_base = base_dir.resolve()
    target = (canonical_base / user_path).resolve()
    try:
        # relative_to raises ValueError if target is outside canonical_base!
        target.relative_to(canonical_base)
        return target
    except ValueError:
        return None

with tempfile.TemporaryDirectory() as tmp:
    base = Path(tmp) / "safe_jail"
    base.mkdir()
    
    # Safe child
    safe = is_safe_child_path(base, "uploads/avatar.png")
    assert safe is not None
    assert safe == base.resolve() / "uploads" / "avatar.png"
    
    # Malicious traversal attack
    attack = is_safe_child_path(base, "../../../etc/shadow")
    assert attack is None
```
</details>

### Challenge 6 (Hard): Synchronize Mirror Directories
Write a function `mirror_directory_tree(src: Path, dst: Path) -> int` that replicates the directory structure and copies all files from `src` to `dst`. If a file already exists in `dst` with the exact same byte size and modification timestamp (`st_mtime`), skip copying it. Return the count of newly copied files.
<details>
<summary><strong>Solution</strong></summary>

```python
from pathlib import Path
import shutil
import tempfile

def mirror_directory_tree(src: Path, dst: Path) -> int:
    copied_count = 0
    for src_file in src.rglob("*"):
        if src_file.is_file():
            rel_path = src_file.relative_to(src)
            target = dst / rel_path
            
            # Check if up-to-date
            if target.is_file():
                s_stat = src_file.stat()
                t_stat = target.stat()
                if s_stat.st_size == t_stat.st_size and s_stat.st_mtime <= t_stat.st_mtime:
                    continue  # Already up to date
            
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src_file, target)
            copied_count += 1
            
    return copied_count

with tempfile.TemporaryDirectory() as tmp:
    s_root = Path(tmp) / "src"
    d_root = Path(tmp) / "dst"
    s_root.mkdir()
    (s_root / "doc.txt").write_text("v1", encoding="utf-8")
    
    c1 = mirror_directory_tree(s_root, d_root)
    assert c1 == 1
    assert (d_root / "doc.txt").read_text(encoding="utf-8") == "v1"
    
    # Second run without changes copies 0 files
    c2 = mirror_directory_tree(s_root, d_root)
    assert c2 == 0
```
</details>
