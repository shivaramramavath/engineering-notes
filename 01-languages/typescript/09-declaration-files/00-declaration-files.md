# Declaration Files (.d.ts)

> Declaration files (`.d.ts`) provide ambient type contracts for JavaScript code, decoupling static type metadata from executable runtime implementations.

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [00-imports-and-exports.md](file:///E:/Learning/engineering-notes/01-languages/typescript/08-modules/00-imports-and-exports.md) |
| **Time to Read** | 24 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 0.9 / TS 5.x / TS 7.0 Native |
| **Tested tsconfig** | `declaration: true`, `emitDeclarationOnly: true` |

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
TypeScript compiles away completely. At runtime, the browser or Node.js executes pure JavaScript without types. When publishing a library to npm, consumers need to know the shapes of functions, classes, and interfaces without having to compile the original TypeScript source code or rely on fragile JSDoc tags.

**Declaration Files (`.d.ts`)** contain only **type metadata**:
- They describe the public interface of JavaScript modules.
- They contain zero executable runtime logic.
- They allow TypeScript projects to consume untyped JavaScript libraries with full type safety.
- They enable fast IDE autocompletion and type-checking across pre-compiled packages.

---

## 2. Core Concepts

### What Belongs in a `.d.ts` File
A declaration file contains ambient declarations introduced by the **`declare`** keyword, interfaces, type aliases, and export signatures:
```typescript
// math-utils.d.ts
export declare const PI: number;
export declare function calculateArea(radius: number): number;
export interface Dimensions {
  width: number;
  height: number;
}
```

### What Cannot Be in a `.d.ts` File
Because declaration files are purely ambient, they cannot contain runtime executable code:
- No function bodies (`function foo() { ... }` is forbidden).
- No variable initializers (`const x = 5;` is forbidden unless marked `declare const x: 5;`).
- No class method implementations.

---

## 3. How It Works Internally

```
Source File (index.ts):                       Emitted Files:
+------------------------------------+        +-------------------------------+
| export function greet(name: string)| =====> | index.js (Runtime logic)      |
| { return "Hello " + name; }        |        +-------------------------------+
+------------------------------------+        | index.d.ts (Type metadata)    |
                                              | declare export function greet |
                                              |   (name: string): string;     |
                                              +-------------------------------+
```

When the compiler emits with `"declaration": true`:
1. The type checker resolves all parameter and return types for exported declarations.
2. The declaration emitter (`src/compiler/declarationEmitter.ts`) strips all executable AST statement blocks (`Block`, function bodies, field initializers).
3. The remaining signatures are transformed into ambient `declare` forms and serialized to disk.

---

## 4. Syntax & API Reference

### Compiler Options for Declarations
```json
{
  "compilerOptions": {
    "declaration": true,
    "declarationMap": true,
    "emitDeclarationOnly": false,
    "declarationDir": "./types"
  }
}
```
- `declarationMap: true`: Emits `.d.ts.map` source maps allowing IDEs ("Go to Definition") to navigate from the `.d.ts` file directly into the original `.ts` source file!
- `emitDeclarationOnly: true`: Emits only `.d.ts` files, skipping `.js` emit (useful when pairing TypeScript with fast JS bundlers like esbuild or Vite).

---

## 5. Practical Examples

### 5.1 Manual Declaration for Untyped CommonJS Module
```typescript
// types/simple-markdown.d.ts
declare module "simple-markdown" {
  export interface ParseOptions {
    sanitize?: boolean;
  }
  export function parse(source: string, options?: ParseOptions): string;
}
```

### 5.2 Real-World Application: Library Packaging
```json
// package.json of published library:
{
  "name": "@acme/logger",
  "version": "1.0.0",
  "main": "./dist/index.js",
  "types": "./dist/index.d.ts",
  "files": [
    "dist"
  ]
}
```

### 5.3 Edge Case: Declaration Maps Navigation
When `declarationMap: true` is enabled:
```
dist/
  index.js
  index.d.ts
  index.d.ts.map  <-- Maps index.d.ts back to src/index.ts
```
Consumers pressing F12 in VS Code jump straight into the readable `src/index.ts` source file instead of being stuck in generated type definitions.

### 5.4 Failure Case: Executable Code in Declaration File
```typescript
// my-types.d.ts
// @ts-expect-error TS1183: An implementation cannot be declared in ambient contexts.
export function add(a: number, b: number): number {
  return a + b;
}
```

### 5.5 Idiomatic Pattern: Declaring Non-Code Assets (CSS, SVG, Images)
```typescript
// globals.d.ts
declare module "*.css" {
  const styles: { readonly [key: string]: string };
  export default styles;
}

declare module "*.svg" {
  const content: string;
  export default content;
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **File Must Not Have Implementation**: Adding a function body or variable assignment in a `.d.ts` file triggers `TS1183`.
2. **Declaration Inaccessibility**: If an exported function signature references a type that is not exported, TypeScript will raise error `TS4023: Exported variable has or is using name from external module but cannot be named`.
3. **Module Specifier Extensions**: In `.d.ts` files targeting modern Node (`node16` / `nodenext`), relative import statements must end with `.js`.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Problem | Solution | Compiler Error |
| :--- | :--- | :--- | :--- |
| Writing function bodies in `.d.ts` file | Not allowed in ambient contexts | Remove implementation; leave signature only | `TS1183: An implementation cannot be declared...` |
| Forgetting `declarationMap: true` | F12 "Go to Definition" lands in opaque `.d.ts` | Enable `declarationMap: true` in tsconfig | Degraded navigation DX |
| Leaking unexported private types | Consumer cannot name the inferred return type | Export all types referenced in public API | `TS4023: Exported variable has or is using name...` |
| Authoring `.d.ts` file with same name as `.ts` | TypeScript refuses to overwrite source files | Let compiler generate `.d.ts` in separate `outDir` | `TS5055: Cannot write file...` |
| Omitting `types` field in `package.json` | Consumers cannot discover the declaration entry | Specify `"types": "./dist/index.d.ts"` | `TS7016: Could not find declaration file...` |
| Hand-authoring `.d.ts` instead of generating | Hand-written types drift out of sync with code | Use `tsc --declaration` to auto-generate | Silent runtime signature drift |

---

## 8. Performance & Type-Checker Complexity
- Type checking against `.d.ts` files is up to **5x faster** than checking against `.ts` source files because the compiler does not need to analyze internal function bodies or control flow graphs.
- Projects with project references (`composite: true`) use declaration files to achieve lightning-fast incremental builds.

---

## 9. Compiler Internals
- `declarationEmitter.ts` traverses the AST using `emitDeclaration`.
- Any node marked `hasModifier(node, ModifierFlags.Export)` is serialized.
- Function nodes have their `body` property substituted with an empty semicolon token.

---

## 10. Best Practices & Design Guidelines
- Always generate `.d.ts` files using `tsc --declaration` rather than hand-crafting them for TypeScript codebases.
- Always generate declaration source maps with `declarationMap: true` for libraries.
- Ensure every type appearing in an exported signature is explicitly exported.

---

## 11. Real-World Case Study
### Fast Monorepo Build Optimization with Project References
An enterprise monorepo with 50 internal packages had 4-minute full build times.

**Optimization:**
1. Enabled `"composite": true` and `"declaration": true` across all packages.
2. In downstream packages, configured `references` pointing to upstream dependencies.
3. When package A changed, downstream package B checked types against `packageA/dist/index.d.ts` without recompiling package A from scratch.

**Result:** Incremental build times dropped from 4 minutes to **11 seconds**.

---

## 12. Interview Questions

<details>
<summary>1. What is the role of a <code>.d.ts</code> declaration file?</summary>

A `.d.ts` file contains static type metadata describing the shapes of JavaScript modules, variables, and functions without containing executable runtime code. It enables compile-time type checking and IDE IntelliSense for published libraries and untyped JavaScript files.
</details>

<details>
<summary>2. What is the difference between <code>declaration: true</code> and <code>emitDeclarationOnly: true</code>?</summary>

`declaration: true` emits both `.js` runtime files and `.d.ts` declaration files. `emitDeclarationOnly: true` emits *only* `.d.ts` files, skipping `.js` generation (useful when bundling runtime JavaScript with tools like esbuild or Vite).
</details>

<details>
<summary>3. What does <code>declarationMap: true</code> do?</summary>

It generates `.d.ts.map` source maps linking generated declaration files back to their original TypeScript source files. When a library consumer uses "Go to Definition" (F12) in an IDE, they are taken directly to the source TypeScript file instead of the generated `.d.ts` stub.
</details>

<details>
<summary>4. What causes error TS4023: "Exported variable has or is using name from external module but cannot be named"?</summary>

It occurs when an exported function or variable's inferred type references an internal interface or type alias that is not exported. The compiler cannot write a valid `.d.ts` signature without an accessible public name for that type.
</details>

<details>
<summary>5. Can a <code>.d.ts</code> file contain variable initializers or function implementations?</summary>

No. Declaration files describe ambient type shapes only. Including function bodies or runtime assignments causes compile error `TS1183`.
</details>

<details>
<summary>6. How do you type non-code imports like <code>.css</code> or <code>.png</code> in TypeScript?</summary>

Using ambient module declarations in a `globals.d.ts` file:
```typescript
declare module "*.css";
declare module "*.png" { const src: string; export default src; }
```
</details>

<details>
<summary>7. Why does type-checking against <code>.d.ts</code> run faster than against <code>.ts</code>?</summary>

Because `.d.ts` files contain only signatures and interface shapes. The compiler completely bypasses function body analysis, expression evaluation, and control flow graph construction.
</details>

<details>
<summary>8. How do you instruct npm packages where to find their declaration file?</summary>

By specifying the `"types"` (or `"typings"`) field in `package.json`, or declaring it inside the `"exports"` block.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Create a CSS Module Declaration (Easy)
Write a declaration file enabling type-safe imports of CSS modules.
<details>
<summary>Solution</summary>

```typescript
// css-modules.d.ts
declare module "*.module.css" {
  const classes: { readonly [key: string]: string };
  export default classes;
}
```
</details>

### Exercise 2: Fix Leaked Type in Export (Easy)
Fix the `TS4023` error in this code:
```typescript
interface SecretState { count: number; }
export function getState(): SecretState { return { count: 0 }; }
```
<details>
<summary>Solution</summary>

```typescript
export interface SecretState { count: number; }
export function getState(): SecretState { return { count: 0 }; }
```
</details>

### Exercise 3: Ambient Lodash Subpath Typing (Medium)
Create an ambient declaration for an untyped subpath `lodash/throttle`.
<details>
<summary>Solution</summary>

```typescript
declare module "lodash/throttle" {
  function throttle<T extends (...args: any[]) => any>(
    fn: T,
    waitMs?: number
  ): T & { cancel(): void };
  export = throttle;
}
```
</details>

### Exercise 4: Emit Declaration Only Setup (Medium)
Configure a `tsconfig.json` optimized for pairing with `esbuild` (esbuild handles `.js`, TypeScript handles `.d.ts`).
<details>
<summary>Solution</summary>

```json
{
  "compilerOptions": {
    "target": "ES2022",
    "module": "NodeNext",
    "declaration": true,
    "declarationMap": true,
    "emitDeclarationOnly": true,
    "outDir": "./dist/types"
  }
}
```
</details>

### Exercise 5: Declarations for Image Assets (Hard)
Write an ambient asset declaration supporting WebP images with responsive attributes (width, height, src).
<details>
<summary>Solution</summary>

```typescript
declare module "*.webp" {
  interface ImageMetadata {
    src: string;
    width: number;
    height: number;
    format: "webp";
  }
  const image: ImageMetadata;
  export default image;
}
```
</details>

### Exercise 6: Programmatic Declaration Extractor (Hard)
Write a Node.js script using the TypeScript Compiler API (`ts.createProgram`) to generate only declaration files for a list of input files.
<details>
<summary>Solution</summary>

```typescript
import ts from "typescript";

function generateDts(fileNames: string[], options: ts.CompilerOptions): void {
  const customOptions: ts.CompilerOptions = {
    ...options,
    declaration: true,
    emitDeclarationOnly: true,
  };
  const program = ts.createProgram(fileNames, customOptions);
  const emitResult = program.emit();
  const diagnostics = ts.getPreEmitDiagnostics(program).concat(emitResult.diagnostics);
  for (const diag of diagnostics) {
    console.error(ts.flattenDiagnosticMessageText(diag.messageText, "
"));
  }
}
```
</details>
