# Caching

A cache stores the result of expensive work (a database query, an API call, a computation) so later requests can reuse it. Done well, it cuts latency and load by orders of magnitude. Done badly, it serves stale or wrong data and is hard to debug.

[Memoization](./05_memoization.md) is the in-function special case for pure computations. This note covers caching **data that changes**, which adds the central difficulty: *when is a cached value no longer valid?*

## Prerequisites

- [Memoization](./05_memoization.md)
- [Map and Set](../09_built-in-objects/06_map-and-set.md)
- [HTTP fundamentals](../15_networking/01_http-fundamentals.md)

---

## Where Caches Live

```text
Browser  →  CDN / reverse proxy  →  App server memory  →  Shared cache (Redis)  →  Database
 HTTP cache     edge caching          in-process Map/LRU      across instances       source of truth
```

| Layer | Pros | Cons |
|---|---|---|
| Browser HTTP cache | Free, no request at all | Hard to force-invalidate once served |
| CDN | Offloads traffic globally | Invalidation delay; don't cache per-user data in shared caches |
| **In-process memory** | Fastest; no network | Per-instance; lost on restart; duplicated across servers; consumes your heap |
| **Shared cache (Redis, etc.)** | One copy for all instances; survives app restarts | Network hop; extra infrastructure |

Rule: put the cache as close to the consumer as correctness allows, and make sure each layer's invalidation story is known.

---

## Core Strategies

### Cache-aside (lazy loading), the default

The application checks the cache first, falls back to the source on a miss, and fills the cache.

```js
async function getUser(id) {
  const cached = cache.get(`user:${id}`);
  if (cached !== undefined) return cached;           // hit

  const user = await db.users.findById(id);          // miss → load
  cache.set(`user:${id}`, user, { ttl: 60_000 });
  return user;
}

async function updateUser(id, patch) {
  await db.users.update(id, patch);
  cache.delete(`user:${id}`);                        // invalidate on write
}
```

| Strategy | How writes work | Trade-off |
|---|---|---|
| **Cache-aside** | Write to DB, delete/refresh cache entry | Simple; brief staleness window possible |
| **Write-through** | Write to cache and DB together | Cache always fresh; slower writes |
| **Write-behind** | Write to cache, flush to DB later | Fast writes; risk of data loss on crash |
| **Read-through** | Cache layer loads data itself on miss | Cleaner call sites; needs cache library support |

---

## An In-Memory TTL + LRU Cache

Two ideas make a cache safe to run in a long-lived process: **TTL** (entries expire) and a **size limit** with LRU eviction (least recently used goes first).

```js
class Cache {
  #map = new Map();   // key → { value, expires }
  #max;
  #ttl;

  constructor({ max = 500, ttl = 60_000 } = {}) {
    this.#max = max;
    this.#ttl = ttl;
  }

  get(key) {
    const entry = this.#map.get(key);
    if (!entry) return undefined;
    if (entry.expires <= Date.now()) {            // expired → treat as miss
      this.#map.delete(key);
      return undefined;
    }
    this.#map.delete(key);                        // refresh recency: re-insert at the end
    this.#map.set(key, entry);
    return entry.value;
  }

  set(key, value, { ttl = this.#ttl } = {}) {
    this.#map.delete(key);
    this.#map.set(key, { value, expires: Date.now() + ttl });
    if (this.#map.size > this.#max) {
      this.#map.delete(this.#map.keys().next().value);   // evict least recently used
    }
  }

  delete(key) { this.#map.delete(key); }
  clear() { this.#map.clear(); }
  get size() { return this.#map.size; }
}
```

Why `Map` works: it iterates in **insertion order**, so deleting and re-inserting on every read keeps the *least recently used* key first. In production, a maintained library (e.g. `lru-cache`) is usually better than hand-rolled code, since it handles timers, stats, and edge cases.

Expired entries here are removed **lazily** (on access), so entries nobody reads again linger until evicted by size. That's fine with a bounded `max`.

---

## Cache Stampede (Thundering Herd)

When a popular entry expires, many concurrent requests all miss and hit the database at once. Fix it by sharing the in-flight load:

```js
const inflight = new Map();

async function getUserSafe(id) {
  const key = `user:${id}`;
  const hit = cache.get(key);
  if (hit !== undefined) return hit;

  if (!inflight.has(key)) {
    const promise = db.users.findById(id)
      .then((user) => { cache.set(key, user); return user; })
      .finally(() => inflight.delete(key));
    inflight.set(key, promise);
  }
  return inflight.get(key);
}
```

The same idea appears in [`memoizeAsync`](./05_memoization.md). Other mitigations: add random jitter to TTLs so keys don't all expire together, or refresh popular entries in the background.

---

## Stale-While-Revalidate

Serve the cached (possibly stale) value immediately and refresh it in the background. Users get fast responses; data converges shortly after. It's a good fit for data where slight staleness is acceptable (feeds, dashboards, config).

