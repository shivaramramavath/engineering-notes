---
title: "Variance (Covariance, Contravariance, Invariance) and Type Narrowing"
description: "Deep dive into type system variance: mathematical subtyping rules, why mutable collections are invariant, covariant read-only sequences, contravariant consumers, PEP 695 automatic variance inference, and control flow type narrowing."
module: "11-type-hints-and-static-typing"
order: 11
tags:
  - python
  - typing
  - variance
  - covariance
  - contravariance
  - invariance
  - subtyping
  - type-narrowing
difficulty: "Advanced"
cpython_version: "3.12+"
---

# Variance (Covariance, Contravariance, Invariance) and Type Narrowing

Variance is one of the most intellectually demanding yet architecturally essential topics in computer science type systems. If class `Dog` is a subtype of `Animal` ($Dog \le Animal$), does that mean a list of dogs (`list[Dog]`) is a subtype of a list of animals (`list[Animal]`)?

In Python, the answer is an emphatic **NO**. If `list[Dog]` were considered a subtype of `list[Animal]`, passing a `list[Dog]` to a function expecting `list[Animal]` would allow that function to append a `Cat` into the dog list, fatally corrupting type safety at runtime!

This note demystifies **Invariance**, **Covariance**, and **Contravariance**, reveals how Python 3.12 (PEP 695) revolutionized variance through **automatic static inference**, and explores advanced control-flow type narrowing.

---

## Table of Contents

