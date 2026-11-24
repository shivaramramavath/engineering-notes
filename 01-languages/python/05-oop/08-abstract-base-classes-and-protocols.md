# Abstract Base Classes (ABCs) and Protocols

- **Topic**: `05-oop`
- **Python version**: 3.12+ (tested on CPython 3.14.0)
- **Tested with**: CPython 3.12, 3.13, 3.14 (standard & free-threaded builds)
- **Free-threading notes**: Compatible. ABC registry lookups and Protocol type-checking caches are thread-safe under CPython.

---

## 1. Concept Overview

Python provides two distinct paradigms for defining formal interfaces and contracts:

1. **Abstract Base Classes (`abc.ABC`)**: **Nominal Subtyping**. Classes explicitly inherit from an ABC and use `@abstractmethod` to enforce that subclasses implement required methods. Attempting to instantiate a subclass missing an abstract method raises `TypeError` at instantiation time.
2. **Protocols (`typing.Protocol`)**: **Structural Subtyping (Duck Typing)**. Introduced in PEP 544. A protocol defines an expected shape (attributes and methods). Any class that implements that shape satisfies the protocol implicitly—no explicit inheritance required.

Together, they allow Python developers to balance strict nominal contract enforcement (ABCs) with flexible, decoupled duck typing (Protocols).

```
   Nominal Typing (ABCs):                     Structural Typing (Protocols):
+-------------------------------+             +-------------------------------+
|      class Repository(ABC)    |             |      class Renderable(Proto)  |
|      @abstractmethod          |             |      def render(self) -> str  |
|      def save(self, data): ...|             +-------------------------------+
+-------------------------------+                            ^
               ^                                             : (Satisfies shape)
               | (Must inherit)                              :
+-------------------------------+             +-------------------------------+
|    class PostgresRepo(Repo)   |             |       class HTMLWidget        |
|    def save(self, data): ...  |             |       def render(self) -> str |
+-------------------------------+             +-------------------------------+
```

---

## 2. Key Mechanics & Architecture

### ABCs vs Protocols Comparison

| Feature | `abc.ABC` | `typing.Protocol` |
| :--- | :--- | :--- |
| **Typing Philosophy** | Nominal (explicit inheritance) | Structural (implicit shape matching) |
| **Subclass Declaration** | `class Derived(MyABC):` | `class MyObject:` (no inheritance needed) |
| **Enforcement Timing** | Runtime at `__init__` / `__new__` | Static analysis time (mypy/pyright) |
| **Runtime Checkable** | Native (`isinstance`, `issubclass`) | Requires `@runtime_checkable` decorator |
| **Virtual Subclasses** | Supported via `MyABC.register(TargetClass)` | Natural via structural matching |
| **Best Used For** | Framework plugins, internal shared base logic | Public APIs, library boundaries, decoupled decoupling |

### Virtual Subclasses with `ABC.register`
ABCs allow "virtual subclassing": registering an external class as a subclass without modifying its inheritance tree. Calling `isinstance(ExternalClass(), MyABC)` returns `True`, but `MyABC` cannot provide default method implementations to virtual subclasses.

---

## 3. Real-World Code Examples

### Example 1: Enforcing Interface Contracts with `abc.ABC`
```python
from abc import ABC, abstractmethod

class StorageBackend(ABC):
    @abstractmethod
    def store(self, key: str, payload: bytes) -> bool:
        """Persist payload under key."""

    @abstractmethod
    def retrieve(self, key: str) -> bytes:
        """Fetch payload for key."""

    def exists(self, key: str) -> bool:
        """Concrete default method available to all subclasses."""
        try:
            self.retrieve(key)
            return True
        except KeyError:
            return False


class InMemoryStorage(StorageBackend):
    def __init__(self):
        self._data: dict[str, bytes] = {}

    def store(self, key: str, payload: bytes) -> bool:
        self._data[key] = payload
        return True

    def retrieve(self, key: str) -> bytes:
        if key not in self._data:
            raise KeyError(key)
        return self._data[key]


store = InMemoryStorage()
store.store("config", b"debug=true")
print(f"Retrieved: {store.retrieve('config')}")
print(f"Exists 'config': {store.exists('config')}")
print(f"Exists 'missing': {store.exists('missing')}")
```
Output:
```
Retrieved: b'debug=true'
Exists 'config': True
Exists 'missing': False
```

### Example 2: Instantiation Failure for Incomplete ABC Subclass
```python
from abc import ABC, abstractmethod

class IncompleteStorage(StorageBackend):
    def store(self, key: str, payload: bytes) -> bool:
        return True
    # Missing retrieve()!


try:
    s = IncompleteStorage()
except TypeError as err:
    print(f"Instantiation caught: {err}")
```
Output:
```
Instantiation caught: Can't instantiate abstract class IncompleteStorage without an implementation for abstract method 'retrieve'
```

