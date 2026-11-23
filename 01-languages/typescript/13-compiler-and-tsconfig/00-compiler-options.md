# Compiler Options and tsconfig Reference in TypeScript

> Master reference for `tsconfig.json` architecture: schema anatomy, `compilerOptions`, file inclusion rules (`include`, `exclude`, `files`), emit controls, source maps, and JSX transpilation modes.

| Metadata | Details |
| :--- | :--- |
| **Category** | Compiler & Configuration |
| **Difficulty** | Intermediate |
| **Prerequisites** | [00-setup/00-toolchain.md](../00-setup/00-toolchain.md), [08-modules/00-imports-and-exports.md](../08-modules/00-imports-and-exports.md) |
| **TypeScript Baseline** | TS 5.0+ (`verbatimModuleSyntax`, `moduleResolution: bundler`), TS 5.4+ |
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
The `tsconfig.json` file is the central command center for any TypeScript project. It directs how the compiler (`tsc`) discovers files, performs type analysis, handles imports, and emits JavaScript artifacts. A misconfigured `tsconfig.json` can cause false compile-time errors, runtime module resolution crashes, invisible type unsafety, or 10x slower build times.

Understanding the interaction between root fields (`compilerOptions`, `include`, `exclude`, `files`, `references`, `extends`) and emit flags ensures your project compiles reliably across local development, CI test pipelines, and multi-package monorepos.

---

## 2. Core Concepts
- **Top-Level Schema Anatomy**:
  - `compilerOptions`: Fine-grained configuration for syntax parsing, type checking, and output generation.
  - `include`: Glob patterns matching files to compile (e.g. `["src/**/*"]`).
  - `exclude`: Glob patterns for files to ignore (default: `["node_modules", "bower_components", "jspm_packages"]`).
  - `files`: Explicit list of exact file paths to include (overrides `exclude`).
  - `extends`: Path to a base tsconfig to inherit settings from (TS 5.0+ supports array of strings).
  - `references`: Composite project references for monorepo incremental builds.
- **Emit Controls**:
  - `noEmit: true`: Disables emitting `.js` or `.d.ts` files (used when Vite/Babel/esbuild/swc handles bundling).
  - `emitDeclarationOnly: true`: Emits only `.d.ts` declaration files while letting native bundlers compile code.
  - `declaration: true` & `declarationMap: true`: Generates `.d.ts` files and sourcemaps linking declarations back to source TypeScript files for IDE "Go to Definition".
- **Source Map Mechanics**: `sourceMap: true` produces external `.js.map` files; `inlineSourceMap: true` embeds base64 sourcemaps directly into emitted `.js`.
- **JSX Compilation Modes**:
  - `react-jsx`: Modern React 17+ transform (automatically imports from `react/jsx-runtime`).
  - `preserve`: Keeps JSX intact (`<div />`) for downstream bundlers (esbuild, Vite, Next.js).
  - `react`: Legacy transform to `React.createElement(...)`.

---

## 3. How It Works Internally

```
                         tsconfig.json
                               |
                               v
                     ts.parseConfigFileTextToJson
                               |
                               v
                  ts.parseJsonConfigFileContent
                               |
         +---------------------+---------------------+
         |                                           |
         v                                           v
  File Discovery                         CompilerOptions Validation
  - Match include globs                  - Merge 'extends' hierarchy
  - Prune exclude globs                  - Apply strict defaults
  - Include files array                  - Validate module/target compatibility
         |                                           |
         +---------------------+---------------------+
                               |
                               v
                      ts.createProgram()
```

The compiler reads `tsconfig.json`, resolves any inherited `extends` chains recursively, and computes an absolute list of root filenames using fast directory scanning. It then passes these inputs to `ts.createProgram`, initiating the AST parsing, type binding, and type-checking phases.

---

## 4. Syntax & API Reference

```json
{
  "$schema": "https://json.schemastore.org/tsconfig",
  "extends": "@tsconfig/node22/tsconfig.json",
  "compilerOptions": {
    "target": "ES2022",
    "module": "NodeNext",
    "moduleResolution": "NodeNext",
    "lib": ["ES2022"],
    "strict": true,
    "noEmit": true,
    "isolatedDeclarations": true,
    "verbatimModuleSyntax": true,
    "skipLibCheck": true,
    "forceConsistentCasingInFileNames": true
  },
  "include": ["src/**/*"],
  "exclude": ["node_modules", "dist", "**/*.spec.ts"]
}
```

