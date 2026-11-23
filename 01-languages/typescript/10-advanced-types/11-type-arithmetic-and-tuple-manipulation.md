# Type Arithmetic and Tuple Manipulation

> Simulating arithmetic, numerical comparison, and advanced tuple operations at compile time using tuple length manipulation and Peano numbers.

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [06-variadic-tuple-types.md](file:///E:/Learning/engineering-notes/01-languages/typescript/10-advanced-types/06-variadic-tuple-types.md), [08-type-level-programming.md](file:///E:/Learning/engineering-notes/01-languages/typescript/10-advanced-types/08-type-level-programming.md) |
| **Time to Read** | 24 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 4.0 / TS 5.x / TS 7.0 Native |
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
TypeScript does not have built-in arithmetic operators in type space: you cannot write `type Add = 5 + 3;`. In type annotations, numbers are static literal values (`5`, `3`).

However, many advanced typing scenarios demand arithmetic logic:
- Bounding recursion counters (`Depth - 1`).
- Calculating fixed array/buffer offsets.
- Checking tuple lengths and validating matrix dimensions.
- Enforcing range constraints (`0 <= Priority <= 5`).

By using **Tuple Length Arithmetic** (Peano number simulation), TypeScript developers can perform addition, subtraction, comparison, and range generation entirely at compile time.

---

## 2. Core Concepts

### Peano Representation via Tuple Lengths
In TypeScript, a tuple type has a strictly typed `length` property:
- `[any]["length"]` is `1`.
- `[any, any, any]["length"]` is `3`.

By constructing tuples of arbitrary length $N$, we represent the integer $N$. Arithmetic operations map to tuple manipulations:
- **Addition ($A + B$)**: Concatenate a tuple of length $A$ with a tuple of length $B$ and read `length`.
- **Subtraction ($A - B$)**: Destructure a tuple of length $A$ by dropping $B$ elements using `infer`.

---

## 3. How It Works Internally

```
Type Addition: Add<2, 3>
1. Build Tuple A of length 2:  [any, any]
2. Build Tuple B of length 3:  [any, any, any]
3. Concatenate via spread:     [...A, ...B] ===> [any, any, any, any, any]
4. Read length:                ["length"]   ===> 5
```

During type evaluation:
1. `BuildTuple<N>` uses tail-recursive accumulator expansion to generate a tuple with $N$ elements.
2. The concatenated tuple is evaluated as a `TupleType`.
3. Reading `["length"]` extracts the literal number type matching the total element count.

---

## 4. Syntax & API Reference

### Core Type Arithmetic Library
```typescript
// 1. Build tuple of length N
type BuildTuple<L extends number, T extends any[] = []> =
  T["length"] extends L ? T : BuildTuple<L, [...T, any]>;

// 2. Add two numbers (A + B)
export type Add<A extends number, B extends number> =
  [...BuildTuple<A>, ...BuildTuple<B>]["length"] & number;

// 3. Subtract two numbers (A - B)
export type Subtract<A extends number, B extends number> =
  BuildTuple<A> extends [...BuildTuple<B>, ...infer Rest]
    ? Rest["length"]
    : never;

// 4. Greater Than (A > B)
export type GreaterThan<A extends number, B extends number> =
  A extends B
    ? false
    : BuildTuple<A> extends [...BuildTuple<B>, ...any[]]
    ? true
    : false;
```

---

## 5. Practical Examples

### 5.1 Basic Addition and Subtraction
```typescript
type Five = Add<2, 3>;         // 5
type Three = Subtract<7, 4>;    // 3
type IsGreater = GreaterThan<10, 5>; // true
type IsLesser = GreaterThan<3, 8>;  // false
```

### 5.2 Real-World Application: Bounded Recursion Decrementer
```typescript
type Decrement<N extends number> =
  BuildTuple<N> extends [any, ...infer Rest] ? Rest["length"] : 0;

type RecurseWithCounter<T, Count extends number> =
  Count extends 0
    ? T
    : RecurseWithCounter<{ wrapped: T }, Decrement<Count>>;

type Result = RecurseWithCounter<string, 3>;
// { wrapped: { wrapped: { wrapped: string } } }
```

### 5.3 Edge Case: Subtraction Resulting in Negative
```typescript
// Subtracting larger from smaller yields 'never':
type Negative = Subtract<3, 5>; // never
```

### 5.4 Failure Case: Large Arithmetic Exceeding Instantiation Cap
```typescript
// Building tuples larger than 1,000 exceeds recursion limits:
// @ts-expect-error TS2589: Type instantiation is excessively deep and possibly infinite.
type TooBig = Add<600, 600>;
```

### 5.5 Idiomatic Pattern: Fixed Length Vector Product
```typescript
type Vector<T, Dimension extends number> =
  BuildTuple<Dimension> extends (infer _U)[] ? { [K in keyof BuildTuple<Dimension>]: T } : never;

type Vector3D = Vector<number, 3>; // [number, number, number]
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Numbers $le 999$**: Due to the compiler's instantiation depth cap, tuple-based arithmetic can only reliably handle integers up to approximately **999**.
2. **Integers Only**: Floating-point numbers (e.g. `3.14`) cannot be modeled with tuple lengths.
3. **Negative Numbers**: Tuple lengths cannot be negative. Subtraction below zero evaluates to `never`.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Problem | Solution | Compiler Error |
| :--- | :--- | :--- | :--- |
| Adding numbers $> 1000$ in types | Exceeds compiler recursion stack limit | Restrict type math to small counters ($N < 50$) | `TS2589: Type instantiation is excessively deep` |
| Attempting floating-point math | Tuples only have discrete integer lengths | Perform math at runtime | Unsupported / syntax error |
| Negative subtraction expecting `-N` | Tuples cannot have negative lengths | Guard with `GreaterThan` check | Returns `never` |
| Performing type math in hot production code | Degrades compiler performance significantly | Use simple tuple lookup array: `Prev = [never, 0, 1]` | High check time |
| Non-tail-recursive `BuildTuple` | Fails after only 50 iterations | Use accumulator parameter in tail position | Stack overflow |
| Using string manipulation for math | Extremely slow Peano parser | Use tuple length spreads | High memory usage |

---

## 8. Performance & Type-Checker Complexity
- Building tuples scales in $O(N)$ type instantiations.
- In enterprise applications, prefer static tuple lookup arrays (e.g. `type Dec = [never, 0, 1, 2, 3, 4]`) over dynamic arithmetic wherever possible.

---

## 9. Compiler Internals
- `BuildTuple` generates repeated `TupleType` instances.
- The `length` property on a concrete tuple is interned as a `NumberLiteralType` in `src/compiler/checker.ts`.

---

## 10. Best Practices & Design Guidelines
- Use type arithmetic solely for recursion depth guards, matrix dimensions, and tuple slicing.
- Never implement runtime business calculation logic in type space.
- Prefer static tuple lookup arrays for small fixed ranges ($N le 10$).

---

## 11. Real-World Case Study
### Strongly Typed Tensor Dimension Verification in Machine Learning Library
A TypeScript ML library required that vector dot products only multiply vectors of identical dimensions:
```typescript
class Tensor<Dimension extends number> {
  constructor(public data: number[]) {}

  dot(other: Tensor<Dimension>): number {
    return this.data.reduce((acc, val, i) => acc + val * other.data[i], 0);
  }
}

const v1 = new Tensor<3>([1, 2, 3]);
const v2 = new Tensor<3>([4, 5, 6]);
const v3 = new Tensor<4>([1, 2, 3, 4]);

v1.dot(v2); // OK! Dimensions match.
// @ts-expect-error TS2345: Argument of type 'Tensor<4>' is not assignable to parameter of type 'Tensor<3>'.
v1.dot(v3); // Compile-time prevented runtime vector mismatch error!
```

---

## 12. Interview Questions

<details>
<summary>1. How can addition be simulated in TypeScript's type system?</summary>

By constructing two tuple types of lengths $A$ and $B$, spreading them into a single concatenated tuple `[...BuildTuple<A>, ...BuildTuple<B>]`, and querying its `length` property.
</details>

<details>
<summary>2. What is the maximum number tuple arithmetic can handle in TypeScript?</summary>

Approximately 999, because recursive conditional types are capped by the compiler's internal recursion depth limit of ~1,000 steps.
</details>

<details>
<summary>3. Can TypeScript perform floating-point arithmetic in type space?</summary>

No. Tuple lengths are always discrete, non-negative integers.
</details>

<details>
<summary>4. What happens when you subtract a larger number from a smaller number using tuple pattern matching?</summary>

The pattern match `BuildTuple<A> extends [...BuildTuple<B>, ...infer Rest]` fails, causing the conditional type to evaluate to `never`.
</details>

<details>
<summary>5. What are Peano numbers?</summary>

A mathematical representation where natural numbers are defined inductively: $0$ is zero, and every successor $S(n)$ is $n + 1$. In TypeScript, Peano numbers are represented by wrapping tuples: $0 = 	ext{[]}$, $1 = 	ext{[any]}$, $2 = 	ext{[any, any]}$.
</details>

<details>
<summary>6. Why is a static lookup array preferred over dynamic type arithmetic for depth counters?</summary>

A static array lookup like `type Prev = [never, 0, 1, 2, 3]` resolves in $O(1)$ constant time with zero recursion, placing minimal overhead on the compiler.
</details>

<details>
<summary>7. How can you compare if number $A > B$ in type space?</summary>

By testing whether a tuple of length $A$ can destructure into a tuple of length $B$ plus at least one extra element: `BuildTuple<A> extends [...BuildTuple<B>, any, ...any[]] ? true : false`.
</details>

<details>
<summary>8. How do you construct a fixed-length array of type <code>T</code>?</summary>

```typescript
type FixedArray<T, N extends number> = BuildTuple<N> extends any[] ? { [K in keyof BuildTuple<N>]: T } : never;
```
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Increment an Integer (Easy)
Implement `Increment<N extends number>` using tuple spread.
<details>
<summary>Solution</summary>

```typescript
type BuildTuple<L extends number, T extends any[] = []> =
  T["length"] extends L ? T : BuildTuple<L, [...T, any]>;

type Increment<N extends number> = [...BuildTuple<N>, any]["length"] & number;

type Four = Increment<3>; // 4
```
</details>

### Exercise 2: Static Decrement Lookup (Easy)
Create a zero-overhead decrement utility for numbers 0 to 5.
<details>
<summary>Solution</summary>

```typescript
type Dec = [never, 0, 1, 2, 3, 4, 5];
type Two = Dec[3]; // 2
```
</details>

### Exercise 3: Multiply by Two (Medium)
Implement `Double<N extends number>` using tuple concatenation.
<details>
<summary>Solution</summary>

```typescript
type Double<N extends number> = [...BuildTuple<N>, ...BuildTuple<N>]["length"] & number;
type Six = Double<3>; // 6
```
</details>

### Exercise 4: Numerical Range Generator (Medium)
Generate a union of numbers from 0 up to $N - 1$.
<details>
<summary>Solution</summary>

```typescript
type NumberRange<N extends number, Acc extends number[] = []> =
  Acc["length"] extends N
    ? Acc[number]
    : NumberRange<N, [...Acc, Acc["length"]]>;

type ZeroToThree = NumberRange<4>; // 0 | 1 | 2 | 3
```
</details>

### Exercise 5: Integer Comparison Utility (Hard)
Implement `LessThan<A, B>` returning `true` if $A < B$, else `false`.
<details>
<summary>Solution</summary>

```typescript
type LessThan<A extends number, B extends number> =
  A extends B
    ? false
    : BuildTuple<B> extends [...BuildTuple<A>, ...any[]]
    ? true
    : false;

type L1 = LessThan<2, 5>; // true
type L2 = LessThan<5, 2>; // false
```
</details>

### Exercise 6: Complete Integer Multiplier (Hard)
Implement `Multiply<A, B>` using recursive addition accumulation.
<details>
<summary>Solution</summary>

```typescript
type Multiply<A extends number, B extends number, Count extends any[] = [], Acc extends any[] = []> =
  Count["length"] extends B
    ? Acc["length"]
    : Multiply<A, B, [...Count, any], [...Acc, ...BuildTuple<A>]>;

type Product = Multiply<4, 3>; // 12
```
</details>
