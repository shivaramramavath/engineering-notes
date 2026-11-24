# Module 01: Standard Input & Output (07-input-and-output)

> **Topic Overview**: Standard I/O streams in Python: `print()` internals, `input()` and prompt handling, `sys.stdin`, `sys.stdout`, `sys.stderr`, stream redirection, line buffering, terminal ANSI escape color codes, and high-performance I/O pipelines.

---

## Metadata Table

| Field | Value |
|---|---|
| **Category** | Python Fundamentals & Syntax |
| **Target Level** | Beginner to Intermediate |
| **Python Version** | Python 3.10+ (tested on CPython 3.14.0) |
| **Tested With** | CPython 3.14.0, sys, io, Ruff 0.17.6 |
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
- **Problem Statement**: Command-line interfaces, background daemons, and microservices communicate with operators, parent processes, and container log aggregators via standard I/O streams. Mixing diagnostic errors with data outputs or failing to flush buffers causes log delays and corrupts CLI pipelines.
- **Mental Model**: CPython exposes three standard POSIX I/O streams via the `sys` module: `sys.stdin` (File descriptor 0), `sys.stdout` (File descriptor 1), and `sys.stderr` (File descriptor 2). The built-in `print()` and `input()` functions are high-level abstractions over these underlying buffered text streams.
- **When to Use**: Interactive CLI tools, terminal utilities, script logging, and piping structured data between Unix processes (`stdout` to `stdin`).
- **When NOT to Use**: High-throughput distributed tracing or structured microservice logging (use the `logging` standard library or `structlog`).

---

## 2. Core Concepts
1. **The Three Standard Streams**:
   - `sys.stdin`: Input text stream read by `input()` or `sys.stdin.read()`.
   - `sys.stdout`: Buffered output stream written by `print(..., file=sys.stdout)`.
   - `sys.stderr`: Unbuffered (or line-buffered) stream intended exclusively for diagnostics, warnings, and error messages.
2. **`print()` Parameters**:
   - `sep`: String inserted between objects (defaults to `" "`).
   - `end`: String appended after the last object (defaults to `"\n"`).
   - `file`: Target stream object implementing `.write(str)` (defaults to `sys.stdout`).
   - `flush`: Boolean flag. If `True`, forcibly flushes the underlying OS stream buffer immediately.
3. **Buffering Semantics**:
   - Interactive TTY: `sys.stdout` is line-buffered (flushes on newline `\n`).
   - Non-interactive (File / Pipe): `sys.stdout` is block-buffered (typically 4 KB or 8 KB chunks).
   - `sys.stderr` is always unbuffered or line-buffered to guarantee immediate error visibility.
4. **Environment Variables**:
   - `PYTHONUNBUFFERED=1` or `python -u`: Forces both `stdout` and `stderr` to be unbuffered, critical for Docker container logging.

---

## 3. How It Works Internally

CPython's I/O layering architecture:

```text
Python Code: `print("Hello", flush=True)`
            │
            ▼
    [ Built-in print() in Python/bltinmodule.c ]
            │
            ▼
Calls `sys.stdout.write("Hello\n")`
            │
            ▼
[ io.TextIOWrapper Layer ] -> Encodes Unicode characters to UTF-8 bytes
            │
            ▼
[ io.BufferedWriter Layer ] -> Accumulates bytes into in-memory buffer
            │
            ▼ (Flush triggered: '\n' on TTY or flush=True)
[ io.FileIO Layer (OS File Descriptor 1) ]
            │
            ▼
POSIX `write(1, buf, len)` / Windows `WriteFile` System Call
```

---

## 4. Syntax & API Reference

### `print()` Signature
```python
print(*objects, sep=" ", end="\n", file=sys.stdout, flush=False)
```

### `input()` Signature
```python
user_str: str = input(prompt="")
# Reads one line from sys.stdin, strips trailing newline '\n', returns str.
# Raises EOFError if stream ends unexpectedly (e.g. Ctrl+D / Ctrl+Z).
```

### Low-Level Stream Access
```python
import sys

sys.stdout.write("Direct write without newline")
sys.stdout.flush()

for line in sys.stdin:
    process(line.rstrip("\n"))
```

---

## 5. Code Examples

### 1. Minimal / Basic Example: Customizing `sep`, `end`, and Buffer Redirection
```python
import io

# Redirect output to an in-memory string buffer
buffer = io.StringIO()
print("Header", "Row", sep=" | ", end="\n---\n", file=buffer)

# Extract captured output via getvalue()
print(buffer.getvalue().strip())
```
**Output**:
```text
Header | Row
---
```

---

