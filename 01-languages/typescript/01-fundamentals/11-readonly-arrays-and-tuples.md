# Readonly Arrays and Tuples

> Readonly arrays and tuples enforce compile-time immutability by stripping mutating methods from their type contracts, guaranteeing that downstream consumers cannot modify shared state.

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [00-types-and-inference.md](file:///E:/Learning/engineering-notes/01-languages/typescript/01-fundamentals/00-types-and-inference.md), [02-arrays-and-tuples.md](file:///E:/Learning/engineering-notes/01-languages/typescript/01-fundamentals/02-arrays-and-tuples.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 3.4 (`readonly` modifier & `as const`) / TS 5.x / TS 7.0 Native |
| **Tested tsconfig** | `strict: true`, `noUncheckedIndexedAccess: true` |

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
- **Problem Statement:** In JavaScript, arrays are mutable reference objects passed by reference. When a function mutates an array argument via `.push()`, `.sort()`, or `.splice()`, it alters state across the entire application, introducing race conditions and side effects in concurrent or reactive pipelines.
- **Historical Context:** TypeScript initially offered the generic interface `ReadonlyArray<T>`. TypeScript 3.4 dramatically improved ergonomics by introducing the `readonly` type modifier for array and tuple syntax (`readonly T[]`, `readonly [A, B]`) and the `const` assertion (`as const`).
- **Real-World Relevance:** In Redux stores, React hooks, functional pipelines, and high-reliability backend services, data structures must remain strictly immutable. Readonly arrays prevent accidental state mutation at build time with zero runtime copying overhead.

---

## 2. Core Concepts
- **Formal Invariants:**
  - A mutable array `T[]` is assignable to a `readonly T[]` (up-casting to a more restrictive interface).
  - A `readonly T[]` is NOT assignable to a mutable `T[]`.
  - Mutating methods (`push`, `pop`, `shift`, `unshift`, `splice`, `sort`, `reverse`) do NOT exist on the type definition of `readonly T[]`.
  - Index assignment (`arr[0] = value`) is blocked by the compiler: `Index signature in type 'readonly T[]' only permits reading`.
- **Mental Model:** Think of `readonly T[]` as a read-only lens or immutable view over an underlying array. It does not freeze the object at runtime (unless `Object.freeze()` is called), but it guarantees compile-time mutation safety.
- **Soundness vs Ergonomics:** Readonly modifiers provide shallow immutability. If an array holds mutable objects (`readonly User[]`), the array itself cannot have elements added or removed, but properties of individual `User` objects can still be modified unless explicitly typed as `readonly`.

---

## 3. How It Works Internally
- **Type Hierarchy:** In the TypeScript standard library, `ReadonlyArray<T>` declares only non-mutating members (`concat`, `slice`, `map`, `filter`, `reduce`, `length`). The mutable `Array<T>` interface extends `ReadonlyArray<T>` and adds mutating methods.
- **Structural Subtyping:** Because `Array<T>` has all members of `ReadonlyArray<T>` plus extras, `Array<T>` is a structural subtype of `ReadonlyArray<T>`.
- **Architectural Flow Diagram:**

```
+-------------------------------------------------------------------------+
|                  Array Type Hierarchy & Method Surface                  |
|                                                                         |
|                       interface ReadonlyArray<T>                        |
|                       ├── length: number                                |
|                       ├── [n: number]: T (readonly)                     |
|                       ├── slice(), map(), filter(), reduce()            |
|                                    ▲                                    |
|                                    │ extends (structural subtype)       |
|                                    │                                    |
|                           interface Array<T>                            |
|                           ├── push(), pop(), shift(), unshift()         |
|                           ├── splice(), sort(), reverse()               |
|                           └── [n: number]: T (read/write)               |
+-------------------------------------------------------------------------+
```

---

## 4. Syntax & API Reference
- **Syntactic Variations:**
  ```typescript
  type Arr1 = readonly string[];
  type Arr2 = ReadonlyArray<string>;
  type Tuple1 = readonly [string, number];
  type ReadonlyWithConst = readonly [number, ...string[]];
  ```
- **Const Assertions:**
  ```typescript
  const immutableList = [1, 2, 3] as const; // readonly [1, 2, 3]
  ```
- **tsconfig Dependencies:**
  - Operates under all standard configurations. Works in tandem with `noUncheckedIndexedAccess: true` to return `T | undefined` on index access.

---

## 5. Practical Examples

### Example 1: Basic / Foundational Usage
- **Goal:** Prevent mutation in utility functions.
- **Code:**
```typescript
function calculateAverage(grades: readonly number[]): number {
  // @ts-expect-error TS2339: Property 'push' does not exist on type 'readonly number[]'.
  grades.push(100);

  // @ts-expect-error TS2542: Index signature in type 'readonly number[]' only permits reading.
  grades[0] = 50;

  const sum = grades.reduce((acc, curr) => acc + curr, 0);
  return grades.length === 0 ? 0 : sum / grades.length;
}

const myGrades: number[] = [90, 85, 95];
const avg = calculateAverage(myGrades); // Legal: mutable number[] assignable to readonly number[]
```
- **Output / Type Evaluation:**
```typescript
// avg evaluates to 90
```
- **Explanation:** Accepting `readonly number[]` documents and enforces that the function will not mutate caller inputs.

### Example 2: Real-World Enterprise Pattern (State Machine Transition Map with As Const)
- **Goal:** Model immutable state machine transition tables.
- **Code:**
```typescript
export const VALID_TRANSITIONS = {
  Draft: ['PendingReview', 'Archived'],
  PendingReview: ['Approved', 'Rejected'],
  Approved: ['Published', 'Archived'],
  Rejected: ['Draft', 'Archived'],
  Published: ['Archived'],
  Archived: [],
} as const;

type State = keyof typeof VALID_TRANSITIONS;

export function canTransition<S extends State>(
  from: S,
  to: string
): boolean {
  const allowed: readonly string[] = VALID_TRANSITIONS[from];
  return allowed.includes(to);
}
```
- **Output / Type Evaluation:**
```typescript
// VALID_TRANSITIONS is deeply readonly; cannot be modified at runtime or compile-time.
```
- **Explanation:** `as const` locks the object and its inner arrays into `readonly` literal tuples, guaranteeing compile-time integrity for state machine rules.

### Example 3: Edge Case / Boundary Condition (Array.prototype.sort Mutates In-Place)
- **Goal:** Safely sort a readonly array without in-place mutation.
- **Code:**
```typescript
function safeSortNumbers(numbers: readonly number[]): number[] {
  // @ts-expect-error TS2339: Property 'sort' does not exist on type 'readonly number[]'.
  numbers.sort();

  // Idiomatic: slice/toSorted before sorting
  // ES2023 toSorted creates a new array:
  return [...numbers].sort((a, b) => a - b);
}

const original: readonly number[] = [5, 2, 8, 1];
const sorted = safeSortNumbers(original);
console.log('Original remains intact:', original); // [5, 2, 8, 1]
console.log('Sorted copy:', sorted);             // [1, 2, 5, 8]
```
- **Output / Type Evaluation:**
```typescript
// Original array remains unmodified.
```
- **Explanation:** JavaScript's `.sort()` mutates the array in-place. Because `readonly number[]` lacks `.sort()`, the developer is prevented from mutating caller state.

### Example 4: Negative Test / Type-Check Failure Case
- **Goal:** Demonstrate rejection when assigning readonly array to mutable parameter.
- **Code:**
```typescript
function mutateInPlace(items: string[]): void {
  items.push('mutation');
}

const readOnlyData: readonly string[] = ['a', 'b', 'c'];

// @ts-expect-error TS2345: Argument of type 'readonly string[]' is not assignable to parameter of type 'string[]'.
mutateInPlace(readOnlyData);
```
- **Real Compiler Error:**
```
error TS2345: Argument of type 'readonly string[]' is not assignable to parameter of type 'string[]'.
  The type 'readonly string[]' is 'readonly' and cannot be assigned to the mutable type 'string[]'.
```
- **Explanation:** The type checker enforces safety: a readonly collection can never be passed to a function that requires a mutable collection.

### Example 5: Idiomatic Modern Usage (Const Assertions for API Route Defs)
- **Goal:** Declare type-safe, immutable HTTP route configurations with exact tuple lengths.
- **Code:**
```typescript
export const ROUTE_DEFS = [
  ['GET', '/api/users', 'listUsers'],
  ['POST', '/api/users', 'createUser'],
  ['GET', '/api/users/:id', 'getUser'],
  ['DELETE', '/api/users/:id', 'deleteUser'],
] as const;

type RouteDefinition = (typeof ROUTE_DEFS)[number];
// readonly ["GET", "/api/users", "listUsers"] | ...

type HttpMethod = RouteDefinition[0];
// "GET" | "POST" | "DELETE"
```
- **Output / Type Evaluation:**
```typescript
// RouteDefinition is a discriminated union of exact 3-element readonly tuples.
```
- **Explanation:** Const assertions derive exact string literal unions and tuple indices without writing redundant manual type declarations.

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Shallow Immutability Only:** `readonly Item[]` prevents adding or reassigning items in the array, but `items[0].name = 'foo'` is still legal if `Item` has mutable fields. Use `DeepReadonly<T>` for full immutability.
2. **Missing Runtime Freezing:** `readonly` is completely erased at runtime. If JavaScript consumers or unsandboxed code call `arr.push()`, it will mutate the array unless `Object.freeze()` was used.
3. **Array Destructuring Drops Readonly:** `const [first, ...rest] = readonlyTuple` infers `rest` as a mutable array (`T[]`) unless explicitly typed as `readonly`.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern (Naive / Broken) | Real Compiler Error / Bug | Idiomatic Solution | Why It Matters |
| :--- | :--- | :--- | :--- |
| Casting away readonly: `arr as T[]` | Silent mutation of shared or frozen memory | Make a shallow copy: `[...arr]` | Preserves immutability guarantees |
| Calling `.sort()` on mutable inputs | Caller array mutated unpredictably | Accept `readonly T[]` and copy before sort | Eliminates unexpected side-effects |
| Using `ReadonlyArray<T>` without `readonly` fields | Nested object properties mutated | Combine with `readonly` object types | Prevents deep data corruption |
| Declaring mutable tuples for constants | Permitted to reassign elements: `pair[0] = 99` | Use `as const` or `readonly [A, B]` | Protects configuration tuples |
| Using `Object.freeze` without `as const` | Inferred as mutable type despite runtime freeze | `Object.freeze([...]) as const` | Aligns compile-time types with runtime behavior |
| Overwriting tuple indices dynamically | Out-of-bounds corruption | Enforce fixed tuple bounds | Guarantees exact coordinate/pair lengths |

---

## 8. Performance & Type-Checker Complexity
- **Zero Runtime Overhead:** `readonly` array and tuple modifiers are pure type annotations; they generate identical JavaScript to mutable arrays.
- **Type Checker Traversal:** Assigning `Array<T>` to `ReadonlyArray<T>` succeeds via fast-path structural subtyping in the checker.
- **Isolated Declarations:** Fully supported under `--isolatedDeclarations`.

---

## 9. Compiler Internals
- **AST Node Representation:**
  - `readonly T[]` is parsed as `ts.TypeOperatorNode` (`SyntaxKind.TypeOperator`) with operator `SyntaxKind.ReadonlyKeyword`.
  - `readonly [A, B]` is a `ts.TupleTypeNode` with `SyntaxKind.ReadonlyKeyword`.
- **Type Flags:** Marked with `TypeFlags.Object` and `ObjectFlags.Tuple` / `ObjectFlags.Reference`.
- **Diagnostic Generation:** Any attempt to index-assign to a readonly array raises `Diagnostics.Index_signature_in_type_0_only_permits_reading` (TS2542).

---

## 10. Best Practices & Design Guidelines
- **Default to `readonly T[]` for Function Parameters:** Unless a function is specifically designed to mutate an array in place, always type array parameters as `readonly T[]`.
- **Use `as const` for Fixed Tables and Options:** Lock configuration lists into readonly tuples to extract literal unions.
- **Implement Deep Immutability for Store State:** Use `DeepReadonly<T>` utilities to guard state in architecture layers.

---

## 11. Real-World Case Study
- **Scenario:** A high-throughput order routing engine maintained an internal cache of active routing rules. A newly added discount calculation service invoked `rules.sort(byPriority)` to evaluate rules. Because `.sort()` mutates arrays in place, the shared cache order was scrambled on every request, causing non-deterministic rule matching and $85,000 in missed volume discounts.
- **Implementation:** Updated the cache contract to return `readonly RoutingRule[]`:
  ```typescript
  // Before:
  function getActiveRules(): RoutingRule[] { ... }

  // After:
  function getActiveRules(): readonly RoutingRule[] { ... }
  ```
- **Outcome:** The compiler immediately raised `TS2339: Property 'sort' does not exist on type 'readonly RoutingRule[]'` inside the discount service, forcing developers to use `[...rules].sort()`, permanently eliminating cache corruption.

---

## 12. Interview Questions

### Q1: Why is `number[]` assignable to `readonly number[]`, but `readonly number[]` is not assignable to `number[]`?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

This is an application of subtyping and the principle of least privilege. `ReadonlyArray<T>` defines a subset of the methods on `Array<T>` (only read operations). A mutable array can perform everything a readonly array promises (it can be read, sliced, mapped), so passing a mutable array where a readonly array is expected is safe. However, a `readonly number[]` cannot fulfill the contract of `number[]` because it lacks mutating methods (`push`, `pop`, index assignment); assigning it to `number[]` would allow callers to mutate a collection that was promised to be immutable.
</details>

### Q2: What is the difference between `const arr = [1, 2]` and `const arr = [1, 2] as const`?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

- `const arr = [1, 2]`: `const` only prevents reassigning the variable identifier `arr`. The inferred type is `number[]` (a mutable array of numbers of arbitrary length).
- `const arr = [1, 2] as const`: The `as const` assertion instructs the compiler to infer the most specific literal types possible and mark the collection immutable. The inferred type is `readonly [1, 2]` (a fixed-length tuple of literal numbers 1 and 2 that cannot be modified).
</details>

### Q3: How do you implement a `DeepReadonly<T>` utility type in TypeScript?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

```typescript
type DeepReadonly<T> = T extends Function | boolean | number | string | symbol | bigint | null | undefined
  ? T
  : T extends Array<infer U>
  ? ReadonlyArray<DeepReadonly<U>>
  : T extends Map<infer K, infer V>
  ? ReadonlyMap<DeepReadonly<K>, DeepReadonly<V>>
  : T extends Set<infer M>
  ? ReadonlySet<DeepReadonly<M>>
  : T extends object
  ? { readonly [K in keyof T]: DeepReadonly<T[K]> }
  : T;
```
This recurses through nested objects, arrays, maps, and sets, applying `readonly` at every level while leaving primitives and functions intact.
</details>

### Q4: Does `readonly` prevent runtime mutations in compiled JavaScript?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

No. `readonly` is a compile-time-only type construct that is completely erased during JavaScript emit. At runtime, the underlying object is a normal JavaScript Array. If untyped JavaScript code or type assertions (`as number[]`) mutate the array, the mutation will succeed at runtime unless `Object.freeze()` was explicitly called.
</details>

### Q5: Can a tuple have optional elements and still be readonly?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Yes. You can declare a readonly tuple with optional elements:
```typescript
type PointConfig = readonly [number, number, string?];
const p1: PointConfig = [10, 20];
const p2: PointConfig = [10, 20, 'center'];
```
All elements, whether present or omitted, are immutable.
</details>

### Q6: What happens when you spread a `readonly` array into another array?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Spreading a readonly array (`const copy = [...readonlyArr]`) creates a new, fresh JavaScript array. The newly created array is inferred as a mutable array (`T[]`), unless the expression is appended with `as const`. This is the standard, idiomatic technique for obtaining a mutable copy of a readonly array.
</details>

### Q7: Why does `Array.isArray()` narrow `unknown` to `any[]` by default instead of `unknown[]`?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

In historical TypeScript standard library definitions, `Array.isArray(arg)` returned `arg is any[]` for backward compatibility. In modern configurations or with strict type definitions, it is recommended to type-guard arrays to `arg is readonly unknown[]` or `arg is unknown[]` to avoid accidental `any` leaks.
</details>

### Q8: How does `readonly` interact with tuple rest elements?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

You can combine `readonly` with rest elements:
```typescript
type Header = readonly [string, ...number[]];
```
This requires the first element to be a string, followed by zero or more numbers. All elements in the sequence are protected against mutation.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Safe Array Slicer (Easy)
- **Problem Statement:** Write a function `firstAndLast<T>(arr: readonly T[]): readonly [T, T] | null` that returns the first and last elements as an immutable tuple, or `null` if the array has fewer than 2 elements.
- **Test Case:**
```typescript
firstAndLast([1, 2, 3, 4] as const); // [1, 4]
firstAndLast([1]); // null
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function firstAndLast<T>(arr: readonly T[]): readonly [T, T] | null {
  if (arr.length < 2) return null;
  return [arr[0]!, arr[arr.length - 1]!] as const;
}
```
</details>

### Exercise 2: Freeze Helper (Easy)
- **Problem Statement:** Write a function `freezeArray<T>(items: T[]): readonly T[]` that calls `Object.freeze` and returns the array typed as `readonly T[]`.
- **Test Case:**
```typescript
const frozen = freezeArray([1, 2, 3]);
// frozen is readonly number[]
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function freezeArray<T>(items: T[]): readonly T[] {
  return Object.freeze([...items]);
}
```
</details>

### Exercise 3: Mutable Copy Utility (Medium)
- **Problem Statement:** Create a generic utility type `Mutable<T>` that strips the `readonly` modifier from arrays, tuples, and object properties.
- **Test Case:**
```typescript
type ReadonlyTuple = readonly [string, number];
type MutableTuple = Mutable<ReadonlyTuple>; // [string, number]
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
type Mutable<T> = {
  -readonly [K in keyof T]: T[K];
};
```
</details>

### Exercise 4: Immutable Push Simulation (Medium)
- **Problem Statement:** Write a pure function `immutablePush<T>(arr: readonly T[], item: T): readonly T[]` that returns a new array with the item appended, without modifying the input array.
- **Test Case:**
```typescript
const original: readonly number[] = [1, 2];
const updated = immutablePush(original, 3); // [1, 2, 3]
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function immutablePush<T>(arr: readonly T[], item: T): readonly T[] {
  return [...arr, item];
}
```
</details>

### Exercise 5: DeepReadonly Type Implementation (Hard)
- **Problem Statement:** Author a comprehensive `DeepReadonly<T>` recursive mapped type that handles objects, arrays, tuples, primitives, and functions.
- **Test Case:**
```typescript
type Complex = {
  users: { id: number; roles: string[] }[];
  config: { meta: { flag: boolean } };
};
type Immutable = DeepReadonly<Complex>;
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
type DeepReadonly<T> = T extends Function | boolean | number | string | symbol | bigint | null | undefined
  ? T
  : T extends Array<infer U>
  ? ReadonlyArray<DeepReadonly<U>>
  : T extends object
  ? { readonly [K in keyof T]: DeepReadonly<T[K]> }
  : T;
```
</details>

### Exercise 6: Compile-Time Fixed Length Vector (Hard)
- **Problem Statement:** Write a type-safe fixed-size matrix transformer `Vector<N extends number, T>` where tuple lengths are strictly enforced at compile time, and author an `innerProduct` function that rejects vectors of mismatched dimensions.
- **Test Case:**
```typescript
type Vec3 = readonly [number, number, number];
const v1: Vec3 = [1, 2, 3];
const v2: Vec3 = [4, 5, 6];
// dotProduct(v1, v2) === 32
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
type TupleOf<T, N extends number, R extends T[] = []> = R['length'] extends N
  ? Readonly<R>
  : TupleOf<T, N, [...R, T]>;

type Vector<N extends number> = TupleOf<number, N>;

function dotProduct<N extends number>(
  a: Vector<N>,
  b: Vector<N>
): number {
  return (a as readonly number[]).reduce((sum, val, idx) => sum + val * (b as readonly number[])[idx]!, 0);
}
```
</details>
