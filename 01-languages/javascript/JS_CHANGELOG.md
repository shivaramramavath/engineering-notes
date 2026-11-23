# JavaScript Engineering Curriculum Changelog

> Detailed chronological record of all structural modifications, corrections, note expansions, and audit remediations across the JavaScript curriculum.

---

## 2026-11-23

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