### Example 3: Virtual Subclasses via `register()`
```python
from abc import ABC, abstractmethod

class Greeter(ABC):
    @abstractmethod
    def greet(self) -> str:
        pass


class SpanishSpeaker:
    def greet(self) -> str:
        return "Hola!"


# SpanishSpeaker does not inherit from Greeter, but we register it virtually
Greeter.register(SpanishSpeaker)

sp = SpanishSpeaker()
print(f"isinstance(sp, Greeter): {isinstance(sp, Greeter)}")
print(f"issubclass(SpanishSpeaker, Greeter): {issubclass(SpanishSpeaker, Greeter)}")
```
Output:
```
isinstance(sp, Greeter): True
issubclass(SpanishSpeaker, Greeter): True
```

### Example 4: Modern Structural Typing with `typing.Protocol`
```python
from typing import Protocol, runtime_checkable

@runtime_checkable
class Closeable(Protocol):
    def close(self) -> None: ...


class DatabaseConn:
    def close(self) -> None:
        print("Database connection closed")

class FileHandle:
    def close(self) -> None:
        print("File handle closed")

class NonCloseable:
    pass


def terminate_resource(res: Closeable):
    res.close()


db = DatabaseConn()
fh = FileHandle()
nc = NonCloseable()

print(f"db is Closeable: {isinstance(db, Closeable)}")
print(f"fh is Closeable: {isinstance(fh, Closeable)}")
print(f"nc is Closeable: {isinstance(nc, Closeable)}")
terminate_resource(db)
```
Output:
```
db is Closeable: True
fh is Closeable: True
nc is Closeable: False
Database connection closed
```

### Example 5: Abstract Properties in ABCs
```python
from abc import ABC, abstractmethod

class BaseSensor(ABC):
    @property
    @abstractmethod
    def reading(self) -> float:
        """Subclasses must provide a reading property."""


class Thermometer(BaseSensor):
    def __init__(self, temp: float):
        self._temp = temp

    @property
    def reading(self) -> float:
        return self._temp


t = Thermometer(21.5)
print(f"Thermometer reading: {t.reading} C")
```
Output:
```
Thermometer reading: 21.5 C
```

---

## 4. Common Anti-Patterns & Gotchas

| Bad Practice | Root Cause | Clean Solution |
| :--- | :--- | :--- |
| Raising `NotImplementedError` manually in base methods instead of `@abstractmethod` | Does not prevent instantiation of the incomplete subclass | Inherit from `abc.ABC` and decorate with `@abstractmethod` |
| Calling `isinstance(obj, MyProtocol)` without `@runtime_checkable` | Raises `TypeError: Instance and class checks can only be used with @runtime_checkable protocols` | Decorate protocol with `@typing.runtime_checkable` |
| Expecting `@runtime_checkable` to verify method argument types at runtime | Runtime checks only verify attribute existence, not type signatures | Rely on static type checkers (mypy/pyright) for signature verification |
| Stacking `@abstractmethod` inside `@property`: `@abstractmethod @property` | Decorator order matters | Place `@property` first: `@property \n @abstractmethod` |
| Excessive nominal inheritance for simple utility interfaces | Tightly couples consumers to a specific package or base class | Prefer `typing.Protocol` for decoupled contracts |
| Expecting virtual subclasses to inherit concrete helper methods from an ABC | Virtual subclasses do not inherit the MRO of the ABC | Use real inheritance if code reuse/mixins are required |

---

## 5. Interview Q&A

<details>
<summary>1. When should you choose an ABC over a Protocol?</summary>
Choose an <b>ABC</b> when you want to enforce nominal inheritance contracts, provide shared concrete default implementations to subclasses, or build a tightly coupled framework architecture. Choose a <b>Protocol</b> when you want structural typing (duck typing), need to decouple caller and callee across third-party library boundaries, or want zero inheritance overhead.
</details>

<details>
<summary>2. How does CPython prevent the instantiation of an abstract class?</summary>
During class creation, the <code>ABCMeta</code> metaclass inspects the class attributes and collects all methods flagged with <code>__isabstractmethod__ = True</code> into a frozen set <code>__abstractmethods__</code>. When <code>cls.__new__</code> is invoked, if <code>__abstractmethods__</code> is non-empty, CPython raises a <code>TypeError</code> immediately.
</details>

<details>
<summary>3. What is a virtual subclass in Python?</summary>
A virtual subclass is a class registered with an ABC via <code>MyABC.register(TargetClass)</code> or customized via <code>__subclasshook__</code>. It allows <code>isinstance(target, MyABC)</code> and <code>issubclass(TargetClass, MyABC)</code> to return <code>True</code> without <code>TargetClass</code> having <code>MyABC</code> in its MRO.
</details>

<details>
<summary>4. What are the limitations of @runtime_checkable on protocols?</summary>
<code>@runtime_checkable</code> only verifies that the candidate object has attributes or callables with matching names via <code>hasattr()</code>. It cannot check method signatures, parameter types, or return types at runtime due to performance costs.
</details>

<details>
<summary>5. Can an abstract method have an implementation in the ABC?</summary>
Yes! An abstract method can contain a concrete implementation in the ABC body. Subclasses must still override the method, but they can invoke the base implementation using <code>super().abstract_method()</code>.
</details>

