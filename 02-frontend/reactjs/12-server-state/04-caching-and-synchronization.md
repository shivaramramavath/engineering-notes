# Caching and Synchronization

The cache is the heart of TanStack Query. Understanding **when it serves data, when it refetches, and when it forgets** removes nearly all the "why did it refetch?" and "why is this stale?" confusion.

## One cache entry per key

```text
queryClient cache
 ├─ ["projects", "list", { status: "open" }]   → data, status, timestamps, observers
 ├─ ["projects", "detail", "42"]               → data, …
 └─ ["user"]                                   → data, …
```

Every `useQuery` with the same key **shares** one entry: one request, one copy, and every subscriber re-renders when it changes. That's the "no duplicated server state" promise from [00](./00-server-vs-client-state.md).

## Fresh, stale, inactive, garbage-collected

Each entry moves through a lifecycle governed by two timers:

```text
fetch succeeds
   │
   ▼
 FRESH ───── staleTime elapses ─────► STALE
 (served from cache,                  (served from cache INSTANTLY,
  no refetch)                          refetch triggers are allowed)

no components using it (unmounted)
   │
   ▼
 INACTIVE ───── gcTime elapses ─────► REMOVED from cache
```

| Option | Default | Meaning |
|---|---|---|
| **`staleTime`** | `0` | How long fetched data counts as **fresh**. While fresh, no automatic refetch happens |
| **`gcTime`** | 5 min | How long an **unused** (no observers) entry is kept before deletion |

Two things people mix up:

- **`staleTime` controls refetching.** Stale doesn't mean "bad": stale data is still shown immediately. It means "a trigger is allowed to refresh it".
- **`gcTime` controls memory.** It starts counting only when nothing is displaying the data. A visited-then-left page's data survives for `gcTime`, which is why Back feels instant.

### The stale-while-revalidate behavior

When a component mounts and cached data exists but is stale:

1. It renders the **cached data immediately** (no spinner).
2. It fires a **background refetch**.
3. When the response arrives, it updates in place.

That's the feeling of "fast and always fresh". The only time users see a spinner is the very first load of a key.

## What triggers a refetch

A refetch happens when a trigger fires **and the data is stale**:

| Trigger | Option (default) |
|---|---|
| A component using the query **mounts** | `refetchOnMount: true` |
| The browser tab/window **regains focus** | `refetchOnWindowFocus: true` |
| The network **reconnects** | `refetchOnReconnect: true` |
| A timer | `refetchInterval` (off by default) |
| You **invalidate** or call `refetch()` | Always |

With `staleTime: 0`, data is always stale, so every mount and focus refetches. That explains the "extra requests" in the Network tab. It's safe but chatty.

### Choosing `staleTime`

Set it per data type, from [00's staleness table](./00-server-vs-client-state.md#fresh-enough):

```ts
// Rarely changes
export const countriesQuery = () => queryOptions({ queryKey: ["countries"], queryFn: getCountries, staleTime: Infinity })

// A few minutes is fine
export const profileQuery = () => queryOptions({ queryKey: ["profile"], queryFn: getProfile, staleTime: 5 * 60_000 })

// Default to a modest global value; override where needed
new QueryClient({ defaultOptions: { queries: { staleTime: 30_000 } } })
```

- `staleTime: Infinity` means never stale automatically; refresh only via invalidation or manual refetch.
- A small global `staleTime` (10–60s) removes most redundant refetches without hiding changes for long.
- Don't set `staleTime: Infinity` globally for data that other users can change.
- Disable `refetchOnWindowFocus` for data where it's annoying (a form's reference data), but it's valuable for lists others edit.

## Deduplication

If five components mount and ask for the same key during the same moment, **one** request is made. Requests for fresh data never start at all. This is also why pushing `useQuery` down into the components that need data ("colocate") is fine and idiomatic: the cache coalesces the calls.

## Structural sharing

When a refetch returns data equal to what's cached, TanStack Query **keeps the old object references** (and replaces only the parts that changed). That means:

- `useEffect`/`useMemo` dependencies on `data` don't fire for no-op refetches.
- A re-render doesn't happen when nothing actually changed.

It relies on the data being JSON-compatible. If your `queryFn` returns class instances or huge payloads, you can opt out with `structuralSharing: false`.

## Invalidation

After a write, cached reads are wrong. **Invalidation** marks them stale and refetches the ones currently on screen:

```ts
queryClient.invalidateQueries({ queryKey: ["projects"] })
```

