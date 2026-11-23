# Production tsconfig Recipes in TypeScript

> Battle-tested, production-ready `tsconfig.json` recipes for Node.js microservices (Node 22 ESM/CJS), Next.js App Router, Vite React SPAs, npm dual-package libraries, and monorepos.

| Metadata | Details |
| :--- | :--- |
| **Category** | Compiler & Configuration |
| **Difficulty** | Advanced |
| **Prerequisites** | [13-compiler-and-tsconfig/00-compiler-options.md](./00-compiler-options.md), [13-compiler-and-tsconfig/02-target-module-and-lib.md](./02-target-module-and-lib.md) |
| **TypeScript Baseline** | TS 5.0+, TS 5.4+ (`module: "preserve"`), Node 20/22 |
| **Tested tsconfig** | Production tested across Node 22, Vite 5, Next.js 14/15 |

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
Different runtime environments demand completely different TypeScript configurations. A backend service running on Node.js 22 LTS requires strict ESM compliance (`NodeNext`) and no DOM library contamination. A React frontend built with Vite requires `moduleResolution: "bundler"`, JSX transformations, and DOM types. An open-source npm library requires dual `.d.ts` declaration emission with isolated declarations.

Copy-pasting outdated or mismatched `tsconfig.json` files leads to subtle build failures, broken module resolution, and invisible type leakage. This guide provides copy-paste, battle-tested recipes for every modern production stack.

---

## 2. Core Concepts
- **Node.js 22 LTS (Pure ESM)**: Targets modern V8 with `module: "NodeNext"`, `verbatimModuleSyntax`, and strict types without DOM.
- **Frontend SPA (Vite / React 19)**: `"noEmit": true`, `moduleResolution: "bundler"`, `jsx: "react-jsx"`, and DOM declaration sets.
- **Next.js App Router**: Leverages Next.js's custom plugin system, server components, and incremental bundling.
- **NPM Library (Dual Package ESM/CJS)**: Emits declarations with `declaration: true`, `declarationMap: true`, and `isolatedDeclarations: true`.
- **Monorepo Composite Hierarchy**: Shared root configuration extended by independent composite packages.

---

## 3. How It Works Internally

```
                      Environment Profiles
                               |
         +---------------------+---------------------+
         |                                           |
         v                                           v
   Node.js Backend                             Frontend (Vite / Next)
   - Runtime: Node.js V8                       - Runtime: Browser Evergreen
   - Target: ES2022                            - Target: ES2022
   - Module: NodeNext                          - Module: ESNext
   - ModuleResolution: NodeNext                - ModuleResolution: bundler
   - Lib: ["ES2022"] (No DOM!)                 - Lib: ["ES2022", "DOM"]
   - Emit: JavaScript + Maps                   - Emit: noEmit: true (Vite bundles)
```

Each recipe aligns the compiler's module resolution, syntax downleveling, and declaration header sets with the actual tool that executes the code at runtime.

---

## 4. Syntax & API Reference

### Recipe 1: Modern Node.js 22 (Pure ESM)
```json
{
  "$schema": "https://json.schemastore.org/tsconfig",
  "compilerOptions": {
    "target": "ES2022",
    "module": "NodeNext",
    "moduleResolution": "NodeNext",
    "lib": ["ES2022"],
    "outDir": "./dist",
    "rootDir": "./src",
    "strict": true,
    "noUncheckedIndexedAccess": true,
    "verbatimModuleSyntax": true,
    "skipLibCheck": true,
    "forceConsistentCasingInFileNames": true,
    "sourceMap": true
  },
  "include": ["src/**/*"]
}
```

### Recipe 2: Vite + React 18/19 (Typecheck Only)
```json
{
  "$schema": "https://json.schemastore.org/tsconfig",
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
    "jsx": "react-jsx",
    "strict": true,
    "noUncheckedIndexedAccess": true
  },
  "include": ["src"]
}
```

### Recipe 3: Next.js 14/15 App Router
```json
{
  "$schema": "https://json.schemastore.org/tsconfig",
  "compilerOptions": {
    "target": "ES2022",
    "lib": ["dom", "dom.iterable", "esnext"],
    "allowJs": true,
    "skipLibCheck": true,
    "strict": true,
    "noEmit": true,
    "esModuleInterop": true,
    "module": "esnext",
    "moduleResolution": "bundler",
    "resolveJsonModule": true,
    "isolatedModules": true,
    "jsx": "preserve",
    "incremental": true,
    "plugins": [{ "name": "next" }],
    "paths": {
      "@/*": ["./src/*"]
    }
  },
  "include": ["next-env.d.ts", "**/*.ts", "**/*.tsx", ".next/types/**/*.ts"],
  "exclude": ["node_modules"]
}
```

