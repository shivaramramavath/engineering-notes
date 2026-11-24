# The super() Built-in and Cooperative Multiple Inheritance

- **Topic**: `05-oop`
- **Python version**: 3.12+ (tested on CPython 3.14.0)
- **Tested with**: CPython 3.12, 3.13, 3.14 (standard & free-threaded builds)
- **Free-threading notes**: Compatible. Zero-argument `super()` relies on compiler-injected `__class__` cell variables, which are thread-safe and immutable at runtime.

---

## 1. Concept Overview

The `super()` built-in is often thought of as a way to call a "parent class" method. In Python, this mental model is dangerously incomplete.

`super()` does **not** call the direct parent of the defining class. Instead, it delegates method calls to the **next class in the Method Resolution Order (MRO)** of the runtime object.

In modern Python 3, calling `super()` without arguments inside a method automatically resolves to:
```python
super(__class__, self)
```
where `__class__` is a compiler-created closure variable capturing the enclosing class, and `self` is the first parameter of the method.

When multiple classes in a hierarchy all call `super()`, they form a **cooperative multiple inheritance chain**. Every class processes its own logic and forwards control along the MRO, guaranteeing that every participating class is invoked exactly once.

```
MRO: [Derived, MixinA, MixinB, Base, object]

Derived.method()
   |
   +--> calls super().method()
           |
           v
        MixinA.method()
           |
           +--> calls super().method()
                   |
                   v
                MixinB.method()
                   |
                   +--> calls super().method()
                           |
                           v
                        Base.method()
```

---

## 2. Key Mechanics & Architecture

### Arguments to `super()`
The full signature of `super()` is:
```python
super(type=..., object_or_type=...)
```
1. **Zero-argument form (standard)**: `super()`. Python injects `__class__` (the class where the method was defined) and `self` (or `cls`).
2. **Bound instance form**: `super(SubClass, self)`. Searches the MRO of `self`, starting strictly **after** `SubClass`.
3. **Bound class form**: `super(SubClass, SubClass)`. Used in `@classmethod` to call class methods on the next class in the class MRO.
4. **Unbound form**: `super(SubClass)`. Returns an unbound super object, rarely used directly.

### Rules of Cooperative Multiple Inheritance
To make cooperative multiple inheritance work without subtle `TypeError` bugs:
1. **Every intermediate class must call `super()`**: If any class fails to call `super()`, the cooperative chain terminates prematurely.
2. **Use `*args, **kwargs`**: Because different classes in the MRO may require different parameters in `__init__`, keyword arguments should be popped and unknown arguments forwarded to `super().__init__(**kwargs)`.
3. **Root class termination**: The ultimate base class (or `object`) consumes no unexpected arguments; ensure `object.__init__()` receives zero extra parameters.

---

## 3. Real-World Code Examples

### Example 1: Cooperative Multiple Inheritance in Action
```python
class BaseService:
    def handle(self, request: dict):
        print("-> BaseService: processing business logic")
        return {"status": "success", "data": request}

class AuthMixin(BaseService):
    def handle(self, request: dict):
        print("-> AuthMixin: verifying token")
        if not request.get("authenticated"):
            return {"status": "unauthorized"}
        return super().handle(request)

class LoggingMixin(BaseService):
    def handle(self, request: dict):
        print(f"-> LoggingMixin: incoming request {request.get('id')}")
        result = super().handle(request)
        print(f"-> LoggingMixin: outgoing response {result.get('status')}")
        return result

class SecureOrderService(LoggingMixin, AuthMixin):
    def handle(self, request: dict):
        print("-> SecureOrderService: starting workflow")
        return super().handle(request)


service = SecureOrderService()
req = {"id": "req-999", "authenticated": True, "item": "laptop"}
res = service.handle(req)
print(f"Final response: {res}")
```
Output:
```
-> SecureOrderService: starting workflow
-> LoggingMixin: incoming request req-999
-> AuthMixin: verifying token
-> BaseService: processing business logic
-> LoggingMixin: outgoing response success
Final response: {'status': 'success', 'data': {'id': 'req-999', 'authenticated': True, 'item': 'laptop'}}
```

