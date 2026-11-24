# Instance vs Class Attributes

- **Topic**: `05-oop`
- **Python version**: 3.12+ (tested on CPython 3.14.0)
- **Tested with**: CPython 3.12, 3.13, 3.14 (standard & free-threaded builds)
- **Free-threading notes**: Compatible. Instance `__dict__` and Class `__dict__` lookups are protected by per-object locks or internal split-table safeguards.

---

## 1. Concept Overview

In Python, attributes can reside at two distinct scopes within an object-oriented hierarchy:
1. **Class Attributes**: Defined directly inside the class body. Shared across all instances of that class. They reside in `Class.__dict__` (a read-only `mappingproxy`).
2. **Instance Attributes**: Assigned to `self` (typically inside `__init__` or other methods). Unique to each individual object instance. They reside in `instance.__dict__` (a standard mutable dictionary).

When an attribute lookup `obj.attr` occurs, Python resolves it through the **attribute lookup chain**:
```
obj.attr lookup sequence:
1. Data Descriptors in Class & Bases (e.g., properties with setter)
2. Instance Dictionary: obj.__dict__['attr']
3. Non-Data Descriptors / Class Attributes: Class.__dict__['attr'] & MRO bases
4. __getattr__(self, 'attr') hook if defined
5. Raises AttributeError
```

A common source of insidious bugs is **attribute shadowing**—assigning `obj.attr = val` creates an instance-level key in `obj.__dict__`, which shadows the class-level attribute for `obj` without altering it for any other instance.

---

## 2. Key Mechanics & Architecture

### The Dual Dictionaries
Every standard Python class and instance maintains a dictionary storing its attributes:
- `MyClass.__dict__`: Type is `mappingproxy`. Prevents direct mutation via `MyClass.__dict__['x'] = 1` (requires `setattr(MyClass, 'x', 1)` or `MyClass.x = 1`).
- `instance.__dict__`: Type is `dict`. Standard hash table or CPython split table mapping string attribute names to object pointers.

```
       +---------------------------------------------+
       |               Account Class                 |
       |  Account.__dict__                           |
       |  ├── 'interest_rate': 0.05                  |
       |  ├── 'bank_name': 'Acme Bank'               |
       |  └── '__init__': <function>                 |
       +---------------------------------------------+
                  ^                       ^
                  |                       |
                  | (Fallback lookup)     | (Fallback lookup)
                  |                       |
       +-----------------------+     +-----------------------+
       |   acc1 (Instance)     |     |   acc2 (Instance)     |
       |  acc1.__dict__        |     |  acc2.__dict__        |
       |  ├── 'owner': 'Alice' |     |  ├── 'owner': 'Bob'   |
       |  └── 'balance': 1000  |     |  ├── 'balance': 2500  |
       |                       |     |  └── 'interest_rate': |  <-- Shadowed!
       +-----------------------+     |         0.08          |
                                     +-----------------------+
```

### The Mutation Trap: Mutable Class Attributes
If a class attribute is a **mutable container** (e.g. `list`, `dict`, `set`), mutating it in-place via an instance (`obj.items.append(x)`) alters the shared container across ALL instances!
Conversely, assigning `obj.items = [x]` creates an instance attribute, leaving the class container untouched.

---

## 3. Real-World Code Examples

### Example 1: Shadowing and Dictionary Inspection
```python
class ServerNode:
    region = "us-east-1"  # Class attribute

    def __init__(self, node_id: str):
        self.node_id = node_id  # Instance attribute


n1 = ServerNode("node-01")
n2 = ServerNode("node-02")

print(f"n1 dict: {n1.__dict__}")
print(f"n1.region (class fallback): {n1.region}")

# Shadowing region on n1 only
n1.region = "eu-west-1"

print(f"After n1.region assignment:")
print(f"n1 dict: {n1.__dict__}")
print(f"n2 dict: {n2.__dict__}")
print(f"n1.region: {n1.region}, n2.region: {n2.region}, Class.region: {ServerNode.region}")
```
Output:
```
n1 dict: {'node_id': 'node-01'}
n1.region (class fallback): us-east-1
After n1.region assignment:
n1 dict: {'node_id': 'node-01', 'region': 'eu-west-1'}
n2 dict: {'node_id': 'node-02'}
n1.region: eu-west-1, n2.region: us-east-1, Class.region: us-east-1
```

