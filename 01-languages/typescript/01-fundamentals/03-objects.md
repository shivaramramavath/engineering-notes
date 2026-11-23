# Object Types, Properties & Index Signatures in TypeScript

> A deep architectural guide to object types in TypeScript, covering property modifiers (`readonly`, `?`), index signatures, mapped types, record utilities, and excess property checks.

| Metadata | Value |
| :--- | :--- |
| **Level** | Beginner / Intermediate |
| **Prerequisites** | [00-types-and-inference.md](file:///e:/Learning/engineering-notes/01-languages/typescript/01-fundamentals/00-types-and-inference.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 5.0+ / TS 7.0 Native |
| **Tested tsconfig** | `strict: true`, `exactOptionalPropertyTypes: true`, `noUncheckedIndexedAccess: true` |

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
Objects are the primary data encapsulation primitive in JavaScript. Without a static type system, functions accessing missing keys fail silently with `undefined` or crash with `TypeError: Cannot read properties of undefined`. TypeScript provides expressive object type modeling: exact property typings, optional modifiers, read-only guarantees, and dynamic dictionary index signatures.

### Historical Context
Early TypeScript relied heavily on open index signatures (`{ [key: string]: any }`), which defeated type safety. Over subsequent versions, TypeScript introduced `readonly` property modifiers, `Record<K, T>`, and `exactOptionalPropertyTypes` to distinguish between missing properties and properties explicitly assigned `undefined`.

### Real-World Relevance
Accurate object modeling prevents bugs in API serialization, database DTO mappings, configuration trees, and complex nested UI component props.

---

## 2. Core Concepts

### Property Modifiers
1. **`readonly`**: Prevents reassignment after object construction.
2. **`?` (Optional)**: Indicates the property may be omitted from the object.
3. **Index Signatures**: Allows dynamic keys while typing all values.

```typescript
interface UserProfile {
  readonly id: string;           // Cannot be mutated
  username: string;              // Mutable
  bio?: string;                  // Optional
  [metadataKey: string]: unknown; // Dynamic index signature
}
```

---

## 3. How It Works Internally

### Shape Compatibility & Structural Assignability
When verifying if object `A` is assignable to object type `B`, the Type Checker loops through all properties defined in `B`. For each property `P`, it checks:
1. Does `P` exist on `A`?
2. Is the type of `A[P]` a subtype of `B[P]`?

```
Target Type: { id: string; name: string }
Source Type: { id: string; name: string; age: number }
                  │
                  ▼ Checks 'id' (compatible)
                  ▼ Checks 'name' (compatible)
                  ▼ Result: Assignable! (Extra properties permitted on non-fresh objects)
```

---

## 4. Syntax & API Reference

### Index Signatures & Record Types

```typescript
// 1. String Index Signature
interface Dictionary<T> {
  [key: string]: T;
}

// 2. Numeric Index Signature (Array-like)
interface NumericList<T> {
  [index: number]: T;
}

// 3. Built-in Record Utility
type RouteMap = Record<'home' | 'about' | 'contact', string>;

// 4. exactOptionalPropertyTypes (tsconfig)
interface Config {
  timeout?: number; // Requires number or omission; NOT undefined!
}
```

---

## 5. Practical Examples

### Example 1: Readonly Object Immutability
- **Goal:** Model an immutable configuration object.
- **Code:**
```typescript
interface DatabaseConfig {
  readonly host: string;
  readonly port: number;
  readonly ssl: boolean;
}

const db: DatabaseConfig = {
  host: 'db.internal.net',
  port: 5432,
  ssl: true
};

// @ts-expect-error TS2540: Cannot assign to 'port' because it is a read-only property.
db.port = 3306;
```
- **Explanation:** `readonly` prevents accidental mutation of configuration settings at compile time.

### Example 2: Safe Dynamic Dictionary with `noUncheckedIndexedAccess`
- **Goal:** Safely read dynamic dictionary properties.
- **Code:**
```typescript
type CacheStore = Record<string, string>;

const cache: CacheStore = {
  session_1: 'user_active'
};

// Under noUncheckedIndexedAccess: true, lookup is: string | undefined
const cachedVal = cache['session_999'];

if (cachedVal !== undefined) {
  console.log('Cache hit:', cachedVal.toUpperCase());
} else {
  console.log('Cache miss.');
}
```
- **Output:**
```
Cache miss.
```

### Example 3: Nested Object Partial Updates
- **Goal:** Author an update utility supporting partial nested configurations.
- **Code:**
```typescript
interface Settings {
  theme: 'light' | 'dark';
  notifications: {
    email: boolean;
    sms: boolean;
  };
}

function updateSettings(current: Settings, patch: Partial<Settings>): Settings {
  return {
    ...current,
    ...patch,
    notifications: {
      ...current.notifications,
      ...(patch.notifications || {})
    }
  };
}

const current: Settings = { theme: 'light', notifications: { email: true, sms: false } };
const updated = updateSettings(current, { theme: 'dark' });
console.log('Updated theme:', updated.theme);
```
- **Output:**
```
Updated theme: dark
```

### Example 4: Negative Test — `exactOptionalPropertyTypes`
- **Goal:** Demonstrate rejection of explicit `undefined` on optional fields.
- **Code:**
```typescript
interface RequestOptions {
  timeout?: number;
}

// With exactOptionalPropertyTypes: true
// @ts-expect-error TS2379: Type 'undefined' is not assignable to type 'number'
const req: RequestOptions = {
  timeout: undefined
};
```
- **Real Compiler Error:**
```
error TS2379: Type 'undefined' is not assignable to type 'number' with 'exactOptionalPropertyTypes: true'. Consider adding 'undefined' to the type of the target.
```

### Example 5: Symbol-Indexed Properties
- **Goal:** Model private symbol metadata on objects.
- **Code:**
```typescript
const METADATA_KEY = Symbol('metadata');

interface SecurePayload {
  data: string;
  [METADATA_KEY]: { signature: string; timestamp: number };
}

const payload: SecurePayload = {
  data: 'confidential',
  [METADATA_KEY]: {
    signature: 'sig_abc123',
    timestamp: Date.now()
  }
};

console.log('Payload data:', payload.data);
console.log('Has metadata:', Boolean(payload[METADATA_KEY]));
```
- **Output:**
```
Payload data: confidential
Has metadata: true
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **`readonly` is Shallow**: Marking an object property as `readonly` (`readonly user: User`) does **not** make `user.name` readonly! Nested properties remain mutable unless wrapped in `Readonly<T>` recursively.
2. **String Index Signature Coercion**: A numeric index signature (`[n: number]: T`) must always return a subtype of the string index signature (`[s: string]: U`), because JavaScript coerces number keys into strings at runtime.
3. **`Object.keys()` Return Type**: In TypeScript, `Object.keys(obj)` returns `string[]`, **not** `(keyof typeof obj)[]`! This is an intentional design choice because structural subtyping allows objects to have extra properties at runtime that were not declared in the type.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Real Problem | Idiomatic Solution | Why It Matters |
| :--- | :--- | :--- | :--- |
| `{ [key: string]: any }` | Disables type safety across the entire object | Use `Record<string, unknown>` | Forces narrowing on property reads |
| Assuming `readonly` prevents runtime mutation | `readonly` is stripped at compile-time | Pair with `Object.freeze()` | Enforces runtime immutability |
| Forgetting `exactOptionalPropertyTypes` | Allows `{ prop: undefined }` which breaks `'prop' in obj` checks | Enable `exactOptionalPropertyTypes: true` | Distinguishes between omitted vs undefined |
| Assuming `Object.keys()` returns `keyof T` | Unsound type assertions on runtime keys | Use custom typed helper or type guard | Prevents crashes if object has excess keys |

---

## 8. Performance & Type-Checker Complexity

- **Wide Index Signatures**: Having an object type with multiple conflicting index signatures forces the `Checker` to evaluate every member against the signature, slowing down compilation.
- **Deep Partial Trees**: Deep recursive mapped types over complex objects (like DOM nodes or ASTs) trigger maximum recursion depth errors (`TS2589`).

---

## 9. Compiler Internals

In TypeScript's AST, object types are represented by `ts.SyntaxKind.TypeLiteral` or `ts.SyntaxKind.InterfaceDeclaration`. Each member is an individual `ts.TypeElement`:
- `ts.PropertySignature`
- `ts.MethodSignature`
- `ts.IndexSignature`

---

## 10. Best Practices & Design Guidelines

1. **Use Interfaces for Public Object Models**: Allows declaration merging and clean extension.
2. **Enable `exactOptionalPropertyTypes`**: Eliminates subtle bugs with the `in` operator.
3. **Prefer `Record<K, V>` over Open Index Signatures**: Restricts key unions to known string sets where possible.

---

## 11. Real-World Case Study

A microservice handling payments used an open index signature `interface PaymentPayload { [key: string]: any }`. A developer misspelled `amountCents` as `amountCent`, which compiled without error. In production, the payment was recorded with a missing amount of `0`, leading to revenue loss.

### Solution:
Replaced the open index signature with a strict interface:
```typescript
interface PaymentPayload {
  readonly amountCents: number;
  readonly currency: 'USD' | 'EUR';
  readonly customerId: string;
}
```
**Outcome**: Immediate compile-time error for any misspelled or missing payload properties.

---

## 12. Interview Questions

### Q1: Why does `Object.keys(obj)` return `string[]` instead of `(keyof typeof obj)[]`?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Because TypeScript uses structural subtyping. An object can satisfy type `T` while having additional runtime properties that were not declared in `T` (e.g. passing a subtype instance). If `Object.keys(obj)` were typed as `(keyof T)[]`, you could index into `T` using any returned key, which would be unsound if the key belonged to an undeclared extra property. Therefore, TypeScript safely types the return as `string[]`.
</details>

### Q2: What is the difference between `interface` and `type` for defining object shapes?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

- `interface`: Can be extended via `extends`, supports **Declaration Merging** (multiple definitions with the same name merge their properties), and is generally faster for the compiler to cache.
- `type`: Cannot be reopened for declaration merging, supports unions (`type X = A | B`), tuples, primitives, and mapped types directly.
</details>

### Q3: What does `exactOptionalPropertyTypes` enforce?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

By default in TypeScript, an optional property `foo?: string` is treated as `foo?: string | undefined`, allowing explicit assignment of `{ foo: undefined }`. However, in JavaScript, `{ foo: undefined }` has the property present (`'foo' in obj === true`), whereas `{}` does not. `exactOptionalPropertyTypes: true` mandates that the property must either be completely omitted or provide a valid string value, forbidding explicit assignment of `undefined`.
</details>

---

## 13. Exercises & Challenges

### Exercise 1 (Easy): Readonly Record Type
- **Problem Statement**: Define a type `EnvConfig` that maps string keys to readonly strings.
- **Solution**:
```typescript
export type EnvConfig = Readonly<Record<string, string>>;
```

### Exercise 2 (Medium): Deep Readonly Object Type
- **Problem Statement**: Implement a generic utility `DeepReadonly<T>` that recursively marks all properties of an object and its nested children as `readonly`.
- **Solution**:
```typescript
export type DeepReadonly<T> = {
  readonly [K in keyof T]: T[K] extends object ? DeepReadonly<T[K]> : T[K];
};

// Verification:
interface State {
  user: { name: string };
}
type ImmutableState = DeepReadonly<State>;
// ImmutableState.user.name is readonly!
```

### Exercise 3 (Hard): Typed Object Keys Helper
- **Problem Statement**: Implement a type-safe `typedKeys` function that returns `(keyof T)[]` for an exact, non-extended object.
- **Solution**:
```typescript
export function typedKeys<T extends object>(obj: T): (keyof T)[] {
  return Object.keys(obj) as (keyof T)[];
}
```
