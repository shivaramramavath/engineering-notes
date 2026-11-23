# Object vs {} vs Object Wrapper

> TypeScript features three superficially identical object types with wildly different type-theoretic semantics: `{}` represents any non-nullish value, `object` represents any non-primitive reference, and `Object` represents the legacy JavaScript prototype interface.

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [00-types-and-inference.md](file:///E:/Learning/engineering-notes/01-languages/typescript/01-fundamentals/00-types-and-inference.md), [03-objects.md](file:///E:/Learning/engineering-notes/01-languages/typescript/01-fundamentals/03-objects.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 2.2 (`object`) / TS 4.8 (`T & {}`) / TS 7.0 Native |
| **Tested tsconfig** | `strict: true`, `noImplicitAny: true`, `exactOptionalPropertyTypes: true` |

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
- **Problem Statement:** One of the most pervasive sources of developer confusion in TypeScript is the subtle distinction between `{}`, `object` (lowercase), and `Object` (uppercase). Developers expecting `{}` to mean "empty dictionary" are shocked when strings and numbers are legally assignable to `{}`.
- **Historical Context:** In early TypeScript, `Object` represented the standard JavaScript prototype object, and `{}` was an empty object literal type. Because JavaScript allows boxing primitives to access methods like `toString()`, primitives matched `Object` and `{}`. TypeScript 2.2 introduced the `object` keyword to denote strictly non-primitive types. TypeScript 4.8 overhauled intersection mechanics so that `T & {}` excludes `null` and `undefined`.
- **Real-World Relevance:** Misusing `{}` in API contracts creates security holes and runtime bugs by inadvertently accepting strings and numbers where structured records were expected. Production architectures use `object` or `Record<string, unknown>`.

---

## 2. Core Concepts
- **Formal Invariants:**
  - `{}` (Empty Object Type): Represents any value that is NOT `null` or `undefined`. Primitives (strings, numbers, booleans, symbols, bigints) ARE assignable to `{}`.
  - `object` (Lowercase Keyword): Represents any non-primitive type. Primitives are strictly REJECTED. Only functions, arrays, objects, maps, sets, and class instances are assignable.
  - `Object` (Capitalized Interface): The global interface describing properties on `Object.prototype` (`hasOwnProperty`, `toString`). In practice, behaves similarly to `{}`, but can be modified via declaration merging.
  - `Record<string, unknown>`: An object type that explicitly allows indexing with string keys, rejecting primitives and un-indexable types.
- **Mental Model:**
  - `{}`: "Anything you can safely access properties on without throwing a TypeError" (i.e. anything except null/undefined).
  - `object`: "A reference type living on the heap" (non-primitive).
  - `Object`: "The ECMAScript Object prototype instance contract."
- **Soundness vs Ergonomics:** While `{}` accepting primitives feels counterintuitive, it mirrors JavaScript runtime auto-boxing (`"hello".toString()`).

---

## 3. How It Works Internally
- **Assignability Lattice:**
  - `string` is assignable to `{}` and `Object`.
  - `string` is NOT assignable to `object`.
  - `null` and `undefined` are NOT assignable to `{}`, `object`, or `Object` (under `strictNullChecks`).
- **TypeScript 4.8 Intersection Normalization:** Intersecting an unconstrained generic `T` with `{}` (`T & {}`) filters out `null` and `undefined` without degrading other types.
- **Architectural Flow Diagram:**

```
+--------------------------------------------------------------------------+
|                      Object Types Assignability Grid                     |
|                                                                          |
|   Value               {}             object            Object            |
|   -----------------------------------------------------------            |
|   { a: 1 }            YES            YES               YES               |
|   [1, 2, 3]           YES            YES               YES               |
|   () => {}            YES            YES               YES               |
|   "hello"             YES            NO (Rejected!)    YES               |
|   42                  YES            NO (Rejected!)    YES               |
|   true                YES            NO (Rejected!)    YES               |
|   null                NO             NO                NO                |
|   undefined           NO             NO                NO                |
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & API Reference
- **Syntax Forms:**
  ```typescript
  let nonPrimitive: object;
  let nonNullish: {};
  let protoInstance: Object;
  let dictionary: Record<string, unknown>;
  ```
- **Type Operators & Standard Library:**
  - `Object.create(proto: object | null): any` (requires `object`, rejects primitives).
  - `WeakMap<K extends object, V>` (keys must be non-primitives).
  - `NonNullable<T> = T & {}` (modern TS 4.8 implementation).
- **tsconfig Dependencies:**
  - `strictNullChecks: true`: Crucial; without it, `null` and `undefined` are assignable to all three.

---

## 5. Practical Examples

### Example 1: Basic / Foundational Usage
- **Goal:** Compare assignment compatibility across `{}`, `object`, and `Object`.
- **Code:**
```typescript
let emptyCurly: {} = 'hello'; // Legal! String is non-nullish
let protoObj: Object = 123;     // Legal! Number is non-nullish

// @ts-expect-error TS2322: Type 'string' is not assignable to type 'object'.
let nonPrimString: object = 'hello';

// @ts-expect-error TS2322: Type 'number' is not assignable to type 'object'.
let nonPrimNum: object = 123;

let validNonPrim: object = { name: 'Alice' }; // Legal
let validArray: object = [1, 2, 3];           // Legal (arrays are objects)
let validFunc: object = () => 42;             // Legal (functions are objects)
```
- **Output / Type Evaluation:**
```typescript
// 'object' enforces reference types; '{}' accepts boxed primitives.
```
- **Explanation:** Only `object` strictly filters out JavaScript primitive values.

### Example 2: Real-World Enterprise Pattern (WeakMap Key Constraints)
- **Goal:** Enforce that cache keys must be garbage-collectible reference objects.
- **Code:**
```typescript
export class ObjectMetadataRegistry<K extends object, V> {
  private registry = new WeakMap<K, V>();

  setMetadata(target: K, data: V): void {
    this.registry.set(target, data);
  }

  getMetadata(target: K): V | undefined {
    return this.registry.get(target);
  }
}

const entity = { id: 'ent_01' };
const registry = new ObjectMetadataRegistry<typeof entity, { lastInspected: number }>();
registry.setMetadata(entity, { lastInspected: Date.now() });

// @ts-expect-error TS2344: Type 'string' does not satisfy the constraint 'object'.
const invalidRegistry = new ObjectMetadataRegistry<string, number>();
```
- **Output / Type Evaluation:**
```typescript
// Compile-time error TS2344 prevents using WeakMap with primitive string keys.
```
- **Explanation:** `WeakMap` keys must be objects to allow garbage collection; constraining `K extends object` prevents runtime exceptions.

### Example 3: Edge Case / Boundary Condition (TypeScript 4.8 `T & {}` NonNullable)
- **Goal:** Demonstrate non-nullish constraint via empty object intersection.
- **Code:**
```typescript
function ensureDefined<T>(val: T): T & {} {
  if (val === null || val === undefined) {
    throw new Error('Value cannot be null or undefined');
  }
  return val; // In TS 4.8+, narrowed cleanly to T & {}
}

const n = ensureDefined<string | null>('valid');
// n is typed as: string (null excluded!)

const num = ensureDefined<number | undefined>(42);
// num is typed as: number
```
- **Output / Type Evaluation:**
```typescript
// Intersecting unconstrained generic T with {} strips null and undefined.
```
- **Explanation:** In TypeScript 4.8+, `T & {}` is the canonical type-level representation of `NonNullable<T>`.

### Example 4: Negative Test / Type-Check Failure Case
- **Goal:** Demonstrate compiler error when attempting to assign `null` or read arbitrary properties on `{}`.
- **Code:**
```typescript
const bag: {} = { id: 100 };

// @ts-expect-error TS2339: Property 'id' does not exist on type '{}'.
console.log(bag.id);

// @ts-expect-error TS2322: Type 'null' is not assignable to type '{}'.
const noNull: {} = null;

// @ts-expect-error TS2322: Type 'undefined' is not assignable to type 'object'.
const noUndef: object = undefined;
```
- **Real Compiler Error:**
```
error TS2339: Property 'id' does not exist on type '{}'.
error TS2322: Type 'null' is not assignable to type '{}'.
error TS2322: Type 'undefined' is not assignable to type 'object'.
```
- **Explanation:** Although `{}` accepts an object with properties, it defines zero known properties itself.

### Example 5: Idiomatic Modern Usage (Safe Dictionary Typing with `Record<string, unknown>`)
- **Goal:** Model an unknown JSON object safely without using `{}` or `object`.
- **Code:**
```typescript
export type SafeDictionary = Record<string, unknown>;

export function hasKey<K extends string>(
  dict: SafeDictionary,
  key: K
): dict is SafeDictionary & Record<K, unknown> {
  return Object.prototype.hasOwnProperty.call(dict, key);
}

function processPayload(payload: SafeDictionary): void {
  if (hasKey(payload, 'timestamp') && typeof payload.timestamp === 'number') {
    console.log('Timestamp:', new Date(payload.timestamp).toISOString());
  }
}

processPayload({ timestamp: Date.now(), user: 'admin' });
```
- **Output / Type Evaluation:**
```typescript
// Safe dictionary lookup with zero implicit any or unsafe indexing.
```
- **Explanation:** `Record<string, unknown>` explicitly communicates a string-keyed dictionary of unknown values, whereas `object` disallows property indexing.

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **`{}` Does Not Mean Empty Object:** `const x: {} = "foo"` is 100% valid TypeScript. To mandate a truly empty object with zero properties, use `type Empty = Record<string, never>`.
2. **`object` Disallows Property Access:** Even if you initialize `const o: object = { a: 1 }`, reading `o.a` produces `TS2339: Property 'a' does not exist on type 'object'`. `object` only asserts reference identity, not structural keys.
3. **Declaration Merging with `Object`:** Because `Object` is an interface in the standard library, declaring `interface Object { custom(): void }` pollutes every object in your program.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern (Naive / Broken) | Real Compiler Error / Bug | Idiomatic Solution | Why It Matters |
| :--- | :--- | :--- | :--- |
| Typing dictionary as `{}` | Accepts strings, numbers, booleans | `Record<string, unknown>` | Prevents non-object payloads from reaching handlers |
| Using `Object` as a type annotation | Confuses global interface with reference type | Use `object` or specific interface | Avoids global prototype pollution and boxing issues |
| Trying to read properties on `object` | `TS2339: Property 'x' does not exist on type 'object'` | Use generic constraint `T extends object` | Preserves property inference on call sites |
| Assuming `{}` means zero keys | Silent type-safety bypass on primitives | `Record<string, never>` | Enforces true empty object constraint |
| Passing primitive to `Object.create()` | `TS2345: Argument of type 'string' not assignable to 'object | null'` | Pass valid object or null | Aligns with ECMAScript `Object.create` specification |
| Using `object` for JSON deserialization | Cannot index into returned value | Use `unknown` with schema validator | Enforces runtime structural verification |

---

## 8. Performance & Type-Checker Complexity
- **Fast Structural Checking:** Checking against `object` is a single bitwise check against primitive type flags (`TypeFlags.NonPrimitive`).
- **Intersection Normalization:** In TS 4.8+, `T & {}` simplifies internal intersection cache structures by collapsing null and undefined early in the type checker pipeline.
- **Isolated Declarations:** All three forms are primitive keywords or global types with zero cross-file inference overhead.

---

## 9. Compiler Internals
- **Type Flags:**
  - `object` is represented by `TypeFlags.NonPrimitive`.
  - `{}` is represented as an anonymous `ObjectType` with empty member tables (`members.size === 0`).
  - `Object` is a `Symbol` with `SymbolFlags.Interface` pointing to `lib.es5.d.ts`.
- **Assignability Checks:** In `src/compiler/checker.ts`, `isTypeAssignableTo` checks:
  ```typescript
  if (target.flags & TypeFlags.NonPrimitive) {
      return (source.flags & (TypeFlags.Primitive | TypeFlags.Null | TypeFlags.Undefined)) === 0;
  }
  ```

---

## 10. Best Practices & Design Guidelines
- **Ban `{}` and `Object` in Code Reviews:** Enable ESLint rule `@typescript-eslint/no-empty-object-type`.
- **Use `object` for Reference Type Constraints:** Use `<T extends object>` when a generic parameter must not be a primitive.
- **Use `Record<string, unknown>` for Dynamic Objects:** For key-value dictionaries, always prefer explicit record typing.

---

## 11. Real-World Case Study
- **Scenario:** A cloud infrastructure orchestrator accepted resource configuration bags typed as `config: {}`. An automation script accidentally passed a numerical resource ID `48291` instead of `{ resourceId: 48291 }`. Because `48291` is assignable to `{}`, the compiler raised no error. In production, `Object.entries(config)` evaluated to empty arrays, causing automated teardown of active database clusters.
- **Implementation:** Re-typed all configuration inputs to `Record<string, unknown>`:
  ```typescript
  // Before:
  function applyConfiguration(config: {}): void {}

  // After:
  function applyConfiguration(config: Record<string, unknown>): void {
    if (Object.keys(config).length === 0) {
      throw new Error('Configuration must be a non-empty object');
    }
  }
  ```
- **Outcome:** The compiler immediately flagged the numeric argument with `TS2345: Argument of type 'number' is not assignable to parameter of type 'Record<string, unknown>'`, preventing catastrophic cluster termination.

---

## 12. Interview Questions

### Q1: Why is `const x: {} = 123;` legal in TypeScript?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

In TypeScript, `{}` represents the empty object type, which matches any value that can have properties read from it without throwing a runtime error (i.e., any non-nullish value). Because JavaScript automatically boxes primitive numbers to `Number` instances when properties or methods like `toString()` are called, numbers satisfy the structural requirements of `{}`. Only `null` and `undefined` are not assignable to `{}` under `strictNullChecks`.
</details>

### Q2: How does the `object` type differ from `{}` and `Object`?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

- `object` (lowercase) represents any non-primitive type. Primitives (string, number, boolean, symbol, bigint) are rejected.
- `{}` represents any non-nullish value, accepting primitives.
- `Object` (uppercase) represents the JavaScript `Object.prototype` interface, also accepting primitives via auto-boxing, but subject to declaration merging.
</details>

### Q3: How do you declare a type that represents a truly empty object with zero properties?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Using `Record<string, never>`:
```typescript
type StrictlyEmptyObject = Record<string, never>;

const valid: StrictlyEmptyObject = {}; // OK
// const invalid: StrictlyEmptyObject = { a: 1 }; // Error: Type 'number' is not assignable to type 'never'
```
Any property added to the object will have its value type checked against `never`, which rejects all values.
</details>

### Q4: How does `T & {}` function in TypeScript 4.8+?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

In TypeScript 4.8+, intersection with `{}` is recognized by the compiler as an operation that removes `null` and `undefined` from `T`. If `T` is an unconstrained generic type parameter (which implicitly extends `unknown`), `T & {}` narrows it to its non-nullable component, making it the canonical definition of the `NonNullable<T>` utility type.
</details>

### Q5: Can a function be assigned to a variable of type `object`?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Yes. In JavaScript, functions are first-class objects (instances of `Function.prototype`, which inherits from `Object.prototype`). Because functions are reference types living on the heap and not primitives, they satisfy `object`.
</details>

### Q6: Why does `Object.create(proto)` require `proto` to be `object | null`?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

The ECMAScript specification for `Object.create` states that the prototype parameter must be either an Object reference or `null`. Passing a primitive (e.g. `Object.create("hello")`) throws a runtime `TypeError`. Typing the parameter as `object | null` accurately mirrors this runtime constraint.
</details>

### Q7: What is the difference between `object` and `Record<string, unknown>`?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

`object` represents any non-primitive value, but defines zero accessible properties or index signatures (you cannot write `obj[key]`). `Record<string, unknown>` defines an object shape that explicitly possesses a string index signature, permitting indexed access and property iteration.
</details>

### Q8: What ESLint rule prevents unintentional usage of `{}` and `Object`?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

The rule is `@typescript-eslint/no-empty-object-type` (in typescript-eslint v8+) or historically `@typescript-eslint/ban-types`. It warns against using `{}` and `Object` as type annotations and recommends `object`, `Record<string, unknown>`, or `Record<string, never>`.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Truly Empty Object Type (Easy)
- **Problem Statement:** Define a type `EmptyObject` that accepts `{}` but rejects objects with any properties and rejects all primitives.
- **Test Case:**
```typescript
const o1: EmptyObject = {}; // OK
// const o2: EmptyObject = { a: 1 }; // Error!
// const o3: EmptyObject = "string"; // Error!
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
type EmptyObject = Record<string, never>;
```
</details>

### Exercise 2: Strict Non-Primitive Guard (Easy)
- **Problem Statement:** Write a type guard `isNonPrimitive(val: unknown): val is object` that verifies a value is a non-nullish reference type.
- **Test Case:**
```typescript
isNonPrimitive({}); // true
isNonPrimitive(() => {}); // true
isNonPrimitive('hello'); // false
isNonPrimitive(null); // false
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function isNonPrimitive(val: unknown): val is object {
  return (typeof val === 'object' && val !== null) || typeof val === 'function';
}
```
</details>

### Exercise 3: IsNonPrimitiveType Utility (Medium)
- **Problem Statement:** Implement a type utility `IsNonPrimitive<T>` that evaluates to `true` if `T` is assignable to `object` and not a primitive, and `false` otherwise.
- **Test Case:**
```typescript
type A = IsNonPrimitive<{ a: 1 }>; // true
type B = IsNonPrimitive<string>; // false
type C = IsNonPrimitive<number>; // false
type D = IsNonPrimitive<() => void>; // true
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
type IsNonPrimitive<T> = [T] extends [never]
  ? false
  : T extends object
  ? true
  : false;
```
</details>

### Exercise 4: Dictionary Key Sanitizer (Medium)
- **Problem Statement:** Write a function `stripPrototypeKeys` that copies an unknown object into a null-prototype dictionary (`Object.create(null)`), stripping prototype pollution vectors.
- **Test Case:**
```typescript
const clean = stripPrototypeKeys({ a: 1, b: 2 });
// clean has no prototype (clean.__proto__ is undefined)
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function stripPrototypeKeys(obj: Record<string, unknown>): Record<string, unknown> {
  const clean = Object.create(null);
  for (const [key, value] of Object.entries(obj)) {
    if (key !== '__proto__' && key !== 'constructor' && key !== 'prototype') {
      clean[key] = value;
    }
  }
  return clean;
}
```
</details>

### Exercise 5: Compile-Time Primitive Disallow Utility (Hard)
- **Problem Statement:** Write a generic function wrapper `createStore<T>` where `T` is constrained such that passing any primitive type (including string literal unions or numbers) causes a compile-time descriptive type error.
- **Test Case:**
```typescript
createStore({ count: 0 }); // OK
// createStore("hello"); // Compile error!
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
type EnsureObject<T> = T extends object
  ? T
  : 'Error: Store state must be a non-primitive object';

function createStore<T extends object>(
  initialState: T & EnsureObject<T>
): { getState(): T } {
  let state = initialState;
  return {
    getState: () => state,
  };
}
```
</details>

### Exercise 6: Deep Object-Only Filter (Hard)
- **Problem Statement:** Write a utility type `OnlyObjectLeaves<T>` that strips all properties from an object whose values are primitives, keeping only nested object properties.
- **Test Case:**
```typescript
type Schema = {
  id: string;
  name: string;
  meta: { version: number };
  nested: { inner: { tag: string } };
};

type Result = OnlyObjectLeaves<Schema>;
// { meta: { version: number }; nested: { inner: { tag: string } } }
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
type OnlyObjectLeaves<T> = {
  [K in keyof T as T[K] extends object
    ? T[K] extends Function | Array<any>
      ? never
      : K
    : never]: T[K];
};
```
</details>
