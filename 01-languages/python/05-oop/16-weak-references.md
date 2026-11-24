# Weak References: weakref, WeakValueDictionary, and Cyclic Garbage Collection

- **Topic**: `05-oop`
- **Python version**: 3.12+ (tested on CPython 3.14.0)
- **Tested with**: CPython 3.12, 3.13, 3.14 (standard & free-threaded builds)
- **Free-threading notes**: Compatible. In free-threaded Python, weak reference lookups and clearing are synchronized to prevent race conditions during concurrent object deallocation.

---

## 1. Concept Overview

CPython's primary memory management system is **reference counting**. Every object tracks the number of active references pointing to it. When an object's reference count drops to zero, its memory is deallocated immediately.

However, reference counting cannot reclaim **reference cycles** (e.g. Object A references Object B, and Object B references Object A). While CPython's generational cyclic garbage collector periodically detects and sweeps unreachable cycles, keeping unwanted strong references alive can cause severe memory bloat, especially in caching, observer patterns, and parent-child tree structures.

The `weakref` module solves this by allowing you to reference an object **without increasing its reference count**. When the referent is garbage collected, the weak reference is automatically cleared (returns `None` or triggers an optional callback).

Key constructs include:
1. `weakref.ref(obj, callback=None)`: Creates a callable weak reference returning the live object or `None`.
2. `weakref.proxy(obj)`: Creates a transparent proxy mimicking the referent until deallocation.
3. `weakref.WeakValueDictionary`: A dictionary whose values are weakly held (keys are automatically purged when values are collected).
4. `weakref.WeakKeyDictionary`: A dictionary whose keys are weakly held.
5. `weakref.WeakSet`: A set of weakly held elements.

```
Strong Reference (Increments refcount):
[Variable x] -------------> [Object Target] (refcount = 1)

Weak Reference (Refcount unaffected):
[Variable x] -------------> [Object Target] (refcount = 1)
                                  ^
                                  : (does NOT increment refcount!)
[weakref.ref] ....................+

When 'del x' executes:
[Object Target] is immediately deallocated!
weakref.ref() now safely returns None.
```

---

## 2. Key Mechanics & Architecture

### What Objects Support Weak References?
To be weakly referenced, an object must define a `__weakref__` slot.
- **Custom Classes**: By default, standard custom classes define a `__dict__` and `__weakref__`, allowing them to be weakly referenced.
- **Slotted Classes**: Slotted classes **forbid** weak references unless `'__weakref__'` is explicitly included in `__slots__`.
- **Built-in Types**: Primitive built-ins (`int`, `str`, `list`, `dict`, `tuple`) **do not** support weak references directly (attempting to create one raises `TypeError: cannot create weak reference to 'list' object`). Subclassing them adds a `__weakref__` slot.

### Weak Reference Lifecycle & Finalization Callbacks
When an object with active weak references dies:
1. Object refcount hits 0.
2. CPython clears all associated weak references, setting their internal pointer to `NULL`.
3. Registered callbacks `callback(ref)` are executed in LIFO order.
4. Memory is deallocated.

---

## 3. Real-World Code Examples

### Example 1: Basic `weakref.ref` and Dereferencing
```python
import weakref

class HeavyResource:
    def __init__(self, name: str):
        self.name = name

    def __repr__(self):
        return f"HeavyResource({self.name!r})"


res = HeavyResource("Model-Weights")
# Create weak reference
wref = weakref.ref(res)

print(f"Referent while alive: {wref()}")
print(f"Target name: {wref().name}")

# Delete the strong reference
del res

# Weak reference now returns None
print(f"Referent after del res: {wref()}")
```
Output:
```
Referent while alive: HeavyResource('Model-Weights')
Target name: Model-Weights
Referent after del res: None
```

