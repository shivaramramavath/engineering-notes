# Route Data Loading

The classic way to load data in React is: render the component, then fetch in an effect, then show a spinner. That produces **waterfalls** (a parent fetches, then the child mounts and fetches) and a lot of loading-state boilerplate.

Data routers invert it: each route declares a **loader**, and React Router runs the loaders for the *entire matched route tree in parallel* **as the navigation starts**, before the page renders.

```text
Click link
   │
   ├─► match routes: Root → Dashboard → Project
   ├─► run loaders in parallel ─────────┐
   │                                    ▼
   └─► (old page stays visible)    all resolved
                                        │
                                        ▼
                                  render new page with data
```

Loaders and actions need a **data router** (`createBrowserRouter`), not `<BrowserRouter>`. See [00](./00-react-router.md).

## Loaders

```tsx
import { useLoaderData, type LoaderFunctionArgs } from "react-router"

export async function projectLoader({ params, request }: LoaderFunctionArgs) {
  const res = await fetch(`/api/projects/${params.projectId}`, { signal: request.signal })
  if (res.status === 404) throw new Response("Not found", { status: 404 })
  if (!res.ok) throw new Response("Failed to load project", { status: res.status })
  return (await res.json()) as Project
}

export function ProjectPage() {
  const project = useLoaderData() as Awaited<ReturnType<typeof projectLoader>>
  return <h1>{project.name}</h1>
}
```

```tsx
{ path: "projects/:projectId", loader: projectLoader, element: <ProjectPage /> }
```

- A loader receives `{ request, params }`. `request` is a standard `Request`: read the URL with `new URL(request.url)`, and pass `request.signal` to `fetch` so a superseded navigation **cancels** the in-flight request.
- Return data (or a `Response`). `useLoaderData()` gives it to the component with **no loading state**; the component only renders once data exists.
- Throwing (any error or a `Response`) renders the route's `errorElement` instead.
- `useLoaderData` is typed loosely by default; cast it as shown, or use the typing helpers your version of React Router provides.

Loaders run **in the browser**, so no secrets belong in them, and they hit your API like any other client code. For server rendering, see [server components and SSR](../15-concurrent-and-modern-react/07-server-components-and-ssr.md).

## Pending UI and no spinners

The previous page stays visible while loaders run, so show progress with `useNavigation`:

```tsx
const navigation = useNavigation()
const isLoading = navigation.state === "loading"
```

(see [03](./03-navigation.md)). Per-link feedback comes from `NavLink`'s `isPending`.

## Actions and forms

An **action** handles writes (POST/PUT/PATCH/DELETE) for a route. Submit with React Router's `<Form>`:

```tsx
import { Form, redirect, useActionData, useNavigation, type ActionFunctionArgs } from "react-router"

export async function newProjectAction({ request }: ActionFunctionArgs) {
  const formData = await request.formData()
  const name = String(formData.get("name") ?? "").trim()

  if (name.length < 2) return { error: "Name must be at least 2 characters" }

  const project = await createProject({ name })
  return redirect(`/projects/${project.id}`)
}

export function NewProjectPage() {
  const actionData = useActionData() as { error?: string } | undefined
  const navigation = useNavigation()
  const submitting = navigation.state === "submitting"

  return (
    <Form method="post">
      <input name="name" aria-invalid={!!actionData?.error} />
      {actionData?.error && <p role="alert">{actionData.error}</p>}
      <button disabled={submitting}>{submitting ? "Creating…" : "Create"}</button>
    </Form>
  )
}
```

```tsx
{ path: "projects/new", action: newProjectAction, element: <NewProjectPage /> }
```

- `<Form>` serializes the fields, calls the action **without a page reload**, and works even before JavaScript hydrates.
- Returning data (like validation errors) exposes it via `useActionData`; returning `redirect()` navigates.
- **Revalidation:** after a successful action, React Router automatically **re-runs the loaders** of the current routes so the UI shows fresh data. That's why lists update after a create/delete without manual refetching.

This is a different philosophy from [React Hook Form](../06-forms/02-react-hook-form.md) plus a mutation hook. Both are valid. Actions suit simple form-per-route flows and progressive enhancement; RHF + mutations suit complex client-side validation and multi-step flows. You can mix: use RHF for the form UI and `useSubmit`/`fetcher.submit` to hand off to an action.

## Fetchers: mutations without navigation

Not every write should change the URL: toggling a favorite, deleting a row, inline edits. `useFetcher` calls loaders/actions **without navigating**:

```tsx
import { useFetcher } from "react-router"

function FavoriteButton({ project }: { project: Project }) {
  const fetcher = useFetcher()
  const isFavorite =
    fetcher.formData ? fetcher.formData.get("favorite") === "true" : project.favorite  // optimistic

  return (
    <fetcher.Form method="post" action={`/projects/${project.id}/favorite`}>
      <button name="favorite" value={String(!isFavorite)}>
        {isFavorite ? "★" : "☆"}
      </button>
    </fetcher.Form>
  )
}
```

Each fetcher has its own `state` (`idle | submitting | loading`) and `data`, so many can run at once. Reading `fetcher.formData` while it's in flight gives you a simple **optimistic UI**. After the action, affected loaders revalidate.

