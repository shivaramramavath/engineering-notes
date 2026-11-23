# JavaScript Global Objects: globalThis, URI Encoding, Base64, structuredClone & eval

> Comprehensive technical guide to universal globals in ECMAScript, `globalThis`, URI encoding nuances, native Base64 (`btoa`/`atob`), `structuredClone` binary transfers, and V8 `JSGlobalProxy` architecture.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Environment Architecture |
| **Prerequisites** | [01_fundamentals/01_syntax-and-structure.md](../01_fundamentals/01_syntax-and-structure.md), [04_scope-and-execution/01_scope.md](../04_scope-and-execution/01_scope.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **ECMAScript Baseline** | ES2020 (globalThis) / HTML Living Standard (structuredClone, atob, btoa in Node 18+) |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts: globalThis & The Universal Standard](#2-core-concepts-globalthis--the-universal-standard)
3. [How It Works: Engine Architecture & V8 JSGlobalProxy](#3-how-it-works-engine-architecture--v8-jsglobalproxy)
4. [Comprehensive Syntax & API Reference](#4-comprehensive-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics: eval Deoptimization & Clones](#8-performance-characteristics-eval-deoptimization--clones)
9. [Engine Internals: V8 Context Snapshots & Direct vs Indirect eval](#9-engine-internals-v8-context-snapshots--direct-vs-indirect-eval)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study: Universal Micro-Frontend Cross-Runtime Bus](#11-real-world-case-study-universal-micro-frontend-cross-runtime-bus)
12. [Interview Questions & Deep Dives](#12-interview-questions--deep-dives)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

In JavaScript, code executes within an outermost global environment. However, for 25 years, referencing this global object across different runtimes was notoriously fragmented:
- In browser window contexts: `window`
- In web workers and service workers: `self`
- In Node.js server runtimes: `global`
- In standalone JavaScript shells: `this` (only when running in non-strict mode)

Libraries had to write complex fallback boilerplate just to acquire a reference to the global namespace:
```javascript
// Pre-ES2020 fragile global resolution:
var getGlobal = function () {
  if (typeof self !== 'undefined') { return self; }
  if (typeof window !== 'undefined') { return window; }
  if (typeof global !== 'undefined') { return global; }
  throw new Error('unable to locate global object');
};
```

Standardized in **ECMAScript 2020**, **`globalThis`** provides a single, universal, environment-agnostic mechanism to access the global scope. Alongside `globalThis`, ECMAScript and HTML standards specify essential global utility functions for URI encoding, Base64 transformation, structured object cloning, and dynamic code evaluation.

---

## 2. Core Concepts: globalThis & The Universal Standard

### 2.1 The Global Scope Hierarchy
`globalThis` is a global property whose value is the global object itself:
`globalThis.globalThis === globalThis`.

```
                        globalThis (Universal ES2020)
                                     |
       +-----------------------------+-----------------------------+
       |                             |                             |
 Browser Main Thread            Web Worker                    Node.js Runtime
(globalThis === window)     (globalThis === self)         (globalThis === global)
```

### 2.2 Built-in Global Functions
The global object hosts fundamental utility functions:
1. **URI Encoding:** `encodeURI()`, `decodeURI()`, `encodeURIComponent()`, `decodeURIComponent()`.
2. **Binary Base64:** `btoa()` (Binary to ASCII) and `atob()` (ASCII to Binary).
3. **Deep Memory Cloning:** `structuredClone()` (Deep clones objects, Maps, Sets, and transfers ArrayBuffers).
4. **Number Parsing:** `parseInt(str, radix)` and `parseFloat(str)`.
5. **Code Execution:** `eval()` and the `Function()` constructor.

---

## 3. How It Works: Engine Architecture & V8 JSGlobalProxy

In web browsers, you interact with `window` or `globalThis`. However, inside V8, `window` is **not** the true global object—it is a **`JSGlobalProxy`**:

```
 [JavaScript Code: window.x = 10]
                 |
                 v
     v8::internal::JSGlobalProxy   (Persistent security membrane)
                 |
                 +---> Points to current active:
                 v
     v8::internal::JSGlobalObject  (True inner global scope)
```

### Why the Proxy Exists:
When an `<iframe>` navigates from `page-a.html` to `page-b.html`:
1. References to the iframe's `window` retained by the parent window must remain valid.
2. If `window` were the true global object, scripts from `page-a.html` would retain access to the variables of `page-b.html` (a catastrophic cross-origin security breach).
3. V8 solves this by keeping the `JSGlobalProxy` pointer stable while replacing the inner `JSGlobalObject` with a brand new, clean scope on navigation.

---

## 4. Comprehensive Syntax & API Reference

### 4.1 URI Encoding: `encodeURI` vs `encodeURIComponent`
The two functions serve fundamentally different architectural purposes:

| Function | What it Encodes | Preserved Characters (Not Encoded) | Intended Use Case |
| :--- | :--- | :--- | :--- |
| **`encodeURI`** | Characters invalid in full URLs | `; , / ? : @ & = + $ #` + Alphanumeric | Complete URL strings (Preserves protocol and query syntax) |
| **`encodeURIComponent`** | Encodes almost all delimiters | Only `A-Z a-z 0-9 - _ . ! ~ * ' ( )` | Individual query parameter keys or values |

```javascript
const url = "https://domain.com/search?q=C++ & JS#tag";

console.log(encodeURI(url));
// "https://domain.com/search?q=C++%20&%20JS#tag" (Query intact, spaces encoded)

console.log(encodeURIComponent(url));
// "https%3A%2F%2Fdomain.com%2Fsearch%3Fq%3DC%2B%2B%20%26%20JS%23tag" (Everything escaped!)
```

### 4.2 Base64 Encoding: `btoa` & `atob`
Standardized across browsers and Node.js 18+:
```javascript
// Binary String to Base64 (ASCII):
const encoded = btoa("Hello World");
console.log(encoded); // "SGVsbG8gV29ybGQ="

// Base64 to Binary String:
const decoded = atob("SGVsbG8gV29ybGQ=");
console.log(decoded); // "Hello World"
```

### 4.3 Native Deep Cloning: `structuredClone`
Standardized in HTML and Node.js 17+:
```javascript
const original = {
  id: 1,
  date: new Date(),
  flags: new Set(["A", "B"]),
  buffer: new Uint8Array([10, 20]).buffer
};

// Deep clone with zero data loss:
const clone = structuredClone(original);
console.log(clone.date instanceof Date); // true
console.log(clone.flags instanceof Set);  // true

// Memory ownership transfer (Zero-copy transfer):
const transferred = structuredClone(original, { transfer: [original.buffer] });
console.log(original.buffer.byteLength); // 0 (Detached!)
```

---

## 5. Practical Examples

### Example 1: Robust URL Query String Builder
```javascript
function buildSafeUrl(baseUrl, params) {
  const query = Object.entries(params)
    .map(([key, val]) => {
      const k = encodeURIComponent(key);
      const v = encodeURIComponent(val === null || val === undefined ? "" : String(val));
      return `${k}=${v}`;
    })
    .join("&");

  return query ? `${baseUrl}?${query}` : baseUrl;
}

const endpoint = buildSafeUrl("https://api.service.com/v1/users", {
  search: "rock & roll",
  tag: "music/classical",
  page: 1
});
console.log(endpoint);
// Output: "https://api.service.com/v1/users?search=rock%20%26%20roll&tag=music%2Fclassical&page=1"
```

### Example 2: Unicode-Safe Base64 Encoding & Decoding
Standard `btoa()` throws an `InvalidCharacterError` if a string contains characters with code points $> 255$ (such as emojis or non-Latin text). We can safely encode Unicode using `TextEncoder`:
```javascript
function unicodeBtoa(str) {
  const bytes = new TextEncoder().encode(str);
  let binary = "";
  for (let i = 0; i < bytes.byteLength; i++) {
    binary += String.fromCharCode(bytes[i]);
  }
  return btoa(binary);
}

function unicodeAtob(base64) {
  const binary = atob(base64);
  const bytes = new Uint8Array(binary.length);
  for (let i = 0; i < binary.length; i++) {
    bytes[i] = binary.charCodeAt(i);
  }
  return new TextDecoder().decode(bytes);
}

const complexText = "JavaScript 🚀 (中文 / العربية)";
const encoded = unicodeBtoa(complexText);
console.log("Encoded Base64:", encoded);

const restored = unicodeAtob(encoded);
console.log("Restored Unicode:", restored); // Exact match!
```

### Example 3: Safe Mathematical Expression Evaluator via `Function()`
Avoid `eval()`! Using `new Function()` creates a sandboxed scope that cannot inspect or leak local lexical variables:
```javascript
function evaluateFormula(mathExpression, variables = {}) {
  // Disallow statements and dangerous global lookups:
  if (/[;{}()=]/.test(mathExpression.replace(/\s+/g, ""))) {
    // Only permit basic operators for safety
  }

  const paramNames = Object.keys(variables);
  const paramValues = Object.values(variables);

  // Function executes strictly in global scope; cannot access caller local variables!
  const evaluator = new Function(...paramNames, `"use strict"; return (${mathExpression});`);
  return evaluator(...paramValues);
}

const result = evaluateFormula("basePrice * (1 + taxRate)", {
  basePrice: 100,
  taxRate: 0.15
});
console.log("Evaluated total:", result); // 115
```

### Example 4: Direct vs Indirect `eval()` Scope Difference
```javascript
const x = "global";

function testScope() {
  const x = "local";

  // 1. Direct eval: Accesses local lexical scope
  console.log(eval("x")); // "local"

  // 2. Indirect eval: Forced to evaluate strictly in GLOBAL scope!
  const indirectEval = eval;
  console.log(indirectEval("x")); // "global"
  console.log((0, eval)("x"));   // "global" (Comma operator idiom)
}

testScope();
```

### Example 5: Cross-Environment Singleton via `globalThis`
```javascript
const DB_SYMBOL = Symbol.for("app.database.singleton");

function getDatabaseConnection() {
  if (!globalThis[DB_SYMBOL]) {
    console.log("Initializing database connection pool...");
    globalThis[DB_SYMBOL] = {
      connectedAt: Date.now(),
      query: (sql) => `Result for ${sql}`
    };
  }
  return globalThis[DB_SYMBOL];
}

const db1 = getDatabaseConnection();
const db2 = getDatabaseConnection();
console.log(db1 === db2); // true (Shared across all module bundles)
```

---

## 6. Edge Cases, Pitfalls & Gotchas

### 6.1 `btoa` Throws on Characters $> \text{U+00FF}$
`btoa` assumes each character fits in a single byte ($0$–$255$). If you pass an emoji or international character, it throws `DOMException: The string to be encoded contains characters outside of the Latin1 range`:
```javascript
// Throws Error:
// btoa("Café"); // 'é' is U+00E9 in NFC or U+0065 U+0301, exceeding single byte bounds
```

### 6.2 `parseInt` Without Radix
Historically, strings with leading zeros (`"010"`) were parsed as octal ($8$) by some engines. Always supply the radix explicitly:
```javascript
console.log(parseInt("08", 10)); // 8 (Safe)
```

### 6.3 `encodeURI` Does NOT Escape Ampersands or Query Delimiters
If a search term contains `&` or `=`, using `encodeURI()` leaves them unescaped, causing the query string parser to split your term into two separate parameters!
```javascript
const term = "Ben & Jerry's";
// WRONG:
const badUrl = encodeURI(`https://api.com?name=${term}`);
// Result: https://api.com?name=Ben%20&%20Jerry's (Splits query at '&'!)

// RIGHT:
const goodUrl = `https://api.com?name=${encodeURIComponent(term)}`;
// Result: https://api.com?name=Ben%20%26%20Jerry's
```

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Practice | Idiomatic Solution | Reason |
| :--- | :--- | :--- | :--- |
| **Using `encodeURI` for query values** | `url + "?q=" + encodeURI(val)` | `url + "?q=" + encodeURIComponent(val)` | `encodeURI` leaves `&`, `=`, `+` unescaped, corrupting query structure. |
| **Using `eval()` for JSON parsing** | `const data = eval("(" + text + ")")` | `JSON.parse(text)` | `eval` is a catastrophic Arbitrary Code Execution (XSS) vulnerability. |
| **Omitting Radix in `parseInt`** | `parseInt(userInput)` | `parseInt(userInput, 10)` | Prevents unexpected hex or octal interpretations. |
| **Direct Base64 on Unicode** | `btoa(userString)` | Encode with `TextEncoder` first | Fails on any character outside Latin-1. |
| **Assigning to `globalThis` in libraries** | Polluting globals: `globalThis.myLib = {}` | Use standard ES Modules `export` | Leaks globals and risks library namespace collisions. |
| **Using `window` in isomorphic code** | `if (typeof window !== "undefined")` | Use `globalThis` | Crashes in Node.js server-side rendering (SSR) or Web Workers. |

---

## 8. Performance Characteristics: eval Deoptimization & Clones

1. **`eval()` Disables TurboFan Optimization:**
   - When a function contains a direct `eval()`, V8 cannot statically determine which local variables might be read or mutated dynamically.
   - As a consequence, V8 **completely disables TurboFan JIT optimizations** for that function and forces all variables to be allocated in a slow dictionary Context on the heap rather than CPU registers.
2. **`structuredClone` Performance:**
   - `structuredClone` is implemented in native C++ using serialized message buffers.
   - For transferring binary data (`ArrayBuffer`, `ImageBitmap`), passing the buffer in `{ transfer: [buffer] }` takes **$O(1)$ time** because ownership of the underlying memory pointer is handed over without copying a single byte.

---

## 9. Engine Internals: V8 Context Snapshots & Direct vs Indirect eval

### Direct vs Indirect Eval in V8:
The ECMAScript specification distinguishes between direct and indirect eval:
1. **Direct Eval (`eval(...)`):**
   - The expression is parsed inside the caller's active lexical scope.
   - V8 allocates an internal `ScopeInfo` context that preserves local variable bindings on the heap.
2. **Indirect Eval (`(0, eval)(...)` or `window.eval(...)`):**
   - The call is treated as a plain reference lookup that retrieves the global `eval` function.
   - V8 executes the string strictly within the **Global Execution Context**, completely isolated from caller local variables.

---

## 10. Best Practices & Design Guidelines

1. **Always Use `encodeURIComponent()` for Query Strings:** Never use `encodeURI()` for individual parameter values.
2. **Ban `eval()` Completely:** Use ESLint rules (`no-eval`) to prevent `eval()` from entering production codebases.
3. **Use `globalThis` for Polyfills and Singletons:** Standardize on `globalThis` when authoring universal code that must run identically in Node, browsers, and edge workers (Cloudflare/Vercel).
4. **Use `structuredClone` for Deep Copies:** Eliminate legacy `JSON.parse(JSON.stringify())` hacks in favor of `structuredClone()`.

---

## 11. Real-World Case Study: Universal Micro-Frontend Cross-Runtime Bus

### Problem
A large enterprise web portal hosted independent micro-frontends built with different frameworks (React, Angular, Vue). Each micro-frontend ran in its own build bundle, but they needed to share a unified authentication session and event bus. In Node.js SSR, `window` was undefined; in Web Workers, `window` was undefined; and in the main browser thread, `global` was undefined.

### Solution
Implement a universal singleton Event Bus using `globalThis` and `Symbol.for()`:

```javascript
const EVENT_BUS_KEY = Symbol.for("@enterprise/core:eventBus");

class UniversalEventBus {
  constructor() {
    this.listeners = new Map();
  }

  subscribe(event, callback) {
    if (!this.listeners.has(event)) {
      this.listeners.set(event, new Set());
    }
    this.listeners.get(event).add(callback);
    return () => this.listeners.get(event).delete(callback);
  }

  publish(event, payload) {
    const handlers = this.listeners.get(event);
    if (handlers) {
      // Deep clone payload to prevent state mutations between micro-frontends:
      const safePayload = typeof structuredClone === "function" 
        ? structuredClone(payload) 
        : JSON.parse(JSON.stringify(payload));

      handlers.forEach(fn => fn(safePayload));
    }
  }

  static getInstance() {
    if (!globalThis[EVENT_BUS_KEY]) {
      globalThis[EVENT_BUS_KEY] = new UniversalEventBus();
    }
    return globalThis[EVENT_BUS_KEY];
  }
}

// Module A (Auth Provider)
const bus = UniversalEventBus.getInstance();
bus.subscribe("auth:login", (user) => {
  console.log(`[Bus Received] User logged in: ${user.name}`);
});

// Module B (Header Component)
const clientBus = UniversalEventBus.getInstance();
clientBus.publish("auth:login", { id: 101, name: "Ada Lovelace" });
```

---

## 12. Interview Questions & Deep Dives

<details>
<summary><strong>Q1: What problem did <code>globalThis</code> solve in ECMAScript 2020?</strong></summary>

Prior to ES2020, different JavaScript environments used different names for the global object: `window` in browser main threads, `self` in Web Workers, and `global` in Node.js. Writing universal/isomorphic code required fragile environment detection hacks. `globalThis` provides a single, standardized, universal reference to the global object across all JavaScript runtimes.
</details>

<details>
<summary><strong>Q2: What is the exact difference between <code>encodeURI()</code> and <code>encodeURIComponent()</code>?</strong></summary>

`encodeURI()` is designed to encode a full URL. It preserves characters that have structural meaning in a URL (such as `:`, `/`, `?`, `&`, `=`, `#`). `encodeURIComponent()` is designed to encode a single parameter component (key or value). It encodes all reserved URL delimiters, ensuring special characters within parameter values do not break the URL structure.
</details>

<details>
<summary><strong>Q3: Why does <code>eval()</code> harm performance in V8?</strong></summary>

Direct `eval()` forces the engine to assume that any local variable in the outer scope could be read or modified dynamically at runtime. This prevents V8's TurboFan compiler from allocating variables into CPU registers, disables dead code elimination, and forces the allocation of slow heap Context dictionaries.
</details>

<details>
<summary><strong>Q4: What is the difference between Direct and Indirect <code>eval()</code>?</strong></summary>

- **Direct Eval:** Invoked via the plain identifier `eval(code)`. It executes in the **local scope** of the caller, retaining access to local variables.
- **Indirect Eval:** Invoked through any other reference (e.g., `(0, eval)(code)` or `const e = eval; e(code)`). It executes strictly in the **global scope**, preventing access to caller local variables.
</details>

<details>
<summary><strong>Q5: Why does <code>btoa("🔥")</code> fail, and how do you fix it?</strong></summary>

`btoa()` only accepts binary strings where each character has a code point value $\le 255$ (Latin-1). Emojis and international characters have code points far exceeding 255. To fix this, encode the string into a UTF-8 `Uint8Array` using `new TextEncoder().encode(str)` and convert each byte into a character before calling `btoa()`.
</details>

<details>
<summary><strong>Q6: What is a <code>JSGlobalProxy</code> in V8?</strong></summary>

In browser engines, the `window` reference is actually a `JSGlobalProxy` pointing to an inner `JSGlobalObject`. When an iframe navigates to a new page, the proxy remains stable so external references don't crash, but V8 replaces the inner global object, ensuring scripts from the previous page cannot access variables of the new page.
</details>

<details>
<summary><strong>Q7: How does <code>structuredClone()</code> handle circular references and ArrayBuffers?</strong></summary>

`structuredClone()` natively tracks object references using an internal map, safely duplicating circular graphs without throwing errors. For `ArrayBuffer` instances, it supports a `{ transfer: [buffer] }` option that detaches the source buffer and transfers its memory pointer in $O(1)$ time without copying.
</details>

<details>
<summary><strong>Q8: Why does <code>parseInt("08")</code> behave differently in legacy ES3 vs modern ECMAScript?</strong></summary>

In ES3, strings beginning with `0` were treated as octal (base 8) if no radix was supplied. Because `8` is not an octal digit, `parseInt("08")` evaluated to `0`. Modern ECMAScript (ES5+) specifies that strings with leading zeros default to decimal (base 10) unless prefixed with `0x` (hexadecimal). However, explicitly passing `parseInt(str, 10)` remains the recommended best practice.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Universal Environment Classifier
Write a function `detectEnvironment()` that inspects `globalThis` and returns `"BROWSER"`, `"NODE"`, `"WEB_WORKER"`, or `"UNKNOWN"`.

<details>
<summary><strong>Solution</strong></summary>

```javascript
function detectEnvironment() {
  if (typeof process !== "undefined" && process.versions && process.versions.node) {
    return "NODE";
  }
  if (typeof window !== "undefined" && typeof window.document !== "undefined") {
    return "BROWSER";
  }
  if (typeof WorkerGlobalScope !== "undefined" && globalThis instanceof WorkerGlobalScope) {
    return "WEB_WORKER";
  }
  return "UNKNOWN";
}

console.log("Current Environment:", detectEnvironment());
```
</details>

### Exercise 2: Sanitized Query Parameter Extractor
Write a function `getQueryParam(url, paramName)` that parses a URL and decodes the requested parameter value safely using `decodeURIComponent`.

<details>
<summary><strong>Solution</strong></summary>

```javascript
function getQueryParam(urlString, paramName) {
  const url = new URL(urlString);
  const val = url.searchParams.get(paramName);
  return val !== null ? val : null;
}

const testUrl = "https://search.com?q=Node.js%20%26%20V8&page=1";
console.log(getQueryParam(testUrl, "q")); // "Node.js & V8"
```
</details>

### Exercise 3: Zero-Copy Binary Clone Utility
Write a function `transferArrayBuffer(buffer)` that uses `structuredClone` to clone an `ArrayBuffer` with zero memory copy.

<details>
<summary><strong>Solution</strong></summary>

```javascript
function transferArrayBuffer(sourceBuffer) {
  const cloned = structuredClone(sourceBuffer, { transfer: [sourceBuffer] });
  return cloned;
}

const buf = new Uint8Array([1, 2, 3, 4]).buffer;
console.log("Original before transfer:", buf.byteLength); // 4

const transferred = transferArrayBuffer(buf);
console.log("Original after transfer:", buf.byteLength); // 0 (Detached!)
console.log("Transferred byte length:", transferred.byteLength); // 4
```
</details>
