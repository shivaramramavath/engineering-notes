# Enumerations: enum.Enum, IntEnum, StrEnum, and Flag

- **Topic**: `05-oop`
- **Python version**: 3.12+ (tested on CPython 3.14.0)
- **Tested with**: CPython 3.12, 3.13, 3.14 (standard & free-threaded builds)
- **Free-threading notes**: Compatible. Enums are initialized at module import time and their members are immutable singletons, making them fully thread-safe.

---

## 1. Concept Overview

The `enum` module (PEP 435, enhanced in Python 3.11 with `StrEnum` and bitwise improvements) provides symbolic names bound to unique, constant values.

Unlike raw constants (e.g. `STATUS_PENDING = 1`) or string literals (`"pending"`), Enums offer:
- **Type Safety**: Prevents accidental comparison between unrelated constant families.
- **Member Identity**: Enum members are singletons; identity comparisons (`member is Status.ACTIVE`) are valid and fast.
- **Iteration and Containment**: Clean membership checking and iteration over allowed choices.
- **Distinct Variants**:
  - `Enum`: Base enumeration. Members are not comparable to plain integers or strings.
  - `IntEnum`: Subclasses `int`. Members compare equal to integers and can be used in arithmetic.
  - `StrEnum` (Python 3.11+): Subclasses `str`. Members compare directly with strings and auto-generate lowercase strings with `auto()`.
  - `Flag` / `IntFlag`: Supports bitwise operations (`|`, `&`, `^`, `~`) to represent bitmask options.

```
                      enum.Enum
                      /   |   \
                     /    |    \
             IntEnum   StrEnum  Flag
                |                 |
                +--------+--------+
                         |
                      IntFlag
```

---

## 2. Key Mechanics & Architecture

### Enum Anatomy
Every Enum member has two core attributes:
- `.name`: The string identifier used in code (e.g. `"PENDING"`).
- `.value`: The underlying value assigned to the member (e.g. `1` or `"pending"`).

### Comparison Matrix

| Enum Type | Equality with Primitives (`member == 1` or `member == "a"`) | Bitwise Operations (`\|`, `&`) | `auto()` Behavior |
| :--- | :--- | :--- | :--- |
| `Enum` | **False** (strict type isolation) | No | Sequential integers: `1, 2, 3...` |
| `IntEnum` | **True** (isinstance of `int`) | Limited | Sequential integers: `1, 2, 3...` |
| `StrEnum` | **True** (isinstance of `str`) | No | Member name lowercased: `"member"` |
| `Flag` | **False** | **Yes** (creates composite flags) | Powers of 2: `1, 2, 4, 8...` |
| `IntFlag` | **True** (subclasses `int`) | **Yes** | Powers of 2: `1, 2, 4, 8...` |

### The `@unique` Decorator
By default, Enums allow aliases (multiple names mapped to the exact same value). Decorating a class with `@enum.unique` raises a `ValueError` during class construction if duplicate values exist.

---

## 3. Real-World Code Examples

### Example 1: Standard `Enum` with `@unique` and `auto()`
```python
from enum import Enum, auto, unique

@unique
class TaskStatus(Enum):
    PENDING = auto()
    RUNNING = auto()
    COMPLETED = auto()
    FAILED = auto()


status = TaskStatus.RUNNING
print(f"Status: name='{status.name}', value={status.value}")
print(f"Identity check: {status is TaskStatus.RUNNING}")

# Strict isolation: Not equal to raw integer!
print(f"status == 2: {status == 2}")

# Iteration
all_statuses = [s.name for s in TaskStatus]
print(f"All statuses: {all_statuses}")
```
Output:
```
Status: name='RUNNING', value=2
Identity check: True
status == 2: False
All statuses: ['PENDING', 'RUNNING', 'COMPLETED', 'FAILED']
```

### Example 2: Modern `StrEnum` for API and JSON Compatibility
```python
from enum import StrEnum, auto

class HTTPMethod(StrEnum):
    GET = auto()      # Automatically becomes "get"
    POST = auto()     # Automatically becomes "post"
    PUT = auto()      # Automatically becomes "put"
    DELETE = auto()   # Automatically becomes "delete"


method = HTTPMethod.GET
print(f"Method: '{method}' (isinstance str: {isinstance(method, str)})")
print(f"String equality: {method == 'get'}")
# Seamless serialization
import json
print(f"JSON dump: {json.dumps({'method': method})}")
```
Output:
```
Method: 'get' (isinstance str: True)
String equality: True
JSON dump: {"method": "get"}
```

