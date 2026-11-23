# Module Resolution and Paths in TypeScript

> Master guide to TypeScript module resolution: `NodeNext`, `Bundler`, `Classic`, `baseUrl`, `paths` alias mapping, package.json `exports` resolution, and subpath imports.

| Metadata | Details |
| :--- | :--- |
| **Category** | Compiler & Configuration |
| **Difficulty** | Intermediate |
| **Prerequisites** | [08-modules/00-imports-and-exports.md](../08-modules/00-imports-and-exports.md), [13-compiler-and-tsconfig/02-target-module-and-lib.md](./02-target-module-and-lib.md) |
| **TypeScript Baseline** | TS 4.7+ (`NodeNext`), TS 5.0+ (`moduleResolution: bundler`) |
| **Tested tsconfig** | `"strict": true, "target": "ES2022", "moduleResolution": "NodeNext"` |

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
Module resolution is the algorithm TypeScript uses to determine what file a given module specifier string (e.g. `import { x } from './utils'` or `import express from 'express'`) points to on the filesystem.

Historically, TypeScript only supported `Node10` (legacy Node CommonJS directory and index scanning) and `Classic`. In modern JavaScript, Node.js introduced conditional `package.json` `exports`, ESM file extension requirements, and subpath imports (`#internal`). Concurrently, modern bundlers (Vite, webpack, esbuild) allow omitting extensions while understanding package exports. TypeScript 4.7 introduced `NodeNext` and TypeScript 5.0 introduced `moduleResolution: "bundler"` to accurately model these distinct ecosystems.

---

## 2. Core Concepts
- **`moduleResolution` Strategies**:
  - `NodeNext` / `Node16`: Faithfully mirrors modern Node.js resolution. Relative imports *must* have explicit `.js` extensions. Understands `package.json` `"exports"` and `"imports"`.
  - `Bundler`: Introduced in TS 5.0. Designed for modern bundlers (Vite, Rollup, webpack 5). Supports `package.json` `"exports"` without requiring explicit file extensions on relative imports.
  - `Node10` (formerly `node`): Legacy CommonJS resolution. Scans `node_modules`, searches for `index.ts`, and ignores modern `package.json` `"exports"`.
  - `Classic`: Obsolete legacy TypeScript strategy (fallback if not configured).
- **`paths` Mapping**: Maps alias prefixes (`@app/*`) to physical directories (`src/*`). Used strictly by the type checker; does NOT rewrite import strings during emit.
- **Node.js Subpath Imports (`#`)**: Native Node.js feature defined in `package.json` (`"imports": { "#utils/*": "./dist/utils/*.js" }`). Both Node.js and TypeScript (`NodeNext` / `Bundler`) natively support this without any `paths` configuration.

---

## 3. How It Works Internally

```
                import { User } from './models/user.js';
                                |
                                v
               Is moduleResolution 'NodeNext'?
                                |
               +----------------+----------------+
               | (Yes)                           | (Bundler)
               v                                 v
   Strip .js extension               Look for user.ts, user.tsx,
   Look for ./models/user.ts         or ./models/user/index.ts
   or ./models/user.d.ts             directly
               |                                 |
               +----------------+----------------+
                                |
                                v
                Resolved Source File in Program
```

In `NodeNext`, TypeScript maps the requested `.js` extension back to the corresponding `.ts`, `.tsx`, or `.d.ts` source file on disk. When resolving non-relative package imports (`import lodash from 'lodash'`), the resolver walks parent directories looking for `node_modules`, reads `package.json`, checks `"exports"`, and selects the appropriate condition (`"types"`, `"import"`, or `"require"`).

---

## 4. Syntax & API Reference

```json
// 1. Modern Node.js Service (NodeNext)
{
  "compilerOptions": {
    "module": "NodeNext",
    "moduleResolution": "NodeNext"
  }
}

// 2. Modern Frontend App (Bundler)
{
  "compilerOptions": {
    "module": "ESNext",
    "moduleResolution": "bundler"
  }
}

// 3. package.json native subpath imports (Universal)
{
  "imports": {
    "#services/*": {
      "types": "./src/services/*.ts",
      "default": "./dist/services/*.js"
    }
  }
}
```

