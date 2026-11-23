# Algorithm Complexity in Type Space: Type-Level Computation and Recursion Limits

> Exploring TypeScript's Turing-complete type system: type-level Peano arithmetic, type-level string and array sorting, tail-call optimization in conditional types, and compiler stack limit bypasses.

| Metadata | Details |
| :--- | :--- |
| **Category** | Data Structures & Algorithms |
| **Difficulty** | Advanced |
| **Prerequisites** | [07-mapped-and-conditional-types/01-conditional-types.md](../07-mapped-and-conditional-types/01-conditional-types.md), [14-compiler-internals/05-compiler-architecture.md](../14-compiler-internals/05-compiler-architecture.md) |
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

TypeScript's type system is **Turing-complete**: it can perform arbitrary computation, arithmetic, list manipulation, and sorting entirely at compile time without generating a single line of runtime JavaScript.

While building production applications does not require writing full type-level compilers, understanding **Type-Level Algorithms** is crucial for senior and staff engineers:
- **Type-Level Parsing**: Validating SQL queries, GraphQL schemas, or URL route templates (`/users/:userId/posts/:postId`) entirely in the type system.
- **Compiler Safety & Guardrails**: Avoiding compiler crashes (`TS2589: Type instantiation is excessively deep`) by mastering tail-call recursion optimization (introduced in TS 4.5).
- **Zero-Cost Verification**: Enforcing array length, matrix dimension multiplication, and state transitions at build time.

---

## 2. Core Concepts

### Type-Level Peano Arithmetic
Because TypeScript has no native type-level arithmetic operators (`+`, `-`, `*`), numbers are modeled as tuple lengths:
- $0 = `[]`$ (length 0)
- $1 = `[unknown]`$ (length 1)
- $N+1 = `[...TupleN, unknown]`$
- Addition: `[...TupleA, ...TupleB]["length"]`

```
+-------------------------------------------------------------------------+
|                  Type-Level Tuple Arithmetic Mechanics                  |
+-------------------------------------------------------------------------+
|                                                                         |
|  Add<2, 3>:                                                             |
|                                                                         |
|  BuildTuple<2> = [1, 1]                                                 |
|  BuildTuple<3> = [1, 1, 1]                                              |
|                                                                         |
|  Concatenation: [...[1, 1], ...[1, 1, 1]] = [1, 1, 1, 1, 1]            |
|                                                                         |
|  Result: [1, 1, 1, 1, 1]["length"] = 5!                                |
|                                                                         |
+-------------------------------------------------------------------------+
```

### Tail-Call Recursion Optimization (TS 4.5+)
Prior to TypeScript 4.5, recursive conditional types were limited to ~50 iterations. TypeScript 4.5 introduced tail-call elimination for conditional types that return immediately without wrapping, raising the recursion depth limit to **1,000 iterations**.

---

## 3. How It Works Internally

1. **Tuple Accumulators**: Tail-recursive conditional types store intermediate results in a tuple type parameter (`Acc extends unknown[] = []`).
2. **Conditional Type Evaluation**: When a conditional type branch is identical to another instantiation of itself, the compiler's `evaluateConditionalType` reuses the stack frame rather than recursing deeply in V8 C++ stack frames.

---

## 4. Syntax & API Reference

### Tuple Generator Helper
```typescript
export type BuildTuple<L extends number, T extends unknown[] = []> =
  T["length"] extends L ? T : BuildTuple<L, [...T, unknown]>;
```

### Type-Level Addition & Subtraction
```typescript
export type Add<A extends number, B extends number> = [
  ...BuildTuple<A>,
  ...BuildTuple<B>,
]["length"] & number;

export type Subtract<A extends number, B extends number> =
  BuildTuple<A> extends [...BuildTuple<B>, ...infer Rest]
    ? Rest["length"]
    : never;
```

---

## 5. Practical Examples

