---
title: "Command-Line Interfaces: argparse, sys.argv, Click, and Typer"
description: "Comprehensive guide to building production-grade command-line interfaces in Python: low-level sys.argv parsing, standard library argparse mastery, subcommands, and modern paradigms with Click and Typer."
module: "09-files-and-io"
order: 9
tags:
  - python
  - cli
  - argparse
  - click
  - typer
  - sys-argv
  - command-line
difficulty: "Intermediate"
cpython_version: "3.12+"
last_verified: "2026-03"
---

# Command-Line Interfaces: argparse, sys.argv, Click, and Typer

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
10. [Real-World Case Study: Production Multi-Subcommand DevOps CLI](#real-world-case-study-production-multi-subcommand-devops-cli)
11. [Interview Questions & Deep-Dive Scenarios](#interview-questions--deep-dive-scenarios)
12. [Practical Exercises & Code Katas](#practical-exercises--code-katas)

---

## What & Why

Command-line interfaces (CLIs) are the primary boundary through which automated workflows, CI/CD pipelines, container runners, and system administrators interact with Python applications. Python provides multiple tiers of CLI construction:
- **`sys.argv`**: Raw, unparsed string list passed directly by the operating system kernel.
- **`argparse`**: The batteries-included standard library framework providing declarative argument definitions, flag parsing, automatic `--help` generation, and shell error exits.
- **`click`**: Widely-used ecosystem framework built around composable function decorators and POSIX argument syntax.
- **`typer`**: Modern, highly ergonomic framework that derives CLI parsers directly from Python 3.10+ type hints.

### Motivation
While reading raw strings from `sys.argv` is simple for single-flag scripts, real-world tools quickly require:
1. Distinguishing positional arguments (`deploy production`) from optional flags (`--timeout 30`, `-v`).
2. Automatic type validation and conversion (converting `"42"` into an integer `42`).
3. Generating human-friendly `--help` output with usage syntax and defaults.
4. Hierarchical subcommands (e.g. `docker container run` vs `docker image prune`).
5. Standardized exit codes (`0` for success, non-zero for errors).

### Real-World Analogy
Consider boarding an airplane:
- **`sys.argv`** is a security guard handing you a raw, unsorted pile of tickets, passports, and baggage stubs. You must manually inspect each paper, check its format, verify the flight number, and decide if it's valid.
- **`argparse`** is an automated turnstile kiosk. You configure the kiosk with exact rules (passport required, max 2 carry-ons, ticket must be valid date). The machine automatically scans tickets, rejects invalid travelers with clear error messages, and hands the flight attendants a structured manifest.
- **`Typer` / `Click`** is a premier biometric concierge desk that looks at your government ID (Python type hints) and automatically configures your boarding path without repetitive paperwork.

### When to Use vs When Not to Use
- **Use `sys.argv` for:** Micro-scripts (10–20 lines) requiring one or two positional values without flags or help text.
- **Use `argparse` for:** Standard library scripts, systems tools, distribution packages with zero external dependencies, and utility hooks.
- **Use `click` / `typer` for:** Complex production enterprise CLI tools, developer tooling, rich interactive terminals, and modern microservice CLIs.

---

## Core Concepts & Mental Model

```
+-------------------------------------------------------------------------+
| Operating System Kernel (Execve)                                        |
| argv: ["python", "app.py", "deploy", "--replicas", "3", "-v"]           |
+-------------------------------------------------------------------------+
                                    |
                                    v
+-------------------------------------------------------------------------+
| Python Runtime: sys.argv = ['app.py', 'deploy', '--replicas', '3', '-v']|
+-------------------------------------------------------------------------+
                                    |
          +-------------------------+-------------------------+
          |                                                   |
          v                                                   v
+------------------------------------+   +------------------------------------+
| Standard Library: argparse         |   | Third-Party: Typer / Click         |
| - ArgumentParser                   |   | - Type Hint Introspection          |
| - add_argument()                   |   | - Decorator Pipeline               |
| - add_subparsers()                 |   | - Automatic Rich Formatter         |
| - parse_args()                     |   | - Command Callbacks                |
+------------------------------------+   +------------------------------------+
          |                                                   |
          v                                                   v
+-------------------------------------------------------------------------+
| Strongly Typed Application Logic Namespace                              |
| args.subcommand = 'deploy'                                              |
| args.replicas = 3 (int)                                                 |
| args.verbose = True (bool)                                              |
+-------------------------------------------------------------------------+
```

### Key Terminology
1. **Positional Arguments**: Mandatory values defined by their sequential position (e.g. `cp <src> <dest>`).
2. **Optional Arguments / Flags**: Prefixed with `-` (short flag, e.g. `-f`) or `--` (long option, e.g. `--force`).
3. **Action**: The behavior triggered when a flag is encountered (e.g. `store_true`, `store_const`, `append`, `count`).
4. **Subparsers**: Hierarchical command routing where the first positional argument selects an entire independent sub-parser (e.g. `git commit` vs `git push`).

---

## How It Works Internally

### 1. `sys.argv` Mechanics
When an OS executes a process (via `execve()` on Linux or `CreateProcessW()` on Windows), it passes an array of null-terminated strings (`char *argv[]`).
In CPython:
- The runtime decodes these strings into Python `str` objects using `os.fsdecode()`.
- `sys.argv[0]` holds the script name or path (or `""` if executed interactively via `-c`).
- `sys.argv[1:]` holds all arguments passed after the script invocation.

### 2. `argparse` Parsing Pipeline
1. **Action Registration**: `add_argument()` registers an `Action` object in an internal list.
2. **Tokenization**: `parse_args()` scans `sys.argv[1:]` and categorizes tokens:
   - Tokens starting with `-` or `--` are resolved to optional actions.
   - Other tokens are assigned to positional actions sequentially.
3. **Conversion & Validation**:
   - Executes the argument's `type` callable (e.g. `int("3")`). If it raises `ValueError`, `argparse` calls `parser.error()`.
   - Validates choices if `choices=[...]` was defined.
4. **Namespace Construction**: Instantiates an `argparse.Namespace` object (a lightweight object with dynamic attributes) and sets values corresponding to `dest` keys.

---

## Syntax & API Reference

### `argparse.ArgumentParser` Constructors & Methods
```python
import argparse

parser = argparse.ArgumentParser(
    prog="mytool",                     # Program name for help display
    description="Tool description",     # Shown before argument help
    epilog="Footer text",               # Shown after argument help
    formatter_class=argparse.HelpFormatter, # Formatting customization
    add_help=True                       # Enables -h / --help automatically
)

# Adding arguments
parser.add_argument(
    "-c", "--count",
    action="store",                     # store, store_true, store_false, append, count
    nargs="?",                          # N, '?', '*', '+', or argparse.REMAINDER
    const=None,                         # Value used if flag present with no value
    default=1,                          # Default if flag omitted
    type=int,                           # Converter callable (int, float, Path, etc.)
    choices=[1, 2, 5, 10],              # Permitted values
    required=False,                     # True if optional flag is mandatory
    help="Number of iterations",        # Documentation string
    metavar="N",                        # Name displayed in usage help
    dest="count"                        # Attribute name in result Namespace
)

# Parsing
args = parser.parse_args(args=None)      # Defaults to sys.argv[1:]
```

### Subcommands with `add_subparsers`
```python
subparsers = parser.add_subparsers(dest="command", required=True)
run_parser = subparsers.add_parser("run", help="Run worker")
run_parser.add_argument("--workers", type=int, default=4)
```

---

## Code Examples

### Example 1: Minimal Standard Library `argparse` Script
```python
import argparse
import sys

def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="portscan",
        description="Scans network ports for active listeners."
    )
    # Positional argument
    parser.add_argument("host", type=str, help="Target hostname or IP address")
    
    # Optional flag with type conversion and default
    parser.add_argument(
        "-p", "--port",
        type=int,
        default=80,
        help="Port number to test (default: 80)"
    )
    
    # Boolean switch flag
    parser.add_argument(
        "-v", "--verbose",
        action="store_true",
        help="Enable debug logging output"
    )
    return parser

def main():
    parser = build_parser()
    # Explicitly pass argument list for reproducible demonstration
    test_args = ["192.168.1.1", "-p", "443", "--verbose"]
    args = parser.parse_args(test_args)

    print("Parsed Host:", args.host)
    print("Parsed Port:", args.port)
    print("Is Verbose:", args.verbose)

main()
# Output:
# Parsed Host: 192.168.1.1
# Parsed Port: 443
# Is Verbose: True
```

### Example 2: Subparsers for Multi-Command Tools (Git-Style)
```python
import argparse

def run_subparser_demo():
    root_parser = argparse.ArgumentParser(prog="pkgmgr")
    subparsers = root_parser.add_subparsers(dest="subcommand", required=True)

    # Subcommand: install
    install_parser = subparsers.add_parser("install", help="Install a package")
    install_parser.add_argument("package_name", help="Package name to fetch")
    install_parser.add_argument("--upgrade", action="store_true", help="Upgrade if exists")

    # Subcommand: remove
    remove_parser = subparsers.add_parser("remove", help="Remove a package")
    remove_parser.add_argument("package_name", help="Package name to delete")
    remove_parser.add_argument("-f", "--force", action="store_true", help="Bypass confirmation")

    # Test 'install' command
    args_install = root_parser.parse_args(["install", "requests", "--upgrade"])
    print("Command:", args_install.subcommand)
    print("Package:", args_install.package_name)
    print("Upgrade:", args_install.upgrade)

    # Test 'remove' command
    args_remove = root_parser.parse_args(["remove", "urllib3", "-f"])
    print("\nCommand:", args_remove.subcommand)
    print("Package:", args_remove.package_name)
    print("Force:", args_remove.force)

run_subparser_demo()
# Output:
# Command: install
# Package: requests
# Upgrade: True
# 
# Command: remove
# Package: urllib3
# Force: True
```

### Example 3: Custom Type Validators and Mutually Exclusive Groups
```python
import argparse
from pathlib import Path

def validate_positive_int(val_str: str) -> int:
    try:
        val = int(val_str)
        if val <= 0:
            raise ValueError()
        return val
    except ValueError:
        raise argparse.ArgumentTypeError(f"'{val_str}' must be a strictly positive integer (> 0)")

def run_validation_demo():
    parser = argparse.ArgumentParser(prog="deployer")
    parser.add_argument("--retries", type=validate_positive_int, default=3)

    # Mutually Exclusive Group: Can pass --json OR --xml, but never both
    output_group = parser.add_mutually_exclusive_group()
    output_group.add_argument("--json", action="store_true", help="Output in JSON")
    output_group.add_argument("--xml", action="store_true", help="Output in XML")

    # Valid execution
    args = parser.parse_args(["--retries", "5", "--json"])
    print("Retries:", args.retries)
    print("JSON Format:", args.json)
    print("XML Format:", args.xml)

run_validation_demo()
# Output:
# Retries: 5
# JSON Format: True
# XML Format: False
```

### Example 4: Modern Declarative CLI with Typer / Click Paradigm
*(Demonstrating Click-style decorator abstraction using pure standard library patterns)*
```python
import argparse
from typing import Callable, Any

class SimpleCLI:
    """Lightweight decorator-driven command registry."""
    def __init__(self, description: str):
        self.parser = argparse.ArgumentParser(description=description)
        self.subparsers = self.parser.add_subparsers(dest="cmd", required=True)
        self.handlers: dict[str, Callable] = {}

    def command(self, name: str):
        def decorator(func: Callable):
            sub = self.subparsers.add_parser(name, help=func.__doc__)
            # Reflect annotations
            for param, p_type in func.__annotations__.items():
                if param != "return":
                    sub.add_argument(f"--{param}", type=p_type, required=True)
            self.handlers[name] = func
            return func
        return decorator

    def execute(self, args_list: list[str]):
        parsed = self.parser.parse_args(args_list)
        kwargs = {k: v for k, v in vars(parsed).items() if k != "cmd"}
        return self.handlers[parsed.cmd](**kwargs)

# Usage
app = SimpleCLI("Service Administration Tool")

@app.command("restart")
def restart_server(service: str, delay_seconds: int):
    """Restarts a specific system service."""
    return f"Service {service} restarting in {delay_seconds}s..."

result = app.execute(["restart", "--service", "nginx", "--delay_seconds", "10"])
print("Execution Result:", result)
# Output:
# Execution Result: Service nginx restarting in 10s...
```

### Example 5: Handling Unknown Arguments and Pass-Through Flags
```python
import argparse

def run_passthrough_demo():
    parser = argparse.ArgumentParser(prog="wrapper")
    parser.add_argument("-e", "--env", default="dev")

    # parse_known_args separates defined args from arbitrary downstream flags
    raw_input = ["--env", "prod", "--extra-flag", "value", "-x"]
    known_args, unknown_flags = parser.parse_known_args(raw_input)

    print("Recognized by wrapper:", known_args)
    print("Forwarded to downstream binary:", unknown_flags)

run_passthrough_demo()
# Output:
# Recognized by wrapper: Namespace(env='prod')
# Forwarded to downstream binary: ['--extra-flag', 'value', '-x']
```

---

## Edge Cases, Pitfalls & Anti-Patterns

### 1. `argparse.ArgumentParser.error()` Calling `sys.exit()`
By default, when `argparse` encounters an invalid flag or missing required argument, it prints usage information to `sys.stderr` and immediately calls `sys.exit(2)`. In test suites or interactive environments, this aborts the process!
- **Solution (Python 3.9+)**: Set `exit_on_error=False` on `ArgumentParser`. When parsing fails, it raises `argparse.ArgumentError` instead of terminating the process.

### 2. Passing Negative Numbers as Positional Arguments
If an argument value is `-5`, `argparse` may mistake it for an optional flag named `-5` and raise `unrecognized arguments: -5`.
- **Solution**: Use the POSIX end-of-options marker `--` (double hyphen):
  `python script.py -- -5` signals to the parser that all subsequent tokens are strictly positional.

### 3. Mutating `sys.argv` Globally
Directly modifying `sys.argv` inside application modules creates hidden side effects across test fixtures. Always pass explicit argument lists to `parser.parse_args(args)` rather than relying on global state.

### Common Mistakes Table

| Bad Code | Good Code | Why | Severity |
| :--- | :--- | :--- | :--- |
| `val = int(sys.argv[1])` | Use `argparse` with `type=int` | Raises unhandled `IndexError` or `ValueError` with zero user guidance. | High |
| `parser.add_argument("--flag", type=bool)` | `action="store_true"` | `bool("False")` evaluates to `True` in Python; `--flag False` results in `True`! | Critical |
| Unhandled `sys.exit()` in unit tests | `ArgumentParser(exit_on_error=False)` | Parsing errors kill test runner process immediately. | High |
| Hand-rolling `--help` parser | Rely on `argparse` built-in help | Inconsistent formatting, missing usage syntax, poor user experience. | Medium |
| Using string concatenation for CLI calls | Pass arguments as a structured list to `subprocess` | Concatenating user inputs into shell strings causes command injection. | Critical |
| Catching generic `Exception` around `parse_args` | Catch `argparse.ArgumentError` | Hides underlying bugs and masks unexpected runtime exceptions. | Medium |

---

## Performance & Memory Characteristics

### Startup Overhead Comparison
- **`sys.argv`**: $\approx 0.001 \text{ ms}$ (pre-populated by CPython runtime; zero imports).
- **`argparse`**: $\approx 0.5 \text{ ms}$ (imports standard library modules `re`, `textwrap`).
- **`click`**: $\approx 15 \text{ ms}$ (imports complex terminal abstraction trees).
- **`typer`**: $\approx 35 \text{ ms}$ (imports `click`, `pydantic`, `typing_extensions`).

For short-lived CLI utilities invoked thousands of times per second in shell scripts, `argparse` provides the ideal compromise between sub-millisecond startup and full CLI feature completeness.

---

## CPython Internals & Bytecode

In CPython (`Python/sysmodule.c`), `sys.argv` is populated during runtime initialization:
```c
PyObject *
_PySys_CreateArgvList(int argc, wchar_t * const *argv) {
    PyObject *list = PyList_New(argc);
    for (int i = 0; i < argc; i++) {
        PyObject *v = PyUnicode_FromWideChar(argv[i], -1);
        PyList_SET_ITEM(list, i, v);
    }
    return list;
}
```
`sys.argv` is a standard Python `list` of `str` objects.

Bytecode for accessing `sys.argv`:
```python
import dis
import sys

def get_first_arg():
    return sys.argv[1]

dis.dis(get_first_arg)
```
```text
  1           0 RESUME                   0
  2           2 LOAD_GLOBAL              1 (sys + NULL)
             12 LOAD_ATTR                3 (argv)
             22 LOAD_CONST               1 (1)
             24 BINARY_SUBSCR
             28 RETURN_VALUE
```

---

## Best Practices & Idiomatic Patterns

1. **Avoid `type=bool` in `add_argument`**:
   Never use `type=bool`. Always use `action="store_true"` or `action="store_false"`.
2. **Set `exit_on_error=False` for Testability**:
   Allows testing your parser with invalid inputs without catching `SystemExit`:
   ```python
   parser = argparse.ArgumentParser(exit_on_error=False)
   ```
3. **Use `metavar` for Clean Documentation**:
   ```python
   parser.add_argument("--timeout", type=int, metavar="SECONDS", help="Timeout duration")
   ```
4. **Follow POSIX Exit Code Standards**:
   - `0`: Success.
   - `1`: General application error.
   - `2`: Misuse of shell built-ins or CLI syntax error (used by `argparse`).

---

## Real-World Case Study: Production Multi-Subcommand DevOps CLI

The following script implements a complete production DevOps CLI for deploying microservices, managing secrets, and inspecting cluster status.

```python
import argparse
import sys
from typing import List, Optional

class DevOpsCLI:
    def __init__(self):
        self.parser = self._configure_parser()

    def _configure_parser(self) -> argparse.ArgumentParser:
        parser = argparse.ArgumentParser(
            prog="clusterctl",
            description="Production Cluster Management Utility",
            epilog="Run 'clusterctl <subcommand> --help' for details on specific commands.",
            exit_on_error=False
        )
        parser.add_argument("-v", "--verbose", action="store_true", help="Enable verbose diagnostics")
        parser.add_argument("--cluster", default="us-east-1", choices=["us-east-1", "eu-west-1", "ap-southeast-1"])

        subparsers = parser.add_subparsers(dest="subcommand", required=True, help="Subcommand to execute")

        # Command: deploy
        deploy = subparsers.add_parser("deploy", help="Deploy image to service")
        deploy.add_argument("service", help="Target service name")
        deploy.add_argument("tag", help="Docker image tag (e.g. v1.2.0)")
        deploy.add_argument("--replicas", type=int, default=3, help="Desired replica count")
        deploy.add_argument("--timeout-sec", type=int, default=120, help="Rollout timeout")

        # Command: status
        status = subparsers.add_parser("status", help="Inspect service health")
        status.add_argument("service", nargs="?", default=None, help="Specific service or omit for all")
        status.add_argument("--healthy-only", action="store_true", help="Filter healthy instances")

        return parser

    def run(self, raw_args: Optional[List[str]] = None) -> int:
        try:
            args = self.parser.parse_args(raw_args)
        except argparse.ArgumentError as err:
            print(f"CLI Error: {err}", file=sys.stderr)
            return 2

        if args.verbose:
            print(f"[DEBUG] Operating on Cluster: {args.cluster}")

        if args.subcommand == "deploy":
            print(f"Deploying {args.service}:{args.tag} with {args.replicas} replicas (Timeout: {args.timeout_sec}s)...")
            return 0
        elif args.subcommand == "status":
            target = args.service or "ALL SERVICES"
            print(f"Fetching status for: {target} (Healthy only: {args.healthy_only})")
            return 0
        return 1

# Verification
if __name__ == "__main__":
    cli = DevOpsCLI()
    print("Test 1: Deploy Subcommand")
    code1 = cli.run(["--cluster", "eu-west-1", "deploy", "auth-api", "v2.0.1", "--replicas", "5", "-v"])
    assert code1 == 0

    print("\nTest 2: Status Subcommand")
    code2 = cli.run(["status", "--healthy-only"])
    assert code2 == 0
```

---

## Interview Questions & Deep-Dive Scenarios

<details>
<summary><strong>1. Junior: Why is passing <code>type=bool</code> to <code>parser.add_argument()</code> almost always a serious bug?</strong></summary>

In Python, `bool("False")` evaluates to `True` because any non-empty string is truthy. If an argument is configured with `type=bool`, executing `--flag False` results in `bool("False")`, which stores `True`! To implement boolean flags, use `action="store_true"` or `action="store_false"`.
</details>

<details>
<summary><strong>2. Junior: What is the purpose of <code>sys.argv[0]</code>?</strong></summary>

`sys.argv[0]` contains the name of the script or the file path used to invoke the Python process. If Python was run with `-c` (inline code), it contains `'-c'`. If run with `-m` (module), it contains the full path of the executed module.
</details>

<details>
<summary><strong>3. Mid: What is the difference between <code>parse_args()</code> and <code>parse_known_args()</code> in <code>argparse</code>?</strong></summary>

`parse_args()` expects all passed arguments to match registered flags or positional arguments; any unrecognized tokens raise an error and exit. `parse_known_args()` parses all recognized arguments into a `Namespace` and returns a 2-tuple: `(namespace, remaining_unrecognized_args)`. This is essential for wrapper scripts forwarding arbitrary trailing flags to underlying CLI commands.
</details>

<details>
<summary><strong>4. Mid: What does the special double-hyphen token <code>--</code> signify in POSIX command-line arguments?</strong></summary>

`--` indicates the **end of command options**. Any tokens appearing after `--` are treated strictly as positional arguments, even if they begin with a hyphen. For example, to delete a file named `-f` without it being interpreted as the `-f` (force) flag, you execute `rm -- -f`.
</details>

<details>
<summary><strong>5. Senior: How does <code>argparse</code> handle subcommand dispatching, and how can you attach execution logic directly to each subcommand?</strong></summary>

Subparsers are added via `parser.add_subparsers(dest="subcommand")`. To avoid large `if/elif` statements checking `args.subcommand`, you can use `parser.set_defaults(func=handler_function)` on each subparser. After calling `args = parser.parse_args()`, you can cleanly execute the assigned function via `args.func(args)`.
</details>

<details>
<summary><strong>6. Senior: How do you prevent <code>argparse</code> from terminating the Python process when a parsing error occurs during automated unit testing?</strong></summary>

In Python 3.9+, pass `exit_on_error=False` to the `argparse.ArgumentParser` constructor. When set, invalid flags or missing required arguments raise `argparse.ArgumentError` instead of printing to `sys.stderr` and calling `sys.exit(2)`. Tests can catch `ArgumentError` using `pytest.raises()` or `self.assertRaises()`.
</details>

<details>
<summary><strong>7. Staff: Compare the architecture and startup performance tradeoffs of standard library <code>argparse</code> versus <code>Typer</code> in high-frequency CLI workflows.</strong></summary>

`argparse` is part of the standard library and imports only minimal text-processing modules, starting up in under 1 ms. `Typer` is built on top of `Click` and `Pydantic` and uses runtime type inspection, increasing cold startup time to 30–50 ms. For long-running interactive tools, 40 ms is imperceptible, and Typer's developer ergonomics, automatic shell completion, and Rich formatting excel. For short-lived Git hooks, Docker health checks, or scripts called thousands of times per second in a pipeline, `argparse` avoids substantial process overhead.
</details>

<details>
<summary><strong>8. Staff: How does CPython handle Unicode arguments on Windows versus POSIX systems?</strong></summary>

On POSIX systems, `argv` is passed as a C array of `char*` encoded using the system locale (typically UTF-8). CPython decodes them using `PyUnicode_DecodeLocale()`. On Windows, the OS native API is wide-character UTF-16 (`wchar_t**`). CPython calls `GetCommandLineW()` and `CommandLineToArgvW()` directly, preserving full Unicode fidelity regardless of the active legacy OEM code page.
</details>

---

## Practical Exercises & Code Katas

### Exercise 1: Dispatcher Function Pattern
**Requirement:** Configure an `argparse` parser with two subcommands (`start` and `stop`) using `.set_defaults(handler=...)` so that `args.handler(args)` runs without any `if/elif` branching.

<details>
<summary>Show Solution</summary>

```python
import argparse

def handle_start(args):
    return f"Daemon started on port {args.port}"

def handle_stop(args):
    return f"Daemon stopped (Force: {args.force})"

parser = argparse.ArgumentParser(prog="daemon")
subparsers = parser.add_subparsers(dest="subcommand", required=True)

# Subcommand: start
p_start = subparsers.add_parser("start")
p_start.add_argument("--port", type=int, default=8080)
p_start.set_defaults(handler=handle_start)

# Subcommand: stop
p_stop = subparsers.add_parser("stop")
p_stop.add_argument("-f", "--force", action="store_true")
p_stop.set_defaults(handler=handle_stop)

# Test execution
parsed_start = parser.parse_args(["start", "--port", "9000"])
print(parsed_start.handler(parsed_start))

parsed_stop = parser.parse_args(["stop", "-f"])
print(parsed_stop.handler(parsed_stop))
```
</details>

### Exercise 2: IP Address Custom Argument Type
**Requirement:** Implement a custom `type` validator function for `argparse` that parses an IPv4 address string into an `ipaddress.IPv4Address` object, raising `argparse.ArgumentTypeError` if invalid.

<details>
<summary>Show Solution</summary>

```python
import argparse
import ipaddress

def parse_ipv4(value: str) -> ipaddress.IPv4Address:
    try:
        return ipaddress.IPv4Address(value)
    except ValueError:
        raise argparse.ArgumentTypeError(f"'{value}' is not a valid IPv4 address")

parser = argparse.ArgumentParser(exit_on_error=False)
parser.add_argument("--ip", type=parse_ipv4, required=True)

# Test valid
args = parser.parse_args(["--ip", "10.0.0.1"])
print("Parsed IP Object:", args.ip, type(args.ip))
assert isinstance(args.ip, ipaddress.IPv4Address)
```
</details>

### Exercise 3: Key-Value Pair Accumulator Action
**Requirement:** Create a custom `argparse.Action` class `KeyValueAction` that parses `--set key=value` arguments into a single dictionary on the namespace.

<details>
<summary>Show Solution</summary>

```python
import argparse

class KeyValueAction(argparse.Action):
    def __call__(self, parser, namespace, values, option_string=None):
        existing = getattr(namespace, self.dest, None) or {}
        for item in values:
            if "=" not in item:
                raise argparse.ArgumentError(self, f"Option '{item}' must be in KEY=VALUE format")
            k, v = item.split("=", 1)
            existing[k.strip()] = v.strip()
        setattr(namespace, self.dest, existing)

parser = argparse.ArgumentParser()
parser.add_argument("--set", nargs="+", action=KeyValueAction, dest="config", default={})

args = parser.parse_args(["--set", "timeout=30", "env=prod", "retries=5"])
print("Config Dictionary:", args.config)
assert args.config == {"timeout": "30", "env": "prod", "retries": "5"}
```
</details>

### Exercise 4: Range-Checking Float Validator
**Requirement:** Create an argument type factory `float_range(min_val, max_val)` returning a callable that enforces inclusive range bounds on float arguments.

<details>
<summary>Show Solution</summary>

```python
import argparse

def float_range(min_val: float, max_val: float):
    def validator(arg_str: str) -> float:
        try:
            val = float(arg_str)
            if not (min_val <= val <= max_val):
                raise ValueError()
            return val
        except ValueError:
            raise argparse.ArgumentTypeError(f"Value '{arg_str}' must be between {min_val} and {max_val}")
    return validator

parser = argparse.ArgumentParser(exit_on_error=False)
parser.add_argument("--threshold", type=float_range(0.0, 1.0), required=True)

args = parser.parse_args(["--threshold", "0.75"])
print("Valid threshold:", args.threshold)
assert args.threshold == 0.75
```
</details>

### Exercise 5: Environment Variable Fallback Argument
**Requirement:** Implement a custom action or helper that populates an argument's default value from an environment variable if set, otherwise falling back to a hardcoded default.

<details>
<summary>Show Solution</summary>

```python
import argparse
import os

def env_default(env_var: str, fallback: str) -> str:
    return os.environ.get(env_var, fallback)

os.environ["API_KEY"] = "secret_env_token_42"

parser = argparse.ArgumentParser()
parser.add_argument(
    "--token",
    default=env_default("API_KEY", "default_guest_token"),
    help="API access token (defaults to $API_KEY)"
)

# Test without passing flag: picks up env var
args = parser.parse_args([])
print("Token picked from environment:", args.token)
assert args.token == "secret_env_token_42"
```
</details>

### Exercise 6: Counting Verbosity Flag
**Requirement:** Use `action="count"` to support progressive verbosity levels (`-v`, `-vv`, `-vvv`) mapped to standard Python logging levels.

<details>
<summary>Show Solution</summary>

```python
import argparse
import logging

def configure_logging():
    parser = argparse.ArgumentParser()
    parser.add_argument("-v", "--verbose", action="count", default=0, help="Increase verbosity level")
    
    args = parser.parse_args(["-vv"])
    
    # 0 = WARNING, 1 = INFO, 2+ = DEBUG
    if args.verbose == 0:
        level = logging.WARNING
    elif args.verbose == 1:
        level = logging.INFO
    else:
        level = logging.DEBUG
        
    print(f"Verbosity Count: {args.verbose}, Logging Level: {logging.getLevelName(level)}")
    return level

res = configure_logging()
assert res == logging.DEBUG
```
</details>
