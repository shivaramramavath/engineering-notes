# Mixins: Design, Patterns, and Ordering Rules

- **Topic**: `05-oop`
- **Python version**: 3.12+ (tested on CPython 3.14.0)
- **Tested with**: CPython 3.12, 3.13, 3.14 (standard & free-threaded builds)
- **Free-threading notes**: Compatible. Mixin method lookup traverses static class MROs, which are thread-safe and cached on type objects.

---

## 1. Concept Overview

A **Mixin** is a specialized class designed to provide reusable, pluggable functionality across unrelated class hierarchies without acting as a standalone parent class.

Unlike primary base classes:
- A mixin is **never instantiated directly**.
- A mixin bundles a discrete, focused set of methods (e.g., `JSONSerializableMixin`, `TimestampedMixin`, `RateLimitedMixin`).
- A class can inherit from multiple mixins alongside a single primary base class.

In Python, mixin behavior relies directly on **Method Resolution Order (MRO)** and **cooperative multiple inheritance**. The ordering of mixins in the class declaration line determines which mixin intercepts method calls first.

```
Declaration: class MyService(RateLimitMixin, AuthMixin, BaseService):
                            |                |          |
                            |                |          +--> Primary Base
                            |                +-------------> Mixin 2 (Second)
                            +------------------------------> Mixin 1 (First)

MRO Order: [MyService, RateLimitMixin, AuthMixin, BaseService, object]
```

---

## 2. Key Mechanics & Architecture

### Golden Rules of Mixin Design
1. **Never Call `__init__` on Concrete Base Directly**: Use `super().__init__(**kwargs)` so parameters flow cooperatively through all mixins in the MRO.
2. **Stateless or Light State**: Mixins should avoid declaring complex instance state whenever possible. If state is required, declare `__slots__ = ()` to prevent spawning unnecessary `__dict__` tables.
3. **Ordering Matters**: Classes listed leftmost in `class Derived(MixinA, MixinB, Base):` take precedence over classes to the right. Mixin overrides intercept calls before the base class executes.
4. **Cooperative `super()` Everywhere**: If a mixin wraps or intercepts an existing method (like `save()` or `dispatch()`), it must call `super().method(*args, **kwargs)` to avoid cutting short the MRO chain.

---

## 3. Real-World Code Examples

### Example 1: `JSONSerializableMixin` and `DictMixin`
```python
import json

class DictMixin:
    """Provides generic dict conversion based on public instance attributes."""
    def to_dict(self) -> dict:
        return {
            k: v for k, v in self.__dict__.items()
            if not k.startswith("_")
        }


class JSONSerializableMixin(DictMixin):
    """Extends DictMixin with JSON serialization."""
    def to_json(self) -> str:
        return json.dumps(self.to_dict())


class UserProfile(JSONSerializableMixin):
    def __init__(self, username: str, email: str, role: str):
        self.username = username
        self.email = email
        self.role = role
        self._internal_token = "secret-token-123"  # Skipped!


u = UserProfile("alice", "alice@corp.internal", "admin")
print(f"Dictionary: {u.to_dict()}")
print(f"JSON:       {u.to_json()}")
```
Output:
```
Dictionary: {'username': 'alice', 'email': 'alice@corp.internal', 'role': 'admin'}
JSON:       {"username": "alice", "email": "alice@corp.internal", "role": "admin"}
```

