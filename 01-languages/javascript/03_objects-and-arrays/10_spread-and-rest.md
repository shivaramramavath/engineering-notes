# Spread and Rest

Both use the same `...` syntax. **Spread expands** a collection into individual items. **Rest collects** individual items into a collection.

```
spread:  [...arr]   ─►  a, b, c        (one → many)
rest:    (...args)  ◄─  a, b, c        (many → one)
```

## How to tell them apart

| Position | Meaning |
|----------|---------|
| In an array/object literal or function call | **Spread** |
| In function parameters or destructuring patterns | **Rest** |

## Spread with arrays

```js
const a = [1, 2];
const b = [3, 4];

const merged = [...a, ...b];         // [1, 2, 3, 4]
const copy = [...a];                 // shallow copy
const withExtra = [0, ...a, 5];      // [0, 1, 2, 5]
const chars = [..."hey"];            // ["h", "e", "y"]
const unique = [...new Set([1, 1, 2])];
Math.max(...b);                      // 4  (spread into a call)
```

Works with any iterable (strings, Sets, Maps, generators, NodeLists).

## Spread with objects

```js
const defaults = { theme: "light", lang: "en" };
const user = { lang: "fr" };

const settings = { ...defaults, ...user };      // { theme: "light", lang: "fr" }  later wins
const updated = { ...settings, theme: "dark" }; // override one field
const copy = { ...settings };                   // shallow copy
```

Rules:

- Copies **own enumerable** properties (strings and symbols)
- Later properties overwrite earlier ones
- `null` and `undefined` are ignored: `{ ...null }` → `{}`
- Getters are read, values copied
- Prototype and non-enumerable properties are **not** copied

## Rest parameters

```js
function sum(first, ...others) {
  return others.reduce((a, b) => a + b, first);
}
sum(1, 2, 3);          // 6
```

Must be the **last** parameter, and only one is allowed.

## Rest in destructuring

```js
const [head, ...tail] = [1, 2, 3];              // tail = [2, 3]
const { id, ...details } = { id: 1, a: 2, b: 3 };// details = { a: 2, b: 3 }
```

## Conditional spreading

```js
const config = {
  base: true,
  ...(isProd && { minify: true }),          // spreads false/undefined as nothing
  ...(user ? { user } : {}),
};

const list = [1, ...(includeTwo ? [2] : []), 3];
```

## Immutable updates

```js
const state = { items: [1, 2], meta: { page: 1 } };

const added = { ...state, items: [...state.items, 3] };
const nextPage = { ...state, meta: { ...state.meta, page: 2 } };
const removed = { ...state, items: state.items.filter(i => i !== 1) };
```

## Spread vs `apply`, `concat`, `assign`

| Old way | Modern way |
|---------|------------|
| `Math.max.apply(null, arr)` | `Math.max(...arr)` |
| `a.concat(b)` | `[...a, ...b]` |
| `Object.assign({}, a, b)` | `{ ...a, ...b }` |
| `Array.prototype.slice.call(args)` | `[...args]` / `(...args)` |

## Limits and performance

- Spreading into a **function call** with a huge array (about 100,000+ items) can throw `RangeError: Maximum call stack size exceeded`. Use a loop or `reduce`.
- Spreading inside `reduce` accumulators makes O(n²) work.
- Spread is **shallow**. Nested objects are shared.

```js
const big = new Array(200000).fill(1);
Math.max(...big);                        // may throw
big.reduce((m, n) => Math.max(m, n), -Infinity);   // safe
```

## Spread pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Assuming deep copy | Nested objects still shared | `structuredClone` |
| Spreading huge arrays in calls | Stack overflow | Loop or `reduce` |
| Spreading a non-iterable into an array (`[...obj]`) | `TypeError` | `Object.entries(obj)` |
| Property order overriding | Later spread wins silently | Put defaults first |
| Spread in `reduce` | O(n²) | Mutate the local accumulator |
| Losing class prototypes (`{ ...instance }`) | Plain object result | Explicit `clone()` |

## Key takeaways

- Spread expands (calls, literals); rest collects (parameters, patterns)
- Object spread: later keys win, shallow copy, own enumerable only
- Great for immutable updates and merging defaults
- Beware of size limits and O(n²) patterns

**Next:** [Optional Chaining and Nullish](./11_optional-chaining-and-nullish.md)
