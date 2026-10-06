# Nested Routes and Layouts

Most apps have shared UI around changing content: a sidebar, header, or tab bar that stays put while the inner page swaps. React Router models this with **nested routes**: URL segments map to a *tree* of components, and each parent renders its child through `<Outlet />`.

```text
/dashboard/projects/42

RootLayout            ← "/"
 └─ DashboardLayout   ← "dashboard"      (sidebar stays mounted)
     └─ ProjectPage   ← "projects/:id"   (swaps)
```

Navigating between two dashboard pages **doesn't remount** `RootLayout` or `DashboardLayout`; only the Outlet's content changes. That preserves things like sidebar scroll position and open menus.

## Outlet

```tsx
import { Outlet, NavLink } from "react-router"

export function DashboardLayout() {
  return (
    <div className="grid grid-cols-[220px_1fr]">
      <aside>
        <NavLink to="/dashboard">Overview</NavLink>
        <NavLink to="/dashboard/projects">Projects</NavLink>
      </aside>
      <section><Outlet /></section>
    </div>
  )
}
```

`<Outlet />` is where the matched child route renders. If there's no matching child, it renders nothing, which is why index routes exist.

## Route tree

```tsx
const router = createBrowserRouter([
  {
    path: "/",
    element: <RootLayout />,
    children: [
      { index: true, element: <LandingPage /> },
      {
        path: "dashboard",
        element: <DashboardLayout />,
        children: [
          { index: true, element: <Overview /> },
          { path: "projects", element: <ProjectsPage /> },
          { path: "projects/:id", element: <ProjectPage /> },
          { path: "settings", element: <SettingsPage /> },
        ],
      },
    ],
  },
])
```

Child paths are relative to the parent, so the full path of `ProjectPage` is `/dashboard/projects/:id`.

## Index routes

An **index route** (`index: true`) renders in the parent's Outlet when the URL matches the parent exactly:

- `/dashboard` → `DashboardLayout` + `Overview`
- Without the index route, `/dashboard` would render the layout with an **empty** Outlet.

Index routes have no `path` and no `children`.

## Pathless layout routes

A route with **no `path`** adds a layout (or a guard) *without adding a URL segment*:

```tsx
{
  path: "/",
  element: <RootLayout />,
  children: [
    // Public pages share a marketing layout
    {
      element: <MarketingLayout />,
      children: [
        { index: true, element: <LandingPage /> },
        { path: "pricing", element: <PricingPage /> },
      ],
    },
    // App pages share an authenticated layout
    {
      element: <AppLayout />,
      children: [
        { path: "dashboard", element: <Overview /> },
        { path: "settings", element: <SettingsPage /> },
      ],
    },
  ],
}
```

URLs stay `/pricing` and `/dashboard`, but each group gets its own wrapper. This is the main tool for giving different areas of an app different shells, and for putting a guard in front of a group of routes ([04](./04-route-protection.md)).

## Passing data down

**Simple:** `Outlet` can pass context to its children:

```tsx
// parent
<Outlet context={{ project }} />

// child
import { useOutletContext } from "react-router"
const { project } = useOutletContext<{ project: Project }>()
```

Handy for a layout that loads something its children need (a project header + tabs + subpages). For app-wide values, use regular [context](../03-hooks/05-useContext.md). For server data, prefer [loaders](./05-route-data-loading.md) or [TanStack Query](../12-server-state/03-tanstack-query.md): every route can read what it needs directly instead of threading props.

## Tabs as nested routes

A "tabs" UI where each tab has its own URL is a layout with nested routes, with links styled as tabs:

```tsx
// /projects/42, /projects/42/activity, /projects/42/settings
{
  path: "projects/:id",
  element: <ProjectLayout />,
  children: [
    { index: true, element: <ProjectOverview /> },
    { path: "activity", element: <ProjectActivity /> },
    { path: "settings", element: <ProjectSettings /> },
  ],
}
```

```tsx
function ProjectLayout() {
  return (
    <>
      <nav>
        <NavLink to="." end>Overview</NavLink>
        <NavLink to="activity">Activity</NavLink>
        <NavLink to="settings">Settings</NavLink>
      </nav>
      <Outlet />
    </>
  )
}
```

Compare with the [Tabs component](../09-ui-components/04-tabs.md): use routes when each tab is page-like with its own data and URL.

## Per-route error handling

Each route can define its own error UI. An error in a child renders the *nearest* `errorElement`, while the parent layout stays visible:

```tsx
{
  path: "dashboard",
  element: <DashboardLayout />,
  errorElement: <DashboardError />,
  children: [ /* … */ ],
}
```

Details in [05](./05-route-data-loading.md); for component-level errors in general, see [error boundaries](../15-concurrent-and-modern-react/04-error-boundaries.md).

## Layout lifecycle: what stays mounted

| Navigation | Layout | Page |
|---|---|---|
| `/dashboard` → `/dashboard/settings` | stays mounted | swaps |
| `/projects/1` → `/projects/2` | stays mounted | **same component, new params** (not remounted) |
| `/dashboard/…` → `/pricing` | unmounts | swaps |

The middle row catches people: going from `/projects/1` to `/projects/2` re-renders the **same** component instance with different params. Local state carries over unless you reset it. Either derive state from the params, or force a remount with a key: `<ProjectPage key={id} />`. See [state preservation and reset](../02-state-and-rendering/05-state-preservation-and-reset.md).

## Common mistakes

- **Forgetting `<Outlet />`** in a layout. Children match but never appear.
- **Missing index route**, leaving a blank area at the parent URL.
- **Absolute child paths** (`"/settings"`) which escape the parent nesting.
- **Duplicating layout markup in every page** instead of using a layout route.
- **Stale state when params change**, because the component wasn't remounted.
- **Putting a global provider inside a page** instead of a layout, remounting it on navigation.
- **One giant `errorElement` at the root**, so any error replaces the whole app including navigation.

## Quick summary

- Nested routes mirror nested UI: parents render children via `<Outlet />`.
- Index routes fill the Outlet at the parent's exact URL.
- Pathless routes add layouts or guards without changing the URL.
- Layouts persist across child navigation; same-route param changes re-render, not remount.
- Put `errorElement`s at layout boundaries to contain failures.

## Next

[02 — Dynamic routes and params](./02-dynamic-routes-and-params.md)
