# Module 00: Python Virtual Environments (02-virtual-environments)

> **Topic Overview**: The internal architecture of Python virtual environments: `pyvenv.cfg`, site-packages isolation, interpreter symlinks vs redirection stubs, activation scripts, the standard `venv` module, and high-performance `uv venv`.

---

## Metadata Table

| Field | Value |
|---|---|
| **Category** | Setup, Toolchains & Python Runtime |
| **Target Level** | Beginner to Senior DevOps Engineer |
| **Python Version** | Python 3.10+ (tested on CPython 3.14.0) |
| **Tested With** | CPython 3.14.0, uv 0.13.9, Windows AMD64 |
| **Free-Threading / Platform** | Platform independent |

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
- **Problem Statement**: Different Python applications depend on incompatible versions of third-party libraries (e.g., App A requires `pydantic<2.0`, while App B requires `pydantic>=2.10`). Installing packages globally leads to dependency collisions, broken system tools, and non-reproducible environments.
- **Mental Model**: A virtual environment is an isolated directory tree containing its own Python executable stub, configuration file (`pyvenv.cfg`), and dedicated `site-packages` directory. It does *not* copy the standard library; it redirects library lookups.
- **When to Use**: Every standalone Python project, script with external dependencies, and local development workspace must reside in a dedicated virtual environment.
- **When NOT to Use**: Ephemeral single-file CLI scripts requiring no third-party dependencies.

---

## 2. Core Concepts
1. **The `pyvenv.cfg` File**: The landmark file residing at the root of a virtual environment (or adjacent to the executable). CPython looks for this file during startup to determine if it is executing inside a virtual environment.
2. **`sys.prefix` vs `sys.base_prefix`**:
   - In global Python: `sys.prefix == sys.base_prefix`.
   - In a virtualenv: `sys.prefix` points to the virtual environment folder; `sys.base_prefix` points to the underlying base Python installation.
3. **Activation Scripts**: Shell scripts (`activate`, `activate.bat`, `Activate.ps1`) that modify the current shell's `PATH` variable to prepend the virtualenv's `bin/` or `Scripts/` directory and set the `VIRTUAL_ENV` environment variable. Note that activation is purely an optional shell convenience—directly invoking `<venv>/Scripts/python.exe` executes inside the virtual environment without activation.
4. **`include-system-site-packages`**: A boolean flag in `pyvenv.cfg`. When set to `false` (default), packages installed in the global base interpreter are invisible to the virtual environment.

---

## 3. How It Works Internally

CPython's startup sequence for virtual environment detection:

```text
Invocation: .venv\Scripts\python.exe script.py
            │
            ▼
    [ C Runtime Entrypoint ]
            │
            ▼
Locate Executable Directory:
    .venv\Scripts\
            │
            ▼
Check for Landmark File:
    Look for .venv\pyvenv.cfg or .venv\Scripts\pyvenv.cfg
            │
    ┌───────┴───────┐
    │ Found         │ Not Found
    ▼               ▼
[ Virtualenv Mode ] [ Global System Mode ]
Read pyvenv.cfg:
  home = C:\Python314
  include-system-site-packages = false
  version = 3.14.0
            │
            ▼
Set Paths:
  sys.prefix = ".venv"
  sys.base_prefix = "C:\Python314"
  sys.path = [
      ".venv/Lib/site-packages",
      "C:\Python314\Lib",
      ...
  ]
```

---

## 4. Syntax & API Reference

### CLI Commands
```bash
# Standard library venv creation
python -m venv .venv

# High-speed creation via uv (10x - 50x faster)
uv venv

# Create virtualenv with a specific Python version via uv
uv venv --python 3.12 .venv312

# Shell Activation (PowerShell)
.venv\Scripts\Activate.ps1

# Shell Activation (Bash / POSIX)
source .venv/bin/activate

# Deactivation
deactivate
```

### Python Runtime Introspection API
```python
import sys

# Check if executing inside a virtual environment
is_venv = sys.prefix != getattr(sys, "base_prefix", sys.prefix)

# Access root path of virtualenv
venv_path = sys.prefix if is_venv else None
```

