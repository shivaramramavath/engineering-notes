# __slots__: Memory Optimization and Attribute Restrictions

- **Topic**: `05-oop`
- **Python version**: 3.12+ (tested on CPython 3.14.0)
- **Tested with**: CPython 3.12, 3.13, 3.14 (standard & free-threaded builds)
- **Free-threading notes**: Compatible. Slotted objects allocate a fixed C-array of pointers, eliminating the instance dictionary and thread-contention over dict resizing.

---

## 1. Concept Overview

By default, every Python object instance possesses an internal dictionary (`self.__dict__`) to store its attributes. While this dynamic dictionary provides immense flexibility—allowing attributes to be added or modified at any point at runtime—it incurs significant memory overhead (typically 100 to 150 bytes per instance dict plus hash table padding).

The `__slots__` declaration allows developers to explicitly declare a fixed set of valid attribute names. When `__slots__` is defined in a class body, CPython:
1. **Suppresses `__dict__` creation**: Instances no longer allocate a dynamic hash table.
2. **Suppresses `__weakref__` creation**: Instances cannot be weakly referenced unless `'__weakref__'` is explicitly included in `__slots__`.
3. **Replaces dictionary lookups with descriptor offsets**: Attributes are stored in a compact C-level pointer array inside the `PyObject` structure, drastically speeding up access and reducing memory consumption by up to 50–70%.

```
Standard Object (Without __slots__):
+-------------------------------+
| PyObject Header (16 bytes)    |
| __dict__ pointer (8 bytes)    | ----> +-------------------------+
| __weakref__ pointer (8 bytes) |       | dict hash table (~150B) |
+-------------------------------+       | 'x' -> 10, 'y' -> 20    |
                                        +-------------------------+

Slotted Object (With __slots__ = ('x', 'y')):
+-------------------------------+
| PyObject Header (16 bytes)    |
| slot 'x' pointer (8 bytes)    | ----> 10 (int object)
| slot 'y' pointer (8 bytes)    | ----> 20 (int object)
+-------------------------------+  (Total instance size ~32-48 bytes!)
```

---

## 2. Key Mechanics & Architecture

### Memory Profile: Dict vs Slots

| Metric | Standard Class | Slotted Class (`__slots__`) |
| :--- | :--- | :--- |
| **Instance Base Size** | 48–56 bytes | 40–48 bytes |
| **Instance `__dict__`** | ~104–152+ bytes | **0 bytes** (absent) |
| **Total Memory Per Instance** | ~150–200 bytes | ~40–48 bytes (**~75% reduction**) |
| **Attribute Lookup** | Hash table lookup (`dict_lookup`) | Fixed array index offset (`member_descriptor`) |
| **Dynamic Attributes** | Allowed (`obj.z = 10`) | Forbidden (raises `AttributeError`) |
| **Weak References** | Allowed by default | Requires explicit `'__weakref__'` in `__slots__` |

### Inheritance Rules for `__slots__`
1. **Empty Subclass Trap**: If a subclass inherits from a slotted class without defining its own `__slots__`, Python **automatically creates `__dict__` for the subclass**, nullifying the memory savings!
2. **Redefining Slots**: A subclass should only declare *new* attributes in its `__slots__`. Redefining base slot names wastes space and creates descriptor ambiguity.
3. **Multiple Inheritance with Slots**: Multiple base classes with non-empty `__slots__` cannot be combined in Python; doing so raises `TypeError: multiple bases have instance lay-out conflict`.

---

## 3. Real-World Code Examples

