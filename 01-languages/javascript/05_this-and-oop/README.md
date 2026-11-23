# Module 05: This & Object-Oriented Programming (OOP)

> Exhaustive guide to JavaScript's prototypal object model, runtime `this` binding evaluation, constructor mechanics, class desugaring, dual prototype chains, hard private fields (`#`), and composition patterns.

## Overview & Curriculum Map

Unlike classical class-based languages (such as Java or C++), JavaScript implements a prototypal delegation system where behavior is shared via live prototype links rather than copied at compile time. Concurrently, the `this` keyword is evaluated dynamically at the call-site according to four hierarchical binding rules.

This module covers the complete evolution of JavaScript OOP—from legacy constructor functions and prototype chaining, through modern ES6+ `class`, `extends`, and `super` semantics, up to ES2022 hard private fields and subclass factory mixins.

---

## Complete Note Index

| Order | Note | Level | Topics Covered | Status |
| :---: | :--- | :---: | :--- | :---: |
| 01 | [01_this.md](./01_this.md) | Core / Inter | The 4 binding rules (new, explicit, implicit, default), call-site precedence, strict vs sloppy mode, lexical arrow `this`, detached method traps | Complete |
| 02 | [02_call-apply-bind.md](./02_call-apply-bind.md) | Core / Inter | `call`, `apply`, `bind`, method borrowing patterns, partial application, polyfills, V8 `BoundFunction` exotic objects | Complete |
| 03 | [03_prototypes-and-prototype-chain.md](./03_prototypes-and-prototype-chain.md) | Core / Inter | `[[Prototype]]` vs `prototype` vs `__proto__`, `Object.create()`, lookup traversal, property shadowing, prototype pollution security defenses | Complete |
| 04 | [04_constructor-functions.md](./04_constructor-functions.md) | Core / Inter | Pre-ES6 constructor pattern, 5-step `new` execution algorithm, return overrides, `new.target`, prototypal inheritance wiring | Complete |
| 05 | [05_classes.md](./05_classes.md) | Core / Inter | Class declarations vs expressions, constructor desugaring, non-enumerable prototype methods, public fields, static blocks | Complete |
| 06 | [06_inheritance.md](./06_inheritance.md) | Inter / Adv | `extends`, derived constructor `super()` requirement, method delegation via `[[HomeObject]]`, dual prototype chains, subclassing built-ins | Complete |
| 07 | [07_private-fields.md](./07_private-fields.md) | Core / Inter | Hard privacy via `#field`, private methods, private static members, realm-safe brand checking with `#field in obj`, V8 PrivateName symbols | Complete |
| 08 | [08_mixins-and-composition.md](./08_mixins-and-composition.md) | Inter / Adv | Composition over inheritance, Fragile Base Class problem, `Object.assign` mixins vs subclass factory mixins, ECS architecture | Complete |

---

## Learning Objectives & Mastery Checklist

By completing this module, you will be able to:
- [x] Predict the exact runtime value of `this` across all function invocation patterns and fix detached context bugs in asynchronous callbacks.
- [x] Implement complete ECMAScript-compliant polyfills for `call()`, `apply()`, and `bind()`.
- [x] Trace property lookup across the prototype chain, differentiate own properties from inherited ones, and defend applications against Prototype Pollution.
- [x] Explain the 5 internal steps of the `new` operator and replicate its behavior using `Object.create()` and `.apply()`.
- [x] Architect clean class hierarchies using `class`, `extends`, and `super()`, explaining the dual prototype chains established by the engine.
- [x] Enforce hard encapsulation using ES2022 private class fields (`#`), private methods, and the `#field in obj` brand check operator.
- [x] Design composable systems using object mixins and subclass factory mixins to avoid monolithic class hierarchies.

---

## Verification & Execution Baseline

All code examples, edge cases, and performance benchmarks in this module are tested and verified against:
- **Node.js**: `v24.11.1` / `v22.x LTS`
- **ECMAScript Standards**: ES1 through ES2022 Class Features
- **Engine Targets**: Google V8 (Chromium / Node.js), JavaScriptCore (Safari), SpiderMonkey (Firefox)
