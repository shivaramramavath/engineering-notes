# Third-Party Types and @types

> Managing external type definitions in TypeScript: the DefinitelyTyped ecosystem, `@types/` package resolution, scoping with `types` and `typeRoots`, and overriding faulty community declarations.

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [00-declaration-files.md](file:///E:/Learning/engineering-notes/01-languages/typescript/09-declaration-files/00-declaration-files.md), [01-ambient-declarations.md](file:///E:/Learning/engineering-notes/01-languages/typescript/09-declaration-files/01-ambient-declarations.md) |
| **Time to Read** | 24 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 2.0 / TS 5.x / TS 7.0 Native |
| **Tested tsconfig** | `strict: true`, `skipLibCheck: true` |

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
Many of the most popular packages on npm—such as Express, React, Lodash, and Node.js built-ins—were historically authored in plain JavaScript or have decoupled type maintenance.

To allow TypeScript developers to consume these untyped libraries with complete type safety, the community created **DefinitelyTyped** (`@types/*`), a massive central repository hosting over 9,000 community-maintained declaration packages published under the `@types` npm scope.

Understanding how TypeScript discovers, resolves, scopes, and overrides these third-party type packages is essential for managing enterprise dependency trees without version conflicts.

---

## 2. Core Concepts

### Two Types of NPM Packages
1. **Bundled Type Packages**: The package author ships `.d.ts` files inside the npm package itself (e.g. `prisma`, `rxjs`, `zod`). No separate install needed.
2. **DefinitelyTyped Packages**: The package author published untyped JavaScript. Community contributors publish types to npm under `@types/<package-name>` (e.g. `npm install -D @types/lodash`).

### Automatic `@types` Discovery
By default, the TypeScript compiler automatically crawls `node_modules/@types` in the current directory and every parent folder up to the drive root, including all discovered type packages in the compilation scope.

---

## 3. How It Works Internally

```
import lodash from "lodash";
            |
            v
1. Check node_modules/lodash/package.json
   Has "types" or "exports" types?
       /                 \
     Yes                  No
     /                     \
Use bundled types     2. Look up node_modules/@types/lodash/index.d.ts
                           /                   \
                         Found                Not Found
                         /                     \
                   Use @types               Emit TS7016: Could not find
                                            declaration file for module
```

When resolving types for an external module `foo`:
1. The compiler checks if `foo/package.json` has a `types` field or conditional export.
2. If missing, it checks `node_modules/@types/foo/index.d.ts`.
3. If neither exists, it raises error `TS7016`.

---

## 4. Syntax & API Reference

### Scoping with `types` and `typeRoots`
```json
{
  "compilerOptions": {
    // Specify custom directories containing @types packages:
    "typeRoots": [
      "./node_modules/@types",
      "./custom-types"
    ],
    // Whitelist ONLY specific @types packages to include in global scope:
    "types": [
      "node",
      "jest"
    ],
    // Skip type-checking declaration files to speed up builds:
    "skipLibCheck": true
  }
}
```
*Warning: If `"types"` is specified, ONLY the listed packages are included in global scope automatically.*

---

## 5. Practical Examples

### 5.1 Installing and Using DefinitelyTyped
```bash
npm install lodash
npm install -D @types/lodash
```

```typescript
import { chunk } from "lodash";
const groups = chunk([1, 2, 3, 4], 2); // strictly typed as number[][]
```

### 5.2 Real-World Application: Overriding Buggy Community Types
```typescript
// src/types/overrides.d.ts
// Override a broken or outdated signature from an installed @types package:
import "some-buggy-lib";

declare module "some-buggy-lib" {
  // Augment or replace interface
  interface ClientOptions {
    newOptionAddedInV5: boolean;
  }
}
```

### 5.3 Edge Case: Scoped Packages under `@types`
For npm scoped packages (e.g. `@acme/core`), slashes are converted to double underscores in `@types`:
```bash
npm install -D @types/acme__core
```

### 5.4 Failure Case: Missing Declaration File
```typescript
// @ts-expect-error TS7016: Could not find a declaration file for module 'untyped-canvas-lib'.
import { renderCanvas } from "untyped-canvas-lib";
```

### 5.5 Idiomatic Pattern: Quick Untyped Shims
```typescript
// globals.d.ts: Quickly silence TS7016 for untyped modules:
declare module "untyped-canvas-lib";
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **`skipLibCheck: true` is Essential**: By default, TypeScript re-checks all `.d.ts` files inside `node_modules`. If two installed `@types` packages have subtle incompatibilities (e.g. mismatched `@types/node` versions), `tsc` will throw errors in files you don't own! Always set `"skipLibCheck": true`.
2. **Specifying `types: []` Disables Global Inclusion**: If you write `"types": ["node"]`, any global ambient declarations from other `@types` packages (like Jest or Mocha) are excluded from the global scope.
3. **Mismatched Version Drift**: `@types/package` versions must match the major and minor version of the runtime package (e.g. `react@18.2.0` pairs with `@types/react@^18.2.0`).

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Problem | Solution | Compiler Error |
| :--- | :--- | :--- | :--- |
| Disabling `skipLibCheck` | Compile errors triggered by third-party type bugs in node_modules | Set `"skipLibCheck": true` | Cascade of node_modules errors |
| Installing `@types` as `dependencies` | Bloats production install with dev-only type definitions | Install with `-D` (`devDependencies`) | Production bundle/install bloat |
| Version mismatch between package and @types | Runtime API does not match type signatures | Align major/minor versions | Missing methods / type errors |
| Overriding `types` array without including `node` | Built-in Node globals (`process`, `Buffer`) vanish | Include `"node"` in `"types"` array | `TS2304: Cannot find name 'process'` |
| Authoring a package and publishing types separately | Poor DX for package consumers | Bundle `.d.ts` files directly in the package | External dependency maintenance |
| Putting custom declarations in `node_modules/@types` | Wiped out by next `npm install` | Put custom types in `src/types` and configure `typeRoots` | Lost types after install |

---

## 8. Performance & Type-Checker Complexity
- Setting `skipLibCheck: true` skips type-checking function bodies in `.d.ts` files, reducing compilation time by **30–50%** in large projects.
- Pruning unnecessary `@types` packages from `package.json` speeds up AST indexing.

---

## 9. Compiler Internals
- `src/compiler/moduleSpecifiers.ts` resolves module names.
- `loadModuleFromNodeModulesWorker` checks `node_modules/@types` directory paths if direct package resolution fails to yield types.
- `getAutomaticTypeDirectiveNames` reads the directory entries of `@types` to populate ambient global type roots.

---

## 10. Best Practices & Design Guidelines
- Always set `skipLibCheck: true` in production `tsconfig.json`.
- Keep `@types/*` dependencies strictly in `devDependencies`.
- Check for bundled types before running `npm install @types/pkg`.
- Align `@types` package major versions with the runtime package version.

---

## 11. Real-World Case Study
### Resolving Jest and Node Global Collision in Monorepo
A full-stack monorepo had conflicting definitions for `setTimeout` because `@types/jest` and `@types/node` both declared global timers with different return types (`number` vs `NodeJS.Timeout`).

**Resolution:**
1. In the client package `tsconfig.json`, scoped `"types": ["jest"]`.
2. In the server package `tsconfig.json`, scoped `"types": ["node"]`.
3. Enabled `"skipLibCheck": true`.
The type checker cleanly isolated the environments, eliminating all 114 compiler conflict errors.

---

## 12. Interview Questions

<details>
<summary>1. What is DefinitelyTyped and the <code>@types</code> scope?</summary>

DefinitelyTyped is a community-run GitHub repository that provides high-quality TypeScript declaration files for JavaScript libraries that do not bundle their own types. Packages from DefinitelyTyped are automatically published to npm under the `@types/` scope (e.g. `@types/node`).
</details>

<details>
<summary>2. Why should <code>skipLibCheck: true</code> be enabled in almost every project?</summary>

`skipLibCheck: true` tells the compiler to skip full type-checking of all declaration files (`.d.ts`) in `node_modules`. This prevents build failures caused by minor incompatibilities between different third-party type packages and cuts compile times by up to 50%.
</details>

<details>
<summary>3. What is the difference between <code>typeRoots</code> and <code>types</code> in <code>tsconfig.json</code>?</summary>

`typeRoots` specifies which **folders** contain declaration packages (defaulting to `node_modules/@types`). `types` specifies a **whitelist of package names** from those folders to include in global scope automatically.
</details>

<details>
<summary>4. How does TypeScript name <code>@types</code> packages for scoped npm packages like <code>@babel/core</code>?</summary>

By replacing the slash with a double underscore: `@types/babel__core`.
</details>

<details>
<summary>5. What happens if a library bundles its own types AND you install an <code>@types</code> package for it?</summary>

TypeScript prioritizes the bundled types found directly via `package.json` `types` over the external `@types` package, rendering the `@types` package redundant.
</details>

<details>
<summary>6. How can you quickly silence error TS7016 for an untyped library?</summary>

By adding a shorthand ambient declaration in a `.d.ts` file: `declare module "untyped-library";`. This types all imports from that module as `any`.
</details>

<details>
<summary>7. Why should <code>@types/*</code> packages be installed as <code>devDependencies</code>?</summary>

Because declaration files are only needed during compilation and development. They emit zero runtime code and have no presence in production execution environments.
</details>

<details>
<summary>8. How do you override an incorrect return type from a third-party <code>@types</code> package?</summary>

Using module augmentation (`declare module "pkg" { interface ExistingInterface { ... } }`) or by creating a local type wrapper function.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Install Correct @types Package (Easy)
Write the npm command to install types for scoped package `@tanstack/react-query`.
<details>
<summary>Solution</summary>

```bash
# Note: TanStack Query bundles its own types, but if it used DefinitelyTyped:
npm install -D @types/tanstack__react-query
```
</details>

### Exercise 2: Silence Untyped Module (Easy)
Create a declaration file that silences `TS7016` for an untyped library `legacy-chart`.
<details>
<summary>Solution</summary>

```typescript
// types/shims.d.ts
declare module "legacy-chart";
```
</details>

### Exercise 3: Restrict Global Types Scope (Medium)
Configure `tsconfig.json` so that only `node` and `vitest` are included in the global scope.
<details>
<summary>Solution</summary>

```json
{
  "compilerOptions": {
    "types": ["node", "vitest"]
  }
}
```
</details>

### Exercise 4: Add Custom Local TypeRoot (Medium)
Configure `tsconfig.json` to look for ambient types in both `node_modules/@types` and a local `./custom-types` folder.
<details>
<summary>Solution</summary>

```json
{
  "compilerOptions": {
    "typeRoots": [
      "./node_modules/@types",
      "./custom-types"
    ]
  }
}
```
</details>

### Exercise 5: Augment Third-Party Return Type (Hard)
Augment an external library's method signature to return a strictly typed entity instead of `any`.
<details>
<summary>Solution</summary>

```typescript
import "csv-parser";

declare module "csv-parser" {
  interface CsvParserOptions {
    strictSchema?: boolean;
  }
}
```
</details>

### Exercise 6: Automated Types Audit Script (Hard)
Write a Node.js script that scans `package.json` dependencies and flags any dependency that lacks either bundled types or an installed `@types` package.
<details>
<summary>Solution</summary>

```typescript
import fs from "fs";
import path from "path";

function auditDependencies(): void {
  const pkg = JSON.parse(fs.readFileSync("./package.json", "utf8"));
  const deps = Object.keys(pkg.dependencies || {});
  const devDeps = Object.keys(pkg.devDependencies || {});

  for (const dep of deps) {
    const pkgPath = path.resolve("./node_modules", dep, "package.json");
    if (!fs.existsSync(pkgPath)) continue;
    const depPkg = JSON.parse(fs.readFileSync(pkgPath, "utf8"));
    const hasBundled = depPkg.types || depPkg.typings || depPkg.exports?.["."]?.types;
    const typePkgName = dep.startsWith("@") ? `@types/${dep.slice(1).replace("/", "__")}` : `@types/${dep}`;
    const hasAtTypes = devDeps.includes(typePkgName);

    if (!hasBundled && !hasAtTypes) {
      console.warn(`Warning: ${dep} has no bundled types and missing ${typePkgName}`);
    }
  }
}
```
</details>
