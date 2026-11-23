# Promises and Combinators in TypeScript

> Comprehensive guide to typing JavaScript Promises, executor functions, chaining mechanics, `Promise.all`, `Promise.allSettled`, `Promise.race`, `Promise.any`, and strict rejection handling in TypeScript.

| Metadata | Details |
| :--- | :--- |
| **Category** | Async & Iteration |
| **Difficulty** | Intermediate |
| **Prerequisites** | [01-fundamentals/00-types-and-inference.md](../01-fundamentals/00-types-and-inference.md), [06-generics/00-generic-functions.md](../06-generics/00-generic-functions.md) |
| **TypeScript Baseline** | TS 2.0+, TS 4.5+ (`Awaited<T>`) |
| **Tested tsconfig** | `"strict": true, "target": "ES2022", "module": "NodeNext"` |

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
The JavaScript `Promise` represents an eventual asynchronous completion or failure. In TypeScript, the generic interface `Promise<T>` tracks the type of the resolved value. However, JavaScript promises have a critical architectural asymmetry: while the resolution value `T` is statically tracked, the rejection error channel is *untyped* (`unknown` / `any`).

Understanding how TypeScript types promise creation, chaining through `.then()` and `.catch()`, and concurrent combinators (`Promise.all`, `Promise.allSettled`, `Promise.race`, `Promise.any`) is essential to prevent runtime bugs and dangling rejections.

---

## 2. Core Concepts
- **`Promise<T>` Type Parameter**: Represents the resolved value. Rejections are always untyped.
- **The Promise Constructor Executor**: `new Promise<T>((resolve, reject) => ...)`:
  - `resolve`: typed as `(value: T | PromiseLike<T>) => void`.
  - `reject`: typed as `(reason?: any) => void`.
- **Chaining Mechanics**: `.then(onfulfilled, onrejected)` automatically unwraps promises returned by callbacks (flattening `Promise<Promise<T>>` into `Promise<T>`).
- **Static Combinators**:
  - `Promise.all<T extends readonly unknown[]>`: Resolves a tuple `[A, B, C]` if all succeed; rejects fast on the first error.
  - `Promise.allSettled<T>`: Never rejects. Returns an array of `PromiseSettledResult<T>` (`PromiseFulfilledResult<T> | PromiseRejectedResult`).
  - `Promise.race<T>`: Resolves or rejects with the first promise that settles.
  - `Promise.any<T>`: Resolves with the first fulfilled value; rejects with an `AggregateError` only if all promises fail.

---

## 3. How It Works Internally

```
+-------------------------------------------------------------+
|                      new Promise<T>()                       |
|           State: PENDING -> FULFILLED or REJECTED           |
+------------------------------+------------------------------+
                               |
            +------------------+------------------+
            |                                     |
       resolve(val: T)                      reject(err: unknown)
            |                                     |
            v                                     v
     State: FULFILLED                      State: REJECTED
     Runs .then() queue                    Runs .catch() queue
            |                                     |
            +------------------+------------------+
                               |
                               v
               Promise Auto-Unwrapping Guarantee
       Promise<Promise<T>> is structurally Promise<T>
```

In the ECMAScript specification, promises cannot be nested. If you resolve a promise with another promise, the runtime unwinds it until a non-promise value is reached. TypeScript mirrors this behavior using the `Awaited<T>` conditional type.

---

## 4. Syntax & API Reference

```typescript
// 1. Explicit generic construction
const p: Promise<string> = new Promise((resolve) => {
  resolve('hello');
});

// 2. Promise.all with tuple inference
const tuplePromise: Promise<[number, string, boolean]> = Promise.all([
  Promise.resolve(10),
  Promise.resolve('text'),
  Promise.resolve(true),
]);

// 3. Promise.allSettled type structure
const settled = await Promise.allSettled([Promise.resolve(1), Promise.reject('err')]);
for (const item of settled) {
  if (item.status === 'fulfilled') {
    console.log(item.value); // number
  } else {
    console.error(item.reason); // any
  }
}

// 4. Promise.any with AggregateError (ES2021+)
try {
  const winner = await Promise.any([fetchPrimary(), fetchBackup()]);
} catch (e: unknown) {
  if (e instanceof AggregateError) {
    console.error(`All ${e.errors.length} sources failed.`);
  }
}
```

---

## 5. Practical Examples

