# Project References and Incremental Builds in TypeScript

> Master guide to scaling monorepos and large codebases with TypeScript Project References: `composite: true`, `references: [{ path }]`, `incremental: true`, `.tsbuildinfo` caching, and `tsc --build` orchestration.

| Metadata | Details |
| :--- | :--- |
| **Category** | Compiler & Configuration |
| **Difficulty** | Advanced |
| **Prerequisites** | [13-compiler-and-tsconfig/00-compiler-options.md](./00-compiler-options.md), [13-compiler-and-tsconfig/03-module-resolution-and-paths.md](./03-module-resolution-and-paths.md) |
| **TypeScript Baseline** | TS 3.0+ (Project References), TS 3.4+ (`incremental`) |
| **Tested tsconfig** | `"composite": true, "declaration": true, "incremental": true` |

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
As TypeScript codebases grow past tens of thousands of lines, running a single monolithic `tsc` compilation leads to memory exhaustion and excruciatingly slow build times (often several minutes). Furthermore, in monorepos where multiple packages depend on each other, compiling all packages in a flat space prevents proper encapsulation and modular caching.

TypeScript **Project References** allow you to decompose a monolithic codebase into smaller, independent projects that reference one another via `"references": [{ "path": "..." }]`. Using the build orchestrator `tsc --build` (`tsc -b`), TypeScript compiles projects in topological dependency order and caches build graphs in `.tsbuildinfo` files, accelerating subsequent builds by up to $10\times - 50\times$.

---

## 2. Core Concepts
- **`composite: true`**: Marks a project as a buildable unit in a project reference graph:
  - Automatically enforces `"declaration": true`.
  - Enforces `"rootDir"` so emitted declarations match source structures.
  - Generates `.tsbuildinfo` state metadata.
- **`references: [{ path: "../core" }]`**: Declares that this project depends on another project. The compiler can type-check against the emitted `.d.ts` of the referenced project without loading its source AST.
- **`tsc --build` (`tsc -b`)**: Specialized build runner that traverses the project dependency graph:
  - Builds upstream projects first.
  - Skips projects whose source files have not changed since the last build.
  - Supports `--clean`, `--dry`, and `--watch`.
- **Solution `tsconfig.json`**: A top-level root config with `"files": []` and a list of all project references used to build the entire monorepo with a single command.
- **Incremental Caching (`.tsbuildinfo`)**: Stores file hashes, symbol graphs, and emit timestamps to skip unchanged files.

---

## 3. How It Works Internally

```
                      Root Solution (tsconfig.json)
                                   |
                  +----------------+----------------+
                  |                                 |
                  v                                 v
        Project: packages/core            Project: packages/utils
        ("composite": true)               ("composite": true)
        Emits: core.d.ts                  Emits: utils.d.ts
        Writes: core.tsbuildinfo          Writes: utils.tsbuildinfo
                  \                                 /
                   \                               /
                    v                             v
                       Project: packages/api-gateway
                       references: [core, utils]
                       Reads core.d.ts & utils.d.ts
                       Emits: api.js, api.d.ts
```

When you run `tsc -b`:
1. The compiler creates a directed acyclic graph (DAG) of all referenced projects.
2. It detects topological order (`core` and `utils` before `api-gateway`).
3. For each project, it checks if any inputs are newer than the corresponding outputs or if `.tsbuildinfo` indicates a cache miss.
4. If cached, it skips compilation instantly.

---

## 4. Syntax & API Reference

```json
// 1. Upstream Project: packages/core/tsconfig.json
{
  "compilerOptions": {
    "composite": true,
    "declaration": true,
    "declarationMap": true,
    "outDir": "./dist",
    "rootDir": "./src",
    "module": "NodeNext",
    "moduleResolution": "NodeNext",
    "target": "ES2022"
  },
  "include": ["src/**/*"]
}

// 2. Downstream Project: packages/api/tsconfig.json
{
  "compilerOptions": {
    "composite": true,
    "declaration": true,
    "outDir": "./dist",
    "rootDir": "./src",
    "module": "NodeNext",
    "moduleResolution": "NodeNext",
    "target": "ES2022"
  },
  "references": [
    { "path": "../core" }
  ],
  "include": ["src/**/*"]
}

// 3. Root Solution: tsconfig.json
{
  "files": [],
  "references": [
    { "path": "./packages/core" },
    { "path": "./packages/api" }
  ]
}
```