### Example 3: Bitwise Flags with `Flag` and `auto()`
```python
from enum import Flag, auto

class Permission(Flag):
    READ = auto()     # 1
    WRITE = auto()    # 2
    EXECUTE = auto()  # 4
    ADMIN = READ | WRITE | EXECUTE  # Composite alias (7)


user_perms = Permission.READ | Permission.WRITE
print(f"User permissions: {user_perms}")
print(f"Can write? {Permission.WRITE in user_perms}")
print(f"Can execute? {Permission.EXECUTE in user_perms}")

# Admin comparison
print(f"Is user Admin? {user_perms == Permission.ADMIN}")
```
Output:
```
User permissions: Permission.READ|WRITE
Can write? True
Can execute? False
Is user Admin? False
```

### Example 4: Custom Methods and Properties on Enums
```python
from enum import Enum

class Environment(Enum):
    DEV = "development"
    STAGING = "staging"
    PROD = "production"

    @property
    def is_production(self) -> bool:
        return self is Environment.PROD

    def get_api_endpoint(self) -> str:
        if self is Environment.PROD:
            return "https://api.domain.com"
        return f"https://{self.value}.api.domain.com"


env = Environment.PROD
print(f"Env: {env.value}, Prod? {env.is_production}")
print(f"Endpoint: {env.get_api_endpoint()}")
```
Output:
```
Env: production, Prod? True
Endpoint: https://api.domain.com
```

### Example 5: Looking Up Members by Value vs Name
```python
from enum import Enum

class LogLevel(Enum):
    DEBUG = 10
    INFO = 20
    WARNING = 30
    ERROR = 40


# Lookup by value
level_from_val = LogLevel(20)
print(f"From value 20: {level_from_val.name}")

# Lookup by name (subscript)
level_from_name = LogLevel["ERROR"]
print(f"From name 'ERROR': {level_from_name.value}")

# Safe lookup with error handling
try:
    LogLevel(999)
except ValueError as e:
    print(f"Safe lookup caught: {e}")
```
Output:
```
From value 20: INFO
From name 'ERROR': 40
Safe lookup caught: 999 is not a valid LogLevel
```

---

## 4. Common Anti-Patterns & Gotchas

| Bad Practice | Root Cause | Clean Solution |
| :--- | :--- | :--- |
| Comparing `Enum` with raw primitives: `Status.PENDING == 1` | `Enum` members do not equal raw numbers (only `IntEnum` does) | Compare against enum member: `status == Status.PENDING` |
| Mutating enum members at runtime | Enum members are strictly immutable singletons | Treat enums as constant identifiers |
| Forgetting `@unique` when duplicate values are bugs | Enums silently allow alias definitions for identical values | Apply `@enum.unique` to raise `ValueError` on collisions |
| Subclassing an Enum that already defines members | Python enums disallow extending an enum that has existing members | Subclass only empty base enums |
| Using `IntEnum` when serializing to JSON without type checking | `IntEnum` members dump as numbers, which may violate string API schemas | Use `StrEnum` for text APIs |
| Using bitwise operations (`|`, `&`) on regular `Enum` | Regular `Enum` does not implement bitwise operators | Use `enum.Flag` or `enum.IntFlag` |

---

## 5. Interview Q&A

<details>
<summary>1. Why should you prefer Enum over plain module-level constants?</summary>
Enums provide strong typing, namespacing, member immutability, iteration, and serialization safety. They prevent invalid values from being passed to functions, allow membership tests (<code>val in MyEnum</code>), and support identity checks (<code>val is MyEnum.MEMBER</code>).
</details>

<details>
<summary>2. How does auto() differ between Enum and Flag?</summary>
In a standard <code>Enum</code>, <code>auto()</code> generates sequential integers starting at 1 (1, 2, 3, ...). In a <code>Flag</code> or <code>IntFlag</code>, <code>auto()</code> automatically generates powers of two (1, 2, 4, 8, ...) to facilitate binary bitmask operations. In <code>StrEnum</code>, <code>auto()</code> returns the member's name converted to lowercase.
</details>

<details>
<summary>3. Why does Python forbid subclassing an Enum that already has members?</summary>
Allowing subclassing of an Enum with existing members would violate the invariant that an Enum is a complete, closed finite set of values. However, subclassing an empty Enum class to share common methods or properties is allowed.
</details>

<details>
<summary>4. What is the difference between Enum member lookup by name vs value?</summary>
- <b>By name</b>: <code>MyEnum['NAME']</code> looks up a member by its identifier string (raises <code>KeyError</code> if missing).
- <b>By value</b>: <code>MyEnum(val)</code> looks up a member by its underlying assigned value (raises <code>ValueError</code> if missing).
</details>

<details>
<summary>5. When should you use StrEnum instead of Enum?</summary>
Use <code>StrEnum</code> when enum values need to interface directly with string APIs, configuration files, database text columns, or JSON payloads without requiring explicit <code>.value</code> extraction.
</details>

<details>
<summary>6. How do aliases work in Enums, and how do you disallow them?</summary>
If multiple enum members are assigned the same value, the first defined member becomes the canonical member and subsequent members become aliases pointing to the first. To forbid aliases, decorate the class with <code>@enum.unique</code>.
</details>