### Example 2: Invalidation Callbacks on Object Collection
```python
import weakref

def on_finalize(ref):
    print("Cleanup callback: Resource has been collected from memory!")

class ManagedBuffer:
    def __init__(self, size: int):
        self.size = size


buf = ManagedBuffer(1024)
# Pass cleanup callback to weakref constructor
wref = weakref.ref(buf, on_finalize)

print("Deleting strong reference buf...")
del buf
print("Execution continues...")
```
Output:
```
Deleting strong reference buf...
Cleanup callback: Resource has been collected from memory!
Execution continues...
```

### Example 3: Memory-Leak-Free Caching with `WeakValueDictionary`
```python
import weakref

class TextureImage:
    def __init__(self, filename: str):
        self.filename = filename

    def __repr__(self):
        return f"<Texture: {self.filename}>"


# Cache textures weakly so unreferenced textures are purged automatically
texture_cache = weakref.WeakValueDictionary()

def get_texture(filename: str) -> TextureImage:
    if filename not in texture_cache:
        img = TextureImage(filename)
        texture_cache[filename] = img
        return img
    return texture_cache[filename]


tex1 = get_texture("rock.png")
tex2 = get_texture("grass.png")
print(f"Cached textures count: {len(texture_cache)}")
print(f"Cache keys: {list(texture_cache.keys())}")

# Deleting strong reference to grass.png
del tex2
print(f"Cached textures after del tex2: {list(texture_cache.keys())}")
```
Output:
```
Cached textures count: 2
Cache keys: ['rock.png', 'grass.png']
Cached textures after del tex2: ['rock.png']
```

### Example 4: Breaking Parent-Child Tree Cycles
```python
import weakref

class TreeNode:
    def __init__(self, val: str, parent: "TreeNode" | None = None):
        self.val = val
        self.children: list["TreeNode"] = []
        # Store parent weakly to prevent circular reference leak
        self._parent = weakref.ref(parent) if parent is not None else None

    @property
    def parent(self) -> "TreeNode | None":
        return self._parent() if self._parent is not None else None

    def add_child(self, child_val: str) -> "TreeNode":
        child = TreeNode(child_val, parent=self)
        self.children.append(child)
        return child


root = TreeNode("Root")
child = root.add_child("Child-1")

print(f"Child: {child.val}, Parent: {child.parent.val}")
del root  # Child does not keep Root alive artificially!
print(f"Child parent after root deleted: {child.parent}")
```
Output:
```
Child: Child-1, Parent: Root
Child parent after root deleted: None
```

### Example 5: Transparent Proxies via `weakref.proxy`
```python
import weakref

class ServiceEndpoint:
    def ping(self) -> str:
        return "pong"


ep = ServiceEndpoint()
# Proxy acts like the object directly, without calling proxy()
proxy = weakref.proxy(ep)

print(f"Proxy ping: {proxy.ping()}")

del ep
try:
    proxy.ping()
except ReferenceError as err:
    print(f"Proxy error caught: {err}")
```
Output:
```
Proxy ping: pong
Proxy error caught: weakly-referenced object no longer exists
```

---

## 4. Common Anti-Patterns & Gotchas

| Bad Practice | Root Cause | Clean Solution |
| :--- | :--- | :--- |
| Attempting to weakref built-in types: `weakref.ref([1, 2])` | Built-in primitive types (`list`, `dict`, `int`) do not have `__weakref__` slots | Subclass the built-in or wrap it in a custom class |
| Attempting to weakref a slotted class missing `'__weakref__'` | Slotted classes omit `__weakref__` by default to save memory | Include `'__weakref__'` in the class `__slots__` |
| Calling methods on a dead weakref without checking: `wref().do_something()` | If referent was collected, `wref()` is `None`, raising `AttributeError` | Bind to local variable and test: `obj = wref(); if obj is not None: ...` |
| Using standard `dict` for caches of long-lived objects | Strong dictionary references prevent GC, causing memory leaks | Use `weakref.WeakValueDictionary` or `WeakKeyDictionary` |
| Using `weakref.proxy` and assuming it will safely return `None` | Accessing an expired proxy raises `ReferenceError`, not `None` | Use `weakref.ref` if checking for existence is required |
| Storing hard references to `self` inside callbacks | Callback closures holding `self` recreate the reference cycle! | Pass only weak references or use module-level standalone callbacks |

