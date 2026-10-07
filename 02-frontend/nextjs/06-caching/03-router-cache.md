# Router Cache (Client Cache)

The router cache lives in the **browser's memory**. It stores the RSC payloads of routes you have visited or that were prefetched, so clicking a link can be instant and back/forward navigation preserves layout and scroll. It is called the **client cache** in recent docs. Unlike the server-side caches, you cannot see it in your build output, which is why stale-page confusion often traces back to it.

> Verified against the Next.js 16.4 docs. `staleTimes` is documented as experimental.

## What it does

- Reuses **shared layouts** so only the changed segment is fetched on navigation (partial rendering).
- Reuses **loading states** (`loading.tsx`) so a skeleton can show instantly.
- Holds **prefetched** route data from `<Link>` elements in the viewport (production only).
- Keeps data in memory only; a full page reload clears it.

```text
Click a link
   │
   ├─ data in client cache and still fresh?  ──► render instantly, no request
   │
   └─ otherwise ──► request the RSC payload for the changed segments
                    ──► store it in the client cache
```

## How long entries live (previous model)

Prefetching behavior sets the freshness window. These are controlled by the experimental `staleTimes` option:

```ts
// next.config.ts
const nextConfig: NextConfig = {
  experimental: {
    staleTimes: {
      dynamic: 30, // seconds
      static: 180,
    },
  },
};
```

| Property | Applies to | Default |
|---|---|---|
| `dynamic` | Pages that are neither static nor fully prefetched | **0** seconds (not cached); this was 30s before Next.js 15 |
| `static` | Statically generated pages, `prefetch={true}` links, `router.prefetch()` | **5 minutes** |

Other details from the docs:

- `loading.tsx` boundaries are treated as reusable for the `static` period.
- This does not affect partial rendering: shared layouts are not refetched on every navigation, only the segment that changes.
- Back/forward navigation is not affected by stale times, so scroll position and layout are preserved.

## With Cache Components: `cacheLife` drives the client cache

When Cache Components is enabled, the cached function's `stale` value controls how long the browser can use it without checking the server:

```tsx
cacheLife({ stale: 300 }); // client may reuse for 5 minutes
```

- The server sends the value in the `x-nextjs-stale-time` response header.
- A **minimum of 30 seconds** is enforced so prefetched links remain usable until clicked.
- Updating `staleTimes.static` also updates the `stale` value of the `default` cache profile.

## Prefetching

`<Link>` prefetches routes that scroll into view, in production only.

Previous model:

- Static routes can be prefetched in full.
- Dynamic routes are prefetched up to the nearest `loading.tsx`.

With **Partial Prefetching** (enabled by default in new projects per the 16.4 docs):

- The router prefetches each route's **App Shell**: static content plus session data derived from `cookies()` and `headers()`.
- Set `prefetch={true}` on a link to also prefetch cached content that depends on that link's URL data (`searchParams`, dynamic `params`). Next.js re-renders the route at prefetch time with the destination URL resolved.
- That per-link prefetch **costs a server invocation per link**, so use it deliberately.

```tsx
<Link href="/search?q=shoes" prefetch={true}>Shoes</Link>
```

`"use cache: private"` results (cached functions that read cookies, headers or search params directly) live only in the browser, as part of the per-link prefetch.

## State preservation with Cache Components

With Cache Components, Next.js keeps recently visited routes mounted but hidden using React's `<Activity>` instead of unmounting them. Consequences:

- Component state, form input values and scroll position persist when you navigate away and back.
- Effects are cleaned up while hidden and re-run when the route becomes visible.
- Code that relied on unmounting to reset state (dropdowns, dialogs, forms after submission) needs explicit reset logic.

Older routes are eventually removed from the DOM to prevent unbounded growth.

## Invalidating the client cache

| Action | Effect |
|---|---|
| Calling `revalidatePath`, `revalidateTag`, `updateTag` or `refresh` inside a **Server Action** | Immediately clears the **entire** client cache, bypassing stale times |
| `revalidatePath("/", "layout")` | Purges the client cache and invalidates all cached data on next visit |
| `router.refresh()` in a Client Component | Re-fetches the current route's Server Components and updates in place, keeping client state |
| Full page reload | Clears the in-memory cache |

`revalidatePath` / `revalidateTag` called from a **Route Handler** do not run in the user's browser and cannot clear that user's client cache; the effect is seen on the next fetch after the stale window.

```tsx
"use client";

import { useRouter } from "next/navigation";

export function RefreshButton() {
  const router = useRouter();
  return <button onClick={() => router.refresh()}>Refresh</button>;
}
```

## Symptoms and fixes

| Symptom | Likely cause | Fix |
|---|---|---|
| Navigating back shows old data | Client cache within its stale window | Revalidate from a Server Action, or call `router.refresh()` |
| Page looks fresh on reload but stale on link click | Prefetched or cached payload reused | Shorten stale time (`cacheLife` / `staleTimes`) or revalidate |
| Mutation succeeds but UI unchanged | Mutation was not followed by revalidation | Call `updateTag` / `revalidatePath` in the Server Action |
| Prefetching never visible in dev | Prefetching is production-only | Test with `build` + `start` |
| Form values still filled after revisiting a route | `<Activity>` state preservation (Cache Components) | Reset state explicitly |
| Too many server invocations from links | `prefetch={true}` on many links | Use it only for links that benefit |

## Common mistakes

| Mistake | Fix |
|---|---|
| Treating the router cache as something you configure on the server | It is per-browser memory; control it with `stale`, `staleTimes`, and revalidation |
| Expecting Route Handler revalidation to update the visible UI instantly | Use a Server Action, or refresh on the client |
| Setting very short `stale` values and expecting instant navigation | Values under 30 seconds are floored to 30 for time-based expiry |
| Assuming `staleTimes` is stable | It is experimental; verify before relying on it |

## Quick Summary

- The router cache is browser memory holding route payloads, enabling instant and partial navigation.
- Previous model: `dynamic` pages 0s, `static` pages 5 minutes (experimental `staleTimes`).
- Cache Components: `cacheLife`'s `stale` (minimum 30s) controls it; Partial Prefetching prefetches the App Shell and optionally per-link content.
- Server Action revalidation clears the whole client cache; `router.refresh()` refetches the current route.
- Cache Components preserves state of recently visited routes with `<Activity>`.

## Next

- [Revalidation](./04-revalidation.md)
- [Navigation](../02-routing/03-navigation.md)
- [Cache Components](./05-cache-components.md)