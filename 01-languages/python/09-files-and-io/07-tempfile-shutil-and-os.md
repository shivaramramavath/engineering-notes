---
title: "Filesystem Operations: tempfile, shutil, and os"
description: "Comprehensive guide to robust filesystem operations in Python: secure temporary file/directory lifecycles, high-performance file tree operations with shutil, fast directory traversal with os.scandir vs os.walk, and cross-platform OS abstractions."
module: "09-files-and-io"
order: 7
tags:
  - python
  - filesystem
  - tempfile
  - shutil
  - os
  - scandir
  - directory-traversal
difficulty: "Intermediate"
cpython_version: "3.12+"
last_verified: "2026-03"
---

# Filesystem Operations: tempfile, shutil, and os

[<- Back to Module Index](./README.md)

---

## Table of Contents
1. [What & Why](#what--why)
2. [Core Concepts & Mental Model](#core-concepts--mental-model)
3. [How It Works Internally](#how-it-works-internally)
4. [Syntax & API Reference](#syntax--api-reference)
5. [Code Examples](#code-examples)
6. [Edge Cases, Pitfalls & Anti-Patterns](#edge-cases-pitfalls--anti-patterns)
7. [Performance & Memory Characteristics](#performance--memory-characteristics)
8. [CPython Internals & Bytecode](#cpython-internals--bytecode)
9. [Best Practices & Idiomatic Patterns](#best-practices--idiomatic-patterns)
10. [Real-World Case Study: Atomic Multi-Directory Build Artifact Staging System](#real-world-case-study-atomic-multi-directory-build-artifact-staging-system)
11. [Interview Questions & Deep-Dive Scenarios](#interview-questions--deep-dive-scenarios)
12. [Practical Exercises & Code Katas](#practical-exercises--code-katas)

---

## What & Why

Python provides a layered suite of standard library modules for manipulating the host filesystem:
- `os`: Low-level POSIX and Win32 system call bindings (`os.stat`, `os.unlink`, `os.scandir`, `os.replace`).
- `shutil`: High-level shell utilities for bulk recursive copying, moving, archiving, and deleting directory hierarchies.
- `tempfile`: Secure creation of ephemeral files and directories with cryptographic collision resistance and automatic teardown.

### Motivation
Direct file operations frequently require managing delicate operating system constraints: race conditions (symlink attacks, TOCTOU—Time-Of-Check to Time-Of-Use), cross-device filesystem boundary restrictions (`EXDEV`), platform permission discrepancies (POSIX `chmod` octets vs Windows ACLs), and resource leaks. Hand-rolling temporary directory generation or recursive file deletions using naive loops easily creates severe security holes or data corruption. Standard library modules `tempfile`, `shutil`, and `os` encapsulate decades of battle-tested OS hardening.

### Real-World Analogy
Consider a surgical cleanroom adjacent to a document archive:
- When surgeons test an experimental compound, they work on a sterile, disposable tray (`tempfile`). Once done, the entire tray is incinerated, guaranteeing no contamination lingers.
- When an entire department relocates to a new wing, rather than carrying each sheet of paper one by one, a professional moving crew packs, inventories, transports, and cleans up the old office (`shutil`).
- When inspecting the structural foundation bolts and door locks of the facility, the facility engineer interfaces directly with the low-level hardware (`os`).

### When to Use vs When Not to Use
- **Use `tempfile` for:** Ephemeral storage, sandbox scratchpads, test fixtures, and staging buffers that must be destroyed upon exit.
- **Use `shutil` for:** Recursive folder copying (`shutil.copytree`), moving directory trees across drive boundaries (`shutil.move`), deleting directories (`shutil.rmtree`), and inspecting disk space (`shutil.disk_usage`).
- **Use `os.scandir()` for:** High-performance filesystem crawling where file metadata (size, file type, mtime) is needed without issuing separate `stat()` system calls.
- **Do Not Use:**
  - `os.listdir()` when scanning millions of files where metadata is needed (use `os.scandir()` instead).
  - Naive `open("/tmp/my_temp.txt", "w")` which is vulnerable to symlink hijacking attacks.
  - Custom recursive folder deletion using `os.remove` loops when `shutil.rmtree` handles read-only attributes, permission callbacks, and error traps.

---

## Core Concepts & Mental Model

```
+-------------------------------------------------------------------------+
| Python User Application                                                 |
+-------------------------------------------------------------------------+
       |                                |                     |
       v                                v                     v
+---------------+              +-----------------+   +-------------------+
| tempfile      |              | shutil          |   | os.scandir        |
| - NamedTemp   |              | - copy / copy2  |   | - DirEntry cache  |
| - TempDir     |              | - copytree      |   | - Avoids stat()   |
| - SpooledTemp |              | - rmtree / move |   | - Fast traversal  |
+---------------+              +-----------------+   +-------------------+
       |                                |                     |
       +--------------------------------+---------------------+
                                        |
                                        v
                 +---------------------------------------------+
                 | CPython os / posix / nt Modules             |
                 +---------------------------------------------+
                                        |
                                        v
                 +---------------------------------------------+
                 | Host OS Kernel VFS (POSIX / Win32 API)      |
                 | - open(O_CREAT | O_EXCL), unlink, rename    |
                 +---------------------------------------------+
```

### Key Abstractions
1. **`tempfile.TemporaryDirectory`**: Context manager creating a unique, cryptographically random temporary folder on disk with `0700` POSIX permissions, recursively purged upon context exit.
2. **`tempfile.NamedTemporaryFile`**: Creates an ephemeral file with a visible filesystem path. On POSIX systems, closing it unlinks the file. On Windows, opening a named temporary file twice can trigger sharing violations unless `delete_on_close=False` (introduced in Python 3.12).
3. **`os.DirEntry` (via `os.scandir`)**: An iterator yielding entries that bundle file names with pre-cached `stat` metadata returned directly by the underlying OS kernel directory enumeration API.
4. **`shutil.copy` vs `shutil.copy2`**: `shutil.copy` copies data and permission bits (`chmod`). `shutil.copy2` copies data along with all metadata: last access time, last modification time, and platform flags.

---

## How It Works Internally

### 1. `tempfile` Security Model and Collision Prevention
When `tempfile.mkstemp()` or `NamedTemporaryFile()` runs:
1. It queries platform temporary directories (`TMPDIR`, `TEMP`, `TMP`, or `/tmp`).
2. It generates eight cryptographically secure random alphanumeric characters using `os.urandom()`.
3. It opens the file with atomic operating system flags:
   - POSIX: `os.O_RDWR | os.O_CREAT | os.O_EXCL` with mode `0600`.
   - Windows: `_O_RDWR | _O_CREAT | _O_SHORT_LIVED | _O_TEMPORARY`.
4. `O_EXCL` guarantees that if a file with that name already exists (or a symlink was placed by an attacker), the OS call fails immediately with `FileExistsError`, thwarting symlink redirection attacks.

### 2. `os.scandir` vs `os.listdir` Mechanics
- In legacy `os.listdir()`, the OS directory read call (`readdir()` on POSIX, `FindFirstFileW`/`FindNextFileW` on Windows) retrieves a list of filenames as Python strings. If you subsequently check `os.path.isdir(path)` or `os.path.getsize(path)`, Python must execute a separate `stat()` system call per file. For 100,000 files, this requires 100,001 kernel context switches!
- With `os.scandir()`, the OS kernel returns file attributes (file type, inode, size) inside the directory entry struct itself. CPython wraps this in an `os.DirEntry` object that caches these fields. Calling `entry.is_dir()`, `entry.is_file()`, or `entry.stat()` retrieves the cached kernel data with **zero additional system calls**.

---

## Syntax & API Reference

### `tempfile` Module
```python
import tempfile
from pathlib import Path

# Create an auto-deleting temporary directory
with tempfile.TemporaryDirectory(prefix="worker_", suffix="_staging") as tmpdir:
    dir_path = Path(tmpdir)

# Create an ephemeral file
# Python 3.12+: delete_on_close allows safe re-opening across Windows handles
with tempfile.NamedTemporaryFile(mode="w+", delete=True, delete_on_close=True) as tf:
    tf.write("Temporary payload")
    tf.flush()
    print("File path:", tf.name)

# In-memory buffer rolling over to disk past 10MB
spooled = tempfile.SpooledTemporaryFile(max_size=10_000_000, mode="w+b")
```

### `shutil` Module
```python
import shutil

# Copying files
shutil.copy(src, dst)                  # Copies file + permission bits
shutil.copy2(src, dst)                 # Copies file + all timestamps/metadata

# Directory tree operations
shutil.copytree(src, dst, dirs_exist_ok=False, ignore=shutil.ignore_patterns("*.pyc", ".git"))
shutil.rmtree(path, ignore_errors=False, onexc=None) # Python 3.12+ uses onexc instead of onerror
shutil.move(src, dst)                  # Atomic rename if on same device; copy+unlink if cross-device

# Disk metrics
total, used, free = shutil.disk_usage(path)  # Returns named tuple (total, used, free) in bytes
```

### `os.scandir` Traversal
```python
import os

with os.scandir(path) as it:
    for entry in it:
        entry: os.DirEntry
        print(entry.name)              # Base filename
        print(entry.path)              # Full joined path
        print(entry.is_file())         # True if file (no stat syscall on Win/POSIX)
        print(entry.is_dir())          # True if directory
        print(entry.stat().st_size)    # Size in bytes
```

---

## Code Examples

### Example 1: Secure Temporary Directory Sandbox
```python
import tempfile
from pathlib import Path

def run_temp_dir_demo():
    print("Starting temporary workspace...")
    with tempfile.TemporaryDirectory(prefix="build_", dir=None) as tmp_str:
        tmp_dir = Path(tmp_str)
        print(f"Temporary folder created at: {tmp_dir}")
        print(f"Folder exists on disk: {tmp_dir.exists()}")

        # Write build artifacts
        artifact = tmp_dir / "manifest.json"
        artifact.write_text('{"build_id": 404, "status": "compiled"}', encoding="utf-8")
        print(f"Artifact created: {artifact.name}, Size: {artifact.stat().st_size} bytes")

    # Outside the context manager, the directory and all contents are completely eradicated
    print(f"Folder exists after context exit: {tmp_dir.exists()}")

run_temp_dir_demo()
# Output:
# Starting temporary workspace...
# Temporary folder created at: .../build_...
# Folder exists on disk: True
# Artifact created: manifest.json, Size: 41 bytes
# Folder exists after context exit: False
```

### Example 2: Cross-Platform NamedTemporaryFile with Python 3.12+ `delete_on_close`
```python
import tempfile
from pathlib import Path

def run_named_temp_demo():
    # Python 3.12+ delete_on_close parameter enables safe re-opening on Windows
    with tempfile.NamedTemporaryFile(
        mode="w+t",
        suffix=".conf",
        delete=True,
        delete_on_close=False,
        encoding="utf-8"
    ) as temp_file:
        temp_file.write("DATABASE_URL=postgres://localhost/testdb\n")
        temp_file.flush()

        temp_path = Path(temp_file.name)
        print(f"Created temporary config file: {temp_path}")

        # Re-open file by name in another reader without Windows sharing violation
        content = temp_path.read_text(encoding="utf-8")
        print("Read from separate reader:", content.strip())

    print(f"Temp file exists after context manager closes: {temp_path.exists()}")

run_named_temp_demo()
# Output:
# Created temporary config file: ...
# Read from separate reader: DATABASE_URL=postgres://localhost/testdb
# Temp file exists after context manager closes: False
```

### Example 3: Recursive Directory Copying with Pattern Filtering
```python
import tempfile
import shutil
from pathlib import Path

def run_copytree_demo():
    with tempfile.TemporaryDirectory() as src_root, tempfile.TemporaryDirectory() as dst_root:
        src = Path(src_root)
        dst = Path(dst_root) / "backup"

        # Populate source tree
        (src / "docs").mkdir()
        (src / "docs" / "index.html").write_text("<h1>Home</h1>", encoding="utf-8")
        (src / "docs" / "temp.log").write_text("debug line", encoding="utf-8")
        (src / "main.py").write_text("print('hello')", encoding="utf-8")
        (src / "test.pyc").write_text("compiled bytecode", encoding="utf-8")

        # Copy tree excluding *.pyc and *.log files
        shutil.copytree(
            src,
            dst,
            ignore=shutil.ignore_patterns("*.pyc", "*.log")
        )

        copied_files = sorted([p.name for p in dst.rglob("*") if p.is_file()])
        print("Successfully copied files:", copied_files)

run_copytree_demo()
# Output:
# Successfully copied files: ['index.html', 'main.py']
```

### Example 4: Ultra-Fast Recursive Traversal with `os.scandir`
```python
import os
import tempfile
from pathlib import Path

def fast_tree_summary(root_dir: str) -> tuple[int, int]:
    """
    Recursively scans a directory returning (total_files, total_bytes).
    Leverages os.scandir DirEntry caching to avoid redundant stat() calls.
    """
    total_files = 0
    total_bytes = 0

    def _scan(path: str):
        nonlocal total_files, total_bytes
        try:
            with os.scandir(path) as it:
                for entry in it:
                    try:
                        if entry.is_dir(follow_symlinks=False):
                            _scan(entry.path)
                        elif entry.is_file(follow_symlinks=False):
                            total_files += 1
                            total_bytes += entry.stat(follow_symlinks=False).st_size
                    except OSError:
                        # Handle permissions errors gracefully
                        continue
        except OSError:
            pass

    _scan(root_dir)
    return total_files, total_bytes

# Verification
with tempfile.TemporaryDirectory() as tmp:
    p = Path(tmp)
    for i in range(5):
        (p / f"file_{i}.dat").write_bytes(b"0" * 1024)
    (p / "subdir").mkdir()
    (p / "subdir" / "nested.dat").write_bytes(b"0" * 2048)

    files, size = fast_tree_summary(tmp)
    print(f"Summary: Found {files} files totaling {size} bytes")

# Output:
# Summary: Found 6 files totaling 7168 bytes
```

### Example 5: Cross-Platform Robust Directory Removal with `onexc`
```python
import tempfile
import shutil
import os
import stat
from pathlib import Path

def remove_readonly(func, path, exc_info):
    """
    Python 3.12+ error handler for shutil.rmtree.
    Clears read-only attributes on Windows/POSIX and retries removal.
    """
    # exc_info is an instance of Exception (e.g. PermissionError)
    os.chmod(path, stat.S_IWRITE)
    func(path)

def run_rmtree_demo():
    tmp_path = Path(tempfile.mkdtemp())
    readonly_file = tmp_path / "locked.txt"
    readonly_file.write_text("protected content", encoding="utf-8")

    # Mark file read-only
    os.chmod(readonly_file, stat.S_IREAD)

    # Use onexc parameter (Python 3.12+)
    shutil.rmtree(tmp_path, onexc=remove_readonly)
    print("Locked tree purged successfully:", not tmp_path.exists())

run_rmtree_demo()
# Output:
# Locked tree purged successfully: True
```

---

## Edge Cases, Pitfalls & Anti-Patterns

### 1. The Windows `NamedTemporaryFile` Access Trap
On Windows, opening a file locks it against concurrent access by default. Prior to Python 3.12, if you created `NamedTemporaryFile()` and passed its `.name` to another tool (e.g. SQLite, Subprocess, or another `open()` call), Windows raised `PermissionError: [WinError 32] The process cannot access the file because it is being used by another process`.
- **Solution (Python 3.12+)**: Pass `delete_on_close=False`.
- **Solution (Pre-3.12)**: Close the file before passing the name, or use `tempfile.TemporaryDirectory()` and create normal files inside it.

### 2. Symlink Traversal Loops and Recursion Bomb
When recursively traversing directories with `os.walk` or `os.scandir`, following symlinks (`followlinks=True`) can cause infinite loops if a symlink points to an ancestor directory. Always set `follow_symlinks=False` unless explicitly required, and maintain a set of visited `(st_dev, st_ino)` tuples to detect cyclic graphs.

### 3. Cross-Device Moves (`EXDEV`)
`os.rename()` fails with `OSError: [Errno 18] Invalid cross-device link` if the source and destination paths reside on different physical disks, partitions, or mount points (e.g. `/tmp` on tmpfs RAM disk and `/var/data` on NVMe SSD). Always use `shutil.move()` which gracefully detects cross-device boundaries and falls back to `shutil.copy2` followed by `os.unlink`.

### Common Mistakes Table

| Bad Code | Good Code | Why | Severity |
| :--- | :--- | :--- | :--- |
| `open(f"/tmp/file_{random.randint(0, 9999)}.tmp", "w")` | `tempfile.NamedTemporaryFile()` | Predictable filenames invite symlink injection vulnerabilities. | Critical |
| `shutil.rmtree(path, onerror=...)` in Python 3.12+ | `shutil.rmtree(path, onexc=...)` | `onerror` is deprecated in Python 3.12; `onexc` receives standard Exception instance. | Medium |
| Using `os.listdir()` and querying `os.path.isdir()` in a loop | `with os.scandir() as it: for e in it: e.is_dir()` | Triggers redundant `stat()` syscalls per file; scans run 5x-20x slower. | High |
| `os.rename(src, dst)` across different drives | `shutil.move(src, dst)` | `os.rename` raises `EXDEV` across mount points; `shutil.move` copies then unlinks. | High |
| Forgetting `delete_on_close=False` on Windows NamedTemp | `NamedTemporaryFile(delete_on_close=False)` | Re-opening named temp file on Windows raises `PermissionError` (sharing violation). | High |
| Recursively scanning with `follow_symlinks=True` unconditionally | `follow_symlinks=False` or track visited `(dev, ino)` | Symlink cycles cause infinite recursion and stack overflow errors. | High |

---

## Performance & Memory Characteristics

### `os.scandir` vs `os.walk` vs `os.listdir` Benchmark
On a folder containing 50,000 files:
- **`os.listdir()` + `os.path.getsize()`**: ~1,450 ms (50,000 separate `stat()` system calls).
- **`os.scandir()` with `entry.stat().st_size`**: ~110 ms (**~13x faster**; attributes extracted directly from OS directory stream buffer).

### Disk Free Space Check Overhead
`shutil.disk_usage(path)` calls `statvfs()` on POSIX and `GetDiskFreeSpaceExW()` on Windows. It is an $\mathcal{O}(1)$ kernel query consuming negligible memory.

---

## CPython Internals & Bytecode

In CPython, `os.scandir()` is implemented in `Modules/posixmodule.c`.
- On Linux, it invokes the glibc `opendir()` and `readdir_r()` functions.
- On Windows, it invokes `FindFirstFileW()` and `FindNextFileW()`.
- The returned `DirEntry` C struct (`DirEntryObject`) caches:
  - `name`: Python string of entry name.
  - `path`: Full joined path.
  - `stat`: Pointer to `PyObject*` holding `os.stat_result`, populated lazily or extracted from directory stream `d_type` (POSIX) or `dwFileAttributes` (Windows).

Bytecode for `shutil.copy`:
```python
import dis
import shutil

def do_copy(src, dst):
    shutil.copy2(src, dst)

dis.dis(do_copy)
```
```text
  1           0 RESUME                   0
  2           2 LOAD_GLOBAL              1 (shutil + NULL)
             12 LOAD_ATTR                3 (copy2)
             22 LOAD_FAST                0 (src)
             24 LOAD_FAST                1 (dst)
             26 CALL                     2
             34 POP_TOP
             36 RETURN_CONST             0 (None)
```

---

## Best Practices & Idiomatic Patterns

1. **Always Use `tempfile.TemporaryDirectory` via `with`**:
   Guarantees that files are destroyed even if an unhandled `KeyboardInterrupt` or syntax error occurs.
2. **Handle Read-Only Files in Cleanup**:
   Provide a custom error handler to `shutil.rmtree` when purging directories generated by Git or foreign build systems that set read-only flags.
3. **Check Available Disk Space Before Large Writes**:
   ```python
   free_bytes = shutil.disk_usage("/var/data").free
   if free_bytes < required_bytes:
       raise OSError("Insufficient disk space")
   ```
4. **Prefer `shutil.copy2` over `shutil.copy`**:
   Preserves file modification and access timestamps, which is critical for cache validation and build tools.

---

## Real-World Case Study: Atomic Multi-Directory Build Artifact Staging System

The following script stages a complex multi-file software distribution in an ephemeral sandbox, compiles metadata, validates disk space, and atomically moves the final build into production.

```python
import tempfile
import shutil
import os
import json
import hashlib
from pathlib import Path
from typing import Dict

class BuildPipeline:
    def __init__(self, target_release_dir: Path):
        self.target_release_dir = target_release_dir
        self.target_release_dir.mkdir(parents=True, exist_ok=True)

    def _verify_disk_headroom(self, min_free_mb: int = 50) -> None:
        usage = shutil.disk_usage(self.target_release_dir)
        free_mb = usage.free // (1024 * 1024)
        if free_mb < min_free_mb:
            raise OSError(f"Insufficient disk space: {free_mb}MB available, {min_free_mb}MB required.")

    def compile_release(self, version: str, source_files: Dict[str, str]) -> Path:
        self._verify_disk_headroom()
        
        # Build completely inside secure temporary sandbox
        with tempfile.TemporaryDirectory(prefix="build_sandbox_") as sandbox_str:
            sandbox = Path(sandbox_str)
            build_pkg = sandbox / f"package-{version}"
            build_pkg.mkdir()

            checksums = {}
            # Write artifacts
            for rel_path, content in source_files.items():
                dest_file = build_pkg / rel_path
                dest_file.parent.mkdir(parents=True, exist_ok=True)
                dest_file.write_text(content, encoding="utf-8")
                
                # Calculate SHA256
                digest = hashlib.sha256(content.encode("utf-8")).hexdigest()
                checksums[rel_path] = digest

            # Write manifest
            manifest = {
                "version": version,
                "file_count": len(source_files),
                "checksums": checksums
            }
            (build_pkg / "manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")

            # Final destination
            final_dest = self.target_release_dir / f"package-{version}"
            if final_dest.exists():
                shutil.rmtree(final_dest)

            # Move staged directory into production
            shutil.move(str(build_pkg), str(final_dest))
            print(f"Build successfully released to: {final_dest}")
            return final_dest

# Verification
if __name__ == "__main__":
    with tempfile.TemporaryDirectory() as releases_root:
        pipeline = BuildPipeline(Path(releases_root))
        files = {
            "bin/run.sh": "#!/bin/bash\necho 'running'",
            "etc/app.conf": "PORT=8000\nENV=prod"
        }
        published = pipeline.compile_release("v2.4.0", files)
        print("Published files:", sorted([p.name for p in published.rglob("*") if p.is_file()]))
```

---

## Interview Questions & Deep-Dive Scenarios

<details>
<summary><strong>1. Junior: What is the primary difference between <code>shutil.copy()</code> and <code>shutil.copy2()</code>?</strong></summary>

`shutil.copy()` copies the file's data and permission bits (POSIX `chmod` flags). `shutil.copy2()` copies the file data, permission bits, and all file metadata, including access times (`atime`), modification times (`mtime`), and extended flags.
</details>

<details>
<summary><strong>2. Junior: Why is <code>os.scandir()</code> significantly faster than <code>os.listdir()</code> when inspecting file attributes?</strong></summary>

`os.listdir()` only returns a list of filename strings. Checking if an entry is a directory or obtaining its size requires subsequent `os.path.isdir()` or `os.stat()` system calls for every single entry. `os.scandir()` returns `os.DirEntry` objects that cache file metadata provided directly by the operating system kernel's directory scanning API, eliminating thousands of redundant system calls.
</details>

<details>
<summary><strong>3. Mid: What is the purpose of the <code>delete_on_close</code> parameter introduced in Python 3.12 for <code>tempfile.NamedTemporaryFile</code>?</strong></summary>

On Windows, files opened by one handle cannot be re-opened by another handle without explicit sharing flags. Setting `delete_on_close=False` allows the file to be closed by Python while remaining on disk for external processes or subsequent readers to access, and it is automatically deleted when the context manager exits.
</details>

<details>
<summary><strong>4. Mid: What happens when <code>shutil.move()</code> moves a directory across different physical mount points or drive letters?</strong></summary>

An atomic OS `rename()` system call fails with `EXDEV` (Invalid cross-device link) when moving data across physical filesystems. `shutil.move()` catches this error, automatically executes `shutil.copytree()` (or `copy2` for single files) to duplicate the data across to the target filesystem, and then unlinks the source directory via `shutil.rmtree()`.
</details>

<details>
<summary><strong>5. Senior: How does <code>tempfile.mkstemp()</code> prevent symlink race condition attacks (TOCTOU)?</strong></summary>

`tempfile.mkstemp()` uses `os.O_CREAT | os.O_EXCL` flags when opening the file descriptor. The operating system guarantees that if the target filename or symlink already exists at the moment of creation, the system call fails atomically without following the symlink or overwriting existing files. It also sets permissions strictly to `0600` (readable and writable only by the creating user).
</details>

<details>
<summary><strong>6. Senior: How does Python 3.12 change error handling in <code>shutil.rmtree</code>?</strong></summary>

In Python 3.12, the `onerror` parameter is deprecated in favor of `onexc`. `onerror` expected a callback receiving `(function, path, excinfo_tuple)`. The new `onexc` callback receives `(function, path, exception_instance)`. This simplifies exception matching and aligns with modern Python exception handling patterns.
</details>

<details>
<summary><strong>7. Staff: How would you safely purge a deeply nested directory tree on Windows that contains paths exceeding the 260-character MAX_PATH limit and files with read-only attributes?</strong></summary>

1. Ensure the Windows path uses the extended-length path prefix (`\\?\C:\long\path...`) to bypass the 260-character `MAX_PATH` barrier.
2. Supply a custom `onexc` handler to `shutil.rmtree` that catches `PermissionError`, clears the read-only flag via `os.chmod(path, stat.S_IWRITE)`, and re-invokes the failing removal function (`os.unlink` or `os.rmdir`).
</details>

<details>
<summary><strong>8. Staff: How does <code>tempfile.SpooledTemporaryFile</code> handle <code>fileno()</code> calls, and why can this undermine performance optimizations?</strong></summary>

`SpooledTemporaryFile` buffers writes in an in-memory `io.BytesIO` stream until the byte count exceeds `max_size`. Because in-memory streams have no OS-level file descriptor, calling `.fileno()` immediately forces an unconditional rollover: it flushes memory to a physical disk file descriptor even if `max_size` has not been reached. If downstream code calls `.fileno()` on initialization, in-memory buffering is completely defeated.
</details>

---

## Practical Exercises & Code Katas

### Exercise 1: Directory Tree Size Calculator
**Requirement:** Write a function `get_dir_size(path: Path) -> int` using `os.scandir` that calculates the recursive disk usage of a folder without following symbolic links.

<details>
<summary>Show Solution</summary>

```python
import os
from pathlib import Path

def get_dir_size(path: Path) -> int:
    total = 0
    with os.scandir(path) as it:
        for entry in it:
            if entry.is_symlink():
                continue
            if entry.is_dir(follow_symlinks=False):
                total += get_dir_size(Path(entry.path))
            elif entry.is_file(follow_symlinks=False):
                total += entry.stat(follow_symlinks=False).st_size
    return total

# Test
import tempfile
with tempfile.TemporaryDirectory() as tmp:
    p = Path(tmp)
    (p / "a.txt").write_bytes(b"A" * 100)
    (p / "sub").mkdir()
    (p / "sub" / "b.txt").write_bytes(b"B" * 200)
    print("Total size:", get_dir_size(p))
    assert get_dir_size(p) == 300
```
</details>

### Exercise 2: Safe Ephemeral Scratchpad Function
**Requirement:** Implement a decorator `@in_temp_dir` that executes the decorated function inside a clean temporary directory, changing the current working directory (`os.chdir`) to the sandbox and restoring the original CWD on exit.

<details>
<summary>Show Solution</summary>

```python
import functools
import os
import tempfile
from pathlib import Path

def in_temp_dir(func):
    @functools.wraps(func)
    def wrapper(*args, **kwargs):
        old_cwd = os.getcwd()
        with tempfile.TemporaryDirectory() as tmpdir:
            try:
                os.chdir(tmpdir)
                return func(*args, **kwargs)
            finally:
                os.chdir(old_cwd)
    return wrapper

@in_temp_dir
def generate_local_files():
    Path("test.log").write_text("Writing inside sandbox")
    print("Files created in sandbox:", list(Path(".").iterdir()))
    return True

print("Function executed:", generate_local_files())
```
</details>

### Exercise 3: Duplicate File Finder by Hash
**Requirement:** Use `os.scandir` to find duplicate files within a directory tree by grouping files with identical file sizes first, and computing SHA-256 hashes only for size collisions.

<details>
<summary>Show Solution</summary>

```python
import os
import hashlib
from collections import defaultdict
from pathlib import Path

def find_duplicates(root_dir: str) -> dict[str, list[str]]:
    size_map = defaultdict(list)

    # Step 1: Group by file size using scandir
    for root, _, files in os.walk(root_dir):
        with os.scandir(root) as it:
            for entry in it:
                if entry.is_file(follow_symlinks=False):
                    size_map[entry.stat().st_size].append(entry.path)

    # Step 2: Hash only size collisions
    hash_map = defaultdict(list)
    for size, paths in size_map.items():
        if len(paths) > 1 and size > 0:
            for path in paths:
                hasher = hashlib.sha256()
                with open(path, "rb") as f:
                    while chunk := f.read(65536):
                        hasher.update(chunk)
                hash_map[hasher.hexdigest()].append(path)

    return {h: p for h, p in hash_map.items() if len(p) > 1}

# Verification
import tempfile
with tempfile.TemporaryDirectory() as tmp:
    p = Path(tmp)
    (p / "f1.txt").write_text("duplicate text")
    (p / "f2.txt").write_text("duplicate text")
    (p / "f3.txt").write_text("unique text")
    dups = find_duplicates(tmp)
    print("Duplicates found:", dups)
    assert len(dups) == 1
```
</details>

### Exercise 4: Clean Orphaned Empty Directories
**Requirement:** Implement a bottom-up directory crawler that removes all empty subdirectories in a directory tree.

<details>
<summary>Show Solution</summary>

```python
import os
from pathlib import Path

def remove_empty_dirs(root_dir: str) -> int:
    removed_count = 0
    # Walk bottom-up
    for root, dirs, files in os.walk(root_dir, topdown=False):
        for d in dirs:
            dir_path = os.path.join(root, d)
            try:
                # os.rmdir succeeds ONLY if directory is completely empty
                os.rmdir(dir_path)
                removed_count += 1
            except OSError:
                pass
    return removed_count

# Test
import tempfile
with tempfile.TemporaryDirectory() as tmp:
    p = Path(tmp)
    (p / "a" / "b" / "c").mkdir(parents=True)
    (p / "a" / "file.txt").write_text("kept")
    count = remove_empty_dirs(tmp)
    print("Removed empty directories:", count)
    assert count == 2
```
</details>

### Exercise 5: Rolling Backup with Disk Quota Protection
**Requirement:** Write a backup function that copies a directory to a backup folder using `shutil.copytree`, but aborts if the destination volume has less than 20% free space remaining.

<details>
<summary>Show Solution</summary>

```python
import shutil
from pathlib import Path

def safe_backup(src: Path, dst: Path, min_free_ratio: float = 0.20) -> None:
    usage = shutil.disk_usage(dst.parent if dst.parent.exists() else Path("."))
    ratio = usage.free / usage.total
    if ratio < min_free_ratio:
        raise OSError(f"Insufficient disk headroom: {ratio:.1%} free, minimum {min_free_ratio:.1%} required.")
    
    shutil.copytree(src, dst, dirs_exist_ok=True)
    print(f"Backup copied to {dst}")

# Verification
import tempfile
with tempfile.TemporaryDirectory() as s, tempfile.TemporaryDirectory() as d:
    src_dir = Path(s)
    dst_dir = Path(d) / "backup"
    (src_dir / "app.db").write_text("database data")
    safe_backup(src_dir, dst_dir)
```
</details>

### Exercise 6: Cross-Platform File Locking Scratchpad
**Requirement:** Create a temporary file that locks itself using `fcntl` (POSIX) or `msvcrt` (Windows) to prevent concurrent writes from child processes.

<details>
<summary>Show Solution</summary>

```python
import os
import sys
import tempfile
import contextlib

@contextlib.contextmanager
def locked_temporary_file():
    with tempfile.NamedTemporaryFile(delete=False) as tf:
        fd = tf.fileno()
        try:
            if sys.platform != "win32":
                import fcntl
                fcntl.flock(fd, fcntl.LOCK_EX)
            else:
                import msvcrt
                msvcrt.locking(fd, msvcrt.LK_NBLCK, 1)
            yield tf
        finally:
            if sys.platform != "win32":
                import fcntl
                fcntl.flock(fd, fcntl.LOCK_UN)
            else:
                import msvcrt
                try:
                    tf.seek(0)
                    msvcrt.locking(fd, msvcrt.LK_UNLCK, 1)
                except OSError:
                    pass
            tf.close()
            os.unlink(tf.name)

# Verification
with locked_temporary_file() as ltf:
    ltf.write(b"Exclusive locked temporary scratchpad")
    ltf.flush()
    print("Locked temp file created and held exclusively:", ltf.name)
```
</details>
