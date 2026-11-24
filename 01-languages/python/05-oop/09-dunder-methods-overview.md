# Dunder Methods Overview: Representation, Containment, and Lifecycle

- **Topic**: `05-oop`
- **Python version**: 3.12+ (tested on CPython 3.14.0)
- **Tested with**: CPython 3.12, 3.13, 3.14 (standard & free-threaded builds)
- **Free-threading notes**: Compatible. Special method slot lookups (`tp_repr`, `tp_str`, `tp_contains`) are thread-safe C-level function pointers on Python type objects.

---

## 1. Concept Overview

Special methods in Python—surrounded by double underscores (`__method__`) and colloquially called **dunder methods**—form the backbone of the Python Data Model. They allow user-defined classes to hook into built-in language syntax, operators, and protocols.

Rather than looking up dunders dynamically on an instance's `__dict__`, CPython looks up special methods **directly on the object's class (`type(obj)`)** and caches them in C-level type slots (`tp_repr`, `tp_iter`, etc.) for extreme speed.

Key foundational dunder categories include:
1. **String Representation**: `__repr__` (unambiguous, for developers) vs `__str__` (readable, for users).
2. **Containment & Sequence Emulation**: `__contains__`, `__len__`, `__getitem__`, `__bool__`.
3. **Context Management**: `__enter__`, `__exit__`.
4. **Callable Objects**: `__call__`.

```
                    Python Syntax / Built-in
                               |
                               v
               Underlying Dunder Method Lookup
                               |
             +-----------------+-----------------+
             |                                   |
    print(obj) / str(obj)                     x in obj
             |                                   |
             v                                   v
      type(obj).__str__                 type(obj).__contains__
```

---

## 2. Key Mechanics & Architecture

### Dunder Lookup Optimization (Type Slots)
In CPython, invoking a dunder (e.g. `len(obj)`) does not perform an attribute lookup on `obj.__dict__`. Doing so would incur massive performance penalties. Instead:
- Python inspects `obj->ob_type->tp_as_sequence->sq_length`.
- If an instance defines `obj.__len__ = lambda: 10`, `len(obj)` **ignores** this instance attribute and queries the class level `type(obj).__len__`.

### `__repr__` vs `__str__` Rule of Thumb
- `__repr__(self) -> str`: Goal is to be **unambiguous**. When possible, should look like valid Python code to recreate the object (e.g. `Point(x=1, y=2)`). If `__str__` is not defined, Python falls back to `__repr__`.
- `__str__(self) -> str`: Goal is to be **readable**. Human-facing presentation format (e.g. `"1, 2"`).

---

## 3. Real-World Code Examples

### Example 1: `__repr__` vs `__str__` and Fallback Mechanics
```python
class Coordinate:
    def __init__(self, lat: float, lon: float):
        self.lat = lat
        self.lon = lon

    def __repr__(self) -> str:
        # Unambiguous code-like representation
        return f"Coordinate(lat={self.lat}, lon={self.lon})"

    def __str__(self) -> str:
        # User-facing pretty representation
        return f"{abs(self.lat)} deg {'N' if self.lat >= 0 else 'S'}, {abs(self.lon)} deg {'E' if self.lon >= 0 else 'W'}"


coord = Coordinate(37.7749, -122.4194)
print(f"str():  {str(coord)}")
print(f"repr(): {repr(coord)}")
print(f"f-string !r: {coord!r}")
```
Output:
```
str():  37.7749 deg N, 122.4194 deg W
repr(): Coordinate(lat=37.7749, lon=-122.4194)
f-string !r: Coordinate(lat=37.7749, lon=-122.4194)
```

### Example 2: Container Protocol (`__len__`, `__getitem__`, `__contains__`)
```python
class TaskQueue:
    def __init__(self, tasks: list[str] | None = None):
        self._tasks = list(tasks or [])

    def __len__(self) -> int:
        return len(self._tasks)

    def __getitem__(self, index: int | slice):
        return self._tasks[index]

    def __contains__(self, item: str) -> bool:
        # Fast membership test hook
        return item in self._tasks

    def __bool__(self) -> bool:
        # Truthiness evaluation
        return len(self._tasks) > 0


queue = TaskQueue(["build", "test", "deploy"])
print(f"Length: {len(queue)}")
print(f"First task: {queue[0]}")
print(f"Slice [1:]: {queue[1:]}")
print(f"Is 'test' in queue: {'test' in queue}")
print(f"Is queue truthy: {bool(queue)}")
```
Output:
```
Length: 3
First task: build
Slice [1:]: ['test', 'deploy']
Is 'test' in queue: True
Is queue truthy: True
```

### Example 3: Callable Objects with `__call__`
```python
class LinearMultiplier:
    def __init__(self, factor: float):
        self.factor = factor

    def __call__(self, x: float) -> float:
        # Allows an instance to be invoked as a standard function
        return x * self.factor


triple = LinearMultiplier(3.0)
print(f"triple(10): {triple(10)}")
print(f"callable(triple): {callable(triple)}")
```
Output:
```
triple(10): 30.0
callable(triple): True
```

