# Common Mistakes and Anti-Patterns in TypeScript

> Detailed catalog of pervasive TypeScript anti-patterns: boolean flag proliferation, structural type traps, mutating readonly structures, accidental `any` leaks, and real compiler diagnostic errors.

| Metadata | Details |
| :--- | :--- |
| **Category** | Best Practices |
| **Difficulty** | Intermediate to Advanced |
| **Prerequisites** | [01-fundamentals/01-primitive-types.md](../01-fundamentals/01-primitive-types.md), [02-interfaces-and-types/00-interfaces.md](../02-interfaces-and-types/00-interfaces.md) |
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

TypeScript is designed to catch software bugs before code reaches production. However, developers migrating from JavaScript or statically typed languages like Java/C# frequently bring habits that undermine the type checker:
- Treating TypeScript interfaces like runtime validation schemas.
- Using type assertions (`as`) as quick fixes to silence compiler warnings.
- Misunderstanding structural subtyping (assigning extra properties without realizing excess property checks only apply to fresh object literals).
- Leaking `any` across public library boundaries.

Eliminating these anti-patterns transforms a fragile codebase into an unyielding, compile-time verified architecture.

---

## 2. Core Concepts

### The Excess Property Check Trap
Excess Property Checking (EPC) occurs **only** on fresh object literals directly passed to a function or assigned to a typed variable. If an object is first assigned to an intermediate variable, EPC is bypassed due to structural subtyping.

```
+-------------------------------------------------------------------------+
|                    Excess Property Check Behavior                       |
+-------------------------------------------------------------------------+
|                                                                         |
|  interface Point { x: number; y: number; }                              |
|                                                                         |
|  Case A: Fresh Literal Assignment                                       |
|  const p: Point = { x: 1, y: 2, z: 3 };                                 |
|  Result: COMPILE ERROR (Object literal may only specify known props)    |
|                                                                         |
|  Case B: Intermediate Variable (Structural Subtyping)                   |
|  const raw = { x: 1, y: 2, z: 3 };                                      |
|  const p: Point = raw;                                                  |
|  Result: COMPILES CLEANLY! (raw has x and y; extra prop z is ignored)   |
|                                                                         |
+-------------------------------------------------------------------------+
```

### The Readonly Mutation Trap
TypeScript's `readonly` modifier is a compile-time assertion. It does **not** make objects immutable at runtime (unlike `Object.freeze()`). Aliasing a readonly object to a mutable type allows runtime modifications.

---

## 3. How It Works Internally

1. **Freshness Tracking**: During AST parsing, object literals are marked with the internal compiler flag `ts.TypeFlags.FreshObjectLiteralType`.
2. **Assignability Check**: When assigning a fresh literal, `ts.TypeChecker` checks `hasExcessProperties()`. Once assigned or passed through an intermediate reference, the freshness flag is cleared.

---

## 4. Syntax & API Reference

### Deep Immutability via `Readonly` and `as const`
```typescript
// Shallow readonly: nested properties are still mutable!
interface Config {
  readonly database: { host: string };
}

// Deep immutable pattern
export type DeepReadonly<T> = {
  readonly [K in keyof T]: T[K] extends object ? DeepReadonly<T[K]> : T[K];
};
```

---

## 5. Practical Examples

### Example 1: Excess Property Check Bypass
```typescript
interface UserProfile {
  name: string;
  email: string;
}

function saveUser(profile: UserProfile): void {
  console.log(`Saved: ${profile.name}`);
}

// Fails EPC as expected:
// saveUser({ name: "Alice", email: "a@test.com", role: "admin" });

// Anti-pattern bypass:
const data = { name: "Alice", email: "a@test.com", role: "admin" };
saveUser(data); // Compiles! Extra property `role` leaks through silently.
```

### Example 2: Readonly Shallow Mutation
```typescript
interface Settings {
  readonly theme: {
    color: string;
  };
}

const settings: Settings = {
  theme: { color: "blue" },
};

// settings.theme = { color: "red" }; // Error: Cannot assign to 'theme' because it is a read-only property.

// Silent anti-pattern: Mutating nested property is permitted!
settings.theme.color = "red"; // Runtime mutation succeeded!

// Fix: Use as const or DeepReadonly
const secureSettings = {
  theme: { color: "blue" },
} as const;
// secureSettings.theme.color = "red"; // Error!
```

### Example 3: Edge Case - Structural Matching of Empty Objects `{}`
```typescript
// In TypeScript, `{}` represents any non-nullish value!
function handleData(val: {}) {
  console.log(val);
}

handleData("hello"); // Compiles! (string is non-nullish)
handleData(12345);   // Compiles! (number is non-nullish)

// Fix: Use Record<string, unknown> or object to enforce real dictionary objects
function handleRealObject(val: Record<string, unknown>) {
  console.log(val);
}
```

