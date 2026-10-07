# Full Route Cache and the Static Shell

This note is about **prerendered output**: the HTML and RSC payload Next.js saves so it does not have to render a route on every request. In the previous model this is the **Full Route Cache**. With Cache Components it becomes the **static shell** (Partial Prerendering). Both are about the same goal, serving a ready-made page instantly, but they differ in granularity.

> Verified against the Next.js 16.4 docs. Check `cacheComponents` in your config to see which section applies.

## Previous model: whole-route, all or nothing

At build time Next.js renders each static route once and stores:

- **HTML** for the first page load
- the **RSC payload** for client-side navigation

Requests then get this stored output, often straight from a CDN, with no server rendering.

```text
next build ──► render static routes ──► store HTML + RSC payload
request     ──► serve stored output (no rendering)
```

### What makes a route dynamic (and skips this cache)

Any one of these makes the **entire route** render per request:

- A request-time API: `cookies()`, `headers()`, `searchParams`, `connection()`, draft mode
- Uncached data fetching reached in a way that opts out (for example `cache: "no-store"`)
- `export const dynamic = "force-dynamic"` or `export const revalidate = 0`

### Route segment config (previous model only)

These exports from a page, layout or route handler steer the behavior:

```tsx
export const dynamic = "auto";
// "auto" | "force-dynamic" | "error" | "force-static"

export const revalidate = 3600; // false | 0 | number (must be statically analyzable)
```

| `dynamic` value | Meaning |
|---|---|
| `"auto"` (default) | Cache as much as possible without blocking dynamic behavior |
| `"force-dynamic"` | Render per request; fetches uncached |
| `"force-static"` | Prerender; `cookies()`, `headers()`, `useSearchParams()` return empty values |
| `"error"` | Prerender; error if anything dynamic is used |

`revalidate` sets the route's default refresh interval. The **lowest** value across the layouts and page of a route wins. A value like `600` is valid; `60 * 10` is not, because it must be statically analyzable.

In development, pages are always rendered on demand and never cached, so test with `next build` and `next start`.

### Invalidating it

The stored output is replaced when:

- you redeploy,
- the time-based `revalidate` interval passes, or
- you call `revalidatePath` / `revalidateTag` (see [Revalidation](./04-revalidation.md)).

## Cache Components: a static shell with holes

With `cacheComponents: true`, a route is no longer all-or-nothing. At build time Next.js renders the component tree and produces a **static shell**: everything that can be known at build time, with placeholders (Suspense fallbacks) where request-time content will stream in. This is Partial Prerendering, the default behavior in this model.

| What the component does | Where it ends up |
|---|---|
| Pure rendering, module imports, synchronous reads | In the static shell automatically |
| Uses `use cache` | In the shell, if its lifetime is long enough |
| Uses uncached data or runtime APIs inside `<Suspense>` | Fallback in the shell; content streams at request time |
| Uses uncached data or runtime APIs **outside** `<Suspense>` | A "blocking route" insight/error: fix by caching or adding Suspense |

```text
┌─────────────────────────────────────┐
│ Header           ← static shell     │
│ Cached blog posts ← static shell    │   served from the CDN instantly
│ [ Loading prefs… ] ← fallback       │
│ Footer           ← static shell     │
└─────────────────────────────────────┘
          then streamed at request time:  <UserPreferences /> (reads cookies)
```

Reading `cookies()` no longer turns the whole route dynamic; only the part behind the Suspense boundary streams. Details and code in [Cache Components](./05-cache-components.md).

### The App Shell

When a route has dynamic params that are not known at build time, the reusable, URL-independent shell is called the **App Shell**: the same static shell with the param-specific parts left behind their fallbacks. It is served instantly for any URL of that route.

### ISR with Cache Components

For a route like `/blog/[slug]`:

- `generateStaticParams` lists the URLs to prerender at build time. Under Cache Components it **must return at least one param** (an empty array errors).
- Any other URL gets the App Shell right away, is rendered in the background with its real params, and is cached for the next visitor.
- `export const dynamicParams` is **not supported** and fails the build. For unknown slugs that should 404, call `notFound()` when the data does not exist.

See the ISR guide in the Next.js docs for the full walkthrough and for the `await params` placement rules.

### Enforcing static output

Next.js 16.4 adds an optional route export to require static output:

```tsx
export const ensureStatic = "navigation";
```

It reports an error if request-specific server work prevents prerendering. Use it only where a route has a hard static requirement.

### Opting out while migrating

A segment can be marked as allowed to block (not required to produce an instant static shell):

```tsx
export const instant = false;
```

It does not force the route to be dynamic; a prerenderable route still gets its shell. Use it to adopt Cache Components incrementally.

### Bots and crawlers

Dynamic metadata streams after the shell. Some crawlers need metadata in the initial `<head>`, so Next.js detects them by user agent, skips the shell, and renders the page dynamically at request time. Data sources used during prerender must therefore also be reachable at request time, or those crawlers can get a 500.

## Previous model vs Cache Components

| | Full Route Cache (previous) | Static shell (Cache Components) |
|---|---|---|
| Granularity | Whole route | Per component via Suspense and `use cache` |
| Reading `cookies()` | Whole route dynamic | Only that component dynamic (inside Suspense) |
| Controls | `dynamic`, `revalidate`, `fetchCache` exports | `use cache`, `cacheLife`, `<Suspense>` |
| Output on a request | Static HTML **or** dynamic render | Static shell **plus** streamed parts |
| Static export (`output: "export"`) | Supported | Not supported |

## Common mistakes

| Mistake | Symptom | Fix |
|---|---|---|
| Expecting `next dev` to show prerender caching | Always fresh in dev | Test with `build` + `start` |
| `cookies()` in a root layout (previous model) | Entire app dynamic | Read it in a leaf component |
| Route config exports under Cache Components | Build error | Replace with `use cache` / `cacheLife` (see migration in [Cache Components](./05-cache-components.md)) |
| Returning `[]` from `generateStaticParams` under Cache Components | `empty-generate-static-params` error | Return at least one real param |
| Using `dynamicParams` with Cache Components | Build error | Delete it; use `notFound()` |
| Static data frozen after deploy | Stale content | Add revalidation (time or tag) |

## Quick Summary

- Previous model: static routes are stored whole (HTML + RSC payload); any dynamic trigger makes the whole route dynamic.
- Cache Components: routes produce a static shell plus streamed dynamic parts (Partial Prerendering).
- App Shell and ISR handle routes with params not known at build time.
- Route segment config (`dynamic`, `revalidate`, `fetchCache`) is replaced by `use cache`, `cacheLife` and Suspense.
- Always verify prerender behavior with a production build.

## Next

- [Router Cache](./03-router-cache.md)
- [Revalidation](./04-revalidation.md)
- [Static Rendering](../04-rendering/01-static-rendering.md)