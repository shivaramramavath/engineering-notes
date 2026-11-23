# JavaScript Technical Facts & Canonical Platform Matrix

> Authoritative reference facts, platform baselines, TC39 proposal stages, ECMAScript specification history, and engine internal architectures.

| Fact Category | Canonical Value |
| :--- | :--- |
| **Active Node.js LTS** | Node.js v22 LTS (Codename: Jod, Active LTS through Oct 2026, Maintenance through April 2027) |
| **Current Node.js Release** | Node.js v24 (Current / Future LTS) |
| **Installed Local Toolchain** | Node.js v24.11.1, npm 11.6.3, Python 3.14.0 |
| **Standard Baseline** | ECMAScript 2024 (15th Edition) / ECMAScript 2025 (16th Edition) |
| **Engine Implementations** | V8 (Google Chrome / Node.js / Deno), JavaScriptCore (Safari / Bun), SpiderMonkey (Firefox) |

---

## 1. ECMAScript Specification History & Editions

| Edition | Year | Milestone Features |
| :---: | :---: | :--- |
| **ES6 / ES2015** | 2015 | `let`, `const`, arrow functions, classes, modules (`import`/`export`), Promises, template literals, destructuring, rest/spread, Symbols, Maps, Sets, WeakMaps, WeakSets, Generators, Proxy, Reflect, binary/octal literals |
| **ES2016** | 2016 | `Array.prototype.includes`, exponentiation operator (`**`) |
| **ES2017** | 2017 | `async`/`await`, `Object.values`, `Object.entries`, `Object.getOwnPropertyDescriptors`, String padding (`padStart`/`padEnd`), trailing commas in function params, SharedArrayBuffer, Atomics |
| **ES2018** | 2018 | Asynchronous iteration (`for-await-of`), Promise.prototype.finally, Rest/Spread for object properties, RegExp named capture groups, RegExp `s` (dotAll) flag, RegExp lookbehind assertions |
| **ES2019** | 2019 | `Array.prototype.flat`, `Array.prototype.flatMap`, `Object.fromEntries`, `String.prototype.trimStart`/`trimEnd`, optional catch binding (`catch {}`), Symbol.description, well-formed JSON.stringify |
| **ES2020** | 2020 | BigInt, Dynamic import (`import()`), Nullish coalescing (`??`), Optional chaining (`?.`), `Promise.allSettled`, `globalThis`, `String.prototype.matchAll`, `for-in` mechanics specified |
| **ES2021** | 2021 | `String.prototype.replaceAll`, `Promise.any`, AggregateError, Logical assignment operators (`&&=`, `\|\|=`, `??=`), Numeric separators (`1_000_000`), WeakRef, FinalizationRegistry |
| **ES2022** | 2022 | Class fields (public & private `#`), private methods/accessors, static class fields & static blocks, ergonomic brand checks for private fields (`#field in obj`), RegExp `/d` (match indices) flag, Top-level `await`, `Array/String/TypedArray.prototype.at()`, `Object.hasOwn()`, Error cause (`new Error('msg', { cause: err })`) |
| **ES2023** | 2023 | Change Array by copy (`toSorted`, `toReversed`, `toSpliced`, `with`), `findLast`, `findLastIndex`, Hashbang grammar (`#!/usr/bin/env node`), Symbols as WeakMap keys |
| **ES2024** | 2024 | `Object.groupBy`, `Map.groupBy`, `Promise.withResolvers()`, Set composition methods (`union`, `intersection`, `difference`, `symmetricDifference`, `isSubsetOf`, `isSupersetOf`, `isDisjointFrom`), RegExp `/v` (set notation + unicode properties) flag, ArrayBuffer transfer (`transfer`, `transferToFixedLength`, `resizable`), `Atomics.waitAsync()` |
| **ES2025** | 2025 | Sync Iterator Helpers (`map`, `filter`, `take`, `drop`, `flatMap`, `reduce`, `toArray`, etc.), `RegExp.escape()`, RegExp modifier flags (`(?i:...)`), Import Attributes (`with { type: 'json' }`), JSON Modules, `Promise.try()`, `Float16Array`, `Uint8Array` Base64 and Hex methods (`toBase64`, `fromBase64`, `toHex`, `fromHex`) |
| **ES2026+** | In Flight | Temporal API (Stage 3/4), Explicit Resource Management (`using` / `Symbol.dispose` - Stage 3), Decorators (Stage 3), ShadowRealm (Stage 3), Array.fromAsync (Stage 4) |

---

## 2. TC39 Maturity Stages

