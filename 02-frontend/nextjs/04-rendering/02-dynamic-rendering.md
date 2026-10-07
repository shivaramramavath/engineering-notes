# Dynamic Rendering

Dynamic rendering produces HTML **at request time**, separately for each request. You need it when the output depends on information that exists only when a user makes the request: who they are, their cookies, the query string, or data that must be fresh every time.

> **Which model?** The list of triggers below describes the **previous model** (no `cacheComponents`), where reading request data makes the whole route dynamic. With Cache Components (default in new projects per the 16.4 docs), everything is dynamic unless cached, reading `cookies()`, `headers()` or `searchParams` only affects the component that reads it, and that component must be inside `<Suspense>` so the rest can prerender into a static shell. See [Cache Components](../06-caching/05-cache-components.md).

## How it works

```text
Request ──► server renders the route's Server Components with this request's context
        ──► streams HTML + RSC payload ──► response
```

No pre-built page is reused. Each request does real work on the server.

## What makes a route dynamic

Using any of these makes the route dynamic, because they can only be known per request:

| Trigger | Example |
|---|---|
| `cookies()` | Read a session or preference |
| `headers()` | Read user agent, IP-related headers, custom headers |
| `searchParams` page prop | `?q=shoes` |
| `connection()` | Explicitly wait for an incoming request |
| Draft mode | Previewing unpublished content |
| Uncached data fetching | Depends on version and configuration |
| `export const dynamic = "force-dynamic"` | Force the route to be dynamic |

In Next.js 15 and later, `cookies()`, `headers()`, `params` and `searchParams` are asynchronous, so you `await` them.

```tsx
// app/dashboard/page.tsx
import { cookies } from "next/headers";

export default async function Dashboard() {
  const cookieStore = await cookies();
  const session = cookieStore.get("session")?.value;
  const user = session ? await getUserBySession(session) : null;

  return <h1>{user ? `Hello ${user.name}` : "Please sign in"}</h1>;
}
```

```tsx
// app/search/page.tsx
export default async function Search({
  searchParams,
}: {
  searchParams: Promise<{ q?: string }>;
}) {
  const { q } = await searchParams;
  const results = q ? await search(q) : [];
  return <p>{results.length} results for {q}</p>;
}
```

## Forcing the behavior

Segment config exports override the default:

```tsx
export const dynamic = "force-dynamic"; // always render per request
```

| Value | Meaning |
|---|---|
| `"auto"` (default) | Let Next.js decide |
| `"force-dynamic"` | Always dynamic |
| `"force-static"` | Always static; dynamic APIs return empty values |
| `"error"` | Static only; error if anything dynamic is used |

For a single component, a more precise tool is `connection()`:

```tsx
import { connection } from "next/server";

export default async function Page() {
  await connection(); // everything after this runs at request time
  const random = Math.random();
  return <p>{random}</p>;
}
```

These exports belong to the previous model. With **Cache Components** enabled, exporting `dynamic`, `revalidate` or `fetchCache` fails the build; remove them and control caching with `use cache` instead (uncached data and request-time APIs already run per request). See [Cache Components](../06-caching/05-cache-components.md).

## The cost of going dynamic

A dynamic route does work on every request:

- The server renders each time. Time to first byte includes data fetching.
- It cannot be served from a CDN as a prebuilt file (unless you cache parts explicitly).
- Everything the route renders waits on the slowest data it awaits, unless you stream.

Streaming is the main way to reduce the sting: send the shell right away and let slow parts arrive later. See [Streaming and Suspense](./03-streaming-and-suspense.md).

## Scope matters: dynamic spreads upward

A dynamic API in a **layout** makes every route using that layout dynamic. A dynamic API deep in a leaf component makes that route dynamic unless it is isolated behind a Suspense boundary or cached.

```text
app/layout.tsx          cookies() here  → whole app dynamic
app/blog/page.tsx       static otherwise
app/dashboard/page.tsx  cookies() here  → only dashboard dynamic
```

Rules of thumb:

- Read request data **as low in the tree as possible**.
- Isolate dynamic parts in small components wrapped in `<Suspense>`.
- Do not read cookies in the root layout just to know "is the user logged in?" if a smaller component can do it.

## Dynamic data without dynamic rendering

Some "dynamic" needs can stay out of the render path:

| Need | Alternative that keeps the page static |
|---|---|
| Show the user's name | Fetch it in a small Client Component after load |
| Frequently changing but shared data | Static + short revalidation interval |
| Filter or sort a list | Client-side state or a Client Component reading `useSearchParams` |
| Personalized widget on a mostly static page | Static shell with a streamed dynamic part |

The right trade-off depends on whether freshness or speed matters more for that page.

## Request memoization within a render

Inside one dynamic render, identical `fetch` calls (same URL and options) are deduplicated, and non-`fetch` functions can be wrapped with React's `cache()`. This lets layouts and pages ask for the same data without extra requests. See [Server Fetching](../05-data-fetching/00-server-fetching.md).

## Runtime

Server rendering runs in the Node.js runtime by default. Route-level `runtime = "edge"` is available for some routes but restricts available Node APIs and is rarely necessary. Prefer the default unless you have a specific latency reason.

## Debugging

| Symptom | Likely cause | Fix |
|---|---|---|
| Route shows `ƒ` but should be static | A dynamic trigger somewhere in the route or its layouts | Search for `cookies`, `headers`, `searchParams`, `connection`, `force-dynamic` |
| Error: route couldn't be rendered statically because it used `cookies` | A static-forcing option plus a dynamic API | Remove the force-static, or the dynamic call |
| Page always slow | Awaiting slow data before anything is sent | Stream with Suspense; cache what is shared |
| Values stuck at build time on a "dynamic" page | Route is actually static | Read request data or use `connection()` |
| Different users see each other's data | Data cached when it should be per-user | Do not cache user-specific data in shared caches |

## Common mistakes

| Mistake | Symptom | Fix |
|---|---|---|
| Reading `cookies()` in the root layout | Whole app dynamic | Move it lower, wrap in Suspense |
| Forgetting to `await` `cookies()`/`searchParams` (Next 15+) | Type error / warning | `await` them |
| `force-dynamic` on everything "to be safe" | Lost static performance | Only force what needs it |
| Putting user-specific output into a cached or static render | Data shown to the wrong user | Keep personal data in dynamic, uncached rendering |
| Using `searchParams` for UI-only state | Needless dynamic rendering | Use `useSearchParams` in a Client Component |
| Expecting `Math.random()`/`Date.now()` to vary on a static route | Same value for everyone | Use `connection()` or a dynamic trigger |

## Quick Summary

- Dynamic rendering builds HTML per request; needed for personalized or always-fresh output.
- Triggers: `cookies()`, `headers()`, `searchParams`, `connection()`, draft mode, uncached data, `force-dynamic`.
- Request APIs are async in Next.js 15+.
- Dynamic spreads upward from layouts; read request data low in the tree and isolate it with Suspense.
- Stream dynamic routes so the shell appears immediately.
- Route-level options like `dynamic` do not apply when Cache Components is enabled.

## Next

- [Streaming and Suspense](./03-streaming-and-suspense.md)
- [Server Fetching](../05-data-fetching/00-server-fetching.md)
- [Cache Components](../06-caching/05-cache-components.md)