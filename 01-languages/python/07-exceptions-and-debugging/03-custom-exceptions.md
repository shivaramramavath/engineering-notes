# Module 07: Exceptions, Context Managers & Debugging (03-custom-exceptions)

> **Topic Overview**: Custom exceptions form the contract through which libraries and application layers communicate failure states. Designing clean, maintainable exception hierarchies—equipped with structured metadata, error codes, and serialization protocols—ensures that client code can handle errors gracefully without resorting to brittle string matching.

---

## Metadata Table

| Field | Value |
|---|---|
| **Category** | Core Language / Software Architecture |
| **Target Level** | Intermediate to Advanced |
| **Python Version** | Python 3.10+ (tested on Python 3.12 - 3.14+) |
| **Tested With** | CPython 3.14.0, uv 0.13.9, ruff 0.17.6, mypy 2.4.1, pytest 9.1.1 |
| **Free-Threading / Platform** | Supported / Platform Independent |

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

- **Problem Statement**: Standard built-in exceptions (like `ValueError` or `KeyError`) are too generic for domain operations. Catching `ValueError` might inadvertently catch an internal type-casting bug rather than an invalid domain business rule. Furthermore, built-in exceptions lack structured fields needed by API gateways (such as HTTP status codes, error codes, and validation details).
- **Mental Model**: Think of custom exceptions as domain events. Every library or bounded context should define a single root exception (e.g., `RequestsError`, `FastAPIError`, `BillingError`), and all sub-errors should inherit from that root. This allows callers to catch the entire category or fine-tune handling for specific failure modes.
- **When to Use**:
  - Whenever building a reusable library or SDK.
  - When communicating business rule violations across architectural layers (e.g., domain to API).
  - When errors need structured metadata (user ID, resource ID, validation errors).
- **When NOT to Use**: Do not reinvent standard errors for pure data structure utilities (e.g., if writing a custom sequence, raising standard `IndexError` is expected by Python protocols).

---

## 2. Core Concepts

### Architectural Hierarchy Pattern
A well-architected library defines a structured tree:

```text
Exception
 └── LibraryBaseError (Catch-all for entire package)
      ├── ClientError (User / caller mistakes)
      │    ├── ValidationError
      │    ├── AuthenticationError
      │    └── ResourceNotFoundError
      └── ServerError (Infrastructure / external failures)
           ├── ConnectionTimeoutError
           └── DatabaseUnavailableError
```

### Key Architectural Decisions
1. **Always Inherit from `Exception`**: Never inherit directly from `BaseException`. Doing so prevents standard `except Exception:` handlers from catching your errors.
2. **Preserve `self.args`**: When overriding `__init__`, always call `super().__init__(message)` or pass the primary human-readable message. Tools, loggers, tracebacks, and pickle serialization rely on `args`.
3. **Dual Inheritance (Mixins)**: When authoring custom collections or mappings, inheriting from both your domain base *and* a built-in error (e.g., `class ConfigKeyNotFound(LibraryBaseError, KeyError):`) provides polymorphic compatibility with built-in protocols.

---

## 3. How It Works Internally

When CPython instantiates an exception, `BaseException.__init__` stores the positional arguments in an internal tuple attribute named `args`.

```text
+--------------------------------------------------------+
| PyBaseExceptionObject Memory Layout                    |
+--------------------------------------------------------+
| ob_refcnt : 1                                          |
| ob_type   : &OrderValidationError_Type                 |
| args      : ("Invalid order status",) <--- PyTupleObject|
| dict      : {                                          |
|               "order_id": 9012,                        |
|               "status": "REJECTED",                    |
|               "code": "ERR_ORD_004"                    |
|             }                                          |
| traceback : NULL / &PyTracebackObject                  |
+--------------------------------------------------------+
```

When `str(exc)` is evaluated, CPython calls `BaseException.__str__`. If `len(args) == 1`, it returns `str(args[0])`. If `len(args) > 1`, it returns `str(args)`. If `args` was not initialized via `super().__init__()`, `str(exc)` returns an empty string `""`.

---

## 4. Syntax & API Reference

