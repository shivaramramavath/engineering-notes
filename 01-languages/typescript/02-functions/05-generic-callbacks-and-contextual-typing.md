# Generic Callbacks and Contextual Typing

> Generic callbacks bridge top-down contextual typing with bottom-up type argument synthesis, allowing higher-order functions to infer complex relationships without manual annotations.

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [00-function-types.md](file:///E:/Learning/engineering-notes/01-languages/typescript/02-functions/00-function-types.md), [02-callbacks.md](file:///E:/Learning/engineering-notes/01-languages/typescript/02-functions/02-callbacks.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 3.4 / TS 5.4 (`NoInfer<T>`) / TS 7.0 Native |
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
- **Problem Statement:** In complex functional architectures, callbacks frequently transform types (e.g. mapping an array of entities to promises of aggregated DTOs). If the type system cannot infer generic parameters across callback boundaries, developers are forced to write verbose explicit type parameters at call sites (`pipe<User, DTO, Response>(...)`).
- **Historical Context:** Early TypeScript struggled with inference cycles when callback parameters depended on generic type variables that were themselves inferred from the callback's return value. TypeScript 3.4 overhauled higher-order function type inference. TypeScript 5.4 introduced `NoInfer<T>` to prevent unwanted inference candidates from callback sites.
- **Real-World Relevance:** Generic callbacks power functional libraries (RxJS, Lodash/fp, Effect-TS), state machine transition reducers, and modern framework composables (TanStack Query, Vue composables).

---

## 2. Core Concepts
- **Formal Invariants:**
  - Bidirectional Inference: TypeScript balances bottom-up expression synthesis (inferring `T` from arguments) with top-down contextual typing (pushing inferred `T` into callback parameters).
  - Multi-Phase Inference: The compiler evaluates non-function arguments first to fix initial type parameters, then uses those parameters to contextually type callback arguments, and finally synthesizes the callback return type.
  - `NoInfer<T>` (TS 5.4+): Blocks an argument or callback from participating in generic type parameter inference, forcing `T` to be resolved exclusively from primary positions.
- **Mental Model:** Think of generic callback inference as an assembly line. Station 1 inspects input data and locks in `T`. Station 2 feeds `T` into the callback parameter. Station 3 inspects what the callback produces to lock in return type `R`.
- **Soundness vs Ergonomics:** When callbacks return promises or union literals, TypeScript computes the common supertype, preserving safety while minimizing annotations.

---

## 3. How It Works Internally
- **Inference Graph Construction:** In `checker.ts`, `inferTypeArguments` builds an inference graph of type variables. Dependencies between type variables are tracked as directed edges.
- **Top-Down Propagation:** `getContextualType` pushes inferred types from prior arguments into the callback parameters before inspecting the callback body.
- **Architectural Flow Diagram:**

```
+--------------------------------------------------------------------------+
|                  Multi-Phase Generic Callback Inference                  |
|                                                                          |
|   Signature: function transform<T, R>(data: T, fn: (item: T) => R): R    |
|   Call Site: transform("hello", (item) => item.length)                   |
|                                                                          |
|   Phase 1: Evaluate 'data' argument ("hello")                            |
|            ──► Candidate for T = string. Locks T = string                |
|                                                                          |
|   Phase 2: Push contextual type into 'fn' parameter                      |
|            ──► 'item' is contextually typed as string                    |
|                                                                          |
|   Phase 3: Evaluate body 'item.length'                                   |
|            ──► Evaluates to number. Candidate for R = number             |
|                                                                          |
|   Result:  Return type is inferred as number!                            |
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & API Reference
- **Generic Higher-Order Signature:**
  ```typescript
  function pipe<A, B, C>(
    a: A,
    ab: (a: A) => B,
    bc: (b: B) => C
  ): C;
  ```
- **Using NoInfer (TypeScript 5.4+):**
  ```typescript
  function createSelector<T>(
    items: T[],
    defaultItem: NoInfer<T> // Prevents defaultItem from widening T!
  ): T;
  ```
- **tsconfig Dependencies:** Operates under standard configurations.

---

## 5. Practical Examples

### Example 1: Basic / Foundational Usage
- **Goal:** Author a generic pipe function with multi-stage callback inference.
- **Code:**
```typescript
export function flow<A, B, C>(
  input: A,
  step1: (arg: A) => B,
  step2: (arg: B) => C
): C {
  return step2(step1(input));
}

// Full inference across steps:
const result = flow(
  '  100  ',
  (str) => parseInt(str.trim(), 10), // str is string, returns number
  (num) => num * 2                   // num is number, returns number
);

console.log(result); // 200
```
- **Output / Type Evaluation:**
```typescript
// result is inferred as number; zero manual type arguments needed.
```
- **Explanation:** Each step's output contextually types the next step's input parameter automatically.

### Example 2: Real-World Enterprise Pattern (State Machine Reducer with NoInfer)
- **Goal:** Prevent fallback values or initial states from corrupting the inferred state union.
- **Code:**
```typescript
export function createFSM<State extends string>(
  validStates: State[],
  initialState: NoInfer<State> // In TS 5.4+, NoInfer guarantees State is derived ONLY from validStates
): { getState(): State } {
  let current = initialState;
  return {
    getState: () => current,
  };
}

const fsm = createFSM(['IDLE', 'RUNNING', 'COMPLETED'], 'IDLE');

// @ts-expect-error TS2345: Argument of type '"CRASHED"' is not assignable to parameter of type '"IDLE" | "RUNNING" | "COMPLETED"'.
const badFsm = createFSM(['IDLE', 'RUNNING', 'COMPLETED'], 'CRASHED');
```
- **Output / Type Evaluation:**
```typescript
// TS2345 rejects 'CRASHED' because NoInfer stops it from widening State!
```
- **Explanation:** Without `NoInfer`, passing `'CRASHED'` would widen `State` to `'IDLE' | 'RUNNING' | 'COMPLETED' | 'CRASHED'`, defeating validation.

### Example 3: Edge Case / Boundary Condition (Callback Returning Promise in Map)
- **Goal:** Correctly unwrap async callback return types in collection transformers.
- **Code:**
```typescript
export async function asyncMap<T, R>(
  items: readonly T[],
  fn: (item: T) => Promise<R>
): Promise<R[]> {
  return Promise.all(items.map(fn));
}

const userIds = ['usr_1', 'usr_2'];
const profiles = await asyncMap(userIds, async (id) => {
  return { id, loadedAt: Date.now() };
});
// profiles is inferred as: { id: string; loadedAt: number }[]
```
- **Output / Type Evaluation:**
```typescript
// Preserves exact promise unwrapping on generic return collections.
```
- **Explanation:** The type checker resolves `Promise<R>` to extract `R` for the resulting array.

### Example 4: Negative Test / Type-Check Failure Case
- **Goal:** Demonstrate error when a generic callback violates constraint relationships.
- **Code:**
```typescript
function pluck<T, K extends keyof T>(items: T[], key: K): T[K][] {
  return items.map((item) => item[key]);
}

type Product = { id: string; price: number };
const products: Product[] = [{ id: 'p1', price: 99 }];

// @ts-expect-error TS2345: Argument of type '"discount"' is not assignable to parameter of type 'keyof Product'.
pluck(products, 'discount');
```
- **Real Compiler Error:**
```
error TS2345: Argument of type '"discount"' is not assignable to parameter of type 'keyof Product'.
```
- **Explanation:** The generic constraint `K extends keyof T` rejects keys not present on the contextually inferred `Product` type.

### Example 5: Idiomatic Modern Usage (Generic Event Listener with Discriminated Callback)
- **Goal:** Implement an event emitter where the listener callback receives the exact payload type for the event name.
- **Code:**
```typescript
type EventSchema = {
  uploadProgress: { bytesLoaded: number; totalBytes: number };
  completed: { downloadUrl: string };
  failed: { error: Error };
};

export class Uploader {
  private handlers = new Map<string, ((payload: any) => void)[]>();

  on<EventName extends keyof EventSchema>(
    event: EventName,
    listener: (payload: EventSchema[EventName]) => void
  ): void {
    if (!this.handlers.has(event)) {
      this.handlers.set(event, []);
    }
    this.handlers.get(event)!.push(listener);
  }
}

const uploader = new Uploader();
// 'payload' is automatically contextually typed as { bytesLoaded: number; totalBytes: number }!
uploader.on('uploadProgress', (payload) => {
  const percent = (payload.bytesLoaded / payload.totalBytes) * 100;
  console.log(`Progress: ${percent}%`);
});
```
- **Output / Type Evaluation:**
```typescript
// Exact payload property completion in callback without manual typing.
```
- **Explanation:** Generic parameter indexing (`EventSchema[EventName]`) dynamically creates the contextual signature for the callback.

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Inference Cycle Bailout:** If callback 1's return type depends on callback 2, and callback 2 depends on callback 1, TypeScript bails out and infers `unknown` or `any`. Structure pipelines linearly to avoid cyclic inference.
2. **Premature Type Widening:** Passing an array literal directly into a generic function may widen literal types (`"GET"` to `string`) before the callback is checked. Use `const` type parameters (`<const T>`) to preserve literal inference.
3. **NoInfer Placement:** `NoInfer<T>` only affects inference candidates; it does NOT alter assignability. The argument must still be assignable to `T`.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern (Naive / Broken) | Real Compiler Error / Bug | Idiomatic Solution | Why It Matters |
| :--- | :--- | :--- | :--- |
| Explicitly specifying redundant type arguments | Fragile refactoring; boilerplate bloat | Rely on contextual inference | Keeps code DRY and aligned with TS type checker |
| Allowing defaults to widen generic unions | Type unions polluted with invalid candidates | Use `NoInfer<T>` (TS 5.4+) | Restricts inference sources to primary arguments |
| Unconstrained generic callbacks | Callback parameter defaults to `unknown` | Add generic constraints (`<T extends object>`) | Guarantees minimum property availability |
| Cyclic dependencies in chained callbacks | Compiler bails to `any` | Break into linear typed steps | Prevents compiler inference failures |
| Forgetting to await generic async callbacks | Returns array of pending Promises | Wrap in `Promise.all()` with unwrapped type | Eliminates unresolved Promise bugs |
| Over-constraining callback return types | Rejects valid compatible returns | Keep return type generic (`=> R`) | Preserves maximum functional composability |

---

## 8. Performance & Type-Checker Complexity
- **Constraint Solver Depth:** Generic callback inference triggers constraint solving in `checker.ts`. Deeply nested pipes (10+ steps) increase type checking latency.
- **NoInfer Optimization:** `NoInfer<T>` reduces inference search space, improving compiler throughput on complex generic APIs.
- **Isolated Declarations:** Exported higher-order functions must explicitly type return types under `--isolatedDeclarations`.

---

## 9. Compiler Internals
- **AST Nodes:** Generic function signatures contain `ts.TypeParameterDeclaration` nodes.
- **Inference Context:** `createInferenceContext` tracks candidates for each type variable during call checking.
- **NoInfer Intrinsic:** Defined in `lib.es5.d.ts` as `type NoInfer<T> = [T][T extends any ? 0 : never]`, internally recognized by the checker to suppress inference candidate generation.

---

## 10. Best Practices & Design Guidelines
- **Use `NoInfer<T>` for Secondary Parameters:** Protect states, options, and fallbacks from widening primary type parameters.
- **Let Contextual Typing Work:** Avoid explicit parameter type annotations in callback arguments when assigning to generic higher-order functions.
- **Chain Generics Linearly:** Design pipelines where each step's return type feeds into the subsequent step's parameter.

---

## 11. Real-World Case Study
- **Scenario:** A query client library provided `useQuery({ queryKey, queryFn, initialData })`. If `queryFn` returned `User` and `initialData` was `null`, TypeScript inferred `User | null` as the data type, forcing every consumer to write `user?.name` checks even after the query completed.
- **Implementation:** Leveraged `NoInfer` on `initialData`:
  ```typescript
  // Before:
  function useQuery<T>(options: { queryFn: () => Promise<T>; initialData?: T }): QueryResult<T>;

  // After (TS 5.4+):
  function useQuery<T>(options: { queryFn: () => Promise<T>; initialData?: NoInfer<T> }): QueryResult<T>;
  ```
- **Outcome:** The return type `T` was locked strictly to the resolved type of `queryFn`, preventing `initialData` from degrading the query's type precision.

---

## 12. Interview Questions

### Q1: How does TypeScript 5.4 `NoInfer<T>` work and what problem does it solve?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

In generic functions with multiple arguments that reference the same type parameter `T`, TypeScript considers every argument an "inference candidate" and computes the union of all candidates. For example, in `function select<T>(items: T[], fallback: T)`, calling `select(['a', 'b'], 'c')` considers both `'a' | 'b'` and `'c'`, widening `T` to `'a' | 'b' | 'c'`. Wrapping `fallback` with `NoInfer<T>` instructs the compiler to ignore `fallback` when calculating `T`, ensuring `T` is inferred solely from `items`. `fallback` is then checked against that locked type, raising an error if it doesn't match.
</details>

### Q2: What is the difference between top-down contextual typing and bottom-up type synthesis?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

- **Bottom-up Type Synthesis:** TypeScript inspects an expression from the leaves up to determine its type independently of its surroundings (e.g. `1 + 2` evaluates to `number`).
- **Top-Down Contextual Typing:** The expected type of an expression is determined by the context in which it appears (e.g. passing a lambda to an array method). The compiler pushes the expected signature down into the lambda, typing its parameters without explicit annotations.
Higher-order generic functions combine both: bottom-up evaluation on initial arguments provides the contextual type for downstream callbacks.
</details>

### Q3: Why does TypeScript process non-function arguments before function arguments in call resolution?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Non-function arguments (like arrays, objects, primitives) have fixed or readily synthesizable types. Function arguments (callbacks), however, often rely on contextual typing to infer their parameter types. Evaluating non-function arguments first allows the compiler to resolve generic type parameters early, which can then be used to contextually type the callback parameters in a subsequent phase.
</details>

### Q4: How do `const` type parameters (TS 5.0+) benefit generic callback functions?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

When a generic parameter is declared with `const` (`<const T>`), TypeScript infers the most specific literal and readonly types possible at the call site without requiring `as const`. When passed into a callback, the callback receives exact literal unions and readonly tuples rather than widened primitives (`"admin"` instead of `string`).
</details>

### Q5: What causes an inference cycle in generic pipelines?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

An inference cycle occurs when parameter $A$ depends on type variable $X$, parameter $B$ depends on type variable $Y$, but $X$ is inferred from the return type of $B$ while $Y$ is inferred from the return type of $A$. The type checker cannot determine which type variable to solve first and bails out, assigning `unknown` or `any` to break the recursion.
</details>

### Q6: Can a generic function return a generic callback?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Yes. A higher-order function can return a callback that itself declares generic type parameters:
```typescript
function makeWrapper<M>(metadata: M): <T>(item: T) => { data: T; meta: M } {
  return (item) => ({ data: item, meta: metadata });
}
```
The returned function can be invoked with different types of `T` independently.
</details>

### Q7: Why do generic callback parameters sometimes infer `unknown` instead of their constraint?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

If the call site provides zero inference candidates for a generic parameter (for example, omitting an argument or passing an unannotated empty array), TypeScript defaults the type parameter to its declared constraint (or `unknown` if unconstrained), resulting in `unknown` parameter types in the callback.
</details>

### Q8: How can you write a generic tap function that inspects a value without altering the pipeline type?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

```typescript
function tap<T>(fn: (val: T) => void): (val: T) => T {
  return (val: T): T => {
    fn(val);
    return val;
  };
}
```
This captures `T` and guarantees that the returned function emits the identical type it received.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Generic Tap Utility (Easy)
- **Problem Statement:** Author a function `tap<T>(val: T, cb: (item: T) => void): T` that runs a side-effect callback on a value and returns the value untouched.
- **Test Case:**
```typescript
const val = tap(42, (n) => console.log(n)); // 42
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function tap<T>(val: T, cb: (item: T) => void): T {
  cb(val);
  return val;
}
```
</details>

### Exercise 2: Generic Array GroupBy (Easy)
- **Problem Statement:** Write a function `groupBy<T, K extends string | number>(items: readonly T[], keyFn: (item: T) => K): Record<K, T[]>`.
- **Test Case:**
```typescript
const grouped = groupBy(['apple', 'banana', 'avocado'], (s) => s[0]!);
// grouped: Record<string, string[]>
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function groupBy<T, K extends string | number>(
  items: readonly T[],
  keyFn: (item: T) => K
): Record<K, T[]> {
  const result = {} as Record<K, T[]>;
  for (const item of items) {
    const key = keyFn(item);
    if (!result[key]) {
      result[key] = [];
    }
    result[key].push(item);
  }
  return result;
}
```
</details>

### Exercise 3: Strict NoInfer Form Defaults (Medium)
- **Problem Statement:** Implement a function `createForm<T extends object>(initialValues: T, resetFallback: NoInfer<T>): void` that prevents `resetFallback` from widening `T`.
- **Test Case:**
```typescript
createForm({ name: 'Alice' }, { name: 'Bob' }); // OK
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function createForm<T extends object>(
  initialValues: T,
  resetFallback: NoInfer<T>
): { getValues(): T; reset(): void } {
  let values = { ...initialValues };
  return {
    getValues: () => values,
    reset: () => { values = { ...resetFallback }; }
  };
}
```
</details>

### Exercise 4: Async Pipeline Reducer (Medium)
- **Problem Statement:** Author an async pipe function `pipeAsync<T>(initial: T, ...transforms: ((val: T) => Promise<T>)[]): Promise<T>`.
- **Test Case:**
```typescript
const res = await pipeAsync(5, async (x) => x + 1, async (x) => x * 2); // 12
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
async function pipeAsync<T>(
  initial: T,
  ...transforms: ((val: T) => Promise<T>)[]
): Promise<T> {
  let current = initial;
  for (const fn of transforms) {
    current = await fn(current);
  }
  return current;
}
```
</details>

### Exercise 5: Compile-Time Pipe Type Generator (Hard)
- **Problem Statement:** Write a type-level function `ComposeTypes<Fns>` that verifies that each function in a tuple returns a type assignable to the next function's parameter.
- **Test Case:**
```typescript
type Fns1 = [(x: string) => number, (n: number) => boolean]; // Valid
type Fns2 = [(x: string) => number, (s: string) => boolean]; // Invalid
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
type ValidatePipeline<Fns extends any[]> = Fns extends [
  (arg: any) => infer R1,
  (arg: infer P2) => infer R2,
  ...infer Rest
]
  ? [R1] extends [P2]
    ? [(arg: any) => R1, ...ValidatePipeline<[(arg: P2) => R2, ...Rest]>]
    : ['Error: Pipeline stage output does not match next stage input', R1, P2]
  : Fns;
```
</details>

### Exercise 6: Curried Generic Mapper (Hard)
- **Problem Statement:** Write a fully curried `map` function `curriedMap` that takes a mapper function `(item: T) => U` and returns a function taking `readonly T[]` and returning `U[]`, preserving full generic inference.
- **Test Case:**
```typescript
const stringify = curriedMap((n: number) => n.toString());
const results = stringify([1, 2, 3]); // string[]
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function curriedMap<T, U>(
  fn: (item: T) => U
): (items: readonly T[]) => U[] {
  return (items: readonly T[]): U[] => items.map(fn);
}
```
</details>