### Example 4: Context Management via `__enter__` and `__exit__`
```python
import time

class ExecutionTimer:
    def __enter__(self):
        self.start = time.perf_counter()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.elapsed = time.perf_counter() - self.start
        # Returning True suppresses any exception; returning False allows it to propagate
        return False


with ExecutionTimer() as timer:
    total = sum(i * i for i in range(100_000))

print(f"Timer elapsed: {timer.elapsed:.4f}s")
```
Output:
```
Timer elapsed: 0.0035s
```

### Example 5: Slot Lookup Demonstration (Instance Dict Ignored)
```python
class SlotDemo:
    def __len__(self):
        return 100


obj = SlotDemo()
# Overwriting on the instance dictionary
obj.__len__ = lambda: 999

# Python queries type(obj).__len__, NOT obj.__dict__['__len__']
print(f"len(obj) calls class slot: {len(obj)}")
print(f"Direct attribute call: {obj.__len__()}")
```
Output:
```
len(obj) calls class slot: 100
Direct attribute call: 999
```

---

## 4. Common Anti-Patterns & Gotchas

| Bad Practice | Root Cause | Clean Solution |
| :--- | :--- | :--- |
| Implementing `__str__` without `__repr__` | When `__repr__` is omitted, Python prints ugly `<ClassName object at 0x...>` in lists and dicts | Always implement `__repr__` first; add `__str__` only if user formatting differs |
| Overwriting dunder methods on instance: `obj.__len__ = ...` | CPython checks class type slots, ignoring instance `__dict__` | Define dunder methods on the class or metaclass |
| Suppressing unexpected exceptions in `__exit__` via `return True` | Hides bugs, crashes, and syntax errors | Return `False` by default; suppress only known, expected exceptions |
| Returning non-boolean from `__bool__` | Python expects a strict `bool` return value | Return `bool(x)` or `True`/`False` |
| Calling dunder methods directly: `obj.__len__()` | Circumvents C-level slot optimizations and standard type conversions | Use built-in functions: `len(obj)`, `str(obj)`, `repr(obj)` |
| Raising exceptions inside `__repr__` | Breaks traceback formatting and logging inside error handlers | Ensure `__repr__` is robust and never raises exceptions |

---

## 5. Interview Q&A

<details>
<summary>1. Why does CPython look up special methods on the class rather than the instance?</summary>
CPython resolves dunder methods directly on the class (<code>type(obj)</code>) for two reasons:
1. <b>Performance</b>: Classes cache dunder function pointers in fast C-level type slots (e.g. <code>tp_repr</code>, <code>sq_length</code>), avoiding slow hash table lookups in <code>obj.__dict__</code>.
2. <b>Consistency</b>: It prevents ambiguous behavior where an instance attribute shadows a fundamental language protocol or metaclass hook.
</details>

<details>
<summary>2. What is the fallback rule between __repr__ and __str__?</summary>
If <code>__str__</code> is not defined on a class, Python automatically falls back to calling <code>__repr__</code>. However, the reverse is not true: if only <code>__str__</code> is defined, <code>repr(obj)</code> will NOT fall back to <code>__str__</code>; it will use <code>object.__repr__</code> (producing <code>&lt;Class object at 0x...&gt;</code>).
</details>

<details>
<summary>3. What does __bool__ do, and what is its fallback if not implemented?</summary>
<code>__bool__(self) -> bool</code> defines the truth value of an object (used in <code>if obj:</code> or <code>bool(obj)</code>). If <code>__bool__</code> is not defined, Python falls back to <code>__len__</code> (an object with length > 0 is True, length == 0 is False). If neither is defined, all instances of the class are considered <code>True</code>.
</details>

<details>
<summary>4. What are the arguments to __exit__, and what does its return value mean?</summary>
<code>__exit__(self, exc_type, exc_val, exc_tb)</code> receives the exception class, exception instance, and traceback if an exception occurred inside the <code>with</code> block (all <code>None</code> if no exception occurred). If <code>__exit__</code> returns a truthy value (e.g. <code>True</code>), the exception is swallowed. If it returns <code>False</code> or <code>None</code>, the exception propagates.
</details>

<details>
<summary>5. How does the in operator resolve membership?</summary>
The expression <code>x in obj</code> first looks for <code>__contains__</code> on <code>type(obj)</code>. If absent, it falls back to iterating via <code>__iter__</code>. If <code>__iter__</code> is absent, it falls back to indexing via <code>__getitem__</code> starting from 0 until <code>IndexError</code>.
</details>

<details>
<summary>6. What makes an object callable in Python?</summary>
An object is callable if its class defines the <code>__call__(self, *args, **kwargs)</code> dunder method. When invoked with parentheses (<code>obj()</code>), Python routes the call to <code>type(obj).__call__(obj, *args, **kwargs)</code>.
</details>

