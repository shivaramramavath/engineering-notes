---
title: "Classes and Objects in Python"
slug: "classes-and-objects"
category: "oop"
difficulty: "beginner-to-intermediate"
tags: ["python", "oop", "classes", "objects", "type-system", "self", "encapsulation"]
prerequisites: ["03-functions/00-defining-functions", "01-fundamentals/01-variables-names-and-objects"]
related_topics: ["01-init-new-and-instance-lifecycle", "02-instance-vs-class-attributes", "03-methods-classmethod-staticmethod"]
python_version: "3.8+"
tested_with: "CPython 3.14.0"
free_threading_support: "Yes"
---

# Classes and Objects in Python

## 1. Executive Summary & Core Value Proposition

In Python, Object-Oriented Programming (OOP) is not an afterthought or an optional layer; it is the fundamental architectural fabric of the runtime. Everything in Python is an object, and every object is an instance of a class (type):
- Built-in types like `int`, `str`, and `list` are classes.
- Functions, modules, and even classes themselves (`type`) are first-class objects residing on the heap.

A Python class is an executable blueprint defined via the `class` statement. When executed, it creates a `PyTypeObject` containing a class dictionary (`__dict__`). Instantiating that class creates a `PyObject` instance with its own instance namespace, bound methods, and attribute resolution protocol.

Understanding Python's object model—specifically the role of `self`, attribute lookup dynamics, and dynamic type creation—is essential for building maintainable, enterprise-scale software architectures.

---

## 2. Syntax, Mechanics & Visual Architecture

### Formal Class Definition Syntax

```python
class ClassName(BaseClass):
    """Optional class docstring."""
    class_attribute: str = "shared_state"

    def __init__(self, param: str) -> None:
        """Instance initialization."""
        self.instance_attribute = param  # Bound to specific instance

    def instance_method(self) -> str:
        """Method receiving instance as explicit first argument."""
        return f"Method called on {self.instance_attribute}"
```

### The Explicit `self` Parameter Mechanism

Unlike C++, Java, or C# (where `this` is an implicit hidden keyword passed automatically by the compiler), Python strictly enforces **explicit `self`**:
1. When calling `obj.method(arg)`, Python translates this invocation into `ClassName.method(obj, arg)`.
2. `self` is simply an ordinary parameter name; it represents the specific instance reference passed into the method function.
3. While `self` is a universal community convention, the name itself is not a reserved keyword (though violating the `self` convention severely violates PEP 8).

```
Visual Architecture: Bound Method Translation
Call Site:
  account.deposit(500)
       |
       v (Python VM translation via Descriptor Protocol)
  BankAccount.deposit(account, 500)
                        ^        ^
                        |        |
                      'self'   'amount'
```

---

## 3. Real-World Code Examples

### Example 1: Core Class Definition and Instantiation

```python
class ServiceEndpoint:
    """Represents a microservice networking target."""

    def __init__(self, service_name: str, host: str, port: int = 8080):
        self.service_name = service_name
        self.host = host
        self.port = port
        self.is_healthy = True

    def get_url(self) -> str:
        """Constructs canonical HTTP URL."""
        return f"http://{self.host}:{self.port}"

    def mark_degraded(self) -> None:
        self.is_healthy = False

# Instantiating objects
auth_service = ServiceEndpoint("auth", "auth.internal.net", 8443)
billing_service = ServiceEndpoint("billing", "billing.internal.net")

print(f"Service: {auth_service.service_name} -> {auth_service.get_url()}")
print(f"Service: {billing_service.service_name} -> {billing_service.get_url()}")

billing_service.mark_degraded()
print(f"Billing healthy? {billing_service.is_healthy}")
print(f"Auth healthy?    {auth_service.is_healthy}")
```

**Output:**
```
Service: auth -> http://auth.internal.net:8443
Service: billing -> http://billing.internal.net:8080
Billing healthy? False
Auth healthy?    True
```

---

### Example 2: Descriptors and Bound vs Unbound Methods

```python
class Worker:
    def execute(self, task_name: str) -> str:
        return f"Worker executing: {task_name}"

w = Worker()

# Unbound function on class
print("Type on class   :", type(Worker.execute))
# Bound method on instance (self is pre-bound!)
print("Type on instance:", type(w.execute))

# Calling bound method
print(w.execute("Deploy v2"))

# Calling unbound method with explicit instance
print(Worker.execute(w, "Run migrations"))
```

**Output:**
```
Type on class   : <class 'function'>
Type on instance: <class 'method'>
Worker executing: Deploy v2
Worker executing: Run migrations
```

---

### Example 3: Dynamic Attribute Attachment and Introspection

In Python, classes and instance namespaces are mutable dictionaries under the hood (unless restricted by `__slots__`).

