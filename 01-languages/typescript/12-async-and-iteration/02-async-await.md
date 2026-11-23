# Async/Await and the Awaited Type in TypeScript

> Mastering `async`/`await` mechanics in TypeScript: return type inference, recursive `Awaited<T>` unwrapping, top-level `await`, exception flow, and transpilation targets.

| Metadata | Details |
| :--- | :--- |
| **Category** | Async & Iteration |
| **Difficulty** | Intermediate |
| **Prerequisites** | [12-async-and-iteration/01-promises.md](./01-promises.md) |
| **TypeScript Baseline** | TS 4.5+ (`Awaited<T>`), ES2022 (Top-Level Await) |
| **Tested tsconfig** | `"strict": true, "target": "ES2022", "module": "NodeNext"` |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-Common-mistakes--anti-patterns)
8. [Performance & Type-Checker Complexity](#8-performance--type-checker-complexity)
9. [Compiler Internals](#9-compiler-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why
The introduction of `async` and `await` transformed asynchronous JavaScript from callback and promise-chaining hierarchies into readable, linear code. In TypeScript, an `async` function always wraps its return value in a `Promise<T>`.

With TypeScript 4.5, the compiler introduced the `Awaited<T>` utility type, formally standardizing the recursive unwrapping rules of the `await` operator. Understanding how `Awaited<T>` resolves deeply nested promises, inter-operates with thenables, and propagates exceptions in `try/catch` statements is crucial for modern type-safe TypeScript development.

---

## 2. Core Concepts
- **`async` Function Return Types**: A function marked `async` always returns `Promise<Awaited<T>>`, even if the implementation explicitly returns a plain primitive.
- **The `Awaited<T>` Utility Type**: A recursive conditional type that models the runtime behavior of `await`:
  ```typescript
  type Awaited<T> = ... // Recursively extracts U from PromiseLike<U>
  ```
- **Top-Level `await`**: Supported in ES2022+ modules. Modules containing top-level `await` act as asynchronous modules, pausing dependent module execution until their promises resolve.
- **Error Propagation in `async/await`**: Thrown errors or rejected promises inside an `async` function become rejected promises returned by that function. Catching them requires `try/catch (err: unknown)`.
- **Concurrency Serialization Gotcha**: Inadvertently placing sequential `await` calls inside loops (`for (const item of list) { await process(item); }`) causes serial execution instead of concurrent execution.

---

## 3. How It Works Internally

```
+-------------------------------------------------------------+
|                     async function example()                |
+------------------------------+------------------------------+
                               |
                   Evaluates synchronously
                   until first 'await'
                               |
                               v
                       await promiseExpr;
                               |
              +----------------+----------------+
              |                                 |
              v                                 v
      Is it a Thenable?                Plain primitive / object?
      Extract value via microtask       Wraps immediately via
      resumed on next tick              Promise.resolve()
              |                                 |
              +----------------+----------------+
                               |
                               v
               Resumes synchronous execution frame
```

When downleveling to ES5/ES2015, the TypeScript compiler transforms `async/await` functions into state machines powered by generator functions (`__awaiter` helper) that yield promises to an internal runner. When targeting ES2017+, TypeScript emits native `async` and `await` keywords directly, allowing runtime V8 optimization.

---

## 4. Syntax & API Reference

```typescript
// 1. Return type inference
async function compute(): Promise<number> {
  return 42; // Wrapped into Promise<number> automatically
}

// 2. Using Awaited<T> utility
type Extracted = Awaited<Promise<Promise<string>>>; // string

// 3. Top-level await in ECMAScript modules (ES2022+)
const dbConfig = await loadConfigFromVault();

// 4. Typing thenable interop
interface CustomThenable<T> {
  then(onfulfilled: (value: T) => void): void;
}
type Unwrapped = Awaited<CustomThenable<boolean>>; // boolean
```

---

## 5. Practical Examples

### Example 1: Recursive Awaited Extraction
```typescript
type DeepPromise = Promise<Promise<Promise<{ token: string }>>>;
type TokenInfo = Awaited<DeepPromise>;
// TokenInfo is inferred as: { token: string }
```

### Example 2: Real-World Safe Async Transaction
```typescript
interface DatabaseClient {
  query(sql: string): Promise<unknown>;
  commit(): Promise<void>;
  rollback(): Promise<void>;
}

async function executeInTransaction<T>(
  db: DatabaseClient,
  action: () => Promise<T>
): Promise<T> {
  try {
    const result = await action();
    await db.commit();
    return result;
  } catch (error: unknown) {
    await db.rollback();
    throw new Error('Transaction failed and was rolled back', { cause: error });
  }
}
```

### Example 3: Edge Case - Awaiting Non-Promise Values
```typescript
// Awaiting a primitive is valid and returns the primitive immediately
async function unwrapAny<T>(value: T): Promise<Awaited<T>> {
  return await value;
}

const n = await unwrapAny(123); // number
const s = await unwrapAny(Promise.resolve('text')); // string
```

### Example 4: Compiler Failure Case - Illegal Await in Non-Async Scope
```typescript
function standardFunction() {
  const p = Promise.resolve(10);
  // @ts-expect-error TS1308: 'await' expressions are only allowed within async functions and at top levels of modules.
  const val = await p;
}
```

### Example 5: Idiomatic Concurrent Batch Processing
```typescript
export async function fetchUsersConcurrently(
  ids: readonly string[]
): Promise<ReadonlyArray<{ id: string; ok: boolean }>> {
  // Avoid serial loop: map to array of promises first!
  const promises = ids.map(async id => {
    const res = await fetch(`https://api.example.com/users/${id}`);
    return { id, ok: res.ok };
  });

  return await Promise.all(promises);
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **The Sequential Await Loop Trap**: Writing `for (const id of ids) { await fetch(id); }` runs requests strictly one after the other. Unless ordering is mandatory, map to promises and use `await Promise.all()`.
2. **Top-Level Await Deadlocks**: Circular module dependencies containing top-level `await` can deadlock the module loader, causing application initialization to hang indefinitely.
3. **Floating Promises**: Calling an async function without `await` or `.catch()` (e.g. `saveToAuditLog(event);`) leads to unhandled rejections if the background promise rejects. Use ESLint `@typescript-eslint/no-floating-promises`.
4. **`async` with No `await`**: An `async` function with no `await` still incurs microtask wrapping overhead and returns a Promise. Avoid `async` keywords on purely synchronous functions.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Consequence | Correction |
| :--- | :--- | :--- |
| `return await promise;` at end of function | Redundant microtask allocation | Use `return promise;` unless inside `try/catch` |
| Floating async call without await | Unhandled rejections escape error boundaries | `await fn()` or attach `.catch()` |
| `ids.forEach(async (id) => { await ... })` | `forEach` does not await promises; fires and forgets | Use `for ... of` or `Promise.all(ids.map(...))` |
| Marking synchronous functions `async` | Unnecessary Promise allocation and microtask delay | Return direct value without `async` |
| Using `Awaited<T>` on cyclic thenable | Compiler recursion error | Ensure thenable has non-circular resolution |

