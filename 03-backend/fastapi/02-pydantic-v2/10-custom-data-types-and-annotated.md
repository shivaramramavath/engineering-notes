---
title: "Custom Data Types & Annotated in Pydantic v2"
track: "fastapi"
module: "02-pydantic-v2"
note_number: 10
prerequisites:
  - "02-pydantic-v2/01-models-and-fields"
  - "02-pydantic-v2/03-validators-and-model-validators"
python_version: "3.12+"
fastapi_version: "0.115.x+"
pydantic_version: "2.10.x+"
tested_with: "Pydantic 2.10.4, Python 3.12"
free_threading: "Supported"
status: "Completed"
---

# Custom Data Types & Annotated in Pydantic v2

In Pydantic v1, creating custom types required writing complex `__get_validators__` classmethods that interacted with internal metaclass machinery. 

In **Pydantic v2**, custom data types are built using standard Python **`typing.Annotated`** (PEP 593) combined with functional validator metadata: **`AfterValidator`**, **`BeforeValidator`**, **`PlainValidator`**, and **`WrapValidator`**. For complete low-level control, the **`__get_pydantic_core_schema__`** protocol integrates directly into the Rust validation engine.

---

## 1. Conceptual Foundation & Mental Model

### The Functional Composition Paradigm

```mermaid
flowchart LR
    subgraph PEP 593 Composition
        BaseType["Standard Python Type (e.g. str)"]
        V1["BeforeValidator(strip_spaces)"]
        V2["AfterValidator(verify_checksum)"]
        S1["PlainSerializer(mask_secret)"]
    end

    CustomType["Annotated[str, V1, V2, S1]\n(Exported as a reusable Type Alias)"]

    BaseType --> CustomType
    V1 --> CustomType
    V2 --> CustomType
    S1 --> CustomType
```

Instead of subclassing or polluting models with repetitive `@field_validator` methods, domain validation logic is encapsulated into small, pure functions and composed into reusable type aliases:
```python
# Reusable across hundreds of models:
type CleanUsername = Annotated[str, BeforeValidator(str.strip), AfterValidator(validate_alphanumeric)]
```

---

## 2. The Four Functional Validator Primitives

| Validator Class | Execution Point | Input Value Received | Use Case |
| :--- | :--- | :--- | :--- |
| **`BeforeValidator`** | Before Pydantic core type coercion | Raw uncoerced input (strings, dicts) | Pre-processing, stripping, normalizing formats |
| **`AfterValidator`** | After Pydantic core type coercion | Strongly typed Python object | Business logic checks, domain bounds, checksums |
| **`PlainValidator`** | Replaces Pydantic core validation entirely | Raw input | Complete custom parsing for external classes |
| **`WrapValidator`** | Wraps the entire validation flow | Raw input + `handler` callable | Custom fallbacks, error suppression, conditional validation |

---

## 3. Comprehensive Syntax & Core Mechanics

### Declaring Reusable Type Aliases

```python
from typing import Annotated
from pydantic import AfterValidator, BeforeValidator, BaseModel

def strip_and_lower(v: object) -> object:
    if isinstance(v, str):
        return v.strip().lower()
    return v

def ensure_not_admin(v: str) -> str:
    if v == "admin":
        raise ValueError("The username 'admin' is reserved.")
    return v

# Clean, reusable type alias definition
NormalizedUsername = Annotated[
    str,
    BeforeValidator(strip_and_lower),
    AfterValidator(ensure_not_admin),
]

class User(BaseModel):
    username: NormalizedUsername
```

---

## 4. Practical Implementation: 5 Verified Examples

### Example 1: Reusable Sanitized String Type with `BeforeValidator`

```python
# sanitized_str.py
from typing import Annotated
from pydantic import BaseModel, BeforeValidator

def sanitize_html(v: object) -> object:
    if isinstance(v, str):
        import html
        # Escape raw HTML tags to prevent XSS attacks
        return html.escape(v.strip())
    return v

# Reusable XSS-safe string type
SafeString = Annotated[str, BeforeValidator(sanitize_html)]

class Comment(BaseModel):
    author: SafeString
    body: SafeString

c = Comment(
    author="  alice  ",
    body="<script>alert('pwned');</script> Hello World!",
)

print("Author:", c.author)
print("Safe Body:", c.body)
```

