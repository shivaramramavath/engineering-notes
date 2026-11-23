# Senior & Staff Interview Guide: Type System Internals & Runtime Execution

> In-depth technical interview guide on TypeScript runtime behavior: type erasure, Node.js native type stripping, V8 hidden class optimization, and JavaScript emit downleveling.

| Metadata | Details |
| :--- | :--- |
| **Category** | Technical Interview Preparation |
| **Target Level** | Senior / Staff Software Engineer |
| **Prerequisites** | [14-compiler-internals/00-type-erasure-and-runtime.md](../14-compiler-internals/00-type-erasure-and-runtime.md) |
| **TypeScript Baseline** | TS 5.0+, Node.js 20/22 LTS |
| **Tested tsconfig** | `"strict": true, "target": "ES2022", "module": "NodeNext"` |

---

## Table of Contents
1. [Core Architectural Concepts](#1-core-architectural-concepts)
2. [Deep-Dive Interview Questions & Answers](#2-deep-dive-interview-questions--answers)
3. [Output Prediction & Code Analysis Questions](#3-output-prediction--code-analysis-questions)
4. [Architecture & System Design Scenario](#4-architecture--system-design-scenario)
5. [Senior Technical Evaluation Rubric](#5-senior-technical-evaluation-rubric)

---

## 1. Core Architectural Concepts

Key interview focus areas:
- **Type Erasure Mechanics**: What code is emitted vs erased during compilation.
- **Node.js 22 Native Type Stripping**: The `--experimental-strip-types` runtime flag and its syntax restrictions.
- **V8 Hidden Classes (Shapes)**: How TypeScript type structures impact monomorphic and megamorphic call sites.
- **Ambient Declarations vs Real Code**: How `.d.ts` interacts with runtime module loaders.

---

## 2. Deep-Dive Interview Questions & Answers

<details>
<summary>Q1: Does TypeScript generate any runtime overhead, and how does Node.js 22's native type stripping work?</summary>

**Answer**:
TypeScript types themselves are completely erased at compile time, creating **zero runtime overhead**. 
However, historical TypeScript features emit real runtime code:
- `enum` (emits IIFE object mappings)
- `namespace` (emits IIFE closures)
- Class parameter properties (`constructor(public x: number)`)

In **Node.js 22 LTS**, the `--experimental-strip-types` flag allows Node to execute `.ts` files directly without an external build step (using an internal Rust-based lexer). Because it only **erases** type annotations without transforming syntax, features that generate runtime JavaScript (such as `enum`, `namespace`, and parameter properties) are **strictly forbidden** under native type stripping.
</details>

<details>
<summary>Q2: How does the V8 engine optimize property accesses on TypeScript objects, and how can careless typing de-optimize it?</summary>

**Answer**:
V8 uses **Hidden Classes (Shapes)** and **Inline Caches (ICs)**. When objects share the exact same properties initialized in the identical order, they share a single hidden class. Property reads compile down to a direct assembly memory offset (`[rax + 16]`).

Careless typing causes de-optimizations:
1. **Dynamic Optional Initialization**:
   ```typescript
   const obj: { a: number; b?: number } = { a: 1 };
   if (condition) obj.b = 2; // Causes shape transition!
   ```
2. **The `delete` Operator**:
   Using `delete obj.prop` drops the object out of the hidden class model into slow dictionary mode (hash table).
3. **Monomorphic to Megamorphic**:
   Passing objects with 5+ different shapes to a hot function causes V8 to abandon fast inline caches in favor of generic hash table lookups.
</details>

<details>
<summary>Q3: What is the difference between `ts.Program`, `ts.SourceFile`, and `ts.TypeChecker` in the TypeScript compiler architecture?</summary>

**Answer**:
- `ts.SourceFile`: The AST representation of an individual file produced by the Scanner and Parser.
- `ts.Program`: The complete compilation context containing all source files, referenced libraries, and compiler options in a dependency graph.
- `ts.TypeChecker`: The core semantic analysis engine. It resolves symbols, calculates assignability, checks diagnostics, and synthesizes types across the entire program.
</details>

<details>
<summary>Q4: What is the "Dual Package Hazard" when publishing TypeScript packages for Node.js?</summary>

**Answer**:
It occurs when a published package supports both CommonJS (`require`) and ESM (`import`), and a consumer inadvertently loads both versions into memory simultaneously. Because Node.js creates two independent module execution contexts, package-level singletons or `instanceof` checks across the two instances will fail.
</details>

---

## 3. Output Prediction & Code Analysis Questions

### Problem: Enum Runtime Output
```typescript
enum Direction {
  Up = "UP",
  Down = "DOWN",
}

const dir = Direction.Up;
console.log(typeof Direction);
```
**Evaluation & Explanation**:
- **Output**: `"object"`
- **Explanation**: TypeScript enums emit runtime JavaScript objects. Unlike types or interfaces which disappear, `Direction` compiles to an IIFE that attaches keys to a global object:
  ```javascript
  var Direction;
  (function (Direction) {
      Direction["Up"] = "UP";
      Direction["Down"] = "DOWN";
  })(Direction || (Direction = {}));
  ```
</details>

---

## 4. Architecture & System Design Scenario

**Scenario**: Your engineering organization is standardizing on Node.js 22 with `--experimental-strip-types` across all microservices to eliminate build steps in development. A legacy shared library uses enums and parameter properties.

**Senior Solution**:
1. Replace all `enum`s with `as const` object dictionaries:
   ```typescript
   export const STATUS = { ACTIVE: "ACTIVE", PENDING: "PENDING" } as const;
   export type Status = (typeof STATUS)[keyof typeof STATUS];
   ```
2. Refactor class constructors to assign properties explicitly:
   ```typescript
   export class User {
     public name: string;
     constructor(name: string) { this.name = name; }
   }
   ```
3. Add `@typescript-eslint/no-extraneous-class` and ban `enum` via ESLint.

---

## 5. Senior Technical Evaluation Rubric

| Level | Performance Characteristics |
| :--- | :--- |
| **Junior / Mid** | Thinks TypeScript code runs directly in the browser; unaware of hidden class transitions. |
| **Senior** | Explains type erasure, V8 inline caches, and CJS/ESM interop differences. |
| **Staff / Principal** | Diagnoses megamorphic call sites via V8 trace flags; designs zero-emission architectures for Node.js 22 native type stripping. |
