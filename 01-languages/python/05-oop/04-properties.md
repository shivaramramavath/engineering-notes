# Properties: @property, Setters, Deleters, and Cached Properties

- **Topic**: `05-oop`
- **Python version**: 3.12+ (tested on CPython 3.14.0)
- **Tested with**: CPython 3.12, 3.13, 3.14 (standard & free-threaded builds)
- **Free-threading notes**: Compatible. Property getter/setter execution adheres to normal thread semantics. In free-threaded Python, `@cached_property` uses a per-instance lock to ensure thread-safe memoization.

---

## 1. Concept Overview

In Python, the `@property` decorator transforms method calls into attribute access syntax. This allows developers to maintain a clean, public attribute interface while enforcing encapsulation, validation, computed values, and lazy evaluation behind the scenes—without breaking backward compatibility.

A property consists of three optional components:
1. **Getter (`@property`)**: Invoked when accessing `obj.attr`.
2. **Setter (`@attr.setter`)**: Invoked when assigning `obj.attr = val`. Enforces data integrity.
3. **Deleter (`@attr.deleter`)**: Invoked when deleting `del obj.attr`. Cleans up resources.

In addition, standard library `functools.cached_property` provides lazy evaluation: computing an expensive attribute once on first access and caching the result directly into the instance `__dict__`.

```
Direct Access:             Property Access:
obj.radius                 obj.radius
   |                          |
   v                          v
Read instance dict         Calls radius(self) getter function
                           Validates or computes dynamically!
```

---

## 2. Key Mechanics & Architecture

### How `@property` Operates as a Data Descriptor
In Python's attribute lookup rules, a **data descriptor** (an object defining `__set__` or `__delete__` in addition to `__get__`) takes strict precedence over an instance's `__dict__`.

Because the `property` built-in implements both `__get__` and `__set__`, writing `obj.attr = value` will **always** trigger the property setter rather than overwriting `obj.__dict__['attr']`. If a setter is not defined, attempting to assign to `obj.attr` raises `AttributeError: can't set attribute`.

```
Attribute Access: obj.attr
                 |
                 v
   Does Class have attr as Data Descriptor (property)?
             /           \
           YES            NO
           /                \
Calls property.__get__()    Checks obj.__dict__['attr']
```

### `@property` vs `functools.cached_property`
| Feature | `@property` | `functools.cached_property` |
| :--- | :--- | :--- |
| **Descriptor Type** | Data Descriptor (`__get__` and `__set__`) | Non-Data Descriptor (implements `__get__` only) |
| **Storage Location** | Does not cache; recomputes on every read | Caches result in `instance.__dict__` |
| **Overwritten by Dict?** | No; setter is required | Yes; once written to `__dict__`, `__dict__` wins |
| **Mutability** | Controlled via `@x.setter` | Mutable by default unless guarded |
| **Use Case** | Validation, real-time computations | Expensive calculations, lazy I/O |

---

## 3. Real-World Code Examples

### Example 1: Encapsulation and Data Validation with Getter & Setter
```python
class BankAccount:
    def __init__(self, owner: str, initial_balance: float):
        self.owner = owner
        self.balance = initial_balance  # Calls the setter!

    @property
    def balance(self) -> float:
        """Getter: returns current balance."""
        return self._balance

    @balance.setter
    def balance(self, value: float):
        """Setter: validates before updating private attribute."""
        if not isinstance(value, (int, float)):
            raise TypeError("Balance must be a numeric value")
        if value < 0:
            raise ValueError("Balance cannot be negative")
        self._balance = round(float(value), 2)

    @balance.deleter
    def balance(self):
        """Deleter: safeguards account closure."""
        print(f"Closing account for {self.owner}...")
        self._balance = 0.0


acc = BankAccount("Alice", 150.75)
print(f"Current balance: ${acc.balance}")
acc.balance = 200.50
print(f"Updated balance: ${acc.balance}")

try:
    acc.balance = -50
except ValueError as e:
    print(f"Validation caught: {e}")

del acc.balance
print(f"After deletion: ${acc.balance}")
```
Output:
```
Current balance: $150.75
Updated balance: $200.5
Validation caught: Balance cannot be negative
Closing account for Alice...
After deletion: $0.0
```