<details>
<summary>7. Are Enum members singletons?</summary>
Yes. CPython guarantees that each Enum member is a unique singleton instance. You can safely and idiomaticly use the <code>is</code> operator for comparisons (e.g. <code>state is State.READY</code>).
</details>

<details>
<summary>8. How do you test if an arbitrary value is a valid member of an Enum?</summary>
You can test value validity using:
<pre><code>val in MyEnum._value2member_map_
# or
val in [item.value for item in MyEnum]
</code></pre>
Or attempt <code>MyEnum(val)</code> inside a <code>try ... except ValueError:</code> block.
</details>

---

## 6. Practical Challenges

<details>
<summary>Challenge 1: Unique State Machine Transitions</summary>

Build a `State(Enum)` with members `INIT`, `ACTIVE`, `SUSPENDED`, `TERMINATED`. Add a method `can_transition_to(target_state: State) -> bool` that enforces valid transition paths.

```python
from enum import Enum, auto, unique

@unique
class State(Enum):
    INIT = auto()
    ACTIVE = auto()
    SUSPENDED = auto()
    TERMINATED = auto()

    def can_transition_to(self, target: "State") -> bool:
        allowed = {
            State.INIT: {State.ACTIVE},
            State.ACTIVE: {State.SUSPENDED, State.TERMINATED},
            State.SUSPENDED: {State.ACTIVE, State.TERMINATED},
            State.TERMINATED: set(),
        }
        return target in allowed[self]

assert State.INIT.can_transition_to(State.ACTIVE) is True
assert State.INIT.can_transition_to(State.TERMINATED) is False
```
</details>

<details>
<summary>Challenge 2: Modern StrEnum REST Routes</summary>

Create an `APIEndpoint(StrEnum)` with `auto()`. Verify that all values equal their lowercased names and can be concatenated directly into URLs.

```python
from enum import StrEnum, auto

class APIEndpoint(StrEnum):
    USERS = auto()
    ORDERS = auto()
    PRODUCTS = auto()

url = f"https://api.v1.io/{APIEndpoint.USERS}"
assert url == "https://api.v1.io/users"
assert APIEndpoint.ORDERS == "orders"
```
</details>

<details>
<summary>Challenge 3: Role-Based Bitmask Flag System</summary>

Build a `Role(Flag)` supporting `GUEST`, `USER`, `MODERATOR`, and `ADMIN`. Add helper `has_permission(required_role)` checking bit overlap.

```python
from enum import Flag, auto

class Role(Flag):
    GUEST = auto()
    USER = auto()
    MODERATOR = auto()
    ADMIN = GUEST | USER | MODERATOR

def has_role(user_roles: Role, target: Role) -> bool:
    return target in user_roles

mod = Role.MODERATOR | Role.USER
assert has_role(mod, Role.USER) is True
assert has_role(mod, Role.GUEST) is False
assert has_role(Role.ADMIN, Role.MODERATOR) is True
```
</details>

<details>
<summary>Challenge 4: Safe Enum Value Parser</summary>

Write a function `parse_enum(enum_cls, val, default=None)` that returns the enum member matching `val` or returns `default` on error.

```python
from enum import Enum

class Priority(Enum):
    LOW = 1
    HIGH = 2

def parse_enum(enum_cls, val, default=None):
    try:
        return enum_cls(val)
    except (ValueError, TypeError):
        return default

assert parse_enum(Priority, 1) is Priority.LOW
assert parse_enum(Priority, 99, default=Priority.LOW) is Priority.LOW
```
</details>

<details>
<summary>Challenge 5: Enum with Ordered Priority Comparison</summary>

Create an `IntEnum` representing `LogLevel(DEBUG=10, INFO=20, WARN=30, ERROR=40)`. Verify numeric comparisons work natively.

```python
from enum import IntEnum

class LogLevel(IntEnum):
    DEBUG = 10
    INFO = 20
    WARN = 30
    ERROR = 40

assert LogLevel.DEBUG < LogLevel.INFO
assert LogLevel.ERROR >= 40
assert max([LogLevel.DEBUG, LogLevel.ERROR]) is LogLevel.ERROR
```
</details>

<details>
<summary>Challenge 6: Multi-Value Enum Pattern</summary>

Create an `HTTPStatus(Enum)` where each member defines both an integer status code and a descriptive message (e.g. `(200, "OK")`).

```python
from enum import Enum

class HTTPStatus(Enum):
    OK = (200, "OK")
    NOT_FOUND = (404, "Not Found")
    SERVER_ERROR = (500, "Internal Server Error")

    def __init__(self, code: int, message: str):
        self.code = code
        self.message = message

assert HTTPStatus.OK.code == 200
assert HTTPStatus.OK.message == "OK"
assert HTTPStatus.NOT_FOUND.code == 404
```
</details>
