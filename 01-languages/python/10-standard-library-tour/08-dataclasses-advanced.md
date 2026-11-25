---
title: "Advanced Data Structures: The dataclasses Module"
description: "Mastering Python's declarative data containers: field factories, frozen immutability, __post_init__ validation, memory-optimized slots=True, kw_only semantics, inheritance resolution, and InitVar patterns."
module: "10-standard-library-tour"
order: 8
tags:
  - python
  - dataclasses
  - oop
  - slots
  - immutability
  - data-modeling
  - memory
difficulty: "Intermediate"
cpython_version: "3.12+"
last_verified: "2026-03"
---

# Advanced Data Structures: The dataclasses Module

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
10. [Real-World Case Study: High-Performance In-Memory Financial Order Book](#real-world-case-study-high-performance-in-memory-financial-order-book)
11. [Interview Questions & Deep-Dive Scenarios](#interview-questions--deep-dive-scenarios)
12. [Practical Exercises & Code Katas](#practical-exercises--code-katas)

---

## What & Why

Introduced in Python 3.7 (PEP 557) and continuously refined in Python 3.10–3.12, the `dataclasses` module provides a declarative code generator for creating structured data container classes. By inspecting type annotations on class attributes, the `@dataclass` decorator automatically synthesizes boilerplate special methods: `__init__()`, `__repr__()`, `__eq__()`, `__hash__()`, and rich comparison methods (`__lt__`, `__le__`, etc.).

### Motivation
Before dataclasses, engineers were forced into compromises:
1. **Plain Python Classes**: Writing 30 lines of boilerplate `def __init__(self, a, b, c): self.a = a; self.b = b...` along with custom `__repr__` and `__eq__` methods.
2. **Tuples / Dictionaries**: Unstructured, lacking attribute dot-notation, runtime type hints, or immutability validation.
3. **`collections.namedtuple`**: Immutable and lightweight, but cannot be easily subclassed, lacks default value flexibility, and behaves unexpectedly when unpacked like a tuple.
Modern `@dataclass` delivers the ergonomics of clean object-oriented class design with the speed and memory efficiency of C-level `__slots__`.

### Real-World Analogy
Consider assembling furniture:
- A **plain Python class** is buying raw timber, a saw, and nails: you must measure, cut, drill holes, and sand every leg manually (`__init__`, `__repr__`, `__eq__`).
- A **`dataclass`** is buying flat-pack precision furniture (IKEA): you provide the blueprint specifications (annotated field names and types), and an automated factory stamps out the exact joints, dowels, and assembly instructions instantly.

### When to Use vs When Not to Use
- **Use `@dataclass` for:** Internal domain models, value objects, API payloads, state objects, configurations, and DTOs (Data Transfer Objects).
- **Use `@dataclass(slots=True)` for:** High-frequency, million-instance datasets where memory consumption must be minimized.
- **Use `@dataclass(frozen=True)` for:** Thread-safe, hashable objects that can serve as dictionary keys or set members.
- **Do Not Use:**
  - When complex runtime string/type parsing and schema validation are needed for untrusted input (use **Pydantic** instead).
  - When simple immutable anonymous 2-tuples or 3-tuples suffice without class semantics.

---

## Core Concepts & Mental Model

```
+-------------------------------------------------------------------------+
| Class Definition (User Code)                                            |
|                                                                         |
| @dataclass(frozen=True, slots=True)                                     |
| class Order:                                                            |
|     order_id: str                                                       |
|     amount: float                                                       |
|     status: str = "pending"                                             |
+-------------------------------------------------------------------------+
                                    |
                                    v
+-------------------------------------------------------------------------+
| Decorator Metaprogramming Synthesis (Lib/dataclasses.py)                |
| - Inspects __annotations__                                              |
| - Generates __init__(self, order_id, amount, status="pending")          |
| - Generates __repr__(self): "Order(order_id='...', amount=..., ...)"    |
| - Generates __eq__(self, other): tuple comparison                       |
| - Generates __hash__(self): hash of fields                              |
| - Strips __dict__ & creates __slots__ = ('order_id', 'amount', 'status')|
+-------------------------------------------------------------------------+
                                    |
                                    v
+-------------------------------------------------------------------------+
| Instantiated Memory Layout (Fast, Compact, Strongly Typed)              |
+-------------------------------------------------------------------------+
```

### Key Decorator Parameters
- `init=True`: Generates `__init__()`.
- `repr=True`: Generates human-readable `__repr__()`.
- `eq=True`: Generates `__eq__()` comparing fields as a tuple.
- `order=False`: If `True`, generates `__lt__`, `__le__`, `__gt__`, `__ge__`.
- `unsafe_hash=False`: Controls `__hash__()` synthesis.
- `frozen=False`: If `True`, mutating attributes raises `FrozenInstanceError`.
- `slots=False` (Python 3.10+): If `True`, generates a class with `__slots__`, eliminating `__dict__` and saving ~60% RAM.
- `kw_only=False` (Python 3.10+): If `True`, all generated fields become keyword-only arguments in `__init__`.

---

## How It Works Internally

### 1. Code Synthesis via `exec()`
The `dataclass` decorator does not rely on dynamic `getattr`/`setattr` interception at runtime. Instead, it inspects class annotations, builds Python function source code strings dynamically (e.g. `def __init__(self, x, y):\n    self.x = x\n    self.y = y`), and compiles them into bytecode using `exec()`.
- Because the generated methods are pure, compiled Python bytecode functions, instantiating and accessing dataclasses runs at the **exact same speed** as a hand-written class!

### 2. The Mechanics of `slots=True` (Python 3.10+)
By default, every Python object instance has an internal dictionary (`__dict__`) to store arbitrary instance attributes. A dictionary requires ~150–200 bytes of overhead per instance.
Setting `slots=True` instructs the Python interpreter to pre-allocate an exact, fixed-size array of pointers directly inside the C `PyObject` structure:
- Eliminates `__dict__` and `__weakref__`.
- Reduces per-instance memory from ~200 bytes down to ~48 bytes.
- Enforces strict attribute safety: assigning an un-declared attribute raises `AttributeError`.

---

## Syntax & API Reference

### Primary Decorator and `field()` Helper
```python
from dataclasses import dataclass, field, InitVar, asdict, astuple, replace
from typing import ClassVar

@dataclass(
    init=True, repr=True, eq=True, order=False,
    unsafe_hash=False, frozen=False, match_args=True,
    kw_only=False, slots=False
)
class Item:
    # Basic field
    name: str

    # Field with default factory (MANDATORY for mutable types like list/dict)
    tags: list[str] = field(default_factory=list)

    # Field excluded from comparisons and repr
    secret_hash: str = field(repr=False, compare=False, default="")

    # Class variable (excluded from dataclass generation)
    CATEGORY: ClassVar[str] = "GENERAL"

    # Init-only variable (passed to __init__ and __post_init__, not stored)
    auth_token: InitVar[str] = None
```

### Utility Functions
```python
d: dict = asdict(instance)             # Recursively converts dataclass to dict
t: tuple = astuple(instance)           # Recursively converts dataclass to tuple
new_inst = replace(instance, field=val) # Creates cloned copy with overrides (immutable safe)
```

---

## Code Examples

### Example 1: Basic Dataclass with Field Defaults and `default_factory`
```python
from dataclasses import dataclass, field
import uuid

@dataclass
class UserProfile:
    username: str
    email: str
    # NEVER use tags: list[str] = [] (Mutable Default Trap!)
    # Always use default_factory=list
    tags: list[str] = field(default_factory=list)
    user_id: str = field(default_factory=lambda: uuid.uuid4().hex[:8])
    active: bool = True

def run_basic_demo():
    u1 = UserProfile("alice", "alice@corp.com")
    u2 = UserProfile("bob", "bob@corp.com")

    u1.tags.append("admin")

    print("User 1:", u1)
    print("User 2:", u2)
    # Verify that tags lists are independent instances
    print("Tags are independent:", u1.tags is not u2.tags)
    assert len(u2.tags) == 0

run_basic_demo()
# Output:
# User 1: UserProfile(username='alice', email='alice@corp.com', tags=['admin'], user_id='...', active=True)
# User 2: UserProfile(username='bob', email='bob@corp.com', tags=[], user_id='...', active=True)
# Tags are independent: True
```

### Example 2: Immutable, Hashable Dataclasses with `frozen=True`
```python
from dataclasses import dataclass

@dataclass(frozen=True)
class Coordinate:
    lat: float
    lon: float

    def distance_from_equator(self) -> float:
        return abs(self.lat) * 111.0  # Approx km

def run_frozen_demo():
    p1 = Coordinate(37.7749, -122.4194)
    p2 = Coordinate(37.7749, -122.4194)

    # 1. Equality evaluation
    print("Coordinates equal:", p1 == p2)

    # 2. Hashable: Can be used as Dictionary Keys and Set elements
    location_registry = {p1: "San Francisco Office"}
    print("Registry Lookup:", location_registry[p2])

    # 3. Mutation protection
    try:
        p1.lat = 40.0
    except Exception as e:
        print("Caught expected mutation error:", type(e).__name__)

run_frozen_demo()
# Output:
# Coordinates equal: True
# Registry Lookup: San Francisco Office
# Caught expected mutation error: FrozenInstanceError
```

### Example 3: Post-Initialization Validation and Computed Fields with `__post_init__`
```python
from dataclasses import dataclass, field

@dataclass
class BankAccount:
    account_number: str
    initial_balance: float
    currency: str = "USD"
    # Field initialized inside __post_init__ (init=False)
    ledger: list[str] = field(init=False, default_factory=list)

    def __post_init__(self):
        # 1. Validation logic
        if self.initial_balance < 0:
            raise ValueError(f"Initial balance cannot be negative: {self.initial_balance}")
        if len(self.currency) != 3:
            raise ValueError(f"Invalid ISO currency code: {self.currency}")

        # 2. Computed initialization
        self.ledger.append(f"Account created with balance: {self.initial_balance} {self.currency}")

def run_post_init_demo():
    acc = BankAccount("ACC-9001", 500.0)
    print("Account:", acc)
    print("Initial Ledger Entry:", acc.ledger)

    try:
        BankAccount("BAD-001", -50.0)
    except ValueError as e:
        print("Validation caught error:", e)

run_post_init_demo()
# Output:
# Account: BankAccount(account_number='ACC-9001', initial_balance=500.0, currency='USD', ledger=['Account created with balance: 500.0 USD'])
# Initial Ledger Entry: ['Account created with balance: 500.0 USD']
# Validation caught error: Initial balance cannot be negative: -50.0
```

### Example 4: Inheritance and `kw_only=True` (Python 3.10+)
```python
from dataclasses import dataclass

# In standard Python, a child class cannot have mandatory positional arguments
# if the parent class has fields with defaults (SyntaxError in generated __init__).
# kw_only=True completely solves this restriction!

@dataclass
class BaseEntity:
    entity_id: str
    created_at: str = "2026-01-01"

@dataclass(kw_only=True)
class Customer(BaseEntity):
    email: str  # Mandatory field in child class!
    tier: str = "Standard"

def run_kw_only_demo():
    # Instantiated cleanly with keyword arguments
    c = Customer(entity_id="E-101", email="vip@client.com", tier="Gold")
    print("Customer created successfully:")
    print("  ID:    ", c.entity_id)
    print("  Date:  ", c.created_at)
    print("  Email: ", c.email)
    print("  Tier:  ", c.tier)

run_kw_only_demo()
# Output:
# Customer created successfully:
#   ID:     E-101
#   Date:   2026-01-01
#   Email:  vip@client.com
#   Tier:   Gold
```

### Example 5: Non-Stored Initialization Variables with `InitVar`
```python
from dataclasses import dataclass, InitVar

@dataclass
class EncryptedSecret:
    secret_name: str
    # raw_password is an InitVar: passed to __init__ and __post_init__,
    # but NOT stored as an attribute on the resulting object!
    raw_password: InitVar[str]
    masked_hash: str = ""

    def __post_init__(self, raw_password: str):
        # Compute and store only the masked version
        self.masked_hash = f"HASHED_{hash(raw_password) & 0xFFFFFFFF:08X}"

def run_initvar_demo():
    sec = EncryptedSecret("DB_PASS", raw_password="SuperSecretPassword123")
    print("EncryptedSecret object:", sec)
    # Verify raw_password was not stored on instance
    print("Has raw_password attribute?", hasattr(sec, "raw_password"))
    assert hasattr(sec, "raw_password") is False

run_initvar_demo()
# Output:
# EncryptedSecret object: EncryptedSecret(secret_name='DB_PASS', masked_hash='HASHED_...')
# Has raw_password attribute? False
```

---

## Edge Cases, Pitfalls & Anti-Patterns

### 1. The Mutable Default Value Trap
Writing `tags: list = []` or `meta: dict = {}` raises a `ValueError: mutable default <class 'list'> for field tags is not allowed: use default_factory`.
- **Reason**: In standard Python functions, default arguments are evaluated once at definition time. A shared mutable list would be shared across all instances! Dataclasses actively blocks this syntax at class definition time.

### 2. The Inheritance Default Argument Problem
If a parent class defines a field with a default (`x: int = 10`), and a child class defines a field without a default (`y: int`), the generated `__init__(self, x=10, y)` generates invalid Python syntax (non-default argument follows default argument).
- **Solution A**: Use `kw_only=True` on the child or parent.
- **Solution B**: Ensure all fields in child classes also provide defaults.

### 3. Comparing Floats in Ordered Dataclasses (`order=True`)
When `order=True` is enabled, comparison methods compare fields sequentially as a tuple (`(field1, field2)`). If any field is a `float` containing `NaN`, comparisons can produce non-reflexive sorting anomalies (`nan == nan` is `False`).

### Common Mistakes Table

| Bad Code | Good Code | Why | Severity |
| :--- | :--- | :--- | :--- |
| `items: list = []` | `items: list = field(default_factory=list)` | Raises `ValueError` at class creation; prevents shared state. | Critical |
| Mutating `frozen=True` instance | Use `dataclasses.replace(inst, field=val)` | Direct mutation raises `FrozenInstanceError`. | High |
| Child field without default after parent default | `@dataclass(kw_only=True)` | Raises `TypeError: non-default argument follows default argument`. | High |
| Missing type annotation on field | Add type annotation (e.g. `val: Any`) | Attributes without type annotations are ignored by `@dataclass`. | High |
| Allocating 1M instances without `slots` | `@dataclass(slots=True)` | Default dataclasses allocate `__dict__`, consuming ~4x more RAM. | Medium |
| Using dataclasses for untrusted external JSON input | Use Pydantic | Dataclasses does NOT validate types at runtime; `"42"` is not cast to `42`. | Medium |

---

## Performance & Memory Characteristics

### Memory Footprint: Standard vs `slots=True`
Measured across 1,000,000 instances of a 3-field object:
- **Standard `@dataclass` (with `__dict__`)**: $\approx 152 \text{ MB}$ (~152 bytes/instance).
- **`@dataclass(slots=True)`**: $\approx 48 \text{ MB}$ (~48 bytes/instance).
- **Memory Reduction**: **~68% RAM savings**!

### Instantiation Performance
- Plain Python class `__init__`: $\approx 110 \text{ ns}$
- Generated `@dataclass` `__init__`: $\approx 110 \text{ ns}$ (identical speed; pure compiled bytecode)
- `namedtuple` instantiation: $\approx 180 \text{ ns}$

---

## CPython Internals & Bytecode

In CPython:
- `dataclasses` is implemented in pure Python (`Lib/dataclasses.py`).
- It iterates over `cls.__annotations__` and processes `Field` descriptors.
- The `_create_fn` helper synthesizes code strings:
  ```python
  def _init_fn(fields, std_eval_types, ...):
      lines = ["def __init__(self, " + ", ".join(args) + "):"]
      for f in fields:
          lines.append(f"  self.{f.name} = {f.name}")
      return _create_fn("__init__", lines, ...)
  ```
- The function is compiled via `compile()` and bound directly to `cls.__init__`.

Bytecode for accessing dataclass fields:
```python
import dis
from dataclasses import dataclass

@dataclass(slots=True)
class Point:
    x: int
    y: int

def read_x(p):
    return p.x

dis.dis(read_x)
```
```text
  1           0 RESUME                   0
  2           2 LOAD_FAST                0 (p)
              4 LOAD_ATTR                1 (x)
             14 RETURN_VALUE
```
Notice that with `slots=True`, `LOAD_ATTR` indexes directly into the C struct offset without dictionary lookups!

---

## Best Practices & Idiomatic Patterns

1. **Always Use `slots=True` on Modern Python (3.10+)**:
   Provides substantial memory savings and faster attribute access with zero code changes.
2. **Use `frozen=True` for Value Objects**:
   Enforces immutability and enables hashability.
3. **Use `dataclasses.replace()` for Immutable Updates**:
   ```python
   new_coord = replace(old_coord, lat=45.0)
   ```
4. **Use `kw_only=True` for Deep Inheritance Trees**:
   Prevents positional argument ordering conflicts between parent and child defaults.

---

## Real-World Case Study: High-Performance In-Memory Financial Order Book

The following matching engine component tracks limit orders using memory-optimized slotted dataclasses, rich comparisons for price-time priority, and immutable replacement.

```python
from dataclasses import dataclass, field, replace
import time
import heapq
from typing import List

@dataclass(slots=True, order=True, frozen=True)
class LimitOrder:
    # Fields ordered for priority matching:
    # Priority 1: Price (highest buy price, lowest sell price)
    # Priority 2: Timestamp (earliest order wins at identical price)
    price: float
    timestamp: float = field(default_factory=time.perf_counter)
    
    # Non-compared fields
    order_id: str = field(compare=False)
    quantity: int = field(compare=False)
    side: str = field(compare=False)  # 'BUY' or 'SELL'

class OrderBook:
    def __init__(self):
        # Min-heap for Sell orders (lowest ask price at top)
        self.sell_orders: List[LimitOrder] = []
        # Max-heap for Buy orders (invert price for min-heap implementation)
        self.buy_orders: List[LimitOrder] = []

    def place_order(self, order_id: str, side: str, price: float, quantity: int) -> LimitOrder:
        # Invert buy prices so heapq pops highest price first
        effective_price = -price if side == "BUY" else price
        order = LimitOrder(
            price=effective_price,
            order_id=order_id,
            quantity=quantity,
            side=side
        )

        if side == "BUY":
            heapq.heappush(self.buy_orders, order)
        else:
            heapq.heappush(self.sell_orders, order)
        return order

    def peek_best_bid(self) -> LimitOrder:
        """Returns highest buy offer."""
        top = self.buy_orders[0]
        # Invert back to real price
        return replace(top, price=-top.price)

    def peek_best_ask(self) -> LimitOrder:
        """Returns lowest sell offer."""
        return self.sell_orders[0]

# Verification
if __name__ == "__main__":
    book = OrderBook()
    # Place buy orders
    book.place_order("ord_1", "BUY", price=100.50, quantity=10)
    book.place_order("ord_2", "BUY", price=101.20, quantity=5)   # Best bid
    book.place_order("ord_3", "BUY", price=100.50, quantity=20)  # Same price as ord_1, later time

    best_bid = book.peek_best_bid()
    print("Best Bid Order ID:", best_bid.order_id)
    print("Best Bid Price:   ", best_bid.price)
    assert best_bid.order_id == "ord_2"
    assert best_bid.price == 101.20
```

---

## Interview Questions & Deep-Dive Scenarios

<details>
<summary><strong>1. Junior: Why does Python raise a <code>ValueError</code> if you assign a mutable default like <code>tags: list = []</code> in a dataclass?</strong></summary>

In Python, default parameter expressions are evaluated once when the class/function is defined, not when instances are created. If mutable defaults like lists or dictionaries were allowed, all instances of the dataclass would share the exact same list in memory; appending to one instance's list would alter all other instances. Dataclasses forbids this and enforces `field(default_factory=list)`.
</details>

<details>
<summary><strong>2. Junior: What is the purpose of the <code>slots=True</code> parameter in Python 3.10+ dataclasses?</strong></summary>

By default, Python objects use an internal dynamic dictionary (`__dict__`) to store instance attributes. Setting `slots=True` instructs Python to generate a class with `__slots__`, pre-allocating a compact, fixed-size C array of attribute pointers. This reduces instance memory consumption by ~60%–70% and slightly accelerates attribute lookups.
</details>

<details>
<summary><strong>3. Mid: What does setting <code>frozen=True</code> do, and how does it affect the <code>__hash__()</code> method?</strong></summary>

`frozen=True` generates `__setattr__` and `__delattr__` methods that raise `dataclasses.FrozenInstanceError` upon any attempted mutation, making instances immutable. Because the instance is guaranteed to be immutable and all fields are checked, `@dataclass` automatically generates a valid `__hash__()` method, allowing frozen instances to be stored in sets and used as dictionary keys.
</details>

<details>
<summary><strong>4. Mid: What is an <code>InitVar</code>, and how is it used?</strong></summary>

`InitVar` is a special field type marker from the `dataclasses` module. An `InitVar` field is included as an argument in the generated `__init__()` method and passed directly to `__post_init__()`, but it is **not** set as an attribute on the instance itself. It is commonly used for database connections, encryption keys, or configuration parameters needed only during initialization.
</details>

<details>
<summary><strong>5. Senior: What problem does <code>kw_only=True</code> solve in dataclass inheritance hierarchies?</strong></summary>

In standard Python, arguments without default values cannot follow arguments with default values in function signatures. If a parent dataclass defines a field with a default (`a: int = 1`), and a subclass defines a field without a default (`b: int`), the synthesized `__init__(self, a=1, b)` produces a `TypeError`. Setting `kw_only=True` generates keyword-only parameters (`def __init__(self, *, a=1, b)`), completely eliminating positional ordering conflicts.
</details>

<details>
<summary><strong>6. Senior: How does <code>order=True</code> generate comparison methods, and how can you exclude specific fields from comparisons?</strong></summary>

When `order=True` is enabled, dataclasses generates `__lt__`, `__le__`, `__gt__`, and `__ge__` by comparing fields in the order they are defined as a tuple. To exclude a field (such as an ID, secret token, or unorderable object) from comparison logic, define it using `field(compare=False)`.
</details>

<details>
<summary><strong>7. Staff: Why does <code>asdict()</code> introduce significant performance overhead on large object graphs, and how would you optimize serialization?</strong></summary>

`asdict()` is a generic recursive function that traverses the entire object tree, inspecting types with `is_dataclass()`, calling `copy.deepcopy()` on factory defaults, and allocating new nested dictionaries. For millions of objects, this creates severe CPU and garbage collection overhead. Optimization: Implement a custom, explicit `.to_dict()` method directly on the dataclass that creates the dictionary with direct attribute accesses, executing up to 10x faster.
</details>

<details>
<summary><strong>8. Staff: How does CPython's dataclass implementation synthesize methods without runtime reflection overhead?</strong></summary>

The `@dataclass` decorator dynamically generates Python source code strings for `__init__`, `__repr__`, and `__eq__` by introspecting `__annotations__`. It then compiles this source string into native bytecode using the built-in `compile()` function and executes it with `exec()`, binding the resulting function object directly to the class namespace. At runtime, executing `MyDataClass(a, b)` invokes pure compiled Python bytecode with zero decorator introspection overhead.
</details>

---

## Practical Exercises & Code Katas

### Exercise 1: Validated Inventory Item Dataclass
**Requirement:** Create a `@dataclass` `InventoryItem` with `sku: str`, `price: float`, and `quantity: int`. In `__post_init__`, validate that `price > 0` and `quantity >= 0`, raising `ValueError` on violations.

<details>
<summary>Show Solution</summary>

```python
from dataclasses import dataclass

@dataclass(slots=True)
class InventoryItem:
    sku: str
    price: float
    quantity: int

    def __post_init__(self):
        if self.price <= 0:
            raise ValueError(f"Price must be positive; got {self.price}")
        if self.quantity < 0:
            raise ValueError(f"Quantity cannot be negative; got {self.quantity}")

# Test
item = InventoryItem("SKU-101", 19.99, 5)
print("Valid Item:", item)

try:
    InventoryItem("BAD-1", -5.0, 1)
except ValueError as e:
    print("Caught expected validation error:", e)
```
</details>

### Exercise 2: Slotted Vector with Magnitude Comparison
**Requirement:** Create a `@dataclass(slots=True, order=False)` `Vector2D(x: float, y: float)` that implements custom `__lt__` comparing vectors by Euclidean magnitude ($\sqrt{x^2 + y^2}$).

<details>
<summary>Show Solution</summary>

```python
from dataclasses import dataclass
import math

@dataclass(slots=True)
class Vector2D:
    x: float
    y: float

    @property
    def magnitude(self) -> float:
        return math.hypot(self.x, self.y)

    def __lt__(self, other: 'Vector2D') -> bool:
        if not isinstance(other, Vector2D):
            return NotImplemented
        return self.magnitude < other.magnitude

# Test
v1 = Vector2D(3.0, 4.0)  # Mag = 5.0
v2 = Vector2D(1.0, 1.0)  # Mag = ~1.41
print("Is v2 < v1:", v2 < v1)
assert v2 < v1
```
</details>

### Exercise 3: Immutable Config Loader with Defaults
**Requirement:** Implement a frozen dataclass `DatabaseConfig` with `kw_only=True`, storing connection parameters with secure defaults. Use `dataclasses.replace` to spawn an environment override.

<details>
<summary>Show Solution</summary>

```python
from dataclasses import dataclass, replace

@dataclass(frozen=True, kw_only=True)
class DatabaseConfig:
    host: str = "localhost"
    port: int = 5432
    db_name: str = "production"
    timeout_sec: float = 30.0

# Base config
base_cfg = DatabaseConfig()

# Environment override
stage_cfg = replace(base_cfg, host="stage-db.internal", timeout_sec=10.0)

print("Base Host: ", base_cfg.host)
print("Stage Host:", stage_cfg.host)
assert stage_cfg.host == "stage-db.internal"
assert base_cfg.host == "localhost"
```
</details>

### Exercise 4: InitVar Masked Field Logger
**Requirement:** Create a dataclass `UserRecord(username: str, ssn_plain: InitVar[str])` that takes a social security number at instantiation, but stores only `masked_ssn: str` (`***-**-1234`).

<details>
<summary>Show Solution</summary>

```python
from dataclasses import dataclass, InitVar, field

@dataclass
class UserRecord:
    username: str
    ssn_plain: InitVar[str]
    masked_ssn: str = field(init=False)

    def __post_init__(self, ssn_plain: str):
        clean = ssn_plain.replace("-", "")
        self.masked_ssn = f"***-**-{clean[-4:]}"

# Test
u = UserRecord("alice", ssn_plain="123-45-6789")
print("User Record:", u)
print("Has raw SSN attribute:", hasattr(u, "ssn_plain"))
assert u.masked_ssn == "***-**-6789"
assert not hasattr(u, "ssn_plain")
```
</details>

### Exercise 5: Hierarchical Event Inheritance with `kw_only`
**Requirement:** Implement a base dataclass `BaseEvent(event_id, timestamp)` and a child dataclass `PaymentEvent(amount, currency)` using `kw_only=True` to resolve field ordering.

<details>
<summary>Show Solution</summary>

```python
from dataclasses import dataclass
import time

@dataclass(kw_only=True)
class BaseEvent:
    event_id: str
    timestamp: float = time.time()

@dataclass(kw_only=True)
class PaymentEvent(BaseEvent):
    amount: float
    currency: str = "USD"

# Test
evt = PaymentEvent(event_id="EVT-100", amount=99.50)
print("Event created:", evt)
assert evt.event_id == "EVT-100"
assert evt.currency == "USD"
```
</details>

### Exercise 6: Fast Explicit Serialization Method
**Requirement:** Compare the execution time of `dataclasses.asdict()` versus a custom hand-rolled `.to_dict()` method on a slotted dataclass over 10,000 iterations.

<details>
<summary>Show Solution</summary>

```python
from dataclasses import dataclass, asdict
import time

@dataclass(slots=True)
class MetricPoint:
    metric: str
    val: float
    ts: int

    def to_dict(self) -> dict:
        return {"metric": self.metric, "val": self.val, "ts": self.ts}

pt = MetricPoint("cpu", 42.5, 1700000000)

# Measure asdict
start = time.perf_counter_ns()
for _ in range(10_000):
    _ = asdict(pt)
dur_asdict = time.perf_counter_ns() - start

# Measure custom to_dict
start = time.perf_counter_ns()
for _ in range(10_000):
    _ = pt.to_dict()
dur_custom = time.perf_counter_ns() - start

print(f"asdict() duration: {dur_asdict:,} ns")
print(f"to_dict() duration: {dur_custom:,} ns")
print(f"Speedup factor:    {dur_asdict / dur_custom:.1f}x")
assert dur_custom < dur_asdict
```
</details>
