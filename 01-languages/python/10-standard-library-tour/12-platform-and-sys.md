---
title: "Runtime Environment and OS Diagnostics: platform and sys"
description: "Mastering Python's runtime introspection and system interfaces: sys module lifecycle control, string interning with sys.intern, shallow vs deep memory with sys.getsizeof, platform OS architecture detection, and cross-platform compatibility branching."
module: "10-standard-library-tour"
order: 12
tags:
  - python
  - sys
  - platform
  - runtime
  - memory
  - interning
  - os-detection
difficulty: "Intermediate"
cpython_version: "3.12+"
last_verified: "2026-03"
---

# Runtime Environment and OS Diagnostics: platform and sys

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
10. [Real-World Case Study: Cross-Platform Hardware-Aware Diagnostic Agent](#real-world-case-study-cross-platform-hardware-aware-diagnostic-agent)
11. [Interview Questions & Deep-Dive Scenarios](#interview-questions--deep-dive-scenarios)
12. [Practical Exercises & Code Katas](#practical-exercises--code-katas)

---

## What & Why

Python provides two complementary standard library modules for interacting with the active Python interpreter runtime and the underlying host operating system environment:
- **`sys`**: Provides access to variables and functions directly maintained or used by the CPython interpreter runtime: standard I/O streams (`sys.stdin`, `sys.stdout`, `sys.stderr`), module registries (`sys.modules`), import search paths (`sys.path`), memory sizing (`sys.getsizeof`), recursion depth limits (`sys.setrecursionlimit`), and process termination (`sys.exit`).
- **`platform`**: Probes the underlying operating system, kernel release, hardware architecture (x86_64, aarch64, arm64), processor type, and Python VM implementation (CPython, PyPy, Jython, GraalPy).

### Motivation
Real-world software must adapt dynamically to its deployment environment:
1. **Cross-Platform Compatibility**: A CLI tool or desktop agent must execute Windows API calls on Windows, POSIX syscalls on Linux, and macOS frameworks on Darwin without crashing.
2. **Memory Profiling and Optimization**: Verifying whether data structures are consuming excessive RAM using `sys.getsizeof()`, and deduplicating string allocations via string interning (`sys.intern()`).
3. **Runtime Protection**: Guarding against stack overflow crashes by inspecting or tuning `sys.getrecursionlimit()`.
4. **Build Distribution**: Detecting whether wheels or binaries should target 64-bit ARM (Apple Silicon / AWS Graviton) or AMD64 architectures.

### Real-World Analogy
Consider a modern electric vehicle:
- **`sys`** is the vehicle's internal computer dashboard: it monitors the battery charge level, speedometer, interior climate control, diagnostic error codes, and the master ignition kill switch (`sys.exit`).
- **`platform`** is the vehicle's external environment sensors: it checks whether the car is driving on a frozen gravel road in Alaska or a paved desert highway in Arizona (OS, kernel version, hardware chassis).

### When to Use vs When Not to Use
- **Use `sys` for:** Exit code management, standard stream redirection, runtime memory sizing, import path inspection, recursion limit adjustments, and string interning.
- **Use `platform` for:** Hardware architecture branching, OS version detection, compiling platform-specific diagnostic telemetry, and reporting system specs.
- **Do Not Use:**
  - `sys.getsizeof()` for measuring complete recursive container memory without a tree crawler (it measures only shallow pointer memory).
  - Unbounded `sys.setrecursionlimit(100_000)` to fix algorithmic recursion errors (causes C-level segmentation faults!).

---

## Core Concepts & Mental Model

```
+-------------------------------------------------------------------------+
| Host Machine Hardware & Operating System                                |
| Architecture: x86_64 / arm64 | OS: Linux 6.5 / Windows 11 / macOS 14    |
+-------------------------------------------------------------------------+
                                    |
                                    v
+-------------------------------------------------------------------------+
| platform Module (Probes OS Kernel & Hardware Attributes)                |
| - platform.system() -> "Linux" / "Windows" / "Darwin"                   |
| - platform.machine() -> "x86_64" / "arm64"                              |
| - platform.python_implementation() -> "CPython"                         |
+-------------------------------------------------------------------------+
                                    |
                                    v
+-------------------------------------------------------------------------+
| sys Module (Controls CPython Virtual Machine State)                     |
| - sys.modules:   Loaded module cache dictionary                         |
| - sys.path:      Ordered import lookup search list                      |
| - sys.intern():  Global string deduplication table                      |
| - sys.getsizeof(): Object shallow memory in bytes                       |
| - sys.exit():    Terminates interpreter process                         |
+-------------------------------------------------------------------------+
```

### Key Distinctions
1. **`sys.platform` vs `platform.system()`**:
   - `sys.platform`: Fast, coarse string identifier (`"win32"`, `"linux"`, `"darwin"`). Note that `"win32"` applies to both 32-bit and 64-bit Windows!
   - `platform.system()`: High-level descriptive OS name (`"Windows"`, `"Linux"`, `"Darwin"`).
2. **`sys.getsizeof()` Shallow Sizing**:
   `sys.getsizeof([1, 2, [3, 4]])` measures only the top-level list container and its pointers to child objects. It does **not** include the memory of the integers or the inner list!
3. **String Interning (`sys.intern`)**:
   Forces Python to store only a single unique copy of an immutable string in a global lookup table, allowing subsequent string comparisons (`s1 is s2`) to execute in $\mathcal{O}(1)$ pointer comparison time rather than $\mathcal{O}(N)$ character comparisons.

---

## How It Works Internally

### 1. `sys.intern()` and the Global Interned Dictionary
In CPython (`Objects/unicodeobject.c`):
- The interpreter maintains a global dictionary of interned strings (`interned`).
- When `sys.intern(s)` is called:
  1. It checks if an identical string exists in the `interned` dictionary.
  2. If found, it returns the existing pointer and decrements the reference count of the candidate string.
  3. If not found, it inserts `s` into the interned table and marks its internal flags as `SSTATE_INTERNED_IMMORTAL` or `SSTATE_INTERNED_MORTAL`.
- This ensures that if you parse 10 million rows containing the status string `"COMPLETED"`, all 10 million rows share the **exact same 48-byte memory address**!

### 2. CPython Recursion Limit and Stack Frames
In CPython (`Python/ceval.c`):
- Every Python function call creates a new execution stack frame.
- CPython imposes a safety barrier: `sys.getrecursionlimit()` (default is 1,000 frames).
- If recursion exceeds this threshold, Python raises `RecursionError` cleanly in user-space.
- **Why it matters**: Increasing `sys.setrecursionlimit(50000)` without caution will eventually overflow the host operating system's native C stack (typically 2 MB to 8 MB), crashing the entire Python process with a fatal, uncatchable **Segmentation Fault (SIGSEGV)**!

---

## Syntax & API Reference

### `sys` Module
```python
import sys

# Interpreter Lifecycle & Streams
sys.exit(arg: int | str = 0)             # Raises SystemExit with code or message
sys.stdin, sys.stdout, sys.stderr        # Standard I/O text streams

# System & Memory
size: int = sys.getsizeof(obj, default=None) # Memory footprint in bytes
interned_str: str = sys.intern(s: str)   # String interning
rec_limit: int = sys.getrecursionlimit() # Default 1000
sys.setrecursionlimit(limit: int)

# Import System
modules_cache: dict = sys.modules        # Map of module_name -> module_object
search_paths: list[str] = sys.path       # sys.path search directories

# Low-level flags
sys.version: str                         # Python version string
sys.version_info: tuple                  # (major, minor, micro, releaselevel, serial)
sys.platform: str                        # 'win32', 'linux', 'darwin'
sys.byteorder: str                       # 'little' (x86/ARM) or 'big'
```

### `platform` Module
```python
import platform

platform.system() -> str                 # 'Windows', 'Linux', 'Darwin'
platform.release() -> str                # Kernel release (e.g. '6.5.0-28-generic')
platform.version() -> str                # Detailed OS version build
platform.machine() -> str                # 'x86_64', 'AMD64', 'arm64', 'aarch64'
platform.processor() -> str              # Hardware chip name (e.g. 'Apple M2')
platform.python_implementation() -> str  # 'CPython', 'PyPy'
platform.python_version() -> str        # '3.12.2'
platform.architecture() -> tuple         # ('64bit', 'WindowsPE')
```

---

## Code Examples

### Example 1: Deep Memory Footprint Calculator (Recursive `getsizeof`)
```python
import sys
from collections.abc import Mapping, Container

def get_deep_size(obj, seen=None) -> int:
    """
    Recursively calculates the true total memory footprint of an object
    and all its referenced child objects in bytes.
    """
    size = sys.getsizeof(obj)
    if seen is None:
        seen = set()

    obj_id = id(obj)
    if obj_id in seen:
        return 0
    seen.add(obj_id)

    if isinstance(obj, Mapping):
        size += sum(get_deep_size(k, seen) + get_deep_size(v, seen) for k, v in obj.items())
    elif isinstance(obj, Container) and not isinstance(obj, (str, bytes, bytearray)):
        size += sum(get_deep_size(item, seen) for item in obj)

    return size

def run_sizeof_demo():
    sample_data = {
        "user_id": 1048204,
        "tags": ["python", "engineering", "notes"],
        "metadata": {"active": True, "balance": 99.50}
    }

    shallow_size = sys.getsizeof(sample_data)
    deep_size = get_deep_size(sample_data)

    print(f"Shallow size (dict container only): {shallow_size} bytes")
    print(f"True deep size (recursive tree):     {deep_size} bytes")
    assert deep_size > shallow_size

run_sizeof_demo()
# Output:
# Shallow size (dict container only): ~184 bytes
# True deep size (recursive tree):     ~950 bytes
```

### Example 2: Memory Deduplication with `sys.intern`
```python
import sys

def run_interning_demo():
    # Construct two dynamically generated strings with identical characters
    s1 = "".join(["order", "_", "status", "_", "pending"])
    s2 = "".join(["order", "_", "status", "_", "pending"])

    # Without interning: They are distinct objects in memory
    print("Without sys.intern:")
    print("  s1 == s2 (value equality):     ", s1 == s2)
    print("  s1 is s2 (identity pointer):   ", s1 is s2)
    print("  id(s1) vs id(s2):              ", id(s1), "!=", id(s2))

    # Intern both strings
    interned_s1 = sys.intern(s1)
    interned_s2 = sys.intern(s2)

    print("\nWith sys.intern:")
    print("  interned_s1 is interned_s2:     ", interned_s1 is interned_s2)
    print("  id(interned_s1) == id(interned_s2):", id(interned_s1) == id(interned_s2))

run_interning_demo()
# Output:
# Without sys.intern:
#   s1 == s2 (value equality):      True
#   s1 is s2 (identity pointer):    False
#   id(s1) vs id(s2):               ... != ...
# 
# With sys.intern:
#   interned_s1 is interned_s2:      True
#   id(interned_s1) == id(interned_s2): True
```

### Example 3: Comprehensive Host Platform Telemetry Inspection
```python
import platform
import sys

def collect_host_telemetry() -> dict[str, str]:
    return {
        "os_name": platform.system(),
        "os_release": platform.release(),
        "os_version": platform.version(),
        "machine_arch": platform.machine(),
        "cpu_processor": platform.processor(),
        "python_impl": platform.python_implementation(),
        "python_version": platform.python_version(),
        "sys_platform": sys.platform,
        "byte_order": sys.byteorder,
        "pointer_bits": platform.architecture()[0]
    }

def run_platform_demo():
    telemetry = collect_host_telemetry()
    print("Host Machine & Interpreter Diagnostics:")
    for key, val in telemetry.items():
        print(f"  {key:15}: {val}")

run_platform_demo()
# Output:
# Host Machine & Interpreter Diagnostics:
#   os_name        : Windows / Linux / Darwin
#   os_release     : ...
#   os_version     : ...
#   machine_arch   : x86_64 / arm64
#   cpu_processor  : ...
#   python_impl    : CPython
#   python_version : 3.12...
#   sys_platform   : win32 / linux / darwin
#   byte_order     : little
#   pointer_bits   : 64bit
```

### Example 4: Standard Output & Error Redirection with `sys.stdout`
```python
import sys
import io

def run_stream_redirection():
    # Save original standard streams
    original_stdout = sys.stdout
    captured_buffer = io.StringIO()

    try:
        # Redirect standard output to an in-memory buffer
        sys.stdout = captured_buffer

        # Standard print calls now write into captured_buffer
        print("Diagnostic Step 1: Kernel initialized")
        print("Diagnostic Step 2: Running health check")

    finally:
        # Restore standard output immediately
        sys.stdout = original_stdout

    intercepted = captured_buffer.getvalue()
    print("Successfully intercepted output from sys.stdout:")
    print(intercepted.strip())

run_stream_redirection()
# Output:
# Successfully intercepted output from sys.stdout:
# Diagnostic Step 1: Kernel initialized
# Diagnostic Step 2: Running health check
```

### Example 5: Dynamic Import Injection with `sys.modules`
```python
import sys
import types

def inject_virtual_module(module_name: str, exports: dict):
    """
    Creates and injects a synthetic in-memory module into sys.modules.
    Any subsequent 'import <module_name>' will retrieve this in-memory module!
    """
    mod = types.ModuleType(module_name)
    mod.__dict__.update(exports)
    sys.modules[module_name] = mod
    return mod

def run_virtual_module_demo():
    # Inject synthetic module 'virtual_config'
    inject_virtual_module("virtual_config", {
        "API_ENDPOINT": "https://api.internal.network/v1",
        "TIMEOUT": 30,
        "get_version": lambda: "v2.4.0"
    })

    # Now standard import resolves the virtual module directly
    import virtual_config

    print("Virtual Module Imported Successfully:")
    print("  Endpoint:", virtual_config.API_ENDPOINT)
    print("  Timeout: ", virtual_config.TIMEOUT)
    print("  Version: ", virtual_config.get_version())

    # Cleanup
    del sys.modules["virtual_config"]

run_virtual_module_demo()
# Output:
# Virtual Module Imported Successfully:
#   Endpoint: https://api.internal.network/v1
#   Timeout:  30
#   Version:  v2.4.0
```

---

## Edge Cases, Pitfalls & Anti-Patterns

### 1. `sys.platform == "win32"` on 64-Bit Windows
A ubiquitous misconception is checking `sys.platform == "win64"` on 64-bit Windows machines.
- In Python, `sys.platform` is **always `"win32"`** on Windows, regardless of whether the operating system or the Python binary is 32-bit or 64-bit!
- To check architecture, inspect `platform.architecture()[0]` (`"64bit"`) or `platform.machine()`.

### 2. Unsafe `sys.setrecursionlimit` Expansion
Raising recursion limits via `sys.setrecursionlimit(100_000)` does not magically give Python infinite call stacks. Every stack frame consumes C-level call stack memory. Once physical OS stack memory (typically 8 MB) is exhausted, the OS kernel terminates the process with an uncatchable **Segmentation Fault (SIGSEGV)**. Convert recursive algorithms to iterative stacks using loops and `collections.deque`.

### 3. Modifying `sys.path` with Relative Paths
Appending relative paths (`sys.path.append("../lib")`) causes resolution errors if your application changes its current working directory (`os.chdir()`). Always resolve to an absolute path first:
```python
sys.path.append(str(Path(__file__).resolve().parent.parent / "lib"))
```

### Common Mistakes Table

| Bad Code | Good Code | Why | Severity |
| :--- | :--- | :--- | :--- |
| `if sys.platform == "win64":` | `if sys.platform == "win32":` | `sys.platform` is `"win32"` on ALL Windows platforms (even 64-bit). | Critical |
| `sys.getsizeof(my_dict)` for memory check | Use recursive tree walker or `tracemalloc` | `sys.getsizeof()` measures only shallow container memory, ignoring values. | High |
| `sys.setrecursionlimit(1_000_000)` | Refactor to iterative loop with deque | Exhausts OS C-stack, causing uncatchable Segmentation Faults. | Critical |
| `sys.path.append("relative/path")` | `sys.path.append(str(Path(p).resolve()))` | Breaks if process changes current working directory via `os.chdir`. | High |
| Calling `sys.exit()` in libraries | Raise descriptive custom exceptions | `sys.exit()` aborts entire host process; libraries must never exit. | High |
| Hand-rolling string interning | Use `sys.intern(s)` | `sys.intern()` utilizes CPython's native C string table. | Low |

---

## Performance & Memory Characteristics

### String Comparison: Standard vs Interned
Comparing two 100-character strings 1,000,000 times:
- **Standard Equality (`s1 == s2`)**: $\approx 18 \text{ ms}$ ($\mathcal{O}(N)$ sequential character checks).
- **Interned Identity (`s1 is s2`)**: $\approx 3.2 \text{ ms}$ ($\mathcal{O}(1)$ direct pointer comparison; **~5.6x faster**).

### Shallow `sys.getsizeof()` Baseline Overheads in CPython (64-bit)
- Empty `int`: 28 bytes
- Empty `str`: 49–80 bytes (depends on ASCII vs Unicode PEP 393 layout)
- Empty `list`: 56 bytes
- Empty `dict`: 64 bytes
- Empty `set`: 216 bytes

---

## CPython Internals & Bytecode

In CPython:
- `sys` functions are defined in `Python/sysmodule.c`.
- `sys.modules` is a direct reference to the interpreter's internal `interp->modules` hash table.
- `sys.exit(arg)` works by raising the built-in `SystemExit(arg)` exception. Because it is an exception inheriting from `BaseException` (not `Exception`), standard `except Exception:` blocks do not intercept it.

Bytecode for `sys.platform`:
```python
import dis
import sys

def check_os():
    return sys.platform == "win32"

dis.dis(check_os)
```
```text
  1           0 RESUME                   0
  2           2 LOAD_GLOBAL              1 (sys + NULL)
             12 LOAD_ATTR                3 (platform)
             22 LOAD_CONST               1 ('win32')
             24 COMPARE_OP              40 (==)
             28 RETURN_VALUE
```

---

## Best Practices & Idiomatic Patterns

1. **Libraries Must Never Call `sys.exit()`**:
   Only top-level CLI scripts or main entry points should call `sys.exit()`. Libraries should raise exceptions.
2. **Use `sys.intern()` for Parsing Repeated Enums/Categories**:
   When parsing multi-gigabyte CSV or JSON files with repeating keys or category labels, interning the strings dramatically reduces memory usage and speeds up grouping operations.
3. **Use `platform.system()` for Cross-Platform Branching**:
   ```python
   match platform.system():
       case "Windows":
           init_windows_service()
       case "Linux":
           init_systemd_daemon()
       case "Darwin":
           init_launchd_agent()
   ```

---

## Real-World Case Study: Cross-Platform Hardware-Aware Diagnostic Agent

The following telemetry agent inspects host machine specs, CPU architecture, memory thresholds, and Python runtime features, outputting a sanitized diagnostic health manifest for distributed cloud workers.

```python
import sys
import platform
import os
from pathlib import Path
from typing import Dict, Any

class SystemDiagnosticAgent:
    def __init__(self):
        self.os_type = platform.system()
        self.arch = platform.machine()

    def generate_health_report(self) -> Dict[str, Any]:
        """Collects cross-platform runtime, hardware, and OS diagnostics."""
        report = {
            "runtime": {
                "python_version": platform.python_version(),
                "implementation": platform.python_implementation(),
                "executable": sys.executable,
                "recursion_limit": sys.getrecursionlimit(),
                "byte_order": sys.byteorder,
                "gil_active": getattr(sys, "_is_gil_enabled", lambda: True)()
            },
            "host_os": {
                "system": self.os_type,
                "release": platform.release(),
                "kernel_version": platform.version(),
                "sys_platform": sys.platform
            },
            "hardware": {
                "architecture": self.arch,
                "processor": platform.processor() or "Unknown",
                "cpu_count": os.cpu_count() or 1,
                "pointer_size": platform.architecture()[0]
            },
            "environment_sanity": {
                "is_64bit": sys.maxsize > 2**32,
                "is_little_endian": sys.byteorder == "little",
                "active_modules_count": len(sys.modules)
            }
        }

        # Platform-specific diagnostic enrichment
        if self.os_type == "Windows":
            report["host_os"]["win32_edition"] = getattr(platform, "win32_edition", lambda: "Standard")()
        elif self.os_type == "Linux":
            try:
                report["host_os"]["libc_version"] = " ".join(platform.libc_ver())
            except Exception:
                pass

        return report

# Verification
if __name__ == "__main__":
    agent = SystemDiagnosticAgent()
    data = agent.generate_health_report()
    print("Health Diagnostic Manifest:")
    print("  OS:            ", data["host_os"]["system"], data["host_os"]["release"])
    print("  Architecture:  ", data["hardware"]["architecture"], f"({data['hardware']['pointer_size']})")
    print("  CPUs:          ", data["hardware"]["cpu_count"])
    print("  Python Runtime:", data["runtime"]["implementation"], data["runtime"]["python_version"])
    assert data["environment_sanity"]["is_64bit"] is True
```

---

## Interview Questions & Deep-Dive Scenarios

<details>
<summary><strong>1. Junior: Why does <code>sys.getsizeof()</code> return a misleadingly small number for nested lists or dictionaries?</strong></summary>

`sys.getsizeof()` calculates only the direct memory allocated for the top-level object itself (its C `PyObject` header and array of pointers to items). It does not recursively inspect child objects. For a list containing 10 large strings, `sys.getsizeof()` reports only the size of the list structure and its 10 pointers (~136 bytes), completely ignoring the memory consumed by the strings themselves.
</details>

<details>
<summary><strong>2. Junior: How does <code>sys.exit()</code> terminate a Python program? Can it be caught?</strong></summary>

`sys.exit(arg)` raises the built-in `SystemExit(arg)` exception. Yes, it can be caught using `try ... except SystemExit:`. It inherits directly from `BaseException` (not `Exception`) so that standard `except Exception:` blocks do not accidentally swallow process exit requests.
</details>

<details>
<summary><strong>3. Mid: What is string interning (<code>sys.intern</code>), and under what conditions does it provide major performance benefits?</strong></summary>

String interning guarantees that only one canonical copy of an immutable string is stored in memory. When comparing interned strings, Python compares their memory pointers using the `is` operator in $\mathcal{O}(1)$ time rather than comparing characters sequentially in $\mathcal{O}(N)$ time. It provides major memory and speed benefits when processing large datasets with repetitive categorical values (e.g. status codes, country names, database column keys).
</details>

<details>
<summary><strong>4. Mid: Why is <code>sys.platform</code> equal to <code>"win32"</code> even on 64-bit Windows installations?</strong></summary>

For historical backwards compatibility. When 64-bit Windows was released, existing Python code contained thousands of checks written as `if sys.platform == 'win32':`. To prevent breaking compatibility across the global ecosystem, Python retained `"win32"` as the platform identifier for all Windows versions. To check architecture, developers inspect `platform.architecture()` or `sys.maxsize > 2**32`.
</details>

<details>
<summary><strong>5. Senior: What are the dangers of calling <code>sys.setrecursionlimit()</code> with large values (e.g. 100,000)?</strong></summary>

Python's recursion limit is a safety guard to prevent stack overflow in the host operating system's native C call stack. Unlike languages with segmented or tail-call optimized stacks, each CPython call frame consumes native C stack space (typically 8 MB max on Linux). Forcing a recursion limit beyond physical stack boundaries causes the OS to hit a guard page, triggering a fatal C-level **Segmentation Fault (SIGSEGV)** that terminates the process instantly without running `finally` blocks or exception handlers.
</details>

<details>
<summary><strong>6. Senior: How does <code>sys.modules</code> operate during the import lifecycle, and what happens if you mutate it directly?</strong></summary>

`sys.modules` is the interpreter's global module cache dictionary. When `import foo` is called:
1. Python checks `sys.modules["foo"]`. If present, it returns the cached module immediately without disk I/O.
2. If absent, Python searches `sys.path`, loads the file, executes its code in a fresh module namespace, stores it in `sys.modules["foo"]`, and returns it.
Mutating `sys.modules` allows mock injection (inserting fake modules during unit tests) and hot-reloading (deleting a key to force re-import).
</details>

<details>
<summary><strong>7. Staff: How does Python 3.13's <code>sys._is_gil_enabled()</code> work in free-threaded builds (PEP 703)?</strong></summary>

In Python 3.13 free-threaded binaries (`python3.13t`), the Global Interpreter Lock can be dynamically disabled or re-enabled at runtime. `sys._is_gil_enabled()` returns a boolean indicating whether the GIL is currently active. If a multi-threaded program loads a legacy C-extension that has not declared support for free-threading (`Py_MOD_GIL_NOT_USED`), CPython automatically re-enables the GIL at runtime to prevent memory corruption, and `sys._is_gil_enabled()` flips back to `True`.
</details>

<details>
<summary><strong>8. Staff: How would you architect a secure sandboxed script runner that restricts standard library imports and system commands?</strong></summary>

1. Restrict `sys.modules`: Remove dangerous modules (`os`, `subprocess`, `socket`, `shutil`, `sys`) from `sys.modules` or inject inert dummy modules.
2. Custom Import Hook: Override `sys.meta_path` with a custom `PathFinder` that validates module import requests against an explicit whitelist.
3. Replace Builtins: Sanitize the `__builtins__` dictionary to strip `exec`, `eval`, `open`, and `__import__`.
4. Apply OS Sandboxing: Because Python-level sandboxes can be bypassed via object traversal (`object.__subclasses__()`), always pair software restrictions with kernel-level containers (Docker, cgroups, seccomp filters, or gVisor).
</details>

---

## Practical Exercises & Code Katas

### Exercise 1: Recursive Object Memory Profiler
**Requirement:** Write a function `estimate_container_memory(container)` that computes the true memory size of a list of strings, demonstrating the discrepancy against shallow `sys.getsizeof()`.

<details>
<summary>Show Solution</summary>

```python
import sys

def estimate_container_memory(items: list[str]) -> tuple[int, int]:
    shallow = sys.getsizeof(items)
    deep = shallow + sum(sys.getsizeof(s) for s in items)
    return shallow, deep

# Test
words = ["python", "dataclasses", "engineering", "telemetry"] * 100
shallow_mem, deep_mem = estimate_container_memory(words)
print(f"Shallow: {shallow_mem} bytes, Deep: {deep_mem} bytes")
assert deep_mem > shallow_mem
```
</details>

### Exercise 2: String Interning Deduplicator
**Requirement:** Given a list of 100,000 strings containing only 5 unique categorical values, write a function that interns all strings and measures identity deduplication.

<details>
<summary>Show Solution</summary>

```python
import sys

def intern_categories(raw_list: list[str]) -> list[str]:
    return [sys.intern(s) for s in raw_list]

# Test
categories = ["PENDING", "PROCESSING", "SHIPPED", "DELIVERED", "CANCELLED"]
synthetic_data = [categories[i % 5] for i in range(1000)]

interned_data = intern_categories(synthetic_data)
# Verify all occurrences of "PENDING" share the exact same id
pending_ids = {id(s) for s in interned_data if s == "PENDING"}
print("Unique memory IDs for 'PENDING':", len(pending_ids))
assert len(pending_ids) == 1
```
</details>

### Exercise 3: Cross-Platform File Path Normalizer
**Requirement:** Write a function that inspects `platform.system()` and normalizes file paths to POSIX slashes (`/`) on Linux/macOS and backslashes (`\`) on Windows without using external libraries.

<details>
<summary>Show Solution</summary>

```python
import platform

def normalize_platform_path(path_str: str) -> str:
    system = platform.system()
    if system == "Windows":
        return path_str.replace("/", "\\")
    else:
        return path_str.replace("\\", "/")

# Test
p = "data/logs/app.log"
norm = normalize_platform_path(p)
print(f"Platform: {platform.system()} -> Path: {norm}")
```
</details>

### Exercise 4: Clean Exit Handler
**Requirement:** Write a function `graceful_exit(status_code: int, message: str)` that prints the message to `sys.stderr` and calls `sys.exit()` with the given status code.

<details>
<summary>Show Solution</summary>

```python
import sys

def graceful_exit(status_code: int, message: str):
    print(f"[FATAL] {message}", file=sys.stderr)
    sys.exit(status_code)

# Test via catching SystemExit
try:
    graceful_exit(2, "Invalid command-line parameters")
except SystemExit as e:
    print("Caught expected SystemExit with code:", e.code)
    assert e.code == 2
```
</details>

### Exercise 5: Recursion Limit Safety Guard
**Requirement:** Write a context manager `temporary_recursion_limit(new_limit: int)` that safely adjusts `sys.setrecursionlimit()` and restores the previous limit upon exit.

<details>
<summary>Show Solution</summary>

```python
import sys
import contextlib

@contextlib.contextmanager
def temporary_recursion_limit(new_limit: int):
    old_limit = sys.getrecursionlimit()
    sys.setrecursionlimit(new_limit)
    try:
        yield
    finally:
        sys.setrecursionlimit(old_limit)

# Test
print("Original limit:", sys.getrecursionlimit())
with temporary_recursion_limit(2500):
    print("Inside block limit:", sys.getrecursionlimit())
    assert sys.getrecursionlimit() == 2500

print("Restored limit:", sys.getrecursionlimit())
assert sys.getrecursionlimit() != 2500
```
</details>

### Exercise 6: In-Memory Module Mocking Context Manager
**Requirement:** Write a context manager `mock_module(module_name: str, **attributes)` that temporarily injects a synthetic module into `sys.modules` and deletes it upon context exit.

<details>
<summary>Show Solution</summary>

```python
import sys
import types
import contextlib

@contextlib.contextmanager
def mock_module(module_name: str, **attributes):
    original = sys.modules.get(module_name)
    fake_mod = types.ModuleType(module_name)
    fake_mod.__dict__.update(attributes)
    sys.modules[module_name] = fake_mod
    try:
        yield fake_mod
    finally:
        if original is None:
            del sys.modules[module_name]
        else:
            sys.modules[module_name] = original

# Test
with mock_module("cloud_secrets", API_KEY="fake_token_123"):
    import cloud_secrets
    print("Mocked Secret:", cloud_secrets.API_KEY)
    assert cloud_secrets.API_KEY == "fake_token_123"

assert "cloud_secrets" not in sys.modules
```
</details>
