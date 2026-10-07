# Cache Components

Cache Components is the current caching model in Next.js 16. Everything is **dynamic by default**, and you choose what to cache with the `use cache` directive. Next.js prerenders a **static shell** from the parts it can (static content and cached results) and streams the rest at request time. Together this is called **Partial Prerendering (PPR)**.

> Verified against the Next.js 16.4 docs. New projects from `create-next-app` with the recommended defaults have Cache Components and Partial Prefetching enabled. The docs say both will be enabled everywhere in the next major release.

## Enabling it

```ts
// next.config.ts
import type { NextConfig } from "next";

const nextConfig: NextConfig = {
  cacheComponents: true,
  partialPrefetching: true,
};

export default nextConfig;
```

Set `partialPrefetching` explicitly; leaving it unset logs a warning. Requirements and limits:

- Requires the **Node.js runtime**. `runtime = "edge"` is not supported.
- Not available with static export (`output: "export"`).
- Replaces `experimental.dynamicIO`, `experimental.useCache` and `experimental.ppr` (the old `experimental_ppr` segment export is removed).

## The model in one page

```text
Component does...                                  → Result
────────────────────────────────────────────────────────────────────────
pure rendering / imports / sync reads              → in the static shell
use cache                                          → cached, in the shell (if long-lived)
uncached data or cookies()/headers()/searchParams  → must be inside <Suspense>
                                                     (fallback in shell, content streams)
```

## `use cache`

Add the directive to an **async** function, component, or whole file:

```tsx
import { cacheLife } from "next/cache";

// Data-level
export async function getUsers() {
  "use cache";
  cacheLife("hours");
  return db.query("SELECT * FROM users");
}

// UI-level
export default async function Page() {
  "use cache";
  cacheLife("hours");

  const users = await db.query("SELECT * FROM users");
  return (
    <ul>
      {users.map((u) => (
        <li key={u.id}>{u.name}</li>
      ))}
    </ul>
  );
}
```

At the top of a file (`"use cache"` before any code), every exported function is cached and must be async. On a `page` or `layout`, each route segment is cached independently, so to prerender a whole route add it to every segment file the route renders (page, layout, parallel slots).

Pair every `use cache` with an explicit [`cacheLife`](./04-revalidation.md) so the lifetime is obvious at the call site; without one, the `default` profile (5 min stale, 15 min revalidate, never expires) applies. Add `cacheTag` for on-demand invalidation:

```tsx
import { cacheLife, cacheTag } from "next/cache";

async function BlogPosts() {
  "use cache";
  cacheLife("hours");
  cacheTag("posts");

  const res = await fetch("https://api.example.com/posts");
  const posts: { id: string; title: string }[] = await res.json();
  return (
    <ul>
      {posts.map((p) => (
        <li key={p.id}>{p.title}</li>
      ))}
    </ul>
  );
}
```

### Cache keys

An entry is keyed by:

1. The build id (or `deploymentId`), so each new deployment starts with an empty cache
2. A hash of the function's location and signature
3. Its serializable **arguments** (props for components)
4. Values it captures from the enclosing scope, which are bound as arguments automatically

Different arguments produce separate entries.

### What can be passed in and returned

Arguments use the more restrictive Server Component serialization; return values use Client Component serialization.

| Arguments | Return values |
|---|---|
| Primitives, plain objects, arrays | Same |
| `Date`, `Map`, `Set`, typed arrays, `ArrayBuffer` | Same |
| React elements and Server Actions, **pass-through only** (do not read them) | Same, plus JSX elements |
| Not supported: class instances, functions (except pass-through), symbols, `WeakMap`/`WeakSet`, `URL` | Same unsupported types |

So you can return JSX but you cannot accept JSX as an argument and inspect it. Passing `children` through is fine:

```tsx
async function CachedWrapper({ children }: { children: React.ReactNode }) {
  "use cache";
  // do not read or modify children; only render them
  return (
    <div className="wrapper">
      <header>Cached header</header>
      {children}
    </div>
  );
}
```

The `children` can be dynamic and do not affect the cache entry.

## Streaming uncached data

For data that must be fresh on every request, do **not** cache it. Wrap the component in `<Suspense>` so the rest of the page can prerender:

```tsx
import { Suspense } from "react";

async function LatestPosts() {
  const res = await fetch("https://api.example.com/posts");
  const posts: { id: string; title: string }[] = await res.json();
  return (
    <ul>
      {posts.map((p) => (
        <li key={p.id}>{p.title}</li>
      ))}
    </ul>
  );
}

export default function Page() {
  return (
    <>
      <h1>My Blog</h1> {/* static shell */}
      <Suspense fallback={<p>Loading posts…</p>}>
        <LatestPosts /> {/* streams at request time */}
      </Suspense>
    </>
  );
}
```

