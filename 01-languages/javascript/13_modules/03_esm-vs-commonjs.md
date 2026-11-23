# ESM vs CommonJS: Interoperability & Migration

> Bridging ECMAScript Modules and CommonJS requires understanding their conflicting loading models, navigating Node.js interop rules, avoiding the dual-package hazard, and executing safe migration strategies.

---

## Metadata

| Attribute | Details |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [ES Modules](./01_es-modules.md), [CommonJS](./02_commonjs.md) |
| **Time to Read** | 24 minutes |
| **Status** | Complete |
| **Standard** | Node.js Module Interoperability Specification / ECMAScript Module Loading |

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

The JavaScript ecosystem currently exists in a hybrid state. While ECMAScript Modules (ESM) represent the modern official language standard supported universally across browsers and runtimes, millions of production backend repositories and npm dependencies were authored using Node's legacy CommonJS (CJS) specification.

Integrating these two systems is one of the most notoriously complex challenges in modern JavaScript engineering. ESM is asynchronous, parsed statically before execution, and passes live bindings. CommonJS is synchronous, executed at runtime, and returns exported object copies. Attempting to use them together without understanding their structural boundaries causes errors such as `ERR_REQUIRE_ESM`, broken default imports, runtime crashes, and split module state (the "dual-package hazard").

---

## 2. Core Concepts

### Architectural Comparison Matrix

| Dimension | CommonJS (CJS) | ECMAScript Modules (ESM) |
| :--- | :--- | :--- |
| **Loading Model** | Synchronous (blocks execution) | Asynchronous (3 distinct phases) |
| **Timing** | Dynamic at runtime | Static at parse/compile time |
| **Binding Type** | Value copy / object reference | Live read-only reference binding |
| **Scope Primitives**| `__filename`, `__dirname`, `require`, `module` | `import.meta.url` (No `__dirname`/`__filename`) |
| **Top-Level Await** | Not supported | Supported natively (ES2022) |
| **Tree-Shaking** | Extremely limited / heuristic | Full static dead-code elimination |
| **Browser Native** | No (requires bundler) | Yes (`<script type="module">`) |
| **File Extensions** | Optional (`.js`, `.json`, directory index) | Strictly required in native Node/browser |

### Interoperability Rules in Node.js

Node.js permits interoperability between ESM and CJS under specific rules:
1. **ESM can import CommonJS**:
   - `import cjsModule from './legacy.cjs'` works. Node automatically wraps the CJS `module.exports` as the `default` export of a synthetic ESM namespace.
   - Named imports (`import { foo } from './legacy.cjs'`) work *only* if Node's static lexical scanner can detect named exports on `module.exports`.
2. **CommonJS cannot synchronously `require()` ES Modules**:
   - Calling `require('./modern.mjs')` throws `ERR_REQUIRE_ESM` (because ESM may contain top-level await and cannot be evaluated synchronously).
   - *Exception*: In modern Node.js (v22+ with `--experimental-require-module`), synchronous `require()` of synchronous ESM graphs is supported.
   - Universal cross-version solution: CJS must use dynamic `import('./modern.mjs')` to load ESM asynchronously.

```text
+---------------------+                      +---------------------+
|   ESM Environment   |                      |   CJS Environment   |
+---------------------+                      +---------------------+
          │                                             │
          │ import pkg from 'cjs'                       │ require('esm')  --> THROWS ERR_REQUIRE_ESM!
          ▼                                             │
+---------------------+                                 │ import('esm')   --> WORKS! (Returns Promise)
| Node Synthetic ESM  |                                 ▼
| namespace:          |                      +---------------------+
| { default: exports }|                      | Promise<Module>     |
+---------------------+                      +---------------------+
```

---

## 3. How It Works Internally

### Node.js CJS-ESM Bridge Architecture

When an ES Module imports a CommonJS module, Node.js constructs a **Synthetic Module Record**:

```
 [ ESM Script imports CommonJS module: import utils from './utils.cjs' ]
                                     |
                                     v
 1. Node identifies target file is CommonJS (.cjs or package "type": "commonjs")
                                     |
                                     v
 2. Executes CJS file via classic loader:
    const cjsExports = require('./utils.cjs');
                                     |
                                     v
 3. Runs CJS-Module-Lexer (C++ scanner) over the CJS source text:
    - Scans for static patterns: exports.add = ..., module.exports = { a, b }
    - Identifies candidate named export keys
                                     |
                                     v
 4. Instantiates a SyntheticModuleRecord:
    - Sets default export: SyntheticRecord.default = cjsExports
    - For each scanned key, binds SyntheticRecord[key] = cjsExports[key]
                                     |
                                     v
 5. Wires the Synthetic Module Record into the ESM dependency graph
```

