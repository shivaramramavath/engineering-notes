# Operator Overloading and NotImplemented

- **Topic**: `05-oop`
- **Python version**: 3.12+ (tested on CPython 3.14.0)
- **Tested with**: CPython 3.12, 3.13, 3.14 (standard & free-threaded builds)
- **Free-threading notes**: Compatible. Operator dispatch and `NotImplemented` return evaluation are thread-safe and stateless.

---

## 1. Concept Overview

Operator overloading allows user-defined objects to respond to standard Python operators (`+`, `-`, `*`, `==`, `<`, `in`, `@`, etc.) using the special method protocol.

Python divides binary operators into three complementary families:
1. **Normal (Left) Operators**: `__add__`, `__sub__`, `__mul__`, `__eq__`, etc. Called when your object is the left-hand operand (`a + b` invokes `a.__add__(b)`).
2. **Reflected (Right / Swapped) Operators**: `__radd__`, `__rsub__`, `__rmul__`, etc. Called when your object is the right-hand operand and the left-hand operand either does not implement the operation or returns `NotImplemented` (`a + b` invokes `b.__radd__(a)`).
3. **In-Place (Augmented) Operators**: `__iadd__`, `__isub__`, `__imul__`, etc. Called during augmented assignment (`a += b`). Can mutate mutable objects in-place or return a new object.

The singleton sentinel value **`NotImplemented`** (distinct from the `NotImplementedError` exception) plays a crucial role: returning it signals to Python that the current object does not know how to handle the operand, prompting Python to try the reflected operation on the other operand before raising a `TypeError`.

```
                    Expression: a + b
                           |
                           v
              Does type(a) implement __add__?
                     /                \
                   YES                 NO
                   /                    \
     result = a.__add__(b)               |
               /        \                |
    != NotImplemented  == NotImplemented |
             |                  |        |
             v                  +--------+
        Return result                    |
                                         v
                         Does type(b) implement __radd__?
                                /                \
                              YES                 NO
                              /                    \
                result = b.__radd__(a)              |
                          /        \                |
               != NotImplemented  == NotImplemented |
                        |               |           |
                        v               +-----+-----+
                   Return result              |
                                              v
                                      Raise TypeError
```

---

## 2. Key Mechanics & Architecture

### Complete Operator Dunder Matrix

| Operator | Left Method | Reflected (Right) Method | In-Place Method |
| :--- | :--- | :--- | :--- |
| `+` | `__add__(self, other)` | `__radd__(self, other)` | `__iadd__(self, other)` |
| `-` | `__sub__(self, other)` | `__rsub__(self, other)` | `__isub__(self, other)` |
| `*` | `__mul__(self, other)` | `__rmul__(self, other)` | `__imul__(self, other)` |
| `/` | `__truediv__(self, other)` | `__rtruediv__(self, other)` | `__itruediv__(self, other)` |
| `//` | `__floordiv__(self, other)` | `__rfloordiv__(self, other)` | `__ifloordiv__(self, other)` |
| `%` | `__mod__(self, other)` | `__rmod__(self, other)` | `__imod__(self, other)` |
| `**` | `__pow__(self, other)` | `__rpow__(self, other)` | `__ipow__(self, other)` |
| `@` (Matrix) | `__matmul__(self, other)` | `__rmatmul__(self, other)` | `__imatmul__(self, other)` |
| `&` | `__and__(self, other)` | `__rand__(self, other)` | `__iand__(self, other)` |
| `\|` | `__or__(self, other)` | `__ror__(self, other)` | `__ior__(self, other)` |
| `^` | `__xor__(self, other)` | `__rxor__(self, other)` | `__ixor__(self, other)` |

### The `NotImplemented` Rule
- **Rule**: If an operator method cannot handle the type of `other`, it must return `NotImplemented`, **never raise `TypeError` or `NotImplementedError`**.
- Raising an exception terminates evaluation immediately, preventing the right-hand operand from attempting its `__r*__` method.
- Returning `NotImplemented` allows Python to gracefully fall back to the right-hand operand or produce a helpful error message: `TypeError: unsupported operand type(s) for +: 'A' and 'B'`.

---

## 3. Real-World Code Examples

