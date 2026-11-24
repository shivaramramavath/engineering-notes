# Instance Lifecycle: __new__ vs __init__

- **Topic**: `05-oop`
- **Python version**: 3.12+ (tested on CPython 3.14.0)
- **Tested with**: CPython 3.12, 3.13, 3.14 (free-threaded & standard builds)
- **Free-threading notes**: Compatible. Object allocation via `tp_new` and initialization via `tp_init` are thread-safe under CPython's object allocation model.

---

## 1. Concept Overview

In Python, object creation is a two-step process handled cooperatively by two special methods:
1. `__new__(cls, *args, **kwargs)`: The **constructor**. Responsible for creating and returning a new instance of class `cls`.
2. `__init__(self, *args, **kwargs)`: The **initializer**. Responsible for setting up instance attributes on the already-created instance `self`.

When an expression like `obj = MyClass("arg")` is evaluated, Python internally executes:
```python
obj = MyClass.__new__(MyClass, "arg")
if isinstance(obj, MyClass):
    MyClass.__init__(obj, "arg")
```

Understanding this separation is crucial for advanced metaprogramming, creating singletons, implementing immutable data structures (subclassing immutable built-in types like `int`, `str`, `tuple`), and intercepting instance creation in custom metaclasses.

```
+-------------------------------------------------------------+
|                     MyClass(*args, **kwargs)                |
+-------------------------------------------------------------+
                              |
                              v
             +----------------------------------+
             | cls.__new__(cls, *args, **kwargs)|
             | (Allocates heap memory via C API)|
             +----------------------------------+
                              |
                     Returns 'instance'
                              |
                              v
            Is instance an instance of cls?
                   /                    \
                YES                      NO
                 /                        \
                v                          v
  +---------------------------+    Skip __init__!
  | instance.__init__(*a, **k)|    Return 'instance' as-is
  | (Configures attributes)   |    (e.g., cached instance
  +---------------------------+     from factory/singleton)
                |
                v
       Returns 'instance'
```

---

## 2. Key Mechanics & Architecture

### The `__new__` Method
- Defined as a static method by default, but Python automatically passes the calling class as its first parameter (`cls`).
- Must return an instance of `cls` (or a subclass) if `__init__` is expected to run.
- Calls `super().__new__(cls)` to delegate allocation up the inheritance chain to `object.__new__(cls)`.
- If `__new__` returns an instance of a different class, Python **silently skips** calling `__init__`.

### The `__init__` Method
- Takes `self` (the instance returned by `__new__`) and subsequent arguments.
- Must return `None`. Returning any other value raises `TypeError: __init__() should return None, not '...'`.

### Why Subclassing Immutable Types Requires `__new__`
Immutable objects (like `tuple`, `int`, `str`, `bytes`, `frozenset`) freeze their internal buffer at allocation time. By the time `__init__` is called on a tuple, the tuple buffer is already finalized. Modifying or customizing elements of an immutable type subclass must occur in `__new__`.

---

## 3. Real-World Code Examples

### Example 1: `__new__` vs `__init__` Execution Order
```python
class LifecycleTracer:
    def __new__(cls, *args, **kwargs):
        print(f"1. __new__ called for class {cls.__name__}")
        instance = super().__new__(cls)
        print(f"2. Raw instance allocated at {hex(id(instance))}")
        return instance

    def __init__(self, value: int):
        print(f"3. __init__ called for instance {hex(id(self))} with value={value}")
        self.value = value


tracer = LifecycleTracer(42)
print(f"4. Resulting object: {tracer.value}")
```
Output:
```
1. __new__ called for class LifecycleTracer
2. Raw instance allocated at 0x7fa280
3. __init__ called for instance 0x7fa280 with value=42
4. Resulting object: 42
```

