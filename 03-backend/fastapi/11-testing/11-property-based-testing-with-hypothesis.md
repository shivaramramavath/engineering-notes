---
title: "Property-Based Testing with Hypothesis in FastAPI"
category: "11-testing"
order: 11
tags: ["fastapi", "testing", "hypothesis", "property-based-testing", "fuzzing", "pydantic"]
py_version: "3.12+"
fastapi_version: "0.115+"
starlette_version: "0.38+"
pydantic_version: "2.8+"
tested_with: "Python 3.12, 3.13"
---

# Property-Based Testing with Hypothesis in FastAPI

## 1. Conceptual Foundation & Mental Model

Traditional unit tests are **example-based**: a developer picks 3 or 4 specific inputs (e.g. `x = 5`, `x = 0`, `x = -1`) and asserts on the corresponding expected outputs. 

However, human developers suffer from **confirmation bias**: we instinctively choose inputs that we know our code handles well. We rarely think to test:
- What happens if a username contains Right-to-Left Arabic override characters (`\u202e`)?
- What happens if a numeric price is negative zero (`-0.0`) or `float('nan')`?
- What happens if a list contains 10,000 duplicate UUIDs?

**Property-Based Testing (PBT)**, pioneered by QuickCheck in Haskell and implemented in Python by **Hypothesis**, turns this paradigm upside down:
- Instead of testing individual examples, you define **universal properties** or invariants that must *always* hold true for all valid inputs (e.g., *"Encoding and then decoding a payload must always return the original payload"* or *"The account balance can never drop below zero"*).
- Hypothesis then acts as an adversarial fuzzer, generating hundreds of randomized, boundary-pushing inputs to systematically break your code.
- When it finds an input that violates your property, it automatically executes an algorithm known as **Shrinking** to reduce the complex failing input down to the simplest, smallest reproducible test case.

### The Mental Model: The Master Locksmith vs The Burglar with 10,000 Keys
- **Example-Based Testing** is checking that your front door lock opens with Key A and Key B. You test the keys you cut yourself.
- **Property-Based Testing** is hiring an adversarial master burglar with a machine that cuts 10,000 bizarre, microscopic, irregular keys (Hypothesis strategies). The burglar tests:
  - *"Property: The door must NEVER unlock unless the exact cylinder pins match."*
  - If a bent paperclip unlocks the door (**Bug Discovered**), the burglar files the paperclip down until finding the absolute thinnest single wire that picks the lock (**Shrinking**).

---

## 2. Architecture & The Hypothesis Execution Loop

```
   1. Strategy Definition: `st.integers()`, `st.text()`, `st.emails()`
                      |
                      v
   +-------------------------------------------------------------+
   | Hypothesis Test Generation Engine                           |
   |                                                             |
   | Generates pseudo-random, adversarial edge-case inputs:      |
   | - Strings: Empty `""`, spaces `"  "`, unicode `\x00`, emoji |
   | - Numbers: 0, -1, NaN, Infinity, 2^63 - 1                   |
   +-------------------------------------------------------------+
                      |
                      v
   Executes Test Function: `test_function(x)`
                      |
        +-------------+-------------+
        |                           |
     PASSES                      FAILS!
        |                           |
        v                           v
   Next Iteration          [ SHRINKING PHASE ]
   (Up to 100 examples)    Systematically reduces input:
                           "long_failing_str_12345" -> " "
                                    |
                                    v
                           Reports minimal reproducible failure
                           Saves to `.hypothesis/examples`
```

---

## 3. Comprehensive Syntax & Core Mechanics

### 1. Basic Hypothesis Test Structure
```python
from hypothesis import given, strategies as st

def reverse_string(s: str) -> str:
    return s[::-1]

# Universal Property: Reversing a string twice always returns the original string!
@given(st.text())
def test_reverse_roundtrip_invariant(s: str):
    assert reverse_string(reverse_string(s)) == s
```

### 2. Common Hypothesis Strategies
- `st.integers(min_value=1, max_value=1000)`: Constrained numbers.
- `st.text(alphabet=st.characters(blacklist_categories=("Cs",)))`: Safe Unicode text.
- `st.emails()`: Valid RFC email formats.
- `st.lists(st.integers(), min_size=1, max_size=50)`: Dynamic arrays.
- `st.from_type(PydanticModel)`: Automatically generates valid instances of any Pydantic model!

