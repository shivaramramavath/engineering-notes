# Copying and Cloning

Assigning an object copies the **reference**, not the data. To get an independent object you must clone it.

```js
const a = { n: 1 };
const b = a;       // same object
b.n = 2;
a.n;               // 2
```

## Shallow vs deep

```
shallow copy:  new top level, nested objects still SHARED
deep copy:     new top level AND new nested objects
```

## Shallow copy techniques

```js
const o1 = { ...original };
const o2 = Object.assign({}, original);
const a1 = [...list];
const a2 = list.slice();
const a3 = Array.from(list);
const m  = new Map(originalMap);
const s  = new Set(originalSet);
```

```js
const original = { name: "Ada", tags: ["a", "b"] };
const copy = { ...original };

copy.name = "Grace";      // original unaffected
copy.tags.push("c");      // original.tags ALSO changes (shared array)
```

## Deep copy: `structuredClone`

Built in to modern browsers, Node 17+, Deno, Bun.

```js
const deep = structuredClone(original);

deep.tags.push("c");      // original untouched
```

It supports: primitives, plain objects, arrays, `Date`, `RegExp`, `Map`, `Set`, `ArrayBuffer`/typed arrays, `Error`, and **circular references**.

It does **not** support:

| Unsupported | What happens |
|-------------|--------------|
| Functions | throws `DataCloneError` |
| DOM nodes | throws |
| Class instances | copied as plain objects (prototype lost) |
| Getters/setters | become plain values |
| Symbols as keys | dropped |
| Property descriptors | not preserved |

## `JSON` round trip (limited)

```js
const copy = JSON.parse(JSON.stringify(original));
```

Loses or breaks: `undefined`, functions, symbols (dropped), `Date` (becomes string), `Map`/`Set` (become `{}`), `NaN`/`Infinity` (become `null`), circular references (throws), `BigInt` (throws). Use only for simple JSON-like data.

## Comparing the methods

| Method | Depth | Dates, Maps, Sets | Circular refs | Functions | Class prototype |
|--------|-------|-------------------|---------------|-----------|-----------------|
| Spread / `assign` | shallow | shared refs | fine | copied by reference | lost (plain object) |
| `structuredClone` | deep | supported | supported | throws | lost |
| JSON round trip | deep | broken | throws | dropped | lost |
| Hand-written recursion | deep | you handle | you handle | your choice | your choice |

## Custom deep clone (learning version)

```js
function deepClone(value, seen = new WeakMap()) {
  if (value === null || typeof value !== "object") return value;
  if (seen.has(value)) return seen.get(value);          // circular refs

  if (value instanceof Date) return new Date(value);
  if (value instanceof Map) return new Map([...value].map(([k, v]) => [deepClone(k, seen), deepClone(v, seen)]));
  if (value instanceof Set) return new Set([...value].map((v) => deepClone(v, seen)));

  const copy = Array.isArray(value) ? [] : Object.create(Object.getPrototypeOf(value));
  seen.set(value, copy);
  for (const key of Reflect.ownKeys(value)) copy[key] = deepClone(value[key], seen);
  return copy;
}
```

## Immutable updates (avoid cloning everything)

Copy only the path you change:

```js
const state = { user: { name: "Ada", prefs: { theme: "light" } }, count: 0 };

const next = {
  ...state,
  user: {
    ...state.user,
    prefs: { ...state.user.prefs, theme: "dark" },
  },
};
// state untouched; unchanged branches are shared safely
```

Arrays:

```js
const added   = [...list, item];
const removed = list.filter((x) => x.id !== id);
const updated = list.map((x) => (x.id === id ? { ...x, done: true } : x));
const sorted  = list.toSorted();          // ES2023, non-mutating
const changed = list.with(1, "new");      // ES2023
```

## Equality after copying

```js
copy === original;                       // false
JSON.stringify(copy) === JSON.stringify(original);   // fragile: order-dependent
```

## Copying pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Assuming spread is deep | Nested objects are shared | `structuredClone` |
| JSON round trip for dates/maps | Data silently changes | `structuredClone` |
| `structuredClone` on class instances | Prototype and methods lost | Custom `clone()` method |
| `structuredClone` with functions | `DataCloneError` | Remove or rebuild them |
| Cloning huge objects on every change | Slow | Immutable path updates |
| Mutating "copied" arrays of objects | Element objects still shared | `map` with spread |

## Key takeaways

- Assignment shares; you must copy explicitly
- Spread and `assign` are shallow; `structuredClone` is deep
- JSON cloning breaks dates, maps, sets, undefined and more
- For state updates, copy only what changes

**Next:** [Arrays](./06_arrays.md)