### Example 1: Robust Vector Addition with `__add__` and `__radd__`
```python
class Vector2D:
    def __init__(self, x: float, y: float):
        self.x = float(x)
        self.y = float(y)

    def __repr__(self) -> str:
        return f"Vector2D({self.x}, {self.y})"

    def __add__(self, other):
        # Case 1: Vector + Vector
        if isinstance(other, Vector2D):
            return Vector2D(self.x + other.x, self.y + other.y)
        # Case 2: Vector + scalar (float/int)
        elif isinstance(other, (int, float)):
            return Vector2D(self.x + other, self.y + other)
        # Case 3: Unknown type -> return NotImplemented!
        return NotImplemented

    def __radd__(self, other):
        # Handles scalar + Vector (e.g. 5 + v) by delegating to __add__
        return self.__add__(other)


v1 = Vector2D(1, 2)
v2 = Vector2D(3, 4)

print(f"Vector + Vector: {v1 + v2}")
print(f"Vector + Scalar: {v1 + 10}")
print(f"Scalar + Vector: {10 + v1}")
```
Output:
```
Vector + Vector: Vector2D(4.0, 6.0)
Vector + Scalar: Vector2D(11.0, 12.0)
Scalar + Vector: Vector2D(11.0, 12.0)
```

### Example 2: In-Place Mutation with `__iadd__` vs Fallback
```python
class MutableBuffer:
    def __init__(self, data: list[int] | None = None):
        self.data = list(data or [])

    def __iadd__(self, other):
        # Mutates existing instance in-place and returns self
        if isinstance(other, (list, tuple)):
            self.data.extend(other)
            return self
        elif isinstance(other, MutableBuffer):
            self.data.extend(other.data)
            return self
        return NotImplemented


buf = MutableBuffer([1, 2])
initial_id = id(buf)
buf += [3, 4]

print(f"Buffer data: {buf.data}")
print(f"Preserved instance identity (in-place): {id(buf) == initial_id}")
```
Output:
```
Buffer data: [1, 2, 3, 4]
Preserved instance identity (in-place): True
```

### Example 3: Comparison Operators and `@functools.total_ordering`
```python
from functools import total_ordering

@total_ordering
class PriorityTask:
    def __init__(self, priority: int, name: str):
        self.priority = priority
        self.name = name

    def __eq__(self, other):
        if not isinstance(other, PriorityTask):
            return NotImplemented
        return self.priority == other.priority

    def __lt__(self, other):
        if not isinstance(other, PriorityTask):
            return NotImplemented
        return self.priority < other.priority


t1 = PriorityTask(1, "Low")
t2 = PriorityTask(5, "High")
t3 = PriorityTask(1, "Another Low")

print(f"t1 < t2:  {t1 < t2}")
print(f"t1 <= t3: {t1 <= t3}")
print(f"t2 > t1:  {t2 > t1}")
print(f"t1 == t3: {t1 == t3}")
```
Output:
```
t1 < t2:  True
t1 <= t3: True
t2 > t1:  True
t1 == t3: True
```

### Example 4: Matrix Multiplication Operator `@` (`__matmul__`)
```python
class Matrix2x2:
    def __init__(self, a, b, c, d):
        self.m = [[a, b], [c, d]]

    def __repr__(self):
        return f"Matrix2x2({self.m[0]}, {self.m[1]})"

    def __matmul__(self, other):
        if not isinstance(other, Matrix2x2):
            return NotImplemented
        a1, b1 = self.m[0]
        c1, d1 = self.m[1]
        a2, b2 = other.m[0]
        c2, d2 = other.m[1]
        return Matrix2x2(
            a1*a2 + b1*c2, a1*b2 + b1*d2,
            c1*a2 + d1*c2, c1*b2 + d1*d2
        )


m1 = Matrix2x2(1, 2, 3, 4)
m2 = Matrix2x2(2, 0, 1, 2)
print(f"m1 @ m2: {m1 @ m2}")
```
Output:
```
m1 @ m2: Matrix2x2([4, 4], [10, 8])
```

### Example 5: Proper NotImplemented Fallback Handling
```python
class Fahrenheit:
    def __init__(self, degrees: float):
        self.degrees = degrees

    def __eq__(self, other):
        if isinstance(other, Fahrenheit):
            return self.degrees == other.degrees
        # Signal that Fahrenheit doesn't know how to compare with other
        return NotImplemented


class Celsius:
    def __init__(self, degrees: float):
        self.degrees = degrees

    def __eq__(self, other):
        if isinstance(other, Fahrenheit):
            # C = (F - 32) * 5/9
            return round(self.degrees, 2) == round((other.degrees - 32) * 5 / 9, 2)
        return NotImplemented


f = Fahrenheit(32.0)
c = Celsius(0.0)

# f == c: f.__eq__(c) returns NotImplemented.
# Python then falls back to c.__eq__(f), which returns True!
print(f"f == c: {f == c}")
```
Output:
```
f == c: True
```

---

## 4. Common Anti-Patterns & Gotchas