### 2. Real-World Production Example: Routing Logs to `sys.stderr` and Data to `sys.stdout`
```python
import json
import sys

def process_records(records: list[dict[str, object]]) -> None:
    """Emits machine-readable JSON to stdout and diagnostics to stderr."""
    sys.stderr.write(f"[INFO] Processing {len(records)} incoming records...\n")
    
    valid_records = []
    for r in records:
        if r.get("is_active"):
            valid_records.append(r)
        else:
            sys.stderr.write(f"[WARN] Skipping inactive record ID: {r.get('id')}\n")
            
    # Output only clean data to stdout for Unix pipe piping
    json.dump(valid_records, sys.stdout, indent=2)
    sys.stdout.write("\n")
    sys.stderr.write("[INFO] Processing complete.\n")

sample_data = [
    {"id": 101, "is_active": True, "name": "Node-Alpha"},
    {"id": 102, "is_active": False, "name": "Node-Beta"},
]

process_records(sample_data)
```
**Output**:
```text
[INFO] Processing 2 incoming records...
[WARN] Skipping inactive record ID: 102
[
  {
    "id": 101,
    "is_active": true,
    "name": "Node-Alpha"
  }
]
[INFO] Processing complete.
```

---

### 3. Edge Case: Simulating Interactive Input via `sys.stdin` Mocking
```python
import io
import sys

def ask_user_age() -> int:
    """Prompts user and parses integer age with validation."""
    raw = input("Enter age: ")
    try:
        age = int(raw.strip())
        if age < 0:
            raise ValueError("Age cannot be negative")
        return age
    except ValueError as exc:
        raise ValueError(f"Invalid age input: '{raw}'") from exc

# Mock sys.stdin with in-memory buffer
original_stdin = sys.stdin
sys.stdin = io.StringIO("25\n")
try:
    user_age = ask_user_age()
    print("Parsed age from mocked input:", user_age)
finally:
    sys.stdin = original_stdin
```
**Output**:
```text
Parsed age from mocked input: 25
```

---

### 4. Failure Case: Unhandled `EOFError` on Premature Stream Termination
```python
import io
import sys

# Simulating empty stdin (EOF immediately)
sys.stdin = io.StringIO("")

try:
    input("Waiting for data: ")
except EOFError as exc:
    print(f"Caught expected error: {type(exc).__name__}: Stream reached EOF without data.")
finally:
    sys.stdin = sys.__stdin__
```
**Output**:
```text
Caught expected error: EOFError: Stream reached EOF without data.
```

---

### 5. Idiomatic Pythonic Pattern: Animated Console Progress Bar with `flush=True`
```python
import sys
import time

def render_progress_bar(total_steps: int) -> None:
    for i in range(1, total_steps + 1):
        percent = int((i / total_steps) * 100)
        bar = "█" * (i * 2) + "-" * ((total_steps - i) * 2)
        # Carriage return '\r' rewinds cursor to start of line
        sys.stdout.write(f"\rProgress: [{bar}] {percent}%")
        sys.stdout.flush()
    sys.stdout.write("\n")

render_progress_bar(5)
```
**Output**:
```text
Progress: [██████████] 100%
```

---

## 6. Edge Cases & Pitfalls
1. **Docker Container Log Buffering**: In Docker containers where `stdout` is redirected to a non-TTY pipe, Python defaults to block-buffering. Logs may appear delayed by minutes. Always set `ENV PYTHONUNBUFFERED=1` in your Dockerfile.
2. **Carriage Return `\r` Across Terminals**: Overwriting console output using `\r` works in standard terminals, but produces garbage characters or concatenated lines in non-interactive CI log runners.
3. **Encoding Errors in Redirected Output (`UnicodeEncodeError`)**: When outputting emojis or non-ASCII characters on older Windows terminals (CP1252 or OEM code pages), `print()` crashes with `UnicodeEncodeError`. Set `PYTHONIOENCODING=utf-8` to enforce UTF-8 across all streams.

---

## 7. Common Mistakes

| Mistake | Why It Fails | Exception / Diagnostic | Correct Pattern |
|---|---|---|---|
| Routing errors to `sys.stdout` | Corrupts downstream Unix pipes expecting clean data | Pipeline parser failures | Use `print(..., file=sys.stderr)` |
| Omitting `PYTHONUNBUFFERED=1` in Docker | Logs are delayed inside block buffer until process exits | Missing realtime logs in Kubernetes | Set `ENV PYTHONUNBUFFERED=1` |
| Calling `input()` in automated background daemons | Blocks waiting on non-interactive closed stdin | `EOFError: EOF when reading a line` | Pass CLI flags or read config files |
| Forgetting `flush=True` on long-running CLI loops | Output remains trapped in memory buffer | User assumes script is frozen | Pass `flush=True` to `print()` |
| Hardcoding Windows backslashes in console output | Escape sequence conflicts | Syntax warnings or mangled text | Use raw strings `r"..."` |
| Using string concatenation inside `print()` (`print("A" + "B")`) | Creates temporary string allocation | Slower execution | Use multiple arguments: `print("A", "B")` |

