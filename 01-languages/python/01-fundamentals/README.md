# Module 01: Python Fundamentals & Syntax

## Overview

Module 01 establishes the foundational syntax, execution rules, and core semantics of the Python language. Rather than treating Python as a simple scripting language, this module explores Python's object model, lexical structure, execution mechanics, and numeric internals to provide engineering-grade mastery.

Every Python variable is a pointer to an object on the heap, statements are distinguished from expressions, and code structures—from basic loops to structural pattern matching—are analyzed down to their bytecode opcodes and architectural trade-offs.

---

## Complete Module Syllabus

| # | Note Title | Slug | Key Topics Covered | Status |
| :-: | :--- | :--- | :--- | :-: |
| 00 | **Syntax and Indentation** | `00-syntax-and-indentation.md` | Off-side rule, 4-space convention, whitespace tokens `INDENT`/`DEDENT`, line joining, `pass` vs `...` | Verified |
| 01 | **Variables, Names, and Objects** | `01-variables-names-and-objects.md` | Name binding, `PyObject` structure (`ob_refcnt`, `ob_type`), mutability, identity (`is`) vs equality (`==`), garbage collection | Verified |
| 02 | **Numbers (int, float, Decimal, Fraction)** | `02-numbers-int-float-decimal-fraction.md` | Arbitrary-precision `int`, IEEE 754 `float`, fixed-point `decimal.Decimal`, rational `fractions.Fraction`, division semantics | Verified |
| 03 | **Strings Introduction** | `03-strings-intro.md` | String immutability, UTF-8/Unicode, triple-quoted multi-line strings, escape sequences, raw strings `r""`, basic slicing | Verified |
| 04 | **Booleans, Truthiness, and None** | `04-booleans-truthiness-and-none.md` | `bool` as subclass of `int`, truth-value testing protocol (`__bool__`, `__len__`), `NoneType` singleton pattern, sentinel design | Verified |
| 05 | **Operators and Expressions** | `05-operators.md` | Arithmetic, bitwise, comparison chaining (`1 < x < 10`), logical short-circuiting, operator precedence | Verified |
| 06 | **Type Conversion (Casting)** | `06-type-conversion.md` | Explicit vs implicit conversion, duck-typing conversion dunders (`__int__`, `__float__`, `__str__`, `__repr__`), lossy casts | Verified |
| 07 | **Input and Output** | `07-input-and-output.md` | `print()` parameters (`sep`, `end`, `file`, `flush`), `input()`, `sys.stdin`/`sys.stdout` buffering, terminal ANSI codes | Verified |
| 08 | **Comments, Docstrings, and Annotations** | `08-comments-and-docstrings.md` | PEP 257 docstring conventions (Google/NumPy/Sphinx), `help()`, `__doc__`, PEP 649/749 deferred annotations | Verified |
| 09 | **Conditionals and Branching** | `09-conditionals.md` | `if/elif/else` flow, ternary operator (`X if C else Y`), short-circuit execution, branch optimization, guard clauses | Verified |
| 10 | **Loops (for, while, range, enumerate, zip)** | `10-loops-range-enumerate-zip.md` | Iteration protocol, `range` memory efficiency, `enumerate()`, `zip(..., strict=True)`, loop `else` clause | Verified |
| 11 | **Structural Pattern Matching** | `11-structural-pattern-matching.md` | PEP 634 `match/case`, sequence/mapping/class patterns, wildcards `_`, capture variables, guard clauses `if`, AS patterns | Verified |
| 12 | **The Walrus Operator** | `12-walrus-operator.md` | PEP 572 assignment expressions `:=`, stream chunking, regex extraction ladders, comprehension scope leakage | Verified |
| 13 | **Integer and Float Internals** | `13-integer-and-float-internals.md` | CPython `PyLongObject` 30-bit digits, Karatsuba multiplication, `PyFloatObject` IEEE 754 Binary64, small int cache, string DoS limits | Verified |

---

## Learning Outcomes

After completing Module 01, you will be able to:
1. Explain how CPython parses whitespace and tokenizes code into `INDENT` and `DEDENT` tokens.
2. Diagram the memory layout of a `PyObject` on the heap and distinguish pointer assignment from in-place mutation.
3. Choose the mathematically appropriate numeric type (`int`, `float`, `Decimal`, `Fraction`) for scientific vs financial workloads.
4. Exploit short-circuiting and comparison chaining for concise, performant boolean evaluations.
5. Apply structural pattern matching (`match/case`) to parse deeply nested JSON-like and AST-like structures safely without capture bugs.
6. Use the walrus operator (`:=`) to write clean single-pass loops and comprehension filters without redundant function calls.
7. Understand CPython's arbitrary-precision bignum architecture and avoid string conversion denial-of-service vulnerabilities.

---

## Prerequisites & Cross-Links

- **Prerequisites:** Module 00 (`00-setup`) for virtual environments, CPython execution pipeline, and code formatting tooling.
- **Next Up:** [Module 02: Data Structures](../02-data-structures/README.md) for lists, tuples, dicts, sets, comprehensions, and memory complexity analysis.
- **Downstream Foundations:**
  - FastAPI Track (`03-backend/fastapi`): Builds on type annotations (Note 08), truthiness (Note 04), and pattern matching (Note 11).
  - Machine Learning Track (`05-ai/machine-learning`): Builds on float IEEE 754 precision limits and numeric types (Notes 02 & 13).