---

## 5. Practical Examples

### Example 1: Typecheck-Only Config for Vite / Modern Bundlers
```json
{
  "compilerOptions": {
    "target": "ES2022",
    "useDefineForClassFields": true,
    "module": "ESNext",
    "lib": ["ES2022", "DOM", "DOM.Iterable"],
    "skipLibCheck": true,
    "moduleResolution": "bundler",
    "allowImportingTsExtensions": true,
    "resolvePackageJsonExports": true,
    "isolatedModules": true,
    "noEmit": true,
    "strict": true,
    "jsx": "react-jsx"
  },
  "include": ["src"]
}
```

### Example 2: Library Authoring Config (Emitting Dual ESM/DTS)
```json
{
  "compilerOptions": {
    "target": "ES2022",
    "module": "NodeNext",
    "moduleResolution": "NodeNext",
    "declaration": true,
    "declarationMap": true,
    "sourceMap": true,
    "outDir": "./dist",
    "rootDir": "./src",
    "strict": true,
    "verbatimModuleSyntax": true,
    "isolatedDeclarations": true
  },
  "include": ["src/**/*"]
}
```

### Example 3: Edge Case - Multiple `extends` Inheritance (TS 5.0+)
```json
{
  "extends": [
    "@tsconfig/bases/tsconfig.json",
    "./tsconfig.strict-rules.json"
  ],
  "compilerOptions": {
    "outDir": "./build"
  }
}
```

### Example 4: Compiler Failure Case - Conflicting `noEmit` and `declaration`
```json
{
  "compilerOptions": {
    "noEmit": true,
    "declaration": true
  }
}
// Running tsc --build or tsc:
// @ts-expect-error TS5053: Option 'declaration' cannot be specified with option 'noEmit'.
```

### Example 5: Clean Path Aliasing for Internal Source
```json
{
  "compilerOptions": {
    "baseUrl": ".",
    "paths": {
      "@core/*": ["src/core/*"],
      "@shared/*": ["src/shared/*"]
    }
  }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **`exclude` Does Not Block Explicit Imports**: If `fileA.ts` is inside an `exclude` directory, but `src/index.ts` imports `fileA`, TypeScript will compile and type-check `fileA.ts` anyway! `exclude` only controls what initial files are discovered by `include`.
2. **`paths` Does NOT Rewrite Imports**: TypeScript's `paths` option only tells the type checker how to resolve types. `tsc` does *not* rewrite `"@core/db"` into `"../core/db"` in the emitted JavaScript. If you use `paths`, you must use a bundler (Vite, esbuild) or Node subpath imports (`#internal`) that understand runtime resolution.
3. **`skipLibCheck` Performance**: Setting `"skipLibCheck": true` skips type checking all `.d.ts` declaration files across `node_modules`. This can cut compiler startup times by 40–70% without sacrificing application code safety.
4. **`allowImportingTsExtensions`**: Allowed only when `"noEmit": true` is set, since TypeScript will not emit `.js` files when imports explicitly reference `.ts`.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Consequence | Correction |
| :--- | :--- | :--- |
| Using `paths` without a runtime resolver | Emitted JavaScript crashes with `Cannot find module '@app/...'` | Use Node subpath imports (`#app/*`) or a bundler |
| Omitting `skipLibCheck: true` | Third-party `.d.ts` errors break your build; slow builds | Enable `"skipLibCheck": true` |
| Setting `"declaration": true` with `"noEmit": true` | **TS5053**: Incompatible compiler options | Use `emitDeclarationOnly: true` instead |
| Forgetting `forceConsistentCasingInFileNames` | Code passes on macOS/Windows, breaks on Linux CI | Enable `"forceConsistentCasingInFileNames": true` |
| Wildcard `include: ["*"]` matching build output | Compiles emitted `.js` files recursively | Specify exact source dir: `include: ["src/**/*"]` |

---

