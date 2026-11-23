# Deep Keys and Path Navigation

> Type-level path navigation utilities extract, validate, and navigate nested object keys using dotted strings or tuple paths, powering type-safe form libraries and lens accessors.

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [04-template-literal-types.md](file:///E:/Learning/engineering-notes/01-languages/typescript/10-advanced-types/04-template-literal-types.md), [04-indexed-access-types.md](file:///E:/Learning/engineering-notes/01-languages/typescript/06-generics/04-indexed-access-types.md) |
| **Time to Read** | 25 minutes |
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
In complex frontend applications and enterprise domain models, data structures are deeply nested:
- Form libraries (React Hook Form, Formik) access fields via dot strings (`"user.address.street"`).
- Data access libraries (Lodash `get`, optics, lenses) update nested states immutably.
- Event emitters subscribe to nested change channels.

Passing raw strings like `get(state, "user.adress.street")` traditionally bypassed TypeScript's type checker, leading to runtime `undefined` bugs when properties were refactored or misspelled.

**Path Navigation Utilities** construct exhaustive compile-time unions of all valid dotted or tuple paths through an object graph, and look up the exact type of the value at the destination path.

---

## 2. Core Concepts

### Generating All Nested Paths (`Paths<T>`)
Recursively traverses object keys and concatenates them with dots:
```typescript
interface UserProfile {
  name: string;
  address: {
    city: string;
    zipCode: number;
  };
}

type UserPaths = Paths<UserProfile>;
// "name" | "address" | "address.city" | "address.zipCode"
```

### Resolving Nested Values (`PathValue<T, P>`)
Recursively extracts the value type at the given path:
```typescript
type ZipType = PathValue<UserProfile, "address.zipCode">; // number
type CityType = PathValue<UserProfile, "address.city">;   // string
```

---

## 3. How It Works Internally

```
Type Evaluation of PathValue<T, "address.city">:
1. Split path into Head ("address") and Tail ("city")
2. Lookup Head in T:
   T["address"] ===> { city: string; zipCode: number }
3. Recurse with T = { city: string; zipCode: number }, P = "city"
4. Match Tail ("city") directly in T:
   T["city"] ===> string
5. Return string
```

In `src/compiler/checker.ts`:
- `Paths<T>` generates template literal types across mapped keys.
- `PathValue<T, P>` uses recursive conditional types with `infer` to pattern-match against the dot delimiter, performing indexed access `T[Head]` step-by-step.

---

## 4. Syntax & API Reference

### Production-Grade Deep Path Definitions with Depth Guard
```typescript
type Prev = [never, 0, 1, 2, 3, 4, 5];

export type DeepPaths<T, Depth extends number = 4> = [Depth] extends [0]
  ? never
  : T extends object
  ? {
      [K in keyof T & string]: T[K] extends object
        ? K | `${K}.${DeepPaths<T[K], Prev[Depth]>}`
        : K;
    }[keyof T & string]
  : never;

export type DeepPathValue<T, P extends string> =
  P extends `${infer Head}.${infer Tail}`
    ? Head extends keyof T
      ? DeepPathValue<T[Head], Tail>
      : never
    : P extends keyof T
    ? T[P]
    : never;
```

---

## 5. Practical Examples

### 5.1 Basic Type-Safe Nested Getter
```typescript
function get<T extends object, P extends DeepPaths<T>>(
  obj: T,
  path: P
): DeepPathValue<T, P> {
  const parts = (path as string).split(".");
  let curr: any = obj;
  for (const part of parts) {
    curr = curr?.[part];
  }
  return curr;
}

const config = {
  db: { primary: { port: 5432, host: "db.corp" } }
};

const port = get(config, "db.primary.port"); // strictly typed as number!
// get(config, "db.primary.missing"); // Compile error!
```

### 5.2 Real-World Application: Type-Safe React Form Controller
```typescript
interface RegistrationForm {
  account: {
    username: string;
    email: string;
  };
  preferences: {
    notifications: boolean;
  };
}

class FormController<T extends object> {
  constructor(private values: T) {}

  watch<P extends DeepPaths<T>>(path: P): DeepPathValue<T, P> {
    return get(this.values, path);
  }

  setValue<P extends DeepPaths<T>>(path: P, value: DeepPathValue<T, P>): void {
    console.log(`Setting ${String(path)} =`, value);
  }
}

const form = new FormController<RegistrationForm>({
  account: { username: "ada", email: "ada@corp.com" },
  preferences: { notifications: true }
});

form.setValue("account.email", "new@corp.com"); // OK
// form.setValue("account.email", 123); // Error: Argument of type 'number' not assignable to 'string'!
```

### 5.3 Edge Case: Tuple and Array Navigation
```typescript
interface OrderState {
  items: Array<{ sku: string; price: number }>;
}

// Navigating into arrays:
type OrderPaths = DeepPaths<OrderState>; // "items"
// To support numeric indices, extend path matching with `${number}`:
type ArrayIndexPath<T> = T extends (infer U)[] ? `${number}` | `${number}.${DeepPaths<U>}` : never;
```

### 5.4 Failure Case: Unchecked Infinite Recursive Model
```typescript
interface CyclicNode {
  name: string;
  parent: CyclicNode;
}

// Without a Depth guard:
// type BadPaths = DeepPaths<CyclicNode>;
// TS2589: Type instantiation is excessively deep and possibly infinite.
```

### 5.5 Idiomatic Pattern: Lens-Style Immutable Setter
```typescript
function set<T extends object, P extends DeepPaths<T>>(
  obj: T,
  path: P,
  value: DeepPathValue<T, P>
): T {
  const keys = (path as string).split(".");
  const root: any = { ...obj };
  let curr = root;
  for (let i = 0; i < keys.length - 1; i++) {
    curr[keys[i]] = { ...curr[keys[i]] };
    curr = curr[keys[i]];
  }
  curr[keys[keys.length - 1]] = value;
  return root;
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Recursion on Cyclic Types**: Self-referencing models (e.g. `parent: Node`) cause infinite loops unless capped with a depth counter (e.g. `Depth extends number = 4`).
2. **Function Properties**: Functions are objects, so unconstrained `Paths<T>` will attempt to navigate into `Function.prototype` methods (`bind`, `call`, `apply`). Always guard with `T extends Function ? never : ...`.
3. **Optional Properties**: If a property is optional (`address?: { city: string }`), navigating into it requires handling `undefined` gracefully.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Problem | Solution | Compiler Error |
| :--- | :--- | :--- | :--- |
| Omitting depth guards on path generators | Cyclic structures crash the compiler | Add tuple decrement depth counter | `TS2589: Type instantiation is excessively deep` |
| Navigating into functions | Emits `fn.bind`, `fn.call` as valid paths | Guard with `T extends Function ? never : ...` | Cluttered path suggestions |
| Not handling `Date` / `RegExp` base cases | Generates internal prototype paths | Exclude built-in non-primitive objects | Polluted path unions |
| Writing deep paths as plain `string` | Loses autocompletion and compile-time checks | Constrain to `P extends DeepPaths<T>` | Silent typo bugs |
| Re-evaluating path values in hot loops | Increases check times in wide unions | Cache path types or flatten models | Slower compilation |
| Mutating target object during set | Breaks immutability contracts | Clone objects at each path level | Shared mutation side effects |

---

## 8. Performance & Type-Checker Complexity
- Generating all deep paths across an object with 5 nested levels of 10 properties each generates thousands of union members.
- Always include a recursion depth cap (`Depth extends number = 3`) in enterprise monorepos.

---

## 9. Compiler Internals
- `DeepPaths` combines mapped types with template literal types.
- Evaluated in `src/compiler/checker.ts` through recursive template literal resolution.
- Key intersections `keyof T & string` ensure that `symbol` and `number` keys are safely omitted.

---

## 10. Best Practices & Design Guidelines
- Always bound `DeepPaths` recursion depth using a numeric tuple guard.
- Guard against `Function`, `Date`, `RegExp`, and `Blob`.
- Export both `DeepPaths<T>` and `DeepPathValue<T, P>` in shared utility packages.

---

## 11. Real-World Case Study
### Type-Safe State Path Patching in Enterprise Redux Slice
An enterprise banking frontend stored client configuration in a 5-level deep state graph. Developers frequently introduced runtime bugs when dispatching patch actions with typos:
```typescript
dispatch(updateField("user.perferences.theme", "dark")); // Silent typo: "perferences"
```
**Solution:**
Implemented `DeepPaths` and `DeepPathValue`:
```typescript
function updateField<P extends DeepPaths<RootState>>(
  path: P,
  value: DeepPathValue<RootState, P>
) {
  return { type: "PATCH_FIELD", path, value };
}
```
TypeScript immediately flagged the typo at compile time and provided autocompletion for all valid dotted paths.

---

## 12. Interview Questions

<details>
<summary>1. How do you generate all dotted property paths of an object type in TypeScript?</summary>

Using a recursive mapped type with template literals:
```typescript
type Paths<T> = T extends object
  ? { [K in keyof T & string]: T[K] extends object ? K | `${K}.${Paths<T[K]>}` : K }[keyof T & string]
  : never;
```
</details>

<details>
<summary>2. How do you extract the value type at a dotted string path?</summary>

Using pattern matching with `infer` and indexed access:
```typescript
type PathValue<T, P extends string> =
  P extends `${infer Head}.${infer Tail}`
    ? Head extends keyof T ? PathValue<T[Head], Tail> : never
    : P extends keyof T ? T[P] : never;
```
</details>

<details>
<summary>3. Why must path navigation utilities include a recursion depth limit?</summary>

To prevent compiler crashes (`TS2589`) when encountering self-referential or cyclic types (e.g. `interface Node { parent: Node }`), which would otherwise recurse infinitely.
</details>

<details>
<summary>4. Why do we intersect keys with <code>string</code> (<code>keyof T &amp; string</code>)?</summary>

Because `keyof T` can contain `number` and `symbol` keys. Template literal types cannot interpolate symbols, so intersecting with `string` safely filters the keys to string literals only.
</details>

<details>
<summary>5. How can you exclude functions from deep path generation?</summary>

By adding a guard before checking for `object`: `T extends Function ? never : T extends object ? ... : never`.
</details>

<details>
<summary>6. How does <code>lodash.get</code> typing benefit from path navigation types?</summary>

It allows `get(obj, "a.b.c")` to be strictly verified at compile time, returning the exact type of `obj.a.b.c` rather than fallback `any`.
</details>

<details>
<summary>7. What is the difference between dotted paths and tuple paths?</summary>

Dotted paths represent navigation as strings (`"a.b.c"`). Tuple paths represent navigation as arrays of keys (`["a", "b", "c"]`), which avoids string parsing but requires array notation.
</details>

<details>
<summary>8. How do optional properties affect <code>PathValue</code>?</summary>

If an intermediate property is optional (`b?: { c: string }`), `PathValue` should account for potential `undefined` by checking `NonNullable<T[Head]>`.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Single-Level Dot Path (Easy)
Write a utility `DirectPaths<T>` that generates string keys of `T`.
<details>
<summary>Solution</summary>

```typescript
type DirectPaths<T> = keyof T & string;
interface User { id: string; age: number; }
type P = DirectPaths<User>; // "id" | "age"
```
</details>

### Exercise 2: Two-Level Path Extractor (Easy)
Extract values from paths of exactly depth 2 (`"a.b"`).
<details>
<summary>Solution</summary>

```typescript
type TwoLevelValue<T, P extends string> =
  P extends `${infer K1 extends keyof T & string}.${infer K2}`
    ? K2 extends keyof T[K1]
      ? T[K1][K2]
      : never
    : never;

interface State { user: { name: string } }
type Name = TwoLevelValue<State, "user.name">; // string
```
</details>

### Exercise 3: Depth-Guarded Deep Paths (Medium)
Implement `SafePaths<T, Depth extends number = 3>`.
<details>
<summary>Solution</summary>

```typescript
type Prev = [never, 0, 1, 2, 3];

type SafePaths<T, D extends number = 3> = [D] extends [0]
  ? never
  : T extends Function
  ? never
  : T extends object
  ? { [K in keyof T & string]: T[K] extends object ? K | `${K}.${SafePaths<T[K], Prev[D]>}` : K }[keyof T & string]
  : never;
```
</details>

### Exercise 4: Tuple Path Navigator (Medium)
Implement a utility that extracts value types using tuple paths (`["user", "address", "city"]`).
<details>
<summary>Solution</summary>

```typescript
type TuplePathValue<T, Path extends readonly unknown[]> =
  Path extends readonly [infer Head, ...infer Tail]
    ? Head extends keyof T
      ? TuplePathValue<T[Head], Tail>
      : never
    : T;

interface Root { a: { b: { c: boolean } } }
type Val = TuplePathValue<Root, ["a", "b", "c"]>; // boolean
```
</details>

### Exercise 5: Strictly Typed Deep Getter Function (Hard)
Implement a function `deepGet` with complete type safety and runtime traversal.
<details>
<summary>Solution</summary>

```typescript
function deepGet<T extends object, P extends DeepPaths<T>>(
  obj: T,
  path: P
): DeepPathValue<T, P> {
  const segments = (path as string).split(".");
  let current: any = obj;
  for (const seg of segments) {
    if (current == null) return undefined as any;
    current = current[seg];
  }
  return current;
}
```
</details>

### Exercise 6: Path Filter by Value Type (Hard)
Implement a utility `PathsMatching<T, TargetValue>` that only returns dotted paths whose values are assignable to `TargetValue`.
<details>
<summary>Solution</summary>

```typescript
type PathsMatching<T, TargetValue> = {
  [K in DeepPaths<T>]: DeepPathValue<T, K> extends TargetValue ? K : never;
}[DeepPaths<T>];

interface ComplexModel {
  id: string;
  count: number;
  sub: {
    score: number;
    title: string;
  };
}

type NumericPaths = PathsMatching<ComplexModel, number>;
// "count" | "sub.score"
```
</details>
