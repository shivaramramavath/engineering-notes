# Awaited and Promise Unwrapping

> Introduced in TypeScript 4.5, `Awaited<T>` recursively unwraps Promises and thenable structures to model runtime `await` semantics accurately at compile time.

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [05-generic-inference.md](file:///E:/Learning/engineering-notes/01-languages/typescript/06-generics/05-generic-inference.md), [03-function-and-class-utilities.md](file:///E:/Learning/engineering-notes/01-languages/typescript/07-utility-types/03-function-and-class-utilities.md) |
| **Time to Read** | 22 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 4.5 / TS 5.x / TS 7.0 Native |
| **Tested tsconfig** | `strict: true`, `target: ES2022` |

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
In JavaScript, the `await` keyword unwraps Promises recursively until a non-thenable value is reached:
```javascript
const p = Promise.resolve(Promise.resolve("value"));
const res = await p; // "value"
```
Before TypeScript 4.5, custom promise unwrappers typically used shallow inference:
```typescript
type BadUnwrap<T> = T extends Promise<infer U> ? U : T;
// Fails on nested promises: BadUnwrap<Promise<Promise<string>>> => Promise<string>!
```
Moreover, JavaScript promises interoperate with custom "Thenable" objects (objects with a `.then()` method) and library promises (Bluebird, Axios).

TypeScript 4.5 introduced **`Awaited<T>`** directly into the standard library to mirror the exact recursive unwrapping semantics of ECMAScript `await` and `Promise.all`.

---

## 2. Core Concepts

### Full Recursive Unwrapping
`Awaited<T>` recursively unpacks inner types:
```typescript
type T1 = Awaited<Promise<string>>;                 // string
type T2 = Awaited<Promise<Promise<number>>>;         // number
type T3 = Awaited<Promise<Promise<Promise<boolean>>>>; // boolean
type T4 = Awaited<number>;                          // number (non-promises pass through)
```

### Custom Thenables
`Awaited<T>` handles non-`Promise` objects that adhere to the Promises/A+ Thenable interface:
```typescript
interface CustomThenable {
  then(onfulfilled: (value: { data: string }) => void): void;
}

type UnwrappedThenable = Awaited<CustomThenable>; // { data: string }
```

### Interleaved Nullish Unions
```typescript
type MaybePromise = Promise<string> | null | undefined;
type Unwrapped = Awaited<MaybePromise>; // string | null | undefined
```

---

## 3. How It Works Internally

```
lib.es5.d.ts Definition:

type Awaited<T> =
  T extends null | undefined ? T :
  T extends object & { then(onfulfilled: infer F, ...args: infer _): any } ?
    F extends ((value: infer V, ...args: infer _) => any) ?
      Awaited<V> :
      never :
  T;
```

Evaluation Steps:
1. If `T` is `null` or `undefined`, return `T` immediately.
2. If `T` is an object with a callable `then` method, extract the `onfulfilled` callback argument (`infer F`).
3. If `F` is a function taking an argument `value: infer V`, recursively evaluate `Awaited<V>`.
4. If `T` is not a thenable object, return `T`.

---

## 4. Syntax & API Reference

### Built-in Usage in Standard Library
`Awaited<T>` powers standard library asynchronous methods:
```typescript
// Promise.all signature:
// Promise.all<T extends readonly unknown[] | []>(values: T): Promise<{ -readonly [P in keyof T]: Awaited<T[P]> }>;
```

---

## 5. Practical Examples

### 5.1 Basic Async Unwrapping
```typescript
async function fetchAccount() {
  return { id: "acc_101", balance: 5000 };
}

type AccountData = Awaited<ReturnType<typeof fetchAccount>>;
// { id: string; balance: number }
```

### 5.2 Real-World Application: Strongly Typed Async Pipeline Aggregator
```typescript
async function runAsyncPipeline<T extends readonly Promise<any>[]>(
  promises: T
): Promise<{ [K in keyof T]: Awaited<T[K]> }> {
  return Promise.all(promises) as any;
}

const p1 = Promise.resolve(42);
const p2 = Promise.resolve("hello");
const p3 = Promise.resolve(true);

const results = await runAsyncPipeline([p1, p2, p3] as const);
// [number, string, boolean]
```

### 5.3 Edge Case: Cyclic Thenable Defense
```typescript
interface CyclicThenable {
  then(onfulfilled: (val: CyclicThenable) => void): void;
}

// TypeScript halts recursion on self-referential thenables and reports:
// TS2589: Type instantiation is excessively deep and possibly infinite.
// type Bad = Awaited<CyclicThenable>;
```

### 5.4 Failure Case: Expecting Non-Thenable to Unpack
```typescript
interface FakePromise {
  then: string; // Not a method!
}

type NonUnwrapped = Awaited<FakePromise>; // FakePromise (not unpacked because then is not callable)
```

### 5.5 Idiomatic Pattern: Generic Database Query Resolver
```typescript
interface DatabaseClient {
  query<T>(sql: string): Promise<T[]>;
}

type QueryResult<TClient extends DatabaseClient, TData> =
  Awaited<ReturnType<TClient["query"]>>;
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Self-Referential Promises**: An object whose `then` callback resolves to itself causes `TS2589`.
2. **`any` and `never`**:
   - `Awaited<any>` evaluates to `any`.
   - `Awaited<never>` evaluates to `never`.
3. **Union Types**: `Awaited<Promise<string> | Promise<number>>` distributes to `string | number`.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Problem | Solution | Compiler Error |
| :--- | :--- | :--- | :--- |
| Writing custom shallow `T extends Promise<infer U>` | Fails on nested promises and non-standard thenables | Use standard `Awaited<T>` | Leaked `Promise<T>` types |
| Passing function identifier instead of return type | `Awaited<fn>` does nothing because function is not a promise | Write `Awaited<ReturnType<typeof fn>>` | Unchanged function type |
| Forgetting `await` at runtime despite `Awaited` type | TypeScript type matches, but runtime is unresolved Promise | Ensure runtime code awaits promise | Runtime Object/Promise bugs |
| Intersecting thenables with primitives | Degrades into `never` | Clean types before unwrapping | Unexpected `never` |
| Handling `Promise.all` with mutable array | Infers `Array<Awaited<T>>` instead of strict tuple | Pass tuple with `as const` | Lost positional types |
| Unwrapping non-promise objects expecting deep traversal | `Awaited` only targets thenables | Use recursive mapped types for object traversal | Unchanged object |

---

## 8. Performance & Type-Checker Complexity
- Because `Awaited<T>` is recursive, resolving deeply nested promise structures uses the compiler's tail-call elimination.
- Standard unwrapping executes in $O(D)$ where $D$ is promise nesting depth (typically $D le 3$).

---

## 9. Compiler Internals
- `Awaited` is declared in `lib.es5.d.ts`.
- Evaluated in `src/compiler/checker.ts` through recursive conditional type unwrapping.
- The compiler specifically protects against circular thenable chains by tracking the active thenable resolution stack.

---

## 10. Best Practices & Design Guidelines
- Always use `Awaited<ReturnType<typeof asyncFunction>>` to derive response types from async service methods.
- Avoid authoring custom promise unwrap types; `Awaited<T>` is standard, thoroughly tested, and handles all edge cases.

---

## 11. Real-World Case Study
### Type-Safe Microservice Message Bus Handler
```typescript
type HandlerFn = (...args: any[]) => Promise<any> | any;

interface HandlersRegistry {
  "user.created": (event: { id: string }) => Promise<{ success: boolean }>;
  "order.placed": (event: { orderId: string }) => Promise<{ invoiceUrl: string }>;
  "system.ping": () => "pong";
}

type HandlerResponse<K extends keyof HandlersRegistry> =
  Awaited<ReturnType<HandlersRegistry[K]>>;

class MessageDispatcher {
  async dispatch<K extends keyof HandlersRegistry>(
    event: K,
    ...args: Parameters<HandlersRegistry[K]>
  ): Promise<HandlerResponse<K>> {
    console.log(`Dispatching ${event}`);
    return {} as HandlerResponse<K>;
  }
}

const bus = new MessageDispatcher();
const pingRes = await bus.dispatch("system.ping"); // type: "pong"
const userRes = await bus.dispatch("user.created", { id: "u1" }); // type: { success: boolean }
```

---

## 12. Interview Questions

<details>
<summary>1. What problem does <code>Awaited&lt;T&gt;</code> solve in TypeScript 4.5+?</summary>

Before TS 4.5, custom promise unwrappers like `T extends Promise<infer U> ? U : T` could not unwrap nested promises (`Promise<Promise<string>>`) or custom Thenable objects that deviate from standard ES Promises. `Awaited<T>` recursively unpacks thenables to match runtime `await` behavior.
</details>

<details>
<summary>2. How does <code>Awaited&lt;T&gt;</code> treat non-promise types?</summary>

Non-promise types (primitives, plain objects, functions) pass through unmodified: `Awaited<string>` is `string`.
</details>

<details>
<summary>3. What is a Thenable in TypeScript?</summary>

A thenable is any object that has a callable `then` method matching the signature:
```typescript
{ then(onfulfilled: (value: any) => any, ...args: any[]): any }
```
</details>

<details>
<summary>4. What happens when you apply <code>Awaited</code> to <code>Promise&lt;string&gt; | null</code>?</summary>

It distributes across the union: `Awaited<Promise<string>> | Awaited<null>`, yielding `string | null`.
</details>

<details>
<summary>5. How does <code>Promise.all</code> use <code>Awaited</code>?</summary>

In modern TypeScript standard library declarations, `Promise.all` is typed with a mapped tuple: `{ [P in keyof T]: Awaited<T[P]> }`. This ensures that every element of the input tuple is properly unwrapped.
</details>

<details>
<summary>6. How does <code>Awaited</code> prevent infinite recursion on circular thenables?</summary>

The TypeScript compiler tracks the recursion stack during `Awaited` evaluation. If a thenable returns itself as its resolved value, the compiler stops recursion and emits error `TS2589`.
</details>

<details>
<summary>7. What does <code>Awaited&lt;any&gt;</code> and <code>Awaited&lt;never&gt;</code> produce?</summary>

`Awaited<any>` produces `any`. `Awaited<never>` produces `never`.
</details>

<details>
<summary>8. How do you extract the resolved payload of an API client method?</summary>

```typescript
type ApiResponse = Awaited<ReturnType<typeof apiClient.getData>>;
```
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Extract Resolved Return (Easy)
Extract the resolved value of `async () => [1, 2, 3]`.
<details>
<summary>Solution</summary>

```typescript
const getNumbers = async () => [1, 2, 3];
type Result = Awaited<ReturnType<typeof getNumbers>>; // number[]
```
</details>

### Exercise 2: Nested Promise Unwrapping (Easy)
Verify that `Awaited` unwraps a triple-nested promise.
<details>
<summary>Solution</summary>

```typescript
type TriplyNested = Promise<Promise<Promise<{ status: string }>>>;
type Unwrapped = Awaited<TriplyNested>; // { status: string }
```
</details>

### Exercise 3: Typed Promise Combinator (Medium)
Write a typed wrapper around `Promise.race` that extracts the union of resolved values from a tuple.
<details>
<summary>Solution</summary>

```typescript
async function safeRace<T extends readonly unknown[]>(
  promises: T
): Promise<Awaited<T[number]>> {
  return Promise.race(promises) as Promise<Awaited<T[number]>>;
}

const raceRes = await safeRace([Promise.resolve("fast"), Promise.resolve(100)] as const);
// string | 100
```
</details>

### Exercise 4: Async Function Dict Unwrapper (Medium)
Given a dictionary of async functions, construct an interface with the resolved types.
<details>
<summary>Solution</summary>

```typescript
type AsyncMethods = {
  fetchUser: () => Promise<{ id: string }>;
  fetchConfig: () => Promise<{ port: number }>;
  ping: () => Promise<boolean>;
};

type ResolvedDict<T extends Record<string, (...args: any[]) => any>> = {
  [K in keyof T]: Awaited<ReturnType<T[K]>>;
};

type State = ResolvedDict<AsyncMethods>;
// { fetchUser: { id: string }; fetchConfig: { port: number }; ping: boolean }
```
</details>

### Exercise 5: Custom Thenable Implementation (Hard)
Implement a custom Thenable class and verify that `Awaited` correctly resolves its value.
<details>
<summary>Solution</summary>

```typescript
class CustomDeferred<T> {
  constructor(private value: T) {}

  then<TResult1 = T, TResult2 = never>(
    onfulfilled?: ((value: T) => TResult1 | PromiseLike<TResult1>) | null,
    onrejected?: ((reason: any) => TResult2 | PromiseLike<TResult2>) | null
  ): Promise<TResult1 | TResult2> {
    const res = onfulfilled ? onfulfilled(this.value) : (this.value as any);
    return Promise.resolve(res);
  }
}

type DefType = Awaited<CustomDeferred<{ code: number }>>;
// { code: number }
```
</details>

### Exercise 6: Deep Awaited Array or Object (Hard)
Implement a utility `DeepAwaitedStructure<T>` that recursively traverses an object and unwraps any Promise properties.
<details>
<summary>Solution</summary>

```typescript
type DeepAwaitedStructure<T> = T extends Promise<any>
  ? DeepAwaitedStructure<Awaited<T>>
  : T extends (...args: any[]) => any
  ? T
  : T extends Array<infer U>
  ? Array<DeepAwaitedStructure<U>>
  : T extends object
  ? { [K in keyof T]: DeepAwaitedStructure<T[K]> }
  : Awaited<T>;

interface NestedAsyncState {
  user: Promise<{
    profile: Promise<{ name: string }>;
    posts: Array<Promise<{ title: string }>>;
  }>;
  version: number;
}

type ResolvedState = DeepAwaitedStructure<NestedAsyncState>;
// {
//   user: {
//     profile: { name: string };
//     posts: Array<{ title: string }>;
//   };
//   version: number;
// }
```
</details>