---

## 5. Practical Examples

### Example 1: Relative Import under `NodeNext`
```typescript
// File: src/math.ts
export const add = (a: number, b: number) => a + b;

// File: src/main.ts
// Must use .js extension even though math.ts is a TypeScript file!
import { add } from './math.js';
console.log(add(2, 3));
```

### Example 2: Modern `package.json` `"exports"` with Dedicated Types
```json
{
  "name": "my-library",
  "type": "module",
  "exports": {
    ".": {
      "types": "./dist/index.d.ts",
      "import": "./dist/index.js",
      "require": "./dist/index.cjs"
    },
    "./utils": {
      "types": "./dist/utils.d.ts",
      "import": "./dist/utils.js"
    }
  }
}
```

### Example 3: Edge Case - TypeScript Mapping `.js` to `.ts`
```typescript
// When writing:
import { helper } from './helpers.js';
// TypeScript looks for:
// 1. src/helpers.ts
// 2. src/helpers.tsx
// 3. src/helpers.d.ts
// 4. src/helpers.js (if allowJs is true)
```

### Example 4: Compiler Failure Case - Missing Extension in `NodeNext`
```typescript
// Under "moduleResolution": "NodeNext"
// @ts-expect-error TS2835: Relative import paths need explicit file extensions in EcmaScript imports when '--moduleResolution' is 'node16' or 'nodenext'. Did you mean './utils.js'?
import { format } from './utils';
```

### Example 5: Clean Subpath Imports without `paths`
```json
// package.json:
{
  "name": "my-app",
  "imports": {
    "#db": "./src/db/client.ts"
  }
}

// Any file in src/:
import { db } from '#db'; // Resolves both in tsc (NodeNext/Bundler) and native Node.js!
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **The `.js` Extension Paradox**: In `NodeNext`, you must write `import from './file.js'`, even though the file on disk is `file.ts` and `file.js` does not exist yet! This is because `tsc` preserves the import specifier verbatim in the emitted `.js` file, and Node.js requires `.js` at runtime.
2. **`paths` Breakage in Production**: Configuring `"paths": { "@app/*": ["src/*"] }` allows `tsc` to find types, but `node dist/main.js` will crash with `ERR_MODULE_NOT_FOUND` because `paths` does not alter emitted JS code.
3. **Condition Order in `"exports"`**: In `package.json` `"exports"`, the `"types"` key **must come first** before `"import"` or `"require"`. If `"import"` comes first, TypeScript's resolver may skip `"types"`.
4. **`moduleResolution: bundler` with `tsc` Emit**: `moduleResolution: bundler` is designed for projects using `"noEmit": true`. If you use it while emitting with `tsc`, emitted imports may not resolve cleanly in standard Node.js.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Consequence | Correction |
| :--- | :--- | :--- |
| Omitting `.js` in `NodeNext` imports | **TS2835**: Relative paths need explicit file extensions | Add `.js` extension to import |
| Using `paths` to create aliases for Node.js | Runtime `Cannot find module` error | Use native `package.json` `"imports"` (`#...`) |
| Putting `"types"` after `"import"` in package.json | TypeScript ignores package types | Always declare `"types"` first |
| Using `moduleResolution: "node"` in modern apps | Ignores `package.json` `"exports"` | Use `NodeNext` or `bundler` |
| Importing private un-exported package paths | Bypasses library encapsulation; breaks in updates | Import only from paths declared in `"exports"` |

---

## 8. Performance & Type-Checker Complexity
- Resolving module paths involves filesystem directory walking and `stat` calls. TypeScript caches resolution results in an internal `ResolutionCache`.
- Restricting `paths` mappings and using native `package.json` subpath imports minimizes directory probing and improves compilation speed.

---