### Example 2: The Mutable Class Attribute Bug & Solution
```python
class BuggySession:
    active_tokens: list[str] = []  # SHARED BY ALL INSTANCES!

    def add_token(self, token: str):
        self.active_tokens.append(token)


class CorrectSession:
    def __init__(self):
        self.active_tokens: list[str] = []  # UNIQUE PER INSTANCE

    def add_token(self, token: str):
        self.active_tokens.append(token)


b1 = BuggySession()
b2 = BuggySession()
b1.add_token("tok-alice")
print(f"Buggy b2 tokens: {b2.active_tokens}")  # Leaked!

c1 = CorrectSession()
c2 = CorrectSession()
c1.add_token("tok-alice")
print(f"Correct c2 tokens: {c2.active_tokens}")
```
Output:
```
Buggy b2 tokens: ['tok-alice']
Correct c2 tokens: []
```

### Example 3: Tracking All Instances via Class Attribute
```python
import weakref

class RegisteredDevice:
    # Use WeakSet to avoid memory leaks when instances are deleted
    _registry = weakref.WeakSet()

    def __init__(self, serial_num: str):
        self.serial_num = serial_num
        RegisteredDevice._registry.add(self)

    @classmethod
    def active_device_count(cls) -> int:
        return len(cls._registry)


d1 = RegisteredDevice("DEV-001")
d2 = RegisteredDevice("DEV-002")
print(f"Active devices: {RegisteredDevice.active_device_count()}")
del d1
print(f"Active devices after del d1: {RegisteredDevice.active_device_count()}")
```
Output:
```
Active devices: 2
Active devices after del d1: 1
```

### Example 4: CPython Split-Table Optimization Inspection
```python
import sys

class Point:
    def __init__(self, x: float, y: float):
        self.x = x
        self.y = y


p1 = Point(1.0, 2.0)
p2 = Point(3.0, 4.0)

# CPython optimizes instances sharing identical attribute keys by sharing
# the keys table in a "split table" dictionary, minimizing memory overhead.
print(f"p1 keys: {list(p1.__dict__.keys())}")
print(f"p2 keys: {list(p2.__dict__.keys())}")
# Injecting an unexpected dynamic attribute converts p1's dict into a combined table
p1.z = 5.0
print(f"p1 dict keys after dynamic add: {list(p1.__dict__.keys())}")
```
Output:
```
p1 keys: ['x', 'y']
p2 keys: ['x', 'y']
p1 dict keys after dynamic add: ['x', 'y', 'z']
```

### Example 5: Class Attribute Constants with Type Annotations
```python
from typing import ClassVar

class PaymentGateway:
    # Explicitly mark class variables with ClassVar to prevent accidental instance assignment
    API_VERSION: ClassVar[str] = "v3"
    TIMEOUT_SECONDS: ClassVar[int] = 30

    def __init__(self, api_key: str):
        self.api_key = api_key  # Instance variable


gw = PaymentGateway("sk_test_12345")
print(f"Gateway: API={gw.API_VERSION}, Key={gw.api_key[:7]}...")
```
Output:
```
Gateway: API=v3, Key=sk_test...
```

---

## 4. Common Anti-Patterns & Gotchas

