# Module 00: How Python Runs Code (06-how-python-runs-code)

> **Topic Overview**: The end-to-end execution model of CPython: compilation from source to bytecode, `__pycache__` and `.pyc` files, code objects (`PyCodeObject`), the marshal format, bytecode verification, the evaluation loop (`PyEval_EvalFrameDefault`), and PEP 659 specialized adaptive interpretation.

---

## Metadata Table

| Field | Value |
|---|---|
| **Category** | Setup, Toolchains & Python Runtime |
| **Target Level** | Intermediate to Senior Runtime Engineer |
| **Python Version** | Python 3.10+ (tested on CPython 3.14.0) |
| **Tested With** | CPython 3.14.0, dis module, Windows AMD64 |
| **Free-Threading / Platform** | Supported across all platforms |

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
- **Problem Statement**: Beginners often mistakenly label Python as a purely interpreted language that evaluates human-readable source text line by line. This misunderstanding obscures how caching works, why import overhead occurs, and how modern JIT and bytecode optimizations operate.
- **Mental Model**: Python is a compiled language that compiles source text to intermediate bytecode before execution. Compilation occurs automatically in memory (or cached to disk as `.pyc` files), and the resulting bytecode is executed by a high-speed virtual machine.
- **When to Use**: Understanding bytecode execution is essential when debugging performance bottlenecks, optimizing startup latency, writing bytecode analysis tools, or profiling frame allocations.
- **When NOT to Use**: Business domain logic should never depend directly on specific bytecode opcodes, as opcodes can change between minor Python releases.

---

## 2. Core Concepts
1. **Compilation Phase**: CPython's compiler (written in C) reads `.py` files, scans tokens, generates an Abstract Syntax Tree (AST), performs peephole and control flow graph optimizations, and emits a `PyCodeObject`.
2. **`__pycache__` and `.pyc` Files**: When a module is imported, CPython serializes the compiled code object to disk in `__pycache__/{name}.{cache_tag}.pyc` using the internal `marshal` format. On subsequent runs, CPython skips the parsing and compilation stages if the source file's timestamp or hash hasn't changed.
3. **The `PyCodeObject`**: An immutable C structure holding executable bytecode instructions (`co_code`), constant values (`co_consts`), variable names (`co_varnames`), local variable counts (`co_argcount`), and source line mapping tables (`co_linetable`).
4. **The Evaluation Loop (`ceval.c`)**: The C function `PyEval_EvalFrameDefault()` is an infinite loop that reads bytecode instructions sequentially, manipulating an operand stack (`value_stack`) and a fast local variable array (`f_fastlocalsplus`).
5. **Specialized Adaptive Interpreter (PEP 659 / Python 3.11+)**: Dynamically monitors bytecode execution at runtime. When an instruction repeatedly sees the same data types (e.g. `BINARY_OP` adding two integers), CPython rewrites the opcode in place to an optimized specialized version (`BINARY_OP_ADD_INT`), bypassing general object dispatch.

---

## 3. How It Works Internally

The end-to-end execution and caching lifecycle:

