# Composition vs Inheritance: "Favor Object Composition"

- **Topic**: `05-oop`
- **Python version**: 3.12+ (tested on CPython 3.14.0)
- **Tested with**: CPython 3.12, 3.13, 3.14 (standard & free-threaded builds)
- **Free-threading notes**: Compatible. Delegated method invocation and composite object references follow standard thread-safety rules.

---

## 1. Concept Overview

A foundational principle of object-oriented design (articulated by the Gang of Four) states:
> *"Favor object composition over class inheritance."*

- **Inheritance ("is-a" relationship)**: Creates tight compile-time coupling between a parent class and child class. Any modification to the base class cascades unpredictably down the hierarchy (the *Fragile Base Class problem*). Subclasses inherit all methods—even those that make no sense in the child context.
- **Composition ("has-a" relationship)**: Combines small, independent, specialized objects as attributes inside a parent object. Behavior is assembled dynamically at runtime through **delegation**.

In Python, composition is vastly more flexible than deep inheritance trees. It enables hot-swapping strategies, mocking components in unit tests, and adhering strictly to the Single Responsibility Principle.

```
Inheritance ("is-a" - Rigid):
       Animal
         ^
         |
        Dog
         ^
         |
    WorkingDog  (Tightly coupled, fragile)

Composition ("has-a" - Flexible):
       Robot
         |── Engine (swappable: Electric, Diesel)
         |── Sensor (swappable: Lidar, Ultrasonic)
         └── Arm    (swappable: Gripper, Welder)
```

---

## 2. Key Mechanics & Architecture

### Comparison Matrix

| Dimension | Class Inheritance ("is-a") | Object Composition ("has-a") |
| :--- | :--- | :--- |
| **Coupling** | Tight (white-box reuse; internals exposed) | Loose (black-box reuse; only public API exposed) |
| **Flexibility** | Static (fixed at class definition time) | Dynamic (components can be swapped at runtime) |
| **Code Smells** | Deep hierarchies, Liskov Substitution violations | Boilerplate method forwarding (mitigated via `__getattr__`) |
| **Testing** | Difficult to isolate unit behavior | Trivially mocked using duck typing or Protocols |
| **Use When** | True semantic subtype with full behavioral compatibility | Sharing capabilities across unrelated objects or varying behavior |

### Transparent Delegation via `__getattr__`
When composing objects, forwarding every single method manually can create repetitive boilerplate. In Python, the `__getattr__` fallback hook allows transparent delegation: if an attribute is not found on the wrapper, Python routes the lookup to the wrapped component.

---

## 3. Real-World Code Examples

### Example 1: The Fragile Base Class Pitfall
```python
# Bad: Subclassing built-in list exposes unwanted mutating methods
class BadCustomStack(list):
    def push(self, item):
        self.append(item)
    # The stack exposes .insert(), .sort(), .reverse(), violating stack invariants!

# Good: Composition encapsulates internal list
class ComposedStack:
    def __init__(self):
        self._items = []

    def push(self, item):
        self._items.append(item)

    def pop(self):
        if not self._items:
            raise IndexError("pop from empty stack")
        return self._items.pop()

    def peek(self):
        return self._items[-1] if self._items else None

    def __len__(self):
        return len(self._items)


stack = ComposedStack()
stack.push("a")
stack.push("b")
print(f"Stack size: {len(stack)}, pop: {stack.pop()}")
# stack.insert(0, "x")  -> AttributeError! Safe from corruption.
```
Output:
```
Stack size: 2, pop: b
```

