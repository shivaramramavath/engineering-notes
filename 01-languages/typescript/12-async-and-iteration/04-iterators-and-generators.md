# Iterators and Generators

An **iterator** produces a sequence of values one at a time. A **generator** is a function that can pause and resume, which makes writing iterators almost trivial. Together they power `for...of`, spread, destructuring, `Map`/`Set`, and streaming data. Their async counterparts, async iterators and async generators, handle sequences whose items arrive over time: paginated APIs, streams, event feeds.

**Prerequisites:**
- [Generic types](../06-generics/01-generic-types.md)
- [Classes](../05-classes/00-classes.md)
- [async/await](./02-async-await.md)

---

## The iteration protocol

An object is **iterable** if it has a `[Symbol.iterator]()` method that returns an **iterator**. An iterator has a `next()` method returning `{ value, done }`.

```ts
const arr = [10, 20];
const it = arr[Symbol.iterator]();

it.next();   // { value: 10, done: false }
it.next();   // { value: 20, done: false }
it.next();   // { value: undefined, done: true }
```

`for...of`, spread (`[...x]`), `Array.from`, destructuring, `new Set(x)`, and `Promise.all(x)` all consume iterables through this protocol.

The built-in types:

| Type | Meaning |
|---|---|
| `Iterable<T>` | has `[Symbol.iterator]()` |
| `Iterator<T, TReturn, TNext>` | has `next()`, optionally `return()` and `throw()` |
| `IterableIterator<T>` | both iterable and an iterator |
| `Generator<T, TReturn, TNext>` | what a generator function returns |

`IteratorResult<T, TReturn>` is `{ done?: false; value: T } | { done: true; value: TReturn }`, a discriminated union on `done`.

## Making a class iterable

```ts
class Range implements Iterable<number> {
  constructor(private start: number, private end: number) {}

  [Symbol.iterator](): Iterator<number> {
    let current = this.start;
    const end = this.end;
    return {
      next(): IteratorResult<number> {
        return current <= end
          ? { value: current++, done: false }
          : { value: undefined, done: true };
      },
    };
  }
}

for (const n of new Range(1, 3)) console.log(n);   // 1, 2, 3
[...new Range(1, 3)];                               // [1, 2, 3]
```

Writing `next()` by hand is verbose. Generators remove the bookkeeping.

## Generators

A function declared with `function*` returns a `Generator`. `yield` produces a value and pauses. Execution resumes on the next `next()` call.

```ts
function* range(start: number, end: number): Generator<number, void, undefined> {
  for (let i = start; i <= end; i++) {
    yield i;
  }
}

for (const n of range(1, 3)) console.log(n);   // 1, 2, 3
```

The same `Range` class, simplified:

```ts
class Range implements Iterable<number> {
  constructor(private start: number, private end: number) {}

  *[Symbol.iterator](): Generator<number> {
    for (let i = this.start; i <= this.end; i++) yield i;
  }
}
```

Note the `*` before the method name.

### The three type parameters

`Generator<T, TReturn, TNext>`:

- `T`: the type of values **yielded**.
- `TReturn`: the type of the value **returned** when the generator finishes (`return x`). `for...of` and spread ignore it.
- `TNext`: the type of values **sent in** through `next(value)`.

```ts
function* conversation(): Generator<string, string, number> {
  const a = yield "How old are you?";   // `a` is a number, sent in via next(24)
  const b = yield `Got ${a}. Another?`;
  return `Total: ${a + b}`;
}

const g = conversation();
g.next();      // { value: "How old are you?", done: false }
g.next(24);    // { value: "Got 24. Another?", done: false }
g.next(6);     // { value: "Total: 30", done: true }
```

Two-way communication is rare in everyday code. In most generators, `TNext` is `undefined` or `unknown` and `TReturn` is `void`. If you omit the annotation, TypeScript infers `T` from the yields.

### Delegating with `yield*`

`yield*` delegates to another iterable, yielding all of its values:

```ts
function* flatten<T>(nested: Iterable<Iterable<T>>): Generator<T> {
  for (const inner of nested) yield* inner;
}

[...flatten([[1, 2], [3], [4, 5]])];   // [1, 2, 3, 4, 5]
```

## Why generators are useful

### Laziness

Values are produced **on demand**. Nothing runs until something asks, so infinite and large sequences are fine:

```ts
function* naturals(): Generator<number> {
  let n = 0;
  while (true) yield n++;
}

function* take<T>(source: Iterable<T>, count: number): Generator<T> {
  if (count <= 0) return;
  let i = 0;
  for (const item of source) {
    yield item;
    if (++i >= count) return;
  }
}

[...take(naturals(), 5)];   // [0, 1, 2, 3, 4]
```

### Lazy pipelines

```ts
function* map<T, U>(source: Iterable<T>, fn: (x: T) => U): Generator<U> {
  for (const x of source) yield fn(x);
}

function* filter<T>(source: Iterable<T>, pred: (x: T) => boolean): Generator<T> {
  for (const x of source) if (pred(x)) yield x;
}

const firstEvenSquares = take(filter(map(naturals(), (n) => n * n), (n) => n % 2 === 0), 3);
[...firstEvenSquares];   // [0, 4, 16]
```