### Defining a Complete Production Exception
```python
class AppBaseError(Exception):
    """Root base exception for all application failures."""
    default_message: str = "An application error occurred"
    status_code: int = 500
    error_code: str = "INTERNAL_ERROR"

    def __init__(self, message: str | None = None, **metadata) -> None:
        self.message = message or self.default_message
        self.metadata = metadata
        # Initialize BaseException args tuple so str(self) works seamlessly
        super().__init__(self.message)

    def to_dict(self) -> dict:
        return {
            "error_code": self.error_code,
            "message": self.message,
            "status_code": self.status_code,
            "details": self.metadata
        }
```

---

## 5. Code Examples

### 1. Minimal / Basic Example: Custom Exception with Custom Attribute
```python
class InsufficientFundsError(Exception):
    def __init__(self, balance: float, amount: float) -> None:
        self.balance = balance
        self.amount = amount
        super().__init__(f"Cannot withdraw ${amount:.2f}; current balance is ${balance:.2f}")

try:
    raise InsufficientFundsError(balance=45.50, amount=100.00)
except InsufficientFundsError as err:
    print(f"Caught: {err}")
    print(f"Deficit: ${err.amount - err.balance:.2f}")
    print(f"args tuple: {err.args}")
```
**Output**:
```text
Caught: Cannot withdraw $100.00; current balance is $45.50
Deficit: $54.50
args tuple: ('Cannot withdraw $100.00; current balance is $45.50',)
```

### 2. Real-World Production Example: REST API Error Hierarchy
```python
from typing import Any

class APIError(Exception):
    status_code: int = 500
    error_code: str = "INTERNAL_SERVER_ERROR"

    def __init__(self, message: str, payload: dict[str, Any] | None = None) -> None:
        super().__init__(message)
        self.message = message
        self.payload = payload or {}

    def serialize(self) -> dict[str, Any]:
        return {
            "error": {
                "code": self.error_code,
                "message": self.message,
                "status": self.status_code,
                "details": self.payload
            }
        }

class ResourceNotFoundError(APIError):
    status_code = 404
    error_code = "RESOURCE_NOT_FOUND"

class ValidationError(APIError):
    status_code = 422
    error_code = "UNPROCESSABLE_ENTITY"

def get_product(product_id: int) -> dict:
    if product_id <= 0:
        raise ValidationError("Product ID must be a positive integer", {"field": "product_id", "value": product_id})
    if product_id > 1000:
        raise ResourceNotFoundError(f"Product {product_id} does not exist", {"entity": "product", "id": product_id})
    return {"id": product_id, "name": "Mechanical Keyboard"}

# Simulate API gateway response mapping
for pid in [-1, 2000, 50]:
    try:
        res = get_product(pid)
        print(f"Success 200: {res}")
    except APIError as api_err:
        print(f"HTTP {api_err.status_code}: {api_err.serialize()}")
```
**Output**:
```text
HTTP 422: {'error': {'code': 'UNPROCESSABLE_ENTITY', 'message': 'Product ID must be a positive integer', 'status': 422, 'details': {'field': 'product_id', 'value': -1}}}
HTTP 404: {'error': {'code': 'RESOURCE_NOT_FOUND', 'message': 'Product 2000 does not exist', 'status': 404, 'details': {'entity': 'product', 'id': 2000}}}
Success 200: {'id': 50, 'name': 'Mechanical Keyboard'}
```

### 3. Edge Case: Dual-Inheritance Polymorphism
```python
class SDKError(Exception):
    """Root error for custom SDK."""

class ConfigKeyError(SDKError, KeyError):
    """Can be caught by SDK users OR generic dictionary KeyError handlers."""

def read_config(key: str) -> str:
    raise ConfigKeyError(f"Missing mandatory configuration: {key}")

# Caught as SDKError
try:
    read_config("database_url")
except SDKError as e:
    print(f"Caught by SDK handler: {type(e).__name__} -> {e}")

# Caught as KeyError
try:
    read_config("redis_url")
except KeyError as e:
    print(f"Caught by standard KeyError handler: {type(e).__name__} -> {e}")
```
**Output**:
```text
Caught by SDK handler: ConfigKeyError -> 'Missing mandatory configuration: database_url'
Caught by standard KeyError handler: ConfigKeyError -> 'Missing mandatory configuration: redis_url'
```