### Example 2: Strategy Pattern via Composition
```python
from typing import Protocol

class CompressionStrategy(Protocol):
    def compress(self, data: bytes) -> bytes: ...


class ZipCompression:
    def compress(self, data: bytes) -> bytes:
        import zlib
        return zlib.compress(data)

class NullCompression:
    def compress(self, data: bytes) -> bytes:
        return data


class FileArchiver:
    def __init__(self, compressor: CompressionStrategy):
        # Injected strategy component
        self.compressor = compressor

    def archive(self, payload: bytes) -> bytes:
        return self.compressor.compress(payload)


archiver = FileArchiver(ZipCompression())
raw_payload = b"Antigravity Engineering Python Notes" * 10
compressed = archiver.archive(raw_payload)
print(f"Original size: {len(raw_payload)}, Compressed size: {len(compressed)}")

# Hot-swap strategy dynamically at runtime!
archiver.compressor = NullCompression()
print(f"Null compressed size: {len(archiver.archive(raw_payload))}")
```
Output:
```
Original size: 360, Compressed size: 49
Null compressed size: 360
```

### Example 3: Transparent Delegation Hook via `__getattr__`
```python
class LoggerComponent:
    def log(self, msg: str):
        print(f"[LOG]: {msg}")

    def error(self, err: str):
        print(f"[ERROR]: {err}")


class MonitoredService:
    def __init__(self, name: str):
        self.name = name
        self._logger = LoggerComponent()

    def execute(self):
        self.log(f"Executing service {self.name}")

    def __getattr__(self, name: str):
        # Fallback delegation: forward unknown attributes to _logger
        return getattr(self._logger, name)


svc = MonitoredService("AuthWorker")
svc.execute()
# Transparently forwarded to LoggerComponent
svc.error("Token expired")
```
Output:
```
[LOG]: Executing service AuthWorker
[ERROR]: Token expired
```

### Example 4: Decorator / Wrapper Pattern via Composition
```python
class DatabaseReader:
    def fetch_data(self, query: str) -> list[str]:
        return [f"result for {query}"]


class CachedDatabaseReader:
    def __init__(self, target: DatabaseReader):
        self._target = target
        self._cache: dict[str, list[str]] = {}

    def fetch_data(self, query: str) -> list[str]:
        if query not in self._cache:
            print(f"Cache miss for '{query}'. Fetching from DB...")
            self._cache[query] = self._target.fetch_data(query)
        return self._cache[query]


reader = CachedDatabaseReader(DatabaseReader())
print(reader.fetch_data("SELECT * FROM users"))
print(reader.fetch_data("SELECT * FROM users"))  # Hits cache!
```
Output:
```
Cache miss for 'SELECT * FROM users'. Fetching from DB...
['result for SELECT * FROM users']
['result for SELECT * FROM users']
```

### Example 5: Multi-Component Assembly
```python
class CPU:
    def process(self): return "CPU computation done"

class Memory:
    def load(self): return "Memory buffer allocated"

class Storage:
    def read_disk(self): return "Disk data read"

class Computer:
    def __init__(self):
        self.cpu = CPU()
        self.memory = Memory()
        self.storage = Storage()

    def boot(self) -> list[str]:
        return [
            self.memory.load(),
            self.storage.read_disk(),
            self.cpu.process()
        ]


pc = Computer()
print(f"Boot sequence: {pc.boot()}")
```
Output:
```
Boot sequence: ['Memory buffer allocated', 'Disk data read', 'CPU computation done']
```

---

## 4. Common Anti-Patterns & Gotchas

| Bad Practice | Root Cause | Clean Solution |
| :--- | :--- | :--- |
| Subclassing standard collections (`list`, `dict`) to restrict behavior | Inheriting all mutating methods violates encapsulation and invariants | Wrap the collection inside a composite class and expose only valid operations |
| Building 5+ level deep inheritance trees | High coupling; base class modifications break deep descendants | Refactor into composable components and strategies |
| Writing infinite recursion in `__getattr__`: accessing `self.target` | If `self.target` doesn't exist, `__getattr__` triggers itself infinitely | Access via `self.__dict__['target']` or initialize carefully in `__init__` |
| Over-forwarding methods manually without helper abstractions | Tedious boilerplate for large interfaces | Use `__getattr__` delegation or proxy descriptors |
| Using inheritance solely to share a single helper method | Forces semantic "is-a" coupling for code reuse convenience | Extract helper into a standalone utility function or composite service |
| Exposing internal composed components directly as mutable public state | Leaks implementation details to callers | Provide high-level wrapper methods and keep internal parts private (`_component`) |

