# React Router

React Router maps URLs to components and keeps the browser's history in sync, without full page reloads. It handles matching, nesting, navigation, and (in data mode) data loading.

## Why a router at all

Without one you'd hand-roll `window.location` checks, `history.pushState`, back-button handling, and scroll behavior. A router gives you:

- **Deep links**: `/projects/42` opens that project when pasted into a new tab
- **Back/forward** that work as users expect
- **Layouts** that stay mounted while pages change
- **Data loading** tied to the URL

## Install

```bash
npm install react-router
```

## Setup (data mode)

```tsx
// src/router.tsx
import { createBrowserRouter } from "react-router"
import { RootLayout } from "./layouts/root-layout"
import { HomePage } from "./pages/home"
import { ProjectsPage } from "./pages/projects"
import { NotFoundPage } from "./pages/not-found"

export const router = createBrowserRouter([
  {
    path: "/",
    element: <RootLayout />,
    children: [
      { index: true, element: <HomePage /> },
      { path: "projects", element: <ProjectsPage /> },
      { path: "*", element: <NotFoundPage /> },
    ],
  },
])
```

```tsx
// src/main.tsx
import { createRoot } from "react-dom/client"
import { RouterProvider } from "react-router/dom"
import { router } from "./router"

createRoot(document.getElementById("root")!).render(<RouterProvider router={router} />)
```

Note the import: in v7, `RouterProvider` for DOM rendering comes from `react-router/dom`, while `createBrowserRouter` comes from `react-router`.

Routes are **plain objects**: `path`, `element`, `children`, plus optional `loader`, `action`, `errorElement`, `lazy`. In v7 you can also use `Component` instead of `element` (pass the component, not a JSX element). Both work; this folder uses `element` because it reads clearly.

Create the router **once, outside any component**. Creating it inside a component recreates it on every render and resets navigation state.

## The declarative alternative

The same app with `<BrowserRouter>`:

```tsx
import { BrowserRouter, Routes, Route } from "react-router"

<BrowserRouter>
  <Routes>
    <Route path="/" element={<RootLayout />}>
      <Route index element={<HomePage />} />
      <Route path="projects" element={<ProjectsPage />} />
    </Route>
  </Routes>
</BrowserRouter>
```

It's valid and fine for small apps, but **loaders, actions, `useNavigation`, and `errorElement` only work with a data router** ([05](./05-route-data-loading.md)). Starting with `createBrowserRouter` avoids a migration later.

## How matching works

```text
/projects/42/settings
        │
        ▼
 collect all route patterns ──► rank by specificity ──► pick best match
```

- Matching is **by specificity, not by order**. `/projects/new` beats `/projects/:id` regardless of which is listed first.
- Static segments outrank dynamic (`:id`), which outrank splats (`*`).
- Paths are relative to the parent route, so `children` use `"projects"`, not `"/projects"`.
- Matching is case-insensitive and ignores trailing slashes by default.
- If a URL matches nothing, the router renders its default 404, unless you add a catch-all `path: "*"` route (above).

## Rendering the matched route

A parent route renders its child with `<Outlet />` (see [01](./01-nested-routes-and-layouts.md)):

```tsx
import { Outlet, Link } from "react-router"

export function RootLayout() {
  return (
    <>
      <nav><Link to="/">Home</Link> <Link to="/projects">Projects</Link></nav>
      <main><Outlet /></main>
    </>
  )
}
```

## Reading the current location

```tsx
import { useLocation, useMatches } from "react-router"

const { pathname, search, hash, state } = useLocation()
```

`useLocation` re-renders when the URL changes, which is useful for analytics page views or closing a mobile menu on navigation:

```tsx
useEffect(() => { setMenuOpen(false) }, [pathname])
```

## Hosting: the SPA fallback

With client-side routing, `/projects/42` doesn't exist as a file. If a user refreshes there, the server must **serve `index.html` for unknown paths** so the router can take over. Without that, you get a 404 on refresh (but not on in-app navigation).

- **Vite dev server**: does this automatically.
- **Production**: configure it: Netlify `_redirects` (`/* /index.html 200`), Vercel rewrites, Nginx `try_files $uri /index.html`. See [deployment](../19-production/02-deployment.md).
- If the app lives under a subpath, set `basename` and Vite's `base` to match:

```tsx
createBrowserRouter(routes, { basename: "/app" })
```

## Router types, briefly

| Router | Use |
|---|---|
| `createBrowserRouter` | Normal web apps (clean URLs) |
| `createHashRouter` | Hosts that can't rewrite to `index.html` (URLs look like `/#/projects`) |
| `createMemoryRouter` | Tests, Storybook, non-browser environments |

`createMemoryRouter` with `initialEntries` is how you test route behavior without a browser; see [component testing](../18-testing-and-debugging/02-component-testing-with-rtl.md).

## Common mistakes

- **Creating the router inside a component** → remounts and lost state.
- **Leading slash in child paths** (`path: "/projects"` under a parent) makes the path absolute, which is rarely what you meant.
- **Using `<a href>` for in-app links.** It triggers a full page reload. Use `Link` ([03](./03-navigation.md)).
- **Expecting loaders to work with `<BrowserRouter>`.** They need a data router.
- **No SPA fallback on the host**, so refresh gives 404.
- **Relying on route order** for matching. Specificity decides.
- **Importing from `react-router-dom` in new v7 code.** It works as a shim, but `react-router` is the source.
- **Forgetting a `*` route**, so unknown URLs show the router's default error screen.

## Quick summary

- A router maps URLs to components and manages history without reloads.
- Use `createBrowserRouter` (outside components) + `RouterProvider` to get data APIs.
- Routes are objects: `path`, `element`, `children`, plus `loader`/`action`/`errorElement`/`lazy`.
- Matching is by specificity, relative to the parent route.
- Production hosting must fall back to `index.html`.

## Next

[01 — Nested routes and layouts](./01-nested-routes-and-layouts.md)