---

## 5. Practical Examples

### Example 1: Building with tsc -b
```bash
# Build all referenced projects in dependency order:
tsc -b

# Force full rebuild ignoring cache:
tsc -b --force

# Clean all emitted .js, .d.ts, and .tsbuildinfo files:
tsc -b --clean

# Watch mode across entire project graph:
tsc -b -w
```

### Example 2: Monorepo Package Reference Structure
```typescript
// packages/core/src/user.ts
export interface User {
  id: string;
  name: string;
}

// packages/api/src/server.ts
// Imports directly from the referenced project package
import { User } from '@myorg/core';

export function handleUser(u: User) {
  console.log(`User: ${u.name}`);
}
```

### Example 3: Edge Case - Prepend Flag (Legacy Bundling)
```json
// In older TS versions, prepend: true concatenated output files.
// Deprecated in TS 5.0+ in favor of modern bundlers.
{
  "references": [
    { "path": "../shared" }
  ]
}
```

### Example 4: Compiler Failure Case - Cyclic References
```json
// packages/A/tsconfig.json references packages/B
// packages/B/tsconfig.json references packages/A
// Running tsc -b:
// @ts-expect-error TS6202: Project references may not form a circular graph. Cycle detected: packages/A/tsconfig.json -> packages/B/tsconfig.json -> packages/A/tsconfig.json
```