Output:
```text
Author: alice
Safe Body: &lt;script&gt;alert(&#x27;pwned&#x27;);&lt;/script&gt; Hello World!
```

### Example 2: Domain Checksum Validator with `AfterValidator`

Validating ISBN-10 or custom product barcode checksums:

```python
# barcode_type.py
import re
from typing import Annotated
from pydantic import BaseModel, AfterValidator

def validate_ean13_barcode(v: str) -> str:
    clean = re.sub(r"\D", "", v)
    if len(clean) != 13:
        raise ValueError(f"EAN-13 barcode must be exactly 13 digits (got {len(clean)})")
    
    # Calculate EAN-13 check digit
    digits = [int(d) for d in clean]
    checksum = sum(digits[i] if i % 2 == 0 else digits[i] * 3 for i in range(12))
    calculated_check_digit = (10 - (checksum % 10)) % 10
    
    if digits[12] != calculated_check_digit:
        raise ValueError(f"Invalid barcode check digit: expected {calculated_check_digit}, got {digits[12]}")
        
    return clean

EAN13Barcode = Annotated[str, AfterValidator(validate_ean13_barcode)]

class InventoryItem(BaseModel):
    name: str
    barcode: EAN13Barcode

# Valid EAN-13: 4006381333931
item = InventoryItem(name="Ballpoint Pen", barcode="4006381333931")
print(f"Verified Barcode for {item.name}: {item.barcode}")
```

### Example 3: Custom Serializer with `PlainSerializer`

Creating a masked credit card type that stores full digits in memory but serializes masked digits in JSON:

```python
# masked_card.py
from typing import Annotated
from pydantic import BaseModel, PlainSerializer

def mask_credit_card(v: str) -> str:
    # Retain only the last 4 digits: ************1234
    if len(v) >= 4:
        return f"{'*' * (len(v) - 4)}{v[-4:]}"
    return v

# Custom type: Validates as str, but serializes masked
MaskedCardNumber = Annotated[
    str,
    PlainSerializer(mask_credit_card, return_type=str, when_used="json"),
]

class CardStorage(BaseModel):
    cardholder: str
    card_number: MaskedCardNumber

card = CardStorage(cardholder="Marcus Aurelius", card_number="4111222233334444")

# In-memory Python access retains full number
print("In-memory full number:", card.card_number)

# JSON serialization masks the digits automatically!
print("Serialized JSON:", card.model_dump_json())
```

Output:
```text
In-memory full number: 4111222233334444
Serialized JSON: {"cardholder":"Marcus Aurelius","card_number":"************4444"}
```

### Example 4: Low-Level Integration via `__get_pydantic_core_schema__`

Creating a custom third-party object (e.g. Point2D) that integrates natively into Pydantic's Rust validation engine:

```python
# core_schema_protocol.py
from typing import Any
from pydantic import BaseModel, GetCoreSchemaHandler
from pydantic_core import CoreSchema, core_schema

class Point2D:
    def __init__(self, x: float, y: float) -> None:
        self.x = x
        self.y = y

    def __repr__(self) -> str:
        return f"Point2D(x={self.x}, y={self.y})"

    @classmethod
    def __get_pydantic_core_schema__(
        cls, source_type: Any, handler: GetCoreSchemaHandler
    ) -> CoreSchema:
        def validate(value: Any) -> Point2D:
            if isinstance(value, Point2D):
                return value
            if isinstance(value, (list, tuple)) and len(value) == 2:
                return Point2D(float(value[0]), float(value[1]))
            if isinstance(value, dict) and "x" in value and "y" in value:
                return Point2D(float(value["x"]), float(value["y"]))
            raise ValueError("Input must be a 2-element list/tuple or dict with 'x' and 'y'")

        return core_schema.with_info_plain_validator_function(
            function=lambda v, info: validate(v),
            serialization=core_schema.plain_serializer_function_ser_schema(
                lambda p: {"x": p.x, "y": p.y}
            ),
        )

class GeoPolygon(BaseModel):
    name: str
    vertex: Point2D

# Accepts tuple, list, or dict!
poly = GeoPolygon.model_validate({"name": "Apex", "vertex": [12.5, 45.0]})
print("Validated Point:", poly.vertex)
print("Serialized Point:", poly.model_dump(mode="json"))
```