### Example 1: Route Parameter Extractor (Type-Level String Parser)
```typescript
// Extracts ':param' keys from URL route templates into a union
export type ExtractRouteParams<T extends string> =
  T extends `${string}:${infer Param}/${infer Rest}`
    ? Param | ExtractRouteParams<Rest>
    : T extends `${string}:${infer Param}`
      ? Param
      : never;

type MyRoute = "/api/v1/users/:orgId/teams/:teamId/members/:memberId";
type Params = ExtractRouteParams<MyRoute>;
// Inferred as: "orgId" | "teamId" | "memberId"
```

### Example 2: Type-Level Array Reversal with Tail-Call Optimization
```typescript
export type Reverse<T extends unknown[], Acc extends unknown[] = []> =
  T extends [infer Head, ...infer Tail]
    ? Reverse<Tail, [Head, ...Acc]>
    : Acc;

type Reversed = Reverse<[1, 2, 3, 4, 5]>;
// Inferred as: [5, 4, 3, 2, 1]
```

### Example 3: Edge Case - TypeScript 4.5 Tail-Call Recursion Depth
```typescript
// Non-tail call (crashes at depth ~50):
// type BadSum<N extends number> = N extends 0 ? 0 : Add<N, BadSum<Subtract<N, 1>>>;

// Tail-call optimized accumulator (executes up to depth 999):
export type SumTo<N extends number, Acc extends unknown[] = [], Counter extends unknown[] = []> =
  Counter["length"] extends N
    ? Acc["length"]
    : SumTo<N, [...Acc, ...Counter, unknown], [...Counter, unknown]>;

type ResultSum = SumTo<10>; // 55
```

### Example 4: Compiler Error & Recursion Limit Exceeded
```typescript
type Infinite<T> = [Infinite<T>];

// @ts-expect-error - TS2589: Type instantiation is excessively deep and possibly infinite.
type Boom = Infinite<string>;
```