---

## 4. Syntax & API Reference

### Consuming CJS from ESM

```javascript
// 1. Default import (Always safe for CJS)
import cjsPackage from 'cjs-only-package';

// 2. Named imports (Safe if static scanner recognizes properties)
import { query, connect } from 'database-driver';

// 3. Creating a synchronous require function inside an ES Module
import { createRequire } from 'node:module';
const require = createRequire(import.meta.url);

// Now you can synchronously require CJS, JSON, or native .node addons!
const legacyConfig = require('./legacy-config.json');
```

### Consuming ESM from CJS

```javascript
// Inside a CommonJS module (.cjs):
async function run() {
  // Synchronous require(esm) will crash with ERR_REQUIRE_ESM
  // MUST use dynamic import:
  const { default: modernLib, feature } = await import('modern-esm-package');
  modernLib.execute();
}
```

### `package.json` Conditional Exports Configuration

```json
{
  "name": "my-isomorphic-library",
  "version": "1.0.0",
  "type": "module",
  "exports": {
    ".": {
      "import": {
        "types": "./dist/index.d.ts",
        "default": "./dist/index.js"
      },
      "require": {
        "types": "./dist/index.d.cts",
        "default": "./dist/index.cjs"
      }
    }
  }
}
```

---

## 5. Practical Examples

### Example 1: Creating a Synchronous `require` inside ESM
```javascript
import { createRequire } from 'node:module';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

// Emulate CJS require within native ESM
const require = createRequire(import.meta.url);

// Synchronously load JSON without experimental JSON import attributes
const manifest = require('../package.json');
console.log('App version:', manifest.version);

// Synchronously load CJS package that lacks proper ESM named exports
const legacyLib = require('legacy-crypto-util');
```

### Example 2: Emulating `__filename` and `__dirname` in ESM
```javascript
import { fileURLToPath } from 'node:url';
import { dirname } from 'node:path';

// Standard boilerplate for ESM modules needing file paths
const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);

console.log('Absolute file:', __filename);
console.log('Absolute directory:', __dirname);
```

### Example 3: The Dual-Package Hazard Demonstration
```javascript
// state.cjs
let count = 0;
module.exports = {
  inc: () => count++,
  get: () => count,
};

// state.mjs (Dual package exporting both ESM and CJS formats naively)
let count = 0;
export const inc = () => count++;
export const get = () => count;

// If consumer A imports state.mjs and consumer B requires state.cjs:
// TWO DISTINCT INSTANCES EXIST IN MEMORY!
// Consumer A: inc() -> ESM count becomes 1
// Consumer B: get() -> CJS count is still 0! Split-brain state bug!
```

### Example 4: Solving the Dual-Package Hazard (Single State Wrapper)
```javascript
// To avoid the dual-package hazard, ALWAYS make the ESM wrapper delegate
// directly to the CommonJS implementation for shared state singletons:

// index.cjs (The source of truth)
let sharedState = 0;
module.exports = {
  increment: () => ++sharedState,
  getValue: () => sharedState,
};

// index.mjs (The ESM wrapper forwarding to CJS)
import { createRequire } from 'node:module';
const require = createRequire(import.meta.url);

const cjs = require('./index.cjs');

export const increment = cjs.increment;
export const getValue = cjs.getValue;
export default cjs;
// Both ESM and CJS consumers now share the EXACT SAME state singleton!
```

