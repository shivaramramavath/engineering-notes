# Type-Checking Performance in TypeScript: Profiling, Union Costs, and Trace Analysis

> Advanced engineering guide to analyzing and optimizing TypeScript type-checker performance: `--generateTrace`, `@typescript/analyze-trace`, type instantiation limits, union explosion, and interface caching vs type aliases.

| Metadata | Details |
| :--- | :--- |
| **Category** | Performance |
| **Difficulty** | Advanced |
| **Prerequisites** | [14-compiler-internals/00-compiler-architecture.md](../14-compiler-internals/00-compiler-architecture.md), [13-compiler-options/00-tsconfig-json.md](../13-compiler-options/00-tsconfig-json.md) |
| **TypeScript Baseline** | TS 5.0+, Node.js 20/22 LTS |
| **Tested tsconfig** | `"strict": true, "target": "ES2022", "module": "NodeNext"` |

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

In enterprise TypeScript repositories, slow compilation is rarely caused by disk I/O or JavaScript emission; it is almost always caused by **type-checker computational complexity**. 
Symptoms include:
- Visual Studio Code Language Server freezing or taking 10+ seconds to show autocomplete suggestions.
- `tsc --noEmit` taking minutes on CI or crashing with `Fatal error: JavaScript heap out of memory`.
- Infinite recursion warnings: `Type instantiation is excessively deep and possibly infinite (TS2589)`.

The TypeScript type system is Turing-complete. Complex types—deeply nested generics, large discriminated unions, and recursive mapped types—can trigger exponential ($O(2^N)$ or $O(N \times M)$) type checking overhead. Understanding how the type checker instantiates and caches types allows developers to eliminate compiler bottlenecks without sacrificing type safety.

---

## 2. Core Concepts

### Trace Generation (`--generateTrace`)
TypeScript provides built-in compiler profiling via `--generateTrace <dir>`. It writes Chrome DevTools-compatible performance traces (`trace.json`) and type identification files (`types.json`), recording exact millisecond durations for type checking every symbol, file, and function.

### The Cost of Large Unions vs. Interfaces
- **Interfaces**: Cached by unique identity (`ts.Symbol`). When comparing two interfaces, TypeScript checks identity first. If identical, the comparison is $O(1)$.
- **Type Aliases & Intersections**: Anonymous intersections (`A & B`) cannot be cached by identity. The compiler must synthesize and flatten new compound types on every invocation.
- **Large Unions**: Comparing a union of $M$ members against a target of $N$ members requires checking $M \times N$ assignability relationships unless discriminated by a common tag property.

```
+-------------------------------------------------------------------------+
|                  Type-Checker Performance Evaluation                    |
+-------------------------------------------------------------------------+
|                                                                         |
|  [ Type Comparison: Interface vs. Union/Intersection ]                  |
|                                                                         |
|  Interface A === Interface B ?                                          |
|  +-- Symbol match -> OK (O(1) instant return)                           |
|                                                                         |
|  Intersection (TypeA & TypeB) ?                                         |
|  +-- Synthesizes new anonymous object type                              |
|  +-- Checks all property conflicts (O(P * Q) complexity)                |
|                                                                         |
|  Union (U1 | U2 | ... | U_50) assignable to (V1 | V2 | ... | V_50) ?    |
|  +-- Without discriminant: 50 x 50 = 2,500 structural evaluations!      |
|  +-- With discriminant tag: O(1) hash map lookup                        |
|                                                                         |
+-------------------------------------------------------------------------+
```

---

## 3. How It Works Internally

1. **Trace Emission**: When `--generateTrace` is active, the compiler wraps key phases in performance measurement blocks:
   - `checkSourceFile`
   - `checkVariableDeclaration`
   - `isTypeAssignableTo`
2. **Type ID Allocation**: Every synthesized type receives a sequential integer `id`.
3. **`@typescript/analyze-trace`**: Reads `trace.json` and `types.json`, finding operations where duration exceeds thresholds (e.g., >500ms) and highlighting "hot spot" source code locations.

---

## 4. Syntax & API Reference

### Profiling Commands
```bash
# 1. Generate performance traces
$ tsc --noEmit --generateTrace ./perf-trace

# 2. Analyze hot spots using the official analyzer
$ npx @typescript/analyze-trace ./perf-trace
```

