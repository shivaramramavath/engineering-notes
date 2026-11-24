# The __main__.py Entry Point and Executable Packages

- **Topic**: `06-modules-and-packages`
- **Python version**: 3.12+ (tested on CPython 3.14.0)
- **Tested with**: CPython 3.12, 3.13, 3.14 (standard & free-threaded builds)
- **Free-threading notes**: Compatible. Script execution via `-m` runs in the main thread with standard initialization locks.

---

## 1. Concept Overview

In Python, a package or directory can be made directly executable from the command line by including a special entry point file named **`__main__.py`**.

When a user executes:
```bash
python -m my_package
```
or executes a directory or `.zip` archive containing Python code:
```bash
python path/to/my_package/
python app.zip
```
CPython looks for `__main__.py` inside that package, directory, or zip archive and executes it as the top-level script, setting `__name__ = "__main__"`.

This convention is standard across Python CLI tools and libraries:
- `python -m http.server` runs Python's built-in HTTP file server.
- `python -m pip install ...` ensures you run the pip executable tied directly to that specific Python interpreter.
- `python -m unittest` discovers and runs tests.
- `python -m venv myenv` creates virtual environments.

```
Execution Command: python -m my_package
                             |
                             v
               Does my_package have __main__.py?
                     /                \
                   YES                 NO
                   /                    \
   Executes my_package/__main__.py       Raises:
   with __name__ == "__main__"           No module named my_package.__main__;
                                         'my_package' is a package and cannot
                                         be directly executed
```

---

## 2. Key Mechanics & Architecture