### Example 5: Idiomatic Monorepo Shared Base Pattern
```json
// tsconfig.base.json
{
  "compilerOptions": {
    "target": "ES2022",
    "module": "NodeNext",
    "moduleResolution": "NodeNext",
    "strict": true,
    "composite": true,
    "declaration": true,
    "declarationMap": true,
    "skipLibCheck": true
  }
}

// packages/core/tsconfig.json
{
  "extends": "../../tsconfig.base.json",
  "compilerOptions": {
    "outDir": "./dist",
    "rootDir": "./src"
  },
  "include": ["src"]
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Circular Project References Are Prohibited**: Unlike circular imports between files in a single project, project references strictly forbid cycles. The project graph must be a strict DAG (Directed Acyclic Graph).
2. **Missing OutDir or RootDir in Composite Projects**: A `composite: true` project requires either explicit `rootDir` or precise source/output alignment. Emitting declarations without `outDir` will pollute source folders.
3. **Running `tsc` vs `tsc -b`**: If you run plain `tsc` in a composite project, it does *not* build referenced upstream projects. You must run `tsc --build` (`tsc -b`) to trigger cross-project dependency builds.
4. **Stale `.tsbuildinfo` Files**: If you delete emitted `.js` files manually without deleting `.tsbuildinfo`, `tsc -b` may believe the build is up to date and fail to re-emit. Run `tsc -b --force` or `tsc -b --clean`.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Consequence | Correction |
| :--- | :--- | :--- |
| Running `tsc` instead of `tsc -b` in monorepo | Upstream packages are not built; import errors | Always use `tsc --build` (`tsc -b`) |
| Creating circular references between packages | **TS6202**: Circular graph detected | Refactor shared types into a `common` leaf package |
| Omitting `declarationMap: true` in libraries | IDE "Go to Definition" opens `.d.ts` instead of `.ts` | Enable `"declarationMap": true` |
| Putting source files into root solution config | Blurs project boundaries; slow builds | Keep root `tsconfig.json` with `"files": []` |
| Committing `.tsbuildinfo` to git | Merge conflicts in cache files across team | Add `*.tsbuildinfo` to `.gitignore` |

---

## 8. Performance & Type-Checker Complexity
- Project references reduce type checking from $\mathcal{O}(TotalSource)$ to $\mathcal{O}(PackageSource + SumOfReferencedDts)$. Because `.d.ts` files contain zero function bodies or complex expressions, parsing them is $\approx 10\times$ faster than parsing original source code.
- Incremental builds using `.tsbuildinfo` only re-typecheck files whose timestamps or hashes have changed.

---

## 9. Compiler Internals
Inside TypeScript's `src/compiler/tsbuild.ts`:
- `createBuildOrder` performs topological sorting on the reference graph using Depth-First Search (DFS).
- `getBuildStatus` compares file modification times against records in `.tsbuildinfo`.
- When compiling downstream projects, the binder only loads the emitted `.d.ts` files of upstream projects, insulating memory from upstream AST structures.

---

## 10. Best Practices & Design Guidelines
- Use a root "solution-style" `tsconfig.json` with `"files": []` and `"references": [...]`.
- Always enable `"declarationMap": true` alongside `"declaration": true` to preserve seamless IDE navigation.
- Add `*.tsbuildinfo` to `.gitignore`.
- Structure package graphs so that common interfaces and domain types sit at the leaves without upstream dependencies.
- In CI pipelines, use `tsc -b --force` to guarantee clean reproducible verification.

---

## 11. Real-World Case Study
A financial exchange monorepo had 18 micro-packages. Developers waited 90 seconds for `tsc` to verify changes, and memory consumption on CI runners exceeded 4GB, triggering frequent OOM kills.

**Refactoring with Project References:**
1. Split the monolithic config into 18 composite projects referencing a shared `tsconfig.base.json`.
2. Created a root solution `tsconfig.json` orchestrating all packages.
3. Enabled `.tsbuildinfo` caching in CI artifacts.
Cold builds dropped from 90s to 24s. Incremental warm builds in local development dropped from 90s to **1.8 seconds**, and memory usage peaked at just 600MB.

---

## 12. Interview Questions

<details>
<summary>1. What is the primary purpose of TypeScript Project References?</summary>

To decompose a large monolithic codebase or monorepo into smaller, modular projects that compile independently in dependency order, enabling incremental builds, parallel execution, and strict architectural boundaries.
</details>

<details>
<summary>2. What constraints does `"composite": true` impose on a `tsconfig.json`?</summary>

- `declaration` must be `true` (or defaults to `true`).
- `rootDir` must be explicitly specified (or defaults to the directory containing the config).
- All source files must be explicitly accounted for via `files` or `include`.
- Incremental build caching (`.tsbuildinfo`) is enabled.
</details>

<details>
<summary>3. Why is `tsc --build` required when using project references?</summary>

Standard `tsc` only compiles the single project it is pointed at and will not automatically compile referenced dependencies. `tsc --build` (`tsc -b`) understands the dependency graph, builds upstream projects first, and uses `.tsbuildinfo` to skip unchanged projects.
</details>

<details>
<summary>4. What does a "solution-style" `tsconfig.json` contain?</summary>

It typically has `"files": []` (no source files of its own) and a `"references"` array pointing to all subprojects in the repository. Running `tsc -b` at the root builds the entire repository.
</details>

<details>
<summary>5. What is stored inside a `.tsbuildinfo` file?</summary>

It contains a JSON record of source file hashes, compiler option hashes, emitted file mappings, and symbol dependency graphs used by the incremental engine to determine if a project needs rebuilding.
</details>

<details>
<summary>6. Why can project references not form circular dependencies?</summary>

Because TypeScript builds projects in topological order by compiling upstream packages into `.d.ts` files first. A cycle would mean Project A cannot be compiled without Project B's `.d.ts`, and Project B cannot be compiled without Project A's `.d.ts`, creating an unresolvable deadlock.
</details>

<details>
<summary>7. How does `"declarationMap": true` improve the developer experience in project references?</summary>

It emits source maps for `.d.ts` files (`.d.ts.map`). When an editor runs "Go to Definition" on a symbol imported from a referenced project, it navigates directly to the original `.ts` source file instead of the compiled `.d.ts` declaration.
</details>

<details>
<summary>8. How do you clean up all build artifacts produced by `tsc --build`?</summary>

Run `tsc --build --clean` (or `tsc -b --clean`).
</details>

---

## 13. Exercises & Challenges

### Easy
1. **Enable Composite Config**: Convert a standard `tsconfig.json` into a composite configuration for project references.
   <details>
   <summary>Solution</summary>

   ```json
   {
     "compilerOptions": {
       "composite": true,
       "declaration": true,
       "declarationMap": true,
       "rootDir": "./src",
       "outDir": "./dist"
     },
     "include": ["src/**/*"]
   }
   ```
   </details>

2. **Clean Build Command**: Write the exact command to force recompile a project reference tree ignoring all cached `.tsbuildinfo` files.
   <details>
   <summary>Solution</summary>

   ```bash
   tsc --build --force
   ```
   </details>

### Medium
3. **Two-Package Monorepo Setup**: Write `packages/shared/tsconfig.json` and `packages/app/tsconfig.json` such that `app` references `shared`.
   <details>
   <summary>Solution</summary>

   ```json
   // packages/shared/tsconfig.json
   {
     "compilerOptions": {
       "composite": true,
       "outDir": "./dist",
       "rootDir": "./src"
     },
     "include": ["src/**/*"]
   }

   // packages/app/tsconfig.json
   {
     "compilerOptions": {
       "composite": true,
       "outDir": "./dist",
       "rootDir": "./src"
     },
     "references": [
       { "path": "../shared" }
     ],
     "include": ["src/**/*"]
   }
   ```
   </details>

4. **Root Solution Config**: Create the root `tsconfig.json` for the above two-package setup.
   <details>
   <summary>Solution</summary>

   ```json
   {
     "files": [],
     "references": [
       { "path": "./packages/shared" },
       { "path": "./packages/app" }
     ]
   }
   ```
   </details>

### Hard
5. **DAG Cycle Detector for Project References**: Write a Node.js script using DFS that reads a root `tsconfig.json`, parses all referenced `tsconfig.json` files, and detects if there are any circular dependencies in the project reference graph.
   <details>
   <summary>Solution</summary>

   ```typescript
   import * as fs from 'fs';
   import * as path from 'path';

   interface TsConfig {
     references?: Array<{ path: string }>;
   }

   export function detectCycles(rootConfigPath: string): boolean {
     const visited = new Set<string>();
     const recStack = new Set<string>();

     function check(configPath: string): boolean {
       const resolved = path.resolve(configPath);
       const target = fs.statSync(resolved).isDirectory()
         ? path.join(resolved, 'tsconfig.json')
         : resolved;

       if (!fs.existsSync(target)) return false;

       visited.add(target);
       recStack.add(target);

       const json: TsConfig = JSON.parse(fs.readFileSync(target, 'utf8'));
       const refs = json.references || [];

       for (const ref of refs) {
         const nextPath = path.resolve(path.dirname(target), ref.path);
         const nextTarget = fs.statSync(nextPath).isDirectory()
           ? path.join(nextPath, 'tsconfig.json')
           : nextPath;

         if (!visited.has(nextTarget)) {
           if (check(nextTarget)) return true;
         } else if (recStack.has(nextTarget)) {
           console.error(`Circular dependency detected: ${target} -> ${nextTarget}`);
           return true;
         }
       }

       recStack.delete(target);
       return false;
     }

     return check(rootConfigPath);
   }
   ```
   </details>

6. **Automated Monorepo Reference Sync**: Implement a script that parses all `package.json` workspaces in a repository, reads each package's `dependencies`, and automatically synchronizes the `"references"` array in each package's `tsconfig.json` to match internal workspace dependencies.
   <details>
   <summary>Solution</summary>

   ```typescript
   import * as fs from 'fs';
   import * as path from 'path';

   export function syncMonorepoReferences(packagesDir: string): void {
     const pkgs = fs.readdirSync(packagesDir);
     const pkgNames = new Map<string, string>(); // name -> dir

     for (const p of pkgs) {
       const pPath = path.join(packagesDir, p, 'package.json');
       if (fs.existsSync(pPath)) {
         const json = JSON.parse(fs.readFileSync(pPath, 'utf8'));
         pkgNames.set(json.name, p);
       }
     }

     for (const p of pkgs) {
       const pJsonPath = path.join(packagesDir, p, 'package.json');
       const tsConfigPath = path.join(packagesDir, p, 'tsconfig.json');
       if (!fs.existsSync(pJsonPath) || !fs.existsSync(tsConfigPath)) continue;

       const pJson = JSON.parse(fs.readFileSync(pJsonPath, 'utf8'));
       const tsConfig = JSON.parse(fs.readFileSync(tsConfigPath, 'utf8'));
       const allDeps = { ...pJson.dependencies, ...pJson.devDependencies };

       const refs: Array<{ path: string }> = [];
       for (const dep of Object.keys(allDeps)) {
         if (pkgNames.has(dep) && pkgNames.get(dep) !== p) {
           refs.push({ path: `../${pkgNames.get(dep)}` });
         }
       }

       tsConfig.references = refs;
       fs.writeFileSync(tsConfigPath, JSON.stringify(tsConfig, null, 2) + '\n', 'utf8');
       console.log(`Synced references for package: ${pJson.name}`);
     }
   }
   ```
   </details>
