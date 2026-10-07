# Network Performance

Even a lean app waits on the network: HTML, JavaScript, CSS, fonts, images, and API calls all cross the wire. Network performance is about three things: **fewer requests**, **smaller requests**, and **starting the important ones earlier**. It also includes making slow things *feel* fast.

(For JavaScript size specifically see [05](./05-bundle-optimization.md); for avoiding data-fetching chains see [waterfalls](../12-server-state/01-fetching-data.md#waterfalls).)

## Know where the time goes

Open the Network panel ([profiling](./00-profiling-and-measuring.md#network-panel)) with caching disabled and throttling on. For each important request, DevTools breaks the time into phases:

```text
Queueing → DNS → Connect (TCP/TLS) → Request sent → Waiting (TTFB) → Content download
```

| Slow phase | Typical cause | Fix |
|---|---|---|
| **Waiting (TTFB)** | Slow server/API, no CDN, cold start | Cache, CDN, faster backend |
| **Content download** | Big payload, no compression | Compress, shrink, paginate |
| **Connect** | New connection to a new origin | `preconnect`, fewer origins, HTTP/2/3 |
| **Queueing/stalled** | Too many parallel requests (HTTP/1.1), priorities | HTTP/2, fewer requests |

Also look at the **shape** of the waterfall. A staircase means requests that wait on each other.

## HTTP caching

The fastest request is the one never made. Caching is controlled by response headers.

### Static assets: cache forever

Vite emits files with **content hashes** (`index-a1b2c3.js`). If the content changes, the name changes, so old URLs can be cached "forever":

```http
Cache-Control: public, max-age=31536000, immutable
```

### `index.html`: never cache it for long

`index.html` references the hashed files, so users must always get the latest to discover new ones:

```http
Cache-Control: no-cache
```

(`no-cache` means "revalidate before using", not "don't store". With an `ETag` that's a cheap `304 Not Modified`.)

This split, **immutable hashed assets + revalidated HTML**, is the standard SPA setup. Configure it on your host/CDN ([deployment](../19-production/02-deployment.md)). Mis-set caching is a classic cause of "users see the old version for days".

### API responses

For GET endpoints, HTTP caching still applies:

- `Cache-Control: max-age=60` for data that can be a minute stale.
- `ETag` / `Last-Modified` with conditional requests: the server answers `304` with no body when nothing changed.
- `stale-while-revalidate=N`: serve stale data immediately while refreshing in the background.
- Never cache user-specific or sensitive responses in shared caches (`Cache-Control: private` or `no-store`).

In-app, the [TanStack Query cache](../12-server-state/04-caching-and-synchronization.md) (`staleTime`) is usually your main client-side lever, working alongside HTTP caching.

## Compression

Text assets (HTML, JS, CSS, JSON, SVG) compress dramatically.

- **Brotli** (`br`) beats gzip by roughly 15–25% on text, and gzip is the universal fallback.
- Check the response header `content-encoding: br` (or `gzip`) in the Network panel. If it's missing, enable it at the CDN/server.
- **Don't** compress already-compressed formats (JPEG, PNG, WebP, video, woff2).
- Compress API JSON responses too. Large JSON payloads often compress 5–10×.

## HTTP/2 and HTTP/3

Modern protocols **multiplex** many requests over one connection, which removes the old HTTP/1.1 "six connections per host" bottleneck. Most CDNs enable HTTP/2/3 by default. Consequences:

- Old advice like bundling everything into one file, domain sharding, and CSS sprites is largely obsolete or harmful.
- Moderate numbers of chunks are fine, but each request still costs something, so don't create hundreds.
- Prefer **one origin** (or few) to avoid extra connection setup.

## Resource hints

Tell the browser what's coming so it can start early. Use them **sparingly**: each one competes for bandwidth and priority.

```html
<!-- Open the connection (DNS + TCP + TLS) to an origin you WILL use soon -->
<link rel="preconnect" href="https://api.example.com" crossorigin />

<!-- Cheaper: just resolve DNS for an origin you MIGHT use -->
<link rel="dns-prefetch" href="https://analytics.example.com" />

<!-- Fetch a resource needed for the CURRENT page, at high priority -->
<link rel="preload" href="/fonts/inter-latin.woff2" as="font" type="font/woff2" crossorigin />

<!-- Fetch a resource likely needed for a FUTURE navigation, at low priority -->
<link rel="prefetch" href="/assets/reports-9f8e7d.js" />

<!-- Preload an ES module and its dependencies -->
<link rel="modulepreload" href="/assets/index-a1b2c3.js" />
```

| Hint | Use for |
|---|---|
| `preconnect` | Your API origin or CDN, when requests to it start early |
| `dns-prefetch` | Third-party origins used later |
| `preload` | Critical resources discovered late (fonts, the LCP image) |
| `prefetch` | Next-page chunks/data during idle time |
| `modulepreload` | Module chunks (Vite adds these for static imports automatically) |

Pitfalls: preloading things that aren't used (the browser warns in the console), preloading too many resources (starves the important ones), and forgetting `crossorigin` on font preloads (the file gets downloaded twice).

You can set priority on individual elements, too:

```html
<img src="/hero.webp" fetchpriority="high" alt="…" />          <!-- the LCP image -->
<img src="/below-fold.webp" loading="lazy" fetchpriority="low" alt="…" />
```

## Images and fonts

Usually the heaviest bytes on the page. Covered in detail in [05 — Images](./05-bundle-optimization.md#images) and [Fonts](./05-bundle-optimization.md#fonts). The network-level essentials:

- Serve correctly sized, modern-format images from a CDN.
- `fetchpriority="high"` and **no lazy loading** for the LCP image; `loading="lazy"` for the rest.
- Always include dimensions to prevent layout shift.
- Preload at most the one or two critical fonts, and use `font-display: swap`.

## Cut the API cost

### Request less

- **Cache** with TanStack Query: sensible `staleTime`, no refetch on every mount ([04](../12-server-state/04-caching-and-synchronization.md#choosing-staletime)).
- **Deduplicate**: identical simultaneous requests share one fetch (built into the query cache).
- **Debounce** search-as-you-type so you don't fire a request per keystroke ([search state](../10-routing/06-search-filter-and-url-state.md#debounced-search-input)).
- **Cancel** outdated requests by forwarding `AbortSignal` ([TanStack Query](../12-server-state/03-tanstack-query.md#the-query-function-context)).
- Don't poll faster than the data changes. Prefer push where freshness matters ([realtime](../11-api-integration/06-realtime-communication.md)).
- Avoid fetching on every render or inside loops (the N+1 request pattern: one request per list item).

### Request smaller

- **Paginate** ([pagination](../12-server-state/07-pagination-and-infinite-queries.md)) instead of returning entire collections.
- **Select fields**: return what the screen needs (sparse fieldsets, a GraphQL query, a purpose-built endpoint). Large nested payloads that the UI barely uses waste bandwidth and parse time.
- Compress responses (above).
- Avoid sending the same large blob repeatedly. Use `ETag`/conditional requests.

### Request earlier and in parallel

- Eliminate **waterfalls**: fetch at the route level with [loaders](../10-routing/05-route-data-loading.md) or hoisted queries, not nested fetch-on-mount components.
- Start independent requests together; avoid sequential `await`s ([parallel fetching](../12-server-state/01-fetching-data.md#parallel-fetching)).
- **Prefetch** what the user is likely to need next (hover on a link, the next page of results):

```tsx
<Link to={`/projects/${id}`} onMouseEnter={() => queryClient.prefetchQuery(projectQuery(id))}>
```

- Prefer one well-designed endpoint over three chained calls when the server can combine them.

### Fewer round trips to the server

API latency is often dominated by **distance and number of round trips**. Place APIs near users (regions, edge), reuse connections, and combine calls where it makes sense.

## Perceived performance

A response that takes 600 ms can *feel* instant or slow depending on the UI.

- **Show something immediately**: [skeletons](../12-server-state/02-loading-and-error-states.md) that match the final layout, not blank screens or full-page spinners.
- **Keep old content visible** while new content loads (`keepPreviousData`, transitions) instead of flashing empty.
- **Optimistic updates** for frequent, low-risk writes ([06](../12-server-state/06-optimistic-updates.md)).
- **Instant feedback**: disable and show progress on the clicked button right away.
- **Stream** content when the server supports it (server rendering with Suspense, [SSR](../15-concurrent-and-modern-react/07-server-components-and-ssr.md)).
- **Prioritize above-the-fold** content; defer the rest.
- Avoid layout shift as things load (reserved space, image dimensions).

## Offline and flaky networks

Real users have slow, intermittent connections.

- Handle failures gracefully with retry and clear messaging ([error handling](../11-api-integration/05-api-error-handling.md)).
- Test with throttling and "Offline" in DevTools.
- A **service worker** can cache the app shell and static assets so repeat visits load without the network, and enable offline reading. It adds real complexity (cache invalidation, update flows), so adopt it deliberately, ideally via a well-maintained tool, rather than hand-rolling it.
- Don't assume `navigator.onLine` is accurate: a device can report "online" while having no actual connectivity.

## Third-party requests

Analytics, fonts from third-party CDNs, embeds, and widgets add DNS lookups, connections, and blocking scripts you don't control.

- Self-host fonts and critical assets when possible (removes a connection and a point of failure).
- Load non-essential scripts with `async`/`defer`, after interaction, or when visible.
- `preconnect` only to third-party origins you genuinely need early.
- Audit the Network panel's "Domain" column: every extra origin has a cost.

## A practical checklist

1. Static assets: hashed filenames, `immutable` long cache; `index.html` set to `no-cache`.
2. Brotli/gzip enabled for text assets and API responses; HTTP/2 or 3 on.
3. LCP image: right size and format, `fetchpriority="high"`, not lazy-loaded.
4. Fonts: subset WOFF2, `font-display: swap`, at most one or two preloaded.
5. `preconnect` to the API/CDN origin if requests start early.
6. No data waterfalls; queries and loaders start in parallel; likely-next data prefetched.
7. Search debounced; stale requests canceled; no polling faster than needed.
8. Responses paginated and trimmed to what the UI uses.
9. A good `staleTime` per query type to avoid pointless refetches.
10. Skeletons and optimistic UI for the perceived-speed wins.

## Common mistakes

- **No long-term caching for hashed assets**, or caching `index.html` for long (users stuck on old versions).
- **Uncompressed text responses** (missing `content-encoding`).
- **Waterfalls**: chained fetches, nested fetch-on-mount components, lazy chunk then data.
- **Preloading everything**, so the critical resources compete with the unimportant ones.
- **Lazy-loading the LCP image** or leaving off `width`/`height`.
- **Missing `crossorigin` on font preloads**, causing double downloads.
- **Request per keystroke** with no debounce or cancellation.
- **Over-fetching**: returning whole collections or huge nested objects for a small UI.
- **Polling too frequently**, or when push would do.
- **Many third-party origins and blocking scripts.**
- **Testing only on fast office Wi-Fi.**
- **Treating the Lighthouse network suggestions as the goal** instead of improving real user experience.

## Quick summary

- Reduce requests, shrink payloads, and **start important work earlier**, then make the remaining waiting feel fast.
- **Cache aggressively**: hashed assets `immutable` for a year, `index.html` revalidated; use `ETag`, `stale-while-revalidate`, and the TanStack Query cache for API data.
- **Compress** text (Brotli/gzip) and run HTTP/2 or 3; don't micro-bundle for HTTP/1.1.
- Use **resource hints** (`preconnect`, `preload`, `prefetch`, `fetchpriority`) sparingly and correctly, especially for the LCP image and critical fonts.
- Eliminate waterfalls, fetch in parallel, paginate and trim payloads, debounce, cancel, and prefetch on intent.
- Invest in perceived performance (skeletons, kept content, optimistic UI) and in resilience on slow networks.

## Next

Continue to [15 — Concurrent and modern React](../15-concurrent-and-modern-react/README.md).
