# Building Custom Utility Types

> Constructing custom utility types combines mapped types, key remapping via `as`, template literal filters, and recursive conditional evaluations to create domain-specific type transformations.

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [00-partial-required-readonly.md](file:///E:/Learning/engineering-notes/01-languages/typescript/07-utility-types/00-partial-required-readonly.md), [05-generic-inference.md](file:///E:/Learning/engineering-notes/01-languages/typescript/06-generics/05-generic-inference.md) |
| **Time to Read** | 26 minutes |
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
While TypeScript includes standard utilities (`Partial`, `Omit`, `Pick`, `Record`), real-world enterprise architectures often demand domain-tailored type logic:
- Making only a specific subset of keys optional or required.
- Selecting only properties whose values match a given type (e.g. all functions or all dates).
- Renaming object keys (e.g. prefixing getters with `get`, camelCasing snake_case keys).
- Recursively deep-freezing nested data graphs.

Understanding how to construct custom utility types allows engineering teams to enforce architectural constraints at compile time, eliminating human error across large monorepos.

---

## 2. Core Concepts

### Key Remapping via `as`
Introduced in TypeScript 4.1, key remapping in mapped types lets you transform property names using an `as` clause:
```typescript
type Getters<T> = {
  [K in keyof T as `get${Capitalize<string & K>}`]: () => T[K];
};

interface User {
  name: string;
  age: number;
}

type UserGetters = Getters<User>;
// { getName: () => string; getAge: () => number }
```

### Filtering Keys with `never`
In key remapping, if a key resolves to `never`, TypeScript drops that key completely from the resulting type:
```typescript
type PickByType<T, ValueType> = {
  [K in keyof T as T[K] extends ValueType ? K : never]: T[K];
};

interface Model {
  id: string;
  count: number;
  isActive: boolean;
  tag: string;
}

type OnlyStrings = PickByType<Model, string>;
// { id: string; tag: string }
```

---

## 3. How It Works Internally

```
Source Type T                 Key Filter & Transform               Resulting Type
+---------------------+      +------------------------+          +--------------------+
| id: string          | ===> | K = "id" (keeps "id")   | =======> | id: string         |
| count: number       | ===> | K = "count" (mapped to |          | tag: string        |
| isActive: boolean   | ===> |       never => DROPPED)|          +--------------------+
| tag: string         | ===> | K = "tag" (keeps "tag")|
+---------------------+      +------------------------+
```

When the TypeScript compiler evaluates `[K in keyof T as NewKey<K>]`:
1. It iterates over every property symbol $K$ in $T$.
2. It evaluates the expression in the `as` clause to determine the output key name.
3. If the resulting name is assignable to `string | number | symbol`, a property is created with the transformed name and the mapped value type.
4. If the `as` clause evaluates to `never`, the property is omitted entirely from the output symbol table.

---

## 4. Syntax & API Reference

### Common Architectural Building Blocks
```typescript
// 1. Selective Optional
type OptionalKeys<T, K extends keyof T> = Omit<T, K> & Partial<Pick<T, K>>;

// 2. Selective Required
type ConcreteKeys<T, K extends keyof T> = Omit<T, K> & Required<Pick<T, K>>;

// 3. Omitting by Value Type
type OmitByType<T, ValueType> = {
  [K in keyof T as T[K] extends ValueType ? never : K]: T[K];
};

// 4. Exact Property Difference
type ObjectDiff<T, U> = {
  [K in keyof T as K extends keyof U ? never : K]: T[K];
};
```

---

## 5. Practical Examples

### 5.1 Basic Key Filtering
```typescript
interface Entity {
  id: number;
  name: string;
  createdAt: Date;
  updatedAt: Date;
  save(): void;
}

// Extract only data fields (omit functions)
type NonFunctionProperties<T> = {
  [K in keyof T as T[K] extends Function ? never : K]: T[K];
};

type EntityData = NonFunctionProperties<Entity>;
// { id: number; name: string; createdAt: Date; updatedAt: Date }
```

### 5.2 Real-World Application: Automated DTO Builder with Event Emitter
```typescript
interface AccountState {
  balance: number;
  currency: string;
  ownerId: string;
}

type StateChangeEvents<T> = {
  [K in keyof T as `on${Capitalize<string & K>}Changed`]: (newValue: T[K], prevValue: T[K]) => void;
};

type AccountListeners = StateChangeEvents<AccountState>;
// {
//   onBalanceChanged: (newValue: number, prevValue: number) => void;
//   onCurrencyChanged: (newValue: string, prevValue: string) => void;
//   onOwnerIdChanged: (newValue: string, prevValue: string) => void;
// }
```

### 5.3 Edge Case: Symbol and Numeric Keys with Template Literals
```typescript
const sym = Symbol("secret");
interface MixedKeys {
  [sym]: string;
  42: number;
  str: boolean;
}

// string & K filters out symbols because symbols cannot be concatenated into template strings:
type SafeStringKeys<T> = {
  [K in keyof T as K extends string ? `prefixed_${K}` : never]: T[K];
};

type Prefixed = SafeStringKeys<MixedKeys>;
// { prefixed_str: boolean } (42 and Symbol are safely dropped)
```

### 5.4 Failure Case: Template Literal on Unchecked Symbol
```typescript
// @ts-expect-error TS2322: Type 'symbol' is not assignable to type 'string | number | bigint | boolean | null | undefined'.
type BadPrefix<T> = { [K in keyof T as `pre_${K}`]: T[K] };
```

### 5.5 Idiomatic Pattern: Strict Non-Empty Array
```typescript
type NonEmptyArray<T> = [T, ...T[]];

function processBatch<T>(items: NonEmptyArray<T>): T {
  // Guaranteed at least one element at compile time:
  return items[0];
}

processBatch(["item1", "item2"]); // OK
// @ts-expect-error TS2322: Source has 0 element(s) but target requires 1.
processBatch([]);
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Symbols in Template Literals**: `keyof T` can contain `symbol`. Concatenating symbols in template strings causes a compile error. Always intersect with `string` (`string & K`) or check `K extends string`.
2. **Homomorphic vs. Non-homomorphic Mapped Types**:
   - Homomorphic: `[K in keyof T]` preserves modifiers (`readonly`, `?`).
   - Non-homomorphic: `[K in string]` or `[K in keyof T as ...]` does **not** automatically preserve homomorphic modifiers in older TS versions; in modern TS, key remapping preserves property flags unless explicitly stripped with `-?` or `-readonly`.
3. **Union Preservation**: Key remapping works per property key, not per union branch. If `T` is a union, distribute `T extends any` first.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Problem | Solution | Compiler Error |
| :--- | :--- | :--- | :--- |
| Concatenating unconstrained `K` into template | `keyof T` contains `symbol` | Use `string & K` or `K extends string ? ... : never` | `TS2322: Type 'symbol' not assignable to...` |
| Using `as never` to filter without understanding | Expecting `never` property value instead of drop | Key mapped to `never` drops the property entirely | Missing properties |
| Recursive utility without base case termination | Unchecked recursion loops until depth limit | Use tuple counter or conditional check against primitive | `TS2589: Type instantiation is excessively deep` |
| Creating intersections instead of mapped types | `A & B & C` creates unwieldy types in hover tooltips | Use mapped type `{ [K in keyof T]: T[K] }` to flatten | Cluttered IDE hover hints |
| Forgetting `readonly` stripping | `Pick` preserves readonly from source | Use `-readonly` to unlock mutation | `TS2540: Cannot assign to read-only property` |
| Mapping over `any` | Returns empty or any-keyed object | Add type parameter constraint `<T extends object>` | Loose typing |

---

## 8. Performance & Type-Checker Complexity
- Authoring custom utility types with `as` key remapping is highly optimized in the compiler.
- Avoid nesting multi-level string parsing (e.g. regex simulators in template literals) within mapped keys across large objects, which can increase type-checking latency.

---

## 9. Compiler Internals
- In `src/compiler/checker.ts`, `resolveMappedTypeMembers` checks for `type.asNameType`.
- If present, `asNameType` is instantiated for each key symbol.
- If the resolved name type is `neverType`, the property symbol is omitted from the member table.

---

## 10. Best Practices & Design Guidelines
- Create a shared `Prettify<T>` / `Id<T>` utility to simplify complex nested types in IDE hover hints:
  ```typescript
  type Prettify<T> = { [K in keyof T]: T[K] } & {};
  ```
- Keep custom utility types generic, pure, and decoupled from domain logic.
- Always add unit tests using `@ts-expect-error` and type assertion tests to verify edge cases.

---

## 11. Real-World Case Study
### Strongly Typed Auto-Generated Event Publisher
```typescript
interface DatabaseModels {
  user: { id: string; email: string };
  order: { orderId: string; totalCents: number };
  payment: { paymentId: string; method: string };
}

type CrudEvents<TModels> = {
  [K in keyof TModels as `${string & K}:created`]: (entity: TModels[K]) => void;
} & {
  [K in keyof TModels as `${string & K}:deleted`]: (id: string) => void;
};

class EventBus {
  private handlers = new Map<string, Function[]>();

  on<K extends keyof CrudEvents<DatabaseModels>>(
    event: K,
    handler: CrudEvents<DatabaseModels>[K]
  ): void {
    const list = this.handlers.get(event) || [];
    list.push(handler);
    this.handlers.set(event, list);
  }
}

const bus = new EventBus();
bus.on("user:created", (user) => console.log(user.email));
bus.on("order:deleted", (id) => console.log(id));
```

---

## 12. Interview Questions

<details>
<summary>1. How does key remapping with <code>as</code> work in TypeScript 4.1+?</summary>

In a mapped type `[K in keyof T as NewKey]`, the `as` clause allows projecting the property name into a new name (for example, applying template literal types like ``get${Capitalize<string & K>}``). If the clause evaluates to `never`, the key is filtered out completely.
</details>

<details>
<summary>2. How do you filter out keys by value type in a mapped type?</summary>

By combining key remapping with a conditional type that evaluates to `never` when the value type does not match:
```typescript
type PickByType<T, V> = {
  [K in keyof T as T[K] extends V ? K : never]: T[K];
};
```
</details>

<details>
<summary>3. What does <code>type Prettify&lt;T&gt; = { [K in keyof T]: T[K] } &amp; {}</code> do?</summary>

It forces the TypeScript compiler to eagerly evaluate and flatten intersections or complex mapped types into a single clean object literal representation, improving readability in IDE hover previews.
</details>

<details>
<summary>4. Why must you write <code>string &amp; K</code> when using keys in template literal types?</summary>

Because `keyof T` can contain `string | number | symbol`. TypeScript does not allow `symbol` to be embedded inside template literal types. Intersecting with `string` filters out numbers and symbols safely.
</details>

<details>
<summary>5. How can you make a utility that makes only a specific subset of properties required?</summary>

```typescript
type RequireOnly<T, K extends keyof T> = Omit<T, K> & Required<Pick<T, K>>;
```
</details>

<details>
<summary>6. What is the difference between a homomorphic and non-homomorphic mapped type?</summary>

A homomorphic mapped type is of the form `[K in keyof T]` and automatically copies modifiers (`readonly`, `?`) from the source type. A non-homomorphic mapped type maps over arbitrary types (e.g. `[K in string]`) and does not inherit property modifiers.
</details>

<details>
<summary>7. How can you extract all required keys of an object type?</summary>

```typescript
type RequiredKeys<T> = {
  [K in keyof T]-?: {} extends Pick<T, K> ? never : K;
}[keyof T];
```
</details>

<details>
<summary>8. How do you implement a <code>DeepPartial&lt;T&gt;</code> custom utility?</summary>

```typescript
type DeepPartial<T> = T extends Function
  ? T
  : T extends Array<infer U>
  ? Array<DeepPartial<U>>
  : T extends object
  ? { [K in keyof T]?: DeepPartial<T[K]> }
  : T;
```
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Extract Keys by Value (Easy)
Write a utility `StringKeys<T>` that extracts all keys whose values are assignable to `string`.
<details>
<summary>Solution</summary>

```typescript
type StringKeys<T> = {
  [K in keyof T]: T[K] extends string ? K : never;
}[keyof T];

interface Data { a: string; b: number; c: string; }
type Keys = StringKeys<Data>; // "a" | "c"
```
</details>

### Exercise 2: Prefix All Keys (Easy)
Write a utility `PrefixKeys<T, Prefix extends string>` that prefixes all string keys with `Prefix`.
<details>
<summary>Solution</summary>

```typescript
type PrefixKeys<T, Prefix extends string> = {
  [K in keyof T as K extends string ? `${Prefix}_${K}` : never]: T[K];
};

interface Conf { host: string; port: number; }
type Prefixed = PrefixKeys<Conf, "env">;
// { env_host: string; env_port: number }
```
</details>

### Exercise 3: Filter Nullable Properties (Medium)
Create a utility `NotNullableProperties<T>` that strips `null` and `undefined` from every property value.
<details>
<summary>Solution</summary>

```typescript
type NotNullableProperties<T> = {
  [K in keyof T]: NonNullable<T[K]>;
};

interface RawInput {
  name: string | null;
  age: number | undefined;
}
type Clean = NotNullableProperties<RawInput>;
// { name: string; age: number }
```
</details>

### Exercise 4: CamelCase to SnakeCase Key Mapper (Medium)
Convert camelCase keys like `userId` into snake_case `user_id`.
<details>
<summary>Solution</summary>

```typescript
type CamelToSnake<S extends string> = S extends `${infer T}${infer U}`
  ? `${T extends Lowercase<T> ? "" : "_"}${Lowercase<T>}${CamelToSnake<U>}`
  : S;

type SnakeKeys<T> = {
  [K in keyof T as K extends string ? CamelToSnake<K> : K]: T[K];
};

interface User { userId: string; firstName: string; }
type SnakeUser = SnakeKeys<User>;
// { user_id: string; first_name: string }
```
</details>

### Exercise 5: Deep Mutable Utility (Hard)
Implement `DeepMutable<T>` that recursively removes `readonly` from objects, arrays, and tuples.
<details>
<summary>Solution</summary>

```typescript
type DeepMutable<T> = T extends (...args: any[]) => any
  ? T
  : T extends readonly (infer U)[]
  ? Array<DeepMutable<U>>
  : T extends object
  ? { -readonly [K in keyof T]: DeepMutable<T[K]> }
  : T;

type ReadonlyConfig = {
  readonly host: string;
  readonly tags: readonly string[];
  readonly db: { readonly port: number };
};

type WritableConfig = DeepMutable<ReadonlyConfig>;
// { host: string; tags: string[]; db: { port: number } }
```
</details>

### Exercise 6: Strictly Checked Path-Based Value Getter (Hard)
Implement a strongly-typed dot-path accessor `getDeep<T, P extends Path<T>>(obj: T, path: P): PathValue<T, P>`.
<details>
<summary>Solution</summary>

```typescript
type Path<T> = T extends object
  ? {
      [K in keyof T & string]: T[K] extends object
        ? K | `${K}.${Path<T[K]>}`
        : K;
    }[keyof T & string]
  : never;

type PathValue<T, P extends string> =
  P extends `${infer Key}.${infer Rest}`
    ? Key extends keyof T
      ? PathValue<T[Key], Rest>
      : never
    : P extends keyof T
    ? T[P]
    : never;

function getDeep<T extends object, P extends Path<T>>(obj: T, path: P): PathValue<T, P> {
  const parts = path.split(".");
  let curr: any = obj;
  for (const part of parts) {
    curr = curr[part];
  }
  return curr;
}

const conf = { db: { primary: { host: "db1" } } };
const h = getDeep(conf, "db.primary.host"); // type: string
```
</details>