---

## 4. Practical Implementation

### Example 1: Testing Pydantic Serialization Roundtrip Invariant
Testing that serializing a model to JSON and parsing it back is completely lossless:
```python
import pytest
from hypothesis import given, strategies as st
from pydantic import BaseModel, Field

class OrderItem(BaseModel):
    product_id: str = Field(min_length=1, max_length=50)
    quantity: int = Field(gt=0, le=1000)
    unit_price: float = Field(ge=0.01, le=10000.0)

# Property: Dump -> Validate roundtrip must be perfectly identical
@given(
    product_id=st.text(min_size=1, max_size=50).filter(lambda x: x.strip() != ""),
    quantity=st.integers(min_value=1, max_value=1000),
    unit_price=st.floats(min_value=0.01, max_value=10000.0, allow_nan=False, allow_infinity=False)
)
def test_order_item_serialization_roundtrip(product_id: str, quantity: int, unit_price: float):
    # Construct model
    original = OrderItem(product_id=product_id, quantity=quantity, unit_price=unit_price)
    
    # Serialize to JSON string
    json_data = original.model_dump_json()
    
    # Parse back from JSON
    reconstructed = OrderItem.model_validate_json(json_data)
    
    # Verify invariants
    assert original.product_id == reconstructed.product_id
    assert original.quantity == reconstructed.quantity
    assert round(original.unit_price, 2) == round(reconstructed.unit_price, 2)
```

### Example 2: Testing Token Expiration Logic Invariants
```python
import time
from hypothesis import given, strategies as st

def is_token_active(created_at: float, current_time: float, ttl_seconds: float) -> bool:
    if current_time < created_at:
        return False  # Future timestamp anomaly
    return (current_time - created_at) <= ttl_seconds

# Property: A token is active IF AND ONLY IF (current - created) <= ttl
@given(
    created_at=st.floats(min_value=1000.0, max_value=100000.0),
    delta=st.floats(min_value=-500.0, max_value=5000.0),
    ttl=st.floats(min_value=1.0, max_value=3600.0)
)
def test_token_active_invariant(created_at: float, delta: float, ttl: float):
    current_time = created_at + delta
    is_active = is_token_active(created_at, current_time, ttl)
    
    if delta < 0:
        assert is_active is False
    elif delta <= ttl:
        assert is_active is True
    else:
        assert is_active is False
```

### Example 3: Edge Case — Testing Database Keyset Pagination Monotonicity
Testing that keyset pagination never returns duplicate items and preserves strict ordering:
```python
from hypothesis import given, strategies as st

def paginate_keyset(items: list[int], last_seen_id: int | None, limit: int = 5) -> list[int]:
    # Pure keyset logic
    sorted_items = sorted(items)
    if last_seen_id is not None:
        sorted_items = [x for x in sorted_items if x > last_seen_id]
    return sorted_items[:limit]

# Property: Paginating across all items must visit every unique item exactly once!
@given(st.lists(st.integers(min_value=1, max_value=10000), unique=True, min_size=1, max_size=100))
def test_keyset_pagination_covers_all_items_without_duplicates(unique_ids: list[int]):
    visited = []
    last_id = None
    
    while True:
        page = paginate_keyset(unique_ids, last_seen_id=last_id, limit=10)
        if not page:
            break
        visited.extend(page)
        last_id = page[-1]
        
    assert visited == sorted(unique_ids)
```

### Example 4: Failure Case — Float Precision Inaccuracy Discovered by Hypothesis
```python
# CODE WITH SUBTLE ARITHMETIC BUG
def add_money(cents_a: float, cents_b: float) -> float:
    # BUG: Floating-point precision issues in Python!
    return cents_a + cents_b

# Hypothesis will immediately discover:
# cents_a = 0.1, cents_b = 0.2
# add_money(0.1, 0.2) returns 0.30000000000000004!
# If asserted: assert add_money(0.1, 0.2) == 0.3 -> FAILS!
# Solution: Use Decimal or integer cents!
```