---

## 5. Code Examples

### 1. Minimal / Basic Example: Introspecting Virtual Environment State
```python
import sys
import os

def check_venv() -> None:
    in_venv = sys.prefix != getattr(sys, "base_prefix", sys.prefix)
    print(f"Running inside Virtual Environment: {in_venv}")
    print(f"Prefix (sys.prefix): {sys.prefix}")
    print(f"Base Prefix (sys.base_prefix): {getattr(sys, 'base_prefix', sys.prefix)}")
    print(f"VIRTUAL_ENV env var: {os.environ.get('VIRTUAL_ENV', 'Not Set')}")

if __name__ == "__main__":
    check_venv()
```
**Output**:
```text
Running inside Virtual Environment: False
Prefix (sys.prefix): C:\Python314
Base Prefix (sys.base_prefix): C:\Python314
VIRTUAL_ENV env var: Not Set
```

---

### 2. Real-World Production Example: Parsing `pyvenv.cfg` Safely
```python
from pathlib import Path
import sys

def parse_pyvenv_cfg() -> dict[str, str]:
    """Reads and parses pyvenv.cfg from the active environment root."""
    prefix_path = Path(sys.prefix)
    cfg_file = prefix_path / "pyvenv.cfg"
    if not cfg_file.exists():
        cfg_file = prefix_path.parent / "pyvenv.cfg"
        
    if not cfg_file.exists():
        return {"status": "global_environment_no_cfg"}
        
    data: dict[str, str] = {}
    for line in cfg_file.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if line and "=" in line and not line.startswith("#"):
            key, val = line.split("=", 1)
            data[key.strip()] = val.strip()
    return data

print(parse_pyvenv_cfg())
```
**Output**:
```text
{'status': 'global_environment_no_cfg'}
```

---

### 3. Edge Case: Programmatic Virtual Environment Creation via `venv` Module
```python
import venv
from pathlib import Path

def create_isolated_env(target_dir: Path) -> None:
    builder = venv.EnvBuilder(
        system_site_packages=False,
        clear=True,
        symlinks=False,
        with_pip=False  # Ultra-fast creation without bootstrapping pip
    )
    builder.create(target_dir)
    cfg = target_dir / "pyvenv.cfg"
    print(f"Created isolated environment at: {target_dir}")
    print(f"pyvenv.cfg generated: {cfg.exists()}")

# Demonstration with temp target
test_env = Path("temp_test_venv")
create_isolated_env(test_env)

# Cleanup
import shutil
shutil.rmtree(test_env)
print("Temporary venv cleaned up successfully.")
```
**Output**:
```text
Created isolated environment at: temp_test_venv
pyvenv.cfg generated: True
Temporary venv cleaned up successfully.
```

---

### 4. Failure Case: Attempting to Move / Rename a Virtual Environment Directory
```python
# A common issue when users move or rename a .venv directory:
# The pyvenv.cfg 'home' path or executable stubs hold hardcoded absolute paths.
import sys

def test_venv_validity() -> None:
    executable_path = sys.executable
    prefix_path = sys.prefix
    # If a venv is moved, executable directory no longer matches sys.prefix
    if sys.prefix != sys.base_prefix:
        if not executable_path.startswith(prefix_path):
            raise RuntimeError(
                f"Virtual environment corruption detected! "
                f"Executable ({executable_path}) is outside sys.prefix ({prefix_path}). "
                f"Virtual environments cannot be moved; recreate with 'uv venv'."
            )
    print("Environment paths are consistent.")

test_venv_validity()
```
**Output**:
```text
Environment paths are consistent.
```

---

### 5. Idiomatic Pythonic Pattern: Enforcing Script Execution Inside a Virtualenv
```python
import sys

def enforce_virtualenv() -> None:
    """Guards application startup to prevent accidental execution in global Python."""
    if sys.prefix == getattr(sys, "base_prefix", sys.prefix):
        print(
            "ERROR: This application must be run inside an isolated virtual environment!\n"
            "Create one with: uv venv && source .venv/bin/activate (or .venv\\Scripts\\Activate.ps1)",
            file=sys.stderr
        )
        # In production applications, call sys.exit(1) here
    else:
        print("Running in isolated environment. Proceeding with application boot.")

enforce_virtualenv()
```
**Output**:
```text
ERROR: This application must be run inside an isolated virtual environment!
Create one with: uv venv && source .venv/bin/activate (or .venv\Scripts\Activate.ps1)
```

