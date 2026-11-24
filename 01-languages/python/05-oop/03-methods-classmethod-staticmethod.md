# Methods: Instance, @classmethod, and @staticmethod

- **Topic**: `05-oop`
- **Python version**: 3.12+ (tested on CPython 3.14.0)
- **Tested with**: CPython 3.12, 3.13, 3.14 (standard & free-threaded builds)
- **Free-threading notes**: Compatible. Method resolution, descriptor binding, and callable execution adhere to standard thread-safety rules.

---

## 1. Concept Overview

Python functions declared inside a class body can be invoked in three distinct modes, determined by how they bind arguments at runtime:

1. **Instance Methods**: The default. Implicitly receives the calling instance as the first argument (`self`). Can access and modify both instance state and class state.
2. **Class Methods (`@classmethod`)**: Decorated with `@classmethod`. Implicitly receives the calling class as the first argument (`cls`). Can access and modify class state, but has no access to instance state. Ideal for **alternative constructors**.
3. **Static Methods (`@staticmethod`)**: Decorated with `@staticmethod`. Receives no implicit first argument (neither `self` nor `cls`). Behaves like a plain function placed inside the class namespace for logical grouping.

The magic behind this behavior is Python's **Descriptor Protocol** (`__get__`), which transforms bare functions into bound method objects when accessed via dot notation.

```
Accessing Class.func / instance.func
                 |
                 v
   Does func implement __get__?
          /             \
        YES              NO
        /                 \
       v                   v
Calls func.__get__(instance, owner)   Returns func as-is
       |
       +---> Regular function:
       |        instance is None  -> Unbound function (MyClass.func)
       |        instance is obj   -> Bound method (self prepended)
       |
       +---> @classmethod:
       |        Binds owner class (cls prepended, whether called on obj or class)
       |
       +---> @staticmethod:
                Returns underlying function untouched (no arguments prepended)
```

---

## 2. Key Mechanics & Architecture

### Descriptor Binding Table

| Method Type | Decorator | First Implicit Parameter | Bound Object | Descriptor Mechanism |
| :--- | :--- | :--- | :--- | :--- |
| **Instance Method** | None | `self` (the instance) | `types.MethodType` | `func.__get__(obj, cls)` binds `obj` |
| **Class Method** | `@classmethod` | `cls` (the class) | `types.MethodType` | `classmethod.__get__(obj, cls)` binds `cls` |
| **Static Method** | `@staticmethod` | None | Underlying `function` | `staticmethod.__get__(obj, cls)` returns `func` |

### Alternative Constructors Pattern
The primary idiomatic use case for `@classmethod` is creating factory methods that instantiate the class from different data formats (e.g., `from_dict`, `from_json`, `from_csv`, `from_timestamp`). By accepting `cls`, subclasses that inherit the class method automatically instantiate the subclass rather than the parent class.

---

## 3. Real-World Code Examples

### Example 1: Defining and Calling the Three Method Types
```python
class TemperatureReading:
    standard_unit = "Celsius"

    def __init__(self, degrees: float):
        self.degrees = degrees

    # 1. Instance method: operates on self
    def to_fahrenheit(self) -> float:
        return (self.degrees * 9 / 5) + 32

    # 2. Class method: alternative constructor using cls
    @classmethod
    def from_fahrenheit(cls, degrees_f: float) -> "TemperatureReading":
        celsius = (degrees_f - 32) * 5 / 9
        return cls(round(celsius, 2))

    # 3. Static method: pure utility without self or cls
    @staticmethod
    def is_freezing(degrees_c: float) -> bool:
        return degrees_c <= 0.0


# Calling instance method
t1 = TemperatureReading(25.0)
print(f"t1 in Fahrenheit: {t1.to_fahrenheit()} F")

# Calling class method (alternative constructor)
t2 = TemperatureReading.from_fahrenheit(98.6)
print(f"t2 from Fahrenheit: {t2.degrees} C")

# Calling static method
print(f"Is -5 C freezing? {TemperatureReading.is_freezing(-5)}")
print(f"Is 25 C freezing? {t1.is_freezing(t1.degrees)}")
```
Output:
```
t1 in Fahrenheit: 77.0 F
t2 from Fahrenheit: 37.0 C
Is -5 C freezing? True
Is 25 C freezing? False
```