`<Suspense>` supplies fallback UI but does not itself make something dynamic. Purely synchronous work still completes during prerender, with or without a boundary. Use error boundaries (`error.tsx` or `catchError`) to contain failures.

## Runtime data: `cookies()`, `headers()`, `searchParams`, `params`

These are only known per request, so components that read them belong inside `<Suspense>`:

```tsx
import { cookies } from "next/headers";
import { Suspense } from "react";

async function UserGreeting() {
  const theme = (await cookies()).get("theme")?.value ?? "light";
  return <p>Your theme: {theme}</p>;
}

export default function Page() {
  return (
    <>
      <h1>Dashboard</h1>
      <Suspense fallback={<p>Loading…</p>}>
        <UserGreeting />
      </Suspense>
    </>
  );
}
```

Reading `cookies()` here does **not** make the whole route dynamic; only `UserGreeting` streams.

**You cannot call `cookies()`, `headers()` or read `searchParams` inside a `use cache` scope**, even indirectly through a helper. Read them outside, then pass the values as arguments (they become part of the cache key):

```tsx
async function ProfileContent() {
  const session = (await cookies()).get("session")?.value;
  return <CachedContent sessionId={session!} />;
}

async function CachedContent({ sessionId }: { sessionId: string }) {
  "use cache";
  cacheLife("minutes");
  const data = await fetchUserData(sessionId);
  return <div>{data}</div>;
}
```

Because this depends on request data, it is not part of the static shell. At runtime it is cached in memory by default, which does not persist on serverless; prefer `use cache: remote` there.

### Maximizing the shell: await as deep as possible

The deeper the async or runtime access sits, the more of the page can be prerendered. If a layout does `const { slug } = await params` at the top, it cannot prerender. Pass the `params` promise down and `await` it inside a `<Suspense>`d child instead, so the sidebar and children stay in the shell.

For routes using `generateStaticParams`, follow the ISR guidance in [Full Route Cache](./02-full-route-cache.md). Client hooks follow the same rule: `useSearchParams` always needs a `<Suspense>` boundary, and `usePathname`, `useParams`, and `useSelectedLayoutSegment(s)` need one when the route has dynamic params not yet known.

## `use cache` variants

| Directive | Where results live | Use when |
|---|---|---|
| `"use cache"` | In-memory per instance; static shell at build | Default |
| `"use cache: remote"` | A durable cache handler shared across instances | Serverless runtime caching with a high hit rate (costs a network round trip and platform fees) |
| `"use cache: private"` | Browser only (client cache), part of per-link prefetch | When you cannot refactor to pass runtime values as arguments, or for compliance reasons |

## Random values and timestamps

`Math.random()`, `Date.now()`, `new Date()` and `crypto.randomUUID()` differ on each call, so Cache Components makes you decide what you mean. For a unique value per request, defer to request time and wrap in Suspense:

```tsx
import { connection } from "next/server";
import { Suspense } from "react";

async function UniqueContent() {
  await connection();
  return <p>Request ID: {crypto.randomUUID()}</p>;
}

export default function Page() {
  return (
    <Suspense fallback={<p>Loading…</p>}>
      <UniqueContent />
    </Suspense>
  );
}
```

Or cache the result so everyone shares one value until revalidation. These calls during prerender are **errors** that cannot be deferred; `performance.now()` is exempt (telemetry).

Predictable values (module imports, synchronous file reads, pure computation) are prerendered automatically. For files that never change per request, read them once at module scope.

## Prerendering and lifetimes

A cache that is too short-lived to store safely leaves a hole that resolves at request time:

- `revalidate: 0`, or `expire` under 5 minutes → excluded from prerenders
- `stale` under 30 seconds → excluded from prerenders
- `stale` between 30 s and 5 minutes → in prerenders but not the App Shell

Of the presets, only `seconds` is affected. Nesting a short-lived `use cache` inside another without an explicit `cacheLife` throws during prerendering, to prevent the outer cache silently becoming short-lived; add an explicit `cacheLife` to the outer scope.

When nesting, an explicit outer `cacheLife` always wins. Without one, shorter inner lifetimes can reduce the outer (default) lifetime, but longer inner lifetimes cannot extend it.

## Where it is stored and what survives

| Environment | Runtime behavior of default `use cache` |
|---|---|
| Self-hosted | Entries persist across requests; size limited by `cacheMaxMemorySize` |
| Serverless | Entries typically do not persist across requests; build-time caching works normally |

All stores are scoped to one deployment: a new deploy starts fresh, even for `remote`. Configure a custom `cacheHandlers` entry to change the storage. For data that must survive deploys, the `fetch` data cache and `unstable_cache` still do.

## Other behavior to know

