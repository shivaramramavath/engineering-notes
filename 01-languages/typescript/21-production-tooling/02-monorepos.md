# TypeScript Monorepos: Turborepo, Workspaces, and Project References

> Comprehensive guide to architecting scalable TypeScript monorepos: npm/pnpm workspaces, TypeScript Project References (`composite: true`), Turborepo caching pipelines, and internal package workflows.

| Metadata | Details |
| :--- | :--- |
| **Category** | Production Tooling |
| **Difficulty** | Advanced |
| **Prerequisites** | [13-compiler-options/00-tsconfig-json.md](../13-compiler-options/00-tsconfig-json.md), [21-production-tooling/01-build-and-bundling.md](./01-build-and-bundling.md) |
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

In modern organizations, managing separate repositories for each microservice, frontend application, shared utility library, and UI component creates immense friction:
- **Dependency Drift**: Package versions get out of sync, leading to subtle runtime incompatibilities.
- **Publishing Overhead**: Making a change in a shared DTO requires publishing an npm package before consuming it in a downstream service.
- **Fragmented Tooling**: Multiple disjoint configurations for linters, formatters, and compilers.

A **monorepo** unifies codebases in a single repository. However, large TypeScript monorepos face severe compiler challenges: `tsc` can run out of memory or take tens of minutes to compile hundreds of interdependent packages.

The solution relies on three synergistic technologies:
1. **pnpm / npm Workspaces**: Link local packages together using symlinks (`workspace:*`).
2. **TypeScript Project References**: Divide the codebase into smaller, incremental compilation units (`composite: true`, `references: [...]`).
3. **Turborepo**: Orchestrates task scheduling, topological dependency execution, and distributed remote build caching.

---

## 2. Core Concepts

### Package Manager Workspaces (`pnpm-workspace.yaml`)
Workspaces allow multiple packages located in `apps/*` and `packages/*` to reference each other without publishing to an npm registry:

```yaml
packages:
  - "apps/*"
  - "packages/*"
```

### TypeScript Project References
Project References allow TypeScript to model dependencies between projects explicitly.
- **`composite: true`**: Enforces strict constraints (must emit declarations, `rootDir` constraints, `noUnusedLocals`).
- **`references: [{ path: '../pkg' }]`**: Declares that this project depends on another.
- **Incremental builds via `tsc -b`**: TypeScript builds an internal dependency graph, skips already built packages using `.tsbuildinfo`, and only recompiles modified packages.

```
+-------------------------------------------------------------------------+
|                  Project Reference Dependency Graph                     |
+-------------------------------------------------------------------------+
|                                                                         |
|                          apps/web-app (tsc -b)                          |
|                                |                                        |
|               +----------------+----------------+                       |
|               |                                 |                       |
|               v                                 v                       |
|      packages/ui-components             packages/api-client             |
|               |                                 |                       |
|               +----------------+----------------+                       |
|                                |                                        |
|                                v                                        |
|                       packages/core-types                               |
|                     (composite: true, leaf)                             |
|                                                                         |
+-------------------------------------------------------------------------+
```

### Internal Package Strategy (Just-in-Time Transpilation vs. Precompiled)
- **Precompiled Packages**: Each package compiles itself to `dist/` before consumers can use it.
- **Just-in-Time (JIT) Packages**: Shared packages export raw `.ts` files, allowing consuming apps (Next.js, Vite) to transpile them directly.

---

## 3. How It Works Internally

When running `tsc --build` in a Project Reference architecture:
1. **Graph Resolution**: TypeScript parses the root `tsconfig.json` and navigates all `references`. It detects circular dependencies and constructs a Directed Acyclic Graph (DAG).
2. **Cache Inspection**: TypeScript inspects `.tsbuildinfo` files in each package. If input hashes have not changed, it skips re-checking that project.
3. **Declaration Synthesis**: For upstream packages, TypeScript generates `.d.ts` and `.d.ts.map` files. Downstream consumers consume only these lightweight declaration files instead of re-evaluating the full upstream implementation ASTs.
4. **Turborepo Task Execution**: Turborepo reads `turbo.json` and hashes package source files, environment variables, and dependencies to retrieve cached outputs from local or remote cache storage in sub-seconds.

---

## 4. Syntax & API Reference

