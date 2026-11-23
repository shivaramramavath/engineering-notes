# Bundlers & Tree-Shaking

> Modern module bundlers analyze the static dependency graph of ECMAScript Modules to perform live code inclusion, dead-code elimination, scope hoisting, and optimal chunk splitting.

---

## Metadata

| Attribute | Details |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [ES Modules](./01_es-modules.md), [CommonJS](./02_commonjs.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Standard** | ECMAScript Module Specification / Rollup & Webpack Optimization Standards |

---

## Table of Contents

1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals](#9-engine-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

In production web applications, deploying thousands of individual ES Module files across the network causes catastrophic performance degradation. Even with HTTP/2 and HTTP/3 multiplexing, deep dependency graphs create severe round-trip latency waterfalls, repetitive header overhead, and suboptimal compression ratios. Furthermore, applications frequently import large third-party libraries (e.g., Lodash, date-fns, Lucide icons) while consuming only a fraction of their exported functions.

**Bundlers** (such as Rollup, Vite, esbuild, and Webpack) solve these challenges by:
1. Concatenating modular source files into optimized distribution bundles.
2. Performing **Tree-Shaking** (Live Code Inclusion): analyzing static ESM exports and omitting all unused code from the final bundle.
3. **Scope Hoisting**: merging module scopes into a single shared closure to eliminate function call overhead and reduce file size.
4. **Code Splitting**: dividing the application into on-demand chunks loaded dynamically as users navigate.

---

## 2. Core Concepts

### Dead-Code Elimination (DCE) vs Tree-Shaking
While often used interchangeably, DCE and Tree-Shaking operate from opposite philosophical directions:
- **Dead-Code Elimination (DCE)**: Starts with the full codebase and aggressively prunes unreachable branches (e.g., `if (false) { ... }`). Historically performed by minifiers like Terser.
- **Tree-Shaking (Live Code Inclusion)**: Starts at the entry point with an empty bundle and traces the directed graph of *used* exports, including *only* what is provably referenced. Introduced by Rollup.

```text
Full Library: [fnA, fnB, fnC, fnD, fnE]
Application:  import { fnA } from 'library';

Dead-Code Elimination:
 Starts with [fnA, fnB, fnC, fnD, fnE] -> Searches for unreachable code -> May miss dynamic references

Tree-Shaking (Live Code Inclusion):
 Starts with Entry Point -> Follows reference to fnA -> Bundle contains ONLY [fnA]
 (fnB, fnC, fnD, fnE are never even added to the bundle!)
```

### The Side-Effects Hazard
Tree-shaking relies on the invariant that unused modules have no observable side effects when imported. A **side effect** is any operation that modifies global state or host APIs upon evaluation (e.g., modifying `window`, modifying prototypes, configuring polyfills, or logging). If a bundler cannot prove that an unreferenced module is free of side effects, it must bundle the module to preserve correctness.

---

## 3. How It Works Internally

### The Static Module Graph Analysis Pipeline

```
 [ Entry Point: app.js ]
            |
            v
 [ AST Parsing & Dependency Discovery ]
   - Parse code with Acorn / SWC / Babel into Abstract Syntax Trees (AST)
   - Scan top-level `import` and `export` statements
   - Build Module Graph of all reachable dependencies
            |
            v
 [ Export Usage Tracking (Mark & Sweep) ]
   - Start with entry point exports as "Live"
   - Traverse downstream imports: mark referenced identifiers as "Used"
   - Any export not marked as "Used" is flagged for omission
            |
            v
 [ Side-Effect Verification ]
   - Check package.json "sideEffects" field
   - Inspect AST for expressions with potential side effects:
     - Global object mutations
     - Unannotated top-level function invocations
     - If module is pure and has no used exports -> DISCARD COMPLETELY
            |
            v
 [ Scope Hoisting & Code Generation ]
   - Concatenate all modules into a single flattened closure
   - Rename variables to prevent name collisions
   - Emit optimized production bundle chunks
```

---

## 4. Syntax & API Reference

### Declarations & Bundler Annotations

```json
// 1. package.json "sideEffects" declaration
// Informs bundlers that all files in this package are pure (safe to drop if unused)
{
  "name": "my-library",
  "sideEffects": false
}

// Or declare specific files that contain real side effects (e.g. CSS or polyfills)
{
  "name": "my-ui-library",
  "sideEffects": [
    "*.css",
    "src/polyfills.js"
  ]
}
```

```javascript
// 2. Pure function call annotation for minifiers/bundlers
// /*#__PURE__*/ tells the bundler that if the result is unused, the call has NO side effects
export const defaultFormatter = /*#__PURE__*/ createComplexFormatter();

// 3. Dynamic Code Splitting boundary
const AdminPanel = await import('./admin/AdminPanel.js');
```

---

## 5. Practical Examples

### Example 1: Tree-Shakable Library Architecture
```javascript
// math-library.js
// PURE EXPORTS: No top-level side effects!
export function add(a, b) {
  return a + b;
}

export function multiply(a, b) {
  return a * b;
}

export function heavyMatrixMultiply(matrixA, matrixB) {
  // Heavy algorithm with large internal lookup tables
  console.log('Heavy computation');
  return [];
}

// app.js
import { add } from './math-library.js';
console.log(add(2, 3));

// Production Bundled Output:
// The bundler omits `multiply` and `heavyMatrixMultiply` entirely!
// Output contains only:
function add(a, b) { return a + b; }
console.log(add(2, 3));
```

### Example 2: How Top-Level Invocations Break Tree-Shaking
```javascript
// BROKEN: Impure Top-Level Call
class Logger {
  log(msg) { console.log(msg); }
}

// Even if `defaultLogger` is never imported, bundlers must retain this
// because `new Logger()` could execute arbitrary side effects!
export const defaultLogger = new Logger();

// FIXED: Mark with /*#__PURE__*/ annotation
export const safeDefaultLogger = /*#__PURE__*/ new Logger();
```

### Example 3: Chunk Splitting Configuration in Vite / Rollup
```javascript
// vite.config.js / rollup.config.js
import { defineConfig } from 'vite';

export default defineConfig({
  build: {
    rollupOptions: {
      output: {
        manualChunks(id) {
          // Isolate vendor dependencies into independent long-lived cached chunks
          if (id.includes('node_modules')) {
            if (id.includes('react')) return 'vendor-react';
            if (id.includes('lodash-es')) return 'vendor-lodash';
            return 'vendor-common';
          }
        },
      },
    },
  },
});
```

### Example 4: Rollup Scope Hoisting Before and After
```javascript
// BEFORE (Standard Modules):
// moduleA.js
export const greeting = 'Hello';
// moduleB.js
import { greeting } from './moduleA.js';
export function greet(name) { return `${greeting}, ${name}`; }

// AFTER (Scope Hoisted Bundle):
// Both modules flattened into single scope; function wrapper overhead eliminated!
const greeting = 'Hello';
function greet(name) { return `${greeting}, ${name}`; }
export { greet };
```

### Example 5: Barrel File Tree-Shaking Hazard
```javascript
// icons/index.js (The Barrel File)
export { HomeIcon } from './HomeIcon.js';
export { UserIcon } from './UserIcon.js';
export { SettingsIcon } from './SettingsIcon.js';
// ... 5,000 more icons ...

// app.js
import { HomeIcon } from './icons/index.js';

// If package.json lacks "sideEffects": false, bundlers must parse and bundle
// all 5,000 icon files because evaluating them might contain side effects!
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Re-exporting via Barrel Files**: Re-exporting hundreds of modules from a single `index.js` forces the bundler to parse and build ASTs for every re-exported module, drastically slowing down build times and frequently leaking unused modules if side-effects are improperly declared.
2. **Class Field Initializers**: Transpiling class properties down to ES5/ES2020 sometimes introduces helper calls (e.g., `_defineProperty`) that bundlers cannot prove are pure without `/*#__PURE__*/`.
3. **CommonJS Bailouts**: When an ES Module imports a CommonJS package (or a package that compiles down to CJS internally), tree-shaking fails completely; the entire CJS module object is included in the bundle.
4. **Mutating Prototype Chains**: Modifying built-in prototypes (`Array.prototype.customMethod = ...`) is an observable side effect that prevents bundlers from discarding the file even if no exports are imported.

---

## 7. Common Mistakes & Anti-Patterns

| Mistake | Incorrect Pattern | Correct Solution | Why It Fails |
| :--- | :--- | :--- | :--- |
| **Missing `"sideEffects": false`** | Omitting field in library `package.json` | Add `"sideEffects": false` | Bundlers assume unimported files may have side effects and bundle them. |
| **Importing CommonJS Lodash** | `import { debounce } from 'lodash'` | `import debounce from 'lodash-es/debounce'` | `lodash` is CommonJS; bundler pulls in all 300+ functions (~70KB). |
| **Top-Level Impure Expressions** | `export const instance = createInstance()` | Annotate with `/*#__PURE__*/` | Bundler cannot know if `createInstance()` mutates global state. |
| **Default Export of Object Bags** | `export default { add, subtract, multiply }` | Use named exports `export { add, ... }` | Bundler treats default object as a single atomic unit, breaking tree-shaking. |
| **Dynamic Import with Variable String** | `import(`./modules/${name}.js`)` | Use explicit mapping or bounded regex | Prevents bundler from statically identifying which chunks to build. |
| **Re-exporting Everything Naively** | `export * from './heavy-subsystem'` | Export only what consumers actually require | Pulls unnecessary subgraphs into the module resolution graph. |

---

## 8. Performance Characteristics

- **Bundle Payload Reduction**: Proper tree-shaking and side-effect annotations typically reduce third-party dependency payload size by 40%–80%.
- **Scope Hoisting Runtime Boost**: Eliminating CommonJS/AMD function wrappers reduces runtime function call overhead and shrinks minified variable name collision tables.
- **Build-Time Memory Footprint**: Deep barrel files and unannotated side-effect trees increase bundler memory consumption and slow down hot module replacement (HMR) cycles by orders of magnitude.

---

## 9. Engine Internals

1. **AST Pruning in esbuild**: Written in Go, `esbuild` constructs ASTs and performs cross-module symbol reference tracing using multi-threaded parallel passes, achieving tree-shaking speeds 10x–50x faster than JavaScript-based tools.
2. **Rollup Tree-Shaking Algorithm**: Rollup creates `Module` instances where each top-level statement is represented by a `Statement` node. It tracks which statements have side effects; statements without side effects whose bound variables are not included in the entry graph are omitted during chunk rendering.

---

## 10. Best Practices & Design Guidelines

1. **Always Set `"sideEffects": false` in Libraries**: If your package consists of pure modules, explicitly declare `"sideEffects": false` in `package.json`.
2. **Use `/*#__PURE__*/` on Top-Level Factory Invocations**: Annotate class instantiations and factory calls assigned to top-level variables.
3. **Prefer Granular Path Imports Over Giant Barrel Files**: Write `import map from 'lodash-es/map'` instead of pulling from a monolithic barrel when tooling cannot optimize barrels.
4. **Leverage Dynamic `import()` for Large Dependencies**: Load charting engines, rich text editors, and PDF generators only when the user requests them.

---

## 11. Real-World Case Study

### Optimizing a Heavy Web App Bundle Size by 65%

An enterprise SaaS dashboard application suffered from a 3.8MB initial JavaScript bundle, resulting in an 8-second Time to Interactive (TTI) on mobile devices.

```javascript
// vite.config.js - PRODUCTION OPTIMIZATION
import { defineConfig } from 'vite';
import { visualizer } from 'rollup-plugin-visualizer';

export default defineConfig({
  build: {
    target: 'es2022',
    minify: 'esbuild',
    rollupOptions: {
      plugins: [
        visualizer({ open: true, filename: 'bundle-stats.html' }),
      ],
      output: {
        // Enforce code-splitting across vendor boundaries
        manualChunks: {
          'vendor-charts': ['chart.js'],
          'vendor-dates': ['date-fns'],
        },
      },
    },
  },
});
```

```javascript
// app.js - REFACTORED ENTRY POINT
// 1. REPLACED MONOLITHIC CJS LODASH WITH TREE-SHAKABLE ESM
// Before: import { debounce } from 'lodash'; (Pulls entire 75KB library)
// After:
import debounce from 'lodash-es/debounce'; // Bundles only ~2KB!

// 2. LAZY-LOADED CHARTING MODULE VIA DYNAMIC IMPORT
const chartButton = document.getElementById('view-chart');
chartButton.addEventListener('click', async () => {
  const { renderSalesChart } = await import('./charts/salesChart.js');
  renderSalesChart();
});

// Outcome:
// - Initial bundle plummeted from 3.8MB to 420KB (-89% initial JS)
// - Time to Interactive dropped from 8.2s to 1.1s on 4G networks.
```

---

## 12. Interview Questions

<details>
<summary><strong>1. What is the fundamental difference between dead-code elimination and tree-shaking?</strong></summary>

Dead-code elimination starts with the entire program and searches for unreachable code to delete. Tree-shaking (live code inclusion) starts at the application entry point and traverses the static ESM import graph, including only functions and variables that are provably referenced.
</details>

<details>
<summary><strong>2. What does <code>"sideEffects": false</code> in package.json tell a bundler?</strong></summary>

It informs bundlers that no files in the package perform observable side effects upon initial evaluation (such as modifying global variables or prototypes). If an application imports a file from this package without using any of its exports, the bundler can safely skip including the file entirely.
</details>

<details>
<summary><strong>3. Why does tree-shaking fail when a library exports an object with functions?</strong></summary>

When a library does `export default { add, subtract }`, it exports a single JavaScript object. The bundler cannot safely determine at compile time which properties of that object will be dynamically accessed at runtime (`obj[prop]`), so it must include the entire object and all its methods.
</details>

<details>
<summary><strong>4. What is the purpose of the <code>/*#__PURE__*/</code> annotation?</strong></summary>

It informs bundlers and minifiers that a top-level function or constructor call has no side effects. If the variable storing the result of that call is never used, the bundler can safely delete the function call entirely.
</details>

<details>
<summary><strong>5. What is "Scope Hoisting"?</strong></summary>

Scope hoisting is a bundler optimization that flattens all imported modules into a single shared closure or scope instead of wrapping each module in an individual function closure. This eliminates function overhead and reduces bundle size.
</details>

<details>
<summary><strong>6. Why do "barrel files" (index.js exporting hundreds of modules) harm performance?</strong></summary>

They force the bundler to parse and construct ASTs for every re-exported module, slowing build times. If side-effects are not properly declared, bundlers may include unused modules in the final bundle simply because they passed through the barrel file.
</details>

<details>
<summary><strong>7. Can CommonJS code be tree-shaken as effectively as ES Modules?</strong></summary>

No. CommonJS modules execute dynamically at runtime, allowing exports to be added conditionally or modified. Bundlers cannot statically analyze the dependency tree of CJS with absolute certainty without executing the code.
</details>

<details>
<summary><strong>8. How does code splitting interact with HTTP caching?</strong></summary>

Code splitting divides the bundle into smaller, content-hashed chunks (e.g. `vendor-react.abc123.js`). When application code updates, users only re-download the mutated application chunks; long-lived vendor chunks remain served directly from the browser HTTP cache.
</details>

---

## 13. Exercises & Challenges

### Challenge 1: Identify and Fix Tree-Shaking Leaks
Examine the following utility module and explain why importing only `formatDate` still bundles `initDatabase`:

```javascript
// utils.js
export function formatDate(date) {
  return date.toISOString().split('T')[0];
}

export function initDatabase() {
  console.log('Connecting to database...');
}

// Side effect at top level!
export const dbInstance = initDatabase();
```

<details>
<summary>View Solution & Explanation</summary>

**Why it fails:**
Even if the consumer only imports `formatDate`, the top-level statement `export const dbInstance = initDatabase();` executes immediately upon module evaluation. Because `initDatabase()` logs to the console (a side effect), the bundler cannot omit `initDatabase()` or `dbInstance` without changing runtime behavior.

**Fix:**
Make the initialization lazy or annotate it if pure:
```javascript
// utils.js
export function formatDate(date) {
  return date.toISOString().split('T')[0];
}

let dbInstance = null;
export function getDatabase() {
  if (!dbInstance) {
    dbInstance = initDatabase();
  }
  return dbInstance;
}

function initDatabase() {
  console.log('Connecting to database...');
  return { connected: true };
}
```
</details>

### Challenge 2: Configure a Granular Code-Splitting Rule
Write a Rollup/Vite `manualChunks` function that separates `node_modules` dependencies into three distinct bundles:
1. `vendor-core` for `react` and `react-dom`.
2. `vendor-charts` for `chart.js` or `d3`.
3. `vendor-other` for any remaining third-party dependencies.

<details>
<summary>View Solution</summary>

```javascript
function manualChunks(id) {
  if (id.includes('node_modules')) {
    if (id.includes('/react/') || id.includes('/react-dom/')) {
      return 'vendor-core';
    }
    if (id.includes('/chart.js/') || id.includes('/d3/')) {
      return 'vendor-charts';
    }
    return 'vendor-other';
  }
}
```
</details>
