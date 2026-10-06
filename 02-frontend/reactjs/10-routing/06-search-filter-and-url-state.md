# Search, Filter, and URL State

Where should "the current page", "the active filter", or "the selected tab" live? For anything a user might want to **bookmark, share, refresh, or navigate back to**, the answer is the **URL**. The URL becomes the single source of truth, and components read from it instead of keeping a parallel `useState`.

```text
?q=react&status=open&sort=newest&page=2
        │
        ▼
 read + validate ──► query key / API params ──► UI
        ▲
        └── user changes a filter → update the URL
```

## What belongs in the URL

| In the URL | Not in the URL |
|---|---|
| Search text, filters, sort order | Whether a dropdown is open |
| Page number / cursor | Hover or focus state |
| Selected tab or view mode | Unsaved form input (usually) |
| Selected item id (e.g., open drawer) | Secrets, tokens, personal data |
| Date range | Large objects |

Test: *"If the user refreshes or sends this link, should they see the same thing?"* If yes, URL.

Never put sensitive data in the URL; it ends up in browser history, server logs, and referrer headers.

## useSearchParams

```tsx
import { useSearchParams } from "react-router"

function ProjectsPage() {
  const [searchParams, setSearchParams] = useSearchParams()

  const q = searchParams.get("q") ?? ""
  const status = searchParams.get("status") ?? "all"

  function setStatus(next: string) {
    setSearchParams((prev) => {
      const params = new URLSearchParams(prev)
      next === "all" ? params.delete("status") : params.set("status", next)
      params.delete("page")                       // changing a filter resets paging
      return params
    })
  }
  // …
}
```

