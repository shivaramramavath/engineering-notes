# Networking Patterns

Reusable designs that turn raw `fetch` calls into **reliable, maintainable** network code. Most examples build on the helpers from the async chapter (`sleep`, `retry`, `mapLimit`, `dedupe`).

## 1. A central API client

One module that owns base URL, headers, JSON, errors, timeouts and auth.

```js
// api-client.js
export class ApiError extends Error {
  constructor(status, problem, response) {
    super(problem?.title ?? problem?.message ?? `HTTP ${status}`);
    this.name = "ApiError";
    this.status = status;
    this.problem = problem;
    this.requestId = response?.headers.get("x-request-id");
  }
  get retryable() { return this.status === 429 || this.status >= 500; }
}

export function createClient({ baseUrl, getToken, onUnauthorized, timeoutMs = 10_000 }) {
  async function request(path, { method = "GET", query, json, headers, signal, retries = method === "GET" ? 2 : 0 } = {}) {
    const url = new URL(path, baseUrl);
    for (const [k, v] of Object.entries(query ?? {})) if (v !== undefined && v !== null) url.searchParams.set(k, String(v));

    const attempt = async () => {
      const token = await getToken?.();
      const res = await fetch(url, {
        method,
        headers: {
          Accept: "application/json",
          ...(json !== undefined && { "Content-Type": "application/json" }),
          ...(token && { Authorization: `Bearer ${token}` }),
          ...headers,
        },
        body: json !== undefined ? JSON.stringify(json) : undefined,
        signal: AbortSignal.any([...(signal ? [signal] : []), AbortSignal.timeout(timeoutMs)]),
      });
      const payload = res.status === 204 ? null : await res.json().catch(() => null);
      if (!res.ok) throw new ApiError(res.status, payload, res);
      return payload;
    };

    return withRetry(attempt, { retries, shouldRetry: (e) => e instanceof ApiError ? e.retryable : e.name === "TypeError" });
  }

  return {
    get: (path, opts) => request(path, { ...opts, method: "GET" }),
    post: (path, json, opts) => request(path, { ...opts, method: "POST", json }),
    put: (path, json, opts) => request(path, { ...opts, method: "PUT", json }),
    patch: (path, json, opts) => request(path, { ...opts, method: "PATCH", json }),
    delete: (path, opts) => request(path, { ...opts, method: "DELETE" }),
  };
}
```

Benefits: one place to add logging, tracing headers, metrics, auth and error mapping.

## 2. Retry with exponential backoff and jitter

```js
const sleep = (ms, signal) => new Promise((resolve, reject) => {
  const id = setTimeout(resolve, ms);
  signal?.addEventListener("abort", () => { clearTimeout(id); reject(signal.reason); }, { once: true });
});

async function withRetry(fn, { retries = 3, baseMs = 300, maxMs = 8000, shouldRetry = () => true, signal } = {}) {
  for (let attempt = 0; ; attempt++) {
    try { return await fn(attempt); }
    catch (err) {
      if (attempt >= retries || signal?.aborted || !shouldRetry(err)) throw err;
      const retryAfter = Number(err?.retryAfterSeconds) * 1000;                          // from the Retry-After header
      const backoff = Math.random() * Math.min(maxMs, baseMs * 2 ** attempt);            // full jitter
      await sleep(Math.max(backoff, retryAfter || 0), signal);
    }
  }
}
```

| Retry | Do not retry |
|-------|--------------|
| network errors (`TypeError`), `408`, `429`, `502`, `503`, `504` | `400`, `401` (refresh instead), `403`, `404`, `409`, `422` |
| idempotent methods (`GET`, `PUT`, `DELETE`) | `POST` **unless** protected by an idempotency key |

Honor `Retry-After` (seconds or HTTP date). Cap total attempts and total time.

## 3. Idempotency keys for safe `POST` retries

```js
const key = crypto.randomUUID();                       // generate ONCE per user action, reuse across retries
await withRetry(() => api.post("/payments", { amount }, { headers: { "Idempotency-Key": key } }), { retries: 3 });
```

The server stores the result per key and returns the same response on duplicates.

## 4. Timeouts and cancellation

```js
const controller = new AbortController();
const res = await fetch(url, { signal: AbortSignal.any([controller.signal, AbortSignal.timeout(5000)]) });
```

- Per-request timeouts plus an **overall** deadline for multi-step operations
- Cancel stale requests (search-as-you-type), on navigation/unmount, and when a user clicks "Cancel"
- Treat `AbortError` as expected, not as a failure to report

## 5. Token refresh with single-flight

When many requests hit `401` at once, refresh **once** and replay them.

