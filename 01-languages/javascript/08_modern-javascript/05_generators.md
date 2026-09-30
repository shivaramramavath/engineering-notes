# Generators

A **generator** is a function that can **pause** and **resume**, producing a sequence of values lazily. Calling it returns an iterator.

```js
function* count() {
  yield 1;
  yield 2;
  yield 3;
}

const it = count();
it.next();   // { value: 1, done: false }
it.next();   // { value: 2, done: false }
it.next();   // { value: 3, done: false }
it.next();   // { value: undefined, done: true }

[...count()];   // [1, 2, 3]
```

## How execution works

1. Calling `count()` runs **nothing**; it creates a generator object
2. `next()` runs until the next `yield`, then pauses and returns the yielded value
3. Local variables and position are preserved between calls
4. When the function finishes (or `return`s), `done` becomes `true`

```js
function* demo() {
  console.log("start");
  const x = yield 1;
  console.log("got", x);
  return "end";
}
const g = demo();
g.next();        // logs "start", returns { value: 1, done: false }
g.next("hi");    // logs "got hi", returns { value: "end", done: true }
```

## Syntax forms

```js
function* a() {}
const b = function* () {};
const obj = { *c() {} };
class K { *d() {} static *e() {} *[Symbol.iterator]() {} }
```

Arrow generators do not exist.

## Two-way communication

`yield` is an expression: the value passed to `next(value)` becomes its result.

```js
function* averager() {
  let sum = 0, count = 0, avg;
  while (true) {
    const n = yield avg;
    sum += n; count++;
    avg = sum / count;
  }
}
const a = averager();
a.next();        // prime it
a.next(10);      // { value: 10, done: false }
a.next(20);      // { value: 15, done: false }
```

The first `next()` argument is ignored (nothing is waiting yet).

## `return()` and `throw()`

```js
const g = count();
g.return(99);         // { value: 99, done: true }, finishes early (runs finally blocks)

function* safe() {
  try {
    yield 1;
    yield 2;
  } catch (e) {
    console.log("caught", e);
    yield "recovered";
  } finally {
    console.log("cleanup");
  }
}
const s = safe();
s.next();             // 1
s.throw("boom");      // logs "caught boom", value "recovered"
s.return();           // logs "cleanup"
```

`for...of` calls `return()` on early exit (`break`), so `finally` blocks run and resources are released.

## Delegation with `yield*`

Delegate to another iterable or generator.

```js
function* inner() { yield 2; yield 3; return "inner done"; }

function* outer() {
  yield 1;
  const result = yield* inner();   // yields 2, 3; `result` is inner's return value
  yield 4;
}
[...outer()];   // [1, 2, 3, 4]
```

Great for recursion:

```js
function* walk(tree) {
  if (!tree) return;
  yield* walk(tree.left);
  yield tree.value;
  yield* walk(tree.right);
}
```

## Lazy and infinite sequences

```js
function* fibonacci() {
  let [a, b] = [0, 1];
  while (true) {
    yield a;
    [a, b] = [b, a + b];
  }
}

function* take(n, it) { let i = 0; for (const x of it) { if (i++ >= n) return; yield x; } }
function* map(fn, it) { for (const x of it) yield fn(x); }
function* filter(fn, it) { for (const x of it) if (fn(x)) yield x; }

[...take(5, map((x) => x * 2, filter((x) => x % 2 === 0, fibonacci())))];
```

Values are computed **only when requested**, and there are no intermediate arrays.

## Use cases

| Use | Example |
|-----|---------|
| Custom iterables | `*[Symbol.iterator]()` |
| Lazy pipelines / infinite sequences | ID generators, fibonacci |
| Traversing trees and graphs | `yield*` recursion |
| Pagination | yield page by page |
| State machines | pause between states |
| Reading big data in chunks | line by line, chunk by chunk |
| Coroutines (historic `co`, Redux-Saga) | Two-way `yield` |

## Pagination example

```js
async function* pages(url) {
  let next = url;
  while (next) {
    const res = await fetch(next);
    const data = await res.json();
    yield data.items;
    next = data.nextUrl;
  }
}

for await (const items of pages("/api/users")) {
  process(items);
}
```

## Async generators

`async function*` combines `await` and `yield`. They return async iterators, consumed with `for await...of`. See `11_asynchronous-javascript/07_async-iterators-and-generators.md`.

## Generators vs async/await

`async`/`await` was originally built on generators plus promises. Modern code uses `async`/`await` for asynchronous control flow; generators remain the tool for lazy sequences and iteration.

## Generator state

```js
const g = count();
g[Symbol.iterator]() === g;     // true (generators are iterable iterators)
g.next(); g.next(); g.next(); g.next();
g.next();                       // { value: undefined, done: true } forever after
```

A generator cannot be restarted; call the function again for a new one.

## Performance notes

- Each `yield` has overhead; for small arrays, normal loops are faster
- Generators shine when data is large, infinite, or expensive to compute eagerly
- Stack depth: `yield*` recursion is fine for trees but adds overhead per level

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Forgetting that calling the generator runs nothing | No output, confusion | Call `.next()` or iterate |
| Ignoring that the first `next(arg)` value is dropped | Off-by-one logic | Prime with `next()` |
| Reusing a finished generator | Empty results | Create a new one |
| Spreading an infinite generator | Hangs | Limit with `take` |
| Mixing `return` values with `for...of` | Return value is not yielded | Use `yield` for values you want to loop over |
| `yield` inside a nested regular function or callback | `SyntaxError` | Only directly inside `function*` |
| Skipping cleanup | Leaks on early exit | `try/finally` in the generator |

## Key takeaways

- `function*` returns an iterator; `yield` pauses and produces a value
- `next(value)` sends data in, `return()` and `throw()` control termination
- `yield*` delegates to other iterables and enables recursion
- Generators give lazy, memory-efficient sequences and custom iteration

**Next:** [ES Features by Version](./06_es-features-by-version.md)