---

## 8. Performance & Complexity
- **`print()` Latency**: Writing to an interactive TTY takes ~5–20 microseconds per call due to OS terminal rendering.
- **Direct `sys.stdout.write()`**: Significantly faster ($2\times - 4\times$) than `print()` because it avoids parameter unpacking, formatting checks, and default space/newline appends.

---

## 9. CPython Internals & Bytecode: The `print()` Builtin
In CPython C code (`Python/bltinmodule.c`):
- `builtin_print()` parses the variable arguments `args` and keyword arguments (`sep`, `end`, `file`, `flush`).
- If `file == None`, it dynamically fetches `sys.stdout` via `PySys_GetObject("stdout")`.
- It iterates through each object, calls `PyObject_Str()`, invokes `file.write()`, and writes `sep` between items.
- If `flush == True`, it calls `file.flush()`.

---

## 10. Best Practices
1. Separate data output (`stdout`) from diagnostic logging (`stderr`).
2. Set `PYTHONUNBUFFERED=1` in all containerized Docker deployments.
3. Handle `EOFError` and `KeyboardInterrupt` gracefully in interactive CLI applications.
4. Pass `flush=True` when rendering dynamic terminal progress indicators.

---

## 11. Real-World Case Study: Unix Filter CLI Utility Architecture
A DevOps team developed a CLI log auditing tool `audit-filter`:
1. The tool reads gigabytes of server logs from standard input: `cat server.log | audit-filter | gzip > clean.gz`.
2. Clean, transformed log lines are written to `sys.stdout`.
3. Ingestion statistics, progress bars, and corrupted record warnings are routed exclusively to `sys.stderr`.
4. Downstream `gzip` processes only valid data, while operators view real-time progress in the terminal without stream contamination.

---

## 12. Interview Questions

### Q1: What is the difference between `sys.stdout` and `sys.stderr`?
<details>
<summary><strong>Answer</strong></summary>
`sys.stdout` is the standard output stream used for normal program data and results; it is typically line-buffered when connected to a terminal and block-buffered when redirected to a file or pipe. `sys.stderr` is the standard error stream reserved for warnings, diagnostic logs, and error messages; it is unbuffered or line-buffered by default to ensure error messages appear immediately even if the program terminates abnormally.
</details>

### Q2: Why is `PYTHONUNBUFFERED=1` critical in Docker containerized applications?
<details>
<summary><strong>Answer</strong></summary>
When Python detects that `sys.stdout` is connected to a pipe or file (as is standard under Docker container logging drivers), it switches from line buffering to block buffering (typically 4 KB or 8 KB). If an application logs infrequently, messages remain trapped in the buffer and are not visible in `docker logs` or Kubernetes log aggregators in real time. Setting `PYTHONUNBUFFERED=1` forces Python to flush stdout and stderr immediately after every write.
</details>

### Q3: What happens when `input()` encounters an End-of-File (EOF) marker?
<details>
<summary><strong>Answer</strong></summary>
When `sys.stdin` reaches EOF without reading any characters (for example, when a piped input terminates, or a user presses `Ctrl+D` on POSIX or `Ctrl+Z` on Windows), `input()` raises an `EOFError`.
</details>

### Q4: How does `print()` differ from `sys.stdout.write()`?
<details>
<summary><strong>Answer</strong></summary>
`print()` is a high-level wrapper that converts all passed objects to strings via `str()`, joins them with the `sep` string, appends the `end` character (default newline `\n`), routes output to the specified `file`, and optionally flushes. `sys.stdout.write()` is a lower-level method that accepts only a single string, does not append newlines automatically, and executes with lower overhead.
</details>

### Q5: What does the `flush` parameter in `print()` do?
<details>
<summary><strong>Answer</strong></summary>
The `flush=True` parameter forces the underlying stream's internal buffer to be written immediately to the operating system's file descriptor, bypassing standard line or block buffering delays.
</details>

### Q6: How can you capture `print()` output inside a unit test without modifying the function?
<details>
<summary><strong>Answer</strong></summary>
By using `contextlib.redirect_stdout`:
```python
import contextlib
import io

buffer = io.StringIO()
with contextlib.redirect_stdout(buffer):
    my_function()
captured = buffer.getvalue()
```
</details>

### Q7: What is `sys.__stdout__` and how does it relate to `sys.stdout`?
<details>
<summary><strong>Answer</strong></summary>
`sys.stdout` is the current active standard output stream, which can be reassigned or monkeypatched by user code. `sys.__stdout__` stores the original, untouched standard output stream established at CPython startup, allowing programs to restore the original stream if `sys.stdout` was redirected.
</details>