## 8. Performance & Type-Checker Complexity
- Restricting `include` to `["src/**/*"]` and setting `skipLibCheck: true` prevents scanning millions of irrelevant AST nodes in `node_modules`.
- In large monorepos, splitting builds into composite project references with `.tsbuildinfo` caches cuts incremental compilation times from 15 seconds to under 400 milliseconds.

---

## 9. Compiler Internals
Inside TypeScript's `src/compiler/commandLineParser.ts`:
- `parseConfigFileTextToJson` converts the file content into an AST using TypeScript's internal JSON scanner.
- `convertToBase64` and source map generators attach mappings between generated AST spans and source positions.
- Incompatible flags (like `noEmit` combined with `declaration`) are checked in `createProgram` by diagnostic verification rules.

---

## 10. Best Practices & Design Guidelines
- Standardize on official `@tsconfig/bases` presets (e.g. `@tsconfig/node22` or `@tsconfig/vite-react`).
- Always enable `"skipLibCheck": true` and `"forceConsistentCasingInFileNames": true`.
- If building with an external bundler (Vite, esbuild, Next.js), set `"noEmit": true` and use `tsc --noEmit` solely as a type-linter.
- If authoring libraries for npm, enable `"declaration": true` and `"declarationMap": true`.
- Use Node.js native subpath imports (`#utils`) in `package.json` rather than `paths` in `tsconfig.json`.

---

## 11. Real-World Case Study
A fast-growing SaaS platform's CI pipeline took 4 minutes just to run `tsc`. Developers frequently had PRs fail due to casing differences (`import from './User'` vs `./user.ts`) that passed on developer MacBooks but failed on Linux runners.

**Refactored tsconfig Optimization:**
1. Enabled `"skipLibCheck": true` (reduced type checking time from 240s to 35s).
2. Enabled `"forceConsistentCasingInFileNames": true` (eliminated all cross-platform casing bugs).
3. Swapped `paths` with native `package.json` subpath imports (`#services/*`).
4. Enabled `"incremental": true` with `.tsbuildinfo` cache in GitHub Actions.
CI pipeline time dropped from 4 minutes to 8 seconds on warm runs.

---

## 12. Interview Questions

<details>
<summary>1. What does `skipLibCheck: true` actually do?</summary>

It instructs the compiler to skip type checking of all declaration files (`.d.ts`). Only the declaration files directly referenced by code are parsed for symbols, saving significant compilation time while assuming third-party libraries are internally type-sound.
</details>

<details>
<summary>2. Why does `paths` in `tsconfig.json` not rewrite module paths in emitted JavaScript?</summary>

TypeScript's philosophy is that `tsc` performs type checking and downlevel syntax erasing, not general-purpose module path rewriting or bundling. Rewriting paths would require making assumptions about runtime resolution environments.
</details>

<details>
<summary>3. What is the difference between `exclude` and `files`?</summary>

`files` is an explicit whitelist of exact files to include. It overrides `exclude`. `exclude` only filters out files that were discovered via wildcard patterns in `include`.
</details>

<details>
<summary>4. What does `verbatimModuleSyntax: true` do?</summary>

Introduced in TS 5.0, it enforces that any import that is not explicitly marked with `import type` is emitted as a real JavaScript import, and any `import type` is completely erased, guaranteeing predictable runtime ESM behavior without heuristics.
</details>

<details>
<summary>5. What is `emitDeclarationOnly` used for?</summary>

It tells `tsc` to generate only `.d.ts` declaration files and suppress `.js` emit. This is used in hybrid build setups where an external fast compiler (like esbuild, SWC, or Vite) compiles JavaScript, while `tsc` generates types.
</details>

<details>
<summary>6. How does TypeScript 5.0 handle multiple `extends` in `tsconfig.json`?</summary>

TS 5.0 permits `extends` to accept an array of file paths: `"extends": ["base1", "base2"]`. Configurations are merged from left to right, with later entries overriding earlier ones.
</details>

<details>
<summary>7. What is the difference between `jsx: "react-jsx"` and `jsx: "react"`?</summary>

`"react"` compiles JSX tags into `React.createElement(...)` calls, requiring `import React from 'react'` in every file. `"react-jsx"` uses the modern React 17+ transform, automatically importing `_jsx` from `react/jsx-runtime`.
</details>

