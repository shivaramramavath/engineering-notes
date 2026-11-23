# Type Testing and Static Type Assertions in TypeScript

> Comprehensive guide to testing types in TypeScript: Vitest `expectTypeOf` and `assertType`, `tsd`, `expect-type`, testing conditional and generic types, and negative compilation assertions with `@ts-expect-error`.

| Metadata | Details |
| :--- | :--- |
| **Category** | Testing and Debugging |
| **Difficulty** | Advanced |
| **Prerequisites** | [06-generics/00-generic-functions.md](../06-generics/00-generic-functions.md), [10-advanced-types/00-conditional-types.md](../10-advanced-types/00-conditional-types.md), [18-testing-and-debugging/00-unit-testing.md](./00-unit-testing.md) |
| **TypeScript Baseline** | TS 4.5+, TS 5.0+ |
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

When building complex TypeScript libraries, custom utility types, ORMs, or validation frameworks, normal runtime unit tests are insufficient. A runtime test only executes emitted JavaScript code; it cannot verify whether a returned value has the exact expected compile-time TypeScript type.

For example, consider a generic utility function:
```typescript
function deepFreeze<T>(obj: T): DeepReadonly<T> { ... }
```
A runtime unit test can assert that `Object.isFrozen(res)` is true. However, it cannot verify whether modifying a nested property `res.user.profile.age = 30` is correctly rejected with a compile-time error by TypeScript!

**Type Testing** allows developers to treat types as testable entities:
1. **Positive Type Assertions**: Proving that type $A$ exactly equals type $B$ (not just assignable, but identical in structure, modifiers, and optionality).
2. **Negative Type Assertions**: Proving that invalid arguments produce compile errors using `@ts-expect-error`.
3. **Preventing Type Regressions**: Ensuring library type changes do not unintentionally widen types to `any`, `unknown`, or `never`.

```
+-----------------------------------------------------------+
|                        Type Testing                       |
|                                                           |
|   Runtime Unit Testing:   expect(result).toBe(42);        |
|                                                           |
|   Static Type Testing:    expectTypeOf(result)            |
|                             .toEqualTypeOf<number>();     |
|                                                           |
|   Compiler Negative Test: // @ts-expect-error TS2322      |
|                           result = "invalid";             |
+-----------------------------------------------------------+
```

---

## 2. Core Concepts

### 1. `expectTypeOf` (Vitest & `expect-type`)
Checks type relationships at compile time without executing any runtime code:
```typescript
import { expectTypeOf } from 'vitest';

expectTypeOf(123).toBeNumber();
expectTypeOf({ id: '1' }).toMatchTypeOf<{ id: string }>();
expectTypeOf<string>().not.toBeAny();
```

### 2. `toEqualTypeOf` vs `toMatchTypeOf`
- **`toMatchTypeOf` (Subtyping / Assignability)**: Asserts that $A$ is assignable to $B$ ($A \subseteq B$). `{ id: string, name: string }` matches `{ id: string }`.
- **`toEqualTypeOf` (Strict Identity)**: Asserts that $A$ and $B$ are completely identical in shape, property modifiers (`readonly`), and optionality (`?`).

### 3. Negative Compilation Testing (`@ts-expect-error`)
Asserts that the subsequent line must fail compilation. If the line compiles without error, TypeScript flags the `@ts-expect-error` directive with `TS2578: Unused '@ts-expect-error' directive`.

---

## 3. How It Works Internally

How does `expectTypeOf` check if two types $A$ and $B$ are strictly identical without being fooled by `any`?

In TypeScript, `any` is assignable to anything, and anything is assignable to `any`. The only mathematical definition of type identity that distinguishes `any` from other types relies on conditional type distribution over functions:
```typescript
export type IsIdentical<T, U> =
  (<G>() => G extends T ? 1 : 2) extends
  (<G>() => G extends U ? 1 : 2) ? true : false;
```
Because the compiler cannot distribute `any` over the higher-order function generic parameter `G`, this conditional type evaluates to `false` if one type is `any` and the other is `unknown` or `never`.

---

## 4. Syntax & API Reference