### Example 5: Idiomatic Modern — Generating Pydantic Models Automatically
Hypothesis integrates directly with Pydantic via `hypothesis.strategies.builds()` or `hypothesis-pydantic`:
```python
from hypothesis import given, strategies as st
from pydantic import BaseModel

class UserPayload(BaseModel):
    username: str
    age: int
    is_active: bool

# Hypothesis automatically infers strategies from Pydantic type annotations!
@given(st.builds(UserPayload))
def test_user_payload_schema_validity(payload: UserPayload):
    assert isinstance(payload.username, str)
    assert isinstance(payload.age, int)
    assert isinstance(payload.is_active, bool)
```

---

## 5. Edge Cases, Pitfalls & Failure Modes

1. **Floating Point Non-Reflexivity (`NaN != NaN`)**: Generating unconstrained floats includes `float('nan')`. In Python, `float('nan') == float('nan')` evaluates to `False`! Always configure `allow_nan=False` in float strategies unless testing NaN handling explicitly.
2. **Filtering Bottlenecks (`.filter()`)**: Using `.filter(lambda x: complex_predicate(x))` can cause Hypothesis to discard too many examples, raising `HypothesisException: Could not find any valid examples`. Instead of filtering, use targeted strategies (e.g. `st.integers(min_value=5)` instead of `st.integers().filter(lambda x: x >= 5)`).
3. **Flaky Non-Deterministic Tests in Property Tests**: If your test relies on external wall-clock time, randomized UUIDs, or uncontrolled network state, Hypothesis cannot reproduce the failure. Property tests must be 100% deterministic functions of their inputs.
4. **Hypothesis Example Database**: When Hypothesis discovers a failure, it records the minimal reproduction payload in the `.hypothesis/` directory on disk. On the next test run, it replays the failed example *first*, guaranteeing instant regression testing!

---

## 6. Common Mistakes & Anti-Patterns

| Anti-Pattern | Root Cause | Consequence | Correct Practice |
|:---|:---|:---|:---|
| Unconstrained `st.floats()` | Forgetting `NaN` and `Infinity` exist | `AssertionError` due to `NaN != NaN` | Set `allow_nan=False, allow_infinity=False` |
| Over-filtering strategies with `.filter()` | Discarding too many examples | `Unsatisfiable: Could not find valid examples` | Use bounded strategies directly |
| Non-deterministic assertions inside `@given` | Generating random values inside test | Flaky tests; unreproducible failures | Let Hypothesis provide all random inputs |
| Excessive example counts in unit tests | Running 1,000 examples per test | 15-minute test suites in CI | Set `@settings(max_examples=50)` |
| Testing mock functions instead of real logic | Mocking out the algorithm | Tests the mock, not the invariant | Test pure domain algorithms |
| Deleting the `.hypothesis/` directory | Ignoring regression cache | Lost reproduction seeds for discovered bugs | Commit or persist `.hypothesis` in CI |

---

## 7. Performance & Scalability Considerations

- **Tuning `max_examples`**: Hypothesis defaults to 100 examples per test. For fast unit tests ($<0.1\text{ms}$), 100 examples execute in 10ms. For tests involving database operations, reduce to `@settings(max_examples=25)`.
- **Database Caching**: Hypothesis caches previous failure seeds, ensuring bugs discovered locally fail immediately in CI on the first example.

---

## 8. Security & OWASP Guardrails

- **Adversarial Input Fuzzing**: Property-based testing is the most effective automated defense against Injection, Integer Overflow, and Buffer Boundary vulnerabilities, subjecting inputs to extreme Unicode and numeric edge cases that human developers overlook.

---

## 9. Testing Strategies & Pytest Recipes

```python
import pytest
from hypothesis import given, settings, strategies as st

# Custom settings profile for CI pipelines
settings.register_profile("ci", max_examples=50, deadline=500)
settings.load_profile("ci")

@given(st.lists(st.integers()))
def test_sorting_preserves_length_and_elements(lst: list[int]):
    sorted_lst = sorted(lst)
    # Invariant 1: Length is identical
    assert len(sorted_lst) == len(lst)
    # Invariant 2: Order is non-decreasing
    assert all(sorted_lst[i] <= sorted_lst[i+1] for i in range(len(sorted_lst)-1))
```