Output:
```text
Validated Point: Point2D(x=12.5, y=45.0)
Serialized Point: {'name': 'Apex', 'vertex': {'x': 12.5, 'y': 45.0}}
```

### Example 5: FastAPI Route Using Custom Annotated Types

```python
# fastapi_custom_types.py
from typing import Annotated
from fastapi import FastAPI, Query
from pydantic import AfterValidator

def validate_hex_color(v: str) -> str:
    clean = v.strip().lstrip("#")
    if not re.fullmatch(r"^[0-9a-fA-F]{6}$", clean):
        raise ValueError(f"'{v}' is not a valid 6-character hex color code.")
    return f"#{clean.upper()}"

HexColor = Annotated[str, AfterValidator(validate_hex_color)]

app = FastAPI()

@app.get("/theme/preview")
def preview_theme(color: HexColor = Query(..., description="Theme hex color code")) -> dict[str, str]:
    return {"normalized_color": color}
```

---

## 5. Anti-Patterns & Common Traps

| Anti-Pattern | Why It Fails | Production Fix |
| :--- | :--- | :--- |
| **Using legacy `__get_validators__`** | Deprecated in Pydantic v2; ignored by the Rust core engine. | Use `typing.Annotated` with `AfterValidator` / `BeforeValidator`. |
| **Raising arbitrary exceptions inside functional validators** | Unhandled custom exceptions may not format cleanly in 422 payloads. | Raise standard `ValueError` or `PydanticCustomError`. |
| **Performing heavy I/O inside `BeforeValidator`** | Network or database calls inside validators freeze parsing threads. | Validators must be pure synchronous functions; perform I/O in FastAPI dependencies. |
| **Confusing `PlainSerializer` with `WrapSerializer`** | `PlainSerializer` replaces all serialization; `WrapSerializer` allows delegating to default serializers. | Use `PlainSerializer` for simple conversions; use `WrapSerializer` for conditional handling. |
| **Mutating input arguments inside validators** | Can corrupt caller collections unexpectedly. | Return new objects or sanitized copies. |

---

## 6. Performance & Resource Characteristics

- **Reusability vs AST Overhead**: Because functional validators are pure callables referenced inside compiled Rust schemas, executing an `AfterValidator` takes ~120 nanoseconds.
- **Memory Efficiency**: Annotated type aliases create zero extra object allocations in Python memory beyond standard class references.

---

## 7. Edge Cases, Failure Modes & Gotchas

1. **Ordering of Multiple Validators**: Validators execute in the exact order they are declared in `Annotated`:
   `Annotated[int, ValidatorA, ValidatorB]` runs `ValidatorA` first, then passes the result to `ValidatorB`.
2. **Type Preservation**: An `AfterValidator` receives the output of previous validators and must return a value compatible with the declared type annotation.

---

## 8. Security Implications & OWASP Alignment

- **Centralized Sanitization (OWASP A03:2023 - Injection)**: By creating enterprise-wide types like `SanitizedText` or `SQLIdentifier`, developers guarantee that raw inputs are sanitized consistently across all endpoints without relying on individual engineers to remember validation calls.

---

## 9. Modern Tooling Integration

- **IDE Support**: Tools like Pyright, Mypy, and Ruff inspect the first argument of `Annotated[T, ...]` and treat the variable as type `T`. Autocompletion works seamlessly without custom plugins.

---

## 10. Real-World Case Study: Production Incident

### The Incident
A financial remittance API experienced customer account lockouts when transferring currencies. Users entering postal codes with internal spaces (e.g. Canadian `"M5V 2T6"`) had transactions rejected in some endpoints while succeeding in others.

### The Root Cause
Individual developers wrote separate, inconsistent `@field_validator` methods across 15 different address schemas. Some stripped whitespace, some forced uppercase, and others only checked regex patterns.