- **Prefix matching.** `["projects"]` matches `["projects", "list", {…}]` and `["projects", "detail", "42"]`. Invalidating a general key covers all specific ones. (This is why [key factories](./03-tanstack-query.md#key-factories) are hierarchical.)
- **Active queries** (a component is using them) refetch immediately.
- **Inactive queries** are only marked stale; they refetch when next used.

Narrowing:

```ts
queryClient.invalidateQueries({ queryKey: projectKeys.lists() })                 // all lists, not details
queryClient.invalidateQueries({ queryKey: projectKeys.detail("42"), exact: true })
queryClient.invalidateQueries({ predicate: (q) => q.queryKey[0] === "projects" && /* custom */ true })
```

`invalidateQueries` returns a promise that resolves when the refetches finish, which matters for [mutations](./05-mutations.md#keeping-the-mutation-pending-until-data-is-fresh).

Related methods:

| Method | Does |
|---|---|
| `invalidateQueries` | Mark stale + refetch active ones. **Default choice after writes** |
| `refetchQueries` | Force refetch now, regardless of staleness |
| `resetQueries` | Reset to initial state (clears data → back to `pending`) and refetch active |
| `removeQueries` | Delete entries from cache, no refetch |
| `cancelQueries` | Abort in-flight fetches (key step for [optimistic updates](./06-optimistic-updates.md)) |
| `clear()` | Wipe the whole cache (logout) |

## Writing directly to the cache

When you already know the new data (the mutation response), skip the round trip:

```ts
queryClient.setQueryData(projectKeys.detail(id), updatedProject)

queryClient.setQueryData<Project[]>(projectKeys.list(filters), (old) =>
  old?.map((p) => (p.id === id ? updatedProject : p))
)
```

- **Never mutate the old value**; return a new one. Query data is treated as immutable.
- The updater receives `undefined` if there's no entry, so handle it.
- `setQueryData` only touches the exact key you give it, not related lists. Updating every list view by hand gets error-prone; often "set the detail, invalidate the lists" is the right compromise.

## Seeding the cache: `initialData` vs `placeholderData`

| | `initialData` | `placeholderData` |
|---|---|---|
| Stored in cache | **Yes**, as real data | **No**, display-only |
| Considered fresh | Per `staleTime` (treated as fetched at `initialDataUpdatedAt`) | Always still fetches (`isPlaceholderData: true`) |
| Status | `success` immediately | `success`-like but flagged placeholder |
| Use for | Data you really have: seeded from another cached query | Temporary stand-ins: previous page, skeleton-ish values |

A common, legitimate use of `initialData`: seeding a detail from the list cache:

```ts
useQuery({
  ...projectQuery(id),
  initialData: () => queryClient.getQueryData<Project[]>(projectKeys.list(filters))?.find((p) => p.id === id),
  initialDataUpdatedAt: () => queryClient.getQueryState(projectKeys.list(filters))?.dataUpdatedAt,
})
```

The `initialDataUpdatedAt` tells Query how old that data actually is, so it knows whether to refetch.

## Prefetching

Fill the cache **before** the user needs it:

```tsx
<Link
  to={`/projects/${p.id}`}
  onMouseEnter={() => queryClient.prefetchQuery(projectQuery(p.id))}   // hover intent
>
```

- `prefetchQuery` is quiet: no error thrown, result cached (and respects `staleTime`, so it won't refetch fresh data).
- `ensureQueryData` returns cached data if it exists, else fetches; ideal for [route loaders](../10-routing/05-route-data-loading.md#loaders-with-tanstack-query).
- Prefetching the **next page** of a paginated list makes paging feel instant ([07](./07-pagination-and-infinite-queries.md)).

## Synchronizing with the outside world

The cache can only know what you tell it. Ways data stays in sync:

- **Refetch triggers**: focus, reconnect, interval ([above](#what-triggers-a-refetch)).
- **Invalidation after your own writes** ([05](./05-mutations.md)).
- **Server pushes** via WebSocket/SSE → `setQueryData` or `invalidateQueries` ([realtime](../11-api-integration/06-realtime-communication.md#wiring-events-into-your-data-layer)).
- **Polling** with `refetchInterval` when push isn't available.
- **Other tabs**: not synced automatically. Focus refetch usually covers it; for tighter coupling, broadcast events between tabs.

### Cache and identity

The cache is **per user session**. On logout (or user switch) call `queryClient.clear()`, or the next user may briefly see the previous user's data ([authentication](../11-api-integration/03-authentication.md)). Include the user ID in keys for user-scoped data if users can switch without a full reload.

### Persistence

The cache lives in memory, so a refresh empties it. For offline-first or instant cold starts, TanStack provides persistence plugins (for example `@tanstack/react-query-persist-client` with a storage persister). Persisted data must be **serializable**, versioned (add a `buster` so old shapes are discarded), and must **exclude sensitive data**. Treat it like `localStorage`, because it usually is.

## Debugging the cache

- Open the **devtools**: see each key's state (fresh / stale / inactive / fetching), data, and observers; trigger refetch or invalidate manually.
- "It refetched and I didn't expect it" → check `staleTime` and which trigger fired (focus? mount?).
- "It didn't refetch and I expected it" → is the data still *fresh* (`staleTime`), or is the key identical (did a variable miss the key)?
- "Wrong data shown" → key collision or missing key variable.
- "Memory grows" → many unique keys with a long `gcTime`; shorten `gcTime` or avoid unbounded key spaces (like per-keystroke search strings).

## Common mistakes

- **Confusing `staleTime` and `gcTime`.** One is refetch freshness, the other is unused-data retention.
- **Leaving `staleTime: 0` and fighting the refetches** with `refetchOnWindowFocus: false` everywhere.
- **`staleTime: Infinity` for shared, mutable data**, then wondering why changes never appear.
- **Mutating cached data in place** in `setQueryData`.
- **Invalidating too broadly** (`invalidateQueries()` with no key) and refetching the whole app.
- **Invalidating too narrowly** and leaving a related list stale.
- **Using `initialData` as a placeholder**, writing fake data into the cache as real.
- **Forgetting `clear()` on logout.**
- **Different key shapes for the same data**, splitting the cache.
- **Persisting sensitive data** to storage.

## Quick summary

- One cache entry per key, shared by every subscriber, with requests deduplicated.
- **Fresh** data is served without refetching; **stale** data is served instantly *and* refreshed in the background. `staleTime` sets the line.
- **`gcTime`** deletes unused entries; it only counts down while nothing is using them.
- Refetch triggers: mount, focus, reconnect, interval, invalidation, each only when stale (except manual).
- After writes: `invalidateQueries` (prefix-matched), or `setQueryData` when you already have the new value.
- Seed with `initialData` (real data) vs `placeholderData` (temporary); prefetch what users are about to need.
- Clear the cache on logout; use the devtools to see what's happening.

## Next

[05 — Mutations](./05-mutations.md)
