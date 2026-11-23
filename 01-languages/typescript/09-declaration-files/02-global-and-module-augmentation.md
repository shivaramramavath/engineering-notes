# Global and Module Augmentation

> Augmentation techniques in TypeScript extend existing global namespaces, interfaces, and third-party external module contracts without mutating original library source files.

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [01-ambient-declarations.md](file:///E:/Learning/engineering-notes/01-languages/typescript/09-declaration-files/01-ambient-declarations.md), [03-declaration-merging.md](file:///E:/Learning/engineering-notes/01-languages/typescript/09-declaration-files/03-declaration-merging.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 1.8 / TS 5.x / TS 7.0 Native |
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
Modern applications frequently install plugins that attach methods to third-party frameworks:
- Adding a session or user object to Express `Request`.
- Adding custom matchers to Jest or Vitest (`expect().toBeWithinRange()`).
- Attaching custom operators to RxJS Observables.
- Adding properties to `process.env` or `window`.

Because the original library's type definitions are immutable inside `node_modules`, developers cannot edit them directly.

TypeScript provides **Module Augmentation** (`declare module "pkg"`) and **Global Augmentation** (`declare global`) to non-invasively stitch new members into existing third-party contracts.

---

## 2. Core Concepts

### Module Augmentation (`declare module "mod"`)
To augment an imported module, import the module first, then declare its namespace and interfaces:
```typescript
import "express";

declare module "express" {
  interface Request {
    user?: { id: string; role: string };
  }
}
```

### Global Augmentation (`declare global`)
Inside an ES module file, `declare global` opens the ambient global scope to attach global types:
```typescript
declare global {
  interface Array<T> {
    toCustomCsv(): string;
  }
}
```

---

## 3. How It Works Internally

```
Original Module Symbol (express.d.ts):
  Interface Request { params: Params; query: Query; }

Augmentation File (custom-express.d.ts):
  declare module "express" {
    interface Request { user?: User; }
  }

Type Checker Merge:
  Symbol "Request" merges member lists:
  Interface Request {
    params: Params;
    query: Query;
    user?: User; // Seamlessly added!
  }
```

During the binding phase, the compiler groups all interface declarations sharing the same identifier within the same namespace into a single composite symbol. When user code accesses `req.user`, the type checker validates it against the merged symbol.

---

## 4. Syntax & API Reference

### Rules of Augmentation
1. You **cannot create brand new top-level exports** in an augmented module (e.g. you cannot declare a new function export that wasn't originally there). You can only augment existing interfaces or classes.
2. The file containing the augmentation must be treated as a module (it must contain an `import` or `export` statement).

---

## 5. Practical Examples

### 5.1 Augmenting Express Request
```typescript
import "express";

declare module "express-serve-static-core" {
  interface Request {
    authenticatedUserId?: string;
  }
}
```

### 5.2 Real-World Application: Custom Vitest Matchers
```typescript
import "vitest";

interface CustomMatchers<R = unknown> {
  toBeWithinRange(floor: number, ceiling: number): R;
}

declare module "vitest" {
  interface Assertion<T = any> extends CustomMatchers<T> {}
  interface AsymmetricMatchersContaining extends CustomMatchers {}
}
```

### 5.3 Edge Case: Fastify Request Decoration
```typescript
import "fastify";

declare module "fastify" {
  interface FastifyRequest {
    jwtPayload: { sub: string; role: string };
  }
}
```

### 5.4 Failure Case: Augmentation in Non-Module File
```typescript
// If file does NOT contain import or export:
// declare module "axios" { ... }
// Error: Declaring an ambient module in an ambient file replaces the WHOLE module rather than augmenting it!
```

### 5.5 Idiomatic Pattern: Augmenting Node.js ProcessEnv
```typescript
declare global {
  namespace NodeJS {
    interface ProcessEnv {
      PORT?: string;
      DATABASE_URL: string;
      JWT_SECRET: string;
    }
  }
}
export {};
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Ambient Shadowing vs Augmentation**: If a file has NO top-level `import` or `export`, writing `declare module "my-lib"` completely **overwrites** the library instead of augmenting it! Always ensure the file contains at least `import "my-lib"` or `export {}`.
2. **Cannot Augment Default Export**: You cannot change the type of `export default` via module augmentation.
3. **Must Match Internal Export Hierarchy**: Some libraries declare types in internal sub-packages (e.g. Express types live in `express-serve-static-core`). Augmenting `"express"` won't work if the interface was defined in a sub-module.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Problem | Solution | Compiler Error |
| :--- | :--- | :--- | :--- |
| Omitting `import` in augmentation file | Overwrites entire module instead of augmenting | Add `import "pkg"` at top of file | All other original exports vanish |
| Attempting to add new top-level exports | Augmentations cannot introduce new top-level exports | Augment existing interfaces or classes | `TS2695: Left side of comma operator...` |
| Augmenting wrong sub-module | Original interface remains unmodified | Target package where interface was originally defined | Property not found on interface |
| Modifying prototype without runtime method | Types compile, but throws TypeError at runtime | Ensure runtime monkey-patch runs before use | Runtime `x is not a function` |
| Conflicting property types | Merging interface with duplicate incompatible key | Use identical or subtype property type | `TS2717: Subsequent property declaration...` |
| Over-augmenting with loose types (`any`) | Degrades type safety across whole application | Provide strictly typed structures | Loss of autocompletion |

---

## 8. Performance & Type-Checker Complexity
- Declaration merging creates merged symbol entries in the compiler symbol cache.
- Augmentations take negligible memory, but keep augmentations in dedicated `.d.ts` files included in `tsconfig.json` `include`.

---

## 9. Compiler Internals
- `src/compiler/binder.ts` checks `SymbolFlags.Interface` during symbol table insertion.
- If a symbol already exists with `SymbolFlags.Interface`, the binder merges member declarations into `symbol.declarations` rather than reporting a duplicate symbol error.

---

## 10. Best Practices & Design Guidelines
- Put all augmentations in a centralized `src/types/` folder (e.g. `express.d.ts`, `vitest.d.ts`).
- Always verify that the runtime implementation actually attaches the property (e.g. via middleware).
- Never use `any` for augmented fields.

---

## 11. Real-World Case Study
### Typing Multi-Tenant Middleware in Fastify API Server
An engineering team built a SaaS product where every request carried a `tenant: TenantContext` object injected by a middleware hook.

**Solution:**
Created `src/types/fastify-tenant.d.ts`:
```typescript
import "fastify";

export interface TenantContext {
  tenantId: string;
  tier: "free" | "enterprise";
  features: string[];
}

declare module "fastify" {
  interface FastifyRequest {
    tenant: TenantContext;
  }
}
```
Every route handler in the application gained strict, type-checked access to `req.tenant.tier` without type casting.

---

## 12. Interview Questions

<details>
<summary>1. What is Module Augmentation in TypeScript?</summary>

Module augmentation allows extending the types of an existing external module (such as adding properties to interfaces or classes defined in a third-party npm package) from within application code without modifying the source files inside `node_modules`.
</details>

<details>
<summary>2. Why must a module augmentation file include an <code>import</code> or <code>export</code> statement?</summary>

If a file does not have an `import` or `export`, TypeScript treats it as an ambient script. In an ambient script, writing `declare module "foo"` completely shadows and replaces the original module definition. With an import/export, the compiler recognizes it as an augmentation and merges the declarations.
</details>

<details>
<summary>3. Can module augmentation add new top-level functions or variables to a module?</summary>

No. Module augmentation can only augment existing interfaces, classes, and namespaces. It cannot introduce new top-level value exports that were not present in the original module.
</details>

<details>
<summary>4. How does <code>declare global</code> work inside an ES Module?</summary>

`declare global` opens the ambient global scope from within an isolated module file, allowing the file to augment global interfaces like `Window`, `NodeJS.ProcessEnv`, or `Array`.
</details>

<details>
<summary>5. What happens if an augmented interface declares a property with a type that conflicts with an existing property?</summary>

The compiler emits error `TS2717: Subsequent property declaration must have the same type`. Merged interface properties must have identical or strictly assignable types.
</details>

<details>
<summary>6. How do you augment custom matchers for testing libraries like Vitest or Jest?</summary>

By augmenting the library's assertion interface (e.g. `declare module "vitest" { interface Assertion<T> { myMatcher(): void; } }`).
</details>

<details>
<summary>7. Can you augment classes via module augmentation?</summary>

Yes, you can add method signatures to existing classes. However, you must also monkey-patch the class prototype at runtime so the method exists when invoked.
</details>

<details>
<summary>8. Where should module augmentation files be located in a project?</summary>

In a dedicated `types/` or `src/types/` directory that is included in the project's `tsconfig.json` under the `include` array.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Augment ProcessEnv (Easy)
Add `AWS_REGION: string` to the global `NodeJS.ProcessEnv`.
<details>
<summary>Solution</summary>

```typescript
declare global {
  namespace NodeJS {
    interface ProcessEnv {
      AWS_REGION: string;
    }
  }
}
export {};
```
</details>

### Exercise 2: Augment String Prototype (Easy)
Add an ambient `toTitleCase(): string` method to `String`.
<details>
<summary>Solution</summary>

```typescript
declare global {
  interface String {
    toTitleCase(): string;
  }
}
export {};
```
</details>

### Exercise 3: Augment Express Session (Medium)
Augment `express-session` to include `userId: string` and `views: number` on `SessionData`.
<details>
<summary>Solution</summary>

```typescript
import "express-session";

declare module "express-session" {
  interface SessionData {
    userId: string;
    views: number;
  }
}
```
</details>

### Exercise 4: Custom Jest Matcher Augmentation (Medium)
Augment Jest's `Matchers<R>` with `toBeValidUuid(): R`.
<details>
<summary>Solution</summary>

```typescript
export {};

declare global {
  namespace jest {
    interface Matchers<R> {
      toBeValidUuid(): R;
    }
  }
}
```
</details>

### Exercise 5: Augment Fastify Instance (Hard)
Augment Fastify's main `FastifyInstance` to include a custom database client decorator.
<details>
<summary>Solution</summary>

```typescript
import "fastify";

interface DatabaseClient {
  query(sql: string): Promise<any>;
}

declare module "fastify" {
  interface FastifyInstance {
    db: DatabaseClient;
  }
}
```
</details>

### Exercise 6: Complete Runtime Monkey-Patch with Typed Augmentation (Hard)
Write both the typed augmentation and the corresponding runtime prototype implementation for `Array.prototype.last()`.
<details>
<summary>Solution</summary>

```typescript
declare global {
  interface Array<T> {
    last(): T | undefined;
  }
}

// Runtime prototype implementation:
if (!Array.prototype.last) {
  Array.prototype.last = function <T>(this: T[]): T | undefined {
    return this.length > 0 ? this[this.length - 1] : undefined;
  };
}

export {};

// Verified:
const nums = [1, 2, 3];
console.log(nums.last()); // 3
```
</details>