### Measuring tsconfig Performance Options
```json
{
  "compilerOptions": {
    "skipLibCheck": true,
    "strict": true,
    "exactOptionalPropertyTypes": true
  }
}
```

---

## 5. Practical Examples

### Example 1: Catastrophic Union Explosion vs. Tagged Discriminated Union
```typescript
// SLOW: Undiscriminated wide union
type UnsafeEvent =
  | { kind?: string; payload: string; timestamp: number }
  | { kind?: string; payload: number; timestamp: number }
  | { kind?: string; payload: boolean; metadata: Record<string, string> };

// FAST: Discriminated union with guaranteed literal tags
type OptimizedEvent =
  | { readonly type: "TEXT"; readonly payload: string }
  | { readonly type: "NUMBER"; readonly payload: number }
  | { readonly type: "BOOL"; readonly payload: boolean };

export function processEvent(ev: OptimizedEvent): string {
  // Fast O(1) branch elimination by discriminant
  switch (ev.type) {
    case "TEXT": return ev.payload;
    case "NUMBER": return String(ev.payload);
    case "BOOL": return ev.payload ? "true" : "false";
  }
}
```

### Example 2: Interface Inheritance vs Anonymous Intersections
```typescript
// POOR PERFORMANCE: Repeated anonymous intersections
type BaseProps = { id: string; className: string };
type ClickProps = { onClick: () => void };
type CombinedProps = BaseProps & ClickProps; // Synthesizes new type on every reference

// HIGH PERFORMANCE: Named Interface extends
// Interfaces create a cached Symbol in the compiler's type table
interface FastBaseProps {
  id: string;
  className: string;
}

interface FastCombinedProps extends FastBaseProps {
  onClick: () => void;
}
```

### Example 3: Edge Case - Recursive Tuple Flattening Depth Limit
```typescript
// Unconstrained recursive type causing TS2589
// type InfiniteFlatten<T> = T extends [infer Head, ...infer Tail]
//   ? [...InfiniteFlatten<Head>, ...InfiniteFlatten<Tail>]
//   : [T];

// Safe pattern with recursion depth accumulator:
type SafeFlatten<T, Depth extends unknown[] = []> =
  Depth["length"] extends 20
    ? T
    : T extends [infer Head, ...infer Tail]
      ? [...SafeFlatten<Head, [...Depth, unknown]>, ...SafeFlatten<Tail, Depth>]
      : [T];
```

### Example 4: Compiler Error & Instantiation Overflow
```typescript
type Loop<T> = { next: Loop<T> };

// Attempting deep identity check without termination condition:
// @ts-expect-error - TS2589: Type instantiation is excessively deep and possibly infinite.
type BadRecursive = Loop<Loop<Loop<Loop<Loop<Loop<string>>>>>>;
```

