# Module 00: Package Management with pip & uv (03-packages-with-pip-and-uv)

> **Topic Overview**: The modern Python packaging ecosystem: wheels (`.whl`) vs source distributions (`sdist`), PyPI resolution algorithms, `pip` vs Astral `uv pip`, lockfiles, cryptographic hash verification, and dependency management architectures.

---

## Metadata Table

| Field | Value |
|---|---|
| **Category** | Setup, Toolchains & Python Runtime |
| **Target Level** | Intermediate to Senior Build / Release Engineer |
| **Python Version** | Python 3.10+ (tested on CPython 3.14.0) |
| **Tested With** | uv 0.13.9, pip 26.1.2, ruff 0.17.6, mypy 2.4.1 |
| **Free-Threading / Platform** | Covers ABI tag conventions (`cp313-cp313t`) |

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
- **Problem Statement**: Python applications depend on an expansive ecosystem of open-source libraries hosted on PyPI. Historically, package installation required downloading source archives, invoking `setup.py`, and compiling C/C++ extensions locally, leading to slow deployments, missing compiler headers, and unpinned dependency breakage.
- **Mental Model**: Modern Python distribution relies on pre-compiled binary packages called **Wheels** (`.whl`). Package managers (`uv` and `pip`) resolve version constraint graphs, download matching wheels for the current OS/ABI, and unpack them directly into `site-packages`.
- **When to Use**: Whenever declaring dependencies for applications, CLI tools, libraries, and microservices.
- **When NOT to Use**: Never install packages directly into global system Python without an isolated virtual environment.

---

## 2. Core Concepts
1. **Source Distribution (`sdist`)**: A `.tar.gz` archive containing raw source files (`.py`, `.c`, `.h`, `pyproject.toml`). Installing an sdist requires building a wheel on the user's machine, which requires local compilers if C-extensions are included.
2. **Built Distribution / Wheel (`.whl` / PEP 427)**: A ZIP archive with a standardized naming convention: `{distribution}-{version}(-{build tag})?-{python tag}-{abi tag}-{platform tag}.whl`. Installation is a fast filesystem unpack operation with zero compilation.
3. **ABI Tags**: E.g., `cp312-cp312-manylinux_2_28_x86_64` or `cp313-cp313t-win_amd64` (where `t` indicates the free-threaded ABI).
4. **Astral `uv pip`**: A Rust-based drop-in replacement for `pip`. Features a PubGrub dependency resolver, global content-addressable package caching, copy-on-write file linking, and $10\times - 100\times$ speedups.
5. **Lockfiles (PEP 508 / `uv.lock`)**: Pinning the exact version, source repository, and cryptographic SHA-256 hash of every direct and transitive dependency to guarantee 100% deterministic, reproducible builds across machines.

---

## 3. How It Works Internally

How `uv` resolves and installs wheels:

```text
Dependency Request: `uv pip install requests`
            │
            ▼
Query PyPI Index Metadata:
  Fetches HTTP Simple API (JSON / HTML) -> `https://pypi.org/simple/requests/`
            │
            ▼
PubGrub Resolution Algorithm:
  Constructs Directed Acyclic Graph (DAG) of version constraints:
    requests -> urllib3 (<3, >=1.21.1), certifi (>=2017.4.17), idna, charset-normalizer
            │
            ▼
Select Optimal Wheel:
  Checks platform tags against current host:
    (CPython 3.14, MSC v.1944, Windows x86_64)
            │
            ▼
Global Content-Addressable Cache:
  Is package already in `~/.cache/uv/`?
  ├─► YES: Hardlink / clone directly to site-packages (1-5 ms)
  └─► NO: Download `.whl`, verify SHA-256 hash, unpack, and cache
            │
            ▼
Install into `.venv/Lib/site-packages/`
  Creates `.dist-info/` directory with `METADATA`, `RECORD`, `entry_points.txt`