- **Stage 0 (Strawperson):** Free-form submission; input ideas.
- **Stage 1 (Proposal):** Make the case for the addition, describe high-level shape, identify challenges. Champion assigned.
- **Stage 2 (Draft):** Initial formal spec text; syntax semantics precisely drafted.
- **Stage 2.7 (Assessment for Stage 3):** Complete specification text, validation of test suites.
- **Stage 3 (Candidate):** Complete spec text signed off, all syntax/semantics resolved, external reviewers approved. Implementation begins in browsers and runtimes.
- **Stage 4 (Finished):** Two independent, spec-compliant implementations exist in shipping engines, passes test262 suite, merged into ECMA-262 master.

---

## 3. Web API Baseline Status Matrix

| API / Feature | Baseline Status | Notes |
| :--- | :---: | :--- |
| **Fetch API** | Widely Available | Supported across all modern browsers and Node.js 18+ |
| **AbortController / AbortSignal** | Widely Available | Full timeout (`AbortSignal.timeout()`) and `any()` support |
| **structuredClone()** | Widely Available | Deep clone algorithm handling circular references and typed arrays |
| **Web Workers** | Widely Available | Dedicated and Shared worker threading models |
| **Web Crypto API (`crypto.subtle`)** | Widely Available | Hardware-accelerated cryptographic primitives in browser and Node |
| **Dialog Element (`<dialog>`)** | Widely Available | Modal dialogs with native backdrop and `.showModal()` |
| **Popover API** | Newly Available | Popover content management without custom z-index/top-layer hacks |
| **View Transitions API** | Newly Available | Smooth page and DOM state transitions |
| **Web Streams API** | Widely Available | `ReadableStream`, `WritableStream`, `TransformStream` |
| **WebTransport** | Newly Available | Low-latency bi-directional multiplexed transport over HTTP/3 / QUIC |
| **WebGPU** | Newly Available | Modern compute and graphics API replacing WebGL |
| **Intl Segmenter / ListFormat** | Widely Available | High-precision locale-aware text and boundary segmentation |

---

## 4. V8 Engine Architecture Internals

```
                  JavaScript Source Code
                            |
                            v
                     [ Parser / AST ]
                            |
                            v
                 [ Ignition Interpreter ]
                 (Bytecode Generation)
                            |
            +---------------+---------------+
            |                               |
    [ Profiling Feedback ]                  |
            |                               v
            v                       [ Sparkplug ]
    [ Maglev (Mid-Tier) ]           (Baseline Compiler)
            |
            v
   [ TurboFan (Opt JIT) ]
   (Optimized Machine Code)
            |
      (Deoptimization on bail-out back to Ignition)
```

- **Hidden Classes (Maps):** V8 assigns internal hidden classes to objects based on property offsets. Changing property layout transitions an object to a new hidden class. Consistent initialization order preserves monomorphic call sites.
- **Inline Caches (IC):**
  - **Monomorphic:** Site observes exactly 1 hidden class (fastest, direct memory offset lookup).
  - **Polymorphic:** Site observes 2–4 hidden classes (short switch/lookup table).
  - **Megamorphic:** Site observes 5+ hidden classes (falls back to global hash table lookup, slowest).
- **Garbage Collection (Orinoco):**
  - **Young Generation (Scavenger / Minor GC):** Semi-space Cheney copying algorithm; survivor spaces; fast pointer-bumping allocations.
  - **Old Generation (Major GC):** Mark-Sweep-Compact algorithm with concurrent marking and parallel sweeping to minimize main-thread stalls.

---

## 5. JavaScript Event Loop & Task Scheduling

```
+--------------------------------------------------------------------+
|                         Call Stack (Active Frame)                  |
+--------------------------------------------------------------------+
                                   |
                          Stack becomes empty
                                   v
+--------------------------------------------------------------------+
|                       Microtask Queue (Run to Exhaustion)          |
|  - Promise callbacks (.then, .catch, .finally)                     |
|  - queueMicrotask(cb)                                              |
|  - MutationObserver callbacks                                      |
+--------------------------------------------------------------------+
                                   |
                        Microtasks fully drained
                                   v
+--------------------------------------------------------------------+
|                      Rendering Pipeline (Browser Only)             |
|  - requestAnimationFrame(cb)                                       |
|  - Style Calculation -> Layout (Reflow) -> Paint                   |
+--------------------------------------------------------------------+
                                   |
                                   v
+--------------------------------------------------------------------+
|                       Macrotask Queue (One task per tick)          |
|  - setTimeout / setInterval                                        |
|  - setImmediate (Node.js)                                          |
|  - I/O callbacks & network events                                  |
|  - MessageChannel / postMessage                                    |
+--------------------------------------------------------------------+
```
"""
,Overwrite:true,Path: