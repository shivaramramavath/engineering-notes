# Type-Level Programming

> TypeScript's type system is a pure, functional, Turing-complete programming language executed entirely at compile time, enabling boolean logic, pattern matching, and complex data structures in types.

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [00-conditional-types.md](file:///E:/Learning/engineering-notes/01-languages/typescript/10-advanced-types/00-conditional-types.md), [02-infer.md](file:///E:/Learning/engineering-notes/01-languages/typescript/10-advanced-types/02-infer.md) |
| **Time to Read** | 26 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 4.1 / TS 5.x / TS 7.0 Native |
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
In most statically typed languages (Java, C++, Rust), types describe data layout and constraints. In TypeScript, the type system is also a **pure functional programming language**:
- It has variables (type parameters `<T>`).
- It has conditional branching (`T extends U ? X : Y`).
- It has pattern matching and destructuring (`infer`).
- It has looping and recursion (recursive type aliases).
- It has lists and arrays (tuples).
- It has string manipulation (template literal types).

In 2016, researchers proved that TypeScript's type system is **Turing complete**. This means any computable algorithm (from simulating a Turing machine to parsing complex string DSLs) can be executed entirely during compilation.

---

## 2. Core Concepts

### Equivalence to Functional Programming
| Runtime JavaScript | Type-Space TypeScript |
| :--- | :--- |
| Variable: `const x = 5;` | Type Alias: `type X = 5;` |
| Function: `function f(x) { return x; }` | Generic Type: `type F<X> = X;` |
| If/Else: `cond ? a : b` | Conditional Type: `T extends U ? A : B` |
| Array: `[1, 2, 3]` | Tuple Type: `[1, 2, 3]` |
| Pattern Matching: `const [head, ...tail] = arr` | Infer Matching: `T extends [infer H, ...infer Tail]` |
| Recursion: `function loop(n) { ... }` | Recursive Type: `type Loop<N> = ...` |

### Type-Level Boolean Logic
```typescript
type And<A extends boolean, B extends boolean> =
  A extends true ? (B extends true ? true : false) : false;

type Or<A extends boolean, B extends boolean> =
  A extends true ? true : (B extends true ? true : false);

type Not<A extends boolean> = A extends true ? false : true;

type T1 = And<true, false>; // false
type T2 = Or<true, false>;  // true
type T3 = Not<true>;        // false
```

---

## 3. How It Works Internally

```
Type Program Execution Flow:
           Input Type Args
                 |
                 v
      Generic Type Application
                 |
                 v
      AST Traversal in checker.ts
                 |
        Pattern Match via 'infer'
                 |
        Conditional Evaluation
                 |
         Tail-Call Loop-Back
                 |
                 v
          Resulting Type Interned
```

When TypeScript evaluates a type-level program, the compiler does not run JavaScript bytecode. Instead, the type checker (`checker.ts`) evaluates type expressions by transforming AST nodes, tracking recursion depth with internal stack counters, and memoizing identical types in an internal symbol cache.

---

## 4. Syntax & API Reference

### Accumulator Pattern (Tail-Call Optimization)
Always pass intermediate results in an accumulator parameter (`Acc`):
```typescript
type Range<N extends number, Acc extends number[] = []> =
  Acc["length"] extends N
    ? Acc
    : Range<N, [...Acc, Acc["length"]]>;

type ZeroToFour = Range<5>; // [0, 1, 2, 3, 4]
```

---

## 5. Practical Examples

### 5.1 Type-Level String Reversal
```typescript
type ReverseString<S extends string, Acc extends string = ""> =
  S extends `${infer Head}${infer Tail}`
    ? ReverseString<Tail, `${Head}${Acc}`>
    : Acc;

type Reversed = ReverseString<"TypeScript">; // "tpircSepyT"
```

### 5.2 Real-World Application: Type-Safe JSON Schema to TypeScript Compiler
```typescript
type FromSchema<T> =
  T extends { type: "string" } ? string :
  T extends { type: "number" } ? number :
  T extends { type: "boolean" } ? boolean :
  T extends { type: "object"; properties: infer P }
    ? { [K in keyof P]: FromSchema<P[K]> }
    : unknown;

const userSchema = {
  type: "object",
  properties: {
    name: { type: "string" },
    age: { type: "number" },
  },
} as const;

type User = FromSchema<typeof userSchema>;
// { name: string; age: number }
```

### 5.3 Edge Case: Tuple Index Equality
```typescript
type Includes<T extends readonly unknown[], Target> =
  T extends readonly [infer Head, ...infer Tail]
    ? [Head, Target] extends [Target, Head]
      ? true
      : Includes<Tail, Target>
    : false;

type HasTwo = Includes<[1, 2, 3], 2>; // true
type HasFour = Includes<[1, 2, 3], 4>; // false
```

### 5.4 Failure Case: Unbounded Number Increment
```typescript
// @ts-expect-error TS2589: Type instantiation is excessively deep and possibly infinite.
type BadLoop<N extends number, Acc extends any[] = []> =
  Acc["length"] extends N ? Acc : BadLoop<N, [...Acc, 1]>;

type Fail = BadLoop<2000>;
```

### 5.5 Idiomatic Pattern: Type-Level Map
```typescript
type MapTuple<T extends readonly unknown[], F extends (arg: any) => any> = {
  [K in keyof T]: T[K] extends Parameters<F>[0] ? ReturnType<F> : never;
};
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Compilation Latency**: Type-level algorithms run in the main thread of the compiler. Overly complex type computations will cause visible latency in VS Code IntelliSense.
2. **Instantiation Limits**: The compiler caps tail-recursive calls at ~1,000 steps and non-tail calls at ~50 steps.
3. **No Floating Point**: Type-level math only operates on discrete integer counts represented as tuple lengths.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Problem | Solution | Compiler Error |
| :--- | :--- | :--- | :--- |
| Implementing complex business logic in types | Slows down IDE, hard to maintain and debug | Keep business logic at runtime | Extreme IDE lag / slow `tsc` |
| Non-tail recursive type algorithms | Hits recursion cap after 50 iterations | Use accumulator parameter in tail position | `TS2589: Type instantiation is excessively deep` |
| Creating large object intersections in loop | $O(N^2)$ memory explosion | Use mapped types instead of `& { [K]: V }` | Out of memory crash |
| Distributing over multiple unions inside loops | Combinatorial explosion ($O(N^K)$) | Wrap operands in tuples to disable distribution | Slow compiler check times |
| Simulating floating-point math in types | Extremely fragile and slow | Use string patterns or runtime validation | Instantiation limit error |
| Omitting terminating base case | Infinite compiler loop | Ensure base case returns non-recursive type | Compiler crash |

---

## 8. Performance & Type-Checker Complexity
- Use `tsc --extendedDiagnostics --noEmit` to monitor total instantiation counts.
- Keep total instantiations in application code below 500,000.

---

## 9. Compiler Internals
- `src/compiler/checker.ts` evaluates generic types via `instantiateType`.
- TypeScript memoizes previously evaluated generic types in `type.mapper` caches to avoid re-evaluating identical type trees.

---

## 10. Best Practices & Design Guidelines
- Reserve type-level programming for library boundaries (routing, validation, ORMs, RPC clients).
- Keep application code types simple and straightforward.
- Add regression tests with `expectTypeOf` or `tsd` to verify that type computations remain stable across TypeScript upgrades.

---

## 11. Real-World Case Study
### Compile-Time SQL Query Validation in Kysely / Prisma
Modern TypeScript query builders (like Kysely) parse SQL table schemas at compile time:
```typescript
type SelectQuery<TableSchema, SelectedColumns extends keyof TableSchema> = {
  execute(): Promise<Array<Pick<TableSchema, SelectedColumns>>>;
};
```
By evaluating table keys and columns during compilation, syntax typos in SQL column names are caught before code ever runs in test environments.

---

## 12. Interview Questions

<details>
<summary>1. What does it mean that TypeScript's type system is Turing complete?</summary>

It means the type system can simulate any computation that a universal Turing machine can perform. It possesses conditional branching, pattern matching, variable substitution, and recursion, allowing arbitrary algorithms to execute at compile time.
</details>

<details>
<summary>2. How can numbers be represented and incremented in type space?</summary>

Using the length of tuple types (Peano arithmetic): an array of length $N$ represents integer $N$. Appending an element (`[...Tuple, any]`) increments the number to $N + 1$.
</details>

<details>
<summary>3. What is the Accumulator pattern in type-level programming?</summary>

Passing an auxiliary generic parameter (e.g. `Acc extends unknown[] = []`) that accumulates results across recursive calls, ensuring the recursive type invocation remains in tail position for compiler optimization.
</details>

<details>
<summary>4. How does TypeScript optimize tail-recursive conditional types?</summary>

Since TS 4.5, if the recursive call is directly returned in the true/false branch without intermediate wrapping, the compiler reuses the evaluation stack frame, raising the recursion limit from ~50 to ~1,000 steps.
</details>

<details>
<summary>5. What tools can measure the performance cost of type-level computations?</summary>

`tsc --extendedDiagnostics` (measures total check time and instantiation count) and `tsc --generateTrace <dir>` (produces Chrome DevTools traces).
</details>

<details>
<summary>6. How can you implement boolean AND in types?</summary>

```typescript
type And<A extends boolean, B extends boolean> = A extends true ? (B extends true ? true : false) : false;
```
</details>

<details>
<summary>7. What is Church encoding in type systems?</summary>

Representing data and operators purely through functions/types (e.g. booleans represented as functions choosing between two branches).
</details>

<details>
<summary>8. Why should heavy type-level programming be avoided in general application code?</summary>

Because type-level computations run on the compiler thread, excessively complex types cause high memory consumption, slow CI builds, and significant latency in IDE autocompletion.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Type-Level Boolean NOT (Easy)
Implement `Not<T>` for boolean literal types.
<details>
<summary>Solution</summary>

```typescript
type Not<T extends boolean> = T extends true ? false : true;
type N1 = Not<true>;  // false
type N2 = Not<false>; // true
```
</details>

### Exercise 2: String Length via Tuples (Easy)
Calculate the character length of a string literal type at compile time.
<details>
<summary>Solution</summary>

```typescript
type StringLength<S extends string, Acc extends any[] = []> =
  S extends `${infer _}${infer Tail}`
    ? StringLength<Tail, [...Acc, any]>
    : Acc["length"];

type Len = StringLength<"hello">; // 5
```
</details>

### Exercise 3: Type-Level Array Flatten (Medium)
Recursively flatten an arbitrarily nested array into a 1D array type.
<details>
<summary>Solution</summary>

```typescript
type Flatten<T extends readonly unknown[]> =
  T extends readonly [infer Head, ...infer Tail]
    ? Head extends readonly unknown[]
      ? [...Flatten<Head>, ...Flatten<Tail>]
      : [Head, ...Flatten<Tail>]
    : [];

type Flat = Flatten<[1, [2, [3, 4]], 5]>; // [1, 2, 3, 4, 5]
```
</details>

### Exercise 4: Tuple Slicer (Medium)
Extract a sub-slice of a tuple from index 0 to length $N$.
<details>
<summary>Solution</summary>

```typescript
type Take<T extends readonly unknown[], N extends number, Acc extends unknown[] = []> =
  Acc["length"] extends N
    ? Acc
    : T extends readonly [infer Head, ...infer Tail]
    ? Take<Tail, N, [...Acc, Head]>
    : Acc;

type FirstTwo = Take<["a", "b", "c", "d"], 2>; // ["a", "b"]
```
</details>

### Exercise 5: Compile-Time JSON String Value Type Extractor (Hard)
Parse a simple JSON string `'{"name": "Alice"}'` and extract the value of the "name" key.
<details>
<summary>Solution</summary>

```typescript
type ExtractJsonStringProp<S extends string, Key extends string> =
  S extends `${string}"${Key}": "${infer Value}"${string}` ? Value : never;

type Name = ExtractJsonStringProp<'{"id": 1, "name": "Alice", "role": "admin"}', "name">;
// "Alice"
```
</details>

### Exercise 6: Fibonacci Sequence in Types (Hard)
Implement `Fibonacci<N>` where $N le 15$ using tuple length arithmetic.
<details>
<summary>Solution</summary>

```typescript
type Fib<N extends number, Current extends any[] = [any], Next extends any[] = [any], Count extends any[] = [any]> =
  Count["length"] extends N
    ? Current["length"]
    : Fib<N, Next, [...Current, ...Next], [...Count, any]>;

type Fib0 = Fib<1>; // 1
type Fib5 = Fib<5>; // 5
type Fib7 = Fib<7>; // 13
```
</details>
