# TanStack Query

TanStack Query (formerly React Query) is a **server-state cache for React**. You describe *what* data you want (a **key**) and *how* to get it (a **function**). It handles fetching, caching, deduplication, background refreshing, retries, and cleanup, so everything [01](./01-fetching-data.md) listed as missing is built in.

This note covers **v5** (`@tanstack/react-query`).

```bash
npm install @tanstack/react-query
npm install -D @tanstack/react-query-devtools
```

## Setup

Create **one** `QueryClient` and provide it at the root:

```tsx
// main.tsx
import { QueryClient, QueryClientProvider } from "@tanstack/react-query"
import { ReactQueryDevtools } from "@tanstack/react-query-devtools"

const queryClient = new QueryClient({
  defaultOptions: {
    queries: {
      staleTime: 30_000,            // see 04: how long data counts as fresh
    },
  },
})

createRoot(document.getElementById("root")!).render(
  <QueryClientProvider client={queryClient}>
    <App />
    <ReactQueryDevtools initialIsOpen={false} />
  </QueryClientProvider>
)
```

The client **holds the cache**. Create it at module level (as above) or once in `useState(() => new QueryClient())` inside a component. Never `new QueryClient()` directly in a render body: you'd create a fresh, empty cache every render.

The devtools (which are excluded from production builds by default) show every query, its status, staleness, and data. Keep them open while learning; they make the cache visible.

## Your first query

```tsx
import { useQuery } from "@tanstack/react-query"

function ProjectList() {
  const { data, isPending, isError, error } = useQuery({
    queryKey: ["projects"],
    queryFn: () => projectsApi.list(),
  })

  if (isPending) return <Skeleton />
  if (isError) return <ErrorState error={error} />
  return <ul>{data.map((p) => <li key={p.id}>{p.name}</li>)}</ul>
}
```

Two required options:

- **`queryKey`**: an array that **identifies** this data in the cache.
- **`queryFn`**: a function returning a promise of the data. **It must throw (or reject) on failure.** TanStack Query only knows a request failed if the promise rejects. A raw `fetch` doesn't reject on 404/500, so use a client that checks `res.ok` ([11 — API client](../11-api-integration/02-api-client.md)).

Render states are covered in [02](./02-loading-and-error-states.md).

## Query keys

The key is the **cache identity** and the **dependency list**. Anything the query function uses to decide *what* to fetch must be in the key:

```ts
useQuery({ queryKey: ["project", projectId], queryFn: () => projectsApi.get(projectId) })
useQuery({ queryKey: ["projects", { status, page }], queryFn: () => projectsApi.list({ status, page }) })
```

- When a key part changes, it's a **different query**: its own cache entry, fetched on demand. Going back to a previous key serves from cache.
- Keys are compared by **deep value**, and object property order doesn't matter: `["projects", { a: 1, b: 2 }]` equals `["projects", { b: 2, a: 1 }]`. Array **order** does matter.
- Keys must be JSON-serializable.
- They're hierarchical (general → specific), which powers [partial invalidation](./04-caching-and-synchronization.md#invalidation): invalidating `["projects"]` hits every key that starts with it.

The rule that prevents most bugs: **if the `queryFn` closes over a variable, it belongs in the key.** (The ESLint plugin `@tanstack/eslint-plugin-query` flags violations.)

### Key factories

Strings scattered across the app invite typos and make invalidation fragile. Centralize them:

```ts
export const projectKeys = {
  all:    ["projects"] as const,
  lists:  () => [...projectKeys.all, "list"] as const,
  list:   (filters: ProjectFilters) => [...projectKeys.lists(), filters] as const,
  details: () => [...projectKeys.all, "detail"] as const,
  detail: (id: string) => [...projectKeys.details(), id] as const,
}

projectKeys.list({ status: "open" })   // ["projects", "list", { status: "open" }]
projectKeys.detail("42")               // ["projects", "detail", "42"]
```

`invalidateQueries({ queryKey: projectKeys.lists() })` now refreshes every list but leaves details alone.

## `queryOptions`: define once, use anywhere

