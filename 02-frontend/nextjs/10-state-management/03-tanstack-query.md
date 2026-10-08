# TanStack Query

TanStack Query (React Query) is a **client-side cache for server data**. It adds things a plain `fetch` in a component does not: shared cache by key, request deduplication, background refetching, retries, polling, infinite scroll and optimistic mutations.

> Verified against the Next.js 16.4 guide "Client-side data fetching with TanStack Query" and the TanStack Query Advanced SSR guide. API names are for TanStack Query v5; check your installed version.

## Do you need it?

The Next.js docs are explicit: **many apps can provide responsive interactions without a client data-fetching library.** If a Client Component only needs to read server data once, fetch it in a Server Component and pass a Promise (unwrapped with `use()`) or plain props.

Reach for TanStack Query when Client Components need a **shared browser cache** with behavior such as:

| Need | Why a query cache helps |
|---|---|
| Polling / live-ish data | `refetchInterval` |
| Revalidate on window focus or reconnect | Built in |
| Infinite scroll, "load more" | `useInfiniteQuery` |
| Autocomplete / search-as-you-type | Cache per query key, cancel stale requests |
| The same data read by many client components | One request, one cache entry |
| Optimistic updates across several components | Update the shared cache, roll back on error |
| Data not needed for the first paint | Fetch after hydration |

If your data is read once on the server and refreshed by Server Action revalidation, you probably do not need it. See [State Overview](./00-state-overview.md) and [Client Fetching](../05-data-fetching/01-client-fetching.md).

## Install

```bash
npm install @tanstack/react-query
npm install -D @tanstack/react-query-devtools   # optional
```

## Provider setup

A new `QueryClient` per **server render**, one shared client in the **browser**:

```tsx
// app/providers.tsx
"use client";

import type { ReactNode } from "react";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";

let browserQueryClient: QueryClient | undefined;

function getQueryClient() {
  if (typeof window === "undefined") {
    return new QueryClient();                 // server: isolated per request
  }
  browserQueryClient ??= new QueryClient();   // browser: reused across renders
  return browserQueryClient;
}

export function Providers({ children }: { children: ReactNode }) {
  return <QueryClientProvider client={getQueryClient()}>{children}</QueryClientProvider>;
}
```

```tsx
// app/products/layout.tsx  (Server Component)
import { Providers } from "./providers";

export default function Layout({ children }: { children: React.ReactNode }) {
  return <Providers>{children}</Providers>;
}
```

Why not `useState(() => new QueryClient())`? If React suspends during the initial render, state can be thrown away and the client recreated. A browser singleton avoids that. Mount the provider on the layout of the section that uses it, not necessarily at the root.

## Fetching on the client

Query functions must run in the browser, so they call a **Route Handler** (or another HTTP API), not a database directly:

```tsx
"use client";

import { useQuery } from "@tanstack/react-query";

type Product = { id: string; name: string };

async function searchProducts(query: string): Promise<Product[]> {
  const res = await fetch(`/api/products?query=${encodeURIComponent(query)}`);
  if (!res.ok) throw new Error("Failed to fetch products");     // fetch does not throw on 4xx/5xx
  return res.json();
}

export function ProductAutocomplete({ query }: { query: string }) {
  const { data = [], error, isPending } = useQuery({
    queryKey: ["product-search", query],
    queryFn: () => searchProducts(query),
    enabled: query.length > 0,                    // do not run until there is input
  });

  if (!query) return null;
  if (error) return <p>Failed to load products.</p>;
  if (isPending) return <p>Loading products…</p>;

  return <ul>{data.map((p) => <li key={p.id}>{p.name}</li>)}</ul>;
}
```

The endpoint is a [Route Handler](../08-route-handlers-and-proxy/00-route-handlers.md); authenticate and validate there.

### The pieces

| Piece | Meaning |
|---|---|
| `queryKey` | The cache identity. Include **every value the query depends on** (`["product-search", query]`). Changing it fetches a new entry |
| `queryFn` | Returns a Promise of data. **Throw on failure** |
| `enabled` | Gate the query (dependent queries, empty input) |
| `staleTime` | How long data counts as fresh (default **0**: stale immediately, so mounting triggers a background refetch) |
| `gcTime` | How long **unused** cache entries are kept (default 5 minutes) |

### Status flags (v5)