---

## 6. Edge Cases & Pitfalls
1. **Virtual Environments Are Not Relocatable**: Renaming or moving a `.venv` directory breaks hardcoded absolute paths inside binary wrappers and activation scripts. If moved, delete and recreate it.
2. **Symlink Failures on Windows**: On Windows systems without Developer Mode enabled, creating symlinks requires administrator privileges. Standard `venv` falls back to copying `python.exe` stubs.
3. **Activation Is Not Mandatory**: Calling `<venv>/bin/python script.py` or `.venv\Scripts\python.exe script.py` runs inside the virtualenv regardless of whether the shell has run `activate`.

---

## 7. Common Mistakes

| Mistake | Why It Fails | Exception / Diagnostic | Correct Pattern |
|---|---|---|---|
| Committing `.venv` to Git | Bloats repo with hundreds of MBs of platform-specific binaries | Git repository pollution and merge conflicts | Add `.venv/` to `.gitignore` |
| Moving or renaming `.venv` folder | Hardcoded paths in `pyvenv.cfg` and scripts point to nonexistent directories | `Fatal error in launcher: Unable to create process` | Delete and recreate with `uv venv` |
| Forgetting to activate in a new terminal session | Commands default back to global Python | `ModuleNotFoundError: No module named 'xyz'` | Run via `.venv/bin/python` or use `uv run` |
| Using `pip` from global Python while venv is active | Installs packages into the global machine site-packages | Dependency not visible to virtual environment | Verify with `which pip` or use `python -m pip` |
| Relying on `virtualenv` third-party package on modern Python | Python 3.3+ has `venv` built into the standard library | Unnecessary third-party dependency | Use `python -m venv` or `uv venv` |
| Installing packages with `sudo` while virtualenv is active | `sudo` resets `PATH` back to root global environment | Corrupts system site-packages | Never use `sudo` with virtual environments |

---

## 8. Performance & Complexity
- **`venv` Creation Overhead**: Standard `python -m venv` takes 2–5 seconds (slow due to `ensurepip` wheel extraction).
- **`uv venv` Creation Overhead**: Astral `uv venv` completes in ~10–30 milliseconds ($100\times$ faster) by leveraging fast filesystem links and omitting pip bootstrapping unless requested.

---

## 9. CPython Internals & Bytecode: How `site.py` Manages `sys.path`
During Python initialization, standard library module `site.py` is automatically imported:
1. It reads `sys.prefix` and verifies whether `pyvenv.cfg` exists.
2. If `include-system-site-packages = false`, it adds only `<venv>/lib/site-packages` to `sys.path`.
3. It executes any `.pth` path configuration files present in site-packages.
4. Calling `python -S` disables `site.py` processing for minimal, ultra-fast interpreter startup.

---

## 10. Best Practices
1. Name the project virtual environment `.venv` and place it at the project root.
2. Always add `.venv/` to `.gitignore`.
3. Prefer `uv venv` over `python -m venv` for near-instant environment creation.
4. In scripts and Docker containers, invoke `<venv>/bin/python` directly rather than relying on shell activation scripts.

---

## 11. Real-World Case Study: Docker Multi-Stage Virtualenv Builds
In enterprise microservices, running applications as root or bloating production containers is prohibited. The standard production pattern:
1. **Builder Stage**: Install `uv`, run `uv venv /opt/venv`, and install dependencies via `uv pip install -r requirements.txt`.
2. **Runtime Stage**: Copy ONLY `/opt/venv` from the builder stage into a minimal `python:3.12-slim` image.
3. Set `ENV PATH="/opt/venv/bin:$PATH"`.
4. The application boots instantly without build tools or network access.

---

## 12. Interview Questions

