# Module 07: Functional Programming (FP)

> In-depth curriculum guide to functional programming in JavaScript, covering referential transparency, deep immutability, structural sharing, declarative pipelines, currying, point-free combinators, and monadic architectures (Maybe, Either, IO).

## Overview & Curriculum Map

While JavaScript is a multi-paradigm language, its first-class functions, closures, and higher-order sequence methods make functional programming a primary design paradigm. By eliminating shared mutable state and replacing imperative control flow with pure mathematical transformations, functional code achieves exceptional predictability, testability, and concurrency safety.

This module provides exhaustive coverage of functional architecture in JavaScript—progressing from purity foundations and persistent immutability, through composition pipelines and currying, up to algebraic data types (Functors, Monads, and Railway-Oriented Programming).

---

## Complete Note Index

| Order | Note | Level | Topics Covered | Status |
| :---: | :--- | :---: | :--- | :---: |
| 01 | [01_pure-functions-and-side-effects.md](./01_pure-functions-and-side-effects.md) | Core / Inter | The two axioms of purity, referential transparency, substitution model, side effect cataloging, "Pure Core, Imperative Shell" architecture | Complete |
| 02 | [02_immutability.md](./02_immutability.md) | Core / Inter | Primitive immutability vs object mutability, shallow `Object.freeze()` vs recursive `deepFreeze()`, structural sharing, Immer copy-on-write proxies | Complete |
| 03 | [03_map-filter-reduce.md](./03_map-filter-reduce.md) | Core / Inter | Declarative iteration, catamorphism via `reduce`, intermediate array allocation hazards, $O(n^2)$ accumulator trap, single-pass Transducers | Complete |
| 04 | [04_composition-and-pipe.md](./04_composition-and-pipe.md) | Intermediate | Right-to-left `compose` vs left-to-right `pipe`, associativity law, unary pipelines, async pipelines (`pipeAsync`), debugging taps | Complete |
| 05 | [05_currying-and-partial-application.md](./05_currying-and-partial-application.md) | Core / Inter | Mathematical currying vs partial application, auto-currying utilities with placeholders, `fn.length` arity traps, data-last architecture | Complete |
| 06 | [06_point-free.md](./06_point-free.md) | Inter / Adv | Tacit programming, Eta reduction ($\eta$-reduction), point-free composition, the multi-arg `parseInt` trap, readability thresholds | Complete |
| 07 | [07_functional-patterns.md](./07_functional-patterns.md) | Advanced | Category theory, Functor & Monad laws, the Maybe Monad, Either Monad (Railway-Oriented Programming), IO Monad, Applicative Validation | Complete |

---

## Learning Objectives & Mastery Checklist

By completing this module, you will be able to:
- [x] Refactor imperative, side-effect-heavy routines into pure, referentially transparent functions.
- [x] Protect domain entities using deep immutability, explain structural sharing, and implement copy-on-write updates with Immer.
- [x] Implement `map` and `filter` in terms of `reduce`, and design single-pass Transducers to eliminate intermediate array allocations.
- [x] Construct synchronous and asynchronous data pipelines using `pipe()` and `pipeAsync()`.
- [x] Build production-grade auto-currying utilities supporting arity overrides and argument placeholders (`_`).
- [x] Differentiate when Point-Free style enhances readability from when it obscures intent.
- [x] Handle nullability and runtime errors algebraically using the `Maybe` and `Either` monads without throwing exceptions.

---

## Verification & Execution Baseline

All code examples, edge cases, and performance benchmarks in this module are tested and verified against:
- **Node.js**: `v24.11.1` / `v22.x LTS`
- **ECMAScript Standards**: ES1 through ES2024
- **Engine Targets**: Google V8 (Chromium / Node.js), JavaScriptCore (Safari), SpiderMonkey (Firefox)