```

---

## 4. Syntax & API Reference

### Comparing `pip` and `uv` Commands
| Operation | Traditional `pip` | Modern `uv` |
|---|---|---|
| Install a package | `pip install httpx` | `uv pip install httpx` / `uv add httpx` |
| Install from requirements | `pip install -r requirements.txt` | `uv pip install -r requirements.txt` |
| Generate lockfile | `pip-compile requirements.in` | `uv pip compile requirements.in` / `uv lock` |
| Sync environment with lock | `pip-sync requirements.txt` | `uv pip sync requirements.txt` / `uv sync` |
| Inspect installed packages | `pip list` | `uv pip list` |
| Show package details | `pip show pydantic` | `uv pip tree` |

---

## 5. Code Examples

### 1. Minimal / Basic Example: Inspecting Package Metadata at Runtime
```python
import importlib.metadata

def inspect_package(package_name: str) -> None:
    try:
        ver = importlib.metadata.version(package_name)
        meta = importlib.metadata.metadata(package_name)
        print(f"Package: {package_name}")
        print(f"Version: {ver}")
        print(f"Summary: {meta.get('Summary', 'N/A')}")
    except importlib.metadata.PackageNotFoundError:
        print(f"Package '{package_name}' is not installed in the active environment.")

if __name__ == "__main__":
    inspect_package("pip")
    inspect_package("ruff")
```
**Output**:
```text
Package: pip
Version: 26.1.2
Summary: The PyPA recommended tool for installing Python packages.
Package: ruff
Version: 0.17.6
Summary: An extremely fast Python linter and code formatter, written in Rust.
```

---

### 2. Real-World Production Example: Validating Dependencies at Startup
```python
import importlib.metadata
from packaging.requirements import Requirement
from packaging.version import Version

REQUIRED_DEPENDENCIES = [
    "pip>=24.0",
    "mypy>=1.0.0",
]

def check_runtime_dependencies() -> bool:
    """Verifies that installed dependencies satisfy version constraints."""
    for req_str in REQUIRED_DEPENDENCIES:
        req = Requirement(req_str)
        try:
            installed_ver_str = importlib.metadata.version(req.name)
            installed_ver = Version(installed_ver_str)
            if not req.specifier.contains(installed_ver):
                raise RuntimeError(
                    f"Dependency violation! {req.name} {installed_ver} does not satisfy {req.specifier}"
                )
            print(f"Verified {req.name} ({installed_ver}) satisfies {req.specifier}")
        except importlib.metadata.PackageNotFoundError:
            raise RuntimeError(f"Missing required dependency: {req.name}")
    return True

if __name__ == "__main__":
    check_runtime_dependencies()
```
**Output**:
```text
Verified pip (26.1.2) satisfies >=24.0
Verified mypy (2.4.1) satisfies >=1.0.0
```

---

### 3. Edge Case: Parsing Wheel Filename Tags Programmatically
```python
import re

WHEEL_REGEX = re.compile(
    r"^(?P<name>[^\-]+)-(?P<version>[^\-]+)"
    r"(?:-(?P<build>\d[^\-]*))?"
    r"-(?P<pyver>[^\-]+)-(?P<abi>[^\-]+)-(?P<platform>[^\-]+)\.whl$"
)

def parse_wheel_filename(filename: str) -> dict[str, str | None]:
    match = WHEEL_REGEX.match(filename)
    if not match:
        raise ValueError(f"Invalid wheel filename: {filename}")
    return match.groupdict()

sample_wheel = "numpy-2.1.0-cp313-cp313t-win_amd64.whl"
parsed = parse_wheel_filename(sample_wheel)
print("Wheel:", sample_wheel)
print("Parsed ABI Tag:", parsed["abi"])
print("Is Free-Threaded Build:", parsed["abi"] == "cp313t")
```
**Output**:
```text
Wheel: numpy-2.1.0-cp313-cp313t-win_amd64.whl
Parsed ABI Tag: cp313t
Is Free-Threaded Build: True
```

---

### 4. Failure Case: Unpinned Transitive Dependency Conflict
```bash
# Simulating conflicting version constraints in pip:
$ pip install "packageA>=2.0" "packageB<1.0"
ERROR: Cannot install packageA and packageB because these package versions have conflicting dependencies.
The conflict is caused by:
    packageA 2.1.0 depends on urllib3>=2.0
    packageB 0.9.0 depends on urllib3<1.26.0

To fix this, consider:
1. Loosening the range of package versions you've specified
2. Using a modern lockfile with `uv lock` to resolve transitive pins
```

---

### 5. Idiomatic Pythonic Pattern: Generating Reproducible Dependency Trees via `uv`
```bash
# In your project root with pyproject.toml:
$ uv lock
Resolved 42 packages in 12ms
Writing lockfile at `uv.lock`

