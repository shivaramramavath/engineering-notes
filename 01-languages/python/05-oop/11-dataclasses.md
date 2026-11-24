# Dataclasses: PEP 557 and Modern Enhancements

- **Topic**: `05-oop`
- **Python version**: 3.12+ (tested on CPython 3.14.0)
- **Tested with**: CPython 3.12, 3.13, 3.14 (standard & free-threaded builds)
- **Free-threading notes**: Compatible. Dataclass code generation occurs at class definition time. Generated methods (`__init__`, `__repr__`, `__eq__`) are plain Python functions.

---

## 1. Concept Overview

Introduced in PEP 557 (Python 3.7) and substantially enhanced across Python 3.10 through 3.14, the `@dataclass` decorator automatically generates boilerplate special methods (`__init__`, `__repr__`, `__eq__`, `__hash__`, etc.) based on type-annotated class fields.

Unlike third-party alternatives (like `pydantic` or `attrs`), `dataclasses` is built into the standard library with zero runtime overhead for validation—it is primarily an ergonomic code generator.

Modern Python features:
- `slots=True` (Python 3.10+): Automatically generates `__slots__` for faster attribute access and drastically reduced memory usage.
- `kw_only=True` (Python 3.10+): Enforces keyword-only instantiation arguments.
- `match_args=True`: Powers structural pattern matching (`match/case`).
- `frozen=True`: Creates immutable instances and automatically derives `__hash__`.
- `field(default_factory=...)`: Safely initializes mutable defaults per-instance.
- `__post_init__`: Custom validation and derived field computation hook.

```
@dataclass
class User:
    id: int
    name: str
    roles: list[str] = field(default_factory=list)

         ||  Python compiler automatically generates:
         \/
def __init__(self, id: int, name: str, roles=None): ...
def __repr__(self) -> str: ...
def __eq__(self, other) -> bool: ...
def __match_args__ = ('id', 'name', 'roles')
```

---

## 2. Key Mechanics & Architecture

### Dataclass Parameter Matrix

| Parameter | Default | Effect when `True` |
| :--- | :--- | :--- |
| `init` | `True` | Generates `__init__` with annotated fields. |
| `repr` | `True` | Generates `__repr__` formatted as `ClassName(field=val, ...)`. |
| `eq` | `True` | Generates `__eq__` comparing fields as a tuple. |
| `order` | `False` | Generates `__lt__`, `__le__`, `__gt__`, `__ge__` (lexicographical comparison). |
| `unsafe_hash` | `False` | Forces generation of `__hash__` even if mutable (caution!). |
| `frozen` | `False` | Emulates immutability; assignment raises `FrozenInstanceError`. Enables safe `__hash__`. |
| `match_args` | `True` | Generates `__match_args__` for structural pattern matching. |
| `kw_only` | `False` | Makes all fields keyword-only in generated `__init__`. |
| `slots` | `False` | Generates a slotted class (no instance `__dict__`). |

### The `field()` Specification
The `dataclasses.field()` function customizes individual field behavior:
- `default_factory`: Zero-argument callable producing fresh mutable defaults (e.g. `list`, `dict`, `set`).
- `init=False`: Excludes the field from `__init__` (useful for fields populated in `__post_init__`).
- `repr=False`: Excludes sensitive data (e.g. passwords, API keys) from `__repr__`.
- `compare=False`: Excludes field from equality and ordering comparisons.
- `kw_only=True`: Makes this specific field keyword-only.

---

## 3. Real-World Code Examples