### Example 1: Basic Promise Construction and Typing
```typescript
function delayedGreeting(name: string, delayMs: number): Promise<string> {
  return new Promise((resolve, reject) => {
    if (!name.trim()) {
      reject(new Error('Name cannot be empty'));
      return;
    }
    setTimeout(() => {
      resolve(`Hello, ${name}!`);
    }, delayMs);
  });
}
```

### Example 2: Real-World Safe Parallel Ingestion with `Promise.allSettled`
```typescript
interface UserProfile { id: string; name: string }

async function fetchMultipleUsers(
  userIds: string[]
): Promise<{ successful: UserProfile[]; failedIds: string[] }> {
  const tasks = userIds.map(async id => {
    const res = await fetch(`https://api.example.com/users/${id}`);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    return (await res.json()) as UserProfile;
  });

  const results = await Promise.allSettled(tasks);
  const successful: UserProfile[] = [];
  const failedIds: string[] = [];

  results.forEach((result, idx) => {
    if (result.status === 'fulfilled') {
      successful.push(result.value);
    } else {
      failedIds.push(userIds[idx]);
    }
  });

  return { successful, failedIds };
}
```

### Example 3: Edge Case - Flattening Nested Promises
```typescript
// Promise resolution unwraps automatically:
const nested: Promise<Promise<number>> = Promise.resolve(Promise.resolve(42));
// TypeScript statically collapses this to Promise<number> when chained:
const flattened: Promise<number> = nested.then(val => val);
```

### Example 4: Compiler Failure Case - Untyped Rejection Channel
```typescript
// In TypeScript, catch callback arguments are always 'any'
function runOperation(): Promise<number> {
  return Promise.resolve(10);
}

