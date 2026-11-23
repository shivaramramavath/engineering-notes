# Isolated Declarations

> Introduced in TypeScript 5.5, `--isolatedDeclarations` guarantees that every file can produce its own declaration file (`.d.ts`) in complete isolation, unlocking parallel multi-threaded builds with tools like `tsgo` and Oxlint.

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [00-declaration-files.md](file:///E:/Learning/engineering-notes/01-languages/typescript/09-declaration-files/00-declaration-files.md), [00-imports-and-exports.md](file:///E:/Learning/engineering-notes/01-languages/typescript/08-modules/00-imports-and-exports.md) |
| **Time to Read** | 24 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 5.5 / TS 6.0 / TS 7.0 Native |
| **Tested tsconfig** | `isolatedDeclarations: true`, `declaration: true` |

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
In large enterprise monorepos, single-file transpilers (esbuild, SWC) can transpile `.ts` to `.js` in milliseconds per file by running across multiple CPU cores in parallel.

However, generating type declaration files (**`.d.ts`**) remained a major build bottleneck:
- Generating `.d.ts` files traditionally required running the full, single-threaded TypeScript type checker across the entire project graph.
- If a function lacked an explicit return type, the compiler had to crawl upstream dependencies to infer the return type before writing the `.d.ts` file.

TypeScript 5.5 solved this architectural limitation with **`--isolatedDeclarations`**:
- It enforces that all exported APIs have sufficient explicit type annotations.
- A compiler or fast native tool (like Oxlint, esbuild, or the **TypeScript 7.0 Native Go compiler `tsgo`**) can emit `.d.ts` files for any file **individually and in parallel**, without needing a full type checker!

---

## 2. Core Concepts

### The Isolated Declarations Rule
Under `isolatedDeclarations: true`, every exported entity must be self-describing:
1. Exported functions and methods must have explicit return type annotations.
2. Exported variables must have explicit type annotations if their type cannot be trivially determined from an immediate literal expression.
3. Classes with public properties must explicitly declare their property types.

```typescript
// VIOLATION under isolatedDeclarations:
// export function add(a: number, b: number) { return a + b; }
// Error TS9007: Function must have an explicit return type annotation with --isolatedDeclarations.

// COMPLIANT:
export function add(a: number, b: number): number {
  return a + b;
}
```

### Trivially Inferrable Types
TypeScript allows omitting annotations for expressions whose type is obvious without cross-file inspection:
```typescript
export const APP_NAME = "MyService"; // string literal (allowed)
export const MAX_RETRIES = 5;         // number literal (allowed)
export const IS_PROD = false;        // boolean literal (allowed)
export const REGEX = /test/g;        // RegExp literal (allowed)
```

---

## 3. How It Works Internally

```
Standard tsc .d.ts Generation:
  FileA ---> FileB ---> FileC ===> Single-threaded Type-Checker crawl (Slow)

With isolatedDeclarations:
  FileA ===> Worker 1 (Emit FileA.d.ts instantly without checking imports)
  FileB ===> Worker 2 (Emit FileB.d.ts instantly)
  FileC ===> Worker 3 (Emit FileC.d.ts instantly)
  (10x to 50x faster parallel declaration emit across CPU cores!)
```

Because every exported symbol explicitly states its types, declaration emitters do not need to perform complex inference or resolve external module graphs. They perform simple syntactic transformations from the file's AST.

---

## 4. Syntax & API Reference

### `tsconfig.json` Activation
```json
{
  "compilerOptions": {
    "declaration": true,
    "isolatedDeclarations": true,
    "strict": true
  }
}
```
*Note: `isolatedDeclarations` requires `declaration: true` or `composite: true`.*

---

## 5. Practical Examples

### 5.1 Exported Function Return Types
```typescript
export interface UserRecord {
  id: string;
  name: string;
}

// Compliant: Explicit return type
export function findUser(id: string): UserRecord | null {
  return null;
}
```

### 5.2 Real-World Application: Class Library Export
```typescript
export class DataRepository<T extends { id: string }> {
  // Public property requires explicit type:
  public tableName: string;

  constructor(tableName: string) {
    this.tableName = tableName;
  }

  // Method requires explicit return type:
  public async getById(id: string): Promise<T | null> {
    return null;
  }
}
```

### 5.3 Edge Case: Exporting Function Expressions
```typescript
// Unannotated arrow function:
// export const calculateTax = (amount: number) => amount * 0.2; // TS9007 Error!

// Correct:
export const calculateTax = (amount: number): number => amount * 0.2;
```

### 5.4 Failure Case: Complex Inferred Object Variable
```typescript
// @ts-expect-error TS9011: Variable must have an explicit type annotation with --isolatedDeclarations.
export const settings = Object.freeze({
  timeout: 5000,
  retries: 3,
});

// Fixed:
export const fixedSettings: { readonly timeout: 5000; readonly retries: 3 } = Object.freeze({
  timeout: 5000,
  retries: 3,
});
```

### 5.5 Idiomatic Pattern: Internal Helper Functions
```typescript
// Private (unexported) functions do NOT require return annotations:
function internalHelper(x: number) {
  return x * 2; // Perfectly valid! Not visible outside the file.
}

export function publicApi(n: number): number {
  return internalHelper(n);
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Unexported Types Leaked in Public Signatures**: If an exported function returns a private, unexported interface, isolated declaration generators cannot generate a valid `.d.ts` file without an error. All types in public signatures must be exported.
2. **Object Spread Inferences**: Spread operations (`export const c = { ...a, ...b }`) cannot be trivially inferred without type checking and require explicit annotations.
3. **Internal Helpers are Exempt**: Functions and variables not exported from the module do not need annotations.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Problem | Solution | Compiler Error |
| :--- | :--- | :--- | :--- |
| Omitting return type on exported function | Blocks isolated .d.ts generation | Add explicit return type annotation | `TS9007: Function must have an explicit return type...` |
| Exporting complex call without type | Cannot trivially infer return type | Annotate variable type explicitly | `TS9011: Variable must have an explicit type annotation...` |
| Returning unexported interface | Causes inaccessible type error in .d.ts | Export the interface | `TS9022: Type must be exported with isolatedDeclarations` |
| Enabling `isolatedDeclarations` without `declaration` | Flag has no effect unless declarations are enabled | Add `"declaration": true` to tsconfig | `TS5098: Option 'isolatedDeclarations' requires 'declaration'...` |
| Annotating non-exported internal code | Unnecessary boilerplate where inference works best | Only annotate exported boundaries | Cluttered internal code |
| Using `as const` on complex expressions | May still require explicit type annotation | Provide explicit type contract | `TS9011: Variable must have an explicit type...` |

---

## 8. Performance & Type-Checker Complexity
- Authoring code with `--isolatedDeclarations` allows enterprise build systems to emit declarations up to **20x faster** by parallelizing `.d.ts` generation across all CPU cores.
- Unlocks the full potential of multi-threaded Go-based declaration emitters in **TypeScript 7.0 Native**.

---

## 9. Compiler Internals
- `isolatedDeclarations` activates `src/compiler/checker.ts` diagnostic checks with error codes in the `9000` range (`TS9007`, `TS9011`, `TS9022`).
- When checking AST export nodes, `hasExplicitTypeAnnotation` is validated.
- If missing, the checker issues an immediate diagnostic rather than performing deep cross-file inference.

---

## 10. Best Practices & Design Guidelines
- Enable `isolatedDeclarations: true` in shared libraries and monorepo packages.
- Keep internal private code clean by relying on inference; reserve explicit annotations for public `export` boundaries.
- Ensure all types referenced in exported signatures are exported from the package.

---

## 11. Real-World Case Study
### Scaling Monorepo CI Builds with Isolated Declarations and Tsgo
A large cloud infrastructure monorepo with 4,000 packages spent 18 minutes in CI generating declaration files.

**Implementation:**
1. Activated `"isolatedDeclarations": true` across package `tsconfig.json` files.
2. Added an automated codemod to add missing return types to public exports.
3. Switched declaration generation in CI to a parallelized declaration emitter.

**Results:**
- Declaration generation time plummeted from 18 minutes to **42 seconds**.
- CI feedback loop accelerated by over 80%.

---

## 12. Interview Questions

<details>
<summary>1. What is the primary motivation behind <code>--isolatedDeclarations</code> in TypeScript 5.5+?</summary>

It guarantees that every file has enough explicit type annotations at its export boundaries that any tool can generate its corresponding `.d.ts` declaration file in complete isolation without running a full, single-threaded type-checker across the entire dependency graph.
</details>

<details>
<summary>2. How does <code>isolatedDeclarations</code> differ from <code>isolatedModules</code>?</summary>

`isolatedModules` ensures that JavaScript code (`.js`) can be emitted by single-file transpilers (e.g. esbuild, Babel) without type checking. `isolatedDeclarations` ensures that TypeScript declaration files (`.d.ts`) can also be generated by single-file tools without type checking.
</details>

<details>
<summary>3. Which exported entities require explicit type annotations under <code>isolatedDeclarations</code>?</summary>

Exported functions, methods, getters, setters, and variables whose types cannot be trivially deduced from immediate primitive/literal expressions.
</details>

<details>
<summary>4. Are non-exported internal functions required to have explicit return types under <code>isolatedDeclarations</code>?</summary>

No. Functions and variables that are not exported do not appear in the generated `.d.ts` file, so TypeScript allows them to use normal full type inference.
</details>

<details>
<summary>5. What happens if an exported function returns an unexported interface under <code>isolatedDeclarations</code>?</summary>

TypeScript reports compile error `TS9022`, because an isolated declaration emitter cannot emit a valid public declaration without access to the unexported type definition.
</details>

<details>
<summary>6. How does <code>isolatedDeclarations</code> benefit the upcoming TypeScript 7.0 Native (Go port)?</summary>

It allows the native Go compiler (`tsgo`) to spin up parallel goroutines across all CPU cores to emit declaration files simultaneously, delivering up to 20x to 50x build speed improvements.
</details>

<details>
<summary>7. What trivial expressions are allowed without annotations for exported const variables?</summary>

String, number, boolean, bigint, and regex literals (e.g. `export const X = 100`), as their types are syntactically obvious without type checking.
</details>

<details>
<summary>8. Does <code>isolatedDeclarations</code> require <code>declaration: true</code>?</summary>

Yes, `isolatedDeclarations` enforces declaration emit constraints, so it requires either `"declaration": true` or `"composite": true` in `tsconfig.json`.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Satisfy Isolated Declarations on Function (Easy)
Fix this function so it passes under `--isolatedDeclarations`:
```typescript
export function double(x: number) { return x * 2; }
```
<details>
<summary>Solution</summary>

```typescript
export function double(x: number): number {
  return x * 2;
}
```
</details>

### Exercise 2: Class Property Annotation (Easy)
Ensure this exported class satisfies isolated declaration rules:
```typescript
export class Counter {
  count = 0;
  increment() { this.count++; }
}
```
<details>
<summary>Solution</summary>

```typescript
export class Counter {
  public count: number = 0;
  public increment(): void {
    this.count++;
  }
}
```
</details>

### Exercise 3: Exporting Object Variable (Medium)
Annotate an exported configuration object created via a helper function.
<details>
<summary>Solution</summary>

```typescript
interface Config {
  port: number;
  env: string;
}

export const appConfig: Config = {
  port: 8080,
  env: "production"
};
```
</details>

### Exercise 4: Fix Leaked Unexported Type (Medium)
Resolve the `TS9022` error in this code:
```typescript
interface InternalResult { status: string; }
export function execute(): InternalResult { return { status: "ok" }; }
```
<details>
<summary>Solution</summary>

```typescript
// Export the interface so it is visible in generated .d.ts files:
export interface InternalResult { status: string; }
export function execute(): InternalResult {
  return { status: "ok" };
}
```
</details>

### Exercise 5: Automated Linter Rule for Isolated Declarations (Hard)
Configure ESLint to catch missing return types on exported functions before running the compiler.
<details>
<summary>Solution</summary>

```json
{
  "rules": {
    "@typescript-eslint/explicit-module-boundary-types": "error"
  }
}
```
</details>

### Exercise 6: Parallel Declaration Generator Runner (Hard)
Write a Node.js script using worker threads to generate declaration files for a list of isolated-compliant files concurrently.
<details>
<summary>Solution</summary>

```typescript
import { Worker, isMainThread, parentPort, workerData } from "worker_threads";
import os from "os";

if (isMainThread) {
  const files = ["fileA.ts", "fileB.ts", "fileC.ts"];
  const numWorkers = Math.min(os.cpus().length, files.length);
  for (let i = 0; i < numWorkers; i++) {
    const file = files[i];
    const worker = new Worker(new URL(import.meta.url), { workerData: { file } });
    worker.on("message", (msg) => console.log(`Finished ${msg.file}`));
  }
} else {
  const { file } = workerData;
  // Syntactic .d.ts generation logic here in parallel...
  parentPort?.postMessage({ file, success: true });
}
```
</details>