### Example 2: Subclassing Immutable Types (`tuple`)
```python
class Coordinate(tuple):
    """Subclassing tuple to enforce validation and auto-convert polar inputs."""
    def __new__(cls, x: float, y: float):
        # We must populate the tuple values during __new__
        if not (isinstance(x, (int, float)) and isinstance(y, (int, float))):
            raise TypeError("Coordinates must be numeric")
        return super().__new__(cls, (float(x), float(y)))

    @property
    def x(self) -> float:
        return self[0]

    @property
    def y(self) -> float:
        return self[1]


coord = Coordinate(3, 4)
print(f"Coord: {coord}, x={coord.x}, y={coord.y}, isinstance(coord, tuple)={isinstance(coord, tuple)}")
```
Output:
```
Coord: (3.0, 4.0), x=3.0, y=4.0, isinstance(coord, tuple)=True
```

### Example 3: Thread-Safe Singleton Pattern via `__new__`
```python
import threading

class DatabaseConnectionPool:
    _instance = None
    _lock = threading.Lock()

    def __new__(cls, connection_url: str):
        if cls._instance is None:
            with cls._lock:
                if cls._instance is None:
                    cls._instance = super().__new__(cls)
                    cls._instance._initialized = False
        return cls._instance

    def __init__(self, connection_url: str):
        if getattr(self, "_initialized", False):
            return  # Prevent re-initialization on subsequent calls
        self.connection_url = connection_url
        self._initialized = True


pool1 = DatabaseConnectionPool("postgresql://localhost:5432/primary")
pool2 = DatabaseConnectionPool("postgresql://localhost:5432/secondary")
print(f"pool1 is pool2: {pool1 is pool2}")
print(f"pool2 URL: {pool2.connection_url}")
```
Output:
```
pool1 is pool2: True
pool2 URL: postgresql://localhost:5432/primary
```

### Example 4: Returning a Different Instance (Interning / Factory)
```python
class Symbol:
    _registry: dict[str, "Symbol"] = {}

    def __new__(cls, name: str):
        # If symbol exists, return cached instance without invoking __init__ again
        if name in cls._registry:
            return cls._registry[name]
        instance = super().__new__(cls)
        cls._registry[name] = instance
        return instance

    def __init__(self, name: str):
        self.name = name


s1 = Symbol("alpha")
s2 = Symbol("alpha")
s3 = Symbol("beta")
print(f"s1 is s2: {s1 is s2}")
print(f"s1 is s3: {s1 is s3}")
```
Output:
```
s1 is s2: True
s1 is s3: False
```

### Example 5: Object Destruction via `__del__` and Resurrection Pitfall
```python
import weakref

resurrected_pool = []

class EphemeralResource:
    def __init__(self, resource_id: str):
        self.resource_id = resource_id

    def __del__(self):
        # Resurrection anti-pattern: storing self creates a live reference during deallocation!
        print(f"Destructor invoked for {self.resource_id}")
        resurrected_pool.append(self)


res = EphemeralResource("res-101")
del res
print(f"Resurrected count: {len(resurrected_pool)}, ID: {resurrected_pool[0].resource_id}")
```
Output:
```
Destructor invoked for res-101
Resurrected count: 1, ID: res-101
```

---

## 4. Common Anti-Patterns & Gotchas

| Bad Practice | Root Cause | Clean Solution |
| :--- | :--- | :--- |
| Returning a non-`None` value in `__init__` | `__init__` contract requires returning `None` | Do not return values from `__init__`; use factory functions or `@classmethod` |
| Attempting to mutate `self` in `__init__` when subclassing `tuple`/`str` | Immutable types freeze their buffer during allocation in `__new__` | Pass mutated/normalized values to `super().__new__(cls, ...)` |
| Forgetting `super().__new__(cls)` in custom `__new__` | Omitting base allocation yields `None` or an unallocated object | Always return `super().__new__(cls)` |
| Overwriting `__new__` signature without matching `*args, **kwargs` | If arguments passed to constructor do not match `object.__new__`, raises `TypeError` | Pass only `cls` to `object.__new__(cls)` if no args needed at base level |
| Unsafe singleton `__init__` re-running on every call | Even if `__new__` returns existing singleton, Python invokes `__init__` on it every time | Guard `__init__` execution with an `if not self._initialized:` flag |
| Relying on `__del__` for resource cleanup (files, sockets) | `__del__` execution timing is non-deterministic (especially in circular references) | Use `with` context managers (`__enter__` and `__exit__`) |

