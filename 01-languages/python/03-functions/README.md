# Module 03: Functions & Callables

## Overview

Module 03 explores Python's function execution model, parameter passing mechanics, functional programming abstractions, and metaprogramming decorators. Rather than treating functions as simple subroutines, this module dissects functions as first-class heap objects (`PyFunctionObject`), analyzing closure cell lifecycles, lexical scoping rules under the LEGB hierarchy, and bytecode dispatch pipelines.

From the infamous mutable default argument trap and TCO absence to modern features like PEP 570 positional-only parameters (`/`), single-dispatch generic functions (`singledispatch`), and Python 3.14+ deferred annotations (`annotationlib`), this module provides deep technical mastery of Python callables.

---

## Complete Module Syllabus

| # | Note Title | Slug | Key Topics Covered | Status |
| :-: | :--- | :--- | :--- | :-: |
| 00 | **Defining Functions** | `00-defining-functions.md` | First-class citizen model, `def` as runtime statement, `PyFunctionObject` heap anatomy, implicit `None`, dynamic definitions | Verified |
| 01 | **Function Arguments** | `01-arguments-positional-keyword-star.md` | Call-by-sharing semantics, mutability impacts, `*args` tuple packing, `**kwargs` dict packing, canonical signature order | Verified |
| 02 | **Default Argument Pitfalls** | `02-default-argument-pitfalls.md` | The mutable default trap (`[]`, `{}`), definition-time evaluation, `__defaults__` tuple, canonical `None` sentinel idiom | Verified |
| 03 | **Scope (The LEGB Rule)** | `03-scope-legb-global-nonlocal.md` | LEGB hierarchy, compile-time scope tagging, `UnboundLocalError`, `global` vs `nonlocal` (PEP 3104), `LOAD_FAST` vs `LOAD_GLOBAL` | Verified |
| 04 | **Closures and Cell Objects** | `04-closures.md` | Free variables, `__closure__` tuple, `PyCellObject` lifecycle, late-binding loop variable trap and fixes, encapsulation | Verified |
| 05 | **Lambdas and Higher-Order Functions** | `05-lambdas-and-higher-order-functions.md` | Anonymous inline expressions, single-expression rule, higher-order functions (`map`, `filter`, `reduce`), comprehensions vs lambdas | Verified |
| 06 | **Decorators in Python** | `06-decorators.md` | `@` syntactic sugar, wrapper preservation via `@functools.wraps`, parameterized decorator factories, class decorators, stacking order | Verified |
| 07 | **The functools Module** | `07-functools.md` | `@lru_cache` & `@cache`, `partial()`, `@singledispatch` & `@singledispatchmethod`, `@total_ordering`, memory leak mitigation | Verified |
| 08 | **Recursion and Limits** | `08-recursion-and-limits.md` | Call stack growth, `sys.getrecursionlimit()`, why Python rejects Tail Call Optimization (TCO), converting recursion to iterative stacks | Verified |
| 09 | **Docstrings and Annotations** | `09-docstrings-and-annotations.md` | PEP 257 docstrings (Google/Sphinx), `inspect.get_annotations()`, PEP 563 future strings, Python 3.14+ PEP 649/749 `annotationlib` | Verified |
| 10 | **Positional-Only and Keyword-Only** | `10-positional-only-and-keyword-only.md` | PEP 570 forward slash `/`, PEP 3102 asterisk `*`, API decoupling, boolean flag trap elimination, `**kwargs` collision resolution | Verified |

---

## Learning Outcomes

After completing Module 03, you will be able to:
1. Explain Python's call-by-sharing parameter evaluation model and avoid unintentional caller object mutation.
2. Neutralize the mutable default argument trap using defensive `None` sentinels and `_UNSET` tokens.
3. Trace variable resolution across the Local, Enclosing, Global, and Built-in (LEGB) namespaces and debug `UnboundLocalError`.
4. Inspect and manipulate closure cell objects (`__closure__`) and resolve late-binding loop variable bugs.
5. Author production-grade decorators and decorator factories with complete metadata preservation using `@functools.wraps`.
6. Implement polymorphic single-dispatch functions (`singledispatch`) and bounded LRU caches with `functools`.
7. Design stable, self-documenting public APIs using positional-only (`/`) and keyword-only (`*`) parameters.

---

## Prerequisites & Cross-Links

- **Prerequisites:** [Module 02: Data Structures](../02-data-structures/README.md) for lists, dictionaries, tuples, and unpacking.
- **Next Up:** [Module 04: Strings & Text Processing](../04-strings-and-text/README.md) for f-strings, Unicode, regular expressions, and string interning.
- **Downstream Foundations:**
  - FastAPI Track (`03-backend/fastapi`): Relies heavily on decorators (Note 06), type annotations (Note 09), and keyword-only arguments (Note 10).
  - Clean Code Track (`22-design-and-clean-code`): Relies on functional composition, total ordering, and clean API boundaries.
