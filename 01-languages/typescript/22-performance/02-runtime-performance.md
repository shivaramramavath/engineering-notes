# Runtime Performance in TypeScript: V8 Optimization, Inlining, and Data Layout

> In-depth analysis of runtime JavaScript execution generated from TypeScript: V8 hidden classes, monomorphic call sites, object shapes, enum cost vs `as const`, and data layout optimization.

| Metadata | Details |
| :--- | :--- |
| **Category** | Performance |
| **Difficulty** | Advanced |
| **Prerequisites** | [01-fundamentals/01-primitive-types.md](../01-fundamentals/01-primitive-types.md), [14-compiler-internals/00-compiler-architecture.md](../14-compiler-internals/00-compiler-architecture.md) |
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

A common misconception is that "TypeScript has zero runtime overhead." While TypeScript type annotations themselves are completely stripped at compile time, **the design of your TypeScript types directly dictates the runtime JavaScript shapes, memory layouts, and execution performance** executed by the V8 JavaScript engine (Node.js and Chrome).

Subtle type modeling choices produce dramatically different runtime code:
- Initializing properties conditionally causes V8 **hidden class transitions**, de-optimizing monomorphic call sites into megamorphic dictionary lookups (up to **10x slower**).
- TypeScript `enum` declarations emit runtime IIFE closures and bidirectional object maps, increasing bundle size and preventing tree-shaking.
- Indiscriminate down-leveling (`target: ES5`) transforms lightweight spread operators and async loops into massive helper functions with high garbage collection churn.

---

## 2. Core Concepts

### V8 Hidden Classes & Shape Transitions
V8 creates internal "hidden classes" (also called Shapes or Maps) for JavaScript objects. If two objects share the identical property layout created in the exact same initialization order, they share the same hidden class.
- **Monomorphic Call Site**: A function that always receives objects with the exact same hidden class. V8 optimizes property access into a direct memory offset ($O(1)$ CPU register lookup).
- **Megamorphic Call Site**: A function receiving objects with 5+ different shapes. V8 falls back to expensive hash table property lookups.

```
+-------------------------------------------------------------------------+
|                  V8 Hidden Class Transitions                            |
+-------------------------------------------------------------------------+
|                                                                         |
|  Object A: { x: 1, y: 2 }              Object B: { y: 2, x: 1 }         |
|                                                                         |
|  Hidden Class 0: Empty                 Hidden Class 0: Empty            |
|       |                                     |                           |
|       v (+x)                                v (+y)                      |
|  Hidden Class 1: { x }                 Hidden Class 3: { y }            |
|       |                                     |                           |
|       v (+y)                                v (+x)                      |
|  Hidden Class 2: { x, y }              Hidden Class 4: { y, x }         |
|                                                                         |
|  Result: Object A and Object B have DIFFERENT hidden classes!           |
|  Functions accessing .x on both become POLYMORPHIC!                     |
|                                                                         |
+-------------------------------------------------------------------------+
```

### TypeScript Enums vs. `as const` Objects
- **TypeScript `enum`**: Emits an IIFE and runtime object:
  ```javascript
  var Status;
  (function (Status) { Status[Status["Active"] = 0] = "Active"; })(Status || (Status = {}));
  ```
- **`as const` Object**: Bundlers can inline and tree-shake literal string values completely:
  ```typescript
  export const STATUS = { Active: "ACTIVE" } as const;
  ```

---

## 3. How It Works Internally

1. **Inline Caches (IC)**: V8 generates inline caches at property access sites (`point.x`). On the first execution, V8 records the object's hidden class. On subsequent calls with the same hidden class, it executes a single memory read.
2. **De-optimization**: If an unexpected property shape is encountered, the JIT compiler de-optimizes the TurboFan-compiled native assembly code back to the Ignition bytecode interpreter.
3. **Array Layouts**: Arrays of packed numbers (`PACKED_SMI_ELEMENTS` or `PACKED_DOUBLE_ELEMENTS`) are stored contiguously in memory. Assigning `undefined` or creating holes downgrades the array to `HOLEY_ELEMENTS`, disabling SIMD optimizations.

---

## 4. Syntax & API Reference

### Idiomatic Zero-Overhead Const Object Pattern
```typescript
export const HTTP_STATUS = {
  OK: 200,
  CREATED: 201,
  BAD_REQUEST: 400,
  UNAUTHORIZED: 401,
  NOT_FOUND: 404,
} as const;

export type HttpStatus = (typeof HTTP_STATUS)[keyof typeof HTTP_STATUS];
```

### Contiguous Packed Array Pattern
```typescript
// Fast: Packed SMI (Small Integer) Array
const fastArray: number[] = new Array<number>(1000);
for (let i = 0; i < 1000; i++) {
  fastArray[i] = i; // Stored contiguously
}
```

---

## 5. Practical Examples

### Example 1: Hidden Class Preservation via Uniform Initialization
```typescript
interface Point {
  x: number;
  y: number;
  z?: number;
}

// SLOW: Inconsistent property initialization orders create different hidden classes
function createSlowPoint(x: number, y: number, z?: number): Point {
  const p: Point = { x, y };
  if (z !== undefined) {
    p.z = z; // Causes shape transition C0 -> C1
  }
  return p;
}

// FAST: Uniform property shape initialized in identical order
function createFastPoint(x: number, y: number, z = 0): Point {
  return { x, y, z }; // Every instance shares the exact same hidden class
}
```

### Example 2: Monomorphic Function Call Site
```typescript
interface Entity {
  readonly id: string;
  readonly value: number;
}

// Monomorphic function: receives ONLY objects of shape { id, value }
export function computeTotal(entities: readonly Entity[]): number {
  let total = 0;
  for (let i = 0; i < entities.length; i++) {
    // Monomorphic property read: single assembly instruction in V8 TurboFan
    total += entities[i]!.value;
  }
  return total;
}
```

### Example 3: Edge Case - `delete` Operator De-optimization
```typescript
interface UserSession {
  token: string;
  temporaryKey?: string;
}

const session: UserSession = { token: "abc", temporaryKey: "temp_123" };

// SLOW: Using `delete` forces V8 to downgrade the object to slow dictionary mode!
// delete session.temporaryKey;

// FAST: Assign undefined instead of deleting keys to maintain hidden class
session.temporaryKey = undefined;
```

### Example 4: Compiler Error & Property Type Safety
```typescript
interface Vector3 {
  x: number;
  y: number;
  z: number;
}

function processVector(v: Vector3): number {
  return v.x + v.y + v.z;
}

// @ts-expect-error - TS2345: Argument of type '{ x: number; y: number; }' is not assignable to parameter of type 'Vector3'.
processVector({ x: 10, y: 20 });
```

### Example 5: High-Performance Flat Object Pool
```typescript
export class VectorPool {
  private pool: Array<{ x: number; y: number }> = [];

  constructor(size = 1000) {
    for (let i = 0; i < size; i++) {
      this.pool.push({ x: 0, y: 0 }); // Pre-allocated identical hidden classes
    }
  }

  public acquire(x: number, y: number): { x: number; y: number } {
    const vec = this.pool.pop() ?? { x: 0, y: 0 };
    vec.x = x;
    vec.y = y;
    return vec;
  }

  public release(vec: { x: number; y: number }): void {
    this.pool.push(vec);
  }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Down-level Iteration Overhead**:
   Targeting `ES5` or `ES2015` with `"downlevelIteration": true` causes `for...of` loops to emit heavy iterator helpers (`__values`, `__read`), creating garbage objects on every loop cycle. Target `ES2022` to emit clean native bytecode.
2. **Hole Creation in Arrays**:
   Accessing `arr[100] = 5` on an array of length 2 creates a holey array (`HOLEY_SMI_ELEMENTS`). V8 must search up the prototype chain on every subsequent index read!
3. **Numeric Key Conversions**:
   In JavaScript, `{ 1: "a", 2: "b" }` stores keys as integer elements, not named properties. Mixing string and numeric keys causes shape fragmentation.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Practice | Good Practice | Problem / Diagnostic |
| :--- | :--- | :--- | :--- |
| **Using `delete`** | `delete obj.prop` | `obj.prop = undefined` | Downgrades object to dictionary mode |
| **Out-of-Order Init** | `{ a: 1, b: 2 }` and `{ b: 2, a: 1 }` | Always initialize properties in same order | Creates disparate hidden classes |
| **Enum Bundle Bloat** | `export enum Mode { A, B }` | `export const MODE = { A: 'A' } as const` | Generates non-tree-shakeable IIFE |
| **Sparse Arrays** | `const arr = []; arr[500] = 1;` | `Array.from` or push sequentially | Holey array degrades to hash table |
| **Old Compiler Target** | `"target": "ES5"` in modern Node | `"target": "ES2022"` | Heavy polyfill helper overhead |
| **Polymorphic Functions** | Passing 10 disparate object shapes | Standardize parameters to unified interface | De-optimizes inline caches |

---

## 8. Performance & Type-Checker Complexity

| Property Access Type | V8 Access Speed | CPU Assembly Equivalent |
| :--- | :--- | :--- |
| **Monomorphic** (1 shape) | ~1.1 ns | Direct memory offset read `[rax + 16]` |
| **Polymorphic** (2-4 shapes) | ~2.5 ns | Comparison jump table |
| **Megamorphic** (5+ shapes) | ~12.8 ns | Hash table dictionary lookup |
| **Dictionary Mode** (after `delete`) | ~28.0 ns | Full property hash resolution |

---

## 9. Compiler Internals

When emitting code:
- The TypeScript emitter (`emitter.ts`) checks the `target` language version.
- If `target >= ES2022`, native class fields (`#field`), parameter properties, and spread operations emit verbatim without auxiliary helper functions.
- Const enums are completely erased from the output JS unless `preserveConstEnums: true` is explicitly enabled.