### Example 1: Measuring Memory Savings with `sys.getsizeof`
```python
import sys

class RegularPoint:
    def __init__(self, x: float, y: float):
        self.x = x
        self.y = y


class SlottedPoint:
    __slots__ = ("x", "y")

    def __init__(self, x: float, y: float):
        self.x = x
        self.y = y


reg = RegularPoint(1.0, 2.0)
slot = SlottedPoint(1.0, 2.0)

reg_mem = sys.getsizeof(reg) + sys.getsizeof(reg.__dict__)
slot_mem = sys.getsizeof(slot)

print(f"Regular instance total memory: {reg_mem} bytes")
print(f"Slotted instance total memory: {slot_mem} bytes")
print(f"Memory reduction: {((reg_mem - slot_mem) / reg_mem) * 100:.1f}%")
```
Output:
```
Regular instance total memory: 152 bytes
Slotted instance total memory: 48 bytes
Memory reduction: 68.4%
```

### Example 2: Attribute Restriction Enforcement
```python
class SlottedNode:
    __slots__ = ("ip", "port")

    def __init__(self, ip: str, port: int):
        self.ip = ip
        self.port = port


node = SlottedNode("192.168.1.1", 8080)
print(f"Node: {node.ip}:{node.port}")

# Attempting to assign an undeclared attribute raises AttributeError
try:
    node.status = "online"
except AttributeError as err:
    print(f"Attribute error caught: {err}")
```
Output:
```
Node: 192.168.1.1:8080
Attribute error caught: 'SlottedNode' object has no attribute 'status' and no __dict__ for setting new attributes
```

### Example 3: Enabling `__weakref__` and `__dict__` in Slots
```python
import weakref

class FlexibleSlotted:
    # Explicitly include '__weakref__' and '__dict__' to get slots speed
    # for fixed fields while still allowing weakrefs and dynamic attributes
    __slots__ = ("id", "name", "__weakref__", "__dict__")

    def __init__(self, id: int, name: str):
        self.id = id
        self.name = name


flex = FlexibleSlotted(1, "Service")
# Dynamic attribute works because '__dict__' is in slots
flex.extra_metadata = "cached"

# Weakref works because '__weakref__' is in slots
r = weakref.ref(flex)
print(f"Weakref target: {r().name}, dynamic attr: {flex.extra_metadata}")
```
Output:
```
Weakref target: Service, dynamic attr: cached
```

### Example 4: Slotted Inheritance Done Right
```python
class BaseEntity:
    __slots__ = ("id", "created_at")

    def __init__(self, id: str, created_at: float):
        self.id = id
        self.created_at = created_at


class SlottedUser(BaseEntity):
    # Only declare fields unique to SlottedUser!
    __slots__ = ("username", "email")

    def __init__(self, id: str, created_at: float, username: str, email: str):
        super().__init__(id, created_at)
        self.username = username
        self.email = email


u = SlottedUser("U1", 1700000000.0, "alice", "alice@example.com")
print(f"User: {u.username}, id: {u.id}")
print(f"Has __dict__: {hasattr(u, '__dict__')}")
```
Output:
```
User: alice, id: U1
Has __dict__: False
```

### Example 5: Incompatible Multiple Inheritance Layout Conflict
```python
class SlotA:
    __slots__ = ("a",)

class SlotB:
    __slots__ = ("b",)

try:
    # Combining two distinct classes with non-empty slots fails!
    class Combined(SlotA, SlotB):
        pass
except TypeError as err:
    print(f"Multiple inheritance conflict caught: {err}")
```
Output:
```
Multiple inheritance conflict caught: multiple bases have instance lay-out conflict
```

---

## 4. Common Anti-Patterns & Gotchas

| Bad Practice | Root Cause | Clean Solution |
| :--- | :--- | :--- |
| Subclassing a slotted class without defining `__slots__ = ()` | Subclass automatically creates a `__dict__`, losing all memory benefits | Define `__slots__ = ()` in subclass if no new attributes are added |
| Repeating base class slot names in derived class slots | Allocates redundant slots, wastes memory, creates descriptor shadowing | Declare only new attributes in child `__slots__` |
| Attempting multiple inheritance of classes with non-empty `__slots__` | Raises `TypeError: multiple bases have instance lay-out conflict` | Use mixins with empty `__slots__ = ()` or use composition |
| Using `__slots__` for security or encapsulation | `__slots__` is a memory optimization tool, not an access control mechanism | Use property getters/setters or conventions (`_attr`) for encapsulation |
| Trying to use `weakref` on a slotted class without `'__weakref__'` | Raises `TypeError: cannot create weak reference to 'ClassName' object` | Add `'__weakref__'` to `__slots__` |
| Using `__slots__` on classes with only a few instances | The memory savings for 10 objects is negligible (<1 KB) | Reserve `__slots__` for high-volume objects (e.g. 100k+ instances) |