```js
let refreshing = null;

async function refreshToken() {
  refreshing ??= fetch("/auth/refresh", { method: "POST", credentials: "include" })
    .then((r) => { if (!r.ok) throw new Error("refresh failed"); return r.json(); })
    .then(({ accessToken }) => (setToken(accessToken), accessToken))
    .finally(() => { refreshing = null; });
  return refreshing;
}

async function authFetch(url, options = {}) {
  let res = await fetch(url, withAuth(options));
  if (res.status !== 401) return res;
  try { await refreshToken(); } catch { redirectToLogin(); throw new Error("Session expired"); }
  res = await fetch(url, withAuth(options));                                // replay once
  return res;
}
```

Keep access tokens short-lived and in memory; keep the refresh token in an `HttpOnly` cookie.

## 6. Deduplicate and cache requests

```js
const inflight = new Map();
const cache = new Map();

async function cachedGet(url, { ttlMs = 30_000 } = {}) {
  const hit = cache.get(url);
  if (hit && hit.expires > Date.now()) return hit.value;

  if (inflight.has(url)) return inflight.get(url);                        // share one request
  const p = api.get(url)
    .then((value) => { cache.set(url, { value, expires: Date.now() + ttlMs }); return value; })
    .finally(() => inflight.delete(url));
  inflight.set(url, p);
  return p;
}
```

Strategies:

| Strategy | Meaning |
|----------|---------|
| Cache-first | use the cache, update only when expired |
| Network-first | try the network, fall back to cache when offline |
| **Stale-while-revalidate** | show cached data immediately, refresh in the background |
| HTTP caching | rely on `Cache-Control`/`ETag`: the browser does it for you |
| Data-fetching libraries | **TanStack Query**, **SWR**, **RTK Query**, **Apollo**: caching, dedupe, retries, refetch on focus, pagination |

Prefer a data-fetching library for UI-heavy apps instead of hand-rolling every feature.

## 7. Pagination and infinite loading

```js
async function* paginate(path, { limit = 50 } = {}) {
  let cursor;
  do {
    const page = await api.get(path, { query: { limit, cursor } });
    yield* page.items;
    cursor = page.nextCursor;
  } while (cursor);
}

for await (const item of paginate("/orders")) process(item);
```

For infinite scroll use an `IntersectionObserver` sentinel to trigger the next page load, abort stale loads, and prevent duplicate loads with an `isLoading` flag.

## 8. Batching and concurrency limits

```js
// at most 5 requests at a time
const results = await mapLimit(ids, 5, (id) => api.get(`/users/${id}`));

// coalesce many lookups into one request (DataLoader-style)
const loadUser = createBatcher((ids) => api.get("/users", { query: { ids: ids.join(",") } }).then((users) => ids.map((id) => users.find((u) => u.id === id))));
```

Respect rate limits: parse `X-RateLimit-*` / `Retry-After`, queue requests, and back off on `429`.

## 9. Optimistic updates

Update the UI immediately, then reconcile with the server.

```js
async function toggleLike(post) {
  const previous = post.liked;
  post.liked = !previous;                                   // optimistic
  render(post);
  try {
    await api.put(`/posts/${post.id}/like`, { liked: post.liked });
  } catch (err) {
    post.liked = previous;                                  // roll back
    render(post);
    showToast("Could not update. Please try again.");
  }
}
```

Combine with request ordering: ignore responses from older requests (version numbers or abort).

## 10. Polling the right way

```js
async function poll(fn, { intervalMs = 2000, until = Boolean, signal, maxMs = 60_000 }) {
  const deadline = Date.now() + maxMs;
  while (Date.now() < deadline) {
    const result = await fn({ signal });
    if (until(result)) return result;
    await sleep(intervalMs, signal);                        // sequential: never overlaps
  }
  throw new Error("Timed out waiting");
}
```

Improve efficiency with `ETag`/`304`, slow down on hidden tabs (`document.hidden`), back off over time, or switch to SSE/WebSocket.

## 11. Large uploads and downloads

| Task | Approach |
|------|----------|
| Big file upload | **chunked/resumable** uploads (`tus` protocol, S3 multipart, `Blob.slice`), retry chunks independently |
| Direct-to-storage | request a **pre-signed URL** from your API, then `PUT` the file to storage (keeps load off your server) |
| Progress | XHR `upload.onprogress` (fetch lacks upload progress) |
| Large download | stream with `res.body`, write via File System Access API/OPFS; `Range` requests for resuming |
| Image/media | CDN, responsive sizes, `Cache-Control: immutable` |

```js
async function uploadInChunks(file, url, chunkSize = 5 * 1024 * 1024) {
  for (let start = 0; start < file.size; start += chunkSize) {
    const chunk = file.slice(start, start + chunkSize);
    await withRetry(() => fetch(url, {
      method: "PUT",
      headers: { "Content-Range": `bytes ${start}-${start + chunk.size - 1}/${file.size}` },
      body: chunk,
    }).then((r) => { if (!r.ok) throw new Error(`HTTP ${r.status}`); }), { retries: 3 });
  }
}
```

## 12. Offline and flaky networks