### Q1: What is the exact purpose of `pyvenv.cfg`?
<details>
<summary><strong>Answer</strong></summary>
`pyvenv.cfg` is the landmark configuration file that indicates to CPython during startup that it is running inside a virtual environment. It specifies the path to the underlying base Python installation (`home`), whether global site-packages are visible (`include-system-site-packages`), and the Python version.
</details>

### Q2: What actually happens when you run `source .venv/bin/activate`?
<details>
<summary><strong>Answer</strong></summary>
The activation script is a shell script that modifies the current shell session's environment:
1. It prepends the virtual environment's `bin/` (or `Scripts/` on Windows) directory to the system `PATH`.
2. It sets the `VIRTUAL_ENV` environment variable to the virtualenv directory.
3. It modifies the shell prompt string (e.g., adding `(.venv)`).
4. It defines the `deactivate` function to restore the previous environment.
</details>

### Q3: Is activating a virtual environment required to execute scripts within it?
<details>
<summary><strong>Answer</strong></summary>
No. Shell activation is purely a convenience that adjusts the shell's `PATH`. Directly executing the virtual environment's binary (e.g., `./.venv/bin/python script.py` or `.\.venv\Scripts\python.exe script.py`) automatically discovers `pyvenv.cfg` and sets `sys.prefix` correctly.
</details>

### Q4: How does Python determine whether it is executing inside a virtual environment in code?
<details>
<summary><strong>Answer</strong></summary>
By comparing `sys.prefix` and `sys.base_prefix`:
```python
is_in_venv = sys.prefix != getattr(sys, "base_prefix", sys.prefix)
```
Inside a virtualenv, `sys.prefix` points to the virtualenv directory while `sys.base_prefix` points to the global Python install. Outside a virtualenv, both are identical.
</details>

### Q5: Why is `uv venv` so much faster than `python -m venv`?
<details>
<summary><strong>Answer</strong></summary>
Standard `python -m venv` spends hundreds of milliseconds unzipping and installing bundled `pip` and `setuptools` wheels via `ensurepip`. `uv venv` is written in Rust, creates directory structures in microseconds, writes `pyvenv.cfg` directly, uses hardlinks/reflinks, and does not unpack pip unless explicitly requested.
</details>

### Q6: What does the `--system-site-packages` flag do?
<details>
<summary><strong>Answer</strong></summary>
It sets `include-system-site-packages = true` in `pyvenv.cfg`. When enabled, Python searches the virtual environment's site-packages first, but falls back to the host operating system's global site-packages if a requested package is not found locally.
</details>

### Q7: Why should virtual environments never be committed to source control?
<details>
<summary><strong>Answer</strong></summary>
Virtual environments contain platform-specific compiled binaries, dynamic libraries, and hardcoded absolute filesystem paths. They are large (often hundreds of megabytes) and non-portable across operating systems or even different filesystem locations on the same machine.
</details>

### Q8: What is a `.pth` file and how does `site.py` process it?
<details>
<summary><strong>Answer</strong></summary>
A `.pth` (path configuration) file is a plain text file placed in `site-packages`. Each line containing a directory path is automatically validated and appended to `sys.path` during startup by `site.py`. Lines beginning with `import ` are executed as Python code, a mechanism historically used by tools like `editable` installs.
</details>

---

## 13. Exercises & Challenges

### Challenge 1 (Easy): Detecting Virtualenv Execution
Write a utility function `get_environment_type()` that returns `"virtualenv"` if running in a virtual environment, or `"system"` if running globally.
<details>
<summary><strong>Solution</strong></summary>

```python
import sys

def get_environment_type() -> str:
    if sys.prefix != getattr(sys, "base_prefix", sys.prefix):
        return "virtualenv"
    return "system"

print(f"Environment type: {get_environment_type()}")
```
</details>

### Challenge 2 (Easy): Extracting Virtualenv Name
Write a function that extracts and returns the name of the active virtual environment directory, or `None` if running globally.
<details>
<summary><strong>Solution</strong></summary>

