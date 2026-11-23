# Null and Undefined

> `null` and `undefined` represent intentional absence and uninitialized state respectively; with `strictNullChecks` and `exactOptionalPropertyTypes`, TypeScript eliminates "the billion-dollar mistake" through compile-time non-nullable enforcement.

| Metadata | Value |
| :--- | :--- |
| **Level** | Beginner / Intermediate |
| **Prerequisites** | [00-types-and-inference.md](file:///E:/Learning/engineering-notes/01-languages/typescript/01-fundamentals/00-types-and-inference.md), [01-primitive-types.md](file:///E:/Learning/engineering-notes/01-languages/typescript/01-fundamentals/01-primitive-types.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 2.0 (`strictNullChecks`) / TS 4.4 (`exactOptionalPropertyTypes`) / TS 7.0 Native |
| **Tested tsconfig** | `strict: true`, `strictNullChecks: true`, `exactOptionalPropertyTypes: true` |

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
- **Problem Statement:** In untyped JavaScript, `null` and `undefined` are the single greatest cause of runtime crashes (`TypeError: Cannot read properties of undefined (reading 'x')`). Sir Tony Hoare famously termed null references his "billion-dollar mistake."
- **Historical Context:** Prior to TypeScript 2.0, `null` and `undefined` were assignable to all types. A `string` variable could secretly be `null` or `undefined` without warning. TypeScript 2.0 introduced `strictNullChecks`, turning `null` and `undefined` into separate, distinct unit types.
- **Real-World Relevance:** In modern TypeScript, every nullable value must be explicitly declared as a union (`string | null`). The compiler forces developers to narrow or handle nullable values before accessing properties, completely preventing null dereference runtime faults.

---

## 2. Core Concepts
- **Formal Invariants:**
  - `undefined`: Indicates a variable has been declared but not assigned a value, or an object property is absent.
  - `null`: Indicates the intentional absence of an object value.
  - Under `strictNullChecks: false`, `null` and `undefined` inhabit all types.
  - Under `strictNullChecks: true`, `null` and `undefined` inhabit only themselves and `any` / `unknown`.
  - Under `exactOptionalPropertyTypes: true` (TS 4.4+), an optional property `foo?: string` permits omitting the property, but disallows explicitly assigning `{ foo: undefined }`.
- **Mental Model:** Treat `null` as an explicit empty value sent by an external protocol (e.g. JSON `null`), and treat `undefined` as the JavaScript runtime default for uninitialized identifiers and omitted arguments.
- **Soundness vs Ergonomics:** Non-null assertions (`x!`) allow bypassing compiler checks when the developer knows a value is present, but should be treated as an exceptional escape hatch.

---

## 3. How It Works Internally
- **Control Flow Narrowing:** The type checker uses CFA nodes to filter out `null` and `undefined` across condition checks (`if (x !== null && x !== undefined)` or truthiness `if (x)`).
- **Optional Chaining Emit:** The `?.` operator emits ternary or temporary variable checks in JavaScript that short-circuit to `undefined` if the target is nullish.
- **Architectural Flow Diagram:**

```
+--------------------------------------------------------------------------+
|                     Nullable Control Flow Narrowing                      |
|                                                                          |
|                   Value: [ input: string | null | undefined ]            |
|                                     │                                    |
|                                     ▼                                    |
|                       Check: if (input != null)                          |
|                       (Eliminates both null & undef)                     |
|                                /        \                               |
|                              TRUE      FALSE                             |
|                              /            \                             |
|                             ▼              ▼                             |
|                  Type: [ string ]    Type: [ null | undefined ]          |
|                  Safe to read .length! Dead code or early return         |
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & API Reference
- **Operators & Syntax:**
  - Optional chaining: `obj?.prop`, `arr?.[0]`, `func?.()`
  - Nullish coalescing: `const val = a ?? b` (evaluates `b` only if `a` is `null` or `undefined`)
  - Non-null assertion: `val!` (asserts to compiler that `val` is `NonNullable<T>`)
  - Utility type: `NonNullable<T>` (removes `null` and `undefined` from `T`)
- **tsconfig Dependencies:**
  - `strictNullChecks: true`: Mandates explicit nullability.
  - `exactOptionalPropertyTypes: true`: Distinguishes missing keys from `undefined` values.

---

## 5. Practical Examples

### Example 1: Basic / Foundational Usage
- **Goal:** Compare nullish coalescing (`??`) with truthy OR (`||`).
- **Code:**
```typescript
function configureTimeout(userTimeout?: number): number {
  // ?? only overrides null and undefined (preserves 0!)
  return userTimeout ?? 5000;
}

function flawedTimeout(userTimeout?: number): number {
  // || overrides 0 because 0 is falsy!
  return userTimeout || 5000;
}

console.log(configureTimeout(0)); // 0 (correct: user requested 0ms timeout)
console.log(flawedTimeout(0));    // 5000 (bug: ignored 0ms timeout!)
```
- **Output / Type Evaluation:**
```typescript
// configureTimeout(0) evaluates to 0
// flawedTimeout(0) evaluates to 5000
```
- **Explanation:** Nullish coalescing (`??`) correctly differentiates valid falsy values (`0`, `""`, `false`) from absent values.

### Example 2: Real-World Enterprise Pattern (Strict Optional Properties)
- **Goal:** Differentiate between omitting a field and setting it to undefined under `exactOptionalPropertyTypes`.
- **Code:**
```typescript
// tsconfig: { "exactOptionalPropertyTypes": true }

type UserProfileUpdate = {
  bio?: string;
  avatarUrl?: string | null;
};

function updateUser(userId: string, update: UserProfileUpdate): void {
  if ('bio' in update) {
    // Property was explicitly passed
    console.log('Update bio:', update.bio);
  }
}

// Valid: omitted property
updateUser('u1', {});

// Valid: string provided
updateUser('u1', { bio: 'Engineer' });

// Valid: explicit null for clearing avatar
updateUser('u1', { avatarUrl: null });
```
- **Output / Type Evaluation:**
```typescript
// Clean distinction between missing properties and null/undefined values.
```
- **Explanation:** In REST/GraphQL APIs, omitting a field means "do not modify", while sending `null` means "clear the value".

### Example 3: Edge Case / Boundary Condition (exactOptionalPropertyTypes Negative Case)
- **Goal:** Demonstrate error when passing explicit undefined to an optional property.
- **Code:**
```typescript
type Settings = {
  theme?: 'dark' | 'light';
};

const validSettings: Settings = {}; // OK: omitted

// @ts-expect-error TS2375: Type 'undefined' is not assignable to type '"dark" | "light"' with exactOptionalPropertyTypes: true
const invalidSettings: Settings = { theme: undefined };
```
- **Output / Type Evaluation:**
```typescript
// TS2375 prevents passing explicit undefined when exactOptionalPropertyTypes is enabled.
```
- **Explanation:** Under `exactOptionalPropertyTypes: true`, `theme?: 'dark' | 'light'` means `theme` may be present with a valid string or absent entirely, but cannot be assigned the literal value `undefined`.

### Example 4: Negative Test / Type-Check Failure Case
- **Goal:** Demonstrate strictNullChecks preventing null dereference.
- **Code:**
```typescript
function getLength(str: string | null): number {
  // @ts-expect-error TS18047: 'str' is possibly 'null'.
  return str.length;
}

function safeGetLength(str: string | null): number {
  if (str === null) return 0;
  return str.length; // Narrowed to string
}
```
- **Real Compiler Error:**
```
error TS18047: 'str' is possibly 'null'.
```
- **Explanation:** `strictNullChecks` rejects direct property access on nullable types until narrowed.

### Example 5: Idiomatic Modern Usage (Optional Chaining with Fallback)
- **Goal:** Traverse deep nested nullable properties safely.
- **Code:**
```typescript
type Company = {
  departments?: {
    engineering?: {
      lead?: {
        getName?: () => string;
      };
    };
  };
};

function getLeadName(company: Company | null | undefined): string {
  return company?.departments?.engineering?.lead?.getName?.() ?? 'No Lead Assigned';
}
```
- **Output / Type Evaluation:**
```typescript
// Returns string unconditionally; zero runtime exceptions if any segment is null/undefined.
```
- **Explanation:** Chaining optional property access and optional method invocation short-circuits to `undefined` safely if any step is nullish.

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Loose Equality Narrowing:** Checking `x == null` checks for BOTH `null` and `undefined` simultaneously in JavaScript. TypeScript's CFA understands this and narrows out both types in one check.
2. **Non-Null Assertion Abuse (`!`):** Overusing `!` disables compiler checks without runtime protection. If the variable is `null` at runtime, the application crashes immediately.
3. **Array Indexing Without `noUncheckedIndexedAccess`:** By default, accessing `arr[10]` on `number[]` is typed as `number`, even if the array is empty. Enable `noUncheckedIndexedAccess: true` so array accesses return `T | undefined`.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern (Naive / Broken) | Real Compiler Error / Bug | Idiomatic Solution | Why It Matters |
| :--- | :--- | :--- | :--- |
| Using `x || defaultVal` for numbers/booleans | Overrides valid `0` or `false` values | Use `x ?? defaultVal` | Preserves intentional zero or boolean false |
| Using `!` operator to silence compiler | Production `TypeError: Cannot read properties of null` | Use narrowing (`if (x != null)`) | Eliminates production runtime crashes |
| Setting `strictNullChecks: false` | The billion-dollar mistake: silent null exceptions | Set `strictNullChecks: true` | Guarantees compile-time null safety |
| Confusing `null` and `undefined` in domain models | Inconsistent API payloads and serialization bugs | Choose a standard (e.g. `undefined` for JS, `null` for DB) | Keeps data layer contracts predictable |
| Accessing arrays without index check | `undefined` leaks into calculations | Enable `noUncheckedIndexedAccess` | Forces handling out-of-bounds indices |
| Explicitly passing `undefined` under exact options | `TS2375: Type 'undefined' is not assignable` | Omit key or annotate `T | undefined` | Enforces strict property semantics |

---

## 8. Performance & Type-Checker Complexity
- **Type Union Resolution:** Resolving `T | null | undefined` requires adding 2 primitive unit types to the union set. The type checker caches `NonNullable<T>` references.
- **Runtime Emit:** Optional chaining (`?.`) and nullish coalescing (`??`) compile down to concise ternary checks when targeting older ES versions (e.g. ES2019), or remain native under `target: "ES2020"`+ with zero transpilation overhead.
- **Isolated Declarations:** Full compatibility with `--isolatedDeclarations`.

---

## 9. Compiler Internals
- **AST Node Representation:**
  - `null` is parsed as `SyntaxKind.NullKeyword`.
  - `undefined` is parsed as `SyntaxKind.UndefinedKeyword`.
- **Type Flags:**
  - `TypeFlags.Null`
  - `TypeFlags.Undefined`
- **Checker Behavior:** In `src/compiler/checker.ts`, `getTypeOfSymbol` checks whether `strictNullChecks` is active. If inactive, `TypeFlags.Null` and `TypeFlags.Undefined` are included in the constituent set of every type.

---

## 10. Best Practices & Design Guidelines
- **Always Enable `strictNullChecks` and `exactOptionalPropertyTypes`:** Maximize compiler rigor.
- **Use Nullish Coalescing (`??`) over Logical OR (`||`):** Prevent accidental overriding of `0`, `false`, and `""`.
- **Prefer `undefined` for Internal TypeScript APIs:** JavaScript functions return `undefined` by default; align internal code with the language runtime.

---

## 11. Real-World Case Study
- **Scenario:** An e-commerce platform experienced cart checkout errors when customers applied coupons offering $0 discount items. The cart calculation used `const discount = coupon.discountAmount || 10`. Because `0` is falsy, customers were unexpectedly granted $10 discounts, causing $60,000 in promotional losses in 48 hours.
- **Implementation:** Migrated to strict nullish coalescing:
  ```typescript
  type Coupon = {
    code: string;
    discountAmount?: number;
  };

  function calculateDiscount(coupon?: Coupon): number {
    return coupon?.discountAmount ?? 0;
  }
  ```
- **Outcome:** The calculation correctly evaluates `0` as a valid number, completely fixing the coupon leak while safely defaulting absent coupons to 0.

---

## 12. Interview Questions

### Q1: What is the exact difference between `null` and `undefined` in JavaScript and TypeScript?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

In JavaScript, `undefined` means a variable has been declared but not yet assigned a value, an object property does not exist, or a function does not explicitly return a value. `null` represents the intentional absence of an object reference. In TypeScript under `strictNullChecks: true`, both are distinct primitive unit types. In JSON serialization, `null` is preserved as `null`, whereas properties with `undefined` values are omitted entirely.
</details>

### Q2: How does TypeScript differentiate between `foo?: string` and `foo: string | undefined` under `exactOptionalPropertyTypes`?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Without `exactOptionalPropertyTypes`, `foo?: string` and `foo: string | undefined` behave almost identically.
With `exactOptionalPropertyTypes: true`:
- `foo?: string` means the property may be present with a `string` value, or omitted entirely. Assigning `{ foo: undefined }` is illegal.
- `foo: string | undefined` means the property key MUST be present on the object, but its value may be `undefined`.
</details>

### Q3: Why is `x == null` considered idiomatic in TypeScript for checking nullishness?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

In JavaScript, loose equality `x == null` evaluates to `true` if and only if `x` is either `null` or `undefined`. TypeScript's Control Flow Analysis understands this semantics: inside `if (x == null)`, `x` is narrowed to `null | undefined`, and in the `else` branch, `x` is narrowed to `NonNullable<T>`. This provides a concise, idiomatic one-liner to guard against both absent values.
</details>

### Q4: What does the non-null assertion operator (`!`) do at runtime?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

The non-null assertion operator (`!`) has zero runtime effect; it is completely erased during compilation. It only instructs the TypeScript type checker to remove `null` and `undefined` from the type of the operand. If the value is actually `null` or `undefined` at runtime, subsequent property accesses will throw a runtime `TypeError`.
</details>

### Q5: How does `noUncheckedIndexedAccess` alter the return type of array lookups?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

By default, accessing an element in `arr: string[]` using `arr[0]` returns `string`, which is unsound because the array might be empty. When `"noUncheckedIndexedAccess": true` is enabled in `tsconfig.json`, indexing any array or tuple with dynamic numbers returns `T | undefined`, forcing developers to check if the element exists before using it.
</details>

### Q6: How does `NonNullable<T>` work under the hood?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

`NonNullable<T>` is defined in the standard library as:
```typescript
type NonNullable<T> = T & {};
```
(or historically `T extends null | undefined ? never : T`). In TypeScript 4.8+, intersecting any type with the empty object `{}` excludes `null` and `undefined`, producing a clean, non-nullable type representation.
</details>

### Q7: What is the behavior of optional chaining on a method call if the method is undefined (`obj.method?.()`)?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

If `obj.method` is `null` or `undefined`, optional call chaining short-circuits execution: the invocation is skipped entirely, the argument expressions inside `()` are not evaluated, and the entire expression evaluates to `undefined`.
</details>

### Q8: Why does `typeof null` return `"object"` in JavaScript and how does TypeScript handle it?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

In the original JavaScript implementation (1995), values were represented with type tags in their lower bits; object references had a tag of `0`, and `null` was represented as a NULL pointer (all bits `0`), causing `typeof null` to return `"object"`. TypeScript's type checker recognizes this quirk: checking `typeof x === "object"` does NOT narrow out `null`. To safely narrow to an object, you must check `typeof x === "object" && x !== null`.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Safe Nullish Normalizer (Easy)
- **Problem Statement:** Write a function `normalizeString(val: string | null | undefined): string` that returns the trimmed string, or an empty string if nullish.
- **Test Case:**
```typescript
normalizeString('  hello  '); // "hello"
normalizeString(null); // ""
normalizeString(undefined); // ""
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function normalizeString(val: string | null | undefined): string {
  return val?.trim() ?? '';
}
```
</details>

### Exercise 2: Strict Defined Guard (Easy)
- **Problem Statement:** Write a type guard `isDefined<T>(val: T | null | undefined): val is T` that narrows out `null` and `undefined`.
- **Test Case:**
```typescript
const items: (string | null | undefined)[] = ['a', null, 'b', undefined];
const filtered: string[] = items.filter(isDefined);
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function isDefined<T>(val: T | null | undefined): val is T {
  return val !== null && val !== undefined;
}
```
</details>

### Exercise 3: Deep Non-Nullable Type (Medium)
- **Problem Statement:** Implement a utility type `DeepNonNullable<T>` that recursively strips `null` and `undefined` from all nested properties and arrays.
- **Test Case:**
```typescript
type Dirty = {
  a?: string | null;
  b: { c: number | undefined } | null;
  d: (string | null)[];
};
type Clean = DeepNonNullable<Dirty>;
// { a: string; b: { c: number }; d: string[] }
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
type DeepNonNullable<T> = T extends null | undefined
  ? never
  : T extends Array<infer U>
  ? Array<DeepNonNullable<U>>
  : T extends object
  ? { [K in keyof T]-?: DeepNonNullable<T[K]> }
  : T;
```
</details>

### Exercise 4: Safe Nested Property Getter (Medium)
- **Problem Statement:** Write a function `safeGet` that takes an object and a path array, returning the nested value or `undefined` without throwing runtime exceptions.
- **Test Case:**
```typescript
const data = { a: { b: { c: 42 } } };
safeGet(data, ['a', 'b', 'c']); // 42
safeGet(data, ['a', 'missing', 'c']); // undefined
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function safeGet(obj: unknown, path: string[]): unknown {
  let current: any = obj;
  for (const segment of path) {
    if (current == null) return undefined;
    current = current[segment];
  }
  return current;
}
```
</details>

### Exercise 5: Compile-Time Nullable Keys Extractor (Hard)
- **Problem Statement:** Write a utility type `NullableKeys<T>` that extracts keys whose types include `null` or `undefined`.
- **Test Case:**
```typescript
type Profile = {
  id: string;
  avatar: string | null;
  bio?: string;
  age: number;
};
type Result = NullableKeys<Profile>; // "avatar" | "bio"
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
type NullableKeys<T> = {
  [K in keyof T]-?: null extends T[K] ? K : undefined extends T[K] ? K : never;
}[keyof T];
```
</details>

### Exercise 6: Exact Optional Object Builder (Hard)
- **Problem Statement:** Create a type-safe builder `createBuilder<T>()` that enforces `exactOptionalPropertyTypes` semantics, allowing chaining `.set('key', value)` where passing `undefined` is rejected if the property type does not include `undefined`.
- **Test Case:**
```typescript
type Schema = { name: string; age?: number };
const builder = createBuilder<Schema>();
builder.set('name', 'Alice').set('age', 30).build();
// builder.set('age', undefined) should produce a compile error!
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function createBuilder<T extends object>() {
  const state: Partial<T> = {};
  return {
    set<K extends keyof T>(key: K, value: T[K]): typeof this {
      state[key] = value;
      return this;
    },
    build(): T {
      return { ...state } as T;
    }
  };
}
```
</details>
