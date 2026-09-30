# Object Static Methods

`Object` has helper functions for inspecting, combining and transforming objects.

## Inspecting

| Method | Returns |
|--------|---------|
| `Object.keys(o)` | own enumerable string keys |
| `Object.values(o)` | own enumerable values |
| `Object.entries(o)` | `[key, value]` pairs |
| `Object.getOwnPropertyNames(o)` | all own string keys (incl. non-enumerable) |
| `Object.getOwnPropertySymbols(o)` | own symbol keys |
| `Reflect.ownKeys(o)` | all own keys (strings and symbols) |
| `Object.hasOwn(o, k)` | own property check |
| `Object.getPrototypeOf(o)` | the prototype |

```js
const user = { name: "Ada", age: 36 };
Object.keys(user);      // ["name", "age"]
Object.values(user);    // ["Ada", 36]
Object.entries(user);   // [["name", "Ada"], ["age", 36]]
```

## `entries` + `fromEntries`: transform objects

```js
const prices = { apple: 1, pear: 2 };

const doubled = Object.fromEntries(
  Object.entries(prices).map(([k, v]) => [k, v * 2])
);
// { apple: 2, pear: 4 }

const onlyCheap = Object.fromEntries(
  Object.entries(prices).filter(([, v]) => v < 2)
);

Object.fromEntries(new Map([["a", 1]]));   // { a: 1 }
Object.fromEntries(new URLSearchParams("x=1&y=2"));   // { x: "1", y: "2" }
```

## Combining: `assign` and spread

```js
const merged = Object.assign({}, defaults, options);   // copies own enumerable props, later wins
const merged2 = { ...defaults, ...options };           // same idea, cleaner
```

`Object.assign(target, ...)` **mutates the first argument**.

## Creating

```js
Object.create(proto, descriptors?);   // new object with a given prototype
Object.create(null);                  // no prototype
```

## Prototypes

```js
Object.getPrototypeOf(obj);
Object.setPrototypeOf(obj, proto);    // slow, avoid in hot code
```

## Locking

```js
Object.freeze(o); Object.seal(o); Object.preventExtensions(o);
Object.isFrozen(o); Object.isSealed(o); Object.isExtensible(o);
```

## Descriptors

```js
Object.defineProperty(o, k, desc);
Object.defineProperties(o, descs);
Object.getOwnPropertyDescriptor(o, k);
Object.getOwnPropertyDescriptors(o);
```

## Comparing

```js
Object.is(NaN, NaN);   // true
Object.is(0, -0);      // false
```

## `Object.groupBy` (ES2024)

```js
const people = [
  { name: "Ada", role: "dev" },
  { name: "Linus", role: "dev" },
  { name: "Grace", role: "ops" },
];

const byRole = Object.groupBy(people, (p) => p.role);
// { dev: [Ada, Linus], ops: [Grace] }   (null-prototype object)

const map = Map.groupBy(people, (p) => p.role);   // Map with same grouping
```

Check runtime support, or use `reduce` as a fallback.

## Handy recipes

```js
// pick
const pick = (o, keys) => Object.fromEntries(keys.filter(k => k in o).map(k => [k, o[k]]));

// omit
const omit = (o, keys) => Object.fromEntries(Object.entries(o).filter(([k]) => !keys.includes(k)));

// count entries
Object.keys(user).length;

// empty check
const isEmpty = (o) => Object.keys(o).length === 0;

// invert
Object.fromEntries(Object.entries(o).map(([k, v]) => [v, k]));
```

## What each method includes

| | Own | Inherited | Non-enumerable | Symbols |
|---|-----|-----------|----------------|---------|
| `Object.keys/values/entries` | Yes | No | No | No |
| `for...in` | Yes | Yes | No | No |
| `Object.getOwnPropertyNames` | Yes | No | Yes | No |
| `Reflect.ownKeys` | Yes | No | Yes | Yes |
| Spread `{ ...o }` / `assign` | Yes | No | No | Yes (enumerable ones) |

## Static method pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| `Object.assign(defaults, opts)` | Mutates `defaults` | `Object.assign({}, defaults, opts)` or spread |
| Expecting deep merge | Both are shallow | Write or import a deep-merge helper |
| `Object.keys` on `null`/`undefined` | `TypeError` | Guard the input |
| `Object.setPrototypeOf` in hot paths | Deoptimizes | `Object.create` or classes |
| Assuming `entries` order for numeric keys | Integers first | Use `Map` |

## Key takeaways

- `keys`/`values`/`entries` plus `fromEntries` transform objects like arrays
- `assign` and spread merge shallowly; only `assign` mutates the target
- `Reflect.ownKeys` sees everything; `Object.keys` sees enumerable strings
- `Object.groupBy` groups items by a computed key

**Next:** [Copying and Cloning](./05_copying-and-cloning.md)