```python
from pathlib import Path
import sys

def get_active_venv_name() -> str | None:
    if sys.prefix != getattr(sys, "base_prefix", sys.prefix):
        return Path(sys.prefix).name
    return None

print(f"Active venv: {get_active_venv_name()}")
```
</details>

### Challenge 3 (Medium): Inspecting Installed Packages in Site-Packages
Write a function that returns a list of all `.dist-info` distribution folders present in the active environment's `site-packages`.
<details>
<summary><strong>Solution</strong></summary>

```python
from pathlib import Path
import sys

def list_site_packages_distributions() -> list[str]:
    dists = []
    for path_entry in sys.path:
        p = Path(path_entry)
        if p.is_dir() and p.name in ("site-packages", "dist-packages"):
            for dist in p.glob("*.dist-info"):
                dists.append(dist.stem)
    return sorted(dists)

print(f"Found distributions: {len(list_site_packages_distributions())}")
```
</details>

### Challenge 4 (Medium): Programmatic Virtualenv Verification Guard
Write a decorator `@require_virtualenv` that prevents a function from executing and raises `EnvironmentError` if called from global Python.
<details>
<summary><strong>Solution</strong></summary>

```python
from collections.abc import Callable
import functools
import sys
from typing import TypeVar

F = TypeVar("F", bound=Callable[..., object])

def require_virtualenv(func: F) -> F:
    @functools.wraps(func)
    def wrapper(*args: object, **kwargs: object) -> object:
        if sys.prefix == getattr(sys, "base_prefix", sys.prefix):
            raise EnvironmentError("Execution blocked: must run inside a virtual environment.")
        return func(*args, **kwargs)
    return wrapper  # type: ignore[return-value]

@require_virtualenv
def deploy_task() -> str:
    return "Task deployed safely."
```
</details>

### Challenge 5 (Hard): Replicating `venv.create()` Minimal Structure
Write a script that creates a compliant minimal virtual environment manually without importing `venv`: creating the directory structure, writing `pyvenv.cfg`, and creating appropriate script directory paths.
<details>
<summary><strong>Solution</strong></summary>

```python
from pathlib import Path
import sys

def manual_venv_scaffold(target: Path) -> Path:
    target.mkdir(parents=True, exist_ok=True)
    scripts_dir = target / ("Scripts" if sys.platform == "win32" else "bin")
    scripts_dir.mkdir(exist_ok=True)
    lib_dir = target / "Lib" / "site-packages" if sys.platform == "win32" else target / "lib" / f"python{sys.version_info.major}.{sys.version_info.minor}" / "site-packages"
    lib_dir.mkdir(parents=True, exist_ok=True)
    
    cfg = target / "pyvenv.cfg"
    cfg.write_text(
        f"home = {sys.base_prefix}\n"
        f"include-system-site-packages = false\n"
        f"version = {sys.version_info.major}.{sys.version_info.minor}.{sys.version_info.micro}\n",
        encoding="utf-8"
    )
    return cfg

dest = Path("temp_manual_venv")
manual_venv_scaffold(dest)
print("Manual venv scaffolded:", (dest / "pyvenv.cfg").exists())
import shutil
shutil.rmtree(dest)
```
</details>

### Challenge 6 (Hard): Detecting Corrupted Virtual Environment
Write a diagnostic tool that checks a virtual environment path for common failure modes: missing `pyvenv.cfg`, broken base interpreter links, or missing executable stubs.
<details>
<summary><strong>Solution</strong></summary>

```python
from pathlib import Path
import sys

def diagnose_venv(venv_root: Path) -> list[str]:
    issues = []
    cfg = venv_root / "pyvenv.cfg"
    if not cfg.exists():
        issues.append("Missing pyvenv.cfg")
    else:
        content = cfg.read_text(encoding="utf-8")
        if "home =" not in content:
            issues.append("pyvenv.cfg missing 'home' key")
            
    scripts = venv_root / ("Scripts" if sys.platform == "win32" else "bin")
    if not scripts.exists():
        issues.append(f"Missing scripts directory: {scripts.name}")
        
    return issues

print("Diagnostics on non-existent venv:", diagnose_venv(Path("non_existent_venv")))
```
</details>
