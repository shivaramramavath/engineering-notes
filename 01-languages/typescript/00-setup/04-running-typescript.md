# Running TypeScript: Runtimes, Transpilers & Native Execution

> A definitive architectural comparison of executing TypeScript across Node.js (native type stripping vs loaders), tsx, ts-node, Bun, Deno, and native binaries.

| Metadata | Value |
| :--- | :--- |
| **Level** | Beginner / Intermediate |
| **Prerequisites** | [00-toolchain.md](file:///e:/Learning/engineering-notes/01-languages/typescript/00-setup/00-toolchain.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | TS 5.0+ / Node.js 22 LTS / Bun 1.x / Deno 2.x |
| **Tested tsconfig** | `strict: true`, `target: ES2022`, `moduleResolution: NodeNext` |

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

### Problem Statement
Historically, executing a `.ts` file required pre-compiling it to `.js` with `tsc`, generating source maps, and running the emitted file with `node`. In modern development, engineers expect instant script execution, hot module reloading, and REPL environments without maintaining parallel build directories.

### Historical Context
`ts-node` was the long-standing standard for running TypeScript in Node.js by intercepting module resolution. However, as ESM and multi-threaded bundlers emerged, `ts-node` struggled with performance and loader hooks. Today, developers choose between ultra-fast transpilation runners (`tsx`), native runtime support (Bun, Deno), and Node.js native type stripping (`node --experimental-strip-types`).

### Real-World Relevance
Selecting the correct execution method dictates developer feedback loops, CI/CD pipeline costs, container startup latency, and production memory consumption.

---

## 2. Core Concepts

### Execution Strategies Matrix

| Runner / Runtime | Type-Checking? | Transpiler Engine | ESM Support | Startup Latency | Production Ready? |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **`tsc` + `node`** | **Yes** (Full Semantic Check) | TypeScript Compiler | Native | Slow (2–10s) | **Yes** (Standard compilation) |
| **`node` (Native Stripping)** | **No** (Whitespace Erasure) | Node.js Internal C++ Lexer | Native | **Instant (< 20ms)** | **Yes** (Node.js 22+) |
| **`tsx`** | **No** (Fast Transpile) | esbuild (Go) | Native | Fast (< 50ms) | Development / Tooling |
| **`ts-node`** | Configurable (Slow by default) | `tsc` or SWC | Partial | Slow (500ms–2s) | Legacy Tooling |
| **Bun** | **No** (JSC Runtime Transpile) | Native Zig Transpiler | Native | **Instant (< 10ms)** | Development & Production |
| **Deno** | Configurable (`deno check`) | SWC (Rust) | Native | Fast (< 30ms) | Development & Production |

```
                              TYPESCRIPT EXECUTION PATHWAYS
                                            │
         ┌──────────────────────────────────┴──────────────────────────────────┐
         ▼                                                                     ▼
   Two-Step Pipeline (Standard Build)                             Direct Execution (Modern Runtimes)
   1. tsc (Full type checking & emit)                             1. Node.js 22+ (Native type stripping)
   2. node dist/index.js                                          2. tsx / esbuild (Memory transpile)
                                                                  3. Bun / Deno (Native runtime integration)
```

---

## 3. How It Works Internally

### Node.js Native Type Stripping vs Transpilation
Node.js 22.6+ includes a built-in type stripper based on `amaro` (a lightweight C++ wrapper around SWC). When you execute `node --experimental-strip-types app.ts`:
1. Node reads the raw `.ts` bytes.
2. The lexer identifies type annotations, interfaces, and type imports.
3. It replaces those tokens with exact-width whitespace (spaces and line breaks).
4. The sanitized string is passed directly into V8's compilation pipeline.
5. **No AST modification occurs**, ensuring line numbers and column offsets in V8 stack traces match the source code exactly.

---

## 4. Syntax & API Reference

### Execution Commands

```bash
# 1. Native Node.js 22 LTS Type Stripping
node --experimental-strip-types src/main.ts

# 2. Modern Development Runner (tsx)
npx tsx src/main.ts
npx tsx watch src/main.ts # Built-in watch mode

# 3. Bun Native Execution
bun run src/main.ts

# 4. Deno Native Execution
deno run src/main.ts
deno check src/main.ts # Explicit type checking

# 5. Traditional Two-Step Compilation
npx tsc && node dist/main.js
```

---

## 5. Practical Examples

### Example 1: Native Execution with Node.js 22
- **Goal:** Run an asynchronous file reading script natively in Node.js.
- **Code (`script.ts`):**
```typescript
import fs from 'node:fs/promises';

interface PackageJson {
  name: string;
  version: string;
}

async function inspectPackage(): Promise<void> {
  const content = await fs.readFile('package.json', 'utf-8');
  const pkg: PackageJson = JSON.parse(content);
  console.log(`Package: ${pkg.name} (v${pkg.version})`);
}

inspectPackage();
```
- **CLI Output:**
```bash
$ node --experimental-strip-types script.ts
Package: ts-learning (v1.0.0)
```

### Example 2: Continuous Watch Mode with `tsx`
- **Goal:** Hot-reload development scripts on file changes.
- **CLI Command:**
```bash
$ npx tsx watch src/server.ts
[tsx] watching src/server.ts
[server] Listening on port 3000...
```

### Example 3: Verifying Bun's Native TypeScript Support
- **Goal:** Execute a TypeScript script using Bun's built-in standard library.
- **Code (`bun-test.ts`):**
```typescript
interface HealthResponse {
  status: 'ok' | 'degraded';
  uptimeSeconds: number;
}

const server = Bun.serve({
  port: 4000,
  fetch() {
    const payload: HealthResponse = {
      status: 'ok',
      uptimeSeconds: process.uptime()
    };
    return Response.json(payload);
  }
});

console.log(`Bun HTTP server active on :${server.port}`);
```

### Example 4: Negative Test — Unsupported Non-Erasable Syntax in Native Node
- **Goal:** Demonstrate failure when attempting to run non-erasable enums under native type stripping.
- **Code (`enum-fail.ts`):**
```typescript
enum Role {
  Admin = 'ADMIN',
  User = 'USER'
}

console.log(Role.Admin);
```
- **Real Runtime Output:**
```
SyntaxError: The requested module contains non-erasable syntax.
Enums, namespaces, and parameter properties are not supported with type-stripping.
```

### Example 5: Idiomatic Cross-Runtime Code
- **Goal:** Author code that runs cleanly across Node, Bun, and Deno without transpilation errors.
- **Code:**
```typescript
export const UserRole = {
  Admin: 'ADMIN',
  User: 'USER'
} as const;

export type UserRole = (typeof UserRole)[keyof typeof UserRole];

export function canAccessAdmin(role: UserRole): boolean {
  return role === UserRole.Admin;
}

console.log('Can access:', canAccessAdmin(UserRole.Admin));
```
- **Output:**
```
Can access: true
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **False Sense of Security**: Neither `node --experimental-strip-types`, `tsx`, nor `bun` perform type-checking! If you write `const x: number = "hello"`, they will execute it without error. You **must** run `tsc --noEmit` in CI/CD.
2. **`tsconfig.json` `paths` Ignored**: Native type strippers do not read your `tsconfig.json`. Path mappings like `@core/utils` will cause runtime `MODULE_NOT_FOUND` errors unless resolved by loaders or package subpath exports (`package.json#imports`).
3. **CommonJS / ESM Mixing in `ts-node`**: `ts-node` struggles when mixing ESM `.ts` files with CommonJS dependencies. `tsx` handles this automatically via esbuild interop.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Real Problem | Idiomatic Solution | Why It Matters |
| :--- | :--- | :--- | :--- |
| Relying on `ts-node` in production containers | High container startup time and memory bloat | Compile to JS with `tsc`/`tsup`, run with `node` | Production images should be pure JS |
| Forgetting to run `tsc --noEmit` in CI | Type errors slip into production undetected | Add `npm run typecheck` to GitHub Actions | Type strippers do zero type verification |
| Using enums with Node native type stripping | Runtime `SyntaxError: non-erasable syntax` | Use `as const` object dictionaries | Ensures universal zero-build execution |
| Using `tsconfig.json` `paths` without package imports | Breaks native Node and Bun execution | Use native Node subpath imports (`#utils/*`) | Standardized by ECMAScript across all runtimes |
| Running `tsc` without `skipLibCheck` during dev | 5–10 second delay on simple script runs | Use `tsx` or native Node for quick runs | Developer flow requires instant iteration |

---

## 8. Performance & Type-Checker Complexity

- **Cold Start Comparison**:
  - `tsc` full compilation: ~1,500ms – 5,000ms.
  - `ts-node`: ~800ms – 2,000ms.
  - `tsx`: ~40ms – 80ms.
  - Node.js native type stripping: ~15ms – 25ms.
  - Bun native execution: ~5ms – 10ms.

---

## 9. Compiler Internals

When a type stripper processes code, it walks the token stream:
```
Token Stream: [const] [id: "x"] [colon] [string] [equals] [quote: "hello"]
                                 ▲─────────────▲
                              Erased to Whitespace
Sanitized:    [const] [id: "x"] [     ] [      ] [equals] [quote: "hello"]
```
Because length and line positions remain constant, V8's source map offset equals zero.

---

## 10. Best Practices & Design Guidelines

1. **Development**: Use `tsx` or `node --experimental-strip-types` for fast script running.
2. **Type Verification**: Run `tsc --noEmit` in pre-commit hooks and CI pipelines.
3. **Production Deployment**: Compile to JavaScript using a bundler (tsup/esbuild) or `tsc`, deploying pure `.js` artifacts into minimal Docker containers.
4. **Use Erasable Syntax**: Enable `"erasableSyntaxOnly": true` in `tsconfig.json` to prevent accidental non-erasable dependencies.

---

## 11. Real-World Case Study

A high-frequency algorithmic trading startup ran integration test suites containing 800 test files. Using `ts-node`, tests took 4 minutes to start executing, causing developer friction.

### Migration:
1. Switched test runner to `vitest` with native esbuild stripping.
2. Replaced development CLI runners with `tsx`.
3. Added a dedicated `tsc --noEmit` step to CI.

**Result**: Test startup dropped from 240 seconds to 3.2 seconds; test run completion time decreased by 78%.

---

## 12. Interview Questions

### Q1: Can Node.js execute TypeScript files natively?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Yes, starting in Node.js 22.6.0 via the `--experimental-strip-types` flag (and enabled by default in recent versions for erasable syntax). Node uses an internal C++ lexer to erase type annotations into whitespace before passing the code to V8. It does not perform type checking and does not support non-erasable syntax like enums or parameter properties.
</details>

### Q2: Why is running `tsc` in production containers considered an anti-pattern?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Running `tsc` inside production containers increases Docker image sizes (requiring devDependencies and the TypeScript compiler), wastes CPU/memory during container boot-up, and slows down auto-scaling horizontally. Production containers should contain pre-compiled JavaScript artifacts executed directly by Node.js.
</details>

### Q3: What is the primary difference between `tsx` and `ts-node`?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

`ts-node` uses the TypeScript compiler or SWC and hooks into Node's module loader, often attempting to type-check by default (unless configured otherwise). `tsx` is powered by esbuild; it strips types in memory with zero type-checking, supports ESM natively without complex configuration, and starts in milliseconds.
</details>

### Q4: How do subpath imports in `package.json` replace `tsconfig.json` `paths`?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

TypeScript `paths` only informs `tsc` during type-checking and is not emitted into JavaScript. Node.js native subpath imports (`"imports": { "#utils/*": "./src/utils/*" }` in `package.json`) are a standardized ECMAScript feature recognized by Node.js, Bun, Deno, and TypeScript (under `NodeNext`). They resolve aliases natively at runtime without bundler path-rewriting plugins.
</details>

---

## 13. Exercises & Challenges

### Exercise 1 (Easy): Run a TypeScript Script with tsx
- **Problem Statement**: Write a package.json script to run `src/cli.ts` in watch mode using `tsx`.
- **Solution**:
```json
{
  "scripts": {
    "dev": "tsx watch src/cli.ts"
  }
}
```

### Exercise 2 (Medium): Author an Erasable Syntax Check
- **Problem Statement**: Write a regex or AST check that inspects a file and returns `false` if it contains `enum ` or `namespace `.
- **Solution**:
```typescript
export function isErasableClean(sourceCode: string): boolean {
  // Disallows enum declarations and namespace keywords
  const nonErasableRegex = /\b(enum\s+[A-Za-z0-9_$]+|namespace\s+[A-Za-z0-9_$]+)\b/;
  return !nonErasableRegex.test(sourceCode);
}
```

### Exercise 3 (Hard): Native Node Type-Stripping Runner Script
- **Problem Statement**: Author a Node.js script using `node:child_process` that spawns Node with `--experimental-strip-types` and pipes stdout/stderr seamlessly.
- **Solution**:
```typescript
import { spawn } from 'node:child_process';

export function runNative(filePath: string): Promise<number> {
  return new Promise((resolve, reject) => {
    const child = spawn(
      process.execPath,
      ['--experimental-strip-types', filePath],
      { stdio: 'inherit' }
    );

    child.on('close', (code) => resolve(code ?? 0));
    child.on('error', reject);
  });
}
```
