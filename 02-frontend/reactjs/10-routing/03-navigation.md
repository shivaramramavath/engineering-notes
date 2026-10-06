# Navigation

Navigation in React Router comes in two flavors: **declarative** (links the user clicks) and **imperative** (code that decides to move the user, such as after a form submit or login). Prefer the declarative kind whenever there's something to click.

## Link

```tsx
import { Link } from "react-router"

<Link to="/projects">Projects</Link>
<Link to={`/projects/${id}`}>Open</Link>
```

`Link` renders a real `<a href>`, intercepts the click, and updates history without a page reload. Because it's an actual anchor, middle-click, "open in new tab", copy-link, and screen reader link navigation all work. A plain `<a href="/projects">` would reload the whole app.

External URLs still use a normal `<a>`:

```tsx
<a href="https://react.dev" target="_blank" rel="noopener noreferrer">React docs</a>
```

### Relative links

By default `to` without a leading slash is relative to the **route** the link is rendered in, not the URL:

```tsx
// rendered inside the route "projects/:id"
<Link to="settings">…</Link>   // /projects/42/settings
<Link to="..">…</Link>         // parent route → /projects
<Link to=".">…</Link>          // this route
```

`..` goes up one **route level**, not one URL segment. For a route like `files/*`, that difference matters; if you want plain URL-segment behavior, add `relative="path"`. Absolute paths (`/projects`) are always safest in shared components.

### `replace` and `state`

```tsx
<Link to="/login" replace>Sign in</Link>          // replace history entry (no extra Back step)
<Link to="/compose" state={{ draftId: 12 }}>New</Link>  // hidden data, read via useLocation().state
```

`state` survives in the history entry (refreshes keep it in browsers) but isn't in the URL, so it can't be shared. Don't use it for anything a user would expect from a shared link.

## NavLink

`NavLink` is a `Link` that knows whether it matches the current URL. It's what menus and tab bars use:

```tsx
import { NavLink } from "react-router"

<NavLink
  to="/projects"
  className={({ isActive, isPending }) =>
    cn("px-3 py-2", isActive && "font-semibold text-primary", isPending && "opacity-60")
  }
>
  Projects
</NavLink>
```

- `isActive`: this link's route is matched. NavLink also sets `aria-current="page"` automatically on the active link, which is good for accessibility.
- `isPending`: navigation to it is in progress (data router with loaders).
- `className`, `style`, and `children` can be functions of that state.

### The `end` prop

A link to `/` (or any parent path) is "active" for every descendant URL, because `/projects/42` *contains* `/`:

```tsx
<NavLink to="/" end>Home</NavLink>
<NavLink to="/dashboard" end>Overview</NavLink>   // not active on /dashboard/settings
```

Use `end` for index-like links; omit it when you want a section link to stay highlighted on nested pages.

## useNavigate: imperative navigation

For navigation that follows an event, not a click on a link:

```tsx
import { useNavigate } from "react-router"

function CreateProjectForm() {
  const navigate = useNavigate()

  async function onSubmit(values: ProjectValues) {
    const project = await createProject(values)
    navigate(`/projects/${project.id}`)
  }
  // …
}
```

```tsx
navigate("/dashboard")
navigate("/login", { replace: true })     // don't leave the previous page in history
navigate(-1)                              // Back
navigate("/search?q=react", { state: { from: "header" } })
```

Use `replace: true` when the previous page shouldn't be reachable via Back: after login, after a redirect, after a successful create/submit.

**Use a `Link` if it's a click.** Wiring `onClick={() => navigate(...)}` on a `div`/`button` throws away real-link behavior and hurts accessibility.

### `navigate` inside effects

`navigate()` during **render** is a side effect and not allowed. Do it in an event handler, an effect, or use `<Navigate>`:

```tsx
// declarative redirect while rendering
if (!user) return <Navigate to="/login" replace />
```

## Redirects from loaders and actions

