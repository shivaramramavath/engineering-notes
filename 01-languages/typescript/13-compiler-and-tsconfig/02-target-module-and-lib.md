# Target, Module, and Lib in TypeScript

> Comprehensive guide to configuring `target`, `module`, and `lib`: ECMAScript syntax emission, module format runtimes (`NodeNext`, `ESNext`, `Preserve`), and standard library declaration sets.

| Metadata | Details |
| :--- | :--- |
| **Category** | Compiler & Configuration |
| **Difficulty** | Intermediate |
| **Prerequisites** | [08-modules/01-es-modules-and-commonjs.md](../08-modules/01-es-modules-and-commonjs.md), [13-compiler-and-tsconfig/00-compiler-options.md](./00-compiler-options.md) |
| **TypeScript Baseline** | TS 4.7+ (`NodeNext`), TS 5.4+ (`target: "ES2024"`), TS 5.6+ |
| **Tested tsconfig** | `"strict": true, "target": "ES2022", "module": "NodeNext"` |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance & Type-Checker Complexity](#8-performance--type-checker-complexity)
9. [Compiler Internals](#9-compiler-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why
The trinity of `target`, `module`, and `lib` forms the foundation of TypeScript's emission and type-checking behavior:
- **`target`**: Governs what version of JavaScript syntax `tsc` downlevels to (e.g. converting `async/await` to generators, or preserving native arrow functions).
- **`module`**: Dictates what module loading syntax is emitted (`import`/`export` vs `require()`/`module.exports`).
- **`lib`**: Declares what built-in JavaScript APIs and global symbols (e.g. `Promise`, `Map`, `document`, `fetch`, `Set`) exist in the execution environment at compile time.

Configuring these incorrectly causes runtime syntax crashes (e.g. emitting class private fields to an environment that does not support them) or compile errors claiming standard APIs like `structuredClone` or `Promise.allSettled` do not exist.

---

## 2. Core Concepts
- **`target` (Syntax Downleveling)**:
  - `ES5`: Downlevels classes to functions, `for..of` to indexed loops, erases block scoping.
  - `ES2015` (ES6): Preserves native classes, arrow functions, and `let`/`const`.
  - `ES2020`: Preserves optional chaining (`?.`), nullish coalescing (`??`), and `BigInt`.
  - `ES2022`: Preserves top-level await, class static blocks, and private field syntax (`#field`).
  - `ESNext`: Follows the newest supported ECMAScript standard.
- **`module` (Module Format)**:
  - `NodeNext` / `Node16`: Follows Node.js native ESM rules (requires `.js` extensions in imports, respects `package.json` `"type"`).
  - `ESNext`: Emits standard ES module syntax (`import`/`export`).
  - `CommonJS`: Emits `require()` and `module.exports`.
  - `Preserve` (TS 5.4+): Emits module syntax unchanged for downstream bundlers.
- **`lib` (Declaration Header Sets)**:
  - When `lib` is omitted, TypeScript supplies default definitions based on `target`. Setting `target: "ES2022"` defaults to `["ES2022", "DOM", "DOM.Iterable", "ScriptHost"]`.
  - If you override `lib`, you must supply the complete list (e.g. `["ES2022"]` for Node.js backends that do not have `DOM`).

---

## 3. How It Works Internally

```
      TypeScript Source Code
                |
                v
      +-------------------+
      | Type Checker      | <--- lib: ["ES2022", "DOM"]
      | (Validates APIs)  |      Supplies global symbols (Map, window, fetch)
      +---------+---------+
                |
                v
      +-------------------+
      | Emitter           | <--- target: "ES2015" (Downlevels syntax)
      | (Produces JS)     | <--- module: "CommonJS" (Transforms imports to require)
      +---------+---------+
                |
                v
       Emitted JavaScript
```

---

## 4. Syntax & API Reference

```json
// Modern Node.js 20+ Configuration
{
  "compilerOptions": {
    "target": "ES2022",
    "module": "NodeNext",
    "moduleResolution": "NodeNext",
    "lib": ["ES2022"],
    "strict": true
  }
}

// Modern Web Browser (Vite / React) Configuration
{
  "compilerOptions": {
    "target": "ES2022",
    "module": "ESNext",
    "moduleResolution": "bundler",
    "lib": ["ES2022", "DOM", "DOM.Iterable"],
    "strict": true,
    "noEmit": true
  }
}
```

---

## 5. Practical Examples

### Example 1: Downleveling Syntax Differences (Target ES5 vs ES2022)
```typescript
class Greeter {
  #name: string;
  constructor(name: string) {
    this.#name = name;
  }
  greet() {
    return `Hello, ${this.#name}`;
  }
}

// When target: "ES2022", #name is emitted as native JavaScript private field #name.
// When target: "ES5", #name is transformed into a WeakMap instance!
```

### Example 2: Node.js Backend without DOM Pollution
```json
{
  "compilerOptions": {
    "target": "ES2022",
    "module": "NodeNext",
    "moduleResolution": "NodeNext",
    "lib": ["ES2022"]
  }
}
// With lib: ["ES2022"] and NO "DOM":
// Accidental references to window, document, or alert fail compilation immediately!
```

### Example 3: Edge Case - `structuredClone` Availability
```typescript
// structuredClone was introduced in Web APIs and Node.js 17+
// To use without DOM in Node, target ES2022 or use TS 4.8+:
const original = { a: 1, b: new Set([2, 3]) };
const cloned = structuredClone(original);
```

### Example 4: Compiler Failure Case - Target Incompatible with Native Features
```json
// tsconfig with target ES5 and ES module syntax
{
  "compilerOptions": {
    "target": "ES5",
    "module": "CommonJS"
  }
}
// In code:
// const p = await fetch();
// @ts-expect-error TS1378: Top-level 'await' expressions are only allowed when 'module' is 'es2022', 'esnext', 'system', 'node16', 'nodenext', or 'preserve', and 'target' is 'es2017' or higher.
```

### Example 5: Idiomatic Web Worker Lib Configuration
```json
{
  "compilerOptions": {
    "target": "ES2022",
    "lib": ["ES2022", "WebWorker"],
    "noEmit": true
  }
}
// "WebWorker" provides postMessage, importScripts, and DedicatedWorkerGlobalScope
// while excluding browser window/document globals.
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **DOM Globals in Node.js**: If you omit `lib` in a Node.js project, TypeScript includes `"DOM"` by default. This causes accidental accesses to `window` or `document` to compile cleanly and crash in production Node. Always set `"lib": ["ES2022"]` in backend code.
2. **`target` Does NOT Polyfill APIs**: Setting `target: "ES2022"` or `"lib": ["ES2022"]` allows you to call `Array.prototype.at()` without compile errors, but `tsc` does *not* inject polyfills. If run on an old browser or Node 14, it crashes at runtime.
3. **`module: "NodeNext"` Requires File Extensions**: Under `NodeNext`, imports must explicitly include the `.js` extension (`import { x } from './utils.js'`) even when writing `.ts` files!
4. **`module: "Preserve"`**: Introduced in TS 5.4, it leaves import and export declarations untouched for external bundlers that handle their own module resolution.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Consequence | Correction |
| :--- | :--- | :--- |
| Including `"DOM"` in backend Node.js `lib` | `window` and `document` are mistakenly available | Set `"lib": ["ES2022"]` |
| Assuming `target` injects polyfills | Runtime crash on older runtimes | Use a runtime polyfill library like `core-js` |
| Setting `module: "CommonJS"` with top-level await | **TS1378**: Top-level await not supported in CommonJS | Use `module: "NodeNext"` or wrap in async function |
| Targeting `ES5` in 2026+ | Generates bloated, slow helper code (`__awaiter`, `__extends`) | Target `ES2022` or `ESNext` |
| Mismatched `module` and `moduleResolution` | Module lookup failures | Pair `NodeNext` with `NodeNext`, or `ESNext` with `bundler` |

---

## 8. Performance & Type-Checker Complexity
- Modern targets (`ES2022`) generate drastically smaller bundles because native VM constructs (async/await, classes, optional chaining) require zero compiler helper functions.
- Loading the `DOM` library parses an extra 25,000 global declarations into TSServer. Omitting `DOM` in backend projects accelerates type checking and IDE symbol completion.

---

## 9. Compiler Internals
TypeScript's compiler libraries live in `lib.es*.d.ts` inside the `typescript` package:
- `lib.core.d.ts` contains base Object and Function prototypes.
- `lib.dom.d.ts` contains browser DOM interfaces.
- The Emitter reads `compilerOptions.target` to decide whether to invoke transform pipelines (e.g. `transformES2017` converts async functions to generators when targeting ES2015).

---

## 10. Best Practices & Design Guidelines
- Target `ES2022` for modern Node.js and evergreen browser applications.
- Never include `"DOM"` in pure Node.js services or microservices.
- For frontend apps using modern bundlers, pair `module: "ESNext"` with `moduleResolution: "bundler"`.
- For npm packages targeting Node.js, always use `module: "NodeNext"`.
- Add `"skipLibCheck": true` to avoid re-evaluating standard library interfaces on every build.

---

## 11. Real-World Case Study
A financial enterprise's Node.js microservice suffered from a memory leak and subtle production crash: a developer wrote `if (name === location)` intending to check a local variable, but typo'd `location`. Because `DOM` was included in the default `lib`, `location` resolved to the global browser `location` object instead of throwing a compile error, silently failing at runtime.

**Remediation:**
The team updated `tsconfig.json` to explicitly specify `"lib": ["ES2022"]`, excluding `"DOM"`. The TypeScript compiler immediately flagged 8 instances of accidental browser global accesses across the backend codebase.

---

## 12. Interview Questions

<details>
<summary>1. What is the fundamental difference between `target` and `lib`?</summary>

`target` specifies what JavaScript syntax `tsc` should emit (e.g. converting modern syntax down to older syntax). `lib` specifies what type declarations exist in the environment for type checking purposes (e.g. informing the compiler that `Promise` or `fetch` exists globally).
</details>

<details>
<summary>2. Why does TypeScript include the DOM library by default if `lib` is omitted?</summary>

For historical reasons. In early versions of TypeScript, the primary use case was browser client scripts, so the DOM library was included by default whenever `target` was configured.
</details>

<details>
<summary>3. What is the effect of setting `target: "ES2022"` on class private fields?</summary>

Under `ES2022`, private fields (`#privateField`) are emitted directly as native JavaScript private fields. When targeting ES2021 or lower, TypeScript downlevels private fields to `WeakMap` instances.
</details>

<details>
<summary>4. What does `module: "Preserve"` do in TypeScript 5.4+?</summary>

It instructs the TypeScript emitter to emit `import` and `export` statements exactly as written in the source code without transforming them, delegating all module transformations to downstream tools (like Bun, Vite, or esbuild).
</details>

<details>
<summary>5. How does `module: "NodeNext"` differ from `module: "ESNext"`?</summary>

`NodeNext` adheres strictly to Node.js's dual CommonJS/ESM module system: import specifiers require file extensions (`./utils.js`), conditional exports in `package.json` are respected, and files are treated as ESM or CJS based on `package.json` `"type"` or file extensions (`.mts` vs `.cts`). `ESNext` emits standard ECMAScript modules without Node-specific resolution constraints.
</details>

<details>
<summary>6. Does setting a modern `target` inject polyfills into the output?</summary>

No. TypeScript is a compiler, not a polyfill provider. It will downlevel syntax, but it never injects runtime polyfills for missing APIs like `Array.prototype.flat` or `structuredClone`.
</details>

<details>
<summary>7. How do you configure TypeScript for a Web Worker project?</summary>

By specifying `"lib": ["ES2022", "WebWorker"]`, which includes `DedicatedWorkerGlobalScope` and worker APIs while excluding standard browser window and document definitions.
</details>

<details>
<summary>8. What happens if you use top-level `await` with `target: "ES5"`?</summary>

TypeScript emits a compiler error (`TS1378`), because top-level await cannot be transpiled down to ES5 without native module support.
</details>

---

## 13. Exercises & Challenges

### Easy
1. **Node Backend Config**: Write a `tsconfig.json` snippet configuring `target`, `module`, and `lib` for Node 22 without DOM pollution.
   <details>
   <summary>Solution</summary>

   ```json
   {
     "compilerOptions": {
       "target": "ES2022",
       "module": "NodeNext",
       "moduleResolution": "NodeNext",
       "lib": ["ES2022"]
     }
   }
   ```
   </details>

2. **Web Worker Config**: Create a `tsconfig.worker.json` that provides worker types for background threads.
   <details>
   <summary>Solution</summary>

   ```json
   {
     "compilerOptions": {
       "target": "ES2022",
       "module": "ESNext",
       "moduleResolution": "bundler",
       "lib": ["ES2022", "WebWorker"],
       "noEmit": true
     }
   }
   ```
   </details>

### Medium
3. **Polyfill Verification Guard**: Write a TypeScript utility `hasArrayAt(): boolean` that safely checks if `Array.prototype.at` is available at runtime before invoking it.
   <details>
   <summary>Solution</summary>

   ```typescript
   export function hasArrayAt(): boolean {
     return typeof Array.prototype.at === 'function';
   }

   export function safeGetAt<T>(arr: T[], index: number): T | undefined {
     if (hasArrayAt()) {
       return arr.at(index);
     }
     return index >= 0 ? arr[index] : arr[arr.length + index];
   }
   ```
   </details>

4. **Dual Build Config (Node vs Browser)**: Structure a project with two tsconfig files (`tsconfig.node.json` and `tsconfig.browser.json`) extending a shared base, demonstrating clean separation of `lib`.
   <details>
   <summary>Solution</summary>

   ```json
   // tsconfig.base.json
   { "compilerOptions": { "strict": true, "target": "ES2022" } }

   // tsconfig.node.json
   {
     "extends": "./tsconfig.base.json",
     "compilerOptions": { "module": "NodeNext", "moduleResolution": "NodeNext", "lib": ["ES2022"] },
     "include": ["src/server/**/*"]
   }

   // tsconfig.browser.json
   {
     "extends": "./tsconfig.base.json",
     "compilerOptions": { "module": "ESNext", "moduleResolution": "bundler", "lib": ["ES2022", "DOM"] },
     "include": ["src/client/**/*"]
   }
   ```
   </details>

### Hard
5. **Custom Global Lib Augmentation**: Create a custom declaration file `globals.d.ts` that adds a missing experimental API `scheduler.yield()` to the global scope without modifying the official `lib` array.
   <details>
   <summary>Solution</summary>

   ```typescript
   declare global {
     interface Scheduler {
       yield(): Promise<void>;
     }
     var scheduler: Scheduler | undefined;
   }

   export {};
   ```
   </details>

6. **Target Compatibility Checker Script**: Write a Node.js script that reads a project's `tsconfig.json` and asserts that if `module` is `NodeNext`, `moduleResolution` is also `NodeNext`, throwing an explanatory error otherwise.
   <details>
   <summary>Solution</summary>

   ```typescript
   import * as fs from 'fs';

   export function validateTsConfig(path: string): void {
     const raw = JSON.parse(fs.readFileSync(path, 'utf8'));
     const opts = raw.compilerOptions ?? {};

     if (opts.module === 'NodeNext' && opts.moduleResolution !== 'NodeNext') {
       throw new Error(
         `Invalid tsconfig: When 'module' is 'NodeNext', 'moduleResolution' must also be 'NodeNext'. Found: '${opts.moduleResolution}'`
       );
     }
     console.log('tsconfig module settings are valid.');
   }
   ```
   </details>