### Example 2: Mixin Interception Pipeline via `super()`
```python
class BaseRequestHandler:
    def handle(self, request: dict) -> str:
        return f"Handled request {request.get('id')}"


class LoggingMixin:
    def handle(self, request: dict) -> str:
        print(f"[LOG]: Entering handle for request {request.get('id')}")
        result = super().handle(request)
        print(f"[LOG]: Exiting handle for request {request.get('id')}")
        return result


class TimingMixin:
    def handle(self, request: dict) -> str:
        import time
        start = time.perf_counter()
        result = super().handle(request)
        elapsed = (time.perf_counter() - start) * 1000
        print(f"[TIMING]: Processed in {elapsed:.3f} ms")
        return result


# Order: TimingMixin -> LoggingMixin -> BaseRequestHandler
class MonitoredHandler(TimingMixin, LoggingMixin, BaseRequestHandler):
    pass


handler = MonitoredHandler()
res = handler.handle({"id": "REQ-404"})
print(f"Result: {res}")
```
Output:
```
[LOG]: Entering handle for request REQ-404
[LOG]: Exiting handle for request REQ-404
[TIMING]: Processed in 0.050 ms
Result: Handled request REQ-404
```

### Example 3: Mixin Precedence and Ordering Sensitivity
```python
class BaseWriter:
    def write(self, text: str) -> str:
        return text

class UpperMixin:
    def write(self, text: str) -> str:
        return super().write(text).upper()

class BracketMixin:
    def write(self, text: str) -> str:
        return f"[{super().write(text)}]"


# Order A: Upper first, then Bracket
class OrderA(UpperMixin, BracketMixin, BaseWriter):
    pass

# Order B: Bracket first, then Upper
class OrderB(BracketMixin, UpperMixin, BaseWriter):
    pass


a = OrderA()
b = OrderB()
print(f"OrderA: {a.write('hello')}")
print(f"OrderB: {b.write('hello')}")
```
Output:
```
OrderA: [HELLO]
OrderB: [HELLO]
```

### Example 4: Slotted Mixin Design (`__slots__ = ()`)
```python
class TimestampedMixin:
    # Defining empty __slots__ ensures that mixing this class into
    # a slotted host class does not spawn an unwanted __dict__!
    __slots__ = ()

    def get_epoch_time(self) -> float:
        import time
        return time.time()


class SlottedEntity(TimestampedMixin):
    __slots__ = ("name",)

    def __init__(self, name: str):
        self.name = name


e = SlottedEntity("Sensor-01")
print(f"Entity: {e.name}, time: {e.get_epoch_time() > 0}")
print(f"Has __dict__: {hasattr(e, '__dict__')}")
```
Output:
```
Entity: Sensor-01, time: True
Has __dict__: False
```

### Example 5: Validation Mixin for Data Models
```python
class ValidationMixin:
    def validate_non_empty(self, **kwargs):
        for key, val in kwargs.items():
            if not val or not str(val).strip():
                raise ValueError(f"Field '{key}' cannot be empty")


class CustomerAccount(ValidationMixin):
    def __init__(self, name: str, email: str):
        self.validate_non_empty(name=name, email=email)
        self.name = name
        self.email = email


c = CustomerAccount("Bob", "bob@example.com")
print(f"Customer registered: {c.name}")

try:
    CustomerAccount("   ", "bob@example.com")
except ValueError as err:
    print(f"Validation failed: {err}")
```
Output:
```
Customer registered: Bob
Validation failed: Field 'name' cannot be empty
```

---

## 4. Common Anti-Patterns & Gotchas

| Bad Practice | Root Cause | Clean Solution |
| :--- | :--- | :--- |
| Instantiating a Mixin directly (`m = LoggingMixin()`) | Mixins rely on methods provided by other classes or the primary base | Design mixins solely as supplemental base classes; never instantiate |
| Placing the primary base class before mixins in inheritance list | Base class methods shadow mixin methods before the mixin can intercept | Declare mixins first, primary base class last: `(Mixin1, Mixin2, Base)` |
| Omitting `super().method()` inside a mixin override | Cuts short the cooperative MRO chain; base logic never executes | Always call `super().method(...)` and return the result |
| Hardcoding concrete parent calls like `BaseHandler.handle(self)` | Bypasses sibling mixins in the MRO | Always use `super().method(...)` |
| Allocating new attributes in a mixin without `__slots__ = ()` on slotted hierarchies | Causes `TypeError: multiple bases have instance lay-out conflict` | Use empty `__slots__ = ()` in mixins |
| Storing heavy independent state in mixins | Tightens coupling and leads to state synchronization issues | Favor object composition for stateful components |