---

## 5. Interview Q&A

<details>
<summary>1. What is the primary purpose of __slots__ in Python?</summary>
The primary purpose is memory optimization. By replacing the dynamic per-instance dictionary (<code>__dict__</code>) with a fixed C-array of member descriptors, <code>__slots__</code> reduces the memory footprint of each instance by up to 60-75% and slightly speeds up attribute access.
</details>

<details>
<summary>2. Why does multiple inheritance fail when combining two classes with non-empty __slots__?</summary>
In CPython, an instance's memory layout is determined by the size and offsets of its C-struct fields. Two independent classes with non-empty slots define differing memory offset layouts. Combining them creates an unresolvable structural conflict in the C-level <code>PyObject</code> allocation, raising <code>TypeError: multiple bases have instance lay-out conflict</code>.
</details>

<details>
<summary>3. What happens if a subclass of a slotted class does not declare __slots__?</summary>
If the subclass omits <code>__slots__</code>, Python automatically allocates a <code>__dict__</code> for instances of the subclass. While attributes declared in the parent's slots will continue to use slot storage, the presence of <code>__dict__</code> destroys the memory savings of the subclass.
</details>

<details>
<summary>4. How does Python implement attribute access for slotted attributes?</summary>
CPython installs a <code>member_descriptor</code> in the class dictionary for each slot name. When <code>obj.x</code> is accessed, the descriptor computes the direct memory offset from the instance pointer (<code>obj + offset</code>) to read the stored Python object pointer, bypassing dictionary hashing entirely.
</details>

<details>
<summary>5. Can a slotted class have dynamic attributes added at runtime?</summary>
Only if <code>'__dict__'</code> is explicitly included in its <code>__slots__</code> declaration. Otherwise, assigning an attribute not listed in <code>__slots__</code> raises an <code>AttributeError</code>.
</details>

<details>
<summary>6. Why cannot slotted objects be pickled by default in older Python versions?</summary>
Historically, the <code>pickle</code> protocol relied on inspecting <code>obj.__dict__</code> to serialize state. In modern Python 3, pickle supports slotted classes natively, but implementing <code>__getstate__</code> and <code>__setstate__</code> remains best practice for complex slotted structures.
</details>

<details>
<summary>7. How do dataclasses integrate with __slots__?</summary>
Since Python 3.10, passing <code>@dataclass(slots=True)</code> automatically derives and injects <code>__slots__</code> containing all annotated fields, creating a slotted class with zero manual boilerplate.
</details>

<details>
<summary>8. Does __slots__ make instances immutable?</summary>
No. Slotted attributes can be freely reassigned (e.g. <code>node.ip = "10.0.0.2"</code>). It only restricts <i>which</i> attributes can exist, not whether their values can change.
</details>

---

## 6. Practical Challenges

<details>
<summary>Challenge 1: High-Volume Slotted Particle Simulator</summary>

Create a `Particle` class with slots `x, y, z, vx, vy, vz`. Instantiating 100,000 particles must verify that `__dict__` does not exist on instances.

```python
class Particle:
    __slots__ = ("x", "y", "z", "vx", "vy", "vz")

    def __init__(self, x=0.0, y=0.0, z=0.0, vx=0.0, vy=0.0, vz=0.0):
        self.x, self.y, self.z = x, y, z
        self.vx, self.vy, self.vz = vx, vy, vz

p = Particle(1.0, 2.0, 3.0)
assert not hasattr(p, "__dict__")
try:
    p.mass = 1.0
    assert False, "Should raise AttributeError"
except AttributeError:
    pass
```
</details>