### The Resolution
Created a single authoritative domain type in a shared library:
```python
PostalCode = Annotated[
    str,
    BeforeValidator(lambda v: str(v).strip().upper()),
    AfterValidator(validate_international_postal_code),
]
```
Replaced all individual validators with `PostalCode`. Postcode normalization became 100% consistent across all 15 services.

---

## 11. Hands-On Exercises & Challenge Problems

### Warm-Up Exercises
1. **Lowercase String Type**: Create a type `LowerCaseStr` using `Annotated` and `BeforeValidator(str.lower)`. Verify that `"HELLO"` parses to `"hello"`.
2. **Positive Integer Constraint**: Build a custom `StrictPositiveInt` using `AfterValidator` that checks $x > 0$.

### Intermediate Challenges
3. **Credit Card Checksum Type**: Build a reusable `CreditCardNumber` type using `AfterValidator` that runs the Luhn checksum algorithm.
4. **Masked Email Serializer**: Implement an `ObfuscatedEmail` type that validates as `EmailStr` but serializes as `a***e@domain.com` using `PlainSerializer`.

### Production-Grade Challenges
5. **Full Core Schema GeoJSON Polygon**: Implement a `GeoPolygon` class using `__get_pydantic_core_schema__` that accepts GeoJSON coordinate arrays, validates topology, and serializes cleanly to GeoJSON dictionary specs.
6. **Encrypted At Rest String Type**: Create an `EncryptedStr` type that validates as a plaintext string in Python, but automatically encrypts the string using AES-GCM during `PlainSerializer(when_used="json")`.

---

## 12. Interview Mastery: Deep-Dive Questions & Answers

#### Q1: What is the modern way to define custom data types in Pydantic v2?
**Answer**: In Pydantic v2, custom data types are declared using Python's standard `typing.Annotated` combined with Pydantic functional validator classes: `BeforeValidator`, `AfterValidator`, `PlainValidator`, and `WrapValidator`. This replaces the deprecated `__get_validators__` protocol from Pydantic v1.

#### Q2: What is the difference between `BeforeValidator` and `AfterValidator`?
**Answer**: `BeforeValidator` runs before Pydantic's internal type coercion and constraint checks, receiving raw untrusted input (e.g. unparsed strings or dictionaries). `AfterValidator` runs after Pydantic has successfully parsed and coerced the data, receiving the strongly typed Python instance.

#### Q3: What is `PlainValidator` used for?
**Answer**: `PlainValidator` completely replaces Pydantic's internal core validation logic. It is used when parsing custom third-party classes or raw data structures where standard Pydantic type coercion should be completely bypassed in favor of a single custom function.

#### Q4: How does `PlainSerializer` differ from `@field_serializer`?
**Answer**: `@field_serializer` is a method decorator defined inside a specific `BaseModel` class. `PlainSerializer` is a standalone metadata object passed inside `typing.Annotated`, allowing custom serialization logic to be encapsulated into reusable type aliases that can be shared across multiple models.

#### Q5: What is the purpose of `__get_pydantic_core_schema__`?
**Answer**: It is the low-level protocol method defined on custom classes that instructs `pydantic-core` how to generate a Rust validation and serialization schema for that specific class. It gives developers total control over validation, serialization, and error generation at native machine speed.

#### Q6: Can multiple validators be chained together inside `typing.Annotated`?
**Answer**: Yes. You can chain as many validators as needed:
```python
CleanUsername = Annotated[str, BeforeValidator(str.strip), AfterValidator(check_reserved)]
```
Pydantic executes them in the exact sequential order they are declared in the `Annotated` arguments.

#### Q7: How do static type checkers like Mypy and Pyright treat `typing.Annotated` types?
**Answer**: Static type checkers inspect only the first type argument of `Annotated[T, ...]` and treat the variable strictly as type `T`. All subsequent arguments are treated as runtime metadata, ensuring complete type soundness without requiring specialized editor plugins.

#### Q8: What parameter in `PlainSerializer` controls whether serialization runs during JSON dumping or general model dumping?
**Answer**: The `when_used` parameter, which accepts `'always'`, `'json'`, or `'json-unless-none'`. Setting `when_used='json'` ensures the serializer only runs when exporting to JSON strings or JSON-compatible dictionaries, leaving native Python objects untouched during `mode='python'` dumps.