### Example 2: Cooperative `__init__` with Keyword Arguments
```python
class RootComponent:
    def __init__(self, **kwargs):
        # Base termination: object.__init__ takes no arguments
        super().__init__()
        if kwargs:
            print(f"Warning: unused arguments: {kwargs}")

class Resizable(RootComponent):
    def __init__(self, width: int = 100, height: int = 100, **kwargs):
        super().__init__(**kwargs)
        self.width = width
        self.height = height

class Colorable(RootComponent):
    def __init__(self, color: str = "white", **kwargs):
        super().__init__(**kwargs)
        self.color = color

class Window(Resizable, Colorable):
    def __init__(self, title: str, **kwargs):
        super().__init__(**kwargs)
        self.title = title


w = Window(title="MainApp", width=800, height=600, color="dark-slate")
print(f"Window: '{w.title}' {w.width}x{w.height}, color={w.color}")
```
Output:
```
Window: 'MainApp' 800x600, color=dark-slate
```

### Example 3: Zero-Argument vs Explicit `super()`
```python
class Parent:
    def identify(self):
        return "Parent"

class Child(Parent):
    def identify(self):
        # 1. Modern zero-arg super
        zero_arg = super().identify()
        # 2. Explicit equivalent
        explicit_arg = super(Child, self).identify()
        return f"{zero_arg} == {explicit_arg}"


c = Child()
print(c.identify())
```
Output:
```
Parent == Parent
```

### Example 4: `super()` in Class Methods
```python
class ConfigBase:
    @classmethod
    def defaults(cls) -> dict:
        return {"timeout": 30, "debug": False}

class CloudConfig(ConfigBase):
    @classmethod
    def defaults(cls) -> dict:
        base_defaults = super().defaults()
        base_defaults.update({"region": "us-east-1", "retries": 3})
        return base_defaults


print(f"Cloud config defaults: {CloudConfig.defaults()}")
```
Output:
```
Cloud config defaults: {'timeout': 30, 'debug': False, 'region': 'us-east-1', 'retries': 3}
```

### Example 5: Inspecting the Behind-the-Scenes `__class__` Cell
```python
class Inspector:
    def show_class_cell(self):
        # Python compiler creates a closure cell named __class__ inside any method using super()
        return __class__


i = Inspector()
print(f"Compiler-injected __class__: {i.show_class_cell()}")
```
Output:
```
Compiler-injected __class__: <class '__main__.Inspector'>
```

---

## 4. Common Anti-Patterns & Gotchas

| Bad Practice | Root Cause | Clean Solution |
| :--- | :--- | :--- |
| Hardcoding `ParentClass.__init__(self, ...)` | Circumvents the MRO; causes diamond base classes to be initialized multiple times | Always use `super().__init__(...)` |
| Calling `super()` in a class that does not consume `**kwargs` | Extra arguments reach `object.__init__`, raising `TypeError` | Intercept specific keywords and forward `**kwargs` up the chain |
| Assuming `super()` always calls the direct parent | `super()` follows the runtime instance's MRO, which depends on the leaf subclass | Design classes cooperatively, not hierarchically rigid |
| Calling `super(self.__class__, self)` | If a subclass inherits this method, `self.__class__` is the subclass, causing an infinite loop | Use bare `super()` or explicit `super(DefiningClass, self)` |
| Omitting `super()` in a mixin method | Breaks the cooperative chain, starving all subsequent classes in the MRO | Always call `super().method(...)` in mixins |
| Modifying method signatures incompatibly across cooperative classes | In cooperative inheritance, any sibling can be called next | Keep cooperative method signatures uniform or use `*args, **kwargs` |

---

## 5. Interview Q&A

