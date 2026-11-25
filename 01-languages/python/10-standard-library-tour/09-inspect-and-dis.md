---
title: "Runtime Introspection and Bytecode Disassembly: inspect and dis"
description: "Mastering Python metaprogramming and internals: signature binding with inspect, call stack frame inspection, code objects, CPython bytecode disassembly with dis, and Python 3.11+ specializing adaptive interpreter instructions."
module: "10-standard-library-tour"
order: 9
tags:
  - python
  - inspect
  - dis
  - bytecode
  - introspection
  - metaprogramming
  - cpython
difficulty: "Advanced"
cpython_version: "3.12+"
last_verified: "2026-03"
---

# Runtime Introspection and Bytecode Disassembly: inspect and dis

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
10. [Real-World Case Study: Production Dependency Injection and RPC Framework](#real-world-case-study-production-dependency-injection-and-rpc-framework)
11. [Interview Questions & Deep-Dive Scenarios](#interview-questions--deep-dive-scenarios)
12. [Practical Exercises & Code Katas](#practical-exercises--code-katas)

---

## What & Why

Python is a dynamic, reflective programming language: running programs can inspect their own structure, examine live execution stack frames, extract live function signatures, read source code from memory, and disassemble compiled CPython virtual machine bytecode instructions.
- **`inspect`**: High-level runtime reflection module. It introspects live objects, extracts caller frames, analyzes type hints, binds positional and keyword arguments to signatures, and checks generator/coroutine states.
- **`dis`**: Low-level CPython bytecode disassembler. It decodes code objects (`__code__`), displays virtual machine opcodes (`LOAD_FAST`, `BINARY_OP`, `CALL`), and reveals the internal compilation choices made by CPython's peephole optimizer and Python 3.11+ Specializing Adaptive Interpreter (PEP 659).

### Motivation
Why inspect bytecode and signatures?
1. **Modern Framework Design (FastAPI, Pytest, Pydantic)**: Frameworks use `inspect.signature` to introspect dependency injection parameters, route handlers, and test fixtures dynamically.
2. **Performance Debugging & Optimization**: When comparing two algorithms (e.g. `list comprehension` vs `map()` vs `for` loop), `dis.dis()` reveals the exact number of stack operations, function call overheads, and interpreter jumps.
3. **Deep Diagnostic Tooling**: Exception handlers, profilers, and debuggers (like `pdb` or Sentry) use frame introspection (`inspect.currentframe()`, `inspect.stack()`) to extract local variables and active source lines when errors occur.

### Real-World Analogy
Consider examining a sports car:
- Normal programming is driving the car (pressing the gas pedal and steering).
- **`inspect`** is the computerized dashboard telemetry scanner: it plugs into the diagnostic port to display engine RPM, valve configurations, tire pressure, and passenger seatbelt sensors.
- **`dis`** is popping the engine hood open and inspecting the physical mechanical pistons, crankshaft gears, and spark plugs in motion.

### When to Use vs When Not to Use
- **Use `inspect` for:**
  - Dependency injection frameworks (FastAPI-style parameter injection).
  - Decorators that must preserve, validate, or modify wrapped function argument signatures.
  - Logging or auditing frameworks capturing caller file names, function names, and line numbers.
- **Use `dis` for:**
  - Micro-optimizing critical algorithmic bottlenecks.
  - Educational deep dives into CPython virtual machine mechanics.
  - Verifying compiler optimizations (constant folding, opcode specialization).
- **Do Not Use:**
  - Frame introspection (`inspect.currentframe()`) in production hot paths (accessing stack frames breaks JIT optimizations and incurs high performance penalties).
  - `dis` in production business logic (CPython opcodes vary across minor Python versions).

---

## Core Concepts & Mental Model

```
+-------------------------------------------------------------------------+
| Python Source Code: def add(x, y=10): return x + y                      |
+-------------------------------------------------------------------------+
                                    |
                                    v [Compilation: AST -> Bytecode]
+-------------------------------------------------------------------------+
| Code Object (add.__code__)                                              |
| - co_code:     b'|\x00|\x01\x16\x00S\x00' (raw byte array)             |
| - co_varnames: ('x', 'y')                                               |
| - co_consts:   (None,)                                                  |
| - co_argcount: 2                                                        |
+-------------------------------------------------------------------------+
       |                                                 |
       v                                                 v
+-------------------------------+             +---------------------------+
| inspect.signature(add)        |             | dis.dis(add)              |
| - Parameter: x (POSITIONAL)   |             | 0 LOAD_FAST    0 (x)      |
| - Parameter: y (default=10)   |             | 2 LOAD_FAST    1 (y)      |
| - Return annotation           |             | 4 BINARY_OP    0 (+)      |
| - Sig.bind(*args, **kwargs)   |             | 8 RETURN_VALUE            |
+-------------------------------+             +---------------------------+
```

### Parameter Kinds in `inspect`
Python formalizes five parameter kinds (`inspect.Parameter.kind`):
1. `POSITIONAL_ONLY`: Parameters before `/` (e.g. `def f(a, /):`).
2. `POSITIONAL_OR_KEYWORD`: Standard parameters (e.g. `def f(a):`).
3. `VAR_POSITIONAL`: Excess positional arguments (e.g. `*args`).
4. `KEYWORD_ONLY`: Parameters after `*` or `*args` (e.g. `def f(*, key):`).
5. `VAR_KEYWORD`: Excess keyword arguments (e.g. `**kwargs`).

---

## How It Works Internally

### 1. Function Signatures and Argument Binding
When `inspect.signature(func)` runs:
- It inspects `func.__code__` (argument counts, variable names) and `func.__defaults__` / `func.__kwdefaults__`.
- It returns an immutable `Signature` object containing a mapping of `Parameter` instances.
- Calling `sig.bind(*args, **kwargs)` tests arguments against the signature without executing the function. If parameters are missing or unexpected, it raises `TypeError`.
- Calling `bound.apply_defaults()` fills in any missing parameters with their declared defaults!

### 2. CPython Bytecode and the Stack Evaluation Loop
CPython is a **stack-based virtual machine**.
When a code object is executed:
- The evaluation loop (`Python/ceval.c` or generated `generated_cases.c.h` in 3.12+) maintains a value stack.
- `LOAD_FAST 0 (x)`: Pushes the local variable at index 0 onto the top of the stack.
- `LOAD_FAST 1 (y)`: Pushes the local variable at index 1 onto the stack.
- `BINARY_OP 0 (+)`: Pops two items, executes addition in C, and pushes the result back onto the stack.
- `RETURN_VALUE`: Pops the top of the stack and returns it to the caller.

### 3. Specializing Adaptive Interpreter (PEP 659 in 3.11+)
In Python 3.11+, bytecode opcodes adapt dynamically at runtime:
- An opcode like `LOAD_ATTR` starts in a generic form.
- After running ~8 times, CPython's tier-1 inline cache "quickens" the instruction to a specialized variant, such as `LOAD_ATTR_SLOT` (for slotted attributes) or `LOAD_ATTR_MODULE` (for module attributes), completely skipping dictionary hash lookups!
- `dis.dis(..., adaptive=True)` shows these specialized opcodes in action.

---

## Syntax & API Reference

### `inspect` Module
```python
import inspect

# Signature Introspection
sig: inspect.Signature = inspect.signature(callable_obj)
params: dict[str, inspect.Parameter] = sig.parameters
bound: inspect.BoundArguments = sig.bind(*args, **kwargs)
bound.apply_defaults()
bound_dict: dict[str, Any] = bound.arguments

# Frame & Stack Inspection
frame = inspect.currentframe()           # Returns active frame object
stack: list[inspect.FrameInfo] = inspect.stack()
caller_frame = stack[1]                  # Frame of caller function

# Object Classification
inspect.isfunction(obj) -> bool
inspect.isclass(obj) -> bool
inspect.iscoroutinefunction(obj) -> bool
inspect.isgeneratorfunction(obj) -> bool

# Source Code
source_str: str = inspect.getsource(obj)
file_path: str = inspect.getfile(obj)
```

### `dis` Module
```python
import dis

# Disassemble to stdout
dis.dis(x=None, *, file=None, depth=None, show_caches=False, adaptive=False)

# Get instructions as an iterator
instructions = dis.get_instructions(x)
for instr in instructions:
    instr: dis.Instruction
    print(instr.opname, instr.argval, instr.starts_line)

# Disassemble code object directly
dis.disco(code_object)
```

---

## Code Examples

### Example 1: Function Signature Binding and Default Application
```python
import inspect

def create_server(host: str, port: int = 8080, *, secure: bool = False, workers: int = 4):
    pass

def run_signature_demo():
    sig = inspect.signature(create_server)
    print("Function Signature:", sig)
    print("Return Annotation: ", sig.return_annotation)

    print("\nParameters Breakdown:")
    for name, param in sig.parameters.items():
        print(f"  {name:8}: kind={param.kind.name:<22} default={param.default!r}")

    # Bind partial arguments (as passed by a caller)
    bound = sig.bind("127.0.0.1", secure=True)
    print("\nBound before apply_defaults():", bound.arguments)

    # Automatically populate missing parameters with their defined defaults!
    bound.apply_defaults()
    print("Bound after apply_defaults(): ", bound.arguments)

run_signature_demo()
# Output:
# Function Signature: (host: str, port: int = 8080, *, secure: bool = False, workers: int = 4)
# Return Annotation:  <class 'inspect._empty'>
# 
# Parameters Breakdown:
#   host    : kind=POSITIONAL_OR_KEYWORD  default=<class 'inspect._empty'>
#   port    : kind=POSITIONAL_OR_KEYWORD  default=8080
#   secure  : kind=KEYWORD_ONLY           default=False
#   workers : kind=KEYWORD_ONLY           default=4
# 
# Bound before apply_defaults(): {'host': '127.0.0.1', 'secure': True}
# Bound after apply_defaults():  {'host': '127.0.0.1', 'port': 8080, 'secure': True, 'workers': 4}
```

### Example 2: Call Stack Frame Introspection for Diagnostic Auditing
```python
import inspect

def audit_trail():
    # Inspect caller frame
    frame = inspect.currentframe().f_back
    try:
        caller_info = inspect.getframeinfo(frame)
        print("--- AUDIT LOG ---")
        print(f"  Caller File:     {caller_info.filename.split('/')[-1]}")
        print(f"  Caller Function: {caller_info.function}")
        print(f"  Caller Line:     {caller_info.lineno}")
        print(f"  Caller Locals:   {frame.f_locals}")
    finally:
        # Critical: Delete frame reference to prevent cyclic reference memory leaks
        del frame

def process_transaction(account_id: str, amount_cents: int):
    # Business logic
    is_valid = True
    audit_trail()

process_transaction("ACC_849204", 50000)
# Output:
# --- AUDIT LOG ---
#   Caller File:     ...
#   Caller Function: process_transaction
#   Caller Line:     ...
#   Caller Locals:   {'account_id': 'ACC_849204', 'amount_cents': 50000, 'is_valid': True}
```

### Example 3: Bytecode Disassembly with `dis.dis`
```python
import dis

def calculate_discount(price: float, rate: float) -> float:
    if rate > 0.0:
        return price * (1.0 - rate)
    return price

print("Bytecode Disassembly for calculate_discount:")
dis.dis(calculate_discount)
# Output (CPython 3.12+):
#   1           0 RESUME                   0
#   2           2 LOAD_FAST                1 (rate)
#               4 LOAD_CONST               1 (0.0)
#               6 COMPARE_OP              68 (>)
#              10 POP_JUMP_IF_FALSE        9 (to 30)
#   3          12 LOAD_FAST                0 (price)
#              14 LOAD_CONST               2 (1.0)
#              16 LOAD_FAST                1 (rate)
#              18 BINARY_OP                1 (-)
#              22 BINARY_OP                5 (*)
#              26 RETURN_VALUE
#   4     >>   30 LOAD_FAST                0 (price)
#              32 RETURN_VALUE
```

### Example 4: Comparing Bytecode: List Comprehension vs `list.append`
```python
import dis

def loop_append(items):
    out = []
    for x in items:
        out.append(x * 2)
    return out

def list_comp(items):
    return [x * 2 for x in items]

print("=== Loop with append() ===")
# Requires LOAD_ATTR (append) and CALL on EVERY iteration
dis.dis(loop_append)

print("\n=== List Comprehension ===")
# Uses specialized LIST_APPEND opcode (bypasses attribute lookup and call overhead!)
dis.dis(list_comp)
```

### Example 5: Detecting Generator and Coroutine Execution States
```python
import inspect
import asyncio

def sample_generator():
    yield 1
    yield 2

async def sample_coroutine():
    await asyncio.sleep(0.01)

def run_state_demo():
    # 1. Generator inspection
    gen = sample_generator()
    print("Gen state (unstarted):", inspect.getgeneratorstate(gen))
    next(gen)
    print("Gen state (suspended):", inspect.getgeneratorstate(gen))
    try:
        next(gen)
        next(gen)
    except StopIteration:
        pass
    print("Gen state (exhausted):", inspect.getgeneratorstate(gen))

    # 2. Coroutine inspection
    coro = sample_coroutine()
    print("\nCoro state (created):  ", inspect.getcoroutinestate(coro))
    coro.close()
    print("Coro state (closed):   ", inspect.getcoroutinestate(coro))

run_state_demo()
# Output:
# Gen state (unstarted): GEN_CREATED
# Gen state (suspended): GEN_SUSPENDED
# Gen state (exhausted): GEN_CLOSED
# 
# Coro state (created):   CORO_CREATED
# Coro state (closed):    CORO_CLOSED
```

---

## Edge Cases, Pitfalls & Anti-Patterns

### 1. Frame Cycle Memory Leaks
When you call `frame = inspect.currentframe()`, the frame object holds references to local variables in `frame.f_locals`. If any local variable references the frame or a function containing the frame, a **reference cycle** is created. Until the garbage collector runs, all local variables (including large datasets or database connections) remain locked in memory!
- **Rule**: Always wrap frame inspection in `try/finally` and explicitly call `del frame`.

### 2. Assuming Bytecode Stability Across Python Versions
CPython bytecode is an internal implementation detail, not a public specification.
- Python 3.11 completely replaced `CALL_FUNCTION` with `CALL` and introduced inline cache entries.
- Python 3.12 eliminated `PRECALL` and merged comparison instructions.
Never write production business logic that branches on specific numerical opcodes.

### 3. Missing `follow_wrapped=True` in Decorated Functions
When introspecting a decorated function (`@my_decorator`), `inspect.signature(func)` by default follows the `__wrapped__` attribute chain set by `functools.wraps`. If a decorator forgot to use `@functools.wraps`, `inspect.signature` returns `(*args, **kwargs)` of the wrapper instead of the genuine target function!

### Common Mistakes Table

| Bad Code | Good Code | Why | Severity |
| :--- | :--- | :--- | :--- |
| `frame = inspect.currentframe()` without `del frame` | `try: ... finally: del frame` | Retaining frame references creates reference cycles that delay GC. | High |
| Hand-rolling argument validation with `len(args)` | `sig.bind(*args, **kwargs)` | Manual validation fails on keyword-only, defaults, and variable kwargs. | High |
| Parsing `func.__code__.co_varnames` directly | Use `inspect.signature(func)` | `co_varnames` includes local variables declared inside function body! | High |
| Writing production logic checking opcode numbers | Inspect AST or use standard reflection | Opcodes change numerical values between minor Python releases. | Critical |
| `inspect.getsource(x)` in frozen binaries (PyInstaller) | Check `inspect.findsource()` or handle OSError | Source files are not bundled in compiled frozen executable packages. | Medium |
| Using `inspect.stack()` in tight loops (e.g. 100k/s) | Avoid frame walking in hot paths | `inspect.stack()` reconstructs entire OS stack frames; ~50x slower. | High |

---

## Performance & Memory Characteristics

### Computational Latency
- `inspect.signature(func)`: $\approx 8.5 \mu\text{s}$ (creates Signature, Parameter objects, and dicts).
  - *Recommendation*: Cache signatures in a dictionary (`lru_cache`) if called repeatedly in request pipelines.
- `sig.bind(*args, **kwargs)`: $\approx 1.2 \mu\text{s}$ (validates and maps arguments).
- `inspect.currentframe()`: $\approx 120 \text{ ns}$ (C-level thread state pointer read).
- `inspect.stack()`: $\approx 45 \mu\text{s}$ (walks entire call chain and reads line sources from disk).

---

## CPython Internals & Bytecode

In CPython:
- Code objects (`PyCodeObject`) are defined in `Include/cpython/code.h`.
- Key struct members:
  - `co_code`: Bytes object containing raw opcode bytes and argument numbers.
  - `co_consts`: Tuple of literal constants.
  - `co_names`: Tuple of global attribute and variable names.
  - `co_varnames`: Tuple of local parameter and variable names.
  - `co_stacksize`: Computed maximum evaluation stack depth.
- In Python 3.12+, instruction words are 16 bits (2 bytes): 1 byte opcode, 1 byte argument. Instructions needing larger arguments are prefixed with the `EXTENDED_ARG` opcode.

---

## Best Practices & Idiomatic Patterns

1. **Cache `inspect.signature` in Web Frameworks**:
   Always cache signatures at startup/registration time; never inspect signatures dynamically on every single HTTP request.
2. **Always Use `bound.apply_defaults()`**:
   Ensures your dependency injector sees complete parameters even when callers omit default values.
3. **Use `inspect.iscoroutinefunction()` for Async Dispatch**:
   ```python
   if inspect.iscoroutinefunction(handler):
       await handler()
   else:
       handler()
   ```
4. **Use `dis.dis()` to Verify Optimizations**:
   Confirm whether Python compiler optimizations (e.g. constant folding `24 * 60 * 60` into `86400`) have occurred.

---

## Real-World Case Study: Production Dependency Injection and RPC Framework

The following lightweight micro-framework uses `inspect.signature` and type annotations to implement a FastAPI-style dependency injection and remote procedure call (RPC) dispatcher.

```python
import inspect
from typing import Callable, Dict, Any, get_type_hints

class DependencyContainer:
    def __init__(self):
        self._services: Dict[type, Any] = {}
        self._routes: Dict[str, Callable] = {}
        self._signatures: Dict[str, inspect.Signature] = {}

    def register_service(self, service_type: type, instance: Any) -> None:
        self._services[service_type] = instance

    def register_route(self, route_name: str, handler: Callable) -> None:
        self._routes[route_name] = handler
        # Pre-cache signature at startup time to avoid per-request inspection overhead
        self._signatures[route_name] = inspect.signature(handler)

    def dispatch(self, route_name: str, client_params: Dict[str, Any]) -> Any:
        if route_name not in self._routes:
            raise KeyError(f"Route '{route_name}' not found")

        handler = self._routes[route_name]
        sig = self._signatures[route_name]

        # Resolve dependencies by inspecting parameter annotations
        resolved_args = {}
        for param_name, param in sig.parameters.items():
            # Check if parameter type is an injected service
            if param.annotation in self._services:
                resolved_args[param_name] = self._services[param.annotation]
            elif param_name in client_params:
                # Type coerce if needed
                val = client_params[param_name]
                if param.annotation != inspect.Parameter.empty and not isinstance(val, param.annotation):
                    val = param.annotation(val)
                resolved_args[param_name] = val

        # Validate arguments against signature
        bound = sig.bind(**resolved_args)
        bound.apply_defaults()

        return handler(*bound.args, **bound.kwargs)

# Verification
class DatabaseConnection:
    def query(self, sql: str) -> str:
        return f"Query executed: {sql}"

def get_user_profile(user_id: int, db: DatabaseConnection, verbose: bool = False):
    result = db.query(f"SELECT * FROM users WHERE id = {user_id}")
    return {"user_id": user_id, "data": result, "verbose": verbose}

if __name__ == "__main__":
    container = DependencyContainer()
    container.register_service(DatabaseConnection, DatabaseConnection())
    container.register_route("user.get", get_user_profile)

    # Dispatch request: 'db' is automatically injected from container!
    response = container.dispatch("user.get", {"user_id": "42", "verbose": True})
    print("Dispatched RPC Response:\n ", response)
    assert response["user_id"] == 42
    assert "Query executed" in response["data"]
```

---

## Interview Questions & Deep-Dive Scenarios

<details>
<summary><strong>1. Junior: What is the difference between <code>func.__code__.co_varnames</code> and <code>inspect.signature(func).parameters</code>?</strong></summary>

`func.__code__.co_varnames` is a raw tuple containing **all** local variable names used anywhere inside the function body, including parameters, loop counters, and temporary variables. `inspect.signature(func).parameters` filters out internal variables and returns an ordered dictionary containing strictly the function's formal input parameters with their kinds, default values, and type annotations.
</details>

<details>
<summary><strong>2. Junior: What does <code>dis.dis()</code> do?</strong></summary>

`dis.dis()` (disassemble) analyzes a Python function, method, class, code object, or code string and prints a human-readable representation of the underlying CPython virtual machine bytecode instructions (such as `LOAD_FAST`, `BINARY_OP`, and `RETURN_VALUE`).
</details>

<details>
<summary><strong>3. Mid: What is the purpose of <code>Signature.bind(*args, **kwargs)</code>?</strong></summary>

`Signature.bind()` simulates CPython's argument binding rules: it checks whether the provided positional and keyword arguments correctly satisfy the function's parameter expectations without executing the function. If required arguments are missing, unexpected keywords are passed, or positional-only rules are violated, it raises `TypeError`.
</details>

<details>
<summary><strong>4. Mid: What is a frame reference cycle, and why does inspect recommend deleting frame objects?</strong></summary>

An execution frame object (`inspect.currentframe()`) contains a reference to the active local variables dictionary (`f_locals`). If a local variable inside that scope holds a reference to the frame (directly or indirectly), a cyclic reference is formed. Because frames are complex interpreter structures, these cycles delay object deallocation until the cyclical garbage collector runs. Wrapping frame inspection in `try/finally` with `del frame` breaks the cycle immediately.
</details>

<details>
<summary><strong>5. Senior: What are the five parameter kinds defined in <code>inspect.Parameter.kind</code>, and how do they map to Python syntax?</strong></summary>

1. `POSITIONAL_ONLY`: Parameters before `/` (e.g. `def f(a, /)`). Must be passed positionally.
2. `POSITIONAL_OR_KEYWORD`: Normal parameters without `/` or `*` (e.g. `def f(a)`).
3. `VAR_POSITIONAL`: Excess positional parameters (e.g. `*args`).
4. `KEYWORD_ONLY`: Parameters declared after `*` or `*args` (e.g. `def f(*, key)`). Must be passed by keyword.
5. `VAR_KEYWORD`: Excess keyword parameters (e.g. `**kwargs`).
</details>

<details>
<summary><strong>6. Senior: How does Python 3.11's Specializing Adaptive Interpreter (PEP 659) change bytecode instructions at runtime?</strong></summary>

In Python 3.11+, generic bytecode opcodes (like `LOAD_ATTR` or `BINARY_SUBSCR`) are observed at runtime. If an instruction executes repeatedly with identical types, CPython "quickens" the opcode into a specialized variant (e.g. `LOAD_ATTR_SLOT`, `LOAD_ATTR_INSTANCE_VALUE`, `BINARY_SUBSCR_LIST_INT`). These specialized opcodes embed inline cache entries directly in the bytecode stream, bypassing generic type checks and hash lookups for 10%–25% faster execution.
</details>

<details>
<summary><strong>7. Staff: How does <code>inspect.getsource()</code> locate source code, and why can it fail in production container deployments?</strong></summary>

`inspect.getsource(obj)` looks up `obj.__code__.co_filename` and opens the physical source `.py` file from disk using Python's `linecache` module. It parses the file's AST to locate the code block matching the object.
Failure modes in production:
1. Stripped source images: Production Docker images running compiled `.pyc` files without accompanying `.py` source files raise `OSError: could not get source code`.
2. Dynamically evaluated code (`exec()` or `eval()`): Does not have an associated physical file on disk.
</details>

<details>
<summary><strong>8. Staff: How would you design a hot-patching profiler that instruments third-party library functions at runtime without modifying their source code?</strong></summary>

1. Use `inspect.getmodule()` and `getattr()` to obtain the target function pointer.
2. Create a dynamic wrapper using `inspect.signature` to accurately preserve parameter names and annotations (`@functools.wraps`).
3. Inside the wrapper, read high-resolution timers (`time.perf_counter_ns()`) before and after executing the original function.
4. Atomically swap the module's function attribute (`setattr(module, func_name, wrapper)`).
5. Ensure safety by restoring the original function pointer in a cleanup / teardown hook.
</details>

---

## Practical Exercises & Code Katas

### Exercise 1: Mandatory Keyword-Only Enforcer
**Requirement:** Write a function `enforce_keyword_only(func: Callable)` that inspects a function's signature and raises `ValueError` if any of its parameters are NOT keyword-only.

<details>
<summary>Show Solution</summary>

```python
import inspect
from typing import Callable

def enforce_keyword_only(func: Callable) -> None:
    sig = inspect.signature(func)
    for name, param in sig.parameters.items():
        if param.kind not in (inspect.Parameter.KEYWORD_ONLY, inspect.Parameter.VAR_KEYWORD):
            raise ValueError(f"Parameter '{name}' in '{func.__name__}' is not keyword-only!")

# Test
def bad_func(a, *, b=1): pass
def good_func(*, a=1, b=2): pass

enforce_keyword_only(good_func)
try:
    enforce_keyword_only(bad_func)
    assert False, "Should have raised ValueError"
except ValueError as e:
    print("Caught expected validation error:", e)
```
</details>

### Exercise 2: Opcode Frequency Analyzer
**Requirement:** Write a function `analyze_opcodes(func: Callable) -> dict[str, int]` using `dis.get_instructions` that counts the frequency of every bytecode opcode in a function.

<details>
<summary>Show Solution</summary>

```python
import dis
from collections import Counter
from typing import Callable

def analyze_opcodes(func: Callable) -> dict[str, int]:
    counts = Counter()
    for instr in dis.get_instructions(func):
        counts[instr.opname] += 1
    return dict(counts)

def sample_math(x, y):
    total = 0
    for i in range(x):
        total += i * y
    return total

# Test
freq = analyze_opcodes(sample_math)
print("Opcode Frequencies:", freq)
assert "FOR_ITER" in freq
assert "LOAD_FAST" in freq
```
</details>

### Exercise 3: Automatic Argument Validator Decorator
**Requirement:** Implement a decorator `@validate_types` that uses `inspect.signature` and `sig.bind` to enforce that passed argument types match their type annotations at runtime.

<details>
<summary>Show Solution</summary>

```python
import inspect
import functools

def validate_types(func):
    sig = inspect.signature(func)

    @functools.wraps(func)
    def wrapper(*args, **kwargs):
        bound = sig.bind(*args, **kwargs)
        bound.apply_defaults()
        for name, val in bound.arguments.items():
            expected = sig.parameters[name].annotation
            if expected != inspect.Parameter.empty and not isinstance(val, expected):
                raise TypeError(f"Argument '{name}' must be of type {expected.__name__}; got {type(val).__name__}")
        return func(*args, **kwargs)
    return wrapper

# Test
@validate_types
def calculate_area(width: int, height: int) -> int:
    return width * height

assert calculate_area(10, 5) == 50
try:
    calculate_area("10", 5)
except TypeError as e:
    print("Validation caught error:", e)
```
</details>

### Exercise 4: Caller Function Name Extractor
**Requirement:** Write a helper `get_caller_name() -> str` that returns the name of the function that called the current function using `inspect.currentframe()`.

<details>
<summary>Show Solution</summary>

```python
import inspect

def get_caller_name() -> str:
    frame = inspect.currentframe().f_back.f_back
    try:
        return frame.f_code.co_name
    finally:
        del frame

def caller_alpha():
    return get_caller_name()

def caller_beta():
    return get_caller_name()

print("Caller Alpha:", caller_alpha())
print("Caller Beta: ", caller_beta())
assert caller_alpha() == "caller_alpha"
assert caller_beta() == "caller_beta"
```
</details>

### Exercise 5: Constant Folding Verifier
**Requirement:** Write a function `has_folded_constants(func: Callable) -> bool` that inspects `func.__code__.co_consts` to verify whether static arithmetic expressions like `60 * 60` were pre-calculated by the compiler.

<details>
<summary>Show Solution</summary>

```python
def check_seconds():
    # 60 * 60 * 24 should fold into 86400
    return 60 * 60 * 24

def has_folded_constants(func, expected_constant) -> bool:
    return expected_constant in func.__code__.co_consts

print("Is 86400 in co_consts:", has_folded_constants(check_seconds, 86400))
assert has_folded_constants(check_seconds, 86400) is True
```
</details>

### Exercise 6: Dependency Injector Resolver
**Requirement:** Write a function `resolve_and_call(func, service_registry: dict)` that uses `inspect.signature` to match parameter names against `service_registry` keys and invoke the function.

<details>
<summary>Show Solution</summary>

```python
import inspect

def resolve_and_call(func, service_registry: dict):
    sig = inspect.signature(func)
    kwargs = {}
    for param_name in sig.parameters:
        if param_name in service_registry:
            kwargs[param_name] = service_registry[param_name]
        else:
            raise KeyError(f"Missing dependency: {param_name}")
    return func(**kwargs)

# Test
services = {
    "db_host": "10.0.0.1",
    "port": 5432,
    "unused_service": "ignore"
}

def connect_db(db_host: str, port: int):
    return f"Connected to {db_host}:{port}"

res = resolve_and_call(connect_db, services)
print("Resolved call result:", res)
assert res == "Connected to 10.0.0.1:5432"
```
</details>
