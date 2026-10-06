# Memoization

Memoization caches a function's result keyed by its arguments, so calling it again with the same arguments returns the stored answer instead of recomputing. It trades **memory for time**.

It's valid only when the function is **pure**: the same inputs always produce the same output with no side effects. Memoizing anything else gives stale or wrong results.

## Prerequisites

- [Pure functions and side effects](../07_functional-programming/01_pure-functions-and-side-effects.md)
- [Closures](../06_closures/02_closure-use-cases.md)
- [Map and Set](../09_built-in-objects/06_map-and-set.md)

---

## Basic Implementation

```js
function memoize(fn) {
  const cache = new Map();
  return function (...args) {
    const key = JSON.stringify(args);
    if (cache.has(key)) return cache.get(key);
    const result = fn.apply(this, args);
    cache.set(key, result);
    return result;
  };
}
```

Classic payoff, recursion with overlapping subproblems:

```js
const fib = memoize((n) => (n < 2 ? n : fib(n - 1) + fib(n - 2)));
fib(80);   // fast: each n computed once, instead of exponential recomputation
```

(The recursion must call the **memoized** `fib`, not the original function.)

Use `cache.has(key)` rather than checking for `undefined`, so functions that legitimately return `undefined`, `0`, or `false` are cached correctly.

---

## The Hard Part: Cache Keys

The key must uniquely and cheaply identify "the same arguments."

| Approach | Good for | Problems |
|---|---|---|
| `JSON.stringify(args)` | Primitives, plain data | Slow on big objects; drops `undefined`/functions; `Map`/`Set`/`Date` lose fidelity; key order matters for objects |
| First argument only (a `Map` keyed by the value) | Single primitive arg | Wrong if other args matter |
| Object identity (`WeakMap`) | Single object arg | Same-content new objects miss the cache |
| Custom `keyFn` | Anything | You own correctness |

```js
function memoize(fn, keyFn = (...args) => JSON.stringify(args)) {
  const cache = new Map();
  return function (...args) {
    const key = keyFn(...args);
    if (cache.has(key)) return cache.get(key);
    const value = fn.apply(this, args);
    cache.set(key, value);
    return value;
  };
}

const getUserLabel = memoize((user) => format(user), (user) => user.id);   // key by id only
```

### `WeakMap` for object arguments

When the key is an object, a `WeakMap` lets cache entries be garbage-collected when the object is no longer referenced elsewhere, so the cache can't keep it alive.

```js
function memoizeByObject(fn) {
  const cache = new WeakMap();
  return (obj) => {
    if (!cache.has(obj)) cache.set(obj, fn(obj));
    return cache.get(obj);
  };
}
```

`WeakMap` keys must be objects, and it can't be iterated or sized. See [WeakMap, WeakSet, WeakRef](../09_built-in-objects/07_weakmap-weakset-weakref.md).

---

## Memoizing Async Functions

Cache the **promise**, not the resolved value. This also de-duplicates concurrent calls: ten callers asking for the same thing at once trigger one request.

```js
function memoizeAsync(fn, keyFn = (...args) => JSON.stringify(args)) {
  const cache = new Map();
  return (...args) => {
    const key = keyFn(...args);
    if (!cache.has(key)) {
      const promise = fn(...args).catch((err) => {
        cache.delete(key);       // never cache failures
        throw err;
      });
      cache.set(key, promise);
    }
    return cache.get(key);
  };
}

const getUser = memoizeAsync((id) => fetch(`/api/users/${id}`).then((r) => r.json()));
```

The `.catch` that evicts the entry is essential: otherwise one transient failure is cached **permanently** and every later call gets the same rejection. Real data usually also needs expiry; see [TTL caching](./08_caching.md).

---

## Bounding the Cache

An unbounded cache is a memory leak with good intentions. If the argument space is large (user input, IDs, search queries), cap it.

```js
function memoizeLimited(fn, max = 500) {
  const cache = new Map();
  return (key) => {
    if (cache.has(key)) return cache.get(key);
    const value = fn(key);
    cache.set(key, value);
    if (cache.size > max) cache.delete(cache.keys().next().value);   // evict oldest inserted
    return value;
  };
}
```

`Map` preserves insertion order, so the first key is the oldest. This is FIFO eviction. A true **LRU** also refreshes entries on read; see [Caching](./08_caching.md). Also see [Memory leaks](../18_memory-and-garbage-collection/03_memory-leaks.md).

---

## When It's Worth It

**Good fit:**
- Expensive pure computations called repeatedly with the same inputs (parsing, formatting, derived data)
- Recursive algorithms with overlapping subproblems (dynamic programming)
- Deduplicating identical in-flight async requests

**Poor fit:**
- Cheap functions: the key construction can cost more than recomputing
- Functions with side effects, randomness, or time-dependent output (`Date.now()`)
- Inputs that rarely repeat: you pay memory and get no hits
- Huge or unbounded input space without eviction

Measure before and after. See [Optimization patterns](../19_performance/05_optimization-patterns.md).

---

## Common Mistakes

| Mistake | Fix |
|---|---|
| Memoizing impure functions | Only memoize pure ones |
| Returning a cached **mutable** object that callers then mutate | Return frozen/copied data, or don't mutate |
| `JSON.stringify` keys with objects whose property order varies | Normalize or use a custom key |
| Cache grows forever | Add a size limit and/or TTL |
| Caching rejected promises | Evict on failure |
| Recursive function calls the un-memoized original | Assign the memoized version to the name used in recursion |
| Wrapping with an arrow and calling `this`-dependent methods | Use `fn.apply(this, args)` in a regular function wrapper |
| Using truthiness (`if (cache[key])`) to detect hits | `Map.has()` |

---

## Testing

```js
import { vi, it, expect } from 'vitest';

it('computes once per distinct argument set', () => {
  const fn = vi.fn((a, b) => a + b);
  const memo = memoize(fn);

  expect(memo(1, 2)).toBe(3);
  expect(memo(1, 2)).toBe(3);
  expect(memo(2, 1)).toBe(3);
  expect(fn).toHaveBeenCalledTimes(2);   // (1,2) once, (2,1) once
});

it('does not cache async failures', async () => {
  const fn = vi.fn()
    .mockRejectedValueOnce(new Error('boom'))
    .mockResolvedValue('ok');
  const memo = memoizeAsync(fn);

  await expect(memo('k')).rejects.toThrow('boom');
  await expect(memo('k')).resolves.toBe('ok');
});
```

---

## Quick Summary

- Memoization = cache results by arguments; **pure functions only**.
- Key design is the hard part: `JSON.stringify` for simple data, custom `keyFn` or `WeakMap` otherwise.
- For async, cache the **promise**, and evict on rejection.
- Bound the cache (size/TTL) or it becomes a leak.
- Memoize only when computation is expensive and inputs repeat. Measure.

**Next:** [Concurrency Control](./06_concurrency-control.md)
