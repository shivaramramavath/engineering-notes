# Mapped Types

> Mapped types iterate over key unions to generate new property structures, transforming property names, optionality, mutability, and values systematically across object graphs.

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [03-keyof-and-typeof.md](file:///E:/Learning/engineering-notes/01-languages/typescript/06-generics/03-keyof-and-typeof.md), [04-indexed-access-types.md](file:///E:/Learning/engineering-notes/01-languages/typescript/06-generics/04-indexed-access-types.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 2.1 / TS 4.1 (as remapping) / TS 7.0 Native |
| **Tested tsconfig** | `strict: true`, `exactOptionalPropertyTypes: true` |

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
In object-oriented programming, creating variants of existing data structures (making all fields optional, making all fields readonly, renaming keys) requires authoring separate, repetitive classes or interfaces. When properties are added to the base class, duplicate definitions fall out of sync.

**Mapped Types** bring list comprehension to type definitions:
```typescript
{ [ P in K ]: T }
```
By iterating over a union of keys `K`, a mapped type constructs a new object type systematically, mapping over properties like `Array.prototype.map` maps over runtime elements.

---

## 2. Core Concepts

### Basic Syntax
```typescript
type Features = "darkTheme" | "offlineMode" | "cloudSync";

type FeatureFlags = {
  [K in Features]: boolean;
};
// { darkTheme: boolean; offlineMode: boolean; cloudSync: boolean }
```

### Modifier Tokens: `+` and `-`
Mapped types can add (`+`) or remove (`-`) `readonly` and optional (`?`) modifiers:
```typescript
// Add optional (?):
type PartialMap<T> = { [K in keyof T]?: T[K] };

// Remove optional (-?):
type ConcreteMap<T> = { [K in keyof T]-?: T[K] };

// Remove readonly (-readonly):
type WritableMap<T> = { -readonly [K in keyof T]: T[K] };
```

### Key Remapping with `as` (TS 4.1+)
Remap or filter property names using template literal types or conditional filtering:
```typescript
type Getters<T> = {
  [K in keyof T as `get${Capitalize<string & K>}`]: () => T[K];
};
```

---

## 3. How It Works Internally

```
Input Keys: "id" | "name"
Mapping:    [K in "id" | "name" as `on${Capitalize<K>}`]: (val: T[K]) => void
                      |
                      v
Iteration:
  K = "id"   ===> onId: (val: T["id"]) => void
  K = "name" ===> onName: (val: T["name"]) => void
                      |
                      v
Result: { onId: (val: string) => void; onName: (val: string) => void }
```

During type evaluation:
1. The compiler iterates through every symbol member in `keyof T`.
2. If an `as` clause is present, it evaluates the new property name. If the name resolves to `never`, the property is omitted.
3. It constructs property signatures, applying modifier tokens (`+`, `-`), and interns the resulting anonymous object type.

---

## 4. Syntax & API Reference

### Homomorphic Mapped Types
A mapped type of the form `[K in keyof T]` is **homomorphic**: it automatically copies `readonly` and `?` modifiers from `T` unless explicitly overridden with `-` or `+`.
```typescript
interface Original {
  readonly id: string;
  name?: string;
}

// Homomorphic: preserves 'readonly' on id and '?' on name:
type IdentityCopy<T> = { [K in keyof T]: T[K] };
type Copied = IdentityCopy<Original>;
// { readonly id: string; name?: string }
```

---

## 5. Practical Examples

### 5.1 Nullable Object Map
```typescript
type Nullable<T> = {
  [K in keyof T]: T[K] | null;
};

interface User { id: string; age: number; }
type NullableUser = Nullable<User>; // { id: string | null; age: number | null }
```

### 5.2 Real-World Application: Type-Safe React Hook Form Values
```typescript
interface RegistrationForm {
  email: string;
  age: number;
  acceptTerms: boolean;
}

type FormErrors<T> = {
  [K in keyof T]?: string;
};

type FormTouched<T> = {
  [K in keyof T]?: boolean;
};

const errors: FormErrors<RegistrationForm> = {
  email: "Invalid email format"
};
```

### 5.3 Edge Case: Filtering Keys by Returning `never` in `as`
```typescript
type PickMethods<T> = {
  [K in keyof T as T[K] extends Function ? K : never]: T[K];
};

interface Service {
  id: string;
  connect(): void;
  disconnect(): void;
}

type ServiceMethods = PickMethods<Service>;
// { connect(): void; disconnect(): void }
```

### 5.4 Failure Case: Mapping Over Non-Union
```typescript
// @ts-expect-error TS2693: 'string' only refers to a type, but is being used as a value here.
type BadMap = { [K in string]: number }; // In modern TS, [K: string]: number is an index signature!
```

### 5.5 Idiomatic Pattern: Proxy Handler Factory
```typescript
type Proxied<T> = {
  [K in keyof T as `${string & K}$`]: Observable<T[K]>;
};

declare class Observable<T> { value: T; }
type UserObservables = Proxied<User>;
// { id$: Observable<string>; age$: Observable<number> }
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Filtering with `never`**: In key remapping, mapping a key to `never` completely drops it from the output structure.
2. **Homomorphic Array Preservation**: When a homomorphic mapped type is applied to an array or tuple, it returns a new array or tuple with mapped element types, rather than a generic object!
3. **Symbol Keys**: Always intersect with `string` (`string & K`) when using keys in template literals to avoid compile errors on `symbol` keys.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Problem | Solution | Compiler Error |
| :--- | :--- | :--- | :--- |
| Concatenating symbol keys in template literal | `symbol` cannot be coerced in template string | Use `string & K` or `K extends string` | `TS2322: Type 'symbol' not assignable...` |
| Using index signature when mapped type is needed | Creates broad dictionary instead of exact keys | Use `[K in SpecificKeys]: V` | Loss of strict key checking |
| Accidental modifier stripping | Non-homomorphic mapping drops `readonly` and `?` | Use homomorphic `[K in keyof T]` | Dropped property modifiers |
| Over-mapping deep objects at depth 1 | Leaves nested objects untouched | Author recursive mapped type | Incomplete immutability/optionality |
| Mapping over unconstrained generic `T` | `keyof T` is empty or unconstrained | Constrain generic: `<T extends object>` | Unexpected empty type |
| Redundant mapped type chaining | Multiple passes slow down compiler | Merge mapping operations into single pass | Slower type checks |

---

## 8. Performance & Type-Checker Complexity
- Mapped types over object interfaces resolve in $O(K)$ where $K$ is the number of keys.
- The compiler defers evaluation of mapped types until properties are accessed.

---

## 9. Compiler Internals
- `MappedTypeNode` (`kind: SyntaxKind.MappedType`) holds `typeParameter`, `nameType` (from `as`), and modifiers.
- Handled in `src/compiler/checker.ts` by `resolveMappedTypeMembers`.
- Clones property symbols and updates `SymbolFlags.Optional` and `CheckFlags.Readonly`.

---

## 10. Best Practices & Design Guidelines
- Use mapped types to build single-source-of-truth DTOs and view models.
- Prefer homomorphic mapped types (`[K in keyof T]`) to preserve original modifiers.
- Use key remapping with `as ... extends ... ? ... : never` to filter properties cleanly.

---

## 11. Real-World Case Study
### Auto-Generated Redux Action Creators from State Slices
```typescript
interface CounterState {
  count: number;
  step: number;
}

type SliceSetters<T> = {
  [K in keyof T as `set${Capitalize<string & K>}`]: (val: T[K]) => { type: string; payload: T[K] };
};

type CounterActions = SliceSetters<CounterState>;
// {
//   setCount: (val: number) => { type: string; payload: number };
//   setStep: (val: number) => { type: string; payload: number };
// }
```

---

## 12. Interview Questions

<details>
<summary>1. What is a mapped type in TypeScript?</summary>

A mapped type is a generic type syntax (`{ [K in Keys]: ValueType }`) that iterates over a union of property keys to produce a new object type, mapping properties and modifiers systematically.
</details>

<details>
<summary>2. What is a homomorphic mapped type?</summary>

A mapped type of the form `[K in keyof T]` is homomorphic. It automatically preserves the `readonly` and optional (`?`) property modifiers from the original type `T` without requiring explicit re-declaration.
</details>

<details>
<summary>3. What happens when a mapped type maps over an array or tuple?</summary>

A homomorphic mapped type preserves the array/tuple structure, returning a new array or tuple with transformed element types, rather than a plain object with array method keys.
</details>

<details>
<summary>4. How does key remapping with <code>as</code> filter out properties?</summary>

If the `as` expression resolves to `never` for a given key (`as T[K] extends Function ? K : never`), the compiler omits that key completely from the resulting type.
</details>

<details>
<summary>5. How do you remove <code>readonly</code> modifiers in a mapped type?</summary>

Using the minus-readonly token: `-readonly [K in keyof T]: T[K]`.
</details>

<details>
<summary>6. How do you remove optional modifiers in a mapped type?</summary>

Using the minus-question-mark token: `[K in keyof T]-?: T[K]`.
</details>

<details>
<summary>7. What is the difference between an index signature and a mapped type?</summary>

An index signature (`{ [key: string]: number }`) describes an open-ended object that can have any runtime string key. A mapped type (`{ [K in "a" | "b"]: number }`) declares an exact, finite set of known keys.
</details>

<details>
<summary>8. Why must you write <code>string &amp; K</code> when using keys in template literals?</summary>

Because `keyof T` can contain `symbol` keys, and symbols cannot be interpolated into template literal types. Intersecting with `string` filters out non-string keys safely.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Boolean Flags Mapper (Easy)
Convert all properties of an interface into boolean flags.
<details>
<summary>Solution</summary>

```typescript
type ToFlags<T> = {
  [K in keyof T]: boolean;
};

interface User { name: string; age: number; }
type UserFlags = ToFlags<User>; // { name: boolean; age: boolean }
```
</details>

### Exercise 2: Strip Optionality (Easy)
Write a mapped type that makes all properties concrete and required.
<details>
<summary>Solution</summary>

```typescript
type MakeConcrete<T> = {
  [K in keyof T]-?: T[K];
};
```
</details>

### Exercise 3: Filter by Value Type (Medium)
Create `PickPropertiesOfType<T, V>` that only keeps properties assignable to `V`.
<details>
<summary>Solution</summary>

```typescript
type PickPropertiesOfType<T, V> = {
  [K in keyof T as T[K] extends V ? K : never]: T[K];
};

interface Schema { id: number; name: string; tag: string; active: boolean; }
type StringsOnly = PickPropertiesOfType<Schema, string>; // { name: string; tag: string }
```
</details>

### Exercise 4: Async Method Converter (Medium)
Transform all methods of an interface to return Promises.
<details>
<summary>Solution</summary>

```typescript
type PromisifyMethods<T> = {
  [K in keyof T]: T[K] extends (...args: infer A) => infer R
    ? (...args: A) => Promise<Awaited<R>>
    : T[K];
};
```
</details>

### Exercise 5: Deep Mapped Mutable (Hard)
Implement a fully recursive mapped type that removes `readonly` at every level of objects and arrays.
<details>
<summary>Solution</summary>

```typescript
type DeepMutable<T> = T extends Function
  ? T
  : T extends readonly (infer U)[]
  ? Array<DeepMutable<U>>
  : T extends object
  ? { -readonly [K in keyof T]: DeepMutable<T[K]> }
  : T;
```
</details>

### Exercise 6: Invert Object Keys and Values with Mapped Types (Hard)
Write a mapped type `Invert<T>` that swaps string-literal keys and string-literal values.
<details>
<summary>Solution</summary>

```typescript
type Invert<T extends Record<string, string>> = {
  [K in keyof T as T[K]]: K;
};

const Config = {
  DEV: "development",
  PROD: "production",
} as const;

type Inverted = Invert<typeof Config>;
// { development: "DEV"; production: "PROD" }
```
</details>
