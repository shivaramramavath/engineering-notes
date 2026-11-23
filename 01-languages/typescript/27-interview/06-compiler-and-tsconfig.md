# Module 27: TypeScript Compiler & tsconfig Interview Mastery

> **Topic Overview**: Deep-dive interview questions, compiler flag tradeoffs, emit mechanics, project references, and build pipeline troubleshooting for Senior and Lead TypeScript engineers.

---

## Metadata Table

| Field | Value |
|---|---|
| **Category** | Interview & Preparation |
| **Target Level** | Senior to Principal / Staff Engineer |
| **TypeScript Baseline** | TypeScript 5.0 - 5.8+ & TS 7.0 (Native Go Port `tsgo`) |
| **Focus Areas** | tsconfig.json, Project References, Module Resolution, Transpilation vs Checking |

---

## Key Interview Questions

### Q1: What is the exact distinction between `tsc` type checking and transpilation, and why do tools like Babel, SWC, Vite (esbuild), and Bun decouple them?
<details>
<summary><strong>Answer & Technical Analysis</strong></summary>

The TypeScript compiler (`tsc`) performs two fundamentally distinct operations:
1. **Type Checking (Semantic Analysis)**: Validating that all types, assignability rules, signatures, and interfaces hold. This requires reading the full Program graph, loading all declaration files (`.d.ts`), resolving cross-file imports, calculating symbol scopes, and computing type relationships ($O(N)$ to $O(N^2)$ algorithmic cost).
2. **Transpilation / Emit (Syntactic Stripping & Transformation)**: Removing type annotations, interfaces, enum declarations, and converting modern ECMAScript down to older targets (e.g., ES2015).

Modern build systems (Vite, SWC, Babel, esbuild, Bun) process files strictly **one-at-a-time (isolatedModules)**. They perform purely syntactic stripping without loading other files or resolving types. Consequently:
- Fast Transpilation: $10\times$ to $100\times$ faster builds in local dev loops (HMR).
- Isolated Check: `tsc --noEmit` is run out-of-band (e.g., in CI or a background terminal thread).

```json
// Required when using decoupled transpilars (Vite, SWC, Babel)
{
  "compilerOptions": {
    "isolatedModules": true,
    "noEmit": true
  }
}
```
**Failure Case with `isolatedModules`**:
```typescript
// types.ts
export type User = { id: string };

// app.ts
export { User } from "./types.js"; // TS1205: Re-exporting a type requires 'export type' under isolatedModules!
export type { User } from "./types.js"; // CORRECT
```
</details>

---

### Q2: What are TypeScript Project References (`composite: true`), and how do they enable incremental, scalable monorepo builds?
<details>
<summary><strong>Answer & Technical Analysis</strong></summary>

In large monorepos, running a monolithic `tsc` over thousands of files causes extreme memory consumption and slow build times. **Project References** break the codebase into independent, modular compilation units.

**Key Requirements for a Referenced Project**:
1. `"composite": true`: Requires `"declaration": true`, enforces exact file boundaries, enables `.tsbuildinfo` generation.
2. `"rootDir"` must encompass all inputs, preventing accidental leaks across subproject boundaries.
3. Consuming projects declare `"references": [{ "path": "../core" }]`.

```json
// packages/core/tsconfig.json
{
  "compilerOptions": {
    "composite": true,
    "declaration": true,
    "declarationMap": true,
    "outDir": "./dist",
    "rootDir": "./src"
  },
  "include": ["src"]
}

// packages/api/tsconfig.json
{
  "compilerOptions": {
    "composite": true,
    "noEmit": true
  },
  "references": [
    { "path": "../core" }
  ]
}
```

**Build Command**:
```bash
tsc --build --verbose packages/api
```
`tsc -b` tracks `.tsbuildinfo` timestamps and hashes, recompiling only modified dependency subprojects and reusing cached `.d.ts` declaration boundaries.
</details>

---

### Q3: Explain `moduleResolution: "NodeNext"` vs `"Bundler"`. When should each be chosen?
<details>
<summary><strong>Answer & Technical Analysis</strong></summary>

TypeScript 5.0 introduced refined module resolution modes matching Node.js ESM and modern bundler specifications:

| Setting | Target Runtime | Spec Compliance | File Extensions Required in Imports |
|---|---|---|---|
| `"node16"` / `"nodenext"` | Pure Node.js ESM / CJS dual packages | Exact Node.js ESM algorithm (`package.json#exports`) | **Yes** (`import from "./math.js"`) |
| `"bundler"` | Webpack 5, Vite, Rollup, esbuild | Hybrid bundler resolution | Optional (allows extensionless & directory index) |

**Why `NodeNext` requires `.js` extensions**:
Node.js ESM natively executes ECMAScript modules and requires exact relative file paths with `.js` extensions. TypeScript adheres to the principle that it never rewrites import specifiers during emit. Therefore, authoring `import { add } from "./math.js"` in TypeScript instructs TS to inspect `math.ts` at compile time and emit `./math.js` verbatim.

```typescript
// Under moduleResolution: "NodeNext"
import { calc } from "./calc";    // TS2835: Relative import paths need explicit file extensions in ECMAScript imports.
import { calc } from "./calc.js"; // Passes: points to ./calc.ts during compilation
```
</details>

---

### Q4: What are the performance and architectural implications of `"skipLibCheck": true`?
<details>
<summary><strong>Answer & Technical Analysis</strong></summary>

By default (`skipLibCheck: false`), TypeScript parses and type-checks the internal signatures of every `.d.ts` declaration file in `node_modules`.

**Drawbacks of `false`**:
1. Conflicting third-party type definitions (e.g., mismatched `@types/node` versions between nested dependencies) cause compile failures that application developers cannot fix directly.
2. Compilation time increases significantly (often $2\times - 5\times$ longer type-check passes).

**When `skipLibCheck: true` is standard**:
Virtually all modern production TS applications set `"skipLibCheck": true`. TS still checks type consumption at the application call-site; it simply skips verifying the internal mathematical consistency of vendor `.d.ts` files.
</details>

---

### Q5: How do `verbatimModuleSyntax` (TS 5.0+) and `preserveValueImports` behave, and how do they resolve import side-effects?
<details>
<summary><strong>Answer & Technical Analysis</strong></summary>

Prior to TS 5.0, `tsc` used heuristic elision: if an imported symbol was only used in type positions, `tsc` deleted the import from the JS output. This caused subtle bugs when an import was intended for runtime side-effects (e.g., polyfills, CSS imports, or DI decorators).

`verbatimModuleSyntax: true` enforces explicit intent:
- Any `import type { T } from "./mod"` is always completely dropped.
- Any regular `import { T } from "./mod"` is **always preserved** in emit, throwing a compiler error if `T` is only a type!
- Side-effect imports `import "./poly"` are preserved.

```typescript
// With "verbatimModuleSyntax": true
interface User { id: string }
class Engine {}

import { User, Engine } from "./models.js"; 
// TS1484: 'User' is a type and must be imported using a type-only import when 'verbatimModuleSyntax' is enabled.

// Correct:
import { Engine, type User } from "./models.js";
```
</details>\n