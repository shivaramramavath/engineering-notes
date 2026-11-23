# Pick, Omit, and Record

> Projection and structural transformation utilities in TypeScript allow engineering precise, fine-grained view models, DTOs, and dictionary representations from base domain entities.

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [00-partial-required-readonly.md](file:///E:/Learning/engineering-notes/01-languages/typescript/07-utility-types/00-partial-required-readonly.md), [03-keyof-and-typeof.md](file:///E:/Learning/engineering-notes/01-languages/typescript/06-generics/03-keyof-and-typeof.md) |
| **Time to Read** | 24 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 2.1 / TS 3.5 / TS 7.0 Native |
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
In software architecture, the shape of data often depends on the context:
- A database model has private identifiers, hashed passwords, and audit timestamps.
- A public API response must conceal security-sensitive columns.
- An input form only asks for user-editable fields.
- A lookup dictionary maps resource identifiers to entity instances.

Rather than authoring divergent interface definitions that become desynchronized, TypeScript provides **`Pick<T, K>`** and **`Omit<T, K>`** for projection, and **`Record<K, T>`** for associative collections.

---

## 2. Core Concepts

### `Pick<T, K>`: Whitelist Projection
`Pick<T, K>` constructs a type by picking the set of properties `K` (where `K extends keyof T`) from type `T`:
```typescript
interface UserAccount {
  id: string;
  email: string;
  passwordHash: string;
  createdDate: Date;
}

type UserPreview = Pick<UserAccount, "id" | "email">;
// { id: string; email: string }
```

### `Omit<T, K>`: Blacklist Projection
`Omit<T, K>` constructs a type by picking all properties from `T` and then dropping the keys specified in union `K`:
```typescript
type PublicUser = Omit<UserAccount, "passwordHash">;
// { id: string; email: string; createdDate: Date }
```

### `Record<K, T>`: Associative Mappings
`Record<K, T>` constructs an object type whose keys are `K` and property values are `T`:
```typescript
type Currency = "USD" | "EUR" | "GBP";
type Rates = Record<Currency, number>;
// { USD: number; EUR: number; GBP: number }
```

---

## 3. How It Works Internally

```
Type Implementations in lib.es5.d.ts:

type Pick<T, K extends keyof T> = {
  [P in K]: T[P];
};

type Omit<T, K extends keyof any> = Pick<T, Exclude<keyof T, K>>;

type Record<K extends keyof any, T> = {
  [P in K]: T;
};
```

Mechanics:
1. `Pick` uses a mapped type constrained directly to the keys of `T`. If an invalid key is specified, the compiler rejects it immediately (`TS2344`).
2. `Omit` uses `Exclude<keyof T, K>` to filter keys from `keyof T`, then feeds the remaining keys into `Pick`. Note that `K` extends `keyof any`, meaning `Omit` does not enforce that `K` must exist on `T`.

---

## 4. Syntax & API Reference

### Combining Projections
```typescript
// Pick and modify
type EditableUser = Partial<Pick<UserAccount, "email">>;

// Omit and extend
type EnrichedUser = Omit<UserAccount, "passwordHash"> & {
  authToken: string;
};
```

---

## 5. Practical Examples

### 5.1 Basic Pick & Omit
```typescript
interface BlogPost {
  id: string;
  title: string;
  body: string;
  authorId: string;
  views: number;
}

type PostDraft = Omit<BlogPost, "id" | "views">;
type PostHeader = Pick<BlogPost, "id" | "title" | "authorId">;
```

### 5.2 Real-World Application: Normalized Entity State Store
```typescript
interface EntityState<TEntity extends { id: string }> {
  byId: Record<string, TEntity>;
  allIds: string[];
}

function createNormalizedStore<T extends { id: string }>(items: T[]): EntityState<T> {
  const byId: Record<string, T> = {};
  const allIds: string[] = [];

  for (const item of items) {
    byId[item.id] = item;
    allIds.push(item.id);
  }

  return { byId, allIds };
}

const userStore = createNormalizedStore([
  { id: "u_1", name: "Alice" },
  { id: "u_2", name: "Bob" },
]);
```

### 5.3 Edge Case: `Omit` with Non-Existent Keys
```typescript
interface Item {
  sku: string;
  price: number;
}

// Omit does NOT error if key does not exist:
type CleanItem = Omit<Item, "nonExistentKey">; // { sku: string; price: number }
```

### 5.4 Failure Case: `Pick` with Non-Existent Keys
```typescript
// @ts-expect-error TS2344: Type '"invalid"' does not satisfy the constraint 'keyof Item'.
type BrokenPick = Pick<Item, "invalid">;
```

### 5.5 Idiomatic Pattern: Strict Omit
```typescript
// A strictly checked version of Omit that errors on typos
type StrictOmit<T, K extends keyof T> = Pick<T, Exclude<keyof T, K>>;

// @ts-expect-error TS2344: Type '"typo"' does not satisfy the constraint 'keyof Item'.
type SafeCheck = StrictOmit<Item, "typo">;
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **`Omit` is Unconstrained in `K`**: `Omit<T, K>` allows `K` to contain arbitrary strings without error. Typos in `Omit` keys fail silently.
2. **`Omit` on Discriminated Unions**: Applying `Omit` directly to a discriminated union flattens the union into a single object type, destroying discrimination! Use a distributive helper:
   ```typescript
   type DistributiveOmit<T, K extends keyof any> = T extends any ? Omit<T, K> : never;
   ```
3. **Symbol Keys**: `Record<symbol, T>` is supported. `Pick` and `Omit` preserve symbol keys properly.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Problem | Solution | Compiler Error |
| :--- | :--- | :--- | :--- |
| Applying `Omit` to discriminated union | Collapses variants into single non-discriminated type | Use distributive conditional `DistributiveOmit` | Lost discriminated union safety |
| Misspelling key in `Omit` | Silently succeeds without omitting the target property | Use `StrictOmit<T, K extends keyof T>` | Silent bug / data leak |
| Using `Pick` to extract methods | Strips `this` typing and prototype inheritance | Preserve method signatures or use interface extension | Incomplete class typing |
| Relying on `Record<string, V>` for known keys | Destroys autocompletion for expected properties | Use union of known string literals `Record<KnownKeys, V>` | Lost autocompletion |
| Nesting `Pick` inside `Omit` unnecessarily | Difficult to read and maintain | Consolidate into a single projection | High cognitive load |
| Forgetting `as const` on key arrays | Array types widen to `string[]` | Pass literal unions directly (`"a" | "b"`) | `TS2344: Type 'string' does not satisfy...` |

---

## 8. Performance & Type-Checker Complexity
- `Pick` resolves in $O(K)$ where $K$ is the number of keys picked.
- `Omit` resolves in $O(N + K)$ where $N$ is the number of keys on the source interface.
- If used heavily over massive unions, prefer `Pick` over `Omit` because `Pick` checks constraints eagerly.

---

## 9. Compiler Internals
- `Pick` is mapped via `MappedTypeNode` with constraint `TypeParameterDeclaration` `K extends keyof T`.
- `Omit` delegates to `Exclude`, an intrinsic conditional type evaluated in `src/compiler/checker.ts` through distributive type filtering.
- The symbol table for `Record<K, T>` constructs fresh `TransientSymbol` members for each key literal in the union `K`.

---

## 10. Best Practices & Design Guidelines
- Use `Pick` when creating narrow, specific view-models (e.g. form fields, summary cards).
- Use `Omit` when removing internal identifiers (e.g. `id`, `createdAt`, `passwordHash`) for creation DTOs.
- Author a project-level `StrictOmit` to catch typos during refactors.
- Always use distributive conditional types when picking or omitting from union types.

---

## 11. Real-World Case Study
### Safe REST API Data Transfer Objects (DTOs)
```typescript
interface ArticleEntity {
  id: string;
  slug: string;
  title: string;
  content: string;
  authorId: string;
  publishedAt: Date | null;
  secretReviewNotes: string;
}

// Creation DTO: User provides content without system fields
type CreateArticleDto = Omit<ArticleEntity, "id" | "slug" | "publishedAt" | "secretReviewNotes">;

// Public DTO: Expose to frontend without internal notes
type PublicArticleDto = Omit<ArticleEntity, "secretReviewNotes">;

// Update DTO: Everything in CreateArticleDto is optional
type UpdateArticleDto = Partial<CreateArticleDto>;

function publishArticle(dto: CreateArticleDto): PublicArticleDto {
  return {
    ...dto,
    id: "art_" + Math.random().toString(36).substring(2),
    slug: dto.title.toLowerCase().replace(/s+/g, "-"),
    publishedAt: new Date(),
  };
}
```

---

## 12. Interview Questions

<details>
<summary>1. How is <code>Omit&lt;T, K&gt;</code> defined in terms of <code>Pick</code> and <code>Exclude</code>?</summary>

```typescript
type Omit<T, K extends keyof any> = Pick<T, Exclude<keyof T, K>>;
```
`Exclude<keyof T, K>` filters out keys present in `K` from the union `keyof T`. The remaining keys are picked from `T`.
</details>

<details>
<summary>2. Why does <code>Omit&lt;T, K&gt;</code> not produce a compiler error if <code>K</code> contains a key that does not exist in <code>T</code>?</summary>

Because `K` is constrained to `keyof any` (`string | number | symbol`), not to `keyof T`. This allows omitting keys regardless of whether they exist on the target type, but risks unnoticed typos.
</details>

<details>
<summary>3. What happens when you apply <code>Omit</code> to a discriminated union?</summary>

It breaks discrimination. Standard `Omit` takes the common keys of the union and constructs a single merged object. To preserve each union variant, a distributive conditional type like `type DistributiveOmit<T, K> = T extends any ? Omit<T, K> : never` must be used.
</details>

<details>
<summary>4. What is the difference between <code>Pick</code> and <code>Extract</code>?</summary>

`Pick<T, K>` operates on **object types**, returning a new object shape with a subset of properties. `Extract<T, U>` operates on **union types**, returning union members that are assignable to `U`.
</details>

<details>
<summary>5. How do you create an exact key-to-value map where all keys are required?</summary>

Using `Record<KeyUnion, ValueType>`. Every key in `KeyUnion` must have an associated value assigned.
</details>

<details>
<summary>6. Can you pick properties from union types?</summary>

Yes, but `Pick<A | B, K>` will only allow picking keys present in `(keyof A) & (keyof B)` (the shared intersection of keys).
</details>

<details>
<summary>7. How do you implement a <code>StrictOmit</code> that rejects nonexistent keys?</summary>

By constraining `K` to `keyof T`:
```typescript
type StrictOmit<T, K extends keyof T> = Pick<T, Exclude<keyof T, K>>;
```
</details>

<details>
<summary>8. Does <code>Pick</code> preserve readonly modifiers on properties?</summary>

Yes. Mapped types like `Pick` preserve both `readonly` and optional (`?`) modifiers from the original property signatures in `T`.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Pick Public Profile Fields (Easy)
Extract `displayName` and `avatarUrl` from a comprehensive account interface.
<details>
<summary>Solution</summary>

```typescript
interface FullAccount {
  id: string;
  email: string;
  displayName: string;
  avatarUrl: string;
  hashedToken: string;
}

type PublicProfile = Pick<FullAccount, "displayName" | "avatarUrl">;
```
</details>

### Exercise 2: Strict Omit Implementation (Easy)
Implement `StrictOmit<T, K extends keyof T>`.
<details>
<summary>Solution</summary>

```typescript
type StrictOmit<T, K extends keyof T> = Pick<T, Exclude<keyof T, K>>;

interface Vehicle { vin: string; make: string; model: string; }
type CleanVehicle = StrictOmit<Vehicle, "vin">; // OK
// type Bad = StrictOmit<Vehicle, "color">; // Compile error
```
</details>

### Exercise 3: Distributive Omit for Unions (Medium)
Implement `DistributiveOmit<T, K extends keyof any>` that works over discriminated unions.
<details>
<summary>Solution</summary>

```typescript
type DistributiveOmit<T, K extends keyof any> = T extends any ? Omit<T, K> : never;

type Action =
  | { type: "A"; payload: string; meta: number }
  | { type: "B"; payload: boolean; meta: number };

type CleanAction = DistributiveOmit<Action, "meta">;
// { type: "A"; payload: string } | { type: "B"; payload: boolean }
```
</details>

### Exercise 4: Dynamic Enum Mapper (Medium)
Write a function that validates every member of a string union is mapped to a handler function.
<details>
<summary>Solution</summary>

```typescript
type Stage = "dev" | "staging" | "prod";

function createDeployer(handlers: Record<Stage, () => void>) {
  return (stage: Stage) => handlers[stage]();
}

const deploy = createDeployer({
  dev: () => console.log("deploy dev"),
  staging: () => console.log("deploy stg"),
  prod: () => console.log("deploy prd"),
});
```
</details>

### Exercise 5: Deep Pick Implementation (Hard)
Implement `DeepPick<T, Paths>` that picks nested properties based on dot notation.
<details>
<summary>Solution</summary>

```typescript
type DeepPick<T, Path extends string> = Path extends `${infer Head}.${infer Tail}`
  ? Head extends keyof T
    ? { [K in Head]: DeepPick<T[Head], Tail> }
    : never
  : Path extends keyof T
  ? { [K in Path]: T[Path] }
  : never;

interface Complex {
  user: { profile: { name: string; age: number }; token: string };
  meta: string;
}

type Result = DeepPick<Complex, "user.profile">;
// { user: { profile: { name: string; age: number } } }
```
</details>

### Exercise 6: Invert Record Keys and Values (Hard)
Write a utility type `InvertRecord<T>` that takes a `Record<K, V>` (where values are unique strings) and swaps keys with values.
<details>
<summary>Solution</summary>

```typescript
type InvertRecord<T extends Record<string, string>> = {
  [K in keyof T as T[K]]: K;
};

const Roles = {
  admin: "ROLE_ADMIN",
  user: "ROLE_USER",
} as const;

type Inverted = InvertRecord<typeof Roles>;
// { ROLE_ADMIN: "admin"; ROLE_USER: "user" }
```
</details>