```ts
import { queryOptions } from "@tanstack/react-query"

export const projectQuery = (id: string) =>
  queryOptions({
    queryKey: projectKeys.detail(id),
    queryFn: ({ signal }) => projectsApi.get(id, signal),
    staleTime: 60_000,
  })
```

```tsx
useQuery(projectQuery(id))                           // in a component
useSuspenseQuery(projectQuery(id))                   // with Suspense
queryClient.prefetchQuery(projectQuery(id))          // before navigation
queryClient.ensureQueryData(projectQuery(id))        // in a route loader
queryClient.setQueryData(projectQuery(id).queryKey, updated)   // typed key → typed data
```

Key and function stay together, type inference flows to every use site, and loaders, prefetching, and components can't drift apart. Prefer this over inline objects for anything used in more than one place.

## The query function context

```ts
queryFn: ({ queryKey, signal }) => projectsApi.get(queryKey[2], signal)
```

Pass **`signal`** to your request. If the query becomes unneeded (the component unmounts or the key changes while in flight), TanStack Query aborts it, saving bandwidth and preventing stale writes. Forward it through your API client ([02](../11-api-integration/02-api-client.md)).

## What you get back

| Field | Meaning |
|---|---|
| `data` | The cached value (`undefined` until first success) |
| `error` | The last error |
| `status` / `fetchStatus` | Data presence / request activity |
| `isPending`, `isLoading`, `isFetching`, `isRefetching`, `isError`, `isSuccess` | Derived flags ([02](./02-loading-and-error-states.md)) |
| `refetch()` | Manually refetch |
| `dataUpdatedAt` | Timestamp of the last success |

## Key options

```ts
useQuery({
  queryKey, queryFn,
  enabled: !!projectId,            // don't run until true (dependent queries)
  staleTime: 60_000,               // fresh for 1 min: no refetch on mount/focus
  gcTime: 10 * 60_000,             // keep unused cache for 10 min
  retry: 2,
  select: (projects) => projects.filter((p) => p.status === "open"),
  placeholderData: keepPreviousData,
  refetchInterval: 5000,           // polling
  refetchOnWindowFocus: false,
})
```

- **`enabled`**: dependent queries and "wait for input". Remember: disabled + no data = `isPending` but not `isLoading`.
- **`staleTime` / `gcTime`**: the caching knobs, explained in [04](./04-caching-and-synchronization.md).
- **`select`**: see below.
- **`placeholderData`**: temporary data while the real data loads ([07](./07-pagination-and-infinite-queries.md)). Distinct from `initialData`, which is *written into the cache* as real data.

### `select`: derive from the cache

```tsx
const { data: openCount } = useQuery({
  ...projectsQuery(),
  select: (projects) => projects.filter((p) => p.status === "open").length,
})
```

