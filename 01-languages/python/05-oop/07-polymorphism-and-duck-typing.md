# Polymorphism and Duck Typing

- **Topic**: `05-oop`
- **Python version**: 3.12+ (tested on CPython 3.14.0)
- **Tested with**: CPython 3.12, 3.13, 3.14 (standard & free-threaded builds)
- **Free-threading notes**: Compatible. Dynamic dispatch and method lookup are thread-safe under CPython's object model.

---

## 1. Concept Overview

In statically typed object-oriented languages like Java or C++, polymorphism is traditionally tied to class inheritance hierarchies: an object must inherit from a common base class or implement an explicit interface to be treated polymorphically.

Python embraces **Duck Typing**:
> *"If it walks like a duck and quacks like a duck, it's a duck."*

In Python, the suitability of an object is determined not by its inheritance tree, but by the presence of specific methods and properties. If a function requires an object with a `.read()` method, it can accept a file, a network socket, an in-memory buffer (`io.StringIO`), or a custom mock object without needing them to share a common ancestor.

Coupled with duck typing is the Pythonic philosophy of **EAFP** (*Easier to Ask for Forgiveness than Permission*) versus **LBYL** (*Look Before You Leap*).

```
LBYL (Non-Pythonic):                     EAFP (Pythonic):
if hasattr(obj, "quack"):               try:
    obj.quack()                             obj.quack()
else:                                   except AttributeError:
    handle_error()                          handle_error()
```

---

## 2. Key Mechanics & Architecture

### EAFP vs LBYL Comparison

| Dimension | EAFP (Ask Forgiveness) | LBYL (Look Before You Leap) |
| :--- | :--- | :--- |
| **Philosophy** | Assume operation succeeds; catch exceptions | Verify preconditions before executing operation |
| **Code Style** | `try: ... except Error: ...` | `if hasattr(...) / if key in dict: ...` |
| **Race Conditions** | Immune to TOCTOU (Time-of-Check to Time-of-Use) | Vulnerable to race conditions between check and action |
| **Performance** | Faster on happy paths ($O(0)$ overhead); slower on exceptions | Consistent overhead for every check |
| **Idiomatic Fit** | Standard Pythonic idiom across standard library | Common in C/Java, discouraged in Python core APIs |

### Structural Polymorphism via Protocols
Python achieves compile-time verified duck typing through `typing.Protocol` (PEP 544). Instead of declaring nominal inheritance (`class FileStream(BaseStream)`), a protocol defines the expected interface structurally. Any class with matching signatures satisfies the protocol implicitly.

---

## 3. Real-World Code Examples

### Example 1: Classic Duck Typing with File-like Objects
```python
import io

def process_stream(stream) -> int:
    """Consumes any object that provides a .read() method."""
    content = stream.read()
    if isinstance(content, str):
        return len(content.split())
    elif isinstance(content, bytes):
        return len(content.split())
    return 0


# 1. Real file stream simulation
class MockFile:
    def read(self):
        return "Alpha Beta Gamma Delta"

# 2. In-memory StringIO stream
mem_stream = io.StringIO("One Two Three")

# 3. Custom Network Buffer
class NetworkBuffer:
    def read(self):
        return b"Payload byte stream"


print(f"MockFile word count: {process_stream(MockFile())}")
print(f"StringIO word count: {process_stream(mem_stream)}")
print(f"NetworkBuffer word count: {process_stream(NetworkBuffer())}")
```
Output:
```
MockFile word count: 4
StringIO word count: 3
NetworkBuffer word count: 3
```

### Example 2: EAFP vs LBYL in Key Lookup & File Access
```python
cache = {"session_id": "tok_xyz"}

# LBYL Approach (Two hash lookups + race condition prone)
if "session_id" in cache:
    val = cache["session_id"]
    print(f"LBYL found: {val}")

# EAFP Approach (Single hash lookup, safe and idiomatic)
try:
    val = cache["session_id"]
    print(f"EAFP found: {val}")
except KeyError:
    print("Session not found")
```
Output:
```
LBYL found: tok_xyz
EAFP found: tok_xyz
```

### Example 3: Polymorphic Notification Engine
```python
class EmailNotifier:
    def send(self, message: str, recipient: str):
        print(f"Sending Email to {recipient}: {message}")

class SMSNotifier:
    def send(self, message: str, recipient: str):
        print(f"Sending SMS to {recipient}: {message}")

class WebhookNotifier:
    def send(self, message: str, recipient: str):
        print(f"Posting Webhook to {recipient}: {message}")


def broadcast(notifiers: list, message: str, recipient: str):
    """Polymorphic dispatch without common base class."""
    for notifier in notifiers:
        notifier.send(message, recipient)


services = [EmailNotifier(), SMSNotifier(), WebhookNotifier()]
broadcast(services, "System reboot at midnight", "ops-team")
```
Output:
```
Sending Email to ops-team: System reboot at midnight
Sending SMS to ops-team: System reboot at midnight
Posting Webhook to ops-team: System reboot at midnight
```