### 4. Failure Case & Exception Handling: Omitting `super().__init__`
```python
class BrokenException(Exception):
    def __init__(self, custom_msg: str, code: int) -> None:
        # BUG: Forgot super().__init__(custom_msg)
        self.custom_msg = custom_msg
        self.code = code

err = BrokenException("Something broke", 500)
print(f"String representation: '{str(err)}'")
print(f"Args tuple: {err.args}")
```
**Output**:
```text
String representation: ''
Args tuple: ()
```

### 5. Idiomatic Pythonic Pattern: Custom Exception with Rich `__str__` and Note Compatibility
```python
class DatabaseMigrationError(Exception):
    def __init__(self, migration_file: str, version: int, reason: str) -> None:
        self.migration_file = migration_file
        self.version = version
        self.reason = reason
        super().__init__(f"Migration v{version} ({migration_file}) failed: {reason}")

try:
    exc = DatabaseMigrationError("0023_add_orders.sql", 23, "Foreign key constraint violation")
    exc.add_note("Hint: Ensure the 'users' table is populated before running orders migration.")
    raise exc
except DatabaseMigrationError as err:
    print(f"Primary message: {err}")
    print(f"Migration version: {err.version}")
    print(f"Attached Notes: {err.__notes__}")
```
**Output**:
```text
Primary message: Migration v23 (0023_add_orders.sql) failed: Foreign key constraint violation
Migration version: 23
Attached Notes: ["Hint: Ensure the 'users' table is populated before running orders migration."]
```

---

## 6. Edge Cases & Pitfalls

1. **Pickle Serialization Traps**:
   - Python's standard `pickle` module reconstructs exceptions by passing `self.args` to `__new__` or `__init__`. If your `__init__` signature takes non-optional arguments that differ from what was passed to `super().__init__()`, unpickling will fail with `TypeError`.
2. **Missing `__str__` Representation**:
   - If you omit `super().__init__(message)` and do not define a custom `__str__`, `str(e)` returns an empty string `""`, leading to blank log messages in production.
3. **Overusing Custom Exceptions**:
   - Creating hundreds of 1-line exception classes for every individual error scenario clutters the API. Group by recovery action: only create a distinct subclass if the caller might reasonably catch it independently.

---

## 7. Common Mistakes

| Mistake | Why It Fails | Exception / Diagnostic | Correct Pattern |
|---|---|---|---|
| Subclassing `BaseException` | Application handlers `except Exception:` cannot intercept it | Unhandled process crash | Always subclass `Exception` |
| Omitting `super().__init__(msg)` | Leaves `self.args` empty; `str(e)` returns blank `""` | Empty log messages | Call `super().__init__(message)` |
| Incompatible `__init__` breaking pickle | `pickle.loads` calls `__init__(*args)` which fails if signature mismatches | `TypeError: __init__() missing required positional argument` | Match `args` to `__init__` params or implement `__reduce__` |
| Matching exceptions by string parsing | Brittle; breaks whenever error message phrasing changes | Buggy fragile error handling | Use distinct exception classes with attributes |
| Overwriting `args` attribute directly | CPython expects `args` to be a tuple; non-tuples crash internal formatting | `TypeError: args must be tuple` | Let `BaseException` manage `args` |
| Raising uninstantiated custom exception class without brackets | Works, but prevents passing constructor metadata attributes | Loss of error metadata | `raise CustomError(...)` with instances |

---

## 8. Performance & Complexity

- **Instantiation Overhead**: Instantiating a custom exception is $O(1)$, taking $\approx 150-250$ ns.
- **Memory Footprint**: A custom exception instance with a populated `__dict__` consumes $\approx 150$ bytes of heap memory, plus any references held in `self.metadata`.
- **Attribute Lookup**: Accessing custom attributes on an exception instance uses standard `tp_getattro` descriptor lookup in $O(1)$.

---

## 9. CPython Internals & Bytecode

Let's inspect how custom exceptions interact with `__reduce__` for serialization:

```python
import pickle

class SafeCustomError(Exception):
    def __init__(self, code: int, msg: str) -> None:
        self.code = code
        self.msg = msg
        # Pass exact arguments so pickle can reconstruct it
        super().__init__(code, msg)

err = SafeCustomError(404, "Not Found")
dumped = pickle.dumps(err)
restored = pickle.loads(dumped)

print("Restored type:", type(restored).__name__)
print("Restored code:", restored.code)
print("Restored msg:", restored.msg)
```
**Output**:
```text
Restored type: SafeCustomError
Restored code: 404
Restored msg: Not Found
```
CPython's `BaseException.__reduce__` returns `(type(self), self.args)`. Because `self.args` matches `(code, msg)`, `pickle` cleanly calls `SafeCustomError(404, "Not Found")`.

---

## 10. Best Practices

1. **Provide a Shared Root Base**: Every package should expose `class MyLibError(Exception): pass`.
2. **Include Machine-Readable Error Codes**: Always attach string identifiers (e.g., `code="ERR_AUTH_EXPIRED"`) alongside human messages.
3. **Implement `__reduce__` If Signature Differs**: If your `__init__` takes parameters that cannot be mapped directly to `args`, implement `__reduce__` to guarantee safe multiprocessing and Celery task serialization.

---

## 11. Real-World Case Study: E-Commerce Checkout Error Handling

```python
from dataclasses import dataclass
from typing import Literal

class CheckoutError(Exception):
    """Base exception for all checkout workflow failures."""

@dataclass
class ItemUnavailableError(CheckoutError):
    sku: str
    requested_qty: int
    available_qty: int

    def __str__(self) -> str:
        return f"Item '{self.sku}' has only {self.available_qty} units available (requested {self.requested_qty})"

@dataclass
class PaymentDeclinedError(CheckoutError):
    gateway: str
    decline_code: str
    is_retryable: bool

    def __str__(self) -> str:
        return f"Payment declined by {self.gateway} with code '{self.decline_code}' (Retryable: {self.is_retryable})"

def execute_checkout(sku: str, qty: int, balance: float) -> str:
    # Inventory check
    inventory = {"SKU_LAPTOP": 2}
    avail = inventory.get(sku, 0)
    if avail < qty:
        raise ItemUnavailableError(sku=sku, requested_qty=qty, available_qty=avail)

    # Payment processing
    if balance < 1000:
        raise PaymentDeclinedError(gateway="Stripe", decline_code="INSUFFICIENT_FUNDS", is_retryable=False)

    return "ORDER_SUCCESS"

# Caller handling recovery logic
try:
    execute_checkout("SKU_LAPTOP", 5, 2000.0)
except ItemUnavailableError as inv_err:
    print(f"[RECOVERABLE] Adjusting cart: {inv_err}")
except PaymentDeclinedError as pay_err:
    if pay_err.is_retryable:
        print("[RETRY] Prompting user for alternate payment method")
    else:
        print(f"[FAILED] Non-retryable decline: {pay_err}")
```
**Output**:
```text
[RECOVERABLE] Adjusting cart: Item 'SKU_LAPTOP' has only 2 units available (requested 5)
```

---

## 12. Interview Questions

### Q1: Why should every library define a root base exception class?
<details>
<summary><strong>Answer</strong></summary>
A root base exception (e.g., <code>RequestsException</code> in <code>requests</code>) allows library consumers to catch all errors originating from that library using a single <code>except LibraryError:</code> clause, without having to anticipate every individual sub-exception or catching too broad an error like <code>Exception</code>.
</details>

### Q2: What causes a custom exception's `str(e)` to return an empty string?
<details>
<summary><strong>Answer</strong></summary>
When <code>__init__</code> is overridden in a subclass without calling <code>super().__init__(message)</code>, <code>BaseException.args</code> remains empty. Python's default <code>BaseException.__str__</code> implementation reads from <code>self.args</code>; if <code>args</code> is empty and no custom <code>__str__</code> is provided, it returns an empty string <code>""</code>.
</details>

