# Module: Exceptions, Context Managers & Debugging (`07-exceptions-and-debugging`)

> **Scope**: Exception hierarchies, try/except/else/finally control flow, exception chaining, custom exceptions, PEP 654 exception groups and except*, context managers and ExitStack, assertions, tracebacks and pdb, warnings framework, and production debugging tools.

---

## 📋 Module Overview

| Metric | Details |
|---|---|
| **Directory** | `01-languages/python/07-exceptions-and-debugging` |
| **Status** | 100% Complete (10/10 Notes Authored & Verified) |
| **Previous Module** | [06-modules-and-packages](../06-modules-and-packages/README.md) |
| **Next Module** | [08-iterators-generators-and-functional](../08-iterators-generators-and-functional/README.md) |

---

## 🎯 Learning Outcomes
By completing this module, you will:
1. Understand the core principles, syntax, and runtime behaviors of exceptions, context managers, and debugging tools.
2. Master idiomatic Python patterns while avoiding common failure modes, memory leaks from tracebacks, and assertion vulnerabilities.
3. Apply performance, memory, and security best practices to real-world software engineering, structured concurrency (`except*`), and production telemetry.

---

## 📑 Syllabus & Notes Directory

*All concept notes in this module strictly follow the 13-section standard (`_templates/concept-template.md`).*

| # | Note | Description | Key Focus Areas |
|---|---|---|---|
| **00** | [00-exception-hierarchy.md](00-exception-hierarchy.md) | The CPython Exception Hierarchy | `BaseException` vs `Exception`, `SystemExit`, `KeyboardInterrupt`, zero-cost exception tables (Python 3.11+). |
| **01** | [01-try-except-else-finally.md](01-try-except-else-finally.md) | Deterministic Control Flow | `try/except/else/finally` sequence, `else` isolation, `finally` return hijacking, scope deletion of error variables. |
| **02** | [02-raise-and-exception-chaining.md](02-raise-and-exception-chaining.md) | Exception Chaining & Causes | PEP 3134 explicit chaining (`raise ... from e`), context suppression (`from None`), `__cause__` vs `__context__`. |
| **03** | [03-custom-exceptions.md](03-custom-exceptions.md) | Domain Exception Architecture | Designing package root errors, structured attributes, `self.args` preservation, pickle serialization protocols. |
| **04** | [04-exception-groups-and-except-star.md](04-exception-groups-and-except-star.md) | Exception Groups & Except* | PEP 654 `ExceptionGroup`, concurrent filtering with `except*`, `subgroup()`, `split()`, `asyncio.TaskGroup`. |
| **05** | [05-context-managers-and-with.md](05-context-managers-and-with.md) | Context Management & RAII | `__enter__` and `__exit__`, error suppression via truthy returns, `@contextmanager`, `ExitStack` dynamic resources. |
| **06** | [06-assertions.md](06-assertions.md) | Assertions & Invariants | `assert` syntax, `-O` optimization bytecode stripping, internal invariants vs input validation, tuple pitfall. |
| **07** | [07-tracebacks-and-pdb.md](07-tracebacks-and-pdb.md) | Tracebacks & Interactive Debugging | PEP 657 column locating (`^^^`), `traceback` formatting, traceback frame retention leaks, `breakpoint()`, `pdb`. |
| **08** | [08-warnings.md](08-warnings.md) | The Warnings Framework | `warnings.warn()`, warning categories, `stacklevel=2` caller blaming, filter actions, CI escalation (`-W error`). |
| **09** | [09-debugging-tools.md](09-debugging-tools.md) | Production Diagnostics & Tools | `faulthandler` C-level segfault dumps, `sys.excepthook`, `threading.excepthook`, `sys.monitoring`, `debugpy`. |