---

## 5. Interview Q&A

<details>
<summary>1. Why should developers "Favor composition over inheritance"?</summary>
Inheritance creates tight compile-time coupling between classes, exposes parent internal implementation details (white-box reuse), and can break derived classes when the base class evolves (Fragile Base Class problem). Composition achieves black-box reuse by assembling loosely coupled components through defined interfaces, allowing behaviors to be swapped dynamically at runtime and easily mocked during testing.
</details>

<details>
<summary>2. What is the Fragile Base Class problem?</summary>
The Fragile Base Class problem occurs when changes to a base class's internal implementation inadvertently break derived subclasses, even if the public interface remains unchanged. For example, changing a base method to call another internal method can cause unintended infinite recursion or broken overrides in subclasses.
</details>

<details>
<summary>3. How does Python's __getattr__ facilitate composition?</summary>
<code>__getattr__(self, name)</code> is only called when an attribute lookup fails to find the attribute on the instance or its class hierarchy. In a composite class, <code>__getattr__</code> can forward the lookup to an internal component (<code>getattr(self._delegate, name)</code>), enabling transparent delegation without writing repetitive forwarding methods.
</details>

<details>
<summary>4. What is the danger of writing self.delegate inside __getattr__?</summary>
If <code>self.delegate</code> has not yet been initialized in <code>__init__</code> (or was mistyped), accessing <code>self.delegate</code> triggers <code>__getattr__</code> again, creating an infinite recursive loop resulting in a <code>RecursionError</code>. To avoid this, use <code>self.__dict__.get("delegate")</code> or ensure attributes exist prior to delegation.
</details>

<details>
<summary>5. How does composition simplify unit testing compared to inheritance?</summary>
With composition, components are typically passed into the constructor (dependency injection). In unit tests, real dependencies (such as databases, network clients, or payment processors) can be seamlessly replaced with lightweight mock or fake objects conforming to the same interface.
</details>

<details>
<summary>6. When IS inheritance preferred over composition?</summary>
Inheritance is preferred when there is a true, undeniable "is-a" relationship that adheres strictly to the Liskov Substitution Principle (every instance of the subclass can substitute for the parent class without altering program correctness), or when building framework extension points (like <code>abc.ABC</code> or Django models).
</details>

<details>
<summary>7. What is the Delegation Pattern?</summary>
The Delegation Pattern is an object-oriented pattern where an object handles a request by delegating it to an internal associated helper object. The outer object acts as a front-end, tailoring or intercepting calls before routing them.
</details>

<details>
<summary>8. How do Python Protocols complement composition?</summary>
<code>typing.Protocol</code> defines structural contracts. When using composition, injecting components typed as Protocols ensures that any object with the required methods can be plugged into the composite host without requiring inheritance from an abstract base class.
</details>

---

## 6. Practical Challenges

<details>
<summary>Challenge 1: Safe Fixed-Size Bounded Queue</summary>

Implement `BoundedQueue` via composition with an internal `collections.deque`. Enforce a maximum capacity and do NOT expose standard deque mutations like `.extendleft()`.

```python
from collections import deque

class BoundedQueue:
    def __init__(self, maxsize: int):
        self._maxsize = maxsize
        self._items = deque(maxlen=maxsize)

    def enqueue(self, item):
        if len(self._items) >= self._maxsize:
            raise OverflowError("Queue is full")
        self._items.append(item)

    def dequeue(self):
        if not self._items:
            raise IndexError("Queue is empty")
        return self._items.popleft()

    def __len__(self):
        return len(self._items)

q = BoundedQueue(2)
q.enqueue(1)
q.enqueue(2)
assert len(q) == 2
assert q.dequeue() == 1
```
</details>

