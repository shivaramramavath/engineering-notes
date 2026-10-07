# Rendering Overview

"Rendering" means turning React components into output the browser can display. In Next.js the interesting questions are **where** it happens (server or browser) and **when** (build time or request time). This note gives you the vocabulary, maps the older terms (SSR, SSG, ISR, CSR) onto the App Router, and explains how Next.js picks a strategy for each route.

> **Two models.** Next.js 16 has two rendering/caching models. With **Cache Components** (`cacheComponents: true`, the default in new projects from recommended `create-next-app` settings per the 16.4 docs), everything is dynamic by default and you opt in to caching with `use cache`. The **previous model** is static by default, with dynamic APIs and route config deciding. Check your `next.config.ts`. This note explains both; much of the "static unless something forces dynamic" detail below describes the previous model, and the Cache Components section near the end describes the new one.

## Two environments

| Environment | Runs | Produces |
|---|---|---|
| **Server** | Server Components, and the first render of Client Components | HTML and the RSC payload |
| **Client (browser)** | Client Components after hydration | DOM updates |

Components in the App Router are Server Components by default. See [Server Components](../03-components/00-server-components.md).

## Three strategies, by timing

| Strategy | When HTML is produced | Typical use |
|---|---|---|
| **Static rendering** | At build time (or on revalidation), then cached | Marketing pages, docs, blog posts |
| **Dynamic rendering** | At request time, for every request | Personalized or real-time pages |
| **Streaming** | In chunks, as parts become ready | Pages with slow data (works with either of the above) |

```text
Static:    build ──► HTML cached ──► CDN serves it instantly to everyone
Dynamic:   request ──► server renders ──► response   (per user, per request)
Streaming: request ──► shell sent immediately ──► slow parts stream in later
```

Most apps use all three, on different routes, and sometimes within one route.

## Old names, new model

Older Next.js terminology maps like this:

| Term | Meaning | In the App Router |
|---|---|---|
| **SSG** (static site generation) | HTML built at build time | Static rendering (the default when nothing is dynamic) |
| **ISR** (incremental static regeneration) | Static pages refreshed over time | Static rendering plus revalidation |
| **SSR** (server-side rendering) | HTML rendered on each request | Dynamic rendering |
| **CSR** (client-side rendering) | Browser renders after fetching data | Client Components that fetch in the browser |

The App Router does not make you pick one per page with special functions like `getStaticProps`/`getServerSideProps`. The strategy follows from what your components do.

## How Next.js decides

In the **previous model** (no `cacheComponents`), a route is **static by default**. It becomes **dynamic** when something requires information only available at request time:

| Trigger | Why it needs the request |
|---|---|
| `cookies()`, `headers()` | Reads request data |
| `searchParams` prop | Query string varies per request |
| `connection()` | Explicit "wait for a real request" |
| Draft mode | Per-request state |
| Uncached data fetching, depending on your version and configuration | Data must be fresh |
| `export const dynamic = "force-dynamic"` | You said so |

If none apply, the route is rendered once and the HTML is reused. Details in [Static Rendering](./01-static-rendering.md) and [Dynamic Rendering](./02-dynamic-rendering.md).

## Reading the build output

`next build` prints how each route will be served:

```text
Route (app)
┌ ○ /
├ ○ /about
├ ● /blog/[slug]
├ ƒ /dashboard
└ ƒ /api/me

○  (Static)   prerendered as static content
●  (SSG)      prerendered as static content using generateStaticParams
ƒ  (Dynamic)  server-rendered on demand
```

Newer versions may add a symbol for partially prerendered routes. This table is the fastest way to verify that a page behaves the way you intended. Treat an unexpected `ƒ` as a bug to investigate.

## What a route is made of

A single route is not all-or-nothing. A page can contain a static shell, with dynamic holes streamed in later:

```text
┌───────────────────────────────┐
│ Header          (static)      │
│ Product info   (static)      │
│ ┌───────────────────────────┐ │
│ │ Cart count   (dynamic)    │ │ ← streamed after the shell
│ └───────────────────────────┘ │
│ Footer          (static)      │
└───────────────────────────────┘
```

This is what [Streaming and Suspense](./03-streaming-and-suspense.md) and Cache Components build on.

## The newer model: Cache Components

Next.js 16 introduces **Cache Components** (enabled with `cacheComponents: true`, and already on in new projects created with the recommended `create-next-app` defaults, per the 16.4 docs). It changes the default question. Instead of "static unless something forces dynamic", the model is: render at request time by default, and **explicitly opt parts into caching** with `use cache`. The framework prerenders a **static shell** from static, cached and non-request-dependent parts, and streams the rest; this is Partial Prerendering. Request-time data (`cookies()`, `headers()`, `searchParams`) no longer makes the whole route dynamic, but it must sit under a `<Suspense>` boundary.

Under this model the route-level options `dynamic`, `revalidate`, `fetchCache` and `dynamicParams` are not supported (they are replaced by `use cache`, `cacheLife` and Suspense), and the Node.js runtime is required. Full coverage, including migration, is in [Cache Components](../06-caching/05-cache-components.md). The static/dynamic sections of this chapter describe the previous model, which still explains a lot of existing code and tutorials.

## What the browser does after the HTML arrives

1. Shows the pre-rendered HTML immediately.
2. Downloads JavaScript for Client Components.
3. **Hydrates**, attaching event handlers and state to the existing HTML.
4. Handles later navigations by requesting only the changed segments' RSC payload.

If the server HTML and the first client render differ, you get a hydration mismatch. See [Hydration Errors](../19-debugging/01-hydration-errors.md).

## Choosing a strategy

| Situation | Favor |
|---|---|
| Same content for everyone, changes rarely | Static |
| Same for everyone, changes on a schedule or on events | Static + revalidation |
| Depends on the logged-in user or request | Dynamic |
| Mostly shared, with a few personalized widgets | Static shell + streamed dynamic parts |
| Slow data sources | Streaming |
| Interactive widgets driven by browser state | Client Components |

## Common misconceptions

| Belief | Reality |
|---|---|
| "Static pages cannot show fresh data" | They can be revalidated on a timer or on demand |
| "Dynamic rendering is always slow" | It is per-request work, but streaming and caching reduce the cost |
| "`use client` means client-side rendering" | Client Components are still server pre-rendered and then hydrated |
| "I choose the strategy with a config flag" | Mostly implicit from your code; flags are overrides |
| "One dynamic call only affects that component" | Without a Suspense boundary or caching, it makes the whole route dynamic |

## Quick Summary

- Rendering happens on the server (Server Components, first render of Client Components) and in the browser (after hydration).
- Three timings: static (build), dynamic (request), streaming (in chunks).
- Routes are static by default and become dynamic when they use request-time data or you force it.
- The `next build` route table shows what you actually got.
- Cache Components (Next.js 16; default in new projects) flips the default to "dynamic unless cached", with a static shell plus streamed parts.

## Next

- [Static Rendering](./01-static-rendering.md)
- [Dynamic Rendering](./02-dynamic-rendering.md)
- [Caching Overview](../06-caching/00-caching-overview.md)