### Example 2: Read-Only Computed Property
```python
class Circle:
    def __init__(self, radius: float):
        self.radius = radius

    @property
    def area(self) -> float:
        # Dynamically computed on every access
        import math
        return round(math.pi * (self.radius ** 2), 4)


c = Circle(5.0)
print(f"Radius: {c.radius}, Area: {c.area}")

try:
    c.area = 100.0  # No setter defined!
except AttributeError as err:
    print(f"Expected error: {err}")
```
Output:
```
Radius: 5.0, Area: 78.5398
Expected error: property 'area' of 'Circle' object has no setter
```

### Example 3: Lazy Evaluation with `functools.cached_property`
```python
from functools import cached_property
import time

class DatasetAnalyzer:
    def __init__(self, data: list[int]):
        self.data = data

    @cached_property
    def stats_summary(self) -> dict[str, float]:
        print("Computing expensive statistics...")
        # Simulate heavy CPU processing
        total = sum(self.data)
        mean = total / len(self.data)
        variance = sum((x - mean) ** 2 for x in self.data) / len(self.data)
        return {"mean": mean, "variance": variance}


analyzer = DatasetAnalyzer([10, 20, 30, 40, 50])
print("1. Analyzer instantiated (property not computed yet)")
print(f"2. First access: {analyzer.stats_summary}")
print(f"3. Second access (cached): {analyzer.stats_summary}")
print(f"4. Keys in instance __dict__: {list(analyzer.__dict__.keys())}")
```
Output:
```
1. Analyzer instantiated (property not computed yet)
2. First access: Computing expensive statistics...
{'mean': 30.0, 'variance': 200.0}
3. Second access (cached): {'mean': 30.0, 'variance': 200.0}
4. Keys in instance __dict__: ['data', 'stats_summary']
```

### Example 4: Caching Invalidation Pattern
```python
from functools import cached_property

class Report:
    def __init__(self, title: str):
        self.title = title

    @cached_property
    def word_count(self) -> int:
        print("Calculating word count...")
        return len(self.title.split())

    def update_title(self, new_title: str):
        self.title = new_title
        # Invalidate the cache by deleting the key from __dict__
        self.__dict__.pop("word_count", None)


rep = Report("Modern Python Engineering Notes")
print(f"Count: {rep.word_count}")
print(f"Count: {rep.word_count}")  # Cached
rep.update_title("Advanced Python Internals and Systems Design")
print(f"Count after update: {rep.word_count}")  # Recomputed
```
Output:
```
Calculating word count...
Count: 4
Count: 4
Calculating word count...
Count after update: 5
```

### Example 5: Property Inheritance and Overriding
```python
class BaseWidget:
    def __init__(self, name: str):
        self._name = name

    @property
    def name(self) -> str:
        return self._name


class UppercaseWidget(BaseWidget):
    # Extending existing property getter
    @BaseWidget.name.getter
    def name(self) -> str:
        base_val = super().name
        return base_val.upper()


w = UppercaseWidget("dashboard")
print(f"Widget name: {w.name}")
```
Output:
```
Widget name: DASHBOARD
```

---

## 4. Common Anti-Patterns & Gotchas

| Bad Practice | Root Cause | Clean Solution |
| :--- | :--- | :--- |
| Writing infinite recursion in setter: `self.x = x` | Calling `self.x = ...` inside `def x(self, val):` invokes the setter recursively | Assign to a private backing attribute like `self._x = x` |
| Performing slow I/O inside standard `@property` getters | Getters are expected to be instantaneous attribute accesses | Use an explicit `fetch_*()` method or `@cached_property` |
| Trying to use `@cached_property` on classes with `__slots__` without `'__dict__'` | `@cached_property` stores its value inside `instance.__dict__` | Add `'__dict__'` to `__slots__` or use a custom caching descriptor |
| Omitting docstring on property | Documentation tools fail to extract property descriptions | Put docstring on getter; `@property` automatically copies it |
| Forgetting that `@cached_property` can be overwritten directly | Non-data descriptors defer to instance `__dict__` assignment | Write a custom read-only cached property if mutation must be prevented |
| Creating Java-style getters/setters (`get_x()`, `set_x()`) | Un-idiomatic Python; clutters the API unnecessarily | Expose public attributes directly; upgrade to `@property` when logic is needed |