`searchParams` is a standard [`URLSearchParams`](https://developer.mozilla.org/docs/Web/API/URLSearchParams). Key points:

- **Values are strings or `null`.** Convert numbers and booleans yourself.
- **`setSearchParams` navigates.** It pushes a new history entry, and the component re-renders with the new value.
- **Use the functional form** to modify one param while **keeping the others**. `setSearchParams({ status: "open" })` *replaces* all params, wiping `q` and `page`.
- Delete params that equal their default so URLs stay clean (`?status=all` is noise).
- `{ replace: true }` avoids a history entry per change; good for typing in a search box, optional for discrete filters:

```tsx
setSearchParams(params, { replace: true })
```

## Parse and validate

URL values are user input. Centralize parsing so the rest of the code works with real types:

```tsx
const STATUSES = ["all", "open", "closed"] as const
type Status = (typeof STATUSES)[number]

function parseFilters(sp: URLSearchParams) {
  const status = sp.get("status")
  const page = Number(sp.get("page"))
  return {
    q: sp.get("q")?.trim() ?? "",
    status: (STATUSES as readonly string[]).includes(status ?? "") ? (status as Status) : "all",
    page: Number.isInteger(page) && page > 0 ? page : 1,
    tags: sp.getAll("tag"),                 // ?tag=a&tag=b → ["a", "b"]
  }
}

const filters = useMemo(() => parseFilters(searchParams), [searchParams])
```

- Fall back to defaults for garbage input instead of crashing.
- Repeated keys give arrays: use `getAll`/`append`, not comma-joined strings, unless you handle the split.
- A schema library like [Zod](../06-forms/01-form-validation.md) works well here: `schema.catch(defaults).parse(Object.fromEntries(sp))` (for single-valued params).
- Wrap this in a custom hook (`useProjectFilters`) so parsing and updating live in one place ([custom hooks](../03-hooks/09-custom-hooks.md)).

## Debounced search input

Don't write to the URL on every keystroke, and don't bind the input directly to the URL value; the input stutters and the caret jumps. Keep **local state for typing** and sync to the URL after a pause:

```tsx
function SearchBox() {
  const [searchParams, setSearchParams] = useSearchParams()
  const urlQuery = searchParams.get("q") ?? ""

  const [input, setInput] = useState(urlQuery)
  const debounced = useDebouncedValue(input, 300)

  // Input → URL (after the pause)
  useEffect(() => {
    if (debounced === urlQuery) return
    setSearchParams((prev) => {
      const params = new URLSearchParams(prev)
      debounced ? params.set("q", debounced) : params.delete("q")
      params.delete("page")
      return params
    }, { replace: true })
  }, [debounced]) // eslint-disable-line react-hooks/exhaustive-deps

  // URL → Input (Back/forward, "clear filters" button)
  useEffect(() => { setInput(urlQuery) }, [urlQuery])

  return <Input value={input} onChange={(e) => setInput(e.target.value)} placeholder="Search…" aria-label="Search projects" />
}
```

Two effects, each with a guard, keep both directions in sync without loops. `useDebouncedValue` is a small custom hook ([custom hooks](../03-hooks/09-custom-hooks.md)). The `exhaustive-deps` suppression is intentional here: we only want to push when the *debounced input* changes. If that bothers you, move this logic into the event handler with a debounced callback instead.

## Driving data from the URL

URL → query key → fetch. When the URL changes, the key changes, and [TanStack Query](../12-server-state/03-tanstack-query.md) fetches (or serves cache):

```tsx
const filters = useProjectFilters()

const { data, isPlaceholderData } = useQuery({
  queryKey: ["projects", filters],
  queryFn: () => fetchProjects(filters),
  placeholderData: keepPreviousData,       // keep old rows visible while the new page loads
})
```

Back/forward then work naturally: the URL goes back, the key goes back, and the cache likely already has the data.

Or read the URL in a [loader](./05-route-data-loading.md):

```tsx
export async function projectsLoader({ request }: LoaderFunctionArgs) {
  const sp = new URL(request.url).searchParams
  return fetchProjects(parseFilters(sp))
}
```

Loaders re-run when search params change, so filters automatically trigger fresh data.

## Example: tables, tabs, and detail panels

- **[Data tables](../09-ui-components/07-data-tables.md)**: map `pagination` and `sorting` state to `page` and `sort` params, and derive the table's state from the URL rather than `useState`.
- **[Tabs](../09-ui-components/04-tabs.md)**: `?tab=activity` as the controlled `value`, with validation and a default.
- **Selected row / drawer**: `?selected=42` opens a detail sheet, and Back closes it, which is what users expect.

## Resetting and "clear filters"

```tsx
<Button variant="ghost" onClick={() => setSearchParams({})}>Clear filters</Button>
```

An empty object clears everything, which is exactly right here. Make sure local input state resyncs from the URL (the second effect above).

## Building links that carry filters

```tsx
<Link to={{ pathname: "/projects", search: "?status=open" }}>Open projects</Link>
<Link to={`/projects?${new URLSearchParams({ status: "open", sort: "newest" })}`}>…</Link>
```

`URLSearchParams` handles encoding for you; don't concatenate user text into query strings by hand.

## Beyond `useSearchParams`

For complex apps with many typed params, dedicated libraries (such as `nuqs`) or router-level typed search schemas (TanStack Router) reduce the parse/serialize boilerplate. They're optional conveniences; the mechanics are the same as above.

## Common mistakes

- **`setSearchParams({ x })` wiping other params.** Use the functional form.
- **Writing to the URL on every keystroke**, flooding history and re-fetching per character.
- **Binding the input's `value` directly to the URL**, causing caret jumps and lag.
- **Forgetting to reset `page`** when a filter changes, leaving users on "page 7 of 2".
- **Unvalidated params** (`?page=abc` → `NaN`, `?status=hacked` → empty list).
- **Duplicating state**: `useState` for filters *and* the URL, which then drift apart.
- **Pushing history entries for every tweak**, so Back needs ten clicks. Use `replace` where it makes sense.
- **Unstable derived objects in query keys or deps**: build `filters` with `useMemo` keyed on `searchParams`.
- **Sensitive data in query strings.**
- **Hand-concatenated query strings** with unencoded user input.

## Quick summary

- If it should survive refresh, sharing, and Back, store it in the URL; the URL is the source of truth.
- `useSearchParams` gives a `URLSearchParams`; values are strings. Update with the functional form to preserve other params.
- Parse and validate in one place; fall back to defaults and drop default values from the URL.
- Debounce search: local state for typing, URL updates after a pause, and sync back on external changes.
- Put filters in the query key (or read them in a loader) so data follows the URL.
- Reset `page` when filters change; use `replace` for high-frequency updates.

## Next

Continue to [11 — API integration](../11-api-integration/README.md).
