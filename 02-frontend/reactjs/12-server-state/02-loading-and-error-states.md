# Loading and Error States

Every read from a server has at least three outcomes: waiting, failed, or done. How you render each one is a large part of how an app *feels*: whether it flashes, jumps, hides stale data, or dead-ends users on an error.

> Examples use TanStack Query's flags because they model these states precisely. Setup is in [03](./03-tanstack-query.md).

## `status` and `fetchStatus`

TanStack Query separates two questions that beginners conflate:

| Property | Question | Values |
|---|---|---|
| `status` | Do we **have data**? | `pending` (none yet) · `success` (have data) · `error` (last fetch failed, no data) |
| `fetchStatus` | Is a **request running**? | `fetching` · `paused` (offline) · `idle` |

They're independent. A query can be `success` + `fetching` (background refresh), or `pending` + `paused` (wants data, but you're offline).

Convenient booleans derived from them:

| Flag | Meaning |
|---|---|
| `isPending` | No data yet (`status === "pending"`) |
| `isFetching` | A request is in flight, initial **or** background |
| `isLoading` | `isPending && isFetching`: the **first** load is actually running |
| `isRefetching` | `isFetching && !isPending`: background refresh with data on screen |
| `isError` | `status === "error"` |
| `isPlaceholderData` | The data shown is placeholder (e.g. previous page), not the real result |

If a query is **disabled** (`enabled: false`) with no data, `isPending` is `true` but `isLoading` is `false`: it isn't loading, it's waiting. Use `isPending` to decide "do I have anything to show?"

## The standard shape

```tsx
function ProjectList() {
  const { data, isPending, isError, error, refetch } = useQuery(projectsQuery())

  if (isPending) return <ProjectListSkeleton />

  if (isError) {
    return (
      <ErrorState
        message={getErrorMessage(error)}
        onRetry={() => refetch()}
      />
    )
  }

  if (data.length === 0) return <EmptyState title="No projects yet" action={<NewProjectButton />} />

  return <ul>{data.map((p) => <ProjectRow key={p.id} project={p} />)}</ul>
}
```

Order matters: **pending → error → empty → data**. After the `isPending`/`isError` checks, TypeScript narrows `data` to defined.

Four states, four designs. If you only design the happy path, users see the other three in production.

## Loading: skeletons over spinners

- **Skeletons** (grey placeholder shapes in the final layout) feel faster and avoid layout shift. Prefer them for lists, cards, and tables.
- **Spinners** suit small, indeterminate actions (a button, a small widget).
- **Avoid flicker** for fast responses. A skeleton that appears for 80ms looks like a glitch. Delay showing it briefly (~150–300ms) if loads are usually quick, or lean on cached data so most visits skip it.
- Reserve space so content doesn't jump when it arrives.
- Add `aria-busy="true"` on the loading region and a visually-hidden status message so screen reader users know something is happening.

## Background refetch: keep the data

The key insight of server-state caching: **don't blank the screen to refresh.** When data exists and a refetch runs, keep showing the data and signal the refresh subtly:

```tsx
const { data, isFetching, isPending } = useQuery(projectsQuery())

return (
  <section aria-busy={isFetching}>
    {isFetching && !isPending && <RefreshingIndicator />}   {/* tiny, non-blocking */}
    <ProjectTable data={data} />
  </section>
)
```

Treating *every* fetch as a loading state (using `isFetching` to show a full skeleton) is the classic bug: the UI flashes empty each time the window regains focus.

For app-wide indicators use `useIsFetching()` (count of in-flight queries) or `useIsMutating()`: a top progress bar, say.

## Errors: three different situations

### 1. Failed with no data (`status: "error"`)

Show an error state in that region, with a **retry** button. This is the case in the standard shape above.

### 2. Failed refetch **with** data on screen

This is subtle and important. If a background refetch fails, the query keeps its old `data`; `status` becomes `"error"` **and `data` is still defined**.

```tsx
const { data, isError, error, isFetching } = useQuery(projectsQuery())

// data exists AND the last refresh failed → keep showing it, warn gently
{isError && data && (
  <Alert variant="warning">Couldn't refresh. Showing the last loaded data.</Alert>
)}
```

Replacing a perfectly good screen with a blocking error because a background poll failed is worse than showing slightly stale data. Check `data` before choosing the full-screen error:

```tsx
if (isError && data === undefined) return <ErrorState … />
```

### 3. Failed mutation

Handled where the action happened: inline field errors for validation, toast for the rest ([05](./05-mutations.md), [API error handling](../11-api-integration/05-api-error-handling.md)).

### Retries happen first