HTTP supports this directly via `Cache-Control: max-age=60, stale-while-revalidate=300` (honored by browsers/CDNs that implement it).

---

## HTTP Caching Essentials

You get a lot of caching for free by sending the right headers.

```http
Cache-Control: public, max-age=31536000, immutable     # fingerprinted assets (app.3f9a2c.js)
Cache-Control: no-cache                                # may store, but must revalidate before use
Cache-Control: no-store                                # never store (sensitive responses)
Cache-Control: private, max-age=60                     # browser only, not shared caches
ETag: "abc123"                                         # validator for conditional requests
```

Conditional requests save bandwidth: the client sends `If-None-Match: "abc123"`, and the server replies `304 Not Modified` with no body if unchanged.

Common confusion: **`no-cache` does not mean "don't cache"**; it means "revalidate before reuse." Use `no-store` to forbid storing.

For static assets, use **content-hashed filenames** and long `max-age`, so you never need to invalidate: new content gets a new URL.

---

## Designing Cache Keys

- Include **everything that affects the result**: user/tenant, locale, query params, API version, feature flags.
- Don't cache **per-user data under a shared key** and certainly not in a shared CDN/proxy cache (`Cache-Control: private` or `no-store`). Leaking one user's response to another is a security bug ([Security checklist](../22_security/06_security-checklist.md)).
- Namespace keys (`user:42`, `product:99:v2`) and version them so a data-shape change doesn't read old-shaped entries.

---

## Invalidation Strategies

| Strategy | Use when |
|---|---|
| **TTL only** | Slight staleness is acceptable; simplest and most robust |
| **Explicit delete on write** | You control all write paths |
| **Versioned keys** (`user:42:v7`) | Bulk invalidation by bumping a version |
| **Event-driven** (publish "user changed" → evict) | Multiple services or instances share data |

Whatever you choose, accept that you're trading freshness for speed, and decide **how stale is acceptable** for each kind of data. TTLs are the safety net even when you also invalidate explicitly.

---

## Common Mistakes

| Mistake | Fix |
|---|---|
| Unbounded in-memory cache | Max size + TTL |
| Caching `null`/errors accidentally (negative caching without intent) | Cache failures only deliberately, with short TTLs |
| Treating `undefined` as a miss when `undefined`/`null` can be a valid result | Use a sentinel or `Map.has` |
| Forgetting multiple server instances → inconsistent in-process caches | Use a shared cache or short TTLs |
| Stampede on expiry | Request coalescing, TTL jitter |
| Cache key missing a varying dimension (locale, user) | Include all inputs |
| Returning cached **mutable** objects that callers mutate | Return copies/frozen data |
| Caching sensitive responses in shared/browser caches | `private` / `no-store` |
| Assuming the cache is always warm | Code must work correctly on a miss |

---

## Debugging

- Add **hit/miss counters** and log them; a low hit rate means the cache isn't earning its keep (keys too varied, TTL too short).
- For "stale data" bugs, find *every layer*: browser, CDN, app, Redis. Check response headers (`Age`, `Cache-Control`, `ETag`, CDN-specific headers) in DevTools → Network.
- To bypass the browser cache while debugging, use DevTools "Disable cache" (while open) or a hard reload.
- Reproduce with a cold cache and a warm one; behavior differing between them points to a keying or invalidation bug.

---

## Testing

```js
import { vi, it, expect, beforeEach, afterEach } from 'vitest';

beforeEach(() => vi.useFakeTimers());
afterEach(() => vi.useRealTimers());

it('expires entries after the TTL', () => {
  const c = new Cache({ ttl: 1000 });
  c.set('a', 1);
  vi.advanceTimersByTime(999);
  expect(c.get('a')).toBe(1);
  vi.advanceTimersByTime(2);
  expect(c.get('a')).toBeUndefined();
});

it('evicts the least recently used entry', () => {
  const c = new Cache({ max: 2 });
  c.set('a', 1); c.set('b', 2);
  c.get('a');            // 'a' is now most recent
  c.set('c', 3);         // evicts 'b'
  expect(c.get('b')).toBeUndefined();
  expect(c.get('a')).toBe(1);
});
```

See [Mocking](../21_testing/05_mocking.md) for fake timers.

---

## Quick Summary

- Caching trades **freshness for speed**; the hard problem is invalidation.
- Default to **cache-aside** with a **TTL** and a **size limit (LRU)**.
- Prevent stampedes by coalescing in-flight loads; consider stale-while-revalidate.
- Use HTTP caching headers well: hashed assets with long `max-age`, `no-store` for sensitive data; `no-cache` ≠ no caching.
- Key on every input that changes the result; never share per-user data across users.
- Measure hit rate, and make sure your code works on a cold cache.

**Next:** [Rate Limiting](./09_rate-limiting.md)