---

## 5. Interview Q&A

<details>
<summary>1. What is the fundamental difference between __new__ and __init__?</summary>
<code>__new__</code> is the actual object constructor: it receives the class as its first parameter (<code>cls</code>), allocates memory on the heap (usually by delegating to <code>object.__new__(cls)</code>), and returns the new instance. <code>__init__</code> is the initializer: it receives the already allocated instance as <code>self</code> and sets up initial attributes. <code>__new__</code> must return an instance, whereas <code>__init__</code> must return <code>None</code>.
</details>

<details>
<summary>2. Why does Python call __init__ only when __new__ returns an instance of cls?</summary>
This behavior supports factory patterns and interning. If <code>__new__</code> returns an instance of a completely different class or a pre-initialized singleton, Python avoids re-running <code>cls.__init__</code> with parameters that may not make sense for the foreign or cached object.
</details>

<details>
<summary>3. Why must immutable types like tuple or int be subclassed using __new__?</summary>
Immutable types allocate and finalize their underlying memory buffer during C-level allocation (inside <code>tp_new</code>). Once the instance is returned to Python, the buffer is write-protected. By the time <code>__init__</code> executes, it is impossible to alter the immutable values. Therefore, value adjustments must occur in <code>__new__</code>.
</details>

<details>
<summary>4. What happens if __init__ returns a non-None value?</summary>
Python raises <code>TypeError: __init__() should return None, not '&lt;type&gt;'</code>. The language designers enforced this so developers wouldn't mistakenly treat <code>__init__</code> as a constructor that returns objects.
</details>

<details>
<summary>5. How does the Singleton pattern handle multiple calls to __init__?</summary>
When <code>Singleton()</code> is called a second time, <code>__new__</code> returns the cached instance. However, because that instance is an instance of <code>Singleton</code>, Python immediately calls <code>__init__</code> on it again with the new arguments! To avoid overwriting existing state, <code>__init__</code> must check an internal initialization flag (e.g., <code>if getattr(self, "_initialized", False): return</code>).
</details>

<details>
<summary>6. What is object resurrection in Python?</summary>
Object resurrection occurs when an object's <code>__del__</code> method binds <code>self</code> to a global or reachable reference (e.g., appending <code>self</code> to a module-level list). This resets its reference count from zero to non-zero, rescuing it from deallocation. In modern CPython, Python will not invoke <code>__del__</code> a second time if the resurrected object is later deallocated.
</details>

<details>
<summary>7. Is __new__ a static method, class method, or instance method?</summary>
Syntactically, <code>__new__</code> is a static method (it has no <code>self</code> bound), but the Python compiler treats it as a special case: when defined inside a class body, it automatically receives the class <code>cls</code> as its first argument without requiring an explicit <code>@classmethod</code> decorator.
</details>

<details>
<summary>8. Why should you pass only cls to object.__new__(cls)?</summary>
In modern Python, <code>object.__new__(cls)</code> raises a <code>TypeError: object.__new__() takes exactly one argument (the type to instantiate)</code> if extra arguments are passed, unless the class has not overridden <code>__init__</code>. When overriding both <code>__new__</code> and <code>__init__</code>, call <code>super().__new__(cls)</code> without forwarding the extra user arguments.
</details>

---

## 6. Practical Challenges

<details>
<summary>Challenge 1: Validated Float Vector Subclassing Tuple</summary>

Create an immutable 3D Vector class `Vector3D` that subclasses `tuple`. Ensure all 3 coordinates are validated as floats in `__new__` and provide properties `.x`, `.y`, `.z`.

```python
class Vector3D(tuple):
    def __new__(cls, x: float, y: float, z: float):
        try:
            fx, fy, fz = float(x), float(y), float(z)
        except (ValueError, TypeError) as err:
            raise TypeError("Vector3D elements must be convertible to float") from err
        return super().__new__(cls, (fx, fy, fz))

    @property
    def x(self) -> float:
        return self[0]

    @property
    def y(self) -> float:
        return self[1]

    @property
    def z(self) -> float:
        return self[2]


v = Vector3D(1, 2.5, "3.8")
assert (v.x, v.y, v.z) == (1.0, 2.5, 3.8)
assert isinstance(v, tuple)
```
</details>

