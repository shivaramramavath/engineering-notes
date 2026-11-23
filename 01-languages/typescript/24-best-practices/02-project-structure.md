# TypeScript Project Structure: Architecture, Barrels, and Aliases

> Enterprise guide to organizing large-scale TypeScript repositories: feature-sliced design vs layer-based architectures, barrel file performance traps, circular imports, and path alias hygiene.

| Metadata | Details |
| :--- | :--- |
| **Category** | Best Practices |
| **Difficulty** | Intermediate to Advanced |
| **Prerequisites** | [13-compiler-options/00-tsconfig-json.md](../13-compiler-options/00-tsconfig-json.md), [21-production-tooling/02-monorepos.md](../21-production-tooling/02-monorepos.md) |
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

As a TypeScript project grows beyond tens of thousands of lines, poor folder architecture creates severe developer friction and operational risks:
- **Barrel File Traps (`index.ts`)**: Aggregating hundreds of exports in root `index.ts` files destroys tree-shaking, explodes dev-server memory usage, and causes massive reload latency.
- **Circular Import Deadlocks**: Two modules importing each other cause `TypeError: Cannot access 'X' before initialization` at runtime.
- **Spaghetti Import Paths**: Relative path climbing (`../../../../services/user`) makes code refactoring tedious and fragile.

Architecting a sustainable TypeScript project requires disciplined folder patterns (such as Feature-Sliced Design), selective direct exports, clean path aliases (`@/`), and automated circular dependency detection.

---

## 2. Core Concepts

### Layer-Based vs. Feature-Based Architecture
- **Layer-Based (Technical Grouping)**: Groups files by technical role (`controllers/`, `services/`, `models/`). Highly cohesive for small apps, but painful for large teams as single features touch 10 folders.
- **Feature-Based (Domain Grouping)**: Groups code by business capability (`features/billing/`, `features/auth/`). Highly scalable, encapsulates internal domain logic, and provides clear boundaries.

```
+-------------------------------------------------------------------------+
|                  Feature-Sliced Project Architecture                    |
+-------------------------------------------------------------------------+
|                                                                         |
|  src/                                                                   |
|   +-- features/                                                         |
|   |    +-- billing/                                                     |
|   |    |    +-- components/                                             |
|   |    |    +-- services/                                               |
|   |    |    +-- types.ts                                                |
|   |    |    +-- index.ts (Public API of billing only)                   |
|   |    +-- auth/                                                        |
|   +-- shared/                                                           |
|        +-- ui/                                                          |
|        +-- utils/                                                       |
|                                                                         |
|  Rule: Features NEVER import internal files of other features!          |
|  Only import from feature root public index.ts.                         |
|                                                                         |
+-------------------------------------------------------------------------+
```

### The Barrel File Problem
A "barrel file" re-exports all modules in a folder:
```typescript
// index.ts
export * from './Button';
export * from './Table';
export * from './Modal';
```
When an application imports `{ Button } from '@/components'`, bundlers and test runners (Jest/Node) parse, evaluate, and hold **every single component** in memory, drastically slowing down cold starts and hot reloads.

---

## 3. How It Works Internally

1. **Path Mapping**: When `paths` are configured in `tsconfig.json`, `ts.resolveModuleName` maps alias patterns (`@/*`) to target filesystem directories (`src/*`).
2. **Circular Graph Detection**: When modules A and B import each other, Node's module loader evaluates module A until it hits the import of B. It then switches to B, which imports A. Because A is still partially evaluated, exported bindings on A are `undefined` (Temporal Dead Zone).

---

## 4. Syntax & API Reference

### Clean Path Mapping in `tsconfig.json`
```json
{
  "compilerOptions": {
    "baseUrl": ".",
    "paths": {
      "@/*": ["src/*"],
      "@shared/*": ["src/shared/*"],
      "@features/*": ["src/features/*"]
    }
  }
}
```

### Vitest / Vite Path Resolution (`vite.config.ts`)
```typescript
import { defineConfig } from "vite";
import path from "node:path";

export default defineConfig({
  resolve: {
    alias: {
      "@": path.resolve(__dirname, "./src"),
    },
  },
});
```

---

## 5. Practical Examples

### Example 1: Avoiding Heavy Barrel Imports
```typescript
// SLOW (Loads 500 components into memory):
// import { Button } from '@/components';

// FAST (Direct path import loads only Button):
import { Button } from "@/components/button/button.js";
```

### Example 2: Circular Dependency Detection via Madge
```bash
# Terminal command detecting circular dependencies in src/
$ npx madge --circular --extensions ts src/

Processed 145 files (1.2s)
✖ Found 1 circular dependency!
1) features/auth/service.ts > features/auth/session.ts > features/auth/service.ts
```