| Flag | Meaning |
|---|---|
| `isPending` | No data yet (first load) |
| `isFetching` | A request is in flight (including background refetch) |
| `isError` / `error` | Last attempt failed (after retries; the default is 3 retries with backoff) |
| `isSuccess` | Has data |

Use `isPending` for the first-load skeleton and `isFetching` for a subtle "refreshing" indicator.

## Suspense: `useSuspenseQuery`

When a nearby `<Suspense>` boundary should own the loading UI, use `useSuspenseQuery`. `data` is always defined, and errors go to the nearest error boundary.

```tsx
"use client";

import { Suspense } from "react";
import { useSuspenseQuery } from "@tanstack/react-query";

export function ProductAutocomplete({ query }: { query: string }) {
  if (!query) return null;
  return (
    <Suspense fallback={<p>Loading products…</p>}>
      <ProductResults query={query} />
    </Suspense>
  );
}

function ProductResults({ query }: { query: string }) {
  const { data } = useSuspenseQuery({
    queryKey: ["product-search", query],
    queryFn: () => searchProducts(query),
  });
  return <ul>{data.map((p) => <li key={p.id}>{p.name}</li>)}</ul>;
}
```

Notes from the docs:

- Later refetches of a query that already has data keep showing it; the fallback appears only for the first load. Use `isFetching` for background feedback.
- Multiple `useSuspenseQuery` calls in **one component run sequentially** (a waterfall). Put independent queries in sibling components, or use `useSuspenseQueries`.

## Share the key and options: `queryOptions`

Keep the key, function and settings together so every call site (server prefetch, client component, mutation) uses the same identity:

```ts
// app/products/[id]/product-cache.ts  (no server-only or client-only imports)
import { queryOptions } from "@tanstack/react-query";

export type Product = { id: string; name: string };

export const productCache = {
  key: (id: string) => ["product", id] as const,
  tag: (id: string) => `product:${id}`,               // the matching Next.js cache tag
  options: (id: string) =>
    queryOptions({
      queryKey: productCache.key(id),
      queryFn: async (): Promise<Product> => {
        const res = await fetch(`/api/products/${id}`);
        if (!res.ok) throw new Error("Failed to fetch product");
        return res.json();
      },
      staleTime: 30_000,
    }),
};
```

`queryOptions` gives type inference across `useQuery`, `prefetchQuery` and `getQueryData`.

## Initial data from a Server Component

To render the first paint with data and let TanStack Query take over in the browser, **prefetch on the server and hydrate**:

```tsx
// app/products/[id]/page.tsx  (Server Component)
import { Suspense } from "react";
import { defaultShouldDehydrateQuery, dehydrate, HydrationBoundary, QueryClient } from "@tanstack/react-query";
import { getProduct } from "./data";
import { productCache } from "./product-cache";
import { ProductView } from "./product-view";

export default function Page({ params }: { params: Promise<{ id: string }> }) {
  return (
    <Suspense fallback={<p>Loading…</p>}>
      {params.then(({ id }) => <ProductData id={id} />)}
    </Suspense>
  );
}

function ProductData({ id }: { id: string }) {
  const queryClient = new QueryClient();

  // Not awaited: rendering is not blocked, and the pending query streams to the client.
  void queryClient.prefetchQuery({
    ...productCache.options(id),
    queryFn: () => getProduct(id),            // server override: call the data function directly
  });

  return (
    <HydrationBoundary
      state={dehydrate(queryClient, {
        shouldDehydrateQuery: (query) =>
          defaultShouldDehydrateQuery(query) || query.state.status === "pending",
      })}
    >
      <ProductView id={id} />
    </HydrationBoundary>
  );
}
```

```tsx
// app/products/[id]/product-view.tsx
"use client";

import { useSuspenseQuery } from "@tanstack/react-query";
import { productCache } from "./product-cache";

export function ProductView({ id }: { id: string }) {
  const { data } = useSuspenseQuery(productCache.options(id));   // same key as the server
  return <h1>{data.name}</h1>;
}
```

Requirements and why:

- **Same query key** on the server and client, which is why options live in one shared module.
- **Override `queryFn` on the server.** The client version fetches a relative URL (`/api/products/1`), which does not resolve on the server; call the data function directly.
- **`staleTime` above 0** (for example 30 seconds) so the client does not refetch the instant it hydrates.
- Dehydrating `pending` queries (TanStack Query 5.40+) lets you **not await** the prefetch and still stream the result.
- A new `QueryClient` per request on the server; never a shared one.
- **Await** (and dehydrate only successes) instead of streaming when you need the data in the initial HTML for a crawler or if you prefer simple blocking behavior.