---

## 8. Performance & Type-Checker Complexity
- In V8, `return await promise` inside an `async` function was optimized in Node.js 12+ so that it preserves stack traces without allocating an extra microtask. However, `return promise;` outside of `try/catch` is still slightly faster.
- `Awaited<T>` in TypeScript 4.5+ uses a tail-recursive conditional type pattern. It evaluates in $\mathcal{O}(D)$ where $D$ is promise nesting depth (typically $D \le 2$).

---

## 9. Compiler Internals
In `lib.es5.d.ts`, `Awaited<T>` is defined as:
```typescript
type Awaited<T> =
  T extends null | undefined ? T :
  T extends object & { then(onfulfilled: infer F, ...args: infer _): any } ?
    F extends ((value: infer V, ...args: infer _) => any) ?
      Awaited<V> :
      never :
  T;
```
It extracts the parameter of `onfulfilled` from any object possessing a `.then()` method, recursing until `V` is no longer a thenable.

---

## 10. Best Practices & Design Guidelines
- Always enable `@typescript-eslint/no-floating-promises` in your linter.
- Only use `return await expr;` when inside a `try/catch` block where catching exceptions in the current scope is required.
- Leverage top-level `await` for database pool initialization or configuration loading in module root files.
- Prefer `Promise.all(items.map(...))` over sequential `await` loops for non-dependent I/O tasks.
- Keep `async` functions focused: do not mix heavy synchronous CPU loops with sparse `await` calls.

---

## 11. Real-World Case Study
A data ingestion pipeline processed batches of 500 documents. The original code used `array.forEach(async doc => await db.insert(doc))`. Tests passed, but in production, the function returned before any inserts finished, causing the process to terminate prematurely and drop 80% of database writes.

**Refactored with Controlled Batching:**
```typescript
export async function ingestDocuments<T>(
  docs: ReadonlyArray<T>,
  insertFn: (doc: T) => Promise<void>,
  batchSize = 25
): Promise<void> {
  for (let i = 0; i < docs.length; i += batchSize) {
    const chunk = docs.slice(i, i + batchSize);
    await Promise.all(chunk.map(doc => insertFn(doc)));
  }
}
```
All documents completed deterministically, batching controlled memory usage, and zero writes were lost.

---

## 12. Interview Questions

<details>
<summary>1. How does the `Awaited<T>` utility type work in TypeScript?</summary>

`Awaited<T>` recursively unwraps promises and "thenables" until a non-promise type is reached, exactly mirroring the runtime behavior of the JavaScript `await` keyword.
</details>

<details>
<summary>2. Why does `array.forEach(async item => { await process(item); })` not wait for tasks to finish?</summary>

The `forEach` method accepts a callback and discards its return value. When passed an `async` function, it receives a `Promise` for each element and ignores it, proceeding synchronously without awaiting completion.
</details>

