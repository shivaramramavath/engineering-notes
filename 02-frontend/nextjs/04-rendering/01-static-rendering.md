# Static Rendering

Static rendering produces a route's HTML **once, ahead of time**, then serves the saved result to every visitor. It is the fastest and cheapest way to serve a page, which is why Next.js uses it whenever it can.

> **Which model?** This note describes the **previous model** (no `cacheComponents`). With Cache Components (the default in new projects per the 16.4 docs), routes are not simply "static or dynamic": Next.js prerenders a static shell and streams request-time parts, route options like `dynamic`, `revalidate` and `dynamicParams` are not supported, and `generateStaticParams` must return at least one param. See [Full Route Cache](../06-caching/02-full-route-cache.md) and [Cache Components](../06-caching/05-cache-components.md).

## How it works

```text
next build
   │  render the route's Server Components
   │  (data fetched now, at build time)
   ▼
HTML + RSC payload saved
   │
   ▼
Every request → serve the saved result (from a CDN if you have one)
```

The server does no per-request rendering. Many users can be served from cache with minimal compute, and time to first byte is excellent.

## A static page

```tsx
// app/about/page.tsx
export default function About() {
  return (
    <main>
      <h1>About us</h1>
      <p>We build things.</p>
    </main>
  );
}
```

Nothing here depends on the request, so the route is static (shown as `○` in the build output).

Static pages can still fetch data. It happens once at build time:

```tsx
// app/changelog/page.tsx
export default async function Changelog() {
  const res = await fetch("https://api.example.com/changelog");
  const entries: { id: string; title: string }[] = await res.json();

  return (
    <ul>
      {entries.map((e) => (
        <li key={e.id}>{e.title}</li>
      ))}
    </ul>
  );
}
```

The data baked into the HTML is whatever the API returned during the build, until the page is regenerated.

## What keeps a route static

A route stays static as long as it does **not** use request-time information. These make a route dynamic (see [Dynamic Rendering](./02-dynamic-rendering.md)):

- `cookies()`, `headers()`, `connection()`
- Reading `searchParams`
- Draft mode
- `export const dynamic = "force-dynamic"`
- Data fetching configured to be uncached

A dynamic call high in the tree, such as in a root layout, makes every route beneath it dynamic. Keep request-dependent code low and isolated.

## Static pages with dynamic segments

A route like `/blog/[slug]` has many possible URLs. Tell Next.js which to pre-render with `generateStaticParams`:

```tsx
// app/blog/[slug]/page.tsx
export async function generateStaticParams() {
  const posts = await getAllPosts();
  return posts.map((p) => ({ slug: p.slug }));
}

export default async function Post({
  params,
}: {
  params: Promise<{ slug: string }>;
}) {
  const { slug } = await params;
  const post = await getPost(slug);
  return <h1>{post.title}</h1>;
}
```

Each returned object becomes a pre-rendered page (the build shows `●`). In the previous model, slugs you did not list are rendered on demand the first time they are requested, then reused, unless you set `export const dynamicParams = false`, in which case they 404. With Cache Components, `dynamicParams` is not supported (delete it and call `notFound()` for unknown data), and `generateStaticParams` must return at least one param. See [Dynamic Routes](../02-routing/01-dynamic-routes.md).

## Keeping static pages fresh

A page built at deploy time goes stale. Two options, both covered in [Revalidation](../06-caching/04-revalidation.md):

**Time-based revalidation:** regenerate in the background after N seconds.

```tsx
// app/changelog/page.tsx
export const revalidate = 3600; // seconds

export default async function Changelog() { /* ... */ }
```

The first request after the interval still gets the old page, while a new one is generated in the background. The following requests get the new page. This stale-while-revalidate behavior is what "incremental static regeneration" means.

**On-demand revalidation:** regenerate when something changes, from a Server Action or Route Handler:

```ts
import { revalidatePath, revalidateTag } from "next/cache";

revalidatePath("/changelog");
revalidateTag("changelog", "max");
```

Check the `revalidateTag` signature for your version; it has changed across recent releases.

With Cache Components enabled, the equivalent controls are `use cache` with `cacheLife` and `cacheTag`. Route segment exports such as `revalidate`, `dynamic` and `fetchCache` fail the build in that mode. See [Cache Components](../06-caching/05-cache-components.md).

## Mixing static pages with client-side data

Parts of a static page that must be fresh or personal can load in the browser:

```tsx
// Static page shell, client-fetched widget
import { UserBadge } from "./user-badge"; // "use client", fetches /api/me in an effect
```

This keeps the route static while still showing per-user data. The cost is a client-side fetch and a loading state.

## Static export: no server at all

If you only need files to host on any static host or CDN:

```ts
// next.config.ts
import type { NextConfig } from "next";

const nextConfig: NextConfig = {
  output: "export",
};

export default nextConfig;
```

`next build` then writes plain HTML, CSS and JS to an `out/` folder. Trade-offs:

| Works | Does not work |
|---|---|
| Static pages, `generateStaticParams`, Client Components | Anything needing a server at request time |
| Client-side data fetching | Dynamic APIs (`cookies`, `headers`), Server Actions |
| | Route Handlers that read the request, `proxy.ts`, ISR / revalidation |
| | The default image optimizer (needs a custom loader) |

Use it only when you genuinely have no server. Otherwise a normal Next.js deployment gives you everything.

## Pros and limits

| Pros | Limits |
|---|---|
| Fastest delivery, CDN-friendly | Content frozen until rebuilt or revalidated |
| Cheap: no per-request compute | Cannot depend on the request (user, cookies, query) |
| Resilient: works even if the origin is slow | Large sites take longer builds if every page is pre-rendered |

## Debugging "why is this not static?"

1. Look at `next build` output: `○`/`●` is static, `ƒ` is dynamic.
2. Search the route and its parent layouts for `cookies()`, `headers()`, `searchParams`, `connection()`, draft mode.
3. Check for `dynamic = "force-dynamic"` or uncached fetches.
4. If a layout is dynamic, every child route below it is too. Move the request-dependent code lower.
5. If you forced static but used a dynamic API, you get an error such as the route "couldn't be rendered statically because it used `cookies`". Remove the dynamic call or stop forcing static.

## Common mistakes

| Mistake | Symptom | Fix |
|---|---|---|
| Expecting fresh data on a static page | Data stuck at build time | Add revalidation, or make the route dynamic |
| Reading `searchParams` for filtering on an otherwise static page | Route becomes dynamic | Filter on the client, or accept dynamic |
| Cookie read in a root layout | Entire app dynamic | Move to the component that needs it, wrap in Suspense |
| Big `generateStaticParams` list | Slow builds | Pre-render the popular ones; let the rest render on demand |
| Using `output: "export"` and then adding Server Actions | Build error | Remove `export` or the feature |
| Expecting the build to hit the API once per request | It runs once per page at build | Revalidate or go dynamic |

## Quick Summary

- Static rendering builds HTML once and serves it to everyone; it is the default when nothing is request-dependent.
- Data fetched in a static page is frozen at build time until revalidation.
- `generateStaticParams` pre-renders dynamic-segment pages; `dynamicParams` controls the rest.
- Refresh with time-based or on-demand revalidation.
- `output: "export"` produces a server-free site with major feature limits.
- Verify with the build's route table; hunt dynamic triggers when a route is unexpectedly `ƒ`.

## Next

- [Dynamic Rendering](./02-dynamic-rendering.md)
- [Revalidation](../06-caching/04-revalidation.md)
- [Full Route Cache](../06-caching/02-full-route-cache.md)