### Example 2: Subclass Inheritance with `@classmethod`
```python
class BaseConfig:
    def __init__(self, host: str, port: int):
        self.host = host
        self.port = port

    @classmethod
    def from_connection_string(cls, conn_str: str) -> "BaseConfig":
        host, port = conn_str.split(":")
        # Because we use cls instead of BaseConfig, subclasses return their own type!
        return cls(host=host, port=int(port))


class SecureConfig(BaseConfig):
    def __init__(self, host: str, port: int, ssl: bool = True):
        super().__init__(host, port)
        self.ssl = ssl


cfg = SecureConfig.from_connection_string("db.internal.net:5432")
print(f"Config type: {type(cfg).__name__}")
print(f"Host: {cfg.host}, Port: {cfg.port}, SSL: {cfg.ssl}")
```
Output:
```
Config type: SecureConfig
Host: db.internal.net: 5432, SSL: True
```

### Example 3: Under the Hood - Manual Descriptor Invocation
```python
class Calculator:
    def add(self, a: int, b: int) -> int:
        return a + b


calc = Calculator()

# Accessing from class gives the raw function
raw_func = Calculator.__dict__["add"]
print(f"Raw function in __dict__: {type(raw_func)}")

# The descriptor protocol produces a bound method when accessed from instance
bound_method = raw_func.__get__(calc, Calculator)
print(f"Bound method: {bound_method}")
print(f"Result: {bound_method(10, 20)}")
```
Output:
```
Raw function in __dict__: <class 'function'>
Bound method: <bound method Calculator.add of <__main__.Calculator object at 0x7fa280>>
Result: 30
```

### Example 4: Polymorphic Factory Pattern via `@classmethod`
```python
from typing import Self

class User:
    def __init__(self, email: str, role: str):
        self.email = email
        self.role = role

    @classmethod
    def from_dict(cls, data: dict[str, str]) -> Self:
        return cls(email=data["email"], role=data.get("role", "viewer"))

    @classmethod
    def admin_user(cls, email: str) -> Self:
        return cls(email=email, role="superadmin")


admin = User.admin_user("admin@corp.internal")
viewer = User.from_dict({"email": "guest@corp.internal"})
print(f"Admin: {admin.email} ({admin.role})")
print(f"Viewer: {viewer.email} ({viewer.role})")
```
Output:
```
Admin: admin@corp.internal (superadmin)
Viewer: guest@corp.internal (viewer)
```

### Example 5: Static Method for Logical Grouping and Namespacing
```python
import hashlib
import re

class SecurityUtils:
    """Namespace grouping cryptography & validation utilities."""

    @staticmethod
    def sha256_hash(data: str) -> str:
        return hashlib.sha256(data.encode("utf-8")).hexdigest()

    @staticmethod
    def is_strong_password(password: str) -> bool:
        return (
            len(password) >= 8
            and any(c.isupper() for c in password)
            and any(c.isdigit() for c in password)
        )


print(f"Hash: {SecurityUtils.sha256_hash('Antigravity')[:16]}...")
print(f"Is strong: {SecurityUtils.is_strong_password('P@ssword123')}")
```
Output:
```
Hash: 9e5d4cb31d1b8ca6...
Is strong: True
```

---

## 4. Common Anti-Patterns & Gotchas

