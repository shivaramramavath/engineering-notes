# Deep Utility Types

> Deep utility types apply recursive property transformations—such as deep partials, deep immutability, deep requirement, and deep mutability—across arbitrary nested object graphs.

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [00-partial-required-readonly.md](file:///E:/Learning/engineering-notes/01-languages/typescript/07-utility-types/00-partial-required-readonly.md), [05-generic-inference.md](file:///E:/Learning/engineering-notes/01-languages/typescript/06-generics/05-generic-inference.md) |
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
Standard TypeScript utilities like `Partial<T>`, `Required<T>`, and `Readonly<T>` are strictly **shallow**:
```typescript
interface Config {
  db: {
    host: string;
    port: number;
  };
}

type P = Partial<Config>;
// db is optional, BUT if db is provided, host and port are still strictly required!
const invalid: P = { db: { host: "localhost" } }; // Error: Property 'port' is missing!
```

In complex enterprise architectures—such as state management stores (Redux, Zustand), configuration trees, ORM entities, and recursive document trees—types are deeply nested.

**Deep Utility Types** use recursive mapped types to traverse arbitrary object graphs, preserving functions, arrays, primitives, and collections while modifying all nested nodes.

---

## 2. Core Concepts

### `DeepPartial<T>`
Recursively marks all properties at all levels as optional:
```typescript
type DeepPartial<T> = T extends Function
  ? T
  : T extends Array<infer U>
  ? _DeepPartialArray<U>
  : T extends object
  ? _DeepPartialObject<T>
  : T;

type _DeepPartialArray<T> = Array<DeepPartial<T>>;
type _DeepPartialObject<T> = { [K in keyof T]?: DeepPartial<T[K]> };
```

### `DeepReadonly<T>`
Recursively adds `readonly` to every level, preventing nested mutation:
```typescript
type DeepReadonly<T> = T extends Function
  ? T
  : T extends ReadonlyArray<infer U>
  ? ReadonlyArray<DeepReadonly<U>>
  : T extends object
  ? { readonly [K in keyof T]: DeepReadonly<T[K]> }
  : T;
```

### `DeepRequired<T>`
Recursively strips optionality (`-?`) from all levels:
```typescript
type DeepRequired<T> = T extends Function
  ? T
  : T extends Array<infer U>
  ? Array<DeepRequired<U>>
  : T extends object
  ? { [K in keyof T]-?: DeepRequired<T[K]> }
  : T;
```

### `DeepMutable<T>`
Recursively removes `readonly` modifiers (`-readonly`):
```typescript
type DeepMutable<T> = T extends Function
  ? T
  : T extends readonly (infer U)[]
  ? Array<DeepMutable<U>>
  : T extends object
  ? { -readonly [K in keyof T]: DeepMutable<T[K]> }
  : T;
```

---

## 3. How It Works Internally

```
Type Evaluation Flow for DeepReadonly<T>:
               Is T a primitive or function?
                       /        \
                    Yes          No
                    /              \
             Return T directly   Is T an Array/Set/Map?
                                       /         \
                                    Yes           No (It's an Object)
                                    /               \
                      Recurse on elements      Recurse on properties
                      [ReadonlyArray<...>]     { readonly [K in keyof T]: ... }
```

Guard Conditions:
- **Functions**: Must be preserved without mapping. If a function is mapped with `[K in keyof T]`, its call signature is stripped, turning it into an empty object!
- **Built-in Objects**: Special classes like `Date`, `RegExp`, `Map`, and `Set` require explicit base-case handling to prevent stripping internal methods.

---

## 4. Syntax & API Reference

### Production-Grade Deep Readonly with Built-in Guards
```typescript
type BuiltIn = Function | Date | RegExp | Error;

export type ProductionDeepReadonly<T> = T extends BuiltIn
  ? T
  : T extends Map<infer K, infer V>
  ? ReadonlyMap<ProductionDeepReadonly<K>, ProductionDeepReadonly<V>>
  : T extends Set<infer U>
  ? ReadonlySet<ProductionDeepReadonly<U>>
  : T extends ReadonlyArray<infer U>
  ? ReadonlyArray<ProductionDeepReadonly<U>>
  : T extends object
  ? { readonly [K in keyof T]: ProductionDeepReadonly<T[K]> }
  : T;
```

---

## 5. Practical Examples

### 5.1 Basic Deep Partial
```typescript
interface SystemConfig {
  server: {
    connection: { host: string; port: number };
    ssl: { enabled: boolean; certPath?: string };
  };
  metrics: { intervalMs: number };
}

// Allows partial configuration at any arbitrary nesting depth:
const devConfig: DeepPartial<SystemConfig> = {
  server: {
    connection: { host: "localhost" } // port is omitted safely!
  }
};
```

### 5.2 Real-World Application: Immutability Enforcer in Redux Store
```typescript
interface StateTree {
  users: Array<{ id: string; profile: { bio: string } }>;
  session: { active: boolean; token: string };
}

function createImmutableStore<T extends object>(initialState: T) {
  let state = initialState as ProductionDeepReadonly<T>;
  return {
    getState(): ProductionDeepReadonly<T> {
      return state;
    },
    setState(newState: ProductionDeepReadonly<T>) {
      state = newState;
    }
  };
}

const store = createImmutableStore<StateTree>({
  users: [{ id: "u_1", profile: { bio: "engineer" } }],
  session: { active: true, token: "jwt" }
});

const current = store.getState();
// @ts-expect-error TS2540: Cannot assign to 'bio' because it is a read-only property.
current.users[0].profile.bio = "hacker";
```

### 5.3 Edge Case: Date and RegExp Objects
```typescript
interface AuditRecord {
  timestamp: Date;
  pattern: RegExp;
  data: { status: string };
}

type FrozenAudit = ProductionDeepReadonly<AuditRecord>;
// timestamp still has .getTime(), .toISOString() etc.
// pattern still has .test(), .exec() etc.
```

### 5.4 Failure Case: Unchecked Function Mapping
```typescript
type NaiveDeepReadonly<T> = { readonly [K in keyof T]: NaiveDeepReadonly<T[K]> };

interface ServiceWithMethods {
  fetchData(): string;
}

// Naive mapped type destroys the callable signature:
type Ruined = NaiveDeepReadonly<ServiceWithMethods>;
// Ruined is { readonly fetchData: {} } -- it can NO LONGER be called as a function!
```

### 5.5 Idiomatic Pattern: Recursive Deep Merge Function
```typescript
function deepMerge<T extends object>(target: T, source: DeepPartial<T>): T {
  const output: any = { ...target };
  for (const key of Object.keys(source)) {
    const sVal = (source as any)[key];
    const tVal = (target as any)[key];
    if (sVal && typeof sVal === "object" && !Array.isArray(sVal)) {
      output[key] = deepMerge(tVal || {}, sVal);
    } else if (sVal !== undefined) {
      output[key] = sVal;
    }
  }
  return output;
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Destruction of Functions**: Mapping over a function type (`{ [K in keyof Function]: ... }`) removes its call signatures. Always guard with `T extends Function ? T : ...`.
2. **Built-in Objects**: Classes with private properties or complex prototype methods (like `Date`, `RegExp`, `Blob`) should not be mapped.
3. **Circular / Self-Referential Types**: If a model references itself (`interface Node { parent: Node }`), recursive utilities must guard against infinite instantiation.
4. **Tuple Preservation**: Ensure arrays and tuples maintain their fixed lengths when mapping.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Problem | Solution | Compiler Error |
| :--- | :--- | :--- | :--- |
| Omitting function guard in recursive mapping | Strips call signatures from functions | Guard with `T extends Function ? T : ...` | Function becomes uncallable object |
| Not handling `Date` / `RegExp` base cases | Turns native objects into empty mapped records | Add `T extends BuiltIn ? T : ...` guard | Missing prototype methods |
| Expecting shallow `Partial<T>` to deep-patch | Nested required fields throw compile errors | Use `DeepPartial<T>` for configuration patches | `TS2741: Property 'x' is missing...` |
| Deep recursion on recursive ASTs without limit | Triggers infinite recursion in compiler | Cap recursion depth with tuple counter | `TS2589: Type instantiation is excessively deep` |
| Forgetting `ReadonlyArray` | Array elements remain mutable | Map arrays to `ReadonlyArray<DeepReadonly<U>>` | Arrays remain mutable via indices |
| Mutating `DeepReadonly` objects via casting | Evades compile-time immutability | Use runtime `Object.freeze` alongside types | Silent runtime bugs |

---

## 8. Performance & Type-Checker Complexity
- Deep recursive types consume $O(D 	imes K)$ instantiations where $D$ is graph depth and $K$ is property count.
- Keep deep utilities constrained to domain boundaries (state store, config resolver) rather than wrapping every generic parameter.

---

## 9. Compiler Internals
- Recursive mapped types utilize lazy symbol resolution.
- In `src/compiler/checker.ts`, properties of mapped types are resolved on-demand when accessed by user code rather than instantiating the entire deep tree upfront.

---

## 10. Best Practices & Design Guidelines
- Include guards for all ECMAScript built-in types (`Function`, `Date`, `RegExp`, `Map`, `Set`, `Promise`).
- Pair `DeepReadonly<T>` with recursive runtime `Object.freeze` for critical application state.
- Keep deep utilities centralized in a core typing library for reuse across services.

---

## 11. Real-World Case Study
### Type-Safe Nested Configuration Manager with Environment Overrides
```typescript
interface DatabaseConfig {
  primary: { host: string; port: number; pool: { min: number; max: number } };
  replica?: { host: string; port: number };
}

interface ApplicationConfig {
  env: "production" | "staging" | "development";
  db: DatabaseConfig;
  logging: { level: "info" | "debug" | "error"; format: "json" | "text" };
}

const DEFAULT_CONFIG: ApplicationConfig = {
  env: "development",
  db: {
    primary: { host: "127.0.0.1", port: 5432, pool: { min: 2, max: 10 } }
  },
  logging: { level: "info", format: "text" }
};

function createConfig(overrides: DeepPartial<ApplicationConfig>): ProductionDeepReadonly<ApplicationConfig> {
  return deepMerge(DEFAULT_CONFIG, overrides) as any;
}

// Deeply partial override without boilerplate:
const config = createConfig({
  db: {
    primary: { pool: { max: 20 } }
  }
});

console.log(config.db.primary.pool.max); // 20
```

---

## 12. Interview Questions

<details>
<summary>1. Why does standard <code>Partial&lt;T&gt;</code> not work for nested configurations?</summary>

Standard `Partial<T>` is shallow: it only applies the optional modifier (`?`) to the top-level keys of `T`. If a nested property is an object, that nested object retains all of its required fields unless explicitly typed with `DeepPartial<T>`.
</details>

<details>
<summary>2. Why must recursive mapped types guard against <code>Function</code>?</summary>

In TypeScript, a function is an object (`T extends object`). If a recursive mapped type does not exclude functions, it iterates over `keyof Function` (e.g. `bind`, `call`, `apply`) and strips the callable call signature, resulting in a non-callable object.
</details>

<details>
<summary>3. How do you prevent <code>DeepReadonly&lt;T&gt;</code> from ruining <code>Date</code> instances?</summary>

By adding an explicit guard:
```typescript
type DeepReadonly<T> = T extends Date ? T : ...
```
This preserves `Date` methods (`getTime`, `toISOString`) without transforming its private properties.
</details>

<details>
<summary>4. How does <code>DeepMutable&lt;T&gt;</code> strip readonly modifiers recursively?</summary>

By applying `-readonly` at every level of recursive object and array mapping:
```typescript
type DeepMutable<T> = T extends readonly (infer U)[]
  ? Array<DeepMutable<U>>
  : T extends object
  ? { -readonly [K in keyof T]: DeepMutable<T[K]> }
  : T;
```
</details>

<details>
<summary>5. What happens when a recursive type is applied to a self-referential interface?</summary>

If the self-referential interface is evaluated infinitely, TypeScript halts with `TS2589: Type instantiation is excessively deep and possibly infinite`.
</details>

<details>
<summary>6. How can you handle <code>Map</code> and <code>Set</code> in <code>DeepReadonly</code>?</summary>

Map `Map<K, V>` to `ReadonlyMap<DeepReadonly<K>, DeepReadonly<V>>` and `Set<U>` to `ReadonlySet<DeepReadonly<U>>`.
</details>

<details>
<summary>7. What is the difference between <code>DeepRequired&lt;T&gt;</code> and <code>DeepNonNullable&lt;T&gt;</code>?</summary>

`DeepRequired<T>` removes the optional property modifier (`-?`) recursively. `DeepNonNullable<T>` strips `null` and `undefined` types from all property values recursively.
</details>

<details>
<summary>8. How does TypeScript optimize compiler memory when using deep mapped types?</summary>

TypeScript uses deferred evaluation for properties of mapped types. Properties are only recursively expanded when a specific property is queried or accessed by user code.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Basic Deep Partial (Easy)
Implement `SimpleDeepPartial<T>` for plain JSON objects.
<details>
<summary>Solution</summary>

```typescript
type SimpleDeepPartial<T> = T extends object
  ? { [K in keyof T]?: SimpleDeepPartial<T[K]> }
  : T;

type Config = { a: { b: { c: number } } };
type PartialConf = SimpleDeepPartial<Config>;
const test: PartialConf = { a: { b: {} } };
```
</details>

### Exercise 2: Deep Readonly for Tuples and Arrays (Easy)
Write `DeepReadonlyArray<T>` that converts nested arrays into `ReadonlyArray`.
<details>
<summary>Solution</summary>

```typescript
type DeepReadonlyArray<T> = T extends Array<infer U>
  ? ReadonlyArray<DeepReadonlyArray<U>>
  : T;

type Matrix = number[][];
type FrozenMatrix = DeepReadonlyArray<Matrix>;
// ReadonlyArray<ReadonlyArray<number>>
```
</details>

### Exercise 3: Deep Nullable (Medium)
Create a utility `DeepNullable<T>` that adds `null` to every leaf property value in an object tree.
<details>
<summary>Solution</summary>

```typescript
type DeepNullable<T> = T extends Function
  ? T
  : T extends object
  ? { [K in keyof T]: DeepNullable<T[K]> }
  : T | null;

interface State {
  user: { name: string; age: number };
}
type NullableState = DeepNullable<State>;
// { user: { name: string | null; age: number | null } }
```
</details>

### Exercise 4: Deep Required Utility (Medium)
Implement `DeepRequired<T>` that recursively removes optional modifiers from nested objects.
<details>
<summary>Solution</summary>

```typescript
type DeepRequired<T> = T extends Function
  ? T
  : T extends Array<infer U>
  ? Array<DeepRequired<U>>
  : T extends object
  ? { [K in keyof T]-?: DeepRequired<T[K]> }
  : T;

interface PartialTree {
  a?: { b?: { c?: string } };
}
type StrictTree = DeepRequired<PartialTree>;
// { a: { b: { c: string } } }
```
</details>

### Exercise 5: Depth-Guarded Deep Partial (Hard)
Implement `BoundedDeepPartial<T, Depth extends number = 3>` to prevent runaway recursion.
<details>
<summary>Solution</summary>

```typescript
type Prev = [never, 0, 1, 2, 3];

type BoundedDeepPartial<T, Depth extends number = 3> = [Depth] extends [0]
  ? T
  : T extends Function
  ? T
  : T extends object
  ? { [K in keyof T]?: BoundedDeepPartial<T[K], Prev[Depth]> }
  : T;

interface DeepTree {
  l1: { l2: { l3: { l4: { l5: string } } } };
}
type Safe = BoundedDeepPartial<DeepTree, 2>;
```
</details>

### Exercise 6: Deep NonNullable (Hard)
Implement `DeepNonNullable<T>` that strips `null` and `undefined` recursively from all objects, arrays, and tuple properties.
<details>
<summary>Solution</summary>

```typescript
type DeepNonNullable<T> = T extends (...args: any[]) => any
  ? T
  : T extends Array<infer U>
  ? Array<DeepNonNullable<NonNullable<U>>>
  : T extends ReadonlyArray<infer U>
  ? ReadonlyArray<DeepNonNullable<NonNullable<U>>>
  : T extends object
  ? { [K in keyof T]: DeepNonNullable<NonNullable<T[K]>> }
  : NonNullable<T>;

type Raw = {
  a?: string | null;
  b: Array<number | null | undefined>;
  c: { d: boolean | undefined };
};

type Pristine = DeepNonNullable<Raw>;
// { a: string; b: number[]; c: { d: boolean } }
```
</details>
