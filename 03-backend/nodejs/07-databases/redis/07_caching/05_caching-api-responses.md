# Caching API Responses

Caching whole HTTP responses in Redis is the highest-leverage cache for read-heavy APIs. This lesson builds an Express middleware step by step, and covers the parts that go wrong in production: cache keys, per-user data, invalidation and headers.

## What to cache (and what not to)

| Cache | Don't cache |
|-------|-------------|
| Public `GET` endpoints (catalogs, articles, search results) | `POST`, `PUT`, `PATCH`, `DELETE` |
| Expensive aggregations and reports | Responses with `Set-Cookie` |
| Data identical for many users | Personalized data **under a shared key** |
| Third-party API results | Errors (`5xx`) |
| | Anything sensitive without a per-user key |

The main danger is serving **one user's data to another**. Get the cache key right before anything else.

## Cache key design

A key must include **everything that changes the response**:

| Ingredient | Why |
|------------|-----|
| HTTP method (only `GET`) | Distinct semantics |
| Path | Which resource |
| **Normalized query string** (sorted) | `?a=1&b=2` and `?b=2&a=1` should share an entry |
| Headers that vary the response (`Accept-Language`, `Accept-Encoding` if you store compressed data) | Same URL, different bodies |
| **User or tenant ID** for private responses | Never leak across users |
| API/schema version | Changing the shape |

```ts
import { createHash } from "node:crypto";
import type { Request } from "express";

function normalizedUrl(req: Request) {
  const url = new URL(req.originalUrl, "http://x");
  url.searchParams.sort();
  return url.pathname + (url.search || "");
}

export function responseKey(req: Request, opts: { vary?: string[]; userId?: string; version?: number } = {}) {
  const parts = [
    req.method,
    normalizedUrl(req),
    ...(opts.vary ?? []).map((h) => `${h}=${req.header(h) ?? ""}`),
    opts.userId ? `u=${opts.userId}` : "",
  ];
  const digest = createHash("sha1").update(parts.join("|")).digest("hex");
  return `shop:cache:http:v${opts.version ?? 1}:${digest}`;
}
```

Hashing keeps keys short and safe from odd characters. The original URL is stored in the value if you need to debug.

## The middleware

```ts
import type { RequestHandler } from "express";

interface CacheResponseOptions {
  ttl: number;                                       // seconds
  vary?: string[];                                   // request headers that change the response
  private?: boolean;                                 // true → key includes the user id
  tags?: (req: Request) => string[];                 // for tag invalidation
  cacheable?: (req: Request) => boolean;             // extra opt-out logic
}

export function cacheResponse(opts: CacheResponseOptions): RequestHandler {
  return async (req, res, next) => {
    if (req.method !== "GET") return next();
    if (opts.cacheable && !opts.cacheable(req)) return next();

    const userId = opts.private ? (req as any).user?.id : undefined;
    if (opts.private && !userId) return next();          // private without a user → never cache

    const key = responseKey(req, { vary: opts.vary, userId });

    // 1. try the cache (fail open)
    try {
      const raw = await redis.get(key);
      if (raw !== null) {
        const { status, body } = JSON.parse(raw);
        res.setHeader("X-Cache", "HIT");
        return res.status(status).json(body);
      }
    } catch (err) {
      req.log?.warn?.({ err }, "response cache read failed");
    }

    // 2. miss: capture the response on its way out
    res.setHeader("X-Cache", "MISS");
    const originalJson = res.json.bind(res);

    res.json = (body: unknown) => {
      const cacheable =
        res.statusCode === 200 &&
        !res.getHeader("Set-Cookie") &&
        !/no-store|private/i.test(String(res.getHeader("Cache-Control") ?? ""));

      if (cacheable) {
        const payload = JSON.stringify({ status: res.statusCode, body });
        const tags = opts.tags?.(req) ?? [];

        const m = redis.multi().set(key, payload, "EX", opts.ttl);
        for (const t of tags) {
          m.sadd(`shop:tag:${t}`, key).expire(`shop:tag:${t}`, opts.ttl * 2);
        }
        m.exec().catch((err) => req.log?.warn?.({ err }, "response cache write failed"));
      }
      return originalJson(body);
    };

    next();
  };
}
```

Usage:

```ts
app.get(
  "/api/products",
  cacheResponse({ ttl: 60, vary: ["Accept-Language"], tags: () => ["products"] }),
  listProducts
);

app.get(
  "/api/products/:id",
  cacheResponse({ ttl: 300, tags: (req) => ["products", `product:${req.params.id}`] }),
  getProduct
);

app.get(
  "/api/me/orders",
  authenticate,
  cacheResponse({ ttl: 30, private: true }),          // per-user key
  listMyOrders
);
```

Highlights:

