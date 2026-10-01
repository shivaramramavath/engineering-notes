# Map and Set

`Map` and `Set` (ES2015) are collections built for dynamic keys and unique values, with predictable ordering and fast lookups.

## Map: key to value

```js
const m = new Map();
m.set("a", 1).set("b", 2);          // set returns the map: chainable
m.get("a");                          // 1
m.has("b");                          // true
m.delete("a");                       // true
m.size;                              // 1 (property, not length)
m.clear();

const m2 = new Map([["x", 1], ["y", 2]]);   // from entries
const m3 = new Map(Object.entries({ a: 1 }));
```

### Any key type

```js
const objKey = {};
const fnKey = () => {};
m.set(objKey, "object").set(fnKey, "function").set(NaN, "nan").set(1, "one").set("1", "string one");
m.get(NaN);      // "nan" (SameValueZero equality)
m.get(1);        // "one"  (1 and "1" are different keys)
m.get({});       // undefined (different reference)
```

Key equality is **SameValueZero**: like `===`, but `NaN` equals `NaN`, and `0` equals `-0`.

### Iteration (insertion order)

```js
for (const [k, v] of m) {}
for (const k of m.keys()) {}
for (const v of m.values()) {}
m.forEach((value, key) => {});        // note the (value, key) order
[...m];  Array.from(m.entries());
Object.fromEntries(m);                // Map to object (string keys)
```

### Map.groupBy (ES2024)

```js
const byType = Map.groupBy(items, (i) => i.type);   // Map { "a" => [...], "b" => [...] }
```

## Set: unique values

```js
const s = new Set([1, 2, 2, 3]);     // {1, 2, 3}
s.add(4).add(1);                     // duplicates ignored
s.has(2);  s.delete(2);  s.size;  s.clear();

[...new Set(list)];                  // deduplicate primitives
new Set("hello");                    // {"h", "e", "l", "o"}
```

Iteration is in **insertion order**. `keys()` and `values()` are the same for Sets; `entries()` yields `[v, v]`.

## Set algebra (ES2025)

```js
const a = new Set([1, 2, 3]);
const b = new Set([3, 4]);

a.union(b);                 // {1, 2, 3, 4}
a.intersection(b);          // {3}
a.difference(b);            // {1, 2}
a.symmetricDifference(b);   // {1, 2, 4}
a.isSubsetOf(b);  a.isSupersetOf(b);  a.isDisjointFrom(b);
```

Fallback for older runtimes:

```js
const union = (x, y) => new Set([...x, ...y]);
const intersection = (x, y) => new Set([...x].filter((v) => y.has(v)));
const difference = (x, y) => new Set([...x].filter((v) => !y.has(v)));
```

## Map vs plain object

| | `Map` | Object |
|---|-------|--------|
| Key types | any | string / symbol |
| Order | insertion | integer keys first, then insertion |
| Size | `m.size` | `Object.keys(o).length` |
| Iteration | directly iterable | `Object.entries` |
| Default keys from prototype | none | `toString`, `constructor`, `__proto__` |
| Frequent add/delete | optimized | slower |
| JSON support | manual conversion | built in |
| Best for | dictionaries, caches, counters | records with known fields |

## Set vs array

| | `Set` | Array |
|---|-------|-------|
| Uniqueness | enforced | manual |
| Membership `has` | O(1) average | `includes` O(n) |
| Index access | no | yes |
| Order operations (sort, slice) | convert first | native |
| Duplicates | no | yes |

## Practical patterns

```js
// Counter
const counts = new Map();
for (const w of words) counts.set(w, (counts.get(w) ?? 0) + 1);

// Group manually
const groups = new Map();
for (const u of users) {
  if (!groups.has(u.role)) groups.set(u.role, []);
  groups.get(u.role).push(u);
}

// Index by id
const byId = new Map(users.map((u) => [u.id, u]));

// Fast membership
const allowed = new Set(["read", "write"]);
allowed.has(action);

// LRU cache: Map keeps insertion order
class LRU {
  #max; #map = new Map();
  constructor(max) { this.#max = max; }
  get(k) {
    if (!this.#map.has(k)) return undefined;
    const v = this.#map.get(k);
    this.#map.delete(k); this.#map.set(k, v);       // refresh
    return v;
  }
  set(k, v) {
    this.#map.delete(k); this.#map.set(k, v);
    if (this.#map.size > this.#max) this.#map.delete(this.#map.keys().next().value);
  }
}
```

## Serialization

```js
JSON.stringify(new Map([["a", 1]]));       // "{}"  (not serialized)
JSON.stringify([...new Map([["a", 1]])]);  // '[["a",1]]'
JSON.stringify([...new Set([1, 2])]);      // "[1,2]"
new Map(JSON.parse('[["a",1]]'));
```

## Object keys and equality

Sets and Maps compare objects by **reference**.

```js
const s = new Set([{ id: 1 }, { id: 1 }]);   // size 2 (different objects)
```

To dedupe by a field: `new Map(items.map((i) => [i.id, i])).values()`.

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| `map["key"] = v` instead of `set` | Sets an ordinary property | `map.set` |
| Using `length` | Undefined | `size` |
| Mutating while iterating | Unexpected visits | Iterate a copy |
| Expecting deep equality for object keys | Reference equality | Use string keys (IDs) |
| `JSON.stringify(map)` | `{}` | Convert to entries/object |
| Unbounded caches in `Map` | Memory leak | LRU, size limits, `WeakMap` |
| `forEach` argument order | `(value, key)` | Remember it |
| Assuming Set methods exist everywhere | Older runtimes | Fallback helpers |

## Key takeaways

- `Map` for dynamic keys of any type; `Set` for unique values
- Both preserve insertion order and have O(1) average lookup
- Keys compare by SameValueZero (objects by reference)
- Convert to arrays/objects for JSON

**Next:** [WeakMap, WeakSet, WeakRef](./07_weakmap-weakset-weakref.md)