Pitfalls from the TanStack docs:

- Do **not** render the same data in both a Server Component and a Client Component from the query; the Server Component output cannot be revalidated by the client cache and the two can drift. Use the Server Component to **prefetch only**.
- Awaiting several prefetches in sequence creates a server-side waterfall; start them without awaiting, or prefetch in parallel.
- Do **not use a Server Action as a `queryFn`**. Actions run one at a time, so queries can stay pending, and passing the action reference can fail. Use a Route Handler for reads. Server Actions are fine for **mutations**.
- Old TypeScript or `@types/react` versions can complain about async Server Components; update them.

## Mutations

```tsx
"use client";

import { useMutation, useQueryClient } from "@tanstack/react-query";
import { renameProductAction } from "./actions";    // a Server Action
import { productCache } from "./product-cache";

export function RenameForm({ id, name }: { id: string; name: string }) {
  const queryClient = useQueryClient();

  const rename = useMutation({
    mutationFn: (newName: string) => renameProductAction(id, newName),

    onMutate: async (newName) => {                              // optimistic update
      const key = productCache.key(id);
      await queryClient.cancelQueries({ queryKey: key });       // stop in-flight refetches overwriting it
      const previous = queryClient.getQueryData<{ id: string; name: string }>(key);
      queryClient.setQueryData(key, (old: typeof previous) => (old ? { ...old, name: newName } : old));
      return { previous };
    },
    onError: (_err, _vars, ctx) => {                            // roll back
      queryClient.setQueryData(productCache.key(id), ctx?.previous);
    },
    onSettled: () => {                                          // reconcile with the server
      void queryClient.invalidateQueries({ queryKey: productCache.key(id) });
    },
  });

  return (
    <form action={(fd) => rename.mutate(String(fd.get("name")))}>
      <input name="name" defaultValue={name} />
      <button disabled={rename.isPending}>Save</button>
      {rename.isError && <p role="alert">Could not save</p>}
    </form>
  );
}
```

The Server Action writes to the database and invalidates the **server** cache:

```ts
// app/products/[id]/actions.ts
"use server";

import { updateTag } from "next/cache";
import { productCache } from "./product-cache";

export async function renameProductAction(id: string, name: string) {
  // authenticate, validate, authorize, write...
  updateTag(productCache.tag(id));   // next server read of the cached data is fresh
}
```

## Two caches, one mutation

With Next.js you can have **three** caches holding related data:

| Layer | Holds | Freshness control |
|---|---|---|
| Next.js server cache | Cached data and Server Component output | `cacheLife` (`revalidate`, `expire`) |
| Next.js client cache | RSC payloads for visited/prefetched routes | `cacheLife` `stale` |
| TanStack Query cache | Browser data under a query key | `staleTime`, `invalidateQueries`, mutations |

They keep **independent freshness policies** and need not match in duration, but a mutation must invalidate each layer that holds the data:

- TanStack Query: `setQueryData` / `invalidateQueries` for the browser copy.
- Next.js: `updateTag` / `revalidateTag` / `revalidatePath` inside the Server Action for the server copies.

Define the query key and the server tag together (as `productCache` above) so they cannot drift. If the server read is not cached, there is no server tag to invalidate. See [Revalidation](../06-caching/04-revalidation.md).

## With Cache Components

When `cacheComponents` is enabled, Next.js also **prerenders Client Components**, which affects TanStack Query:

- Keep queries needed for the initial render **behind `<Suspense>`**. TanStack Query reads the current time while creating active query state; the boundary lets Next.js defer that work instead of raising a "current time" prerender error.
- `dehydrate()` also reads `Date.now()` during prerendering. For **cached** initial data, the Next.js guide shows a custom hydration helper that caches only the timestamp (with `use cache` and the same tags as the data), builds the dehydrated state by hand, and keeps data and timestamp advancing together. Use that pattern rather than the plain `dehydrate()` call. See [Cache Components](../06-caching/05-cache-components.md) and the Next.js guide for the helper.
- A `cacheLife` on the server read and a `staleTime` in the query are separate settings.

## Other patterns