## 9. Compiler Internals
In TypeScript's `src/compiler/moduleNameResolver.ts`:
- `nodeModuleNameResolver` implements the Node.js resolution spec.
- When resolving package paths, `loadNodeModulesFromDirectory` reads `package.json` and evaluates the `"exports"` field against active conditions (`types`, `import`, `require`, `node`, `default`).
- Diagnostic `TS2835` is triggered in `NodeNext` if an import specifier lacks an extension.

---

## 10. Best Practices & Design Guidelines
- Use `moduleResolution: "NodeNext"` for Node.js services and npm libraries.
- Use `moduleResolution: "bundler"` for Vite, Next.js, and browser SPAs with `"noEmit": true`.
- Prefer native `package.json` `"imports"` (`#utils`) over `tsconfig.json` `paths`.
- Always put `"types"` as the very first condition in package.json `"exports"`.
- Use `tsc --traceResolution` when debugging obscure module import failures.

---

## 11. Real-World Case Study
A company published an open-source TypeScript SDK. Downstream consumers using Node.js ESM reported `ERR_PACKAGE_PATH_NOT_EXPORTED`, while TypeScript users reported `Could not find a declaration file for module 'my-sdk'`.

**Audit of `package.json` `"exports"`:**
```json
// BROKEN:
"exports": {
  ".": {
    "import": "./dist/index.js",
    "types": "./dist/index.d.ts"
  }
}
```
Because `"import"` was placed before `"types"`, Node's resolver matched `"import"` first and halted, causing TypeScript to miss the types.

**Remediation:**
```json
// FIXED:
"exports": {
  ".": {
    "types": "./dist/index.d.ts",
    "import": "./dist/index.js",
    "require": "./dist/index.cjs"
  }
}
```
All downstream ESM, CommonJS, and TypeScript consumers resolved cleanly without errors.

---

## 12. Interview Questions

<details>
<summary>1. Why must you write `.js` in relative imports when using `moduleResolution: "NodeNext"` on TypeScript files?</summary>

Because TypeScript's emitter preserves relative module specifiers verbatim into the generated `.js` files. Node.js native ESM mandates that all relative import specifiers include an explicit file extension at runtime.
</details>

<details>
<summary>2. What is the difference between `moduleResolution: "bundler"` and `moduleResolution: "NodeNext"`?</summary>

`NodeNext` strictly enforces Node.js runtime rules, including mandatory file extensions and format boundaries (.cjs vs .mjs). `bundler` allows omitting file extensions and resolving directory index files while still supporting `package.json` `"exports"`, mirroring bundlers like Vite and webpack.
</details>

<details>
<summary>3. Why is `package.json` `"imports"` preferred over `tsconfig.json` `paths`?</summary>

`package.json` `"imports"` (subpath imports like `#utils`) is a native Node.js and web standard supported by runtimes and bundlers. `tsconfig.json` `paths` is only a compiler type-checking alias and does not rewrite imports in emitted JavaScript.
</details>

<details>
<summary>4. Why must `"types"` always be the first key in conditional `"exports"`?</summary>

The ECMAScript / Node.js conditional export specification processes keys in declaration order. If an environment supports both `"import"` and `"types"`, placing `"import"` first will cause the resolver to match `"import"` and stop, ignoring the subsequent `"types"` entry.
</details>

<details>
<summary>5. What does the `tsc --traceResolution` flag do?</summary>

It prints a comprehensive, step-by-step diagnostic trace of every directory probed, `package.json` inspected, and condition evaluated during module resolution.
</details>

<details>
<summary>6. How does TypeScript know whether a `.ts` file will be emitted as CommonJS or ESM under `NodeNext`?</summary>

By checking the nearest enclosing `package.json`'s `"type"` field (`"module"` vs CommonJS), or inspecting the file extension: `.mts` is always ESM, `.cts` is always CommonJS.
</details>

<details>
<summary>7. What is `resolvePackageJsonExports`?</summary>

A compiler option that forces TypeScript to consult the `"exports"` field in `package.json` when using older resolution modes like `Node10`. It is enabled by default in `NodeNext` and `bundler`.
</details>