### Example 4: Structural Typing with `typing.Protocol`
```python
from typing import Protocol, runtime_checkable

@runtime_checkable
class Renderable(Protocol):
    def render(self) -> str: ...


class MarkdownDoc:
    def render(self) -> str:
        return "# Title\nContent"

class HTMLDoc:
    def render(self) -> str:
        return "<h1>Title</h1><p>Content</p>"

class PlainText:
    def display(self) -> str:  # Missing .render()!
        return "Title: Content"


def display_document(doc: Renderable):
    print(doc.render())


md = MarkdownDoc()
html = HTMLDoc()
plain = PlainText()

print(f"Markdown satisfies Renderable: {isinstance(md, Renderable)}")
print(f"HTML satisfies Renderable: {isinstance(html, Renderable)}")
print(f"PlainText satisfies Renderable: {isinstance(plain, Renderable)}")
```
Output:
```
Markdown satisfies Renderable: True
HTML satisfies Renderable: True
PlainText satisfies Renderable: False
```

### Example 5: Polymorphism via Special Methods (Iterable Duck Typing)
```python
class FibonacciSequence:
    """An object that behaves like an iterable purely by implementing __iter__."""
    def __init__(self, limit: int):
        self.limit = limit

    def __iter__(self):
        a, b = 0, 1
        for _ in range(self.limit):
            yield a
            a, b = b, a + b


fib = FibonacciSequence(6)
print(f"Fibonacci duck typing: {list(fib)}")
```
Output:
```
Fibonacci duck typing: [0, 1, 1, 2, 3, 5]
```

---

## 4. Common Anti-Patterns & Gotchas

| Bad Practice | Root Cause | Clean Solution |
| :--- | :--- | :--- |
| Writing `if type(obj) is ExpectedClass:` | Breaks polymorphism; fails for valid subclasses or duck types | Use duck typing (call method directly) or `isinstance` |
| Overusing `hasattr()` before every method call | Degrades into defensive LBYL; creates race conditions | Call method directly inside a `try ... except AttributeError:` block |
| Catching bare `except Exception:` in EAFP | Swallows unrelated errors like syntax bugs or typos | Catch only the specific expected exception (e.g., `except (KeyError, AttributeError):`) |
| Enforcing nominal inheritance where unnecessary | Forces rigid coupling to base classes | Use `typing.Protocol` for structural interfaces |
| Expecting duck typing to catch signature mismatches | If an object has the method name but wrong arguments, raises `TypeError` | Use static type checkers (mypy/pyright) and `@runtime_checkable` |
| Using LBYL for file system operations (`if os.path.exists: open`) | Subject to TOCTOU race conditions (file can be deleted between check and open) | Open directly in `try ... except FileNotFoundError:` |

---

## 5. Interview Q&A

<details>
<summary>1. What is the core philosophy of Duck Typing?</summary>
Duck Typing means an object's suitability is determined solely by the methods and properties it exposes at runtime, rather than its explicit position in an inheritance tree. If an object implements the required interface (e.g., <code>.read()</code>, <code>__iter__()</code>), Python code will accept and execute it without requiring a shared base class.
</details>

<details>
<summary>2. Explain the difference between EAFP and LBYL.</summary>
<b>LBYL (Look Before You Leap)</b> checks preconditions before executing an operation (e.g. checking <code>if key in dict:</code> or <code>if hasattr(obj, 'attr'):</code>). <b>EAFP (Easier to Ask for Forgiveness than Permission)</b> assumes the operation will succeed and catches the exception if it fails (e.g. <code>try: val = dict[key] except KeyError: ...</code>). EAFP is the idiomatic Python standard because it is faster on normal paths and eliminates TOCTOU race conditions.
</details>

<details>
<summary>3. What is a TOCTOU (Time-of-Check to Time-of-Use) race condition?</summary>
TOCTOU is a concurrency bug where state changes between the moment a condition is checked (LBYL) and the moment the resource is accessed. For example, checking <code>if os.path.exists(file): open(file)</code> can fail if another process deletes or locks the file between the check and the open call. EAFP handles this atomically via <code>try: open(file) except FileNotFoundError: ...</code>.
</details>

<details>
<summary>4. How does typing.Protocol provide structural subtyping (static duck typing)?</summary>
Introduced in PEP 544, <code>typing.Protocol</code> defines an interface structurally rather than nominally. A class does not need to inherit from the protocol; as long as its methods and attribute signatures match the protocol, static type checkers (like mypy) will accept it as a valid subtype.
</details>

