# Never and Void

> `never` represents the uninhabitable bottom type for impossible values and exhaustive compile-time verification, whereas `void` designates the absence of an observable return value while safely permitting callback substitutability.

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [00-types-and-inference.md](file:///E:/Learning/engineering-notes/01-languages/typescript/01-fundamentals/00-types-and-inference.md), [06-any-and-unknown.md](file:///E:/Learning/engineering-notes/01-languages/typescript/01-fundamentals/06-any-and-unknown.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 2.0 (`never`) / TS 5.x / TS 7.0 Native |
| **Tested tsconfig** | `strict: true`, `noImplicitReturns: true`, `exactOptionalPropertyTypes: true` |

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
- **Problem Statement:** In strongly typed systems, two distinct concepts frequently get conflated: (1) functions that return normally without producing a useful value, and (2) computations that never return at all (infinite loops, thrown exceptions, or unreachable code branches).
- **Historical Context:** In early TypeScript, functions that threw exceptions or executed infinite loops were typed as returning `void`. TypeScript 2.0 introduced `never` as the formal bottom type of the type lattice ($ot$).
- **Real-World Relevance:** `never` powers exhaustive pattern matching in discriminated unions, dead code detection, and type-level filtering in conditional types (`Exclude<T, U>`). `void` ensures that functions can discard results safely while allowing high-order callback functions to accept procedures that return arbitrary values without type mismatch.

---

## 2. Core Concepts
- **Formal Invariants:**
  - `never` is the bottom type: it has zero elements in its domain ($emptyset$). No value (not even `any`) is assignable to `never`, but `never` is assignable to every type.
  - `void` represents the absence of a return value. A function returning `void` returns `undefined` at JavaScript runtime.
  - Callback substitutability: A function returning a value (e.g. `() => number`) is assignable to a callback slot expecting `() => void`.
- **Mental Model:**
  - `never`: "This execution branch can NEVER complete or produce a value."
  - `void`: "This caller DOES NOT CARE about the return value; ignore it."
- **Soundness vs Ergonomics:** Allowing `() => number` to be passed where `() => void` is expected seems unsound at first glance, but is essential for idiomatic JavaScript (e.g. `[1, 2].forEach(x => list.push(x))`, where `Array.prototype.push` returns the new length, but `forEach` ignores it).

---

## 3. How It Works Internally
- **Control Flow Analysis (CFA):** The compiler tracks code reachability. If all branches of a function terminate with `throw` or an infinite loop, the inferred return type is `never`.
- **Union Distribution Elimination:** When `never` is part of a union (`string | never`), the compiler removes `never` because the union with the empty set leaves the domain unchanged: $T cup emptyset = T$.
- **Architectural Flow Diagram:**

```
+--------------------------------------------------------------------------+
|                       Void vs Never Decision Tree                        |
|                                                                          |
|                     Does the function return control?                    |
|                              /             \                            |
|                            YES              NO                           |
|                            /                 \                          |
|             Does it yield a value?       Why does it not return?         |
|              /               \             /               \           |
|            YES                NO         Throws          Infinite Loop   |
|            /                   \          /                  \          |
|    Return Type: T          Return: void  Return: never      Return: never|
|    (e.g. string)          (yields undef) (unreachable)     (unreachable) |
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & API Reference
- **Return Type Annotations:**
  ```typescript
  function logMessage(msg: string): void {
    console.log(msg);
  }

  function fail(message: string): never {
    throw new Error(message);
  }
  ```
- **Union and Intersection Identities:**
  - `T | never` $equiv$ `T`
  - `T & never` $equiv$ `never`
  - `T | void` $equiv$ `T | void`
  - `T & void` $equiv$ `never` (for primitive types incompatible with undefined)
- **tsconfig Dependencies:**
  - `noImplicitReturns: true`: Ensures all code branches explicitly return a value if any branch does.
  - `allowUnreachableCode: false`: Flags statements that occur after a `never`-returning call.

---

## 5. Practical Examples

### Example 1: Basic / Foundational Usage
- **Goal:** Contrast `void` and `never` in control flow.
- **Code:**
```typescript
function notifyUser(name: string): void {
  console.log(`Hello, ${name}`);
  // Implicitly returns undefined
}

function terminateProcess(code: number): never {
  process.exit(code);
}

const resVoid: void = notifyUser('Alice');
// @ts-expect-error TS2322: Type 'void' is not assignable to type 'never'.
const resNever: never = notifyUser('Alice');
```
- **Output / Type Evaluation:**
```typescript
// resVoid is undefined at runtime; never cannot be assigned anything.
```
- **Explanation:** `void` yields an actual runtime return of `undefined`, whereas `never` never completes.

### Example 2: Real-World Enterprise Pattern (Exhaustive Pattern Matching)
- **Goal:** Guarantee compile-time exhaustiveness across a discriminated union.
- **Code:**
```typescript
type NetworkState =
  | { state: 'idle' }
  | { state: 'loading' }
  | { state: 'success'; data: string[] }
  | { state: 'error'; error: Error };

export function assertNever(x: never, message: string = 'Unhandled discriminant'): never {
  throw new Error(`${message}: ${JSON.stringify(x)}`);
}

export function renderState(s: NetworkState): string {
  switch (s.state) {
    case 'idle':
      return 'Ready';
    case 'loading':
      return 'Fetching data...';
    case 'success':
      return `Loaded ${s.data.length} items`;
    case 'error':
      return `Failed: ${s.error.message}`;
    default:
      // If a new state (e.g. 'paused') is added to NetworkState,
      // the compiler produces a compile-time error right here!
      return assertNever(s);
  }
}
```
- **Output / Type Evaluation:**
```typescript
// Guarantees all variants of NetworkState are handled at compile time.
```
- **Explanation:** In the `default` branch, all members of `NetworkState` have been eliminated by CFA. `s` is narrowed to `never`. If an unhandled variant exists, `s` has that variant's type, causing `assertNever(s)` to fail type checking.

### Example 3: Edge Case / Boundary Condition (Callback Substitutability of `void`)
- **Goal:** Understand why a function returning a number can satisfy a `void` callback contract.
- **Code:**
```typescript
type VoidCallback = () => void;

function registerListener(cb: VoidCallback): void {
  cb();
}

const numbers = [1, 2, 3];
// Array.prototype.push returns number (the new length)
// But VoidCallback permits it!
registerListener(() => numbers.push(4));

// However, direct invocation of a void-returning typed variable cannot be assigned:
const fn: VoidCallback = () => 42;
const result = fn(); // result is typed as void

// @ts-expect-error TS2322: Type 'void' is not assignable to type 'number'.
const num: number = result;
```
- **Output / Type Evaluation:**
```typescript
// Type 'void' cannot be consumed as a value, but implementation return values are ignored safely.
```
- **Explanation:** In TypeScript, a `void` return in a function type declaration means: "The caller will not inspect the return value." It does not mean "The callee must return undefined."

### Example 4: Negative Test / Type-Check Failure Case
- **Goal:** Demonstrate compiler error on missing exhaustiveness or unreachable code.
- **Code:**
```typescript
type Action = { type: 'INCREMENT' } | { type: 'DECREMENT' } | { type: 'RESET' };

function reduce(action: Action): number {
  switch (action.type) {
    case 'INCREMENT':
      return 1;
    case 'DECREMENT':
      return -1;
    // Missing 'RESET' case!
  }
  // @ts-expect-error TS2366: Function lacks ending return statement and return type does not include 'undefined'.
}
```
- **Real Compiler Error:**
```
error TS2366: Function lacks ending return statement and return type does not include 'undefined'.
```
- **Explanation:** With `noImplicitReturns: true`, TypeScript detects that the missing case branch falls through to the end of the function without returning a `number`.

### Example 5: Idiomatic Modern Usage (Type-Level Filter using `never`)
- **Goal:** Implement a utility type that filters out non-function properties from an interface using `never`.
- **Code:**
```typescript
type FunctionKeys<T> = {
  [K in keyof T]: T[K] extends (...args: any[]) => any ? K : never;
}[keyof T];

type OnlyMethods<T> = Pick<T, FunctionKeys<T>>;

type ServiceContract = {
  name: string;
  version: number;
  connect(): Promise<void>;
  disconnect(): Promise<void>;
  ping(): boolean;
};

type Methods = OnlyMethods<ServiceContract>;
// Evaluates to:
// { connect(): Promise<void>; disconnect(): Promise<void>; ping(): boolean }
```
- **Output / Type Evaluation:**
```typescript
// Strips 'name' and 'version' properties completely.
```
- **Explanation:** In indexed access `{ ... }[keyof T]`, mapping non-matching properties to `never` eliminates them from the resulting union: `"connect" | "disconnect" | "ping" | never` simplifies to `"connect" | "disconnect" | "ping"`.

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Conditional Types Distributing Over `never`:** In naked generic conditional types `T extends X ? A : B`, passing `never` as `T` evaluates to `never` immediately without checking either branch! To test for `never`, wrap `T` in a tuple: `[T] extends [never] ? true : false`.
2. **`void` vs `undefined` as Return Type:** A function typed with `() => undefined` MUST explicitly return `undefined` (or `return;`). A function typed with `() => void` can return anything (or nothing), but its returned value is treated as useless by callers.
3. **Dead Code Warnings:** Under `allowUnreachableCode: false`, any statements placed after a call to a function with return type `never` will trigger compiler errors.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern (Naive / Broken) | Real Compiler Error / Bug | Idiomatic Solution | Why It Matters |
| :--- | :--- | :--- | :--- |
| Omitting default branch in switch | Unhandled runtime state crashes silently | `default: assertNever(val)` | Guarantees compile-time exhaustiveness checking |
| Typing infinite loops or crash handlers as `void` | Misleading call-site control flow analysis | Type as `never` | Allows compiler to mark subsequent statements as unreachable |
| `T extends never ? true : false` | Silently evaluates to `never` instead of boolean | `[T] extends [never] ? true : false` | Prevents broken conditional type evaluations |
| Typing callbacks as `() => undefined` | Incompatible with standard library callbacks | Type as `() => void` | Preserves callback substitutability |
| Assigning `undefined` to `never` | `TS2322: Type 'undefined' is not assignable to 'never'` | Do not initialize `never` variables | `never` cannot hold any runtime values |
| Using `void` as a variable type | Variable can only hold `undefined` | Use `undefined` directly | Avoids semantic confusion |

---

## 8. Performance & Type-Checker Complexity
- **Union Simplification:** The type checker collapses `never | T` to `T` in $O(1)$ operations during union normalization.
- **Empty Set Intersection:** Any intersection `never & T` instantly resolves to `never`, terminating branch exploration early.
- **Isolated Declarations:** Both `void` and `never` are primitive type keywords supported natively by `--isolatedDeclarations` with zero performance overhead.

---

## 9. Compiler Internals
- **AST Node Representation:**
  - `void` is parsed as `ts.KeywordTypeNode` with `SyntaxKind.VoidKeyword`.
  - `never` is parsed as `ts.KeywordTypeNode` with `SyntaxKind.NeverKeyword`.
- **Type Flags:**
  - `TypeFlags.Void` (bitmask flag for void types).
  - `TypeFlags.Never` (bitmask flag for bottom types).
- **Assignability Checking:** In `src/compiler/checker.ts`, the function `isTypeRelatedTo` checks:
  ```typescript
  if (source.flags & TypeFlags.Never) {
      return true; // never is assignable to everything!
  }
  if (target.flags & TypeFlags.Never) {
      return false; // nothing is assignable to never!
  }
  ```

---

## 10. Best Practices & Design Guidelines
- **Always Use `assertNever` in Exhaustive Switches:** Protect every discriminated union evaluation against future additions.
- **Use `void` for Callback Return Types:** When designing interfaces accepting event listeners or callbacks, annotate return types with `void` to give implementers maximum flexibility.
- **Annotate Custom Throw Utilities as `never`:** If writing a custom `panic(msg: string)` or `raise(err: Error)` helper, explicitly declare the return type as `never`.

---

## 11. Real-World Case Study
- **Scenario:** A core payment routing engine handled financial transactions across 4 states: `Pending`, `Settled`, `Declined`, `Cancelled`. A new engineer introduced `Disputed` to the union without updating the downstream ledger settlement function. The missing case fell through silently, leaving $1.4M in disputed payments unrecorded in financial ledgers.
- **Implementation:** Added compile-time exhaustiveness checking with `assertNever`:
  ```typescript
  function settleTransaction(tx: Transaction): void {
    switch (tx.status) {
      case 'Pending': handlePending(tx); break;
      case 'Settled': handleSettled(tx); break;
      case 'Declined': handleDeclined(tx); break;
      case 'Cancelled': handleCancelled(tx); break;
      case 'Disputed': handleDisputed(tx); break;
      default:
        assertNever(tx.status, 'Unhandled transaction status in ledger settlement');
    }
  }
  ```
- **Outcome:** The TypeScript compiler now prevents build completion if any new transaction state is added without an explicit handler, completely eliminating unhandled state bugs.

---

## 12. Interview Questions

### Q1: Why does `type Test<T> = T extends never ? true : false` evaluate to `never` when `T` is `never`?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

In TypeScript, conditional types distribute over union types when the generic parameter is "naked" (not wrapped in a tuple or object). `never` is represented as an empty union of zero types ($emptyset$). When distributing over an empty union, there are zero iterations to perform, so the entire conditional expression evaluates to `never`. To prevent distribution and compare against `never` directly, wrap both sides in tuples: `type Test<T> = [T] extends [never] ? true : false`.
</details>

### Q2: What is callback substitutability and why does TypeScript allow a function returning `number` to be assigned to a `() => void` parameter?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Callback substitutability is an ergonomic design choice in TypeScript. When a callback signature declares a return type of `void`, it promises that the caller will ignore any returned value. In JavaScript, functions like `Array.prototype.push` return values (`number`), yet are commonly executed for side effects in `forEach` or event listeners. Requiring exact `undefined` return types would force developers to write tedious wrapper functions: `arr.forEach(x => { list.push(x); return undefined; })`.
</details>

### Q3: How is `never` used to enforce exhaustive checks in discriminated unions?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

In each `case` branch of a switch statement, TypeScript's Control Flow Analysis (CFA) narrows the union type by eliminating the matched variant. If all variants are handled, the variable in the `default` branch is narrowed to `never`. Passing this variable to a helper function `function assertNever(x: never): never` compiles cleanly. If a new variant is later added to the union, it falls into the `default` branch, and because that variant is not assignable to `never`, the compiler raises a type error.
</details>

### Q4: What is the type-theoretic difference between `any`, `unknown`, and `never`?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

- `unknown` is the top type ($	op$): every type is a subtype of `unknown`. No values can be operated on without narrowing.
- `never` is the bottom type ($ot$): `never` is a subtype of every type. It has zero values and cannot be assigned to.
- `any` breaks the type lattice: it acts simultaneously as a top type (all types assignable to it) and a bottom type (it is assignable to all types except `never`), bypassing type safety entirely.
</details>

### Q5: Can a variable ever hold the value `never` at runtime?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

No. By definition, the type `never` has no runtime representation and no inhabitants. A variable of type `never` cannot be instantiated or assigned a value unless unsafe type assertions (`as never`) or `any` are used to circumvent the compiler.
</details>

### Q6: What does `undefined` vs `void` mean when used as function parameter types?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

- If a parameter is typed as `x: undefined`, callers MUST explicitly pass `undefined` as an argument: `fn(undefined)`.
- If a parameter is typed as `x: void`, callers can pass `undefined` or omit the parameter in certain contexts, though typing parameters as `void` is an anti-pattern. Parameters should use optional syntax (`x?: string`) or `x: undefined`.
</details>

### Q7: Why does `Exclude<string | number | boolean, boolean>` utilize `never`?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

The definition of `Exclude<T, U>` is:
```typescript
type Exclude<T, U> = T extends U ? never : T;
```
It distributes over the union. When `T` is `boolean`, `boolean extends boolean` evaluates to `never`. The resulting union is `string | number | never`. Because `never` is the identity element of unions ($T cup emptyset = T$), the compiler automatically drops `never`, leaving `string | number`.
</details>

### Q8: What is the effect of `noImplicitReturns` on functions returning `void`?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

`noImplicitReturns` only affects functions with a non-void return type. Functions returning `void` are permitted to finish execution without an explicit `return` statement or return implicitly at the end of the block.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Safe Exhaustive Handler (Easy)
- **Problem Statement:** Implement a utility function `unreachable(val: never): never` that can be used in the default branch of a switch statement.
- **Test Case:**
```typescript
type Direction = 'UP' | 'DOWN';
function move(d: Direction) {
  switch (d) {
    case 'UP': return 1;
    case 'DOWN': return -1;
    default: return unreachable(d);
  }
}
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function unreachable(val: never): never {
  throw new Error(`Unreachable execution path reached with value: ${JSON.stringify(val)}`);
}
```
</details>

### Exercise 2: Strict IsNever Checker (Easy)
- **Problem Statement:** Write a type utility `IsNever<T>` that evaluates to `true` if `T` is `never` and `false` for any other type.
- **Test Case:**
```typescript
type A = IsNever<never>; // true
type B = IsNever<string>; // false
type C = IsNever<any>; // false
type D = IsNever<unknown>; // false
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
type IsNever<T> = [T] extends [never] ? true : false;
```
</details>

### Exercise 3: Non-Never Keys Extractor (Medium)
- **Problem Statement:** Write a utility type `NonNeverKeys<T>` that returns a union of all keys whose values are NOT `never`.
- **Test Case:**
```typescript
type Target = { a: string; b: never; c: number; d: never };
type Result = NonNeverKeys<Target>; // "a" | "c"
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
type NonNeverKeys<T> = {
  [K in keyof T]: [T[K]] extends [never] ? never : K;
}[keyof T];
```
</details>

### Exercise 4: Void Callback Invoker (Medium)
- **Problem Statement:** Author a function `executeIgnoringReturn` that accepts an arbitrary function returning any type, invokes it, and guarantees that the return value is sanitized to `void` (`undefined`).
- **Test Case:**
```typescript
const add = (a: number, b: number) => a + b;
const result = executeIgnoringReturn(() => add(2, 3));
// result is undefined, return type is void
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function executeIgnoringReturn(fn: () => unknown): void {
  fn();
}
```
</details>

### Exercise 5: Compile-Time Union Intersection Reducer (Hard)
- **Problem Statement:** Write a conditional type `NonEmptyIntersection<A, B>` that evaluates to `A & B` if they overlap structurally, or `never` if their intersection yields empty fields.
- **Test Case:**
```typescript
type T1 = NonEmptyIntersection<{ a: string }, { a: number }>; // never
type T2 = NonEmptyIntersection<{ a: string }, { b: number }>; // { a: string } & { b: number }
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
type NonEmptyIntersection<A, B> = {
  [K in keyof A & keyof B]: [A[K] & B[K]] extends [never] ? true : false;
}[keyof A & keyof B] extends false | never
  ? A & B
  : never;
```
</details>

### Exercise 6: Exhaustive Discriminated Union Validator Type (Hard)
- **Problem Statement:** Create a type-level function `ValidateHandlers<U, Discriminant extends string, Handlers>` that ensures every variant of union `U` is represented as a key in `Handlers` without excess keys.
- **Test Case:**
```typescript
type Events = { type: 'A'; val: number } | { type: 'B'; str: string };
// Validate that an object covers precisely 'A' and 'B'
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
type ValidateHandlers<U extends Record<D, string>, D extends string, H> =
  keyof H extends U[D]
    ? U[D] extends keyof H
      ? H
      : `Error: Missing handler for ${Exclude<U[D], keyof H>}`
    : `Error: Excess handler detected`;
```
</details>