### Recipe 4: Open Source NPM Library (Dual ESM/CJS & Isolated Declarations)
```json
{
  "$schema": "https://json.schemastore.org/tsconfig",
  "compilerOptions": {
    "target": "ES2022",
    "module": "NodeNext",
    "moduleResolution": "NodeNext",
    "declaration": true,
    "declarationMap": true,
    "isolatedDeclarations": true,
    "outDir": "./dist",
    "rootDir": "./src",
    "strict": true,
    "verbatimModuleSyntax": true,
    "skipLibCheck": true
  },
  "include": ["src/**/*"]
}
```

---

## 5. Practical Examples

### Example 1: Verifying `verbatimModuleSyntax` with Node.js 22
```typescript
// With verbatimModuleSyntax: true in Node 22
import { type User, calculateTax } from './tax.js';

// The emitter guarantees 'import { calculateTax } from "./tax.js"' is emitted,
// and 'User' is 100% erased, preventing runtime SyntaxError in Node ESM.
```

### Example 2: React Server Component Support in Next.js Recipe
```typescript
// In Next.js App Router, jsx: "preserve" allows Next.js compiler (Turbopack/SWC)
// to differentiate client components from server components before emitting.
export default async function UserPage({ params }: { params: { id: string } }) {
  const data = await fetchUserData(params.id);
  return <div>User: {data.name}</div>;
}
```

### Example 3: Edge Case - `isolatedDeclarations` in Library Recipe (TS 5.5+)
```typescript
// Under "isolatedDeclarations": true, all exported functions MUST have explicit return types:
export function multiply(a: number, b: number): number {
  return a * b; // OK
}

// @ts-expect-error TS9007: Function must have an explicit return type with --isolatedDeclarations.
export function untypedMultiply(a: number, b: number) {
  return a * b;
}
```

### Example 4: Compiler Failure Case - Illegal DOM Usage in Node Recipe
```typescript
// Compiling with Recipe 1 (Node.js pure ESM - No DOM):
// @ts-expect-error TS2584: Cannot find name 'window'. Do you need to change your target library?
console.log(window.location.href);
```