### Example 5: Dynamic Import Feature Polyfill in CJS
```javascript
// server.cjs
const express = require('express');
const app = express();

app.get('/render', async (req, res) => {
  try {
    // Dynamically import ESM-only markdown parser inside CJS route
    const { marked } = await import('marked');
    const html = marked('# Hello from ESM inside CJS');
    res.send(html);
  } catch (err) {
    res.status(500).send(err.message);
  }
});
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **`ERR_REQUIRE_ESM`**: Attempting to synchronously `require()` an ESM module from CommonJS throws `Error [ERR_REQUIRE_ESM]: require() of ES Module not supported`. You must either switch the consumer to ESM or use dynamic `import()`.
2. **Missing Named Exports on CJS**: When importing a CJS module from ESM, if the CJS module assigns an object dynamically (e.g. `module.exports = getExports()`), Node's lexical scanner cannot detect named exports. `import { foo }` throws `SyntaxError`. The fix is to use default import: `import pkg from './lib.cjs'; const { foo } = pkg;`.
3. **The Dual-Package Split-Brain Hazard**: If an npm package provides both CJS and ESM builds, and an application's dependency tree loads both versions, libraries maintaining internal singletons (like React context, loggers, or database connections) get instantiated twice, breaking equality checks (`instanceof`) and state synchronization.
4. **JSON File Loading**: In ESM, importing JSON via `import data from './data.json'` requires Import Attributes: `import data from './data.json' with { type: 'json' };` (ES2025/Node 20+). Alternatively, use `createRequire`.

---

## 7. Common Mistakes & Anti-Patterns

| Mistake | Incorrect Pattern | Correct Solution | Why It Fails |
| :--- | :--- | :--- | :--- |
| **Synchronous require of ESM** | `const esm = require('esm-pkg')` | `const esm = await import('esm-pkg')` | Throws `ERR_REQUIRE_ESM`. |
| **Using `__dirname` in ESM** | `const p = path.join(__dirname, 'f')` | Use `import.meta.url` + `dirname` | `__dirname` is not defined in ESM scope. |
| **Default Export Confusion** | `import cjs from 'cjs'` expecting `.default` | Node maps `module.exports` directly to `default` | In ESM, `cjs.default` is often undefined; use `cjs` directly. |
| **Naive Dual-Package Publishing** | Compiling both CJS and ESM with independent state | Export ESM wrapper that re-exports CJS singleton | Duplicates singleton state across runtime instances. |
| **Omitting Extensions in ESM Imports**| `import { x } from './utils'` | `import { x } from './utils.js'` | Throws `ERR_MODULE_NOT_FOUND` in standard Node ESM. |
| **Unconditional Named Imports from CJS**| `import { dynamicMethod } from 'dyn-cjs'` | `import pkg from 'dyn-cjs'; const { dynamicMethod } = pkg;` | Node's CJS lexer cannot find dynamically generated properties. |

---

## 8. Performance Characteristics

- **Dynamic `import()` Latency**: Dynamic imports return a Promise and require at least one microtask tick to resolve, adding slight overhead compared to synchronous `require()` cache lookups.
- **CJS Lexer Overhead**: When Node.js loads a CommonJS file from an ES Module, it invokes `cjs-module-lexer` written in fast C++/WebAssembly, adding negligible overhead ($< 1\text{ms}$ per file).
- **Bundle Bloat from Dual Packages**: Bundling an application that pulls in both CJS and ESM builds of the same package duplicates code in the final bundle, increasing payload size.

---

## 9. Engine Internals

1. **`cjs-module-lexer`**: Node.js embeds a fast regex-based C++ lexer to detect named exports in CommonJS files at startup without executing the code. It scans for patterns like `exports.foo = ...` and `module.exports = { a, b }`.
2. **Experimental `require(esm)`**: Node.js v22.12.0+ includes `--experimental-require-module`. When enabled, if an ES Module does **not** contain top-level await, Node allows synchronous `require()` of that ESM file, converting the module evaluation into a synchronous execution step.

---

## 10. Best Practices & Design Guidelines

1. **Migrate to ESM ("type": "module") for New Projects**: Set `"type": "module"` in `package.json` for all modern applications.
2. **Use Conditional Exports in Library `package.json`**: Define explicit `"import"` and `"require"` targets in the `"exports"` map.
3. **Always Anchor State in Dual Packages**: If maintaining dual packages, ensure stateful singletons reside in a shared CommonJS file that the ESM bundle imports.
4. **Prefer `node:` URL Prefix**: Always write `import 'node:fs'` and `require('node:fs')` to avoid third-party module spoofing.

---

## 11. Real-World Case Study

### Safely Migrating a Legacy Express Backend to Native ESM

An enterprise Node.js backend with 200 CommonJS controllers is migrated to native ESM without breaking existing CJS plugins or suffering downtime.

```json
// package.json
{
  "name": "enterprise-api",
  "version": "2.0.0",
  "type": "module",
  "scripts": {
    "start": "node server.js"
  }
}
```

```javascript
// server.js (Native ESM entry point)
import express from 'express';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';
import { createRequire } from 'node:module';

const __dirname = dirname(fileURLToPath(import.meta.url));
const require = createRequire(import.meta.url);

const app = express();

// 1. Native ESM Middleware
import { authMiddleware } from './middleware/auth.js';
app.use(authMiddleware);

// 2. Safely Loading Legacy CJS Plugins
function loadLegacyPlugin(pluginName) {
  // Use createRequire to safely load legacy plugins synchronously
  const plugin = require(`./legacy-plugins/${pluginName}.cjs`);
  plugin.register(app);
}

loadLegacyPlugin('audit-logger');

// 3. Top-Level Await Database Initialization
import { databasePool } from './db.js';
await databasePool.connect();
console.log('Connected to database cluster via top-level await');