---

## 5. Interview Q&A

<details>
<summary>1. How does Python's descriptor protocol make properties work?</summary>
The built-in <code>property</code> type implements the descriptor protocol methods: <code>__get__</code>, <code>__set__</code>, and <code>__delete__</code>. When <code>obj.x</code> is evaluated and <code>x</code> is a property on <code>type(obj)</code>, Python invokes <code>property.__get__(x, obj, type(obj))</code>, which in turn calls the wrapped getter method.
</details>

<details>
<summary>2. Why does obj.attr = 10 trigger the property setter instead of writing to obj.__dict__?</summary>
In CPython's attribute resolution order, <b>data descriptors</b> (descriptors that define <code>__set__</code> or <code>__delete__</code>) take priority over the instance's <code>__dict__</code>. Because <code>property</code> implements <code>__set__</code>, Python delegates attribute assignments to the property descriptor.
</details>

<details>
<summary>3. What is the fundamental difference between @property and functools.cached_property?</summary>
<code>@property</code> is a data descriptor that recalculates its return value on every access and does not write to the instance dictionary. <code>@cached_property</code> is a non-data descriptor that computes the value on first access, stores it directly in <code>obj.__dict__</code>, and subsequently lets standard dictionary lookup return the cached value.
</details>

<details>
<summary>4. What happens when you delete an attribute managed by a property?</summary>
If the property has a registered deleter (<code>@attr.deleter</code>), that method is invoked. If no deleter was specified, Python raises <code>AttributeError: can't delete attribute</code>.
</details>

<details>
<summary>5. Can @cached_property be used on classes with __slots__?</summary>
Only if <code>'__dict__'</code> is explicitly included in <code>__slots__</code>. Because <code>cached_property</code> stores cached values in <code>instance.__dict__</code>, attempting to access it on a slotted class without <code>__dict__</code> raises <code>TypeError: No '__dict__' attribute on 'ClassName' instance to cache 'prop' property</code>.
</details>

<details>
<summary>6. How do you override only the setter of a property in a subclass?</summary>
In the subclass, refer to the parent property:
<pre><code>class Child(Parent):
    @Parent.attr.setter
    def attr(self, value):
        ...
</code></pre>
This keeps the parent getter intact while redefining the setter.
</details>

<details>
<summary>7. Why should you avoid executing network requests inside a property getter?</summary>
Properties mimic plain attribute access syntactically. Developers and callers assume that attribute access is fast ($O(1)$) and side-effect free. Performing network I/O or heavy database queries in getters can lead to performance degradation and unhandled network errors during basic debugging or logging.
</details>

<details>
<summary>8. How do you implement a write-once (read-only after first assignment) property?</summary>
Define a property with a setter that checks whether the private backing attribute has already been assigned. If so, raise an <code>AttributeError</code> or <code>ValueError</code>.
</details>

---

## 6. Practical Challenges

<details>
<summary>Challenge 1: Validated Temperature with Multiple Units</summary>

Create a `Temperature` class storing degrees Kelvin internally (`_kelvin`). Provide properties `celsius` and `fahrenheit` with both getters and setters. Raise `ValueError` if set below absolute zero ($0\text{ K}$).

```python
class Temperature:
    def __init__(self, kelvin: float):
        self.kelvin = kelvin

    @property
    def kelvin(self) -> float:
        return self._kelvin

    @kelvin.setter
    def kelvin(self, val: float):
        if val < 0:
            raise ValueError("Temperature below absolute zero is impossible")
        self._kelvin = float(val)

    @property
    def celsius(self) -> float:
        return round(self._kelvin - 273.15, 2)

    @celsius.setter
    def celsius(self, val: float):
        self.kelvin = val + 273.15

t = Temperature(300)
assert t.celsius == 26.85
t.celsius = 0
assert round(t.kelvin, 2) == 273.15
```
</details>

<details>
<summary>Challenge 2: Write-Once / Immutable Attribute Guard</summary>

Implement an `AuditRecord` class where `record_id` can be set once during initialization or first assignment, but subsequent assignments raise `AttributeError`.

