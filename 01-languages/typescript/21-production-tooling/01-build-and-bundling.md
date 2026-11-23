# Build and Bundling in TypeScript: tsup, esbuild, swc, and tsc

> In-depth exploration of TypeScript build systems and bundlers: pure type emission (`tsc`), fast transpilations (`esbuild`, `swc`), bundle generation with `tsup`, declaration maps, and dual CJS/ESM outputs.

| Metadata | Details |
| :--- | :--- |
| **Category** | Production Tooling |
| **Difficulty** | Advanced |
| **Prerequisites** | [13-compiler-options/01-module-resolution.md](../13-compiler-options/01-module-resolution.md), [13-compiler-options/02-emit-declaration-options.md](../13-compiler-options/02-emit-declaration-options.md) |
| **TypeScript Baseline** | TS 5.0+, Node.js 20/22 LTS |
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

In early TypeScript workflows, the official compiler (`tsc`) was responsible for both checking types and emitting JavaScript code (`tsc --build`). However, as codebases grew to hundreds of thousands of lines, `tsc` became an execution bottleneck in CI/CD and local hot-reload loops.

Modern engineering decouples the build pipeline into two distinct responsibilities:
1. **Type Checking & Declarations**: Managed by `tsc` via `tsc --noEmit` (for validation) and `tsc --emitDeclarationOnly` (for generating `.d.ts` type definitions).
2. **Code Transpilation & Bundling**: Handled by ultra-fast native compilers like **esbuild** (written in Go) or **swc** (written in Rust), often orchestrated through high-level packaging tools like **tsup** or **Vite**.

```
+-------------------------------------------------------------------------+
|                  Modern Two-Track Build Architecture                    |
+-------------------------------------------------------------------------+
|                                                                         |
|                          TypeScript Source Code                         |
|                                     |                                   |
|                +--------------------+--------------------+              |
|                |                                         |              |
|                v                                         v              |
|        [ Track 1: Types ]                       [ Track 2: Runtime ]    |
|       tsc --emitDeclarationOnly               esbuild / swc / tsup      |
|                |                                         |              |
|         Type Validation &                         Tree-Shaking,         |
|         Symbol Resolution                       Minification & Bundle   |
|                |                                         |              |
|                v                                         v              |
|          dist/index.d.ts                      dist/index.js (ESM/CJS)   |
|         dist/index.d.ts.map                     dist/index.js.map       |
|                                                                         |
+-------------------------------------------------------------------------+
```

---

## 2. Core Concepts

### Transpilation vs. Type Checking
Native bundlers (esbuild, swc) do not evaluate types. They perform **type stripping**: they scan the syntax, discard interfaces, type aliases, and type annotations, and convert remaining modern JavaScript into targeted runtime syntax. 
- Transpilation speed: ~100x faster than `tsc`.
- Limitation: Unchecked runtime errors (e.g. invalid type assignments) pass through without warnings if `tsc --noEmit` is not run.

### Isolated Modules (`isolatedModules: true`)
Because native bundlers transpile each file in isolation without parsing imported dependencies, certain TypeScript language features cannot be safely compiled:
- Re-exporting types using `export { MyType } from './mod'` without the `type` modifier (causes runtime crashes when the bundler tries to import a stripped symbol).
- `const enum` declarations without ambient definitions.
- Implicit global namespaces.

### Declaration Maps (`declarationMap: true`)
Generates `.d.ts.map` source maps linking `.d.ts` declaration files back to original `.ts` source files. This allows IDEs ("Go to Definition") to navigate directly to TypeScript sources instead of generated declaration files in monorepos.

---

## 3. How It Works Internally

### esbuild / tsup Compilation Pipeline
1. **Scanning**: Resolves entry points and recursively discovers `import` and `export` statements.
2. **Lexing & Stripping**: Strips TypeScript-specific grammar nodes in Go memory pools without building a full type symbol table.
3. **Tree-Shaking**: Performs dead-code elimination by tracing static export usage graphs.
4. **Target Lowering**: Down-levels syntax (e.g. private class fields `#field`, nullish coalescing `??`) to match the target environment (e.g. `node18` or `es2022`).
5. **DTS Generation**: `tsup` spawns an isolated `tsc` worker thread with `emitDeclarationOnly: true` or uses `rolldown`/`isolated-declarations` (introduced in TS 5.5) to output bundled `.d.ts` bundles.

---

## 4. Syntax & API Reference

### tsup Configuration (`tsup.config.ts`)
```typescript
import { defineConfig } from "tsup";

export default defineConfig({
  entry: ["src/index.ts", "src/cli.ts"],
  format: ["cjs", "esm"],
  dts: true, // Generate .d.ts files
  sourcemap: true,
  clean: true,
  target: "node20",
  splitting: false,
  treeshake: true,
  minify: process.env.NODE_ENV === "production",
});
```

