# WeakMap, WeakSet, WeakRef

Weak collections hold their keys **weakly**: a key does not prevent garbage collection. When nothing else references the key object, the entry can disappear.

```
Map:      key ══strong══► value      (entry keeps key alive)
WeakMap:  key ──weak───► value       (entry does not keep key alive)
```

## WeakMap

```js
const meta = new WeakMap();

function track(el) {
  meta.set(el, { clicks: 0 });
}
const el = document.createElement("div");
track(el);
meta.get(el);    // { clicks: 0 }
meta.has(el);    // true
meta.delete(el);
```

When `el` is no longer referenced anywhere else, both the element and its metadata can be collected. No manual cleanup.

### Rules

| Rule | Detail |
|------|--------|
| Keys must be **objects** (or non-registered symbols, ES2023) | primitives throw `TypeError` |
| Not iterable | no `keys()`, `forEach`, `size`, `clear` |
| Values are held strongly while the key lives | a value referencing its key is fine |
| Lookups are by reference | |

## WeakSet

A set of objects that does not prevent collection.

```js
const visited = new WeakSet();

function walk(node) {
  if (visited.has(node)) return;     // handles cycles
  visited.add(node);
  node.children.forEach(walk);
}
```

Methods: `add`, `has`, `delete`. No iteration, no `size`.

## Use cases

| Use | Example |
|-----|---------|
| **Private data** for objects | `WeakMap` keyed by instance (pre-`#private`) |
| **Metadata/caches** tied to object lifetime | computed results per DOM node/object |
| **Marking** objects | "already processed" tags via `WeakSet` |
| **Cycle detection** in traversal | `WeakSet` of visited nodes |
| **Extending third-party objects** without mutation | store extra data in `WeakMap` |

### Cache keyed by object

```js
const cache = new WeakMap();

function heavy(obj) {
  if (cache.has(obj)) return cache.get(obj);
  const result = expensive(obj);
  cache.set(obj, result);
  return result;
}
```

The cache empties itself as objects become unreachable.

### Private data pattern (legacy)

```js
const priv = new WeakMap();
class Counter {
  constructor() { priv.set(this, { n: 0 }); }
  inc() { return ++priv.get(this).n; }
}
```

Modern code prefers `#private` fields.

## WeakRef

A weak reference to an object: you can read it if it is still alive, but it does not keep it alive.

```js
let obj = { big: new Array(1e6) };
const ref = new WeakRef(obj);

obj = null;                 // remove the strong reference
const alive = ref.deref();  // the object, or undefined if collected
if (alive) use(alive);
```

Rules:

- `deref()` may return the object or `undefined`; after the first non-`undefined` result in a job, it stays stable until the job ends
- Collection timing is **unpredictable**; never depend on it for logic

### Cache with WeakRef

```js
const cache = new Map();   // key -> WeakRef(value)

function getUser(id) {
  const cached = cache.get(id)?.deref();
  if (cached) return cached;
  const user = loadUser(id);
  cache.set(id, new WeakRef(user));
  return user;
}
```

The `Map` keys still accumulate; pair with a `FinalizationRegistry` to clean up.

## FinalizationRegistry

Register a callback to run **after** an object is collected.

```js
const registry = new FinalizationRegistry((heldValue) => {
  console.log("cleaned up", heldValue);
  cache.delete(heldValue);           // remove the stale key
});

registry.register(user, id);          // when `user` is collected, callback gets `id`
registry.unregister(token);           // optional, with an unregister token
```

Cautions:

- Callbacks may run late or **never** (for example when the program exits)
- Do not use for critical cleanup (closing files, releasing locks); use explicit `close()`, `try/finally`, or `using`
- The held value must not reference the target, or it stays alive

## Strong vs weak: which to use

| Need | Use |
|------|-----|
| Lookup by key, keep entries until deleted | `Map` |
| Auxiliary data that should vanish with the object | `WeakMap` |
| Set membership tied to object lifetime | `WeakSet` |
| Optional, best-effort reference to a big object | `WeakRef` |
| Cleanup notifications (best effort) | `FinalizationRegistry` |
| Need to iterate or count entries | `Map` / `Set` (weak collections cannot) |

## Debugging

DevTools Memory tab: take heap snapshots, trigger GC (trash can icon), and check whether objects remain. Weak references make this behavior nondeterministic in normal code, so avoid asserting on it in tests (Node: `--expose-gc` and `global.gc()` for experiments).

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Primitive keys in `WeakMap` | `TypeError` | Use `Map` |
| Expecting to iterate or count | Not supported | `Map` |
| Relying on GC timing | Non-deterministic | Explicit lifecycle management |
| Value referencing the key strongly outside | Key never collected | Avoid extra strong refs |
| Using `WeakRef` everywhere | Complexity, surprises | Only for real cache/memory needs |
| Critical cleanup in `FinalizationRegistry` | May never run | `try/finally`, explicit `dispose` |
| Holding the key in a closure | Key stays alive | Check retainers in heap snapshots |

## Key takeaways

- `WeakMap`/`WeakSet` attach data to objects without preventing garbage collection
- They accept object keys only, and cannot be iterated or sized
- `WeakRef` and `FinalizationRegistry` are best-effort tools for advanced caches
- Never build correctness on garbage collection timing

**Next:** [JSON](./08_json.md)