<details>
<summary>1. What does super() actually do in Python?</summary>
<code>super()</code> returns a proxy object that delegates method calls to the next class in the Method Resolution Order (MRO) of the object or class passed as the second argument, searching strictly after the class specified in the first argument. It does NOT simply call the direct parent of the defining class.
</details>

<details>
<summary>2. How does Python 3's zero-argument super() work without explicit parameters?</summary>
During compilation, whenever the Python compiler encounters a call to <code>super()</code> without arguments inside a class body, it automatically injects a hidden closure cell named <code>__class__</code> and captures the first argument of the method (typically <code>self</code> or <code>cls</code>). At runtime, <code>super()</code> inspects the frame to read these two values automatically.
</details>

<details>
<summary>3. Why is super(self.__class__, self) an anti-pattern?</summary>
If class <code>A</code> defines <code>super(self.__class__, self).foo()</code> and class <code>B</code> inherits from <code>A</code>, when <code>b = B()</code> calls <code>b.foo()</code>, <code>self.__class__</code> resolves to <code>B</code>! Python then searches the MRO starting after <code>B</code>, which finds <code>A.foo()</code> again, resulting in infinite recursion and a <code>RecursionError</code>.
</details>

<details>
<summary>4. What is Cooperative Multiple Inheritance?</summary>
Cooperative Multiple Inheritance is an architecture where every class in an inheritance graph cooperates by calling <code>super().method()</code>. This guarantees that in complex multiple inheritance or diamond hierarchies, every class's implementation is executed in the order defined by C3 linearization, and no base class is executed twice.
</details>

<details>
<summary>5. Why must kwargs be passed to super().__init__(**kwargs)?</summary>
In a cooperative multiple inheritance hierarchy, a class rarely knows who its sibling or successor in the MRO will be at runtime. By accepting <code>**kwargs</code>, extracting the parameters it needs, and passing the remaining <code>**kwargs</code> to <code>super().__init__(**kwargs)</code>, arguments flow seamlessly to whatever class comes next in the MRO.
</details>

<details>
<summary>6. What happens if object.__init__ receives unexpected arguments?</summary>
In Python 3, <code>object.__init__</code> raises <code>TypeError: object.__init__() takes exactly one argument (the instance to initialize)</code>. Therefore, the cooperative chain must consume all custom arguments before reaching <code>object</code>.
</details>

<details>
<summary>7. Can super() be used outside of a class method?</summary>
Yes, by passing explicit arguments: <code>super(Class, instance).method()</code>. However, bare zero-argument <code>super()</code> cannot be used at module level because there is no enclosing class or method frame containing <code>__class__</code> and <code>self</code>.
</details>

<details>
<summary>8. How do you skip a specific class in the MRO using super()?</summary>
By calling <code>super(TargetClass, self)</code>, Python begins searching the MRO strictly <b>after</b> <code>TargetClass</code>. This allows intentionally bypassing <code>TargetClass</code> and its predecessors.
</details>

---

## 6. Practical Challenges

<details>
<summary>Challenge 1: Cooperative Serialization Pipeline</summary>

Build a cooperative class hierarchy where `JSONSerializerMixin` and `CompressedSerializerMixin` cooperate via `super().serialize(data)` to produce a final serialized payload.

```python
class BaseSerializer:
    def serialize(self, data: str) -> str:
        return f"<raw>{data}</raw>"

class JSONSerializerMixin(BaseSerializer):
    def serialize(self, data: str) -> str:
        result = super().serialize(data)
        return f'{{"content": "{result}"}}'

class CompressedSerializerMixin(BaseSerializer):
    def serialize(self, data: str) -> str:
        result = super().serialize(data)
        return f"GZIP({result})"

class Pipeline(CompressedSerializerMixin, JSONSerializerMixin):
    pass

p = Pipeline()
assert p.serialize("payload") == 'GZIP({"content": "<raw>payload</raw>"})'
```
</details>

