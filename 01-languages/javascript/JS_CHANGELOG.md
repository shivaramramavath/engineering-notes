# JavaScript Engineering Curriculum Changelog

> Detailed chronological record of all structural modifications, corrections, note expansions, and audit remediations across the JavaScript curriculum.

---

## 2026-11-23

### Batch 3: Functions & Objects/Arrays (Modules 02, 03)
- **Module 02 (`02_functions`):**
  - Expanded `01_declarations-and-expressions.md` (520 lines) covering hoisting differences, function name inference (`fn.name`), and V8 compilation pipelines.
  - Expanded `02_arrow-functions.md` (501 lines) covering lexical `this`, absence of `arguments`/`super`/`new.target`, non-constructability, and method pitfalls.
  - Expanded `03_parameters-and-arguments.md` (534 lines) covering default parameters, TDZ in parameter lists, rest parameters (`...args`), and `fn.length` arity calculation rules.
  - Expanded `04_return-values.md` (530 lines) covering ASI return traps, constructor return overrides (primitive vs object returns), and early returns.
  - Expanded `05_callbacks.md` (610 lines) covering callback contracts, inversion of control, error-first conventions in Node.js, and synchronization patterns.
  - Expanded `06_higher-order-functions.md` (562 lines) covering function composition, currying, partial application, transducers, and memoization.
  - Expanded `07_iife-and-function-properties.md` (523 lines) covering modern IIFE use cases (top-level await, scope isolation), `fn.caller`/`fn.arguments` deprecation in strict mode, and custom function metadata.
  - Expanded `08_recursion.md` (541 lines) covering base case guarantees, call stack frame limits, trampolining, and recursion-to-loop transformations.
  - Authored new note `09_function-internals-and-tail-calls.md` (500 lines) covering V8 `JSFunction` heap layout, Context allocation for closures, and Proper Tail Calls (PTC) status.
  - Updated `02_functions/README.md` with complete curriculum roadmap and reading guide.
- **Module 03 (`03_objects-and-arrays`):**
  - Expanded `01_objects.md` (497 lines) covering object literals, computed keys, `delete` performance traps, prototype chain basics, and V8 Hidden Class (Shape) transitions.
  - Expanded `02_property-descriptors.md` (551 lines) covering `Object.defineProperty`, data vs accessor descriptors, `Object.freeze()`, `Object.seal()`, and `Object.preventExtensions()`.
  - Expanded `03_getters-and-setters.md` (597 lines) covering accessor properties, reactive computed state, recursion traps with self-assignment, and prototype inheritance of accessors.
  - Expanded `04_object-static-methods.md` (540 lines) covering `keys`, `values`, `entries`, `assign`, `fromEntries`, `Object.hasOwn()`, and ES2024 `Object.groupBy()` / `Map.groupBy()`.
  - Expanded `05_copying-and-cloning.md` (562 lines) covering reference equality, shallow copy vs deep clone, native `structuredClone()` capabilities/limitations, and JSON serialization loss.
  - Expanded `06_arrays.md` (515 lines) covering dense vs sparse arrays, holes vs undefined, length manipulation, and V8 `PACKED` vs `HOLEY` ElementsKind transitions.
  - Expanded `07_array-methods.md` (530 lines) covering mutating vs pure transformations, ES2023 `toSorted`, `toReversed`, `toSpliced`, `with`, `flatMap`, and `reduce` deep dive.
  - Expanded `08_sorting-and-searching.md` (520 lines) covering V8 Timsort architecture, comparator mathematical contracts, ES2019 stability guarantees, `localeCompare`, and binary search.
  - Expanded `09_destructuring.md` (510 lines) covering positional array destructuring, nominal object destructuring, default value evaluation semantics (`=== undefined`), nested patterns, and aliasing.
  - Expanded `10_spread-and-rest.md` (505 lines) covering rest parameter collection, iterable array spread, ES2018 object spread, shallow copy mechanics, and call stack overflow limits.
  - Expanded `11_optional-chaining-and-nullish.md` (495 lines) covering optional chaining `?.`, nullish coalescing `??`, logical nullish assignment `??=`, and V8 jump bytecode optimization.
  - Authored new note `12_property-enumeration-and-symbols.md` (525 lines) covering ECMAScript §9.1.11 `[[OwnPropertyKeys]]` deterministic 3-phase ordering, Symbol keys, well-known symbols, and V8 Elements vs Properties backing stores.
  - Updated `03_objects-and-arrays/README.md` with complete curriculum roadmap and reading guide.