<details>
<summary>7. What is the format specification dunder __format__?</summary>
<code>__format__(self, format_spec: str) -> str</code> is invoked when an object is formatted inside an f-string or <code>format()</code> with a specifier (e.g. <code>f"{obj:0.2f}"</code>). It receives the string after the colon and returns the custom formatted string.
</details>

<details>
<summary>8. Why should __repr__ ideally be eval()-able?</summary>
The informal Python convention is that <code>eval(repr(obj)) == obj</code> when feasible. This makes debugging, logging, and reproducing test cases seamless because pasting the representation into a Python REPL reconstructs the exact object state.
</details>

---

## 6. Practical Challenges

<details>
<summary>Challenge 1: Complete Developer-Friendly Point Class</summary>

Build a 2D `Point` class implementing `__repr__`, `__str__`, `__format__` (supporting `".2f"` formatting), and `__eq__`.

```python
class Point:
    def __init__(self, x: float, y: float):
        self.x = float(x)
        self.y = float(y)

    def __repr__(self) -> str:
        return f"Point({self.x}, {self.y})"

    def __str__(self) -> str:
        return f"({self.x}, {self.y})"

    def __format__(self, spec: str) -> str:
        if spec:
            return f"({self.x:{spec}}, {self.y:{spec}})"
        return str(self)

    def __eq__(self, other) -> bool:
        if isinstance(other, Point):
            return self.x == other.x and self.y == other.y
        return NotImplemented

p = Point(1.2345, 6.7891)
assert repr(p) == "Point(1.2345, 6.7891)"
assert str(p) == "(1.2345, 6.7891)"
assert f"{p:.2f}" == "(1.23, 6.79)"
assert p == Point(1.2345, 6.7891)
```
</details>

<details>
<summary>Challenge 2: Smart Custom Range Sequence</summary>

Implement an immutable sequence class `SimpleRange` defining `__len__`, `__getitem__`, and `__contains__` in $O(1)$ time without expanding into a list.

```python
class SimpleRange:
    def __init__(self, start: int, stop: int):
        self.start = start
        self.stop = stop

    def __len__(self) -> int:
        return max(0, self.stop - self.start)

    def __getitem__(self, idx: int) -> int:
        if idx < 0:
            idx += len(self)
        if 0 <= idx < len(self):
            return self.start + idx
        raise IndexError("Range index out of range")

    def __contains__(self, val: int) -> bool:
        return self.start <= val < self.stop

r = SimpleRange(10, 20)
assert len(r) == 10
assert r[0] == 10
assert r[-1] == 19
assert 15 in r
assert 25 not in r
```
</details>

<details>
<summary>Challenge 3: Exception-Suppressing Context Manager</summary>

Implement a context manager `Suppress(*exceptions)` that swallows specified exception types and lets other exceptions propagate.

```python
class Suppress:
    def __init__(self, *exceptions):
        self.exceptions = exceptions

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        if exc_type is not None and issubclass(exc_type, self.exceptions):
            return True
        return False

with Suppress(KeyError, ValueError):
    raise KeyError("Suppressed")

# Should not raise!
```
</details>

<details>
<summary>Challenge 4: Stateful Callable Memoizer</summary>

Implement a class `CallCounter` that wraps any function via `__call__` and records the total number of invocations in an attribute `calls`.

```python
class CallCounter:
    def __init__(self, func):
        self.func = func
        self.calls = 0

    def __call__(self, *args, **kwargs):
        self.calls += 1
        return self.func(*args, **kwargs)

@CallCounter
def compute(x):
    return x * 2

assert compute(5) == 10
assert compute(10) == 20
assert compute.calls == 2
```
</details>

<details>
<summary>Challenge 5: Lazy File Line Indexer via __getitem__</summary>

Build a class `LineIndexer(io_stream)` that allows lazy line indexing `indexer[n]` via `__getitem__`.

```python
import io

class LineIndexer:
    def __init__(self, stream):
        self.lines = stream.getvalue().splitlines()

    def __getitem__(self, index):
        return self.lines[index]

stream = io.StringIO("Line 0\nLine 1\nLine 2")
idx = LineIndexer(stream)
assert idx[1] == "Line 1"
```
</details>

<details>
<summary>Challenge 6: Reverse Dunder Hash & Equality Contract</summary>

Demonstrate why defining `__eq__` without `__hash__` makes instances unhashable, and restore it with a composite hash.

```python
class KeyItem:
    def __init__(self, key: str, val: int):
        self.key = key
        self.val = val

    def __eq__(self, other):
        if isinstance(other, KeyItem):
            return (self.key, self.val) == (other.key, other.val)
        return NotImplemented

    def __hash__(self):
        return hash((self.key, self.val))

k1 = KeyItem("a", 1)
k2 = KeyItem("a", 1)
s = {k1}
assert k2 in s
```
</details>