<details>
<summary>Challenge 2: Robust Cooperative Init with Argument Popping</summary>

Implement two mixins `WithID` (requires `entity_id`) and `WithTimestamp` (requires `timestamp`). Ensure they both safely cooperate and initialize without raising `TypeError` at `object.__init__`.

```python
class RootEntity:
    def __init__(self, **kwargs):
        super().__init__()
        assert not kwargs, f"Unhandled kwargs: {kwargs}"

class WithID(RootEntity):
    def __init__(self, entity_id: str, **kwargs):
        super().__init__(**kwargs)
        self.entity_id = entity_id

class WithTimestamp(RootEntity):
    def __init__(self, timestamp: float, **kwargs):
        super().__init__(**kwargs)
        self.timestamp = timestamp

class AuditedRecord(WithID, WithTimestamp):
    pass

r = AuditedRecord(entity_id="E-1", timestamp=1000.0)
assert r.entity_id == "E-1"
assert r.timestamp == 1000.0
```
</details>

<details>
<summary>Challenge 3: MRO Explorer Utility</summary>

Create a utility `explain_super(cls: type, method_name: str)` that prints each class in the MRO that implements `method_name`.

```python
def explain_super(cls: type, method_name: str) -> list[str]:
    chain = []
    for c in cls.__mro__:
        if method_name in c.__dict__:
            chain.append(c.__name__)
    return chain

class A:
    def run(self): pass
class B(A):
    def run(self): pass
class C(B):
    pass

assert explain_super(C, "run") == ["B", "A"]
```
</details>

<details>
<summary>Challenge 4: Cooperative Context Manager</summary>

Implement two mixins `DBTransactionMixin` and `FileLockMixin` that each implement `__enter__` and `__exit__` using `super()` to create a combined transactional context.

```python
class BaseCtx:
    def __enter__(self):
        return self
    def __exit__(self, exc_type, exc_val, exc_tb):
        return False

class DBTransactionMixin(BaseCtx):
    def __enter__(self):
        super().__enter__()
        events.append("db_begin")
        return self
    def __exit__(self, exc_type, exc_val, exc_tb):
        events.append("db_commit")
        return super().__exit__(exc_type, exc_val, exc_tb)

class FileLockMixin(BaseCtx):
    def __enter__(self):
        super().__enter__()
        events.append("lock_acquired")
        return self
    def __exit__(self, exc_type, exc_val, exc_tb):
        events.append("lock_released")
        return super().__exit__(exc_type, exc_val, exc_tb)

class WorkUnit(DBTransactionMixin, FileLockMixin):
    pass

events = []
with WorkUnit():
    events.append("work")

assert events == ["lock_acquired", "db_begin", "work", "db_commit", "lock_released"]
```
</details>

<details>
<summary>Challenge 5: Explicit Skip in MRO</summary>

Given classes `A -> B -> C -> D`, write a call in `D` that explicitly invokes `A.action()` while bypassing `B` and `C`.

```python
class A:
    def action(self): return "A"
class B(A):
    def action(self): return "B"
class C(B):
    def action(self): return "C"
class D(C):
    def bypass_to_a(self):
        # Starts search after B, finding A
        return super(B, self).action()

d = D()
assert d.bypass_to_a() == "A"
```
</details>

<details>
<summary>Challenge 6: Zero-Arg super() Inspection in Dynamic Methods</summary>

Demonstrate why adding a dynamic method to a class using `setattr` fails with zero-argument `super()`, and fix it using explicit parameters.

```python
class Base:
    def greet(self):
        return "Hello"

class DynamicChild(Base):
    pass

# Fails with bare super() because no __class__ cell exists in free function:
def dynamic_greet(self):
    return super(DynamicChild, self).greet() + " from dynamic!"

DynamicChild.greet = dynamic_greet
c = DynamicChild()
assert c.greet() == "Hello from dynamic!"
```
</details>
