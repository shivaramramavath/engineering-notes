# Suspense

Suspense lets a component say **"I'm not ready yet"** while it waits for something (code, data), and lets a parent declare **what to show in the meantime**. Loading states become declarative: instead of every component checking `isLoading`, you wrap a region in `<Suspense>` and describe the fallback once.

```tsx
import { Suspense } from "react"

<Suspense fallback={<ProjectListSkeleton />}>
  <ProjectList />          {/* may "suspend" while loading */}
</Suspense>
```

While anything inside suspends, React shows the `fallback`. When it's ready, React swaps in the real content.

## What triggers suspension

A component suspends only when it reads from a **Suspense-compatible source**:

| Source | How |
|---|---|
| Lazy-loaded components | `React.lazy(() => import("./x"))` ([code splitting](../14-performance/03-code-splitting-and-lazy-loading.md)) |
| Promises read with `use` | `use(promise)` (React 19) |
| Suspense-enabled data libraries | `useSuspenseQuery` ([TanStack Query](../12-server-state/03-tanstack-query.md#suspense-variant)), framework data APIs |
| Server Components / streaming | [07](./07-server-components-and-ssr.md) |

**Not** triggered by `useEffect` + `fetch` + `setState`. React can't know an effect is loading something. Fetching in effects needs manual `isLoading` state, and Suspense doesn't see it.

## The `use` hook

`use` reads a resource during render. For a promise, it **suspends until the promise resolves**, then returns the value:

```tsx
import { use, Suspense } from "react"

function Comments({ commentsPromise }: { commentsPromise: Promise<Comment[]> }) {
  const comments = use(commentsPromise)       // suspends until resolved; throws if rejected
  return <ul>{comments.map((c) => <li key={c.id}>{c.text}</li>)}</ul>
}

function Post({ id }: { id: string }) {
  const commentsPromise = getComments(id)     // ⚠ see below: must be stable across renders
  return (
    <Suspense fallback={<p>Loading comments…</p>}>
      <Comments commentsPromise={commentsPromise} />
    </Suspense>
  )
}
```

Key facts about `use`:

- Unlike hooks, it **can be called conditionally** (inside `if`, loops, after early returns).
- It also reads **context**: `use(ThemeContext)`.
- A **rejected** promise throws during render and goes to the nearest [error boundary](./04-error-boundaries.md).
- **The promise must be stable across renders.** Creating a promise inside the component that calls `use` produces a new promise every render, which suspends again, forever:

```tsx
function Bad({ id }: { id: string }) {
  const data = use(fetch(`/api/${id}`).then((r) => r.json()))   // ✗ new promise each render → infinite suspend loop
}
```

The promise has to come from something that **caches it**: a parent that created it once (a route loader, a Server Component passing it down), a Suspense-aware library (TanStack Query), or your own cache keyed by input. That's the reason most apps use `use` indirectly through a library or framework instead of hand-rolling promise caching ([fetching data](../12-server-state/01-fetching-data.md#reacts-19-use-and-suspense)).

## Boundary placement: granularity matters

Where you put `<Suspense>` decides what the user sees while loading.

```tsx
// One boundary: everything waits for the slowest part. The whole page is a skeleton.
<Suspense fallback={<PageSkeleton />}>
  <Header />
  <Feed />
  <Sidebar />
</Suspense>

// Separate boundaries: sections load and reveal independently.
<Header />
<Suspense fallback={<FeedSkeleton />}><Feed /></Suspense>
<Suspense fallback={<SidebarSkeleton />}><Sidebar /></Suspense>
```

Think of boundaries as **loading regions**:

- **Too coarse:** one slow widget blanks the whole page.
- **Too fine:** a confetti of spinners appears and disappears at different times, which feels chaotic.
- A good heuristic is **one boundary per meaningful region** that the user perceives as a unit (a card, a panel, a list), with skeletons that match the final layout to avoid [layout shift](../14-performance/00-profiling-and-measuring.md#what-to-measure-core-web-vitals).

Boundaries **nest**. A component suspends to its *nearest* boundary, and if there's none in the tree at all, React throws an error. Use an outer boundary as a safety net and inner ones for finer regions. Place `<Suspense>` **above** the component that suspends, never inside it. A component can't catch its own suspension.

## Showing old content during updates

By default, when a component that was already showing content **suspends again** (new data for new props), React replaces it with the fallback, and users see content flash to a skeleton. Two tools keep the old UI:

**1. A transition**: wrap the update that causes the re-suspend:

```tsx
const [isPending, startTransition] = useTransition()
startTransition(() => setPage(next))     // old page stays visible; new one appears when ready
```

**2. A deferred value**: feed the Suspense-reading component a lagging value ([02](./02-useDeferredValue.md#with-data-fetching-and-suspense)).

In both cases React shows the fallback **only for boundaries that weren't already revealed**, so the first load shows skeletons, and subsequent updates keep content visible with a subtle pending indicator ([01](./01-transitions.md#transitions-and-suspense)).

To intentionally reset and show the fallback again for a new entity (a different record rather than the same one refreshed), change the `key` of the boundary or the component:

```tsx
<Suspense key={userId} fallback={<ProfileSkeleton />}>
  <Profile userId={userId} />
</Suspense>
```

## Suspense and waterfalls

Suspense makes loading *declarative*, not *parallel*. A component that suspends stops rendering at that point, so anything below it (children that would start their own fetches) doesn't get to run until the data arrives:

```tsx
function Page() {
  const user = useSuspenseQuery(userQuery())               // suspends
  return <Projects userId={user.data.id} />                // only renders after user arrives → its fetch starts late
}
```

That's the classic **waterfall**. Mitigations:

- **Start fetches early**: in route loaders, or by prefetching, so data is already loading before the component renders ([route data loading](../10-routing/05-route-data-loading.md#loaders-with-tanstack-query), [prefetching](../12-server-state/04-caching-and-synchronization.md#prefetching)).
- **Fetch in parallel at the top** rather than nested.
- **Restructure the API** so one request returns what's needed.
- Don't rely on sibling components under one boundary to fetch in parallel. How React treats siblings of a suspended component has changed across React 19 releases (pre-rendering siblings to warm up their requests), so treat it as an optimization rather than a guarantee, and check the release notes for your version.

## Suspense with errors

Suspense handles **waiting**; it doesn't handle **failure**. A rejected promise throws, and an error boundary catches it. Pair them:

```tsx
<ErrorBoundary fallback={<LoadFailed />}>
  <Suspense fallback={<Skeleton />}>
    <DataThing />
  </Suspense>
</ErrorBoundary>
```

Boundary order: error boundary **outside**, Suspense **inside**, so a failure replaces the whole region with the error UI, rather than leaving the skeleton up forever. See [04](./04-error-boundaries.md) and the Query-specific reset pattern in [loading and error states](../12-server-state/02-loading-and-error-states.md#suspense-and-error-boundaries).

## Suspense on the server

Suspense is also how **streaming server rendering** works: the server sends the HTML for everything that's ready, with fallbacks as placeholders, then streams in the rest as each boundary resolves, and React hydrates boundaries independently, so a slow section doesn't hold up the rest of the page. See [07](./07-server-components-and-ssr.md#streaming).

## Designing fallbacks

- **Match the layout**: skeletons sized like the real content prevent layout shift.
- **Avoid flicker**: if content usually arrives in under ~200 ms, a flash of skeleton looks worse than a brief blank. Transitions and cached data reduce how often fallbacks appear.
- **Keep fallbacks cheap**: they render *while* heavy work happens elsewhere. Don't put heavy components in them.
- **Make them accessible**: `aria-busy`, and avoid trapping focus or announcing noise.

## Common mistakes

- **Expecting `useEffect` + `fetch` to trigger Suspense.** It won't. Use a Suspense-enabled source.
- **Creating the promise inside the component calling `use`**, which causes an infinite suspend loop.
- **No boundary above a suspending component** (error), or a single huge boundary that blanks the page.
- **Putting `<Suspense>` inside the component that suspends.**
- **No error boundary**, so a rejected promise crashes the tree.
- **Error boundary placed *inside* Suspense**, leaving the skeleton up when a request fails.
- **Re-suspending the same boundary on every update**, flashing fallbacks, instead of using transitions or deferred values.
- **Assuming parallel fetching**, then building waterfalls (suspend → child renders → child suspends).
- **Too many tiny boundaries**, producing a spinner confetti.
- **Skeletons that don't match the final layout.**

## Quick summary

- Suspense = "show this fallback while something inside is waiting", declaratively.
- Components suspend through **`lazy`**, **`use(promise)`**, or **Suspense-enabled libraries/frameworks**, not through effect-based fetching.
- `use` can be conditional and reads promises and context, but the promise must be **stable and cached**.
- Place boundaries **above** suspending components, at region granularity, and nest them.
- Use **transitions or deferred values** to keep old UI instead of re-showing fallbacks; change `key` to deliberately reset.
- Suspense doesn't parallelize fetching; start requests early (loaders, prefetch).
- Pair with an error boundary: error boundary outside, Suspense inside.

## Next

[04 — Error boundaries](./04-error-boundaries.md)