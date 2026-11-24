# Module 01: Conditionals & Branching (09-conditionals)

> **Topic Overview**: Control flow and decision branching in Python: `if/elif/else` statements, conditional expressions (the ternary operator `x if cond else y`), truthiness evaluations, nested conditionals, guard clauses, and bytecode branch prediction.

---

## Metadata Table

| Field | Value |
|---|---|
| **Category** | Python Fundamentals & Syntax |
| **Target Level** | Beginner to Intermediate |
| **Python Version** | Python 3.10+ (tested on CPython 3.14.0) |
| **Tested With** | CPython 3.14.0, dis module, Ruff 0.17.6 |
| **Free-Threading / Platform** | Platform independent |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Code Examples](#5-code-examples)
6. [Edge Cases & Pitfalls](#6-edge-cases--pitfalls)
7. [Common Mistakes](#7-common-mistakes)
8. [Performance & Complexity](#8-performance--complexity)
9. [CPython Internals & Bytecode](#9-cpython-internals--bytecode)
10. [Best Practices](#10-best-practices)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why
- **Problem Statement**: Software must adapt dynamic behavior based on runtime inputs, permissions, network responses, and business state. Poorly structured conditional logic leads to deeply nested code blocks, difficult-to-trace edge cases, and high cyclomatic complexity.
- **Mental Model**: Python evaluates conditions sequentially from top to bottom. The first branch whose condition evaluates to truthy executes its block; all subsequent `elif` and `else` branches are skipped entirely.
- **When to Use**: Decision-making logic, parameter validation, role-based access control, and fallback routing.
- **When NOT to Use**: For multi-branch switching based on object shapes, sequence structures, or class attributes, prefer **Structural Pattern Matching** (`match/case`, covered in Note 11).

---

## 2. Core Concepts
1. **The `if/elif/else` Chain**:
   - `if <expr>:`: Initial mandatory branch.
   - `elif <expr>:`: Zero or more optional intermediate branches evaluated only if all preceding branches were falsy.
   - `else:`: Optional final fallback executed if no preceding branch was truthy.
2. **Conditional Expressions (Ternary Operator)**:
   - Syntax: `result = value_if_true if condition else value_if_false`
   - Short-circuits: Only the branch corresponding to the evaluated condition is executed.
3. **Guard Clauses & Early Exits**: An architectural pattern where invalid inputs or boundary conditions are checked at the top of a function, returning immediately to keep the primary happy path completely flat.
4. **Boolean Short-Circuiting**: Chained boolean expressions in conditions (`if a and b:`, `if x or y:`) evaluate left-to-right and stop as soon as the outcome is determined.

---

## 3. How It Works Internally

How CPython compiles and jumps across conditional branches:

```text
Source Code:
if score >= 80:
    grade = "B"
else:
    grade = "F"

Bytecode Execution Flow:
            │
            ▼
    [ LOAD_FAST score ]
    [ LOAD_CONST 80 ]
    [ COMPARE_OP (>=) ]
            │
            ├─► True?  ──► Continues to line 2 (Sets grade = "B")
            │              JUMP_FORWARD past else block to line 4!
            │
            ▼ False
    [ POP_JUMP_IF_FALSE to Else Offset ]
            │
            ▼
    [ Sets grade = "F" ]
            │
            ▼
    [ Offset 4: Execution Continues ]
```

---

## 4. Syntax & API Reference

### Multi-Branch Conditional Statement
```python
if condition_1:
    action_1()
elif condition_2:
    action_2()
else:
    default_action()
```

### Ternary Conditional Expression
```python
status_message = "Access Granted" if is_authenticated else "Access Denied"
```

---

## 5. Code Examples

### 1. Minimal / Basic Example: Ternary Conditional Expression
```python
status_code = 404
message = "OK" if status_code == 200 else ("Not Found" if status_code == 404 else "Error")

print(f"Status: {status_code} -> {message}")
```
**Output**:
```text
Status: 404 -> Not Found
```

---

### 2. Real-World Production Example: HTTP Response Status Classifier
```python
def classify_http_status(status_code: int) -> str:
    """Classifies an HTTP response status code into RFC categories."""
    if not (100 <= status_code <= 599):
        raise ValueError(f"Invalid HTTP status code: {status_code}")

    if 200 <= status_code < 300:
        return "Success"
    elif 300 <= status_code < 400:
        return "Redirection"
    elif 400 <= status_code < 500:
        return "Client Error"
    elif 500 <= status_code < 600:
        return "Server Error"
    else:
        return "Informational"

for code in [200, 301, 404, 503]:
    print(f"Code {code}: {classify_http_status(code)}")
```
**Output**:
```text
Code 200: Success
Code 301: Redirection
Code 404: Client Error
Code 503: Server Error
```

---

### 3. Edge Case: Lazy Short-Circuiting in Ternary Expressions
```python
def expensive_calculation() -> int:
    print("Executing expensive calculation...")
    return 42

flag = False
# The ternary expression does NOT execute the false branch when condition is True:
result = 100 if not flag else expensive_calculation()

print("Result:", result)
```
**Output**:
```text
Result: 100
```
*(Notice that `expensive_calculation()` was never invoked).*

---

### 4. Failure Case: Unintentional Assignment inside Conditions
```python
# In languages like C/Java, accidentally writing `if (x = 5)` assigns 5.
# In Python, assignment statements (=) are syntactically prohibited inside `if` statements!
syntax_test = """
x = 0
if x = 5:
    pass
"""

try:
    compile(syntax_test, "<test>", "exec")
except SyntaxError as exc:
    print(f"Caught syntax error: {type(exc).__name__}: Assignment is invalid in if conditions.")
```
**Output**:
```text
Caught syntax error: SyntaxError: Assignment is invalid in if conditions.
```
*(Note: To perform assignment in a conditional, use the explicit Walrus operator `:=`, covered in Note 12).*

---

### 5. Idiomatic Pythonic Pattern: Guard Clauses Flattening Business Logic
```python
from dataclasses import dataclass

@dataclass
class UserRequest:
    user_id: str | None
    auth_token: str | None
    rate_limit_remaining: int

def handle_request(req: UserRequest) -> str:
    """Uses flat guard clauses to handle all rejection scenarios upfront."""
    if not req.user_id:
        return "Rejected: Missing user ID"
    if not req.auth_token or len(req.auth_token) < 16:
        return "Rejected: Invalid auth token"
    if req.rate_limit_remaining <= 0:
        return "Rejected: Rate limit exceeded"

    # Happy path is completely unindented
    return f"Request processed successfully for user {req.user_id}"

req_valid = UserRequest("usr_42", "token_abcdef12345678", 50)
req_rate_limited = UserRequest("usr_42", "token_abcdef12345678", 0)

print(handle_request(req_valid))
print(handle_request(req_rate_limited))
```
**Output**:
```text
Request processed successfully for user usr_42
Rejected: Rate limit exceeded
```

---

## 6. Edge Cases & Pitfalls
1. **Chained Ternary Readability Decay**: Nesting ternary operators (`a if x else (b if y else c)`) degrades readability quickly. If you have more than one conditional branch, use standard `if/elif/else` blocks.
2. **Order of `elif` Precedence**: In overlapping ranges (e.g. checking `x > 50` before `x > 100`), order matters! Put more specific conditions before general ones.
3. **Evaluating Expensive Expressions First**: In `if is_admin and check_db_permissions(user):`, place the cheap memory check `is_admin` first to short-circuit before running the slow database query.

---

## 7. Common Mistakes

| Mistake | Why It Fails | Exception / Diagnostic | Correct Pattern |
|---|---|---|---|
| Deeply nested conditionals ("Pyramid of Doom") | High cyclomatic complexity and unreadable code | Maintainability bugs | Use early returns with guard clauses |
| Using `if x = 5:` | Single `=` is an assignment statement, illegal in conditionals | `SyntaxError: invalid syntax` | Use `==` for comparison or `:=` |
| Overlapping `elif` branches in wrong order | General condition triggers first, starving specific branch | Silent logic bugs | Order from most specific to least specific |
| Redundant else after a return | Adds unnecessary indentation to subsequent code | Code style violation | Remove `else:` if `if` branch returns |
| Re-evaluating identical conditions | Wastes CPU and slows down execution | Performance degradation | Cache result in variable |
| Using ternary operator with side effects | Ternary expressions are meant for value selection | Unpythonic style | Use standard `if/else` block for statements |

---

## 8. Performance & Complexity
- **Condition Evaluation**: $O(1)$ constant time for scalar comparisons.
- **Short-Circuit Optimization**: Placing the most frequently falsy condition first in an `and` chain avoids evaluating subsequent terms.

---

## 9. CPython Internals & Bytecode: The `POP_JUMP_IF_FALSE` Instruction
CPython compiles conditionals into jump instructions:
```python
import dis

def check_value(x: int) -> str:
    if x > 10:
        return "high"
    return "low"

dis.dis(check_value)
```
Disassembly:
```text
  1           0 RESUME                   0

  2           2 LOAD_FAST                0 (x)
              4 LOAD_CONST               1 (10)
              6 COMPARE_OP              68 (>)
             10 POP_JUMP_IF_FALSE        3 (to 18)

  3          12 LOAD_CONST               2 ('high')
             14 RETURN_VALUE

  4     >>   18 LOAD_CONST               3 ('low')
             20 RETURN_VALUE
```
If the comparison evaluates to false, `POP_JUMP_IF_FALSE` directly advances the instruction pointer `next_instr` to offset 18, skipping the `'high'` return path entirely.

---

## 10. Best Practices
1. Invert conditional checks and use guard clauses to return early.
2. Reserve the ternary operator (`x if c else y`) for simple variable assignments and return statements.
3. Order `and` conditions so that cheap in-memory checks precede expensive I/O operations.
4. Avoid redundant `else` blocks after branches that terminate with `return`, `raise`, or `break`.

---

## 11. Real-World Case Study: Feature Flag Evaluation Engine
A feature flag evaluation engine in an e-commerce microservice evaluated tenant access rules:
1. The original implementation had 5 levels of nested `if` statements checking user status, subscription tier, beta rollout percentage, and tenant overrides.
2. Sentry error reports revealed that a null tenant ID was throwing unhandled exceptions deep in the nested hierarchy.
3. The team refactored the logic into flat guard clauses:
   - Guard 1: Is user active? (If not, return False).
   - Guard 2: Is tenant globally disabled? (If so, return False).
   - Guard 3: Has explicit override? (If so, return override value).
   - Guard 4: Match percentage rollout.
4. Code coverage increased from 74% to 98% with zero unhandled exceptions.

---

## 12. Interview Questions

### Q1: How does Python's ternary operator differ in syntax from C/Java/JavaScript?
<details>
<summary><strong>Answer</strong></summary>
In C-family languages, the ternary operator uses the syntax `condition ? value_if_true : value_if_false`. In Python, the syntax is written in plain English order: `value_if_true if condition else value_if_false`. Like C's ternary operator, Python's conditional expression short-circuits: only the branch corresponding to the evaluated condition is executed.
</details>

### Q2: What are guard clauses and why are they considered a clean code best practice?
<details>
<summary><strong>Answer</strong></summary>
A guard clause is a conditional statement placed at the beginning of a function that checks for boundary conditions or invalid parameters and exits immediately (via `return` or `raise`). By handling exceptional cases upfront, the rest of the function represents the happy path without being indented inside nested `if/else` ladders, keeping cyclomatic complexity low.
</details>

### Q3: Why does `a and b or c` fail as a substitute for `b if a else c`?
<details>
<summary><strong>Answer</strong></summary>
In legacy Python, developers sometimes used `a and b or c` before the ternary operator was introduced. However, this pattern contains a critical bug: if `a` is true but `b` evaluates to falsy (e.g. `b = 0` or `b = ""`), `a and b` evaluates to `b` (falsy), and `or c` will cause `c` to be returned instead of `b`! The ternary operator `b if a else c` does not have this flaw.
</details>

### Q4: Can multiple statements be executed inside a Python ternary expression?
<details>
<summary><strong>Answer</strong></summary>
No. Python's conditional expression is strictly an *expression*, meaning it must evaluate to a single value. It cannot contain statement keywords like `return`, `raise`, `pass`, or variable assignment statements (though the Walrus operator `:=` can be used within its condition).
</details>

### Q5: How does short-circuiting optimize `if condition1 and condition2:`?
<details>
<summary><strong>Answer</strong></summary>
Python evaluates logical operators left-to-right. In an `and` chain, if `condition1` evaluates to falsy, CPython knows the entire expression cannot possibly be true. It immediately halts evaluation and jumps to the next branch without evaluating `condition2`. This allows placing fast checks first (e.g. `if obj is not None and obj.is_valid():`) to prevent `AttributeError`.
</details>

### Q6: What is the purpose of the `elif` keyword?
<details>
<summary><strong>Answer</strong></summary>
`elif` is short for "else if". It introduces an alternative conditional branch that is evaluated only if all preceding `if` and `elif` checks were falsy. Using `elif` avoids the rightward indentation drift that would occur if each subsequent check used a nested `else: \n if:` block.
</details>

### Q7: Does Python have a `switch/case` statement?
<details>
<summary><strong>Answer</strong></summary>
Python historically did not have a `switch` statement. In Python 3.10+, PEP 634 introduced **Structural Pattern Matching** (`match/case`), which provides a superset of traditional switch statements, matching not only literal values but also data structures, sequences, and object types.
</details>

### Q8: What happens if an `if` statement has no `else` block and all conditions evaluate to false?
<details>
<summary><strong>Answer</strong></summary>
The interpreter simply skips the entire conditional compound statement and continues execution at the next statement immediately following the `if` block, leaving any variables defined solely inside the unexecuted branches uninitialized.
</details>

---

## 13. Exercises & Challenges

### Challenge 1 (Easy): Leap Year Validator
Write a function `is_leap_year(year)` that returns `True` if a year is a leap year according to Gregorian calendar rules: divisible by 4, but not 100 unless also divisible by 400.
<details>
<summary><strong>Solution</strong></summary>

```python
def is_leap_year(year: int) -> bool:
    if year % 400 == 0:
        return True
    if year % 100 == 0:
        return False
    return year % 4 == 0

print("2024 is leap:", is_leap_year(2024))
print("1900 is leap:", is_leap_year(1900))
print("2000 is leap:", is_leap_year(2000))
```
</details>

### Challenge 2 (Easy): Tiered Discount Selector via Ternary
Write a function `get_discount_rate(customer_tier)` that returns `0.20` for `"gold"`, `0.10` for `"silver"`, and `0.0` for all other tiers using a conditional expression.
<details>
<summary><strong>Solution</strong></summary>

```python
def get_discount_rate(tier: str) -> float:
    t = tier.lower()
    return 0.20 if t == "gold" else (0.10 if t == "silver" else 0.0)

print("Gold discount:  ", get_discount_rate("gold"))
print("Bronze discount:", get_discount_rate("bronze"))
```
</details>

### Challenge 3 (Medium): Password Complexity Validator
Write a function `validate_password(pwd)` using guard clauses that verifies: length $\ge 8$, at least one uppercase letter, at least one digit, and at least one special character from `"!@#$%^&*"`, returning `(True, "OK")` or `(False, reason)`.
<details>
<summary><strong>Solution</strong></summary>

```python
def validate_password(pwd: str) -> tuple[bool, str]:
    if len(pwd) < 8:
        return False, "Password must be at least 8 characters long."
    if not any(c.isupper() for c in pwd):
        return False, "Password must contain at least one uppercase letter."
    if not any(c.isdigit() for c in pwd):
        return False, "Password must contain at least one digit."
    if not any(c in "!@#$%^&*" for c in pwd):
        return False, "Password must contain at least one special character (!@#$%^&*)."
    return True, "Password is valid."

print(validate_password("Short1!"))
print(validate_password("SecurePass123!"))
```
</details>

### Challenge 4 (Medium): Role-Based Permission Gate
Implement a function `can_perform_action(role, action, is_owner)` that evaluates permissions based on role hierarchy (`"admin"`, `"editor"`, `"viewer"`).
<details>
<summary><strong>Solution</strong></summary>

```python
def can_perform_action(role: str, action: str, is_owner: bool) -> bool:
    if role == "admin":
        return True
    if role == "editor":
        return action in ("view", "edit")
    if role == "viewer":
        return action == "view" or (action == "edit" and is_owner)
    return False

print("Admin delete:", can_perform_action("admin", "delete", False))
print("Viewer delete:", can_perform_action("viewer", "delete", False))
```
</details>

### Challenge 5 (Hard): AST Nested If-Depth Analyzer
Write a tool that analyzes Python code and flags any functions that exceed an indentation branching threshold of 3 nested `if` statements.
<details>
<summary><strong>Solution</strong></summary>

```python
import ast

def check_branching_depth(source: str, max_depth: int = 3) -> list[str]:
    tree = ast.parse(source)
    violations = []

    for func in [n for n in ast.walk(tree) if isinstance(n, ast.FunctionDef)]:
        def walk_node(node: ast.AST, depth: int) -> None:
            if isinstance(node, ast.If):
                if depth > max_depth:
                    violations.append(f"{func.name}: exceeded depth {max_depth}")
                for child in node.body + node.orelse:
                    walk_node(child, depth + 1)
            else:
                for child in ast.iter_child_nodes(node):
                    walk_node(child, depth)

        walk_node(func, 1)
    return violations

bad_code = """
def deeply_nested(x):
    if x > 0:
        if x > 10:
            if x > 20:
                if x > 30:
                    return "Deep"
"""
print("Violations:", check_branching_depth(bad_code, max_depth=3))
```
</details>

### Challenge 6 (Hard): Custom Decision Table Engine
Implement a declarative rule-based decision engine where conditions are expressed as predicates and actions are executed based on the first matching rule.
<details>
<summary><strong>Solution</strong></summary>

```python
from collections.abc import Callable
from typing import Any

class RuleEngine:
    def __init__(self) -> None:
        self.rules: list[tuple[Callable[[Any], bool], Callable[[Any], Any]]] = []

    def add_rule(self, predicate: Callable[[Any], bool], action: Callable[[Any], Any]) -> None:
        self.rules.append((predicate, action))

    def evaluate(self, context: Any, default_action: Callable[[Any], Any]) -> Any:
        for pred, action in self.rules:
            if pred(context):
                return action(context)
        return default_action(context)

engine = RuleEngine()
engine.add_rule(lambda score: score >= 90, lambda s: "Grade A")
engine.add_rule(lambda score: score >= 80, lambda s: "Grade B")

print("Evaluated 95:", engine.evaluate(95, lambda s: "Grade F"))
print("Evaluated 75:", engine.evaluate(75, lambda s: "Grade F"))
```
</details>