No intermediate arrays are created, and only as many values as needed are computed. Recent runtimes and TypeScript versions are adding built-in iterator helper methods (`.map`, `.filter`, `.take`) directly on iterators. Check your runtime and `lib` before depending on them.

### Cleanup on early exit

When a `for...of` loop exits early (`break`, `return`, or a throw), the iterator's `return()` method is called, which runs the generator's `finally` blocks:

```ts
function* openFile(): Generator<string> {
  const handle = acquire();
  try {
    yield* readLines(handle);
  } finally {
    release(handle);      // runs even if the consumer breaks early
  }
}
```

This makes generators a tidy way to scope resources to iteration.

## Async iterators and generators

An **async iterable** has `[Symbol.asyncIterator]()` returning an iterator whose `next()` returns a `Promise<IteratorResult>`. Consume with `for await...of`:

```ts
async function* fetchPages(url: string): AsyncGenerator<Page, void, undefined> {
  let next: string | null = url;
  while (next) {
    const res = await fetch(next);
    const body = (await res.json()) as { items: Page[]; next: string | null };
    yield* body.items;
    next = body.next;
  }
}

for await (const page of fetchPages("/api/pages")) {
  console.log(page.title);
}
```

This is the cleanest way to consume paginated APIs: the consumer sees a flat stream, and pages are fetched lazily, only as fast as the loop asks. A `break` stops fetching.

The types:

| Sync | Async |
|---|---|
| `Iterable<T>` | `AsyncIterable<T>` |
| `Iterator<T>` | `AsyncIterator<T>` |
| `Generator<T, R, N>` | `AsyncGenerator<T, R, N>` |
| `for...of` | `for await...of` |

`for await...of` also accepts sync iterables of promises. Many streams are async iterable too: Node readable streams, `readline` interfaces, and web `ReadableStream` in supporting runtimes.

```ts
import { createReadStream } from "node:fs";
import { createInterface } from "node:readline";

const rl = createInterface({ input: createReadStream("big.log") });
for await (const line of rl) {
  if (line.includes("ERROR")) console.log(line);
}
```

Async iteration is **sequential**: the loop waits for each item. For concurrency, see [concurrency patterns](./05-concurrency-patterns.md).

## Compiler settings

- With `target` ES2015 or newer, `for...of`, spread, and generators work natively.
- With `target` ES5, iterating anything except arrays needs **`downlevelIteration`**, which emits helper code. Without it, `for (const x of someSet)` is an error.
- Async generators and `for await` need `lib` to include `es2018.asynciterable` (included in `es2018` and newer) and a target that supports them, or downleveling.

See [target, module, and lib](../13-compiler-and-tsconfig/02-target-module-and-lib.md).

## Important rules and misconceptions

**Generators are single-use.** After a generator completes, iterating it again yields nothing. An *iterable* object (like `Range`) creates a fresh generator each time `[Symbol.iterator]()` is called, so it can be looped repeatedly. A raw generator object cannot.

```ts
const g = range(1, 3);
[...g];   // [1, 2, 3]
[...g];   // []  (already exhausted)
```

**`return` value is not iterated.** `for...of` ignores `TReturn`. Use `next()` manually if you need it.

**Plain objects are not iterable.** `for (const x of { a: 1 })` is an error. Use `Object.entries(obj)`, `Object.keys`, or add a `[Symbol.iterator]`.

**Generators do not run in parallel.** They are cooperative: only one piece runs at a time, and only when asked.

**Spread of an infinite generator never ends.** Limit it first with something like `take`.

**Arrow functions cannot be generators.** Use `function*` or a `*method`.

## Common mistakes

- Forgetting the `*` on a generator method (`[Symbol.iterator]()` instead of `*[Symbol.iterator]()`).
- Consuming a generator twice and getting nothing the second time.
- Spreading an infinite generator.
- Using `for...of` on an object that is not iterable.
- Targeting ES5 without `downlevelIteration`.
- Using `for await` with a long chain of items when you wanted concurrency.
- Annotating a generator as returning `T[]` instead of `Generator<T>` or `Iterable<T>`.
- Skipping `try/finally` in a generator that holds a resource.

## Debugging

- Hover the generator to see the inferred `Generator<T, TReturn, TNext>`. A `yield` expression typed `any` means `TNext` is unspecified.
- If a loop runs once and then yields nothing on a second pass, the source is a single-use generator object.
- For "Type 'X' must have a '[Symbol.iterator]()' method that returns an iterator", the value is not iterable, or `lib`/`target` is too low.
- Add logging inside a generator to see exactly when it runs, which shows its laziness.
- Use `.return()` explicitly in tests to confirm cleanup in `finally` blocks.

## Quick summary

- An iterable has `[Symbol.iterator]()`, and an iterator has `next()` returning `{ value, done }`. `for...of`, spread, and destructuring use this protocol.
- `function*` returns a `Generator<T, TReturn, TNext>`. `yield` produces values lazily, and `yield*` delegates.
- Generators enable lazy pipelines, infinite sequences, and resource cleanup with `finally`.
- `async function*` and `for await...of` handle sequences that arrive over time, like paginated APIs and streams.
- Generators are single-use. Plain objects are not iterable. ES5 targets need `downlevelIteration`.

**Next:** [Concurrency patterns](./05-concurrency-patterns.md)