### Example 3: Edge Case - Breaking Circular Reference via Interface Decoupling
```typescript
// circular_bad.ts:
// Service imports Repository, Repository imports Service.

// Solution: Extract common interface into types.ts
// types.ts
export interface UserRecord {
  id: string;
  email: string;
}

export interface IUserRepository {
  findById(id: string): Promise<UserRecord | null>;
}

// Service depends ONLY on IUserRepository interface, breaking the cycle!
```

### Example 4: Compiler Error & Path Alias Misconfiguration
```typescript
// If tsconfig has paths, but build tool (tsup/vite) lacks matching alias:
// The compiler passes, but runtime throws:
// Error: Cannot find package '@/utils/format' imported from src/index.js
```

### Example 5: Feature Encapsulation via Package Exports or ESLint
```javascript
// eslint.config.mjs rule forbidding cross-feature deep imports
export default [
  {
    rules: {
      'no-restricted-imports': [
        'error',
        {
          patterns: [
            {
              group: ['@features/*/*/**'],
              message: 'Direct imports from internal feature files are forbidden. Use public feature entry point.',
            },
          ],
        },
      ],
    },
  },
];
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Path Aliases Do Not Transpile Automatically**:
   `tsc` does not rewrite `@/*` paths into relative paths (`./`) in the emitted JavaScript by default! You must use bundlers (`tsup`, `esbuild`) or `tsconfig-paths` at runtime.
2. **Barrel Files in Libraries**:
   Published libraries with barrel files force consumers to load megabytes of unused code unless the package specifies `"sideEffects": false` in `package.json`.
3. **Temporal Dead Zone (TDZ) in Circular Classes**:
   If Class A extends Class B and they exist in circular files, Node.js throws `TypeError: Super expression must either be null or a function` on startup.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Practice | Good Practice | Problem / Diagnostic |
| :--- | :--- | :--- | :--- |
| **Giant Root Barrel** | `export * from './everything'` | Direct imports or granular subpath barrels | Slow dev server, high RAM |
| **Deep Relative Paths** | `import '../../../../utils'` | Configure clean `@/*` path aliases | Refactoring nightmare |
| **Circular Imports** | Module A -> Module B -> Module A | Decouple via shared types | TDZ runtime exceptions |
| **Unmapped TS Paths in Bundler** | Setting `paths` only in tsconfig | Mirror aliases in Vite/tsup config | Module not found at runtime |
| **Leaking Feature Internals** | Importing private feature helpers | Enforce public index boundaries | Tight coupling |
| **Side Effects in Barrels** | Top-level execution inside barrel | Keep barrels purely declarative | Accidental initialization |

---

## 8. Performance & Type-Checker Complexity

- Removing large barrel files can reduce cold test-suite startup times (Vitest/Jest) by **50%–70%**.
- Deep import paths allow bundlers like esbuild to perform tree shaking with minimal graph traversal.

---

## 9. Compiler Internals

When resolving module names:
1. `ts.resolveModuleName` reads `compilerOptions.paths`.
2. It attempts pattern matching sequentially.
3. If matched, it tests candidates against `baseUrl`.
4. It resolves to a `.ts`, `.tsx`, or `.d.ts` file without mutating import paths during emit.

---

## 10. Best Practices & Design Guidelines

1. **Group Code by Domain Feature**: Prefer Feature-Sliced Architecture over arbitrary technical folders.
2. **Avoid Heavy Barrel Files**: Use direct imports for large component suites.
3. **Audit Circular Dependencies in CI**: Run `madge --circular` as a blocking pull request gate.
4. **Synchronize Path Aliases**: Ensure tsconfig, bundlers, and test runners share identical alias mappings.

---

## 11. Real-World Case Study

A Next.js enterprise dashboard with 3,000 components took 45 seconds to load local development pages.

**Investigation**:
All icons and UI elements were imported through a single barrel: `import { Icon, Button } from '@/components'`. On every page visit, Next.js was forced to compile and evaluate all 3,000 components.

**Remediation**:
1. Enabled `optimizePackageImports` in Next.js config.
2. Refactored direct imports: `import { Button } from '@/components/button'`.
3. Deleted recursive root barrel files.

**Outcome**: Local page compile latency dropped from **45 seconds** to **1.2 seconds**.

---

## 12. Interview Questions

<details>
<summary>1. What are the performance hazards of barrel files (`index.ts`) in large TypeScript projects?</summary>

Barrel files force bundlers, test runners, and dev servers to load, parse, and evaluate all re-exported files into memory even if only one small utility is imported, causing severe compilation delays and breaking tree shaking.
</details>

<details>
<summary>2. Why doesn't `tsc` rewrite `paths` aliases in the emitted JavaScript files?</summary>

TypeScript deliberately avoids rewriting module specifiers during emit. Path aliases are intended to inform the type checker of runtime resolution patterns configured in the runtime host (e.g. Node loaders or bundlers).
</details>

<details>
<summary>3. What is Feature-Sliced Design (FSD)?</summary>

FSD is an architectural methodology that organizes code into hierarchical layers (app, pages, features, entities, shared) based on domain concepts rather than technical file types, establishing strict unidirectional dependency flows.
</details>

<details>
<summary>4. What causes the Temporal Dead Zone (TDZ) error in circular dependencies?</summary>

When module A imports module B, and B imports A, the JavaScript engine begins evaluating B before A has completed. If B accesses a class or variable exported by A that has not yet been initialized, a TDZ `ReferenceError` is thrown.
</details>

<details>
<summary>5. How can circular dependencies be resolved architecturally?</summary>

By extracting the mutually referenced types or functions into a separate, independent leaf module (e.g. `types.ts` or `common.ts`) that neither module depends on circularly.
</details>

<details>
<summary>6. How do you prevent circular dependencies automatically in CI?</summary>

By running circular dependency analyzers such as `madge --circular --extensions ts src` in continuous integration pipelines.
</details>

<details>
<summary>7. What is the benefit of `"sideEffects": false` in `package.json`?</summary>

It informs bundlers (Webpack, Rollup, Vite) that the package's modules do not execute global side effects on import, enabling bundlers to safely tree-shake unused exports even when imported through barrels.
</details>

<details>
<summary>8. How do you restrict cross-feature imports in a monorepo?</summary>

By using ESLint rules (such as `no-restricted-imports`) or tools like Nx and Turborepo to enforce boundary boundaries between feature folders.
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Challenge 1 (Easy): Configure path aliases in tsconfig</summary>

**Task**: Configure `tsconfig.json` so `@/*` maps to `src/*`.

**Solution**:
```json
{
  "compilerOptions": {
    "baseUrl": ".",
    "paths": {
      "@/*": ["src/*"]
    }
  }
}
```
</details>

<details>
<summary>Challenge 2 (Easy): Add circular dependency check script</summary>

**Task**: Add a script to `package.json` that fails if circular dependencies exist in `src/`.

**Solution**:
```json
{
  "scripts": {
    "check:circular": "madge --circular --extensions ts src/"
  }
}
```
</details>

<details>
<summary>Challenge 3 (Medium): Break a circular dependency via interface extraction</summary>

**Task**: Refactor two classes where `OrderService` uses `CustomerService` and `CustomerService` uses `OrderService`.

**Solution**:
Extract shared interfaces into `order-types.ts` and `customer-types.ts`:
```typescript
// order-types.ts
export interface IOrderService {
  cancelOrdersForCustomer(customerId: string): Promise<void>;
}

// customer-service.ts
import type { IOrderService } from "./order-types.js";

export class CustomerService {
  constructor(private orderService: IOrderService) {}
}
```
</details>

<details>
<summary>Challenge 4 (Medium): ESLint rule for public feature API enforcement</summary>

**Task**: Configure an ESLint rule preventing deep imports from `src/features/*/internal/*`.

**Solution**:
```javascript
export default [
  {
    rules: {
      'no-restricted-imports': [
        'error',
        {
          patterns: ['@/features/*/internal/*'],
        },
      ],
    },
  },
];
```
</details>

<details>
<summary>Challenge 5 (Hard): Scripted Barrel Audit and Elimination</summary>

**Task**: Write a Node.js script that scans `src/` and flags any `index.ts` file that re-exports more than 10 files.

**Solution**:
```typescript
import fs from "node:fs";
import path from "node:path";

export function findHeavyBarrels(dir: string) {
  const entries = fs.readdirSync(dir, { withFileTypes: true });
  for (const entry of entries) {
    const fullPath = path.join(dir, entry.name);
    if (entry.isDirectory()) {
      findHeavyBarrels(fullPath);
    } else if (entry.name === "index.ts") {
      const content = fs.readFileSync(fullPath, "utf-8");
      const exportMatches = content.match(/export\s+\*\s+from/g) ?? [];
      if (exportMatches.length > 10) {
        console.warn(`Heavy Barrel detected (${exportMatches.length} exports): ${fullPath}`);
      }
    }
  }
}
```
</details>

<details>
<summary>Challenge 6 (Hard): Automated Path Alias Tsconfig Validator</summary>

**Task**: Write a TypeScript script using Node's `fs` that reads `tsconfig.json` and verifies that all target alias directories actually exist on disk.

**Solution**:
```typescript
import fs from "node:fs";
import path from "node:path";

export function validateTsconfigPaths() {
  const tsconfig = JSON.parse(fs.readFileSync("tsconfig.json", "utf-8"));
  const paths = tsconfig.compilerOptions?.paths ?? {};

  for (const [alias, targets] of Object.entries<string[]>(paths)) {
    for (const target of targets) {
      const cleanPath = target.replace(/\/\*$/, "");
      if (!fs.existsSync(cleanPath)) {
        throw new Error(`Invalid path mapping: Alias '${alias}' points to non-existent path '${cleanPath}'`);
      }
    }
  }
  console.log("All tsconfig path aliases resolved to valid directories.");
}
```
</details>
