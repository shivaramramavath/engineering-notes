# Async Iterators and Generators

> Asynchronous iterators and generator functions provide a unified, pull-based streaming protocol for processing sequential asynchronous data with natural backpressure and resource lifecycle guarantees.

---

## Metadata

| Attribute | Details |
| :--- | :--- |
| **Level** | Intermediate to Advanced |
| **Prerequisites** | [Iterators & Iterables](../08_modern-javascript/04_iterators-and-iterables.md), [Generators](../08_modern-javascript/05_generators.md), [Promises](./03_promises.md), [Async/Await](./05_async-await.md) |
| **Time to Read** | 24 minutes |
| **Status** | Complete |
| **Standard** | ECMAScript 2018 (ES9) / Baseline Widely Available |

---

## Table of Contents

1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals](#9-engine-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

Synchronous iteration (`Symbol.iterator` + `for...of`) models pull-based consumption where the consumer controls when the next value is produced. However, standard iterables assume that every value is already present in memory or can be calculated synchronously. When data arrives incrementally over I/O—such as network chunk streams, database cursor batches, message queue tails, or filesystem reads—synchronous iterators block execution or fail to model the asynchronous wait states.

Historically, Node.js and web browsers handled asynchronous streaming via push-based event emitters (`stream.on('data', chunk => ...)`). Push streams, while flexible, frequently suffer from backpressure mismanagement: if the producer pushes chunks faster than the downstream consumer can process them, memory explodes unless complex pause/resume machinery is hand-rolled.

Introduced in ECMAScript 2018 (ES9), **Async Iteration** (`Symbol.asyncIterator`, `for await...of`, and `async function*`) bridges the gap between pull-based control flow and asynchronous operations. The consumer pulls the next value via a promise (`next() -> Promise<{ value, done }>`), natively enforcing backpressure: the producer is suspended until the consumer resolves the previous yield and requests the next chunk.

---

## 2. Core Concepts

### The Async Iterable and Async Iterator Protocol
An object is an **Async Iterable** if it implements a method keyed by `Symbol.asyncIterator` that returns an **Async Iterator**.

An **Async Iterator** is an object with a `next()` method adhering to the protocol:
- Calling `next()` returns a `Promise` resolving to an iteration result: `{ value: any, done: boolean }`.
- It may optionally implement `return(value)` to signal premature termination (e.g., `break` or an exception inside `for await...of`).
- It may optionally implement `throw(error)` to inject an error into the generator sequence.

```text
+-----------------------+           Symbol.asyncIterator()          +-----------------------+
|    Async Iterable     | ---------------------------------------> |    Async Iterator     |
+-----------------------+                                          +-----------------------+
                                                                               |
                                                                               | next()
                                                                               v
                                                                   +-----------------------+
                                                                   | Promise<{value, done}>|
                                                                   +-----------------------+
```

### Async Generators (`async function*`)
An async generator function combines the suspension capabilities of generator functions (`yield`) with the promise-handling capabilities of `async` functions (`await`). Within an async generator:
- `await expr` suspends the generator until the promise settles.
- `yield expr` pauses execution, wrapping `expr` in a resolved promise returned to the consumer's `next()` call.
- `yield* asyncIterable` delegates iteration to another sync or async iterable.

---

## 3. How It Works Internally

When code invokes `for await (const item of asyncIterable)`, the JavaScript runtime executes a standardized protocol loop.

```
       +-------------------------------------------------------------+
       | Consumer executes: for await (const chunk of asyncIterable) |
       +-------------------------------------------------------------+
                                      |
                                      v
       +-------------------------------------------------------------+
       | Call iterator = asyncIterable[Symbol.asyncIterator]()       |
       +-------------------------------------------------------------+
                                      |
                                      v
+----> +-------------------------------------------------------------+
|      | Call promise = iterator.next()                              |
|      +-------------------------------------------------------------+
|                                     |
|                                     v
|      +-------------------------------------------------------------+
|      | Await promise settlement in microtask queue                 |
|      +-------------------------------------------------------------+
|                                     |
|                                     v
|                   +-----------------------------------+
|                   | Is result.done === true?          |
|                   +-----------------------------------+
|                         /                       \
|                   YES  /                         \ NO
|                       v                           v
|        +---------------------------+   +------------------------------------+
|        | Exit loop normally        |   | Assign chunk = result.value        |
|        +---------------------------+   | Execute loop body block            |
|                                        +------------------------------------+
|                                                           |
|                                                           v
|                                        +------------------------------------+
|                                        | Loop iteration finished. Repeat.   |
+--------------------------------------- +------------------------------------+
```

### Cleanup Semantics via `return()`
If the consumer exits the loop prematurely (via `break`, `return`, or an uncaught exception), the runtime automatically calls `iterator.return()`. This triggers any `finally` blocks inside the async generator, guaranteeing that underlying resources (file descriptors, sockets, database locks) are released without leaks.

---

## 4. Syntax & API Reference

### Declarations
```javascript
// Async generator function declaration
async function* fetchPages(baseUrl) {
  let url = baseUrl;
  while (url) {
    const res = await fetch(url);
    const data = await res.json();
    yield data.items;
    url = data.nextPageUrl;
  }
}

// Async generator expression
const generateIds = async function* () { yield 1; };

// Async generator object method
const datasource = {
  async *[Symbol.asyncIterator]() {
    yield 'record-1';
    yield 'record-2';
  }
};
```

### Consumption
```javascript
for await (const item of asyncIterable) {
  // item is the unpacked result.value
}
```

---

## 5. Practical Examples

### Example 1: Paginated REST API Scraper with Native Backpressure
```javascript
async function* paginateApi(endpoint, pageSize = 50) {
  let page = 1;
  let hasMore = true;

  while (hasMore) {
    const response = await fetch(`${endpoint}?page=${page}&limit=${pageSize}`);
    if (!response.ok) {
      throw new Error(`API HTTP Error: ${response.status}`);
    }

    const payload = await response.json();
    for (const item of payload.records) {
      yield item;
    }

    hasMore = page < payload.totalPages;
    page++;
  }
}

// Usage
async function processAllRecords() {
  const records = paginateApi('https://api.example.com/v1/customers');
  for await (const record of records) {
    if (record.isArchived) break; // Automatically triggers generator cleanup
    console.log(`Processing customer: ${record.id}`);
  }
}
```

### Example 2: Line-by-Line Node.js Readable Stream Reader
```javascript
import fs from 'node:fs';

async function* readLines(filePath) {
  const stream = fs.createReadStream(filePath, { encoding: 'utf8' });
  let buffer = '';

  try {
    for await (const chunk of stream) {
      buffer += chunk;
      const lines = buffer.split('\n');
      buffer = lines.pop() ?? ''; // Keep incomplete trailing fragment
      for (const line of lines) {
        yield line;
      }
    }
    if (buffer.length > 0) {
      yield buffer;
    }
  } finally {
    stream.destroy();
  }
}
```

### Example 3: Async Pipeline Operator (Map, Filter, Take)
```javascript
async function* asyncMap(iterable, fn) {
  for await (const item of iterable) {
    yield await fn(item);
  }
}

async function* asyncFilter(iterable, predicate) {
  for await (const item of iterable) {
    if (await predicate(item)) {
      yield item;
    }
  }
}

async function* asyncTake(iterable, limit) {
  let count = 0;
  for await (const item of iterable) {
    if (count++ >= limit) return;
    yield item;
  }
}

// Composition pipeline
async function runPipeline() {
  async function* sourceNumbers() {
    for (let i = 1; i <= 100; i++) {
      yield i;
    }
  }

  const pipeline = asyncTake(
    asyncMap(
      asyncFilter(sourceNumbers(), async x => x % 2 === 0),
      async x => x * 10
    ),
    3
  );

  for await (const val of pipeline) {
    console.log('Pipeline value:', val); // 20, 40, 60
  }
}
```

### Example 4: Web Socket Event Queue to Async Stream
```javascript
class AsyncQueue {
  #queue = [];
  #resolvers = [];
  #closed = false;

  push(value) {
    if (this.#closed) throw new Error('Queue is closed');
    if (this.#resolvers.length > 0) {
      const resolve = this.#resolvers.shift();
      resolve({ value, done: false });
    } else {
      this.#queue.push(value);
    }
  }

  close() {
    this.#closed = true;
    while (this.#resolvers.length > 0) {
      const resolve = this.#resolvers.shift();
      resolve({ value: undefined, done: true });
    }
  }

  async *[Symbol.asyncIterator]() {
    while (true) {
      if (this.#queue.length > 0) {
        yield this.#queue.shift();
      } else if (this.#closed) {
        return;
      } else {
        const nextPromise = new Promise(resolve => this.#resolvers.push(resolve));
        const { value, done } = await nextPromise;
        if (done) return;
        yield value;
      }
    }
  }
}
```

### Example 5: Polling with Exponential Backoff Generator
```javascript
async function* pollService(statusEndpoint, maxRetries = 5, baseDelayMs = 200) {
  let attempt = 0;

  while (attempt < maxRetries) {
    const res = await fetch(statusEndpoint);
    const data = await res.json();

    yield data;

    if (data.status === 'COMPLETED' || data.status === 'FAILED') {
      return;
    }

    const delay = baseDelayMs * 2 ** attempt;
    await new Promise(r => setTimeout(r, delay));
    attempt++;
  }

  throw new Error(`Polling timed out after ${maxRetries} attempts`);
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **`for await...of` on Synchronous Iterables**: `for await...of` works on normal synchronous arrays and iterables! It wraps each synchronous value in `Promise.resolve(val)`. However, doing this introduces microtask overhead on every single iteration.
2. **Missing `return()` Handling**: If you implement custom `Symbol.asyncIterator` without a `return()` method, exiting early (`break`) will fail to clean up open network connections or timers.
3. **Concurrent `next()` Invocations**: An async iterator expects calls to `next()` to be sequenced. If a consumer calls `iter.next()` multiple times concurrently before previous promises settle, async generators queue the requests internally in an engine-managed FIFO queue.
4. **Unhandled Rejections inside Generators**: Errors thrown inside `async function*` reject the promise returned by the active `next()` call. If unhandled by the consumer, Node.js triggers `unhandledRejection`.

---

## 7. Common Mistakes & Anti-Patterns

| Mistake | Incorrect Pattern | Correct Solution | Why It Fails |
| :--- | :--- | :--- | :--- |
| **Using `for...of` instead of `for await...of`** | `for (const p of asyncGen())` | `for await (const v of asyncGen())` | `for...of` yields pending Promises instead of unpacked values. |
| **Forgetting `await` inside generator** | `async function* f() { yield fetch(url); }` | `async function* f() { yield await (await fetch(url)).json(); }` | Yields an unresolved Promise object to the consumer. |
| **Ignoring Cleanup on Early Break** | Omitting `finally` block in generator | Enclose resource allocation in `try...finally` | Leaks file handles and network sockets when consumer breaks early. |
| **Pushing into Array instead of Streaming** | Fetching all 10,000 DB records into an array and returning | Stream records with `yield` one by one | Eliminates memory bloat by maintaining `O(1)` memory consumption. |
| **Using `forEach` with Async Functions** | `stream.forEach(async chunk => await process(chunk))` | `for await (const chunk of stream) { await process(chunk); }` | `forEach` ignores promises and processes chunks concurrently out-of-order. |
| **Concurrent unbuffered `next()` calls** | Calling `Promise.all([it.next(), it.next()])` naively | Consume sequentially or use an async buffer pattern | While spec-compliant, race conditions in stateful generators cause logic bugs. |

---

## 8. Performance Characteristics

- **Memory Efficiency**: Async generators operate with $O(1)$ intermediate memory overhead. Even when iterating through millions of database rows, only the currently active item resides in memory.
- **Microtask Tick per Iteration**: Each step in `for await...of` executes at least one microtask to settle the promise returned by `next()`. For tight, high-throughput in-memory loops, synchronous iterators are 5x–10x faster. Reserve async iterators for I/O-bound workflows.
- **Backpressure**: Because each `yield` halts the generator until the consumer awaits `next()`, downstream processing bottlenecks naturally throttle upstream I/O read rates without buffer overflow.

---

## 9. Engine Internals

In V8, an `async function*` creates an internal `JSAsyncGeneratorObject`.
1. **Internal State Machine**: The generator transitions through four states: `[SuspendedStart]`, `[SuspendedYield]`, `[Executing]`, and `[Completed]`.
2. **The AsyncGeneratorQueue**: V8 attaches an internal FIFO queue (`async_generator_request_queue`) to each async generator instance. When `next()`, `return()`, or `throw()` is invoked while the generator is already in `[Executing]` state, V8 enqueues an `AsyncGeneratorRequest` node holding the user's promise capability.
3. **Promise Unwrap Optimization**: When yielding, V8 unrolls the inner promise resolution directly via the `AsyncGeneratorResolve` bytecode, avoiding unnecessary intermediate promise allocation whenever possible.

---

## 10. Best Practices & Design Guidelines

1. **Always wrap external resources in `try...finally`**: When opening sockets, database transactions, or file descriptors inside an async generator, clean them up in `finally`.
2. **Combine with Web Streams**: Modern browser and Node.js `ReadableStream` instances implement `Symbol.asyncIterator` natively.
3. **Don't use `for await` on CPU-bound sync arrays**: Prefer standard `for (const x of arr)` for synchronous data structures to avoid microtask queuing overhead.

---

## 11. Real-World Case Study

### High-Volume Database Export with Streaming CSV Transformer

A financial reporting system needs to stream 500,000 audit records from Postgres and export them as a gzipped CSV download over HTTP without exceeding a 64MB container memory limit.

```javascript
import { createGzip } from 'node:zlib';
import { Readable } from 'node:stream';

// 1. Async Generator DB Cursor
async function* fetchAuditLogs(dbPool, batchSize = 1000) {
  let lastId = 0;
  while (true) {
    const rows = await dbPool.query(
      'SELECT id, user_id, action, timestamp FROM audit_logs WHERE id > $1 ORDER BY id ASC LIMIT $2',
      [lastId, batchSize]
    );

    if (rows.length === 0) break;

    for (const row of rows) {
      yield row;
    }
    lastId = rows[rows.length - 1].id;
  }
}

// 2. Transformer
async function* toCsv(recordStream) {
  yield 'ID,User,Action,Timestamp\n';
  for await (const r of recordStream) {
    yield `${r.id},"${r.user_id}","${r.action}","${r.timestamp}"\n`;
  }
}

// 3. HTTP Server Handler
export async function handleDownload(req, res, dbPool) {
  res.writeHead(200, {
    'Content-Type': 'application/gzip',
    'Content-Disposition': 'attachment; filename="audit.csv.gz"',
  });

  const recordStream = fetchAuditLogs(dbPool);
  const csvStream = toCsv(recordStream);

  // Convert async iterable to Node readable stream and pipe through Gzip
  const webReadable = Readable.from(csvStream);
  const gzip = createGzip();

  webReadable.pipe(gzip).pipe(res);
}
```

---

## 12. Interview Questions

<details>
<summary><strong>1. What is the difference between an Async Iterator and a standard Iterator?</strong></summary>

A standard iterator's `next()` returns an iteration result object directly: `{ value, done }`. An async iterator's `next()` returns a `Promise<{ value, done }>`. Furthermore, async iterables implement `Symbol.asyncIterator`, while sync iterables implement `Symbol.iterator`.
</details>

<details>
<summary><strong>2. What happens if you run <code>for await...of</code> over a regular synchronous Array?</strong></summary>

It executes successfully. The runtime unwraps each synchronous element by passing it through `Promise.resolve()`. However, each iteration incurs a microtask queue turn, making it substantially slower than a standard `for...of` loop for purely in-memory data.
</details>

<details>
<summary><strong>3. How does <code>for await...of</code> handle early loop termination via <code>break</code>?</strong></summary>

When a `break`, `return`, or uncaught exception exits a `for await...of` loop, the runtime inspects the iterator for a `.return()` method. If present, it awaits `iterator.return()`, ensuring that any `try...finally` blocks inside an async generator execute immediately.
</details>

<details>
<summary><strong>4. Can you use <code>yield</code> and <code>await</code> in the same function?</strong></summary>

Yes, but only in an **async generator function** (`async function*`). Inside an async generator, `await` pauses execution until a promise settles, and `yield` emits a value wrapped in a resolved promise to the consumer.
</details>

<details>
<summary><strong>5. How do async generators solve the producer-consumer backpressure problem?</strong></summary>

Unlike event emitters that push chunks continuously, async generators are pull-based. The generator's execution halts at `yield` until the consumer calls `next()`. Upstream data fetching is suspended until the consumer has finished processing the prior item.
</details>

<details>
<summary><strong>6. What is the result of calling <code>next()</code> on a closed async generator?</strong></summary>

It returns a Promise resolving to `{ value: undefined, done: true }`.
</details>

<details>
<summary><strong>7. How does error propagation work in an async generator?</strong></summary>

If an async generator throws an exception (or an awaited promise rejects without a `try/catch`), the active `next()` promise rejects with that error, marking the generator as permanently closed (`done: true`).
</details>

<details>
<summary><strong>8. What is the difference between <code>Readable.from(iterable)</code> and manual streaming?</strong></summary>

Node.js `Readable.from(asyncIterable)` automatically adapts any async iterable into a first-class Node.js `Readable` stream, wiring up stream pause/resume events directly to the async iterator's `next()` calls with built-in backpressure handling.
</details>

---

## 13. Exercises & Challenges

### Challenge 1: Implement an Async Batching Generator
Create an async generator `batch(asyncIterable, size)` that collects incoming items from an async iterable and yields them in chunks of `size`.

<details>
<summary>View Solution</summary>

```javascript
async function* batch(asyncIterable, size) {
  let chunk = [];
  for await (const item of asyncIterable) {
    chunk.push(item);
    if (chunk.length === size) {
      yield chunk;
      chunk = [];
    }
  }
  if (chunk.length > 0) {
    yield chunk;
  }
}
```
</details>

### Challenge 2: Implement Async Rate Limiter
Write an async generator `rateLimit(asyncIterable, itemsPerSecond)` that throttles the consumption of items, yielding at most `itemsPerSecond`.

<details>
<summary>View Solution</summary>

```javascript
async function* rateLimit(asyncIterable, itemsPerSecond) {
  const intervalMs = 1000 / itemsPerSecond;
  let lastYieldTime = 0;

  for await (const item of asyncIterable) {
    const now = Date.now();
    const elapsed = now - lastYieldTime;
    if (elapsed < intervalMs) {
      await new Promise(r => setTimeout(r, intervalMs - elapsed));
    }
    lastYieldTime = Date.now();
    yield item;
  }
}
```
</details>
