# Callbacks

> Callbacks define higher-order function contracts, governing asynchronous event notifications, contextual typing propagation, and error-first conventions with compile-time type safety.

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [00-function-types.md](file:///E:/Learning/engineering-notes/01-languages/typescript/02-functions/00-function-types.md), [07-never-and-void.md](file:///E:/Learning/engineering-notes/01-languages/typescript/01-fundamentals/07-never-and-void.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 0.9 / TS 5.x / TS 7.0 Native |
| **Tested tsconfig** | `strict: true`, `strictFunctionTypes: true`, `noImplicitAny: true` |

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
- **Problem Statement:** In JavaScript, callbacks drive asynchronous I/O, event dispatchers, and collection transformations. Untyped callbacks introduce subtle bugs: calling callbacks multiple times, passing arguments in the wrong order, or silently failing to handle error arguments.
- **Historical Context:** Node.js popularized the error-first callback convention (`(err, data) => void`). Modern TypeScript structures these patterns into strongly typed generic contracts, ensuring that errors are handled and payloads are validated before use.
- **Real-World Relevance:** Even in Promise-based modern codebases, callbacks are ubiquitous: `Array.prototype.map`, event listeners, retry utilities, stream processing, and reactive subscriptions depend on typed callbacks.

---

## 2. Core Concepts
- **Formal Invariants:**
  - Return Type of `void`: A callback typed as `() => void` allows the callee to return any value, which will be safely ignored by the caller.
  - Return Type of `undefined`: A callback typed as `() => undefined` strictly forbids returning any value other than `undefined`.
  - Contextual Typing: Callback parameter types are inferred top-down from the receiving function's type declaration.
  - Optional Callback Parameters: A callback implementation may choose to ignore parameters declared by the higher-order function signature.
- **Mental Model:** A callback is an inversion-of-control contract. The caller defines what information will be supplied and when; the implementer provides the execution logic.
- **Soundness vs Ergonomics:** Allowing a callback returning a number to satisfy a `void` callback contract is an intentional soundness compromise designed to support JavaScript side-effect methods like `array.push()`.

---

## 3. How It Works Internally
- **Contextual Signature Resolution:** When passing an anonymous function as an argument, the type checker matches the function literal against the parameter's expected callback signature before type-checking the body.
- **Bivariance vs Contravariance:** With `strictFunctionTypes: true`, callback types are checked contravariantly, preventing handlers from expecting narrower argument types than the caller provides.
- **Architectural Flow Diagram:**

```
+--------------------------------------------------------------------------+
|                  Contextual Typing Flow in Callbacks                     |
|                                                                          |
|   Declaration: function map<T, U>(arr: T[], fn: (item: T) => U): U[]    |
|                                    │                                     |
|   Call Site:   map([1, 2, 3], (x) => x * 2)                              |
|                               │                                          |
|                               ▼                                          |
|   [ Type Checker Traversal ]                                             |
|   ├── arr is number[] ──► T is inferred as number                        |
|   ├── fn expected type is (item: number) => U                            |
|   ├── Contextual type of 'x' is pushed as 'number'                       |
|   └── Body evaluates (number * number) ──► U is inferred as number       |
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & API Reference
- **Synchronous Callback:**
  ```typescript
  type Predicate<T> = (item: T) => boolean;
  type Consumer<T> = (item: T) => void;
  type Transformer<T, R> = (item: T) => R;
  ```
- **Node-Style Error-First Callback:**
  ```typescript
  type NodeCallback<T> = (err: Error | null, result?: T) => void;
  ```
- **tsconfig Dependencies:**
  - `strictFunctionTypes: true`: Crucial for catching argument type incompatibilities.

---

## 5. Practical Examples

### Example 1: Basic / Foundational Usage
- **Goal:** Author a typed custom `filter` function with contextual typing.
- **Code:**
```typescript
export function customFilter<T>(
  items: readonly T[],
  predicate: (item: T, index: number) => boolean
): T[] {
  const result: T[] = [];
  for (let i = 0; i < items.length; i++) {
    if (predicate(items[i]!, i)) {
      result.push(items[i]!);
    }
  }
  return result;
}

// Contextual typing: 'item' is inferred as string!
const names = ['Alice', 'Bob', 'Alexander'];
const longNames = customFilter(names, (item) => item.length > 4);
console.log(longNames); // ["Alice", "Alexander"]
```
- **Output / Type Evaluation:**
```typescript
// longNames is typed as string[]; predicate parameters inferred automatically.
```
- **Explanation:** Generic parameters connect the array element type to the callback's input argument seamlessly.

### Example 2: Real-World Enterprise Pattern (Error-First Callback to Promise Promisify)
- **Goal:** Implement a type-safe `promisify` utility transforming Node-style callbacks into Promises.
- **Code:**
```typescript
type Callback<T> = (err: Error | null, result?: T) => void;

export function promisify<T>(
  fn: (cb: Callback<T>) => void
): () => Promise<T> {
  return () =>
    new Promise<T>((resolve, reject) => {
      fn((err, result) => {
        if (err !== null) {
          reject(err);
        } else {
          resolve(result as T);
        }
      });
    });
}

// Legacy async API simulation
function readConfigAsync(cb: Callback<{ host: string; port: number }>): void {
  setTimeout(() => cb(null, { host: 'localhost', port: 5432 }), 10);
}

const readConfigPromise = promisify(readConfigAsync);
readConfigPromise().then((cfg) => console.log('Host:', cfg.host));
```
- **Output / Type Evaluation:**
```typescript
// Preserves strong return typing on the generated Promise!
```
- **Explanation:** Generics extract the payload type from the callback's second parameter, ensuring type safety after promisification.

### Example 3: Edge Case / Boundary Condition (Void Return Type Substitutability)
- **Goal:** Demonstrate why callbacks returning values are assignable to `void` callbacks.
- **Code:**
```typescript
type LogCallback = (msg: string) => void;

function runLogging(cb: LogCallback): void {
  cb('Log entry');
}

const buffer: string[] = [];

// Array.prototype.push returns number (the new length)
// But it is legally assigned to a void callback!
runLogging((msg) => buffer.push(msg));

// However, if typed with undefined, it is REJECTED:
type StrictUndefinedCallback = (msg: string) => undefined;

// @ts-expect-error TS2322: Type 'number' is not assignable to type 'undefined'.
const strictFn: StrictUndefinedCallback = (msg) => buffer.push(msg);
```
- **Output / Type Evaluation:**
```typescript
// TS2322 catches unintended value returns when () => undefined is requested.
```
- **Explanation:** `void` means "caller will ignore return value", while `undefined` means "callee must not return anything".

### Example 4: Negative Test / Type-Check Failure Case
- **Goal:** Demonstrate error when callback parameter types are incompatible.
- **Code:**
```typescript
function forEachNumber(
  numbers: number[],
  action: (val: number) => void
): void {
  for (const n of numbers) action(n);
}

// @ts-expect-error TS2345: Argument of type '(val: string) => void' is not assignable to parameter of type '(val: number) => void'.
forEachNumber([1, 2, 3], (val: string) => {
  console.log(val.toUpperCase());
});
```
- **Real Compiler Error:**
```
error TS2345: Argument of type '(val: string) => void' is not assignable to parameter of type '(val: number) => void'.
  Types of parameters 'val' and 'val' are incompatible.
    Type 'number' is not assignable to type 'string'.
```
- **Explanation:** The callback cannot demand a `string` when the caller provides a `number`.

### Example 5: Idiomatic Modern Usage (Type Guard Callbacks)
- **Goal:** Implement a filter function that narrows array elements using a type guard callback.
- **Code:**
```typescript
export function partition<T, S extends T>(
  items: readonly T[],
  guard: (item: T) => item is S
): { pass: S[]; fail: Exclude<T, S>[] } {
  const pass: S[] = [];
  const fail: Exclude<T, S>[] = [];

  for (const item of items) {
    if (guard(item)) {
      pass.push(item);
    } else {
      fail.push(item as Exclude<T, S>);
    }
  }

  return { pass, fail };
}

const rawList: (string | number)[] = ['apple', 42, 'banana', 100];
const partitioned = partition(rawList, (x): x is string => typeof x === 'string');

// partitioned.pass is typed as string[]
// partitioned.fail is typed as number[]
```
- **Output / Type Evaluation:**
```typescript
// pass: string[], fail: number[]
```
- **Explanation:** Type predicate callbacks (`item is S`) enable higher-order functions to split unions cleanly without manual casting.

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Async Callbacks in `Array.prototype.forEach`:** `forEach` does NOT await promises returned from callbacks. Passing `async (item) => await save(item)` fires requests concurrently without awaiting completion. Use `for...of` or `Promise.all(arr.map(...))`.
2. **Ignored Callback Parameters:** Callbacks are permitted to accept fewer parameters than declared by the caller. Never assume a callback will inspect your index or array arguments.
3. **Double Invocations:** In JavaScript, callbacks can be invoked multiple times by faulty async libraries. Use wrapper guards like `once()` to guarantee single-fire execution.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern (Naive / Broken) | Real Compiler Error / Bug | Idiomatic Solution | Why It Matters |
| :--- | :--- | :--- | :--- |
| Passing async callback to `forEach` | Silent unhandled rejections, race conditions | Use `for (const item of arr)` | Guarantees sequential asynchronous awaiting |
| Typing callback as `() => undefined` | Rejects standard library functions like `push` | Type as `() => void` | Preserves idiomatic JavaScript substitutability |
| Omitting error check in error-first callback | `TypeError: Cannot read properties of undefined` | Check `if (err != null)` first | Prevents crashing on failed async operations |
| Calling callback multiple times | Non-deterministic state updates | Wrap with `once(cb)` guard | Guarantees idempotent callback execution |
| Passing broad handler without contravariance | Runtime method missing crashes | Enable `strictFunctionTypes: true` | Catches parameter mismatches at compile time |
| Mutating array inside collection callback | Skips elements or infinite loops | Treat array as `readonly` | Prevents collection iteration bugs |

---

## 8. Performance & Type-Checker Complexity
- **Contextual Type Cache:** The compiler memoizes contextual signatures for callbacks, minimizing re-checking time across complex chains (`.map().filter().reduce()`).
- **Lambda Allocations:** Every callback declaration creates a closure allocation at runtime. In hot loops, prefer hoisted named functions over anonymous inline closures.
- **Isolated Declarations:** Exported functions taking callbacks must explicitly annotate the callback's parameter types.

---

## 9. Compiler Internals
- **Type Checking Traversal:** When checking a call expression, `checkCallExpression` checks arguments against parameter signatures. For callbacks, it invokes `getContextualType` to push expected parameter types into the arrow function AST node.
- **Predicate Checking:** If a callback return type is an `is` predicate (`item is S`), the checker records a `TypePredicate` structure on the signature.

---

## 10. Best Practices & Design Guidelines
- **Use `void` for Action Callback Returns:** Annotate listener and action callbacks with `void` to give callers maximum ergonomic flexibility.
- **Leverage Type Predicates for Filter Callbacks:** Use `(x: T) => x is S` when filtering collections to retain refined types.
- **Prefer Promises for Asynchronous APIs:** Modern codebases should expose Promises/async functions, reserving raw callbacks for high-frequency synchronous iteration and streaming.

---

## 11. Real-World Case Study
- **Scenario:** A file processing pipeline used `fs.readdir(path, (err, files) => { files.forEach(...) })`. The callback omitted the `if (err)` check. When a permissions error occurred on a directory, `files` was `undefined`, crashing the entire server process with `TypeError: Cannot read properties of undefined (reading 'forEach')`.
- **Implementation:** Created a strongly typed wrapper:
  ```typescript
  function safeAsyncRead(
    path: string,
    callback: (result: { ok: true; files: string[] } | { ok: false; error: Error }) => void
  ): void {
    fs.readdir(path, (err, files) => {
      if (err || !files) {
        callback({ ok: false, error: err ?? new Error('Unknown error') });
      } else {
        callback({ ok: true, files });
      }
    });
  }
  ```
- **Outcome:** The discriminated union forced all consuming handlers to check `if (res.ok)` before accessing `res.files`, eliminating 100% of unhandled directory read crashes.

---

## 12. Interview Questions

### Q1: Why is `() => void` different from `() => undefined` when typing callbacks?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

- `() => void` indicates that the caller will ignore any returned value. The callback implementation is permitted to return any value (such as a number, string, or boolean), and TypeScript will discard it safely.
- `() => undefined` strictly requires the callback implementation to return `undefined` (or execute `return;`). If an implementation returns any value (e.g. `() => list.push(x)`), the compiler will emit a type error.
</details>

### Q2: Why does `[1, 2, 3].forEach(async (n) => { await delay(100); })` not wait for promises?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

`Array.prototype.forEach` expects a callback returning `void`. When an `async` function is passed, it returns a `Promise<void>`. Because `Promise<void>` satisfies `void` via callback substitutability, TypeScript compiles without error. However, JavaScript's `forEach` implementation simply invokes the callback synchronously and discards the returned Promise, never awaiting it. To wait for asynchronous iterations, use `for (const n of [1, 2, 3])` or `Promise.all(arr.map(...))`.
</details>

### Q3: How does contextual typing infer callback parameters without manual annotations?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

When the TypeScript compiler evaluates a function argument, it checks if the corresponding parameter in the callee's signature has a function type. If so, that function type serves as the "contextual type". The compiler propagates the contextual parameter types downward into the argument function's AST node before type-checking the body, eliminating the need for explicit type annotations on lambda parameters.
</details>

### Q4: Can a callback declare fewer parameters than the higher-order function passes?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Yes. TypeScript intentionally allows callbacks to omit parameters. For example, `Array.prototype.map` passes three arguments: `(value, index, array)`. Callbacks that only care about `value` can write `x => x * 2`. Requiring exact parameter count parity would force developers to write `(value, _index, _array) => value * 2`, which would severely harm ergonomics without providing any safety benefit.
</details>

### Q5: How do type predicate callbacks work in higher-order functions?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

A type predicate callback returns `item is Subtype` instead of plain `boolean`. When a higher-order function like `Array.prototype.filter` is overloaded with a signature accepting `(val: T) => val is S`, TypeScript refines the return type of the collection from `T[]` to `S[]`, enabling type narrowing across collection pipelines.
</details>

### Q6: What is the Node.js error-first callback pattern and how is it typed?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

The error-first convention mandates that the first parameter represents an optional error object (`Error | null`), and subsequent parameters represent successful results (`result?: T`). In TypeScript, it is typed as `(err: Error | null, result?: T) => void` or as a discriminated tuple in modern architectures.
</details>

### Q7: Why should callbacks used for event listeners use property syntax instead of method syntax in interfaces?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Method syntax in interfaces (`addListener(cb: (e: Event) => void): void`) uses bivariant parameter checking for legacy reasons. Function property syntax (`addListener: (cb: (e: Event) => void) => void`) enforces strict contravariance under `strictFunctionTypes: true`, preventing callers from passing callbacks that expect incompatible event subtypes.
</details>

### Q8: How can you write a utility function that ensures a callback is only executed once?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

```typescript
function once<Args extends any[], R>(fn: (...args: Args) => R): (...args: Args) => R | undefined {
  let executed = false;
  let result: R;
  return (...args: Args): R | undefined => {
    if (!executed) {
      executed = true;
      result = fn(...args);
      return result;
    }
    return result;
  };
}
```
This captures parameter and return types with generics while enforcing single-fire execution.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Typed Map Implementation (Easy)
- **Problem Statement:** Author a generic function `customMap<T, U>(arr: readonly T[], fn: (item: T, idx: number) => U): U[]` that applies a callback to each item.
- **Test Case:**
```typescript
const lengths = customMap(['apple', 'pear'], (s) => s.length); // [5, 4]
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function customMap<T, U>(
  arr: readonly T[],
  fn: (item: T, idx: number) => U
): U[] {
  const result: U[] = [];
  for (let i = 0; i < arr.length; i++) {
    result.push(fn(arr[i]!, i));
  }
  return result;
}
```
</details>

### Exercise 2: Retry with Backoff Callback (Easy)
- **Problem Statement:** Write a function `retry<T>(task: () => Promise<T>, retries: number): Promise<T>` that executes an async task callback up to `retries` times.
- **Test Case:**
```typescript
let count = 0;
const res = await retry(async () => {
  count++;
  if (count < 2) throw new Error('fail');
  return 'success';
}, 3); // "success"
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
async function retry<T>(task: () => Promise<T>, retries: number): Promise<T> {
  let lastError: unknown;
  for (let i = 0; i <= retries; i++) {
    try {
      return await task();
    } catch (err) {
      lastError = err;
    }
  }
  throw lastError;
}
```
</details>

### Exercise 3: Filter and Map in Single Pass (Medium)
- **Problem Statement:** Write `filterMap<T, U>(arr: readonly T[], fn: (item: T) => U | undefined): U[]` that applies a callback and discards `undefined` results.
- **Test Case:**
```typescript
const nums = [1, 2, 3, 4];
const evensDoubled = filterMap(nums, (n) => (n % 2 === 0 ? n * 2 : undefined)); // [4, 8]
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function filterMap<T, U>(
  arr: readonly T[],
  fn: (item: T) => U | undefined
): U[] {
  const result: U[] = [];
  for (const item of arr) {
    const mapped = fn(item);
    if (mapped !== undefined) {
      result.push(mapped);
    }
  }
  return result;
}
```
</details>

### Exercise 4: Async Callback Queue (Medium)
- **Problem Statement:** Implement a FIFO queue that accepts async task callbacks and processes them one at a time.
- **Test Case:**
```typescript
const queue = new AsyncQueue();
queue.enqueue(async () => { /* task 1 */ });
queue.enqueue(async () => { /* task 2 */ });
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
class AsyncQueue {
  private queue: (() => Promise<void>)[] = [];
  private running = false;

  enqueue(task: () => Promise<void>): void {
    this.queue.push(task);
    this.process();
  }

  private async process(): Promise<void> {
    if (this.running) return;
    this.running = true;
    while (this.queue.length > 0) {
      const task = this.queue.shift()!;
      try {
        await task();
      } catch (err) {
        console.error('Queue task error:', err);
      }
    }
    this.running = false;
  }
}
```
</details>

### Exercise 5: Compile-Time Callback Return Validator (Hard)
- **Problem Statement:** Author a higher-order wrapper `enforceVoidReturn<F>` that statically rejects any callback whose implementation returns a non-void value.
- **Test Case:**
```typescript
type PureVoid<F> = F extends (...args: any[]) => infer R
  ? [R] extends [void]
    ? F
    : 'Error: Callback must return void'
  : never;
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
type PureVoid<F> = F extends (...args: any[]) => infer R
  ? [R] extends [void]
    ? F
    : 'Error: Callback must not return a value'
  : never;

function registerAction<F extends (...args: any[]) => any>(
  fn: F & PureVoid<F>
): void {
  fn();
}
```
</details>

### Exercise 6: Typed Event Listener Registry (Hard)
- **Problem Statement:** Build a type-safe pub-sub class where listeners can be registered and unregistered with exact payload typing per event name.
- **Test Case:**
```typescript
type Events = { click: { x: number; y: number }; keypress: { key: string } };
const bus = new TypedEventBus<Events>();
bus.on('click', (data) => console.log(data.x));
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
class TypedEventBus<Events extends Record<string, any>> {
  private listeners: { [K in keyof Events]?: ((payload: Events[K]) => void)[] } = {};

  on<K extends keyof Events>(event: K, handler: (payload: Events[K]) => void): () => void {
    if (!this.listeners[event]) {
      this.listeners[event] = [];
    }
    this.listeners[event]!.push(handler);
    return () => {
      this.listeners[event] = this.listeners[event]!.filter((h) => h !== handler);
    };
  }

  emit<K extends keyof Events>(event: K, payload: Events[K]): void {
    const handlers = this.listeners[event];
    if (handlers) {
      for (const h of handlers) {
        h(payload);
      }
    }
  }
}
```
</details>
