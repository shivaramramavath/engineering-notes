# Generic Inference and Conditional Types

> Generic inference uses the `infer` keyword within conditional type branches to pattern-match and unwrap inner types, powering advanced type transformation and extraction pipelines.

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [00-generic-functions.md](file:///E:/Learning/engineering-notes/01-languages/typescript/06-generics/00-generic-functions.md), [04-indexed-access-types.md](file:///E:/Learning/engineering-notes/01-languages/typescript/06-generics/04-indexed-access-types.md) |
| **Time to Read** | 26 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 2.8 / TS 5.x / TS 7.0 Native |
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
TypeScript's type system operates as a functional, declarative language at compile time. Before TypeScript 2.8, developers could not inspect or destructure the types embedded inside generic wrappers—such as extracting the resolved type $T$ from a `Promise<T>`, the return type from a function, or the payload from an event.

Conditional types introduced ternary logic to types:
```typescript
T extends U ? TrueBranch : FalseBranch
```
Coupled with the **`infer`** keyword, conditional types enable structural pattern matching: the compiler inspects the shape of `T`, binds sub-components to fresh type variables, and makes them available inside the true branch.

---

## 2. Core Concepts

### Conditional Types Mechanics
A conditional type tests whether `T` is assignable to `U`:
```typescript
type IsString<T> = T extends string ? true : false;
type A = IsString<"hello">; // true
type B = IsString<42>;      // false
```

### Type Inference with `infer`
The `infer` keyword can only appear inside the `extends` clause of a conditional type. It tells TypeScript: "Introduce a new type variable here and let the compiler infer its identity by pattern matching against `T`":

```typescript
type Flatten<T> = T extends Array<infer Element> ? Element : T;

type Str = Flatten<string[]>; // string
type Num = Flatten<number>;   // number (falls through to false branch)
```

### Distributive Conditional Types
When a conditional type operates on a bare generic type parameter `T`, it automatically distributes over union types:
```typescript
type ToArray<T> = T extends any ? T[] : never;
type Mixed = ToArray<string | number>; // string[] | number[]
```
If you want to prevent distribution, wrap `T` and `any` in tuple brackets: `[T] extends [any]`.

---

## 3. How It Works Internally

```
Target Type T: Promise<User>
Condition:     T extends Promise<infer R> ? R : never
                         |        |
                         | match  +---------> Bind R = User
                         v
Result:        User
```

When the compiler resolves `T extends Pattern<infer R> ? TrueType : FalseType`:
1. It compares `T` against `Pattern`.
2. Any position marked with `infer R` becomes an inference candidate hole.
3. The type checker runs structural subtyping from `T` into `Pattern`.
4. If a valid type for `R` satisfies the relationship, `R` is instantiated with that candidate and the `TrueType` branch is evaluated.
5. If `T` does not match the structural pattern, the `FalseType` branch is taken and `R` is not in scope.

---

## 4. Syntax & API Reference

### Built-in Conditional Utilities Powered by `infer`
```typescript
// Extract return type of a function
type ReturnType<T extends (...args: any) => any> = T extends (...args: any) => infer R ? R : any;

// Extract parameter tuple
type Parameters<T extends (...args: any) => any> = T extends (...args: infer P) => any ? P : never;

// Extract constructor instance
type InstanceType<T extends abstract new (...args: any) => any> = T extends abstract new (...args: any) => infer R ? R : any;

// Unwrap Promise (recursive in TS 4.5+)
type MyAwaited<T> = T extends Promise<infer U> ? MyAwaited<U> : T;
```

### Multiple `infer` Declarations
```typescript
type UnpackKeyValue<T> = T extends Map<infer K, infer V> ? [K, V] : never;
type MapTypes = UnpackKeyValue<Map<string, number>>; // [string, number]
```

---

## 5. Practical Examples

### 5.1 Basic Function Return Type Inference
```typescript
function fetchUser() {
  return { id: 101, username: "shiva", role: "architect" as const };
}

type UserResult = ReturnType<typeof fetchUser>;
// { id: number; username: string; role: "architect" }
```

### 5.2 Real-World Application: Redux Action Type Extractor
```typescript
interface ActionCreators {
  setAge: (age: number) => { type: "SET_AGE"; payload: number };
  setName: (name: string) => { type: "SET_NAME"; payload: string };
  reset: () => { type: "RESET" };
}

type ExtractActions<T> = {
  [K in keyof T]: T[K] extends (...args: any[]) => infer Action ? Action : never;
}[keyof T];

type AppActions = ExtractActions<ActionCreators>;
// { type: "SET_AGE"; payload: number } | { type: "SET_NAME"; payload: string } | { type: "RESET" }
```

### 5.3 Edge Case: Co- and Contra-Variant Inference Positions
```typescript
// Inferring from a parameter position (contravariant)
type ArgumentType<T> = T extends (arg: infer A) => void ? A : never;

// When multiple function signatures overload or intersect:
type Overloaded = ((x: string) => void) & ((x: number) => void);
type InferArg = ArgumentType<Overloaded>; // string & number (never) or intersection!
```

### 5.4 Failure Case: `infer` Outside Extends Clause
```typescript
// @ts-expect-error TS1338: 'infer' declarations are only permitted in the 'extends' clause of a conditional type.
type InvalidInfer<T> = infer U;
```

### 5.5 Idiomatic Pattern: Async Function Unwrapper
```typescript
type AsyncReturnType<T extends (...args: any[]) => Promise<any>> =
  T extends (...args: any[]) => Promise<infer R> ? R : never;

async function queryDatabase(query: string) {
  return [{ id: 1, row: query }];
}

type QueryResult = AsyncReturnType<typeof queryDatabase>;
// { id: number; row: string }[]
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Union Distribution**: `T extends any` splits unions. To prevent `(A | B)` from distributing into `Branch<A> | Branch<B>`, use tuple syntax `[T] extends [Pattern]`.
2. **Never Distribution**: Conditional types distribute over `never` as an empty union, resulting in `never` without evaluating the false branch!
   ```typescript
   type CheckNever<T> = T extends never ? true : false;
   type Res = CheckNever<never>; // never, NOT true!
   type SafeCheckNever<T> = [T] extends [never] ? true : false;
   type SafeRes = SafeCheckNever<never>; // true
   ```
3. **Multiple Candidates in Covariant Positions**: If `infer R` appears multiple times in covariant positions (like return types), the compiler forms a **union**:
   ```typescript
   type Foo<T> = T extends { a: infer U; b: infer U } ? U : never;
   type T1 = Foo<{ a: string; b: number }>; // string | number
   ```
4. **Multiple Candidates in Contravariant Positions**: If `infer R` appears in contravariant positions (function arguments), the compiler forms an **intersection**:
   ```typescript
   type Bar<T> = T extends { a: (x: infer U) => void; b: (x: infer U) => void } ? U : never;
   type T2 = Bar<{ a: (x: string) => void; b: (x: number) => void }>; // string & number (never)
   ```

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Problem | Solution | Compiler Error |
| :--- | :--- | :--- | :--- |
| Checking `T extends never` directly | Union distribution collapses `never` to `never` | Use `[T] extends [never]` | Unexpected `never` result |
| Using `infer` in top-level type alias | `infer` is only valid inside conditional `extends` | Nest inside `T extends ... ? ... : ...` | `TS1338: 'infer' declarations are only permitted...` |
| Over-distributing unions | Unintended Cartesian product of union members | Wrap both sides in tuples `[T] extends [U]` | Broadened union type |
| Failing to handle recursive promises | Fails when promise resolves to another promise | Use recursive conditional type or `Awaited<T>` | `Promise<Promise<string>>` leakage |
| Unconstrained `infer` matching `unknown` | Type checker cannot narrow the inferred type | Add extends to infer: `infer R extends Base` | `TS2344: Type does not satisfy constraint` |
| Writing deep nested ternaries | Unreadable, causes compiler stack depth limit | Break into modular single-responsibility types | `TS2589: Type instantiation is excessively deep` |

---

## 8. Performance & Type-Checker Complexity
- Recursive conditional types with `infer` consume type-checker call stack frames. TypeScript enforces a limit of 1000 recursive instantiations before raising error `TS2589`.
- Always provide early base-case termination branches in recursive conditional types.

---

## 9. Compiler Internals
- `infer` expressions parse into `InferTypeNode` (`kind: SyntaxKind.InferType`).
- Resolved in `src/compiler/checker.ts` through `getTypeFromConditionalType`.
- When an `InferTypeNode` is present, the checker sets up an inference context via `createInferenceContext(typeParameters, ...)`.
- During `checkTypeRelatedTo`, inferred type arguments are populated into candidate accumulators. If co-variant, candidates are merged via `getUnionType`; if contra-variant, via `getIntersectionType`.

---

## 10. Best Practices & Design Guidelines
- Prefer the standard library `Awaited<T>` over custom Promise unwrappers.
- Use `infer` with type constraints (`infer R extends HTMLElement`, TS 4.7+) to immediately enforce bounds on inferred types.
- Always guard against distributive behavior when operating on union parameters where the union should be evaluated as an atomic unit (`[T] extends [U]`).

---

## 11. Real-World Case Study
### Type-Safe RPC Protocol Dispatcher
```typescript
type RemoteService = {
  getUser(req: { id: string }): Promise<{ id: string; name: string }>;
  listOrders(req: { limit: number }): Promise<Array<{ orderId: string; total: number }>>;
  ping(): Promise<"pong">;
};

// Extract request and response types for any remote procedure:
type ProcedureRequest<T, K extends keyof T> =
  T[K] extends (...args: infer Args) => any ? (Args["length"] extends 0 ? void : Args[0]) : never;

type ProcedureResponse<T, K extends keyof T> =
  T[K] extends (...args: any[]) => Promise<infer Res> ? Res : never;

class RpcClient<T> {
  async call<K extends keyof T>(
    method: K,
    ...args: ProcedureRequest<T, K> extends void ? [] : [ProcedureRequest<T, K>]
  ): Promise<ProcedureResponse<T, K>> {
    console.log(`RPC Invocation: ${String(method)}`, args);
    return {} as ProcedureResponse<T, K>;
  }
}

const client = new RpcClient<RemoteService>();
const user = await client.call("getUser", { id: "u_99" });
const pong = await client.call("ping");
```

---

## 12. Interview Questions

<details>
<summary>1. What is the purpose of the <code>infer</code> keyword in TypeScript?</summary>

The `infer` keyword introduces a generic type variable dynamically inside the condition of a conditional type (`T extends SomePattern<infer U> ? U : never`), allowing the type checker to deduce and extract sub-types from complex composite types.
</details>

<details>
<summary>2. What is a distributive conditional type and how do you disable distribution?</summary>

A conditional type is distributive when the condition checks a bare type parameter (`T extends U ? X : Y`). When `T` is a union `A | B`, it distributes across the union as `(A extends U ? X : Y) | (B extends U ? X : Y)`. To disable distribution, wrap the operand and pattern in square brackets: `[T] extends [U] ? X : Y`.
</details>

<details>
<summary>3. Why does <code>never extends never ? true : false</code> evaluate to <code>never</code>?</summary>

Because `never` represents an empty union. Distributive conditional types distribute over each member of the union. When there are zero members, the operation runs zero times, producing an empty union (`never`). Wrapping in tuples (`[T] extends [never]`) disables distribution and correctly returns `true`.
</details>

<details>
<summary>4. What happens when the same <code>infer R</code> variable appears in multiple function argument positions?</summary>

Function argument positions are contravariant. When multiple candidates are inferred for the same variable in contravariant positions, TypeScript merges them using an **intersection** (`A & B`).
</details>

<details>
<summary>5. Can <code>infer</code> be constrained directly in modern TypeScript?</summary>

Yes, starting in TypeScript 4.7, you can write `infer R extends Constraint`, such as `T extends Promise<infer R extends object> ? R : never`.
</details>

<details>
<summary>6. How does <code>ReturnType<T></code> work under the hood?</summary>

It uses a conditional type:
```typescript
type ReturnType<T extends (...args: any) => any> = T extends (...args: any) => infer R ? R : any;
```
The compiler matches the function signature and infers `R` as the return type.
</details>

<details>
<summary>7. How does TypeScript handle recursive conditional types?</summary>

Since TypeScript 4.1, conditional types can directly reference themselves recursively. TypeScript includes a recursion depth counter (defaulting to 1000 stack frames) to prevent infinite loops during compilation.
</details>

<details>
<summary>8. How can you convert a union of types into an intersection using conditional types and <code>infer</code>?</summary>

By distributing the union into function argument positions (contravariant) and inferring the argument:
```typescript
type UnionToIntersection<U> =
  (U extends any ? (k: U) => void : never) extends (k: infer I) => void ? I : never;
```
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Unwrap Promise Type (Easy)
Implement a utility `UnboxPromise<T>` that extracts `U` from `Promise<U>`, returning `T` if not a promise.
<details>
<summary>Solution</summary>

```typescript
type UnboxPromise<T> = T extends Promise<infer U> ? U : T;

type A = UnboxPromise<Promise<number>>; // number
type B = UnboxPromise<string>;          // string
```
</details>

### Exercise 2: Infer Array Element Type (Easy)
Implement `ArrayElement<T>` using `infer`.
<details>
<summary>Solution</summary>

```typescript
type ArrayElement<T> = T extends readonly (infer E)[] ? E : never;

type S = ArrayElement<string[]>; // string
type N = ArrayElement<readonly number[]>; // number
```
</details>

### Exercise 3: Last Element of Tuple (Medium)
Implement a utility `Last<T>` that returns the last element of a non-empty tuple.
<details>
<summary>Solution</summary>

```typescript
type Last<T extends readonly unknown[]> =
  T extends readonly [...infer _, infer L] ? L : never;

type L1 = Last<[1, 2, 3]>; // 3
type L2 = Last<["a", "b"]>; // "b"
```
</details>

### Exercise 4: Constructor Parameters Extractor (Medium)
Implement `MyConstructorParameters<T>` to extract the parameter tuple of a constructable class.
<details>
<summary>Solution</summary>

```typescript
type MyConstructorParameters<T extends abstract new (...args: any) => any> =
  T extends abstract new (...args: infer P) => any ? P : never;

class Server {
  constructor(port: number, host: string) {}
}

type ServerArgs = MyConstructorParameters<typeof Server>; // [port: number, host: string]
```
</details>

### Exercise 5: Union to Intersection Converter (Hard)
Implement `UnionToIntersection<U>` and explain its mechanics.
<details>
<summary>Solution</summary>

```typescript
type UnionToIntersection<U> =
  (U extends any ? (k: U) => void : never) extends (k: infer I) => void ? I : never;

type Res = UnionToIntersection<{ a: 1 } | { b: 2 }>;
// { a: 1 } & { b: 2 }
```
Mechanics: The first conditional distributes `U` across function argument positions: `((k: { a: 1 }) => void) | ((k: { b: 2 }) => void)`. The outer conditional tests assignability against `(k: infer I) => void`. Because function parameter positions are contravariant, candidate types are merged as an intersection!
</details>

### Exercise 6: Deep Awaited Utility (Hard)
Implement a fully recursive `DeepAwaited<T>` that recursively unwraps nested promises or thenable objects.
<details>
<summary>Solution</summary>

```typescript
type DeepAwaited<T> = T extends null | undefined
  ? T
  : T extends { then(onfulfilled: (arg: infer V) => any): any }
  ? DeepAwaited<V>
  : T;

type Nested = Promise<Promise<Promise<{ data: string }>>>;
type Clean = DeepAwaited<Nested>; // { data: string }
```
</details>
