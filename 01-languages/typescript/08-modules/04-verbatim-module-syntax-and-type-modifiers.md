# Verbatim Module Syntax and Type Modifiers

> Introduced in TypeScript 5.0, `verbatimModuleSyntax` establishes unambiguous, 1:1 parity between source code imports and emitted JavaScript statements, preventing runtime crashes under Node.js native type stripping.

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [00-imports-and-exports.md](file:///E:/Learning/engineering-notes/01-languages/typescript/08-modules/00-imports-and-exports.md), [01-es-modules-and-commonjs.md](file:///E:/Learning/engineering-notes/01-languages/typescript/08-modules/01-es-modules-and-commonjs.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 5.0 / TS 5.x / TS 7.0 Native |
| **Tested tsconfig** | `verbatimModuleSyntax: true`, `module: NodeNext` |

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
Historically, TypeScript used complex heuristics to decide whether an `import` should be emitted to JavaScript:
- If imported symbols were only used as types, the entire import was erased.
- If even one symbol was used as a value, the import was emitted.
- If a class was imported, was it used as a type annotation or instantiated with `new`?

These heuristics caused serious issues:
1. **Side effects dropped unexpectedly**: An import written solely to execute module-level setup could be accidentally elided if TypeScript thought its exported type was unused.
2. **Runtime errors with single-file transpilers**: Tools like esbuild, Babel, SWC, and **Node.js 22 native type stripping** (`node --experimental-strip-types`) strip types without type-checking! If an untagged import like `import { SomeType } from "./mod.js"` is left in the file, Node.js attempts to load `SomeType` at runtime, throwing `SyntaxError: The requested module does not provide an export named 'SomeType'`.

TypeScript 5.0 unified and deprecated older flags (`importsNotUsedAsValues`, `preserveValueImports`) under one single authoritative flag: **`verbatimModuleSyntax: true`**.

---

## 2. Core Concepts

### The Verbatim Rule
Under `verbatimModuleSyntax: true`:
- Any import without a `type` modifier is **always emitted verbatim** into the output JavaScript.
- Any import with a top-level `type` modifier (`import type { ... }`) is **completely erased**.
- Inline type modifiers (`import { val, type Typ }`) emit the import statement containing only the value symbols.

```typescript
// Source:
import { Car, type Engine } from "./vehicles.js";
import type { Driver } from "./drivers.js";
import "./telemetry.js";

// Emitted JavaScript (100% predictable):
import { Car } from "./vehicles.js";
import "./telemetry.js";
// Driver import is completely removed!
```

---

## 3. How It Works Internally

```
Source Import                       Compiler Rule                       JavaScript Emit
+-------------------------------+   +-------------------------------+   +-----------------------------+
| import { A } from "./m.js"    |   | No 'type' keyword present     |   | import { A } from "./m.js"; |
+-------------------------------+   +-------------------------------+   +-----------------------------+
                                    | (Emitted EVEN IF A is a type!)|
                                    | Under verbatimModuleSyntax,   |
                                    | compiler throws TS1484!       |
                                    +-------------------------------+

+-------------------------------+   +-------------------------------+   +-----------------------------+
| import type { B } from "./m"  |   | Top-level 'type' keyword      |   | (Completely Erased)         |
+-------------------------------+   +-------------------------------+   +-----------------------------+

+-------------------------------+   +-------------------------------+   +-----------------------------+
| import { C, type D } from "./m|   | Inline 'type' on D            |   | import { C } from "./m.js"; |
+-------------------------------+   +-------------------------------+   +-----------------------------+
```

When `verbatimModuleSyntax` is active:
1. The compiler checks whether an import or export lacks the `type` keyword.
2. If it is only used as a type, the compiler issues error **`TS1484: 'X' is a type and must be imported using a type-only import`**.
3. This forces the developer to annotate types accurately, guaranteeing that transpilers running type stripping will never emit nonexistent runtime bindings.

---

## 4. Syntax & API Reference

### Compiler Configuration
```json
{
  "compilerOptions": {
    "module": "NodeNext",
    "moduleResolution": "NodeNext",
    "verbatimModuleSyntax": true
  }
}
```

### Exporting Types Under Verbatim Rules
```typescript
export { Component } from "./Component.js";
export type { ComponentProps } from "./Component.types.js";
```

---

## 5. Practical Examples

### 5.1 Basic Inline Type Import
```typescript
import { createServer, type IncomingMessage, type ServerResponse } from "http";

const server = createServer((req: IncomingMessage, res: ServerResponse) => {
  res.end("ok");
});
```

### 5.2 Real-World Application: Node 22 Type-Stripping Compatible Microservice
```typescript
import express, { type Request, type Response } from "express";
import { UserService } from "./services/user.service.js";
import type { UserDto } from "./dtos/user.dto.js";

const app = express();
const userService = new UserService();

app.post("/users", (req: Request, res: Response) => {
  const user: UserDto = req.body;
  const created = userService.create(user);
  res.status(201).json(created);
});
```

### 5.3 Edge Case: Side-Effect Imports
```typescript
// If you want a module's side effects to run, write a side-effect import:
import "./instrumentation.js";

// If you want types only, write:
import type { TracingConfig } from "./instrumentation.js";
```

### 5.4 Failure Case: Untagged Type Import Under Verbatim Rules
```typescript
interface UserProfile {
  name: string;
}

// In another file:
// @ts-expect-error TS1484: 'UserProfile' is a type and must be imported using a type-only import when 'verbatimModuleSyntax' is enabled.
import { UserProfile } from "./user.js";
```

### 5.5 Idiomatic Pattern: Type Re-Export Facade
```typescript
export { DatabasePool } from "./pool.js";
export type { PoolOptions, QueryResult } from "./pool.types.js";
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Node.js 22 Type Stripping Compatibility**: Node 22 natively runs `.ts` files by converting type annotations to whitespace. If you import a type without `type`, Node.js tries to find that export in JavaScript and crashes! `verbatimModuleSyntax` guarantees this never happens.
2. **Enums and Namespaces**: Enums generate runtime objects. If an enum is imported without `type`, it is preserved. If imported with `type`, it can only be used as a type annotation, not as a runtime value object.
3. **Replaces `importsNotUsedAsValues` and `preserveValueImports`**: Both older flags are deprecated in TS 5.0+ in favor of `verbatimModuleSyntax`.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Problem | Solution | Compiler Error |
| :--- | :--- | :--- | :--- |
| Omitting `type` on interface imports | Transpilers emit the import; fails at runtime | Use inline `type` or `import type` | `TS1484: 'X' is a type and must be imported...` |
| Using deprecated `importsNotUsedAsValues` | Deprecated in TS 5.0; inconsistent behavior | Use `verbatimModuleSyntax: true` | Compiler deprecation warning |
| Importing class with `import type` then calling `new` | Type-only import cannot be instantiated at runtime | Use regular value import: `import { Cls }` | `TS1361: 'Cls' cannot be used as a value...` |
| Relying on import elision to suppress side effects | Transpiler will NOT elide value import | Don't import modules whose side effects you don't want | Unintended side effects executed |
| Forgetting `export type` on re-exports | Bundler retains module reference in bundle | Write `export type { TypeName }` | Larger production bundle |
| Mixing CJS `module.exports` with `verbatimModuleSyntax` | Verbatim syntax enforces standard ESM rules | Use ES modules or explicit CJS interop | Syntax incompatibility |

---

## 8. Performance & Type-Checker Complexity
- `verbatimModuleSyntax` accelerates compilation because the emitter does not need to cross-reference usage flags across the AST to decide if an import should be dropped.
- Single-file bundlers (esbuild, Vite, SWC) can transpile files instantly with 100% confidence.

---

## 9. Compiler Internals
- In `src/compiler/checker.ts`, `checkImportBinding` inspects if `compilerOptions.verbatimModuleSyntax` is true.
- If true, any imported identifier whose symbol has only `SymbolFlags.Type` (and lacks `SymbolFlags.Value`) is checked for the `type` token modifier. If missing, diagnostic `TS1484` is reported.

---

## 10. Best Practices & Design Guidelines
- Enable `verbatimModuleSyntax: true` in all new TypeScript projects.
- Use inline type imports (`import { val, type Typ }`) to keep related imports grouped cleanly on a single line.
- Use `import "./file.js"` whenever you intend to trigger side effects.

---

## 11. Real-World Case Study
### Safe Native TypeScript Execution in Node.js 22 LTS
A financial tech backend team adopted Node.js 22 with native type stripping (`node --experimental-strip-types`) to eliminate build steps in development.

**Issue:** Several developers wrote `import { TransactionPayload } from "./models.js"`. When Node.js ran the file, it threw `SyntaxError: The requested module './models.js' does not provide an export named 'TransactionPayload'`.

**Solution:**
Enabled `"verbatimModuleSyntax": true` in `tsconfig.json`. The compiler instantly flagged all 47 missing `type` modifiers during CI type-checking, preventing runtime breakage.

---

## 12. Interview Questions

<details>
<summary>1. What does <code>verbatimModuleSyntax</code> do in TypeScript 5.0+?</summary>

It enforces that the emitted JavaScript module syntax matches the TypeScript source exactly: any import or export written without the `type` modifier is emitted as a real JavaScript import/export, and anything marked with `type` is discarded. If an untagged import is only a type, the compiler issues error TS1484.
</details>

<details>
<summary>2. Why is <code>verbatimModuleSyntax</code> crucial for Node.js 22 native type stripping?</summary>

Node.js native type stripping erases type annotations using fast syntactic whitespace replacement without running a full TypeScript type checker. If a type is imported without the `type` keyword, Node.js retains the import at runtime, causing a `SyntaxError` when the export cannot be found in the JavaScript file.
</details>

<details>
<summary>3. What older compiler options did <code>verbatimModuleSyntax</code> replace and deprecate?</summary>

It replaced `importsNotUsedAsValues` and `preserveValueImports`.
</details>

<details>
<summary>4. What happens if you import a class using <code>import type</code>?</summary>

The class can only be used in type-space contexts (such as parameter annotations or variable types). Attempting to instantiate it with `new` or access its static properties triggers compile error `TS1361`.
</details>

<details>
<summary>5. How does <code>verbatimModuleSyntax</code> treat side-effect imports like <code>import "./polyfill.js"</code>?</summary>

They are emitted verbatim into the generated JavaScript, exactly as written.
</details>

<details>
<summary>6. How do inline type imports work under <code>verbatimModuleSyntax</code>?</summary>

Syntax like `import { valueA, type TypeB } from "./module.js"` emits `import { valueA } from "./module.js"`, stripping `TypeB` while preserving `valueA`.
</details>

<details>
<summary>7. What error does TypeScript emit when an interface is imported without <code>type</code> under this flag?</summary>

`TS1484: 'X' is a type and must be imported using a type-only import when 'verbatimModuleSyntax' is enabled.`
</details>

<details>
<summary>8. Can you use <code>export type * from "./mod.js"</code>?</summary>

Yes, starting in TypeScript 4.5, type-only namespace re-exports are fully supported.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Fix Verbatim Import Errors (Easy)
Fix the following import under `verbatimModuleSyntax: true`:
```typescript
import { Logger, LogLevel } from "./logger.js"; // LogLevel is a type alias
```
<details>
<summary>Solution</summary>

```typescript
import { Logger, type LogLevel } from "./logger.js";
```
</details>

### Exercise 2: Type-Only Export Conversion (Easy)
Ensure this barrel file complies with verbatim rules:
```typescript
export { User, UserSchema } from "./user.js"; // User is interface, UserSchema is Zod schema
```
<details>
<summary>Solution</summary>

```typescript
export { UserSchema, type User } from "./user.js";
```
</details>

### Exercise 3: Side-Effect Distinction (Medium)
Refactor a module that imports CSS and types from a design system so the CSS side-effect is guaranteed to run while types are cleanly elided.
<details>
<summary>Solution</summary>

```typescript
import "./theme.css";
import type { ThemeConfig } from "./theme.js";
```
</details>

### Exercise 4: Enforcing Verbatim Compliance via Linter (Medium)
Configure an ESLint rule (`@typescript-eslint/consistent-type-imports`) to automatically enforce verbatim type import syntax.
<details>
<summary>Solution</summary>

```json
{
  "rules": {
    "@typescript-eslint/consistent-type-imports": [
      "error",
      { "prefer": "type-imports", "fixStyle": "inline-type-imports" }
    ]
  }
}
```
</details>

### Exercise 5: Universal Node 22 Type-Stripping Runner Script (Hard)
Write a bash/PowerShell invocation demonstrating compiling and executing a TypeScript project under Node 22 native type stripping with verbatim compliance.
<details>
<summary>Solution</summary>

```bash
# 1. Type-check without emit (verifies verbatim syntax)
npx tsc --noEmit

# 2. Run directly with Node.js 22 experimental type stripping
node --experimental-strip-types src/index.ts
```
</details>

### Exercise 6: Verbatim AST Emitter Simulator (Hard)
Write a TypeScript function that inspects an import declaration AST and determines whether it should be kept, stripped, or filtered under verbatim rules.
<details>
<summary>Solution</summary>

```typescript
interface ImportSpecifierInfo { name: string; isTypeOnly: boolean; }
interface ImportDeclarationInfo { isTypeOnly: boolean; specifiers: ImportSpecifierInfo[]; modulePath: string; }

function emitVerbatim(decl: ImportDeclarationInfo): string | null {
  if (decl.isTypeOnly) return null;
  const runtimeSpecifiers = decl.specifiers.filter((s) => !s.isTypeOnly);
  if (decl.specifiers.length > 0 && runtimeSpecifiers.length === 0) return null;
  if (runtimeSpecifiers.length === 0) return `import "${decl.modulePath}";`;
  const names = runtimeSpecifiers.map((s) => s.name).join(", ");
  return `import { ${names} } from "${decl.modulePath}";`;
}
```
</details>