### Vitest `expectTypeOf` Matchers
```typescript
expectTypeOf<Actual>().toEqualTypeOf<Expected>();     // Strict identity
expectTypeOf<Actual>().toMatchTypeOf<Expected>();      // Subtyping
expectTypeOf<Actual>().toBeString();
expectTypeOf<Actual>().toBeNumber();
expectTypeOf<Actual>().toBeBoolean();
expectTypeOf<Actual>().toBeArray();
expectTypeOf<Actual>().toBeFunction();
expectTypeOf<Actual>().toBeAny();
expectTypeOf<Actual>().toBeUnknown();
expectTypeOf<Actual>().toBeNever();
expectTypeOf<Actual>().toBeNull();
expectTypeOf<Actual>().toBeUndefined();
expectTypeOf<Actual>().parameter(0).toEqualTypeOf<string>();
expectTypeOf<Actual>().returns.toEqualTypeOf<Promise<void>>();
```

---

## 5. Practical Examples

### Example 1: Basic Positive Type Assertions on Return Types
```typescript
import { describe, it, expectTypeOf } from 'vitest';

export function pluckId<T extends { id: string }>(item: T): T['id'] {
  return item.id;
}

describe('pluckId typing', () => {
  it('infers exact string return type', () => {
    const res = pluckId({ id: 'user_123', name: 'Alice' });
    expectTypeOf(res).toEqualTypeOf<string>();
    expectTypeOf(res).not.toBeAny();
  });
});
```

### Example 2: Real-World Testing of Complex Conditional Utility Types
```typescript
export type Flatten<T> = T extends Array<infer U> ? Flatten<U> : T;

describe('Flatten utility type', () => {
  it('recursively flattens nested arrays', () => {
    expectTypeOf<Flatten<number[][][]>>().toEqualTypeOf<number>();
    expectTypeOf<Flatten<string[]>>().toEqualTypeOf<string>();
    expectTypeOf<Flatten<boolean>>().toEqualTypeOf<boolean>();
  });
});
```

### Example 3: Edge Case - Function Signature Testing (Parameters and Return)
```typescript
type ApiHandler = (req: Request, retryCount?: number) => Promise<Response>;

describe('ApiHandler signature', () => {
  it('verifies parameter and return types', () => {
    expectTypeOf<ApiHandler>().parameters.toEqualTypeOf<[Request, number?]>();
    expectTypeOf<ApiHandler>().returns.toEqualTypeOf<Promise<Response>>();
  });
});
```

### Example 4: Failure Case - Negative Compilation Assertion with Compiler Codes
```typescript
interface StrictConfig {
  readonly apiKey: string;
  readonly timeoutMs: number;
}

const config: StrictConfig = {
  apiKey: 'secret_123',
  timeoutMs: 5000
};

// @ts-expect-error TS2540: Cannot assign to 'apiKey' because it is a read-only property.
config.apiKey = 'mutated';
```

### Example 5: Idiomatic Zero-Runtime Pure Type Assertion Function
```typescript
export function assertType<T>(_value: T): void {}

type IsEqual<A, B> = (<G>() => G extends A ? 1 : 2) extends (<G>() => G extends B ? 1 : 2) ? true : false;

export function assertTypeEqual<A, B>(_proof: IsEqual<A, B> extends true ? true : never): void {}

// Usage:
assertTypeEqual<Flatten<string[]>, string>(true);
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **`toMatchTypeOf` Subtyping Trap**:
   `expectTypeOf<{ id: string, extra: number }>().toMatchTypeOf<{ id: string }>()` passes because the first object is a structural subtype of the second. If you intended to assert that `{ extra: number }` does NOT exist, you must use `toEqualTypeOf()`.
2. **The `any` Contamination Trap**:
   If a complex generic resolves to `any`, a naive `expectTypeOf<T>().toMatchTypeOf<string>()` passes because `any` is assignable to `string`! Always assert `expectTypeOf<T>().not.toBeAny()`.
3. **`@ts-ignore` vs `@ts-expect-error`**:
   Never use `@ts-ignore` in type tests! `@ts-ignore` suppresses any error, and continues suppressing even if future refactoring fixes the error. `@ts-expect-error` fails the build if no error occurs, ensuring the negative test remains active.
4. **Branded Types and Nominal Identity**:
   Branded types `type UserId = string & { __brand: 'UserId' }` will fail `toEqualTypeOf<string>()`, exactly as intended.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Code | Good Code | Compiler / Runtime Impact |
| :--- | :--- | :--- | :--- |
| **Using `@ts-ignore` in Tests** | `// @ts-ignore` | `// @ts-expect-error TS2322` | Hides errors silently when code is fixed |
| **Assignability Instead of Equality** | `toMatchTypeOf` for exact shape | `toEqualTypeOf` | Fails to detect unwanted extra properties |
| **Ignoring `any` Leakage** | Assuming passing type test means safe | Assert `.not.toBeAny()` explicitly | Type checks pass because type is actually `any` |
| **Runtime Assertion on Types** | `expect(typeof x).toBe('string')` | `expectTypeOf(x).toBeString()` | Runtime tests only check primitives, not complex types |
| **Testing with `as` Assertions** | `fn(arg as any)` | Pass real typed fixtures | Tests invalid types and masks compiler errors |
| **Missing Negative Tests** | Only testing what works | Test invalid inputs with `@ts-expect-error` | Allows unsafe API inputs to compile undetected |