### Example 5: Type-Level BubbleSort for Numeric Literal Tuples
```typescript
type GreaterThan<A extends number, B extends number> =
  A extends B
    ? false
    : BuildTuple<A> extends [...BuildTuple<B>, ...unknown[]]
      ? true
      : false;

type BubblePass<T extends number[]> =
  T extends [infer X extends number, infer Y extends number, ...infer Rest extends number[]]
    ? GreaterThan<X, Y> extends true
      ? [Y, ...BubblePass<[X, ...Rest]>]
      : [X, ...BubblePass<[Y, ...Rest]>]
    : T;

export type Sort<T extends number[], Count extends unknown[] = T> =
  Count["length"] extends 0
    ? T
    : Count extends [unknown, ...infer Rest]
      ? Sort<BubblePass<T>, Rest>
      : T;

type SortedNumbers = Sort<[5, 2, 8, 1, 4]>;
// Inferred as: [1, 2, 4, 5, 8]!
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **The 1,000-Limit Ceiling**:
   Even with tail-call optimization, TypeScript strictly terminates recursion at 1,000 iterations to protect IDEs and CI runners from freezing.
2. **Negative Numbers**:
   Tuple length arithmetic cannot represent negative numbers; `BuildTuple<-5>` loops infinitely or produces never.
3. **IDE Performance Impact**:
   Heavy type-level metaprogramming slows down Language Server autocomplete responses across the entire file.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Practice | Good Practice | Problem / Diagnostic |
| :--- | :--- | :--- | :--- |
| **Non-Tail Recursion** | Wrapping return in expression | Accumulator in parameter | `TS2589: Instantiation excessively deep` |
| **Negative Tuple Length** | `BuildTuple<-1>` | Guard with positive constraints | Infinite type evaluation |
| **Complex Metaprogramming** | 500 lines of type math in app | Keep type algebra minimal | Massive IDE latency |
| **Unconstrained Inferences** | `infer X` without `extends` | `infer X extends number` | Mismatched property access |
| **Distributive Union Leaks** | Naked type parameter condition | Wrap in tuple `[T] extends [never]` | Unintended union distribution |
| **Ignoring Cache Memoization** | Inlining identical types | Extract to named type alias | Repetitive type synthesis |

---

## 8. Performance & Type-Checker Complexity

- Simple type-level operations ($O(N)$ with $N < 20$) evaluate in under 1ms.
- Type-level BubbleSort on an array of length 10 evaluates $10 \times 10 = 100$ instantiations (~15ms).
- Exceeding depth 1,000 triggers immediate compilation error TS2589.

---

## 9. Compiler Internals

Inside `ts.TypeChecker`:
1. `instantiateType`: Creates a type instance with substituted type arguments.
2. `recursionCount`: Incremented on each nested invocation.
3. In TS 4.5+, `evaluateConditionalType` checks if the result is directly another conditional type invocation. If so, it updates parameters in-place and continues evaluation in a `while` loop rather than recursing the C++ call stack.

---

## 10. Best Practices & Design Guidelines

1. **Always Use Accumulators for Recursion**: Leverages TypeScript 4.5+ tail-call optimization.
2. **Guard Recursion Depth**: Supply a fallback depth counter to terminate gracefully.
3. **Reserve Type Math for High-Value Contracts**: Such as typed route extractors or matrix dimension verifiers.
4. **Prefer Named Type Aliases**: Helps the compiler memoize evaluation results.

---

## 11. Real-World Case Study

A high-performance web framework (similar to Hono or Elysia) needed to parse route parameters at compile time:
```typescript
app.get("/users/:userId/books/:bookId", (c) => {
  const userId = c.req.param("userId"); // Typed as string
  // c.req.param("invalid"); // Compile error!
});
```

**Implementation**:
Used type-level template literal string matching (`${string}:${infer Param}/${infer Rest}`). The framework delivers 100% compile-time route parameter verification with zero runtime schema validation overhead.

---

## 12. Interview Questions

<details>
<summary>1. How does TypeScript support tail-call optimization in conditional types?</summary>

Introduced in TypeScript 4.5, if a conditional type returns directly another conditional type without wrapping it in a tuple, union, or intersection, the compiler evaluates it in a loop rather than pushing to the call stack, increasing recursion depth limits from ~50 to ~1,000.
</details>

<details>
<summary>2. How can arithmetic be performed in TypeScript's type system?</summary>

By modeling numbers as the length of fixed-size tuple types. Addition is performed by spreading two tuples into a combined tuple and reading its `["length"]`. Subtraction is performed by pattern matching one tuple against another with `infer Rest`.
</details>

<details>
<summary>3. What causes compiler error TS2589?</summary>

TS2589 ("Type instantiation is excessively deep and possibly infinite") occurs when a generic recursive type exceeds the compiler's hard-coded recursion depth limit (typically 1,000 iterations in tail-call mode or ~50 in standard mode).
</details>

<details>
<summary>4. What is template literal type parsing?</summary>

A feature introduced in TypeScript 4.1 that allows pattern-matching on string literal types using the `infer` keyword inside template strings (e.g. `T extends `${infer Head}.${infer Tail}`? ... : ...`).
</details>

<details>
<summary>5. Why does `[T] extends [never]` prevent distributive conditional types?</summary>

By wrapping the type parameter `T` in a single-element tuple `[T]`, TypeScript disables distributive conditional type behavior across unions, allowing exact comparison against `never`.
</details>

<details>
<summary>6. Can you sort an array at compile time in TypeScript?</summary>

Yes. Using recursive conditional types and type-level comparison (tuple length checks), sorting algorithms like BubbleSort, QuickSort, or InsertionSort can be implemented purely within the type system.
</details>

<details>
<summary>7. What are the performance implications of complex type-level programming?</summary>

Complex type-level recursion increases compiler check time and memory consumption, potentially causing Language Server lag and high CI build durations.
</details>

<details>
<summary>8. How do you prevent infinite recursion in type-level algorithms?</summary>

By passing an accumulator tuple as a recursion counter and terminating when `Counter["length"]` reaches a fixed threshold.
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Challenge 1 (Easy): Type-Level String Length</summary>

**Task**: Implement `StringLength<S>` that returns the character count of a string literal.

**Solution**:
```typescript
type StringToTuple<S extends string> =
  S extends `${infer Char}${infer Rest}`
    ? [Char, ...StringToTuple<Rest>]
    : [];

