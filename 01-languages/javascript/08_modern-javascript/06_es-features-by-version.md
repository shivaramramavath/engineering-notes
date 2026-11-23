# ECMAScript Evolution: Complete Feature Roadmap from ES2015 to ES2025+

> Comprehensive technical guide to ECMAScript standard evolution, the TC39 staging process, complete year-by-year feature matrix (ES2015–ES2025+), engine adoption, and legacy migration strategies.

| Metadata | Details |
| :--- | :--- |
| **Level** | Comprehensive Reference |
| **Prerequisites** | [01_fundamentals/01_syntax-and-structure.md](../01_fundamentals/01_syntax-and-structure.md) |
| **Time to Read** | 28 minutes |
| **Status** | Complete |
| **ECMAScript Baseline** | ES2015 through ES2024 / ES2025 Stage 3 Proposals / Node 22 & 24 LTS |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts: TC39 Process & Browser Baseline](#2-core-concepts-tc39-process--browser-baseline)
3. [How It Works: Specification Lifecycle & Engine Adoption](#3-how-it-works-specification-lifecycle--engine-adoption)
4. [Complete Feature Reference by ECMAScript Edition](#4-complete-feature-reference-by-ecmascript-edition)
5. [Practical Examples: Modernizing Patterns](#5-practical-examples-modernizing-patterns)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics: Native vs Polyfilled](#8-performance-characteristics-native-vs-polyfilled)
9. [Engine Internals: V8 Staging Flags & Runtime Implementation](#9-engine-internals-v8-staging-flags--runtime-implementation)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study: Modernizing an Enterprise Codebase from ES6 to ES2024](#11-real-world-case-study-modernizing-an-enterprise-codebase-from-es6-to-es2024)
12. [Interview Questions & Deep Dives](#12-interview-questions--deep-dives)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

Before 2015, JavaScript evolution was agonizingly slow. ECMAScript 3 was standardized in 1999, ECMAScript 4 was abandoned after years of political gridlock, and ECMAScript 5 arrived a decade later in 2009 with modest updates (`strict mode`, JSON, array iteration methods).

In 2015, Ecma International published **ECMAScript 2015 (ES6)**: a massive, monumental overhaul that transformed JavaScript into a modern, expressive language with classes, modules, promises, arrow functions, and generators. Recognizing that six-year release gaps caused catastrophic ecosystem disruption, TC39 (Technical Committee 39) adopted an **annual release cycle**.

Since 2015, ECMAScript releases are published every June, named after the year of release (ES2016, ES2017, ... ES2024, ES2025). Features are incorporated as soon as they reach maturity, preventing stagnation and ensuring continuous improvement of browser engines and the Node.js runtime.

---

## 2. Core Concepts: TC39 Process & Browser Baseline

### 2.1 The TC39 5-Stage Proposal Process
Every language enhancement must progress through five formal stages before becoming part of the ISO/IEC ECMA-262 standard:

```
[Stage 0: Strawman]
       |
       v
[Stage 1: Proposal]      ---> Problem outlined, high-level syntax proposed, champion assigned
       |
       v
[Stage 2: Draft]         ---> Initial formal specification text written, semantics defined
       |
       v
[Stage 2.5 / 3: Candidate] -> Complete spec text, 2+ browser engine implementations, test262 tests
       |
       v
[Stage 4: Finished]      ---> Shipped in 2+ engines, passed tests, scheduled for next annual release
```

| Stage | Purpose | Requirements for Progression |
| :--- | :--- | :--- |
| **0: Strawman** | Free-form idea submission | Must be submitted by a TC39 member or registered contributor. |
| **1: Proposal** | Make the formal case for the feature | Explains the problem, solution, cross-cutting concerns, and syntax sketches. |
| **2: Draft** | Precisely define syntax and semantics | Complete formal specification text describing types, grammars, and semantics. |
| **3: Candidate** | Solution largely complete; needs implementation experience | Complete spec signed off; at least two independent engine implementations in progress. |
| **4: Finished** | Ready for inclusion in the standard | Two compatible shipping implementations; pull request merged into `tc39/ecma262`. |

### 2.2 Baseline Web Status
Managed by the WebDX Community Group (Chrome, Firefox, Safari, Edge):
- **Newly Available:** Supported by all core engines (Chromium, Gecko, WebKit) within the last 30 months.
- **Widely Available:** Supported by all core engines for more than 30 months, safe to use without polyfills.

---

## 3. How It Works: Specification Lifecycle & Engine Adoption

When a proposal reaches Stage 4:
1. **Engine Implementation:** Browser vendors implement the feature behind compile-time or runtime flags (e.g., V8's `--harmony-*` flags in Node.js and Chromium).
2. **Standardization Freeze:** In March of each year, the feature list for the upcoming edition is frozen.
3. **General Assembly Approval:** In June, the Ecma General Assembly formally ratifies the annual specification.
4. **Toolchain Integration:** TypeScript, Babel, ESLint, and Prettier update their parsers to support the syntax natively.

```
+---------------------------------------------------------------------------------+
|                       TC39 Specification Lifecycle                              |
|                                                                                 |
|  [Proposal Champion]  --->  [Engine Engineers (V8, JSC, SM)]  ---> [test262 QA] |
|           |                               |                              |      |
|           v                               v                              v      |
|  Formal Spec Text             Experimental Flagged Binary         Conformance   |
|  (ecma262 repo)               (--harmony-temporal)                Verification  |
+---------------------------------------------------------------------------------+
```

---

## 4. Complete Feature Reference by ECMAScript Edition

### 4.1 ES2015 (ES6) – The Foundational Overhaul
- **Declarations:** Block-scoped `let` and `const`, Temporal Dead Zone (TDZ).
- **Functions:** Arrow functions (`=>`), default parameters, rest parameters (`...args`), spread syntax (`...arr`).
- **Objects & Classes:** Class syntax (`class`, `extends`, `super`), enhanced object literals (method shorthand, computed property keys).
- **Collections:** `Map`, `Set`, `WeakMap`, `WeakSet`.
- **Primitives & Metaprogramming:** `Symbol`, `Proxy`, `Reflect`.
- **Asynchrony & Control Flow:** Native `Promise`, Generators (`function*`, `yield`), Iterators & `for...of`.
- **Strings & Templates:** Template literals (backticks, `${}`), tagged templates, `String.raw`.
- **Modules:** Native ESM (`import` / `export`).

### 4.2 ES2016 (ES7)
- **Exponentiation Operator:** `base ** exponent` (replaces `Math.pow`).
- **Array Search:** `Array.prototype.includes(element, fromIndex)`.

### 4.3 ES2017 (ES8)
- **Asynchronous Functions:** `async` / `await` syntax.
- **Object Introspection:** `Object.values()`, `Object.entries()`, `Object.getOwnPropertyDescriptors()`.
- **String Padding:** `String.prototype.padStart()`, `String.prototype.padEnd()`.
- **Syntax Polish:** Trailing commas in function parameter lists and call arguments.
- **Shared Memory:** `SharedArrayBuffer` and `Atomics`.

### 4.4 ES2018 (ES9)
- **Asynchronous Iteration:** `for await (const x of asyncIterable)`, `Symbol.asyncIterator`.
- **Rest/Spread on Objects:** `const { a, ...rest } = obj;`, `{ ...a, ...b }`.
- **Promise Cleanup:** `Promise.prototype.finally(callback)`.
- **RegExp Superpowers:** Lookbehind assertions (`(?<=...)`, `(?<!...)`), named capture groups (`(?<name>...)`), `s` (`dotAll`) flag, Unicode property escapes (`\p{Script=Greek}`).

### 4.5 ES2019 (ES10)
- **Array Flattening:** `Array.prototype.flat()`, `Array.prototype.flatMap()`.
- **Object Transformation:** `Object.fromEntries(iterableOfEntries)`.
- **String Trimming:** `String.prototype.trimStart()`, `String.prototype.trimEnd()`.
- **Error Ergonomics:** Optional catch binding (`try { ... } catch { ... }` without `(err)`).
- **Symbols:** `Symbol.prototype.description`.
- **JSON Compatibility:** JSON superset (allowing unescaped U+2028 and U+2029).

### 4.6 ES2020 (ES11)
- **Optional Chaining:** `user?.profile?.address?.zip`.
- **Nullish Coalescing:** `val ?? defaultValue` (checks strictly `null` or `undefined`).
- **Arbitrary Precision:** `BigInt` (e.g., `9007199254740993n`).
- **Promise Coordination:** `Promise.allSettled(promises)`.
- **Global Context:** `globalThis` unified across Node, window, and workers.
- **Module Dynamism:** `import(specifier)` dynamic imports and `import.meta`.

### 4.7 ES2021 (ES12)
- **String Replacement:** `String.prototype.replaceAll(str, replacement)`.
- **Promise First-Settled:** `Promise.any(promises)` (rejects with `AggregateError` if all fail).
- **Logical Assignment:** `&&=`, `||=`, `??=`.
- **Numeric Clarity:** Numeric separators (`const bytes = 1_000_000_000;`).
- **Memory Metaprogramming:** `WeakRef` and `FinalizationRegistry`.

### 4.8 ES2022 (ES13)
- **Class Field Ergonomics:** Public fields, private instance fields (`#private`), private methods (`#method()`), static private fields (`static #key`).
- **Static Initialization:** Class static initialization blocks (`static { ... }`).
- **Top-Level `await`:** Native `await` in ECMAScript modules without wrapping in an async function.
- **Index Ergonomics:** Relative indexing via `.at(-1)` on `Array`, `String`, and `TypedArray`.
- **Object Property Verification:** `Object.hasOwn(obj, prop)` (replaces `Object.prototype.hasOwnProperty.call`).
- **Error Causality:** `new Error("Failed", { cause: originalError })`.
- **RegExp Match Indices:** `/d` flag producing `.indices` array on match results.

### 4.9 ES2023 (ES14)
- **Array Find Last:** `Array.prototype.findLast()`, `Array.prototype.findLastIndex()`.
- **Non-Mutating Array Transformations:**
  - `arr.toReversed()`
  - `arr.toSorted(compareFn)`
  - `arr.toSpliced(start, deleteCount, ...items)`
  - `arr.with(index, value)`
- **Scripting Integration:** Hashbang / Shebang syntax support (`#!/usr/bin/env node`).
- **WeakMap Keys:** Non-registered Symbols allowed as keys in `WeakMap` and `WeakSet`.

### 4.10 ES2024 (ES15)
- **Promise Synchronization:** `Promise.withResolvers()` returning `{ promise, resolve, reject }`.
- **Collection Grouping:** `Object.groupBy(iterable, callback)` and `Map.groupBy(iterable, callback)`.
- **Buffer Resizing:** In-place resizable `ArrayBuffer` and `ArrayBuffer.prototype.transfer()`.
- **RegExp Set Operations:** `v` flag for unicode set operations (difference `--`, intersection `&&`).
- **Concurrency:** `Atomics.waitAsync()` for non-blocking worker thread coordination.

### 4.11 ES2025 & Stage 3 Pipeline (Emerging / Near-Final)
- **Temporal API (Stage 3):** Complete, modern replacement for broken `Date` object (`Temporal.Now`, `Temporal.PlainDate`, `Temporal.ZonedDateTime`).
- **Explicit Resource Management (Stage 3):** `using` and `await using` declarations for automatic disposal (`Symbol.dispose`, `Symbol.asyncDispose`).
- **Iterator Helpers (Stage 3 / Shipped in Node 22):** `.map()`, `.filter()`, `.take()`, `.drop()` on native iterators.
- **Decorators (Stage 3):** Standardized method, field, and class decorators (`@logged`, `@validate`).
- **JSON Modules / Import Attributes (Stage 3):** `import data from "./config.json" with { type: "json" };`.

---

## 5. Practical Examples: Modernizing Patterns

### Example 1: Grouping Data (ES2015 vs ES2024)
```javascript
const inventory = [
  { name: "Bananas", type: "fruit", quantity: 5 },
  { name: "Asparagus", type: "vegetable", quantity: 10 },
  { name: "Cherries", type: "fruit", quantity: 15 }
];

// Pre-ES2024: Verbose reduce
const oldGrouped = inventory.reduce((acc, item) => {
  acc[item.type] = acc[item.type] || [];
  acc[item.type].push(item);
  return acc;
}, {});

// Modern ES2024: Native Object.groupBy
const modernGrouped = Object.groupBy(inventory, item => item.type);
console.log(modernGrouped.fruit.length); // 2
```

### Example 2: Deferred Promises (Pre-ES2024 vs ES2024)
```javascript
// Pre-ES2024: Clunky closure extraction
let legacyResolve, legacyReject;
const legacyPromise = new Promise((res, rej) => {
  legacyResolve = res;
  legacyReject = rej;
});

// Modern ES2024: Clean destructuring
const { promise, resolve, reject } = Promise.withResolvers();

// Resolve from outside:
resolve("Operation finished successfully!");
promise.then(console.log);
```

### Example 3: Immutable Array Manipulation (ES2015 vs ES2023)
```javascript
const numbers = [5, 2, 8, 1, 9];

// Pre-ES2023: Must slice to avoid mutating original
const oldSorted = numbers.slice().sort((a, b) => a - b);
const oldReplaced = [...numbers];
oldReplaced[2] = 99;

// Modern ES2023: Pure, immutable built-ins
const modernSorted = numbers.toSorted((a, b) => a - b);
const modernReplaced = numbers.with(2, 99);

console.log("Original untouched:", numbers); // [5, 2, 8, 1, 9]
console.log("Replaced copy:", modernReplaced); // [5, 2, 99, 1, 9]
```

### Example 4: Nested Property Access & Defaults (ES5 vs ES2020)
```javascript
const response = {
  data: {
    user: {
      settings: {
        theme: 0 // valid falsy value!
      }
    }
  }
};

// Pre-ES2020: Verbose guard checks and || bug
const oldTheme = (response && response.data && response.data.user && response.data.user.settings)
  ? (response.data.user.settings.theme || "light") // BUG: 0 is replaced by "light"!
  : "light";

// Modern ES2020: Optional chaining & Nullish Coalescing
const modernTheme = response?.data?.user?.settings?.theme ?? "light";
console.log("Modern theme:", modernTheme); // 0 (Preserved correctly!)
```

### Example 5: Clean Error Causality & Diagnostics (ES2015 vs ES2022)
```javascript
async function fetchConfig(url) {
  try {
    const res = await fetch(url);
    return await res.json();
  } catch (originalError) {
    // ES2022: Preserve original stack trace and causal chain
    throw new Error(`Failed to load service configuration from ${url}`, {
      cause: originalError
    });
  }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

### 6.1 Logical Assignment Short-Circuiting Semantics
Unlike standard assignment `a = a || b` (which assigns unconditionally), logical assignment operators (`||=`, `&&=`, `??=`) **short-circuit**. The right-hand side is not evaluated if the condition is not met:
```javascript
let count = 0;
const getValue = () => { count++; return 42; };

let x = 100;
x ||= getValue(); // x is truthy; getValue is NEVER called!
console.log(count); // 0

let y = null;
y ??= getValue(); // y is nullish; getValue is called!
console.log(count); // 1
```

### 6.2 Nullish Coalescing (`??`) vs Logical OR (`||`)
`||` checks for **falsy** values (`false`, `0`, `""`, `NaN`, `null`, `undefined`).
`??` checks strictly for **nullish** values (`null`, `undefined`).
Mixing `&&` and `??` without explicit grouping parentheses is a syntax error:
```javascript
// Throws SyntaxError:
// const val = a && b ?? c;

// Must be explicitly parenthesized:
const val = (a && b) ?? c;
```

### 6.3 Array.prototype.sort Mutates in Place; toSorted Returns New Copy
Calling `arr.sort()` modifies `arr` directly in memory and returns a reference to `arr`. Calling `arr.toSorted()` leaves `arr` untouched and returns a freshly allocated array.
```javascript
const a = [3, 1, 2];
const b = a.toSorted();
console.log(a === b); // false
console.log(a);       // [3, 1, 2] (original intact)
```

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Practice | Idiomatic Solution | Reason |
| :--- | :--- | :--- | :--- |
| **Using `||` for default numbers** | `const port = userPort || 3000;` | `const port = userPort ?? 3000;` | If `userPort` is `0`, `||` incorrectly defaults to `3000`. |
| **Using `hasOwnProperty` directly** | `obj.hasOwnProperty("key")` | `Object.hasOwn(obj, "key")` | Direct call fails if `obj` was created via `Object.create(null)`. |
| **Manual Array Slicing before Sort** | `const copy = arr.slice().sort()` | `const copy = arr.toSorted()` | `toSorted` is cleaner, native, and avoids intermediate mutation bugs. |
| **Throwing strings or unlinked errors** | `throw new Error("Msg: " + err);` | `throw new Error("Msg", { cause: err });` | String concatenation destroys the original stack trace. |
| **Transpiling modern syntax unnecessarily** | Shipping ES5 output to modern evergreen browsers | Target `ES2022` or `ES2024` in build configuration | ES5 polyfills bloat bundle size by 20–35% and degrade execution speed. |
| **Spreading nested properties unsafely** | `user && user.address && user.address.city` | `user?.address?.city` | Verbose and error-prone compared to optional chaining. |

---

## 8. Performance Characteristics: Native vs Polyfilled

| Feature | Polyfill Implementation Cost | Modern Native Engine Performance | Memory Overhead |
| :--- | :--- | :--- | :--- |
| **Class Private Fields (`#x`)** | `WeakMap` lookup ($O(1)$ amortized, lookup overhead) | Direct slot offset in V8 Shape ($O(1)$, zero overhead) | Native: 0 extra heap objects; WeakMap: entry allocated |
| **`Object.groupBy`** | JavaScript closure + loop + object allocation | C++ micro-optimized engine loop | Polyfill creates intermediate closures; native minimizes allocations |
| **`Array.prototype.at()`** | Polyfill modulo arithmetic | Direct negative offset bounds check in C++ | Identical |
| **`Promise.withResolvers()`** | Manual Promise executor closure | Fast-path native struct allocation | Eliminates closure context allocation |

---

## 9. Engine Internals: V8 Staging Flags & Runtime Implementation

In V8 (Chrome & Node.js), experimental and upcoming ECMAScript features are managed through three tiers of feature flags:

```
[In-Development]  --->  Passed via: --js-flags="--harmony-<feature>"
        |
        v
[Staged]          --->  Enabled via: --harmony (Candidate features ready for testing)
        |
        v
[Shipping]        --->  Enabled by default in standard Node.js & Chromium releases
```

### Checking and Enabling Flags in Node.js:
To experiment with Stage 3 features in Node.js (e.g., Temporal or Explicit Resource Management):
```bash
# Enable specific proposal in Node:
node --harmony-temporal app.js

# List all available V8 flags:
node --v8-options | grep harmony
```

---

## 10. Best Practices & Design Guidelines

1. **Set Modern Compilation Targets:** In `tsconfig.json` or Babel configs for Node.js services, set `"target": "ES2022"` or `"ES2024"`. Avoid down-leveling syntax that Node supports natively.
2. **Embrace Immutable Array Methods:** Use `toSorted()`, `toReversed()`, and `with()` in state management layers (e.g., React reducers or event handlers) to prevent accidental state mutation.
3. **Use `Object.hasOwn()` Exclusively:** Replace all legacy occurrences of `Object.prototype.hasOwnProperty.call(obj, prop)` with the modern `Object.hasOwn(obj, prop)`.
4. **Preserve Error Chains via `cause`:** When catching low-level errors and re-throwing domain-specific exceptions, always attach `{ cause: err }` to maintain observability.

---

## 11. Real-World Case Study: Modernizing an Enterprise Codebase from ES6 to ES2024

### Context
A fintech payments engine written in 2017 relied on heavy Babel transpilation down to ES5, extensive Lodash helpers (`lodash.groupBy`, `lodash.get`), and manual defensive cloning. Bundle size had ballooned to 4.2 MB.

### Migration Roadmap:
1. **Remove Lodash Utility Bloat:**
   - Replaced `_.get(obj, "a.b.c", fallback)` with `obj?.a?.b?.c ?? fallback`.
   - Replaced `_.groupBy(records, "currency")` with native `Object.groupBy(records, r => r.currency)`.
   - Replaced `_.cloneDeep` on 1D arrays with `toSorted()` / `toReversed()`.
2. **Modernize Error Chains:**
   - Refactored payment gateway adapters to pass `{ cause: axiosError }`, allowing Datadog APM to capture both the high-level business error and the raw HTTP network failure in a single trace.
3. **Update Build Targets:**
   - Shifted tsconfig `"target"` from `"ES5"` to `"ES2022"`.
4. **Results:**
   - Bundle size decreased by **34%** (eliminated 1.4 MB of polyfills and helper shims).
   - JSON parsing and record grouping throughput increased by **28%**.

---

## 12. Interview Questions & Deep Dives

<details>
<summary><strong>Q1: What is the primary difference between how TC39 operates today versus how ECMAScript was standardized prior to 2015?</strong></summary>

Prior to 2015, ECMAScript releases were monolithic, multi-year projects (e.g., ES5 took 10 years after ES3). If a single feature caused disagreement, the entire specification was delayed. Since 2015, TC39 uses an annual release cadence driven by a 5-stage proposal pipeline. Features are integrated as soon as they reach Stage 4 with two independent shipping implementations, regardless of the status of other proposals.
</details>

<details>
<summary><strong>Q2: Why was <code>Object.hasOwn(obj, prop)</code> introduced when <code>obj.hasOwnProperty(prop)</code> already existed?</strong></summary>

`obj.hasOwnProperty(prop)` fails if the object does not inherit from `Object.prototype` (e.g., `Object.create(null)`), or if the property name `"hasOwnProperty"` has been shadowed on the instance. Previously, developers had to write `Object.prototype.hasOwnProperty.call(obj, prop)`. `Object.hasOwn(obj, prop)` is a clean, static, native method that handles all objects safely.
</details>

<details>
<summary><strong>Q3: How does <code>Promise.any()</code> differ from <code>Promise.race()</code>?</strong></summary>

`Promise.race()` settles as soon as the *first* promise settles (either resolves or rejects). In contrast, `Promise.any()` fulfills as soon as the *first* promise **fulfills** (ignores rejections). It only rejects if *all* input promises reject, throwing an `AggregateError` containing all rejection reasons.
</details>

<details>
<summary><strong>Q4: Explain the difference between <code>Object.groupBy()</code> and <code>Map.groupBy()</code> in ES2024.</strong></summary>

`Object.groupBy(items, fn)` returns a plain null-prototype object whose keys are coerced to strings (or symbols). `Map.groupBy(items, fn)` returns a standard `Map` instance where keys can be of any type, including objects, numbers, or booleans without string coercion.
</details>

<details>
<summary><strong>Q5: What are the Change Array by Copy methods introduced in ES2023, and why are they important?</strong></summary>

They are `toReversed()`, `toSorted()`, `toSpliced()`, and `with()`. Unlike their legacy counterparts (`reverse`, `sort`, `splice`), they do not mutate the target array in place. Instead, they return a new shallow copy with the changes applied. This is critical for functional programming, React state immutability, and deterministic data flow.
</details>

<details>
<summary><strong>Q6: How does <code>Promise.withResolvers()</code> solve common promise ergonomics issues?</strong></summary>

Previously, if asynchronous operations needed to expose `resolve` and `reject` outside the Promise executor callback, developers had to create dummy variables and assign them inside the callback closure. `Promise.withResolvers()` directly returns an object containing `{ promise, resolve, reject }`, eliminating boilerplate and preventing unhandled race conditions.
</details>

<details>
<summary><strong>Q7: What is the purpose of the RegExp <code>v</code> flag standardized in ES2024?</strong></summary>

The `v` flag supersedes the `u` flag. It adds support for set notation inside character classes, allowing set difference (`--`), intersection (`&&`), and matching multi-character Unicode grapheme clusters (such as flag emojis).
</details>

<details>
<summary><strong>Q8: What problem does the Stage 3 Temporal API solve compared to the legacy <code>Date</code> object?</strong></summary>

The legacy `Date` object conflates timestamps, wall-clock time, and time zones, has mutable instances, index-0 months, and lacks support for non-Gregorian calendars. The `Temporal` API provides immutable, domain-specific classes (`PlainDate`, `PlainTime`, `ZonedDateTime`, `Duration`) with robust timezone and calendar arithmetic.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Implement a Polyfill for `Object.groupBy`
Write a compliant standalone implementation of `groupBy(iterable, callback)` that behaves identically to ES2024 `Object.groupBy`.

<details>
<summary><strong>Solution</strong></summary>

```javascript
function groupBy(iterable, callback) {
  if (iterable === null || iterable === undefined) {
    throw new TypeError("Cannot iterate over null or undefined");
  }
  if (typeof callback !== "function") {
    throw new TypeError("Callback must be a function");
  }

  const result = Object.create(null); // Null-prototype object
  let index = 0;

  for (const item of iterable) {
    const key = callback(item, index++);
    const propertyKey = typeof key === "symbol" ? key : String(key);

    if (!(propertyKey in result)) {
      result[propertyKey] = [];
    }
    result[propertyKey].push(item);
  }

  return result;
}

const numbers = [1, 2, 3, 4, 5, 6];
const parity = groupBy(numbers, n => (n % 2 === 0 ? "even" : "odd"));
console.log(parity); // [Object: null prototype] { odd: [1, 3, 5], even: [2, 4, 6] }
```
</details>

### Exercise 2: Refactoring Legacy Object Mutation to Immutable ES2023
Take the following mutating function and refactor it to be 100% pure and immutable using ES2023 array methods:

```javascript
// Mutating code to refactor:
function updateLeaderboard(players, newPlayerIndex, newScore) {
  players[newPlayerIndex].score = newScore;
  players.sort((a, b) => b.score - a.score);
  return players;
}
```

<details>
<summary><strong>Solution</strong></summary>

```javascript
function updateLeaderboard(players, newPlayerIndex, newScore) {
  const updatedPlayer = {
    ...players[newPlayerIndex],
    score: newScore
  };

  return players
    .with(newPlayerIndex, updatedPlayer)
    .toSorted((a, b) => b.score - a.score);
}

const initial = [
  { name: "Alice", score: 50 },
  { name: "Bob", score: 80 }
];

const nextState = updateLeaderboard(initial, 0, 95);
console.log("Initial remains intact:", initial[0].score); // 50
console.log("Leaderboard updated:", nextState[0].name);    // "Alice"
```
</details>

### Exercise 3: Polyfill `Promise.withResolvers`
Implement `withResolvers()` as a static method on `Promise` ensuring exact ECMAScript 2024 compliance.

<details>
<summary><strong>Solution</strong></summary>

```javascript
function withResolvers() {
  let resolve;
  let reject;

  const promise = new Promise((res, rej) => {
    resolve = res;
    reject = rej;
  });

  return { promise, resolve, reject };
}

const { promise, resolve } = withResolvers();
setTimeout(() => resolve("Deferred success!"), 20);
promise.then(console.log);
```
</details>