### Example 5: Explicit Return Type Optimization
```typescript
// SLOW: Omitting return types on complex functions forces the compiler
// to infer the return type by recursively evaluating all expressions
export function slowCompute(x: number, y: number) {
  return {
    sum: x + y,
    product: x * y,
    items: Array.from({ length: x }, (_, i) => ({ index: i, val: y }))
  };
}

// FAST: Explicit return type avoids inferring deeply nested structures
export interface ComputeResult {
  sum: number;
  product: number;
  items: Array<{ index: number; val: number }>;
}

export function fastCompute(x: number, y: number): ComputeResult {
  return {
    sum: x + y,
    product: x * y,
    items: Array.from({ length: x }, (_, i) => ({ index: i, val: y }))
  };
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Third-Party Declaration Bloat**:
   Huge libraries (like AWS SDK v2, Material UI, or Lodash) contain tens of thousands of type definitions. Omitting `"skipLibCheck": true` causes `tsc` to validate all external `.d.ts` files, often doubling compile times.
2. **Deeply Nested `Record<string, any>` Mappings**:
   Unconstrained string keys force the type checker to match arbitrary property accesses across wide union branches.
3. **Excessive Mapped Type Invocations**:
   Applying `DeepReadonly<T>` or `DeepPartial<T>` over large database entity models forces the compiler to traverse hundreds of AST branches repeatedly.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Practice | Good Practice | Problem / Diagnostic |
| :--- | :--- | :--- | :--- |
| **Omitting skipLibCheck** | `skipLibCheck: false` in large apps | Set `"skipLibCheck": true` | 2x-5x slower build times |
| **Undiscriminated Unions** | Wide union without tag property | Add literal discriminant (`type`, `kind`) | $O(N \times M)$ structural checks |
| **Excessive Intersections** | Chaining 10 `type A & B & C` | Use `interface D extends A, B, C` | Re-evaluates intersection shape |
| **Missing Return Types** | Omitting return types on exported APIs | Provide explicit return types | Forces recursive return inference |
| **Unbounded Recursion** | Recursive type without accumulator | Guard with tuple depth counter | `TS2589: Type instantiation is excessively deep` |
| **Large Literal Unions** | Union of 500 string literals | Use `Set<string>` or enum lookup | Slow type completion and checking |

---

## 8. Performance & Type-Checker Complexity

- **Interface Lookup**: $O(1)$ by symbol reference.
- **Structural Comparison**: $O(P)$ where $P$ is the property count.
- **Deep Mapped Type on Graph**: $O(V + E)$ AST traversal, potentially exponential if circular references exist.
- Use `--extendedDiagnostics` to inspect total type count, memory usage, and check time:
  ```bash
  $ tsc --noEmit --extendedDiagnostics
  Files:              450
  Lines:            82400
  Identifiers:      94200
  Symbols:         120500
  Types:            45000
  Check time:       1.85s
  ```

---

## 9. Compiler Internals

Inside `ts.TypeChecker`:
1. `getTypeAtLocation(node)`: Returns or instantiates the type representation for an AST node.
2. `isTypeAssignableTo(source, target)`: The core assignability engine. It checks identity caches (`relationCache`) before executing structural property matching.
3. If an evaluation depth counter exceeds 100 iterations, the checker emits error TS2589 to protect the V8 host from call stack overflows.

---

## 10. Best Practices & Design Guidelines

1. **Always Set `skipLibCheck: true`**: Do not re-verify external package declarations.
2. **Prefer Interfaces Over Intersections**: Interfaces are cached by symbol identity.
3. **Provide Explicit Return Types on Exported Functions**: Cuts inference cascades across module boundaries.
4. **Use Discriminated Unions**: Ensure literal tag properties exist for rapid branch pruning.

---

## 11. Real-World Case Study

A SaaS frontend project using styled-components and a complex icon library experienced 45-second IDE autocomplete delays.

**Investigation via `--generateTrace`**:
- `@typescript/analyze-trace` identified an icon helper type that generated a union of 4,000 icon names combined with 12 theme variations ($4,000 \times 12 = 48,000$ types).
- Resolving component props forced the compiler to evaluate this union on every JSX element.

**Fix**:
- Flattened the prop type to a simple `string` with autocomplete hints (`string & {}`).
- Autocomplete latency dropped from **45 seconds** to **150 milliseconds**.

---

## 12. Interview Questions

<details>
<summary>1. How do you generate and analyze a TypeScript compiler trace?</summary>

Run `tsc --noEmit --generateTrace ./trace-dir`, which produces `trace.json` and `types.json`. Then analyze the output using the official tool `npx @typescript/analyze-trace ./trace-dir`.
</details>

<details>
<summary>2. Why are TypeScript interfaces faster for the type checker than type intersections (`&`)?</summary>

Interfaces are assigned a stable `ts.Symbol` in the symbol table and cached by identity. When comparing two interfaces, TypeScript checks identity first ($O(1)$). Intersections create anonymous synthesized types that cannot be cached globally and must be structurally flattened repeatedly.
</details>

<details>
<summary>3. What causes compiler error TS2589?</summary>

TS2589 ("Type instantiation is excessively deep and possibly infinite") occurs when a recursive generic type exceeds the compiler's internal recursion depth limit without hitting a termination condition.
</details>

<details>
<summary>4. What does `--extendedDiagnostics` reveal?</summary>

It outputs performance metrics including total source files, lines of code, symbol counts, total type instances, memory footprint, I/O read time, check time, and emit time.
</details>

<details>
<summary>5. Why does omitting return types on exported functions hurt compiler performance?</summary>

Without an explicit return type, the compiler must traverse and evaluate the entire function body (including all internal expressions, branch logic, and return statements) to infer the type, triggering cascades of inference across dependent modules.
</details>

<details>
<summary>6. How does a discriminant tag property speed up union checking?</summary>

When checking assignability of a discriminated union, the type checker uses the discriminant literal property as a hash key to match the exact single union member instantly, avoiding an $M \times N$ pairwise structural comparison of all properties.
</details>

<details>
<summary>7. What is the impact of `skipLibCheck: true`?</summary>

It skips type checking of all `.d.ts` declaration files inside the project and `node_modules`, saving massive amounts of parse and check time in projects with large dependency trees.
</details>

<details>
<summary>8. How can you provide autocomplete for string literals without paying the performance cost of a massive union?</summary>

Use the string literal widening trick: `type Icon = "edit" | "delete" | (string & {})`. This provides IDE autocomplete for suggested strings while allowing any string without ballooning compiler union tables.
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Challenge 1 (Easy): Add diagnostic flags to compile script</summary>

**Task**: Add `--extendedDiagnostics` to your package.json typecheck script.

**Solution**:
```json
{
  "scripts": {
    "typecheck:profile": "tsc --noEmit --extendedDiagnostics"
  }
}
```
</details>

<details>
<summary>Challenge 2 (Easy): Convert type intersections to interfaces</summary>

**Task**: Refactor `type Combined = A & B` into an interface for better compiler caching.

**Solution**:
```typescript
interface A {
  id: string;
}