<details>
<summary>6. What is the correct decorator stacking order for abstract properties?</summary>
The correct order is <code>@property</code> on the outside and <code>@abstractmethod</code> on the inside:
<pre><code>@property
@abstractmethod
def name(self) -> str:
    ...
</code></pre>
</details>

<details>
<summary>7. How does collections.abc use __subclasshook__?</summary>
Standard library ABCs like <code>collections.abc.Iterable</code> implement <code>__subclasshook__(cls, C)</code>. When <code>issubclass(C, Iterable)</code> is evaluated, the hook checks if <code>"__iter__"</code> is in <code>C.__mdict__</code> or its MRO, enabling automatic structural subclass detection without manual registration.
</details>

<details>
<summary>8. Does Protocol inherit from object or ABC?</summary>
<code>typing.Protocol</code> inherits from <code>typing.Generic</code> and has a specialized metaclass <code>_ProtocolMeta</code>. It is completely independent of <code>abc.ABC</code>.
</details>

---

## 6. Practical Challenges

<details>
<summary>Challenge 1: ABC Plugin Architecture</summary>

Create an ABC `Plugin` with abstract method `execute(data: str) -> str`. Implement `UpperPlugin` and `ReversePlugin` and a runner that processes a pipeline of plugins.

```python
from abc import ABC, abstractmethod

class Plugin(ABC):
    @abstractmethod
    def execute(self, data: str) -> str: pass

class UpperPlugin(Plugin):
    def execute(self, data: str) -> str: return data.upper()

class ReversePlugin(Plugin):
    def execute(self, data: str) -> str: return data[::-1]

def run_pipeline(text: str, plugins: list[Plugin]) -> str:
    for p in plugins:
        text = p.execute(text)
    return text

assert run_pipeline("hello", [UpperPlugin(), ReversePlugin()]) == "OLLEH"
```
</details>

<details>
<summary>Challenge 2: Custom __subclasshook__ for Sized Streams</summary>

Build an ABC `SizedStream` that automatically recognizes any class defining both `.read()` and `__len__()` as a subclass using `__subclasshook__`.

```python
from abc import ABCMeta

class SizedStream(metaclass=ABCMeta):
    @classmethod
    def __subclasshook__(cls, subclass):
        if cls is SizedStream:
            has_read = any("read" in b.__dict__ for b in subclass.__mro__)
            has_len = any("__len__" in b.__dict__ for b in subclass.__mro__)
            if has_read and has_len:
                return True
        return NotImplemented

class CustomBuffer:
    def read(self): return "data"
    def __len__(self): return 4

assert issubclass(CustomBuffer, SizedStream)
```
</details>

<details>
<summary>Challenge 3: Generic Protocol with TypeVar</summary>

Define a generic Protocol `Transformer[T, R]` that defines `transform(val: T) -> R`.

```python
from typing import Protocol, TypeVar

T = TypeVar("T")
R = TypeVar("R")

class Transformer(Protocol[T, R]):
    def transform(self, val: T) -> R: ...

class IntToString:
    def transform(self, val: int) -> str:
        return f"VAL_{val}"

t: Transformer[int, str] = IntToString()
assert t.transform(42) == "VAL_42"
```
</details>

<details>
<summary>Challenge 4: ABC with Abstract Class Method</summary>

Implement an ABC `Model` with `@classmethod` and `@abstractmethod` `table_name(cls) -> str`.

```python
from abc import ABC, abstractmethod

class Model(ABC):
    @classmethod
    @abstractmethod
    def table_name(cls) -> str: pass

class UserModel(Model):
    @classmethod
    def table_name(cls) -> str:
        return "users"

assert UserModel.table_name() == "users"
```
</details>

<details>
<summary>Challenge 5: Multi-Method Runtime Protocol Check</summary>

Create a runtime-checkable protocol `Transactional` requiring `commit()` and `rollback()`. Test an object that implements only `commit()`.

```python
from typing import Protocol, runtime_checkable

@runtime_checkable
class Transactional(Protocol):
    def commit(self) -> None: ...
    def rollback(self) -> None: ...

class PartialTx:
    def commit(self): pass

class FullTx:
    def commit(self): pass
    def rollback(self): pass

assert not isinstance(PartialTx(), Transactional)
assert isinstance(FullTx(), Transactional)
```
</details>

<details>
<summary>Challenge 6: Abstract Method with Concrete Base Fallback</summary>

Build an ABC `Serializer` with an abstract method `serialize()`. The base implementation formats JSON, but subclasses are forced to call `super().serialize()` to extend it.

```python
from abc import ABC, abstractmethod
import json

class Serializer(ABC):
    @abstractmethod
    def serialize(self, data: dict) -> str:
        return json.dumps(data)

class EncryptedSerializer(Serializer):
    def serialize(self, data: dict) -> str:
        base_json = super().serialize(data)
        return f"ENC:{base_json}"

s = EncryptedSerializer()
assert s.serialize({"a": 1}) == 'ENC:{"a": 1}'
```
</details>
