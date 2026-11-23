# Strict Mode and Pedantic Flags in TypeScript

> Comprehensive breakdown of TypeScript's `"strict": true` family: `noImplicitAny`, `strictNullChecks`, `strictFunctionTypes`, `strictBindCallApply`, `strictPropertyInitialization`, `noImplicitThis`, `useUnknownInCatchVariables`, plus additional pedantic flags.

| Metadata | Details |
| :--- | :--- |
| **Category** | Compiler & Configuration |
| **Difficulty** | Intermediate |
| **Prerequisites** | [01-fundamentals/00-types-and-inference.md](../01-fundamentals/00-types-and-inference.md), [13-compiler-and-tsconfig/00-compiler-options.md](./00-compiler-options.md) |
| **TypeScript Baseline** | TS 2.3+ (`strict: true`), TS 4.4+ (`useUnknownInCatchVariables`) |
| **Tested tsconfig** | `"strict": true, "noUncheckedIndexedAccess": true, "exactOptionalPropertyTypes": true` |

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
Without `"strict": true`, TypeScript functions largely as a documentation linter: `null` and `undefined` are silently assignable to every type, parameters default to `any`, and uninitialized class fields compile without warnings. Turning on `"strict": true` converts TypeScript into a sound, resilient type system that catches the vast majority of common JavaScript runtime bugs at compile time.

Beyond `"strict": true`, TypeScript provides powerful **pedantic flags** (like `noUncheckedIndexedAccess` and `exactOptionalPropertyTypes`) that eliminate entire categories of undefined property access and array out-of-bounds errors.

---

## 2. Core Concepts
- **The `"strict": true` Family (Enabled Automatically)**:
  1. `noImplicitAny`: Disallows expressions and declarations with an implied `any` type when no type can be inferred.
  2. `strictNullChecks`: Makes `null` and `undefined` distinct types. They are no longer assignable to `string`, `number`, or object types.
  3. `strictFunctionTypes`: Enforces contravariant parameter comparison on function signatures instead of bivariant checking.
  4. `strictBindCallApply`: Enforces strict argument checking on built-in `call`, `bind`, and `apply` methods.
  5. `strictPropertyInitialization`: Ensures class properties are initialized in the constructor or carry a default value.
  6. `noImplicitThis`: Raises an error on `this` expressions with an implied `any` type.
  7. `useUnknownInCatchVariables`: Types catch clause variables as `unknown` instead of `any`.
  8. `alwaysStrict`: Emits `"use strict"` at the top of every generated JavaScript file.
- **Additional Pedantic Flags (NOT part of `strict: true`)**:
  - `noUncheckedIndexedAccess`: Accessing properties via index signatures (`record[key]` or `array[0]`) automatically appends `| undefined`.
  - `exactOptionalPropertyTypes`: Distinguishes between a property being omitted vs being explicitly assigned `undefined` (`{ prop?: string }` cannot be passed `{ prop: undefined }`).
  - `noImplicitReturns`: Ensures all code paths in a function return a value.
  - `noFallthroughCasesInSwitch`: Disallows falling through non-empty switch cases.

---

## 3. How It Works Internally

```
+-------------------------------------------------------------+
|                     "strict": true                          |
+------------------------------+------------------------------+
                               |
         +---------------------+---------------------+
         |                     |                     |
         v                     v                     v
 noImplicitAny         strictNullChecks      strictFunctionTypes
 (Blocks untyped       (Splits Null/Undef    (Contravariant param
  parameters)           from primitives)      subtype checking)
         |                     |                     |
         v                     v                     v
   TS7006 Error          TS2322 Error          TS2322 Error
```

When `strict` is set to `true`, the TypeScript compiler's options parser flips internal flags: `options.noImplicitAny = true`, `options.strictNullChecks = true`, etc., unless explicitly overridden in the configuration object. The Type Checker activates additional diagnostic routines during symbol resolution and assignability checks.

---

## 4. Syntax & API Reference

