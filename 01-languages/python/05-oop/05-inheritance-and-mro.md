# Inheritance and Method Resolution Order (MRO)

- **Topic**: `05-oop`
- **Python version**: 3.12+ (tested on CPython 3.14.0)
- **Tested with**: CPython 3.12, 3.13, 3.14 (standard & free-threaded builds)
- **Free-threading notes**: Compatible. MRO resolution is computed at class creation time and cached in `type.__mro__`, protected by GIL-free type immutability.

---

## 1. Concept Overview

Python supports both **single inheritance** and **multiple inheritance**. In a multiple inheritance hierarchy, a class can inherit from more than one base class, potentially leading to the classic **Diamond Problem** (where two parent classes inherit from the same grandparent).

To resolve method and attribute lookups deterministically without ambiguity, Python uses the **C3 Linearization Algorithm** to construct a strict, linear order called the **Method Resolution Order (MRO)**.

Every class has an `__mro__` attribute (a tuple of classes) or `.mro()` method that dictates the exact order in which classes are searched when resolving attributes and `super()` calls:
```
Subclass -> Parent1 -> Parent2 -> ... -> object
```

If an inheritance hierarchy violates the mathematical constraints of C3 linearization (e.g. creating circular dependencies or contradictory precedence orders), Python refuses to compile the class and raises `TypeError: Cannot create a consistent method resolution order (MRO)`.

```
             object
               ^
               |
               A
             /   \
            B     C
             \   /
               D

MRO for D: [D, B, C, A, object]
```

---

## 2. Key Mechanics & Architecture

### The C3 Linearization Algorithm
The C3 algorithm satisfies three fundamental properties:
1. **Local Precedence Order**: If class $C$ inherits from $(B_1, B_2, \dots, B_n)$, then $B_1$ precedes $B_2$ in the MRO of $C$.
2. **Monotonicity**: If class $X$ precedes class $Y$ in the MRO of any parent class, $X$ must precede $Y$ in the MRO of any derived class.
3. **Extended Precedence Criterion**: Preserves the order of direct base classes as declared.

### Mathematical Formulation
The linearization of a class $C$ with base classes $(B_1, B_2, \dots, B_n)$ is defined recursively:
$$L(C) = [C] + \text{merge}\Big(L(B_1), L(B_2), \dots, L(B_n), [B_1, B_2, \dots, B_n]\Big)$$

The `merge` operation works as follows:
1. Look at the head (first element) of the first list $L(B_1)$.
2. If this head is NOT in the tail (all elements except the first) of any other list being merged:
   - Add it to the output linearization.
   - Remove it from all lists in the merge set.
3. If it IS in the tail of another list, move to the next list's head and repeat.
4. If all remaining heads appear in someone else's tail, a consistent MRO cannot be constructed, raising `TypeError`.
5. Repeat until all lists are exhausted.

---

## 3. Real-World Code Examples

### Example 1: Classic Diamond Problem Resolution
```python
class Base:
    def execute(self):
        print("Base.execute")

class Left(Base):
    def execute(self):
        print("Left.execute")
        super().execute()

class Right(Base):
    def execute(self):
        print("Right.execute")
        super().execute()

class Diamond(Left, Right):
    def execute(self):
        print("Diamond.execute")
        super().execute()


d = Diamond()
d.execute()
print("\nDiamond MRO:")
for idx, cls in enumerate(Diamond.__mro__):
    print(f"  {idx}: {cls.__name__}")
```
Output:
```
Diamond.execute
Left.execute
Right.execute
Base.execute

Diamond MRO:
  0: Diamond
  1: Left
  2: Right
  3: Base
  4: object
```

### Example 2: Inconsistent MRO Detection at Class Creation
```python
class X: pass
class Y(X): pass

try:
    # Defining Z inheriting from X before Y violates monotonicity:
    # Y already inherits from X (so Y must come before X),
    # but (X, Y) demands X come before Y!
    class Inconsistent(X, Y):
        pass
except TypeError as err:
    print(f"MRO compilation failure caught: {err}")
```
Output:
```
MRO compilation failure caught: Cannot create a consistent method resolution
order (MRO) for bases X, Y
```

### Example 3: Inspecting `__mro__` and `.mro()`
```python
class Device: pass
class Networked(Device): pass
class BatteryPowered(Device): pass
class SmartWatch(Networked, BatteryPowered): pass

# __mro__ is a tuple, .mro() returns a list
print(f"SmartWatch MRO tuple: {[c.__name__ for c in SmartWatch.__mro__]}")
print(f"SmartWatch MRO list:  {[c.__name__ for c in SmartWatch.mro()]}")
```
Output:
```
SmartWatch MRO tuple: ['SmartWatch', 'Networked', 'BatteryPowered', 'Device', 'object']
SmartWatch MRO list:  ['SmartWatch', 'Networked', 'BatteryPowered', 'Device', 'object']
```

