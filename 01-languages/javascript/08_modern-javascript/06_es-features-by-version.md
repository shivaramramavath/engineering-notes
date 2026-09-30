# ES Features by Version

A map of what each ECMAScript edition added. Knowing the timeline helps you read old code, choose a build target, and check compatibility.

> The language keeps evolving. Verify the newest items on [MDN compatibility tables](https://developer.mozilla.org) and the [TC39 proposals repo](https://github.com/tc39/proposals) before relying on them.

## How to read this

| Term | Meaning |
|------|---------|
| ES6 = ES2015 | Same edition (yearly naming started with ES2016) |
| Stage 4 | Proposal is finished and enters the next edition |
| Baseline | Feature works across all major browsers |

## ES5 (2009): the baseline

`"use strict"`, `Array.prototype.forEach/map/filter/reduce`, `JSON`, `Object.keys/create/defineProperty/freeze`, `Function.prototype.bind`, getters/setters, trailing commas in literals.

## ES2015 (ES6): the big one

| Category | Features |
|----------|----------|
| Declarations | `let`, `const`, block scope |
| Functions | arrow functions, default and rest parameters, spread |
| Objects | shorthand, computed keys, method shorthand, `Object.assign`, `Object.is` |
| Classes | `class`, `extends`, `super`, `static` |
| Strings | template literals, `includes`, `startsWith`, `endsWith`, `repeat`, code-point methods |
| Destructuring | arrays and objects |
| Modules | `import` / `export` |
| Async | `Promise` |
| Iteration | iterators, `for...of`, generators |
| Collections | `Map`, `Set`, `WeakMap`, `WeakSet` |
| Symbols | `Symbol` and well-known symbols |
| Meta | `Proxy`, `Reflect`, `new.target` |
| Arrays | `Array.from`, `Array.of`, `find`, `findIndex`, `fill`, `copyWithin`, `entries/keys/values` |
| Numbers | `Number.isNaN`, `isInteger`, `isSafeInteger`, `EPSILON`, `Math.trunc/sign/cbrt/hypot...` |
| Binary data | typed arrays, `ArrayBuffer` |

## ES2016

- `Array.prototype.includes`
- Exponent operator `**`

## ES2017

- `async` / `await`
- `Object.values`, `Object.entries`, `Object.getOwnPropertyDescriptors`
- `String.prototype.padStart` / `padEnd`
- Trailing commas in function parameter lists
- `SharedArrayBuffer`, `Atomics`

## ES2018

- Object **rest/spread** (`{ ...a }`, `{ a, ...rest }`)
- **Async iteration** (`for await...of`, async generators)
- `Promise.prototype.finally`
- RegExp: named capture groups, lookbehind, `s` (dotAll) flag, Unicode property escapes (`\p{...}`)

## ES2019

- `Array.prototype.flat`, `flatMap`
- `Object.fromEntries`
- `String.prototype.trimStart` / `trimEnd`
- Optional catch binding: `catch { }`
- `Symbol.prototype.description`
- Stable `Array.prototype.sort`
- Well-formed `JSON.stringify`, `Function.prototype.toString` revision

## ES2020

- **Optional chaining** `?.`
- **Nullish coalescing** `??`
- `BigInt`
- `Promise.allSettled`
- Dynamic `import()`
- `globalThis`
- `import.meta`
- `String.prototype.matchAll`
- `export * as ns from "mod"`

## ES2021

- Logical assignment: `||=`, `&&=`, `??=`
- Numeric separators: `1_000_000`
- `Promise.any` and `AggregateError`
- `String.prototype.replaceAll`
- `WeakRef` and `FinalizationRegistry`

## ES2022

- **Class fields** (public), **private** fields and methods (`#x`), static class fields and **static blocks**
- `#x in obj` brand checks
- **Top-level `await`** in modules
- `Array.prototype.at` (and `String`/TypedArray `at`)
- `Object.hasOwn`
- `Error` `cause` option
- RegExp match indices (`d` flag)

## ES2023

- `Array.prototype.findLast`, `findLastIndex`
- Non-mutating array methods: `toSorted`, `toReversed`, `toSpliced`, `with`
- Hashbang (`#!/usr/bin/env node`) syntax support
- Symbols as `WeakMap` keys

## ES2024

- `Object.groupBy`, `Map.groupBy`
- `Promise.withResolvers`
- `Array.fromAsync`
- Resizable and transferable `ArrayBuffer`
- `String.prototype.isWellFormed` / `toWellFormed`
- RegExp `v` flag (set notation)
- `Atomics.waitAsync`

## ES2025

- **Iterator helpers** (`map`, `filter`, `take`, `drop`, `flatMap`, `reduce`, `toArray`, `Iterator.from`)
- **New `Set` methods**: `union`, `intersection`, `difference`, `symmetricDifference`, `isSubsetOf`, `isSupersetOf`, `isDisjointFrom`
- **JSON modules** and **import attributes** (`import data from "./x.json" with { type: "json" }`)
- `Promise.try`
- `RegExp.escape`
- RegExp modifiers and duplicate named capture groups
- `Float16Array` and `Math.f16round`

## Coming next (check status before use)

| Feature | Notes |
|---------|-------|
| **Temporal** | Modern date/time API; already shipping in some engines, verify support |
| Explicit resource management (`using`) | Automatic cleanup of resources |
| Decorators | Class and member decorators |
| Error and binary-data helpers | Newer Stage 3/4 items appear each year |
| Pipeline operator | Long-running proposal |

## Quick "which version?" table

| I want... | Need at least |
|-----------|---------------|
| `let`/`const`, arrows, classes, promises, modules | ES2015 |
| `async`/`await` | ES2017 |
| Object spread | ES2018 |
| `?.` and `??` | ES2020 |
| `replaceAll`, `??=` | ES2021 |
| Private fields, top-level `await`, `at()`, `Object.hasOwn` | ES2022 |
| `toSorted`, `findLast` | ES2023 |
| `Object.groupBy`, `Promise.withResolvers` | ES2024 |
| Iterator helpers, Set methods | ES2025 |

## Choosing a target

| Environment | Guidance |
|-------------|----------|
| Node.js | Pick an active LTS and check `node.green` |
| Modern browsers | Use Baseline data; target ES2020 or newer safely |
| Legacy browsers | Transpile with Babel/SWC and add polyfills |
| Libraries | Publish ESM, document minimum runtime |

Tools: `browserslist`, `@babel/preset-env`, TypeScript `target`, ESLint `ecmaVersion`, `eslint-plugin-compat`.

```json
{ "browserslist": ["defaults", "not IE 11", "maintained node versions"] }
```

## Polyfills vs transpiling

| | Transpile | Polyfill |
|---|-----------|----------|
| Changes | **Syntax** (`?.`, classes) | **APIs** (`Array.prototype.at`, `Promise.any`) |
| Tools | Babel, SWC, TypeScript | `core-js`, feature detection |
| Cannot polyfill | Syntax itself | Some things like `Proxy` are impractical |

## Feature detection

```js
if (typeof Array.prototype.toSorted === "function") { /* use it */ }
const supportsGroupBy = typeof Object.groupBy === "function";
```

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Using a feature without checking runtime support | Crashes in older environments | MDN/Baseline check, transpile, polyfill |
| Assuming `ES6` and `ES2015` differ | Same thing | Use year names |
| Trusting tutorials with old syntax | Outdated advice | Prefer current MDN |
| Polyfilling everything "just in case" | Bundle bloat | Target real browsers |
| Confusing syntax (transpile) with APIs (polyfill) | Missing pieces | Configure both |
| Relying on Stage 2/3 proposals in production | May change | Wait for Stage 4 or accept risk |

## Key takeaways

- ES2015 is the big modernization; a new edition arrives every year
- Syntax needs transpiling; APIs need polyfills
- Check MDN and Baseline for support before using newer features
- Set your build target from real browser/Node usage data

**Next:** [Modern Syntax Cheatsheet](./07_modern-syntax-cheatsheet.md)