---

## 5. Interview Q&A

<details>
<summary>1. What is a weak reference and why is it useful?</summary>
A weak reference is a reference that allows accessing an object without increasing its reference count. It is primarily useful for breaking reference cycles (e.g. parent pointers in tree hierarchies, observer subscriptions) and implementing memory-sensitive caches that do not prevent their stored objects from being reclaimed by the garbage collector.
</details>

<details>
<summary>2. What happens when you attempt to dereference a dead weakref?</summary>
Calling a <code>weakref.ref()</code> object whose target has been deallocated returns <code>None</code>. If you are using a <code>weakref.proxy()</code>, attempting to access any attribute or method on the expired proxy raises a <code>ReferenceError: weakly-referenced object no longer exists</code>.
</details>

<details>
<summary>3. Why can't standard Python built-ins like list, dict, or int be weakly referenced?</summary>
CPython optimizes built-in primitive types for memory and speed. Omitting the <code>__weakref__</code> pointer saves 8 bytes on every integer, string, and list allocated across the entire Python runtime. If weak referencing is required, subclass the built-in (e.g. <code>class WeakableList(list): pass</code>).
</details>

<details>
<summary>4. What is the difference between WeakValueDictionary and WeakKeyDictionary?</summary>
- <b>WeakValueDictionary</b>: Holds its <i>values</i> weakly. When an object stored as a value is collected, its key-value entry is automatically deleted from the dictionary (ideal for object caches).
- <b>WeakKeyDictionary</b>: Holds its <i>keys</i> weakly. When a key object is collected, the entire entry is deleted (ideal for associating metadata with objects without modifying them).
</details>

<details>
<summary>5. How do you make a slotted class compatible with weak references?</summary>
By explicitly adding <code>'__weakref__'</code> to the <code>__slots__</code> tuple:
<pre><code>class SlottedModel:
    __slots__ = ("id", "name", "__weakref__")
</code></pre>
This allocates the single 8-byte C pointer needed for the weak reference linked list.
</details>

<details>
<summary>6. How do weak references interact with cyclic garbage collection?</summary>
Weak references do not form cycles because they do not increment reference counts. Therefore, cycles broken with weak references can be collected immediately by standard reference counting ($O(0)$ delay) without waiting for the cyclic garbage collector's generational scan.
</details>

<details>
<summary>7. What is weakref.finalize?</summary>
Introduced in Python 3.4, <code>weakref.finalize(obj, func, *args, **kwargs)</code> provides a cleaner, more reliable alternative to <code>__del__</code>. It registers a callable to run when <code>obj</code> is garbage collected, without the object resurrection risks and cycle traps associated with <code>__del__</code>.
</details>

<details>
<summary>8. Can you weakref a lambda function?</summary>
Yes. In Python, function objects (including lambdas) support weak references because <code>types.FunctionType</code> includes a <code>__weakref__</code> slot.
</details>

---

## 6. Practical Challenges

<details>
<summary>Challenge 1: Observer Pattern with Weak References</summary>

Implement an `EventEmitter` where subscribers are stored in a `WeakSet`, ensuring that if a subscriber is deleted, it is automatically unsubscribed without memory leaks.

```python
import weakref

class EventEmitter:
    def __init__(self):
        self._subscribers = weakref.WeakSet()

    def subscribe(self, subscriber):
        self._subscribers.add(subscriber)

    def emit(self, event: str) -> list[str]:
        return [sub.on_event(event) for sub in self._subscribers]

class Listener:
    def __init__(self, name): self.name = name
    def on_event(self, event): return f"{self.name}:{event}"

emitter = EventEmitter()
l1 = Listener("L1")
l2 = Listener("L2")
emitter.subscribe(l1)
emitter.subscribe(l2)
assert len(emitter._subscribers) == 2
del l1
assert len(emitter._subscribers) == 1
```
</details>

