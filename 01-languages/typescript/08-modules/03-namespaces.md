# Namespaces and Internal Modules

> TypeScript namespaces provide legacy code encapsulation and declaration grouping, functioning as compile-time object wrappers with distinct scoping, ambient merging, and type-elision rules.

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [00-imports-and-exports.md](file:///E:/Learning/engineering-notes/01-languages/typescript/08-modules/00-imports-and-exports.md), [03-declaration-merging.md](file:///E:/Learning/engineering-notes/01-languages/typescript/09-declaration-files/03-declaration-merging.md) |
| **Time to Read** | 22 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 0.9 / TS 5.x / TS 7.0 Native |
| **Tested tsconfig** | `strict: true`, `isolatedModules: false` |

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
Before the ECMAScript 2015 module standard (`import` / `export`), JavaScript developers structured large client applications by attaching objects to the global window namespace (e.g. `window.MyApp = window.MyApp || {}`).

TypeScript modeled this pattern using **Namespaces** (originally called "Internal Modules" using the `module` keyword). Namespaces compile into JavaScript Immediately Invoked Function Expressions (IIFEs) that attach properties to a shared object.

In modern application code, **ES Modules are the universal standard**. However, namespaces remain essential in TypeScript for:
- Writing ambient declaration files (`.d.ts`) for legacy JavaScript libraries.
- Declaration merging (attaching utility types or static methods to functions, enums, or classes).

---

## 2. Core Concepts

### Declaring a Namespace
A namespace groups types and values under a single identifier:
```typescript
namespace Validation {
  const emailRegex = /^[^s@]+@[^s@]+.[^s@]+$/; // Private to namespace

  export function isValidEmail(email: string): boolean {
    return emailRegex.test(email);
  }

  export interface ValidationResult {
    valid: boolean;
    error?: string;
  }
}

// Accessing exported members:
const isValid = Validation.isValidEmail("ada@example.com");
type Result = Validation.ValidationResult;
```

### Emitted JavaScript
The namespace compiles into an IIFE:
```javascript
var Validation;
(function (Validation) {
    var emailRegex = /^[^s@]+@[^s@]+.[^s@]+$/;
    function isValidEmail(email) {
        return emailRegex.test(email);
    }
    Validation.isValidEmail = isValidEmail;
})(Validation || (Validation = {}));
```

---

## 3. How It Works Internally

```
Namespace Binding:
+---------------------------------------+
|  Namespace Identifier: Validation    |
+---------------------------------------+
| SymbolFlags.NamespaceModule           |
| SymbolFlags.Value (IIFE Closure)      |
+---------------------------------------+
     |                       |
     | exports               | members
     v                       v
isValidEmail (Function)    emailRegex (Private var)
ValidationResult (Interface)
```

When a namespace contains only types (interfaces, type aliases), the compiler elides the entire IIFE from the emitted JavaScript! If it contains executable functions, classes, or const/let variables, the IIFE is emitted to preserve runtime values.

---

## 4. Syntax & API Reference

### Multi-File Namespaces (Using Triple-Slash References)
Historically, namespaces could span multiple files linked by reference tags:
```typescript
// StringValidation.ts
/// <reference path="Validation.ts" />
namespace Validation {
  export function isString(val: any): boolean { return typeof val === "string"; }
}
```

### Namespace Merging with Functions or Enums
```typescript
function buildUrl(path: string): string {
  return buildUrl.baseUrl + path;
}

namespace buildUrl {
  export let baseUrl = "https://api.example.com";
}

buildUrl("/users"); // "https://api.example.com/users"
```

---

## 5. Practical Examples

### 5.1 Basic Namespace Grouping
```typescript
namespace Geometry {
  export class Point {
    constructor(public x: number, public y: number) {}
  }
  export function distance(p1: Point, p2: Point): number {
    return Math.hypot(p1.x - p2.x, p1.y - p2.y);
  }
}

const p1 = new Geometry.Point(0, 0);
const p2 = new Geometry.Point(3, 4);
console.log(Geometry.distance(p1, p2)); // 5
```

### 5.2 Real-World Application: Merging Static Types on Functions
```typescript
function fetchUser(id: string): Promise<fetchUser.Response> {
  return Promise.resolve({ id, name: "Alice" });
}

namespace fetchUser {
  export interface Response {
    id: string;
    name: string;
  }
  export const cache = new Map<string, Response>();
}

type Res = fetchUser.Response;
```

### 5.3 Edge Case: Ambient Namespaces in Declaration Files
```typescript
// globals.d.ts
declare namespace NodeJS {
  interface ProcessEnv {
    DATABASE_URL: string;
    PORT: string;
  }
}
```

### 5.4 Failure Case: Namespaces Under `--isolatedModules`
```typescript
// Under tsconfig "isolatedModules": true
// In modern bundlers (esbuild, babel, swc):
namespace Legacy {
  // @ts-expect-error TS1208: Cannot compile namespaces when '--isolatedModules' is enabled.
  export const val = 1;
}
```

### 5.5 Idiomatic Pattern: Namespace Aliasing with `import`
```typescript
namespace Deeply.Nested.Long.Namespace {
  export const value = 42;
}

// Create a local shortcut alias:
import Short = Deeply.Nested.Long.Namespace;
console.log(Short.value); // 42
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Incompatible with Modern Bundlers**: Babel, esbuild, and SWC process one file at a time without full type graphs. Multi-file namespaces or namespaces with `const enum` often fail to compile correctly under these single-file transpilers.
2. **Type Elision of Empty Namespaces**: If a namespace only exports interfaces, no runtime JS object is generated. Calling `Object.keys(MyNamespace)` at runtime will throw `ReferenceError: MyNamespace is not defined`.
3. **Global Scope Pollution**: Namespaces not enclosed within an ES module contaminate the global window/global scope.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Problem | Solution | Compiler Error |
| :--- | :--- | :--- | :--- |
| Using namespaces for new modular application code | Obsolete; fails with modern bundlers & ESM | Use standard ES Modules (`import` / `export`) | Maintenance friction |
| Using namespaces under `isolatedModules` | Single-file compilers cannot resolve cross-file namespaces | Use ES modules | `TS1208: Cannot compile namespaces...` |
| Attempting runtime reflection on type-only namespace | Namespace was completely elided by compiler | Provide at least one runtime value or use object | Runtime `ReferenceError: X is not defined` |
| Mixing `module` keyword with `namespace` | `module` keyword is legacy syntax | Use `namespace` keyword | Obsolete syntax warning |
| Over-nesting namespaces (`A.B.C.D`) | High cognitive overhead and verbose calls | Flatten structure or use import aliases | Deeply nested identifiers |
| Forgetting `export` on internal members | Member remains private to namespace IIFE | Prefix member with `export` | `TS2339: Property 'x' does not exist on...` |

---

## 8. Performance & Type-Checker Complexity
- Namespaces require merging multiple declarations into unified symbol tables.
- Cross-file namespaces linked via `/// <reference>` disable incremental caching optimizations in modern builds.

---

## 9. Compiler Internals
- `ModuleDeclaration` (`kind: SyntaxKind.ModuleDeclaration`) represents namespaces.
- The binder flags the symbol with `SymbolFlags.ValueModule` if it contains values, or `SymbolFlags.NamespaceModule` if type-only.
- The emitter translates `ValueModule` symbols into self-executing IIFE wrapper calls.

---

## 10. Best Practices & Design Guidelines
- **Do not use namespaces for application code**: Use modern ES modules (`import` / `export`).
- **Use namespaces for**:
  - Global namespace augmentation in `.d.ts` files (e.g. extending `NodeJS.ProcessEnv` or `Express.Request`).
  - Declaration merging to group auxiliary types onto a function or class.

---

## 11. Real-World Case Study
### Augmenting Express Request with Session Data
```typescript
// types/express.d.ts
import "express";

declare global {
  namespace Express {
    interface Request {
      user?: { id: string; role: string };
      sessionId?: string;
    }
  }
}

// In application code:
import express from "express";
const app = express();
app.use((req, res, next) => {
  req.sessionId = "sess_123"; // Strictly typed and recognized!
  next();
});
```

---

## 12. Interview Questions

<details>
<summary>1. What is the difference between a TypeScript namespace and an ES module?</summary>

An ES module is an official ECMAScript file-level isolation mechanism where symbols are private unless exported and linked via `import` / `export`. A namespace is a TypeScript-specific construct that compiles into a JavaScript IIFE attaching properties to a global or scoped object.
</details>

<details>
<summary>2. Why are namespaces discouraged in modern TypeScript application code?</summary>

Namespaces do not integrate cleanly with modern ECMAScript standard tooling (Babel, esbuild, SWC, Vite), break tree-shaking, cannot support asynchronous dynamic loading natively, and complicate dependency graphs compared to standard ES modules.
</details>

<details>
<summary>3. Where are namespaces still valid and commonly used today?</summary>

In declaration files (`.d.ts`) for ambient typing of third-party JavaScript libraries, global augmentation (e.g. `declare global { namespace NodeJS { ... } }`), and declaration merging with functions or classes.
</details>

<details>
<summary>4. What happens to a namespace at runtime if it contains only TypeScript interfaces?</summary>

It is completely elided (erased) from the emitted JavaScript output, producing zero runtime code.
</details>

<details>
<summary>5. How does declaration merging work with namespaces and functions?</summary>

A function and a namespace sharing the same identifier merge their symbols: the function provides the callable value, while the namespace attaches static properties and sub-types onto that function object.
</details>

<details>
<summary>6. What does <code>import Q = A.B.C</code> do inside a namespace?</summary>

It creates an internal alias for a nested namespace or member, acting like a compile-time and runtime shortcut pointer.
</details>

<details>
<summary>7. What compiler option flags issues with namespaces in modern toolchains?</summary>

`isolatedModules: true` flags namespaces that cannot be safely transpiled by single-file transpilers like esbuild or Babel.
</details>

<details>
<summary>8. What was the original keyword used for namespaces in TypeScript 1.0?</summary>

The `module` keyword was used (e.g. `module MyModule { ... }`). TypeScript 1.5 introduced the `namespace` keyword to disambiguate internal namespaces from ECMAScript official modules.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Group Types with a Namespace (Easy)
Create a namespace `MathUtils` that exports an `add` function and a `Point` interface.
<details>
<summary>Solution</summary>

```typescript
namespace MathUtils {
  export interface Point { x: number; y: number; }
  export function add(a: number, b: number): number { return a + b; }
}

const p: MathUtils.Point = { x: 1, y: 2 };
const sum = MathUtils.add(p.x, p.y);
```
</details>

### Exercise 2: Function Namespace Merging (Easy)
Merge a function `greet` with a namespace providing its default options.
<details>
<summary>Solution</summary>

```typescript
function greet(name: string): string {
  return `${greet.salutation}, ${name}!`;
}

namespace greet {
  export let salutation = "Hello";
}

console.log(greet("Alice")); // "Hello, Alice!"
```
</details>

### Exercise 3: Global Environment Augmentation (Medium)
Augment the global `NodeJS.ProcessEnv` to include `API_KEY: string`.
<details>
<summary>Solution</summary>

```typescript
declare global {
  namespace NodeJS {
    interface ProcessEnv {
      API_KEY: string;
    }
  }
}
export {}; // Ensure file is treated as a module
```
</details>

### Exercise 4: Namespace Alias (Medium)
Create a nested namespace hierarchy and alias its leaf node for clean usage.
<details>
<summary>Solution</summary>

```typescript
namespace App.Security.Encryption {
  export function hash(data: string): string { return `hash_${data}`; }
}

import Encrypt = App.Security.Encryption;
const h = Encrypt.hash("secret");
```
</details>

### Exercise 5: Enum-Namespace Merging (Hard)
Merge an enum with a namespace providing helper methods for that enum.
<details>
<summary>Solution</summary>

```typescript
enum Priority {
  LOW = 1,
  MEDIUM = 2,
  HIGH = 3,
}

namespace Priority {
  export function isUrgent(p: Priority): boolean {
    return p === Priority.HIGH;
  }
}

const p = Priority.HIGH;
console.log(Priority.isUrgent(p)); // true
```
</details>

### Exercise 6: Convert Legacy Namespace to ES Module (Hard)
Refactor a multi-property namespace with private variables into a clean ES Module.
<details>
<summary>Solution</summary>

```typescript
// Before:
// namespace Cache {
//   let store = new Map();
//   export function get(k) { return store.get(k); }
// }

// After (ESM):
const store = new Map<string, unknown>();
export function get(key: string): unknown {
  return store.get(key);
}
export function set(key: string, value: unknown): void {
  store.set(key, value);
}
```
</details>
