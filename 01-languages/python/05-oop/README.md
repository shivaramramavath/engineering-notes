# Module 05: Object-Oriented Programming (OOP)

## Syllabus & Topic Overview

Module 05 delivers comprehensive, production-grade coverage of Python's object-oriented architecture and Data Model. From foundational class construction and descriptor bindings to advanced multiple inheritance (C3 Linearization), dataclass optimizations (slots, kw_only), and weak-reference memory management, this module provides the deep mechanistic insight needed to design robust, clean, and performant object hierarchies.

### Concept Notes Index

1. [00-classes-and-objects.md](file:///E:/Learning/engineering-notes/01-languages/python/05-oop/00-classes-and-objects.md) - Classes, objects, namespaces, type instantiation, and the Python object model.
2. [01-init-new-and-instance-lifecycle.md](file:///E:/Learning/engineering-notes/01-languages/python/05-oop/01-init-new-and-instance-lifecycle.md) - `__new__` vs `__init__`, immutable subclassing, and singleton pattern mechanics.
3. [02-instance-vs-class-attributes.md](file:///E:/Learning/engineering-notes/01-languages/python/05-oop/02-instance-vs-class-attributes.md) - Instance dictionary vs class mapping proxy, attribute shadowing, and CPython split tables.
4. [03-methods-classmethod-staticmethod.md](file:///E:/Learning/engineering-notes/01-languages/python/05-oop/03-methods-classmethod-staticmethod.md) - Method descriptor binding, `@classmethod` alternative constructors, and `@staticmethod`.
5. [04-properties.md](file:///E:/Learning/engineering-notes/01-languages/python/05-oop/04-properties.md) - Data descriptors, `@property`, setters, deleters, and lazy `@cached_property`.
6. [05-inheritance-and-mro.md](file:///E:/Learning/engineering-notes/01-languages/python/05-oop/05-inheritance-and-mro.md) - Single/multiple inheritance, diamond problem, and the C3 Linearization algorithm.
7. [06-super.md](file:///E:/Learning/engineering-notes/01-languages/python/05-oop/06-super.md) - The `super()` built-in, compiler `__class__` cell, and cooperative multiple inheritance.
8. [07-polymorphism-and-duck-typing.md](file:///E:/Learning/engineering-notes/01-languages/python/05-oop/07-polymorphism-and-duck-typing.md) - Duck typing, dynamic dispatch, EAFP vs LBYL, and structural polymorphism.
9. [08-abstract-base-classes-and-protocols.md](file:///E:/Learning/engineering-notes/01-languages/python/05-oop/08-abstract-base-classes-and-protocols.md) - Nominal subtyping (`abc.ABC`) vs structural subtyping (`typing.Protocol`).
10. [09-dunder-methods-overview.md](file:///E:/Learning/engineering-notes/01-languages/python/05-oop/09-dunder-methods-overview.md) - Python Data Model dunders (`__repr__`, `__str__`, `__len__`, `__getitem__`, `__enter__`).
11. [10-operator-overloading.md](file:///E:/Learning/engineering-notes/01-languages/python/05-oop/10-operator-overloading.md) - Arithmetic dunders, reflected dunders (`__radd__`), in-place mutation, and `NotImplemented`.
12. [11-dataclasses.md](file:///E:/Learning/engineering-notes/01-languages/python/05-oop/11-dataclasses.md) - PEP 557 `@dataclass`, `slots=True`, `kw_only=True`, `frozen=True`, and `__post_init__`.
13. [12-enums.md](file:///E:/Learning/engineering-notes/01-languages/python/05-oop/12-enums.md) - `enum.Enum`, `IntEnum`, modern `StrEnum`, `Flag`, and bitwise flags.
14. [13-slots.md](file:///E:/Learning/engineering-notes/01-languages/python/05-oop/13-slots.md) - `__slots__` memory optimization, eliminating `__dict__`, and inheritance layout rules.
15. [14-composition-vs-inheritance.md](file:///E:/Learning/engineering-notes/01-languages/python/05-oop/14-composition-vs-inheritance.md) - "Favor composition over inheritance", delegation patterns, and `__getattr__` forwarding.
16. [15-mixins.md](file:///E:/Learning/engineering-notes/01-languages/python/05-oop/15-mixins.md) - Mixin architecture, cooperative MRO chains, slotted mixins, and ordering rules.
17. [16-weak-references.md](file:///E:/Learning/engineering-notes/01-languages/python/05-oop/16-weak-references.md) - `weakref` module, `WeakValueDictionary`, breaking reference cycles, and `weakref.finalize`.

---

## Learning Outcomes & Core Takeaways

- **Dunder Execution Mechanics**: Understand how CPython's type slots (`tp_repr`, `tp_str`, `tp_call`) bypass `__dict__` for special methods, yielding high performance.
- **Inheritance vs Composition**: Know when to use subclassing (strict Liskov substitution) versus object composition and delegation (`__getattr__`).
- **Cooperative Multiple Inheritance**: Safely design diamond hierarchies and mixin chains using `super().method(*args, **kwargs)`.
- **Memory Optimization**: Cut object memory overhead by up to 75% using slotted classes (`__slots__`) and slotted dataclasses (`@dataclass(slots=True)`).
- **Modern Typing & Contracts**: Choose between nominal interface enforcement (`abc.ABC`) and structural duck-typing contracts (`typing.Protocol`).
