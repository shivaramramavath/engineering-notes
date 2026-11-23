# Imports and Exports

> ES module import and export declarations in TypeScript govern dependency graph modularization, symbol visibility, re-export pipelines, and compile-time type elision.

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [00-toolchain.md](file:///E:/Learning/engineering-notes/01-languages/typescript/00-setup/00-toolchain.md), [01-primitive-types.md](file:///E:/Learning/engineering-notes/01-languages/typescript/01-fundamentals/01-primitive-types.md) |
| **Time to Read** | 24 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 1.5 / TS 5.x / TS 7.0 Native |
| **Tested tsconfig** | `module: NodeNext`, `verbatimModuleSyntax: true` |

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
JavaScript initially had no native module system. The language relied on global namespaces, script tags, and eventually community standards like CommonJS (Node.js) and AMD. ECMAScript 2015 (ES6) standardized official ES Modules (`import` and `export`).

TypeScript was designed from its inception to support modular architecture, adding a crucial feature: **Type Elision**. Because types do not exist at JavaScript runtime, TypeScript inspects every `import` and `export`. If an imported identifier is only used in type positions (type annotations, interfaces, generics), the TypeScript compiler completely strips the import from the emitted JavaScript output.

---

## 2. Core Concepts

### Named Exports and Imports
Named exports allow exposing multiple explicit bindings per file:
```typescript
// math.ts
export const PI = 3.14159;
export function add(a: number, b: number): number { return a + b; }
export interface CalculationResult { value: number; timestamp: number; }

// app.ts
import { PI, add, type CalculationResult } from "./math.js";
```

### Default Exports
Default exports allow a module to designate a primary export:
```typescript
// logger.ts
export default class Logger {
  log(msg: string): void { console.log(msg); }
}

// consumer.ts
import Logger from "./logger.js";
```

### Re-exporting and Aggregation
Modules can aggregate and re-export members from internal sub-paths:
```typescript
export { UserService } from "./services/user.service.js";
export * as StringUtils from "./utils/string.js";
export type { UserDto } from "./dtos/user.dto.js";
```

---

## 3. How It Works Internally

```
Source Code (TypeScript):
+------------------------------------------+
| import { User, fetchUser } from "./api"; |
| const id: User["id"] = 101;              |
| fetchUser(id);                           |
+------------------------------------------+
                    |
                    | Type Checker Analysis
                    v
  fetchUser: Used as VALUE (runtime) -> KEEP
  User:      Used ONLY as TYPE       -> ELIDE
                    |
                    | Emitter Phase
                    v
JavaScript Output:
+------------------------------------------+
| import { fetchUser } from "./api";       |
| const id = 101;                          |
| fetchUser(id);                           |
+------------------------------------------+
```

During the binding and type checking phases:
1. The compiler tracks every imported symbol in the file's `SourceFile.symbol.exports` and `resolvedModules`.
2. For each imported symbol, the compiler marks its usage with flags (`UsageFlags.Value` or `UsageFlags.Type`).
3. If an import declaration contains *only* symbols marked with `UsageFlags.Type`, the emitter strips the entire `import` statement from the emitted `.js` file, unless `verbatimModuleSyntax` or side-effect import syntax (`import "./side-effects"`) is used.

---

## 4. Syntax & API Reference

### Explicit Type-Only Imports & Exports (TS 3.8+ / 4.5+)
```typescript
// Top-level type-only import
import type { LargeConfig } from "./config.js";

// Inline type-only import (TS 4.5+)
import { executeTask, type TaskContext } from "./tasks.js";

// Type-only export
export type { TaskContext };
```

### Dynamic Imports
```typescript
async function loadAnalytics() {
  const { trackEvent } = await import("./analytics.js");
  trackEvent("init");
}
```

---

## 5. Practical Examples

### 5.1 Basic Named and Aliased Imports
```typescript
import { readFile as fsReadFile, type PathLike } from "fs";

function readConfiguration(path: PathLike): void {
  fsReadFile(path, (err, data) => {
    if (err) throw err;
    console.log("File loaded", data.byteLength);
  });
}
```

### 5.2 Real-World Application: Clean Architecture Service Layer
```typescript
// domain/user.ts
export interface User { id: string; email: string; }
export class UserEntity implements User {
  constructor(public id: string, public email: string) {}
}

// application/user.service.ts
import { UserEntity, type User } from "../domain/user.js";

export class UserService {
  private users: User[] = [];
  register(email: string): User {
    const user = new UserEntity(Math.random().toString(), email);
    this.users.push(user);
    return user;
  }
}
```

### 5.3 Edge Case: Circular Dependency Type Elision
```typescript
// nodeA.ts
import type { NodeB } from "./nodeB.js";
export interface NodeA { neighbor?: NodeB; }

// nodeB.ts
import type { NodeA } from "./nodeA.js";
export interface NodeB { neighbor?: NodeA; }

// Circular type imports compile cleanly because both imports are completely erased!
```

### 5.4 Failure Case: Missing Mandatory File Extension in NodeNext
```typescript
// Under tsconfig "module": "NodeNext":
// @ts-expect-error TS2835: Relative import paths need explicit file extensions in EcmaScript imports when '--moduleResolution' is 'node16' or 'nodenext'. Did you mean './math.js'?
import { add } from "./math";
```

### 5.5 Idiomatic Pattern: Type Re-Export Boundary
```typescript
// index.ts: Single public API surface
export { DataPipeline } from "./pipeline.js";
export type { PipelineConfig, PipelineStage } from "./pipeline.types.js";
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Mandatory `.js` Extensions**: When targeting `NodeNext` or `Node16`, relative imports MUST end with `.js`, even though the source file is `.ts`. The compiler does not append extensions during emit!
2. **Side-Effect Imports**: `import "./polyfill.js"` is never elided because it indicates runtime side effects.
3. **Default Export Renaming**: Default exports can be given arbitrary names at the import site, which can obscure symbol searching in large codebases.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Problem | Solution | Compiler Error |
| :--- | :--- | :--- | :--- |
| Omitting `.js` extension in `NodeNext` | Runtime ES module loader fails to find file | Always import with `./file.js` | `TS2835: Relative import paths need explicit file extensions...` |
| Overusing `export default` | Fragile naming, breaks refactoring tools | Prefer explicit named exports | Inconsistent naming across files |
| Importing class for type without `type` | Compiler may keep import statement alive | Use `import type { MyClass }` | Unwanted runtime bundle bloat |
| Re-exporting whole namespace `export * from ...` | Creates namespace pollution and tree-shaking hazards | Export explicit named tokens | Broken tree-shaking / name clashes |
| Mixing CommonJS `require()` with `import` | Degrades type-checking and creates dual-package hazards | Use modern ES module imports exclusively | `TS1202: Import assignment cannot be used...` |
| Circular runtime imports | Causes runtime `undefined` values before module evaluation | Decouple dependencies or use dependency injection | Runtime `TypeError: Object is not a constructor` |

---

## 8. Performance & Type-Checker Complexity
- Resolving module paths across thousands of files is one of the most expensive compiler operations.
- Using explicit file extensions (`./file.js`) eliminates disk probe heuristics, speeding up compiler resolution.

---

## 9. Compiler Internals
- `ImportDeclaration` (`kind: SyntaxKind.ImportDeclaration`) contains `importClause`.
- In `src/compiler/binder.ts`, the binder creates an alias symbol for each imported identifier.
- During emit (`src/compiler/emitter.ts`), `isReferencedAliasDeclaration` tests whether the alias was referenced in a value context. If not, the import is dropped from the emitted JavaScript.

---

## 10. Best Practices & Design Guidelines
- Use inline type imports (`import { val, type Typ } from "./mod.js"`) for maximum clarity.
- Target `module: NodeNext` for forward compatibility with modern Node.js and runtime bundlers.
- Prefer named exports over default exports for refactorability and autocomplete.

---

## 11. Real-World Case Study
### Tree-Shakeable Enterprise SDK Export Architecture
```typescript
// packages/sdk/src/client.ts
export class ApiClient {
  constructor(private apiKey: string) {}
  async query(endpoint: string): Promise<unknown> { return fetch(endpoint); }
}

// packages/sdk/src/types.ts
export interface SdkOptions { timeoutMs: number; retries: number; }

// packages/sdk/src/index.ts
// Re-export values and types explicitly to optimize consumer bundle sizes:
export { ApiClient } from "./client.js";
export type { SdkOptions } from "./types.js";
```

---

## 12. Interview Questions

<details>
<summary>1. How does TypeScript decide whether to elide an import in emitted JavaScript?</summary>

The compiler inspects every symbol imported by an `ImportDeclaration`. If all imported symbols are used solely in type-space contexts (such as type annotations, interface definitions, or generic type arguments), the entire import declaration is omitted from the emitted JavaScript output.
</details>

<details>
<summary>2. Why must you write <code>import { fn } from "./math.js"</code> when the source file is <code>math.ts</code>?</summary>

In Node.js ECMAScript Modules (`Node16` / `NodeNext`), the runtime module resolver requires exact file extensions. TypeScript follows the principle of not rewriting file paths or extensions during emit; therefore, you write the extension that the emitted runtime JavaScript file will have (`.js`).
</details>

<details>
<summary>3. What is the difference between <code>import type { Foo }</code> and <code>import { type Foo }</code>?</summary>

`import type { Foo }` declares that the entire import statement is type-only. `import { type Foo, bar }` is an inline type import (TS 4.5+), allowing type and value imports to share a single declaration line while explicitly tagging `Foo` as type-only.
</details>

<details>
<summary>4. What is a side-effect import and does TypeScript elide it?</summary>

A side-effect import is of the form `import "./setup.js"`. TypeScript never elides side-effect imports because their sole purpose is to execute module-level code (e.g. polyfills, global registration) at runtime.
</details>

<details>
<summary>5. Why are named exports preferred over default exports in production libraries?</summary>

Named exports enforce consistent naming across all consumer files, integrate seamlessly with IDE refactoring/renaming tools, prevent accidental shadowing, and work better with tree-shaking algorithms in modern bundlers.
</details>

<details>
<summary>6. How does TypeScript handle circular dependencies between types?</summary>

Circular dependencies between type-only imports compile without issue because types are completely erased before runtime JavaScript execution.
</details>

<details>
<summary>7. What does <code>export * as ns from "./mod.js"</code> do?</summary>

It aggregates all exported members of `./mod.js` into a single nested namespace object named `ns` and exports it from the current module.
</details>

<details>
<summary>8. What compiler flag forces explicit type import syntax?</summary>

`verbatimModuleSyntax: true` (introduced in TS 5.0) mandates that any import or export used solely for types must explicitly use the `type` modifier.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Clean Type-Only Import Refactoring (Easy)
Refactor this mixed import so that types are explicitly marked without emitting runtime references.
<details>
<summary>Solution</summary>

```typescript
import { createServer, type ServerOptions, type IncomingMessage } from "http";
```
</details>

### Exercise 2: Re-Export Facade (Easy)
Create a barrel file that exports `User` as a type and `UserManager` as a class from `./user.js`.
<details>
<summary>Solution</summary>

```typescript
export { UserManager, type User } from "./user.js";
```
</details>

### Exercise 3: Dynamic Async Module Loader (Medium)
Write a typed dynamic loader that imports a module on demand and calls an initialization function.
<details>
<summary>Solution</summary>

```typescript
interface PluginModule {
  initPlugin(config: { debug: boolean }): Promise<void>;
}

async function loadAndRunPlugin(modulePath: string): Promise<void> {
  const mod: PluginModule = await import(modulePath);
  await mod.initPlugin({ debug: true });
}
```
</details>

### Exercise 4: Enforce NodeNext Extension Resolution (Medium)
Given a project targeting `module: NodeNext`, correct a broken import structure.
<details>
<summary>Solution</summary>

```typescript
// Before: import { utils } from "./utils"; // TS2835 Error
// After:
import { utils } from "./utils.js";
```
</details>

### Exercise 5: Universal Module Re-Export with Namespace (Hard)
Author an aggregation index that bundles multiple services under distinct namespaces while preserving strict types.
<details>
<summary>Solution</summary>

```typescript
export * as AuthService from "./auth/index.js";
export * as PaymentService from "./payment/index.js";
export * as NotificationService from "./notification/index.js";
```
</details>

### Exercise 6: Verbatim Module Syntax Compliance Checker (Hard)
Audit an import declaration and make it fully compliant with `verbatimModuleSyntax: true`.
<details>
<summary>Solution</summary>

```typescript
// Incompliant under verbatimModuleSyntax:
// import { Connection, ConnectionOptions } from "./db.js"; // where ConnectionOptions is an interface

// Fully compliant:
import { Connection, type ConnectionOptions } from "./db.js";
```
</details>