| Detail | Why |
|--------|-----|
| Only `GET` and only `200` | Errors and mutations are never cached |
| Fails open | A Redis error just skips the cache |
| `X-Cache: HIT/MISS` header | Instant visibility when debugging |
| Skips responses with `Set-Cookie` or `Cache-Control: private/no-store` | Respects the handler's intent |
| `private: true` keys by user | No cross-user leaks |
| Cache write is fire-and-forget | Doesn't slow the response |

This wraps `res.json`. For handlers that use `res.send` or stream files, extend it accordingly or leave those routes uncached.

## Invalidation on writes

```ts
app.put("/api/products/:id", authenticate, async (req, res) => {
  const product = await db.products.update(req.params.id, req.body);   // commit first

  await invalidateTag(`product:${req.params.id}`);                     // then invalidate
  await invalidateTag("products");                                     // list pages, too

  res.json(product);
});

async function invalidateTag(tag: string) {
  const tagKey = `shop:tag:${tag}`;
  const keys = await redis.smembers(tagKey);
  if (keys.length) await redis.unlink(...keys, tagKey);
}
```

Tags mean write handlers don't need to know every cached URL (list pages, filtered lists, sort orders). Full discussion: [Cache Invalidation](./03_cache-invalidation.md).

For very broad or very frequent invalidation, prefer a **namespace version** in the key (`responseKey` with a `version` read from `shop:ver:products`) instead of large tag sets.

## Stampede protection

Combine the middleware with [single flight](./04_cache-problems.md#fix-a-single-flight-inside-the-process) so concurrent misses for the same URL share one handler execution. If handlers are expensive, wrap the loader in a lock or use stale-while-revalidate as shown in the previous lesson.

## HTTP caching headers (browser and CDN)

Redis is a **server-side** cache. Add headers so browsers and CDNs can help too:

```ts
res.set("Cache-Control", "public, max-age=30, stale-while-revalidate=60");
```

| Header | Meaning |
|--------|---------|
| `Cache-Control: public, max-age=N` | Shared caches (CDN) and browsers may reuse for N seconds |
| `Cache-Control: private, max-age=N` | Browser only, never shared caches |
| `Cache-Control: no-store` | Never store |
| `stale-while-revalidate=N` | Serve stale while refreshing (CDN/browser support varies) |
| `Vary: Accept-Language` | Tells caches the response depends on that header |
| `ETag` | Validator for conditional requests |

Express generates an `ETag` for `res.json` and answers conditional requests with `304 Not Modified` for you, including when the body came from Redis.

Rules:

- Personalized responses: `Cache-Control: private` (or `no-store`)
- If your Redis key varies by a header, send the matching `Vary` header
- Layers stack: browser → CDN → Redis → database. Each has its own TTL and invalidation story, so keep the browser and CDN TTLs **shorter** than what you can tolerate being stale (you can't reach into browsers to invalidate)

## Cache warming

Prime the cache for the pages you know will be hot:

```ts
async function warm() {
  for (const path of ["/api/products?sort=popular", "/api/categories"]) {
    await fetch(`http://localhost:3000${path}`).catch(() => {});
  }
}
```

Run it after deploys, or from a scheduled job. Warming pairs well with TTL jitter (see [Cache Problems](./04_cache-problems.md)).

## Observability

Track, per route:

- Hit ratio (from `X-Cache` or counters)
- Latency by `HIT` vs `MISS`
- Cache size and eviction counts (`INFO memory`, `evicted_keys`)
- Invalidation counts by tag

```ts
redis.hincrby("shop:metrics:http-cache", `${route}:${hit ? "hit" : "miss"}`, 1).catch(() => {});
```

Log the key **digest**, not raw URLs with tokens or personal data.

## Security checklist

- [ ] Private responses use `private: true` (per-user key) and are skipped when there's no user
- [ ] Responses that set cookies aren't cached
- [ ] `Authorization` and other secrets never appear in keys or values
- [ ] Vary headers used in the key match the `Vary` response header
- [ ] Tenant ID is part of the key in multi-tenant apps
- [ ] Cached bodies don't contain per-request data (CSRF tokens, timestamps you rely on)
- [ ] TTLs reflect the sensitivity of the data

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| Same key for different users | `private: true`, or include the user or tenant in the key |
| Query params in different orders creating duplicates | Sort the query string |
| Caching `4xx`/`5xx` responses for long | Only `200`, or a very short negative TTL |
| Cache key ignores `Accept-Language` | Add it to `vary` |
| Invalidating before the write commits | Commit first |
| Huge response bodies | Paginate, compress, or don't cache |
| No fail-open behavior | `try/catch` on reads and writes |
| Relying on browsers to see invalidations | Short browser TTLs, ETags |
| Middleware placed **before** authentication | Place it **after** so `req.user` exists |

## Key takeaways

- The **cache key** is the whole game: method, normalized URL, varying headers, user or tenant
- Cache only successful `GET` responses, and never share personalized data
- Invalidate by tag after the write commits, with TTLs as the backstop
- Combine Redis with HTTP headers, single flight and metrics

**Next module:** [08_nodejs-integration](../08_nodejs-integration/README.md)