| Bad Practice | Root Cause | Clean Solution |
| :--- | :--- | :--- |
| Defining mutable defaults (`[]`, `{}`) in class body | Class body executes once at import time; container is shared across all instances | Initialize mutable containers in `__init__` as `self.items = []` |
| Attempting to update a class attribute via `self.val = ...` | Creates an instance attribute in `self.__dict__`, shadowing the class attribute | Mutate via `type(self).val = ...` or `ClassName.val = ...` |
| Modifying `MyClass.__dict__` directly | `MyClass.__dict__` is an immutable `mappingproxy` | Use `setattr(MyClass, key, val)` or standard dot notation `MyClass.key = val` |
| Storing hard references to instances in class-level lists | Prevents garbage collector from reclaiming instances | Use `weakref.WeakSet` or `weakref.WeakValueDictionary` |
| Confusing `ClassVar` with normal type annotations | Without `ClassVar`, linters and dataclasses treat annotated variables as instance fields | Annotate class-only variables with `typing.ClassVar` |
| Relying on class attribute inheritance after instance override | Once shadowed, `self.attr` will never reflect future updates to `BaseClass.attr` | Call `del self.attr` to restore inheritance fallback |

---

## 5. Interview Q&A

<details>
<summary>1. What happens under the hood when you write obj.x = 10 versus Class.x = 10?</summary>
<code>obj.x = 10</code> checks if <code>x</code> is a data descriptor on <code>Class</code>. If not, it writes directly into <code>obj.__dict__['x'] = 10</code>. This creates or overwrites an instance attribute. <code>Class.x = 10</code> writes directly into the class namespace (managed by <code>type.__setattro__</code>), affecting all existing and future instances that do not shadow <code>x</code>.
</details>

<details>
<summary>2. What is attribute shadowing?</summary>
Attribute shadowing occurs when an instance attribute has the exact same name as a class attribute. In Python's attribute lookup resolution, the instance's <code>__dict__</code> is checked before non-data class attributes. Therefore, looking up <code>obj.name</code> returns the instance attribute, hiding ("shadowing") the class attribute.
</details>

<details>
<summary>3. How can you remove an instance attribute and restore access to the class attribute?</summary>
You can delete the key from the instance dictionary using <code>del obj.attr</code>. Once removed from <code>obj.__dict__</code>, subsequent lookups of <code>obj.attr</code> fall back to the class attribute in <code>type(obj).__dict__</code>.
</details>

<details>
<summary>4. Why is Class.__dict__ a mappingproxy rather than a standard dict?</summary>
CPython wraps class namespaces in a <code>mappingproxy</code> to ensure internal consistency and fast method dispatch. Directly modifying a class dict could bypass internal type cache invalidation (which CPython uses to optimize method and attribute lookups).
</details>

<details>
<summary>5. What is the role of typing.ClassVar?</summary>
<code>typing.ClassVar[T]</code> is a typing construct introduced in PEP 526. It marks a class attribute as strictly belonging to the class, indicating that it must not be set on instances. It also instructs <code>dataclasses.dataclass</code> to ignore the variable when generating <code>__init__</code> parameters.
</details>

<details>
<summary>6. How does CPython optimize memory for instance dictionaries with split tables?</summary>
Introduced in PEP 412, split tables allow multiple instances of the same class to share a single "keys" table (storing attribute names and hashes), while each instance only allocates an array of values. This reduces memory usage per instance by approximately 30-40%.
</details>

<details>
<summary>7. What is the difference between obj.list_attr.append(1) and obj.list_attr = [1] when list_attr is on the class?</summary>
<code>obj.list_attr.append(1)</code> performs an attribute lookup (finding the shared class list) and mutates the list in place, affecting all instances. <code>obj.list_attr = [1]</code> performs an attribute assignment, creating a new instance attribute in <code>obj.__dict__</code> that shadows the class attribute.
</details>

<details>
<summary>8. Does modifying a class attribute dynamically affect existing instances?</summary>
Yes, as long as those existing instances have not shadowed that attribute. When an instance resolves an attribute that is absent from its own <code>__dict__</code>, it queries the class hierarchy dynamically at lookup time.
</details>

---

## 6. Practical Challenges

<details>
<summary>Challenge 1: Safe Instance Counter</summary>