The cache keeps the full list; this component re-renders only when **its selected result** changes. Multiple components can select different slices of one cache entry, with no duplicate requests and no copied state ([00](./00-server-vs-client-state.md#derive-dont-store)). Define `select` as a stable function (outside the component or `useCallback`) when it's expensive.

## Defaults worth knowing

Out of the box, TanStack Query is deliberately aggressive about freshness:

| Default | Effect |
|---|---|
| `staleTime: 0` | Data is stale immediately, so it refetches whenever a trigger fires |
| `gcTime: 5 min` | Unused entries are garbage-collected after 5 minutes |
| `retry: 3` | With exponential backoff for queries (mutations default to no retries) |
| `refetchOnWindowFocus` / `refetchOnReconnect` / `refetchOnMount` | `true` (for stale data) |
| Deduplication | Identical concurrent requests share one fetch |

That's why new users see "extra" requests: it's by design, and `staleTime` is the dial that calms it. Details in [04](./04-caching-and-synchronization.md).

## Parallel and dynamic queries

Multiple `useQuery` calls in one component run in parallel. For a variable number of queries:

```tsx
const results = useQueries({
  queries: projectIds.map((id) => projectQuery(id)),
})
// results[i].data, results[i].isPending, …
```

## Suspense variant

```tsx
const { data } = useSuspenseQuery(projectQuery(id))   // data is never undefined
```

Pair with `<Suspense>` and an error boundary ([02](./02-loading-and-error-states.md#suspense-and-error-boundaries)). `useSuspenseQuery` has no `enabled` or `placeholderData` options, and `data` is always defined: it either suspends or throws.

## Using the client directly

```tsx
const queryClient = useQueryClient()

queryClient.invalidateQueries({ queryKey: projectKeys.lists() })
queryClient.setQueryData(projectKeys.detail(id), updated)
queryClient.getQueryData(projectKeys.detail(id))
queryClient.prefetchQuery(projectQuery(id))
queryClient.removeQueries({ queryKey: projectKeys.all })
queryClient.clear()                                  // e.g. on logout
```

`useQueryClient()` inside components; outside React (route loaders, API error handlers), import the `queryClient` instance itself.

## With React Router

Loaders can warm the cache so the page renders with data ready, while the component keeps all of Query's features. See [route data loading](../10-routing/05-route-data-loading.md#loaders-with-tanstack-query):

```tsx
loader: ({ params }) => queryClient.ensureQueryData(projectQuery(params.projectId!))
```

## TypeScript

```ts
const { data } = useQuery({ queryKey: ["x"], queryFn: () => projectsApi.get("1") })
// data: Project | undefined   ← inferred from queryFn's return type
```

Let inference work from `queryFn`; don't write `useQuery<Project>(...)`. Specify error types globally if you throw a custom class:

```ts
declare module "@tanstack/react-query" {
  interface Register { defaultError: ApiError }
}
```

## Testing

Create a **fresh `QueryClient` per test** (shared caches leak data between tests) with `retry: false`, or failing requests retry with backoff and tests time out. Mock at the network layer with [MSW](../18-testing-and-debugging/04-mocking-and-msw.md).

## Coming from v4

| v4 | v5 |
|---|---|
| `cacheTime` | **`gcTime`** |
| `isLoading` (no data yet) | **`isPending`** (and `isLoading` now means pending **and** fetching) |
| `keepPreviousData: true` | `placeholderData: keepPreviousData` |
| `onSuccess`/`onError`/`onSettled` on **queries** | **Removed** (still exist on mutations). Use effects or the global caches |
| `useQuery(key, fn, options)` overloads | **Object form only** |
| `useInfiniteQuery` optional `initialPageParam` | **`initialPageParam` required** |
| Mutation `isLoading` | **`isPending`** |

Old blog posts and Stack Overflow answers often use the v4 API, so check the version first.

## Common mistakes

- **`new QueryClient()` inside a component body**, giving an empty cache every render.
- **Missing variables in the query key**, so changing a filter doesn't refetch (or shows another filter's data).
- **`queryFn` that doesn't throw** on HTTP errors, so failures look like success.
- **Copying `data` into state** ([00](./00-server-vs-client-state.md)).
- **Reading `data` without handling `undefined`** (use the status checks or `useSuspenseQuery`).
- **Inconsistent key shapes** (`["project", 1]` vs `["project", "1"]`): two cache entries for one thing.
- **Leaving `staleTime` at 0 and being surprised by refetches.**
- **Using `onSuccess` on queries** (removed in v5).
- **Not forwarding `signal`** to the request.
- **Shared `QueryClient` across tests.**
- **Using `initialData` when you meant `placeholderData`**, which puts fake data in the cache as if it were real.

## Quick summary

- TanStack Query is a server-state cache: **key** identifies the data, **function** fetches it, and the library handles the rest.
- One `QueryClient` and one provider; use the devtools.
- Put everything the function depends on in the **key**; use key factories and `queryOptions` for reuse.
- `queryFn` must throw on failure; forward `signal`.
- `select` derives slices without copying; `enabled` gates dependent queries.
- Defaults are aggressive (`staleTime: 0`); tune `staleTime` next.
- v5 renames: `gcTime`, `isPending`, `placeholderData: keepPreviousData`, object-only signatures.

## Next

[04 — Caching and synchronization](./04-caching-and-synchronization.md)
