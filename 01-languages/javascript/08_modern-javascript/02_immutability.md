# Immutability

**Immutable** data never changes after creation. To "change" it you create a **new** value. This removes a whole class of bugs where one part of a program silently alters data another part relies on.

```js
// mutation
const user = { name: "Ada", tags: ["a"] };
user.name = "Grace";                 // original changed for everyone holding it

// immutable update
const updated = { ...user, name: "Grace" };   // user untouched
```

## What is already immutable

| Type | Immutable? |
|------|-----------|
| `string`, `number`, `boolean`, `bigint`, `symbol`, `null`, `undefined` | Yes (primitives) |
| Objects, arrays, `Map`, `Set`, `Date` | No (mutable by default) |

```js
const s = "hello";
s[0] = "H";                 // ignored; strings never change
s.toUpperCase();            // new string
```

## `const` is not immutability

```js
const list = [1, 2];
list.push(3);               // allowed: const locks the binding, not the contents
```

## Mutating vs non-mutating operations

| Task | Mutating | Non-mutating |
|------|----------|--------------|
| Add to array | `push`, `unshift` | `[...arr, x]`, `[x, ...arr]`, `concat` |
| Remove from array | `pop`, `shift`, `splice` | `filter`, `slice`, `toSpliced` |
| Replace item | `arr[i] = v` | `map`, `with(i, v)` |
| Sort / reverse | `sort`, `reverse` | `toSorted`, `toReversed` |
| Set property | `obj.k = v` | `{ ...obj, k: v }` |
| Delete property | `delete obj.k` | `const { k, ...rest } = obj` |
| Merge | `Object.assign(a, b)` | `{ ...a, ...b }` |
| Map/Set add | `set.add(x)`, `map.set(k, v)` | `new Set([...set, x])`, `new Map(map).set(k, v)` |

## Updating nested data

Copy each level along the path you change.

```js
const state = {
  user: { name: "Ada", address: { city: "London", zip: "N1" } },
  todos: [{ id: 1, done: false }, { id: 2, done: false }],
};

const moved = {
  ...state,
  user: {
    ...state.user,
    address: { ...state.user.address, city: "Paris" },
  },
};

const toggled = {
  ...state,
  todos: state.todos.map((t) => (t.id === 2 ? { ...t, done: true } : t)),
};

const withoutFirst = { ...state, todos: state.todos.filter((t) => t.id !== 1) };
```

Untouched branches are **shared** between old and new versions (structural sharing), which is safe because none of them mutate. It is also cheap: only the changed path is copied.

## `Object.freeze`

Enforces immutability at runtime, **shallowly**.

```js
"use strict";
const config = Object.freeze({ mode: "prod", limits: { max: 5 } });

config.mode = "dev";          // TypeError in strict mode (silent in sloppy mode)
config.limits.max = 10;       // works: nested object not frozen
```

Deep freeze:

```js
function deepFreeze(value) {
  if (value && typeof value === "object" && !Object.isFrozen(value)) {
    Object.freeze(value);
    Object.values(value).forEach(deepFreeze);
  }
  return value;
}
```

Use freeze in development and tests to catch accidental mutation; it has a small cost.

## `structuredClone` for deep copies

```js
const copy = structuredClone(state);   // independent deep copy
```

Fine occasionally, but copying everything on every update is wasteful. Prefer targeted updates.

## Read-only in TypeScript / JSDoc

```ts
const items: readonly number[] = [1, 2];
type Point = Readonly<{ x: number; y: number }>;
const config = { mode: "prod" } as const;
```

## Libraries

| Library | Idea |
|---------|------|
| **Immer** | Write "mutating" code on a draft, get an immutable result |
| **Immutable.js** | Persistent data structures |
| **Redux Toolkit** | Uses Immer internally |

```js
import { produce } from "immer";

const next = produce(state, (draft) => {
  draft.user.address.city = "Paris";      // looks like mutation, produces a new object
  draft.todos.push({ id: 3, done: false });
});
```

## Benefits

| Benefit | Why |
|---------|-----|
| Predictability | Data cannot change under your feet |
| Easy change detection | `prev !== next` is enough (React, Redux) |
| Undo/redo, time travel | Keep old versions |
| Safe sharing and concurrency | No races on shared data |
| Simple caching/memoization | Same reference means same value |

## Costs

- More allocations (mitigated by structural sharing)
- Verbose nested updates (use helpers or Immer)
- Very large arrays copied per change can be slow: batch updates or use persistent structures

## Practical helpers

```js
const set = (obj, key, value) => ({ ...obj, [key]: value });
const update = (obj, key, fn) => ({ ...obj, [key]: fn(obj[key]) });
const insertAt = (arr, i, x) => [...arr.slice(0, i), x, ...arr.slice(i)];
const removeAt = (arr, i) => [...arr.slice(0, i), ...arr.slice(i + 1)];
const replaceAt = (arr, i, x) => arr.map((v, idx) => (idx === i ? x : v));

const counter = update({ count: 1 }, "count", (n) => n + 1);   // { count: 2 }
```

## Mutation that is fine

Local mutation that never escapes is safe:

```js
function range(n) {
  const out = [];                      // private, built up, then returned
  for (let i = 0; i < n; i++) out.push(i);
  return out;
}
```

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Assuming `const` prevents change | Contents still mutate | `Object.freeze` or copy on update |
| Shallow copy of nested data, then mutating nested part | Original changes | Copy each level on the path |
| `sort()` on state arrays | Mutates state | `toSorted()` or `[...arr].sort()` |
| `Object.freeze` assumed deep | Nested objects stay mutable | `deepFreeze` or Immer |
| Cloning the entire state each update | Slow, breaks sharing | Update only the changed path |
| `JSON.parse(JSON.stringify(x))` for cloning | Loses Dates, Maps, undefined | `structuredClone` |
| Mutating function arguments | Surprises callers | Return new values |

## Key takeaways

- Treat data as values: create new versions instead of editing
- Copy only along the changed path; share the rest
- `const` and `freeze` are shallow: know their limits
- Use Immer when nested updates get noisy

**Next:** [map, filter, reduce](./03_map-filter-reduce.md)