### package.json Dual Output Configuration
```json
{
  "name": "@acme/core-sdk",
  "version": "1.0.0",
  "type": "module",
  "main": "./dist/index.cjs",
  "module": "./dist/index.js",
  "types": "./dist/index.d.ts",
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
  },
  "files": [
    "dist"
  ]
}
```

---

## 5. Practical Examples

### Example 1: Type-Only Re-exports with Isolated Modules
```typescript
// types.ts
export interface UserSession {
  token: string;
  expiresAt: number;
}

// index.ts
// BAD (fails under isolatedModules / esbuild):
// export { UserSession } from './types';

// GOOD: Explicit type modifier ensures bundler strips the statement completely
export type { UserSession } from "./types.js";
export { createSession } from "./session.js";
```

### Example 2: tsup Custom Plugin for Env Inlining
```typescript
// tsup.config.ts
import { defineConfig } from "tsup";

export default defineConfig({
  entry: ["src/index.ts"],
  format: ["esm"],
  dts: true,
  define: {
    __VERSION__: JSON.stringify("1.2.4"),
    __BUILD_DATE__: JSON.stringify(new Date().toISOString()),
  },
  esbuildOptions(options) {
    options.drop = process.env.NODE_ENV === "production" ? ["console", "debugger"] : [];
  },
});
```

### Example 3: Edge Case - TypeScript 5.5 `isolatedDeclarations`
```typescript
// When "isolatedDeclarations": true is enabled in tsconfig.json,
// tsc requires explicit return types on all exported functions so
// fast tools (oxc, swc) can generate .d.ts files without running a type checker.

// FAILS under isolatedDeclarations:
// export function computeMetrics(x: number, y: number) {
//   return { sum: x + y, diff: x - y };
// }

// PASSES: Explicit return signature
export interface Metrics {
  sum: number;
  diff: number;
}

export function computeMetrics(x: number, y: number): Metrics {
  return { sum: x + y, diff: x - y };
}
```

### Example 4: Compiler Error & Type-Strip Incompatibility
```typescript
// Enums generate real JavaScript runtime objects, but const enums
// can cause runtime ReferenceError when consumed by isolated bundlers.

const enum CacheMode {
  Memory = "MEMORY",
  Redis = "REDIS",
}

// If consumed from an external npm package compiled without preserveConstEnums:
// @ts-expect-error - TS2475: 'const' enums can only be used in property or index access expressions or the right hand side of an import declaration.
console.log(CacheMode);

// Preferred Idiomatic Alternative:
export const CACHE_MODE = {
  Memory: "MEMORY",
  Redis: "REDIS",
} as const;

export type CacheModeType = (typeof CACHE_MODE)[keyof typeof CACHE_MODE];
```

