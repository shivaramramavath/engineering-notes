# Dynamic Routes and Params

Most URLs contain data: `/projects/42`, `/users/ana/posts/7`, `/docs/guides/setup`. **Dynamic segments** capture those parts of the URL so one route can render any record.

## Dynamic segments

Prefix a segment with `:` to make it a parameter:

```tsx
{ path: "projects/:projectId", element: <ProjectPage /> }
```

Read it with `useParams`:

```tsx
import { useParams } from "react-router"

export function ProjectPage() {
  const { projectId } = useParams()   // string | undefined
  // /projects/42 → "42"
}
```

Key facts:

- Params are **always strings**. `"42"`, not `42`.
- In TypeScript each value is `string | undefined`, because the hook can't know which route it's inside.
- Multiple params work: `users/:userId/posts/:postId`.
- Param names are yours to choose; pick descriptive ones (`projectId`, not `id`) when nesting, so child routes don't shadow parents.

## Converting and validating

Never trust the URL; users edit it by hand.

```tsx
const { projectId } = useParams()
const id = Number(projectId)

if (!Number.isInteger(id) || id <= 0) {
  return <NotFoundPage />
}
```

A small helper keeps this consistent:

```tsx
export function useRequiredParam(name: string): string {
  const value = useParams()[name]
  if (!value) throw new Error(`Missing route param: ${name}`)
  return value
}
```

Throwing hands control to the route's `errorElement`. That's right for "this should never happen" cases (a programming mistake). For "this record doesn't exist", throw a 404 from the loader instead (below).

## Optional segments

Add `?` to make a segment optional:

```tsx
{ path: ":lang?/pricing", element: <PricingPage /> }
// matches /pricing and /en/pricing
{ path: "users/:userId/edit?", element: <UserPage /> }
// matches /users/7 and /users/7/edit
```

Use sparingly. Optional segments make URLs ambiguous. Two explicit routes are often clearer.

## Splat (catch-all) routes

`*` matches the rest of the URL, including slashes:

```tsx
{ path: "files/*", element: <FileBrowser /> }
// /files/docs/2026/report.pdf
const { "*": rest } = useParams()   // "docs/2026/report.pdf"
```

A bare `path: "*"` is the standard 404 route.

## Static vs dynamic: ranking

```tsx
{ path: "projects/new", element: <NewProjectPage /> },
{ path: "projects/:projectId", element: <ProjectPage /> },
```

`/projects/new` matches the static route even though `:projectId` *could* match `"new"`. Specificity wins over order. The catch: if a real project has the slug `new`, it will never be reachable. Reserve words like `new`, `edit`, `settings` in your slug rules.

## Fetching by param

The classic approach, with TanStack Query ([details](../12-server-state/03-tanstack-query.md)):

```tsx
function ProjectPage() {
  const { projectId } = useParams()
  const { data, isPending, error } = useQuery({
    queryKey: ["projects", projectId],
    queryFn: () => fetchProject(projectId!),
    enabled: !!projectId,
  })
  // …
}
```

Including `projectId` in the key means navigating `/projects/1` → `/projects/2` fetches the new record and caches both. Or fetch in a [loader](./05-route-data-loading.md), which reads `params` before the component renders.

## "Not found" for missing records

A well-formed URL for a record that doesn't exist is still a 404. In a loader:

```tsx
export async function projectLoader({ params }: LoaderFunctionArgs) {
  const project = await getProject(params.projectId!)
  if (!project) throw new Response("Not found", { status: 404 })
  return project
}
```

The thrown response renders the route's `errorElement`, where you can show a proper "not found" screen:

```tsx
import { isRouteErrorResponse, useRouteError } from "react-router"

function RouteError() {
  const error = useRouteError()
  if (isRouteErrorResponse(error) && error.status === 404) return <NotFound />
  return <GenericError />
}
```

## Params and component state

Changing `:projectId` re-renders the **same** component instance (see [01](./01-nested-routes-and-layouts.md#layout-lifecycle-what-stays-mounted)). Local state from the previous record sticks around:

```tsx
// Reset everything when the param changes
<Route path="projects/:projectId" element={<ProjectPage />} />

// …inside the layout/wrapper:
const { projectId } = useParams()
return <ProjectForm key={projectId} />
```

Prefer deriving state from props/params over syncing with `useEffect`; see [you might not need an effect](../03-hooks/03-you-might-not-need-an-effect.md).

## Building links with params

```tsx
<Link to={`/projects/${project.id}`}>{project.name}</Link>
```

If the value can contain special characters (a user-entered slug, a search string), encode it: `` `/tags/${encodeURIComponent(tag)}` ``. `useParams` returns the decoded value.

For many links, centralize path building so a URL change touches one file:

```tsx
export const paths = {
  projects: () => "/projects",
  project: (id: string) => `/projects/${id}`,
  projectSettings: (id: string) => `/projects/${id}/settings`,
}
```

## Params vs search params

| | Path params | Search params |
|---|---|---|
| Identify | *Which* resource (`/projects/42`) | *How to view* it (`?tab=activity&sort=new`) |
| Required | Usually | Usually optional |
| Example | `:projectId` | filters, sorting, pagination |

If removing it would change *what* the page is about, it's a path param. If it only changes the view, it's a search param; see [06](./06-search-filter-and-url-state.md).

## Common mistakes

- **Treating params as numbers.** `projectId === 42` is never true; compare strings or convert.
- **Non-null assertion everywhere** (`projectId!`) with no validation, then crashing on a hand-edited URL.
- **Not including the param in query keys or effect deps**, so data doesn't refresh when the param changes.
- **Stale local state** after navigating between two records on the same route.
- **Same param name at multiple levels**, so the child's value shadows the parent's.
- **Unencoded user input in paths** (slashes and `?` break the URL).
- **Putting filters in path params**, leaving ugly, unmatched URLs like `/projects/sort/newest/page/3`.

## Quick summary

- `:name` captures a segment; `useParams()` returns strings (`string | undefined`).
- Validate and convert before use; the URL is user input.
- `?` makes a segment optional; `*` captures the rest of the path.
- Static routes outrank dynamic ones regardless of order.
- Missing records → throw a 404 response from the loader; render it in `errorElement`.
- Param change = same instance re-rendered; reset state with `key`.
- Path params identify the resource; search params describe the view.

## Next

[03 — Navigation](./03-navigation.md)