### Example 4: Manual Step-by-Step C3 Merge Calculation
```python
def manual_c3_demo():
    # Classes:
    # O = object
    # A(O), B(O)
    # C(A, B)
    # L(C) = [C] + merge(L(A), L(B), [A, B])
    # L(A) = [A, O]
    # L(B) = [B, O]
    # merge([A, O], [B, O], [A, B])
    # 1. Head A: not in tails (tails are [O], [O], [B]). Pick A!
    #    Remaining: merge([O], [B, O], [B])
    # 2. Head O: is in tail of [B, O]! Cannot pick O.
    #    Look at next list head: B. B is not in tails ([O], [O]). Pick B!
    #    Remaining: merge([O], [O])
    # 3. Head O: not in tails. Pick O!
    # Result: [C, A, B, O]
    class A: pass
    class B: pass
    class C(A, B): pass
    return [cls.__name__ for cls in C.__mro__]

print(f"Calculated C3 order: {manual_c3_demo()}")
```
Output:
```
Calculated C3 order: ['C', 'A', 'B', 'object']
```

### Example 5: `isinstance` and `issubclass` MRO Traversal
```python
class Component: pass
class UIElement(Component): pass
class Clickable: pass
class Button(UIElement, Clickable): pass

btn = Button()
print(f"isinstance(btn, Clickable): {isinstance(btn, Clickable)}")
print(f"isinstance(btn, Component): {isinstance(btn, Component)}")
print(f"issubclass(Button, object): {issubclass(Button, object)}")
```
Output:
```
isinstance(btn, Clickable): True
isinstance(btn, Component): True
issubclass(Button, object): True
```

---

## 4. Common Anti-Patterns & Gotchas

| Bad Practice | Root Cause | Clean Solution |
| :--- | :--- | :--- |
| Passing child after parent in inheritance list: `class D(Base, Child)` | Violates C3 monotonicity: `Child` already requires `Base` to follow it | Order base classes from most specific to most generic: `class D(Child, Base)` |
| Calling `ParentClass.method(self)` instead of `super().method()` | Skips the cooperative MRO sequence, leading to duplicate or missing parent calls | Always use `super().method()` in multiple inheritance hierarchies |
| Assuming depth-first search order (Python 2.2 legacy) | Modern Python uses C3 linearization, which guarantees breadth-first monotonicity for shared bases | Inspect `Class.__mro__` to verify the actual execution order |
| Mixing incompatible class hierarchies without cooperative design | Classes expecting different signatures in `__init__` break `super().__init__` | Use `*args, **kwargs` across cooperative hierarchies |
| Deep inheritance trees (>4 levels) | Causes fragile base class problem and cognitive overload | Favor composition or lightweight mixins over deep hierarchies |
| Circular inheritance: `class A(B): ... class B(A):` | Directly raises `NameError` or `TypeError` during class evaluation | Decouple dependencies using interfaces, protocols, or composition |

---

## 5. Interview Q&A

<details>
<summary>1. What is the C3 Linearization Algorithm, and why did Python adopt it?</summary>
C3 Linearization is an algorithm introduced in Python 2.3 (PEP 253) to calculate the Method Resolution Order (MRO) for multiple inheritance. It replaced the legacy depth-first search (DFS) algorithm, which could call methods in an unintuitive order and violated monotonicity in diamond hierarchies. C3 guarantees local precedence order, monotonicity, and preservation of declared base order.
</details>

<details>
<summary>2. What is Monotonicity in the context of Python MRO?</summary>
Monotonicity means that if Class A precedes Class B in the MRO of any parent class, Class A MUST precede Class B in the MRO of any derived class that inherits from that parent. It prevents parent method overrides from being unexpectedly shadowed or reversed in subclasses.
</details>

<details>
<summary>3. Why does class Bad(object, MyClass) fail if MyClass is a new-style class?</summary>
All standard classes inherit from <code>object</code>. Since <code>MyClass</code> inherits from <code>object</code>, its MRO is <code>[MyClass, object]</code>. In <code>class Bad(object, MyClass)</code>, the bases list declares <code>object</code> before <code>MyClass</code>. This creates a direct contradiction with <code>MyClass</code>'s own MRO, causing C3 merge to fail with a <code>TypeError: Cannot create a consistent method resolution order (MRO)</code>.
</details>

<details>
<summary>4. What is the difference between Class.__mro__ and Class.mro()?</summary>
<code>Class.__mro__</code> is a read-only tuple storing the linearized MRO calculated when the class was defined. <code>Class.mro()</code> is a method defined on <code>type</code> that can be called (or overridden by custom metaclasses) to compute and return the MRO as a list during class construction.
</details>

<details>
<summary>5. How does Python prevent infinite loops during MRO traversal?</summary>
Because the MRO is a pre-computed, finite linear tuple (cached on the class object), attribute lookup simply iterates sequentially through the tuple until it finds the attribute or reaches <code>object</code>. There is no recursive runtime graph traversal, eliminating infinite loops.
</details>

<details>
<summary>6. In Diamond inheritance (D inherits from B and C; B and C inherit from A), who does super() in B call when invoked on D?</summary>
When invoked on an instance of <code>D</code>, <code>super()</code> in <code>B</code> calls <code>C</code>, NOT <code>A</code>! The MRO of <code>D</code> is <code>[D, B, C, A, object]</code>. Because <code>super()</code> looks up the next class in the runtime instance's MRO, <code>C</code> follows <code>B</code>.
</details>