```json
{
  "compilerOptions": {
    "strict": true,
    "noUncheckedIndexedAccess": true,
    "exactOptionalPropertyTypes": true,
    "noImplicitReturns": true,
    "noFallthroughCasesInSwitch": true,
    "noImplicitOverride": true
  }
}
```

---

## 5. Practical Examples

### Example 1: `strictNullChecks` in Action
```typescript
function getLength(str: string | null): number {
  // Without strictNullChecks, str.length compiles fine and crashes at runtime if null!
  if (str === null) {
    return 0;
  }
  return str.length; // Safe
}
```

### Example 2: `noUncheckedIndexedAccess` (Pedantic Flag)
```typescript
const numbers = [10, 20, 30];

// With noUncheckedIndexedAccess: true
// first is typed as: number | undefined
const first = numbers[0];

if (first !== undefined) {
  console.log(first * 2); // Safe: narrowed to number
}
```

### Example 3: Edge Case - `exactOptionalPropertyTypes`
```typescript
interface UserProfile {
  name: string;
  avatarUrl?: string; // With exactOptionalPropertyTypes: cannot assign 'undefined' explicitly!
}

const p: UserProfile = {
  name: 'Alice',
  // @ts-expect-error TS2379: Type 'undefined' is not assignable to type 'string' with exactOptionalPropertyTypes.
  avatarUrl: undefined,
};

// Valid: omit the key entirely or pass a string
const validP: UserProfile = { name: 'Alice' };
```

### Example 4: Compiler Failure Case - `strictPropertyInitialization`
```typescript
class Account {
  // @ts-expect-error TS2564: Property 'balance' has no initializer and is not definitely assigned in the constructor.
  balance: number;

  constructor() {
    // Forgot to initialize balance!
  }
}
```

