# Variadic Tuple Types

> Variadic tuple types introduce generic spread operations within tuple definitions, allowing functions to model arbitrary prefixing, suffixing, concatenation, and slicing of parameter lists.

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [02-arrays-and-tuples.md](file:///E:/Learning/engineering-notes/01-languages/typescript/01-fundamentals/02-arrays-and-tuples.md), [00-generic-functions.md](file:///E:/Learning/engineering-notes/01-languages/typescript/06-generics/00-generic-functions.md) |
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
In JavaScript, functions frequently combine or slice parameter arrays:
- `concat(arr1, arr2)`
- `curry(fn, ...initialArgs)`
- `pipe(fn1, fn2, fn3)`

Before TypeScript 4.0, tuple types were rigid. You could not spread a generic type parameter inside a tuple. Typing a function that concatenated two generic tuples required authoring dozens of manual overloads:
```typescript
// Pre-TS 4.0 Overload hell:
function concat<T1, T2>(a: [T1], b: [T2]): [T1, T2];
function concat<T1, T2, T3>(a: [T1, T2], b: [T3]): [T1, T2, T3];
// ... hundreds more overloads!
```

TypeScript 4.0 introduced **Variadic Tuple Types**:
- You can spread generic type parameters (`...T`) anywhere inside a tuple.
- The compiler dynamically calculates the resulting concatenated tuple lengths and element positions.

---

## 2. Core Concepts

### Generic Tuple Spread
```typescript
type Concat<T extends readonly unknown[], U extends readonly unknown[]> = [...T, ...U];

type Res = Concat<[string, number], [boolean, symbol]>;
// [string, number, boolean, symbol]
```

### Spreads in Any Position
Unlike older TypeScript where rest elements were only allowed at the end, variadic spreads can appear anywhere:
```typescript
type Prepend<T, List extends unknown[]> = [T, ...List];
type Append<List extends unknown[], T> = [...List, T];
type Sandwich<A extends unknown[], B, C extends unknown[]> = [...A, B, ...C];

type S = Sandwich<[1, 2], "middle", [true]>; // [1, 2, "middle", true]
```

---

## 3. How It Works Internally

```
Type Expression: [...[string, number], boolean]
                       |
                       v
Type Checker concatenates element types into a single TupleType:
  Element 0: string
  Element 1: number
  Element 2: boolean
  Length: 3
```

During type evaluation:
1. The compiler inspects each spread element in the tuple definition.
2. If the spread element is a concrete tuple, its elements are flattened directly into the target tuple at that offset.
3. If the spread element is a rest array (`string[]`), the resulting tuple receives a rest element at that position.
4. Labeled tuple elements (e.g. `[id: string, ...rest: number[]]`) retain their names in tooltips.

---

## 4. Syntax & API Reference

### Function Signatures with Variadic Tuples
```typescript
function callWithPrefix<T extends unknown[]>(
  prefix: string,
  fn: (...args: [string, ...T]) => void,
  ...rest: T
): void {
  fn(prefix, ...rest);
}

callWithPrefix("LOG", (p, count, active) => {
  console.log(p, count, active);
}, 42, true);
```

---

## 5. Practical Examples

### 5.1 Typed Curry Function
```typescript
function curry2<A, B, R>(fn: (a: A, b: B) => R) {
  return (a: A) => (b: B): R => fn(a, b);
}

const add = (a: number, b: number) => a + b;
const curriedAdd = curry2(add);
console.log(curriedAdd(10)(20)); // 30
```

### 5.2 Real-World Application: Function Argument Binding
```typescript
function partialApply<T extends unknown[], U extends unknown[], R>(
  fn: (...args: [...T, ...U]) => R,
  ...headArgs: T
): (...tailArgs: U) => R {
  return (...tailArgs: U) => fn(...headArgs, ...tailArgs);
}

function sendEmail(to: string, subject: string, body: string, urgent: boolean) {
  return `Sent to ${to}: ${subject}`;
}

// Partially apply first two arguments:
const sendAlert = partialApply(sendEmail, "admin@corp.com", "System Alert");

// Tail arguments are strictly verified:
sendAlert("Disk space low", true);
```

### 5.3 Edge Case: Multiple Rest Arrays in a Single Tuple
```typescript
// In TypeScript, a tuple can only have ONE unbounded rest array:
// type Bad = [...string[], ...number[]]; // Error: A rest element cannot follow another rest element!

// But multiple FIXED-LENGTH tuples can be spread freely:
type Valid = [...[1, 2], ...[3, 4], ...[5, 6]]; // [1, 2, 3, 4, 5, 6]
```

### 5.4 Failure Case: Spreading Non-Array Generic
```typescript
// @ts-expect-error TS2498: An interface or type parameter used as an element of a tuple type must be an array type.
type BadSpread<T> = [...T];
```

### 5.5 Idiomatic Pattern: Labeled Variadic Tuples
```typescript
type EventCallback<TArgs extends unknown[]> =
  (...args: [timestamp: number, eventName: string, ...payload: TArgs]) => void;

const onUserUpdate: EventCallback<[userId: string, role: string]> =
  (timestamp, eventName, userId, role) => {
    console.log(timestamp, eventName, userId, role);
  };
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **At Most One Rest Element**: A tuple can have at most one unbounded array spread (`...T[]`). You cannot have two unbounded arrays in the same tuple.
2. **Generic Constraint Required**: To spread a type parameter `...T`, `T` must be constrained to an array or tuple type: `<T extends readonly unknown[]>`.
3. **Optional Elements Must Follow Required Elements**: If a tuple has optional elements (`x?: number`), optional elements cannot precede required elements unless represented as a union.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Problem | Solution | Compiler Error |
| :--- | :--- | :--- | :--- |
| Spreading unconstrained generic `...T` | Compiler cannot verify that `T` is an array | Constrain generic: `<T extends unknown[]>` | `TS2498: A type parameter used as an element...` |
| Multiple unbounded rest arrays in tuple | Compiler cannot determine element boundaries | Ensure only one unbounded rest array exists | `TS1265: A rest element cannot follow another...` |
| Overloading tuple concatenation | Unwieldy and rigid code | Use variadic spread `[...A, ...B]` | Cluttered signatures |
| Dropping readonly modifier in spread | Spreading mutable into readonly causes type mismatch | Use `T extends readonly unknown[]` | Readonly assignment error |
| Forgetting labeled elements | Tooltip loses argument names | Use labeled elements: `[head: string, ...rest: T]` | Anonymous tuple tooltips |
| Passing non-tuple array to fixed position | Loses length verification | Pass explicit tuple or `as const` | Premature type widening |

---

## 8. Performance & Type-Checker Complexity
- Concatenating fixed tuples resolves in $O(L_1 + L_2)$ time.
- Highly optimized in `src/compiler/checker.ts`; replaces hundreds of manual overloads with a single check.

---

## 9. Compiler Internals
- `SpreadElement` in a tuple is parsed as `SyntaxKind.SpreadElement`.
- `getTupleType` in `checker.ts` inspects element flags (`ElementFlags.Rest`, `ElementFlags.Variadic`).
- If a spread element is generic, it creates a `VariadicTupleType` whose properties are resolved dynamically upon instantiation.

---

## 10. Best Practices & Design Guidelines
- Constrain all variadic type parameters with `extends readonly unknown[]`.
- Use labeled tuple elements (`[name: string, ...rest: T]`) to provide readable IntelliSense parameter descriptions.
- Use variadic tuple types to author clean higher-order functions (curry, compose, memoize, partialApply).

---

## 11. Real-World Case Study
### Strongly Typed Middleware Runner Pipeline
```typescript
type Middleware<Context, ExtraArgs extends unknown[]> =
  (ctx: Context, ...args: ExtraArgs) => Promise<void>;

class Pipeline<Context, ExtraArgs extends unknown[]> {
  private middlewares: Array<Middleware<Context, ExtraArgs>> = [];

  use(fn: Middleware<Context, ExtraArgs>): this {
    this.middlewares.push(fn);
    return this;
  }

  async run(ctx: Context, ...args: ExtraArgs): Promise<void> {
    for (const mw of this.middlewares) {
      await mw(ctx, ...args);
    }
  }
}

interface AppContext { requestId: string; }
const pipeline = new Pipeline<AppContext, [startTime: number, traceId: string]>();

pipeline.use(async (ctx, startTime, traceId) => {
  console.log(ctx.requestId, startTime, traceId);
});

await pipeline.run({ requestId: "req_101" }, Date.now(), "trace_xyz");
```

---

## 12. Interview Questions

<details>
<summary>1. What are Variadic Tuple Types in TypeScript 4.0+?</summary>

Variadic tuple types allow spreading generic type parameters (`...T`) inside tuple type definitions, enabling dynamic composition, concatenation, prefixing, and suffixing of tuple and parameter types.
</details>

<details>
<summary>2. How did variadic tuple types eliminate overload bloat?</summary>

Before TS 4.0, functions like `concat` or `curry` required dozens of overloaded signatures for each combination of tuple lengths. Variadic tuples allow expressing arbitrary concatenation with a single generic signature: `function concat<T extends any[], U extends any[]>(a: T, b: U): [...T, ...U]`.
</details>

<details>
<summary>3. Can a tuple have two rest elements?</summary>

A tuple can have multiple fixed-length generic tuple spreads, but it can have **at most one unbounded array rest element** (e.g. `...string[]`), because the compiler cannot determine where one unbounded sequence ends and the next begins.
</details>

<details>
<summary>4. What constraint is required on type parameter <code>T</code> to spread it in a tuple?</summary>

`T` must be constrained to an array or tuple type: `<T extends readonly unknown[]>` or `<T extends unknown[]>`.
</details>

<details>
<summary>5. What are labeled tuple elements?</summary>

Introduced in TypeScript 4.0, labeled tuple elements allow giving names to elements within a tuple: `[id: string, count: number]`. These labels appear in IDE autocompletion tooltips and parameter hints.
</details>

<details>
<summary>6. How do you prepend an element to a tuple type?</summary>

```typescript
type Prepend<Element, Tuple extends unknown[]> = [Element, ...Tuple];
```
</details>

<details>
<summary>7. What happens when you spread a union of tuples?</summary>

The operation distributes over the union of tuples, generating a union of concatenated tuples.
</details>

<details>
<summary>8. How do variadic tuples interact with function rest parameters?</summary>

When a function parameter is typed as a variadic tuple `...args: [string, ...T]`, the compiler type-checks the first argument as a `string` and forwards the remaining arguments into `T`.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Tuple Concatenator (Easy)
Write a utility type `Concat<A, B>` that merges two tuples.
<details>
<summary>Solution</summary>

```typescript
type Concat<A extends readonly unknown[], B extends readonly unknown[]> = [...A, ...B];
type C = Concat<[1, 2], ["a", "b"]>; // [1, 2, "a", "b"]
```
</details>

### Exercise 2: Prepend Element (Easy)
Write `Push<T, E>` that adds `E` to the end of tuple `T`.
<details>
<summary>Solution</summary>

```typescript
type Push<T extends readonly unknown[], E> = [...T, E];
type P = Push<[1, 2], 3>; // [1, 2, 3]
```
</details>

### Exercise 3: Drop First Tuple Element (Medium)
Write `Shift<T>` that removes the first element of a tuple.
<details>
<summary>Solution</summary>

```typescript
type Shift<T extends readonly unknown[]> =
  T extends readonly [unknown, ...infer Rest] ? Rest : [];

type S = Shift<[string, number, boolean]>; // [number, boolean]
```
</details>

### Exercise 4: Pop Last Tuple Element (Medium)
Write `Pop<T>` that removes the last element of a non-empty tuple.
<details>
<summary>Solution</summary>

```typescript
type Pop<T extends readonly unknown[]> =
  T extends readonly [...infer Rest, unknown] ? Rest : [];

type P = Pop<[1, 2, 3]>; // [1, 2]
```
</details>

### Exercise 5: Generic Partial Application Typing (Hard)
Type a function that binds the first argument of any generic function, returning a function accepting the rest.
<details>
<summary>Solution</summary>

```typescript
function bindFirst<First, Rest extends unknown[], Return>(
  fn: (first: First, ...rest: Rest) => Return,
  firstVal: First
): (...rest: Rest) => Return {
  return (...rest: Rest) => fn(firstVal, ...rest);
}

function divide(a: number, b: number): number { return a / b; }
const halve = bindFirst(divide, 100);
console.log(halve(2)); // 50
```
</details>

### Exercise 6: Curried Pipe Composer (Hard)
Type a `compose` function that takes two functions and combines their argument tuples.
<details>
<summary>Solution</summary>

```typescript
function compose<Args extends unknown[], Mid, Out>(
  fn1: (...args: Args) => Mid,
  fn2: (arg: Mid) => Out
): (...args: Args) => Out {
  return (...args: Args) => fn2(fn1(...args));
}

const parse = (s: string, radix: number) => parseInt(s, radix);
const double = (n: number) => n * 2;
const parseAndDouble = compose(parse, double);
console.log(parseAndDouble("10", 10)); // 20
```
</details>