<details>
<summary>Challenge 2: Automatic Instance Registry with WeakValueDictionary</summary>

Create an `Entity` class whose instances are automatically indexed by `entity_id` in a `WeakValueDictionary`. Verify instances disappear from the registry on `del`.

```python
import weakref

class Entity:
    _registry = weakref.WeakValueDictionary()

    def __init__(self, entity_id: str):
        self.entity_id = entity_id
        Entity._registry[entity_id] = self

    @classmethod
    def get_by_id(cls, entity_id: str):
        return cls._registry.get(entity_id)

e = Entity("E-100")
assert Entity.get_by_id("E-100") is e
del e
assert Entity.get_by_id("E-100") is None
```
</details>

<details>
<summary>Challenge 3: Cyclic Graph Node with Weak Back-Edges</summary>

Implement a `GraphNode` that maintains strong references to downstream children, but weak references to upstream parents.

```python
import weakref

class GraphNode:
    def __init__(self, name: str):
        self.name = name
        self.outgoing = []
        self._incoming = []

    def connect_to(self, other: "GraphNode"):
        self.outgoing.append(other)
        other._incoming.append(weakref.ref(self))

    @property
    def incoming_nodes(self):
        return [r() for r in self._incoming if r() is not None]

n1 = GraphNode("A")
n2 = GraphNode("B")
n1.connect_to(n2)
assert n2.incoming_nodes == [n1]
del n1
assert n2.incoming_nodes == []
```
</details>

<details>
<summary>Challenge 4: Modern Resource Cleanup with weakref.finalize</summary>

Build a `TempFileManager` that writes data to a mock path and registers `weakref.finalize` to guarantee cleanup when the instance is collected.

```python
import weakref

class TempFileManager:
    cleanup_called = False

    def __init__(self, path: str):
        self.path = path
        # Finalizer must not reference self directly!
        weakref.finalize(self, self._cleanup, path)

    @classmethod
    def _cleanup(cls, path: str):
        cls.cleanup_called = True

f = TempFileManager("/tmp/test.dat")
assert not TempFileManager.cleanup_called
del f
assert TempFileManager.cleanup_called
```
</details>

<details>
<summary>Challenge 5: Weakly-Referenced Slotted Object</summary>

Build a slotted class `SlottedResource` with fields `name, handle, __weakref__`. Verify that creating a `weakref.ref` succeeds.

```python
import weakref

class SlottedResource:
    __slots__ = ("name", "handle", "__weakref__")

    def __init__(self, name: str, handle: int):
        self.name = name
        self.handle = handle

res = SlottedResource("GPU-0", 123)
w = weakref.ref(res)
assert w().handle == 123
del res
assert w() is None
```
</details>

<details>
<summary>Challenge 6: Invalidation Callback Counter</summary>

Implement a registry that increments a dead object counter `reclaimed_count` via a weakref callback whenever an instance of `Worker` is destroyed.

```python
import weakref

class WorkerTracker:
    reclaimed_count = 0

    @classmethod
    def _on_reclaim(cls, ref):
        cls.reclaimed_count += 1

    @classmethod
    def track(cls, worker):
        return weakref.ref(worker, cls._on_reclaim)

class Worker:
    pass

w1 = Worker()
w2 = Worker()
ref1 = WorkerTracker.track(w1)
ref2 = WorkerTracker.track(w2)
assert WorkerTracker.reclaimed_count == 0
del w1
assert WorkerTracker.reclaimed_count == 1
del w2
assert WorkerTracker.reclaimed_count == 2
```
</details>