## Errors

Throw from loaders/actions; handle at the route boundary:

```tsx
import { isRouteErrorResponse, useRouteError } from "react-router"

export function ProjectError() {
  const error = useRouteError()

  if (isRouteErrorResponse(error)) {
    if (error.status === 404) return <p>That project doesn't exist.</p>
    return <p>Request failed ({error.status}).</p>
  }
  return <p>Something went wrong.</p>
}
```

```tsx
{ path: "projects/:projectId", loader: projectLoader, element: <ProjectPage />, errorElement: <ProjectError /> }
```

The error renders in place of that route only; parent layouts (navigation, sidebar) stay intact. Put `errorElement` on layout routes to contain failures. Without one, errors bubble to the next parent's boundary, and finally to a router default. See [error boundaries](../15-concurrent-and-modern-react/04-error-boundaries.md) and [API error handling](../11-api-integration/05-api-error-handling.md).

## Deferring slow data

Awaiting everything delays the whole page. For non-critical data, return the promise **without awaiting** and render it with `Await`:

```tsx
export async function dashboardLoader() {
  const summary = await getSummary()           // critical: awaited
  const activity = getActivity()               // slow: promise, not awaited
  return { summary, activity }
}

function Dashboard() {
  const { summary, activity } = useLoaderData() as DashboardData
  return (
    <>
      <Summary data={summary} />
      <Suspense fallback={<ActivitySkeleton />}>
        <Await resolve={activity} errorElement={<p>Couldn't load activity.</p>}>
          {(items) => <ActivityList items={items} />}
        </Await>
      </Suspense>
    </>
  )
}
```

Critical content blocks navigation; the rest streams in under [Suspense](../15-concurrent-and-modern-react/03-suspense.md). In React Router v7, promises returned inside the loader result can be used this way directly; v6 required a `defer()` wrapper, so check which you're on if you copy older examples.

## Loaders with TanStack Query

Loaders and [TanStack Query](../12-server-state/03-tanstack-query.md) overlap, and they combine well: the loader **warms the cache** at navigation time, and the component reads from the cache with hooks.

```tsx
const projectQuery = (id: string) => queryOptions({
  queryKey: ["projects", id],
  queryFn: () => fetchProject(id),
})

export const projectLoader = (queryClient: QueryClient) =>
  async ({ params }: LoaderFunctionArgs) => {
    await queryClient.ensureQueryData(projectQuery(params.projectId!))
    return null
  }

function ProjectPage() {
  const { projectId } = useParams()
  const { data: project } = useSuspenseQuery(projectQuery(projectId!))
  // …
}
```

```tsx
const router = createBrowserRouter([
  { path: "projects/:projectId", loader: projectLoader(queryClient), element: <ProjectPage /> },
])
```

You get loader-timed fetching (no waterfall) *and* query features: caching, background refetch, mutations with invalidation, optimistic updates. `ensureQueryData` returns cached data immediately when present, so repeat visits are instant.

**When to use which:**

| Use | When |
|---|---|
| Plain loaders | Small apps, simple fetching, you like actions + `<Form>` |
| Loaders + TanStack Query | Real server state: caching, refetching, mutations, shared data across screens |
| Query only (no loaders) | Fine for many apps; accept that fetching starts after render |

## Lazy routes (code splitting)

```tsx
{
  path: "reports",
  lazy: async () => {
    const { ReportsPage, reportsLoader } = await import("./pages/reports")
    return { Component: ReportsPage, loader: reportsLoader }
  },
}
```

`lazy` loads the route's code **and** its loader when the route is navigated to. See [code splitting](../14-performance/03-code-splitting-and-lazy-loading.md).

## Common mistakes

- **Using loaders with `<BrowserRouter>`.** They're ignored; you need a data router.
- **Not forwarding `request.signal`**, so abandoned navigations keep fetching.
- **Awaiting everything** and making the page wait on slow, non-critical data. Defer it.
- **Storing loader data in `useState`** (a stale copy). Read `useLoaderData()` directly; it updates on revalidation.
- **Redirecting inside the component** when a loader/action could `redirect()` before render.
- **Returning a `Response` object from a non-error loader** and expecting the parsed body; return plain data.
- **No `errorElement`**, so any thrown error falls to the root default.
- **Assuming parent loaders finish first.** Nested loaders run in parallel; don't make child loaders depend on parent results.
- **Using actions for everything**, even when a fetcher or a mutation hook fits better (writes that shouldn't navigate).
- **Secrets in loaders.** They run in the browser.

## Quick summary

- Loaders run in parallel at navigation start, so no render-then-fetch waterfalls and no loading state in the component.
- `useLoaderData` reads the result; throw responses/errors to hit `errorElement`; pass `request.signal` to cancel.
- Actions + `<Form>` handle writes; loaders revalidate automatically afterward.
- `useFetcher` mutates without navigating and enables simple optimistic UI.
- Defer slow data with promises + `Await` + Suspense.
- Pair loaders with TanStack Query via `ensureQueryData` for caching and refetching.
- Use `lazy` to split route code.

## Next

[06 — Search, filter, and URL state](./06-search-filter-and-url-state.md)
