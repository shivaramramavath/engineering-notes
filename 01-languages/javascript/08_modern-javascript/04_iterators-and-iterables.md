# Iterators and Iterables

The **iteration protocols** define how `for...of`, spread, destructuring, `Array.from`, `Map`, `Set` and `Promise.all` consume sequences of values.

```
iterable  ──[Symbol.iterator]()──►  iterator  ──next()──►  { value, done }
```

## The two protocols

| Protocol | Requirement |
|----------|-------------|
| **Iterable** | has a `[Symbol.iterator]()` method that returns an iterator |
| **Iterator** | has a `next()` method returning `{ value, done }` |

```js
const iterator = [10, 20][Symbol.iterator]();
iterator.next();   // { value: 10, done: false }
iterator.next();   // { value: 20, done: false }
iterator.next();   // { value: undefined, done: true }
```

## Built-in iterables

| Iterable | Yields |
|----------|--------|
| Array, TypedArray | items |
| String | code points (not UTF-16 units) |
| Map | `[key, value]` pairs |
| Set | values |
| `arguments`, `NodeList` | items |
| Generators | whatever they `yield` |

Not iterable: plain objects (use `Object.entries(obj)`), `WeakMap`, `WeakSet`.

```js
[..."a😀"];                       // ["a", "😀"]  (by code point)
for (const [k, v] of new Map([["a", 1]])) {}
```

## What consumes iterables

```js
for (const x of iterable) {}
[...iterable];
const [a, b] = iterable;
Array.from(iterable);
new Set(iterable); new Map(iterable);
Promise.all(iterable);
Object.fromEntries(iterable);
Math.max(...iterable);
yield* iterable;
```

## How `for...of` works

```js
const it = iterable[Symbol.iterator]();
let step = it.next();
while (!step.done) {
  const item = step.value;
  // loop body
  step = it.next();
}
```

`break`, `return` or a thrown error inside the loop calls `it.return()` (if present) so the iterator can clean up (close files, release locks).

## Writing a custom iterable (manual)

```js
class Range {
  constructor(from, to) { this.from = from; this.to = to; }

  [Symbol.iterator]() {
    let current = this.from;
    const to = this.to;
    return {
      next: () => (current <= to ? { value: current++, done: false } : { value: undefined, done: true }),
      return: () => ({ done: true }),     // optional cleanup
    };
  }
}

[...new Range(1, 4)];   // [1, 2, 3, 4]
```

## Custom iterable with a generator (simpler)

```js
class Range2 {
  constructor(from, to) { this.from = from; this.to = to; }
  *[Symbol.iterator]() {
    for (let i = this.from; i <= this.to; i++) yield i;
  }
}
```

## Iterators that are also iterable

Built-in iterators return themselves from `[Symbol.iterator]()`, so they can be used in `for...of` directly.

```js
const it = [1, 2, 3].values();
for (const x of it) {}          // works
it[Symbol.iterator]() === it;   // true
```

Iterators are **single use**: once consumed, they are done.

```js
const gen = [1, 2][Symbol.iterator]();
[...gen];   // [1, 2]
[...gen];   // []
```

## Array iterator methods

```js
const arr = ["a", "b"];
arr.keys();     // 0, 1
arr.values();   // "a", "b"
arr.entries();  // [0, "a"], [1, "b"]

new Map([[1, "x"]]).entries();
new Set([1]).values();
```

## Infinite iterables

```js
const naturals = {
  *[Symbol.iterator]() { let n = 1; while (true) yield n++; },
};

function* take(n, iterable) {
  let i = 0;
  for (const x of iterable) {
    if (i++ >= n) return;
    yield x;
  }
}
[...take(3, naturals)];   // [1, 2, 3]
```

Never spread or `Array.from` an infinite iterable without limiting it.

## Iterator helpers (ES2025)

Iterator objects gain lazy methods, so you can chain without building intermediate arrays.

```js
const result = naturals[Symbol.iterator]()
  .filter((n) => n % 2 === 0)
  .map((n) => n * n)
  .take(3)
  .toArray();                     // [4, 16, 36]
```

| Helper | Purpose |
|--------|---------|
| `map`, `filter`, `take`, `drop`, `flatMap` | lazy transforms (return iterators) |
| `reduce`, `toArray`, `forEach`, `some`, `every`, `find` | consume the iterator |
| `Iterator.from(x)` | wrap any iterable/iterator |

Check runtime support (recent Chrome, Firefox, Safari, Node 22+), or use generators/libraries as a fallback.

## Async iterables

```js
const source = {
  async *[Symbol.asyncIterator]() {
    for (let i = 0; i < 3; i++) {
      await new Promise((r) => setTimeout(r, 100));
      yield i;
    }
  },
};
for await (const x of source) console.log(x);
```

Covered in `11_asynchronous-javascript/07_async-iterators-and-generators.md`.

## Iterables vs arrays

| | Array | Generic iterable |
|---|-------|------------------|
| Random access `arr[i]` | Yes | No |
| Known length | Yes | No |
| Lazy / infinite | No | Yes |
| Reusable | Yes | Often single-use |
| Memory | All items stored | One at a time |

Use iterables for streams, large data, and pipelines; convert with `Array.from` when you need indexing.

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Using `for...of` on a plain object | `TypeError: not iterable` | `Object.entries(obj)` |
| Reusing a consumed iterator | Empty second pass | Create a fresh one |
| Spreading an infinite iterable | Hangs, out of memory | `take` first |
| Forgetting `return()` cleanup | Resource leaks on early exit | Implement `return` or use generators with `try/finally` |
| Modifying a collection while iterating | Skips or repeats | Iterate a copy |
| `next()` returning a non-object | `TypeError` | Return `{ value, done }` |
| Assuming string iteration is by UTF-16 unit | Surrogate pairs | Iteration is by code point; `length` is not |

## Key takeaways

- Iterable: `[Symbol.iterator]()`. Iterator: `next()` returning `{ value, done }`
- `for...of`, spread, destructuring and many APIs consume iterables
- Generators are the easiest way to write iterators
- Iterators are lazy and single-use; iterator helpers add `map`/`filter`/`take`

**Next:** [Generators](./05_generators.md)