---

## 10. Real-World Case Study / Production Incident

**The Incident**: A cryptocurrency trading desk deployed an automated market-making bot. During a flash crash, the bot attempted to execute a trade and crashed with `ZeroDivisionError: float division by zero`, freezing trading operations while positions lost \$450,000 in value.

**Root Cause**: The volume-weighted average price (VWAP) calculation function assumed trade volumes were always positive integers:
```python
def calculate_vwap(trades):
    total_volume = sum(t["volume"] for t in trades)
    # If trades is empty list: total_volume = 0 -> ZeroDivisionError!
    return sum(t["price"] * t["volume"] for t in trades) / total_volume
```
The developer wrote 5 manual unit tests, but all 5 tests passed lists containing 2 to 10 trades. Nobody tested an empty list `[]`.

**The Fix**:
1. Added a property-based test with Hypothesis:
   ```python
   @given(st.lists(st.builds(Trade)))
   def test_vwap_never_crashes(trades):
       vwap = calculate_vwap(trades)
       assert vwap >= 0.0
   ```
2. Hypothesis caught the empty list `[]` bug in 3 milliseconds during the initial test run, shrinking the failing input to `trades = []`.
3. Updated the function to return `None` or `0.0` when volume is zero.

---

## 11. Technical Interview Questions & Answers

<details>
<summary>1. What is the fundamental difference between Example-Based Testing and Property-Based Testing?</summary>
<b>Example-Based Testing</b> checks specific, hardcoded inputs against expected outputs chosen by the developer. <b>Property-Based Testing</b> defines universal truths (invariants) that must hold for all valid inputs; a framework (like Hypothesis) generates hundreds of randomized, adversarial edge cases to attempt to disprove the invariant.
</details>

<details>
<summary>2. What is "Shrinking" in Hypothesis?</summary>
When Hypothesis finds an input that breaks an assertion, it does not simply report the large, complex random data. It systematically simplifies the input (e.g. shortening strings, reducing integers toward zero, removing list elements) until it finds the <b>minimal reproducible example</b> that still triggers the failure, making debugging straightforward.
</details>

<details>
<summary>3. What is a "Roundtrip Invariant" in property-based testing?</summary>
A roundtrip invariant asserts that transforming data from format A to format B and back to format A produces the exact original value: <code>deserialize(serialize(x)) == x</code>. It is commonly used to test encoders, decoders, compression algorithms, and Pydantic serialization.
</details>

<details>
<summary>4. Why should you use bounded strategies instead of <code>.filter()</code> in Hypothesis?</summary>
Using <code>.filter()</code> generates random inputs and discards any that do not match the predicate. If the filter condition is narrow, Hypothesis wastes iterations generating and rejecting invalid samples, eventually raising an <code>Unsatisfiable</code> error. Bounded strategies (like <code>st.integers(min_value=10)</code>) generate valid inputs directly without rejection.
</details>

<details>
<summary>5. What is the role of the <code>.hypothesis/</code> directory created by tests?</summary>
It is an on-disk cache database where Hypothesis stores inputs that previously caused test failures. On subsequent test runs, Hypothesis replays these exact failing examples first, ensuring that previously discovered bugs are immediately caught as regression tests.
</details>

<details>
<summary>6. How can you generate arbitrary Pydantic models with Hypothesis?</summary>
Using <code>hypothesis.strategies.builds(MyPydanticModel)</code>. Hypothesis inspects the field types and default values of the Pydantic class and automatically generates matching strategies for all fields.
</details>

<details>
<summary>7. What is an Idempotence Invariant in property-based testing?</summary>
An idempotence invariant asserts that applying an operation a second time produces no further side effects or state changes: <code>f(f(x)) == f(x)</code>. Examples include text trimming (<code>strip(strip(s)) == strip(s)</code>) or database state normalization.
</details>

<details>
<summary>8. Why must property-based test functions be strictly deterministic?</summary>
Hypothesis relies on being able to re-run the test function with the exact same inputs during its shrinking phase. If the test depends on random numbers, system time, or external network state, the test will behave inconsistently, preventing Hypothesis from shrinking or reproducing the bug.
</details>

---

## 12. Practical Exercises & Solutions

