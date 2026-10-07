# Data Cache

The data cache stores the **results of data fetching** on the server so later requests can skip the network or database call. In the previous caching model this means `fetch` options and `unstable_cache`. With Cache Components, the equivalent is the `use cache` directive. This note covers the previous model in depth and shows how it maps onto `use cache`.

> Verified against the Next.js 16.4 docs. If `cacheComponents: true` is set in your config, read [Cache Components](./05-cache-components.md) first; this note is most useful for existing apps and for understanding older code.

## Default behavior

Since Next.js 15, `fetch` responses are **not cached by default**:

```tsx
await fetch("https://api.example.com/data"); // not stored in the data cache
```

One subtlety in the previous model: a `fetch` with no `cache` option that is reached **before any request-time API** (like `cookies()`) is fetched once during `next build`, because the route is prerendered up to that point. Requests reached **after** a request-time API run on every request. This is why identical-looking code can behave differently depending on where it sits in a component.

## Opting in with `fetch`

```tsx
// Cache indefinitely (until revalidated)
await fetch(url, { cache: "force-cache" });

// Cache and revalidate after N seconds (time-based)
await fetch(url, { next: { revalidate: 3600 } });

// Cache and tag for on-demand invalidation
await fetch(url, { next: { tags: ["posts"] } });

// Never reuse
await fetch(url, { cache: "no-store" });
```

| Option | Effect |
|---|---|
| `cache: "force-cache"` | Store the response and reuse it |
| `next.revalidate: n` | Reuse for `n` seconds, then refresh in the background (stale-while-revalidate) |
| `next.tags: [...]` | Attach tags so `revalidateTag` can invalidate it |
| `cache: "no-store"` | Always fetch fresh; makes the route dynamic |

Tags are case-sensitive and must be 256 characters or fewer.

## Caching non-`fetch` work: `unstable_cache`

For database queries and SDK calls, `fetch` options do not apply. Wrap the function:

```ts
import { unstable_cache } from "next/cache";
import { db } from "@/lib/db";

export const getCachedUser = unstable_cache(
  async (id: string) => db.user.findUnique({ where: { id } }),
  ["user"], // key prefix; arguments are added to the key
  { tags: ["user"], revalidate: 3600 },
);
```

The third argument takes `tags` and `revalidate` (seconds). The name carries `unstable_` as a historical prefix; in the Cache Components model it is replaced by `use cache`, but it keeps working.

## Persistence

An important difference between the two mechanisms:

| | `fetch` data cache and `unstable_cache` | `use cache` (Cache Components) |
|---|---|---|
| Survives a new deployment | Yes | **No**; the cache key includes the build id |
| Shared across serverless instances | Yes | **No** by default (in-memory); use `use cache: remote` or a cache handler |
| Stored | Durable cache storage | In memory by default |

If you migrate, expect cached values to recompute after each deploy.

## How it relates to revalidation

Cached data stays until it is revalidated. You refresh it by:

- time: `next.revalidate` / `revalidate` option
- event: `revalidateTag("posts", "max")` or `revalidatePath(...)` from a Server Action or Route Handler

Full coverage in [Revalidation](./04-revalidation.md).

## Mapping to Cache Components

| Previous model | Cache Components |
|---|---|
| `fetch(url, { cache: "force-cache" })` | Wrap in a function with `"use cache"` |
| `next: { revalidate: 3600 }` | `cacheLife("hours")` or `cacheLife({ revalidate: 3600 })` |
| `next: { tags: ["data"] }` | `cacheTag("data")` |
| `unstable_cache(fn, keys, opts)` | `async function fn() { "use cache"; cacheLife(...); cacheTag(...) }` |
| `cache: "no-store"` / `noStore()` | Leave outside a cache scope; it is uncached by default |

```tsx
// Previous model
const res = await fetch("https://api.example.com/data", {
  cache: "force-cache",
  next: { revalidate: 3600, tags: ["data"] },
});

// Cache Components
import { cacheLife, cacheTag } from "next/cache";

async function getData() {
  "use cache";
  cacheLife("hours");
  cacheTag("data");
  const res = await fetch("https://api.example.com/data");
  return res.json();
}
```

Your existing `fetch` and `unstable_cache` code keeps working under Cache Components, so you can migrate gradually.

## Advanced: `fetchCache`

A route segment export (`export const fetchCache = "force-no-store"` and friends) overrides the default `cache` option for every `fetch` in a layout or page. It is an advanced escape hatch in the previous model and is **not supported** with Cache Components, so prefer setting caching at the data source.

## Common mistakes

| Mistake | Symptom | Fix |
|---|---|---|
| Assuming `fetch` is cached by default (Next.js 14 habit) | Extra requests | Opt in with `force-cache` / `revalidate`, or use `use cache` |
| Caching user-specific data in a shared cache | One user sees another's data | Keep personal data uncached, or include the user id as an explicit key |
| Forgetting tags | Cannot invalidate on demand | Add `next.tags` / `cacheTag` |
| Dropping `unstable_cache` key parts | Different calls share one entry | Make keys unique per input |
| Expecting `use cache` entries to survive deploys | Cold cache after each release | Plan for recompute, or use a durable remote cache |
| Revalidating a path when data is shared across pages | Other pages stay stale | Revalidate by tag |

## Quick Summary

- `fetch` is uncached by default in Next.js 15+; opt in with `cache`, `next.revalidate`, `next.tags`.
- `unstable_cache` caches non-`fetch` work; `use cache` supersedes it under Cache Components.
- `fetch` and `unstable_cache` persist across deploys; `use cache` is per deployment and in-memory by default.
- In the previous model, where a `fetch` sits relative to request-time APIs affects whether it runs at build or per request.

## Next

- [Full Route Cache](./02-full-route-cache.md)
- [Revalidation](./04-revalidation.md)
- [Cache Components](./05-cache-components.md)