---

## 8. Performance & Type-Checker Complexity

- **Execution Overhead**: `expectTypeOf` assertions emit **zero bytes of JavaScript**. They are evaluated entirely by the TypeScript compiler type-checker during type checking and are removed from runtime test bundles.
- **Compiler Time**: Type tests with hundreds of deep generic instantiations can increase `tsc` check times. Keep type tests in separate `*.test-d.ts` files if they become large.

---

## 9. Compiler Internals

When TypeScript processes `@ts-expect-error`:
The compiler checks if a diagnostic is produced on the following line. If a diagnostic exists, it is suppressed. If NO diagnostic is produced, the compiler reports `TS2578: Unused '@ts-expect-error' directive`.

---

## 10. Best Practices & Design Guidelines

1. **Use `expectTypeOf` in Vitest**: Take advantage of built-in type testing without extra dependencies.
2. **Always Use `@ts-expect-error` with Error Codes**: Document the expected compiler code (`// @ts-expect-error TS2345`).
3. **Assert `.not.toBeAny()` on Generics**: Ensure type inference didn't silently degrade to `any`.
4. **Colocate Type Tests with Unit Tests**: Keep type tests in `*.test.ts` or dedicated `*.test-d.ts` files.

---

## 11. Real-World Case Study

### Type-Safe Event Bus Library Verification
Testing that an event bus library correctly narrows payloads and disallows unregistered event names:

```typescript
export interface AppEvents {
  'user:created': { id: string; email: string };
  'order:placed': { orderId: string; amountCents: number };
}

export declare function emitEvent<K extends keyof AppEvents>(
  event: K,
  payload: AppEvents[K]
): void;

describe('emitEvent type safety', () => {
  it('accepts valid event and exact payload', () => {
    expectTypeOf(emitEvent).toBeFunction();
    expectTypeOf(emitEvent<'user:created'>).parameter(1).toEqualTypeOf<{ id: string; email: string }>();
  });

  it('rejects invalid payloads and unknown event names', () => {
    // @ts-expect-error TS2345: Argument of type '"unknown:event"' is not assignable to parameter of type 'keyof AppEvents'.
    emitEvent('unknown:event', {});

    // @ts-expect-error TS2345: Property 'email' is missing in type '{ id: string; }'.
    emitEvent('user:created', { id: 'u_1' });
  });
});
```

---

## 12. Interview Questions

<details>
<summary>1. What is the difference between `expectTypeOf` and standard `expect()` in Vitest?</summary>

`expect()` performs runtime assertions during test execution, evaluating real JavaScript values. `expectTypeOf` performs compile-time assertions evaluated by the TypeScript type checker, verifying types without executing runtime code.
</details>

<details>
<summary>2. Why should developers use `@ts-expect-error` instead of `@ts-ignore` for negative type tests?</summary>

`@ts-ignore` suppresses compiler diagnostics unconditionally. If a future refactoring or TypeScript update fixes the bug or changes the API such that no error occurs, `@ts-ignore` remains silent. `@ts-expect-error` requires an error; if no error occurs, the compiler throws `TS2578: Unused '@ts-expect-error' directive`, failing CI and alerting developers that the test expectation is obsolete.
</details>

<details>
<summary>3. How does TypeScript test if two types are strictly equal rather than merely assignable?</summary>

By testing whether function generic distribution behaves identically for both types:
```typescript
type IsIdentical<A, B> = (<T>() => T extends A ? 1 : 2) extends (<T>() => T extends B ? 1 : 2) ? true : false;
```
This distinguishes `any` from `unknown`, `never`, and primitive types.
</details>

<details>
<summary>4. What is the difference between `toEqualTypeOf` and `toMatchTypeOf`?</summary>

`toMatchTypeOf` tests assignability ($A \subseteq B$). It allows excess properties on $A$. `toEqualTypeOf` tests strict identity: $A$ and $B$ must have the exact same properties, types, optionality flags, and `readonly` qualifiers.
</details>