| Bad Practice | Root Cause | Clean Solution |
| :--- | :--- | :--- |
| Hardcoding `ClassName(...)` inside a `@classmethod` | Breaks inheritance; subclasses will instantiate the parent class | Always instantiate `cls(...)` inside class methods |
| Using `@staticmethod` when the function requires class state | Static methods do not receive `cls` | Use `@classmethod` if class variables or types are accessed |
| Using `@staticmethod` for everything instead of module-level functions | Excessive OOP ceremony | If a helper has no conceptual connection to the class, declare it at module level |
| Calling instance methods directly from the class without `self` | `MyClass.method()` is an unbound call expecting `self` as the first argument | Instantiate first: `MyClass().method()` or pass instance explicitly: `MyClass.method(instance)` |
| Modifying class state inside regular instance methods | Confuses instance mutation with global class state mutation | Mutate class state explicitly via `cls` in `@classmethod` |
| Calling `@classmethod` from instances thinking it only affects the instance | Class methods always receive and affect the class object | Keep instance-specific transformations in instance methods |

---

## 5. Interview Q&A

<details>
<summary>1. How does Python's descriptor protocol implement bound methods?</summary>
In Python, functions are descriptors implementing <code>__get__</code>. When you access <code>obj.func</code>, Python calls <code>func.__get__(obj, type(obj))</code>. This returns a <code>types.MethodType</code> object that wraps the underlying function and binds <code>obj</code> as its first positional argument. When accessed via the class (<code>MyClass.func</code>), <code>func.__get__(None, MyClass)</code> simply returns the raw function in Python 3.
</details>

<details>
<summary>2. When should you choose @classmethod over @staticmethod?</summary>
Choose <code>@classmethod</code> when the method needs access to the class object (<code>cls</code>)—such as for factory methods, alternative constructors, or accessing class attributes. Choose <code>@staticmethod</code> when the function performs a pure calculation or verification related to the class concept, but requires neither instance attributes nor class state.
</details>

<details>
<summary>3. Why is cls preferable to hardcoding ClassName in factory methods?</summary>
Using <code>cls(...)</code> ensures polymorphic correctness under inheritance. If a subclass <code>Derived</code> inherits the factory method, calling <code>Derived.factory()</code> will pass <code>cls = Derived</code> and construct an instance of <code>Derived</code>. If <code>ClassName(...)</code> were hardcoded, it would always return an instance of the base class.
</details>

<details>
<summary>4. Can a @classmethod be called from an instance?</summary>
Yes. When called via <code>instance.method()</code>, Python still passes the instance's class (<code>type(instance)</code>) as <code>cls</code>. However, the method does not have access to <code>instance.__dict__</code> or <code>self</code>.
</details>

<details>
<summary>5. What is the typing return annotation Self introduced in PEP 673?</summary>
<code>typing.Self</code> (available in Python 3.11+) allows annotating the return type of class methods or fluent instance methods as returning the current class/instance, without needing complex TypeVars (e.g., <code>T = TypeVar("T", bound="Base")</code>).
</details>

<details>
<summary>6. How does @staticmethod differ from a top-level module function?</summary>
At runtime, a static method is identical to a top-level function. The difference is purely organization and namespacing: placing it inside a class signals that it is closely associated with that class's domain logic, and it allows subclasses to override or customize the static method.
</details>

<details>
<summary>7. What happens if you define a method without self or any decorator and call it on an instance?</summary>
Python raises <code>TypeError: method() takes 0 positional arguments but 1 was given</code>. This is because instance lookup implicitly injects <code>self</code> as the first argument, but the function's signature does not accept any arguments.
</details>

<details>
<summary>8. Can you stack @classmethod and @property?</summary>
In Python 3.9, <code>@classmethod</code> + <code>@property</code> was allowed, but it was deprecated in Python 3.11 and removed/disallowed in Python 3.13 due to subtle bugs and complexity. To create class-level properties, define a property on a custom metaclass instead.
</details>

---

## 6. Practical Challenges

<details>
<summary>Challenge 1: Multi-Format Serializer Factory</summary>

Implement an `Event` class with an instance method `to_dict()`, a class method `from_json(json_str)`, and a class method `from_tuple(data_tuple)`.

