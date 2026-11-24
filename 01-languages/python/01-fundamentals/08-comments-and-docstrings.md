# Module 01: Comments, Docstrings & Documentation (08-comments-and-docstrings)

> **Topic Overview**: Python documentation standards: single-line comments, inline comments, docstring conventions (PEP 257), Google vs NumPy vs Sphinx docstring formats, runtime introspection via `__doc__`, doctest verification, and documentation generation engines.

---

## Metadata Table

| Field | Value |
|---|---|
| **Category** | Python Fundamentals & Syntax |
| **Target Level** | Beginner to Intermediate |
| **Python Version** | Python 3.10+ (tested on CPython 3.14.0) |
| **Tested With** | CPython 3.14.0, doctest, inspect, Ruff 0.17.6 |
| **Free-Threading / Platform** | Platform independent |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Code Examples](#5-code-examples)
6. [Edge Cases & Pitfalls](#6-edge-cases--pitfalls)
7. [Common Mistakes](#7-Common-Mistakes)
8. [Performance & Complexity](#8-Performance--Complexity)
9. [CPython Internals & Bytecode](#9-CPython-Internals--Bytecode)
10. [Best Practices](#10-Best-Practices)
11. [Real-World Case Study](#11-Real-World-Case-Study)
12. [Interview Questions](#12-Interview-Questions)
13. [Exercises & Challenges](#13-Exercises--Challenges)

---

## 1. What & Why
- **Problem Statement**: Undocumented code increases developer onboarding latency, leads to misused APIs, and forces engineers to decipher implementation details to understand intent. Conversely, misleading or outdated comments introduce dangerous misunderstandings during refactoring.
- **Mental Model**: In Python, **comments are for developers reading the implementation** (explaining *why*, not *what*). **Docstrings are for consumers of the API** (explaining *how to use* classes, functions, and modules). Unlike comments, docstrings are preserved at runtime and accessible via the `__doc__` attribute.
- **When to Use**:
  - Comments (`#`): Clarifying non-obvious algorithms, documenting workarounds for upstream bugs, marking `# TODO:` items.
  - Docstrings (`"""..."""`): Every public module, function, class, and method.
- **When NOT to Use**: Never write redundant comments that merely restate what the code clearly expresses (e.g., `# increment i by 1 \n i += 1`).

---

## 2. Core Concepts
1. **Comments (`#`)**: Ignored by the parser and tokenizer. They do not exist in bytecode, have zero runtime memory presence, and cannot be introspected.
2. **Docstrings (PEP 257)**: A string literal that occurs as the very first statement in a module, function, class, or method definition. CPython compiles docstrings into the object's `__doc__` attribute.
3. **Docstring Format Standards**:
   - *Google Style*: Clean, human-readable sections (`Args:`, `Returns:`, `Raises:`). Widely favored in modern enterprise Python.
   - *NumPy Style*: Detailed parameter blocks with dashed underlines, favored in scientific packages (NumPy, SciPy).
   - *Sphinx / reStructuredText (reST)*: Direct `:param name: description` syntax for Sphinx doc builds.
4. **Interactive `help()`**: The built-in `help(obj)` function inspects `obj.__doc__` and formats its signatures for terminal reading.
5. **Executable Docstrings (`doctest`)**: Standard library tool that searches docstrings for interactive REPL prompts (`>>> `), executes the code, and verifies that the output matches.

---

## 3. How It Works Internally

How CPython handles docstrings during bytecode compilation:

```text
Source Code:
def add(a, b):
    """Adds two integers."""
    return a + b
            │
            ▼
[ CPython Compiler ]
            ├─► Detects string literal as FIRST statement in function body
            ├─► Binds string literal to `co_consts[0]`
            │
            ▼
Creates `PyFunctionObject`
            ├─► Sets `func.__doc__ = "Adds two integers."`
            │
            ▼ (Optimization Flag: python -OO)
Strips all docstrings! (co_consts[0] becomes None to save memory)
```

---

## 4. Syntax & API Reference

### Google Style Docstring Format
```python
def query_user(user_id: str, timeout: float = 5.0) -> dict[str, object]:
    """Queries user record from the database.

    Args:
        user_id: Unique string identifier of the user account.
        timeout: Maximum seconds to wait before raising TimeoutError.

    Returns:
        A dictionary containing user profile records.

    Raises:
        UserNotFoundError: If no account matches user_id.
        TimeoutError: If the database does not respond within timeout.
    """
    ...
```

### Accessing Docstrings Programmatically
```python
import inspect

func.__doc__                       # Raw docstring string (or None)
inspect.getdoc(func)              # Cleaned docstring with uniform indentation stripped
inspect.cleandoc(multiline_str)   # Strips common leading whitespace from docstring lines
```

---

## 5. Code Examples

### 1. Minimal / Basic Example: Introspecting Function Docstrings
```python
def compute_tax(subtotal: float) -> float:
    """Calculates sales tax for an invoice subtotal.

    Args:
        subtotal: The pre-tax monetary amount.

    Returns:
        The calculated tax amount.
    """
    return subtotal * 0.0825

print(f"Docstring exists: {bool(compute_tax.__doc__)}")
print(f"First line:       {compute_tax.__doc__.strip().splitlines()[0]}")
```
**Output**:
```text
Docstring exists: True
First line:       Calculates sales tax for an invoice subtotal.
```

---

### 2. Real-World Production Example: Validating Executable Documentation with `doctest`
```python
import doctest

def calculate_discount(price: float, discount_percent: float) -> float:
    """Calculates the discounted price.

    >>> calculate_discount(100.0, 20.0)
    80.0
    >>> calculate_discount(50.0, 0.0)
    50.0
    >>> calculate_discount(10.0, 100.0)
    0.0
    """
    return round(price * (1.0 - (discount_percent / 100.0)), 2)

if __name__ == "__main__":
    # Run embedded doctests
    failures, total = doctest.testmod()
    print(f"Doctests run: {total}, Failures: {failures}")
```
**Output**:
```text
Doctests run: 3, Failures: 0
```

---

### 3. Edge Case: Cleaning Indentation via `inspect.cleandoc`
```python
import inspect

def raw_docstring_sample():
    """First summary line.

    A paragraph with indentation
    that spans multiple lines.
    """
    pass

raw_text = raw_docstring_sample.__doc__
cleaned_text = inspect.cleandoc(raw_text or "")

print("Raw docstring representation:")
print(repr(raw_text))
print("\nCleaned docstring:")
print(cleaned_text)
```
**Output**:
```text
Raw docstring representation:
'First summary line.\n\n    A paragraph with indentation\n    that spans multiple lines.\n    '

Cleaned docstring:
First summary line.

A paragraph with indentation
that spans multiple lines.
```

---

### 4. Failure Case: Running Under Python `-OO` Optimization
```python
import sys

def important_service():
    """Service metadata critical for introspection."""
    return "Running"

# Simulating what happens under 'python -OO':
if "-OO" in sys.argv:
    assert important_service.__doc__ is None
else:
    print("Normal mode: docstring exists ->", bool(important_service.__doc__))
```
**Output**:
```text
Normal mode: docstring exists -> True
```
*Architecture Warning*: Never write business logic that depends on `__doc__` being non-None (e.g. using docstrings for runtime routing), as running under `python -OO` permanently strips all docstrings to `None`!

---

### 5. Idiomatic Pythonic Pattern: Module-Level Docstring
```python
"""Authentication and Session Management Module.

This module provides cryptographically secure JWT issuance, session validation,
and token refresh handlers for the API gateway.

Typical usage example:
    from auth import issue_token, verify_token
    token = issue_token(user_id="usr_123")
    claims = verify_token(token)
"""

print("Module docstring registered successfully.")
```
**Output**:
```text
Module docstring registered successfully.
```

---

## 6. Edge Cases & Pitfalls
1. **Docstring Placed After Expressions**: If an assignment or statement precedes the triple-quoted string, CPython treats it as an unused multiline string literal, NOT a docstring (`__doc__` will be `None`).
2. **Stripping Under `-OO` Flag**: Running `python -OO` deletes all docstrings from compiled code objects (`co_consts`). Libraries (like early click or docopt) that parsed CLI help from `__doc__` would crash under `-OO`.
3. **Leading / Trailing Blank Lines**: PEP 257 mandates no blank lines before or after single-line docstrings, and a single blank line separating the summary from subsequent paragraphs in multiline docstrings.

---

## 7. Common Mistakes

| Mistake | Why It Fails | Exception / Diagnostic | Correct Pattern |
|---|---|---|---|
| Statement placed before docstring | String is no longer first statement; `__doc__` remains `None` | `__doc__ is None` | Place docstring as the absolute first line |
| Writing comments that restate the code | Adds visual noise without providing context | Maintenance burden | Comment *why*, not *what* |
| Relying on `__doc__` in application logic | Breaks when users execute with `python -OO` | `AttributeError: 'NoneType' object has no attribute...` | Store metadata in class attributes or decorators |
| Inconsistent docstring styles across modules | Harder for team to read and breaks doc generators | Inconsistent Sphinx rendering | Standardize on Google or NumPy style |
| Failing to update docstrings when refactoring arguments | Code and documentation drift out of sync | Misleads API consumers | Use Ruff / flake8-docstrings |
| Using `#` comments for module documentation | Not accessible via `help()` or Sphinx | Undocumented module in documentation builds | Use module-level `"""..."""` |

---

## 8. Performance & Complexity
- **Docstring Memory Overhead**: Each docstring is stored as an immutable `str` in the code object's `co_consts` tuple.
- **Stripping Overhead**: Passing `python -OO` strips docstrings, reducing memory footprint by ~5–10% in large codebases.

---

## 9. CPython Internals & Bytecode: How Docstrings are Attached
In CPython's compiler (`Python/compile.c`):
- When compiling an AST `FunctionDef`, the compiler inspects `body[0]`.
- If `body[0]` is an `ast.Expr` whose value is an `ast.Constant` of type string:
  - It assigns `func->func_doc = const_value`.
  - It removes the expression from the executable bytecode instruction sequence, leaving only the reference in `co_consts`.

---

## 10. Best Practices
1. Write a docstring for every public module, class, method, and function.
2. Standardize on **Google Style** docstrings for readability and clean IDE tooltips.
3. Keep comments concise, explaining the *rationale* behind non-obvious design decisions.
4. Use `doctest` to guarantee that code examples in docstrings stay accurate and working over time.

---

## 11. Real-World Case Study: Automated Test Coverage of Documentation via Doctest
An open-source mathematical library noticed that user documentation examples were frequently breaking after minor API refactors:
1. The maintainers embedded representative usage examples directly into function docstrings using standard `>>> ` doctest syntax.
2. They added `pytest --doctest-modules` to their CI test suite.
3. Any documentation example with an outdated parameter or incorrect return value immediately triggered CI failure, ensuring 100% accuracy of public documentation.

---

## 12. Interview Questions

### Q1: What is the technical difference between a comment and a docstring?
<details>
<summary><strong>Answer</strong></summary>
A comment (prefixed with `#`) is handled purely during tokenization and discarded before the Abstract Syntax Tree (AST) is generated; comments do not exist in compiled bytecode and cannot be accessed at runtime. A docstring is a string literal placed as the first statement of a module, class, or function; CPython compiles it into the object's `__doc__` attribute on heap memory, making it accessible at runtime via `obj.__doc__` and `help()`.
</details>

### Q2: What does PEP 257 specify?
<details>
<summary><strong>Answer</strong></summary>
PEP 257 defines docstring conventions for Python:
1. Multi-line docstrings should begin with a one-line summary ending with a period, followed by a blank line, followed by detailed descriptions.
2. Triple double quotes `"""` should always be used.
3. The closing quotes should be on their own line for multiline docstrings.
4. Single-line docstrings should have quotes on the same line (`"""Summary."""`).
</details>

### Q3: What happens to docstrings when Python is executed with the `-OO` flag?
<details>
<summary><strong>Answer</strong></summary>
Running Python with `-OO` activates maximum bytecode optimization:
1. It discards `assert` statements (like `-O`).
2. It strips all docstrings from code objects (`co_consts`). As a result, `__doc__` evaluates to `None` for all functions, classes, and modules, saving memory.
</details>

### Q4: How does `inspect.cleandoc()` work?
<details>
<summary><strong>Answer</strong></summary>
Docstrings often have uniform indentation inherited from the class or function block they reside in. `inspect.cleandoc(doc)` removes the leading whitespace shared by all lines (excluding the first line if it starts immediately after quotes) and strips leading and trailing empty lines, producing a clean, left-aligned string for formatting.
</details>

### Q5: What is `doctest` and how does it execute?
<details>
<summary><strong>Answer</strong></summary>
`doctest` is a standard library module that scans docstrings for text formatted like an interactive Python REPL session (lines beginning with `>>> ` and `... `). It parses the code, evaluates it in an isolated namespace, captures stdout, and compares the actual output against the expected text in the docstring, reporting test failures on discrepancies.
</details>

### Q6: What are the three primary docstring formats used in Python?
<details>
<summary><strong>Answer</strong></summary>
1. **Google Style**: Uses simple indentation and capitalized section headers (`Args:`, `Returns:`, `Raises:`).
2. **NumPy Style**: Uses underline headers with dashes (`Parameters\n----------`), popular in data science.
3. **Sphinx / reStructuredText**: Uses inline tags (`:param x: description`, `:return: description`), historically used by Sphinx doc tools.
</details>

### Q7: Why should comments explain "why" rather than "what"?
<details>
<summary><strong>Answer</strong></summary>
Python code is designed to be expressive and self-describing; reading the syntax explains *what* the code does. Comments should explain the business reason, edge cases, external constraints, or mathematical proofs that necessitated that specific implementation (*why*), preventing future maintainers from inadvertently breaking subtle assumptions.
</details>

### Q8: What is a module-level docstring and where must it reside?
<details>
<summary><strong>Answer</strong></summary>
A module-level docstring is a string literal placed at the absolute top of a `.py` file before any executable statements or imports (only shebang lines or encoding cookies may precede it). It documents the overall purpose of the module, exposed classes, and high-level architectural usage.
</details>

---

## 13. Exercises & Challenges

### Challenge 1 (Easy): Print Function Documentation
Write a function `display_help(fn)` that prints the name and first sentence of a function's docstring, or `"No documentation"` if missing.
<details>
<summary><strong>Solution</strong></summary>

```python
from collections.abc import Callable
from typing import Any

def display_help(fn: Callable[..., Any]) -> None:
    doc = fn.__doc__
    if not doc:
        print(f"{fn.__name__}: No documentation")
        return
    first_line = doc.strip().splitlines()[0]
    print(f"{fn.__name__}: {first_line}")

def sample_task() -> None:
    """Performs background telemetry synchronization."""
    pass

display_help(sample_task)
```
</details>

### Challenge 2 (Easy): Multi-Line Google-Style Docstring Authoring
Write a function `calculate_bmi(weight_kg, height_m)` complete with a verified Google-style docstring detailing `Args:`, `Returns:`, and `Raises:`.
<details>
<summary><strong>Solution</strong></summary>

```python
def calculate_bmi(weight_kg: float, height_m: float) -> float:
    """Calculates Body Mass Index (BMI).

    Args:
        weight_kg: Subject weight in kilograms. Must be positive.
        height_m: Subject height in meters. Must be positive.

    Returns:
        The calculated BMI score rounded to two decimal places.

    Raises:
        ValueError: If weight_kg or height_m is non-positive.
    """
    if weight_kg <= 0 or height_m <= 0:
        raise ValueError("Weight and height must be strictly positive.")
    return round(weight_kg / (height_m ** 2), 2)

print("BMI:", calculate_bmi(70, 1.75))
```
</details>

### Challenge 3 (Medium): Embedded Doctest Validator
Write a function `reverse_words(sentence)` with embedded doctests for single-word, multi-word, and empty string cases, and run `doctest.run_docstring_examples`.
<details>
<summary><strong>Solution</strong></summary>

```python
import doctest

def reverse_words(sentence: str) -> str:
    """Reverses the order of words in a space-delimited string.

    >>> reverse_words("hello world")
    'world hello'
    >>> reverse_words("python")
    'python'
    >>> reverse_words("")
    ''
    """
    words = sentence.split()
    return " ".join(reversed(words))

doctest.run_docstring_examples(reverse_words, globals())
print("Doctest verified.")
```
</details>

### Challenge 4 (Medium): Docstring Coverage Linter
Write an AST analysis tool `check_docstring_coverage(source_code)` that returns the percentage of functions in a source string that possess a docstring.
<details>
<summary><strong>Solution</strong></summary>

```python
import ast

def check_docstring_coverage(source_code: str) -> float:
    tree = ast.parse(source_code)
    funcs = [n for n in ast.walk(tree) if isinstance(n, ast.FunctionDef)]
    if not funcs:
        return 100.0

    documented = sum(1 for f in funcs if ast.get_docstring(f) is not None)
    return round((documented / len(funcs)) * 100.0, 1)

code_sample = """
def documented_fn():
    '''Has docstring.'''
    pass

def undocumented_fn():
    pass
"""
print("Docstring coverage:", check_docstring_coverage(code_sample), "%")
```
</details>

### Challenge 5 (Hard): Google-to-Sphinx Docstring Converter
Write a function that parses a Google-style `Args:` block from a docstring and converts it into Sphinx `:param name: description` syntax.
<details>
<summary><strong>Solution</strong></summary>

```python
import re

def convert_google_args_to_sphinx(doc: str) -> str:
    lines = doc.splitlines()
    output = []
    in_args = False

    for line in lines:
        stripped = line.strip()
        if stripped == "Args:":
            in_args = True
            continue
        elif in_args and (stripped.endswith(":") or not line.startswith("    ")):
            in_args = False

        if in_args and ":" in stripped:
            param, desc = stripped.split(":", 1)
            output.append(f":param {param.strip()}: {desc.strip()}")
        else:
            output.append(line)

    return "\n".join(output)

google_doc = """Summary line.

Args:
    user_id: The unique ID.
    timeout: Request timeout.

Returns:
    Result object.
"""
print(convert_google_args_to_sphinx(google_doc))
```
</details>

### Challenge 6 (Hard): Automated AST Docstring Validator for Deprecated APIs
Write an AST analyzer that scans functions decorated with `@deprecated` and asserts that their docstrings contain the warning phrase `"DEPRECATED"`.
<details>
<summary><strong>Solution</strong></summary>

```python
import ast

def assert_deprecated_docstrings(source: str) -> list[str]:
    tree = ast.parse(source)
    missing_warnings = []
    for node in ast.walk(tree):
        if isinstance(node, ast.FunctionDef):
            has_dep_decorator = any(
                isinstance(d, ast.Name) and d.id == "deprecated" for d in node.decorator_list
            )
            if has_dep_decorator:
                doc = ast.get_docstring(node)
                if not doc or "DEPRECATED" not in doc:
                    missing_warnings.append(node.name)
    return missing_warnings

sample = """
@deprecated
def old_function():
    '''Regular docstring.'''
    pass
"""
print("Functions missing deprecation notices:", assert_deprecated_docstrings(sample))
```
</details>