### What Happens Under the Hood with `-m`
When `python -m my_package` is executed:
1. Python searches `sys.path` for `my_package`.
2. It imports `my_package`, executing its `__init__.py` first (if it's a regular package).
3. It locates and executes `my_package/__main__.py`.
4. Inside `__main__.py`:
   - `__name__` is set to `"__main__"`.
   - `__package__` is set to `"my_package"`.
   - `__file__` is set to the full path of `__main__.py`.
5. Relative imports (e.g. `from .cli import main`) **work seamlessly** because `__package__` is populated!

### Zipapp Integration (PEP 441)
Python natively supports executing standalone `.zip` archives containing an application and a `__main__.py` file at the root. The standard library module `zipapp` bundles directories into standalone single-file executables.

---

## 3. Real-World Code Examples

### Example 1: Idiomatic `__main__.py` CLI Entry Point Pattern
```python
# my_cli_app/__main__.py
import sys

def main() -> int:
    args = sys.argv[1:]
    if not args:
        print("Usage: python -m my_cli_app <command>")
        return 1
    command = args[0]
    print(f"Executing command: '{command}'")
    return 0

if __name__ == "__main__":
    sys.exit(main())
```

### Example 2: Structuring Package CLI with `argparse`
```python
# my_package/cli.py
import argparse

def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="my_tool", description="Production utility tool")
    parser.add_argument("--port", type=int, default=8000, help="Server port")
    parser.add_argument("--debug", action="store_true", help="Enable debug mode")
    return parser

# my_package/__main__.py
"""
import sys
from .cli import create_parser

def main():
    parser = create_parser()
    args = parser.parse_args()
    print(f"Running server on port {args.port}, debug={args.debug}")

if __name__ == "__main__":
    main()
"""
```

### Example 3: Creating an Executable Zipapp via `zipapp`
```python
import zipapp
import tempfile
import pathlib
import subprocess
import sys

with tempfile.TemporaryDirectory() as temp_dir:
    src_dir = pathlib.Path(temp_dir) / "app"
    src_dir.mkdir()
    
    # Create the main entry point
    (src_dir / "__main__.py").write_text(
        "import sys\n"
        "print('Hello from standalone zipapp executable!')\n"
        "sys.exit(0)\n"
    )
    
    archive_path = pathlib.Path(temp_dir) / "app.pyz"
    
    # Bundle into executable zipapp
    zipapp.create_archive(src_dir, target=archive_path)
    print(f"Created zipapp archive: {archive_path.name}")
    
    # Execute the zipapp with python
    result = subprocess.run([sys.executable, str(archive_path)], capture_output=True, text=True)
    print(f"Execution output: {result.stdout.strip()}")
```
Output:
```
Created zipapp archive: app.pyz
Execution output: Hello from standalone zipapp executable!
```

### Example 4: The if `__name__ == "__main__"` Idiom Inside `__main__.py`
```python
# While __main__.py is executed when invoked via -m, it is ALSO possible
# for another module to import it: 'import my_package.__main__'.
# Therefore, wrapping execution in if __name__ == "__main__": is still best practice!

def run():
    print("Application running...")

if __name__ == "__main__":
    run()
```

### Example 5: Forwarding Exit Codes Safely
```python
import sys

def main() -> int:
    try:
        # Perform critical task
        success = True
        return 0 if success else 1
    except Exception as err:
        print(f"Fatal error: {err}", file=sys.stderr)
        return 2

# Calling sys.exit(main()) ensures the shell receives the proper exit code
# (0 for success, non-zero for failure)
```

---

## 4. Common Anti-Patterns & Gotchas

| Bad Practice | Root Cause | Clean Solution |
| :--- | :--- | :--- |
| Putting heavy business logic directly inside `__main__.py` | Hard to unit-test; cannot be imported cleanly without running CLI code | Put CLI logic in `cli.py`; keep `__main__.py` as a 3-line wrapper calling `cli.main()` |
| Omission of `sys.exit(main())` in `__main__.py` | Uncaught errors or failure states may exit with code 0 | Return exit codes (`0`, `1`, `2`) from `main()` and wrap in `sys.exit(main())` |
| Attempting `python -m my_package` without `__main__.py` | Raises `No module named my_package.__main__` | Add `__main__.py` to the package directory |
| Hardcoding absolute paths inside zipapps | Breaks portability when moved to other machines | Use relative imports or `pathlib.Path(__file__).parent` |
| Running `python package/__main__.py` directly | Sets `__package__ = None`, breaking relative imports inside `__main__.py` | Always run using `python -m package` |
| Forgetting that `__init__.py` runs before `__main__.py` | Package-level side effects in `__init__.py` execute before CLI parsing | Keep `__init__.py` lightweight |

---

## 5. Interview Q&A

<details>
<summary>1. What is the role of __main__.py in a Python package?</summary>
<code>__main__.py</code> serves as the execution entry point when a package or directory is invoked directly using <code>python -m &lt;package&gt;</code>, <code>python &lt;directory&gt;</code>, or <code>python &lt;archive.zip&gt;</code>. It allows packages to function as both importable libraries and runnable CLI applications.
</details>

<details>
<summary>2. Why is python -m pip preferred over invoking pip directly?</summary>
Running <code>python -m pip</code> guarantees that the <code>pip</code> tool executing is the one installed inside the exact Python environment currently running. When multiple Python installations exist (e.g. system Python, Homebrew, virtualenvs), running bare <code>pip</code> often accidentally invokes a different environment's binary.
</details>

<details>
<summary>3. Does __init__.py execute when running python -m my_package?</summary>
Yes. Because <code>my_package</code> must be imported to locate its submodules, Python executes <code>my_package/__init__.py</code> first, and then executes <code>my_package/__main__.py</code>.
</details>

<details>
<summary>4. What is the value of __package__ inside __main__.py when invoked with -m?</summary>
When invoked with <code>python -m my_package</code>, Python sets <code>__name__ = "__main__"</code> and sets <code>__package__ = "my_package"</code>. This allows relative imports (e.g. <code>from .core import run</code>) inside <code>__main__.py</code> to resolve correctly.
</details>

<details>
<summary>5. What error occurs if you run python -m on a package without __main__.py?</summary>
Python raises <code>No module named &lt;package&gt;.__main__; '&lt;package&gt;' is a package and cannot be directly executed</code>.
</details>

<details>
<summary>6. How does zipapp (PEP 441) work with __main__.py?</summary>
A zipapp is a zip archive containing a Python application. When Python executes a zip file (<code>python app.pyz</code>), it mounts the zip archive on <code>sys.path</code>, finds <code>__main__.py</code> at the root of the archive, and executes it as a script.
</details>

<details>
<summary>7. Why should you still include if __name__ == "__main__": inside __main__.py?</summary>
Although <code>__main__.py</code> is intended to be executed directly, another module could theoretically import it (e.g. <code>import my_pkg.__main__</code> in tests or documentation extractors). Wrapping execution in <code>if __name__ == "__main__":</code> prevents unintended execution during imports.
</details>

<details>
<summary>8. How do console script entry points in pyproject.toml compare to __main__.py?</summary>
Console script entry points (defined in <code>pyproject.toml</code> under <code>[project.scripts]</code>) generate standalone OS executable wrappers (like <code>mytool.exe</code> or <code>/usr/local/bin/mytool</code>) during <code>pip install</code>. <code>__main__.py</code> provides the <code>python -m mytool</code> interface. Production packages typically provide both.
</details>

---

## 6. Practical Challenges

<details>
<summary>Challenge 1: Minimal Runnable Package Entry Point</summary>

Write a test that programmatically invokes an in-memory package's `__main__.py` and verifies exit code handling.

```python
import sys

def mock_cli_entry():
    return 0

assert mock_cli_entry() == 0
```
</details>

<details>
<summary>Challenge 2: Argument Parsing Entry Point</summary>

Build a function `cli_main(args: list[str]) -> int` that parses `--count N` and prints `N` times, returning 0 on success and 2 on bad arguments.

```python
import argparse

def cli_main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--count", type=int, default=1)
    try:
        args = parser.parse_args(argv)
        return 0
    except SystemExit:
        return 2

assert cli_main(["--count", "5"]) == 0
assert cli_main(["--invalid"]) == 2
```
</details>

<details>
<summary>Challenge 3: Programmatic Zipapp Generator</summary>

Use the `zipapp` module to generate a temporary `.pyz` file and verify it is a valid zip file containing `__main__.py`.

```python
import zipapp
import zipfile
import tempfile
import pathlib

with tempfile.TemporaryDirectory() as td:
    src = pathlib.Path(td) / "src"
    src.mkdir()
    (src / "__main__.py").write_text("print('zipapp works')\n")
    target = pathlib.Path(td) / "out.pyz"
    zipapp.create_archive(src, target=target)

    assert target.exists()
    with zipfile.ZipFile(target) as zf:
        assert "__main__.py" in zf.namelist()
```
</details>

<details>
<summary>Challenge 4: Check if Package is Executable</summary>

Write a function `is_package_executable(pkg_name: str) -> bool` using `importlib.util.find_spec` that checks if a package contains `__main__.py`.

```python
import importlib.util

def is_package_executable(pkg_name: str) -> bool:
    spec = importlib.util.find_spec(f"{pkg_name}.__main__")
    return spec is not None

assert is_package_executable("json") is False
assert is_package_executable("http.server") is True
```
</details>

<details>
<summary>Challenge 5: Exit Code Propagation Test</summary>

Verify that `sys.exit(code)` raises `SystemExit` with the exact integer code.

```python
def run_and_catch_exit(fn):
    try:
        fn()
        return 0
    except SystemExit as e:
        return e.code

assert run_and_catch_exit(lambda: sys.exit(42)) == 42
```
</details>

<details>
<summary>Challenge 6: Standalone Shebang Header for Zipapp</summary>

Create an executable zipapp with a Unix shebang (`#!/usr/bin/env python3`) using `zipapp.create_archive(..., interpreter='/usr/bin/env python3')`.

```python
with tempfile.TemporaryDirectory() as td:
    src = pathlib.Path(td) / "src"
    src.mkdir()
    (src / "__main__.py").write_text("pass\n")
    target = pathlib.Path(td) / "app"
    zipapp.create_archive(src, target=target, interpreter="/usr/bin/env python3")

    with open(target, "rb") as f:
        header = f.readline()
        assert header.startswith(b"#!")
```
</details>