| Bad Practice | Root Cause | Clean Solution |
| :--- | :--- | :--- |
| Raising `TypeError` instead of returning `NotImplemented` | Short-circuits Python's fallback to the right-hand operand's reflected method | Return `NotImplemented` to allow graceful fallback |
| Raising `NotImplementedError` in operator methods | `NotImplementedError` is an exception for abstract classes, not the operator sentinel | Return the singleton constant `NotImplemented` |
| Forgetting to return `self` from `__iadd__` | `a += b` assigns the return value of `__iadd__` back to `a`, causing `a` to become `None` | Always `return self` at the end of in-place dunder methods |
| Implementing `__eq__` without considering symmetric comparisons | `a == b` might evaluate differently from `b == a` | Ensure comparison logic is symmetric and returns `NotImplemented` for foreign types |
| Mutating immutable objects inside `__iadd__` | Violates immutability contract | Return a new instance from `__iadd__` (or omit `__iadd__` to let Python fall back to `__add__`) |
| Manually writing all 6 rich comparison operators | Repetitive boilerplate and prone to contradictory logic | Implement `__eq__` and `__lt__`, decorate with `@functools.total_ordering` |

---

## 5. Interview Q&A

<details>
<summary>1. What is the difference between NotImplemented and NotImplementedError?</summary>
<code>NotImplemented</code> is a built-in singleton constant returned by binary special methods to signal that an operation is not supported for the given operand types, prompting Python to try reflected methods or raise a <code>TypeError</code>. <code>NotImplementedError</code> is a standard exception subclassed from <code>RuntimeError</code>, intended to be raised in abstract base classes or unfinished stubs.
</details>

<details>
<summary>2. How does Python evaluate a + b when a and b are of different types?</summary>
1. Python checks if <code>type(b)</code> is a strict subclass of <code>type(a)</code>. If so, <code>b.__radd__(a)</code> is attempted first to allow subclasses to override parent behavior.
2. Otherwise, Python calls <code>a.__add__(b)</code>.
3. If <code>a.__add__(b)</code> returns <code>NotImplemented</code>, Python calls <code>b.__radd__(a)</code>.
4. If <code>b.__radd__(a)</code> also returns <code>NotImplemented</code> (or is absent), Python raises <code>TypeError: unsupported operand type(s)</code>.
</details>

<details>
<summary>3. Why must __iadd__ return a value?</summary>
In Python, augmented assignment expressions like <code>x += y</code> are evaluated as <code>x = x.__iadd__(y)</code>. If <code>__iadd__</code> does not explicitly return <code>self</code>, it implicitly returns <code>None</code>, inadvertently overwriting the variable <code>x</code> with <code>None</code>.
</details>

<details>
<summary>4. What happens during a += b if __iadd__ is not defined?</summary>
If <code>__iadd__</code> is not defined on <code>type(a)</code>, Python automatically falls back to <code>a = a + b</code> (calling <code>__add__</code> or <code>__radd__</code>). This is standard behavior for immutable types like <code>int</code>, <code>str</code>, and <code>tuple</code>.
</details>

<details>
<summary>5. How does @functools.total_ordering simplify rich comparisons?</summary>
Python defines 6 rich comparison operators: <code>__eq__</code>, <code>__ne__</code>, <code>__lt__</code>, <code>__le__</code>, <code>__gt__</code>, <code>__ge__</code>. The <code>@total_ordering</code> class decorator requires only <code>__eq__</code> and any one ordering method (e.g. <code>__lt__</code>), and automatically derives all remaining comparison methods.
</details>

<details>
<summary>6. What does the matrix multiplication operator @ (__matmul__) do?</summary>
Introduced in Python 3.5 (PEP 465), <code>@</code> was added specifically for matrix and tensor multiplication in numerical libraries like NumPy, PyTorch, and TensorFlow, cleanly separating matrix multiplication from element-wise multiplication (<code>*</code>).
</details>

<details>
<summary>7. What does the bitwise inversion operator ~ invoke?</summary>
The unary tilde operator <code>~obj</code> invokes the <code>__invert__(self)</code> special method.
</details>

<details>
<summary>8. Why is __ne__ automatically derived if only __eq__ is implemented?</summary>
In Python 3, if a class defines <code>__eq__</code> but not <code>__ne__</code>, Python provides a default <code>__ne__</code> implementation that delegates to <code>not (self == other)</code>, eliminating the need to manually implement <code>__ne__</code>.
</details>

---

## 6. Practical Challenges

<details>
<summary>Challenge 1: Complete Fraction Type with Operator Overloading</summary>

Implement an immutable `Fraction(num, den)` class that supports `+`, `-`, `*`, `==` with other fractions and ints, returning simplified fractions.