By default queries retry failed requests three times with exponential backoff before `isError` becomes true, so a flaky network can look like a long loading state. Tune with `retry`/`retryDelay`, and don't retry 4xx ([05 — API error handling](../11-api-integration/05-api-error-handling.md#retries)).

## Empty is not loading

`data = []` is a **success**. Render an intentional empty state ("No projects yet", with a call to action). Never use `!data?.length` as your loading check, which shows a spinner forever on an empty list.

## Where to put the states

Handle loading and error states **close to the data**, not at the page root: a failed sidebar widget shouldn't blank the whole page.

```text
Page
 ├─ Header                ← always renders
 ├─ <Section A>           ← its own pending/error/empty
 └─ <Section B>           ← its own pending/error/empty
```

Small components that own a query and its states (`<ActivityFeed />`) compose well and fail independently.

## Suspense and error boundaries

The alternative to per-component `if (isPending)` checks is to declare states **once, above the component**:

```tsx
function ProjectList() {
  const { data } = useSuspenseQuery(projectsQuery())   // data is always defined here
  return <ul>{data.map(/* … */)}</ul>
}

<QueryErrorResetBoundary>
  {({ reset }) => (
    <ErrorBoundary onReset={reset} fallbackRender={({ resetErrorBoundary }) => (
      <ErrorState onRetry={resetErrorBoundary} />
    )}>
      <Suspense fallback={<ProjectListSkeleton />}>
        <ProjectList />
      </Suspense>
    </ErrorBoundary>
  )}
</QueryErrorResetBoundary>
```

- `useSuspenseQuery` **suspends** while pending and **throws** on error, so the component only handles success.
- `<Suspense fallback>` renders the loading UI; an [error boundary](../15-concurrent-and-modern-react/04-error-boundaries.md) renders the error UI.
- `QueryErrorResetBoundary` lets "Try again" reset the failed queries (the `ErrorBoundary` component here is from the `react-error-boundary` package).
- Boundaries let you group: one skeleton for several components, or a boundary per section.

Trade-offs:

| | Per-component `isPending` | Suspense + boundaries |
|---|---|---|
| Boilerplate | More `if`s | Less, but needs boundary setup |
| Control over each state | Fine-grained | Coarser |
| Parallel fetches | Natural | Siblings in one Suspense boundary can **waterfall** unless fetched together or prefetched |
| Data type | `data` may be `undefined` | `data` always defined |

For the non-Suspense hooks you can also opt specific errors into boundaries with the `throwOnError` option (a boolean or a function of the error), e.g. throw 5xx to a boundary but handle 4xx locally.

## Transitions: keeping old UI while loading

When params change (a new page, a new filter), the new query has no data yet, which means `isPending` and a skeleton flash. Two fixes:

- `placeholderData: keepPreviousData` shows the previous result until the new one arrives ([07](./07-pagination-and-infinite-queries.md)).
- With Suspense, wrap the state change in `startTransition`/`useTransition` so React keeps the old UI and avoids the fallback ([transitions](../15-concurrent-and-modern-react/01-transitions.md)).

## Accessibility

- Announce important state changes: errors in a `role="alert"` region, loading via `aria-busy` and/or a polite live region.
- Don't convey errors by color alone.
- Move focus sensibly when content replaces a spinner, and don't steal focus on background refetches.
- Retry buttons must be real buttons, reachable by keyboard.

## Common mistakes

- **Using `isFetching` for the initial skeleton**, so every refetch blanks the UI. Use `isPending`.
- **Using `!data` as "loading"**, which misfires for disabled queries and empty results.
- **Blocking error screen when stale data exists** after a failed refetch.
- **Forgetting the empty state.**
- **No retry affordance** on errors.
- **Page-level spinners/errors** for one failed widget.
- **Layout shift** because skeletons don't match final content.
- **Skeleton flicker** on near-instant loads.
- **Swallowing errors** in `queryFn` (`try/catch` returning `[]`), hiding failures from the UI entirely. Let it throw.
- **Suspense without an error boundary**, so a failed request crashes the tree.
- **Relying on color only** for error state.

## Quick summary

- `status` = do we have data; `fetchStatus` = is a request running. Use `isPending` for "nothing to show", `isFetching` for "something is refreshing".
- Render in order: **pending → error → empty → data**, and design all four.
- Keep data on screen during refetches; show a subtle indicator.
- A failed refetch with data present should warn, not blank the screen.
- Put states near the data. Optionally centralize with Suspense + error boundaries.
- Avoid flashes with `keepPreviousData` or transitions.

## Next

[03 — TanStack Query](./03-tanstack-query.md)