app.listen(3000, () => {
  console.log('Server running in native ESM mode on port 3000');
});
```

---

## 12. Interview Questions

<details>
<summary><strong>1. Why does <code>require('./my-module.mjs')</code> throw an error in Node.js?</strong></summary>

Because ES Modules support asynchronous loading and Top-Level Await. `require()` is fundamentally synchronous; if an imported ESM file uses top-level await, Node.js cannot synchronously return an exported value without halting the entire thread, which the engine disallows.
</details>

<details>
<summary><strong>2. What is the Dual-Package Hazard?</strong></summary>

It occurs when an npm package publishes both CommonJS and ES Module versions, and an application or its dependencies inadvertently load both versions into the same process. This creates two separate instances of the module in memory, duplicating state, singletons, and breaking `instanceof` identity checks.
</details>

<details>
<summary><strong>3. How does Node.js support named imports from CommonJS modules in ESM?</strong></summary>

Node runs a fast C++ static analysis tool (`cjs-module-lexer`) on the CommonJS source text before execution to detect common named export patterns (`exports.name = ...`). If recognized, it creates named bindings on the synthetic ESM module record.
</details>

<details>
<summary><strong>4. How do you import a JSON file in native ESM?</strong></summary>

Using Import Attributes (Node 20+):
```javascript
import config from './config.json' with { type: 'json' };
```
Or using `createRequire`:
```javascript
import { createRequire } from 'node:module';
const require = createRequire(import.meta.url);
const config = require('./config.json');
```
</details>

<details>
<summary><strong>5. What is the difference between <code>import * as pkg from 'cjs'</code> and <code>import pkg from 'cjs'</code>?</strong></summary>

When importing CommonJS from ESM, `import pkg from 'cjs'` assigns `module.exports` to `pkg`. `import * as pkg from 'cjs'` returns the synthetic namespace object where named exports are top-level properties and `module.exports` is also available under `pkg.default`.
</details>

<details>
<summary><strong>6. How can a CommonJS module asynchronously load an ES Module?</strong></summary>

By using the dynamic `import()` expression:
```javascript
const { default: esmMod } = await import('esm-package');
```
</details>

<details>
<summary><strong>7. How does the <code>"exports"</code> field in <code>package.json</code> differ from the legacy <code>"main"</code> field?</strong></summary>

`"main"` only allows defining a single entry point for all consumers. `"exports"` allows conditional resolution based on the consumer's environment (`"import"` for ESM, `"require"` for CJS, `"browser"` vs `"node"`), and encapsulates internal package files by blocking unauthorized subpath imports.
</details>

<details>
<summary><strong>8. Can an ES Module be executed inside a CommonJS environment without modifying code?</strong></summary>

In Node.js v22.12.0+ with the `--experimental-require-module` flag enabled, synchronous ES modules (modules without top-level await) can be directly required via `require('esm-module')`.
</details>

---

## 13. Exercises & Challenges

### Challenge 1: Construct a Universal Conditional Package Manifest
Author a complete `package.json` for an open-source library named `"fast-cache"` that supports both TypeScript types, native ESM consumers, legacy CommonJS consumers, and prevents unauthorized internal subpath access.

<details>
<summary>View Solution</summary>

```json
{
  "name": "fast-cache",
  "version": "1.0.0",
  "type": "module",
  "main": "./dist/index.cjs",
  "module": "./dist/index.js",
  "types": "./dist/index.d.ts",
  "exports": {
    ".": {
      "types": {
        "import": "./dist/index.d.ts",
        "require": "./dist/index.d.cts"
      },
      "import": "./dist/index.js",
      "require": "./dist/index.cjs"
    },
    "./package.json": "./package.json"
  },
  "files": [
    "dist"
  ]
}
```
</details>

### Challenge 2: Safe Cross-Format Loader Utility
Write a utility function `loadModule(specifier)` that safely loads a module regardless of whether the host environment is running in CJS or ESM mode, and returns the normalized exported interface.

<details>
<summary>View Solution</summary>

```javascript
async function loadModule(specifier) {
  try {
    // Dynamic import works across both ESM and modern CommonJS runtimes
    const mod = await import(specifier);
    // Normalize: prefer default export if present, otherwise namespace
    return mod.default ?? mod;
  } catch (err) {
    if (err.code === 'ERR_VM_DYNAMIC_IMPORT_CALLBACK_MISSING') {
      // In older environments without dynamic import in vm/CJS
      const { createRequire } = await import('node:module');
      const require = createRequire(import.meta.url);
      return require(specifier);
    }
    throw err;
  }
}
```
</details>