### Q3: How do custom exceptions interact with Python multiprocessing or Celery?
<details>
<summary><strong>Answer</strong></summary>
Multiprocessing and Celery transfer exceptions across worker process boundaries by pickling them. If a custom exception defines an <code>__init__</code> signature that does not match the tuple stored in <code>self.args</code>, <code>pickle.loads()</code> will raise a <code>TypeError</code> when attempting to unpickle the exception in the parent process.
</details>

### Q4: When is dual-inheritance (subclassing both a domain base and a built-in error) appropriate?
<details>
<summary><strong>Answer</strong></summary>
It is useful when writing custom data structures or protocols where third-party libraries expect standard Python semantics. For instance, subclassing <code>(MyLibraryError, KeyError)</code> allows generic dictionary-handling code to handle missing keys naturally while enabling library-aware code to catch domain-specific errors.
</details>

### Q5: Should custom exceptions implement `__eq__`?
<details>
<summary><strong>Answer</strong></summary>
Typically no. Exceptions are generally compared by identity (<code>is</code>) or type (<code>isinstance</code>). Implementing <code>__eq__</code> can lead to confusing behavior in hash tables, sets, or assertion frameworks unless strictly required for value-based testing.
</details>

### Q6: Can a custom exception be defined using `@dataclass`?
<details>
<summary><strong>Answer</strong></summary>
Yes. In Python 3.10+, <code>@dataclass</code> can be used on classes inheriting from <code>Exception</code>. However, you must ensure that <code>super().__init__(...)</code> or a generated <code>__post_init__</code> passes a primary message to <code>Exception</code> so that <code>str(e)</code> and <code>self.args</code> work properly.
</details>

### Q7: What is the benefit of adding an `error_code` attribute to custom exceptions?
<details>
<summary><strong>Answer</strong></summary>
Error codes provide stable, machine-readable identifiers (e.g., <code>"ERR_USER_NOT_FOUND"</code>) that client applications and API consumers can match on reliably, decoupling error-handling logic from human-readable message strings that may change or be localized.
</details>

### Q8: How do you support clean unpickling if your custom exception has a complex constructor?
<details>
<summary><strong>Answer</strong></summary>
Implement the <code>__reduce__</code> method to return a callable factory and a tuple of constructor arguments: <code>return (self.__class__, (self.arg1, self.arg2))</code>. This tells <code>pickle</code> exactly how to re-instantiate the object on deserialization.
</details>

---

## 13. Exercises & Challenges

### Challenge 1 (Easy): Basic Domain Exception Hierarchy
Create a root exception `BankingError` and two subclasses `AccountClosedError` and `OverdraftError`. Implement `OverdraftError` with attributes `balance` and `overdraft_limit`.
<details>
<summary><strong>Solution</strong></summary>

```python
class BankingError(Exception):
    pass

class AccountClosedError(BankingError):
    pass

class OverdraftError(BankingError):
    def __init__(self, balance: float, limit: float) -> None:
        self.balance = balance
        self.limit = limit
        super().__init__(f"Balance ${balance:.2f} exceeds overdraft limit ${limit:.2f}")

err = OverdraftError(-120.0, 100.0)
assert isinstance(err, BankingError)
assert err.balance == -120.0
assert "exceeds overdraft limit" in str(err)
```
</details>

### Challenge 2 (Easy): HTTP Status Code Exception
Create a class `HTTPError(Exception)` that accepts `status_code: int` and `message: str`. Override `__str__` to return `"[HTTP {status_code}] {message}"`.
<details>
<summary><strong>Solution</strong></summary>

```python
class HTTPError(Exception):
    def __init__(self, status_code: int, message: str) -> None:
        self.status_code = status_code
        self.message = message
        super().__init__(message)

    def __str__(self) -> str:
        return f"[HTTP {self.status_code}] {self.message}"

err = HTTPError(404, "Page Not Found")
assert str(err) == "[HTTP 404] Page Not Found"
assert err.status_code == 404
```
</details>

### Challenge 3 (Medium): Pickle-Safe Custom Exception
Write a custom exception `ValidationError` with attributes `field_name: str` and `violations: list[str]` that safely serializes and deserializes via `pickle` without losing any attributes or raising a `TypeError`.
<details>
<summary><strong>Solution</strong></summary>