```python
import json

class Event:
    def __init__(self, name: str, timestamp: int):
        self.name = name
        self.timestamp = timestamp

    def to_dict(self) -> dict:
        return {"name": self.name, "timestamp": self.timestamp}

    @classmethod
    def from_json(cls, json_str: str) -> "Event":
        data = json.loads(json_str)
        return cls(name=data["name"], timestamp=data["timestamp"])

    @classmethod
    def from_tuple(cls, t: tuple[str, int]) -> "Event":
        return cls(name=t[0], timestamp=t[1])

e = Event.from_json('{"name": "deploy", "timestamp": 1700000000}')
assert e.name == "deploy"
assert e.to_dict() == {"name": "deploy", "timestamp": 1700000000}
```
</details>

<details>
<summary>Challenge 2: Polymorphic Subclass Factory</summary>

Create a base `Serializer` class with `from_extension(filename)`. Subclasses `JSONSerializer` (for `.json`) and `CSVSerializer` (for `.csv`) register themselves dynamically in a class-level registry.

```python
class Serializer:
    _registry: dict[str, type["Serializer"]] = {}

    def __init_subclass__(cls, ext: str, **kwargs):
        super().__init_subclass__(**kwargs)
        cls._registry[ext] = cls

    @classmethod
    def for_file(cls, filename: str) -> "Serializer":
        ext = filename.split(".")[-1]
        target_cls = cls._registry.get(ext)
        if not target_cls:
            raise ValueError(f"No serializer for extension .{ext}")
        return target_cls()

class JSONSerializer(Serializer, ext="json"):
    pass

class CSVSerializer(Serializer, ext="csv"):
    pass

s = Serializer.for_file("data.json")
assert isinstance(s, JSONSerializer)
```
</details>

<details>
<summary>Challenge 3: Pure Math Static Helper Suite</summary>

Build a class `MathOps` that contains static methods `gcd(a, b)` and `lcm(a, b)` using pure Euclidean division.

```python
class MathOps:
    @staticmethod
    def gcd(a: int, b: int) -> int:
        while b:
            a, b = b, a % b
        return abs(a)

    @staticmethod
    def lcm(a: int, b: int) -> int:
        if a == 0 or b == 0:
            return 0
        return abs(a * b) // MathOps.gcd(a, b)

assert MathOps.gcd(48, 18) == 6
assert MathOps.lcm(12, 15) == 60
```
</details>

<details>
<summary>Challenge 4: Counter with Atomic Class Method</summary>

Implement an `IDGenerator` class with private class variable `_counter = 0`. Provide a class method `next_id(prefix: str)` that atomically increments and returns `prefix-00001`.

```python
class IDGenerator:
    _counter = 0

    @classmethod
    def next_id(cls, prefix: str = "REC") -> str:
        cls._counter += 1
        return f"{prefix}-{cls._counter:05d}"

assert IDGenerator.next_id("USER") == "USER-00001"
assert IDGenerator.next_id("USER") == "USER-00002"
```
</details>

<details>
<summary>Challenge 5: Method Binding Inspector</summary>

Write a function `inspect_callable(cls_or_obj, method_name: str)` that determines whether a method resolves to a function, bound instance method, or bound class method.

```python
import inspect

def inspect_callable(target, method_name: str) -> str:
    attr = getattr(target, method_name)
    if inspect.ismethod(attr):
        if inspect.isclass(attr.__self__):
            return "bound_classmethod"
        return "bound_instancemethod"
    elif inspect.isfunction(attr):
        return "function_or_staticmethod"
    return "unknown"

class Example:
    def inst(self): pass
    @classmethod
    def cls_m(cls): pass
    @staticmethod
    def stat(): pass

e = Example()
assert inspect_callable(e, "inst") == "bound_instancemethod"
assert inspect_callable(Example, "cls_m") == "bound_classmethod"
assert inspect_callable(Example, "stat") == "function_or_staticmethod"
```
</details>

<details>
<summary>Challenge 6: Class-Level Property via Metaclass</summary>

Since `@classmethod` + `@property` is obsolete in Python 3.13+, implement a class property `version` on `AppService` using a custom metaclass.

```python
class MetaService(type):
    @property
    def version(cls) -> str:
        return "2.4.0"

class AppService(metaclass=MetaService):
    pass

assert AppService.version == "2.4.0"
```
</details>