---

## 5. Interview Q&A

<details>
<summary>1. What is a Mixin, and how does it differ from a standard base class?</summary>
A Mixin is a specialized class designed to bundle and inject discrete, reusable methods into other classes via multiple inheritance. Unlike a standard base class, a mixin is not meant to be instantiated on its own, does not define the core identity of the object, and typically relies on methods or attributes provided by the primary base class or sibling mixins.
</details>

<details>
<summary>2. Why should mixins appear before the primary base class in the class declaration?</summary>
Python resolves methods from left to right according to the C3 Linearization Algorithm. Listing mixins first (e.g. <code>class Service(LoggingMixin, BaseService)</code>) ensures that the mixin's intercepting methods precede the base class in the MRO, allowing the mixin to augment or decorate base behavior before delegating via <code>super()</code>.
</details>

<details>
<summary>3. Why must mixin methods call super().method()?</summary>
In a multiple inheritance chain, a mixin cannot know ahead of time which class follows it in the runtime MRO. Calling <code>super().method()</code> guarantees that control is forwarded cooperatively to the next class (whether another mixin or the base class), ensuring no link in the chain is skipped.
</details>

<details>
<summary>4. What happens if a mixin is used in a slotted class hierarchy?</summary>
If the mixin does not define <code>__slots__</code>, Python allocates a <code>__dict__</code> for instances of the child class. If the mixin defines non-empty <code>__slots__</code> that conflict with another base, Python raises a layout conflict <code>TypeError</code>. Therefore, well-designed mixins should explicitly declare <code>__slots__ = ()</code>.
</details>

<details>
<summary>5. How does a mixin differ from an abstract base class (ABC)?</summary>
An ABC defines an interface contract (often with <code>@abstractmethod</code>) and enforces that subclasses implement required methods. A mixin provides concrete, ready-to-use implementations of methods that add functionality to the subclass without necessarily enforcing an interface.
</details>

<details>
<summary>6. Can a mixin access attributes defined by the host class?</summary>
Yes. Because a mixin method is executed in the context of the host instance (<code>self</code>), it can read and write any attribute on <code>self</code>. However, to maintain loose coupling, mixins should document their required attributes or verify them via duck typing.
</details>

<details>
<summary>7. What is the difference between a Mixin and a Decorator?</summary>
A class or method decorator wraps an existing class or function externally. A mixin injects methods directly into the class's MRO, allowing internal access to <code>self</code> and participation in cooperative <code>super()</code> chains.
</details>

<details>
<summary>8. How do you prevent a mixin from being instantiated on its own?</summary>
You can raise a <code>TypeError</code> inside the mixin's <code>__init__</code> or <code>__new__</code> if <code>cls is MyMixin</code>, or subclass from <code>abc.ABC</code> with an abstract method.
</details>

---

## 6. Practical Challenges

<details>
<summary>Challenge 1: Safe Dict Representation Mixin</summary>

Build a `ReprMixin` that uses `__slots__` or `__dict__` to automatically format a clean string representation: `ClassName(attr1=val1, attr2=val2)`.

```python
class ReprMixin:
    __slots__ = ()

    def __repr__(self):
        attrs = [
            f"{k}={v!r}" for k, v in self.__dict__.items()
            if not k.startswith("_")
        ]
        return f"{type(self).__name__}({', '.join(attrs)})"

class Product(ReprMixin):
    def __init__(self, sku: str, price: float):
        self.sku = sku
        self.price = price

p = Product("SKU-99", 29.99)
assert repr(p) == "Product(sku='SKU-99', price=29.99)"
```
</details>