```python
class DynamicNode:
    def __init__(self, node_id: str):
        self.node_id = node_id

node = DynamicNode("node-01")

# Dynamically attach attributes at runtime
node.region = "us-east-1"
node.cpu_count = 16

print("Instance __dict__ contents:")
for k, v in node.__dict__.items():
    print(f"  {k}: {v}")

# Built-in attribute inspection
print("\nAttribute queries:")
print("Has 'region' attribute?  :", hasattr(node, "region"))
print("Has 'latency' attribute? :", hasattr(node, "latency"))
print("Get 'region' dynamically :", getattr(node, "region"))
print("Get with fallback default:", getattr(node, "latency", "99ms"))
```

**Output:**
```
Instance __dict__ contents:
  node_id: node-01
  region: us-east-1
  cpu_count: 16

Attribute queries:
Has 'region' attribute?  : True
Has 'latency' attribute? : False
Get 'region' dynamically : us-east-1
Get with fallback default: 99ms
```

---

### Example 4: Class as a Callable Factory (First-Class Type Objects)

Classes are first-class objects: they can be passed as arguments, stored in dictionaries, and invoked dynamically like functions.

```python
class DatabaseDriver:
    def connect(self) -> str: return "Base connection"

class PostgresDriver(DatabaseDriver):
    def connect(self) -> str: return "Connected to PostgreSQL via libpq"

class SQLiteDriver(DatabaseDriver):
    def connect(self) -> str: return "Connected to SQLite in-memory DB"

# Factory registry mapping driver names to Class references (not instances!)
DRIVER_REGISTRY: dict[str, type[DatabaseDriver]] = {
    "postgres": PostgresDriver,
    "sqlite": SQLiteDriver,
}

def create_connection_pool(driver_name: str) -> DatabaseDriver:
    driver_cls = DRIVER_REGISTRY.get(driver_name)
    if not driver_cls:
        raise ValueError(f"Unsupported driver: {driver_name}")
    # Instantiate class dynamically
    return driver_cls()

driver = create_connection_pool("postgres")
print(driver.connect())
```

**Output:**
```
Connected to PostgreSQL via libpq
```

---

### Example 5: Encapsulation and Private Attribute Conventions

Python does not have language-enforced access modifiers (`private`, `protected`, `public`). Instead, it relies on conventions and name mangling:
- `_attribute` (Single underscore): Internal/protected convention ("private by agreement").
- `__attribute` (Double underscore): Triggers **name mangling** to prevent accidental overrides in subclasses.

```python
class BankVault:
    def __init__(self, initial_cash: float):
        self.public_status = "ONLINE"
        self._audit_log = []            # Protected by convention
        self.__secret_key = "K-98124"   # Name-mangled private attribute

    def log_access(self, user: str) -> None:
        self._audit_log.append(user)

vault = BankVault(100_000.0)
print("Public status:", vault.public_status)

# Name mangling transforms __secret_key into _BankVault__secret_key
try:
    print(vault.__secret_key)
except AttributeError as err:
    print("Caught expected access error:", err)

# Inspecting mangled name in __dict__
print("Mangled name in dict:", vault._BankVault__secret_key)
```

**Output:**
```
Public status: ONLINE
Caught expected access error: 'BankVault' object has no attribute '__secret_key'
Mangled name in dict: K-98124
```

---

## 4. Common Anti-Patterns & Mistakes

| Anti-Pattern / Mistake | Broken / Unidiomatic Snippet | Idiomatic Correction | Why It Fails |
| :--- | :--- | :--- | :--- |
| **Omitting `self` in method** | `def get_name(): return "a"` | `def get_name(self): return "a"` | Calling `obj.get_name()` passes instance; raises `TypeError: takes 0 positional arguments but 1 was given`. |
| **Using `__var__` for private data** | `self.__secret__ = 123` | Use `self._secret` or `self.__secret` | `__name__` syntax is strictly reserved for Python language dunders. |
| **Hardcoding class name in methods** | `return BankVault.default_val` | `return self.default_val` or `cls` | Breaks subclassing polymorphism if a derived class overrides the attribute. |
| **Type-checking with `type(x) == C`** | `if type(x) == Animal:` | `if isinstance(x, Animal):` | Direct `type()` equality check breaks inheritance (rejects subclasses like `Dog`). |
| **Excessive `__private` mangling** | Mangling every attribute with `__` | Single underscore `_attr` | Unnecessary name mangling impedes debugging, testing, and legitimate subclass extensions. |
| **Defining methods without `__init__`** | Setting attributes in random methods | Initialize all attributes in `__init__` | Attributes initialized outside `__init__` lead to `AttributeError` if methods are called out of order. |

---

## 5. Architectural & Design Decisions

