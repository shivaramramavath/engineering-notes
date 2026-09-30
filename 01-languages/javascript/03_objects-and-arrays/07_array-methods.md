# Array Methods

A tour of the built-in array toolbox, grouped by purpose.

## Transform

| Method | Returns | Example |
|--------|---------|---------|
| `map(fn)` | new array, same length | `[1, 2].map(n => n * 2)` → `[2, 4]` |
| `filter(fn)` | items where `fn` is truthy | `[1, 2, 3].filter(n => n > 1)` → `[2, 3]` |
| `reduce(fn, init)` | single accumulated value | `[1, 2, 3].reduce((a, n) => a + n, 0)` → `6` |
| `reduceRight(fn, init)` | reduce from the end | |
| `flat(depth)` | flatten nested arrays | `[1, [2, [3]]].flat(Infinity)` → `[1, 2, 3]` |
| `flatMap(fn)` | map then `flat(1)` | `["a b", "c"].flatMap(s => s.split(" "))` |

```js
const orders = [
  { id: 1, total: 30, paid: true },
  { id: 2, total: 50, paid: false },
  { id: 3, total: 20, paid: true },
];

const paidTotal = orders
  .filter(o => o.paid)
  .map(o => o.total)
  .reduce((sum, n) => sum + n, 0);   // 50
```

## Reduce recipes

```js
// group by
const byPaid = orders.reduce((acc, o) => {
  (acc[o.paid] ??= []).push(o);
  return acc;
}, {});

// count occurrences
["a", "b", "a"].reduce((acc, x) => ({ ...acc, [x]: (acc[x] ?? 0) + 1 }), {});

// max
nums.reduce((m, n) => Math.max(m, n), -Infinity);
```

Always pass an **initial value**; `reduce` on an empty array without one throws.

## Search and test

| Method | Returns |
|--------|---------|
| `find(fn)` | first matching item or `undefined` |
| `findIndex(fn)` | its index or `-1` |
| `findLast(fn)` / `findLastIndex(fn)` | from the end (ES2023) |
| `indexOf(x)` / `lastIndexOf(x)` | index using `===` |
| `includes(x)` | boolean (finds `NaN`) |
| `some(fn)` | true if any match (stops early) |
| `every(fn)` | true if all match (stops early) |

```js
[NaN].includes(NaN);   // true
[NaN].indexOf(NaN);    // -1
```

## Iterate

| Method | Notes |
|--------|-------|
| `forEach(fn)` | side effects only, cannot `break`, ignores return values |
| `entries()` / `keys()` / `values()` | iterators |
| `for...of` | supports `break`, `continue`, `await` |

## Add, remove, extract

| Method | Mutates? | Purpose |
|--------|----------|---------|
| `slice(start, end)` | No | copy a portion |
| `concat(...arrays)` | No | join arrays |
| `push/pop/shift/unshift` | Yes | ends |
| `splice(start, del, ...items)` | Yes | insert / delete |
| `toSpliced(...)` | No | same as splice, copy |
| `at(i)` | No | item by index, negatives allowed |
| `with(i, v)` | No | copy with one item replaced |

## Order

| Method | Mutates? |
|--------|----------|
| `sort(cmp)` | Yes |
| `toSorted(cmp)` | No |
| `reverse()` | Yes |
| `toReversed()` | No |

Sorting is covered in the next file.

## Convert and join

```js
["a", "b"].join(", ");           // "a, b"
Array.from(iterable, mapFn);
[..."abc"];                      // ["a", "b", "c"]
Array.prototype.slice.call(arrayLike);
Object.groupBy(items, fn);       // ES2024, see Object static methods
```

## Fill and create

```js
Array(3).fill(0);
Array.from({ length: 5 }, (_, i) => i + 1);   // [1, 2, 3, 4, 5]
```

## Chaining example

```js
const words = ["apple", "Banana", "cherry", "apple"];

const result = [...new Set(words.map(w => w.toLowerCase()))]   // unique
  .filter(w => w.length > 5)
  .toSorted()
  .map(w => w.toUpperCase());
// ["BANANA", "CHERRY"]
```

## Callback signature

All iteration methods call `fn(element, index, array)`. See `02_functions/05_callbacks.md` for the `map(parseInt)` trap.

## Async and array methods

`map`, `filter`, `forEach` do **not** await async callbacks.

```js
const results = await Promise.all(ids.map(id => fetchUser(id)));   // parallel

for (const id of ids) {          // sequential
  await save(id);
}
```

## Method selection guide

| I want to... | Use |
|--------------|-----|
| change every item | `map` |
| keep some items | `filter` |
| get one value from many | `reduce` |
| first match | `find` |
| does one match? | `some` |
| do all match? | `every` |
| exists (primitive)? | `includes` |
| flatten | `flat` / `flatMap` |
| loop with `break` | `for...of` |
| sort without mutation | `toSorted` |

## Method pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| `map` without using the result | Wasted array | `forEach` |
| `reduce` without initial value | Throws on empty array | Provide it |
| Spread inside `reduce` accumulators | O(n²) | Mutate the accumulator you own |
| `forEach` with `async` | Not awaited | `for...of` / `Promise.all` |
| `filter(...)[0]` for first match | Scans everything | `find` |
| `includes` on objects | Compares references | `some(pred)` |
| Mutating in `map` | Hidden side effects | Return new objects |

## Key takeaways

- `map`, `filter`, `reduce` cover most data transformations
- `find`, `some`, `every` stop early; `includes` handles `NaN`
- Prefer non-mutating variants (`toSorted`, `with`, `toSpliced`)
- Async callbacks need `Promise.all` or `for...of`

**Next:** [Sorting and Searching](./08_sorting-and-searching.md)
