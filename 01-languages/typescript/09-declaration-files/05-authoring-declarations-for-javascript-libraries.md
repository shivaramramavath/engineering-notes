# Authoring Declarations for JavaScript Libraries

> Step-by-step methodology for reverse-engineering, structuring, and authoring production-grade `.d.ts` declaration files for untyped JavaScript npm packages and UMD global bundles.

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [00-declaration-files.md](file:///E:/Learning/engineering-notes/01-languages/typescript/09-declaration-files/00-declaration-files.md), [01-ambient-declarations.md](file:///E:/Learning/engineering-notes/01-languages/typescript/09-declaration-files/01-ambient-declarations.md) |
| **Time to Read** | 26 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 2.0 / TS 5.x / TS 7.0 Native |
| **Tested tsconfig** | `strict: true`, `module: NodeNext` |

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
When integrating legacy JavaScript packages into enterprise TypeScript codebases, you will frequently encounter modules with no existing type definitions on npm or DefinitelyTyped.

Writing a high-quality `.d.ts` file requires:
- Analyzing the JavaScript runtime export patterns (CommonJS, ES Module, UMD, or global namespace).
- Accurately typing function overloads, optional configurations, callbacks, and events.
- Creating reusable interfaces so downstream consumers can type their data models.

This guide provides the authoritative blueprint for authoring, packaging, and testing declaration files for untyped JavaScript libraries.

---

## 2. Core Concepts

### Identifying the Library Architecture
Before writing a single line of type definitions, inspect how the JavaScript library exports its functionality:
1. **ES Module**: Uses `export const`, `export default`.
2. **CommonJS Single Entity**: Uses `module.exports = function(...) {}`.
3. **CommonJS Object**: Uses `module.exports = { a, b, c }`.
4. **UMD (Universal Module Definition)**: Works as both an npm import and a `<script>` global tag.

---

## 3. How It Works Internally

```
JavaScript Implementation (lib.js):
module.exports = function createClient(options) {
  return {
    connect: function() { ... },
    disconnect: function() { ... }
  };
};

Authoring Declaration File (lib.d.ts):
+----------------------------------------------------+
| export interface ClientOptions { timeout?: number;}|
| export interface Client {                          |
|   connect(): Promise<void>;                        |
|   disconnect(): Promise<void>;                     |
| }                                                  |
| declare function createClient(                     |
|   options?: ClientOptions                          |
| ): Client;                                         |
| export default createClient;                       |
+----------------------------------------------------+
```

The declaration file mirrors the runtime JavaScript shape. When user code compiles against `lib.js`, TypeScript loads `lib.d.ts` instead, substituting real static contracts for runtime objects.

---

## 4. Syntax & API Reference

### Declarations for CommonJS `module.exports = fn`
```typescript
declare function parseMarkdown(src: string): string;
declare namespace parseMarkdown {
  export const version: string;
  export interface Options { sanitize?: boolean; }
}
export = parseMarkdown;
```

### Declarations for UMD Libraries (`export as namespace`)
```typescript
export as namespace MyAnalytics;

export function trackEvent(name: string): void;
export const apiKey: string;
```
*The `export as namespace` directive allows the module to be imported via `import { trackEvent } from "lib"` OR referenced globally as `MyAnalytics.trackEvent("click")`!*

---

## 5. Practical Examples

### 5.1 Authoring a Declaration for a Builder Library
```typescript
// my-builder.d.ts
export interface QueryOptions {
  limit?: number;
  offset?: number;
}

export class QueryBuilder<T> {
  where(condition: Record<string, unknown>): this;
  limit(count: number): this;
  execute(): Promise<T[]>;
}

export function createQuery<T>(table: string): QueryBuilder<T>;
```

### 5.2 Real-World Application: Authoring Type Definitions for an HTTP Client
```typescript
// http-client.d.ts
export interface RequestConfig {
  headers?: Record<string, string>;
  timeoutMs?: number;
}

export interface HttpResponse<T = unknown> {
  status: number;
  data: T;
  headers: Record<string, string>;
}

export class HttpClient {
  constructor(baseUrl: string, defaultConfig?: RequestConfig);
  get<T = unknown>(path: string, config?: RequestConfig): Promise<HttpResponse<T>>;
  post<T = unknown>(path: string, body: unknown, config?: RequestConfig): Promise<HttpResponse<T>>;
}

export default HttpClient;
```

### 5.3 Edge Case: Typing Event Emitter Callback Signatures
```typescript
export interface StreamEvents {
  data: (chunk: Buffer) => void;
  error: (err: Error) => void;
  end: () => void;
}

export class DataStream {
  on<K extends keyof StreamEvents>(event: K, listener: StreamEvents[K]): this;
  emit<K extends keyof StreamEvents>(event: K, ...args: Parameters<StreamEvents[K]>): boolean;
}
```

### 5.4 Failure Case: Mismatched Export Style
```typescript
// When JS uses: module.exports = fn;
// WRONG: export default fn;
// Causes runtime error: Cannot call fn because it is imported as an object!
// CORRECT: export = fn;
```

### 5.5 Idiomatic Pattern: Ambient Module Wire-up
```typescript
// In local tsconfig:
// "include": ["src/**/*", "types/**/*.d.ts"]
// types/untyped-lib.d.ts
declare module "untyped-lib" {
  export function compute(val: number): number;
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **`export =` vs `export default`**: If a CommonJS library assigns directly to `module.exports = fn`, use `export = fn`. Using `export default fn` will cause runtime breakage under strict ESM interop!
2. **Missing Function Overloads**: If a JavaScript function accepts different parameters based on the first argument's type, declare all overloads in the `.d.ts` file.
3. **Circular Reference Types**: Ensure declaration files don't import the package they are typing.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Problem | Solution | Compiler Error |
| :--- | :--- | :--- | :--- |
| Using `export default` for `module.exports = fn` | Fails when imported with `require()` or CJS interop | Use `export = fn;` | Incompatible import shape |
| Using `any` for complex config arguments | Defeats type checking for library consumers | Create explicit configuration interfaces | Silent runtime misconfiguration |
| Omitting generic parameters on responses | Forces consumers to cast responses manually | Use `<T = unknown>` generic parameter | Excess `as Type` casting |
| Forgetting to include `.d.ts` in `files` or `include` | Compiler cannot locate authored declaration | Add file path to `tsconfig.json` `include` | `TS7016: Could not find declaration file` |
| Writing function implementations in `.d.ts` | Not allowed in ambient declaration files | Provide signatures only (end with `;`) | `TS1183: An implementation cannot be declared...` |
| Over-constraining parameter types | Rejects valid runtime JavaScript inputs | Inspect actual JS implementation for flexible inputs | Type mismatch on valid JS calls |

---

## 8. Performance & Type-Checker Complexity
- Keep declaration files clean: avoid deep, unnecessary recursive types inside public library declaration signatures.
- Use explicit interface names for return types rather than complex inlined mapped types.

---

## 9. Compiler Internals
- `src/compiler/declarationEmitter.ts` serializes declarations.
- When loading ambient declarations, the binder resolves `declare module "name"` into the global ambient module cache.

---

## 10. Best Practices & Design Guidelines
- Inspect the library's actual JavaScript runtime source code and test suite to verify supported argument types.
- Always write type tests (using `tsd` or `@ts-expect-error`) to verify the declaration file against real call patterns.
- Export all constituent interfaces so consumers can reference them in their code.

---

## 11. Real-World Case Study
### Typing Legacy Payment Gateway SDK
A fintech team needed to use an untyped payment gateway SDK `pay-gate-v2`.

**Implementation:**
Created `src/types/pay-gate.d.ts`:
```typescript
declare module "pay-gate-v2" {
  export interface ChargeRequest {
    amountCents: number;
    currency: "USD" | "EUR" | "GBP";
    token: string;
  }

  export interface ChargeResult {
    chargeId: string;
    success: boolean;
    errorCode?: string;
  }

  export class GatewayClient {
    constructor(secretKey: string);
    createCharge(req: ChargeRequest): Promise<ChargeResult>;
  }
}
```
The application implemented payments with 100% type safety and zero runtime compilation errors.

---

## 12. Interview Questions

<details>
<summary>1. How do you decide whether to use <code>export =</code> or <code>export default</code> when authoring a declaration file?</summary>

Inspect the library's JavaScript source. If it exports a single function or class via `module.exports = MyEntity`, use `export = MyEntity` to accurately represent the CommonJS contract. If it uses ES module syntax (`export default`) or exports an object with a `default` property, use `export default`.
</details>

<details>
<summary>2. What is the purpose of <code>export as namespace</code> in a declaration file?</summary>

It creates a UMD (Universal Module Definition) declaration, allowing the module to be consumed via standard ES module imports (`import { x } from "pkg"`) as well as accessed via global variable syntax (`pkg.x`) when loaded in a browser via a script tag.
</details>

<details>
<summary>3. How do you test that an authored declaration file works correctly?</summary>

Using type-testing tools like `tsd` or `vitest` (`expectTypeOf`), or writing a dedicated `test.ts` file that exercises valid and invalid API calls with `// @ts-expect-error`.
</details>

<details>
<summary>4. What should you do if an untyped library accepts both callbacks and Promises?</summary>

Use function overloads: declare one signature that accepts a callback returning `void`, and another signature without the callback returning a `Promise<T>`.
</details>

<details>
<summary>5. Why should all sub-interfaces used in function signatures be exported?</summary>

If an interface like `Options` is not exported, consumers cannot author separate helper functions that prepare or accept those options without relying on complex indexed access types (`Parameters<typeof fn>[0]`).
</details>

<details>
<summary>6. How can you declare a function that can also be called as a constructor?</summary>

Using a combined interface with call and construct signatures:
```typescript
interface ComplexCallable {
  (arg: string): void;
  new (arg: string): object;
}
```
</details>

<details>
<summary>7. What is the difference between an ambient module declaration and a standalone <code>.d.ts</code> file?</summary>

A standalone `.d.ts` file contains direct top-level `export` statements and describes its own file path. An ambient module declaration uses `declare module "name" { ... }` and can be placed in any `.d.ts` file to describe an external package.
</details>

<details>
<summary>8. How do you model optional and default options in a declaration file?</summary>

Mark optional properties with `?` on the options interface: `{ timeout?: number; retries?: number; }`.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Declare Simple Markdown Parser (Easy)
Write a declaration for `simple-md` which exports `parse(src: string): string`.
<details>
<summary>Solution</summary>

```typescript
declare module "simple-md" {
  export function parse(src: string): string;
}
```
</details>

### Exercise 2: CommonJS Single Entity Function (Easy)
Write a declaration for `slugify` where `module.exports = function(str: string): string`.
<details>
<summary>Solution</summary>

```typescript
declare function slugify(str: string): string;
export = slugify;
```
</details>

### Exercise 3: Function with Overloaded Callback / Promise (Medium)
Declare a function `fetchData` that returns a Promise if no callback is supplied, or invokes a callback if passed.
<details>
<summary>Solution</summary>

```typescript
declare module "data-fetcher" {
  export function fetchData(url: string): Promise<string>;
  export function fetchData(url: string, callback: (err: Error | null, data: string) => void): void;
}
```
</details>

### Exercise 4: UMD Library Declaration (Medium)
Declare a UMD library `mathlib` accessible both globally as `MathLib` and as an imported module.
<details>
<summary>Solution</summary>

```typescript
export as namespace MathLib;

export function add(a: number, b: number): number;
export const version: string;
```
</details>

### Exercise 5: Typing Middleware Function with Next (Hard)
Declare a typed Express-compatible middleware package `rate-limiter`.
<details>
<summary>Solution</summary>

```typescript
declare module "express-simple-limiter" {
  import type { Request, Response, NextFunction } from "express";

  export interface LimiterOptions {
    maxRequests: number;
    windowSeconds: number;
  }

  export function createLimiter(
    options: LimiterOptions
  ): (req: Request, res: Response, next: NextFunction) => void;
}
```
</details>

### Exercise 6: Complete Ambient Plugin Registry (Hard)
Author declarations for a plugin system where plugins can register hooks and trigger them with arbitrary payload shapes.
<details>
<summary>Solution</summary>

```typescript
declare module "plugin-engine" {
  export type HookHandler<T = any> = (payload: T) => void | Promise<void>;

  export class PluginHost {
    registerHook<T>(hookName: string, handler: HookHandler<T>): void;
    triggerHook<T>(hookName: string, payload: T): Promise<void>;
  }

  export const defaultHost: PluginHost;
}
```
</details>