### Root `turbo.json` Configuration
```json
{
  "$schema": "https://turbo.build/schema.json",
  "tasks": {
    "build": {
      "dependsOn": ["^build"],
      "outputs": ["dist/**", ".next/**", "!.next/cache/**"]
    },
    "typecheck": {
      "dependsOn": ["^typecheck"],
      "outputs": ["**/*.tsbuildinfo"]
    },
    "lint": {
      "dependsOn": ["^build"]
    },
    "dev": {
      "cache": false,
      "persistent": true
    }
  }
}
```

### Leaf Package `tsconfig.json` (`packages/core-types/tsconfig.json`)
```json
{
  "extends": "../../tsconfig.base.json",
  "compilerOptions": {
    "composite": true,
    "outDir": "./dist",
    "rootDir": "./src",
    "declaration": true,
    "declarationMap": true
  },
  "include": ["src"]
}
```

### Consuming Package `tsconfig.json` (`apps/web/tsconfig.json`)
```json
{
  "extends": "../../tsconfig.base.json",
  "compilerOptions": {
    "composite": true,
    "outDir": "./dist",
    "rootDir": "./src"
  },
  "references": [
    { "path": "../../packages/core-types" }
  ],
  "include": ["src"]
}
```

---

## 5. Practical Examples

### Example 1: Shared Internal Type Package
```typescript
// packages/core-types/src/user.ts
export interface UserIdentity {
  id: string;
  email: string;
  roles: readonly ("admin" | "member" | "viewer")[];
}

export interface AuthContext {
  user: UserIdentity;
  sessionId: string;
}
```

### Example 2: Consuming in an API Gateway Service
```typescript
// apps/api-gateway/src/handler.ts
import type { AuthContext, UserIdentity } from "@acme/core-types";

export function authorizeAdmin(ctx: AuthContext): boolean {
  return ctx.user.roles.includes("admin");
}

export function createAuditLog(user: UserIdentity): string {
  return `User ${user.email} (${user.id}) performed administrative action`;
}
```

### Example 3: Edge Case - Declaration Map Navigation Across Workspaces
```typescript
// When declarationMap: true is configured, jumping to definition in VS Code
// navigates straight to user.ts rather than packages/core-types/dist/user.d.ts.

import { UserIdentity } from "@acme/core-types";

export const systemAdmin: UserIdentity = {
  id: "sys_01",
  email: "admin@enterprise.internal",
  roles: ["admin"] as const,
};
```

### Example 4: Compiler Error & Project Reference Circularity
```typescript
// If packages/api-client references apps/web, while apps/web references packages/api-client:
// tsc -b fails with the following diagnostic:
// Error: TS6202: Project references may not form a circular graph.
// Cycle detected:
// apps/web/tsconfig.json -> packages/api-client/tsconfig.json -> apps/web/tsconfig.json

// Correct Pattern: Extract shared interfaces into packages/core-types
```