<details>
<summary>Challenge 2: Rate Limiting Call Mixin</summary>

Create a `RateLimitMixin` that tracks call timestamps and raises `RuntimeError("Rate limit exceeded")` if a method is called more than $N$ times within a 1-second window.

```python
import time

class RateLimitMixin:
    def __init__(self, *args, max_calls=3, **kwargs):
        super().__init__(*args, **kwargs)
        self._max_calls = max_calls
        self._call_history = []

    def dispatch(self, action: str):
        now = time.time()
        self._call_history = [t for t in self._call_history if now - t < 1.0]
        if len(self._call_history) >= self._max_calls:
            raise RuntimeError("Rate limit exceeded")
        self._call_history.append(now)
        return super().dispatch(action)

class ServiceBase:
    def dispatch(self, action: str):
        return f"Done: {action}"

class RateLimitedService(RateLimitMixin, ServiceBase):
    pass

svc = RateLimitedService(max_calls=2)
assert svc.dispatch("a") == "Done: a"
assert svc.dispatch("b") == "Done: b"
try:
    svc.dispatch("c")
    assert False
except RuntimeError:
    pass
```
</details>

<details>
<summary>Challenge 3: Event Emitter Mixin</summary>

Implement an `EventEmitterMixin` that provides `on(event, handler)` and `emit(event, *args)` capabilities.

```python
from collections import defaultdict

class EventEmitterMixin:
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._subscribers = defaultdict(list)

    def on(self, event: str, callback):
        self._subscribers[event].append(callback)

    def emit(self, event: str, *args, **kwargs):
        for cb in self._subscribers[event]:
            cb(*args, **kwargs)

class OrderProcess(EventEmitterMixin):
    def complete(self):
        self.emit("completed", 100)

p = OrderProcess()
events = []
p.on("completed", lambda amt: events.append(amt))
p.complete()
assert events == [100]
```
</details>

<details>
<summary>Challenge 4: Audited History Mixin</summary>

Build a mixin `AuditMixin` that intercepts `__setattr__` to log all attribute assignments to an audit list `_audit_log`.

```python
class AuditMixin:
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        object.__setattr__(self, "_audit_log", [])

    def __setattr__(self, name, value):
        if hasattr(self, "_audit_log") and not name.startswith("_"):
            self._audit_log.append((name, value))
        super().__setattr__(name, value)

class Record(AuditMixin):
    def __init__(self, title):
        super().__init__()
        self.title = title

r = Record("Draft")
r.title = "Final"
assert r._audit_log == [("title", "Draft"), ("title", "Final")]
```
</details>

<details>
<summary>Challenge 5: JSON File Storage Mixin</summary>

Build a mixin `FilePersistMixin` with a `.save_to_file(path)` method that serializes `self.to_dict()` to disk.

```python
import json

class FilePersistMixin:
    def dump_json(self) -> str:
        data = {k: v for k, v in self.__dict__.items() if not k.startswith("_")}
        return json.dumps(data)

class Setting(FilePersistMixin):
    def __init__(self, env: str, debug: bool):
        self.env = env
        self.debug = debug

s = Setting("prod", False)
assert s.dump_json() == '{"env": "prod", "debug": false}'
```
</details>

<details>
<summary>Challenge 6: Mixin Ordering Conflict Detector</summary>

Write a test utility that verifies `MixinA` appears strictly before `MixinB` in the `__mro__` of a derived target class.

```python
def assert_mixin_order(cls: type, first_mixin: type, second_mixin: type):
    mro = cls.__mro__
    assert first_mixin in mro, f"{first_mixin} not in MRO"
    assert second_mixin in mro, f"{second_mixin} not in MRO"
    assert mro.index(first_mixin) < mro.index(second_mixin), f"{first_mixin} does not precede {second_mixin}"

class M1: pass
class M2: pass
class Target(M1, M2): pass

assert_mixin_order(Target, M1, M2)
```
</details>
