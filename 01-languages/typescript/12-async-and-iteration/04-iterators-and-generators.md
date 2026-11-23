# Iterators, Generators, and Async Iteration in TypeScript

> Deep dive into iteration protocols in TypeScript: `Iterable<T>`, `Iterator<T, TReturn, TNext>`, synchronous and asynchronous generators (`AsyncGenerator<T>`), for-await-of loops, and backpressure.

| Metadata | Details |
| :--- | :--- |
| **Category** | Async & Iteration |
| **Difficulty** | Advanced |
| **Prerequisites** | [01-fundamentals/02-arrays-and-tuples.md](../01-fundamentals/02-arrays-and-tuples.md), [12-async-and-iteration/01-promises.md](./01-promises.md) |
| **TypeScript Baseline** | TS 3.6+ (Strict Generator Typing), ES2018 (Async Iteration) |
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
JavaScript provides protocols for consuming sequences of values sequentially: the **Iterable Protocol** and the **Iterator Protocol**. Generators (`function*`) and async generators (`async function*`) offer declarative coroutines that pause and resume execution on demand.

In TypeScript 3.6, generator typing was completely overhauled to support three separate type parameters: `Generator<TYield, TReturn, TNext>`. Understanding how values are yielded, returned, and injected back into the generator via `.next(value)`—as well as streaming asynchronous data via `AsyncIterable<T>` and `for await (... of ...)`—is foundational for reactive programming, database cursor pagination, and stream transformations.

---

## 2. Core Concepts
- **`Iterable<T>` and `Symbol.iterator`**: An object is iterable if it defines a method with key `Symbol.iterator` returning an `Iterator<T>`.
- **The Three Generator Type Parameters**:
  - `TYield`: The type of values produced by `yield` expressions.
  - `TReturn`: The type of the value produced by a final `return` expression (default `void`).
  - `TNext`: The type of values passed into the generator via `generator.next(val)` and returned by `yield` expressions inside the generator.
- **`AsyncIterable<T>` and `Symbol.asyncIterator`**: Represents an asynchronous stream where each iteration returns a `Promise<IteratorResult<T>>`.
- **`for await (... of ...)`**: Syntactic loop that awaits each yielded promise from an `AsyncIterable`, managing backpressure automatically.
- **Generator Coroutines**: Bi-directional communication where the consumer sends values back into the paused generator body.

---

## 3. How It Works Internally

```
+-------------------------------------------------------------+
|                  Consumer: generator.next(arg)              |
+------------------------------+------------------------------+
                               |
                               v
+-------------------------------------------------------------+
|                   Generator Function Execution              |
|        Runs until 'yield value' or 'return finalValue'      |
+------------------------------+------------------------------+
                               |
            +------------------+------------------+
            |                                     |
       yield yieldedValue;               return returnVal;
            |                                     |
            v                                     v
   { value: yieldedValue,                { value: returnVal,
     done: false }                         done: true }
            |                                     |
            +------------------+------------------+
                               |
                               v
                     State Pauses at Yield
          Next call injects 'arg' as expression result
```

In V8, generators allocate a generator object on the heap that preserves local variables, evaluation stack pointers, and the instruction counter when paused. In TypeScript, downleveling to ES5 transforms generators into state-machine switch statements (`__generator` helper).

---

## 4. Syntax & API Reference

```typescript
// 1. Explicit 3-argument Generator typing
function* counter(): Generator<number, string, boolean> {
  let count = 0;
  while (true) {
    const shouldReset: boolean = yield count++;
    if (shouldReset) {
      count = 0;
    }
  }
  return 'finished';
}

// 2. Custom Iterable Class
class NumberRange implements Iterable<number> {
  constructor(private start: number, private end: number) {}

  *[Symbol.iterator](): Iterator<number> {
    for (let i = this.start; i <= this.end; i++) {
      yield i;
    }
  }
}

// 3. Async Generator Streaming
async function* fetchPages(baseUrl: string): AsyncGenerator<string[], void, unknown> {
  let page = 1;
  while (page <= 5) {
    const res = await fetch(`${baseUrl}?page=${page}`);
    const data = (await res.json()) as string[];
    yield data;
    page++;
  }
}
```

---