<details>
<summary>Challenge 2: Pluggable Notification Service</summary>

Create an `AlertManager` class that takes a list of alert channels (e.g. EmailChannel, SlackChannel) via composition and broadcasts alerts to all registered channels.

```python
class EmailChannel:
    def send(self, alert: str) -> str: return f"EMAIL: {alert}"

class SlackChannel:
    def send(self, alert: str) -> str: return f"SLACK: {alert}"

class AlertManager:
    def __init__(self, channels):
        self.channels = channels

    def notify_all(self, msg: str) -> list[str]:
        return [c.send(msg) for c in self.channels]

mgr = AlertManager([EmailChannel(), SlackChannel()])
assert mgr.notify_all("Outage") == ["EMAIL: Outage", "SLACK: Outage"]
```
</details>

<details>
<summary>Challenge 3: Transparent Delegation Proxy</summary>

Write a generic `DelegatingProxy(target)` class that intercepts method calls, logs the method name, and delegates to `target` using `__getattr__`.

```python
class DelegatingProxy:
    def __init__(self, target):
        self._target = target
        self.logged_calls = []

    def __getattr__(self, name):
        attr = getattr(self._target, name)
        if callable(attr):
            def wrapper(*args, **kwargs):
                self.logged_calls.append(name)
                return attr(*args, **kwargs)
            return wrapper
        return attr

class Worker:
    def run(self, val): return val * 2

w = Worker()
proxy = DelegatingProxy(w)
assert proxy.run(5) == 10
assert proxy.logged_calls == ["run"]
```
</details>

<details>
<summary>Challenge 4: Dynamic Strategy Switching</summary>

Implement an `Order` class with a composable `DiscountStrategy`. Demonstrate switching between `PercentageDiscount` and `FlatDiscount` at runtime.

```python
class PercentageDiscount:
    def __init__(self, pct: float): self.pct = pct
    def apply(self, total: float) -> float: return total * (1 - self.pct)

class FlatDiscount:
    def __init__(self, amount: float): self.amount = amount
    def apply(self, total: float) -> float: return max(0.0, total - self.amount)

class Order:
    def __init__(self, total: float, strategy):
        self.total = total
        self.strategy = strategy

    def final_price(self) -> float:
        return self.strategy.apply(self.total)

ord1 = Order(100.0, PercentageDiscount(0.2))
assert ord1.final_price() == 80.0
ord1.strategy = FlatDiscount(15.0)
assert ord1.final_price() == 85.0
```
</details>

<details>
<summary>Challenge 5: Safe Dict Wrapper with Audit Logging</summary>

Build an `AuditedDict` composing an internal dictionary that logs every key written or deleted to an audit list.

```python
class AuditedDict:
    def __init__(self):
        self._store = {}
        self.audit_log = []

    def __setitem__(self, key, val):
        self.audit_log.append(f"SET {key}")
        self._store[key] = val

    def __getitem__(self, key):
        return self._store[key]

    def __delitem__(self, key):
        self.audit_log.append(f"DEL {key}")
        del self._store[key]

d = AuditedDict()
d["a"] = 1
del d["a"]
assert d.audit_log == ["SET a", "DEL a"]
```
</details>

<details>
<summary>Challenge 6: Composite Workflow Pipeline</summary>

Build a `Pipeline` class that composes multiple filter objects, feeding the output of one component into the input of the next.

```python
class StripFilter:
    def process(self, text: str) -> str: return text.strip()

class LowerFilter:
    def process(self, text: str) -> str: return text.lower()

class Pipeline:
    def __init__(self, *steps):
        self.steps = steps

    def run(self, data: str) -> str:
        for s in self.steps:
            data = s.process(data)
        return data

p = Pipeline(StripFilter(), LowerFilter())
assert p.run("  HELLO WORLD  ") == "hello world"
```
</details>