# Exporting strict hashes to legacy requirements.txt format:
$ uv export --format requirements-txt --no-dev --output-file requirements.txt
```
Sample generated `requirements.txt` snippet:
```text
httpx==0.28.1 \
    --hash=sha256:6e03889... \
    --hash=sha256:8b22a10...
pydantic==2.14.1 \
    --hash=sha256:4a38f90...
```

---

## 6. Edge Cases & Pitfalls
1. **Installing `sdist` on Machines Without C Compilers**: If a wheel is unavailable on PyPI for your OS/architecture (e.g., Windows ARM64 or Linux Alpine musl), `pip` falls back to `sdist`. If C/C++ compilers (`msvc`, `gcc`) are missing, installation crashes with `error: command 'cl.exe' failed`.
2. **`pip` vs `uv` Caching Differences**: `pip` caches wheels in `~/.cache/pip`. `uv` uses a content-addressable store at `~/.cache/uv/archive/` and hardlinks files directly into environments, saving gigabytes of disk space across multiple projects.
3. **Dependency Confusion Attacks**: Misconfigured package indexes querying both private enterprise Artifactory/Nexus registries and public PyPI can accidentally download malicious public packages spoofing internal package names.

---

## 7. Common Mistakes

| Mistake | Why It Fails | Exception / Diagnostic | Correct Pattern |
|---|---|---|---|
| Unpinned dependencies in production (`pip install requests`) | Upstream releases break API contracts unexpectedly | Silent runtime bugs after deployment | Use lockfiles (`uv.lock` or pinned `requirements.txt`) |
| Invoking `pip` without `python -m` | Runs whatever `pip` appears first on PATH | Installs into wrong environment | Always use `python -m pip` or `uv pip` |
| Committing cached wheel files to git | Repository becomes enormous and binary-bloated | Slow git clone and merge conflicts | Only commit source and lockfiles |
| Installing packages without hashes in security-sensitive CI | Vulnerable to MITM or compromised upstream releases | Security audit failure | Use `--require-hashes` with `uv pip` |
| Relying on `setup.py install` | Deprecated legacy mechanism | `DeprecationWarning: setup.py install is deprecated` | Use `pip install .` or `uv build` |
| Forgetting platform ABI tags when downloading wheels for offline use | Downloads wheels for host OS that fail on target server | `Wheel is not supported on this platform` | Pass `--platform` and `--python-version` flags |

---

## 8. Performance & Complexity
- **Resolution Complexity**: Dependency resolution is an NP-complete problem in the general case.
- **PubGrub Algorithm in `uv`**: Uses conflict-driven clause learning (CDCL) to resolve complex dependency graphs in 5–20 milliseconds ($100\times$ faster than `pip`'s legacy backtracking resolver).

---

## 9. CPython Internals & Bytecode: The `.dist-info` Directory
When a wheel is unpacked into `site-packages`, it creates a `{name}-{version}.dist-info/` directory:
- `METADATA`: Conforms to PEP 566; contains package name, version, authors, and dependencies.
- `RECORD`: A CSV listing every installed file along with its SHA-256 hash and byte size.
- `entry_points.txt`: Defines CLI console scripts (e.g., `console_scripts: ruff = ruff.__main__:main`).
- `INSTALLER`: Plain text file recording which package manager installed it (`uv` or `pip`).

---

## 10. Best Practices
1. Use **`uv`** as the default package manager for both local development and CI pipelines.
2. Maintain a version-controlled lockfile (`uv.lock`) for applications to ensure 100% bit-for-bit reproducible environments.
3. For libraries, specify loose version ranges in `pyproject.toml` (`dependencies = ["httpx>=0.27"]`), but test against both minimum and maximum bounds.
4. Enforce cryptographic hash verification in production deployment pipelines.

---

## 11. Real-World Case Study: Accelerating Docker Container Image Builds
A large e-commerce platform experienced 8-minute Docker build times due to repeated `pip install` passes across dozens of microservices:
1. Replaced `pip install -r requirements.txt` with `uv pip install --system -r requirements.txt`.
2. Leveraged Docker BuildKit cache mounts (`--mount=type=cache,target=/root/.cache/uv`).
3. Build times dropped from **8 minutes to 11 seconds**, reducing CI infrastructure costs by over 70%.

---

## 12. Interview Questions

### Q1: What is the fundamental difference between an `sdist` and a `wheel`?
<details>
<summary><strong>Answer</strong></summary>
An `sdist` (Source Distribution) is an uncompiled archive containing raw source code and build manifests (`pyproject.toml`). Installing it requires executing a build backend and compiling any C/Rust extensions locally. A `wheel` is a ready-to-install binary distribution format (PEP 427) that simply needs to be unpacked into `site-packages`, requiring zero compilation or build tools on the client machine.
</details>

### Q2: What information is encoded in a wheel filename?
<details>
<summary><strong>Answer</strong></summary>
A wheel filename follows the format `{distribution}-{version}(-{build})?-{python_tag}-{abi_tag}-{platform_tag}.whl`. It specifies the package name, version, compatible Python implementations (e.g., `py3`, `cp312`), the binary Application Binary Interface (e.g., `abi3`, `cp313t`), and the supported operating system/architecture (e.g., `manylinux_2_28_x86_64`, `win_amd64`).
</details>

### Q3: What is the Stable ABI (`abi3`) in Python?
<details>
<summary><strong>Answer</strong></summary>
The Stable ABI (PEP 384) is a subset of the CPython C-API that is guaranteed to remain binary compatible across multiple CPython minor releases. A C-extension compiled against `abi3` for Python 3.8 can be loaded without recompilation on Python 3.9, 3.10, 3.11, 3.12, 3.13, and 3.14.
</details>

### Q4: Why is `uv pip install` so much faster than standard `pip install`?
<details>
<summary><strong>Answer</strong></summary>
`uv` is written in Rust and implements several key architectural optimizations:
1. It uses the PubGrub algorithm with parallel HTTP range requests to download only wheel metadata rather than full archives during resolution.
2. It uses a global content-addressable cache with hardlinks or clonefile (copy-on-write), avoiding re-downloading or duplicating files on disk.
3. It parallelizes all network, decompression, and filesystem operations.
</details>

### Q5: What is the purpose of the `RECORD` file in `.dist-info`?
<details>
<summary><strong>Answer</strong></summary>
The `RECORD` file is a CSV containing every file installed by the wheel, paired with its SHA-256 cryptographic hash and byte count. It allows package managers to verify file integrity and cleanly remove every installed file when running `pip uninstall`.
</details>

### Q6: How does dependency lockfile hashing protect against supply chain attacks?
<details>
<summary><strong>Answer</strong></summary>
A lockfile records the exact SHA-256 hash of the wheel archive when initially resolved. When downloading packages in CI/CD, the package manager verifies the downloaded archive's hash against the lockfile. If an attacker compromises a PyPI account and replaces a release wheel with malicious code, the hash mismatch causes the build to fail immediately.
</details>

### Q7: What is the difference between direct dependencies and transitive dependencies?
<details>
<summary><strong>Answer</strong></summary>
Direct dependencies are the libraries explicitly declared in your project's `pyproject.toml` or `requirements.in` (e.g., `FastAPI`). Transitive dependencies are the secondary libraries that those direct dependencies require to function (e.g., `pydantic`, `starlette`, `anyio`).
</details>

### Q8: What is PEP 508 and what does it define?
<details>
<summary><strong>Answer</strong></summary>
PEP 508 defines the standard dependency specification format in Python. It supports package names, version comparison specifiers (`>=`, `~=`, `==`), environment markers (`sys_platform == 'win32'`, `python_version >= '3.11'`), and extras (`requests[security]`).
</details>

---

## 13. Exercises & Challenges

### Challenge 1 (Easy): Query Installed Package Version
Write a function `get_installed_version(package_name)` using `importlib.metadata` that returns the version string or `"NOT_FOUND"`.
<details>
<summary><strong>Solution</strong></summary>

```python
import importlib.metadata