## 5. Practical Examples

### Example 1: Infinite Sequence Generator
```typescript
function* fibonacciSequence(): Generator<bigint, void, unknown> {
  let current = 0n;
  let next = 1n;
  while (true) {
    yield current;
    const temp = current + next;
    current = next;
    next = temp;
  }
}

const fib = fibonacciSequence();
console.log(fib.next().value); // 0n
console.log(fib.next().value); // 1n
console.log(fib.next().value); // 1n
console.log(fib.next().value); // 2n
```

### Example 2: Real-World Database Cursor Paginator with `AsyncIterable`
```typescript
interface DatabaseRow { id: string; name: string }

async function* streamDatabaseQuery(
  batchSize = 100
): AsyncGenerator<DatabaseRow, void, unknown> {
  let lastId: string | null = null;
  let hasMore = true;

  while (hasMore) {
    const batch: DatabaseRow[] = await fetchDatabaseBatch(lastId, batchSize);
    if (batch.length === 0) {
      hasMore = false;
      break;
    }

    for (const row of batch) {
      yield row;
      lastId = row.id;
    }

    if (batch.length < batchSize) {
      hasMore = false;
    }
  }
}

async function fetchDatabaseBatch(_lastId: string | null, _limit: number): Promise<DatabaseRow[]> {
  return [{ id: '1', name: 'Alpha' }, { id: '2', name: 'Beta' }];
}
```

### Example 3: Edge Case - Bi-Directional Value Passing with `next(value)`
```typescript
function* accumulator(initial: number): Generator<number, void, number> {
  let total = initial;
  while (true) {
    // The value passed to generator.next(x) becomes the result of the yield expression!
    const delta: number = yield total;
    total += delta;
  }
}

const acc = accumulator(10);
console.log(acc.next().value);   // 10 (First call to next() primes the generator)
console.log(acc.next(5).value);  // 15
console.log(acc.next(20).value); // 35
```

### Example 4: Compiler Failure Case - Type Mismatch on `next(arg)`
```typescript
function* strictlyTypedGenerator(): Generator<string, void, number> {
  const injected: number = yield 'ready';
  console.log(`Received: ${injected * 2}`);
}

const gen = strictlyTypedGenerator();
gen.next(); // Prime generator
// @ts-expect-error TS2345: Argument of type 'string' is not assignable to parameter of type 'number'.
gen.next('invalid string');
```

### Example 5: Idiomatic Async Stream Transformer Pipeline
```typescript
export async function* filterAsync<T>(
  source: AsyncIterable<T>,
  predicate: (item: T) => boolean | Promise<boolean>
): AsyncGenerator<T, void, unknown> {
  for await (const item of source) {
    if (await predicate(item)) {
      yield item;
    }
  }
}

export async function* mapAsync<T, R>(
  source: AsyncIterable<T>,
  transform: (item: T) => R | Promise<R>
): AsyncGenerator<R, void, unknown> {
  for await (const item of source) {
    yield await transform(item);
  }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **The First `.next()` Ignores Arguments**: In JavaScript, arguments passed to the initial `.next()` call that primes the generator are discarded because execution has not yet paused at any `yield` expression.
2. **Missing `Symbol.asyncIterator` Support**: If your `tsconfig.json` `target` is below `ES2018`, you must include `"lib": ["ES2018.AsyncIterable", ...]` to access `AsyncIterable` and `AsyncIterator` types.
3. **Closing Iterators with `finally`**: When breaking early out of a `for ... of` loop (`break` or `return`), the runtime automatically calls `iterator.return()`. Always put cleanup code inside a `finally` block within your generator.
4. **Memory Leaks in Infinite Generators**: Storing infinite generator instances in arrays or global sets prevents garbage collection of the entire generator frame.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Consequence | Correction |
| :--- | :--- | :--- |
| Passing values to the first `gen.next(val)` call | Argument is silently ignored by runtime | Only pass arguments to subsequent `gen.next()` calls |
| Breaking from `for-await-of` without cleanup | Resource leaks (dangling sockets, open files) | Put cleanup logic in generator's `finally` block |
| `Generator<T>` without specifying `TReturn, TNext` | Defaults to `Generator<T, any, unknown>` | Annotate all three parameters when using `.next(val)` |
| Buffering entire async streams into memory | Out of memory (OOM) on large datasets | Stream item-by-item using `for await (... of ...)` |
| Omitting `Symbol.asyncIterator` method | Object is not consumable by `for-await-of` | Implement `[Symbol.asyncIterator]()` |

---

## 8. Performance & Type-Checker Complexity
- Generators provide **lazy evaluation**: elements are computed on demand ($\mathcal{O}(1)$ memory regardless of sequence length).
- Yielding values avoids large memory allocations compared to returning large arrays.
- TypeScript's type checker tracks generator types using three type variables, performing checking without noticeable overhead.

---

## 9. Compiler Internals
In TypeScript 3.6+:
- `Generator<T, TReturn, TNext>` is an interface extending `Iterator<T, TReturn, TNext>`.
- The Type Checker verifies that expressions assigned to `yield` match `TYield`, expressions receiving the value of `yield` match `TNext`, and `return` expressions match `TReturn`.

---

## 10. Best Practices & Design Guidelines
- Use `AsyncIterable<T>` for data streams (files, database rows, network chunks) to prevent high memory consumption.
- Always include a `finally` block in generators that allocate sockets, file handles, or transactions.
- Use `Generator<YieldType, ReturnType, NextType>` to strictly type coroutines.
- Keep generator functions focused on generation; compose them with separate filter/map stream transformers.
- In `tsconfig.json`, target `ES2022` or ensure `ES2018.AsyncIterable` is present in `lib`.

---

## 11. Real-World Case Study
A financial compliance service scanned multi-gigabyte audit log files from S3. Loading entire files using `fs.readFile` crashed containers with `JavaScript heap out of memory`.

**Refactored with Async Streaming Generators:**
```typescript
import * as fs from 'fs';
import * as readline from 'readline';

