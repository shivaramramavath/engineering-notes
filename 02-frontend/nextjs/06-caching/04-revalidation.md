# Revalidation

Revalidation updates cached data so you can keep serving fast cached responses without showing stale content forever. There are two strategies: **time-based** (refresh after a duration) and **on-demand** (refresh when something happens). This note covers every API, where each can be called, and how to choose.

> Verified against the Next.js 16.4 docs. `revalidateTag` now requires a second argument; the single-argument form is deprecated.

## Time-based revalidation

### With Cache Components: `cacheLife`

`cacheLife` sets how long a cached function or component stays valid. Call it inside a `use cache` scope:

```ts
import { cacheLife } from "next/cache";

export async function getProducts() {
  "use cache";
  cacheLife("hours");
  return db.query("SELECT * FROM products");
}
```

A profile has three timings:

| Property | Where | Meaning |
|---|---|---|
| `stale` | Client | How long the browser can reuse it without asking the server |
| `revalidate` | Server | After this, the next request gets the cached version and triggers a background refresh |
| `expire` | Server | After this with no traffic, the next request waits for fresh content |

Built-in profiles:

| Profile | Use for | `stale` | `revalidate` | `expire` |
|---|---|---|---|---|
| `default` | Standard content | 5 min | 15 min | never |
| `seconds` | Real-time data | 30 s | 1 s | 1 min |
| `minutes` | Frequently updated | 5 min | 1 min | 1 hour |
| `hours` | Updated several times a day | 5 min | 1 hour | 1 day |
| `days` | Updated daily | 5 min | 1 day | 1 week |
| `weeks` | Updated weekly | 5 min | 1 week | 30 days |
| `max` | Rarely changes | 5 min | 30 days | 1 year |

Pass an object for one-off values, or define named profiles in `next.config.ts`:

```ts
// inline
cacheLife({ stale: 3600, revalidate: 900, expire: 86400 });
```

```ts
// next.config.ts
const nextConfig: NextConfig = {
  cacheComponents: true,
  cacheLife: {
    editorial: { stale: 600, revalidate: 3600, expire: 86400 },
  },
};
```

Rules: `expire` must be longer than `revalidate`; omitted properties inherit from `default`; call `cacheLife` in the same scope as `use cache`, not at module level; set one explicitly in every `use cache` scope. Very short lifetimes (zero `revalidate`, `expire` under 5 minutes, `stale` under 30 s) are excluded from prerenders and become request-time holes; see [Cache Components](./05-cache-components.md).

### Previous model

```tsx
// per fetch
await fetch(url, { next: { revalidate: 3600 } });

// route default
export const revalidate = 3600; // statically analyzable number

// non-fetch
unstable_cache(fn, ["key"], { revalidate: 3600 });
```

The first request after the interval still receives the old copy while a new one is generated in the background; later requests get the new copy.

## On-demand revalidation

### 1. Tag the data

```ts
// Cache Components
import { cacheTag } from "next/cache";

async function getPosts() {
  "use cache";
  cacheTag("posts");
  // ...
}
```

```ts
// Previous model
await fetch(url, { next: { tags: ["posts"] } });
```

Tags are case-sensitive, up to 256 characters. A longer tag is silently never assigned.

### 2. Invalidate it

| API | Where it can be called | Behavior |
|---|---|---|
| `updateTag(tag)` | **Server Actions only** | Expires the tag immediately; next request waits for fresh data (read-your-own-writes) |
| `revalidateTag(tag, profile)` | Server Actions and Route Handlers | Marks data stale; stale-while-revalidate |
| `revalidatePath(path, type?)` | Server Functions and Route Handlers | Invalidates a specific page or layout path |
| `refresh()` | **Server Actions only** | Refreshes the client router from a Server Action |

None of these can be called from Client Components or the proxy.

#### `updateTag`: the user must see their change

```ts
"use server";

import { updateTag } from "next/cache";
import { redirect } from "next/navigation";

export async function createPost(formData: FormData) {
  const post = await db.post.create({
    data: { title: String(formData.get("title")), content: String(formData.get("content")) },
  });

  updateTag("posts");
  updateTag(`post-${post.id}`);
  redirect(`/posts/${post.id}`);
}
```

#### `revalidateTag`: a slight delay is fine

```ts
import { revalidateTag } from "next/cache";

revalidateTag("posts", "max");
```

The second argument says how long stale content may be served while fresh content generates:

- `"max"` (recommended): a one-year window, so requests always get stale content while it refreshes in the background.
- Another profile name or `{ expire: n }`: a different window.
- `{ expire: 0 }`: never serve stale; the next request blocks for fresh data. Use it in a webhook Route Handler when you need immediate expiry, since `updateTag` is not available there.
- No second argument is **deprecated** and behaves like `{ expire: 0 }`.

A revalidation is triggered by a **request**, not by the call itself, so pages refresh as they are visited rather than all at once.

```ts
// app/api/revalidate/route.ts: CMS webhook
import { revalidateTag } from "next/cache";
import type { NextRequest } from "next/server";

export async function POST(request: NextRequest) {
  const { tag } = await request.json();
  revalidateTag(tag, "max");
  return Response.json({ revalidated: true });
}
```

Authenticate webhook endpoints (a shared secret or signature check) so strangers cannot trigger revalidation.

#### `revalidatePath`

```ts
revalidatePath("/blog/post-1"); // one specific page
revalidatePath("/blog/[slug]", "page"); // all matching pages
revalidatePath("/blog/[slug]", "layout"); // layout and everything beneath it
revalidatePath("/", "layout"); // everything; also purges the client cache
```

- If the path contains a dynamic segment (`/product/[slug]`), the `type` argument is required.
- Do not add `/page` or `/layout` to the path.
- With **rewrites**, pass the **destination** path (the real route), not the URL in the address bar.
- In a Server Function it updates the visible UI immediately; in a Route Handler it marks the path and revalidates on the next visit.
- It only refreshes the targeted path. Other pages using the same tagged data stay cached until their tag is invalidated.

Prefer tags to paths when possible; they are more precise and do not over-invalidate. They combine well:

```ts
revalidatePath("/blog"); // this page
updateTag("posts"); // every page that uses the 'posts' tag
```

#### `refresh`

```ts
"use server";

import { refresh } from "next/cache";

export async function createPost(formData: FormData) {
  await db.post.create({ data: { /* ... */ } });
  refresh(); // refresh the client router
}
```

## Choosing

| Situation | Use |
|---|---|
| Content changes on a schedule | `cacheLife` (or `revalidate` in the previous model) |
| User saves something and must see it right away | `updateTag` in the Server Action |
| CMS or external webhook announces a change | `revalidateTag(tag, "max")` in a Route Handler |
| Revalidate one known page | `revalidatePath("/exact/path")` |
| Everything under a layout | `revalidatePath("/x", "layout")` |
| Long-lived content with rare edits | `cacheTag` + `cacheLife("max")` + invalidate on publish |
| Update the UI without changing cached data | `router.refresh()` (client) or `refresh()` (Server Action) |

Time-based and on-demand work together: a long `cacheLife` plus a tag avoids needless refreshes for data that has not changed.

## Serverless caveat

With the default in-memory cache, entries may not persist between serverless requests or across revalidations. If you rely on cache hits at runtime, use `use cache: remote` or a cache handler; see [Cache Components](./05-cache-components.md).

## Common mistakes

| Mistake | Symptom | Fix |
|---|---|---|
| `revalidateTag("x")` with one argument | Deprecated; works only with TypeScript errors suppressed | `revalidateTag("x", "max")` or `updateTag("x")` in actions |
| `updateTag` in a Route Handler | Throws: only valid in Server Actions | Use `revalidateTag(tag, { expire: 0 })` or `"max"` |
| `revalidatePath` with a dynamic segment and no `type` | Error | Pass `"page"` or `"layout"` |
| Revalidating the rewrite source path | Nothing refreshes | Use the destination path |
| Revalidating by path when data is shared | Other pages stay stale | Revalidate by tag |
| Tag over 256 characters | Never assigned, revalidation does nothing | Shorten |
| Unprotected revalidation endpoint | Anyone can trigger it | Verify a secret or signature |
| Calling these from a Client Component | Not allowed | Call a Server Action |

## Quick Summary

- Time-based: `cacheLife` profiles (Cache Components) or `next.revalidate` / `revalidate` (previous model).
- On-demand: tag with `cacheTag` (or `next.tags`), invalidate with `updateTag` (Server Actions, read-your-own-writes) or `revalidateTag(tag, "max")` (stale-while-revalidate).
- `revalidatePath` targets a path; `type` is required for dynamic segments; use destination paths with rewrites.
- `refresh()` and `updateTag` work only in Server Actions.
- Server Action revalidation also clears the client cache.

## Next

- [Cache Components](./05-cache-components.md)
- [07 · Server Actions](../07-server-actions/README.md)
- [Webhooks](../08-route-handlers-and-proxy/03-webhooks.md)