### Exercise 1 (Easy): String Trimming Idempotency Invariant
**Task**: Use Hypothesis to verify that Python's `s.strip()` is idempotent (`s.strip().strip() == s.strip()`) for all arbitrary text strings.
<details>
<summary>Solution</summary>

```python
from hypothesis import given, strategies as st

@given(st.text())
def test_strip_is_idempotent(s: str):
    once = s.strip()
    twice = once.strip()
    assert once == twice
```
</details>

### Exercise 2 (Easy): Integer Addition Commutativity
**Task**: Use `@given` to verify that integer addition is commutative (`a + b == b + a`) for all integers.
<details>
<summary>Solution</summary>

```python
from hypothesis import given, strategies as st

@given(st.integers(), st.integers())
def test_addition_commutative(a: int, b: int):
    assert a + b == b + a
```
</details>

### Exercise 3 (Medium): URL Path Sanitizer Invariant
**Task**: Implement a function `sanitize_slug(title: str) -> str` that replaces spaces with hyphens and converts to lowercase. Use Hypothesis to verify that the output never contains spaces or uppercase characters.
<details>
<summary>Solution</summary>

```python
from hypothesis import given, strategies as st

def sanitize_slug(title: str) -> str:
    cleaned = title.strip().lower().replace(" ", "-")
    return cleaned

@given(st.text())
def test_slug_invariants(title: str):
    slug = sanitize_slug(title)
    assert " " not in slug
    assert slug == slug.lower()
```
</details>

### Exercise 4 (Medium): Bounded Discount Calculator
**Task**: Build a function `apply_discount(price: float, discount_pct: float) -> float`. Use Hypothesis to verify that the final price is *always* between 0.0 and the original price for discounts between 0% and 100%.
<details>
<summary>Solution</summary>

```python
from hypothesis import given, strategies as st

def apply_discount(price: float, discount_pct: float) -> float:
    discount_amount = price * (discount_pct / 100.0)
    return round(max(0.0, price - discount_amount), 2)

@given(
    price=st.floats(min_value=0.0, max_value=100000.0, allow_nan=False),
    discount=st.floats(min_value=0.0, max_value=100.0, allow_nan=False)
)
def test_discount_bounds_invariant(price: float, discount: float):
    final_price = apply_discount(price, discount)
    assert 0.0 <= final_price <= price + 0.01  # Tolerance for float rounding
```
</details>

### Exercise 5 (Hard): Pydantic Model Generation with Auto-Shrink
**Task**: Create a Pydantic model `AccountRecord` requiring a non-empty string and a positive balance. Write a test asserting that attempting to construct `AccountRecord` with balance $< 0$ *always* raises `ValidationError`.
<details>
<summary>Solution</summary>

```python
import pytest
from hypothesis import given, strategies as st
from pydantic import BaseModel, Field, ValidationError

class AccountRecord(BaseModel):
    username: str = Field(min_length=1)
    balance_cents: int = Field(ge=0)

@given(
    username=st.text(min_size=1),
    negative_balance=st.integers(max_value=-1)
)
def test_negative_balance_always_rejected(username: str, negative_balance: int):
    with pytest.raises(ValidationError):
        AccountRecord(username=username, balance_cents=negative_balance)
```
</details>

### Exercise 6 (Hard): List Deduplication Preserves Invariants
**Task**: Write a function `deduplicate_preserve_order(items: list) -> list`. Use Hypothesis to verify two invariants: (1) all elements in output are unique, and (2) all elements in the input appear in the output.
<details>
<summary>Solution</summary>

```python
from hypothesis import given, strategies as st

def deduplicate_preserve_order(items: list[int]) -> list[int]:
    seen = set()
    result = []
    for item in items:
        if item not in seen:
            seen.add(item)
            result.append(item)
    return result

@given(st.lists(st.integers()))
def test_deduplication_invariants(items: list[int]):
    deduped = deduplicate_preserve_order(items)
    
    # Invariant 1: Result has no duplicates
    assert len(deduped) == len(set(deduped))
    
    # Invariant 2: Set of elements is identical
    assert set(deduped) == set(items)
    
    # Invariant 3: Result length is less than or equal to original
    assert len(deduped) <= len(items)
```
</details>