### "We Are All Consenting Adults Here"
Python's creator Guido van Rossum deliberately omitted hard compiler-enforced `private` keywords. The philosophy is that encapsulation should communicate intent, not enforce artificial security boundaries. A single underscore `_attr` warns developers that the attribute is internal implementation detail. If a developer intentionally accesses `_attr`, they accept the risk that internal APIs may change in future updates.

### The Role of `type` (The Metaclass of All Classes)
In Python's type hierarchy:
- Everything is an object (`isinstance(x, object)` is True for everything).
- Every class is an instance of `type` (`isinstance(ClassName, type)` is True).
- `type` is itself an instance of `type` (`type(type) is type`), forming the recursive base of Python's object model.

---

## 6. Verification & Practical Testing

```python
def test_class_instantiation():
    class Task:
        def __init__(self, title):
            self.title = title
    t = Task("Audit code")
    assert t.title == "Audit code"
    assert isinstance(t, Task)
    assert isinstance(t, object)
    assert isinstance(Task, type)

def test_name_mangling():
    class Secure:
        def __init__(self):
            self.__pin = 1234
    s = Secure()
    assert not hasattr(s, "__pin")
    assert getattr(s, "_Secure__pin") == 1234

def test_explicit_self_translation():
    class Calculator:
        def square(self, x): return x * x
    c = Calculator()
    assert c.square(4) == Calculator.square(c, 4) == 16

if __name__ == "__main__":
    test_class_instantiation()
    test_name_mangling()
    test_explicit_self_translation()
    print("All classes and objects tests passed successfully.")
```

**Output:**
```
All classes and objects tests passed successfully.
```

---

## 7. Deep Dive: The Descriptor Binding Pipeline (`__get__`)

Why does `w.execute` return a `<bound method>` while `Worker.execute` returns a `<function>`?

In Python, all functions are **descriptors** implementing `__get__`:
1. When you access `Worker.execute`, Python calls `FunctionType.__get__(None, Worker)`, returning the raw function.
2. When you access `w.execute`, Python calls `FunctionType.__get__(w, Worker)`, returning a `MethodType` instance that binds `self = w` into the function call stack.

---

## 8. Summary & Key Takeaways

1. **First-Class Types:** Classes are executable heap objects and instances of `type`.
2. **Explicit `self`:** Method invocations `obj.method(x)` translate to `Class.method(obj, x)`.
3. **Encapsulation:** Use `_var` for protected convention; use `__var` only when name mangling is required.
4. **Introspection:** Use `isinstance()`, `hasattr()`, `getattr()`, and `setattr()` for dynamic object queries.
5. **Initialization:** Always declare and initialize all instance attributes inside `__init__`.

---

## 9. Frequently Asked Questions (Interview & Senior Roles)

<details>
<summary>1. Why is `self` explicitly required in Python method definitions?</summary>

Explicit `self` simplifies the language semantics by maintaining an unambiguous distinction between local variables and instance attributes. It allows methods to be treated as ordinary functions that happen to receive an instance as their first argument, facilitating dynamic method binding, monkey-patching, and functional composition.
</details>

<details>
<summary>2. What is the difference between `isinstance(obj, Cls)` and `type(obj) is Cls`?</summary>

- `type(obj) is Cls` strictly checks if `obj` was instantiated from `Cls`, ignoring inheritance hierarchies.
- `isinstance(obj, Cls)` checks if `obj` is an instance of `Cls` **or any subclass derived from `Cls`**. In production OOP, `isinstance()` is preferred to preserve polymorphism.
</details>

<details>
<summary>3. What is name mangling and why does Python do it?</summary>

When an attribute name begins with two or more leading underscores (and at most one trailing underscore, e.g. `__secret`), Python's parser textually replaces it with `_ClassName__secret`. This is not for security or hard privacy; it exists to prevent accidental attribute collisions in complex inheritance hierarchies.
</details>

<details>
<summary>4. Can you dynamically add methods to an existing instance at runtime?</summary>

Yes, using `types.MethodType(func, instance)`. Assigning a raw function directly to an instance (`instance.fn = func`) attaches it as a plain attribute, not a bound method; wrapping it with `MethodType` binds `self` to the instance.
</details>

<details>
<summary>5. What is the difference between an attribute on the class vs an attribute on the instance?</summary>

A class attribute is shared across all instances of that class and stored in `Class.__dict__`. An instance attribute is unique to that specific instance and stored in `instance.__dict__`.
</details>

<details>
<summary>6. What happens if an attribute is accessed via `obj.attr`?</summary>

Python traverses the attribute lookup pipeline:
1. Data descriptors in class MRO (e.g. `@property`).
2. Instance `__dict__`.
3. Non-data descriptors in class MRO (e.g. methods).
4. Class `__dict__` and superclasses.
5. `__getattr__()` hook if defined; otherwise raises `AttributeError`.
</details>

