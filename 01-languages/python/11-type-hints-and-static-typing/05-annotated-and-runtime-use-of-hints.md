---
title: "Annotated and Runtime Introspection of Type Hints"
description: "Mastering typing.Annotated (PEP 593) and runtime type reflection: get_type_hints, get_origin, get_args, runtime metadata extraction, and framework integration in FastAPI, Pydantic, and Typer."
module: "11-type-hints-and-static-typing"
order: 5
tags:
  - python
  - typing
  - annotated
  - pep-593
  - runtime-reflection
  - get-type-hints
  - metadata
difficulty: "Advanced"
cpython_version: "3.12+"
---

# Annotated and Runtime Introspection of Type Hints

Python type hints were originally conceived strictly for static analysis tools like Mypy and IDEs (PEP 484). However, modern Python frameworks—most notably **FastAPI**, **Pydantic v2**, and **Typer**—rely heavily on **runtime introspection** of type hints to drive data validation, serialization, dependency injection, and automatic OpenAPI schema generation.

With **`typing.Annotated`** (introduced in Python 3.9 via PEP 593), Python standardized how arbitrary runtime metadata can be attached to types without conflicting with static type checkers.

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
10. [Real-World Case Study: Declarative HTTP Request Validator and Dependency Injector](#real-world-case-study-declarative-http-request-validator-and-dependency-injector)
11. [Interview Questions & Deep-Dive Scenarios](#interview-questions--deep-dive-scenarios)
12. [Practical Exercises & Code Katas](#practical-exercises--code-katas)

---

## What & Why

Before `Annotated`, attaching metadata to types required awkward workarounds:
- Subclassing primitive types or creating complex wrapper types that confused static analyzers.
- Using docstring parsing or ad-hoc custom decorators.
- Polluting function signatures with framework-specific sentinel default values (`x: int = Query(...)`), which broke default argument evaluation and code reusability.

`typing.Annotated[T, ...]` cleanly separates concerns:
1. **To the Static Type Checker (Mypy/Pyright)**: `Annotated[T, M1, M2]` is treated identically to `T`. The metadata elements `M1, M2` are completely ignored during static verification.
2. **To Runtime Frameworks (Pydantic/FastAPI)**: The metadata elements are preserved and introspected at runtime to apply validation rules, serialization formats, or dependency injection instructions.

---

## Core Concepts & Mental Model

### Static vs Runtime Treatment of Annotated

```mermaid
graph TD
    A["Annotated[int, ValueRange(0, 100), Unit('celsius')]"]
    
    subgraph Static Analysis View (Mypy / Pyright / IDE)
        A -->|Unwraps to Base Type| B["int"]
        B --> C["Type verification: 42 is valid int"]
    end
    
    subgraph Runtime Reflection View (Frameworks / FastAPI)
        A -->|typing.get_args()| D["Extract Base Type: int"]
        A -->|Extract Metadata Tuple| E["(ValueRange(0, 100), Unit('celsius'))"]
        E --> F["Runtime Validator enforces 0 <= val <= 100"]
    end
```

### Reflection Toolset: `get_origin`, `get_args`, `get_type_hints`

When introspecting types at runtime, three foundational functions in the `typing` module form the bedrock of reflection:

| Function | Input | Return Value | Purpose |
|:---|:---|:---|:---|
| `get_origin(tp)` | `list[str]` | `list` | Extracts the unsubscripted generic origin type or special form. |
| `get_origin(tp)` | `Annotated[int, ...]` | `typing.Annotated` | Identifies whether a type is wrapped with `Annotated`. |
| `get_args(tp)` | `list[str]` | `(str,)` | Extracts generic type parameters as a tuple. |
| `get_args(tp)` | `Annotated[int, "meta"]` | `(int, "meta")` | Extracts the base type followed by all metadata objects. |
| `get_type_hints(fn)` | `def f(x: int):` | `{'x': <class 'int'>}` | Resolves forward references and returns a clean dictionary of evaluated annotations. |

---

## How It Works Internally

### The `include_extras` Parameter

By default, calling `typing.get_type_hints(obj)` strips `Annotated` wrappers and returns only the underlying base type:

```python
from typing import Annotated, get_type_hints

def process(score: Annotated[int, "must be positive"]) -> None:
    pass

# Default behavior: strips metadata
hints_default = get_type_hints(process)
# Result: {'score': <class 'int'>}

# Explicitly including extras:
hints_extras = get_type_hints(process, include_extras=True)
# Result: {'score': typing.Annotated[int, 'must be positive']}
```

Frameworks like FastAPI and Pydantic pass `include_extras=True` to inspect both the static type and runtime metadata simultaneously.

---

## Syntax & API Reference

### 1. Declaring Annotated Types

```python
from typing import Annotated

# Primitive with validation metadata
type PortNumber = Annotated[int, "TCP/UDP port number 1-65535"]

# Reusable typed constraint objects
class MinLength:
    def __init__(self, length: int) -> None:
        self.length = length

type Username = Annotated[str, MinLength(3)]
```

### 2. Introspecting Metadata

```python
from typing import get_origin, get_args, Annotated

T = Annotated[int, "gt=0", "le=100"]

# Check if type is Annotated
if get_origin(T) is Annotated:
    args = get_args(T)
    base_type = args[0]          # int
    metadata = args[1:]          # ("gt=0", "le=100")
```

---

## Code Examples

### Example 1: Basic Metadata Extraction and Inspection

```python
from typing import Annotated, get_origin, get_args, get_type_hints

class Precision:
    def __init__(self, decimals: int) -> None:
        self.decimals = decimals

    def __repr__(self) -> str:
        return f"Precision({self.decimals})"

def format_currency(amount: Annotated[float, Precision(2), "USD"]) -> str:
    return f"${amount:.2f}"

hints = get_type_hints(format_currency, include_extras=True)
arg_type = hints["amount"]

print("Origin:", get_origin(arg_type))
print("Raw Args:", get_args(arg_type))
base_type, *metadata = get_args(arg_type)
print(f"Base type: {base_type.__name__}, Metadata: {metadata}")
# Output:
# Origin: <class 'typing.Annotated'>
# Raw Args: (<class 'float'>, Precision(2), 'USD')
# Base type: float, Metadata: [Precision(2), 'USD']
```

### Example 2: Building a Lightweight Runtime Field Validator

```python
from typing import Annotated, get_type_hints, get_origin, get_args
from dataclasses import dataclass

@dataclass(frozen=True)
class Gt:
    threshold: float
    def validate(self, val: float) -> bool:
        return val > self.threshold

@dataclass(frozen=True)
class NotEmpty:
    def validate(self, val: str) -> bool:
        return len(val.strip()) > 0

class UserRegistration:
    username: Annotated[str, NotEmpty()]
    age: Annotated[int, Gt(18)]

    def __init__(self, username: str, age: int) -> None:
        self.username = username
        self.age = age
        self.validate()

    def validate(self) -> None:
        hints = get_type_hints(self.__class__, include_extras=True)
        for field_name, field_type in hints.items():
            if get_origin(field_type) is Annotated:
                val = getattr(self, field_name)
                for meta in get_args(field_type)[1:]:
                    if hasattr(meta, "validate") and not meta.validate(val):
                        raise ValueError(f"Field '{field_name}' with value {val!r} failed validation constraint {meta}")

# Valid construction
u = UserRegistration("alice", 24)
print(f"Registered: {u.username}, age {u.age}")

# Invalid construction:
try:
    UserRegistration("bob", 16)
except ValueError as exc:
    print("Caught error:", exc)
# Output:
# Registered: alice, age 24
# Caught error: Field 'age' with value 16 failed validation constraint Gt(threshold=18)
```

### Example 3: Forward Reference Resolution with `get_type_hints`

```python
from __future__ import annotations
from typing import get_type_hints

class TreeNode:
    value: int
    left: TreeNode | None
    right: TreeNode | None

    def __init__(self, value: int) -> None:
        self.value = value
        self.left = None
        self.right = None

# Under 'from __future__ import annotations', __annotations__ stores strings:
print("Raw annotations dict:", TreeNode.__annotations__["left"])

# get_type_hints resolves string representations into real Python types:
resolved_hints = get_type_hints(TreeNode)
print("Resolved left hint:", resolved_hints["left"])
# Output:
# Raw annotations dict: TreeNode | None
# Resolved left hint: TreeNode | None
```

### Example 4: Dependency Injection via `Annotated` (FastAPI-Style)

```python
from typing import Annotated, Callable, get_type_hints, get_origin, get_args

class Depends:
    def __init__(self, dependency: Callable[[], object]) -> None:
        self.dependency = dependency

# Reusable dependencies
def get_db_connection() -> str:
    return "DatabaseConnection(postgresql://master:5432)"

def get_auth_token() -> str:
    return "Bearer eyJhbGciOi..."

type DB = Annotated[str, Depends(get_db_connection)]
type Auth = Annotated[str, Depends(get_auth_token)]

def handle_request(db: DB, token: Auth, query: str) -> None:
    print(f"Executing '{query}' using {db} with token {token[:12]}...")

def invoke_with_dependencies(endpoint: Callable[..., None], **caller_kwargs: object) -> None:
    hints = get_type_hints(endpoint, include_extras=True)
    resolved_args: dict[str, object] = dict(caller_kwargs)

    for param_name, param_type in hints.items():
        if param_name == "return":
            continue
        if get_origin(param_type) is Annotated:
            for meta in get_args(param_type)[1:]:
                if isinstance(meta, Depends):
                    resolved_args[param_name] = meta.dependency()

    endpoint(**resolved_args)

invoke_with_dependencies(handle_request, query="SELECT * FROM users")
# Output:
# Executing 'SELECT * FROM users' using DatabaseConnection(postgresql://master:5432) with token Bearer eyJhb...
```

### Example 5: Introspecting Nested Generics (`list[dict[str, int]]`)

```python
from typing import get_origin, get_args

def analyze_type(tp: object, depth: int = 0) -> None:
    indent = "  " * depth
    origin = get_origin(tp)
    args = get_args(tp)
    print(f"{indent}Type: {tp} | Origin: {origin} | Arguments: {args}")
    for arg in args:
        analyze_type(arg, depth + 1)

type ComplexType = dict[str, list[int | None]]
print("Analyzing ComplexType:")
analyze_type(ComplexType)
# Output:
# Analyzing ComplexType:
# Type: dict[str, list[int | None]] | Origin: <class 'dict'> | Arguments: (<class 'str'>, list[int | None])
#   Type: <class 'str'> | Origin: None | Arguments: ()
#   Type: list[int | None] | Origin: <class 'list'> | Arguments: (int | None,)
#     Type: int | None | Origin: <class 'types.UnionType'> | Arguments: (<class 'int'>, <class 'NoneType'>)
#       Type: <class 'int'> | Origin: None | Arguments: ()
#       Type: <class 'NoneType'> | Origin: None | Arguments: ()
```

---

## Edge Cases, Pitfalls & Anti-Patterns

### Common Mistakes Table

| Mistake | Bad Code | Correct Idiom | Why It Fails |
|:---|:---|:---|:---|
| **Reading `__annotations__` directly** | `cls.__annotations__['x']` | `get_type_hints(cls, include_extras=True)['x']` | Direct read returns strings under PEP 563 and misses inherited base class annotations. |
| **Omitting `include_extras=True`** | `get_type_hints(f)` when reading metadata | `get_type_hints(f, include_extras=True)` | `get_type_hints()` strips `Annotated` wrappers by default. |
| **Nesting `Annotated` redundantly** | `Annotated[Annotated[int, M1], M2]` | `Annotated[int, M1, M2]` | Flattened metadata list is cleaner and avoids deep recursive inspection loops. |
| **Failing to pass namespaces for local types** | `get_type_hints(func)` defined inside a nested scope | `get_type_hints(func, localns=locals())` | Forward references to locally defined classes fail with `NameError`. |
| **Expecting `isinstance(x, Annotated[int, ...])`** | `isinstance(42, Annotated[int, ...])` | `isinstance(42, int)` | Parameterized special typing forms cannot be passed to `isinstance()`. |
| **Mutating metadata objects at runtime** | Using mutable lists or dicts inside `Annotated[T, [1, 2]]` | Use immutable frozen dataclasses or tuples | Shared mutable metadata can cause cross-field leakage. |

---

## Performance & Memory Characteristics

### Runtime Overhead of Type Reflection

1. **Static Analysis**: Identical to unannotated code.
2. **Import-Time Cost**: Evaluating `Annotated[T, ...]` allocates a lightweight generic alias object in memory.
3. **`get_type_hints()` Execution Speed**: Calling `get_type_hints()` traverses the class MRO, parses string annotations via `eval()` if deferred, and constructs a new dictionary. It takes ~5µs to 25µs per function.
   - **Optimization Rule**: High-performance frameworks (FastAPI, Pydantic) execute `get_type_hints()` **once** at application boot/registration time and cache the parsed schema in memory.

---

## CPython Internals & Bytecode

Let us inspect the internal structure of an `Annotated` object in CPython:

```python
from typing import Annotated

T = Annotated[int, "min=10", {"role": "admin"}]

print("Type of T:", type(T))
print("__origin__:", T.__origin__)
print("__metadata__:", T.__metadata__)
```

Output:
```text
Type of T: <class 'typing._AnnotatedAlias'>
__origin__: <class 'int'>
__metadata__: ('min=10', {'role': 'admin'})
```

CPython encapsulates `Annotated` instances in `typing._AnnotatedAlias`. The underlying type is exposed via `__origin__`, and the sequence of metadata elements is stored as a tuple in `__metadata__`.

---

## Best Practices & Idiomatic Patterns

1. **Wrap Metadata in Dedicated Frozen Dataclasses**: Never use raw strings like `Annotated[int, "positive"]`; create typed structs like `Annotated[int, Positive()]`.
2. **Always Use `get_type_hints(..., include_extras=True)`**: Avoid accessing `__annotations__` directly.
3. **Define Type Aliases for Common Constraints**:
   ```python
   PositiveInt = Annotated[int, Gt(0)]
   EmailStr = Annotated[str, Regex(r"^[\w\.-]+@[\w\.-]+\.\w+$")]
   ```
4. **Cache Introspection Results**: Reflection is fast enough for initialization, but should never be executed on hot per-request paths.

---

## Real-World Case Study: Declarative HTTP Request Validator and Dependency Injector

Below is an enterprise API route handler framework that automatically unpacks query parameters, validates path arguments, and injects authentication credentials:

```python
from typing import Annotated, Callable, get_type_hints, get_origin, get_args
from dataclasses import dataclass
import json

@dataclass(frozen=True)
class QueryParam:
    name: str
    default: str | None = None

@dataclass(frozen=True)
class PathParam:
    name: str

@dataclass(frozen=True)
class SecurityToken:
    header: str

class APIController:
    """Minimal declarative HTTP router using Annotated reflection."""

    def __init__(self) -> None:
        self._routes: dict[str, Callable[..., dict[str, object]]] = {}

    def get(self, path: str):
        def decorator(fn: Callable[..., dict[str, object]]):
            self._routes[path] = fn
            return fn
        return decorator

    def dispatch(self, path: str, query_params: dict[str, str], headers: dict[str, str]) -> str:
        handler = self._routes.get(path)
        if not handler:
            return json.dumps({"error": "Route Not Found", "status": 404})

        hints = get_type_hints(handler, include_extras=True)
        call_kwargs: dict[str, object] = {}

        for param_name, param_type in hints.items():
            if param_name == "return":
                continue
            if get_origin(param_type) is Annotated:
                base_type, *metadata = get_args(param_type)
                for meta in metadata:
                    if isinstance(meta, QueryParam):
                        val = query_params.get(meta.name, meta.default)
                        call_kwargs[param_name] = base_type(val) if val is not None else None
                    elif isinstance(meta, SecurityToken):
                        token = headers.get(meta.header)
                        if not token or not token.startswith("Bearer "):
                            return json.dumps({"error": "Unauthorized", "status": 401})
                        call_kwargs[param_name] = token.split(" ")[1]

        response = handler(**call_kwargs)
        return json.dumps(response)

# Application routes
app = APIController()

type AuthUser = Annotated[str, SecurityToken(header="Authorization")]
type PageLimit = Annotated[int, QueryParam(name="limit", default="20")]

@app.get("/api/v1/orders")
def get_orders(user: AuthUser, limit: PageLimit) -> dict[str, object]:
    return {
        "status": 200,
        "authenticated_user": user,
        "results": [f"Order #{i}" for i in range(1, limit + 1)],
    }

# Simulating valid HTTP request
resp = app.dispatch(
    path="/api/v1/orders",
    query_params={"limit": "3"},
    headers={"Authorization": "Bearer token_secret_994"}
)
print("Successful Dispatch Response:")
print(resp)

# Simulating unauthenticated HTTP request
unauth_resp = app.dispatch(
    path="/api/v1/orders",
    query_params={},
    headers={}
)
print("\nUnauthorized Dispatch Response:")
print(unauth_resp)
# Output:
# Successful Dispatch Response:
# {"status": 200, "authenticated_user": "token_secret_994", "results": ["Order #1", "Order #2", "Order #3"]}
#
# Unauthorized Dispatch Response:
# {"error": "Unauthorized", "status": 401}
```

---

## Interview Questions & Deep-Dive Scenarios

<details>
<summary>1. Why should you use `typing.get_type_hints` instead of reading `fn.__annotations__` directly?</summary>

Reading `__annotations__` directly presents several major pitfalls:
1. **Deferred Annotations (PEP 563)**: When `from __future__ import annotations` is active, `__annotations__` contains raw strings instead of resolved type objects.
2. **Inheritance Resolution**: It fails to resolve inherited annotations from base classes in a class hierarchy.
3. **Forward References**: It does not evaluate forward references against the proper global and local namespace scopes.
`typing.get_type_hints()` transparently parses strings into real types, handles inheritance, and resolves namespaces.
</details>

<details>
<summary>2. What is the difference between `get_origin(tp)` and `type(tp)`?</summary>

- `type(tp)` returns the metaclass or underlying Python implementation class (e.g., `typing._GenericAlias` or `types.UnionType`).
- `get_origin(tp)` returns the semantic unsubscripted type or typing construct (e.g., `get_origin(list[int])` returns `list`, and `get_origin(Union[int, str])` returns `typing.Union`).
Frameworks should always use `get_origin` to inspect parameterized types.
</details>

<details>
<summary>3. Can `Annotated` have multiple metadata arguments, and how are they ordered?</summary>

Yes. `Annotated[T, M1, M2, M3]` accepts an arbitrary number of metadata objects. They are preserved in `get_args(tp)[1:]` in the exact positional order in which they were declared.
</details>

<details>
<summary>4. What happens if you nest `Annotated` types: `Annotated[Annotated[int, A], B]`?</summary>

CPython automatically flattens nested `Annotated` definitions. The resulting type is identical to `Annotated[int, A, B]`. The underlying base type is preserved, and the metadata lists are concatenated in left-to-right order.
</details>

<details>
<summary>5. How does Python 3.14 (PEP 649) impact `get_type_hints` and annotations?</summary>

PEP 649 replaces PEP 563 stringification with deferred evaluation functions (`__annotate__`). Instead of storing strings, annotations are wrapped in a compiler-generated code block executed lazily only when `__annotations__` or `annotationlib.get_annotations()` is requested.
</details>

<details>
<summary>6. How do static type checkers handle `Annotated[T, Metadata]`?</summary>

Static type checkers (Mypy, Pyright) completely ignore `Metadata`. To the static type checker, `Annotated[T, ...]` is treated exactly as `T`. Any type compatibility check applied to `Annotated[T, ...]` is evaluated directly against `T`.
</details>

<details>
<summary>7. How can you distinguish between `int | None` and `Annotated[int | None, ...]`?</summary>

Call `get_origin(tp)`:
- For `Annotated[int | None, ...]`, `get_origin(tp)` returns `typing.Annotated`.
- For `int | None`, `get_origin(tp)` returns `types.UnionType` (in Python 3.10+).
</details>

<details>
<summary>8. Why is it dangerous to use mutable objects as metadata inside `Annotated`?</summary>

Because type aliases and function signatures are evaluated once at module import time, any mutable object placed in `Annotated` metadata becomes a shared singleton. If one part of the application mutates it, all other functions referencing that type alias will observe the mutation, introducing elusive cross-contamination bugs.
</details>

---

## Practical Exercises & Code Katas

### Exercise 1: Extract Metadata Helper
Write a utility function `extract_metadata[T](tp: object, meta_type: type[T]) -> T | None` that searches through an `Annotated` type and returns the first metadata instance matching `meta_type`.

<details>
<summary>Solution</summary>

```python
from typing import Annotated, get_origin, get_args

def extract_metadata[T](tp: object, meta_type: type[T]) -> T | None:
    if get_origin(tp) is Annotated:
        for item in get_args(tp)[1:]:
            if isinstance(item, meta_type):
                return item
    return None

class MinVal:
    def __init__(self, val: int) -> None: self.val = val

type Count = Annotated[int, "description", MinVal(10)]
found = extract_metadata(Count, MinVal)
assert found is not None
print("Found threshold:", found.val)  # 10
```
</details>

### Exercise 2: Type Sanitizer Function
Write a function that inspects a dataclass and strips whitespace from all string fields annotated with `Annotated[str, "strip"]`.

<details>
<summary>Solution</summary>

```python
from typing import Annotated, get_type_hints, get_origin, get_args
from dataclasses import dataclass

def sanitize_instance[T: object](obj: T) -> T:
    hints = get_type_hints(type(obj), include_extras=True)
    for field_name, field_type in hints.items():
        if get_origin(field_type) is Annotated and "strip" in get_args(field_type)[1:]:
            val = getattr(obj, field_name)
            if isinstance(val, str):
                setattr(obj, field_name, val.strip())
    return obj

@dataclass
class Customer:
    name: Annotated[str, "strip"]
    raw_code: str

c = Customer("   John Doe   \n", "  SECRET_ABC  ")
sanitize_instance(c)
print(f"Sanitized name: '{c.name}', raw code: '{c.raw_code}'")
# Output: Sanitized name: 'John Doe', raw code: '  SECRET_ABC  '
```
</details>

### Exercise 3: CLI Option Parser from Annotations
Build a simple argument parser that inspects a CLI handler function and prints help flags based on docstrings attached via `Annotated`.

<details>
<summary>Solution</summary>

```python
from typing import Annotated, get_type_hints, get_origin, get_args, Callable

type Host = Annotated[str, "Server binding host address"]
type Port = Annotated[int, "Network listen port"]

def run_server(host: Host = "127.0.0.1", port: Port = 8080) -> None:
    pass

def generate_cli_help(func: Callable[..., None]) -> None:
    print(f"Usage for {func.__name__}:")
    hints = get_type_hints(func, include_extras=True)
    for name, tp in hints.items():
        if name == "return":
            continue
        desc = ""
        if get_origin(tp) is Annotated:
            desc = " - " + str(get_args(tp)[1])
        print(f"  --{name} ({get_args(tp)[0].__name__}){desc}")

generate_cli_help(run_server)
# Output:
# Usage for run_server:
#   --host (str) - Server binding host address
#   --port (int) - Network listen port
```
</details>

### Exercise 4: Dynamic JSON Deserializer with Custom Key Mapping
Create a deserializer that maps snake_case JSON fields to camelCase Python attributes using `Annotated[T, "alias=<name>"]`.

<details>
<summary>Solution</summary>

```python
from typing import Annotated, get_type_hints, get_origin, get_args

class UserDTO:
    first_name: Annotated[str, "alias=first_name"]
    last_name: Annotated[str, "alias=last_name"]
    user_id: Annotated[int, "alias=id"]

def deserialize[T](cls: type[T], data: dict[str, object]) -> T:
    instance = cls.__new__(cls)
    hints = get_type_hints(cls, include_extras=True)
    for attr, tp in hints.items():
        field_key = attr
        if get_origin(tp) is Annotated:
            for m in get_args(tp)[1:]:
                if isinstance(m, str) and m.startswith("alias="):
                    field_key = m.split("=")[1]
        setattr(instance, attr, data[field_key])
    return instance

dto = deserialize(UserDTO, {"first_name": "Bob", "last_name": "Ross", "id": 101})
print(f"User: {dto.first_name} {dto.last_name} (ID: {dto.user_id})")
# Output: User: Bob Ross (ID: 101)
```
</details>

### Exercise 5: Recursive Type Printer
Write a function `dump_type_tree(tp: object)` that recursively prints the nested hierarchy of arbitrary complex union and generic types.

<details>
<summary>Solution</summary>

```python
from typing import get_origin, get_args

def dump_type_tree(tp: object, depth: int = 0) -> None:
    origin = get_origin(tp)
    args = get_args(tp)
    display = origin.__name__ if hasattr(origin, "__name__") else str(origin or tp)
    print("  " * depth + f"- {display}")
    for arg in args:
        dump_type_tree(arg, depth + 1)

dump_type_tree(dict[str, list[int | str]])
# Output:
# - dict
#   - str
#   - list
#     - UnionType
#       - int
#       - str
```
</details>

### Exercise 6: Dependency Injection Lifecycle Manager
Extend the dependency injection pattern to support cleanup teardown (generators with `yield`) using `Annotated`.

<details>
<summary>Solution</summary>

```python
from typing import Annotated, get_type_hints, get_origin, get_args, Iterator, Callable
from contextlib import contextmanager

def get_db_cursor() -> Iterator[str]:
    print("[Teardown-DI] Opening cursor")
    yield "ActiveCursor"
    print("[Teardown-DI] Closing cursor")

type DBCursor = Annotated[str, get_db_cursor]

def execute_query(cursor: DBCursor, sql: str) -> None:
    print(f"Executing '{sql}' on {cursor}")

def run_with_lifecycle(func: Callable[..., None], **kwargs: object) -> None:
    hints = get_type_hints(func, include_extras=True)
    cleanups = []
    call_args = dict(kwargs)
    try:
        for name, tp in hints.items():
            if get_origin(tp) is Annotated:
                for meta in get_args(tp)[1:]:
                    if callable(meta):
                        gen = meta()
                        if hasattr(gen, "__next__"):
                            val = next(gen)
                            cleanups.append(gen)
                            call_args[name] = val
        func(**call_args)
    finally:
        for gen in cleanups:
            try:
                next(gen)
            except StopIteration:
                pass

run_with_lifecycle(execute_query, sql="SELECT 1")
# Output:
# [Teardown-DI] Opening cursor
# Executing 'SELECT 1' on ActiveCursor
# [Teardown-DI] Closing cursor
```
</details>
