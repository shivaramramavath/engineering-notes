# map, filter, reduce

The three core **higher-order array functions**. Each takes a callback, returns a **new** value, and never mutates the original array.

```
map     : [a, b, c]  ─► [f(a), f(b), f(c)]        same length, transformed
filter  : [a, b, c]  ─► [a, c]                    same items, fewer
reduce  : [a, b, c]  ─► one value                 anything
```

## map: transform every item

```js
const nums = [1, 2, 3];
nums.map((n) => n * 2);                       // [2, 4, 6]

const users = [{ name: "Ada", age: 36 }, { name: "Linus", age: 17 }];
users.map((u) => u.name);                     // ["Ada", "Linus"]
users.map((u) => ({ ...u, adult: u.age >= 18 }));
```

Callback receives `(item, index, array)`. Result length always equals input length.

## filter: keep matching items

```js
nums.filter((n) => n % 2 === 1);              // [1, 3]
users.filter((u) => u.age >= 18);
["a", "", null, "b"].filter(Boolean);          // ["a", "b"]
```

## reduce: fold into one value

```js
nums.reduce((sum, n) => sum + n, 0);          // 6
```

Signature: `reduce((accumulator, item, index, array) => newAccumulator, initialValue)`.

Always provide an initial value: without it, an empty array throws and the first item becomes the accumulator, which can hide bugs.

## Reduce recipes

```js
// Group by
const byAge = users.reduce((acc, u) => {
  const key = u.age >= 18 ? "adult" : "minor";
  (acc[key] ??= []).push(u);
  return acc;
}, {});

// Count occurrences
["a", "b", "a"].reduce((acc, x) => ((acc[x] = (acc[x] ?? 0) + 1), acc), {});

// Index by id
const byId = users.reduce((acc, u) => ({ ...acc, [u.id]: u }), {});   // O(n²) on big lists; use Object.fromEntries

// Flatten
[[1], [2, 3]].reduce((acc, a) => acc.concat(a), []);

// Max
nums.reduce((m, n) => Math.max(m, n), -Infinity);

// Pipeline of functions
const pipe = (...fns) => (x) => fns.reduce((v, f) => f(v), x);

// Async sequence
await tasks.reduce((p, task) => p.then(() => task()), Promise.resolve());
```

Prefer `Object.fromEntries(users.map(u => [u.id, u]))` or `Object.groupBy` for indexing/grouping.

## Implement them yourself

```js
const map = (fn, list) => list.reduce((acc, x, i) => [...acc, fn(x, i)], []);
const filter = (pred, list) => list.reduce((acc, x) => (pred(x) ? [...acc, x] : acc), []);

function reduce(fn, init, list) {
  let acc = init;
  for (const x of list) acc = fn(acc, x);
  return acc;
}
```

Shows that `reduce` is the most general: `map` and `filter` are special cases.

## The other members of the family

| Method | Purpose |
|--------|---------|
| `find` / `findLast` | first / last match |
| `findIndex` / `findLastIndex` | its index |
| `some` / `every` | any / all match (early exit) |
| `flatMap` | `map` then flatten one level |
| `flat` | flatten nested arrays |
| `forEach` | side effects only |
| `reduceRight` | fold from the end |
| `toSorted`, `toReversed`, `with` | immutable versions of mutating methods |

## Chaining pipelines

```js
const orders = [
  { id: 1, status: "paid", items: [{ price: 10, qty: 2 }, { price: 5, qty: 1 }] },
  { id: 2, status: "open", items: [{ price: 8, qty: 1 }] },
  { id: 3, status: "paid", items: [{ price: 20, qty: 1 }] },
];

const revenue = orders
  .filter((o) => o.status === "paid")
  .flatMap((o) => o.items)
  .map((i) => i.price * i.qty)
  .reduce((sum, n) => sum + n, 0);        // 45
```

Reads top to bottom like a recipe: what, not how.

## Performance notes

- Each step in a chain creates an intermediate array: O(n) per step, fine for most data
- For huge data or early exit, use a single `for...of` loop, iterator helpers (`.values().filter().map().take()` in modern runtimes), or generators
- `reduce` with object/array spread in the accumulator is O(n²); mutate the local accumulator instead
- `some`, `every`, `find` stop early; `filter(...).length > 0` does not

```js
// Iterator helpers (ES2025, check support): lazy, no intermediate arrays
const firstThree = bigArray.values().filter(isValid).map(transform).take(3).toArray();
```

## Async gotchas

`map`, `filter`, `forEach`, `reduce` do not await.

```js
const results = await Promise.all(ids.map((id) => fetchUser(id)));   // parallel

for (const id of ids) await save(id);                                // sequential

const passed = (await Promise.all(items.map(async (i) => [i, await check(i)])))
  .filter(([, ok]) => ok)
  .map(([i]) => i);                                                  // async filter
```

## Choosing between them

| I want to... | Use |
|--------------|-----|
| change each item | `map` |
| drop some items | `filter` |
| compute one value | `reduce` |
| just run an action | `for...of` / `forEach` |
| find one item | `find` |
| answer yes/no | `some` / `every` / `includes` |

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| `map` used for side effects | Creates a throwaway array | `forEach` / `for...of` |
| `reduce` without initial value | Throws on `[]`, wrong types | Always pass one |
| Mutating items inside `map` | Original data changes | Return new objects |
| Spread in `reduce` accumulator | O(n²) | Mutate the local accumulator |
| `map(parseInt)` | Index passed as radix | `map((s) => parseInt(s, 10))` |
| Long unreadable `reduce` | Hard to maintain | `map`/`filter` first, name the steps |
| `filter(...)[0]` for first match | Scans everything | `find` |
| Expecting async callbacks to be awaited | Promises ignored | `Promise.all` |

## Key takeaways

- `map` transforms, `filter` selects, `reduce` combines
- None of them mutate; all return new values
- Chain them for readable pipelines; use loops for early exit or huge data
- Give `reduce` an initial value and beware O(n²) spreads

**Next:** [Composition and Pipe](./04_composition-and-pipe.md)
