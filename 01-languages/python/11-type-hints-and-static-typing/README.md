# Module 11: Type Hints & Static Typing (`11-type-hints-and-static-typing`)

> **Mastering Python's static type system from foundations to advanced type-theoretic concepts: PEP 484, PEP 695 type parameter syntax, structural protocols (PEP 544), higher-order callables with ParamSpec (PEP 612), runtime metadata introspection with Annotated (PEP 593), sound type narrowing with TypeIs (PEP 742), Mypy & Pyright static analysis engines, PEP 561 packaging with `py.typed`, forward references and deferred annotations (PEP 649), subtyping variance, and gradual migration blueprints.**

---

## 📋 Module Overview

| Metric | Details |
|---|---|
| **Directory** | `01-languages/python/11-type-hints-and-static-typing` |
| **Status** | 100% Complete (13 Notes + Module README) |
| **Total Lines Authored** | ~8,800+ lines of rigorous technical documentation |
| **Target Python Version** | Python 3.12 / 3.13 / 3.14 (`annotationlib`, PEP 695, PEP 742 `TypeIs`) |
| **Previous Module** | [10-standard-library-tour](../10-standard-library-tour/README.md) |
| **Next Module** | [12-concurrency](../12-concurrency/README.md) |

---

## 🎯 Learning Outcomes

By completing this module, you will:
1. **Master Core & Modern Type Syntax**: Write idiomatic annotations using Python 3.10+ pipe unions (`|`), built-in generic collections (`list[str]`), and Python 3.12+ PEP 695 type parameter syntax (`[T]`, `class Box[T]:`, `type Alias[T] = ...`).
2. **Harness Generics & ParamSpec**: Parameterize functions and classes with bounds and constraints; preserve decorator signatures and inject parameters via `ParamSpec` and `Concatenate`.
3. **Decouple Systems via Structural Typing**: Apply `typing.Protocol` (PEP 544) for static duck typing and understand `@runtime_checkable` boundaries vs nominal `abc.ABC`.
4. **Leverage Runtime Reflection & Annotated**: Intercept and introspect metadata using `typing.Annotated`, `get_type_hints(..., include_extras=True)`, `get_origin`, and `get_args` to build frameworks like FastAPI and Pydantic.
5. **Implement Sound Narrowing & Overloads**: Design polymorphic call signatures with `@typing.overload` and build bidirectional type predicates with `TypeIs` (PEP 742).
6. **Configure Production Static Analyzers**: Master Mypy and Pyright `pyproject.toml` configurations, strict mode flags, error code suppressions (`# type: ignore[code]`), and daemon performance (`dmypy`).
7. **Package & Distribute Typed Libraries**: Ship PEP 561 compliant wheels with `py.typed`, author `.pyi` type stubs for C-extensions, and automate parity verification with `stubtest`.
8. **Navigate Deferred Annotations & Circular Imports**: Prevent circular import deadlocks using `if typing.TYPE_CHECKING:` and understand the evolution from PEP 563 stringification to PEP 649 lazy evaluation.
9. **Understand Type System Variance**: Prove why mutable collections must be invariant, apply covariant sequences and contravariant sinks, and leverage PEP 695 automatic variance inference.
10. **Execute Enterprise Migrations**: Apply phased migration roadmaps, generate annotations from runtime test execution via `MonkeyType`, and enforce non-regressive type debt ratchets in CI.

---

## 📑 Syllabus & Notes Directory

All 13 concept notes strictly adhere to the 13-section technical specification (`_templates/concept-template.md`), including Mermaid architecture diagrams, verified progressive code examples, edge cases, 6+ row pitfall tables, CPython internals, production case studies, 8 interview questions with `<details>`, and 6 practical exercises with verified solutions in `<details>`.

| # | Note | Primary Topics Covered | Lines | Difficulty |
|:---:|:---|:---|:---:|:---:|
| 00 | [Type Hints Basics](00-type-hints-basics.md) | PEP 484 foundations, primitive annotations, pipe unions (`\|`), built-in generics (PEP 585), type aliases, static analysis vs runtime execution | 672 | Intermediate |
| 01 | [Typing Module Core](01-typing-module-core.md) | `Union`, `Optional`, `Any` vs `object`, `Literal`, `Final`, `TypedDict`, `NamedTuple`, `NoReturn`, `Never`, `assert_never` | 677 | Intermediate |
| 02 | [Generics, TypeVar, and PEP 695](02-generics-and-typevar-incl-new-syntax.md) | `TypeVar`, bounds, constraints, modern PEP 695 `[T]` type parameters, `type` statements, generic inheritance, PEP 696 defaults | 765 | Advanced |
| 03 | [Protocols & Structural Subtyping](03-protocols-and-structural-typing.md) | Structural duck typing (`typing.Protocol`), nominal vs structural, `@runtime_checkable`, attribute protocols, stdlib protocols | 744 | Advanced |
| 04 | [ParamSpec, Concatenate & Callable](04-paramspec-concatenate-and-callable-typing.md) | `Callable`, `ParamSpec` (PEP 612), `Concatenate`, decorator signature preservation, dependency injection, `TypeVarTuple` (PEP 646) | 703 | Advanced |
| 05 | [Annotated & Runtime Introspection](05-annotated-and-runtime-use-of-hints.md) | `typing.Annotated` (PEP 593), metadata attachment, `get_type_hints`, `get_origin`, `get_args`, FastAPI / Pydantic reflection | 750 | Advanced |
| 06 | [Overloads & TypeGuard / TypeIs](06-overloads-and-typeguard.md) | `@typing.overload` polymorphic dispatch, `TypeGuard` (PEP 647), modern `TypeIs` (PEP 742), bidirectional narrowing, literal tags | 719 | Advanced |
| 07 | [Mypy & Pyright in Practice](07-mypy-and-pyright-in-practice.md) | Architecture comparison, `pyproject.toml` configurations, strict mode flags, error codes, `# type: ignore[code]`, `dmypy`, CI workflows | 567 | Advanced |
| 08 | [Stubs, py.typed, and Typeshed](08-stubs-and-py-typed.md) | PEP 561 distribution, `py.typed` marker file, `.pyi` stub files, typeshed architecture, typing C-extensions, `stubgen` and `stubtest` | 560 | Advanced |
| 09 | [Deferred Annotations & Forward References](09-deferred-annotations-and-forward-references.md) | Forward references, `from __future__ import annotations` (PEP 563), `if TYPE_CHECKING:` circular imports, PEP 649 `annotationlib` | 663 | Advanced |
| 10 | [Typing Best Practices](10-typing-best-practices.md) | Postel's law (abstract inputs, concrete outputs), eliminating `Any`, TypedDict vs Dataclass, `@final`, exhaustive pattern matching | 670 | Advanced |
| 11 | [Variance and Type Narrowing](11-variance-and-type-narrowing.md) | Mathematical subtyping ($A \le B$), invariant mutable containers, covariant sequences, contravariant sinks, PEP 695 auto-variance | 693 | Advanced |
| 12 | [Gradual Typing Migration Strategies](12-gradual-typing-migration.md) | 4-phase migration roadmap, `check_untyped_defs`, `MonkeyType` trace-based generation, CI ratchet gates, type debt tracking | 622 | Advanced |

---

## 🛠️ Verification & Recommended Tooling

To run static type checking against the examples in this module:

```bash
# Verify with Mypy in strict mode
mypy 01-languages/python/11-type-hints-and-static-typing/ --strict

# Verify with Pyright in strict mode
pyright 01-languages/python/11-type-hints-and-static-typing/
```