### Example 5: High-Performance SWC Transformation Setup
```json
{
  "$schema": "https://json.schemastore.org/swcrc",
  "jsc": {
    "parser": {
      "syntax": "typescript",
      "dynamicImport": true,
      "decorators": true
    },
    "transform": {
      "legacyDecorator": true,
      "decoratorMetadata": true
    },
    "target": "es2022"
  },
  "module": {
    "type": "es6"
  }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Dual Package Hazard**:
   If a consumer imports both the CommonJS (`dist/index.cjs`) and ESM (`dist/index.js`) versions of your library, two independent instances of singleton state or `instanceof` symbols will be instantiated in memory. Always ensure exported state is encapsulated or provided as a pure stateless API.
2. **The `.d.ts` Resolution Ordering in `exports`**:
   Node.js reads `exports` conditions in order. If the `default` condition appears *before* `types`, TypeScript's module resolver will skip reading the types! Always place `"types"` first in every export subpath block.
3. **Missing File Extensions in ESM Imports**:
   When targeting pure ESM (`"module": "NodeNext"`), relative imports *must* specify the `.js` extension (`import { util } from './util.js'`) even though the authoring file is `util.ts`. Bundlers that magically resolve extensionless paths will fail when native Node.js runs the code.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Practice | Good Practice | Problem / Diagnostic |
| :--- | :--- | :--- | :--- |
| **Using `tsc` for Bundling** | Relying on `outFile` with `tsc` | Using `tsup` or `esbuild` for bundling | `TS6046: Option 'outFile' cannot be specified without 'module' of type 'amd' or 'system'` |
| **Omitting Type in Re-export** | `export { TypeA } from './a'` | `export type { TypeA } from './a'` | Bundler emits empty import causing runtime failure |
| **Types After Default in Exports** | `{"default": "...", "types": "..."}` | `{"types": "...", "default": "..."}` | TypeScript fails to find declaration files |
| **Missing Declaration Maps** | `declarationMap: false` in monorepo | `declarationMap: true` | IDE "Go to Definition" jumps to compiled `.d.ts` instead of source |
| **Implicit Return in Exports** | Unannotated export with `isolatedDeclarations` | Explicit interface or return type annotation | `TS9007: Function must have an explicit return type annotation with isolatedDeclarations` |
| **Const Enums Across Libraries** | `export const enum Flag {}` | `export const Flag = {} as const;` | Inlining fails across package boundaries |

---

## 8. Performance & Type-Checker Complexity

| Tool | Language | Bundling / Transpile Speed (100k LOC) | Type Checking Support |
| :--- | :--- | :--- | :--- |
| **tsc** | TypeScript / JS | ~12.5 seconds | Full AST & Symbol checking |
| **esbuild** | Go | ~0.15 seconds | None (syntax stripping only) |
| **swc** | Rust | ~0.22 seconds | None (syntax stripping only) |
| **tsup** | Node + esbuild + tsc | ~1.8 seconds (with `.d.ts`) | Delegates `.d.ts` to `tsc` |

To maximize build speed in CI:
- Run `esbuild` or `tsup` to emit production bundles.
- In a parallel CI step, run `tsc --noEmit` to validate types.

---

## 9. Compiler Internals

When `tsc --emitDeclarationOnly` runs:
1. The **Parser** builds the AST.
2. The **Binder** populates symbol tables and computes visibility flags.
3. The **TypeChecker** infers type representations for expressions.
4. The **Emitter** (`dtsWriter`) suppresses function bodies, control flow, and non-exported symbols, writing solely:
   - Exported interfaces, classes, and types.
   - Public signatures with fully resolved structural types.
5. In TypeScript 5.5, `isolatedDeclarations` guarantees that step 3 can be completely bypassed by third-party parsers like Oxlint or SWC, enabling $10\times$ faster type definition emission.

---

## 10. Best Practices & Design Guidelines

1. **Always Set `isolatedModules: true`**: Guarantees compatibility with fast native bundlers.
2. **Enable `isolatedDeclarations: true` for Shared Libraries**: Enables sub-second `.d.ts` generation across massive monorepos.
3. **Use `tsup` for Libraries**: Simplifies CJS/ESM dual bundling, DTS generation, and asset handling in a single zero-config CLI.
4. **Enforce `.js` Extensions in Module Imports**: Guarantees strict ESM standards compliance under NodeNext.

---

## 11. Real-World Case Study

An enterprise monorepo with 45 packages experienced 8-minute build times on local machines. The root cause was that every package ran `tsc -b` to compile TypeScript to JavaScript.

**Solution Architecture**:
1. Converted all package build scripts from `tsc` to `tsup src/index.ts --format esm,cjs --dts`.
2. Added `"isolatedModules": true` and `"declarationMap": true` to the base `tsconfig.json`.
3. CI parallelized `pnpm build` (bundling via tsup) and `pnpm typecheck` (`tsc -b --noEmit`).

**Result**: Build time dropped from 8 minutes to 28 seconds (a 94% reduction).

---

## 12. Interview Questions

<details>
<summary>1. Why can't esbuild or swc perform type checking?</summary>

Native bundlers are written in systems languages (Go, Rust) and only implement lexers and parsers for syntax stripping. Type checking requires evaluating the complete TypeScript type algebra (unions, generics, conditional types, mapped types) which requires the complete TypeScript compiler program and symbol graph.
</details>

<details>
<summary>2. What is `isolatedDeclarations` introduced in TypeScript 5.5?</summary>

`isolatedDeclarations` is a compiler option requiring all exported APIs to have explicit type annotations. This guarantees that `.d.ts` files can be emitted by standalone transpilers (like SWC or Oxc) file-by-file without computing global type inference.
</details>

<details>
<summary>3. What is the Dual Package Hazard in Node.js?</summary>

It occurs when a consumer package loads both the CommonJS and ESM builds of the same dependency simultaneously, creating duplicate module instances and breaking `instanceof` checks and internal package singletons.
</details>

<details>
<summary>4. Why must `"types"` always be first in `package.json` `exports`?</summary>

TypeScript iterates through conditional export keys sequentially. If a key like `"import"` or `"default"` comes first and points to a `.js` file, TypeScript stops checking and assumes no declaration file exists.
</details>

<details>
<summary>5. What is the purpose of `declarationMap: true`?</summary>

It emits `.d.ts.map` source maps that enable IDEs to navigate from a consuming file directly to the original TypeScript source file (`.ts`) instead of landing on the generated `.d.ts` declaration file.
</details>

<details>
<summary>6. How does `tsup` handle `.d.ts` generation?</summary>

Under the hood, `tsup` uses `esbuild` for instant JS bundling and spawns an instance of TypeScript's compiler (`tsc`) or `isolated-declarations` worker to emit and bundle declarations.
</details>

<details>
<summary>7. Why should you avoid `const enum` in libraries published to npm?</summary>

If a consumer compiles with `isolatedModules: true` or native transpilers (Babel, esbuild, swc), the compiler cannot look up ambient const enum values, leading to runtime undefined errors.
</details>

<details>
<summary>8. What is the difference between `tsc --noEmit` and `tsc --build`?</summary>

`tsc --noEmit` checks types across the project without generating output files on disk. `tsc --build` (or `-b`) understands project references, incrementally compiling dirty packages and emitting `.js` and `.d.ts` artifacts based on `.tsbuildinfo` caches.
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Challenge 1 (Easy): Create a minimal tsup configuration file</summary>

**Task**: Write a `tsup.config.ts` that bundles `src/index.ts` into both ESM and CJS formats with generated declaration types and sourcemaps.

**Solution**:
```typescript
import { defineConfig } from "tsup";

