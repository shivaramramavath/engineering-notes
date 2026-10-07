# Streaming and Suspense

Without streaming, a dynamic page is **all or nothing**: the server waits for every query to finish, then sends the whole page. One slow query delays everything. Streaming breaks the page into chunks and sends each as soon as it is ready, so users see useful content immediately.

## The problem

```text
Without streaming
─────────────────
Request ──► [fetch user 100ms][fetch feed 2000ms][fetch ads 400ms] ──► send everything at 2.5s

With streaming
──────────────
Request ──► send shell at ~0ms (header, nav, skeletons)
        ──► user card arrives at 100ms
        ──► ads arrive at 400ms
        ──► feed arrives at 2000ms
```

Total work is the same; **perceived** performance is much better. The browser can paint the shell, and interactivity begins as parts arrive.

## How it works

Next.js uses React's `<Suspense>`. When a component inside a Suspense boundary is still waiting for data, React sends the boundary's **fallback** first. When the data resolves, the server streams the real content, and React swaps it in without a full reload. This happens over one HTTP response using chunked transfer.

## Two ways to stream

### 1. `loading.tsx`: for a whole route segment

```tsx
// app/dashboard/loading.tsx
export default function Loading() {
  return <p>Loading dashboard…</p>;
}
```

```tsx
// app/dashboard/page.tsx
export default async function Dashboard() {
  const data = await getSlowData();
  return <Chart data={data} />;
}
```

Next.js automatically wraps `page.tsx` (and its children) in `<Suspense fallback={<Loading />}>`. Behavior:

- The layout renders and is interactive immediately.
- The fallback appears instantly on navigation, before data arrives.
- Navigation is interruptible: the user can click elsewhere without waiting.
- For dynamic routes, links prefetch up to the nearest `loading.tsx`, so the skeleton appears instantly on click. See [Navigation](../02-routing/03-navigation.md).

### 2. `<Suspense>`: for individual components

`loading.tsx` covers the whole page. For finer control, wrap slow components yourself:

```tsx
// app/dashboard/page.tsx
import { Suspense } from "react";
import { Feed } from "./feed";
import { Stats } from "./stats";

export default function Dashboard() {
  return (
    <main>
      <h1>Dashboard</h1>

      <Suspense fallback={<p>Loading stats…</p>}>
        <Stats />
      </Suspense>

      <Suspense fallback={<p>Loading feed…</p>}>
        <Feed />
      </Suspense>
    </main>
  );
}
```

```tsx
// app/dashboard/feed.tsx  (async Server Component)
export async function Feed() {
  const items = await getFeed(); // slow
  return (
    <ul>
      {items.map((i) => (
        <li key={i.id}>{i.title}</li>
      ))}
    </ul>
  );
}
```

`<h1>` renders immediately. `Stats` and `Feed` stream independently and in whichever order they finish.

## The most common mistake: awaiting above the boundary

Suspense only helps for work that happens **inside** the boundary. If the parent awaits first, the page is blocked and the fallback never shows:

```tsx
// Wrong: the page waits for the data before rendering anything
export default async function Page() {
  const items = await getFeed();            // blocks the whole page
  return (
    <Suspense fallback={<p>Loading…</p>}>
      <Feed items={items} />                {/* too late: data already loaded */}
    </Suspense>
  );
}
```

```tsx
// Right: the component that needs the data fetches it
export default function Page() {
  return (
    <Suspense fallback={<p>Loading…</p>}>
      <Feed />                              {/* fetches inside */}
    </Suspense>
  );
}
```

Keep the `await` in the component that is wrapped by Suspense.

## Parallel data in a streamed page

Independent Suspense boundaries fetch **in parallel**, each starting as soon as React renders it. To avoid waterfalls inside one component, start requests together:

```tsx
const [a, b] = await Promise.all([getA(), getB()]);
```

See [Parallel and Sequential](../05-data-fetching/02-parallel-and-sequential.md).

## Where to place boundaries

| Placement | Effect |
|---|---|
| One boundary around the whole page (`loading.tsx` alone) | Simple, but everything waits for the slowest part |
| Boundary per independent slow section | Best perceived speed |
| Boundary around every tiny component | "Popcorn" effect: lots of pieces pop in, layout jumps |

Group things that should appear together; separate things that load at different speeds. Design the fallback to match the size of the final content to avoid layout shift (skeletons).

## Static shell and dynamic holes

Streaming also works with static rendering. The idea:

```text
Prerendered at build:        Streamed per request:
┌──────────────────────┐
│ Header (static)      │
│ Product (static)     │
│ [ fallback ]  ◄──────┼──── Cart / recommendations (dynamic)
│ Footer (static)      │
└──────────────────────┘
```

The static shell is served instantly from the cache, and dynamic parts fill the holes. With **Cache Components** (Next.js 16; enabled by default in new projects), this is the core model: cached and non-request-dependent parts form the shell, and anything that reads request-time data must sit inside a Suspense boundary or the build reports an error. See [Cache Components](../06-caching/05-cache-components.md).

## Streaming to Client Components

You can pass an unawaited Promise to a Client Component and read it with `use`, so the client part suspends until data arrives. See [Composition Patterns](../03-components/03-composition-patterns.md).

## Things to know

- **Status codes.** Once the first chunk is sent, the response status (200) is already committed. If a streamed part later calls `notFound()` or throws, the status cannot change, so Next.js uses client-side handling and a `noindex` tag for not-found. Resolve what must determine the status (such as "does this post exist?") before streaming starts, or in a layer above the boundary. See [Error and Not Found](../02-routing/05-error-and-not-found.md).
- **Errors in streamed parts** are caught by the nearest `error.tsx`/error boundary; the rest of the page stays.
- **JavaScript disabled or bots.** Streamed content is delivered as HTML in the same response, so it does not require client JS to appear. Crawlers receive the final content, but verify with your own tests for critical SEO pages.
- **Proxies must not buffer.** If you self-host behind Nginx or similar, response buffering delays chunks until the end and defeats streaming. Disable buffering for the app; see [Nginx](../22-production/03-nginx.md).

## Common mistakes

| Mistake | Symptom | Fix |
|---|---|---|
| `await` above the `<Suspense>` | Fallback never shown; whole page waits | Move fetching into the wrapped component |
| Only a page-level `loading.tsx` for a page with fast and slow parts | Fast parts wait for slow ones | Add component-level boundaries |
| A boundary around every component | Popcorn UI, layout shift | Group related content |
| Skeleton size differs from real content | Layout jumps | Match dimensions |
| Reading `cookies()` outside any boundary under Cache Components | Build error about uncached data outside Suspense | Wrap the request-dependent component in `<Suspense>` |
| Streaming appears to work in dev, not in production | Proxy or CDN buffering | Disable buffering on the proxy |
| Calling `notFound()` from a streamed component for status-critical checks | 200 response | Check existence before the boundary |

## Quick Summary

- Streaming sends a page in chunks so users see content as it becomes ready.
- `loading.tsx` wraps a segment in Suspense; `<Suspense>` gives per-component control.
- Fetch **inside** the suspended component; awaiting above the boundary blocks everything.
- Independent boundaries load in parallel and fail independently.
- Static shells with streamed dynamic holes are the foundation of Cache Components.
- Make sure proxies do not buffer, and resolve status-critical checks before streaming.

## Next

- [05 · Data Fetching](../05-data-fetching/README.md)
- [Parallel and Sequential](../05-data-fetching/02-parallel-and-sequential.md)
- [Cache Components](../06-caching/05-cache-components.md)