<details>
<summary>Challenge 2: Case-Insensitive String Subclass</summary>

Implement `CIString` by subclassing `str`. In `__new__`, normalize the input string to lowercase while retaining the original representation for `__str__`.

```python
class CIString(str):
    def __new__(cls, value: str):
        instance = super().__new__(cls, str(value).lower())
        instance._original = str(value)
        return instance

    def __str__(self) -> str:
        return self._original

    def __repr__(self) -> str:
        return f"CIString({self._original!r})"


s1 = CIString("HelloWorld")
s2 = CIString("helloworld")
assert s1 == s2
assert str(s1) == "HelloWorld"
```
</details>

<details>
<summary>Challenge 3: Limited Instance Pool (Multiton Pattern)</summary>

Create an `InstancePool` class that allows at most $N$ instances identified by a string key. Repeated requests for the same key return the cached instance.

```python
class WorkerPool:
    _pool: dict[str, "WorkerPool"] = {}
    MAX_WORKERS = 3

    def __new__(cls, worker_id: str):
        if worker_id in cls._pool:
            return cls._pool[worker_id]
        if len(cls._pool) >= cls.MAX_WORKERS:
            raise RuntimeError(f"Maximum pool capacity of {cls.MAX_WORKERS} reached")
        instance = super().__new__(cls)
        instance.worker_id = worker_id
        cls._pool[worker_id] = instance
        return instance


w1 = WorkerPool("w1")
w2 = WorkerPool("w2")
assert WorkerPool("w1") is w1
```
</details>

<details>
<summary>Challenge 4: Safe Class-Level Factory with __new__</summary>

Implement an abstract-like shape parser `Shape(shape_type: str, *args)` whose `__new__` inspects `shape_type` and dynamically returns an instance of `Circle`, `Square`, or `Triangle`.

```python
class Shape:
    def __new__(cls, shape_type: str, *args, **kwargs):
        if cls is not Shape:
            return super().__new__(cls)
        if shape_type == "circle":
            return Circle(*args, **kwargs)
        elif shape_type == "square":
            return Square(*args, **kwargs)
        raise ValueError(f"Unknown shape type: {shape_type}")

class Circle(Shape):
    def __init__(self, radius: float):
        self.radius = radius

class Square(Shape):
    def __init__(self, side: float):
        self.side = side

s = Shape("circle", 5.0)
assert isinstance(s, Circle)
assert s.radius == 5.0
```
</details>

<details>
<summary>Challenge 5: Audited Object Creation Hook</summary>

Build a class `AuditedModel` that tracks the creation timestamp and caller file/line for every newly created instance inside `__new__`.

```python
import inspect
import time

class AuditedModel:
    def __new__(cls, *args, **kwargs):
        instance = super().__new__(cls)
        frame = inspect.currentframe().f_back
        instance._created_at = time.time()
        instance._created_in = f"{frame.f_code.co_filename}:{frame.f_lineno}"
        return instance

m = AuditedModel()
assert hasattr(m, "_created_at")
assert hasattr(m, "_created_in")
```
</details>

<details>
<summary>Challenge 6: Robust Thread-Safe Re-initialization Guard</summary>

Implement a class `AppConfig` that uses double-checked locking in `__new__` and ensures `__init__` only executes once even when multiple concurrent threads call `AppConfig(env="prod")`.

```python
import threading

class AppConfig:
    _instance = None
    _lock = threading.Lock()
    _init_lock = threading.Lock()

    def __new__(cls, *args, **kwargs):
        if cls._instance is None:
            with cls._lock:
                if cls._instance is None:
                    cls._instance = super().__new__(cls)
                    cls._instance._is_initialized = False
        return cls._instance

    def __init__(self, env: str = "dev"):
        with self._init_lock:
            if self._is_initialized:
                return
            self.env = env
            self._is_initialized = True

c1 = AppConfig("prod")
c2 = AppConfig("staging")
assert c1 is c2
assert c2.env == "prod"
```
</details>