In data routers, redirect *before* anything renders:

```tsx
import { redirect } from "react-router"

export async function createProjectAction({ request }: ActionFunctionArgs) {
  const formData = await request.formData()
  const project = await createProject(Object.fromEntries(formData))
  return redirect(`/projects/${project.id}`)
}
```

`redirect()` returns a response; the router performs the navigation. Redirecting in a loader avoids the flash of the wrong page that an in-component `<Navigate>` can cause; see [04](./04-route-protection.md).

## Pending UI

With a data router, `useNavigation` reports when a navigation is loading data:

```tsx
import { useNavigation } from "react-router"

function RootLayout() {
  const navigation = useNavigation()
  const isNavigating = navigation.state !== "idle"   // "loading" | "submitting" | "idle"

  return (
    <>
      {isNavigating && <TopProgressBar />}
      <Outlet />
    </>
  )
}
```

The old page stays on screen until the next route's loaders resolve, so a slim progress bar is usually better than a full-page spinner.

## Scroll and focus

- **Scroll restoration**: render `<ScrollRestoration />` once inside your root layout. New navigations scroll to top; Back/forward restore the previous position.

```tsx
import { ScrollRestoration } from "react-router"
<RootLayout> … <ScrollRestoration /> </RootLayout>
```

- **Focus**: client-side navigation doesn't move focus or announce the page change the way a full page load does. Screen reader and keyboard users can be left stranded. A common fix is to move focus to the page heading (or a skip target) on route change and update `document.title`. See [keyboard and focus management](../08-accessibility/02-keyboard-and-focus-management.md).

## Guarding against lost work

```tsx
import { useBlocker } from "react-router"

const blocker = useBlocker(({ currentLocation, nextLocation }) =>
  isDirty && currentLocation.pathname !== nextLocation.pathname
)
// blocker.state === "blocked" → show a confirm dialog, then blocker.proceed() or blocker.reset()
```

`useBlocker` intercepts in-app navigation (needs a data router). It does **not** cover closing the tab or a full reload; pair it with a `beforeunload` listener for that.

## Which one do I use?

| Situation | Use |
|---|---|
| User clicks to go somewhere | `Link` / `NavLink` |
| Menu item that goes to a page | `NavLink` (+ `asChild` in [menus](../09-ui-components/03-dropdowns-and-menus.md)) |
| After a successful mutation in a handler | `navigate()` |
| After a form action on the route | `redirect()` from the action |
| Guard: "not logged in" before render | `redirect()` in a loader, or `<Navigate>` in a layout |
| Update filters/sort/page | `setSearchParams` ([06](./06-search-filter-and-url-state.md)) |

## Common mistakes

- **`<a href>` for internal links** → full reload, lost state.
- **`navigate()` during render** → warning or loops. Use `<Navigate>` or an effect.
- **`onClick` + `navigate` instead of `Link`**, losing right-click, new-tab, and semantics.
- **Forgetting `end`** on a "Home" `NavLink`, so it's always highlighted.
- **Relative link surprises** in splat or pathless-layout routes; use absolute paths when unsure.
- **Using `navigate(-1)` as a "Back" button** when the user may have landed directly on the page (it leaves the app). Provide an explicit fallback route.
- **Putting meaningful data in `state`** that users expect to be shareable.
- **No focus/title update on route change**, which is a real accessibility gap in SPAs.

## Quick summary

- `Link`/`NavLink` for clicks (they're real anchors); `NavLink` adds active/pending state and `aria-current`.
- `useNavigate` for event-driven navigation; `redirect()` in loaders/actions; `<Navigate>` for render-time redirects.
- `replace` avoids polluting history; `end` prevents parent links from always matching.
- `useNavigation` powers pending UI; `<ScrollRestoration />` handles scroll.
- SPAs need explicit focus and title handling on navigation.

## Next

[04 — Route protection](./04-route-protection.md)