export type StringLength<S extends string> = StringToTuple<S>["length"];
type L = StringLength<"TypeScript">; // 10
```
</details>

<details>
<summary>Challenge 2 (Easy): Type-Level String Join</summary>

**Task**: Implement `Join<T, Delimiter>` that joins a tuple of strings with a delimiter.

**Solution**:
```typescript
export type Join<T extends string[], D extends string> =
  T extends []
    ? ""
    : T extends [infer Single extends string]
      ? Single
      : T extends [infer Head extends string, ...infer Tail extends string[]]
        ? `${Head}${D}${Join<Tail, D>}`
        : string;

type Joined = Join<["a", "b", "c"], "-">; // "a-b-c"
```
</details>

<details>
<summary>Challenge 3 (Medium): Type-Level Fibonacci Sequence</summary>

**Task**: Implement `Fibonacci<N>` returning the $N$th Fibonacci number at compile time.

**Solution**:
```typescript
import { BuildTuple, Add } from "./arithmetic.js";

export type Fibonacci<
  N extends number,
  Counter extends unknown[] = [unknown],
  Prev extends number = 0,
  Curr extends number = 1
> = Counter["length"] extends N
  ? Curr
  : Fibonacci<N, [...Counter, unknown], Curr, Add<Prev, Curr>>;

type Fib6 = Fibonacci<6>; // 8
```
</details>

<details>
<summary>Challenge 4 (Medium): Deep Object Path Getter (Type-Level Lodash `get`)</summary>

**Task**: Implement `Path<T, P>` that resolves dotted property paths like `"user.profile.age"`.

**Solution**:
```typescript
export type Path<T, P extends string> =
  P extends `${infer Key}.${infer Rest}`
    ? Key extends keyof T
      ? Path<T[Key], Rest>
      : never
    : P extends keyof T
      ? T[P]
      : never;

interface AppState {
  user: { profile: { email: string } };
}

type UserEmail = Path<AppState, "user.profile.email">; // string
```
</details>

<details>
<summary>Challenge 5 (Hard): Type-Level SQL SELECT Column Validator</summary>

**Task**: Given a schema record and a SQL string like `"SELECT id, email FROM users"`, extract the selected column types into an interface.

**Solution**:
```typescript
type Trim<S extends string> =
  S extends ` ${infer Rest}` ? Trim<Rest> : S extends `${infer Rest} ` ? Trim<Rest> : S;

type Split<S extends string, D extends string> =
  S extends `${infer Head}${D}${infer Tail}`
    ? [Trim<Head>, ...Split<Tail, D>]
    : [Trim<S>];

export type ParseSelect<SQL extends string, Schema extends Record<string, any>> =
  SQL extends `SELECT ${infer Cols} FROM ${string}`
    ? { [K in Split<Cols, ",">[number] & keyof Schema]: Schema[K] }
    : never;

interface UserTable {
  id: number;
  email: string;
  secret: string;
}

type QueryResult = ParseSelect<"SELECT id, email FROM users", UserTable>;
// Inferred as: { id: number; email: string; }
```
</details>

<details>
<summary>Challenge 6 (Hard): Type-Level JSON Parser</summary>

**Task**: Implement a minimal type-level JSON parser that parses string representations of boolean, null, and numbers into their literal types.

**Solution**:
```typescript
export type ParseJsonPrimitive<T extends string> =
  T extends "true"
    ? true
    : T extends "false"
      ? false
      : T extends "null"
        ? null
        : T extends `${infer N extends number}`
          ? N
          : T;

type B = ParseJsonPrimitive<"true">; // true
type N = ParseJsonPrimitive<"42">;   // 42
type S = ParseJsonPrimitive<"hello">;// "hello"
```
</details>
