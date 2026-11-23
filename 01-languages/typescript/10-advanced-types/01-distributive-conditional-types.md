# Distributive Conditional Types

> When conditional types check an unconstrained generic type parameter, they automatically distribute over union types, enabling elegant union filtering, mapping, and extraction.

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [00-conditional-types.md](file:///E:/Learning/engineering-notes/01-languages/typescript/10-advanced-types/00-conditional-types.md) |
| **Time to Read** | 24 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 2.8 / TS 5.x / TS 7.0 Native |
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
In TypeScript, a union type `A | B | C` represents a value that could be any of the three types. When performing operations on unions, we often want to map or filter *each constituent member individually* rather than treating the union as an indivisible monolithic entity.

**Distributive Conditional Types** provide this behavior natively. When a conditional type is applied to a bare generic type parameter `T`, the operation distributes over union members:
```typescript
(A | B | C) extends Target ? TrueBranch : FalseBranch
// Expands automatically to:
(A extends Target ? TrueBranch : FalseBranch) |
(B extends Target ? TrueBranch : FalseBranch) |
(C extends Target ? TrueBranch : FalseBranch)
```

This single mechanism powers foundational standard utilities like `Exclude<T, U>`, `Extract<T, U>`, and `NonNullable<T>`.

---

## 2. Core Concepts

### Bare Type Parameter Rule
Distribution ONLY occurs when the checked type is a **bare type parameter** (a naked variable like `T`):
```typescript
// Distributive (T is naked):
type ToArray<T> = T extends any ? T[] : never;
type Res1 = ToArray<string | number>; // string[] | number[]

// Non-distributive (T is wrapped in a tuple):
type ToArrayNonDist<T> = [T] extends [any] ? T[] : never;
type Res2 = ToArrayNonDist<string | number>; // (string | number)[]
```

### Filtering with `never`
Because `never` in a union represents an empty set, evaluating a union branch to `never` eliminates that branch from the resulting union:
```typescript
type ExcludeStrings<T> = T extends string ? never : T;
type Clean = ExcludeStrings<string | number | boolean>;
// never | number | boolean => number | boolean
```

---

## 3. How It Works Internally

```
Input: Exclude<string | number | boolean, string>
                        |
                        v
        Distribute across each member:
    +-----------------------------------------------+
    | ("string"  extends string ? never : string)   | ===> never (Eliminated)
    | ("number"  extends string ? never : number)   | ===> number
    | ("boolean" extends string ? never : boolean)  | ===> boolean
    +-----------------------------------------------+
                        |
                        v
        Result Union: number | boolean
```

Inside `checker.ts`:
1. When checking `T extends U ? X : Y`, the compiler tests if `T` has `TypeFlags.Union` and is an unadorned type parameter.
2. If true, it invokes `mapType(T, (t) => getTypeFromConditionalType(...))`.
3. It accumulates results from each member into a union using `getUnionType`.

---

## 4. Syntax & API Reference

### Disabling Distribution
Wrap both the checked type and the extends target in square brackets:
```typescript
type SafeNeverCheck<T> = [T] extends [never] ? true : false;
type DistributiveNeverCheck<T> = T extends never ? true : false;

type A = SafeNeverCheck<never>;        // true
type B = DistributiveNeverCheck<never>; // never! (zero branches evaluated)
```

---

## 5. Practical Examples

### 5.1 Discriminated Union Filter
```typescript
type Event =
  | { type: "click"; x: number; y: number }
  | { type: "hover"; element: string }
  | { type: "scroll"; offset: number };

type FilterEvents<T, TypeString extends string> =
  T extends { type: TypeString } ? T : never;

type ClickEvent = FilterEvents<Event, "click">;
// { type: "click"; x: number; y: number }
```

### 5.2 Real-World Application: Distributive Omit
Standard `Omit<T, K>` breaks discriminated unions by flattening them into a single non-discriminated shape. A distributive version fixes this:
```typescript
type Action =
  | { type: "INIT"; payload: string; meta: number }
  | { type: "RESET"; payload: boolean; meta: number };

// Flattens into non-discriminated:
type Broken = Omit<Action, "meta">;

// Distributive preserves individual variants:
type DistributiveOmit<T, K extends keyof any> =
  T extends any ? Omit<T, K> : never;

type CleanAction = DistributiveOmit<Action, "meta">;
// { type: "INIT"; payload: string } | { type: "RESET"; payload: boolean }
```

### 5.3 Edge Case: Accidental Distribution Over Boolean
```typescript
type IsNever<T> = T extends never ? "YES" : "NO";
type T1 = IsNever<never>; // evaluates to never, NOT "YES"!
```

### 5.4 Failure Case: Unintended Union Splitting
```typescript
// Expected: Check if whole union is assignable
type IsUnionStringOrNumber<T> = T extends string | number ? true : false;

// If passed 'string | boolean', it distributes:
// (string extends ...) | (boolean extends ...) => true | false (boolean)!
type Surprise = IsUnionStringOrNumber<string | boolean>; // boolean
```

### 5.5 Idiomatic Pattern: Union to Tuple Length Check
```typescript
type IsSingleType<T> = [T] extends [infer U]
  ? [U] extends [never] ? false : true
  : false;
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **The `never` Trap**: `never` is an empty union (zero elements). Distributing over zero elements produces zero iterations, returning `never`! Always wrap in tuples: `[T] extends [never]`.
2. **Boolean is a Union**: `boolean` distributes as `true | false`.
3. **Cartesian Product Blowups**: If two distributive types are nested, they evaluate all pairwise combinations ($O(N 	imes M)$).

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Problem | Solution | Compiler Error |
| :--- | :--- | :--- | :--- |
| Checking `T extends never` directly | Evaluates to `never` without taking true branch | Wrap in tuples: `[T] extends [never]` | Unexpected `never` result |
| Using standard `Omit` on discriminated union | Destroys discrimination between union members | Use distributive conditional `DistributiveOmit` | Lost discriminated union safety |
| Forgetting that `boolean` is a union | Conditional splits into `true` and `false` cases | Disable distribution or handle union explicitly | Returns `boolean` instead of literal |
| Distributing over large unions in recursion | Exponential evaluation time ($O(N^K)$) | Disable distribution with tuples | `TS2589: Type instantiation is excessively deep` |
| Naked generic in template literal | Distributes across all combinations | Combine strings before distributing | Cartesian product explosion |
| Expecting object interfaces to distribute | Only unions distribute | Unionize interfaces first if needed | Single-pass evaluation |

---

## 8. Performance & Type-Checker Complexity
- Distributive types evaluate in $O(N)$ where $N$ is the number of union members.
- If crossed with another union, complexity becomes $O(N 	imes M)$. Keep union sizes bounded.

---

## 9. Compiler Internals
- `src/compiler/checker.ts` inspects `type.flags & TypeFlags.TypeParameter`.
- In `getTypeFromConditionalType`, if `checkType` is an unadorned generic parameter and resolved to a union, the checker maps each union constituent individually through `getTypeFromConditionalType` and merges results.

---

## 10. Best Practices & Design Guidelines
- Wrap bare generic parameters in tuples (`[T] extends [U]`) when you intend to test the union as a whole.
- Use distributive conditional types to implement clean union filtering and discriminated union projections.
- Always use tuple wrapping when checking for `never` or `any`.

---

## 11. Real-World Case Study
### Type-Safe Event Bus with Discriminated Action Filtering
```typescript
type AppEvents =
  | { category: "AUTH"; action: "LOGIN"; userId: string }
  | { category: "AUTH"; action: "LOGOUT"; userId: string }
  | { category: "UI"; action: "CLICK"; buttonId: string };

type EventsByCategory<T, C extends string> =
  T extends { category: C } ? T : never;

type AuthEvents = EventsByCategory<AppEvents, "AUTH">;
// { category: "AUTH"; action: "LOGIN"; userId: string } | { category: "AUTH"; action: "LOGOUT"; userId: string }
```

---

## 12. Interview Questions

<details>
<summary>1. What makes a conditional type distributive in TypeScript?</summary>

A conditional type is distributive when the type being checked in the `extends` clause is a **naked, bare generic type parameter** (`T extends U ? X : Y`). When a union is passed for `T`, the conditional distributes over each member of the union.
</details>

<details>
<summary>2. How do you prevent a conditional type from distributing over a union?</summary>

By wrapping the checked type parameter and the target type in square brackets (tuples): `[T] extends [U] ? X : Y`.
</details>

<details>
<summary>3. Why does <code>never extends never ? true : false</code> evaluate to <code>never</code> when <code>T</code> is generic?</summary>

Because `never` is an empty union (a union with 0 members). Distributive conditional types execute once per union member. Since there are zero members, the operation runs zero times, producing an empty union (`never`).
</details>

<details>
<summary>4. How does <code>Exclude&lt;T, U&gt;</code> work using distribution?</summary>

`type Exclude<T, U> = T extends U ? never : T;`. It tests each member of `T`. If a member extends `U`, it returns `never` (which is eliminated from unions). Otherwise, it keeps the member.
</details>

<details>
<summary>5. What happens when you pass <code>boolean</code> to a distributive conditional type?</summary>

Because `boolean` is a union of `true | false`, the conditional type evaluates twice (once for `true` and once for `false`), often returning a union of both branches.
</details>

<details>
<summary>6. Why is a custom <code>DistributiveOmit</code> necessary for discriminated unions?</summary>

Standard `Omit` extracts common properties and flattens the union into a single merged object shape, destroying discriminant tags. `DistributiveOmit` applies `Omit` to each union member individually, preserving discriminated variant structures.
</details>

<details>
<summary>7. Does distribution happen if <code>T</code> is constrained, e.g. <code>T extends object</code>?</summary>

Yes, as long as `T` is written bare in the condition: `type Check<T extends object> = T extends ...`.
</details>

<details>
<summary>8. How do you check if a generic type is a union?</summary>

By testing if distribution splits the type into different components:
```typescript
type IsUnion<T, U = T> =
  T extends any ? ([U] extends [T] ? false : true) : never;
```
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Filter Numbers from Union (Easy)
Write a distributive utility `FilterNumbers<T>` that removes all numbers from a union.
<details>
<summary>Solution</summary>

```typescript
type FilterNumbers<T> = T extends number ? never : T;
type Res = FilterNumbers<string | number | boolean>; // string | boolean
```
</details>

### Exercise 2: Safe Never Checker (Easy)
Write a non-distributive `IsNever<T>` utility that correctly returns `true` for `never`.
<details>
<summary>Solution</summary>

```typescript
type IsNever<T> = [T] extends [never] ? true : false;
type T1 = IsNever<never>;  // true
type T2 = IsNever<string>; // false
```
</details>

### Exercise 3: Distributive Pick (Medium)
Implement `DistributivePick<T, K extends keyof any>` that picks keys from each member of a union.
<details>
<summary>Solution</summary>

```typescript
type DistributivePick<T, K extends keyof any> =
  T extends any ? Pick<T, Extract<keyof T, K>> : never;

type U = { a: 1; b: 2 } | { a: 3; c: 4 };
type Picked = DistributivePick<U, "a">; // { a: 1 } | { a: 3 }
```
</details>

### Exercise 4: Union Detector (Medium)
Implement `IsUnion<T>` returning `true` if `T` is a union, else `false`.
<details>
<summary>Solution</summary>

```typescript
type IsUnion<T, U = T> =
  T extends any ? ([U] extends [T] ? false : true) : never;

type T1 = IsUnion<string | number>; // true
type T2 = IsUnion<string>;          // false
```
</details>

### Exercise 5: Extract Discriminated Variant by Key-Value (Hard)
Write a utility `ExtractByProp<TUnion, TKey extends string, TVal>` that extracts the matching variant.
<details>
<summary>Solution</summary>

```typescript
type ExtractByProp<T, K extends string, V> =
  T extends Record<K, any>
    ? T[K] extends V
      ? T
      : never
    : never;

type Actions =
  | { type: "A"; payload: number }
  | { type: "B"; payload: string };

type ActionB = ExtractByProp<Actions, "type", "B">; // { type: "B"; payload: string }
```
</details>

### Exercise 6: Union to Tuple Converter (Hard)
Convert a union type into a tuple type using distributive conditional types and function parameter inference.
<details>
<summary>Solution</summary>

```typescript
type UnionToIntersection<U> =
  (U extends any ? (k: U) => void : never) extends (k: infer I) => void ? I : never;

type LastOfUnion<T> =
  UnionToIntersection<T extends any ? () => T : never> extends () => infer R ? R : never;

type UnionToTuple<T, Last = LastOfUnion<T>> =
  [T] extends [never] ? [] : [...UnionToTuple<Exclude<T, Last>>, Last];

type Tuple = UnionToTuple<"a" | "b" | "c">; // ["a", "b", "c"]
```
</details>