### Example 5: Monorepo Root Base Inheritance Recipe
```json
// tsconfig.base.json
{
  "compilerOptions": {
    "target": "ES2022",
    "strict": true,
    "skipLibCheck": true,
    "forceConsistentCasingInFileNames": true,
    "composite": true,
    "declaration": true,
    "declarationMap": true
  }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **`noEmit` with `allowImportingTsExtensions`**: If you enable `allowImportingTsExtensions: true` (e.g. `import './file.ts'`), you *must* set `"noEmit": true`. Attempting to emit `.js` with `.ts` extensions causes compiler error `TS5096`.
2. **Next.js Modifies Your tsconfig**: Next.js automatically rewrites portions of `tsconfig.json` during development if mandatory Next.js flags (like `jsx: "preserve"`, `plugins: [{ name: "next" }]`) are missing.
3. **`isolatedModules: true`**: Mandatory when using Vite, Babel, or esbuild. It prevents TypeScript features that require multi-file cross-type analysis during single-file transpilation (e.g. `const enum` without preserve, ambient re-exports).

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Consequence | Correction |
| :--- | :--- | :--- |
| Using `moduleResolution: "node"` with Vite | Fails to resolve `package.json` conditional exports | Use `moduleResolution: "bundler"` |
| Leaving `"DOM"` in pure Node.js services | Accidental use of browser globals | Remove `"DOM"` from `"lib"` |
| Omitting `isolatedDeclarations` in libraries | Slow down downstream Rust/Go declaration emitters | Enable `isolatedDeclarations: true` |
| Setting `"module": "CommonJS"` in Next.js | Breaks Next.js server components and tree shaking | Set `"module": "esnext"` |
| Forgetting `declarationMap: true` in npm libraries | Breaks IDE "Go to Source" navigation | Enable `"declarationMap": true` |

---

## 8. Performance & Type-Checker Complexity
- Using `"noEmit": true` and delegating emit to Vite/esbuild/SWC speeds up dev server reload times from $\sim 2\text{s}$ to $<50\text{ms}$.
- `isolatedDeclarations: true` (TS 5.5+) allows external ultra-fast compilers (like `tsgo`, Oxlint, or SWC) to generate `.d.ts` files in parallel across threads without loading the full type checker.

---

## 9. Compiler Internals
When `plugins: [{ name: "next" }]` is configured, TypeScript's language service hooks into Next.js's custom plugin compiler (`next/dist/build/swc`). This validates route segment configurations, typed links, and server action parameters inside the editor.

---

## 10. Best Practices & Design Guidelines
- Select the recipe matching your exact runtime and build tool.
- Keep `"strict": true` and `"skipLibCheck": true` active in all environments.
- Use `verbatimModuleSyntax: true` to prevent ambiguity between runtime values and compile-time types.
- Set `"noEmit": true` whenever Vite, esbuild, or Next.js handles bundling.
- Enable `"isolatedDeclarations": true` for shared libraries to future-proof for TypeScript 7 native tooling.

---

## 11. Real-World Case Study
A financial enterprise maintained a multi-package monorepo containing a React frontend, a Node.js API, and a shared domain library. Developers shared a single generic `tsconfig.json`. The backend accidentally imported browser DOM types, and the frontend broke on Node ESM imports.

**Remediation:**
The team migrated to dedicated recipes extending a shared base:
- `tsconfig.base.json`: Common strict flags.
- `apps/web/tsconfig.json`: Recipe 2 (Vite + DOM).
- `apps/api/tsconfig.json`: Recipe 1 (Node 22 ESM, No DOM).
- `packages/domain/tsconfig.json`: Recipe 4 (Dual ESM/DTS with isolated declarations).
Build failures vanished, and type integrity was strictly enforced per runtime environment.

---

## 12. Interview Questions

<details>
<summary>1. Why should `noEmit: true` be set in a Vite or Next.js project?</summary>

Because Vite (via esbuild/Rollup) and Next.js (via Turbopack/SWC) handle their own bundling and transpilation of TypeScript to JavaScript. Running `tsc` is done purely for static type verification, so emitting `.js` files from `tsc` would produce redundant or conflicting duplicate files.
</details>

<details>
<summary>2. What is the benefit of `isolatedDeclarations: true` introduced in TypeScript 5.5?</summary>

It forces library authors to explicitly type all exported function signatures and public members. This allows external multi-threaded tools (such as TypeScript 7 `tsgo`, SWC, or Biome) to generate `.d.ts` declaration files without needing to run the full, single-threaded TypeScript type checker.
</details>

<details>
<summary>3. Why does Next.js mandate `jsx: "preserve"`?</summary>

Next.js uses its own compiler (SWC/Turbopack) to process JSX so it can inject client/server boundaries (`"use client"`), perform automatic React imports, and optimize Server Components.
</details>

<details>
<summary>4. What flag prevents accidental browser globals (`window`, `document`) from appearing in a Node.js project?</summary>

Explicitly setting `"lib": ["ES2022"]` (or similar ES version) and omitting `"DOM"` from the `lib` array in `tsconfig.json`.
</details>

<details>
<summary>5. What does `isolatedModules: true` enforce?</summary>

It ensures that every file can be safely transpiled in isolation without relying on cross-file type information, forbidding features like `const enum` declarations and ambient namespace merging.
</details>

<details>
<summary>6. How does `verbatimModuleSyntax` improve modern Node.js ESM compilation?</summary>

It simplifies module emit rules: anything marked `import type` is completely dropped, and any standard `import` is emitted verbatim without TypeScript attempting to guess whether the imported symbol is a value or a type.
</details>

<details>
<summary>7. What is the difference between `moduleResolution: "bundler"` and `moduleResolution: "node10"`?</summary>

`bundler` understands `package.json` `"exports"` and modern package structures while permitting extensionless imports. `node10` uses legacy CommonJS folder scanning and ignores modern package `"exports"`.
</details>

<details>
<summary>8. How do you configure sourcemaps so that developers can debug original TypeScript in production error monitoring tools?</summary>

Set `"sourceMap": true` in `compilerOptions` and upload the generated `.js.map` files to your error tracking platform (e.g. Sentry, Datadog), while optionally withholding them from public static assets.
</details>

---

## 13. Exercises & Challenges

### Easy
1. **Node 22 Pure ESM Config**: Write a complete `tsconfig.json` for a Node 22 microservice that outputs to `./build` and includes strict null checks.
   <details>
   <summary>Solution</summary>

   ```json
   {
     "compilerOptions": {
       "target": "ES2022",
       "module": "NodeNext",
       "moduleResolution": "NodeNext",
       "lib": ["ES2022"],
       "outDir": "./build",
       "rootDir": "./src",
       "strict": true,
       "skipLibCheck": true
     },
     "include": ["src/**/*"]
   }
   ```
   </details>

2. **Frontend Typecheck Config**: Create a config for Vite React that performs strict typechecking with no emit.
   <details>
   <summary>Solution</summary>

   ```json
   {
     "compilerOptions": {
       "target": "ES2022",
       "module": "ESNext",
       "moduleResolution": "bundler",
       "lib": ["ES2022", "DOM"],
       "noEmit": true,
       "strict": true,
       "jsx": "react-jsx"
     },
     "include": ["src/**/*"]
   }
   ```
   </details>

### Medium
3. **Dual Package Declarations**: Configure a library `tsconfig.json` that outputs `.d.ts` declaration files with source maps into `./dist/types`.
   <details>
   <summary>Solution</summary>

   ```json
   {
     "compilerOptions": {
       "target": "ES2022",
       "module": "NodeNext",
       "moduleResolution": "NodeNext",
       "declaration": true,
       "declarationMap": true,
       "emitDeclarationOnly": true,
       "outDir": "./dist/types",
       "rootDir": "./src",
       "strict": true
     },
     "include": ["src/**/*"]
   }
   ```
   </details>

4. **Next.js Path Alias Config**: Write a `tsconfig.json` that configures Next.js App Router with path aliases for `@components/*` and `@lib/*`.
   <details>
   <summary>Solution</summary>

   ```json
   {
     "compilerOptions": {
       "target": "ES2022",
       "lib": ["dom", "dom.iterable", "esnext"],
       "module": "esnext",
       "moduleResolution": "bundler",
       "jsx": "preserve",
       "noEmit": true,
       "strict": true,
       "paths": {
         "@components/*": ["./src/components/*"],
         "@lib/*": ["./src/lib/*"]
       }
     },
     "include": ["src/**/*", "next-env.d.ts"]
   }
   ```
   </details>

### Hard
5. **Config Recipe Validator CI Tool**: Implement a CLI script that reads all `tsconfig.json` files across a repository and asserts that no backend service includes `"DOM"` in its `lib` and all frontend apps enable `noEmit: true`.
   <details>
   <summary>Solution</summary>

   ```typescript
   import * as fs from 'fs';
   import * as path from 'path';

   export function auditRepoConfigs(rootDir: string): void {
     const backendConfigPath = path.join(rootDir, 'apps/api/tsconfig.json');
     const frontendConfigPath = path.join(rootDir, 'apps/web/tsconfig.json');

     if (fs.existsSync(backendConfigPath)) {
       const backendConfig = JSON.parse(fs.readFileSync(backendConfigPath, 'utf8'));
       const libs: string[] = backendConfig.compilerOptions?.lib || [];
       if (libs.some(l => l.toUpperCase().includes('DOM'))) {
         throw new Error('VIOLATION: Backend tsconfig contains DOM library!');
       }
       console.log('Backend tsconfig verified: No DOM pollution.');
     }

     if (fs.existsSync(frontendConfigPath)) {
       const frontendConfig = JSON.parse(fs.readFileSync(frontendConfigPath, 'utf8'));
       if (frontendConfig.compilerOptions?.noEmit !== true) {
         throw new Error('VIOLATION: Frontend tsconfig must have noEmit: true!');
       }
       console.log('Frontend tsconfig verified: noEmit is true.');
     }
   }
   ```
   </details>

6. **Automated Isolated Declarations Migration Assistant**: Write a TypeScript linter script using the TypeScript Compiler API that parses all exported functions in a package, identifies any function missing an explicit return type annotation, and outputs the exact file line number to fix before enabling `isolatedDeclarations`.
   <details>
   <summary>Solution</summary>

   ```typescript
   import * as ts from 'typescript';

   export function findMissingReturnTypes(fileNames: string[], options: ts.CompilerOptions): void {
     const program = ts.createProgram(fileNames, options);

     for (const sourceFile of program.getSourceFiles()) {
       if (sourceFile.isDeclarationFile) continue;

       ts.forEachChild(sourceFile, node => {
         if (ts.isFunctionDeclaration(node) && node.modifiers?.some(m => m.kind === ts.SyntaxKind.ExportKeyword)) {
           if (!node.type) {
             const { line, character } = sourceFile.getLineAndCharacterOfPosition(node.getStart());
             console.warn(
               `[Missing Return Type] ${sourceFile.fileName}:${line + 1}:${character + 1} - Function '${node.name?.text}' requires explicit return type for isolatedDeclarations.`
             );
           }
         }
       });
     }
   }
   ```
   </details>