### Batch 2: Setup & Fundamentals (Modules 00, 01)
- **Module 00 (`00_setup`):**
  - Expanded `01_what-is-javascript.md` to 473 lines covering TC39 stage processes, ES2015-ES2025 evolution, and V8 Ignition/TurboFan execution pipelines.
  - Expanded `02_runtimes-browser-and-node.md` to 441 lines with deep-dives on browser Web APIs vs Node.js C++ bindings, `libuv`, and isomorphic execution via `globalThis`.
  - Expanded `03_npm-and-package-managers.md` to 511 lines with package.json anatomy, semantic versioning algorithms, lockfile mechanics, and pnpm content-addressable storage.
  - Expanded `04_devtools-and-debugging.md` to 492 lines with Chrome DevTools heap snapshot profiling, memory leak detection, conditional breakpoints, and `node --inspect`.
  - Expanded `05_tooling.md` to 621 lines covering ESLint 9+ flat config (`eslint.config.js`), Prettier 3+, Biome Rust linter/formatter, Husky, and `lint-staged`.
  - Authored new note `06_jsdoc-and-type-checking.md` (655 lines) covering zero-build type safety with `checkJs`, `@type`, `@typedef`, `@template`, and `tsc --noEmit`.
  - Updated `00_setup/README.md` with environment verification instructions and module roadmap.
- **Module 01 (`01_fundamentals`):**
  - Expanded `01_variables.md` to 562 lines covering `var`, `let`, `const`, lexical scope, hoisting, and Temporal Dead Zone (TDZ) mechanics.
  - Expanded `02_data-types.md` to 569 lines covering the 7 primitives + Object, auto-boxing wrappers, `Symbol`, `BigInt`, and V8 SMI tagged pointer internals.
  - Expanded `03_type-conversion-and-equality.md` to 550 lines covering ECMAScript abstract operations (`ToPrimitive`, `ToNumber`, `ToBoolean`), loose `==`, strict `===`, `Object.is`, and `SameValueZero`.
  - Expanded `04_operators.md` to 511 lines covering arithmetic, bitwise 32-bit integer casting, short-circuiting, `??`, `?.`, logical assignment, and operator precedence.
  - Expanded `05_expressions-and-statements.md` to 512 lines covering the dual grammar, Automatic Semicolon Insertion (ASI) specification rules, and restricted productions.
  - Expanded `06_conditionals.md` to 563 lines covering `if/else`, strict `switch`, intentional fallthrough, guard clauses, and strategy dispatch maps.
  - Expanded `07_loops.md` to 536 lines covering classic `for`, `while`, `do...while`, `for...of` (Iteration Protocol), `for...in` prototype traversal pitfalls, and labeled loops.
  - Authored new note `08_floating-point-and-number-precision.md` (491 lines) covering the IEEE 754 binary64 double-precision standard, `0.1 + 0.2`, `Number.EPSILON`, safe integer limits, and currency math.
  - Authored new note `09_common-javascript-gotchas.md` (514 lines) cataloging 26 subtle JavaScript quirks with code reproductions, engine explanations, and defensive solutions.
  - Updated `01_fundamentals/README.md` with curriculum roadmap and reading guide.

### Batch 1: Structural Initialization & Standards Setup
- **Baseline Establishment:** Recorded Node.js v24.11.1 / v22 LTS and ECMAScript 2024 / 2025 platform baselines in `JS_FACTS.md`.
- **Authoritative Templates Derived:** Authored `_templates/concept-template.md` (with 13 mandatory sections) and `_templates/project-template.md` tailored for modern JavaScript engineering.
- **Track Audit:** Generated `JS_AUDIT.md` scanning all 190 existing files, identifying 3 stray duplicate READMEs, and identifying missing modules.
- **Architectural Documentation:** Created `JS_COMPLETION_PLAN.md`, `JS_DECISIONS.md`, `JS_PROGRESS.md`, and `JS_CHANGELOG.md`.
- **Duplicate README Reconciled:** Renamed misnamed `README (1).md` in `21_testing`, `22_security`, and `23_real-world-patterns` to `README.md`.
- **Cheatsheet Migration:** Migrated `08_modern-javascript/07_modern-syntax-cheatsheet.md` to `27_cheatsheets/07_modern-syntax.md` and added forwarding pointer.
