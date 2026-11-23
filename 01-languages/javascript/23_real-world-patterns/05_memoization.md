# Memoization Pattern & Cache Eviction in JavaScript

> A production-grade implementation and architectural guide to memoization in JavaScript, featuring LRU cache eviction, in-flight async promise deduplication, WeakMap garbage collection safety, and composite key serialization.

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced / Production-Grade |
| **Prerequisites** | [06_closures/02_closure-use-cases.md](file:///e:/Learning/engineering-notes/01-languages/javascript/06_closures/02_closure-use-cases.md), [09_built-in-objects/04_map-set-weakmap-weakset.md](file:///e:/Learning/engineering-notes/01-languages/javascript/09_built-in-objects/04_map-set-weakmap-weakset.md) |
| **Standard Baseline** | ECMAScript 2024 / Node.js 20+ LTS / Browser Evergreen |
| **Status** | Production Ready |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals](#9-engine-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

In computational algorithms, complex data transformations, and high-throughput web applications, programs frequently invoke identical functions with identical arguments repeatedly. Examples include:
- Recursive mathematical evaluations (dynamic programming, Fibonacci, combinations).
- AST parsing and regular expression compiling.
- Computing derived UI state or selector transformations (e.g. Redux / Zustand selectors).
- Fetching static or read-heavy entities from remote databases or microservices.

**Memoization** is an optimization technique where the return values of a pure function are cached against its input arguments. When the function is subsequently invoked with previously seen arguments, the cached result is returned immediately ($O(1)$ lookup), bypassing redundant CPU work and I/O latency.

---

## 2. Core Concepts

### Pure Functions Requirement
Memoization is **only valid for pure functions**:
- **Deterministic**: For a given set of arguments, the function must always return the exact same output.
- **Side-Effect Free**: The function must not mutate global state, write to disks, or depend on non-deterministic external inputs (e.g. `Date.now()`, `Math.random()`, or mutable global variables).

### Cache Eviction & Bounded Memory
An unbounded cache (`const cache = new Map()`) accumulates entries forever. In long-running Node.js processes or Single Page Applications, unbounded caches cause inevitable memory leaks. Production memoizers must enforce bounded storage via **Cache Eviction Strategies**:
- **LRU (Least Recently Used)**: Discards the item that has not been accessed for the longest time when capacity is reached.
- **TTL (Time to Live)**: Discards entries after a specified duration.
- **WeakMap Memoization**: Caches values keyed by object references, allowing the garbage collector to reclaim cache entries automatically when the key objects are collected.

```
Access Order:    [ A ] <-> [ B ] <-> [ C ]  (Max Capacity = 3)
Read 'B':        [ A ] <-> [ C ] <-> [ B ]  (B moves to most recently used)
Insert 'D':      [ C ] <-> [ B ] <-> [ D ]  (A evicted as least recently used)
```

---

## 3. How It Works Internally

An efficient LRU memoizer combines a **JavaScript `Map`** with an insertion-order maintenance strategy. Because JavaScript's standard `Map` retains insertion order:
1. When an existing key is read, it is deleted and immediately re-inserted: `map.delete(key); map.set(key, val);`. This moves the key to the back of the iteration order (most recently used).
2. When the cache exceeds `maxSize`, the oldest entry is obtained via `map.keys().next().value` and deleted in $O(1)$ time.

```
  memoizedFn(...args)
          │
  Compute key = resolver(...args)
          │
  cache.has(key)?
          ├── YES ──► Value found in cache!
          │             - Refresh LRU position (delete & set)
          │             - Check TTL expiration
          │             - Return cached value
          │
          └── NO  ──► Cache Miss!
                        - result = fn(...args)
                        - If cache.size >= maxSize: evict oldest entry
                        - cache.set(key, result)
                        - Return result
```

---

## 4. Syntax & API Reference

### Production-Grade LRU & TTL Memoizer
```javascript
/**
 * Creates a memoized function with LRU eviction and optional TTL.
 *
 * @template T
 * @param {(...args: any[]) => T} fn - Pure function to memoize.
 * @param {Object} [options={}] - Configuration options.
 * @param {number} [options.maxSize=100] - Maximum cache capacity.
 * @param {number} [options.ttlMs] - Time-to-live in milliseconds.
 * @param {(...args: any[]) => string} [options.resolver] - Custom key serializer.
 * @returns {Function & { cache: Map, clear: () => void }} Memoized function.
 */
export function memoize(fn, options = {}) {
  const {
    maxSize = 100,
    ttlMs = null,
    resolver = (...args) => (args.length === 1 && typeof args[0] !== 'object' ? String(args[0]) : JSON.stringify(args)),
  } = options;

  const cache = new Map();

  function memoized(...args) {
    const key = resolver(...args);
    const now = Date.now();

    if (cache.has(key)) {
      const entry = cache.get(key);

      // Check TTL expiry
      if (ttlMs !== null && now - entry.createdAt > ttlMs) {
        cache.delete(key);
      } else {
        // Move to most recently used (re-insert)
        cache.delete(key);
        cache.set(key, entry);
        return entry.value;
      }
    }

    const value = fn.apply(this, args);

    // Evict oldest entry if capacity reached
    if (cache.size >= maxSize) {
      const oldestKey = cache.keys().next().value;
      cache.delete(oldestKey);
    }

    cache.set(key, { value, createdAt: now });
    return value;
  }

  memoized.cache = cache;
  memoized.clear = () => cache.clear();

  return memoized;
}
```

---

## 5. Practical Examples

### Example 1: Recursive Fibonacci with Dynamic Programming Memoization
```javascript
import { memoize } from './memoize.js';

// Without memoization: O(2^N) exponential time complexity
// With memoization: O(N) linear time complexity
export const fibonacci = memoize((n) => {
  if (n <= 1) return BigInt(n);
  return fibonacci(n - 1) + fibonacci(n - 2);
});

console.log(fibonacci(100).toString()); // Computes instantly in < 1ms
```

### Example 2: Async In-Flight Promise Deduplication (SingleFlight Pattern)
```javascript
/**
 * Deduplicates in-flight asynchronous requests so concurrent identical calls
 * share a single Promise rather than firing multiple duplicate HTTP requests.
 */
export function memoizeAsync(asyncFn, options = {}) {
  const cache = new Map();
  const { ttlMs = 5000, maxSize = 200 } = options;

  return async function (...args) {
    const key = JSON.stringify(args);
    const now = Date.now();

    if (cache.has(key)) {
      const entry = cache.get(key);
      if (now - entry.createdAt <= ttlMs) {
        return entry.promise;
      }
      cache.delete(key);
    }

    // Wrap in promise cache
    const promise = asyncFn.apply(this, args).catch((err) => {
      // CRITICAL: Evict from cache on rejection so future attempts can retry!
      cache.delete(key);
      throw err;
    });

    if (cache.size >= maxSize) {
      const oldest = cache.keys().next().value;
      cache.delete(oldest);
    }

    cache.set(key, { promise, createdAt: now });
    return promise;
  };
}

// Usage
const fetchUserProfile = memoizeAsync(async (userId) => {
  console.log(`Fetching user ${userId} from API...`);
  const res = await fetch(`/api/users/${userId}`);
  return res.json();
});

// Concurrent callers share the exact same network promise
await Promise.all([
  fetchUserProfile('usr_101'),
  fetchUserProfile('usr_101'),
  fetchUserProfile('usr_101')
]); // Logs "Fetching user usr_101 from API..." exactly ONCE
```

### Example 3: Leak-Free Object Caching via `WeakMap`
```javascript
/**
 * Memoizes single-argument object computations without preventing garbage collection.
 */
export function memoizeWeak(fn) {
  const weakCache = new WeakMap();

  return function (obj) {
    if (obj === null || typeof obj !== 'object') {
      throw new TypeError('Weak memoize key must be an object');
    }

    if (weakCache.has(obj)) {
      return weakCache.get(obj);
    }

    const result = fn.call(this, obj);
    weakCache.set(obj, result);
    return result;
  };
}

// DOM Node bounding box layout calculator
export const getElementMetrics = memoizeWeak((domElement) => {
  console.log('Calculating bounding rect for DOM element...');
  return domElement.getBoundingClientRect();
});
// When domElement is removed from DOM and unreferenced, its cache entry is automatically GC'd!
```

### Example 4: Expensive Markdown / AST Parsing Cache
```javascript
import { memoize } from './memoize.js';
import crypto from 'node:crypto';

function hashString(content) {
  return crypto.createHash('sha256').update(content).digest('hex');
}

export const compileMarkdown = memoize(
  (markdownText) => {
    console.log('Parsing complex Markdown AST...');
    // Expensive lexical analysis & token parsing
    return {
      tokens: markdownText.split('\n').map((line) => line.trim()),
      compiledAt: new Date().toISOString(),
    };
  },
  {
    maxSize: 50,
    resolver: (content) => hashString(content), // Use SHA-256 hash as fixed-size key
  }
);
```

### Example 5: Multi-Argument Reselect Selector Pattern
```javascript
export function createSelector(selectors, combiner) {
  let lastInputs = null;
  let lastResult = null;

  return function (state) {
    const nextInputs = selectors.map((sel) => sel(state));

    const inputsChanged =
      lastInputs === null ||
      nextInputs.length !== lastInputs.length ||
      nextInputs.some((val, idx) => val !== lastInputs[idx]);

    if (inputsChanged) {
      lastResult = combiner(...nextInputs);
      lastInputs = nextInputs;
    }

    return lastResult;
  };
}

// Usage in state management
const selectItems = (state) => state.cart.items;
const selectTaxRate = (state) => state.settings.taxRate;

const selectCartTotal = createSelector(
  [selectItems, selectTaxRate],
  (items, taxRate) => {
    console.log('Computing cart total...');
    const subtotal = items.reduce((sum, item) => sum + item.price * item.qty, 0);
    return subtotal * (1 + taxRate);
  }
);
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Serializing Complex Arguments (`JSON.stringify`)**:
   Default serializers like `JSON.stringify(args)` fail silently when arguments contain:
   - Functions or symbols (which serialize to `undefined` or drop entirely).
   - Circular object references (which throw `TypeError: Converting circular structure to JSON`).
   - Objects with different key insertion orders (`{ a: 1, b: 2 }` vs `{ b: 2, a: 1 }` produce different string keys even though the objects are semantically identical).
2. **Caching Async Promise Rejections**:
   If an async function throws a network error and its rejected promise is cached, all subsequent calls will receive the cached rejection indefinitely, even after the remote service recovers. Rejected promises **must be deleted from the cache immediately**.
3. **Memoizing Impure Functions**:
   Wrapping a function that depends on external state (e.g., `getCurrentUser()`, `Date.now()`, or DB queries without versioning) causes stale data to be served indefinitely.
4. **Memory Leaks from Unbounded Maps**:
   Never use `new Map()` as a cache without setting a `maxSize` limit or using a `WeakMap`. In server-side Node.js applications, an unbounded cache will steadily consume RAM until the process crashes with `FATAL ERROR: Ineffective mark-compacts near heap limit Allocation failed - JavaScript heap out of memory`.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Operational Risk | Production Fix |
| :--- | :--- | :--- |
| Unbounded `new Map()` cache | Uncontrolled heap growth leading to OOM crash. | Enforce LRU eviction with a strict `maxSize` ceiling. |
| Naive `JSON.stringify` on circular objects | Throws runtime exception and crashes request pipeline. | Use a safe serializer or custom deterministic key generator. |
| Retaining rejected Promises in cache | Permanent cache poisoning after a single network blip. | Catch rejection and call `cache.delete(key)` immediately. |
| Memoizing mutating functions | Suppresses necessary side effects on subsequent calls. | Memoize only pure functions with no side effects. |
| Memory pinning via strong object keys | Prevents garbage collector from freeing unreferenced objects. | Use `WeakMap` when caching values derived from objects. |
| Key collisions on primitive coercion | Coercing arguments via `args.join(',')` causes `[1, 2]` and `['1,2']` to collide. | Use structured serialization or composite `Map` trees. |

---

## 8. Performance Characteristics

- **Lookup vs. Compute Trade-off**: A `Map.get()` lookup takes approximately $10\text{ns}$ to $30\text{ns}$ in V8. If the function being memoized takes less than $50\text{ns}$ (e.g. basic arithmetic), memoization actually *degrades* performance due to key hashing and serialization overhead. Only memoize functions where computation cost significantly exceeds cache lookup overhead.
- **LRU Map Efficiency**: Using `map.delete(key)` and `map.set(key, val)` provides $O(1)$ amortized time complexity for cache updates and reordering in V8.
- **Garbage Collection Optimization**: WeakMaps do not prevent their keys from being collected. When a key object has no other references, V8 reclaims both the key and value in the next minor/major GC cycle without requiring manual cache invalidation.

---

## 9. Engine Internals

### V8 Map Hash Tables & Ephemerons
- **Ordered Hash Tables**: V8 implements `Map` as an OrderedHashTable with dynamic capacity doubling. Keys and values are stored in a contiguous array of bucket elements. Deleting an element writes a "hole" (tombstone) rather than shifting elements, maintaining $O(1)$ performance. When tombstones exceed 50% of capacity, V8 compacts the table during the next insertion.
- **WeakMap Ephemeron Tables**: V8 implements `WeakMap` using **Ephemeron** pairs. The garbage collector treats the value as reachable *only if* the key is reachable. If the key is unreachable anywhere else on the heap, the entire ephemeron entry is discarded during mark-and-sweep GC.

---

## 10. Best Practices & Design Guidelines

1. **Benchmark Before Memoizing**: Profile with `console.time` or `performance.now()`. Ensure computation latency is $>1\text{ms}$ or invoked millions of times before adding memoization.
2. **Always Cap Cache Size**: Use `maxSize` (typically 50–500 entries) to prevent memory leaks.
3. **Isolate Async Failures**: Always remove failed promises from the cache in `.catch()` blocks.
4. **Use Shallow Equality for Selectors**: In UI selector libraries, store only the last input arguments and compare using `===` (referential equality) rather than deep stringification.

---

## 11. Real-World Case Study

### High-Throughput E-Commerce Pricing Engine
**Problem**: An e-commerce backend calculated dynamic cart pricing based on promotional coupon rules, volume tier discounts, and regional tax matrices. During Black Friday sales, the pricing microservice pegged CPU at 98% with an average response time of $450\text{ms}$, resulting in checkout timeouts.

**Analysis**: Profiling revealed that 78% of incoming pricing calculations during the flash sale involved identical cart configurations and promotion codes across thousands of concurrent shoppers.

**Solution**:
- Wrapped the core `calculateCartDiscounts(cartItems, promoCode)` function with an LRU memoizer:
  - `maxSize: 5,000` entries.
  - `ttlMs: 30,000` (30 seconds) to ensure promotional pricing updates propagate promptly.
  - Serializer: generated a deterministic composite hash of sorted item IDs and promo strings.
- Applied in-flight async deduplication to prevent cache stampedes on popular bundles.

**Results**:
- Cache hit ratio reached 82%.
- Average response time dropped from $450\text{ms}$ down to $6\text{ms}$.
- CPU utilization stabilized at 28% under 5x higher peak traffic.

---

## 12. Interview Questions

<details>
<summary>1. What are the essential requirements for a function to be safely memoized?</summary>

The function must be **pure**:
1. It must be **deterministic**: identical inputs must always produce the identical output.
2. It must have **no observable side effects**: it cannot mutate global state, write to files, modify external objects, or emit network traffic that other systems depend on.
3. It must not depend on external mutable state (e.g. global variables, current timestamps, random number generators, or database records that change over time).
</details>

<details>
<summary>2. How can JavaScript's standard <code>Map</code> be used to implement an LRU (Least Recently Used) cache?</summary>

JavaScript's `Map` preserves insertion order of keys. When an existing key is accessed, we call `map.delete(key)` and then immediately `map.set(key, value)`. This moves the accessed key to the end of the insertion order (marking it most recently used). When `map.size` exceeds capacity, we evict the oldest key using `const oldest = map.keys().next().value; map.delete(oldest);`.
</details>

<details>
<summary>3. Why is <code>WeakMap</code> uniquely suited for caching object-related computations?</summary>

A standard `Map` holds strong references to its keys, meaning an object used as a key cannot be garbage-collected as long as the map exists, leading to memory leaks. A `WeakMap` holds *weak references* to its keys. When an object key is dereferenced by the rest of the application, V8 automatically garbage-collects the object and removes its entry from the WeakMap without requiring manual cache eviction.
</details>

<details>
<summary>4. What critical bug occurs if an asynchronous memoizer caches rejected Promises?</summary>

If an async function fails (e.g., transient network timeout or database hiccup) and the rejected promise is stored in the cache, every subsequent call with the same arguments will immediately return the cached rejection without ever retrying the operation. The cache becomes poisoned. The fix is to attach a `.catch()` handler to the promise that deletes the key from the cache immediately upon rejection.
</details>

<details>
<summary>5. What are the performance hazards of using <code>JSON.stringify</code> as a memoization key resolver?</summary>

1. `JSON.stringify` is computationally expensive ($O(N)$ string allocation). If the arguments object is large, serialization can take longer than the function execution itself.
2. It fails on circular references (`TypeError: Converting circular structure to JSON`).
3. It ignores non-JSON types (functions, symbols, `undefined`).
4. Object key ordering differences (`{ a: 1, b: 2 }` vs `{ b: 2, a: 1 }`) produce different string hashes despite representing identical object data.
</details>

<details>
<summary>6. When should you NOT memoize a function?</summary>

1. When the function is computationally trivial (e.g., basic additions, property lookups, boolean checks), because cache lookup overhead exceeds computation time.
2. When function inputs have extremely high cardinality / randomness (every call has unique arguments), resulting in a 0% cache hit rate while wasting memory.
3. When the function has side effects (logging, DB writes, DOM mutations).
4. When memory is constrained and the cache cannot be bounded.
</details>

<details>
<summary>7. What is the "SingleFlight" (in-flight deduplication) pattern in async memoization?</summary>

The SingleFlight pattern caches the **pending Promise** rather than waiting for the resolved value. If multiple callers invoke an async function with the same parameters simultaneously before the first request finishes, all callers receive references to the same in-flight Promise. This collapses concurrent duplicate requests into a single network call, preventing "cache stampedes" or thundering herds against backend APIs.
</details>

<details>
<summary>8. How do you handle cache invalidation in memoized functions?</summary>

Production memoizers expose lifecycle controls:
1. `.clear()` to purge the entire cache.
2. `.delete(...args)` to selectively invalidate entries by key.
3. TTL (time-to-live) timestamps attached to cache entries to automatically expire stale data after a duration.
4. Version tags or cache-busting arguments passed into the resolver.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Build a Basic Single-Argument Memoizer
**Specification**: Implement `memoizeOne(fn)` that caches results for single-argument functions using a `Map` without serialization.

<details>
<summary>Solution</summary>

```javascript
export function memoizeOne(fn) {
  const cache = new Map();

  return function (arg) {
    if (cache.has(arg)) {
      return cache.get(arg);
    }
    const result = fn.call(this, arg);
    cache.set(arg, result);
    return result;
  };
}
```
</details>

### Exercise 2: Implement an LRU Cache with Max Capacity
**Specification**: Implement a class `LRUCache` with `get(key)` and `set(key, value)` methods adhering to $O(1)$ time complexity and a strict capacity limit.

<details>
<summary>Solution</summary>

```javascript
export class LRUCache {
  constructor(capacity) {
    if (capacity <= 0) throw new Error('Capacity must be greater than zero');
    this.capacity = capacity;
    this.cache = new Map();
  }

  get(key) {
    if (!this.cache.has(key)) return undefined;

    // Refresh LRU position by re-inserting
    const val = this.cache.get(key);
    this.cache.delete(key);
    this.cache.set(key, val);
    return val;
  }

  set(key, value) {
    if (this.cache.has(key)) {
      this.cache.delete(key);
    } else if (this.cache.size >= this.capacity) {
      // Evict least recently used (first item in Map iterator)
      const oldestKey = this.cache.keys().next().value;
      this.cache.delete(oldestKey);
    }

    this.cache.set(key, value);
  }

  has(key) {
    return this.cache.has(key);
  }

  clear() {
    this.cache.clear();
  }
}
```
</details>