<details>
<summary>8. What is `declarationMap` and why should library authors enable it?</summary>

It generates `.d.ts.map` source maps linking `.d.ts` files back to original `.ts` source files. When consumers in an IDE run "Go to Definition" on a library symbol, their editor navigates directly to the original TypeScript source instead of the compiled `.d.ts` file.
</details>

---

## 13. Exercises & Challenges

### Easy
1. **Minimal Typecheck Config**: Write a minimal `tsconfig.json` for a modern frontend project using Vite that performs strict typechecking without emitting files.
   <details>
   <summary>Solution</summary>

   ```json
   {
     "compilerOptions": {
       "target": "ES2022",
       "module": "ESNext",
       "moduleResolution": "bundler",
       "strict": true,
       "noEmit": true,
       "skipLibCheck": true
     },
     "include": ["src/**/*"]
   }
   ```
   </details>

2. **Declaration Only Config**: Create a config that emits only `.d.ts` files into `./types`.
   <details>
   <summary>Solution</summary>

   ```json
   {
     "compilerOptions": {
       "declaration": true,
       "emitDeclarationOnly": true,
       "outDir": "./types",
       "rootDir": "./src"
     },
     "include": ["src/**/*"]
   }
   ```
   </details>

### Medium
3. **Multi-Target Monorepo Base**: Create a root base `tsconfig.base.json` that enforces strict typing, modern ESM, and consistent file casing, to be extended by individual packages.
   <details>
   <summary>Solution</summary>

   ```json
   {
     "$schema": "https://json.schemastore.org/tsconfig",
     "compilerOptions": {
       "target": "ES2022",
       "module": "NodeNext",
       "moduleResolution": "NodeNext",
       "strict": true,
       "skipLibCheck": true,
       "forceConsistentCasingInFileNames": true,
       "verbatimModuleSyntax": true,
       "composite": true,
       "declaration": true,
       "declarationMap": true
     }
   }
   ```
   </details>

4. **Path Alias Setup with Subpaths**: Write a `tsconfig.json` that configures path aliases for `@models/*` and `@controllers/*` alongside a corresponding `package.json` imports mapping.
   <details>
   <summary>Solution</summary>

   ```json
   // tsconfig.json
   {
     "compilerOptions": {
       "baseUrl": ".",
       "paths": {
         "@models/*": ["src/models/*"],
         "@controllers/*": ["src/controllers/*"]
       }
     }
   }
   ```
   ```json
   // package.json
   {
     "imports": {
       "#models/*": "./dist/models/*.js",
       "#controllers/*": "./dist/controllers/*.js"
     }
   }
   ```
   </details>

### Hard
5. **Programmatic tsconfig Parser with TypeScript API**: Write a TypeScript script using `ts.readConfigFile` and `ts.parseJsonConfigFileContent` to load, resolve extends, and print the effective compiler options of a project.
   <details>
   <summary>Solution</summary>

   ```typescript
   import * as ts from 'typescript';
   import * as path from 'path';

   export function getEffectiveCompilerOptions(configPath: string): ts.CompilerOptions {
     const configFile = ts.readConfigFile(configPath, ts.sys.readFile);
     if (configFile.error) {
       throw new Error(`Failed to read config: ${configFile.error.messageText}`);
     }

     const basePath = path.dirname(configPath);
     const parsed = ts.parseJsonConfigFileContent(
       configFile.config,
       ts.sys,
       basePath
     );

     return parsed.options;
   }
   ```
   </details>

6. **Monorepo Solution Config Setup**: Design a complete "solution-style" `tsconfig.json` with project references pointing to `packages/core`, `packages/api`, and `packages/client`, explaining how `tsc --build` processes dependencies.
   <details>
   <summary>Solution</summary>

   ```json
   {
     "files": [],
     "references": [
       { "path": "./packages/core" },
       { "path": "./packages/api" },
       { "path": "./packages/client" }
     ]
   }
   ```
   `tsc --build` uses this root solution config to construct a dependency graph between referenced packages, compiling leaf dependencies (e.g. `core`) first and caching `.tsbuildinfo` artifacts to skip unchanged subprojects.
   </details>
