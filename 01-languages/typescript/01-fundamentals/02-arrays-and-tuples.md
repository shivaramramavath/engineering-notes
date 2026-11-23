# Arrays, Tuples & Variadic Elements in TypeScript

> A comprehensive technical guide to typed arrays, fixed-length tuples, labeled tuple elements, optional tuple elements, variadic tuple spreads, and `readonly` array immutability.

| Metadata | Value |
| :--- | :--- |
| **Level** | Beginner / Intermediate |
| **Prerequisites** | [01-primitive-types.md](file:///e:/Learning/engineering-notes/01-languages/typescript/01-fundamentals/01-primitive-types.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 5.0+ / TS 7.0 Native |
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

### Problem Statement
In JavaScript, arrays are dynamic, heterogeneous lists that can hold arbitrary mixtures of numbers, strings, and objects. However, production software frequently requires strict collections: homogeneous sequences (e.g. `number[]`) or fixed-length records where each position holds a specific semantic type (e.g. `[lat: number, lng: number]`). Without TypeScript tuples, function return pairs (like React's `[state, setState]`) widen into generic unions, losing positional type guarantees.

### Historical Context
Early TypeScript supported simple arrays (`T[]`) and basic tuples (`[T, U]`). In TypeScript 4.0, **Labeled Tuple Elements** and **Variadic Tuple Types** were introduced, allowing tuples to support spread elements anywhere in the definition, optional elements, and self-documenting parameter labels.

### Real-World Relevance
Tuples power coordinate systems, database rows, function parameter list manipulation, currying engines, and custom hook state returns.

---

## 2. Core Concepts

### Arrays vs Tuples
- **Array (`T[]` or `Array<T>`)**: An unbounded list containing zero or more elements of type `T`.
- **Tuple (`[T, U, V]`)**: A fixed-length, position-specific sequence where index 0 is type `T`, index 1 is type `U`, and index 2 is type `V`.

```typescript
const arr: number[] = [1, 2, 3, 4, 5]; // Length is unbounded
const point: [number, number] = [10, 20]; // Length is strictly 2
```

### Labeled Tuple Elements
Labels improve readability and IDE autocomplete without changing runtime behavior:

```typescript
type GeoPoint = [latitude: number, longitude: number, altitude?: number];
```

---

## 3. How It Works Internally

### Tuple Type Representation
Internally, TypeScript models a tuple as a subtype of `Array` with a fixed numeric literal `length` property and specific numeric property indices (`0`, `1`, `2`):

```
Tuple [string, number] Type Structure:
{
  0: string;
  1: number;
  length: 2;
  slice(): Array<string | number>;
  ... Array prototype methods
}
```

Because `length` is typed as a numeric literal (e.g. `2`), TypeScript prevents accessing out-of-bounds indices at compile time.

---

## 4. Syntax & API Reference

### Variadic & Optional Tuples

```typescript
// 1. Optional Elements (must follow required elements)
type HttpResponse = [statusCode: number, message?: string];

// 2. Rest / Variadic Tuple Elements
type StringWithNumbers = [string, ...number[]];
const data: StringWithNumbers = ['scores', 95, 88, 92];

// 3. Middle Rest Elements (TS 4.2+)
type PaddedRow = [start: boolean, ...content: string[], end: boolean];

// 4. Readonly Tuples
type Point3D = readonly [x: number, y: number, z: number];
```

---

## 5. Practical Examples

### Example 1: React-Style Hook State Return
- **Goal:** Return a typed state value and setter pair without union widening.
- **Code:**
```typescript
function useToggle(initial: boolean): [state: boolean, toggle: () => void] {
  let value = initial;
  const toggle = () => { value = !value; };
  return [value, toggle];
}

const [isOpen, toggleOpen] = useToggle(false);
console.log('Initial state:', isOpen);
toggleOpen();
```
- **Output:**
```
Initial state: false
```
- **Explanation:** Using a tuple return type ensures `isOpen` is typed strictly as `boolean` and `toggleOpen` is typed as `() => void`.

### Example 2: Variadic Tuple for Typed Event Emitters
- **Goal:** Type-safe event argument parameters.
- **Code:**
```typescript
type EventMap = {
  login: [userId: string, timestamp: number];
  error: [err: Error];
  logout: [];
};

class TypedEmitter {
  emit<K extends keyof EventMap>(event: K, ...args: EventMap[K]): void {
    console.log(`Event [${String(event)}] dispatched with ${args.length} args.`);
  }
}

const emitter = new TypedEmitter();
emitter.emit('login', 'usr_42', Date.now()); // OK
emitter.emit('logout');                       // OK: No arguments required
```
- **Output:**
```
Event [login] dispatched with 2 args.
Event [logout] dispatched with 0 args.
```

### Example 3: Function Curry / Concatenation via Variadic Tuples
- **Goal:** Concatenate argument tuples dynamically.
- **Code:**
```typescript
function concatArgs<T extends readonly unknown[], U extends readonly unknown[]>(
  a: T,
  b: U
): [...T, ...U] {
  return [...a, ...b];
}

const merged = concatArgs([1, 'a'] as const, [true, null] as const);
// merged is: [1, "a", true, null]
console.log('Merged length:', merged.length);
```
- **Output:**
```
Merged length: 4
```

### Example 4: Negative Test — Mutating a Readonly Tuple
- **Goal:** Demonstrate compiler rejection when attempting to push to a readonly tuple.
- **Code:**
```typescript
const coordinates: readonly [number, number] = [40.7128, -74.0060];

// @ts-expect-error TS2339: Property 'push' does not exist on type 'readonly [number, number]'
coordinates.push(100);
```
- **Real Compiler Error:**
```
error TS2339: Property 'push' does not exist on type 'readonly [number, number]'.
```

### Example 5: `noUncheckedIndexedAccess` with Array Indexing
- **Goal:** Verify safe array index handling.
- **Code:**
```typescript
const scores: number[] = [95, 82, 88];

// Under noUncheckedIndexedAccess: true, first is: number | undefined
const first = scores[0];

if (first !== undefined) {
  console.log('First score:', first.toFixed(1));
}
```
- **Output:**
```
First score: 95.0
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Tuples and `.push()` in Sloppy Types**: Without `readonly`, standard TypeScript tuples historically allowed `.push()`, `.pop()`, and mutating operations that violated the fixed `length` constraint at runtime! Always prefer `readonly [T, U]` for true immutability.
2. **Array Widening without `as const`**: `const point = [10, 20];` is inferred as `number[]`, NOT `[number, number]`. Add `as const` or `: [number, number]` to preserve the tuple type.
3. **Array Destructuring Rest Inference**: In `const [head, ...tail] = [1, 2, 3] as const;`, `head` is `1` and `tail` is inferred as `readonly [2, 3]`.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Real Compiler Issue | Idiomatic Solution | Why It Matters |
| :--- | :--- | :--- | :--- |
| Omitting `as const` on returned pairs | Inferred as `(string \| number)[]` array union | Return `as const` or explicit tuple | Prevents loss of position-specific types |
| Using mutable tuples | Allows `.push()` to violate length invariant | Use `readonly [T, U]` | Guarantees true compile-time fixed length |
| Indexing arrays without checking `undefined` | Unhandled runtime index out-of-bounds | Enable `noUncheckedIndexedAccess: true` | Forces runtime safety checks on array reads |
| Using unlabeled tuple elements in public APIs | Poor developer ergonomics (`[string, string]`) | Use labeled tuples: `[id: string, name: string]` | Provides self-documenting editor hints |

---

## 8. Performance & Type-Checker Complexity

- **Variadic Tuples Flattening**: Deeply nested variadic spreads (`[...T, ...U, ...V]`) where each type has multiple union components cause polynomial expansion in the type checker. Limit variadic nesting depth in generic hot paths.

---

## 9. Compiler Internals

In the TypeScript AST:
- An Array Type is represented by `ts.SyntaxKind.ArrayType`.
- A Tuple Type is represented by `ts.SyntaxKind.TupleType`.
- Tuple elements are `NamedTupleMember` nodes holding an optional name identifier, question token (`?`), and rest token (`...`).

---

## 10. Best Practices & Design Guidelines

1. **Use Labeled Elements for Multi-Type Tuples**: `[latitude: number, longitude: number]`.
2. **Always Mark Returned Tuples as `readonly`**: Prevents accidental mutations.
3. **Leverage `noUncheckedIndexedAccess`**: Treats array lookups defensively as `T | undefined`.

---

## 11. Real-World Case Study

A geospatial microservice returned latitude and longitude as a plain 2-element array `[lat, lng]`. A junior developer mistakenly reversed the parameters when storing coordinates, reversing longitude and latitude globally for thousands of locations.

### Solution:
Migrated the API to labeled readonly tuples and branded coordinates:
```typescript
type Latitude = number & { readonly __brand: 'Lat' };
type Longitude = number & { readonly __brand: 'Lng' };
type GeoCoordinate = readonly [lat: Latitude, lng: Longitude];
```
**Outcome**: Inverted coordinate arguments immediately caused compilation failures across the entire microservice.

---

## 12. Interview Questions

### Q1: What is the difference between `number[]` and `[number]` in TypeScript?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

`number[]` is an unbounded array containing zero or more numbers; its `length` property is typed as generic `number`.
`[number]` is a tuple type containing **exactly one** number; its `length` property is typed as the literal type `1`. Accessing index `1` produces a compile-time error.
</details>

### Q2: Why does `[number, string]` allow `.push()` without the `readonly` modifier?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

By design, TypeScript tuples inherit from `Array.prototype`. In non-readonly tuples, array mutation methods like `.push()` are permitted, but any pushed elements must match the union of tuple element types (`number | string`). However, calling `.push()` mutates the array length at runtime while TypeScript still considers its static length to be `2`. Adding `readonly` strips mutating methods like `push` and `pop`.
</details>

### Q3: How do Variadic Tuple Types enable typed `Promise.all`?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Variadic tuple types allow mapping over tuple elements using mapped tuple syntax:
`type AwaitedTuple<T> = { [K in keyof T]: Awaited<T[K]> }`.
When passing an arbitrary tuple of promises (`[Promise<string>, Promise<number>]`), TypeScript maps over each position individually, producing `[string, number]` rather than widening to `(string | number)[]`.
</details>

---

## 13. Exercises & Challenges

### Exercise 1 (Easy): Declare a Labeled Tuple
- **Problem Statement**: Declare a tuple type `UserRow` containing an integer `id`, a string `username`, and an optional boolean `isAdmin`.
- **Solution**:
```typescript
export type UserRow = [id: number, username: string, isAdmin?: boolean];
```

### Exercise 2 (Medium): Prepend Element via Variadic Tuple
- **Problem Statement**: Write a generic type `Prepend<T, List extends unknown[]>` that prepends `T` to `List`.
- **Solution**:
```typescript
export type Prepend<T, List extends readonly unknown[]> = [T, ...List];

// Test:
type Result = Prepend<string, [number, boolean]>; // [string, number, boolean]
```

### Exercise 3 (Hard): Typed Head and Tail Extractor
- **Problem Statement**: Implement type-level `Head<T>` and `Tail<T>` utilities for tuples using conditional type pattern matching.
- **Solution**:
```typescript
export type Head<T extends readonly unknown[]> = T extends readonly [infer First, ...any[]] ? First : never;
export type Tail<T extends readonly unknown[]> = T extends readonly [any, ...(infer Rest)] ? Rest : [];

// Verification:
type H = Head<[string, number, boolean]>; // string
type T = Tail<[string, number, boolean]>; // [number, boolean]
```