```python
class AuditRecord:
    def __init__(self, record_id: str):
        self.record_id = record_id

    @property
    def record_id(self) -> str:
        return self._record_id

    @record_id.setter
    def record_id(self, val: str):
        if hasattr(self, "_record_id"):
            raise AttributeError("record_id is immutable once set")
        self._record_id = val

r = AuditRecord("TX-100")
assert r.record_id == "TX-100"
try:
    r.record_id = "TX-200"
    assert False, "Should raise AttributeError"
except AttributeError:
    pass
```
</details>

<details>
<summary>Challenge 3: Expiring Cached Property</summary>

Implement a custom descriptor `@timed_cached_property(ttl_seconds)` that caches the computed result for a specified time-to-live (TTL).

```python
import time

class timed_cached_property:
    def __init__(self, ttl_seconds: float):
        self.ttl = ttl_seconds

    def __call__(self, func):
        self.func = func
        self.name = func.__name__
        return self

    def __get__(self, instance, owner=None):
        if instance is None:
            return self
        now = time.time()
        cache_key = f"_{self.name}_cache"
        time_key = f"_{self.name}_time"
        if hasattr(instance, cache_key):
            last_time = getattr(instance, time_key)
            if now - last_time < self.ttl:
                return getattr(instance, cache_key)
        val = self.func(instance)
        setattr(instance, cache_key, val)
        setattr(instance, time_key, now)
        return val

class Sensor:
    count = 0
    @timed_cached_property(ttl_seconds=1.0)
    def reading(self) -> int:
        self.count += 1
        return self.count

s = Sensor()
assert s.reading == 1
assert s.reading == 1  # Cached!
```
</details>

<details>
<summary>Challenge 4: Dynamic Deprecation Warning Property</summary>

Build a property wrapper `deprecated_property(old_name, new_name)` that issues a `DeprecationWarning` when the legacy attribute is accessed or assigned.

```python
import warnings

def deprecated_property(new_name: str):
    def getter(self):
        warnings.warn(f"Property is deprecated; use {new_name}", DeprecationWarning, stacklevel=2)
        return getattr(self, new_name)
    def setter(self, val):
        warnings.warn(f"Property is deprecated; use {new_name}", DeprecationWarning, stacklevel=2)
        setattr(self, new_name, val)
    return property(getter, setter)

class UserAccount:
    def __init__(self, full_name: str):
        self.display_name = full_name

    name = deprecated_property("display_name")

u = UserAccount("Alice")
with warnings.catch_warnings(record=True) as w:
    warnings.simplefilter("always")
    assert u.name == "Alice"
    assert len(w) == 1
    assert issubclass(w[-1].category, DeprecationWarning)
```
</details>

<details>
<summary>Challenge 5: Type-Enforced Property Factory</summary>

Create a helper function `typed_property(name, expected_type)` that dynamically returns a property enforcing strict type checks.

```python
def typed_property(name: str, expected_type: type):
    storage_name = f"_{name}"
    def getter(self):
        return getattr(self, storage_name)
    def setter(self, value):
        if not isinstance(value, expected_type):
            raise TypeError(f"'{name}' must be of type {expected_type.__name__}, got {type(value).__name__}")
        setattr(self, storage_name, value)
    return property(getter, setter)

class Profile:
    age = typed_property("age", int)

p = Profile()
p.age = 30
assert p.age == 30
try:
    p.age = "thirty"
    assert False
except TypeError:
    pass
```
</details>

<details>
<summary>Challenge 6: Invalidate Dependent Cached Properties</summary>

Build a class `Invoice` with items `list[float]`. Provide `@cached_property` for `subtotal` and `total_with_tax`. When `add_item()` is called, systematically purge both cached values.

```python
from functools import cached_property

class Invoice:
    TAX_RATE = 0.10

    def __init__(self):
        self.items: list[float] = []

    def add_item(self, price: float):
        self.items.append(price)
        self.__dict__.pop("subtotal", None)
        self.__dict__.pop("total_with_tax", None)

    @cached_property
    def subtotal(self) -> float:
        return sum(self.items)

    @cached_property
    def total_with_tax(self) -> float:
        return round(self.subtotal * (1 + self.TAX_RATE), 2)

inv = Invoice()
inv.add_item(100.0)
assert inv.subtotal == 100.0
assert inv.total_with_tax == 110.0
inv.add_item(50.0)
assert inv.subtotal == 150.0
assert inv.total_with_tax == 165.0
```
</details>