<details>
<summary>5. What are `*.test-d.ts` files and how does `tsd` use them?</summary>

`*.test-d.ts` files are declaration files used exclusively for type testing. Tools like `tsd` parse these files with the TypeScript compiler, verifying that `expectType<T>()` and `expectError()` assertions hold without emitting JavaScript.
</details>

<details>
<summary>6. Why does `any` pass assignability checks to any other type, and how does that affect type tests?</summary>

In TypeScript, `any` is both a top type and a bottom type. It is assignable to everything and everything is assignable to it. If a generic returns `any`, tests like `expectTypeOf(x).toMatchTypeOf<string>()` pass erroneously. Explicit checks like `.not.toBeAny()` must be used.
</details>

<details>
<summary>7. How do you test whether a property is optional (`?`) versus nullable (`| undefined`)?</summary>

Using strict type equality (`toEqualTypeOf`): `{ a?: number }` is NOT strictly identical to `{ a: number | undefined }` because the latter requires the key `'a'` to exist on the object literal, whereas the former allows the key to be omitted entirely.
</details>

<details>
<summary>8. How do you verify that a function parameter is inferred contravariantly?</summary>

By extracting the parameter type using `Parameters<Fn>[Index]` and asserting strict equality against the expected type.
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Exercise 1 (Easy): Basic `toBeString` Type Assertion</summary>

**Problem:** Write a type assertion verifying that `toUpperCase()` returns a string.

**Solution:**
```typescript
import { expectTypeOf } from 'vitest';

const str = 'hello'.toUpperCase();
expectTypeOf(str).toBeString();
```
</details>

<details>
<summary>Exercise 2 (Easy): Negative Compile Test on Constant Reassignment</summary>

**Problem:** Write an `@ts-expect-error` negative test verifying that `const x = 5` cannot be reassigned.

**Solution:**
```typescript
const x = 5;
// @ts-expect-error TS2588: Cannot assign to 'x' because it is a constant.
x = 10;
```
</details>

<details>
<summary>Exercise 3 (Medium): Testing DeepReadonly Utility Type</summary>

**Problem:** Test a recursive `DeepReadonly<T>` utility type, asserting that nested properties cannot be mutated.

**Solution:**
```typescript
type DeepReadonly<T> = { readonly [K in keyof T]: DeepReadonly<T[K]> };

type User = { profile: { name: string } };
type FrozenUser = DeepReadonly<User>;

declare const u: FrozenUser;
// @ts-expect-error TS2540: Cannot assign to 'name' because it is a read-only property.
u.profile.name = 'Bob';
```
</details>

<details>
<summary>Exercise 4 (Medium): Function Parameter Type Extraction Test</summary>

**Problem:** Use `expectTypeOf` to assert that the second argument of `setTimeout` is of type `number | undefined`.

**Solution:**
```typescript
import { expectTypeOf } from 'vitest';

expectTypeOf<typeof setTimeout>().parameter(1).toMatchTypeOf<number | undefined>();
```
</details>

<details>
<summary>Exercise 5 (Hard): Pure Type Equality Conditional Type</summary>

**Problem:** Implement a pure type `AssertStrictEqual<A, B>` that yields `true` only if $A$ and $B$ are strictly identical (rejecting `any` vs `number`), and `false` otherwise.

**Solution:**
```typescript
export type AssertStrictEqual<A, B> =
  (<T>() => T extends A ? 1 : 2) extends
  (<T>() => T extends B ? 1 : 2) ? true : false;

// Verifications:
type Test1 = AssertStrictEqual<string, string>; // true
type Test2 = AssertStrictEqual<any, number>;    // false
type Test3 = AssertStrictEqual<never, never>;   // true
```
</details>

<details>
<summary>Exercise 6 (Hard): Testing Generic Higher-Order Function Return Inferences</summary>

**Problem:** Write a type test verifying that a `curry` function returns the exact expected chained function types for a 3-argument function.

**Solution:**
```typescript
import { expectTypeOf } from 'vitest';

declare function curry<A, B, C, R>(fn: (a: A, b: B, c: C) => R): (a: A) => (b: B) => (c: C) => R;

const sum = (a: number, b: string, c: boolean) => [a, b, c];
const curried = curry(sum);

expectTypeOf(curried)
  .toEqualTypeOf<(a: number) => (b: string) => (c: boolean) => (string | number | boolean)[]>();
```
</details>