<details>
<summary>8. Can you import TypeScript files directly using `.ts` extensions in imports?</summary>

Only if `"allowImportingTsExtensions": true` is set, which requires `"noEmit": true` because `tsc` will not rewrite the extension upon emission.
</details>

---

## 13. Exercises & Challenges

### Easy
1. **Fix Relative Import**: Fix the import `import { add } from './math';` in a project configured with `moduleResolution: "NodeNext"`.
   <details>
   <summary>Solution</summary>

   ```typescript
   import { add } from './math.js';
   ```
   </details>

2. **Package.json Subpath Alias**: Configure a subpath import `#config` in `package.json` pointing to `./src/config.ts`.
   <details>
   <summary>Solution</summary>

   ```json
   {
     "imports": {
       "#config": "./src/config.ts"
     }
   }
   ```
   </details>

### Medium
3. **Trace Resolution Diagnoser**: Write a shell command to debug why `import { helper } from 'my-lib/helper'` fails to resolve types in your project.
   <details>
   <summary>Solution</summary>

   ```bash
   npx tsc --noEmit --traceResolution | grep "my-lib/helper"
   ```
   </details>

4. **Dual Export package.json**: Write a complete `package.json` `"exports"` map for a library exposing root `.` and subpath `./analytics` for both ESM and CJS.
   <details>
   <summary>Solution</summary>

   ```json
   {
     "name": "my-analytics-sdk",
     "type": "module",
     "exports": {
       ".": {
         "types": "./dist/index.d.ts",
         "import": "./dist/index.js",
         "require": "./dist/index.cjs"
       },
       "./analytics": {
         "types": "./dist/analytics.d.ts",
         "import": "./dist/analytics.js",
         "require": "./dist/analytics.cjs"
       }
     }
   }
   ```
   </details>

### Hard
5. **Programmatic Module Resolver with TypeScript Compiler API**: Write a Node.js utility using `ts.resolveModuleName` that resolves an import specifier relative to a file and reports the exact resolved filesystem path and whether it is an external library.
   <details>
   <summary>Solution</summary>

   ```typescript
   import * as ts from 'typescript';

   export function inspectModuleResolution(
     moduleName: string,
     containingFile: string,
     compilerOptions: ts.CompilerOptions
   ): string | undefined {
     const resolution = ts.resolveModuleName(
       moduleName,
       containingFile,
       compilerOptions,
       ts.sys
     );

     if (resolution.resolvedModule) {
       console.log(`Resolved: ${resolution.resolvedModule.resolvedFileName}`);
       console.log(`Is External: ${resolution.resolvedModule.isExternalLibraryImport}`);
       return resolution.resolvedModule.resolvedFileName;
     }

     console.error(`Failed to resolve: ${moduleName}`);
     return undefined;
   }
   ```
   </details>

6. **Monorepo Cross-Package Import Verifier**: Write a script that parses all `package.json` files in a monorepo workspace and verifies that whenever Package A imports Package B, Package B's `"exports"` contains a matching `"types"` definition for that subpath.
   <details>
   <summary>Solution</summary>

   ```typescript
   import * as fs from 'fs';
   import * as path from 'path';

   export function verifyPackageExports(pkgDir: string): boolean {
     const pkgJson = JSON.parse(fs.readFileSync(path.join(pkgDir, 'package.json'), 'utf8'));
     if (!pkgJson.exports) return true;

     for (const [subpath, target] of Object.entries(pkgJson.exports)) {
       if (typeof target === 'object' && target !== null) {
         const keys = Object.keys(target);
         if (!keys.includes('types')) {
           console.error(`Subpath ${subpath} in ${pkgJson.name} is missing 'types' condition!`);
           return false;
         }
         if (keys[0] !== 'types') {
           console.warn(`Subpath ${subpath} in ${pkgJson.name}: 'types' should be the first condition!`);
         }
       }
     }
     return true;
   }
   ```
   </details>
