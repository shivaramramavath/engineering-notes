# Caching Overview

Caching stores the result of work (a database query, a rendered page) so later requests can reuse it instead of repeating the work. Next.js caches in several places, and the rules differ depending on which caching model your project uses. This note gives you the map; the following notes go deep on each part.

> Verified against the Next.js 16.4 docs. Check which model you use first (below). Mixing advice from the two models is the most common source of caching confusion.

## The two models

| | **Cache Components** | **Previous model** |
|---|---|---|
| Enabled by | `cacheComponents: true` in `next.config.ts` (default for new projects with recommended `create-next-app` defaults) | Not setting it |
| Default for data and UI | **Dynamic**: runs at request time | **Static if possible**: prerendered unless something forces dynamic |
| How you cache | `use cache` directive plus `cacheLife` / `cacheTag` | `fetch` options, `unstable_cache`, route segment config |
| Request-time data (`cookies()`, `headers()`, `searchParams`) | Fine, but wrap in `<Suspense>` so the rest can prerender | Makes the whole route dynamic |
| Route config (`dynamic`, `revalidate`, `fetchCache`) | Not supported; replaced by `use cache` / `cacheLife` | Supported |
| Output | Static shell + streamed dynamic parts (Partial Prerendering) | Whole route static or whole route dynamic |

```text
Previous model:    static by default  →  you opt OUT (use cookies(), force-dynamic...)
Cache Components:  dynamic by default →  you opt IN (use cache)
```

Cache Components requires the Node.js runtime and does not support static export (`output: "export"`).

## The layers

Whatever the model, caching happens in four places:

| Layer | Where | What it holds | Lifetime |
|---|---|---|---|
| **Request memoization** | Server, one render | Results of identical `fetch` calls and `React.cache` functions | One request |
| **Data / function cache** | Server | `fetch` results and `unstable_cache` (previous), or `use cache` outputs (Cache Components) | Until revalidated (see below) |
| **Prerendered output** | Server / CDN | HTML and RSC payload for static routes (Full Route Cache), or the static shell | Until revalidated or redeployed |
| **Client (router) cache** | Browser memory | RSC payloads for visited and prefetched routes | Seconds to minutes; cleared by Server Action revalidation |

```text
Browser                              Server
┌────────────────┐   request         ┌──────────────────────────────┐
│ Client cache   │ ───────────────►  │ Prerendered output (CDN/disk)│
│ (router cache) │                   │        │ miss / dynamic      │
└────────────────┘                   │        ▼                     │
                                     │ Render (Server Components)   │
                                     │   ├─ Request memoization     │
                                     │   └─ Data cache / use cache  │
                                     │        │ miss                │
                                     │        ▼                     │
                                     │ Database / API               │
                                     └──────────────────────────────┘
```

Each layer is covered in its own note: [Data Cache](./01-data-cache.md), [Full Route Cache](./02-full-route-cache.md), [Router Cache](./03-router-cache.md).

## Request memoization (both models)

Within a single render, you can call the same data function from many components without repeated work:

- Identical `fetch` calls (same URL and options) are executed once.
- For ORM/database calls, wrap the function in React's `cache()`.

```ts
import { cache } from "react";

export const getUser = cache(async (id: string) => db.user.findUnique({ where: { id } }));
```

This lasts one render only; it is not shared between requests. Inside a `use cache` scope, `React.cache` has its own isolated scope, so values do not carry in from outside.

## Deciding what to cache

Cache data that:

- does not depend on the current user or request (no `cookies()`, `headers()`, `searchParams` inside the cached work), and
- can tolerate being slightly out of date, and
- is expensive or shared by many requests.

| Data | Approach |
|---|---|
| Marketing copy, docs, CMS content | Cache long (`days`/`max`); revalidate on publish via a tag |
| Product catalog, blog list | Cache with `hours`; revalidate on change |
| Live scores, stock prices | Short-lived or uncached, streamed behind `<Suspense>` |
| Per-user data (profile, cart) | Uncached at request time (or keyed by an explicit argument) |
| Results of mutations | Revalidate immediately (`updateTag`) |

When you do not want any caching, leave the data outside a `use cache` scope (Cache Components) or opt out with `cache: "no-store"` (previous model).

## Staleness and freshness: the vocabulary

| Term | Meaning |
|---|---|
| **Stale** | Cached content is older than its freshness window |
| **Revalidate** | Recompute and replace cached content |
| **Stale-while-revalidate** | Serve the stale copy immediately while refreshing in the background |
| **Expire** | Hard limit after which the next request waits for fresh content |
| **Tag** | A label you attach to cached data so you can invalidate it on demand |

Details and the APIs are in [Revalidation](./04-revalidation.md).

## Debugging cache behavior

- **Check the model.** Is `cacheComponents` set? Advice for the other model will not apply.
- **Test production behavior.** Run `next build` then `next start`. In the previous model, pages in `next dev` are always rendered on demand and never cached, so you will not see caching there.
- **Read the build output.** The route table shows what was prerendered.
- **Log cache decisions.** Set `NEXT_PRIVATE_DEBUG_CACHE=1` for verbose cache logging in dev or production.
- **Cache Components dev overlay.** It shows validation insights (for example "blocking route") that point to uncached data or runtime APIs outside `<Suspense>`.
- **Serverless caveat.** The default in-memory cache for `use cache` may not persist between requests on serverless platforms; see [Cache Components](./05-cache-components.md).

## Misconceptions

| Belief | Reality |
|---|---|
| "Next.js caches `fetch` by default" | Not since Next.js 15; you opt in (previous model) or use `use cache` |
| "`use cache` is a global cache shared by all deployments" | Entries are scoped to one deployment; a new build starts fresh |
| "Reading `cookies()` makes the whole page dynamic" | Previous model: yes. Cache Components: only the part behind `<Suspense>` streams at request time |
| "`revalidatePath` refreshes everything that uses the data" | It targets a path; use tags to invalidate data across pages |
| "Dev behaves like production" | Caching and prerendering differ in dev; verify with a production build |
| "Caching is only on the server" | The browser also caches route payloads (router cache) |

## Quick Summary

- Two models: **Cache Components** (dynamic by default, `use cache`) and the **previous model** (static by default, `fetch`/route config). Check `next.config.ts`.
- Four layers: request memoization, data/function cache, prerendered output, client cache.
- Cache shared, tolerable-stale, expensive data; leave user-specific data uncached.
- Verify with a production build; use `NEXT_PRIVATE_DEBUG_CACHE=1` to see what is happening.

## Next

- [Data Cache](./01-data-cache.md)
- [Revalidation](./04-revalidation.md)
- [Cache Components](./05-cache-components.md)