<details>
<summary>Challenge 2: Slotted Mixin with Empty Slots</summary>

Build a mixin `JSONExportMixin` that defines `__slots__ = ()` and provides a `.to_dict()` method that reads all slots across the inheritance hierarchy.

```python
class JSONExportMixin:
    __slots__ = ()

    def to_dict(self) -> dict:
        result = {}
        for cls in type(self).__mro__:
            for s in getattr(cls, "__slots__", ()):
                if not s.startswith("_") and hasattr(self, s):
                    result[s] = getattr(self, s)
        return result

class SlottedRecord(JSONExportMixin):
    __slots__ = ("title", "views")
    def __init__(self, title, views):
        self.title = title
        self.views = views

r = SlottedRecord("Python OOP", 5000)
assert r.to_dict() == {"title": "Python OOP", "views": 5000}
```
</details>

<details>
<summary>Challenge 3: Weakref-Enabled Slotted Cache</summary>

Create a slotted `CacheEntry` that stores `key` and `data` in slots, but explicitly supports being referenced by `weakref`.

```python
import weakref

class CacheEntry:
    __slots__ = ("key", "data", "__weakref__")

    def __init__(self, key: str, data: str):
        self.key = key
        self.data = data

entry = CacheEntry("k1", "data-payload")
ref = weakref.ref(entry)
assert ref().key == "k1"
```
</details>

<details>
<summary>Challenge 4: Deep Slotted Inheritance Chain</summary>

Build an inheritance chain `Entity -> Device -> Router` where each class adds 1-2 new slot attributes, ensuring no intermediate class accidentally spawns a `__dict__`.

```python
class Entity:
    __slots__ = ("id",)
    def __init__(self, id): self.id = id

class Device(Entity):
    __slots__ = ("name",)
    def __init__(self, id, name):
        super().__init__(id)
        self.name = name

class Router(Device):
    __slots__ = ("ip",)
    def __init__(self, id, name, ip):
        super().__init__(id, name)
        self.ip = ip

r = Router("R1", "Gateway", "192.168.1.1")
assert not hasattr(r, "__dict__")
assert (r.id, r.name, r.ip) == ("R1", "Gateway", "192.168.1.1")
```
</details>

<details>
<summary>Challenge 5: Custom Descriptor on Slotted Class</summary>

Implement a validated integer descriptor and apply it to a slotted class `BoundedBox(width, height)`.

```python
class PositiveInt:
    def __init__(self, name):
        self.name = name
    def __get__(self, instance, owner):
        if instance is None: return self
        return getattr(instance, f"_{self.name}")
    def __set__(self, instance, value):
        if value <= 0: raise ValueError("Must be positive")
        setattr(instance, f"_{self.name}", value)

class BoundedBox:
    __slots__ = ("_width", "_height")
    width = PositiveInt("width")
    height = PositiveInt("height")

    def __init__(self, w: int, h: int):
        self.width = w
        self.height = h

b = BoundedBox(10, 20)
assert b.width == 10
try:
    b.width = -5
    assert False
except ValueError:
    pass
```
</details>

<details>
<summary>Challenge 6: Slotted Dynamic Fallback Dict</summary>

Create a class `SemiSlotted` with fixed slots `a, b` plus `'__dict__'` that tracks which attributes reside in slots vs the instance dict.

```python
class SemiSlotted:
    __slots__ = ("a", "b", "__dict__")
    def __init__(self, a, b):
        self.a = a
        self.b = b

s = SemiSlotted(1, 2)
s.extra = 3
assert "extra" in s.__dict__
assert "a" not in s.__dict__
assert "b" not in s.__dict__
```
</details>