<details>
<summary>7. What does the term "Cooperative Multiple Inheritance" mean?</summary>
It refers to a design pattern where every method in a multiple-inheritance hierarchy calls <code>super().method(*args, **kwargs)</code>. Because <code>super()</code> delegates to the next class in the runtime MRO, every class in the chain gets executed exactly once in a predictable sequence.
</details>

<details>
<summary>8. Can you manually alter an existing class's __mro__ at runtime?</summary>
No. <code>__mro__</code> is an immutable, read-only attribute on type objects. Attempting to assign to <code>cls.__mro__</code> raises <code>TypeError: readonly attribute</code>. Modifying MRO requires dynamically creating a new class via <code>type()</code>.
</details>

---

## 6. Practical Challenges

<details>
<summary>Challenge 1: Programmatic C3 Linearization Solver</summary>

Write a pure Python function `c3_linearize(cls)` that calculates the C3 MRO of any class hierarchy without using `.__mro__`.

```python
def c3_linearize(cls):
    if not cls.__bases__:
        return [cls]
    def merge(seqs):
        res = []
        while True:
            non_empty = [s for s in seqs if s]
            if not non_empty:
                return res
            for seq in non_empty:
                head = seq[0]
                if not any(head in s[1:] for s in non_empty):
                    res.append(head)
                    for s in non_empty:
                        if s[0] == head:
                            s.pop(0)
                    break
            else:
                raise TypeError("Inconsistent MRO")

    base_lin = [c3_linearize(b) for b in cls.__bases__]
    return [cls] + merge(base_lin + [list(cls.__bases__)])

class A: pass
class B(A): pass
class C(A): pass
class D(B, C): pass

assert c3_linearize(D) == list(D.__mro__)
```
</details>

<details>
<summary>Challenge 2: Inconsistent MRO Detector</summary>

Write a utility function `is_valid_hierarchy(*bases: type) -> bool` that tests whether a set of base classes can form a valid class without raising `TypeError`.

```python
def is_valid_hierarchy(*bases: type) -> bool:
    try:
        type("TestSubclass", bases, {})
        return True
    except TypeError:
        return False

class X: pass
class Y(X): pass

assert is_valid_hierarchy(Y, X) is True
assert is_valid_hierarchy(X, Y) is False
```
</details>

<details>
<summary>Challenge 3: MRO Call Graph Visualizer</summary>

Write a function `format_mro(cls: type) -> str` that returns a clean visual arrow diagram of the MRO path (e.g. `App -> Service -> Base -> object`).

```python
def format_mro(cls: type) -> str:
    return " -> ".join(c.__name__ for c in cls.__mro__)

class Base: pass
class Worker(Base): pass
class TaskWorker(Worker): pass

assert format_mro(TaskWorker) == "TaskWorker -> Worker -> Base -> object"
```
</details>

<details>
<summary>Challenge 4: Attribute Origin Inspector</summary>

Implement `find_attribute_origin(instance: object, attr_name: str) -> type` that returns the exact class in the MRO where the resolved attribute is defined.

```python
def find_attribute_origin(instance: object, attr_name: str):
    cls = type(instance)
    for c in cls.__mro__:
        if attr_name in c.__dict__:
            return c
    raise AttributeError(f"'{attr_name}' not found")

class GrandParent:
    val = 1
class Parent(GrandParent):
    pass
class Child(Parent):
    pass

c = Child()
assert find_attribute_origin(c, "val") is GrandParent
```
</details>

<details>
<summary>Challenge 5: Diamond Logging Pipeline</summary>

Build a diamond inheritance logging pipeline where each class appends its name to an audit list using cooperative `super()`.

```python
class Root:
    def log(self, records: list[str]):
        records.append("Root")

class FilterA(Root):
    def log(self, records: list[str]):
        records.append("FilterA")
        super().log(records)

class FilterB(Root):
    def log(self, records: list[str]):
        records.append("FilterB")
        super().log(records)

class Pipeline(FilterA, FilterB):
    def log(self, records: list[str]):
        records.append("Pipeline")
        super().log(records)

p = Pipeline()
trail = []
p.log(trail)
assert trail == ["Pipeline", "FilterA", "FilterB", "Root"]
```
</details>

<details>
<summary>Challenge 6: Custom Metaclass MRO Customizer</summary>

Create a metaclass `ReversedBasesMeta` that reverses the direct base classes specified by the caller before creating the class.

```python
class ReversedBasesMeta(type):
    def __new__(mcls, name, bases, namespace):
        reversed_bases = tuple(reversed(bases))
        return super().__new__(mcls, name, reversed_bases, namespace)

class A: pass
class B: pass
class Target(A, B, metaclass=ReversedBasesMeta):
    pass

# Direct bases should be (B, A) instead of (A, B)
assert Target.__bases__ == (B, A)
assert Target.__mro__ == (Target, B, A, object)
```
</details>
