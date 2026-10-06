# Fetching Data

Before reaching for a library, it's worth seeing what data fetching *actually requires* in React. That explains every feature of [TanStack Query](./03-tanstack-query.md), and keeps you from cargo-culting it.

(HTTP basics, `res.ok`, and abort handling are in [11 — Fetch](../11-api-integration/00-fetch.md). This note is about the React side.)

## The states you must model

A request isn't "data or not". It has a lifecycle:

```text
idle ──► pending ──► success (data)
              └────► error   (error)
```

And once data exists, further fetches are **refetches**: data is on screen *and* a request is in flight (or failed). Modelling this as separate booleans (`isLoading`, `hasError`, `data`) leads to impossible combinations ("loading and error at once"). Use a **discriminated union**:

```ts
type AsyncState<T> =
  | { status: "pending" }
  | { status: "error"; error: Error }
  | { status: "success"; data: T }
```

TypeScript then narrows: inside `status === "success"`, `data` is definitely defined.

## The hand-rolled version

```tsx
function useFetch<T>(load: (signal: AbortSignal) => Promise<T>, deps: unknown[]) {
  const [state, setState] = useState<AsyncState<T>>({ status: "pending" })

  useEffect(() => {
    const controller = new AbortController()
    setState({ status: "pending" })

    load(controller.signal)
      .then((data) => setState({ status: "success", data }))
      .catch((error) => {
        if (error.name !== "AbortError") setState({ status: "error", error })
      })

    return () => controller.abort()        // ignore stale responses
  }, deps) // eslint-disable-line react-hooks/exhaustive-deps

  return state
}

function ProjectPage({ id }: { id: string }) {
  const state = useFetch((signal) => projectsApi.get(id, signal), [id])

  if (state.status === "pending") return <Spinner />
  if (state.status === "error") return <ErrorMessage error={state.error} />
  return <h1>{state.data.name}</h1>
}
```

This is **correct** for one component fetching one thing. Now see what it doesn't do.

## What this version is missing

| Problem | What happens |
|---|---|
| **No cache** | Navigate away and back: spinner again, refetch from scratch |
| **No deduplication** | Header and page both call `useFetch(getUser)`: two identical requests |
| **No sharing** | Each component holds its own copy; updating one leaves the others stale |
| **Spinner on every change** | Switching `id` flashes `pending` instead of keeping the old data visible |
| **No refetch** | Data never refreshes after tab refocus, reconnect, or a write elsewhere |
| **No retry** | One flaky request = permanent error screen |
| **No invalidation** | After a POST, nothing tells the list to refetch |
| **Fragile deps** | The `eslint-disable` above is a smell: stale closures if `load` changes |
| **Memory** | Nothing ever cleans up cached data (because nothing is cached) |

Each fix is a feature: a cache keyed by request, a registry of in-flight requests, subscriptions so components re-render when data changes, timers for staleness, retry with backoff, an invalidation API. That's the library. Writing it yourself is a project, not a hook.

## Fetching patterns that matter regardless of tools

### Waterfalls

A **waterfall** is a chain of requests that could have run in parallel but wait on each other:

```tsx
function Dashboard() {
  const user = useUser()                       // request 1
  if (!user.data) return <Spinner />
  return <Projects userId={user.data.id} />    // mounts only after 1 → request 2 starts late
}
```

Waterfalls come from **nested components that each fetch after they render**. Total time = sum of all requests. Fixes:

- **Fetch at the top** (a route [loader](../10-routing/05-route-data-loading.md), or hoist the queries) and let children read the results.
- **Prefetch** data you know you'll need.
- Make endpoints return what the screen needs, instead of requiring N calls.
- Check the **Network tab**: staircase-shaped timelines are waterfalls.

### Parallel fetching

Independent requests should start together:

```tsx
// Each of these runs concurrently, since both are in the same render
const user = useQuery(userQuery())
const projects = useQuery(projectsQuery())

// For a dynamic number, useQueries
const results = useQueries({ queries: ids.map((id) => projectQuery(id)) })
```

Outside React, `Promise.all([a(), b()])`. Note that `await a(); await b()` is sequential, which is a waterfall hiding in plain async code.

### Dependent fetching

Sometimes request 2 truly needs request 1's result (look up the user, then their projects). Then you *must* wait, but make it explicit rather than accidental:

```tsx
const user = useQuery(userQuery())
const projects = useQuery({
  ...projectsQuery(user.data?.id),
  enabled: !!user.data,          // don't run until we have the id
})
```

Ask first whether the **server** could do it in one call (`/me/projects`). Removing the dependency beats optimizing it.

### Race conditions

Responses can arrive out of order, so an old answer overwrites a newer one. The hand-rolled hook above avoids it with `AbortController` cleanup. Libraries do this by keying results to their request: a response for `["project", 1]` can never land in the slot for `["project", 2]`.

### Don't fetch in render

```tsx
// ✗ side effect in render: fires on every render, infinite loops with setState
function Bad() {
  fetch("/api/projects").then(setData)
}
```

Render must be pure. Fetching belongs in an event handler, a loader, or a library hook. Effects are the escape hatch of last resort; see [you might not need an effect](../03-hooks/03-you-might-not-need-an-effect.md).

## React 19's `use` and Suspense

React 19 can read a promise during render with `use`, suspending until it resolves:

```tsx
function Projects({ projectsPromise }: { projectsPromise: Promise<Project[]> }) {
  const projects = use(projectsPromise)
  return <List items={projects} />
}

<Suspense fallback={<Skeleton />}>
  <Projects projectsPromise={projectsPromise} />
</Suspense>
```

The catch: **the promise must be stable across renders**. Creating it inside the component (`use(fetch(...))`) makes a new promise every render and loops. It has to come from a parent, a cache, a loader, or a Suspense-aware library. That's why `use` is a building block for libraries and frameworks ([Suspense](../15-concurrent-and-modern-react/03-suspense.md), [server components](../15-concurrent-and-modern-react/07-server-components-and-ssr.md)), and TanStack Query's `useSuspenseQuery` handles it for you.

## What to use

| Situation | Tool |
|---|---|
| Client-side server state in an SPA | **TanStack Query** (or SWR/RTK Query) |
| Data tied to a route, fetched before render | Route **loaders** (+ Query for caching) |
| Framework with server rendering | The framework's data APIs / server components |
| A single one-off call (submit, download) | Plain `fetch` in an event handler; no cache needed |
| Truly tiny demo | `useEffect` with cleanup, knowing its limits |

## Common mistakes

- **Fetching in render**: loops and duplicate requests.
- **No cleanup/abort** in effect-based fetching, causing races and `setState` after unmount.
- **Boolean soup** (`isLoading`/`isError`/`data` separately) instead of one status.
- **Nested fetch-on-mount components**: waterfalls.
- **`await` in sequence** for independent requests.
- **Resetting to a spinner** when params change, instead of keeping previous data visible.
- **Copying fetched data into state** ([00](./00-server-vs-client-state.md)).
- **Creating a promise inside a component and passing it to `use`.**
- **Re-implementing a cache by hand** when a library exists.

## Quick summary

- A request has a lifecycle (`pending → success | error`), and refetches add "data **and** fetching". Model it as a discriminated union.
- A correct `useEffect` fetch handles one component and one request; it lacks caching, dedup, sharing, refetching, retry, and invalidation.
- Avoid waterfalls: fetch high, in parallel, or prefetch. Make dependencies explicit (`enabled`).
- Never fetch during render. `use()` needs a stable promise.
- For real server state, use a library: [TanStack Query](./03-tanstack-query.md).

## Next

[02 — Loading and error states](./02-loading-and-error-states.md)
