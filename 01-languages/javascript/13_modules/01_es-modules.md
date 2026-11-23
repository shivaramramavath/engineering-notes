# ECMAScript Modules (ESM)

> ECMAScript Modules (ESM) provide the official, standardized module system for JavaScript, featuring static dependency graphs, asynchronous module resolution, live read-only bindings, and native browser and server runtime support.

---

## Metadata

| Attribute | Details |
| :--- | :--- |
| **Level** | Core / Intermediate |
| **Prerequisites** | [Scope & Execution Context](../04_scope-and-execution/01_execution-context.md), [Promises](../11_asynchronous-javascript/03_promises.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Standard** | ECMAScript 2015 (ES6) / ES2020 Dynamic Import / ES2022 Top-Level Await / Baseline Widely Available |

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

Before ECMAScript 2015 (ES6), JavaScript possessed no native, standardized language-level module system. The web relied on global script tags (`<script src="...">`), immediate invoked function expressions (IIFE), and community standards like AMD (Asynchronous Module Definition) for browsers and CommonJS for Node.js. These early workarounds suffered from global namespace pollution, lack of static analysis, order-dependent script loading, and inability to perform static tree-shaking.

**ECMAScript Modules (ESM)** introduced a universal, standardized module system built directly into the JavaScript grammar. ESM syntax (`import` and `export`) is statically analyzable, allowing engines and bundlers to determine the complete dependency graph before executing a single line of code. Today, ESM is natively supported across all modern browsers, Node.js, Deno, Bun, and modern build tools.

---

## 2. Core Concepts

### Static Structure
Unlike CommonJS `require()`, which is a runtime function call that can be placed inside conditionals or loops, static `import` and `export` statements must appear at the top-level scope of a module. This invariant allows engines to parse imports and build the module dependency graph without running application code.

### Live Read-Only Bindings
Imports in ESM are not copies or object references: they are **live bindings** (pointers) to the exported variable in the exporting module's scope. If the exporting module mutates the variable internally, the importing module observes the updated value immediately. However, the importer cannot reassign imported identifiers directly (they are read-only).

```text
+-------------------------+                    +-------------------------+
|     counter.js (ESM)    |                    |      main.js (ESM)      |
|                         |                    |                         |
| export let count = 0;   | <--- Live Read --- | import { count }        |
|                         |      Only Binding  | from './counter.js';    |
| export function inc() { |                    |                         |
|   count++;              |                    | count = 10; // ERROR!   |
| }                       |                    | inc();                  |
+-------------------------+                    | console.log(count); // 1|
                                               +-------------------------+
```

### Module Loading Phases
ESM loading is strictly divided into three asynchronous phases:
1. **Construction (Parsing)**: Locate, fetch, and parse all module files into **Module Records**.
2. **Instantiation**: Allocate memory slots for all exports and wire up import bindings (without executing code).
3. **Evaluation**: Execute the module code top-to-bottom, filling the memory slots with actual values.

---

## 3. How It Works Internally

The lifecycle of an ES Module graph execution proceeds through three distinct phases:

```
[ PHASE 1: CONSTRUCTION ]
  Start with entry point (e.g., main.js)
  Fetch source text -> Parse AST -> Identify dependencies
  Recursively fetch and parse child modules
  Build directed acyclic graph of Module Records
             |
             v
[ PHASE 2: INSTANTIATION ]
  Traverse module graph depth-first (post-order)
  Allocate memory space for all exported variables
  Wire importer's import specifiers directly to exporter's memory slots
  (Live bindings established; values remain uninitialized / unassigned)
             |
             v
[ PHASE 3: EVALUATION ]
  Execute module code top-to-bottom (depth-first post-order)
  Populate memory slots with evaluated runtime values
  If top-level await is encountered, pause dependent modules asynchronously
```

---

## 4. Syntax & API Reference

### Export Variations

```javascript
// 1. Named Exports
export const API_URL = 'https://api.example.com';
export function calculate(a, b) { return a + b; }
export class Service {}

// 2. Export List (renaming)
const internalKey = 'secret';
export { internalKey as apiKey };

// 3. Default Export (one per module)
export default class Engine {}
// export default function() {}
// export default 42;

// 4. Re-exporting / Aggregating
export { userModel } from './models/user.js';
export * from './utilities.js';
export * as analytics from './analytics.js'; // Namespace re-export
export { default as BaseLogger } from './logger.js';
```

### Import Variations

```javascript
// 1. Named Imports
import { calculate, API_URL } from './math.js';
import { apiKey as key } from './auth.js';

// 2. Default Import
import Engine from './engine.js';

// 3. Mixed Default and Named
import React, { useState, useEffect } from 'react';

// 4. Namespace Import (Namespace Object)
import * as MathUtils from './math.js';

// 5. Side-Effect Only Import
import './polyfills.js';

// 6. Dynamic Import (returns Promise<Module>)
const module = await import('./heavy-feature.js');
module.run();

// 7. import.meta metadata
console.log(import.meta.url); // File URL of current module
```

---

## 5. Practical Examples

### Example 1: Demonstrating Live Read-Only Bindings
```javascript
// counter.js
export let count = 0;

export function increment() {
  count++;
}

// app.js
import { count, increment } from './counter.js';

console.log('Initial count:', count); // 0
increment();
console.log('Count after increment():', count); // 1 (Live binding updated!)

try {
  count = 99; // TypeError: Assignment to constant variable.
} catch (err) {
  console.error('Cannot reassign imported binding:', err.message);
}
```

### Example 2: Dynamic Code Splitting with `import()`
```javascript
// Lazy loading route handlers or expensive plugins
async function loadAnalyticsDashboard() {
  const loadingSpinner = showSpinner();

  try {
    // Dynamic import fetches chunk over the network only when called
    const { AnalyticsView, renderChart } = await import('./analytics/dashboard.js');
    const view = new AnalyticsView();
    renderChart(view);
  } catch (error) {
    console.error('Failed to load analytics module:', error);
  } finally {
    loadingSpinner.hide();
  }
}
```

### Example 3: Top-Level Await for Asynchronous Module Initialization
```javascript
// dbConnection.js (ES2022 Top-Level Await)
const config = await fetch('/api/db-config').then(r => r.json());

export const connection = await initializeDatabase(config);
console.log('Database initialized successfully');

// app.js
// app.js will automatically wait for dbConnection.js evaluation before running!
import { connection } from './dbConnection.js';

const users = await connection.query('SELECT * FROM users');
console.log('Loaded users:', users.length);
```

### Example 4: Platform-Agnostic Directory Resolution via `import.meta.url`
```javascript
// Node.js ESM replacement for __dirname and __filename
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';
import fs from 'node:fs/promises';

const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);

export async function readPackageManifest() {
  const packagePath = join(__dirname, '../package.json');
  const content = await fs.readFile(packagePath, 'utf8');
  return JSON.parse(content);
}
```

### Example 5: Module Mocking / Swapping Pattern
```javascript
// storage-factory.js
const storageProvider = process.env.STORAGE_TYPE === 's3' 
  ? './providers/s3-storage.js' 
  : './providers/local-storage.js';

export const { StorageService } = await import(storageProvider);
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Mandatory File Extensions in Native ESM**: In standard native ES Modules (browsers and Node.js `"type": "module"`), relative specifiers **must** include the explicit file extension (`import './util.js'`, not `import './util'`). Bundlers allow omitting extensions, but native engines throw `ERR_MODULE_NOT_FOUND`.
2. **Top-Level Await Blocks Dependent Modules**: When module $A$ uses top-level await, any module that imports $A$ is blocked from evaluating until $A$'s await resolves. If a top-level await hangs or rejects, the entire importing subtree stalls or crashes.
3. **Circular Dependencies and Temporal Dead Zone (TDZ)**: If module $A$ imports $B$, and $B$ imports $A$, functions work if declared with `function`, but accessing imported `const` or `let` variables before their module evaluation completes results in `ReferenceError: Cannot access 'x' before initialization`.
4. **Dynamic Import Path Must Be String or Expression**: Static analyzers cannot trace completely dynamic specifiers (`import(variable)`), disabling bundler chunk preloading.

---

## 7. Common Mistakes & Anti-Patterns

| Mistake | Incorrect Pattern | Correct Solution | Why It Fails |
| :--- | :--- | :--- | :--- |
| **Omitting File Extension in Node ESM** | `import { x } from './utils'` | `import { x } from './utils.js'` | Native Node.js ESM requires explicit extensions. |
| **Reassigning Imported Identifier** | `import { count } from './c.js'; count = 5;` | Export setter: `setCount(5)` | Imported bindings are immutable references for consumers. |
| **Using `__dirname` in ESM** | `console.log(__dirname)` | Use `import.meta.url` + `fileURLToPath` | `__dirname` and `__filename` do not exist in ES modules. |
| **Conditional Top-Level Import** | `if (test) import { x } from 'a';` | Use dynamic `await import('a')` | Static `import` declarations cannot be nested in blocks. |
| **Default Export Misalignment** | `import myUtil from './utils.js'` (where `utils.js` used named exports) | `import * as myUtil from './utils.js'` | Default import looks specifically for `export default`. |
| **Mixing ESM and CJS indiscriminately** | `const fs = require('fs')` inside `.mjs` | `import fs from 'node:fs'` | `require` is not defined in ES module scope. |

---

## 8. Performance Characteristics

- **Static Tree-Shaking**: Because module import/export structures are parsed without executing code, bundlers (Vite, Rollup, Webpack) safely prune unused named exports, reducing bundle sizes by 30%–60%.
- **Module Caching**: Module instances are singletons. Subsequent imports of the same specifier return the cached, already-evaluated Module Record with $O(1)$ overhead.
- **Waterfall Network Requests**: In native browser environments without bundlers, deeply nested module graphs cause HTTP request waterfalls. Use HTTP/2 Server Push or bundlers in production.

---

## 9. Engine Internals

1. **Module Records**: In V8, an ES module is parsed into a `SourceTextModuleRecord`. It contains tables for:
   - `[[RequestedModules]]`: List of all imported module specifiers.
   - `[[ImportEntries]]`: Mappings of local variable names to imported module exports.
   - `[[LocalExportEntries]]`: Mappings of internal local variables exposed as exports.
2. **Evaluation State Flags**: A module record transitions through `unlinked` $\rightarrow$ `linking` $\rightarrow$ `linked` $\rightarrow$ `evaluating` $\rightarrow$ `evaluated`.
3. **Top-Level Await Implementation**: V8 treats any module with top-level await as an implicit async function, wrapping the module evaluation in an internal Promise that propagates up the dependency tree.

---

## 10. Best Practices & Design Guidelines

1. **Favor Named Exports over Default Exports**: Named exports provide clearer auto-imports in IDEs, protect against accidental renaming, and tree-shake more reliably.
2. **Always Use Explicit Extensions in Internal Imports**: Write `./module.js` to ensure compatibility across Node.js, browsers, and standard tools.
3. **Use Dynamic Imports for Heavy or Rare Routes**: Defer downloading non-critical features until needed via `await import()`.
4. **Use Top-Level Await Sparingly**: Limit top-level await to essential startup configurations (database handles, WASM compilation); never use it for slow or non-critical network queries.

---

## 11. Real-World Case Study

### Universal Isomorphic Configuration Loader

A modern library must run seamlessly across browsers and Node.js without duplicate code or runtime errors, dynamically reading configuration while preserving tree-shaking.

```javascript
// config-loader.js
import { fileURLToPath } from 'node:url';
import { dirname, resolve } from 'node:path';

class ConfigManager {
  #config = null;

  async init() {
    if (this.#config) return this.#config;

    // Isomorphic environment detection
    const isNode = typeof process !== 'undefined' && process.versions?.node;

    if (isNode) {
      // In Node.js, read from local filesystem using import.meta.url
      const fs = await import('node:fs/promises');
      const __dirname = dirname(fileURLToPath(import.meta.url));
      const configPath = resolve(__dirname, '../app.config.json');
      const raw = await fs.readFile(configPath, 'utf8');
      this.#config = JSON.parse(raw);
    } else {
      // In browser, fetch over HTTP
      const response = await fetch('/api/app.config.json');
      this.#config = await response.json();
    }

    return this.#config;
  }

  get(key) {
    if (!this.#config) throw new Error('Config not initialized. Call init() first.');
    return this.#config[key];
  }
}

export const config = new ConfigManager();
```

---

## 12. Interview Questions

<details>
<summary><strong>1. What is the fundamental difference between ESM live bindings and CommonJS value exports?</strong></summary>

CommonJS exports copy values (or object references) at the time of assignment; mutating the exported variable inside the original module does not update the imported variable. ESM imports are **live bindings** (direct pointers to the exporter's memory slot); if the exporting module changes the value, the importer immediately observes the change.
</details>

<details>
<summary><strong>2. Can you modify an imported variable in the importing module?</strong></summary>

No. Imported bindings are strictly read-only. Attempting to reassign an imported binding (`count = 5`) throws a `TypeError: Assignment to constant variable`.
</details>

<details>
<summary><strong>3. What are the three phases of ES Module loading?</strong></summary>

1. **Construction**: Finding, downloading, and parsing all module files into Module Records.
2. **Instantiation**: Allocating memory locations for exports and linking import bindings to export memory slots.
3. **Evaluation**: Executing module code statements top-to-bottom to populate memory slots with actual values.
</details>

<details>
<summary><strong>4. Why do ES Modules enable better tree-shaking than CommonJS?</strong></summary>

Because ESM `import` and `export` statements are static and top-level. The engine or bundler can construct the complete dependency graph and detect unused exports at build time without executing any code. CommonJS `require()` is dynamic, so exports cannot be guaranteed unused until runtime.
</details>

<details>
<summary><strong>5. What is <code>import.meta</code>?</strong></summary>

`import.meta` is a host-populated object containing context-specific metadata about the currently executing module. Its most common property is `import.meta.url`, which returns the absolute file or HTTP URL of the module.
</details>

<details>
<summary><strong>6. How does top-level <code>await</code> affect module evaluation order?</strong></summary>

When a module uses top-level await, its evaluation becomes asynchronous. Any module that imports it will wait for the awaited promise to resolve before executing its own evaluation phase.
</details>

<details>
<summary><strong>7. How do you handle circular dependencies in ES Modules?</strong></summary>

Circular dependencies work in ESM due to the separation of instantiation and evaluation. However, if module $A$ evaluates and accesses a `const` or `let` from module $B$ before $B$ has finished evaluating, a TDZ (Temporal Dead Zone) `ReferenceError` will be thrown. The solution is to use functions (which are hoisted) or refactor shared dependencies into a third module.
</details>

<details>
<summary><strong>8. What is the return type of a dynamic <code>import()</code> call?</strong></summary>

It returns a `Promise` that resolves to the module namespace object containing all named exports as properties and the default export under the `default` key.
</details>

---

## 13. Exercises & Challenges

### Challenge 1: Circular Dependency Debugging
Identify why the following ESM code throws a `ReferenceError` and refactor it so both modules work properly:

```javascript
// a.js
import { b } from './b.js';
export const a = 'A';
console.log('Value of b in a.js:', b);

// b.js
import { a } from './a.js';
export const b = 'B';
console.log('Value of a in b.js:', a);
```

<details>
<summary>View Solution & Explanation</summary>

**Why it fails:**
When `a.js` is loaded, it instantiates and begins evaluating. It immediately imports `b.js`. In `b.js`, it attempts to evaluate `console.log(a)`. Because `a.js` has not reached `export const a = 'A'`, variable `a` is still in the Temporal Dead Zone (TDZ), throwing `ReferenceError: Cannot access 'a' before initialization`.

**Refactored Solution (using hoisted function declarations or lazy getters):**
```javascript
// a.js
import { getB } from './b.js';
export function getA() { return 'A'; }
console.log('Value of b in a.js:', getB());

// b.js
import { getA } from './a.js';
export function getB() { return 'B'; }
console.log('Value of a in b.js:', getA());
```
</details>

### Challenge 2: Dynamic Feature Plugin Loader
Implement an async function `loadPlugins(pluginNames)` that takes an array of module specifiers, dynamically imports each using `Promise.allSettled`, and returns an array of successfully loaded plugin instances that implement an `initialize()` method.

<details>
<summary>View Solution</summary>

```javascript
async function loadPlugins(pluginNames) {
  const loadTasks = pluginNames.map(async (name) => {
    const module = await import(name);
    const PluginClass = module.default ?? module.Plugin;
    if (typeof PluginClass !== 'function') {
      throw new Error(`Module ${name} does not export a valid plugin class`);
    }
    const instance = new PluginClass();
    await instance.initialize?.();
    return { name, instance };
  });

  const results = await Promise.allSettled(loadTasks);
  const activePlugins = [];

  for (const result of results) {
    if (result.status === 'fulfilled') {
      activePlugins.push(result.value);
    } else {
      console.warn('Failed to load plugin:', result.reason);
    }
  }

  return activePlugins;
}
```
</details>