### Example 4: Compiler Error & Diagnostic Breakdown
```typescript
interface StrictConfig {
  endpoint: string;
  timeoutMs: number;
}

// @ts-expect-error - TS2353: Object literal may only specify known properties, and 'retries' does not exist in type 'StrictConfig'.
const cfg: StrictConfig = {
  endpoint: "https://api.internal",
  timeoutMs: 5000,
  retries: 3,
};
```

### Example 5: Defending Against Array Mutation
```typescript
// Anti-pattern: Mutating array passed as parameter
function badSort(numbers: number[]): number[] {
  return numbers.sort(); // Mutates the original array in place!
}

// Idiomatic: Enforce readonly arrays on pure functions
function goodSort(numbers: readonly number[]): number[] {
  return [...numbers].sort((a, b) => a - b);
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **`Function` vs `() => void`**:
   The built-in global `Function` type allows calling with arbitrary arguments and returns `any`, completely disabling type safety. Always use specific arrow function types: `(...args: unknown[]) => void`.
2. **`Object` vs `object`**:
   `Object` (capitalized) represents the JavaScript Object interface (including prototype methods). `object` (lowercase) represents any non-primitive type. Always use `object`.
3. **Optional Properties vs `| undefined`**:
   `{ prop?: string }` allows omitting the key, while `{ prop: string | undefined }` requires explicitly specifying `prop: undefined`.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Practice | Good Practice | Problem / Diagnostic |
| :--- | :--- | :--- | :--- |
| **EPC Bypass via Variable** | Assigning to intermediate `let` | Pass object literal directly | Leaks unexpected properties |
| **Shallow Readonly** | `readonly obj: { x: 1 }` | `as const` or `DeepReadonly` | Nested properties can be mutated |
| **Using `{}` for Objects** | `arg: {}` | `arg: Record<string, unknown>` | Accepts numbers and strings |
| **Using `Function` type** | `callback: Function` | `callback: () => void` | Returns `any`, bypasses checks |
| **In-place Array Sort** | `arr.sort()` on arguments | `[...arr].sort()` with `readonly T[]` | Mutates caller's data structure |
| **Overriding Return with `as`** | `return data as Target` | Specify explicit return type on fn | Masks mismatched properties |

---

## 8. Performance & Type-Checker Complexity

- Recursive utility types like `DeepReadonly<T>` incur $O(D)$ recursion depth during type evaluation.
- `as const` assertions create literal types that are cached instantly by the compiler without overhead.

---

## 9. Compiler Internals

When an object literal is assigned:
1. `ts.TypeChecker` examines `destType.members`.
2. If `sourceType.flags & ts.TypeFlags.FreshObjectLiteralType`, it verifies that every property in `sourceType` exists on `destType`.
3. If an unrecognized property is detected, the compiler emits TS2353 ("Object literal may only specify known properties").

---

## 10. Best Practices & Design Guidelines

1. **Use `readonly` Arrays on Function Arguments**: Prevents accidental mutation of caller state.
2. **Never Use `Function` or `Object`**: Use explicit function signatures and `Record<string, unknown>`.
3. **Use `as const` for Deep Immutability**: Guarantees zero-overhead compile-time frozen structures.
4. **Avoid Bypassing Excess Property Checks**: Do not use intermediate variables to smuggle untyped fields.

---

## 11. Real-World Case Study

An e-commerce order processing system stored user updates in PostgreSQL. A function updated user addresses:
```typescript
interface AddressUpdate {
  street: string;
  city: string;
  zip: string;
}