export default defineConfig({
  entry: ["src/index.ts"],
  format: ["cjs", "esm"],
  dts: true,
  sourcemap: true,
  clean: true,
});
```
</details>

<details>
<summary>Challenge 2 (Easy): Fix export statements for isolatedModules</summary>

**Task**: Refactor the following re-export so that it complies with `"isolatedModules": true`:
```typescript
export { UserRole, AdminPermissions } from './auth'; // UserRole is enum, AdminPermissions is interface
```

**Solution**:
```typescript
export { UserRole } from "./auth.js";
export type { AdminPermissions } from "./auth.js";
```
</details>

<details>
<summary>Challenge 3 (Medium): Configure package.json exports for strict dual publishing</summary>

**Task**: Write the `exports` field of a `package.json` for a package with root and `./helpers` subpaths supporting ESM and CJS.

**Solution**:
```json
{
  "exports": {
    ".": {
      "types": "./dist/index.d.ts",
      "import": "./dist/index.js",
      "require": "./dist/index.cjs"
    },
    "./helpers": {
      "types": "./dist/helpers.d.ts",
      "import": "./dist/helpers.js",
      "require": "./dist/helpers.cjs"
    }
  }
}
```
</details>

<details>
<summary>Challenge 4 (Medium): Refactor code to satisfy isolatedDeclarations</summary>

**Task**: Modify this function so that it satisfies TypeScript 5.5's `"isolatedDeclarations": true`:
```typescript
export function makeConfig(env: string) {
  return {
    environment: env,
    debug: env !== "production",
    retries: 3
  };
}
```

**Solution**:
```typescript
export interface AppConfig {
  environment: string;
  debug: boolean;
  retries: number;
}

export function makeConfig(env: string): AppConfig {
  return {
    environment: env,
    debug: env !== "production",
    retries: 3,
  };
}
```
</details>

<details>
<summary>Challenge 5 (Hard): Custom esbuild build script with type-checking hook</summary>

**Task**: Write a standalone Node.js build script using the `esbuild` API that builds the project and spawns `tsc` to verify types.

**Solution**:
```typescript
import { build } from "esbuild";
import { execSync } from "node:child_process";

async function runBuild() {
  console.log("Transpiling with esbuild...");
  await build({
    entryPoints: ["src/index.ts"],
    bundle: true,
    platform: "node",
    target: "node20",
    outfile: "dist/bundle.js",
    sourcemap: true,
  });

  console.log("Verifying types with tsc...");
  execSync("tsc --noEmit", { stdio: "inherit" });
  console.log("Build complete!");
}

runBuild().catch((err) => {
  console.error("Build failed:", err);
  process.exit(1);
});
```
</details>

<details>
<summary>Challenge 6 (Hard): Clean declaration tree-shaking pattern</summary>

**Task**: Demonstrate how to use internal private helper functions in TypeScript without leaking implementation details into the generated `.d.ts` file.

**Solution**:
```typescript
// internal.ts (Not exported in package.json exports)
export function internalHash(val: string): string {
  return `hash_${val}`;
}

// index.ts (Public entry point)
import { internalHash } from "./internal.js";

export interface TokenGenerator {
  generate(id: string): string;
}

export class PublicTokenGenerator implements TokenGenerator {
  public generate(id: string): string {
    return internalHash(id);
  }
}
// The generated index.d.ts will contain PublicTokenGenerator and TokenGenerator
// without embedding or exposing internalHash signatures.
```
</details>