<details>
<summary>7. What is `__class__` on an object instance?</summary>

`obj.__class__` is a reference pointing to the class object from which the instance was created. In Python, reassigning `obj.__class__ = NewClass` dynamically mutates the object's type at runtime (though this is an advanced pattern that should be used with extreme care).
</details>

<details>
<summary>8. How do classes and objects behave in Free-threaded Python (PEP 703)?</summary>

In free-threaded Python (no-GIL), reading attributes on shared objects is thread-safe. However, modifying instance `__dict__` or class attributes concurrently from multiple threads requires explicit mutex locking to avoid race conditions.
</details>

---

## 10. Practical Exercises & Self-Assessment

<details>
<summary>Exercise 1: Safe Account Balance Tracker</summary>

**Task:** Create a `BankAccount` class with protected balance `_balance`, providing `deposit()`, `withdraw()`, and a read-only getter `get_balance()`.

**Solution:**
```python
class BankAccount:
    def __init__(self, initial_deposit: float = 0.0):
        if initial_deposit < 0: raise ValueError("Initial deposit cannot be negative")
        self._balance = initial_deposit

    def deposit(self, amount: float) -> None:
        if amount <= 0: raise ValueError("Amount must be positive")
        self._balance += amount

    def withdraw(self, amount: float) -> None:
        if amount > self._balance: raise ValueError("Insufficient funds")
        self._balance -= amount

    def get_balance(self) -> float:
        return self._balance

acc = BankAccount(100.0)
acc.deposit(50.0); acc.withdraw(30.0)
print("Balance:", acc.get_balance())  # 120.0
```
</details>

<details>
<summary>Exercise 2: Dynamic Method Binding</summary>

**Task:** Dynamically bind an external function to a specific instance so that it receives `self` automatically.

**Solution:**
```python
import types

class Robot:
    def __init__(self, name): self.name = name

def speak(self):
    return f"Robot {self.name} is online."

bot = Robot("R2D2")
# Bind function to bot instance
bot.speak = types.MethodType(speak, bot)
print(bot.speak())  # Robot R2D2 is online.
```
</details>

<details>
<summary>Exercise 3: Introspect Class MRO and Attributes</summary>

**Task:** Write a function `dump_object_info(obj)` that prints the object's class name, base classes, and all user-defined instance attributes.

**Solution:**
```python
def dump_object_info(obj):
    cls = obj.__class__
    print(f"Class Name   : {cls.__name__}")
    print(f"Base Classes : {[b.__name__ for b in cls.__bases__]}")
    print(f"Instance Data: {obj.__dict__}")

class Worker:
    def __init__(self): self.job = "Engineer"

dump_object_info(Worker())
```
</details>

<details>
<summary>Exercise 4: Verify Name Mangling on Subclasses</summary>

**Task:** Demonstrate that a subclass defining `__id` does not overwrite the superclass's `__id`.

**Solution:**
```python
class Parent:
    def __init__(self): self.__id = "PARENT"
    def get_parent_id(self): return self.__id

class Child(Parent):
    def __init__(self):
        super().__init__()
        self.__id = "CHILD"
    def get_child_id(self): return self.__id

c = Child()
assert c.get_parent_id() == "PARENT"
assert c.get_child_id() == "CHILD"
print("Parent and child mangled attributes co-exist safely!")
```
</details>

<details>
<summary>Exercise 5: Dynamic Attribute Validator</summary>

**Task:** Write an object where setting an attribute via `setattr` validates that the value is non-empty if it is a string.

**Solution:**
```python
class ValidatedRecord:
    def __setattr__(self, name, value):
        if isinstance(value, str) and not value.strip():
            raise ValueError(f"Attribute '{name}' cannot be empty string")
        super().__setattr__(name, value)

rec = ValidatedRecord()
rec.title = "Valid Title"
try:
    rec.title = "   "
except ValueError as e:
    print("Caught validation error:", e)
```
</details>

<details>
<summary>Exercise 6: Dynamic Class Instantiation via Factory Dictionary</summary>

**Task:** Implement a shapes registry that instantiates `Circle(radius)` or `Rectangle(w, h)` based on a string name and arguments.

**Solution:**
```python
class Circle:
    def __init__(self, radius): self.radius = radius
class Rectangle:
    def __init__(self, w, h): self.w, self.h = w, h

SHAPES = {"circle": Circle, "rect": Rectangle}

def shape_factory(kind: str, *args):
    return SHAPES[kind](*args)

c = shape_factory("circle", 10)
r = shape_factory("rect", 4, 5)
assert isinstance(c, Circle) and isinstance(r, Rectangle)
print("Factory shapes instantiated successfully!")
```
</details>