### Example 1: Full-Featured Modern Dataclass with Slots and Kw-Only
```python
from dataclasses import dataclass, field
import uuid
import datetime

@dataclass(slots=True, kw_only=True)
class APIKey:
    owner: str
    scopes: list[str] = field(default_factory=lambda: ["read"])
    key_id: str = field(default_factory=lambda: str(uuid.uuid4())[:8])
    secret: str = field(repr=False)  # Hidden from __repr__
    created_at: datetime.datetime = field(default_factory=datetime.datetime.now)


key = APIKey(owner="alice", secret="super-secret-hex-key", scopes=["read", "write"])
print(f"APIKey repr (secret masked): {key}")
print(f"Owner: {key.owner}, Key ID: {key.key_id}, Scopes: {key.scopes}")
# Slotted check: instance has no __dict__
print(f"Has __dict__: {hasattr(key, '__dict__')}")
```
Output:
```
APIKey repr (secret masked): APIKey(owner='alice', scopes=['read', 'write'], key_id='a1b2c3d4', created_at=datetime.datetime(...))
Owner: alice, Key ID: a1b2c3d4, Scopes: ['read', 'write']
Has __dict__: False
```

### Example 2: Immutable (Frozen) Dataclass with Hashing
```python
from dataclasses import dataclass, FrozenInstanceError

@dataclass(frozen=True)
class Coordinate3D:
    x: float
    y: float
    z: float


c1 = Coordinate3D(1.0, 2.0, 3.0)
c2 = Coordinate3D(1.0, 2.0, 3.0)

# 1. Structural equality out of the box
print(f"c1 == c2: {c1 == c2}")

# 2. Hashable: can be used as dict keys or in sets!
coord_set = {c1, c2}
print(f"Set length (deduplicated): {len(coord_set)}")

# 3. Mutation raises FrozenInstanceError
try:
    c1.x = 10.0
except FrozenInstanceError as err:
    print(f"Mutation blocked: {type(err).__name__}")
```
Output:
```
c1 == c2: True
Set length (deduplicated): 1
Mutation blocked: FrozenInstanceError
```

### Example 3: Validation and Derived Fields in `__post_init__`
```python
from dataclasses import dataclass, field

@dataclass
class ShoppingCartItem:
    name: str
    unit_price: float
    quantity: int = 1
    total_price: float = field(init=False)  # Calculated in __post_init__

    def __post_init__(self):
        if self.unit_price < 0:
            raise ValueError("Unit price must be non-negative")
        if self.quantity <= 0:
            raise ValueError("Quantity must be positive")
        self.total_price = round(self.unit_price * self.quantity, 2)


item = ShoppingCartItem("Mechanical Keyboard", 120.0, 2)
print(f"Item: {item.name}, Total: ${item.total_price}")

try:
    ShoppingCartItem("Invalid", -10.0, 1)
except ValueError as e:
    print(f"Validation error: {e}")
```
Output:
```
Item: Mechanical Keyboard, Total: $240.0
Validation error: Unit price must be non-negative
```

### Example 4: Setting Attributes in Frozen Dataclass via `object.__setattr__`
```python
from dataclasses import dataclass

@dataclass(frozen=True)
class ImmutableUUID:
    raw_id: str

    def __post_init__(self):
        # In a frozen dataclass, self.norm_id = ... raises FrozenInstanceError!
        # Must bypass immutability during __post_init__ using object.__setattr__
        object.__setattr__(self, "norm_id", self.raw_id.strip().lower())


uid = ImmutableUUID("  A1B2-C3D4  ")
print(f"Normalized ID: '{uid.norm_id}'")
```
Output:
```
Normalized ID: 'a1b2-c3d4'
```

### Example 5: Conversion to dict, tuple, and replace
```python
from dataclasses import dataclass, asdict, astuple, replace

@dataclass
class ServerProfile:
    host: str
    port: int
    active: bool = True


p1 = ServerProfile("10.0.0.1", 8080)
print(f"asdict:  {asdict(p1)}")
print(f"astuple: {astuple(p1)}")

# Functional non-destructive update via replace()
p2 = replace(p1, port=9000)
print(f"p2 replaced: {p2}")
print(f"p1 original: {p1}")
```
Output:
```
asdict:  {'host': '10.0.0.1', 'port': 8080, 'active': True}
astuple: ('10.0.0.1', 8080, True)
p2 replaced: ServerProfile(host='10.0.0.1', port=9000, active=True)
p1 original: ServerProfile(host='10.0.0.1', port=8080, active=True)
```