### Q8: What is `PYTHONIOENCODING`?
<details>
<summary><strong>Answer</strong></summary>
`PYTHONIOENCODING` is an environment variable that overrides the encoding used for standard streams (`stdin`, `stdout`, `stderr`). For example, setting `PYTHONIOENCODING=utf-8:surrogateescape` guarantees that standard streams use UTF-8 regardless of the underlying operating system terminal locale.
</details>

---

## 13. Exercises & Challenges

### Challenge 1 (Easy): Print CSV Header and Rows
Write a function `print_csv_row(*values)` that prints values separated by commas without surrounding brackets or quotes.
<details>
<summary><strong>Solution</strong></summary>

```python
from typing import Any

def print_csv_row(*values: Any) -> None:
    print(*values, sep=",")

print_csv_row("id", "timestamp", "status_code", "ip_address")
print_csv_row(101, "2026-11-24T00:00:00Z", 200, "192.168.1.1")
```
</details>

### Challenge 2 (Easy): Safe Input Helper with Default
Write a function `prompt_with_default(prompt_text, default)` that prompts the user and returns the entered string, or `default` if the user simply hits Enter.
<details>
<summary><strong>Solution</strong></summary>

```python
def prompt_with_default(prompt_text: str, default: str) -> str:
    user_val = input(f"{prompt_text} [{default}]: ").strip()
    return user_val if user_val else default
```
</details>

### Challenge 3 (Medium): Output Redirection Context Manager
Implement a custom context manager `capture_stdout()` that intercepts all `sys.stdout` writes during its block and yields an `io.StringIO` buffer.
<details>
<summary><strong>Solution</strong></summary>

```python
from collections.abc import Generator
import contextlib
import io
import sys

@contextlib.contextmanager
def capture_stdout() -> Generator[io.StringIO, None, None]:
    new_out = io.StringIO()
    old_out = sys.stdout
    sys.stdout = new_out
    try:
        yield new_out
    finally:
        sys.stdout = old_out

with capture_stdout() as buf:
    print("Testing output interception.")

print("Captured text:", buf.getvalue().strip())
```
</details>

### Challenge 4 (Medium): ANSI Color Terminal Formatter
Write a utility class `TerminalColor` with methods `red(text)`, `green(text)`, and `blue(text)` that wrap strings in ANSI escape codes, resetting the style at the end.
<details>
<summary><strong>Solution</strong></summary>

```python
class TerminalColor:
    RESET = "\033[0m"
    RED = "\033[31m"
    GREEN = "\033[32m"
    BLUE = "\033[34m"

    @classmethod
    def red(cls, text: str) -> str:
        return f"{cls.RED}{text}{cls.RESET}"

    @classmethod
    def green(cls, text: str) -> str:
        return f"{cls.GREEN}{text}{cls.RESET}"

    @classmethod
    def blue(cls, text: str) -> str:
        return f"{cls.BLUE}{text}{cls.RESET}"

print(TerminalColor.green("SUCCESS: Service is healthy"))
```
</details>

### Challenge 5 (Hard): High-Speed Line Processor over `sys.stdin`
Write a memory-efficient generator function `stream_stdin_lines()` that streams lines from standard input in chunks, stripping trailing newlines and ignoring empty lines.
<details>
<summary><strong>Solution</strong></summary>

```python
from collections.abc import Generator
import io
import sys

def stream_stdin_lines(stream: io.TextIOBase = sys.stdin) -> Generator[str, None, None]:
    for line in stream:
        cleaned = line.rstrip("\r\n")
        if cleaned:
            yield cleaned

# Testing with simulated stream
mock_stream = io.StringIO("Line 1\n\nLine 2\nLine 3\n")
for line in stream_stdin_lines(mock_stream):
    print("Yielded:", line)
```
</details>

### Challenge 6 (Hard): Tee Stream Duplicator
Implement a `Tee` class that conforms to Python's text stream interface (`write`, `flush`) and duplicates all writes to two target streams simultaneously (e.g. stdout and a log file).
<details>
<summary><strong>Solution</strong></summary>

```python
import io
import sys

class Tee(io.TextIOBase):
    def __init__(self, stream_a: io.TextIOBase, stream_b: io.TextIOBase) -> None:
        self.stream_a = stream_a
        self.stream_b = stream_b

    def write(self, s: str) -> int:
        self.stream_a.write(s)
        self.stream_b.write(s)
        return len(s)

    def flush(self) -> None:
        self.stream_a.flush()
        self.stream_b.flush()

buf1 = io.StringIO()
buf2 = io.StringIO()
tee = Tee(buf1, buf2)
print("Dual write test", file=tee)
print("Buffer 1:", buf1.getvalue().strip())
print("Buffer 2:", buf2.getvalue().strip())
```
</details>
