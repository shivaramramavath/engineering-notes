# ECMAScript Features by Version Cheatsheet (ES2015 – ES2025)

> Comprehensive reference of modern JavaScript language capabilities categorized by standardized TC39 publication year (ES6 through ES2025).

| Metadata | Details |
| :--- | :--- |
| **Track** | Reference Cheatsheets (`27_cheatsheets`) |
| **Baseline** | ECMAScript 2024 / ECMAScript 2025 Stage 4 |
| **Level** | Reference / Quick Lookup |

---

## 1. TC39 Specification Process

Every JavaScript addition progresses through five standardized TC39 maturity stages:
- **Stage 0 (Strawperson)**: Initial submission idea.
- **Stage 1 (Proposal)**: Formal problem definition, API shape, and champion.
- **Stage 2 (Draft)**: Precise specification syntax and semantics drafted.
- **Stage 3 (Candidate)**: Specification complete; requires experimental implementations (browsers / Node.js).
- **Stage 4 (Finished)**: Shipped in 2+ independent implementations; merged into the annual standard.

---

## 2. ES2024 (15th Edition)

| Feature | Key Syntax / API | Description |
| :--- | :--- | :--- |
| **`Object.groupBy` / `Map.groupBy`** | `Object.groupBy(arr, fn)` | Groups array items into object or Map partitions |
| **`Promise.withResolvers`** | `const { promise, resolve, reject } = Promise.withResolvers()` | Extracts promise and resolver callbacks |
| **`RegExp` `v` Flag** | `/[\p{Letter}--[a-z]]/v` | Set operations (intersection/subtraction) in RegExp |
| **`ArrayBuffer.prototype.resize`** | `buffer.resize(newByteLength)` | Resizable ArrayBuffers and Growable SharedArrayBuffers |
| **`String.prototype.isWellFormed`** | `str.isWellFormed()`, `toWellFormed()` | Validates absence of lone UTF-16 surrogates |

---

## 3. ES2023 (14th Edition)

| Feature | Key Syntax / API | Description |
| :--- | :--- | :--- |
| **Change Array by Copy** | `toSorted()`, `toReversed()`, `toSpliced()`, `with()` | Non-mutating immutable array operations |
| **Array Find from Last** | `findLast()`, `findLastIndex()` | Reverse-order searching |
| **Hashbang Grammar** | `#!/usr/bin/env node` | Direct support for executable CLI hashbang lines |
| **Symbols as WeakMap Keys** | `weakMap.set(Symbol('id'), obj)` | Permits unique Symbols in WeakMaps and WeakSets |

---

## 4. ES2022 (13th Edition)

| Feature | Key Syntax / API | Description |
| :--- | :--- | :--- |
| **Class Private Fields & Methods** | `#privateField`, `#privateMethod()` | Truly private, engine-enforced class encapsulation |
| **Class Static Blocks** | `static { ... }` | Evaluated once during class definition |
| **Top-Level `await`** | `await fetch(...)` in ES modules | Asynchronous module loading without wrapping async IIFE |
| **`Object.hasOwn`** | `Object.hasOwn(obj, 'prop')` | Ergonomic, safe alternative to `hasOwnProperty` |
| **`Error.cause`** | `new Error('Failed', { cause: originalErr })` | Error chaining for deep diagnostic traces |
| **`at()` Method** | `arr.at(-1)`, `str.at(-1)` | Negative index lookup for arrays and strings |
| **`RegExp` Match Indices (`d` flag)**| `/pattern/d` | Emits start/end index boundaries for capture groups |

---

## 5. ES2021 (12th Edition)

| Feature | Key Syntax / API | Description |
| :--- | :--- | :--- |
| **Logical Assignment Operators** | `&&=`, `\|\|=`, `??=` | Combines logical operations with in-place assignment |
| **Numeric Separators** | `1_000_000_000` | Underscores in numeric literals for visual clarity |
| **`String.prototype.replaceAll`** | `str.replaceAll('a', 'b')` | Replaces all string occurrences without global regex |
| **`Promise.any`** | `Promise.any([p1, p2])` | Resolves on fastest fulfillment; rejects if all fail |
| **WeakRefs & FinalizationRegistry** | `new WeakRef(obj)`, `new FinalizationRegistry(fn)` | Weak object pointers and cleanup hooks on GC |

---

## 6. ES2020 (11th Edition)

| Feature | Key Syntax / API | Description |
| :--- | :--- | :--- |
| **Optional Chaining (`?.`)** | `user?.address?.city`, `fn?.()` | Safe nested navigation without throwing `TypeError` |
| **Nullish Coalescing (`??`)** | `val ?? defaultVal` | Fallback only when value is `null` or `undefined` |
| **`BigInt`** | `9007199254740991n` | Arbitrary-precision integer arithmetic |
| **`Promise.allSettled`** | `Promise.allSettled([p1, p2])` | Waits for all promises regardless of outcome |
| **Dynamic `import()`** | `const mod = await import('./m.js')` | Asynchronous on-demand code splitting |
| **`globalThis`** | `globalThis` | Universal global object across Browser, Node, Workers |
| **`String.prototype.matchAll`** | `str.matchAll(/regex/g)` | Returns full iterator of regex match objects |

---

## 7. ES2019 – ES2015 Foundation Highlights

- **ES2019**: `flat()`, `flatMap()`, `Object.fromEntries()`, `trimStart()`, `trimEnd()`, optional catch binding `try { ... } catch { ... }`.
- **ES2018**: Async Iteration (`for await...of`), Object Rest/Spread (`{ ...obj }`), Promise `.finally()`.
- **ES2017**: `async` / `await`, `Object.values()`, `Object.entries()`, `String.prototype.padStart()`, `padEnd()`.
- **ES2016**: Exponentiation operator (`**`), `Array.prototype.includes()`.
- **ES2015 (ES6)**: `let` / `const`, Arrow functions, Classes, Modules (`import`/`export`), Promises, Template Literals, Destructuring, Default params, `Map`/`Set`/`WeakMap`/`WeakSet`, Symbols, Iterators & Generators, Proxies & Reflect.
