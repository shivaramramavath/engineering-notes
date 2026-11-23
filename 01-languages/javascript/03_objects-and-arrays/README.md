# Module 03: Objects & Arrays

> Comprehensive curriculum guide covering JavaScript object data modeling, memory layout, property descriptor mechanics, prototype structures, functional array transformations, and deterministic specification ordering.

## Overview & Curriculum Map

In JavaScript, objects and arrays form the bedrock of application state, memory models, and data interchange. Modern JavaScript engines treat these structures with sophisticated optimization pipelines—allocating hidden classes (Shapes) for objects and unboxed backing stores (ElementsKind) for arrays.

This module provides exhaustive coverage of object mutation contracts, immutability patterns, ES2023 non-mutating array pipelines, ES2024 grouped collections, and ECMAScript specification property enumeration rules.

---

## Complete Note Index

| Order | Note | Level | Topics Covered | Status |
| :---: | :--- | :---: | :--- | :---: |
| 01 | [01_objects.md](./01_objects.md) | Core | Object literals, dynamic computed property keys, property deletion, prototype chain basics, V8 Shape transitions | Complete |
| 02 | [02_property-descriptors.md](./02_property-descriptors.md) | Intermediate | `Object.defineProperty`, accessor vs data descriptors, `freeze`, `seal`, `preventExtensions`, integrity verification | Complete |
| 03 | [03_getters-and-setters.md](./03_getters-and-setters.md) | Intermediate | Accessor properties, dynamic computed properties, reactive state traps, prototype inheritance of accessors | Complete |
| 04 | [04_object-static-methods.md](./04_object-static-methods.md) | Intermediate | `Object.keys`, `values`, `entries`, `assign`, `fromEntries`, `Object.hasOwn`, and ES2024 `Object.groupBy` | Complete |
| 05 | [05_copying-and-cloning.md](./05_copying-and-cloning.md) | Core / Inter | Shallow copy vs deep clone, reference equality, `structuredClone` capabilities and limitations, JSON serialization pitfalls | Complete |
| 06 | [06_arrays.md](./06_arrays.md) | Core / Inter | Dense vs sparse arrays, holes vs undefined, length manipulation, V8 `PACKED` vs `HOLEY` ElementsKind transitions | Complete |
| 07 | [07_array-methods.md](./07_array-methods.md) | Core / Inter | Mutating vs pure methods, ES2023 `toSorted`, `toReversed`, `toSpliced`, `with`, higher-order functional pipelines, `flatMap` | Complete |
| 08 | [08_sorting-and-searching.md](./08_sorting-and-searching.md) | Inter / Adv | V8 Timsort architecture, comparator mathematical contracts, ES2019 stability guarantees, `localeCompare`, binary search | Complete |
| 09 | [09_destructuring.md](./09_destructuring.md) | Core / Inter | Positional array destructuring, nominal object destructuring, default value evaluation semantics, nested patterns, aliasing | Complete |
| 10 | [10_spread-and-rest.md](./10_spread-and-rest.md) | Core / Inter | Rest parameter collection, iterable array spread, ES2018 object spread, shallow copy semantics, call-stack limits | Complete |
| 11 | [11_optional-chaining-and-nullish.md](./11_optional-chaining-and-nullish.md) | Core / Inter | Optional chaining `?.`, nullish coalescing `??`, logical nullish assignment `??=`, short-circuiting jump mechanics | Complete |
| 12 | [12_property-enumeration-and-symbols.md](./12_property-enumeration-and-symbols.md) | Advanced | ECMAScript §9.1.11 `[[OwnPropertyKeys]]` deterministic 3-phase order, Symbol keys, well-known symbols, V8 properties store | Complete |

---

## Learning Objectives & Mastery Checklist

By completing this module, you will be able to:
- [x] Configure explicit property attributes (`writable`, `enumerable`, `configurable`) and enforce data immutability using `Object.freeze()`.
- [x] Implement reactive computed properties using getters and setters without introducing infinite recursion traps.
- [x] Construct immutable data transformations using pure ES2023 methods (`toSorted()`, `toReversed()`, `toSpliced()`, `with()`).
- [x] Predict and prevent V8 array performance deoptimizations caused by holey array allocation and mixed data types (`PACKED_SMI` to `HOLEY_ELEMENTS`).
- [x] Group collections deterministically using ES2024 `Object.groupBy()` and `Map.groupBy()`.
- [x] Implement stable multi-key custom sort comparators adhering to the mathematical total preorder contract.
- [x] Utilize destructuring patterns and safe fallback operators (`?.`, `??`, `??=`) to handle deeply nested and untrusted API payloads defensively.
- [x] Leverage Symbol property keys for library encapsulation and explain the exact 3-phase specification ordering of `Reflect.ownKeys()`.

---

## Verification & Execution Baseline

All code examples, edge cases, and performance benchmarks in this module are tested and verified against:
- **Node.js**: `v24.11.1` / `v22.x LTS`
- **ECMAScript Standards**: ES2015 through ES2024
- **Engine Targets**: Google V8 (Chromium / Node.js), JavaScriptCore (Safari), SpiderMonkey (Firefox)