### Example 5: `strictFunctionTypes` Contravariance
```typescript
class Animal { name = 'animal'; }
class Dog extends Animal { bark() {} }

let handleAnimal = (a: Animal) => {};
let handleDog = (d: Dog) => {};

// Under strictFunctionTypes, function parameters are contravariant:
// A handler accepting Animal CAN handle Dog, but a handler requiring Dog CANNOT handle arbitrary Animal!
handleDog = handleAnimal; // OK: Animal handler is safe for Dog

// @ts-expect-error TS2322: Type '(d: Dog) => void' is not assignable to type '(a: Animal) => void'.
handleAnimal = handleDog;
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Methods vs Function Properties**: In TypeScript classes and interfaces, method shorthand declarations (`method(x: Dog): void`) are checked **bivariantly** for backwards compatibility with `Array<T>`. Only property signatures (`method: (x: Dog) => void`) are checked contravariantly under `strictFunctionTypes`!
2. **Definite Assignment Assertions (`!:` )**: Using `field!: string` bypasses `strictPropertyInitialization`. If the field is not actually assigned before access, runtime `TypeError` crashes occur.
3. **`noUncheckedIndexedAccess` with Array Iteration**: `for (const x of array)` correctly infers `x` as `T`, not `T | undefined`, because iteration guarantees element presence. Only numerical index access `array[i]` appends `| undefined`.
4. **Third-Party Types with Pedantic Flags**: Some older `@types` packages fail compilation when `exactOptionalPropertyTypes` is enabled. Use `"skipLibCheck": true` to protect your build.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Consequence | Correction |
| :--- | :--- | :--- |
| Disabling `strictNullChecks` to avoid `if (x)` | Million-dollar runtime `null` pointer bugs | Keep enabled; use optional chaining `?.` or guards |
| Method syntax in interfaces for callbacks | Bypasses `strictFunctionTypes` checking | Use property syntax: `cb: (x: T) => void` |
| Overusing `!` non-null assertion operator | Masks real bugs; crashes at runtime | Verify with control-flow checks |
| Accessing `array[0]` without checking undefined | Out of bounds crash when array is empty | Enable `noUncheckedIndexedAccess` |
| `{ prop?: string }` assigned `{ prop: undefined }` | Object carries own key with undefined | Use `exactOptionalPropertyTypes` or `prop?: string \| undefined` |

---

## 8. Performance & Type-Checker Complexity
Enabling `strict: true` introduces minimal compiler overhead (typically $<5\%$ compilation time increase) while eliminating runtime debugging sessions. Flags like `noUncheckedIndexedAccess` add union types (`T | undefined`) that are resolved in $\mathcal{O}(1)$ by the type checker.

---

## 9. Compiler Internals
In TypeScript's `checker.ts`:
- When checking assignability, `checkTypeAssignableTo` inspects `compilerOptions.strictNullChecks`. If false, `nullType` and `undefinedType` return true for all target types.
- Under `strictFunctionTypes`, parameter assignability switches from `isTypeRelatedTo(source, target, bivariantRelation)` to contravariant checking (`isTypeRelatedTo(target, source, contravariantRelation)`).

---

## 10. Best Practices & Design Guidelines
- Always enable `"strict": true` on new projects.
- Enable `"noUncheckedIndexedAccess": true` to eliminate array boundary bugs.
- Declare callback parameters in interfaces as properties (`fn: (arg: T) => void`) rather than methods (`fn(arg: T): void`) to benefit from contravariant type checking.
- Do not use non-null assertions (`!`) as a shortcut to appease the compiler.
- In legacy migrations, turn on strict flags incrementally: `noImplicitAny` first, then `strictNullChecks`.

---

## 11. Real-World Case Study
A financial portfolio tracker suffered intermittent crashes in production when querying asset prices. The code read `assets[0].price`. When a newly registered user had zero assets, the array was empty, causing `TypeError: Cannot read properties of undefined (reading 'price')`.

**Solution with `noUncheckedIndexedAccess`:**
The team enabled `"noUncheckedIndexedAccess": true` in `tsconfig.json`. The compiler immediately flagged 47 unsafe index accesses across the repository, forcing engineers to write safe guards:
```typescript
const primaryAsset = user.assets[0];
if (!primaryAsset) {
  return renderEmptyPortfolioState();
}
renderAsset(primaryAsset.price);
```
Production crash rates dropped to 0.

---

## 12. Interview Questions

<details>
<summary>1. What compiler flags are bundled under `"strict": true`?</summary>

`noImplicitAny`, `strictNullChecks`, `strictFunctionTypes`, `strictBindCallApply`, `strictPropertyInitialization`, `noImplicitThis`, `useUnknownInCatchVariables`, and `alwaysStrict`.
</details>

<details>
<summary>2. Why does `strictFunctionTypes` not apply to method shorthand syntax?</summary>

In JavaScript and early TypeScript, methods like `Array.prototype.push` were designed bivariantly so that `Dog[]` could be treated as a subtype of `Animal[]`. Enforcing contravariance on methods would make `Animal[]` and `Dog[]` mutually incompatible in common array workflows.
</details>

<details>
<summary>3. What is the difference between `optional?: string` and `optional?: string | undefined` under `exactOptionalPropertyTypes`?</summary>

Under `exactOptionalPropertyTypes`:
- `optional?: string`: The property can be present with a `string` value, or completely omitted. Passing `{ optional: undefined }` is a compile-time error.
- `optional?: string | undefined`: The property can be omitted, assigned a `string`, OR explicitly assigned the value `undefined`.
</details>

<details>
<summary>4. What does `noUncheckedIndexedAccess` do?</summary>

It changes the return type of index signature accesses (e.g. `arr[0]` or `record[key]`) from `T` to `T | undefined`, forcing developers to check whether the index actually exists before accessing its properties.
</details>

<details>
<summary>5. How does `strictNullChecks` prevent the "billion-dollar mistake"?</summary>

By treating `null` and `undefined` as independent types in their own right, rather than implicit subtypes of every string, number, or object type. Variables cannot be null unless explicitly annotated as a union (`string | null`).
</details>

<details>
<summary>6. What does `strictPropertyInitialization` require in classes?</summary>

It requires that every declared property in a class is assigned either an initial value at declaration, or assigned within the constructor body.
</details>

<details>
<summary>7. What does `noImplicitThis` prevent?</summary>

It raises an error whenever the `this` keyword is evaluated in a function where TypeScript cannot deduce its contextual type, preventing calls that throw `TypeError: Cannot read properties of undefined` when a method loses its binding.
</details>

<details>
<summary>8. How do you disable a single strict flag while keeping `"strict": true` enabled?</summary>

By setting `"strict": true` and then explicitly overriding the specific flag to `false`:
```json
{
  "compilerOptions": {
    "strict": true,
    "strictPropertyInitialization": false
  }
}
```
</details>

---

## 13. Exercises & Challenges

### Easy
1. **Strict Null Guard**: Fix the function `function printLength(s?: string) { return s.length; }` under `strictNullChecks`.
   <details>
   <summary>Solution</summary>

   ```typescript
   function printLength(s?: string): number {
     return s ? s.length : 0;
   }
   ```
   </details>

2. **Safe Array Access**: Safely access the third element of `const list = [1, 2]` with `noUncheckedIndexedAccess: true`.
   <details>
   <summary>Solution</summary>

   ```typescript
   const list = [1, 2];
   const item = list[2];
   const value = item ?? 0;
   ```
   </details>

### Medium
3. **Exact Optional Property Validator**: Define an interface `UpdateUserDto` under `exactOptionalPropertyTypes` that allows updating `email`, and write a function that correctly checks if `email` was passed in `dto`.
   <details>
   <summary>Solution</summary>

   ```typescript
   interface UpdateUserDto {
     name?: string;
     email?: string;
   }

   function handleUpdate(dto: UpdateUserDto) {
     if ('email' in dto && dto.email !== undefined) {
       console.log(`Updating email to: ${dto.email}`);
     }
   }
   ```
   </details>

4. **Class Constructor Initialization**: Refactor a class with 3 fields so that it satisfies `strictPropertyInitialization` without using definite assignment assertions (`!`).
   <details>
   <summary>Solution</summary>

   ```typescript
   class SessionManager {
     public readonly sessionId: string;
     public active: boolean;
     public lastPing: number;

     constructor(sessionId: string) {
       this.sessionId = sessionId;
       this.active = true;
       this.lastPing = Date.now();
     }
   }
   ```
   </details>

### Hard
5. **Strict Record Inversion with `noUncheckedIndexedAccess`**: Implement `invertRecord<K extends string, V extends string>(rec: Record<K, V>): Record<V, K | undefined>` that strictly tracks potential missing keys when reversing dictionary mappings.
   <details>
   <summary>Solution</summary>

   ```typescript
   export function invertRecord<K extends string, V extends string>(
     rec: Record<K, V>
   ): Record<V, K | undefined> {
     const result = {} as Record<V, K | undefined>;
     for (const [key, value] of Object.entries(rec)) {
       result[value as V] = key as K;
     }
     return result;
   }
   ```
   </details>

6. **Contravariant Callback Registry**: Author an event subscriber class `EventHub<Events extends Record<string, unknown>>` that enforces strict contravariant handler callbacks on `.on(event, handler)` using function property syntax rather than method syntax.
   <details>
   <summary>Solution</summary>

   ```typescript
   export class EventHub<Events extends Record<string, unknown>> {
     private listeners: {
       [K in keyof Events]?: Array<{ handler: (payload: Events[K]) => void }>;
     } = {};

     on<K extends keyof Events>(
       event: K,
       handler: (payload: Events[K]) => void
     ): void {
       const list = this.listeners[event] ?? [];
       list.push({ handler });
       this.listeners[event] = list;
     }

     emit<K extends keyof Events>(event: K, payload: Events[K]): void {
       const list = this.listeners[event];
       if (list) {
         for (const listener of list) {
           listener.handler(payload);
         }
       }
     }
   }
   ```
   </details>