---

## 4. Common Anti-Patterns & Gotchas

| Bad Practice | Root Cause | Clean Solution |
| :--- | :--- | :--- |
| Writing `items: list = []` as field default | Mutable default is shared by all instances, raising `ValueError` in dataclasses | Use `items: list = field(default_factory=list)` |
| Modifying attributes directly in `__post_init__` on `frozen=True` | Frozen classes disallow attribute assignment via standard setter | Use `object.__setattr__(self, 'attr', val)` inside `__post_init__` |
| Expecting `@dataclass` to enforce runtime types | Dataclasses use type annotations only for field discovery, not runtime validation | Use Pydantic or perform checks in `__post_init__` |
| Defining non-default field after default field without `kw_only=True` | Python syntax error: non-default argument follows default argument | Place default fields at the bottom or pass `kw_only=True` |
| Setting `unsafe_hash=True` on mutable objects | Mutating an object after inserting into a hash set or dict corrupts hash tables | Use `frozen=True` whenever hashing is needed |
| Missing type annotation on a field | Variables without type annotations are treated as class attributes, ignored by `@dataclass` | Always provide a type annotation (e.g. `val: Any = ...`) |

---

## 5. Interview Q&A

<details>
<summary>1. How does @dataclass discover which variables to include in __init__?</summary>
<code>@dataclass</code> inspects the class's <code>__annotations__</code> dictionary. Any variable with a type annotation is treated as a dataclass field. Class variables annotated with <code>typing.ClassVar</code> are explicitly skipped. Variables assigned without type annotations are treated as plain class attributes and ignored.
</details>

<details>
<summary>2. Why does dataclasses disallow mutable default values like field: list = []?</summary>
In standard Python functions and classes, default values are evaluated once at definition time and shared across all invocations/instances. If <code>field: list = []</code> were allowed, every instance would share the same list instance. Dataclasses detects mutable defaults and raises <code>ValueError: mutable default &lt;class 'list'&gt; for field is not allowed: use default_factory</code>.
</details>

<details>
<summary>3. What is the benefit of slots=True in Python 3.10+ dataclasses?</summary>
<code>slots=True</code> instructs the dataclass decorator to generate a class with <code>__slots__</code> containing all defined fields. This eliminates the per-instance <code>__dict__</code>, yielding up to 40-50% memory savings and faster attribute lookup times.
</details>

<details>
<summary>4. How does frozen=True make instances hashable?</summary>
When <code>frozen=True</code> is specified, dataclasses generates <code>__hash__</code> by computing the hash of a tuple of all fields participating in comparisons (where <code>compare=True</code>). It simultaneously generates <code>__setattr__</code> and <code>__delattr__</code> methods that raise <code>FrozenInstanceError</code>, ensuring instances remain immutable and safe for hashing.
</details>

<details>
<summary>5. What is the role of dataclasses.KW_ONLY in Python 3.10+?</summary>
<code>dataclasses.KW_ONLY</code> is a sentinel type. When placed as a dummy field in a dataclass definition (e.g. <code>_: KW_ONLY</code>), all fields defined after it are automatically converted into keyword-only parameters in the generated <code>__init__</code>.
</details>

<details>
<summary>6. How do you exclude a sensitive field from __repr__?</summary>
Use <code>field(repr=False)</code>:
<pre><code>password: str = field(repr=False)
</code></pre>
This prevents accidental leakage of credentials when objects are printed or logged.
</details>

<details>
<summary>7. What is dataclasses.InitVar?</summary>
<code>InitVar[T]</code> declares an "init-only" pseudo-field that is passed to <code>__init__</code> and forwarded to <code>__post_init__</code>, but is not stored as an actual instance attribute on the object.
</details>

<details>
<summary>8. Does @dataclass validate field types at runtime?</summary>
No. Python's <code>@dataclass</code> is purely a code generator for boilerplate methods. It does not perform runtime type checking or coercion. If runtime validation is required, perform manual checks in <code>__post_init__</code> or use Pydantic.
</details>

