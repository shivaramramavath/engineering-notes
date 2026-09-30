# Sorting and Searching

## Default `sort` is by string

```js
[10, 9, 1, 100].sort();        // [1, 10, 100, 9]   (compared as strings!)
["b", "a", "C"].sort();        // ["C", "a", "b"]   (UTF-16 order)
```

## Comparator function

`compare(a, b)` returns:

| Return | Meaning |
|--------|---------|
| negative | `a` before `b` |
| `0` | keep equal |
| positive | `a` after `b` |

```js
nums.sort((a, b) => a - b);      // ascending
nums.sort((a, b) => b - a);      // descending

people.sort((a, b) => a.age - b.age);
```

`sort` **mutates**. Use `toSorted` (ES2023) or copy first: `[...nums].sort(...)`.

## Sorting strings correctly

```js
words.sort((a, b) => a.localeCompare(b));                     // locale aware
words.sort((a, b) => a.localeCompare(b, "en", { sensitivity: "base" }));   // ignore case/accents

const collator = new Intl.Collator("en", { numeric: true });
["file10", "file2"].sort(collator.compare);                   // ["file2", "file10"]
```

## Multi-key sorting

```js
users.sort((a, b) =>
  a.role.localeCompare(b.role) ||   // first by role
  b.age - a.age ||                  // then age descending
  a.name.localeCompare(b.name)      // then name
);
```

## Sorting rules to remember

- Sorting is **stable** (ES2019): equal items keep their original order
- `undefined` values go last and are not passed to the comparator
- The comparator must be **consistent** (no random returns)
- Comparing booleans or mixed types needs conversion: `Number(a) - Number(b)`
- `a - b` can overflow or give `NaN` for non-finite values; use `a < b ? -1 : a > b ? 1 : 0` for safety

## Decorate-sort-undecorate

When computing the key is expensive, compute once.

```js
const sorted = items
  .map(item => ({ item, key: expensiveKey(item) }))
  .sort((a, b) => a.key - b.key)
  .map(({ item }) => item);
```

## Searching

| Need | Method | Complexity |
|------|--------|-----------|
| Item by condition | `find` | O(n) |
| Index by condition | `findIndex` | O(n) |
| Primitive exists | `includes` | O(n) |
| Position of primitive | `indexOf` | O(n) |
| Any / all match | `some` / `every` | O(n), early exit |
| Membership in a big collection | `Set.has` | O(1) avg |
| Lookup by key | `Map.get` / object | O(1) avg |
| Sorted data | binary search | O(log n) |

```js
const ids = new Set(list.map(x => x.id));
ids.has(42);                              // fast lookups

const byId = new Map(list.map(x => [x.id, x]));
byId.get(42);
```

## Binary search (sorted arrays only)

```js
function binarySearch(sorted, target) {
  let lo = 0, hi = sorted.length - 1;
  while (lo <= hi) {
    const mid = (lo + hi) >>> 1;
    if (sorted[mid] === target) return mid;
    if (sorted[mid] < target) lo = mid + 1;
    else hi = mid - 1;
  }
  return -1;
}
binarySearch([1, 3, 5, 7, 9], 7);   // 3
```

## Lower bound (insertion point)

```js
function lowerBound(sorted, target) {
  let lo = 0, hi = sorted.length;
  while (lo < hi) {
    const mid = (lo + hi) >>> 1;
    if (sorted[mid] < target) lo = mid + 1;
    else hi = mid;
  }
  return lo;   // index where target could be inserted
}
```

## Uniqueness and duplicates

```js
[...new Set(list)];                                   // unique primitives
list.filter((x, i) => list.indexOf(x) === i);         // O(n²), avoid on big data

const uniqueById = [...new Map(items.map(i => [i.id, i])).values()];
```

## Shuffling

Do not use `sort(() => Math.random() - 0.5)`; it is biased. Use Fisher–Yates:

```js
function shuffle(array) {
  const a = [...array];
  for (let i = a.length - 1; i > 0; i--) {
    const j = Math.floor(Math.random() * (i + 1));
    [a[i], a[j]] = [a[j], a[i]];
  }
  return a;
}
```

## Sorting and searching pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Sorting numbers without comparator | Lexicographic order | `(a, b) => a - b` |
| Forgetting `sort` mutates | Changes shared data | `toSorted` |
| Comparing strings with `<` for users | Ignores locale | `localeCompare` / `Intl.Collator` |
| Comparator returning boolean | Inconsistent results | Return a number |
| `indexOf` on objects | Reference equality | `findIndex` with predicate |
| Linear search in loops | O(n²) overall | `Set` / `Map` |
| Random sort shuffle | Biased | Fisher–Yates |
| Binary search on unsorted data | Wrong answers | Sort first |

## Key takeaways

- Always give `sort` a comparator for numbers and use `localeCompare` for text
- `sort` is stable and mutating; `toSorted` copies
- Use `Set`/`Map` for fast lookups instead of repeated scans
- Binary search needs sorted input

**Next:** [Destructuring](./09_destructuring.md)
