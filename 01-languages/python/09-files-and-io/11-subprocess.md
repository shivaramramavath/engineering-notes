---
title: "Subprocess and Process Management in Python"
description: "Mastering the standard library subprocess module: subprocess.run, Popen streaming pipelines, deadlocks, shell=True injection vulnerabilities, timeout management, signal handling, and cross-platform process lifecycle controls."
module: "09-files-and-io"
order: 11
tags:
  - python
  - subprocess
  - popen
  - process-management
  - security
  - shell-injection
  - pipes
difficulty: "Intermediate"
cpython_version: "3.12+"
last_verified: "2026-03"
---

# Subprocess and Process Management in Python

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
10. [Real-World Case Study: Resilient External Tool Pipeline with Streaming Telemetry](#real-world-case-study-resilient-external-tool-pipeline-with-streaming-telemetry)
11. [Interview Questions & Deep-Dive Scenarios](#interview-questions--deep-dive-scenarios)
12. [Practical Exercises & Code Katas](#practical-exercises--code-katas)

---

## What & Why

Python provides the `subprocess` module to spawn new child processes, connect to their standard input/output/error pipes, and obtain their return codes. It replaces legacy, unsafe functions like `os.system()`, `os.popen()`, and the `commands` module.

### Motivation
No programming language operates in total isolation. Production Python applications frequently need to invoke external command-line binaries, run shell scripts, orchestrate system tools (e.g. `git`, `ffmpeg`, `docker`, `openssl`), or execute isolated worker processes. Doing so safely requires managing delicate operating system primitives: preventing command injection attacks, streaming large data payloads through UNIX pipes without deadlocking, enforcing hard execution timeouts, and handling cross-platform process signals.

### Real-World Analogy
Consider an executive hiring an external specialized contractor to perform an audit:
- The executive specifies the task and hands the contractor an exact briefing dossier (`args`).
- The contractor works in an isolated room (`child process`).
- Communication occurs strictly through a two-way intercom (`stdin`, `stdout`, `stderr` pipes).
- If the contractor takes longer than agreed upon, the executive cancels the contract (`timeout`).
- If the executive simply opens the window and shouts the task into a crowded street (`shell=True`), a malicious bystander could intercept the instructions and shout back harmful commands (`command injection`).

### When to Use vs When Not to Use
- **Use `subprocess` for:**
  - Invoking external CLI tools (`git`, `tar`, `curl`, `kubectl`, `ffmpeg`).
  - Running isolated, un-trusted code sandboxes in a separate process.
  - Streaming binary pipelines between two standalone executables.
- **Do Not Use `subprocess` for:**
  - Parallelizing CPU-bound Python functions (use `multiprocessing` or `concurrent.futures.ProcessPoolExecutor`).
  - Asynchronous I/O across thousands of network connections (use `asyncio`).
  - Tasks that can be achieved natively via standard library modules (e.g. copying files via `shutil` instead of spawning `cp`).

---

## Core Concepts & Mental Model

```
+-------------------------------------------------------------------------+
| Parent Python Process                                                   |
|                                                                         |
|  subprocess.run(['git', 'status'])                                      |
|            |                                                            |
|            v                                                            |
|  +--------------------+                                                 |
|  | subprocess.Popen   |                                                 |
|  +--------------------+                                                 |
|       |           ^           ^                                         |
|       | stdin     | stdout    | stderr                                  |
+-------|-----------|-----------|-----------------------------------------+
        | Pipe      | Pipe      | Pipe
        v           |           |
+-------------------------------------------------------------------------+
| Child Process (e.g. /usr/bin/git)                                       |
| Standard Streams: [0: stdin] [1: stdout] [2: stderr]                     |
| Return Code: 0 (Success) or non-zero (Error)                            |
+-------------------------------------------------------------------------+
```

### Key Abstractions
1. **`subprocess.run()`**: The recommended high-level blocking API introduced in Python 3.5. It spawns the command, waits for it to complete, and returns a `CompletedProcess` instance.
2. **`subprocess.Popen()`**: The underlying low-level process controller. It starts the child process asynchronously and returns immediately, allowing streaming, bi-directional pipe communication, and background execution.
3. **`CompletedProcess`**: Represents the finished process, containing `.returncode`, `.stdout`, `.stderr`, and `.args`.
4. **`subprocess.CalledProcessError`**: Raised when `check=True` and the child process exits with a non-zero exit code.

---

## How It Works Internally

### 1. POSIX Process Creation (`fork` + `execve` vs `posix_spawn`)
On Linux and macOS:
- Traditionally, Python called `fork()` to duplicate the parent process, set up pipe file descriptors via `dup2()`, and called `execve()` to replace the child image with the target binary.
- In modern CPython (3.8+), when `close_fds=True` (default) and standard options are used, CPython utilizes `posix_spawn()`. This avoids duplicating the parent's memory page tables, resulting in significantly faster process instantiation and lower peak memory overhead on large processes.

### 2. Windows Process Creation (`CreateProcessW`)
On Windows:
- Python invokes the Win32 API `CreateProcessW()`.
- Arguments passed as a list are converted into a single formatted command line string according to Microsoft's command-line parsing rules (`list2cmdline()`), quoting arguments containing spaces or escape characters.

### 3. The Pipe Deadlock Trap
Operating system pipes have fixed buffer sizes (typically 64 KB on Linux, 4 KB on Windows).
If a child process outputs more data than the pipe buffer can hold, the child process **blocks** waiting for the parent to read data. If the parent is waiting for the child to finish (e.g. via `p.wait()`) instead of reading from `stdout`, both processes wait on each other indefinitely: a classic **deadlock**!
`subprocess.run()` and `p.communicate()` prevent this by reading from `stdout` and `stderr` concurrently using background threads or `select()`/`poll()`.

---

## Syntax & API Reference

### High-Level API: `subprocess.run`
```python
import subprocess

result: subprocess.CompletedProcess = subprocess.run(
    args: list[str] | str,             # Command and arguments (prefer list)
    stdin=None,                        # None, PIPE, DEVNULL, or file-like object
    input=None,                        # bytes or str passed to child stdin
    stdout=None,                       # None, PIPE, DEVNULL, or file-like object
    stderr=None,                       # None, PIPE, DEVNULL, or STDOUT
    capture_output=False,              # If True, sets stdout=PIPE and stderr=PIPE
    shell=False,                       # If True, executes through OS shell (DANGEROUS)
    cwd=None,                          # Working directory for child
    timeout=None,                      # Seconds before TimeoutExpired is raised
    check=False,                       # If True, raises CalledProcessError on non-zero exit
    encoding=None,                     # Text encoding (e.g. 'utf-8')
    errors=None,                       # Decoding error handling
    text=None,                         # Alias for universal_newlines; if True, strings instead of bytes
    env=None                           # Child environment dictionary
)
```

### Low-Level API: `subprocess.Popen`
```python
p = subprocess.Popen(
    args,
    bufsize=-1,
    stdin=subprocess.PIPE,
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE,
    text=True
)

stdout_data, stderr_data = p.communicate(input=None, timeout=None)
return_code: int = p.poll()            # Returns None if running, or exit code
p.wait(timeout=None)                   # Blocks until exit
p.terminate()                          # SIGTERM (POSIX) / TerminateProcess (Win)
p.kill()                               # SIGKILL (POSIX) / TerminateProcess (Win)
```

---

## Code Examples

### Example 1: Basic Safe Execution with `check=True` and Output Capture
```python
import subprocess
import sys

def run_basic_command():
    # Execute python binary itself to query its version
    cmd = [sys.executable, "--version"]
    
    # capture_output=True captures stdout/stderr; text=True decodes bytes to str
    result = subprocess.run(
        cmd,
        capture_output=True,
        text=True,
        check=True
    )
    
    print("Return Code:", result.returncode)
    print("Stdout:", result.stdout.strip())
    print("Args executed:", result.args)

run_basic_command()
# Output:
# Return Code: 0
# Stdout: Python 3.12...
# Args executed: ['.../python', '--version']
```

### Example 2: Handling Execution Errors with `CalledProcessError`
```python
import subprocess
import sys

def run_failing_command():
    # Run a script that deliberately exits with code 42
    code = "import sys; sys.stderr.write('Fatal crash occurred!\\n'); sys.exit(42)"
    cmd = [sys.executable, "-c", code]

    try:
        subprocess.run(cmd, capture_output=True, text=True, check=True)
    except subprocess.CalledProcessError as err:
        print(f"Process failed with exit code: {err.returncode}")
        print(f"Captured Stderr: {err.stderr.strip()}")
        print(f"Command executed: {err.cmd}")

run_failing_command()
# Output:
# Process failed with exit code: 42
# Captured Stderr: Fatal crash occurred!
# Command executed: ['.../python', '-c', '...']
```

### Example 3: Timeout Enforcement and Process Cleanup
```python
import subprocess
import sys
import time

def run_with_timeout():
    # Spawns a child process that sleeps for 5 seconds
    code = "import time; time.sleep(5)"
    cmd = [sys.executable, "-c", code]

    start = time.perf_counter()
    try:
        # Enforce a 1.0 second timeout limit
        subprocess.run(cmd, timeout=1.0)
    except subprocess.TimeoutExpired as exc:
        duration = time.perf_counter() - start
        print(f"Process exceeded timeout limit after {duration:.2f}s!")
        print(f"TimeoutExpired args: {exc.cmd}, Timeout: {exc.timeout}s")

run_with_timeout()
# Output:
# Process exceeded timeout limit after ~1.00s!
# TimeoutExpired args: ['.../python', '-c', '...'], Timeout: 1.0s
```

### Example 4: Asynchronous Streaming with `Popen`
```python
import subprocess
import sys

def stream_child_output():
    # Python script printing 5 lines with simulated work
    code = (
        "import sys, time\n"
        "for i in range(1, 4):\n"
        "    print(f'Progress checkpoint {i}', flush=True)\n"
        "    time.sleep(0.1)\n"
    )
    cmd = [sys.executable, "-u", "-c", code]  # -u forces unbuffered stdout

    process = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )

    # Stream lines in real-time as they are emitted by the child
    print("Reading child stream:")
    for line in process.stdout:
        print(f"  [STREAMED]: {line.strip()}")

    process.wait()
    print("Child process finalized with code:", process.returncode)

stream_child_output()
# Output:
# Reading child stream:
#   [STREAMED]: Progress checkpoint 1
#   [STREAMED]: Progress checkpoint 2
#   [STREAMED]: Progress checkpoint 3
# Child process finalized with code: 0
```

### Example 5: Connecting Multi-Process Pipelines (`p1 | p2`)
```python
import subprocess
import sys

def run_piped_processes():
    """
    Simulates shell pipeline: cat names.txt | grep Alice
    without invoking shell=True.
    """
    # Process 1: Generates names
    gen_code = "print('Bob\\nAlice\\nCharlie\\nAlice Smith')"
    p1 = subprocess.Popen([sys.executable, "-c", gen_code], stdout=subprocess.PIPE)

    # Process 2: Filters names matching 'Alice'
    filter_code = "import sys; [print('Found:', line.strip()) for line in sys.stdin if 'Alice' in line]"
    p2 = subprocess.Popen(
        [sys.executable, "-c", filter_code],
        stdin=p1.stdout,  # Pipe p1 stdout directly to p2 stdin
        stdout=subprocess.PIPE,
        text=True
    )

    # Allow p1 to receive SIGPIPE if p2 exits early
    p1.stdout.close()
    
    output, _ = p2.communicate()
    print("Pipeline Output:\n" + output.strip())

run_piped_processes()
# Output:
# Pipeline Output:
# Found: Alice
# Found: Alice Smith
```

---

## Edge Cases, Pitfalls & Anti-Patterns

### 1. The `shell=True` Command Injection Catastrophe
When `shell=True` is enabled, Python passes the command string directly to the host shell (`/bin/sh -c` on POSIX or `cmd.exe /c` on Windows). If any part of the command string incorporates untrusted user input, an attacker can append shell operators (`;`, `&&`, `|`) to execute arbitrary commands!

```python
# CATASTROPHIC VULNERABILITY:
user_input = "file.txt; rm -rf /"
# Executes: ls file.txt; rm -rf /
subprocess.run(f"ls {user_input}", shell=True)

# SECURE PARADIGM:
# Passes arguments directly to the OS kernel without shell parsing
safe_args = ["ls", user_input]
subprocess.run(safe_args, shell=False)  # Looks for a file literally named 'file.txt; rm -rf /'
```

### 2. The `p.stdout.read()` Deadlock
Calling `p.stdout.read()` or `p.wait()` on a `Popen` instance when the child generates more than 64 KB of data on both `stdout` and `stderr` leads to permanent deadlock.
- **Why**: The OS pipe buffer fills up, causing the child to freeze waiting for the parent. The parent is waiting for the child to exit before reading.
- **Fix**: Always use `p.communicate()` to consume both streams concurrently.

### 3. Missing `flush=True` in Child Processes
If you are streaming output from a child Python script, Python's runtime buffers `stdout` in 4 KB or 8 KB blocks when connected to a pipe (non-TTY). The parent will receive nothing until the child flushes or exits.
- **Fix**: Run the child with `python -u` (unbuffered) or call `print(..., flush=True)`.

### Common Mistakes Table

| Bad Code | Good Code | Why | Severity |
| :--- | :--- | :--- | :--- |
| `subprocess.run(f"git clone {url}", shell=True)` | `subprocess.run(["git", "clone", url])` | Severe command injection vulnerability. | Critical |
| `p = Popen(...); p.wait(); out = p.stdout.read()` | `out, _ = p.communicate()` | Pipe buffer saturation (>64KB) causes permanent deadlock. | Critical |
| Omitting `check=True` and ignoring returncode | `subprocess.run(..., check=True)` | Silent failures cascade into corrupted application state. | High |
| `subprocess.run("ls -l")` (string with `shell=False`) | `subprocess.run(["ls", "-l"])` | Searches for executable literally named `"ls -l"`; raises `FileNotFoundError`. | High |
| Forgetting `timeout=` on external CLI calls | `subprocess.run(..., timeout=30)` | External process hang leaves parent Python thread blocked forever. | High |
| Reading large binary output with `text=True` | `capture_output=True` (binary default) | Corrupts binary streams with Unicode decoding errors. | Medium |

---

## Performance & Memory Characteristics

### Process Creation Overhead
Spawning an OS child process is several orders of magnitude more expensive than spawning a Python thread:
- **Python Thread**: $\approx 10 \mu\text{s}$ to spawn.
- **`subprocess.run` (via `posix_spawn`)**: $\approx 1.5 - 3.0 \text{ ms}$.
- **`subprocess.run(shell=True)`**: $\approx 6.0 - 15.0 \text{ ms}$ (spawns `/bin/sh` which then spawns target binary).

Avoid spawning subprocesses in tight loops (e.g. 10,000 times). Instead, spawn a single persistent worker process and communicate via pipes or sockets.

---

## CPython Internals & Bytecode

In CPython, process spawning is orchestrated in `Modules/_posixsubprocess.c` on Unix.
1. CPython creates POSIX pipes using `pipe2()` with `O_CLOEXEC` to prevent file descriptor leaks into unrelated processes.
2. In the child process branch, `dup2()` binds pipe descriptors to `0` (stdin), `1` (stdout), and `2` (stderr).
3. If `posix_spawn` is viable, CPython sets file actions and attributes and calls `posix_spawnp()` directly, executing in the host C runtime without Python VM overhead.

Bytecode for `subprocess.run`:
```python
import dis
import subprocess

def invoke(cmd):
    return subprocess.run(cmd, check=True)

dis.dis(invoke)
```
```text
  1           0 RESUME                   0
  2           2 LOAD_GLOBAL              1 (subprocess + NULL)
             12 LOAD_ATTR                3 (run)
             22 LOAD_FAST                0 (cmd)
             24 LOAD_CONST               1 (True)
             26 KW_NAMES                 2 ('check',)
             28 CALL                     2
             36 RETURN_VALUE
```

---

## Best Practices & Idiomatic Patterns

1. **Always Pass Command Arguments as a List**:
   ```python
   subprocess.run(["ffmpeg", "-i", "input.mp4", "output.avi"])
   ```
2. **Always Enforce a Timeout**:
   Protect services from hangs caused by external tools waiting on network resources:
   ```python
   subprocess.run(cmd, timeout=60.0)
   ```
3. **Capture Both Streams or Discard Explicitly**:
   ```python
   # Discard if unneeded
   subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
   ```
4. **Use `shlex.split()` When Parsing Shell Strings**:
   If you receive a command string from configuration, parse it safely into a list:
   ```python
   import shlex
   args = shlex.split("docker run --rm -v /tmp:/data alpine:latest")
   subprocess.run(args)
   ```

---

## Real-World Case Study: Resilient External Tool Pipeline with Streaming Telemetry

The following production runner executes external binaries with streaming telemetry, memory/timeout limits, and graceful escalation from `SIGTERM` to `SIGKILL`.

```python
import subprocess
import time
import sys
from typing import List, Optional

class ProcessExecutionError(Exception):
    def __init__(self, code: int, stderr: str):
        super().__init__(f"Command failed with exit code {code}: {stderr}")
        self.code = code
        self.stderr = stderr

class ResilientCommandRunner:
    def __init__(self, default_timeout: float = 30.0):
        self.default_timeout = default_timeout

    def execute_streaming(self, cmd: List[str], timeout: Optional[float] = None) -> int:
        timeout = timeout or self.default_timeout
        start_time = time.perf_counter()

        print(f"[RUNNER] Spawning: {' '.join(cmd)}")
        process = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1  # Line buffered
        )

        try:
            # Stream stdout line-by-line
            while True:
                line = process.stdout.readline()
                if not line and process.poll() is not None:
                    break
                if line:
                    print(f"  [STDOUT] {line.strip()}", flush=True)

                # Check timeout
                if (time.perf_counter() - start_time) > timeout:
                    raise subprocess.TimeoutExpired(cmd, timeout)

            # Process completed; read remaining stderr
            _, stderr_data = process.communicate()
            if process.returncode != 0:
                raise ProcessExecutionError(process.returncode, stderr_data.strip())

            print(f"[RUNNER] Success! Elapsed: {time.perf_counter() - start_time:.2f}s")
            return process.returncode

        except subprocess.TimeoutExpired:
            print(f"[RUNNER WARN] Process timed out after {timeout}s. Terminating...", file=sys.stderr)
            process.terminate()
            try:
                # Wait 2 seconds for graceful shutdown
                process.wait(timeout=2.0)
            except subprocess.TimeoutExpired:
                print("[RUNNER ERR] Child refused SIGTERM. Issuing SIGKILL...", file=sys.stderr)
                process.kill()
                process.wait()
            raise TimeoutError(f"Process exceeded maximum time limit of {timeout}s")

# Verification
if __name__ == "__main__":
    runner = ResilientCommandRunner(default_timeout=5.0)
    
    # 1. Successful execution
    cmd_ok = [sys.executable, "-u", "-c", "import sys; print('Worker initialized'); print('Task completed')"]
    runner.execute_streaming(cmd_ok)

    # 2. Timeout escalation execution
    cmd_hang = [sys.executable, "-c", "import time; time.sleep(10)"]
    try:
        runner.execute_streaming(cmd_hang, timeout=1.0)
    except TimeoutError as e:
        print("Caught expected timeout:", e)
```

---

## Interview Questions & Deep-Dive Scenarios

<details>
<summary><strong>1. Junior: Why is <code>shell=True</code> strongly discouraged when invoking subprocesses?</strong></summary>

When `shell=True` is enabled, the command is passed to the system shell (`/bin/sh` or `cmd.exe`). If any component of the command contains unvalidated user input, attackers can inject shell metacharacters (e.g. `;`, `&&`, `|`, `` ` ``) to execute arbitrary commands with the privileges of the Python process.
</details>

<details>
<summary><strong>2. Junior: What is the difference between <code>subprocess.run()</code> and <code>subprocess.Popen()</code>?</strong></summary>

`subprocess.run()` is a blocking, high-level wrapper introduced in Python 3.5. It starts the child process, waits for it to finish, and returns a `CompletedProcess` object. `subprocess.Popen()` is the lower-level API that starts the child process asynchronously and returns immediately, allowing streaming I/O, background execution, and manual lifecycle management.
</details>

<details>
<summary><strong>3. Mid: What causes a deadlock when using <code>p.stdout.read()</code> on a <code>Popen</code> object, and how is it prevented?</strong></summary>

OS pipes have fixed-size internal buffers (typically 64 KB). If a child process writes more data to `stdout` or `stderr` than the pipe buffer can hold, the child process blocks waiting for buffer space to clear. If the parent is waiting for the child to finish via `p.wait()` or reading only `stdout` while the child is blocked trying to write to `stderr`, both processes freeze forever. To prevent this, use `p.communicate()`, which reads all streams concurrently using background threads or `selectors`.
</details>

<details>
<summary><strong>4. Mid: What does <code>check=True</code> do in <code>subprocess.run()</code>?</strong></summary>

By default (`check=False`), `subprocess.run()` returns a `CompletedProcess` object regardless of whether the child succeeded or failed. Setting `check=True` causes Python to inspect `returncode`. If the return code is non-zero, it automatically raises `subprocess.CalledProcessError`, containing the exit code, command, stdout, and stderr.
</details>

<details>
<summary><strong>5. Senior: How does CPython optimize process creation on modern Linux using <code>posix_spawn</code> instead of <code>fork()</code>?</strong></summary>

When `close_fds=True` and standard options are specified, CPython uses `posix_spawn()` rather than traditional `fork()` + `execve()`. In a multi-gigabyte Python process, `fork()` must duplicate the page table entries for the entire address space, causing noticeable latency and risking memory allocation failures under strict overcommit policies (`vm.overcommit_memory=2`). `posix_spawn()` bypasses page table copying, spawning the new binary in a fraction of the time with minimal memory overhead.
</details>

<details>
<summary><strong>6. Senior: How do you handle zombie processes when spawning long-running background tasks with <code>Popen</code>?</strong></summary>

A zombie process occurs on POSIX systems when a child terminates but the parent has not read its exit status via `wait()` or `waitpid()`. To prevent zombie leaks:
1. Regularly call `p.poll()` on active child processes.
2. In async servers, register a signal handler for `signal.SIGCHLD` calling `os.waitpid(-1, os.WNOHANG)`.
3. In multi-threaded systems, spawn a reaper thread that executes `process.wait()`.
</details>

<details>
<summary><strong>7. Staff: How does process signal escalation work across POSIX and Windows when terminating child processes?</strong></summary>

On POSIX, termination begins with `process.terminate()` which sends `SIGTERM`, allowing the child to run cleanup handlers. If the child fails to exit within a grace period, `process.kill()` sends `SIGKILL`, which the OS kernel handles directly without user-space interception.
On Windows, POSIX signals do not exist natively. `terminate()` and `kill()` both invoke the Win32 `TerminateProcess()` API, which immediately terminates the process without triggering in-process cleanup handlers.
</details>

<details>
<summary><strong>8. Staff: How would you architect a secure sandboxed runner that executes untrusted user-submitted code via <code>subprocess</code>?</strong></summary>

1. Pass arguments strictly as a list with `shell=False`.
2. Restrict process privileges: On Linux, use `preexec_fn` or `user=`/`group=` arguments to switch to an unprivileged user (`nobody`), apply `prctl(PR_SET_NO_NEW_PRIVS)`, and apply `seccomp` system call filters.
3. Enforce hard resource quotas: Use `resource.setrlimit()` to bound CPU time, memory address space, and max open file descriptors.
4. Execute inside containerized or cgroup boundaries (e.g. systemd transient cgroups or Docker rootless).
5. Enforce strict timeouts and stream byte quotas on captured output.
</details>

---

## Practical Exercises & Code Katas

### Exercise 1: Safe Git Branch Inspector
**Requirement:** Write a function `get_current_git_branch() -> str` that runs `git rev-parse --abbrev-ref HEAD` safely, returning the branch name or raising a descriptive exception if not inside a git repository.

<details>
<summary>Show Solution</summary>

```python
import subprocess

def get_current_git_branch() -> str:
    try:
        result = subprocess.run(
            ["git", "rev-parse", "--abbrev-ref", "HEAD"],
            capture_output=True,
            text=True,
            check=True
        )
        return result.stdout.strip()
    except subprocess.CalledProcessError as err:
        raise RuntimeError(f"Failed to query git branch: {err.stderr.strip()}")
    except FileNotFoundError:
        raise RuntimeError("git executable is not installed or not in PATH")

# Test
try:
    branch = get_current_git_branch()
    print("Active Git Branch:", branch)
except RuntimeError as e:
    print("Expected non-git environment error:", e)
```
</details>

### Exercise 2: Real-Time Line Counter Streamer
**Requirement:** Use `subprocess.Popen` to execute a script that outputs numbers and count lines in real-time without buffering.

<details>
<summary>Show Solution</summary>

```python
import subprocess
import sys

def count_streamed_lines() -> int:
    code = "import sys; [print(f'line_{i}', flush=True) for i in range(10)]"
    cmd = [sys.executable, "-u", "-c", code]
    
    count = 0
    with subprocess.Popen(cmd, stdout=subprocess.PIPE, text=True) as proc:
        for line in proc.stdout:
            count += 1
    return count

total = count_streamed_lines()
print("Total streamed lines counted:", total)
assert total == 10
```
</details>

### Exercise 3: Subprocess Exit Code Classifier
**Requirement:** Write a function that executes a command and categorizes the outcome into an enum: `SUCCESS` (0), `NOT_FOUND` (executable missing), `TIMEOUT`, or `ERROR` (non-zero exit).

<details>
<summary>Show Solution</summary>

```python
import subprocess
from enum import Enum
from typing import List

class CommandStatus(Enum):
    SUCCESS = "success"
    NOT_FOUND = "not_found"
    TIMEOUT = "timeout"
    ERROR = "error"

def classify_execution(cmd: List[str], timeout: float = 2.0) -> CommandStatus:
    try:
        res = subprocess.run(cmd, timeout=timeout, capture_output=True)
        return CommandStatus.SUCCESS if res.returncode == 0 else CommandStatus.ERROR
    except FileNotFoundError:
        return CommandStatus.NOT_FOUND
    except subprocess.TimeoutExpired:
        return CommandStatus.TIMEOUT

# Tests
import sys
print("Existing:", classify_execution([sys.executable, "--version"]))
print("Missing:", classify_execution(["non_existent_binary_xyz_123"]))
```
</details>

### Exercise 4: Piping Data Directly to Subprocess Standard Input
**Requirement:** Run Python's `base64` module via subprocess, feeding raw string data into its `stdin` and capturing the base64-encoded output from `stdout`.

<details>
<summary>Show Solution</summary>

```python
import subprocess
import sys

def encode_base64_subprocess(payload: str) -> str:
    code = "import sys, base64; sys.stdout.write(base64.b64encode(sys.stdin.read().encode()).decode())"
    cmd = [sys.executable, "-c", code]

    res = subprocess.run(
        cmd,
        input=payload,
        capture_output=True,
        text=True,
        check=True
    )
    return res.stdout

encoded = encode_base64_subprocess("Antigravity Engineering")
print("Subprocess Base64 Encoded:", encoded)
assert encoded == "QW50aWdyYXZpdHkgRW5naW5lZXJpbmc="
```
</details>

### Exercise 5: Command Output Truncator
**Requirement:** Execute an external command and capture up to `max_bytes` of output, stopping the process early if output exceeds the limit.

<details>
<summary>Show Solution</summary>

```python
import subprocess
import sys

def run_with_byte_limit(cmd: list[str], max_bytes: int = 100) -> bytes:
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE)
    collected = bytearray()
    
    while chunk := proc.stdout.read(32):
        collected.extend(chunk)
        if len(collected) >= max_bytes:
            proc.kill()
            proc.wait()
            return bytes(collected[:max_bytes])
            
    proc.wait()
    return bytes(collected)

# Test: Script outputting infinite text
code = "import sys; [sys.stdout.write('A' * 1000) for _ in range(10)]"
res = run_with_byte_limit([sys.executable, "-u", "-c", code], max_bytes=50)
print(f"Collected exactly {len(res)} bytes:", res[:10])
assert len(res) == 50
```
</details>

### Exercise 6: Environment Variable Forwarder Filter
**Requirement:** Spawn a child process forwarding ONLY environment variables whose names start with `"APP_"`, blocking all other host variables from leaking into the child process.

<details>
<summary>Show Solution</summary>

```python
import os
import subprocess
import sys

def run_with_filtered_env(cmd: list[str]) -> str:
    filtered_env = {k: v for k, v in os.environ.items() if k.startswith("APP_")}
    # Required system paths on Windows/POSIX
    for sys_key in ("SYSTEMROOT", "PATH"):
        if sys_key in os.environ:
            filtered_env[sys_key] = os.environ[sys_key]

    res = subprocess.run(
        cmd,
        env=filtered_env,
        capture_output=True,
        text=True,
        check=True
    )
    return res.stdout.strip()

# Test
os.environ["APP_CONFIG_VAL"] = "secure_token_123"
os.environ["HOST_SECRET"] = "do_not_leak"

code = "import os; print('APP_CONFIG_VAL:', os.getenv('APP_CONFIG_VAL'), 'HOST_SECRET:', os.getenv('HOST_SECRET'))"
out = run_with_filtered_env([sys.executable, "-c", code])
print("Child process saw:", out)
assert "APP_CONFIG_VAL: secure_token_123" in out
assert "HOST_SECRET: None" in out
```
</details>