<details>
<summary>5. Why is if type(x) is int discouraged compared to isinstance(x, int)?</summary>
<code>type(x) is int</code> performs an exact identity check and rejects subclasses of <code>int</code> (such as <code>bool</code> or custom integer subclasses). <code>isinstance(x, int)</code> respects inheritance hierarchies and polymorphism.
</details>

<details>
<summary>6. When is LBYL preferred over EAFP in Python?</summary>
LBYL is preferred when the failure rate is exceptionally high and the cost of raising and catching exceptions impacts performance, or when an exception could produce irreversible side effects before failing.
</details>

<details>
<summary>7. How does Python implement duck typing for operators (like + or *)?</summary>
Python uses special dunder methods. Any class that implements <code>__add__(self, other)</code> can participate in addition (<code>a + b</code>), regardless of whether it inherits from a numeric base class.
</details>

<details>
<summary>8. What is the overhead of a try-except block in modern Python 3.11+?</summary>
Python 3.11 introduced "zero-cost exceptions". In the happy path (when no exception is raised), a <code>try</code> block has zero runtime overhead because exception tables are stored separately and only consulted when an exception is actually thrown.
</details>

---

## 6. Practical Challenges

<details>
<summary>Challenge 1: Universal Data Reader</summary>

Write a function `dump_records(source)` that polymorphically extracts text from strings, lists, or open file objects using duck typing.

```python
def dump_records(source) -> list[str]:
    if hasattr(source, "read"):
        return source.read().splitlines()
    elif isinstance(source, str):
        return source.splitlines()
    elif hasattr(source, "__iter__"):
        return [str(x) for x in source]
    return []

import io
assert dump_records("line1\nline2") == ["line1", "line2"]
assert dump_records(io.StringIO("a\nb")) == ["a", "b"]
assert dump_records([1, 2, 3]) == ["1", "2", "3"]
```
</details>

<details>
<summary>Challenge 2: EAFP Configuration Extractor</summary>

Write a function `safe_nested_lookup(d: dict, *keys, default=None)` that traverses nested dictionaries using pure EAFP without checking `isinstance` or `key in d`.

```python
def safe_nested_lookup(d, *keys, default=None):
    curr = d
    try:
        for k in keys:
            curr = curr[k]
        return curr
    except (KeyError, TypeError, IndexError):
        return default

data = {"server": {"database": {"port": 5432}}}
assert safe_nested_lookup(data, "server", "database", "port") == 5432
assert safe_nested_lookup(data, "server", "cache", "port", default=6379) == 6379
```
</details>

<details>
<summary>Challenge 3: Runtime Checkable Serializable Protocol</summary>

Define a `@runtime_checkable` `Serializable` protocol requiring a `to_json() -> str` method. Write a validator that verifies input objects conform to it.

```python
from typing import Protocol, runtime_checkable

@runtime_checkable
class Serializable(Protocol):
    def to_json(self) -> str: ...

class UserRecord:
    def to_json(self) -> str:
        return '{"user": "alice"}'

class InvalidRecord:
    pass

assert isinstance(UserRecord(), Serializable)
assert not isinstance(InvalidRecord(), Serializable)
```
</details>

<details>
<summary>Challenge 4: Duck Typing Closable Context</summary>

Write a function `auto_close(resource, action_fn)` that calls `action_fn(resource)` and closes the resource if it has a `.close()` method, regardless of its type.

```python
def auto_close(resource, action_fn):
    try:
        return action_fn(resource)
    finally:
        try:
            resource.close()
        except AttributeError:
            pass

class DummyResource:
    closed = False
    def close(self): self.closed = True

r = DummyResource()
res = auto_close(r, lambda x: 42)
assert res == 42
assert r.closed is True
```
</details>

<details>
<summary>Challenge 5: Polymorphic Sizer</summary>

Implement `polymorphic_size(obj)` that checks for `.size()`, `len(obj)`, or falls back to `0`.

```python
def polymorphic_size(obj) -> int:
    try:
        return obj.size()
    except (AttributeError, TypeError):
        try:
            return len(obj)
        except TypeError:
            return 0

class CustomCollection:
    def size(self): return 100

assert polymorphic_size([1, 2, 3]) == 3
assert polymorphic_size(CustomCollection()) == 100
assert polymorphic_size(42) == 0
```
</details>

<details>
<summary>Challenge 6: EAFP Safe Type Coercion</summary>

Write a function `coerce_numeric(val)` that attempts to convert `val` to `int`, then `float`, then returns `val` as-is using pure EAFP.

```python
def coerce_numeric(val):
    try:
        return int(val)
    except (ValueError, TypeError):
        try:
            return float(val)
        except (ValueError, TypeError):
            return val

assert coerce_numeric("42") == 42
assert coerce_numeric("3.14") == 3.14
assert coerce_numeric("abc") == "abc"
```
</details>