1. [What & Why](#what--why)
2. [Core Concepts & Mathematical Subtyping Model](#core-concepts--mathematical-subtyping-model)
3. [The Three Types of Variance](#the-three-types-of-variance)
4. [How It Works Internally: PEP 695 Automatic Inference](#how-it-works-internally-pep-695-automatic-inference)
5. [Syntax & API Reference](#syntax--api-reference)
6. [Code Examples](#code-examples)
7. [Edge Cases, Pitfalls & Anti-Patterns](#edge-cases-pitfalls--anti-patterns)
8. [Performance & Memory Characteristics](#performance--memory-characteristics)
9. [CPython Internals & Bytecode](#cpython-internals--bytecode)
10. [Real-World Case Study: Heterogeneous Event Processing Pipeline with Covariant Producers and Contravariant Consumers](#real-world-case-study-heterogeneous-event-processing-pipeline-with-covariant-producers-and-contravariant-consumers)
11. [Interview Questions & Deep-Dive Scenarios](#interview-questions--deep-dive-scenarios)
12. [Practical Exercises & Code Katas](#practical-exercises--code-katas)

---

## What & Why

Variance describes how the subtyping relationship between complex generic types (e.g., `Container[A]` and `Container[B]`) depends on the subtyping relationship between their component type arguments ($A$ and $B$).

Without strict variance enforcement:
1. **Container Corruption**: Mutable collections could be polluted with incompatible subtypes.
2. **Callable Signature Violations**: Callbacks expecting specific subclasses could receive broader superclasses that lack required methods, causing runtime `AttributeError` crashes.

Understanding variance is the difference between fighting static type checkers with `# type: ignore` and designing elegant, flexible, type-sound APIs.

---

## Core Concepts & Mathematical Subtyping Model

Let $A \le B$ denote that type $A$ is a subtype of type $B$ (i.e. every instance of $A$ can be safely substituted where $B$ is expected).

```mermaid
graph TD
    subgraph Covariance (Preserves Subtype Order)
        C1["Dog <= Animal"] -->|F is Covariant| C2["F[Dog] <= F[Animal]"]
        C2 --> C3["Read-Only / Producers (Sequence, tuple, Iterator)"]
    end

    subgraph Contravariance (Inverts Subtype Order)
        K1["Dog <= Animal"] -->|F is Contravariant| K2["F[Animal] <= F[Dog]"]
        K2 --> K3["Write-Only / Consumers (Callable[[T], None], Sinks)"]
    end

    subgraph Invariance (No Subtyping Allowed)
        I1["Dog <= Animal"] -->|F is Invariant| I2["F[Dog] != F[Animal]"]
        I2 --> I3["Read-Write Mutable (list[T], dict[K, V], set[T])"]
    end
```

---

## The Three Types of Variance

### 1. Invariance ($F[A] \not\le F[B]$ and $F[B] \not\le F[A]$)
A generic type $F$ is invariant when no subtyping relationship is preserved between $F[A]$ and $F[B]$, regardless of how $A$ and $B$ relate.
- **Why?** If a container allows both **reading** (producing $T$) and **writing** (consuming $T$), it must be invariant.
- **Example**: `list[T]`.
  ```python
  class Animal: pass
  class Dog(Animal): pass
  class Cat(Animal): pass

  def add_cat(cage: list[Animal]) -> None:
      cage.append(Cat())  # Perfectly legal for list[Animal]

  dogs: list[Dog] = [Dog()]
  # If list was covariant, the next line would typecheck:
  # add_cat(dogs)
  # But now dogs has a Cat! dogs[0].bark() crashes with AttributeError!
  # Therefore: list[Dog] CANNOT be list[Animal]!
  ```

### 2. Covariance ($A \le B \implies F[A] \le F[B]$)
A generic type $F$ is covariant when it preserves the subtyping direction of its type arguments.
- **Why?** When a container is **read-only** (it only outputs/produces $T$), it is completely safe to treat a collection of `Dog` objects as a collection of `Animal` objects.
- **Examples**: `Sequence[T]`, `tuple[T, ...]`, `Iterable[T]`, `Iterator[T]`.

### 3. Contravariance ($A \le B \implies F[B] \le F[A]$)
A generic type $F$ is contravariant when it inverts the subtyping direction of its type arguments.
- **Why?** When a generic type is **write-only** (it only accepts/consumes $T$ as an input parameter, such as a callback or serializer), a consumer capable of handling any broad `Animal` can safely be used wherever a consumer of specialized `Dog` instances is needed!
- **Example**: `Callable[[T], None]`.

---

## How It Works Internally: PEP 695 Automatic Inference

Prior to Python 3.12, variance had to be manually declared when creating a `TypeVar`:
```python
# Legacy PEP 484:
T_co = TypeVar("T_co", covariant=True)
T_contra = TypeVar("T_contra", contravariant=True)
T_inv = TypeVar("T_inv")  # default: invariant
```

If a developer mistakenly marked a mutable class as `covariant=True`, Mypy would raise obscure errors like `Cannot use a covariant type variable as a parameter`.

### Python 3.12 PEP 695 Automatic Inference
In Python 3.12+, the type checker **automatically calculates the variance** of each type parameter based on where it appears in class definitions:
```python
# Modern PEP 695:
class ReadOnlyBox[T]:
    # T only appears in return position -> inferred as COVARIANT!
    def __init__(self, value: T) -> None:
        self._val = value
    def get(self) -> T:
        return self._val

class Sink[T]:
    # T only appears in parameter position -> inferred as CONTRAVARIANT!
    def consume(self, item: T) -> None:
        pass

class MutableBox[T]:
    # T appears in both parameter and return positions -> inferred as INVARIANT!
    def get(self) -> T: ...
    def set(self, val: T) -> None: ...
```

---

## Syntax & API Reference

### 1. Legacy Manual Variance (Python 3.11 and earlier)

```python
from typing import TypeVar, Generic

T_co = TypeVar("T_co", covariant=True)
T_contra = TypeVar("T_contra", contravariant=True)

class Producer(Generic[T_co]):
    def produce(self) -> T_co: ...

class Consumer(Generic[T_contra]):
    def consume(self, item: T_contra) -> None: ...
```

### 2. Modern Automatic Variance (Python 3.12+)

```python
# Variance is inferred automatically by Mypy / Pyright
class Producer[T]:
    def produce(self) -> T: ...

class Consumer[T]:
    def consume(self, item: T) -> None: ...
```

---

## Code Examples

### Example 1: Covariant Read-Only Stream

```python
from collections.abc import Sequence

class Animal:
    def speak(self) -> str:
        return "Generic Animal Sound"

class Dog(Animal):
    def speak(self) -> str:
        return "Woof!"

class Cat(Animal):
    def speak(self) -> str:
        return "Meow!"

# Sequence is covariant: Sequence[Dog] IS a subtype of Sequence[Animal]!
def listen_to_animals(animals: Sequence[Animal]) -> None:
    for a in animals:
        print(f"Animal says: {a.speak()}")

dogs: list[Dog] = [Dog(), Dog()]

# Type checker accepts this safely because Sequence is read-only:
listen_to_animals(dogs)
# Output:
# Animal says: Woof!
# Animal says: Woof!
```

### Example 2: Contravariant Consumer Callback

```python
from collections.abc import Callable

class Message:
    def __init__(self, content: str) -> None:
        self.content = content

class EncryptedMessage(Message):
    def __init__(self, content: str, key_id: str) -> None:
        super().__init__(content)
        self.key_id = key_id

# Handler capable of processing ANY generic Message:
def log_general_message(msg: Message) -> None:
    print(f"[LOG] Message content: {msg.content}")

# Consumer expecting specifically an EncryptedMessage handler:
def process_encrypted_pipeline(
    handler: Callable[[EncryptedMessage], None],
    msg: EncryptedMessage
) -> None:
    handler(msg)

# Contravariance in action:
# Callable[[Message], None] is a SUBTYPE of Callable[[EncryptedMessage], None]!
enc_msg = EncryptedMessage("Secret payload", key_id="key-991")
process_encrypted_pipeline(log_general_message, enc_msg)
# Output:
# [LOG] Message content: Secret payload
```

### Example 3: Mutable Invariance Trap and Safe Workaround

```python
from collections.abc import Sequence

def print_first_element(items: Sequence[object]) -> None:
    if items:
        print("First:", items[0])

# list[int] cannot be passed to a function expecting list[object],
# BUT it CAN be passed to a function expecting Sequence[object]!
my_numbers: list[int] = [10, 20, 30]

print_first_element(my_numbers)
# Output:
# First: 10
```

### Example 4: Advanced Type Narrowing with Control Flow Analysis

```python
from typing import Literal

def process_identifier(identifier: int | str | None) -> str:
    # 1. Narrowing via None identity check:
    if identifier is None:
        return "ANONYMOUS"
    
    # At this point, type checker narrows to: int | str
    
    # 2. Narrowing via isinstance:
    if isinstance(identifier, int):
        # Narrows to int:
        return f"ID_INT_{identifier:06d}"
    
    # 3. Falls through to str:
    # Narrows to str:
    return f"ID_STR_{identifier.upper()}"

print(process_identifier(None))
print(process_identifier(42))
print(process_identifier("usr_dev"))
# Output:
# ANONYMOUS
# ID_INT_000042
# ID_STR_USR_DEV
```

### Example 5: Type Narrowing with Set and Dictionary Invariants

```python
from typing import TypedDict

class AdminUser(TypedDict):
    role: Literal["admin"]
    permissions: list[str]

class GuestUser(TypedDict):
    role: Literal["guest"]
    ip_address: str

type AppUser = AdminUser | GuestUser

def inspect_permissions(user: AppUser) -> int:
    # Narrowing via key existence or literal check:
    if user["role"] == "admin":
        # Statically narrowed to AdminUser
        return len(user["permissions"])
    else:
        # Statically narrowed to GuestUser
        return 0

admin: AdminUser = {"role": "admin", "permissions": ["deploy", "rollback"]}
guest: GuestUser = {"role": "guest", "ip_address": "127.0.0.1"}

print("Admin perms:", inspect_permissions(admin))
print("Guest perms:", inspect_permissions(guest))
# Output:
# Admin perms: 2
# Guest perms: 0
```

---

## Edge Cases, Pitfalls & Anti-Patterns

### Common Mistakes Table

| Mistake | Bad Code | Correct Idiom | Why It Fails |
|:---|:---|:---|:---|
| **Expecting `list[Derived]` to match `list[Base]`** | `def f(x: list[Animal]): ...; f(dogs)` | `def f(x: Sequence[Animal]): ...` | `list` is invariant; passing a subtype list violates type soundness. |
| **Declaring `covariant=True` on mutable class** | `class Box(Generic[T_co]): def set(self, x: T_co):` | Use invariant `TypeVar` or PEP 695 | A method parameter is a contravariant position; cannot use covariant `TypeVar`. |
| **Using `isinstance` on generic type parameters** | `isinstance(x, T)` | `isinstance(x, concrete_class)` | Generic type variables are erased at runtime. |
| **Contravariant return types** | `class Consumer(Generic[T_contra]): def get(self) -> T_contra:` | Return positions must be covariant | Return value is a producer; cannot be contravariant. |
| **Ignoring negative branch narrowing** | Assuming `else:` retains original union after `TypeGuard` | Use `TypeIs` (PEP 742) for bidirectional narrowing | `TypeGuard` does not narrow the `else` branch. |
| **Overriding methods with narrower parameter types** | Subclass overrides `def f(self, x: Dog)` where base has `(self, x: Animal)` | Subclasses must accept equal or broader types (Liskov Substitution Principle) | Violates LSP; callers passing `Animal` crash when invoking subclass. |

---

## Performance & Memory Characteristics

- **Zero Runtime Overhead**: Variance exists entirely in the static analyzer’s mathematical type checker. At runtime, CPython has no concept of covariance or contravariance.
- **Architectural Flexibility**: Using covariant interfaces (`Sequence`, `Mapping`) allows clients to pass specialized domain models directly without having to copy or convert collections into new lists.

---

## CPython Internals & Bytecode

Let us inspect the internal attributes of `TypeVar` instances in CPython:

```python
from typing import TypeVar

T_inv = TypeVar("T_inv")
T_co = TypeVar("T_co", covariant=True)
T_contra = TypeVar("T_contra", contravariant=True)

print("T_inv attributes:", (T_inv.__covariant__, T_inv.__contravariant__))
print("T_co attributes:", (T_co.__covariant__, T_co.__contravariant__))
print("T_contra attributes:", (T_contra.__covariant__, T_contra.__contravariant__))
```

Output:
```text
T_inv attributes: (False, False)
T_co attributes: (True, False)
T_contra attributes: (False, True)
```

In Python 3.12, PEP 695 type parameter tokens store variance information in the `__infer_variance__` boolean attribute, allowing type checkers to calculate variance lazily.

---

## Best Practices & Idiomatic Patterns

1. **Use `Sequence[T]` Instead of `list[T]` in Function Signatures**: Automatically grants covariance for input collections.
2. **Use `Mapping[K, V]` Instead of `dict[K, V]` for Inputs**: Covariant in both key and value types.
3. **Remember the Position Rule**:
   - Method **Return Type** $\rightarrow$ **Covariant Position** ($+$).
   - Method **Argument Type** $\rightarrow$ **Contravariant Position** ($-$).
4. **Rely on PEP 695 on Python 3.12+**: Avoid declaring manual `covariant=True` or `contravariant=True` flags; let Mypy and Pyright infer variance automatically.

---

## Real-World Case Study: Heterogeneous Event Processing Pipeline with Covariant Producers and Contravariant Consumers

Below is an enterprise telemetry streaming system leveraging covariance for data feeds and contravariance for metric consumers:

```python
from collections.abc import Sequence, Callable
from typing import Protocol
from dataclasses import dataclass

# Subtyping Domain Hierarchy
class BaseEvent:
    def __init__(self, timestamp: float) -> None:
        self.timestamp = timestamp

class SystemMetric(BaseEvent):
    def __init__(self, timestamp: float, host: str, cpu_load: float) -> None:
        super().__init__(timestamp)
        self.host = host
        self.cpu_load = cpu_load

class SecurityAlert(BaseEvent):
    def __init__(self, timestamp: float, severity: str, ip: str) -> None:
        super().__init__(timestamp)
        self.severity = severity
        self.ip = ip

# 1. Covariant Producer Protocol (Only outputs T)
class EventSource[T: BaseEvent](Protocol):
    def fetch_batch(self) -> Sequence[T]:
        ...

# 2. Contravariant Consumer Protocol (Only inputs T)
class EventSink[T: BaseEvent](Protocol):
    def ingest(self, event: T) -> None:
        ...

# Concrete Metric Producer
class ServerMetricFeed:
    def fetch_batch(self) -> list[SystemMetric]:
        return [
            SystemMetric(1710000001.0, "node-01.prod", 42.5),
            SystemMetric(1710000002.0, "node-02.prod", 88.1),
        ]

# Generic Base Event Sink (Can ingest ANY BaseEvent)
class CloudWatchArchiver:
    def ingest(self, event: BaseEvent) -> None:
        print(f"[ARCHIVER] Archived event at timestamp {event.timestamp}")

# Streaming Engine marrying Covariant Sources and Contravariant Sinks
class StreamEngine:
    def forward_events[E: BaseEvent](
        self,
        source: EventSource[E],
        sink: EventSink[E]
    ) -> int:
        batch = source.fetch_batch()
        for evt in batch:
            sink.ingest(evt)
        return len(batch)

# Execution:
engine = StreamEngine()
feed = ServerMetricFeed()            # Produces SystemMetric
archiver = CloudWatchArchiver()      # Consumes BaseEvent (Contravariantly compatible with SystemMetric!)

count = engine.forward_events(feed, archiver)
print(f"Successfully processed {count} events.")
# Output:
# [ARCHIVER] Archived event at timestamp 1710000001.0
# [ARCHIVER] Archived event at timestamp 1710000002.0
# Successfully processed 2 events.
```

---

## Interview Questions & Deep-Dive Scenarios

<details>
<summary>1. Why must mutable collections like `list[T]` be invariant?</summary>

If `list[Dog]` were allowed to be a subtype of `list[Animal]` (covariance), a function accepting `list[Animal]` could legitimately append a `Cat` into that list. When the caller holding the original `list[Dog]` reference retrieved that element and attempted to call dog-specific methods like `.bark()`, the program would crash with an `AttributeError`. Invariance prevents this soundly.
</details>

<details>
<summary>2. Why are function arguments contravariant while return types are covariant?</summary>

- **Arguments (Contravariant)**: If a caller expects a function that can accept a `Dog`, a function capable of accepting any `Animal` is strictly more capable and safe to use. The requirement inverts the subtype direction.
- **Return Values (Covariant)**: If a caller expects a function that returns an `Animal`, a function that returns a specialized `Dog` satisfies all expectations of an `Animal`.
</details>

<details>
<summary>3. What is the variance of `dict[K, V]`?</summary>

In Python, `dict[K, V]` is **invariant** in both `K` and `V` because it is mutable (supports both `__getitem__` read operations and `__setitem__` write operations). However, the read-only abstract base class `collections.abc.Mapping[K, V]` is **covariant** in both `K` and `V`.
</details>

<details>
<summary>4. How does Python 3.12 PEP 695 eliminate the need for `covariant=True`?</summary>

In PEP 695, type parameters declared in `class C[T]:` are analyzed by the compiler and static type checkers across all method signatures. The checker checks whether `T` appears solely in covariant positions (returns), contravariant positions (arguments), or both. Variance is inferred automatically, removing human error.
</details>

<details>
<summary>5. What is the Liskov Substitution Principle (LSP) regarding method overriding?</summary>

When a subclass overrides a method from a parent class:
- The parameter types must be the **same or broader** (contravariant parameter rule).
- The return type must be the **same or narrower** (covariant return rule).
Violating this principle breaks static typing and runtime polymorphism.
</details>

<details>
<summary>6. Why can a method parameter in a covariant generic class accept `T` if it uses a bounded TypeVar?</summary>

In a covariant class `Producer[T_co]`, defining `def add(self, x: T_co)` is illegal because parameters are contravariant. However, you can write:
```python
def add[U](self, item: U) -> "Producer[T_co | U]": ...
```
This safely generalizes the container's return type without violating variance.
</details>

<details>
<summary>7. How does control flow analysis (CFA) narrow types after an exception raise?</summary>

If a conditional branch raises an exception (`raise ValueError(...)`) or calls a function returning `NoReturn`, the static analyzer prunes that branch from the reachable flow. All variables in the subsequent code are narrowed by subtracting the condition that triggered the raise.
</details>

<details>
<summary>8. Is `tuple` covariant in Python?</summary>

Yes! `tuple[T, ...]` is an immutable sequence. Because its elements cannot be modified or replaced after construction, it is fully covariant: `tuple[Dog, ...]` is a valid subtype of `tuple[Animal, ...]`.
</details>

---

## Practical Exercises & Code Katas

### Exercise 1: Invariance Violation Demonstration
Write a function that demonstrates why `list[int]` cannot be passed to a function expecting `list[object]`.

<details>
<summary>Solution</summary>

```python
def corrupt_list(items: list[object]) -> None:
    items.append("unexpected string")

numbers: list[int] = [1, 2, 3]

# In static analysis, this line is flagged as [arg-type]:
# corrupt_list(numbers)

# If forced with cast/ignore, runtime consequences:
corrupt_list(numbers)  # type: ignore[arg-type]
print("Corrupted list:", numbers)
# Output: Corrupted list: [1, 2, 3, 'unexpected string']
```
</details>

### Exercise 2: Covariant Immutable Box
Implement an immutable container `Box[T]` whose variance is inferred as covariant in Python 3.12+.

<details>
<summary>Solution</summary>

```python
class Box[T]:
    def __init__(self, item: T) -> None:
        self._item: T = item

    def get(self) -> T:
        return self._item

# Because T only appears in get() return position,
# Box[Dog] is safely accepted as Box[Animal]
class Animal: pass
class Dog(Animal): pass

dog_box: Box[Dog] = Box(Dog())
animal_box: Box[Animal] = dog_box  # Valid in PEP 695!
print("Box contains:", type(animal_box.get()).__name__)
# Output: Box contains: Dog
```
</details>

### Exercise 3: Contravariant Comparator Function
Write a comparator function `compare_animals(a: Animal, b: Animal) -> int` and demonstrate its use sorting a list of `Dog` objects.

<details>
<summary>Solution</summary>

```python
from collections.abc import Callable

class Animal:
    def __init__(self, weight: float) -> None:
        self.weight = weight

class Dog(Animal):
    def __init__(self, weight: float, breed: str) -> None:
        super().__init__(weight)
        self.breed = breed

def animal_weight_key(animal: Animal) -> float:
    return animal.weight

def sort_dogs(dogs: list[Dog], key_fn: Callable[[Dog], float]) -> None:
    dogs.sort(key=key_fn)

pack = [Dog(25.0, "Labrador"), Dog(12.0, "Beagle"), Dog(40.0, "Mastiff")]
# animal_weight_key accepts Animal, passed to function expecting (Dog) -> float!
sort_dogs(pack, animal_weight_key)
print("Sorted weights:", [d.weight for d in pack])
# Output: Sorted weights: [12.0, 25.0, 40.0]
```
</details>

### Exercise 4: Type Narrowing with Discriminated Unions
Implement a shape area calculator that narrows a discriminated union between `Circle` and `Rectangle`.

<details>
<summary>Solution</summary>

```python
import math
from typing import Literal
from dataclasses import dataclass

@dataclass
class Circle:
    kind: Literal["circle"]
    radius: float

@dataclass
class Rectangle:
    kind: Literal["rectangle"]
    width: float
    height: float

type Shape = Circle | Rectangle

def calculate_area(shape: Shape) -> float:
    if shape.kind == "circle":
        return math.pi * (shape.radius ** 2)
    else:
        return shape.width * shape.height

print("Circle area:", calculate_area(Circle("circle", 5.0)))
print("Rectangle area:", calculate_area(Rectangle("rectangle", 4.0, 6.0)))
# Output:
# Circle area: 78.53981633974483
# Rectangle area: 24.0
```
</details>

### Exercise 5: Covariant Dictionary Wrapper
Wrap a standard Python dictionary in a read-only class that provides covariant access to values.

<details>
<summary>Solution</summary>

```python
class ReadOnlyDict[K, V]:
    def __init__(self, mapping: dict[K, V]) -> None:
        self._data = dict(mapping)

    def __getitem__(self, key: K) -> V:
        return self._data[key]

    def get(self, key: K, default: V | None = None) -> V | None:
        return self._data.get(key, default)

    def __contains__(self, key: K) -> bool:
        return key in self._data

d: ReadOnlyDict[str, int] = ReadOnlyDict({"port": 8080})
print("Port:", d["port"])
# Output: Port: 8080
```
</details>

### Exercise 6: Flow-Sensitive Type Narrowing with Assertion Guards
Write a function that accepts `str | int | None` and uses assertions to guarantee `int` before performing arithmetic.

<details>
<summary>Solution</summary>

```python
def multiply_by_two(val: str | int | None) -> int:
    if isinstance(val, str):
        val = int(val)
    assert val is not None, "Value cannot be None"
    # Static analyzer narrows val to int:
    return val * 2

print(multiply_by_two("21"))
print(multiply_by_two(50))
# Output:
# 42
# 100
```
</details>