---

## 10. Best Practices & Design Guidelines

1. **Initialize All Object Properties Upfront**: Never add properties dynamically after creation.
2. **Avoid `delete`**: Assign `undefined` or `null` to retain the hidden class.
3. **Replace Enums with `as const` Objects**: Enables zero-overhead tree shaking.
4. **Target Modern ECMAScript (`ES2022`+)**: Avoids performance-degrading downlevel compiler helpers.

---

## 11. Real-World Case Study

A real-time geospatial tracking service handling 100,000 GPS coordinates/second suffered high CPU spikes and garbage collection pauses.

**Investigation**:
V8 profiling revealed that GPS points were constructed conditionally:
```typescript
const point = { lat, lng };
if (altitude) point.alt = altitude; // Shape transition!
```
Every point fed into `calculateDistance()` encountered 2 different hidden classes, causing the function to run polymorphically.

**Remediation**:
Refactored point construction to always initialize `alt: altitude ?? 0`.
**Result**:
- Function inlined cleanly into TurboFan native machine code.
- CPU consumption decreased by **42%**.
- Throughput increased from 100k to **175k points/second**.

---

## 12. Interview Questions

<details>
<summary>1. What are V8 hidden classes and why do they matter for TypeScript developers?</summary>

Hidden classes (Shapes) are internal data structures V8 uses to track object properties and their memory offsets. Initializing properties in different orders or adding properties dynamically creates distinct hidden classes, turning fast monomorphic call sites into slow polymorphic or megamorphic dictionary lookups.
</details>

<details>
<summary>2. Why does using the `delete` operator hurt performance in Node.js?</summary>

The `delete` operator alters the object's shape unpredictably, forcing V8 to abandon the hidden class model for that object and downgrade it to "dictionary mode" (hash table), making all subsequent property accesses significantly slower.
</details>

<details>
<summary>3. Why are `as const` objects preferred over standard TypeScript `enum`s?</summary>

Standard `enum`s emit runtime IIFE JavaScript functions that cannot be tree-shaken by modern bundlers. `as const` objects compile to plain JavaScript objects or are inlined directly at compile time, eliminating runtime overhead.
</details>

<details>
<summary>4. What is the difference between a monomorphic and megamorphic call site?</summary>

A monomorphic call site observes only one object shape and compiles down to a direct memory offset instruction. A megamorphic call site encounters 5 or more distinct shapes, requiring V8 to perform generic hash table lookups for every property access.
</details>

<details>
<summary>5. What is a "holey array" in V8?</summary>

A holey array contains unassigned index elements (e.g. `const arr = []; arr[10] = 'value'`). V8 cannot assume contiguous memory and must traverse prototype chains to verify whether missing indices are defined on `Array.prototype`, degrading array iteration speed.
</details>

<details>
<summary>6. How does compiler `target` affect runtime performance?</summary>

Lower targets (like `ES5`) require TypeScript to emit polyfill helpers for `async/await`, class fields, and spreads, increasing bundle size and function call overhead. Modern targets (`ES2022`) emit native V8 bytecode instructions.
</details>