Write a class `WorkerThread` that maintains an accurate count of currently living instances using `weakref`, supporting clean GC deallocation.

```python
import weakref

class WorkerThread:
    _instances = weakref.WeakSet()

    def __init__(self, name: str):
        self.name = name
        WorkerThread._instances.add(self)

    @classmethod
    def live_count(cls) -> int:
        return len(cls._instances)

w1 = WorkerThread("worker-1")
w2 = WorkerThread("worker-2")
assert WorkerThread.live_count() == 2
del w1
assert WorkerThread.live_count() == 1
```
</details>

<details>
<summary>Challenge 2: Shadowing Reset Method</summary>

Write a class `Config` with class attributes `DEBUG = False` and `PORT = 8080`. Add a method `reset_override(attr_name: str)` that deletes any instance override and reverts to class default.

```python
class Config:
    DEBUG = False
    PORT = 8080

    def reset_override(self, attr_name: str):
        if attr_name in self.__dict__:
            del self.__dict__[attr_name]

cfg = Config()
cfg.PORT = 9000
assert cfg.PORT == 9000
cfg.reset_override("PORT")
assert cfg.PORT == 8080
```
</details>

<details>
<summary>Challenge 3: ClassVar-Enforced Configuration</summary>

Use `dataclasses` and `ClassVar` to define an `AppSettings` class with class-level database defaults and instance-level user credentials.

```python
from dataclasses import dataclass
from typing import ClassVar

@dataclass
class AppSettings:
    DB_HOST: ClassVar[str] = "localhost"
    DB_PORT: ClassVar[int] = 5432
    username: str
    password: str

s = AppSettings(username="admin", password="secret_password")
assert s.username == "admin"
assert s.DB_HOST == "localhost"
assert "DB_HOST" not in s.__dict__
```
</details>

<details>
<summary>Challenge 4: Detect Shadowed Attributes Dynamically</summary>

Implement a function `get_shadowed_attributes(instance: object) -> list[str]` that returns the names of all instance attributes currently shadowing a class attribute.

```python
def get_shadowed_attributes(instance: object) -> list[str]:
    cls = type(instance)
    shadowed = []
    for key in instance.__dict__:
        if hasattr(cls, key):
            shadowed.append(key)
    return shadowed

class Node:
    status = "pending"

n = Node()
n.status = "running"
n.ip = "127.0.0.1"
assert get_shadowed_attributes(n) == ["status"]
```
</details>

<details>
<summary>Challenge 5: Hierarchical Scoped Lookup with Class Default</summary>

Build a `TenantContext` class where instances represent tenants. If a tenant defines a `feature_flag`, use it; otherwise fallback to the class-wide default setting.

```python
class TenantContext:
    defaults = {"dark_mode": False, "beta_api": False}

    def __init__(self, tenant_id: str, **custom_flags):
        self.tenant_id = tenant_id
        self.flags = custom_flags

    def is_enabled(self, flag_name: str) -> bool:
        if flag_name in self.flags:
            return self.flags[flag_name]
        return self.defaults.get(flag_name, False)

t1 = TenantContext("t1", dark_mode=True)
t2 = TenantContext("t2")
assert t1.is_enabled("dark_mode") is True
assert t2.is_enabled("dark_mode") is False
```
</details>

<details>
<summary>Challenge 6: Read-Only Class Attribute Enforcement</summary>

Create a class `ImmutableClassAttributeMeta` metaclass that forbids mutating class attributes once the class has been constructed.

```python
class ReadOnlyClassMeta(type):
    def __setattr__(cls, name, value):
        if hasattr(cls, name) and not name.startswith("_"):
            raise AttributeError(f"Cannot reassign class attribute '{name}'")
        super().__setattr__(name, value)

class FixedConstants(metaclass=ReadOnlyClassMeta):
    MAX_RETRIES = 5

try:
    FixedConstants.MAX_RETRIES = 10
    assert False, "Should have raised AttributeError"
except AttributeError:
    pass
```
</details>