- **Draft Mode:** cached functions re-execute on every request and results are not saved. You may read `draftMode().isEnabled` inside a `use cache` scope, but not toggle it.
- **`React.cache` isolation:** each `use cache` scope has its own `React.cache`; pass data into a cached scope through arguments.
- **State preservation:** recently visited routes stay mounted but hidden with React `<Activity>`, so state persists across navigations. See [Router Cache](./03-router-cache.md).
- **Instant navigation validation:** in dev, Next.js validates that navigating into each route renders instantly and shows insights (such as "blocking route") in the overlay. They do not change the HTTP response. To defer one, set `export const instant = false` on the segment.

## Migrating from route segment config

| Previous | With Cache Components |
|---|---|
| `dynamic = "force-dynamic"` | Remove it; uncached data and runtime APIs already run at request time |
| `dynamic = "force-static"` | Remove it; add `use cache` + `cacheLife("max")`. Runtime APIs now return real values |
| `dynamic = "error"` | Remove it; make caching explicit; optionally `ensureStatic = "navigation"` (16.4+) |
| `revalidate = 3600` | `use cache` + `cacheLife("hours")` |
| `revalidate = 0` | Remove it |
| `fetchCache` | Remove it; cache data by wrapping it in `use cache` |
| `fetch(url, { cache, next })` | `use cache` + `cacheLife` + `cacheTag` |
| `unstable_cache` | Keeps working; migrate later if you like |
| `noStore()` / `unstable_noStore` | Remove it; uncached is the default |
| `dynamicParams` | Not supported; delete it and use `notFound()` |
| `generateStaticParams` returning `[]` | Must return at least one param |
| `runtime = "edge"` | Not supported; use Node.js (or Proxy for edge behavior) |
| `experimental_ppr` | Removed; `cacheComponents` enables PPR |
| `GET` Route Handler with `force-static` | Remove it; call a cached helper marked `use cache` |

After enabling the flag, segments that still export `dynamic`, `revalidate` or `fetchCache` fail the build. To adopt incrementally, enable the flag, fix the config exports, then use `instant = false` (a codemod can add it across the app) and convert routes one at a time. Synchronous IO such as `new Date()` during prerender cannot be deferred and must be fixed up front.

`generateMetadata` and `generateViewport` follow the same rules: cache external data with `use cache`; if metadata truly needs runtime data, add a dynamic marker component to the page.

## Debugging

```bash
NEXT_PRIVATE_DEBUG_CACHE=1 npm run dev
# or: NEXT_PRIVATE_DEBUG_CACHE=1 npm run start
```

In dev, console logs from cached functions appear with a `Cache` prefix.

| Symptom | Cause | Fix |
|---|---|---|
| `next-request-in-use-cache` error | `cookies()`/`headers()` read inside a cached scope (or a helper it calls) | Read outside; pass values as arguments. This can pass `next build` and fail at runtime on a dynamic route |
| Build hangs ~50 s then times out ("Filling a cache during prerender timed out") | A Promise for runtime/uncached data was passed into a cached scope (props, closure, shared `Map`) | Await the value outside and pass the plain value |
| Blocking-route insight | Uncached data or runtime API outside `<Suspense>` | Cache it, or wrap in `<Suspense>` |
| `dynamicParams` error | Not compatible with `cacheComponents` | Delete it |
| Cache always misses on serverless | In-memory entries do not persist | `use cache: remote` or a cache handler |
| Cache empty after deploy | Key includes the build id | Expected; cache warms again |
| Serialization error | Class instance or function passed as an argument | Pass plain data |

## Common mistakes

| Mistake | Fix |
|---|---|
| `use cache` on a non-async function | Make it async |
| No `cacheLife` | Set one explicitly in every scope |
| Caching per-user data in a shared scope without the user in the key | Pass the id as an argument, or leave it uncached |
| Awaiting `params`/`cookies()` at the top of a layout | Push the await down into a Suspense-wrapped child |
| Mixing advice from the previous model | Remove `dynamic`/`revalidate`/`fetchCache` exports |
| Expecting runtime caching to work on serverless without `remote` | Use `use cache: remote` |

## Quick Summary

- Cache Components: dynamic by default; opt in with `use cache`; Next.js prerenders a static shell and streams the rest (PPR).
- Always set `cacheLife`; add `cacheTag` for on-demand invalidation.
- Runtime data (`cookies`, `headers`, `searchParams`) goes inside `<Suspense>` and cannot be read inside `use cache`; pass values as arguments.
- Route segment config (`dynamic`, `revalidate`, `fetchCache`, `dynamicParams`) is replaced; Node.js runtime only.
- Entries are per deployment and in-memory by default; use `use cache: remote` or a cache handler on serverless.
- Use `NEXT_PRIVATE_DEBUG_CACHE=1` and the dev overlay insights to understand behavior.

## Next

- [Revalidation](./04-revalidation.md)
- [07 · Server Actions](../07-server-actions/README.md)
- [Streaming and Suspense](../04-rendering/03-streaming-and-suspense.md)