interface B {
  name: string;
}

interface Combined extends A, B {
  active: boolean;
}
```
</details>

<details>
<summary>Challenge 3 (Medium): Add depth-limited recursion guard to a generic type</summary>

**Task**: Write a tuple reverse type that terminates safely at 10 levels of recursion.

**Solution**:
```typescript
type Reverse<T extends unknown[], Depth extends unknown[] = []> =
  Depth["length"] extends 10
    ? T
    : T extends [infer First, ...infer Rest]
      ? [...Reverse<Rest, [...Depth, unknown]>, First]
      : [];
```
</details>

<details>
<summary>Challenge 4 (Medium): String literal suggestion without union cost</summary>

**Task**: Implement a `ThemeColor` type that autocompletes `"primary"` and `"secondary"` but accepts any arbitrary hex string without slow union checks.

**Solution**:
```typescript
type ThemeColor = "primary" | "secondary" | (string & {});

const c1: ThemeColor = "primary"; // Autocompleted!
const c2: ThemeColor = "#ff0000"; // Valid string accepted!
```
</details>

<details>
<summary>Challenge 5 (Hard): Scripted Compiler Benchmark Runner</summary>

**Task**: Write a Node.js script that runs `tsc --extendedDiagnostics` and parses check time and memory usage.

**Solution**:
```typescript
import { execSync } from "node:child_process";

const output = execSync("tsc --noEmit --extendedDiagnostics").toString();
const checkTimeMatch = output.match(/Check time:\s+([\d.]+s)/);
const memoryMatch = output.match(/Memory used:\s+([\d]+K)/);

console.log("TypeScript Compiler Health Report:");
console.log(`- Check Time: ${checkTimeMatch ? checkTimeMatch[1] : "N/A"}`);
console.log(`- Memory: ${memoryMatch ? (parseInt(memoryMatch[1]) / 1024).toFixed(1) + " MB" : "N/A"}`);
```
</details>

<details>
<summary>Challenge 6 (Hard): Refactor Megamorphic Union into Indexed Record</summary>

**Task**: Given a 100-member action union with identical property shapes, refactor the typing into a key-value record pattern to speed up compiler lookups.

**Solution**:
```typescript
// Define actions as an interface map instead of a massive union
export interface ActionPayloadMap {
  USER_LOGIN: { userId: string };
  USER_LOGOUT: { userId: string; reason: string };
  PAGE_VIEW: { url: string; referrer: string };
}

// Derive a clean, tagged union from the map
export type AppAction = {
  [K in keyof ActionPayloadMap]: {
    type: K;
    payload: ActionPayloadMap[K];
  };
}[keyof ActionPayloadMap];
```
</details>