runOperation().catch((err) => {
  // @ts-expect-error TS18046: 'err' is of type 'unknown' when strict catch types are simulated
  // Or note that catch() parameter defaults to any in standard lib:
  console.log(err.message); // Dangerous at runtime if string was rejected!
});
```

### Example 5: Idiomatic Timeout Race Helper
```typescript
export function withTimeout<T>(promise: Promise<T>, timeoutMs: number): Promise<T> {
  let timerId: ReturnType<typeof setTimeout>;

  const timeoutPromise = new Promise<never>((_, reject) => {
    timerId = setTimeout(() => {
      reject(new Error(`Operation timed out after ${timeoutMs}ms`));
    }, timeoutMs);
  });

  return Promise.race([promise, timeoutPromise]).finally(() => {
    clearTimeout(timerId);
  });
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **`Promise.all` Fast-Failure Risk**: If one promise fails in `Promise.all`, the outer promise immediately rejects, but the remaining sister promises *continue executing in the background*. Resources are still consumed unless cancelled via `AbortSignal`.
2. **Rejection Reason is `any`**: TypeScript's standard library declares `catch(onrejected?: (reason: any) => ...)` with `any`. Do not assume `reason` is an instance of `Error`; always narrow defensively.
3. **Empty `Promise.all([])`**: Resolves synchronously to `[]` on the microtask turn.
4. **Empty `Promise.any([])`**: Rejects immediately with `AggregateError: All promises were rejected`.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Consequence | Correction |
| :--- | :--- | :--- |
| `new Promise(async (resolve) => ...)` | Unhandled exceptions inside executor crash silently | Keep executors synchronous; wrap with `.catch()` |
| `Promise.all` without error isolation | One failure aborts all other results | Use `Promise.allSettled` when partial success is valid |
| Assuming `reason` in `.catch` is `Error` | Throws secondary `TypeError` on string rejection | Narrow with `if (err instanceof Error)` |
| Leaving dangling promises without `.catch` | Node process crash on `unhandledRejection` | Always chain `.catch()` or await inside try/catch |
| Forgetting `return` in `.then()` chains | Breaks chaining; subsequent `.then()` receives `undefined` | Always return a value or promise |

---

## 8. Performance & Type-Checker Complexity
- `Promise.all` maintains $\mathcal{O}(N)$ tuple types in TypeScript. In TypeScript 4.0+, variadic tuple types allow `Promise.all` to accurately preserve heterogeneous tuples (`[Promise<string>, Promise<number>]` $\to$ `Promise<[string, number]>`).
- In V8, native promises are implemented directly in TurboFan with microtask queue optimizations. Creating a resolved promise via `Promise.resolve(v)` allocates a single internal heap object without scheduling delays.

---

## 9. Compiler Internals
In TypeScript's `lib.es2015.promise.d.ts`:
```typescript
interface PromiseConstructor {
  all<T extends readonly unknown[] | []>(values: T): Promise<{ -readonly [P in keyof T]: Awaited<T[P]> }>;
}
```
The mapped type `{ -readonly [P in keyof T]: Awaited<T[P]> }` maps each element of the input tuple through the recursive `Awaited<T>` type, peeling off promise layers.

---

## 10. Best Practices & Design Guidelines
- Use `Promise.allSettled` whenever tasks are independent and partial failures can be tolerated.
- Combine `Promise.race` with `clearTimeout` in a `.finally()` block to prevent timer resource leaks.
- Avoid the `new Promise(async ...)` anti-pattern. If you must use async logic, write an `async` function directly.
- Standardize on `Awaited<T>` when extracting resolution types from generic promises.
- In Node.js, always listen to `process.on('unhandledRejection', ...)` at the application root.

---

## 11. Real-World Case Study
A media aggregation portal fetched data from 5 external partner APIs. Using `Promise.all`, whenever a single partner experienced high latency or a 503 error, the entire user page failed with an error banner.

**Refactored Resilient Aggregator:**
```typescript
interface SourceData { provider: string; articles: string[] }

export async function fetchFeed(providers: string[]): Promise<SourceData[]> {
  const requests = providers.map(async (provider): Promise<SourceData> => {
    const res = await fetch(`https://api.${provider}.com/news`);
    if (!res.ok) throw new Error(`${provider} returned HTTP ${res.status}`);
    const articles = (await res.json()) as string[];
    return { provider, articles };
  });

  const settled = await Promise.allSettled(requests);

  return settled
    .filter((r): r is PromiseFulfilledResult<SourceData> => r.status === 'fulfilled')
    .map(r => r.value);
}
```
Site uptime jumped from 94% to 99.99%, rendering available feeds while gracefully ignoring failing third-party partners.

---

## 12. Interview Questions

<details>
<summary>1. How does `Promise.allSettled` differ from `Promise.all`?</summary>

`Promise.all` rejects immediately when the first promise rejects, discarding the results of the other promises. `Promise.allSettled` waits for all promises to either fulfill or reject, returning an array of objects describing each outcome without ever rejecting.
</details>

<details>
<summary>2. Why does `Promise<T>` not have an error type parameter like `Promise<T, E>` in TypeScript?</summary>

Because JavaScript promises natively reject with arbitrary values across asynchronous boundaries, and catching an error in `.then(..., onrejected)` can itself return a new resolved promise or throw another error. Static analysis cannot reliably guarantee what error will reach a catch handler.
</details>

<details>
<summary>3. What does `Promise.any` do and when does it reject?</summary>

`Promise.any` accepts an iterable of promises and returns a promise that fulfills as soon as any of the promises fulfills. It only rejects if *all* input promises reject, producing an `AggregateError` containing all rejection reasons.
</details>

<details>
<summary>4. What is the danger of passing an `async` executor to `new Promise(async (resolve, reject) => ...)`?</summary>

If an exception is thrown inside the `async` executor before `resolve` or `reject` is invoked, it results in an unhandled promise rejection inside the executor that cannot be caught by the outer promise's `.catch()` method.
</details>

<details>
<summary>5. How does TypeScript unpack nested promises in `Promise.all`?</summary>

Using the recursive conditional type `Awaited<T>`, which repeatedly unwraps any type implementing `.then(...)` until the base non-promise type is reached.
</details>

<details>
<summary>6. What does `Promise.race` return if passed an empty array `[]`?</summary>

It returns a promise that remains forever in the `pending` state, because there are no promises to settle first.
</details>

<details>
<summary>7. How can you clean up resources when using `Promise.race` with a timeout?</summary>

Use `.finally()` on the combined race promise to clear the underlying `setTimeout` timer, ensuring the timer handle does not keep the event loop alive.
</details>

<details>
<summary>8. What is the difference between `PromiseLike<T>` and `Promise<T>`?</summary>

`Promise<T>` is the complete ES6 Promise class including `.catch()`, `.finally()`, and symbol species. `PromiseLike<T>` is a minimal "thenable" interface requiring only a `.then()` method, allowing interop with third-party promise libraries (e.g. Bluebird, Q).
</details>

---

## 13. Exercises & Challenges

### Easy
1. **Deferred Promise Factory**: Implement a `createDeferred<T>()` utility that returns `{ promise: Promise<T>, resolve: (v: T) => void, reject: (e: unknown) => void }`.
   <details>
   <summary>Solution</summary>

   ```typescript
   export interface Deferred<T> {
     promise: Promise<T>;
     resolve: (value: T) => void;
     reject: (reason: unknown) => void;
   }

   export function createDeferred<T>(): Deferred<T> {
     let resolve!: (value: T) => void;
     let reject!: (reason: unknown) => void;
     const promise = new Promise<T>((res, rej) => {
       resolve = res;
       reject = rej;
     });
     return { promise, resolve, reject };
   }
   ```
   </details>

2. **Wait for Success**: Implement `firstSuccess<T>(promises: Promise<T>[]): Promise<T>` using `Promise.any`.
   <details>
   <summary>Solution</summary>

   ```typescript
   export function firstSuccess<T>(promises: Promise<T>[]): Promise<T> {
     return Promise.any(promises);
   }
   ```
   </details>

### Medium
3. **Settled Partition Helper**: Implement a generic function `partitionSettled<T>(results: PromiseSettledResult<T>[]): { values: T[]; errors: unknown[] }`.
   <details>
   <summary>Solution</summary>

   ```typescript
   export function partitionSettled<T>(results: PromiseSettledResult<T>[]): { values: T[]; errors: unknown[] } {
     const values: T[] = [];
     const errors: unknown[] = [];
     for (const r of results) {
       if (r.status === 'fulfilled') values.push(r.value);
       else errors.push(r.reason);
     }
     return { values, errors };
   }
   ```
   </details>

4. **Sequential Promise Reducer**: Implement `reduceAsync<T, R>(items: T[], reducer: (acc: R, item: T) => Promise<R>, initial: R): Promise<R>` that processes items one at a time sequentially.
   <details>
   <summary>Solution</summary>

   ```typescript
   export async function reduceAsync<T, R>(
     items: T[],
     reducer: (acc: R, item: T) => Promise<R>,
     initial: R
   ): Promise<R> {
     let current = initial;
     for (const item of items) {
       current = await reducer(current, item);
     }
     return current;
   }
   ```
   </details>

### Hard
5. **Typed Promise Pool with Concurrency Limit**: Write a class `PromisePool<T, R>` that takes tasks returning `Promise<R>` and executes them with a strict maximum concurrency limit of $N$ simultaneous in-flight operations, preserving task completion order.
   <details>
   <summary>Solution</summary>

   ```typescript
   export class PromisePool<T, R> {
     constructor(private readonly limit: number) {}

     async runAll(items: T[], worker: (item: T) => Promise<R>): Promise<R[]> {
       const results: R[] = new Array(items.length);
       let cursor = 0;

       const executeNext = async (): Promise<void> => {
         while (cursor < items.length) {
           const index = cursor++;
           results[index] = await worker(items[index]);
         }
       };

       const workers = Array.from({ length: Math.min(this.limit, items.length) }, () => executeNext());
       await Promise.all(workers);
       return results;
     }
   }
   ```
   </details>

6. **Retryable Promise with Jitter**: Author a function `retryWithJitter<T>(fn: () => Promise<T>, maxRetries: number, baseDelayMs: number): Promise<T>` that catches rejections, adds full jitter to backoff delay, and preserves the causal stack of the final rejection.
   <details>
   <summary>Solution</summary>

   ```typescript
   export async function retryWithJitter<T>(
     fn: () => Promise<T>,
     maxRetries: number,
     baseDelayMs: number
   ): Promise<T> {
     let lastError: unknown;

     for (let attempt = 0; attempt <= maxRetries; attempt++) {
       try {
         return await fn();
       } catch (err: unknown) {
         lastError = err;
         if (attempt < maxRetries) {
           const maxBackoff = baseDelayMs * Math.pow(2, attempt);
           const delay = Math.random() * maxBackoff;
           await new Promise(res => setTimeout(res, delay));
         }
       }
     }

     throw new Error(`Exhausted ${maxRetries} retries`, { cause: lastError });
   }
   ```
   </details>
