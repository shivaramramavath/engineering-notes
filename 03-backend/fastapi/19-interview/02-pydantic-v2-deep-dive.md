# Technical Interview Mastery: Pydantic v2 Core Internals & Performance

## Overview & Evaluation Criteria

Pydantic v2 was rewritten from the ground up in Rust (`pydantic-core`). Senior candidate interviews expect deep knowledge of the compilation boundary between Python and Rust, validation phases (`before`, `after`, `wrap`, `plain`), serialization mechanics, and strict migration changes from Pydantic v1.

---

## Technical Q&A Deep Dive

<details>
<summary><strong>Q1: What architectural changes make Pydantic v2 5x to 20x faster than Pydantic v1? How does <code>pydantic-core</code> interact with Python?</strong></summary>

### Answer:
- **Rust Implementation (`pydantic-core`)**: In v1, model traversal, type validation, and error accumulation occurred via interpreted Python loops and dynamic attribute access. In v2, the validation logic, schema compilation, and JSON parsing are executed entirely in compiled Rust.
- **Compiled Validation Trees**: When a model is declared, Pydantic builds a static, immutable validation tree in Rust during class creation. At runtime, input dicts or JSON byte buffers are passed directly to Rust, avoiding Python function call overhead.
- **Direct C-API Object Creation**: Instead of instantiating Python objects via `__init__`, Rust allocates Python dictionaries and objects directly through C-API calls (`PyDict_SetItem`, `PyObject_New`).
- **Simultaneous Parsing and Validation**: In v1, JSON was first parsed into Python dicts with `json.loads()`, then validated into models. In v2, `model_validate_json()` validates directly from raw UTF-8 JSON bytes in Rust without creating intermediate Python dictionaries.

</details>

<details>
<summary><strong>Q2: Explain the four validator modes in Pydantic v2: <code>before</code>, <code>after</code>, <code>wrap</code>, and <code>plain</code>. When is each appropriate?</strong></summary>

### Answer:
```python
from pydantic import BaseModel, field_validator, ValidationInfo

class UserProfile(BaseModel):
    username: str

    # 1. 'after' (Default): Operates on already-validated type (here, validated str)
    @field_validator("username", mode="after")
    @classmethod
    def check_clean(cls, v: str) -> str:
        return v.lower().strip()

    # 2. 'before': Operates on raw input data before Pydantic core validation runs
    # Ideal for sanitizing malformed inputs, coercing formats, or stripping characters

    # 3. 'wrap': Intercepts validation with a callable handler(raw_input).
    # Allows try/except error recovery or conditional fallback values

    # 4. 'plain': Completely replaces Pydantic's internal validation for this field.
```
- **Rule**: Use `after` for business invariant checks on parsed data; use `before` for input parsing/coercion; use `wrap` for error handling/logging.

</details>

<details>
<summary><strong>Q3: How does Pydantic v2 handle ORM model conversion (<code>from_attributes=True</code>) without triggering N+1 query loops?</strong></summary>

### Answer:
- In v1, `orm_mode = True` relied on Python's `getattr(obj, field)`. If an un-loaded SQLAlchemy relationship was accessed, it triggered an implicit SQL query (the classic N+1 problem).
- In v2, `ConfigDict(from_attributes=True)` still uses getter inspection, but supports strict schema boundaries.
- **Preventing N+1 in FastAPI**:
  1. Use SQLAlchemy 2.0's `selectinload` or `joinedload` in the repository layer to eagerly load required relationships before returning to the router.
  2. In async SQLAlchemy, accessing an un-loaded relationship under `from_attributes=True` raises `MissingGreenlet: Parent instance is not bound to a Session` instead of silently hanging the event loop with synchronous queries.
</details>

<details>
<summary><strong>Q4: What is the purpose of <code>TypeAdapter</code> in Pydantic v2, and when should you use it over a standard <code>BaseModel</code>?</strong></summary>

### Answer:
- A `BaseModel` requires an object structure (mapping/dictionary). It cannot natively validate top-level primitive types (e.g. `list[int]`, `dict[str, UUID]`, or `Annotated[int, Gt(0)]`).
- **`TypeAdapter`**: A generic adapter capable of validating and serializing arbitrary Python types, dataclasses, and primitives without declaring an artificial wrapper model:
```python
from pydantic import TypeAdapter

# Validate a raw list of integers directly
adapter = TypeAdapter(list[int])
result = adapter.validate_json("[1, 2, 3, 4]")
```
- Highly useful in FastAPI for validating standalone query parameters, custom dependency outputs, and external webhook payloads.
</details>
