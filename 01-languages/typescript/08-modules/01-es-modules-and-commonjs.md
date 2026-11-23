# ES Modules and CommonJS Interoperability

> Bridging the architectural divide between CommonJS and ES Modules: `esModuleInterop`, default import synthesis, synthetic module records, and dual-package hazards.

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [00-imports-and-exports.md](file:///E:/Learning/engineering-notes/01-languages/typescript/08-modules/00-imports-and-exports.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 2.7 / TS 5.x / TS 7.0 Native |
| **Tested tsconfig** | `esModuleInterop: true`, `moduleResolution: NodeNext` |

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
The JavaScript ecosystem spent a decade running on Node.js's **CommonJS (CJS)** module system (`module.exports = ...`, `const x = require(...)`). Meanwhile, the ECMAScript committee defined **ES Modules (ESM)** with static, asynchronous syntax (`import` / `export`).

The two systems have fundamentally different semantics:
- CommonJS evaluates modules synchronously at runtime; exports are mutable dynamic objects.
- ES Modules are parsed and linked statically in an asynchronous lifecycle; exports are immutable live bindings.

TypeScript introduced flags like **`esModuleInterop`** and **`allowSyntheticDefaultImports`** to allow developers to import legacy CommonJS packages using standard ES module syntax without runtime crashes.

---

## 2. Core Concepts

### CommonJS Module Pattern
```javascript
// legacy-lib.js (CJS)
module.exports = function doWork() { return "done"; };
module.exports.version = "1.0.0";
```

### The ESM Import Problem
In strict ECMAScript specification:
```typescript
import doWork from "./legacy-lib.js";
```
ESM expects the target module to have an explicit export named `default`. In pure CommonJS, there is no `default` property; the exported entity *is* the function itself! Without interoperability shims, `doWork` evaluates to `undefined` at runtime.

### The Solution: `esModuleInterop: true`
When enabled, TypeScript emits helper functions (`__importDefault` and `__importStar`) that inspect the imported CommonJS object:
- If `obj.__esModule` is true, it passes through unmodified.
- If not, it synthesizes a wrapper object: `{ default: obj, ...obj }`.

---

## 3. How It Works Internally

```
Source:
import React from "react";

With esModuleInterop: false (Legacy TS):
var React = require("react"); // If React is module.exports = {}, React.default is missing!

With esModuleInterop: true:
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
var react_1 = __importDefault(require("react"));
```

The helper bridges the gap: accessing `React` translates to `react_1.default`, which points correctly to the CommonJS export object.

---

## 4. Syntax & API Reference

### Compiler Flags
In `tsconfig.json`:
```json
{
  "compilerOptions": {
    "module": "NodeNext",
    "moduleResolution": "NodeNext",
    "esModuleInterop": true,
    "allowSyntheticDefaultImports": true
  }
}
```
*Note: Under `module: NodeNext` or `moduleResolution: NodeNext`, `esModuleInterop` is enabled by default.*

---

## 5. Practical Examples

### 5.1 Importing CommonJS in ESM
```typescript
import express from "express";
import fs from "fs";

const app = express();
app.get("/health", (req, res) => {
  res.json({ ok: true });
});
```

### 5.2 Real-World Application: Dual-Package Safe Consumer
```typescript
import lodash from "lodash";
const { chunk, uniq } = lodash;

const numbers = [1, 2, 2, 3, 4, 4, 5];
const unique = uniq(numbers);
const batches = chunk(unique, 2);
console.log(batches); // [[1, 2], [3, 4], [5]]
```

### 5.3 Edge Case: Calling a CommonJS Callable Default
```typescript
// When a CJS package exports a single function: module.exports = fn;
import debugFactory from "debug";
const debug = debugFactory("app:server");
debug("Server initialized");
```

### 5.4 Failure Case: `import * as` on a Callable Function
```typescript
// In legacy TS without esModuleInterop:
import * as debug from "debug";

// @ts-expect-error TS2349: This expression is not callable. Type 'typeof import("debug")' has no call signatures.
debug("error");
```

### 5.5 Idiomatic Pattern: `export = ` and `import = require()`
For authoring backwards-compatible declarations targeting CommonJS:
```typescript
// types/legacy-tool.d.ts
declare function legacyTool(config: object): void;
declare namespace legacyTool {
  export const version: string;
}
export = legacyTool;

// consumer.ts
import legacyTool = require("legacy-tool");
legacyTool({});
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **The Dual-Package Hazard**: If a package provides both CJS and ESM entry points, and an application imports both, Node.js instantiates two separate instances of the module! Singletons (like database connection pools) will break.
2. **`import * as foo` vs `import foo`**: Never use `import * as foo` if the target module is a function or primitive.
3. **`__esModule` Marker**: Tools like Babel and Webpack emit `Object.defineProperty(exports, "__esModule", { value: true })`. TypeScript's interop helpers explicitly check for this property.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Problem | Solution | Compiler Error |
| :--- | :--- | :--- | :--- |
| Disabling `esModuleInterop` in modern projects | Breaks default imports from CommonJS libraries | Set `"esModuleInterop": true` | `TS1259: Module can only be default-imported using 'esModuleInterop'...` |
| Calling namespace import `import * as fn from ...; fn()` | Not callable under strict ECMAScript specs | Use default import: `import fn from ...` | `TS2349: This expression is not callable` |
| Dual-package hazard in stateful singletons | Two distinct memory copies of module loaded | Enforce single module format in dependencies | Broken singletons / state split |
| Using `require()` inside ESM without `createRequire` | `require` is not defined in ES module scope | Use `createRequire(import.meta.url)` | Runtime `ReferenceError: require is not defined` |
| Forgetting `__esModule` flag when authoring CJS | ESM consumers cannot resolve default export cleanly | Export `default` property explicitly | Default export is undefined |
| Importing JSON without import attributes | Unsafe in modern ESM engines | Use `with { type: "json" }` | `TS2821: Import assertion or attribute required` |

---

## 8. Performance & Type-Checker Complexity
- Emitting interop helpers adds negligible runtime overhead (~5 lines of code per bundle).
- Setting `"importHelpers": true` in `tsconfig.json` delegates these helpers to `tslib`, reducing bundle size across large codebases.

---

## 9. Compiler Internals
- `esModuleInterop` activates `TransformFlags.ContainsESModuleInterop` in the AST emitter.
- In `src/compiler/emitter.ts`, the emitter injects calls to `ts.createImportDefaultHelper` or `ts.createImportStarHelper`.
- If `importHelpers` is active, helpers are emitted as calls to `require("tslib").__importDefault`.

---

## 10. Best Practices & Design Guidelines
- Always enable `esModuleInterop: true` in all modern TypeScript projects.
- Write standard ES module syntax (`import / export`) exclusively in application code.
- If CommonJS interoperability is required in ESM files, use `import { createRequire } from "module"`.

---

## 11. Real-World Case Study
### Safe Migration of Enterprise Express Monorepo from CJS to ESM
An enterprise backend team migrated an Express codebase from CommonJS to ES Modules.

**Problem:** Third-party packages (e.g. `cors`, `cookie-parser`, `multer`) were written in CJS and failed with `TypeError: (0, cors_1.default) is not a function`.

**Solution:**
1. Enabled `"module": "NodeNext"` and `"esModuleInterop": true`.
2. Changed all `import * as cors from "cors"` to `import cors from "cors"`.
3. Added `createRequire` for legacy dynamic configuration loaders:
```typescript
import { createRequire } from "module";
const require = createRequire(import.meta.url);
const plugin = require(process.env.PLUGIN_PATH!);
```

---

## 12. Interview Questions

<details>
<summary>1. What problem does <code>esModuleInterop</code> solve?</summary>

CommonJS modules export an object via `module.exports`, without a dedicated `default` export. Under strict ECMAScript Module specifications, default imports (`import x from "pkg"`) expect a `default` property. `esModuleInterop` emits runtime helper functions that synthesize a `default` property wrapping the CommonJS export object.
</details>

<details>
<summary>2. What is the difference between <code>esModuleInterop</code> and <code>allowSyntheticDefaultImports</code>?</summary>

`allowSyntheticDefaultImports` is purely a type-system flag: it permits writing default imports for modules without an explicit default export, but emits **no runtime JavaScript helpers**. `esModuleInterop` turns on `allowSyntheticDefaultImports` and **emits runtime helper functions** to ensure the code executes safely at runtime.
</details>

<details>
<summary>3. What is the Dual-Package Hazard in Node.js?</summary>

The dual-package hazard occurs when a library publishes both CommonJS and ESM versions, and a project (or its dependencies) loads both. Node.js treats them as two completely separate modules, creating two independent instances with duplicated module-level state and broken `instanceof` checks.
</details>

<details>
<summary>4. Why is <code>import * as express from "express"; express();</code> considered an anti-pattern?</summary>

The ECMAScript specification states that namespace imports (`import * as ns`) represent module namespace objects, which are plain objects and cannot be callable functions. `esModuleInterop` allows writing the spec-compliant `import express from "express"`.
</details>

<details>
<summary>5. How can you use CommonJS <code>require</code> in an ES Module in Node.js?</summary>

```typescript
import { createRequire } from "module";
const require = createRequire(import.meta.url);
const pkg = require("./package.json");
```
</details>

<details>
<summary>6. What does the <code>__esModule</code> property do?</summary>

It is a boolean convention added to transpiled CommonJS modules to tell tools that the module was originally an ES Module, allowing interoperability helpers to distinguish between transpiled ESM and native CJS.
</details>

<details>
<summary>7. What does <code>export =</code> mean in TypeScript?</summary>

`export =` is TypeScript's specific syntax to model CommonJS `module.exports = x`. It exports a single entity directly rather than adding it to an exports dictionary.
</details>

<details>
<summary>8. How does <code>importHelpers: true</code> affect module emit?</summary>

Instead of inlining interop helpers (like `__importDefault` and `__importStar`) into every generated `.js` file, `importHelpers` imports them from the shared runtime package `tslib`, saving bundle size.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Configure CJS Interop (Easy)
Configure `tsconfig.json` to allow default imports from CommonJS modules without compile errors.
<details>
<summary>Solution</summary>

```json
{
  "compilerOptions": {
    "esModuleInterop": true,
    "moduleResolution": "NodeNext"
  }
}
```
</details>

### Exercise 2: Fix Non-Callable Namespace Import (Easy)
Refactor this broken code:
```typescript
import * as morgan from "morgan";
const logger = morgan("combined");
```
<details>
<summary>Solution</summary>

```typescript
import morgan from "morgan";
const logger = morgan("combined");
```
</details>

### Exercise 3: Hybrid CommonJS Loader (Medium)
Create a function in an ES module that safely loads a legacy synchronous CommonJS file using Node's `createRequire`.
<details>
<summary>Solution</summary>

```typescript
import { createRequire } from "module";

function loadLegacyPlugin<T>(specifier: string): T {
  const require = createRequire(import.meta.url);
  return require(specifier) as T;
}
```
</details>

### Exercise 4: Authoring Export = Declaration (Medium)
Write a declaration file for an untyped library that does `module.exports = function(a: string): number`.
<details>
<summary>Solution</summary>

```typescript
// my-lib.d.ts
declare function parseString(input: string): number;
declare namespace parseString {
  export const defaultRadix: number;
}
export = parseString;
```
</details>

### Exercise 5: Universal Consumer for Dual Package (Hard)
Write a utility function that extracts the default export regardless of whether a module was bundled as CJS or ESM.
<details>
<summary>Solution</summary>

```typescript
function resolveDefault<T>(moduleObj: T | { default: T }): T {
  if (moduleObj && typeof moduleObj === "object" && "default" in moduleObj) {
    return (moduleObj as { default: T }).default;
  }
  return moduleObj as T;
}
```
</details>

### Exercise 6: TypeScript Interop Helper Analysis (Hard)
Implement a pure JavaScript reproduction of TypeScript's `__importDefault` and `__importStar` and explain how they prevent nullish reference errors.
<details>
<summary>Solution</summary>

```typescript
function importDefault(mod: any) {
  return (mod && mod.__esModule) ? mod : { default: mod };
}

function importStar(mod: any) {
  if (mod && mod.__esModule) return mod;
  const result: any = {};
  if (mod != null) {
    for (const k of Object.keys(mod)) {
      if (k !== "default" && Object.prototype.hasOwnProperty.call(mod, k)) {
        result[k] = mod[k];
      }
    }
  }
  result.default = mod;
  return result;
}
```
</details>
