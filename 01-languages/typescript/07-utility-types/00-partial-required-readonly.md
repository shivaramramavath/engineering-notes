# Partial, Required, Readonly, and Record

> Property modifier utility types in TypeScript transform the mutability and presence of object keys, establishing robust compile-time contracts for data patching, configuration initialization, and immutable data flow.

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [03-objects.md](file:///E:/Learning/engineering-notes/01-languages/typescript/01-fundamentals/03-objects.md), [01-generic-types.md](file:///E:/Learning/engineering-notes/01-languages/typescript/06-generics/01-generic-types.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 2.1 / TS 5.x / TS 7.0 Native |
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
In TypeScript application development, data models frequently transition through different lifecycle stages:
- A new record being created might have missing optional fields before insertion.
- An update operation (such as HTTP PATCH) only provides a subset of properties.
- A frozen domain entity or Redux state slice must forbid mutation.
- A key-value lookup table maps arbitrary keys to structured payloads.

Instead of duplicating interface declarations for every variant (e.g. `User`, `CreateUserDto`, `UpdateUserDto`, `ReadonlyUser`), TypeScript provides mapped type utilities: **`Partial<T>`**, **`Required<T>`**, **`Readonly<T>`**, and **`Record<K, T>`**. These derive specialized contracts from a single source of truth.

---

## 2. Core Concepts

### `Partial<T>`: Optionality Modifier
`Partial<T>` iterates over every property key in `T` and adds the optional (`?`) modifier:
```typescript
interface UserProfile {
  id: string;
  name: string;
  age: number;
}

type UpdateUserPayload = Partial<UserProfile>;
// { id?: string; name?: string; age?: number }
```

### `Required<T>`: Concrete Presence Modifier
`Required<T>` strips away the optional modifier (`-?`) from all properties, ensuring no keys are undefined or omitted:
```typescript
interface ConfigOptions {
  host?: string;
  port?: number;
  timeout?: number;
}

type ResolvedConfig = Required<ConfigOptions>;
// { host: string; port: number; timeout: number }
```

### `Readonly<T>`: Immutability Modifier
`Readonly<T>` adds the `readonly` modifier to each property in `T`, preventing reassignments:
```typescript
type ImmutableUser = Readonly<UserProfile>;
const user: ImmutableUser = { id: "u1", name: "Ada", age: 36 };
// user.age = 37; // Compile error: Cannot assign to 'age' because it is a read-only property.
```

### `Record<K, T>`: Key-to-Value Mapping
`Record<K, T>` constructs an object type whose keys belong to union `K` and whose values are of type `T`:
```typescript
type Role = "admin" | "editor" | "guest";
type Permissions = Record<Role, string[]>;
// { admin: string[]; editor: string[]; guest: string[] }
```

---

## 3. How It Works Internally

```
Type Definitions in TypeScript's lib.es5.d.ts:

type Partial<T> = {
  [P in keyof T]?: T[P];
};

type Required<T> = {
  [P in keyof T]-?: T[P];
};

type Readonly<T> = {
  readonly [P in keyof T]: T[P];
};

type Record<K extends keyof any, T> = {
  [P in K]: T;
};
```

The `+` and `-` prefixes explicitly add or remove modifiers:
- `?` is shorthand for `+?` (add optional).
- `-?` removes the optional modifier.
- `readonly` is shorthand for `+readonly`.
- `-readonly` removes the read-only modifier (producing mutable equivalents).

---

## 4. Syntax & API Reference

### Combining Modifiers
Modifiers can be composed or inverted:
```typescript
// Remove readonly modifier to make mutable:
type Mutable<T> = {
  -readonly [P in keyof T]: T[P];
};

// Make only a subset of properties partial:
type PartialBy<T, K extends keyof T> = Omit<T, K> & Partial<Pick<T, K>>;
```

### Interaction with `exactOptionalPropertyTypes`
Under `exactOptionalPropertyTypes: true`:
- A property marked `name?: string` permits `{ }` (omitted) or `{ name: "Alice" }`.
- It explicitly **rejects** `{ name: undefined }`.
- `Partial<T>` creates properties with `?`, respecting this boundary.

---

## 5. Practical Examples

### 5.1 Basic Usage
```typescript
interface Device {
  mac: string;
  ip: string;
  online: boolean;
}

function updateDevice(id: string, patch: Partial<Device>): void {
  console.log(`Updating ${id}`, patch);
}

updateDevice("dev-01", { online: false });
```

### 5.2 Real-World Application: Configuration Fallback Resolver
```typescript
interface ServerOptions {
  host: string;
  port: number;
  ssl: boolean;
  maxConnections: number;
}

const DEFAULT_OPTIONS: ServerOptions = {
  host: "127.0.0.1",
  port: 8080,
  ssl: false,
  maxConnections: 100,
};

function initializeServer(userOptions: Partial<ServerOptions>): Required<ServerOptions> {
  return {
    ...DEFAULT_OPTIONS,
    ...userOptions,
  };
}

const serverConfig = initializeServer({ port: 3000 });
console.log(serverConfig.host); // "127.0.0.1" (strictly guaranteed present)
```

### 5.3 Edge Case: Shallow Nature of Built-in Utilities
```typescript
interface NestedAccount {
  id: string;
  settings: {
    notifications: boolean;
  };
}

type ShallowPartial = Partial<NestedAccount>;
// settings is optional, but if present, notifications is still required!
const acc: ShallowPartial = {
  settings: {
    // @ts-expect-error TS2741: Property 'notifications' is missing in type '{}' but required in type '{ notifications: boolean; }'.
    notifications: undefined
  }
};
```

### 5.4 Failure Case: Modifying Readonly Property
```typescript
interface Point {
  x: number;
  y: number;
}

const origin: Readonly<Point> = { x: 0, y: 0 };

// @ts-expect-error TS2540: Cannot assign to 'x' because it is a read-only property.
origin.x = 10;
```

### 5.5 Idiomatic Pattern: Dynamic Routing Dictionary
```typescript
type HttpMethod = "GET" | "POST" | "PUT" | "DELETE";

type RouteTable = Record<HttpMethod, Map<string, (req: any) => void>>;

const routes: RouteTable = {
  GET: new Map(),
  POST: new Map(),
  PUT: new Map(),
  DELETE: new Map(),
};

routes.GET.set("/status", () => ({ ok: true }));
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Shallow Transformations**: Built-in `Partial`, `Required`, and `Readonly` only operate at depth 1. Nested objects remain unchanged unless deep mapped utilities are used.
2. **`Record<string, T>` vs Array Indexing**: `Record<string, T>` permits indexing with numbers because JS converts numbers to strings.
3. **Array Mutation Under `Readonly<T>`**: `Readonly<string[]>` is equivalent to `readonly string[]`. It disables `push`, `pop`, `splice`, and element assignment.
4. **`Required<T>` on Nullable Fields**: `Required<{ name?: string | null }>` produces `{ name: string | null }`. It removes the optional flag, but preserves `null`!

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Problem | Solution | Compiler Error |
| :--- | :--- | :--- | :--- |
| Expecting deep immutability with `Readonly<T>` | Nested properties remain freely mutable at runtime | Use recursive `DeepReadonly<T>` | Silent mutation bugs |
| Expecting `Partial<T>` to make nested fields optional | Nested properties still require full shape when defined | Use recursive `DeepPartial<T>` | `TS2741: Property 'x' is missing...` |
| Overwriting entire object with `Partial<T>` | Missing fields at runtime lead to `undefined` | Merge with defaults: `{ ...defaults, ...patch }` | Runtime TypeError on missing props |
| Passing `undefined` under `exactOptionalPropertyTypes` | `exactOptionalPropertyTypes` distinguishes omit vs `undefined` | Do not pass explicit `undefined` unless typed `T | undefined` | `TS2375: Type 'undefined' is not assignable to...` |
| Using `Record<any, T>` | Destroys key type safety | Constrain keys: `Record<AllowedKeys, T>` | Loss of key checking |
| Confusing `Required<T>` with non-nullable | Leaves `null` and `undefined` in value union | Combine with `NonNullable<T[K]>` | Uncaught `null` reference |

---

## 8. Performance & Type-Checker Complexity
- Mapped types over large unions (`Record<WideUnion, T>`) allocate symbol tables proportional to the number of union members.
- Chained mapped modifiers (e.g. `Readonly<Partial<Required<T>>>`) are normalized into single-pass mapped types internally by the compiler without creating intermediate AST nodes.

---

## 9. Compiler Internals
- `MappedTypeNode` (`kind: SyntaxKind.MappedType`) handles `[P in K]`.
- Modifiers are represented via `TypeParameterDeclaration` tokens:
  - `questionToken`: `SyntaxKind.QuestionToken` (`+`), or preceded by `SyntaxKind.MinusToken` (`-`).
  - `readonlyToken`: `SyntaxKind.ReadonlyKeyword` (`+`), or preceded by `SyntaxKind.MinusToken` (`-`).
- In `src/compiler/checker.ts`, `resolveMappedTypeMembers` iterates over the keys of the constraint type, generating cloned property symbols with updated flags (`SymbolFlags.Optional`, `CheckFlags.Readonly`).

---

## 10. Best Practices & Design Guidelines
- Use `Partial<T>` exclusively for update patches or input options where defaults exist.
- Use `Required<T>` on output config objects after merging with system defaults.
- Always use `Readonly<T>` (or `ReadonlyArray<T>`) for function parameters that must not be mutated.
- For nested structures, author explicit `DeepReadonly` or `DeepPartial` utilities.

---

## 11. Real-World Case Study
### Type-Safe Patch API Handler with Selective Immutability
```typescript
interface UserEntity {
  readonly id: string;
  readonly createdAt: Date;
  email: string;
  displayName: string;
  bio?: string;
}

// Clients cannot patch 'id' or 'createdAt'
type UserPatchPayload = Partial<Omit<UserEntity, "id" | "createdAt">>;

class UserService {
  private users = new Map<string, UserEntity>();

  patchUser(id: string, patch: UserPatchPayload): UserEntity {
    const existing = this.users.get(id);
    if (!existing) throw new Error("Not found");

    const updated: UserEntity = {
      ...existing,
      ...patch,
      id: existing.id,              // Invariant
      createdAt: existing.createdAt // Invariant
    };

    this.users.set(id, updated);
    return updated;
  }
}
```

---

## 12. Interview Questions

<details>
<summary>1. How does <code>Required&lt;T&gt;</code> remove optionality in its implementation?</summary>

It uses the minus-question-mark (`-?`) modifier in a mapped type:
```typescript
type Required<T> = { [P in keyof T]-?: T[P] };
```
The `-?` instructs the compiler to remove the `QuestionToken` from every property symbol.
</details>

<details>
<summary>2. Is <code>Readonly&lt;T&gt;</code> deep or shallow?</summary>

It is strictly **shallow**. It only applies the `readonly` modifier to top-level properties of `T`. Any nested objects, arrays, or references remain mutable unless wrapped in a recursive `DeepReadonly<T>` type.
</details>

<details>
<summary>3. What is the difference between <code>Record&lt;string, unknown&gt;</code> and <code>object</code>?</summary>

`Record<string, unknown>` represents any object that can be indexed with string keys, allowing index notation `obj[key]`. The `object` primitive represents any non-primitive value (anything that is not number, string, boolean, symbol, null, or undefined), but does not permit property access without narrowing or assertions.
</details>

<details>
<summary>4. How does <code>exactOptionalPropertyTypes</code> affect <code>Partial&lt;T&gt;</code>?</summary>

Under `exactOptionalPropertyTypes: true`, optional properties cannot be assigned the literal value `undefined` unless explicitly typed as `T | undefined`. `Partial<T>` makes keys optional, meaning they can be omitted, but passing `{ key: undefined }` will trigger a compile error if `T[key]` does not include `undefined`.
</details>

<details>
<summary>5. How can you implement a <code>Mutable&lt;T&gt;</code> utility to reverse <code>Readonly&lt;T&gt;</code>?</summary>

Using the minus-readonly (`-readonly`) modifier:
```typescript
type Mutable<T> = { -readonly [P in keyof T]: T[P] };
```
</details>

<details>
<summary>6. Does <code>Required&lt;{ tag?: string | null }&gt;</code> strip <code>null</code>?</summary>

No. `Required<T>` only removes the optional flag (`?`). It does not modify type unions within property values, so `null` remains part of the type.
</details>

<details>
<summary>7. What happens if you pass a union to <code>Record&lt;K, T&gt;</code>?</summary>

It generates an object type requiring **every** key in the union to be present. For example, `Record<"a" | "b", number>` requires both `a` and `b`.
</details>

<details>
<summary>8. How do you make only specific properties optional in an interface?</summary>

By combining `Omit` and `Pick` with `Partial`:
```typescript
type PartialKeys<T, K extends keyof T> = Omit<T, K> & Partial<Pick<T, K>>;
```
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Implement Mutable Utility (Easy)
Implement a utility `Mutable<T>` that strips `readonly` from all properties.
<details>
<summary>Solution</summary>

```typescript
type Mutable<T> = {
  -readonly [K in keyof T]: T[K];
};

interface Frozen {
  readonly id: number;
  readonly name: string;
}
type Writable = Mutable<Frozen>;
// { id: number; name: string }
```
</details>

### Exercise 2: Strict Optional Keys Extractor (Easy)
Extract all keys from `T` that are optional.
<details>
<summary>Solution</summary>

```typescript
type OptionalKeys<T> = {
  [K in keyof T]-?: {} extends Pick<T, K> ? K : never;
}[keyof T];

interface Sample {
  required: string;
  optional?: number;
}
type Opts = OptionalKeys<Sample>; // "optional"
```
</details>

### Exercise 3: Selective Required Utility (Medium)
Implement `RequireKeys<T, K extends keyof T>` which makes only specified keys required while preserving others.
<details>
<summary>Solution</summary>

```typescript
type RequireKeys<T, K extends keyof T> = Omit<T, K> & Required<Pick<T, K>>;

interface Config {
  host?: string;
  port?: number;
  ssl?: boolean;
}
type ServerConf = RequireKeys<Config, "host" | "port">;
// { ssl?: boolean; host: string; port: number }
```
</details>

### Exercise 4: Dictionary Value Transformer (Medium)
Write a function that maps the values of a `Record<K, V>` to a new `Record<K, R>` using a mapper function.
<details>
<summary>Solution</summary>

```typescript
function mapRecord<K extends string | number | symbol, V, R>(
  record: Record<K, V>,
  mapper: (val: V, key: K) => R
): Record<K, R> {
  const result = {} as Record<K, R>;
  for (const key of Object.keys(record) as K[]) {
    result[key] = mapper(record[key], key);
  }
  return result;
}

const counts = { a: 1, b: 2 };
const formatted = mapRecord(counts, (v) => `count: ${v}`);
```
</details>

### Exercise 5: Strictly Guarded Partial Merger (Hard)
Implement an immutable merge utility that merges a default object with a partial patch, guaranteeing the return type is `Required<T>`.
<details>
<summary>Solution</summary>

```typescript
function safeMerge<T extends object>(defaults: Required<T>, patch: Partial<T>): Required<T> {
  return {
    ...defaults,
    ...patch,
  };
}

interface Settings {
  theme: "light" | "dark";
  retries: number;
}
const defs: Required<Settings> = { theme: "light", retries: 3 };
const merged = safeMerge(defs, { retries: 5 });
```
</details>

### Exercise 6: Deep Readonly Implementation (Hard)
Implement a fully recursive `DeepReadonly<T>` that freezes nested objects, arrays, maps, and sets while preserving function signatures.
<details>
<summary>Solution</summary>

```typescript
type DeepReadonly<T> = T extends (...args: any[]) => any
  ? T
  : T extends ReadonlyMap<infer K, infer V>
  ? ReadonlyMap<DeepReadonly<K>, DeepReadonly<V>>
  : T extends ReadonlySet<infer M>
  ? ReadonlySet<DeepReadonly<M>>
  : T extends object
  ? { readonly [K in keyof T]: DeepReadonly<T[K]> }
  : T;

type Nested = DeepReadonly<{ user: { tags: string[] } }>;
// { readonly user: { readonly tags: readonly string[] } }
```
</details>
