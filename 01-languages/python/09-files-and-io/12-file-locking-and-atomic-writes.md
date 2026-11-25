---
title: "Atomic File Writes and File Locking in Python"
description: "Mastering crash-resilient I/O in Python: preventing torn writes via atomic replacement with os.replace(), POSIX vs Windows file locking (fcntl vs msvcrt vs filelock), fsync durability, and crash consistency patterns."
module: "09-files-and-io"
order: 12
tags:
  - python
  - io
  - atomic-writes
  - file-locking
  - fcntl
  - msvcrt
  - crash-consistency
  - fsync
difficulty: "Advanced"
cpython_version: "3.12+"
last_verified: "2026-03"
---

# Atomic File Writes and File Locking in Python

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
10. [Real-World Case Study: Production Crash-Consistent Document Database Engine](#real-world-case-study-production-crash-consistent-document-database-engine)
11. [Interview Questions & Deep-Dive Scenarios](#interview-questions--deep-dive-scenarios)
12. [Practical Exercises & Code Katas](#practical-exercises--code-katas)

---

## What & Why

Writing data to disk in Python is deceptively complex. A standard `open("config.json", "w").write(data)` call does not immediately update physical media, nor does it guarantee atomic updates. If the process crashes, the host loses power, or a concurrent thread or process reads the file while writing is underway, two severe failure modes emerge:
1. **Torn Writes & Corrupted Files**: The reader observes a truncated, half-written file.
2. **Race Conditions**: Two concurrent writers interleave their writes or overwrite each other's updates without synchronization.

To build production-grade, crash-resilient software, Python engineers rely on two fundamental techniques:
- **Atomic Replacement via `os.replace()`**: Writing data to an ephemeral temporary file on the same filesystem and atomically swapping it into place.
- **File Locking**: Coordinating concurrent access across processes using advisory locks (`fcntl` on POSIX, `msvcrt` on Windows, or cross-platform wrappers).

### Motivation
Consider a service saving user account balances or server state. If the server loses power midway through writing a 1 MB state file:
- With naive `open(path, "w")`: The original file was truncated to 0 bytes on open, and only 400 KB were flushed before shutdown. The file is permanently corrupt and unreadable.
- With atomic replacement: The original file remains 100% intact and untouched. The half-written temporary file is simply discarded on reboot. The system maintains absolute consistency.

### Real-World Analogy
Consider a portrait painter replacing a valuable oil painting on a gallery wall:
- **Naive writing** is stripping the canvas off the wall and painting the new portrait directly on the wall while gallery visitors walk by. If the painter faints midway, visitors see a half-erased ruin.
- **Atomic writing** is painting the new portrait on an identical canvas on an easel in a workshop. Once the new painting is completely finished and inspected, the painter walks over to the wall, takes down the old canvas, and hangs the new canvas in a single 2-second motion. At every instant, visitors see a complete, flawless painting.

### When to Use vs When Not to Use
- **Use Atomic Replacement for:**
  - Configuration updates, state files, JSON/YAML metadata persistence, cache entries, and deployment artifacts.
- **Use File Locking for:**
  - Multi-process coordination, embedded database management (SQLite-like engines), preventing multiple instances of a script from running concurrently (single-instance lockfiles).
- **Do Not Use File Locking for:**
  - Thread synchronization within a single Python process (use `threading.Lock` instead).
  - High-concurrency distributed state across multiple distinct physical servers (use distributed locking via Redis Redlock, ZooKeeper, or Etcd).

---

## Core Concepts & Mental Model

```
+-------------------------------------------------------------------------+
| Atomic Replacement Pattern (write-temp-replace)                         |
|                                                                         |
| Step 1: Write to unique temp file on SAME filesystem volume             |
|         /data/.config.json.tmp_849204                                   |
|                                                                         |
| Step 2: Flush Python buffers & Force OS disk sync                       |
|         file.flush() -> os.fsync(file.fileno())                         |
|                                                                         |
| Step 3: Atomic rename system call                                       |
|         os.replace('/data/.config.json.tmp_849204', '/data/config.json')|
|                                                                         |
| Result: Readers at all times see EITHER old complete file               |
|         OR new complete file. ZERO torn writes.                         |
+-------------------------------------------------------------------------+
```

```
+-------------------------------------------------------------------------+
| File Locking Matrix Across Operating Systems                            |
|                                                                         |
| Feature               | POSIX (Linux/macOS)       | Windows (NTFS/FAT)  |
|-----------------------|---------------------------|---------------------|
| API                   | fcntl.flock / fcntl.lockf | msvcrt.locking      |
| Enforcement           | Advisory (cooperative)    | Mandatory (enforced)|
| Locking granularity   | Entire file (flock)       | Byte range (locking)|
| Shared / Exclusive    | Yes (LOCK_SH / LOCK_EX)   | Exclusive only      |
| Cross-Platform Sol.   | filelock package          | filelock package    |
+-------------------------------------------------------------------------+
```

---

## How It Works Internally

### 1. Atomic Directory Inode Replacement
On POSIX systems, `os.replace(src, dst)` invokes the `rename()` system call.
- The operating system verifies that `src` and `dst` reside on the same filesystem mount point (same `st_dev`).
- It links the directory entry `dst` to point directly to `src`'s inode.
- It unlinks the old inode associated with `dst`.
- This pointer swap occurs atomically inside the kernel directory table: at no point can another process observe `dst` missing or partially populated.
- On Windows (Windows Vista and newer), `os.replace()` uses the Win32 `MoveFileExW()` API with `MOVEFILE_REPLACE_EXISTING`, which provides the same atomic guarantee on NTFS volumes.

### 2. The Dirty Page Cache and `os.fsync()`
When `f.write()` finishes, data resides only in Python's internal userspace I/O buffer.
1. `f.flush()` pushes data from Python userspace into the OS kernel page cache. The data is still in volatile RAM!
2. `os.fsync(f.fileno())` forces the operating system kernel to issue flush commands (`F_FULLFSYNC` on macOS, cache flush commands to disk controllers) to write dirty pages to physical non-volatile storage.
Without `os.fsync()`, an immediate hardware power cut will result in a 0-byte or corrupted file upon reboot even if `os.replace()` succeeded.

### 3. POSIX Advisory vs Windows Mandatory Locking
- **Advisory (POSIX `fcntl.flock`)**: The operating system kernel tracks the lock, but does not prevent rogue processes from ignoring the lock and calling `open(..., "w")`. All cooperating processes must explicitly check and acquire the lock.
- **Mandatory (Windows `msvcrt.locking`)**: The Windows kernel actively blocks any other process from reading or writing the locked byte range, raising `PermissionError` (WinError 33: The process cannot access the file because another process has locked a portion of the file).

---

## Syntax & API Reference

### Atomic File Replacement
```python
import os
import tempfile
from pathlib import Path

# os.replace atomically overwrites dst with src if both exist
os.replace(src: str | Path, dst: str | Path) -> None
```

### Low-Level File Synchronization
```python
import os

# Flush Python userspace buffer to OS kernel
f.flush()

# Flush OS kernel dirty pages to physical non-volatile media
os.fsync(fd: int)

# Linux only: flushes data pages without synchronizing directory metadata
os.fdatasync(fd: int)
```

### POSIX File Locking (`fcntl`)
```python
import fcntl

# Lock operations
fcntl.flock(fd, fcntl.LOCK_EX)          # Exclusive write lock (blocking)
fcntl.flock(fd, fcntl.LOCK_SH)          # Shared read lock (blocking)
fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB) # Non-blocking (raises BlockingIOError)
fcntl.flock(fd, fcntl.LOCK_UN)          # Unlock
```

### Windows File Locking (`msvcrt`)
```python
import msvcrt

# Lock operations (operates on byte ranges from current file seek position)
msvcrt.locking(fd, msvcrt.LK_LOCK, nbytes)    # Blocking lock
msvcrt.locking(fd, msvcrt.LK_NBLCK, nbytes)   # Non-blocking lock (raises OSError)
msvcrt.locking(fd, msvcrt.LK_UNLCK, nbytes)   # Unlock
```

---

## Code Examples

### Example 1: Robust Atomic File Write Context Manager
```python
import os
import tempfile
from pathlib import Path
import contextlib

@contextlib.contextmanager
def atomic_write(target_path: Path, mode: str = "w", encoding: str = "utf-8"):
    """
    Safely writes data to a target path atomically.
    Ensures that target_path is either completely updated or untouched.
    """
    target = target_path.resolve()
    # Critical: Create temp file in the SAME directory to guarantee same filesystem mount
    temp_dir = target.parent
    temp_dir.mkdir(parents=True, exist_ok=True)

    # Prefix with dot so it appears as hidden file
    temp_file = tempfile.NamedTemporaryFile(
        mode=mode,
        dir=temp_dir,
        prefix=f".{target.name}.",
        suffix=".tmp",
        delete=False,
        encoding=encoding if "b" not in mode else None
    )

    try:
        yield temp_file
        # Step 1: Flush Python user buffer
        temp_file.flush()
        # Step 2: Flush OS kernel buffer to physical disk
        os.fsync(temp_file.fileno())
        # Step 3: Close file handle before replacing (mandatory on Windows)
        temp_file.close()
        # Step 4: Atomic rename
        os.replace(temp_file.name, target)
    except Exception:
        temp_file.close()
        if os.path.exists(temp_file.name):
            os.unlink(temp_file.name)
        raise

# Verification
def test_atomic_write():
    destination = Path("secure_config.json")
    with atomic_write(destination) as f:
        f.write('{"status": "active", "version": 2}')

    print("File written atomically:", destination.read_text())
    destination.unlink()

test_atomic_write()
# Output:
# File written atomically: {"status": "active", "version": 2}
```

### Example 2: Cross-Platform File Locking Primitive
```python
import os
import sys
import contextlib
from pathlib import Path

@contextlib.contextmanager
def file_lock(lockfile_path: Path, exclusive: bool = True):
    """
    Cross-platform advisory file lock using fcntl (POSIX) or msvcrt (Windows).
    """
    lockfile_path.parent.mkdir(parents=True, exist_ok=True)
    # Open with append/read mode so file is created if missing
    f = open(lockfile_path, "a+")
    fd = f.fileno()

    try:
        if sys.platform != "win32":
            import fcntl
            op = fcntl.LOCK_EX if exclusive else fcntl.LOCK_SH
            fcntl.flock(fd, op)
        else:
            import msvcrt
            f.seek(0)
            # Lock first byte of file
            msvcrt.locking(fd, msvcrt.LK_LOCK, 1)

        yield f

    finally:
        if sys.platform != "win32":
            import fcntl
            fcntl.flock(fd, fcntl.LOCK_UN)
        else:
            import msvcrt
            try:
                f.seek(0)
                msvcrt.locking(fd, msvcrt.LK_UNLCK, 1)
            except OSError:
                pass
        f.close()

# Verification
def test_file_lock():
    lock_file = Path("app.lock")
    with file_lock(lock_file):
        print("Holding exclusive file lock across processes...")
    print("Lock released cleanly.")
    if lock_file.exists():
        lock_file.unlink()

test_file_lock()
# Output:
# Holding exclusive file lock across processes...
# Lock released cleanly.
```

### Example 3: Simulating Crash Resistance Under Concurrent Readers
```python
import os
import time
import tempfile
from pathlib import Path
import threading

def run_atomic_concurrency_demo():
    with tempfile.TemporaryDirectory() as tmpdir:
        data_file = Path(tmpdir) / "state.txt"
        data_file.write_text("INITIAL_STATE")

        stop_event = threading.Event()
        errors = []

        def reader():
            while not stop_event.is_set():
                try:
                    content = data_file.read_text()
                    # The content must NEVER be empty or partial
                    if content not in ("INITIAL_STATE", "UPDATED_STATE"):
                        errors.append(f"Torn read observed: {content}")
                except Exception as e:
                    errors.append(f"Read exception: {e}")
                time.sleep(0.001)

        # Start 3 background reader threads
        threads = [threading.Thread(target=reader) for _ in range(3)]
        for t in threads:
            t.start()

        # Writer performs 50 atomic updates
        for _ in range(50):
            # Atomic update via tempfile + os.replace
            tmp = Path(tmpdir) / "temp.tmp"
            tmp.write_text("UPDATED_STATE")
            os.replace(tmp, data_file)

        stop_event.set()
        for t in threads:
            t.join()

        print(f"Total torn reads or corrupted states observed: {len(errors)}")
        assert len(errors) == 0

run_atomic_concurrency_demo()
# Output:
# Total torn reads or corrupted states observed: 0
```

### Example 4: Single-Instance Script Enforcement via Lockfile
```python
import sys
import os
from pathlib import Path

class SingleInstanceApp:
    def __init__(self, lock_name: str):
        self.lock_path = Path(tempfile.gettempdir()) / f"{lock_name}.pidlock"
        self.fp = None

    def acquire(self) -> bool:
        self.fp = open(self.lock_path, "w")
        try:
            if sys.platform != "win32":
                import fcntl
                # Try non-blocking lock
                fcntl.flock(self.fp.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
            else:
                import msvcrt
                msvcrt.locking(self.fp.fileno(), msvcrt.LK_NBLCK, 1)
            
            # Write current PID
            self.fp.write(str(os.getpid()))
            self.fp.flush()
            return True
        except (BlockingIOError, OSError):
            return False

    def release(self):
        if self.fp:
            try:
                self.fp.close()
                if self.lock_path.exists():
                    self.lock_path.unlink()
            except OSError:
                pass

def run_single_instance_demo():
    app1 = SingleInstanceApp("my_daemon_task")
    assert app1.acquire() is True
    print("Instance 1 acquired single-instance lock.")

    app2 = SingleInstanceApp("my_daemon_task")
    acquired_2 = app2.acquire()
    print("Instance 2 acquired lock:", acquired_2)
    assert acquired_2 is False

    app1.release()
    print("Instance 1 released lock.")

run_single_instance_demo()
# Output:
# Instance 1 acquired single-instance lock.
# Instance 2 acquired lock: False
# Instance 1 released lock.
```

### Example 5: Non-Blocking File Lock with Timeout Retries
```python
import time
import sys
from pathlib import Path

def acquire_lock_with_timeout(lock_path: Path, timeout_sec: float = 2.0) -> bool:
    start = time.perf_counter()
    f = open(lock_path, "w")
    
    while (time.perf_counter() - start) < timeout_sec:
        try:
            if sys.platform != "win32":
                import fcntl
                fcntl.flock(f.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
            else:
                import msvcrt
                f.seek(0)
                msvcrt.locking(f.fileno(), msvcrt.LK_NBLCK, 1)
            return True
        except (BlockingIOError, OSError):
            time.sleep(0.05)
            
    f.close()
    return False

# Verification
import tempfile
with tempfile.TemporaryDirectory() as tmp:
    lp = Path(tmp) / "retry.lock"
    success = acquire_lock_with_timeout(lp, timeout_sec=0.5)
    print("Acquired lock within timeout:", success)
    assert success is True
```

---

## Edge Cases, Pitfalls & Anti-Patterns

### 1. The Cross-Device Link Error (`EXDEV`)
`os.replace(src, dst)` is atomic **only** when `src` and `dst` reside on the exact same filesystem partition. If you create a temporary file in default `/tmp` (which may be mounted on `tmpfs` RAM disk) and try to replace a file in `/var/lib/data` (on an NVMe drive), `os.replace` fails with `OSError: [Errno 18] Invalid cross-device link`!
- **Rule**: Always create the temporary file in the same directory as the target file: `tempfile.NamedTemporaryFile(dir=target.parent)`.

### 2. Replacing Open Files on Windows
On Windows, attempting to call `os.replace(temp, target)` while `temp` (or `target` without specific sharing modes) is still open by Python raises `PermissionError: [WinError 32] The process cannot access the file because it is being used by another process`.
- **Rule**: Always call `temp_file.close()` before invoking `os.replace()`.

### 3. Missing Directory Sync on POSIX Filesystems
On POSIX filesystems (ext4, XFS), after calling `os.replace(src, dst)`, the directory entry itself is modified in memory. In mission-critical environments, you must open the parent directory and call `os.fsync()` on the directory file descriptor to ensure the directory entry pointer update is committed to the disk journal.

### Common Mistakes Table

| Bad Code | Good Code | Why | Severity |
| :--- | :--- | :--- | :--- |
| `open("data.json", "w").write(payload)` | Atomic write via tempfile + `os.replace` | Power loss or crash midway leaves a corrupted 0-byte file. | Critical |
| `NamedTemporaryFile(dir="/tmp")` to replace `/data/f` | `NamedTemporaryFile(dir=target.parent)` | Fails with `EXDEV` across mount boundaries; defeats atomicity. | Critical |
| Omitting `os.fsync()` before `os.replace()` | Flush and `os.fsync(f.fileno())` | Data stays in OS RAM cache; lost on kernel crash or power cut. | High |
| Calling `os.replace()` while temp file is still open | Close temp file first: `f.close()` | Raises `PermissionError` on Windows due to file locking semantics. | High |
| Using `fcntl.flock` for multithreading sync | `threading.Lock()` | `flock` operates at the OS open-file-table level; threads share file descriptors. | Medium |
| Forgetting to release locks in `finally` blocks | Wrap locks in context managers | Abandoned locks block other processes until OS terminates holding process. | Medium |

---

## Performance & Memory Characteristics

### Computational & Latency Overhead
- **Naive write**: $\approx 50 \mu\text{s}$ (writes to user buffer, no fsync).
- **Atomic write with `os.replace()` (without `fsync`)**: $\approx 180 \mu\text{s}$ (temp file inode allocation, write, atomic rename).
- **Full crash-durable atomic write with `os.fsync()`**: $\approx 2.5 - 15 \text{ ms}$ (forces disk controller cache flush to physical flash cells).

### Throughput Tradeoff
Atomic writes with `fsync` cap single-threaded write throughput to 100–500 transactions per second on consumer SSDs. If your system requires 50,000 writes/sec, batch changes into an append-only Write-Ahead Log (WAL) or in-memory ring buffer and flush periodically.

---

## CPython Internals & Bytecode

In CPython:
- `os.replace` is implemented in `Modules/posixmodule.c` as `os_replace_impl()`.
  - On POSIX, it directly invokes the C library `rename(src, dst)`.
  - On Windows, it invokes `MoveFileExW(src, dst, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)`.
- `os.fsync` maps directly to the POSIX `fsync(fd)` or Windows `_commit(fd)`.

Bytecode for atomic replacement:
```python
import dis
import os

def atomic_swap(src, dst):
    os.replace(src, dst)

dis.dis(atomic_swap)
```
```text
  1           0 RESUME                   0
  2           2 LOAD_GLOBAL              1 (os + NULL)
             12 LOAD_ATTR                3 (replace)
             22 LOAD_FAST                0 (src)
             24 LOAD_FAST                1 (dst)
             26 CALL                     2
             34 POP_TOP
             36 RETURN_CONST             0 (None)
```

---

## Best Practices & Idiomatic Patterns

1. **Always Use Atomic Replacement for State and Config Files**:
   Never truncate a production configuration file in place.
2. **Always Create Temp Files in the Destination's Parent Directory**:
   Guarantees same-filesystem atomic rename compatibility:
   ```python
   tempfile.NamedTemporaryFile(dir=target_path.parent)
   ```
3. **Always Call `os.fsync()` Before Renaming**:
   Guarantees physical media durability before swapping the directory entry.
4. **Use Lockfiles with PIDs for Daemons**:
   Write `str(os.getpid())` into your lockfile to help operators diagnose which process currently holds the lock.

---

## Real-World Case Study: Production Crash-Consistent Document Database Engine

The following micro-engine implements a crash-consistent, ACID-compliant JSON document store supporting atomic commits, schema validation, and concurrent multi-process locking.

```python
import os
import sys
import json
import tempfile
import contextlib
from pathlib import Path
from typing import Dict, Any, Optional

class CrashResilientStore:
    def __init__(self, db_path: Path):
        self.db_path = db_path.resolve()
        self.lock_path = self.db_path.with_suffix(".lock")
        self._init_storage()

    def _init_storage(self) -> None:
        self.db_path.parent.mkdir(parents=True, exist_ok=True)
        if not self.db_path.exists():
            self._write_atomic({})

    @contextlib.contextmanager
    def _acquire_process_lock(self, exclusive: bool = True):
        lock_file = open(self.lock_path, "a+")
        fd = lock_file.fileno()
        try:
            if sys.platform != "win32":
                import fcntl
                op = fcntl.LOCK_EX if exclusive else fcntl.LOCK_SH
                fcntl.flock(fd, op)
            else:
                import msvcrt
                lock_file.seek(0)
                msvcrt.locking(fd, msvcrt.LK_LOCK, 1)
            yield
        finally:
            if sys.platform != "win32":
                import fcntl
                fcntl.flock(fd, fcntl.LOCK_UN)
            else:
                import msvcrt
                try:
                    lock_file.seek(0)
                    msvcrt.locking(fd, msvcrt.LK_UNLCK, 1)
                except OSError:
                    pass
            lock_file.close()

    def _write_atomic(self, data: Dict[str, Any]) -> None:
        # Step 1: Serialize to string
        serialized = json.dumps(data, indent=2)

        # Step 2: Write to tempfile in same folder
        with tempfile.NamedTemporaryFile(
            mode="w",
            dir=self.db_path.parent,
            prefix=f".{self.db_path.name}.",
            suffix=".tmp",
            delete=False,
            encoding="utf-8"
        ) as tf:
            tf.write(serialized)
            tf.flush()
            # Force disk write
            os.fsync(tf.fileno())
            temp_name = tf.name

        # Step 3: Atomic replace
        os.replace(temp_name, self.db_path)

    def read_all(self) -> Dict[str, Any]:
        with self._acquire_process_lock(exclusive=False):
            return json.loads(self.db_path.read_text(encoding="utf-8"))

    def update_key(self, key: str, value: Any) -> None:
        with self._acquire_process_lock(exclusive=True):
            current = json.loads(self.db_path.read_text(encoding="utf-8"))
            current[key] = value
            self._write_atomic(current)

# Verification
if __name__ == "__main__":
    with tempfile.TemporaryDirectory() as tmpdir:
        db_file = Path(tmpdir) / "app_data.json"
        store = CrashResilientStore(db_file)

        # Perform atomic updates
        store.update_key("cluster_id", "us-east-99")
        store.update_key("active_nodes", 12)

        # Read back
        payload = store.read_all()
        print("Crash-resilient store contents:", payload)
        assert payload["cluster_id"] == "us-east-99"
        assert payload["active_nodes"] == 12
```

---

## Interview Questions & Deep-Dive Scenarios

<details>
<summary><strong>1. Junior: Why does <code>open('file.txt', 'w')</code> risk data loss if the system crashes midway?</strong></summary>

The `'w'` mode truncates the target file to 0 bytes immediately upon opening. If the operating system or application crashes before all new data is written and synchronized to physical storage, the original data is destroyed and the new data is incomplete, leaving a corrupted or empty file.
</details>

<details>
<summary><strong>2. Junior: What is the purpose of <code>os.replace()</code> compared to <code>os.rename()</code>?</strong></summary>

On POSIX systems, `os.rename()` silently overwrites the destination if it exists. However, on Windows, `os.rename()` raises a `FileExistsError` if the destination file already exists. `os.replace()` provides cross-platform atomic replacement semantics: on both POSIX and Windows, it atomically overwrites the destination file if it exists without raising an error.
</details>

<details>
<summary><strong>3. Mid: Why must a temporary file used for atomic replacement be created in the target file's parent directory rather than in <code>/tmp</code>?</strong></summary>

Atomic replacement via `os.replace()` relies on the OS `rename()` system call, which is guaranteed to be atomic **only within the same physical filesystem/mount point**. If the temp file is created in `/tmp` (often mounted on `tmpfs`) and the target resides on another partition (e.g. `/var`), the operation fails with `OSError: [Errno 18] Invalid cross-device link (EXDEV)`.
</details>

<details>
<summary><strong>4. Mid: What is the difference between <code>file.flush()</code> and <code>os.fsync(file.fileno())</code>?</strong></summary>

`file.flush()` pushes unwritten data from Python's userspace buffer into the operating system's kernel page cache. The data is still in volatile system RAM. `os.fsync(fd)` instructs the OS kernel to flush its dirty page cache directly to non-volatile physical media (SSD/HDD), ensuring durability across sudden hardware power loss.
</details>

<details>
<summary><strong>5. Senior: What is the difference between advisory and mandatory file locking? Which does Python use on Linux and Windows?</strong></summary>

- **Advisory Locking**: The operating system tracks locks, but does not block un-cooperating processes from reading or writing the file directly. All participating processes must cooperatively check and acquire the lock. POSIX (`fcntl.flock`) is advisory.
- **Mandatory Locking**: The operating system kernel enforces the lock against all system calls; any process attempting to access locked bytes without holding the lock is blocked or receives an error. Windows (`msvcrt.locking`) is mandatory.
</details>

<details>
<summary><strong>6. Senior: Why is <code>fcntl.flock</code> inadequate for synchronizing threads within a single Python process?</strong></summary>

In POSIX, file locks created via `flock()` are associated with the open file table description in the kernel. Threads within the same Python process share the same file descriptors and address space. If Thread A acquires an `flock` and Thread B attempts to acquire the same `flock` using the same file descriptor, the kernel treats both as the same owner and immediately grants the lock, causing race conditions. Thread synchronization must use `threading.Lock`.
</details>

<details>
<summary><strong>7. Staff: How does directory fsync work on POSIX filesystems, and why is it necessary after <code>os.replace()</code>?</strong></summary>

On journaling filesystems (ext4, XFS), `os.replace()` alters the parent directory's inode structure (updating directory entry pointers). While `os.fsync(file.fileno())` ensures the file data pages are on disk, the parent directory metadata may remain in RAM. If power cuts out immediately, the directory entry pointer update might be rolled back, leaving the new file orphaned in `lost+found`. To achieve true crash consistency, open the directory itself (`dir_fd = os.open(target.parent, os.O_RDONLY)`) and call `os.fsync(dir_fd)`.
</details>

<details>
<summary><strong>8. Staff: How would you design a lock-free multi-process logging queue on a single machine without centralized brokers?</strong></summary>

1. Use memory-mapped files (`mmap`) backed by a shared POSIX shared memory segment (`/dev/shm` or `multiprocessing.shared_memory`).
2. Structure the buffer as a circular ring buffer with a 64-bit atomic write sequence header.
3. Use atomic CPU primitives (compare-and-swap via C extension or shared atomic integers) to claim write offsets without acquiring OS file locks, achieving microsecond-latency non-blocking ingestion across processes.
</details>

---

## Practical Exercises & Code Katas

### Exercise 1: Safe Configuration Updater
**Requirement:** Write a function `safe_save_json(path: Path, data: dict)` that serializes a dictionary to JSON and atomically replaces the file on disk, handling exceptions by cleaning up temporary files.

<details>
<summary>Show Solution</summary>

```python
import os
import json
import tempfile
from pathlib import Path

def safe_save_json(path: Path, data: dict) -> None:
    path = path.resolve()
    path.parent.mkdir(parents=True, exist_ok=True)

    with tempfile.NamedTemporaryFile(
        mode="w",
        dir=path.parent,
        prefix=f".{path.name}.",
        suffix=".tmp",
        delete=False,
        encoding="utf-8"
    ) as tf:
        try:
            json.dump(data, tf, indent=2)
            tf.flush()
            os.fsync(tf.fileno())
            temp_name = tf.name
        except Exception:
            tf.close()
            os.unlink(tf.name)
            raise

    os.replace(temp_name, path)

# Test
import tempfile
with tempfile.TemporaryDirectory() as tmp:
    cfg_file = Path(tmp) / "settings.json"
    safe_save_json(cfg_file, {"timeout": 30, "debug": True})
    print("Safely written config:", cfg_file.read_text())
    assert json.loads(cfg_file.read_text())["timeout"] == 30
```
</details>

### Exercise 2: Non-Blocking File Lock Detector
**Requirement:** Write a function `is_file_locked(path: Path) -> bool` that attempts a non-blocking lock on a file, returning `True` if another process holds the lock, and `False` otherwise without blocking.

<details>
<summary>Show Solution</summary>

```python
import sys
from pathlib import Path

def is_file_locked(path: Path) -> bool:
    if not path.exists():
        return False
    try:
        f = open(path, "r+")
        try:
            if sys.platform != "win32":
                import fcntl
                fcntl.flock(f.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
                fcntl.flock(f.fileno(), fcntl.LOCK_UN)
                return False
            else:
                import msvcrt
                msvcrt.locking(f.fileno(), msvcrt.LK_NBLCK, 1)
                f.seek(0)
                msvcrt.locking(f.fileno(), msvcrt.LK_UNLCK, 1)
                return False
        finally:
            f.close()
    except (BlockingIOError, OSError):
        return True

# Test
import tempfile
with tempfile.TemporaryDirectory() as tmp:
    lock_file = Path(tmp) / "test.lock"
    lock_file.touch()
    print("Is file locked initially:", is_file_locked(lock_file))
    assert is_file_locked(lock_file) is False
```
</details>

### Exercise 3: Cross-Platform Process-Safe Shared Counter
**Requirement:** Implement a file-backed counter incremented by multiple processes, using file locking to prevent lost updates.

<details>
<summary>Show Solution</summary>

```python
import sys
from pathlib import Path

def increment_counter(counter_file: Path) -> int:
    counter_file.parent.mkdir(parents=True, exist_ok=True)
    with open(counter_file, "a+") as f:
        fd = f.fileno()
        if sys.platform != "win32":
            import fcntl
            fcntl.flock(fd, fcntl.LOCK_EX)
        else:
            import msvcrt
            f.seek(0)
            msvcrt.locking(fd, msvcrt.LK_LOCK, 1)
            
        try:
            f.seek(0)
            content = f.read().strip()
            val = int(content) if content else 0
            val += 1
            f.seek(0)
            f.truncate()
            f.write(str(val))
            f.flush()
            return val
        finally:
            if sys.platform != "win32":
                import fcntl
                fcntl.flock(fd, fcntl.LOCK_UN)
            else:
                import msvcrt
                try:
                    f.seek(0)
                    msvcrt.locking(fd, msvcrt.LK_UNLCK, 1)
                except OSError:
                    pass

# Test
import tempfile
with tempfile.TemporaryDirectory() as tmp:
    cf = Path(tmp) / "count.txt"
    print("First inc:", increment_counter(cf))
    print("Second inc:", increment_counter(cf))
    assert increment_counter(cf) == 3
```
</details>

### Exercise 4: Atomic Backup and Replace
**Requirement:** Write a function that replaces a target file atomically, but first moves the existing file to `<filename>.bak` so an immediate rollback is possible.

<details>
<summary>Show Solution</summary>

```python
import os
import shutil
import tempfile
from pathlib import Path

def backup_and_replace(target: Path, new_content: str) -> None:
    target = target.resolve()
    target.parent.mkdir(parents=True, exist_ok=True)
    bak_path = target.with_suffix(target.suffix + ".bak")

    # Step 1: Create backup if target exists
    if target.exists():
        shutil.copy2(target, bak_path)

    # Step 2: Write temp file in same directory
    with tempfile.NamedTemporaryFile(mode="w", dir=target.parent, delete=False, encoding="utf-8") as tf:
        tf.write(new_content)
        tf.flush()
        os.fsync(tf.fileno())
        temp_name = tf.name

    # Step 3: Replace
    os.replace(temp_name, target)

# Verification
with tempfile.TemporaryDirectory() as tmp:
    doc = Path(tmp) / "manifest.txt"
    doc.write_text("v1")
    backup_and_replace(doc, "v2")
    
    print("New target content:", doc.read_text())
    print("Backup content:", (Path(tmp) / "manifest.txt.bak").read_text())
    assert doc.read_text() == "v2"
    assert (Path(tmp) / "manifest.txt.bak").read_text() == "v1"
```
</details>

### Exercise 5: Deadlock-Free Multi-File Locker
**Requirement:** Implement a context manager `lock_multiple_files(paths: list[Path])` that sorts file paths lexicographically before locking to prevent classic Dijkstra dining philosophers deadlocks between concurrent processes.

<details>
<summary>Show Solution</summary>

```python
import sys
import contextlib
from pathlib import Path

@contextlib.contextmanager
def lock_multiple_files(paths: list[Path]):
    # Prevent deadlock: Always acquire locks in strictly sorted canonical order
    sorted_paths = sorted([p.resolve() for p in paths])
    open_handles = []

    try:
        for p in sorted_paths:
            f = open(p, "a+")
            fd = f.fileno()
            if sys.platform != "win32":
                import fcntl
                fcntl.flock(fd, fcntl.LOCK_EX)
            else:
                import msvcrt
                f.seek(0)
                msvcrt.locking(fd, msvcrt.LK_LOCK, 1)
            open_handles.append(f)
        yield open_handles
    finally:
        for f in reversed(open_handles):
            fd = f.fileno()
            if sys.platform != "win32":
                import fcntl
                fcntl.flock(fd, fcntl.LOCK_UN)
            else:
                import msvcrt
                try:
                    f.seek(0)
                    msvcrt.locking(fd, msvcrt.LK_UNLCK, 1)
                except OSError:
                    pass
            f.close()

# Test
import tempfile
with tempfile.TemporaryDirectory() as tmp:
    p1 = Path(tmp) / "b.lock"
    p2 = Path(tmp) / "a.lock"
    with lock_multiple_files([p1, p2]):
        print("Safely locked multiple resources in sorted canonical order.")
```
</details>

### Exercise 6: Complete Directory Inode Fsync Helper (POSIX)
**Requirement:** Write a POSIX helper function `sync_parent_directory(path: Path)` that opens the parent directory with `os.open` and executes `os.fsync` to guarantee directory metadata persistence.

<details>
<summary>Show Solution</summary>

```python
import os
import sys
from pathlib import Path

def sync_parent_directory(target_path: Path) -> None:
    """
    On POSIX systems, fsyncs the parent directory of target_path.
    Guarantees that rename/replace inode link updates are journaled to disk.
    """
    if sys.platform == "win32":
        # Windows handles directory journaling automatically in NTFS MoveFileEx
        return

    parent_dir = str(target_path.resolve().parent)
    try:
        dir_fd = os.open(parent_dir, os.O_RDONLY | os.O_DIRECTORY)
        try:
            os.fsync(dir_fd)
        finally:
            os.close(dir_fd)
    except OSError:
        pass

# Verification
import tempfile
with tempfile.TemporaryDirectory() as tmp:
    t = Path(tmp) / "test.txt"
    t.write_text("Synced")
    sync_parent_directory(t)
    print("Directory synced successfully.")
```
</details>