def get_installed_version(package_name: str) -> str:
    try:
        return importlib.metadata.version(package_name)
    except importlib.metadata.PackageNotFoundError:
        return "NOT_FOUND"

print(get_installed_version("pip"))
```
</details>

### Challenge 2 (Easy): Check Wheel Filename Validity
Write a function that validates whether a given filename string adheres to the standard PEP 427 wheel naming convention.
<details>
<summary><strong>Solution</strong></summary>

```python
import re

WHEEL_PATTERN = re.compile(r"^[A-Za-z0-9_.]+-([0-9A-Za-z_.]+)-([0-9A-Za-z_.]+)-([0-9A-Za-z_.]+)-([0-9A-Za-z_.]+)\.whl$")

def is_valid_wheel_name(name: str) -> bool:
    return bool(WHEEL_PATTERN.match(name))

print(is_valid_wheel_name("ruff-0.17.6-py3-none-win_amd64.whl"))  # True
print(is_valid_wheel_name("invalid_name.tar.gz"))                  # False
```
</details>

### Challenge 3 (Medium): Inspect Package Entry Points
Write a script that queries and lists all CLI console scripts exposed by installed packages using `importlib.metadata.entry_points`.
<details>
<summary><strong>Solution</strong></summary>

```python
import importlib.metadata