<details>
<summary>7. How can you ensure two TypeScript object literals share the exact same V8 hidden class?</summary>

By declaring all properties upfront in the exact same order with defined initial values (using `null` or `undefined` instead of omitting the key).
</details>

<details>
<summary>8. Does TypeScript's type checking happen at runtime?</summary>

No. TypeScript types are purely static compile-time constructs. They are completely erased during compilation and do not execute at runtime. However, the data structures and shapes they define directly govern runtime engine performance.
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Challenge 1 (Easy): Replace enum with as const</summary>

**Task**: Refactor `enum LogLevel { DEBUG, INFO, ERROR }` into a zero-overhead `as const` object.

**Solution**:
```typescript
export const LOG_LEVEL = {
  DEBUG: "DEBUG",
  INFO: "INFO",
  ERROR: "ERROR",
} as const;

export type LogLevel = (typeof LOG_LEVEL)[keyof typeof LOG_LEVEL];
```
</details>

<details>
<summary>Challenge 2 (Easy): Fix property order for hidden class consistency</summary>

**Task**: Fix the second object literal so both share the same V8 hidden class.
```typescript
const obj1 = { id: 1, name: "Alpha", active: true };
const obj2 = { name: "Beta", id: 2, active: false };
```

**Solution**:
```typescript
const obj1 = { id: 1, name: "Alpha", active: true };
const obj2 = { id: 2, name: "Beta", active: false }; // Same order!
```
</details>

<details>
<summary>Challenge 3 (Medium): Pre-allocate packed SMI array</summary>

**Task**: Write a function that initializes a packed integer array of size $N$ without creating array holes.

**Solution**:
```typescript
export function createPackedIntArray(size: number): number[] {
  const arr = new Array<number>(size);
  for (let i = 0; i < size; i++) {
    arr[i] = 0; // Initialize immediately to avoid HOLEY elements
  }
  return arr;
}
```
</details>

<details>
<summary>Challenge 4 (Medium): Avoid delete operator</summary>

**Task**: Refactor a session logout function that currently uses `delete session.authToken` to maintain V8 shape stability.

**Solution**:
```typescript
interface Session {
  userId: string;
  authToken: string | null;
}

export function logout(session: Session): void {
  session.authToken = null; // Preserves hidden class
}
```
</details>

<details>
<summary>Challenge 5 (Hard): Monomorphic Micro-benchmark in TypeScript</summary>

**Task**: Write a benchmark script that demonstrates the performance difference between accessing properties on monomorphic vs megamorphic objects.

**Solution**:
```typescript
function benchmark() {
  const mono = Array.from({ length: 100_000 }, (_, i) => ({ x: i, y: i * 2 }));
  const mega = Array.from({ length: 100_000 }, (_, i) => {
    switch (i % 5) {
      case 0: return { x: i, a: 1 };
      case 1: return { b: 1, x: i };
      case 2: return { c: 1, d: 2, x: i };
      case 3: return { x: i, e: 1, f: 2 };
      default: return { g: 1, x: i };
    }
  });

  console.time("Monomorphic");
  let sumMono = 0;
  for (const item of mono) sumMono += item.x;
  console.timeEnd("Monomorphic");

  console.time("Megamorphic");
  let sumMega = 0;
  for (const item of mega) sumMega += item.x;
  console.timeEnd("Megamorphic");
}

benchmark();
```
</details>

<details>
<summary>Challenge 6 (Hard): Flat Struct of Arrays (SoA) Layout</summary>

**Task**: Implement a Particle system using Struct of Arrays (SoA) layout instead of Array of Structs (AoS) to maximize CPU cache locality.

**Solution**:
```typescript
// Struct of Arrays (SoA): Maximizes sequential memory access in V8
export class ParticleSystemSoA {
  public posX: Float32Array;
  public posY: Float32Array;
  public velX: Float32Array;
  public velY: Float32Array;

  constructor(public readonly count: number) {
    this.posX = new Float32Array(count);
    this.posY = new Float32Array(count);
    this.velX = new Float32Array(count);
    this.velY = new Float32Array(count);
  }

  public update(dt: number): void {
    const len = this.count;
    for (let i = 0; i < len; i++) {
      this.posX[i] += this.velX[i]! * dt;
      this.posY[i] += this.velY[i]! * dt;
    }
  }
}
```
</details>
