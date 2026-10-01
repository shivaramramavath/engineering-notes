# Async Iterators and Generators

A promise gives **one** future value. An **async iterator** gives a **sequence** of future values: pages from an API, lines from a file, chunks of a stream, messages from a socket.

```js
for await (const chunk of source) {
  process(chunk);
}
```

## The protocols

| Sync | Async |
|------|-------|
| iterable: `[Symbol.iterator]()` | async iterable: `[Symbol.asyncIterator]()` |
| iterator: `next()` returns `{ value, done }` | async iterator: `next()` returns a **Promise** of `{ value, done }` |
| `for...of` | `for await...of` |
| `function*` / `yield` | `async function*` / `yield` (and `await`) |

```js
const asyncIterable = {
  [Symbol.asyncIterator]() {
    let i = 0;
    return {
      async next() {
        await sleep(100);
        return i < 3 ? { value: i++, done: false } : { value: undefined, done: true };
      },
      async return() { return { done: true }; },       // optional cleanup on early exit
    };
  },
};

for await (const n of asyncIterable) console.log(n);   // 0, 1, 2
```

## Async generators

The easy way to build async iterators.

```js
async function* ticker(count, ms) {
  for (let i = 1; i <= count; i++) {
    await sleep(ms);
    yield i;
  }
}

for await (const n of ticker(3, 500)) console.log(n);
```

- `await` and `yield` both allowed
- Calling it returns an async generator object (async iterable and iterator)
- Values are produced **on demand**: the generator pauses until the consumer asks for the next one

## `for await...of`

```js
for await (const item of asyncIterable) { ... }
```

- Also accepts **sync iterables**, awaiting each element: `for await (const x of [p1, p2, p3])` processes promises in order
- Must be inside an `async` function or a module with top-level await
- `break`, `return` or a throw calls the iterator's `return()` so it can clean up

## Real examples

### Pagination

```js
async function* fetchPages(url) {
  let next = url;
  while (next) {
    const res = await fetch(next);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    const { items, nextUrl } = await res.json();
    yield* items;                  // yield each item (works with arrays)
    next = nextUrl;
  }
}

for await (const user of fetchPages("/api/users")) {
  if (user.id === target) break;     // stops requesting more pages
}
```

### Read lines from a file (Node)

```js
import { createReadStream } from "node:fs";
import { createInterface } from "node:readline";

const rl = createInterface({ input: createReadStream("big.log"), crlfDelay: Infinity });
for await (const line of rl) {
  if (line.includes("ERROR")) console.log(line);
}
```

### Streams are async iterables

```js
// Node readable streams
for await (const chunk of fs.createReadStream("a.bin")) total += chunk.length;

// Web streams (fetch bodies)
const res = await fetch(url);
for await (const chunk of res.body) { /* Uint8Array chunks (supported in modern runtimes) */ }

// Decode text chunks
const decoder = new TextDecoder();
for await (const chunk of res.body) text += decoder.decode(chunk, { stream: true });
```

### Events to async iterator

```js
import { on } from "node:events";
const ac = new AbortController();
for await (const [msg] of on(emitter, "message", { signal: ac.signal })) {
  handle(msg);
}
```

### Server-sent or polling loop

```js
async function* poll(fn, intervalMs, signal) {
  while (!signal?.aborted) {
    yield await fn();
    await sleep(intervalMs, signal);
  }
}
```

## Transforming async iterables

Compose lazily, like sync generators.

```js
async function* map(source, fn) { for await (const x of source) yield fn(x); }
async function* filter(source, pred) { for await (const x of source) if (await pred(x)) yield x; }
async function* take(source, n) {
  if (n <= 0) return;
  let i = 0;
  for await (const x of source) { yield x; if (++i >= n) return; }
}

const firstTenErrors = take(filter(map(readLines("app.log"), parse), (e) => e.level === "error"), 10);
for await (const e of firstTenErrors) console.log(e);
```

Newer runtimes add **async iterator helpers** (`.map`, `.filter`, `.take`, `.toArray`) as the proposals land; check support. `Array.fromAsync(source)` collects everything into an array.

## Delegation with `yield*`

```js
async function* all() {
  yield* firstSource();
  yield* secondSource();
}
```

## Bounded parallelism with async iterators

```js
async function* mapLimit(source, limit, fn) {
  const running = new Set();
  for await (const item of source) {
    const p = fn(item).then((value) => { running.delete(p); return value; });
    running.add(p);
    if (running.size >= limit) yield await Promise.race(running);
  }
  while (running.size) yield await Promise.race(running);   // completion order, not input order
}
```

## Backpressure

Generators pause at `yield` until the consumer requests more, so slow consumers naturally **slow producers**. This avoids buffering unbounded data. In Node, prefer `stream.pipeline` (or `Readable.from(asyncGen)`) for I/O pipelines.

```js
import { Readable } from "node:stream";
import { pipeline } from "node:stream/promises";
await pipeline(Readable.from(rows()), transform, fs.createWriteStream("out.csv"));
```

## Cleanup and early exit

```js
async function* withResource() {
  const handle = await open();
  try {
    while (true) yield await handle.readChunk();
  } finally {
    await handle.close();               // runs on break, return, throw, or completion
  }
}
```

`for await` calls `return()` on exit, which runs the generator's `finally` blocks.

## Sync vs async iteration

| | `for...of` | `for await...of` |
|---|-----------|------------------|
| Each step | synchronous | awaits a promise |
| Over promises array | gets promises | gets resolved values |
| Concurrency | n/a | one item at a time (sequential) |
| Use for | in-memory collections | streams, paginated or event data |

`for await` processes **sequentially**. To process items concurrently, collect and use `Promise.all` or a pool.

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| `for await` over a plain object | `TypeError: not async iterable` | Implement `Symbol.asyncIterator` or use a generator |
| Expecting parallelism from `for await` | Sequential by design | Pool / `Promise.all` |
| Forgetting `try/finally` for resources in generators | Leaks on early exit | `finally` blocks |
| Consuming unbounded streams into arrays | Memory blowup | Process incrementally |
| Ignoring errors inside the generator | Terminates the loop with a throw | `try/catch` around `for await` |
| Using `await` in sync generators | `SyntaxError` | `async function*` |
| Multiple concurrent `next()` calls on one iterator | Queued in order, surprising | Single consumer |
| Using async iterators for one-shot values | Overkill | Promises |

## Key takeaways

- Async iterables model sequences of future values; consume them with `for await...of`
- `async function*` makes producers simple, lazy and backpressure-friendly
- Early exit triggers `return()`, so use `try/finally` for cleanup
- `for await` is sequential: use pools for concurrency

**Next:** [Cancellation and Abort](./08_cancellation-and-abort.md)