<details>
<summary>3. When is `return await promise;` necessary?</summary>

Inside a `try/catch` block. If you write `return promise;` without `await`, the promise is returned directly in pending state, and any rejection will bypass the local `catch` block and be caught by the caller instead.
</details>

<details>
<summary>4. What happens when you `await` a primitive value like `await 42`?</summary>

The JavaScript engine wraps the primitive into a resolved Promise (`Promise.resolve(42)`) and pauses execution until the next microtask turn, returning `42`.
</details>

<details>
<summary>5. What are the requirements for using top-level `await` in TypeScript?</summary>

The project must target `module: "NodeNext"`, `"ES2022"`, or `"Preserve"`, with a target of `ES2022` or later. The file must be treated as an ES module (e.g. contains `import`/`export` or `.mts` extension).
</details>

<details>
<summary>6. How does TypeScript verify that an `async` function returns a `Promise`?</summary>

The return type of any function declared with `async` is checked by the compiler to ensure it is assignable to `Promise<Awaited<R>>` where `R` is the type of expressions in `return` statements.
</details>

<details>
<summary>7. What is a "thenable"?</summary>

Any object or function that has a callable `then` method accepting at least one callback argument. TypeScript treats thenables as promises during `await` unwrapping.
</details>

<details>
<summary>8. How can top-level `await` cause application deadlocks?</summary>

If Module A awaits Module B at the top level, and Module B directly or indirectly imports and awaits Module A, a circular dependency cycle is formed, causing the runtime module loader to hang indefinitely.
</details>

---

## 13. Exercises & Challenges

### Easy
1. **Extract Return Type**: Given `type ApiCall = () => Promise<Promise<{ id: string }>>`, define a type `ApiResponse` that extracts the underlying object using `Awaited`.
   <details>
   <summary>Solution</summary>

   ```typescript
   type ApiCall = () => Promise<Promise<{ id: string }>>;
   type ApiResponse = Awaited<ReturnType<ApiCall>>; // { id: string }
   ```
   </details>

2. **Async Identity**: Write an async function `asyncIdentity<T>(val: T): Promise<T>` that awaits `val` and returns it.
   <details>
   <summary>Solution</summary>

   ```typescript
   async function asyncIdentity<T>(val: T): Promise<Awaited<T>> {
     return await val;
   }
   ```
   </details>

### Medium
3. **Sequential Pipeline with Async/Await**: Implement `chainAsync<T>(initial: T, ...fns: Array<(arg: T) => Promise<T>>): Promise<T>` executing functions sequentially.
   <details>
   <summary>Solution</summary>

   ```typescript
   export async function chainAsync<T>(
     initial: T,
     ...fns: Array<(arg: T) => Promise<T>>
   ): Promise<T> {
     let current = initial;
     for (const fn of fns) {
       current = await fn(current);
     }
     return current;
   }
   ```
   </details>

4. **Async Filter Function**: Implement `filterAsync<T>(items: T[], predicate: (item: T) => Promise<boolean>): Promise<T[]>`.
   <details>
   <summary>Solution</summary>

   ```typescript
   export async function filterAsync<T>(
     items: T[],
     predicate: (item: T) => Promise<boolean>
   ): Promise<T[]> {
     const mask = await Promise.all(items.map(predicate));
     return items.filter((_, idx) => mask[idx]);
   }
   ```
   </details>

### Hard
5. **Async Retry with Cancellation & Timeout**: Implement `withAsyncRetry<T>(task: () => Promise<T>, options: { retries: number; timeoutMs: number }): Promise<T>` that combines timeouts and retries cleanly.
   <details>
   <summary>Solution</summary>

   ```typescript
   export async function withAsyncRetry<T>(
     task: () => Promise<T>,
     options: { retries: number; timeoutMs: number }
   ): Promise<T> {
     let lastError: unknown;

     for (let i = 0; i <= options.retries; i++) {
       try {
         let timerId: ReturnType<typeof setTimeout>;
         const timeoutPromise = new Promise<never>((_, reject) => {
           timerId = setTimeout(() => reject(new Error('Operation timed out')), options.timeoutMs);
         });

         const result = await Promise.race([task(), timeoutPromise]);
         clearTimeout(timerId!);
         return result;
       } catch (err: unknown) {
         lastError = err;
       }
     }

     throw new Error(`Failed after ${options.retries} retries`, { cause: lastError });
   }
   ```
   </details>

6. **Custom Recursive Awaited Re-implementation**: Re-create the `MyAwaited<T>` type from scratch using conditional types, handling nested thenables, null, and undefined without using the built-in `Awaited`.
   <details>
   <summary>Solution</summary>

   ```typescript
   type MyAwaited<T> =
     T extends null | undefined
       ? T
       : T extends { then(onfulfilled: (arg: infer U) => any): any }
         ? MyAwaited<U>
         : T;
   ```
   </details>
