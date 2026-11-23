# JavaScript Engineering Curriculum Changelog

> Detailed chronological record of all structural modifications, corrections, note expansions, and audit remediations across the JavaScript curriculum.

---

## 2026-11-23

### Batch 4: Scopes, OOP, Closures & Functional Programming (Modules 04, 05, 06, 07)
- **Module 04 (`04_scope-and-execution`):**
  - Expanded `01_scope.md` (490 lines) covering the 4 scopes (Global, Module, Function, Block), lexical vs dynamic scope, identifier resolution algorithm, and multi-tenant sandboxing.
  - Expanded `02_lexical-environment.md` (515 lines) covering ECMAScript §9.1 Lexical Environments, Declarative vs Object vs Module Environment Records, `[[OuterEnv]]` pointers, and V8 heap `Context` allocation.
  - Expanded `03_execution-context-and-call-stack.md` (510 lines) covering Creation Phase vs Execution Phase, LIFO Call Stack mechanics, VariableEnvironment vs LexicalEnvironment, stack overflows, and stack trace sanitization.
  - Expanded `04_hoisting-and-tdz.md` (520 lines) covering declaration hoisting, Temporal Dead Zone boundaries, `typeof` TDZ semantics, parameter TDZ, and V8 `the_hole` internal sentinel checks.
  - Expanded `05_strict-mode.md` (495 lines) covering `"use strict"`, automatic strict mode in ES modules and classes, silent failures converted to exceptions, `eval` scope isolation, and undefined `this`.
  - Updated `04_scope-and-execution/README.md` with complete curriculum roadmap and reading guide.
- **Module 05 (`05_this-and-oop`):**
  - Expanded `01_this.md` (530 lines) covering the 4 binding rules (new, explicit, implicit, default), call-site precedence, strict mode differences, lexical arrow `this`, and detached method bugs.
  - Expanded `02_call-apply-bind.md` (510 lines) covering `call`, `apply`, `bind`, method borrowing patterns, partial application, polyfill implementations, and V8 `JSBoundFunction` exotic objects.
  - Expanded `03_prototypes-and-prototype-chain.md` (525 lines) covering prototypal delegation, `[[Prototype]]` vs `prototype` vs `__proto__`, `Object.create()`, property shadowing, and Prototype Pollution security defenses.
  - Expanded `04_constructor-functions.md` (515 lines) covering pre-ES6 constructor mechanics, 5-step `new` operator execution algorithm, return overrides, `new.target` meta-property, and prototype wiring.
  - Expanded `05_classes.md` (535 lines) covering ES6 class declarations vs expressions, constructor desugaring, non-enumerable prototype methods, public class fields, and ES2022 static initialization blocks.
  - Expanded `06_inheritance.md` (520 lines) covering `extends`, mandatory derived constructor `super()` requirement, method delegation via `[[HomeObject]]`, dual prototype chains, and subclassing built-ins.
  - Expanded `07_private-fields.md` (510 lines) covering hard privacy via `#field`, private methods, private static members, realm-safe brand checking with `#field in obj`, and V8 `PrivateName` internals.
  - Expanded `08_mixins-and-composition.md` (525 lines) covering composition over inheritance, Fragile Base Class problem, `Object.assign` mixins vs subclass factory mixins, and Entity-Component-System (ECS) architecture.
  - Updated `05_this-and-oop/README.md` with complete curriculum roadmap and reading guide.
- **Module 06 (`06_closures`):**
  - Expanded `01_closures.md` (505 lines) covering lexical closure definition, `[[Environment]]` internal slot, live variable references vs value snapshots, V8 Context allocation, and `var` vs `let` loop mechanics.
  - Expanded `02_closure-use-cases.md` (520 lines) covering the Revealing Module Pattern, private state factories, currying, parametric decorators, memoization caches, and production debounce & throttle engines.
  - Expanded `03_closure-pitfalls.md` (530 lines) covering memory leaks, the Meteor shared-context leak, stale closures in timers and React hooks, Chrome DevTools heap snapshots, and lifecycle disposal.
  - Updated `06_closures/README.md` with complete curriculum roadmap and reading guide.
- **Module 07 (`07_functional-programming`):**
  - Expanded `01_pure-functions-and-side-effects.md` (515 lines) covering the two axioms of purity, referential transparency, substitution model, side effect cataloging, and "Pure Core, Imperative Shell" architecture.
  - Expanded `02_immutability.md` (520 lines) covering primitive immutability vs object mutability, shallow `Object.freeze()` vs recursive `deepFreeze()`, structural sharing, and Immer copy-on-write proxies.
  - Expanded `03_map-filter-reduce.md` (530 lines) covering declarative iteration, catamorphism via `reduce`, intermediate array allocation hazards, $O(n^2)$ accumulator trap, and single-pass Transducers.
  - Expanded `04_composition-and-pipe.md` (510 lines) covering right-to-left `compose` vs left-to-right `pipe`, associativity law, unary pipelines, async pipelines (`pipeAsync`), and debugging taps.
  - Expanded `05_currying-and-partial-application.md` (525 lines) covering mathematical currying vs partial application, auto-currying utilities with placeholders, `fn.length` arity traps, and data-last architecture.
  - Expanded `06_point-free.md` (495 lines) covering tacit programming, Eta reduction ($\eta$-reduction), point-free composition, the multi-arg `parseInt` trap, and readability thresholds.
  - Expanded `07_functional-patterns.md` (545 lines) covering category theory, Functor & Monad laws, the Maybe Monad, Either Monad (Railway-Oriented Programming), IO Monad, and Applicative Validation.
  - Updated `07_functional-programming/README.md` with complete curriculum roadmap and reading guide.

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