---

## 6. Practical Challenges

<details>
<summary>Challenge 1: Audited Frozen Entity</summary>

Create an immutable slotted dataclass `UserEntity` with `user_id: int`, `username: str`, `email: str`, and a hidden `hashed_password: str` excluded from `__repr__`.

```python
from dataclasses import dataclass, field

@dataclass(frozen=True, slots=True)
class UserEntity:
    user_id: int
    username: str
    email: str
    hashed_password: str = field(repr=False)

u = UserEntity(1, "alice", "alice@example.com", "sha256$hash")
assert repr(u) == "UserEntity(user_id=1, username='alice', email='alice@example.com')"
assert not hasattr(u, "__dict__")
```
</details>

<details>
<summary>Challenge 2: InitVar-Driven Database Record</summary>

Use `InitVar` to accept a raw connection string during initialization, parse the host and port in `__post_init__`, without saving the raw connection string as an attribute.

```python
from dataclasses import dataclass, InitVar

@dataclass
class DBEndpoint:
    conn_str: InitVar[str]
    host: str = ""
    port: int = 0

    def __post_init__(self, conn_str: str):
        h, p = conn_str.split(":")
        self.host = h
        self.port = int(p)

ep = DBEndpoint("db.prod.internal:5432")
assert ep.host == "db.prod.internal"
assert ep.port == 5432
assert not hasattr(ep, "conn_str")
```
</details>

<details>
<summary>Challenge 3: Keyword-Only Sentinel Divider</summary>

Create a dataclass `ServerConfig` where `host` and `port` are positional, while `timeout` and `retries` are strictly keyword-only using `KW_ONLY`.

```python
from dataclasses import dataclass, KW_ONLY

@dataclass
class ServerConfig:
    host: str
    port: int
    _: KW_ONLY
    timeout: int = 30
    retries: int = 3

cfg = ServerConfig("localhost", 8080, timeout=10)
assert cfg.host == "localhost"
assert cfg.timeout == 10
try:
    ServerConfig("localhost", 8080, 10)  # Positional timeout should fail!
    assert False
except TypeError:
    pass
```
</details>

<details>
<summary>Challenge 4: Deep Copying Immutable Dataclass with replace()</summary>

Use `dataclasses.replace` to non-destructively increment the revision number of a frozen `DocumentVersion` instance.

```python
from dataclasses import dataclass, replace

@dataclass(frozen=True)
class DocumentVersion:
    doc_id: str
    revision: int
    content: str

v1 = DocumentVersion("DOC-1", 1, "Initial Draft")
v2 = replace(v1, revision=v1.revision + 1, content="Updated Content")
assert v1.revision == 1
assert v2.revision == 2
assert v2.content == "Updated Content"
```
</details>

<details>
<summary>Challenge 5: Custom Ordering Dataclass</summary>

Create a dataclass `MetricLog` with `order=True` that orders instances strictly by `severity` and `timestamp`, ignoring the `message` field during comparisons.

```python
from dataclasses import dataclass, field

@dataclass(order=True)
class MetricLog:
    severity: int
    timestamp: float
    message: str = field(compare=False)

l1 = MetricLog(1, 100.0, "Critical Failure")
l2 = MetricLog(2, 50.0, "Minor Notice")
assert l1 < l2  # Ordered by severity first
```
</details>

<details>
<summary>Challenge 6: Derived Normalized Fields in Frozen Class</summary>

Implement a frozen dataclass `NormalizedURL` that automatically lowercases and strips trailing slashes from `url` inside `__post_init__` using `object.__setattr__`.

```python
from dataclasses import dataclass

@dataclass(frozen=True)
class NormalizedURL:
    raw_url: str

    def __post_init__(self):
        cleaned = self.raw_url.strip().lower().rstrip("/")
        object.__setattr__(self, "url", cleaned)

n = NormalizedURL("  HTTPS://Example.COM/api/  ")
assert n.url == "https://example.com/api"
```
</details>