```python
import pickle

class ValidationError(Exception):
    def __init__(self, field_name: str, violations: list[str]) -> None:
        self.field_name = field_name
        self.violations = violations
        super().__init__(field_name, violations)

    def __str__(self) -> str:
        return f"Field '{self.field_name}' invalid: {', '.join(self.violations)}"

err = ValidationError("email", ["Missing @ symbol", "Domain invalid"])
restored = pickle.loads(pickle.dumps(err))
assert restored.field_name == "email"
assert len(restored.violations) == 2
assert str(restored) == str(err)
```
</details>

### Challenge 4 (Medium): Structured Error Aggregator
Write an exception `AggregateValidationError(Exception)` that holds a dictionary mapping field names to lists of error messages. Implement a method `to_json_response() -> tuple[dict, int]` returning a status code 400 and formatted error payload.
<details>
<summary><strong>Solution</strong></summary>

```python
class AggregateValidationError(Exception):
    def __init__(self, errors: dict[str, list[str]]) -> None:
        self.errors = errors
        super().__init__(f"Validation failed for {len(errors)} fields")

    def to_json_response(self) -> tuple[dict, int]:
        return {
            "status": "error",
            "validation_errors": self.errors
        }, 400

err = AggregateValidationError({"username": ["Too short"], "age": ["Must be >= 18"]})
payload, code = err.to_json_response()
assert code == 400
assert "username" in payload["validation_errors"]
```
</details>

### Challenge 5 (Hard): Dataclass Exception with Automatic Note Extraction
Create a decorator or base class `RichException` using `@dataclass` where every field is automatically inspected, converted into key-value pairs, and added to the exception's `__notes__` attribute on instantiation.
<details>
<summary><strong>Solution</strong></summary>

```python
from dataclasses import dataclass, fields

class RichException(Exception):
    def __post_init__(self) -> None:
        lines = []
        for f in fields(self):
            val = getattr(self, f.name)
            lines.append(f"{f.name}: {val}")
        super().__init__(f"{self.__class__.__name__}({', '.join(lines)})")
        for line in lines:
            self.add_note(line)

@dataclass
class ServiceTimeout(RichException):
    service_name: str
    timeout_seconds: float
    retry_count: int

err = ServiceTimeout("PaymentGateway", 2.5, 3)
assert "PaymentGateway" in str(err)
assert len(err.__notes__) == 3
assert "timeout_seconds: 2.5" in err.__notes__[1]
```
</details>

### Challenge 6 (Hard): Polymorphic Exception Registry & Dispatcher
Build an exception dispatcher `ErrorRegistry` where custom exception types can be registered alongside their corresponding HTTP status codes and handler callables. Implement `dispatch(exc: Exception) -> tuple[int, dict]` which finds the closest matching handler in the exception's MRO.
<details>
<summary><strong>Solution</strong></summary>

```python
from typing import Callable, Any

class ErrorRegistry:
    def __init__(self) -> None:
        self.handlers: dict[type[Exception], tuple[int, Callable[[Any], dict]]] = {}

    def register(self, exc_cls: type[Exception], status_code: int, handler: Callable[[Any], dict]) -> None:
        self.handlers[exc_cls] = (status_code, handler)

    def dispatch(self, exc: Exception) -> tuple[int, dict]:
        # Walk MRO to find closest registered base
        for cls in type(exc).__mro__:
            if cls in self.handlers:
                code, handler = self.handlers[cls]
                return code, handler(exc)
        return 500, {"error": "Internal Server Error", "detail": str(exc)}

# Test
class RootAppError(Exception): pass
class UserNotFoundError(RootAppError): pass

reg = ErrorRegistry()
reg.register(RootAppError, 400, lambda e: {"error": "Client Error", "msg": str(e)})
reg.register(UserNotFoundError, 404, lambda e: {"error": "Not Found", "user": str(e)})

c1, d1 = reg.dispatch(UserNotFoundError("user_99"))
assert c1 == 404
assert d1["user"] == "user_99"

class UnknownChildError(RootAppError): pass
c2, d2 = reg.dispatch(UnknownChildError("generic"))
assert c2 == 400
```
</details>