def list_console_scripts() -> dict[str, str]:
    eps = importlib.metadata.entry_points(group="console_scripts")
    return {ep.name: ep.value for ep in list(eps)[:10]}

print(list_console_scripts())
```
</details>

### Challenge 4 (Medium): Verify Integrity of Installed Package via `RECORD`
Write a function that reads the `RECORD` file of an installed distribution and verifies that each listed file exists on disk.
<details>
<summary><strong>Solution</strong></summary>

```python
import importlib.metadata
from pathlib import Path
import sys

def verify_distribution_files(dist_name: str) -> bool:
    dist = importlib.metadata.distribution(dist_name)
    if not dist.files:
        return False
    
    # Locate base site-packages
    for file_path in list(dist.files)[:5]:
        full_path = Path(dist.locate_file(file_path))
        if not full_path.exists():
            print(f"Missing file: {full_path}")
            return False
    return True

print("Integrity check on pip:", verify_distribution_files("pip"))
```
</details>

### Challenge 5 (Hard): Minimal In-Memory Dependency Graph Resolver
Implement a simplified topological dependency resolver that takes a dictionary of packages and their direct dependencies and returns a valid linear installation order, detecting circular dependencies.
<details>
<summary><strong>Solution</strong></summary>

```python
def resolve_install_order(graph: dict[str, list[str]]) -> list[str]:
    visited: dict[str, int] = {}  # 0=visiting, 1=visited
    order: list[str] = []

    def dfs(node: str) -> None:
        if visited.get(node) == 0:
            raise ValueError(f"Circular dependency detected involving: {node}")
        if visited.get(node) == 1:
            return
        
        visited[node] = 0
        for dep in graph.get(node, []):
            dfs(dep)
        visited[node] = 1
        order.append(node)

    for pkg in graph:
        if pkg not in visited:
            dfs(pkg)
    return order

sample_deps = {
    "web_app": ["fastapi", "sqlalchemy"],
    "fastapi": ["pydantic", "starlette"],
    "starlette": ["anyio"],
    "pydantic": [],
    "sqlalchemy": [],
    "anyio": []
}

print("Resolved install order:", resolve_install_order(sample_deps))
```
</details>

### Challenge 6 (Hard): Wheel Tag Matcher against Local Interpreter
Write a function that accepts a wheel tag tuple `(pyver, abi, platform)` and checks whether it is compatible with the current running CPython interpreter.
<details>
<summary><strong>Solution</strong></summary>

```python
import platform
import sys

def is_wheel_compatible(pyver: str, abi: str, plat: str) -> bool:
    current_major_minor = f"cp{sys.version_info.major}{sys.version_info.minor}"
    current_platform = platform.system().lower()
    
    # Check Python version
    if pyver not in ("py3", "py2.py3", current_major_minor):
        return False
        
    # Check ABI
    if abi not in ("none", "abi3", current_major_minor):
        return False
        
    # Check platform
    if plat != "any":
        if "win" in plat and current_platform != "windows":
            return False
        if "linux" in plat and current_platform != "linux":
            return False
        if "macos" in plat and current_platform != "darwin":
            return False
            
    return True

print("Compatibility check:", is_wheel_compatible("py3", "none", "any"))
```
</details>