- Detect with `online`/`offline` events as **hints**, but rely on real request outcomes
- Queue mutations in **IndexedDB** and replay when back online (idempotency keys!)
- Use a **service worker** (Workbox) for cache strategies and **Background Sync** where supported
- Show clear states: loading, empty, error with retry, offline banner, stale-data indicator

```js
window.addEventListener("online", () => flushQueue());
```

## 13. Validating responses

Never trust network data.

```js
import { z } from "zod";

const User = z.object({ id: z.number(), name: z.string(), email: z.string().email() });

async function getUser(id) {
  const data = await api.get(`/users/${id}`);
  return User.parse(data);                 // throws a descriptive error if the shape is wrong
}
```

TypeScript types alone are erased at runtime and do not validate server data.

## 14. Error handling in the UI

| Error | UX |
|-------|----|
| Network / timeout | message with **Retry** button, keep user input |
| `401` | silent refresh, otherwise redirect to login |
| `403` | explain missing permissions |
| `404` | "not found" view |
| `409` / `412` | conflict dialog: reload or merge |
| `422` | field-level messages from the error body |
| `429` | "Too many requests, try again in N seconds" |
| `5xx` | generic apology, request id for support, automatic retry for reads |

Log errors with the **request id** and correlation headers; surface the id to users when helpful.

## 15. Observability and tracing

```js
headers["X-Request-Id"] = crypto.randomUUID();
headers.traceparent = makeTraceparent();                     // W3C trace context: link client and server spans
```

- Measure latency with `performance.getEntriesByType("resource")` or `PerformanceObserver`
- Read `Server-Timing` response headers
- Send client errors and slow request metrics to monitoring (with sampling)
- Track success rate, p95 latency, retries and cancellations

## 16. Security patterns

| Concern | Practice |
|---------|----------|
| CSRF (cookie auth) | `SameSite=Lax/Strict`, CSRF tokens (`X-CSRF-Token`), check `Origin` |
| XSS | never inject response data with `innerHTML`; CSP |
| Secrets | never ship API secrets to the browser; proxy through your backend |
| Token storage | prefer `HttpOnly` cookies; keep access tokens in memory |
| SSRF (server-side fetch of user URLs) | allowlists, block internal IPs, timeouts, size limits |
| Input | validate on the server regardless of client validation |
| HTTPS | enforce with HSTS; avoid mixed content |

## 17. Testing network code

```js
// Mock Service Worker (MSW) intercepts fetch in tests and the browser
import { http, HttpResponse } from "msw";
import { setupServer } from "msw/node";

const server = setupServer(
  http.get("https://api.example.com/users/:id", ({ params }) => HttpResponse.json({ id: Number(params.id), name: "Ada" })),
  http.post("https://api.example.com/users", () => HttpResponse.json({ error: "boom" }, { status: 500 })),
);
beforeAll(() => server.listen({ onUnhandledRequest: "error" }));
afterEach(() => server.resetHandlers());
afterAll(() => server.close());
```

Test: success, each error class, timeouts and aborts (fake timers), retries/backoff, token refresh races, and malformed payloads.

## Pattern chooser

| Problem | Pattern |
|---------|---------|
| Flaky network | retry + backoff + jitter (idempotent calls) |
| Hung requests | timeouts + cancellation |
| Duplicate `POST`s | idempotency keys, disabled UI |
| Many identical requests | dedupe + cache (or a data-fetching library) |
| Many small requests | batching |
| Overloading APIs | concurrency limits, honor `429` |
| Expired sessions | single-flight token refresh |
| Slow-feeling UI | optimistic updates, stale-while-revalidate |
| Live data | SSE/WebSocket, polling as fallback |
| Large files | chunked/resumable, pre-signed URLs |
| Untrusted data | schema validation |

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Scattered `fetch` calls with copy-pasted handling | Inconsistent errors, hard changes | Central client |
| Retrying everything | Duplicates, overload | Retry only safe cases with backoff |
| Parallel token refreshes | Race conditions, revoked tokens | Single-flight refresh |
| No timeouts | Spinners forever | Timeout every request |
| Unbounded parallelism | Rate limits, memory spikes | `mapLimit`, queues |
| Caching without invalidation | Stale or wrong data | TTLs, invalidation on mutations, SWR |
| Optimistic updates without rollback | UI lies on failure | Rollback and notify |
| Trusting response shapes | Runtime crashes | Validate with schemas |
| Overlapping polls | Piling up requests | Sequential loop with `await sleep` |
| Logging full payloads/tokens | Privacy and security leaks | Redact, log ids and metadata |
| Reinventing caching/fetching logic in big UIs | Bugs and complexity | TanStack Query / SWR |

## Key takeaways

- Centralize networking in a client that handles auth, JSON, errors, timeouts and tracing
- Retry only idempotent (or key-protected) requests, with backoff, jitter and `Retry-After`
- Deduplicate, cache, batch and limit concurrency to be kind to servers and fast for users
- Validate data, handle each error class explicitly in the UI, and test failure paths with mocks

**Next:** [Node.js](../16_nodejs/00_README.md)