### Dependent queries

```tsx
const { data: user } = useQuery(userOptions(id));
const { data: orders } = useQuery({
  ...ordersOptions(user?.id ?? ""),
  enabled: !!user?.id,
});
```

### Infinite scroll

```tsx
const { data, fetchNextPage, hasNextPage, isFetchingNextPage } = useInfiniteQuery({
  queryKey: ["posts"],
  queryFn: ({ pageParam }) => fetch(`/api/posts?cursor=${pageParam}`).then((r) => r.json()),
  initialPageParam: "",                                  // required in v5
  getNextPageParam: (lastPage) => lastPage.nextCursor ?? undefined,
});
```

### Polling

```tsx
useQuery({ queryKey: ["status"], queryFn: fetchStatus, refetchInterval: 5000 });
```

### Devtools

```tsx
import { ReactQueryDevtools } from "@tanstack/react-query-devtools";

<QueryClientProvider client={client}>
  {children}
  <ReactQueryDevtools initialIsOpen={false} />
</QueryClientProvider>
```

Devtools are excluded from production builds by default.

## Query key design

- Start with a stable resource name, then parameters: `["products"]`, `["products", { q, page }]`, `["product", id]`.
- Include **everything the function reads**; the key is the cache identity.
- Use arrays of primitives and plain objects. Object property order does not matter in keys.
- Invalidating `["products"]` also invalidates keys that **start with** it.

## Debugging

| Symptom | Likely cause | Fix |
|---|---|---|
| Client refetches immediately after hydrating | `staleTime` is 0 | Set `staleTime` on the options |
| Data fetched twice (server and client) | Keys differ between server prefetch and client query | Share `queryOptions` |
| Server prefetch fails with "Invalid URL" | The client `queryFn` uses a relative URL on the server | Override `queryFn` on the server |
| `useQuery` data not in the server HTML | Only `useSuspenseQuery` suspends for server render | Use `useSuspenseQuery` with prefetch/hydration |
| Pending queries stuck forever | A Server Action used as `queryFn` | Use a Route Handler |
| Stale data after a mutation | Only one cache invalidated | Invalidate TanStack Query **and** the Next.js tag |
| Prerender error about the current time | Query created outside Suspense under Cache Components | Wrap in `<Suspense>`; use the prerenderable hydration helper |
| Optimistic update flickers back | A refetch overwrote it | `cancelQueries` in `onMutate` |
| One request per keystroke | No debounce | Debounce the input, or rely on `enabled` and keys |
| Errors silently swallowed | `queryFn` returns an error body instead of throwing | Throw when `!res.ok` |

## Common mistakes

| Mistake | Fix |
|---|---|
| Installing it when Server Components would do | Fetch on the server; add it only for browser-side behavior |
| `fetch` without checking `res.ok` | Throw so the query enters the error state |
| One shared `QueryClient` on the server | New client per request |
| Different keys for the same data | Shared `queryOptions` module |
| Rendering the same query in Server and Client Components | Prefetch on the server, render on the client |
| Putting query results into Zustand/Context | Read them with `useQuery` where needed |
| Forgetting `enabled` for dependent queries | Gate with `enabled` |
| `staleTime: 0` after server hydration | Set a sensible `staleTime` |
| Mutating the server but not the Next.js cache | `updateTag` / `revalidateTag` in the action |
| Leaving `gcTime` and `staleTime` confused | `staleTime` = freshness; `gcTime` = how long unused entries are kept |

## Quick Summary

- TanStack Query is a browser cache for server data; add it for polling, infinite scroll, shared client caching and optimistic updates, not by default.
- New `QueryClient` per server request, one singleton in the browser; query functions call Route Handlers.
- Share keys and options (`queryOptions`) between the server prefetch and client hooks; set `staleTime` above 0 when hydrating.
- Prefetch in Server Components without awaiting, dehydrate pending queries, and hydrate with `HydrationBoundary`; do not render the same query in both component types.
- Use Server Actions for mutations, not reads; update the TanStack cache **and** invalidate the Next.js tag.
- Under Cache Components, keep initial-render queries behind `<Suspense>` and use the prerenderable hydration pattern for cached data.

## Next

- [11 · Authentication](../11-authentication/README.md)
- [Client Fetching](../05-data-fetching/01-client-fetching.md)
- [Revalidation](../06-caching/04-revalidation.md)
