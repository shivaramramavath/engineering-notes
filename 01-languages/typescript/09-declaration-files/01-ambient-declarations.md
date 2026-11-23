# Ambient Declarations

> Ambient declarations (`declare`) inform the TypeScript type checker about global variables, functions, and modules that exist in the execution environment without producing emitted runtime code.

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [00-declaration-files.md](file:///E:/Learning/engineering-notes/01-languages/typescript/09-declaration-files/00-declaration-files.md) |
| **Time to Read** | 22 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 0.9 / TS 5.x / TS 7.0 Native |
| **Tested tsconfig** | `strict: true`, `noImplicitAny: true` |

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
JavaScript runtimes provide built-in globals:
- Browsers provide `window`, `document`, `fetch`, `localStorage`.
- Node.js provides `process`, `Buffer`, `__dirname`.
- HTML scripts often inject global CDN libraries: `window.Stripe`, `window.dataLayer`.

When TypeScript compiles code that references these globals, it requires type definitions to verify signatures. An **Ambient Declaration** uses the **`declare`** keyword to say to the compiler:
> "Trust me: an identifier with this name and shape exists in the runtime environment. Verify that my code uses it correctly, but do not emit any JavaScript for this declaration."

---

## 2. Core Concepts

### `declare var`, `declare let`, `declare const`
Declares ambient global variables:
```typescript
declare const __DEV__: boolean;
declare const API_BASE_URL: string;

if (__DEV__) {
  console.log("Connecting to:", API_BASE_URL);
}
```

### `declare function`
Declares ambient callable functions:
```typescript
declare function gtag(command: string, ...args: any[]): void;
gtag("event", "page_view");
```

### `declare class`
Declares ambient constructors:
```typescript
declare class AnalyticsTracker {
  constructor(apiKey: string);
  track(event: string): void;
}

const tracker = new AnalyticsTracker("key_123");
tracker.track("click");
```

---

## 3. How It Works Internally

```
Source:
declare const Stripe: (publishableKey: string) => StripeInstance;
const stripe = Stripe("pk_live_123");

Type-Checker Phase:
  Finds 'Stripe' symbol in global scope.
  Verifies argument "pk_live_123" matches string.
  Sets type of 'stripe' to StripeInstance.

Emitter Phase:
  'declare const Stripe' is completely stripped!
  Emitted JS:
  const stripe = Stripe("pk_live_123");
```

When the binder encounters a statement with modifier `SyntaxKind.DeclareKeyword`:
1. It registers the identifier in the target symbol table (either global scope or module scope).
2. It sets `SymbolFlags.Ambient` on the symbol.
3. The emitter completely ignores ambient symbols, ensuring zero overhead in output bundles.

---

## 4. Syntax & API Reference

### Ambient Namespaces and Modules
```typescript
// Ambient namespace
declare namespace WebGL {
  interface ContextOptions { alpha: boolean; }
}

// Ambient module
declare module "external-library" {
  export function runJob(): void;
}
```

---

## 5. Practical Examples

### 5.1 Declaring Global Window Extensions
```typescript
declare global {
  interface Window {
    __REDUX_DEVTOOLS_EXTENSION__?: () => any;
  }
}

if (typeof window !== "undefined" && window.__REDUX_DEVTOOLS_EXTENSION__) {
  console.log("DevTools active");
}
```

### 5.2 Real-World Application: Bundler Build-Time Feature Flags
```typescript
// env.d.ts
declare const __BUILD_TIMESTAMP__: string;
declare const __APP_VERSION__: string;
declare const __ENABLE_PROFILING__: boolean;

export function getBuildInfo() {
  return {
    version: __APP_VERSION__,
    builtAt: __BUILD_TIMESTAMP__,
    profiling: __ENABLE_PROFILING__,
  };
}
```

### 5.3 Edge Case: Ambient `const enum`
```typescript
// Ambient const enums are inlined at compile time:
declare const enum KeyCodes {
  ENTER = 13,
  ESCAPE = 27,
}

const pressed = KeyCodes.ENTER; // Emits: const pressed = 13;
```

### 5.4 Failure Case: Initializing Ambient Variable
```typescript
// @ts-expect-error TS1039: Initializers are not allowed in ambient contexts.
declare const maxConnections: number = 100;
```

### 5.5 Idiomatic Pattern: Ambient ProcessEnv Typing
```typescript
declare namespace NodeJS {
  interface ProcessEnv {
    NODE_ENV: "development" | "production" | "test";
    PORT: string;
    DATABASE_URL: string;
  }
}

const port = parseInt(process.env.PORT, 10);
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Initializers Forbidden**: You cannot assign values in `declare const x = 10;` (unless it is a literal type in `declare const x: 10`).
2. **Runtime Missing Identifiers**: If you declare `declare const secret: string` but fail to define it in the runtime environment (via Webpack `DefinePlugin` or a script tag), code will crash at runtime with `ReferenceError: secret is not defined`.
3. **Top-Level Imports Turn File into a Module**: As soon as a file contains a top-level `import` or `export`, it becomes an ES Module. Any `declare const` inside it is scoped to that file rather than global scope! Use `declare global { ... }` to declare globals within a module.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Problem | Solution | Compiler Error |
| :--- | :--- | :--- | :--- |
| Initializing ambient variable | Ambient declarations cannot contain values | Remove initializer: `declare const x: number;` | `TS1039: Initializers are not allowed in ambient contexts` |
| Forgetting to inject runtime value | Type passes compile check, crashes at runtime | Ensure bundler/script defines the variable | Runtime `ReferenceError: X is not defined` |
| Declaring globals in module without `declare global` | Variable is scoped locally to module file | Wrap in `declare global { ... }` | Identifier not visible across files |
| Overusing `declare var` with `any` | Degrades global type safety | Provide explicit structural interfaces | Loss of type checking |
| Declaring duplicate conflicting globals | Name clash with standard library | Use unique prefixes or interface merging | `TS2403: Subsequent variable declaration...` |
| Using ambient `const enum` with Babel/esbuild | Single-file transpilers cannot inline ambient enums | Use regular string literals or const objects | Runtime ReferenceError |

---

## 8. Performance & Type-Checker Complexity
- Ambient declarations add symbols directly to the global or module scope.
- Excessive numbers of global ambient declarations (e.g. hundreds of global types) slightly slow down global scope lookup; prefer module-scoped imports where possible.

---

## 9. Compiler Internals
- Parsed with `parseDeclaration` in `src/compiler/parser.ts`.
- Evaluated as `ModifierFlags.Ambient`.
- In `src/compiler/emitter.ts`, the emitter explicitly skips any AST node where `node.flags & NodeFlags.Ambient` is truthy.

---

## 10. Best Practices & Design Guidelines
- Wrap global augmentations inside `declare global { ... }` in dedicated `.d.ts` files.
- Document the runtime origin of every ambient variable in comments (e.g. "Injected by Vite DefinePlugin").
- Type environment variables strictly via `NodeJS.ProcessEnv` augmentation.

---

## 11. Real-World Case Study
### Typing Third-Party Analytics SDK Injected via Tag Manager
An enterprise frontend loaded Google Tag Manager and Segment via HTML script tags. Developers needed type-safe access to `window.analytics`.

**Solution:**
Created `src/types/analytics.d.ts`:
```typescript
interface AnalyticsSDK {
  track(event: string, properties?: Record<string, unknown>): void;
  identify(userId: string, traits?: Record<string, unknown>): void;
}

declare global {
  interface Window {
    analytics: AnalyticsSDK;
  }
}

export {}; // Ensure file is an isolated module
```
All React components safely called `window.analytics.track("purchase", { total: 99.99 })` with full autocompletion and compile-time verification.

---

## 12. Interview Questions

<details>
<summary>1. What does the <code>declare</code> keyword do in TypeScript?</summary>

The `declare` keyword introduces ambient declarations. It tells the compiler that a variable, function, class, or module exists in the runtime environment at execution time, allowing type checking against it without emitting any JavaScript code.
</details>

<details>
<summary>2. Why are initializers not allowed in ambient declarations?</summary>

Because ambient declarations are purely type metadata and emit zero JavaScript code. Initializing a variable requires emitting an assignment statement, which contradicts the ambient nature of `declare`.
</details>

<details>
<summary>3. How does adding an <code>import</code> statement affect ambient declarations in a file?</summary>

Adding a top-level `import` or `export` converts the file into an ES Module. Any `declare const` in the file becomes local to that module rather than global. To declare global variables inside a module, you must wrap them in `declare global { ... }`.
</details>

<details>
<summary>4. What is the difference between <code>declare const x: number</code> and <code>const x: number = 5</code>?</summary>

`const x: number = 5` is an executable declaration that emits `const x = 5;` into the JavaScript output. `declare const x: number` produces zero emitted JavaScript; it merely asserts that a global `x` exists at runtime.
</details>

<details>
<summary>5. How can you augment the global <code>NodeJS.ProcessEnv</code> interface?</summary>

```typescript
declare namespace NodeJS {
  interface ProcessEnv {
    API_SECRET: string;
  }
}
```
</details>

<details>
<summary>6. What happens if you declare an ambient variable that is not actually present at runtime?</summary>

TypeScript compiles cleanly, but when the code executes in the runtime environment, accessing the variable throws a runtime `ReferenceError: [variable] is not defined`.
</details>

<details>
<summary>7. What is an ambient module declaration?</summary>

A declaration of the form `declare module "module-name" { ... }` that describes the exported types of an untyped external JavaScript library.
</details>

<details>
<summary>8. Can you declare ambient classes?</summary>

Yes, `declare class MyClass { constructor(x: number); method(): void; }` describes a class constructor and its instance methods without implementing them.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Declare Build Timestamp (Easy)
Declare an ambient global constant `__VERSION__` of type `string`.
<details>
<summary>Solution</summary>

```typescript
declare const __VERSION__: string;
console.log(__VERSION__);
```
</details>

### Exercise 2: Global Window Property (Easy)
Add a custom property `appReady: boolean` to the global `Window` interface.
<details>
<summary>Solution</summary>

```typescript
declare global {
  interface Window {
    appReady: boolean;
  }
}
export {};
```
</details>

### Exercise 3: Ambient Function with Overloads (Medium)
Declare an ambient function `queryDatabase` supporting query strings or query config objects.
<details>
<summary>Solution</summary>

```typescript
interface QueryConfig { sql: string; timeout: number; }
declare function queryDatabase(sql: string): Promise<any[]>;
declare function queryDatabase(config: QueryConfig): Promise<any[]>;
```
</details>

### Exercise 4: Untyped NPM Package Declaration (Medium)
Declare types for an untyped package `fast-uuid` that exports `function v4(): string`.
<details>
<summary>Solution</summary>

```typescript
declare module "fast-uuid" {
  export function v4(): string;
}
```
</details>

### Exercise 5: Ambient WebSocket Global (Hard)
Declare an ambient WebSocket factory with strict event handler types.
<details>
<summary>Solution</summary>

```typescript
declare class SecureSocket {
  constructor(url: string);
  on(event: "message", handler: (data: string) => void): void;
  on(event: "close", handler: (code: number) => void): void;
  send(payload: string): void;
}
```
</details>

### Exercise 6: Ambient Environment Validator (Hard)
Write a TypeScript utility function that safely accesses an ambient variable and throws an informative error if it was not injected by the bundler.
<details>
<summary>Solution</summary>

```typescript
declare const __APP_CONFIG__: { apiUrl: string } | undefined;

function getAppConfig(): { apiUrl: string } {
  if (typeof __APP_CONFIG__ === "undefined") {
    throw new Error("__APP_CONFIG__ was not defined by the bundler. Check build configuration.");
  }
  return __APP_CONFIG__;
}
```
</details>