function updateAddress(userId: string, input: AddressUpdate) {
  db.update("users", userId, input); // Raw SQL update
}
```
A developer passed `req.body` through an intermediate variable containing `role: 'admin'`. Because EPC was bypassed by the intermediate assignment, the raw SQL statement updated the user's role to admin in production.

**Fix**:
Implemented strict Zod boundary parsing and typed inputs directly with fresh literal checks.

---

## 12. Interview Questions

<details>
<summary>1. What is Excess Property Checking (EPC) and when does it apply?</summary>

EPC is a compile-time check where TypeScript triggers an error if an object literal contains properties not declared in the target type. It applies *only* to fresh object literals during direct assignments or function arguments, and does not apply to intermediate variables.
</details>

<details>
<summary>2. Why does `const p: Point = { x: 1, y: 2, z: 3 }` fail, but `const raw = { x: 1, y: 2, z: 3 }; const p: Point = raw;` succeeds?</summary>

The first case is a fresh object literal subject to Excess Property Checking. In the second case, `raw` is an intermediate variable evaluated under standard structural subtyping (width subtyping): because `raw` satisfies all required properties of `Point` (`x` and `y`), the extra property `z` is allowed.
</details>

<details>
<summary>3. What is the difference between `readonly T[]` and `ReadonlyArray<T>`?</summary>

They are identical syntax for the same type. `readonly T[]` is shorthand syntax for `ReadonlyArray<T>`, which removes mutable array methods (`push`, `pop`, `splice`, `sort`).
</details>

<details>
<summary>4. Why is using the global type `Function` dangerous?</summary>

`Function` accepts any callable function, allows arbitrary arguments, and returns `any`. It disables type checking for both parameters and return values.
</details>

<details>
<summary>5. What does the type `{}` represent in TypeScript?</summary>

`{}` represents any value that is not `null` or `undefined`. This includes primitives like numbers, strings, and booleans. To represent actual object records, use `Record<string, unknown>` or `object`.
</details>

<details>
<summary>6. How does `as const` affect array and object literals?</summary>

It marks all properties as `readonly`, narrows primitives to literal types (e.g. `"active"` instead of `string`), and converts array literals into fixed-length readonly tuples.
</details>

<details>
<summary>7. Does TypeScript's `readonly` prevent mutation at runtime?</summary>

No. `readonly` is purely a compile-time check. If a readonly object is cast to a mutable type, passed to JavaScript, or aliased, it can be modified at runtime unless frozen with `Object.freeze()`.
</details>

<details>
<summary>8. What is the difference between `interface A { x?: number }` and `interface B { x: number | undefined }`?</summary>

In `A`, property `x` is optional and can be omitted entirely. In `B`, property `x` is required, meaning the key `x` must be explicitly present, even if its value is `undefined`.
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Challenge 1 (Easy): Prevent mutation on function array parameter</summary>

**Task**: Refactor `function getMin(numbers: number[])` so that TypeScript forbids calling mutating methods like `.sort()` on `numbers`.

**Solution**:
```typescript
export function getMin(numbers: readonly number[]): number {
  if (numbers.length === 0) throw new Error("Empty array");
  return Math.min(...numbers);
}
```
</details>

<details>
<summary>Challenge 2 (Easy): Replace Function type with safe signature</summary>

**Task**: Fix `function runTask(task: Function)` to accept only a no-argument asynchronous task returning void.

**Solution**:
```typescript
export function runTask(task: () => Promise<void>): Promise<void> {
  return task();
}
```
</details>

<details>
<summary>Challenge 3 (Medium): Implement DeepReadonly utility type</summary>

**Task**: Write a recursive `DeepReadonly<T>` type that makes all nested object properties readonly.

**Solution**:
```typescript
export type DeepReadonly<T> = {
  readonly [K in keyof T]: T[K] extends (...args: any[]) => any
    ? T[K]
    : T[K] extends object
      ? DeepReadonly<T[K]>
      : T[K];
};
```
</details>

<details>
<summary>Challenge 4 (Medium): Enforce Real Object Constraint</summary>

**Task**: Write a function that accepts only real dictionary objects, rejecting primitives and arrays.

**Solution**:
```typescript
export function processDictionary<T extends Record<string, unknown>>(data: T): string[] {
  if (Array.isArray(data)) throw new Error("Arrays rejected");
  return Object.keys(data);
}
```
</details>

<details>
<summary>Challenge 5 (Hard): Enforce Exact Properties (Disallow Excess Even Through Variables)</summary>

**Task**: Implement a helper type `Exact<T, Shape>` that rejects excess properties even when passed through an intermediate variable.

**Solution**:
```typescript
export type Exact<T, Shape> = T extends Shape
  ? Exclude<keyof T, keyof Shape> extends never
    ? T
    : never
  : never;

export function createExact<Shape>() {
  return <T>(obj: Exact<T, Shape>): T => obj;
}

interface User {
  id: string;
  name: string;
}

const makeUser = createExact<User>();
const raw = { id: "1", name: "Bob", extra: 123 };
// @ts-expect-error - TS2345: Argument is not assignable because extra is rejected
makeUser(raw);
```
</details>

<details>
<summary>Challenge 6 (Hard): Runtime-Enforced Deep Freeze</summary>

**Task**: Implement a `deepFreeze` function that combines compile-time `DeepReadonly<T>` typing with recursive runtime `Object.freeze()`.

**Solution**:
```typescript
export function deepFreeze<T extends object>(obj: T): DeepReadonly<T> {
  Object.freeze(obj);
  for (const key of Object.getOwnPropertyNames(obj)) {
    const val = (obj as any)[key];
    if (val && typeof val === "object" && !Object.isFrozen(val)) {
      deepFreeze(val);
    }
  }
  return obj as DeepReadonly<T>;
}
```
</details>