### Example 5: Turborepo Pipeline Script in Root `package.json`
```json
{
  "name": "enterprise-monorepo",
  "private": true,
  "scripts": {
    "build": "turbo run build",
    "typecheck": "turbo run typecheck",
    "lint": "turbo run lint",
    "clean": "turbo run clean && rm -rf node_modules"
  },
  "devDependencies": {
    "turbo": "^2.0.0",
    "typescript": "^5.5.0"
  }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Missing `declarationMap`**:
   Without `"declarationMap": true`, developers clicking "Go to Definition" in their IDE are sent into read-only generated `.d.ts` files inside `dist/`. Always enable `declarationMap: true` across all workspace packages.
2. **The "Phantom Dependency" Problem**:
   With npm or older yarn workspaces, hoisted packages allowed code to import dependencies that were not declared in the local `package.json`. Use **pnpm** with strict isolated node_modules (`node-linker=isolated`) to guarantee that undeclared dependencies fail at compile time.
3. **Out-of-Sync `.tsbuildinfo`**:
   Git branch switching can leave outdated `.tsbuildinfo` cache files, causing `tsc -b` to erroneously report that packages are up-to-date. Configure a `clean` task or invalidate caches when checking out disparate branches.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Practice | Good Practice | Problem / Diagnostic |
| :--- | :--- | :--- | :--- |
| **Circular Project References** | Package A -> Package B -> Package A | Extract shared definitions to Package C | `TS6202: Project references may not form a circular graph` |
| **Missing `composite: true`** | Referencing project without `composite` | Set `"composite": true` in referenced project | `TS6306: Referenced project must have setting "composite": true` |
| **Undeclared Workspace Dependency** | Importing `@acme/shared` without package.json entry | Add `"@acme/shared": "workspace:*"` | Runtime module resolution failure |
| **Monolithic `tsc` Execution** | Running `tsc` from root without Project References | Running `tsc -b` with Project References | OOM / catastrophic memory exhaustion |
| **Missing `outDir` in Composite** | Leaving `outDir` empty in composite project | Explicitly configure `"outDir": "./dist"` | `TS5069: Composite project may not specify 'outFile'` |
| **Relative Cross-Package Imports** | `import '../../packages/ui/Button'` | `import { Button } from '@acme/ui'` | Breaks encapsulation and bundler boundaries |

---

## 8. Performance & Type-Checker Complexity

In a monorepo containing 100 packages:
- **Single monolithic `tsc` run**: Evaluates millions of AST nodes simultaneously. Memory usage easily exceeds 4GB, causing V8 garbage collection thrashing.
- **Project References (`tsc -b`)**: Evaluates projects in topological slices. Once a project is built, its AST is discarded from memory; only lightweight `.d.ts` symbols are retained.
- **Turborepo Remote Cache**: Caches build and typecheck artifacts across CI runners and developer machines. Unchanged packages execute in 0.00s (`[FULL TURBO]`).

---

## 9. Compiler Internals

Under `tsc --build`:
1. The **Project Resolver** builds a graph of `ResolvedProjectReference` nodes.
2. The **Graph Sorter** applies topological sort algorithms (Tarjan's strongly connected components) to detect cycles.
3. The **Build Engine** checks `.tsbuildinfo` timestamps and signature hashes (`ts.getOutputDeclarationFileSize`).
4. If upstream `.d.ts` files have identical exported shapes, downstream packages are not invalidated even if the upstream `.ts` implementation changed!

---

## 10. Best Practices & Design Guidelines

1. **Inherit from a Single `tsconfig.base.json`**: Centralize common strict options (`strict`, `noUncheckedIndexedAccess`, `skipLibCheck`).
2. **Prefix Internal Packages**: Use a consistent naming convention like `@acme/*` or `@org/*`.
3. **Define Granular Packages**: Separate pure domain types (`@acme/types`) from infrastructure clients (`@acme/api-client`) and UI components (`@acme/ui`).
4. **Leverage Turborepo Remote Caching**: Configure Vercel or self-hosted S3 remote caching to achieve sub-minute CI build times.

---

## 11. Real-World Case Study

A scale-up organization operating 12 micro-frontends suffered from 25-minute CI pull-request checks. Every service re-compiled shared libraries from scratch.

**Monorepo Migration**:
1. Converted repository to **pnpm workspaces** and enabled TypeScript Project References.
2. Added `turbo.json` caching both `dist/**` and `.tsbuildinfo`.
3. Connected Turborepo to a centralized remote cache.

**Outcome**:
- Cached PR runs completed in under **40 seconds**.
- Cache hit rate reached **82%** across the engineering team.
- Zero type drift between frontend and backend DTOs.

---

## 12. Interview Questions

<details>
<summary>1. What is the role of `composite: true` in a TypeScript project?</summary>

`composite: true` enables TypeScript Project References. It forces the compiler to emit declarations (`.d.ts`), enforce strict `rootDir` boundaries, and generate `.tsbuildinfo` build state files so that downstream projects can consume its types without recompiling its source code.
</details>

<details>
<summary>2. How does `tsc -b` differ from running `tsc`?</summary>

`tsc` compiles a single project defined by a `tsconfig.json`. `tsc -b` (build mode) parses project references, calculates topological dependency order, performs incremental builds using `.tsbuildinfo`, and compiles or cleans multiple interdependent projects automatically.
</details>

<details>
<summary>3. What is the difference between pnpm workspaces and Turborepo?</summary>

pnpm workspaces is a package manager feature responsible for linking local repository folders together via symlinks and resolving dependencies. Turborepo is a build orchestration tool that schedules tasks (build, lint, test) in parallel based on dependency order and caches the task outputs.
</details>

<details>
<summary>4. Why does TypeScript fail with TS6202 in monorepos?</summary>

TS6202 signifies a circular dependency in Project References (e.g., A references B and B references A). TypeScript requires a Directed Acyclic Graph (DAG) to determine build order. The fix is to extract shared types into an independent leaf package.
</details>

<details>
<summary>5. What are "phantom dependencies" and how do modern monorepos prevent them?</summary>

Phantom dependencies occur when code imports a package that exists in `node_modules` due to hoisting, even though it is not declared in that package's own `package.json`. pnpm prevents this by creating a non-flat, symlinked virtual store where each package can only access its explicitly declared dependencies.
</details>

<details>
<summary>6. How does `declarationMap: true` assist developers in a monorepo?</summary>

It emits `.d.ts.map` source maps that link the emitted `.d.ts` types back to the original `.ts` source files. When an engineer uses IDE navigation ("Go to Definition") across packages, the IDE opens the editable source file rather than the compiled type stub.
</details>

<details>
<summary>7. What does Turborepo's `"^build"` syntax mean in `turbo.json`?</summary>

The caret `^` indicates a topological dependency: before task `build` can run in the current package, the `build` task must successfully complete in all of its direct workspace dependencies.
</details>

<details>
<summary>8. What is the difference between Just-in-Time (JIT) packages and precompiled packages in a monorepo?</summary>

JIT packages export uncompiled TypeScript source files directly, delegating transpilation to the consuming application (e.g. Next.js). Precompiled packages build their own `.js` and `.d.ts` into a `dist/` folder via tools like `tsup` before being consumed.
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Challenge 1 (Easy): Setup a pnpm-workspace.yaml file</summary>

**Task**: Write a `pnpm-workspace.yaml` file configuring two application directories (`apps/web`, `apps/api`) and multiple shared packages under `packages/*`.

**Solution**:
```yaml
packages:
  - "apps/*"
  - "packages/*"
```
</details>

<details>
<summary>Challenge 2 (Easy): Configure package.json with workspace protocol</summary>

**Task**: Show how `apps/api/package.json` should declare a dependency on local package `@acme/database`.

**Solution**:
```json
{
  "name": "@acme/api",
  "version": "1.0.0",
  "dependencies": {
    "@acme/database": "workspace:*"
  }
}
```
</details>

<details>
<summary>Challenge 3 (Medium): Author a base tsconfig and a composite leaf package tsconfig</summary>

**Task**: Write `tsconfig.base.json` and `packages/models/tsconfig.json` enabling composite references.

**Solution**:
`tsconfig.base.json`:
```json
{
  "compilerOptions": {
    "target": "ES2022",
    "module": "NodeNext",
    "moduleResolution": "NodeNext",
    "strict": true,
    "skipLibCheck": true
  }
}
```

`packages/models/tsconfig.json`:
```json
{
  "extends": "../../tsconfig.base.json",
  "compilerOptions": {
    "composite": true,
    "outDir": "./dist",
    "rootDir": "./src",
    "declaration": true,
    "declarationMap": true
  },
  "include": ["src"]
}
```
</details>

<details>
<summary>Challenge 4 (Medium): Configure turbo.json with strict topological typechecking</summary>

**Task**: Author a `turbo.json` where `typecheck` depends on upstream dependencies being typechecked first.

**Solution**:
```json
{
  "$schema": "https://turbo.build/schema.json",
  "tasks": {
    "typecheck": {
      "dependsOn": ["^typecheck"],
      "outputs": ["**/*.tsbuildinfo"]
    }
  }
}
```
</details>

<details>
<summary>Challenge 5 (Hard): Resolve a cyclic monorepo dependency</summary>

**Task**: `packages/users` imports `Order` from `packages/orders`, while `packages/orders` imports `User` from `packages/users`. Refactor this architecture into valid Project References.

**Solution**:
Create a new package `packages/common-types`:
```typescript
// packages/common-types/src/index.ts
export interface UserSummary {
  id: string;
  name: string;
}

export interface OrderSummary {
  orderId: string;
  amount: number;
}
```
Both `packages/users` and `packages/orders` reference `packages/common-types` instead of referencing each other, eliminating the circular dependency graph.
</details>

<details>
<summary>Challenge 6 (Hard): Root Incremental Build Script</summary>

**Task**: Write a Node.js automation script that executes `tsc --build --verbose` across the monorepo and measures compilation time for each referenced project.

**Solution**:
```typescript
import { spawn } from "node:child_process";

const start = performance.now();
console.log("Starting monorepo incremental build...");

const child = spawn("tsc", ["--build", "--verbose"], { stdio: "inherit", shell: true });

child.on("close", (code) => {
  const duration = ((performance.now() - start) / 1000).toFixed(2);
  if (code === 0) {
    console.log(`Incremental build succeeded in ${duration}s`);
  } else {
    console.error(`Build failed with exit code ${code}`);
    process.exit(code ?? 1);
  }
});
```
</details>