export async function* readAuditLogLines(filePath: string): AsyncGenerator<string, void, unknown> {
  const fileStream = fs.createReadStream(filePath);
  const rl = readline.createInterface({
    input: fileStream,
    crlfDelay: Infinity,
  });

  try {
    for await (const line of rl) {
      if (line.trim().length > 0) {
        yield line;
      }
    }
  } finally {
    fileStream.destroy();
  }
}
```
Memory usage dropped from 4GB+ to a flat 35MB, allowing the service to process arbitrarily large files with zero crashes.

---

## 12. Interview Questions

<details>
<summary>1. What are the three generic parameters of `Generator<T, TReturn, TNext>`?</summary>

- `T`: The type of elements yielded by the generator (`yield x`).
- `TReturn`: The type of the value returned when the generator finishes (`return y`).
- `TNext`: The type accepted by `.next(arg)` calls, which becomes the evaluation result of the `yield` expression inside the generator.
</details>

<details>
<summary>2. Why is the argument passed to the first `.next()` call on a generator ignored?</summary>

Because when a generator is freshly created, it has not yet begun execution and is not paused at any `yield` expression. There is no `yield` expression waiting to receive a value.
</details>

<details>
<summary>3. What happens to a generator when a `for ... of` loop terminates early via `break`?</summary>

The JavaScript engine automatically invokes the `.return()` method on the iterator object. If the generator has a `try ... finally` block, the `finally` block executes immediately, guaranteeing clean resource disposal.
</details>

<details>
<summary>4. What is the difference between `Iterable<T>` and `Iterator<T>`?</summary>

An `Iterable<T>` is an object that has a `[Symbol.iterator]()` method that produces an iterator. An `Iterator<T>` is the object itself that has a `.next()` method returning `{ value: T, done: boolean }`.
</details>

<details>
<summary>5. How does `for await (... of ...)` handle promises?</summary>

It accepts an `AsyncIterable<T>` or `Iterable<Promise<T>>`. On each iteration, it pauses until the promise for the next iterator result resolves, unwraps `.value`, and binds it to the loop variable.
</details>

<details>
<summary>6. How do you create an infinite sequence without running out of memory?</summary>

By using a generator function with a `while (true) { yield item; }` loop. Because generators execute lazily, values are produced only when `.next()` is called, using $\mathcal{O}(1)$ memory.
</details>

<details>
<summary>7. What does `yield*` do in a generator?</summary>

`yield*` delegates iteration to another iterable or generator, yielding all of its elements in sequence, and evaluating to the return value of the delegated generator.
</details>

<details>
<summary>8. How do you type an asynchronous generator that produces numbers and returns void?</summary>

`AsyncGenerator<number, void, unknown>`
</details>

---

## 13. Exercises & Challenges

### Easy
1. **Range Generator**: Implement `function* range(start: number, end: number, step = 1): Generator<number, void, unknown>`.
   <details>
   <summary>Solution</summary>

   ```typescript
   export function* range(start: number, end: number, step = 1): Generator<number, void, unknown> {
     for (let i = start; i <= end; i += step) {
       yield i;
     }
   }
   ```
   </details>

2. **Array to Async Iterable**: Write a function `toAsyncIterable<T>(array: T[]): AsyncIterable<T>`.
   <details>
   <summary>Solution</summary>

   ```typescript
   export async function* toAsyncIterable<T>(array: T[]): AsyncIterable<T> {
     for (const item of array) {
       yield item;
     }
   }
   ```
   </details>

### Medium
3. **Take While Generator**: Implement `takeWhile<T>(source: Iterable<T>, predicate: (item: T) => boolean): Generator<T, void, unknown>`.
   <details>
   <summary>Solution</summary>

   ```typescript
   export function* takeWhile<T>(
     source: Iterable<T>,
     predicate: (item: T) => boolean
   ): Generator<T, void, unknown> {
     for (const item of source) {
       if (!predicate(item)) return;
       yield item;
     }
   }
   ```
   </details>

4. **Async Chunking Stream**: Implement `chunkAsync<T>(source: AsyncIterable<T>, chunkSize: number): AsyncGenerator<T[], void, unknown>`.
   <details>
   <summary>Solution</summary>

   ```typescript
   export async function* chunkAsync<T>(
     source: AsyncIterable<T>,
     chunkSize: number
   ): AsyncGenerator<T[], void, unknown> {
     let buffer: T[] = [];
     for await (const item of source) {
       buffer.push(item);
       if (buffer.length >= chunkSize) {
         yield buffer;
         buffer = [];
       }
     }
     if (buffer.length > 0) {
       yield buffer;
     }
   }
   ```
   </details>

### Hard
5. **Bi-directional Coroutine State Machine**: Implement a generator `trafficLight()` typed as `Generator<string, void, 'ADVANCE' | 'RESET'>` that cycles `RED` -> `GREEN` -> `YELLOW` on `'ADVANCE'`, or returns to `RED` immediately on `'RESET'`.
   <details>
   <summary>Solution</summary>

   ```typescript
   type Signal = 'RED' | 'GREEN' | 'YELLOW';
   type Command = 'ADVANCE' | 'RESET';

   export function* trafficLight(): Generator<Signal, void, Command> {
     let state: Signal = 'RED';
     while (true) {
       const cmd: Command = yield state;
       if (cmd === 'RESET') {
         state = 'RED';
       } else if (cmd === 'ADVANCE') {
         state = state === 'RED' ? 'GREEN' : state === 'GREEN' ? 'YELLOW' : 'RED';
       }
     }
   }
   ```
   </details>

6. **Merging Multiple Async Streams with Fair Interleaving**: Implement `mergeAsync<T>(...sources: AsyncIterable<T>[]): AsyncIterable<T>` that pulls from multiple async streams concurrently and yields values as soon as any source produces them.
   <details>
   <summary>Solution</summary>

   ```typescript
   export async function* mergeAsync<T>(...sources: AsyncIterable<T>[]): AsyncIterable<T> {
     const iterators = sources.map(s => s[Symbol.asyncIterator]());
     const nextPromises = iterators.map((it, idx) =>
       it.next().then(res => ({ idx, res }))
     );

     let activeCount = iterators.length;

     while (activeCount > 0) {
       const { idx, res } = await Promise.race(nextPromises.filter(p => p !== null));
       if (res.done) {
         nextPromises[idx] = null as any;
         activeCount--;
       } else {
         yield res.value;
         nextPromises[idx] = iterators[idx].next().then(r => ({ idx, res: r }));
       }
     }
   }
   ```
   </details>