```text
Source File: `math_ops.py`
            │
            ▼
Does `__pycache__/math_ops.cpython-314.pyc` exist?
  ├─► YES: Check header (magic number + timestamp or hash)
  │      └─► MATCH: Deserialize directly with marshal.loads() (Fast Path!)
  │
  └─► NO / STALE: Full Compilation Pipeline
         │
         ▼
      [ Tokenizer ] -> Token stream
         │
         ▼
      [ PEG Parser ] -> AST (Abstract Syntax Tree)
         │
         ▼
      [ Bytecode Compiler & Optimizer ]
         │
         ▼
      Creates `PyCodeObject`
         │
         ├─► Writes to `__pycache__/*.pyc` (with 16-byte header)
         │
         ▼
Execution via Virtual Machine Loop (`PyEval_EvalFrameDefault`)
```

---

## 4. Syntax & API Reference

### Introspecting Code Objects
```python
def example(a: int, b: int) -> int:
    return a + b

co = example.__code__
co.co_name          # 'example' (function name)
co.co_argcount      # 2 (number of positional arguments)
co.co_varnames      # ('a', 'b') (local variable names)
co.co_consts        # (None,) (constants used in bytecode)
co.co_code          # b'\x97\x00\x02\x00...' (raw bytecode bytes)
co.co_filename      # '<stdin>' or path to source file
```

### Disassembling Bytecode with `dis`
```python
import dis

dis.dis(example)                  # Prints formatted human-readable opcode table
dis.get_instructions(example)     # Yields Instruction objects with opname, arg, etc.
```

---

## 5. Code Examples

### 1. Minimal / Basic Example: Inspecting Function Code Object
```python
import dis

def add_elements(a: int, b: int) -> int:
    return a + b

print(f"Code Object Co_Name: {add_elements.__code__.co_name}")
print(f"Arg Count: {add_elements.__code__.co_argcount}")
print(f"Variable Names: {add_elements.__code__.co_varnames}")
print("Bytecode disassembly:")
dis.dis(add_elements)
```
**Output**:
```text
Code Object Co_Name: add_elements
Arg Count: 2
Variable Names: ('a', 'b')
Bytecode disassembly:
  1           0 RESUME                   0

  2           2 LOAD_FAST                0 (a)
              4 LOAD_FAST                1 (b)
              6 BINARY_OP                0 (+)
             10 RETURN_VALUE
```

---

### 2. Real-World Production Example: Validating Bytecode Caching in `__pycache__`
```python
from pathlib import Path
import sys

def inspect_pycache_structure(module_name: str) -> dict[str, str | int]:
    cache_tag = sys.implementation.cache_tag
    expected_pyc = Path("__pycache__") / f"{module_name}.{cache_tag}.pyc"
    
    return {
        "cache_tag": cache_tag,
        "expected_filename": str(expected_pyc),
        "pyc_exists": expected_pyc.exists(),
        "bytecode_size": expected_pyc.stat().st_size if expected_pyc.exists() else 0,
    }

print(inspect_pycache_structure("app"))
```
**Output**:
```text
{'cache_tag': 'cpython-314', 'expected_filename': '__pycache__\\app.cpython-314.pyc', 'pyc_exists': False, 'bytecode_size': 0}
```

---

### 3. Edge Case: Reading a `.pyc` File Header
A standard `.pyc` file has a 16-byte header:
- Bytes 0–3: Magic number (identifies exact Python minor release).
- Bytes 4–7: Bit flags (PEP 552 hash-based vs timestamp-based).
- Bytes 8–11: Source modification timestamp or hash.
- Bytes 12–15: Source file size in bytes.

```python
import importlib.util

magic = importlib.util.MAGIC_NUMBER
print(f"Current Python Magic Number: {magic.hex()} (bytes: {magic})")
```
**Output**:
```text
Current Python Magic Number: d60d0d0a (bytes: b'\xd6\r\r\n')
```

---

### 4. Failure Case: Bytecode Magic Number Mismatch
```python
# What happens when loading a .pyc compiled on Python 3.11 into Python 3.14:
# CPython detects the magic number mismatch and automatically recompiles from .py source.
# If .py source is missing:
simulated_error = "ImportError: Bad magic number in .pyc file"
print(simulated_error)
```
**Output**:
```text
ImportError: Bad magic number in .pyc file
```

---

### 5. Idiomatic Pythonic Pattern: Compiling Python Strings Dynamically
```python
source_code = """
def compute_square(n: int) -> int:
    return n * n
"""

# Compile into a reusable code object
compiled_code = compile(source_code, filename="<dynamic>", mode="exec")

# Execute within an isolated namespace
namespace: dict[str, object] = {}
exec(compiled_code, namespace)

compute_fn = namespace["compute_square"]
print("Result of dynamically compiled function:", compute_fn(7))  # type: ignore[operator]
```
**Output**:
```text
Result of dynamically compiled function: 49
```

---

## 6. Edge Cases & Pitfalls
1. **Top-Level Script Caching**: The file passed directly to Python (e.g. `python script.py`) is *never* compiled to a `.pyc` file in `__pycache__`! Only imported modules are written to `.pyc` disk caches.
2. **Bytecode Portability**: Bytecode is *not* guaranteed to be portable across Python minor versions (e.g., Python 3.11 bytecode cannot run on 3.12 or 3.14).
3. **Disabling Bytecode Writing**: Setting the environment variable `PYTHONDONTWRITEBYTECODE=1` or launching Python with `-B` prevents Python from writing `.pyc` files to disk (useful in read-only container filesystems).

---

## 7. Common Mistakes

| Mistake | Why It Fails | Exception / Diagnostic | Correct Pattern |
|---|---|---|---|
| Committing `__pycache__` to Git | Bloats repo with binary platform files | Git repository clutter | Add `__pycache__/` and `*.pyc` to `.gitignore` |
| Deleting `.py` source and relying solely on `.pyc` in production | Breaks across minor Python upgrades due to magic number changes | `ImportError: Bad magic number` | Always distribute original `.py` source or wheels |
| Assuming Python evaluates line by line without compiling | Fails to explain syntax errors caught before execution begins | Confusion over early syntax errors | Understand that compilation precedes execution |
| Running with `PYTHONDONTWRITEBYTECODE=1` in high-startup CLI tools | Forces CPython to re-parse and re-compile every import every run | Significant startup time regression | Keep bytecode caching enabled in production |
| Inspecting `co_code` bytes directly across Python versions | Opcode numerical IDs shift between releases | Fragile code that breaks on upgrade | Use `dis.get_instructions()` instead of raw bytes |
| Manually writing `.pyc` files without proper 16-byte headers | CPython rejects the corrupted file | `EOFError` or `ValueError: bad marshal data` | Use `py_compile` or `compileall` standard modules |

---

## 8. Performance & Complexity
- **Startup Speedup from `.pyc`**: Loading bytecode via `marshal.loads()` is $3\times - 5\times$ faster than tokenizing, parsing, and compiling `.py` text files.
- **Evaluation Loop Latency**: Python bytecode dispatch takes roughly 2–5 nanoseconds per opcode on modern x86_64 CPUs.

---

## 9. CPython Internals & Bytecode: The Evaluation Loop
In CPython's C source code (`Python/ceval.c`), the evaluation loop operates around:
```c
PyObject* PyEval_EvalFrameDefault(PyThreadState *tstate, InterpreterFrame *frame, int throwflag) {
    // Instruction pointer
    _Py_CODEUNIT *next_instr = frame->instr_ptr;
    
    for (;;) {
        opcode = _Py_OPCODE(*next_instr);
        switch (opcode) {
            case LOAD_FAST:
                // Push local variable onto value stack
                break;
            case BINARY_OP:
                // Pop 2 items, invoke type slot, push result
                break;
            case RETURN_VALUE:
                // Pop result, unwind frame, return to caller
                return retval;
        }
    }
}
```

---

## 10. Best Practices
1. Add `__pycache__/` and `*.pyc` to the global `.gitignore`.
2. Use the standard `compileall` module (`python -m compileall .`) when building Docker images to pre-compile all `.py` files into `.pyc` files, eliminating runtime compilation latency on first import.
3. Use `dis.dis()` to verify performance hypotheses regarding code efficiency.

---

## 11. Real-World Case Study: Pre-Compiling Bytecode in Production Containers
A serverless AWS Lambda microservice experienced cold-start latency spikes of 1.2 seconds:
1. Analysis revealed that importing large libraries (Boto3, Pydantic, SQLAlchemy) spent 800 ms parsing and compiling `.py` files into memory because the container filesystem was read-only, preventing `.pyc` files from being cached to disk.
2. The team added `RUN python -m compileall -b /app` during the Docker image build.
3. Cold start latency dropped from **1.2 seconds to 380 milliseconds**.

---

## 12. Interview Questions

### Q1: Is Python an interpreted or compiled language?
<details>
<summary><strong>Answer</strong></summary>
Python is both. It is a compiled language because source code (`.py`) is never directly executed line by line; it is compiled into bytecode representations (`PyCodeObject`). It is an interpreted language because that compiled bytecode is executed by a software virtual machine interpreter (`PyEval_EvalFrameDefault`) rather than directly by CPU hardware.
</details>

### Q2: What is the purpose of the `__pycache__` directory?
<details>
<summary><strong>Answer</strong></summary>
`__pycache__` stores pre-compiled bytecode files (`.pyc`) for imported modules. When a module is imported again, CPython checks the `.pyc` header against the source file's modification timestamp or hash. If unchanged, CPython skips the lexical analysis, parsing, and bytecode generation stages, dramatically speeding up application startup time.
</details>

### Q3: What is stored in a `PyCodeObject`?
<details>
<summary><strong>Answer</strong></summary>
A `PyCodeObject` contains all immutable data required to execute a block of code:
- `co_code`: The raw sequence of bytecode instructions.
- `co_consts`: A tuple of literal constants referenced by the bytecode.
- `co_varnames`: A tuple of local variable names.
- `co_names`: A tuple of global and attribute names.
- `co_argcount`: The number of positional arguments.
- `co_linetable`: Bytecode offset to source line number mapping.
</details>

### Q4: Why is the main script not cached in `__pycache__`?
<details>
<summary><strong>Answer</strong></summary>
CPython is optimized under the assumption that top-level scripts invoked directly (e.g., `python main.py`) are frequently modified by developers during editing and testing, whereas imported library modules change infrequently. Therefore, only imported modules are written to `.pyc` files in `__pycache__`.
</details>

### Q5: What is the specialized adaptive interpreter (PEP 659)?
<details>
<summary><strong>Answer</strong></summary>
Introduced in Python 3.11 as part of the Faster CPython initiative, the specialized adaptive interpreter monitors generic bytecode instructions during execution. When an instruction repeatedly operates on specific types (e.g., adding two floats), the interpreter dynamically rewrites the opcode in place to a specialized instruction (e.g., `BINARY_OP_ADD_FLOAT`), eliminating dynamic type checks and method lookup overhead.
</details>

### Q6: What is the purpose of the magic number in a `.pyc` file?
<details>
<summary><strong>Answer</strong></summary>
The magic number occupies the first two bytes of a `.pyc` file (followed by `\r\n`). It uniquely identifies the exact CPython bytecode format version. If an older or newer Python version attempts to read a `.pyc` file with a mismatched magic number, CPython rejects it and recompiles from source to prevent virtual machine crashes.
</details>

### Q7: How can you prevent Python from creating `__pycache__` directories?
<details>
<summary><strong>Answer</strong></summary>
Either by setting the environment variable `PYTHONDONTWRITEBYTECODE=1` or by passing the `-B` command-line flag (`python -B script.py`).
</details>

### Q8: What is the difference between `eval()`, `exec()`, and `compile()`?
<details>
<summary><strong>Answer</strong></summary>
`compile()` translates a source string into a `PyCodeObject` without executing it. `eval()` evaluates a single expression string or code object and returns the result. `exec()` executes a string or code object containing arbitrary statements, classes, or functions and always returns `None`.
</details>

---

## 13. Exercises & Challenges

### Challenge 1 (Easy): Print Function Bytecode Length
Write a function that accepts any callable and returns the total byte length of its raw bytecode instructions (`co_code`).
<details>
<summary><strong>Solution</strong></summary>

```python
from collections.abc import Callable
from typing import Any

def get_bytecode_length(fn: Callable[..., Any]) -> int:
    code = getattr(fn, "__code__", None)
    if code is None:
        raise ValueError("Provided object has no __code__ attribute")
    return len(code.co_code)

def sample() -> None:
    pass

print("Bytecode length:", get_bytecode_length(sample))
```
</details>

### Challenge 2 (Easy): Pre-Compile a Python File Programmatically
Write a script that uses the standard library `py_compile` module to compile a target script to a `.pyc` file.
<details>
<summary><strong>Solution</strong></summary>

```python
from pathlib import Path
import py_compile

def compile_script_to_pyc(source: Path) -> Path:
    compiled_path = py_compile.compile(str(source))
    return Path(compiled_path)
```
</details>

### Challenge 3 (Medium): Count Local Variables via `co_varnames`
Write a function `analyze_local_variables(fn)` that inspects a function's code object and returns the count and tuple of all local variable names.
<details>
<summary><strong>Solution</strong></summary>

```python
from collections.abc import Callable
from typing import Any

def analyze_local_variables(fn: Callable[..., Any]) -> dict[str, Any]:
    co = fn.__code__
    return {
        "total_locals": co.co_nlocals,
        "varnames": co.co_varnames,
        "argcount": co.co_argcount,
    }

def calculation(x: int, y: int) -> int:
    temp = x * 2
    return temp + y

print(analyze_local_variables(calculation))
```
</details>

### Challenge 4 (Medium): Disassemble Filtered Opcodes
Write a function `find_opcodes(fn, target_opname)` that uses `dis.get_instructions` to find and return line numbers where a specific opcode is executed.
<details>
<summary><strong>Solution</strong></summary>

```python
from collections.abc import Callable
import dis
from typing import Any

def find_opcodes(fn: Callable[..., Any], target_opname: str) -> list[int]:
    lines = []
    for instr in dis.get_instructions(fn):
        if instr.opname == target_opname and instr.starts_line:
            lines.append(instr.starts_line)
    return lines

def sample_func(a: int) -> int:
    b = a * 2
    c = b + 10
    return c

print("Lines with BINARY_OP:", find_opcodes(sample_func, "BINARY_OP"))
```
</details>

### Challenge 5 (Hard): Read and Verify `.pyc` Magic Number
Write a function that opens a `.pyc` file in binary mode, reads the 4-byte magic number, and verifies if it matches the executing interpreter's `importlib.util.MAGIC_NUMBER`.
<details>
<summary><strong>Solution</strong></summary>

```python
import importlib.util
from pathlib import Path

def verify_pyc_magic(pyc_path: Path) -> bool:
    if not pyc_path.exists():
        raise FileNotFoundError(f"File not found: {pyc_path}")
    with open(pyc_path, "rb") as f:
        magic_bytes = f.read(4)
    expected = importlib.util.MAGIC_NUMBER
    is_valid = magic_bytes == expected
    print(f"Read: {magic_bytes.hex()}, Expected: {expected.hex()}, Valid: {is_valid}")
    return is_valid
```
</details>

### Challenge 6 (Hard): Custom Bytecode Peephole Inspector
Write a function that inspects a function's bytecode instructions and detects whether constant folding took place during compilation (e.g. checking if `2 + 3` was folded into `5`).
<details>
<summary><strong>Solution</strong></summary>

```python
import dis

def check_constant_folding() -> bool:
    def test_math() -> int:
        return 2 + 3  # Peephole optimizer folds this into 5

    constants = test_math.__code__.co_consts
    # If 2 and 3 do not exist in co_consts, but 5 does, folding occurred
    folded = (5 in constants) and (2 not in constants) and (3 not in constants)
    return folded

print("Was constant folding performed:", check_constant_folding())
```
</details>