```python
import math

class Fraction:
    def __init__(self, num: int, den: int):
        if den == 0:
            raise ZeroDivisionError("Denominator cannot be zero")
        g = math.gcd(num, den)
        self.num = num // g
        self.den = den // g

    def __repr__(self):
        return f"{self.num}/{self.den}"

    def __add__(self, other):
        if isinstance(other, int):
            other = Fraction(other, 1)
        if isinstance(other, Fraction):
            return Fraction(self.num * other.den + other.num * self.den, self.den * other.den)
        return NotImplemented

    def __radd__(self, other):
        return self.__add__(other)

    def __eq__(self, other):
        if isinstance(other, int):
            other = Fraction(other, 1)
        if isinstance(other, Fraction):
            return (self.num, self.den) == (other.num, other.den)
        return NotImplemented

f1 = Fraction(1, 2)
f2 = Fraction(1, 4)
assert f1 + f2 == Fraction(3, 4)
assert f1 + 1 == Fraction(3, 2)
assert 1 + f1 == Fraction(3, 2)
```
</details>

<details>
<summary>Challenge 2: Bitwise Path Builder via / Operator</summary>

Overload the `/` operator (`__truediv__`) on a `PathBuilder` class to concatenate path segments cleanly like `path / "subfolder" / "file.txt"`.

```python
class PathBuilder:
    def __init__(self, path: str = ""):
        self.path = path.strip("/")

    def __truediv__(self, other):
        if isinstance(other, str):
            clean_other = other.strip("/")
            new_path = f"{self.path}/{clean_other}" if self.path else clean_other
            return PathBuilder(new_path)
        return NotImplemented

    def __str__(self):
        return f"/{self.path}"

p = PathBuilder("var") / "log" / "app.log"
assert str(p) == "/var/log/app.log"
```
</details>

<details>
<summary>Challenge 3: Set-Like Union and Intersection Operators</summary>

Build a `TagSet` class supporting `|` (`__or__`) for union and `&` (`__and__`) for intersection.

```python
class TagSet:
    def __init__(self, *tags):
        self.tags = frozenset(tags)

    def __or__(self, other):
        if isinstance(other, TagSet):
            return TagSet(*(self.tags | other.tags))
        return NotImplemented

    def __and__(self, other):
        if isinstance(other, TagSet):
            return TagSet(*(self.tags & other.tags))
        return NotImplemented

    def __eq__(self, other):
        if isinstance(other, TagSet):
            return self.tags == other.tags
        return NotImplemented

t1 = TagSet("python", "backend")
t2 = TagSet("backend", "cloud")
assert (t1 | t2) == TagSet("python", "backend", "cloud")
assert (t1 & t2) == TagSet("backend")
```
</details>

<details>
<summary>Challenge 4: Currency Arithmetic with Exchange Conversion</summary>

Implement `Money(amount, currency)` that allows adding two `Money` instances of the same currency directly, and converts different currencies using an exchange rate table.

```python
class Money:
    RATES = {("EUR", "USD"): 1.10, ("USD", "EUR"): 0.91}

    def __init__(self, amount: float, currency: str = "USD"):
        self.amount = round(float(amount), 2)
        self.currency = currency

    def __add__(self, other):
        if not isinstance(other, Money):
            return NotImplemented
        if self.currency == other.currency:
            return Money(self.amount + other.amount, self.currency)
        rate = self.RATES.get((other.currency, self.currency))
        if rate is None:
            raise ValueError(f"No rate between {other.currency} and {self.currency}")
        converted = other.amount * rate
        return Money(self.amount + converted, self.currency)

m1 = Money(100, "USD")
m2 = Money(50, "EUR")
res = m1 + m2
assert res.currency == "USD"
assert res.amount == 155.0
```
</details>

<details>
<summary>Challenge 5: Safe In-Place Accumulator</summary>

Create an `Accumulator` class where `+=` mutates the internal running sum and history list, returning `self`.

```python
class Accumulator:
    def __init__(self):
        self.total = 0
        self.history = []

    def __iadd__(self, val: int):
        self.total += val
        self.history.append(val)
        return self

acc = Accumulator()
acc += 10
acc += 20
assert acc.total == 30
assert acc.history == [10, 20]
```
</details>

<details>
<summary>Challenge 6: Rich Comparison with Mixed Types</summary>

Build a `Version(major, minor, patch)` class implementing complete comparisons against both other `Version` instances and string representations (e.g. `"1.2.0"`).

```python
from functools import total_ordering

@total_ordering
class Version:
    def __init__(self, major: int, minor: int, patch: int):
        self.v = (major, minor, patch)

    @classmethod
    def parse(cls, s: str):
        return cls(*map(int, s.split(".")))

    def __eq__(self, other):
        if isinstance(other, str):
            other = Version.parse(other)
        if isinstance(other, Version):
            return self.v == other.v
        return NotImplemented

    def __lt__(self, other):
        if isinstance(other, str):
            other = Version.parse(other)
        if isinstance(other, Version):
            return self.v < other.v
        return NotImplemented

v = Version(2, 1, 0)
assert v == "2.1.0"
assert v > "2.0.9"
assert v < "2.2.0"
```
</details>
