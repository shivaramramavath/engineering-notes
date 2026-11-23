# Module 08: Modern JavaScript & ECMAScript Evolution

> Authoritative curriculum and deep-dive technical notes on ECMAScript modern features, metaprogramming protocols, symbols, iterators, generators, and the annual TC39 standard evolution from ES2015 to ES2025+.

## Module Overview

Modern JavaScript (post-ES2015) has shifted from a fragmented, loosely specified scripting language to a robust, standardized language released on a predictable annual cycle by TC39. This module covers core language enhancements, metaprogramming extension hooks, iterable protocols, suspendable execution frames, and feature evolution:

1. **Template Literals & Tagged Templates:** Interpolation grammar, escaping raw strings, and authoring domain-specific languages (DSLs) for secure HTML/SQL sanitization.
2. **Enhanced Object Literals:** Method shorthands, dynamic computed property keys, prototype binding via `super`, and object spread semantics.
3. **Symbols & Well-Known Hooks:** Primitive uniqueness, the Global Symbol Registry, and tapping into core engine protocols (`Symbol.iterator`, `Symbol.toPrimitive`, `Symbol.hasInstance`).
4. **Iterators & Iterables:** The ECMAScript Iteration Protocol, custom sequences, lazy consumption, and modern ES2024 Iterator Helpers.
5. **Generators & Coroutines:** Suspendable heap execution frames (`function*`), bidirectional message passing (`yield`), and cooperative multitasking patterns.
6. **ECMAScript Evolution (ES2015–ES2025+):** TC39 5-stage proposal pipeline, comprehensive edition-by-edition feature breakdowns, and legacy codebase migration strategies.

---

## Curriculum & Reading Order

| Note | Title | Level | Reading Time | Topics Covered |
| :--- | :--- | :--- | :--- | :--- |
| **[01](01_template-literals.md)** | [Template Literals & Tagged Templates](01_template-literals.md) | Core | 21 min | Interpolation, multiline formatting, tagged templates, `String.raw`, AST sanitization |
| **[02](02_enhanced-object-literals.md)** | [Enhanced Object Literals](02_enhanced-object-literals.md) | Core | 22 min | Shorthands, computed property keys, `[[HomeObject]]`, `super`, prototype manipulation |
| **[03](03_symbols.md)** | [Symbols & Metaprogramming](03_symbols.md) | Advanced | 25 min | Primitives, Global Symbol Registry, well-known symbols, V8 `Name` representation |
| **[04](04_iterators-and-iterables.md)** | [Iterators & Iterables](04_iterators-and-iterables.md) | Intermediate | 24 min | Iteration protocols, `[Symbol.iterator]`, lazy sequences, ES2024 Iterator Helpers |
| **[05](05_generators.md)** | [Generators & State Machines](05_generators.md) | Advanced | 26 min | `function*`, `yield`, bidirectional communication, coroutines, V8 heap frames |
| **[06](06_es-features-by-version.md)** | [ECMAScript Evolution Roadmap](06_es-features-by-version.md) | Comprehensive | 28 min | TC39 stages 0–4, year-by-year feature matrix (ES2015–ES2025+), engine flags |
| **[07](27_cheatsheets/07_modern-syntax.md)** | [Modern Syntax Cheatsheet](../27_cheatsheets/07_modern-syntax.md) | Quick Ref | 10 min | ES5 vs ES6+ syntax migration reference (moved to Track Cheatsheets) |

---

## Cross-Module References

- **Variable Declarations (`let`/`const`):** Covered in [01_fundamentals/01_variables-and-constants.md](../01_fundamentals/01_variables-and-constants.md).
- **Arrow Functions & Parameter Rest/Spread:** Covered in [02_functions/02_arrow-functions.md](../02_functions/02_arrow-functions.md) & [02_functions/03_parameters-and-arguments.md](../02_functions/03_parameters-and-arguments.md).
- **Classes & Private Identifiers (`#`):** Covered in [05_this-and-oop/05_classes.md](../05_this-and-oop/05_classes.md) & [05_this-and-oop/07_private-fields.md](../05_this-and-oop/07_private-fields.md).
- **Asynchronous Flow (Promises, `async`/`await`):** Covered in [11_asynchronous-javascript/](../11_asynchronous-javascript/).
- **ES Modules (`import`/`export`):** Covered in [13_modules/](../13_modules/).
