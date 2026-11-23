# Indexed Access Types

> Indexed access types (`T[K]`), also known as lookup types, query the type of a specific property or elements from an object, tuple, or array type at compile time.

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [03-keyof-and-typeof.md](file:///E:/Learning/engineering-notes/01-languages/typescript/06-generics/03-keyof-and-typeof.md), [02-arrays-and-tuples.md](file:///E:/Learning/engineering-notes/01-languages/typescript/01-fundamentals/02-arrays-and-tuples.md) |
| **Time to Read** | 24 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 2.1 / TS 5.x / TS 7.0 Native |
| **Tested tsconfig** | `strict: true`, `noUncheckedIndexedAccess: true` |

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
When writing large applications, data structures are deeply nested and continuously evolving. Duplicating types for nested sub-properties (e.g. declaring `interface UserProfile` and separately `interface Address`) often leads to drift when an upstream schema changes.

**Indexed Access Types** (lookup types) mirror runtime property bracket notation (`obj[key]`) entirely in the type system. With syntax `T[K]`, developers can reference the exact type of any property, element, or nested hierarchy directly from the canonical definition.

---

## 2. Core Concepts

### Property Lookup
If `T` is an object type and `K` is a type assignable to `keyof T`, `T[K]` yields the type of that property:

```typescript
interface Customer {
  id: string;
  name: string;
  contact: {
    email: string;
    phone: string | null;
  };
}

type CustomerId = Customer["id"]; // string
type CustomerContact = Customer["contact"]; // { email: string; phone: string | null }
type CustomerPhone = Customer["contact"]["phone"]; // string | null
```

### Indexing with Unions
If the index `K` is a union type, the resulting type is the union of the indexed properties:

```typescript
type IdOrName = Customer["id" | "name"]; // string
type ContactDetails = Customer["contact"]["email" | "phone"]; // string | null
```

### Array and Tuple Element Access
Arrays and tuples can be indexed using `number` or literal numeric indices:

```typescript
type StringList = string[];
type ElementType = StringList[number]; // string

type Coordinate = [latitude: number, longitude: number, altitude?: number];
type Lat = Coordinate[0]; // number
type Long = Coordinate[1]; // number
type Alt = Coordinate[2]; // number | undefined
type AllCoordElements = Coordinate[number]; // number | undefined
```

---

## 3. How It Works Internally

```
Type Target T             Index K              Result T[K]
+-------------------+     +-------------+      +------------------+
| Customer {        |     |             |      |                  |
|   id: string      |  +  | "id" | "name"| ===> | string           |
|   name: string    |     |             |      |                  |
|   age: number     |     +-------------+      +------------------+
+-------------------+
```

When the compiler resolves `T[K]`:
1. It validates that `K extends keyof T`. If `K` includes keys not present in `T`, it emits error `TS2339` or `TS2536`.
2. It evaluates each member of `K`: for each key $k_i in K$, it fetches the member type in $T$.
3. It constructs a union of all resolved types: $igcup_{k in K} T[k]$.

---

## 4. Syntax & API Reference

### Chained Lookups
```typescript
interface ResponseEnvelope<T> {
  data: {
    items: T[];
    pagination: { total: number; offset: number };
  };
  meta: { traceId: string };
}

type ItemType<E> = E extends ResponseEnvelope<infer U> ? U : never;
type TotalType = ResponseEnvelope<any>["data"]["pagination"]["total"]; // number
```

### `number` Index on Const Tuples
```typescript
const ROLES = ["admin", "editor", "viewer"] as const;
type Role = (typeof ROLES)[number]; // "admin" | "editor" | "viewer"
```

---

## 5. Practical Examples

### 5.1 Basic Property and Array Element Lookup
```typescript
interface Order {
  orderId: string;
  lines: Array<{ sku: string; quantity: number; price: number }>;
}

type OrderLine = Order["lines"][number];
// { sku: string; quantity: number; price: number }
```

### 5.2 Real-World Application: Generic API Response Extractor
```typescript
interface ApiResponseContract {
  "/auth/login": {
    request: { user: string; pass: string };
    response: { token: string; expires: number };
  };
  "/users/me": {
    request: void;
    response: { id: string; username: string };
  };
}

async function apiCall<Path extends keyof ApiResponseContract>(
  path: Path,
  body: ApiResponseContract[Path]["request"]
): Promise<ApiResponseContract[Path]["response"]> {
  const res = await fetch(path, {
    method: "POST",
    body: body ? JSON.stringify(body) : undefined,
  });
  return res.json() as Promise<ApiResponseContract[Path]["response"]>;
}

// Strictly typed parameter and return:
const loginRes = await apiCall("/auth/login", { user: "admin", pass: "secret" });
console.log(loginRes.token);
```

### 5.3 Edge Case: Tuple Index Out of Bounds
```typescript
type Triplet = [string, number, boolean];
type First = Triplet[0]; // string
type Second = Triplet[1]; // number
type Third = Triplet[2]; // boolean
type Fourth = Triplet[3]; // undefined (in TS 5.x with tuple bound checking)
```

### 5.4 Failure Case: Indexing with Non-Existent Key
```typescript
interface Account {
  email: string;
  verified: boolean;
}

// @ts-expect-error TS2339: Property 'password' does not exist on type 'Account'.
type UserPassword = Account["password"];
```

### 5.5 Idiomatic Pattern: Value Extractors for Discriminated Unions
```typescript
type AppAction =
  | { type: "LOGIN"; payload: { token: string } }
  | { type: "LOGOUT"; payload: { reason: string } }
  | { type: "RESET"; payload: void };

// Extract payload for a specific action type
type ActionPayload<TAction extends { type: string }, TType extends TAction["type"]> =
  Extract<TAction, { type: TType }>["payload"];

type LoginData = ActionPayload<AppAction, "LOGIN">; // { token: string }
type LogoutData = ActionPayload<AppAction, "LOGOUT">; // { reason: string }
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Runtime Values Cannot Be Used as Indexes**: You cannot write `Customer[propertyName]` if `propertyName` is a runtime string variable. You must write `Customer[typeof propertyName]`.
2. **Distribution Over Unions**: If `T` is a union `A | B`, `(A | B)[K]` is `A[K] | B[K]`. However, `K` must be assignable to `(keyof A) & (keyof B)`.
3. **Optional Elements in Tuples**: Indexing optional tuple elements returns `T | undefined`.
4. **`T[number]` on Non-Array**: If applied to a type without a numeric index signature, the compiler emits `TS2536`.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Problem | Solution | Compiler Error |
| :--- | :--- | :--- | :--- |
| Passing variable to type index `T[varName]` | Variable name is a value, not a type | Use `T[typeof varName]` | `TS2749: 'varName' refers to a value, but is being used as a type here.` |
| Indexing union with non-common property | Property does not exist on all union members | Discriminate union first or use conditional type | `TS2339: Property 'x' does not exist on type 'A | B'.` |
| Creating duplicate sub-types manually | Types drift out of sync when base interface changes | Derive using indexed access (`Order["items"][number]`) | Drift / Incompatibility bugs |
| `Tuple[number]` expecting literal position | Loses index distinction; merges all element types | Use literal indices: `Tuple[0]`, `Tuple[1]` | Premature type widening |
| Using dot notation `T.prop` in type space | Dot is invalid type index syntax | Use bracket notation `T["prop"]` | `TS1005: ';' expected.` |
| Indexing `Record<string, V>` with unbounded generic | Key may not exist | Ensure `K extends string` | `TS2536: Type 'K' cannot be used to index type 'T'.` |

---

## 8. Performance & Type-Checker Complexity
- Chained indexed access `A[B][C][D]` is resolved efficiently via constant-time symbol table hash lookups.
- However, combining deeply nested indexed accesses with distributed conditional types can create large intersection/union type trees. Minimize circular or unbounded recursion in indexed lookups.

---

## 9. Compiler Internals
- Represented in the AST by `IndexedAccessTypeNode` (`kind: SyntaxKind.IndexedAccessType`), consisting of an `objectType` and an `indexType`.
- Evaluated in `checker.ts` by `getIndexedAccessType(objectType: Type, indexType: Type)`.
- If `indexType` is a type parameter, the compiler defers evaluation, creating an unevaluated `IndexedAccessType` object until instantiation time.

---

## 10. Best Practices & Design Guidelines
- Never redefine a nested type manually if it already exists as part of a parent structure. Use `Parent["child"]`.
- Extract array element types using `T[number]` instead of creating separate plural/singular interfaces.
- Pair `as const` array declarations with `[number]` to produce single-source-of-truth literal unions.

---

## 11. Real-World Case Study
### Strongly Typed State Management Slice Reducer
```typescript
interface RootApplicationState {
  auth: { user: { id: string; email: string } | null; token: string | null };
  cart: { items: Array<{ id: string; qty: number }>; coupon?: string };
  ui: { theme: "light" | "dark"; sidebarOpen: boolean };
}

type SliceKey = keyof RootApplicationState;

type SliceState<K extends SliceKey> = RootApplicationState[K];

function createSliceHandler<K extends SliceKey>(slice: K) {
  return {
    getInitialState: (): SliceState<K> => {
      // Mock initial state retrieval
      const defaults: RootApplicationState = {
        auth: { user: null, token: null },
        cart: { items: [] },
        ui: { theme: "light", sidebarOpen: false },
      };
      return defaults[slice];
    },
    update: (patch: Partial<SliceState<K>>): void => {
      console.log(`Updating slice ${slice}`, patch);
    },
  };
}

const authSlice = createSliceHandler("auth");
authSlice.update({ token: "jwt_token_123" });
```

---

## 12. Interview Questions

<details>
<summary>1. How do you extract the element type of an array or tuple in TypeScript?</summary>

Using the indexed access operator with `number`:
```typescript
type ElementOf<T extends readonly unknown[]> = T[number];
```
For a specific tuple index, pass the literal index: `MyTuple[0]`.
</details>

<details>
<summary>2. What happens if you index an interface with a union of two keys: <code>T["a" | "b"]</code>?</summary>

The compiler returns the union of the two property types: `T["a"] | T["b"]`.
</details>

<details>
<summary>3. Why can you not use dot notation like <code>Customer.contact</code> to lookup types?</summary>

In TypeScript, dot notation in type space is reserved for namespace and module qualification (e.g. `Namespace.Type`). Type indexed access mirrors JavaScript's bracket notation `T[K]`.
</details>

<details>
<summary>4. What is the difference between <code>arr[number]</code> on <code>string[]</code> vs on <code>const arr = ["a", "b"] as const</code>?</summary>

On `string[]`, `arr[number]` yields `string`. On the const tuple, it yields the exact literal union `"a" | "b"`.
</details>

<details>
<summary>5. How does <code>noUncheckedIndexedAccess</code> affect indexed access types?</summary>

`noUncheckedIndexedAccess` is a compiler flag that affects runtime indexed expressions (`obj[key]`), causing them to be typed as `T | undefined`. It does not alter static compile-time lookup types like `T["prop"]`, which remain strictly `T["prop"]`.
</details>

<details>
<summary>6. Can indexed access types be used on generic type parameters before they are instantiated?</summary>

Yes. If `T` is constrained (`<T extends Record<string, unknown>, K extends keyof T>`), `T[K]` is a deferred indexed access type that resolves once concrete arguments are supplied.
</details>

<details>
<summary>7. What happens when indexing a tuple with an out-of-bounds literal index?</summary>

In modern TypeScript, indexing an out-of-bounds index on a fixed-length tuple produces `undefined` or a compile-time type error.
</details>

<details>
<summary>8. How do you look up the return type of a method from an interface using indexed access?</summary>

Combine indexed access with `ReturnType`:
```typescript
interface Service {
  fetchData(): Promise<{ status: boolean }>;
}
type FetchResult = ReturnType<Service["fetchData"]>; // Promise<{ status: boolean }>
```
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Extract Element from Array Type (Easy)
Define a generic utility type `UnwrapArray<T>` that returns the element type of an array or `T` if not an array.
<details>
<summary>Solution</summary>

```typescript
type UnwrapArray<T> = T extends (infer U)[] ? U : T extends readonly (infer U)[] ? U : T;

type Test1 = UnwrapArray<string[]>; // string
type Test2 = UnwrapArray<number>;   // number
```
</details>

### Exercise 2: Safe Deep Getter Type (Easy)
Write a type `GetNested<T, K1 extends keyof T, K2 extends keyof T[K1]>` that returns `T[K1][K2]`.
<details>
<summary>Solution</summary>

```typescript
type GetNested<T, K1 extends keyof T, K2 extends keyof T[K1]> = T[K1][K2];

interface Root {
  user: { name: string; age: number };
}
type NameType = GetNested<Root, "user", "name">; // string
```
</details>

### Exercise 3: Discriminated Payload Extractor (Medium)
Given a discriminated union of network responses, write a utility type `ExtractResponse<T, Status>` that extracts the `body` property for that status.
<details>
<summary>Solution</summary>

```typescript
type HttpResponse =
  | { status: 200; body: { data: string } }
  | { status: 404; body: { error: string } }
  | { status: 500; body: { fatal: boolean } };

type ExtractResponse<T extends { status: number; body: unknown }, S extends T["status"]> =
  Extract<T, { status: S }>["body"];

type OkBody = ExtractResponse<HttpResponse, 200>; // { data: string }
type ErrBody = ExtractResponse<HttpResponse, 404>; // { error: string }
```
</details>

### Exercise 4: Tuple Head and Tail Types (Medium)
Implement `Head<T>` and `Tail<T>` for tuple types using indexed access and rest tuples.
<details>
<summary>Solution</summary>

```typescript
type Head<T extends readonly unknown[]> = T[0];
type Tail<T extends readonly unknown[]> = T extends readonly [unknown, ...infer Rest] ? Rest : [];

type H = Head<[string, number, boolean]>; // string
type T = Tail<[string, number, boolean]>; // [number, boolean]
```
</details>

### Exercise 5: Deep Path Value Resolver (Hard)
Given a path string `"a.b.c"`, resolve the property value type from an object type `T`.
<details>
<summary>Solution</summary>

```typescript
type PathValue<T, P extends string> =
  P extends `${infer Key}.${infer Rest}`
    ? Key extends keyof T
      ? PathValue<T[Key], Rest>
      : never
    : P extends keyof T
    ? T[P]
    : never;

interface Config {
  database: {
    connection: {
      host: string;
      port: number;
    };
  };
}

type PortType = PathValue<Config, "database.connection.port">; // number
```
</details>

### Exercise 6: Strongly Typed Mutable Property Setter (Hard)
Write an immutable deep updater function that accepts an object, a top-level key, and a second-level key, verifying the replacement value matches the indexed type.
<details>
<summary>Solution</summary>

```typescript
function updateNested<
  T extends object,
  K1 extends keyof T,
  K2 extends keyof T[K1]
>(obj: T, k1: K1, k2: K2, value: T[K1][K2]): T {
  return {
    ...obj,
    [k1]: {
      ...(obj[k1] as object),
      [k2]: value,
    },
  };
}

const conf = { db: { port: 5432, name: "prod" } };
const updated = updateNested(conf, "db", "port", 5433);
```
</details>
